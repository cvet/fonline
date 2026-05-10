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

#include "ClientRuntimeApi.h"

FO_BEGIN_NAMESPACE

auto IsSupportedClientRuntimeAbi(uint32_t host_abi_version) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return host_abi_version == FO_CLIENT_RUNTIME_HOST_ABI_VERSION;
}

auto IsValidClientRuntimeMetadata(const ClientRuntimeMetadata& metadata) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return metadata.StructSize == sizeof(ClientRuntimeMetadata) && metadata.HostAbiVersion != 0 && metadata.RuntimeName != nullptr && metadata.BuildHash != nullptr && metadata.CompatibilityVersion != nullptr;
}

auto IsValidClientRuntimeResult(const ClientRuntimeResult& result) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (result.StructSize != sizeof(ClientRuntimeResult)) {
        return false;
    }

    // Reload requires the runtime to nominate a target path; without it the host has
    // nothing to load on the second pass.
    if (result.ResultKind == ClientRuntimeResultKind::ReloadRequested && (result.RequestedRuntimePath == nullptr || result.RequestedRuntimePath[0] == '\0')) {
        return false;
    }

    return true;
}

auto IsValidClientRuntimeExports(const ClientRuntimeExports& exports) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return exports.StructSize == sizeof(ClientRuntimeExports) && IsValidClientRuntimeMetadata(exports.Metadata) && exports.Run != nullptr;
}

auto IsClientRuntimeCompatibilityMatch(const ClientRuntimeMetadata& metadata, string_view compatibility_version) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return metadata.CompatibilityVersion != nullptr && string_view(metadata.CompatibilityVersion) == compatibility_version;
}

auto IsClientRuntimeCompatibilityMatch(const ClientRuntimeResult& result, string_view compatibility_version) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return result.RequestedCompatibilityVersion != nullptr && string_view(result.RequestedCompatibilityVersion) == compatibility_version;
}

FO_END_NAMESPACE
