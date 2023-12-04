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

#include "MsgFiles.h"
#include "DiskFileSystem.h"
#include "FileSystem.h"
#include "GenericUtils.h"
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

void FOMsg::AddStr(uint num, string_view str)
{
    STACK_TRACE_ENTRY();

    _strData.emplace(num, string(str));
}

auto FOMsg::GetStr(uint num) const -> const string&
{
    STACK_TRACE_ENTRY();

    const size_t str_count = _strData.count(num);
    auto it = _strData.find(num);

    switch (str_count) {
    case 0:
        return _emptyStr;

    case 1:
        break;

    default:
        const int random_skip = GenericUtils::Random(0, static_cast<int>(str_count)) - 1;

        for (int i = 0; i < random_skip; i++) {
            ++it;
        }

        break;
    }

    return it->second;
}

auto FOMsg::GetStr(uint num, size_t skip) const -> const string&
{
    STACK_TRACE_ENTRY();

    const size_t str_count = _strData.count(num);
    auto it = _strData.find(num);

    if (skip >= str_count) {
        return _emptyStr;
    }

    for (uint i = 0; i < skip; i++) {
        ++it;
    }

    return it->second;
}

auto FOMsg::GetStrNumUpper(uint num) const -> uint
{
    STACK_TRACE_ENTRY();

    const auto it = _strData.upper_bound(num);

    if (it == _strData.end()) {
        return 0;
    }

    return it->first;
}

auto FOMsg::GetStrNumLower(uint num) const -> uint
{
    STACK_TRACE_ENTRY();

    const auto it = _strData.lower_bound(num);

    if (it == _strData.end()) {
        return 0;
    }

    return it->first;
}

auto FOMsg::GetStrCount(uint num) const -> size_t
{
    STACK_TRACE_ENTRY();

    return _strData.count(num);
}

void FOMsg::EraseStr(uint num)
{
    STACK_TRACE_ENTRY();

    _strData.erase(num);
}

void FOMsg::Merge(const FOMsg& other)
{
    STACK_TRACE_ENTRY();

    for (auto&& [key, value] : other._strData) {
        AddStr(key, value);
    }
}

auto FOMsg::GetSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    return _strData.size();
}

auto FOMsg::IsIntersects(const FOMsg& other) const -> bool
{
    STACK_TRACE_ENTRY();

    for (auto&& [key, value] : _strData) {
        if (other._strData.count(key) != 0) {
            return true;
        }
    }

    return false;
}

auto FOMsg::GetBinaryData() const -> vector<uint8>
{
    STACK_TRACE_ENTRY();

    vector<uint8> data;
    auto writer = DataWriter {data};

    writer.Write<uint>(static_cast<uint>(_strData.size()));

    for (auto&& [num, str] : _strData) {
        writer.Write<uint>(num);
        writer.Write<uint>(static_cast<uint>(str.length()));
        writer.WritePtr(str.data(), str.length());
    }

    return data;
}

auto FOMsg::LoadFromBinaryData(const vector<uint8>& data) -> bool
{
    STACK_TRACE_ENTRY();

    auto reader = DataReader {data};

    const auto count = reader.Read<uint>();
    string str;

    for (uint i = 0; i < count; i++) {
        const auto num = reader.Read<uint>();
        const auto str_len = reader.Read<uint>();

        if (str_len != 0) {
            str.resize(str_len);
            reader.ReadPtr(str.data(), str_len);
        }
        else {
            str.resize(0);
        }

        AddStr(num, str);
    }

    return true;
}

auto FOMsg::LoadFromString(const string& str, HashResolver& hash_resolver) -> bool
{
    STACK_TRACE_ENTRY();

    auto failed = false;

    istringstream sstr(str);
    string line;

    while (std::getline(sstr, line, '\n')) {
        uint num = 0;
        size_t offset = 0;

        for (auto i = 0; i < 3; i++) {
            const auto first = line.find('{', offset);
            auto last = line.find('}', first);

            if (first == string::npos || last == string::npos) {
                if (i == 2 && first != string::npos) {
                    string additional_line;
                    while (last == string::npos && std::getline(sstr, additional_line, '\n')) {
                        line += "\n" + additional_line;
                        last = line.find('}', first);
                    }
                }

                if (first == string::npos || last == string::npos) {
                    if (i > 0 || first != string::npos) {
                        failed = true;
                    }

                    break;
                }
            }

            auto substr = line.substr(first + 1, last - first - 1);
            offset = last + 1;

            if (i == 0 && num == 0) {
                num = _str(substr).isNumber() ? _str(substr).toInt() : hash_resolver.ToHashedString(substr).as_int();
            }
            else if (i == 1 && num != 0) {
                num += !substr.empty() ? (_str(substr).isNumber() ? _str(substr).toInt() : hash_resolver.ToHashedString(substr).as_int()) : 0;
            }
            else if (i == 2 && num != 0) {
                AddStr(num, substr);
            }
            else {
                failed = true;
            }
        }
    }

    return !failed;
}

void FOMsg::LoadFromMap(const map<string, string>& kv)
{
    STACK_TRACE_ENTRY();

    for (auto&& [key, value] : kv) {
        const auto num = _str(key).toUInt();

        if (num != 0) {
            AddStr(num, value);
        }
    }
}

void FOMsg::Clear()
{
    STACK_TRACE_ENTRY();

    _strData.clear();
}

auto FOMsg::GetMsgType(string_view type_name) -> int
{
    STACK_TRACE_ENTRY();

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

void LanguagePack::ParseTexts(FileSystem& resources, HashResolver& hash_resolver, string_view lang_name)
{
    STACK_TRACE_ENTRY();

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
                    if (!Msg[i].LoadFromString(msg_file.GetStr(), hash_resolver)) {
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
    STACK_TRACE_ENTRY();

    for (auto i = 0; i < TEXTMSG_COUNT; i++) {
        auto file = DiskFileSystem::OpenFile(_str("{}/{}-{}.fotxtb", dir, Name, Data->TextMsgFileName[i]), true);
        RUNTIME_ASSERT(file);

        const auto write_file_ok = file.Write(Msg[i].GetBinaryData());
        RUNTIME_ASSERT(write_file_ok);
    }
}

void LanguagePack::LoadTexts(FileSystem& resources, string_view lang_name)
{
    STACK_TRACE_ENTRY();

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
                }
            }
        }
    }

    if (Msg[TEXTMSG_GAME].GetSize() == 0) {
        throw LanguagePackException("Unable to load game texts from file", lang_name);
    }
}
