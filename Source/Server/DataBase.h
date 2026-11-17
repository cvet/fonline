// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "AnyData.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(DataBaseException);

enum class DataBaseKeyType
{
    IntId,
    String,
};

enum class DataBaseStringKeyEscaping
{
    Raw,
    File,
    Hex,
};

using DataBasePanicCallback = function<void()>;
using DataBaseKey = variant<ident_t, string>;
using DataBaseCollection = unordered_map<DataBaseKey, AnyData::Document>;
using DataBaseCollections = unordered_map<hstring, DataBaseCollection>;
using DataBaseCollectionSchema = pair<hstring, DataBaseKeyType>;
using DataBaseCollectionSchemas = vector<DataBaseCollectionSchema>;

FO_DECLARE_TYPE_FORMATTER_EXT(FO_NAMESPACE DataBaseKey,
    std::visit(
        [](const auto& raw_value) -> FO_NAMESPACE string {
            using T = std::decay_t<decltype(raw_value)>;
            if constexpr (std::is_same_v<T, FO_NAMESPACE ident_t>) {
                return FO_NAMESPACE strex("{}", raw_value).str();
            }
            else {
                return raw_value;
            }
        },
        value));

FO_DECLARE_TYPE_HASHER_EXT2(FO_NAMESPACE DataBaseKey, std::visit([](const auto& value) -> uint64_t { return FO_NAMESPACE hashing::hash<std::decay_t<decltype(value)>> {}(value); }, v));

class DataBaseImpl;

class DataBase
{
    friend auto ConnectToDataBase(ptr<DataBaseSettings> db_settings, string_view connection_info, const DataBaseCollectionSchemas& collection_schemas, DataBasePanicCallback panic_callback) -> DataBase;

public:
    using Collection = DataBaseCollection;
    using Collections = DataBaseCollections;

    DataBase();
    DataBase(const DataBase&) = delete;
    DataBase(DataBase&&) noexcept;
    auto operator=(const DataBase&) = delete;
    auto operator=(DataBase&&) noexcept -> DataBase&;
    ~DataBase();

    [[nodiscard]] auto InValidState() const noexcept -> bool;
    [[nodiscard]] auto GetDbRequestsPerMinute() const -> size_t;
    [[nodiscard]] auto GetAllIds(hstring collection_name) const -> vector<DataBaseKey>;
    [[nodiscard]] auto GetAllIntIds(hstring collection_name) const -> vector<ident_t>;
    [[nodiscard]] auto GetAllStringIds(hstring collection_name) const -> vector<string>;
    [[nodiscard]] auto Get(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document;
    [[nodiscard]] auto GetMany(hstring collection_name, const vector<DataBaseKey>& ids) const -> vector<AnyData::Document>;
    [[nodiscard]] auto Valid(hstring collection_name, const DataBaseKey& id) const -> bool;

    void Insert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc);
    auto TryInsert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) -> bool;
    auto CompareExchange(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& expected, const AnyData::Value& desired) -> bool;
    void Update(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& value);
    void UpdateFields(hstring collection_name, const DataBaseKey& id, const AnyData::Document& changes);
    void Delete(hstring collection_name, const DataBaseKey& id);
    void StartCommitChanges();
    void WaitCommitChanges();
    auto CreateSnapshot() -> vector<uint8_t>;
    void RestoreSnapshot(const_span<uint8_t> snapshot_data);
    void ClearChanges() noexcept;
    void DrawGui();

private:
    explicit DataBase(unique_ptr<DataBaseImpl> impl);

    unique_nptr<DataBaseImpl> _impl {};
};

auto ConnectToDataBase(ptr<DataBaseSettings> db_settings, string_view connection_info, const DataBaseCollectionSchemas& collection_schemas, DataBasePanicCallback panic_callback) -> DataBase;

class DataBaseImpl
{
public:
    class RecoveryLogHandle
    {
    public:
        using FdType = int;
        enum class ReplayAppendResult
        {
            Appended,
            RevisionChanged,
            Failed,
        };
        enum class ConditionalAppendResult
        {
            NoPending,
            Appended,
            Failed,
        };

        explicit RecoveryLogHandle(string path);
        RecoveryLogHandle(const RecoveryLogHandle&) = delete;
        RecoveryLogHandle(RecoveryLogHandle&&) noexcept = delete;
        auto operator=(const RecoveryLogHandle&) = delete;
        auto operator=(RecoveryLogHandle&&) noexcept = delete;
        ~RecoveryLogHandle() noexcept;

        [[nodiscard]] auto GetPath() const noexcept -> string_view { return _path; }
        [[nodiscard]] auto IsUsable() const noexcept -> bool;
        [[nodiscard]] auto IsUsableAtRevision(uint64_t revision) const noexcept -> bool;
        [[nodiscard]] auto HasTruncatedSince(uint64_t revision) const noexcept -> bool;
        [[nodiscard]] auto GetLinesCount() const noexcept -> size_t;
        [[nodiscard]] auto GetTextSize() const noexcept -> size_t;
        [[nodiscard]] auto SnapshotContent() const -> vector<string>;
        [[nodiscard]] auto SnapshotContentWithRevision() const -> pair<vector<string>, uint64_t>;

        auto Truncate() noexcept -> bool;
        auto TruncateIfRevision(uint64_t expected_revision) noexcept -> bool;
        auto Append(string_view text) noexcept -> bool;
        auto AppendIfPending(string_view text) noexcept -> ConditionalAppendResult;
        auto AppendReplayProgress(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision, string_view text) noexcept -> ReplayAppendResult;
        auto MarkCleanupAndTruncate(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision, uint64_t& cleanup_revision) noexcept -> bool;
        auto CompleteMarkedCleanup(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision, bool add_committed_marker, uint64_t& cleanup_revision) noexcept -> bool;
        auto CompleteLegacyCleanup(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision) noexcept -> bool;

    private:
        auto Read() noexcept -> optional<string>;
        auto TruncateLocked() noexcept -> bool FO_TSA_REQUIRES(_locker);
        auto AppendLocked(string_view text) noexcept -> bool FO_TSA_REQUIRES(_locker);

        string _path;
        FdType _fd {-1};
        std::atomic_bool _ioFailed {};
        mutable mutex _locker {};
        size_t _textSize {};
        vector<string> _content {};
        uint64_t _revision {};
        uint64_t _lastTruncationRevision {};
    };

    explicit DataBaseImpl(ptr<DataBaseSettings> db_settings, DataBasePanicCallback panic_callback);
    DataBaseImpl(const DataBaseImpl&) = delete;
    DataBaseImpl(DataBaseImpl&&) noexcept = delete;
    auto operator=(const DataBaseImpl&) = delete;
    auto operator=(DataBaseImpl&&) noexcept = delete;
    virtual ~DataBaseImpl() = default;

    [[nodiscard]] auto InValidState() const noexcept -> bool;
    [[nodiscard]] auto GetDbRequestsPerMinute() const -> size_t;
    [[nodiscard]] auto GetCollectionKeyType(hstring collection_name) const -> DataBaseKeyType;
    [[nodiscard]] virtual auto GetStringKeyEscaping() const noexcept -> DataBaseStringKeyEscaping = 0;
    [[nodiscard]] virtual auto GetAllRecordIds(hstring collection_name) const -> vector<DataBaseKey> = 0;
    [[nodiscard]] auto GetAllDocumentIds(hstring collection_name) const -> vector<DataBaseKey>;
    [[nodiscard]] auto GetDocument(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document;
    [[nodiscard]] auto GetDocuments(hstring collection_name, const vector<DataBaseKey>& ids) const -> vector<AnyData::Document>;

    void InitializeCollections(const DataBaseCollectionSchemas& collection_schemas);
    void InitializeOpLogs();
    void RestorePendingChanges();
    void Insert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc);
    auto TryInsert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) -> bool;
    auto CompareExchange(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& expected, const AnyData::Value& desired) -> bool;
    void Update(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& value);
    void UpdateFields(hstring collection_name, const DataBaseKey& id, const AnyData::Document& changes);
    void Delete(hstring collection_name, const DataBaseKey& id);
    void StartCommitChanges();
    void WaitCommitChanges();
    auto CreateSnapshot() -> vector<uint8_t>;
    void RestoreSnapshot(const_span<uint8_t> snapshot_data);
    void ClearChanges() noexcept;
    virtual void DrawGui();

protected:
    void StartCommitThread();
    void StopCommitThread() noexcept;

    enum class ReplayMutation
    {
        None,
        Insert,
        Update,
        Delete,
    };

    struct ReplayRecordSnapshot
    {
        AnyData::Document Record {};
        uint64_t Revision {};
    };

    virtual void EnsureCollection(hstring collection_name, DataBaseKeyType key_type) = 0;
    virtual auto GetRecord(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document = 0;
    virtual auto GetRecords(hstring collection_name, const vector<DataBaseKey>& ids) const -> vector<AnyData::Document>;
    virtual auto ReadReplayRecord(hstring collection_name, const DataBaseKey& id) const -> ReplayRecordSnapshot = 0;
    virtual auto ApplyReplayMutation(hstring collection_name, const DataBaseKey& id, uint64_t expected_revision, ReplayMutation mutation, const AnyData::Document& doc) -> bool = 0;
    virtual void InsertRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) = 0;
    virtual void UpdateRecord(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) = 0;
    virtual void DeleteRecord(hstring collection_name, const DataBaseKey& id) = 0;
    virtual auto CreateSnapshotData() -> vector<uint8_t>;
    virtual void RestoreSnapshotData(const_span<uint8_t> snapshot_data);
    virtual auto TryReconnect() -> bool { return true; }

    virtual void OnCommitOperationWrittenToOpLog() { } // Testing override point for a failed commit operation being durably written to oplog
    virtual void OnPendingChangesLogTruncated() { } // Testing override point after pending log cleanup
    virtual void OnRecoveryLogsSnapshotted() { } // Testing override point before acting on log snapshots
    virtual void OnReplayReadValidated() { } // Testing override point before a replayed backend mutation
    virtual void OnReplayWriteValidated() { } // Testing override point before recording replay progress
    virtual void OnReplayBeforeCommittedSnapshot() { } // Testing override point before the replay committed log snapshot
    virtual void OnReplayBeforePendingRefresh() { } // Testing override point before refreshing the pending log snapshot
    virtual void OnReplayBeforeCleanupSnapshot() { } // Testing override point before the final committed log snapshot
    virtual void OnPendingChangesRestored() { } // Testing override point for successful pending oplog restore

private:
    struct DbRequestsPerMinuteBucket
    {
        int64_t Second {};
        size_t Count {};
    };

    enum class CommitOperationType
    {
        Insert,
        Update,
        Delete,
    };

    struct CommitOperationData
    {
        CommitOperationType Type {};
        hstring CollectionName {};
        DataBaseKey RecordId {};
        AnyData::Document Doc {};
    };

    struct PendingReplayOverlay
    {
        vector<shared_ptr<CommitOperationData>> Operations {};
        uint64_t PendingRevision {};
        uint64_t CommittedRevision {};
        bool Active {};
    };

    struct DocumentReadState
    {
        size_t Readers {};
        uint64_t Generation {};
    };

    struct RecoveryWatch
    {
        bool Invalidated {};
    };

    class ScopedRecoveryWatch final
    {
    public:
        explicit ScopedRecoveryWatch(ptr<DataBaseImpl> owner);
        ScopedRecoveryWatch(const ScopedRecoveryWatch&) = delete;
        ScopedRecoveryWatch(ScopedRecoveryWatch&&) noexcept = delete;
        auto operator=(const ScopedRecoveryWatch&) -> ScopedRecoveryWatch& = delete;
        auto operator=(ScopedRecoveryWatch&&) noexcept -> ScopedRecoveryWatch& = delete;
        ~ScopedRecoveryWatch();

        [[nodiscard]] auto Get() noexcept -> RecoveryWatch& { return _watch; }

    private:
        ptr<DataBaseImpl> _owner;
        RecoveryWatch _watch {};
    };

    void InsertInner(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) FO_TSA_REQUIRES(_mutationLocker);
    void UpdateInner(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& value) FO_TSA_REQUIRES(_mutationLocker);
    void ScheduleCommit();
    void CommitNextChange() noexcept;
    void NoteCommittedRecordChange(hstring collection_name, const DataBaseKey& id, bool key_set_may_change) FO_TSA_REQUIRES(_stateLocker);
    void CommitThreadEntry() noexcept;
    void RestorePendingChanges(RecoveryWatch& watch);
    auto TryRestorePendingChanges(RecoveryWatch& watch) -> bool;
    auto ReadPendingReplayOverlay() const -> PendingReplayOverlay;
    auto IsPendingReplayOverlayCurrent(const PendingReplayOverlay& overlay) const noexcept -> bool;
    void CompleteRecovery(RecoveryWatch& watch);
    void MarkBackendFailed() const;
    void RegisterDbRequests(size_t request_count) const;
    auto ResolveCollectionName(string_view collection_name) const -> hstring;
    void ValidateCollectionKey(hstring collection_name, const DataBaseKey& id) const;
    auto ClaimPanic() noexcept -> bool;
    void DeliverPanic(string_view message);
    void StartPanic(string_view message);

    ptr<DataBaseSettings> _settings;
    mutex _mutationLocker {};
    mutable mutex _stateLocker {};
    mutable atomic_mutex _recoveryWatchLocker {};
    thread _commitThread {};
    std::condition_variable_any _commitThreadSignal {};
    mutable std::condition_variable_any _commitThreadDoneSignal {};
    bool _commitThreadStopRequested FO_TSA_GUARDED_BY(_stateLocker) {};
    bool _commitThreadActive FO_TSA_GUARDED_BY(_stateLocker) {};
    bool _commitInProgress FO_TSA_GUARDED_BY(_stateLocker) {};
    bool _snapshotInProgress FO_TSA_GUARDED_BY(_stateLocker) {};
    std::condition_variable_any _snapshotDoneSignal {};
    deque<shared_ptr<CommitOperationData>> _pendingCommitOperations FO_TSA_GUARDED_BY(_stateLocker) {};
    mutable unordered_map<pair<hstring, DataBaseKey>, DocumentReadState> _documentReadStates FO_TSA_GUARDED_BY(_stateLocker) {};
    mutable vector<ptr<RecoveryWatch>> _recoveryWatches FO_TSA_GUARDED_BY(_recoveryWatchLocker) {};
    unordered_map<hstring, uint64_t> _committedKeyChanges FO_TSA_GUARDED_BY(_stateLocker) {};
    mutable std::atomic_bool _backendFailed {};
    std::atomic_bool _panicStarted {};
    nanotime _panicRequestedTime {};
    bool _opLogEnabled {};
    optional<RecoveryLogHandle> _pendingChangesLog {};
    optional<RecoveryLogHandle> _committedChangesLog {};
    size_t _pendingChangesPanicThreshold {};
    timespan _panicShutdownTimeout {};
    timespan _reconnectRetryPeriod {};
    nanotime _reconnectRetryTime {};
    DataBasePanicCallback _panicCallback {};
    mutable mutex _dbRequestsMetricLocker {};
    mutable vector<DbRequestsPerMinuteBucket> _dbRequestsPerMinuteBuckets FO_TSA_GUARDED_BY(_dbRequestsMetricLocker) {vector<DbRequestsPerMinuteBucket>(60)};
    mutable std::atomic_size_t _dbRequestsPerMinute {};
    unordered_map<string, hstring> _collectionNames {};
    unordered_map<hstring, DataBaseKeyType> _collectionKeyTypes {};
};

auto GetDbKeyType(const DataBaseKey& key) noexcept -> DataBaseKeyType;

FO_END_NAMESPACE
