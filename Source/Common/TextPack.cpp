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

#include "TextPack.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

static auto ExtractBraceToken(string& line, size_t& offset, string& token, bool allow_multiline, istringstream* sstr) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto first = line.find('{', offset);

    if (first == string::npos) {
        return false;
    }

    auto last = line.find('}', first);

    if (last == string::npos && allow_multiline && sstr != nullptr) {
        string additional_line;

        while (last == string::npos && std::getline(*sstr, additional_line, '\n')) {
            line += "\n" + additional_line;
            last = line.find('}', first);
        }
    }

    if (last == string::npos) {
        return false;
    }

    token = line.substr(first + 1, last - first - 1);
    offset = last + 1;
    return true;
}

auto TextPackKey::FromParts(HashResolver& hash_resolver, string_view collection, string_view key1, string_view key2, string_view key3) -> TextPackKey
{
    FO_STACK_TRACE_ENTRY();

    const auto hcollection = hash_resolver.ToHashedString(collection);
    const auto hkey1 = hash_resolver.ToHashedString(key1);
    const auto hkey2 = hash_resolver.ToHashedString(key2);
    const auto hkey3 = hash_resolver.ToHashedString(key3);
    return TextPackKey {TextPackName {hcollection}, hkey1, hkey2, hkey3};
}

auto TextPackKey::FromPack(HashResolver& hash_resolver, string_view collection, string_view key1, string_view key2, string_view key3) -> TextPackKey
{
    FO_STACK_TRACE_ENTRY();

    return FromParts(hash_resolver, collection, key1, key2, key3);
}

auto TextPackKey::Parse(HashResolver& hash_resolver, string_view str, TextPackKey& result) -> bool
{
    FO_STACK_TRACE_ENTRY();

    string source {str};
    size_t offset = 0;
    string tokens[4];

    for (auto& token : tokens) {
        if (!ExtractBraceToken(source, offset, token, false, nullptr)) {
            return false;
        }
    }

    result = FromParts(hash_resolver, tokens[0], tokens[1], tokens[2], tokens[3]);
    return true;
}

TextPack::TextPack(HashResolver& hash_resolver) :
    _hashResolver {&hash_resolver}
{
    FO_STACK_TRACE_ENTRY();
}

auto TextPack::GetText(TextPackKey key) const -> const string&
{
    FO_STACK_TRACE_ENTRY();

    return GetText(key, 0);
}

auto TextPack::GetText(TextPackKey key, size_t skip) const -> const string&
{
    FO_STACK_TRACE_ENTRY();

    return GetStr(key, skip);
}

auto TextPack::GetTextCount(TextPackKey key) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return GetStrCount(key);
}

auto TextPack::IsTextPresent(TextPackKey key) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetTextCount(key) != 0;
}

auto TextPack::GetStr(TextPackKey key) const -> const string&
{
    FO_STACK_TRACE_ENTRY();

    const size_t str_count = _strData.count(key);
    auto it = _strData.find(key);

    switch (str_count) {
    case 0:
        return _emptyStr;

    case 1:
        break;

    default:
        const int32 random_skip = std::uniform_int_distribution<int32> {0, numeric_cast<int32>(str_count)}(_randomGenerator)-1;

        for (int32 i = 0; i < random_skip; i++) {
            ++it;
        }

        break;
    }

    return it->second;
}

auto TextPack::GetStr(TextPackKey key, size_t skip) const -> const string&
{
    FO_STACK_TRACE_ENTRY();

    const size_t str_count = _strData.count(key);
    auto it = _strData.find(key);

    if (skip >= str_count) {
        return _emptyStr;
    }

    for (size_t i = 0; i < skip; i++) {
        ++it;
    }

    return it->second;
}

auto TextPack::GetStrCount(TextPackKey key) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return _strData.count(key);
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

    for (auto&& [key, str] : _strData) {
        WriteKeyPart(writer, key.Collection.underlying_value());
        WriteKeyPart(writer, key.Key1);
        WriteKeyPart(writer, key.Key2);
        WriteKeyPart(writer, key.Key3);
        writer.Write<uint32>(numeric_cast<uint32>(str.length()));
        writer.WritePtr(str.data(), str.length());
    }

    return data;
}

auto TextPack::LoadFromBinaryData(const vector<uint8>& data, string_view collection) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto reader = DataReader {data};
    const auto collection_key = TextPackName {MakeKeyPart(collection)};

    const auto count = reader.Read<uint32>();

    for (uint32 i = 0; i < count; i++) {
        TextPackKey key;

        key.Collection = TextPackName {ReadKeyPart(reader)};
        key.Key1 = ReadKeyPart(reader);
        key.Key2 = ReadKeyPart(reader);
        key.Key3 = ReadKeyPart(reader);

        if (!key.Collection && collection_key) {
            key.Collection = collection_key;
        }

        const auto str_len = reader.Read<uint32>();

        string str;

        if (str_len != 0) {
            str.resize(str_len);
            reader.ReadPtr(str.data(), str_len);
        }
        else {
            str.resize(0);
        }

        AddStr(key, std::move(str));
    }

    return true;
}

auto TextPack::LoadFromString(const string& str, string_view collection) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto failed = false;

    istringstream sstr(str);
    string line;

    while (std::getline(sstr, line, '\n')) {
        size_t offset = 0;

        string token1;
        string token2;
        string token3;

        if (!ExtractBraceToken(line, offset, token1, false, nullptr)) {
            continue;
        }
        if (!ExtractBraceToken(line, offset, token2, false, nullptr)) {
            failed = true;
            continue;
        }
        if (!ExtractBraceToken(line, offset, token3, true, &sstr)) {
            failed = true;
            continue;
        }

        if (collection.empty() || token1.empty()) {
            failed = true;
            continue;
        }

        AddStr(TextPackKey::FromParts(*_hashResolver, collection, token1, token2), std::move(token3));
    }

    return !failed;
}

void TextPack::LoadFromMap(const map<string, string>& kv, string_view collection)
{
    FO_STACK_TRACE_ENTRY();

    for (auto&& [key, value] : kv) {
        TextPackKey text_key;

        if (strvex(key).starts_with('{')) {
            if (TextPackKey::Parse(*_hashResolver, key, text_key)) {
                AddStr(text_key, value);
            }
        }
        else {
            if (!collection.empty() && !key.empty()) {
                AddStr(TextPackKey::FromParts(*_hashResolver, collection, key), value);
            }
        }
    }
}

void TextPack::LoadFromResources(FileSystem& resources, string_view language)
{
    FO_STACK_TRACE_ENTRY();

    auto text_files = resources.FilterFiles("fotxt-bin");

    for (const auto& text_file_header : text_files) {
        const auto text_file = File::Load(text_file_header);
        const auto file_name = text_file.GetNameNoExt();

        const auto name_triplet = strvex(file_name).split('.');
        FO_RUNTIME_ASSERT(name_triplet.size() == 3);
        const auto& pack_name_str = name_triplet[1];
        const auto& lang_name = name_triplet[2];
        FO_RUNTIME_ASSERT(!pack_name_str.empty());
        FO_RUNTIME_ASSERT(!lang_name.empty());

        if (!language.empty() && lang_name != language) {
            continue;
        }

        if (!LoadFromBinaryData(text_file.GetData(), pack_name_str)) {
            throw TextPackException("Invalid binary text file", text_file.GetPath());
        }
    }
}

void TextPack::AddStr(TextPackKey key, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    _strData.emplace(key, string(str));
}

void TextPack::AddStr(TextPackKey key, string&& str)
{
    FO_STACK_TRACE_ENTRY();

    _strData.emplace(key, std::move(str));
}

void TextPack::EraseStr(TextPackKey key)
{
    FO_STACK_TRACE_ENTRY();

    _strData.erase(key);
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
        const auto has_same_entry = _strData.count(key) != 0;

        if (!has_same_entry) {
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

void TextPack::Clear()
{
    FO_STACK_TRACE_ENTRY();

    _strData.clear();
}

void TextPack::FixPacks(const_span<string> bake_languages, vector<pair<string, map<string, TextPack>>>& lang_packs)
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

auto TextPack::MakeKeyPart(string_view value) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    return !value.empty() ? _hashResolver->ToHashedString(value) : hstring {};
}

void TextPack::WriteKeyPart(DataWriter& writer, hstring part) const
{
    FO_STACK_TRACE_ENTRY();

    const auto& str = part.as_str();
    writer.Write<uint32>(numeric_cast<uint32>(str.length()));

    if (!str.empty()) {
        writer.WritePtr(str.data(), str.length());
    }
}

auto TextPack::ReadKeyPart(DataReader& reader) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    const auto str_len = reader.Read<uint32>();

    if (str_len == 0) {
        return {};
    }

    string str;
    str.resize(str_len);
    reader.ReadPtr(str.data(), str_len);
    return MakeKeyPart(str);
}

FO_END_NAMESPACE
