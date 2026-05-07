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

TEST_CASE("EffectBaker")
{
    using namespace BakerTests;

    TestRig rig;
    const auto bakers = MakeRequestedBakers({string(EffectBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == EffectBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 4);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
}

TEST_CASE("EffectBakerBakesExplicitBindings")
{
    using namespace BakerTests;

    constexpr string_view effect = R"(
[Effect]

[VertexShader]
layout(binding = 0, std140) uniform ProjBuf { mat4 ProjectionMatrix; };

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec4 InColor;
layout(location = 2) in vec2 InTexCoord;

layout(location = 0) out vec2 TexCoord;

void main(void)
{
    gl_Position = ProjectionMatrix * vec4(InPosition.xy, 0.0, 1.0);
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

    TestRig rig;
    rig.AddSourceFile("Effects/Test.fofx", effect);

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
