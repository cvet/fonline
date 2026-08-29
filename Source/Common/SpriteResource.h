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

#include "AnimationInfo.h"

FO_BEGIN_NAMESPACE

constexpr uint8_t SPRITE_RESOURCE_MAGIC = 43;
constexpr uint8_t SPRITE_RESOURCE_VERSION = 4;

// A sprite may carry a per-pixel surface plane, baked from the `<name>.depth.png` and
// `<name>.normal.png` companions placed beside the source image. It describes the geometry the flat
// image stands for, so a shader can light it and sort it in depth rather than treat it as a decal.
//
// The two companions are packed into one RGBA quad per pixel, laid out to be uploaded into a texture
// with no per-pixel work at load time and sampled with the sprite's own texture coordinates:
//
//   r, g  camera-space unit normal, hemi-octahedral. Only two channels because a unit vector has two
//         degrees of freedom, and the third is not even ambiguous here: every normal faces the
//         camera, so the sign is known. Decode:
//             e = vec2(r, g) * 2 - 1
//             p = vec2(e.x + e.y, e.x - e.y) * 0.5
//             n = normalize(vec3(p, 1 - abs(p.x) - abs(p.y)))
//             n.z = -n.z
//   b, a  distance along the view ray, as a big-endian pair spanning this frame's own range:
//             metres = DepthNear + (b * 256 + a) / 65535 * (DepthFar - DepthNear)
//
// Depth is normalised per frame rather than on one absolute scale, because a scale wide enough for a
// building leaves a prop occupying a few per cent of the sixteen bits. The two bounds below make the
// values comparable across sprites again, at eight bytes per frame.
//
// The split of depth across two channels survives bilinear filtering intact: the decode is linear
// and the hardware filters each channel independently in floating point, so filtering the pair and
// then decoding equals decoding and then filtering.
//
// Coverage is deliberately absent - the frame's own alpha is the single source of truth for what is
// solid, and a transparent pixel has no surface to describe.

enum class SpriteMeshKind : uint8_t
{
    Quad = 0,
    Empty = 1,
    Mesh = 2,
};

struct SpriteMeshData
{
    isize32 SourceSize {};
    ipos32 SourceOffset {};
    vector<ipos32> Vertices {};
    vector<uint16_t> Indices {};
};

struct SpriteResourceFrameData
{
    optional<uint16_t> SharedFrameIndex {};
    ipos32 Offset {};
    isize32 Size {};
    ipos32 NextOffset {};
    vector<ucolor> Pixels {};
    // Empty when the source had no companions. Otherwise one entry per pixel, in the same row-major
    // order as Pixels, packed as described at the top of this file.
    vector<ucolor> Surface {};
    float32_t DepthNear {};
    float32_t DepthFar {};
    optional<SpriteMeshData> Mesh {};
};

struct SpriteResourceDirectionData
{
    vector<SpriteResourceFrameData> Frames {};
};

struct SpriteResourceData
{
    AnimationInfo Animation {};
    vector<SpriteResourceDirectionData> Directions {};
};

struct SpriteResourceImageData
{
    isize32 Size {};
    vector<ucolor> Pixels {};
};

auto ReadSpriteResource(const_span<uint8_t> data) -> SpriteResourceData;
auto ExtractSpriteResourceFrameImage(SpriteResourceFrameData frame) -> SpriteResourceImageData;

FO_END_NAMESPACE
