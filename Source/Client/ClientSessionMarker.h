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
    RuntimeUnloaded = 6,
};

struct PreviousClientSession
{
    ClientShutdownStage Stage {};
    string StageName {};
    string BuildHash {};
    string StartedAt {};
};

extern auto GetClientShutdownStageName(ClientShutdownStage stage) noexcept -> string_view;
// Built from the writable root the client actually uses, so the host records its own shutdown stages
// in the same place - it never loads settings and cannot resolve that root itself
extern auto MakeClientSessionMarkerPath(string_view writable_root) -> string;

// Reads and deletes the marker of the run before this one. A value means that run never reached its
// clean exit, and the stage says how far it got
extern auto TakePreviousClientSession(string_view marker_path) noexcept -> optional<PreviousClientSession>;

extern void BeginClientSession(string_view marker_path) noexcept;
extern void SetClientShutdownStage(string_view marker_path, ClientShutdownStage stage) noexcept;
extern void EndClientSession(string_view marker_path) noexcept;

FO_END_NAMESPACE
