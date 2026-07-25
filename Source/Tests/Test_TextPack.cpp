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

    template<size_t Size>
    auto TextEquals(u8string_view value, const char8_t (&expected)[Size]) -> bool
    {
        return value.native_view() == std::u8string_view {expected, Size - 1};
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

    SECTION("LoadFromTextParsesKeysAndMultilineValues")
    {
        TextPack pack(&TestHashes);

        const u8string input {u8"{100}{}{Hello}\n{200}{3}{World}\n{300}{}{Line1\nLine2}"};

        REQUIRE(pack.LoadFromText(input.view(), "Dialogs"));
        CHECK(pack.GetSize() == 3);
        CHECK(TextEquals(pack.GetText(MakeKey("Dialogs", "100"), 0), u8"Hello"));
        CHECK(TextEquals(pack.GetText(MakeKey("Dialogs", "200", "3"), 0), u8"World"));
        CHECK(TextEquals(pack.GetText(MakeKey("Dialogs", "300"), 0), u8"Line1\nLine2"));
        CHECK(pack.GetTextCount(MakeKey("Dialogs", "300")) == 1);
        CHECK(pack.GetText(MakeKey("Dialogs", "9999")).empty());
    }

    SECTION("LoadFromTextSupportsNamedKeysAndOffsets")
    {
        TextPack pack(&TestHashes);

        const u8string input {u8"{QuestEntry}{}{Base}\n{QuestEntry}{Suffix}{Combined}"};

        REQUIRE(pack.LoadFromText(input.view(), "Quest"));
        CHECK(TextEquals(pack.GetText(MakeKey("Quest", "QuestEntry"), 0), u8"Base"));
        CHECK(TextEquals(pack.GetText(MakeKey("Quest", "QuestEntry", "Suffix"), 0), u8"Combined"));
    }

    SECTION("LoadFromTextParsesStructuredTupleKeys")
    {
        TextPack pack(&TestHashes);

        const auto key = TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name");
        const u8string input {u8"{LaserRifle}{Name}{Scoped energy rifle}"};

        REQUIRE(pack.LoadFromText(input.view(), "Items"));
        CHECK(TextEquals(pack.GetText(key, 0), u8"Scoped energy rifle"));
        CHECK(pack.GetTextCount(key) == 1);
    }

    SECTION("BinaryRoundtripPreservesEntries")
    {
        TextPack pack(&TestHashes);
        const auto structured_key = TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name");
        const auto dialogs_ten = MakeKey("Dialogs", "10");
        const auto dialogs_twenty = MakeKey("Dialogs", "20");

        pack.AddText(structured_key, u8"Laser Rifle");
        pack.AddText(dialogs_ten, u8"Alpha");
        pack.AddText(dialogs_twenty, u8"Beta");
        pack.AddText(dialogs_twenty, u8"Gamma");

        const auto data = pack.GetBinaryData();

        TextPack restored(&TestHashes);
        REQUIRE(restored.LoadFromBinaryData(data));
        CHECK(restored.GetSize() == 4);
        CHECK(TextEquals(restored.GetText(structured_key, 0), u8"Laser Rifle"));
        CHECK(restored.GetTextCount(structured_key) == 1);
        CHECK(restored.GetTextCount(dialogs_ten) == 1);
        CHECK(restored.GetTextCount(dialogs_twenty) == 2);
        CHECK(TextEquals(restored.GetText(dialogs_ten, 0), u8"Alpha"));
        CHECK(TextEquals(restored.GetText(dialogs_twenty, 0), u8"Beta"));
        CHECK(TextEquals(restored.GetText(dialogs_twenty, 1), u8"Gamma"));
    }

    SECTION("BinaryRoundtripValidatesUnicodePayloads")
    {
        TextPack pack(&TestHashes);
        const auto key = MakeKey("Dialogs", "Unicode");
        pack.AddText(key, u8"Привет, 世界 🌍 é �");

        vector<byte> data = pack.GetBinaryData();
        TextPack restored(&TestHashes);
        REQUIRE(restored.LoadFromBinaryData(data));
        CHECK(TextEquals(restored.GetText(key, 0), u8"Привет, 世界 🌍 é �"));

        REQUIRE(!data.empty());
        data.back() = byte {0xFF};
        TextPack malformed(&TestHashes);
        CHECK_THROWS_AS((void)malformed.LoadFromBinaryData(data), TextValidationException);
    }

    SECTION("FixTextAddsMissingAndRemovesUnknownKeys")
    {
        TextPack base_pack(&TestHashes);
        base_pack.AddText(MakeKey("Dialogs", "1"), u8"BaseOne");
        base_pack.AddText(MakeKey("Dialogs", "2"), u8"BaseTwo");

        TextPack localized_pack(&TestHashes);
        localized_pack.AddText(MakeKey("Dialogs", "2"), u8"LocalizedTwo");
        localized_pack.AddText(MakeKey("Dialogs", "3"), u8"Unexpected");

        localized_pack.FixText(base_pack);

        CHECK(localized_pack.GetSize() == 2);
        CHECK(TextEquals(localized_pack.GetText(MakeKey("Dialogs", "1"), 0), u8"BaseOne"));
        CHECK(TextEquals(localized_pack.GetText(MakeKey("Dialogs", "2"), 0), u8"LocalizedTwo"));
        CHECK(localized_pack.GetText(MakeKey("Dialogs", "3")).empty());
    }

    SECTION("FixTextMatchesStructuredKeysDirectly")
    {
        TextPack base_pack(&TestHashes);
        base_pack.AddText(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name"), u8"Laser Rifle");
        base_pack.AddText(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Desc"), u8"Base description");

        TextPack localized_pack(&TestHashes);
        localized_pack.AddText(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name"), u8"Лазерная винтовка");
        localized_pack.AddText(TextPackKey::FromParts(TestHashes, "Items", "Unused", "Desc"), u8"Лишнее");

        localized_pack.FixText(base_pack);

        CHECK(TextEquals(localized_pack.GetText(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name"), 0), u8"Лазерная винтовка"));
        CHECK(TextEquals(localized_pack.GetText(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Desc"), 0), u8"Base description"));
        CHECK(localized_pack.GetText(TextPackKey::FromParts(TestHashes, "Items", "Unused", "Desc")).empty());
    }

    SECTION("LoadFromTextMapAndClearWorkTogether")
    {
        TextPack pack(&TestHashes);
        const auto structured_key = TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Desc");
        const map<string, u8string> entries {{"7", u8string {u8"Seven"}}, {"11", u8string {u8"Eleven"}}, {FormatKey(structured_key), u8string {u8"Description"}}};

        pack.LoadFromTextMap(entries, "Dialogs");

        CHECK(pack.GetSize() == 3);
        CHECK(TextEquals(pack.GetText(MakeKey("Dialogs", "7"), 0), u8"Seven"));
        CHECK(TextEquals(pack.GetText(MakeKey("Dialogs", "11"), 0), u8"Eleven"));
        CHECK(TextEquals(pack.GetText(structured_key, 0), u8"Description"));

        pack.Clear();

        CHECK(pack.GetSize() == 0);
        CHECK(pack.GetText(MakeKey("Dialogs", "7")).empty());
        CHECK(pack.GetText(structured_key).empty());
    }

    SECTION("MergeEraseAndIntersectionTrackSharedKeys")
    {
        TextPack base_pack(&TestHashes);
        base_pack.AddText(MakeKey("Dialogs", "1"), u8"BaseOne");
        base_pack.AddText(MakeKey("Dialogs", "2"), u8"BaseTwo");

        TextPack incoming_pack(&TestHashes);
        incoming_pack.AddText(MakeKey("Dialogs", "2"), u8"IncomingTwo");
        incoming_pack.AddText(MakeKey("Dialogs", "3"), u8"IncomingThree");

        CHECK(base_pack.CheckIntersections(incoming_pack));

        base_pack.Merge(incoming_pack);

        CHECK(base_pack.GetSize() == 4);
        CHECK(base_pack.GetTextCount(MakeKey("Dialogs", "2")) == 2);
        CHECK(TextEquals(base_pack.GetText(MakeKey("Dialogs", "2"), 0), u8"BaseTwo"));
        CHECK(TextEquals(base_pack.GetText(MakeKey("Dialogs", "2"), 1), u8"IncomingTwo"));
        CHECK(TextEquals(base_pack.GetText(MakeKey("Dialogs", "3"), 0), u8"IncomingThree"));

        base_pack.EraseText(MakeKey("Dialogs", "2"));

        CHECK(base_pack.GetTextCount(MakeKey("Dialogs", "2")) == 0);
        CHECK(base_pack.GetText(MakeKey("Dialogs", "2")).empty());
        CHECK(base_pack.GetSize() == 2);

        TextPack disjoint_pack(&TestHashes);
        disjoint_pack.AddText(MakeKey("Dialogs", "99"), u8"OnlyHere");
        CHECK_FALSE(base_pack.CheckIntersections(disjoint_pack));
    }

    SECTION("GetTextSkipOutOfRangeReturnsEmptyString")
    {
        TextPack pack(&TestHashes);
        const auto key = MakeKey("Dialogs", "5");
        pack.AddText(key, u8"Alpha");
        pack.AddText(key, u8"Beta");

        CHECK(TextEquals(pack.GetText(key, 0), u8"Alpha"));
        CHECK(TextEquals(pack.GetText(key, 1), u8"Beta"));
        CHECK(pack.GetText(key, 2).empty());
        CHECK(pack.GetText(MakeKey("Dialogs", "42"), 0).empty());
    }

    SECTION("MalformedLoadFromTextReportsFailureAfterKeepingValidEntries")
    {
        TextPack pack(&TestHashes);

        const u8string input {u8"{10}{}{Valid}\n{20}{Broken\n{30}{}{StillValid}"};

        CHECK_FALSE(pack.LoadFromText(input.view(), "Dialogs"));
        CHECK(TextEquals(pack.GetText(MakeKey("Dialogs", "10"), 0), u8"Valid"));
        CHECK(pack.GetText(MakeKey("Dialogs", "20")).empty());
        CHECK(TextEquals(pack.GetText(MakeKey("Dialogs", "30"), 0), u8"StillValid"));
        CHECK(pack.GetSize() == 2);
    }

    SECTION("FixPacksAddsMissingLanguagesAndNormalizesAgainstBase")
    {
        vector<string> bake_languages {"engl", "russ", "germ"};
        vector<pair<string, map<string, TextPack>>> lang_packs;

        TextPack engl_dialogs(&TestHashes);
        engl_dialogs.AddText(MakeKey("Dialogs", "1"), u8"Hello");
        engl_dialogs.AddText(MakeKey("Dialogs", "2"), u8"World");

        TextPack engl_items(&TestHashes);
        engl_items.AddText(MakeKey("Items", "10"), u8"Item");

        TextPack russ_dialogs(&TestHashes);
        russ_dialogs.AddText(MakeKey("Dialogs", "2"), u8"Mir");
        russ_dialogs.AddText(MakeKey("Dialogs", "3"), u8"Extra");

        TextPack unsupported_dialogs(&TestHashes);
        unsupported_dialogs.AddText(MakeKey("Dialogs", "1"), u8"Hola");

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
        CHECK(TextEquals(russ_pack.at("Dialogs").GetText(MakeKey("Dialogs", "1"), 0), u8"Hello"));
        CHECK(TextEquals(russ_pack.at("Dialogs").GetText(MakeKey("Dialogs", "2"), 0), u8"Mir"));
        CHECK(russ_pack.at("Dialogs").GetText(MakeKey("Dialogs", "3")).empty());
        CHECK(TextEquals(russ_pack.at("Items").GetText(MakeKey("Items", "10"), 0), u8"Item"));

        const auto& germ_pack = lang_packs[2].second;
        REQUIRE(germ_pack.size() == 2);
        CHECK(TextEquals(germ_pack.at("Dialogs").GetText(MakeKey("Dialogs", "1"), 0), u8"Hello"));
        CHECK(TextEquals(germ_pack.at("Dialogs").GetText(MakeKey("Dialogs", "2"), 0), u8"World"));
        CHECK(TextEquals(germ_pack.at("Items").GetText(MakeKey("Items", "10"), 0), u8"Item"));
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
