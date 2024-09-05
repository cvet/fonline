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

#include "ConfigFile.h"
#include "StringUtils.h"

ConfigFile::ConfigFile(string_view fname_hint, const string& str, HashResolver* hash_resolver, ConfigFileOption options) :
    _fileNameHint {fname_hint},
    _hashResolver {hash_resolver},
    _options {options}
{
    STACK_TRACE_ENTRY();

    map<string, string>* cur_section;
    string section_name_hint;

    const auto it_section = _sectionKeyValues.find(_emptyStr);

    if (it_section == _sectionKeyValues.end()) {
        auto it = _sectionKeyValues.emplace(_emptyStr, map<string, string>());
        cur_section = &it->second;
    }
    else {
        cur_section = &it_section->second;
    }

    string section_content;
    if (IsEnumSet(_options, ConfigFileOption::CollectContent)) {
        section_content.reserve(str.length());
    }

    istringstream istr(str);
    string line;
    string accum_line;

    while (std::getline(istr, line, '\n')) {
        if (!line.empty()) {
            line = _str(line).trim();
        }

        // Accumulate line
        if (!accum_line.empty()) {
            line.insert(0, accum_line);
            accum_line.clear();
        }

        if (line.empty()) {
            continue;
        }

        if (line.length() >= 2 && line.back() == '\\' && (line[line.length() - 2] == ' ' || line[line.length() - 2] == '\t')) {
            line.pop_back();
            accum_line = _str(line).trim() + " ";
            continue;
        }

        // New section
        if (line[0] == '[') {
            if (IsEnumSet(_options, ConfigFileOption::ReadFirstSection) && _sectionKeyValues.size() == 2) {
                break;
            }

            // Parse name
            const auto end = line.find(']');

            if (end == string::npos) {
                continue;
            }

            string section_name = _str(line.substr(1, end - 1)).trim();

            if (section_name.empty()) {
                continue;
            }

            section_name_hint = section_name;
            map<string, string> section_kv;

            extern void ConfigSectionParseHook(const string& fname, string& section, map<string, string>& init_section_kv);
            ConfigSectionParseHook(_fileNameHint, section_name, section_kv);

            if (section_name.empty()) {
                continue;
            }

            // Store current section content
            if (IsEnumSet(_options, ConfigFileOption::CollectContent)) {
                (*cur_section)[_emptyStr] = section_content;
                section_content.clear();

                if (!section_kv.empty()) {
                    for (auto&& [key, value] : section_kv) {
                        section_content += _str("{} = {}\n", key, value);
                    }
                }
            }

            // Add new section
            auto it = _sectionKeyValues.emplace(section_name, std::move(section_kv));
            cur_section = &it->second;
        }
        // Section content
        else {
            // Store raw content
            if (IsEnumSet(_options, ConfigFileOption::CollectContent)) {
                section_content.append(line).append("\n");
            }

            // Text format {}{}{}
            if (line[0] == '{') {
                uint num = 0;
                size_t offset = 0;

                for (auto i = 0; i < 3; i++) {
                    auto first = line.find('{', offset);
                    auto last = line.find('}', first);

                    if (first == string::npos || last == string::npos) {
                        break;
                    }

                    auto str2 = line.substr(first + 1, last - first - 1);
                    offset = last + 1;

                    if (i == 0 && num == 0u) {
                        num = _str(str2).isNumber() ? _str(str2).toInt() : _hashResolver->ToHashedString(str2).as_int();
                    }
                    else if (i == 1 && num != 0u) {
                        num += !str2.empty() ? (_str(str2).isNumber() ? _str(str2).toInt() : _hashResolver->ToHashedString(str2).as_int()) : 0;
                    }
                    else if (i == 2 && num != 0u) {
                        (*cur_section)[_str("{}", num)] = str2;
                    }
                }
            }
            else {
                // Cut comments
                auto comment = line.find('#');

                if (comment != string::npos) {
                    line.erase(comment);
                }

                if (line.empty()) {
                    continue;
                }

                // Key value format
                const auto separator = line.find('=');

                if (separator != string::npos && separator > 0) {
                    extern void ConfigEntryParseHook(const string& fname, const string& section, string& key, string& value);

                    if (line[separator - 1] == '+') {
                        string key = _str(line.substr(0, separator - 1)).trim();
                        string value = _str(line.substr(separator + 1)).trim();

                        ConfigEntryParseHook(_fileNameHint, section_name_hint, key, value);

                        if (!key.empty()) {
                            if (cur_section->count(key) != 0u) {
                                if (!value.empty()) {
                                    (*cur_section)[key] += " ";
                                    (*cur_section)[key] += value;
                                }
                            }
                            else {
                                (*cur_section)[key] = value;
                            }
                        }
                    }
                    else {
                        string key = _str(line.substr(0, separator)).trim();
                        string value = _str(line.substr(separator + 1)).trim();

                        ConfigEntryParseHook(_fileNameHint, section_name_hint, key, value);

                        if (!key.empty()) {
                            (*cur_section)[key] = value;
                        }
                    }
                }
            }
        }
    }

    if (!IsEnumSet(_options, ConfigFileOption::ReadFirstSection)) {
        RUNTIME_ASSERT(istr.eof());
    }

    // Store current section content
    if (IsEnumSet(_options, ConfigFileOption::CollectContent)) {
        (*cur_section)[_emptyStr] = section_content;
    }
}

auto ConfigFile::GetRawValue(string_view section_name, string_view key_name) const noexcept -> const string*
{
    STACK_TRACE_ENTRY();

    const auto it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return nullptr;
    }

    const auto it_key = it_section->second.find(key_name);

    if (it_key == it_section->second.end()) {
        return nullptr;
    }

    return &it_key->second;
}

auto ConfigFile::GetStr(string_view section_name, string_view key_name) const noexcept -> const string&
{
    STACK_TRACE_ENTRY();

    const auto* str = GetRawValue(section_name, key_name);

    return str != nullptr ? *str : _emptyStr;
}

auto ConfigFile::GetStr(string_view section_name, string_view key_name, const string& def_val) const noexcept -> const string&
{
    STACK_TRACE_ENTRY();

    const auto* str = GetRawValue(section_name, key_name);

    return str != nullptr ? *str : def_val;
}

auto ConfigFile::GetInt(string_view section_name, string_view key_name) const noexcept -> int
{
    STACK_TRACE_ENTRY();

    const auto* str = GetRawValue(section_name, key_name);

    if (str != nullptr && str->length() == "true"_len && _str(*str).compareIgnoreCase("true")) {
        return 1;
    }
    if (str != nullptr && str->length() == "false"_len && _str(*str).compareIgnoreCase("false")) {
        return 0;
    }

    return str != nullptr ? _str(*str).toInt() : 0;
}

auto ConfigFile::GetInt(string_view section_name, string_view key_name, int def_val) const noexcept -> int
{
    STACK_TRACE_ENTRY();

    const auto* str = GetRawValue(section_name, key_name);

    if (str != nullptr && str->length() == "true"_len && _str(*str).compareIgnoreCase("true")) {
        return 1;
    }
    if (str != nullptr && str->length() == "false"_len && _str(*str).compareIgnoreCase("false")) {
        return 0;
    }

    return str != nullptr ? _str(*str).toInt() : def_val;
}

void ConfigFile::SetStr(string_view section_name, string_view key_name, string_view val)
{
    STACK_TRACE_ENTRY();

    const auto it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        map<string, string> key_values;
        key_values.emplace(key_name, val);

        _sectionKeyValues.emplace(section_name, key_values);
    }
    else {
        const auto it_key = it_section->second.find(key_name);

        if (it_key == it_section->second.end()) {
            it_section->second.emplace(key_name, val);
        }
        else {
            it_key->second = val;
        }
    }
}

void ConfigFile::SetInt(string_view section_name, string_view key_name, int val)
{
    STACK_TRACE_ENTRY();

    SetStr(section_name, key_name, _str("{}", val));
}

auto ConfigFile::GetSection(string_view section_name) const -> const map<string, string>&
{
    STACK_TRACE_ENTRY();

    const auto it = _sectionKeyValues.find(section_name);
    RUNTIME_ASSERT(it != _sectionKeyValues.end());

    return it->second;
}

auto ConfigFile::GetSections(string_view section_name) -> vector<map<string, string>*>
{
    STACK_TRACE_ENTRY();

    const auto count = _sectionKeyValues.count(section_name);
    auto it = _sectionKeyValues.find(section_name);

    vector<map<string, string>*> key_values;
    key_values.reserve(key_values.size() + count);

    for (size_t i = 0; i < count; i++, ++it) {
        key_values.push_back(&it->second);
    }

    return key_values;
}

auto ConfigFile::GetSections() noexcept -> multimap<string, map<string, string>>&
{
    STACK_TRACE_ENTRY();

    return _sectionKeyValues;
}

auto ConfigFile::CreateSection(string_view section_name) -> map<string, string>&
{
    STACK_TRACE_ENTRY();

    const auto it = _sectionKeyValues.emplace(section_name, map<string, string>());

    return it->second;
}

auto ConfigFile::HasSection(string_view section_name) const noexcept -> bool
{
    STACK_TRACE_ENTRY();

    const auto it_section = _sectionKeyValues.find(section_name);

    return it_section != _sectionKeyValues.end();
}

auto ConfigFile::HasKey(string_view section_name, string_view key_name) const noexcept -> bool
{
    STACK_TRACE_ENTRY();

    const auto it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return false;
    }

    const auto it_key = it_section->second.find(key_name);

    if (it_key == it_section->second.end()) {
        return false;
    }

    return true;
}

auto ConfigFile::GetSectionKeyValues(string_view section_name) noexcept -> const map<string, string>*
{
    STACK_TRACE_ENTRY();

    const auto it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return nullptr;
    }

    return &it_section->second;
}

auto ConfigFile::GetSectionContent(string_view section_name) -> const string&
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(IsEnumSet(_options, ConfigFileOption::CollectContent));

    const auto it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return _emptyStr;
    }

    const auto it_key = it_section->second.find(_emptyStr);

    if (it_key == it_section->second.end()) {
        return _emptyStr;
    }

    return it_key->second;
}
