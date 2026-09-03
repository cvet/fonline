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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

        while (last == string::npos && getline(*sstr, additional_line, '\n')) {
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

auto TextPackKey::FromParts(hash_resolver& hashes, string_view collection, string_view key1, string_view key2, string_view key3) -> TextPackKey
{
    FO_STACK_TRACE_ENTRY();

    hstring hcollection = hashes.to_hashed_string(collection);
    hstring hkey1 = hashes.to_hashed_string(key1);
    hstring hkey2 = hashes.to_hashed_string(key2);
    hstring hkey3 = hashes.to_hashed_string(key3);
    return TextPackKey {TextPackName {hcollection}, hkey1, hkey2, hkey3};
}

auto TextPackKey::FromPack(hash_resolver& hashes, string_view collection, string_view key1, string_view key2, string_view key3) -> TextPackKey
{
    FO_STACK_TRACE_ENTRY();

    return FromParts(hashes, collection, key1, key2, key3);
}

auto TextPackKey::Parse(hash_resolver& hashes, string_view str, TextPackKey& result) -> bool
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

    result = FromParts(hashes, tokens[0], tokens[1], tokens[2], tokens[3]);
    return true;
}

TextPack::TextPack(ptr<hash_resolver> hashes) :
    _hashResolver {hashes}
{
    FO_STACK_TRACE_ENTRY();
}

auto TextPack::GetText(TextPackKey key) const -> string_view
{
    FO_STACK_TRACE_ENTRY();

    return GetStr(key);
}

auto TextPack::GetText(TextPackKey key, size_t text_index) const -> string_view
{
    FO_STACK_TRACE_ENTRY();

    return GetStr(key, text_index);
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

auto TextPack::GetStr(TextPackKey key) const -> string_view
{
    FO_STACK_TRACE_ENTRY();

    const_span<pair<TextPackKey, string>> entries = FindEntries(key);

    if (entries.empty()) {
        return _emptyStr;
    }

    if (entries.size() == 1) {
        return entries.front().second;
    }

    return entries[_randomGenerator.next_below(numeric_cast<uint32_t>(entries.size()))].second;
}

auto TextPack::GetStr(TextPackKey key, size_t text_index) const -> string_view
{
    FO_STACK_TRACE_ENTRY();

    const_span<pair<TextPackKey, string>> entries = FindEntries(key);

    if (text_index >= entries.size()) {
        return _emptyStr;
    }

    return entries[text_index].second;
}

auto TextPack::GetStrCount(TextPackKey key) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return FindEntries(key).size();
}

auto TextPack::GetSize() const noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return _strData.size();
}

auto TextPack::CheckIntersections(TextPack& other) -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool result = false;

    EnsureSorted();
    other.EnsureSorted();

    for (auto&& [key, value] : SortedEntries()) {
        const_span<pair<TextPackKey, string>> other_entries = other.FindEntries(key);

        if (!other_entries.empty()) {
            logging::write("Intersection of key {} (count {}) value 1 '{}', value 2 '{}'", key, other_entries.size(), value, other_entries.front().second);
            result = true;
        }
    }

    return result;
}

auto TextPack::GetBinaryData() -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    auto writer = data_writer {data};

    EnsureSorted();

    const_span<pair<TextPackKey, string>> entries = SortedEntries();

    writer.write<uint32_t>(numeric_cast<uint32_t>(entries.size()));

    for (auto&& [key, str] : entries) {
        WriteKeyPart(writer, key.Collection.underlying_value());
        WriteKeyPart(writer, key.Key1);
        WriteKeyPart(writer, key.Key2);
        WriteKeyPart(writer, key.Key3);
        writer.write<uint32_t>(numeric_cast<uint32_t>(str.length()));
        writer.write_string_bytes(str);
    }

    return data;
}

auto TextPack::LoadFromBinaryData(const vector<uint8_t>& data, string_view collection) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto reader = data_reader {data};
    auto collection_key = TextPackName {MakeKeyPart(collection)};

    auto count = reader.read<uint32_t>();

    _strData.reserve(_strData.size() + count);

    for (uint32_t i = 0; i < count; i++) {
        TextPackKey key;

        key.Collection = TextPackName {ReadKeyPart(reader)};
        key.Key1 = ReadKeyPart(reader);
        key.Key2 = ReadKeyPart(reader);
        key.Key3 = ReadKeyPart(reader);

        if (!key.Collection && collection_key) {
            key.Collection = collection_key;
        }

        auto str_len = reader.read<uint32_t>();

        string str;

        if (str_len != 0) {
            str.resize(str_len);
            reader.read_string_bytes(str);
        }
        else {
            str.resize(0);
        }

        AddStr(key, std::move(str));
    }

    EnsureSorted();

    return true;
}

auto TextPack::LoadFromString(const string& str, string_view collection) -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool failed = false;

    istringstream sstr(make_stream_string(str));
    string line;

    while (getline(sstr, line, '\n')) {
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

    EnsureSorted();

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

    EnsureSorted();
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

void TextPack::AddStr(TextPackKey key, string_view str)
{
    FO_STACK_TRACE_ENTRY();

    _strDataSorted = _strDataSorted && (_strData.empty() || !(key < _strData.back().first));
    _strData.emplace_back(key, string(str));
}

void TextPack::AddStr(TextPackKey key, string&& str)
{
    FO_STACK_TRACE_ENTRY();

    _strDataSorted = _strDataSorted && (_strData.empty() || !(key < _strData.back().first));
    _strData.emplace_back(key, std::move(str));
}

void TextPack::EraseStr(TextPackKey key)
{
    FO_STACK_TRACE_ENTRY();

    pair<size_t, size_t> range = EqualRange(key);

    _strData.erase(_strData.begin() + static_cast<ptrdiff_t>(range.first), _strData.begin() + static_cast<ptrdiff_t>(range.second));
}

void TextPack::Merge(const TextPack& other)
{
    FO_STACK_TRACE_ENTRY();

    for (auto&& [key, value] : other._strData) {
        AddStr(key, value);
    }

    EnsureSorted();
}

void TextPack::FixStr(TextPack& base_pack)
{
    FO_STACK_TRACE_ENTRY();

    EnsureSorted();
    base_pack.EnsureSorted();

    // The base pack is in key order, so its several strings under one key arrive adjacently and only the
    // first of them is taken, which is what the node tree did when its own insert was visible to the next check
    vector<pair<TextPackKey, string>> missing;
    TextPackKey last_key;

    for (auto&& [key, value] : base_pack.SortedEntries()) {
        if (key == last_key) {
            continue;
        }

        last_key = key;

        if (FindEntries(key).empty()) {
            missing.emplace_back(key, value);
        }
    }

    for (auto&& [key, value] : missing) {
        AddStr(key, std::move(value));
    }

    EnsureSorted();

    // Remove keys that are not in the base pack
    (void)std::erase_if(_strData, [&base_pack](const pair<TextPackKey, string>& entry) { return base_pack.FindEntries(entry.first).empty(); });
}

void TextPack::Clear()
{
    FO_STACK_TRACE_ENTRY();

    _strData.clear();
    _strDataSorted = true;
}

// Every path that adds entries ends here, so restoring the key order is a mutation and never happens on a
// read — several threads resolve text from one pack at once. Stable, so strings under one key keep their order
void TextPack::EnsureSorted()
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_strDataSorted) {
        std::stable_sort(_strData.begin(), _strData.end(), [](const pair<TextPackKey, string>& left, const pair<TextPackKey, string>& right) { return left.first < right.first; });
        _strDataSorted = true;
    }
}

// The single read entry point, so a pack reaching a reader out of order is caught here rather than answering
// a lookup from a binary search over unsorted data
auto TextPack::SortedEntries() const -> const_span<pair<TextPackKey, string>>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_strDataSorted, "Text pack must be sorted before it is read", _strData.size());

    return {_strData.data(), _strData.size()};
}

auto TextPack::FindEntries(TextPackKey key) const -> const_span<pair<TextPackKey, string>>
{
    FO_NO_STACK_TRACE_ENTRY();

    pair<size_t, size_t> range = EqualRange(key);

    return {_strData.data() + range.first, range.second - range.first};
}

// One binary search, then a walk over the duplicates, of which a key has one or two
auto TextPack::EqualRange(TextPackKey key) const -> pair<size_t, size_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    const_span<pair<TextPackKey, string>> entries = SortedEntries();

    auto first = std::lower_bound(entries.begin(), entries.end(), key, [](const pair<TextPackKey, string>& entry, const TextPackKey& probe) { return entry.first < probe; });
    auto last = first;

    while (last != entries.end() && last->first == key) {
        ++last;
    }

    return {static_cast<size_t>(first - entries.begin()), static_cast<size_t>(last - entries.begin())};
}

auto TextPack::ParseBakeLanguages(const_span<string> declarations) -> BakeLanguageConfig
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!declarations.empty(), "BakeLanguages must contain at least one language declaration");

    BakeLanguageConfig result;

    for (const string& declaration : declarations) {
        size_t separator = declaration.find(':');
        bool has_fallback = separator != string::npos;
        FO_VERIFY_AND_THROW(!declaration.empty() && (!has_fallback || (separator != 0 && separator + 1 < declaration.size() && declaration.find(':', separator + 1) == string::npos)), "Bake language declaration must use language or language:parent format", declaration);

        string language = has_fallback ? declaration.substr(0, separator) : declaration;
        FO_VERIFY_AND_THROW(std::ranges::find(result.Languages, language) == result.Languages.end(), "Bake language is declared more than once", language);

        if (has_fallback) {
            string parent_language = declaration.substr(separator + 1);
            FO_VERIFY_AND_THROW(std::ranges::find(result.Languages, parent_language) != result.Languages.end(), "Bake language fallback parent must be declared before its child", language, parent_language);
            result.Fallbacks.emplace(language, parent_language);
        }

        result.Languages.emplace_back(language);
    }

    return result;
}

void TextPack::FixPacks(const BakeLanguageConfig& bake_languages, vector<pair<string, map<string, TextPack>>>& lang_packs)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!bake_languages.Languages.empty(), "Text pack normalization cannot choose a base language because BakeLanguages is empty", lang_packs.size());

    // Add default language
    if (lang_packs.empty() || lang_packs.front().first != bake_languages.Languages.front()) {
        lang_packs.emplace(lang_packs.begin(), bake_languages.Languages.front(), map<string, TextPack>());
    }

    // Add missed languages
    for (const auto& lang : bake_languages.Languages) {
        if (std::ranges::find_if(lang_packs, [&](auto&& l) { return l.first == lang; }) == lang_packs.end()) {
            lang_packs.emplace_back(lang, map<string, TextPack>());
        }
    }

    // Remove unsupported languages
    for (auto it = lang_packs.begin(); it != lang_packs.end();) {
        if (std::ranges::find_if(bake_languages.Languages, [&](auto&& l) { return l == it->first; }) == bake_languages.Languages.end()) {
            it = lang_packs.erase(it);
        }
        else {
            ++it;
        }
    }

    // Normalize language packs
    auto find_language_pack = [&](string_view language) -> ptr<map<string, TextPack>> {
        auto lang_it = std::ranges::find_if(lang_packs, [&](const auto& lang_pack) { return lang_pack.first == language; });
        FO_VERIFY_AND_THROW(lang_it != lang_packs.end(), "Bake language pack is missing during normalization", language);
        return make_ptr(&lang_it->second);
    };

    for (size_t i = 1; i < bake_languages.Languages.size(); i++) {
        const string& language = bake_languages.Languages[i];
        auto fallback_it = bake_languages.Fallbacks.find(language);
        const string& fallback_language = fallback_it != bake_languages.Fallbacks.end() ? fallback_it->second : bake_languages.Languages.front();
        auto fallback_lang_pack = find_language_pack(fallback_language);
        auto lang_pack = find_language_pack(language);

        // Remove packs that are not in the base language pack
        for (auto it = lang_pack->begin(); it != lang_pack->end();) {
            if (fallback_lang_pack->count(it->first) == 0) {
                it = lang_pack->erase(it);
            }
            else {
                ++it;
            }
        }

        // Add packs that are in the base language pack but not in this pack
        for (auto&& [pack_name, text_pack] : *fallback_lang_pack) {
            if (lang_pack->count(pack_name) == 0) {
                lang_pack->emplace(pack_name, text_pack);
            }
        }

        FO_VERIFY_AND_THROW(lang_pack->size() == fallback_lang_pack->size(), "Normalized language pack set does not match the fallback language pack set", language, fallback_language, lang_pack->size(), fallback_lang_pack->size());

        // Normalize texts to the base language
        for (auto&& [pack_name, text_pack] : *lang_pack) {
            auto it = fallback_lang_pack->find(pack_name);
            FO_VERIFY_AND_THROW(it != fallback_lang_pack->end(), "Lookup failed in fallback lang pack");
            text_pack.FixStr(it->second);
        }
    }
}

auto TextPack::MakeKeyPart(string_view value) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    return !value.empty() ? _hashResolver->to_hashed_string(value) : hstring {};
}

void TextPack::WriteKeyPart(data_writer& writer, hstring part) const
{
    FO_STACK_TRACE_ENTRY();

    string_view str = part.as_str();
    writer.write<uint32_t>(numeric_cast<uint32_t>(str.length()));

    if (!str.empty()) {
        writer.write_string_bytes(str);
    }
}

auto TextPack::ReadKeyPart(data_reader& reader) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    auto str_len = reader.read<uint32_t>();

    if (str_len == 0) {
        return {};
    }

    string str;
    str.resize(str_len);
    reader.read_string_bytes(str);
    return MakeKeyPart(str);
}

FO_END_NAMESPACE
