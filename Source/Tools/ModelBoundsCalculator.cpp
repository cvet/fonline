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

#include "ModelBoundsCalculator.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

static auto SampleJointTransform(const ModelAnimationJointSource& joint, float32_t time, float32_t duration, bool reversed) -> mat44;
static auto SampleVectorTrack(float32_t time, float32_t duration, bool reversed, const ModelAnimationVec3Track& track) -> vec3;
static auto SampleRotationTrack(float32_t time, float32_t duration, bool reversed, const ModelAnimationQuaternionTrack& track) -> quaternion;
static auto FindForwardKeySpan(const vector<float32_t>& times, float32_t time) -> optional<size_t>;
static auto FindReversedKeySpan(const vector<float32_t>& times, float32_t time) -> optional<size_t>;
static auto ValidateJointTracks(const ModelAnimationJointSource& joint) -> bool;
static void AppendTrackSampleTimes(const vector<float32_t>& times, float32_t duration, bool reversed, vector<float32_t>& sample_times);
static auto IsFinite(const vec3& value) -> bool;
static auto IsFinite(const quaternion& value) -> bool;
static auto IsFinite(const mat44& value) -> bool;

ModelBoundsSampler::ModelBoundsSampler(const ModelMeshData& model_data, const vector<string>& disabled_meshes, ModelBoundsMeasurement measurement) :
    _measurement {measurement}
{
    FO_STACK_TRACE_ENTRY();

    try {
        FO_VERIFY_AND_THROW(model_data.RootBone, "Baked model has no root bone");
        AppendBone(*model_data.RootBone, std::nullopt);

        _hierarchyUsable = BuildBoneIndex() && BuildBindPoseTransforms();
        _measurable = _hierarchyUsable && BuildDrawableMeshes(disabled_meshes);
    }
    catch (const ModelBoundsException&) {
        throw;
    }
    catch (const std::exception& ex) {
        throw ModelBoundsException("Invalid baked model data while preparing bounds", ex.what());
    }
}

auto ModelBoundsSampler::CalculateStaticBounds() const -> optional<ModelBounds3D>
{
    FO_STACK_TRACE_ENTRY();

    if (!_measurable) {
        return std::nullopt;
    }

    optional<ModelBounds3D> result;

    if (!IncludePosedGeometry(_bindPoseTransforms, result) || !result) {
        return std::nullopt;
    }

    return CalculateGuardedModelBounds(*result);
}

auto ModelBoundsSampler::CalculateAnimationBounds(const ModelAnimationSource& animation, bool reversed) const -> optional<ModelBounds3D>
{
    FO_STACK_TRACE_ENTRY();

    try {
        if (!_measurable) {
            return std::nullopt;
        }

        optional<vector<nptr<const ModelAnimationJointSource>>> outputs = BuildBoneOutputs(animation);

        if (!outputs) {
            return std::nullopt;
        }

        optional<vector<float32_t>> sample_times = BuildSampleTimes(animation, *outputs, reversed);

        if (!sample_times) {
            return std::nullopt;
        }

        vector<mat44> combined_transforms(_bones.size());
        optional<ModelBounds3D> result;

        for (float32_t sample_time : *sample_times) {
            if (!BuildCombinedTransforms(*outputs, sample_time, animation.Duration, reversed, combined_transforms)) {
                return std::nullopt;
            }
            if (!IncludePosedGeometry(combined_transforms, result)) {
                return std::nullopt;
            }
        }

        if (!result) {
            return std::nullopt;
        }

        return CalculateGuardedModelBounds(*result);
    }
    catch (const ModelBoundsException&) {
        throw;
    }
    catch (const std::exception& ex) {
        throw ModelBoundsException("Invalid baked model data while calculating animation bounds", ex.what());
    }
}

auto ModelBoundsSampler::GetBindPoseBoneTransform(string_view bone) const -> optional<mat44>
{
    FO_STACK_TRACE_ENTRY();

    if (!_hierarchyUsable) {
        return std::nullopt;
    }

    auto it = _boneIndex.find(string(bone));

    if (it == _boneIndex.end()) {
        return std::nullopt;
    }

    return _bindPoseTransforms[it->second];
}

auto ModelBoundsSampler::SampleBoneTransforms(const ModelAnimationSource& animation, bool reversed, string_view bone) const -> optional<vector<mat44>>
{
    FO_STACK_TRACE_ENTRY();

    try {
        if (!_hierarchyUsable) {
            return std::nullopt;
        }

        auto bone_it = _boneIndex.find(string(bone));

        if (bone_it == _boneIndex.end()) {
            return std::nullopt;
        }

        optional<vector<nptr<const ModelAnimationJointSource>>> outputs = BuildBoneOutputs(animation);

        if (!outputs) {
            return std::nullopt;
        }

        optional<vector<float32_t>> sample_times = BuildSampleTimes(animation, *outputs, reversed);

        if (!sample_times) {
            return std::nullopt;
        }

        vector<mat44> combined_transforms(_bones.size());
        vector<mat44> result;
        result.reserve(sample_times->size());

        for (float32_t sample_time : *sample_times) {
            if (!BuildCombinedTransforms(*outputs, sample_time, animation.Duration, reversed, combined_transforms)) {
                return std::nullopt;
            }

            result.emplace_back(combined_transforms[bone_it->second]);
        }

        return result;
    }
    catch (const ModelBoundsException&) {
        throw;
    }
    catch (const std::exception& ex) {
        throw ModelBoundsException("Invalid baked model data while sampling a link bone", ex.what());
    }
}

auto CalculateRigidAttachmentBounds(const_span<mat44> bone_transforms, const ModelBounds3D& attachment_bounds, const mat44& attachment_transform) -> optional<ModelBounds3D>
{
    FO_STACK_TRACE_ENTRY();

    if (!IsValidModelBounds(attachment_bounds) || !IsFinite(attachment_transform)) {
        return std::nullopt;
    }

    optional<ModelBounds3D> result;

    for (const mat44& bone_transform : bone_transforms) {
        if (!IncludeTransformedModelBounds(result, attachment_bounds, bone_transform * attachment_transform)) {
            return std::nullopt;
        }
    }

    return result;
}

void ModelBoundsSampler::AppendBone(const ModelMeshBoneData& bone, optional<size_t> parent)
{
    FO_STACK_TRACE_ENTRY();

    size_t bone_index = _bones.size();
    Bone& target = _bones.emplace_back();
    target.Name = bone.Name;
    target.BindTransform = bone.TransformationMatrix;
    target.Parent = parent;

    if (!IsFinite(target.BindTransform)) {
        throw ModelBoundsException("Baked model contains a non-finite bind transform");
    }

    if (bone.AttachedMesh) {
        const ModelMeshGeometryData& source = *bone.AttachedMesh;
        target.AttachedMesh = Mesh {};
        Mesh& mesh = *target.AttachedMesh;
        mesh.Vertices.reserve(source.Vertices.size());

        for (const ModelMeshVertexData& source_vertex : source.Vertices) {
            Vertex& vertex = mesh.Vertices.emplace_back();
            vertex.Position = source_vertex.Position;
            std::ranges::copy(source_vertex.BlendWeights, vertex.BlendWeights);
            std::ranges::copy(source_vertex.BlendIndices, vertex.BlendIndices);

            if (!IsFinite(vertex.Position)) {
                throw ModelBoundsException("Baked model contains a non-finite vertex position");
            }
        }

        mesh.Indices.assign(source.Indices.begin(), source.Indices.end());
        mesh.SkinBoneNames = source.SkinBoneNames;
        mesh.SkinBoneOffsets = source.SkinBoneOffsets;

        if (mesh.SkinBoneNames.size() != mesh.SkinBoneOffsets.size()) {
            throw ModelBoundsException("Skin bone count does not match inverse-bind offset count", mesh.SkinBoneNames.size(), mesh.SkinBoneOffsets.size());
        }

        for (const mat44& offset : mesh.SkinBoneOffsets) {
            if (!IsFinite(offset)) {
                throw ModelBoundsException("Baked model contains a non-finite inverse-bind offset");
            }
        }
    }

    for (const auto& child : bone.Children) {
        AppendBone(*child, bone_index);
    }
}

auto ModelBoundsSampler::BuildBoneIndex() -> bool
{
    FO_STACK_TRACE_ENTRY();

    _boneIndex.reserve(_bones.size());

    for (size_t i = 0; i < _bones.size(); i++) {
        if (!_boneIndex.emplace(_bones[i].Name, i).second) {
            return false;
        }
    }

    return true;
}

auto ModelBoundsSampler::BuildBindPoseTransforms() -> bool
{
    FO_STACK_TRACE_ENTRY();

    _bindPoseTransforms.resize(_bones.size());

    for (size_t i = 0; i < _bones.size(); i++) {
        if (_bones[i].Parent) {
            size_t parent = *_bones[i].Parent;
            FO_VERIFY_AND_THROW(parent < i, "Baked model hierarchy parent must precede its child", parent, i);
            _bindPoseTransforms[i] = _bindPoseTransforms[parent] * _bones[i].BindTransform;
        }
        else {
            _bindPoseTransforms[i] = _bones[i].BindTransform;
        }

        if (!IsFinite(_bindPoseTransforms[i])) {
            return false;
        }
    }

    return true;
}

auto ModelBoundsSampler::BuildDrawableMeshes(const vector<string>& disabled_meshes) -> bool
{
    FO_STACK_TRACE_ENTRY();

    constexpr float64_t weight_sum_tolerance = 0.001;
    bool all_meshes_disabled = std::ranges::find(disabled_meshes, string {}) != disabled_meshes.end();

    for (size_t bone_index = 0; bone_index < _bones.size(); bone_index++) {
        const Bone& owner_bone = _bones[bone_index];

        if (!owner_bone.AttachedMesh || owner_bone.AttachedMesh->Vertices.empty() || owner_bone.AttachedMesh->Indices.empty()) {
            continue;
        }
        if (all_meshes_disabled || std::ranges::find(disabled_meshes, owner_bone.Name) != disabled_meshes.end()) {
            continue;
        }

        const Mesh& mesh = *owner_bone.AttachedMesh;
        DrawableMesh& drawable_mesh = _drawableMeshes.emplace_back();
        drawable_mesh.OwnerBone = bone_index;

        vector<bool> referenced_vertices(mesh.Vertices.size());

        for (ModelMeshIndexData vertex_index : mesh.Indices) {
            if (numeric_cast<size_t>(vertex_index) >= mesh.Vertices.size()) {
                return false;
            }

            referenced_vertices[numeric_cast<size_t>(vertex_index)] = true;
        }

        for (size_t vertex_index = 0; vertex_index < referenced_vertices.size(); vertex_index++) {
            if (referenced_vertices[vertex_index]) {
                drawable_mesh.VertexIndices.emplace_back(vertex_index);
            }
        }

        if (drawable_mesh.VertexIndices.empty()) {
            _drawableMeshes.pop_back();
            continue;
        }

        if (!mesh.SkinBoneNames.empty()) {
            drawable_mesh.SkinBones.reserve(mesh.SkinBoneNames.size());

            for (const string& skin_bone_name : mesh.SkinBoneNames) {
                if (skin_bone_name.empty()) {
                    drawable_mesh.SkinBones.emplace_back(bone_index);
                    continue;
                }

                auto it = _boneIndex.find(skin_bone_name);

                if (it == _boneIndex.end()) {
                    return false;
                }

                drawable_mesh.SkinBones.emplace_back(it->second);
            }

            for (size_t vertex_index : drawable_mesh.VertexIndices) {
                const Vertex& vertex = mesh.Vertices[vertex_index];
                float64_t weight_sum = 0.0;
                bool has_influence = false;

                for (size_t influence = 0; influence < MODEL_MESH_BONES_PER_VERTEX; influence++) {
                    float32_t weight = vertex.BlendWeights[influence];
                    float32_t raw_index = vertex.BlendIndices[influence];

                    if (!std::isfinite(weight) || !std::isfinite(raw_index) || weight < 0.0f) {
                        return false;
                    }
                    if (weight == 0.0f) {
                        continue;
                    }

                    int32_t index = iround<int32_t>(raw_index);

                    if (index < 0 || numeric_cast<size_t>(index) >= drawable_mesh.SkinBones.size() || !is_float_equal(raw_index, numeric_cast<float32_t>(index))) {
                        return false;
                    }

                    weight_sum += weight;
                    has_influence = true;
                }

                if (!has_influence || std::abs(weight_sum - 1.0) > weight_sum_tolerance) {
                    return false;
                }
            }
        }

        if (_measurement == ModelBoundsMeasurement::PerBoneEnvelope) {
            BuildMeshEnvelopes(drawable_mesh);
        }
    }

    return !_drawableMeshes.empty();
}

// One envelope per skin slot, measured after the inverse-bind offset, so a posed slot needs only its own
// bone matrix. A blended vertex is a convex combination of its slots, hence inside the union of the boxes
void ModelBoundsSampler::BuildMeshEnvelopes(DrawableMesh& drawable_mesh) const
{
    FO_STACK_TRACE_ENTRY();

    const Mesh& mesh = *_bones[drawable_mesh.OwnerBone].AttachedMesh;

    if (drawable_mesh.SkinBones.empty()) {
        for (size_t vertex_index : drawable_mesh.VertexIndices) {
            (void)IncludeModelBoundsPoint(drawable_mesh.RigidEnvelope, mesh.Vertices[vertex_index].Position);
        }

        return;
    }

    drawable_mesh.SkinBoneEnvelopes.resize(drawable_mesh.SkinBones.size());

    for (size_t vertex_index : drawable_mesh.VertexIndices) {
        const Vertex& vertex = mesh.Vertices[vertex_index];

        for (size_t influence = 0; influence < MODEL_MESH_BONES_PER_VERTEX; influence++) {
            if (vertex.BlendWeights[influence] == 0.0f) {
                continue;
            }

            size_t skin_index = numeric_cast<size_t>(iround<int32_t>(vertex.BlendIndices[influence]));
            glm::vec4 offset_position = mesh.SkinBoneOffsets[skin_index] * glm::vec4 {vertex.Position, 1.0f};
            (void)IncludeModelBoundsPoint(drawable_mesh.SkinBoneEnvelopes[skin_index], vec3 {offset_position});
        }
    }
}

auto ModelBoundsSampler::BuildBoneOutputs(const ModelAnimationSource& animation) const -> optional<vector<nptr<const ModelAnimationJointSource>>>
{
    FO_STACK_TRACE_ENTRY();

    unordered_map<string, size_t> output_index;
    output_index.reserve(animation.Joints.size());

    for (size_t i = 0; i < animation.Joints.size(); i++) {
        if (!output_index.emplace(animation.Joints[i].OutputName, i).second) {
            return std::nullopt;
        }
    }

    vector<nptr<const ModelAnimationJointSource>> result(_bones.size());

    for (size_t i = 0; i < _bones.size(); i++) {
        if (auto it = output_index.find(_bones[i].Name); it != output_index.end()) {
            result[i] = &animation.Joints[it->second];
        }
    }

    return result;
}

auto ModelBoundsSampler::BuildSampleTimes(const ModelAnimationSource& animation, const vector<nptr<const ModelAnimationJointSource>>& outputs, bool reversed) const -> optional<vector<float32_t>>
{
    FO_STACK_TRACE_ENTRY();

    constexpr float64_t GRID_SAMPLES_PER_SECOND = 60.0;

    if (!std::isfinite(animation.Duration) || animation.Duration <= 0.0f) {
        throw ModelBoundsException("Animation has invalid duration", animation.Name, animation.Duration);
    }

    vector<float32_t> result;

    for (nptr<const ModelAnimationJointSource> output : outputs) {
        if (!output) {
            continue;
        }
        if (!ValidateJointTracks(*output)) {
            return std::nullopt;
        }

        AppendTrackSampleTimes(output->Scale.Times, animation.Duration, reversed, result);
        AppendTrackSampleTimes(output->Rotation.Times, animation.Duration, reversed, result);
        AppendTrackSampleTimes(output->Translation.Times, animation.Duration, reversed, result);
    }

    float64_t interval_count_value = std::ceil(numeric_cast<float64_t>(animation.Duration) * GRID_SAMPLES_PER_SECOND);
    size_t interval_count = std::max<size_t>(1, iround<size_t>(interval_count_value));
    result.reserve(result.size() + interval_count + 1);

    for (size_t i = 0; i <= interval_count; i++) {
        float64_t factor = numeric_cast<float64_t>(i) / numeric_cast<float64_t>(interval_count);
        result.emplace_back(numeric_cast<float32_t>(numeric_cast<float64_t>(animation.Duration) * factor));
    }

    std::ranges::sort(result);
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

auto ModelBoundsSampler::BuildCombinedTransforms(const vector<nptr<const ModelAnimationJointSource>>& outputs, float32_t time, float32_t duration, bool reversed, vector<mat44>& combined_transforms) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(outputs.size() == _bones.size(), "Animation output mapping size does not match model hierarchy");
    FO_VERIFY_AND_THROW(combined_transforms.size() == _bones.size(), "Combined transform buffer size does not match model hierarchy");

    for (size_t i = 0; i < _bones.size(); i++) {
        mat44 local_transform = outputs[i] ? SampleJointTransform(*outputs[i], time, duration, reversed) : _bones[i].BindTransform;

        if (_bones[i].Parent) {
            size_t parent = *_bones[i].Parent;
            FO_VERIFY_AND_THROW(parent < i, "Baked model hierarchy parent must precede its child", parent, i);
            combined_transforms[i] = combined_transforms[parent] * local_transform;
        }
        else {
            combined_transforms[i] = local_transform;
        }

        if (!IsFinite(combined_transforms[i])) {
            return false;
        }
    }

    return true;
}

auto ModelBoundsSampler::IncludePosedGeometry(const vector<mat44>& combined_transforms, optional<ModelBounds3D>& bounds) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    for (const DrawableMesh& drawable_mesh : _drawableMeshes) {
        const Mesh& mesh = *_bones[drawable_mesh.OwnerBone].AttachedMesh;

        if (drawable_mesh.OwnerBone >= combined_transforms.size() || drawable_mesh.SkinBones.size() != mesh.SkinBoneOffsets.size()) {
            return false;
        }

        if (_measurement == ModelBoundsMeasurement::PerBoneEnvelope) {
            if (drawable_mesh.SkinBones.empty()) {
                if (!drawable_mesh.RigidEnvelope || !IncludeTransformedModelBounds(bounds, *drawable_mesh.RigidEnvelope, combined_transforms[drawable_mesh.OwnerBone])) {
                    return false;
                }

                continue;
            }

            for (size_t skin_index = 0; skin_index < drawable_mesh.SkinBones.size(); skin_index++) {
                if (!drawable_mesh.SkinBoneEnvelopes[skin_index]) {
                    continue;
                }
                if (drawable_mesh.SkinBones[skin_index] >= combined_transforms.size()) {
                    return false;
                }
                if (!IncludeTransformedModelBounds(bounds, *drawable_mesh.SkinBoneEnvelopes[skin_index], combined_transforms[drawable_mesh.SkinBones[skin_index]])) {
                    return false;
                }
            }

            continue;
        }

        for (size_t vertex_index : drawable_mesh.VertexIndices) {
            const Vertex& vertex = mesh.Vertices[vertex_index];
            glm::vec4 transformed {};

            if (drawable_mesh.SkinBones.empty()) {
                transformed = combined_transforms[drawable_mesh.OwnerBone] * glm::vec4 {vertex.Position, 1.0f};
            }
            else {
                for (size_t influence = 0; influence < MODEL_MESH_BONES_PER_VERTEX; influence++) {
                    float32_t weight = vertex.BlendWeights[influence];

                    if (weight == 0.0f) {
                        continue;
                    }

                    size_t skin_index = numeric_cast<size_t>(iround<int32_t>(vertex.BlendIndices[influence]));

                    if (skin_index >= drawable_mesh.SkinBones.size() || drawable_mesh.SkinBones[skin_index] >= combined_transforms.size()) {
                        return false;
                    }

                    transformed += combined_transforms[drawable_mesh.SkinBones[skin_index]] * mesh.SkinBoneOffsets[skin_index] * glm::vec4 {vertex.Position, 1.0f} * weight;
                }
            }

            if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) || !std::isfinite(transformed.z) || !is_float_equal(transformed.w, 1.0f)) {
                return false;
            }

            if (!IncludeModelBoundsPoint(bounds, vec3 {transformed})) {
                return false;
            }
        }
    }

    return bounds.has_value();
}

static auto ValidateJointTracks(const ModelAnimationJointSource& joint) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto validate_track = [](const auto& times, const auto& values) {
        if (times.size() != values.size()) {
            return false;
        }
        if (times.empty()) {
            return false;
        }

        for (size_t i = 0; i < times.size(); i++) {
            if (!std::isfinite(times[i]) || !IsFinite(values[i])) {
                return false;
            }
            if (i != 0 && times[i] < times[i - 1]) {
                return false;
            }
        }

        return true;
    };

    return validate_track(joint.Scale.Times, joint.Scale.Values) && validate_track(joint.Rotation.Times, joint.Rotation.Values) && validate_track(joint.Translation.Times, joint.Translation.Values);
}

static void AppendTrackSampleTimes(const vector<float32_t>& times, float32_t duration, bool reversed, vector<float32_t>& sample_times)
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < times.size(); i++) {
        float32_t sample_time = std::clamp(reversed ? duration - times[i] : times[i], 0.0f, duration);
        sample_times.emplace_back(sample_time);

        // A reversed clip reads a key from the span ending on it, so the pose just before the key is one the
        // key time itself never produces
        if (reversed && sample_time > 0.0f) {
            sample_times.emplace_back(std::max(0.0f, std::nextafter(sample_time, std::numeric_limits<float32_t>::lowest())));
        }

        if (i != 0) {
            float64_t midpoint = (numeric_cast<float64_t>(times[i - 1]) + numeric_cast<float64_t>(times[i])) * 0.5;
            float32_t midpoint_time = numeric_cast<float32_t>(midpoint);
            sample_times.emplace_back(std::clamp(reversed ? duration - midpoint_time : midpoint_time, 0.0f, duration));
        }
    }
}

static auto SampleJointTransform(const ModelAnimationJointSource& joint, float32_t time, float32_t duration, bool reversed) -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    vec3 scale = SampleVectorTrack(time, duration, reversed, joint.Scale);
    quaternion rotation = SampleRotationTrack(time, duration, reversed, joint.Rotation);
    vec3 translation = SampleVectorTrack(time, duration, reversed, joint.Translation);
    return glm::translate(mat44 {1.0f}, translation) * glm::mat4_cast(rotation) * glm::scale(mat44 {1.0f}, scale);
}

static auto SampleVectorTrack(float32_t time, float32_t duration, bool reversed, const ModelAnimationVec3Track& track) -> vec3
{
    FO_NO_STACK_TRACE_ENTRY();

    if (reversed) {
        float32_t reversed_time = duration - time;
        optional<size_t> span = FindReversedKeySpan(track.Times, reversed_time);

        if (!span) {
            return track.Values.front();
        }

        size_t index = *span;
        vec3 result = track.Values[index];
        float32_t factor = (reversed_time - track.Times[index]) / (track.Times[index] - track.Times[index - 1]);
        result.x += (track.Values[index - 1].x - result.x) * factor;
        result.y += (track.Values[index - 1].y - result.y) * factor;
        result.z += (track.Values[index - 1].z - result.z) * factor;
        return result;
    }

    optional<size_t> span = FindForwardKeySpan(track.Times, time);

    if (!span) {
        return track.Values.back();
    }

    size_t index = *span;
    vec3 result = track.Values[index];
    float32_t factor = (time - track.Times[index]) / (track.Times[index + 1] - track.Times[index]);
    result.x += (track.Values[index + 1].x - result.x) * factor;
    result.y += (track.Values[index + 1].y - result.y) * factor;
    result.z += (track.Values[index + 1].z - result.z) * factor;
    return result;
}

static auto SampleRotationTrack(float32_t time, float32_t duration, bool reversed, const ModelAnimationQuaternionTrack& track) -> quaternion
{
    FO_NO_STACK_TRACE_ENTRY();

    if (reversed) {
        float32_t reversed_time = duration - time;
        optional<size_t> span = FindReversedKeySpan(track.Times, reversed_time);

        if (!span) {
            return track.Values.front();
        }

        size_t index = *span;
        float32_t factor = (reversed_time - track.Times[index]) / (track.Times[index] - track.Times[index - 1]);
        return glm::normalize(glm::slerp(track.Values[index], track.Values[index - 1], factor));
    }

    optional<size_t> span = FindForwardKeySpan(track.Times, time);

    if (!span) {
        return track.Values.back();
    }

    size_t index = *span;
    float32_t factor = (time - track.Times[index]) / (track.Times[index + 1] - track.Times[index]);
    return glm::normalize(glm::slerp(track.Values[index], track.Values[index + 1], factor));
}

// Key times are non-decreasing, so the span holding the sample is the one ending at the first key past it.
// Absent means the sample sits outside the authored span range and the caller clamps to an end key
static auto FindForwardKeySpan(const vector<float32_t>& times, float32_t time) -> optional<size_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto upper = std::ranges::upper_bound(times, time);

    if (upper == times.begin() || upper == times.end()) {
        return std::nullopt;
    }

    return numeric_cast<size_t>(std::distance(times.begin(), upper)) - 1;
}

// The reversed pass reads a span by its end key, so it anchors on the first key at or after the sample
static auto FindReversedKeySpan(const vector<float32_t>& times, float32_t time) -> optional<size_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto lower = std::ranges::lower_bound(times, time);

    if (lower == times.begin() || lower == times.end()) {
        return std::nullopt;
    }

    return numeric_cast<size_t>(std::distance(times.begin(), lower));
}

static auto IsFinite(const vec3& value) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

static auto IsFinite(const quaternion& value) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
}

static auto IsFinite(const mat44& value) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<const float32_t> values = glm::value_ptr(value);

    for (size_t i = 0; i < 16; i++) {
        if (!std::isfinite(values[i])) {
            return false;
        }
    }

    return true;
}

FO_END_NAMESPACE

#endif
