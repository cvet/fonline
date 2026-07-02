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
static void FlushLogAtExit();

struct BaseLoggingData
{
    BaseLoggingData()
    {
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
        const auto result = std::at_quick_exit(FlushLogAtExit);
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
    std::atomic_bool AsyncEnabled {};
    std::mutex AsyncQueueMutex {};
    std::condition_variable AsyncSignal {};
    std::deque<AsyncEntry> AsyncQueue {};
    size_t AsyncDroppedCount {};
    bool AsyncFinish {};
    std::thread AsyncWorker {};
};
FO_GLOBAL_DATA(BaseLoggingData, BaseLogging);

extern void LogToFile(string_view path, bool append)
{
    if (build_condition<FO_WEB>()) {
        return;
    }

    bool open_failed = false;

    {
        std::scoped_lock locker {BaseLogging->LogLocker};

        if (BaseLogging->LogFileHandle.is_open()) {
            BaseLogging->LogFileHandle.close();
        }

        const auto open_mode = std::ios::out | std::ios::binary | (append ? std::ios::app : std::ios::trunc);
        BaseLogging->LogFileHandle.open(std::string(path), open_mode);

        if (!BaseLogging->LogFileHandle) {
            open_failed = true;
        }
    }

    if (open_failed) {
        WriteBaseLog(std::string("Can't create log file '").append(path).append("'\n"));
    }
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
        BaseLogging->AsyncEnabled.store(true, std::memory_order_release);
    }

    BaseLogging->AsyncWorker = std::thread([] { AsyncWorkerLoop(); });
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
    try {
        std::scoped_lock locker {BaseLogging->LogLocker};

        if (BaseLogging->LogFileHandle) {
            BaseLogging->LogFileHandle.seekp(0, std::ios::end);
            BaseLogging->LogFileHandle << message;
            BaseLogging->LogFileHandle.flush();
        }

        std::cout << message;
        std::cout.flush();
    }
    catch (...) {
        BreakIntoDebugger();
    }
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
        const auto resolved = ResolveStackTrace(st);

        for (const auto& frame : resolved) {
            WriteBaseLog("- [");
            WriteBaseLog(frame.Type == StackTraceFrame::FrameType::Script ? "Script" : "Native");
            WriteBaseLog("] ");
            WriteBaseLog(frame.Function);

            if (!frame.File.empty()) {
                std::string_view file_name {frame.File};

                if (const auto pos = file_name.find_last_of("/\\"); pos != std::string_view::npos) {
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
            const NativeStackFrameAddress addr = st.NativeFrames[i];
            WriteBaseLog(ItoA(static_cast<int64_t>(addr), itoa_buf, 16));
            WriteBaseLog("\n");
        }
    }

    WriteBaseLog("\n");
}

FO_END_NAMESPACE
