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

#pragma once

#include "Common.h"

#include "CacheStorage.h"
#include "FileSystem.h"
#include "MsgStr-Include.h"

#define TEXTMSG_TEXT (0)
#define TEXTMSG_DLG (1)
#define TEXTMSG_ITEM (2)
#define TEXTMSG_GAME (3)
#define TEXTMSG_GM (4)
#define TEXTMSG_COMBAT (5)
#define TEXTMSG_QUEST (6)
#define TEXTMSG_HOLO (7)
#define TEXTMSG_INTERNAL (8)
#define TEXTMSG_LOCATIONS (9)
#define TEXTMSG_COUNT (10)

class FOMsg
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

    void AddStr(uint num, const string& str);
    void AddBinary(uint num, const uchar* binary, uint len);

    [[nodiscard]] auto GetStr(uint num) -> string;
    [[nodiscard]] auto GetStr(uint num, uint skip) -> string;
    [[nodiscard]] auto GetStrNumUpper(uint num) -> uint;
    [[nodiscard]] auto GetStrNumLower(uint num) -> uint;
    [[nodiscard]] auto GetInt(uint num) -> int;
    [[nodiscard]] auto GetBinary(uint num, UCharVec& data) -> uint;
    [[nodiscard]] auto Count(uint num) const -> uint;
    void EraseStr(uint num);
    [[nodiscard]] auto GetSize() const -> uint;
    [[nodiscard]] auto IsIntersects(const FOMsg& other) -> bool;

    // Serialization
    void GetBinaryData(UCharVec& data);
    auto LoadFromBinaryData(const UCharVec& data) -> bool;
    auto LoadFromString(const string& str) -> bool;
    void LoadFromMap(const StrMap& kv);
    void Clear();

    static auto GetMsgType(const string& type_name) -> int;

private:
    MsgMap _strData {};
};

class LanguagePack final
{
public:
    // Todo: move loading to constructors
    LanguagePack() = default;
    LanguagePack(const LanguagePack&) = default;
    LanguagePack(LanguagePack&&) noexcept = default;
    auto operator=(const LanguagePack&) -> LanguagePack& = default;
    auto operator=(LanguagePack&&) noexcept -> LanguagePack& = default;
    ~LanguagePack() = default;

    auto operator==(const uint name_code) const -> bool { return NameCode == name_code; }
    void LoadFromFiles(FileManager& file_mngr, const string& lang_name);
    void LoadFromCache(CacheStorage& cache, const string& lang_name);
    [[nodiscard]] auto GetMsgCacheName(int msg_num) const -> string;

    string Name {};
    uint NameCode {};
    bool IsAllMsgLoaded {};

    FOMsg Msg[TEXTMSG_COUNT] {};
};
