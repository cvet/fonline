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

    SECTION("LoadFromStringSupportsHashedKeysAndOffsets")
    {
        HashStorage hashes;
        TextPack pack;

        const auto base_hash = hashes.ToHashedString("QuestEntry").as_int32();
        const auto offset_hash = hashes.ToHashedString("Suffix").as_int32();
        const string input = "{QuestEntry}{}{Base}\n{QuestEntry}{Suffix}{Combined}";

        REQUIRE(pack.LoadFromString(input, hashes));
        CHECK(pack.GetStr(base_hash, 0) == "Base");
        CHECK(pack.GetStr(base_hash + offset_hash, 0) == "Combined");
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

    SECTION("MergeEraseAndIntersectionTrackSharedKeys")
    {
        TextPack base_pack;
        base_pack.AddStr(1, string_view {"BaseOne"});
        base_pack.AddStr(2, string_view {"BaseTwo"});

        TextPack incoming_pack;
        incoming_pack.AddStr(2, string_view {"IncomingTwo"});
        incoming_pack.AddStr(3, string_view {"IncomingThree"});

        CHECK(base_pack.CheckIntersections(incoming_pack));

        base_pack.Merge(incoming_pack);

        CHECK(base_pack.GetSize() == 4);
        CHECK(base_pack.GetStrCount(2) == 2);
        CHECK(base_pack.GetStr(2, 0) == "BaseTwo");
        CHECK(base_pack.GetStr(2, 1) == "IncomingTwo");
        CHECK(base_pack.GetStr(3, 0) == "IncomingThree");

        base_pack.EraseStr(2);

        CHECK(base_pack.GetStrCount(2) == 0);
        CHECK(base_pack.GetStr(2).empty());
        CHECK(base_pack.GetSize() == 2);

        TextPack disjoint_pack;
        disjoint_pack.AddStr(99, string_view {"OnlyHere"});
        CHECK_FALSE(base_pack.CheckIntersections(disjoint_pack));
    }

    SECTION("GetStrSkipOutOfRangeReturnsEmptyString")
    {
        TextPack pack;
        pack.AddStr(5, string_view {"Alpha"});
        pack.AddStr(5, string_view {"Beta"});

        CHECK(pack.GetStr(5, 0) == "Alpha");
        CHECK(pack.GetStr(5, 1) == "Beta");
        CHECK(pack.GetStr(5, 2).empty());
        CHECK(pack.GetStr(42, 0).empty());
    }

    SECTION("MalformedLoadFromStringReportsFailureAfterKeepingValidEntries")
    {
        HashStorage hashes;
        TextPack pack;

        const string input = "{10}{}{Valid}\n{20}{Broken\n{30}{}{StillValid}";

        CHECK_FALSE(pack.LoadFromString(input, hashes));
        CHECK(pack.GetStr(10, 0) == "Valid");
        CHECK(pack.GetStr(20).empty());
        CHECK(pack.GetStr(30, 0) == "StillValid");
        CHECK(pack.GetSize() == 2);
    }

    SECTION("FixPacksAddsMissingLanguagesAndNormalizesAgainstBase")
    {
        vector<string> bake_languages {"engl", "russ", "germ"};
        vector<pair<string, map<string, TextPack>>> lang_packs;

        TextPack engl_dialogs;
        engl_dialogs.AddStr(1, string_view {"Hello"});
        engl_dialogs.AddStr(2, string_view {"World"});

        TextPack engl_items;
        engl_items.AddStr(10, string_view {"Item"});

        TextPack russ_dialogs;
        russ_dialogs.AddStr(2, string_view {"Mir"});
        russ_dialogs.AddStr(3, string_view {"Extra"});

        TextPack unsupported_dialogs;
        unsupported_dialogs.AddStr(1, string_view {"Hola"});

        lang_packs.emplace_back("engl", map<string, TextPack> {});
        lang_packs[0].second.emplace("Dialogs", engl_dialogs);
        lang_packs[0].second.emplace("Items", engl_items);

        lang_packs.emplace_back("russ", map<string, TextPack> {});
        lang_packs[1].second.emplace("Dialogs", russ_dialogs);

        lang_packs.emplace_back("span", map<string, TextPack> {});
        lang_packs[2].second.emplace("Dialogs", unsupported_dialogs);

        TextPack::FixPacks(bake_languages, lang_packs);

        REQUIRE(lang_packs.size() == 3);
        CHECK(lang_packs[0].first == "engl");
        CHECK(lang_packs[1].first == "russ");
        CHECK(lang_packs[2].first == "germ");

        const auto& russ_pack = lang_packs[1].second;
        REQUIRE(russ_pack.size() == 2);
        CHECK(russ_pack.contains("Dialogs"));
        CHECK(russ_pack.contains("Items"));
        CHECK(russ_pack.at("Dialogs").GetStr(1, 0) == "Hello");
        CHECK(russ_pack.at("Dialogs").GetStr(2, 0) == "Mir");
        CHECK(russ_pack.at("Dialogs").GetStr(3).empty());
        CHECK(russ_pack.at("Items").GetStr(10, 0) == "Item");

        const auto& germ_pack = lang_packs[2].second;
        REQUIRE(germ_pack.size() == 2);
        CHECK(germ_pack.at("Dialogs").GetStr(1, 0) == "Hello");
        CHECK(germ_pack.at("Dialogs").GetStr(2, 0) == "World");
        CHECK(germ_pack.at("Items").GetStr(10, 0) == "Item");
    }

    SECTION("FixPacksBootstrapsDefaultLanguageWhenInputIsEmpty")
    {
        vector<string> bake_languages {"engl", "russ"};
        vector<pair<string, map<string, TextPack>>> lang_packs;

        TextPack::FixPacks(bake_languages, lang_packs);

        REQUIRE(lang_packs.size() == 2);
        CHECK(lang_packs[0].first == "engl");
        CHECK(lang_packs[1].first == "russ");
        CHECK(lang_packs[0].second.empty());
        CHECK(lang_packs[1].second.empty());
    }
}

FO_END_NAMESPACE
