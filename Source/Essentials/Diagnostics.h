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

#pragma once

#include "BasicCore.h"
#include "Containers.h"
#include "NetSockets.h"
#include "SmartPointers.h"
#include "StringUtils.h"
#include "Threading.h"
#include "TimeRelated.h"

FO_BEGIN_NAMESPACE

namespace diagnostics
{

    extern void here(string_view name = "here", const std::source_location& location = std::source_location::current()) noexcept;

    extern void checkpoint_message(string_view name, string message) noexcept;

    template<typename... args_type>
    void checkpoint(string_view name, std::format_string<args_type...>&& format, args_type&&... args) noexcept
    {
        checkpoint_message(name, strex(strex::safe_format, std::move(format), std::forward<args_type>(args)...));
    }

    extern void checkpoint(string_view name) noexcept;
    [[nodiscard]] extern auto stack_trace() -> string;
    extern void stack_checkpoint(string_view name);
    extern void thread_checkpoint(string_view name) noexcept;
    [[nodiscard]] extern auto debugger_attached() noexcept -> bool;
    [[nodiscard]] extern auto debug_break() noexcept -> bool;
    [[nodiscard]] extern auto debug_break_if(bool condition) noexcept -> bool;

    struct process_snapshot
    {
        string process_id {};
        size_t resident_memory_bytes {};
        size_t private_memory_bytes {};
    };

    [[nodiscard]] extern auto capture_process() noexcept -> process_snapshot;
    extern void process_checkpoint(string_view name) noexcept;

    struct memory_delta
    {
        int64_t resident_memory_bytes {};
        int64_t private_memory_bytes {};
    };

    [[nodiscard]] extern auto capture_memory_delta(const process_snapshot& before, const process_snapshot& after) noexcept -> memory_delta;

    class scoped_memory_delta final
    {
    public:
        explicit scoped_memory_delta(string_view name, size_t report_threshold_bytes = 0);
        scoped_memory_delta(const scoped_memory_delta&) = delete;
        scoped_memory_delta(scoped_memory_delta&&) noexcept = delete;
        auto operator=(const scoped_memory_delta&) -> scoped_memory_delta& = delete;
        auto operator=(scoped_memory_delta&&) noexcept -> scoped_memory_delta& = delete;
        ~scoped_memory_delta() noexcept;

        [[nodiscard]] auto delta() const noexcept -> memory_delta;
        void cancel() noexcept;

    private:
        string _name;
        process_snapshot _started;
        size_t _report_threshold_bytes {};
        bool _cancelled {};
    };

    [[nodiscard]] extern auto hex_dump(const_span<uint8_t> data, size_t max_bytes = 256, size_t bytes_per_line = 16) noexcept -> string;
    extern void hex_checkpoint(string_view name, const_span<uint8_t> data, size_t max_bytes = 256, size_t bytes_per_line = 16) noexcept;
    [[nodiscard]] extern auto write_text_dump(string_view path, string_view content) -> bool;
    [[nodiscard]] extern auto write_binary_dump(string_view path, const_span<uint8_t> content) -> bool;
    [[nodiscard]] extern auto write_hex_dump(string_view path, const_span<uint8_t> data, size_t max_bytes = 0, size_t bytes_per_line = 16) -> bool;

    class scoped_timer final
    {
    public:
        explicit scoped_timer(string_view name, timespan report_threshold = timespan::zero, bool capture_stack_on_exception = false);
        scoped_timer(const scoped_timer&) = delete;
        scoped_timer(scoped_timer&&) noexcept = delete;
        auto operator=(const scoped_timer&) -> scoped_timer& = delete;
        auto operator=(scoped_timer&&) noexcept -> scoped_timer& = delete;
        ~scoped_timer() noexcept;

        [[nodiscard]] auto elapsed() const noexcept -> timespan;
        auto lap(string_view name = "lap") noexcept -> timespan;
        void cancel() noexcept;

    private:
        string _name;
        nanotime _started_at;
        nanotime _last_lap_at;
        timespan _report_threshold;
        bool _capture_stack_on_exception {};
        int32_t _uncaught_exceptions_at_start {};
        bool _cancelled {};
    };

    class one_shot_trigger final
    {
    public:
        explicit one_shot_trigger(string_view environment_name);
        one_shot_trigger(const one_shot_trigger&) = delete;
        one_shot_trigger(one_shot_trigger&&) noexcept = delete;
        auto operator=(const one_shot_trigger&) -> one_shot_trigger& = delete;
        auto operator=(one_shot_trigger&&) noexcept -> one_shot_trigger& = delete;
        ~one_shot_trigger() = default;

        [[nodiscard]] auto is_set() const noexcept -> bool;
        [[nodiscard]] auto consume() -> optional<string>;
        void reset() noexcept;

    private:
        string _environment_name;
        std::atomic_bool _consumed {};
    };

    class hit_counter final
    {
    public:
        explicit hit_counter(string_view name, uint64_t report_first = 1, uint64_t report_every = 1);
        hit_counter(const hit_counter&) = delete;
        hit_counter(hit_counter&&) noexcept = delete;
        auto operator=(const hit_counter&) -> hit_counter& = delete;
        auto operator=(hit_counter&&) noexcept -> hit_counter& = delete;
        ~hit_counter() = default;

        [[nodiscard]] auto hit() noexcept -> bool;
        [[nodiscard]] auto count() const noexcept -> uint64_t;
        void report() const noexcept;

    private:
        string _name;
        uint64_t _report_first {};
        uint64_t _report_every {};
        std::atomic_uint64_t _count {};
    };

    struct rate_snapshot
    {
        uint64_t event_count {};
        timespan elapsed {};
        float64_t events_per_second {};
    };

    class rate_counter final
    {
    public:
        explicit rate_counter(string_view name, timespan report_interval = std::chrono::seconds {1});
        rate_counter(const rate_counter&) = delete;
        rate_counter(rate_counter&&) noexcept = delete;
        auto operator=(const rate_counter&) -> rate_counter& = delete;
        auto operator=(rate_counter&&) noexcept -> rate_counter& = delete;
        ~rate_counter() = default;

        [[nodiscard]] auto hit(uint64_t amount = 1) -> optional<rate_snapshot>;
        [[nodiscard]] auto snapshot() const -> rate_snapshot;
        void clear();

    private:
        [[nodiscard]] static auto make_snapshot(uint64_t event_count, timespan elapsed) noexcept -> rate_snapshot;

        string _name;
        timespan _report_interval;
        mutable mutex _locker;
        uint64_t _event_count FO_TSA_GUARDED_BY(_locker) {};
        nanotime _started_at FO_TSA_GUARDED_BY(_locker);
    };

    struct duration_snapshot
    {
        uint64_t sample_count {};
        timespan total {};
        timespan minimum {};
        timespan maximum {};
        timespan latest {};
        timespan average {};
    };

    class duration_statistics final
    {
    public:
        explicit duration_statistics(string_view name);
        duration_statistics(const duration_statistics&) = delete;
        duration_statistics(duration_statistics&&) noexcept = delete;
        auto operator=(const duration_statistics&) -> duration_statistics& = delete;
        auto operator=(duration_statistics&&) noexcept -> duration_statistics& = delete;
        ~duration_statistics() = default;

        void add(timespan sample);
        [[nodiscard]] auto snapshot() const -> duration_snapshot;
        void dump() const;
        void clear();

    private:
        string _name;
        mutable mutex _locker;
        uint64_t _sample_count FO_TSA_GUARDED_BY(_locker) {};
        timespan _total FO_TSA_GUARDED_BY(_locker) {};
        timespan _minimum FO_TSA_GUARDED_BY(_locker) {};
        timespan _maximum FO_TSA_GUARDED_BY(_locker) {};
        timespan _latest FO_TSA_GUARDED_BY(_locker) {};
    };

    class scoped_duration final
    {
    public:
        explicit scoped_duration(duration_statistics& statistics) noexcept;
        scoped_duration(const scoped_duration&) = delete;
        scoped_duration(scoped_duration&&) noexcept = delete;
        auto operator=(const scoped_duration&) -> scoped_duration& = delete;
        auto operator=(scoped_duration&&) noexcept -> scoped_duration& = delete;
        ~scoped_duration() noexcept;

        [[nodiscard]] auto elapsed() const noexcept -> timespan;
        void cancel() noexcept;

    private:
        nptr<duration_statistics> _statistics;
        nanotime _started_at;
        bool _cancelled {};
    };

    class fault_injector final
    {
    public:
        explicit fault_injector(string_view name, uint64_t inject_at = 1, uint64_t repeat_every = 0);
        fault_injector(const fault_injector&) = delete;
        fault_injector(fault_injector&&) noexcept = delete;
        auto operator=(const fault_injector&) -> fault_injector& = delete;
        auto operator=(fault_injector&&) noexcept -> fault_injector& = delete;
        ~fault_injector() = default;

        [[nodiscard]] auto hit() noexcept -> bool;
        [[nodiscard]] auto count() const noexcept -> uint64_t;
        void reset() noexcept;

    private:
        string _name;
        uint64_t _inject_at {};
        uint64_t _repeat_every {};
        std::atomic_uint64_t _count {};
    };

    extern void inject_delay(timespan delay) noexcept;
    [[nodiscard]] extern auto corrupt_bytes(span<uint8_t> data, size_t offset = 0, size_t count = 1, uint8_t xor_mask = 0xFF) noexcept -> size_t;

    class udp_sender final
    {
    public:
        explicit udp_sender(string_view host, uint16_t port, string_view bind_host = "0.0.0.0");
        udp_sender(const udp_sender&) = delete;
        udp_sender(udp_sender&&) noexcept = delete;
        auto operator=(const udp_sender&) -> udp_sender& = delete;
        auto operator=(udp_sender&&) noexcept -> udp_sender& = delete;
        ~udp_sender() = default;

        [[nodiscard]] auto is_ready() const -> bool;
        [[nodiscard]] auto send_bytes(const_span<uint8_t> data) -> int32_t;
        [[nodiscard]] auto send_message(string_view message) -> int32_t;

    private:
        string _host;
        uint16_t _port {};
        mutable mutex _locker;
        udp_socket _socket FO_TSA_GUARDED_BY(_locker);
    };

    inline constexpr uint32_t pcap_link_ethernet = 1;
    inline constexpr uint32_t pcap_link_raw_ip = 101;
    inline constexpr uint32_t pcap_link_user0 = 147;

    class pcap_writer final
    {
    public:
        explicit pcap_writer(string_view path, uint32_t link_type = pcap_link_user0, uint32_t snap_length = 65535);
        pcap_writer(const pcap_writer&) = delete;
        pcap_writer(pcap_writer&&) noexcept = delete;
        auto operator=(const pcap_writer&) -> pcap_writer& = delete;
        auto operator=(pcap_writer&&) noexcept -> pcap_writer& = delete;
        ~pcap_writer() = default;

        [[nodiscard]] auto is_ready() const -> bool;
        [[nodiscard]] auto write_packet(const_span<uint8_t> data, size_t original_size = 0) -> bool;
        [[nodiscard]] auto flush() -> bool;
        [[nodiscard]] auto path() const noexcept -> string_view;

    private:
        string _path;
        uint32_t _snap_length {};
        mutable mutex _locker;
        std::ofstream _file FO_TSA_GUARDED_BY(_locker);
    };

    class timeout_watchdog final
    {
    public:
        explicit timeout_watchdog(string_view name, timespan timeout, function<void()> on_timeout = {}, bool break_into_debugger = false);
        timeout_watchdog(const timeout_watchdog&) = delete;
        timeout_watchdog(timeout_watchdog&&) noexcept = delete;
        auto operator=(const timeout_watchdog&) -> timeout_watchdog& = delete;
        auto operator=(timeout_watchdog&&) noexcept -> timeout_watchdog& = delete;
        ~timeout_watchdog() noexcept;

        [[nodiscard]] auto expired() const noexcept -> bool;
        void cancel() noexcept;

    private:
        struct state;

        static void run(shared_ptr<state> watchdog_state) noexcept;

        shared_ptr<state> _state;
        thread _worker;
    };

    template<typename value_type>
    class change_detector final
    {
    public:
        change_detector() = default;
        change_detector(const change_detector&) = delete;
        change_detector(change_detector&&) noexcept = default;
        auto operator=(const change_detector&) -> change_detector& = delete;
        auto operator=(change_detector&&) noexcept -> change_detector& = default;
        ~change_detector() = default;

        [[nodiscard]] auto changed(const value_type& value) -> bool
        {
            if (!_value.has_value() || *_value != value) {
                _value = value;
                return true;
            }

            return false;
        }

        [[nodiscard]] auto value() const noexcept -> const optional<value_type>& { return _value; }
        void reset() noexcept { _value.reset(); }

    private:
        optional<value_type> _value {};
    };

    struct history_entry
    {
        uint64_t sequence {};
        timespan since_start {};
        string channel {};
        string message {};
    };

    class event_history final
    {
    public:
        explicit event_history(string_view name, size_t capacity = 128);
        event_history(const event_history&) = delete;
        event_history(event_history&&) noexcept = delete;
        auto operator=(const event_history&) -> event_history& = delete;
        auto operator=(event_history&&) noexcept -> event_history& = delete;
        ~event_history() = default;

        template<typename... args_type>
        void record(string_view channel, std::format_string<args_type...>&& format, args_type&&... args)
        {
            record_message(channel, strex(strex::safe_format, std::move(format), std::forward<args_type>(args)...));
        }

        void record_message(string_view channel, string message);
        void record_stack(string_view channel, string_view message);
        [[nodiscard]] auto snapshot() const -> vector<history_entry>;
        void dump() const;
        void clear();

    private:
        string _name;
        size_t _capacity {};
        mutable mutex _locker;
        vector<history_entry> _entries FO_TSA_GUARDED_BY(_locker);
        uint64_t _sequence FO_TSA_GUARDED_BY(_locker) {};
        nanotime _started_at FO_TSA_GUARDED_BY(_locker);
    };

} // namespace diagnostics

FO_END_NAMESPACE
