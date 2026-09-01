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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

constexpr size_t async_queue_drop_limit = 100000;

static void start_async_worker();
static void stop_async_worker() noexcept;
static void async_worker_loop() noexcept;
static void write_sync(string_view message) noexcept;
static void flush_log_at_exit();

struct base_logging_data
{
    base_logging_data()
    {
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
        int32_t result = std::at_quick_exit(flush_log_at_exit);
        ignore_unused(result);
#else
        ignore_unused(flush_log_at_exit);
#endif

#if FO_WINDOWS
        SetConsoleOutputCP(CP_UTF8);
#endif
    }

    ~base_logging_data() { stop_async_worker(); }

    struct async_entry
    {
        std::string message {};
        std::optional<catched_stack_trace_data> stack_trace_of {};
    };

    std::mutex log_locker {};
    std::ofstream log_file_handle {};
    std::atomic_bool async_enabled {};
    std::mutex async_queue_mutex {};
    std::condition_variable async_signal {};
    std::deque<async_entry> async_queue {};
    size_t async_dropped_count {};
    bool async_finish {};
    std::thread async_worker {};
};
FO_GLOBAL_DATA(base_logging_data, base_logging);

extern void log_to_file(string_view path, bool append)
{
    if (build_condition<FO_WEB>()) {
        return;
    }

    bool open_failed = false;

    {
        std::scoped_lock locker {base_logging->log_locker};

        if (base_logging->log_file_handle.is_open()) {
            base_logging->log_file_handle.close();
        }

        std::ios_base::openmode open_mode = std::ios::out | std::ios::binary | (append ? std::ios::app : std::ios::trunc);
        base_logging->log_file_handle.open(std::string(path), open_mode);

        if (!base_logging->log_file_handle) {
            open_failed = true;
        }
    }

    if (open_failed) {
        write_base_log(std::string("Can't create log file '").append(path).append("'\n"));
    }
}

extern void set_async_log_writing(bool enabled)
{
    if (build_condition<FO_WEB>()) {
        return;
    }

    if (enabled) {
        start_async_worker();
    }
    else {
        stop_async_worker();
    }
}

extern void suspend_async_log_writing() noexcept
{
    if (base_logging != nullptr) {
        base_logging->async_enabled.store(false, std::memory_order_release);
    }
}

extern void write_base_log(string_view message, const catched_stack_trace_data* st) noexcept
{
    try {
        if (base_logging == nullptr) {
            std::cout << message;

            if (st != nullptr) {
                std::cout << format_stack_trace(*st) << "\n";
            }

            std::cout.flush();
            return;
        }

        if (base_logging->async_enabled.load(std::memory_order_acquire)) {
            bool enqueued = false;
            bool dropped = false;

            {
                std::scoped_lock queue_lock {base_logging->async_queue_mutex};

                if (base_logging->async_enabled.load(std::memory_order_relaxed)) {
                    if (base_logging->async_queue.size() >= async_queue_drop_limit) {
                        base_logging->async_dropped_count++;
                        dropped = true;
                    }
                    else {
                        base_logging_data::async_entry entry;
                        entry.message.assign(message);

                        if (st != nullptr) {
                            entry.stack_trace_of = *st;
                        }

                        base_logging->async_queue.emplace_back(std::move(entry));
                        enqueued = true;
                    }
                }
            }

            if (enqueued) {
                base_logging->async_signal.notify_one();
            }

            if (enqueued || dropped) {
                return;
            }
        }

        if (st != nullptr) {
            std::string combined;
            combined.reserve(message.size() + 256);
            combined.append(message);
            combined.append(format_stack_trace(*st));
            combined.append("\n");
            write_sync(combined);
        }
        else {
            write_sync(message);
        }
    }
    catch (...) {
        break_into_debugger();
    }
}

static void start_async_worker()
{
    if (base_logging->async_worker.joinable()) {
        return;
    }

    {
        std::scoped_lock queue_lock {base_logging->async_queue_mutex};

        base_logging->async_finish = false;
    }

    base_logging->async_worker = std::thread([] { async_worker_loop(); });
    base_logging->async_enabled.store(true, std::memory_order_release);
}

static void stop_async_worker() noexcept
{
    if (!base_logging->async_worker.joinable()) {
        base_logging->async_enabled.store(false, std::memory_order_release);
        return;
    }

    {
        std::scoped_lock queue_lock {base_logging->async_queue_mutex};

        base_logging->async_enabled.store(false, std::memory_order_release);
        base_logging->async_finish = true;
    }

    base_logging->async_signal.notify_all();

    try {
        base_logging->async_worker.join();
    }
    catch (...) {
        break_into_debugger();
    }
}

static void async_worker_loop() noexcept
{
    try {
        std::deque<base_logging_data::async_entry> drained;
        size_t drained_drop_count = 0;

        while (true) {
            {
                std::unique_lock queue_lock {base_logging->async_queue_mutex};

                while (!base_logging->async_finish && base_logging->async_queue.empty() && base_logging->async_dropped_count == 0) {
                    base_logging->async_signal.wait(queue_lock);
                }

                drained.swap(base_logging->async_queue);
                drained_drop_count = base_logging->async_dropped_count;
                base_logging->async_dropped_count = 0;

                if (drained.empty() && drained_drop_count == 0 && base_logging->async_finish) {
                    return;
                }
            }

            for (const auto& entry : drained) {
                if (entry.stack_trace_of.has_value()) {
                    try {
                        std::string combined;
                        combined.reserve(entry.message.size() + 256);
                        combined.append(entry.message);
                        combined.append(format_stack_trace(entry.stack_trace_of.value()));
                        combined.append("\n");
                        write_sync(combined);
                    }
                    catch (...) {
                        write_sync(entry.message);
                        break_into_debugger();
                    }
                }
                else {
                    write_sync(entry.message);
                }
            }

            if (drained_drop_count != 0) {
                std::string drop_notice = "Dropped ";
                drop_notice += std::to_string(drained_drop_count);
                drop_notice += " log messages due to high volume (queue limit ";
                drop_notice += std::to_string(async_queue_drop_limit);
                drop_notice += ")\n";
                write_sync(drop_notice);
            }

            drained.clear();
            drained_drop_count = 0;
        }
    }
    catch (...) {
        break_into_debugger();
    }
}

static void write_sync(string_view message) noexcept
{
    try {
        std::scoped_lock locker {base_logging->log_locker};

        if (base_logging->log_file_handle) {
            base_logging->log_file_handle.seekp(0, std::ios::end);
            base_logging->log_file_handle << message;
            base_logging->log_file_handle.flush();
        }

        std::cout << message;
        std::cout.flush();
    }
    catch (...) {
        break_into_debugger();
    }
}

static void flush_log_at_exit()
{
    if (base_logging != nullptr) {
        stop_async_worker();

        if (base_logging->log_locker.try_lock()) {
            if (base_logging->log_file_handle) {
                base_logging->log_file_handle.close();
            }

            base_logging->log_locker.unlock();
        }
    }
}

extern void safe_write_stack_trace(const stack_trace_data& st) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    char itoa_buf[64] = {};

    if (st.native_truncated) {
        write_base_log("Stack trace (most recent call first, truncated at ");
        write_base_log(itoa(static_cast<int64_t>(STACK_TRACE_MAX_NATIVE_FRAMES), itoa_buf, 10));
        write_base_log(" frames):\n");
    }
    else {
        write_base_log("Stack trace (most recent call first):\n");
    }

    bool resolution_succeeded = false;

    try {
        auto resolved = resolve_stack_trace(st);

        for (const auto& frame : resolved) {
            write_base_log("- [");
            write_base_log(frame.type == stack_trace_frame::frame_type::script ? "Script" : "Native");
            write_base_log("] ");
            write_base_log(frame.function);

            if (!frame.file.empty()) {
                std::string_view file_name {frame.file};

                if (auto pos = file_name.find_last_of("/\\"); pos != std::string_view::npos) {
                    file_name = file_name.substr(pos + 1);
                }

                write_base_log(" (");
                write_base_log(file_name);
                write_base_log(" line ");
                write_base_log(itoa(static_cast<int64_t>(frame.line), itoa_buf, 10));
                write_base_log(")");
            }

            write_base_log("\n");
        }

        resolution_succeeded = true;
    }
    catch (...) {
        resolution_succeeded = false;
    }

    if (!resolution_succeeded) {
        if (st.script_layers) {
            for (const auto& layer : *st.script_layers) {
                for (const auto& frame : layer.script_frames) {
                    write_base_log("- [Script] ");
                    write_base_log(frame.function);
                    write_base_log("\n");
                }
            }
        }

        for (uint32_t i = 0; i < st.native_frame_count; i++) {
            write_base_log("- [Native] 0x");
            native_stack_frame_address addr = st.native_frames[i];
            write_base_log(itoa(static_cast<int64_t>(addr), itoa_buf, 16));
            write_base_log("\n");
        }
    }

    write_base_log("\n");
}

FO_END_NAMESPACE
