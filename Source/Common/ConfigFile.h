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

// Section names are stored verbatim. A name containing '/' is a nested section: the parser only
// recognizes the nesting syntax, never what a prefix means - resolving a prefix against the section
// it belongs to is the consuming format's rule, and GetOrderedSections() exposes the file order it
// needs for that. ConfigFileOption::SkipNestedSections parses only non-nested sections and skips
// nested section bodies, which keeps header enumeration cheap on files with large nested payloads.

enum class ConfigFileOption : uint8_t
{
    None = 0,
    CollectContent = 0x1,
    SkipNestedSections = 0x2,
};

class ConfigFile final
{
public:
    explicit ConfigFile(string str, ConfigFileOption options = ConfigFileOption::None);
    ConfigFile(const ConfigFile&) = delete;
    ConfigFile(ConfigFile&&) noexcept = default;
    auto operator=(const ConfigFile&) = delete;
    auto operator=(ConfigFile&&) noexcept -> ConfigFile& = default;
    ~ConfigFile() = default;

    [[nodiscard]] auto HasSection(string_view section_name) const noexcept -> bool;
    [[nodiscard]] auto HasKey(string_view section_name, string_view key_name) const noexcept -> bool;
    [[nodiscard]] auto GetAsStr(string_view section_name, string_view key_name) const noexcept -> string_view;
    [[nodiscard]] auto GetAsStr(string_view section_name, string_view key_name, string_view def_val) const noexcept -> string_view;
    [[nodiscard]] auto GetAsInt(string_view section_name, string_view key_name) const noexcept -> int32_t;
    [[nodiscard]] auto GetAsInt(string_view section_name, string_view key_name, int32_t def_val) const noexcept -> int32_t;
    [[nodiscard]] auto GetSection(string_view section_name) const -> const map<string_view, string_view>&;
    [[nodiscard]] auto GetSections(string_view section_name) -> vector<ptr<map<string_view, string_view>>>;
    [[nodiscard]] auto GetSections() noexcept -> ptr<multimap<string_view, map<string_view, string_view>>>;
    [[nodiscard]] auto GetOrderedSections() const noexcept -> const vector<pair<string_view, ptr<map<string_view, string_view>>>>& { return _orderedSections; }
    [[nodiscard]] auto GetSectionKeyValues(string_view section_name) noexcept -> nptr<const map<string_view, string_view>>;
    [[nodiscard]] auto GetSectionContent(string_view section_name) const -> string_view;

private:
    auto ParseConfigKeyValueLine(string_view line, string_view& key, string_view& value, bool& append_value) -> bool;
    void TrimConfigRange(string_view line, size_t& begin, size_t& end);
    auto IsConfigSpace(char ch) -> bool;
    auto GetRawValue(string_view section_name, string_view key_name) const noexcept -> nptr<const string_view>;
    auto StoreOwnedString(string_view value) -> string_view;
    auto StoreOwnedString(string&& value) -> string_view;

    ConfigFileOption _options;
    list<string> _ownedStrings {}; // List nodes keep their address across a move, an SSO string member would not
    multimap<string_view, map<string_view, string_view>> _sectionKeyValues {};
    vector<pair<string_view, ptr<map<string_view, string_view>>>> _orderedSections {};
};

FO_END_NAMESPACE
