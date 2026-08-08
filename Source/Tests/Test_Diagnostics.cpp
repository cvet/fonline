//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
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

#include "catch_amalgamated.hpp"

#include "Essentials.h"

#include <atomic>
#include <cstdlib>

FO_BEGIN_NAMESPACE

TEST_CASE("Diagnostics")
{
    SECTION("CheckpointsStackAndTimersUseEngineLog")
    {
        vector<string> captured;

        SetLogCallback("diagnostics-test", [&](LogType, string_view message, nptr<const CatchedStackTraceData>) { captured.emplace_back(message); });

        diagnostics::here("location");
        diagnostics::checkpoint("state", "value: {}", 42);
        diagnostics::stack_checkpoint("stack");
        diagnostics::thread_checkpoint("worker");
        diagnostics::process_checkpoint("memory");
        array<uint8_t, 5> packet = {'A', 0, 0x7F, 'z', 0xFF};
        diagnostics::hex_checkpoint("packet", packet, 4, 4);

        {
            diagnostics::scoped_timer timer {"scope"};
            CHECK(timer.elapsed() >= timespan::zero);
            CHECK(timer.lap("first") >= timespan::zero);
        }

        SetLogCallback("diagnostics-test", {});

        string combined;

        for (const string& message : captured) {
            combined += message;
        }

        CHECK(combined.find("[TEMP_DIAGNOSTIC] here location | file: ") != string::npos);
        CHECK(combined.find("[TEMP_DIAGNOSTIC] checkpoint state | value: 42") != string::npos);
        CHECK(combined.find("[TEMP_DIAGNOSTIC] stack stack\n") != string::npos);
        CHECK(combined.find("[TEMP_DIAGNOSTIC] thread worker | thread: ") != string::npos);
        CHECK(combined.find("[TEMP_DIAGNOSTIC] process memory | pid: ") != string::npos);
        CHECK(combined.find("[TEMP_DIAGNOSTIC] hex packet | bytes: 5\n") != string::npos);
        CHECK(combined.find("41 00 7f 7a") != string::npos);
        CHECK(combined.find("1 byte(s) omitted") != string::npos);
        CHECK(combined.find("[TEMP_DIAGNOSTIC] timing scope | lap: first, lap_elapsed: ") != string::npos);
        CHECK(combined.find("[TEMP_DIAGNOSTIC] timing scope | elapsed: ") != string::npos);
        CHECK_FALSE(diagnostics::stack_trace().empty());

        diagnostics::process_snapshot process = diagnostics::capture_process();
        CHECK_FALSE(process.process_id.empty());
        CHECK(diagnostics::debugger_attached() == IsRunInDebugger());
        CHECK_FALSE(diagnostics::debug_break_if(false));
    }

    SECTION("ProbeLinesQuoteValuesCarryingDelimiters")
    {
        vector<string> captured;

        SetLogCallback("diagnostics-format-test", [&](LogType, string_view message, nptr<const CatchedStackTraceData>) { captured.emplace_back(message); });

        diagnostics::event_history history {"events", 4};
        history.record_message("net", "path: C:/tmp/a, b");
        history.dump();

        SetLogCallback("diagnostics-format-test", {});

        string combined;

        for (const string& message : captured) {
            combined += message;
        }

        // Header and payload are split on the first " | "; a value holding ':' or ',' must be quoted
        // so a consumer cannot mistake it for another pair.
        CHECK(combined.find("[TEMP_DIAGNOSTIC] history events | count: 1") != string::npos);
        CHECK(combined.find("channel: net") != string::npos);
        CHECK(combined.find("message: \"path: C:/tmp/a, b\"") != string::npos);
    }

    SECTION("OneShotTriggerConsumesPayloadOnce")
    {
        constexpr const char* environment_name = "FO_UNIT_TEST_DIAGNOSTICS_TRIGGER";

#if FO_WINDOWS
        REQUIRE(_putenv_s(environment_name, "payload") == 0);
#else
        REQUIRE(setenv(environment_name, "payload", 1) == 0);
#endif

        diagnostics::one_shot_trigger trigger {environment_name};
        CHECK(trigger.is_set());

        optional<string> first = trigger.consume();
        REQUIRE(first.has_value());
        CHECK(*first == "payload");
        CHECK_FALSE(trigger.consume().has_value());

        trigger.reset();
        CHECK(trigger.consume() == optional<string> {"payload"});

#if FO_WINDOWS
        REQUIRE(_putenv_s(environment_name, "") == 0);
#else
        REQUIRE(unsetenv(environment_name) == 0);
#endif
    }

    SECTION("CountersAndChangeDetectionAreLocal")
    {
        diagnostics::hit_counter counter {"hits", 2, 4};
        CHECK(counter.hit());
        CHECK(counter.hit());
        CHECK_FALSE(counter.hit());
        CHECK(counter.hit());
        CHECK(counter.count() == 4);

        diagnostics::change_detector<int32_t> change;
        CHECK(change.changed(10));
        CHECK_FALSE(change.changed(10));
        CHECK(change.changed(11));
        REQUIRE(change.value().has_value());
        CHECK(*change.value() == 11);

        change.reset();
        CHECK_FALSE(change.value().has_value());
    }

    SECTION("RatesAndDurationsProvideSnapshots")
    {
        diagnostics::rate_counter rate {"events", std::chrono::nanoseconds {1}};
        optional<diagnostics::rate_snapshot> report = rate.hit(4);
        REQUIRE(report.has_value());
        CHECK(report->event_count == 4);
        CHECK(report->elapsed > timespan::zero);
        CHECK(report->events_per_second > 0.0);
        CHECK(rate.snapshot().event_count == 0);

        rate.clear();
        CHECK(rate.snapshot().event_count == 0);

        diagnostics::duration_statistics durations {"work"};
        durations.add(std::chrono::milliseconds {1});
        durations.add(std::chrono::milliseconds {3});
        durations.add(std::chrono::milliseconds {-1});

        diagnostics::duration_snapshot duration = durations.snapshot();
        CHECK(duration.sample_count == 3);
        CHECK(duration.total == timespan {std::chrono::milliseconds {4}});
        CHECK(duration.minimum == timespan::zero);
        CHECK(duration.maximum == timespan {std::chrono::milliseconds {3}});
        CHECK(duration.latest == timespan::zero);
        CHECK(duration.average.nanoseconds() == duration.total.nanoseconds() / 3);

        durations.dump();
        durations.clear();
        CHECK(durations.snapshot().sample_count == 0);

        diagnostics::duration_statistics scoped_durations {"scoped-work"};

        {
            diagnostics::scoped_duration scoped_sample {scoped_durations};
            diagnostics::inject_delay(std::chrono::milliseconds {1});
            CHECK(scoped_sample.elapsed() > timespan::zero);
        }

        diagnostics::duration_snapshot scoped_duration = scoped_durations.snapshot();
        CHECK(scoped_duration.sample_count == 1);
        CHECK(scoped_duration.total >= timespan {std::chrono::milliseconds {1}});

        {
            diagnostics::scoped_duration cancelled_sample {scoped_durations};
            cancelled_sample.cancel();
        }

        CHECK(scoped_durations.snapshot().sample_count == 1);
    }

    SECTION("HexDumpSupportsFullAndBoundedOutput")
    {
        array<uint8_t, 5> packet = {'A', 0, 0x7F, 'z', 0xFF};
        string full_dump = diagnostics::hex_dump(packet, 0, 4);
        CHECK(full_dump.find("00000000  41 00 7f 7a") != string::npos);
        CHECK(full_dump.find("|A..z|") != string::npos);
        CHECK(full_dump.find("00000004  ff") != string::npos);
        CHECK(full_dump.find("omitted") == string::npos);

        string bounded_dump = diagnostics::hex_dump(packet, 2, 4);
        CHECK(bounded_dump.find("41 00") != string::npos);
        CHECK(bounded_dump.find("3 byte(s) omitted") != string::npos);
    }

    SECTION("MemoryDeltasAndFaultInjectionAreExplicit")
    {
        diagnostics::process_snapshot before {"1", 100, 300};
        diagnostics::process_snapshot after {"1", 150, 250};
        diagnostics::memory_delta delta = diagnostics::capture_memory_delta(before, after);
        CHECK(delta.resident_memory_bytes == 50);
        CHECK(delta.private_memory_bytes == -50);

        diagnostics::scoped_memory_delta memory_scope {"memory-scope", std::numeric_limits<size_t>::max()};
        (void)memory_scope.delta();
        memory_scope.cancel();

        diagnostics::fault_injector fault {"packet", 2, 2};
        CHECK_FALSE(fault.hit());
        CHECK(fault.hit());
        CHECK_FALSE(fault.hit());
        CHECK(fault.hit());
        CHECK(fault.count() == 4);
        fault.reset();
        CHECK(fault.count() == 0);

        array<uint8_t, 4> packet = {0x10, 0x20, 0x30, 0x40};
        CHECK(diagnostics::corrupt_bytes(packet, 1, 2, 0xFF) == 2);
        CHECK(packet == (array<uint8_t, 4> {0x10, 0xDF, 0xCF, 0x40}));
        CHECK(diagnostics::corrupt_bytes(packet, packet.size(), 1) == 0);
    }

    SECTION("FileAndPcapDumpsAreReadable")
    {
        std::filesystem::path base_path = std::filesystem::temp_directory_path() / std::format("fo_diagnostics_{}", std::chrono::steady_clock::now().time_since_epoch().count());
        string base = fs_path_to_string(base_path);
        string text_path = strex(base).combine_path("state.txt").str();
        string binary_path = strex(base).combine_path("packet.bin").str();
        string hex_path = strex(base).combine_path("packet.hex").str();
        string pcap_path = strex(base).combine_path("packet.pcap").str();
        array<uint8_t, 3> packet = {0x01, 0x02, 0x03};

        REQUIRE(diagnostics::write_text_dump(text_path, "state=ready"));
        REQUIRE(diagnostics::write_binary_dump(binary_path, packet));
        REQUIRE(diagnostics::write_hex_dump(hex_path, packet));
        CHECK(fs_read_file(text_path) == optional<string> {"state=ready"});
        REQUIRE(fs_read_file(binary_path).has_value());
        optional<string> hex_content = fs_read_file(hex_path);
        REQUIRE(hex_content.has_value());
        CHECK(hex_content->find("01 02 03") != string::npos);

        {
            diagnostics::pcap_writer writer {pcap_path};
            REQUIRE(writer.is_ready());
            CHECK(writer.path() == pcap_path);
            REQUIRE(writer.write_packet(packet));
            REQUIRE(writer.flush());
        }

        optional<string> pcap_content = fs_read_file(pcap_path);
        REQUIRE(pcap_content.has_value());
        const_span<uint8_t> pcap_bytes = make_const_span(*pcap_content);
        REQUIRE(pcap_bytes.size() == 24 + 16 + packet.size());
        CHECK(pcap_bytes[0] == 0xD4);
        CHECK(pcap_bytes[1] == 0xC3);
        CHECK(pcap_bytes[2] == 0xB2);
        CHECK(pcap_bytes[3] == 0xA1);
        CHECK(pcap_bytes[20] == 0x93);
        CHECK(std::equal(packet.begin(), packet.end(), pcap_bytes.begin() + 40));

        REQUIRE(fs_remove_dir_tree(base));
    }

    SECTION("TimeoutWatchdogExpiresOrCancels")
    {
        std::atomic_bool callback_called {};

        {
            diagnostics::timeout_watchdog watchdog {
                "short-operation",
                std::chrono::milliseconds {1},
                [&callback_called] { callback_called.store(true, std::memory_order_release); },
            };

            for (int32_t attempt = 0; attempt != 200 && (!watchdog.expired() || !callback_called.load(std::memory_order_acquire)); attempt++) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }

            CHECK(watchdog.expired());
            CHECK(callback_called.load(std::memory_order_acquire));
        }

        {
            diagnostics::timeout_watchdog watchdog {"cancelled-operation", std::chrono::seconds {1}};
            watchdog.cancel();
            CHECK_FALSE(watchdog.expired());
        }
    }

    SECTION("UdpSenderWritesToLoopback")
    {
        REQUIRE(net_sockets::startup());

        static std::atomic<uint16_t> next_port {44000};
        udp_socket receiver;
        uint16_t receiver_port = 0;

        for (int32_t attempt = 0; attempt != 128 && !receiver.is_valid(); attempt++) {
            receiver_port = next_port.fetch_add(1, std::memory_order_relaxed);
            receiver.bind("127.0.0.1", receiver_port);
        }

        REQUIRE(receiver.is_valid());

        diagnostics::udp_sender sender {"127.0.0.1", receiver_port, "127.0.0.1"};
        REQUIRE(sender.is_ready());

        string_view message = "diagnostic packet";
        REQUIRE(sender.send_message(message) == numeric_cast<int32_t>(message.size()));
        REQUIRE(receiver.can_read(std::chrono::milliseconds {200}));

        array<uint8_t, 64> buffer {};
        string source_host;
        uint16_t source_port = 0;
        int32_t received_size = receiver.receive_from(buffer, source_host, source_port);

        REQUIRE(received_size == numeric_cast<int32_t>(message.size()));
        string_view received_message {make_ptr(buffer.data()).reinterpret_as<const char>().get(), numeric_cast<size_t>(received_size)};
        CHECK(received_message == message);
        CHECK(source_host == "127.0.0.1");
        CHECK(source_port != 0);
    }

    SECTION("EventHistoryIsBoundedAndClearable")
    {
        diagnostics::event_history history {"events", 2};
        history.record("state", "value={}", 1);
        history.record("state", "value={}", 2);
        history.record("state", "value={}", 3);

        vector<diagnostics::history_entry> entries = history.snapshot();
        REQUIRE(entries.size() == 2);
        CHECK(entries[0].sequence == 2);
        CHECK(entries[0].channel == "state");
        CHECK(entries[0].message == "value=2");
        CHECK(entries[1].sequence == 3);
        CHECK(entries[1].message == "value=3");

        history.clear();
        CHECK(history.snapshot().empty());
    }
}

FO_END_NAMESPACE
