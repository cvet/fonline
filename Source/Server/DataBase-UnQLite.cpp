#include "DataBase.h"

FO_DISABLE_WARNINGS_PUSH()
#include <bson/bson.h>
FO_DISABLE_WARNINGS_POP()

#if FO_HAVE_UNQLITE
#include <unqlite.h>
#endif

#include "WinApiUndef-Include.h"

FO_BEGIN_NAMESPACE

#if FO_HAVE_UNQLITE
class DbUnQLite final : public DataBaseImpl
{
public:
    DbUnQLite() = delete;
    DbUnQLite(const DbUnQLite&) = delete;
    DbUnQLite(DbUnQLite&&) noexcept = delete;
    auto operator=(const DbUnQLite&) = delete;
    auto operator=(DbUnQLite&&) noexcept = delete;

    explicit DbUnQLite(DataBaseSettings& db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) :
        DataBaseImpl(db_settings, std::move(panic_callback)),
        _storageDir {storage_dir},
        _openFlags {UNQLITE_OPEN_CREATE | (db_settings.UnQLiteOmitJournaling ? UNQLITE_OPEN_OMIT_JOURNALING : 0)}
    {
        FO_STACK_TRACE_ENTRY();

        (void)fs_create_directories(storage_dir);
        ValidateStorage();
        StartCommitThread();
    }

    ~DbUnQLite() override
    {
        FO_STACK_TRACE_ENTRY();

        StopCommitThread();

        for (auto& value : _collections | std::views::values) {
            unqlite_close(value.get());
        }
    }

    [[nodiscard]] auto GetAllRecordIds(hstring collection_name) const -> vector<ident_t> override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

        auto* db = GetCollection(collection_name);

        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        unqlite_kv_cursor* cursor = nullptr;
        const auto kv_cursor_init = unqlite_kv_cursor_init(db, &cursor);

        if (kv_cursor_init != UNQLITE_OK) {
            throw DataBaseException("DbUnQLite unqlite_kv_cursor_init", kv_cursor_init);
        }

        const auto kv_cursor_first_entry = unqlite_kv_cursor_first_entry(cursor);

        if (kv_cursor_first_entry != UNQLITE_OK && kv_cursor_first_entry != UNQLITE_DONE) {
            throw DataBaseException("DbUnQLite unqlite_kv_cursor_first_entry", kv_cursor_first_entry);
        }

        vector<ident_t> ids;

        while (unqlite_kv_cursor_valid_entry(cursor) != 0) {
            ident_t id;

            const auto kv_cursor_key_callback = unqlite_kv_cursor_key_callback(
                cursor,
                [](const void* output, unsigned output_len, void* user_data) {
                    static_assert(sizeof(ident_t) == sizeof(int64));
                    FO_RUNTIME_ASSERT(output_len == sizeof(int64));
                    *cast_from_void<int64*>(user_data) = *cast_from_void<const int64*>(output);
                    return UNQLITE_OK;
                },
                &id);

            if (kv_cursor_key_callback != UNQLITE_OK) {
                throw DataBaseException("DbUnQLite unqlite_kv_cursor_init", kv_cursor_key_callback);
            }
            if (id == ident_t {}) {
                throw DataBaseException("DbUnQLite Id is zero");
            }

            ids.emplace_back(id);

            const auto kv_cursor_next_entry = unqlite_kv_cursor_next_entry(cursor);

            if (kv_cursor_next_entry != UNQLITE_OK && kv_cursor_next_entry != UNQLITE_DONE) {
                throw DataBaseException("DbUnQLite kv_cursor_next_entry", kv_cursor_next_entry);
            }
        }

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(hstring collection_name, ident_t id) const -> AnyData::Document override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

        auto* db = GetCollection(collection_name);

        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        AnyData::Document doc;

        const auto kv_fetch_callback = unqlite_kv_fetch_callback(
            db, &id, sizeof(id),
            [](const void* output, unsigned output_len, void* user_data) {
                bson_t bson;

                if (!bson_init_static(&bson, cast_from_void<const uint8*>(output), output_len)) {
                    throw DataBaseException("DbUnQLite bson_init_static");
                }

                auto& doc2 = *cast_from_void<AnyData::Document*>(user_data);
                BsonToDocument(&bson, doc2);

                return UNQLITE_OK;
            },
            &doc);

        if (kv_fetch_callback != UNQLITE_OK && kv_fetch_callback != UNQLITE_NOTFOUND) {
            throw DataBaseException("DbUnQLite unqlite_kv_fetch_callback", kv_fetch_callback);
        }

        return doc;
    }

    void InsertRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_collectionsLocker};

        auto* db = GetCollection(collection_name);

        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        const auto kv_fetch_callback = unqlite_kv_fetch_callback(db, &id, sizeof(id), [](const void*, unsigned, void*) { return UNQLITE_OK; }, nullptr);

        if (kv_fetch_callback != UNQLITE_NOTFOUND) {
            throw DataBaseException("DbUnQLite unqlite_kv_fetch_callback", kv_fetch_callback);
        }

        bson_t bson;
        bson_init(&bson);
        DocumentToBson(doc, &bson);

        const auto* bson_data = bson_get_data(&bson);

        if (bson_data == nullptr) {
            throw DataBaseException("DbUnQLite bson_get_data");
        }

        const auto kv_store = unqlite_kv_store(db, &id, sizeof(id), bson_data, bson.len);

        if (kv_store != UNQLITE_OK) {
            bson_destroy(&bson);
            throw DataBaseException("DbUnQLite unqlite_kv_store", kv_store);
        }

        bson_destroy(&bson);
        CommitCollection(db);
    }

    void UpdateRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_collectionsLocker};

        auto* db = GetCollection(collection_name);

        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        auto actual_doc = GetRecord(collection_name, id);

        if (actual_doc.Empty()) {
            throw DataBaseException("DbUnQLite Document not found", collection_name, id);
        }

        for (auto&& [doc_key, doc_value] : doc) {
            actual_doc.Assign(doc_key, doc_value.Copy());
        }

        bson_t bson;
        bson_init(&bson);
        DocumentToBson(actual_doc, &bson);

        const auto* bson_data = bson_get_data(&bson);

        if (bson_data == nullptr) {
            throw DataBaseException("DbUnQLite bson_get_data");
        }

        const auto kv_store = unqlite_kv_store(db, &id, sizeof(id), bson_data, bson.len);

        if (kv_store != UNQLITE_OK) {
            bson_destroy(&bson);
            throw DataBaseException("DbUnQLite unqlite_kv_store", kv_store);
        }

        bson_destroy(&bson);
        CommitCollection(db);
    }

    void DeleteRecord(hstring collection_name, ident_t id) override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

        auto* db = GetCollection(collection_name);

        if (db == nullptr) {
            throw DataBaseException("DbUnQLite Can't open collection", collection_name);
        }

        const auto kv_delete = unqlite_kv_delete(db, &id, sizeof(id));

        if (kv_delete != UNQLITE_OK) {
            throw DataBaseException("DbUnQLite unqlite_kv_delete", kv_delete);
        }

        CommitCollection(db);
    }

    auto TryReconnect() -> bool override
    {
        FO_STACK_TRACE_ENTRY();

        try {
            ValidateStorage();
            return true;
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            return false;
        }
    }

private:
    void ValidateStorage() const
    {
        FO_STACK_TRACE_ENTRY();

        unqlite* ping_db = nullptr;
        const string ping_db_path = strex("{}/Ping.unqlite", _storageDir);

        if (unqlite_open(&ping_db, ping_db_path.c_str(), _openFlags) != UNQLITE_OK) {
            throw DataBaseException("DbUnQLite Can't open db", ping_db_path);
        }

        constexpr auto ping = 42u;

        if (unqlite_kv_store(ping_db, &ping, sizeof(ping), &ping, sizeof(ping)) != UNQLITE_OK) {
            unqlite_close(ping_db);
            throw DataBaseException("DbUnQLite Can't write to db", ping_db_path);
        }

        unqlite_close(ping_db);
        (void)fs_remove_file(ping_db_path);
    }

    void CommitCollection(unqlite* db) const
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

        const auto commit = unqlite_commit(db);

        if (commit != UNQLITE_OK) {
            throw DataBaseException("DbUnQLite unqlite_commit", commit);
        }
    }

    auto GetCollection(hstring collection_name) const -> unqlite*
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

        unqlite* db;

        const auto it = _collections.find(collection_name);

        if (it == _collections.end()) {
            const string db_path = strex("{}/{}.unqlite", _storageDir, collection_name);
            const auto r = unqlite_open(&db, db_path.c_str(), _openFlags);

            if (r != UNQLITE_OK) {
                throw DataBaseException("DbUnQLite Can't open db", collection_name, r);
            }

            _collections.emplace(collection_name, db);
        }
        else {
            db = it->second.get();
        }

        return db;
    }

    string _storageDir {};
    int32 _openFlags {};
    mutable std::recursive_mutex _collectionsLocker {};
    mutable unordered_map<hstring, raw_ptr<unqlite>> _collections {};
};

auto CreateUnQLiteDataBase(DataBaseSettings& db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) -> DataBaseImpl*
{
    return SafeAlloc::MakeRaw<DbUnQLite>(db_settings, storage_dir, std::move(panic_callback));
}
#endif

FO_END_NAMESPACE
