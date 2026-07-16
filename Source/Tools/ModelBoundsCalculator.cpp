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

#include "ModelBoundsCalculator.h"
#include "Rendering.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

struct BoundsMesh
{
    size_t OwnerBone {};
    vector<Vertex3D> Vertices {};
    vector<vindex_t> Indices {};
    vector<string> SkinBoneNames {};
    vector<mat44> SkinBoneOffsets {};
};

struct BoundsDrawableMesh
{
    nptr<const BoundsMesh> Mesh {};
    vector<size_t> VertexIndices {};
    vector<size_t> SkinBones {};
};

struct BoundsBone
{
    string Name {};
    mat44 BindTransform {};
    optional<size_t> Parent {};
    optional<BoundsMesh> Mesh {};
};

struct BoundsAnimationOutput
{
    string BoneName {};
    vector<float32_t> ScaleTimes {};
    vector<vec3> ScaleValues {};
    vector<float32_t> RotationTimes {};
    vector<quaternion> RotationValues {};
    vector<float32_t> TranslationTimes {};
    vector<vec3> TranslationValues {};
};

struct BoundsAnimation
{
    string Name {};
    float32_t Duration {};
    vector<BoundsAnimationOutput> Outputs {};
};

struct BoundsModel
{
    vector<BoundsBone> Bones {};
    vector<BoundsAnimation> Animations {};
};

static auto ReadBoundsModel(const_span<uint8_t> data) -> BoundsModel;
static auto ReadBoundsBone(DataReader& reader, BoundsModel& model, optional<size_t> parent) -> size_t;
static auto ReadBoundsMesh(DataReader& reader, size_t owner_bone) -> BoundsMesh;
static auto ReadBoundsAnimation(DataReader& reader) -> BoundsAnimation;
static auto FindBoundsAnimation(const BoundsModel& model, string_view animation_name) -> nptr<const BoundsAnimation>;
static auto BuildBoneIndex(const BoundsModel& model) -> optional<unordered_map<string, size_t>>;
static auto BuildAnimationOutputIndex(const BoundsAnimation& animation) -> optional<unordered_map<string, size_t>>;
static auto BuildDrawableMeshes(const BoundsModel& model, const unordered_map<string, size_t>& bone_index, const vector<string>& disabled_meshes) -> optional<vector<BoundsDrawableMesh>>;
static auto BuildBoneAnimationOutputs(const BoundsModel& model, const BoundsAnimation& animation, const unordered_map<string, size_t>& output_index) -> vector<nptr<const BoundsAnimationOutput>>;
static auto BuildAnimationSampleTimes(const BoundsAnimation& animation, const vector<nptr<const BoundsAnimationOutput>>& outputs, bool reversed) -> optional<vector<float32_t>>;
static auto ValidateAnimationOutput(const BoundsAnimationOutput& output) -> bool;
static void AppendTrackSampleTimes(const vector<float32_t>& times, float32_t duration, bool reversed, vector<float32_t>& sample_times);
static auto SampleAnimationOutput(const BoundsAnimationOutput& output, float32_t time, float32_t duration, bool reversed) -> mat44;
static auto SampleVectorTrack(float32_t time, float32_t duration, bool reversed, const vector<float32_t>& times, const vector<vec3>& values) -> vec3;
static auto SampleRotationTrack(float32_t time, float32_t duration, bool reversed, const vector<float32_t>& times, const vector<quaternion>& values) -> quaternion;
static auto BuildCombinedTransforms(const BoundsModel& model, const vector<nptr<const BoundsAnimationOutput>>& outputs, float32_t time, float32_t duration, bool reversed, vector<mat44>& combined_transforms) -> bool;
static auto IncludeTransformedGeometry(const vector<BoundsDrawableMesh>& drawable_meshes, const vector<mat44>& combined_transforms, optional<ModelBounds3D>& bounds) -> bool;
static auto IsFinite(const vec3& value) -> bool;
static auto IsFinite(const quaternion& value) -> bool;
static auto IsFinite(const mat44& value) -> bool;

auto CalculateModelStaticBounds(const_span<uint8_t> model_data, const vector<string>& disabled_meshes) -> optional<ModelBounds3D>
{
    FO_STACK_TRACE_ENTRY();

    try {
        const BoundsModel model = ReadBoundsModel(model_data);
        const optional<unordered_map<string, size_t>> bone_index = BuildBoneIndex(model);

        if (!bone_index) {
            return std::nullopt;
        }

        const optional<vector<BoundsDrawableMesh>> drawable_meshes = BuildDrawableMeshes(model, *bone_index, disabled_meshes);

        if (!drawable_meshes) {
            return std::nullopt;
        }

        vector<mat44> combined_transforms(model.Bones.size());
        optional<ModelBounds3D> result;

        for (size_t i = 0; i < model.Bones.size(); i++) {
            if (model.Bones[i].Parent) {
                const size_t parent = *model.Bones[i].Parent;
                FO_VERIFY_AND_THROW(parent < i, "Baked model hierarchy parent must precede its child", parent, i);
                combined_transforms[i] = combined_transforms[parent] * model.Bones[i].BindTransform;
            }
            else {
                combined_transforms[i] = model.Bones[i].BindTransform;
            }

            if (!IsFinite(combined_transforms[i])) {
                return std::nullopt;
            }
        }

        if (!IncludeTransformedGeometry(*drawable_meshes, combined_transforms, result)) {
            return std::nullopt;
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
        throw ModelBoundsException(strex("Invalid baked model data while calculating static bounds: {}", ex.what()));
    }
}

auto CalculateModelAnimationBounds(const_span<uint8_t> model_data, const_span<uint8_t> animation_data, string_view animation_name, bool reversed, const vector<string>& disabled_meshes) -> optional<ModelBounds3D>
{
    FO_STACK_TRACE_ENTRY();

    try {
        const BoundsModel model = ReadBoundsModel(model_data);
        const BoundsModel animation_model = ReadBoundsModel(animation_data);
        const auto animation = FindBoundsAnimation(animation_model, animation_name);

        if (!animation) {
            throw ModelBoundsException(strex("Animation '{}' was not found in baked model data", animation_name));
        }

        const optional<unordered_map<string, size_t>> bone_index = BuildBoneIndex(model);
        const optional<unordered_map<string, size_t>> output_index = BuildAnimationOutputIndex(*animation);

        if (!bone_index || !output_index) {
            return std::nullopt;
        }

        const optional<vector<BoundsDrawableMesh>> drawable_meshes = BuildDrawableMeshes(model, *bone_index, disabled_meshes);

        if (!drawable_meshes) {
            return std::nullopt;
        }

        const vector<nptr<const BoundsAnimationOutput>> outputs = BuildBoneAnimationOutputs(model, *animation, *output_index);
        const optional<vector<float32_t>> sample_times = BuildAnimationSampleTimes(*animation, outputs, reversed);

        if (!sample_times) {
            return std::nullopt;
        }

        vector<mat44> combined_transforms(model.Bones.size());
        optional<ModelBounds3D> result;

        for (const float32_t sample_time : *sample_times) {
            if (!BuildCombinedTransforms(model, outputs, sample_time, animation->Duration, reversed, combined_transforms)) {
                return std::nullopt;
            }

            if (!IncludeTransformedGeometry(*drawable_meshes, combined_transforms, result)) {
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
        throw ModelBoundsException(strex("Invalid baked model data while calculating animation bounds: {}", ex.what()));
    }
}

static auto ReadBoundsModel(const_span<uint8_t> data) -> BoundsModel
{
    FO_STACK_TRACE_ENTRY();

    auto reader = DataReader(data);
    BoundsModel model;
    (void)ReadBoundsBone(reader, model, {});

    const uint32_t animation_count = reader.Read<uint32_t>();
    model.Animations.reserve(animation_count);

    for (uint32_t i = 0; i < animation_count; i++) {
        model.Animations.emplace_back(ReadBoundsAnimation(reader));
    }

    reader.VerifyEnd();
    return model;
}

static auto ReadBoundsBone(DataReader& reader, BoundsModel& model, optional<size_t> parent) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    const size_t bone_index = model.Bones.size();
    model.Bones.emplace_back();
    model.Bones[bone_index].Name = reader.ReadString();
    model.Bones[bone_index].BindTransform = reader.Read<mat44>();
    (void)reader.Read<mat44>(); // Global bind transform is not used by the runtime skinning path.
    model.Bones[bone_index].Parent = parent;

    if (!IsFinite(model.Bones[bone_index].BindTransform)) {
        throw ModelBoundsException("Baked model contains a non-finite bind transform");
    }

    if (reader.Read<uint8_t>() != 0) {
        model.Bones[bone_index].Mesh.emplace(ReadBoundsMesh(reader, bone_index));
    }

    const uint32_t child_count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < child_count; i++) {
        (void)ReadBoundsBone(reader, model, bone_index);
    }

    return bone_index;
}

static auto ReadBoundsMesh(DataReader& reader, size_t owner_bone) -> BoundsMesh
{
    FO_STACK_TRACE_ENTRY();

    BoundsMesh mesh;
    mesh.OwnerBone = owner_bone;

    const uint32_t vertex_count = reader.Read<uint32_t>();
    mesh.Vertices.resize(vertex_count);
    reader.ReadObjectArray(span {mesh.Vertices});

    const uint32_t index_count = reader.Read<uint32_t>();
    mesh.Indices.resize(index_count);
    reader.ReadObjectArray(span {mesh.Indices});
    (void)reader.ReadString();

    const uint32_t skin_bone_count = reader.Read<uint32_t>();
    mesh.SkinBoneNames.reserve(skin_bone_count);

    for (uint32_t i = 0; i < skin_bone_count; i++) {
        mesh.SkinBoneNames.emplace_back(reader.ReadString());
    }

    const uint32_t skin_offset_count = reader.Read<uint32_t>();

    if (skin_offset_count != skin_bone_count) {
        throw ModelBoundsException(strex("Skin bone count {} does not match inverse-bind offset count {}", skin_bone_count, skin_offset_count));
    }

    mesh.SkinBoneOffsets.resize(skin_offset_count);
    reader.ReadObjectArray(span {mesh.SkinBoneOffsets});

    for (const Vertex3D& vertex : mesh.Vertices) {
        if (!IsFinite(vertex.Position)) {
            throw ModelBoundsException("Baked model contains a non-finite vertex position");
        }
    }
    for (const mat44& offset : mesh.SkinBoneOffsets) {
        if (!IsFinite(offset)) {
            throw ModelBoundsException("Baked model contains a non-finite inverse-bind offset");
        }
    }

    return mesh;
}

static auto ReadBoundsAnimation(DataReader& reader) -> BoundsAnimation
{
    FO_STACK_TRACE_ENTRY();

    (void)reader.ReadString();

    BoundsAnimation animation;
    animation.Name = reader.ReadString();
    animation.Duration = reader.Read<float32_t>();

    const uint32_t hierarchy_count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < hierarchy_count; i++) {
        const uint32_t bone_count = reader.Read<uint32_t>();

        for (uint32_t j = 0; j < bone_count; j++) {
            (void)reader.ReadString();
        }
    }

    const uint32_t output_count = reader.Read<uint32_t>();
    animation.Outputs.reserve(output_count);

    for (uint32_t i = 0; i < output_count; i++) {
        BoundsAnimationOutput& output = animation.Outputs.emplace_back();
        output.BoneName = reader.ReadString();

        uint32_t count = reader.Read<uint32_t>();
        output.ScaleTimes.resize(count);
        output.ScaleValues.resize(count);
        reader.ReadObjectArray(span {output.ScaleTimes});
        reader.ReadObjectArray(span {output.ScaleValues});

        count = reader.Read<uint32_t>();
        output.RotationTimes.resize(count);
        output.RotationValues.resize(count);
        reader.ReadObjectArray(span {output.RotationTimes});
        reader.ReadObjectArray(span {output.RotationValues});

        count = reader.Read<uint32_t>();
        output.TranslationTimes.resize(count);
        output.TranslationValues.resize(count);
        reader.ReadObjectArray(span {output.TranslationTimes});
        reader.ReadObjectArray(span {output.TranslationValues});
    }

    return animation;
}

static auto FindBoundsAnimation(const BoundsModel& model, string_view animation_name) -> nptr<const BoundsAnimation>
{
    FO_STACK_TRACE_ENTRY();

    if (animation_name == "Base") {
        return !model.Animations.empty() ? &model.Animations.front() : nullptr;
    }

    for (const BoundsAnimation& animation : model.Animations) {
        if (strex(animation.Name).compare_ignore_case(animation_name)) {
            return &animation;
        }
    }

    return nullptr;
}

static auto BuildBoneIndex(const BoundsModel& model) -> optional<unordered_map<string, size_t>>
{
    FO_STACK_TRACE_ENTRY();

    unordered_map<string, size_t> result;
    result.reserve(model.Bones.size());

    for (size_t i = 0; i < model.Bones.size(); i++) {
        if (!result.emplace(model.Bones[i].Name, i).second) {
            return std::nullopt;
        }
    }

    return result;
}

static auto BuildAnimationOutputIndex(const BoundsAnimation& animation) -> optional<unordered_map<string, size_t>>
{
    FO_STACK_TRACE_ENTRY();

    unordered_map<string, size_t> result;
    result.reserve(animation.Outputs.size());

    for (size_t i = 0; i < animation.Outputs.size(); i++) {
        if (!result.emplace(animation.Outputs[i].BoneName, i).second) {
            return std::nullopt;
        }
    }

    return result;
}

static auto BuildDrawableMeshes(const BoundsModel& model, const unordered_map<string, size_t>& bone_index, const vector<string>& disabled_meshes) -> optional<vector<BoundsDrawableMesh>>
{
    FO_STACK_TRACE_ENTRY();

    constexpr float64_t weight_sum_tolerance = 0.001;
    vector<BoundsDrawableMesh> result;
    const bool all_meshes_disabled = std::ranges::find(disabled_meshes, string {}) != disabled_meshes.end();

    for (const BoundsBone& owner_bone : model.Bones) {
        if (!owner_bone.Mesh || owner_bone.Mesh->Vertices.empty() || owner_bone.Mesh->Indices.empty()) {
            continue;
        }
        if (all_meshes_disabled || std::ranges::find(disabled_meshes, owner_bone.Name) != disabled_meshes.end()) {
            continue;
        }

        const BoundsMesh& mesh = *owner_bone.Mesh;
        BoundsDrawableMesh& drawable_mesh = result.emplace_back();
        drawable_mesh.Mesh = &mesh;

        vector<bool> referenced_vertices(mesh.Vertices.size());

        for (const vindex_t vertex_index : mesh.Indices) {
            if (numeric_cast<size_t>(vertex_index) >= mesh.Vertices.size()) {
                return std::nullopt;
            }

            referenced_vertices[numeric_cast<size_t>(vertex_index)] = true;
        }

        for (size_t vertex_index = 0; vertex_index < referenced_vertices.size(); vertex_index++) {
            if (referenced_vertices[vertex_index]) {
                drawable_mesh.VertexIndices.emplace_back(vertex_index);
            }
        }

        if (drawable_mesh.VertexIndices.empty()) {
            result.pop_back();
            continue;
        }

        if (mesh.SkinBoneNames.empty()) {
            continue;
        }

        if (mesh.SkinBoneNames.size() != mesh.SkinBoneOffsets.size()) {
            return std::nullopt;
        }

        drawable_mesh.SkinBones.reserve(mesh.SkinBoneNames.size());

        for (const string& skin_bone_name : mesh.SkinBoneNames) {
            if (skin_bone_name.empty()) {
                drawable_mesh.SkinBones.emplace_back(mesh.OwnerBone);
                continue;
            }

            const auto it = bone_index.find(skin_bone_name);

            if (it == bone_index.end()) {
                return std::nullopt;
            }

            drawable_mesh.SkinBones.emplace_back(it->second);
        }

        for (const size_t vertex_index : drawable_mesh.VertexIndices) {
            const Vertex3D& vertex = mesh.Vertices[vertex_index];
            float64_t weight_sum = 0.0;
            bool has_influence = false;

            for (size_t influence = 0; influence < MODEL_BONES_PER_VERTEX; influence++) {
                const float32_t weight = vertex.BlendWeights[influence];
                const float32_t raw_index = vertex.BlendIndices[influence];

                if (!std::isfinite(weight) || !std::isfinite(raw_index) || weight < 0.0f) {
                    return std::nullopt;
                }
                if (weight == 0.0f) {
                    continue;
                }

                const int32_t index = iround<int32_t>(raw_index);

                if (index < 0 || numeric_cast<size_t>(index) >= drawable_mesh.SkinBones.size() || !is_float_equal(raw_index, numeric_cast<float32_t>(index))) {
                    return std::nullopt;
                }

                weight_sum += weight;
                has_influence = true;
            }

            if (!has_influence || std::abs(weight_sum - 1.0) > weight_sum_tolerance) {
                return std::nullopt;
            }
        }
    }

    if (result.empty()) {
        return std::nullopt;
    }

    return result;
}

static auto BuildBoneAnimationOutputs(const BoundsModel& model, const BoundsAnimation& animation, const unordered_map<string, size_t>& output_index) -> vector<nptr<const BoundsAnimationOutput>>
{
    FO_STACK_TRACE_ENTRY();

    vector<nptr<const BoundsAnimationOutput>> result(model.Bones.size());

    for (size_t i = 0; i < model.Bones.size(); i++) {
        if (const auto it = output_index.find(model.Bones[i].Name); it != output_index.end()) {
            result[i] = &animation.Outputs[it->second];
        }
    }

    return result;
}

static auto BuildAnimationSampleTimes(const BoundsAnimation& animation, const vector<nptr<const BoundsAnimationOutput>>& outputs, bool reversed) -> optional<vector<float32_t>>
{
    FO_STACK_TRACE_ENTRY();

    constexpr float64_t samples_per_second = 60.0;

    if (!std::isfinite(animation.Duration) || animation.Duration <= 0.0f) {
        throw ModelBoundsException(strex("Animation '{}' has invalid duration {}", animation.Name, animation.Duration));
    }

    vector<float32_t> result;

    for (const nptr<const BoundsAnimationOutput> output : outputs) {
        if (!output) {
            continue;
        }
        if (!ValidateAnimationOutput(*output)) {
            return std::nullopt;
        }

        AppendTrackSampleTimes(output->ScaleTimes, animation.Duration, reversed, result);
        AppendTrackSampleTimes(output->RotationTimes, animation.Duration, reversed, result);
        AppendTrackSampleTimes(output->TranslationTimes, animation.Duration, reversed, result);
    }

    const float64_t interval_count_value = std::ceil(numeric_cast<float64_t>(animation.Duration) * samples_per_second);
    const size_t interval_count = std::max<size_t>(1, iround<size_t>(interval_count_value));
    result.reserve(result.size() + interval_count + 1);

    for (size_t i = 0; i <= interval_count; i++) {
        const float64_t factor = numeric_cast<float64_t>(i) / numeric_cast<float64_t>(interval_count);
        result.emplace_back(numeric_cast<float32_t>(numeric_cast<float64_t>(animation.Duration) * factor));
    }

    std::ranges::sort(result);
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

static auto ValidateAnimationOutput(const BoundsAnimationOutput& output) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto validate_track = [](const auto& times, const auto& values) {
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

    return validate_track(output.ScaleTimes, output.ScaleValues) && validate_track(output.RotationTimes, output.RotationValues) && validate_track(output.TranslationTimes, output.TranslationValues);
}

static void AppendTrackSampleTimes(const vector<float32_t>& times, float32_t duration, bool reversed, vector<float32_t>& sample_times)
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < times.size(); i++) {
        const float32_t sample_time = std::clamp(reversed ? duration - times[i] : times[i], 0.0f, duration);
        sample_times.emplace_back(sample_time);

        if (reversed && sample_time > 0.0f) {
            sample_times.emplace_back(std::max(0.0f, std::nextafter(sample_time, std::numeric_limits<float32_t>::lowest())));
        }

        if (i != 0) {
            const float64_t midpoint = (numeric_cast<float64_t>(times[i - 1]) + numeric_cast<float64_t>(times[i])) * 0.5;
            const float32_t midpoint_time = numeric_cast<float32_t>(midpoint);
            sample_times.emplace_back(std::clamp(reversed ? duration - midpoint_time : midpoint_time, 0.0f, duration));
        }
    }
}

static auto SampleAnimationOutput(const BoundsAnimationOutput& output, float32_t time, float32_t duration, bool reversed) -> mat44
{
    FO_STACK_TRACE_ENTRY();

    const vec3 scale = SampleVectorTrack(time, duration, reversed, output.ScaleTimes, output.ScaleValues);
    const quaternion rotation = SampleRotationTrack(time, duration, reversed, output.RotationTimes, output.RotationValues);
    const vec3 translation = SampleVectorTrack(time, duration, reversed, output.TranslationTimes, output.TranslationValues);
    return glm::translate(mat44 {1.0f}, translation) * glm::mat4_cast(rotation) * glm::scale(mat44 {1.0f}, scale);
}

static auto SampleVectorTrack(float32_t time, float32_t duration, bool reversed, const vector<float32_t>& times, const vector<vec3>& values) -> vec3
{
    FO_STACK_TRACE_ENTRY();

    if (reversed) {
        const float32_t reversed_time = duration - time;

        for (int32_t i = numeric_cast<int32_t>(times.size() - 1); i >= 0; i--) {
            if (i >= 1) {
                const size_t index = numeric_cast<size_t>(i);

                if (reversed_time <= times[index] && reversed_time > times[index - 1]) {
                    vec3 result = values[index];
                    const float32_t factor = (reversed_time - times[index]) / (times[index] - times[index - 1]);
                    result.x += (values[index - 1].x - result.x) * factor;
                    result.y += (values[index - 1].y - result.y) * factor;
                    result.z += (values[index - 1].z - result.z) * factor;
                    return result;
                }
            }
            else {
                return values[0];
            }
        }

        throw ModelBoundsException("Reversed animation vector track sampling failed");
    }

    for (size_t i = 0; i < times.size(); i++) {
        if (i + 1 < times.size()) {
            if (time >= times[i] && time < times[i + 1]) {
                vec3 result = values[i];
                const float32_t factor = (time - times[i]) / (times[i + 1] - times[i]);
                result.x += (values[i + 1].x - result.x) * factor;
                result.y += (values[i + 1].y - result.y) * factor;
                result.z += (values[i + 1].z - result.z) * factor;
                return result;
            }
        }
        else {
            return values[i];
        }
    }

    throw ModelBoundsException("Animation vector track sampling failed");
}

static auto SampleRotationTrack(float32_t time, float32_t duration, bool reversed, const vector<float32_t>& times, const vector<quaternion>& values) -> quaternion
{
    FO_STACK_TRACE_ENTRY();

    if (reversed) {
        const float32_t reversed_time = duration - time;

        for (int32_t i = numeric_cast<int32_t>(times.size() - 1); i >= 0; i--) {
            if (i >= 1) {
                const size_t index = numeric_cast<size_t>(i);

                if (reversed_time <= times[index] && reversed_time > times[index - 1]) {
                    const float32_t factor = (reversed_time - times[index]) / (times[index] - times[index - 1]);
                    return glm::normalize(glm::slerp(values[index], values[index - 1], factor));
                }
            }
            else {
                return values[0];
            }
        }

        throw ModelBoundsException("Reversed animation rotation track sampling failed");
    }

    for (size_t i = 0; i < times.size(); i++) {
        if (i + 1 < times.size()) {
            if (time >= times[i] && time < times[i + 1]) {
                const float32_t factor = (time - times[i]) / (times[i + 1] - times[i]);
                return glm::normalize(glm::slerp(values[i], values[i + 1], factor));
            }
        }
        else {
            return values[i];
        }
    }

    throw ModelBoundsException("Animation rotation track sampling failed");
}

static auto BuildCombinedTransforms(const BoundsModel& model, const vector<nptr<const BoundsAnimationOutput>>& outputs, float32_t time, float32_t duration, bool reversed, vector<mat44>& combined_transforms) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(outputs.size() == model.Bones.size(), "Animation output mapping size does not match model hierarchy");
    FO_VERIFY_AND_THROW(combined_transforms.size() == model.Bones.size(), "Combined transform buffer size does not match model hierarchy");

    for (size_t i = 0; i < model.Bones.size(); i++) {
        const mat44 local_transform = outputs[i] ? SampleAnimationOutput(*outputs[i], time, duration, reversed) : model.Bones[i].BindTransform;

        if (model.Bones[i].Parent) {
            const size_t parent = *model.Bones[i].Parent;
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

static auto IncludeTransformedGeometry(const vector<BoundsDrawableMesh>& drawable_meshes, const vector<mat44>& combined_transforms, optional<ModelBounds3D>& bounds) -> bool
{
    FO_STACK_TRACE_ENTRY();

    for (const BoundsDrawableMesh& drawable_mesh : drawable_meshes) {
        FO_VERIFY_AND_THROW(drawable_mesh.Mesh, "Drawable bounds mesh is missing its source mesh");
        const BoundsMesh& mesh = *drawable_mesh.Mesh;

        if (mesh.OwnerBone >= combined_transforms.size() || drawable_mesh.SkinBones.size() != mesh.SkinBoneOffsets.size()) {
            return false;
        }

        for (const size_t vertex_index : drawable_mesh.VertexIndices) {
            if (vertex_index >= mesh.Vertices.size()) {
                return false;
            }

            const Vertex3D& vertex = mesh.Vertices[vertex_index];
            glm::vec4 transformed {};

            if (drawable_mesh.SkinBones.empty()) {
                transformed = combined_transforms[mesh.OwnerBone] * glm::vec4 {vertex.Position, 1.0f};
            }
            else {
                for (size_t influence = 0; influence < MODEL_BONES_PER_VERTEX; influence++) {
                    const float32_t weight = vertex.BlendWeights[influence];

                    if (weight == 0.0f) {
                        continue;
                    }

                    const size_t skin_index = numeric_cast<size_t>(iround<int32_t>(vertex.BlendIndices[influence]));

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

    const ptr<const float32_t> values = glm::value_ptr(value);

    for (size_t i = 0; i < 16; i++) {
        if (!std::isfinite(values[i])) {
            return false;
        }
    }

    return true;
}

FO_END_NAMESPACE

#endif
