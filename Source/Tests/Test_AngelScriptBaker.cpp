//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#if FO_ANGELSCRIPT_SCRIPTING
#include "AngelScriptBaker.h"
#include "Test_BakerHelpers.h"
#endif

FO_BEGIN_NAMESPACE

#if FO_ANGELSCRIPT_SCRIPTING
static void AddAngelScriptMetadataForAllSides(BakerTests::TestRig& rig)
{
    const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
    rig.AddBakedFile("Metadata.fometa-server", metadata_blob);
    rig.AddBakedFile("Metadata.fometa-client", metadata_blob);
    rig.AddBakedFile("Metadata.fometa-mapper", metadata_blob);
}
#endif

TEST_CASE("AngelScriptBaker")
{
#if FO_ANGELSCRIPT_SCRIPTING
    using namespace BakerTests;

    TestRig rig;
    const auto bakers = MakeRequestedBakers({string(AngelScriptBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == AngelScriptBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 4);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));

    SECTION("IgnoresNonScriptSourceFiles")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Scripts/Readme.txt", "not a script");

        AngelScriptBaker baker(local_rig.MakeContext("ScriptsIgnored"));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipAllScriptTargets")
    {
        TestRig local_rig;
        AddAngelScriptMetadataForAllSides(local_rig);
        local_rig.AddSourceFile("Scripts/Skip.fos", "namespace BakerScriptSkip { void Touch() {} }\n", 77);

        vector<pair<string, uint64_t>> checks;
        AngelScriptBaker baker(local_rig.MakeContext("ScriptsSkip", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return false;
        }));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.empty());
        REQUIRE(checks.size() == 3);
        CHECK(checks.front().second == 77);
        CHECK(std::ranges::any_of(checks, [](const auto& check) { return check.first == "ScriptsSkip.fos-bin-server"; }));
        CHECK(std::ranges::any_of(checks, [](const auto& check) { return check.first == "ScriptsSkip.fos-bin-client"; }));
        CHECK(std::ranges::any_of(checks, [](const auto& check) { return check.first == "ScriptsSkip.fos-bin-mapper"; }));
    }

    SECTION("BakesSelectedScriptTargets")
    {
        TestRig local_rig;
        AddAngelScriptMetadataForAllSides(local_rig);
        local_rig.AddSourceFile("Scripts/Selected.fos", "namespace BakerScriptSelected { void Touch() {} }\n");

        AngelScriptBaker baker(local_rig.MakeContext("ScriptsSelected", [](string_view path, uint64_t) { return path.ends_with(".fos-bin-client") || path.ends_with(".fos-bin-mapper"); }));
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK_FALSE(local_rig.Outputs.contains("ScriptsSelected.fos-bin-server"));
        CHECK(local_rig.Outputs.contains("ScriptsSelected.fos-bin-client"));
        CHECK(local_rig.Outputs.contains("ScriptsSelected.fos-bin-mapper"));
        CHECK(local_rig.Outputs.size() == 2);
    }

    SECTION("RejectsBrokenScripts")
    {
        TestRig local_rig;
        AddAngelScriptMetadataForAllSides(local_rig);
        local_rig.AddSourceFile("Scripts/Broken.fos", "namespace BakerScriptBroken { void Broken() { int value = ; } }\n");

        AngelScriptBaker baker(local_rig.MakeContext("ScriptsBroken", [](string_view path, uint64_t) { return path.ends_with(".fos-bin-server"); }));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), AngelScriptBakerException);
    }

    SECTION("DeduplicatesCompileMessagesAcrossTargets")
    {
        TestRig local_rig;
        AddAngelScriptMetadataForAllSides(local_rig);
        local_rig.AddSourceFile("Scripts/DuplicateDiagnostic.fos", "namespace BakerScriptDuplicateDiagnostic { void Broken() { int value = ; } }\n");

        AngelScriptBaker baker(local_rig.MakeContext("ScriptsDuplicateDiagnostic", [](string_view path, uint64_t) { return path.ends_with(".fos-bin-client") || path.ends_with(".fos-bin-mapper"); }));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), AngelScriptBakerException);
    }

    SECTION("RethrowsUnexpectedErrorsInSyncMode")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Scripts/MissingMetadata.fos", "namespace BakerScriptMissingMetadata { void Touch() {} }\n");

        AngelScriptBaker baker(local_rig.MakeContext("ScriptsMissingMetadataSync", [](string_view path, uint64_t) { return path.ends_with(".fos-bin-server"); }));
        CHECK_THROWS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
    }

    SECTION("AggregatesUnexpectedErrorsOutsideSyncMode")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Scripts/MissingMetadata.fos", "namespace BakerScriptMissingMetadata { void Touch() {} }\n");

        auto context = local_rig.MakeContext("ScriptsMissingMetadataAsync", [](string_view path, uint64_t) { return path.ends_with(".fos-bin-server"); });
        context->ForceSyncMode = false;

        AngelScriptBaker baker(std::move(context));
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), AngelScriptBakerException);
    }

    SECTION("CompilerDiagnosticsUseSourceFileNameOnly")
    {
        TestRig local_rig;
        local_rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());

        BakerServerEngine compiler_engine {local_rig.BakedFiles};
        const vector<pair<string, string>> script_files = {{"Scripts/Nested/BrokenScript.fos", "namespace BrokenScript\n{\nvoid Broken()\n{\n    int value = ;\n}\n}\n"}};
        vector<string> messages;

        CHECK_THROWS_AS(CompileInlineScripts(&compiler_engine, "DiagnosticScripts", script_files, [&messages](string_view message) { messages.emplace_back(message); }), ScriptCompilerException);

        REQUIRE(!messages.empty());

        const auto first_error = std::ranges::find_if(messages, [](const string& message) { return message.find("error") != string::npos; });

        REQUIRE(first_error != messages.end());
        CHECK(first_error->starts_with("BrokenScript.fos("));
        CHECK(first_error->find("Scripts/Nested/") == string::npos);
    }
#endif
}

TEST_CASE("AngelScript mutable globals are disallowed")
{
#if FO_ANGELSCRIPT_SCRIPTING
    using namespace BakerTests;

    // Defaults: ScriptSettings::MutableGlobalsAllowedSourcePaths is empty, so no path is exempt.
    // The gate must fire for any non-const module-level global.
    TestRig rig;
    rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());

    {
        BakerServerEngine compiler_engine(rig.BakedFiles);
        REQUIRE_THROWS_WITH(CompileInlineScripts(&compiler_engine, rig.Settings, "MutableGlobalScripts", {{"Scripts/MutableGlobal.fos", "namespace MutableGlobal\n{\nint Value = 1;\n}\n"}}, [](string_view) { }), Catch::Matchers::ContainsSubstring("mutable global variable"));
    }

    {
        BakerServerEngine compiler_engine(rig.BakedFiles);
        REQUIRE_NOTHROW(CompileInlineScripts(&compiler_engine, rig.Settings, "ConstGlobalScripts", {{"Scripts/ConstGlobal.fos", "namespace ConstGlobal\n{\nconst int Value = 1;\nconst int[] Values = {1, 2, 3};\n}\n"}}, [](string_view) { }));
    }
#endif
}

FO_END_NAMESPACE
