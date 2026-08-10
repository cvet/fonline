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
#include "Platform.h"

FO_BEGIN_NAMESPACE

static auto TrimSettingValue(u8string_view value) -> u8string_view
{
    FO_STACK_TRACE_ENTRY();

    std::u8string_view native_value = value.native_view();
    size_t begin = 0;
    size_t end = native_value.size();
    auto is_space = [](char8_t ch) noexcept { return ch == u8' ' || ch == u8'\t' || ch == u8'\r' || ch == u8'\n'; };

    while (begin < end && is_space(native_value[begin])) {
        begin++;
    }
    while (end > begin && is_space(native_value[end - 1])) {
        end--;
    }

    return u8string_view::FromChecked(native_value.substr(begin, end - begin));
}

static auto ParseSettingValue(u8string_view value, bool as_array, AnyData::ValueType value_type) -> AnyData::Value
{
    FO_STACK_TRACE_ENTRY();

    return AnyData::ParseValue(value, false, as_array, value_type);
}

static auto JoinUtf8SettingValues(const vector<u8string>& values) -> u8string
{
    FO_STACK_TRACE_ENTRY();

    u8string result;

    for (const u8string& value : values) {
        if (!result.empty()) {
            result.append(" ");
        }

        result.append(value.view());
    }

    return result;
}

template<typename T>
static auto FixedSettingForEdit(const T& value) noexcept -> ptr<T>
{
    FO_NO_STACK_TRACE_ENTRY();

    return const_cast<T*>(&value);
}

template<typename T>
static void SetEntry(T& entry, u8string_view value, bool append)
{
    FO_STACK_TRACE_ENTRY();

    if constexpr (std::same_as<T, string>) {
        auto any_value = ParseSettingValue(value, false, AnyData::ValueType::String);
        string string_value = utf8_to_string(any_value.AsString());
        string replacement = append ? entry : string {};

        if (append && !replacement.empty()) {
            replacement += " ";
        }

        replacement += string_value;
        entry = std::move(replacement);
    }
    else if constexpr (std::same_as<T, u8string>) {
        auto any_value = ParseSettingValue(value, false, AnyData::ValueType::String);
        u8string replacement = append ? entry : u8string {};
        u8string parsed_value {any_value.AsString()};

        if (append && !replacement.empty()) {
            replacement.append(" ");
        }

        replacement.append(parsed_value.view());
        entry = std::move(replacement);
    }
    else if constexpr (std::same_as<T, bool>) {
        auto any_value = ParseSettingValue(value, false, AnyData::ValueType::Bool);
        if (!append) {
            entry = {};
        }
        entry |= any_value.AsBool();
    }
    else if constexpr (std::floating_point<T>) {
        auto any_value = ParseSettingValue(value, false, AnyData::ValueType::Float64);
        if (!append) {
            entry = {};
        }
        entry += numeric_cast<float32_t>(any_value.AsDouble());
    }
    else if constexpr (std::is_enum_v<T>) {
        auto any_value = ParseSettingValue(value, false, AnyData::ValueType::Int64);
        if (!append) {
            entry = {};
        }
        entry = static_cast<T>(static_cast<int64_t>(entry) | any_value.AsInt64());
    }
    else if constexpr (some_strong_type<T>) {
        auto any_value = ParseSettingValue(value, false, AnyData::ValueType::Int64);
        if (!append) {
            entry = {};
        }
        entry = T {numeric_cast<typename T::underlying_type>(any_value.AsInt64())};
    }
    else if constexpr (some_property_plain_type<T>) {
        auto any_value = ParseSettingValue(value, false, AnyData::ValueType::String);
        if (!append) {
            entry = {};
        }
        string string_value = utf8_to_string(any_value.AsString());
        istringstream istr {string_value};
        istr >> entry;
    }
    else {
        auto any_value = ParseSettingValue(value, false, AnyData::ValueType::Int64);
        if (!append) {
            entry = {};
        }
        entry += numeric_cast<T>(any_value.AsInt64());
    }
}

template<typename T>
static void SetEntry(vector<T>& entry, u8string_view value, bool append)
{
    FO_STACK_TRACE_ENTRY();

    if constexpr (std::same_as<T, string>) {
        auto arr_value = ParseSettingValue(value, true, AnyData::ValueType::String);
        const auto& arr = arr_value.AsArray();
        vector<string> replacement = append ? entry : vector<string> {};

        for (const auto& arr_entry : arr) {
            replacement.emplace_back(utf8_to_string(arr_entry.AsString()));
        }

        entry = std::move(replacement);
    }
    else if constexpr (std::same_as<T, u8string>) {
        auto arr_value = ParseSettingValue(value, true, AnyData::ValueType::String);
        const auto& arr = arr_value.AsArray();
        vector<u8string> replacement = append ? entry : vector<u8string> {};

        for (const auto& arr_entry : arr) {
            replacement.emplace_back(arr_entry.AsString());
        }

        entry = std::move(replacement);
    }
    else if constexpr (std::same_as<T, bool>) {
        auto arr_value = ParseSettingValue(value, true, AnyData::ValueType::Bool);
        const auto& arr = arr_value.AsArray();
        vector<bool> replacement = append ? entry : vector<bool> {};

        for (const auto& arr_entry : arr) {
            replacement.emplace_back(arr_entry.AsBool());
        }

        entry = std::move(replacement);
    }
    else if constexpr (std::floating_point<T>) {
        auto arr_value = ParseSettingValue(value, true, AnyData::ValueType::Float64);
        const auto& arr = arr_value.AsArray();
        vector<T> replacement = append ? entry : vector<T> {};

        for (const auto& arr_entry : arr) {
            replacement.emplace_back(numeric_cast<float32_t>(arr_entry.AsDouble()));
        }

        entry = std::move(replacement);
    }
    else if constexpr (std::is_enum_v<T>) {
        auto arr_value = ParseSettingValue(value, true, AnyData::ValueType::Int64);
        const auto& arr = arr_value.AsArray();
        vector<T> replacement = append ? entry : vector<T> {};

        for (const auto& arr_entry : arr) {
            replacement.emplace_back(numeric_cast<std::underlying_type_t<T>>(arr_entry.AsInt64()));
        }

        entry = std::move(replacement);
    }
    else {
        auto arr_value = ParseSettingValue(value, true, AnyData::ValueType::Int64);
        const auto& arr = arr_value.AsArray();
        vector<T> replacement = append ? entry : vector<T> {};

        for (const auto& arr_entry : arr) {
            replacement.emplace_back(numeric_cast<T>(arr_entry.AsInt64()));
        }

        entry = std::move(replacement);
    }
}

template<typename T>
static void DrawEntry(string_view name, const T& entry)
{
    FO_STACK_TRACE_ENTRY();

    if constexpr (std::same_as<T, u8string> || std::same_as<T, vector<u8string>>) {
        u8string value = [&]() -> u8string {
            if constexpr (std::same_as<T, u8string>) {
                return entry;
            }
            else {
                return JoinUtf8SettingValues(entry);
            }
        }();
        u8string text = FormatUtf8("{}: {}", name, value);
        string text_chars {utf8_as_char_view(text.view())};
        ImGui::TextUnformatted(text_chars.c_str());
    }
    else {
        ImGui::TextUnformatted(strex("{}: {}", name, entry).c_str());
    }
}

template<typename T>
static void DrawEditableEntry(string_view name, T& entry)
{
    FO_STACK_TRACE_ENTRY();

    DrawEntry(name, entry);
}

GlobalSettings::GlobalSettings(bool baking_mode) :
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
        _appliedSettings.emplace("Client.UserWritablePath");
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

void GlobalSettings::ApplyConfigAtPath(u8string_view config_name, u8string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    if (config_name.empty()) {
        return;
    }

    u8string config_path = fs_path_to_u8string(std::filesystem::path {fs_make_path(config_dir)} / std::filesystem::path {fs_make_path(config_name)});

    if (auto settings_content = fs_read_file_text(config_path.view())) {
        _appliedConfigs.emplace_back(config_path);

        auto config = ConfigFile(std::move(*settings_content));
        ApplyConfigFile(config, config_dir);
    }
    else {
        throw SettingsException("Config not found", config_path.view());
    }
}

void GlobalSettings::ApplyConfigFile(ConfigFile& config, u8string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (auto&& [key, value] : config.GetSection("")) {
        SetValue(key, value, config_dir);
    }

    AddResourcePacks(config.GetSections("ResourcePack"), config_dir);
    AddSubConfigs(config.GetSections("SubConfig"), config_dir);
}

void GlobalSettings::ApplyCommandLine(::fo::CommandLineArgs args)
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < args.size(); i++) {
        u8string_view arg = args.Get(i);

        if (arg.empty()) {
            continue;
        }

        if (i == 0 && !CommandLineArgs::IsOption(arg)) {
            continue;
        }

        if (CommandLineArgs::IsOption(arg)) {
            bool has_next_arg = i + 1 < args.size();
            u8string_view next_arg = args.Get(i + 1);
            u8string_view trimmed_arg = TrimSettingValue(arg);
            string arg_text = utf8_to_string(trimmed_arg);
            string key = arg_text.substr(arg_text.starts_with("--") ? 2 : 1);
            u8string_view value = has_next_arg && !CommandLineArgs::IsOption(next_arg) ? TrimSettingValue(next_arg) : u8"1";

            if (key != "ApplyConfig" && key != "ApplySubConfig") {
                u8string_view shown = IsSecretSettingName(key) ? u8"***" : value;
                WriteLog(LogType::Info, "Set {} to {}", key, shown);
                SetValue(key, value);
            }
        }
    }
}

void GlobalSettings::ApplyInternalConfig()
{
    FO_STACK_TRACE_ENTRY();

#include "InternalConfig.gen.inc"

    string config_str = strex().assignVolatile(INTERNAL_CONFIG, sizeof(INTERNAL_CONFIG)).str();

    if (strvex(config_str).starts_with("###InternalConfig###")) {
        throw SettingsException("Internal config not patched");
    }

    u8string config_text = config_str;
    auto config = ConfigFile(config_text);
    ApplyConfigFile(config, u8string_view {});
}

void GlobalSettings::ApplyDefaultSettings()
{
    FO_STACK_TRACE_ENTRY();

    FO_DISABLE_WARNINGS_PUSH()
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#define FIXED_SETTING(type, group, name, ...) (*FixedSettingForEdit(name)) = type {__VA_ARGS__}
#define VARIABLE_SETTING(type, group, name, ...) name = type {__VA_ARGS__}
#include "Settings.inc"
    FO_DISABLE_WARNINGS_POP()
}

void GlobalSettings::ApplyAutoSettings()
{
    FO_STACK_TRACE_ENTRY();

    *FixedSettingForEdit(Packaged) = IsPackaged();

#if FO_WEB
    *FixedSettingForEdit(WebBuild) = true;
#else
    *FixedSettingForEdit(WebBuild) = false;
#endif
#if FO_WINDOWS
    *FixedSettingForEdit(WindowsBuild) = true;
#else
    *FixedSettingForEdit(WindowsBuild) = false;
#endif
#if FO_LINUX
    *FixedSettingForEdit(LinuxBuild) = true;
#else
    *FixedSettingForEdit(LinuxBuild) = false;
#endif
#if FO_MAC
    *FixedSettingForEdit(MacOsBuild) = true;
#else
    *FixedSettingForEdit(MacOsBuild) = false;
#endif
#if FO_ANDROID
    *FixedSettingForEdit(AndroidBuild) = true;
#else
    *FixedSettingForEdit(AndroidBuild) = false;
#endif
#if FO_IOS
    *FixedSettingForEdit(IOsBuild) = true;
#else
    *FixedSettingForEdit(IOsBuild) = false;
#endif
    *FixedSettingForEdit(DesktopBuild) = WindowsBuild || LinuxBuild || MacOsBuild;
    *FixedSettingForEdit(TabletBuild) = AndroidBuild || IOsBuild;

    *FixedSettingForEdit(MapHexagonal) = GameSettings::HEXAGONAL_GEOMETRY;
    *FixedSettingForEdit(MapSquare) = GameSettings::SQUARE_GEOMETRY;
    *FixedSettingForEdit(MapDirCount) = GameSettings::MAP_DIR_COUNT;

#if FO_DEBUG
    *FixedSettingForEdit(DebugBuild) = true;
    *FixedSettingForEdit(RenderDebug) = true;
#endif

    if (MapDirectDraw) {
        *FixedSettingForEdit(MapZoomEnabled) = false;
    }

    *FixedSettingForEdit(GitBranch) = FO_GIT_BRANCH;
    *FixedSettingForEdit(GitCommit) = FO_BUILD_HASH;
    *FixedSettingForEdit(CompatibilityVersion) = !ForceCompatibilityVersion.empty() ? ForceCompatibilityVersion : string_view(FO_COMPATIBILITY_VERSION);
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
#define FIXED_SETTING(type, group, name, ...) (*FixedSettingForEdit(name)) = other.name
#define VARIABLE_SETTING(type, group, name, ...) name = other.name
#include "Settings.inc"
}

void GlobalSettings::ApplySubConfigSection(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    auto find_predicate = [&](const SubConfigInfo& cfg) { return cfg.Name == name; };
    auto it = std::ranges::find_if(_subConfigs, find_predicate);

    if (it == _subConfigs.end()) {
        throw SettingsException("Sub config not found", name);
    }

    for (auto&& [key, value] : it->Settings) {
        SetValue(key, value.view(), it->ConfigDir.view());
    }
}

auto GlobalSettings::GetCustomSetting(string_view name) const -> const any_t&
{
    FO_STACK_TRACE_ENTRY();

    auto it = _customSettings.find(name);

    if (it == _customSettings.end()) {
        return _emptySetting;
    }

    return it->second;
}

auto GlobalSettings::FindCustomSetting(string_view name) const -> nptr<const any_t>
{
    FO_STACK_TRACE_ENTRY();

    auto it = _customSettings.find(name);

    if (it == _customSettings.end()) {
        return nullptr;
    }

    return &it->second;
}

void GlobalSettings::SetCustomSetting(string_view name, any_t value)
{
    FO_STACK_TRACE_ENTRY();

    _customSettings[string(name)] = std::move(value);
}

void GlobalSettings::SetValue(string_view setting_name, u8string_view setting_value, u8string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    string owned_setting_name {setting_name};
    u8string checked_setting_value {setting_value};
    std::u8string_view setting_value_utf8 = checked_setting_value.view().native_view();
    bool append = !setting_value_utf8.empty() && setting_value_utf8.front() == u8'+';
    u8string_view value = u8string_view::FromChecked(setting_value_utf8.substr(append ? 1 : 0));

    // Resolve environment variables and files
    u8string resolved_value;
    size_t prev_pos = 0;
    size_t pos = setting_value_utf8.find(u8'$');

    if (pos != std::u8string_view::npos) {
        while (pos != std::u8string_view::npos) {
            bool is_env = setting_value_utf8.compare(pos, "$ENV{"_len, u8"$ENV{") == 0;
            bool is_file = setting_value_utf8.compare(pos, "$FILE{"_len, u8"$FILE{") == 0;
            bool is_target_env = setting_value_utf8.compare(pos, "$TARGET_ENV{"_len, u8"$TARGET_ENV{") == 0;
            bool is_target_file = setting_value_utf8.compare(pos, "$TARGET_FILE{"_len, u8"$TARGET_FILE{") == 0;
            size_t len = is_env ? "$ENV{"_len : (is_file ? "$FILE{"_len : (is_target_env ? "$TARGET_ENV{"_len : "$TARGET_FILE{"_len));

            if (is_env || is_file || (!_bakingMode && (is_target_env || is_target_file))) {
                pos += len;
                size_t end_pos = setting_value_utf8.find(u8'}', pos);

                if (end_pos != std::u8string_view::npos) {
                    u8string_view name = u8string_view::FromChecked(setting_value_utf8.substr(pos, end_pos - pos));

                    if (is_env || is_target_env) {
                        string variable_name = utf8_to_string(name);
                        optional<u8string> env = !variable_name.empty() ? Platform::GetEnvironmentUtf8(string_view_nt_from_span(const_span<char> {variable_name.data(), variable_name.size() + 1})) : std::nullopt;

                        if (env) {
                            resolved_value.append(u8string_view::FromChecked(setting_value_utf8.substr(prev_pos, pos - prev_pos - len)));
                            resolved_value.append(env->view());
                            end_pos++;
                        }
                        else {
                            WriteLog(LogType::Warning, "Environment variable {} for setting {} is not found", name, owned_setting_name);
                            resolved_value.append(u8string_view::FromChecked(setting_value_utf8.substr(prev_pos, pos - prev_pos)));
                            resolved_value.append(name);
                        }
                    }
                    else {
                        u8string name_utf8 {name};
                        u8string file_path = fs_is_absolute_path(name_utf8.view()) ? name_utf8 : fs_path_to_u8string(std::filesystem::path {fs_make_path(config_dir)} / std::filesystem::path {fs_make_path(name_utf8.view())});
                        if (auto file_content = fs_read_file_text(file_path.view())) {
                            std::u8string_view trimmed_file_content = file_content->view().native_view();
                            size_t trimmed_begin = trimmed_file_content.find_first_not_of(u8" \n\r\t");

                            if (trimmed_begin == std::u8string_view::npos) {
                                trimmed_file_content = {};
                            }
                            else {
                                size_t trimmed_end = trimmed_file_content.find_last_not_of(u8" \n\r\t");
                                trimmed_file_content = trimmed_file_content.substr(trimmed_begin, trimmed_end - trimmed_begin + 1);
                            }

                            resolved_value.append(u8string_view::FromChecked(setting_value_utf8.substr(prev_pos, pos - prev_pos - len)));
                            resolved_value.append(u8string_view::FromChecked(trimmed_file_content));
                            end_pos++;
                        }
                        else {
                            WriteLog(LogType::Warning, "File {} for setting {} is not found", file_path.view(), owned_setting_name);
                            resolved_value.append(u8string_view::FromChecked(setting_value_utf8.substr(prev_pos, pos - prev_pos)));
                            resolved_value.append(name);
                        }
                    }

                    prev_pos = end_pos;
                    pos = setting_value_utf8.find(u8'$', end_pos);
                }
                else {
                    throw SettingsException("Not closed $ tag in settings", owned_setting_name, checked_setting_value.view());
                }
            }
            else {
                pos = setting_value_utf8.find(u8'$', pos + 1);
            }
        }

        if (prev_pos != std::u8string_view::npos) {
            resolved_value.append(u8string_view::FromChecked(setting_value_utf8.substr(prev_pos)));
        }

        value = resolved_value.view();
    }

    u8string strict_value {value};

#define SET_SETTING(sett) \
    SetEntry(sett, strict_value.view(), append); \
    break
#define FIXED_SETTING(type, group, name, ...) \
    case const_hash(#name): \
    case const_hash(#group "." #name): \
        SET_SETTING(*FixedSettingForEdit(name))
#define VARIABLE_SETTING(type, group, name, ...) \
    case const_hash(#name): \
    case const_hash(#group "." #name): \
        SET_SETTING(name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()

    switch (const_hash(owned_setting_name.c_str())) {
#include "Settings.inc"
    default:
        _customSettings[owned_setting_name] = any_t(utf8_to_string(strict_value.view()));
        break;
    }

#undef SET_SETTING

    if (_bakingMode) {
        _appliedSettings.emplace(owned_setting_name);
    }
}

void GlobalSettings::AddResourcePacks(const vector<ptr<ConfigKeyValueMap>>& res_packs, u8string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (ptr<const ConfigKeyValueMap> res_pack : res_packs) {
        auto get_map_value = [&](string_view key) -> u8string {
            auto it = res_pack->find(key);
            return it != res_pack->end() ? u8string {it->second} : u8string {};
        };

        ResourcePackInfo pack_info;

        if (auto name = get_map_value("Name"); !name.empty()) {
            pack_info.Name = utf8_to_string(name.view());
        }
        else {
            throw SettingsException("Resource pack name not specifed");
        }

        if (auto server_only = get_map_value("ServerOnly"); !server_only.empty()) {
            pack_info.ServerOnly = u8strvex(server_only).to_bool();
        }
        if (auto client_only = get_map_value("ClientOnly"); !client_only.empty()) {
            pack_info.ClientOnly = u8strvex(client_only).to_bool();
        }
        if (auto mapper_only = get_map_value("MapperOnly"); !mapper_only.empty()) {
            pack_info.MapperOnly = u8strvex(mapper_only).to_bool();
        }
        if (std::bit_cast<int8_t>(pack_info.ServerOnly) + std::bit_cast<int8_t>(pack_info.ClientOnly) + std::bit_cast<int8_t>(pack_info.MapperOnly) > 1) {
            throw SettingsException("Resource pack can be common or server, client or mapper only");
        }

        if (auto input_dirs = get_map_value("InputDirs"); !input_dirs.empty()) {
            for (const u8string& input_dir : u8strex(input_dirs).split(u8' ')) {
                pack_info.InputDirs.emplace_back(fs_path_to_u8string(std::filesystem::path {fs_make_path(config_dir)} / std::filesystem::path {fs_make_path(input_dir.view())}));
            }
        }
        if (auto input_files = get_map_value("InputFiles"); !input_files.empty()) {
            for (const u8string& path : u8strex(input_files).split(u8' ')) {
                pack_info.InputFiles.emplace_back(fs_path_to_u8string(std::filesystem::path {fs_make_path(config_dir)} / std::filesystem::path {fs_make_path(path.view())}));
            }
        }
        if (auto include_patterns = get_map_value("IncludePatterns"); !include_patterns.empty()) {
            for (const u8string& pattern : u8strex(include_patterns).split(u8' ')) {
                pack_info.IncludePatterns.emplace_back(utf8_to_string(pattern));
            }
        }
        if (auto exclude_patterns = get_map_value("ExcludePatterns"); !exclude_patterns.empty()) {
            for (const u8string& pattern : u8strex(exclude_patterns).split(u8' ')) {
                pack_info.ExcludePatterns.emplace_back(utf8_to_string(pattern));
            }
        }

        if (pack_info.ServerOnly) {
            FixedSettingForEdit(ServerResourceEntries)->emplace_back(pack_info.Name);
        }
        else if (pack_info.ClientOnly) {
            FixedSettingForEdit(ClientResourceEntries)->emplace_back(pack_info.Name);
        }
        else if (pack_info.MapperOnly) {
            FixedSettingForEdit(MapperResourceEntries)->emplace_back(pack_info.Name);
        }
        else {
            FixedSettingForEdit(ServerResourceEntries)->emplace_back(pack_info.Name);
            FixedSettingForEdit(ClientResourceEntries)->emplace_back(pack_info.Name);
        }

        if (auto bakers = get_map_value("Bakers"); !bakers.empty()) {
            for (const u8string& baker : u8strex(bakers).split(u8' ')) {
                pack_info.Bakers.emplace_back(utf8_to_string(baker));
            }
        }

        _resourcePacks.emplace_back(std::move(pack_info));
    }
}

void GlobalSettings::AddSubConfigs(const vector<ptr<ConfigKeyValueMap>>& sub_configs, u8string_view config_dir)
{
    FO_STACK_TRACE_ENTRY();

    for (ptr<const ConfigKeyValueMap> sub_config : sub_configs) {
        auto get_map_value = [&](string_view key) -> u8string {
            auto it = sub_config->find(key);
            return it != sub_config->end() ? u8string {it->second} : u8string {};
        };

        SubConfigInfo config_info;
        config_info.ConfigDir = u8string {config_dir};

        if (auto name = get_map_value("Name"); !name.empty()) {
            config_info.Name = utf8_to_string(name.view());
        }
        else {
            throw SettingsException("Sub config name not specifed");
        }

        u8string parents_value = get_map_value("Parent");
        if (auto parents = u8strex(parents_value).split(u8' '); !parents.empty()) {
            for (const u8string& parent : parents) {
                string parent_name = utf8_to_string(parent);
                auto find_predicate = [&](const SubConfigInfo& cfg) { return cfg.Name == parent_name; };
                auto it = std::ranges::find_if(_subConfigs, find_predicate);

                if (it == _subConfigs.end()) {
                    throw SettingsException("Parent sub config not found", parent_name);
                }

                // Merge, not assign: with multiple parents (Parent = A B) later parents override earlier
                // ones per key, and the section's own settings (below) override all parents.
                for (auto&& [key, value] : it->Settings) {
                    config_info.Settings[key] = value;
                }
            }
        }

        for (auto&& [key, value] : *sub_config) {
            if (key != "Name" && key != "Parent") {
                config_info.Settings[string(key)] = u8string {value};
            }
        }

        _subConfigs.emplace_back(std::move(config_info));
    }
}

auto GlobalSettings::Save() const -> map<string, u8string>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_bakingMode, "Settings can only be saved in baking mode");

    map<string, u8string> result;

    for (auto&& [key, value] : _customSettings) {
        if (_appliedSettings.count(key) != 0) {
            string formatted_value = strex("{}", value);
            result.emplace(key, formatted_value);
        }
    }

    auto add_setting = [&](string_view name, const auto& value) {
        if (_appliedSettings.count(name) != 0) {
            using value_type = std::remove_cvref_t<decltype(value)>;

            if constexpr (std::same_as<value_type, u8string>) {
                result.emplace(name, value);
            }
            else if constexpr (std::same_as<value_type, vector<u8string>>) {
                result.emplace(name, JoinUtf8SettingValues(value));
            }
            else {
                string formatted_value = strex("{}", value);
                result.emplace(name, formatted_value);
            }
        }
    };

#define FIXED_SETTING(type, group, name, ...) add_setting(#group "." #name, name)
#define VARIABLE_SETTING(type, group, name, ...) add_setting(#group "." #name, name)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings.inc"

    return result;
}

void GlobalSettings::Draw(bool editable)
{
    FO_STACK_TRACE_ENTRY();

#define FIXED_SETTING(type, group, name, ...) \
    if (editable) { \
        DrawEditableEntry(#group "." #name, *FixedSettingForEdit(name)); \
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
#include "Settings.inc"
}

auto BaseSettings::GetResourcePacks() const -> const_span<ResourcePackInfo>
{
    FO_STACK_TRACE_ENTRY();

    if (_resourcePacks.empty()) {
        throw SettingsException("No information about resource packs found");
    }

    return _resourcePacks;
}

bool GlobalSettings::IsSecretSettingName(string_view name) const
{
    FO_STACK_TRACE_ENTRY();

    string lower_name = strex(name).lower().str();

    for (const auto& token : SecretSettingTokens) {
        if (!token.empty() && lower_name.find(strex(token).lower().str()) != string::npos) {
            return true;
        }
    }

    return false;
}

FO_END_NAMESPACE
