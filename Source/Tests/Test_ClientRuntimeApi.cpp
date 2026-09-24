//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
#include "ClientSessionMarker.h"
#include "Settings.h"
#include "Updater.h"

FO_BEGIN_NAMESPACE

static auto PromoteExpectedRuntime(string_view runtime_path) -> bool
{
    return runtime_path == "runtime";
}

static auto FailUnexpectedRuntimePromotion(string_view) -> bool
{
    FAIL_CHECK("Runtime promotion must not be called");
    return false;
}

TEST_CASE("ClientRuntimeApi")
{
    SECTION("NativeModuleSelfUpdateSupportMatchesPlatformLifecycle")
    {
        CHECK(CanSelfUpdateNativeModules(UpdatePlatform::Windows));
        CHECK(CanSelfUpdateNativeModules(UpdatePlatform::Linux));
        CHECK(CanSelfUpdateNativeModules(UpdatePlatform::MacOS));
        CHECK_FALSE(CanSelfUpdateNativeModules(UpdatePlatform::Android));
        CHECK_FALSE(CanSelfUpdateNativeModules(UpdatePlatform::IOS));
        CHECK_FALSE(CanSelfUpdateNativeModules(UpdatePlatform::Web));
        CHECK_FALSE(CanSelfUpdateNativeModules(UpdatePlatform::Unknown));
    }

    SECTION("OnlyClientSideUpdaterFailuresAreReported")
    {
        // A server that is down, restarting or unreachable is the one terminal result that says nothing
        // about this client, so it must never reach the crash reporter - one event per player per restart
        CHECK_FALSE(IsUpdaterFailureReportable(UpdaterResult::ConnectionFailed));

        CHECK(IsUpdaterFailureReportable(UpdaterResult::Failed));
        CHECK(IsUpdaterFailureReportable(UpdaterResult::MetadataMismatch));
        CHECK(IsUpdaterFailureReportable(UpdaterResult::UpdaterOutdated));
        CHECK(IsUpdaterFailureReportable(UpdaterResult::PlatformUnsupported));
        CHECK(IsUpdaterFailureReportable(UpdaterResult::ServerMissingNativeUpdate));
    }

    SECTION("CurrentHostAbiIsSupported")
    {
        CHECK(IsSupportedClientRuntimeAbi(FO_CLIENT_RUNTIME_HOST_ABI_VERSION));
        CHECK_FALSE(IsSupportedClientRuntimeAbi(FO_CLIENT_RUNTIME_HOST_ABI_VERSION + 1));
    }

    SECTION("UnsafeInProcessReloadHostAbiIsRejected")
    {
        static constexpr uint32_t unsafe_host_abi = 2;

        CHECK_FALSE(IsSupportedClientRuntimeAbi(unsafe_host_abi));
    }

    SECTION("InvalidExportsAreRejected")
    {
        ClientRuntimeExports exports {};

        CHECK_FALSE(IsValidClientRuntimeExports(exports));
    }

    SECTION("ValidExportsAreAccepted")
    {
        ClientRuntimeRunFunc stub_run = +[](int32_t argc, char** argv, ClientRuntimeResult* raw_runtime_result) noexcept {
            CommandLineArgs args {argc, argv};
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

        // ReloadRequested without a path is a contract violation
        result.RequestedRuntimePath = nullptr;
        CHECK_FALSE(IsValidClientRuntimeResult(result));

        // Empty string is treated the same way — host has nothing to reload from
        result.RequestedRuntimePath = "";
        CHECK_FALSE(IsValidClientRuntimeResult(result));

        // Any non-empty path is accepted; the host promotes it and exits for a clean next launch
        result.RequestedRuntimePath = "runtime";
        CHECK(IsValidClientRuntimeResult(result));

        // Other result kinds do not require a path
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

    SECTION("CapturedResultStringsOutliveTheirSource")
    {
        // The runtime frees the data its result strings came from before the host reads them, so the capture
        // must own copies. Longer than the inline string buffer, so a freed source would really be heap
        ClientRuntimeResult result {};
        string runtime_path;
        string compatibility_version;
        string_view expected_path = "C:/Games/LastFrontier/Staged/LF_Client.runtime.staged.dll";

        {
            string source_path {expected_path};
            string source_compatibility = "compatibility-version-of-the-staged-runtime";
            result.RequestedRuntimePath = source_path.c_str();
            result.RequestedCompatibilityVersion = source_compatibility.c_str();
            CaptureClientRuntimeResultStrings(result, runtime_path, compatibility_version);
        }

        REQUIRE(result.RequestedRuntimePath == runtime_path.c_str());
        CHECK(string_view(result.RequestedRuntimePath) == expected_path);
        CHECK(string_view(result.RequestedCompatibilityVersion) == "compatibility-version-of-the-staged-runtime");

        // The host captures what the runtime already captured: the second pass must keep the text
        CaptureClientRuntimeResultStrings(result, runtime_path, compatibility_version);
        CHECK(string_view(result.RequestedRuntimePath) == expected_path);

        // An absent string stays absent and leaves nothing behind
        result.RequestedCompatibilityVersion = nullptr;
        CaptureClientRuntimeResultStrings(result, runtime_path, compatibility_version);
        CHECK(result.RequestedCompatibilityVersion == nullptr);
        CHECK(compatibility_version.empty());
    }

    SECTION("ReloadRequestPromotesAndExitsForRestart")
    {
        optional<ClientRuntimeHostResult> runtime_result {std::in_place};
        runtime_result->Result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
        runtime_result->Result.ResultKind = ClientRuntimeResultKind::ReloadRequested;
        runtime_result->Result.Success = true;
        runtime_result->RequestedRuntimePath = "runtime";

        optional<bool> host_result = RunClientRuntimeHostPass(runtime_result, PromoteExpectedRuntime);

        REQUIRE(host_result.has_value());
        CHECK(host_result.value());
    }

    SECTION("NormalRuntimeShutdownDoesNotPromote")
    {
        optional<ClientRuntimeHostResult> runtime_result {std::in_place};
        runtime_result->Result.StructSize = numeric_cast<uint32_t>(sizeof(ClientRuntimeResult));
        runtime_result->Result.ResultKind = ClientRuntimeResultKind::Shutdown;
        runtime_result->Result.Success = true;

        optional<bool> host_result = RunClientRuntimeHostPass(runtime_result, FailUnexpectedRuntimePromotion);

        REQUIRE(host_result.has_value());
        CHECK(host_result.value());
    }

    SECTION("StagingPathDerivesFromLivePath")
    {
        // Both helpers depend on Platform::get_exe_path, so the test only validates the
        // structural contract: staging is the live path with a non-empty suffix appended
        string live = GetClientRuntimeLivePath();
        string staging = MakeClientRuntimeStagingPath(live);

        CHECK_FALSE(live.empty());
        CHECK_FALSE(staging.empty());
        CHECK(staging.size() > live.size());
        CHECK(string_view(staging).starts_with(live));
        CHECK_FALSE(string_view(staging).ends_with(live));
    }

    SECTION("CurrentRuntimeLibraryNameIsNonEmpty")
    {
        string name = GetCurrentClientRuntimeLibraryName();

        CHECK_FALSE(name.empty());
        // Library name must not contain a path separator — it is a basename, not a path
        CHECK(name.find('/') == string::npos);
        CHECK(name.find('\\') == string::npos);
    }

    SECTION("ClientBinaryPathsFollowTheWritableRoot")
    {
        // One rule for both halves of the client: a writable root holds the modules it may replace and the
        // selector that names them, and without one they sit beside the exe with nothing to select
        string root = fs::resolve_path(fs::path_to_string(std::filesystem::temp_directory_path() / "lf_client_binary_root"));

        CHECK(GetClientBinaryDir(root) == root);
        CHECK(string_view(GetClientRuntimeLivePath()).starts_with(GetClientBinaryDir("")));
        CHECK_FALSE(MakeClientRuntimeBootstrapPath("").has_value());

        auto bootstrap_path = MakeClientRuntimeBootstrapPath(root);
        string selector_name = strex("{}{}.path", GetCurrentClientRuntimeLibraryName(), GetClientRuntimeLibraryExtension()).str();

        REQUIRE(bootstrap_path.has_value());
        CHECK(fs::is_absolute_path(bootstrap_path.value()));
        CHECK(string_view(bootstrap_path.value()).starts_with(root));
        CHECK(string_view(bootstrap_path.value()).ends_with(selector_name));
    }

    SECTION("InstalledRuntimeBootstrapRoundTrip")
    {
        std::filesystem::path base = std::filesystem::temp_directory_path() / std::format("lf_client_runtime_bootstrap_{}", std::chrono::steady_clock::now().time_since_epoch().count());
        string temp_dir = fs::path_to_string(base);
        string runtime_file_name = strex("Runtime{}", GetClientRuntimeLibraryExtension()).str();
        string runtime_path = fs::resolve_path(strex(temp_dir).combine_path(runtime_file_name).str());
        string bootstrap_path = fs::resolve_path(strex(temp_dir).combine_path("selector/runtime.path").str());
        ignore_unused(fs::remove_dir_tree(temp_dir));

        REQUIRE(WriteClientRuntimeBootstrapTarget(bootstrap_path, runtime_path, runtime_file_name));

        optional<string> restored_path = ReadClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name);
        REQUIRE(restored_path.has_value());
        CHECK(restored_path.value() == runtime_path);

        string fallback_path = fs::resolve_path(strex(temp_dir).combine_path("BaseRuntime").str());
        CHECK(ResolveClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name, fallback_path) == fallback_path);

        REQUIRE(fs::write_file(MakeClientRuntimeStagingPath(runtime_path), "staged"));
        CHECK(ResolveClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name, fallback_path) == runtime_path);

        REQUIRE(fs::write_file(runtime_path, "live"));
        REQUIRE(fs::remove_file(MakeClientRuntimeStagingPath(runtime_path)));
        CHECK(ResolveClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name, fallback_path) == runtime_path);
        CHECK(fs::remove_dir_tree(temp_dir));
    }

    SECTION("InstalledRuntimeBootstrapRejectsUnsafeTargets")
    {
        std::filesystem::path base = std::filesystem::temp_directory_path() / std::format("lf_client_runtime_bootstrap_invalid_{}", std::chrono::steady_clock::now().time_since_epoch().count());
        string temp_dir = fs::path_to_string(base);
        string runtime_file_name = strex("Runtime{}", GetClientRuntimeLibraryExtension()).str();
        string bootstrap_path = fs::resolve_path(strex(temp_dir).combine_path("runtime.path").str());
        ignore_unused(fs::remove_dir_tree(temp_dir));

        CHECK_FALSE(WriteClientRuntimeBootstrapTarget(bootstrap_path, "relative/Runtime.dll", runtime_file_name));

        string wrong_runtime_path = fs::resolve_path(strex(temp_dir).combine_path("OtherRuntime.dll").str());
        CHECK_FALSE(WriteClientRuntimeBootstrapTarget(bootstrap_path, wrong_runtime_path, runtime_file_name));

        string valid_runtime_path = fs::resolve_path(strex(temp_dir).combine_path(runtime_file_name).str());
        REQUIRE(fs::write_file(bootstrap_path, strex("{}\n{}", valid_runtime_path, valid_runtime_path).str()));
        CHECK_FALSE(ReadClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name).has_value());

        REQUIRE(fs::write_file(bootstrap_path, string(4097, 'x')));
        CHECK_FALSE(ReadClientRuntimeBootstrapTarget(bootstrap_path, runtime_file_name).has_value());
        CHECK(fs::remove_dir_tree(temp_dir));
    }
}

TEST_CASE("ClientSessionMarkerRecordsShutdownStageAcrossRuns")
{
    CHECK(fs::is_absolute_path(MakeClientSessionMarkerPath("")));

    std::filesystem::path base = std::filesystem::temp_directory_path() / std::format("lf_client_session_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    string temp_dir = fs::path_to_string(base);
    // An absolute path stands for the resolved writable root: fs::make_writable_path leaves it as given
    string marker = MakeClientSessionMarkerPath(fs::resolve_path(strex(temp_dir).combine_path("client.session").str()));
    ignore_unused(fs::remove_dir_tree(temp_dir));

    // A run that never started leaves nothing to report
    CHECK(!TakePreviousClientSession(marker).has_value());

    BeginClientSession(marker);
    REQUIRE(fs::exists(marker));

    // Every stage the shutdown reaches replaces the one before it, so the file always states how far it got
    SetClientShutdownStage(marker, ClientShutdownStage::MainLoopExited);
    SetClientShutdownStage(marker, ClientShutdownStage::ShutdownHookDone);

    auto interrupted = TakePreviousClientSession(marker);
    REQUIRE(interrupted.has_value());
    CHECK(interrupted->Stage == ClientShutdownStage::ShutdownHookDone);
    CHECK(interrupted->StageName == "ShutdownHookDone");
    CHECK(interrupted->BuildHash == string(FO_BUILD_HASH));
    CHECK(!interrupted->StartedAt.empty());
    CHECK(interrupted->TeardownSet.empty());
    // The marker names the process that wrote it; that process is this one, so it does not count as a survivor
    CHECK(strex("{}", interrupted->Pid).str() == platform::get_current_process_id_str());
    CHECK_FALSE(interrupted->StillRunning);

    // Taking it consumes it: the same interrupted run must not be reported by every later launch
    CHECK(!fs::exists(marker));
    CHECK(!TakePreviousClientSession(marker).has_value());

    // A finished exit keeps its marker, so an exit that hangs in the process teardown stays visible; once the
    // process is gone the next run consumes it without a report
    BeginClientSession(marker);
    SetClientShutdownStage(marker, ClientShutdownStage::RuntimeReturned);
    SetClientShutdownStage(marker, ClientShutdownStage::ExitRequested);
    CHECK(fs::exists(marker));
    auto exited = TakePreviousClientSession(marker);
    platform::process_identity process = platform::get_current_process_identity();

    if (process.pid > 0 && process.start_time != 0) {
        CHECK(!exited.has_value());
    }
    else {
        REQUIRE(exited.has_value());
        CHECK(exited->Stage == ClientShutdownStage::ExitRequested);
    }

    CHECK(!fs::exists(marker));

    // Staging a marker that was never begun writes nothing: the host records stages after the runtime
    // returned, and a runtime that failed before its settings never began one
    SetClientShutdownStage(marker, ClientShutdownStage::ExitRequested);
    CHECK(!fs::exists(marker));

    // A marker from a process that is gone, stopped before it asked to exit, is the unclean run it always was
    REQUIRE(fs::write_file(marker, "Stage: 6\nBuild: previous\nStarted: 2026-09-19 10:00:00\nPid: 1\nProcessStart: 1\nTeardown: global_pools\n"));
    auto stopped_in_teardown = TakePreviousClientSession(marker);
    REQUIRE(stopped_in_teardown.has_value());
    CHECK(stopped_in_teardown->Stage == ClientShutdownStage::GlobalDataTeardown);
    CHECK(stopped_in_teardown->StageName == "GlobalDataTeardown");
    CHECK(stopped_in_teardown->TeardownSet == "global_pools");
    CHECK(stopped_in_teardown->Pid == 1);
    CHECK_FALSE(stopped_in_teardown->StillRunning);

    // A marker an older build left, with no process named, keeps its old meaning
    REQUIRE(fs::write_file(marker, "Stage: 4\nBuild: previous\nStarted: 2026-09-19 10:00:00\n"));
    auto older_marker = TakePreviousClientSession(marker);
    REQUIRE(older_marker.has_value());
    CHECK(older_marker->Stage == ClientShutdownStage::ShutdownHookDone);
    CHECK(older_marker->Pid == 0);
    CHECK_FALSE(older_marker->StillRunning);

    // An unreadable marker is consumed rather than reported for ever
    REQUIRE(fs::write_file(marker, "not a marker"));
    auto unparsable = TakePreviousClientSession(marker);
    REQUIRE(unparsable.has_value());
    CHECK(unparsable->Stage == ClientShutdownStage::Unknown);
    CHECK(!fs::exists(marker));

    CHECK(fs::remove_dir_tree(temp_dir));
}

TEST_CASE("ClientSessionMarkerDoesNotInventACleanExit")
{
    std::filesystem::path base = std::filesystem::temp_directory_path() / std::format("lf_client_session_invalid_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    string temp_dir = fs::path_to_string(base);
    string marker = MakeClientSessionMarkerPath(temp_dir);
    auto cleanup = scope_exit([&]() noexcept { (void)fs::remove_dir_tree(temp_dir); });

    SECTION("UnknownStageIsNotExitRequested")
    {
        string_view stage = GENERATE("99", "-1", "256", "7.5", "7x", "");
        REQUIRE(fs::write_file(marker, strex("Stage: {}\nBuild: previous\nPid: 1\nProcessStart: 1\n", stage).str()));
        auto previous = TakePreviousClientSession(marker);
        REQUIRE(previous.has_value());
        CHECK(previous->StageName == "Unknown");
    }

    SECTION("ExitWithoutProcessIdentityIsNotProvenClean")
    {
        REQUIRE(fs::write_file(marker, "Stage: 7\nBuild: previous\n"));
        auto previous = TakePreviousClientSession(marker);
        REQUIRE(previous.has_value());
        CHECK(previous->Stage == ClientShutdownStage::ExitRequested);
    }

    SECTION("AnotherClientCannotFinishThisSession")
    {
        platform::process_identity process = platform::get_current_process_identity();
        string foreign_marker = strex("Stage: 0\nBuild: other\nPid: {}\nProcessStart: {}\n", process.pid, process.start_time + 1).str();
        REQUIRE(fs::write_file(marker, foreign_marker));
        SetClientShutdownStage(marker, ClientShutdownStage::ExitRequested);
        CHECK(fs::read_file(marker).value_or("") == foreign_marker);
    }
}

namespace
{
    // What the marker said at the moment each fake set was torn down, read from inside its delete callback
    string TeardownMarkerPath {};
    vector<string> TeardownMarkerSnapshots {};
    string TeardownMarkerReplacement {};

    void SnapshotTeardownMarker() noexcept
    {
        TeardownMarkerSnapshots.emplace_back(fs::read_file(TeardownMarkerPath).value_or(""));

        if (!TeardownMarkerReplacement.empty()) {
            (void)fs::write_file(TeardownMarkerPath, TeardownMarkerReplacement);
            TeardownMarkerReplacement.clear();
        }
    }

    // Stands in for the real sweep: it builds nothing, and afterwards hands the set back as created without
    // running a constructor, since the real globals of the test process were never touched
    struct FakeGlobalDataSweep final
    {
        std::array<global_data::callback, global_data::MAX_CALLBACKS> SavedDelete {};
        std::array<const char*, global_data::MAX_CALLBACKS> SavedNames {};
        int32_t SavedCount {};

        FakeGlobalDataSweep()
        {
            std::copy(std::begin(global_data::delete_callbacks), std::end(global_data::delete_callbacks), SavedDelete.begin());
            std::copy(std::begin(global_data::callback_names), std::end(global_data::callback_names), SavedNames.begin());
            SavedCount = global_data::callbacks_count;

            global_data::callbacks_count = 2;
            global_data::delete_callbacks[0] = &SnapshotTeardownMarker;
            global_data::delete_callbacks[1] = &SnapshotTeardownMarker;
            global_data::callback_names[0] = "FakeSetA";
            global_data::callback_names[1] = "FakeSetB";
        }

        ~FakeGlobalDataSweep()
        {
            global_data::callbacks_count = 0;
            (void)global_data::create();

            std::copy(SavedDelete.begin(), SavedDelete.end(), std::begin(global_data::delete_callbacks));
            std::copy(SavedNames.begin(), SavedNames.end(), std::begin(global_data::callback_names));
            global_data::callbacks_count = SavedCount;
        }
    };
}

TEST_CASE("ClientSessionMarkerNamesTheGlobalDataPartTeardownStoppedIn")
{
    std::filesystem::path base = std::filesystem::temp_directory_path() / std::format("lf_client_teardown_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    string temp_dir = fs::path_to_string(base);
    string marker = MakeClientSessionMarkerPath(fs::resolve_path(strex(temp_dir).combine_path("client.session").str()));
    ignore_unused(fs::remove_dir_tree(temp_dir));

    TeardownMarkerPath = marker;
    TeardownMarkerSnapshots.clear();
    TeardownMarkerReplacement.clear();

    SECTION("EachPartIsNamedBeforeItGoes")
    {
        BeginClientSession(marker);
        SetClientShutdownStage(marker, ClientShutdownStage::ShutdownHookDone);

        {
            FakeGlobalDataSweep sweep;
            DestroyGlobalDataRecordingTeardown(marker);
        }

        // Each delete callback saw the stage and its own name, which is what a run stuck inside it leaves behind
        REQUIRE(TeardownMarkerSnapshots.size() == 2);
        CHECK(TeardownMarkerSnapshots[0].find("Stage: 6\n") == 0);
        CHECK(TeardownMarkerSnapshots[0].find("Teardown: FakeSetA\n") != string::npos);
        CHECK(TeardownMarkerSnapshots[1].find("Teardown: FakeSetB\n") != string::npos);
        CHECK(TeardownMarkerSnapshots[1].find("FakeSetA") == string::npos);
        CHECK(TeardownMarkerSnapshots[1].find("Build: ") != string::npos);

        // Past the last part the name is gone again: a stop there is on the way back to the host
        REQUIRE(fs::write_file(strex(temp_dir).combine_path("stuck.session").str(), TeardownMarkerSnapshots[1]));
        auto stuck = TakePreviousClientSession(strex(temp_dir).combine_path("stuck.session").str());
        REQUIRE(stuck.has_value());
        CHECK(stuck->Stage == ClientShutdownStage::GlobalDataTeardown);
        CHECK(stuck->TeardownSet == "FakeSetB");

        auto finished = TakePreviousClientSession(marker);
        REQUIRE(finished.has_value());
        CHECK(finished->Stage == ClientShutdownStage::GlobalDataTeardown);
        CHECK(finished->TeardownSet.empty());

        // The host records its own stage next, and the teardown line does not ride along into it
        BeginClientSession(marker);
        {
            FakeGlobalDataSweep sweep;
            DestroyGlobalDataRecordingTeardown(marker);
        }
        SetClientShutdownStage(marker, ClientShutdownStage::RuntimeReturned);
        CHECK(fs::read_file(marker).value_or("").find("Teardown:") == string::npos);
    }

    SECTION("WithoutAMarkerTheSetIsStillTornDown")
    {
        {
            FakeGlobalDataSweep sweep;
            TeardownMarkerPath = strex(temp_dir).combine_path("absent.session").str();
            DestroyGlobalDataRecordingTeardown("");
        }

        CHECK(TeardownMarkerSnapshots.size() == 2);
        CHECK(!fs::exists(marker));
    }

    SECTION("AnotherClientsMarkerSurvivesTeardown")
    {
        platform::process_identity process = platform::get_current_process_identity();
        string foreign_marker = strex("Stage: 0\nBuild: other\nPid: {}\nProcessStart: {}\n", process.pid, process.start_time + 1).str();
        REQUIRE(fs::write_file(marker, foreign_marker));

        {
            FakeGlobalDataSweep sweep;
            DestroyGlobalDataRecordingTeardown(marker);
        }

        REQUIRE(TeardownMarkerSnapshots.size() == 2);
        CHECK(TeardownMarkerSnapshots[0] == foreign_marker);
        CHECK(TeardownMarkerSnapshots[1] == foreign_marker);
        CHECK(fs::read_file(marker).value_or("") == foreign_marker);
    }

    SECTION("MarkerReplacedBetweenSetsSurvivesRemainingTeardown")
    {
        BeginClientSession(marker);
        platform::process_identity process = platform::get_current_process_identity();
        string foreign_marker = strex("Stage: 0\nBuild: other\nPid: {}\nProcessStart: {}\n", process.pid, process.start_time + 1).str();
        TeardownMarkerReplacement = foreign_marker;

        {
            FakeGlobalDataSweep sweep;
            DestroyGlobalDataRecordingTeardown(marker);
        }

        REQUIRE(TeardownMarkerSnapshots.size() == 2);
        CHECK(TeardownMarkerSnapshots[0].find("Teardown: FakeSetA\n") != string::npos);
        CHECK(TeardownMarkerSnapshots[1] == foreign_marker);
        CHECK(fs::read_file(marker).value_or("") == foreign_marker);
    }

    ignore_unused(fs::remove_dir_tree(temp_dir));
}

FO_END_NAMESPACE
