//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#if FO_ANGELSCRIPT_SCRIPTING
#include "../../../Scripts/Extension/DialogBaker.h"
#include "Test_BakerHelpers.h"
#endif

FO_BEGIN_NAMESPACE

TEST_CASE("DialogBaker")
{
#if FO_ANGELSCRIPT_SCRIPTING
    using namespace BakerTests;

    const auto make_script_blob = [](string_view script_source) {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("DialogBakerCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine compiler_engine {compiler_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "DialogBakerScripts", {{"Scripts/TestDialogCallbacks.fos", string(script_source)}}, [](string_view message) {
            const auto message_str = string(message);

            if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                throw ScriptSystemException(message_str);
            }
        });
    };

    static constexpr string_view DialogFile = R"([Dialog]
Speech 1
  Answer Exit
    Demand Script TestDialogCallbacks::CanUse
    Result Script TestDialogCallbacks::Apply

[Text engl]
{Name}{}{Dialog tester}
{Speech 1}{}{Hello}
{Speech 1 Answer Exit}{}{Bye}
])";

    static constexpr string_view ValidScript = R"(namespace TestDialogCallbacks
{
[[DialogDemand]]
bool CanUse(Critter cr, Critter npc)
{
    return true;
}

[[DialogResult]]
void Apply(Critter cr, Critter npc)
{
}
})";

    static constexpr string_view MissingDemandAttributeScript = R"(namespace TestDialogCallbacks
{
bool CanUse(Critter cr, Critter npc)
{
    return true;
}

[[DialogResult]]
void Apply(Critter cr, Critter npc)
{
}
})";

    static constexpr string_view MissingResultAttributeScript = R"(namespace TestDialogCallbacks
{
[[DialogDemand]]
bool CanUse(Critter cr, Critter npc)
{
    return true;
}

void Apply(Critter cr, Critter npc)
{
}
})";

    SECTION("BakesDialogsWhenScriptCallbacksHaveProjectAttributes")
    {
        TestRig rig;
        rig.AddSourceFile("Dialogs/TestDialog.fodlg", DialogFile);

        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
        rig.AddBakedFile("Metadata.fometa-server", metadata_blob);
        rig.AddBakedFile("DialogTestScripts.fos-bin-server", make_script_blob(ValidScript));

        DialogBaker baker(rig.MakeContext("DialogPackTest"));
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        CHECK(rig.Outputs.contains("Dialogs/TestDialog.fodlg"));
    }

    SECTION("RejectsDialogDemandCallbacksWithoutAttribute")
    {
        TestRig rig;
        rig.AddSourceFile("Dialogs/TestDialog.fodlg", DialogFile);

        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
        rig.AddBakedFile("Metadata.fometa-server", metadata_blob);
        rig.AddBakedFile("DialogTestScripts.fos-bin-server", make_script_blob(MissingDemandAttributeScript));

        DialogBaker baker(rig.MakeContext("DialogPackTest"));
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), DialogBakerException);
    }

    SECTION("RejectsDialogResultCallbacksWithoutAttribute")
    {
        TestRig rig;
        rig.AddSourceFile("Dialogs/TestDialog.fodlg", DialogFile);

        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();
        rig.AddBakedFile("Metadata.fometa-server", metadata_blob);
        rig.AddBakedFile("DialogTestScripts.fos-bin-server", make_script_blob(MissingResultAttributeScript));

        DialogBaker baker(rig.MakeContext("DialogPackTest"));
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), DialogBakerException);
    }
#endif
}

FO_END_NAMESPACE
