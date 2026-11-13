// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "Application.h"

#include "CacheStorage.h"
#include "ConfigFile.h"
#include "DiagnosticSelfTest.h"
#include "FileSystem.h"
#include "WebRelated.h"

FO_BEGIN_NAMESPACE

// File the installer drops next to the exe to mark an installed (non-portable) build. The portable
// zip has no marker and keeps writing next to the exe
static constexpr string_view INSTALLED_MARKER_NAME {"INSTALLED"};

FO_KEEP_DATA_SYMBOL char PACKAGED_BUILD_NAME[128] {"###NotPackaged###"
                                                   "##############################################################################################################"};
static auto ReadPackagedBuildName() -> string;
static const string PackagedBuildName {ReadPackagedBuildName()};

static unique_nptr<Application> App {};

// Startup failures can precede the application, so error-dialog policy belongs to the process set
struct ErrorMessagePolicyData
{
    bool NonInteractive {};

    // Borrowed only while ApplicationInitHook runs, so failures reported from inside it see live changes
    nptr<const GlobalSettings> InitializingSettings {};
};
FO_GLOBAL_DATA(ErrorMessagePolicyData, ErrorMessagePolicy);

void ApplicationInitHook(AppInitFlags flags, GlobalSettings& settings);

static auto IsPackaged() -> bool;
static void SetupExceptionCallback(bool show_message_on_exception);
static void InitAppImpl(CommandLineArgs args, AppInitFlags flags, bool unit_testing);
static auto LoadTestingAppSettings() -> GlobalSettings;
static auto ResolveAppSettingDirectory(const CommandLineArgs& args, string_view name, string directory) -> string;
static void PrebakeResources(BakingSettings& settings);
static void SetupSignals();

auto IsAppInitialized() noexcept -> bool
{
    return !!App;
}

auto GetApp() noexcept -> ptr<Application>
{
    FO_STRONG_ASSERT(IsAppInitialized(), "Application accessed before initialization");
    return App;
}

void ResetApp() noexcept
{
    App.reset();
}

auto IsNonInteractiveMode() -> bool
{
#if FO_HEADLESS_APP
    return true;
#else
    if (IsAppInitialized()) {
        return GetApp()->Settings.Common.NonInteractive;
    }

    if (ErrorMessagePolicy.is_created() && ErrorMessagePolicy->InitializingSettings) {
        return ErrorMessagePolicy->InitializingSettings->Common.NonInteractive;
    }

    return ErrorMessagePolicy.is_created() && ErrorMessagePolicy->NonInteractive;
#endif
}

void InitApp(CommandLineArgs args, AppInitFlags flags)
{
    InitAppImpl(args, flags, false);
}

void InitAppForTesting(AppInitFlags flags)
{
    InitAppImpl({}, flags, true);
}

static void InitAppImpl(CommandLineArgs args, AppInitFlags flags, bool unit_testing)
{
    FO_TRACE_ZONE(App);

    // Ensure that we call init only once
    static std::once_flag once {};
    bool first_call {false};
    std::call_once(once, [&first_call] { first_call = true; });
    FO_STRONG_ASSERT(first_call, "Application can be initialized only once");

    // Fork the process if requested
    if (args.GetSettingValue(CommandLineSettings::Fork).has_value()) {
        (void)platform::fork_process();
    }

    // Create global data as soon as possible
    (void)global_data::create();

    // Read the unattended-launch switch before configuration loading itself can fail
    ErrorMessagePolicy->NonInteractive = strvex(args.GetSettingValue(SettingNames::Common_NonInteractive).value_or("0")).to_bool();

    // Write log and show message box on exception
    SetupExceptionCallback(is_enum_set(flags, AppInitFlags::ShowMessageOnException));

    // Tracy
#if FO_TRACE_ENABLED
    TracySetProgramName(FO_NICE_NAME);
#endif

    // Logging. The writable root is resolved from the command line and the installer marker alone, so
    // the log opens at its final location before anything is read from disk
    logging::to_file(fs::make_writable_path(ResolveWritableRoot(args), GetExeLogFileName()), is_enum_set(flags, AppInitFlags::AppendLogFile));

    if (is_enum_set(flags, AppInitFlags::DisableLogTags)) {
        logging::disable_tags();
    }

    logging::write("Starting {}", FO_NICE_NAME);

    auto settings {unit_testing ? LoadTestingAppSettings() : LoadAppSettings(args, flags)};
    ErrorMessagePolicy->NonInteractive = settings.Common.NonInteractive;

    logging::write("Version: {}", settings.Common.GameVersion);
    logging::write("Engine: {} (revision {})", FO_ENGINE_VERSION, FO_ENGINE_REVISION);

    // Disable message box on exception if headless window is used
    if (is_enum_set(flags, AppInitFlags::ShowMessageOnException) && settings.Render.HeadlessWindow) {
        SetupExceptionCallback(false);
    }

    // Switch logging to a dedicated worker thread once the user setting is known
    if (settings.Common.AsyncLogWrite) {
        logging::set_async_writing(true);
    }

    // Diagnostic self-test: with logging, the exception callback and the async-log mode all live, verify
    // that crash diagnostics reach the log for the crash class named by FO_SELFTEST_CRASH. Inert otherwise
    DiagnosticSelfTest::RunIfRequested();

    // Project-side early init (before App frontend, after settings + exception/log callbacks). The hook may
    // change Common.NonInteractive and may fail, so lend it the live settings and retire the borrow on every exit
    {
        ErrorMessagePolicy->InitializingSettings = &settings;
        scope_exit sync_non_interactive {[&settings]() noexcept {
            ErrorMessagePolicy->NonInteractive = settings.Common.NonInteractive;
            ErrorMessagePolicy->InitializingSettings = nullptr;
        }};

        ApplicationInitHook(flags, settings);
    }

    // Prebake resources. Several processes launched from one checkout would each re-validate every pack for
    // minutes, so a launcher that has just baked may start them on the existing output instead
    if (!settings.Common.Packaged && is_enum_set(flags, AppInitFlags::PrebakeResources)) {
        if (settings.Baking.PrebakeOnStartup) {
            logging::write("Prebake resources");
            PrebakeResources(settings);
        }
        else {
            logging::write("Prebake skipped by Baking.PrebakeOnStartup, starting on the resources already baked");
        }
    }

    // Application frontend initialization
    App = safe_alloc::make_unique<Application>(std::move(settings), flags);

    // Request quit on bad alloc
    memory::set_bad_alloc_callback([]() FO_DEFERRED { GetApp()->RequestQuit(); });

    // Request quit on interrupt signals
    SetupSignals();

    // Set up clipboard events for web
    WebRelated::SetupClipboard();
}

static void SetupExceptionCallback(bool show_message_on_exception)
{
    bool bootstrap_packaged {IsPackaged()};

    exceptions::set_callback([show_message_on_exception, bootstrap_packaged](string_view message, const stack_trace::catched_data& st, bool fatal_error) FO_DEFERRED {
        if (IsNonInteractiveMode()) {
            Application::ShowErrorMessage(message, stack_trace::format(st), fatal_error);
            return;
        }

        logging::write_message(logging::type::error, message, &st);

        if (fatal_error) {
            logging::write_message(logging::type::error, "Shutdown!");

#if FO_WEB
            if (IsAppInitialized()) {
                GetApp()->RequestQuit();
            }
#endif
        }

        nptr<const GlobalSettings> settings {IsAppInitialized() ? &GetApp()->Settings : ErrorMessagePolicy->InitializingSettings};
        bool packaged {bootstrap_packaged};

        if (settings) {
            ptr<const GlobalSettings> current_settings {settings};
            packaged = current_settings->Common.Packaged;
        }

        if (show_message_on_exception || (!packaged && (fatal_error || !IsAppInitialized()))) {
            Application::ShowErrorMessage(message, stack_trace::format(st), fatal_error);
        }
    });
}

static auto LoadTestingAppSettings() -> GlobalSettings
{
    auto settings {GlobalSettings(false)};
    settings.SetupDefaultSettings();
    settings.ApplyAutoSettings(PackagedBuildName);
    return settings;
}

auto LoadAppSettings(CommandLineArgs args, AppInitFlags flags) -> GlobalSettings
{
    FO_TRACE_ZONE(Core);

    auto settings {GlobalSettings(false)};
    bool packaged {IsPackaged()};
    string writable_root {ResolveWritableRoot(args)};
    string resources_dir {};

    if (!packaged) {
        // Apply config
        string config_to_apply {};
        string config_to_apply_dir {};
        bool auto_find_config {false};

        for (const CommandLineSetting& setting : args.GetSettings(CommandLineSettings::ApplyConfig)) {
            if (!setting.Value) {
                throw AppInitException("Config name not provided");
            }

            config_to_apply = strex(*setting.Value).trim().extract_file_name();
            config_to_apply_dir = strex(*setting.Value).trim().extract_dir();
        }

        if (config_to_apply.empty()) {
            auto_find_config = true;
            auto dir {std::filesystem::current_path()};

            while (true) {
                string config_path {fs::path_to_string(dir / FO_MAIN_CONFIG)};

                if (fs::exists(config_path) && !fs::is_dir(config_path)) {
                    config_to_apply = FO_MAIN_CONFIG;
                    config_to_apply_dir = fs::path_to_string(dir);
                    break;
                }
                else {
                    if (dir.has_parent_path() && dir.parent_path() != dir) {
                        dir = dir.parent_path();
                    }
                    else {
                        throw AppInitException("Config file not found", FO_MAIN_CONFIG);
                    }
                }
            }
        }

        logging::write("Apply config {}", strex(config_to_apply_dir).combine_path(config_to_apply));
        settings.SetupConfigAtPath(config_to_apply, config_to_apply_dir);

        // Apply sub config
        vector<string> sub_configs_to_apply {};

        for (const CommandLineSetting& setting : args.GetSettings(CommandLineSettings::ApplySubConfig)) {
            if (!setting.Value) {
                throw AppInitException("Sub config name not provided");
            }

            sub_configs_to_apply.emplace_back(strex(*setting.Value).trim());
        }

        if (auto_find_config && sub_configs_to_apply.empty()) {
            sub_configs_to_apply.emplace_back(settings.Common.UnpackagedSubConfig);
        }

        for (const auto& sub_config_name : sub_configs_to_apply) {
            if (!sub_config_name.empty() && sub_config_name != "NONE") {
                logging::write("Apply sub config {}", sub_config_name);
                settings.ApplySubConfigSection(sub_config_name);
            }
        }
    }
    else {
        auto sub_configs {args.GetSettings(CommandLineSettings::ApplySubConfig)};
        FO_VERIFY_AND_CONTINUE(sub_configs.empty(), "Sub configs are not available in a packaged build", sub_configs.front().Value.value_or(string_view {}));

        resources_dir = ReadInternalConfigValue(SettingNames::Common_ResourcesPath);
        auto resolved_dir {ResolveSettingValue(SettingNames::Common_ResourcesPath, resources_dir, "", false)};

        if (resolved_dir) {
            resources_dir = std::move(*resolved_dir);
        }

        resources_dir = ResolveAppSettingDirectory(args, SettingNames::Common_ResourcesPath, std::move(resources_dir));

        bool standalone_client {is_enum_set(flags, AppInitFlags::StandaloneClient)};
        FileSystem metadata_source {};

        if (standalone_client) {
            AddClientPackSource(metadata_source, resources_dir, writable_root, METADATA_PACK_NAME, true);
        }
        else {
            metadata_source.AddPackSource(true, resources_dir, METADATA_PACK_NAME, true);
        }

        auto content {ReadResourceConfig(metadata_source, false)};

        // The new runtime can start before its resource update, so incompatible client profiles wait for synchronization
        if (standalone_client && content && !strvex(*content).starts_with("#" FO_COMPATIBILITY_VERSION "\n")) {
            content.reset();
        }

        if (content) {
            settings.SetupResourceConfig(*content);
            logging::write("Resource configuration loaded before application initialization");
        }
        else {
            settings.SetupInternalConfig();
        }
    }

    settings.ApplyWritableRoot(writable_root);

    if (!packaged) {
        resources_dir = ResolveAppSettingDirectory(args, SettingNames::Common_ResourcesPath, settings.Common.ResourcesPath);
    }

    if (!settings.Common.UserWritablePath.empty()) {
        (void)fs::create_directories(GetClientWritableResourceDir(writable_root, resources_dir));
        logging::write("Writable data path: {}", settings.Common.UserWritablePath);
    }

    ApplyAppSettingsOverrides(settings, args);
    return settings;
}

void ApplyAppSettingsOverrides(GlobalSettings& settings, CommandLineArgs args)
{
    FO_TRACE_ZONE(Core);

    string cache_resources {ResolveAppSettingDirectory(args, SettingNames::Common_CachePath, settings.Common.CachePath)};
    string cache_dir {fs::make_writable_path(settings.Common.UserWritablePath, cache_resources)};

    if (!settings.Common.UserWritablePath.empty()) {
        (void)fs::create_directories(cache_dir);
    }

    if (fs::is_dir(cache_dir)) {
        auto cache {CacheStorage(cache_dir)};

        if (cache.HasEntry(LOCAL_CONFIG_NAME)) {
            auto config {ConfigFile(cache.GetString(LOCAL_CONFIG_NAME))};
            settings.ApplyConfigFile(config, "");
        }
    }

    settings.ApplyCommandLine(args);
    settings.ApplyAutoSettings(PackagedBuildName);
}

auto ResolveWritableRoot(CommandLineArgs args) -> string
{
    FO_TRACE_ZONE(FileSystem);

    // Deliberately settings-free: the log, the cache and the local config all live under this root, so
    // nothing that is read from disk may decide where it is. Only the command line and the marker do
    string root {};

    for (const CommandLineSetting& setting : args.GetSettings(SettingNames::Common_UserWritablePath)) {
        if (!setting.Value) {
            continue;
        }

        string_view value {strvex(*setting.Value).trim().strv()};

        if (!value.empty() && !CommandLineArgs::IsOption(value)) {
            root = string(value);
            break;
        }
    }

    if (root.empty()) {
        auto exe_path {platform::get_exe_path()};

        if (!exe_path.has_value() || !fs::exists(strex(*exe_path).extract_dir().combine_path(INSTALLED_MARKER_NAME).str())) {
            return "";
        }

        root = "*";
    }

    // An explicit "*" asks for the same per-user directory the marker selects, which is how a launcher
    // requests it without knowing the per-OS path
    if (root == "*") {
        string base {platform::get_user_data_base()};

        if (base.empty()) {
            logging::write(logging::type::warning, "Installed layout requested but no user data dir found; writing to the working directory");
            return "";
        }

        // Named after the project rather than Common.GameName, which is a config value and therefore
        // unknown this early - and a window title must not be able to move a player's data
        root = strex(base).combine_path(FO_NICE_NAME).str();
    }

    root = fs::resolve_path(root);

    if (!fs::create_directories(root)) {
        logging::write(logging::type::warning, "Can't create writable path '{}'; writing to the working directory", root);
        return "";
    }

    return root;
}

static auto IsPackaged() -> bool
{
    return !PackagedBuildName.empty();
}

static auto ReadPackagedBuildName() -> string
{
    string raw {strex().assignVolatile(PACKAGED_BUILD_NAME, sizeof(PACKAGED_BUILD_NAME)).str()};

    if (raw.find("NotPackaged") != string::npos) {
        return {};
    }

    if (auto null_pos {raw.find('\0')}; null_pos != string::npos) {
        raw.resize(null_pos);
    }

    return raw;
}

static auto ResolveAppSettingDirectory(const CommandLineArgs& args, string_view name, string directory) -> string
{
    for (const CommandLineSetting& setting : args.GetSettings(name)) {
        string raw_value {strex(setting.Value.value_or("1")).trim().str()};
        bool append {!raw_value.empty() && raw_value.front() == '+'};
        string_view value {append ? string_view(raw_value).substr(1) : raw_value};
        auto resolved {ResolveSettingValue(name, raw_value, "", false)};

        if (resolved) {
            value = *resolved;
        }

        if (!append) {
            directory.clear();
        }
        else if (!directory.empty()) {
            directory += " ";
        }

        directory += value;
    }

    return directory;
}

static void PrebakeResources(BakingSettings& settings)
{
    FO_TRACE_ZONE(Baking);

    using BakeResourcesFunc = bool (*)(void*);
    auto bake_resources {platform::get_func_addr<BakeResourcesFunc>(nullptr, "FO_BakeResources")};

    strex lib_name {strex("{}_BakerLib", FO_DEV_NAME)};

    if (bake_resources == nullptr) {
        auto exe_path {platform::get_exe_path()};
        string lib_path {strex(exe_path.value_or("")).extract_dir().combine_path(lib_name).str()};
        // Never unloaded: like every engine library it carries statically linked runtimes whose process-wide
        // callbacks cannot be withdrawn, so unmapping it would leave them pointing at nothing
        nptr<void> baker_dll {platform::load_pinned_module(lib_path)};

        if (baker_dll) {
            bake_resources = platform::get_func_addr<BakeResourcesFunc>(baker_dll, "FO_BakeResources");
        }
    }

    if (bake_resources != nullptr) {
        bool result {false};
        Application::ShowProgressWindow("Baking resources... Please wait", [&] { result = bake_resources(&settings); });

        if (!result) {
            throw AppInitException("Resource baking failed. See baker log for details");
        }
    }
    else {
        if (fs::exists(settings.Baking.BakeOutput) && fs::is_dir(settings.Baking.BakeOutput)) {
            if (!settings.Baking.IgnoreMissingBakerWarning) {
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
    if (auto exe_path {platform::get_exe_path()}) {
        return strex("{}.log", strex(exe_path.value()).extract_file_name().erase_file_extension());
    }

    return strex("{}.log", FO_DEV_NAME);
}

auto GetExePreviousLogFileName() -> string
{
    return strex("{}.prev.log", strex(GetExeLogFileName()).erase_file_extension());
}

#if FO_LINUX || FO_MAC
// Written from a signal handler, where a lock-free store is the only async-signal-safe move; global
// because a signal targets the process, not an engine instance
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
#if FO_LINUX || FO_MAC
    return QuitSignalReceived.load(std::memory_order_acquire);
#elif FO_WINDOWS
    return winapi::is_console_interrupt_received();
#else
    return false;
#endif
}

static void SetupSignals()
{
#if FO_LINUX || FO_MAC
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
#elif FO_WINDOWS
    if (!winapi::install_console_interrupt_handler()) {
        logging::write(logging::type::warning, "Unable to install the console interrupt handler");
    }
#endif
}

FO_END_NAMESPACE
