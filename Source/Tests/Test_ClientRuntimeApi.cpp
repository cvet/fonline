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
#include "Settings.h"
#include "Updater.h"

FO_BEGIN_NAMESPACE

static auto PromoteExpectedRuntime(string_view runtime_path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return runtime_path == "runtime";
}

static auto FailUnexpectedRuntimePromotion(string_view) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FAIL("Runtime promotion must not be called");
    return false;
}

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
        const ClientRuntimeRunFunc stub_run = +[](int32_t argc, char** argv, ClientRuntimeResult* raw_runtime_result) noexcept {
            const CommandLineArgs args {argc, argv};
            if (raw_runtime_result != nullptr) {
                auto result = make_ptr(raw_runtime_result);
                result->StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
                result->ResultKind = ClientRuntimeResultKind::Shutdown;
                result->Success = args.empty();
                result->RequestedRuntimePath = nullptr;
                result->RequestedCompatibilityVersion = nullptr;
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

        exports.Run(0, nullptr, &result);

        CHECK(result.Success);
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

        // Any non-empty path is accepted; the host promotes it and exits for a clean next launch.
        result.RequestedRuntimePath = "runtime";
        CHECK(IsValidClientRuntimeResult(result));

        // Other result kinds do not require a path.
        result.ResultKind = ClientRuntimeResultKind::Shutdown;
        result.RequestedRuntimePath = nullptr;
        CHECK(IsValidClientRuntimeResult(result));
    }

    SECTION("UnknownResultKindIsRejected")
    {
        ClientRuntimeResult result {};
        result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
        result.ResultKind = static_cast<ClientRuntimeResultKind>(42);

        CHECK_FALSE(IsValidClientRuntimeResult(result));
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

    SECTION("ReloadRequestPromotesAndExitsForRestart")
    {
        optional<ClientRuntimeHostResult> runtime_result {std::in_place};
        runtime_result->Result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
        runtime_result->Result.ResultKind = ClientRuntimeResultKind::ReloadRequested;
        runtime_result->Result.Success = true;
        runtime_result->RequestedRuntimePath = "runtime";

        const optional<bool> host_result = RunClientRuntimeHostPass(runtime_result, PromoteExpectedRuntime);

        REQUIRE(host_result.has_value());
        CHECK(host_result.value());
    }

    SECTION("NormalRuntimeShutdownDoesNotPromote")
    {
        optional<ClientRuntimeHostResult> runtime_result {std::in_place};
        runtime_result->Result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
        runtime_result->Result.ResultKind = ClientRuntimeResultKind::Shutdown;
        runtime_result->Result.Success = true;

        const optional<bool> host_result = RunClientRuntimeHostPass(runtime_result, FailUnexpectedRuntimePromotion);

        REQUIRE(host_result.has_value());
        CHECK(host_result.value());
    }

    SECTION("StagingPathDerivesFromLivePath")
    {
        // Both helpers depend on Platform::GetExePath, so the test only validates the
        // structural contract: staging is the live path with a non-empty suffix appended.
        const auto live = GetClientRuntimeLivePath();
        const auto staging = MakeClientRuntimeStagingPath(live);

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

    SECTION("InstalledRuntimeBootstrapRoundTrip")
    {
        const std::filesystem::path base = std::filesystem::temp_directory_path() / std::format("lf_client_runtime_bootstrap_{}", std::chrono::steady_clock::now().time_since_epoch().count());
        const string temp_dir = fs_path_to_string(base);
        const string runtime_file_name = strex("Runtime{}", GetClientRuntimeLibraryExtension()).str();
        const string runtime_path = fs_resolve_path(strex(temp_dir).combine_path(runtime_file_name).str());
        const string bootstrap_path = fs_resolve_path(strex(temp_dir).combine_path("selector/runtime.path").str());
        ignore_unused(fs_remove_dir_tree(temp_dir));

        REQUIRE(WriteClientRuntimeBootstrapTarget(bootstrap_path, runtime_path, runtime_file_name));

        const optional<string> restored_path = ReadClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name);
        REQUIRE(restored_path.has_value());
        CHECK(restored_path.value() == runtime_path);

        const string fallback_path = fs_resolve_path(strex(temp_dir).combine_path("BaseRuntime").str());
        CHECK(ResolveClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name, fallback_path) == fallback_path);

        REQUIRE(fs_write_file(MakeClientRuntimeStagingPath(runtime_path), "staged"));
        CHECK(ResolveClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name, fallback_path) == runtime_path);

        REQUIRE(fs_write_file(runtime_path, "live"));
        REQUIRE(fs_remove_file(MakeClientRuntimeStagingPath(runtime_path)));
        CHECK(ResolveClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name, fallback_path) == runtime_path);
        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("InstalledRuntimeBootstrapRejectsUnsafeTargets")
    {
        const std::filesystem::path base = std::filesystem::temp_directory_path() / std::format("lf_client_runtime_bootstrap_invalid_{}", std::chrono::steady_clock::now().time_since_epoch().count());
        const string temp_dir = fs_path_to_string(base);
        const string runtime_file_name = strex("Runtime{}", GetClientRuntimeLibraryExtension()).str();
        const string bootstrap_path = fs_resolve_path(strex(temp_dir).combine_path("runtime.path").str());
        ignore_unused(fs_remove_dir_tree(temp_dir));

        CHECK_FALSE(WriteClientRuntimeBootstrapTarget(bootstrap_path, "relative/Runtime.dll", runtime_file_name));

        const string wrong_runtime_path = fs_resolve_path(strex(temp_dir).combine_path("OtherRuntime.dll").str());
        CHECK_FALSE(WriteClientRuntimeBootstrapTarget(bootstrap_path, wrong_runtime_path, runtime_file_name));

        const string valid_runtime_path = fs_resolve_path(strex(temp_dir).combine_path(runtime_file_name).str());
        REQUIRE(fs_write_file(bootstrap_path, strex("{}\n{}", valid_runtime_path, valid_runtime_path).str()));
        CHECK_FALSE(ReadClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name).has_value());

        REQUIRE(fs_write_file(bootstrap_path, string(4097, 'x')));
        CHECK_FALSE(ReadClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name).has_value());
        CHECK(fs_remove_dir_tree(temp_dir));
    }
}

FO_END_NAMESPACE
