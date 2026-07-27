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

#pragma once

#include "Common.h"
#include "ConfigFile.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(SettingsException);

struct ResourcePackInfo
{
    string Name {};
    vector<u8string> InputDirs {};
    vector<u8string> InputFiles {};
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
    u8string ConfigDir {};
    map<string, u8string> Settings {};
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
    [[nodiscard]] auto GetSubConfigs() const noexcept -> const_span<SubConfigInfo> { return _subConfigs; }
    [[nodiscard]] auto GetAppliedConfigs() const -> const_span<u8string> { return _appliedConfigs; }

protected:
    vector<ResourcePackInfo> _resourcePacks {};
    vector<SubConfigInfo> _subConfigs {};
    vector<u8string> _appliedConfigs {};
    unordered_set<string> _appliedSettings {};
};

#define SETTING_GROUP(name, ...) \
    struct name : __VA_ARGS__ \
    { \
        name() = default; \
        name(const name&) = delete; \
        name(name&&) noexcept = default; \
        auto operator=(const name&) -> name& = delete; \
        auto operator=(name&&) noexcept -> name& = delete
#define SETTING_GROUP_END() }
#define FIXED_SETTING(type, group, name, ...) const type name = {}
#define VARIABLE_SETTING(type, group, name, ...) type name = {}
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
    [[nodiscard]] auto Save() const -> map<string, u8string>;

    void ApplyConfigAtPath(u8string_view config_name, u8string_view config_dir);
    void ApplyConfigFile(ConfigFile& config, u8string_view config_dir);
    void ApplyCommandLine(::fo::CommandLineArgs args);
    void ApplyInternalConfig();
    void ApplySubConfigSection(string_view name);
    void ApplyDefaultSettings();
    void ApplyAutoSettings();
    void CopyFrom(const GlobalSettings& other);
    void SetCustomSetting(string_view name, any_t value);
    void Draw(bool editable);

private:
    bool IsSecretSettingName(string_view name) const;
    void SetValue(string_view setting_name, u8string_view setting_value, u8string_view config_dir = {});
    void AddResourcePacks(const vector<ptr<ConfigKeyValueMap>>& res_packs, u8string_view config_dir);
    void AddSubConfigs(const vector<ptr<ConfigKeyValueMap>>& sub_configs, u8string_view config_dir);

    bool _bakingMode;
    unordered_map<string, any_t> _customSettings {};
    any_t _emptySetting {};
};

FO_END_NAMESPACE
