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
        _escapeDot {db_settings.DataBaseMongoEscapeChar.empty() ? '\0' : db_settings.DataBaseMongoEscapeChar.front()}
    {
        FO_STACK_TRACE_ENTRY();

        if (db_settings.DataBaseMongoEscapeChar.length() > 1) {
            throw DataBaseException("DbMongo escape char must be empty or a single character", db_settings.DataBaseMongoEscapeChar);
        }
        if (_escapeDot == '.') {
            throw DataBaseException("DbMongo escape char can't be '.'", db_settings.DataBaseMongoEscapeChar);
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

    [[nodiscard]] auto GetAllRecordIds(hstring collection_name) const -> vector<ident_t> override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

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

        vector<ident_t> ids;
        const bson_t* document = nullptr;

        while (mongoc_cursor_next(cursor, &document)) {
            bson_iter_t iter;

            static_assert(sizeof(ident_t) == sizeof(int64));

            if (!bson_iter_init(&iter, document)) {
                throw DataBaseException("DbMongo bson_iter_init", collection_name);
            }
            if (!bson_iter_next(&iter)) {
                throw DataBaseException("DbMongo bson_iter_next", collection_name);
            }
            if (bson_iter_type(&iter) != BSON_TYPE_INT64) {
                throw DataBaseException("DbMongo bson_iter_type", collection_name, bson_iter_type(&iter));
            }

            const auto id = bson_iter_int64(&iter);
            ids.emplace_back(id);
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
    [[nodiscard]] auto GetRecord(hstring collection_name, ident_t id) const -> AnyData::Document override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

        auto* collection = GetCollection(collection_name);

        if (collection == nullptr) {
            return {};
        }

        bson_t filter;
        bson_init(&filter);

        static_assert(sizeof(ident_t) == sizeof(int64));

        if (!bson_append_int64(&filter, "_id", 3, id.underlying_value())) {
            throw DataBaseException("DbMongo bson_append_int64", collection_name, id);
        }

        bson_t opts;
        bson_init(&opts);

        if (!bson_append_int32(&opts, "limit", 5, 1)) {
            throw DataBaseException("DbMongo bson_append_int32", collection_name, id);
        }

        auto* cursor = mongoc_collection_find_with_opts(collection, &filter, &opts, nullptr);

        if (cursor == nullptr) {
            throw DataBaseException("DbMongo mongoc_collection_find", collection_name, id);
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

    void InsertRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_collectionsLocker};

        auto* collection = GetOrCreateCollection(collection_name);

        bson_t insert;
        bson_init(&insert);

        static_assert(sizeof(ident_t) == sizeof(int64));

        if (!bson_append_int64(&insert, "_id", 3, id.underlying_value())) {
            throw DataBaseException("DbMongo bson_append_int64", collection_name, id);
        }

        DocumentToBson(doc, &insert, _escapeDot);

        bson_error_t error;

        if (!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &insert, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_insert", collection_name, id, error.message);
        }

        bson_destroy(&insert);
    }

    void UpdateRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!doc.Empty());

        std::scoped_lock locker {_collectionsLocker};

        auto* collection = GetCollection(collection_name);

        if (collection == nullptr) {
            throw DataBaseException("DbMongo Collection not found on update", collection_name, id);
        }

        bson_t selector;
        bson_init(&selector);

        static_assert(sizeof(ident_t) == sizeof(int64));

        if (!bson_append_int64(&selector, "_id", 3, id.underlying_value())) {
            throw DataBaseException("DbMongo bson_append_int64", collection_name, id);
        }

        bson_t update;
        bson_init(&update);

        bson_t update_set;

        if (!bson_append_document_begin(&update, "$set", 4, &update_set)) {
            throw DataBaseException("DbMongo bson_append_document_begin", collection_name, id);
        }

        DocumentToBson(doc, &update_set, _escapeDot);

        if (!bson_append_document_end(&update, &update_set)) {
            throw DataBaseException("DbMongo bson_append_document_end", collection_name, id);
        }

        bson_error_t error;

        if (!mongoc_collection_update_one(collection, &selector, &update, nullptr, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_update_one", collection_name, id, error.message);
        }

        bson_destroy(&selector);
        bson_destroy(&update);
    }

    void DeleteRecord(hstring collection_name, ident_t id) override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

        auto* collection = GetCollection(collection_name);

        if (collection == nullptr) {
            throw DataBaseException("DbMongo Collection not found on delete", collection_name, id);
        }

        bson_t selector;
        bson_init(&selector);

        static_assert(sizeof(ident_t) == sizeof(int64));

        if (!bson_append_int64(&selector, "_id", 3, id.underlying_value())) {
            throw DataBaseException("DbMongo bson_append_int64", collection_name, id);
        }

        bson_error_t error;

        if (!mongoc_collection_delete_one(collection, &selector, nullptr, nullptr, &error)) {
            throw DataBaseException("DbMongo mongoc_collection_delete_one", collection_name, id, error.message);
        }

        bson_destroy(&selector);
    }

    auto TryReconnect() -> bool override
    {
        FO_STACK_TRACE_ENTRY();

        std::scoped_lock locker {_collectionsLocker};

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
            return nullptr;
        }

        return it->second.get_no_const();
    }

    auto GetOrCreateCollection(hstring collection_name) -> mongoc_collection_t*
    {
        FO_STACK_TRACE_ENTRY();

        mongoc_collection_t* collection;

        const auto it = _collections.find(collection_name.as_str());

        if (it == _collections.end()) {
            collection = mongoc_database_get_collection(_database.get(), collection_name.as_str().c_str());

            if (collection == nullptr) {
                throw DataBaseException("DbMongo Can't create collection", collection_name);
            }

            _collections.emplace(collection_name.as_str(), collection);
        }
        else {
            collection = it->second.get();
        }

        return collection;
    }

    raw_ptr<mongoc_client_t> _client {};
    raw_ptr<mongoc_database_t> _database {};
    char _escapeDot {};
    mutable std::mutex _collectionsLocker {};
    unordered_map<string, raw_ptr<mongoc_collection_t>> _collections {};
};

auto CreateMongoDataBase(DataBaseSettings& db_settings, string_view uri, string_view db_name, DataBasePanicCallback panic_callback) -> DataBaseImpl*
{
    return SafeAlloc::MakeRaw<DbMongo>(db_settings, uri, db_name, std::move(panic_callback));
}
#endif

FO_END_NAMESPACE
