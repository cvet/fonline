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

#include "catch_amalgamated.hpp"

#include "ClientRuntimeApi.h"
#include "Updater.h"

FO_BEGIN_NAMESPACE

TEST_CASE("ClientRuntimeApi")
{
    SECTION("CurrentHostAbiIsSupported")
    {
        CHECK(IsSupportedClientRuntimeAbi(FO_CLIENT_RUNTIME_HOST_ABI_VERSION));
        CHECK_FALSE(IsSupportedClientRuntimeAbi(FO_CLIENT_RUNTIME_HOST_ABI_VERSION + 1));
    }

    SECTION("InvalidExportsAreRejected")
    {
        ClientRuntimeExports exports {};

        CHECK_FALSE(IsValidClientRuntimeExports(exports));
    }

    SECTION("ValidExportsAreAccepted")
    {
        const ClientRuntimeRunFunc stub_run = +[](int32_t /*argc*/, char** /*argv*/, ClientRuntimeResult* runtime_result) noexcept {
            if (runtime_result != nullptr) {
                runtime_result->StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
                runtime_result->ResultKind = ClientRuntimeResultKind::Shutdown;
                runtime_result->Success = true;
                runtime_result->RequestedRuntimePath = nullptr;
                runtime_result->RequestedCompatibilityVersion = nullptr;
            }
        };

        ClientRuntimeResult result {};
        result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));

        ClientRuntimeExports exports {};
        exports.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeExports));
        exports.Metadata.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeMetadata));
        exports.Metadata.HostAbiVersion = FO_CLIENT_RUNTIME_HOST_ABI_VERSION;
        exports.Metadata.RuntimeName = "LF_ClientEngine";
        exports.Metadata.BuildHash = "build-hash";
        exports.Metadata.CompatibilityVersion = "compat";
        exports.Run = stub_run;

        CHECK(IsValidClientRuntimeExports(exports));
        CHECK(IsValidClientRuntimeResult(result));
    }

    SECTION("ReloadRequestedRequiresRuntimePath")
    {
        ClientRuntimeResult result {};
        result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
        result.ResultKind = ClientRuntimeResultKind::ReloadRequested;
        result.Success = true;

        // ReloadRequested without a path is a contract violation.
        result.RequestedRuntimePath = nullptr;
        CHECK_FALSE(IsValidClientRuntimeResult(result));

        // Empty string is treated the same way — host has nothing to reload from.
        result.RequestedRuntimePath = "";
        CHECK_FALSE(IsValidClientRuntimeResult(result));

        // Any non-empty path is accepted; the host then loads it on the second pass.
        result.RequestedRuntimePath = "runtime";
        CHECK(IsValidClientRuntimeResult(result));

        // Other result kinds do not require a path.
        result.ResultKind = ClientRuntimeResultKind::Shutdown;
        result.RequestedRuntimePath = nullptr;
        CHECK(IsValidClientRuntimeResult(result));
    }

    SECTION("CompatibilityMatchUsesMetadataString")
    {
        ClientRuntimeMetadata metadata {};
        metadata.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeMetadata));
        metadata.HostAbiVersion = FO_CLIENT_RUNTIME_HOST_ABI_VERSION;
        metadata.RuntimeName = "LF_ClientLib";
        metadata.BuildHash = "build-hash";
        metadata.CompatibilityVersion = "compat-a";

        CHECK(IsClientRuntimeCompatibilityMatch(metadata, "compat-a"));
        CHECK_FALSE(IsClientRuntimeCompatibilityMatch(metadata, "compat-b"));
    }

    SECTION("CompatibilityMatchUsesRequestedRuntimeResult")
    {
        ClientRuntimeResult result {};
        result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
        result.RequestedCompatibilityVersion = "compat-a";

        CHECK(IsClientRuntimeCompatibilityMatch(result, "compat-a"));
        CHECK_FALSE(IsClientRuntimeCompatibilityMatch(result, "compat-b"));
    }

    SECTION("StagingPathDerivesFromLivePath")
    {
        // Both helpers depend on Platform::GetExePath, so the test only validates the
        // structural contract: staging is the live path with a non-empty suffix appended.
        const auto live = GetClientRuntimeLivePath();
        const auto staging = GetClientRuntimeStagingPath();

        CHECK_FALSE(live.empty());
        CHECK_FALSE(staging.empty());
        CHECK(staging.size() > live.size());
        CHECK(string_view(staging).starts_with(live));
        CHECK_FALSE(string_view(staging).ends_with(live));
    }

    SECTION("CurrentRuntimeLibraryNameIsNonEmpty")
    {
        const auto name = GetCurrentClientRuntimeLibraryName();

        CHECK_FALSE(name.empty());
        // Library name must not contain a path separator — it is a basename, not a path.
        CHECK(name.find('/') == string::npos);
        CHECK(name.find('\\') == string::npos);
    }
}

FO_END_NAMESPACE
