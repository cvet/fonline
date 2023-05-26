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

#pragma once

#include "Common.h"

#include "FileSystem.h"

DECLARE_EXCEPTION(LanguagePackException);

static constexpr auto TEXTMSG_TEXT = 0;
static constexpr auto TEXTMSG_DLG = 1;
static constexpr auto TEXTMSG_ITEM = 2;
static constexpr auto TEXTMSG_GAME = 3;
static constexpr auto TEXTMSG_GM = 4;
static constexpr auto TEXTMSG_COMBAT = 5;
static constexpr auto TEXTMSG_QUEST = 6;
static constexpr auto TEXTMSG_HOLO = 7;
static constexpr auto TEXTMSG_INTERNAL = 8;
static constexpr auto TEXTMSG_LOCATIONS = 9;
static constexpr auto TEXTMSG_COUNT = 10;

class FOMsg final
{
public:
    using MsgMap = multimap<uint, string>;

    FOMsg() = default;
    FOMsg(const FOMsg&) = default;
    FOMsg(FOMsg&&) noexcept = default;
    auto operator=(const FOMsg&) -> FOMsg& = default;
    auto operator=(FOMsg&&) noexcept -> FOMsg& = default;
    auto operator+=(const FOMsg& other) -> FOMsg&;
    ~FOMsg() = default;

    static auto GetMsgType(string_view type_name) -> int;

    [[nodiscard]] auto GetStr(uint num) const -> string; // Todo: pass default to fomsg gets
    [[nodiscard]] auto GetStr(uint num, uint skip) const -> string;
    [[nodiscard]] auto GetStrNumUpper(uint num) const -> uint;
    [[nodiscard]] auto GetStrNumLower(uint num) const -> uint;
    [[nodiscard]] auto GetInt(uint num) const -> int;
    [[nodiscard]] auto GetBinary(uint num) const -> vector<uint8>;
    [[nodiscard]] auto Count(uint num) const -> uint;
    [[nodiscard]] auto GetSize() const -> uint;
    [[nodiscard]] auto IsIntersects(const FOMsg& other) const -> bool;
    [[nodiscard]] auto GetBinaryData() const -> vector<uint8>;

    auto LoadFromBinaryData(const vector<uint8>& data) -> bool;
    auto LoadFromString(string_view str, HashResolver& hash_resolver) -> bool;
    void LoadFromMap(const map<string, string>& kv);
    void AddStr(uint num, string_view str);
    void AddBinary(uint num, const uint8* binary, uint len);
    void EraseStr(uint num);
    void Clear();

private:
    MsgMap _strData {};
};

class LanguagePack final
{
public:
    LanguagePack() = default;
    LanguagePack(const LanguagePack&) = default;
    LanguagePack(LanguagePack&&) noexcept = default;
    auto operator=(const LanguagePack&) -> LanguagePack& = default;
    auto operator=(LanguagePack&&) noexcept -> LanguagePack& = default;
    auto operator==(const string& name) const -> bool { return name == Name; }
    auto operator==(const uint name_code) const -> bool { return name_code == NameCode; }
    ~LanguagePack() = default;

    void ParseTexts(FileSystem& resources, HashResolver& hash_resolver, string_view lang_name);
    void SaveTextsToDisk(string_view dir) const;
    void LoadTexts(FileSystem& resources, string_view lang_name);

    string Name {};
    uint NameCode {};
    FOMsg Msg[TEXTMSG_COUNT] {};
};
