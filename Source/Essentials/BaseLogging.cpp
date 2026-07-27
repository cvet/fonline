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

#include "BaseLogging.h"
#include "GlobalData.h"

#if FO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include "WinApiUndef.inc"

FO_BEGIN_NAMESPACE

constexpr size_t AsyncQueueDropLimit = 100000;

static void StartAsyncWorker();
static void StopAsyncWorker() noexcept;
static void AsyncWorkerLoop() noexcept;
static void WriteSync(string_view message) noexcept;
static void RotateLogIfOversizedLocked() noexcept;
static auto CopyAndTruncateLogLocked(const std::string& rotated_path) noexcept -> bool;
static auto ReopenLogFileLocked(std::ios::openmode mode) noexcept -> bool;
static void WriteRotationNoticeLocked(const std::string& rotated_path) noexcept;
static void DisableLogRotationLocked() noexcept;
static void DeleteRotatedLogParts(const std::string& path) noexcept;
static auto IsNullLogDevicePath(string_view path) noexcept -> bool;
static void FlushLogAtExit();

struct BaseLoggingData
{
    BaseLoggingData()
    {
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
        int32_t result = std::at_quick_exit(FlushLogAtExit);
        ignore_unused(result);
#else
        ignore_unused(FlushLogAtExit);
#endif

#if FO_WINDOWS
        SetConsoleOutputCP(CP_UTF8);
#endif
    }

    ~BaseLoggingData() { StopAsyncWorker(); }

    struct AsyncEntry
    {
        std::string Message {};
        std::optional<CatchedStackTraceData> StackTrace {};
    };

    std::mutex LogLocker {};
    std::ofstream LogFileHandle {};
    std::string LogFilePath {};
    bool LogFileIsNullDevice {};
    bool LogFileAppendMode {};
    size_t MaxLogFileSize {};
    size_t LogRotationCounter {};
    bool LogRotationFailed {};
    std::atomic_bool AsyncEnabled {};
    std::mutex AsyncQueueMutex {};
    std::condition_variable AsyncSignal {};
    std::deque<AsyncEntry> AsyncQueue {};
    size_t AsyncDroppedCount {};
    bool AsyncFinish {};
    std::thread AsyncWorker {};
};
FO_GLOBAL_DATA(BaseLoggingData, BaseLogging);

extern void LogToFile(string_view path, bool append, bool cleanup_rotated_parts)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (build_condition<FO_WEB>()) {
        return;
    }

    bool open_failed = false;
    std::string new_log_path {path};
    bool new_log_is_null_device = IsNullLogDevicePath(new_log_path);

    {
        std::scoped_lock locker {BaseLogging->LogLocker};

        std::ios_base::openmode open_mode = std::ios::out | std::ios::binary | (append ? std::ios::app : std::ios::trunc);
        std::ofstream new_log_file {new_log_path, open_mode};

        if (!new_log_file) {
            open_failed = true;
        }
        else {
            if (BaseLogging->LogFileHandle.is_open()) {
                BaseLogging->LogFileHandle.close();
            }

            BaseLogging->LogFileHandle = std::move(new_log_file);
            BaseLogging->LogFilePath = std::move(new_log_path);
            BaseLogging->LogFileIsNullDevice = new_log_is_null_device;
            BaseLogging->LogFileAppendMode = append;
            BaseLogging->LogRotationCounter = 0;
            BaseLogging->LogRotationFailed = false;

            // Cleanup happens only after the replacement log opened successfully. A writable-path
            // client switch still appends the current run's handoff lines, but explicitly removes
            // numbered parts retained from the previous run.
            if (!new_log_is_null_device && (!append || cleanup_rotated_parts)) {
                DeleteRotatedLogParts(BaseLogging->LogFilePath);
            }
        }
    }

    if (open_failed) {
        WriteBaseLog(std::string("Can't create log file '").append(path).append("'\n"));
    }
}

extern void SetMaxLogFileSize(size_t size)
{
    FO_NO_STACK_TRACE_ENTRY();

    std::scoped_lock locker {BaseLogging->LogLocker};

    BaseLogging->MaxLogFileSize = size;
    BaseLogging->LogRotationFailed = false;
}

extern void SetAsyncLogWriting(bool enabled)
{
    if (build_condition<FO_WEB>()) {
        return;
    }

    if (enabled) {
        StartAsyncWorker();
    }
    else {
        StopAsyncWorker();
    }
}

extern void SuspendAsyncLogWriting() noexcept
{
    if (BaseLogging != nullptr) {
        BaseLogging->AsyncEnabled.store(false, std::memory_order_release);
    }
}

extern void WriteBaseLog(string_view message, const CatchedStackTraceData* st) noexcept
{
    try {
        if (BaseLogging == nullptr) {
            std::cout << message;

            if (st != nullptr) {
                std::cout << FormatStackTrace(*st) << "\n";
            }

            std::cout.flush();
            return;
        }

        if (BaseLogging->AsyncEnabled.load(std::memory_order_acquire)) {
            bool enqueued = false;
            bool dropped = false;

            {
                std::scoped_lock queue_lock {BaseLogging->AsyncQueueMutex};

                if (BaseLogging->AsyncEnabled.load(std::memory_order_relaxed)) {
                    if (BaseLogging->AsyncQueue.size() >= AsyncQueueDropLimit) {
                        BaseLogging->AsyncDroppedCount++;
                        dropped = true;
                    }
                    else {
                        BaseLoggingData::AsyncEntry entry;
                        entry.Message.assign(message);

                        if (st != nullptr) {
                            entry.StackTrace = *st;
                        }

                        BaseLogging->AsyncQueue.emplace_back(std::move(entry));
                        enqueued = true;
                    }
                }
            }

            if (enqueued) {
                BaseLogging->AsyncSignal.notify_one();
            }

            if (enqueued || dropped) {
                return;
            }
        }

        if (st != nullptr) {
            std::string combined;
            combined.reserve(message.size() + 256);
            combined.append(message);
            combined.append(FormatStackTrace(*st));
            combined.append("\n");
            WriteSync(combined);
        }
        else {
            WriteSync(message);
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

static void StartAsyncWorker()
{
    if (BaseLogging->AsyncWorker.joinable()) {
        return;
    }

    {
        std::scoped_lock queue_lock {BaseLogging->AsyncQueueMutex};

        BaseLogging->AsyncFinish = false;
    }

    BaseLogging->AsyncWorker = std::thread([] { AsyncWorkerLoop(); });
    BaseLogging->AsyncEnabled.store(true, std::memory_order_release);
}

static void StopAsyncWorker() noexcept
{
    if (!BaseLogging->AsyncWorker.joinable()) {
        BaseLogging->AsyncEnabled.store(false, std::memory_order_release);
        return;
    }

    {
        std::scoped_lock queue_lock {BaseLogging->AsyncQueueMutex};

        BaseLogging->AsyncEnabled.store(false, std::memory_order_release);
        BaseLogging->AsyncFinish = true;
    }

    BaseLogging->AsyncSignal.notify_all();

    try {
        BaseLogging->AsyncWorker.join();
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

static void AsyncWorkerLoop() noexcept
{
    try {
        std::deque<BaseLoggingData::AsyncEntry> drained;
        size_t drained_drop_count = 0;

        while (true) {
            {
                std::unique_lock queue_lock {BaseLogging->AsyncQueueMutex};

                while (!BaseLogging->AsyncFinish && BaseLogging->AsyncQueue.empty() && BaseLogging->AsyncDroppedCount == 0) {
                    BaseLogging->AsyncSignal.wait(queue_lock);
                }

                drained.swap(BaseLogging->AsyncQueue);
                drained_drop_count = BaseLogging->AsyncDroppedCount;
                BaseLogging->AsyncDroppedCount = 0;

                if (drained.empty() && drained_drop_count == 0 && BaseLogging->AsyncFinish) {
                    return;
                }
            }

            for (const auto& entry : drained) {
                if (entry.StackTrace.has_value()) {
                    try {
                        std::string combined;
                        combined.reserve(entry.Message.size() + 256);
                        combined.append(entry.Message);
                        combined.append(FormatStackTrace(entry.StackTrace.value()));
                        combined.append("\n");
                        WriteSync(combined);
                    }
                    catch (...) {
                        WriteSync(entry.Message);
                        BreakIntoDebugger();
                    }
                }
                else {
                    WriteSync(entry.Message);
                }
            }

            if (drained_drop_count != 0) {
                std::string drop_notice = "Dropped ";
                drop_notice += std::to_string(drained_drop_count);
                drop_notice += " log messages due to high volume (queue limit ";
                drop_notice += std::to_string(AsyncQueueDropLimit);
                drop_notice += ")\n";
                WriteSync(drop_notice);
            }

            drained.clear();
            drained_drop_count = 0;
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

static void WriteSync(string_view message) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        std::scoped_lock locker {BaseLogging->LogLocker};

        if (BaseLogging->LogFileHandle) {
            BaseLogging->LogFileHandle.seekp(0, std::ios::end);
            BaseLogging->LogFileHandle << message;
            BaseLogging->LogFileHandle.flush();

            RotateLogIfOversizedLocked();
        }

        std::cout << message;
        std::cout.flush();
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

static void RotateLogIfOversizedLocked() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        if (BaseLogging->MaxLogFileSize == 0 || BaseLogging->LogRotationFailed) {
            return;
        }

        if (BaseLogging->LogFileIsNullDevice) {
            return;
        }

        std::streamoff log_size = static_cast<std::streamoff>(BaseLogging->LogFileHandle.tellp());

        if (log_size < 0 || std::cmp_less_equal(log_size, BaseLogging->MaxLogFileSize)) {
            return;
        }

        // Pick the first free numbered part
        std::string rotated_path;
        std::error_code exists_error;

        do {
            BaseLogging->LogRotationCounter++;
            rotated_path = BaseLogging->LogFilePath;
            rotated_path += '.';
            rotated_path += std::to_string(BaseLogging->LogRotationCounter);
        } while (std::filesystem::exists(rotated_path, exists_error));

        // Append mode is used for the synchronous client host/runtime handoff. Truncate the active
        // file in place so the host's already open handle remains attached to the canonical path.
        if (BaseLogging->LogFileAppendMode) {
            if (CopyAndTruncateLogLocked(rotated_path)) {
                WriteRotationNoticeLocked(rotated_path);
            }
            else {
                DisableLogRotationLocked();
            }
            return;
        }

        BaseLogging->LogFileHandle.close();

        std::error_code rename_error;
        std::filesystem::rename(BaseLogging->LogFilePath, rotated_path, rename_error);

        if (rename_error) {
            (void)ReopenLogFileLocked(std::ios::app);
            DisableLogRotationLocked();
            return;
        }

        if (!ReopenLogFileLocked(std::ios::trunc)) {
            std::error_code remove_error;
            (void)std::filesystem::remove(BaseLogging->LogFilePath, remove_error);

            std::error_code rollback_error;
            std::filesystem::rename(rotated_path, BaseLogging->LogFilePath, rollback_error);

            if (!rollback_error) {
                (void)ReopenLogFileLocked(std::ios::app);
            }

            DisableLogRotationLocked();
            return;
        }

        WriteRotationNoticeLocked(rotated_path);
    }
    catch (...) {
        DisableLogRotationLocked();
        BreakIntoDebugger();
    }
}

static auto CopyAndTruncateLogLocked(const std::string& rotated_path) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        std::error_code copy_error;
        (void)std::filesystem::copy_file(BaseLogging->LogFilePath, rotated_path, copy_error);

        if (copy_error) {
            return false;
        }

        std::error_code truncate_error;
        std::filesystem::resize_file(BaseLogging->LogFilePath, 0, truncate_error);

        if (truncate_error) {
            return false;
        }

        BaseLogging->LogFileHandle.clear();
        BaseLogging->LogFileHandle.seekp(0, std::ios::end);
        return !!BaseLogging->LogFileHandle;
    }
    catch (...) {
        BreakIntoDebugger();
        return false;
    }
}

static auto ReopenLogFileLocked(std::ios::openmode mode) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        if (BaseLogging->LogFileHandle.is_open()) {
            BaseLogging->LogFileHandle.close();
        }

        BaseLogging->LogFileHandle.clear();
        BaseLogging->LogFileHandle.open(BaseLogging->LogFilePath, std::ios::out | std::ios::binary | mode);
        return !!BaseLogging->LogFileHandle;
    }
    catch (...) {
        BreakIntoDebugger();
        return false;
    }
}

static void WriteRotationNoticeLocked(const std::string& rotated_path) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        std::string notice = "Log rotated, previous part: '";
        notice += rotated_path;
        notice += "'\n";
        BaseLogging->LogFileHandle << notice;
        BaseLogging->LogFileHandle.flush();

        if (!BaseLogging->LogFileHandle) {
            DisableLogRotationLocked();
        }
    }
    catch (...) {
        DisableLogRotationLocked();
        BreakIntoDebugger();
    }
}

static void DisableLogRotationLocked() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    BaseLogging->LogRotationFailed = true;

    constexpr string_view notice = "Log rotation failed, further file rotation is disabled\n";

    try {
        if (BaseLogging->LogFileHandle.is_open() && !BaseLogging->LogFileHandle) {
            BaseLogging->LogFileHandle.clear();
            BaseLogging->LogFileHandle.seekp(0, std::ios::end);
        }

        if (BaseLogging->LogFileHandle) {
            BaseLogging->LogFileHandle << notice;
            BaseLogging->LogFileHandle.flush();
        }

        std::cout << notice;
        std::cout.flush();
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

static void DeleteRotatedLogParts(const std::string& path) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        std::filesystem::path log_path {path};
        std::string prefix = log_path.filename().string() + ".";

        std::filesystem::path log_dir = log_path.parent_path();

        if (log_dir.empty()) {
            log_dir = ".";
        }

        std::error_code iterate_error;

        for (std::filesystem::directory_iterator dir_it {log_dir, iterate_error}; !iterate_error && dir_it != std::filesystem::directory_iterator {}; dir_it.increment(iterate_error)) {
            std::error_code file_type_error;

            if (!dir_it->is_regular_file(file_type_error)) {
                continue;
            }

            std::string file_name = dir_it->path().filename().string();

            if (file_name.size() <= prefix.size() || file_name.compare(0, prefix.size(), prefix) != 0) {
                continue;
            }

            std::string_view part_index = std::string_view {file_name}.substr(prefix.size());

            if (!std::ranges::all_of(part_index, [](char ch) { return ch >= '0' && ch <= '9'; })) {
                continue;
            }

            std::error_code remove_error;
            (void)std::filesystem::remove(dir_it->path(), remove_error);
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

static auto IsNullLogDevicePath(string_view path) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return path.size() == 3 && (path[0] == 'N' || path[0] == 'n') && (path[1] == 'U' || path[1] == 'u') && (path[2] == 'L' || path[2] == 'l');
#else
    return path == "/dev/null";
#endif
}

static void FlushLogAtExit()
{
    if (BaseLogging != nullptr) {
        StopAsyncWorker();

        if (BaseLogging->LogLocker.try_lock()) {
            if (BaseLogging->LogFileHandle) {
                BaseLogging->LogFileHandle.close();
            }

            BaseLogging->LogLocker.unlock();
        }
    }
}

extern void SafeWriteStackTrace(const StackTraceData& st) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    char itoa_buf[64] = {};

    if (st.NativeTruncated) {
        WriteBaseLog("Stack trace (most recent call first, truncated at ");
        WriteBaseLog(ItoA(static_cast<int64_t>(STACK_TRACE_MAX_NATIVE_FRAMES), itoa_buf, 10));
        WriteBaseLog(" frames):\n");
    }
    else {
        WriteBaseLog("Stack trace (most recent call first):\n");
    }

    bool resolution_succeeded = false;

    try {
        auto resolved = ResolveStackTrace(st);

        for (const auto& frame : resolved) {
            WriteBaseLog("- [");
            WriteBaseLog(frame.Type == StackTraceFrame::FrameType::Script ? "Script" : "Native");
            WriteBaseLog("] ");
            WriteBaseLog(frame.Function);

            if (!frame.File.empty()) {
                std::string_view file_name {frame.File};

                if (auto pos = file_name.find_last_of("/\\"); pos != std::string_view::npos) {
                    file_name = file_name.substr(pos + 1);
                }

                WriteBaseLog(" (");
                WriteBaseLog(file_name);
                WriteBaseLog(" line ");
                WriteBaseLog(ItoA(static_cast<int64_t>(frame.Line), itoa_buf, 10));
                WriteBaseLog(")");
            }

            WriteBaseLog("\n");
        }

        resolution_succeeded = true;
    }
    catch (...) {
        resolution_succeeded = false;
    }

    if (!resolution_succeeded) {
        if (st.ScriptLayers) {
            for (const auto& layer : *st.ScriptLayers) {
                for (const auto& frame : layer.ScriptFrames) {
                    WriteBaseLog("- [Script] ");
                    WriteBaseLog(frame.Function);
                    WriteBaseLog("\n");
                }
            }
        }

        for (uint32_t i = 0; i < st.NativeFrameCount; i++) {
            WriteBaseLog("- [Native] 0x");
            NativeStackFrameAddress addr = st.NativeFrames[i];
            WriteBaseLog(ItoA(static_cast<int64_t>(addr), itoa_buf, 16));
            WriteBaseLog("\n");
        }
    }

    WriteBaseLog("\n");
}

FO_END_NAMESPACE
