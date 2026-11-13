// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ModelBounds.h"

FO_BEGIN_NAMESPACE

class FileSystem;

constexpr string_view SPRITE_INFO_DIRECTORY {"SpriteInfo"};
constexpr int32_t SPRITE_INFO_VERSION {2};

struct SpriteFrameInfo
{
    optional<int32_t> SharedFrameIndex {};
    ipos32 Offset {};
    isize32 Size {};
    ipos32 NextOffset {};
    timespan Duration {};
};

struct SpriteDirInfo
{
    vector<SpriteFrameInfo> Frames {};
};

auto GetSpriteDirDuration(const SpriteDirInfo& direction) -> timespan;
auto GetSpriteDurationMilliseconds(timespan duration) -> uint32_t;

struct SpriteInfo
{
    vector<SpriteDirInfo> Directions {};
};

struct SpriteInfoFileEntry
{
    string SourcePath {};
    string ResourcePath {};
    SpriteInfo Info {};
};

#if FO_ENABLE_3D

struct ModelAnimationInfo
{
    ModelBounds3D ModelBounds {};
    ModelBounds3D ViewBounds {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, timespan> AnimationDurations {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, ModelBounds3D> AnimationBounds {};
};

auto ReadModelAnimationInfo(const FileSystem& resources, hash_resolver& hashes) -> unordered_map<hstring, ModelAnimationInfo>;

#endif

struct AnimationInfo
{
    optional<SpriteInfo> Sprite {};
#if FO_ENABLE_3D
    optional<ModelAnimationInfo> Model {};
#endif
};

auto ReadAnimationInfo(const FileSystem& resources, hash_resolver& hashes) -> unordered_map<hstring, AnimationInfo>;
auto ReadSpriteInfoFile(string_view file_name, string_view content) -> vector<SpriteInfoFileEntry>;
auto WriteSpriteInfoFile(const vector<SpriteInfoFileEntry>& entries) -> string;

FO_END_NAMESPACE
