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

FO_BEGIN_NAMESPACE

class FileSystem;

constexpr string_view SPRITE_INFO_DIRECTORY = "SpriteInfo";
constexpr int32_t SPRITE_INFO_VERSION = 1;

struct SpriteFrameInfo
{
    optional<uint16_t> SharedFrameIndex {};
    ipos32 Offset {};
    isize32 Size {};
    ipos32 NextOffset {};
};

struct SpriteDirInfo
{
    vector<SpriteFrameInfo> Frames {};
};

struct SpriteInfo
{
    uint16_t FrameCount {};
    timespan Duration {};
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

auto ReadModelAnimationInfo(const FileSystem& resources, HashResolver& hash_resolver) -> unordered_map<hstring, ModelAnimationInfo>;

#endif

struct AnimationInfo
{
    optional<SpriteInfo> Sprite {};
#if FO_ENABLE_3D
    optional<ModelAnimationInfo> Model {};
#endif
};

auto ReadAnimationInfo(const FileSystem& resources, HashResolver& hash_resolver) -> unordered_map<hstring, AnimationInfo>;
auto ReadSpriteInfoFile(string_view file_name, string_view content) -> vector<SpriteInfoFileEntry>;
auto WriteSpriteInfoFile(const vector<SpriteInfoFileEntry>& entries) -> string;

FO_END_NAMESPACE
