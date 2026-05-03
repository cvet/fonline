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

#include "HashedString.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE

namespace
{
    HashStorage TestHashes;

    auto MakeKey(string_view collection, string_view key1, string_view key2 = {}) -> TextPackKey
    {
        return TextPackKey::FromParts(TestHashes, collection, key1, key2);
    }

    auto FormatKey(const TextPackKey& key) -> string
    {
        return strex("{}", key).str();
    }
}

TEST_CASE("TextPack")
{
    SECTION("TextPackKeyFormatsAndParsesStructuredTuple")
    {
        const auto key = TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name", "Short");
        CHECK(FormatKey(key) == "{Items}{LaserRifle}{Name}{Short}");

        TextPackKey parsed;
        REQUIRE(TextPackKey::Parse(TestHashes, FormatKey(key), parsed));
        CHECK(parsed == key);
    }

    SECTION("LoadFromStringParsesKeysAndMultilineValues")
    {
        TextPack pack(TestHashes);

        const string input = "{100}{}{Hello}\n{200}{3}{World}\n{300}{}{Line1\nLine2}";

        REQUIRE(pack.LoadFromString(input, "Dialogs"));
        CHECK(pack.GetSize() == 3);
        CHECK(pack.GetStr(MakeKey("Dialogs", "100"), 0) == "Hello");
        CHECK(pack.GetStr(MakeKey("Dialogs", "200", "3"), 0) == "World");
        CHECK(pack.GetStr(MakeKey("Dialogs", "300"), 0) == "Line1\nLine2");
        CHECK(pack.GetStrCount(MakeKey("Dialogs", "300")) == 1);
        CHECK(pack.GetStr(MakeKey("Dialogs", "9999")).empty());
    }

    SECTION("LoadFromStringSupportsNamedKeysAndOffsets")
    {
        TextPack pack(TestHashes);

        const string input = "{QuestEntry}{}{Base}\n{QuestEntry}{Suffix}{Combined}";

        REQUIRE(pack.LoadFromString(input, "Quest"));
        CHECK(pack.GetStr(MakeKey("Quest", "QuestEntry"), 0) == "Base");
        CHECK(pack.GetStr(MakeKey("Quest", "QuestEntry", "Suffix"), 0) == "Combined");
    }

    SECTION("LoadFromStringParsesStructuredTupleKeys")
    {
        TextPack pack(TestHashes);

        const auto key = TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name");
        const string input = "{LaserRifle}{Name}{Scoped energy rifle}";

        REQUIRE(pack.LoadFromString(input, "Items"));
        CHECK(pack.GetStr(key, 0) == "Scoped energy rifle");
        CHECK(pack.GetStrCount(key) == 1);
    }

    SECTION("BinaryRoundtripPreservesEntries")
    {
        TextPack pack(TestHashes);
        const auto structured_key = TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name");
        const auto dialogs_ten = MakeKey("Dialogs", "10");
        const auto dialogs_twenty = MakeKey("Dialogs", "20");

        pack.AddStr(structured_key, string_view {"Laser Rifle"});
        pack.AddStr(dialogs_ten, string_view {"Alpha"});
        pack.AddStr(dialogs_twenty, string_view {"Beta"});
        pack.AddStr(dialogs_twenty, string_view {"Gamma"});

        const auto data = pack.GetBinaryData();

        TextPack restored(TestHashes);
        REQUIRE(restored.LoadFromBinaryData(data));
        CHECK(restored.GetSize() == 4);
        CHECK(restored.GetStr(structured_key, 0) == "Laser Rifle");
        CHECK(restored.GetStrCount(structured_key) == 1);
        CHECK(restored.GetStrCount(dialogs_ten) == 1);
        CHECK(restored.GetStrCount(dialogs_twenty) == 2);
        CHECK(restored.GetStr(dialogs_ten, 0) == "Alpha");
        CHECK(restored.GetStr(dialogs_twenty, 0) == "Beta");
        CHECK(restored.GetStr(dialogs_twenty, 1) == "Gamma");
    }

    SECTION("FixStrAddsMissingAndRemovesUnknownKeys")
    {
        TextPack base_pack(TestHashes);
        base_pack.AddStr(MakeKey("Dialogs", "1"), string_view {"BaseOne"});
        base_pack.AddStr(MakeKey("Dialogs", "2"), string_view {"BaseTwo"});

        TextPack localized_pack(TestHashes);
        localized_pack.AddStr(MakeKey("Dialogs", "2"), string_view {"LocalizedTwo"});
        localized_pack.AddStr(MakeKey("Dialogs", "3"), string_view {"Unexpected"});

        localized_pack.FixStr(base_pack);

        CHECK(localized_pack.GetSize() == 2);
        CHECK(localized_pack.GetStr(MakeKey("Dialogs", "1"), 0) == "BaseOne");
        CHECK(localized_pack.GetStr(MakeKey("Dialogs", "2"), 0) == "LocalizedTwo");
        CHECK(localized_pack.GetStr(MakeKey("Dialogs", "3")).empty());
    }

    SECTION("FixStrMatchesStructuredKeysDirectly")
    {
        TextPack base_pack(TestHashes);
        base_pack.AddStr(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name"), string_view {"Laser Rifle"});
        base_pack.AddStr(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Desc"), string_view {"Base description"});

        TextPack localized_pack(TestHashes);
        localized_pack.AddStr(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name"), string_view {"Лазерная винтовка"});
        localized_pack.AddStr(TextPackKey::FromParts(TestHashes, "Items", "Unused", "Desc"), string_view {"Лишнее"});

        localized_pack.FixStr(base_pack);

        CHECK(localized_pack.GetStr(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name"), 0) == "Лазерная винтовка");
        CHECK(localized_pack.GetStr(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Desc"), 0) == "Base description");
        CHECK(localized_pack.GetStr(TextPackKey::FromParts(TestHashes, "Items", "Unused", "Desc")).empty());
    }

    SECTION("LoadFromMapAndClearWorkTogether")
    {
        TextPack pack(TestHashes);
        const auto structured_key = TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Desc");
        const map<string, string> entries {{"7", "Seven"}, {"11", "Eleven"}, {FormatKey(structured_key), "Description"}};

        pack.LoadFromMap(entries, "Dialogs");

        CHECK(pack.GetSize() == 3);
        CHECK(pack.GetStr(MakeKey("Dialogs", "7"), 0) == "Seven");
        CHECK(pack.GetStr(MakeKey("Dialogs", "11"), 0) == "Eleven");
        CHECK(pack.GetStr(structured_key, 0) == "Description");

        pack.Clear();

        CHECK(pack.GetSize() == 0);
        CHECK(pack.GetStr(MakeKey("Dialogs", "7")).empty());
        CHECK(pack.GetStr(structured_key).empty());
    }

    SECTION("MergeEraseAndIntersectionTrackSharedKeys")
    {
        TextPack base_pack(TestHashes);
        base_pack.AddStr(MakeKey("Dialogs", "1"), string_view {"BaseOne"});
        base_pack.AddStr(MakeKey("Dialogs", "2"), string_view {"BaseTwo"});

        TextPack incoming_pack(TestHashes);
        incoming_pack.AddStr(MakeKey("Dialogs", "2"), string_view {"IncomingTwo"});
        incoming_pack.AddStr(MakeKey("Dialogs", "3"), string_view {"IncomingThree"});

        CHECK(base_pack.CheckIntersections(incoming_pack));

        base_pack.Merge(incoming_pack);

        CHECK(base_pack.GetSize() == 4);
        CHECK(base_pack.GetStrCount(MakeKey("Dialogs", "2")) == 2);
        CHECK(base_pack.GetStr(MakeKey("Dialogs", "2"), 0) == "BaseTwo");
        CHECK(base_pack.GetStr(MakeKey("Dialogs", "2"), 1) == "IncomingTwo");
        CHECK(base_pack.GetStr(MakeKey("Dialogs", "3"), 0) == "IncomingThree");

        base_pack.EraseStr(MakeKey("Dialogs", "2"));

        CHECK(base_pack.GetStrCount(MakeKey("Dialogs", "2")) == 0);
        CHECK(base_pack.GetStr(MakeKey("Dialogs", "2")).empty());
        CHECK(base_pack.GetSize() == 2);

        TextPack disjoint_pack(TestHashes);
        disjoint_pack.AddStr(MakeKey("Dialogs", "99"), string_view {"OnlyHere"});
        CHECK_FALSE(base_pack.CheckIntersections(disjoint_pack));
    }

    SECTION("GetStrSkipOutOfRangeReturnsEmptyString")
    {
        TextPack pack(TestHashes);
        const auto key = MakeKey("Dialogs", "5");
        pack.AddStr(key, string_view {"Alpha"});
        pack.AddStr(key, string_view {"Beta"});

        CHECK(pack.GetStr(key, 0) == "Alpha");
        CHECK(pack.GetStr(key, 1) == "Beta");
        CHECK(pack.GetStr(key, 2).empty());
        CHECK(pack.GetStr(MakeKey("Dialogs", "42"), 0).empty());
    }

    SECTION("MalformedLoadFromStringReportsFailureAfterKeepingValidEntries")
    {
        TextPack pack(TestHashes);

        const string input = "{10}{}{Valid}\n{20}{Broken\n{30}{}{StillValid}";

        CHECK_FALSE(pack.LoadFromString(input, "Dialogs"));
        CHECK(pack.GetStr(MakeKey("Dialogs", "10"), 0) == "Valid");
        CHECK(pack.GetStr(MakeKey("Dialogs", "20")).empty());
        CHECK(pack.GetStr(MakeKey("Dialogs", "30"), 0) == "StillValid");
        CHECK(pack.GetSize() == 2);
    }

    SECTION("FixPacksAddsMissingLanguagesAndNormalizesAgainstBase")
    {
        vector<string> bake_languages {"engl", "russ", "germ"};
        vector<pair<string, map<string, TextPack>>> lang_packs;

        TextPack engl_dialogs(TestHashes);
        engl_dialogs.AddStr(MakeKey("Dialogs", "1"), string_view {"Hello"});
        engl_dialogs.AddStr(MakeKey("Dialogs", "2"), string_view {"World"});

        TextPack engl_items(TestHashes);
        engl_items.AddStr(MakeKey("Items", "10"), string_view {"Item"});

        TextPack russ_dialogs(TestHashes);
        russ_dialogs.AddStr(MakeKey("Dialogs", "2"), string_view {"Mir"});
        russ_dialogs.AddStr(MakeKey("Dialogs", "3"), string_view {"Extra"});

        TextPack unsupported_dialogs(TestHashes);
        unsupported_dialogs.AddStr(MakeKey("Dialogs", "1"), string_view {"Hola"});

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
        CHECK(russ_pack.at("Dialogs").GetStr(MakeKey("Dialogs", "1"), 0) == "Hello");
        CHECK(russ_pack.at("Dialogs").GetStr(MakeKey("Dialogs", "2"), 0) == "Mir");
        CHECK(russ_pack.at("Dialogs").GetStr(MakeKey("Dialogs", "3")).empty());
        CHECK(russ_pack.at("Items").GetStr(MakeKey("Items", "10"), 0) == "Item");

        const auto& germ_pack = lang_packs[2].second;
        REQUIRE(germ_pack.size() == 2);
        CHECK(germ_pack.at("Dialogs").GetStr(MakeKey("Dialogs", "1"), 0) == "Hello");
        CHECK(germ_pack.at("Dialogs").GetStr(MakeKey("Dialogs", "2"), 0) == "World");
        CHECK(germ_pack.at("Items").GetStr(MakeKey("Items", "10"), 0) == "Item");
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
