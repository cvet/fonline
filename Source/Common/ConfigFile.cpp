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

#include "ConfigFile.h"
#include "StringUtils.h"

ConfigFile::ConfigFile(string_view str, NameResolver& name_resolver) : _nameResolver {&name_resolver}
{
    ParseStr(str);
}

ConfigFile::ConfigFile(string_view str, std::nullptr_t) : _nameResolver {nullptr}
{
    ParseStr(str);
}

void ConfigFile::CollectContent()
{
    _collectContent = true;
}

void ConfigFile::AppendData(string_view str)
{
    ParseStr(str);
}

void ConfigFile::ParseStr(string_view str)
{
    map<string, string>* cur_app = nullptr;

    auto it_app = _appKeyValues.find("");
    if (it_app == _appKeyValues.end()) {
        auto it = _appKeyValues.insert(std::make_pair("", map<string, string>()));
        _appKeyValuesOrder.push_back(it);
        cur_app = &it->second;
    }
    else {
        cur_app = &it_app->second;
    }

    string app_content;
    if (_collectContent) {
        app_content.reserve(0xFFFF);
    }

    const auto str_ = string(str);
    istringstream istr(str_);
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
            // Parse name
            auto end = line.find(']');
            if (end == string::npos) {
                continue;
            }

            string buf = _str(line.substr(1, end - 1)).trim();
            if (buf.empty()) {
                continue;
            }

            // Store current section content
            if (_collectContent) {
                (*cur_app)[""] = app_content;
                app_content.clear();
            }

            // Add new section
            auto it = _appKeyValues.insert(std::make_pair(buf, map<string, string>()));
            _appKeyValuesOrder.push_back(it);
            cur_app = &it->second;
        }
        // Section content
        else {
            // Store raw content
            if (_collectContent) {
                app_content.append(line).append("\n");
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
                        num = _str(str2).isNumber() ? _str(str2).toInt() : _nameResolver->ToHashedString(str2).as_int();
                    }
                    else if (i == 1 && num != 0u) {
                        num += !str2.empty() ? (_str(str2).isNumber() ? _str(str2).toInt() : _nameResolver->ToHashedString(str2).as_int()) : 0;
                    }
                    else if (i == 2 && num != 0u) {
                        (*cur_app)[_str("{}", num)] = str2;
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
                auto separator = line.find('=');
                if (separator != string::npos) {
                    string key = _str(line.substr(0, separator)).trim();
                    string value = _str(line.substr(separator + 1)).trim();

                    if (!key.empty()) {
                        (*cur_app)[key] = value;
                    }
                }
            }
        }
    }
    RUNTIME_ASSERT(istr.eof());

    // Store current section content
    if (_collectContent) {
        (*cur_app)[""] = app_content;
    }
}

auto ConfigFile::SerializeData() -> string
{
    size_t str_size = 32;

    for (auto& app_it : _appKeyValuesOrder) {
        const auto& [app_name, app_kv] = *app_it;
        if (!app_name.empty()) {
            str_size += app_name.size() + 3;
        }

        for (const auto& [key, value] : app_kv) {
            if (!key.empty()) {
                str_size += key.size() + value.size() + 4;
            }
        }

        str_size++;
    }

    string str;
    str.reserve(str_size);

    for (auto& app_it : _appKeyValuesOrder) {
        const auto& [app_name, app_kv] = *app_it;
        if (!app_name.empty()) {
            str += _str("[{}]\n", app_name);
        }

        for (const auto& [key, value] : app_kv) {
            if (!key.empty()) {
                str += _str("{} = {}\n", key, value);
            }
        }

        str += "\n";
    }

    return str;
}

auto ConfigFile::GetRawValue(string_view app_name, string_view key_name) const -> const string*
{
    const auto it_app = _appKeyValues.find(string(app_name));
    if (it_app == _appKeyValues.end()) {
        return nullptr;
    }

    const auto it_key = it_app->second.find(string(key_name));
    if (it_key == it_app->second.end()) {
        return nullptr;
    }

    return &it_key->second;
}

auto ConfigFile::GetStr(string_view app_name, string_view key_name) const -> string
{
    const auto* str = GetRawValue(app_name, key_name);
    return str != nullptr ? *str : "";
}

auto ConfigFile::GetStr(string_view app_name, string_view key_name, string_view def_val) const -> string
{
    const auto* str = GetRawValue(app_name, key_name);
    return str != nullptr ? *str : string(def_val);
}

auto ConfigFile::GetInt(string_view app_name, string_view key_name) const -> int
{
    const auto* str = GetRawValue(app_name, key_name);
    if (str != nullptr && str->length() == "true"_len && _str(*str).compareIgnoreCase("true")) {
        return 1;
    }
    if (str != nullptr && str->length() == "false"_len && _str(*str).compareIgnoreCase("false")) {
        return 0;
    }
    return str != nullptr ? _str(*str).toInt() : 0;
}

auto ConfigFile::GetInt(string_view app_name, string_view key_name, int def_val) const -> int
{
    const auto* str = GetRawValue(app_name, key_name);
    if (str != nullptr && str->length() == "true"_len && _str(*str).compareIgnoreCase("true")) {
        return 1;
    }
    if (str != nullptr && str->length() == "false"_len && _str(*str).compareIgnoreCase("false")) {
        return 0;
    }
    return str != nullptr ? _str(*str).toInt() : def_val;
}

void ConfigFile::SetStr(string_view app_name, string_view key_name, string_view val)
{
    const auto it_app = _appKeyValues.find(string(app_name));
    if (it_app == _appKeyValues.end()) {
        map<string, string> key_values;
        key_values[string(key_name)] = val;
        const auto it = _appKeyValues.insert(std::make_pair(app_name, key_values));
        _appKeyValuesOrder.push_back(it);
    }
    else {
        it_app->second[string(key_name)] = val;
    }
}

void ConfigFile::SetInt(string_view app_name, string_view key_name, int val)
{
    SetStr(app_name, key_name, _str("{}", val));
}

auto ConfigFile::GetApp(string_view app_name) const -> const map<string, string>&
{
    const auto it = _appKeyValues.find(string(app_name));
    RUNTIME_ASSERT(it != _appKeyValues.end());
    return it->second;
}

auto ConfigFile::GetApps(string_view app_name) -> vector<map<string, string>*>
{
    const auto count = _appKeyValues.count(string(app_name));
    auto it = _appKeyValues.find(string(app_name));

    vector<map<string, string>*> key_values;
    key_values.reserve(key_values.size() + count);

    for (size_t i = 0; i < count; i++, ++it) {
        key_values.push_back(&it->second);
    }
    return key_values;
}

auto ConfigFile::SetApp(string_view app_name) -> map<string, string>&
{
    const auto it = _appKeyValues.insert(std::make_pair(app_name, map<string, string>()));
    _appKeyValuesOrder.push_back(it);
    return it->second;
}

auto ConfigFile::IsApp(string_view app_name) const -> bool
{
    const auto it_app = _appKeyValues.find(string(app_name));
    return it_app != _appKeyValues.end();
}

auto ConfigFile::IsKey(string_view app_name, string_view key_name) const -> bool
{
    const auto it_app = _appKeyValues.find(string(app_name));
    if (it_app == _appKeyValues.end()) {
        return false;
    }
    return it_app->second.find(string(key_name)) != it_app->second.end();
}

auto ConfigFile::GetAppNames() const -> set<string>
{
    set<string> apps;
    for (const auto& [key, value] : _appKeyValues) {
        apps.insert(key);
    }
    return apps;
}

void ConfigFile::GotoNextApp(string_view app_name)
{
    const auto it_app = _appKeyValues.find(string(app_name));
    if (it_app == _appKeyValues.end()) {
        return;
    }

    const auto it = std::find(_appKeyValuesOrder.begin(), _appKeyValuesOrder.end(), it_app);
    RUNTIME_ASSERT(it != _appKeyValuesOrder.end());
    _appKeyValuesOrder.erase(it);
    _appKeyValues.erase(it_app);
}

auto ConfigFile::GetAppKeyValues(string_view app_name) -> const map<string, string>*
{
    const auto it_app = _appKeyValues.find(string(app_name));
    return it_app != _appKeyValues.end() ? &it_app->second : nullptr;
}

auto ConfigFile::GetAppContent(string_view app_name) -> string
{
    RUNTIME_ASSERT(_collectContent);

    const auto it_app = _appKeyValues.find(string(app_name));
    if (it_app == _appKeyValues.end()) {
        return "";
    }

    const auto it_key = it_app->second.find("");
    return it_key != it_app->second.end() ? it_key->second : "";
}
