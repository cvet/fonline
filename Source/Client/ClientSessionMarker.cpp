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

auto GetClientShutdownStageName(ClientShutdownStage stage) noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

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
    case ClientShutdownStage::RuntimeUnloaded:
        return "RuntimeUnloaded";
    default:
        return "Unknown";
    }
}

auto MakeClientSessionMarkerPath(string_view writable_root) -> string
{
    FO_STACK_TRACE_ENTRY();

    // Named after the executable, so two clients sharing one root keep their own
    string marker_name = strex("{}{}", strex(GetExeLogFileName()).erase_file_extension(), SessionMarkerExtension).str();
    return fs_make_writable_path(writable_root, marker_name);
}

auto TakePreviousClientSession(string_view marker_path) noexcept -> optional<PreviousClientSession>
{
    FO_STACK_TRACE_ENTRY();

    optional<string> content;

    try {
        if (!fs_exists(marker_path)) {
            return std::nullopt;
        }

        content = fs_read_file_bounded(marker_path, SessionMarkerMaxSize);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }

    // The marker is consumed whatever it holds: a file we cannot parse must not be reported on every
    // launch from here on
    (void)fs_remove_file(marker_path);

    if (!content.has_value()) {
        return std::nullopt;
    }

    PreviousClientSession session;
    session.Stage = ClientShutdownStage::Running;

    for (string_view line : strex(content.value()).split('\n')) {
        auto separator = line.find(':');

        if (separator == string_view::npos) {
            continue;
        }

        string_view key = strex(line.substr(0, separator)).trim();
        string_view value = strex(line.substr(separator + 1)).trim();

        if (key == "Stage") {
            int32_t raw_stage = strex(value).to_int32();
            session.Stage = static_cast<ClientShutdownStage>(numeric_cast<uint8_t>(std::clamp(raw_stage, 0, static_cast<int32_t>(ClientShutdownStage::RuntimeUnloaded))));
        }
        else if (key == "Build") {
            session.BuildHash = value;
        }
        else if (key == "Started") {
            session.StartedAt = value;
        }
    }

    session.StageName = GetClientShutdownStageName(session.Stage);
    return session;
}

void BeginClientSession(string_view marker_path) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    safe_call([&] { (void)fs_create_directories(strex(marker_path).extract_dir().str()); });

    time_desc_t time = nanotime::now().desc(true);
    string started = strex("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", time.year, time.month, time.day, time.hour, time.minute, time.second).str();
    string content = strex("Stage: {}\nBuild: {}\nStarted: {}\n", static_cast<int32_t>(ClientShutdownStage::Running), FO_BUILD_HASH, started).str();

    safe_call([&] { (void)fs_write_file(marker_path, content); });
}

void SetClientShutdownStage(string_view marker_path, ClientShutdownStage stage) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    // Rewritten rather than appended, so the file always states the furthest stage reached and stays
    // one small write even when a shutdown crosses process boundaries
    safe_call([&] {
        auto previous = fs_read_file_bounded(marker_path, SessionMarkerMaxSize);

        if (!previous.has_value()) {
            return;
        }

        string content = strex("Stage: {}", static_cast<int32_t>(stage)).str();

        for (string_view line : strex(previous.value()).split('\n')) {
            if (!line.starts_with("Stage:") && !line.empty()) {
                content += strex("\n{}", line).str();
            }
        }

        content += "\n";
        (void)fs_write_file(marker_path, content);
    });
}

void EndClientSession(string_view marker_path) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    safe_call([&] { (void)fs_remove_file(marker_path); });
}

FO_END_NAMESPACE
