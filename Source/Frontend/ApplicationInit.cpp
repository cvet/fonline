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

#include "Application.h"

#include "CacheStorage.h"
#include "ConfigFile.h"
#include "DiagnosticSelfTest.h"
#include "FileSystem.h"
#include "WebRelated.h"

FO_BEGIN_NAMESPACE

// File the installer drops next to the exe to mark an installed (non-portable) build. The portable
// zip has no marker and keeps writing next to the exe.
static constexpr u8string_view INSTALLED_MARKER_NAME {u8"INSTALLED"};

static unique_nptr<Application> App {};

extern void ApplicationInitHook(AppInitFlags flags, GlobalSettings& settings);

static void SetupExceptionCallback(bool show_message_on_exception);
static void InitAppImpl(CommandLineArgs args, AppInitFlags flags, bool unit_testing);
static auto LoadTestingAppSettings() -> GlobalSettings;
static void PrebakeResources(BakingSettings& settings);
static void SetupSignals();

auto IsAppInitialized() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !!App;
}

auto GetApp() noexcept -> ptr<Application>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(IsAppInitialized(), "Application accessed before initialization");
    return App;
}

void ResetApp() noexcept
{
    FO_STACK_TRACE_ENTRY();

    App.reset();
}

void InitApp(CommandLineArgs args, AppInitFlags flags)
{
    FO_STACK_TRACE_ENTRY();

    InitAppImpl(args, flags, false);
}

void InitAppForTesting(AppInitFlags flags)
{
    FO_STACK_TRACE_ENTRY();

    InitAppImpl({}, flags, true);
}

static void InitAppImpl(CommandLineArgs args, AppInitFlags flags, bool unit_testing)
{
    FO_STACK_TRACE_ENTRY();

    // Ensure that we call init only once
    static std::once_flag once;
    bool first_call = false;
    std::call_once(once, [&first_call] { first_call = true; });
    FO_STRONG_ASSERT(first_call, "Application can be initialized only once");

    // Fork the process if requested
    if (std::ranges::any_of(args, [](const u8string& arg) { return arg.view() == u8"--fork"; })) {
        Platform::ForkProcess();
    }

    // Create global data as soon as possible
    CreateGlobalData();

    // Write log and show message box on exception
    SetupExceptionCallback(IsEnumSet(flags, AppInitFlags::ShowMessageOnException));

    // Tracy
#if FO_TRACY
    TracySetProgramName(FO_NICE_NAME);
#endif

    // Logging
    LogToFile(GetExeLogFileName(), IsEnumSet(flags, AppInitFlags::AppendLogFile));

    if (IsEnumSet(flags, AppInitFlags::DisableLogTags)) {
        LogDisableTags();
    }

    WriteLog("Starting {}", FO_NICE_NAME);

    // Load settings
    auto settings = unit_testing ? LoadTestingAppSettings() : LoadAppSettings(args);

    // Installed client: the install dir is read-only, so move the log file into the per-user writable
    // data dir now that settings (and the resolved writable path) are known.
    if (!settings.UserWritablePath.empty()) {
        const u8string user_writable_path = settings.UserWritablePath;
        const u8string log_file_name = GetExeLogFileName();
        const u8string log_path = fs_make_writable_path(user_writable_path.view(), log_file_name.view());
        WriteLog("Switch log to path '{}'", log_path);
        LogToFile(log_path, IsEnumSet(flags, AppInitFlags::AppendLogFile));
        WriteLog("Starting {}", FO_NICE_NAME);
    }

    WriteLog("Version: {}", settings.GameVersion);

    // Disable message box on exception if headless window is used
    if (IsEnumSet(flags, AppInitFlags::ShowMessageOnException) && settings.HeadlessWindow) {
        SetupExceptionCallback(false);
    }

    // Switch logging to a dedicated worker thread once the user setting is known
    if (settings.AsyncLogWrite) {
        SetAsyncLogWriting(true);
    }

    // Diagnostic self-test: with logging, the exception callback and the async-log mode all live, verify
    // that crash diagnostics reach the log for the crash class named by FO_SELFTEST_CRASH. Inert otherwise.
    DiagnosticSelfTest::RunIfRequested();

    // Project-side early init (before App frontend, after settings + exception/log callbacks)
    ApplicationInitHook(flags, settings);

    // Prebake resources
    if (!IsPackaged() && IsEnumSet(flags, AppInitFlags::PrebakeResources)) {
        WriteLog("Prebake resources");
        PrebakeResources(settings);
    }

    // Application frontend initialization
    App = SafeAlloc::MakeUnique<Application>(std::move(settings), flags);

    // Request quit on bad alloc
    SetBadAllocCallback([]() FO_DEFERRED { GetApp()->RequestQuit(); });

    // Request quit on interrupt signals
    SetupSignals();

    // Set up clipboard events for web
    WebRelated::SetupClipboard();
}

static void SetupExceptionCallback(bool show_message_on_exception)
{
    FO_STACK_TRACE_ENTRY();

    SetExceptionCallback([show_message_on_exception](u8string_view message, const CatchedStackTraceData& st, bool fatal_error) FO_DEFERRED {
        WriteLogMessage(LogType::Error, message, &st);

        if (fatal_error) {
            WriteLogMessage(LogType::Error, u8"Shutdown!");

#if FO_WEB
            if (IsAppInitialized()) {
                GetApp()->RequestQuit();
            }
#endif
        }

        if (show_message_on_exception || (!IsPackaged() && (fatal_error || !IsAppInitialized()))) {
            const u8string traceback = FormatStackTrace(st);
            Application::ShowErrorMessage(message, traceback, fatal_error);
        }
    });
}

static auto LoadTestingAppSettings() -> GlobalSettings
{
    FO_STACK_TRACE_ENTRY();

    auto settings = GlobalSettings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    return settings;
}

auto LoadAppSettings(CommandLineArgs args) -> GlobalSettings
{
    FO_STACK_TRACE_ENTRY();

    auto settings = GlobalSettings(false);

    if (!IsPackaged()) {
        // Apply config
        u8string config_to_apply;
        u8string config_to_apply_dir;
        bool auto_find_config = false;

        for (size_t i = 0; i < args.size(); i++) {
            auto arg = args.Get(i);

            if (arg.empty()) {
                continue;
            }

            const u8string arg_value = u8strex(arg).trim();

            if (arg_value == u8"-ApplyConfig" || arg_value == u8"--ApplyConfig") {
                auto next_arg = args.Get(i + 1);

                if (i + 1 >= args.size() || CommandLineArgs::IsOption(next_arg)) {
                    throw AppInitException("Config name not provided");
                }

                const u8string config_path = u8strex(next_arg).trim();
                const std::filesystem::path native_config_path {fs_make_path(config_path.view())};
                config_to_apply = fs_path_to_u8string(native_config_path.filename());
                config_to_apply_dir = fs_path_to_u8string(native_config_path.parent_path());
            }
        }

        if (config_to_apply.empty()) {
            auto_find_config = true;
            auto dir = std::filesystem::current_path();

            while (true) {
                const auto config_path = fs_path_to_u8string(dir / FO_MAIN_CONFIG);

                if (fs_exists(config_path.view()) && !fs_is_dir(config_path.view())) {
                    config_to_apply = FO_MAIN_CONFIG;
                    config_to_apply_dir = fs_path_to_u8string(dir);
                    break;
                }
                else {
                    if (dir.has_parent_path()) {
                        dir = dir.parent_path();
                    }
                    else {
                        throw AppInitException("Config file not found", FO_MAIN_CONFIG);
                    }
                }
            }
        }

        const u8string applied_config_path = fs_path_to_u8string(std::filesystem::path {fs_make_path(config_to_apply_dir.view())} / std::filesystem::path {fs_make_path(config_to_apply.view())});
        WriteLog("Apply config {}", applied_config_path.view());
        settings.ApplyConfigAtPath(config_to_apply.view(), config_to_apply_dir.view());

        // Apply sub config
        vector<string> sub_configs_to_apply;

        for (size_t i = 0; i < args.size(); i++) {
            auto arg = args.Get(i);

            if (arg.empty()) {
                continue;
            }

            const u8string arg_value = u8strex(arg).trim();

            if (arg_value == u8"-ApplySubConfig" || arg_value == u8"--ApplySubConfig") {
                auto next_arg = args.Get(i + 1);

                if (i + 1 >= args.size() || CommandLineArgs::IsOption(next_arg)) {
                    throw AppInitException("Sub config name not provided");
                }

                sub_configs_to_apply.emplace_back(utf8_to_string(u8strvex(next_arg).trim()));
            }
        }

        if (auto_find_config && sub_configs_to_apply.empty()) {
            sub_configs_to_apply.emplace_back(settings.UnpackagedSubConfig);
        }

        for (const auto& sub_config_name : sub_configs_to_apply) {
            if (!sub_config_name.empty() && sub_config_name != "NONE") {
                WriteLog("Apply sub config {}", sub_config_name);
                settings.ApplySubConfigSection(sub_config_name);
            }
        }
    }
    else {
        settings.ApplyInternalConfig();
    }

    // Resolve the installed-client writable root now that the config is applied, so the local-config
    // cache below — and all later cache/log/update writes — land in the per-user writable directory.
    ResolveUserWritablePath(settings);

    const u8string user_writable_path = settings.UserWritablePath;
    const u8string cache_resources = settings.CacheResources;
    const u8string cache_dir = fs_make_writable_path(user_writable_path.view(), cache_resources.view());

    if (fs_is_dir(cache_dir.view())) {
        const auto cache = CacheStorage(cache_dir.view());

        if (cache.HasEntry(LOCAL_CONFIG_NAME)) {
            const u8string cache_text = cache.GetText(LOCAL_CONFIG_NAME);
            const u8string config_name = LOCAL_CONFIG_NAME;
            auto config = ConfigFile(config_name.view(), cache_text);
            settings.ApplyConfigFile(config, u8string_view {});
        }
    }

    settings.ApplyCommandLine(args);
    settings.ApplyAutoSettings();
    return settings;
}

void ResolveUserWritablePath(GlobalSettings& settings)
{
    FO_STACK_TRACE_ENTRY();

    // Resolve settings.UserWritablePath to an absolute writable root, or "" to stay portable.
    u8string root = settings.UserWritablePath;

    if (root.empty()) {
        // No explicit path: switch to the per-user writable layout only when the installer marker is
        // present next to the exe; otherwise stay portable.
        const auto exe_path = Platform::GetExePath();

        const u8string marker_path = exe_path.has_value() ? fs_path_to_u8string(std::filesystem::path {fs_make_path(exe_path->view())}.parent_path() / std::filesystem::path {fs_make_path(INSTALLED_MARKER_NAME)}) : u8string {};

        if (!exe_path.has_value() || !fs_exists(marker_path.view())) {
            settings.UserWritablePath = u8string {};
            return;
        }

        root = u8string {u8"*"};
    }

    if (root.view() == u8"*") {
        const u8string base = Platform::GetUserDataBase();

        if (base.empty()) {
            WriteLog(LogType::Warning, "Client user-writable path requested but no user data dir found; using portable layout");
            settings.UserWritablePath = u8string {};
            return;
        }

        const u8string game_name = settings.GameName;
        root = fs_path_to_u8string(std::filesystem::path {fs_make_path(base.view())} / std::filesystem::path {fs_make_path(game_name.view())});
    }

    root = fs_resolve_path(root.view());

    if (!fs_create_directories(root.view())) {
        WriteLog(LogType::Warning, "Can't create client user-writable path '{}'; using portable layout", root.view());
        settings.UserWritablePath = u8string {};
        return;
    }

    settings.UserWritablePath = root;

    // Pre-create the writable cache + resource-overlay subdirs so the cache and the self-update
    // resource writer never fail on a missing parent directory.
    const u8string cache_resources = settings.CacheResources;
    const u8string client_resources = settings.ClientResources;
    const u8string cache_path = fs_make_writable_path(root.view(), cache_resources.view());
    const u8string client_path = fs_make_writable_path(root.view(), client_resources.view());
    (void)fs_create_directories(cache_path.view());
    (void)fs_create_directories(client_path.view());

    WriteLog("Client user-writable data path: {}", root.view());
}

static void PrebakeResources(BakingSettings& settings)
{
    FO_STACK_TRACE_ENTRY();

    using BakeResourcesFunc = bool (*)(void*);
    auto bake_resources = Platform::GetFuncAddr<BakeResourcesFunc>(nullptr, string_view_nt {"FO_BakeResources"});

    nptr<void> baker_dll = nullptr;
    auto unload_baker_dll = scope_exit([&]() noexcept { Platform::UnloadModule(baker_dll); });

    const u8string lib_name = u8strex("{}_BakerLib", FO_DEV_NAME);

    if (bake_resources == nullptr) {
        const auto exe_path = Platform::GetExePath();
        const u8string lib_path = u8strex(exe_path ? exe_path->view() : u8string_view {}).extract_dir().combine_path(lib_name);
        baker_dll = Platform::LoadModule(lib_path.view_nt());

        if (baker_dll) {
            bake_resources = Platform::GetFuncAddr<BakeResourcesFunc>(baker_dll, string_view_nt {"FO_BakeResources"});
        }
    }

    if (bake_resources != nullptr) {
        bool result = false;
        Application::ShowProgressWindow("Baking resources... Please wait", [&] { result = bake_resources(&settings); });

        if (!result) {
            throw AppInitException("Resource baking failed. See baker log for details");
        }
    }
    else {
        const u8string bake_output = settings.BakeOutput;

        if (fs_exists(bake_output.view()) && fs_is_dir(bake_output.view())) {
            if (!settings.IgnoreMissingBakerWarning) {
                Application::ShowErrorMessage(u8strex("Warning! {} not found. Resources may be out of date", lib_name), u8"", false);
            }
        }
        else {
            throw AppInitException("Baker not found. Unable to bake resources");
        }
    }
}

auto GetExeLogFileName() -> u8string
{
    FO_STACK_TRACE_ENTRY();

    if (const auto exe_path = Platform::GetExePath()) {
        const std::filesystem::path native_exe_path {fs_make_path(exe_path->view())};
        const u8string executable_stem = fs_path_to_u8string(native_exe_path.stem());
        return u8strex("{}.log", executable_stem);
    }

    return u8strex("{}.log", FO_DEV_NAME);
}

#if FO_LINUX || FO_MAC
// Written from the signal handler, so it must stay async-signal-safe: a lock-free atomic store is
// the only thing the handler may do (no logging, allocation or condition-variable work — malloc or
// a cv notify from a signal can deadlock against the interrupted thread). Process-global by nature:
// a signal targets the process, not an engine instance. Consumed via IsQuitSignalReceived().
static std::atomic<bool> QuitSignalReceived {};
static_assert(std::atomic<bool>::is_always_lock_free);

static void SignalHandler(int sig)
{
    std::signal(sig, SignalHandler);
    QuitSignalReceived.store(true, std::memory_order_release);
}
#endif

auto IsQuitSignalReceived() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC
    return QuitSignalReceived.load(std::memory_order_acquire);
#else
    return false;
#endif
}

static void SetupSignals()
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
#endif
}

FO_END_NAMESPACE
