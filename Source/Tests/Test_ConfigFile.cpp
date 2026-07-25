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

template<size_t NameSize>
static auto MakeConfig(const char8_t (&name)[NameSize], u8string content, ConfigFileOption options = ConfigFileOption::None) -> ConfigFile
{
    FO_STACK_TRACE_ENTRY();

    const u8string_view strict_name = u8string_view::FromChecked(std::u8string_view {name, NameSize - 1});
    return ConfigFile {strict_name, std::move(content), options};
}

template<size_t NameSize, size_t ContentSize>
static auto MakeConfig(const char8_t (&name)[NameSize], const char8_t (&content)[ContentSize], ConfigFileOption options = ConfigFileOption::None) -> ConfigFile
{
    FO_STACK_TRACE_ENTRY();

    return MakeConfig(name, u8string {content}, options);
}

static auto NativeText(u8string_view value) noexcept -> std::u8string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    return value.native_view();
}

static auto BuildConfigBenchmarkInput(int32_t section_count, int32_t keys_per_section) -> u8string
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
    SECTION("StoresViewsAndOwnedHookResults")
    {
        u8string source {u8"[ProtoItem]\n$Name = ItemOne\nName = Base\nName += Extra\n"};
        ConfigFile config = MakeConfig(u8"Test.fomap", source);

        CHECK(config.HasSection("ProtoItem"));
        CHECK(NativeText(config.GetAsStr("ProtoItem", "$Name")) == u8"ItemOne");
        CHECK(NativeText(config.GetAsStr("ProtoItem", "Name")) == u8"Base Extra");

        source.assign(u8"broken");

        CHECK(NativeText(config.GetAsStr("ProtoItem", "$Name")) == u8"ItemOne");
        CHECK(NativeText(config.GetAsStr("ProtoItem", "Name")) == u8"Base Extra");
    }

    SECTION("PreservesViewsAfterMove")
    {
        ConfigFile original = MakeConfig(u8"Test.fomap", u8"[ProtoItem]\n$Name = One\nName = Value\n");
        ConfigFile moved {std::move(original)};

        CHECK(moved.HasSection("ProtoItem"));
        CHECK(NativeText(moved.GetAsStr("ProtoItem", "$Name")) == u8"One");
        CHECK(NativeText(moved.GetAsStr("ProtoItem", "Name")) == u8"Value");
    }

    SECTION("PreservesViewsAfterMoveAssignment")
    {
        ConfigFile source = MakeConfig(u8"Test.fomap", u8"[ProtoItem]\n$Name = Assigned\nName = Payload\n");
        ConfigFile target = MakeConfig(u8"Other.fomap", u8"[Other]\nValue = Legacy\n");

        target = std::move(source);

        CHECK(target.HasSection("ProtoItem"));
        CHECK(NativeText(target.GetAsStr("ProtoItem", "$Name")) == u8"Assigned");
        CHECK(NativeText(target.GetAsStr("ProtoItem", "Name")) == u8"Payload");
    }

    SECTION("CollectsSectionContent")
    {
        const u8string source {u8"[ShaderCommon]\nline_1 \\\nline_2\nvalue # keep content before comment stripping\n\n[VertexShader]\nvoid main() {}\n"};
        const ConfigFile config = MakeConfig(u8"Effect.fofx", source, ConfigFileOption::CollectContent);

        CHECK(NativeText(config.GetSectionContent("ShaderCommon")) == u8"line_1 line_2\nvalue # keep content before comment stripping\n");
        CHECK(NativeText(config.GetSectionContent("VertexShader")) == u8"void main() {}\n");
    }

    SECTION("CollectsSectionContentForTabContinuedLines")
    {
        const u8string source {u8"[ShaderCommon]\nline_1\t\\\nline_2\n"};
        const ConfigFile config = MakeConfig(u8"Effect.fofx", source, ConfigFileOption::CollectContent);

        CHECK(NativeText(config.GetSectionContent("ShaderCommon")) == u8"line_1 line_2\n");
    }

    SECTION("TreatsWholeLineSlashCommentsAsContentRatherThanEntries")
    {
        const u8string source {u8"[ShaderCommon]\n// Power curve — гамма = 1.0\nEndpoint = https://example.invalid/effect\n"};
        const ConfigFile config = MakeConfig(u8"Effect.fofx", source, ConfigFileOption::CollectContent);

        CHECK(NativeText(config.GetSectionContent("ShaderCommon")) == u8"// Power curve — гамма = 1.0\nEndpoint = https://example.invalid/effect\n");
        CHECK(NativeText(config.GetAsStr("ShaderCommon", "Endpoint")) == u8"https://example.invalid/effect");
        CHECK_FALSE(config.HasKey("ShaderCommon", "// Power curve"));
    }

    SECTION("ParsesCrLfLinesAndContinuation")
    {
        const u8string source {u8"[ShaderCommon]\r\nline_1 \\\r\nline_2\r\n[Section]\r\nKey = Value\r\n"};
        const ConfigFile config = MakeConfig(u8"Effect.fofx", source, ConfigFileOption::CollectContent);

        CHECK(NativeText(config.GetSectionContent("ShaderCommon")) == u8"line_1 line_2\n");
        CHECK(NativeText(config.GetAsStr("Section", "Key")) == u8"Value");
    }

    SECTION("ParsesBoolIntsAndDefaults")
    {
        const u8string source {u8"[Section]\nEnabled = true\nDisabled = FALSE\nCount = 42\nName = Value\n"};
        const ConfigFile config = MakeConfig(u8"Test.cfg", source);

        CHECK(NativeText(config.GetNameHint()) == u8"Test.cfg");
        CHECK(config.GetAsInt("Section", "Enabled") == 1);
        CHECK(config.GetAsInt("Section", "Disabled") == 0);
        CHECK(config.GetAsInt("Section", "Enabled", 99) == 1);
        CHECK(config.GetAsInt("Section", "Disabled", 99) == 0);
        CHECK(config.GetAsInt("Section", "Count") == 42);
        CHECK(config.GetAsInt("Section", "Missing") == 0);
        CHECK(config.GetAsInt("Section", "Missing", 11) == 11);
        CHECK(NativeText(config.GetAsStr("Section", "Name")) == u8"Value");
        CHECK(config.GetAsStr("Section", "Missing") == u8string_view {});
        CHECK(NativeText(config.GetAsStr("Section", "Missing", u8"Fallback")) == u8"Fallback");
    }

    SECTION("PreservesUnicodeFileNamesAndValues")
    {
        const ConfigFile config = MakeConfig(u8"Конфигурация.fomain", u8"[Text]\nValue = Привет, 世界 🌍\nCombining = é\n");

        CHECK(NativeText(config.GetNameHint()) == u8"Конфигурация.fomain");
        CHECK(NativeText(config.GetAsStr("Text", "Value")) == u8"Привет, 世界 🌍");
        CHECK(NativeText(config.GetAsStr("Text", "Combining")) == u8"é");
    }

    SECTION("RejectsNonAsciiSectionNamesAndKeys")
    {
        CHECK_THROWS_AS(MakeConfig(u8"Test.fomain", u8"[Раздел]\nValue = text\n"), TextValidationException);
        CHECK_THROWS_AS(MakeConfig(u8"Test.fomain", u8"[Section]\nКлюч = text\n"), TextValidationException);
    }

    SECTION("TreatsFormFeedAndVerticalTabAsConfigWhitespace")
    {
        const u8string source {u8"[Section]\n\fCount\v=\f42\v\n\vEnabled\f=\vtrue\f\nText\f=\vValue\f\n"};
        const ConfigFile config = MakeConfig(u8"Test.cfg", source);

        CHECK(config.GetAsInt("Section", "Count") == 42);
        CHECK(config.GetAsInt("Section", "Enabled") == 1);
        CHECK(NativeText(config.GetAsStr("Section", "Text")) == u8"Value");
    }

    SECTION("PreservesEscapedCommentCharacters")
    {
        const u8string source {u8"[Section]\nText = keep\\#hash # strip this\nOther = value\n"};
        const ConfigFile config = MakeConfig(u8"Test.cfg", source);

        CHECK(NativeText(config.GetAsStr("Section", "Text")) == u8"keep\\#hash");
        CHECK(NativeText(config.GetAsStr("Section", "Other")) == u8"value");
    }

    SECTION("PreservesCommentCharactersInsideDoubleQuotes")
    {
        const u8string source {u8"[Section]\nText = \"quoted # hash\" # strip this\nOther = value\n"};
        const ConfigFile config = MakeConfig(u8"Test.cfg", source);

        CHECK(NativeText(config.GetAsStr("Section", "Text")) == u8"\"quoted # hash\"");
        CHECK(NativeText(config.GetAsStr("Section", "Other")) == u8"value");
    }

    SECTION("PreservesCommentCharactersInsideDoubleQuotesAcrossContinuedLines")
    {
        const u8string source {u8"[Section]\nText = \"quoted # hash\" \\\ncontinued # tail\nOther = value\n"};
        const ConfigFile config = MakeConfig(u8"Test.cfg", source);

        CHECK(NativeText(config.GetAsStr("Section", "Text")) == u8"\"quoted # hash\" continued");
        CHECK(NativeText(config.GetAsStr("Section", "Other")) == u8"value");
    }

    SECTION("PreservesCommentCharactersInsideQuotedAppendedValues")
    {
        const u8string source {u8"[Section]\nText = base\nText += \"quoted # hash\" # strip this\n"};
        const ConfigFile config = MakeConfig(u8"Test.cfg", source);

        CHECK(NativeText(config.GetAsStr("Section", "Text")) == u8"base \"quoted # hash\"");
    }

    SECTION("PreservesCommentCharactersAfterEscapedQuotesInsideDoubleQuotes")
    {
        const u8string source {u8"[Section]\nText = \"quoted \\\" # hash\" # strip this\nOther = value\n"};
        const ConfigFile config = MakeConfig(u8"Test.cfg", source);

        CHECK(NativeText(config.GetAsStr("Section", "Text")) == u8"\"quoted \\\" # hash\"");
        CHECK(NativeText(config.GetAsStr("Section", "Other")) == u8"value");
    }

    SECTION("SkipsBraceFormatLines")
    {
        const u8string source {u8"[Section]\n{Speech 2 Answer 10}{}{Уровень >= 3.}\nKey = Value\n"};
        const ConfigFile config = MakeConfig(u8"Test.cfg", source);

        REQUIRE(config.GetSection("Section").size() == 1);
        CHECK(NativeText(config.GetAsStr("Section", "Key")) == u8"Value");
    }

    SECTION("ReturnsRepeatedSections")
    {
        const u8string source {u8"[ProtoItem]\n$Name = One\n[ProtoItem]\n$Name = Two\n"};
        ConfigFile config = MakeConfig(u8"Items.fopro", source);
        vector<ptr<ConfigKeyValueMap>> sections = config.GetSections("ProtoItem");

        REQUIRE(sections.size() == 2);
        CHECK(NativeText(sections[0]->at("$Name")) == u8"One");
        CHECK(NativeText(sections[1]->at("$Name")) == u8"Two");
    }

    SECTION("ReturnsSectionViews")
    {
        u8string source {u8"[ProtoItem]\n$Name = One\nName = Base\nName += Two\n"};
        ConfigFile config = MakeConfig(u8"Items.fopro", source);
        vector<ptr<ConfigKeyValueMap>> sections = config.GetSections("ProtoItem");

        source.assign(u8"broken");

        REQUIRE(sections.size() == 1);
        CHECK(NativeText(sections[0]->at("$Name")) == u8"One");
        CHECK(NativeText(sections[0]->at("Name")) == u8"Base Two");
    }

    SECTION("GetSectionReturnsFirstRepeatedSection")
    {
        ConfigFile config = MakeConfig(u8"Items.fopro", u8"[ProtoItem]\n$Name = One\n[ProtoItem]\n$Name = Two\n");

        CHECK(NativeText(config.GetSection("ProtoItem").at("$Name")) == u8"One");
    }

    SECTION("AppendsIntoMissingKeyWithoutLeadingSpace")
    {
        ConfigFile config = MakeConfig(u8"Items.fopro", u8"[ProtoItem]\nName += Two\n");

        CHECK(NativeText(config.GetAsStr("ProtoItem", "Name")) == u8"Two");
    }

    SECTION("IgnoresEmptyAppendedValueForExistingKey")
    {
        ConfigFile config = MakeConfig(u8"Items.fopro", u8"[ProtoItem]\nName = Base\nName +=    # ignored\n");

        CHECK(NativeText(config.GetAsStr("ProtoItem", "Name")) == u8"Base");
    }

    SECTION("StopsAfterFirstSectionWhenRequested")
    {
        ConfigFile config = MakeConfig(u8"Items.fopro", u8"[ProtoItem]\n$Name = One\n[ProtoItem]\n$Name = Two\n", ConfigFileOption::ReadFirstSection);

        CHECK(config.HasSection("ProtoItem"));
        CHECK(NativeText(config.GetAsStr("ProtoItem", "$Name")) == u8"One");
        CHECK(config.GetSections("ProtoItem").size() == 1);
        CHECK(config.GetSections()->size() == 2);
    }

    SECTION("CollectsContentWhenStoppingAfterFirstSection")
    {
        const auto options = static_cast<ConfigFileOption>(static_cast<uint8_t>(ConfigFileOption::CollectContent) | static_cast<uint8_t>(ConfigFileOption::ReadFirstSection));

        ConfigFile config = MakeConfig(u8"Effect.fofx", u8"[ShaderCommon]\nline_1 \\\nline_2\n[VertexShader]\nvoid main() {}\n", options);

        CHECK(config.HasSection("ShaderCommon"));
        CHECK_FALSE(config.HasSection("VertexShader"));
        CHECK(config.GetSections("ShaderCommon").size() == 1);
        CHECK(config.GetSections()->size() == 2);
        CHECK(NativeText(config.GetSectionContent("ShaderCommon")) == u8"line_1 line_2\n");
    }

    SECTION("ReturnsNullForMissingSectionKeyValues")
    {
        ConfigFile config = MakeConfig(u8"Items.fopro", u8"[ProtoItem]\n$Name = One\n");

        const auto existing_section = config.GetSectionKeyValues("ProtoItem");
        const auto missing_section = config.GetSectionKeyValues("Missing");
        vector<ptr<ConfigKeyValueMap>> missing_sections = config.GetSections("Missing");
        auto all_sections = config.GetSections();

        REQUIRE(static_cast<bool>(existing_section));
        CHECK(NativeText(existing_section->at("$Name")) == u8"One");
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
        ConfigFile config = MakeConfig(u8"Effect.fofx", u8"[VertexShader]\nvoid main1() {}\n[VertexShader]\nvoid main2() {}\n", ConfigFileOption::CollectContent);

        vector<ptr<ConfigKeyValueMap>> sections = config.GetSections("VertexShader");

        REQUIRE(sections.size() == 2);
        CHECK(NativeText(sections[0]->at(string_view {})) == u8"void main1() {}\n");
        CHECK(NativeText(sections[1]->at(string_view {})) == u8"void main2() {}\n");
    }

    SECTION("ReturnsEmptyCollectedContentForMissingOrEmptySections")
    {
        ConfigFile config = MakeConfig(u8"Effect.fofx", u8"[Empty]\n[Filled]\nvalue\n", ConfigFileOption::CollectContent);

        CHECK(config.GetSectionContent("Empty").empty());
        CHECK(config.GetSectionContent("Missing").empty());
        CHECK(NativeText(config.GetSectionContent("Filled")) == u8"value\n");
    }

    SECTION("IgnoresMalformedSectionsAndEntries")
    {
        const u8string source {u8"[]\nNoSeparator\n[ValidSection\n[Good]\nKey = Value\n"};
        const ConfigFile config = MakeConfig(u8"Test.cfg", source);

        CHECK_FALSE(config.HasSection("ValidSection"));
        CHECK(config.HasSection("Good"));
        CHECK(NativeText(config.GetAsStr("Good", "Key")) == u8"Value");
    }

    SECTION("IgnoresEntriesWithEmptyTrimmedKeys")
    {
        const u8string source {u8"[Good]\n   = Ignored\n\t+= IgnoredToo\nKey = Value\n"};
        ConfigFile config = MakeConfig(u8"Test.cfg", source);
        const auto section = config.GetSectionKeyValues("Good");

        REQUIRE(static_cast<bool>(section));
        CHECK(section->size() == 1);
        CHECK_FALSE(config.HasKey("Good", string_view {}));
        CHECK(NativeText(config.GetAsStr("Good", "Key")) == u8"Value");
    }

    const u8string benchmark_input = BuildConfigBenchmarkInput(128, 12);

    BENCHMARK("ParseLargeConfig")
    {
        ConfigFile config = MakeConfig(u8"Bench.fopro", benchmark_input);
        return numeric_cast<int32_t>(config.GetSections()->size());
    };
}

FO_END_NAMESPACE
