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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

enum class ConfigFileOption
{
    None = 0,
    CollectContent = 0x1,
    ReadFirstSection = 0x2,
};

class ConfigFile final
{
public:
    explicit ConfigFile(string_view fname_hint, const string& str, HashResolver* hash_resolver = nullptr, ConfigFileOption options = ConfigFileOption::None);
    ConfigFile(const ConfigFile&) = delete;
    ConfigFile(ConfigFile&&) noexcept = default;
    auto operator=(const ConfigFile&) = delete;
    auto operator=(ConfigFile&&) noexcept = delete;
    ~ConfigFile() = default;

    [[nodiscard]] auto HasSection(string_view section_name) const -> bool;
    [[nodiscard]] auto HasKey(string_view section_name, string_view key_name) const -> bool;
    [[nodiscard]] auto GetStr(string_view section_name, string_view key_name) const -> const string&;
    [[nodiscard]] auto GetStr(string_view section_name, string_view key_name, const string& def_val) const -> const string&;
    [[nodiscard]] auto GetInt(string_view section_name, string_view key_name) const -> int;
    [[nodiscard]] auto GetInt(string_view section_name, string_view key_name, int def_val) const -> int;
    [[nodiscard]] auto GetSection(string_view section_name) const -> const map<string, string>&;
    [[nodiscard]] auto GetSections(string_view section_name) -> vector<map<string, string>*>;
    [[nodiscard]] auto GetSections() -> multimap<string, map<string, string>>&;
    [[nodiscard]] auto GetSectionKeyValues(string_view section_name) -> const map<string, string>*;
    [[nodiscard]] auto GetSectionContent(string_view section_name) -> const string&;

    void SetStr(string_view section_name, string_view key_name, string_view val);
    void SetInt(string_view section_name, string_view key_name, int val);
    auto CreateSection(string_view section_name) -> map<string, string>&;

private:
    [[nodiscard]] auto GetRawValue(string_view section_name, string_view key_name) const -> const string*;

    string _fileNameHint;
    HashResolver* _hashResolver;
    ConfigFileOption _options;
    multimap<string, map<string, string>> _sectionKeyValues {};
    const string _emptyStr {};
};
