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

#include <filesystem>

FO_BEGIN_NAMESPACE

namespace
{
    class TestDataBase final : public DataBaseImpl
    {
    public:
        explicit TestDataBase(DataBaseSettings& db_settings) :
            DataBaseImpl(db_settings, nullptr)
        {
            StartCommitThread();
        }

        ~TestDataBase() override { StopCommitThread(); }

        auto GetAllRecordIds(hstring collection_name) const -> vector<ident_t> override
        {
            std::scoped_lock locker {_collectionsLocker};

            if (_collections.count(collection_name) == 0) {
                return {};
            }

            vector<ident_t> ids;

            for (const auto id : _collections.at(collection_name) | std::views::keys) {
                ids.emplace_back(id);
            }

            return ids;
        }

        void PrimeRecord(hstring collection_name, ident_t id, const AnyData::Document& doc)
        {
            std::scoped_lock locker {_collectionsLocker};
            _collections[collection_name][id] = doc.Copy();
        }

        auto SnapshotRecord(hstring collection_name, ident_t id) const -> AnyData::Document
        {
            std::scoped_lock locker {_collectionsLocker};

            if (_collections.count(collection_name) == 0) {
                return {};
            }

            const auto& collection = _collections.at(collection_name);
            const auto it = collection.find(id);
            return it != collection.end() ? it->second.Copy() : AnyData::Document {};
        }

        [[nodiscard]] auto GetRecordReadCount(ident_t id) const -> size_t
        {
            std::scoped_lock locker {_readStatsLocker};
            return _recordReadCount.contains(id) ? _recordReadCount.at(id) : 0;
        }

        void SetOnGetRecord(function<void()> callback)
        {
            std::scoped_lock locker {_callbackLocker};
            _onGetRecord = std::move(callback);
        }

        void SetStrictRecordSemantics(bool enabled = true)
        {
            std::scoped_lock locker {_collectionsLocker};
            _strictRecordSemantics = enabled;
        }

        void SetBackendWriteFailure(bool enabled = true)
        {
            std::scoped_lock locker {_collectionsLocker};
            _failBackendWrites = enabled;
        }

        void WaitUntilCommitOperationWrittenToOpLog()
        {
            std::unique_lock locker {_mirrorLocker};
            _mirrorCv.wait(locker, [this] { return _pendingChangesMirrored; });
        }

        void WaitUntilPendingChangesRestored()
        {
            std::unique_lock locker {_restoreLocker};
            _restoreCv.wait(locker, [this] { return _pendingChangesRestored; });
        }

        void BlockRecordRead(ident_t id)
        {
            std::scoped_lock locker {_blockedReadLocker};
            _blockedReadEnabled = true;
            _blockedReadId = id;
            _blockedReadEntered = false;
        }

        void WaitUntilBlockedReadEntered()
        {
            std::unique_lock locker {_blockedReadLocker};
            _blockedReadCv.wait(locker, [this] { return _blockedReadEntered; });
        }

        void UnblockRecordRead()
        {
            {
                std::scoped_lock locker {_blockedReadLocker};
                _blockedReadEnabled = false;
            }

            _blockedReadCv.notify_all();
        }

    protected:
        auto TryReconnect() -> bool override
        {
            std::scoped_lock locker {_collectionsLocker};
            return !_failBackendWrites;
        }

        void OnCommitOperationWrittenToOpLog() override
        {
            {
                std::scoped_lock locker {_mirrorLocker};
                _pendingChangesMirrored = true;
            }

            _mirrorCv.notify_all();
        }

        void OnPendingChangesRestored() override
        {
            {
                std::scoped_lock locker {_restoreLocker};
                _pendingChangesRestored = true;
            }

            _restoreCv.notify_all();
        }

        auto GetRecord(hstring collection_name, ident_t id) const -> AnyData::Document override
        {
            {
                std::scoped_lock locker {_readStatsLocker};
                _recordReadCount[id]++;
            }

            {
                std::unique_lock locker {_blockedReadLocker};

                if (_blockedReadEnabled && _blockedReadId == id) {
                    _blockedReadEntered = true;
                    _blockedReadCv.notify_all();
                    _blockedReadCv.wait(locker, [this] { return !_blockedReadEnabled; });
                }
            }

            {
                function<void()> callback;
                {
                    std::scoped_lock locker {_callbackLocker};
                    callback = std::move(_onGetRecord);
                    _onGetRecord = {};
                }

                if (callback) {
                    callback();
                }
            }

            std::scoped_lock locker {_collectionsLocker};

            if (_collections.count(collection_name) == 0) {
                return {};
            }

            const auto& collection = _collections.at(collection_name);
            const auto it = collection.find(id);
            return it != collection.end() ? it->second.Copy() : AnyData::Document {};
        }

        void InsertRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) override
        {
            std::scoped_lock locker {_collectionsLocker};

            if (_failBackendWrites) {
                throw DataBaseException("Simulated database write failure");
            }

            auto& collection = _collections[collection_name];

            if (_strictRecordSemantics && collection.contains(id)) {
                throw DataBaseException("Strict test insert duplicate");
            }

            collection.emplace(id, doc.Copy());
        }

        void UpdateRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) override
        {
            std::scoped_lock locker {_collectionsLocker};

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

        void DeleteRecord(hstring collection_name, ident_t id) override
        {
            std::scoped_lock locker {_collectionsLocker};

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
        mutable std::mutex _collectionsLocker {};
        mutable std::mutex _callbackLocker {};
        mutable std::mutex _blockedReadLocker {};
        mutable std::mutex _readStatsLocker {};
        mutable std::mutex _mirrorLocker {};
        mutable std::mutex _restoreLocker {};
        mutable std::condition_variable _blockedReadCv {};
        mutable std::condition_variable _mirrorCv {};
        mutable std::condition_variable _restoreCv {};
        mutable DataBase::Collections _collections {};
        mutable unordered_map<ident_t, size_t> _recordReadCount {};
        mutable function<void()> _onGetRecord {};
        mutable bool _blockedReadEnabled {};
        mutable bool _blockedReadEntered {};
        mutable ident_t _blockedReadId {};
        bool _pendingChangesMirrored {};
        bool _pendingChangesRestored {};
        bool _strictRecordSemantics {};
        bool _failBackendWrites {};
    };

    auto MakeDoc(std::initializer_list<pair<string_view, int64>> values) -> AnyData::Document
    {
        AnyData::Document doc;

        for (const auto& [key, value] : values) {
            doc.Assign(string(key), value);
        }

        return doc;
    }

    class ScopedRecoveryLogs final
    {
    public:
        explicit ScopedRecoveryLogs(string_view test_name)
        {
            const auto unique_suffix = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto dir_name = strex("lf-db-tests-{}-{}", test_name, unique_suffix).str();

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

        [[nodiscard]] auto Dir() const -> const std::filesystem::path& { return _dir; }
        [[nodiscard]] auto PendingPath() const -> const string& { return _pendingPath; }
        [[nodiscard]] auto CommittedPath() const -> const string& { return _committedPath; }

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
        const_cast<bool&>(settings.DataBaseOpLogEnabled) = true;
        const_cast<string&>(settings.DataBaseOpLogPath) = string(oplog_path);
        const_cast<int32&>(settings.DataBaseReconnectRetryPeriod) = 20;
        const_cast<int32&>(settings.DataBasePanicOpLogSizeThreshold) = 1024 * 1024;
        const_cast<int32&>(settings.DataBasePanicShutdownTimeout) = 1;
    }

    void WriteRecoveryLogs(const ScopedRecoveryLogs& recovery_logs, string_view pending_content, string_view committed_content = {})
    {
        REQUIRE(fs_write_file(recovery_logs.PendingPath(), pending_content));
        REQUIRE(fs_write_file(recovery_logs.CommittedPath(), committed_content));
    }

    void CheckRecoveryLogsCleared(const ScopedRecoveryLogs& recovery_logs)
    {
        const auto pending_content = fs_read_file(recovery_logs.PendingPath());
        const auto committed_content = fs_read_file(recovery_logs.CommittedPath());

        REQUIRE(pending_content.has_value());
        REQUIRE(committed_content.has_value());
        CHECK(pending_content->empty());
        CHECK(committed_content->empty());
    }
} // namespace

TEST_CASE("DataBaseCommitOperationsPreserveOrder")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    const auto collection = hashes.ToHashedString("test_collection");
    const auto record_id = ident_t {1001};

    db.Insert(collection, record_id, MakeDoc({{"a", 1}}));
    db.Update(collection, record_id, "b", numeric_cast<int64>(2));
    db.Update(collection, record_id, "a", numeric_cast<int64>(3));

    db.StartCommitChanges();
    db.WaitCommitChanges();

    const auto committed_doc = db.SnapshotRecord(collection, record_id);
    REQUIRE(!committed_doc.Empty());
    CHECK(committed_doc["a"].AsInt64() == 3);
    CHECK(committed_doc["b"].AsInt64() == 2);
}

TEST_CASE("DataBaseGetDocumentAppliesConcurrentPendingChange")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    const auto collection = hashes.ToHashedString("test_collection");
    const auto record_id = ident_t {1001};

    db.PrimeRecord(collection, record_id, MakeDoc({{"value", 1}}));
    db.SetOnGetRecord([&] { db.Update(collection, record_id, "value", numeric_cast<int64>(2)); });

    const auto doc = db.GetDocument(collection, record_id);

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
    const auto collection = hashes.ToHashedString("test_collection");
    const auto record_id = ident_t {1001};

    constexpr size_t worker_count = 4;
    constexpr int64 updates_per_worker = 5;
    unordered_set<int64> expected_values;

    for (size_t worker_index = 0; worker_index < worker_count; worker_index++) {
        for (int64 update_index = 0; update_index < updates_per_worker; update_index++) {
            expected_values.emplace(numeric_cast<int64>(100 + worker_index * 10 + update_index));
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
            for (int64 update_index = 0; update_index < updates_per_worker; update_index++) {
                const auto value = numeric_cast<int64>(100 + worker_index * 10 + update_index);
                db.Update(collection, record_id, "value", value);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    db.UnblockRecordRead();

    const auto doc = doc_future.get();
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
    const auto collection = hashes.ToHashedString("test_collection");
    const auto record_id = ident_t {1001};

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
    db.Update(collection, record_id, "value", numeric_cast<int64>(2));
    db.WaitCommitChanges();
    db.UnblockRecordRead();

    const auto doc = doc_future.get();
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
    const auto collection = hashes.ToHashedString("test_collection");
    const auto target_id = ident_t {1001};
    const auto other_id = ident_t {1002};

    db.PrimeRecord(collection, target_id, MakeDoc({{"value", 1}}));
    db.PrimeRecord(collection, other_id, MakeDoc({{"value", 10}}));
    db.SetOnGetRecord([&] { db.Update(collection, other_id, "value", numeric_cast<int64>(20)); });

    const auto doc = db.GetDocument(collection, target_id);

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
    const auto collection = hashes.ToHashedString("test_collection");
    const auto target_id = ident_t {1001};

    constexpr size_t worker_count = 4;
    constexpr size_t records_per_worker = 6;

    db.PrimeRecord(collection, target_id, MakeDoc({{"value", 1}}));

    for (size_t worker_index = 0; worker_index < worker_count; worker_index++) {
        for (size_t record_index = 0; record_index < records_per_worker; record_index++) {
            const auto other_id = ident_t {numeric_cast<int64>(2000 + worker_index * 100 + record_index)};
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
                const auto other_id = ident_t {numeric_cast<int64>(2000 + worker_index * 100 + record_index)};

                for (int64 update_index = 0; update_index < 5; update_index++) {
                    db.Update(collection, other_id, "value", numeric_cast<int64>(1000 + worker_index * 100 + record_index * 10 + update_index));
                }
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    db.UnblockRecordRead();

    const auto target_doc = target_future.get();
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
    const auto collection = hashes.ToHashedString("test_collection");

    constexpr size_t thread_count = 4;
    constexpr size_t records_per_thread = 8;

    db.StartCommitChanges();

    vector<std::thread> workers;
    workers.reserve(thread_count);

    for (size_t thread_index = 0; thread_index < thread_count; thread_index++) {
        workers.emplace_back([&, thread_index] {
            for (size_t record_index = 0; record_index < records_per_thread; record_index++) {
                const auto id = ident_t {numeric_cast<int64>(thread_index * 100 + record_index + 1)};
                db.Insert(collection, id, MakeDoc({{"thread", numeric_cast<int64>(thread_index)}}));
                db.Update(collection, id, "record", numeric_cast<int64>(record_index));
                db.Update(collection, id, "value", numeric_cast<int64>(thread_index * 1000 + record_index));
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    db.WaitCommitChanges();

    for (size_t thread_index = 0; thread_index < thread_count; thread_index++) {
        for (size_t record_index = 0; record_index < records_per_thread; record_index++) {
            const auto id = ident_t {numeric_cast<int64>(thread_index * 100 + record_index + 1)};
            const auto doc = db.SnapshotRecord(collection, id);

            REQUIRE(!doc.Empty());
            CHECK(doc["thread"].AsInt64() == numeric_cast<int64>(thread_index));
            CHECK(doc["record"].AsInt64() == numeric_cast<int64>(record_index));
            CHECK(doc["value"].AsInt64() == numeric_cast<int64>(thread_index * 1000 + record_index));
        }
    }
}

TEST_CASE("DataBaseSlowReadDoesNotBlockOtherReads")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    TestDataBase db {settings};
    const auto collection = hashes.ToHashedString("test_collection");
    const auto blocked_id = ident_t {1001};
    const auto free_id = ident_t {1002};

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
    const auto free_doc = free_future.get();
    REQUIRE(!free_doc.Empty());
    CHECK(free_doc["value"].AsInt64() == 22);

    db.UnblockRecordRead();

    const auto blocked_doc = blocked_future.get();
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
    ScopedCurrentPath current_path {recovery_logs.Dir()};
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
    ScopedCurrentPath current_path {recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");

    const auto collection = hashes.ToHashedString("test_collection");
    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 1}}));
        db.InitializeOpLogs();

        REQUIRE_NOTHROW(db.RestorePendingChanges());

        const auto doc = db.SnapshotRecord(collection, ident_t {1001});
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
    ScopedCurrentPath current_path {recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");

    const auto collection = hashes.ToHashedString("test_collection");
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
    ScopedCurrentPath current_path {recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");

    const auto collection = hashes.ToHashedString("test_collection");
    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 1}, {"other", 7}}));
        db.InitializeOpLogs();

        REQUIRE_NOTHROW(db.RestorePendingChanges());

        const auto doc = db.SnapshotRecord(collection, ident_t {1001});
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
        CHECK(doc["other"].AsInt64() == 7);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseWaitCommitChangesReturnsAfterSpillToOplog")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"stop-after-spill"};
    ScopedCurrentPath current_path {recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    const auto collection = hashes.ToHashedString("test_collection");

    {
        TestDataBase db {settings};
        db.InitializeOpLogs();
        db.SetBackendWriteFailure();
        db.StartCommitChanges();
        db.Insert(collection, ident_t {1001}, MakeDoc({{"value", 1}}));
        db.WaitUntilCommitOperationWrittenToOpLog();

        std::mutex fallback_locker;
        std::condition_variable fallback_cv;
        bool stop_completed = false;
        std::thread fallback_thread {[&] {
            std::unique_lock locker {fallback_locker};

            if (!fallback_cv.wait_for(locker, std::chrono::milliseconds {500}, [&] { return stop_completed; })) {
                db.SetBackendWriteFailure(false);
            }
        }};

        const auto start = std::chrono::steady_clock::now();
        db.WaitCommitChanges();
        const auto elapsed = std::chrono::steady_clock::now() - start;

        {
            std::scoped_lock locker {fallback_locker};
            stop_completed = true;
        }

        fallback_cv.notify_one();
        fallback_thread.join();

        CHECK(elapsed < std::chrono::milliseconds {250});
    }

    const auto pending_content = fs_read_file(recovery_logs.PendingPath());
    REQUIRE(pending_content.has_value());
    CHECK(!pending_content->empty());
}

TEST_CASE("DataBaseReconnectRestoresPendingChangesFromOplog")
{
    GlobalSettings settings {false};
    HashStorage hashes;
    ScopedRecoveryLogs recovery_logs {"reconnect-restore"};
    ScopedCurrentPath current_path {recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    const auto collection = hashes.ToHashedString("test_collection");
    const auto record_id = ident_t {1001};

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

        const auto doc = db.SnapshotRecord(collection, record_id);
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

FO_END_NAMESPACE
