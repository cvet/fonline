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

#include "AnimationInfo.h"

FO_BEGIN_NAMESPACE

constexpr uint8_t SPRITE_RESOURCE_MAGIC = 43;
constexpr uint8_t SPRITE_RESOURCE_VERSION = 2;

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
