#include "DataBase.h"

#if FO_HAVE_MONGO
FO_DISABLE_WARNINGS_PUSH()
#include <mongoc/mongoc.h>
FO_DISABLE_WARNINGS_POP()
#endif

#include "WinApiUndef.inc"

FO_BEGIN_NAMESPACE

#if FO_HAVE_MONGO
FO_CLANG_IGNORE_WARNINGS_PUSH("-Walign-mismatch")

class DbMongo final : public DataBaseImpl
{
public:
    DbMongo(const DbMongo&) = delete;
    DbMongo(DbMongo&&) noexcept = delete;
    auto operator=(const DbMongo&) = delete;
    auto operator=(DbMongo&&) noexcept = delete;

    explicit DbMongo(ptr<DataBaseSettings> db_settings, string_view uri, string_view db_name, DataBasePanicCallback panic_callback) :
        DataBaseImpl(db_settings, std::move(panic_callback)),
        _escapeDot {db_settings->MongoEscapeChar.empty() ? '\0' : db_settings->MongoEscapeChar.front()}
    {
        FO_STACK_TRACE_ENTRY();

        if (db_settings->MongoEscapeChar.length() > 1) {
            throw DataBaseException("DbMongo escape char must be empty or a single character", db_settings->MongoEscapeChar);
        }
        if (_escapeDot == '.') {
            throw DataBaseException("DbMongo escape char can't be '.'", db_settings->MongoEscapeChar);
        }

        mongoc_init();
        auto mongoc_cleanup_guard = scope_fail([]() noexcept { mongoc_cleanup(); });

        bson_error_t error;
        string uri_text = string(uri);
        auto uri_ptr = make_ptr(uri_text.c_str());
        auto mongo_uri = make_nptr(mongoc_uri_new_with_error(uri_ptr.get(), &error));

        if (!mongo_uri) {
            throw DataBaseException("DbMongo Failed to parse URI", uri, error.message);
        }

        auto client = make_nptr(mongoc_client_new_from_uri(mongo_uri.get()));

        if (!client) {
            mongoc_uri_destroy(mongo_uri.get());
            throw DataBaseException("DbMongo Can't create client");
        }

        auto client_guard = scope_fail([&]() noexcept { mongoc_client_destroy(client.get()); });

        mongoc_uri_destroy(mongo_uri.get());
        mongoc_client_set_appname(client.get(), FO_DEV_NAME);

        string db_name_text = string(db_name);
        auto db_name_ptr = make_ptr(db_name_text.c_str());
        auto database = make_nptr(mongoc_client_get_database(client.get(), db_name_ptr.get()));

        if (!database) {
            throw DataBaseException("DbMongo Can't get database", db_name);
        }

        auto database_guard = scope_fail([&]() noexcept { mongoc_database_destroy(database.get()); });
        auto collections_guard = scope_fail([&]() noexcept {
            scoped_lock locker {_storageLocker};
            for (auto& value : _collections | std::views::values) {
                mongoc_collection_destroy(value.get());
            }
        });

        {
            scoped_lock locker {_storageLocker};

            auto collections_cursor = make_nptr(mongoc_database_find_collections_with_opts(database.get(), nullptr));
            FO_VERIFY_AND_THROW(collections_cursor, "Failed to obtain collections cursor");
            auto cursor_guard = scope_fail([&]() noexcept { mongoc_cursor_destroy(collections_cursor.get()); });
            nptr<const bson_t> collection_doc;

            while (mongoc_cursor_next(collections_cursor.get(), collection_doc.get_pp())) {
                FO_VERIFY_AND_THROW(collection_doc, "Cursor returned a null collection document");
                bson_iter_t collection_iter;
                auto aligned_collection_doc = std::assume_aligned<BSON_ALIGN_OF_PTR>(collection_doc.get());

                if (bson_iter_init_find(&collection_iter, aligned_collection_doc, "name")) {
                    auto collection_name = make_nptr(bson_iter_utf8(&collection_iter, nullptr));

                    if (!collection_name) {
                        throw DataBaseException("DbMongo invalid collection name");
                    }

                    auto collection = make_nptr(mongoc_database_get_collection(database.get(), collection_name.get()));

                    if (!collection) {
                        throw DataBaseException("DbMongo Can't get collection", collection_name.get());
                    }

                    _collections.emplace(collection_name.get(), collection);
                }
            }

            if (mongoc_cursor_error(collections_cursor.get(), &error)) {
                throw DataBaseException("DbMongo Can't retreive collections", error.message);
            }

            mongoc_cursor_destroy(collections_cursor.get());
        }

        auto ping_doc = make_nptr(BCON_NEW("ping", BCON_INT32(1)));
        FO_VERIFY_AND_THROW(ping_doc, "Failed to build ping command document");
        auto ping = make_unique_del_ptr(ping_doc, [](ptr<bson_t> doc) FO_DEFERRED {
            auto aligned_doc = std::assume_aligned<BSON_ALIGN_OF_PTR>(doc.get());
            bson_destroy(aligned_doc);
        });
        ptr<const bson_t> ping_ptr = ping.get();
        bson_t reply;
        auto aligned_ping = std::assume_aligned<BSON_ALIGN_OF_PTR>(ping_ptr.get());

        if (!mongoc_client_command_simple(client.get(), "admin", aligned_ping, nullptr, &reply, &error)) {
            throw DataBaseException("DbMongo Can't ping database", error.message);
        }

        {
            scoped_lock locker {_storageLocker};

            _client = client;
            _database = database;
        }

        StartCommitThread();
    }

    ~DbMongo() override
    {
        FO_STACK_TRACE_ENTRY();

        StopCommitThread();

        scoped_lock locker {_storageLocker};

        for (auto& value : _collections | std::views::values) {
            mongoc_collection_destroy(value.get());
        }

        mongoc_database_destroy(_database.get());
        mongoc_client_destroy(_client.get());
        mongoc_cleanup();
    }

protected:
    [[nodiscard]] auto GetStringKeyEscaping() const noexcept -> DataBaseStringKeyEscaping override { return DataBaseStringKeyEscaping::Raw; }

    void EnsureCollection(hstring collection_name, DataBaseKeyType key_type) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(key_type);

        scoped_lock locker {_storageLocker};

        auto it = _collections.find(collection_name.as_str());

        if (it == _collections.end()) {
            bson_error_t error;
            auto collection_name_ptr = make_ptr(collection_name.c_str());
            auto collection = make_nptr(mongoc_database_create_collection(_database.get(), collection_name_ptr.get(), nullptr, &error));

            if (!collection) {
                collection = mongoc_database_get_collection(_database.get(), collection_name_ptr.get());
            }

            if (!collection) {
                throw DataBaseException("DbMongo Can't create collection", collection_name, error.message);
            }

            _collections.emplace(collection_name.as_str(), collection);
        }
    }

    [[nodiscard]] auto GetAllRecordIds(hstring collection_name) const -> vector<DataBaseKey> override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        auto key_type = GetCollectionKeyType(collection_name);

        ptr<mongoc_collection_t> collection = GetCollection(collection_name);

        bson_t filter;
        bson_init(&filter);

        auto opts_doc = make_nptr(BCON_NEW("projection", "{", "_id", BCON_BOOL(true), "}"));
        FO_VERIFY_AND_THROW(opts_doc, "Failed to build query options document");
        auto opts = make_unique_del_ptr(opts_doc, [](ptr<bson_t> doc) FO_DEFERRED {
            auto aligned_doc = std::assume_aligned<BSON_ALIGN_OF_PTR>(doc.get());
            bson_destroy(aligned_doc);
        });
        ptr<const bson_t> opts_ptr = opts.get();
        auto aligned_opts = std::assume_aligned<BSON_ALIGN_OF_PTR>(opts_ptr.get());
        auto cursor = make_nptr(mongoc_collection_find_with_opts(collection.get(), &filter, aligned_opts, nullptr));

        if (!cursor) {
            throw DataBaseException("DbMongo mongoc_collection_find", collection_name);
        }

        vector<DataBaseKey> ids;
        nptr<const bson_t> document;

        while (mongoc_cursor_next(cursor.get(), document.get_pp())) {
            FO_VERIFY_AND_THROW(document, "Cursor returned a null document");
            bson_iter_t iter;
            auto aligned_document = std::assume_aligned<BSON_ALIGN_OF_PTR>(document.get());

            if (!bson_iter_init(&iter, aligned_document)) {
                throw DataBaseException("DbMongo bson_iter_init", collection_name);
            }
            if (!bson_iter_next(&iter)) {
                throw DataBaseException("DbMongo bson_iter_next", collection_name);
            }
            if (bson_iter_type(&iter) == BSON_TYPE_INT64) {
                if (key_type != DataBaseKeyType::IntId) {
                    throw DataBaseException("DbMongo invalid key type in collection", collection_name, MongoDbKeyTypeName(key_type));
                }

                ids.emplace_back(ident_t {bson_iter_int64(&iter)});
            }
            else if (bson_iter_type(&iter) == BSON_TYPE_UTF8) {
                if (key_type != DataBaseKeyType::String) {
                    throw DataBaseException("DbMongo invalid key type in collection", collection_name, MongoDbKeyTypeName(key_type));
                }

                uint32_t len = 0;
                auto value = make_nptr(bson_iter_utf8(&iter, &len));

                if (!value || len == 0) {
                    throw DataBaseException("DbMongo invalid string key", collection_name);
                }

                ids.emplace_back(string(value.get(), len));
            }
            else {
                throw DataBaseException("DbMongo bson_iter_type", collection_name, bson_iter_type(&iter));
            }
        }

        bson_error_t error;

        if (mongoc_cursor_error(cursor.get(), &error)) {
            throw DataBaseException("DbMongo mongoc_cursor_error", collection_name, error.message);
        }

        mongoc_cursor_destroy(cursor.get());
        bson_destroy(&filter);

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        ptr<mongoc_collection_t> collection = GetCollection(collection_name);

        bson_t filter;
        bson_init(&filter);

        AppendMongoDbKey(&filter, id, collection_name);

        bson_t opts;
        bson_init(&opts);

        if (!bson_append_int32(&opts, "limit", 5, 1)) {
            throw DataBaseException("DbMongo bson_append_int32", collection_name, FormatMongoDbKey(id));
        }

        auto cursor = make_nptr(mongoc_collection_find_with_opts(collection.get(), &filter, &opts, nullptr));

        if (!cursor) {
            throw DataBaseException("DbMongo mongoc_collection_find", collection_name, FormatMongoDbKey(id));
        }

        nptr<const bson_t> bson;

        if (!mongoc_cursor_next(cursor.get(), bson.get_pp())) {
            mongoc_cursor_destroy(cursor.get());
            bson_destroy(&filter);
            bson_destroy(&opts);
            return {};
        }

        FO_VERIFY_AND_THROW(bson, "Cursor returned a null document");
        AnyData::Document doc;
        BsonToDocument(bson, doc, _escapeDot);

        mongoc_cursor_destroy(cursor.get());
        bson_destroy(&filter);
        bson_destroy(&opts);
        return doc;
    }

    void InsertRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(!doc.Empty(), "Mongo database insert received an empty document", collection_name, id);

        scoped_lock locker {_storageLocker};

        ptr<mongoc_collection_t> collection = GetCollection(collection_name);

        bson_t insert;
        bson_init(&insert);

        AppendMongoDbKey(&insert, id, collection_name);

        DocumentToBson(doc, &insert, _escapeDot);

        bson_error_t error;

        if (!mongoc_collection_insert(collection.get(), MONGOC_INSERT_NONE, &insert, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_insert", collection_name, id, error.message);
        }

        bson_destroy(&insert);
    }

    void UpdateRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(!doc.Empty(), "Mongo database update received an empty document", collection_name, id);

        scoped_lock locker {_storageLocker};

        ptr<mongoc_collection_t> collection = GetCollection(collection_name);

        bson_t selector;
        bson_init(&selector);

        AppendMongoDbKey(&selector, id, collection_name);

        bson_t update;
        bson_init(&update);

        bson_t update_set;

        if (!bson_append_document_begin(&update, "$set", 4, &update_set)) {
            throw DataBaseException("DbMongo bson_append_document_begin", collection_name, FormatMongoDbKey(id));
        }

        DocumentToBson(doc, &update_set, _escapeDot);

        if (!bson_append_document_end(&update, &update_set)) {
            throw DataBaseException("DbMongo bson_append_document_end", collection_name, FormatMongoDbKey(id));
        }

        bson_error_t error;

        if (!mongoc_collection_update_one(collection.get(), &selector, &update, nullptr, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_update_one", collection_name, id, error.message);
        }

        bson_destroy(&selector);
        bson_destroy(&update);
    }

    void DeleteRecord(hstring collection_name, const DataBaseKey& id) override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        ptr<mongoc_collection_t> collection = GetCollection(collection_name);

        bson_t selector;
        bson_init(&selector);

        AppendMongoDbKey(&selector, id, collection_name);

        bson_error_t error;

        if (!mongoc_collection_delete_one(collection.get(), &selector, nullptr, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_delete_one", collection_name, id, error.message);
        }

        bson_destroy(&selector);
    }

    auto TryReconnect() -> bool override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        auto ping_doc = make_nptr(BCON_NEW("ping", BCON_INT32(1)));
        FO_VERIFY_AND_THROW(ping_doc, "Failed to build ping command document");
        auto ping = make_unique_del_ptr(ping_doc, [](ptr<bson_t> doc) FO_DEFERRED {
            auto aligned_doc = std::assume_aligned<BSON_ALIGN_OF_PTR>(doc.get());
            bson_destroy(aligned_doc);
        });
        ptr<const bson_t> ping_ptr = ping.get();
        bson_t reply;
        bson_error_t error;
        auto aligned_ping = std::assume_aligned<BSON_ALIGN_OF_PTR>(ping_ptr.get());
        bool ok = mongoc_client_command_simple(_client.get(), "admin", aligned_ping, nullptr, &reply, &error);

        if (ok) {
            bson_destroy(&reply);
        }
        else {
            WriteLog("Mongo reconnect probe failed: {}", error.message);
        }

        return ok;
    }

private:
    ptr<mongoc_collection_t> GetCollection(hstring collection_name) const FO_TSA_REQUIRES(_storageLocker)
    {
        FO_STACK_TRACE_ENTRY();

        auto it = _collections.find(collection_name.as_str());

        if (it == _collections.end()) {
            throw DataBaseException("DbMongo Collection not found", collection_name);
        }

        return it->second;
    }

    static auto FormatMongoDbKey(const DataBaseKey& key) -> string
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

    static auto MongoDbKeyTypeName(DataBaseKeyType key_type) noexcept -> string_view
    {
        switch (key_type) {
        case DataBaseKeyType::IntId:
            return "Id";
        case DataBaseKeyType::String:
            return "String";
        }

        return "Unknown";
    }

    static void AppendMongoDbKey(ptr<bson_t> bson, const DataBaseKey& key, hstring collection_name)
    {
        FO_STACK_TRACE_ENTRY();

        auto aligned_bson = std::assume_aligned<BSON_ALIGN_OF_PTR>(bson.get());

        std::visit(
            [&, aligned_bson](const auto& value) {
                using T = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<T, ident_t>) {
                    if (!bson_append_int64(aligned_bson, "_id", 3, value.underlying_value())) {
                        throw DataBaseException("DbMongo bson_append_int64", collection_name, value);
                    }
                }
                else {
                    auto key_value = make_ptr(value.c_str());
                    if (!bson_append_utf8(aligned_bson, "_id", 3, key_value.get(), numeric_cast<int32_t>(value.length()))) {
                        throw DataBaseException("DbMongo bson_append_utf8", collection_name, value);
                    }
                }
            },
            key);
    }

    mutable mutex _storageLocker {};
    nptr<mongoc_client_t> _client FO_TSA_GUARDED_BY(_storageLocker) {};
    nptr<mongoc_database_t> _database FO_TSA_GUARDED_BY(_storageLocker) {};
    unordered_map<string, ptr<mongoc_collection_t>> _collections FO_TSA_GUARDED_BY(_storageLocker) {};
    char _escapeDot {};
};

auto CreateMongoDataBase(ptr<DataBaseSettings> db_settings, string_view uri, string_view db_name, DataBasePanicCallback panic_callback) -> unique_ptr<DataBaseImpl>
{
    return SafeAlloc::MakeUnique<DbMongo>(db_settings, uri, db_name, std::move(panic_callback));
}

FO_CLANG_IGNORE_WARNINGS_POP()
#endif

FO_END_NAMESPACE
