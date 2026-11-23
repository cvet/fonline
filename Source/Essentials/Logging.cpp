// FOnline Engine
// SPDX-License-Identifier: MIT

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
static void flush_message_log_at_exit();

struct logging_data
{
    logging_data()
    {
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
        (void)std::at_quick_exit(flush_message_log_at_exit);
#else
        ignore_unused(flush_message_log_at_exit);
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
    try {
        if (!log_state.is_created()) {
            string result {};
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
    std::scoped_lock locker {log_state->locker};

    flush_log_message_repeats_locked();

    log_state->tags_disabled = true;
}

static void emit_log_message(logging::type type, string_view message, nptr<const stack_trace::catched_data> st)
{
    // Make message
    string result {};
    result.reserve(message.length() + 64);

    if (!log_state->tags_disabled) {
        time_desc_t time {nanotime::now().desc(true)};
        result += strex("[{:02}/{:02}/{:02}] ", time.day, time.month, time.year % 100);
        result += strex("[{:02}:{:02}:{:02}] ", time.hour, time.minute, time.second);

        if (std::thread::id thread_id {std::this_thread::get_id()}; thread_id != log_state->main_thread_id) {
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
        auto reset_in_process {scope_exit([]() noexcept { log_state->log_functions_in_process = false; })};

        for (const auto& func : log_state->log_functions | std::views::values) {
            func(type, result, st);
        }
    }

    if constexpr (FO_DEBUG) {
        platform::info_log(result);
    }

#if FO_TRACE_CATEGORY_ENABLED(Log)
    auto tracy_message {make_ptr(result.c_str())};
    TracyMessage(tracy_message.get(), result.length());
#endif
}

static void flush_log_message_repeats_locked()
{
    if (!log_state->last_log_type.has_value()) {
        return;
    }

    optional<logging::type> last_log_type {log_state->last_log_type};
    uint64_t same_message_count {log_state->same_log_message_count};

    clear_last_log_message();

    if (same_message_count == 0) {
        return;
    }

    string repeat_message {};

    if (same_message_count == 1) {
        repeat_message = "...and 1 more same message";
    }
    else {
        repeat_message = strex("...and {} more same messages", same_message_count);
    }

    emit_log_message(*last_log_type, repeat_message, nullptr);
}

static void flush_message_log_at_exit()
{
    if (log_state.is_created()) {
        std::scoped_lock locker {log_state->locker};

        flush_log_message_repeats_locked();
    }
}

static auto is_same_as_last_log_message(logging::type type, string_view message) -> bool
{
    return log_state->last_log_type.has_value() && *log_state->last_log_type == type && string_view {log_state->last_log_message} == message;
}

static void remember_last_log_message(logging::type type, string_view message) noexcept
{
    log_state->last_log_type = type;
    log_state->last_log_message.assign(message);
    log_state->same_log_message_count = 0;
}

static void clear_last_log_message() noexcept
{
    log_state->last_log_type.reset();
    log_state->last_log_message.clear();
    log_state->same_log_message_count = 0;
}

FO_END_NAMESPACE
