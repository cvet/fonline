// FOnline Engine
// SPDX-License-Identifier: MIT

#include "DataBase.h"

#include "DataBase-Json.h"
#include "DataBase-Memory.h"
#include "DataBase-Mongo.h"
#include "DataBase-SQLite.h"
#include "DataBaseSerialization.h"
#include "ImGuiStuff.h"

FO_DISABLE_WARNINGS_PUSH()
#include <json.hpp>
FO_DISABLE_WARNINGS_POP()

#include "WinApiUndef.inc"

FO_BEGIN_NAMESPACE

#if FO_WINDOWS
namespace osfile = winapi;
#else
namespace osfile = posix;
#endif

static constexpr std::chrono::milliseconds FAILED_BACKEND_POLL_INTERVAL {10};

class ScopedBackendWrite final
{
public:
    explicit ScopedBackendWrite(ptr<DataBaseImpl> owner) noexcept :
        _owner {owner},
        _previous {_active}
    {
        _active = this;
    }

    ScopedBackendWrite(const ScopedBackendWrite&) = delete;
    auto operator=(const ScopedBackendWrite&) = delete;

    ~ScopedBackendWrite() noexcept { _active = _previous; }

    static auto Contains(ptr<DataBaseImpl> owner) noexcept -> bool
    {
        auto frame {_active};

        while (frame) {
            if (frame->_owner == owner) {
                return true;
            }

            frame = frame->_previous;
        }

        return false;
    }

private:
    ptr<DataBaseImpl> _owner;
    nptr<ScopedBackendWrite> _previous {};
    static thread_local nptr<ScopedBackendWrite> _active;
};

thread_local nptr<ScopedBackendWrite> ScopedBackendWrite::_active {};

static void ValidateFiniteAnyValue(const AnyData::Value& value);
static void ValidateFiniteAnyDocument(const AnyData::Document& doc);
static auto AreDocumentsEqual(const AnyData::Document& left, const AnyData::Document& right) -> bool;
static auto DoesDocumentContain(const AnyData::Document& target, const AnyData::Document& patch) -> bool;
static auto IsDbKeyValueValid(const DataBaseKey& key) noexcept -> bool;
static auto DbKeyTypeName(DataBaseKeyType key_type) noexcept -> string_view;
static auto EncodeStorageDbKey(const DataBaseKey& key, DataBaseKeyType key_type, DataBaseStringKeyEscaping escaping) -> string;
static auto DecodeStorageDbKey(string_view key_str, DataBaseKeyType key_type, DataBaseStringKeyEscaping escaping) -> DataBaseKey;
static auto EncodeBackendDbKey(const DataBaseKey& key, DataBaseKeyType key_type, DataBaseStringKeyEscaping escaping) -> DataBaseKey;
static auto DecodeBackendDbKey(const DataBaseKey& key, DataBaseKeyType key_type, DataBaseStringKeyEscaping escaping) -> DataBaseKey;
static auto EncodeDbStringKey(string_view value, DataBaseStringKeyEscaping escaping) -> string;
static auto DecodeDbStringKey(string_view value, DataBaseStringKeyEscaping escaping) -> string;
static auto ShouldEscapeDbStringByte(uint8_t byte, DataBaseStringKeyEscaping escaping) noexcept -> bool;
static auto DecodeHexDigit(char ch) -> uint8_t;

DataBase::DataBase() = default;

DataBase::DataBase(DataBase&&) noexcept = default;

auto DataBase::operator=(DataBase&& other) noexcept -> DataBase&
{
    if (this != &other) {
        _impl = std::move(other._impl);
    }

    return *this;
}

DataBase::~DataBase() = default;

DataBase::DataBase(unique_ptr<DataBaseImpl> impl) :
    _impl {std::move(impl)}
{
    FO_VERIFY_AND_THROW(_impl, "Missing database backend state");
}

auto DataBase::InValidState() const noexcept -> bool
{
    FO_STRONG_ASSERT(_impl, "Database implementation is null");
    return _impl->InValidState();
}

auto DataBase::GetDbRequestsPerMinute() const -> size_t
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    return _impl->GetDbRequestsPerMinute();
}

auto DataBase::GetAllIds(hstring collection_name) const -> vector<DataBaseKey>
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    return _impl->GetAllDocumentIds(collection_name);
}

auto DataBase::GetAllIntIds(hstring collection_name) const -> vector<ident_t>
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    auto key_type {_impl->GetCollectionKeyType(collection_name)};

    if (key_type != DataBaseKeyType::IntId) {
        throw DataBaseException("Invalid database collection key type", collection_name, DbKeyTypeName(DataBaseKeyType::IntId), DbKeyTypeName(key_type));
    }

    vector<ident_t> typed_ids {};

    for (const auto& id : GetAllIds(collection_name)) {
        typed_ids.emplace_back(std::get<ident_t>(id));
    }

    return typed_ids;
}

auto DataBase::GetAllStringIds(hstring collection_name) const -> vector<string>
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    auto key_type {_impl->GetCollectionKeyType(collection_name)};

    if (key_type != DataBaseKeyType::String) {
        throw DataBaseException("Invalid database collection key type", collection_name, DbKeyTypeName(DataBaseKeyType::String), DbKeyTypeName(key_type));
    }

    vector<string> typed_ids {};

    for (const auto& id : GetAllIds(collection_name)) {
        typed_ids.emplace_back(std::get<string>(id));
    }

    return typed_ids;
}

auto DataBase::Get(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    return _impl->GetDocument(collection_name, id);
}

auto DataBase::GetMany(hstring collection_name, const vector<DataBaseKey>& ids) const -> vector<AnyData::Document>
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    return _impl->GetDocuments(collection_name, ids);
}

auto DataBase::Valid(hstring collection_name, const DataBaseKey& id) const -> bool
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    return !_impl->GetDocument(collection_name, id).Empty();
}

void DataBase::Insert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc)
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    _impl->Insert(collection_name, id, doc);
}

void DataBase::Update(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& value)
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    _impl->Update(collection_name, id, key, value);
}

void DataBase::UpdateFields(hstring collection_name, const DataBaseKey& id, const AnyData::Document& changes)
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    _impl->UpdateFields(collection_name, id, changes);
}

auto DataBase::TryInsert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) -> bool
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    return _impl->TryInsert(collection_name, id, doc);
}

auto DataBase::CompareExchange(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& expected, const AnyData::Value& desired) -> bool
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    return _impl->CompareExchange(collection_name, id, key, expected, desired);
}

void DataBase::Delete(hstring collection_name, const DataBaseKey& id)
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    _impl->Delete(collection_name, id);
}

void DataBase::StartCommitChanges()
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    _impl->StartCommitChanges();
}

void DataBase::WaitCommitChanges()
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    _impl->WaitCommitChanges();
}

auto DataBase::CreateSnapshot() -> vector<uint8_t>
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    return _impl->CreateSnapshot();
}

void DataBase::RestoreSnapshot(const_span<uint8_t> snapshot_data)
{
    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    _impl->RestoreSnapshot(snapshot_data);
}

void DataBase::ClearChanges() noexcept
{
    FO_STRONG_ASSERT(_impl, "Database implementation is null");
    _impl->ClearChanges();
}

void DataBase::DrawGui()
{
    if (!InValidState()) {
        ImGui::TextColored(ImVec4 {1.0f, 0.4f, 0.4f, 1.0f}, "Database is in failed state");
        return;
    }

    FO_VERIFY_AND_THROW(_impl, "Database implementation is null");
    _impl->DrawGui();
}

DataBaseImpl::DataBaseImpl(ptr<DataBaseSettings> db_settings, DataBasePanicCallback panic_callback) :
    _settings {db_settings},
    _opLogEnabled {_settings->DataBase.OpLogEnabled},
    _pendingChangesPanicThreshold {numeric_cast<size_t>(_settings->DataBase.PanicOpLogSizeThreshold)},
    _panicShutdownTimeout {std::chrono::milliseconds {_settings->DataBase.PanicShutdownTimeout}},
    _reconnectRetryPeriod {std::chrono::milliseconds {std::max(_settings->DataBase.ReconnectRetryPeriod, 1)}},
    _panicCallback {std::move(panic_callback)}
{
}

auto DataBaseImpl::GetCollectionKeyType(hstring collection_name) const -> DataBaseKeyType
{
    auto it {_collectionKeyTypes.find(collection_name)};

    if (it == _collectionKeyTypes.end()) {
        throw DataBaseException("Unknown database collection", collection_name);
    }

    return it->second;
}

auto DataBaseImpl::ResolveCollectionName(string_view collection_name) const -> hstring
{
    auto it {_collectionNames.find(collection_name)};

    if (it == _collectionNames.end()) {
        throw DataBaseException("Unknown database collection name", collection_name);
    }

    return it->second;
}

void DataBaseImpl::InitializeCollections(const DataBaseCollectionSchemas& collection_schemas)
{
    FO_TRACE_ZONE(Database);

    for (const auto& [collection_name, key_type] : collection_schemas) {
        FO_VERIFY_AND_THROW(!_collectionNames.contains(collection_name.as_str()), "Database collection name is already registered", collection_name, key_type);
        _collectionNames.emplace(collection_name.as_str(), collection_name);
        _collectionKeyTypes.emplace(collection_name, key_type);
        EnsureCollection(collection_name, key_type);
    }
}

void DataBaseImpl::InitializeOpLogs()
{
    FO_TRACE_ZONE(Database);

    if (!_opLogEnabled) {
        return;
    }

    if (_settings->DataBase.OpLogPath.empty()) {
        throw DataBaseException("Empty oplog path in settings");
    }

    auto close_on_error {scope_fail([this]() noexcept {
        _committedChangesLog.reset();
        _pendingChangesLog.reset();
    })};

    auto open_log_file {[&](optional<RecoveryLogHandle>& handle, string_view file_path, string_view file_desc, bool allow_reset_marker) {
        if (file_path.empty()) {
            throw DataBaseException("Empty oplog path", file_desc);
        }

        string dir {strex(file_path).extract_dir().str()};

        if (!dir.empty() && !fs::create_directories(dir)) {
            throw DataBaseException("Oplog directory can't be created", file_desc, dir);
        }

        handle.emplace(string(file_path));

        // Validate oplog line format
        vector<string> content {handle->SnapshotContent()};

        for (size_t i {0}; i < content.size(); i++) {
            auto line {string_view {content[i]}};

            if ((line == "@reset-v2" || (line == "@reset-v1" && allow_reset_marker)) && i > 0 && i + 1 == content.size()) {
                continue;
            }

            auto first_space {line.find(' ')};
            size_t second_space {first_space != string_view::npos ? line.find(' ', first_space + 1) : string_view::npos};
            size_t third_space {second_space != string_view::npos ? line.find(' ', second_space + 1) : string_view::npos};

            if (first_space == string_view::npos || first_space == 0 || second_space == string_view::npos || second_space == first_space + 1) {
                throw DataBaseException("Oplog file has invalid command format", i + 1, file_path);
            }

            auto command {line.substr(0, first_space)};
            auto collection {line.substr(first_space + 1, second_space - first_space - 1)};
            string_view record_id_str {};
            string_view other {};
            bool has_other {false};

            if (third_space == string_view::npos) {
                if (second_space + 1 >= line.size()) {
                    throw DataBaseException("Oplog file has invalid command format", i + 1, file_path);
                }

                record_id_str = line.substr(second_space + 1);
            }
            else {
                if (third_space == second_space + 1 || third_space + 1 >= line.size()) {
                    throw DataBaseException("Oplog file has invalid command format", i + 1, file_path);
                }

                record_id_str = line.substr(second_space + 1, third_space - second_space - 1);
                other = line.substr(third_space + 1);
                has_other = true;
            }

            if (command != "insert" && command != "update" && command != "delete") {
                throw DataBaseException("Oplog file has unknown command", command, i + 1, file_path);
            }
            if ((command == "insert" || command == "update") && !has_other) {
                throw DataBaseException("Oplog file has invalid insert/update command format", i + 1, file_path);
            }
            if (command == "delete" && has_other) {
                throw DataBaseException("Oplog file has invalid delete command format", i + 1, file_path);
            }

            try {
                hstring collection_name {ResolveCollectionName(collection)};
                auto record_id {DecodeStorageDbKey(record_id_str, GetCollectionKeyType(collection_name), GetStringKeyEscaping())};

                if (!IsDbKeyValueValid(record_id)) {
                    throw DataBaseException("Oplog file has invalid record id value", record_id_str, i + 1, file_path);
                }
            }
            catch (const std::exception&) {
                throw DataBaseException("Oplog file has invalid record id value", record_id_str, i + 1, file_path);
            }

            if (has_other && (other.find('\n') != string_view::npos || other.find('\r') != string_view::npos)) {
                throw DataBaseException("Oplog file has invalid payload line break", i + 1, file_path);
            }
        }

        return;
    }};

    string oplog_path {fs::make_writable_path(_settings->Common.UserWritablePath, _settings->DataBase.OpLogPath)};
    open_log_file(_pendingChangesLog, oplog_path, "pending database changes file", false);
    open_log_file(_committedChangesLog, strex(oplog_path).replace(".oplog", "-committed.oplog").str(), "committed database changes file", true);

    auto committed_content {_committedChangesLog->SnapshotContent()};
    bool cleanup_marked {!committed_content.empty() && (committed_content.back() == "@reset-v1" || committed_content.back() == "@reset-v2")};

    if (!cleanup_marked && committed_content.size() > _pendingChangesLog->GetLinesCount()) {
        throw DataBaseException("Committed database changes file line count is greater than pending database changes file line count");
    }

    close_on_error.release();
}

DataBaseImpl::ScopedRecoveryWatch::ScopedRecoveryWatch(ptr<DataBaseImpl> owner) :
    _owner {owner}
{
    scoped_lock locker {_owner->_recoveryWatchLocker};
    _owner->_recoveryWatches.emplace_back(&_watch);
}

DataBaseImpl::ScopedRecoveryWatch::~ScopedRecoveryWatch()
{
    scoped_lock locker {_owner->_recoveryWatchLocker};
    auto it {std::ranges::find(_owner->_recoveryWatches, ptr<RecoveryWatch> {&_watch})};
    FO_STRONG_ASSERT(it != _owner->_recoveryWatches.end(), "Database recovery watch is missing");
    _owner->_recoveryWatches.erase(it);
}

void DataBaseImpl::RestorePendingChanges()
{
    if (!_opLogEnabled) {
        return;
    }

    if (ScopedBackendWrite::Contains(this)) {
        throw DataBaseException("Nested recovery from a backend write is not supported");
    }

    ScopedRecoveryWatch watch {this};
    RestorePendingChanges(watch.Get());
}

void DataBaseImpl::RestorePendingChanges(RecoveryWatch& watch)
{
    while (!TryRestorePendingChanges(watch)) {
    }
}

auto DataBaseImpl::TryRestorePendingChanges(RecoveryWatch& watch) -> bool
{
    FO_TRACE_ZONE(Database);

    FO_VERIFY_AND_THROW(_pendingChangesLog, "Missing required pending changes log");
    FO_VERIFY_AND_THROW(_committedChangesLog, "Missing required committed changes log");

    if (!_pendingChangesLog->IsUsable() || !_committedChangesLog->IsUsable()) {
        throw DataBaseException("Database recovery log is unusable after an I/O failure");
    }

    vector<string> pending_changes_content {};
    vector<string> committed_changes_content {};
    uint64_t pending_revision {};

    while (true) {
        auto pending_snapshot {_pendingChangesLog->SnapshotContentWithRevision()};
        pending_changes_content = std::move(pending_snapshot.first);
        pending_revision = pending_snapshot.second;
        auto committed_initial_snapshot {_committedChangesLog->SnapshotContentWithRevision()};
        committed_changes_content = std::move(committed_initial_snapshot.first);
        OnRecoveryLogsSnapshotted();

        if (!_pendingChangesLog->IsUsableAtRevision(pending_revision) || !_committedChangesLog->IsUsableAtRevision(committed_initial_snapshot.second)) {
            continue;
        }

        bool pending_marked {!pending_changes_content.empty() && pending_changes_content.back() == "@reset-v2"};
        bool cleanup_marked_v2 {!committed_changes_content.empty() && committed_changes_content.back() == "@reset-v2"};
        bool cleanup_marked {!committed_changes_content.empty() && committed_changes_content.back() == "@reset-v1"};

        if (pending_marked) {
            pending_changes_content.pop_back();

            if (cleanup_marked_v2) {
                committed_changes_content.pop_back();
            }

            if (pending_changes_content != committed_changes_content || committed_changes_content.empty()) {
                throw DataBaseException("Marked pending database cleanup does not match committed changes");
            }

            uint64_t cleanup_revision {};
            if (!_pendingChangesLog->CompleteMarkedCleanup(*_committedChangesLog, pending_revision, committed_initial_snapshot.second, !cleanup_marked_v2, cleanup_revision)) {
                if (!_pendingChangesLog->IsUsable() || !_committedChangesLog->IsUsable()) {
                    throw DataBaseException("Marked database changes files can't complete cleanup");
                }

                continue;
            }

            OnPendingChangesLogTruncated();

            if (!_committedChangesLog->TruncateIfRevision(cleanup_revision) && !_committedChangesLog->IsUsable()) {
                throw DataBaseException("Marked committed database changes file can't be truncated");
            }

            CompleteRecovery(watch);
            OnPendingChangesRestored();
            return true;
        }

        if (cleanup_marked_v2) {
            if (!_committedChangesLog->TruncateIfRevision(committed_initial_snapshot.second) && !_committedChangesLog->IsUsable()) {
                throw DataBaseException("Marked committed database changes file can't be truncated");
            }

            continue;
        }

        if (cleanup_marked) {
            if (!pending_changes_content.empty()) {
                throw DataBaseException("Marked database cleanup has ambiguous pending changes");
            }

            if (!_pendingChangesLog->CompleteLegacyCleanup(*_committedChangesLog, pending_revision, committed_initial_snapshot.second)) {
                if (!_pendingChangesLog->IsUsable() || !_committedChangesLog->IsUsable()) {
                    throw DataBaseException("Legacy database changes files can't complete cleanup", _settings->DataBase.OpLogPath);
                }

                continue;
            }

            OnPendingChangesLogTruncated();

            CompleteRecovery(watch);
            OnPendingChangesRestored();
            return true;
        }

        break;
    }

    size_t replayed_commands {0};
    uint64_t cleanup_revision {};

    while (true) {
        OnReplayBeforeCommittedSnapshot();
        auto committed_replay_snapshot {_committedChangesLog->SnapshotContentWithRevision()};
        committed_changes_content = std::move(committed_replay_snapshot.first);
        uint64_t committed_revision {committed_replay_snapshot.second};

        if (!_pendingChangesLog->IsUsableAtRevision(pending_revision) || !_committedChangesLog->IsUsableAtRevision(committed_revision) || !_pendingChangesLog->IsUsableAtRevision(pending_revision)) {
            return false;
        }

        if (committed_changes_content.size() > pending_changes_content.size()) {
            throw DataBaseException("Committed database changes file has more lines than pending database changes file");
        }

        for (size_t i {0}; i < committed_changes_content.size(); i++) {
            if (pending_changes_content[i] != committed_changes_content[i]) {
                throw DataBaseException("Committed oplog line doesn't match pending oplog line", i + 1, _settings->DataBase.OpLogPath);
            }
        }

        size_t pending_commands_to_replay {replayed_commands + pending_changes_content.size() - committed_changes_content.size()};

        try {
            for (size_t i {committed_changes_content.size()}; i < pending_changes_content.size(); i++) {
                const auto& line {pending_changes_content[i]};
                auto line_view {string_view {line}};
                auto first_space {line_view.find(' ')};
                auto second_space {line_view.find(' ', first_space + 1)};
                FO_VERIFY_AND_THROW(first_space != string_view::npos && first_space != 0, "Pending database oplog command has no collection name", i + 1, _settings->DataBase.OpLogPath, line_view.size(), first_space);
                FO_VERIFY_AND_THROW(second_space != string_view::npos && second_space != first_space + 1, "Pending database oplog command has no record id", i + 1, _settings->DataBase.OpLogPath, line_view.size(), first_space, second_space);

                auto command {line_view.substr(0, first_space)};
                auto collection {line_view.substr(first_space + 1, second_space - first_space - 1)};
                string_view record_id_str {};
                string_view other {};

                auto third_space {line_view.find(' ', second_space + 1)};

                if (third_space == string_view::npos) {
                    FO_VERIFY_AND_THROW(second_space + 1 < line_view.size(), "Pending database oplog command has no record id after the second separator", i + 1, command, collection, second_space, line_view.size());
                    record_id_str = line_view.substr(second_space + 1);
                }
                else {
                    FO_VERIFY_AND_THROW(third_space != second_space + 1 && third_space + 1 < line_view.size(), "Pending database oplog command has an empty record id or missing payload", i + 1, command, collection, second_space, third_space, line_view.size());
                    record_id_str = line_view.substr(second_space + 1, third_space - second_space - 1);
                    other = line_view.substr(third_space + 1);
                }

                hstring collection_name {ResolveCollectionName(collection)};
                auto key_type {GetCollectionKeyType(collection_name)};
                auto record_id {DecodeStorageDbKey(record_id_str, key_type, GetStringKeyEscaping())};
                auto storage_record_id {EncodeBackendDbKey(record_id, key_type, GetStringKeyEscaping())};
                auto snapshot {ReadReplayRecord(collection_name, storage_record_id)};
                auto& current_doc {snapshot.Record};

                if (!_pendingChangesLog->IsUsableAtRevision(pending_revision) || !_committedChangesLog->IsUsableAtRevision(committed_revision)) {
                    return false;
                }

                OnReplayReadValidated();

                if (!_pendingChangesLog->IsUsableAtRevision(pending_revision) || !_committedChangesLog->IsUsableAtRevision(committed_revision)) {
                    return false;
                }

                ReplayMutation mutation {ReplayMutation::None};
                AnyData::Document doc {};

                if (command == "delete") {
                    if (!current_doc.Empty()) {
                        mutation = ReplayMutation::Delete;
                    }
                }
                else {
                    doc = DataBaseSerialization::FromJson(nlohmann::json::parse(other));

                    if (command == "insert") {
                        if (current_doc.Empty()) {
                            mutation = ReplayMutation::Insert;
                        }
                        else if (!AreDocumentsEqual(current_doc, doc)) {
                            if (!ApplyReplayMutation(collection_name, storage_record_id, snapshot.Revision, ReplayMutation::None, doc)) {
                                return false;
                            }

                            throw DataBaseException("Pending database insert replay conflict", record_id, _settings->DataBase.OpLogPath);
                        }
                    }
                    else if (!DoesDocumentContain(current_doc, doc)) {
                        mutation = ReplayMutation::Update;
                    }
                }

                bool applied {};

                if (mutation == ReplayMutation::None) {
                    applied = ApplyReplayMutation(collection_name, storage_record_id, snapshot.Revision, mutation, doc);
                }
                else {
                    ScopedBackendWrite backend_write {this};
                    applied = ApplyReplayMutation(collection_name, storage_record_id, snapshot.Revision, mutation, doc);
                }

                if (!applied) {
                    return false;
                }

                {
                    scoped_lock locker {_stateLocker};
                    NoteCommittedRecordChange(collection_name, record_id, command == "insert" || command == "delete");
                }

                OnReplayWriteValidated();

                auto append_result {_pendingChangesLog->AppendReplayProgress(*_committedChangesLog, pending_revision, committed_revision, strex("{}\n", line).str())};
                if (append_result == RecoveryLogHandle::ReplayAppendResult::RevisionChanged) {
                    return false;
                }
                if (append_result == RecoveryLogHandle::ReplayAppendResult::Failed) {
                    throw DataBaseException("Committed pending database changes file append failed during restore");
                }

                committed_revision++;
                if (++replayed_commands % 100 == 0) {
                    logging::write("Replayed {}/{} pending database commands", replayed_commands, pending_commands_to_replay);
                }
            }
        }
        catch (const DataBaseException&) {
            throw;
        }
        catch (const std::exception& ex) {
            throw DataBaseException("Pending database command parsing failed", ex.what(), _settings->DataBase.OpLogPath);
        }

        auto refresh_pending {[&]() -> optional<bool> {
            auto latest {_pendingChangesLog->SnapshotContentWithRevision()};

            if (latest.second == pending_revision) {
                return false;
            }

            if (_pendingChangesLog->HasTruncatedSince(pending_revision) || (!latest.first.empty() && latest.first.back() == "@reset-v2")) {
                return std::nullopt;
            }

            if (latest.first.size() <= pending_changes_content.size() || !std::equal(pending_changes_content.begin(), pending_changes_content.end(), latest.first.begin())) {
                throw DataBaseException("Pending database changes file was replaced during restore");
            }

            pending_changes_content = std::move(latest.first);
            pending_revision = latest.second;
            return true;
        }};

        OnReplayBeforePendingRefresh();
        auto refreshed {refresh_pending()};
        if (!refreshed) {
            return false;
        }
        if (*refreshed) {
            continue;
        }

        OnReplayBeforeCleanupSnapshot();
        auto committed_snapshot {_committedChangesLog->SnapshotContentWithRevision()};

        if (!_pendingChangesLog->IsUsableAtRevision(pending_revision) || !_committedChangesLog->IsUsableAtRevision(committed_snapshot.second) || !_pendingChangesLog->IsUsableAtRevision(pending_revision)) {
            return false;
        }

        FO_VERIFY_AND_THROW(std::ranges::equal(pending_changes_content, committed_snapshot.first), "Pending and committed database logs contain different command payloads");
        cleanup_revision = committed_snapshot.second;
        bool pending_cleared {pending_changes_content.empty() ? _pendingChangesLog->TruncateIfRevision(pending_revision) : _pendingChangesLog->MarkCleanupAndTruncate(*_committedChangesLog, pending_revision, committed_snapshot.second, cleanup_revision)};

        if (!pending_cleared) {
            refreshed = refresh_pending();
            if (!refreshed) {
                return false;
            }
            if (*refreshed) {
                continue;
            }

            throw DataBaseException("Pending database changes file changed or can't be truncated after restore", _settings->DataBase.OpLogPath);
        }

        break;
    }

    OnPendingChangesLogTruncated();

    if (!_committedChangesLog->TruncateIfRevision(cleanup_revision) && !_committedChangesLog->IsUsable()) {
        throw DataBaseException("Committed pending database changes file can't be truncated after successful restore", strex(_settings->DataBase.OpLogPath).replace(".oplog", "-committed.oplog").str());
    }

    logging::write("Pending database changes successfully restored, total {} commands replayed", replayed_commands);
    CompleteRecovery(watch);
    OnPendingChangesRestored();
    return true;
}

void DataBaseImpl::CompleteRecovery(RecoveryWatch& watch)
{
    scoped_lock watch_locker {_recoveryWatchLocker};

    if (!watch.Invalidated) {
        _backendFailed = false;
    }
}

void DataBaseImpl::MarkBackendFailed() const
{
    {
        scoped_lock locker {_stateLocker};
        scoped_lock watch_locker {_recoveryWatchLocker};
        _backendFailed = true;

        for (auto watch : _recoveryWatches) {
            watch->Invalidated = true;
        }
    }

    _commitThreadDoneSignal.notify_all();
}

auto DataBaseImpl::InValidState() const noexcept -> bool
{
    return !_backendFailed.load(std::memory_order_relaxed) && !_panicStarted.load(std::memory_order_relaxed);
}

auto DataBaseImpl::GetDbRequestsPerMinute() const -> size_t
{
    return _dbRequestsPerMinute.load(std::memory_order_relaxed);
}

auto DataBaseImpl::ReadPendingReplayOverlay() const -> PendingReplayOverlay
{
    FO_TRACE_ZONE(Database);

    if (!_opLogEnabled || !_pendingChangesLog || !_committedChangesLog) {
        return {};
    }

    while (true) {
        auto pending_snapshot {_pendingChangesLog->SnapshotContentWithRevision()};
        auto committed_snapshot {_committedChangesLog->SnapshotContentWithRevision()};
        auto& pending {pending_snapshot.first};
        auto& committed {committed_snapshot.first};
        PendingReplayOverlay overlay {{}, pending_snapshot.second, committed_snapshot.second, true};

        if (!IsPendingReplayOverlayCurrent(overlay)) {
            continue;
        }

        bool pending_marked {!pending.empty() && pending.back() == "@reset-v2"};
        bool committed_marked_v2 {!committed.empty() && committed.back() == "@reset-v2"};
        bool committed_marked_v1 {!committed.empty() && committed.back() == "@reset-v1"};

        if (pending_marked) {
            pending.pop_back();

            if (committed_marked_v2) {
                committed.pop_back();
            }

            FO_VERIFY_AND_THROW(!committed.empty() && pending == committed, "Marked pending database cleanup does not match committed changes");
            return overlay;
        }

        if (committed_marked_v2) {
            committed.clear();
        }
        else if (committed_marked_v1) {
            FO_VERIFY_AND_THROW(pending.empty(), "Marked database cleanup has ambiguous pending changes");
            return overlay;
        }

        FO_VERIFY_AND_THROW(committed.size() <= pending.size(), "Committed database changes file has more lines than pending database changes file");

        for (size_t i {0}; i < committed.size(); i++) {
            FO_VERIFY_AND_THROW(pending[i] == committed[i], "Committed oplog line doesn't match pending oplog line", i + 1, _settings->DataBase.OpLogPath);
        }

        for (size_t i {committed.size()}; i < pending.size(); i++) {
            string_view line {pending[i]};
            auto first_space {line.find(' ')};
            auto second_space {first_space != string_view::npos ? line.find(' ', first_space + 1) : string_view::npos};
            auto third_space {second_space != string_view::npos ? line.find(' ', second_space + 1) : string_view::npos};
            FO_VERIFY_AND_THROW(first_space != string_view::npos && first_space != 0 && second_space != string_view::npos && second_space > first_space + 1, "Pending database oplog command has invalid fields", i + 1);

            auto command {line.substr(0, first_space)};
            auto collection_name {ResolveCollectionName(line.substr(first_space + 1, second_space - first_space - 1))};
            auto record_end {third_space == string_view::npos ? line.size() : third_space};
            FO_VERIFY_AND_THROW(record_end > second_space + 1, "Pending database oplog command has no record id", i + 1);
            auto record_id {DecodeStorageDbKey(line.substr(second_space + 1, record_end - second_space - 1), GetCollectionKeyType(collection_name), GetStringKeyEscaping())};
            auto operation {safe_alloc::make_shared<CommitOperationData>()};
            operation->CollectionName = collection_name;
            operation->RecordId = std::move(record_id);

            if (command == "delete") {
                FO_VERIFY_AND_THROW(third_space == string_view::npos, "Pending database delete has an unexpected payload", i + 1);
                operation->Type = CommitOperationType::Delete;
            }
            else {
                FO_VERIFY_AND_THROW((command == "insert" || command == "update") && third_space != string_view::npos && third_space + 1 < line.size(), "Pending database oplog command has invalid payload", i + 1);
                operation->Type = command == "insert" ? CommitOperationType::Insert : CommitOperationType::Update;

                try {
                    operation->Doc = DataBaseSerialization::FromJson(nlohmann::json::parse(line.substr(third_space + 1)));
                }
                catch (const DataBaseException&) {
                    throw;
                }
                catch (const std::exception& ex) {
                    throw DataBaseException("Pending database command parsing failed", ex.what(), _settings->DataBase.OpLogPath);
                }
            }

            overlay.Operations.emplace_back(std::move(operation));
        }

        if (IsPendingReplayOverlayCurrent(overlay)) {
            return overlay;
        }
    }
}

auto DataBaseImpl::IsPendingReplayOverlayCurrent(const PendingReplayOverlay& overlay) const noexcept -> bool
{
    return !overlay.Active || (_pendingChangesLog->IsUsableAtRevision(overlay.PendingRevision) && _committedChangesLog->IsUsableAtRevision(overlay.CommittedRevision));
}

auto DataBaseImpl::GetAllDocumentIds(hstring collection_name) const -> vector<DataBaseKey>
{
    FO_TRACE_ZONE(Database);

    auto key_type {GetCollectionKeyType(collection_name)};
    auto key_escaping {GetStringKeyEscaping()};

    // Keys the commit thread added or removed while the backend was listed are listed again, so the result is the
    // stored key set with the still-pending inserts and deletes laid over it, the same view document reads give
    while (true) {
        uint64_t committed_before {0};

        {
            scoped_lock locker {_stateLocker};

            auto it {_committedKeyChanges.find(collection_name)};
            committed_before = it != _committedKeyChanges.end() ? it->second : 0;
        }

        auto replay_overlay {ReadPendingReplayOverlay()};
        vector<DataBaseKey> ids {};

        try {
            ids = GetAllRecordIds(collection_name);
        }
        catch (const std::exception&) {
            MarkBackendFailed();
            throw;
        }

        for (auto& id : ids) {
            id = DecodeBackendDbKey(id, key_type, key_escaping);

            if (GetDbKeyType(id) != key_type) {
                throw DataBaseException("Database collection returned invalid key type", collection_name, id, DbKeyTypeName(key_type));
            }
        }

        if (!IsPendingReplayOverlayCurrent(replay_overlay)) {
            continue;
        }

        vector<shared_ptr<CommitOperationData>> queued_ops {};

        {
            scoped_lock locker {_stateLocker};

            auto committed_it {_committedKeyChanges.find(collection_name)};
            uint64_t committed_after {committed_it != _committedKeyChanges.end() ? committed_it->second : 0};

            if (committed_after != committed_before) {
                continue;
            }

            for (const auto& pending_op : _pendingCommitOperations) {
                if (pending_op->CollectionName == collection_name) {
                    queued_ops.emplace_back(pending_op);
                }
            }
        }

        if (!IsPendingReplayOverlayCurrent(replay_overlay)) {
            continue;
        }

        unordered_set<DataBaseKey> present {ids.begin(), ids.end()};
        vector<DataBaseKey> inserted {};

        auto apply_pending {[&](const auto& operations) {
            for (const auto& pending_op : operations) {
                if (pending_op->CollectionName != collection_name) {
                    continue;
                }

                if (pending_op->Type == CommitOperationType::Insert) {
                    if (present.emplace(pending_op->RecordId).second) {
                        inserted.emplace_back(pending_op->RecordId);
                    }
                }
                else if (pending_op->Type == CommitOperationType::Delete) {
                    present.erase(pending_op->RecordId);
                }
            }
        }};

        apply_pending(replay_overlay.Operations);
        apply_pending(queued_ops);

        vector<DataBaseKey> result {};
        result.reserve(present.size());

        auto take_present {[&](vector<DataBaseKey>& source) {
            for (auto& id : source) {
                if (present.erase(id) != 0) {
                    result.emplace_back(std::move(id));
                }
            }
        }};

        take_present(ids);
        take_present(inserted);
        return result;
    }
}

auto DataBaseImpl::GetDocument(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document
{
    auto docs {GetDocuments(collection_name, {id})};
    FO_VERIFY_AND_THROW(docs.size() == 1, "Database returned a different number of documents than requested", collection_name, id, docs.size());
    return std::move(docs.front());
}

auto DataBaseImpl::GetDocuments(hstring collection_name, const vector<DataBaseKey>& ids) const -> vector<AnyData::Document>
{
    FO_TRACE_ZONE(Database);

    if (!InValidState()) {
        throw DataBaseException("Database backend is in failed state");
    }

    vector<AnyData::Document> docs {vector<AnyData::Document>(ids.size())};

    if (ids.empty()) {
        return docs;
    }

    auto key_type {GetCollectionKeyType(collection_name)};
    auto key_escaping {GetStringKeyEscaping()};
    unordered_map<DataBaseKey, size_t> unique_index_by_id {};
    vector<DataBaseKey> unique_ids {};
    vector<DataBaseKey> storage_ids {};
    vector<size_t> doc_unique_index {};
    doc_unique_index.reserve(ids.size());

    for (const auto& id : ids) {
        auto [it, inserted] {unique_index_by_id.emplace(id, unique_ids.size())};

        if (inserted) {
            ValidateCollectionKey(collection_name, id);
            storage_ids.emplace_back(EncodeBackendDbKey(id, key_type, key_escaping));
            unique_ids.emplace_back(id);
        }

        doc_unique_index.emplace_back(it->second);
    }

    {
        scoped_lock locker {_stateLocker};

        for (const auto& id : unique_ids) {
            _documentReadStates[{collection_name, id}].Readers++;
        }
    }

    auto release_readers {scope_exit([&]() noexcept {
        safe_call([&]() {
            scoped_lock locker {_stateLocker};

            for (const auto& id : unique_ids) {
                auto it {_documentReadStates.find({collection_name, id})};
                FO_STRONG_ASSERT(it != _documentReadStates.end() && it->second.Readers != 0, "Database reader tracking is missing", collection_name, id);

                if (--it->second.Readers == 0) {
                    _documentReadStates.erase(it);
                }
            }
        });
    })};

    vector<AnyData::Document> unique_docs {vector<AnyData::Document>(unique_ids.size())};
    vector<size_t> pending_indices {vector<size_t>(unique_ids.size())};
    std::iota(pending_indices.begin(), pending_indices.end(), size_t {0});

    // A record committed while it was being read changes its generation and is read again, so
    // every returned document is the stored one with the still-pending operations laid over it
    while (!pending_indices.empty()) {
        vector<DataBaseKey> request_ids {vec_transform(pending_indices, [&](size_t index) -> DataBaseKey { return storage_ids[index]; })};
        vector<uint64_t> expected_generations {};
        expected_generations.reserve(pending_indices.size());

        {
            scoped_lock locker {_stateLocker};

            for (size_t index : pending_indices) {
                expected_generations.emplace_back(_documentReadStates.at({collection_name, unique_ids[index]}).Generation);
            }
        }

        auto replay_overlay {ReadPendingReplayOverlay()};
        vector<AnyData::Document> records {};

        try {
            records = GetRecords(collection_name, request_ids);
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
            MarkBackendFailed();
            throw DataBaseException("Database backend failed to get document", ex.what());
        }

        FO_VERIFY_AND_THROW(records.size() == request_ids.size(), "Database backend returned a different number of documents than requested", collection_name, request_ids.size(), records.size());
        RegisterDbRequests(1);

        if (!IsPendingReplayOverlayCurrent(replay_overlay)) {
            continue;
        }

        vector<size_t> retry_indices {};
        vector<uint8_t> read_completed {vector<uint8_t>(unique_ids.size())};
        vector<vector<shared_ptr<CommitOperationData>>> pending_ops {vector<vector<shared_ptr<CommitOperationData>>>(unique_ids.size())};

        {
            scoped_lock locker {_stateLocker};

            for (size_t request_index {0}; request_index < pending_indices.size(); request_index++) {
                size_t index {pending_indices[request_index]};

                if (_documentReadStates.at({collection_name, unique_ids[index]}).Generation != expected_generations[request_index]) {
                    retry_indices.emplace_back(index);
                }
                else {
                    read_completed[index] = 1;
                }
            }

            for (const auto& pending_op : _pendingCommitOperations) {
                if (pending_op->CollectionName != collection_name) {
                    continue;
                }

                if (auto it {unique_index_by_id.find(pending_op->RecordId)}; it != unique_index_by_id.end() && read_completed[it->second] != 0) {
                    pending_ops[it->second].emplace_back(pending_op);
                }
            }
        }

        if (!IsPendingReplayOverlayCurrent(replay_overlay)) {
            continue;
        }

        vector<vector<shared_ptr<CommitOperationData>>> replay_ops {vector<vector<shared_ptr<CommitOperationData>>>(unique_ids.size())};

        for (const auto& pending_op : replay_overlay.Operations) {
            if (pending_op->CollectionName != collection_name) {
                continue;
            }

            if (auto it {unique_index_by_id.find(pending_op->RecordId)}; it != unique_index_by_id.end() && read_completed[it->second] != 0) {
                replay_ops[it->second].emplace_back(pending_op);
            }
        }

        for (size_t i {0}; i < pending_indices.size(); i++) {
            size_t index {pending_indices[i]};

            if (read_completed[index] == 0) {
                continue;
            }

            AnyData::Document doc {std::move(records[i])};

            for (const auto& pending_op : replay_ops[index]) {
                if (pending_op->Type == CommitOperationType::Insert) {
                    if (!doc.Empty() && !AreDocumentsEqual(doc, pending_op->Doc)) {
                        throw DataBaseException("Pending database insert replay conflict", unique_ids[index], _settings->DataBase.OpLogPath);
                    }

                    doc = pending_op->Doc.Copy();
                }
                else if (pending_op->Type == CommitOperationType::Update) {
                    for (const auto& [key, value] : pending_op->Doc) {
                        doc.Assign(key, value.Copy());
                    }
                }
                else {
                    doc = {};
                }
            }

            for (const auto& pending_op : pending_ops[index]) {
                if (pending_op->Type == CommitOperationType::Insert) {
                    doc = pending_op->Doc.Copy();
                }
                else if (pending_op->Type == CommitOperationType::Update) {
                    for (const auto& [key, value] : pending_op->Doc) {
                        doc.Assign(key, value.Copy());
                    }
                }
                else if (pending_op->Type == CommitOperationType::Delete) {
                    doc = {};
                }
            }

            unique_docs[index] = std::move(doc);
        }

        pending_indices = std::move(retry_indices);
    }

    vector<size_t> first_doc_position {vector<size_t>(unique_ids.size(), ids.size())};

    for (size_t i {0}; i < ids.size(); i++) {
        size_t index {doc_unique_index[i]};

        if (first_doc_position[index] == ids.size()) {
            docs[i] = std::move(unique_docs[index]);
            first_doc_position[index] = i;
        }
        else {
            docs[i] = docs[first_doc_position[index]].Copy();
        }
    }

    return docs;
}

auto DataBaseImpl::GetRecords(hstring collection_name, const vector<DataBaseKey>& ids) const -> vector<AnyData::Document>
{
    return vec_transform(ids, [&](const DataBaseKey& id) -> AnyData::Document { return GetRecord(collection_name, id); });
}

void DataBaseImpl::Insert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc)
{
    FO_TRACE_ZONE(Database);

    scoped_lock mutation_locker {_mutationLocker};

    InsertInner(collection_name, id, doc);
}

auto DataBaseImpl::TryInsert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc) -> bool
{
    FO_TRACE_ZONE(Database);

    if (doc.Empty()) {
        throw DataBaseException("Cannot insert empty document");
    }

    ValidateFiniteAnyDocument(doc);

    scoped_lock mutation_locker {_mutationLocker};

    if (!GetDocument(collection_name, id).Empty()) {
        return false;
    }

    InsertInner(collection_name, id, doc);
    return true;
}

auto DataBaseImpl::CompareExchange(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& expected, const AnyData::Value& desired) -> bool
{
    FO_TRACE_ZONE(Database);

    ValidateFiniteAnyValue(desired);

    scoped_lock mutation_locker {_mutationLocker};

    auto doc {GetDocument(collection_name, id)};

    if (!doc.Contains(key) || doc[string {key}] != expected) {
        return false;
    }

    UpdateInner(collection_name, id, key, desired);
    return true;
}

void DataBaseImpl::InsertInner(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc)
{
    if (doc.Empty()) {
        throw DataBaseException("Cannot insert empty document");
    }

    ValidateFiniteAnyDocument(doc);
    ValidateCollectionKey(collection_name, id);

    {
        unique_lock locker {_stateLocker};

        while (_snapshotInProgress) {
            _snapshotDoneSignal.wait(locker);
        }

        auto op {safe_alloc::make_shared<CommitOperationData>()};
        op->Type = CommitOperationType::Insert;
        op->CollectionName = collection_name;
        op->RecordId = id;
        op->Doc = doc.Copy();
        _pendingCommitOperations.emplace_back(std::move(op));
    }

    _commitThreadSignal.notify_one();
}

void DataBaseImpl::Update(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& value)
{
    scoped_lock mutation_locker {_mutationLocker};

    UpdateInner(collection_name, id, key, value);
}

void DataBaseImpl::UpdateFields(hstring collection_name, const DataBaseKey& id, const AnyData::Document& changes)
{
    FO_TRACE_ZONE(Database);

    if (changes.Empty()) {
        throw DataBaseException("Cannot update with empty document");
    }

    ValidateFiniteAnyDocument(changes);
    ValidateCollectionKey(collection_name, id);
    scoped_lock mutation_locker {_mutationLocker};

    {
        unique_lock locker {_stateLocker};

        while (_snapshotInProgress) {
            _snapshotDoneSignal.wait(locker);
        }

        auto op {safe_alloc::make_shared<CommitOperationData>()};
        op->Type = CommitOperationType::Update;
        op->CollectionName = collection_name;
        op->RecordId = id;
        op->Doc = changes.Copy();
        _pendingCommitOperations.emplace_back(std::move(op));
    }

    _commitThreadSignal.notify_one();
}

void DataBaseImpl::UpdateInner(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& value)
{
    ValidateFiniteAnyValue(value);
    ValidateCollectionKey(collection_name, id);

    {
        unique_lock locker {_stateLocker};

        while (_snapshotInProgress) {
            _snapshotDoneSignal.wait(locker);
        }

        auto op {safe_alloc::make_shared<CommitOperationData>()};
        op->Type = CommitOperationType::Update;
        op->CollectionName = collection_name;
        op->RecordId = id;
        op->Doc.Assign(string(key), value.Copy());
        _pendingCommitOperations.emplace_back(std::move(op));
    }

    _commitThreadSignal.notify_one();
}

void DataBaseImpl::Delete(hstring collection_name, const DataBaseKey& id)
{
    FO_TRACE_ZONE(Database);

    scoped_lock mutation_locker {_mutationLocker};

    ValidateCollectionKey(collection_name, id);

    {
        unique_lock locker {_stateLocker};

        while (_snapshotInProgress) {
            _snapshotDoneSignal.wait(locker);
        }

        auto op {safe_alloc::make_shared<CommitOperationData>()};
        op->Type = CommitOperationType::Delete;
        op->CollectionName = collection_name;
        op->RecordId = id;
        _pendingCommitOperations.emplace_back(std::move(op));
    }

    _commitThreadSignal.notify_one();
}

void DataBaseImpl::StartCommitChanges()
{
    {
        scoped_lock locker {_stateLocker};

        _commitThreadActive = true;
    }

    _commitThreadSignal.notify_one();
}

void DataBaseImpl::WaitCommitChanges()
{
    FO_TRACE_ZONE(Database);

    unique_lock locker {_stateLocker};

    if (!_commitThreadActive) {
        return;
    }

    while (!_pendingCommitOperations.empty()) {
        if (!InValidState()) {
            logging::write("Database is not in valid state, pending commit operations can't be guaranteed to be durably committed");
            break;
        }

        _commitThreadDoneSignal.wait(locker);
    }
}

auto DataBaseImpl::CreateSnapshot() -> vector<uint8_t>
{
    FO_TRACE_ZONE(Database);

    {
        unique_lock locker {_stateLocker};

        while (_snapshotInProgress) {
            _snapshotDoneSignal.wait(locker);
        }

        if (!_commitThreadActive) {
            throw DataBaseException("Database snapshot requires an active commit thread");
        }

        while (!_pendingCommitOperations.empty()) {
            if (!InValidState()) {
                throw DataBaseException("Database snapshot cannot drain a failed backend");
            }

            _commitThreadDoneSignal.wait(locker);
        }

        if (!InValidState()) {
            throw DataBaseException("Database snapshot requires a valid backend");
        }

        _snapshotInProgress = true;
    }

    auto finish_snapshot {scope_exit([this]() noexcept {
        safe_call([this] {
            {
                scoped_lock locker {_stateLocker};

                _snapshotInProgress = false;
            }

            _snapshotDoneSignal.notify_all();
        });
    })};

    return CreateSnapshotData();
}

void DataBaseImpl::RestoreSnapshot(const_span<uint8_t> snapshot_data)
{
    FO_TRACE_ZONE(Database);

    scoped_lock mutation_locker {_mutationLocker};

    if (snapshot_data.empty()) {
        throw DataBaseException("Database snapshot data is empty");
    }

    {
        unique_lock locker {_stateLocker};

        while (_snapshotInProgress) {
            _snapshotDoneSignal.wait(locker);
        }

        if (!_pendingCommitOperations.empty()) {
            throw DataBaseException("Database snapshot restore requires a drained commit queue", _pendingCommitOperations.size());
        }
        if (!InValidState()) {
            throw DataBaseException("Database snapshot restore requires a valid backend");
        }

        _snapshotInProgress = true;
    }

    auto finish_restore {scope_exit([this]() noexcept {
        safe_call([this] {
            {
                scoped_lock locker {_stateLocker};

                _snapshotInProgress = false;
            }

            _snapshotDoneSignal.notify_all();
        });
    })};

    RestoreSnapshotData(snapshot_data);
}

auto DataBaseImpl::CreateSnapshotData() -> vector<uint8_t>
{
    throw DataBaseException("Database backend does not support snapshots");
}

void DataBaseImpl::RestoreSnapshotData(const_span<uint8_t> snapshot_data)
{
    throw DataBaseException("Database backend does not support snapshots", snapshot_data.size());
}

void DataBaseImpl::ClearChanges() noexcept
{
    scoped_lock mutation_locker {_mutationLocker};

    {
        scoped_lock locker {_stateLocker};

        if (_commitInProgress) {
            FO_STRONG_ASSERT(!_pendingCommitOperations.empty(), "In-flight database operation is missing from the queue");

            while (_pendingCommitOperations.size() > 1) {
                _pendingCommitOperations.pop_back();
            }
        }
        else {
            _pendingCommitOperations.clear();
        }
    }

    _commitThreadDoneSignal.notify_all();
}

void DataBaseImpl::DrawGui()
{
    constexpr ImGuiTableFlags TABLE_FLAGS {ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp};

    auto info_row {[](string_view key, string_view value) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGuiTextUnformatted(key);
        ImGui::TableSetColumnIndex(1);
        ImGuiTextUnformatted(value);
    }};

    size_t pending_count {0};
    bool commit_thread_active {false};

    {
        scoped_lock locker {_stateLocker};

        pending_count = _pendingCommitOperations.size();
        commit_thread_active = _commitThreadActive;
    }

    if (ImGui::BeginTable("##DbSummary", 2, TABLE_FLAGS)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        info_row("Valid state", strex("{}", InValidState()).str());
        info_row("Backend failed", strex("{}", _backendFailed.load()).str());
        info_row("Panic started", strex("{}", _panicStarted.load()).str());
        info_row("Op log enabled", strex("{}", _opLogEnabled).str());
        info_row("Commit thread active", strex("{}", commit_thread_active).str());
        info_row("Pending commit operations", strex("{}", pending_count).str());
        info_row("Pending changes panic threshold", strex("{}", _pendingChangesPanicThreshold).str());
        info_row("DB requests per minute", strex("{}", GetDbRequestsPerMinute()).str());
        info_row("Collections", strex("{}", _collectionKeyTypes.size()).str());

        if (_pendingChangesLog) {
            info_row("Pending changes log path", _pendingChangesLog->GetPath());
            info_row("Pending changes log lines", strex("{}", _pendingChangesLog->GetLinesCount()).str());
            info_row("Pending changes log size", strex("{}", _pendingChangesLog->GetTextSize()).str());
        }

        if (_committedChangesLog) {
            info_row("Committed changes log path", _committedChangesLog->GetPath());
            info_row("Committed changes log lines", strex("{}", _committedChangesLog->GetLinesCount()).str());
            info_row("Committed changes log size", strex("{}", _committedChangesLog->GetTextSize()).str());
        }

        ImGui::EndTable();
    }

    if (!_collectionKeyTypes.empty()) {
        string collections_label {strex("Collections ({})", _collectionKeyTypes.size()).str()};
        auto collections_label_cstr {make_ptr(collections_label.c_str())};

        if (ImGui::TreeNode(collections_label_cstr.get())) {
            if (ImGui::BeginTable("##DbCollections", 2, TABLE_FLAGS)) {
                ImGui::TableSetupColumn("Collection", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                ImGui::TableSetupColumn("Key type", ImGuiTableColumnFlags_WidthStretch);

                for (const auto& [collection_name, key_type] : _collectionKeyTypes) {
                    string_view key_type_str {key_type == DataBaseKeyType::IntId ? "IntId" : "String"};
                    info_row(collection_name, key_type_str);
                }

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }
    }
}

void DataBaseImpl::ScheduleCommit()
{
    bool should_notify {false};

    {
        scoped_lock locker {_stateLocker};

        if (!_pendingCommitOperations.empty()) {
            should_notify = true;
        }
    }

    if (should_notify) {
        _commitThreadSignal.notify_one();
    }
}

void DataBaseImpl::StartCommitThread()
{
    FO_VERIFY_AND_THROW(!_commitThread.joinable(), "Commit thread joinable is already set");

    {
        scoped_lock locker {_stateLocker};

        _commitThreadStopRequested = false;
    }

    _commitThread = run_thread("DataBaseCommitter", [this] { CommitThreadEntry(); });
}

void DataBaseImpl::StopCommitThread() noexcept
{
    FO_TRACE_ZONE(Database);

    try {
        FO_VERIFY_AND_THROW(_commitThread.joinable(), "Commit thread is not joinable");

        {
            scoped_lock locker {_stateLocker};

            _commitThreadStopRequested = true;
        }

        _commitThreadSignal.notify_one();
        _commitThread.join();
        _commitThread = {};
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

void DataBaseImpl::CommitThreadEntry() noexcept
{
    while (true) {
        try {
            bool has_changes {false};
            bool stop_requested {false};
            bool inactive_failure {false};
            bool deliver_panic {false};

            {
                unique_lock locker {_stateLocker};

                while ((!_commitThreadActive || _pendingCommitOperations.empty()) && !_commitThreadStopRequested && !_backendFailed) {
                    _commitThreadDoneSignal.notify_all();
                    _commitThreadSignal.wait(locker);
                }

                if (!_commitThreadActive) {
                    if (_commitThreadStopRequested) {
                        break;
                    }
                    if (_backendFailed) {
                        inactive_failure = true;
                        deliver_panic = ClaimPanic();
                    }
                    else {
                        continue;
                    }
                }

                has_changes = !_pendingCommitOperations.empty();
                stop_requested = _commitThreadStopRequested;
            }

            if (inactive_failure) {
                if (deliver_panic) {
                    DeliverPanic("Database backend is in failed state and commit thread is not active, can't recover");
                }

                break;
            }

            if (stop_requested) {
                while (has_changes) {
                    CommitNextChange();

                    scoped_lock locker {_stateLocker};

                    has_changes = !_pendingCommitOperations.empty();
                }

                break;
            }
            else if (_backendFailed && !_panicStarted && nanotime::now() >= _reconnectRetryTime) {
                _reconnectRetryTime = nanotime::now() + _reconnectRetryPeriod;

                try {
                    ScopedRecoveryWatch watch {this};

                    if (TryReconnect()) {
                        if (_opLogEnabled) {
                            RestorePendingChanges(watch.Get());
                        }
                        else {
                            CompleteRecovery(watch.Get());
                        }
                    }
                }
                catch (const std::exception& ex) {
                    exceptions::report_and_continue(ex);
                    StartPanic("Failed to restore pending changes after successful backend reconnection");
                }
            }
            else if (has_changes) {
                CommitNextChange();
            }
            else if (_backendFailed) {
                coarse_sleep(FAILED_BACKEND_POLL_INTERVAL);
            }
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
        }
    }

    _commitThreadDoneSignal.notify_all();
}

void DataBaseImpl::CommitNextChange() noexcept
{
    FO_TRACE_ZONE(Database);

    shared_ptr<CommitOperationData> op {};

    {
        scoped_lock locker {_stateLocker};

        if (_pendingCommitOperations.empty()) {
            return;
        }

        op = _pendingCommitOperations.front();
        FO_STRONG_ASSERT(!_commitInProgress, "Multiple database commit operations are in flight");
        _commitInProgress = true;
    }

    auto finish_commit {scope_exit([this]() noexcept {
        scoped_lock locker {_stateLocker};
        _commitInProgress = false;
    })};

    optional<string> serialized_op {};
    auto get_log_data {[&]() -> const string& {
        if (serialized_op) {
            return *serialized_op;
        }

        string key {EncodeStorageDbKey(op->RecordId, GetCollectionKeyType(op->CollectionName), GetStringKeyEscaping())};

        if (op->Type == CommitOperationType::Delete) {
            serialized_op = strex("delete {} {}\n", op->CollectionName.as_str(), key).str();
        }
        else {
            auto doc_json {DataBaseSerialization::ToJson(op->Doc).dump()};
            if (op->Type == CommitOperationType::Insert) {
                FO_VERIFY_AND_THROW(doc_json.find_first_of("\r\n") == string::npos, "Database insert oplog JSON contains a newline and cannot be stored as a single log command", op->CollectionName, doc_json.size(), doc_json.find_first_of("\r\n"));
            }
            else {
                FO_STRONG_ASSERT(op->Type == CommitOperationType::Update, "Unknown database commit operation type");
                FO_VERIFY_AND_THROW(doc_json.find_first_of("\r\n") == string::npos, "Database update oplog JSON contains a newline and cannot be stored as a single log command", op->CollectionName, doc_json.size(), doc_json.find_first_of("\r\n"));
            }

            string_view command {op->Type == CommitOperationType::Insert ? "insert" : "update"};
            serialized_op = strex("{} {} {} {}\n", command, op->CollectionName.as_str(), key, doc_json).str();
        }

        return *serialized_op;
    }};

    bool prelogged {false};

    if (!_backendFailed && _opLogEnabled && _pendingChangesLog && _pendingChangesLog->GetLinesCount() != 0) {
        try {
            auto result {_pendingChangesLog->AppendIfPending(get_log_data())};

            if (result == RecoveryLogHandle::ConditionalAppendResult::Failed) {
                if (_pendingChangesLog->IsUsable()) {
                    _reconnectRetryTime = nanotime::now();
                    MarkBackendFailed();
                }
                else {
                    StartPanic("Append failed before database commit");
                }

                return;
            }

            prelogged = result == RecoveryLogHandle::ConditionalAppendResult::Appended;
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
            StartPanic("Exception while preparing database commit journal");
            return;
        }
    }

    bool backend_write_completed {false};

    if (prelogged) {
        try {
            ScopedRecoveryWatch watch {this};
            RestorePendingChanges(watch.Get());
            backend_write_completed = true;
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
            _reconnectRetryTime = nanotime::now() + _reconnectRetryPeriod;
            MarkBackendFailed();
        }
    }

    if (!_backendFailed && !backend_write_completed) {
        try {
            auto storage_record_id {EncodeBackendDbKey(op->RecordId, GetCollectionKeyType(op->CollectionName), GetStringKeyEscaping())};

            ScopedBackendWrite backend_write {this};
            switch (op->Type) {
            case CommitOperationType::Insert:
                InsertRecord(op->CollectionName, storage_record_id, op->Doc);
                break;
            case CommitOperationType::Update:
                UpdateRecord(op->CollectionName, storage_record_id, op->Doc);
                break;
            case CommitOperationType::Delete:
                DeleteRecord(op->CollectionName, storage_record_id);
                break;
            }

            backend_write_completed = true;
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
            _reconnectRetryTime = nanotime::now() + _reconnectRetryPeriod;
            MarkBackendFailed();
        }
    }

    if (_backendFailed && !backend_write_completed) {
        try {
            if (!_opLogEnabled) {
                StartPanic("Database backend write failed and oplog is disabled, can't recover");
                return;
            }

            if (!prelogged && !_pendingChangesLog->Append(get_log_data())) {
                StartPanic("Append failed during commit failure handling");
                return;
            }
            if (_pendingChangesLog->GetTextSize() >= _pendingChangesPanicThreshold) {
                StartPanic("Oplog file exceeds configured panic threshold");
            }

            OnCommitOperationWrittenToOpLog();
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
            StartPanic("Exception during commit failure handling");
            return;
        }
    }

    try {
        RegisterDbRequests(1);
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
    }

    try {
        scoped_lock state_locker {_stateLocker};

        // The change stops being laid over reads here, so a reader that fetched the record before the write reads it again
        NoteCommittedRecordChange(op->CollectionName, op->RecordId, op->Type != CommitOperationType::Update);

        _pendingCommitOperations.pop_front();
        _commitInProgress = false;
        finish_commit.release();
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
        StartPanic("Exception during post-commit bookkeeping");
    }
}

void DataBaseImpl::NoteCommittedRecordChange(hstring collection_name, const DataBaseKey& id, bool key_set_may_change)
{
    if (auto it {_documentReadStates.find({collection_name, id})}; it != _documentReadStates.end()) {
        it->second.Generation++;
    }

    if (key_set_may_change) {
        _committedKeyChanges[collection_name]++;
    }
}

void DataBaseImpl::RegisterDbRequests(size_t request_count) const
{
    if (request_count == 0) {
        return;
    }

    auto now_second {std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count()};

    scoped_lock locker {_dbRequestsMetricLocker};

    auto& bucket {_dbRequestsPerMinuteBuckets[static_cast<size_t>(now_second % numeric_cast<int64_t>(_dbRequestsPerMinuteBuckets.size()))]};

    if (bucket.Second != now_second) {
        bucket.Second = now_second;
        bucket.Count = 0;
    }

    bucket.Count += request_count;

    size_t requests_per_minute {0};

    for (const auto& it : _dbRequestsPerMinuteBuckets) {
        if (it.Second > 0 && now_second - it.Second < 60) {
            requests_per_minute += it.Count;
        }
    }

    _dbRequestsPerMinute.store(requests_per_minute, std::memory_order_relaxed);
}

void DataBaseImpl::ValidateCollectionKey(hstring collection_name, const DataBaseKey& id) const
{
    if (!IsDbKeyValueValid(id)) {
        throw DataBaseException("Invalid database key value", collection_name, id);
    }

    auto expected_type {GetCollectionKeyType(collection_name)};

    if (GetDbKeyType(id) != expected_type) {
        throw DataBaseException("Invalid database key type for collection", collection_name, DbKeyTypeName(expected_type), id);
    }
}

auto DataBaseImpl::ClaimPanic() noexcept -> bool
{
    return !_panicStarted.exchange(true);
}

void DataBaseImpl::StartPanic(string_view message)
{
    if (!ClaimPanic()) {
        return;
    }

    DeliverPanic(message);
}

void DataBaseImpl::DeliverPanic(string_view message)
{
    logging::write("Critical database failure: {}", message);

    if (_panicCallback) {
        safe_call([this] { _panicCallback(); });
    }

    try {
        run_thread("Panic", [timeout = _panicShutdownTimeout.value()]() {
            coarse_sleep(timeout);
            fatal::report_and_exit("Database panic shutdown timed out");
        }).detach();
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
        fatal::report_and_exit("Database panic shutdown timer could not start");
    }
}

DataBaseImpl::RecoveryLogHandle::RecoveryLogHandle(string path) :
    _path {std::move(path)}
{
    FO_TRACE_ZONE(Database);

    if (_path.empty()) {
        throw DataBaseException("Empty recovery log file path");
    }

    _fd = osfile::open_exclusive_file(_path);

    if (_fd < 0) {
        throw DataBaseException("Failed to open recovery oplog file, it may be used by another process", _path);
    }

    auto fd_guard {scope_fail([this]() noexcept { osfile::close_exclusive_file(_fd); })};

    auto read_content {Read()};

    if (!read_content.has_value()) {
        throw DataBaseException("Oplog file can't be read", _path);
    }

    string normalized_content {strex(read_content.value()).normalize_line_endings().str()};

    if (!normalized_content.empty() && !normalized_content.ends_with('\n')) {
        throw DataBaseException("Oplog file has invalid line ending", _path);
    }

    _content = strex(normalized_content).split('\n');
    _textSize = normalized_content.size();

    fd_guard.release();
}

DataBaseImpl::RecoveryLogHandle::~RecoveryLogHandle() noexcept
{
    osfile::close_exclusive_file(_fd);
}

auto DataBaseImpl::RecoveryLogHandle::IsUsable() const noexcept -> bool
{
    scoped_lock locker {_locker};
    return !_ioFailed;
}

auto DataBaseImpl::RecoveryLogHandle::IsUsableAtRevision(uint64_t revision) const noexcept -> bool
{
    scoped_lock locker {_locker};
    return !_ioFailed && _revision == revision;
}

auto DataBaseImpl::RecoveryLogHandle::HasTruncatedSince(uint64_t revision) const noexcept -> bool
{
    scoped_lock locker {_locker};
    return _lastTruncationRevision > revision;
}

auto DataBaseImpl::RecoveryLogHandle::GetLinesCount() const noexcept -> size_t
{
    scoped_lock locker {_locker};
    return _content.size();
}

auto DataBaseImpl::RecoveryLogHandle::GetTextSize() const noexcept -> size_t
{
    scoped_lock locker {_locker};
    return _textSize;
}

auto DataBaseImpl::RecoveryLogHandle::SnapshotContent() const -> vector<string>
{
    scoped_lock locker {_locker};

    if (_ioFailed) {
        throw DataBaseException("Cannot snapshot an unusable recovery log");
    }

    return _content;
}

auto DataBaseImpl::RecoveryLogHandle::SnapshotContentWithRevision() const -> pair<vector<string>, uint64_t>
{
    scoped_lock locker {_locker};

    if (_ioFailed) {
        throw DataBaseException("Cannot snapshot an unusable recovery log");
    }

    return {_content, _revision};
}

auto DataBaseImpl::RecoveryLogHandle::Read() noexcept -> optional<string>
{
    int64_t size {osfile::seek_file_end(_fd)};

    if (size < 0) {
        return std::nullopt;
    }

    if (!osfile::seek_file_begin(_fd)) {
        return std::nullopt;
    }

    string content {};

    if (!std::in_range<size_t>(size)) {
        return std::nullopt;
    }

    size_t content_size {numeric_cast<size_t>(size)};

    if (content_size > content.max_size()) {
        return std::nullopt;
    }

    content.resize(content_size);

    size_t offset {0};

    while (offset < content.size()) {
        size_t remaining {content.size() - offset};
        auto read_pos {make_ptr(content.data() + offset)};

        int64_t read_size {osfile::read_file_chunk(_fd, read_pos, remaining)};

        if (read_size <= 0) {
            return std::nullopt;
        }

        offset += static_cast<size_t>(read_size);
    }

    if (osfile::seek_file_end(_fd) < 0) {
        return std::nullopt;
    }

    return content;
}

auto DataBaseImpl::RecoveryLogHandle::Truncate() noexcept -> bool
{
    FO_TRACE_ZONE(Database);

    scoped_lock locker {_locker};
    return TruncateLocked();
}

auto DataBaseImpl::RecoveryLogHandle::TruncateIfRevision(uint64_t expected_revision) noexcept -> bool
{
    scoped_lock locker {_locker};

    if (_revision != expected_revision) {
        return false;
    }

    return TruncateLocked();
}

auto DataBaseImpl::RecoveryLogHandle::TruncateLocked() noexcept -> bool
{
    if (_ioFailed) {
        return false;
    }

    FO_STRONG_ASSERT(_revision != std::numeric_limits<uint64_t>::max(), "Recovery log revision overflow");
    _ioFailed = true;

    if (!osfile::truncate_file(_fd)) {
        return false;
    }

    if (!osfile::seek_file_begin(_fd)) {
        return false;
    }

    if (!osfile::sync_file(_fd)) {
        return false;
    }

    _content.clear();
    _textSize = 0;
    _revision++;
    _lastTruncationRevision = _revision;
    _ioFailed = false;
    return true;
}

auto DataBaseImpl::RecoveryLogHandle::Append(string_view text) noexcept -> bool
{
    FO_TRACE_ZONE(Database);

    scoped_lock locker {_locker};
    return AppendLocked(text);
}

auto DataBaseImpl::RecoveryLogHandle::AppendIfPending(string_view text) noexcept -> ConditionalAppendResult
{
    FO_TRACE_ZONE(Database);

    scoped_lock locker {_locker};

    if (_ioFailed) {
        return ConditionalAppendResult::Failed;
    }

    if (_content.empty()) {
        return ConditionalAppendResult::NoPending;
    }

    if (text.empty() || !AppendLocked(text)) {
        return ConditionalAppendResult::Failed;
    }

    return ConditionalAppendResult::Appended;
}

auto DataBaseImpl::RecoveryLogHandle::AppendReplayProgress(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision, string_view text) noexcept -> ReplayAppendResult
{
    FO_TRACE_ZONE(Database);

    FO_STRONG_ASSERT(this != &committed, "Recovery logs must be distinct");
    scoped_lock locker {_locker};
    scoped_lock committed_locker {committed._locker};

    if (_ioFailed || committed._ioFailed) {
        return ReplayAppendResult::Failed;
    }
    if (_revision != expected_revision || committed._revision != expected_committed_revision) {
        return ReplayAppendResult::RevisionChanged;
    }
    if (!committed.AppendLocked(text)) {
        return ReplayAppendResult::Failed;
    }

    return ReplayAppendResult::Appended;
}

auto DataBaseImpl::RecoveryLogHandle::MarkCleanupAndTruncate(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision, uint64_t& cleanup_revision) noexcept -> bool
{
    FO_STRONG_ASSERT(this != &committed, "Recovery logs must be distinct");
    scoped_lock locker {_locker};
    scoped_lock committed_locker {committed._locker};

    if (_revision != expected_revision || committed._revision != expected_committed_revision || !AppendLocked("@reset-v2\n")) {
        return false;
    }

    if (!committed.AppendLocked("@reset-v2\n")) {
        return false;
    }

    cleanup_revision = committed._revision;
    return TruncateLocked();
}

auto DataBaseImpl::RecoveryLogHandle::CompleteMarkedCleanup(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision, bool add_committed_marker, uint64_t& cleanup_revision) noexcept -> bool
{
    FO_STRONG_ASSERT(this != &committed, "Recovery logs must be distinct");
    scoped_lock locker {_locker};
    scoped_lock committed_locker {committed._locker};

    if (_ioFailed || committed._ioFailed || _revision != expected_revision || committed._revision != expected_committed_revision) {
        return false;
    }

    if (add_committed_marker && !committed.AppendLocked("@reset-v2\n")) {
        return false;
    }

    cleanup_revision = committed._revision;
    return TruncateLocked();
}

auto DataBaseImpl::RecoveryLogHandle::CompleteLegacyCleanup(RecoveryLogHandle& committed, uint64_t expected_revision, uint64_t expected_committed_revision) noexcept -> bool
{
    FO_STRONG_ASSERT(this != &committed, "Recovery logs must be distinct");
    scoped_lock locker {_locker};
    scoped_lock committed_locker {committed._locker};

    if (_ioFailed || committed._ioFailed || _revision != expected_revision || committed._revision != expected_committed_revision || !_content.empty() || committed._content.empty() || committed._content.back() != "@reset-v1") {
        return false;
    }

    return TruncateLocked() && committed.TruncateLocked();
}

auto DataBaseImpl::RecoveryLogHandle::AppendLocked(string_view text) noexcept -> bool
{
    if (_fd < 0 || _ioFailed) {
        return false;
    }

    if (text.empty()) {
        return true;
    }

    if (!_content.empty() && _content.back() == "@reset-v2") {
        return false;
    }

    string normalized_content {strex(text).normalize_line_endings().str()};

    if (!normalized_content.ends_with('\n')) {
        return false;
    }

    vector<string> parsed_content {};
    size_t line_begin {0};

    while (line_begin < normalized_content.size()) {
        auto line_end {normalized_content.find('\n', line_begin)};

        if (line_end == string::npos || line_end == line_begin) {
            return false;
        }

        auto line {string_view {normalized_content}.substr(line_begin, line_end - line_begin)};

        if (line == "@reset-v2" && line_begin == 0 && line_end + 1 == normalized_content.size() && !_content.empty() && _content.back() != "@reset-v2") {
            parsed_content.emplace_back(line);
            line_begin = line_end + 1;
            continue;
        }

        auto first_space {line.find(' ')};
        size_t second_space {first_space != string_view::npos ? line.find(' ', first_space + 1) : string_view::npos};
        size_t third_space {second_space != string_view::npos ? line.find(' ', second_space + 1) : string_view::npos};

        if (first_space == string_view::npos || first_space == 0 || second_space == string_view::npos || second_space == first_space + 1) {
            return false;
        }

        auto command {line.substr(0, first_space)};
        string_view other {};
        bool has_other {false};

        if (third_space == string_view::npos) {
            if (second_space + 1 >= line.size()) {
                return false;
            }
        }
        else {
            if (third_space == second_space + 1 || third_space + 1 >= line.size()) {
                return false;
            }

            other = line.substr(third_space + 1);
            has_other = true;
        }

        if (command != "insert" && command != "update" && command != "delete") {
            return false;
        }
        if ((command == "insert" || command == "update") && !has_other) {
            return false;
        }
        if (command == "delete" && has_other) {
            return false;
        }
        if (has_other && (other.find('\n') != string_view::npos || other.find('\r') != string_view::npos)) {
            return false;
        }

        parsed_content.emplace_back(line);
        line_begin = line_end + 1;
    }

    FO_STRONG_ASSERT(_revision != std::numeric_limits<uint64_t>::max(), "Recovery log revision overflow");
    _ioFailed = true;

    if (osfile::seek_file_end(_fd) < 0) {
        return false;
    }

    size_t offset {0};

    while (offset < normalized_content.size()) {
        size_t remaining {normalized_content.size() - offset};
        auto write_pos {make_ptr(normalized_content.data() + offset)};

        int64_t written_size {osfile::write_file_chunk(_fd, write_pos, remaining)};

        if (written_size <= 0) {
            return false;
        }

        offset += static_cast<size_t>(written_size);
    }

    if (!osfile::sync_file(_fd)) {
        return false;
    }

    for (auto& line : parsed_content) {
        _content.emplace_back(std::move(line));
    }

    _textSize += normalized_content.size();
    _revision++;
    _ioFailed = false;

    return true;
}

auto ConnectToDataBase(ptr<DataBaseSettings> db_settings, string_view connection_info, const DataBaseCollectionSchemas& collection_schemas, DataBasePanicCallback panic_callback) -> DataBase
{
    FO_TRACE_ZONE(Database);

    auto finish_connect {[&](unique_ptr<DataBaseImpl> impl) -> DataBase {
        impl->InitializeCollections(collection_schemas);
        impl->InitializeOpLogs();
        impl->RestorePendingChanges();
        return DataBase(std::move(impl));
    }};

    connection_info = strvex(connection_info).trim();

    if (auto options {strvex(connection_info).split(' ')}; !options.empty()) {
        logging::write("Connect to {} data base", options.front());
        string_view storage_dir {static_cast<string_view>(strvex(connection_info.substr(options.front().size())).trim())};
        ignore_unused(storage_dir);

#if FO_DATABASE_JSON
        if (options.front() == "JSON" && !storage_dir.empty()) {
            return finish_connect(DataBaseJson::Create(db_settings, storage_dir, std::move(panic_callback)));
        }
#else
        if (options.front() == "JSON") {
            throw DataBaseException("Database provider is disabled; enable FO_DATABASE_JSON");
        }
#endif
#if FO_DATABASE_SQLITE
        if (options.front() == "DbSQLite" && !storage_dir.empty()) {
            return finish_connect(DataBaseSQLite::Create(db_settings, storage_dir, std::move(panic_callback)));
        }
#else
        if (options.front() == "DbSQLite") {
            throw DataBaseException("Database provider is disabled; enable FO_DATABASE_SQLITE");
        }
#endif
#if FO_DATABASE_MONGO
        if (options.front() == "Mongo" && options.size() == 3) {
            return finish_connect(DataBaseMongo::Create(db_settings, options[1], options[2], std::move(panic_callback)));
        }
#else
        if (options.front() == "Mongo") {
            throw DataBaseException("Database provider is disabled; enable FO_DATABASE_MONGO");
        }
#endif
        if (options.front() == "Memory" && options.size() == 1) {
            return finish_connect(DataBaseMemory::Create(db_settings, std::move(panic_callback)));
        }
    }

    throw DataBaseException("Wrong storage options", connection_info);
}

static void ValidateFiniteAnyValue(const AnyData::Value& value)
{
    switch (value.Type()) {
    case AnyData::ValueType::Float64:
        if (!std::isfinite(value.AsDouble())) {
            throw DataBaseException("Database value is not finite", value.AsDouble());
        }

        break;
    case AnyData::ValueType::Array:
        for (const AnyData::Value& array_value : value.AsArray()) {
            ValidateFiniteAnyValue(array_value);
        }

        break;
    case AnyData::ValueType::Dict:
        for (const auto& dict_entry : value.AsDict()) {
            ValidateFiniteAnyValue(dict_entry.second);
        }

        break;
    case AnyData::ValueType::Int64:
    case AnyData::ValueType::Bool:
    case AnyData::ValueType::String:
        break;
    default:
        break;
    }
}

static void ValidateFiniteAnyDocument(const AnyData::Document& doc)
{
    for (const auto& doc_entry : doc) {
        ValidateFiniteAnyValue(doc_entry.second);
    }
}

static auto AreDocumentsEqual(const AnyData::Document& left, const AnyData::Document& right) -> bool
{
    return left == right;
}

static auto DoesDocumentContain(const AnyData::Document& target, const AnyData::Document& patch) -> bool
{
    for (auto&& [patch_key, patch_value] : patch) {
        if (!target.Contains(patch_key)) {
            return false;
        }
        if (!(target[patch_key] == patch_value)) {
            return false;
        }
    }

    return true;
}

static auto IsDbKeyValueValid(const DataBaseKey& key) noexcept -> bool
{
    return std::visit(
        [](const auto& value) noexcept -> bool {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, ident_t>) {
                return value != ident_t {};
            }
            else {
                return !value.empty() && strvex(value).is_valid_utf8();
            }
        },
        key);
}

auto GetDbKeyType(const DataBaseKey& key) noexcept -> DataBaseKeyType
{
    return std::holds_alternative<ident_t>(key) ? DataBaseKeyType::IntId : DataBaseKeyType::String;
}

static auto DbKeyTypeName(DataBaseKeyType key_type) noexcept -> string_view
{
    switch (key_type) {
    case DataBaseKeyType::IntId:
        return "Id";
    case DataBaseKeyType::String:
        return "String";
    }

    return "Unknown";
}

static auto EncodeStorageDbKey(const DataBaseKey& key, DataBaseKeyType key_type, DataBaseStringKeyEscaping escaping) -> string
{
    if (GetDbKeyType(key) != key_type) {
        throw DataBaseException("Database key type mismatch", DbKeyTypeName(key_type), key);
    }

    return std::visit(
        [key_type, escaping](const auto& value) -> string {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, ident_t>) {
                FO_VERIFY_AND_THROW(key_type == DataBaseKeyType::IntId, "Database storage key expected a numeric identifier but the collection key type differs", DbKeyTypeName(key_type), value);
                FO_VERIFY_AND_THROW(value != ident_t {}, "Database storage key cannot encode an empty identifier", DbKeyTypeName(key_type));
                return strex("{}", value).str();
            }
            else {
                FO_VERIFY_AND_THROW(key_type == DataBaseKeyType::String, "Database storage key expected a string identifier but the collection key type differs", DbKeyTypeName(key_type), value);
                FO_VERIFY_AND_THROW(!value.empty(), "Database storage key cannot encode an empty string identifier", DbKeyTypeName(key_type), escaping);
                return EncodeDbStringKey(value, escaping);
            }
        },
        key);
}

static auto DecodeStorageDbKey(string_view key_str, DataBaseKeyType key_type, DataBaseStringKeyEscaping escaping) -> DataBaseKey
{
    if (key_str.empty()) {
        throw DataBaseException("Invalid database key value", key_str);
    }

    if (key_type == DataBaseKeyType::IntId) {
        if (!strvex(key_str).is_number()) {
            throw DataBaseException("Invalid database numeric key format", key_str);
        }

        int64_t id_value {strvex(key_str).to_int64()};

        if (id_value <= 0) {
            throw DataBaseException("Invalid database numeric key value", key_str);
        }

        return ident_t {id_value};
    }

    string decoded_value {DecodeDbStringKey(key_str, escaping)};

    if (decoded_value.empty()) {
        throw DataBaseException("Invalid database string key value", key_str);
    }
    if (!strvex(decoded_value).is_valid_utf8()) {
        throw DataBaseException("Invalid database string key utf8", key_str);
    }

    return decoded_value;
}

static auto EncodeBackendDbKey(const DataBaseKey& key, DataBaseKeyType key_type, DataBaseStringKeyEscaping escaping) -> DataBaseKey
{
    if (key_type == DataBaseKeyType::IntId) {
        return key;
    }

    if (GetDbKeyType(key) != key_type) {
        throw DataBaseException("Database key type mismatch", DbKeyTypeName(key_type), key);
    }

    const auto& value {std::get<string>(key)};
    FO_VERIFY_AND_THROW(!value.empty(), "Backend database key cannot encode an empty string identifier", DbKeyTypeName(key_type), escaping);

    if (!strvex(value).is_valid_utf8()) {
        throw DataBaseException("Invalid database string key utf8", value);
    }

    if (escaping == DataBaseStringKeyEscaping::Raw) {
        return string(value);
    }

    return EncodeDbStringKey(value, escaping);
}

static auto DecodeBackendDbKey(const DataBaseKey& key, DataBaseKeyType key_type, DataBaseStringKeyEscaping escaping) -> DataBaseKey
{
    if (key_type == DataBaseKeyType::IntId) {
        return key;
    }

    if (GetDbKeyType(key) != key_type) {
        throw DataBaseException("Database collection returned invalid key type", DbKeyTypeName(key_type), key);
    }

    const auto& value {std::get<string>(key)};
    FO_VERIFY_AND_THROW(!value.empty(), "Backend database key cannot decode an empty string identifier", DbKeyTypeName(key_type), escaping);

    if (escaping == DataBaseStringKeyEscaping::Raw) {
        if (!strvex(value).is_valid_utf8()) {
            throw DataBaseException("Invalid database string key utf8", value);
        }

        return string(value);
    }

    string decoded_value {DecodeDbStringKey(value, escaping)};

    if (!strvex(decoded_value).is_valid_utf8()) {
        throw DataBaseException("Invalid database string key utf8", value);
    }

    return decoded_value;
}

static auto EncodeDbStringKey(string_view value, DataBaseStringKeyEscaping escaping) -> string
{
    static constexpr char hex_digits[] {"0123456789abcdef"};

    if (escaping == DataBaseStringKeyEscaping::Hex) {
        string result {};
        result.reserve(2 + value.size() * 2);
        result += "s_";

        for (auto ch : value) {
            uint8_t byte {static_cast<uint8_t>(ch)};
            result.push_back(hex_digits[byte >> 4]);
            result.push_back(hex_digits[byte & 0x0F]);
        }

        return result;
    }

    string result {};
    result.reserve(value.size());

    for (auto ch : value) {
        uint8_t byte {static_cast<uint8_t>(ch)};

        if (!ShouldEscapeDbStringByte(byte, escaping)) {
            result.push_back(ch);
            continue;
        }

        result.push_back('%');
        result.push_back(hex_digits[byte >> 4]);
        result.push_back(hex_digits[byte & 0x0F]);
    }

    return result;
}

static auto DecodeDbStringKey(string_view value, DataBaseStringKeyEscaping escaping) -> string
{
    if (escaping == DataBaseStringKeyEscaping::Hex) {
        if (!value.starts_with("s_")) {
            throw DataBaseException("Invalid database string key format", value);
        }

        auto encoded_value {value.substr(2)};

        if (encoded_value.empty() || encoded_value.size() % 2 != 0) {
            throw DataBaseException("Invalid database string key value", value);
        }

        string decoded_value {};
        decoded_value.reserve(encoded_value.size() / 2);

        for (size_t i {0}; i < encoded_value.size(); i += 2) {
            uint8_t high {DecodeHexDigit(encoded_value[i])};
            uint8_t low {DecodeHexDigit(encoded_value[i + 1])};
            decoded_value.push_back(static_cast<char>((high << 4) | low));
        }

        return decoded_value;
    }

    string decoded_value {};
    decoded_value.reserve(value.size());

    for (size_t i {0}; i < value.size(); i++) {
        if (value[i] != '%') {
            decoded_value.push_back(value[i]);
            continue;
        }

        if (i + 2 >= value.size()) {
            throw DataBaseException("Invalid database string key value", value);
        }

        uint8_t high {DecodeHexDigit(value[i + 1])};
        uint8_t low {DecodeHexDigit(value[i + 2])};
        decoded_value.push_back(static_cast<char>((high << 4) | low));
        i += 2;
    }

    return decoded_value;
}

static auto ShouldEscapeDbStringByte(uint8_t byte, DataBaseStringKeyEscaping escaping) noexcept -> bool
{
    if (byte == '%') {
        return true;
    }

    switch (escaping) {
    case DataBaseStringKeyEscaping::Raw:
        return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n';
    case DataBaseStringKeyEscaping::File:
        return byte < 32 || byte == ' ' || byte == '\\' || byte == '/' || byte == ':' || byte == '*' || byte == '?' || byte == '"' || byte == '<' || byte == '>' || byte == '|';
    case DataBaseStringKeyEscaping::Hex:
        return true;
    }

    return true;
}

static auto DecodeHexDigit(char ch) -> uint8_t
{
    if (ch >= '0' && ch <= '9') {
        return static_cast<uint8_t>(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f') {
        return static_cast<uint8_t>(10 + ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return static_cast<uint8_t>(10 + ch - 'A');
    }

    throw DataBaseException("Invalid database key hex digit", string_view {&ch, 1});
}

FO_END_NAMESPACE
