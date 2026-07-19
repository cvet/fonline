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

#include "catch_amalgamated.hpp"

#include "ConfigFile.h"

FO_BEGIN_NAMESPACE

static auto BuildConfigBenchmarkInput(int32_t section_count, int32_t keys_per_section) -> string
{
    string input;

    for (int32_t section_index = 0; section_index < section_count; section_index++) {
        input += strex("[ProtoItem]\n$Name = BenchItem_{}\n", section_index);

        for (int32_t key_index = 0; key_index < keys_per_section; key_index++) {
            input += strex("Key{} = Value{}_{}\n", key_index, section_index, key_index);
        }

        input += "Description += extra\n";
        input += "Description += tokens\n";
    }

    return input;
}

TEST_CASE("ConfigFile")
{
    SECTION("StoresViewsIntoOwnedInput")
    {
        string source = "[ProtoItem]\n$Name = ItemOne\nName = Base\nName += Extra\n";
        ConfigFile config {source};

        CHECK(config.HasSection("ProtoItem"));
        CHECK(config.GetAsStr("ProtoItem", "$Name") == "ItemOne");
        CHECK(config.GetAsStr("ProtoItem", "Name") == "Base Extra");

        source.assign("broken");

        CHECK(config.GetAsStr("ProtoItem", "$Name") == "ItemOne");
        CHECK(config.GetAsStr("ProtoItem", "Name") == "Base Extra");
    }

    SECTION("PreservesViewsAfterMove")
    {
        ConfigFile original {"[ProtoItem]\n$Name = One\nName = Value\n"};
        ConfigFile moved {std::move(original)};

        CHECK(moved.HasSection("ProtoItem"));
        CHECK(moved.GetAsStr("ProtoItem", "$Name") == "One");
        CHECK(moved.GetAsStr("ProtoItem", "Name") == "Value");
    }

    SECTION("PreservesViewsAfterMoveAssignment")
    {
        ConfigFile source {"[ProtoItem]\n$Name = Assigned\nName = Payload\n"};
        ConfigFile target {"[Other]\nValue = Legacy\n"};

        target = std::move(source);

        CHECK(target.HasSection("ProtoItem"));
        CHECK(target.GetAsStr("ProtoItem", "$Name") == "Assigned");
        CHECK(target.GetAsStr("ProtoItem", "Name") == "Payload");
    }

    SECTION("CollectsSectionContent")
    {
        const string source = "[ShaderCommon]\nline_1 \\\nline_2\nvalue # keep content before comment stripping\n\n[VertexShader]\nvoid main() {}\n";
        const ConfigFile config {source, ConfigFileOption::CollectContent};

        CHECK(config.GetSectionContent("ShaderCommon") == "line_1 line_2\nvalue # keep content before comment stripping\n");
        CHECK(config.GetSectionContent("VertexShader") == "void main() {}\n");
    }

    SECTION("CollectsSectionContentForTabContinuedLines")
    {
        const string source = "[ShaderCommon]\nline_1\t\\\nline_2\n";
        const ConfigFile config {source, ConfigFileOption::CollectContent};

        CHECK(config.GetSectionContent("ShaderCommon") == "line_1 line_2\n");
    }

    SECTION("ParsesCrLfLinesAndContinuation")
    {
        const string source = "[ShaderCommon]\r\nline_1 \\\r\nline_2\r\n[Section]\r\nKey = Value\r\n";
        const ConfigFile config {source, ConfigFileOption::CollectContent};

        CHECK(config.GetSectionContent("ShaderCommon") == "line_1 line_2\n");
        CHECK(config.GetAsStr("Section", "Key") == "Value");
    }

    SECTION("ParsesBoolIntsAndDefaults")
    {
        const string source = "[Section]\nEnabled = true\nDisabled = FALSE\nCount = 42\nName = Value\n";
        const ConfigFile config {source};

        CHECK(config.GetAsInt("Section", "Enabled") == 1);
        CHECK(config.GetAsInt("Section", "Disabled") == 0);
        CHECK(config.GetAsInt("Section", "Enabled", 99) == 1);
        CHECK(config.GetAsInt("Section", "Disabled", 99) == 0);
        CHECK(config.GetAsInt("Section", "Count") == 42);
        CHECK(config.GetAsInt("Section", "Missing") == 0);
        CHECK(config.GetAsInt("Section", "Missing", 11) == 11);
        CHECK(config.GetAsStr("Section", "Name") == "Value");
        CHECK(config.GetAsStr("Section", "Missing") == string_view {});
        CHECK(config.GetAsStr("Section", "Missing", "Fallback") == "Fallback");
    }

    SECTION("TreatsFormFeedAndVerticalTabAsConfigWhitespace")
    {
        const string source = "[Section]\n\fCount\v=\f42\v\n\vEnabled\f=\vtrue\f\nText\f=\vValue\f\n";
        const ConfigFile config {source};

        CHECK(config.GetAsInt("Section", "Count") == 42);
        CHECK(config.GetAsInt("Section", "Enabled") == 1);
        CHECK(config.GetAsStr("Section", "Text") == "Value");
    }

    SECTION("PreservesEscapedCommentCharacters")
    {
        const string source = "[Section]\nText = keep\\#hash # strip this\nOther = value\n";
        const ConfigFile config {source};

        CHECK(config.GetAsStr("Section", "Text") == "keep\\#hash");
        CHECK(config.GetAsStr("Section", "Other") == "value");
    }

    SECTION("PreservesCommentCharactersInsideDoubleQuotes")
    {
        const string source = "[Section]\nText = \"quoted # hash\" # strip this\nOther = value\n";
        const ConfigFile config {source};

        CHECK(config.GetAsStr("Section", "Text") == "\"quoted # hash\"");
        CHECK(config.GetAsStr("Section", "Other") == "value");
    }

    SECTION("PreservesCommentCharactersInsideDoubleQuotesAcrossContinuedLines")
    {
        const string source = "[Section]\nText = \"quoted # hash\" \\\ncontinued # tail\nOther = value\n";
        const ConfigFile config {source};

        CHECK(config.GetAsStr("Section", "Text") == "\"quoted # hash\" continued");
        CHECK(config.GetAsStr("Section", "Other") == "value");
    }

    SECTION("PreservesCommentCharactersInsideQuotedAppendedValues")
    {
        const string source = "[Section]\nText = base\nText += \"quoted # hash\" # strip this\n";
        const ConfigFile config {source};

        CHECK(config.GetAsStr("Section", "Text") == "base \"quoted # hash\"");
    }

    SECTION("PreservesCommentCharactersAfterEscapedQuotesInsideDoubleQuotes")
    {
        const string source = "[Section]\nText = \"quoted \\\" # hash\" # strip this\nOther = value\n";
        const ConfigFile config {source};

        CHECK(config.GetAsStr("Section", "Text") == "\"quoted \\\" # hash\"");
        CHECK(config.GetAsStr("Section", "Other") == "value");
    }

    SECTION("SkipsBraceFormatLines")
    {
        const string source = "[Section]\n{100}{20}{Payload}\nKey = Value\n";
        const ConfigFile config {source};

        CHECK_FALSE(config.HasKey("Section", "120"));
        CHECK(config.GetAsStr("Section", "Key") == "Value");
    }

    SECTION("ReturnsRepeatedSections")
    {
        const string source = "[ProtoItem]\n$Name = One\n[ProtoItem]\n$Name = Two\n";
        ConfigFile config {source};
        vector<ptr<map<string_view, string_view>>> sections = config.GetSections("ProtoItem");

        REQUIRE(sections.size() == 2);
        CHECK(sections[0]->at("$Name") == "One");
        CHECK(sections[1]->at("$Name") == "Two");
    }

    SECTION("ReturnsSectionViews")
    {
        string source = "[ProtoItem]\n$Name = One\nName = Base\nName += Two\n";
        ConfigFile config {source};
        vector<ptr<map<string_view, string_view>>> sections = config.GetSections("ProtoItem");

        source.assign("broken");

        REQUIRE(sections.size() == 1);
        CHECK(sections[0]->at("$Name") == "One");
        CHECK(sections[0]->at("Name") == "Base Two");
    }

    SECTION("GetSectionReturnsFirstRepeatedSection")
    {
        ConfigFile config {"[ProtoItem]\n$Name = One\n[ProtoItem]\n$Name = Two\n"};

        CHECK(config.GetSection("ProtoItem").at("$Name") == "One");
    }

    SECTION("AppendsIntoMissingKeyWithoutLeadingSpace")
    {
        ConfigFile config {"[ProtoItem]\nName += Two\n"};

        CHECK(config.GetAsStr("ProtoItem", "Name") == "Two");
    }

    SECTION("IgnoresEmptyAppendedValueForExistingKey")
    {
        ConfigFile config {"[ProtoItem]\nName = Base\nName +=    # ignored\n"};

        CHECK(config.GetAsStr("ProtoItem", "Name") == "Base");
    }

    SECTION("StoresNestedSectionNamesVerbatim")
    {
        const string source = "[ProtoMap]\n"
                              "$Name = MapOne\n"
                              "[$Name/Item]\n"
                              "Kind = FromOne\n"
                              "[MapOne/Item]\n"
                              "Kind = ExplicitOne\n"
                              "[/Item]\n"
                              "Kind = BareSlash\n";
        ConfigFile config {source};

        // The parser resolves no prefix: a nested name is stored exactly as authored, so what
        // "$Name" or any other prefix means is left entirely to the consuming format
        CHECK(config.GetSections("$Name/Item").size() == 1);
        CHECK(config.GetAsStr("$Name/Item", "Kind") == "FromOne");
        CHECK(config.GetAsStr("MapOne/Item", "Kind") == "ExplicitOne");
        CHECK(config.GetAsStr("/Item", "Kind") == "BareSlash");
    }

    SECTION("ExposesSectionsInFileOrder")
    {
        const string source = "[ProtoMap]\n"
                              "$Name = MapOne\n"
                              "[$Name/Item]\n"
                              "Kind = FromOne\n"
                              "[ProtoMap]\n"
                              "$Name = MapTwo\n"
                              "[$Name/Item]\n"
                              "Kind = FromTwo\n"
                              "[$Name/Critter]\n"
                              "Kind = TwosCritter\n";
        ConfigFile config {source};

        // Repeated names collapse in the by-name multimap, so the ordered view is what lets a
        // consumer bind nested sections to the section they follow
        const auto& ordered = config.GetOrderedSections();
        REQUIRE(ordered.size() == 6); // default section + 2 anchors + 3 nested

        CHECK(ordered[0].first.empty());
        CHECK(ordered[1].first == "ProtoMap");
        CHECK(ordered[1].second->at("$Name") == "MapOne");
        CHECK(ordered[2].first == "$Name/Item");
        CHECK(ordered[2].second->at("Kind") == "FromOne");
        CHECK(ordered[3].first == "ProtoMap");
        CHECK(ordered[3].second->at("$Name") == "MapTwo");
        CHECK(ordered[4].first == "$Name/Item");
        CHECK(ordered[4].second->at("Kind") == "FromTwo");
        CHECK(ordered[5].first == "$Name/Critter");
        CHECK(ordered[5].second->at("Kind") == "TwosCritter");
    }

    SECTION("SkipsNestedSectionsWhenRequested")
    {
        const string source = "[ProtoMap]\n"
                              "$Name = MapOne\n"
                              "[$Name/Item]\n"
                              "Kind = Skipped\n"
                              "[MapOne/Critter]\n"
                              "Kind = SkippedAsWell\n"
                              "[ProtoMap]\n"
                              "$Name = MapTwo\n";
        ConfigFile config {source, ConfigFileOption::SkipNestedSections};

        CHECK(config.GetSections("ProtoMap").size() == 2);
        CHECK_FALSE(config.HasSection("$Name/Item"));
        CHECK_FALSE(config.HasSection("MapOne/Critter"));
        CHECK(config.GetSections()->size() == 3);
        CHECK(config.GetOrderedSections().size() == 3); // default section + both anchors

        vector<ptr<map<string_view, string_view>>> anchors = config.GetSections("ProtoMap");
        CHECK(anchors[0]->at("$Name") == "MapOne");
        CHECK(anchors[1]->at("$Name") == "MapTwo");
    }

    SECTION("ReturnsNullForMissingSectionKeyValues")
    {
        ConfigFile config {"[ProtoItem]\n$Name = One\n"};

        const auto existing_section = config.GetSectionKeyValues("ProtoItem");
        const auto missing_section = config.GetSectionKeyValues("Missing");
        vector<ptr<map<string_view, string_view>>> missing_sections = config.GetSections("Missing");
        auto all_sections = config.GetSections();

        REQUIRE(static_cast<bool>(existing_section));
        CHECK(existing_section->at("$Name") == "One");
        CHECK_FALSE(static_cast<bool>(missing_section));
        CHECK(missing_sections.empty());
        CHECK(all_sections->size() == 2);
        CHECK(all_sections->begin()->first.empty());
        CHECK(config.HasKey("ProtoItem", "$Name"));
        CHECK_FALSE(config.HasKey("ProtoItem", "Missing"));
        CHECK_FALSE(config.HasKey("Missing", "$Name"));
    }

    SECTION("CollectsContentForRepeatedSections")
    {
        ConfigFile config {"[VertexShader]\nvoid main1() {}\n[VertexShader]\nvoid main2() {}\n", ConfigFileOption::CollectContent};

        vector<ptr<map<string_view, string_view>>> sections = config.GetSections("VertexShader");

        REQUIRE(sections.size() == 2);
        CHECK(sections[0]->at(string_view {}) == "void main1() {}\n");
        CHECK(sections[1]->at(string_view {}) == "void main2() {}\n");
    }

    SECTION("ReturnsEmptyCollectedContentForMissingOrEmptySections")
    {
        ConfigFile config {"[Empty]\n[Filled]\nvalue\n", ConfigFileOption::CollectContent};

        CHECK(config.GetSectionContent("Empty").empty());
        CHECK(config.GetSectionContent("Missing").empty());
        CHECK(config.GetSectionContent("Filled") == "value\n");
    }

    SECTION("IgnoresMalformedSectionsAndEntries")
    {
        const string source = "[]\nNoSeparator\n[ValidSection\n[Good]\nKey = Value\n";
        const ConfigFile config {source};

        CHECK_FALSE(config.HasSection("ValidSection"));
        CHECK(config.HasSection("Good"));
        CHECK(config.GetAsStr("Good", "Key") == "Value");
    }

    SECTION("IgnoresEntriesWithEmptyTrimmedKeys")
    {
        const string source = "[Good]\n   = Ignored\n\t+= IgnoredToo\nKey = Value\n";
        ConfigFile config {source};
        const auto section = config.GetSectionKeyValues("Good");

        REQUIRE(static_cast<bool>(section));
        CHECK(section->size() == 1);
        CHECK_FALSE(config.HasKey("Good", string_view {}));
        CHECK(config.GetAsStr("Good", "Key") == "Value");
    }

    const string benchmark_input = BuildConfigBenchmarkInput(128, 12);

    BENCHMARK("ParseLargeConfig")
    {
        ConfigFile config {benchmark_input};
        return numeric_cast<int32_t>(config.GetSections()->size());
    };
}

FO_END_NAMESPACE
