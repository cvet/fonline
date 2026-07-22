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

#pragma once

#include "Common.h"
#include "ModelBounds.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

constexpr int32_t MODEL_SPRITE_FRAME_SCALE = 2;

struct ModelSpriteLayout
{
    isize32 DrawSize {};
    irect32 DrawRect {};
    irect32 ViewRect {};
};

struct ModelSpriteBoundsEnvelopeId
{
    array<int32_t, 2> BodyAnimationIndices {-1, -1};
    array<int32_t, 2> MoveAnimationIndices {-1, -1};
    uint64_t CombinedMeshGenerationRevision {};
    uint8_t BodyAnimationCount {};
    uint8_t MoveAnimationCount {};
    bool ShadowEnabled {};
    bool FullFrame {};
};

struct ModelSpriteBounds
{
    irect32 Rect {};
    isize32 RequiredFrameSize {};
    ModelSpriteBoundsEnvelopeId EnvelopeId {};
};

auto CalculateModelSpriteFrameSize(float32_t min_x, float32_t min_y, float32_t max_x, float32_t max_y) -> optional<isize32>;
auto CalculateModelSpriteLayout(const ModelBounds3D& bounds, const mat44& post_direction_transform, const mat44& pre_direction_transform, float32_t projection_factor, bool include_shadow) -> optional<ModelSpriteLayout>;

FO_END_NAMESPACE

#endif
