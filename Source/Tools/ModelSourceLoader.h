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

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

#include "FileSystem.h"

FO_BEGIN_NAMESPACE

inline constexpr float32_t MODEL_ANIMATION_HALF_MAX = 65504.0f;
inline constexpr float32_t MODEL_ANIMATION_QUATERNION_NORM_TOLERANCE = 1.0e-4f;

struct ModelAnimationVec3Track
{
    vector<float32_t> Times {};
    vector<vec3> Values {};
};

struct ModelAnimationQuaternionTrack
{
    vector<float32_t> Times {};
    vector<quaternion> Values {};
};

struct ModelAnimationJointSource
{
    string OutputName {};
    vector<string> Hierarchy {};
    ModelAnimationVec3Track Translation {};
    ModelAnimationQuaternionTrack Rotation {};
    ModelAnimationVec3Track Scale {};
};

struct ModelAnimationSource
{
    string FileName {};
    string Name {};
    float32_t Duration {};
    vector<ModelAnimationJointSource> Joints {};
};

struct ModelSkeletonJoint
{
    string Name {};
    vector<string> Hierarchy {};
    mat44 RestLocalTransform {1.0f};
};

struct ModelSkeletonSource
{
    string FileName {};
    vector<ModelSkeletonJoint> Joints {};
};

struct ModelSkeletonClipSource
{
    string FileName {};
    string ClipName {};
    vector<ModelSkeletonJoint> Joints {};
    vector<vector<string>> AnimatedJointHierarchies {};
};

FO_DECLARE_EXCEPTION(ModelSourceLoaderException);

struct ModelSourceAsset
{
    string FileName {};
    uint64_t WriteTime {};
    ModelSkeletonSource Skeleton {};
    vector<ModelAnimationSource> Animations {};
};

[[nodiscard]] auto LoadModelSourceAsset(string_view path, const File& file) -> ModelSourceAsset;
void ValidateModelSourceAsset(const ModelSourceAsset& asset);

// One cache belongs to one immutable FileCollection/bake pass. Concurrent Get()
// calls for the same path share both a successful result and a failing load.
class ModelSourceAssetCache final
{
public:
    // The callback may run concurrently for different paths; same-path calls are single-flight.
    using LoadCallback = function<ModelSourceAsset(string_view, const File&)>;

    explicit ModelSourceAssetCache(const FileCollection& files, LoadCallback load_callback = {});
    ModelSourceAssetCache(FileCollection&&, LoadCallback = {}) = delete;
    ModelSourceAssetCache(const ModelSourceAssetCache&) = delete;
    ModelSourceAssetCache(ModelSourceAssetCache&&) noexcept = delete;
    auto operator=(const ModelSourceAssetCache&) -> ModelSourceAssetCache& = delete;
    auto operator=(ModelSourceAssetCache&&) noexcept -> ModelSourceAssetCache& = delete;
    ~ModelSourceAssetCache() = default;

    [[nodiscard]] auto Get(string_view path) const -> shared_ptr<const ModelSourceAsset>;

private:
    using PendingAsset = std::shared_future<shared_ptr<const ModelSourceAsset>>;

    ptr<const FileCollection> _files;
    LoadCallback _loadCallback {};
    mutable mutex _mutex {};
    mutable unordered_map<string, PendingAsset> _loads FO_TSA_GUARDED_BY(_mutex) {};
};

FO_END_NAMESPACE

#endif
