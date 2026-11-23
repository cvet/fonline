// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

#include "FileSystem.h"

FO_BEGIN_NAMESPACE

inline constexpr float32_t MODEL_ANIMATION_HALF_MAX {65504.0f};
inline constexpr float32_t MODEL_ANIMATION_QUATERNION_NORM_TOLERANCE {1.0e-4f};

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

auto LoadModelSourceAsset(string_view path, const File& file) -> ModelSourceAsset;
void ValidateModelSourceAsset(const ModelSourceAsset& asset);

// One cache belongs to one immutable FileCollection/bake pass. Concurrent Get()
// calls for the same path share both a successful result and a failing load
class ModelSourceAssetCache final
{
public:
    // The callback may run concurrently for different paths; same-path calls are single-flight
    using LoadCallback = copyable_function<ModelSourceAsset(string_view, const File&)>;

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
