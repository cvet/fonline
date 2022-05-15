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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
    ConfigFile(string_view str, NameResolver& name_resolver);
    ConfigFile(string_view str, std::nullptr_t);
    ConfigFile(const ConfigFile&) = delete;
    ConfigFile(ConfigFile&&) noexcept = default;
    auto operator=(const ConfigFile&) = delete;
    auto operator=(ConfigFile&&) noexcept = delete;
    explicit operator bool() const { return !_appKeyValues.empty(); }
    ~ConfigFile() = default;

    [[nodiscard]] auto IsApp(string_view app_name) const -> bool;
    [[nodiscard]] auto IsKey(string_view app_name, string_view key_name) const -> bool;
    [[nodiscard]] auto GetStr(string_view app_name, string_view key_name) const -> string;
    [[nodiscard]] auto GetStr(string_view app_name, string_view key_name, string_view def_val) const -> string;
    [[nodiscard]] auto GetInt(string_view app_name, string_view key_name) const -> int;
    [[nodiscard]] auto GetInt(string_view app_name, string_view key_name, int def_val) const -> int;
    [[nodiscard]] auto GetApp(string_view app_name) const -> const map<string, string>&;
    [[nodiscard]] auto GetApps(string_view app_name) -> vector<map<string, string>*>;
    [[nodiscard]] auto GetAppNames() const -> set<string>;
    [[nodiscard]] auto GetAppKeyValues(string_view app_name) -> const map<string, string>*;
    [[nodiscard]] auto GetAppContent(string_view app_name) -> string;
    [[nodiscard]] auto SerializeData() -> string;

    void CollectContent();
    void AppendData(string_view str);
    void SetStr(string_view app_name, string_view key_name, string_view val);
    void SetInt(string_view app_name, string_view key_name, int val);
    auto SetApp(string_view app_name) -> map<string, string>&;
    void GotoNextApp(string_view app_name);

private:
    using ValuesMap = multimap<string, map<string, string>>;
    using ValuesMapItVec = vector<ValuesMap::const_iterator>;

    [[nodiscard]] auto GetRawValue(string_view app_name, string_view key_name) const -> const string*;

    void ParseStr(string_view str);

    NameResolver* _nameResolver;
    bool _collectContent {};
    ValuesMap _appKeyValues {};
    ValuesMapItVec _appKeyValuesOrder {};
};
