//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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
//

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

static constexpr uint32_t FO_CLIENT_RUNTIME_HOST_ABI_VERSION = 2;

enum class ClientRuntimeResultKind : uint32_t
{
    Shutdown = 0,
    ReloadRequested = 1,
    FatalError = 2,
};

struct ClientRuntimeMetadata
{
    uint32_t StructSize {};
    uint32_t HostAbiVersion {};
    const char* RuntimeName {};
    const char* BuildHash {};
    const char* CompatibilityVersion {};
};

struct ClientRuntimeResult
{
    uint32_t StructSize {};
    ClientRuntimeResultKind ResultKind {};
    bool Success {};
    const char* RequestedRuntimePath {};
    const char* RequestedCompatibilityVersion {};
};

using ClientRuntimeRunFunc = void (*)(int32_t argc, char** argv, ClientRuntimeResult* result) noexcept;

struct ClientRuntimeExports
{
    uint32_t StructSize {};
    ClientRuntimeMetadata Metadata {};
    ClientRuntimeRunFunc Run {};
};

using QueryClientRuntimeExportsFunc = bool (*)(uint32_t host_abi_version, ClientRuntimeExports* exports) noexcept;

extern auto IsSupportedClientRuntimeAbi(uint32_t host_abi_version) noexcept -> bool;
extern auto IsValidClientRuntimeMetadata(const ClientRuntimeMetadata& metadata) noexcept -> bool;
extern auto IsValidClientRuntimeResult(const ClientRuntimeResult& result) noexcept -> bool;
extern auto IsValidClientRuntimeExports(const ClientRuntimeExports& exports) noexcept -> bool;
extern auto IsClientRuntimeCompatibilityMatch(const ClientRuntimeMetadata& metadata, string_view compatibility_version) noexcept -> bool;
extern auto IsClientRuntimeCompatibilityMatch(const ClientRuntimeResult& result, string_view compatibility_version) noexcept -> bool;

FO_END_NAMESPACE
