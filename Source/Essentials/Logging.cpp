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

#include "Logging.h"
#include "BaseLogging.h"
#include "GlobalData.h"
#include "Platform.h"
#include "StackTrace.h"
#include "TimeRelated.h"
#include "WorkThread.h"

FO_BEGIN_NAMESPACE

static void emit_log_message(logging::type type, string_view message, nptr<const stack_trace::catched_data> st);
static void flush_log_message_repeats_locked();
static auto is_same_as_last_log_message(logging::type type, string_view message) -> bool;
static void remember_last_log_message(logging::type type, string_view message) noexcept;
static void clear_last_log_message() noexcept;
static void flush_log_at_exit();

struct logging_data
{
    logging_data()
    {
        FO_STACK_TRACE_ENTRY();

#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
        (void)std::at_quick_exit(flush_log_at_exit);
#else
        ignore_unused(flush_log_at_exit);
#endif

        main_thread_id = std::this_thread::get_id();
    }

    std::recursive_mutex locker {};
    vector<pair<string, logging::callback>> log_functions {};
    std::atomic_bool log_functions_in_process {};
    std::thread::id main_thread_id {};
    optional<logging::type> last_log_type {};
    string last_log_message {};
    uint64_t same_log_message_count {};
    bool tags_disabled {};
};
FO_GLOBAL_DATA(logging_data, log_state);

void logging::write_message(logging::type type, string_view message, nptr<const stack_trace::catched_data> st) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        if (log_state == nullptr) {
            string result;
            result.reserve(message.length() + 1);
            result += message;
            result += '\n';

            if (st) {
                logging::write_base(result, st.get());
            }
            else {
                logging::write_base(result);
            }

            return;
        }

        std::scoped_lock locker {log_state->locker};

        if (is_same_as_last_log_message(type, message)) {
            log_state->same_log_message_count++;
            return;
        }

        flush_log_message_repeats_locked();
        emit_log_message(type, message, st);
        remember_last_log_message(type, message);
    }
    catch (...) {
        break_into_debugger();
    }
}

void logging::set_callback(string_view key, logging::callback callback)
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock locker {log_state->locker};

    flush_log_message_repeats_locked();

    if (!key.empty()) {
        std::erase_if(log_state->log_functions, [key](auto&& e) { return e.first == key; });

        if (callback) {
            log_state->log_functions.emplace_back(key, std::move(callback));
        }
    }
    else {
        log_state->log_functions.clear();
    }
}

void logging::disable_tags()
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock locker {log_state->locker};

    flush_log_message_repeats_locked();

    log_state->tags_disabled = true;
}

static void emit_log_message(logging::type type, string_view message, nptr<const stack_trace::catched_data> st)
{
    FO_STACK_TRACE_ENTRY();

    // Make message
    string result;
    result.reserve(message.length() + 64);

    if (!log_state->tags_disabled) {
        time_desc_t time = nanotime::now().desc(true);
        result += strex("[{:02}/{:02}/{:02}] ", time.day, time.month, time.year % 100);
        result += strex("[{:02}:{:02}:{:02}] ", time.hour, time.minute, time.second);

        if (std::thread::id thread_id = std::this_thread::get_id(); thread_id != log_state->main_thread_id) {
            result += strex("[{}] ", get_this_thread_name());
        }
    }

    result += message;
    result += '\n';

    if (st) {
        logging::write_base(result, st.get());
    }
    else {
        logging::write_base(result);
    }

    if (!log_state->log_functions.empty()) {
        if (log_state->log_functions_in_process) {
            return;
        }

        log_state->log_functions_in_process = true;
        auto reset_in_process = scope_exit([]() noexcept { log_state->log_functions_in_process = false; });

        for (const auto& func : log_state->log_functions | std::views::values) {
            func(type, result, st);
        }
    }

    if constexpr (FO_DEBUG) {
        platform::info_log(result);
    }

#if FO_TRACY
    auto tracy_message = make_ptr(result.c_str());
    TracyMessage(tracy_message.get(), result.length());
#endif
}

static void flush_log_message_repeats_locked()
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!log_state->last_log_type.has_value()) {
        return;
    }

    optional<logging::type> last_log_type = log_state->last_log_type;
    uint64_t same_message_count = log_state->same_log_message_count;

    clear_last_log_message();

    if (same_message_count == 0) {
        return;
    }

    string repeat_message;

    if (same_message_count == 1) {
        repeat_message = "...and 1 more same message";
    }
    else {
        repeat_message = strex("...and {} more same messages", same_message_count);
    }

    emit_log_message(*last_log_type, repeat_message, nullptr);
}

static void flush_log_at_exit()
{
    FO_NO_STACK_TRACE_ENTRY();

    if (log_state != nullptr) {
        std::scoped_lock locker {log_state->locker};

        flush_log_message_repeats_locked();
    }
}

static auto is_same_as_last_log_message(logging::type type, string_view message) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return log_state->last_log_type.has_value() && *log_state->last_log_type == type && string_view {log_state->last_log_message} == message;
}

static void remember_last_log_message(logging::type type, string_view message) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    log_state->last_log_type = type;
    log_state->last_log_message.assign(message);
    log_state->same_log_message_count = 0;
}

static void clear_last_log_message() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    log_state->last_log_type.reset();
    log_state->last_log_message.clear();
    log_state->same_log_message_count = 0;
}

FO_END_NAMESPACE
