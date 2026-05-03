#include "DataBase.h"

#if FO_HAVE_MONGO
FO_DISABLE_WARNINGS_PUSH()
#include <mongoc/mongoc.h>
FO_DISABLE_WARNINGS_POP()
#endif

#include "WinApiUndef-Include.h"

FO_BEGIN_NAMESPACE

#if FO_HAVE_MONGO
class DbMongo final : public DataBaseImpl
{
public:
    DbMongo(const DbMongo&) = delete;
    DbMongo(DbMongo&&) noexcept = delete;
    auto operator=(const DbMongo&) = delete;
    auto operator=(DbMongo&&) noexcept = delete;

    explicit DbMongo(DataBaseSettings& db_settings, string_view uri, string_view db_name, DataBasePanicCallback panic_callback) :
        DataBaseImpl(db_settings, std::move(panic_callback)),
        _escapeDot {db_settings.MongoEscapeChar.empty() ? '\0' : db_settings.MongoEscapeChar.front()}
    {
        FO_STACK_TRACE_ENTRY();

        if (db_settings.MongoEscapeChar.length() > 1) {
            throw DataBaseException("DbMongo escape char must be empty or a single character", db_settings.MongoEscapeChar);
        }
        if (_escapeDot == '.') {
            throw DataBaseException("DbMongo escape char can't be '.'", db_settings.MongoEscapeChar);
        }

        mongoc_init();

        bson_error_t error;
        auto* mongo_uri = mongoc_uri_new_with_error(string(uri).c_str(), &error);

        if (mongo_uri == nullptr) {
            throw DataBaseException("DbMongo Failed to parse URI", uri, error.message);
        }

        auto* client = mongoc_client_new_from_uri(mongo_uri);

        if (client == nullptr) {
            throw DataBaseException("DbMongo Can't create client");
        }

        mongoc_uri_destroy(mongo_uri);
        mongoc_client_set_appname(client, FO_DEV_NAME.c_str());

        auto* database = mongoc_client_get_database(client, string(db_name).c_str());

        if (database == nullptr) {
            throw DataBaseException("DbMongo Can't get database", db_name);
        }

        mongoc_cursor_t* collections_cursor = mongoc_database_find_collections_with_opts(database, nullptr);
        const bson_t* collection_doc;

        while (mongoc_cursor_next(collections_cursor, &collection_doc)) {
            bson_iter_t collection_iter;

            if (bson_iter_init_find(&collection_iter, collection_doc, "name")) {
                const char* collection_name = bson_iter_utf8(&collection_iter, nullptr);
                mongoc_collection_t* collection = mongoc_database_get_collection(database, collection_name);

                if (collection == nullptr) {
                    throw DataBaseException("DbMongo Can't get collection", collection_name);
                }

                _collections.emplace(collection_name, collection);
            }
        }

        if (mongoc_cursor_error(collections_cursor, &error)) {
            throw DataBaseException("DbMongo Can't retreive collections", error.message);
        }

        mongoc_cursor_destroy(collections_cursor);

        const auto* ping = BCON_NEW("ping", BCON_INT32(1));
        bson_t reply;

        if (!mongoc_client_command_simple(client, "admin", ping, nullptr, &reply, &error)) {
            throw DataBaseException("DbMongo Can't ping database", error.message);
        }

        _client = client;
        _database = database;
        StartCommitThread();
    }

    ~DbMongo() override
    {
        FO_STACK_TRACE_ENTRY();

        StopCommitThread();

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
        ignore_unused(key_type);

        std::scoped_lock locker {_storageLocker};

        const auto it = _collections.find(collection_name.as_str());

        if (it == _collections.end()) {
            bson_error_t error;
            auto* collection = mongoc_database_create_collection(_database.get(), collection_name.as_str().c_str(), nullptr, &error);

            if (collection == nullptr) {
                collection = mongoc_database_get_collection(_database.get(), collection_name.as_str().c_str());
            }

            if (collection == nullptr) {
                throw DataBaseException("DbMongo Can't create collection", collection_name, error.message);
            }

            _collections.emplace(collection_name.as_str(), collection);
        }
    }

    [[nodiscard]] auto GetAllRecordIds(hstring collection_name) const -> vector<DataBaseKey> override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_storageLocker};

        const auto key_type = GetCollectionKeyType(collection_name);

        auto* collection = GetCollection(collection_name);

        if (collection == nullptr) {
            return {};
        }

        bson_t filter;
        bson_init(&filter);

        auto* opts = BCON_NEW("projection", "{", "_id", BCON_BOOL(true), "}");
        auto* cursor = mongoc_collection_find_with_opts(collection, &filter, opts, nullptr);

        if (cursor == nullptr) {
            throw DataBaseException("DbMongo mongoc_collection_find", collection_name);
        }

        vector<DataBaseKey> ids;
        const bson_t* document = nullptr;

        while (mongoc_cursor_next(cursor, &document)) {
            bson_iter_t iter;

            if (!bson_iter_init(&iter, document)) {
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
                const auto* value = bson_iter_utf8(&iter, &len);

                if (value == nullptr || len == 0) {
                    throw DataBaseException("DbMongo invalid string key", collection_name);
                }

                ids.emplace_back(string(value, value + len));
            }
            else {
                throw DataBaseException("DbMongo bson_iter_type", collection_name, bson_iter_type(&iter));
            }
        }

        bson_error_t error;

        if (mongoc_cursor_error(cursor, &error)) {
            throw DataBaseException("DbMongo mongoc_cursor_error", collection_name, error.message);
        }

        mongoc_cursor_destroy(cursor);
        bson_destroy(&filter);
        bson_destroy(opts);

        return ids;
    }

protected:
    [[nodiscard]] auto GetRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_storageLocker};

        auto* collection = GetCollection(collection_name);

        if (collection == nullptr) {
            return {};
        }

        bson_t filter;
        bson_init(&filter);

        AppendMongoDbKey(&filter, id, collection_name);

        bson_t opts;
        bson_init(&opts);

        if (!bson_append_int32(&opts, "limit", 5, 1)) {
            throw DataBaseException("DbMongo bson_append_int32", collection_name, FormatMongoDbKey(id));
        }

        auto* cursor = mongoc_collection_find_with_opts(collection, &filter, &opts, nullptr);

        if (cursor == nullptr) {
            throw DataBaseException("DbMongo mongoc_collection_find", collection_name, FormatMongoDbKey(id));
        }

        const bson_t* bson = nullptr;

        if (!mongoc_cursor_next(cursor, &bson)) {
            mongoc_cursor_destroy(cursor);
            bson_destroy(&filter);
            bson_destroy(&opts);
            return {};
        }

        AnyData::Document doc;
        BsonToDocument(bson, doc, _escapeDot);

        mongoc_cursor_destroy(cursor);
        bson_destroy(&filter);
        bson_destroy(&opts);
        return doc;
    }

    void InsertRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_storageLocker};

        auto* collection = GetCollection(collection_name);

        bson_t insert;
        bson_init(&insert);

        AppendMongoDbKey(&insert, id, collection_name);

        DocumentToBson(doc, &insert, _escapeDot);

        bson_error_t error;

        if (!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &insert, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_insert", collection_name, id, error.message);
        }

        bson_destroy(&insert);
    }

    void UpdateRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_storageLocker};

        auto* collection = GetCollection(collection_name);

        if (collection == nullptr) {
            throw DataBaseException("DbMongo Collection not found on update", collection_name, FormatMongoDbKey(id));
        }

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

        if (!mongoc_collection_update_one(collection, &selector, &update, nullptr, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_update_one", collection_name, id, error.message);
        }

        bson_destroy(&selector);
        bson_destroy(&update);
    }

    void DeleteRecord(hstring collection_name, const DataBaseKey& id) override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_storageLocker};

        auto* collection = GetCollection(collection_name);

        if (collection == nullptr) {
            throw DataBaseException("DbMongo Collection not found on delete", collection_name, FormatMongoDbKey(id));
        }

        bson_t selector;
        bson_init(&selector);

        AppendMongoDbKey(&selector, id, collection_name);

        bson_error_t error;

        if (!mongoc_collection_delete_one(collection, &selector, nullptr, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_delete_one", collection_name, id, error.message);
        }

        bson_destroy(&selector);
    }

    auto TryReconnect() -> bool override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_storageLocker};

        const auto* ping = BCON_NEW("ping", BCON_INT32(1));
        bson_t reply;
        bson_error_t error;
        const auto ok = mongoc_client_command_simple(_client.get(), "admin", ping, nullptr, &reply, &error);
        bson_destroy(const_cast<bson_t*>(ping));

        if (ok) {
            bson_destroy(&reply);
        }
        else {
            WriteLog("Mongo reconnect probe failed: {}", error.message);
        }

        return ok;
    }

private:
    auto GetCollection(hstring collection_name) const -> mongoc_collection_t*
    {
        FO_STACK_TRACE_ENTRY();

        const auto it = _collections.find(collection_name.as_str());

        if (it == _collections.end()) {
            throw DataBaseException("DbMongo Collection not found", collection_name);
        }

        return it->second.get_no_const();
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

    static void AppendMongoDbKey(bson_t* bson, const DataBaseKey& key, hstring collection_name)
    {
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<T, ident_t>) {
                    if (!bson_append_int64(bson, "_id", 3, value.underlying_value())) {
                        throw DataBaseException("DbMongo bson_append_int64", collection_name, value);
                    }
                }
                else {
                    if (!bson_append_utf8(bson, "_id", 3, value.c_str(), numeric_cast<int>(value.length()))) {
                        throw DataBaseException("DbMongo bson_append_utf8", collection_name, value);
                    }
                }
            },
            key);
    }

    raw_ptr<mongoc_client_t> _client {};
    raw_ptr<mongoc_database_t> _database {};
    char _escapeDot {};
    mutable std::mutex _storageLocker {};
    unordered_map<string, raw_ptr<mongoc_collection_t>> _collections {};
};

auto CreateMongoDataBase(DataBaseSettings& db_settings, string_view uri, string_view db_name, DataBasePanicCallback panic_callback) -> DataBaseImpl*
{
    return SafeAlloc::MakeRaw<DbMongo>(db_settings, uri, db_name, std::move(panic_callback));
}
#endif

FO_END_NAMESPACE
