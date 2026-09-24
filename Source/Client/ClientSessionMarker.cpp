//      __________        ___               ______            _
//     / ____/ __ \____   / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \ / / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / // / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_//_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                   /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ClientSessionMarker.h"
#include "Application.h"

FO_BEGIN_NAMESPACE

static constexpr string_view SessionMarkerExtension = ".session";
static constexpr size_t SessionMarkerMaxSize = 4096;

// What the teardown observer needs, built before the sweep: it runs inside it and may use nothing the sweep destroys
struct TeardownRecordState
{
    string MarkerPath {};
    string Prefix {};
    string Content {};
};

static auto ReplaceMarkerLines(string_view previous, string_view first_line, string_view dropped_key) -> string;
static auto ReadMarkerProcess(string_view content) noexcept -> platform::process_identity;
static auto IsCurrentClientSession(const platform::process_identity& process) noexcept -> bool;
static void RecordTeardownSet(void* context, const char* set_name) noexcept;
static void WriteTeardownRecord(TeardownRecordState& state, string_view set_name) noexcept;

auto GetClientShutdownStageName(ClientShutdownStage stage) noexcept -> string_view
{
    switch (stage) {
    case ClientShutdownStage::Running:
        return "Running";
    case ClientShutdownStage::MainLoopExited:
        return "MainLoopExited";
    case ClientShutdownStage::ClientStopped:
        return "ClientStopped";
    case ClientShutdownStage::ApplicationReset:
        return "ApplicationReset";
    case ClientShutdownStage::ShutdownHookDone:
        return "ShutdownHookDone";
    case ClientShutdownStage::RuntimeReturned:
        return "RuntimeReturned";
    case ClientShutdownStage::GlobalDataTeardown:
        return "GlobalDataTeardown";
    case ClientShutdownStage::ExitRequested:
        return "ExitRequested";
    default:
        return "Unknown";
    }
}

auto MakeClientSessionMarkerPath(string_view writable_root) -> string
{
    // Named after the executable, so two clients sharing one root keep their own
    string marker_name = strex("{}{}", strex(GetExeLogFileName()).erase_file_extension(), SessionMarkerExtension).str();
    return fs::resolve_path(fs::make_writable_path(writable_root, marker_name));
}

auto TakePreviousClientSession(string_view marker_path) noexcept -> optional<PreviousClientSession>
{
    FO_TRACE_ZONE(Core);

    optional<string> content;

    try {
        if (!fs::exists(marker_path)) {
            return std::nullopt;
        }

        content = fs::read_file_bounded(marker_path, SessionMarkerMaxSize);
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
    }

    // The marker is consumed whatever it holds: a file we cannot parse must not be reported on every
    // launch from here on
    (void)fs::remove_file(marker_path);

    if (!content.has_value()) {
        return std::nullopt;
    }

    PreviousClientSession session;
    session.Stage = ClientShutdownStage::Unknown;
    platform::process_identity previous_process = ReadMarkerProcess(content.value());

    for (string_view line : strex(content.value()).split('\n')) {
        auto separator = line.find(':');

        if (separator == string_view::npos) {
            continue;
        }

        string_view key = strex(line.substr(0, separator)).trim();
        string_view value = strex(line.substr(separator + 1)).trim();

        if (key == "Stage") {
            uint8_t raw_stage = 0;
            auto parsed = std::from_chars(value.data(), value.data() + value.size(), raw_stage);
            bool valid_stage = parsed.ec == std::errc {} && parsed.ptr == value.data() + value.size() && raw_stage <= static_cast<uint8_t>(ClientShutdownStage::ExitRequested);
            session.Stage = valid_stage ? static_cast<ClientShutdownStage>(raw_stage) : ClientShutdownStage::Unknown;
        }
        else if (key == "Build") {
            session.BuildHash = value;
        }
        else if (key == "Started") {
            session.StartedAt = value;
        }
        else if (key == "Teardown") {
            session.TeardownSet = value;
        }
    }

    session.Pid = previous_process.pid;

    // The id and the start time name one process, so a stranger that reused the id is not taken for the previous
    // run, and neither is this run itself when the marker was written by it
    bool is_current_process = IsCurrentClientSession(previous_process);
    session.StillRunning = !is_current_process && platform::is_process_running(previous_process);

    if (session.Stage == ClientShutdownStage::ExitRequested && previous_process.pid > 0 && previous_process.start_time != 0 && !session.StillRunning) {
        return std::nullopt;
    }

    session.StageName = GetClientShutdownStageName(session.Stage);
    return session;
}

void BeginClientSession(string_view marker_path) noexcept
{
    FO_TRACE_ZONE(Core);

    safe_call([&] { (void)fs::create_directories(strex(marker_path).extract_dir().str()); });

    safe_call([&] {
        time_desc_t time = nanotime::now().desc(true);
        platform::process_identity process = platform::get_current_process_identity();
        string started = strex("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", time.year, time.month, time.day, time.hour, time.minute, time.second).str();
        string content = strex("Stage: {}\nBuild: {}\nStarted: {}\nPid: {}\nProcessStart: {}\n", static_cast<int32_t>(ClientShutdownStage::Running), FO_BUILD_HASH, started, process.pid, process.start_time).str();
        (void)fs::write_file(marker_path, content);
    });
}

void SetClientShutdownStage(string_view marker_path, ClientShutdownStage stage) noexcept
{
    // Rewritten rather than appended, so the file always states the furthest stage reached and stays
    // one small write even when a shutdown crosses process boundaries
    safe_call([&] {
        auto previous = fs::read_file_bounded(marker_path, SessionMarkerMaxSize);

        if (!previous.has_value() || !IsCurrentClientSession(ReadMarkerProcess(previous.value()))) {
            return;
        }

        string stage_line = strex("Stage: {}", static_cast<int32_t>(stage)).str();
        (void)fs::write_file(marker_path, ReplaceMarkerLines(previous.value(), stage_line, "Teardown"));
    });
}

void DestroyGlobalDataRecordingTeardown(string_view marker_path) noexcept
{
    TeardownRecordState state;

    safe_call([&] {
        if (marker_path.empty()) {
            return;
        }

        auto previous = fs::read_file_bounded(marker_path, SessionMarkerMaxSize);

        if (!previous.has_value() || !IsCurrentClientSession(ReadMarkerProcess(previous.value()))) {
            return;
        }

        string stage_line = strex("Stage: {}", static_cast<int32_t>(ClientShutdownStage::GlobalDataTeardown)).str();
        state.Prefix = ReplaceMarkerLines(previous.value(), stage_line, "Teardown");
        state.Content.reserve(state.Prefix.size() + 256);
        state.MarkerPath = marker_path;
    });

    if (state.MarkerPath.empty()) {
        global_data::destroy();
        return;
    }

    global_data::destroy(&RecordTeardownSet, &state);

    // Past the last part: a run that stops from here on stopped on the way back to the host, not inside a set
    WriteTeardownRecord(state, {});
}

// The stage line leads, the other lines of the previous marker follow in their order, and a previous stage or a
// line of the dropped key is left out, so neither is ever stated twice
static auto ReplaceMarkerLines(string_view previous, string_view first_line, string_view dropped_key) -> string
{
    string content {first_line};

    for (string_view line : strex(previous).split('\n')) {
        bool is_dropped_key = line.starts_with(dropped_key) && line.substr(dropped_key.size()).starts_with(":");

        if (line.empty() || line.starts_with("Stage:") || is_dropped_key) {
            continue;
        }

        content += "\n";
        content += line;
    }

    content += "\n";
    return content;
}

static auto ReadMarkerProcess(string_view content) noexcept -> platform::process_identity
{
    platform::process_identity process;

    for (string_view line : strvex(content).split('\n')) {
        size_t separator = line.find(':');

        if (separator == string_view::npos) {
            continue;
        }

        string_view key = strvex(line.substr(0, separator)).trim();
        string_view value = strvex(line.substr(separator + 1)).trim();

        if (key == "Pid") {
            process.pid = strvex(value).to_int64();
        }
        else if (key == "ProcessStart") {
            uint64_t start_time = 0;
            auto parsed = std::from_chars(value.data(), value.data() + value.size(), start_time);
            process.start_time = parsed.ec == std::errc {} && parsed.ptr == value.data() + value.size() ? start_time : 0;
        }
    }

    return process;
}

static auto IsCurrentClientSession(const platform::process_identity& process) noexcept -> bool
{
    platform::process_identity current = platform::get_current_process_identity();
    return process.pid == current.pid && process.start_time == current.start_time;
}

static void RecordTeardownSet(void* context, const char* set_name) noexcept
{
    auto state = cast_from_void<TeardownRecordState*>(context);
    FO_STRONG_ASSERT(state, "Teardown observer called without its record state");
    WriteTeardownRecord(*state, set_name != nullptr ? string_view {set_name} : string_view {});
}

static void WriteTeardownRecord(TeardownRecordState& state, string_view set_name) noexcept
{
    // Every helper here must work outside the global-data lifetime: logging and string tables may already be gone
    safe_call([&] {
        auto previous = fs::read_file_bounded(state.MarkerPath, SessionMarkerMaxSize);

        if (!previous.has_value() || !IsCurrentClientSession(ReadMarkerProcess(previous.value()))) {
            return;
        }

        state.Content.assign(state.Prefix);

        if (!set_name.empty()) {
            state.Content.append("Teardown: ");
            state.Content.append(set_name);
            state.Content.append("\n");
        }

        (void)fs::write_file(state.MarkerPath, state.Content);
    });
}

FO_END_NAMESPACE
