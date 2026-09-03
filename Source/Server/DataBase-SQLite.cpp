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

FO_DISABLE_WARNINGS_PUSH()
#include <bson/bson.h>
FO_DISABLE_WARNINGS_POP()

#if FO_HAVE_SQLITE
FO_DISABLE_WARNINGS_PUSH()
#include "sqlite3.h"
FO_DISABLE_WARNINGS_POP()
#endif

#include "WinApiUndef.inc"

FO_BEGIN_NAMESPACE

#if FO_HAVE_SQLITE

// SQLite hands xFree/xRealloc/xSize only the pointer, so the usable size is carried in a header ahead
// of every block. Eight bytes keeps the payload on the alignment plain malloc would have given
struct SqliteAllocHeader
{
    uint64_t Size;
};

static_assert(sizeof(SqliteAllocHeader) == 8);
static_assert(alignof(SqliteAllocHeader) <= 8);

// The single place that steps back from the payload SQLite sees to the header in front of it
[[nodiscard]] static auto SqliteAllocHeaderOf(void* payload) noexcept -> nptr<SqliteAllocHeader>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto bytes = make_nptr(payload).reinterpret_as<uint8_t>();

    if (!bytes) {
        return {};
    }

    return make_nptr(bytes.get() - sizeof(SqliteAllocHeader)).reinterpret_as<SqliteAllocHeader>();
}

static auto SqliteMemMalloc(int32_t size) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (size <= 0) {
        return nullptr;
    }

    size_t total = numeric_cast<size_t>(size) + sizeof(SqliteAllocHeader);
    auto block = safe_alloc::malloc_raw(total).reinterpret_as<uint8_t>();
    auto header = block.reinterpret_as<SqliteAllocHeader>();
    header->Size = numeric_cast<uint64_t>(size);
    return block.get() + sizeof(SqliteAllocHeader);
}

static void SqliteMemFree(void* mem)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto header = SqliteAllocHeaderOf(mem);

    if (!header) {
        return;
    }

    safe_alloc::free_raw(header.void_cast());
}

static auto SqliteMemRealloc(void* mem, int32_t size) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (mem == nullptr) {
        return SqliteMemMalloc(size);
    }
    if (size <= 0) {
        SqliteMemFree(mem);
        return nullptr;
    }

    auto base = SqliteAllocHeaderOf(mem);
    size_t total = numeric_cast<size_t>(size) + sizeof(SqliteAllocHeader);
    auto moved = safe_alloc::realloc_raw(base.void_cast(), total).reinterpret_as<uint8_t>();
    auto header = moved.reinterpret_as<SqliteAllocHeader>();
    header->Size = numeric_cast<uint64_t>(size);
    return moved.get() + sizeof(SqliteAllocHeader);
}

static auto SqliteMemSize(void* mem) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    auto header = SqliteAllocHeaderOf(mem);

    if (!header) {
        return 0;
    }

    return numeric_cast<int32_t>(header->Size);
}

static auto SqliteMemRoundup(int32_t size) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (size <= 0) {
        return 0;
    }

    constexpr int32_t alignment = 8;

    if (size > std::numeric_limits<int32_t>::max() - (alignment - 1)) {
        return 0;
    }

    return (size + alignment - 1) & ~(alignment - 1);
}

static auto SqliteMemInit(void* app_data) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(app_data);
    return SQLITE_OK;
}

static void SqliteMemShutdown(void* app_data)
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(app_data);
}

void InitializeSQLiteRuntime()
{
    FO_STACK_TRACE_ENTRY();

    static std::once_flag once;
    static int32_t init_result = SQLITE_OK;

    std::call_once(once, [] {
        sqlite3_mem_methods methods {};
        methods.xMalloc = &SqliteMemMalloc;
        methods.xFree = &SqliteMemFree;
        methods.xRealloc = &SqliteMemRealloc;
        methods.xSize = &SqliteMemSize;
        methods.xRoundup = &SqliteMemRoundup;
        methods.xInit = &SqliteMemInit;
        methods.xShutdown = &SqliteMemShutdown;

        int32_t config = sqlite3_config(SQLITE_CONFIG_MALLOC, &methods);

        if (config != SQLITE_OK) {
            init_result = config;
            return;
        }

        init_result = sqlite3_initialize();
    });

    FO_VERIFY_AND_THROW(init_result == SQLITE_OK, "Can't initialize SQLite", init_result);
}

class DbSQLite final : public DataBaseImpl
{
public:
    DbSQLite() = delete;
    DbSQLite(const DbSQLite&) = delete;
    DbSQLite(DbSQLite&&) noexcept = delete;
    auto operator=(const DbSQLite&) = delete;
    auto operator=(DbSQLite&&) noexcept = delete;

    explicit DbSQLite(ptr<DataBaseSettings> db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) :
        DataBaseImpl(db_settings, std::move(panic_callback)),
        _storageDir {storage_dir}
    {
        FO_STACK_TRACE_ENTRY();

        InitializeSQLiteRuntime();
        fs::create_directories(storage_dir);
        OpenDataBase();
        StartCommitThread();
    }

    ~DbSQLite() override
    {
        FO_STACK_TRACE_ENTRY();

        // The commit thread drives this backend, so it must be stopped before the handle closes
        StopCommitThread();

        scoped_lock locker {_storageLocker};

        _collections.clear();

        if (_db) {
            (void)sqlite3_close(_db.get());
            _db = nullptr;
        }
    }

protected:
    [[nodiscard]] auto GetStringKeyEscaping() const noexcept -> DataBaseStringKeyEscaping override { return DataBaseStringKeyEscaping::Hex; }

    void EnsureCollection(hstring collection_name, DataBaseKeyType key_type) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(key_type);

        scoped_lock locker {_storageLocker};

        if (_collections.count(collection_name) != 0) {
            return;
        }

        string sql = strex("CREATE TABLE IF NOT EXISTS {} (key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL) WITHOUT ROWID", QuoteIdentifier(collection_name.as_str())).str();
        Execute(sql, collection_name);
        _collections.emplace(collection_name);
    }

    [[nodiscard]] auto GetAllRecordIds(hstring collection_name) const -> vector<DataBaseKey> override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        DataBaseKeyType key_type = GetCollectionKeyType(collection_name);
        VerifyCollection(collection_name);

        string sql = strex("SELECT key FROM {}", QuoteIdentifier(collection_name.as_str())).str();
        Statement stmt {*this, sql, collection_name};

        vector<DataBaseKey> ids;

        while (stmt.Step()) {
            ids.emplace_back(ParseSqliteKey(stmt.ColumnBlob(0), key_type));
        }

        return ids;
    }

    [[nodiscard]] auto GetRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        return GetRecordUnlocked(collection_name, id);
    }

    void InsertRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(!doc.Empty(), "SQLite database insert received an empty document", collection_name, id);

        scoped_lock locker {_storageLocker};

        VerifyCollection(collection_name);

        auto key = MakeSqliteKey(id, GetCollectionKeyType(collection_name));
        string sql = strex("INSERT INTO {} (key, value) VALUES (?, ?)", QuoteIdentifier(collection_name.as_str())).str();

        WriteDocument(sql, collection_name, id, key, doc);
    }

    void UpdateRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(!doc.Empty(), "SQLite database update received an empty document", collection_name, id);

        scoped_lock locker {_storageLocker};

        VerifyCollection(collection_name);

        auto actual_doc = GetRecordUnlocked(collection_name, id);

        if (actual_doc.Empty()) {
            throw DataBaseException("DbSQLite Document not found", collection_name, FormatSqliteDbKey(id));
        }

        for (auto&& [doc_key, doc_value] : doc) {
            actual_doc.Assign(doc_key, doc_value.Copy());
        }

        auto key = MakeSqliteKey(id, GetCollectionKeyType(collection_name));
        string sql = strex("UPDATE {} SET value = ?2 WHERE key = ?1", QuoteIdentifier(collection_name.as_str())).str();

        WriteDocument(sql, collection_name, id, key, actual_doc);
    }

    void DeleteRecord(hstring collection_name, const DataBaseKey& id) override
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        VerifyCollection(collection_name);

        auto key = MakeSqliteKey(id, GetCollectionKeyType(collection_name));
        string sql = strex("DELETE FROM {} WHERE key = ?", QuoteIdentifier(collection_name.as_str())).str();

        Statement stmt {*this, sql, collection_name};
        stmt.BindBlob(1, key);

        while (stmt.Step()) {
            // No rows are produced; drain for symmetry with the other statements
        }
    }

    auto TryReconnect() -> bool override
    {
        FO_STACK_TRACE_ENTRY();

        try {
            scoped_lock locker {_storageLocker};

            // BEGIN IMMEDIATE takes the write lock, which proves the file is still writable, and the
            // rollback leaves nothing behind
            Execute("BEGIN IMMEDIATE", hstring());
            Execute("ROLLBACK", hstring());
            return true;
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
            return false;
        }
    }

private:
    class Statement
    {
    public:
        Statement(const DbSQLite& db, string_view sql, hstring context) FO_TSA_REQUIRES(db._storageLocker)
        {
            FO_STACK_TRACE_ENTRY();

            ptr<sqlite3> db_handle = db.GetHandle();
            nptr<sqlite3_stmt> stmt;
            int32_t prepare = sqlite3_prepare_v2(db_handle.get(), sql.data(), numeric_cast<int32_t>(sql.size()), stmt.get_pp(), nullptr);

            if (prepare != SQLITE_OK) {
                throw DataBaseException("DbSQLite sqlite3_prepare_v2", context, sql, db.LastError());
            }

            FO_VERIFY_AND_THROW(stmt, "Prepared statement is null");
            _stmt = stmt;
            _db = db_handle;
            _context = context;
        }

        Statement(const Statement&) = delete;
        Statement(Statement&&) noexcept = delete;
        auto operator=(const Statement&) = delete;
        auto operator=(Statement&&) noexcept = delete;

        ~Statement()
        {
            FO_STACK_TRACE_ENTRY();

            if (_stmt) {
                (void)sqlite3_finalize(_stmt.get());
            }
        }

        void BindBlob(int32_t index, const vector<uint8_t>& data)
        {
            FO_STACK_TRACE_ENTRY();

            // SQLITE_TRANSIENT makes SQLite copy the bytes, so the caller's buffer need not outlive
            // the bind
            int32_t bind = sqlite3_bind_blob(_stmt.get(), index, data.data(), numeric_cast<int32_t>(data.size()), SQLITE_TRANSIENT);

            if (bind != SQLITE_OK) {
                throw DataBaseException("DbSQLite sqlite3_bind_blob", _context, LastError());
            }
        }

        void BindBlob(int32_t index, const_span<uint8_t> data)
        {
            FO_STACK_TRACE_ENTRY();

            int32_t bind = sqlite3_bind_blob(_stmt.get(), index, data.data(), numeric_cast<int32_t>(data.size()), SQLITE_TRANSIENT);

            if (bind != SQLITE_OK) {
                throw DataBaseException("DbSQLite sqlite3_bind_blob", _context, LastError());
            }
        }

        [[nodiscard]] auto Step() -> bool
        {
            FO_STACK_TRACE_ENTRY();

            int32_t step = sqlite3_step(_stmt.get());

            if (step == SQLITE_ROW) {
                return true;
            }
            if (step == SQLITE_DONE) {
                return false;
            }

            throw DataBaseException("DbSQLite sqlite3_step", _context, LastError());
        }

        [[nodiscard]] auto ColumnBlob(int32_t index) const -> const_span<uint8_t>
        {
            FO_STACK_TRACE_ENTRY();

            // The statement stays mutable for the C API even when read through a const accessor
            auto stmt = make_ptr(_stmt.get_no_const());
            auto data = cast_from_void<const uint8_t*>(sqlite3_column_blob(stmt.get(), index));
            int32_t size = sqlite3_column_bytes(stmt.get(), index);
            FO_VERIFY_AND_THROW(size >= 0, "Negative column size", _context, size);

            if (size == 0) {
                return {};
            }

            FO_VERIFY_AND_THROW(data, "Column payload is null with a non-zero size", _context);
            return make_const_span(data.get(), numeric_cast<size_t>(size));
        }

    private:
        [[nodiscard]] auto LastError() const -> string
        {
            FO_STACK_TRACE_ENTRY();

            auto text = make_nptr(sqlite3_errmsg(make_ptr(_db.get_no_const()).get()));
            return text ? string(text.get()) : string("unknown");
        }

        nptr<sqlite3_stmt> _stmt {};
        nptr<sqlite3> _db {};
        hstring _context {};
    };

    void OpenDataBase()
    {
        FO_STACK_TRACE_ENTRY();

        scoped_lock locker {_storageLocker};

        string db_path = strex("{}/Storage.sqlite", _storageDir);
        auto db_path_ptr = make_ptr(db_path.c_str());

        nptr<sqlite3> db;
        int32_t open = sqlite3_open_v2(db_path_ptr.get(), db.get_pp(), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);

        if (open != SQLITE_OK) {
            // sqlite3_open_v2 hands back a handle even on failure so the error can be read from it
            string error = db ? string(sqlite3_errmsg(db.get())) : string("unknown");

            if (db) {
                (void)sqlite3_close(db.get());
            }

            throw DataBaseException("DbSQLite sqlite3_open_v2", db_path, open, error);
        }

        FO_VERIFY_AND_THROW(db, "Opened database handle is null");
        _db = db;

        auto close_db = scope_fail([&]() FO_TSA_REQUIRES(_storageLocker) noexcept {
            (void)sqlite3_close(_db.get());
            _db = nullptr;
        });

        // WAL keeps readers from blocking the writer, which matches the commit-thread model
        Execute("PRAGMA journal_mode = WAL", hstring());
        // NORMAL pairs with WAL: a crash may lose the last transactions but cannot corrupt the database, and
        // FULL closes only that window at a real write cost
        Execute("PRAGMA synchronous = NORMAL", hstring());
        Execute("PRAGMA foreign_keys = ON", hstring());
    }

    [[nodiscard]] ptr<sqlite3> GetHandle() const FO_TSA_REQUIRES(_storageLocker)
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(_db, "SQLite database is not open");
        return make_ptr(_db.get_no_const());
    }

    [[nodiscard]] string LastError() const FO_TSA_REQUIRES(_storageLocker)
    {
        FO_STACK_TRACE_ENTRY();

        if (!_db) {
            return "database is not open";
        }

        auto text = make_nptr(sqlite3_errmsg(make_ptr(_db.get_no_const()).get()));
        return text ? string(text.get()) : string("unknown");
    }

    void Execute(string_view sql, hstring context) const FO_TSA_REQUIRES(_storageLocker)
    {
        FO_STACK_TRACE_ENTRY();

        Statement stmt {*this, sql, context};

        while (stmt.Step()) {
            // Drain any rows a PRAGMA may return
        }
    }

    void WriteDocument(string_view sql, hstring collection_name, const DataBaseKey& id, const vector<uint8_t>& key, const AnyData::Document& doc) FO_TSA_REQUIRES(_storageLocker)
    {
        FO_STACK_TRACE_ENTRY();

        bson_t bson;
        bson_init(&bson);
        auto destroy_bson = scope_exit([&]() noexcept { bson_destroy(&bson); });

        DocumentToBson(doc, &bson);

        auto bson_data = make_nptr(bson_get_data(&bson));

        if (!bson_data) {
            throw DataBaseException("DbSQLite bson_get_data", collection_name);
        }

        Statement stmt {*this, sql, collection_name};
        stmt.BindBlob(1, key);
        stmt.BindBlob(2, make_const_span(bson_data.get(), numeric_cast<size_t>(bson.len)));

        while (stmt.Step()) {
            // No rows are produced by INSERT/UPDATE
        }

        if (sqlite3_changes(GetHandle().get()) == 0) {
            throw DataBaseException("DbSQLite write affected no rows", collection_name, FormatSqliteDbKey(id));
        }
    }

    [[nodiscard]] AnyData::Document GetRecordUnlocked(hstring collection_name, const DataBaseKey& id) const FO_TSA_REQUIRES(_storageLocker)
    {
        FO_STACK_TRACE_ENTRY();

        VerifyCollection(collection_name);

        auto key = MakeSqliteKey(id, GetCollectionKeyType(collection_name));
        string sql = strex("SELECT value FROM {} WHERE key = ?", QuoteIdentifier(collection_name.as_str())).str();

        Statement stmt {*this, sql, collection_name};
        stmt.BindBlob(1, key);

        if (!stmt.Step()) {
            return {};
        }

        auto value = stmt.ColumnBlob(0);

        bson_t bson;

        if (!bson_init_static(&bson, value.data(), value.size())) {
            throw DataBaseException("DbSQLite bson_init_static", collection_name);
        }

        AnyData::Document doc;
        BsonToDocument(&bson, doc);
        return doc;
    }

    void VerifyCollection(hstring collection_name) const FO_TSA_REQUIRES(_storageLocker)
    {
        FO_STACK_TRACE_ENTRY();

        if (_collections.count(collection_name) == 0) {
            throw DataBaseException("DbSQLite Invalid collection", collection_name);
        }
    }

    // Collection names come from engine metadata rather than user input, but they still reach SQL as
    // identifiers, so they are quoted properly instead of interpolated raw
    [[nodiscard]] static auto QuoteIdentifier(string_view name) -> string
    {
        FO_STACK_TRACE_ENTRY();

        string quoted;
        quoted.reserve(name.size() + 2);
        quoted += '"';

        for (char ch : name) {
            if (ch == '"') {
                quoted += '"';
            }

            quoted += ch;
        }

        quoted += '"';
        return quoted;
    }

    static auto MakeSqliteKey(const DataBaseKey& key, DataBaseKeyType key_type) -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        if (key_type == DataBaseKeyType::IntId) {
            nptr<const ident_t> numeric_key = std::get_if<ident_t>(&key);

            if (!numeric_key) {
                throw DataBaseException("DbSQLite Invalid numeric key type", FormatSqliteDbKey(key));
            }

            static_assert(sizeof(ident_t) == sizeof(int64_t));

            vector<uint8_t> result(sizeof(int64_t));
            int64_t value = numeric_key->underlying_value();
            memory::copy(result.data(), &value, sizeof(value));
            return result;
        }

        string key_str = std::get<string>(key);
        return vector<uint8_t>(key_str.begin(), key_str.end());
    }

    static auto ParseSqliteKey(const_span<uint8_t> key_data, DataBaseKeyType key_type) -> DataBaseKey
    {
        FO_STACK_TRACE_ENTRY();

        if (key_type == DataBaseKeyType::IntId) {
            if (key_data.size() != sizeof(int64_t)) {
                throw DataBaseException("DbSQLite invalid numeric key size", key_data.size());
            }

            int64_t value {};
            memory::copy(&value, key_data.data(), sizeof(value));

            if (value <= 0) {
                throw DataBaseException("DbSQLite invalid numeric key", value);
            }

            return ident_t {value};
        }

        if (key_data.empty()) {
            throw DataBaseException("DbSQLite empty key");
        }

        return string {span_to_string(key_data)};
    }

    static auto FormatSqliteDbKey(const DataBaseKey& key) -> string
    {
        FO_STACK_TRACE_ENTRY();

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

    string _storageDir {};
    mutable mutex _storageLocker {};
    nptr<sqlite3> _db FO_TSA_GUARDED_BY(_storageLocker) {};
    set<hstring> _collections FO_TSA_GUARDED_BY(_storageLocker) {};
};

auto CreateSQLiteDataBase(ptr<DataBaseSettings> db_settings, string_view storage_dir, DataBasePanicCallback panic_callback) -> unique_ptr<DataBaseImpl>
{
    InitializeBsonMemory();
    return safe_alloc::make_unique<DbSQLite>(db_settings, storage_dir, std::move(panic_callback));
}

#endif

FO_END_NAMESPACE
