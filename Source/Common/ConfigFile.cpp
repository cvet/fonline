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

#include "ConfigFile.h"

FO_BEGIN_NAMESPACE();

extern void ConfigSectionParseHook(const string& fname, string& section, map<string, string>& init_section_kv);
extern void ConfigEntryParseHook(const string& fname, const string& section, string& key, string& value);

ConfigFile::ConfigFile(string_view name_hint, const string& str, HashResolver* hash_resolver, ConfigFileOption options) :
    _fileNameHint {name_hint},
    _hashResolver {hash_resolver},
    _options {options}
{
    FO_STACK_TRACE_ENTRY();

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
            line = strex(line).trim();
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
            accum_line = strex(line).trim();
            accum_line.append(" ");
            continue;
        }

        // New section
        if (line.front() == '[') {
            if (IsEnumSet(_options, ConfigFileOption::ReadFirstSection) && _sectionKeyValues.size() == 2) {
                break;
            }

            // Parse name
            const auto end = line.find(']');

            if (end == string::npos) {
                continue;
            }

            string section_name = strex(line.substr(1, end - 1)).trim();

            if (section_name.empty()) {
                continue;
            }

            section_name_hint = section_name;
            map<string, string> section_kv;

            ConfigSectionParseHook(_fileNameHint, section_name, section_kv);

            if (section_name.empty()) {
                continue;
            }

            // Store current section content
            if (IsEnumSet(_options, ConfigFileOption::CollectContent)) {
                (*cur_section)[_emptyStr] = std::move(section_content);
                section_content.clear();

                if (!section_kv.empty()) {
                    for (auto&& [key, value] : section_kv) {
                        section_content += strex("{} = {}\n", key, value);
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
            if (line.front() == '{') {
                uint32 num = 0;
                size_t offset = 0;

                for (auto i = 0; i < 3; i++) {
                    auto first = line.find('{', offset);
                    auto last = line.find('}', first);

                    if (first == string::npos || last == string::npos) {
                        break;
                    }

                    auto str2 = line.substr(first + 1, last - first - 1);
                    offset = last + 1;

                    if (i == 0 && num == 0) {
                        num = strex(str2).is_number() ? static_cast<uint32>(strex(str2).to_int64()) : _hashResolver->ToHashedString(str2).as_int32();
                    }
                    else if (i == 1 && num != 0) {
                        num += !str2.empty() ? (strex(str2).is_number() ? static_cast<uint32>(strex(str2).to_int64()) : _hashResolver->ToHashedString(str2).as_int32()) : 0;
                    }
                    else if (i == 2 && num != 0) {
                        (*cur_section)[strex("{}", num)] = str2;
                    }
                }
            }
            else {
                // Cut comments
                auto comment_pos = line.find('#');

                while (comment_pos != string::npos && comment_pos != 0 && line[comment_pos - 1] == '\\') {
                    comment_pos = line.find('#', comment_pos + 1);
                }

                if (comment_pos != string::npos) {
                    line.erase(comment_pos);
                }

                if (line.empty()) {
                    continue;
                }

                // Key value format
                const auto separator = line.find('=');

                if (separator != string::npos && separator > 0) {
                    if (line[separator - 1] == '+') {
                        string key = strex(line.substr(0, separator - 1)).trim();
                        string value = strex(line.substr(separator + 1)).trim();

                        ConfigEntryParseHook(_fileNameHint, section_name_hint, key, value);

                        if (!key.empty()) {
                            if (cur_section->count(key) != 0) {
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
                        string key = strex(line.substr(0, separator)).trim();
                        string value = strex(line.substr(separator + 1)).trim();

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
        FO_RUNTIME_ASSERT(istr.eof());
    }

    // Store current section content
    if (IsEnumSet(_options, ConfigFileOption::CollectContent)) {
        (*cur_section)[_emptyStr] = section_content;
    }
}

auto ConfigFile::GetRawValue(string_view section_name, string_view key_name) const noexcept -> const string*
{
    FO_STACK_TRACE_ENTRY();

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

auto ConfigFile::GetAsStr(string_view section_name, string_view key_name) const noexcept -> string_view
{
    FO_STACK_TRACE_ENTRY();

    const auto* str = GetRawValue(section_name, key_name);

    return str != nullptr ? *str : _emptyStr;
}

auto ConfigFile::GetAsStr(string_view section_name, string_view key_name, string_view def_val) const noexcept -> string_view
{
    FO_STACK_TRACE_ENTRY();

    const auto* str = GetRawValue(section_name, key_name);

    return str != nullptr ? *str : def_val;
}

auto ConfigFile::GetAsInt(string_view section_name, string_view key_name) const noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    const auto* str = GetRawValue(section_name, key_name);

    if (str != nullptr && str->length() == "true"_len && strex(*str).compare_ignore_case("true")) {
        return 1;
    }
    if (str != nullptr && str->length() == "false"_len && strex(*str).compare_ignore_case("false")) {
        return 0;
    }

    return str != nullptr ? strex(*str).to_int32() : 0;
}

auto ConfigFile::GetAsInt(string_view section_name, string_view key_name, int32 def_val) const noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    const auto* str = GetRawValue(section_name, key_name);

    if (str != nullptr && str->length() == "true"_len && strex(*str).compare_ignore_case("true")) {
        return 1;
    }
    if (str != nullptr && str->length() == "false"_len && strex(*str).compare_ignore_case("false")) {
        return 0;
    }

    return str != nullptr ? strex(*str).to_int32() : def_val;
}

auto ConfigFile::GetSection(string_view section_name) const -> const map<string, string>&
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _sectionKeyValues.find(section_name);
    FO_RUNTIME_ASSERT(it != _sectionKeyValues.end());

    return it->second;
}

auto ConfigFile::GetSections(string_view section_name) -> vector<map<string, string>*>
{
    FO_STACK_TRACE_ENTRY();

    const auto count = _sectionKeyValues.count(section_name);
    auto it = _sectionKeyValues.find(section_name);

    vector<map<string, string>*> key_values;
    key_values.reserve(count);

    for (size_t i = 0; i < count; i++, ++it) {
        key_values.emplace_back(&it->second);
    }

    return key_values;
}

auto ConfigFile::GetSections() noexcept -> multimap<string, map<string, string>>&
{
    FO_STACK_TRACE_ENTRY();

    return _sectionKeyValues;
}

auto ConfigFile::HasSection(string_view section_name) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto it_section = _sectionKeyValues.find(section_name);

    return it_section != _sectionKeyValues.end();
}

auto ConfigFile::HasKey(string_view section_name, string_view key_name) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    const auto it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return nullptr;
    }

    return &it_section->second;
}

auto ConfigFile::GetSectionContent(string_view section_name) -> const string&
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(IsEnumSet(_options, ConfigFileOption::CollectContent));

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

FO_END_NAMESPACE();
