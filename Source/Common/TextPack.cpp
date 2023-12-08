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

#include "TextPack.h"
#include "DiskFileSystem.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "StringUtils.h"

void TextPack::AddStr(TextPackKey num, string_view str)
{
    STACK_TRACE_ENTRY();

    _strData.emplace(num, string(str));
}

auto TextPack::GetStr(TextPackKey num) const -> const string&
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

auto TextPack::GetStr(TextPackKey num, size_t skip) const -> const string&
{
    STACK_TRACE_ENTRY();

    const size_t str_count = _strData.count(num);
    auto it = _strData.find(num);

    if (skip >= str_count) {
        return _emptyStr;
    }

    for (size_t i = 0; i < skip; i++) {
        ++it;
    }

    return it->second;
}

auto TextPack::GetStrNumUpper(TextPackKey num) const -> TextPackKey
{
    STACK_TRACE_ENTRY();

    const auto it = _strData.upper_bound(num);

    if (it == _strData.end()) {
        return 0;
    }

    return it->first;
}

auto TextPack::GetStrNumLower(TextPackKey num) const -> TextPackKey
{
    STACK_TRACE_ENTRY();

    const auto it = _strData.lower_bound(num);

    if (it == _strData.end()) {
        return 0;
    }

    return it->first;
}

auto TextPack::GetStrCount(TextPackKey num) const -> size_t
{
    STACK_TRACE_ENTRY();

    return _strData.count(num);
}

void TextPack::EraseStr(TextPackKey num)
{
    STACK_TRACE_ENTRY();

    _strData.erase(num);
}

void TextPack::Merge(const TextPack& other)
{
    STACK_TRACE_ENTRY();

    for (auto&& [key, value] : other._strData) {
        AddStr(key, value);
    }
}

auto TextPack::GetSize() const -> size_t
{
    STACK_TRACE_ENTRY();

    return _strData.size();
}

auto TextPack::IsIntersects(const TextPack& other) const -> bool
{
    STACK_TRACE_ENTRY();

    for (auto&& [key, value] : _strData) {
        if (other._strData.count(key) != 0) {
            return true;
        }
    }

    return false;
}

auto TextPack::GetBinaryData() const -> vector<uint8>
{
    STACK_TRACE_ENTRY();

    vector<uint8> data;
    auto writer = DataWriter {data};

    writer.Write<uint>(static_cast<uint>(_strData.size()));

    for (auto&& [num, str] : _strData) {
        writer.Write<TextPackKey>(num);
        writer.Write<uint>(static_cast<uint>(str.length()));
        writer.WritePtr(str.data(), str.length());
    }

    return data;
}

auto TextPack::LoadFromBinaryData(const vector<uint8>& data) -> bool
{
    STACK_TRACE_ENTRY();

    auto reader = DataReader {data};

    const auto count = reader.Read<uint>();
    string str;

    for (uint i = 0; i < count; i++) {
        const auto num = reader.Read<TextPackKey>();
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

auto TextPack::LoadFromString(const string& str, HashResolver& hash_resolver) -> bool
{
    STACK_TRACE_ENTRY();

    auto failed = false;

    istringstream sstr(str);
    string line;

    while (std::getline(sstr, line, '\n')) {
        TextPackKey num = 0;
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

void TextPack::LoadFromMap(const map<string, string>& kv)
{
    STACK_TRACE_ENTRY();

    for (auto&& [key, value] : kv) {
        const TextPackKey num = _str(key).toUInt();

        if (num != 0) {
            AddStr(num, value);
        }
    }
}

void TextPack::Clear()
{
    STACK_TRACE_ENTRY();

    _strData.clear();
}

LanguagePack::LanguagePack(string_view lang_name, const NameResolver& name_resolver) :
    _langName {lang_name},
    _nameResolver {&name_resolver}
{
    STACK_TRACE_ENTRY();

    _textPacks.resize(std::numeric_limits<std::underlying_type_t<TextPackName>>::max() + 1);
    _textPacks[static_cast<size_t>(TextPackName::Game)] = std::make_unique<TextPack>();
    _textPacks[static_cast<size_t>(TextPackName::Dialogs)] = std::make_unique<TextPack>();
    _textPacks[static_cast<size_t>(TextPackName::Items)] = std::make_unique<TextPack>();
    _textPacks[static_cast<size_t>(TextPackName::Locations)] = std::make_unique<TextPack>();
}

auto LanguagePack::GetName() const -> const string&
{
    STACK_TRACE_ENTRY();

    return _langName;
}

auto LanguagePack::GetTextPack(TextPackName pack_name) const -> const TextPack&
{
    STACK_TRACE_ENTRY();

    const auto pack_index = static_cast<size_t>(pack_name);

    return _textPacks[pack_index] ? *_textPacks[pack_index] : _emptyPack;
}

auto LanguagePack::GetTextPackForEdit(TextPackName pack_name) -> TextPack&
{
    STACK_TRACE_ENTRY();

    const auto pack_index = static_cast<size_t>(pack_name);

    if (!_textPacks[pack_index]) {
        _textPacks[pack_index] = std::make_unique<TextPack>();
    }

    return *_textPacks[pack_index];
}

auto LanguagePack::ResolveTextPackName(string_view pack_name_str, bool* failed) const -> TextPackName
{
    STACK_TRACE_ENTRY();

    try {
        return static_cast<TextPackName>(_nameResolver->ResolveEnumValue("TextPackName", pack_name_str));
    }
    catch (const EnumResolveException&) {
        if (failed != nullptr) {
            *failed = true;
            return {};
        }

        throw LanguagePackException("Invalid text pack name", pack_name_str);
    }
}

void LanguagePack::ParseTexts(FileSystem& resources, HashResolver& hash_resolver)
{
    STACK_TRACE_ENTRY();

    auto msg_files = resources.FilterFiles("fotxt");

    while (msg_files.MoveNext()) {
        auto msg_file = msg_files.GetCurFile();
        const auto& msg_file_name = msg_file.GetName();

        const auto sep = msg_file_name.find('.');
        RUNTIME_ASSERT(sep != string::npos);

        string pack_name_str = msg_file_name.substr(0, sep);
        string lang_name = msg_file_name.substr(sep + 1);
        RUNTIME_ASSERT(!pack_name_str.empty());
        RUNTIME_ASSERT(!lang_name.empty());

        if (lang_name == _langName) {
            const auto pack_name = ResolveTextPackName(pack_name_str);
            auto& text_pack = GetTextPackForEdit(pack_name);

            if (!text_pack.LoadFromString(msg_file.GetStr(), hash_resolver)) {
                throw LanguagePackException("Invalid text file", msg_file.GetPath());
            }
        }
    }

    if (GetTextPack(TextPackName::Game).GetSize() == 0) {
        throw LanguagePackException("Unable to load game texts from file", _langName);
    }
}

void LanguagePack::SaveTextsToDisk(string_view dir) const
{
    STACK_TRACE_ENTRY();

    for (size_t i = 1; i < _textPacks.size(); i++) {
        auto&& text_pack = _textPacks[i];

        if (text_pack) {
            const string& pack_name_str = _nameResolver->ResolveEnumValueName("TextPackName", static_cast<int>(i));

            auto file = DiskFileSystem::OpenFile(_str("{}/{}.{}.fotxtb", dir, pack_name_str, _langName), true);
            RUNTIME_ASSERT(file);

            const auto write_file_ok = file.Write(text_pack->GetBinaryData());
            RUNTIME_ASSERT(write_file_ok);
        }
    }
}

void LanguagePack::LoadTexts(FileSystem& resources)
{
    STACK_TRACE_ENTRY();

    auto msg_files = resources.FilterFiles("fotxtb");

    while (msg_files.MoveNext()) {
        auto msg_file = msg_files.GetCurFile();

        const auto& msg_file_name = msg_file.GetName();

        const auto sep = msg_file_name.find('.');
        RUNTIME_ASSERT(sep != string::npos);

        string pack_name_str = msg_file_name.substr(0, sep);
        string lang_name = msg_file_name.substr(sep + 1);
        RUNTIME_ASSERT(!pack_name_str.empty());
        RUNTIME_ASSERT(!lang_name.empty());

        if (lang_name == _langName) {
            const auto pack_name = ResolveTextPackName(pack_name_str);
            auto& text_pack = GetTextPackForEdit(pack_name);

            if (!text_pack.LoadFromBinaryData(msg_file.GetData())) {
                throw LanguagePackException("Invalid binary text file", msg_file.GetPath());
            }
        }
    }

    if (GetTextPack(TextPackName::Game).GetSize() == 0) {
        throw LanguagePackException("Unable to load game texts from file", _langName);
    }
}
