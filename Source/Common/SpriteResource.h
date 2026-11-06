// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "AnimationInfo.h"

FO_BEGIN_NAMESPACE

constexpr uint8_t SPRITE_RESOURCE_MAGIC {43};
constexpr uint8_t SPRITE_RESOURCE_VERSION {3};

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
    optional<int32_t> SharedFrameIndex {};
    ipos32 Offset {};
    isize32 Size {};
    ipos32 NextOffset {};
    timespan Duration {};
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
