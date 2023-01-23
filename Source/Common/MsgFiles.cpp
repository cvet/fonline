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

#include "MsgFiles.h"
#include "DiskFileSystem.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"

struct MsgFilesData
{
    string TextMsgFileName[TEXTMSG_COUNT] {
        "text",
        "_dialogs",
        "_items",
        "game",
        "worldmap",
        "combat",
        "quest",
        "holo",
        "_internal",
        "_locations",
    };
};
GLOBAL_DATA(MsgFilesData, Data);

static void HexToStr(uchar hex, char* str)
{
    PROFILER_ENTRY();

    for (auto i = 0; i < 2; i++) {
        const auto val = i == 0 ? hex >> 4 : hex & 0xF;
        if (val < 10) {
            *str++ = static_cast<char>('0' + val);
        }
        else {
            *str++ = static_cast<char>('A' + val - 10);
        }
    }
}

static auto StrToHex(const char* str) -> uchar
{
    PROFILER_ENTRY();

    uchar result = 0;
    for (auto i = 0; i < 2; i++) {
        const auto c = *str++;
        if (c < 'A') {
            result |= (c - '0') << (i == 0 ? 4 : 0);
        }
        else {
            result |= (c - 'A' + 10) << (i == 0 ? 4 : 0);
        }
    }
    return result;
}

auto FOMsg::operator+=(const FOMsg& other) -> FOMsg&
{
    PROFILER_ENTRY();

    for (auto&& [key, value] : other._strData) {
        EraseStr(key);
        AddStr(key, value);
    }
    return *this;
}

void FOMsg::AddStr(uint num, string_view str)
{
    PROFILER_ENTRY();

    _strData.insert(std::make_pair(num, str));
}

void FOMsg::AddBinary(uint num, const uchar* binary, uint len)
{
    PROFILER_ENTRY();

    vector<char> str;
    str.resize(len * 2 + 1);

    size_t str_cur = 0;
    for (uint i = 0; i < len; i++) {
        HexToStr(binary[i], &str[str_cur]);
        str_cur += 2;
    }

    AddStr(num, static_cast<char*>(str.data()));
}

auto FOMsg::GetStr(uint num) const -> string
{
    PROFILER_ENTRY();

    const auto str_count = static_cast<uint>(_strData.count(num));
    auto it = _strData.find(num);

    switch (str_count) {
    case 0:
        return "";
    case 1:
        break;
    default:
        for (auto i = 0, j = GenericUtils::Random(0, static_cast<int>(str_count)) - 1; i < j; i++) {
            ++it;
        }
        break;
    }

    return it->second;
}

auto FOMsg::GetStr(uint num, uint skip) const -> string
{
    PROFILER_ENTRY();

    const auto str_count = static_cast<uint>(_strData.count(num));
    auto it = _strData.find(num);

    if (skip >= str_count) {
        return "";
    }
    for (uint i = 0; i < skip; i++) {
        ++it;
    }

    return it->second;
}

auto FOMsg::GetStrNumUpper(uint num) const -> uint
{
    PROFILER_ENTRY();

    const auto it = _strData.upper_bound(num);
    if (it == _strData.end()) {
        return 0;
    }
    return it->first;
}

auto FOMsg::GetStrNumLower(uint num) const -> uint
{
    PROFILER_ENTRY();

    const auto it = _strData.lower_bound(num);
    if (it == _strData.end()) {
        return 0;
    }
    return it->first;
}

auto FOMsg::GetInt(uint num) const -> int
{
    PROFILER_ENTRY();

    const auto str_count = static_cast<uint>(_strData.count(num));
    auto it = _strData.find(num);

    switch (str_count) {
    case 0:
        return -1;
    case 1:
        break;
    default:
        for (auto i = 0, j = GenericUtils::Random(0, static_cast<int>(str_count)) - 1; i < j; i++) {
            ++it;
        }
        break;
    }

    return _str(it->second).toInt();
}

auto FOMsg::GetBinary(uint num) const -> vector<uchar>
{
    PROFILER_ENTRY();

    vector<uchar> result;

    if (Count(num) == 0u) {
        return result;
    }

    const auto str = GetStr(num);
    const auto len = static_cast<uint>(str.length()) / 2;
    result.resize(len);
    for (uint i = 0; i < len; i++) {
        result[i] = StrToHex(&str[i * 2]);
    }

    return result;
}

auto FOMsg::Count(uint num) const -> uint
{
    PROFILER_ENTRY();

    return static_cast<uint>(_strData.count(num));
}

void FOMsg::EraseStr(uint num)
{
    PROFILER_ENTRY();

    while (true) {
        auto it = _strData.find(num);
        if (it != _strData.end()) {
            _strData.erase(it);
        }
        else {
            break;
        }
    }
}

auto FOMsg::GetSize() const -> uint
{
    PROFILER_ENTRY();

    return static_cast<uint>(_strData.size());
}

auto FOMsg::IsIntersects(const FOMsg& other) const -> bool
{
    PROFILER_ENTRY();

    for (auto&& [key, value] : _strData) {
        if (other._strData.count(key) != 0u) {
            return true;
        }
    }
    return false;
}

auto FOMsg::GetBinaryData() const -> vector<uchar>
{
    PROFILER_ENTRY();

    // Fill raw data
    const auto count = static_cast<uint>(_strData.size());

    vector<uchar> data;
    data.resize(sizeof(count));
    std::memcpy(data.data(), &count, sizeof(count));

    for (auto&& [num, str] : _strData) {
        auto str_len = static_cast<uint>(str.length());

        data.resize(data.size() + sizeof(num) + sizeof(str_len) + str_len);
        std::memcpy(&data[data.size() - (sizeof(num) + sizeof(str_len) + str_len)], &num, sizeof(num));
        std::memcpy(&data[data.size() - (sizeof(str_len) + str_len)], &str_len, sizeof(str_len));
        if (str_len != 0u) {
            std::memcpy(&data[data.size() - str_len], str.c_str(), str_len);
        }
    }

    // Compress
    return data;
}

auto FOMsg::LoadFromBinaryData(const vector<uchar>& data) -> bool
{
    PROFILER_ENTRY();

    Clear();

    // Read count of strings
    const uchar* buf = data.data();
    uint count = 0;
    std::memcpy(&count, buf, sizeof(count));
    buf += sizeof(count);

    // Read strings
    uint num = 0;
    uint str_len = 0;
    string str;
    for (uint i = 0; i < count; i++) {
        std::memcpy(&num, buf, sizeof(num));
        buf += sizeof(num);

        std::memcpy(&str_len, buf, sizeof(str_len));
        buf += sizeof(str_len);

        str.resize(str_len);
        if (str_len != 0u) {
            std::memcpy(str.data(), buf, str_len);
        }
        buf += str_len;

        AddStr(num, str);
    }

    return true;
}

auto FOMsg::LoadFromString(string_view str, NameResolver& name_resolver) -> bool
{
    PROFILER_ENTRY();

    auto fail = false;

    const auto sstr = string(str);
    istringstream istr(sstr);
    string line;
    while (std::getline(istr, line, '\n')) {
        uint num = 0;
        size_t offset = 0;
        for (auto i = 0; i < 3; i++) {
            const auto first = line.find('{', offset);
            auto last = line.find('}', first);
            if (first == string::npos || last == string::npos) {
                if (i == 2 && first != string::npos) {
                    string additional_line;
                    while (last == string::npos && std::getline(istr, additional_line, '\n')) {
                        line += "\n" + additional_line;
                        last = line.find('}', first);
                    }
                }

                if (first == string::npos || last == string::npos) {
                    if (i > 0 || first != string::npos) {
                        fail = true;
                    }
                    break;
                }
            }

            auto substr = line.substr(first + 1, last - first - 1);
            offset = last + 1;
            if (i == 0 && num == 0u) {
                num = _str(substr).isNumber() ? _str(substr).toInt() : name_resolver.ToHashedString(substr).as_int();
            }
            else if (i == 1 && num != 0u) {
                num += !substr.empty() ? (_str(substr).isNumber() ? _str(substr).toInt() : name_resolver.ToHashedString(substr).as_int()) : 0;
            }
            else if (i == 2 && num != 0u) {
                AddStr(num, substr);
            }
            else {
                fail = true;
            }
        }
    }

    return !fail;
}

void FOMsg::LoadFromMap(const map<string, string>& kv)
{
    PROFILER_ENTRY();

    for (auto&& [key, value] : kv) {
        const auto num = _str(key).toUInt();
        if (num != 0u) {
            AddStr(num, value);
        }
    }
}

void FOMsg::Clear()
{
    _strData.clear();
}

auto FOMsg::GetMsgType(string_view type_name) -> int
{
    PROFILER_ENTRY();

    if (_str(type_name).compareIgnoreCase("text")) {
        return TEXTMSG_TEXT;
    }
    else if (_str(type_name).compareIgnoreCase("dlg")) {
        return TEXTMSG_DLG;
    }
    else if (_str(type_name).compareIgnoreCase("item")) {
        return TEXTMSG_ITEM;
    }
    else if (_str(type_name).compareIgnoreCase("obj")) {
        return TEXTMSG_ITEM;
    }
    else if (_str(type_name).compareIgnoreCase("game")) {
        return TEXTMSG_GAME;
    }
    else if (_str(type_name).compareIgnoreCase("gm")) {
        return TEXTMSG_GM;
    }
    else if (_str(type_name).compareIgnoreCase("combat")) {
        return TEXTMSG_COMBAT;
    }
    else if (_str(type_name).compareIgnoreCase("quest")) {
        return TEXTMSG_QUEST;
    }
    else if (_str(type_name).compareIgnoreCase("holo")) {
        return TEXTMSG_HOLO;
    }
    else if (_str(type_name).compareIgnoreCase("internal")) {
        return TEXTMSG_INTERNAL;
    }
    return -1;
}

void LanguagePack::ParseTexts(FileSystem& resources, NameResolver& name_resolver, string_view lang_name)
{
    PROFILER_ENTRY();

    RUNTIME_ASSERT(lang_name.length() == sizeof(NameCode));
    Name = lang_name;
    NameCode = *reinterpret_cast<const uint*>(lang_name.data());

    auto msg_files = resources.FilterFiles("fotxt");
    while (msg_files.MoveNext()) {
        auto msg_file = msg_files.GetCurFile();

        auto name = msg_file.GetName();
        RUNTIME_ASSERT(name.length() > 5);
        RUNTIME_ASSERT(name[4] == '-');

        if (name.substr(0, 4) == lang_name) {
            for (auto i = 0; i < TEXTMSG_COUNT; i++) {
                if (Data->TextMsgFileName[i] == name.substr(5)) {
                    if (!Msg[i].LoadFromString(msg_file.GetStr(), name_resolver)) {
                        throw LanguagePackException("Invalid text file", msg_file.GetPath());
                    }
                }
            }
        }
    }

    if (Msg[TEXTMSG_GAME].GetSize() == 0) {
        throw LanguagePackException("Unable to load game texts from file", lang_name);
    }
}

void LanguagePack::SaveTextsToDisk(string_view dir) const
{
    PROFILER_ENTRY();

    for (auto i = 0; i < TEXTMSG_COUNT; i++) {
        auto file = DiskFileSystem::OpenFile(_str("{}/{}-{}.fotxtb", dir, Name, Data->TextMsgFileName[i]), true);
        RUNTIME_ASSERT(file);
        const auto write_file_ok = file.Write(Msg[i].GetBinaryData());
        RUNTIME_ASSERT(write_file_ok);
    }
}

void LanguagePack::LoadTexts(FileSystem& resources, string_view lang_name)
{
    PROFILER_ENTRY();

    RUNTIME_ASSERT(lang_name.length() == sizeof(NameCode));
    Name = lang_name;
    NameCode = *reinterpret_cast<const uint*>(lang_name.data());

    auto msg_files = resources.FilterFiles("fotxtb");
    while (msg_files.MoveNext()) {
        auto msg_file = msg_files.GetCurFile();

        auto name = msg_file.GetName();
        RUNTIME_ASSERT(name.length() > 5);
        RUNTIME_ASSERT(name[4] == '-');

        if (name.substr(0, 4) == lang_name) {
            for (auto i = 0; i < TEXTMSG_COUNT; i++) {
                if (Data->TextMsgFileName[i] == name.substr(5)) {
                    if (!Msg[i].LoadFromBinaryData(msg_file.GetData())) {
                        throw LanguagePackException("Invalid text file", msg_file.GetPath());
                    }

                    WriteLog("Loaded {} texts for language '{}' from '{}'", Msg[i].GetSize(), lang_name, msg_file.GetPath());
                }
            }
        }
    }

    if (Msg[TEXTMSG_GAME].GetSize() == 0) {
        throw LanguagePackException("Unable to load game texts from file", lang_name);
    }
}
