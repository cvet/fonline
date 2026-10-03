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
#include "ModelBounds.h"
#include "ModelMeshData.h"
#include "ModelSourceLoader.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ModelBoundsException);

// How a bounds pass measures the posed geometry. Both modes walk the same timeline, because a coarser one
// misses the extreme of a fast arc and clips a model
enum class ModelBoundsMeasurement : uint8_t
{
    // Every skinned vertex at every sample: the exact envelope
    PerVertex,
    // One envelope box per bone slot. A blended vertex is a convex combination of its bones' transformed
    // positions, so the union of the transformed boxes always contains it and the result can only over-size
    PerBoneEnvelope,
};

// One baked model prepared for repeated bounds queries, so a model measured against hundreds of clips
// reads its vertex data once
class ModelBoundsSampler final
{
public:
    ModelBoundsSampler(const ModelMeshData& model_data, const vector<string>& disabled_meshes, ModelBoundsMeasurement measurement);
    ModelBoundsSampler(const ModelBoundsSampler&) = delete;
    ModelBoundsSampler(ModelBoundsSampler&&) noexcept = default;
    auto operator=(const ModelBoundsSampler&) -> ModelBoundsSampler& = delete;
    auto operator=(ModelBoundsSampler&&) noexcept -> ModelBoundsSampler& = default;
    ~ModelBoundsSampler() = default;

    // False when the hierarchy is unusable or the disabled-mesh set leaves nothing to measure. The caller
    // decides whether that is a hard failure or a retry against the unfiltered mesh
    [[nodiscard]] auto IsMeasurable() const noexcept -> bool { return _measurable; }

    auto CalculateStaticBounds() const -> optional<ModelBounds3D>;
    auto CalculateAnimationBounds(const ModelAnimationSource& animation, bool reversed) const -> optional<ModelBounds3D>;

    // Rest-pose and per-sample world transforms of one bone. Every rigid attachment on that bone folds its own
    // box through the shared track instead of walking the hierarchy once per attachment
    auto GetBindPoseBoneTransform(string_view bone) const -> optional<mat44>;
    auto SampleBoneTransforms(const ModelAnimationSource& animation, bool reversed, string_view bone) const -> optional<vector<mat44>>;

private:
    // Only what a bound needs: the renderer vertex carries normals, tangents and colours this pass never reads
    struct Vertex
    {
        vec3 Position {};
        float32_t BlendWeights[MODEL_MESH_BONES_PER_VERTEX] {};
        float32_t BlendIndices[MODEL_MESH_BONES_PER_VERTEX] {};
    };

    struct Mesh
    {
        vector<Vertex> Vertices {};
        vector<ModelMeshIndexData> Indices {};
        vector<string> SkinBoneNames {};
        vector<mat44> SkinBoneOffsets {};
    };

    struct Bone
    {
        string Name {};
        mat44 BindTransform {};
        optional<size_t> Parent {};
        optional<Mesh> AttachedMesh {};
    };

    // Geometry of one enabled mesh: the vertices its indices actually reach, the bone slot each skin index
    // resolves to, and the envelope box of the vertices each of those slots moves
    struct DrawableMesh
    {
        size_t OwnerBone {};
        vector<size_t> VertexIndices {};
        vector<size_t> SkinBones {};
        vector<optional<ModelBounds3D>> SkinBoneEnvelopes {};
        optional<ModelBounds3D> RigidEnvelope {};
    };

    void AppendBone(const ModelMeshBoneData& bone, optional<size_t> parent);
    auto BuildBoneIndex() -> bool;
    auto BuildBindPoseTransforms() -> bool;
    auto BuildDrawableMeshes(const vector<string>& disabled_meshes) -> bool;
    void BuildMeshEnvelopes(DrawableMesh& drawable_mesh) const;
    auto BuildSampleTimes(const ModelAnimationSource& animation, const vector<nptr<const ModelAnimationJointSource>>& outputs, bool reversed) const -> optional<vector<float32_t>>;
    auto BuildBoneOutputs(const ModelAnimationSource& animation) const -> optional<vector<nptr<const ModelAnimationJointSource>>>;
    auto BuildCombinedTransforms(const vector<nptr<const ModelAnimationJointSource>>& outputs, float32_t time, float32_t duration, bool reversed, vector<mat44>& combined_transforms) const -> bool;
    auto IncludePosedGeometry(const vector<mat44>& combined_transforms, optional<ModelBounds3D>& bounds) const -> bool;

    ModelBoundsMeasurement _measurement {};
    vector<Bone> _bones {};
    unordered_map<string, size_t> _boneIndex {};
    vector<mat44> _bindPoseTransforms {};
    vector<DrawableMesh> _drawableMeshes {};
    bool _hierarchyUsable {};
    bool _measurable {};
};

// Fold a rigid attachment box through the parent bone transforms prepared by ModelBoundsSampler
auto CalculateRigidAttachmentBounds(const_span<mat44> bone_transforms, const ModelBounds3D& attachment_bounds, const mat44& attachment_transform) -> optional<ModelBounds3D>;

FO_END_NAMESPACE

#endif
