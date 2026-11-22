// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "BasicCore.h"
#include "Containers.h"
#include "SmartPointers.h"

#if FO_WINDOWS

FO_BEGIN_NAMESPACE

// The Windows API the engine calls, and the only place its header is included. <Windows.h> defines macros over
// hundreds of ordinary words, so every file that pulls it in has to undo them again through WinApiUndef.inc
namespace winapi
{
    struct cpu_core_times
    {
        uint64_t idle_time {};
        uint64_t total_time {};
    };

    void output_debug_string(const string& text) noexcept;

    // Absent before Windows 10 1607, so the entry point is resolved at call time and absence is not an error
    [[nodiscard]] auto set_thread_description(const string& name) noexcept -> bool;

    [[nodiscard]] auto get_current_process_id() noexcept -> uint32_t;
    // The creation time of a process that is still running, in 100 ns ticks; none once it has exited or cannot be
    // opened. With the id it names one process for good, since the id alone is handed on after the process ends
    [[nodiscard]] auto get_running_process_start_time(uint32_t pid) noexcept -> optional<uint64_t>;
    [[nodiscard]] auto get_module_file_name() noexcept -> optional<string>;
    // Read through the wide API: the narrow environment is in the ANSI code page, which cannot carry every profile path
    [[nodiscard]] auto get_environment_variable(const string& name) noexcept -> optional<string>;
    // The wide command line split the way the C runtime splits argv, but kept in UTF-8
    [[nodiscard]] auto get_command_line_args() -> optional<vector<string>>;
    // Asked of the shell, not the environment: a service or a sandbox has no LOCALAPPDATA, and answering
    // "none" sends the caller back to a directory it may not be allowed to write
    [[nodiscard]] auto get_local_app_data_path() noexcept -> optional<string>;
    [[nodiscard]] auto get_process_working_set_size() noexcept -> size_t;
    [[nodiscard]] auto get_process_private_usage() noexcept -> size_t;
    [[nodiscard]] auto get_process_cpu_time_ns() noexcept -> optional<uint64_t>;
    [[nodiscard]] auto get_active_processor_count() noexcept -> uint32_t;
    [[nodiscard]] auto get_system_cpu_times() noexcept -> optional<cpu_core_times>;
    [[nodiscard]] auto fill_system_random(span<uint8_t> buf) noexcept -> bool;
    [[nodiscard]] auto install_console_interrupt_handler() noexcept -> bool;
    [[nodiscard]] auto is_console_interrupt_received() noexcept -> bool;

    [[nodiscard]] auto load_library(const string& path) noexcept -> nptr<void>;
    // Loaded and pinned: FreeLibrary no longer unmaps it, and it stays until the process exits
    [[nodiscard]] auto load_pinned_library(const string& path) noexcept -> nptr<void>;
    [[nodiscard]] auto is_library_loaded(const string& name) noexcept -> bool;
    void free_library(nptr<void> module_handle) noexcept;
    // A null module handle means the running executable, which is where the loader search starts
    [[nodiscard]] auto get_proc_address(nptr<void> module_handle, const string& func_name) noexcept -> nptr<void>;
    // Code and system text of the calling thread's last error, read right after the call that failed
    [[nodiscard]] auto get_last_error_text() noexcept -> string;
    [[nodiscard]] auto get_os_version() noexcept -> string;

    // A file this process holds alone: a second opener is refused rather than allowed to share it. The
    // descriptor is the platform's own, and -1 means the open failed
    [[nodiscard]] auto open_exclusive_file(const string& path) noexcept -> int32_t;
    void close_exclusive_file(int32_t fd) noexcept;
    [[nodiscard]] auto seek_file_end(int32_t fd) noexcept -> int64_t;
    [[nodiscard]] auto seek_file_begin(int32_t fd) noexcept -> bool;
    [[nodiscard]] auto read_file_chunk(int32_t fd, ptr<char> buffer, size_t size) noexcept -> int64_t;
    [[nodiscard]] auto write_file_chunk(int32_t fd, ptr<const char> data, size_t size) noexcept -> int64_t;
    [[nodiscard]] auto truncate_file(int32_t fd) noexcept -> bool;
    [[nodiscard]] auto sync_file(int32_t fd) noexcept -> bool;
    // Windows flushes a directory only through a handle opened for writing, so a read-only directory answers false
    [[nodiscard]] auto sync_directory(const string& path) noexcept -> bool;
    [[nodiscard]] auto lock_named_mutex(const string& name) noexcept -> nptr<void>;
    void unlock_named_mutex(nptr<void> lock) noexcept;
    [[nodiscard]] auto rename_file_durable(const string& from, const string& to) noexcept -> bool;
    [[nodiscard]] auto replace_file(const string& from, const string& to) noexcept -> bool;

    // A pack file opened for shared positional reading: the offset travels with each call, so a reader never
    // depends on a shared file cursor. The descriptor is the platform own one, and -1 means the open failed
    [[nodiscard]] auto open_shared_read_file(const string& path) noexcept -> int32_t;
    [[nodiscard]] auto open_new_write_file(const string& path, bool append = false, bool exclusive = false) noexcept -> int32_t;
    [[nodiscard]] auto resize_file(int32_t fd, uint64_t size) noexcept -> bool;
    void close_file(int32_t fd) noexcept;
    [[nodiscard]] auto get_file_size(int32_t fd) noexcept -> int64_t;
    [[nodiscard]] auto read_file_at(int32_t fd, uint64_t offset, ptr<uint8_t> buffer, size_t size) noexcept -> int64_t;
    // Claims the space up front so a long write fails early instead of part way through
    [[nodiscard]] auto preallocate_file(int32_t fd, uint64_t size) noexcept -> bool;

    // Runs a command with its output captured, chunk by chunk through the callback, returning its exit code or
    // -1. Not noexcept: the callback is caller code, and a throw from it must reach the caller, not end here
    [[nodiscard]] auto run_process_capturing_output(const string& command, const function<void(string_view)>& on_output) -> int32_t;

    // Per-user registry values, always REG_SZ holding raw UTF-8 so the round trip preserves every byte
    [[nodiscard]] auto registry_read_value(const string& sub_key, const string& name) noexcept -> optional<string>;
    [[nodiscard]] auto registry_write_value(const string& sub_key, const string& name, const string& value) noexcept -> bool;
    void registry_delete_value(const string& sub_key, const string& name) noexcept;

    // Sub-millisecond waits: the plain sleep rounds its request up to the timer tick, this does not
    [[nodiscard]] auto create_high_resolution_timer() noexcept -> nptr<void>;
    [[nodiscard]] auto set_relative_timer(nptr<void> timer, int64_t delay_100ns) noexcept -> bool;
    void wait_for_object(nptr<void> handle) noexcept;
    void close_handle(nptr<void> handle) noexcept;

    // The exception handler runs on the faulting thread, the report handler after it on a thread of its own with that
    // thread's CONTEXT and handle, so a stack overflow is still reported
    struct crash_handlers
    {
        void (*on_exception)(uint32_t code, uint32_t flags, nptr<const void> address, nptr<const void> context) noexcept {};
        void (*on_report)(nptr<const void> context, nptr<void> thread) noexcept {};
        void (*on_signal)(int32_t signum) noexcept {};
        void (*on_runtime_error)(string_view reason) noexcept {};
    };

    void install_crash_handlers(const crash_handlers& handlers) noexcept;
}

FO_END_NAMESPACE

#endif
