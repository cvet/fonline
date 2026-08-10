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
static void WriteSync(const_span<byte> message) noexcept;
static void WriteConsole(const_span<byte> message) noexcept;
static void WriteAscii(string_view message) noexcept;
static auto CombineWithStackTrace(const_span<byte> message, const CatchedStackTraceData& st) -> std::vector<byte>;
static auto FormatDropNotice(size_t dropped_count) -> std::vector<byte>;
static void AppendAscii(std::vector<byte>& output, string_view text);
static void AppendDecimal(std::vector<byte>& output, size_t value);
static auto BytesForStream(const_span<byte> message) noexcept -> string_view;
static auto ToStreamSize(size_t size) noexcept -> std::streamsize;
static void FlushLogAtExit();

struct BaseLoggingData
{
    BaseLoggingData()
    {
        FO_NO_STACK_TRACE_ENTRY();

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

    ~BaseLoggingData()
    {
        FO_NO_STACK_TRACE_ENTRY();

        StopAsyncWorker();
    }

    struct AsyncEntry
    {
        std::vector<byte> Message {};
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

extern auto base_logging_detail::OpenLogFileNative(const std::filesystem::path& path, bool append) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (build_condition<FO_WEB>()) {
        return true;
    }

    std::scoped_lock locker {BaseLogging->LogLocker};

    if (BaseLogging->LogFileHandle.is_open()) {
        BaseLogging->LogFileHandle.close();
    }

    std::ios_base::openmode open_mode = std::ios::out | std::ios::binary | (append ? std::ios::app : std::ios::trunc);
    BaseLogging->LogFileHandle.open(path, open_mode);

    return !!BaseLogging->LogFileHandle;
}

extern void SetAsyncLogWriting(bool enabled)
{
    FO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    if (BaseLogging != nullptr) {
        BaseLogging->AsyncEnabled.store(false, std::memory_order_release);
    }
}

extern void WriteBaseLogBytes(const_span<byte> message, const CatchedStackTraceData* st) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        if (BaseLogging == nullptr) {
            WriteConsole(message);

            if (st != nullptr) {
                std::vector<byte> combined = CombineWithStackTrace({}, *st);
                WriteConsole(combined);
            }

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
                        entry.Message.assign(message.begin(), message.end());

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
            std::vector<byte> combined = CombineWithStackTrace(message, *st);
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
    FO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
                        std::vector<byte> combined = CombineWithStackTrace(entry.Message, entry.StackTrace.value());
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
                std::vector<byte> drop_notice = FormatDropNotice(drained_drop_count);
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

static void WriteSync(const_span<byte> message) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        std::scoped_lock locker {BaseLogging->LogLocker};
        string_view stream_message = BytesForStream(message);

        if (BaseLogging->LogFileHandle) {
            BaseLogging->LogFileHandle.seekp(0, std::ios::end);

            if (!stream_message.empty()) {
                BaseLogging->LogFileHandle.write(stream_message.data(), ToStreamSize(stream_message.size()));
            }

            BaseLogging->LogFileHandle.flush();
        }

        if (!stream_message.empty()) {
            std::cout.write(stream_message.data(), ToStreamSize(stream_message.size()));
        }

        std::cout.flush();
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

static void WriteConsole(const_span<byte> message) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        string_view stream_message = BytesForStream(message);

        if (!stream_message.empty()) {
            std::cout.write(stream_message.data(), ToStreamSize(stream_message.size()));
        }

        std::cout.flush();
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

static void WriteAscii(string_view message) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    WriteBaseLogBytes(std::as_bytes(const_span<char> {message.data(), message.size()}));
}

static auto CombineWithStackTrace(const_span<byte> message, const CatchedStackTraceData& st) -> std::vector<byte>
{
    FO_NO_STACK_TRACE_ENTRY();

    std::string formatted_stack = FormatStackTrace(st);
    const_span<byte> stack_bytes = std::as_bytes(const_span<char> {formatted_stack.data(), formatted_stack.size()});

    std::vector<byte> combined;
    combined.reserve(message.size() + stack_bytes.size() + 1);
    combined.insert(combined.end(), message.begin(), message.end());
    combined.insert(combined.end(), stack_bytes.begin(), stack_bytes.end());
    combined.emplace_back(byte {0x0A});
    return combined;
}

static auto FormatDropNotice(size_t dropped_count) -> std::vector<byte>
{
    FO_NO_STACK_TRACE_ENTRY();

    std::vector<byte> result;
    result.reserve(96);
    AppendAscii(result, "Dropped ");
    AppendDecimal(result, dropped_count);
    AppendAscii(result, " log messages due to high volume (queue limit ");
    AppendDecimal(result, AsyncQueueDropLimit);
    AppendAscii(result, ")\n");
    return result;
}

static void AppendAscii(std::vector<byte>& output, string_view text)
{
    FO_NO_STACK_TRACE_ENTRY();

    const_span<byte> bytes = std::as_bytes(const_span<char> {text.data(), text.size()});
    output.insert(output.end(), bytes.begin(), bytes.end());
}

static void AppendDecimal(std::vector<byte>& output, size_t value)
{
    FO_NO_STACK_TRACE_ENTRY();

    char buffer[32] = {};
    auto [end, error] = std::to_chars(std::begin(buffer), std::end(buffer), value);
    assert(error == std::errc {});
    size_t length = static_cast<size_t>(end - buffer);
    AppendAscii(output, string_view {buffer, length});
}

static auto BytesForStream(const_span<byte> message) noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    if (message.empty()) {
        return {};
    }

    return string_view {std::bit_cast<const char*>(message.data()), message.size()};
}

static auto ToStreamSize(size_t size) noexcept -> std::streamsize
{
    FO_NO_STACK_TRACE_ENTRY();

    // BaseLogging precedes SafeArithmetics in the Essentials DAG. Standard contiguous containers cannot
    // reach a size above PTRDIFF_MAX, which is also representable by streamsize on supported targets.
    assert(size <= static_cast<size_t>(std::numeric_limits<std::streamsize>::max()));
    return static_cast<std::streamsize>(size);
}

static void FlushLogAtExit()
{
    FO_NO_STACK_TRACE_ENTRY();

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
        WriteAscii("Stack trace (most recent call first, truncated at ");
        string_view frame_count = ItoA(static_cast<int64_t>(STACK_TRACE_MAX_NATIVE_FRAMES), itoa_buf, 10);
        WriteAscii(frame_count);
        WriteAscii(" frames):\n");
    }
    else {
        WriteAscii("Stack trace (most recent call first):\n");
    }

    bool resolution_succeeded = false;

    try {
        auto resolved = ResolveStackTrace(st);

        for (const auto& frame : resolved) {
            WriteAscii("- [");
            WriteAscii(frame.Type == StackTraceFrame::FrameType::Script ? "Script" : "Native");
            WriteAscii("] ");
            WriteBaseLogBytes(std::as_bytes(const_span<char> {frame.Function.data(), frame.Function.size()}));

            if (!frame.File.empty()) {
                std::string_view file_name {frame.File};

                if (auto pos = file_name.find_last_of("/\\"); pos != std::string_view::npos) {
                    file_name = file_name.substr(pos + 1);
                }

                WriteAscii(" (");
                WriteBaseLogBytes(std::as_bytes(const_span<char> {file_name.data(), file_name.size()}));
                WriteAscii(" line ");
                string_view line_number = ItoA(static_cast<int64_t>(frame.Line), itoa_buf, 10);
                WriteAscii(line_number);
                WriteAscii(")");
            }

            WriteAscii("\n");
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
                    WriteAscii("- [Script] ");
                    WriteBaseLogBytes(std::as_bytes(const_span<char> {frame.Function.data(), frame.Function.size()}));
                    WriteAscii("\n");
                }
            }
        }

        for (uint32_t i = 0; i < st.NativeFrameCount; i++) {
            WriteAscii("- [Native] 0x");
            NativeStackFrameAddress addr = st.NativeFrames[i];
            string_view address = ItoA(static_cast<int64_t>(addr), itoa_buf, 16);
            WriteAscii(address);
            WriteAscii("\n");
        }
    }

    WriteAscii("\n");
}

FO_END_NAMESPACE
