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
//

#include "DataBase.h"
#include "ImGuiStuff.h"

#if FO_WINDOWS
#include <fcntl.h>
#include <io.h>
#include <share.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

FO_DISABLE_WARNINGS_PUSH()
#include <bson/bson.h>
FO_DISABLE_WARNINGS_POP()

#include <json.hpp>

#include "WinApiUndef-Include.h"

FO_BEGIN_NAMESPACE

static auto AnyDocumentToJson(const AnyData::Document& doc) -> nlohmann::json;
static auto JsonToAnyDocument(const nlohmann::json& doc_json) -> AnyData::Document;
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
auto DataBase::operator=(DataBase&&) noexcept -> DataBase& = default;
DataBase::~DataBase() = default;

DataBase::DataBase(DataBaseImpl* impl) :
    _impl {impl}
{
    FO_STACK_TRACE_ENTRY();
}

auto DataBase::InValidState() const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _impl->InValidState();
}

auto DataBase::GetDbRequestsPerMinute() const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return _impl->GetDbRequestsPerMinute();
}

auto DataBase::GetAllIds(hstring collection_name) const -> vector<DataBaseKey>
{
    FO_STACK_TRACE_ENTRY();

    const auto key_type = _impl->GetCollectionKeyType(collection_name);
    auto ids = _impl->GetAllRecordIds(collection_name);

    for (auto& id : ids) {
        id = DecodeBackendDbKey(id, key_type, _impl->GetStringKeyEscaping());

        if (GetDbKeyType(id) != key_type) {
            throw DataBaseException("Database collection returned invalid key type", collection_name, id, DbKeyTypeName(key_type));
        }
    }

    return ids;
}

auto DataBase::GetAllIntIds(hstring collection_name) const -> vector<ident_t>
{
    FO_STACK_TRACE_ENTRY();

    const auto key_type = _impl->GetCollectionKeyType(collection_name);

    if (key_type != DataBaseKeyType::IntId) {
        throw DataBaseException("Invalid database collection key type", collection_name, DbKeyTypeName(DataBaseKeyType::IntId), DbKeyTypeName(key_type));
    }

    vector<ident_t> typed_ids;

    for (const auto& id : GetAllIds(collection_name)) {
        typed_ids.emplace_back(std::get<ident_t>(id));
    }

    return typed_ids;
}

auto DataBase::GetAllStringIds(hstring collection_name) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    const auto key_type = _impl->GetCollectionKeyType(collection_name);

    if (key_type != DataBaseKeyType::String) {
        throw DataBaseException("Invalid database collection key type", collection_name, DbKeyTypeName(DataBaseKeyType::String), DbKeyTypeName(key_type));
    }

    vector<string> typed_ids;

    for (const auto& id : GetAllIds(collection_name)) {
        typed_ids.emplace_back(std::get<string>(id));
    }

    return typed_ids;
}

auto DataBase::Get(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document
{
    FO_STACK_TRACE_ENTRY();

    return _impl->GetDocument(collection_name, id);
}

auto DataBase::Valid(hstring collection_name, const DataBaseKey& id) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return !_impl->GetDocument(collection_name, id).Empty();
}

void DataBase::Insert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc)
{
    FO_STACK_TRACE_ENTRY();

    _impl->Insert(collection_name, id, doc);
}

void DataBase::Update(hstring collection_name, const DataBaseKey& id, string_view key, const AnyData::Value& value)
{
    FO_STACK_TRACE_ENTRY();

    _impl->Update(collection_name, id, key, value);
}

void DataBase::Delete(hstring collection_name, const DataBaseKey& id)
{
    FO_STACK_TRACE_ENTRY();

    _impl->Delete(collection_name, id);
}

void DataBase::StartCommitChanges()
{
    FO_STACK_TRACE_ENTRY();

    _impl->StartCommitChanges();
}

void DataBase::WaitCommitChanges()
{
    FO_STACK_TRACE_ENTRY();

    _impl->WaitCommitChanges();
}

void DataBase::ClearChanges() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _impl->ClearChanges();
}

void DataBase::DrawGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!InValidState()) {
        ImGui::TextColored(ImVec4 {1.0f, 0.4f, 0.4f, 1.0f}, "Database is in failed state");
        return;
    }

    _impl->DrawGui();
}

DataBaseImpl::DataBaseImpl(DataBaseSettings& db_settings, DataBasePanicCallback panic_callback) :
    _settings {&db_settings},
    _opLogEnabled {_settings->OpLogEnabled},
    _pendingChangesPanicThreshold {numeric_cast<size_t>(_settings->PanicOpLogSizeThreshold)},
    _panicShutdownTimeout {std::chrono::milliseconds {_settings->PanicShutdownTimeout}},
    _reconnectRetryPeriod {std::chrono::milliseconds {std::max(_settings->ReconnectRetryPeriod, 1)}},
    _panicCallback {std::move(panic_callback)}
{
    FO_STACK_TRACE_ENTRY();
}

auto DataBaseImpl::GetCollectionKeyType(hstring collection_name) const -> DataBaseKeyType
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _collectionKeyTypes.find(collection_name);

    if (it == _collectionKeyTypes.end()) {
        throw DataBaseException("Unknown database collection", collection_name);
    }

    return it->second;
}

auto DataBaseImpl::ResolveCollectionName(string_view collection_name) const -> hstring
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _collectionNames.find(collection_name);

    if (it == _collectionNames.end()) {
        throw DataBaseException("Unknown database collection name", collection_name);
    }

    return it->second;
}

void DataBaseImpl::InitializeCollections(const DataBaseCollectionSchemas& collection_schemas)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& [collection_name, key_type] : collection_schemas) {
        FO_RUNTIME_ASSERT(!_collectionNames.contains(collection_name.as_str()));
        _collectionNames.emplace(collection_name.as_str(), collection_name);
        _collectionKeyTypes.emplace(collection_name, key_type);
        EnsureCollection(collection_name, key_type);
    }
}

void DataBaseImpl::InitializeOpLogs()
{
    FO_STACK_TRACE_ENTRY();

    if (!_opLogEnabled) {
        return;
    }

    if (_settings->OpLogPath.empty()) {
        throw DataBaseException("Empty oplog path in settings");
    }

    const auto open_log_file = [&](string_view file_path, string_view file_desc) {
        if (file_path.empty()) {
            throw DataBaseException("Empty oplog path", file_desc);
        }

        const auto dir = strex(file_path).extract_dir().str();

        if (!dir.empty() && !fs_create_directories(dir)) {
            throw DataBaseException("Oplog directory can't be created", file_desc, dir);
        }

        auto handle = SafeAlloc::MakeUnique<RecoveryLogHandle>(string(file_path));

        // Validate oplog line format
        const auto& content = handle->GetContent();

        for (size_t i = 0; i < handle->GetLinesCount(); i++) {
            const auto line = string_view {content[i]};
            const auto first_space = line.find(' ');
            const auto second_space = first_space != string_view::npos ? line.find(' ', first_space + 1) : string_view::npos;
            const auto third_space = second_space != string_view::npos ? line.find(' ', second_space + 1) : string_view::npos;

            if (first_space == string_view::npos || first_space == 0 || second_space == string_view::npos || second_space == first_space + 1) {
                throw DataBaseException("Oplog file has invalid command format", i + 1, file_path);
            }

            const auto command = line.substr(0, first_space);
            const auto collection = line.substr(first_space + 1, second_space - first_space - 1);
            string_view record_id_str {};
            string_view other {};
            bool has_other = false;

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
                const auto collection_name = ResolveCollectionName(collection);
                const auto record_id = DecodeStorageDbKey(record_id_str, GetCollectionKeyType(collection_name), GetStringKeyEscaping());

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

        return std::move(handle);
    };

    _pendingChangesLog = open_log_file(_settings->OpLogPath, "pending database changes file");
    _committedChangesLog = open_log_file(strex(_settings->OpLogPath).replace(".oplog", "-committed.oplog").str(), "committed database changes file");

    if (_committedChangesLog->GetContent().size() > _pendingChangesLog->GetContent().size()) {
        throw DataBaseException("Committed database changes file line count is greater than pending database changes file line count");
    }
}

void DataBaseImpl::RestorePendingChanges()
{
    FO_STACK_TRACE_ENTRY();

    if (!_opLogEnabled) {
        return;
    }

    FO_RUNTIME_ASSERT(_pendingChangesLog);
    FO_RUNTIME_ASSERT(_committedChangesLog);

    const auto& pending_changes_content = _pendingChangesLog->GetContent();
    const auto& committed_changes_content = _committedChangesLog->GetContent();

    if (committed_changes_content.size() > pending_changes_content.size()) {
        throw DataBaseException("Committed database changes file has more lines than pending database changes file");
    }

    for (size_t i = 0; i < committed_changes_content.size(); i++) {
        FO_RUNTIME_ASSERT(i < pending_changes_content.size());
        const size_t line_index = i + 1;

        if (pending_changes_content[i] != committed_changes_content[i]) {
            throw DataBaseException("Committed oplog line doesn't match pending oplog line", line_index, _settings->OpLogPath);
        }
    }

    const size_t pending_commands_to_replay = pending_changes_content.size() - committed_changes_content.size();
    size_t replayed_commands = 0;

    try {
        for (size_t i = committed_changes_content.size(); i < pending_changes_content.size(); i++) {
            const auto& line = pending_changes_content[i];
            const auto line_view = string_view {line};
            const auto first_space = line_view.find(' ');
            const auto second_space = line_view.find(' ', first_space + 1);
            FO_RUNTIME_ASSERT(first_space != string_view::npos && first_space != 0);
            FO_RUNTIME_ASSERT(second_space != string_view::npos && second_space != first_space + 1);

            const auto command = line_view.substr(0, first_space);
            const auto collection = line_view.substr(first_space + 1, second_space - first_space - 1);
            string_view record_id_str {};
            string_view other {};

            const auto third_space = line_view.find(' ', second_space + 1);

            if (third_space == string_view::npos) {
                FO_RUNTIME_ASSERT(second_space + 1 < line_view.size());
                record_id_str = line_view.substr(second_space + 1);
            }
            else {
                FO_RUNTIME_ASSERT(third_space != second_space + 1 && third_space + 1 < line_view.size());
                record_id_str = line_view.substr(second_space + 1, third_space - second_space - 1);
                other = line_view.substr(third_space + 1);
            }

            const auto collection_name = ResolveCollectionName(collection);
            const auto key_type = GetCollectionKeyType(collection_name);
            const auto record_id = DecodeStorageDbKey(record_id_str, key_type, GetStringKeyEscaping());
            const auto storage_record_id = EncodeBackendDbKey(record_id, key_type, GetStringKeyEscaping());
            const auto current_doc = GetRecord(collection_name, storage_record_id);

            if (command == "delete") {
                if (!current_doc.Empty()) {
                    DeleteRecord(collection_name, storage_record_id);
                }
            }
            else {
                auto doc = JsonToAnyDocument(nlohmann::json::parse(other));

                if (command == "insert") {
                    if (current_doc.Empty()) {
                        InsertRecord(collection_name, storage_record_id, doc);
                    }
                    else if (!AreDocumentsEqual(current_doc, doc)) {
                        throw DataBaseException("Pending database insert replay conflict", record_id, _settings->OpLogPath);
                    }
                }
                else if (!DoesDocumentContain(current_doc, doc)) {
                    UpdateRecord(collection_name, storage_record_id, doc);
                }
            }

            if (!_committedChangesLog->Append(strex("{}\n", line).str())) {
                throw DataBaseException("Committed pending database changes file append failed during restore");
            }

            if (++replayed_commands % 100 == 0) {
                WriteLog("Replayed {}/{} pending database commands", replayed_commands, pending_commands_to_replay);
            }
        }
    }
    catch (const DataBaseException&) {
        throw;
    }
    catch (const std::exception& ex) {
        throw DataBaseException("Pending database command parsing failed", ex.what(), _settings->OpLogPath);
    }

    FO_RUNTIME_ASSERT(_pendingChangesLog->GetLinesCount() == _committedChangesLog->GetLinesCount());
    FO_RUNTIME_ASSERT(_pendingChangesLog->GetContent() == _committedChangesLog->GetContent());
    WriteLog("Pending database changes successfully restored, total {} commands replayed", replayed_commands);

    if (!_pendingChangesLog->Truncate()) {
        throw DataBaseException("Pending database changes file can't be truncated after successful restore", _settings->OpLogPath);
    }
    if (!_committedChangesLog->Truncate()) {
        throw DataBaseException("Committed pending database changes file can't be truncated after successful restore", strex(_settings->OpLogPath).replace(".oplog", "-committed.oplog").str());
    }

    OnPendingChangesRestored();
}

auto DataBaseImpl::InValidState() const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return !_backendFailed.load(std::memory_order_relaxed) && !_panicStarted.load(std::memory_order_relaxed);
}

auto DataBaseImpl::GetDbRequestsPerMinute() const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return _dbRequestsPerMinute.load(std::memory_order_relaxed);
}

auto DataBaseImpl::GetDocument(hstring collection_name, const DataBaseKey& id) const -> AnyData::Document
{
    FO_STACK_TRACE_ENTRY();

    if (!InValidState()) {
        throw DataBaseException("Database backend is in failed state");
    }

    ValidateCollectionKey(collection_name, id);
    const auto storage_id = EncodeBackendDbKey(id, GetCollectionKeyType(collection_name), GetStringKeyEscaping());

    {
        std::scoped_lock locker {_stateLocker};

        _docReadRetryMarkers.emplace(collection_name, id);
    }

    auto release_reader = scope_exit([&]() noexcept {
        safe_call([&]() {
            std::scoped_lock locker {_stateLocker};

            _docReadRetryMarkers.erase({collection_name, id});
        });
    });

    while (true) {
        AnyData::Document doc;

        try {
            doc = GetRecord(collection_name, storage_id);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            _backendFailed = true;
            throw DataBaseException("Database backend failed to get document", ex.what());
        }

        RegisterDbRequests(1);

        vector<shared_ptr<CommitOperationData>> pending_ops;

        {
            std::scoped_lock locker {_stateLocker};

            if (_docReadRetryMarkers.erase({collection_name, id}) == 0) {
                _docReadRetryMarkers.emplace(collection_name, id);
                continue;
            }

            for (const auto& pending_op : _pendingCommitOperations) {
                if (pending_op->CollectionName == collection_name && pending_op->RecordId == id) {
                    pending_ops.emplace_back(pending_op);
                }
            }
        }

        for (const auto& pending_op : pending_ops) {
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

        return doc;
    }
}

void DataBaseImpl::Insert(hstring collection_name, const DataBaseKey& id, const AnyData::Document& doc)
{
    FO_STACK_TRACE_ENTRY();

    if (doc.Empty()) {
        throw DataBaseException("Cannot insert empty document");
    }

    ValidateCollectionKey(collection_name, id);

    {
        std::scoped_lock locker {_stateLocker};

        auto op = SafeAlloc::MakeShared<CommitOperationData>();
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
    FO_STACK_TRACE_ENTRY();

    ValidateCollectionKey(collection_name, id);

    {
        std::scoped_lock locker {_stateLocker};

        auto op = SafeAlloc::MakeShared<CommitOperationData>();
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
    FO_STACK_TRACE_ENTRY();

    ValidateCollectionKey(collection_name, id);

    {
        std::scoped_lock locker {_stateLocker};

        auto op = SafeAlloc::MakeShared<CommitOperationData>();
        op->Type = CommitOperationType::Delete;
        op->CollectionName = collection_name;
        op->RecordId = id;
        _pendingCommitOperations.emplace_back(std::move(op));
    }

    _commitThreadSignal.notify_one();
}

void DataBaseImpl::StartCommitChanges()
{
    FO_STACK_TRACE_ENTRY();

    {
        std::scoped_lock locker {_stateLocker};

        _commitThreadActive = true;
    }

    _commitThreadSignal.notify_one();
}

void DataBaseImpl::WaitCommitChanges()
{
    FO_STACK_TRACE_ENTRY();

    std::unique_lock locker {_stateLocker};

    if (!_commitThreadActive) {
        throw DataBaseException("Commit thread is not active");
    }

    while (!_pendingCommitOperations.empty()) {
        if (!InValidState()) {
            WriteLog("Database is not in valid state, pending commit operations can't be guaranteed to be durably committed");
            break;
        }

        _commitThreadDoneSignal.wait(locker);
    }
}

void DataBaseImpl::ClearChanges() noexcept
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock locker {_stateLocker};

    _pendingCommitOperations.clear();
}

void DataBaseImpl::DrawGui()
{
    FO_STACK_TRACE_ENTRY();

    ImGui::TextUnformatted("No info");
}

void DataBaseImpl::ScheduleCommit()
{
    FO_STACK_TRACE_ENTRY();

    bool should_notify = false;

    {
        std::scoped_lock locker {_stateLocker};

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
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_commitThread.joinable());

    {
        std::scoped_lock locker {_stateLocker};

        _commitThreadStopRequested = false;
    }

    _commitThread = run_thread("DataBaseCommitter", [this] { CommitThreadEntry(); });
}

void DataBaseImpl::StopCommitThread() noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        FO_RUNTIME_ASSERT(_commitThread.joinable());

        {
            std::scoped_lock locker {_stateLocker};

            _commitThreadStopRequested = true;
        }

        _commitThreadSignal.notify_one();
        _commitThread.join();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

void DataBaseImpl::CommitThreadEntry() noexcept
{
    FO_STACK_TRACE_ENTRY();

    while (true) {
        try {
            bool has_changes = false;
            bool stop_requested = false;

            {
                std::unique_lock locker {_stateLocker};

                while ((!_commitThreadActive || _pendingCommitOperations.empty()) && !_commitThreadStopRequested && !_backendFailed) {
                    _commitThreadDoneSignal.notify_all();
                    _commitThreadSignal.wait(locker);
                }

                if (!_commitThreadActive) {
                    if (_commitThreadStopRequested) {
                        break;
                    }
                    if (_backendFailed) {
                        StartPanic("Database backend is in failed state and commit thread is not active, can't recover");
                        break;
                    }

                    continue;
                }

                has_changes = !_pendingCommitOperations.empty();
                stop_requested = _commitThreadStopRequested;
            }

            if (stop_requested) {
                while (has_changes) {
                    CommitNextChange();

                    std::scoped_lock locker {_stateLocker};

                    has_changes = !_pendingCommitOperations.empty();
                }

                break;
            }
            else if (_backendFailed && !_panicStarted && nanotime::now() >= _reconnectRetryTime) {
                _reconnectRetryTime = nanotime::now() + _reconnectRetryPeriod;

                try {
                    if (TryReconnect()) {
                        RestorePendingChanges();
                        _backendFailed = false;
                    }
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                    StartPanic("Failed to restore pending changes after successful backend reconnection");
                }
            }
            else if (has_changes) {
                CommitNextChange();
            }
            else if (_backendFailed) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
    }

    _commitThreadDoneSignal.notify_all();
}

void DataBaseImpl::CommitNextChange() noexcept
{
    FO_STACK_TRACE_ENTRY();

    shared_ptr<CommitOperationData> op;

    {
        std::scoped_lock locker {_stateLocker};

        if (_pendingCommitOperations.empty()) {
            return;
        }

        op = _pendingCommitOperations.front();
        _docReadRetryMarkers.erase({op->CollectionName, op->RecordId});
    }

    if (!_backendFailed) {
        try {
            const auto storage_record_id = EncodeBackendDbKey(op->RecordId, GetCollectionKeyType(op->CollectionName), GetStringKeyEscaping());

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
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            _backendFailed = true;
            _reconnectRetryTime = nanotime::now() + _reconnectRetryPeriod;
        }
    }

    if (_backendFailed) {
        try {
            if (!_opLogEnabled) {
                StartPanic("Database backend write failed and oplog is disabled, can't recover");
                return;
            }

            string log_data;

            switch (op->Type) {
            case CommitOperationType::Insert: {
                const auto doc_json = AnyDocumentToJson(op->Doc).dump();
                FO_RUNTIME_ASSERT(doc_json.find_first_of("\r\n") == string::npos);
                const auto key = EncodeStorageDbKey(op->RecordId, GetCollectionKeyType(op->CollectionName), GetStringKeyEscaping());
                log_data += strex("insert {} {} {}\n", op->CollectionName.as_str(), key, doc_json).str();
            } break;
            case CommitOperationType::Update: {
                const auto doc_json = AnyDocumentToJson(op->Doc).dump();
                FO_RUNTIME_ASSERT(doc_json.find_first_of("\r\n") == string::npos);
                const auto key = EncodeStorageDbKey(op->RecordId, GetCollectionKeyType(op->CollectionName), GetStringKeyEscaping());
                log_data += strex("update {} {} {}\n", op->CollectionName.as_str(), key, doc_json).str();
            } break;
            case CommitOperationType::Delete: {
                const auto key = EncodeStorageDbKey(op->RecordId, GetCollectionKeyType(op->CollectionName), GetStringKeyEscaping());
                log_data += strex("delete {} {}\n", op->CollectionName.as_str(), key).str();
            } break;
            }

            if (!_pendingChangesLog->Append(log_data)) {
                StartPanic("Append failed during commit failure handling");
                return;
            }
            if (_pendingChangesLog->GetTextSize() >= _pendingChangesPanicThreshold) {
                StartPanic("Oplog file exceeds configured panic threshold");
            }

            OnCommitOperationWrittenToOpLog();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            StartPanic("Exception during commit failure handling");
            return;
        }
    }

    try {
        RegisterDbRequests(1);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }

    try {
        std::scoped_lock state_locker {_stateLocker};

        _pendingCommitOperations.pop_front();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
        StartPanic("Exception during post-commit bookkeeping");
    }
}

void DataBaseImpl::RegisterDbRequests(size_t request_count) const
{
    FO_STACK_TRACE_ENTRY();

    if (request_count == 0) {
        return;
    }

    const auto now_second = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

    std::scoped_lock locker {_dbRequestsMetricLocker};

    auto& bucket = _dbRequestsPerMinuteBuckets[static_cast<size_t>(now_second % numeric_cast<int64_t>(_dbRequestsPerMinuteBuckets.size()))];

    if (bucket.Second != now_second) {
        bucket.Second = now_second;
        bucket.Count = 0;
    }

    bucket.Count += request_count;

    size_t requests_per_minute = 0;

    for (const auto& it : _dbRequestsPerMinuteBuckets) {
        if (it.Second > 0 && now_second - it.Second < 60) {
            requests_per_minute += it.Count;
        }
    }

    _dbRequestsPerMinute.store(requests_per_minute, std::memory_order_relaxed);
}

void DataBaseImpl::ValidateCollectionKey(hstring collection_name, const DataBaseKey& id) const
{
    FO_STACK_TRACE_ENTRY();

    if (!IsDbKeyValueValid(id)) {
        throw DataBaseException("Invalid database key value", collection_name, id);
    }

    const auto expected_type = GetCollectionKeyType(collection_name);

    if (GetDbKeyType(id) != expected_type) {
        throw DataBaseException("Invalid database key type for collection", collection_name, DbKeyTypeName(expected_type), id);
    }
}

void DataBaseImpl::StartPanic(string_view message)
{
    FO_STACK_TRACE_ENTRY();

    if (_panicStarted) {
        return;
    }

    WriteLog("Critical database failure: {}", message);
    _panicStarted = true;

    if (_panicCallback) {
        safe_call([this] { _panicCallback(); });
    }

    run_thread("Panic", [timeout = _panicShutdownTimeout.value()]() {
        std::this_thread::sleep_for(timeout);
        ExitApp(false);
    }).detach();
}

DataBaseImpl::RecoveryLogHandle::RecoveryLogHandle(string path) :
    _path {std::move(path)}
{
    FO_STACK_TRACE_ENTRY();

    if (_path.empty()) {
        throw DataBaseException("Empty recovery log file path");
    }

#if FO_WINDOWS
    if (_sopen_s(&_fd, _path.c_str(), _O_BINARY | _O_RDWR | _O_CREAT, _SH_DENYRW, _S_IREAD | _S_IWRITE) != 0) {
        throw DataBaseException("Failed to open recovery oplog file", _path);
    }

#else
    _fd = open(_path.c_str(), O_RDWR | O_CREAT, 0666);

    if (_fd < 0) {
        throw DataBaseException("Failed to open recovery oplog file", _path);
    }

    if (flock(_fd, LOCK_EX | LOCK_NB) != 0) {
        close(_fd);
        throw DataBaseException("Failed to lock recovery oplog file, it may be used by another process", _path);
    }
#endif

    const auto read_content = Read();

    if (!read_content.has_value()) {
        throw DataBaseException("Oplog file can't be read", _path);
    }

    const auto normalized_content = strex(read_content.value()).normalize_line_endings().str();

    if (!normalized_content.empty() && !normalized_content.ends_with('\n')) {
        throw DataBaseException("Oplog file has invalid line ending", _path);
    }

    _content = strex(normalized_content).split('\n');
    _textSize = normalized_content.size();
}

DataBaseImpl::RecoveryLogHandle::~RecoveryLogHandle() noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    _close(_fd);
#else
    flock(_fd, LOCK_UN);
    close(_fd);
#endif
}

auto DataBaseImpl::RecoveryLogHandle::Read() noexcept -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    const auto size = _lseeki64(_fd, 0, SEEK_END);
#else
    const auto size = lseek(_fd, 0, SEEK_END);
#endif

    if (size < 0) {
        return std::nullopt;
    }

#if FO_WINDOWS
    if (_lseeki64(_fd, 0, SEEK_SET) < 0) {
        return std::nullopt;
    }
#else
    if (lseek(_fd, 0, SEEK_SET) < 0) {
        return std::nullopt;
    }
#endif

    string content;
    content.resize(static_cast<size_t>(size));

    size_t offset = 0;
    auto* buf = content.data();

    while (offset < content.size()) {
        const auto remaining = content.size() - offset;

#if FO_WINDOWS
        const auto chunk = static_cast<unsigned int>(std::min(remaining, static_cast<size_t>(std::numeric_limits<int>::max())));
        const auto read_size = _read(_fd, buf + offset, chunk);
#else
        const auto read_size = read(_fd, buf + offset, remaining);
#endif

        if (read_size <= 0) {
            return std::nullopt;
        }

        offset += static_cast<size_t>(read_size);
    }

#if FO_WINDOWS
    if (_lseeki64(_fd, 0, SEEK_END) < 0) {
        return std::nullopt;
    }
#else
    if (lseek(_fd, 0, SEEK_END) < 0) {
        return std::nullopt;
    }
#endif

    return content;
}

auto DataBaseImpl::RecoveryLogHandle::Truncate() noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    if (_chsize_s(_fd, 0) != 0) {
        return false;
    }
    if (_lseeki64(_fd, 0, SEEK_SET) < 0) {
        return false;
    }
    if (_commit(_fd) != 0) {
        return false;
    }
#else
    if (ftruncate(_fd, 0) != 0) {
        return false;
    }
    if (lseek(_fd, 0, SEEK_SET) < 0) {
        return false;
    }
    if (fsync(_fd) != 0) {
        return false;
    }
#endif

    _content.clear();
    _textSize = 0;
    return true;
}

auto DataBaseImpl::RecoveryLogHandle::Append(string_view text) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_fd < 0) {
        return false;
    }

    if (text.empty()) {
        return true;
    }

    const auto normalized_content = strex(text).normalize_line_endings().str();

    if (!normalized_content.ends_with('\n')) {
        return false;
    }

    vector<string> parsed_content {};
    size_t line_begin = 0;

    while (line_begin < normalized_content.size()) {
        const auto line_end = normalized_content.find('\n', line_begin);

        if (line_end == string::npos || line_end == line_begin) {
            return false;
        }

        const auto line = string_view {normalized_content}.substr(line_begin, line_end - line_begin);
        const auto first_space = line.find(' ');
        const auto second_space = first_space != string_view::npos ? line.find(' ', first_space + 1) : string_view::npos;
        const auto third_space = second_space != string_view::npos ? line.find(' ', second_space + 1) : string_view::npos;

        if (first_space == string_view::npos || first_space == 0 || second_space == string_view::npos || second_space == first_space + 1) {
            return false;
        }

        const auto command = line.substr(0, first_space);
        string_view record_id {};
        string_view other {};
        bool has_other = false;

        if (third_space == string_view::npos) {
            if (second_space + 1 >= line.size()) {
                return false;
            }

            record_id = line.substr(second_space + 1);
        }
        else {
            if (third_space == second_space + 1 || third_space + 1 >= line.size()) {
                return false;
            }

            record_id = line.substr(second_space + 1, third_space - second_space - 1);
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

#if FO_WINDOWS
    if (_lseeki64(_fd, 0, SEEK_END) < 0) {
        return false;
    }
#else
    if (lseek(_fd, 0, SEEK_END) < 0) {
        return false;
    }
#endif

    size_t offset = 0;

    while (offset < normalized_content.size()) {
        const auto remaining = normalized_content.size() - offset;

#if FO_WINDOWS
        const auto chunk = static_cast<unsigned int>(std::min(remaining, static_cast<size_t>(std::numeric_limits<int>::max())));
        const auto written_size = _write(_fd, normalized_content.data() + offset, chunk);
#else
        const auto written_size = write(_fd, normalized_content.data() + offset, remaining);
#endif

        if (written_size <= 0) {
            return false;
        }

        offset += static_cast<size_t>(written_size);
    }

#if FO_WINDOWS
    if (_commit(_fd) != 0) {
        return false;
    }
#else
    if (fsync(_fd) != 0) {
        return false;
    }
#endif

    for (auto& line : parsed_content) {
        _content.emplace_back(std::move(line));
    }

    _textSize += normalized_content.size();

    return true;
}

auto ConnectToDataBase(DataBaseSettings& db_settings, string_view connection_info, const DataBaseCollectionSchemas& collection_schemas, DataBasePanicCallback panic_callback) -> DataBase
{
    FO_STACK_TRACE_ENTRY();

    const auto finish_connect = [&](auto* raw_impl) -> DataBase {
        auto impl = unique_ptr<DataBaseImpl> {raw_impl};
        impl->InitializeCollections(collection_schemas);
        impl->InitializeOpLogs();
        impl->RestorePendingChanges();
        return DataBase(impl.release());
    };

    if (const auto options = strvex(connection_info).split(' '); !options.empty()) {
        WriteLog("Connect to {} data base", options.front());

        if (options.front() == "JSON" && options.size() == 2) {
            return finish_connect(CreateJsonDataBase(db_settings, options[1], std::move(panic_callback)));
        }
#if FO_HAVE_UNQLITE
        if (options.front() == "DbUnQLite" && options.size() == 2) {
            return finish_connect(CreateUnQLiteDataBase(db_settings, options[1], std::move(panic_callback)));
        }
#endif
#if FO_HAVE_MONGO
        if (options.front() == "Mongo" && options.size() == 3) {
            return finish_connect(CreateMongoDataBase(db_settings, options[1], options[2], std::move(panic_callback)));
        }
#endif
        if (options.front() == "Memory" && options.size() == 1) {
            return finish_connect(CreateMemoryDataBase(db_settings, std::move(panic_callback)));
        }
    }

    throw DataBaseException("Wrong storage options", connection_info);
}

static void ValueToBson(string_view key, const AnyData::Value& value, bson_t* bson, char escape_dot)
{
    FO_STACK_TRACE_ENTRY();

    auto key_buf = strex(key);
    const auto escaped_key = escape_dot != 0 ? key_buf.replace('.', escape_dot).strv() : key;
    const char* key_data = escaped_key.data();
    const auto key_len = numeric_cast<int32_t>(escaped_key.length());

    if (value.Type() == AnyData::ValueType::Int64) {
        if (!bson_append_int64(bson, key_data, key_len, value.AsInt64())) {
            throw DataBaseException("ValueToBson bson_append_int64", key, value.AsInt64());
        }
    }
    else if (value.Type() == AnyData::ValueType::Float64) {
        if (!bson_append_double(bson, key_data, key_len, value.AsDouble())) {
            throw DataBaseException("ValueToBson bson_append_double", key, value.AsDouble());
        }
    }
    else if (value.Type() == AnyData::ValueType::Bool) {
        if (!bson_append_bool(bson, key_data, key_len, value.AsBool())) {
            throw DataBaseException("ValueToBson bson_append_bool", key, value.AsBool());
        }
    }
    else if (value.Type() == AnyData::ValueType::String) {
        if (!bson_append_utf8(bson, key_data, key_len, value.AsString().c_str(), numeric_cast<int32_t>(value.AsString().length()))) {
            throw DataBaseException("ValueToBson bson_append_utf8", key, value.AsString());
        }
    }
    else if (value.Type() == AnyData::ValueType::Array) {
        bson_t bson_arr;

        if (!bson_append_array_begin(bson, key_data, key_len, &bson_arr)) {
            throw DataBaseException("ValueToBson bson_append_array_begin", key);
        }

        const auto& arr = value.AsArray();
        size_t arr_index = 0;

        for (const auto& arr_entry : arr) {
            string arr_key = strex("{}", arr_index++);
            ValueToBson(arr_key, arr_entry, &bson_arr, escape_dot);
        }

        if (!bson_append_array_end(bson, &bson_arr)) {
            throw DataBaseException("ValueToBson bson_append_array_end");
        }
    }
    else if (value.Type() == AnyData::ValueType::Dict) {
        bson_t bson_doc;

        if (!bson_append_document_begin(bson, key_data, key_len, &bson_doc)) {
            throw DataBaseException("ValueToBson bson_append_bool", key);
        }

        const auto& dict = value.AsDict();

        for (auto&& [dict_key, dict_value] : dict) {
            ValueToBson(dict_key, dict_value, &bson_doc, escape_dot);
        }

        if (!bson_append_document_end(bson, &bson_doc)) {
            throw DataBaseException("ValueToBson bson_append_document_end");
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

void DocumentToBson(const AnyData::Document& doc, bson_t* bson, char escape_dot)
{
    FO_STACK_TRACE_ENTRY();

    for (auto&& [doc_key, doc_value] : doc) {
        ValueToBson(doc_key, doc_value, bson, escape_dot);
    }
}

static auto BsonToValue(bson_iter_t* iter, char escape_dot) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    const auto* value = bson_iter_value(iter);

    if (value->value_type == BSON_TYPE_INT32) {
        return numeric_cast<int64_t>(value->value.v_int32);
    }
    else if (value->value_type == BSON_TYPE_INT64) {
        return value->value.v_int64;
    }
    else if (value->value_type == BSON_TYPE_DOUBLE) {
        return value->value.v_double;
    }
    else if (value->value_type == BSON_TYPE_BOOL) {
        return value->value.v_bool;
    }
    else if (value->value_type == BSON_TYPE_UTF8) {
        return string(value->value.v_utf8.str, value->value.v_utf8.len);
    }
    else if (value->value_type == BSON_TYPE_ARRAY) {
        bson_iter_t arr_iter;

        if (!bson_iter_recurse(iter, &arr_iter)) {
            throw DataBaseException("BsonToValue bson_iter_recurse");
        }

        AnyData::Array arr;

        while (bson_iter_next(&arr_iter)) {
            auto arr_entry = BsonToValue(&arr_iter, escape_dot);
            arr.EmplaceBack(std::move(arr_entry));
        }

        return std::move(arr);
    }
    else if (value->value_type == BSON_TYPE_DOCUMENT) {
        bson_iter_t doc_iter;

        if (!bson_iter_recurse(iter, &doc_iter)) {
            throw DataBaseException("BsonToValue bson_iter_recurse");
        }

        AnyData::Dict dict;

        while (bson_iter_next(&doc_iter)) {
            const auto* key = bson_iter_key(&doc_iter);
            auto unescaped_key = escape_dot != 0 ? strex(key).replace(escape_dot, '.') : string(key);
            auto dict_value = BsonToValue(&doc_iter, escape_dot);
            dict.Emplace(std::move(unescaped_key), std::move(dict_value));
        }

        return std::move(dict);
    }
    else {
        throw DataBaseException("BsonToValue Invalid type", value->value_type);
    }
}

void BsonToDocument(const bson_t* bson, AnyData::Document& doc, char escape_dot)
{
    FO_STACK_TRACE_ENTRY();

    bson_iter_t iter;

    if (!bson_iter_init(&iter, bson)) {
        throw DataBaseException("BsonToDocument bson_iter_init");
    }

    while (bson_iter_next(&iter)) {
        const auto* key = bson_iter_key(&iter);

        if (key[0] == '_' && key[1] == 'i' && key[2] == 'd' && key[3] == 0) {
            continue;
        }

        auto value = BsonToValue(&iter, escape_dot);
        auto unescaped_key = escape_dot != 0 ? strex(key).replace(escape_dot, '.') : string(key);
        doc.Emplace(std::move(unescaped_key), std::move(value));
    }
}

static auto AnyValueToJson(const AnyData::Value& value) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    switch (value.Type()) {
    case AnyData::ValueType::Int64:
        return value.AsInt64();
    case AnyData::ValueType::Float64:
        return value.AsDouble();
    case AnyData::ValueType::Bool:
        return value.AsBool();
    case AnyData::ValueType::String:
        return value.AsString();
    case AnyData::ValueType::Array: {
        auto arr_json = nlohmann::json::array();

        for (const auto& arr_entry : value.AsArray()) {
            arr_json.emplace_back(AnyValueToJson(arr_entry));
        }

        return arr_json;
    }
    case AnyData::ValueType::Dict: {
        auto dict_json = nlohmann::json::object();

        for (auto&& [dict_key, dict_value] : value.AsDict()) {
            dict_json[dict_key.c_str()] = AnyValueToJson(dict_value);
        }

        return dict_json;
    }
    }

    throw DataBaseException("Invalid AnyData value type for pending database json", static_cast<int>(value.Type()));
}

static auto JsonToAnyValue(const nlohmann::json& value) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    if (value.is_number_integer() || value.is_number_unsigned()) {
        return numeric_cast<int64_t>(value.get<int64_t>());
    }
    if (value.is_number_float()) {
        return value.get<float64_t>();
    }
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_string()) {
        return value.get<string>();
    }
    if (value.is_array()) {
        AnyData::Array arr;

        for (const auto& arr_entry : value) {
            arr.EmplaceBack(JsonToAnyValue(arr_entry));
        }

        return std::move(arr);
    }
    if (value.is_object()) {
        AnyData::Dict dict;

        for (auto&& [dict_key, dict_value] : value.items()) {
            dict.Emplace(string(dict_key), JsonToAnyValue(dict_value));
        }

        return std::move(dict);
    }

    throw DataBaseException("Invalid pending database json value type");
}

static auto AnyDocumentToJson(const AnyData::Document& doc) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    auto doc_json = nlohmann::json::object();

    for (auto&& [doc_key, doc_value] : doc) {
        doc_json[doc_key.c_str()] = AnyValueToJson(doc_value);
    }

    return doc_json;
}

static auto JsonToAnyDocument(const nlohmann::json& doc_json) -> AnyData::Document
{
    FO_STACK_TRACE_ENTRY();

    if (!doc_json.is_object()) {
        throw DataBaseException("Invalid pending database json document");
    }

    AnyData::Document doc;

    for (auto&& [doc_key, doc_value] : doc_json.items()) {
        doc.Emplace(string(doc_key), JsonToAnyValue(doc_value));
    }

    return doc;
}

static auto AreDocumentsEqual(const AnyData::Document& left, const AnyData::Document& right) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return left == right;
}

static auto DoesDocumentContain(const AnyData::Document& target, const AnyData::Document& patch) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    return std::holds_alternative<ident_t>(key) ? DataBaseKeyType::IntId : DataBaseKeyType::String;
}

static auto DbKeyTypeName(DataBaseKeyType key_type) noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    if (GetDbKeyType(key) != key_type) {
        throw DataBaseException("Database key type mismatch", DbKeyTypeName(key_type), key);
    }

    return std::visit(
        [key_type, escaping](const auto& value) -> string {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, ident_t>) {
                FO_RUNTIME_ASSERT(key_type == DataBaseKeyType::IntId);
                FO_RUNTIME_ASSERT(value != ident_t {});
                return strex("{}", value).str();
            }
            else {
                FO_RUNTIME_ASSERT(key_type == DataBaseKeyType::String);
                FO_RUNTIME_ASSERT(!value.empty());
                return EncodeDbStringKey(value, escaping);
            }
        },
        key);
}

static auto DecodeStorageDbKey(string_view key_str, DataBaseKeyType key_type, DataBaseStringKeyEscaping escaping) -> DataBaseKey
{
    FO_STACK_TRACE_ENTRY();

    if (key_str.empty()) {
        throw DataBaseException("Invalid database key value", key_str);
    }

    if (key_type == DataBaseKeyType::IntId) {
        if (!strvex(key_str).is_number()) {
            throw DataBaseException("Invalid database numeric key format", key_str);
        }

        const auto id_value = strvex(key_str).to_int64();

        if (id_value <= 0) {
            throw DataBaseException("Invalid database numeric key value", key_str);
        }

        return ident_t {id_value};
    }

    const auto decoded_value = DecodeDbStringKey(key_str, escaping);

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
    FO_STACK_TRACE_ENTRY();

    if (key_type == DataBaseKeyType::IntId) {
        return key;
    }

    if (GetDbKeyType(key) != key_type) {
        throw DataBaseException("Database key type mismatch", DbKeyTypeName(key_type), key);
    }

    const auto& value = std::get<string>(key);
    FO_RUNTIME_ASSERT(!value.empty());

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
    FO_STACK_TRACE_ENTRY();

    if (key_type == DataBaseKeyType::IntId) {
        return key;
    }

    if (GetDbKeyType(key) != key_type) {
        throw DataBaseException("Database collection returned invalid key type", DbKeyTypeName(key_type), key);
    }

    const auto& value = std::get<string>(key);
    FO_RUNTIME_ASSERT(!value.empty());

    if (escaping == DataBaseStringKeyEscaping::Raw) {
        if (!strvex(value).is_valid_utf8()) {
            throw DataBaseException("Invalid database string key utf8", value);
        }

        return string(value);
    }

    const auto decoded_value = DecodeDbStringKey(value, escaping);

    if (!strvex(decoded_value).is_valid_utf8()) {
        throw DataBaseException("Invalid database string key utf8", value);
    }

    return decoded_value;
}

static auto EncodeDbStringKey(string_view value, DataBaseStringKeyEscaping escaping) -> string
{
    FO_STACK_TRACE_ENTRY();

    static constexpr char hex_digits[] = "0123456789abcdef";

    if (escaping == DataBaseStringKeyEscaping::Hex) {
        string result;
        result.reserve(2 + value.size() * 2);
        result += "s_";

        for (const auto ch : value) {
            const auto byte = static_cast<uint8_t>(ch);
            result.push_back(hex_digits[byte >> 4]);
            result.push_back(hex_digits[byte & 0x0F]);
        }

        return result;
    }

    string result;
    result.reserve(value.size());

    for (const auto ch : value) {
        const auto byte = static_cast<uint8_t>(ch);

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
    FO_STACK_TRACE_ENTRY();

    if (escaping == DataBaseStringKeyEscaping::Hex) {
        if (!value.starts_with("s_")) {
            throw DataBaseException("Invalid database string key format", value);
        }

        const auto encoded_value = value.substr(2);

        if (encoded_value.empty() || encoded_value.size() % 2 != 0) {
            throw DataBaseException("Invalid database string key value", value);
        }

        string decoded_value;
        decoded_value.reserve(encoded_value.size() / 2);

        for (size_t i = 0; i < encoded_value.size(); i += 2) {
            const auto high = DecodeHexDigit(encoded_value[i]);
            const auto low = DecodeHexDigit(encoded_value[i + 1]);
            decoded_value.push_back(static_cast<char>((high << 4) | low));
        }

        return decoded_value;
    }

    string decoded_value;
    decoded_value.reserve(value.size());

    for (size_t i = 0; i < value.size(); i++) {
        if (value[i] != '%') {
            decoded_value.push_back(value[i]);
            continue;
        }

        if (i + 2 >= value.size()) {
            throw DataBaseException("Invalid database string key value", value);
        }

        const auto high = DecodeHexDigit(value[i + 1]);
        const auto low = DecodeHexDigit(value[i + 2]);
        decoded_value.push_back(static_cast<char>((high << 4) | low));
        i += 2;
    }

    return decoded_value;
}

static auto ShouldEscapeDbStringByte(uint8_t byte, DataBaseStringKeyEscaping escaping) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
