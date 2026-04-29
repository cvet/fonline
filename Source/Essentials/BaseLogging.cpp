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
#include "WinApi-Include.h"

FO_BEGIN_NAMESPACE

static void StartAsyncWorker();
static void StopAsyncWorker() noexcept;
static void AsyncWorkerLoop() noexcept;
static void WriteSync(string_view message) noexcept;
[[maybe_unused]] static void FlushLogAtExit();

struct BaseLoggingData
{
    BaseLoggingData()
    {
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
        const auto result = std::at_quick_exit(FlushLogAtExit);
        ignore_unused(result);
#endif

#if FO_WINDOWS
        SetConsoleOutputCP(CP_UTF8);
#endif
    }

    ~BaseLoggingData() { StopAsyncWorker(); }

    std::mutex LogLocker {};
    std::ofstream LogFileHandle {};
    std::atomic_bool AsyncEnabled {};
    std::mutex AsyncQueueMutex {};
    std::condition_variable AsyncSignal {};
    std::deque<std::string> AsyncQueue {};
    bool AsyncFinish {};
    std::thread AsyncWorker {};
};
FO_GLOBAL_DATA(BaseLoggingData, BaseLogging);

extern void LogToFile(string_view path)
{
    bool open_failed = false;

    {
        std::scoped_lock locker {BaseLogging->LogLocker};

        if (BaseLogging->LogFileHandle.is_open()) {
            BaseLogging->LogFileHandle.close();
        }

        BaseLogging->LogFileHandle.open(std::string(path), std::ios::out | std::ios::binary | std::ios::trunc);

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
    if (enabled) {
        StartAsyncWorker();
    }
    else {
        StopAsyncWorker();
    }
}

extern void WriteBaseLog(string_view message) noexcept
{
    try {
        if (BaseLogging->AsyncEnabled.load(std::memory_order_acquire)) {
            bool enqueued = false;

            {
                std::scoped_lock queue_lock {BaseLogging->AsyncQueueMutex};

                if (BaseLogging->AsyncEnabled.load(std::memory_order_relaxed)) {
                    BaseLogging->AsyncQueue.emplace_back(message);
                    enqueued = true;
                }
            }

            if (enqueued) {
                BaseLogging->AsyncSignal.notify_one();
                return;
            }
        }

        WriteSync(message);
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
        std::deque<std::string> drained;

        while (true) {
            {
                std::unique_lock queue_lock {BaseLogging->AsyncQueueMutex};

                BaseLogging->AsyncSignal.wait(queue_lock, [] { return BaseLogging->AsyncFinish || !BaseLogging->AsyncQueue.empty(); });

                drained.swap(BaseLogging->AsyncQueue);

                if (drained.empty() && BaseLogging->AsyncFinish) {
                    return;
                }
            }

            for (const auto& msg : drained) {
                WriteSync(msg);
            }

            drained.clear();
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
            std::scoped_lock locker {std::adopt_lock, BaseLogging->LogLocker};

            if (BaseLogging->LogFileHandle) {
                BaseLogging->LogFileHandle.close();
            }
        }
    }
}

FO_END_NAMESPACE
