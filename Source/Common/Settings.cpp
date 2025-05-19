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

#include "Settings.h"
#include "AnyData.h"
#include "CacheStorage.h"
#include "ConfigFile.h"
#include "DiskFileSystem.h"
#include "ImGuiStuff.h"
#include "Log.h"
#include "StringUtils.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

FO_BEGIN_NAMESPACE();

template<typename T>
static void SetEntry(T& entry, string_view value, bool append)
{
    FO_STACK_TRACE_ENTRY();

    if (!append) {
        entry = {};
    }

    if constexpr (std::is_same_v<T, string>) {
        if (append && !entry.empty()) {
            entry += " ";
        }

        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::String);
        entry += any_value.AsString();
    }
    else if constexpr (std::is_same_v<T, bool>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Bool);
        entry |= any_value.AsBool();
    }
    else if constexpr (std::is_floating_point_v<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Double);
        entry += static_cast<float>(any_value.AsDouble());
    }
    else if constexpr (std::is_enum_v<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Int64);
        entry = static_cast<T>(static_cast<int>(entry) | any_value.AsInt64());
    }
    else if constexpr (is_strong_type_v<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Int64);
        entry = T {static_cast<typename T::underlying_type>(any_value.AsInt64())};
    }
    else if constexpr (is_valid_pod_type_v<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::String);
        entry = parse_from_string<T>(any_value.AsString());
    }
    else {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Int64);
        entry += static_cast<T>(any_value.AsInt64());
    }
}

template<typename T>
static void SetEntry(vector<T>& entry, string_view value, bool append)
{
    FO_STACK_TRACE_ENTRY();

    if (!append) {
        entry.clear();
    }

    if constexpr (std::is_same_v<T, string>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::String);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(arr_entry.AsString());
        }
    }
    else if constexpr (std::is_same_v<T, bool>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Bool);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(arr_entry.AsBool());
        }
    }
    else if constexpr (std::is_floating_point_v<T>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Double);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(static_cast<float>(arr_entry.AsDouble()));
        }
    }
    else if constexpr (std::is_enum_v<T>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Int64);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(static_cast<std::underlying_type_t<T>>(arr_entry.AsInt64()));
        }
    }
    else {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Int64);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(static_cast<T>(arr_entry.AsInt64()));
        }
    }
}

template<typename T>
static void DrawEntry(string_view name, const T& entry)
{
    FO_STACK_TRACE_ENTRY();

    ImGui::TextUnformatted(strex("{}: {}", name, entry).c_str());
}

template<typename T>
static void DrawEditableEntry(string_view name, T& entry)
{
    FO_STACK_TRACE_ENTRY();

    // Todo: improve editable entries
    DrawEntry(name, entry);
}

GlobalSettings::GlobalSettings(int argc, char** argv)
{
    FO_STACK_TRACE_ENTRY();

    // Unit tests
    if (argc == -1) {
        return;
    }

    // Debugging config
    if (IsRunInDebugger()) {
        WriteLog("Apply debugging config {}", FO_DEBUGGING_MAIN_CONFIG);

        const string config_name = strex(FO_DEBUGGING_MAIN_CONFIG).extractFileName();
        const string config_dir = strex(FO_DEBUGGING_MAIN_CONFIG).extractDir();
        ApplyConfigPath(config_name, config_dir);
    }

    // Injected config
    {
        static volatile constexpr char INTERNAL_CONFIG[10044] = {"###InternalConfig###"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                                                                 "###InternalConfigEnd###"};

        const auto volatile_char_to_string = [](const volatile char* str, size_t len) -> string {
            string result;
            result.resize(len);
            for (size_t i = 0; i < len; i++) {
                result[i] = str[i];
            }
            return result;
        };

        const auto config_str = volatile_char_to_string(INTERNAL_CONFIG, sizeof(INTERNAL_CONFIG));

        if (!strex(config_str).startsWith("###InternalConfig###")) {
            auto config = ConfigFile("InternalConfig.fomain", config_str);
            ApplyConfigFile(config, "");
        }
    }

    // High priority command line settings before local config and other command line settings
    unordered_set<string> high_priority_cmd_line_settings = {
        "ApplyConfig",
        "ServerResources",
        "ClientResources",
        "ApplySubConfig",
        "DebuggingSubConfig",
    };

    for (int i = 0; i < argc; i++) {
        if (i == 0 && argv[0][0] != '-') {
            continue;
        }

        if (argv[i][0] == '-') {
            const string key = strex("{}", argv[i]).trim().str().substr(argv[i][1] == '-' ? 2 : 1);

            if (high_priority_cmd_line_settings.count(key) != 0) {
                const auto value = i < argc - 1 && argv[i + 1][0] != '-' ? strex("{}", argv[i + 1]).trim().str() : "1";

                if (key == "ApplyConfig") {
                    WriteLog("Apply config {}", value);

                    const string config_name = strex(value).extractFileName();
                    const string config_dir = strex(value).extractDir();
                    SetValue(key, config_name, config_dir);
                    ApplySettingsConfig(config_dir);
                }
                else {
                    SetValue(key, value);
                }
            }
        }
    }

    if (!_someConfigApplied) {
        throw SettingsException("No config applied");
    }

    // Local config
    if (const string cache_path = strex(ClientResources).combinePath("Cache.fobin"); DiskFileSystem::IsExists(cache_path)) {
        const auto cache = CacheStorage(cache_path);

        if (cache.HasEntry(LOCAL_CONFIG_NAME)) {
            WriteLog("Load local config {}", LOCAL_CONFIG_NAME);

            auto config = ConfigFile(LOCAL_CONFIG_NAME, cache.GetString(LOCAL_CONFIG_NAME));
            ApplyConfigFile(config, ClientResources);
        }
    }

    // Apply sub configs
    ApplySubConfigSection(ApplySubConfig);

    if (IsRunInDebugger()) {
        ApplySubConfigSection(DebuggingSubConfig);
    }

    // Command line config
    for (int i = 0; i < argc; i++) {
        if (i == 0 && argv[0][0] != '-') {
            continue;
        }

        const_cast<vector<string>&>(CommandLineArgs).emplace_back(argv[i]);
        const_cast<string&>(CommandLine) += argv[i];

        if (i < argc - 1) {
            const_cast<string&>(CommandLine) += " ";
        }

        if (argv[i][0] == '-') {
            const auto key = strex("{}", argv[i]).trim().str().substr(argv[i][1] == '-' ? 2 : 1);

            if (high_priority_cmd_line_settings.count(key) == 0) {
                const auto value = i < argc - 1 && argv[i + 1][0] != '-' ? strex("{}", argv[i + 1]).trim().str() : "1";

                WriteLog("Set {} to {}", key, value);
                SetValue(key, value);
            }
        }
    }

    // Auto settings
    ApplyAutoSettings();

    WriteLog("Version {}", GameVersion);
}

GlobalSettings::GlobalSettings(string_view config_path, string_view sub_config)
{
    FO_STACK_TRACE_ENTRY();

    _bakingMode = true;

    const string config_name = strex(config_path).extractFileName();
    const string config_dir = strex(config_path).extractDir();
    ApplyConfigPath(config_name, config_dir);

    const_cast<string&>(ApplySubConfig) = "";
    const_cast<string&>(DebuggingSubConfig) = "";

    ApplySubConfigSection(sub_config);

    const_cast<vector<string>&>(AppliedConfigs).clear();

    // Auto settings
    _appliedSettings.emplace("ApplyConfig");
    _appliedSettings.emplace("AppliedConfigs");
    _appliedSettings.emplace("ApplySubConfig");
    _appliedSettings.emplace("DebuggingSubConfig");
    _appliedSettings.emplace("CommandLine");
    _appliedSettings.emplace("CommandLineArgs");
    _appliedSettings.emplace("WebBuild");
    _appliedSettings.emplace("WindowsBuild");
    _appliedSettings.emplace("LinuxBuild");
    _appliedSettings.emplace("MacOsBuild");
    _appliedSettings.emplace("AndroidBuild");
    _appliedSettings.emplace("IOsBuild");
    _appliedSettings.emplace("DesktopBuild");
    _appliedSettings.emplace("TabletBuild");
    _appliedSettings.emplace("MapHexagonal");
    _appliedSettings.emplace("MapSquare");
    _appliedSettings.emplace("MapDirCount");
    _appliedSettings.emplace("DebugBuild");
    _appliedSettings.emplace("RenderDebug");
    _appliedSettings.emplace("MonitorWidth");
    _appliedSettings.emplace("MonitorHeight");
    _appliedSettings.emplace("ClientResourceEntries");
    _appliedSettings.emplace("MapperResourceEntries");
    _appliedSettings.emplace("ServerResourceEntries");
    _appliedSettings.emplace("MousePos");
    _appliedSettings.emplace("ScreenOffset");
    _appliedSettings.emplace("DummyIntVec");
    _appliedSettings.emplace("Ping");
    _appliedSettings.emplace("ScrollMouseUp");
    _appliedSettings.emplace("ScrollMouseDown");
    _appliedSettings.emplace("ScrollMouseLeft");
    _appliedSettings.emplace("ScrollMouseRight");
    _appliedSettings.emplace("ScrollKeybUp");
    _appliedSettings.emplace("ScrollKeybDown");
    _appliedSettings.emplace("ScrollKeybLeft");
    _appliedSettings.emplace("ScrollKeybRight");
}

void GlobalSettings::ApplyConfigPath(string_view config_name, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    if (config_name.empty()) {
        return;
    }

    const string config_path = strex(config_dir).combinePath(config_name);

    if (auto settings_file = DiskFileSystem::OpenFile(config_path, false)) {
        const_cast<vector<string>&>(AppliedConfigs).emplace_back(config_path);

        string settings_content;
        settings_content.resize(settings_file.GetSize());
        settings_file.Read(settings_content.data(), settings_content.size());

        auto config = ConfigFile("Config.fomain", settings_content);
        ApplyConfigFile(config, config_dir);
    }
    else {
        if (!BreakIntoDebugger()) {
            throw SettingsException("Config not found", config_path);
        }
    }
}

void GlobalSettings::ApplyConfigFile(ConfigFile& config, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (auto&& [key, value] : config.GetSection("")) {
        SetValue(key, value, config_dir);
    }

    AddResourcePacks(config.GetSections("ResourcePack"), config_dir);
    AddSubConfigs(config.GetSections("SubConfig"), config_dir);

    ApplySettingsConfig(config_dir);

    _someConfigApplied = true;
}

void GlobalSettings::ApplySettingsConfig(string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    if (ApplyConfig.empty()) {
        return;
    }

    const auto configs_to_apply = copy(ApplyConfig);
    const_cast<vector<string>&>(ApplyConfig).clear();

    for (const auto& config_to_apply : configs_to_apply) {
        const string config_name = strex(config_to_apply).extractFileName();
        const string config_dir2 = strex(config_dir).combinePath(strex(config_to_apply).extractDir());
        ApplyConfigPath(config_name, config_dir2);
    }
}

void GlobalSettings::AddResourcePacks(const vector<map<string, string>*>& res_packs, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto* res_pack : res_packs) {
        const auto get_map_value = [&](string_view key) -> string {
            const auto it = res_pack->find(key);
            return it != res_pack->end() ? it->second : string();
        };

        ResourcePackInfo pack_info;

        if (auto name = get_map_value("Name"); !name.empty()) {
            pack_info.Name = std::move(name);
        }
        else {
            throw SettingsException("Resource pack name not specifed");
        }

        if (auto server_only = get_map_value("ServerOnly"); !server_only.empty()) {
            pack_info.ServerOnly = strex(server_only).toBool();
        }
        if (auto client_only = get_map_value("ClientOnly"); !client_only.empty()) {
            pack_info.ClientOnly = strex(client_only).toBool();
        }
        if (auto mapper_only = get_map_value("MapperOnly"); !mapper_only.empty()) {
            pack_info.MapperOnly = strex(mapper_only).toBool();
        }
        if (static_cast<int>(pack_info.ServerOnly) + static_cast<int>(pack_info.ClientOnly) + static_cast<int>(pack_info.MapperOnly) > 1) {
            throw SettingsException("Resource pack can be common or server, client or mapper only");
        }

        if (auto inpurt_dir = get_map_value("InputDir"); !inpurt_dir.empty()) {
            for (auto& dir : strex(inpurt_dir).split(' ')) {
                dir = strex(config_dir).combinePath(dir);
                pack_info.InputDir.emplace_back(std::move(dir));
            }
        }
        if (auto recursive_input = get_map_value("RecursiveInput"); !recursive_input.empty()) {
            pack_info.RecursiveInput = strex(recursive_input).toBool();
        }
        if (auto bake_order = get_map_value("BakeOrder"); !bake_order.empty()) {
            pack_info.BakeOrder = strex(bake_order).toInt();
        }

        if (pack_info.Name != "Embedded") {
            if (pack_info.ServerOnly) {
                const_cast<vector<string>&>(ServerResourceEntries).emplace_back(pack_info.Name);
            }
            else if (pack_info.ClientOnly) {
                const_cast<vector<string>&>(ClientResourceEntries).emplace_back(pack_info.Name);
            }
            else if (pack_info.MapperOnly) {
                const_cast<vector<string>&>(MapperResourceEntries).emplace_back(pack_info.Name);
            }
            else {
                const_cast<vector<string>&>(ServerResourceEntries).emplace_back(pack_info.Name);
                const_cast<vector<string>&>(ClientResourceEntries).emplace_back(pack_info.Name);
            }
        }

        _resourcePacks.emplace_back(std::move(pack_info));
    }
}

void GlobalSettings::AddSubConfigs(const vector<map<string, string>*>& sub_configs, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto* sub_config : sub_configs) {
        const auto get_map_value = [&](string_view key) -> string {
            const auto it = sub_config->find(key);
            return it != sub_config->end() ? it->second : string();
        };

        SubConfigInfo config_info;
        config_info.ConfigDir = config_dir;

        if (auto name = get_map_value("Name"); !name.empty()) {
            config_info.Name = std::move(name);
        }
        else {
            throw SettingsException("Sub config name not specifed");
        }

        if (auto parent = get_map_value("Parent"); !parent.empty()) {
            const auto find_predicate = [&](const SubConfigInfo& cfg) { return cfg.Name == parent; };
            const auto it = std::find_if(_subConfigs.begin(), _subConfigs.end(), find_predicate);

            if (it == _subConfigs.end()) {
                throw SettingsException("Parent sub config not found", parent);
            }

            config_info.Settings = it->Settings;
        }

        for (auto&& [key, value] : *sub_config) {
            if (key != "Name" && key != "Parent") {
                config_info.Settings[key] = value;
            }
        }

        _subConfigs.emplace_back(std::move(config_info));
    }
}

void GlobalSettings::ApplyAutoSettings()
{
    FO_STACK_TRACE_ENTRY();

#if FO_WEB
    const_cast<bool&>(WebBuild) = true;
#else
    const_cast<bool&>(WebBuild) = false;
#endif
#if FO_WINDOWS
    const_cast<bool&>(WindowsBuild) = true;
#else
    const_cast<bool&>(WindowsBuild) = false;
#endif
#if FO_LINUX
    const_cast<bool&>(LinuxBuild) = true;
#else
    const_cast<bool&>(LinuxBuild) = false;
#endif
#if FO_MAC
    const_cast<bool&>(MacOsBuild) = true;
#else
    const_cast<bool&>(MacOsBuild) = false;
#endif
#if FO_ANDROID
    const_cast<bool&>(AndroidBuild) = true;
#else
    const_cast<bool&>(AndroidBuild) = false;
#endif
#if FO_IOS
    const_cast<bool&>(IOsBuild) = true;
#else
    const_cast<bool&>(IOsBuild) = false;
#endif
    const_cast<bool&>(DesktopBuild) = WindowsBuild || LinuxBuild || MacOsBuild;
    const_cast<bool&>(TabletBuild) = AndroidBuild || IOsBuild;

#if FO_WINDOWS && !FO_UWP
    if (::GetSystemMetrics(SM_TABLETPC) != 0) {
        const_cast<bool&>(DesktopBuild) = false;
        const_cast<bool&>(TabletBuild) = true;
    }
#endif

    const_cast<bool&>(MapHexagonal) = GameSettings::HEXAGONAL_GEOMETRY;
    const_cast<bool&>(MapSquare) = GameSettings::SQUARE_GEOMETRY;
    const_cast<int&>(MapDirCount) = GameSettings::MAP_DIR_COUNT;

#if FO_DEBUG
    const_cast<bool&>(DebugBuild) = true;
    const_cast<bool&>(RenderDebug) = true;
#endif
}

void GlobalSettings::ApplySubConfigSection(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    if (name.empty() || name == "NONE") {
        return;
    }

    const auto find_predicate = [&](const SubConfigInfo& cfg) { return cfg.Name == name; };
    const auto it = std::find_if(_subConfigs.begin(), _subConfigs.end(), find_predicate);

    if (it == _subConfigs.end()) {
        throw SettingsException("Sub config not found", name);
    }

    for (auto&& [key, value] : it->Settings) {
        SetValue(key, value, it->ConfigDir);
    }
}

auto GlobalSettings::GetResourcePacks() const -> const_span<ResourcePackInfo>
{
    FO_STACK_TRACE_ENTRY();

    if (_resourcePacks.empty()) {
        throw SettingsException("No information about resource packs found");
    }

    return _resourcePacks;
}

auto GlobalSettings::GetCustomSetting(string_view name) const -> const string&
{
    const auto it = _customSettings.find(name);

    if (it != _customSettings.end()) {
        return it->second;
    }

    return _empty;
}

void GlobalSettings::SetCustomSetting(string_view name, string value)
{
    FO_STACK_TRACE_ENTRY();

    _customSettings[name] = std::move(value);
}

void GlobalSettings::SetValue(const string& setting_name, const string& setting_value, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    const bool append = !setting_value.empty() && setting_value[0] == '+';
    string_view value = append ? string_view(setting_value).substr(1) : setting_value;

    // Resolve environment variables and files
    string resolved_value;
    size_t prev_pos = 0;
    size_t pos = setting_value.find('$');

    if (pos != string::npos) {
        while (pos != string::npos) {
            const bool is_env = setting_value.compare(pos, "$ENV{"_len, "$ENV{") == 0;
            const bool is_file = setting_value.compare(pos, "$FILE{"_len, "$FILE{") == 0;
            const bool is_target_env = setting_value.compare(pos, "$TARGET_ENV{"_len, "$TARGET_ENV{") == 0;
            const bool is_target_file = setting_value.compare(pos, "$TARGET_FILE{"_len, "$TARGET_FILE{") == 0;
            const size_t len = is_env ? "$ENV{"_len : is_file ? "$FILE{"_len : is_target_env ? "$TARGET_ENV{"_len : "$TARGET_FILE{"_len;

            if (is_env || is_file || (!_bakingMode && (is_target_env || is_target_file))) {
                pos += len;
                size_t end_pos = setting_value.find('}', pos);

                if (end_pos != string::npos) {
                    const string name = setting_value.substr(pos, end_pos - pos);

                    if (is_env || is_target_env) {
                        const char* env = !name.empty() ? std::getenv(name.c_str()) : nullptr;

                        if (env != nullptr) {
                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos - len) + string(env);
                            end_pos++;
                        }
                        else {
                            WriteLog(LogType::Warning, "Environment variable {} for setting {} is not found", name, setting_name);
                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos) + name;
                        }
                    }
                    else {
                        const string file_path = strex(config_dir).combinePath(name);
                        auto file = DiskFileSystem::OpenFile(file_path, false);

                        if (file) {
                            string file_content;
                            file_content.resize(file.GetSize());
                            file.Read(file_content.data(), file_content.size());
                            file_content = strex(file_content).trim();

                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos - len) + string(file_content);
                            end_pos++;
                        }
                        else {
                            WriteLog(LogType::Warning, "File {} for setting {} is not found", file_path, setting_name);
                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos) + name;
                        }
                    }

                    prev_pos = end_pos;
                    pos = setting_value.find('$', end_pos);
                }
                else {
                    throw SettingsException("Not closed $ tag in settings", setting_name, setting_value);
                }
            }
            else {
                pos = setting_value.find('$', pos + 1);
            }
        }

        if (prev_pos != string::npos) {
            resolved_value += setting_value.substr(prev_pos);
        }

        value = resolved_value;
    }

#define SET_SETTING(sett) \
    SetEntry(sett, value, append); \
    break
#define FIXED_SETTING(type, name, ...) \
    case const_hash(#name): \
        SET_SETTING(const_cast<type&>(name))
#define VARIABLE_SETTING(type, name, ...) \
    case const_hash(#name): \
        SET_SETTING(name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()

    switch (const_hash(setting_name.c_str())) {
#include "Settings-Include.h"
    default:
        _customSettings[setting_name] = setting_value;
        break;
    }

#undef SET_SETTING

    if (_bakingMode) {
        _appliedSettings.emplace(setting_name);
    }
}

auto GlobalSettings::Save() const -> map<string, string>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_bakingMode);

    map<string, string> result;

    for (auto&& [key, value] : _customSettings) {
        if (_appliedSettings.count(key) != 0) {
            result[key] = value;
        }
    }

    const auto add_setting = [&](string_view name, const auto& value) {
        if (_appliedSettings.count(name) != 0) {
            result.emplace(name, strex("{}", value));
        }
    };

#define FIXED_SETTING(type, name, ...) add_setting(#name, name)
#define VARIABLE_SETTING(type, name, ...) add_setting(#name, name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings-Include.h"

    return result;
}

void GlobalSettings::Draw(bool editable)
{
    FO_STACK_TRACE_ENTRY();

#define FIXED_SETTING(type, name, ...) \
    if (editable) { \
        DrawEditableEntry(#name, const_cast<type&>(name)); \
    } \
    else { \
        DrawEntry(#name, name); \
    }
#define VARIABLE_SETTING(type, name, ...) \
    if (editable) { \
        DrawEditableEntry(#name, name); \
    } \
    else { \
        DrawEntry(#name, name); \
    }
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings-Include.h"
}

FO_END_NAMESPACE();
