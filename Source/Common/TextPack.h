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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(TextPackException);

class FileSystem;

///@ ExportValueType TextPackName TextPackName Layout = hstring-value
using TextPackName = strong_type<hstring, struct TextPackName_, strong_type_bool_test_tag, strong_type_sortings_tag>;

///@ ExportValueType LanguageName LanguageName Layout = hstring-value
using LanguageName = strong_type<hstring, struct LanguageName_, strong_type_bool_test_tag, strong_type_sortings_tag>;

///@ ExportValueType TextPackKey TextPackKey Layout = TextPackName-Collection+hstring-Key1+hstring-Key2+hstring-Key3
struct TextPackKey
{
    constexpr TextPackKey() noexcept = default;
    constexpr explicit TextPackKey(hstring key1, hstring key2 = {}, hstring key3 = {}) noexcept :
        Key1 {key1},
        Key2 {key2},
        Key3 {key3}
    {
    }
    constexpr explicit TextPackKey(TextPackName collection, hstring key1, hstring key2 = {}, hstring key3 = {}) noexcept :
        Collection {collection},
        Key1 {key1},
        Key2 {key2},
        Key3 {key3}
    {
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return Collection && (Key1 || Key2 || Key3); }
    [[nodiscard]] constexpr auto operator==(const TextPackKey& other) const noexcept -> bool = default;
    [[nodiscard]] constexpr auto operator<(const TextPackKey& other) const noexcept -> bool { return std::tie(Collection, Key1, Key2, Key3) < std::tie(other.Collection, other.Key1, other.Key2, other.Key3); }

    [[nodiscard]] static auto FromParts(HashResolver& hash_resolver, string_view collection, string_view key1, string_view key2 = {}, string_view key3 = {}) -> TextPackKey;
    [[nodiscard]] static auto FromPack(HashResolver& hash_resolver, string_view collection, string_view key1, string_view key2 = {}, string_view key3 = {}) -> TextPackKey;
    [[nodiscard]] static auto Parse(HashResolver& hash_resolver, string_view str, TextPackKey& result) -> bool;

    TextPackName Collection {};
    hstring Key1 {};
    hstring Key2 {};
    hstring Key3 {};
};
static_assert(std::is_standard_layout_v<TextPackKey>);

FO_END_NAMESPACE
template<>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE TextPackKey>
{
    using is_avalanching = void;

    auto operator()(const FO_NAMESPACE TextPackKey& v) const noexcept
    {
        const FO_NAMESPACE hstring::hash_t hashes[] = {v.Collection.underlying_value().as_hash(), v.Key1.as_hash(), v.Key2.as_hash(), v.Key3.as_hash()};
        return FO_NAMESPACE hashing_ex::hash(hashes, sizeof(hashes));
    }
};
template<>
struct std::formatter<FO_NAMESPACE TextPackKey> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE TextPackKey& value, FormatContext& ctx) const
    {
        string result;
        std::format_to(std::back_inserter(result), "{{{}}}{{{}}}{{{}}}{{{}}}", value.Collection, value.Key1, value.Key2, value.Key3);
        return formatter<FO_NAMESPACE string_view>::format(result, ctx);
    }
};
FO_BEGIN_NAMESPACE

class TextPack final
{
public:
    explicit TextPack(HashResolver& hash_resolver);
    TextPack(const TextPack&) = default;
    TextPack(TextPack&&) noexcept = default;
    auto operator=(const TextPack&) -> TextPack& = default;
    auto operator=(TextPack&&) noexcept -> TextPack& = default;
    ~TextPack() = default;

    [[nodiscard]] auto GetText(TextPackKey key) const -> const string&;
    [[nodiscard]] auto GetText(TextPackKey key, size_t skip) const -> const string&;
    [[nodiscard]] auto GetTextCount(TextPackKey key) const -> size_t;
    [[nodiscard]] auto IsTextPresent(TextPackKey key) const -> bool;
    [[nodiscard]] auto GetStr(TextPackKey key) const -> const string&;
    [[nodiscard]] auto GetStr(TextPackKey key, size_t skip) const -> const string&;
    [[nodiscard]] auto GetStrCount(TextPackKey key) const -> size_t;
    [[nodiscard]] auto GetSize() const noexcept -> size_t;
    [[nodiscard]] auto CheckIntersections(const TextPack& other) const -> bool;
    [[nodiscard]] auto GetBinaryData() const -> vector<uint8_t>;

    auto LoadFromBinaryData(const vector<uint8_t>& data, string_view collection = {}) -> bool;
    auto LoadFromString(const string& str, string_view collection = {}) -> bool;
    void LoadFromMap(const map<string, string>& kv, string_view collection = {});
    void LoadFromResources(FileSystem& resources, string_view language = {});
    void AddStr(TextPackKey key, string_view str);
    void AddStr(TextPackKey key, string&& str);
    void EraseStr(TextPackKey key);
    void Merge(const TextPack& other);
    void FixStr(const TextPack& base_pack);
    void Clear();

    static void FixPacks(const_span<string> bake_languages, vector<pair<string, map<string, TextPack>>>& lang_packs);

    friend struct TextPackKey;

private:
    auto MakeKeyPart(string_view value) -> hstring;
    void WriteKeyPart(DataWriter& writer, hstring part) const;
    auto ReadKeyPart(DataReader& reader) -> hstring;

    raw_ptr<HashResolver> _hashResolver;
    multimap<TextPackKey, string> _strData {};
    string _emptyStr {};
    mutable std::mt19937 _randomGenerator {MakeSeededRandomGenerator()};
};

FO_END_NAMESPACE
