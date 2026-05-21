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

    SECTION("CompilerDiagnosticsUseSourceFileNameOnly")
    {
        TestRig local_rig;
        local_rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());

        BakerServerEngine compiler_engine {local_rig.BakedFiles};
        const vector<pair<string, string>> script_files = {{"Scripts/Nested/BrokenScript.fos", "namespace BrokenScript\n{\nvoid Broken()\n{\n    int value = ;\n}\n}\n"}};
        vector<string> messages;

        CHECK_THROWS_AS(CompileInlineScripts(&compiler_engine, "DiagnosticScripts", script_files, [&messages](string_view message) {
            messages.emplace_back(message);
        }), ScriptCompilerException);

        REQUIRE(!messages.empty());

        const auto first_error = std::ranges::find_if(messages, [](const string& message) { return message.find("error") != string::npos; });

        REQUIRE(first_error != messages.end());
        CHECK(first_error->starts_with("BrokenScript.fos("));
        CHECK(first_error->find("Scripts/Nested/") == string::npos);
    }
#endif
}

FO_END_NAMESPACE
