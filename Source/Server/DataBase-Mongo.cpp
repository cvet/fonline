//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

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
FO_GCC_IGNORE_WARNINGS_PUSH("-Wignored-attributes")

// Upper bound on the ids one batch read sends in a single $in filter, keeping the query document far below the 16 MB limit
static constexpr size_t MONGO_BATCH_READ_SIZE = 1000;

static void InitializeMongoRuntime();
static void MongoLogHandler(mongoc_log_level_t log_level, const char* log_domain, const char* message, void* user_data);

class DbMongo final : public DataBaseImpl
{
public:
    DbMongo(const DbMongo&) = delete;
    DbMongo(DbMongo&&) noexcept = delete;
    auto operator=(const DbMongo&) = delete;
    auto operator=(DbMongo&&) noexcept = delete;

    explicit DbMongo(ptr<DataBaseSettings> db_settings, string_view uri, string_view db_name, DataBasePanicCallback panic_callback) :
        DataBaseImpl(db_settings, std::move(panic_callback)),
        _escapeDot {db_settings->DataBase.MongoEscapeChar.empty() ? '\0' : db_settings->DataBase.MongoEscapeChar.front()}
    {
        FO_STACK_TRACE_ENTRY();

        if (db_settings->DataBase.MongoEscapeChar.length() > 1) {
            throw DataBaseException("DbMongo escape char must be empty or a single character", db_settings->DataBase.MongoEscapeChar);
        }
        if (_escapeDot == '.') {
            throw DataBaseException("DbMongo escape char can't be '.'", db_settings->DataBase.MongoEscapeChar);
        }

        InitializeMongoRuntime();

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

    [[nodiscard]] auto GetRecords(hstring collection_name, const vector<DataBaseKey>& ids) const -> vector<AnyData::Document> override
    {
        FO_STACK_TRACE_ENTRY();

        vector<AnyData::Document> docs(ids.size());
        unordered_map<DataBaseKey, size_t> index_by_id;

        for (size_t i = 0; i < ids.size(); i++) {
            FO_VERIFY_AND_THROW(index_by_id.emplace(ids[i], i).second, "Batch read requested the same record twice", collection_name, FormatMongoDbKey(ids[i]));
        }

        scoped_lock locker {_storageLocker};

        ptr<mongoc_collection_t> collection = GetCollection(collection_name);

        for (size_t chunk_start = 0; chunk_start < ids.size(); chunk_start += MONGO_BATCH_READ_SIZE) {
            size_t chunk_end = std::min(chunk_start + MONGO_BATCH_READ_SIZE, ids.size());

            bson_t filter;
            bson_init(&filter);
            auto destroy_filter = scope_exit([&filter]() noexcept { bson_destroy(&filter); });

            bson_t id_filter;

            if (!bson_append_document_begin(&filter, "_id", 3, &id_filter)) {
                throw DataBaseException("DbMongo bson_append_document_begin", collection_name);
            }

            bson_t id_list;

            if (!bson_append_array_unsafe_begin(&id_filter, "$in", 3, &id_list)) {
                throw DataBaseException("DbMongo bson_append_array_unsafe_begin", collection_name);
            }

            for (size_t i = chunk_start; i < chunk_end; i++) {
                AppendMongoDbValue(&id_list, strex("{}", i - chunk_start).str(), ids[i], collection_name);
            }

            if (!bson_append_array_end(&id_filter, &id_list)) {
                throw DataBaseException("DbMongo bson_append_array_end", collection_name);
            }
            if (!bson_append_document_end(&filter, &id_filter)) {
                throw DataBaseException("DbMongo bson_append_document_end", collection_name);
            }

            bson_t opts;
            bson_init(&opts);
            auto destroy_opts = scope_exit([&opts]() noexcept { bson_destroy(&opts); });

            // The server answers a find with 101 documents unless told otherwise, and each further batch is one more round
            // trip; the limit closes the cursor with the last record instead of leaving an empty getMore to find the end
            int32_t chunk_size = numeric_cast<int32_t>(chunk_end - chunk_start);

            if (!bson_append_int32(&opts, "batchSize", 9, chunk_size) || !bson_append_int32(&opts, "limit", 5, chunk_size)) {
                throw DataBaseException("DbMongo bson_append_int32", collection_name);
            }

            auto cursor = make_nptr(mongoc_collection_find_with_opts(collection.get(), &filter, &opts, nullptr));

            if (!cursor) {
                throw DataBaseException("DbMongo mongoc_collection_find", collection_name);
            }

            auto destroy_cursor = scope_exit([&cursor]() noexcept { mongoc_cursor_destroy(cursor.get()); });
            nptr<const bson_t> bson;

            while (mongoc_cursor_next(cursor.get(), bson.get_pp())) {
                FO_VERIFY_AND_THROW(bson, "Cursor returned a null document");
                DataBaseKey id = ReadMongoDbKey(bson, collection_name);
                auto it = index_by_id.find(id);
                FO_VERIFY_AND_THROW(it != index_by_id.end(), "Batch read returned a record that was not requested", collection_name, FormatMongoDbKey(id));
                BsonToDocument(bson, docs[it->second], _escapeDot);
            }

            bson_error_t error;

            if (mongoc_cursor_error(cursor.get(), &error)) {
                throw DataBaseException("DbMongo mongoc_cursor_error", collection_name, error.message);
            }
        }

        return docs;
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
            logging::write("Mongo reconnect probe failed: {}", error.message);
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

        AppendMongoDbValue(bson, "_id", key, collection_name);
    }

    static void AppendMongoDbValue(ptr<bson_t> bson, string_view field_name, const DataBaseKey& key, hstring collection_name)
    {
        FO_STACK_TRACE_ENTRY();

        auto aligned_bson = std::assume_aligned<BSON_ALIGN_OF_PTR>(bson.get());
        string field_name_str {field_name};
        auto field_name_ptr = make_ptr(field_name_str.c_str());
        int32_t field_name_len = numeric_cast<int32_t>(field_name_str.length());

        std::visit(
            [&, aligned_bson](const auto& value) {
                using T = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<T, ident_t>) {
                    if (!bson_append_int64(aligned_bson, field_name_ptr.get(), field_name_len, value.underlying_value())) {
                        throw DataBaseException("DbMongo bson_append_int64", collection_name, value);
                    }
                }
                else {
                    auto key_value = make_ptr(value.c_str());
                    if (!bson_append_utf8(aligned_bson, field_name_ptr.get(), field_name_len, key_value.get(), numeric_cast<int32_t>(value.length()))) {
                        throw DataBaseException("DbMongo bson_append_utf8", collection_name, value);
                    }
                }
            },
            key);
    }

    static auto ReadMongoDbKey(ptr<const bson_t> bson, hstring collection_name) -> DataBaseKey
    {
        FO_STACK_TRACE_ENTRY();

        bson_iter_t iter;
        auto aligned_bson = std::assume_aligned<BSON_ALIGN_OF_PTR>(bson.get());

        if (!bson_iter_init_find(&iter, aligned_bson, "_id")) {
            throw DataBaseException("DbMongo record without _id", collection_name);
        }

        if (bson_iter_type(&iter) == BSON_TYPE_INT64) {
            return ident_t {bson_iter_int64(&iter)};
        }

        if (bson_iter_type(&iter) == BSON_TYPE_UTF8) {
            uint32_t len = 0;
            auto value = make_nptr(bson_iter_utf8(&iter, &len));

            if (!value || len == 0) {
                throw DataBaseException("DbMongo invalid string key", collection_name);
            }

            return string(value.get(), len);
        }

        throw DataBaseException("DbMongo bson_iter_type", collection_name, bson_iter_type(&iter));
    }

    mutable mutex _storageLocker {};
    nptr<mongoc_client_t> _client FO_TSA_GUARDED_BY(_storageLocker) {};
    nptr<mongoc_database_t> _database FO_TSA_GUARDED_BY(_storageLocker) {};
    unordered_map<string, ptr<mongoc_collection_t>> _collections FO_TSA_GUARDED_BY(_storageLocker) {};
    char _escapeDot {};
};

auto CreateMongoDataBase(ptr<DataBaseSettings> db_settings, string_view uri, string_view db_name, DataBasePanicCallback panic_callback) -> unique_ptr<DataBaseImpl>
{
    InitializeBsonMemory();
    return safe_alloc::make_unique<DbMongo>(db_settings, uri, db_name, std::move(panic_callback));
}

// Once per process and never undone. mongoc_init and mongoc_cleanup are each one-shot: a cleanup frees the
// handshake data and its lock, and no later init restores them, so a second database would run on freed state
static void InitializeMongoRuntime()
{
    FO_STACK_TRACE_ENTRY();

    static std::once_flag once;
    std::call_once(once, [] {
        mongoc_init();
        mongoc_log_set_handler(&MongoLogHandler, nullptr);
    });
}

static void MongoLogHandler(mongoc_log_level_t log_level, const char* log_domain, const char* message, void* user_data)
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(user_data);

    auto domain_text = make_nptr(log_domain);
    auto message_text = make_nptr(message);
    string_view domain = domain_text ? string_view(domain_text.get()) : string_view("mongoc");
    string_view text = message_text ? string_view(message_text.get()) : string_view();

    safe_call([&] {
        switch (log_level) {
        case MONGOC_LOG_LEVEL_ERROR:
        case MONGOC_LOG_LEVEL_CRITICAL:
            logging::write(logging::type::error, "Mongo driver [{}]: {}", domain, text);
            break;
        case MONGOC_LOG_LEVEL_WARNING:
            logging::write(logging::type::warning, "Mongo driver [{}]: {}", domain, text);
            break;
        case MONGOC_LOG_LEVEL_MESSAGE:
        case MONGOC_LOG_LEVEL_INFO:
            logging::write("Mongo driver [{}]: {}", domain, text);
            break;
        default:
            break;
        }
    });
}

FO_GCC_IGNORE_WARNINGS_POP()
FO_CLANG_IGNORE_WARNINGS_POP()
#endif

FO_END_NAMESPACE
