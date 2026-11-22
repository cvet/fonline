// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ClientRuntimeApi.h"

FO_BEGIN_NAMESPACE

auto IsSupportedClientRuntimeAbi(uint32_t host_abi_version) noexcept -> bool
{
    return host_abi_version == FO_CLIENT_RUNTIME_HOST_ABI_VERSION;
}

auto IsValidClientRuntimeMetadata(const ClientRuntimeMetadata& metadata) noexcept -> bool
{
    return metadata.StructSize == sizeof(ClientRuntimeMetadata) && metadata.HostAbiVersion != 0 && metadata.RuntimeName != nullptr && metadata.BuildHash != nullptr && metadata.CompatibilityVersion != nullptr;
}

auto IsValidClientRuntimeResult(const ClientRuntimeResult& result) noexcept -> bool
{
    if (result.StructSize != sizeof(ClientRuntimeResult)) {
        return false;
    }

    switch (result.ResultKind) {
    case ClientRuntimeResultKind::Shutdown:
    case ClientRuntimeResultKind::ReloadRequested:
    case ClientRuntimeResultKind::FatalError:
        break;
    default:
        return false;
    }

    if (result.ResultKind == ClientRuntimeResultKind::ReloadRequested && (result.RequestedRuntimePath == nullptr || result.RequestedRuntimePath[0] == '\0')) {
        return false;
    }

    return true;
}

auto IsValidClientRuntimeExports(const ClientRuntimeExports& exports) noexcept -> bool
{
    return exports.StructSize == sizeof(ClientRuntimeExports) && IsValidClientRuntimeMetadata(exports.Metadata) && exports.Run != nullptr;
}

auto IsClientRuntimeCompatibilityMatch(const ClientRuntimeMetadata& metadata, string_view compatibility_version) noexcept -> bool
{
    return metadata.CompatibilityVersion != nullptr && string_view(metadata.CompatibilityVersion) == compatibility_version;
}

auto IsClientRuntimeCompatibilityMatch(const ClientRuntimeResult& result, string_view compatibility_version) noexcept -> bool
{
    return result.RequestedCompatibilityVersion != nullptr && string_view(result.RequestedCompatibilityVersion) == compatibility_version;
}

auto RunClientRuntimeHostPass(const optional<ClientRuntimeHostResult>& runtime_result, ClientRuntimeHostPromoteFunc promote_runtime) -> optional<bool>
{
    if (!runtime_result.has_value()) {
        return std::nullopt;
    }

    if (runtime_result->Result.ResultKind == ClientRuntimeResultKind::ReloadRequested) {
        FO_VERIFY_AND_THROW(promote_runtime != nullptr, "Client runtime reload promotion function is not set");
        return promote_runtime(runtime_result->RequestedRuntimePath);
    }

    return runtime_result->Result.Success;
}

void CaptureClientRuntimeResultStrings(ClientRuntimeResult& result, string& runtime_path, string& compatibility_version)
{
    auto capture_text {[](nptr<const char> text, string& storage) -> nptr<const char> {
        if (!text) {
            storage.clear();
            return nullptr;
        }

        // A result captured twice already points into this storage, and assigning a string its own buffer is
        // not something every string implementation promises to survive
        if (text.get() != storage.c_str()) {
            storage = text.get();
        }

        return storage.c_str();
    }};

    result.RequestedRuntimePath = capture_text(result.RequestedRuntimePath, runtime_path).get();
    result.RequestedCompatibilityVersion = capture_text(result.RequestedCompatibilityVersion, compatibility_version).get();
}

auto ClientRuntimeResultKindToString(ClientRuntimeResultKind kind) noexcept -> string_view
{
    switch (kind) {
    case ClientRuntimeResultKind::Shutdown:
        return "Shutdown";
    case ClientRuntimeResultKind::ReloadRequested:
        return "ReloadRequested";
    case ClientRuntimeResultKind::FatalError:
        return "FatalError";
    default:
        return "Unknown";
    }
}

FO_END_NAMESPACE
