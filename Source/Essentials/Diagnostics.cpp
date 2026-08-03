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

#include "Diagnostics.h"
#include "CommonHelpers.h"
#include "DiskFileSystem.h"
#include "Logging.h"
#include "MemorySystem.h"
#include "Platform.h"
#include "SafeArithmetics.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

namespace diagnostics
{

    static constexpr string_view diagnostic_log_prefix = "TEMP_DIAGNOSTIC";

    static auto make_memory_delta_component(size_t before, size_t after) noexcept -> int64_t
    {
        FO_NO_STACK_TRACE_ENTRY();

        size_t maximum_delta = const_numeric_cast<size_t>(std::numeric_limits<int64_t>::max());

        if (after >= before) {
            return numeric_cast<int64_t>(std::min(after - before, maximum_delta));
        }

        return -numeric_cast<int64_t>(std::min(before - after, maximum_delta));
    }

    static void append_u16_le(vector<uint8_t>& data, uint16_t value) noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        data.push_back(numeric_cast<uint8_t>(value & 0xFF));
        data.push_back(numeric_cast<uint8_t>((value >> 8) & 0xFF));
    }

    static void append_u32_le(vector<uint8_t>& data, uint32_t value) noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        data.push_back(numeric_cast<uint8_t>(value & 0xFF));
        data.push_back(numeric_cast<uint8_t>((value >> 8) & 0xFF));
        data.push_back(numeric_cast<uint8_t>((value >> 16) & 0xFF));
        data.push_back(numeric_cast<uint8_t>((value >> 24) & 0xFF));
    }

    void here(string_view name, const std::source_location& location) noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        WriteLog("[{}] {} at {}:{} ({})", diagnostic_log_prefix, name, location.file_name(), location.line(), location.function_name());
    }

    void checkpoint_message(string_view name, string message) noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        WriteLog("[{}] checkpoint {}: {}", diagnostic_log_prefix, name, message);
    }

    void checkpoint(string_view name) noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        WriteLog("[{}] checkpoint {}", diagnostic_log_prefix, name);
    }

    auto stack_trace() -> string
    {
        FO_NO_STACK_TRACE_ENTRY();

        StackTraceData captured_stack_trace = GetStackTrace();
        std::string formatted_stack_trace = FormatStackTrace(captured_stack_trace);
        return string {formatted_stack_trace.data(), formatted_stack_trace.size()};
    }

    void stack_checkpoint(string_view name)
    {
        FO_NO_STACK_TRACE_ENTRY();

        WriteLog("[{}] stack {}:\n{}", diagnostic_log_prefix, name, stack_trace());
    }

    void thread_checkpoint(string_view name) noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        string_view thread_name = get_this_thread_name();
        WriteLog("[{}] thread {}: {}", diagnostic_log_prefix, name, thread_name.empty() ? "<unnamed>" : thread_name);
    }

    auto debugger_attached() noexcept -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        return IsRunInDebugger();
    }

    auto debug_break() noexcept -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        return BreakIntoDebugger();
    }

    auto debug_break_if(bool condition) noexcept -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        return condition && debug_break();
    }

    auto capture_process() noexcept -> process_snapshot
    {
        FO_NO_STACK_TRACE_ENTRY();

        return process_snapshot {
            Platform::GetCurrentProcessIdStr(),
            Platform::GetProcessMemoryUsage(),
            Platform::GetProcessPrivateMemoryUsage(),
        };
    }

    void process_checkpoint(string_view name) noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        process_snapshot snapshot = capture_process();
        WriteLog("[{}] process {}: pid: {}, resident: {} bytes, private: {} bytes", diagnostic_log_prefix, name, snapshot.process_id, snapshot.resident_memory_bytes, snapshot.private_memory_bytes);
    }

    auto capture_memory_delta(const process_snapshot& before, const process_snapshot& after) noexcept -> memory_delta
    {
        FO_NO_STACK_TRACE_ENTRY();

        return memory_delta {
            make_memory_delta_component(before.resident_memory_bytes, after.resident_memory_bytes),
            make_memory_delta_component(before.private_memory_bytes, after.private_memory_bytes),
        };
    }

    scoped_memory_delta::scoped_memory_delta(string_view name, size_t report_threshold_bytes) :
        _name {name},
        _started {capture_process()},
        _report_threshold_bytes {report_threshold_bytes}
    {
        FO_NO_STACK_TRACE_ENTRY();
    }

    scoped_memory_delta::~scoped_memory_delta() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (_cancelled) {
            return;
        }

        memory_delta current = delta();
        uint64_t resident_magnitude = current.resident_memory_bytes >= 0 ? numeric_cast<uint64_t>(current.resident_memory_bytes) : numeric_cast<uint64_t>(-current.resident_memory_bytes);
        uint64_t private_magnitude = current.private_memory_bytes >= 0 ? numeric_cast<uint64_t>(current.private_memory_bytes) : numeric_cast<uint64_t>(-current.private_memory_bytes);

        if (std::max(resident_magnitude, private_magnitude) >= _report_threshold_bytes) {
            WriteLog("[{}] memory {}: resident {:+} bytes, private {:+} bytes", diagnostic_log_prefix, _name, current.resident_memory_bytes, current.private_memory_bytes);
        }
    }

    auto scoped_memory_delta::delta() const noexcept -> memory_delta
    {
        FO_NO_STACK_TRACE_ENTRY();

        return capture_memory_delta(_started, capture_process());
    }

    void scoped_memory_delta::cancel() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        _cancelled = true;
    }

    auto hex_dump(const_span<uint8_t> data, size_t max_bytes, size_t bytes_per_line) noexcept -> string
    {
        FO_NO_STACK_TRACE_ENTRY();

        size_t line_width = std::clamp<size_t>(bytes_per_line, 1, 64);
        size_t visible_size = max_bytes == 0 ? data.size() : std::min(data.size(), max_bytes);
        string result;

        for (size_t line_start = 0; line_start < visible_size; line_start += line_width) {
            result += strex("{:08x}  ", line_start);

            for (size_t column = 0; column < line_width; column++) {
                size_t byte_index = line_start + column;

                if (byte_index < visible_size) {
                    result += strex("{:02x} ", numeric_cast<uint32_t>(data[byte_index]));
                }
                else {
                    result += "   ";
                }

                if (column == 7 && line_width > 8) {
                    result += ' ';
                }
            }

            result += " |";

            for (size_t byte_index = line_start; byte_index < std::min(line_start + line_width, visible_size); byte_index++) {
                uint8_t value = data[byte_index];
                result += value >= 32 && value <= 126 ? numeric_cast<char>(value) : '.';
            }

            result += '|';

            if (line_start + line_width < visible_size) {
                result += '\n';
            }
        }

        if (visible_size < data.size()) {
            if (!result.empty()) {
                result += '\n';
            }

            result += strex("... {} byte(s) omitted", data.size() - visible_size);
        }

        return result;
    }

    void hex_checkpoint(string_view name, const_span<uint8_t> data, size_t max_bytes, size_t bytes_per_line) noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        WriteLog("[{}] hex {} ({} byte(s)):\n{}", diagnostic_log_prefix, name, data.size(), hex_dump(data, max_bytes, bytes_per_line));
    }

    auto write_text_dump(string_view path, string_view content) -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        bool written = fs_write_file(path, content);
        WriteLog("[{}] text dump {}: {} ({} byte(s))", diagnostic_log_prefix, path, written ? "written" : "failed", content.size());
        return written;
    }

    auto write_binary_dump(string_view path, const_span<uint8_t> content) -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        bool written = fs_write_file(path, content);
        WriteLog("[{}] binary dump {}: {} ({} byte(s))", diagnostic_log_prefix, path, written ? "written" : "failed", content.size());
        return written;
    }

    auto write_hex_dump(string_view path, const_span<uint8_t> data, size_t max_bytes, size_t bytes_per_line) -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        return write_text_dump(path, hex_dump(data, max_bytes, bytes_per_line));
    }

    scoped_timer::scoped_timer(string_view name, timespan report_threshold, bool capture_stack_on_exception) :
        _name {name},
        _started_at {nanotime::now()},
        _last_lap_at {_started_at},
        _report_threshold {report_threshold},
        _capture_stack_on_exception {capture_stack_on_exception},
        _uncaught_exceptions_at_start {std::uncaught_exceptions()}
    {
        FO_NO_STACK_TRACE_ENTRY();
    }

    scoped_timer::~scoped_timer() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (_cancelled) {
            return;
        }

        nanotime finished_at = nanotime::now();
        timespan elapsed_time = finished_at - _started_at;
        bool unwinding = std::uncaught_exceptions() > _uncaught_exceptions_at_start;

        if (unwinding || elapsed_time >= _report_threshold) {
            WriteLog("[{}] timing {}: {}{}", diagnostic_log_prefix, _name, elapsed_time, unwinding ? " (exception unwinding)" : "");
        }

        if (unwinding && _capture_stack_on_exception) {
            StackTraceData stack_trace = GetStackTrace();
            SafeWriteStackTrace(stack_trace);
        }
    }

    auto scoped_timer::elapsed() const noexcept -> timespan
    {
        FO_NO_STACK_TRACE_ENTRY();

        return nanotime::now() - _started_at;
    }

    auto scoped_timer::lap(string_view name) noexcept -> timespan
    {
        FO_NO_STACK_TRACE_ENTRY();

        nanotime now = nanotime::now();
        timespan lap_elapsed = now - _last_lap_at;
        timespan total_elapsed = now - _started_at;
        _last_lap_at = now;
        WriteLog("[{}] timing {} / {}: lap {}, total {}", diagnostic_log_prefix, _name, name, lap_elapsed, total_elapsed);
        return lap_elapsed;
    }

    void scoped_timer::cancel() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        _cancelled = true;
    }

    one_shot_trigger::one_shot_trigger(string_view environment_name) :
        _environment_name {environment_name}
    {
        FO_NO_STACK_TRACE_ENTRY();
    }

    auto one_shot_trigger::is_set() const noexcept -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        const char* value = std::getenv(_environment_name.c_str());
        return value != nullptr && value[0] != '\0';
    }

    auto one_shot_trigger::consume() -> optional<string>
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (_consumed.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        const char* value = std::getenv(_environment_name.c_str());

        if (value == nullptr || value[0] == '\0') {
            return std::nullopt;
        }

        bool expected = false;

        if (!_consumed.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return std::nullopt;
        }

        return string {value};
    }

    void one_shot_trigger::reset() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        _consumed.store(false, std::memory_order_release);
    }

    hit_counter::hit_counter(string_view name, uint64_t report_first, uint64_t report_every) :
        _name {name},
        _report_first {report_first},
        _report_every {report_every}
    {
        FO_NO_STACK_TRACE_ENTRY();
    }

    auto hit_counter::hit() noexcept -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        uint64_t hit_count = _count.fetch_add(1, std::memory_order_relaxed) + 1;
        bool should_report = hit_count <= _report_first || (_report_every != 0 && hit_count % _report_every == 0);

        if (should_report) {
            WriteLog("[{}] hit {}: {}", diagnostic_log_prefix, _name, hit_count);
        }

        return should_report;
    }

    auto hit_counter::count() const noexcept -> uint64_t
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _count.load(std::memory_order_relaxed);
    }

    void hit_counter::report() const noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        WriteLog("[{}] hit {}: {} total", diagnostic_log_prefix, _name, count());
    }

    rate_counter::rate_counter(string_view name, timespan report_interval) :
        _name {name},
        _report_interval {report_interval > timespan::zero ? report_interval : timespan {1}},
        _started_at {nanotime::now()}
    {
        FO_NO_STACK_TRACE_ENTRY();
    }

    auto rate_counter::make_snapshot(uint64_t event_count, timespan elapsed) noexcept -> rate_snapshot
    {
        FO_NO_STACK_TRACE_ENTRY();

        float64_t elapsed_seconds = numeric_cast<float64_t>(elapsed.nanoseconds()) / 1'000'000'000.0;
        return rate_snapshot {
            event_count,
            elapsed,
            elapsed_seconds > 0.0 ? numeric_cast<float64_t>(event_count) / elapsed_seconds : 0.0,
        };
    }

    auto rate_counter::hit(uint64_t amount) -> optional<rate_snapshot>
    {
        FO_NO_STACK_TRACE_ENTRY();

        rate_snapshot report;

        {
            scoped_lock lock {_locker};
            uint64_t updated_event_count = checked_add<uint64_t>(_event_count, amount);

            nanotime now = nanotime::now();
            timespan elapsed = now - _started_at;

            if (elapsed < _report_interval) {
                _event_count = updated_event_count;
                return std::nullopt;
            }

            report = make_snapshot(updated_event_count, elapsed);
            _event_count = 0;
            _started_at = now;
        }

        WriteLog("[{}] rate {}: {:.3f}/s ({} event(s) over {})", diagnostic_log_prefix, _name, report.events_per_second, report.event_count, report.elapsed);
        return report;
    }

    auto rate_counter::snapshot() const -> rate_snapshot
    {
        FO_NO_STACK_TRACE_ENTRY();

        scoped_lock lock {_locker};
        return make_snapshot(_event_count, nanotime::now() - _started_at);
    }

    void rate_counter::clear()
    {
        FO_NO_STACK_TRACE_ENTRY();

        scoped_lock lock {_locker};
        _event_count = 0;
        _started_at = nanotime::now();
    }

    duration_statistics::duration_statistics(string_view name) :
        _name {name}
    {
        FO_NO_STACK_TRACE_ENTRY();
    }

    void duration_statistics::add(timespan sample)
    {
        FO_NO_STACK_TRACE_ENTRY();

        timespan normalized_sample = sample >= timespan::zero ? sample : timespan::zero;
        scoped_lock lock {_locker};
        uint64_t updated_sample_count = checked_add<uint64_t>(_sample_count, uint64_t {1});
        timespan updated_total {checked_add<int64_t>(_total.nanoseconds(), normalized_sample.nanoseconds())};
        _sample_count = updated_sample_count;
        _total = updated_total;
        _latest = normalized_sample;

        if (_sample_count == 1) {
            _minimum = normalized_sample;
            _maximum = normalized_sample;
        }
        else {
            _minimum = std::min(_minimum, normalized_sample);
            _maximum = std::max(_maximum, normalized_sample);
        }
    }

    auto duration_statistics::snapshot() const -> duration_snapshot
    {
        FO_NO_STACK_TRACE_ENTRY();

        scoped_lock lock {_locker};
        timespan average = _sample_count != 0 ? timespan {_total.nanoseconds() / numeric_cast<int64_t>(_sample_count)} : timespan::zero;
        return duration_snapshot {
            _sample_count,
            _total,
            _minimum,
            _maximum,
            _latest,
            average,
        };
    }

    void duration_statistics::dump() const
    {
        FO_NO_STACK_TRACE_ENTRY();

        duration_snapshot current = snapshot();
        WriteLog("[{}] durations {}: count: {}, total: {}, min: {}, max: {}, latest: {}, average: {}", diagnostic_log_prefix, _name, current.sample_count, current.total, current.minimum, current.maximum, current.latest, current.average);
    }

    void duration_statistics::clear()
    {
        FO_NO_STACK_TRACE_ENTRY();

        scoped_lock lock {_locker};
        _sample_count = 0;
        _total = timespan::zero;
        _minimum = timespan::zero;
        _maximum = timespan::zero;
        _latest = timespan::zero;
    }

    scoped_duration::scoped_duration(duration_statistics& statistics) noexcept :
        _statistics {&statistics},
        _started_at {nanotime::now()}
    {
        FO_NO_STACK_TRACE_ENTRY();
    }

    scoped_duration::~scoped_duration() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (_cancelled) {
            return;
        }

        safe_call([this] { _statistics->add(elapsed()); });
    }

    auto scoped_duration::elapsed() const noexcept -> timespan
    {
        FO_NO_STACK_TRACE_ENTRY();

        return nanotime::now() - _started_at;
    }

    void scoped_duration::cancel() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        _cancelled = true;
    }

    fault_injector::fault_injector(string_view name, uint64_t inject_at, uint64_t repeat_every) :
        _name {name},
        _inject_at {inject_at},
        _repeat_every {repeat_every}
    {
        FO_NO_STACK_TRACE_ENTRY();
    }

    auto fault_injector::hit() noexcept -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        uint64_t hit_count = _count.fetch_add(1, std::memory_order_relaxed) + 1;
        bool inject = _inject_at != 0 && (hit_count == _inject_at || (hit_count > _inject_at && _repeat_every != 0 && (hit_count - _inject_at) % _repeat_every == 0));

        if (inject) {
            WriteLog("[{}] fault {} injected at hit {}", diagnostic_log_prefix, _name, hit_count);
        }

        return inject;
    }

    auto fault_injector::count() const noexcept -> uint64_t
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _count.load(std::memory_order_relaxed);
    }

    void fault_injector::reset() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        _count.store(0, std::memory_order_relaxed);
    }

    void inject_delay(timespan delay) noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (delay > timespan::zero) {
            std::this_thread::sleep_for(delay.value());
        }
    }

    auto corrupt_bytes(span<uint8_t> data, size_t offset, size_t count, uint8_t xor_mask) noexcept -> size_t
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (offset >= data.size() || count == 0) {
            return 0;
        }

        size_t changed_count = std::min(count, data.size() - offset);

        for (size_t index = 0; index < changed_count; index++) {
            data[offset + index] ^= xor_mask;
        }

        return changed_count;
    }

    udp_sender::udp_sender(string_view host, uint16_t port, string_view bind_host) :
        _host {host},
        _port {port}
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (!net_sockets::startup() || !_socket.bind(bind_host, 0, false)) {
            WriteLog("[{}] udp sender {}:{} initialization failed: {}", diagnostic_log_prefix, _host, _port, net_sockets::last_error_text());
        }
    }

    auto udp_sender::is_ready() const -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        scoped_lock lock {_locker};
        return _socket.is_valid();
    }

    auto udp_sender::send_bytes(const_span<uint8_t> data) -> int32_t
    {
        FO_NO_STACK_TRACE_ENTRY();

        scoped_lock lock {_locker};
        int32_t sent_size = _socket.send_to(_host, _port, data);

        if (sent_size != numeric_cast<int32_t>(data.size())) {
            WriteLog("[{}] udp sender {}:{} sent {}/{} byte(s): {}", diagnostic_log_prefix, _host, _port, sent_size, data.size(), net_sockets::last_error_text());
        }

        return sent_size;
    }

    auto udp_sender::send_message(string_view message) -> int32_t
    {
        FO_NO_STACK_TRACE_ENTRY();

        return send_bytes(make_const_span(message.data(), message.size()));
    }

    pcap_writer::pcap_writer(string_view path, uint32_t link_type, uint32_t snap_length) :
        _path {path},
        _snap_length {std::max<uint32_t>(snap_length, 1)}
    {
        FO_NO_STACK_TRACE_ENTRY();

        string directory = strex(path).extract_dir().str();

        if (!directory.empty() && !fs_create_directories(directory)) {
            WriteLog("[{}] pcap {}: unable to create parent directory", diagnostic_log_prefix, _path);
            return;
        }

        _file.open(std::filesystem::path {fs_make_path(_path)}, std::ios::binary | std::ios::trunc);

        if (!_file) {
            WriteLog("[{}] pcap {}: unable to open", diagnostic_log_prefix, _path);
            return;
        }

        vector<uint8_t> header;
        header.reserve(24);
        append_u32_le(header, 0xA1B2C3D4);
        append_u16_le(header, 2);
        append_u16_le(header, 4);
        append_u32_le(header, 0);
        append_u32_le(header, 0);
        append_u32_le(header, _snap_length);
        append_u32_le(header, link_type);
        _file.write(make_ptr(header.data()).reinterpret_as<const char>().get(), numeric_cast<std::streamsize>(header.size()));
        _file.flush();

        if (!_file) {
            WriteLog("[{}] pcap {}: global header write failed", diagnostic_log_prefix, _path);
            _file.close();
        }
    }

    auto pcap_writer::is_ready() const -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        scoped_lock lock {_locker};
        return !!_file;
    }

    auto pcap_writer::write_packet(const_span<uint8_t> data, size_t original_size) -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        size_t captured_size = std::min<size_t>(data.size(), _snap_length);
        size_t packet_size = original_size != 0 ? std::max(original_size, data.size()) : data.size();
        std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
        std::chrono::system_clock::duration since_epoch = timestamp.time_since_epoch();
        std::chrono::seconds timestamp_seconds = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
        std::chrono::microseconds timestamp_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(since_epoch - timestamp_seconds);
        int64_t timestamp_seconds_count = timestamp_seconds.count();
        int64_t maximum_u32 = numeric_cast<int64_t>(std::numeric_limits<uint32_t>::max());
        uint32_t pcap_seconds = timestamp_seconds_count > 0 ? numeric_cast<uint32_t>(std::min(timestamp_seconds_count, maximum_u32)) : 0;
        uint32_t pcap_microseconds = numeric_cast<uint32_t>(std::max<int64_t>(timestamp_microseconds.count(), 0));
        uint32_t pcap_captured_size = numeric_cast<uint32_t>(std::min<size_t>(captured_size, std::numeric_limits<uint32_t>::max()));
        uint32_t pcap_packet_size = numeric_cast<uint32_t>(std::min<size_t>(packet_size, std::numeric_limits<uint32_t>::max()));

        vector<uint8_t> record;
        record.reserve(16 + captured_size);
        append_u32_le(record, pcap_seconds);
        append_u32_le(record, pcap_microseconds);
        append_u32_le(record, pcap_captured_size);
        append_u32_le(record, pcap_packet_size);
        const_span<uint8_t> captured_data = data.first(captured_size);
        record.insert(record.end(), captured_data.begin(), captured_data.end());

        scoped_lock lock {_locker};

        if (!_file) {
            return false;
        }

        _file.write(make_ptr(record.data()).reinterpret_as<const char>().get(), numeric_cast<std::streamsize>(record.size()));
        _file.flush();
        return !!_file;
    }

    auto pcap_writer::flush() -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        scoped_lock lock {_locker};

        if (!_file) {
            return false;
        }

        _file.flush();
        return !!_file;
    }

    auto pcap_writer::path() const noexcept -> string_view
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _path;
    }

    struct timeout_watchdog::state
    {
        static constexpr uint8_t active = 0;
        static constexpr uint8_t cancelled = 1;
        static constexpr uint8_t timed_out = 2;

        string name;
        nanotime deadline;
        function<void()> on_timeout;
        bool break_into_debugger {};
        std::atomic_uint8_t status {};
    };

    timeout_watchdog::timeout_watchdog(string_view name, timespan timeout, function<void()> on_timeout, bool break_into_debugger) :
        _state {SafeAlloc::MakeShared<state>()}
    {
        FO_NO_STACK_TRACE_ENTRY();

        timespan normalized_timeout = std::max(timeout, timespan::zero);
        _state->name = name;
        _state->deadline = nanotime::now() + normalized_timeout;
        _state->on_timeout = std::move(on_timeout);
        _state->break_into_debugger = break_into_debugger;
        shared_ptr<state> watchdog_state = _state;

        try {
            _worker = run_thread("DiagnosticWatchdog", [watchdog_state] { run(watchdog_state); });
        }
        catch (...) {
            _state->status.store(state::cancelled, std::memory_order_release);
            throw;
        }
    }

    timeout_watchdog::~timeout_watchdog() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        cancel();
        safe_call([this] { _worker.join(); });
    }

    auto timeout_watchdog::expired() const noexcept -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _state->status.load(std::memory_order_acquire) == state::timed_out;
    }

    void timeout_watchdog::cancel() noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        uint8_t expected = state::active;
        (void)_state->status.compare_exchange_strong(expected, state::cancelled, std::memory_order_acq_rel);
    }

    void timeout_watchdog::run(shared_ptr<state> watchdog_state) noexcept
    {
        FO_NO_STACK_TRACE_ENTRY();

        timespan poll_interval {std::chrono::milliseconds {10}};

        while (watchdog_state->status.load(std::memory_order_acquire) == state::active) {
            nanotime now = nanotime::now();

            if (now >= watchdog_state->deadline) {
                break;
            }

            timespan remaining = watchdog_state->deadline - now;
            std::this_thread::sleep_for(std::min(remaining, poll_interval).value());
        }

        uint8_t expected = state::active;

        if (!watchdog_state->status.compare_exchange_strong(expected, state::timed_out, std::memory_order_acq_rel)) {
            return;
        }

        WriteLog("[{}] watchdog {} expired", diagnostic_log_prefix, watchdog_state->name);

        if (watchdog_state->on_timeout) {
            safe_call(watchdog_state->on_timeout);
        }

        (void)debug_break_if(watchdog_state->break_into_debugger);
    }

    event_history::event_history(string_view name, size_t capacity) :
        _name {name},
        _capacity {std::max<size_t>(capacity, 1)},
        _started_at {nanotime::now()}
    {
        FO_NO_STACK_TRACE_ENTRY();

        _entries.reserve(_capacity);
    }

    void event_history::record_message(string_view channel, string message)
    {
        FO_NO_STACK_TRACE_ENTRY();

        scoped_lock lock {_locker};

        if (_entries.size() == _capacity) {
            _entries.erase(_entries.begin());
        }

        _entries.emplace_back(history_entry {
            ++_sequence,
            nanotime::now() - _started_at,
            string {channel},
            std::move(message),
        });
    }

    void event_history::record_stack(string_view channel, string_view message)
    {
        FO_NO_STACK_TRACE_ENTRY();

        StackTraceData stack_trace = GetStackTrace();
        record(channel, "{}\n{}", message, FormatStackTrace(stack_trace));
    }

    auto event_history::snapshot() const -> vector<history_entry>
    {
        FO_NO_STACK_TRACE_ENTRY();

        scoped_lock lock {_locker};
        return _entries;
    }

    void event_history::dump() const
    {
        FO_NO_STACK_TRACE_ENTRY();

        vector<history_entry> entries = snapshot();
        WriteLog("[{}] history {}: {} event(s)", diagnostic_log_prefix, _name, entries.size());

        for (const history_entry& entry : entries) {
            WriteLog("[{}] history {} #{} +{} [{}] {}", diagnostic_log_prefix, _name, entry.sequence, entry.since_start, entry.channel, entry.message);
        }
    }

    void event_history::clear()
    {
        FO_NO_STACK_TRACE_ENTRY();

        scoped_lock lock {_locker};
        _entries.clear();
        _sequence = 0;
        _started_at = nanotime::now();
    }

} // namespace diagnostics

FO_END_NAMESPACE
