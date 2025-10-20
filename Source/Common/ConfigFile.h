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

enum class ConfigFileOption : uint8
{
    None = 0,
    CollectContent = 0x1,
    ReadFirstSection = 0x2,
};

class ConfigFile final
{
public:
    explicit ConfigFile(string_view name_hint, const string& str, HashResolver* hash_resolver = nullptr, ConfigFileOption options = ConfigFileOption::None);
    ConfigFile(const ConfigFile&) = delete;
    ConfigFile(ConfigFile&&) noexcept = default;
    auto operator=(const ConfigFile&) = delete;
    auto operator=(ConfigFile&&) noexcept -> ConfigFile& = default;
    ~ConfigFile() = default;

    [[nodiscard]] auto GetNameHint() const -> const string& { return _fileNameHint; }
    [[nodiscard]] auto HasSection(string_view section_name) const noexcept -> bool;
    [[nodiscard]] auto HasKey(string_view section_name, string_view key_name) const noexcept -> bool;
    [[nodiscard]] auto GetAsStr(string_view section_name, string_view key_name) const noexcept -> string_view;
    [[nodiscard]] auto GetAsStr(string_view section_name, string_view key_name, string_view def_val) const noexcept -> string_view;
    [[nodiscard]] auto GetAsInt(string_view section_name, string_view key_name) const noexcept -> int32;
    [[nodiscard]] auto GetAsInt(string_view section_name, string_view key_name, int32 def_val) const noexcept -> int32;
    [[nodiscard]] auto GetSection(string_view section_name) const -> const map<string, string>&;
    [[nodiscard]] auto GetSections(string_view section_name) -> vector<map<string, string>*>;
    [[nodiscard]] auto GetSections() noexcept -> multimap<string, map<string, string>>&;
    [[nodiscard]] auto GetSectionKeyValues(string_view section_name) noexcept -> const map<string, string>*;
    [[nodiscard]] auto GetSectionContent(string_view section_name) -> const string&;

private:
    [[nodiscard]] auto GetRawValue(string_view section_name, string_view key_name) const noexcept -> const string*;

    string _fileNameHint;
    raw_ptr<HashResolver> _hashResolver;
    ConfigFileOption _options;
    multimap<string, map<string, string>> _sectionKeyValues {}; // Todo: rework ConfigFile entries to string_view
    string _emptyStr {};
};

FO_END_NAMESPACE();
