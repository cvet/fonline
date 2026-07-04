//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "ConfigFile.h"
#include "EffectBaker.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

static constexpr string_view VALID_EFFECT = R"(
[Effect]

[VertexShader]
layout(binding = 0, std140) uniform ProjBuf { mat4 ProjMatrix; };

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec4 InColor;
layout(location = 2) in vec2 InTexCoord;

layout(location = 0) out vec2 TexCoord;

void main(void)
{
    gl_Position = ProjMatrix * vec4(InPosition.xy, 0.0, 1.0);
    TexCoord = InTexCoord;
}

[FragmentShader]
layout(binding = 0) uniform sampler2D MainTex;
layout(binding = 3, std140) uniform TimeBuf { vec4 FrameTime; vec4 GameTime; };

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = texture(MainTex, TexCoord + GameTime.xx * 0.0);
}
)";

static constexpr string_view EFFECT_WITH_AUX_BINDINGS = R"(
[Effect]

[VertexShader]
layout(location = 0) in vec3 InPosition;
layout(location = 2) in vec2 InTexCoord;
layout(location = 0) out vec2 TexCoord;

void main(void)
{
    gl_Position = vec4(InPosition.xy, 0.0, 1.0);
    TexCoord = InTexCoord;
}

[FragmentShader]
layout(binding = 2) uniform sampler2D IndoorMaskTex;
layout(binding = 4, std140) uniform RandomValueBuf { vec4 RandomValue; };
layout(binding = 5, std140) uniform ScriptValueBuf { vec4 ScriptValue[MAX_SCRIPT_VALUES / 4]; };
layout(binding = 6, std140) uniform CameraBuf { vec4 MapAnchorScreenPos; vec4 ChunkScreenAnchor; };

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

void main(void)
{
    vec2 uv = MapAnchorScreenPos.xy + TexCoord * MapAnchorScreenPos.zw;
    FragColor = texture(IndoorMaskTex, uv) + vec4(RandomValue.x + ScriptValue[0].x + ChunkScreenAnchor.x * 0.0);
}
)";

static constexpr string_view MULTI_PASS_EFFECT = R"(
[Effect]
Passes = 2

[VertexShader]
layout(location = 0) in vec3 InPosition;

void main(void)
{
    gl_Position = vec4(InPosition.xy, 0.0, 1.0);
}

[FragmentShader]
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = vec4(0.25, 0.5, 0.75, 1.0);
}

[VertexShader Pass2]
layout(location = 0) in vec3 InPosition;

void main(void)
{
    gl_Position = vec4(InPosition.xy * 0.5, 0.0, 1.0);
}

[FragmentShader Pass2]
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = vec4(0.75, 0.5, 0.25, 1.0);
}
)";

static constexpr string_view EFFECT_WITH_BAD_PROJ_BUFFER = R"(
[Effect]

[VertexShader]
layout(binding = 0, std140) uniform ProjBuf { vec4 OnlyOne; };

layout(location = 0) in vec3 InPosition;

void main(void)
{
    gl_Position = vec4(InPosition.xy + OnlyOne.xy, 0.0, 1.0);
}

[FragmentShader]
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = vec4(1.0);
}
)";

TEST_CASE("EffectBaker")
{
    using namespace BakerTests;

    TestRig rig;
    const auto bakers = MakeRequestedBakers({string(EffectBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == EffectBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 4);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));

    SECTION("SkipsNonEffectSourcesAndTargets")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Readme.txt", "not an effect");
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);

        EffectBaker baker {local_rig.MakeContext()};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "skip.bin"));
        CHECK(local_rig.Outputs.empty());

        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("Effects/Test.fofx"));
    }

    SECTION("BakesWithoutBakeChecker")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);

        auto context = local_rig.MakeContext();
        context->BakeChecker = {};

        EffectBaker baker {std::move(context)};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("Effects/Test.fofx"));
        CHECK(local_rig.Outputs.contains("Effects/Test.fofx-1-info"));
    }

    SECTION("BakeCheckerCanSkipEffect")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT, 77);

        vector<pair<string, uint64_t>> checks;
        EffectBaker baker {local_rig.MakeContext("Effects", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return path != "Effects/Test.fofx";
        })};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.empty());
        CHECK(std::ranges::any_of(checks, [](const auto& check) { return check == pair<string, uint64_t> {"Effects/Test.fofx", 77}; }));
        CHECK(std::ranges::any_of(checks, [](const auto& check) { return check == pair<string, uint64_t> {"Effects/Test.fofx-1-info", 77}; }));
        CHECK(std::ranges::any_of(checks, [](const auto& check) { return check == pair<string, uint64_t> {"Effects/Test.fofx-1-frag-msl_ios", 77}; }));
    }

    SECTION("SkipsMissingExplicitTarget")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);

        EffectBaker baker {local_rig.MakeContext()};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "Effects/Missing.fofx-1-info"));
        CHECK(local_rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipExplicitTarget")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT, 88);

        EffectBaker baker {local_rig.MakeContext("Effects", [](string_view, uint64_t) { return false; })};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "Effects/Test.fofx-1-info"));
        CHECK(local_rig.Outputs.empty());
    }

    SECTION("BakesExplicitTarget")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);

        EffectBaker baker {local_rig.MakeContext()};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "Effects/Test.fofx-1-info"));
        CHECK(local_rig.Outputs.contains("Effects/Test.fofx"));
        CHECK(local_rig.Outputs.contains("Effects/Test.fofx-1-info"));
    }

    SECTION("BakesPassSpecificShaders")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Multi.fofx", MULTI_PASS_EFFECT);

        EffectBaker baker {local_rig.MakeContext()};
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.contains("Effects/Multi.fofx"));
        CHECK(local_rig.Outputs.contains("Effects/Multi.fofx-1-info"));
        CHECK(local_rig.Outputs.contains("Effects/Multi.fofx-2-info"));
        CHECK(local_rig.Outputs.contains("Effects/Multi.fofx-2-vert-spv"));
        CHECK(local_rig.Outputs.contains("Effects/Multi.fofx-2-frag-spv"));
    }

    SECTION("RejectsInvalidEffects")
    {
        const vector<pair<string, string>> invalid_effects = {
            {"Effects/NoEffect.fofx", "[VertexShader]\nvoid main(void) { gl_Position = vec4(0.0); }\n"},
            {"Effects/NoVertex.fofx", "[Effect]\n\n[FragmentShader]\nvoid main(void) { }\n"},
            {"Effects/NoFragment.fofx", "[Effect]\n\n[VertexShader]\nvoid main(void) { gl_Position = vec4(0.0); }\n"},
            {"Effects/BadVertex.fofx", "[Effect]\n\n[VertexShader]\nvoid main(void) { gl_Position = ; }\n\n[FragmentShader]\nlayout(location = 0) out vec4 FragColor;\nvoid main(void) { FragColor = vec4(1.0); }\n"},
            {"Effects/BadFragment.fofx", "[Effect]\n\n[VertexShader]\nvoid main(void) { gl_Position = vec4(0.0); }\n\n[FragmentShader]\nlayout(location = 0) out vec4 FragColor;\nvoid main(void) { FragColor = ; }\n"},
        };

        for (const auto& [path, content] : invalid_effects) {
            TestRig local_rig;
            local_rig.AddSourceFile(path, content);

            EffectBaker baker {local_rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), EffectBakerException);
        }
    }

    SECTION("RejectsMismatchedKnownUniformBuffer")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/BadProjBuffer.fofx", EFFECT_WITH_BAD_PROJ_BUFFER);

        EffectBaker baker {local_rig.MakeContext()};
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), EffectBakerException);
    }
}

TEST_CASE("EffectBakerBakesAuxiliaryBindings")
{
    using namespace BakerTests;

    TestRig rig;
    rig.AddSourceFile("Effects/Aux.fofx", EFFECT_WITH_AUX_BINDINGS);

    EffectBaker baker {rig.MakeContext()};

    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    const auto info = ConfigFile("Effects/Aux.fofx-1-info", rig.GetOutputText("Effects/Aux.fofx-1-info"));

    REQUIRE(info.HasSection("EffectInfo"));
    CHECK(info.GetAsInt("EffectInfo", "IndoorMaskTex", -1) == 2);
    CHECK(info.GetAsInt("EffectInfo", "RandomValueBuf", -1) == 4);
    CHECK(info.GetAsInt("EffectInfo", "ScriptValueBuf", -1) == 5);
    CHECK(info.GetAsInt("EffectInfo", "CameraBuf", -1) == 6);
}

TEST_CASE("EffectBakerBakesExplicitBindings")
{
    using namespace BakerTests;

    TestRig rig;
    rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);

    EffectBaker baker {rig.MakeContext()};

    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    const auto info = ConfigFile("Effects/Test.fofx-1-info", rig.GetOutputText("Effects/Test.fofx-1-info"));

    REQUIRE(info.HasSection("EffectInfo"));
    CHECK(info.GetAsInt("EffectInfo", "MainTex", -1) == 0);
    CHECK(info.GetAsInt("EffectInfo", "ProjBuf", -1) == 0);
    CHECK(info.GetAsInt("EffectInfo", "TimeBuf", -1) == 3);
    CHECK(rig.Outputs.contains("Effects/Test.fofx-1-vert-spv"));
    CHECK(rig.Outputs.contains("Effects/Test.fofx-1-frag-spv"));
}

FO_END_NAMESPACE
