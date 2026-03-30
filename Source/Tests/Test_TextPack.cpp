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

#include "catch_amalgamated.hpp"

#include "TextPack.h"

FO_BEGIN_NAMESPACE

TEST_CASE("TextPack")
{
    SECTION("LoadFromStringParsesNumericOffsetsAndMultilineValues")
    {
        HashStorage hashes;
        TextPack pack;

        const string input = "{100}{}{Hello}\n{200}{3}{World}\n{300}{}{Line1\nLine2}";

        REQUIRE(pack.LoadFromString(input, hashes));
        CHECK(pack.GetSize() == 3);
        CHECK(pack.GetStr(100, 0) == "Hello");
        CHECK(pack.GetStr(203, 0) == "World");
        CHECK(pack.GetStr(300, 0) == "Line1\nLine2");
        CHECK(pack.GetStrCount(300) == 1);
        CHECK(pack.GetStr(9999).empty());
    }

    SECTION("BinaryRoundtripPreservesEntries")
    {
        TextPack pack;
        pack.AddStr(10, string_view {"Alpha"});
        pack.AddStr(20, string_view {"Beta"});
        pack.AddStr(20, string_view {"Gamma"});

        const auto data = pack.GetBinaryData();

        TextPack restored;
        REQUIRE(restored.LoadFromBinaryData(data));
        CHECK(restored.GetSize() == 3);
        CHECK(restored.GetStrCount(10) == 1);
        CHECK(restored.GetStrCount(20) == 2);
        CHECK(restored.GetStr(10, 0) == "Alpha");
        CHECK(restored.GetStr(20, 0) == "Beta");
        CHECK(restored.GetStr(20, 1) == "Gamma");
    }

    SECTION("FixStrAddsMissingAndRemovesUnknownKeys")
    {
        TextPack base_pack;
        base_pack.AddStr(1, string_view {"BaseOne"});
        base_pack.AddStr(2, string_view {"BaseTwo"});

        TextPack localized_pack;
        localized_pack.AddStr(2, string_view {"LocalizedTwo"});
        localized_pack.AddStr(3, string_view {"Unexpected"});

        localized_pack.FixStr(base_pack);

        CHECK(localized_pack.GetSize() == 2);
        CHECK(localized_pack.GetStr(1, 0) == "BaseOne");
        CHECK(localized_pack.GetStr(2, 0) == "LocalizedTwo");
        CHECK(localized_pack.GetStr(3).empty());
    }

    SECTION("LoadFromMapAndClearWorkTogether")
    {
        TextPack pack;
        const map<string, string> entries {{"7", "Seven"}, {"0", "Ignored"}, {"11", "Eleven"}};

        pack.LoadFromMap(entries);

        CHECK(pack.GetSize() == 2);
        CHECK(pack.GetStr(7, 0) == "Seven");
        CHECK(pack.GetStr(11, 0) == "Eleven");

        pack.Clear();

        CHECK(pack.GetSize() == 0);
        CHECK(pack.GetStr(7).empty());
    }
}

FO_END_NAMESPACE
