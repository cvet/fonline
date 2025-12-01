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

#include "TextPack.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE();

void TextPack::AddStr(TextPackKey num, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    _strData.emplace(num, string(str));
}

void TextPack::AddStr(TextPackKey num, string&& str)
{
    FO_STACK_TRACE_ENTRY();

    _strData.emplace(num, std::move(str));
}

auto TextPack::GetStr(TextPackKey num) const -> const string&
{
    FO_STACK_TRACE_ENTRY();

    const size_t str_count = _strData.count(num);
    auto it = _strData.find(num);

    switch (str_count) {
    case 0:
        return _emptyStr;

    case 1:
        break;

    default:
        const int32 random_skip = GenericUtils::Random(0, numeric_cast<int32>(str_count)) - 1;

        for (int32 i = 0; i < random_skip; i++) {
            ++it;
        }

        break;
    }

    return it->second;
}

auto TextPack::GetStr(TextPackKey num, size_t skip) const -> const string&
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    const auto it = _strData.upper_bound(num);

    if (it == _strData.end()) {
        return 0;
    }

    return it->first;
}

auto TextPack::GetStrNumLower(TextPackKey num) const -> TextPackKey
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _strData.lower_bound(num);

    if (it == _strData.end()) {
        return 0;
    }

    return it->first;
}

auto TextPack::GetStrCount(TextPackKey num) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return _strData.count(num);
}

void TextPack::EraseStr(TextPackKey num)
{
    FO_STACK_TRACE_ENTRY();

    _strData.erase(num);
}

void TextPack::Merge(const TextPack& other)
{
    FO_STACK_TRACE_ENTRY();

    for (auto&& [key, value] : other._strData) {
        AddStr(key, value);
    }
}

void TextPack::FixStr(const TextPack& base_pack)
{
    FO_STACK_TRACE_ENTRY();

    // Add keys that are in the base pack but not in this pack
    for (auto&& [key, value] : base_pack._strData) {
        if (_strData.count(key) == 0) {
            AddStr(key, value);
        }
    }

    // Remove keys that are not in the base pack
    for (auto it = _strData.begin(); it != _strData.end();) {
        if (base_pack._strData.count(it->first) == 0) {
            it = _strData.erase(it);
        }
        else {
            ++it;
        }
    }
}

auto TextPack::GetSize() const noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return _strData.size();
}

auto TextPack::CheckIntersections(const TextPack& other) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool result = false;

    for (auto&& [key, value] : _strData) {
        if (other._strData.count(key) != 0) {
            WriteLog("Intersection of key {} (count {}) value 1 '{}', value 2 '{}'", key, other._strData.count(key), value, other._strData.find(key)->second);
            result = true;
        }
    }

    return result;
}

auto TextPack::GetBinaryData() const -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8> data;
    auto writer = DataWriter {data};

    writer.Write<uint32>(numeric_cast<uint32>(_strData.size()));

    for (auto&& [num, str] : _strData) {
        writer.Write<TextPackKey>(num);
        writer.Write<uint32>(numeric_cast<uint32>(str.length()));
        writer.WritePtr(str.data(), str.length());
    }

    return data;
}

auto TextPack::LoadFromBinaryData(const vector<uint8>& data) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto reader = DataReader {data};

    const auto count = reader.Read<uint32>();

    for (uint32 i = 0; i < count; i++) {
        const auto num = reader.Read<TextPackKey>();
        const auto str_len = reader.Read<uint32>();

        string str;

        if (str_len != 0) {
            str.resize(str_len);
            reader.ReadPtr(str.data(), str_len);
        }
        else {
            str.resize(0);
        }

        AddStr(num, std::move(str));
    }

    return true;
}

auto TextPack::LoadFromString(const string& str, HashResolver& hash_resolver) -> bool
{
    FO_STACK_TRACE_ENTRY();

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
                num = strex(substr).is_number() ? strex(substr).to_int32() : hash_resolver.ToHashedString(substr).as_int32();
            }
            else if (i == 1 && num != 0) {
                num += !substr.empty() ? (strex(substr).is_number() ? strex(substr).to_int32() : hash_resolver.ToHashedString(substr).as_int32()) : 0;
            }
            else if (i == 2 && num != 0) {
                AddStr(num, std::move(substr));
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
    FO_STACK_TRACE_ENTRY();

    for (auto&& [key, value] : kv) {
        const TextPackKey num = strex(key).to_uint32();

        if (num != 0) {
            AddStr(num, value);
        }
    }
}

void TextPack::Clear()
{
    FO_STACK_TRACE_ENTRY();

    _strData.clear();
}

void TextPack::FixPacks(span<const string> bake_languages, vector<pair<string, map<string, TextPack>>>& lang_packs)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!bake_languages.empty());

    // Add default language
    if (lang_packs.empty() || lang_packs.front().first != bake_languages.front()) {
        lang_packs.emplace(lang_packs.begin(), bake_languages.front(), map<string, TextPack>());
    }

    // Add missed languages
    for (const auto& lang : bake_languages) {
        if (std::ranges::find_if(lang_packs, [&](auto&& l) { return l.first == lang; }) == lang_packs.end()) {
            lang_packs.emplace_back(lang, map<string, TextPack>());
        }
    }

    // Remove unsupported languages
    for (auto it = lang_packs.begin(); it != lang_packs.end();) {
        if (std::ranges::find_if(bake_languages, [&](auto&& l) { return l == it->first; }) == bake_languages.end()) {
            it = lang_packs.erase(it);
        }
        else {
            ++it;
        }
    }

    // Normalize language packs
    const auto& base_lang_pack = lang_packs.front().second;

    for (size_t i = 1; i < lang_packs.size(); i++) {
        auto& lang_pack = lang_packs[i].second;

        // Remove packs that are not in the base language pack
        for (auto it = lang_pack.begin(); it != lang_pack.end();) {
            if (base_lang_pack.count(it->first) == 0) {
                it = lang_pack.erase(it);
            }
            else {
                ++it;
            }
        }

        // Add packs that are in the base language pack but not in this pack
        for (auto&& [pack_name, text_pack] : base_lang_pack) {
            if (lang_pack.count(pack_name) == 0) {
                lang_pack.emplace(pack_name, text_pack);
            }
        }

        FO_RUNTIME_ASSERT(lang_pack.size() == base_lang_pack.size());

        // Normalize texts to the base language
        for (auto&& [pack_name, text_pack] : lang_pack) {
            const auto it = base_lang_pack.find(pack_name);
            FO_RUNTIME_ASSERT(it != base_lang_pack.end());
            text_pack.FixStr(it->second);
        }
    }
}

LanguagePack::LanguagePack(string_view lang_name, const NameResolver& name_resolver) :
    _langName {lang_name},
    _nameResolver {&name_resolver}
{
    FO_STACK_TRACE_ENTRY();

    _textPacks.resize(std::numeric_limits<std::underlying_type_t<TextPackName>>::max() + 1);
    _textPacks[static_cast<size_t>(TextPackName::Items)] = SafeAlloc::MakeUnique<TextPack>();
    _textPacks[static_cast<size_t>(TextPackName::Critters)] = SafeAlloc::MakeUnique<TextPack>();
    _textPacks[static_cast<size_t>(TextPackName::Maps)] = SafeAlloc::MakeUnique<TextPack>();
    _textPacks[static_cast<size_t>(TextPackName::Locations)] = SafeAlloc::MakeUnique<TextPack>();
}

auto LanguagePack::GetName() const noexcept -> const string&
{
    FO_STACK_TRACE_ENTRY();

    return _langName;
}

auto LanguagePack::GetTextPack(TextPackName pack_name) const -> const TextPack&
{
    FO_STACK_TRACE_ENTRY();

    const auto pack_index = static_cast<size_t>(pack_name);
    FO_RUNTIME_ASSERT(pack_index < _textPacks.size());

    return _textPacks[pack_index] ? *_textPacks[pack_index] : _emptyPack;
}

auto LanguagePack::GetTextPackForEdit(TextPackName pack_name) -> TextPack&
{
    FO_STACK_TRACE_ENTRY();

    const auto pack_index = static_cast<size_t>(pack_name);
    FO_RUNTIME_ASSERT(pack_index < _textPacks.size());

    if (!_textPacks[pack_index]) {
        _textPacks[pack_index] = SafeAlloc::MakeUnique<TextPack>();
    }

    return *_textPacks[pack_index];
}

auto LanguagePack::ResolveTextPackName(string_view pack_name_str, bool* failed) const -> TextPackName
{
    FO_STACK_TRACE_ENTRY();

    try {
        return static_cast<TextPackName>(_nameResolver->ResolveEnumValue(string("TextPackName"), string(pack_name_str)));
    }
    catch (const EnumResolveException&) {
        if (failed != nullptr) {
            *failed = true;
            return {};
        }

        throw LanguagePackException("Invalid text pack name", pack_name_str);
    }
}

void LanguagePack::LoadFromResources(FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    auto text_files = resources.FilterFiles("fotxt-bin");

    for (const auto& text_file_header : text_files) {
        const auto text_file = File::Load(text_file_header);
        const auto file_name = text_file.GetNameNoExt();

        const auto name_triplet = strex(file_name).split('.');
        FO_RUNTIME_ASSERT(name_triplet.size() == 3);
        const auto& pack_name_str = name_triplet[1];
        const auto& lang_name = name_triplet[2];
        FO_RUNTIME_ASSERT(!pack_name_str.empty());
        FO_RUNTIME_ASSERT(!lang_name.empty());

        if (lang_name == _langName) {
            const auto pack_name = ResolveTextPackName(pack_name_str);
            auto& text_pack = GetTextPackForEdit(pack_name);

            if (!text_pack.LoadFromBinaryData(text_file.GetData())) {
                throw LanguagePackException("Invalid binary text file", text_file.GetPath());
            }
        }
    }
}

FO_END_NAMESPACE();
