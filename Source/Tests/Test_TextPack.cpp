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

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "HashedString.h"
#include "Test_BakerHelpers.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE

namespace
{
    hash_storage TestHashes;

    auto MakeKey(string_view collection, string_view key1, string_view key2 = {}) -> TextPackKey
    {
        return TextPackKey::FromParts(TestHashes, collection, key1, key2);
    }

    auto FormatKey(const TextPackKey& key) -> string
    {
        return strex("{}", key).str();
    }

    auto MakeTruncatedTextPack(TextPackKey first_key) -> vector<uint8_t>
    {
        vector<uint8_t> data;
        data_writer writer {data};
        writer.write<uint32_t>(uint32_t {2});
        for (hstring part : {first_key.Collection.underlying_value(), first_key.Key1, first_key.Key2, first_key.Key3}) {
            writer.write_string(part.as_str());
        }
        writer.write_string("Accepted prefix");
        for (hstring part : {first_key.Collection.underlying_value(), first_key.Key1, first_key.Key2, first_key.Key3}) {
            writer.write_string(part.as_str());
        }
        writer.write<uint32_t>(uint32_t {3});
        writer.write_string_bytes("x");
        return data;
    }
}

TEST_CASE("TextPackResourceBatchMatchesSequentialLoads", "[text-pack]")
{
    string language = GENERATE(string {"engl"}, string {"russ"}, string {});
    CAPTURE(language);
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("TextBatch");
    TextPackKey duplicate = MakeKey("Shared", "Duplicate");

    for (int32_t index = 0; index < 64; index++) {
        string pack_name = strex("Pack{:02}", 63 - index).str();
        for (string_view locale : {string_view {"engl"}, string_view {"russ"}}) {
            TextPack pack {&TestHashes};
            pack.AddStr(duplicate, strex("{} {} first", locale, index).str());
            pack.AddStr(MakeKey("Other", pack_name), strex("{} {} unique", locale, index).str());
            pack.AddStr(duplicate, strex("{} {} second", locale, index).str());
            // Empty serialized collections inherit the text-pack name from the filename
            pack.AddStr(MakeKey("", "Local"), string_view {"Local value"});
            source->AddFile(strex("Texts.{}.{}.fotxt-bin", pack_name, locale).str(), pack.GetBinaryData());
        }
    }
    source->AddFile("unrelated.fotxt", "Ignored");
    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    TextPack batch {&TestHashes};
    TextPack sequential {&TestHashes};
    for (ptr<TextPack> pack : {make_ptr(&batch), make_ptr(&sequential)}) {
        pack->AddStr(duplicate, string_view {"Existing first"});
        pack->AddStr(MakeKey("Seed", "Unsorted"), string_view {"Existing other"});
        pack->AddStr(duplicate, string_view {"Existing second"});
    }

    for (int32_t pass = 0; pass < 2; pass++) {
        vector<string> expected {"Existing first", "Existing second"};
        for (int32_t repetition = 0; repetition <= pass; repetition++) {
            for (const FileHeader& header : resources.FilterFiles("fotxt-bin")) {
                vector<string_view> parts = strvex(header.GetNameNoExt()).split('.');
                if (!language.empty() && parts[2] != language) {
                    continue;
                }
                File file = File::Load(header);
                if (repetition == pass) {
                    REQUIRE(sequential.LoadFromBinaryData(file.GetData(), parts[1]));
                }
                TextPack pack {&TestHashes};
                REQUIRE(pack.LoadFromBinaryData(file.GetData(), parts[1]));
                expected.emplace_back(pack.GetStr(duplicate, 0));
                expected.emplace_back(pack.GetStr(duplicate, 1));
            }
        }

        batch.LoadFromResources(resources, language);
        REQUIRE(batch.GetBinaryData() == sequential.GetBinaryData());
        REQUIRE(batch.GetStrCount(duplicate) == expected.size());
        for (size_t index = 0; index < expected.size(); index++) {
            CHECK(batch.GetStr(duplicate, index) == expected[index]);
        }
        CHECK(batch.GetStr(MakeKey("Pack00", "Local"), 0) == "Local value");
        CHECK(batch.GetStr(MakeKey("Pack63", "Local"), 0) == "Local value");
    }
}

TEST_CASE("TextPackDecodeFailureSkipsSortUntilRetry", "[text-pack]")
{
    TextPackKey lower = MakeKey("Recovery", "One");
    TextPackKey higher = MakeKey("Recovery", "Two");
    if (higher < lower) {
        std::swap(lower, higher);
    }
    TextPack pack {&TestHashes};
    pack.AddStr(higher, string_view {"Existing"});
    vector<uint8_t> invalid = MakeTruncatedTextPack(lower);
    bool resources_route = GENERATE(false, true);
    CAPTURE(resources_route);
    if (resources_route) {
        auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("TextRecovery");
        source->AddFile("Texts.Recovery.engl.fotxt-bin", invalid);
        FileSystem resources;
        resources.AddCustomSource(std::move(source));
        CHECK_THROWS_AS(pack.LoadFromResources(resources, "engl"), DataReadingException);
    }
    else {
        CHECK_THROWS_AS(pack.LoadFromBinaryData(invalid), DataReadingException);
    }
    CHECK(pack.GetSize() == 2);
    CHECK_THROWS_AS(pack.GetStr(higher, 0), VerificationException);
    CHECK_THROWS_AS(pack.GetStr(lower, 0), VerificationException);
    TextPack retry {&TestHashes};
    retry.AddStr(lower, string_view {"Retry"});
    REQUIRE(pack.LoadFromBinaryData(retry.GetBinaryData()));
    CHECK(pack.GetStr(higher, 0) == "Existing");
    CHECK(pack.GetStrCount(lower) == 2);
    CHECK(pack.GetStr(lower, 0) == "Accepted prefix");
    CHECK(pack.GetStr(lower, 1) == "Retry");
}

TEST_CASE("TextPackResourceBatchFailureRetainsEarlierFilesUntilRetry", "[text-pack]")
{
    TextPackKey lower = MakeKey("Recovery", "One");
    TextPackKey higher = MakeKey("Recovery", "Two");
    if (higher < lower) {
        std::swap(lower, higher);
    }
    TextPack first {&TestHashes};
    first.AddStr(lower, string_view {"First one"});
    first.AddStr(lower, string_view {"First two"});
    vector<uint8_t> invalid;
    data_writer writer {invalid};
    writer.write<uint32_t>(uint32_t {1});
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("TextBatchRecovery");
    ptr<BakerTests::MemoryDataSource> source_ref = source.get();
    source->AddFile("Texts.First.engl.fotxt-bin", first.GetBinaryData());
    source->AddFile("Texts.Second.engl.fotxt-bin", invalid);
    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    auto files = resources.FilterFiles("fotxt-bin");
    REQUIRE(files.GetFilesCount() == 2);
    REQUIRE(files.GetFileByIndex(0).GetNameNoExt() == "Texts.First.engl");
    REQUIRE(files.GetFileByIndex(1).GetNameNoExt() == "Texts.Second.engl");
    TextPack pack {&TestHashes};
    pack.AddStr(higher, string_view {"Existing"});

    CHECK_THROWS_AS(pack.LoadFromResources(resources, "engl"), DataReadingException);
    CHECK(pack.GetSize() == 3);
    CHECK_THROWS_AS(pack.GetStr(higher, 0), VerificationException);
    CHECK_THROWS_AS(pack.GetStrCount(lower), VerificationException);

    TextPack second {&TestHashes};
    second.AddStr(lower, string_view {"Second"});
    source_ref->AddFile("Texts.Second.engl.fotxt-bin", second.GetBinaryData());
    pack.LoadFromResources(resources, "engl");
    CHECK(pack.GetSize() == 6);
    CHECK(pack.GetStr(higher, 0) == "Existing");
    REQUIRE(pack.GetStrCount(lower) == 5);
    CHECK(pack.GetStr(lower, 0) == "First one");
    CHECK(pack.GetStr(lower, 1) == "First two");
    CHECK(pack.GetStr(lower, 2) == "First one");
    CHECK(pack.GetStr(lower, 3) == "First two");
    CHECK(pack.GetStr(lower, 4) == "Second");
}

TEST_CASE("TextPackBinaryRejectsImpossibleSizesBeforeAllocation", "[text-pack]")
{
    int32_t invalid_kind = GENERATE(0, 1, 2);
    CAPTURE(invalid_kind);
    vector<uint8_t> invalid;
    data_writer writer {invalid};
    writer.write<uint32_t>(invalid_kind == 0 ? std::numeric_limits<uint32_t>::max() : uint32_t {1});
    if (invalid_kind != 0) {
        for (int32_t field = 0; field < 5; field++) {
            writer.write<uint32_t>(field == (invalid_kind == 1 ? 0 : 4) ? std::numeric_limits<uint32_t>::max() : uint32_t {0});
        }
    }
    TextPack pack {&TestHashes};
    pack.AddStr(MakeKey("Existing", "Keep"), string_view {"Existing value"});
    vector<uint8_t> before = pack.GetBinaryData();
    CHECK_THROWS_AS(pack.LoadFromBinaryData(invalid), DataReadingException);
    CHECK(pack.GetBinaryData() == before);
}

TEST_CASE("TextPackBatchPreservesEmptyAndFilteredInputs", "[text-pack]")
{
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("TextEmpty");
    TextPack empty {&TestHashes};
    vector<uint8_t> padded = empty.GetBinaryData();
    padded.emplace_back(uint8_t {123});
    source->AddFile("Texts.Empty.engl.fotxt-bin", padded);
    TextPack other {&TestHashes};
    other.AddStr(MakeKey("Foreign", "Key"), string_view {"Foreign value"});
    source->AddFile("Texts.Foreign.russ.fotxt-bin", other.GetBinaryData());
    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    TextPack pack {&TestHashes};
    TextPackKey first = MakeKey("Seed", "One");
    TextPackKey second = MakeKey("Seed", "Two");
    if (first < second) {
        std::swap(first, second);
    }
    pack.AddStr(first, string_view {"First"});
    pack.AddStr(second, string_view {"Second"});
    pack.LoadFromResources(resources, "engl");
    CHECK(pack.GetSize() == 2);
    CHECK(pack.GetStr(first, 0) == "First");
    CHECK(pack.GetStr(second, 0) == "Second");
    CHECK(pack.GetStr(MakeKey("Foreign", "Key"), 0).empty());
    FileSystem no_files;
    pack.LoadFromResources(no_files);
    CHECK(pack.GetSize() == 2);
    REQUIRE(pack.LoadFromBinaryData(padded));
    CHECK(pack.GetSize() == 2);
}

TEST_CASE("TextPack")
{
    SECTION("TextPackKeyFormatsAndParsesStructuredTuple")
    {
        auto key = TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name", "Short");
        CHECK(FormatKey(key) == "{Items}{LaserRifle}{Name}{Short}");

        TextPackKey parsed;
        REQUIRE(TextPackKey::Parse(TestHashes, FormatKey(key), parsed));
        CHECK(parsed == key);
    }

    SECTION("LoadFromStringParsesKeysAndMultilineValues")
    {
        TextPack pack(&TestHashes);

        string input = "{100}{}{Hello}\n{200}{3}{World}\n{300}{}{Line1\nLine2}";

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
        TextPack pack(&TestHashes);

        string input = "{QuestEntry}{}{Base}\n{QuestEntry}{Suffix}{Combined}";

        REQUIRE(pack.LoadFromString(input, "Quest"));
        CHECK(pack.GetStr(MakeKey("Quest", "QuestEntry"), 0) == "Base");
        CHECK(pack.GetStr(MakeKey("Quest", "QuestEntry", "Suffix"), 0) == "Combined");
    }

    SECTION("LoadFromStringParsesStructuredTupleKeys")
    {
        TextPack pack(&TestHashes);

        auto key = TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name");
        string input = "{LaserRifle}{Name}{Scoped energy rifle}";

        REQUIRE(pack.LoadFromString(input, "Items"));
        CHECK(pack.GetStr(key, 0) == "Scoped energy rifle");
        CHECK(pack.GetStrCount(key) == 1);
    }

    SECTION("BinaryRoundtripPreservesEntries")
    {
        TextPack pack(&TestHashes);
        auto structured_key = TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name");
        auto dialogs_ten = MakeKey("Dialogs", "10");
        auto dialogs_twenty = MakeKey("Dialogs", "20");

        pack.AddStr(structured_key, string_view {"Laser Rifle"});
        pack.AddStr(dialogs_ten, string_view {"Alpha"});
        pack.AddStr(dialogs_twenty, string_view {"Beta"});
        pack.AddStr(dialogs_twenty, string_view {"Gamma"});

        auto data = pack.GetBinaryData();
        vector<uint8_t> framed {uint8_t {0xA5}};
        framed.insert(framed.end(), data.begin(), data.end());
        framed.emplace_back(uint8_t {0x5A});

        TextPack restored(&TestHashes);
        REQUIRE(restored.LoadFromBinaryData(const_span<uint8_t> {framed}.subspan(1, data.size())));
        std::fill(framed.begin(), framed.end(), uint8_t {});
        CHECK(restored.GetBinaryData() == data);
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
        TextPack base_pack(&TestHashes);
        base_pack.AddStr(MakeKey("Dialogs", "1"), string_view {"BaseOne"});
        base_pack.AddStr(MakeKey("Dialogs", "2"), string_view {"BaseTwo"});

        TextPack localized_pack(&TestHashes);
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
        TextPack base_pack(&TestHashes);
        base_pack.AddStr(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name"), string_view {"Laser Rifle"});
        base_pack.AddStr(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Desc"), string_view {"Base description"});

        TextPack localized_pack(&TestHashes);
        localized_pack.AddStr(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name"), string_view {"Лазерная винтовка"});
        localized_pack.AddStr(TextPackKey::FromParts(TestHashes, "Items", "Unused", "Desc"), string_view {"Лишнее"});

        localized_pack.FixStr(base_pack);

        CHECK(localized_pack.GetStr(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Name"), 0) == "Лазерная винтовка");
        CHECK(localized_pack.GetStr(TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Desc"), 0) == "Base description");
        CHECK(localized_pack.GetStr(TextPackKey::FromParts(TestHashes, "Items", "Unused", "Desc")).empty());
    }

    SECTION("LoadFromMapAndClearWorkTogether")
    {
        TextPack pack(&TestHashes);
        auto structured_key = TextPackKey::FromParts(TestHashes, "Items", "LaserRifle", "Desc");
        map<string, string> entries {{"7", "Seven"}, {"11", "Eleven"}, {FormatKey(structured_key), "Description"}};

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
        TextPack base_pack(&TestHashes);
        base_pack.AddStr(MakeKey("Dialogs", "1"), string_view {"BaseOne"});
        base_pack.AddStr(MakeKey("Dialogs", "2"), string_view {"BaseTwo"});

        TextPack incoming_pack(&TestHashes);
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

        TextPack disjoint_pack(&TestHashes);
        disjoint_pack.AddStr(MakeKey("Dialogs", "99"), string_view {"OnlyHere"});
        CHECK_FALSE(base_pack.CheckIntersections(disjoint_pack));
    }

    SECTION("GetStrSkipOutOfRangeReturnsEmptyString")
    {
        TextPack pack(&TestHashes);
        auto key = MakeKey("Dialogs", "5");
        pack.AddStr(key, string_view {"Alpha"});
        pack.AddStr(key, string_view {"Beta"});

        CHECK(pack.GetStr(key, 0) == "Alpha");
        CHECK(pack.GetStr(key, 1) == "Beta");
        CHECK(pack.GetStr(key, 2).empty());
        CHECK(pack.GetStr(MakeKey("Dialogs", "42"), 0).empty());
    }

    SECTION("RandomVariantPickIsUniformAcrossTheWholeSet")
    {
        // An off-by-one in the pick gives the first line two of the set's shares, which a player hears
        // as a character repeating one phrase, so the distribution itself is this lookup's contract
        constexpr size_t VARIANT_COUNT = 4;
        constexpr size_t SAMPLE_COUNT = 20000;
        constexpr size_t EXPECTED = SAMPLE_COUNT / VARIANT_COUNT;
        // Generous enough that a fair generator never trips it, tight enough to catch the doubled first share
        constexpr size_t TOLERANCE = EXPECTED / 2;

        TextPack pack(&TestHashes);
        auto key = MakeKey("Dialogs", "Bark");
        vector<string> variants {"Alpha", "Beta", "Gamma", "Delta"};

        for (const auto& variant : variants) {
            pack.AddStr(key, string_view {variant});
        }

        REQUIRE(pack.GetStrCount(key) == VARIANT_COUNT);

        map<string, size_t> hits;
        for (size_t i = 0; i < SAMPLE_COUNT; i++) {
            hits[string(pack.GetStr(key))]++;
        }

        REQUIRE(hits.size() == VARIANT_COUNT);

        for (const auto& variant : variants) {
            size_t count = hits[variant];
            INFO("variant " << variant << " picked " << count << " times, expected about " << EXPECTED);
            CHECK(count > EXPECTED - TOLERANCE);
            CHECK(count < EXPECTED + TOLERANCE);
        }
    }

    SECTION("MalformedLoadFromStringReportsFailureAfterKeepingValidEntries")
    {
        TextPack pack(&TestHashes);

        string input = "{10}{}{Valid}\n{20}{Broken\n{30}{}{StillValid}";

        CHECK_FALSE(pack.LoadFromString(input, "Dialogs"));
        CHECK(pack.GetStr(MakeKey("Dialogs", "10"), 0) == "Valid");
        CHECK(pack.GetStr(MakeKey("Dialogs", "20")).empty());
        CHECK(pack.GetStr(MakeKey("Dialogs", "30"), 0) == "StillValid");
        CHECK(pack.GetSize() == 2);
    }

    SECTION("FixPacksAddsMissingLanguagesAndNormalizesAgainstBase")
    {
        BakeLanguageConfig bake_languages = TextPack::ParseBakeLanguages(vector<string> {"engl", "russ", "germ"});
        vector<pair<string, map<string, TextPack>>> lang_packs;

        TextPack engl_dialogs(&TestHashes);
        engl_dialogs.AddStr(MakeKey("Dialogs", "1"), string_view {"Hello"});
        engl_dialogs.AddStr(MakeKey("Dialogs", "2"), string_view {"World"});

        TextPack engl_items(&TestHashes);
        engl_items.AddStr(MakeKey("Items", "10"), string_view {"Item"});

        TextPack russ_dialogs(&TestHashes);
        russ_dialogs.AddStr(MakeKey("Dialogs", "2"), string_view {"Mir"});
        russ_dialogs.AddStr(MakeKey("Dialogs", "3"), string_view {"Extra"});

        TextPack unsupported_dialogs(&TestHashes);
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
        BakeLanguageConfig bake_languages = TextPack::ParseBakeLanguages(vector<string> {"engl", "russ"});
        vector<pair<string, map<string, TextPack>>> lang_packs;

        TextPack::FixPacks(bake_languages, lang_packs);

        REQUIRE(lang_packs.size() == 2);
        CHECK(lang_packs[0].first == "engl");
        CHECK(lang_packs[1].first == "russ");
        CHECK(lang_packs[0].second.empty());
        CHECK(lang_packs[1].second.empty());
    }

    SECTION("FixPacksInheritsMissingTextFromConfiguredLanguageParent")
    {
        BakeLanguageConfig bake_languages = TextPack::ParseBakeLanguages(vector<string> {"russ", "engl", "ru18:russ", "en18:engl"});
        vector<pair<string, map<string, TextPack>>> lang_packs;

        TextPack russ_dialogs(&TestHashes);
        russ_dialogs.AddStr(MakeKey("Dialogs", "1"), string_view {"Базовая реплика"});
        russ_dialogs.AddStr(MakeKey("Dialogs", "2"), string_view {"Базовый ответ"});

        TextPack engl_dialogs(&TestHashes);
        engl_dialogs.AddStr(MakeKey("Dialogs", "1"), string_view {"Base line"});
        engl_dialogs.AddStr(MakeKey("Dialogs", "2"), string_view {"Base answer"});

        TextPack en18_dialogs(&TestHashes);
        en18_dialogs.AddStr(MakeKey("Dialogs", "2"), string_view {"Adult answer"});

        lang_packs.emplace_back("russ", map<string, TextPack> {});
        lang_packs[0].second.emplace("Dialogs", russ_dialogs);
        lang_packs.emplace_back("engl", map<string, TextPack> {});
        lang_packs[1].second.emplace("Dialogs", engl_dialogs);
        lang_packs.emplace_back("en18", map<string, TextPack> {});
        lang_packs[2].second.emplace("Dialogs", en18_dialogs);

        TextPack::FixPacks(bake_languages, lang_packs);

        auto en18_it = std::ranges::find_if(lang_packs, [](const auto& lang_pack) { return lang_pack.first == "en18"; });
        REQUIRE(en18_it != lang_packs.end());
        const auto& en18_pack = en18_it->second.at("Dialogs");
        CHECK(en18_pack.GetStr(MakeKey("Dialogs", "1"), 0) == "Base line");
        CHECK(en18_pack.GetStr(MakeKey("Dialogs", "2"), 0) == "Adult answer");

        auto ru18_it = std::ranges::find_if(lang_packs, [](const auto& lang_pack) { return lang_pack.first == "ru18"; });
        REQUIRE(ru18_it != lang_packs.end());
        const auto& ru18_pack = ru18_it->second.at("Dialogs");
        CHECK(ru18_pack.GetStr(MakeKey("Dialogs", "1"), 0) == "Базовая реплика");
        CHECK(ru18_pack.GetStr(MakeKey("Dialogs", "2"), 0) == "Базовый ответ");
    }

    SECTION("ParseBakeLanguagesNormalizesInlineFallbacks")
    {
        BakeLanguageConfig bake_languages = TextPack::ParseBakeLanguages(vector<string> {"russ", "engl", "ru18:russ", "en18:engl"});

        CHECK(bake_languages.Languages == vector<string> {"russ", "engl", "ru18", "en18"});
        CHECK(bake_languages.Fallbacks == map<string, string> {{"en18", "engl"}, {"ru18", "russ"}});
    }

    SECTION("ParseBakeLanguagesRejectsInvalidDeclarations")
    {
        CHECK_THROWS(TextPack::ParseBakeLanguages(vector<string> {}));
        CHECK_THROWS(TextPack::ParseBakeLanguages(vector<string> {"russ", "en18:"}));
        CHECK_THROWS(TextPack::ParseBakeLanguages(vector<string> {"russ", ":russ"}));
        CHECK_THROWS(TextPack::ParseBakeLanguages(vector<string> {"russ", "en18:engl:extra"}));
        CHECK_THROWS(TextPack::ParseBakeLanguages(vector<string> {"russ", "en18:engl"}));
        CHECK_THROWS(TextPack::ParseBakeLanguages(vector<string> {"russ", "engl:en18", "en18"}));
        CHECK_THROWS(TextPack::ParseBakeLanguages(vector<string> {"russ", "engl", "en18:engl", "en18:russ"}));
    }
}

FO_END_NAMESPACE
