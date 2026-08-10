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

static auto ExtractBraceToken(string& line, size_t& offset, string& token, bool allow_multiline, nptr<istringstream> sstr) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto first = line.find('{', offset);

    if (first == string::npos) {
        return false;
    }

    auto last = line.find('}', first);

    if (last == string::npos && allow_multiline && sstr) {
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

static auto ExtractBraceToken(u8string& line, size_t& offset, u8string& token, bool allow_multiline, nptr<u8istringstream> sstr) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto first = line.view().native_view().find(u8'{', offset);

    if (first == std::u8string_view::npos) {
        return false;
    }

    auto last = line.view().native_view().find(u8'}', first);

    if (last == std::u8string_view::npos && allow_multiline && sstr) {
        u8string additional_line;

        while (last == std::u8string_view::npos && getline(*sstr, additional_line)) {
            line.append(u8"\n");
            line.append(additional_line);
            last = line.view().native_view().find(u8'}', first);
        }
    }

    if (last == std::u8string_view::npos) {
        return false;
    }

    token.assign(u8string_view::FromChecked(line.view().native_view().substr(first + 1, last - first - 1)));
    offset = last + 1;
    return true;
}

auto TextPackKey::FromParts(HashResolver& hash_resolver, string_view collection, string_view key1, string_view key2, string_view key3) -> TextPackKey
{
    FO_STACK_TRACE_ENTRY();

    hstring hcollection = hash_resolver.ToHashedString(collection);
    hstring hkey1 = hash_resolver.ToHashedString(key1);
    hstring hkey2 = hash_resolver.ToHashedString(key2);
    hstring hkey3 = hash_resolver.ToHashedString(key3);
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

TextPack::TextPack(ptr<HashResolver> hash_resolver) :
    _hashResolver {hash_resolver}
{
    FO_STACK_TRACE_ENTRY();
}

auto TextPack::GetText(TextPackKey key) const -> u8string_view
{
    FO_STACK_TRACE_ENTRY();

    return GetText(key, 0);
}

auto TextPack::GetText(TextPackKey key, size_t skip) const -> u8string_view
{
    FO_STACK_TRACE_ENTRY();

    size_t text_count = _textData.count(key);
    auto it = _textData.find(key);

    if (skip >= text_count) {
        return _emptyText.view();
    }

    for (size_t i = 0; i < skip; i++) {
        ++it;
    }

    return it->second.view();
}

auto TextPack::GetTextCount(TextPackKey key) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return _textData.count(key);
}

auto TextPack::IsTextPresent(TextPackKey key) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetTextCount(key) != 0;
}

auto TextPack::GetSize() const noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return _textData.size();
}

auto TextPack::CheckIntersections(const TextPack& other) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool result = false;

    for (auto&& [key, value] : _textData) {
        if (other._textData.count(key) != 0) {
            WriteLog("Intersection of key {} (count {}) value 1 '{}', value 2 '{}'", key, other._textData.count(key), value.view(), other._textData.find(key)->second.view());
            result = true;
        }
    }

    return result;
}

auto TextPack::GetBinaryData() const -> vector<byte>
{
    FO_STACK_TRACE_ENTRY();

    vector<byte> serialized_data;
    auto writer = DataWriter {serialized_data};

    writer.Write<uint32_t>(numeric_cast<uint32_t>(_textData.size()));

    for (auto&& [key, text] : _textData) {
        WriteKeyPart(writer, key.Collection.underlying_value());
        WriteKeyPart(writer, key.Key1);
        WriteKeyPart(writer, key.Key2);
        WriteKeyPart(writer, key.Key3);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(text.size()));
        writer.WriteBytes(utf8_to_byte_span(text.view()));
    }

    return serialized_data;
}

auto TextPack::LoadFromBinaryData(const_span<byte> data, string_view collection) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto reader = DataReader {data};
    auto collection_key = TextPackName {MakeKeyPart(collection)};

    auto count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < count; i++) {
        TextPackKey key;

        key.Collection = TextPackName {ReadKeyPart(reader)};
        key.Key1 = ReadKeyPart(reader);
        key.Key2 = ReadKeyPart(reader);
        key.Key3 = ReadKeyPart(reader);

        if (!key.Collection && collection_key) {
            key.Collection = collection_key;
        }

        auto str_len = reader.Read<uint32_t>();

        u8string text = utf8_from_byte_span(reader.ReadBytes(str_len));
        AddText(key, std::move(text));
    }

    return true;
}

auto TextPack::LoadFromText(u8string_view text, string_view collection) -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool failed = false;

    u8istringstream sstr {text};
    u8string line;

    while (getline(sstr, line)) {
        size_t offset = 0;

        u8string token1;
        u8string token2;
        u8string token3;

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

        AddText(TextPackKey::FromParts(*_hashResolver, collection, utf8_to_string(token1), utf8_to_string(token2)), std::move(token3));
    }

    return !failed;
}

void TextPack::LoadFromTextMap(const map<string, u8string>& kv, string_view collection)
{
    FO_STACK_TRACE_ENTRY();

    for (auto&& [key, value] : kv) {
        TextPackKey text_key;

        if (strvex(key).starts_with('{')) {
            if (TextPackKey::Parse(*_hashResolver, key, text_key)) {
                AddText(text_key, value.view());
            }
        }
        else {
            if (!collection.empty() && !key.empty()) {
                AddText(TextPackKey::FromParts(*_hashResolver, collection, key), value.view());
            }
        }
    }
}

void TextPack::LoadFromResources(FileSystem& resources, string_view language)
{
    FO_STACK_TRACE_ENTRY();

    auto text_files = resources.FilterFiles("fotxt-bin");

    for (const auto& text_file_header : text_files) {
        auto text_file = File::Load(text_file_header);
        string_view file_name = text_file.GetNameNoExt();

        auto name_triplet = strvex(file_name).split('.');
        FO_VERIFY_AND_THROW(name_triplet.size() == 3, "Baked text filename must contain pack prefix, text pack name and language suffix", text_file_header.GetPath(), file_name, name_triplet.size());
        const auto& pack_name_str = name_triplet[1];
        const auto& lang_name = name_triplet[2];
        FO_VERIFY_AND_THROW(!pack_name_str.empty(), "Baked text filename has an empty text pack name segment", text_file_header.GetPath(), file_name);
        FO_VERIFY_AND_THROW(!lang_name.empty(), "Baked text filename has an empty language segment", text_file_header.GetPath(), file_name);

        if (!language.empty() && lang_name != language) {
            continue;
        }

        if (!LoadFromBinaryData(text_file.GetData(), pack_name_str)) {
            throw TextPackException("Invalid binary text file", text_file.GetPath());
        }
    }
}

void TextPack::AddText(TextPackKey key, u8string_view text)
{
    FO_STACK_TRACE_ENTRY();

    _textData.emplace(key, u8string {text});
}

void TextPack::AddText(TextPackKey key, u8string&& text)
{
    FO_STACK_TRACE_ENTRY();

    _textData.emplace(key, std::move(text));
}

void TextPack::EraseText(TextPackKey key)
{
    FO_STACK_TRACE_ENTRY();

    _textData.erase(key);
}

void TextPack::Merge(const TextPack& other)
{
    FO_STACK_TRACE_ENTRY();

    for (auto&& [key, value] : other._textData) {
        AddText(key, value.view());
    }
}

void TextPack::FixText(const TextPack& base_pack)
{
    FO_STACK_TRACE_ENTRY();

    // Add keys that are in the base pack but not in this pack
    for (auto&& [key, value] : base_pack._textData) {
        auto has_same_entry = _textData.count(key) != 0;

        if (!has_same_entry) {
            AddText(key, value.view());
        }
    }

    // Remove keys that are not in the base pack
    for (auto it = _textData.begin(); it != _textData.end();) {
        if (base_pack._textData.count(it->first) == 0) {
            it = _textData.erase(it);
        }
        else {
            ++it;
        }
    }
}

void TextPack::Clear()
{
    FO_STACK_TRACE_ENTRY();

    _textData.clear();
}

void TextPack::FixPacks(const_span<string> bake_languages, vector<pair<string, map<string, TextPack>>>& lang_packs)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!bake_languages.empty(), "Text pack normalization cannot choose a base language because BakeLanguages is empty", lang_packs.size());

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

        FO_VERIFY_AND_THROW(lang_pack.size() == base_lang_pack.size(), "Normalized language pack set does not match the base language pack set", lang_packs[i].first, lang_pack.size(), base_lang_pack.size());

        // Normalize texts to the base language
        for (auto&& [pack_name, text_pack] : lang_pack) {
            auto it = base_lang_pack.find(pack_name);
            FO_VERIFY_AND_THROW(it != base_lang_pack.end(), "Lookup failed in base lang pack");
            text_pack.FixText(it->second);
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

    string_view str = part.as_str();
    writer.Write<uint32_t>(numeric_cast<uint32_t>(str.length()));

    if (!str.empty()) {
        writer.WriteStringBytes(str);
    }
}

auto TextPack::ReadKeyPart(DataReader& reader) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    auto str_len = reader.Read<uint32_t>();

    if (str_len == 0) {
        return {};
    }

    string str;
    str.resize(str_len);
    reader.ReadStringBytes(str);
    return MakeKeyPart(str);
}

FO_END_NAMESPACE
