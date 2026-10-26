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

#pragma once

#include "Common.h"

#include "Rendering.h"
#include "SpriteResource.h"

FO_BEGIN_NAMESPACE

struct PreparedSpriteTransform
{
    bool MapSprite {};
    float32_t ProjectionY {};
    float32_t ProjectionZ {};
    bool Rotate {};
    fpos32 Center {};
    float32_t CosAngle {1.0f};
    float32_t SinAngle {};
    float32_t YScale {1.0f};
    bool Standing {};
    float32_t SceneRootY {};
    float32_t DepthSlope {};
    float32_t EggFlags[2] {};
};

struct PreparedSpriteGeometry
{
    frect32 DrawRect {};
    frect32 TextureRect {};
    ucolor ColorLeft {};
    ucolor ColorRight {};
    nptr<const SpriteMeshData> Mesh {};
    isize32 FrameSize {};
    PreparedSpriteTransform Transform {};
    // Deferred commands pin immutable topology; the direct call borrows it only until the kernel returns
    shared_ptr<const SpriteMeshData> MeshOwner {};
};

[[nodiscard]] auto GetPreparedSpriteVertexCount(const PreparedSpriteGeometry& geometry) -> size_t;
[[nodiscard]] auto GetPreparedSpriteIndexCount(const PreparedSpriteGeometry& geometry) -> size_t;
void WritePreparedSpriteGeometry(const PreparedSpriteGeometry& geometry, span<Vertex2D> vertices, span<vindex_t> indices, size_t base_vertex);
void TransformPreparedSpriteGeometry(const PreparedSpriteTransform& transform, span<Vertex2D> vertices);

FO_END_NAMESPACE

