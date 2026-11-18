// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "DataBase-Mongo.h"
#include "DataBase-SQLite.h"
#include "DataBase.h"
#include "DataBaseSerialization.h"
#include "DiskFileSystem.h"

#if FO_DATABASE_SQLITE
FO_DISABLE_WARNINGS_PUSH()
#include "sqlite3.h"
FO_DISABLE_WARNINGS_POP()
#endif

#include <filesystem>

FO_BEGIN_NAMESPACE

namespace
{
    template<typename T>
    auto FixedSettingForOverride(const T& setting) noexcept -> ptr<T>
    {
        return const_cast<T*>(&setting);
    }

    class TestDataBase final : public DataBaseImpl
    {
    public:
        explicit TestDataBase(DataBaseSettings& db_settings, DataBaseStringKeyEscaping string_key_escaping = DataBaseStringKeyEscaping::Raw, DataBasePanicCallback panic_callback = nullptr, bool start_thread = true) :
            DataBaseImpl(SettingsPtr(db_settings), std::move(panic_callback)),
            _stringKeyEscaping {string_key_escaping}
        {
            // _hashes (member) must outlive _collectionKeyTypes/_collectionNames in the base class,
            // because the hstrings registered below hold pointers into _hashes._hashStorage
            InitializeCollections({
                {_hashes.to_hashed_string("test_collection"), DataBaseKeyType::IntId},
                {_hashes.to_hashed_string("test_string_collection"), DataBaseKeyType::String},
            });
            if (start_thread) {
                StartInactiveCommitThread();
            }
        }

        ~TestDataBase() override
        {
            if (_threadStarted) {
                StopCommitThread();
            }
        }

        void StartInactiveCommitThread()
        {
            FO_VERIFY_AND_THROW(!_threadStarted, "Test database commit thread is already started");
            StartCommitThread();
            _threadStarted = true;
        }

        [[nodiscard]] auto GetStringKeyEscaping() const noexcept -> DataBaseStringKeyEscaping override { return _stringKeyEscaping; }

        [[nodiscard]] auto GetRecordReadCount(ident_t id) const -> size_t
        {
            scoped_lock locker {_readStatsLocker};
            return _recordReadCount.contains(id) ? _recordReadCount.at(id) : 0;
        }

        auto GetAllIds(hstring collection_name) const -> vector<DataBaseKey>
        {
            auto key_type {GetCollectionKeyType(collection_name)};
            auto ids {GetAllRecordIds(collection_name)};

            for (auto& id : ids) {
                if (GetDbKeyType(id) != key_type) {
                    throw DataBaseException("Database collection returned invalid key type", collection_name, id);
                }

                if (key_type == DataBaseKeyType::String) {
                    const auto& value {std::get<string>(id)};

                    if (!strvex(value).is_valid_utf8()) {
                        throw DataBaseException("Invalid database string key utf8", value);
                    }
                }
            }

            return ids;
        }

        auto GetAllRecordIds(hstring collection_name) const -> vector<DataBaseKey> override
        {
            vector<DataBaseKey> ids {};

            {
                scoped_lock locker {_collectionsLocker};

                if (_collections.count(collection_name) != 0) {
                    for (const auto& id : _collections.at(collection_name) | std::views::keys) {
                        ids.emplace_back(id);
                    }
                }
            }

            function<void()> callback {};

            {
                scoped_lock locker {_callbackLocker};

                callback = std::move(_onGetAllRecordIds);
                _onGetAllRecordIds = {};
            }

            if (callback) {
                callback();
            }

            return ids;
        }

        void PrimeRecord(hstring collection_name, ident_t id, const AnyData::Document& doc) { PrimeRecord(collection_name, DataBaseKey {id}, doc); }

        void PrimeRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc)
        {
            scoped_lock locker {_collectionsLocker};

            AdvanceStorageRevision();
            _collections[collection_name][id] = doc.Copy();
        }

        auto SnapshotRecord(hstring collection_name, ident_t id) const -> AnyData::Document { return SnapshotRecord(collection_name, DataBaseKey {id}); }

        auto SnapshotRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document
        {
            scoped_lock locker {_collectionsLocker};

            if (_collections.count(collection_name) == 0) {
                return {};
            }

            const auto& collection {_collections.at(collection_name)};
            auto it {collection.find(id)};
            return it != collection.end() ? it->second.Copy() : AnyData::Document {};
        }

        void SetOnGetRecord(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};

            _onGetRecord = std::move(callback);
        }

        void SetOnPendingRestored(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};

            _onPendingRestored = std::move(callback);
        }

        void SetOnCommitMirrored(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};

            _onCommitMirrored = std::move(callback);
        }

        void SetOnPendingLogTruncated(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};

            _onPendingLogTruncated = std::move(callback);
        }

        void SetOnRecoveryLogsSnapshotted(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};
            _onRecoveryLogsSnapshotted = std::move(callback);
        }

        void SetOnReplayBeforeCommittedSnapshot(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};
            _onReplayBeforeCommittedSnapshot = std::move(callback);
        }

        void SetOnReplayBeforeCleanupSnapshot(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};
            _onReplayBeforeCleanupSnapshot = std::move(callback);
        }

        void SetOnReplayBeforePendingRefresh(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};
            _onReplayBeforePendingRefresh = std::move(callback);
        }

        void SetOnReplayReadValidated(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};
            _onReplayReadValidated = std::move(callback);
        }

        void SetOnReplayWriteValidated(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};
            _onReplayWriteValidated = std::move(callback);
        }

        void SetOnBackendUpdateApplied(int64_t value, function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};

            _backendUpdateHookValue = value;
            _onBackendUpdateApplied = std::move(callback);
        }

        // Runs once, after a key listing has been taken and before it is returned
        void SetOnGetAllRecordIds(function<void()> callback)
        {
            scoped_lock locker {_callbackLocker};

            _onGetAllRecordIds = std::move(callback);
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

        void FailAfterNextBackendMutation() { FailAfterBackendMutationNumber(1); }

        void FailAfterBackendMutationNumber(size_t number)
        {
            scoped_lock locker {_collectionsLocker};

            FO_VERIFY_AND_THROW(number > 0, "Failure injection mutation number must be positive");
            _mutationsBeforeFailure = number;
        }

        void SetReconnectFailure(bool enabled = true)
        {
            scoped_lock locker {_collectionsLocker};
            _failReconnect = enabled;
        }

        void WaitUntilCommitOperationWrittenToOpLog()
        {
            unique_lock locker {_mirrorLocker};

            _mirrorCv.wait(locker, [this]() FO_TSA_REQUIRES(_mirrorLocker) { return _pendingChangesMirrored; });
        }

        auto HasCommitOperationWrittenToOpLog() const -> bool
        {
            scoped_lock locker {_mirrorLocker};
            return _pendingChangesMirrored;
        }

        auto GetCommitOperationsWrittenToOpLog() const -> size_t
        {
            scoped_lock locker {_mirrorLocker};
            return _commitOperationsMirrored;
        }

        void BlockPendingChangesRestored()
        {
            scoped_lock locker {_restoreLocker};
            _restoreBlocked = true;
            _pendingChangesRestored = false;
        }

        void UnblockPendingChangesRestored()
        {
            {
                scoped_lock locker {_restoreLocker};
                _restoreBlocked = false;
            }

            _restoreCv.notify_all();
        }

        void WaitUntilPendingChangesRestored()
        {
            unique_lock locker {_restoreLocker};

            _restoreCv.wait(locker, [this]() FO_TSA_REQUIRES(_restoreLocker) { return _pendingChangesRestored; });
        }

        auto WaitUntilPendingChangesRestored(std::chrono::nanoseconds timeout) -> bool
        {
            unique_lock locker {_restoreLocker};
            return _restoreCv.wait_for(locker, timeout, [this]() FO_TSA_REQUIRES(_restoreLocker) { return _pendingChangesRestored; });
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

        auto WaitUntilBlockedReadEntered(std::chrono::nanoseconds timeout) -> bool
        {
            unique_lock locker {_blockedReadLocker};
            return _blockedReadCv.wait_for(locker, timeout, [this]() FO_TSA_REQUIRES(_blockedReadLocker) { return _blockedReadEntered; });
        }

        void UnblockRecordRead()
        {
            {
                scoped_lock locker {_blockedReadLocker};

                _blockedReadEnabled = false;
                _blockedReadAfterCopy = false;
            }

            _blockedReadCv.notify_all();
        }

        // Holds a read after it has fetched the record, so the backend can change between the fetch and its check
        void BlockRecordReadAfterCopy(ident_t id, bool first_only = false)
        {
            scoped_lock locker {_blockedReadLocker};

            _blockedReadEnabled = true;
            _blockedReadAfterCopy = true;
            _blockedReadFirstOnly = first_only;
            _blockedReadClaimed = false;
            _blockedReadId = id;
            _blockedReadEntered = false;
        }

        void BlockRecordWrite(ident_t id, bool first_only = false)
        {
            scoped_lock locker {_blockedWriteLocker};

            _blockedWriteEnabled = true;
            _blockedWriteId = id;
            _blockedWriteFirstOnly = first_only;
            _blockedWriteEntered = false;
            _blockedWriteCount = 0;
        }

        void WaitUntilBlockedWriteEntered()
        {
            unique_lock locker {_blockedWriteLocker};

            _blockedWriteCv.wait(locker, [this]() FO_TSA_REQUIRES(_blockedWriteLocker) { return _blockedWriteEntered; });
        }

        auto WaitUntilBlockedWriteEntered(std::chrono::nanoseconds timeout) -> bool
        {
            unique_lock locker {_blockedWriteLocker};
            return _blockedWriteCv.wait_for(locker, timeout, [this]() FO_TSA_REQUIRES(_blockedWriteLocker) { return _blockedWriteEntered; });
        }

        auto WaitUntilBlockedWritesEntered(size_t count, std::chrono::nanoseconds timeout) -> bool
        {
            unique_lock locker {_blockedWriteLocker};
            return _blockedWriteCv.wait_for(locker, timeout, [this, count]() FO_TSA_REQUIRES(_blockedWriteLocker) { return _blockedWriteCount >= count; });
        }

        void UnblockRecordWrite()
        {
            {
                scoped_lock locker {_blockedWriteLocker};

                _blockedWriteEnabled = false;
            }

            _blockedWriteCv.notify_all();
        }

        void BlockSnapshot()
        {
            scoped_lock locker {_snapshotTestLocker};

            _snapshotBlocked = true;
            _snapshotEntered = false;
        }

        void WaitUntilSnapshotEntered()
        {
            unique_lock locker {_snapshotTestLocker};

            _snapshotTestCv.wait(locker, [this]() FO_TSA_REQUIRES(_snapshotTestLocker) { return _snapshotEntered; });
        }

        void UnblockSnapshot()
        {
            {
                scoped_lock locker {_snapshotTestLocker};

                _snapshotBlocked = false;
            }

            _snapshotTestCv.notify_all();
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
            return !_failBackendWrites && !_failReconnect;
        }

        void OnReplayReadValidated() override
        {
            function<void()> callback {};
            {
                scoped_lock locker {_callbackLocker};
                callback = std::move(_onReplayReadValidated);
                _onReplayReadValidated = {};
            }

            if (callback) {
                callback();
            }
        }

        void OnReplayWriteValidated() override
        {
            function<void()> callback {};
            {
                scoped_lock locker {_callbackLocker};
                callback = std::move(_onReplayWriteValidated);
                _onReplayWriteValidated = {};
            }

            if (callback) {
                callback();
            }
        }

        void OnReplayBeforeCommittedSnapshot() override
        {
            function<void()> callback {};
            {
                scoped_lock locker {_callbackLocker};
                callback = std::move(_onReplayBeforeCommittedSnapshot);
                _onReplayBeforeCommittedSnapshot = {};
            }

            if (callback) {
                callback();
            }
        }

        void OnReplayBeforePendingRefresh() override
        {
            function<void()> callback {};
            {
                scoped_lock locker {_callbackLocker};
                callback = std::move(_onReplayBeforePendingRefresh);
                _onReplayBeforePendingRefresh = {};
            }

            if (callback) {
                callback();
            }
        }

        void OnReplayBeforeCleanupSnapshot() override
        {
            function<void()> callback {};
            {
                scoped_lock locker {_callbackLocker};
                callback = std::move(_onReplayBeforeCleanupSnapshot);
                _onReplayBeforeCleanupSnapshot = {};
            }

            if (callback) {
                callback();
            }
        }

        void OnRecoveryLogsSnapshotted() override
        {
            function<void()> callback {};
            {
                scoped_lock locker {_callbackLocker};
                callback = std::move(_onRecoveryLogsSnapshotted);
                _onRecoveryLogsSnapshotted = {};
            }

            if (callback) {
                callback();
            }
        }

        void OnPendingChangesLogTruncated() override
        {
            function<void()> callback {};

            {
                scoped_lock locker {_callbackLocker};

                callback = std::move(_onPendingLogTruncated);
                _onPendingLogTruncated = {};
            }

            if (callback) {
                callback();
            }
        }

        void OnCommitOperationWrittenToOpLog() override
        {
            {
                scoped_lock locker {_mirrorLocker};

                _pendingChangesMirrored = true;
                _commitOperationsMirrored++;
            }

            _mirrorCv.notify_all();

            function<void()> callback {};

            {
                scoped_lock locker {_callbackLocker};

                callback = std::move(_onCommitMirrored);
                _onCommitMirrored = {};
            }

            if (callback) {
                callback();
            }
        }

        void OnPendingChangesRestored() override
        {
            {
                unique_lock locker {_restoreLocker};
                _pendingChangesRestored = true;
                _restoreCv.notify_all();

                _restoreCv.wait(locker, [this]() FO_TSA_REQUIRES(_restoreLocker) { return !_restoreBlocked; });
            }

            function<void()> callback {};

            {
                scoped_lock locker {_callbackLocker};
                callback = std::move(_onPendingRestored);
                _onPendingRestored = {};
            }

            if (callback) {
                callback();
            }
        }

        auto GetRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document override
        {
            auto snapshot {ReadReplayRecord(collection_name, id)};
            return std::move(snapshot.Record);
        }

        auto ReadReplayRecord(hstring collection_name, const DataBaseKey& id) const -> ReplayRecordSnapshot override
        {
            {
                scoped_lock locker {_readStatsLocker};

                _recordReadCount[id]++;
            }

            bool block_after_copy {false};

            {
                unique_lock locker {_blockedReadLocker};

                if (_blockedReadEnabled && _blockedReadId == id) {
                    if (_blockedReadAfterCopy && (!_blockedReadFirstOnly || !_blockedReadClaimed)) {
                        block_after_copy = true;
                        _blockedReadClaimed = true;
                    }
                    else if (!_blockedReadAfterCopy) {
                        _blockedReadEntered = true;
                        _blockedReadCv.notify_all();
                        _blockedReadCv.wait(locker, [this]() FO_TSA_REQUIRES(_blockedReadLocker) { return !_blockedReadEnabled; });
                    }
                }
            }

            {
                function<void()> callback {};
                {
                    scoped_lock locker {_callbackLocker};

                    callback = std::move(_onGetRecord);
                    _onGetRecord = {};
                }

                if (callback) {
                    callback();
                }
            }

            ReplayRecordSnapshot snapshot {CopyStoredRecordForReplay(collection_name, id)};

            if (block_after_copy) {
                unique_lock locker {_blockedReadLocker};

                _blockedReadEntered = true;
                _blockedReadCv.notify_all();
                _blockedReadCv.wait(locker, [this]() FO_TSA_REQUIRES(_blockedReadLocker) { return !_blockedReadEnabled; });
            }

            return snapshot;
        }

        auto ApplyReplayMutation(hstring collection_name, const DataBaseKey& id, uint64_t expected_revision, ReplayMutation mutation, const AnyData::Document& doc) -> bool override
        {
            if (mutation != ReplayMutation::None) {
                BlockRecordWriteIfRequested(id);
            }

            {
                scoped_lock locker {_collectionsLocker};

                if (_storageRevision != expected_revision) {
                    return false;
                }

                if (mutation == ReplayMutation::None) {
                    return true;
                }

                if (_failBackendWrites) {
                    throw DataBaseException("Simulated database write failure");
                }

                auto& collection {_collections[collection_name]};

                if (mutation == ReplayMutation::Insert) {
                    if (_strictRecordSemantics && collection.contains(id)) {
                        throw DataBaseException("Strict test insert duplicate");
                    }
                    AdvanceStorageRevision();
                    collection.emplace(id, doc.Copy());
                }
                else if (mutation == ReplayMutation::Update) {
                    if (_strictRecordSemantics && !collection.contains(id)) {
                        throw DataBaseException("Strict test update missing record");
                    }
                    AdvanceStorageRevision();
                    auto& target_doc {collection[id]};

                    for (const auto& [doc_key, doc_value] : doc) {
                        target_doc.Assign(doc_key, doc_value.Copy());
                    }
                }
                else {
                    FO_VERIFY_AND_THROW(mutation == ReplayMutation::Delete, "Unknown simulated database replay mutation");
                    if (_strictRecordSemantics && !collection.contains(id)) {
                        throw DataBaseException("Strict test delete missing record");
                    }
                    AdvanceStorageRevision();
                    collection.erase(id);
                }

                FailAfterMutationIfRequested();
            }

            if (mutation == ReplayMutation::Update) {
                RunBackendUpdateHook(doc);
            }

            return true;
        }

        void InsertRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
        {
            BlockRecordWriteIfRequested(id);
            scoped_lock locker {_collectionsLocker};

            if (_failBackendWrites) {
                throw DataBaseException("Simulated database write failure");
            }

            auto& collection {_collections[collection_name]};

            if (_strictRecordSemantics && collection.contains(id)) {
                throw DataBaseException("Strict test insert duplicate");
            }

            AdvanceStorageRevision();
            collection.emplace(id, doc.Copy());

            FailAfterMutationIfRequested();
        }

        void UpdateRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) override
        {
            BlockRecordWriteIfRequested(id);

            {
                scoped_lock locker {_collectionsLocker};

                if (_failBackendWrites) {
                    throw DataBaseException("Simulated database write failure");
                }

                auto& collection {_collections[collection_name]};

                if (_strictRecordSemantics && !collection.contains(id)) {
                    throw DataBaseException("Strict test update missing record");
                }

                AdvanceStorageRevision();
                auto& target_doc {collection[id]};

                for (auto&& [doc_key, doc_value] : doc) {
                    target_doc.Assign(doc_key, doc_value.Copy());
                }

                FailAfterMutationIfRequested();
            }

            RunBackendUpdateHook(doc);
        }

        void RunBackendUpdateHook(const AnyData::Document& doc)
        {
            function<void()> callback {};

            {
                scoped_lock locker {_callbackLocker};

                if (_onBackendUpdateApplied && doc.Contains("value") && doc["value"].AsInt64() == _backendUpdateHookValue) {
                    callback = std::move(_onBackendUpdateApplied);
                    _onBackendUpdateApplied = {};
                }
            }

            if (callback) {
                callback();
            }
        }

        void DeleteRecord(hstring collection_name, const DataBaseKey& id) override
        {
            BlockRecordWriteIfRequested(id);
            scoped_lock locker {_collectionsLocker};

            if (_failBackendWrites) {
                throw DataBaseException("Simulated database write failure");
            }

            auto& collection {_collections[collection_name]};

            if (_strictRecordSemantics && !collection.contains(id)) {
                throw DataBaseException("Strict test delete missing record");
            }

            AdvanceStorageRevision();

            if (_collections.count(collection_name) != 0) {
                collection.erase(id);
            }

            FailAfterMutationIfRequested();
        }

        auto CreateSnapshotData() -> vector<uint8_t> override
        {
            {
                unique_lock locker {_snapshotTestLocker};

                _snapshotEntered = true;
                _snapshotTestCv.notify_all();
                _snapshotTestCv.wait(locker, [this]() FO_TSA_REQUIRES(_snapshotTestLocker) { return !_snapshotBlocked; });
            }

            return vector<uint8_t> {1, 2, 3, 4};
        }

    private:
        static auto SettingsPtr(DataBaseSettings& settings) noexcept -> ptr<DataBaseSettings> { return &settings; }

        void FailAfterMutationIfRequested() FO_TSA_REQUIRES(_collectionsLocker)
        {
            if (_mutationsBeforeFailure != 0 && --_mutationsBeforeFailure == 0) {
                throw DataBaseException("Simulated failure after database mutation");
            }
        }

        void BlockRecordWriteIfRequested(const DataBaseKey& id)
        {
            unique_lock locker {_blockedWriteLocker};

            if (_blockedWriteEnabled && _blockedWriteId == id && (!_blockedWriteFirstOnly || _blockedWriteCount == 0)) {
                _blockedWriteEntered = true;
                _blockedWriteCount++;
                _blockedWriteCv.notify_all();
                _blockedWriteCv.wait(locker, [this]() FO_TSA_REQUIRES(_blockedWriteLocker) { return !_blockedWriteEnabled; });
            }
        }

        void AdvanceStorageRevision() FO_TSA_REQUIRES(_collectionsLocker)
        {
            FO_STRONG_ASSERT(_storageRevision != std::numeric_limits<uint64_t>::max(), "Simulated database storage revision overflow");
            _storageRevision++;
        }

        auto CopyStoredRecordForReplay(hstring collection_name, const DataBaseKey& id) const -> ReplayRecordSnapshot
        {
            scoped_lock locker {_collectionsLocker};

            if (_collections.count(collection_name) == 0) {
                return {{}, _storageRevision};
            }

            const auto& collection {_collections.at(collection_name)};
            auto it {collection.find(id)};
            AnyData::Document record {it != collection.end() ? it->second.Copy() : AnyData::Document {}};
            return {std::move(record), _storageRevision};
        }

        hash_storage _hashes {};
        DataBaseStringKeyEscaping _stringKeyEscaping {};
        bool _threadStarted {};
        mutable mutex _collectionsLocker {};
        uint64_t _storageRevision FO_TSA_GUARDED_BY(_collectionsLocker) {};
        mutable mutex _callbackLocker {};
        mutable mutex _blockedReadLocker {};
        mutable mutex _blockedWriteLocker {};
        mutable mutex _readStatsLocker {};
        mutable mutex _mirrorLocker {};
        mutable mutex _restoreLocker {};
        mutable mutex _snapshotTestLocker {};
        mutable std::condition_variable_any _blockedReadCv {};
        mutable std::condition_variable_any _blockedWriteCv {};
        mutable std::condition_variable_any _mirrorCv {};
        mutable std::condition_variable_any _restoreCv {};
        mutable std::condition_variable_any _snapshotTestCv {};
        mutable DataBase::Collections _collections FO_TSA_GUARDED_BY(_collectionsLocker) {};
        mutable unordered_map<DataBaseKey, size_t> _recordReadCount FO_TSA_GUARDED_BY(_readStatsLocker) {};
        mutable function<void()> _onGetRecord FO_TSA_GUARDED_BY(_callbackLocker) {};
        mutable function<void()> _onGetAllRecordIds FO_TSA_GUARDED_BY(_callbackLocker) {};
        function<void()> _onPendingRestored FO_TSA_GUARDED_BY(_callbackLocker) {};
        function<void()> _onCommitMirrored FO_TSA_GUARDED_BY(_callbackLocker) {};
        function<void()> _onPendingLogTruncated FO_TSA_GUARDED_BY(_callbackLocker) {};
        function<void()> _onRecoveryLogsSnapshotted FO_TSA_GUARDED_BY(_callbackLocker) {};
        function<void()> _onReplayBeforeCommittedSnapshot FO_TSA_GUARDED_BY(_callbackLocker) {};
        function<void()> _onReplayBeforePendingRefresh FO_TSA_GUARDED_BY(_callbackLocker) {};
        function<void()> _onReplayBeforeCleanupSnapshot FO_TSA_GUARDED_BY(_callbackLocker) {};
        function<void()> _onReplayReadValidated FO_TSA_GUARDED_BY(_callbackLocker) {};
        function<void()> _onReplayWriteValidated FO_TSA_GUARDED_BY(_callbackLocker) {};
        function<void()> _onBackendUpdateApplied FO_TSA_GUARDED_BY(_callbackLocker) {};
        int64_t _backendUpdateHookValue FO_TSA_GUARDED_BY(_callbackLocker) {};
        mutable bool _blockedReadEnabled FO_TSA_GUARDED_BY(_blockedReadLocker) {};
        mutable bool _blockedReadEntered FO_TSA_GUARDED_BY(_blockedReadLocker) {};
        mutable DataBaseKey _blockedReadId FO_TSA_GUARDED_BY(_blockedReadLocker) {ident_t {}};
        mutable bool _blockedReadAfterCopy FO_TSA_GUARDED_BY(_blockedReadLocker) {};
        mutable bool _blockedReadFirstOnly FO_TSA_GUARDED_BY(_blockedReadLocker) {};
        mutable bool _blockedReadClaimed FO_TSA_GUARDED_BY(_blockedReadLocker) {};
        bool _blockedWriteEnabled FO_TSA_GUARDED_BY(_blockedWriteLocker) {};
        bool _blockedWriteEntered FO_TSA_GUARDED_BY(_blockedWriteLocker) {};
        bool _blockedWriteFirstOnly FO_TSA_GUARDED_BY(_blockedWriteLocker) {};
        size_t _blockedWriteCount FO_TSA_GUARDED_BY(_blockedWriteLocker) {};
        DataBaseKey _blockedWriteId FO_TSA_GUARDED_BY(_blockedWriteLocker) {ident_t {}};
        bool _pendingChangesMirrored FO_TSA_GUARDED_BY(_mirrorLocker) {};
        size_t _commitOperationsMirrored FO_TSA_GUARDED_BY(_mirrorLocker) {};
        bool _pendingChangesRestored FO_TSA_GUARDED_BY(_restoreLocker) {};
        bool _restoreBlocked FO_TSA_GUARDED_BY(_restoreLocker) {};
        bool _strictRecordSemantics FO_TSA_GUARDED_BY(_collectionsLocker) {};
        bool _failBackendWrites FO_TSA_GUARDED_BY(_collectionsLocker) {};
        size_t _mutationsBeforeFailure FO_TSA_GUARDED_BY(_collectionsLocker) {};
        bool _failReconnect FO_TSA_GUARDED_BY(_collectionsLocker) {};
        bool _snapshotBlocked FO_TSA_GUARDED_BY(_snapshotTestLocker) {};
        bool _snapshotEntered FO_TSA_GUARDED_BY(_snapshotTestLocker) {};
    };

    auto MakeDoc(std::initializer_list<pair<string_view, int64_t>> values) -> AnyData::Document
    {
        AnyData::Document doc {};

        for (const auto& [key, value] : values) {
            doc.Assign(string(key), value);
        }

        return doc;
    }

    auto MakeComplexDoc() -> AnyData::Document
    {
        AnyData::Array values {};
        values.EmplaceBack(numeric_cast<int64_t>(7));
        values.EmplaceBack(2.5);
        values.EmplaceBack(false);
        values.EmplaceBack(string {"array"});

        AnyData::Dict nested {};
        nested.Emplace("flag", true);
        nested.Emplace("label", string {"nested"});
        nested.Emplace("score", numeric_cast<int64_t>(9));

        AnyData::Document doc {};
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

        const auto& values {doc["array"].AsArray()};
        REQUIRE(values.Size() == 4);
        CHECK(values[0].AsInt64() == 7);
        CHECK(values[1].AsDouble() == 2.5);
        CHECK_FALSE(values[2].AsBool());
        CHECK(values[3].AsString() == "array");

        const auto& nested {doc["dict"].AsDict()};
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
            auto unique_suffix {std::chrono::steady_clock::now().time_since_epoch().count()};
            string dir_name {strex("lf-db-tests-{}-{}", test_name, unique_suffix).str()};

            _dir = std::filesystem::temp_directory_path() / std::filesystem::path {fs::make_path(dir_name)};
            std::filesystem::create_directories(_dir);
            _pendingPath = fs::path_to_string(_dir / "DbPendingChanges.oplog");
            _committedPath = fs::path_to_string(_dir / "DbPendingChanges-committed.oplog");
        }

        ~ScopedRecoveryLogs()
        {
            std::error_code ec {};
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
            std::error_code ec {};
            std::filesystem::current_path(_prevPath, ec);
        }

    private:
        std::filesystem::path _prevPath {};
    };

    void ConfigureRecoverySettings(GlobalSettings& settings, string_view oplog_path)
    {
        *FixedSettingForOverride(settings.DataBase.OpLogEnabled) = true;
        *FixedSettingForOverride(settings.DataBase.OpLogPath) = string(oplog_path);
        *FixedSettingForOverride(settings.DataBase.ReconnectRetryPeriod) = 20;
        *FixedSettingForOverride(settings.DataBase.PanicOpLogSizeThreshold) = 1024 * 1024;
        *FixedSettingForOverride(settings.DataBase.PanicShutdownTimeout) = 1;
    }

    void WriteRecoveryLogs(const ScopedRecoveryLogs& recovery_logs, string_view pending_content, string_view committed_content = {})
    {
        REQUIRE(fs::write_file(recovery_logs.PendingPath(), pending_content));
        REQUIRE(fs::write_file(recovery_logs.CommittedPath(), committed_content));
    }

    void CheckRecoveryLogsCleared(const ScopedRecoveryLogs& recovery_logs)
    {
        auto pending_content {fs::read_file(recovery_logs.PendingPath())};
        auto committed_content {fs::read_file(recovery_logs.CommittedPath())};

        REQUIRE(pending_content.has_value());
        REQUIRE(committed_content.has_value());
        CHECK(pending_content->empty());
        CHECK(committed_content->empty());
    }

    // A backend answers a batch read as it answers the same ids one at a time: in request order, empty for a
    // missing record, a copy for a repeated id, and all of it counted as one request
    void CheckBatchReadMatchesSingleReads(DataBase& db, hstring int_collection, hstring string_collection, size_t record_count)
    {
        vector<DataBaseKey> int_ids {};

        for (size_t i {0}; i < record_count; i++) {
            DataBaseKey id {ident_t {numeric_cast<int64_t>(1001 + i)}};
            db.Insert(int_collection, id, MakeDoc({{"value", numeric_cast<int64_t>(i)}}));
            int_ids.emplace_back(id);
        }

        // Keys the file and hex encodings rewrite must still come back under the id they were requested by
        vector<DataBaseKey> string_ids {string("plain"), string("steam:user-123"), string("steam% user/Привет")};

        for (size_t i {0}; i < string_ids.size(); i++) {
            db.Insert(string_collection, string_ids[i], MakeDoc({{"value", numeric_cast<int64_t>(i)}}));
        }

        db.WaitCommitChanges();

        vector<DataBaseKey> requested_int_ids {vector<DataBaseKey>(int_ids.rbegin(), int_ids.rend())};
        requested_int_ids.insert(requested_int_ids.begin() + 1, DataBaseKey {ident_t {999999}});
        requested_int_ids.emplace_back(int_ids.front());

        size_t requests_before {db.GetDbRequestsPerMinute()};
        auto int_docs {db.GetMany(int_collection, requested_int_ids)};
        size_t requests_after {db.GetDbRequestsPerMinute()};

        CHECK(requests_after - requests_before == 1);
        REQUIRE(int_docs.size() == requested_int_ids.size());
        CHECK(int_docs[1].Empty());
        REQUIRE_FALSE(int_docs.back().Empty());
        CHECK(int_docs.back()["value"].AsInt64() == 0);

        for (size_t i {0}; i < requested_int_ids.size(); i++) {
            auto single_doc {db.Get(int_collection, requested_int_ids[i])};
            REQUIRE(int_docs[i].Empty() == single_doc.Empty());

            if (!single_doc.Empty()) {
                CHECK(int_docs[i]["value"].AsInt64() == single_doc["value"].AsInt64());
            }
        }

        vector<DataBaseKey> requested_string_ids {string_ids[2], DataBaseKey {string("missing")}, string_ids[0], string_ids[1]};
        auto string_docs {db.GetMany(string_collection, requested_string_ids)};

        REQUIRE(string_docs.size() == 4);
        REQUIRE_FALSE(string_docs[0].Empty());
        CHECK(string_docs[0]["value"].AsInt64() == 2);
        CHECK(string_docs[1].Empty());
        REQUIRE_FALSE(string_docs[2].Empty());
        CHECK(string_docs[2]["value"].AsInt64() == 0);
        REQUIRE_FALSE(string_docs[3].Empty());
        CHECK(string_docs[3]["value"].AsInt64() == 1);
    }

#if FO_DATABASE_SQLITE
    // Writes straight into the storage file, bypassing the backend, so the key-validation paths can be
    // exercised against data the backend would never have produced itself
    void StoreRawSQLiteRecord(const std::filesystem::path& storage_dir, string_view collection_name, const vector<uint8_t>& key_data, string_view value)
    {
        // sqlite3_config only applies before initialization, so initializing directly here would silently lose the
        // engine allocator for the backend created later
        DataBaseSQLite::InitializeRuntime();

        REQUIRE(fs::create_directories(fs::path_to_string(storage_dir)));

        string db_path {fs::path_to_string(storage_dir / "Storage.sqlite")};
        sqlite3* db {nullptr};
        REQUIRE(sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) == SQLITE_OK);

        auto close_db {scope_exit([&]() noexcept { (void)sqlite3_close(db); })};

        string create_sql {strex(R"(CREATE TABLE IF NOT EXISTS "{}" (key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL) WITHOUT ROWID)", collection_name).str()};
        REQUIRE(sqlite3_exec(db, create_sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK);

        string insert_sql {strex(R"(INSERT OR REPLACE INTO "{}" (key, value) VALUES (?, ?))", collection_name).str()};
        sqlite3_stmt* stmt {nullptr};
        REQUIRE(sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK);

        auto finalize_stmt {scope_exit([&]() noexcept { (void)sqlite3_finalize(stmt); })};

        REQUIRE(sqlite3_bind_blob(stmt, 1, key_data.data(), numeric_cast<int32_t>(key_data.size()), SQLITE_TRANSIENT) == SQLITE_OK);
        REQUIRE(sqlite3_bind_blob(stmt, 2, value.data(), numeric_cast<int32_t>(value.size()), SQLITE_TRANSIENT) == SQLITE_OK);
        REQUIRE(sqlite3_step(stmt) == SQLITE_DONE);
    }
#endif
} // namespace

TEST_CASE("DataBaseCommitOperationsPreserveOrder")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t record_id {ident_t {1001}};

    db.Insert(collection, record_id, MakeDoc({{"a", 1}}));
    db.Update(collection, record_id, "b", numeric_cast<int64_t>(2));
    db.Update(collection, record_id, "a", numeric_cast<int64_t>(3));

    db.StartCommitChanges();
    db.WaitCommitChanges();

    auto committed_doc {db.SnapshotRecord(collection, record_id)};
    REQUIRE(!committed_doc.Empty());
    CHECK(committed_doc["a"].AsInt64() == 3);
    CHECK(committed_doc["b"].AsInt64() == 2);
}

TEST_CASE("DataBaseSnapshotDrainsAndBlocksNewProducers")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t record_id {ident_t {1001}};

    db.StartCommitChanges();
    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.BlockSnapshot();

    std::exception_ptr snapshot_error {};
    thread snapshot_thread {run_thread("SnapshotDrain", [&] {
        try {
            (void)db.CreateSnapshot();
        }
        catch (...) {
            snapshot_error = std::current_exception();
        }
    })};

    db.WaitUntilSnapshotEntered();

    auto drained_doc {db.SnapshotRecord(collection, record_id)};
    CHECK_FALSE(drained_doc.Empty());

    if (!drained_doc.Empty()) {
        CHECK(drained_doc["value"].AsInt64() == 1);
    }

    std::atomic_bool producer_started {false};
    std::atomic_bool producer_finished {false};
    std::exception_ptr producer_error {};
    thread producer_thread {run_thread("BlockedProducer", [&] {
        producer_started = true;

        try {
            db.Update(collection, record_id, "value", numeric_cast<int64_t>(2));
        }
        catch (...) {
            producer_error = std::current_exception();
        }

        producer_finished = true;
    })};

    while (!producer_started.load()) {
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds {20});
    CHECK_FALSE(producer_finished.load());

    db.UnblockSnapshot();
    snapshot_thread.join();
    producer_thread.join();

    CHECK_FALSE(snapshot_error);
    CHECK_FALSE(producer_error);
    CHECK(producer_finished.load());

    db.WaitCommitChanges();
    auto updated_doc {db.SnapshotRecord(collection, record_id)};
    REQUIRE(!updated_doc.Empty());
    CHECK(updated_doc["value"].AsInt64() == 2);
}

TEST_CASE("DataBaseRejectsNonFiniteFloatUpdates")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t record_id {ident_t {1001}};

    db.PrimeRecord(collection, record_id, MakeDoc({{"value", 1}}));

    CHECK_THROWS_AS(db.Update(collection, record_id, "value", std::numeric_limits<float64_t>::quiet_NaN()), DataBaseException);
    CHECK_THROWS_AS(db.Update(collection, record_id, "value", std::numeric_limits<float64_t>::infinity()), DataBaseException);

    auto doc {db.SnapshotRecord(collection, record_id)};
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseRejectsNonFiniteFloatDocuments")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};

    AnyData::Document plain_doc {};
    plain_doc.Assign("value", std::numeric_limits<float64_t>::infinity());
    CHECK_THROWS_AS(db.Insert(collection, ident_t {1001}, plain_doc), DataBaseException);

    AnyData::Array values {};
    values.EmplaceBack(numeric_cast<int64_t>(1));
    values.EmplaceBack(std::numeric_limits<float64_t>::quiet_NaN());

    AnyData::Document array_doc {};
    array_doc.Assign("values", AnyData::Value {std::move(values)});
    CHECK_THROWS_AS(db.Insert(collection, ident_t {1002}, array_doc), DataBaseException);

    AnyData::Dict nested {};
    nested.Emplace("value", std::numeric_limits<float64_t>::infinity());
    CHECK_THROWS_AS(db.Update(collection, ident_t {1003}, "nested", AnyData::Value {std::move(nested)}), DataBaseException);
}

TEST_CASE("DataBaseGetDocumentAppliesConcurrentPendingChange")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t record_id {ident_t {1001}};

    db.PrimeRecord(collection, record_id, MakeDoc({{"value", 1}}));
    db.SetOnGetRecord([&] { db.Update(collection, record_id, "value", numeric_cast<int64_t>(2)); });

    auto doc {db.GetDocument(collection, record_id)};

    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 2);
    CHECK(db.GetRecordReadCount(record_id) == 1);

    db.ClearChanges();
}

TEST_CASE("DataBaseGetDocumentAppliesSameRecordChangesUnderLoad")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t record_id {ident_t {1001}};

    constexpr size_t worker_count {4};
    constexpr int64_t updates_per_worker {5};
    unordered_set<int64_t> expected_values {};

    for (size_t worker_index {0}; worker_index < worker_count; worker_index++) {
        for (int64_t update_index {0}; update_index < updates_per_worker; update_index++) {
            expected_values.emplace(numeric_cast<int64_t>(100 + worker_index * 10 + update_index));
        }
    }

    db.PrimeRecord(collection, record_id, MakeDoc({{"value", 1}}));
    db.BlockRecordRead(record_id);

    std::promise<AnyData::Document> doc_promise {};
    auto doc_future {doc_promise.get_future()};
    std::thread reader {[&] {
        try {
            doc_promise.set_value(db.GetDocument(collection, record_id));
        }
        catch (...) {
            doc_promise.set_exception(std::current_exception());
        }
    }};

    db.WaitUntilBlockedReadEntered();

    vector<std::thread> workers {};
    workers.reserve(worker_count);

    for (size_t worker_index {0}; worker_index < worker_count; worker_index++) {
        workers.emplace_back([&, worker_index] {
            for (int64_t update_index {0}; update_index < updates_per_worker; update_index++) {
                int64_t value {numeric_cast<int64_t>(100 + worker_index * 10 + update_index)};
                db.Update(collection, record_id, "value", value);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    db.UnblockRecordRead();

    auto doc {doc_future.get()};
    reader.join();

    REQUIRE(!doc.Empty());
    CHECK(expected_values.contains(doc["value"].AsInt64()));
    CHECK(db.GetRecordReadCount(record_id) == 1);

    db.ClearChanges();
}

TEST_CASE("DataBaseGetDocumentAppliesCommittedChangeCompletedDuringRead")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t record_id {ident_t {1001}};

    db.PrimeRecord(collection, record_id, MakeDoc({{"value", 1}}));
    db.StartCommitChanges();
    db.BlockRecordRead(record_id);

    std::promise<AnyData::Document> doc_promise {};
    auto doc_future {doc_promise.get_future()};
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

    auto doc {doc_future.get()};
    reader.join();

    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 2);
    CHECK(db.GetRecordReadCount(record_id) >= 2);
}

TEST_CASE("DataBaseGetDocumentRereadsChangeCommittedAfterItsFetch")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t record_id {ident_t {1001}};

    // The commit thread has taken the change before the read starts and writes it after the read fetched the record
    db.PrimeRecord(collection, record_id, MakeDoc({{"value", 1}}));
    db.BlockRecordWrite(record_id);
    db.Update(collection, record_id, "value", numeric_cast<int64_t>(2));
    db.StartCommitChanges();
    db.WaitUntilBlockedWriteEntered();
    db.BlockRecordReadAfterCopy(record_id);

    std::promise<AnyData::Document> doc_promise {};
    auto doc_future {doc_promise.get_future()};
    std::thread reader {[&] {
        try {
            doc_promise.set_value(db.GetDocument(collection, record_id));
        }
        catch (...) {
            doc_promise.set_exception(std::current_exception());
        }
    }};

    db.WaitUntilBlockedReadEntered();
    db.UnblockRecordWrite();
    db.WaitCommitChanges();
    db.UnblockRecordRead();

    auto doc {doc_future.get()};
    reader.join();

    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 2);
    CHECK(db.GetRecordReadCount(record_id) == 2);
}

TEST_CASE("DataBaseGetDocumentIgnoresOtherRecordChanges")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t target_id {ident_t {1001}};
    ident_t other_id {ident_t {1002}};

    db.PrimeRecord(collection, target_id, MakeDoc({{"value", 1}}));
    db.PrimeRecord(collection, other_id, MakeDoc({{"value", 10}}));
    db.SetOnGetRecord([&] { db.Update(collection, other_id, "value", numeric_cast<int64_t>(20)); });

    auto doc {db.GetDocument(collection, target_id)};

    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
    CHECK(db.GetRecordReadCount(target_id) == 1);

    db.ClearChanges();
}

TEST_CASE("DataBaseGetDocumentIgnoresOtherRecordChangesUnderLoad")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t target_id {ident_t {1001}};

    constexpr size_t worker_count {4};
    constexpr size_t records_per_worker {6};

    db.PrimeRecord(collection, target_id, MakeDoc({{"value", 1}}));

    for (size_t worker_index {0}; worker_index < worker_count; worker_index++) {
        for (size_t record_index {0}; record_index < records_per_worker; record_index++) {
            ident_t other_id {ident_t {numeric_cast<int64_t>(2000 + worker_index * 100 + record_index)}};
            db.PrimeRecord(collection, other_id, MakeDoc({{"value", 10}}));
        }
    }

    db.BlockRecordRead(target_id);

    std::promise<AnyData::Document> target_promise {};
    auto target_future {target_promise.get_future()};
    std::thread target_reader {[&] {
        try {
            target_promise.set_value(db.GetDocument(collection, target_id));
        }
        catch (...) {
            target_promise.set_exception(std::current_exception());
        }
    }};

    db.WaitUntilBlockedReadEntered();

    vector<std::thread> workers {};
    workers.reserve(worker_count);

    for (size_t worker_index {0}; worker_index < worker_count; worker_index++) {
        workers.emplace_back([&, worker_index] {
            for (size_t record_index {0}; record_index < records_per_worker; record_index++) {
                ident_t other_id {ident_t {numeric_cast<int64_t>(2000 + worker_index * 100 + record_index)}};

                for (int64_t update_index {0}; update_index < 5; update_index++) {
                    db.Update(collection, other_id, "value", numeric_cast<int64_t>(1000 + worker_index * 100 + record_index * 10 + update_index));
                }
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    db.UnblockRecordRead();

    auto target_doc {target_future.get()};
    target_reader.join();

    REQUIRE(!target_doc.Empty());
    CHECK(target_doc["value"].AsInt64() == 1);
    CHECK(db.GetRecordReadCount(target_id) == 1);

    db.ClearChanges();
}

TEST_CASE("DataBaseGetDocumentsKeepsRequestOrderAndReadsEachRecordOnce")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};

    db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 1}}));
    db.PrimeRecord(collection, ident_t {1002}, MakeDoc({{"value", 2}}));

    auto docs {db.GetDocuments(collection, {ident_t {1002}, ident_t {1999}, ident_t {1001}, ident_t {1002}})};

    REQUIRE(docs.size() == 4);
    CHECK(docs[0]["value"].AsInt64() == 2);
    CHECK(docs[1].Empty());
    CHECK(docs[2]["value"].AsInt64() == 1);
    CHECK(docs[3]["value"].AsInt64() == 2);
    CHECK(db.GetRecordReadCount(ident_t {1001}) == 1);
    CHECK(db.GetRecordReadCount(ident_t {1002}) == 1);
    CHECK(db.GetDocuments(collection, {}).empty());
}

TEST_CASE("DataBaseGetDocumentsAppliesPendingChangesPerRecord")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};

    db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 1}}));
    db.PrimeRecord(collection, ident_t {1002}, MakeDoc({{"value", 2}}));
    db.Update(collection, ident_t {1001}, "value", numeric_cast<int64_t>(10));
    db.Delete(collection, ident_t {1002});
    db.Insert(collection, ident_t {1003}, MakeDoc({{"value", 3}}));

    auto docs {db.GetDocuments(collection, {ident_t {1001}, ident_t {1002}, ident_t {1003}})};

    REQUIRE(docs.size() == 3);
    CHECK(docs[0]["value"].AsInt64() == 10);
    CHECK(docs[1].Empty());
    REQUIRE(!docs[2].Empty());
    CHECK(docs[2]["value"].AsInt64() == 3);

    db.ClearChanges();
}

TEST_CASE("DataBaseGetDocumentsRereadsOnlyRecordsCommittedDuringRead")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t committed_id {ident_t {1001}};
    ident_t blocked_id {ident_t {1002}};

    db.PrimeRecord(collection, committed_id, MakeDoc({{"value", 1}}));
    db.PrimeRecord(collection, blocked_id, MakeDoc({{"value", 2}}));
    db.StartCommitChanges();
    db.BlockRecordRead(blocked_id);

    std::promise<vector<AnyData::Document>> docs_promise {};
    auto docs_future {docs_promise.get_future()};
    std::thread reader {[&] {
        try {
            docs_promise.set_value(db.GetDocuments(collection, {committed_id, blocked_id}));
        }
        catch (...) {
            docs_promise.set_exception(std::current_exception());
        }
    }};

    // The first record is already read when the batch stalls on the second, so its commit lands mid-batch
    db.WaitUntilBlockedReadEntered();
    db.Update(collection, committed_id, "value", numeric_cast<int64_t>(10));
    db.WaitCommitChanges();
    db.UnblockRecordRead();

    auto docs {docs_future.get()};
    reader.join();

    REQUIRE(docs.size() == 2);
    CHECK(docs[0]["value"].AsInt64() == 10);
    CHECK(docs[1]["value"].AsInt64() == 2);
    CHECK(db.GetRecordReadCount(committed_id) >= 2);
    CHECK(db.GetRecordReadCount(blocked_id) == 1);
}

TEST_CASE("DataBaseConcurrentProducersCommitAllRecords")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};

    constexpr size_t thread_count {4};
    constexpr size_t records_per_thread {8};

    db.StartCommitChanges();

    vector<std::thread> workers {};
    workers.reserve(thread_count);

    for (size_t thread_index {0}; thread_index < thread_count; thread_index++) {
        workers.emplace_back([&, thread_index] {
            for (size_t record_index {0}; record_index < records_per_thread; record_index++) {
                ident_t id {ident_t {numeric_cast<int64_t>(thread_index * 100 + record_index + 1)}};
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

    for (size_t thread_index {0}; thread_index < thread_count; thread_index++) {
        for (size_t record_index {0}; record_index < records_per_thread; record_index++) {
            ident_t id {ident_t {numeric_cast<int64_t>(thread_index * 100 + record_index + 1)}};
            auto doc {db.SnapshotRecord(collection, id)};

            REQUIRE(!doc.Empty());
            CHECK(doc["thread"].AsInt64() == numeric_cast<int64_t>(thread_index));
            CHECK(doc["record"].AsInt64() == numeric_cast<int64_t>(record_index));
            CHECK(doc["value"].AsInt64() == numeric_cast<int64_t>(thread_index * 1000 + record_index));
        }
    }
}

TEST_CASE("DataBaseTryInsertAtomicallyChoosesOneConcurrentWriter")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t record_id {ident_t {1001}};

    constexpr size_t writer_count {16};
    std::atomic_size_t ready_count {};
    std::atomic_bool start {false};
    std::atomic_size_t inserted_count {};
    vector<std::exception_ptr> errors {vector<std::exception_ptr>(writer_count)};
    vector<std::thread> writers {};
    writers.reserve(writer_count);
    auto join_writers {scope_exit([&]() noexcept {
        start = true;

        for (auto& writer : writers) {
            if (writer.joinable()) {
                writer.join();
            }
        }
    })};

    for (size_t writer_index {0}; writer_index < writer_count; writer_index++) {
        writers.emplace_back([&, writer_index] {
            ready_count.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            try {
                if (db.TryInsert(collection, record_id, MakeDoc({{"writer", numeric_cast<int64_t>(writer_index)}}))) {
                    inserted_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
            catch (const std::exception&) {
                errors[writer_index] = std::current_exception();
            }
        });
    }

    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (ready_count.load(std::memory_order_acquire) != writer_count && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    bool all_ready {ready_count.load(std::memory_order_acquire) == writer_count};
    start.store(true, std::memory_order_release);

    for (auto& writer : writers) {
        writer.join();
    }

    CHECK(all_ready);

    for (const auto& error : errors) {
        CHECK_FALSE(error);
    }

    CHECK(inserted_count.load(std::memory_order_relaxed) == 1);

    auto pending_doc {db.GetDocument(collection, record_id)};
    REQUIRE_FALSE(pending_doc.Empty());
    CHECK(pending_doc.Contains("writer"));

    db.StartCommitChanges();
    db.WaitCommitChanges();

    auto committed_doc {db.SnapshotRecord(collection, record_id)};
    REQUIRE_FALSE(committed_doc.Empty());
    CHECK(committed_doc["writer"].AsInt64() == pending_doc["writer"].AsInt64());
}

TEST_CASE("DataBaseCompareExchangeAtomicallyAdvancesOneConcurrentWriter")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t record_id {ident_t {1001}};

    AnyData::Document initial_doc {};
    initial_doc.Emplace("state", string {"prepared"});
    db.Insert(collection, record_id, initial_doc);

    constexpr size_t writer_count {16};
    std::atomic_size_t ready_count {};
    std::atomic_bool start {false};
    std::atomic_size_t exchanged_count {};
    vector<std::exception_ptr> errors {vector<std::exception_ptr>(writer_count)};
    vector<std::thread> writers {};
    writers.reserve(writer_count);
    auto join_writers {scope_exit([&]() noexcept {
        start = true;

        for (auto& writer : writers) {
            if (writer.joinable()) {
                writer.join();
            }
        }
    })};

    for (size_t writer_index {0}; writer_index < writer_count; writer_index++) {
        writers.emplace_back([&, writer_index] {
            ready_count.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            string desired {strex("player-bound-{}", writer_index).str()};

            try {
                if (db.CompareExchange(collection, record_id, "state", AnyData::Value {string {"prepared"}}, AnyData::Value {std::move(desired)})) {
                    exchanged_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
            catch (const std::exception&) {
                errors[writer_index] = std::current_exception();
            }
        });
    }

    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (ready_count.load(std::memory_order_acquire) != writer_count && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    bool all_ready {ready_count.load(std::memory_order_acquire) == writer_count};
    start.store(true, std::memory_order_release);

    for (auto& writer : writers) {
        writer.join();
    }

    CHECK(all_ready);

    for (const auto& error : errors) {
        CHECK_FALSE(error);
    }

    CHECK(exchanged_count.load(std::memory_order_relaxed) == 1);

    auto pending_doc {db.GetDocument(collection, record_id)};
    REQUIRE_FALSE(pending_doc.Empty());
    CHECK(pending_doc["state"].AsString().starts_with("player-bound-"));
}

TEST_CASE("DataBaseConditionalReadCannotBorrowAnotherReadersCommitMarker")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    db.PrimeRecord(collection, id, MakeDoc({{"value", 1}}));
    db.Update(collection, id, "value", int64_t {2});
    db.BlockRecordReadAfterCopy(id);

    std::atomic_bool conditional_finished {};
    std::atomic_bool reader_entered {};
    std::atomic_bool release_reader {};
    bool exchanged {false};
    std::exception_ptr conditional_error {};
    std::exception_ptr reader_error {};
    AnyData::Document observed {};
    thread reader {};
    thread conditional {run_thread("ConditionalReadBeforeCommit", [&] {
        try {
            exchanged = db.CompareExchange(collection, id, "value", int64_t {1}, int64_t {3});
        }
        catch (const std::exception&) {
            conditional_error = std::current_exception();
        }

        conditional_finished = true;
    })};

    auto cleanup {scope_exit([&]() noexcept {
        release_reader = true;
        db.UnblockRecordRead();

        if (conditional.joinable()) {
            conditional.join();
        }
        if (reader.joinable()) {
            reader.join();
        }
    })};

    REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));
    db.StartCommitChanges();
    db.WaitCommitChanges();
    db.SetOnGetRecord([&] {
        reader_entered = true;

        while (!release_reader.load()) {
            std::this_thread::yield();
        }
    });

    reader = run_thread("AnotherReadAfterCommit", [&] {
        try {
            observed = db.GetDocument(collection, id);
        }
        catch (const std::exception&) {
            reader_error = std::current_exception();
        }
    });

    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (!reader_entered.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(reader_entered.load());
    db.UnblockRecordRead();
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};

    while (!conditional_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(conditional_finished.load());
    conditional.join();
    CHECK_FALSE(conditional_error);
    CHECK_FALSE(exchanged);
    release_reader = true;
    reader.join();
    CHECK_FALSE(reader_error);
    REQUIRE_FALSE(observed.Empty());
    CHECK(observed["value"].AsInt64() == 2);
    db.WaitCommitChanges();
    CHECK(db.SnapshotRecord(collection, id)["value"].AsInt64() == 2);
}

TEST_CASE("DataBaseConditionalMutationsFollowPendingStateAndTypedValues")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    hstring collection {hashes.to_hashed_string("test_strings")};
    auto db {ConnectToDataBase(&settings, "Memory", {{collection, DataBaseKeyType::String}}, {})};
    DataBaseKey id {string {"key with spaces"}};
    AnyData::Document first {};
    first.Emplace("state", string {"first"});
    AnyData::Document second {};
    second.Emplace("state", string {"second"});

    CHECK_FALSE(db.CompareExchange(collection, id, "state", string {"first"}, string {"wrong"}));
    REQUIRE(db.TryInsert(collection, id, first));
    CHECK_FALSE(db.TryInsert(collection, id, second));
    CHECK_FALSE(db.CompareExchange(collection, id, "missing", string {}, string {"wrong"}));
    CHECK_FALSE(db.CompareExchange(collection, id, "state", int64_t {1}, string {"wrong"}));
    REQUIRE(db.CompareExchange(collection, id, "state", string {"first"}, string {"advanced"}));
    CHECK_FALSE(db.CompareExchange(collection, id, "state", string {"first"}, string {"wrong"}));
    CHECK(db.Get(collection, id)["state"].AsString() == "advanced");

    db.Update(collection, id, "state", string {"ordinary"});
    REQUIRE(db.CompareExchange(collection, id, "state", string {"ordinary"}, string {"updated"}));
    db.Delete(collection, id);
    CHECK_FALSE(db.CompareExchange(collection, id, "state", string {"updated"}, string {"wrong"}));
    REQUIRE(db.TryInsert(collection, id, second));
    CHECK(db.Get(collection, id)["state"].AsString() == "second");
    db.StartCommitChanges();
    db.WaitCommitChanges();
    CHECK(db.Get(collection, id)["state"].AsString() == "second");
    CHECK_FALSE(db.TryInsert(collection, id, first));
    REQUIRE(db.CompareExchange(collection, id, "state", string {"second"}, string {"committed"}));
    db.WaitCommitChanges();
    CHECK(db.Get(collection, id)["state"].AsString() == "committed");
    CHECK_THROWS_AS(db.TryInsert(collection, id, {}), DataBaseException);
    CHECK_THROWS_AS(db.CompareExchange(collection, id, "state", string {"committed"}, std::numeric_limits<float64_t>::infinity()), DataBaseException);
    CHECK(db.Get(collection, id)["state"].AsString() == "committed");
}

TEST_CASE("DataBaseConditionalReadSerializesRestoreAndQueueClear")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    db.PrimeRecord(collection, id, MakeDoc({{"value", 1}}));
    std::atomic_bool reading {};
    std::atomic_bool release {};
    std::atomic_bool other_started {};
    std::atomic_bool other_finished {};
    std::exception_ptr producer_error {};
    std::exception_ptr other_error {};
    bool exchanged {false};
    bool restore {GENERATE(false, true)};
    db.SetOnGetRecord([&] {
        reading = true;

        while (!release.load()) {
            std::this_thread::yield();
        }
    });

    thread producer {run_thread("ConditionalRead", [&] {
        try {
            exchanged = db.CompareExchange(collection, id, "value", int64_t {1}, int64_t {2});
        }
        catch (const std::exception&) {
            producer_error = std::current_exception();
        }
    })};

    auto release_producer {scope_exit([&]() noexcept {
        release = true;
        producer.join();
    })};

    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (!reading.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(reading.load());
    thread other {run_thread("ConcurrentRestoreOrClear", [&] {
        other_started = true;

        try {
            if (restore) {
                db.RestoreSnapshot(vector<uint8_t> {1});
            }
            else {
                db.ClearChanges();
            }
        }
        catch (const std::exception&) {
            other_error = std::current_exception();
        }

        other_finished = true;
    })};

    auto release_other {scope_exit([&]() noexcept {
        release = true;
        other.join();
    })};

    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};

    while (!other_started.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(other_started.load());
    coarse_sleep(std::chrono::milliseconds {20});
    CHECK_FALSE(other_finished.load());
    release = true;
    producer.join();
    release_producer.release();
    other.join();
    release_other.release();
    CHECK_FALSE(producer_error);
    CHECK(exchanged);
    CHECK(bool {other_error} == restore);
    CHECK(db.GetDocument(collection, id)["value"].AsInt64() == (restore ? 2 : 1));
}

TEST_CASE("DataBaseClearChangesPreservesLaterWritesDuringCommit")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t in_flight_id {1001};
    ident_t later_id {1002};
    ident_t discarded_id {1003};
    bool queue_after_clear {GENERATE(false, true)};
    int32_t operation {GENERATE(0, 1, 2)};

    if (operation != 0) {
        db.PrimeRecord(collection, in_flight_id, MakeDoc({{"value", 1}}));
    }

    db.PrimeRecord(collection, later_id, MakeDoc({{"value", 1}}));
    db.PrimeRecord(collection, discarded_id, MakeDoc({{"value", 1}}));
    db.BlockRecordWrite(in_flight_id);
    auto unblock {scope_exit([&db]() noexcept { db.UnblockRecordWrite(); })};

    if (operation == 0) {
        db.Insert(collection, in_flight_id, MakeDoc({{"value", 2}}));
    }
    else if (operation == 1) {
        db.Update(collection, in_flight_id, "value", int64_t {2});
    }
    else {
        db.Delete(collection, in_flight_id);
    }

    db.StartCommitChanges();
    REQUIRE(db.WaitUntilBlockedWriteEntered(std::chrono::seconds {5}));
    db.Update(collection, discarded_id, "value", int64_t {2});

    std::atomic_bool cleared {false};
    thread cleaner {run_thread("ClearPendingWrites", [&] {
        db.ClearChanges();
        cleared = true;
    })};

    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {2}};

    while (!cleared.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    if (!cleared.load()) {
        db.UnblockRecordWrite();
        unblock.release();
        cleaner.join();
        FAIL("Queue clear did not finish while the backend write was blocked");
    }

    cleaner.join();

    auto active_overlay {db.GetDocument(collection, in_flight_id)};
    auto ids {db.GetAllDocumentIds(collection)};
    bool active_listed {std::ranges::find(ids, DataBaseKey {in_flight_id}) != ids.end()};
    CHECK(active_listed == (operation != 2));
    CHECK(active_overlay.Empty() == (operation == 2));

    if (!active_overlay.Empty()) {
        CHECK(active_overlay["value"].AsInt64() == 2);
    }

    CHECK(db.GetDocument(collection, discarded_id)["value"].AsInt64() == 1);

    if (queue_after_clear) {
        db.Update(collection, later_id, "value", int64_t {3});
        CHECK(db.GetDocument(collection, later_id)["value"].AsInt64() == 3);
    }

    std::atomic_bool wait_finished {false};
    thread waiter {run_thread("WaitInFlightWrite", [&] {
        db.WaitCommitChanges();
        wait_finished = true;
    })};

    deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds {100};

    while (!wait_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    CHECK_FALSE(wait_finished.load());
    db.UnblockRecordWrite();
    unblock.release();
    waiter.join();
    CHECK(wait_finished.load());

    auto active {db.SnapshotRecord(collection, in_flight_id)};
    CHECK(active.Empty() == (operation == 2));

    auto committed_ids {db.GetAllDocumentIds(collection)};
    bool committed_listed {std::ranges::find(committed_ids, DataBaseKey {in_flight_id}) != committed_ids.end()};
    CHECK(committed_listed == (operation != 2));

    if (!active.Empty()) {
        CHECK(active["value"].AsInt64() == 2);
    }

    auto discarded {db.SnapshotRecord(collection, discarded_id)};
    REQUIRE(!discarded.Empty());
    CHECK(discarded["value"].AsInt64() == 1);

    if (!queue_after_clear) {
        db.Update(collection, later_id, "value", int64_t {3});
        db.WaitCommitChanges();
    }

    auto committed {db.SnapshotRecord(collection, later_id)};
    REQUIRE(!committed.Empty());
    CHECK(committed["value"].AsInt64() == 3);
}

TEST_CASE("DataBaseSlowReadDoesNotBlockOtherReads")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t blocked_id {ident_t {1001}};
    ident_t free_id {ident_t {1002}};

    db.PrimeRecord(collection, blocked_id, MakeDoc({{"value", 11}}));
    db.PrimeRecord(collection, free_id, MakeDoc({{"value", 22}}));
    db.BlockRecordRead(blocked_id);

    std::promise<AnyData::Document> blocked_promise {};
    auto blocked_future {blocked_promise.get_future()};
    std::thread blocked_reader {[&] {
        try {
            blocked_promise.set_value(db.GetDocument(collection, blocked_id));
        }
        catch (...) {
            blocked_promise.set_exception(std::current_exception());
        }
    }};

    db.WaitUntilBlockedReadEntered();

    std::promise<AnyData::Document> free_promise {};
    auto free_future {free_promise.get_future()};
    std::thread free_reader {[&] {
        try {
            free_promise.set_value(db.GetDocument(collection, free_id));
        }
        catch (...) {
            free_promise.set_exception(std::current_exception());
        }
    }};

    REQUIRE(free_future.wait_for(std::chrono::milliseconds {200}) == std::future_status::ready);
    auto free_doc {free_future.get()};
    REQUIRE(!free_doc.Empty());
    CHECK(free_doc["value"].AsInt64() == 22);

    db.UnblockRecordRead();

    auto blocked_doc {blocked_future.get()};
    REQUIRE(!blocked_doc.Empty());
    CHECK(blocked_doc["value"].AsInt64() == 11);

    blocked_reader.join();
    free_reader.join();
}

TEST_CASE("DataBaseInactivePanicCallbackCanRestore", "[.][database][panic]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"panic-callback-restore"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    *FixedSettingForOverride(settings.DataBase.PanicShutdownTimeout) = 60000;
    WriteRecoveryLogs(recovery_logs, "");

    nptr<TestDataBase> target {};
    std::atomic_bool callback_finished {false};
    std::atomic_bool restore_succeeded {false};
    DataBasePanicCallback callback {[&] {
        FO_VERIFY_AND_THROW(target, "Test database is missing");
        target->RestorePendingChanges();
        restore_succeeded = true;
        callback_finished = true;
    }};
    TestDataBase db {settings, DataBaseStringKeyEscaping::Raw, std::move(callback), false};
    target = &db;
    db.InitializeOpLogs();
    db.SetOnGetRecord([] { throw DataBaseException("Simulated database read failure"); });
    hstring collection {hashes.to_hashed_string("test_collection")};
    CHECK_THROWS_AS(db.GetDocument(collection, ident_t {1001}), DataBaseException);
    db.StartInactiveCommitThread();

    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {3}};

    while (!callback_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    CHECK(callback_finished.load());
    CHECK(restore_succeeded.load());
}

TEST_CASE("DataBaseInactivePanicCallbackCanAwaitFailedRead", "[.][database][panic]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"panic-callback-read"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    *FixedSettingForOverride(settings.DataBase.PanicShutdownTimeout) = 60000;
    WriteRecoveryLogs(recovery_logs, "");

    std::atomic_bool reader_entered {false};
    std::atomic_bool release_reader {false};
    std::atomic_bool reader_finished {false};
    std::atomic_bool reader_failed {false};
    std::atomic_bool callback_finished {false};
    std::atomic_bool callback_observed_reader {false};
    DataBasePanicCallback callback {[&] {
        release_reader = true;
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {2}};

        while (!reader_finished.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        callback_observed_reader = reader_finished.load();
        callback_finished = true;
    }};
    TestDataBase db {settings, DataBaseStringKeyEscaping::Raw, std::move(callback), false};
    db.InitializeOpLogs();
    hstring collection {hashes.to_hashed_string("test_collection")};
    db.SetOnGetRecord([&] {
        reader_entered = true;

        while (!release_reader.load()) {
            std::this_thread::yield();
        }

        throw DataBaseException("Simulated blocked database read failure");
    });
    thread reader {run_thread("BlockedDatabaseRead", [&] {
        try {
            (void)db.GetDocument(collection, ident_t {1001});
        }
        catch (const DataBaseException&) {
            reader_failed = true;
        }

        reader_finished = true;
    })};
    auto finish_reader {scope_exit([&]() noexcept {
        release_reader = true;
        reader.join();
    })};
    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (!reader_entered.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(reader_entered.load());
    db.SetOnGetRecord([] { throw DataBaseException("Simulated database read failure"); });
    CHECK_THROWS_AS(db.GetDocument(collection, ident_t {1002}), DataBaseException);
    db.StartInactiveCommitThread();
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};

    while (!callback_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(callback_finished.load());
    CHECK(callback_observed_reader.load());
    reader.join();
    finish_reader.release();
    CHECK(reader_failed.load());
}

TEST_CASE("DataBaseInactivePanicDecisionSurvivesConcurrentStart", "[.][database][panic]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"panic-callback-concurrent-start"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    *FixedSettingForOverride(settings.DataBase.PanicShutdownTimeout) = 60000;
    WriteRecoveryLogs(recovery_logs, "");

    nptr<TestDataBase> target {};
    std::atomic_bool callback_entered {false};
    std::atomic_bool release_callback {false};
    std::atomic_bool callback_finished {false};
    std::atomic_int32_t callback_count {0};
    DataBasePanicCallback callback {[&] {
        callback_count++;
        callback_entered = true;

        while (!release_callback.load()) {
            std::this_thread::yield();
        }

        FO_VERIFY_AND_THROW(target, "Test database is missing");
        target->RestorePendingChanges();
        callback_finished = true;
    }};
    TestDataBase db {settings, DataBaseStringKeyEscaping::Raw, std::move(callback), false};
    auto release_panic {scope_exit([&]() noexcept { release_callback = true; })};
    target = &db;
    db.InitializeOpLogs();
    db.SetOnGetRecord([] { throw DataBaseException("Simulated database read failure"); });
    hstring collection {hashes.to_hashed_string("test_collection")};
    CHECK_THROWS_AS(db.GetDocument(collection, ident_t {1001}), DataBaseException);
    db.StartInactiveCommitThread();
    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (!callback_entered.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(callback_entered.load());
    std::atomic_bool start_finished {false};
    thread starter {run_thread("StartDuringDatabasePanic", [&] {
        db.StartCommitChanges();
        start_finished = true;
    })};
    auto finish_starter {scope_exit([&]() noexcept {
        release_callback = true;
        starter.join();
    })};
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {2};

    while (!start_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    bool started_during_callback {start_finished.load()};
    release_callback = true;
    starter.join();
    finish_starter.release();
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};

    while (!callback_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    CHECK(started_during_callback);
    REQUIRE(callback_finished.load());
    CHECK(callback_count.load() == 1);
    CHECK_FALSE(db.InValidState());
}

TEST_CASE("DataBaseRestorePendingChangesWithoutOpLogIsNoOp")
{
    GlobalSettings settings {false};
    *FixedSettingForOverride(settings.DataBase.OpLogEnabled) = false;
    TestDataBase db {settings};
    bool callback_called {false};
    db.SetOnPendingRestored([&] { callback_called = true; });

    REQUIRE_NOTHROW(db.RestorePendingChanges());
    CHECK_FALSE(callback_called);
    CHECK(db.InValidState());
}

TEST_CASE("DataBaseReconnectWithoutOpLogAfterConcurrentReadFailure")
{
    GlobalSettings settings {false};
    *FixedSettingForOverride(settings.DataBase.OpLogEnabled) = false;
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t active_id {1001};
    ident_t read_id {1002};
    db.PrimeRecord(collection, active_id, MakeDoc({{"value", 1}}));
    db.BlockRecordWrite(active_id);
    auto unblock {scope_exit([&db]() noexcept { db.UnblockRecordWrite(); })};

    db.Update(collection, active_id, "value", int64_t {2});
    db.StartCommitChanges();
    REQUIRE(db.WaitUntilBlockedWriteEntered(std::chrono::seconds {5}));
    db.SetOnGetRecord([] { throw DataBaseException("Simulated database read failure"); });

    CHECK_THROWS_AS(db.GetDocument(collection, read_id), DataBaseException);
    CHECK_FALSE(db.InValidState());
    db.UnblockRecordWrite();
    unblock.release();

    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (!db.InValidState() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(db.InValidState());
    db.WaitCommitChanges();
    auto record {db.SnapshotRecord(collection, active_id)};
    REQUIRE(!record.Empty());
    CHECK(record["value"].AsInt64() == 2);
}

TEST_CASE("DataBaseRestorePendingChangesAllowsCommitCallback")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-from-commit-callback"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    std::atomic_bool callback_completed {false};

    {
        TestDataBase db {settings};
        db.InitializeOpLogs();
        db.SetReconnectFailure();
        db.SetBackendWriteFailure();
        db.SetOnCommitMirrored([&] {
            db.SetBackendWriteFailure(false);
            db.RestorePendingChanges();
            callback_completed = true;
        });

        db.Insert(collection, id, MakeDoc({{"value", 9}}));
        db.StartCommitChanges();
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

        while (!callback_completed.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        REQUIRE(callback_completed.load());
        db.WaitCommitChanges();
        auto stored {db.SnapshotRecord(collection, id)};
        REQUIRE(!stored.Empty());
        CHECK(stored["value"].AsInt64() == 9);
        CHECK(db.InValidState());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestorePendingChangesAllowsNestedRestoredCallback")
{
    GlobalSettings settings {false};
    ScopedRecoveryLogs recovery_logs {"restore-nested-callback"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "");

    {
        TestDataBase db {settings};
        db.InitializeOpLogs();
        bool callback_called {false};
        db.SetOnPendingRestored([&] {
            callback_called = true;
            db.RestorePendingChanges();
        });

        REQUIRE_NOTHROW(db.RestorePendingChanges());
        CHECK(callback_called);
        CHECK(db.InValidState());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestoredCallbackAllowsConcurrentConditionalWrite", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-callback-concurrent-conditional"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "");
    hstring collection {hashes.to_hashed_string("test_collection")};
    TestDataBase db {settings};
    db.InitializeOpLogs();
    std::atomic_bool finished {false};
    bool finished_in_callback {false};
    bool inserted {false};
    thread writer {};
    auto join_writer {scope_exit([&]() noexcept {
        if (writer.joinable()) {
            safe_call([&] { writer.join(); });
        }
    })};

    db.SetOnPendingRestored([&] {
        writer = run_thread("ConditionalFromRestoredCallback", [&] {
            inserted = db.TryInsert(collection, ident_t {1001}, MakeDoc({{"value", 1}}));
            finished = true;
        });

        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {2}};

        while (!finished.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        finished_in_callback = finished.load();
    });

    db.RestorePendingChanges();
    writer.join();
    join_writer.release();
    CHECK(finished_in_callback);
    CHECK(inserted);
    db.ClearChanges();
}

TEST_CASE("DataBaseConditionalReadAllowsRecoveryFromCommitCallback", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"conditional-read-commit-restore"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    TestDataBase db {settings};
    db.InitializeOpLogs();
    db.SetReconnectFailure();
    db.SetBackendWriteFailure();
    db.Insert(collection, ident_t {1001}, MakeDoc({{"value", 1}}));
    std::atomic_bool restored {false};
    bool restored_during_read {false};

    db.SetOnCommitMirrored([&] {
        db.SetBackendWriteFailure(false);
        db.RestorePendingChanges();
        restored = true;
    });
    db.SetOnGetRecord([&] {
        db.StartCommitChanges();
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {2}};

        while (!restored.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        restored_during_read = restored.load();
    });

    (void)db.TryInsert(collection, ident_t {1002}, MakeDoc({{"value", 2}}));
    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (!restored.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    CHECK(restored_during_read);
    REQUIRE(restored.load());
    db.WaitCommitChanges();
}

TEST_CASE("DataBaseNestedRestorePreservesFailureInInnerCallback")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-nested-failure"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "");

    {
        TestDataBase db {settings};
        db.InitializeOpLogs();
        db.SetReconnectFailure();
        db.StartCommitChanges();
        hstring collection {hashes.to_hashed_string("test_collection")};
        bool caught_failure {false};
        bool failed_after_nested {false};

        db.SetOnPendingRestored([&] {
            db.SetOnPendingRestored([&] {
                db.SetOnGetRecord([] { throw DataBaseException("Simulated database read failure"); });

                try {
                    (void)db.GetDocument(collection, ident_t {1001});
                }
                catch (const DataBaseException&) {
                    caught_failure = true;
                }
            });

            db.RestorePendingChanges();
            failed_after_nested = !db.InValidState();
        });

        REQUIRE_NOTHROW(db.RestorePendingChanges());
        CHECK(caught_failure);
        CHECK(failed_after_nested);
        CHECK_FALSE(db.InValidState());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestorePendingChangesAllowsRetryAfterCallbackThrows")
{
    GlobalSettings settings {false};
    ScopedRecoveryLogs recovery_logs {"restore-callback-throws"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "");

    {
        TestDataBase db {settings};
        db.InitializeOpLogs();
        db.SetOnPendingRestored([] { throw DataBaseException("Simulated restore callback failure"); });

        CHECK_THROWS_AS(db.RestorePendingChanges(), DataBaseException);
        CHECK(db.InValidState());
        REQUIRE_NOTHROW(db.RestorePendingChanges());
        CHECK(db.InValidState());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestorePendingChangesAllowsRetryAfterBackendThrows")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-backend-throws"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");

    {
        TestDataBase db {settings};
        db.InitializeOpLogs();
        db.SetOnGetRecord([] { throw DataBaseException("Simulated restore backend failure"); });

        CHECK_THROWS_AS(db.RestorePendingChanges(), DataBaseException);
        REQUIRE_NOTHROW(db.RestorePendingChanges());
        auto restored {db.SnapshotRecord(hashes.to_hashed_string("test_collection"), ident_t {1001})};
        REQUIRE(!restored.Empty());
        CHECK(restored["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseDirectRestoreRefreshesConcurrentRead", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"direct-restore-concurrent-read"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    db.InitializeOpLogs();
    AnyData::Document observed {};

    SECTION("SequentialControl")
    {
        db.RestorePendingChanges();
        observed = db.GetDocument(collection, id);
    }

    SECTION("ConcurrentRead")
    {
        db.BlockRecordReadAfterCopy(id, true);
        std::exception_ptr read_error {};
        thread reader {run_thread("ReadDuringDirectRestore", [&] {
            try {
                observed = db.GetDocument(collection, id);
            }
            catch (const std::exception&) {
                read_error = std::current_exception();
            }
        })};
        auto finish_reader {scope_exit([&]() noexcept {
            db.UnblockRecordRead();
            reader.join();
        })};

        REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));
        std::atomic_bool restore_finished {false};
        std::exception_ptr restore_error {};
        thread restore {run_thread("RestoreDuringRead", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception&) {
                restore_error = std::current_exception();
            }

            restore_finished = true;
        })};
        auto finish_restore {scope_exit([&]() noexcept {
            db.UnblockRecordRead();
            restore.join();
        })};
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

        while (!restore_finished.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        bool restored_before_release {restore_finished.load()};
        db.UnblockRecordRead();
        restore.join();
        finish_restore.release();
        reader.join();
        finish_reader.release();
        REQUIRE(restored_before_release);
        REQUIRE_FALSE(restore_error);
        REQUIRE_FALSE(read_error);
    }

    REQUIRE(!observed.Empty());
    CHECK(observed["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseDirectRestoreRefreshesConcurrentIdListing", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"direct-restore-concurrent-ids"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    TestDataBase db {settings};
    db.InitializeOpLogs();

    SECTION("SequentialControl")
    {
        db.RestorePendingChanges();
    }

    SECTION("ConcurrentListing")
    {
        db.SetOnGetAllRecordIds([&] { db.RestorePendingChanges(); });
    }

    auto ids {db.GetAllDocumentIds(collection)};
    CHECK(ids == vector<DataBaseKey> {DataBaseKey {ident_t {1001}}});
}

TEST_CASE("DataBaseDirectRestoreRetryRefreshesConcurrentRead", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"direct-restore-retry-read"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    db.InitializeOpLogs();
    db.BlockRecordReadAfterCopy(id, true);
    AnyData::Document observed {};
    std::exception_ptr read_error {};
    thread reader {run_thread("ReadDuringFailedRestore", [&] {
        try {
            observed = db.GetDocument(collection, id);
        }
        catch (const std::exception&) {
            read_error = std::current_exception();
        }
    })};
    auto finish_reader {scope_exit([&]() noexcept {
        db.UnblockRecordRead();
        reader.join();
    })};

    REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));
    db.FailAfterNextBackendMutation();
    CHECK_THROWS_AS(db.RestorePendingChanges(), DataBaseException);
    REQUIRE_NOTHROW(db.RestorePendingChanges());
    db.UnblockRecordRead();
    reader.join();
    finish_reader.release();
    REQUIRE_FALSE(read_error);
    REQUIRE(!observed.Empty());
    CHECK(observed["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseDirectRestoreRetryRefreshesConcurrentIdListing", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"direct-restore-retry-ids"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    TestDataBase db {settings};
    db.InitializeOpLogs();
    db.FailAfterNextBackendMutation();
    bool failed_as_expected {false};
    db.SetOnGetAllRecordIds([&] {
        try {
            db.RestorePendingChanges();
        }
        catch (const DataBaseException&) {
            failed_as_expected = true;
        }

        db.RestorePendingChanges();
    });

    auto ids {db.GetAllDocumentIds(collection)};
    CHECK(failed_as_expected);
    CHECK(ids == vector<DataBaseKey> {DataBaseKey {ident_t {1001}}});
}

TEST_CASE("DataBaseDirectRestoreRetryRefreshesListingAfterDelete", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"direct-restore-retry-delete"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "delete test_collection 1001\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.PrimeRecord(collection, id, MakeDoc({{"value", 1}}));
    db.InitializeOpLogs();
    db.FailAfterNextBackendMutation();
    bool failed_as_expected {false};
    bool deleted_after_failure {false};
    db.SetOnGetAllRecordIds([&] {
        try {
            db.RestorePendingChanges();
        }
        catch (const DataBaseException&) {
            failed_as_expected = true;
            deleted_after_failure = db.SnapshotRecord(collection, id).Empty();
        }

        db.RestorePendingChanges();
    });

    auto ids {db.GetAllDocumentIds(collection)};
    CHECK(failed_as_expected);
    CHECK(deleted_after_failure);
    CHECK(ids.empty());
}

TEST_CASE("DataBaseUpdatesRelatedFieldsTogether", "[database][migration]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    db.PrimeRecord(collection, id, MakeDoc({{"value", 1}, {"marker", 0}, {"unrelated", 9}}));
    CHECK_THROWS_AS(db.UpdateFields(collection, id, AnyData::Document {}), DataBaseException);
    bool callback_seen {false};
    bool marker_seen_with_value {false};
    db.SetOnBackendUpdateApplied(2, [&] {
        auto record {db.SnapshotRecord(collection, id)};
        marker_seen_with_value = record.Contains("marker") && record["marker"].AsInt64() == 1;
        callback_seen = true;
    });

    AnyData::Document changes {MakeDoc({{"value", 2}, {"marker", 1}})};
    db.UpdateFields(collection, id, changes);
    changes.Assign("value", numeric_cast<int64_t>(50));
    AnyData::Document invalid {};
    invalid.Assign("value", std::numeric_limits<float64_t>::quiet_NaN());
    CHECK_THROWS_AS(db.UpdateFields(collection, id, invalid), DataBaseException);
    auto pending {db.GetDocument(collection, id)};
    CHECK(pending["value"].AsInt64() == 2);
    CHECK(pending["marker"].AsInt64() == 1);
    CHECK(pending["unrelated"].AsInt64() == 9);
    db.StartCommitChanges();
    db.WaitCommitChanges();

    CHECK(callback_seen);
    CHECK(marker_seen_with_value);
    auto committed {db.SnapshotRecord(collection, id)};
    CHECK(committed["value"].AsInt64() == 2);
    CHECK(committed["marker"].AsInt64() == 1);
    CHECK(committed["unrelated"].AsInt64() == 9);
}

#if FO_DATABASE_JSON || FO_DATABASE_SQLITE
TEST_CASE("DataBaseRelatedFieldUpdateSurvivesReopen", "[database][migration]")
{
    vector<string> providers {};
#if FO_DATABASE_JSON
    providers.emplace_back("JSON");
#endif
#if FO_DATABASE_SQLITE
    providers.emplace_back("DbSQLite");
#endif

    for (const string& provider : providers) {
        CAPTURE(provider);
        GlobalSettings settings {false};
        hash_storage hashes {};
        ScopedRecoveryLogs storage {strex("related-fields-{}", provider).str()};
        hstring collection {hashes.to_hashed_string("test_collection")};
        auto schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
        string connection {strex("{} {}", provider, fs::path_to_string(*storage.Dir())).str()};
        ident_t id {1001};

        {
            auto db {ConnectToDataBase(&settings, connection, schemas, {})};
            db.Insert(collection, id, MakeDoc({{"value", 1}, {"marker", 0}, {"unrelated", 9}}));
            db.StartCommitChanges();
            db.WaitCommitChanges();
            db.UpdateFields(collection, id, MakeDoc({{"value", 2}, {"marker", 1}}));
            db.WaitCommitChanges();
        }

        auto reopened {ConnectToDataBase(&settings, connection, schemas, {})};
        auto record {reopened.Get(collection, id)};
        REQUIRE(!record.Empty());
        CHECK(record["value"].AsInt64() == 2);
        CHECK(record["marker"].AsInt64() == 1);
        CHECK(record["unrelated"].AsInt64() == 9);
    }
}
#endif

TEST_CASE("DataBaseDirectRestorePreservesNewerWrite", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"direct-restore-newer-write"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    db.InitializeOpLogs();
    db.StartCommitChanges();

    SECTION("SequentialControl")
    {
        db.RestorePendingChanges();
        db.Update(collection, id, "value", int64_t {2});
        db.WaitCommitChanges();
    }

    SECTION("ConcurrentReplay")
    {
        db.BlockRecordReadAfterCopy(id);
        std::exception_ptr restore_error {};
        thread restore_thread {run_thread("DirectRestoreOlderValue", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception&) {
                restore_error = std::current_exception();
            }
        })};
        auto finish_restore {scope_exit([&]() noexcept {
            db.UnblockRecordRead();
            restore_thread.join();
        })};

        REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));
        db.Update(collection, id, "value", int64_t {2});

        std::atomic_bool drained {false};
        thread waiter {run_thread("WaitNewerValue", [&] {
            db.WaitCommitChanges();
            drained = true;
        })};
        auto finish_waiter {scope_exit([&]() noexcept {
            db.UnblockRecordRead();
            waiter.join();
        })};
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {3}};

        while (!drained.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        db.UnblockRecordRead();
        restore_thread.join();
        finish_restore.release();
        waiter.join();
        finish_waiter.release();
        CHECK_FALSE(restore_error);
    }

    auto final_doc {db.GetDocument(collection, id)};
    REQUIRE(!final_doc.Empty());
    CHECK(final_doc["value"].AsInt64() == 2);
}

TEST_CASE("DataBaseDirectRestorePreservesNewerDelete", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"direct-restore-newer-delete"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    db.InitializeOpLogs();
    db.StartCommitChanges();

    SECTION("SequentialControl")
    {
        db.RestorePendingChanges();
        db.Delete(collection, id);
        db.WaitCommitChanges();
    }

    SECTION("ConcurrentReplay")
    {
        db.BlockRecordReadAfterCopy(id);
        std::exception_ptr restore_error {};
        thread restore_thread {run_thread("DirectRestoreBeforeDelete", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception&) {
                restore_error = std::current_exception();
            }
        })};
        auto finish_restore {scope_exit([&]() noexcept {
            db.UnblockRecordRead();
            restore_thread.join();
        })};

        REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));
        db.Delete(collection, id);
        std::atomic_bool drained {false};
        thread waiter {run_thread("WaitNewerDelete", [&] {
            db.WaitCommitChanges();
            drained = true;
        })};
        auto finish_waiter {scope_exit([&]() noexcept {
            db.UnblockRecordRead();
            waiter.join();
        })};
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {3}};

        while (!drained.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        db.UnblockRecordRead();
        restore_thread.join();
        finish_restore.release();
        waiter.join();
        finish_waiter.release();
        CHECK_FALSE(restore_error);
    }

    CHECK(db.GetDocument(collection, id).Empty());
}

TEST_CASE("DataBaseAutomaticRecoveryOrdersQueuedDeleteAfterReplay", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"automatic-recovery-queued-delete"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
        db.InitializeOpLogs();
        db.SetReconnectFailure();
        db.SetBackendWriteFailure();
        db.StartCommitChanges();
        db.Update(collection, id, "value", int64_t {1});
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

        while (!db.HasCommitOperationWrittenToOpLog() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        REQUIRE(db.HasCommitOperationWrittenToOpLog());
        db.BlockRecordReadAfterCopy(id);
        auto unblock {scope_exit([&db]() noexcept { db.UnblockRecordRead(); })};
        db.SetBackendWriteFailure(false);
        db.SetReconnectFailure(false);
        REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));
        db.Delete(collection, id);
        db.UnblockRecordRead();
        unblock.release();
        REQUIRE(db.WaitUntilPendingChangesRestored(std::chrono::seconds {5}));
        db.WaitCommitChanges();
        CHECK(db.InValidState());
        CHECK(db.SnapshotRecord(collection, id).Empty());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseAutomaticRecoveryRejectsConditionalWriteUntilReplayCompletes", "[database][recovery]")
{
    bool existing {GENERATE(false, true)};
    CAPTURE(existing);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"automatic-recovery-conditional-write"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();

        if (existing) {
            db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
        }

        db.InitializeOpLogs();
        db.SetReconnectFailure();
        db.SetBackendWriteFailure();
        db.StartCommitChanges();

        if (existing) {
            db.Update(collection, id, "value", int64_t {1});
        }
        else {
            db.Insert(collection, id, MakeDoc({{"value", 1}}));
        }

        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};
        while (!db.HasCommitOperationWrittenToOpLog() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        REQUIRE(db.HasCommitOperationWrittenToOpLog());
        db.BlockRecordReadAfterCopy(id);
        auto unblock {scope_exit([&db]() noexcept { db.UnblockRecordRead(); })};
        db.SetBackendWriteFailure(false);
        db.SetReconnectFailure(false);
        REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));

        auto conditional_write {[&]() -> bool { return existing ? db.CompareExchange(collection, id, "value", int64_t {0}, int64_t {2}) : db.TryInsert(collection, id, MakeDoc({{"value", 2}})); }};

        CHECK_THROWS_AS(conditional_write(), DataBaseException);
        db.UnblockRecordRead();
        unblock.release();
        REQUIRE(db.WaitUntilPendingChangesRestored(std::chrono::seconds {5}));
        db.WaitCommitChanges();
        CHECK(db.InValidState());
        CHECK_FALSE(conditional_write());
        auto result {db.GetDocument(collection, id)};
        REQUIRE(!result.Empty());
        CHECK(result["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReplayRefreshRetriesAfterConcurrentCleanup", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"replay-refresh-concurrent-cleanup"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
        db.InitializeOpLogs();
        bool nested_completed {false};
        db.SetOnReplayBeforePendingRefresh([&] {
            db.RestorePendingChanges();
            nested_completed = true;
        });

        CHECK_NOTHROW(db.RestorePendingChanges());
        CHECK(nested_completed);
        auto record {db.SnapshotRecord(collection, id)};
        REQUIRE(!record.Empty());
        CHECK(record["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReplayRefreshPreservesAppendAfterConcurrentCleanup", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"replay-refresh-cleanup-append"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t first_id {1001};
    ident_t second_id {1002};

    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, first_id, MakeDoc({{"value", 0}}));
        db.PrimeRecord(collection, second_id, MakeDoc({{"value", 0}}));
        db.InitializeOpLogs();
        db.SetReconnectFailure();
        bool cleanup_completed {false};
        db.SetOnReplayBeforePendingRefresh([&] {
            std::exception_ptr nested_error {};
            thread nested {run_thread("ReplayRefreshCleanup", [&] {
                try {
                    db.RestorePendingChanges();
                }
                catch (...) {
                    nested_error = std::current_exception();
                }
            })};
            nested.join();
            if (nested_error) {
                std::rethrow_exception(nested_error);
            }
            cleanup_completed = true;

            db.SetBackendWriteFailure();
            db.StartCommitChanges();
            db.Update(collection, second_id, "value", int64_t {2});
            auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};
            while (!db.HasCommitOperationWrittenToOpLog() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }
            if (!db.HasCommitOperationWrittenToOpLog()) {
                throw DataBaseException("Timed out preparing a new pending change");
            }
            db.SetBackendWriteFailure(false);
        });

        CHECK_NOTHROW(db.RestorePendingChanges());
        CHECK(cleanup_completed);
        auto first {db.SnapshotRecord(collection, first_id)};
        auto second {db.SnapshotRecord(collection, second_id)};
        REQUIRE(!first.Empty());
        REQUIRE(!second.Empty());
        CHECK(first["value"].AsInt64() == 1);
        CHECK(second["value"].AsInt64() == 2);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseConcurrentRestoresPreserveRecordsAndJournal", "[database][recovery]")
{
    int32_t operation {GENERATE(0, 1, 2)};
    CAPTURE(operation);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"concurrent-direct-restores"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string command {operation == 0 ? "insert test_collection 1001 {\"value\":1}\n" : operation == 1 ? "update test_collection 1001 {\"value\":1}\n" : "delete test_collection 1001\n"};
    WriteRecoveryLogs(recovery_logs, command);
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();

        if (operation != 0) {
            db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
        }

        db.InitializeOpLogs();
        db.BlockRecordReadAfterCopy(id, true);

        std::exception_ptr first_error {};
        std::exception_ptr second_error {};
        string first_message {};
        string second_message {};
        std::atomic_bool second_started {false};
        std::atomic_bool second_finished {false};
        thread first {run_thread("FirstDirectRestore", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception& ex) {
                first_error = std::current_exception();
                first_message = ex.what();
            }
        })};
        auto finish_first {scope_exit([&]() noexcept {
            db.UnblockRecordRead();
            first.join();
        })};
        REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));

        thread second {run_thread("SecondDirectRestore", [&] {
            second_started = true;
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception& ex) {
                second_error = std::current_exception();
                second_message = ex.what();
            }
            second_finished = true;
        })};
        auto finish_second {scope_exit([&]() noexcept {
            db.UnblockRecordRead();
            second.join();
        })};
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {3}};
        while (!second_started.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        REQUIRE(second_started.load());

        while (!second_finished.load() && db.GetRecordReadCount(id) < 2 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        db.UnblockRecordRead();
        first.join();
        finish_first.release();
        second.join();
        finish_second.release();

        INFO("First restore: " << first_message);
        INFO("Second restore: " << second_message);
        CHECK_FALSE(first_error);
        CHECK_FALSE(second_error);
        auto result {db.GetDocument(collection, id)};

        if (operation == 2) {
            CHECK(result.Empty());
        }
        else {
            REQUIRE(!result.Empty());
            CHECK(result["value"].AsInt64() == 1);
        }

        CHECK_NOTHROW(db.RestorePendingChanges());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseConcurrentReplayInsertPreservesRecordAndJournal", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"concurrent-replay-insert"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.InitializeOpLogs();
        db.BlockRecordWrite(id);
        std::exception_ptr first_error {};
        std::exception_ptr second_error {};
        thread first {run_thread("FirstReplayInsert", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception&) {
                first_error = std::current_exception();
            }
        })};
        thread second {};
        auto finish {scope_exit([&]() noexcept {
            db.UnblockRecordWrite();
            first.join();
            if (second.joinable()) {
                second.join();
            }
        })};
        REQUIRE(db.WaitUntilBlockedWriteEntered(std::chrono::seconds {5}));
        second = run_thread("SecondReplayInsert", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception&) {
                second_error = std::current_exception();
            }
        });

        bool both_entered {db.WaitUntilBlockedWritesEntered(2, std::chrono::seconds {2})};
        db.UnblockRecordWrite();
        first.join();
        second.join();
        finish.release();

        INFO("Both replay writes entered backend before release: " << both_entered);
        CHECK_FALSE(first_error);
        CHECK_FALSE(second_error);
        auto result {db.GetDocument(collection, id)};
        REQUIRE(!result.Empty());
        CHECK(result["value"].AsInt64() == 1);
        CHECK_NOTHROW(db.RestorePendingChanges());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseLateReplayDoesNotOverwriteWriteAfterJournalCleanup", "[database][recovery]")
{
    bool restore_original_value {GENERATE(false, true)};
    CAPTURE(restore_original_value);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"late-replay-after-cleanup"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.SetStrictRecordSemantics();
    db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    db.InitializeOpLogs();
    db.BlockRecordWrite(id, true);

    std::exception_ptr first_error {};
    std::exception_ptr second_error {};
    std::atomic_bool second_finished {false};
    std::atomic_bool write_finished {false};
    thread first {run_thread("LateReplayFirst", [&] {
        try {
            db.RestorePendingChanges();
        }
        catch (const std::exception&) {
            first_error = std::current_exception();
        }
    })};
    thread second {};
    thread waiter {};
    auto cleanup {scope_exit([&]() noexcept {
        db.UnblockRecordWrite();
        if (first.joinable()) {
            first.join();
        }
        if (second.joinable()) {
            second.join();
        }
        if (waiter.joinable()) {
            waiter.join();
        }
    })};

    REQUIRE(db.WaitUntilBlockedWriteEntered(std::chrono::seconds {5}));
    second = run_thread("LateReplaySecond", [&] {
        try {
            db.RestorePendingChanges();
        }
        catch (const std::exception&) {
            second_error = std::current_exception();
        }
        second_finished = true;
    });

    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {2}};
    while (!second_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    db.StartCommitChanges();
    db.Update(collection, id, "value", int64_t {2});

    if (restore_original_value) {
        db.Update(collection, id, "value", int64_t {0});
    }

    waiter = run_thread("LateReplayNewerWrite", [&] {
        db.WaitCommitChanges();
        write_finished = true;
    });
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {2};
    while (!write_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    bool second_finished_before_release {second_finished.load()};
    bool write_finished_before_release {write_finished.load()};
    db.UnblockRecordWrite();
    first.join();
    second.join();
    waiter.join();
    cleanup.release();

    INFO("Second recovery finished before release: " << second_finished_before_release);
    INFO("Newer write finished before release: " << write_finished_before_release);
    CHECK_FALSE(first_error);
    CHECK_FALSE(second_error);
    auto result {db.GetDocument(collection, id)};
    REQUIRE(!result.Empty());
    CHECK(result["value"].AsInt64() == (restore_original_value ? 0 : 2));
}

TEST_CASE("DataBaseConcurrentReplayRetainsOrderedPartialUpdates", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"concurrent-replay-partial-updates"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1,\"first\":1}\nupdate test_collection 1001 {\"value\":2,\"second\":2}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}, {"first", 0}, {"second", 0}}));
        db.InitializeOpLogs();
        db.BlockRecordWrite(id, true);
        std::exception_ptr first_error {};
        std::exception_ptr second_error {};
        std::atomic_bool second_finished {false};
        thread first {run_thread("FirstReplayPartialUpdates", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception&) {
                first_error = std::current_exception();
            }
        })};
        thread second {};
        auto finish {scope_exit([&]() noexcept {
            db.UnblockRecordWrite();
            if (first.joinable()) {
                first.join();
            }
            if (second.joinable()) {
                second.join();
            }
        })};
        REQUIRE(db.WaitUntilBlockedWriteEntered(std::chrono::seconds {5}));
        second = run_thread("SecondReplayPartialUpdates", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception&) {
                second_error = std::current_exception();
            }
            second_finished = true;
        });
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {2}};
        while (!second_finished.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        db.UnblockRecordWrite();
        first.join();
        second.join();
        finish.release();

        CHECK_FALSE(first_error);
        CHECK_FALSE(second_error);
        auto result {db.SnapshotRecord(collection, id)};
        REQUIRE(!result.Empty());
        CHECK(result["value"].AsInt64() == 2);
        CHECK(result["first"].AsInt64() == 1);
        CHECK(result["second"].AsInt64() == 2);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestoreInsideAppliedBackendWritePreservesBothChanges", "[.][database][recovery]")
{
    bool previous_insert_already_applied {GENERATE(false, true)};
    CAPTURE(previous_insert_already_applied);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-inside-applied-write"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, previous_insert_already_applied ? "insert test_collection 1001 {\"value\":1,\"recovered\":3}\n" : "update test_collection 1001 {\"value\":1,\"recovered\":3}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.PrimeRecord(collection, id, previous_insert_already_applied ? MakeDoc({{"value", 1}, {"recovered", 3}}) : MakeDoc({{"value", 0}, {"recovered", 0}}));
    db.InitializeOpLogs();
    bool callback_fired {false};
    bool nested_rejected {false};
    string rejection_message {};
    db.SetOnBackendUpdateApplied(2, [&] {
        callback_fired = true;

        try {
            db.RestorePendingChanges();
        }
        catch (const DataBaseException& ex) {
            nested_rejected = true;
            rejection_message = ex.what();
        }
    });
    db.StartCommitChanges();
    db.Update(collection, id, "value", int64_t {2});
    db.WaitCommitChanges();

    CHECK(callback_fired);
    CHECK(nested_rejected);
    CHECK(rejection_message.find("recovery from a backend write") != string::npos);
    CHECK_NOTHROW(db.RestorePendingChanges());
    auto result {db.GetDocument(collection, id)};
    REQUIRE(!result.Empty());
    CHECK(result["value"].AsInt64() == 2);
    CHECK(result["recovered"].AsInt64() == 3);
}

TEST_CASE("DataBaseReplayRejectsStableMismatchedJournalProgress", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"stable-mismatched-replay-progress"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string pending {"update test_collection 1001 {\"value\":1}\n"};
    string committed {"update test_collection 1001 {\"value\":2}\n"};
    WriteRecoveryLogs(recovery_logs, pending, committed);
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
        db.InitializeOpLogs();
        CHECK_THROWS_AS(db.RestorePendingChanges(), DataBaseException);
        CHECK(db.SnapshotRecord(collection, id)["value"].AsInt64() == 0);
    }

    CHECK(fs::read_file(recovery_logs.PendingPath()) == pending);
    CHECK(fs::read_file(recovery_logs.CommittedPath()) == committed);
}

TEST_CASE("DataBaseReplayRetriesWhenNewJournalProgressOvertakesSnapshot", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"replay-committed-overtakes-pending"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t first_id {1001};
    ident_t second_id {1002};

    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, first_id, MakeDoc({{"value", 0}}));
        db.PrimeRecord(collection, second_id, MakeDoc({{"value", 0}}));
        db.InitializeOpLogs();
        db.SetReconnectFailure();
        std::atomic_bool nested_ready {false};
        std::atomic_bool release_nested {false};
        std::atomic_bool outer_done {false};
        std::exception_ptr nested_error {};
        std::exception_ptr outer_error {};
        string outer_message {};
        thread nested {};
        thread outer {};
        auto cleanup {scope_exit([&]() noexcept {
            release_nested = true;
            if (outer.joinable()) {
                outer.join();
            }
            if (nested.joinable()) {
                nested.join();
            }
        })};
        db.SetOnReplayBeforeCommittedSnapshot([&] {
            db.SetBackendWriteFailure();
            db.StartCommitChanges();
            db.Update(collection, second_id, "value", int64_t {2});
            auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};
            while (!db.HasCommitOperationWrittenToOpLog() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }
            if (!db.HasCommitOperationWrittenToOpLog()) {
                throw DataBaseException("Timed out preparing pending change");
            }
            db.SetBackendWriteFailure(false);
            db.SetOnReplayBeforeCleanupSnapshot([&] {
                nested_ready = true;
                while (!release_nested.load()) {
                    std::this_thread::yield();
                }
            });
            nested = run_thread("ReplayOvertakingSnapshot", [&] {
                try {
                    db.RestorePendingChanges();
                }
                catch (const std::exception&) {
                    nested_error = std::current_exception();
                }
            });
            deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};
            while (!nested_ready.load() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }
            if (!nested_ready.load()) {
                throw DataBaseException("Timed out waiting for replay progress");
            }
        });
        outer = run_thread("ReplayStalePendingSnapshot", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception& ex) {
                outer_error = std::current_exception();
                outer_message = ex.what();
            }
            outer_done = true;
        });
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {8}};
        while (!outer_done.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        bool finished_without_nested_release {outer_done.load()};
        release_nested = true;
        outer.join();
        if (nested.joinable()) {
            nested.join();
        }
        cleanup.release();

        INFO("Outer recovery: " << outer_message);
        CHECK(nested_ready.load());
        CHECK(finished_without_nested_release);
        CHECK_FALSE(outer_error);
        CHECK_FALSE(nested_error);
        CHECK(db.SnapshotRecord(collection, first_id)["value"].AsInt64() == 1);
        CHECK(db.SnapshotRecord(collection, second_id)["value"].AsInt64() == 2);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReplayCleanupSnapshotRetriesAfterNestedRecovery", "[database][recovery]")
{
    bool other_thread {GENERATE(false, true)};
    CAPTURE(other_thread);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"cleanup-snapshot-nested-recovery"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
        db.InitializeOpLogs();
        bool nested_completed {false};
        db.SetOnReplayBeforeCleanupSnapshot([&] {
            if (other_thread) {
                std::exception_ptr nested_error {};
                thread nested {run_thread("CleanupSnapshotNested", [&] {
                    try {
                        db.RestorePendingChanges();
                    }
                    catch (const std::exception&) {
                        nested_error = std::current_exception();
                    }
                })};
                nested.join();
                if (nested_error) {
                    std::rethrow_exception(nested_error);
                }
            }
            else {
                db.RestorePendingChanges();
            }
            nested_completed = true;
        });

        CHECK_NOTHROW(db.RestorePendingChanges());
        CHECK(nested_completed);
        auto result {db.SnapshotRecord(collection, id)};
        REQUIRE(!result.Empty());
        CHECK(result["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReplayProgressRechecksAfterNestedRecovery", "[database][recovery]")
{
    bool other_thread {GENERATE(false, true)};
    CAPTURE(other_thread);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"replay-progress-nested-recovery"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
        db.InitializeOpLogs();
        bool nested_completed {false};
        std::atomic_bool nested_finished {false};
        std::exception_ptr nested_error {};
        thread nested {};
        auto finish_nested {scope_exit([&]() noexcept {
            if (nested.joinable()) {
                nested.join();
            }
        })};
        db.SetOnReplayWriteValidated([&] {
            if (other_thread) {
                nested = run_thread("RecoveryBeforeReplayProgress", [&] {
                    try {
                        db.RestorePendingChanges();
                    }
                    catch (const std::exception&) {
                        nested_error = std::current_exception();
                    }

                    nested_finished = true;
                });
                auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {2}};
                while (!nested_finished.load() && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::yield();
                }

                nested_completed = nested_finished.load();
            }
            else {
                db.RestorePendingChanges();
                nested_completed = true;
            }
        });

        CHECK_NOTHROW(db.RestorePendingChanges());
        if (nested.joinable()) {
            nested.join();
        }
        finish_nested.release();
        CHECK(nested_completed);
        CHECK_FALSE(nested_error);
        auto result {db.SnapshotRecord(collection, id)};
        REQUIRE(!result.Empty());
        CHECK(result["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReplayWriteRechecksAfterNestedRecovery", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"replay-write-nested-recovery"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
        db.InitializeOpLogs();
        bool callback_fired {false};
        bool finished_in_callback {false};
        std::atomic_bool nested_finished {false};
        std::exception_ptr nested_error {};
        thread nested {};
        auto finish_nested {scope_exit([&]() noexcept {
            if (nested.joinable()) {
                nested.join();
            }
        })};
        db.SetOnBackendUpdateApplied(1, [&] {
            callback_fired = true;
            nested = run_thread("RecoveryDuringReplayWrite", [&] {
                try {
                    db.RestorePendingChanges();
                }
                catch (const std::exception&) {
                    nested_error = std::current_exception();
                }

                nested_finished = true;
            });
            auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {2}};
            while (!nested_finished.load() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }

            finished_in_callback = nested_finished.load();
        });

        CHECK_NOTHROW(db.RestorePendingChanges());
        CHECK(callback_fired);
        REQUIRE(nested.joinable());
        nested.join();
        finish_nested.release();
        CHECK(finished_in_callback);
        CHECK_FALSE(nested_error);
        auto result {db.SnapshotRecord(collection, id)};
        REQUIRE(!result.Empty());
        CHECK(result["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRecoveryFromBackendWriteInAnotherThreadPreservesChanges", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"recovery-in-backend-write-other-thread"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1,\"recovered\":3}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.PrimeRecord(collection, id, MakeDoc({{"value", 0}, {"recovered", 0}}));
    db.InitializeOpLogs();
    std::atomic_bool finished {false};
    bool finished_in_callback {false};
    bool rejected {false};
    string rejection_message {};
    std::exception_ptr unexpected_error {};
    thread nested {};
    auto finish_nested {scope_exit([&]() noexcept {
        if (nested.joinable()) {
            nested.join();
        }
    })};

    db.SetOnBackendUpdateApplied(2, [&] {
        nested = run_thread("NestedRecoveryInBackendWrite", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const DataBaseException& ex) {
                rejected = true;
                rejection_message = ex.what();
            }
            catch (const std::exception&) {
                unexpected_error = std::current_exception();
            }

            finished = true;
        });

        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {2}};
        while (!finished.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        finished_in_callback = finished.load();
    });
    db.StartCommitChanges();
    db.Update(collection, id, "value", int64_t {2});
    db.WaitCommitChanges();
    REQUIRE(nested.joinable());
    nested.join();
    finish_nested.release();

    CHECK(finished_in_callback);
    CHECK_FALSE(unexpected_error);
    if (rejected) {
        CHECK(rejection_message.find("recovery from a backend write") != string::npos);
    }
    CHECK_NOTHROW(db.RestorePendingChanges());
    auto result {db.GetDocument(collection, id)};
    REQUIRE(!result.Empty());
    CHECK(result["value"].AsInt64() == 2);
    CHECK(result["recovered"].AsInt64() == 3);
}

TEST_CASE("DataBaseRejectsRecoveryInsideBackendWriteWithEmptyLog", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"recovery-in-backend-write-empty"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    db.InitializeOpLogs();
    bool nested_rejected {false};
    string rejection_message {};
    db.SetOnBackendUpdateApplied(2, [&] {
        try {
            db.RestorePendingChanges();
        }
        catch (const DataBaseException& ex) {
            nested_rejected = true;
            rejection_message = ex.what();
        }
    });
    db.StartCommitChanges();
    db.Update(collection, id, "value", int64_t {2});
    db.WaitCommitChanges();

    CHECK(nested_rejected);
    CHECK(rejection_message.find("recovery from a backend write") != string::npos);
    CHECK_NOTHROW(db.RestorePendingChanges());
    auto result {db.GetDocument(collection, id)};
    REQUIRE(!result.Empty());
    CHECK(result["value"].AsInt64() == 2);
}

TEST_CASE("DataBaseRejectsRecoveryInsideReplayBackendWrite", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"recovery-in-replay-write"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":2}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    db.InitializeOpLogs();
    bool nested_rejected {false};
    string rejection_message {};
    db.SetOnBackendUpdateApplied(2, [&] {
        try {
            db.RestorePendingChanges();
        }
        catch (const DataBaseException& ex) {
            nested_rejected = true;
            rejection_message = ex.what();
        }
    });

    CHECK_NOTHROW(db.RestorePendingChanges());
    CHECK(nested_rejected);
    CHECK(rejection_message.find("recovery from a backend write") != string::npos);
    auto result {db.GetDocument(collection, id)};
    REQUIRE(!result.Empty());
    CHECK(result["value"].AsInt64() == 2);
}

TEST_CASE("DataBaseBackendWriteAllowsOtherDatabaseRecovery", "[database][recovery]")
{
    GlobalSettings first_settings {false};
    GlobalSettings second_settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs first_logs {"recovery-backend-first"};
    ScopedRecoveryLogs second_logs {"recovery-backend-second"};
    ConfigureRecoverySettings(first_settings, first_logs.PendingPath());
    ConfigureRecoverySettings(second_settings, second_logs.PendingPath());
    WriteRecoveryLogs(first_logs, "");
    WriteRecoveryLogs(second_logs, "update test_collection 1001 {\"value\":3}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase first {first_settings};
    TestDataBase second {second_settings};
    first.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    second.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    first.InitializeOpLogs();
    second.InitializeOpLogs();
    bool second_restored {false};
    bool first_rejected {false};
    second.SetOnBackendUpdateApplied(3, [&] {
        try {
            first.RestorePendingChanges();
        }
        catch (const DataBaseException& ex) {
            first_rejected = string_view {ex.what()}.find("recovery from a backend write") != string_view::npos;
        }
    });
    first.SetOnBackendUpdateApplied(2, [&] {
        second.RestorePendingChanges();
        second_restored = true;
    });
    first.StartCommitChanges();
    first.Update(collection, id, "value", int64_t {2});
    first.WaitCommitChanges();

    CHECK(second_restored);
    CHECK(first_rejected);
    auto first_result {first.GetDocument(collection, id)};
    auto second_result {second.GetDocument(collection, id)};
    REQUIRE(!first_result.Empty());
    REQUIRE(!second_result.Empty());
    CHECK(first_result["value"].AsInt64() == 2);
    CHECK(second_result["value"].AsInt64() == 3);
}

TEST_CASE("DataBaseConditionalReadAllowsNestedRestore", "[database][recovery]")
{
    bool existing {GENERATE(false, true)};
    CAPTURE(existing);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"conditional-read-nested-restore"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, existing ? "update test_collection 1001 {\"value\":1}\n" : "insert test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.SetStrictRecordSemantics();

    if (existing) {
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    }

    db.InitializeOpLogs();
    bool restored {false};
    db.SetOnGetRecord([&] {
        db.RestorePendingChanges();
        restored = true;
    });

    bool changed {existing ? db.CompareExchange(collection, id, "value", int64_t {0}, int64_t {2}) : db.TryInsert(collection, id, MakeDoc({{"value", 2}}))};
    auto logical {db.GetDocument(collection, id)};
    db.ClearChanges();

    CHECK(restored);
    CHECK_FALSE(changed);
    REQUIRE(!logical.Empty());
    CHECK(logical["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseIndependentRecoveriesPreserveRecordAndJournal", "[.][database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"independent-recoveries"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
        db.InitializeOpLogs();
        db.BlockRecordRead(id);
        std::exception_ptr first_error {};
        std::exception_ptr second_error {};
        std::atomic_bool second_started {};
        thread first {run_thread("FirstRecovery", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception&) {
                first_error = std::current_exception();
            }
        })};
        thread second {};
        auto finish {scope_exit([&]() noexcept {
            db.UnblockRecordRead();
            first.join();
            if (second.joinable()) {
                second.join();
            }
        })};
        REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));
        second = run_thread("SecondRecovery", [&] {
            second_started = true;
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception&) {
                second_error = std::current_exception();
            }
        });
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {2}};
        while ((!second_started.load() || db.GetRecordReadCount(id) < 2) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        CHECK(second_started.load());
        db.UnblockRecordRead();
        first.join();
        second.join();
        finish.release();

        CHECK_FALSE(first_error);
        CHECK_FALSE(second_error);
        auto result {db.GetDocument(collection, id)};
        REQUIRE(!result.Empty());
        CHECK(result["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRecoveryOwnerDoesNotBlockCommitHookCycle", "[.][database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"recovery-owner-commit-cycle"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t first_id {1001};
    ident_t second_id {1002};
    TestDataBase db {settings};
    db.PrimeRecord(collection, first_id, MakeDoc({{"value", 0}}));
    db.InitializeOpLogs();
    db.SetReconnectFailure();
    db.SetBackendWriteFailure();
    std::atomic_bool owner_entered {};
    std::atomic_bool nested_finished {};
    std::atomic_bool owner_saw_nested {};
    std::atomic_bool owner_failed {};
    std::atomic_bool nested_unexpected_error {};
    db.SetOnGetRecord([&] {
        owner_entered = true;
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {2}};
        while (!nested_finished.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        owner_saw_nested = nested_finished.load();
    });
    db.SetOnCommitMirrored([&] {
        db.SetBackendWriteFailure(false);
        try {
            db.RestorePendingChanges();
        }
        catch (const DataBaseException& ex) {
            nested_unexpected_error = string_view {ex.what()}.find("Cyclic database recovery") == string_view::npos;
        }
        nested_finished = true;
    });
    thread owner {run_thread("RecoveryOwnerCycle", [&] {
        try {
            db.RestorePendingChanges();
        }
        catch (const DataBaseException&) {
            owner_failed = true;
        }
    })};
    auto finish {scope_exit([&]() noexcept { owner.join(); })};
    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};
    while (!owner_entered.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    REQUIRE(owner_entered.load());
    db.StartCommitChanges();
    db.Insert(collection, second_id, MakeDoc({{"value", 2}}));
    owner.join();
    finish.release();
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};
    while (!nested_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    CHECK(nested_finished.load());
    CHECK(owner_saw_nested.load());
    CHECK_FALSE(nested_unexpected_error.load());
    CHECK_FALSE(owner_failed.load());
}

TEST_CASE("DataBaseRecoveryCommitHookWaitsForIndependentRecovery", "[.][database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"independent-recovery-commit-hook"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t first_id {1001};
    ident_t second_id {1002};

    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, first_id, MakeDoc({{"value", 0}}));
        db.InitializeOpLogs();
        db.SetReconnectFailure();
        db.SetBackendWriteFailure();
        db.BlockRecordReadAfterCopy(first_id, true);
        std::atomic_bool nested_started {};
        std::atomic_bool nested_finished {};
        std::exception_ptr owner_error {};
        std::exception_ptr nested_error {};
        string owner_message {};
        string nested_message {};
        db.SetOnCommitMirrored([&] {
            db.SetBackendWriteFailure(false);
            nested_started = true;

            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception& ex) {
                nested_error = std::current_exception();
                nested_message = ex.what();
            }

            nested_finished = true;
        });
        thread owner {run_thread("IndependentRecoveryOwner", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception& ex) {
                owner_error = std::current_exception();
                owner_message = ex.what();
            }
        })};
        auto finish_owner {scope_exit([&]() noexcept {
            db.UnblockRecordRead();
            owner.join();
        })};
        REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));
        db.StartCommitChanges();
        db.Insert(collection, second_id, MakeDoc({{"value", 2}}));
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

        while (!nested_started.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        CHECK(nested_started.load());
        db.UnblockRecordRead();
        owner.join();
        finish_owner.release();
        deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};

        while (!nested_finished.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        REQUIRE(nested_finished.load());
        INFO("First recovery: " << owner_message);
        INFO("Nested recovery: " << nested_message);
        CHECK_FALSE(owner_error);
        CHECK_FALSE(nested_error);
        auto first_record {db.SnapshotRecord(collection, first_id)};
        auto second_record {db.SnapshotRecord(collection, second_id)};
        REQUIRE(!first_record.Empty());
        REQUIRE(!second_record.Empty());
        CHECK(first_record["value"].AsInt64() == 1);
        CHECK(second_record["value"].AsInt64() == 2);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReplayReadRechecksAfterNestedRecovery", "[database][recovery]")
{
    int32_t operation {GENERATE(0, 1, 2)};
    bool other_thread {GENERATE(false, true)};
    bool after_validation {GENERATE(false, true)};
    CAPTURE(operation, other_thread, after_validation);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"replay-read-nested-recovery"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string command {operation == 0 ? "insert test_collection 1001 {\"value\":1}\n" : operation == 1 ? "update test_collection 1001 {\"value\":1}\n" : "delete test_collection 1001\n"};
    WriteRecoveryLogs(recovery_logs, command);
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();

        if (operation != 0) {
            db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
        }

        db.InitializeOpLogs();
        bool nested_completed {false};
        function<void()> nested_callback {[&] {
            if (other_thread) {
                std::exception_ptr nested_error {};
                thread nested {run_thread("NestedRecoveryDuringReplayRead", [&] {
                    try {
                        db.RestorePendingChanges();
                    }
                    catch (const std::exception&) {
                        nested_error = std::current_exception();
                    }
                })};
                nested.join();

                if (nested_error) {
                    std::rethrow_exception(nested_error);
                }
            }
            else {
                db.RestorePendingChanges();
            }

            nested_completed = true;
        }};

        if (after_validation) {
            db.SetOnReplayReadValidated(std::move(nested_callback));
        }
        else {
            db.SetOnGetRecord(std::move(nested_callback));
        }

        CHECK_NOTHROW(db.RestorePendingChanges());
        CHECK(nested_completed);
        auto result {db.SnapshotRecord(collection, id)};
        if (operation == 2) {
            CHECK(result.Empty());
        }
        else {
            REQUIRE(!result.Empty());
            CHECK(result["value"].AsInt64() == 1);
        }
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReplayReadAllowsNestedConditionalWrite", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"replay-read-nested-conditional"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t replay_id {1001};
    ident_t conditional_id {1002};
    TestDataBase db {settings};
    db.SetStrictRecordSemantics();
    db.PrimeRecord(collection, replay_id, MakeDoc({{"value", 0}}));
    db.PrimeRecord(collection, conditional_id, MakeDoc({{"value", 3}}));
    db.InitializeOpLogs();
    bool called {false};
    bool exchanged {false};
    db.SetOnGetRecord([&] {
        called = true;
        exchanged = db.CompareExchange(collection, conditional_id, "value", int64_t {3}, int64_t {4});
    });

    db.RestorePendingChanges();
    auto replayed {db.GetDocument(collection, replay_id)};
    auto conditional {db.GetDocument(collection, conditional_id)};
    db.ClearChanges();

    CHECK(called);
    CHECK(exchanged);
    REQUIRE(!replayed.Empty());
    REQUIRE(!conditional.Empty());
    CHECK(replayed["value"].AsInt64() == 1);
    CHECK(conditional["value"].AsInt64() == 4);
}

TEST_CASE("DataBaseReadsOverlayPendingJournalInOrder", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"read-pending-journal"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs,
        "insert test_collection 1001 {\"value\":1}\n"
        "update test_collection 1002 {\"value\":2}\n"
        "delete test_collection 1003\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    TestDataBase db {settings};
    db.PrimeRecord(collection, ident_t {1002}, MakeDoc({{"value", 0}, {"other", 9}}));
    db.PrimeRecord(collection, ident_t {1003}, MakeDoc({{"value", 3}}));
    db.InitializeOpLogs();

    auto records {db.GetDocuments(collection, {ident_t {1001}, ident_t {1002}, ident_t {1003}, ident_t {1001}})};
    REQUIRE(records.size() == 4);
    REQUIRE(!records[0].Empty());
    REQUIRE(!records[1].Empty());
    CHECK(records[0]["value"].AsInt64() == 1);
    CHECK(records[1]["value"].AsInt64() == 2);
    CHECK(records[1]["other"].AsInt64() == 9);
    CHECK(records[2].Empty());
    CHECK(records[3] == records[0]);
}

TEST_CASE("DataBaseListsOverlayPendingJournalInOrder", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"list-pending-journal"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs,
        "insert test_collection 1001 {\"value\":1}\n"
        "update test_collection 1002 {\"value\":2}\n"
        "delete test_collection 1003\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    TestDataBase db {settings};
    db.PrimeRecord(collection, ident_t {1002}, MakeDoc({{"value", 0}}));
    db.PrimeRecord(collection, ident_t {1003}, MakeDoc({{"value", 3}}));
    db.InitializeOpLogs();

    auto ids {db.GetAllDocumentIds(collection)};
    CHECK(ids.size() == 2);
    CHECK(std::ranges::find(ids, DataBaseKey {ident_t {1001}}) != ids.end());
    CHECK(std::ranges::find(ids, DataBaseKey {ident_t {1002}}) != ids.end());
    CHECK(std::ranges::find(ids, DataBaseKey {ident_t {1003}}) == ids.end());
}

TEST_CASE("DataBaseReadRejectsConflictingPendingInsert", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"read-conflicting-pending-insert"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    TestDataBase db {settings};
    db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 9}}));
    db.InitializeOpLogs();

    CHECK_THROWS_AS(db.GetDocument(collection, ident_t {1001}), DataBaseException);
    CHECK(db.SnapshotRecord(collection, ident_t {1001})["value"].AsInt64() == 9);
}

TEST_CASE("DataBaseReplayReadOrdersNestedConditionalWriteOnSameRecord", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"replay-read-nested-same-record"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.SetStrictRecordSemantics();
    db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    db.InitializeOpLogs();
    bool called {false};
    bool exchanged {false};
    db.SetOnGetRecord([&] {
        called = true;
        exchanged = db.CompareExchange(collection, id, "value", int64_t {0}, int64_t {2});
    });

    db.RestorePendingChanges();
    auto logical {db.GetDocument(collection, id)};
    db.ClearChanges();

    CHECK(called);
    CHECK_FALSE(exchanged);
    REQUIRE(!logical.Empty());
    CHECK(logical["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseDirectRestoreOrdersConditionalWriteAfterPendingChange", "[database][recovery]")
{
    bool existing {GENERATE(false, true)};
    bool concurrent {GENERATE(false, true)};
    CAPTURE(existing, concurrent);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"direct-restore-try-insert"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, existing ? "update test_collection 1001 {\"value\":1}\n" : "insert test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.SetStrictRecordSemantics();
    if (existing) {
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}}));
    }
    db.InitializeOpLogs();

    if (!concurrent) {
        db.RestorePendingChanges();
        bool changed {existing ? db.CompareExchange(collection, id, "value", int64_t {0}, int64_t {2}) : db.TryInsert(collection, id, MakeDoc({{"value", 2}}))};
        auto logical {db.GetDocument(collection, id)};
        db.ClearChanges();
        CHECK_FALSE(changed);
        REQUIRE(!logical.Empty());
        CHECK(logical["value"].AsInt64() == 1);
        return;
    }

    db.BlockRecordReadAfterCopy(id, true);

    std::exception_ptr restore_error {};
    thread restore_thread {run_thread("RestorePendingInsert", [&] {
        try {
            db.RestorePendingChanges();
        }
        catch (const std::exception&) {
            restore_error = std::current_exception();
        }
    })};
    auto finish_restore {scope_exit([&]() noexcept {
        db.UnblockRecordRead();
        restore_thread.join();
    })};
    REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));

    std::atomic_bool write_started {false};
    std::atomic_bool write_finished {false};
    bool changed {false};
    std::exception_ptr write_error {};
    thread write_thread {run_thread("ConditionalWriteDuringRestore", [&] {
        write_started = true;

        try {
            changed = existing ? db.CompareExchange(collection, id, "value", int64_t {0}, int64_t {2}) : db.TryInsert(collection, id, MakeDoc({{"value", 2}}));
        }
        catch (const std::exception&) {
            write_error = std::current_exception();
        }

        write_finished = true;
    })};
    auto finish_write {scope_exit([&]() noexcept {
        db.UnblockRecordRead();
        write_thread.join();
    })};
    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (!write_started.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(write_started.load());
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {2};

    while (!write_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    db.UnblockRecordRead();
    restore_thread.join();
    finish_restore.release();
    write_thread.join();
    finish_write.release();

    auto logical {db.GetDocument(collection, id)};
    db.ClearChanges();
    CHECK_FALSE(restore_error);
    CHECK_FALSE(write_error);
    CHECK_FALSE(changed);
    REQUIRE(!logical.Empty());
    CHECK(logical["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseHealthyPendingJournalEventuallyDrainsAfterNewWrite", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"healthy-pending-eventually-drains"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1,\"old_field\":7}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}, {"old_field", 0}}));
        db.InitializeOpLogs();
        db.StartCommitChanges();
        db.Update(collection, id, "value", int64_t {2});
        db.WaitCommitChanges();
        CHECK(db.InValidState());
        auto logical {db.GetDocument(collection, id)};
        REQUIRE(!logical.Empty());
        CHECK(logical["value"].AsInt64() == 2);
        CHECK(logical["old_field"].AsInt64() == 7);

        CHECK(db.WaitUntilPendingChangesRestored(std::chrono::seconds {2}));
        auto stored {db.SnapshotRecord(collection, id)};
        REQUIRE(!stored.Empty());
        CHECK(stored["value"].AsInt64() == 2);
        CHECK(stored["old_field"].AsInt64() == 7);
    }
}

TEST_CASE("DataBasePreloggedWriteFailureIsRecordedOnceAndReplayed", "[database][recovery]")
{
    bool fail_after_mutation {GENERATE(false, true)};
    CAPTURE(fail_after_mutation);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"prelogged-write-failure"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string first {"update test_collection 1001 {\"value\":1,\"legacy\":7}\n"};
    string second {"update test_collection 1001 {\"value\":2}\n"};
    WriteRecoveryLogs(recovery_logs, first);
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    AnyData::Document stored_before_restart {};

    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}, {"legacy", 0}}));
        db.InitializeOpLogs();
        db.SetReconnectFailure();

        if (fail_after_mutation) {
            db.FailAfterNextBackendMutation();
        }
        else {
            db.SetBackendWriteFailure();
        }

        db.StartCommitChanges();
        db.Update(collection, id, "value", int64_t {2});
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

        while (!db.HasCommitOperationWrittenToOpLog() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        REQUIRE(db.HasCommitOperationWrittenToOpLog());
        stored_before_restart = db.SnapshotRecord(collection, id);
        REQUIRE(!stored_before_restart.Empty());
        CHECK(stored_before_restart["value"].AsInt64() == (fail_after_mutation ? 1 : 0));
    }

    CHECK(fs::read_file(recovery_logs.PendingPath()) == first + second);

    {
        TestDataBase reopened {settings};
        reopened.PrimeRecord(collection, id, stored_before_restart);
        reopened.InitializeOpLogs();
        REQUIRE_NOTHROW(reopened.RestorePendingChanges());
        auto restored {reopened.GetDocument(collection, id)};
        REQUIRE(!restored.Empty());
        CHECK(restored["value"].AsInt64() == 2);
        CHECK(restored["legacy"].AsInt64() == 7);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBasePreloggedReplaySurvivesFailureAfterEitherMutation", "[database][recovery]")
{
    size_t failure_mutation {GENERATE(size_t {1}, size_t {2})};
    CAPTURE(failure_mutation);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"prelogged-replay-failure-stages"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string first {"update test_collection 1001 {\"value\":1,\"legacy\":7}\n"};
    string second {"update test_collection 1001 {\"value\":2}\n"};
    WriteRecoveryLogs(recovery_logs, first);
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    AnyData::Document stored_before_restart {};

    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}, {"legacy", 0}}));
        db.InitializeOpLogs();
        db.SetReconnectFailure();

        db.FailAfterBackendMutationNumber(failure_mutation);

        db.StartCommitChanges();
        db.Update(collection, id, "value", int64_t {2});
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

        while (!db.HasCommitOperationWrittenToOpLog() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        REQUIRE(db.HasCommitOperationWrittenToOpLog());
        stored_before_restart = db.SnapshotRecord(collection, id);
        REQUIRE(!stored_before_restart.Empty());
        CHECK(stored_before_restart["value"].AsInt64() == (failure_mutation == 1 ? 1 : 2));
    }

    CHECK(fs::read_file(recovery_logs.PendingPath()) == first + second);

    {
        TestDataBase reopened {settings};
        reopened.PrimeRecord(collection, id, stored_before_restart);
        reopened.InitializeOpLogs();
        REQUIRE_NOTHROW(reopened.RestorePendingChanges());
        auto restored {reopened.GetDocument(collection, id)};
        REQUIRE(!restored.Empty());
        CHECK(restored["value"].AsInt64() == 2);
        CHECK(restored["legacy"].AsInt64() == 7);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestartPreservesNewerWriteAndOlderUnrelatedField", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restart-preserves-newer-write"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1,\"legacy\":7}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    AnyData::Document stored_before_restart {};

    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, id, MakeDoc({{"value", 0}, {"legacy", 0}}));
        db.InitializeOpLogs();
        db.StartCommitChanges();
        db.Update(collection, id, "value", int64_t {2});
        db.WaitCommitChanges();
        stored_before_restart = db.SnapshotRecord(collection, id);
        REQUIRE(stored_before_restart["value"].AsInt64() == 2);
    }

    {
        TestDataBase reopened {settings};
        reopened.PrimeRecord(collection, id, stored_before_restart);
        reopened.InitializeOpLogs();
        REQUIRE_NOTHROW(reopened.RestorePendingChanges());
        auto restored {reopened.GetDocument(collection, id)};
        REQUIRE(!restored.Empty());
        CHECK(restored["value"].AsInt64() == 2);
        CHECK(restored["legacy"].AsInt64() == 7);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseDirectRestorePreservesAlreadyAppliedWrite", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"direct-restore-active-write"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1,\"legacy\":7}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.SetStrictRecordSemantics();
    db.PrimeRecord(collection, id, MakeDoc({{"value", 0}, {"legacy", 0}}));
    db.InitializeOpLogs();
    std::atomic_bool applied {false};
    std::atomic_bool release_write {false};
    db.SetOnBackendUpdateApplied(2, [&] {
        applied = true;

        while (!release_write.load()) {
            std::this_thread::yield();
        }
    });
    auto release_worker {scope_exit([&]() noexcept { release_write = true; })};
    db.Update(collection, id, "value", int64_t {2});
    db.StartCommitChanges();
    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (!applied.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(applied.load());
    std::atomic_bool restore_finished {false};
    std::exception_ptr restore_error {};
    thread restore {run_thread("RestoreAfterActiveWrite", [&] {
        try {
            db.RestorePendingChanges();
        }
        catch (const std::exception&) {
            restore_error = std::current_exception();
        }

        restore_finished = true;
    })};
    auto finish_restore {scope_exit([&]() noexcept {
        release_write = true;
        restore.join();
    })};
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {3};

    while (!restore_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    release_write = true;
    restore.join();
    finish_restore.release();
    db.WaitCommitChanges();
    CHECK_FALSE(restore_error);
    auto final_doc {db.GetDocument(collection, id)};
    REQUIRE(!final_doc.Empty());
    CHECK(final_doc["value"].AsInt64() == 2);
    CHECK(final_doc["legacy"].AsInt64() == 7);
}

TEST_CASE("RecoveryLogAppendIfPendingKeepsDurableOrder", "[database][recovery]")
{
    ScopedRecoveryLogs logs {"append-if-pending"};
    string first {"update test_collection 1001 {\"value\":1}\n"};
    string second {"update test_collection 1001 {\"value\":2}\n"};
    using Result = DataBaseImpl::RecoveryLogHandle::ConditionalAppendResult;

    {
        DataBaseImpl::RecoveryLogHandle pending {string(logs.PendingPath())};
        CHECK(pending.AppendIfPending(second) == Result::NoPending);
        CHECK(pending.SnapshotContent().empty());
        REQUIRE(pending.Append(first));
        CHECK(pending.AppendIfPending("") == Result::Failed);
        CHECK(pending.AppendIfPending("invalid command\n") == Result::Failed);
        CHECK(pending.IsUsable());
        CHECK(pending.AppendIfPending(second) == Result::Appended);
    }

    DataBaseImpl::RecoveryLogHandle reopened {string(logs.PendingPath())};
    CHECK((reopened.SnapshotContent() == vector<string> {"update test_collection 1001 {\"value\":1}", "update test_collection 1001 {\"value\":2}"}));
    REQUIRE(reopened.Append("@reset-v2\n"));
    CHECK(reopened.AppendIfPending(second) == Result::Failed);
    CHECK(reopened.IsUsable());
    CHECK(reopened.SnapshotContent().back() == "@reset-v2");
}

TEST_CASE("RecoveryLogAppendIfPendingDoesNotRaceCleanup", "[database][recovery]")
{
    ScopedRecoveryLogs logs {"append-if-pending-cleanup"};
    DataBaseImpl::RecoveryLogHandle pending {string(logs.PendingPath())};
    DataBaseImpl::RecoveryLogHandle committed {string(logs.CommittedPath())};
    string first {"update test_collection 1001 {\"value\":1}\n"};
    string second {"update test_collection 1001 {\"value\":2}\n"};
    using Result = DataBaseImpl::RecoveryLogHandle::ConditionalAppendResult;

    for (size_t iteration {0}; iteration < 16; iteration++) {
        REQUIRE(pending.Truncate());
        REQUIRE(committed.Truncate());
        REQUIRE(pending.Append(first));
        REQUIRE(committed.Append(first));
        auto pending_revision {pending.SnapshotContentWithRevision().second};
        auto committed_revision {committed.SnapshotContentWithRevision().second};
        std::atomic_bool start {false};
        bool cleaned {false};
        uint64_t cleanup_revision {};
        Result result {Result::Failed};

        thread cleanup {run_thread("ConditionalAppendCleanup", [&] {
            while (!start.load()) {
                std::this_thread::yield();
            }
            cleaned = pending.MarkCleanupAndTruncate(committed, pending_revision, committed_revision, cleanup_revision);
        })};
        thread append {run_thread("ConditionalAppendWriter", [&] {
            while (!start.load()) {
                std::this_thread::yield();
            }
            result = pending.AppendIfPending(second);
        })};
        start = true;
        cleanup.join();
        append.join();

        INFO("Iteration: " << iteration);
        if (cleaned) {
            CHECK(result == Result::NoPending);
            CHECK(pending.SnapshotContent().empty());
            CHECK((committed.SnapshotContent() == vector<string> {"update test_collection 1001 {\"value\":1}", "@reset-v2"}));
        }
        else {
            CHECK(result == Result::Appended);
            CHECK((pending.SnapshotContent() == vector<string> {"update test_collection 1001 {\"value\":1}", "update test_collection 1001 {\"value\":2}"}));
            CHECK(committed.SnapshotContent() == vector<string> {"update test_collection 1001 {\"value\":1}"});
        }
    }

    REQUIRE(pending.Truncate());
    REQUIRE(committed.Truncate());
    REQUIRE(pending.Append(first));
    REQUIRE(committed.Append(first));
    auto pending_revision {pending.SnapshotContentWithRevision().second};
    auto committed_revision {committed.SnapshotContentWithRevision().second};
    REQUIRE(pending.AppendIfPending(second) == Result::Appended);
    uint64_t cleanup_revision {};
    CHECK_FALSE(pending.MarkCleanupAndTruncate(committed, pending_revision, committed_revision, cleanup_revision));
    CHECK(pending.GetLinesCount() == 2);

    REQUIRE(pending.Truncate());
    REQUIRE(committed.Truncate());
    REQUIRE(pending.Append(first));
    REQUIRE(committed.Append(first));
    pending_revision = pending.SnapshotContentWithRevision().second;
    committed_revision = committed.SnapshotContentWithRevision().second;
    REQUIRE(pending.MarkCleanupAndTruncate(committed, pending_revision, committed_revision, cleanup_revision));
    CHECK(pending.AppendIfPending(second) == Result::NoPending);
    CHECK(pending.SnapshotContent().empty());
}

TEST_CASE("RecoveryLogReplayProgressRequiresBothCurrentRevisions", "[database][recovery]")
{
    ScopedRecoveryLogs logs {"replay-progress-revisions"};
    DataBaseImpl::RecoveryLogHandle pending {string(logs.PendingPath())};
    DataBaseImpl::RecoveryLogHandle committed {string(logs.CommittedPath())};
    string first {"update test_collection 1001 {\"value\":1}\n"};
    string second {"update test_collection 1002 {\"value\":2}\n"};
    REQUIRE(pending.Append(first));
    auto pending_revision {pending.SnapshotContentWithRevision().second};
    auto committed_revision {committed.SnapshotContentWithRevision().second};
    REQUIRE(pending.Append(second));
    CHECK(pending.AppendReplayProgress(committed, pending_revision, committed_revision, first) == DataBaseImpl::RecoveryLogHandle::ReplayAppendResult::RevisionChanged);
    CHECK(committed.GetLinesCount() == 0);

    pending_revision = pending.SnapshotContentWithRevision().second;
    REQUIRE(committed.Append(first));
    CHECK(pending.AppendReplayProgress(committed, pending_revision, committed_revision, second) == DataBaseImpl::RecoveryLogHandle::ReplayAppendResult::RevisionChanged);
    CHECK(committed.GetLinesCount() == 1);

    committed_revision = committed.SnapshotContentWithRevision().second;
    CHECK(pending.AppendReplayProgress(committed, pending_revision, committed_revision, second) == DataBaseImpl::RecoveryLogHandle::ReplayAppendResult::Appended);
    CHECK(committed.SnapshotContent() == pending.SnapshotContent());
}

TEST_CASE("RecoveryLogRejectsInvalidAppendWithoutDisablingWrites", "[database][recovery]")
{
    ScopedRecoveryLogs logs {"invalid-append"};
    DataBaseImpl::RecoveryLogHandle log {string(logs.PendingPath())};
    CHECK_FALSE(log.Append("invalid command\n"));
    CHECK(log.IsUsable());
    CHECK(log.Append("insert test_collection 1001 {\"value\":1}\n"));
    CHECK(log.GetLinesCount() == 1);
}

TEST_CASE("RecoveryLogSnapshotRemainsStableAcrossAppendAndTruncate", "[database][recovery]")
{
    ScopedRecoveryLogs logs {"snapshot-independent"};
    DataBaseImpl::RecoveryLogHandle log {string(logs.PendingPath())};
    REQUIRE(log.Append("insert test_collection 1001 {\"value\":1}\n"));
    auto snapshot {log.SnapshotContent()};
    REQUIRE(log.Append("update test_collection 1001 {\"value\":2}\n"));
    REQUIRE(log.Truncate());
    REQUIRE(snapshot.size() == 1);
    CHECK(snapshot[0] == "insert test_collection 1001 {\"value\":1}");
    CHECK(log.SnapshotContent().empty());
}

TEST_CASE("RecoveryLogConditionalTruncateRejectsIdenticalReplacement", "[database][recovery]")
{
    ScopedRecoveryLogs logs {"truncate-revision-aba"};
    DataBaseImpl::RecoveryLogHandle log {string(logs.PendingPath())};
    string line {"insert test_collection 1001 {\"value\":1}\n"};
    REQUIRE(log.Append(line));
    auto old_snapshot {log.SnapshotContentWithRevision()};
    CHECK(log.IsUsableAtRevision(old_snapshot.second));
    CHECK_FALSE(log.HasTruncatedSince(old_snapshot.second));
    REQUIRE(log.Truncate());
    REQUIRE(log.Append(line));
    auto replacement {log.SnapshotContentWithRevision()};
    CHECK_FALSE(log.IsUsableAtRevision(old_snapshot.second));
    CHECK(log.IsUsableAtRevision(replacement.second));
    CHECK(log.HasTruncatedSince(old_snapshot.second));
    CHECK_FALSE(log.HasTruncatedSince(replacement.second));
    REQUIRE(replacement.first == old_snapshot.first);
    CHECK(replacement.second != old_snapshot.second);
    CHECK_FALSE(log.TruncateIfRevision(old_snapshot.second));
    CHECK(log.SnapshotContent() == replacement.first);
    CHECK(log.IsUsable());
    CHECK(log.TruncateIfRevision(replacement.second));
    CHECK(log.SnapshotContent().empty());
}

TEST_CASE("RecoveryLogCleanupUsesBothLogRevisions", "[database][recovery]")
{
    ScopedRecoveryLogs logs {"cleanup-both-revisions"};
    DataBaseImpl::RecoveryLogHandle pending {string(logs.PendingPath())};
    DataBaseImpl::RecoveryLogHandle committed {string(logs.CommittedPath())};
    string first {"insert test_collection 1001 {\"value\":1}\n"};
    string second {"insert test_collection 1002 {\"value\":2}\n"};
    REQUIRE(pending.Append(first));
    REQUIRE(committed.Append(first));
    auto pending_snapshot {pending.SnapshotContentWithRevision()};
    auto committed_snapshot {committed.SnapshotContentWithRevision()};
    uint64_t cleanup_revision {};

    SECTION("StalePendingRevisionDoesNotMarkEitherLog")
    {
        REQUIRE(pending.Append(second));
        CHECK_FALSE(pending.MarkCleanupAndTruncate(committed, pending_snapshot.second, committed_snapshot.second, cleanup_revision));
        CHECK(pending.GetLinesCount() == 2);
        CHECK(committed.SnapshotContent() == committed_snapshot.first);
        CHECK(cleanup_revision == 0);
    }

    SECTION("StaleCommittedRevisionDoesNotMarkEitherLog")
    {
        REQUIRE(committed.Append(second));
        CHECK_FALSE(pending.MarkCleanupAndTruncate(committed, pending_snapshot.second, committed_snapshot.second, cleanup_revision));
        CHECK(pending.SnapshotContent() == pending_snapshot.first);
        CHECK(committed.GetLinesCount() == 2);
        CHECK(cleanup_revision == 0);
    }

    SECTION("OldCleanupCannotTruncateReplacementProgress")
    {
        REQUIRE(pending.MarkCleanupAndTruncate(committed, pending_snapshot.second, committed_snapshot.second, cleanup_revision));
        CHECK(pending.GetLinesCount() == 0);
        REQUIRE(committed.Truncate());
        REQUIRE(committed.Append(second));
        CHECK_FALSE(committed.TruncateIfRevision(cleanup_revision));
        auto replacement {committed.SnapshotContent()};
        REQUIRE(replacement.size() == 1);
        CHECK(replacement.front() == "insert test_collection 1002 {\"value\":2}");
    }
}

TEST_CASE("RecoveryLogMarkedCleanupChecksBothRevisions", "[database][recovery]")
{
    bool committed_marked {GENERATE(false, true)};
    CAPTURE(committed_marked);
    ScopedRecoveryLogs logs {"marked-cleanup-both-revisions"};
    DataBaseImpl::RecoveryLogHandle pending {string(logs.PendingPath())};
    DataBaseImpl::RecoveryLogHandle committed {string(logs.CommittedPath())};
    string command {"insert test_collection 1001 {\"value\":1}\n"};
    REQUIRE(pending.Append(command));
    REQUIRE(pending.Append("@reset-v2\n"));
    REQUIRE(committed.Append(command));
    if (committed_marked) {
        REQUIRE(committed.Append("@reset-v2\n"));
    }

    auto pending_snapshot {pending.SnapshotContentWithRevision()};
    auto committed_snapshot {committed.SnapshotContentWithRevision()};
    uint64_t cleanup_revision {};

    SECTION("IdenticalPendingReplacementIsNotTruncated")
    {
        REQUIRE(pending.Truncate());
        REQUIRE(pending.Append(command));
        REQUIRE(pending.Append("@reset-v2\n"));
        CHECK_FALSE(pending.CompleteMarkedCleanup(committed, pending_snapshot.second, committed_snapshot.second, !committed_marked, cleanup_revision));
        CHECK(pending.SnapshotContent() == pending_snapshot.first);
        CHECK(committed.SnapshotContent() == committed_snapshot.first);
    }

    SECTION("IdenticalCommittedReplacementIsNotTruncated")
    {
        REQUIRE(committed.Truncate());
        REQUIRE(committed.Append(command));
        if (committed_marked) {
            REQUIRE(committed.Append("@reset-v2\n"));
        }

        CHECK_FALSE(pending.CompleteMarkedCleanup(committed, pending_snapshot.second, committed_snapshot.second, !committed_marked, cleanup_revision));
        CHECK(pending.SnapshotContent() == pending_snapshot.first);
        CHECK(committed.SnapshotContent() == committed_snapshot.first);
    }
}

TEST_CASE("RecoveryLogLegacyCleanupChecksBothRevisions", "[database][recovery]")
{
    ScopedRecoveryLogs logs {"legacy-cleanup-both-revisions"};
    string command {"insert test_collection 1001 {\"value\":1}\n"};
    WriteRecoveryLogs(logs, "", command + "@reset-v1\n");
    DataBaseImpl::RecoveryLogHandle pending {string(logs.PendingPath())};
    DataBaseImpl::RecoveryLogHandle committed {string(logs.CommittedPath())};
    auto pending_snapshot {pending.SnapshotContentWithRevision()};
    auto committed_snapshot {committed.SnapshotContentWithRevision()};

    SECTION("NewPendingCommandSurvives")
    {
        REQUIRE(pending.Append(command));
        CHECK_FALSE(pending.CompleteLegacyCleanup(committed, pending_snapshot.second, committed_snapshot.second));
        CHECK(pending.GetLinesCount() == 1);
        CHECK(committed.SnapshotContent() == committed_snapshot.first);
    }

    SECTION("NewCommittedContentSurvives")
    {
        REQUIRE(committed.Truncate());
        REQUIRE(committed.Append(command));
        auto replacement {committed.SnapshotContent()};
        CHECK_FALSE(pending.CompleteLegacyCleanup(committed, pending_snapshot.second, committed_snapshot.second));
        CHECK(pending.GetLinesCount() == 0);
        CHECK(committed.SnapshotContent() == replacement);
    }

    SECTION("MatchingLegacyMarkerIsCleared")
    {
        REQUIRE(pending.CompleteLegacyCleanup(committed, pending_snapshot.second, committed_snapshot.second));
        CHECK(pending.GetLinesCount() == 0);
        CHECK(committed.GetLinesCount() == 0);
    }
}

TEST_CASE("RecoveryLogConcurrentAppendsKeepEveryLine", "[database][recovery]")
{
    ScopedRecoveryLogs logs {"concurrent-appends"};
    DataBaseImpl::RecoveryLogHandle log {string(logs.PendingPath())};
    constexpr size_t writer_count {4};
    constexpr size_t lines_per_writer {10};
    std::atomic_size_t ready {};
    std::atomic_size_t written {};
    std::atomic_bool start {};
    vector<std::thread> writers {};
    auto join_writers {scope_exit([&]() noexcept {
        start = true;
        for (auto& writer : writers) {
            if (writer.joinable()) {
                writer.join();
            }
        }
    })};

    for (size_t i {0}; i < writer_count; i++) {
        writers.emplace_back([&] {
            ready++;
            while (!start.load()) {
                std::this_thread::yield();
            }
            for (size_t line {0}; line < lines_per_writer; line++) {
                if (log.Append("insert test_collection 1001 {\"value\":1}\n")) {
                    written++;
                }
            }
        });
    }

    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};
    while (ready.load() != writer_count && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    REQUIRE(ready.load() == writer_count);
    start = true;
    for (auto& writer : writers) {
        writer.join();
    }
    join_writers.release();

    CHECK(written.load() == writer_count * lines_per_writer);
    CHECK(log.GetLinesCount() == writer_count * lines_per_writer);
    CHECK(log.SnapshotContent().size() == writer_count * lines_per_writer);
    CHECK(log.IsUsable());
}

TEST_CASE("RecoveryLogStaysUsableDuringSuccessfulConcurrentAppends", "[database][recovery]")
{
    ScopedRecoveryLogs logs {"concurrent-append-usability"};
    DataBaseImpl::RecoveryLogHandle log {string(logs.PendingPath())};
    constexpr size_t writer_count {4};
    constexpr size_t lines_per_writer {32};
    string payload {strex("insert test_collection 1001 {{\"value\":\"{}\"}}\n", string(4096, 'x')).str()};
    std::atomic_size_t ready {};
    std::atomic_size_t completed {};
    std::atomic_size_t written {};
    std::atomic_bool start {};
    std::atomic_bool saw_unusable {};
    vector<std::thread> writers {};
    auto join_writers {scope_exit([&]() noexcept {
        start = true;
        for (auto& writer : writers) {
            if (writer.joinable()) {
                writer.join();
            }
        }
    })};

    for (size_t i {0}; i < writer_count; i++) {
        writers.emplace_back([&] {
            ready++;
            while (!start.load()) {
                std::this_thread::yield();
            }
            for (size_t line {0}; line < lines_per_writer; line++) {
                if (log.Append(payload)) {
                    written++;
                }
            }
            completed++;
        });
    }

    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};
    while (ready.load() != writer_count && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    REQUIRE(ready.load() == writer_count);
    start = true;
    while (completed.load() != writer_count) {
        if (!log.IsUsable()) {
            saw_unusable = true;
        }
        std::this_thread::yield();
    }
    for (auto& writer : writers) {
        writer.join();
    }
    join_writers.release();

    CHECK(written.load() == writer_count * lines_per_writer);
    CHECK_FALSE(saw_unusable.load());
    CHECK(log.GetLinesCount() == writer_count * lines_per_writer);
    CHECK(log.IsUsable());
}

TEST_CASE("DataBaseRecoveryRechecksSnapshotsAfterNestedRecovery", "[database][recovery]")
{
    int32_t initial_state {GENERATE(0, 1, 2, 3, 4)};
    bool other_thread {GENERATE(false, true)};
    CAPTURE(initial_state, other_thread);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"recovery-snapshot-nested"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string first {"update test_collection 1001 {\"value\":1}\n"};
    string second {"update test_collection 1001 {\"value\":2}\n"};
    string pending {initial_state == 0 ? first : initial_state == 1 ? second : initial_state == 4 ? "" : first + "@reset-v2\n"};
    string committed {initial_state == 0 ? "" : first + (initial_state == 4 ? "@reset-v1\n" : initial_state == 3 ? "" : "@reset-v2\n")};
    WriteRecoveryLogs(recovery_logs, pending, committed);
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.PrimeRecord(collection, id, MakeDoc({{"value", initial_state == 0 ? 0 : 1}}));
        db.InitializeOpLogs();
        bool nested_completed {false};
        db.SetOnRecoveryLogsSnapshotted([&] {
            if (other_thread) {
                std::exception_ptr nested_error {};
                thread nested {run_thread("NestedRecoveryAfterSnapshot", [&] {
                    try {
                        db.RestorePendingChanges();
                    }
                    catch (const std::exception&) {
                        nested_error = std::current_exception();
                    }
                })};
                nested.join();

                if (nested_error) {
                    std::rethrow_exception(nested_error);
                }
            }
            else {
                db.RestorePendingChanges();
            }

            nested_completed = true;
        });
        CHECK_NOTHROW(db.RestorePendingChanges());
        CHECK(nested_completed);
        auto result {db.SnapshotRecord(collection, id)};
        REQUIRE(!result.Empty());
        CHECK(result["value"].AsInt64() == (initial_state == 1 ? 2 : 1));
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestoreResumesMarkedLogCleanup", "[database][recovery]")
{
    bool pending_already_truncated {GENERATE(false, true)};
    CAPTURE(pending_already_truncated);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-marked-log-cleanup"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());

    string commands {"insert test_collection 1001 {\"value\":1}\nupdate test_collection 1001 {\"value\":2}\n"};
    WriteRecoveryLogs(recovery_logs, pending_already_truncated ? "" : commands + "@reset-v2\n", commands + "@reset-v2\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, id, MakeDoc({{"value", 2}}));
        REQUIRE_NOTHROW(db.InitializeOpLogs());
        REQUIRE_NOTHROW(db.RestorePendingChanges());
        auto restored {db.SnapshotRecord(collection, id)};
        REQUIRE(!restored.Empty());
        CHECK(restored["value"].AsInt64() == 2);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestoreCompletesPendingOnlyCleanupMarker", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-pending-only-marker"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string command {"update test_collection 1001 {\"value\":1}\n"};
    WriteRecoveryLogs(recovery_logs, command + "@reset-v2\n", command);

    {
        TestDataBase db {settings};
        db.PrimeRecord(hashes.to_hashed_string("test_collection"), ident_t {1001}, MakeDoc({{"value", 2}}));
        db.InitializeOpLogs();
        REQUIRE_NOTHROW(db.RestorePendingChanges());
        CHECK(db.SnapshotRecord(hashes.to_hashed_string("test_collection"), ident_t {1001})["value"].AsInt64() == 2);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestoreReplaysIdenticalNewPendingAfterMarkedCleanup", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-identical-new-pending"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string command {"update test_collection 1001 {\"value\":1}\n"};
    WriteRecoveryLogs(recovery_logs, command, command + "@reset-v2\n");

    {
        TestDataBase db {settings};
        db.PrimeRecord(hashes.to_hashed_string("test_collection"), ident_t {1001}, MakeDoc({{"value", 2}}));
        db.InitializeOpLogs();
        REQUIRE_NOTHROW(db.RestorePendingChanges());
        CHECK(db.SnapshotRecord(hashes.to_hashed_string("test_collection"), ident_t {1001})["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestoreRejectsInvalidMarkedLogCleanup", "[database][recovery]")
{
    int invalid_case {GENERATE(0, 1, 2, 3)};
    CAPTURE(invalid_case);
    GlobalSettings settings {false};
    ScopedRecoveryLogs recovery_logs {"restore-invalid-marked-cleanup"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());

    string commands {"insert test_collection 1001 {\"value\":1}\nupdate test_collection 1001 {\"value\":2}\n"};
    string pending {commands};
    string committed {commands + "@reset-v1\n"};

    if (invalid_case == 0) {
        pending = "insert test_collection 1001 {\"value\":1}\n";
    }
    else if (invalid_case == 1) {
        committed += "@reset-v1\n";
    }
    else if (invalid_case == 2) {
        pending += "@reset-v1\n";
    }
    else {
        committed = "@reset-v1\n";
        pending.clear();
    }

    WriteRecoveryLogs(recovery_logs, pending, committed);

    {
        TestDataBase db {settings};
        CHECK_THROWS_AS(([&] {
            db.InitializeOpLogs();
            db.RestorePendingChanges();
        }()),
            DataBaseException);
    }

    auto pending_after {fs::read_file(recovery_logs.PendingPath())};
    auto committed_after {fs::read_file(recovery_logs.CommittedPath())};
    REQUIRE(pending_after.has_value());
    REQUIRE(committed_after.has_value());
    CHECK(pending_after.value() == pending);
    CHECK(committed_after.value() == committed);
}

TEST_CASE("DataBaseRestorePreservesAmbiguousIdenticalPendingAfterMarkedCleanup", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-ambiguous-identical-cleanup"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string command {"update test_collection 1001 {\"value\":1}\n"};
    WriteRecoveryLogs(recovery_logs, command, command + "@reset-v1\n");

    {
        TestDataBase db {settings};
        db.PrimeRecord(hashes.to_hashed_string("test_collection"), ident_t {1001}, MakeDoc({{"value", 2}}));
        db.InitializeOpLogs();
        CHECK_THROWS_AS(db.RestorePendingChanges(), DataBaseException);
    }

    CHECK(fs::read_file(recovery_logs.PendingPath()) == command);
    CHECK(fs::read_file(recovery_logs.CommittedPath()) == command + "@reset-v1\n");
}

TEST_CASE("DataBaseRestoreRetriesAfterPendingLogTruncate", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-interrupted-log-reset"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\nupdate test_collection 1001 {\"value\":2}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.InitializeOpLogs();
        bool reached_truncate_boundary {false};
        db.SetOnPendingLogTruncated([&] {
            reached_truncate_boundary = true;
            throw DataBaseException("Simulated failure before committed log truncate");
        });

        CHECK_THROWS_AS(db.RestorePendingChanges(), DataBaseException);
        REQUIRE(reached_truncate_boundary);
        auto applied {db.SnapshotRecord(collection, id)};
        REQUIRE(!applied.Empty());
        REQUIRE(applied["value"].AsInt64() == 2);
        REQUIRE_NOTHROW(db.RestorePendingChanges());
        auto restored {db.SnapshotRecord(collection, id)};
        REQUIRE(!restored.Empty());
        CHECK(restored["value"].AsInt64() == 2);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestoreKeepsAppendDuringLogCleanup", "[.][database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-append-during-cleanup"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t new_id {1002};
    bool appended {false};
    bool recovered_new_record {false};
    bool restore_failed {false};

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.InitializeOpLogs();
        db.SetReconnectFailure();
        db.SetOnPendingLogTruncated([&] {
            db.SetBackendWriteFailure();
            db.StartCommitChanges();
            db.Insert(collection, new_id, MakeDoc({{"value", 2}}));
            auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

            while (!db.HasCommitOperationWrittenToOpLog() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }

            appended = db.HasCommitOperationWrittenToOpLog();
            db.SetBackendWriteFailure(false);
        });

        try {
            db.RestorePendingChanges();
        }
        catch (const DataBaseException&) {
            restore_failed = true;
        }

        REQUIRE(appended);
        if (!restore_failed) {
            db.SetReconnectFailure(false);
            auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

            while (!db.InValidState() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }

            if (db.InValidState()) {
                db.WaitCommitChanges();
                auto record {db.SnapshotRecord(collection, new_id)};
                recovered_new_record = !record.Empty() && record["value"].AsInt64() == 2;
            }
        }
    }

    auto pending {fs::read_file(recovery_logs.PendingPath())};
    REQUIRE(pending.has_value());
    CHECK((recovered_new_record || pending->find("insert test_collection 1002 {\"value\":2}") != string::npos));
}

TEST_CASE("DataBaseOldCleanupPreservesNewRecoveryProgress", "[.][database][recovery]")
{
    int32_t initial_state {GENERATE(0, 1, 2, 3, 4)};
    CAPTURE(initial_state);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"old-cleanup-new-progress"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string first {"insert test_collection 1001 {\"value\":1}\n"};
    string pending {initial_state == 0 || initial_state == 4 ? "" : first + (initial_state >= 2 ? "@reset-v2\n" : "")};
    string committed {initial_state == 4 ? first + "@reset-v1\n" : initial_state >= 2 ? first + (initial_state == 2 ? "@reset-v2\n" : "") : ""};
    WriteRecoveryLogs(recovery_logs, pending, committed);
    hstring collection {hashes.to_hashed_string("test_collection")};
    TestDataBase db {settings};
    if (initial_state >= 2) {
        db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 1}}));
    }
    db.InitializeOpLogs();
    db.SetReconnectFailure();
    thread newer_restore {};
    std::exception_ptr newer_error {};
    bool newer_reached_second_record {false};
    uintmax_t committed_size_before_cleanup {};
    std::error_code size_error {};
    auto finish_newer {scope_exit([&]() noexcept {
        db.UnblockRecordRead();
        if (newer_restore.joinable()) {
            newer_restore.join();
        }
    })};

    db.SetOnPendingLogTruncated([&] {
        if (initial_state == 4) {
            db.RestorePendingChanges();
        }

        db.SetBackendWriteFailure();
        db.StartCommitChanges();
        db.Insert(collection, ident_t {1002}, MakeDoc({{"value", 2}}));
        db.Insert(collection, ident_t {1003}, MakeDoc({{"value", 3}}));
        auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

        while (db.GetCommitOperationsWrittenToOpLog() < 2 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        if (db.GetCommitOperationsWrittenToOpLog() < 2) {
            return;
        }

        db.SetBackendWriteFailure(false);
        db.BlockRecordRead(ident_t {1003});
        newer_restore = run_thread("NewRecoveryDuringOldCleanup", [&] {
            try {
                db.RestorePendingChanges();
            }
            catch (const std::exception&) {
                newer_error = std::current_exception();
            }
        });
        newer_reached_second_record = db.WaitUntilBlockedReadEntered(std::chrono::seconds {5});
        committed_size_before_cleanup = std::filesystem::file_size(std::filesystem::path {fs::make_path(recovery_logs.CommittedPath())}, size_error);
    });

    std::exception_ptr old_error {};
    try {
        db.RestorePendingChanges();
    }
    catch (const std::exception&) {
        old_error = std::current_exception();
    }

    REQUIRE(newer_reached_second_record);
    REQUIRE_FALSE(size_error);
    REQUIRE(committed_size_before_cleanup == string_view {"insert test_collection 1002 {\"value\":2}\n"}.size());
    auto committed_size_after_cleanup {std::filesystem::file_size(std::filesystem::path {fs::make_path(recovery_logs.CommittedPath())}, size_error)};
    REQUIRE_FALSE(size_error);
    CHECK(committed_size_after_cleanup == committed_size_before_cleanup);
    db.UnblockRecordRead();
    newer_restore.join();
    finish_newer.release();
    CHECK_FALSE(old_error);
    CHECK_FALSE(newer_error);
}

TEST_CASE("DataBaseRestoreReopensAfterPendingLogTruncate", "[database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-reopen-interrupted-reset"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\nupdate test_collection 1001 {\"value\":2}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.InitializeOpLogs();
        bool reached_truncate_boundary {false};
        db.SetOnPendingLogTruncated([&] {
            reached_truncate_boundary = true;
            throw DataBaseException("Simulated failure before committed log truncate");
        });

        CHECK_THROWS_AS(db.RestorePendingChanges(), DataBaseException);
        REQUIRE(reached_truncate_boundary);
        auto applied {db.SnapshotRecord(collection, id)};
        REQUIRE(!applied.Empty());
        REQUIRE(applied["value"].AsInt64() == 2);
    }

    auto pending_before_reopen {fs::read_file(recovery_logs.PendingPath())};
    auto committed_before_reopen {fs::read_file(recovery_logs.CommittedPath())};
    REQUIRE(pending_before_reopen.has_value());
    REQUIRE(committed_before_reopen.has_value());
    CHECK(pending_before_reopen->empty());
    CHECK(committed_before_reopen->ends_with("@reset-v2\n"));

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, id, MakeDoc({{"value", 2}}));
        db.InitializeOpLogs();
        REQUIRE_NOTHROW(db.RestorePendingChanges());
        auto restored {db.SnapshotRecord(collection, id)};
        REQUIRE(!restored.Empty());
        CHECK(restored["value"].AsInt64() == 2);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRejectsAliasedRecoveryLogs", "[database][recovery]")
{
    GlobalSettings settings {false};
    ScopedRecoveryLogs recovery_logs {"aliased-recovery-logs"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    string contents {"insert test_collection 1001 {\"value\":1}\n"};

    SECTION("SamePath")
    {
        string path {fs::path_to_string(*recovery_logs.Dir() / "pending")};
        ConfigureRecoverySettings(settings, path);
        REQUIRE(fs::write_file(path, contents));

        {
            TestDataBase db {settings};
            CHECK_THROWS_WITH(db.InitializeOpLogs(), Catch::Matchers::ContainsSubstring("Failed to open recovery oplog file"));
            CHECK_NOTHROW([&] {
                DataBaseImpl::RecoveryLogHandle probe {path};
                ignore_unused(probe);
            }());
        }

        CHECK(fs::read_file(path) == contents);
    }

    SECTION("HardLink")
    {
        ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
        REQUIRE(fs::write_file(recovery_logs.PendingPath(), contents));
        std::error_code error {};
        std::filesystem::create_hard_link(std::filesystem::path {fs::make_path(recovery_logs.PendingPath())}, std::filesystem::path {fs::make_path(recovery_logs.CommittedPath())}, error);

        if (error) {
            SKIP("Hard links are unavailable on the test filesystem");
        }

        {
            TestDataBase db {settings};
            CHECK_THROWS_WITH(db.InitializeOpLogs(), Catch::Matchers::ContainsSubstring("Failed to open recovery oplog file"));
            CHECK_NOTHROW([&] {
                DataBaseImpl::RecoveryLogHandle probe {string(recovery_logs.PendingPath())};
                ignore_unused(probe);
            }());
        }

        CHECK(fs::read_file(recovery_logs.PendingPath()) == contents);
        CHECK(fs::read_file(recovery_logs.CommittedPath()) == contents);
    }
}

TEST_CASE("DataBaseInitializeOpLogsReleasesHandlesAfterValidationFailure", "[database][recovery]")
{
    int failure_case {GENERATE(0, 1, 2)};
    CAPTURE(failure_case);
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"oplog-validation-retry"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string valid {"insert test_collection 1001 {\"value\":1}\n"};
    string invalid {"not-a-command\n"};
    WriteRecoveryLogs(recovery_logs, failure_case == 1 ? invalid : failure_case == 2 ? "" : valid, failure_case == 0 ? invalid : failure_case == 2 ? valid : "");
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};

    {
        TestDataBase db {settings};
        REQUIRE_THROWS_AS(db.InitializeOpLogs(), DataBaseException);
        REQUIRE_NOTHROW([&] {
            DataBaseImpl::RecoveryLogHandle pending_log {string(recovery_logs.PendingPath())};
            DataBaseImpl::RecoveryLogHandle committed_log {string(recovery_logs.CommittedPath())};
            vector<string> expected_pending {failure_case == 1 ? vector<string> {"not-a-command"} : failure_case == 2 ? vector<string> {} : vector<string> {"insert test_collection 1001 {\"value\":1}"}};
            vector<string> expected_committed {failure_case == 0 ? vector<string> {"not-a-command"} : failure_case == 2 ? vector<string> {"insert test_collection 1001 {\"value\":1}"} : vector<string> {}};
            CHECK(pending_log.SnapshotContent() == expected_pending);
            CHECK(committed_log.SnapshotContent() == expected_committed);
        }());
        REQUIRE(fs::write_file(recovery_logs.PendingPath(), valid));
        REQUIRE(fs::write_file(recovery_logs.CommittedPath(), ""));
        REQUIRE_NOTHROW(db.InitializeOpLogs());
        REQUIRE_NOTHROW(db.RestorePendingChanges());
        auto restored {db.GetDocument(collection, id)};
        REQUIRE(!restored.Empty());
        CHECK(restored["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

#if FO_DATABASE_JSON || FO_DATABASE_SQLITE
TEST_CASE("DataBaseRecoveryAndMarkedCleanupSurviveRealBackendReopen", "[database][recovery]")
{
    vector<string> providers {};
#if FO_DATABASE_JSON
    providers.emplace_back("JSON");
#endif
#if FO_DATABASE_SQLITE
    providers.emplace_back("DbSQLite");
#endif

    for (const string& provider : providers) {
        for (int32_t state {0}; state < 4; state++) {
            CAPTURE(provider, state);
            GlobalSettings settings {false};
            hash_storage hashes {};
            ScopedRecoveryLogs recovery_logs {strex("real-recovery-{}-{}", provider, state).str()};
            ScopedCurrentPath current_path {*recovery_logs.Dir()};
            hstring collection {hashes.to_hashed_string("test_collection")};
            ident_t id {1001};
            auto schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
            string connection {strex("{} {}", provider, fs::path_to_string(*recovery_logs.Dir() / "records")).str()};

            *FixedSettingForOverride(settings.DataBase.OpLogEnabled) = false;
            {
                auto db {ConnectToDataBase(&settings, connection, schemas, {})};
                db.StartCommitChanges();
                db.Insert(collection, id, MakeDoc({{"value", state == 0 ? 0 : 2}, {"other", 7}}));
                db.WaitCommitChanges();
            }

            ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
            string command {"update test_collection 1001 {\"value\":2}\n"};
            string pending {state == 0 ? command : state == 3 ? "" : command + "@reset-v2\n"};
            string committed {state == 0 ? "" : state == 1 ? command : command + "@reset-v2\n"};
            WriteRecoveryLogs(recovery_logs, pending, committed);

            {
                auto db {ConnectToDataBase(&settings, connection, schemas, {})};
                auto record {db.Get(collection, id)};
                REQUIRE(!record.Empty());
                CHECK(record["value"].AsInt64() == 2);
                CHECK(record["other"].AsInt64() == 7);
            }

            CheckRecoveryLogsCleared(recovery_logs);

            {
                auto db {ConnectToDataBase(&settings, connection, schemas, {})};
                auto record {db.Get(collection, id)};
                REQUIRE(!record.Empty());
                CHECK(record["value"].AsInt64() == 2);
                CHECK(record["other"].AsInt64() == 7);
            }
        }
    }
}

TEST_CASE("DataBaseRestoreRejectsAmbiguousLegacyLogWithoutChangingRecords", "[database][recovery]")
{
    vector<string> providers {};
#if FO_DATABASE_JSON
    providers.emplace_back("JSON");
#endif
#if FO_DATABASE_SQLITE
    providers.emplace_back("DbSQLite");
#endif

    for (const string& provider : providers) {
        CAPTURE(provider);
        GlobalSettings settings {false};
        hash_storage hashes {};
        ScopedRecoveryLogs recovery_logs {strex("restore-reopen-interrupted-reset-{}", provider).str()};
        ScopedCurrentPath current_path {*recovery_logs.Dir()};
        hstring collection {hashes.to_hashed_string("test_collection")};
        ident_t id {1001};
        auto schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
        string connection_info {strex("{} {}", provider, fs::path_to_string(*recovery_logs.Dir() / "records")).str()};

        *FixedSettingForOverride(settings.DataBase.OpLogEnabled) = false;
        {
            auto db {ConnectToDataBase(&settings, connection_info, schemas, {})};
            db.StartCommitChanges();
            db.Insert(collection, id, MakeDoc({{"value", 1}}));
            db.Update(collection, id, "value", int64_t {2});
            db.WaitCommitChanges();
            REQUIRE(db.Get(collection, id)["value"].AsInt64() == 2);
        }

        ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
        string pending {"insert test_collection 1001 {\"value\":1}\nupdate test_collection 1001 {\"value\":2}\n"};
        WriteRecoveryLogs(recovery_logs, pending);

        CHECK_THROWS_AS(ConnectToDataBase(&settings, connection_info, schemas, {}), DataBaseException);
        CHECK(fs::read_file(recovery_logs.PendingPath()) == pending);
        CHECK(fs::read_file(recovery_logs.CommittedPath()) == "");

        *FixedSettingForOverride(settings.DataBase.OpLogEnabled) = false;
        auto reopened {ConnectToDataBase(&settings, connection_info, schemas, {})};
        CHECK(reopened.Get(collection, id)["value"].AsInt64() == 2);
    }
}
#endif

TEST_CASE("DataBaseDirectRestoreDuringConcurrentOplogAppend", "[.][database][recovery]")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"direct-restore-concurrent-append"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t first_id {1001};
    ident_t second_id {1002};
    TestDataBase db {settings};
    db.InitializeOpLogs();
    db.SetReconnectFailure();
    db.SetBackendWriteFailure();
    db.StartCommitChanges();
    db.Insert(collection, first_id, MakeDoc({{"value", 1}}));
    db.WaitUntilCommitOperationWrittenToOpLog();
    db.SetBackendWriteFailure(false);
    db.BlockRecordRead(first_id);

    std::exception_ptr restore_error {};
    string restore_message {};
    thread restore_thread {run_thread("DirectRestore", [&] {
        try {
            db.RestorePendingChanges();
        }
        catch (const std::exception& ex) {
            restore_error = std::current_exception();
            restore_message = ex.what();
        }
    })};

    auto finish_restore {scope_exit([&]() noexcept {
        db.UnblockRecordRead();
        restore_thread.join();
    })};

    REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));
    db.Insert(collection, second_id, MakeDoc({{"value", 2}}));
    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (db.GetCommitOperationsWrittenToOpLog() < 2 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(db.GetCommitOperationsWrittenToOpLog() >= 2);
    db.UnblockRecordRead();
    restore_thread.join();
    finish_restore.release();
    INFO("Direct restore failure: " << restore_message);
    CHECK_FALSE(restore_error);
    db.SetReconnectFailure(false);
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};

    while (!db.InValidState() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(db.InValidState());
    db.WaitCommitChanges();
    auto first {db.SnapshotRecord(collection, first_id)};
    auto second {db.SnapshotRecord(collection, second_id)};
    REQUIRE(!first.Empty());
    REQUIRE(!second.Empty());
    CHECK(first["value"].AsInt64() == 1);
    CHECK(second["value"].AsInt64() == 2);
}

TEST_CASE("DataBaseRestorePendingDeleteIsIdempotent")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-delete"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "delete test_collection 1001\n");

    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.InitializeOpLogs();

        REQUIRE_NOTHROW(db.RestorePendingChanges());
        CHECK(db.SnapshotRecord(hashes.to_hashed_string("test_collection"), ident_t {1001}).Empty());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestorePendingInsertSkipsEqualDocument")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-insert-same"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");

    hstring collection {hashes.to_hashed_string("test_collection")};
    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 1}}));
        db.InitializeOpLogs();

        REQUIRE_NOTHROW(db.RestorePendingChanges());

        auto doc {db.SnapshotRecord(collection, ident_t {1001})};
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestorePendingInsertDetectsConflict")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-insert-conflict"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "insert test_collection 1001 {\"value\":1}\n");

    hstring collection {hashes.to_hashed_string("test_collection")};
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
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-update-same"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");

    hstring collection {hashes.to_hashed_string("test_collection")};
    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 1}, {"other", 7}}));
        db.InitializeOpLogs();

        REQUIRE_NOTHROW(db.RestorePendingChanges());

        auto doc {db.SnapshotRecord(collection, ident_t {1001})};
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
        CHECK(doc["other"].AsInt64() == 7);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseRestorePendingUpdateAppliesPatch")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"restore-update-apply"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":3,\"added\":9}\n");

    hstring collection {hashes.to_hashed_string("test_collection")};
    {
        TestDataBase db {settings};
        db.SetStrictRecordSemantics();
        db.PrimeRecord(collection, ident_t {1001}, MakeDoc({{"value", 1}, {"other", 7}}));
        db.InitializeOpLogs();

        REQUIRE_NOTHROW(db.RestorePendingChanges());

        auto doc {db.SnapshotRecord(collection, ident_t {1001})};
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

    string pending_content {"insert test_string_collection "};
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
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"stop-after-spill"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};

    {
        TestDataBase db {settings};
        db.InitializeOpLogs();
        db.SetBackendWriteFailure();
        db.StartCommitChanges();
        db.Insert(collection, ident_t {1001}, MakeDoc({{"value", 1}}));
        db.WaitUntilCommitOperationWrittenToOpLog();

        mutex fallback_locker {};
        std::condition_variable_any fallback_cv {};
        bool stop_completed {false};
        std::thread fallback_thread {[&] {
            unique_lock locker {fallback_locker};

            if (!fallback_cv.wait_for(locker, std::chrono::milliseconds {500}, [&] { return stop_completed; })) {
                db.SetBackendWriteFailure(false);
            }
        }};

        auto start {std::chrono::steady_clock::now()};
        db.WaitCommitChanges();
        auto elapsed {std::chrono::steady_clock::now() - start};

        {
            scoped_lock locker {fallback_locker};

            stop_completed = true;
        }

        fallback_cv.notify_one();
        fallback_thread.join();

        CHECK(elapsed < std::chrono::milliseconds {250});
    }

    auto pending_content {fs::read_file(recovery_logs.PendingPath())};
    REQUIRE(pending_content.has_value());
    CHECK(!pending_content->empty());
}

TEST_CASE("DataBaseWaitCommitChangesWakesOnSpillAfterClear")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"clear-wait-spill"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t active_id {1001};
    ident_t later_id {1002};
    TestDataBase db {settings};
    db.InitializeOpLogs();
    db.PrimeRecord(collection, active_id, MakeDoc({{"value", 1}}));
    db.PrimeRecord(collection, later_id, MakeDoc({{"value", 1}}));
    db.BlockRecordWrite(active_id);
    auto unblock {scope_exit([&db]() noexcept { db.UnblockRecordWrite(); })};
    db.Update(collection, active_id, "value", int64_t {2});
    db.StartCommitChanges();
    REQUIRE(db.WaitUntilBlockedWriteEntered(std::chrono::seconds {5}));
    db.ClearChanges();
    db.Update(collection, later_id, "value", int64_t {3});

    std::atomic_bool waiter_started {false};
    std::atomic_bool waiter_finished {false};
    thread waiter {run_thread("WaitForFailedWrite", [&] {
        waiter_started = true;
        db.WaitCommitChanges();
        waiter_finished = true;
    })};

    while (!waiter_started.load()) {
        std::this_thread::yield();
    }

    auto deadline {std::chrono::steady_clock::now() + std::chrono::milliseconds {100}};

    while (!waiter_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    CHECK_FALSE(waiter_finished.load());
    db.SetBackendWriteFailure();
    db.UnblockRecordWrite();
    unblock.release();
    db.WaitUntilCommitOperationWrittenToOpLog();
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {1};

    while (!waiter_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    bool returned_before_recovery {waiter_finished.load()};
    db.SetBackendWriteFailure(false);
    waiter.join();
    CHECK(returned_before_recovery);

    db.WaitUntilPendingChangesRestored();
    db.WaitCommitChanges();
    CHECK(db.SnapshotRecord(collection, active_id)["value"].AsInt64() == 2);
    CHECK(db.SnapshotRecord(collection, later_id)["value"].AsInt64() == 3);
}

TEST_CASE("DataBaseWaitCommitChangesWakesOnReadFailureDuringCommit")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"wait-read-failure"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t active_id {1001};
    ident_t read_id {1002};
    TestDataBase db {settings};
    db.InitializeOpLogs();
    db.PrimeRecord(collection, active_id, MakeDoc({{"value", 1}}));
    db.PrimeRecord(collection, read_id, MakeDoc({{"value", 1}}));
    db.BlockRecordWrite(active_id);
    auto unblock {scope_exit([&db]() noexcept { db.UnblockRecordWrite(); })};
    db.Update(collection, active_id, "value", int64_t {2});
    db.StartCommitChanges();
    REQUIRE(db.WaitUntilBlockedWriteEntered(std::chrono::seconds {5}));

    std::atomic_bool waiter_started {false};
    std::atomic_bool waiter_finished {false};
    thread waiter {run_thread("WaitForFailedRead", [&] {
        waiter_started = true;
        db.WaitCommitChanges();
        waiter_finished = true;
    })};

    while (!waiter_started.load()) {
        std::this_thread::yield();
    }

    auto deadline {std::chrono::steady_clock::now() + std::chrono::milliseconds {100}};

    while (!waiter_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    CHECK_FALSE(waiter_finished.load());
    db.SetOnGetRecord([] { throw DataBaseException("Simulated database read failure"); });
    CHECK_THROWS_AS(db.GetDocument(collection, read_id), DataBaseException);
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {1};

    while (!waiter_finished.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    bool returned_before_release {waiter_finished.load()};
    db.UnblockRecordWrite();
    unblock.release();
    waiter.join();
    CHECK(returned_before_release);
    db.WaitUntilPendingChangesRestored();
    db.WaitCommitChanges();
    CHECK(db.SnapshotRecord(collection, active_id)["value"].AsInt64() == 2);
    CHECK_FALSE(db.HasCommitOperationWrittenToOpLog());
}

TEST_CASE("DataBaseSuccessfulCommitIsNotSpilledByConcurrentReadFailure")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"successful-write-read-failure"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t active_id {1001};
    ident_t read_id {1002};
    TestDataBase db {settings};
    db.InitializeOpLogs();
    db.PrimeRecord(collection, active_id, MakeDoc({{"value", 1}}));
    db.BlockRecordWrite(active_id);
    auto unblock {scope_exit([&db]() noexcept { db.UnblockRecordWrite(); })};
    db.Update(collection, active_id, "value", int64_t {2});
    db.StartCommitChanges();
    REQUIRE(db.WaitUntilBlockedWriteEntered(std::chrono::seconds {5}));

    db.SetOnGetRecord([] { throw DataBaseException("Simulated database read failure"); });
    CHECK_THROWS_AS(db.GetDocument(collection, read_id), DataBaseException);
    CHECK_FALSE(db.InValidState());
    db.UnblockRecordWrite();
    unblock.release();
    REQUIRE(db.WaitUntilPendingChangesRestored(std::chrono::seconds {5}));

    CHECK_FALSE(db.HasCommitOperationWrittenToOpLog());
    CHECK(db.SnapshotRecord(collection, active_id)["value"].AsInt64() == 2);
}

TEST_CASE("DataBaseRecoveryPreservesReadFailureDuringRestoredCallback")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"failure-during-recovery"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t id {1001};
    TestDataBase db {settings};
    db.InitializeOpLogs();
    db.BlockPendingChangesRestored();
    auto unblock {scope_exit([&db]() noexcept { db.UnblockPendingChangesRestored(); })};
    db.SetBackendWriteFailure();
    db.StartCommitChanges();
    db.Insert(collection, id, MakeDoc({{"value", 1}}));
    db.WaitUntilCommitOperationWrittenToOpLog();
    db.SetBackendWriteFailure(false);
    REQUIRE(db.WaitUntilPendingChangesRestored(std::chrono::seconds {5}));

    db.SetBackendWriteFailure();
    db.SetOnGetRecord([] { throw DataBaseException("Simulated database read failure"); });
    CHECK_THROWS_AS(db.GetDocument(collection, id), DataBaseException);
    CHECK_FALSE(db.InValidState());
    db.UnblockPendingChangesRestored();
    unblock.release();

    auto deadline {std::chrono::steady_clock::now() + std::chrono::milliseconds {500}};
    bool failure_lost {false};

    while (std::chrono::steady_clock::now() < deadline) {
        if (db.InValidState()) {
            failure_lost = true;
            break;
        }

        std::this_thread::yield();
    }

    db.SetBackendWriteFailure(false);
    CHECK_FALSE(failure_lost);
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};

    while (!db.InValidState() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    CHECK(db.InValidState());
    db.WaitCommitChanges();
    CHECK(db.SnapshotRecord(collection, id)["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseRecoveryPreservesReadFailureDuringReplay")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"failure-during-replay"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t active_id {1001};
    ident_t read_id {1002};
    TestDataBase db {settings};
    db.InitializeOpLogs();
    std::atomic_bool read_entered {false};
    std::atomic_bool fail_read {false};
    std::exception_ptr read_error {};
    db.SetOnGetRecord([&] {
        read_entered = true;

        while (!fail_read.load()) {
            std::this_thread::yield();
        }

        throw DataBaseException("Simulated database read failure");
    });

    thread reader {run_thread("FailReadDuringReplay", [&] {
        try {
            (void)db.GetDocument(collection, read_id);
        }
        catch (const std::exception&) {
            read_error = std::current_exception();
        }
    })};

    auto finish_reader {scope_exit([&]() noexcept {
        fail_read = true;
        reader.join();
    })};

    auto deadline {std::chrono::steady_clock::now() + std::chrono::seconds {5}};

    while (!read_entered.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    REQUIRE(read_entered.load());
    db.BlockRecordRead(active_id);
    auto unblock {scope_exit([&db]() noexcept { db.UnblockRecordRead(); })};
    db.SetBackendWriteFailure();
    db.StartCommitChanges();
    db.Insert(collection, active_id, MakeDoc({{"value", 1}}));
    db.WaitUntilCommitOperationWrittenToOpLog();
    db.SetBackendWriteFailure(false);
    REQUIRE(db.WaitUntilBlockedReadEntered(std::chrono::seconds {5}));

    db.SetReconnectFailure();
    fail_read = true;
    reader.join();
    finish_reader.release();
    CHECK(bool {read_error});
    CHECK_FALSE(db.InValidState());
    db.UnblockRecordRead();
    unblock.release();
    REQUIRE(db.WaitUntilPendingChangesRestored(std::chrono::seconds {5}));
    CHECK_FALSE(db.InValidState());

    db.SetReconnectFailure(false);
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};

    while (!db.InValidState() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    CHECK(db.InValidState());
    db.WaitCommitChanges();
    CHECK(db.SnapshotRecord(collection, active_id)["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseSupportsStringKeys")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_string_collection")};
    DataBaseKey record_id {string("steam:user-123")};

    db.StartCommitChanges();
    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.WaitCommitChanges();

    auto doc {db.SnapshotRecord(collection, record_id)};
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

TEST_CASE("DataBaseGetAllDocumentIdsAppliesPendingInsertsAndDeletes")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};

    // Nothing is committed yet: the listing must already show the insert and hide the delete
    db.PrimeRecord(collection, ident_t {1}, MakeDoc({{"value", 1}}));
    db.PrimeRecord(collection, ident_t {2}, MakeDoc({{"value", 2}}));
    db.Insert(collection, ident_t {3}, MakeDoc({{"value", 3}}));
    db.Delete(collection, ident_t {1});
    db.Insert(collection, ident_t {4}, MakeDoc({{"value", 4}}));
    db.Delete(collection, ident_t {4});

    auto pending_ids {db.GetAllDocumentIds(collection)};
    CHECK(pending_ids == vector<DataBaseKey> {DataBaseKey {ident_t {2}}, DataBaseKey {ident_t {3}}});

    db.StartCommitChanges();
    db.WaitCommitChanges();

    auto committed_ids {db.GetAllDocumentIds(collection)};
    std::ranges::sort(committed_ids, std::less<> {});
    CHECK(committed_ids == pending_ids);
}

TEST_CASE("DataBaseGetAllDocumentIdsMarksBackendReadFailure")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings, DataBaseStringKeyEscaping::Raw, nullptr, false};
    hstring collection {hashes.to_hashed_string("test_collection")};
    db.SetOnGetAllRecordIds([] { throw std::logic_error("Simulated backend listing failure"); });
    bool preserved_exception {false};

    try {
        (void)db.GetAllDocumentIds(collection);
    }
    catch (const std::logic_error& ex) {
        preserved_exception = string_view {ex.what()} == "Simulated backend listing failure";
    }

    CHECK(preserved_exception);
    CHECK_FALSE(db.InValidState());
}

TEST_CASE("DataBaseGetAllDocumentIdsRelistsKeysCommittedDuringListing")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_collection")};

    // The insert is committed and leaves the queue after the backend was listed but before the listing is checked
    db.Insert(collection, ident_t {5}, MakeDoc({{"value", 5}}));
    db.SetOnGetAllRecordIds([&db] {
        db.StartCommitChanges();
        db.WaitCommitChanges();
    });

    auto ids {db.GetAllDocumentIds(collection)};
    CHECK(ids == vector<DataBaseKey> {DataBaseKey {ident_t {5}}});
}

#if FO_DATABASE_JSON
TEST_CASE("DataBaseJsonReadReportsInvalidCollectionPath")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"json-read-invalid-collection"};
    hstring collection {hashes.to_hashed_string("test_collection")};
    auto schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
    string connection_info {strex("JSON {}", fs::path_to_string(*recovery_logs.Dir())).str()};
    auto db {ConnectToDataBase(&settings, connection_info, schemas, {})};

    CHECK(db.Get(collection, ident_t {1}).Empty());

    auto collection_path {*recovery_logs.Dir() / "test_collection"};
    REQUIRE(std::filesystem::remove(collection_path));

    SECTION("CollectionIsMissing")
    {
    }
    SECTION("CollectionIsAFile")
    {
        REQUIRE(fs::write_file(fs::path_to_string(collection_path), "not a directory"));
    }

    CHECK_THROWS_AS(db.Get(collection, ident_t {1}), DataBaseException);
}

TEST_CASE("DataBaseJsonReadReportsDanglingRecordLink")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"json-read-dangling-record"};
    hstring collection {hashes.to_hashed_string("test_collection")};
    auto schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
    string connection_info {strex("JSON {}", fs::path_to_string(*recovery_logs.Dir())).str()};
    auto db {ConnectToDataBase(&settings, connection_info, schemas, {})};
    auto record_path {*recovery_logs.Dir() / "test_collection" / "1.json"};
    std::error_code error {};
    std::filesystem::create_symlink(*recovery_logs.Dir() / "missing.json", record_path, error);

    if (error) {
        SKIP("Creating symbolic links is unavailable");
    }

    CHECK_THROWS_AS(db.Get(collection, ident_t {1}), DataBaseException);
    CHECK(std::filesystem::is_symlink(std::filesystem::symlink_status(record_path, error)));
    CHECK_FALSE(error);
}

TEST_CASE("DataBaseJsonListingReportsMissingCollectionDirectory")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"json-listing-missing-directory"};
    hstring collection {hashes.to_hashed_string("test_collection")};
    auto schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
    string connection_info {strex("JSON {}", fs::path_to_string(*recovery_logs.Dir())).str()};
    auto db {ConnectToDataBase(&settings, connection_info, schemas, {})};
    CHECK(db.GetAllIntIds(collection).empty());

    auto collection_path {*recovery_logs.Dir() / "test_collection"};
    REQUIRE(std::filesystem::remove(collection_path));
    CHECK_THROWS_AS(db.GetAllIntIds(collection), DataBaseException);
}

TEST_CASE("DataBaseJsonGetAllStringIdsDecodesStoredKeys")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"json-string-ids"};
    hstring collection {hashes.to_hashed_string("test_string_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::String}}};
    string connection_info {strex("JSON {}", fs::path_to_string(*recovery_logs.Dir())).str()};
    DataBaseKey record_id {string("steam% user/Привет")};

    auto db {ConnectToDataBase(&settings, connection_info, collection_schemas, {})};

    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.StartCommitChanges();
    db.WaitCommitChanges();

    auto all_ids {db.GetAllStringIds(collection)};
    REQUIRE(all_ids.size() == 1);
    CHECK(all_ids.front() == std::get<string>(record_id));
    CHECK(db.Valid(collection, record_id));
}
#endif

TEST_CASE("DataBaseTypedGetAllIdsRejectCollectionTypeMismatch")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    hstring int_collection {hashes.to_hashed_string("test_collection")};
    hstring string_collection {hashes.to_hashed_string("test_string_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {
        {int_collection, DataBaseKeyType::IntId},
        {string_collection, DataBaseKeyType::String},
    }};

    auto db {ConnectToDataBase(&settings, "Memory", collection_schemas, {})};

    REQUIRE_THROWS_AS(db.GetAllIntIds(string_collection), DataBaseException);
    REQUIRE_THROWS_AS(db.GetAllStringIds(int_collection), DataBaseException);
}

TEST_CASE("DataBaseGetAllIdsRejectsBackendKeyTypeMismatch")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring int_collection {hashes.to_hashed_string("test_collection")};
    hstring string_collection {hashes.to_hashed_string("test_string_collection")};

    db.PrimeRecord(int_collection, DataBaseKey {string("wrong-type")}, MakeDoc({{"value", 1}}));
    db.PrimeRecord(string_collection, DataBaseKey {ident_t {1001}}, MakeDoc({{"value", 1}}));

    REQUIRE_THROWS_AS(db.GetAllIds(int_collection), DataBaseException);
    REQUIRE_THROWS_AS(db.GetAllIds(string_collection), DataBaseException);
}

TEST_CASE("DataBaseGetAllIdsRejectsInvalidUtf8BackendStringKey")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings, DataBaseStringKeyEscaping::Raw};
    hstring string_collection {hashes.to_hashed_string("test_string_collection")};
    string invalid_key {string(1, static_cast<char>(0xC3))};

    db.PrimeRecord(string_collection, DataBaseKey {invalid_key}, MakeDoc({{"value", 1}}));

    REQUIRE_THROWS_AS(db.GetAllIds(string_collection), DataBaseException);
}

#if FO_DATABASE_JSON
TEST_CASE("DataBaseJsonGetAllStringIdsRejectsInvalidEscapedKey")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"json-invalid-escaped-string-id"};
    hstring collection {hashes.to_hashed_string("test_string_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::String}}};
    string storage_root {fs::path_to_string(*recovery_logs.Dir())};
    string collection_dir {strex("{}/{}", storage_root, collection).str()};
    string bad_doc_path {strex("{}/bad%zz.json", collection_dir).str()};
    string connection_info {strex("JSON {}", storage_root).str()};

    REQUIRE(fs::create_directories(collection_dir));
    REQUIRE(fs::write_file(bad_doc_path, "{\"value\":1}"));

    auto db {ConnectToDataBase(&settings, connection_info, collection_schemas, {})};

    REQUIRE_THROWS_AS(db.GetAllStringIds(collection), DataBaseException);
}
#endif

TEST_CASE("DataBaseRejectsInvalidUtf8StringKeys")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings};
    hstring collection {hashes.to_hashed_string("test_string_collection")};
    DataBaseKey invalid_record_id {string(1, static_cast<char>(0xC3))};

    REQUIRE_THROWS_AS(db.Insert(collection, invalid_record_id, MakeDoc({{"value", 1}})), DataBaseException);
}

TEST_CASE("DataBaseRelaxedStringKeysKeepRawBackendIds")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings, DataBaseStringKeyEscaping::Raw};
    hstring collection {hashes.to_hashed_string("test_string_collection")};
    DataBaseKey record_id {string("steam% user\n123")};

    db.StartCommitChanges();
    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.WaitCommitChanges();

    auto stored_ids {db.GetAllRecordIds(collection)};
    REQUIRE(stored_ids.size() == 1);
    CHECK(stored_ids.front() == record_id);

    auto doc {db.GetDocument(collection, record_id)};
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseFileStringKeysEncodeBackendIds")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings, DataBaseStringKeyEscaping::File};
    hstring collection {hashes.to_hashed_string("test_string_collection")};
    DataBaseKey record_id {string("steam user/123")};

    db.StartCommitChanges();
    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.WaitCommitChanges();

    auto stored_ids {db.GetAllRecordIds(collection)};
    REQUIRE(stored_ids.size() == 1);
    REQUIRE(std::holds_alternative<string>(stored_ids.front()));
    CHECK(std::get<string>(stored_ids.front()) == "steam%20user%2f123");

    auto doc {db.GetDocument(collection, record_id)};
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseHexStringKeysEncodeBackendIds")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    TestDataBase db {settings, DataBaseStringKeyEscaping::Hex};
    hstring collection {hashes.to_hashed_string("test_string_collection")};
    DataBaseKey record_id {string("steam:user-123")};

    db.StartCommitChanges();
    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.WaitCommitChanges();

    auto stored_ids {db.GetAllRecordIds(collection)};
    REQUIRE(stored_ids.size() == 1);
    REQUIRE(std::holds_alternative<string>(stored_ids.front()));
    CHECK(std::get<string>(stored_ids.front()) == "s_737465616d3a757365722d313233");

    auto doc {db.GetDocument(collection, record_id)};
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
}

TEST_CASE("DataBaseReconnectRestoresPendingChangesFromOplog")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"reconnect-restore"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t record_id {ident_t {1001}};

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

        auto doc {db.GetDocument(collection, record_id)};
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReconnectRestoresComplexDocumentFromOplog")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"reconnect-restore-complex-doc"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_collection")};
    ident_t record_id {ident_t {1001}};

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
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"reconnect-restore-string-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_string_collection")};
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

        auto doc {db.GetDocument(collection, record_id)};
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReconnectRestoresRelaxedStringKeysFromOplog")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"reconnect-restore-relaxed-string-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_string_collection")};
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

        auto stored_ids {db.GetAllRecordIds(collection)};
        REQUIRE(stored_ids.size() == 1);
        CHECK(stored_ids.front() == record_id);

        auto doc {db.GetDocument(collection, record_id)};
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReconnectRestoresFileStringKeysFromOplog")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"reconnect-restore-file-string-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_string_collection")};
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

        auto stored_ids {db.GetAllRecordIds(collection)};
        REQUIRE(stored_ids.size() == 1);
        REQUIRE(std::holds_alternative<string>(stored_ids.front()));
        CHECK(std::get<string>(stored_ids.front()) == "steam%25%20user%2f123");

        auto doc {db.GetDocument(collection, record_id)};
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("DataBaseReconnectRestoresHexStringKeysFromOplog")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"reconnect-restore-hex-string-key"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    hstring collection {hashes.to_hashed_string("test_string_collection")};
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

        auto stored_ids {db.GetAllRecordIds(collection)};
        REQUIRE(stored_ids.size() == 1);
        REQUIRE(std::holds_alternative<string>(stored_ids.front()));
        CHECK(std::get<string>(stored_ids.front()) == "s_737465616d253a757365722d313233");

        auto doc {db.GetDocument(collection, record_id)};
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

#if FO_DATABASE_JSON
TEST_CASE("JsonDataBaseStorageFollowsTheWritableRoot")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs writable_root_scope {"json-writable-root"};
    string writable_root {fs::path_to_string(*writable_root_scope.Dir())};
    hstring collection {hashes.to_hashed_string("test_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
    ident_t record_id {ident_t {2001}};

    // A relative storage directory is where a server writes its world, so it belongs under the root the
    // process was told it may write to rather than wherever it happens to have been started
    settings.ApplyWritableRoot(writable_root);

    auto db {ConnectToDataBase(&settings, "JSON WorldData", collection_schemas, {})};

    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.StartCommitChanges();
    db.WaitCommitChanges();

    CHECK(fs::exists(fs::path_to_string(*writable_root_scope.Dir() / "WorldData" / "test_collection" / "2001.json")));
    CHECK_FALSE(fs::exists("WorldData"));
}
#endif

#if FO_DATABASE_JSON
TEST_CASE("JsonDataBaseAcceptsStoragePathWithSpaces")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs storage_dir_scope {"json path with spaces"};
    string storage_dir {fs::path_to_string(*storage_dir_scope.Dir() / "player session")};
    hstring collection {hashes.to_hashed_string("test_collection")};
    hstring string_collection {hashes.to_hashed_string("test_strings")};
    auto collection_schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}, {string_collection, DataBaseKeyType::String}}};
    string connection_info {strex("  JSON  {}  ", storage_dir).str()};
    ident_t record_id {ident_t {1001}};

    {
        auto db {ConnectToDataBase(&settings, connection_info, collection_schemas, {})};
        CHECK(db.TryInsert(collection, record_id, MakeDoc({{"value", 42}})));
        CHECK_FALSE(db.TryInsert(collection, record_id, MakeDoc({{"value", 99}})));
        CHECK(db.CompareExchange(collection, record_id, "value", AnyData::Value {int64_t {42}}, AnyData::Value {int64_t {43}}));
        CHECK_FALSE(db.CompareExchange(collection, record_id, "value", AnyData::Value {int64_t {42}}, AnyData::Value {int64_t {99}}));
        CHECK(db.TryInsert(string_collection, string {"key with spaces"}, MakeDoc({{"value", 5}})));
        CHECK(db.CompareExchange(string_collection, string {"key with spaces"}, "value", AnyData::Value {int64_t {5}}, AnyData::Value {int64_t {6}}));
        CHECK(db.Get(collection, record_id)["value"].AsInt64() == 43);
        db.StartCommitChanges();
        db.WaitCommitChanges();
        CHECK(db.Valid(collection, record_id));
    }

    auto reopened {ConnectToDataBase(&settings, connection_info, collection_schemas, {})};
    REQUIRE(reopened.Valid(collection, record_id));
    CHECK(reopened.Get(collection, record_id)["value"].AsInt64() == 43);
    CHECK(reopened.Get(string_collection, string {"key with spaces"})["value"].AsInt64() == 6);
}

TEST_CASE("JsonDataBaseRoundTripsDocumentsAndIds")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs storage_dir_scope {"json-roundtrip"};
    string storage_dir {fs::path_to_string(*storage_dir_scope.Dir() / "storage")};
    hstring collection {hashes.to_hashed_string("test_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
    ident_t first_id {ident_t {1001}};
    ident_t second_id {ident_t {1002}};
    ident_t complex_id {ident_t {1003}};
    *FixedSettingForOverride(settings.DataBase.JsonIndent) = 2;
    auto db {ConnectToDataBase(&settings, strex("JSON {}", storage_dir).str(), collection_schemas, {})};

    db.Insert(collection, first_id, MakeDoc({{"value", 1}, {"other", 7}}));
    db.Insert(collection, second_id, MakeDoc({{"value", 2}}));
    db.Insert(collection, complex_id, MakeComplexDoc());
    db.StartCommitChanges();
    db.WaitCommitChanges();

    auto ids {db.GetAllIntIds(collection)};
    std::sort(ids.begin(), ids.end());

    REQUIRE(ids.size() == 3);
    CHECK(ids[0] == first_id);
    CHECK(ids[1] == second_id);
    CHECK(ids[2] == complex_id);

    auto first_doc {db.Get(collection, first_id)};
    REQUIRE(!first_doc.Empty());
    CHECK(first_doc["value"].AsInt64() == 1);
    CHECK(first_doc["other"].AsInt64() == 7);
    CheckComplexDoc(db.Get(collection, complex_id));

    auto json_content {fs::read_file(fs::path_to_string(*storage_dir_scope.Dir() / "storage" / "test_collection" / "1001.json"))};
    REQUIRE(json_content.has_value());
    CHECK(json_content->find("\n  \"value\"") != string::npos);

    db.Update(collection, first_id, "value", numeric_cast<int64_t>(3));
    db.Delete(collection, second_id);
    db.WaitCommitChanges();

    auto updated_doc {db.Get(collection, first_id)};
    REQUIRE(!updated_doc.Empty());
    CHECK(updated_doc["value"].AsInt64() == 3);
    CHECK(updated_doc["other"].AsInt64() == 7);
    CHECK_FALSE(db.Valid(collection, second_id));

    ids = db.GetAllIntIds(collection);
    REQUIRE(ids.size() == 2);
    CHECK(std::ranges::find(ids, first_id) != ids.end());
    CHECK(std::ranges::find(ids, complex_id) != ids.end());
    CHECK_FALSE(fs::exists(fs::path_to_string(*storage_dir_scope.Dir() / "storage" / "test_collection" / "1002.json")));
}
#endif

#if FO_DATABASE_JSON
TEST_CASE("JsonDataBaseRejectsBrokenStorageFiles")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs storage_dir_scope {"json-errors"};
    auto storage_dir {*storage_dir_scope.Dir() / "storage"};
    auto collection_dir {storage_dir / "test_collection"};
    hstring collection {hashes.to_hashed_string("test_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
    std::filesystem::create_directories(collection_dir);

    auto db {ConnectToDataBase(&settings, strex("JSON {}", fs::path_to_string(storage_dir)).str(), collection_schemas, {})};

    REQUIRE(fs::write_file(fs::path_to_string(collection_dir / "0.json"), "{}"));
    REQUIRE_THROWS_AS(db.GetAllIds(collection), DataBaseException);

    REQUIRE(std::filesystem::remove(collection_dir / "0.json"));
    REQUIRE(fs::write_file(fs::path_to_string(collection_dir / "1001.json"), "{"));
    REQUIRE_THROWS_AS(db.Get(collection, ident_t {1001}), DataBaseException);
}
#endif

TEST_CASE("MemoryDataBaseRoundTripsDocumentsAndIds")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    hstring collection {hashes.to_hashed_string("test_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
    ident_t first_id {ident_t {1001}};
    ident_t second_id {ident_t {1002}};
    auto db {ConnectToDataBase(&settings, "Memory", collection_schemas, {})};

    CHECK(db.InValidState());
    CHECK(db.GetAllIntIds(collection).empty());
    CHECK_FALSE(db.Valid(collection, first_id));
    CHECK(db.Get(collection, first_id).Empty());

    db.StartCommitChanges();
    db.Insert(collection, first_id, MakeDoc({{"value", 1}, {"other", 7}}));
    db.Insert(collection, second_id, MakeDoc({{"value", 2}}));
    db.WaitCommitChanges();

    auto ids {db.GetAllIntIds(collection)};
    std::sort(ids.begin(), ids.end());

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == first_id);
    CHECK(ids[1] == second_id);

    auto doc {db.Get(collection, first_id)};
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

TEST_CASE("MemoryDataBaseBatchReadMatchesSingleReads")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    hstring int_collection {hashes.to_hashed_string("test_collection")};
    hstring string_collection {hashes.to_hashed_string("test_string_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{int_collection, DataBaseKeyType::IntId}, {string_collection, DataBaseKeyType::String}}};
    auto db {ConnectToDataBase(&settings, "Memory", collection_schemas, {})};

    db.StartCommitChanges();
    CheckBatchReadMatchesSingleReads(db, int_collection, string_collection, 1100);
}

#if FO_DATABASE_JSON
TEST_CASE("JsonDataBaseBatchReadMatchesSingleReads")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs storage_dir_scope {"json-batch-read"};
    string storage_dir {fs::path_to_string(*storage_dir_scope.Dir() / "storage")};
    hstring int_collection {hashes.to_hashed_string("test_collection")};
    hstring string_collection {hashes.to_hashed_string("test_string_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{int_collection, DataBaseKeyType::IntId}, {string_collection, DataBaseKeyType::String}}};
    auto db {ConnectToDataBase(&settings, strex("JSON {}", storage_dir).str(), collection_schemas, {})};

    db.StartCommitChanges();
    CheckBatchReadMatchesSingleReads(db, int_collection, string_collection, 30);
}
#endif

#if FO_DATABASE_JSON
TEST_CASE("DataBaseConnectionValidationAndMetrics")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    hstring collection {hashes.to_hashed_string("test_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
    ident_t record_id {ident_t {1001}};

    REQUIRE_THROWS_AS(ConnectToDataBase(&settings, "Unknown", collection_schemas, {}), DataBaseException);
    REQUIRE_THROWS_AS(ConnectToDataBase(&settings, "JSON", collection_schemas, {}), DataBaseException);
    REQUIRE_THROWS_AS(ConnectToDataBase(&settings, "Memory extra", collection_schemas, {}), DataBaseException);

    auto db {ConnectToDataBase(&settings, "Memory", collection_schemas, {})};
    db.StartCommitChanges();
    db.Insert(collection, record_id, MakeDoc({{"value", 1}}));
    db.WaitCommitChanges();

    auto doc {db.Get(collection, record_id)};
    REQUIRE(!doc.Empty());
    CHECK(doc["value"].AsInt64() == 1);
    CHECK(db.GetDbRequestsPerMinute() >= 1);
    // A backend without a byte representation refuses in both directions instead of returning nothing
    CHECK_THROWS_AS((void)db.CreateSnapshot(), DataBaseException);
    CHECK_THROWS_AS(db.RestoreSnapshot(vector<uint8_t> {1, 2, 3}), DataBaseException);
    CHECK_THROWS_AS(db.RestoreSnapshot(vector<uint8_t> {}), DataBaseException);
}
#endif

#if FO_DATABASE_SQLITE
TEST_CASE("SQLiteDataBaseAcceptsStoragePathWithSpaces")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs storage_dir_scope {"sqlite path with spaces"};
    string storage_dir {fs::path_to_string(*storage_dir_scope.Dir() / "player session")};
    hstring collection {hashes.to_hashed_string("test_collection")};
    hstring string_collection {hashes.to_hashed_string("test_strings")};
    auto collection_schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}, {string_collection, DataBaseKeyType::String}}};
    string connection_info {strex("  DbSQLite  {}  ", storage_dir).str()};
    ident_t record_id {ident_t {1001}};

    {
        auto db {ConnectToDataBase(&settings, connection_info, collection_schemas, {})};
        CHECK(db.TryInsert(collection, record_id, MakeDoc({{"value", 42}})));
        CHECK_FALSE(db.TryInsert(collection, record_id, MakeDoc({{"value", 99}})));
        CHECK(db.CompareExchange(collection, record_id, "value", AnyData::Value {int64_t {42}}, AnyData::Value {int64_t {43}}));
        CHECK_FALSE(db.CompareExchange(collection, record_id, "value", AnyData::Value {int64_t {42}}, AnyData::Value {int64_t {99}}));
        CHECK(db.TryInsert(string_collection, string {"key with spaces"}, MakeDoc({{"value", 5}})));
        CHECK(db.CompareExchange(string_collection, string {"key with spaces"}, "value", AnyData::Value {int64_t {5}}, AnyData::Value {int64_t {6}}));
        CHECK(db.Get(collection, record_id)["value"].AsInt64() == 43);
        db.StartCommitChanges();
        db.WaitCommitChanges();
        CHECK(db.Valid(collection, record_id));
    }

    auto reopened {ConnectToDataBase(&settings, connection_info, collection_schemas, {})};
    REQUIRE(reopened.Valid(collection, record_id));
    CHECK(reopened.Get(collection, record_id)["value"].AsInt64() == 43);
    CHECK(reopened.Get(string_collection, string {"key with spaces"})["value"].AsInt64() == 6);
}

TEST_CASE("SQLiteDataBaseListingPreservesBackendFailure")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs storage_dir_scope {"sqlite-listing-failure"};
    string storage_dir {fs::path_to_string(*storage_dir_scope.Dir() / "storage")};
    hstring collection {hashes.to_hashed_string("test_collection")};
    auto schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
    auto db {ConnectToDataBase(&settings, strex("DbSQLite {}", storage_dir).str(), schemas, {})};
    db.StartCommitChanges();
    CHECK(db.GetAllIntIds(collection).empty());

    string sqlite_path {fs::path_to_string(*storage_dir_scope.Dir() / "storage" / "Storage.sqlite")};
    sqlite3* external {nullptr};
    auto open_result {sqlite3_open_v2(sqlite_path.c_str(), &external, SQLITE_OPEN_READWRITE, nullptr)};
    auto close_external {scope_exit([&]() noexcept { (void)sqlite3_close(external); })};
    REQUIRE(open_result == SQLITE_OK);
    REQUIRE(sqlite3_exec(external, "DROP TABLE test_collection", nullptr, nullptr, nullptr) == SQLITE_OK);

    bool preserved_backend_error {false};
    string backend_error {};

    try {
        (void)db.GetAllIntIds(collection);
    }
    catch (const DataBaseException& ex) {
        backend_error = ex.what();
        auto message {string_view {backend_error}};
        bool sqlite_statement_error {message.find("DbSQLite sqlite3_prepare_v2") != string_view::npos || message.find("DbSQLite sqlite3_step") != string_view::npos};
        preserved_backend_error = sqlite_statement_error && message.find("no such table") != string_view::npos;
    }

    INFO(backend_error);
    CHECK(preserved_backend_error);
}

TEST_CASE("SQLiteDataBaseRoundTripsDocumentsAndIds")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs storage_dir_scope {"sqlite-roundtrip"};
    string storage_dir {fs::path_to_string(*storage_dir_scope.Dir() / "storage")};
    hstring collection {hashes.to_hashed_string("test_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
    ident_t first_id {ident_t {1001}};
    ident_t second_id {ident_t {1002}};
    auto db {ConnectToDataBase(&settings, strex("DbSQLite {}", storage_dir).str(), collection_schemas, {})};

    CHECK(db.InValidState());
    CHECK(db.GetAllIntIds(collection).empty());
    CHECK_FALSE(db.Valid(collection, first_id));
    CHECK(db.Get(collection, first_id).Empty());

    db.StartCommitChanges();
    db.Insert(collection, first_id, MakeDoc({{"value", 1}, {"other", 7}}));
    db.Insert(collection, second_id, MakeDoc({{"value", 2}}));
    db.WaitCommitChanges();

    auto ids {db.GetAllIntIds(collection)};
    std::sort(ids.begin(), ids.end());

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == first_id);
    CHECK(ids[1] == second_id);

    auto doc {db.Get(collection, first_id)};
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
    CHECK(fs::exists(fs::path_to_string(*storage_dir_scope.Dir() / "storage" / "Storage.sqlite")));
}

TEST_CASE("SQLiteDataBasePersistsDocumentsAcrossReconnects")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs storage_dir_scope {"sqlite-reconnect"};
    string storage_dir {fs::path_to_string(*storage_dir_scope.Dir() / "storage")};
    hstring int_collection {hashes.to_hashed_string("test_collection")};
    hstring string_collection {hashes.to_hashed_string("test_string_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {
        {int_collection, DataBaseKeyType::IntId},
        {string_collection, DataBaseKeyType::String},
    }};
    ident_t int_id {ident_t {1001}};
    DataBaseKey string_id {string("steam:user/Привет")};
    string connection_info {strex("DbSQLite {}", storage_dir).str()};

    {
        auto db {ConnectToDataBase(&settings, connection_info, collection_schemas, {})};

        db.StartCommitChanges();
        db.Insert(int_collection, int_id, MakeDoc({{"value", 1}}));
        db.Insert(string_collection, string_id, MakeDoc({{"value", 2}}));
        db.WaitCommitChanges();

        db.Update(int_collection, int_id, "patched", numeric_cast<int64_t>(7));
        db.WaitCommitChanges();
    }

    {
        auto db {ConnectToDataBase(&settings, connection_info, collection_schemas, {})};

        auto int_ids {db.GetAllIntIds(int_collection)};
        REQUIRE(int_ids.size() == 1);
        CHECK(int_ids.front() == int_id);

        auto string_ids {db.GetAllStringIds(string_collection)};
        REQUIRE(string_ids.size() == 1);
        CHECK(string_ids.front() == std::get<string>(string_id));

        auto doc {db.Get(int_collection, int_id)};
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
        auto db {ConnectToDataBase(&settings, connection_info, collection_schemas, {})};

        CHECK_FALSE(db.Valid(int_collection, int_id));
        CHECK(db.GetAllIntIds(int_collection).empty());

        auto string_ids {db.GetAllStringIds(string_collection)};
        REQUIRE(string_ids.size() == 1);
        CHECK(string_ids.front() == std::get<string>(string_id));
    }
}

TEST_CASE("SQLiteDataBaseSnapshotRoundTripsThroughBytes")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs storage_dir_scope {"sqlite-snapshot"};
    string source_storage_dir {fs::path_to_string(*storage_dir_scope.Dir() / "source")};
    hstring collection {hashes.to_hashed_string("test_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{collection, DataBaseKeyType::IntId}}};
    ident_t record_id {ident_t {1001}};
    string source_connection {strex("DbSQLite {}", source_storage_dir).str()};
    vector<uint8_t> snapshot_data {};

    {
        auto source {ConnectToDataBase(&settings, source_connection, collection_schemas, {})};
        source.StartCommitChanges();
        source.Insert(collection, record_id, MakeDoc({{"value", 1}}));

        // CreateSnapshot drains the commit queue itself and holds new producers until the backend copy ends.
        // The result is plain bytes: the database never names a file or touches a directory
        snapshot_data = source.CreateSnapshot();
        CHECK_FALSE(snapshot_data.empty());

        // A SQLite serialization starts with the file format header, so the bytes are a real page image
        REQUIRE(snapshot_data.size() > 16);
        CHECK(string_view {reinterpret_cast<const char*>(snapshot_data.data()), 15} == string_view {"SQLite format 3"});

        source.Update(collection, record_id, "value", numeric_cast<int64_t>(2));
        source.WaitCommitChanges();

        auto changed_doc {source.Get(collection, record_id)};
        REQUIRE(!changed_doc.Empty());
        CHECK(changed_doc["value"].AsInt64() == 2);

        // Restoring the captured bytes puts the live storage back to the captured content
        source.RestoreSnapshot(snapshot_data);

        auto restored_doc {source.Get(collection, record_id)};
        REQUIRE(!restored_doc.Empty());
        CHECK(restored_doc["value"].AsInt64() == 1);

        CHECK_THROWS_AS(source.RestoreSnapshot(vector<uint8_t> {}), DataBaseException);
        CHECK_THROWS_AS(source.RestoreSnapshot(vector<uint8_t> {1, 2, 3, 4, 5, 6, 7, 8}), DataBaseException);
    }

    {
        // The restore survives a reconnect, so it reached the storage file rather than a memory copy
        auto source {ConnectToDataBase(&settings, source_connection, collection_schemas, {})};
        auto doc {source.Get(collection, record_id)};
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }

    {
        // The same bytes load into an unrelated storage
        string other_storage_dir {fs::path_to_string(*storage_dir_scope.Dir() / "other")};
        auto other {ConnectToDataBase(&settings, strex("DbSQLite {}", other_storage_dir).str(), collection_schemas, {})};
        other.RestoreSnapshot(snapshot_data);

        auto doc {other.Get(collection, record_id)};
        REQUIRE(!doc.Empty());
        CHECK(doc["value"].AsInt64() == 1);
    }
}

TEST_CASE("SQLiteDataBaseRejectsCorruptedStoredKeys")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs storage_dir_scope {"sqlite-corrupted-keys"};
    string storage_dir {fs::path_to_string(*storage_dir_scope.Dir() / "storage")};
    hstring int_collection {hashes.to_hashed_string("test_collection")};
    hstring string_collection {hashes.to_hashed_string("test_string_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {
        {int_collection, DataBaseKeyType::IntId},
        {string_collection, DataBaseKeyType::String},
    }};

    SECTION("Invalid numeric key size")
    {
        StoreRawSQLiteRecord(*storage_dir_scope.Dir() / "storage", "test_collection", vector<uint8_t> {1, 2, 3}, "payload");

        auto db {ConnectToDataBase(&settings, strex("DbSQLite {}", storage_dir).str(), collection_schemas, {})};

        REQUIRE_THROWS_AS(db.GetAllIds(int_collection), DataBaseException);
    }

    SECTION("Invalid numeric key value")
    {
        vector<uint8_t> zero_key {vector<uint8_t>(sizeof(int64_t))};
        StoreRawSQLiteRecord(*storage_dir_scope.Dir() / "storage", "test_collection", zero_key, "payload");

        auto db {ConnectToDataBase(&settings, strex("DbSQLite {}", storage_dir).str(), collection_schemas, {})};

        REQUIRE_THROWS_AS(db.GetAllIds(int_collection), DataBaseException);
    }

    SECTION("Invalid encoded string key")
    {
        StoreRawSQLiteRecord(*storage_dir_scope.Dir() / "storage", "test_string_collection", vector<uint8_t> {'b', 'a', 'd'}, "payload");

        auto db {ConnectToDataBase(&settings, strex("DbSQLite {}", storage_dir).str(), collection_schemas, {})};

        REQUIRE_THROWS_AS(db.GetAllStringIds(string_collection), DataBaseException);
    }
}

// More records than one statement binds, so the batch is split and still reads as a single request
TEST_CASE("SQLiteDataBaseBatchReadMatchesSingleReads")
{
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs storage_dir_scope {"sqlite-batch-read"};
    string storage_dir {fs::path_to_string(*storage_dir_scope.Dir() / "storage")};
    hstring int_collection {hashes.to_hashed_string("test_collection")};
    hstring string_collection {hashes.to_hashed_string("test_string_collection")};
    auto collection_schemas {DataBaseCollectionSchemas {{int_collection, DataBaseKeyType::IntId}, {string_collection, DataBaseKeyType::String}}};
    auto db {ConnectToDataBase(&settings, strex("DbSQLite {}", storage_dir).str(), collection_schemas, {})};

    db.StartCommitChanges();
    CheckBatchReadMatchesSingleReads(db, int_collection, string_collection, 1100);
}
#endif

#if FO_DATABASE_MONGO
TEST_CASE("MongoDataBaseCanBeCreatedAgainAfterAFailedStart")
{
    // The driver's init and cleanup are one-shot each. A database that cleaned up after failing to start left
    // the next one in the process on freed handshake state, which is what a server stop and start does
    GlobalSettings settings {false};
    ptr<DataBaseSettings> db_settings {&settings};
    string_view unreachable_uri {"mongodb://127.0.0.1:1/?serverSelectionTimeoutMS=200&connectTimeoutMS=200"};

    for (int32_t attempt {0}; attempt < 3; attempt++) {
        CHECK_THROWS_AS(DataBaseMongo::Create(db_settings, unreachable_uri, "lf_unit_tests", [] { }), DataBaseException);
    }
}
#endif

TEST_CASE("DatabaseSerializationPreservesAnyDataTypesAndIntegerLimits")
{
    AnyData::Document doc {MakeComplexDoc()};
    doc.Emplace("minimum", std::numeric_limits<int64_t>::min());
    doc.Emplace("maximum", std::numeric_limits<int64_t>::max());
    doc.Emplace("precise", int64_t {9007199254740993});
    doc.Emplace(string {"nul\0key", 7}, string {"nul\0value", 9});

    nlohmann::json json {};
    json = DataBaseSerialization::ToJson(doc);
    CHECK(DataBaseSerialization::FromJson(json) == doc);
    auto text {json.dump()};
    CHECK(DataBaseSerialization::FromJsonText(string_view {text.data(), text.size()}) == doc);

#if FO_DATABASE_SQLITE
    auto cbor {DataBaseSerialization::ToCbor(doc)};
    CHECK(DataBaseSerialization::FromCbor(cbor) == doc);

    // A CBOR record is independent of BSON and must survive close/reopen byte-for-byte
    ScopedRecoveryLogs storage {"cbor-codec"};
    string record_path {fs::path_to_string(*storage.Dir() / "record.cbor")};
    REQUIRE(fs::write_file(record_path, cbor));
    auto stored {fs::read_file(record_path)};
    REQUIRE(stored);
    CHECK(DataBaseSerialization::FromCbor(make_span(stored->data(), stored->size())) == doc);
#endif
}

TEST_CASE("DatabaseSerializationRoundTripsEmptyContainersAtTheDepthLimit")
{
    AnyData::Value nested {AnyData::Array {}};

    SECTION("Arrays")
    {
        for (size_t depth {1}; depth < DataBaseSerialization::MAX_DOCUMENT_DEPTH; depth++) {
            AnyData::Array parent {};
            parent.EmplaceBack(std::move(nested));
            nested = AnyData::Value {std::move(parent)};
        }
    }
    SECTION("Dictionaries")
    {
        nested = AnyData::Value {AnyData::Dict {}};

        for (size_t depth {1}; depth < DataBaseSerialization::MAX_DOCUMENT_DEPTH; depth++) {
            AnyData::Dict parent {};
            parent.Emplace("child", std::move(nested));
            nested = AnyData::Value {std::move(parent)};
        }
    }

    AnyData::Document doc {};
    doc.Emplace("x", std::move(nested));
    nlohmann::json json {};
    json = DataBaseSerialization::ToJson(doc);
    CHECK(DataBaseSerialization::FromJson(json) == doc);
    auto text {json.dump()};
    CHECK(DataBaseSerialization::FromJsonText(string_view {text.data(), text.size()}) == doc);

#if FO_DATABASE_SQLITE
    CHECK(DataBaseSerialization::FromCbor(DataBaseSerialization::ToCbor(doc)) == doc);
#endif
}

TEST_CASE("DatabaseSerializationRejectsUnrepresentableAndCorruptDocuments")
{
    CHECK_THROWS(DataBaseSerialization::FromJsonText("[]"));
    CHECK_THROWS(DataBaseSerialization::FromJsonText("{\"x\":null}"));
    CHECK_THROWS(DataBaseSerialization::FromJsonText("{\"x\":18446744073709551615}"));
    CHECK_THROWS(DataBaseSerialization::FromJsonText("{\"x\":"));

    string deep {"{\"x\":"};
    deep.append(DataBaseSerialization::MAX_DOCUMENT_DEPTH + 1, '[');
    deep += "1";
    deep.append(DataBaseSerialization::MAX_DOCUMENT_DEPTH + 1, ']');
    deep += "}";
    CHECK_THROWS(DataBaseSerialization::FromJsonText(deep));

#if FO_DATABASE_SQLITE
    CHECK_THROWS(DataBaseSerialization::FromCbor(vector<uint8_t> {}));
    CHECK_THROWS(DataBaseSerialization::FromCbor(vector<uint8_t> {0xa1, 0x61, 'x'}));
    CHECK_THROWS(DataBaseSerialization::FromCbor(vector<uint8_t> {0xa1, 0x61, 'x', 0xf6}));
    CHECK_THROWS(DataBaseSerialization::FromCbor(vector<uint8_t> {0xa1, 0x61, 'x', 0x41, 0x00}));
    CHECK_THROWS(DataBaseSerialization::FromCbor(vector<uint8_t> {0xa1, 0x61, 'x', 0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}));
    CHECK_THROWS(DataBaseSerialization::FromCbor(vector<uint8_t> {0xa1, 0x61, 'x', 0x9b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe}));
    CHECK_THROWS(DataBaseSerialization::FromCbor(vector<uint8_t> {0xa0, 0x00}));

    vector<uint8_t> deep_cbor {0xa1, 0x61, 'x'};
    deep_cbor.insert(deep_cbor.end(), DataBaseSerialization::MAX_DOCUMENT_DEPTH + 1, 0x81);
    deep_cbor.emplace_back(1);
    CHECK_THROWS(DataBaseSerialization::FromCbor(deep_cbor));
#endif
}

#if !FO_DATABASE_JSON || !FO_DATABASE_SQLITE || !FO_DATABASE_MONGO
TEST_CASE("DisabledDatabaseProvidersIdentifyTheirRequiredOptions")
{
    GlobalSettings settings {false};
    DataBaseCollectionSchemas schemas {};

#if !FO_DATABASE_JSON
    CHECK_THROWS_WITH(ConnectToDataBase(&settings, "JSON WorldData", schemas, {}), Catch::Matchers::ContainsSubstring("FO_DATABASE_JSON"));
#endif
#if !FO_DATABASE_SQLITE
    CHECK_THROWS_WITH(ConnectToDataBase(&settings, "DbSQLite WorldData", schemas, {}), Catch::Matchers::ContainsSubstring("FO_DATABASE_SQLITE"));
#endif
#if !FO_DATABASE_MONGO
    CHECK_THROWS_WITH(ConnectToDataBase(&settings, "Mongo mongodb://127.0.0.1:1 test", schemas, {}), Catch::Matchers::ContainsSubstring("FO_DATABASE_MONGO"));
#endif
}
#endif

#if FO_DATABASE_MONGO
TEST_CASE("MongoRecoveryRejectsFindErrorWithoutClearingJournal", "[.MongoIntegration]")
{
    nptr<const char> uri_env {std::getenv("FO_TEST_MONGO_FAULT_URI")};

    if (!uri_env) {
        SKIP("Set FO_TEST_MONGO_FAULT_URI to an isolated Mongo-compatible fault server");
    }

    string uri {uri_env.get()};
    REQUIRE(strvex(uri).starts_with("mongodb://127.0.0.1:"));
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"mongo-recovery-find-error"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    int32_t operation {GENERATE(0, 1, 2)};
    CAPTURE(operation);
    string pending {operation == 0 ? "delete test_collection 1001\n" : operation == 1 ? "update test_collection 1001 {\"value\":1}\n" : "insert test_collection 1001 {\"value\":1}\n"};
    WriteRecoveryLogs(recovery_logs, pending);
    hstring collection {hashes.to_hashed_string("test_collection")};
    DataBaseCollectionSchemas schemas {{collection, DataBaseKeyType::IntId}};
    string connection {strex("Mongo {} {}", uri, "recovery_fault_test").str()};

    CHECK_THROWS_WITH(ConnectToDataBase(&settings, connection, schemas, {}), Catch::Matchers::ContainsSubstring("injected recovery find error"));
    auto persisted_pending {fs::read_file(recovery_logs.PendingPath())};
    REQUIRE(persisted_pending.has_value());
    CHECK(*persisted_pending == pending);
}

TEST_CASE("MongoRecoveryRejectsWriteErrorWithoutClearingJournal", "[.MongoIntegration]")
{
    nptr<const char> uri_env {std::getenv("FO_TEST_MONGO_FAULT_URI")};
    nptr<const char> op_env {std::getenv("FO_TEST_MONGO_WRITE_OP")};

    if (!uri_env || !op_env) {
        SKIP("Set FO_TEST_MONGO_FAULT_URI and FO_TEST_MONGO_WRITE_OP for the isolated fault server");
    }

    string uri {uri_env.get()};
    string operation {op_env.get()};
    REQUIRE(strvex(uri).starts_with("mongodb://127.0.0.1:"));
    REQUIRE((operation == "insert" || operation == "delete"));
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"mongo-recovery-write-error"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string pending {operation == "insert" ? "insert test_collection 1001 {\"value\":1}\n" : "delete test_collection 1001\n"};
    WriteRecoveryLogs(recovery_logs, pending);
    hstring collection {hashes.to_hashed_string("test_collection")};
    DataBaseCollectionSchemas schemas {{collection, DataBaseKeyType::IntId}};
    string connection {strex("Mongo {} {}", uri, "recovery_fault_test").str()};

    CHECK_THROWS_WITH(ConnectToDataBase(&settings, connection, schemas, {}), Catch::Matchers::ContainsSubstring("injected recovery write error"));
    auto persisted_pending {fs::read_file(recovery_logs.PendingPath())};
    REQUIRE(persisted_pending.has_value());
    CHECK(*persisted_pending == pending);
}

TEST_CASE("MongoRecoveryAcceptsMissingRecordForDelete", "[.MongoIntegration]")
{
    nptr<const char> uri_env {std::getenv("FO_TEST_MONGO_FAULT_URI")};

    if (!uri_env) {
        SKIP("Set FO_TEST_MONGO_FAULT_URI to an isolated Mongo-compatible server");
    }

    string uri {uri_env.get()};
    REQUIRE(strvex(uri).starts_with("mongodb://127.0.0.1:"));
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"mongo-recovery-missing"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "delete test_collection 1001\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    DataBaseCollectionSchemas schemas {{collection, DataBaseKeyType::IntId}};
    string connection {strex("Mongo {} {}", uri, "recovery_fault_test").str()};

    {
        auto db {ConnectToDataBase(&settings, connection, schemas, {})};
        CHECK(db.InValidState());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("MongoRecoveryAcceptsAlreadyAppliedUpdate", "[.MongoIntegration]")
{
    nptr<const char> uri_env {std::getenv("FO_TEST_MONGO_FAULT_URI")};

    if (!uri_env) {
        SKIP("Set FO_TEST_MONGO_FAULT_URI to an isolated Mongo-compatible server");
    }

    string uri {uri_env.get()};
    REQUIRE(strvex(uri).starts_with("mongodb://127.0.0.1:"));
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"mongo-recovery-present"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    DataBaseCollectionSchemas schemas {{collection, DataBaseKeyType::IntId}};
    string connection {strex("Mongo {} {}", uri, "recovery_fault_test").str()};

    {
        auto db {ConnectToDataBase(&settings, connection, schemas, {})};
        CHECK(db.InValidState());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("MongoRecoveryRejectsUnmatchedUpdateWithoutClearingJournal", "[.MongoIntegration]")
{
    nptr<const char> uri_env {std::getenv("FO_TEST_MONGO_FAULT_URI")};

    if (!uri_env) {
        SKIP("Set FO_TEST_MONGO_FAULT_URI to an isolated Mongo-compatible fault server");
    }

    string uri {uri_env.get()};
    REQUIRE(strvex(uri).starts_with("mongodb://127.0.0.1:"));
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"mongo-recovery-unmatched"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    string pending {"update test_collection 1001 {\"value\":1}\n"};
    WriteRecoveryLogs(recovery_logs, pending);
    hstring collection {hashes.to_hashed_string("test_collection")};
    DataBaseCollectionSchemas schemas {{collection, DataBaseKeyType::IntId}};
    string connection {strex("Mongo {} {}", uri, "recovery_fault_test").str()};

    CHECK_THROWS_WITH(ConnectToDataBase(&settings, connection, schemas, {}), Catch::Matchers::ContainsSubstring("Mongo update did not match a record"));
    auto persisted_pending {fs::read_file(recovery_logs.PendingPath())};
    REQUIRE(persisted_pending.has_value());
    CHECK(*persisted_pending == pending);
}

TEST_CASE("MongoRecoveryAcceptsMatchedUpdate", "[.MongoIntegration]")
{
    nptr<const char> uri_env {std::getenv("FO_TEST_MONGO_FAULT_URI")};

    if (!uri_env) {
        SKIP("Set FO_TEST_MONGO_FAULT_URI to an isolated Mongo-compatible server");
    }

    string uri {uri_env.get()};
    REQUIRE(strvex(uri).starts_with("mongodb://127.0.0.1:"));
    GlobalSettings settings {false};
    hash_storage hashes {};
    ScopedRecoveryLogs recovery_logs {"mongo-recovery-matched"};
    ScopedCurrentPath current_path {*recovery_logs.Dir()};
    ConfigureRecoverySettings(settings, recovery_logs.PendingPath());
    WriteRecoveryLogs(recovery_logs, "update test_collection 1001 {\"value\":1}\n");
    hstring collection {hashes.to_hashed_string("test_collection")};
    DataBaseCollectionSchemas schemas {{collection, DataBaseKeyType::IntId}};
    string connection {strex("Mongo {} {}", uri, "recovery_fault_test").str()};

    {
        auto db {ConnectToDataBase(&settings, connection, schemas, {})};
        CHECK(db.InValidState());
    }

    CheckRecoveryLogsCleared(recovery_logs);
}

TEST_CASE("MongoDataBaseRecoversAfterIsolatedServerRestart", "[.MongoIntegration]")
{
    nptr<const char> uri_env {std::getenv("FO_TEST_MONGO_URI")};
    nptr<const char> control_env {std::getenv("FO_TEST_MONGO_CONTROL")};

    if (!uri_env || !control_env) {
        SKIP("Set FO_TEST_MONGO_URI and FO_TEST_MONGO_CONTROL for an isolated local mongod");
    }

    string uri {uri_env.get()};
    REQUIRE(strvex(uri).starts_with("mongodb://127.0.0.1:"));
    string control_dir {control_env.get()};
    string pending_path {strex(control_dir).combine_path("pending.oplog").str()};
    string database_name {strex("fo_optional_components_{}", std::chrono::steady_clock::now().time_since_epoch().count()).str()};
    GlobalSettings settings {false};
    *FixedSettingForOverride(settings.DataBase.OpLogPath) = pending_path;
    *FixedSettingForOverride(settings.DataBase.OpLogEnabled) = true;
    *FixedSettingForOverride(settings.DataBase.ReconnectRetryPeriod) = 100;
    *FixedSettingForOverride(settings.DataBase.PanicOpLogSizeThreshold) = 1024 * 1024;
    *FixedSettingForOverride(settings.DataBase.PanicShutdownTimeout) = 60000;
    hash_storage hashes {};
    hstring collection {hashes.to_hashed_string("codec_records")};
    DataBaseCollectionSchemas schemas {{collection, DataBaseKeyType::IntId}};
    ident_t id {1};
    string connection {strex("Mongo {} {}", uri, database_name).str()};

    {
        auto db {ConnectToDataBase(&settings, connection, schemas, {})};
        db.StartCommitChanges();
        db.Insert(collection, id, MakeComplexDoc());
        db.WaitCommitChanges();
        CheckComplexDoc(db.Get(collection, id));
        REQUIRE(fs::write_file(strex(control_dir).combine_path("ready").str(), database_name));

        string stopped_path {strex(control_dir).combine_path("stopped").str()};
        auto stop_deadline {std::chrono::steady_clock::now() + std::chrono::seconds {30}};

        while (!fs::exists(stopped_path) && std::chrono::steady_clock::now() < stop_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds {20});
        }

        REQUIRE(fs::exists(stopped_path));
        db.Update(collection, id, "after_restart", int64_t {9007199254740993});
        auto spill_deadline {std::chrono::steady_clock::now() + std::chrono::seconds {30}};

        while (fs::file_size(pending_path).value_or(0) == 0 && std::chrono::steady_clock::now() < spill_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds {20});
        }

        REQUIRE(fs::file_size(pending_path).value_or(0) > 0);
        REQUIRE_FALSE(db.InValidState());
        REQUIRE(fs::write_file(strex(control_dir).combine_path("spilled").str(), database_name));
        string restarted_path {strex(control_dir).combine_path("restarted").str()};
        auto recovery_deadline {std::chrono::steady_clock::now() + std::chrono::seconds {30}};

        while ((!fs::exists(restarted_path) || !db.InValidState()) && std::chrono::steady_clock::now() < recovery_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds {20});
        }

        REQUIRE(fs::exists(restarted_path));
        REQUIRE(db.InValidState());
        db.WaitCommitChanges();
        auto recovered {db.Get(collection, id)};
        CHECK(recovered["after_restart"].AsInt64() == int64_t {9007199254740993});
        CheckComplexDoc(recovered);
    }

    auto db {ConnectToDataBase(&settings, connection, schemas, {})};
    CHECK(db.Get(collection, id)["after_restart"].AsInt64() == int64_t {9007199254740993});
    db.Delete(collection, id);
    db.WaitCommitChanges();
    CHECK_FALSE(db.Valid(collection, id));
    REQUIRE(fs::write_file(strex(control_dir).combine_path("complete").str(), database_name));
}
#endif

FO_END_NAMESPACE
