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

static auto BuildConfigBenchmarkInput(int32 section_count, int32 keys_per_section) -> string
{
    string input;

    for (int32 section_index = 0; section_index < section_count; section_index++) {
        input += strex("[ProtoItem]\n$Name = BenchItem_{}\n", section_index);

        for (int32 key_index = 0; key_index < keys_per_section; key_index++) {
            input += strex("Key{} = Value{}_{}\n", key_index, section_index, key_index);
        }

        input += "Description += extra\n";
        input += "Description += tokens\n";
    }

    return input;
}

TEST_CASE("ConfigFile")
{
    SECTION("StoresViewsAndOwnedHookResults")
    {
        string source = "[ProtoItem]\n$Name = ItemOne\nName = Base\nName += Extra\n";
        ConfigFile config {"Test.fomap", source, nullptr};

        CHECK(config.HasSection("ProtoItem"));
        CHECK(config.GetAsStr("ProtoItem", "$Name") == "ItemOne");
        CHECK(config.GetAsStr("ProtoItem", "Name") == "Base Extra");

        source.assign("broken");

        CHECK(config.GetAsStr("ProtoItem", "$Name") == "ItemOne");
        CHECK(config.GetAsStr("ProtoItem", "Name") == "Base Extra");
    }

    SECTION("PreservesViewsAfterMove")
    {
        ConfigFile original {"Test.fomap", "[ProtoItem]\n$Name = One\nName = Value\n", nullptr};
        ConfigFile moved {std::move(original)};

        CHECK(moved.HasSection("ProtoItem"));
        CHECK(moved.GetAsStr("ProtoItem", "$Name") == "One");
        CHECK(moved.GetAsStr("ProtoItem", "Name") == "Value");
    }

    SECTION("CollectsSectionContent")
    {
        const string source = "[ShaderCommon]\nline_1 \\\nline_2\nvalue # keep content before comment stripping\n\n[VertexShader]\nvoid main() {}\n";
        const ConfigFile config {"Effect.fofx", source, nullptr, ConfigFileOption::CollectContent};

        CHECK(config.GetSectionContent("ShaderCommon") == "line_1 line_2\nvalue # keep content before comment stripping\n");
        CHECK(config.GetSectionContent("VertexShader") == "void main() {}\n");
    }

    SECTION("ParsesBoolIntsAndDefaults")
    {
        const string source = "[Section]\nEnabled = true\nDisabled = FALSE\nCount = 42\nName = Value\n";
        const ConfigFile config {"Test.cfg", source, nullptr};

        CHECK(config.GetAsInt("Section", "Enabled") == 1);
        CHECK(config.GetAsInt("Section", "Disabled") == 0);
        CHECK(config.GetAsInt("Section", "Count") == 42);
        CHECK(config.GetAsInt("Section", "Missing", 11) == 11);
        CHECK(config.GetAsStr("Section", "Name") == "Value");
        CHECK(config.GetAsStr("Section", "Missing") == string_view {});
        CHECK(config.GetAsStr("Section", "Missing", "Fallback") == "Fallback");
    }

    SECTION("PreservesEscapedCommentCharacters")
    {
        const string source = "[Section]\nText = keep\\#hash # strip this\nOther = value\n";
        const ConfigFile config {"Test.cfg", source, nullptr};

        CHECK(config.GetAsStr("Section", "Text") == "keep\\#hash");
        CHECK(config.GetAsStr("Section", "Other") == "value");
    }

    SECTION("PreservesCommentCharactersInsideDoubleQuotes")
    {
        const string source = "[Section]\nText = \"quoted # hash\" # strip this\nOther = value\n";
        const ConfigFile config {"Test.cfg", source, nullptr};

        CHECK(config.GetAsStr("Section", "Text") == "\"quoted # hash\"");
        CHECK(config.GetAsStr("Section", "Other") == "value");
    }

    SECTION("PreservesCommentCharactersInsideDoubleQuotesAcrossContinuedLines")
    {
        const string source = "[Section]\nText = \"quoted # hash\" \\\ncontinued # tail\nOther = value\n";
        const ConfigFile config {"Test.cfg", source, nullptr};

        CHECK(config.GetAsStr("Section", "Text") == "\"quoted # hash\" continued");
        CHECK(config.GetAsStr("Section", "Other") == "value");
    }

    SECTION("PreservesCommentCharactersInsideQuotedAppendedValues")
    {
        const string source = "[Section]\nText = base\nText += \"quoted # hash\" # strip this\n";
        const ConfigFile config {"Test.cfg", source, nullptr};

        CHECK(config.GetAsStr("Section", "Text") == "base \"quoted # hash\"");
    }

    SECTION("SupportsBraceFormatEntries")
    {
        const string source = "[Section]\n{100}{20}{Payload}\n";
        const ConfigFile config {"Test.cfg", source, nullptr};

        CHECK(config.GetAsStr("Section", "120") == "Payload");
    }

    SECTION("IgnoresIncompleteBraceFormatEntries")
    {
        const string source = "[Section]\n{100}{20\n{100}{20}\n";
        const ConfigFile config {"Test.cfg", source, nullptr};

        CHECK_FALSE(config.HasKey("Section", "120"));
        CHECK(config.GetAsStr("Section", "120") == string_view {});
    }

    SECTION("ReturnsRepeatedSections")
    {
        const string source = "[ProtoItem]\n$Name = One\n[ProtoItem]\n$Name = Two\n";
        ConfigFile config {"Items.fopro", source, nullptr};
        vector<map<string_view, string_view>*> sections = config.GetSections("ProtoItem");

        REQUIRE(sections.size() == 2);
        CHECK(sections[0]->at("$Name") == "One");
        CHECK(sections[1]->at("$Name") == "Two");
    }

    SECTION("ReturnsSectionViews")
    {
        string source = "[ProtoItem]\n$Name = One\nName = Base\nName += Two\n";
        ConfigFile config {"Items.fopro", source, nullptr};
        vector<map<string_view, string_view>*> sections = config.GetSections("ProtoItem");

        source.assign("broken");

        REQUIRE(sections.size() == 1);
        CHECK(sections[0]->at("$Name") == "One");
        CHECK(sections[0]->at("Name") == "Base Two");
    }

    SECTION("AppendsIntoMissingKeyWithoutLeadingSpace")
    {
        ConfigFile config {"Items.fopro", "[ProtoItem]\nName += Two\n", nullptr};

        CHECK(config.GetAsStr("ProtoItem", "Name") == "Two");
    }

    SECTION("StopsAfterFirstSectionWhenRequested")
    {
        ConfigFile config {"Items.fopro", "[ProtoItem]\n$Name = One\n[ProtoItem]\n$Name = Two\n", nullptr, ConfigFileOption::ReadFirstSection};

        CHECK(config.HasSection("ProtoItem"));
        CHECK(config.GetAsStr("ProtoItem", "$Name") == "One");
        CHECK(config.GetSections("ProtoItem").size() == 1);
        CHECK(config.GetSections().size() == 2);
    }

    SECTION("CollectsContentWhenStoppingAfterFirstSection")
    {
        const auto options = static_cast<ConfigFileOption>(static_cast<uint8>(ConfigFileOption::CollectContent) | static_cast<uint8>(ConfigFileOption::ReadFirstSection));

        ConfigFile config {"Effect.fofx", "[ShaderCommon]\nline_1 \\\nline_2\n[VertexShader]\nvoid main() {}\n", nullptr, options};

        CHECK(config.HasSection("ShaderCommon"));
        CHECK_FALSE(config.HasSection("VertexShader"));
        CHECK(config.GetSections("ShaderCommon").size() == 1);
        CHECK(config.GetSections().size() == 2);
        CHECK(config.GetSectionContent("ShaderCommon") == "line_1 line_2\n");
    }

    SECTION("ReturnsNullForMissingSectionKeyValues")
    {
        ConfigFile config {"Items.fopro", "[ProtoItem]\n$Name = One\n", nullptr};

        const auto* existing_section = config.GetSectionKeyValues("ProtoItem");
        const auto* missing_section = config.GetSectionKeyValues("Missing");

        REQUIRE(existing_section != nullptr);
        CHECK(existing_section->at("$Name") == "One");
        CHECK(missing_section == nullptr);
        CHECK(config.HasKey("ProtoItem", "$Name"));
        CHECK_FALSE(config.HasKey("ProtoItem", "Missing"));
        CHECK_FALSE(config.HasKey("Missing", "$Name"));
    }

    SECTION("CollectsContentForRepeatedSections")
    {
        ConfigFile config {"Effect.fofx", "[VertexShader]\nvoid main1() {}\n[VertexShader]\nvoid main2() {}\n", nullptr, ConfigFileOption::CollectContent};

        vector<map<string_view, string_view>*> sections = config.GetSections("VertexShader");

        REQUIRE(sections.size() == 2);
        CHECK(sections[0]->at(string_view {}) == "void main1() {}\n");
        CHECK(sections[1]->at(string_view {}) == "void main2() {}\n");
    }

    SECTION("IgnoresMalformedSectionsAndEntries")
    {
        const string source = "[]\nNoSeparator\n[ValidSection\n[Good]\nKey = Value\n";
        const ConfigFile config {"Test.cfg", source, nullptr};

        CHECK_FALSE(config.HasSection("ValidSection"));
        CHECK(config.HasSection("Good"));
        CHECK(config.GetAsStr("Good", "Key") == "Value");
    }

    const string benchmark_input = BuildConfigBenchmarkInput(128, 12);

    BENCHMARK("ParseLargeConfig")
    {
        ConfigFile config {"Bench.fopro", benchmark_input, nullptr};
        return numeric_cast<int32>(config.GetSections().size());
    };
}

FO_END_NAMESPACE
