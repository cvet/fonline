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

// Versioned file the installer drops next to the exe to mark an installed (non-portable) build and
// identify its per-user directory. The portable zip has no marker and keeps writing next to the exe.
static constexpr string_view INSTALLED_MARKER_NAME = "INSTALLED";
static constexpr string_view INSTALLED_MARKER_HEADER = "FONLINE_INSTALLED_CLIENT_V1";
static constexpr size_t INSTALLED_MARKER_MAX_SIZE = 256;
static constexpr size_t INSTALLED_DIRECTORY_NAME_MAX_SIZE = 128;

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
    if (std::ranges::any_of(args, [](nptr<const char> arg) { return arg && string_view(arg.get()) == "--fork"; })) {
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

    // Logging. An installed client resolves this before settings so both the thin host and the
    // runtime module can report startup/self-update failures without writing under Program Files.
    string initial_log_path = GetExeLogFileName();
    const optional<string> installed_root = ResolveCurrentInstalledClientWritableRoot();

    if (installed_root.has_value() && fs_create_directories(*installed_root)) {
        initial_log_path = strex(*installed_root).combine_path(GetExeLogFileName()).str();
    }

    LogToFile(initial_log_path, IsEnumSet(flags, AppInitFlags::AppendLogFile));

    if (IsEnumSet(flags, AppInitFlags::DisableLogTags)) {
        LogDisableTags();
    }

    WriteLog("Starting {}", FO_NICE_NAME);

    // Load settings
    auto settings = unit_testing ? LoadTestingAppSettings() : LoadAppSettings(args);

    // Installed client: the install dir is read-only, so move the log file into the per-user writable
    // data dir now that settings (and the resolved writable path) are known.
    if (!settings.UserWritablePath.empty()) {
        string log_path = fs_make_writable_path(settings.UserWritablePath, GetExeLogFileName());
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

    SetExceptionCallback([show_message_on_exception](string_view message, const CatchedStackTraceData& st, bool fatal_error) FO_DEFERRED {
        WriteLogMessage(LogType::Error, message, &st);

        if (fatal_error) {
            WriteLogMessage(LogType::Error, "Shutdown!");

#if FO_WEB
            if (IsAppInitialized()) {
                GetApp()->RequestQuit();
            }
#endif
        }

        if (show_message_on_exception || (!IsPackaged() && (fatal_error || !IsAppInitialized()))) {
            Application::ShowErrorMessage(message, FormatStackTrace(st), fatal_error);
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
        string config_to_apply;
        string config_to_apply_dir;
        bool auto_find_config = false;

        for (size_t i = 0; i < args.size(); i++) {
            string_view arg = args.Get(i);

            if (arg.empty()) {
                continue;
            }

            string_view arg_view = strex(arg).trim().strv();

            if (arg_view == "-ApplyConfig" || arg_view == "--ApplyConfig") {
                string_view next_arg = args.Get(i + 1);

                if (i + 1 >= args.size() || CommandLineArgs::IsOption(next_arg)) {
                    throw AppInitException("Config name not provided");
                }

                config_to_apply = strex(next_arg).trim().extract_file_name();
                config_to_apply_dir = strex(next_arg).trim().extract_dir();
            }
        }

        if (config_to_apply.empty()) {
            auto_find_config = true;
            auto dir = std::filesystem::current_path();

            while (true) {
                string config_path = fs_path_to_string(dir / FO_MAIN_CONFIG);

                if (fs_exists(config_path) && !fs_is_dir(config_path)) {
                    config_to_apply = FO_MAIN_CONFIG;
                    config_to_apply_dir = strex("{}", dir.string()).normalize_path_slashes();
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

        WriteLog("Apply config {}", strex(config_to_apply_dir).combine_path(config_to_apply));
        settings.ApplyConfigAtPath(config_to_apply, config_to_apply_dir);

        // Apply sub config
        vector<string> sub_configs_to_apply;

        for (size_t i = 0; i < args.size(); i++) {
            string_view arg = args.Get(i);

            if (arg.empty()) {
                continue;
            }

            string_view arg_view = strex(arg).trim().strv();

            if (arg_view == "-ApplySubConfig" || arg_view == "--ApplySubConfig") {
                string_view next_arg = args.Get(i + 1);

                if (i + 1 >= args.size() || CommandLineArgs::IsOption(next_arg)) {
                    throw AppInitException("Sub config name not provided");
                }

                sub_configs_to_apply.emplace_back(strex(next_arg).trim());
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

    string cache_dir = fs_make_writable_path(settings.UserWritablePath, settings.CacheResources);

    if (fs_is_dir(cache_dir)) {
        auto cache = CacheStorage(cache_dir);

        if (cache.HasEntry(LOCAL_CONFIG_NAME)) {
            auto config = ConfigFile(cache.GetString(LOCAL_CONFIG_NAME));
            settings.ApplyConfigFile(config, "");
        }
    }

    settings.ApplyCommandLine(args);
    settings.ApplyAutoSettings();
    // Command-line/local overrides may change this bootstrap setting. Resolve it again
    // idempotently; a valid installed marker remains authoritative for installed packages.
    ResolveUserWritablePath(settings);
    return settings;
}

static auto IsSafeInstalledDirectoryName(string_view name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (name.empty() || name.size() > INSTALLED_DIRECTORY_NAME_MAX_SIZE || name == "." || name == ".." || name.back() == '.' || name.back() == ' ') {
        return false;
    }

    for (const char ch : name) {
        if ((ch >= 0 && ch < 0x20) || string_view("<>:\"/\\|?*").find(ch) != string_view::npos) {
            return false;
        }
    }

    const size_t extension_pos = name.find('.');
    const string device_name = strex(name.substr(0, extension_pos)).lower().rtrim(" ").str();

    if (device_name == "con" || device_name == "prn" || device_name == "aux" || device_name == "nul") {
        return false;
    }

    if (device_name.starts_with("com") || device_name.starts_with("lpt")) {
        const string_view device_digit = string_view(device_name).substr(3);
        const bool is_ascii_device_digit = device_digit.size() == 1 && device_digit[0] >= '1' && device_digit[0] <= '9';
        // Windows reserves superscript 1/2/3 as COM/LPT device-number aliases as well.
        const bool is_superscript_device_digit = device_digit == "\xC2\xB9" || device_digit == "\xC2\xB2" || device_digit == "\xC2\xB3";

        if (is_ascii_device_digit || is_superscript_device_digit) {
            return false;
        }
    }

    return true;
}

auto ResolveInstalledClientWritableRoot(string_view installed_marker_path, string_view user_data_base) -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

    if (user_data_base.empty()) {
        return std::nullopt;
    }

    const optional<uint64_t> marker_size = fs_file_size(installed_marker_path);

    if (!marker_size.has_value() || *marker_size > INSTALLED_MARKER_MAX_SIZE) {
        return std::nullopt;
    }

    const optional<string> marker_content = fs_read_file(installed_marker_path);

    if (!marker_content.has_value()) {
        return std::nullopt;
    }

    const vector<string> marker_lines = strex(*marker_content).normalize_line_endings().split('\n');

    if (marker_lines.size() != 2 || marker_lines[0] != INSTALLED_MARKER_HEADER || !IsSafeInstalledDirectoryName(marker_lines[1])) {
        return std::nullopt;
    }

    return fs_resolve_path(strex(user_data_base).combine_path(marker_lines[1]).str());
}

auto ResolveCurrentInstalledClientWritableRoot() -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

    const optional<string> exe_path = Platform::GetExePath();

    if (!exe_path.has_value()) {
        return std::nullopt;
    }

    const string installed_marker_path = strex(*exe_path).extract_dir().combine_path(INSTALLED_MARKER_NAME).str();
    return ResolveInstalledClientWritableRoot(installed_marker_path, Platform::GetUserDataBase());
}

void ResolveUserWritablePath(GlobalSettings& settings)
{
    FO_STACK_TRACE_ENTRY();

    // Resolve settings.UserWritablePath to an absolute writable root, or "" to stay portable.
    const optional<string> installed_root = ResolveCurrentInstalledClientWritableRoot();
    string root = installed_root.has_value() ? *installed_root : string(settings.UserWritablePath);

    if (root.empty()) {
        settings.UserWritablePath = "";
        return;
    }

    if (root == "*") {
        string base = Platform::GetUserDataBase();

        if (base.empty()) {
            WriteLog(LogType::Warning, "Client user-writable path requested but no user data dir found; using portable layout");
            settings.UserWritablePath = "";
            return;
        }

        root = strex(base).combine_path(settings.GameName).str();
    }

    root = fs_resolve_path(root);

    if (!fs_create_directories(root)) {
        WriteLog(LogType::Warning, "Can't create client user-writable path '{}'; using portable layout", root);
        settings.UserWritablePath = "";
        return;
    }

    settings.UserWritablePath = root;

    // Pre-create the writable cache + resource-overlay subdirs so the cache and the self-update
    // resource writer never fail on a missing parent directory.
    fs_create_directories(fs_make_writable_path(settings.UserWritablePath, settings.CacheResources));
    fs_create_directories(fs_make_writable_path(settings.UserWritablePath, settings.ClientResources));

    WriteLog("Client user-writable data path: {}", root);
}

static void PrebakeResources(BakingSettings& settings)
{
    FO_STACK_TRACE_ENTRY();

    using BakeResourcesFunc = bool (*)(void*);
    auto bake_resources = Platform::GetFuncAddr<BakeResourcesFunc>(nullptr, "FO_BakeResources");

    nptr<void> baker_dll = nullptr;
    auto unload_baker_dll = scope_exit([&]() noexcept { Platform::UnloadModule(baker_dll); });

    strex lib_name = strex("{}_BakerLib", FO_DEV_NAME);

    if (bake_resources == nullptr) {
        auto exe_path = Platform::GetExePath();
        string lib_path = strex(exe_path.value_or("")).extract_dir().combine_path(lib_name).str();
        baker_dll = Platform::LoadModule(lib_path);

        if (baker_dll) {
            bake_resources = Platform::GetFuncAddr<BakeResourcesFunc>(baker_dll, "FO_BakeResources");
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
        if (fs_exists(settings.BakeOutput) && fs_is_dir(settings.BakeOutput)) {
            if (!settings.IgnoreMissingBakerWarning) {
                Application::ShowErrorMessage(strex("Warning! {} not found. Resources may be out of date", lib_name), "", false);
            }
        }
        else {
            throw AppInitException("Baker not found. Unable to bake resources");
        }
    }
}

auto GetExeLogFileName() -> string
{
    FO_STACK_TRACE_ENTRY();

    if (auto exe_path = Platform::GetExePath()) {
        return strex("{}.log", strex(exe_path.value()).extract_file_name().erase_file_extension());
    }

    return strex("{}.log", FO_DEV_NAME);
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
