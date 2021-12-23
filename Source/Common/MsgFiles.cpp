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

#include "MsgFiles.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"

struct MsgFilesData
{
    string TextMsgFileName[TEXTMSG_COUNT] {
        "FOTEXT",
        "FODLG",
        "FOOBJ",
        "FOGAME",
        "FOGM",
        "FOCOMBAT",
        "FOQUEST",
        "FOHOLO",
        "FOINTERNAL",
        "FOLOCATIONS",
    };
};
GLOBAL_DATA(MsgFilesData, Data);

static void HexToStr(uchar hex, char* str)
{
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
    for (const auto& [key, value] : other._strData) {
        EraseStr(key);
        AddStr(key, value);
    }
    return *this;
}

void FOMsg::AddStr(uint num, string_view str)
{
    _strData.insert(std::make_pair(num, str));
}

void FOMsg::AddBinary(uint num, const uchar* binary, uint len)
{
    vector<char> str;
    str.resize(len * 2 + 1);

    size_t str_cur = 0;
    for (uint i = 0; i < len; i++) {
        HexToStr(binary[i], &str[str_cur]);
        str_cur += 2;
    }

    AddStr(num, static_cast<char*>(&str[0]));
}

auto FOMsg::GetStr(uint num) const -> string
{
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
    const auto it = _strData.upper_bound(num);
    if (it == _strData.end()) {
        return 0;
    }
    return it->first;
}

auto FOMsg::GetStrNumLower(uint num) const -> uint
{
    const auto it = _strData.lower_bound(num);
    if (it == _strData.end()) {
        return 0;
    }
    return it->first;
}

auto FOMsg::GetInt(uint num) const -> int
{
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
    vector<uchar> result;

    if (Count(num) == 0u) {
        return result;
    }

    auto str = GetStr(num);
    const auto len = static_cast<uint>(str.length()) / 2;
    result.resize(len);
    for (uint i = 0; i < len; i++) {
        result[i] = StrToHex(&str[i * 2]);
    }

    return result;
}

auto FOMsg::Count(uint num) const -> uint
{
    return static_cast<uint>(_strData.count(num));
}

void FOMsg::EraseStr(uint num)
{
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
    return static_cast<uint>(_strData.size());
}

auto FOMsg::IsIntersects(const FOMsg& other) const -> bool
{
    for (auto& [key, value] : _strData) {
        if (other._strData.count(key) != 0u) {
            return true;
        }
    }
    return false;
}

auto FOMsg::GetBinaryData() const -> vector<uchar>
{
    // Fill raw data
    auto count = static_cast<uint>(_strData.size());

    vector<uchar> data;
    data.resize(sizeof(count));
    std::memcpy(&data[0], &count, sizeof(count));

    for (auto& [num, str] : _strData) {
        auto str_len = static_cast<uint>(str.length());

        data.resize(data.size() + sizeof(num) + sizeof(str_len) + str_len);
        std::memcpy(&data[data.size() - (sizeof(num) + sizeof(str_len) + str_len)], &num, sizeof(num));
        std::memcpy(&data[data.size() - (sizeof(str_len) + str_len)], &str_len, sizeof(str_len));
        if (str_len != 0u) {
            std::memcpy(&data[data.size() - str_len], str.c_str(), str_len);
        }
    }

    // Compress
    return Compressor::Compress(data);
}

auto FOMsg::LoadFromBinaryData(const vector<uchar>& data) -> bool
{
    Clear();

    // Uncompress
    auto uncompressed_data = Compressor::Uncompress(data, 10);
    if (uncompressed_data.empty()) {
        return false;
    }

    // Read count of strings
    const uchar* buf = &uncompressed_data[0];
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
            std::memcpy(&str[0], buf, str_len);
        }
        buf += str_len;

        AddStr(num, str);
    }

    return true;
}

auto FOMsg::LoadFromString(string_view str) -> bool
{
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
                num = _str(substr).isNumber() ? _str(substr).toInt() : _str(substr).toHash().Value;
            }
            else if (i == 1 && num != 0u) {
                num += !substr.empty() ? (_str(substr).isNumber() ? _str(substr).toInt() : _str(substr).toHash().Value) : 0;
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
    for (const auto& [key, value] : kv) {
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

void LanguagePack::LoadFromFiles(FileManager& file_mngr, string_view lang_name)
{
    RUNTIME_ASSERT(lang_name.length() == sizeof(NameCode));
    Name = lang_name;
    NameCode = *reinterpret_cast<const uint*>(lang_name.data());

    auto fail = false;

    auto msg_files = file_mngr.FilterFiles("msg");
    while (msg_files.MoveNext()) {
        auto msg_file = msg_files.GetCurFile();

        // Check pattern '...Texts/lang/file'
        auto dirs = _str(msg_file.GetPath()).split('/');
        if (dirs.size() >= 3 && dirs[dirs.size() - 3] == "Texts" && dirs[dirs.size() - 2] == lang_name) {
            for (auto i = 0; i < TEXTMSG_COUNT; i++) {
                if (_str(Data->TextMsgFileName[i]).compareIgnoreCase(msg_file.GetName())) {
                    if (!Msg[i].LoadFromString(msg_file.GetCStr())) {
                        WriteLog("Invalid MSG file '{}'.\n", msg_file.GetPath());
                        fail = true;
                    }
                    break;
                }
            }
        }
    }

    if (Msg[TEXTMSG_GAME].GetSize() == 0) {
        WriteLog("Unable to load '{}' from file.\n", Data->TextMsgFileName[TEXTMSG_GAME]);
    }

    IsAllMsgLoaded = Msg[TEXTMSG_GAME].GetSize() > 0 && !fail;
}

void LanguagePack::LoadFromCache(CacheStorage& cache, string_view lang_name)
{
    RUNTIME_ASSERT(lang_name.length() == sizeof(NameCode));
    Name = lang_name;
    NameCode = *reinterpret_cast<const uint*>(lang_name.data());

    auto errors = 0;
    for (auto i = 0; i < TEXTMSG_COUNT; i++) {
        uint buf_len = 0;
        auto* buf = cache.GetRawData(GetMsgCacheName(i), buf_len);
        if (buf != nullptr) {
            vector<uchar> data;
            data.resize(buf_len);
            std::memcpy(&data[0], buf, buf_len);
            delete[] buf;

            if (!Msg[i].LoadFromBinaryData(data)) {
                errors++;
            }
        }
        else {
            errors++;
        }
    }

    if (errors != 0) {
        WriteLog("Cached language '{}' not found.\n", lang_name);
    }

    IsAllMsgLoaded = errors == 0;
}

auto LanguagePack::GetMsgCacheName(int msg_num) const -> string
{
    return _str("${}-{}.cache", Name, Data->TextMsgFileName[msg_num]);
}
