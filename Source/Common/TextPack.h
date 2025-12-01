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

#pragma once

#include "Common.h"

#include "FileSystem.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(LanguagePackException);

///@ ExportEnum
enum class TextPackName : uint8
{
    None = 0,
    Items = 2,
    Critters = 3,
    Maps = 4,
    Locations = 5,
    Protos = 6,
};

using TextPackKey = uint32;

class TextPack final
{
public:
    TextPack() = default;
    TextPack(const TextPack&) = default;
    TextPack(TextPack&&) noexcept = default;
    auto operator=(const TextPack&) -> TextPack& = default;
    auto operator=(TextPack&&) noexcept -> TextPack& = default;
    ~TextPack() = default;

    [[nodiscard]] auto GetStr(TextPackKey num) const -> const string&;
    [[nodiscard]] auto GetStr(TextPackKey num, size_t skip) const -> const string&;
    [[nodiscard]] auto GetStrNumUpper(TextPackKey num) const -> TextPackKey;
    [[nodiscard]] auto GetStrNumLower(TextPackKey num) const -> TextPackKey;
    [[nodiscard]] auto GetStrCount(TextPackKey num) const -> size_t;
    [[nodiscard]] auto GetSize() const noexcept -> size_t;
    [[nodiscard]] auto CheckIntersections(const TextPack& other) const -> bool;
    [[nodiscard]] auto GetBinaryData() const -> vector<uint8>;

    auto LoadFromBinaryData(const vector<uint8>& data) -> bool;
    auto LoadFromString(const string& str, HashResolver& hash_resolver) -> bool;
    void LoadFromMap(const map<string, string>& kv);
    void AddStr(TextPackKey num, string_view str);
    void AddStr(TextPackKey num, string&& str);
    void EraseStr(TextPackKey num);
    void Merge(const TextPack& other);
    void FixStr(const TextPack& base_pack);
    void Clear();

    static void FixPacks(span<const string> bake_languages, vector<pair<string, map<string, TextPack>>>& lang_packs);

private:
    multimap<TextPackKey, string> _strData {};
    string _emptyStr {};
};

class LanguagePack final
{
public:
    LanguagePack() = default;
    LanguagePack(string_view lang_name, const NameResolver& name_resolver);
    LanguagePack(const LanguagePack&) = delete;
    LanguagePack(LanguagePack&&) noexcept = default;
    auto operator=(const LanguagePack&) -> LanguagePack& = delete;
    auto operator=(LanguagePack&&) noexcept -> LanguagePack& = default;
    auto operator==(string_view lang_name) const -> bool { return lang_name == _langName; }
    ~LanguagePack() = default;

    [[nodiscard]] auto GetName() const noexcept -> const string&;
    [[nodiscard]] auto GetTextPack(TextPackName pack_name) const -> const TextPack&;
    [[nodiscard]] auto GetTextPackForEdit(TextPackName pack_name) -> TextPack&;
    [[nodiscard]] auto ResolveTextPackName(string_view pack_name_str, bool* failed = nullptr) const -> TextPackName;

    void LoadFromResources(FileSystem& resources);

private:
    string _langName {};
    raw_ptr<const NameResolver> _nameResolver {};
    vector<unique_ptr<TextPack>> _textPacks {};
    TextPack _emptyPack {};
};

FO_END_NAMESPACE();
