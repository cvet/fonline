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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "FileSystem.h"
#include "Version-Include.h"

FO_BEGIN_NAMESPACE();

raw_ptr<Application> App;

static void SetupExceptionCallback(bool show_message_on_exception);
static void SetupLogging(bool disable_log_tags);
static auto LoadSettings(int32 argc, char** argv) -> GlobalSettings;
static void PrebakeResources(BakingSettings& settings);
static void SetupSignals();
static void SetupWebClipboard();

void InitApp(int32 argc, char** argv, AppInitFlags flags)
{
    FO_STACK_TRACE_ENTRY();

    // Ensure that we call init only once
    static std::once_flag once;
    bool first_call = false;
    std::call_once(once, [&first_call] { first_call = true; });
    FO_STRONG_ASSERT(first_call);

    // Fork the process if requested
    if (argc > 0 && std::any_of(argv, argv + argc, [](const char* arg) { return string_view(arg) == "--fork"; })) {
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
    SetupLogging(IsEnumSet(flags, AppInitFlags::DisableLogTags));
    WriteLog("Starting {}", FO_NICE_NAME);

    // Load settings
    auto settings = LoadSettings(argc, argv);
    WriteLog("Version {}", settings.GameVersion);

    // Prebake resources
    if (!IsPackaged() && IsEnumSet(flags, AppInitFlags::PrebakeResources)) {
        PrebakeResources(settings);
    }

    // Application frontend initialization
    App = SafeAlloc::MakeRaw<Application>(std::move(settings), flags);

    // Request quit on bad alloc
    SetBadAllocCallback([] { App->RequestQuit(); });

    // Request quit on interrupt signals
    SetupSignals();

    // Set up clipboard events for web
    SetupWebClipboard();

    // Init mouse pos
    App->Settings.MousePos = App->Input.GetMousePosition();
}

static void SetupExceptionCallback(bool show_message_on_exception)
{
    FO_STACK_TRACE_ENTRY();

    SetExceptionCallback([show_message_on_exception](string_view message, string_view traceback, bool fatal_error) {
        WriteLog(LogType::Error, "{}\n{}", message, traceback);

        if (fatal_error) {
            WriteLog(LogType::Error, "Shutdown!");
        }

        if (show_message_on_exception || (!IsPackaged() && (fatal_error || !App))) {
            Application::ShowErrorMessage(message, traceback, fatal_error);
        }
    });
}

static void SetupLogging(bool disable_log_tags)
{
    FO_STACK_TRACE_ENTRY();

#if !FO_WEB
    if (const auto exe_path = Platform::GetExePath()) {
        LogToFile(strex("{}.log", strex(exe_path.value()).extract_file_name().erase_file_extension()));
    }
    else {
        LogToFile(strex("{}.log", FO_DEV_NAME));
    }
#endif

    if (disable_log_tags) {
        LogDisableTags();
    }
}

auto LoadSettings(int32 argc, char** argv) -> GlobalSettings
{
    FO_STACK_TRACE_ENTRY();

    auto settings = GlobalSettings(false);

    if (argc == -1) {
        return settings; // Unit testing
    }

    if (!IsPackaged()) {
        // Apply config
        string config_to_apply;
        string config_to_apply_dir;
        bool auto_find_config = false;

        for (int32 i = 0; i < argc; i++) {
            const string_view arg = strex(argv[i]).trim().strv();

            if (arg == "-ApplyConfig" || arg == "--ApplyConfig") {
                if (i == argc - 1 || *argv[i + 1] == '-') {
                    throw AppInitException("Config name not provided");
                }

                config_to_apply = strex(argv[i + 1]).trim().extract_file_name();
                config_to_apply_dir = strex(argv[i + 1]).trim().extract_dir();
            }
        }

        if (config_to_apply.empty()) {
            auto_find_config = true;
            auto dir = std::filesystem::current_path();

            while (true) {
                if (std::filesystem::exists(dir / FO_MAIN_CONFIG) && !std::filesystem::is_directory(dir / FO_MAIN_CONFIG)) {
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

        for (int32 i = 0; i < argc; i++) {
            const string_view arg = strex(argv[i]).trim().strv();

            if (arg == "-ApplySubConfig" || arg == "--ApplySubConfig") {
                if (i == argc - 1 || *argv[i + 1] == '-') {
                    throw AppInitException("Sub config name not provided");
                }

                sub_configs_to_apply.emplace_back(strex(argv[i + 1]).trim());
            }
        }

        if (auto_find_config && sub_configs_to_apply.empty()) {
            sub_configs_to_apply.emplace_back(settings.UnpackagedSubConfig);
        }

        if (auto_find_config && Platform::IsShiftDown()) {
            const auto& sub_configs = settings.GetSubConfigs();
            vector<string> sub_config_names;
            set<int32> selected_sub_configs;

            for (size_t i = 0; i < sub_configs.size(); i++) {
                const auto& sub_config = sub_configs[i];
                sub_config_names.emplace_back(sub_config.Name);

                if (std::ranges::find(sub_configs_to_apply, sub_config.Name) != sub_configs_to_apply.end()) {
                    selected_sub_configs.emplace(numeric_cast<int32>(i));
                }
            }

            sub_config_names.emplace_back("RUN AS PACKAGED");
            sub_configs_to_apply.clear();
            Application::ChooseOptionsWindow("Choose sub config(s) to apply:", sub_config_names, selected_sub_configs);

            for (const auto i : selected_sub_configs) {
                if (i == numeric_cast<int32>(sub_config_names.size()) - 1) {
                    ForcePackaged();
                }
                else {
                    sub_configs_to_apply.emplace_back(sub_config_names[i]);
                }
            }
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

    if (DiskFileSystem::IsDir(settings.CacheResources)) {
        const auto cache = CacheStorage(settings.CacheResources);

        if (cache.HasEntry(LOCAL_CONFIG_NAME)) {
            auto config = ConfigFile(LOCAL_CONFIG_NAME, cache.GetString(LOCAL_CONFIG_NAME));
            settings.ApplyConfigFile(config, "");
        }
    }

    settings.ApplyCommandLine(argc, argv);
    settings.ApplyAutoSettings();
    return settings;
}

static void PrebakeResources(BakingSettings& settings)
{
    FO_STACK_TRACE_ENTRY();

    using BakeResourcesFunc = bool (*)(void*);
    auto bake_resources = Platform::GetFuncAddr<BakeResourcesFunc>(nullptr, "FO_BakeResources");

    void* baker_dll = nullptr;
    auto unload_baker_dll = ScopeCallback([&]() noexcept { Platform::UnloadModule(baker_dll); });

    const auto lib_name = strex("{}_BakerLib", FO_DEV_NAME);

    if (bake_resources == nullptr) {
        const auto exe_path = Platform::GetExePath();
        const auto lib_path = strex(exe_path.value_or("")).extract_dir().combine_path(lib_name).str();
        baker_dll = Platform::LoadModule(lib_path);

        if (baker_dll != nullptr) {
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
        if (std::filesystem::exists(settings.BakeOutput) && std::filesystem::is_directory(settings.BakeOutput)) {
            Application::ShowErrorMessage(strex("Warning! {} not found. Resources may be out of date", lib_name), "", false);
        }
        else {
            throw AppInitException("Baker not found. Unable to bake resources");
        }
    }
}

#if FO_LINUX || FO_MAC
static void SignalHandler(int32 sig)
{
    std::signal(sig, SignalHandler);
    App->RequestQuit();
}
#endif

static void SetupSignals()
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
#endif
}

#if FO_WEB
FO_END_NAMESPACE();
extern "C"
{
    EMSCRIPTEN_KEEPALIVE const char* Emscripten_ClipboardGet()
    {
        return FO_NAMESPACE App->Input.GetClipboardText().c_str();
    }
    EMSCRIPTEN_KEEPALIVE void Emscripten_ClipboardSet(const char* text)
    {
        FO_NAMESPACE App->Input.SetClipboardText(text);
    }
}
FO_BEGIN_NAMESPACE();
#endif

static void SetupWebClipboard()
{
    FO_STACK_TRACE_ENTRY();

#if FO_WEB
    // clang-format off
    MAIN_THREAD_EM_ASM({
        var canvas = document.querySelector(UTF8ToString($0));
        if (canvas) {
            canvas.addEventListener("copy", (event) => {
                const text = _Emscripten_ClipboardGet();
                event.clipboardData.setData("text/plain", UTF8ToString(text));
                event.preventDefault();
            });
            canvas.addEventListener("paste", (event) => {
                const text = event.clipboardData.getData('text/plain');
                _Emscripten_ClipboardSet(text);
                event.preventDefault();
            });
        }
    }, WEB_CANVAS_ID.c_str());
    // clang-format on
#endif
}

FO_END_NAMESPACE();
