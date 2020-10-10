//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

class ConfigFile final
{
public:
    ConfigFile() = delete;
    explicit ConfigFile(const string& str);
    ConfigFile(const ConfigFile&) = delete;
    ConfigFile(ConfigFile&&) noexcept = default;
    auto operator=(const ConfigFile&) = delete;
    auto operator=(ConfigFile&&) noexcept -> ConfigFile& = default;
    ~ConfigFile() = default;

    explicit operator bool() const { return !_appKeyValues.empty(); }

    void CollectContent();
    void AppendData(const string& str);
    auto SerializeData() -> string;

    [[nodiscard]] auto GetStr(const string& app_name, const string& key_name) const -> string;
    [[nodiscard]] auto GetStr(const string& app_name, const string& key_name, const string& def_val) const -> string;
    [[nodiscard]] auto GetInt(const string& app_name, const string& key_name) const -> int;
    [[nodiscard]] auto GetInt(const string& app_name, const string& key_name, int def_val) const -> int;

    void SetStr(const string& app_name, const string& key_name, const string& val);
    void SetInt(const string& app_name, const string& key_name, int val);

    [[nodiscard]] auto GetApp(const string& app_name) const -> const StrMap&;
    void GetApps(const string& app_name, PStrMapVec& key_values);
    auto SetApp(const string& app_name) -> StrMap&;

    [[nodiscard]] auto IsApp(const string& app_name) const -> bool;
    [[nodiscard]] auto IsKey(const string& app_name, const string& key_name) const -> bool;

    void GetAppNames(StrSet& apps) const;
    void GotoNextApp(const string& app_name);
    auto GetAppKeyValues(const string& app_name) -> const StrMap*;
    auto GetAppContent(const string& app_name) -> string;

private:
    using ValuesMap = multimap<string, StrMap>;
    using ValuesMapItVec = vector<ValuesMap::const_iterator>;

    void ParseStr(const string& str);
    [[nodiscard]] auto GetRawValue(const string& app_name, const string& key_name) const -> const string*;

    bool _collectContent {};
    ValuesMap _appKeyValues {};
    ValuesMapItVec _appKeyValuesOrder {};
};
