// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ClientSessionException);

// A client that hangs while shutting down cannot report anything: the send path is part of what is
// stopping. So the stage is written to disk as it is reached, and the next run reports what it finds
enum class ClientShutdownStage : uint8_t
{
    Running = 0,
    MainLoopExited = 1,
    ClientStopped = 2,
    ApplicationReset = 3,
    ShutdownHookDone = 4,
    RuntimeReturned = 5,
    // Appended, so a marker an older build left keeps its meaning. Reached after ShutdownHookDone and before
    // RuntimeReturned: the runtime tearing its global data down, with the set it is in written beside the stage
    GlobalDataTeardown = 6,
    // The host asked the process to exit. Kept rather than deleted, so the next run can tell an exit that
    // finished from one that hangs inside the process teardown by whether the process is still there
    ExitRequested = 7,
    Unknown = 255,
};

struct PreviousClientSession
{
    ClientShutdownStage Stage {};
    string StageName {};
    string BuildHash {};
    string StartedAt {};
    string TeardownSet {};
    int64_t Pid {};
    // The recorded process is still alive; Running alone does not distinguish normal play from a hang
    bool StillRunning {};
};

auto GetClientShutdownStageName(ClientShutdownStage stage) noexcept -> string_view;
// Built from the writable root the client actually uses, so the host records its own shutdown stages
// in the same place - it never loads settings and cannot resolve that root itself
auto MakeClientSessionMarkerPath(string_view writable_root) -> string;

// Reads and deletes the marker of the run before this one. A value means that run never finished its exit, or
// that its process is still alive; ExitRequested is silent only with a recorded process identity and no survivor
auto TakePreviousClientSession(string_view marker_path) noexcept -> optional<PreviousClientSession>;

void BeginClientSession(string_view marker_path) noexcept;

// A second client can replace the shared marker; only update the session owned by this process
void SetClientShutdownStage(string_view marker_path, ClientShutdownStage stage) noexcept;

// Tears the global data set down, writing the name of each part into the marker before it goes, so a teardown
// that never returns names the part it stopped in. Without a begun marker it only tears the set down
void DestroyGlobalDataRecordingTeardown(string_view marker_path) noexcept;

FO_END_NAMESPACE
