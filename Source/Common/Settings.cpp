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

#include "Settings.h"
#include "AnyData.h"
#include "ConfigFile.h"
#include "ImGuiStuff.h"
#include "WinApi-Include.h"

FO_BEGIN_NAMESPACE

template<typename T>
static void SetEntry(T& entry, string_view value, bool append)
{
    FO_STACK_TRACE_ENTRY();

    if (!append) {
        entry = {};
    }

    if constexpr (std::same_as<T, string>) {
        if (append && !entry.empty()) {
            entry += " ";
        }

        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::String);
        entry += any_value.AsString();
    }
    else if constexpr (std::same_as<T, bool>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Bool);
        entry |= any_value.AsBool();
    }
    else if constexpr (std::floating_point<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Float64);
        entry += numeric_cast<float32_t>(any_value.AsDouble());
    }
    else if constexpr (std::is_enum_v<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Int64);
        entry = static_cast<T>(static_cast<int64_t>(entry) | any_value.AsInt64());
    }
    else if constexpr (some_strong_type<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Int64);
        entry = T {numeric_cast<typename T::underlying_type>(any_value.AsInt64())};
    }
    else if constexpr (some_property_plain_type<T>) {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::String);
        istringstream istr(any_value.AsString());
        istr >> entry;
    }
    else {
        const auto any_value = AnyData::ParseValue(string(value), false, false, AnyData::ValueType::Int64);
        entry += numeric_cast<T>(any_value.AsInt64());
    }
}

template<typename T>
static void SetEntry(vector<T>& entry, string_view value, bool append)
{
    FO_STACK_TRACE_ENTRY();

    if (!append) {
        entry.clear();
    }

    if constexpr (std::same_as<T, string>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::String);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(arr_entry.AsString());
        }
    }
    else if constexpr (std::same_as<T, bool>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Bool);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(arr_entry.AsBool());
        }
    }
    else if constexpr (std::floating_point<T>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Float64);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(numeric_cast<float32_t>(arr_entry.AsDouble()));
        }
    }
    else if constexpr (std::is_enum_v<T>) {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Int64);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(numeric_cast<std::underlying_type_t<T>>(arr_entry.AsInt64()));
        }
    }
    else {
        const auto arr_value = AnyData::ParseValue(string(value), false, true, AnyData::ValueType::Int64);
        const auto& arr = arr_value.AsArray();

        for (const auto& arr_entry : arr) {
            entry.emplace_back(numeric_cast<T>(arr_entry.AsInt64()));
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

    DrawEntry(name, entry);
}

GlobalSettings::GlobalSettings(bool baking_mode) :
    Common {static_cast<CommonSettings&>(*this)},
    Network {static_cast<NetworkSettings&>(*this)},
    ServerNetwork {static_cast<ServerNetworkSettings&>(*this)},
    ClientNetwork {static_cast<ClientNetworkSettings&>(*this)},
    Audio {static_cast<AudioSettings&>(*this)},
    View {static_cast<ViewSettings&>(*this)},
    Geometry {static_cast<GeometrySettings&>(*this)},
    Render {static_cast<RenderSettings&>(*this)},
    Timer {static_cast<TimerSettings&>(*this)},
    Baking {static_cast<BakingSettings&>(*this)},
    Critter {static_cast<CritterSettings&>(*this)},
    CritterView {static_cast<CritterViewSettings&>(*this)},
    Hex {static_cast<HexSettings&>(*this)},
    Platform {static_cast<PlatformSettings&>(*this)},
    Input {static_cast<InputSettings&>(*this)},
    Mapper {static_cast<MapperSettings&>(*this)},
    Client {static_cast<ClientSettings&>(*this)},
    Server {static_cast<ServerSettings&>(*this)},
    _bakingMode {baking_mode}
{
    FO_STACK_TRACE_ENTRY();

    if (_bakingMode) {
        // Auto settings
        _appliedSettings.emplace("ApplyConfig");
        _appliedSettings.emplace("ApplySubConfig");
        _appliedSettings.emplace("Common.UnpackagedSubConfig");
        _appliedSettings.emplace("Common.CommandLine");
        _appliedSettings.emplace("Common.CommandLineArgs");
        _appliedSettings.emplace("Common.GitBranch");
        _appliedSettings.emplace("Common.GitCommit");
        _appliedSettings.emplace("Network.CompatibilityVersion");
        _appliedSettings.emplace("Platform.WebBuild");
        _appliedSettings.emplace("Platform.WindowsBuild");
        _appliedSettings.emplace("Platform.LinuxBuild");
        _appliedSettings.emplace("Platform.MacOsBuild");
        _appliedSettings.emplace("Platform.AndroidBuild");
        _appliedSettings.emplace("Platform.IOsBuild");
        _appliedSettings.emplace("Platform.DesktopBuild");
        _appliedSettings.emplace("Platform.TabletBuild");
        _appliedSettings.emplace("Geometry.MapHexagonal");
        _appliedSettings.emplace("Geometry.MapSquare");
        _appliedSettings.emplace("Geometry.MapDirCount");
        _appliedSettings.emplace("Common.Packaged");
        _appliedSettings.emplace("Common.DebugBuild");
        _appliedSettings.emplace("Render.RenderDebug");
        _appliedSettings.emplace("View.MonitorWidth");
        _appliedSettings.emplace("View.MonitorHeight");
        _appliedSettings.emplace("Baking.ClientResourceEntries");
        _appliedSettings.emplace("Baking.MapperResourceEntries");
        _appliedSettings.emplace("Baking.ServerResourceEntries");
        _appliedSettings.emplace("ClientNetwork.Ping");
        _appliedSettings.emplace("Hex.ScrollMouseUp");
        _appliedSettings.emplace("Hex.ScrollMouseDown");
        _appliedSettings.emplace("Hex.ScrollMouseLeft");
        _appliedSettings.emplace("Hex.ScrollMouseRight");
        _appliedSettings.emplace("Hex.ScrollKeybUp");
        _appliedSettings.emplace("Hex.ScrollKeybDown");
        _appliedSettings.emplace("Hex.ScrollKeybLeft");
        _appliedSettings.emplace("Hex.ScrollKeybRight");
    }
}

void GlobalSettings::ApplyConfigAtPath(string_view config_name, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    if (config_name.empty()) {
        return;
    }

    const string config_path = strex(config_dir).combine_path(config_name);

    if (const auto settings_content = fs_read_file(config_path)) {
        _appliedConfigs.emplace_back(config_path);

        auto config = ConfigFile(config_name, *settings_content);
        ApplyConfigFile(config, config_dir);
    }
    else {
        throw SettingsException("Config not found", config_path);
    }
}

void GlobalSettings::ApplyConfigFile(ConfigFile& config, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (auto&& [key, value] : config.GetSection("")) {
        SetValue(string(key), string(value), config_dir);
    }

    AddResourcePacks(config.GetSections("ResourcePack"), config_dir);
    AddSubConfigs(config.GetSections("SubConfig"), config_dir);
}

void GlobalSettings::ApplyCommandLine(int32_t argc, char** argv)
{
    FO_STACK_TRACE_ENTRY();

    for (int32_t i = 0; i < argc; i++) {
        if (i == 0 && argv[0][0] != '-') {
            continue;
        }

        if (argv[i][0] == '-') {
            const string key = strex("{}", argv[i]).trim().str().substr(argv[i][1] == '-' ? 2 : 1);
            const auto value = i < argc - 1 && argv[i + 1][0] != '-' ? strex("{}", argv[i + 1]).trim().str() : "1";

            if (key != "ApplyConfig" && key != "ApplySubConfig") {
                WriteLog(LogType::Info, "Set {} to {}", key, value);
                SetValue(key, value);
            }
        }
    }
}

void GlobalSettings::ApplyInternalConfig()
{
    FO_STACK_TRACE_ENTRY();

#include "InternalConfig-Include.h"

    const auto config_str = strex().assignVolatile(INTERNAL_CONFIG, sizeof(INTERNAL_CONFIG)).str();

    if (strvex(config_str).starts_with("###InternalConfig###")) {
        throw SettingsException("Internal config not patched");
    }

    auto config = ConfigFile("InternalConfig.fomain", config_str);
    ApplyConfigFile(config, "");
}

void GlobalSettings::ApplyDefaultSettings()
{
    FO_STACK_TRACE_ENTRY();

    FO_DISABLE_WARNINGS_PUSH()
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#define FIXED_SETTING(type, group, name, ...) const_cast<type&>(name) = {__VA_ARGS__}
#define VARIABLE_SETTING(type, group, name, ...) name = {__VA_ARGS__}
#include "Settings-Include.h"
    FO_DISABLE_WARNINGS_POP()
}

void GlobalSettings::ApplyAutoSettings()
{
    FO_STACK_TRACE_ENTRY();

    const_cast<bool&>(Packaged) = IsPackaged();

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

#if FO_WINDOWS
    if (::GetSystemMetrics(SM_TABLETPC) != 0) {
        const_cast<bool&>(DesktopBuild) = false;
        const_cast<bool&>(TabletBuild) = true;
    }
#endif

    const_cast<bool&>(MapHexagonal) = GameSettings::HEXAGONAL_GEOMETRY;
    const_cast<bool&>(MapSquare) = GameSettings::SQUARE_GEOMETRY;
    const_cast<int32_t&>(MapDirCount) = GameSettings::MAP_DIR_COUNT;

#if FO_DEBUG
    const_cast<bool&>(DebugBuild) = true;
    const_cast<bool&>(RenderDebug) = true;
#endif

    if (MapDirectDraw) {
        const_cast<bool&>(MapZoomEnabled) = false;
    }

    const_cast<string&>(GitBranch) = FO_GIT_BRANCH;
    const_cast<string&>(GitCommit) = FO_BUILD_HASH;
    const_cast<string&>(CompatibilityVersion) = !ForceCompatibilityVersion.empty() ? ForceCompatibilityVersion : string_view(FO_COMPATIBILITY_VERSION);
}

void GlobalSettings::CopyFrom(const GlobalSettings& other)
{
    FO_STACK_TRACE_ENTRY();

    _resourcePacks = other._resourcePacks;
    _subConfigs = other._subConfigs;
    _appliedConfigs = other._appliedConfigs;
    _appliedSettings = other._appliedSettings;
    _bakingMode = other._bakingMode;
    _customSettings = other._customSettings;
    _emptySetting = other._emptySetting;

#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#define FIXED_SETTING(type, group, name, ...) const_cast<type&>(name) = other.name
#define VARIABLE_SETTING(type, group, name, ...) name = other.name
#include "Settings-Include.h"
}

void GlobalSettings::ApplySubConfigSection(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    const auto find_predicate = [&](const SubConfigInfo& cfg) { return cfg.Name == name; };
    const auto it = std::ranges::find_if(_subConfigs, find_predicate);

    if (it == _subConfigs.end()) {
        throw SettingsException("Sub config not found", name);
    }

    for (auto&& [key, value] : it->Settings) {
        SetValue(key, value, it->ConfigDir);
    }
}

auto GlobalSettings::GetCustomSetting(string_view name) const -> const any_t&
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _customSettings.find(name);

    if (it == _customSettings.end()) {
        return _emptySetting;
    }

    return it->second;
}

void GlobalSettings::SetCustomSetting(string_view name, any_t value)
{
    FO_STACK_TRACE_ENTRY();

    _customSettings[string(name)] = std::move(value);
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
            const size_t len = is_env ? "$ENV{"_len : (is_file ? "$FILE{"_len : (is_target_env ? "$TARGET_ENV{"_len : "$TARGET_FILE{"_len));

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
                        const string file_path = fs_is_absolute_path(name) ? name : strex(config_dir).combine_path(name);
                        if (auto file_content = fs_read_file(file_path)) {
                            *file_content = strvex(*file_content).trim();

                            resolved_value += setting_value.substr(prev_pos, pos - prev_pos - len) + *file_content;
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
#define FIXED_SETTING(type, group, name, ...) \
    case const_hash(#name): \
    case const_hash(#group "." #name): \
        SET_SETTING(const_cast<type&>(name))
#define VARIABLE_SETTING(type, group, name, ...) \
    case const_hash(#name): \
    case const_hash(#group "." #name): \
        SET_SETTING(name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()

    switch (const_hash(setting_name.c_str())) {
#include "Settings-Include.h"
    default:
        _customSettings[setting_name] = any_t(string(value));
        break;
    }

#undef SET_SETTING

    if (_bakingMode) {
        _appliedSettings.emplace(setting_name);
    }
}

void GlobalSettings::AddResourcePacks(const vector<map<string_view, string_view>*>& res_packs, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto* res_pack : res_packs) {
        const auto get_map_value = [&](string_view key) -> string {
            const auto it = res_pack->find(key);
            return it != res_pack->end() ? string(it->second) : string();
        };

        ResourcePackInfo pack_info;

        if (auto name = get_map_value("Name"); !name.empty()) {
            pack_info.Name = std::move(name);
        }
        else {
            throw SettingsException("Resource pack name not specifed");
        }

        if (auto server_only = get_map_value("ServerOnly"); !server_only.empty()) {
            pack_info.ServerOnly = strvex(server_only).to_bool();
        }
        if (auto client_only = get_map_value("ClientOnly"); !client_only.empty()) {
            pack_info.ClientOnly = strvex(client_only).to_bool();
        }
        if (auto mapper_only = get_map_value("MapperOnly"); !mapper_only.empty()) {
            pack_info.MapperOnly = strvex(mapper_only).to_bool();
        }
        if (std::bit_cast<int8_t>(pack_info.ServerOnly) + std::bit_cast<int8_t>(pack_info.ClientOnly) + std::bit_cast<int8_t>(pack_info.MapperOnly) > 1) {
            throw SettingsException("Resource pack can be common or server, client or mapper only");
        }

        if (auto inpurt_dirs = get_map_value("InputDirs"); !inpurt_dirs.empty()) {
            for (auto& inpurt_dir : strex(inpurt_dirs).split(' ')) {
                inpurt_dir = strex(config_dir).combine_path(inpurt_dir);
                pack_info.InputDirs.emplace_back(std::move(inpurt_dir));
            }
        }
        if (auto input_files = get_map_value("InputFiles"); !input_files.empty()) {
            for (auto& path : strex(input_files).split(' ')) {
                path = strex(config_dir).combine_path(path);
                pack_info.InputFiles.emplace_back(std::move(path));
            }
        }
        if (auto recursive_input = get_map_value("RecursiveInput"); !recursive_input.empty()) {
            pack_info.RecursiveInput = strvex(recursive_input).to_bool();
        }

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

        if (auto bakers = get_map_value("Bakers"); !bakers.empty()) {
            for (auto& baker : strex(bakers).split(' ')) {
                pack_info.Bakers.emplace_back(std::move(baker));
            }
        }

        _resourcePacks.emplace_back(std::move(pack_info));
    }
}

void GlobalSettings::AddSubConfigs(const vector<map<string_view, string_view>*>& sub_configs, string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto* sub_config : sub_configs) {
        const auto get_map_value = [&](string_view key) -> string {
            const auto it = sub_config->find(key);
            return it != sub_config->end() ? string(it->second) : string();
        };

        SubConfigInfo config_info;
        config_info.ConfigDir = config_dir;

        if (auto name = get_map_value("Name"); !name.empty()) {
            config_info.Name = std::move(name);
        }
        else {
            throw SettingsException("Sub config name not specifed");
        }

        if (auto parents = strex(get_map_value("Parent")).split(' '); !parents.empty()) {
            for (const auto& parent : parents) {
                const auto find_predicate = [&](const SubConfigInfo& cfg) { return cfg.Name == parent; };
                const auto it = std::ranges::find_if(_subConfigs, find_predicate);

                if (it == _subConfigs.end()) {
                    throw SettingsException("Parent sub config not found", parent);
                }

                config_info.Settings = it->Settings;
            }
        }

        for (auto&& [key, value] : *sub_config) {
            if (key != "Name" && key != "Parent") {
                config_info.Settings[string(key)] = string(value);
            }
        }

        _subConfigs.emplace_back(std::move(config_info));
    }
}

auto GlobalSettings::Save() const -> map<string, string>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_bakingMode);

    map<string, string> result;

    for (auto&& [key, value] : _customSettings) {
        if (_appliedSettings.count(key) != 0) {
            result.emplace(key, value);
        }
    }

    const auto add_setting = [&](string_view name, const auto& value) {
        if (_appliedSettings.count(name) != 0) {
            result.emplace(name, strex("{}", value));
        }
    };

#define FIXED_SETTING(type, group, name, ...) add_setting(#group "." #name, name)
#define VARIABLE_SETTING(type, group, name, ...) add_setting(#group "." #name, name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings-Include.h"

    return result;
}

void GlobalSettings::Draw(bool editable)
{
    FO_STACK_TRACE_ENTRY();

#define FIXED_SETTING(type, group, name, ...) \
    if (editable) { \
        DrawEditableEntry(#group "." #name, const_cast<type&>(name)); \
    } \
    else { \
        DrawEntry(#group "." #name, name); \
    }
#define VARIABLE_SETTING(type, group, name, ...) \
    if (editable) { \
        DrawEditableEntry(#group "." #name, name); \
    } \
    else { \
        DrawEntry(#group "." #name, name); \
    }
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings-Include.h"
}

auto BaseSettings::GetResourcePacks() const -> const_span<ResourcePackInfo>
{
    FO_STACK_TRACE_ENTRY();

    if (_resourcePacks.empty()) {
        throw SettingsException("No information about resource packs found");
    }

    return _resourcePacks;
}

FO_END_NAMESPACE
