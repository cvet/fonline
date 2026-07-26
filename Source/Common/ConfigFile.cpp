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

#include "ConfigFile.h"

FO_BEGIN_NAMESPACE

ConfigFile::ConfigFile(u8string str, ConfigFileOption options) :
    _options {options}
{
    FO_STACK_TRACE_ENTRY();

    // The input is the first owned node, so the section views into it keep a stable address even
    // after this object is moved; appending further nodes never invalidates it
    const u8string& owned_input = _ownedValues.emplace_back(std::move(str));
    const std::u8string_view input = owned_input.view().native_view();

    auto cur_section_it = _sectionKeyValues.emplace(string_view {}, ConfigKeyValueMap {});
    ptr<ConfigKeyValueMap> cur_section = &cur_section_it->second;
    bool skip_cur_section = false;

    _orderedSections.emplace_back(string_view {}, cur_section);

    std::u8string section_content;

    if (IsEnumSet(_options, ConfigFileOption::CollectContent)) {
        section_content.reserve(input.size());
    }

    string key;
    u8string value;
    std::u8string accum_line;
    size_t line_begin = 0;

    while (line_begin <= input.size()) {
        const size_t line_end = input.find(u8'\n', line_begin);
        const size_t view_end = line_end != std::u8string_view::npos ? line_end : input.size();
        std::u8string_view line = input.substr(line_begin, view_end - line_begin);

        std::u8string merged_line;

        if (!line.empty() && line.back() == u8'\r') {
            line.remove_suffix(1);
        }

        line_begin = line_end != std::u8string_view::npos ? line_end + 1 : input.size() + 1;
        size_t trimmed_begin = 0;
        size_t trimmed_end = line.size();
        TrimConfigRange(line, trimmed_begin, trimmed_end);
        line = line.substr(trimmed_begin, trimmed_end - trimmed_begin);

        if (!accum_line.empty()) {
            accum_line.append(line);
            merged_line = std::move(accum_line);
            line = std::u8string_view {merged_line};
        }

        accum_line.clear();

        if (line.empty()) {
            continue;
        }

        if (line.size() >= 2 && line.back() == u8'\\' && (line[line.size() - 2] == u8' ' || line[line.size() - 2] == u8'\t')) {
            const std::u8string_view continued_line = line.substr(0, line.size() - 1);
            size_t continued_begin = 0;
            size_t continued_end = continued_line.size();
            TrimConfigRange(continued_line, continued_begin, continued_end);
            accum_line.assign(continued_line.substr(continued_begin, continued_end - continued_begin));
            accum_line.push_back(u8' ');
            continue;
        }

        // New section
        if (line.front() == u8'[') {
            // Parse name
            const size_t end = line.find(u8']');

            if (end == std::u8string_view::npos) {
                continue;
            }

            std::u8string_view raw_section_name_utf8 = line.substr(1, end - 1);
            size_t section_begin = 0;
            size_t section_end = raw_section_name_utf8.size();
            TrimConfigRange(raw_section_name_utf8, section_begin, section_end);
            raw_section_name_utf8 = raw_section_name_utf8.substr(section_begin, section_end - section_begin);

            if (raw_section_name_utf8.empty()) {
                continue;
            }

            const string raw_section_name = utf8_to_string(raw_section_name_utf8);

            // A name with a separator is a nested section; what its prefix means is up to the consumer
            const bool nested_section = raw_section_name.find('/') != string::npos;

            // Store current section content
            if (IsEnumSet(_options, ConfigFileOption::CollectContent) && !skip_cur_section) {
                (*cur_section)[string_view {}] = StoreOwnedValue(u8string::FromChecked(section_content));
                section_content.clear();
            }

            if (nested_section && IsEnumSet(_options, ConfigFileOption::SkipNestedSections)) {
                skip_cur_section = true;
                section_content.clear();
                continue;
            }

            skip_cur_section = false;

            // Add new section
            const string_view stored_section_name = StoreOwnedKey(raw_section_name);

            cur_section_it = _sectionKeyValues.emplace(stored_section_name, ConfigKeyValueMap {});
            cur_section = &cur_section_it->second;
            _orderedSections.emplace_back(stored_section_name, cur_section);
        }
        // Section content
        else {
            if (skip_cur_section) {
                continue;
            }

            // Store raw content
            if (IsEnumSet(_options, ConfigFileOption::CollectContent)) {
                section_content.append(line);
                section_content.push_back(u8'\n');
            }

            bool append_value = false;

            if (!ParseConfigKeyValueLine(line, key, value, append_value)) {
                continue;
            }

            const string_view stored_key = StoreOwnedKey(key);
            const u8string_view stored_value = StoreOwnedValue(value.view());

            if (append_value) {
                auto existing_it = cur_section->find(stored_key);

                if (existing_it != cur_section->end()) {
                    if (!stored_value.empty()) {
                        u8string merged_value {existing_it->second};
                        merged_value.reserve(existing_it->second.size() + 1 + stored_value.size());
                        merged_value.append(" ");
                        merged_value.append(stored_value);
                        existing_it->second = StoreOwnedValue(std::move(merged_value));
                    }
                }
                else {
                    (*cur_section)[stored_key] = stored_value;
                }
            }
            else {
                (*cur_section)[stored_key] = stored_value;
            }
        }
    }

    // Store current section content
    if (IsEnumSet(_options, ConfigFileOption::CollectContent) && !skip_cur_section) {
        (*cur_section)[string_view {}] = StoreOwnedValue(u8string::FromChecked(section_content));
    }
}

auto ConfigFile::ParseConfigKeyValueLine(std::u8string_view line, string& key, u8string& value, bool& append_value) -> bool
{
    FO_STACK_TRACE_ENTRY();

    size_t first_non_space = 0;

    while (first_non_space < line.size() && IsConfigSpace(line[first_non_space])) {
        first_non_space++;
    }

    if (first_non_space < line.size() && line[first_non_space] == u8'{') {
        return false;
    }

    if (first_non_space + 1 < line.size() && line[first_non_space] == u8'/' && line[first_non_space + 1] == u8'/') {
        return false;
    }

    bool inside_double_quotes = false;
    size_t backslash_run = 0;
    size_t separator_pos = std::u8string_view::npos;
    size_t content_begin = 0;
    size_t content_end = line.size();

    for (size_t i = 0; i < line.size(); i++) {
        const char8_t ch = line[i];
        bool escaped = (backslash_run & 1U) != 0;

        if (ch == u8'"' && !escaped) {
            inside_double_quotes = !inside_double_quotes;
        }
        else if (ch == u8'#' && !inside_double_quotes && !escaped) {
            content_end = i;
            break;
        }
        else if (ch == u8'=' && separator_pos == std::u8string_view::npos) {
            separator_pos = i;
        }

        if (ch == u8'\\') {
            backslash_run++;
        }
        else {
            backslash_run = 0;
        }
    }

    TrimConfigRange(line, content_begin, content_end);

    if (content_begin == content_end || separator_pos == std::u8string_view::npos || separator_pos <= content_begin || separator_pos >= content_end) {
        return false;
    }

    append_value = line[separator_pos - 1] == u8'+';

    size_t key_begin = content_begin;
    size_t key_end = append_value ? separator_pos - 1 : separator_pos;
    size_t value_begin = separator_pos + 1;
    size_t value_end = content_end;

    TrimConfigRange(line, key_begin, key_end);
    TrimConfigRange(line, value_begin, value_end);

    if (key_begin == key_end) {
        return false;
    }

    key = utf8_to_string(line.substr(key_begin, key_end - key_begin));
    value = u8string::FromChecked(line.substr(value_begin, value_end - value_begin));
    return true;
}

void ConfigFile::TrimConfigRange(std::u8string_view line, size_t& begin, size_t& end)
{
    FO_STACK_TRACE_ENTRY();

    while (begin < end && IsConfigSpace(line[begin])) {
        begin++;
    }
    while (end > begin && IsConfigSpace(line[end - 1])) {
        end--;
    }
}

auto ConfigFile::IsConfigSpace(char8_t ch) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return ch == u8' ' || ch == u8'\t' || ch == u8'\r' || ch == u8'\n' || ch == u8'\f' || ch == u8'\v';
}

auto ConfigFile::StoreOwnedKey(string_view value) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    _ownedKeys.emplace_back(value);
    return _ownedKeys.back();
}

auto ConfigFile::StoreOwnedKey(string&& value) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    _ownedKeys.emplace_back(std::move(value));
    return _ownedKeys.back();
}

auto ConfigFile::StoreOwnedValue(u8string_view value) -> u8string_view
{
    FO_STACK_TRACE_ENTRY();

    _ownedValues.emplace_back(value);
    return _ownedValues.back().view();
}

auto ConfigFile::StoreOwnedValue(u8string&& value) -> u8string_view
{
    FO_STACK_TRACE_ENTRY();

    _ownedValues.emplace_back(std::move(value));
    return _ownedValues.back().view();
}

auto ConfigFile::GetRawValue(string_view section_name, string_view key_name) const noexcept -> nptr<const u8string_view>
{
    FO_STACK_TRACE_ENTRY();

    const ConfigSections::const_iterator it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return nullptr;
    }

    const ConfigKeyValueMap::const_iterator it_key = it_section->second.find(key_name);

    if (it_key == it_section->second.end()) {
        return nullptr;
    }

    return &it_key->second;
}

auto ConfigFile::GetAsStr(string_view section_name, string_view key_name) const noexcept -> u8string_view
{
    FO_STACK_TRACE_ENTRY();

    auto str = GetRawValue(section_name, key_name);

    return str ? *str : u8string_view {};
}

auto ConfigFile::GetAsStr(string_view section_name, string_view key_name, u8string_view def_val) const noexcept -> u8string_view
{
    FO_STACK_TRACE_ENTRY();

    auto str = GetRawValue(section_name, key_name);

    return str ? *str : def_val;
}

auto ConfigFile::GetAsInt(string_view section_name, string_view key_name) const noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto str = GetRawValue(section_name, key_name);

    if (str && str->size() == "true"_len && u8strvex(*str).compare_ignore_case(u8"true")) {
        return 1;
    }
    if (str && str->size() == "false"_len && u8strvex(*str).compare_ignore_case(u8"false")) {
        return 0;
    }

    return str ? u8strvex(*str).to_int32() : 0;
}

auto ConfigFile::GetAsInt(string_view section_name, string_view key_name, int32_t def_val) const noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto str = GetRawValue(section_name, key_name);

    if (str && str->size() == "true"_len && u8strvex(*str).compare_ignore_case(u8"true")) {
        return 1;
    }
    if (str && str->size() == "false"_len && u8strvex(*str).compare_ignore_case(u8"false")) {
        return 0;
    }

    return str ? u8strvex(*str).to_int32() : def_val;
}

auto ConfigFile::GetSection(string_view section_name) const -> const ConfigKeyValueMap&
{
    FO_STACK_TRACE_ENTRY();

    const ConfigSections::const_iterator it = _sectionKeyValues.find(section_name);
    FO_VERIFY_AND_THROW(it != _sectionKeyValues.end(), "Lookup failed in section key values");

    return it->second;
}

auto ConfigFile::GetSections(string_view section_name) -> vector<ptr<ConfigKeyValueMap>>
{
    FO_STACK_TRACE_ENTRY();

    size_t count = _sectionKeyValues.count(section_name);
    auto it = _sectionKeyValues.find(section_name);

    vector<ptr<ConfigKeyValueMap>> key_values;
    key_values.reserve(count);

    for (size_t i = 0; i < count; i++, ++it) {
        key_values.emplace_back(&it->second);
    }

    return key_values;
}

auto ConfigFile::GetSections() noexcept -> ptr<ConfigSections>
{
    FO_STACK_TRACE_ENTRY();

    return &_sectionKeyValues;
}

auto ConfigFile::HasSection(string_view section_name) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto it_section = _sectionKeyValues.find(section_name);
    return it_section != _sectionKeyValues.end();
}

auto ConfigFile::HasKey(string_view section_name, string_view key_name) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return false;
    }

    auto it_key = it_section->second.find(key_name);

    if (it_key == it_section->second.end()) {
        return false;
    }

    return true;
}

auto ConfigFile::GetSectionKeyValues(string_view section_name) noexcept -> nptr<const ConfigKeyValueMap>
{
    FO_STACK_TRACE_ENTRY();

    auto it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return nullptr;
    }

    return &it_section->second;
}

auto ConfigFile::GetSectionContent(string_view section_name) const -> u8string_view
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsEnumSet(_options, ConfigFileOption::CollectContent), "Config file content collection was not enabled");

    auto it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return {};
    }

    auto it_key = it_section->second.find(string_view {});

    if (it_key == it_section->second.end()) {
        return {};
    }

    return it_key->second;
}

FO_END_NAMESPACE
