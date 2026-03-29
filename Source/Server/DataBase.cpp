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

auto DataBase::GetAllIds(hstring collection_name) const -> vector<ident_t>
{
    FO_STACK_TRACE_ENTRY();

    return _impl->GetAllRecordIds(collection_name);
}

auto DataBase::Get(hstring collection_name, ident_t id) const -> AnyData::Document
{
    FO_STACK_TRACE_ENTRY();

    return _impl->GetDocument(collection_name, id);
}

auto DataBase::Valid(hstring collection_name, ident_t id) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return !_impl->GetDocument(collection_name, id).Empty();
}

void DataBase::Insert(hstring collection_name, ident_t id, const AnyData::Document& doc)
{
    FO_STACK_TRACE_ENTRY();

    _impl->Insert(collection_name, id, doc);
}

void DataBase::Update(hstring collection_name, ident_t id, string_view key, const AnyData::Value& value)
{
    FO_STACK_TRACE_ENTRY();

    _impl->Update(collection_name, id, key, value);
}

void DataBase::Delete(hstring collection_name, ident_t id)
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
    _reconnectRetryPeriod {std::chrono::milliseconds {std::max(_settings->ReconnectRetryPeriod, 1)}},
    _panicShutdownTimeout {std::chrono::milliseconds {_settings->PanicShutdownTimeout}},
    _panicCallback {std::move(panic_callback)}
{
    FO_STACK_TRACE_ENTRY();
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
            if (!strvex(record_id_str).is_number() || strvex(record_id_str).to_int64() <= 0) {
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

            const auto collection_name = _pendingChangesHashes.ToHashedString(collection);
            const auto id_value = strvex(record_id_str).to_int64();
            const auto record_id = ident_t {id_value};
            const auto current_doc = GetRecord(collection_name, record_id);

            if (command == "delete") {
                if (!current_doc.Empty()) {
                    DeleteRecord(collection_name, record_id);
                }
            }
            else {
                auto doc = JsonToAnyDocument(nlohmann::json::parse(other));

                if (command == "insert") {
                    if (current_doc.Empty()) {
                        InsertRecord(collection_name, record_id, doc);
                    }
                    else if (!AreDocumentsEqual(current_doc, doc)) {
                        throw DataBaseException("Pending database insert replay conflict", id_value, _settings->OpLogPath);
                    }
                }
                else if (!DoesDocumentContain(current_doc, doc)) {
                    UpdateRecord(collection_name, record_id, doc);
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

auto DataBaseImpl::GetDocument(hstring collection_name, ident_t id) const -> AnyData::Document
{
    FO_STACK_TRACE_ENTRY();

    if (!InValidState()) {
        throw DataBaseException("Database backend is in failed state");
    }

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
            doc = GetRecord(collection_name, id);
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

void DataBaseImpl::Insert(hstring collection_name, ident_t id, const AnyData::Document& doc)
{
    FO_STACK_TRACE_ENTRY();

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

void DataBaseImpl::Update(hstring collection_name, ident_t id, string_view key, const AnyData::Value& value)
{
    FO_STACK_TRACE_ENTRY();

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

void DataBaseImpl::Delete(hstring collection_name, ident_t id)
{
    FO_STACK_TRACE_ENTRY();

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

    _commitThread = std::thread([this] { CommitThreadEntry(); });
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

    SetThisThreadName("DataBaseCommitter");

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
            switch (op->Type) {
            case CommitOperationType::Insert:
                InsertRecord(op->CollectionName, op->RecordId, op->Doc);
                break;
            case CommitOperationType::Update:
                UpdateRecord(op->CollectionName, op->RecordId, op->Doc);
                break;
            case CommitOperationType::Delete:
                DeleteRecord(op->CollectionName, op->RecordId);
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
                log_data += strex("insert {} {} {}\n", op->CollectionName.as_str(), op->RecordId, doc_json).str();
            } break;
            case CommitOperationType::Update: {
                const auto doc_json = AnyDocumentToJson(op->Doc).dump();
                FO_RUNTIME_ASSERT(doc_json.find_first_of("\r\n") == string::npos);
                log_data += strex("update {} {} {}\n", op->CollectionName.as_str(), op->RecordId, doc_json).str();
            } break;
            case CommitOperationType::Delete: {
                log_data += strex("delete {} {}\n", op->CollectionName.as_str(), op->RecordId).str();
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

    auto& bucket = _dbRequestsPerMinuteBuckets[static_cast<size_t>(now_second % numeric_cast<int64>(_dbRequestsPerMinuteBuckets.size()))];

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

    std::thread {[timeout = _panicShutdownTimeout.value()]() {
        std::this_thread::sleep_for(timeout);
        ExitApp(false);
    }}.detach();
}

static auto AreDocumentsEqual(const AnyData::Document& left, const AnyData::Document& right) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return left == right;
}

static auto DoesDocumentContain(const AnyData::Document& target, const AnyData::Document& patch) -> bool
{
    FO_STACK_TRACE_ENTRY();

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
        const auto collection = line.substr(first_space + 1, second_space - first_space - 1);
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
        if (!strvex(record_id).is_number() || strvex(record_id).to_int64() <= 0) {
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

auto ConnectToDataBase(DataBaseSettings& db_settings, string_view connection_info, DataBasePanicCallback panic_callback) -> DataBase
{
    FO_STACK_TRACE_ENTRY();

    const auto finish_connect = [&](auto* raw_impl) -> DataBase {
        unique_ptr<DataBaseImpl> impl {raw_impl};
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
    const auto key_len = numeric_cast<int32>(escaped_key.length());

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
        if (!bson_append_utf8(bson, key_data, key_len, value.AsString().c_str(), numeric_cast<int32>(value.AsString().length()))) {
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
        return numeric_cast<int64>(value->value.v_int32);
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
        return numeric_cast<int64>(value.get<int64>());
    }
    if (value.is_number_float()) {
        return value.get<float64>();
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

FO_END_NAMESPACE
