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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(SettingsException);

class ConfigFile;
class FileSystem;

struct ResourcePackInfo
{
    string Name {};
    string ConfigDir {};
    vector<string> InputDirs {};
    vector<string> InputFiles {};
    vector<string> IncludePatterns {};
    vector<string> ExcludePatterns {};
    bool ServerOnly {};
    bool ClientOnly {};
    bool MapperOnly {};
    vector<string> Bakers {};
};

struct SubConfigInfo
{
    string Name {};
    string ConfigDir {};
    map<string, string> Settings {};
};

struct BaseSettings
{
public:
    BaseSettings() = default;
    BaseSettings(const BaseSettings&) = delete;
    BaseSettings(BaseSettings&&) noexcept = default;
    auto operator=(const BaseSettings&) -> BaseSettings& = delete;
    auto operator=(BaseSettings&&) noexcept -> BaseSettings& = delete;

    [[nodiscard]] auto GetResourcePacks() const -> const_span<ResourcePackInfo>;
    [[nodiscard]] auto GetServerResourcePacks() const -> vector<string>;
    [[nodiscard]] auto GetClientResourcePacks() const -> vector<string>;
    [[nodiscard]] auto GetMapperResourcePacks() const -> vector<string>;
    [[nodiscard]] auto GetResourcePackDeclarations() const -> string;
    [[nodiscard]] auto GetSubConfigs() const noexcept -> const_span<SubConfigInfo> { return _subConfigs; }
    [[nodiscard]] auto GetAppliedConfigs() const -> const_span<string> { return _appliedConfigs; }
    [[nodiscard]] auto GetResourceConfigContent() const noexcept -> const optional<string>& { return _resourceConfigContent; }
    [[nodiscard]] auto FindSettingValue(string_view name) const -> nptr<const string>;

protected:
    vector<ResourcePackInfo> _resourcePacks {};
    vector<SubConfigInfo> _subConfigs {};
    vector<string> _appliedConfigs {};
    unordered_set<string> _appliedSettings {};
    map<string, string> _settingValues {};
    optional<string> _resourceConfigContent {};
};

// A group owns its settings as a nested aggregate named after it, so a setting is addressed as Settings.Group.Name
// and two groups may share a short name
#define SETTING_GROUP(group, ...) \
    struct group##Settings : __VA_ARGS__ \
    { \
        group##Settings() = default; \
        group##Settings(const group##Settings&) = delete; \
        group##Settings(group##Settings&&) noexcept = default; \
        auto operator=(const group##Settings&) -> group##Settings& = delete; \
        auto operator=(group##Settings&&) noexcept -> group##Settings& = delete; \
        struct group##Group \
        {
#define SETTING_GROUP_END(group) \
    } \
    group {}; \
    }
#define SETTING(type, group, name, ...) \
    const type name \
    { \
    }
#include "Settings.inc"

struct GlobalSettings : virtual ClientSettings, virtual ServerSettings, virtual BakingSettings, virtual BaseSettings
{
public:
    explicit GlobalSettings(bool baking_mode);
    GlobalSettings(const GlobalSettings&) = delete;
    GlobalSettings(GlobalSettings&&) noexcept = default;
    auto operator=(const GlobalSettings&) -> GlobalSettings& = delete;
    auto operator=(GlobalSettings&&) noexcept -> GlobalSettings& = delete;
    ~GlobalSettings() = default;

    [[nodiscard]] auto GetCustomSetting(string_view name) const -> const any_t&;
    [[nodiscard]] auto FindCustomSetting(string_view name) const -> nptr<const any_t>;
    [[nodiscard]] auto GetCustomSettingsGeneration() const noexcept -> uint64_t { return _customSettingsGeneration; }
    [[nodiscard]] auto GetCommandLineArgs() const -> CommandLineArgs;
    [[nodiscard]] auto GetPackagedRuntimeName() const noexcept -> const string& { return _packagedRuntimeName; }
    [[nodiscard]] auto HasEngineSettingsChanges(const GlobalSettings& other) const -> bool;
    [[nodiscard]] auto HasCompleteConfig() const noexcept -> bool { return _setupState == SetupState::Complete; }

    void SetupConfigAtPath(string_view config_name, string_view config_dir);
    void SetupInternalConfig();
    void SetupResourceConfig(string_view content);
    void SetupDefaultSettings();
    void ApplyConfigFile(ConfigFile& config, string_view config_dir);
    void ApplyCommandLine(CommandLineArgs args);
    void ApplyGameSettings(const GlobalSettings& other);
    void ApplyStartupOverrides(const GlobalSettings& source);
    void ApplySubConfigSection(string_view name);
    void ApplyAutoSettings(string_view packaged_runtime_name);
    void ApplyWritableRoot(string_view root);
    void CopyFrom(const GlobalSettings& other);
    void SetSettingValue(string_view name, string_view value);
    void SetCustomSetting(string_view name, any_t value);
    auto GetRuntimeSetting(const string& name) const -> string;
    void SetRuntimeSetting(const string& name, const string& value);
    void Draw(bool editable);
    auto Save() const -> map<string, string>;

private:
    enum class SetupState : uint8_t
    {
        Fresh,
        Initialized,
        Complete,
    };

    void VerifyFreshSettings() const;
    bool IsSecretSettingName(string_view name) const;
    void SetValue(const string& setting_name, const string& setting_value, string_view config_dir = "");
    void AddResourcePacks(const vector<ptr<map<string_view, string_view>>>& res_packs, string_view config_dir);
    void AddSubConfigs(const vector<ptr<map<string_view, string_view>>>& sub_configs, string_view config_dir);
    void ApplyIgnoreInputDirs();

    bool _bakingMode {};
    SetupState _setupState {SetupState::Fresh};
    vector<ResourcePackInfo> _declaredResourcePacks {};
    unordered_map<string, any_t> _customSettings {};
    uint64_t _customSettingsGeneration {};
    string _packagedRuntimeName {};
    vector<string> _commandLineArgs {};
    vector<pair<string, string>> _startupOverrides {};
    any_t _emptySetting {};
};

auto ReadInternalConfigValue(string_view name) -> string;
auto ReadResourceConfig(const FileSystem& resources, bool required) -> optional<string>;
void PreserveLocalConfig(const GlobalSettings& previous, const GlobalSettings& updated);
auto ResolveSettingValue(string_view name, const string& value, string_view config_dir, bool baking_mode) -> optional<string>;

// Typed read of one builtin numeric or bool setting, so the managed bridge skips the format/parse round trip.
// Settings are immutable, so there is no write half; exactly one Read* member is set, by the declared type
struct NumericSettingAccess
{
    bool (*ReadBool)(ptr<const GlobalSettings>) {};
    int64_t (*ReadSigned)(ptr<const GlobalSettings>) {};
    uint64_t (*ReadUnsigned)(ptr<const GlobalSettings>) {};
    float64_t (*ReadFloat)(ptr<const GlobalSettings>) {};
};

auto SplitSettingPath(string_view setting_name) -> vector<string>;
auto FindNumericSettingAccess(string_view name) -> nptr<const NumericSettingAccess>;

FO_END_NAMESPACE
