//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

namespace EffectDefaults
{
    inline constexpr string_view Sprite2D = "Effects/2D_Default.fofx";
    inline constexpr string_view NoDepth2D = "Effects/2D_NoDepth.fofx";
    inline constexpr string_view Primitive = "Effects/Primitive_Default.fofx";
    inline constexpr string_view Light = "Effects/Primitive_Light.fofx";
    inline constexpr string_view Fog = "Effects/Primitive_Fog.fofx";
    inline constexpr string_view FlushRenderTarget = "Effects/Flush_RenderTarget.fofx";
    inline constexpr string_view FlushPrimitive = "Effects/Flush_Primitive.fofx";
    inline constexpr string_view FlushMap = "Effects/Flush_Map.fofx";
    inline constexpr string_view FlushLight = "Effects/Flush_Light.fofx";
    inline constexpr string_view FlushFog = "Effects/Flush_Fog.fofx";
    inline constexpr string_view SkinnedModel = "Effects/3D_Skinned.fofx";

    inline constexpr string_view ViewAreaFog = "Effects/Primitive_ViewAreaFog.fofx";
    inline constexpr string_view ParticleColorMul = "Effects/Particles_ColorMulAtlas.fofx";
    inline constexpr string_view ParticleColorAdd = "Effects/Particles_ColorAddAtlas.fofx";
    inline constexpr string_view ParticleColorSub = "Effects/Particles_ColorSubAtlas.fofx";
    inline constexpr string_view ParticleDistortion = "Effects/Particles_DistortionAtlas.fofx";
    inline constexpr string_view ParticleDistortionAdd = "Effects/Particles_DistortionAddAtlas.fofx";

    inline constexpr auto Paths = std::to_array<string_view>({
        Sprite2D,
        NoDepth2D,
        Primitive,
        Light,
        Fog,
        FlushRenderTarget,
        FlushPrimitive,
        FlushMap,
        FlushLight,
        FlushFog,
        ViewAreaFog,
#if FO_ENABLE_3D
        SkinnedModel,
#endif
#if FO_EFFEKSEER_PARTICLES
        ParticleColorMul,
        ParticleColorAdd,
        ParticleColorSub,
#if FO_ENABLE_3D
        ParticleDistortion,
        ParticleDistortionAdd,
#endif
#endif
    });
}

namespace FontDefaults
{
    inline constexpr string_view Updater = "Fonts/Default.fofnt";
}

FO_END_NAMESPACE
