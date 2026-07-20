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

ConfigFile::ConfigFile(string str, ConfigFileOption options) :
    _options {options}
{
    FO_STACK_TRACE_ENTRY();

    // The input is the first owned node, so the section views into it keep a stable address even
    // after this object is moved; appending further nodes never invalidates it
    const string& input = _ownedStrings.emplace_back(std::move(str));

    auto cur_section_it = _sectionKeyValues.emplace(string_view {}, map<string_view, string_view> {});
    ptr<map<string_view, string_view>> cur_section = &cur_section_it->second;
    bool skip_cur_section = false;

    _orderedSections.emplace_back(string_view {}, cur_section);

    string section_content;

    if (IsEnumSet(_options, ConfigFileOption::CollectContent)) {
        section_content.reserve(input.length());
    }

    string accum_line;
    size_t line_begin = 0;

    while (line_begin <= input.length()) {
        const size_t line_end = input.find('\n', line_begin);
        const size_t view_end = line_end != string::npos ? line_end : input.length();
        string_view line;

        if (view_end != line_begin) {
            auto line_begin_ptr = make_ptr(input.data() + line_begin);
            line = {line_begin_ptr.get(), view_end - line_begin};
        }

        bool line_stable = true;
        string merged_line;

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        line_begin = line_end != string::npos ? line_end + 1 : input.length() + 1;
        line = strvex(line).trim();

        if (!accum_line.empty()) {
            accum_line.append(line);
            merged_line = std::move(accum_line);
            line = merged_line;
            line_stable = false;
        }

        accum_line.clear();

        if (line.empty()) {
            continue;
        }

        if (line.size() >= 2 && line.back() == '\\' && (line[line.size() - 2] == ' ' || line[line.size() - 2] == '\t')) {
            accum_line.assign(strvex(line.substr(0, line.length() - 1)).trim());
            accum_line.append(" ");
            continue;
        }

        // New section
        if (line.front() == '[') {
            // Parse name
            const size_t end = line.find(']');

            if (end == string_view::npos) {
                continue;
            }

            const string_view raw_section_name = strvex(line.substr(1, end - 1)).trim();

            if (raw_section_name.empty()) {
                continue;
            }

            // Store current section content
            if (IsEnumSet(_options, ConfigFileOption::CollectContent) && !skip_cur_section) {
                (*cur_section)[string_view {}] = StoreOwnedString(std::move(section_content));
                section_content.clear();
            }

            // A name with a separator is a nested section; what its prefix means is up to the consumer
            const bool nested_section = raw_section_name.find('/') != string_view::npos;

            if (nested_section && IsEnumSet(_options, ConfigFileOption::SkipNestedSections)) {
                skip_cur_section = true;
                section_content.clear();
                continue;
            }

            skip_cur_section = false;

            const string_view stored_section_name = line_stable ? raw_section_name : StoreOwnedString(raw_section_name);

            // Add new section
            cur_section_it = _sectionKeyValues.emplace(stored_section_name, map<string_view, string_view> {});
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
                section_content.append(line.data(), line.size()).append("\n");
            }

            string_view raw_key;
            string_view raw_value;
            bool append_value = false;

            if (!ParseConfigKeyValueLine(line, raw_key, raw_value, append_value)) {
                continue;
            }

            const string_view stored_key = line_stable ? raw_key : StoreOwnedString(raw_key);
            const string_view stored_value = line_stable ? raw_value : StoreOwnedString(raw_value);

            if (append_value) {
                const auto existing_it = cur_section->find(stored_key);

                if (existing_it != cur_section->end()) {
                    if (!stored_value.empty()) {
                        string merged_value;
                        merged_value.reserve(existing_it->second.size() + 1 + stored_value.size());
                        merged_value.append(existing_it->second);
                        merged_value.push_back(' ');
                        merged_value.append(stored_value);
                        existing_it->second = StoreOwnedString(std::move(merged_value));
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
        (*cur_section)[string_view {}] = StoreOwnedString(std::move(section_content));
    }
}

auto ConfigFile::ParseConfigKeyValueLine(string_view line, string_view& key, string_view& value, bool& append_value) -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool inside_double_quotes = false;
    size_t backslash_run = 0;
    size_t separator_pos = string_view::npos;
    size_t content_begin = 0;
    size_t content_end = line.size();

    for (size_t i = 0; i < line.size(); i++) {
        const auto ch = line[i];
        const bool escaped = (backslash_run & 1U) != 0;

        if (ch == '"' && !escaped) {
            inside_double_quotes = !inside_double_quotes;
        }
        else if (ch == '#' && !inside_double_quotes && !escaped) {
            content_end = i;
            break;
        }
        else if (ch == '=' && separator_pos == string_view::npos) {
            separator_pos = i;
        }

        if (ch == '\\') {
            backslash_run++;
        }
        else {
            backslash_run = 0;
        }
    }

    TrimConfigRange(line, content_begin, content_end);

    if (content_begin == content_end || separator_pos == string_view::npos || separator_pos <= content_begin || separator_pos >= content_end) {
        return false;
    }

    append_value = line[separator_pos - 1] == '+';

    size_t key_begin = content_begin;
    size_t key_end = append_value ? separator_pos - 1 : separator_pos;
    size_t value_begin = separator_pos + 1;
    size_t value_end = content_end;

    TrimConfigRange(line, key_begin, key_end);
    TrimConfigRange(line, value_begin, value_end);

    if (key_begin == key_end) {
        return false;
    }

    key = line.substr(key_begin, key_end - key_begin);
    value = line.substr(value_begin, value_end - value_begin);
    return true;
}

void ConfigFile::TrimConfigRange(string_view line, size_t& begin, size_t& end)
{
    FO_STACK_TRACE_ENTRY();

    while (begin < end && IsConfigSpace(line[begin])) {
        begin++;
    }
    while (end > begin && IsConfigSpace(line[end - 1])) {
        end--;
    }
}

auto ConfigFile::IsConfigSpace(char ch) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v';
}

auto ConfigFile::StoreOwnedString(string_view value) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    _ownedStrings.emplace_back(value);
    return _ownedStrings.back();
}

auto ConfigFile::StoreOwnedString(string&& value) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    _ownedStrings.emplace_back(std::move(value));
    return _ownedStrings.back();
}
auto ConfigFile::GetRawValue(string_view section_name, string_view key_name) const noexcept -> nptr<const string_view>
{
    FO_STACK_TRACE_ENTRY();

    const multimap<string_view, map<string_view, string_view>>::const_iterator it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return nullptr;
    }

    const map<string_view, string_view>::const_iterator it_key = it_section->second.find(key_name);

    if (it_key == it_section->second.end()) {
        return nullptr;
    }

    return &it_key->second;
}

auto ConfigFile::GetAsStr(string_view section_name, string_view key_name) const noexcept -> string_view
{
    FO_STACK_TRACE_ENTRY();

    const auto str = GetRawValue(section_name, key_name);

    return str ? *str : string_view {};
}

auto ConfigFile::GetAsStr(string_view section_name, string_view key_name, string_view def_val) const noexcept -> string_view
{
    FO_STACK_TRACE_ENTRY();

    const auto str = GetRawValue(section_name, key_name);

    return str ? *str : def_val;
}

auto ConfigFile::GetAsInt(string_view section_name, string_view key_name) const noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto str = GetRawValue(section_name, key_name);

    if (str && str->length() == "true"_len && strvex(*str).compare_ignore_case("true")) {
        return 1;
    }
    if (str && str->length() == "false"_len && strvex(*str).compare_ignore_case("false")) {
        return 0;
    }

    return str ? strvex(*str).to_int32() : 0;
}

auto ConfigFile::GetAsInt(string_view section_name, string_view key_name, int32_t def_val) const noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto str = GetRawValue(section_name, key_name);

    if (str && str->length() == "true"_len && strvex(*str).compare_ignore_case("true")) {
        return 1;
    }
    if (str && str->length() == "false"_len && strvex(*str).compare_ignore_case("false")) {
        return 0;
    }

    return str ? strvex(*str).to_int32() : def_val;
}

auto ConfigFile::GetSection(string_view section_name) const -> const map<string_view, string_view>&
{
    FO_STACK_TRACE_ENTRY();

    const multimap<string_view, map<string_view, string_view>>::const_iterator it = _sectionKeyValues.find(section_name);
    FO_VERIFY_AND_THROW(it != _sectionKeyValues.end(), "Lookup failed in section key values");

    return it->second;
}

auto ConfigFile::GetSections(string_view section_name) -> vector<ptr<map<string_view, string_view>>>
{
    FO_STACK_TRACE_ENTRY();

    const size_t count = _sectionKeyValues.count(section_name);
    auto it = _sectionKeyValues.find(section_name);

    vector<ptr<map<string_view, string_view>>> key_values;
    key_values.reserve(count);

    for (size_t i = 0; i < count; i++, ++it) {
        key_values.emplace_back(&it->second);
    }

    return key_values;
}

auto ConfigFile::GetSections() noexcept -> ptr<multimap<string_view, map<string_view, string_view>>>
{
    FO_STACK_TRACE_ENTRY();

    return &_sectionKeyValues;
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

auto ConfigFile::GetSectionKeyValues(string_view section_name) noexcept -> nptr<const map<string_view, string_view>>
{
    FO_STACK_TRACE_ENTRY();

    const auto it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return nullptr;
    }

    return &it_section->second;
}

auto ConfigFile::GetSectionContent(string_view section_name) const -> string_view
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsEnumSet(_options, ConfigFileOption::CollectContent), "Config file content collection was not enabled");

    const auto it_section = _sectionKeyValues.find(section_name);

    if (it_section == _sectionKeyValues.end()) {
        return {};
    }

    const auto it_key = it_section->second.find(string_view {});

    if (it_key == it_section->second.end()) {
        return {};
    }

    return it_key->second;
}

FO_END_NAMESPACE
