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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(SettingsException);

class ConfigFile;

struct DummySettings
{
};

#define SETTING_GROUP(name, ...) \
    struct name : __VA_ARGS__ \
    {
#define SETTING_GROUP_END() }
#define FIXED_SETTING(type, name, ...) const type name = {__VA_ARGS__}
#define VARIABLE_SETTING(type, name, ...) type name = {__VA_ARGS__}
#include "Settings-Include.h"

struct ResourcePackInfo
{
    string Name {};
    vector<string> InputDir {};
    bool RecursiveInput {};
    bool ServerOnly {};
    bool ClientOnly {};
    bool MapperOnly {};
    int32 BakeOrder {};
};

struct SubConfigInfo
{
    string Name {};
    string ConfigDir {};
    map<string, string> Settings {};
};

struct GlobalSettings : virtual ClientSettings, virtual ServerSettings, virtual BakerSettings
{
public:
    explicit GlobalSettings(int32 argc, char** argv);
    explicit GlobalSettings(string_view config_path, string_view sub_config);
    GlobalSettings(const GlobalSettings&) = default;
    GlobalSettings(GlobalSettings&&) = default;
    auto operator=(const GlobalSettings&) = delete;
    auto operator=(GlobalSettings&&) noexcept = delete;
    ~GlobalSettings() = default;

    [[nodiscard]] auto GetResourcePacks() const -> const_span<ResourcePackInfo>;
    [[nodiscard]] auto GetSubConfigs() const noexcept -> const_span<SubConfigInfo> { return _subConfigs; }
    [[nodiscard]] auto GetCustomSetting(string_view name) const -> const string&;
    [[nodiscard]] auto Save() const -> map<string, string>;

    void SetCustomSetting(string_view name, string value);
    void Draw(bool editable);

private:
    void ApplyConfigPath(string_view config_name, string_view config_dir);
    void ApplyConfigFile(ConfigFile& config, string_view config_dir);
    void ApplySettingsConfig(string_view config_dir);
    void ApplySubConfigSection(string_view name);
    void SetValue(const string& setting_name, const string& setting_value, string_view config_dir = "");
    void AddResourcePacks(const vector<map<string, string>*>& res_packs, string_view config_dir);
    void AddSubConfigs(const vector<map<string, string>*>& sub_configs, string_view config_dir);
    void ApplyAutoSettings();

    bool _bakingMode {};
    bool _someConfigApplied {};
    vector<ResourcePackInfo> _resourcePacks {};
    vector<SubConfigInfo> _subConfigs {};
    unordered_map<string, string> _customSettings {};
    unordered_set<string> _appliedSettings {};
    string _empty {};
};

FO_END_NAMESPACE();
