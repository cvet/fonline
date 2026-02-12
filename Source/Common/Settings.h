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

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(SettingsException);

class ConfigFile;

struct ResourcePackInfo
{
    string Name {};
    vector<string> InputDirs {};
    vector<string> InputFiles {};
    bool RecursiveInput {};
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
    [[nodiscard]] auto GetSubConfigs() const noexcept -> const_span<SubConfigInfo> { return _subConfigs; }
    [[nodiscard]] auto GetAppliedConfigs() const -> const_span<string> { return _appliedConfigs; }

protected:
    vector<ResourcePackInfo> _resourcePacks {};
    vector<SubConfigInfo> _subConfigs {};
    vector<string> _appliedConfigs {};
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
#define FIXED_SETTING(type, name, ...) const type name = {}
#define VARIABLE_SETTING(type, name, ...) type name = {}
#include "Settings-Include.h"

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
    [[nodiscard]] auto Save() const -> map<string, string>;

    void ApplyConfigAtPath(string_view config_name, string_view config_dir);
    void ApplyConfigFile(ConfigFile& config, string_view config_dir);
    void ApplyCommandLine(int32 argc, char** argv);
    void ApplyInternalConfig();
    void ApplySubConfigSection(string_view name);
    void ApplyAutoSettings();
    void SetCustomSetting(string_view name, any_t value);
    void Draw(bool editable);

private:
    void SetValue(const string& setting_name, const string& setting_value, string_view config_dir = "");
    void AddResourcePacks(const vector<map<string, string>*>& res_packs, string_view config_dir);
    void AddSubConfigs(const vector<map<string, string>*>& sub_configs, string_view config_dir);

    bool _bakingMode;
    unordered_map<string, any_t> _customSettings {};
};

FO_END_NAMESPACE
