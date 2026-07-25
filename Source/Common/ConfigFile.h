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

enum class ConfigFileOption : uint8_t
{
    None = 0,
    CollectContent = 0x1,
    ReadFirstSection = 0x2,
};

using ConfigKeyValueMap = map<string_view, u8string_view>;
using ConfigSections = multimap<string_view, ConfigKeyValueMap>;

class ConfigFile final
{
public:
    explicit ConfigFile(u8string_view name_hint, u8string str, ConfigFileOption options = ConfigFileOption::None);
    ConfigFile(const ConfigFile&) = delete;
    ConfigFile(ConfigFile&&) noexcept;
    auto operator=(const ConfigFile&) = delete;
    auto operator=(ConfigFile&&) noexcept -> ConfigFile&;
    ~ConfigFile();

    [[nodiscard]] auto GetNameHint() const noexcept -> u8string_view { return _fileNameHint.view(); }
    [[nodiscard]] auto HasSection(string_view section_name) const noexcept -> bool;
    [[nodiscard]] auto HasKey(string_view section_name, string_view key_name) const noexcept -> bool;
    [[nodiscard]] auto GetAsStr(string_view section_name, string_view key_name) const noexcept -> u8string_view;
    [[nodiscard]] auto GetAsStr(string_view section_name, string_view key_name, u8string_view def_val) const noexcept -> u8string_view;
    [[nodiscard]] auto GetAsInt(string_view section_name, string_view key_name) const noexcept -> int32_t;
    [[nodiscard]] auto GetAsInt(string_view section_name, string_view key_name, int32_t def_val) const noexcept -> int32_t;
    [[nodiscard]] auto GetSection(string_view section_name) const -> const ConfigKeyValueMap&;
    [[nodiscard]] auto GetSections(string_view section_name) -> vector<ptr<ConfigKeyValueMap>>;
    [[nodiscard]] auto GetSections() noexcept -> ptr<ConfigSections>;
    [[nodiscard]] auto GetSectionKeyValues(string_view section_name) noexcept -> nptr<const ConfigKeyValueMap>;
    [[nodiscard]] auto GetSectionContent(string_view section_name) const -> u8string_view;

private:
    struct Data;

    auto ParseConfigKeyValueLine(std::u8string_view line, string& key, u8string& value, bool& append_value) -> bool;
    void TrimConfigRange(std::u8string_view line, size_t& begin, size_t& end);
    auto IsConfigSpace(char8_t ch) -> bool;
    auto GetRawValue(string_view section_name, string_view key_name) const noexcept -> nptr<const u8string_view>;
    auto StoreOwnedKey(string_view value) -> string_view;
    auto StoreOwnedKey(string&& value) -> string_view;
    auto StoreOwnedValue(u8string_view value) -> u8string_view;
    auto StoreOwnedValue(u8string&& value) -> u8string_view;

    u8string _fileNameHint;
    ConfigFileOption _options;
    unique_ptr<Data> _data;
    ConfigSections _sectionKeyValues {};
};

FO_END_NAMESPACE
