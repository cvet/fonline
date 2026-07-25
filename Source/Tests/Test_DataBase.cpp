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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "catch_amalgamated.hpp"

#include "DataBase.h"
#include "DiskFileSystem.h"

#if FO_HAVE_UNQLITE
FO_DISABLE_WARNINGS_PUSH()
#include <unqlite.h>
FO_DISABLE_WARNINGS_POP()
#endif

#include <filesystem>

FO_BEGIN_NAMESPACE

namespace
{
    template<typename T>
    [[nodiscard]] auto FixedSettingForOverride(const T& setting) noexcept -> ptr<T>
    {
        FO_NO_STACK_TRACE_ENTRY();

        return const_cast<T*>(&setting);
    }

    class TestDataBase final : public DataBaseImpl
    {
    public:
        explicit TestDataBase(DataBaseSettings& db_settings, DataBaseStringKeyEscaping string_key_escaping = DataBaseStringKeyEscaping::Raw) :
            DataBaseImpl(SettingsPtr(db_settings), nullptr),
            _stringKeyEscaping {string_key_escaping}
        {
            // _hashes (member) must outlive _collectionKeyTypes/_collectionNames in the base class,
            // because the hstrings registered below hold pointers into _hashes._hashStorage.
            InitializeCollections({
                {_hashes.ToHashedString("test_collection"), DataBaseKeyType::IntId},
                {_hashes.ToHashedString("test_string_collection"), DataBaseKeyType::String},
            });
            StartCommitThread();
        }

        ~TestDataBase() override { StopCommitThread(); }

        [[nodiscard]] auto GetStringKeyEscaping() const noexcept -> DataBaseStringKeyEscaping override { return _stringKeyEscaping; }

        auto GetAllIds(hstring collection_name) const -> vector<DataBaseKey>
        {
            auto key_type = GetCollectionKeyType(collection_name);
            auto ids = GetAllRecordIds(collection_name);

            for (auto& id : ids) {
                if (GetDbKeyType(id) != key_type) {
                    throw DataBaseException("Database collection returned invalid key type", collection_name, id);
                }

                if (key_type == DataBaseKeyType::String) {
                    const auto& value = std::get<string>(id);

                    if (!strvex(value).is_valid_utf8()) {
                        throw DataBaseException("Invalid database string key utf8", value);
                    }
                }
            }

            return ids;
        }

        auto GetAllRecordIds(hstring collection_name) const -> vector<DataBaseKey> override
        {
            scoped_lock locker {_collectionsLocker};

            if (_collections.count(collection_name) == 0) {
                return {};
            }

            vector<DataBaseKey> ids;

            for (const auto& id : _collections.at(collection_name) | std::views::keys) {
                ids.emplace_back(id);
            }

            return ids;
        }

        void PrimeRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) { PrimeRecord(collection_name, DataBaseKey {id}, doc); }

        void PrimeRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc)
        {
            scoped_lock locker {_collectionsLocker};
            _collections[collection_name][id] = doc.Copy();
        }

        auto SnapshotRecord(hstring collection_name, ident_t id) const -> AnyData::Document { return SnapshotRecord(collection_name, DataBaseKey {id}); }

        auto SnapshotRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document
        {
            scoped_lock locker {_collectionsLocker};

            if (_collections.count(collection_name) == 0) {
                return {};
            }

            const auto& collection = _collections.at(collection_name);
            auto it = collection.find(id);
            return it != collection.end() ? it->second.Copy() : AnyData::Document {};
        }

        [[nodiscard]] auto GetRecordReadCount(ident_t id) const -> size_t
        {
            scoped_lock locker {_readStatsLocker};
            return _recordReadCount.contains(id) ? _recordReadCount.at(id) : 0;
        }

        void SetOnGetRecord(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};
            _onGetRecord = std::move(callback);
        }

        void SetStrictRecordSemantics(bool enabled = true)
        {
            scoped_lock locker {_collectionsLocker};
            _strictRecordSemantics = enabled;
        }

        void SetBackendWriteFailure(bool enabled = true)
        {
            scoped_lock locker {_collectionsLocker};
            _failBackendWrites = enabled;
        }

        void WaitUntilCommitOperationWrittenToOpLog()
        {
            unique_lock locker {_mirrorLocker};
            _mirrorCv.wait(locker, [this]() FO_TSA_REQUIRES(_mirrorLocker) { return _pendingChangesMirrored; });
        }

        void WaitUntilPendingChangesRestored()
        {
            unique_lock locker {_restoreLocker};
            _restoreCv.wait(locker, [this]() FO_TSA_REQUIRES(_restoreLocker) { return _pendingChangesRestored; });
        }

        void BlockRecordRead(ident_t id)
        {
            scoped_lock locker {_blockedReadLocker};
            _blockedReadEnabled = true;
            _blockedReadId = id;
            _blockedReadEntered = false;
        }

        void WaitUntilBlockedReadEntered()
        {
            unique_lock locker {_blockedReadLocker};
            _blockedReadCv.wait(locker, [this]() FO_TSA_REQUIRES(_blockedReadLocker) { return _blockedReadEntered; });
        }

        void UnblockRecordRead()
        {
            {
                scoped_lock locker {_blockedReadLocker};
                _blockedReadEnabled = false;
            }

            _blockedReadCv.notify_all();
        }

    protected:
        void EnsureCollection(hstring collection_name, DataBaseKeyType key_type) override
        {
            ignore_unused(key_type);

            scoped_lock locker {_collectionsLocker};
            _collections.try_emplace(collection_name);
        }

        auto TryReconnect() -> bool override
        {
            scoped_lock locker {_collectionsLocker};
            return !_failBackendWrites;
        }

        void OnCommitOperationWrittenToOpLog() override
        {
            {
                scoped_lock locker {_mirrorLocker};
                _pendingChangesMirrored = true;
            }

            _mirrorCv.notify_all();
        }

        void OnPendingChangesRestored() override
        {
            {
                scoped_lock locker {_restoreLocker};
                _pendingChangesRestored = true;
            }

            _restoreCv.notify_all();
        }

        auto GetRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document override
        {
            {
                scoped_lock locker {_readStatsLocker};
                _recordReadCount[id]++;
            }

            {
                unique_lock locker {_blockedReadLocker};

                if (_blockedReadEnabled && _blockedReadId == id) {
                    _blockedReadEntered = true;
                    _blockedReadCv.notify_all();
                    _blockedReadCv.wait(locker, [this]() FO_TSA_REQUIRES(_blockedReadLocker) { return !_blockedReadEnabled; });
                }
            }

            {
                function<void()> callback;
                {
                    scoped_lock locker {_callbackLocker};
                    callback = std::move(_onGetRecord);
                    _onGetRecord = {};
                }

                if (callback) {
                    callback();
                }
            }

            scoped_lock locker {_collectionsLocker};

            if (_collections.count(collection_name) == 0) {
                return {};
            }

            const auto& collection = _collections.at(collection_name);
            auto it = collection.find(id);
            return it != collection.end() ? it->second.Copy() : AnyData::Document {};
        }

        void InsertRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
        {
            scoped_lock locker {_collectionsLocker};

            if (_failBackendWrites) {
                throw DataBaseException("Simulated database write failure");
            }

            auto& collection = _collections[collection_name];

            if (_strictRecordSemantics && collection.contains(id)) {
                throw DataBaseException("Strict test insert duplicate");
            }

            collection.emplace(id, doc.Copy());
        }

        void UpdateRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
        {
            scoped_lock locker {_collectionsLocker};

            if (_failBackendWrites) {
                throw DataBaseException("Simulated database write failure");
            }

            auto& collection = _collections[collection_name];

            if (_strictRecordSemantics && !collection.contains(id)) {
                throw DataBaseException("Strict test update missing record");
            }

            auto& target_doc = collection[id];

            for (auto&& [doc_key, doc_value] : doc) {
                target_doc.Assign(doc_key, doc_value.Copy());
            }
        }

        void DeleteRecord(hstring collection_name, const DataBaseKey& id) override
        {
            scoped_lock locker {_collectionsLocker};

            if (_failBackendWrites) {
                throw DataBaseException("Simulated database write failure");
            }

            auto& collection = _collections[collection_name];

            if (_strictRecordSemantics && !collection.contains(id)) {
                throw DataBaseException("Strict test delete missing record");
            }

            if (_collections.count(collection_name) != 0) {
                collection.erase(id);
            }
        }

    private:
        static auto SettingsPtr(DataBaseSettings& settings) noexcept -> ptr<DataBaseSettings>
        {
            FO_NO_STACK_TRACE_ENTRY();

            return &settings;
        }

        HashStorage _hashes {};
        DataBaseStringKeyEscaping _stringKeyEscaping {};
        mutable mutex _collectionsLocker {};
        mutable mutex _callbackLocker {};
        mutable mutex _blockedReadLocker {};
        mutable mutex _readStatsLocker {};
        mutable mutex _mirrorLocker {};
        mutable mutex _restoreLocker {};
        mutable std::condition_variable_any _blockedReadCv {};
        mutable std::condition_variable_any _mirrorCv {};
        mutable std::condition_variable_any _restoreCv {};
        mutable DataBase::Collections _collections FO_TSA_GUARDED_BY(_collectionsLocker) {};
        mutable unordered_map<DataBaseKey, size_t> _recordReadCount FO_TSA_GUARDED_BY(_readStatsLocker) {};
        mutable function<void()> _onGetRecord FO_TSA_GUARDED_BY(_callbackLocker) {};
        mutable bool _blockedReadEnabled FO_TSA_GUARDED_BY(_blockedReadLocker) {};
        mutable bool _blockedReadEntered FO_TSA_GUARDED_BY(_blockedReadLocker) {};
        mutable DataBaseKey _blockedReadId FO_TSA_GUARDED_BY(_blockedReadLocker) {ident_t {}};
        bool _pendingChangesMirrored FO_TSA_GUARDED_BY(_mirrorLocker) {};
        bool _pendingChangesRestored FO_TSA_GUARDED_BY(_restoreLocker) {};
        bool _strictRecordSemantics FO_TSA_GUARDED_BY(_collectionsLocker) {};
        bool _failBackendWrites FO_TSA_GUARDED_BY(_collectionsLocker) {};
    };

    auto MakeDoc(std::initializer_list<pair<string_view, int64_t>> values) -> AnyData::Document
    {
        AnyData::Document doc;

        for (const auto& [key, value] : values) {
            doc.Assign(string(key), value);
        }

        return doc;
    }

    auto MakeComplexDoc() -> AnyData::Document
    {
        AnyData::Array values;
        values.EmplaceBack(numeric_cast<int64_t>(7));
        values.EmplaceBack(2.5);
        values.EmplaceBack(false);
        values.EmplaceBack(string {"array"});

        AnyData::Dict nested;
        nested.Emplace("flag", true);
        nested.Emplace("label", string {"nested"});
        nested.Emplace("score", numeric_cast<int64_t>(9));

        AnyData::Document doc;
        doc.Assign("int", numeric_cast<int64_t>(42));
        doc.Assign("float", 3.25);
        doc.Assign("bool", true);
        doc.Assign("string", string {"text"});
        doc.Assign("array", AnyData::Value {std::move(values)});
        doc.Assign("dict", AnyData::Value {std::move(nested)});
        return doc;
    }

    void CheckComplexDoc(const AnyData::Document& doc)
    {
        REQUIRE(!doc.Empty());
        CHECK(doc["int"].AsInt64() == 42);
        CHECK(doc["float"].AsDouble() == 3.25);
        CHECK(doc["bool"].AsBool());
        CHECK(doc["string"].AsString() == "text");

        const auto& values = doc["array"].AsArray();
        REQUIRE(values.Size() == 4);
        CHECK(values[0].AsInt64() == 7);
        CHECK(values[1].AsDouble() == 2.5);
        CHECK_FALSE(values[2].AsBool());
        CHECK(values[3].AsString() == "array");

        const auto& nested = doc["dict"].AsDict();
        REQUIRE(nested.Size() == 3);
        CHECK(nested["flag"].AsBool());
        CHECK(nested["label"].AsString() == "nested");
        CHECK(nested["score"].AsInt64() == 9);
    }

    class ScopedRecoveryLogs final
    {
    public:
        explicit ScopedRecoveryLogs(string_view test_name)
        {
            auto unique_suffix = std::chrono::steady_clock::now().time_since_epoch().count();
            string dir_name = strex("lf-db-tests-{}-{}", test_name, unique_suffix).str();

            _dir = std::filesystem::temp_directory_path() / std::filesystem::path {fs_make_path(dir_name)};
            std::filesystem::create_directories(_dir);
            _pendingPath = fs_path_to_string(_dir / "DbPendingChanges.oplog");
            _committedPath = fs_path_to_string(_dir / "DbPendingChanges-committed.oplog");
        }

        ~ScopedRecoveryLogs()
        {
            std::error_code ec;
            std::filesystem::remove_all(_dir, ec);
        }

        [[nodiscard]] auto Dir() const -> ptr<const std::filesystem::path> { return &_dir; }
        [[nodiscard]] auto PendingPath() const -> string_view { return _pendingPath; }
        [[nodiscard]] auto CommittedPath() const -> string_view { return _committedPath; }

    private:
        std::filesystem::path _dir {};
        string _pendingPath {};
        string _committedPath {};
    };

    class ScopedCurrentPath final
    {
    public:
        explicit ScopedCurrentPath(const std::filesystem::path& path) :
            _prevPath {std::filesystem::current_path()}
        {
            std::filesystem::current_path(path);
        }

        ~ScopedCurrentPath()
        {
            std::error_code ec;
            std::filesystem::current_path(_prevPath, ec);
        }

    private:
        std::filesystem::path _prevPath {};
    };

    void ConfigureRecoverySettings(GlobalSettings& settings, string_view oplog_path)
    {
        *FixedSettingForOverride(settings.OpLogEnabled) = true;
        *FixedSettingForOverride(settings.OpLogPath) = string(oplog_path);
        *FixedSettingForOverride(settings.ReconnectRetryPeriod) = 20;
        *FixedSettingForOverride(settings.PanicOpLogSizeThreshold) = 1024 * 1024;
        *FixedSettingForOverride(settings.PanicShutdownTimeout) = 1;
    }

    void WriteRecoveryLogs(const ScopedRecoveryLogs& recovery_logs, string_view pending_content, string_view committed_content = {})
    {
        REQUIRE(fs_write_file(recovery_logs.PendingPath(), pending_content));
        REQUIRE(fs_write_file(recovery_logs.CommittedPath(), committed_content));
    }

    void CheckRecoveryLogsCleared(const ScopedRecoveryLogs& recovery_logs)
    {
        auto pending_content = fs_read_file(recovery_logs.PendingPath());
        auto committed_content = fs_read_file(recovery_logs.CommittedPath());

        REQUIRE(pending_content.has_value());
        REQUIRE(committed_content.has_value());
        CHECK(pending_content->empty());
        CHECK(committed_content->empty());
    }

#if FO_HAVE_UNQLITE
    void StoreRawUnQLiteRecord(const std::filesystem::path& db_path, const vector<uint8_t>& key_data, string_view value)
    {
        FO_STACK_TRACE_ENTRY();

        REQUIRE(fs_create_directories(fs_path_to_string(db_path.parent_path())));

        unqlite* db = nullptr;
        REQUIRE(unqlite_open(&db, fs_path_to_string(db_path).c_str(), UNQLITE_OPEN_CREATE) == UNQLITE_OK);

        auto close_db = scope_exit([&]() noexcept { unqlite_close(db); });

        REQUIRE(unqlite_kv_store(db, key_data.data(), numeric_cast<int>(key_data.size()), value.data(), numeric_cast<unqlite_int64>(value.size())) == UNQLITE_OK);
        REQUIRE(unqlite_commit(db) == UNQLITE_OK);
    }
#endif
} // namespace

TEST_CASE("DataBaseCommitOperationsPreserveOrder")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_collection");
    ident_t record_id = ident_t {1001};

    db.Insert(collection, record_id, MakeDoc({{"a", 1}}));
    db.Update(collection, record_id, "b", numeric_cast<int64_t>(2));
    db.Update(collection, record_id, "a", numeric_cast<int64_t>(3));

    db.StartCommitChanges();
    db.WaitCommitChanges();

    auto committed_doc = db.SnapshotRecord(collection, record_id);
    REQUIRE(!committed_doc.Empty());
    CHECK(committed_doc["a"].AsInt64() == 3);
    CHECK(committed_doc["b"].AsInt64() == 2);
}

TEST_CASE("DataBaseRejectsNonFiniteFloatUpdates")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_collection");
    ident_t record_id = ident_t {1001};

    db.PrimeRecord(collection, record_id, MakeDoc({{"value", 1}}));

    CHECK_THROWS_AS(db.Update(collection, record_id, "value", std::numeric_limits<float64_t>::quiet_NaN()), DataBaseException);
    CHECK_THROWS_AS(db.Update(collection, record_id, "value", std::numeric_limits<float64_t>::infinity()), DataBaseException);

    auto doc = db.SnapshotRecord(collection, record_id);
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseRejectsNonFiniteFloatDocuments")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_collection");

    AnyData::Document plain_doc;
    plain_doc.Assign("value", std::numeric_limits<float64_t>::infinity());
    CHECK_THROWS_AS(db.Insert(collection, ident_t {1001}, plain_doc), DataBaseException);

    AnyData::Array values;
    values.EmplaceBack(numeric_cast<int64_t>(1));
    values.EmplaceBack(std::numeric_limits<float64_t>::quiet_NaN());

    AnyData::Document array_doc;
    array_doc.Assign("values", AnyData::Value {std::move(values)});
    CHECK_THROWS_AS(db.Insert(collection, ident_t {1002}, array_doc), DataBaseException);

    AnyData::Dict nested;
    nested.Emplace("value", std::numeric_limits<float64_t>::infinity());
    CHECK_THROWS_AS(db.Update(collection, ident_t {1003}, "nested", AnyData::Value {std::move(nested)}), DataBaseException);
}

TEST_CASE("DataBaseGetDocumentAppliesConcurrentPendingChange")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_collection");
    ident_t record_id = ident_t {1001};

    db.PrimeRecord(collection, record_id, MakeDoc({{"value", 1}}));
    db.SetOnGetRecord([&] { db.Update(collection, record_id, "value", numeric_cast<int64_t>(2)); });

    auto doc = db.GetDocument(collection, record_id);

    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 2);
    CHECK(db.GetRecordReadCount(record_id) == 1);

    db.ClearChanges();
}

TEST_CASE("DataBaseGetDocumentAppliesSameRecordChangesUnderLoad")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_collection");
    ident_t record_id = ident_t {1001};

    constexpr size_t worker_count = 4;
    constexpr int64_t updates_per_worker = 5;
    unordered_set<int64_t> expected_values;

    for (size_t worker_index = 0; worker_index < worker_count; worker_index++) {
        for (int64_t update_index = 0; update_index < updates_per_worker; update_index++) {
            expected_values.emplace(numeric_cast<int64_t>(100 + worker_index * 10 + update_index));
        }
    }

    db.PrimeRecord(collection, record_id, MakeDoc({{"value", 1}}));
    db.BlockRecordRead(record_id);

    std::promise<AnyData::Document> doc_promise;
    auto doc_future = doc_promise.get_future();
    std::thread reader {[&] {
        try {
            doc_promise.set_value(db.GetDocument(collection, record_id));
        }
        catch (...) {
            doc_promise.set_exception(std::current_exception());
        }
    }};

    db.WaitUntilBlockedReadEntered();

    vector<std::thread> workers;
    workers.reserve(worker_count);

    for (size_t worker_index = 0; worker_index < worker_count; worker_index++) {
        workers.emplace_back([&, worker_index] {
            for (int64_t update_index = 0; update_index < updates_per_worker; update_index++) {
                int64_t value = numeric_cast<int64_t>(100 + worker_index * 10 + update_index);
                db.Update(collection, record_id, "value", value);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    db.UnblockRecordRead();

    auto doc = doc_future.get();
    reader.join();

    REQUIRE(!doc.Empty());
    CHECK(expected_values.contains(doc["value"].AsInt64()));
    CHECK(db.GetRecordReadCount(record_id) == 1);

    db.ClearChanges();
}

TEST_CASE("DataBaseGetDocumentAppliesCommittedChangeCompletedDuringRead")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_collection");
    ident_t record_id = ident_t {1001};

    db.PrimeRecord(collection, record_id, MakeDoc({{"value", 1}}));
    db.StartCommitChanges();
    db.BlockRecordRead(record_id);

    std::promise<AnyData::Document> doc_promise;
    auto doc_future = doc_promise.get_future();
    std::thread reader {[&] {
        try {
            doc_promise.set_value(db.GetDocument(collection, record_id));
        }
        catch (...) {
            doc_promise.set_exception(std::current_exception());
        }
    }};

    db.WaitUntilBlockedReadEntered();
    db.Update(collection, record_id, "value", numeric_cast<int64_t>(2));
    db.WaitCommitChanges();
    db.UnblockRecordRead();

    auto doc = doc_future.get();
    reader.join();

    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 2);
    CHECK(db.GetRecordReadCount(record_id) >= 2);
}

TEST_CASE("DataBaseGetDocumentIgnoresOtherRecordChanges")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_collection");
    ident_t target_id = ident_t {1001};
    ident_t other_id = ident_t {1002};

    db.PrimeRecord(collection, target_id, MakeDoc({{"value", 1}}));
    db.PrimeRecord(collection, other_id, MakeDoc({{"value", 10}}));
    db.SetOnGetRecord([&] { db.Update(collection, other_id, "value", numeric_cast<int64_t>(20)); });

    auto doc = db.GetDocument(collection, target_id);

    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
    CHECK(db.GetRecordReadCount(target_id) == 1);

    db.ClearChanges();
}

TEST_CASE("DataBaseGetDocumentIgnoresOtherRecordChangesUnderLoad")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_collection");
    ident_t target_id = ident_t {1001};

    constexpr size_t worker_count = 4;
    constexpr size_t records_per_worker = 6;

    db.PrimeRecord(collection, target_id, MakeDoc({{"value", 1}}));

    for (size_t worker_index = 0; worker_index < worker_count; worker_index++) {
        for (size_t record_index = 0; record_index < records_per_worker; record_index++) {
            ident_t other_id = ident_t {numeric_cast<int64_t>(2000 + worker_index * 100 + record_index)};
            db.PrimeRecord(collection, other_id, MakeDoc({{"value", 10}}));
        }
    }

    db.BlockRecordRead(target_id);

    std::promise<AnyData::Document> target_promise;
    auto target_future = target_promise.get_future();
    std::thread target_reader {[&] {
        try {
            target_promise.set_value(db.GetDocument(collection, target_id));
        }
        catch (...) {
            target_promise.set_exception(std::current_exception());
        }
    }};

    db.WaitUntilBlockedReadEntered();

    vector<std::thread> workers;
    workers.reserve(worker_count);

    for (size_t worker_index = 0; worker_index < worker_count; worker_index++) {
        workers.emplace_back([&, worker_index] {
            for (size_t record_index = 0; record_index < records_per_worker; record_index++) {
                ident_t other_id = ident_t {numeric_cast<int64_t>(2000 + worker_index * 100 + record_index)};

                for (int64_t update_index = 0; update_index < 5; update_index++) {
                    db.Update(collection, other_id, "value", numeric_cast<int64_t>(1000 + worker_index * 100 + record_index * 10 + update_index));
                }
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    db.UnblockRecordRead();

    auto target_doc = target_future.get();
    target_reader.join();

    REQUIRE(!target_doc.Empty());
    CHECK(target_doc["value"].AsInt64() == 1);
    CHECK(db.GetRecordReadCount(target_id) == 1);

    db.ClearChanges();
}

TEST_CASE("DataBaseConcurrentProducersCommitAllRecords")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_collection");

    constexpr size_t thread_count = 4;
    constexpr size_t records_per_thread = 8;

    db.StartCommitChanges();

    vector<std::thread> workers;
    workers.reserve(thread_count);

    for (size_t thread_index = 0; thread_index < thread_count; thread_index++) {
        workers.emplace_back([&, thread_index] {
            for (size_t record_index = 0; record_index < records_per_thread; record_index++) {
                ident_t id = ident_t {numeric_cast<int64_t>(thread_index * 100 + record_index + 1)};
                db.Insert(collection, id, MakeDoc({{"thread", numeric_cast<int64_t>(thread_index)}}));
                db.Update(collection, id, "record", numeric_cast<int64_t>(record_index));
                db.Update(collection, id, "value", numeric_cast<int64_t>(thread_index * 1000 + record_index));
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    db.WaitCommitChanges();

    for (size_t thread_index = 0; thread_index < thread_count; thread_index++) {
        for (size_t record_index = 0; record_index < records_per_thread; record_index++) {
            ident_t id = ident_t {numeric_cast<int64_t>(thread_index * 100 + record_index + 1)};
            auto doc = db.SnapshotRecord(collection, id);

            REQUIRE(!doc.Empty());
            CHECK(doc["thread"].AsInt64() == numeric_cast<int64_t>(thread_index));
            CHECK(doc["record"].AsInt64() == numeric_cast<int64_t>(record_index));
            CHECK(doc["value"].AsInt64() == numeric_cast<int64_t>(thread_index * 1000 + record_index));
        }
    }
}

TEST_CASE("DataBaseSlowReadDoesNotBlockOtherReads")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_collection");
    ident_t blocked_id = ident_t {1001};
    ident_t free_id = ident_t {1002};

    db.PrimeRecord(collection, blocked_id, MakeDoc({{"value", 11}}));
    db.PrimeRecord(collection, free_id, MakeDoc({{"value", 22}}));
    db.BlockRecordRead(blocked_id);

    std::promise<AnyData::Document> blocked_promise;
    auto blocked_future = blocked_promise.get_future();
    std::thread blocked_reader {[&] {
        try {
            blocked_promise.set_value(db.GetDocument(collection, blocked_id));
        }
        catch (...) {
            blocked_promise.set_exception(std::current_exception());
        }
    }};

    db.WaitUntilBlockedReadEntered();

    std::promise<AnyData::Document> free_promise;
    auto free_future = free_promise.get_future();
    std::thread free_reader {[&] {
        try {
            free_promise.set_value(db.GetDocument(collection, free_id));
        }
        catch (...) {
            free_promise.set_exception(std::current_exception());
        }
    }};

    REQUIRE(free_future.wait_for(std::chrono::milliseconds {200}) == std::future_status::ready);
    auto free_doc = free_future.get();
    REQUIRE(!free_doc.Empty());
    CHECK(free_doc["value"].AsInt64() == 22);

    db.UnblockRecordRead();

    auto blocked_doc = blocked_future.get();
    REQUIRE(!blocked_doc.Empty());
    CHECK(blocked_doc["value"].AsInt64() == 11);

    blocked_reader.join();
    free_reader.join();
}

TEST_CASE("DataBaseRestorePendingDeleteIsIdempotent")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"restore-delete"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "delete test_collection 1001\n");

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.InitializeOpLogs();

        REQUIRE_NOTHROW(db.RestorePendingChanges());
        CHECK(db.SnapshotRecord(hashes.ToHashedString("test_collection"), ident_t {1001}).Empty());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestorePendingInsertSkipsEqualDocument")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"restore-insert-same"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");

    hstring collection = hashes.ToHashedString("test_collection");
    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 1}}));
        db.InitializeOpLogs();

        REQUIRE_NOTHROW(db.RestorePendingChanges());

        auto doc = db.SnapshotRecord(collection, ident_t {1001});
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestorePendingInsertDetectsConflict")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"restore-insert-conflict"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");

    hstring collection = hashes.ToHashedString("test_collection");
    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 2}}));
        db.InitializeOpLogs();

        REQUIRE_THROWS_AS(db.RestorePendingChanges(), DataBaseException);
    }
}

TEST_CASE("DataBaseRestorePendingUpdateSkipsAlreadyAppliedPatch")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"restore-update-same"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");

    hstring collection = hashes.ToHashedString("test_collection");
    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 1}, {"other", 7}}));
        db.InitializeOpLogs();

        REQUIRE_NOTHROW(db.RestorePendingChanges());

        auto doc = db.SnapshotRecord(collection, ident_t {1001});
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
        CHECK(doc["other"].AsInt64() == 7);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestorePendingUpdateAppliesPatch")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"restore-update-apply"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":3,\"added\":9}\n");

    hstring collection = hashes.ToHashedString("test_collection");
    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 1}, {"other", 7}}));
        db.InitializeOpLogs();

        REQUIRE_NOTHROW(db.RestorePendingChanges());

        auto doc = db.SnapshotRecord(collection, ident_t {1001});
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 3);
        CHECK(doc["other"].AsInt64() == 7);
        CHECK(doc["added"].AsInt64() == 9);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestorePendingRejectsInvalidUtf8StringKey")
{
    GlobalSettings settings {false};
    ScopedRecoveryLogs recovery_logs {"restore-invalid-utf8-string-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());

    string pending_content = "insert test_string_collection ";
    pending_content.push_back(static_cast<char>(0xC3));
    pending_content += " {\"value\":1}\n";
    WriteRecoveryLogs(recovery_logs, pending_content);

    {
        TestDataBase db {settings};
        REQUIRE_THROWS_AS(db.InitializeOpLogs(), DataBaseException);
    }
}

TEST_CASE("DataBaseInitializeOpLogsRejectsUnknownCommand")
{
    GlobalSettings settings {false};
    ScopedRecoveryLogs recovery_logs {"oplog-unknown-command"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "upsert test_collection 1001 {\"value\":1}\n");

    {
        TestDataBase db {settings};
        REQUIRE_THROWS_AS(db.InitializeOpLogs(), DataBaseException);
    }
}

TEST_CASE("DataBaseInitializeOpLogsRejectsInvalidNumericKey")
{
    GlobalSettings settings {false};
    ScopedRecoveryLogs recovery_logs {"oplog-invalid-numeric-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection invalid-id {\"value\":1}\n");

    {
        TestDataBase db {settings};
        REQUIRE_THROWS_AS(db.InitializeOpLogs(), DataBaseException);
    }
}

TEST_CASE("DataBaseInitializeOpLogsRejectsInvalidEscapedFileStringKey")
{
    GlobalSettings settings {false};
    ScopedRecoveryLogs recovery_logs {"oplog-invalid-file-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_string_collection bad%zz {\"value\":1}\n");

    {
        TestDataBase db {settings, DataBaseStringKeyEscaping::File};
        REQUIRE_THROWS_AS(db.InitializeOpLogs(), DataBaseException);
    }
}

TEST_CASE("DataBaseInitializeOpLogsRejectsInvalidHexStringKey")
{
    GlobalSettings settings {false};
    ScopedRecoveryLogs recovery_logs {"oplog-invalid-hex-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_string_collection s_1 {\"value\":1}\n");

    {
        TestDataBase db {settings, DataBaseStringKeyEscaping::Hex};
        REQUIRE_THROWS_AS(db.InitializeOpLogs(), DataBaseException);
    }
}

TEST_CASE("DataBaseWaitCommitChangesReturnsAfterSpillToOplog")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"stop-after-spill"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection = hashes.ToHashedString("test_collection");

    {
        TestDataBase db {settings};
        db.InitializeOpLogs();
        db.SetBackendWriteFailure();
        db.StartCommitChanges();
        db.Insert(collection, ident_t {1001}, MakeDoc({{"value", 1}}));
        db.WaitUntilCommitOperationWrittenToOpLog();

        mutex fallback_locker;
        std::condition_variable_any fallback_cv;
        bool stop_completed = false;
        std::thread fallback_thread {[&] {
            unique_lock locker {fallback_locker};

            if (!fallback_cv.wait_for(locker, std::chrono::milliseconds {500}, [&] { return stop_completed; })) {
                db.SetBackendWriteFailure(false);
            }
        }};

        auto start = std::chrono::steady_clock::now();
        db.WaitCommitChanges();
        auto elapsed = std::chrono::steady_clock::now() - start;

        {
            scoped_lock locker {fallback_locker};
            stop_completed = true;
        }

        fallback_cv.notify_one();
        fallback_thread.join();

        CHECK(elapsed < std::chrono::milliseconds {250});
    }

    auto pending_content = fs_read_file(recovery_logs.PendingPath());
    REQUIRE(pending_content.has_value());
    CHECK(!pending_content->empty());
}

TEST_CASE("DataBaseSupportsStringKeys")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_string_collection");
    DataBaseKey record_id {string("steam:user-123")};

    db.StartCommitChanges();
    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.WaitCommitChanges();

    auto doc = db.SnapshotRecord(collection, record_id);
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);

    db.Update(collection, record_id, "other", numeric_cast<int64_t>(7));
    db.WaitCommitChanges();

    doc = db.SnapshotRecord(collection, record_id);
    REQUIRE(!doc.Empty());
    CHECK(doc["other"].AsInt64() == 7);

    db.Delete(collection, record_id);
    db.WaitCommitChanges();

    CHECK(db.SnapshotRecord(collection, record_id).Empty());
}

TEST_CASE("DataBaseJsonGetAllStringIdsDecodesStoredKeys")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"json-string-ids"};
    hstring collection = hashes.ToHashedString("test_string_collection");
    auto collection_schemas = DataBaseCollectionSchemas {{collection, DataBaseKeyType::String}};
    string connection_info = strex("JSON {}", fs_path_to_string(*recovery_logs.Dir())).str();
    DataBaseKey record_id {string("steam% user/Привет")};

    auto db = ConnectToDataBase(&settings, connection_info, collection_schemas, {});

    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.StartCommitChanges();
    db.WaitCommitChanges();

    auto all_ids = db.GetAllStringIds(collection);
    REQUIRE(all_ids.size() == 1);
    CHECK(all_ids.front() == std::get<string>(record_id));
    CHECK(db.Valid(collection, record_id));
}

TEST_CASE("DataBaseTypedGetAllIdsRejectCollectionTypeMismatch")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    hstring int_collection = hashes.ToHashedString("test_collection");
    hstring string_collection = hashes.ToHashedString("test_string_collection");
    auto collection_schemas = DataBaseCollectionSchemas {
        {int_collection, DataBaseKeyType::IntId},
        {string_collection, DataBaseKeyType::String},
    };

    auto db = ConnectToDataBase(&settings, "Memory", collection_schemas, {});

    REQUIRE_THROWS_AS(db.GetAllIntIds(string_collection), DataBaseException);
    REQUIRE_THROWS_AS(db.GetAllStringIds(int_collection), DataBaseException);
}

TEST_CASE("DataBaseGetAllIdsRejectsBackendKeyTypeMismatch")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring int_collection = hashes.ToHashedString("test_collection");
    hstring string_collection = hashes.ToHashedString("test_string_collection");

    db.PrimeRecord(int_collection, DataBaseKey {string("wrong-type")}, MakeDoc({{"value", 1}}));
    db.PrimeRecord(string_collection, DataBaseKey {ident_t {1001}}, MakeDoc({{"value", 1}}));

    REQUIRE_THROWS_AS(db.GetAllIds(int_collection), DataBaseException);
    REQUIRE_THROWS_AS(db.GetAllIds(string_collection), DataBaseException);
}

TEST_CASE("DataBaseGetAllIdsRejectsInvalidUtf8BackendStringKey")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings, DataBaseStringKeyEscaping::Raw};
    hstring string_collection = hashes.ToHashedString("test_string_collection");
    string invalid_key = string(1, static_cast<char>(0xC3));

    db.PrimeRecord(string_collection, DataBaseKey {invalid_key}, MakeDoc({{"value", 1}}));

    REQUIRE_THROWS_AS(db.GetAllIds(string_collection), DataBaseException);
}

TEST_CASE("DataBaseJsonGetAllStringIdsRejectsInvalidEscapedKey")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"json-invalid-escaped-string-id"};
    hstring collection = hashes.ToHashedString("test_string_collection");
    auto collection_schemas = DataBaseCollectionSchemas {{collection, DataBaseKeyType::String}};
    string storage_root = fs_path_to_string(*recovery_logs.Dir());
    string collection_dir = strex("{}/{}", storage_root, collection).str();
    string bad_doc_path = strex("{}/bad%zz.json", collection_dir).str();
    string connection_info = strex("JSON {}", storage_root).str();

    REQUIRE(fs_create_directories(collection_dir));
    REQUIRE(fs_write_file(bad_doc_path, "{\"value\":1}"));

    auto db = ConnectToDataBase(&settings, connection_info, collection_schemas, {});

    REQUIRE_THROWS_AS(db.GetAllStringIds(collection), DataBaseException);
}

TEST_CASE("DataBaseRejectsInvalidUtf8StringKeys")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    hstring collection = hashes.ToHashedString("test_string_collection");
    DataBaseKey invalid_record_id {string(1, static_cast<char>(0xC3))};

    REQUIRE_THROWS_AS(db.Insert(collection, invalid_record_id, MakeDoc({{"value", 1}})), DataBaseException);
}

TEST_CASE("DataBaseRelaxedStringKeysKeepRawBackendIds")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings, DataBaseStringKeyEscaping::Raw};
    hstring collection = hashes.ToHashedString("test_string_collection");
    DataBaseKey record_id {string("steam% user\n123")};

    db.StartCommitChanges();
    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.WaitCommitChanges();

    auto stored_ids = db.GetAllRecordIds(collection);
    REQUIRE(stored_ids.size() == 1);
    CHECK(stored_ids.front() == record_id);

    auto doc = db.GetDocument(collection, record_id);
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseFileStringKeysEncodeBackendIds")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings, DataBaseStringKeyEscaping::File};
    hstring collection = hashes.ToHashedString("test_string_collection");
    DataBaseKey record_id {string("steam user/123")};

    db.StartCommitChanges();
    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.WaitCommitChanges();

    auto stored_ids = db.GetAllRecordIds(collection);
    REQUIRE(stored_ids.size() == 1);
    REQUIRE(std::holds_alternative<string>(stored_ids.front()));
    CHECK(std::get<string>(stored_ids.front()) == "steam%20user%2f123");

    auto doc = db.GetDocument(collection, record_id);
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseHexStringKeysEncodeBackendIds")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings, DataBaseStringKeyEscaping::Hex};
    hstring collection = hashes.ToHashedString("test_string_collection");
    DataBaseKey record_id {string("steam:user-123")};

    db.StartCommitChanges();
    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.WaitCommitChanges();

    auto stored_ids = db.GetAllRecordIds(collection);
    REQUIRE(stored_ids.size() == 1);
    REQUIRE(std::holds_alternative<string>(stored_ids.front()));
    CHECK(std::get<string>(stored_ids.front()) == "s_737465616d3a757365722d313233");

    auto doc = db.GetDocument(collection, record_id);
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseReconnectRestoresPendingChangesFromOplog")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"reconnect-restore"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection = hashes.ToHashedString("test_collection");
    ident_t record_id = ident_t {1001};

    {
        TestDataBase db {settings};
        db.InitializeOpLogs();
        db.SetBackendWriteFailure();
        db.StartCommitChanges();
        db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
        db.WaitUntilCommitOperationWrittenToOpLog();

        CHECK(db.SnapshotRecord(collection, record_id).Empty());

        db.SetBackendWriteFailure(false);
        db.WaitUntilPendingChangesRestored();
        db.WaitCommitChanges();

        auto doc = db.GetDocument(collection, record_id);
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReconnectRestoresComplexDocumentFromOplog")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"reconnect-restore-complex-doc"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection = hashes.ToHashedString("test_collection");
    ident_t record_id = ident_t {1001};

    {
        TestDataBase db {settings};
        db.InitializeOpLogs();
        db.SetBackendWriteFailure();
        db.StartCommitChanges();
        db.Insert(collection, record_id, MakeComplexDoc());
        db.WaitUntilCommitOperationWrittenToOpLog();

        CHECK(db.SnapshotRecord(collection, record_id).Empty());

        db.SetBackendWriteFailure(false);
        db.WaitUntilPendingChangesRestored();
        db.WaitCommitChanges();

        CheckComplexDoc(db.GetDocument(collection, record_id));
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReconnectRestoresStringKeyChangesFromOplog")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"reconnect-restore-string-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection = hashes.ToHashedString("test_string_collection");
    DataBaseKey record_id {string("steam:user-123")};

    {
        TestDataBase db {settings};
        db.InitializeOpLogs();
        db.SetBackendWriteFailure();
        db.StartCommitChanges();
        db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
        db.WaitUntilCommitOperationWrittenToOpLog();

        CHECK(db.SnapshotRecord(collection, record_id).Empty());

        db.SetBackendWriteFailure(false);
        db.WaitUntilPendingChangesRestored();
        db.WaitCommitChanges();

        auto doc = db.GetDocument(collection, record_id);
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReconnectRestoresRelaxedStringKeysFromOplog")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"reconnect-restore-relaxed-string-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection = hashes.ToHashedString("test_string_collection");
    DataBaseKey record_id {string("steam% user\n123")};

    {
        TestDataBase db {settings, DataBaseStringKeyEscaping::Raw};
        db.InitializeOpLogs();
        db.SetBackendWriteFailure();
        db.StartCommitChanges();
        db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
        db.WaitUntilCommitOperationWrittenToOpLog();

        CHECK(db.SnapshotRecord(collection, record_id).Empty());

        db.SetBackendWriteFailure(false);
        db.WaitUntilPendingChangesRestored();
        db.WaitCommitChanges();

        auto stored_ids = db.GetAllRecordIds(collection);
        REQUIRE(stored_ids.size() == 1);
        CHECK(stored_ids.front() == record_id);

        auto doc = db.GetDocument(collection, record_id);
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReconnectRestoresFileStringKeysFromOplog")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"reconnect-restore-file-string-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection = hashes.ToHashedString("test_string_collection");
    DataBaseKey record_id {string("steam% user/123")};

    {
        TestDataBase db {settings, DataBaseStringKeyEscaping::File};
        db.InitializeOpLogs();
        db.SetBackendWriteFailure();
        db.StartCommitChanges();
        db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
        db.WaitUntilCommitOperationWrittenToOpLog();

        CHECK(db.SnapshotRecord(collection, record_id).Empty());

        db.SetBackendWriteFailure(false);
        db.WaitUntilPendingChangesRestored();
        db.WaitCommitChanges();

        auto stored_ids = db.GetAllRecordIds(collection);
        REQUIRE(stored_ids.size() == 1);
        REQUIRE(std::holds_alternative<string>(stored_ids.front()));
        CHECK(std::get<string>(stored_ids.front()) == "steam%25%20user%2f123");

        auto doc = db.GetDocument(collection, record_id);
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReconnectRestoresHexStringKeysFromOplog")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"reconnect-restore-hex-string-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection = hashes.ToHashedString("test_string_collection");
    DataBaseKey record_id {string("steam%:user-123")};

    {
        TestDataBase db {settings, DataBaseStringKeyEscaping::Hex};
        db.InitializeOpLogs();
        db.SetBackendWriteFailure();
        db.StartCommitChanges();
        db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
        db.WaitUntilCommitOperationWrittenToOpLog();

        CHECK(db.SnapshotRecord(collection, record_id).Empty());

        db.SetBackendWriteFailure(false);
        db.WaitUntilPendingChangesRestored();
        db.WaitCommitChanges();

        auto stored_ids = db.GetAllRecordIds(collection);
        REQUIRE(stored_ids.size() == 1);
        REQUIRE(std::holds_alternative<string>(stored_ids.front()));
        CHECK(std::get<string>(stored_ids.front()) == "s_737465616d253a757365722d313233");

        auto doc = db.GetDocument(collection, record_id);
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("JsonDataBaseRoundTripsDocumentsAndIds")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs storage_dir_scope {"json-roundtrip"};
    string storage_dir = fs_path_to_string(*storage_dir_scope.Dir() / "storage");
    hstring collection = hashes.ToHashedString("test_collection");
    auto collection_schemas = DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}};
    ident_t first_id = ident_t {1001};
    ident_t second_id = ident_t {1002};
    ident_t complex_id = ident_t {1003};
    *FixedSettingForOverride(settings.JsonIndent) = 2;
    auto db = ConnectToDataBase(&settings, strex("JSON {}", storage_dir).str(), collection_schemas, {});

    db.Insert(collection, first_id, MakeDoc({{"value", 1}, {"other", 7}}));
    db.Insert(collection, second_id, MakeDoc({{"value", 2}}));
    db.Insert(collection, complex_id, MakeComplexDoc());
    db.StartCommitChanges();
    db.WaitCommitChanges();

    auto ids = db.GetAllIntIds(collection);
    std::sort(ids.begin(), ids.end());

    REQUIRE(ids.size() == 3);
    CHECK(ids[0] == first_id);
    CHECK(ids[1] == second_id);
    CHECK(ids[2] == complex_id);

    auto first_doc = db.Get(collection, first_id);
    REQUIRE(!first_doc.Empty());
    CHECK(first_doc["value"].AsInt64() == 1);
    CHECK(first_doc["other"].AsInt64() == 7);
    CheckComplexDoc(db.Get(collection, complex_id));

    auto json_content = fs_read_file(fs_path_to_string(*storage_dir_scope.Dir() / "storage" / "test_collection" / "1001.json"));
    REQUIRE(json_content.has_value());
    CHECK(json_content->find("\n  \"value\"") != string::npos);

    db.Update(collection, first_id, "value", numeric_cast<int64_t>(3));
    db.Delete(collection, second_id);
    db.WaitCommitChanges();

    auto updated_doc = db.Get(collection, first_id);
    REQUIRE(!updated_doc.Empty());
    CHECK(updated_doc["value"].AsInt64() == 3);
    CHECK(updated_doc["other"].AsInt64() == 7);
    CHECK_FALSE(db.Valid(collection, second_id));

    ids = db.GetAllIntIds(collection);
    REQUIRE(ids.size() == 2);
    CHECK(std::ranges::find(ids, first_id) != ids.end());
    CHECK(std::ranges::find(ids, complex_id) != ids.end());
    CHECK_FALSE(fs_exists(fs_path_to_string(*storage_dir_scope.Dir() / "storage" / "test_collection" / "1002.json")));
}

TEST_CASE("JsonDataBaseRejectsBrokenStorageFiles")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs storage_dir_scope {"json-errors"};
    auto storage_dir = *storage_dir_scope.Dir() / "storage";
    auto collection_dir = storage_dir / "test_collection";
    hstring collection = hashes.ToHashedString("test_collection");
    auto collection_schemas = DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}};
    std::filesystem::create_directories(collection_dir);

    auto db = ConnectToDataBase(&settings, strex("JSON {}", fs_path_to_string(storage_dir)).str(), collection_schemas, {});

    REQUIRE(fs_write_file(fs_path_to_string(collection_dir / "0.json"), "{}"));
    REQUIRE_THROWS_AS(db.GetAllIds(collection), DataBaseException);

    REQUIRE(std::filesystem::remove(collection_dir / "0.json"));
    REQUIRE(fs_write_file(fs_path_to_string(collection_dir / "1001.json"), "{"));
    REQUIRE_THROWS_AS(db.Get(collection, ident_t {1001}), DataBaseException);
}

TEST_CASE("MemoryDataBaseRoundTripsDocumentsAndIds")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    hstring collection = hashes.ToHashedString("test_collection");
    auto collection_schemas = DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}};
    ident_t first_id = ident_t {1001};
    ident_t second_id = ident_t {1002};
    auto db = ConnectToDataBase(&settings, "Memory", collection_schemas, {});

    CHECK(db.InValidState());
    CHECK(db.GetAllIntIds(collection).empty());
    CHECK_FALSE(db.Valid(collection, first_id));
    CHECK(db.Get(collection, first_id).Empty());

    db.StartCommitChanges();
    db.Insert(collection, first_id, MakeDoc({{"value", 1}, {"other", 7}}));
    db.Insert(collection, second_id, MakeDoc({{"value", 2}}));
    db.WaitCommitChanges();

    auto ids = db.GetAllIntIds(collection);
    std::sort(ids.begin(), ids.end());

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == first_id);
    CHECK(ids[1] == second_id);

    auto doc = db.Get(collection, first_id);
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
    CHECK(doc["other"].AsInt64() == 7);

    db.Update(collection, first_id, "value", numeric_cast<int64_t>(3));
    db.Delete(collection, second_id);
    db.WaitCommitChanges();

    doc = db.Get(collection, first_id);
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 3);
    CHECK(doc["other"].AsInt64() == 7);
    CHECK_FALSE(db.Valid(collection, second_id));
    CHECK(db.Get(collection, second_id).Empty());

    ids = db.GetAllIntIds(collection);
    REQUIRE(ids.size() == 1);
    CHECK(ids.front() == first_id);
}

TEST_CASE("DataBaseConnectionValidationAndMetrics")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    hstring collection = hashes.ToHashedString("test_collection");
    auto collection_schemas = DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}};
    ident_t record_id = ident_t {1001};

    REQUIRE_THROWS_AS(ConnectToDataBase(&settings, "Unknown", collection_schemas, {}), DataBaseException);
    REQUIRE_THROWS_AS(ConnectToDataBase(&settings, "JSON", collection_schemas, {}), DataBaseException);
    REQUIRE_THROWS_AS(ConnectToDataBase(&settings, "Memory extra", collection_schemas, {}), DataBaseException);

    auto db = ConnectToDataBase(&settings, "Memory", collection_schemas, {});
    db.StartCommitChanges();
    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.WaitCommitChanges();

    auto doc = db.Get(collection, record_id);
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
    CHECK(db.GetDbRequestsPerMinute() >= 1);
}

#if FO_HAVE_UNQLITE
TEST_CASE("UnQLiteDataBaseRoundTripsDocumentsAndIds")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs storage_dir_scope {"unqlite-roundtrip"};
    string storage_dir = fs_path_to_string(*storage_dir_scope.Dir() / "storage");
    hstring collection = hashes.ToHashedString("test_collection");
    auto collection_schemas = DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}};
    ident_t first_id = ident_t {1001};
    ident_t second_id = ident_t {1002};
    auto db = ConnectToDataBase(&settings, strex("DbUnQLite {}", storage_dir).str(), collection_schemas, {});

    CHECK(db.InValidState());
    CHECK(db.GetAllIntIds(collection).empty());
    CHECK_FALSE(db.Valid(collection, first_id));
    CHECK(db.Get(collection, first_id).Empty());

    db.StartCommitChanges();
    db.Insert(collection, first_id, MakeDoc({{"value", 1}, {"other", 7}}));
    db.Insert(collection, second_id, MakeDoc({{"value", 2}}));
    db.WaitCommitChanges();

    auto ids = db.GetAllIntIds(collection);
    std::sort(ids.begin(), ids.end());

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == first_id);
    CHECK(ids[1] == second_id);

    auto doc = db.Get(collection, first_id);
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
    CHECK(doc["other"].AsInt64() == 7);

    db.Update(collection, first_id, "value", numeric_cast<int64_t>(3));
    db.Delete(collection, second_id);
    db.WaitCommitChanges();

    doc = db.Get(collection, first_id);
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 3);
    CHECK(doc["other"].AsInt64() == 7);
    CHECK_FALSE(db.Valid(collection, second_id));
    CHECK(db.Get(collection, second_id).Empty());

    ids = db.GetAllIntIds(collection);
    REQUIRE(ids.size() == 1);
    CHECK(ids.front() == first_id);
    CHECK(fs_exists(fs_path_to_string(*storage_dir_scope.Dir() / "storage" / "test_collection.unqlite")));
}

TEST_CASE("UnQLiteDataBasePersistsDocumentsAcrossReconnects")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs storage_dir_scope {"unqlite-reconnect"};
    string storage_dir = fs_path_to_string(*storage_dir_scope.Dir() / "storage");
    hstring int_collection = hashes.ToHashedString("test_collection");
    hstring string_collection = hashes.ToHashedString("test_string_collection");
    auto collection_schemas = DataBaseCollectionSchemas {
        {int_collection, DataBaseKeyType::IntId},
        {string_collection, DataBaseKeyType::String},
    };
    ident_t int_id = ident_t {1001};
    DataBaseKey string_id {string("steam:user/Привет")};
    string connection_info = strex("DbUnQLite {}", storage_dir).str();

    {
        auto db = ConnectToDataBase(&settings, connection_info, collection_schemas, {});

        db.StartCommitChanges();
        db.Insert(int_collection, int_id, MakeDoc({{"value", 1}}));
        db.Insert(string_collection, string_id, MakeDoc({{"value", 2}}));
        db.WaitCommitChanges();

        db.Update(int_collection, int_id, "patched", numeric_cast<int64_t>(7));
        db.WaitCommitChanges();
    }

    {
        auto db = ConnectToDataBase(&settings, connection_info, collection_schemas, {});

        auto int_ids = db.GetAllIntIds(int_collection);
        REQUIRE(int_ids.size() == 1);
        CHECK(int_ids.front() == int_id);

        auto string_ids = db.GetAllStringIds(string_collection);
        REQUIRE(string_ids.size() == 1);
        CHECK(string_ids.front() == std::get<string>(string_id));

        auto doc = db.Get(int_collection, int_id);
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
        CHECK(doc["patched"].AsInt64() == 7);

        doc = db.Get(string_collection, string_id);
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 2);

        db.StartCommitChanges();
        db.Delete(int_collection, int_id);
        db.WaitCommitChanges();
    }

    {
        auto db = ConnectToDataBase(&settings, connection_info, collection_schemas, {});

        CHECK_FALSE(db.Valid(int_collection, int_id));
        CHECK(db.GetAllIntIds(int_collection).empty());

        auto string_ids = db.GetAllStringIds(string_collection);
        REQUIRE(string_ids.size() == 1);
        CHECK(string_ids.front() == std::get<string>(string_id));
    }
}

TEST_CASE("UnQLiteDataBaseRejectsCorruptedStoredKeys")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs storage_dir_scope {"unqlite-corrupted-keys"};
    string storage_dir = fs_path_to_string(*storage_dir_scope.Dir() / "storage");
    hstring int_collection = hashes.ToHashedString("test_collection");
    hstring string_collection = hashes.ToHashedString("test_string_collection");
    auto collection_schemas = DataBaseCollectionSchemas {
        {int_collection, DataBaseKeyType::IntId},
        {string_collection, DataBaseKeyType::String},
    };

    SECTION("Invalid numeric key size")
    {
        StoreRawUnQLiteRecord(*storage_dir_scope.Dir() / "storage" / "test_collection.unqlite", vector<uint8_t> {1, 2, 3}, "payload");

        auto db = ConnectToDataBase(&settings, strex("DbUnQLite {}", storage_dir).str(), collection_schemas, {});

        REQUIRE_THROWS_AS(db.GetAllIds(int_collection), DataBaseException);
    }

    SECTION("Invalid numeric key value")
    {
        vector<uint8_t> zero_key(sizeof(int64_t));
        StoreRawUnQLiteRecord(*storage_dir_scope.Dir() / "storage" / "test_collection.unqlite", zero_key, "payload");

        auto db = ConnectToDataBase(&settings, strex("DbUnQLite {}", storage_dir).str(), collection_schemas, {});

        REQUIRE_THROWS_AS(db.GetAllIds(int_collection), DataBaseException);
    }

    SECTION("Invalid encoded string key")
    {
        StoreRawUnQLiteRecord(*storage_dir_scope.Dir() / "storage" / "test_string_collection.unqlite", vector<uint8_t> {'b', 'a', 'd'}, "payload");

        auto db = ConnectToDataBase(&settings, strex("DbUnQLite {}", storage_dir).str(), collection_schemas, {});

        REQUIRE_THROWS_AS(db.GetAllStringIds(string_collection), DataBaseException);
    }
}
#endif

FO_END_NAMESPACE
