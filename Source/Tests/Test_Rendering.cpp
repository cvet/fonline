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

#include "Rendering.h"

FO_BEGIN_NAMESPACE

static auto MakeTestEffectLoader() -> RenderEffectLoader
{
    return [](string_view name) -> string {
        if (name == "Effects/Test_Default.fofx") {
            return R"([Effect]
Passes = 1
)";
        }

        if (name == "Effects/Test_Default.fofx-1-info") {
            return R"([EffectInfo]
MainTex = 0
MainTexBuf = 1
ProjBuf = 2
)";
        }

        throw GenericException("Unexpected test effect request", name);
    };
}

TEST_CASE("NullRenderer")
{
    Null_Renderer renderer;
    GlobalSettings settings {false};
    settings.ScreenWidth = 320;
    settings.ScreenHeight = 200;
    renderer.Init(settings, nullptr);

    SECTION("TextureReadWriteAndClear")
    {
        auto tex = renderer.CreateTexture({4, 4}, false, false);
        REQUIRE(tex != nullptr);

        const array<ucolor, 4> row_data {{ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}, ucolor {9, 10, 11, 12}, ucolor {13, 14, 15, 16}}};
        tex->UpdateTextureRegion({0, 0}, {4, 1}, row_data.data());

        CHECK(tex->GetTexturePixel({0, 0}) == row_data[0]);
        CHECK(tex->GetTexturePixel({3, 0}) == row_data[3]);

        renderer.SetRenderTarget(tex.get());
        renderer.ClearRenderTarget(ucolor {20, 30, 40, 50});

        CHECK(tex->GetTexturePixel({1, 1}) == ucolor {20, 30, 40, 50});
    }

    SECTION("DrawBufferUploadAndEffectDraw")
    {
        auto tex = renderer.CreateTexture({2, 2}, false, false);
        auto dbuf = renderer.CreateDrawBuffer(false);
        auto effect = renderer.CreateEffect(EffectUsage::QuadSprite, "Effects/Test_Default.fofx", MakeTestEffectLoader());

        REQUIRE(tex != nullptr);
        REQUIRE(dbuf != nullptr);
        REQUIRE(effect != nullptr);

        dbuf->Vertices.resize(4);
        dbuf->VertCount = 4;
        dbuf->Indices = {0, 1, 2, 2, 3, 0};
        dbuf->IndCount = dbuf->Indices.size();

        REQUIRE_NOTHROW(dbuf->Upload(EffectUsage::QuadSprite));

        effect->MainTex = tex.get();
        REQUIRE_NOTHROW(effect->DrawBuffer(dbuf.get()));
        CHECK(effect->MainTexBuf.has_value());
        CHECK(effect->ProjBuf.has_value());
    }
}

FO_END_NAMESPACE
