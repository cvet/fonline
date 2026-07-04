#include "DataBase.h"

FO_DISABLE_WARNINGS_PUSH()
#include <bson/bson.h>
FO_DISABLE_WARNINGS_POP()

#if FO_HAVE_UNQLITE
#include <unqlite.h>
#endif

#include "WinApiUndef.inc"

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

    explicit DbUnQLite(ptr<DataBaseSettings> db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) :
        DataBaseImpl(db_settings, std::move(panic_callback)),
        _storageDir {storage_dir},
        _openFlags {UNQLITE_OPEN_CREATE | (db_settings->UnQLiteOmitJournaling ? UNQLITE_OPEN_OMIT_JOURNALING : 0)}
    {
        FO_STACK_TRACE_ENTRY();

        fs_create_directories(storage_dir);
        ValidateStorage();
        StartCommitThread();
    }

    ~DbUnQLite() override
    {
        FO_STACK_TRACE_ENTRY();

        StopCommitThread();

        scoped_lock locker {_storageLocker};

        for (auto& value : _collections | std::views::values) {
            unqlite_close(value.get());
        }
    }

protected:
    [[nodiscard]] auto GetStringKeyEscaping() const noexcept -> DataBaseStringKeyEscaping override { return DataBaseStringKeyEscaping::Hex; }

    void EnsureCollection(hstring collection_name, DataBaseKeyType key_type) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(key_type);

        scoped_lock locker {_storageLocker};

        const auto it = _collections.find(collection_name);

        if (it == _collections.end()) {
            nptr<unqlite> db;
            const string db_path = strex("{}/{}.unqlite", _storageDir, collection_name);
            ptr<const char> db_path_ptr = db_path.c_str();
            const auto r = unqlite_open(db.get_pp(), db_path_ptr.get(), _openFlags);

            if (r != UNQLITE_OK) {
                throw DataBaseException("DbUnQLite Can't open db", collection_name, r);
            }

            FO_VERIFY_AND_THROW(db, "Opened database handle is null");
            _collections.emplace(collection_name, db.as_ptr());
        }
    }

    [[nodiscard]] auto GetAllRecordIds(hstring collection_name) const -> vector<DataBaseKey> override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        const auto key_type = GetCollectionKeyType(collection_name);
        ptr<unqlite> db = GetCollection(collection_name);

        nptr<unqlite_kv_cursor> cursor;
        const auto kv_cursor_init = unqlite_kv_cursor_init(db.get(), cursor.get_pp());

        if (kv_cursor_init != UNQLITE_OK) {
            throw DataBaseException("DbUnQLite unqlite_kv_cursor_init", kv_cursor_init);
        }

        FO_VERIFY_AND_THROW(cursor, "Initialized key-value cursor is null");
        const auto kv_cursor_first_entry = unqlite_kv_cursor_first_entry(cursor.get());

        if (kv_cursor_first_entry != UNQLITE_OK && kv_cursor_first_entry != UNQLITE_DONE) {
            throw DataBaseException("DbUnQLite unqlite_kv_cursor_first_entry", kv_cursor_first_entry);
        }

        vector<DataBaseKey> ids;

        while (unqlite_kv_cursor_valid_entry(cursor.get()) != 0) {
            vector<uint8_t> key_data;
            ptr<void> key_data_user_data = cast_to_void(&key_data);

            const auto kv_cursor_key_callback = unqlite_kv_cursor_key_callback(
                cursor.get(),
                [](const void* output, unsigned output_len, void* user_data) {
                    nptr<vector<uint8_t>> nullable_result = cast_from_void<vector<uint8_t>*>(user_data);
                    FO_VERIFY_AND_THROW(!!nullable_result, "Missing key callback user data");
                    auto result = nullable_result.as_ptr();
                    nptr<const uint8_t> nullable_output_data = cast_from_void<const uint8_t*>(output);

                    if (output_len != 0) {
                        FO_VERIFY_AND_THROW(!!nullable_output_data, "Missing key callback output data");
                        auto output_data = nullable_output_data.as_ptr();
                        const auto output_bytes = make_const_span(output_data, output_len);
                        result->assign(output_bytes.begin(), output_bytes.end());
                    }
                    else {
                        result->clear();
                    }

                    return UNQLITE_OK;
                },
                key_data_user_data.get());

            if (kv_cursor_key_callback != UNQLITE_OK) {
                throw DataBaseException("DbUnQLite unqlite_kv_cursor_init", kv_cursor_key_callback);
            }

            ids.emplace_back(ParseUnQLiteKey(key_data, key_type));

            const auto kv_cursor_next_entry = unqlite_kv_cursor_next_entry(cursor.get());

            if (kv_cursor_next_entry != UNQLITE_OK && kv_cursor_next_entry != UNQLITE_DONE) {
                throw DataBaseException("DbUnQLite kv_cursor_next_entry", kv_cursor_next_entry);
            }
        }

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        return GetRecordUnlocked(collection_name, id);
    }

    void InsertRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(!doc.Empty(), "UnQLite database insert received an empty document", collection_name, id);

        scoped_lock locker {_storageLocker};

        ptr<unqlite> db = GetCollection(collection_name);

        const auto key = MakeUnQLiteKey(id, GetCollectionKeyType(collection_name));
        nptr<const uint8_t> key_data = key.data();
        const int32_t key_size = numeric_cast<int32_t>(key.size());
        const auto kv_fetch_callback = unqlite_kv_fetch_callback(db.get(), key_data.get(), key_size, [](const void*, unsigned, void*) { return UNQLITE_OK; }, nullptr);

        if (kv_fetch_callback != UNQLITE_NOTFOUND) {
            throw DataBaseException("DbUnQLite unqlite_kv_fetch_callback", kv_fetch_callback);
        }

        bson_t bson;
        bson_init(&bson);
        DocumentToBson(doc, &bson);

        nptr<const uint8_t> bson_data = bson_get_data(&bson);

        if (!bson_data) {
            throw DataBaseException("DbUnQLite bson_get_data");
        }

        const auto kv_store = unqlite_kv_store(db.get(), key_data.get(), key_size, bson_data.get(), bson.len);

        if (kv_store != UNQLITE_OK) {
            bson_destroy(&bson);
            throw DataBaseException("DbUnQLite unqlite_kv_store", kv_store);
        }

        bson_destroy(&bson);
        CommitCollection(db);
    }

    void UpdateRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(!doc.Empty(), "UnQLite database update received an empty document", collection_name, id);

        scoped_lock locker {_storageLocker};

        ptr<unqlite> db = GetCollection(collection_name);

        const auto key = MakeUnQLiteKey(id, GetCollectionKeyType(collection_name));
        const int32_t key_size = numeric_cast<int32_t>(key.size());
        auto actual_doc = GetRecordUnlocked(collection_name, id);

        if (actual_doc.Empty()) {
            throw DataBaseException("DbUnQLite Document not found", collection_name, FormatUnQLiteDbKey(id));
        }

        for (auto&& [doc_key, doc_value] : doc) {
            actual_doc.Assign(doc_key, doc_value.Copy());
        }

        bson_t bson;
        bson_init(&bson);
        DocumentToBson(actual_doc, &bson);

        nptr<const uint8_t> bson_data = bson_get_data(&bson);

        if (!bson_data) {
            throw DataBaseException("DbUnQLite bson_get_data");
        }

        const auto kv_store = unqlite_kv_store(db.get(), key.data(), key_size, bson_data.get(), bson.len);

        if (kv_store != UNQLITE_OK) {
            bson_destroy(&bson);
            throw DataBaseException("DbUnQLite unqlite_kv_store", kv_store);
        }

        bson_destroy(&bson);
        CommitCollection(db);
    }

    void DeleteRecord(hstring collection_name, const DataBaseKey& id) override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        ptr<unqlite> db = GetCollection(collection_name);

        const auto key = MakeUnQLiteKey(id, GetCollectionKeyType(collection_name));
        const int32_t key_size = numeric_cast<int32_t>(key.size());
        const auto kv_delete = unqlite_kv_delete(db.get(), key.data(), key_size);

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
    static auto MakeUnQLiteKey(const DataBaseKey& key, DataBaseKeyType key_type) -> vector<uint8_t>
    {
        if (key_type == DataBaseKeyType::IntId) {
            nptr<const ident_t> numeric_key = std::get_if<ident_t>(&key);

            if (!numeric_key) {
                throw DataBaseException("DbUnQLite Invalid numeric key type", FormatUnQLiteDbKey(key));
            }

            static_assert(sizeof(ident_t) == sizeof(int64_t));

            vector<uint8_t> result(sizeof(int64_t));
            const int64_t value = numeric_key->underlying_value();
            MemCopy(result.data(), &value, sizeof(value));
            return result;
        }

        const auto key_str = std::get<string>(key);
        return vector<uint8_t>(key_str.begin(), key_str.end());
    }

    static auto ParseUnQLiteKey(const_span<uint8_t> key_data, DataBaseKeyType key_type) -> DataBaseKey
    {
        if (key_type == DataBaseKeyType::IntId) {
            if (key_data.size() != sizeof(int64_t)) {
                throw DataBaseException("DbUnQLite invalid numeric key size", key_data.size());
            }

            int64_t value {};
            MemCopy(&value, key_data.data(), sizeof(value));

            if (value <= 0) {
                throw DataBaseException("DbUnQLite invalid numeric key", value);
            }

            return ident_t {value};
        }

        if (key_data.empty()) {
            throw DataBaseException("DbUnQLite empty key");
        }

        return string {span_to_string(key_data)};
    }

    static auto FormatUnQLiteDbKey(const DataBaseKey& key) -> string
    {
        return std::visit(
            [](const auto& value) -> string {
                using T = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<T, ident_t>) {
                    return strex("{}", value).str();
                }
                else {
                    return value;
                }
            },
            key);
    }

    void ValidateStorage() const
    {
        FO_STACK_TRACE_ENTRY();

        nptr<unqlite> nullable_ping_db;
        const string ping_db_path = strex("{}/Ping.unqlite", _storageDir);
        ptr<const char> ping_db_path_ptr = ping_db_path.c_str();

        if (unqlite_open(nullable_ping_db.get_pp(), ping_db_path_ptr.get(), _openFlags) != UNQLITE_OK) {
            throw DataBaseException("DbUnQLite Can't open db", ping_db_path);
        }

        FO_VERIFY_AND_THROW(nullable_ping_db, "Opened ping database handle is null");
        auto ping_db = nullable_ping_db.as_ptr();

        constexpr uint32_t ping = 42u;

        if (unqlite_kv_store(ping_db.get(), &ping, sizeof(ping), &ping, sizeof(ping)) != UNQLITE_OK) {
            unqlite_close(ping_db.get());
            throw DataBaseException("DbUnQLite Can't write to db", ping_db_path);
        }

        unqlite_close(ping_db.get());
        fs_remove_file(ping_db_path);
    }

    void CommitCollection(ptr<unqlite> db) const
    {
        FO_STACK_TRACE_ENTRY();

        const auto commit = unqlite_commit(db.get());

        if (commit != UNQLITE_OK) {
            throw DataBaseException("DbUnQLite unqlite_commit", commit);
        }
    }

    ptr<unqlite> GetCollection(hstring collection_name) const FO_TSA_REQUIRES(_storageLocker)
    {
        FO_STACK_TRACE_ENTRY();

        const auto it = _collections.find(collection_name);

        if (it == _collections.end()) {
            throw DataBaseException("DbUnQLite Invalid collection", collection_name);
        }

        return it->second;
    }

    AnyData::Document GetRecordUnlocked(hstring collection_name, const DataBaseKey& id) const FO_TSA_REQUIRES(_storageLocker)
    {
        FO_STACK_TRACE_ENTRY();

        ptr<unqlite> db = GetCollection(collection_name);

        const auto key = MakeUnQLiteKey(id, GetCollectionKeyType(collection_name));
        const int32_t key_size = numeric_cast<int32_t>(key.size());
        AnyData::Document doc;
        ptr<void> document_user_data = cast_to_void(&doc);

        const auto kv_fetch_callback = unqlite_kv_fetch_callback(
            db.get(), key.data(), key_size,
            [](const void* output, unsigned output_len, void* user_data) {
                bson_t bson;
                nptr<const uint8_t> output_data = cast_from_void<const uint8_t*>(output);
                FO_VERIFY_AND_THROW(!!output_data, "Missing fetch callback output data");

                if (!bson_init_static(&bson, output_data.get(), output_len)) {
                    throw DataBaseException("DbUnQLite bson_init_static");
                }

                nptr<AnyData::Document> nullable_document = cast_from_void<AnyData::Document*>(user_data);
                FO_VERIFY_AND_THROW(!!nullable_document, "Missing fetch callback document user data");
                auto document = nullable_document.as_ptr();
                BsonToDocument(&bson, *document);

                return UNQLITE_OK;
            },
            document_user_data.get());

        if (kv_fetch_callback != UNQLITE_OK && kv_fetch_callback != UNQLITE_NOTFOUND) {
            throw DataBaseException("DbUnQLite unqlite_kv_fetch_callback", kv_fetch_callback);
        }

        return doc;
    }

    string _storageDir {};
    int32_t _openFlags {};
    mutable mutex _storageLocker {};
    unordered_map<hstring, ptr<unqlite>> _collections FO_TSA_GUARDED_BY(_storageLocker) {};
};

auto CreateUnQLiteDataBase(ptr<DataBaseSettings> db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) -> unique_ptr<DataBaseImpl>
{
    return SafeAlloc::MakeUnique<DbUnQLite>(db_settings, storage_dir, std::move(panic_callback));
}
#endif

FO_END_NAMESPACE
