// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

namespace EffectDefaults
{
    inline constexpr string_view Sprite2D {"Effects/2D_Default.fofx"};
    inline constexpr string_view NoDepth2D {"Effects/2D_NoDepth.fofx"};
    inline constexpr string_view Primitive {"Effects/Primitive_Default.fofx"};
    inline constexpr string_view Light {"Effects/Primitive_Light.fofx"};
    inline constexpr string_view Fog {"Effects/Primitive_Fog.fofx"};
    inline constexpr string_view FlushRenderTarget {"Effects/Flush_RenderTarget.fofx"};
    inline constexpr string_view FlushPrimitive {"Effects/Flush_Primitive.fofx"};
    inline constexpr string_view FlushMap {"Effects/Flush_Map.fofx"};
    inline constexpr string_view FlushLight {"Effects/Flush_Light.fofx"};
    inline constexpr string_view FlushFog {"Effects/Flush_Fog.fofx"};
    inline constexpr string_view SkinnedModel {"Effects/3D_Skinned.fofx"};

    inline constexpr string_view ViewAreaFog {"Effects/Primitive_ViewAreaFog.fofx"};
    inline constexpr string_view ParticleColorMul {"Effects/Particles_ColorMulAtlas.fofx"};
    inline constexpr string_view ParticleColorAdd {"Effects/Particles_ColorAddAtlas.fofx"};
    inline constexpr string_view ParticleColorSub {"Effects/Particles_ColorSubAtlas.fofx"};
    inline constexpr string_view ParticleDistortion {"Effects/Particles_DistortionAtlas.fofx"};
    inline constexpr string_view ParticleDistortionAdd {"Effects/Particles_DistortionAddAtlas.fofx"};

    inline constexpr auto Paths {std::to_array<string_view>({
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
    })};
}

namespace FontDefaults
{
    inline constexpr string_view Updater {"Fonts/Default.fofnt"};
}

FO_END_NAMESPACE
