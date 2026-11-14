// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

static constexpr uint32_t FO_CLIENT_RUNTIME_HOST_ABI_VERSION {3};

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

struct ClientRuntimeHostResult
{
    ClientRuntimeResult Result {};
    string RequestedRuntimePath {};
    string RequestedCompatibilityVersion {};
    string LoadedBuildHash {};
};

using ClientRuntimeHostPromoteFunc = bool (*)(string_view);
using ClientRuntimeRunFunc = void (*)(int32_t argc, char** argv, ClientRuntimeResult* result) noexcept;

struct ClientRuntimeExports
{
    uint32_t StructSize {};
    ClientRuntimeMetadata Metadata {};
    ClientRuntimeRunFunc Run {};
};

using QueryClientRuntimeExportsFunc = bool (*)(uint32_t host_abi_version, ClientRuntimeExports* exports) noexcept;

auto IsSupportedClientRuntimeAbi(uint32_t host_abi_version) noexcept -> bool;
auto IsValidClientRuntimeMetadata(const ClientRuntimeMetadata& metadata) noexcept -> bool;
auto IsValidClientRuntimeResult(const ClientRuntimeResult& result) noexcept -> bool;
auto IsValidClientRuntimeExports(const ClientRuntimeExports& exports) noexcept -> bool;
auto IsClientRuntimeCompatibilityMatch(const ClientRuntimeMetadata& metadata, string_view compatibility_version) noexcept -> bool;
auto IsClientRuntimeCompatibilityMatch(const ClientRuntimeResult& result, string_view compatibility_version) noexcept -> bool;
auto RunClientRuntimeHostPass(const optional<ClientRuntimeHostResult>& runtime_result, ClientRuntimeHostPromoteFunc promote_runtime) -> optional<bool>;
void CaptureClientRuntimeResultStrings(ClientRuntimeResult& result, string& runtime_path, string& compatibility_version);
auto ClientRuntimeResultKindToString(ClientRuntimeResultKind kind) noexcept -> string_view;

FO_END_NAMESPACE
