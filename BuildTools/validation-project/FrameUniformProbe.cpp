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

#include "Application.h"
#include "catch_amalgamated.hpp"
#include "EffectBaker.h"
#include "Test_BakerHelpers.h"

FO_USING_NAMESPACE();

static auto RunProbe() -> int32_t
{
    constexpr string_view shader = R"(
[Effect]
BlendFunc = One Zero

[VertexShader]
layout(location = 0) in vec3 InPosition;
void main(void)
{
    gl_Position = vec4(InPosition.xy, 0.0, 1.0);
}

[FragmentShader]
layout(binding = 3, std140) uniform TimeBuf { vec4 FrameTime; vec4 GameTime; };
layout(binding = 4, std140) uniform RandomValueBuf { vec4 RandomValue; };
layout(location = 0) out vec4 FragColor;
void main(void)
{
    FragColor = vec4(GameTime.x, FrameTime.x, RandomValue.x, 1.0);
}
)";

    BakerTests::TestRig rig;
    rig.AddSourceFile("Effects/TimeProbe.fofx", shader);
    EffectBaker baker {rig.MakeContext()};
    baker.BakeFiles(rig.GetAllSourceFiles(), "");
    RenderEffectLoader loader = [&rig](string_view path) -> string {
        auto found = rig.Outputs.find(string(path));
        FO_VERIFY_AND_THROW(found != rig.Outputs.end(), "Probe baked effect output is missing", path);
        return string(found->second.begin(), found->second.end());
    };

    auto app = GetApp();
    auto& render = app->Render;
    auto target = render.CreateTexture({8, 4}, false, false);
    auto buffer = render.CreateDrawBuffer(false);
    auto effect = render.CreateEffect(EffectUsage::QuadSprite, "Effects/TimeProbe.fofx", loader);
    buffer->Vertices = {
        {.PosX = -1.0f, .PosY = -1.0f},
        {.PosX = -1.0f, .PosY = 1.0f},
        {.PosX = 1.0f, .PosY = 1.0f},
        {.PosX = 1.0f, .PosY = -1.0f},
    };
    buffer->VertCount = 4;
    buffer->Indices = {0, 1, 3, 1, 2, 3};
    buffer->IndCount = 6;
    bool accepted = true;

    for (int32_t frame = 0; frame < 2; frame++) {
        app->BeginFrame();
        bool frame_open = true;
        auto finish_frame = scope_exit([&]() noexcept {
            if (frame_open) {
                safe_call([&] {
                    render.SetRenderTarget(nullptr);
                    app->EndFrame();
                });
            }
        });

        render.SetRenderTarget(target.get());
        render.ClearRenderTarget(ucolor {0, 0, 0, 255});
        buffer->Upload(EffectUsage::QuadSprite);
        effect->TimeBuf = RenderEffect::TimeBuffer();
        effect->TimeBuf->GameTime[0] = frame == 0 ? 0.25f : 0.75f;
        effect->TimeBuf->FrameTime[0] = frame == 0 ? 0.5f : 0.25f;
        effect->RandomValueBuf = RenderEffect::RandomValueBuffer();
        effect->RandomValueBuf->RandomValue[0] = frame == 0 ? 0.75f : 0.5f;

        render.EnableScissor({0, 0, 4, 4});
        effect->DrawBuffer(buffer);
        bool time_after_first = effect->TimeBuf.has_value();
        render.EnableScissor({4, 0, 4, 4});
        effect->DrawBuffer(buffer);
        render.DisableScissor();

        auto pixels = target->GetTextureRegion({0, 0}, {8, 4});
        FO_VERIFY_AND_THROW(pixels.size() == 32, "Probe texture readback size is invalid", pixels.size());
        ucolor first = pixels[9];
        ucolor second = pixels[13];
        ucolor expected = frame == 0 ? ucolor {64, 128, 191, 255} : ucolor {191, 64, 128, 255};
        auto matches = [expected](ucolor value) {
            return std::abs(int32_t {value.comp.r} - int32_t {expected.comp.r}) <= 1
                && std::abs(int32_t {value.comp.g} - int32_t {expected.comp.g}) <= 1
                && std::abs(int32_t {value.comp.b} - int32_t {expected.comp.b}) <= 1
                && value.comp.a == 255;
        };
        bool first_matches = matches(first);
        bool second_matches = matches(second);
        accepted = accepted && first_matches && second_matches;
        logging::write("HUD_TIME_PROBE frame={} first={},{},{},{} second={},{},{},{} expected={},{},{},{} time_after_first={} first_matches={} second_matches={}",
            frame, first.comp.r, first.comp.g, first.comp.b, first.comp.a,
            second.comp.r, second.comp.g, second.comp.b, second.comp.a,
            expected.comp.r, expected.comp.g, expected.comp.b, expected.comp.a,
            time_after_first, first_matches, second_matches);

        render.SetRenderTarget(nullptr);
        app->EndFrame();
        frame_open = false;
    }

    logging::write("HUD_TIME_PROBE terminal accepted={}", accepted);
    return accepted ? 0 : 1;
}

int32_t main(int32_t argc, char** argv)
{
    try {
        InitApp(CommandLineArgs {argc, argv}, AppInitFlags::None);
        return RunProbe();
    }
    catch (const std::exception& ex) {
        logging::write("HUD_TIME_PROBE failed: {}", ex.what());
        return 2;
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}
