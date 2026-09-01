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

#include "catch_amalgamated.hpp"

#include "Common.h"

#if FO_ENABLE_3D

#include "ModelAnimation.h"
#include "ModelAnimationData.h"
#include "ModelBoundsCalculator.h"
#include "ModelHierarchy.h"
#include "ModelInfoBaker.h"
#include "ModelMeshBaker.h"
#include "ModelMeshData.h"
#include "Rendering.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

static void WriteTestModelBone(data_writer& writer, string_view name, bool attached_mesh, string_view diffuse_texture, initializer_list<string_view> skin_bone_names, float32_t vertex_scale = 1.0f)
{
    FO_STACK_TRACE_ENTRY();

    writer.write_string(name);

    mat44 matrix {1.0f};
    writer.write<mat44>(matrix);
    writer.write<mat44>(matrix);
    writer.write<uint8_t>(attached_mesh ? uint8_t {1} : uint8_t {0});

    if (attached_mesh) {
        array<ModelMeshVertexData, 3> vertices {};
        vertices[0].Position = vec3 {-0.5f, 0.0f, 0.0f} * vertex_scale;
        vertices[1].Position = vec3 {0.5f, 0.0f, 0.0f} * vertex_scale;
        vertices[2].Position = vec3 {0.0f, 1.0f, 0.0f} * vertex_scale;

        for (ModelMeshVertexData& vertex : vertices) {
            vertex.BlendWeights[0] = 1.0f;
        }

        constexpr array<ModelMeshIndexData, 3> indices {0, 1, 2};
        writer.write<uint32_t>(numeric_cast<uint32_t>(vertices.size())); // Vertices
        writer.write_object_array(const_span<ModelMeshVertexData> {vertices});
        writer.write<uint32_t>(numeric_cast<uint32_t>(indices.size())); // Indices
        writer.write_object_array(const_span<ModelMeshIndexData> {indices});
        writer.write_string(diffuse_texture);

        vector<string_view> effective_skin_bone_names(skin_bone_names);

        if (effective_skin_bone_names.empty()) {
            effective_skin_bone_names.emplace_back();
        }
        writer.write<uint32_t>(numeric_cast<uint32_t>(effective_skin_bone_names.size())); // Skin bones
        for (string_view skin_bone_name : effective_skin_bone_names) {
            writer.write_string(skin_bone_name);
        }

        writer.write<uint32_t>(numeric_cast<uint32_t>(effective_skin_bone_names.size())); // Skin bone offsets
        for (size_t i = 0; i < effective_skin_bone_names.size(); i++) {
            writer.write<mat44>(matrix);
        }
    }

    writer.write<uint32_t>(uint32_t {0}); // Children
}

static void WriteTestModelSourceVec3Track(data_writer& writer, const ModelAnimationVec3Track& track)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track.Times.size() == track.Values.size(), "Test model source vec3 track sizes differ");
    writer.write<uint32_t>(numeric_cast<uint32_t>(track.Times.size()));
    writer.write_object_array(const_span<float32_t> {track.Times});
    writer.write_object_array(const_span<vec3> {track.Values});
}

static void WriteTestModelSourceQuaternionTrack(data_writer& writer, const ModelAnimationQuaternionTrack& track)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track.Times.size() == track.Values.size(), "Test model source quaternion track sizes differ");
    writer.write<uint32_t>(numeric_cast<uint32_t>(track.Times.size()));
    writer.write_object_array(const_span<float32_t> {track.Times});
    writer.write_object_array(const_span<quaternion> {track.Values});
}

static auto ReadTestModelSourceVec3Track(data_reader& reader) -> ModelAnimationVec3Track
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationVec3Track track;
    uint32_t count = reader.read<uint32_t>();
    track.Times.resize(count);
    track.Values.resize(count);
    reader.read_object_array(span<float32_t> {track.Times});
    reader.read_object_array(span<vec3> {track.Values});
    return track;
}

static auto ReadTestModelSourceQuaternionTrack(data_reader& reader) -> ModelAnimationQuaternionTrack
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationQuaternionTrack track;
    uint32_t count = reader.read<uint32_t>();
    track.Times.resize(count);
    track.Values.resize(count);
    reader.read_object_array(span<float32_t> {track.Times});
    reader.read_object_array(span<quaternion> {track.Values});
    return track;
}

static auto WriteTestModelSourceFixture(const ModelSourceAsset& asset) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    data_writer writer {data};
    writer.write_string("LF_TEST_MODEL_SOURCE");
    writer.write<uint32_t>(numeric_cast<uint32_t>(asset.Skeleton.Joints.size()));

    for (const ModelSkeletonJoint& joint : asset.Skeleton.Joints) {
        writer.write_string(joint.Name);
        writer.write_string_vector(joint.Hierarchy);
        writer.write<mat44>(joint.RestLocalTransform);
    }

    writer.write<uint32_t>(numeric_cast<uint32_t>(asset.Animations.size()));

    for (const ModelAnimationSource& animation : asset.Animations) {
        writer.write_string(animation.Name);
        writer.write<float32_t>(animation.Duration);
        writer.write<uint32_t>(numeric_cast<uint32_t>(animation.Joints.size()));

        for (const ModelAnimationJointSource& joint : animation.Joints) {
            writer.write_string(joint.OutputName);
            writer.write_string_vector(joint.Hierarchy);
            WriteTestModelSourceVec3Track(writer, joint.Translation);
            WriteTestModelSourceQuaternionTrack(writer, joint.Rotation);
            WriteTestModelSourceVec3Track(writer, joint.Scale);
        }
    }

    return data;
}

static auto LoadTestModelSourceFixture(string_view path, const File& file) -> ModelSourceAsset
{
    FO_STACK_TRACE_ENTRY();

    auto reader = data_reader(file.GetDataSpan());
    FO_VERIFY_AND_THROW(reader.read_string() == "LF_TEST_MODEL_SOURCE", "Unexpected test model source fixture", path);

    ModelSourceAsset asset;
    asset.FileName = path;
    asset.WriteTime = file.GetWriteTime();
    asset.Skeleton.FileName = path;
    asset.Skeleton.Joints.resize(reader.read<uint32_t>());

    for (ModelSkeletonJoint& joint : asset.Skeleton.Joints) {
        joint.Name = reader.read_string();
        joint.Hierarchy = reader.read_string_vector();
        joint.RestLocalTransform = reader.read<mat44>();
    }

    asset.Animations.resize(reader.read<uint32_t>());

    for (ModelAnimationSource& animation : asset.Animations) {
        animation.FileName = path;
        animation.Name = reader.read_string();
        animation.Duration = reader.read<float32_t>();
        animation.Joints.resize(reader.read<uint32_t>());

        for (ModelAnimationJointSource& joint : animation.Joints) {
            joint.OutputName = reader.read_string();
            joint.Hierarchy = reader.read_string_vector();
            joint.Translation = ReadTestModelSourceVec3Track(reader);
            joint.Rotation = ReadTestModelSourceQuaternionTrack(reader);
            joint.Scale = ReadTestModelSourceVec3Track(reader);
        }
    }

    reader.verify_end();
    return asset;
}

static auto MakeTestModelAnimationJoint(string_view name, vector<string> hierarchy) -> ModelAnimationJointSource
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationJointSource joint;
    joint.OutputName = name;
    joint.Hierarchy = std::move(hierarchy);
    joint.Translation = ModelAnimationVec3Track {{0.0f, 1.0f}, {vec3 {0.0f}, vec3 {0.0f}}};
    joint.Rotation = ModelAnimationQuaternionTrack {{0.0f, 1.0f}, {quaternion {1.0f, 0.0f, 0.0f, 0.0f}, quaternion {1.0f, 0.0f, 0.0f, 0.0f}}};
    joint.Scale = ModelAnimationVec3Track {{0.0f, 1.0f}, {vec3 {1.0f}, vec3 {1.0f}}};
    return joint;
}

static auto MakeTestModelSource(string_view file_name, string_view root_bone, initializer_list<string_view> animation_names) -> ModelSourceAsset
{
    FO_STACK_TRACE_ENTRY();

    ModelSourceAsset asset;
    asset.FileName = file_name;
    asset.Skeleton.FileName = file_name;
    asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {string(root_bone), {string(root_bone)}, mat44 {1.0f}});

    for (string_view animation_name : animation_names) {
        ModelAnimationSource& animation = asset.Animations.emplace_back();
        animation.FileName = file_name;
        animation.Name = animation_name;
        animation.Duration = 1.0f;
        animation.Joints.emplace_back(MakeTestModelAnimationJoint(root_bone, {string(root_bone)}));
    }

    return asset;
}

static auto MakeTestModelSourceWithChild(string_view file_name, string_view root_bone, string_view child_bone, float32_t child_translation_x, initializer_list<string_view> animation_names) -> ModelSourceAsset
{
    FO_STACK_TRACE_ENTRY();

    ModelSourceAsset asset = MakeTestModelSource(file_name, root_bone, {});
    mat44 child_transform {1.0f};
    child_transform[3][0] = child_translation_x;
    asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {string(child_bone), {string(root_bone), string(child_bone)}, child_transform});

    for (string_view animation_name : animation_names) {
        ModelAnimationSource& animation = asset.Animations.emplace_back();
        animation.FileName = file_name;
        animation.Name = animation_name;
        animation.Duration = 1.0f;
        animation.Joints.emplace_back(MakeTestModelAnimationJoint(child_bone, {string(root_bone), string(child_bone)}));
    }

    return asset;
}

static void AddTestModelSource(BakerTests::TestRig& rig, const ModelSourceAsset& asset, uint64_t write_time = 1)
{
    FO_STACK_TRACE_ENTRY();

    rig.AddSourceFile(asset.FileName, WriteTestModelSourceFixture(asset), write_time);
}

static auto MakeTestBakedModel(string_view root_bone, bool attached_mesh, string_view diffuse_texture = {}, initializer_list<string_view> skin_bone_names = {}, float32_t vertex_scale = 1.0f) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    data_writer writer {data};
    WriteModelMeshHeader(writer);
    WriteTestModelBone(writer, root_bone, attached_mesh, diffuse_texture, skin_bone_names, vertex_scale);
    return data;
}

static void AddTestModel(BakerTests::TestRig& rig, string_view file_name, string_view root_bone, bool attached_mesh, initializer_list<string_view> animation_names = {}, string_view diffuse_texture = {}, initializer_list<string_view> skin_bone_names = {}, uint64_t write_time = 1)
{
    FO_STACK_TRACE_ENTRY();

    AddTestModelSource(rig, MakeTestModelSource(file_name, root_bone, animation_names), write_time);
    rig.AddBakedFile(file_name, MakeTestBakedModel(root_bone, attached_mesh, diffuse_texture, skin_bone_names), write_time);
}

static auto MakeTestBakedModelWithChildBone(string_view root_bone, string_view child_bone) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    auto writer = data_writer(data);

    WriteModelMeshHeader(writer);
    writer.write_string(root_bone);

    mat44 matrix {1.0f};
    writer.write<mat44>(matrix);
    writer.write<mat44>(matrix);
    writer.write<uint8_t>(uint8_t {1});

    array<ModelMeshVertexData, 3> vertices {};
    vertices[0].Position = vec3 {-0.5f, 0.0f, 0.0f};
    vertices[1].Position = vec3 {0.5f, 0.0f, 0.0f};
    vertices[2].Position = vec3 {0.0f, 1.0f, 0.0f};

    for (ModelMeshVertexData& vertex : vertices) {
        vertex.BlendWeights[0] = 1.0f;
    }

    constexpr array<ModelMeshIndexData, 3> indices {0, 1, 2};
    writer.write<uint32_t>(numeric_cast<uint32_t>(vertices.size())); // Vertices
    writer.write_object_array(const_span<ModelMeshVertexData> {vertices});
    writer.write<uint32_t>(numeric_cast<uint32_t>(indices.size())); // Indices
    writer.write_object_array(const_span<ModelMeshIndexData> {indices});
    writer.write_string({});
    writer.write<uint32_t>(uint32_t {1}); // Skin bones
    writer.write_string({});
    writer.write<uint32_t>(uint32_t {1}); // Skin bone offsets
    writer.write<mat44>(matrix);
    writer.write<uint32_t>(uint32_t {1}); // Children

    WriteTestModelBone(writer, child_bone, false, {}, {});

    return data;
}

static auto MakeTestBakedModelChain(size_t joint_count) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(joint_count != 0, "Test baked model chain requires at least one joint");
    vector<uint8_t> data;
    data_writer writer {data};
    WriteModelMeshHeader(writer);

    for (size_t joint_index = 0; joint_index < joint_count; joint_index++) {
        writer.write_string(strex("Bone{}", joint_index));
        writer.write<mat44>(mat44 {1.0f});
        writer.write<mat44>(mat44 {1.0f});
        writer.write<uint8_t>(uint8_t {0});
        writer.write<uint32_t>(joint_index + 1 < joint_count ? uint32_t {1} : uint32_t {0});
    }

    return data;
}

static auto MakeTestBakedAnimationModelWithChild(string_view root_bone, string_view child_bone, float32_t child_translation_x) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    auto writer = data_writer(data);

    WriteModelMeshHeader(writer);
    mat44 root_matrix {1.0f};
    writer.write_string(root_bone);
    writer.write<mat44>(root_matrix);
    writer.write<mat44>(root_matrix);
    writer.write<uint8_t>(uint8_t {0});
    writer.write<uint32_t>(uint32_t {1}); // Children

    mat44 child_matrix {1.0f};
    child_matrix[3][0] = child_translation_x;
    writer.write_string(child_bone);
    writer.write<mat44>(child_matrix);
    writer.write<mat44>(child_matrix);
    writer.write<uint8_t>(uint8_t {0});
    writer.write<uint32_t>(uint32_t {0}); // Children
    return data;
}

static void AddTestModelWithChildBone(BakerTests::TestRig& rig, string_view file_name, string_view root_bone, string_view child_bone, float32_t child_translation_x = 0.0f)
{
    FO_STACK_TRACE_ENTRY();

    AddTestModelSource(rig, MakeTestModelSourceWithChild(file_name, root_bone, child_bone, child_translation_x, {}));
    rig.AddBakedFile(file_name, MakeTestBakedModelWithChildBone(root_bone, child_bone));
}

static void AddTestAnimationModelWithChild(BakerTests::TestRig& rig, string_view file_name, string_view root_bone, string_view child_bone, float32_t child_translation_x)
{
    FO_STACK_TRACE_ENTRY();

    AddTestModelSource(rig, MakeTestModelSourceWithChild(file_name, root_bone, child_bone, child_translation_x, {"Idle"}));
    rig.AddBakedFile(file_name, MakeTestBakedAnimationModelWithChild(root_bone, child_bone, child_translation_x));
}

static auto MakeTestBakedModelWithMismatchedSkinOffsets(string_view root_bone, string_view skin_bone) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    auto writer = data_writer(data);

    WriteModelMeshHeader(writer);
    writer.write_string(root_bone);

    mat44 matrix {1.0f};
    writer.write<mat44>(matrix);
    writer.write<mat44>(matrix);
    writer.write<uint8_t>(uint8_t {1});
    writer.write<uint32_t>(uint32_t {0}); // Vertices
    writer.write<uint32_t>(uint32_t {0}); // Indices
    writer.write_string({});
    writer.write<uint32_t>(uint32_t {1}); // Skin bones
    writer.write_string(skin_bone);
    writer.write<uint32_t>(uint32_t {0}); // Skin bone offsets
    writer.write<uint32_t>(uint32_t {0}); // Children

    return data;
}

static void AddModelInfoMetadata(BakerTests::TestRig& rig)
{
    FO_STACK_TRACE_ENTRY();

    rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeEmptyMetadataBlob());
}

static void BakeModelInfoFiles(BakerTests::TestRig& rig, string_view target_path = {})
{
    FO_STACK_TRACE_ENTRY();

    ModelInfoBaker info_baker(rig.MakeContext(), LoadTestModelSourceFixture);
    info_baker.BakeFiles(rig.GetAllSourceFiles(), target_path);
}

static auto CaptureModelInfoBakingError(BakerTests::TestRig& rig, string_view target_path = {}) -> string
{
    FO_STACK_TRACE_ENTRY();

    vector<string> captured_messages;
    set_log_callback("model-info-animation-geometry-test", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) { captured_messages.emplace_back(message); });
    auto remove_callback = scope_exit([]() noexcept { set_log_callback("model-info-animation-geometry-test", {}); });
    CHECK_THROWS_AS(BakeModelInfoFiles(rig, target_path), ModelInfoBakerException);

    auto diagnostic = std::ranges::find_if(captured_messages, [](const string& message) { return message.find("Model description baking error:") != string::npos; });
    REQUIRE(diagnostic != captured_messages.end());
    return *diagnostic;
}

struct SavedModelInfoLink
{
    int32_t Layer {};
    int32_t LayerValue {};
    string LinkBone {};
    string ChildName {};
    bool IsParticles {};
    float32_t RotX {};
    float32_t RotY {};
    float32_t RotZ {};
    float32_t MoveX {};
    float32_t MoveY {};
    float32_t MoveZ {};
    float32_t ScaleX {};
    float32_t ScaleY {};
    float32_t ScaleZ {};
    float32_t SpeedAdjust {};
    uint32_t DisabledLayerCount {};
    uint32_t DisabledMeshCount {};
    uint32_t TextureInfoCount {};
    uint32_t EffectInfoCount {};
    uint32_t CutInfoCount {};
    optional<ModelBounds3D> Bounds {};
    vector<tuple<int32_t, int32_t, ModelBounds3D>> AnimationBounds {};
};

static auto ReadSavedModelInfoString(data_reader& reader) -> string
{
    FO_STACK_TRACE_ENTRY();

    uint32_t len = reader.read<uint32_t>();
    const_span<uint8_t> str_bytes = reader.read_bytes(len);
    return !str_bytes.empty() ? string(reinterpret_cast<const char*>(str_bytes.data()), len) : string {};
}

static void ReadSavedModelInfoHeader(data_reader& reader)
{
    FO_STACK_TRACE_ENTRY();

    const_span<uint8_t> magic = reader.read_bytes(MODEL_DESCRIPTION_MAGIC.size());
    REQUIRE(std::equal(magic.begin(), magic.end(), MODEL_DESCRIPTION_MAGIC.begin()));
    CHECK(reader.read<uint16_t>() == MODEL_DESCRIPTION_SCHEMA_VERSION);
    CHECK(reader.read<uint16_t>() == MODEL_DESCRIPTION_SUPPORTED_FLAGS);
}

static void SkipSavedModelInfoCut(data_reader& reader)
{
    FO_STACK_TRACE_ENTRY();

    (void)ReadSavedModelInfoString(reader);

    uint32_t layer_count = reader.read<uint32_t>();
    (void)reader.read_bytes(numeric_cast<size_t>(layer_count) * sizeof(int32_t));

    uint32_t shape_count = reader.read<uint32_t>();
    for (uint32_t i = 0; i < shape_count; i++) {
        (void)ReadSavedModelInfoString(reader);
    }

    (void)ReadSavedModelInfoString(reader);
    (void)ReadSavedModelInfoString(reader);
    (void)ReadSavedModelInfoString(reader);
    (void)reader.read<uint8_t>();
}

static auto ReadSavedModelInfoLink(data_reader& reader) -> SavedModelInfoLink
{
    FO_STACK_TRACE_ENTRY();

    SavedModelInfoLink link;
    link.Layer = reader.read<int32_t>();
    link.LayerValue = reader.read<int32_t>();
    link.LinkBone = ReadSavedModelInfoString(reader);
    link.ChildName = ReadSavedModelInfoString(reader);
    link.IsParticles = reader.read<uint8_t>() != 0;
    link.RotX = reader.read<float32_t>();
    link.RotY = reader.read<float32_t>();
    link.RotZ = reader.read<float32_t>();
    link.MoveX = reader.read<float32_t>();
    link.MoveY = reader.read<float32_t>();
    link.MoveZ = reader.read<float32_t>();
    link.ScaleX = reader.read<float32_t>();
    link.ScaleY = reader.read<float32_t>();
    link.ScaleZ = reader.read<float32_t>();
    link.SpeedAdjust = reader.read<float32_t>();

    link.DisabledLayerCount = reader.read<uint32_t>();
    (void)reader.read_bytes(numeric_cast<size_t>(link.DisabledLayerCount) * sizeof(int32_t));

    link.DisabledMeshCount = reader.read<uint32_t>();
    for (uint32_t i = 0; i < link.DisabledMeshCount; i++) {
        (void)ReadSavedModelInfoString(reader);
    }

    link.TextureInfoCount = reader.read<uint32_t>();
    for (uint32_t i = 0; i < link.TextureInfoCount; i++) {
        (void)ReadSavedModelInfoString(reader);
        (void)ReadSavedModelInfoString(reader);
        (void)reader.read<int32_t>();
    }

    link.EffectInfoCount = reader.read<uint32_t>();
    for (uint32_t i = 0; i < link.EffectInfoCount; i++) {
        (void)ReadSavedModelInfoString(reader);
        (void)ReadSavedModelInfoString(reader);
    }

    link.CutInfoCount = reader.read<uint32_t>();
    for (uint32_t i = 0; i < link.CutInfoCount; i++) {
        SkipSavedModelInfoCut(reader);
    }

    uint8_t has_geometry_value = reader.read<uint8_t>();
    CHECK(has_geometry_value <= uint8_t {1});
    bool has_geometry = has_geometry_value != 0;
    bool expected_geometry = !link.ChildName.empty() && !link.IsParticles;
    CHECK(has_geometry == expected_geometry);

    if (has_geometry) {
        link.Bounds = ModelBounds3D {
            .Min = reader.read<vec3>(),
            .Max = reader.read<vec3>(),
        };

        uint32_t animation_bounds_count = reader.read<uint32_t>();

        for (uint32_t i = 0; i < animation_bounds_count; i++) {
            int32_t state_anim = reader.read<int32_t>();
            int32_t action_anim = reader.read<int32_t>();
            ModelBounds3D clip_bounds {.Min = reader.read<vec3>(), .Max = reader.read<vec3>()};
            link.AnimationBounds.emplace_back(state_anim, action_anim, clip_bounds);
        }
    }

    return link;
}

struct BakedModelMeshSummary
{
    uint32_t Bones {};
    uint32_t AttachedMeshes {};
    uint32_t Vertices {};
    uint32_t Indices {};
    uint32_t SkinBones {};
    string DiffuseTexture {};
    optional<Vertex3D> FirstVertex {};
    vector<Vertex3D> VertexData {};
    vector<vindex_t> IndexData {};
};

static void SkipBakedModelMeshPayload(data_reader& reader, BakedModelMeshSummary& summary)
{
    FO_STACK_TRACE_ENTRY();

    uint32_t vertex_count = reader.read<uint32_t>();
    summary.Vertices += vertex_count;
    size_t vertex_offset = summary.VertexData.size();
    summary.VertexData.resize(vertex_offset + vertex_count);
    reader.read_object_array(span<Vertex3D> {summary.VertexData}.subspan(vertex_offset, vertex_count));
    if (vertex_count != 0 && !summary.FirstVertex) {
        summary.FirstVertex = summary.VertexData[vertex_offset];
    }

    uint32_t index_count = reader.read<uint32_t>();
    summary.Indices += index_count;
    size_t index_offset = summary.IndexData.size();
    summary.IndexData.resize(index_offset + index_count);
    reader.read_object_array(span<vindex_t> {summary.IndexData}.subspan(index_offset, index_count));

    summary.DiffuseTexture = ReadSavedModelInfoString(reader);

    uint32_t skin_bone_count = reader.read<uint32_t>();
    summary.SkinBones += skin_bone_count;
    for (uint32_t i = 0; i < skin_bone_count; i++) {
        (void)ReadSavedModelInfoString(reader);
    }

    uint32_t skin_bone_offset_count = reader.read<uint32_t>();
    (void)reader.read_bytes(numeric_cast<size_t>(skin_bone_offset_count) * sizeof(mat44));
}

static void ReadBakedModelMeshSummaryBone(data_reader& reader, BakedModelMeshSummary& summary)
{
    FO_STACK_TRACE_ENTRY();

    (void)ReadSavedModelInfoString(reader);
    summary.Bones++;

    (void)reader.read_bytes(sizeof(mat44));
    (void)reader.read_bytes(sizeof(mat44));

    if (reader.read<uint8_t>() != 0) {
        summary.AttachedMeshes++;
        SkipBakedModelMeshPayload(reader, summary);
    }

    uint32_t children_count = reader.read<uint32_t>();
    for (uint32_t i = 0; i < children_count; i++) {
        ReadBakedModelMeshSummaryBone(reader, summary);
    }
}

static auto ReadBakedModelMeshSummary(const vector<uint8_t>& data) -> BakedModelMeshSummary
{
    FO_STACK_TRACE_ENTRY();

    auto reader = data_reader({data.data(), data.size()});
    BakedModelMeshSummary summary;

    ReadModelMeshHeader(reader, "test baked model mesh");
    ReadBakedModelMeshSummaryBone(reader, summary);

    CHECK_NOTHROW(reader.verify_end());
    return summary;
}

static auto MakeMinimalObjMesh(string_view object_name) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex(R"(o {}
v 0 0 0 1 0 0 1
v 1 0 0 0 1 0 1
v 0 1 0 0 0 1 1
vt 0 0
vt 1 0
vt 0 1
vn 0 0 1
usemtl BodyMat
f 1/1/1 2/2/1 3/3/1
)",
        object_name)
        .str();
}

static auto MakeMinimalPositionOnlyObjMesh(string_view object_name) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex(R"(o {}
v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)",
        object_name)
        .str();
}

static auto MakeRepeatedTriangleObjMesh(string_view object_name, size_t triangle_count) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result = strex(R"(o {}
v 0 0 0
v 1 0 0
v 0 1 0
)",
        object_name)
                        .str();

    for (size_t i = 0; i < triangle_count; i++) {
        result += "f 1 2 3\n";
    }

    return result;
}

static auto MakeInterleavedStripObjMesh(string_view object_name, size_t strip_count, size_t triangles_per_strip) -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(strip_count >= 2, "Interleaved strip fixture requires at least two strips");
    FO_VERIFY_AND_THROW(triangles_per_strip >= 2, "Interleaved strip fixture requires at least two triangles per strip");

    string result = strex("o {}\n", object_name).str();
    size_t vertices_per_strip = triangles_per_strip + 2;

    for (size_t strip = 0; strip < strip_count; strip++) {
        for (size_t vertex = 0; vertex < vertices_per_strip; vertex++) {
            result += strex("v {} {} 0\n", vertex, strip * 4 + vertex % 2).str();
        }
    }

    for (size_t triangle = 0; triangle < triangles_per_strip; triangle++) {
        for (size_t strip = 0; strip < strip_count; strip++) {
            size_t first = strip * vertices_per_strip + triangle + 1;

            if (triangle % 2 == 0) {
                result += strex("f {} {} {}\n", first, first + 1, first + 2).str();
            }
            else {
                result += strex("f {} {} {}\n", first + 1, first, first + 2).str();
            }
        }
    }

    return result;
}

static auto MakeMinimalSkinnedAsciiFbx(string_view mesh_name, const vector<array<float64_t, 3>>& cluster_weights) -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!cluster_weights.empty(), "Test skinned FBX fixture requires at least one cluster");
    int64_t geometry_id = 1000;
    int64_t mesh_model_id = 2000;
    int64_t skin_id = 3000;
    int64_t first_cluster_id = 4000;
    int64_t first_bone_id = 5000;
    string result = strex(R"(; FBX 7.4.0 project file
FBXHeaderExtension:  {{
    FBXHeaderVersion: 1003
    FBXVersion: 7400
    Creator: "FOnline ModelMeshBaker skin validation test"
}}
Definitions:  {{
    Version: 100
    Count: {}
    ObjectType: "Model" {{
        Count: {}
    }}
    ObjectType: "Geometry" {{
        Count: 1
    }}
    ObjectType: "Deformer" {{
        Count: {}
    }}
}}
Objects:  {{
    Geometry: {}, "Geometry::{}", "Mesh" {{
        GeometryVersion: 124
        Vertices: *9 {{
            a: 0,0,0,1,0,0,0,1,0
        }}
        PolygonVertexIndex: *3 {{
            a: 0,1,-3
        }}
    }}
    Model: {}, "Model::{}", "Mesh" {{
        Version: 232
        Properties70:  {{
            P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0
            P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0
            P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1
        }}
        Shading: T
        Culling: "CullingOff"
    }}
    Deformer: {}, "Deformer::Skin", "Skin" {{
        Version: 101
        Link_DeformAcuracy: 50
    }}
)",
        3 + cluster_weights.size() * 2, 1 + cluster_weights.size(), 1 + cluster_weights.size(), geometry_id, mesh_name, mesh_model_id, mesh_name, skin_id)
                        .str();

    constexpr string_view identity_matrix = "1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1";

    for (size_t cluster_index = 0; cluster_index < cluster_weights.size(); cluster_index++) {
        int64_t cluster_id = first_cluster_id + numeric_cast<int64_t>(cluster_index);
        int64_t bone_id = first_bone_id + numeric_cast<int64_t>(cluster_index);
        string indexes;
        string weights;
        size_t influence_count = 0;

        for (size_t vertex_index = 0; vertex_index < cluster_weights[cluster_index].size(); vertex_index++) {
            float64_t weight = cluster_weights[cluster_index][vertex_index];

            if (weight == 0.0) {
                continue;
            }

            if (influence_count != 0) {
                indexes += ',';
                weights += ',';
            }

            indexes += strex("{}", vertex_index);
            weights += strex("{}", weight);
            influence_count++;
        }

        result += strex(R"(    Deformer: {}, "SubDeformer::Cluster{}", "Cluster" {{
        Version: 100
        UserData: "", ""
        Indexes: *{} {{
            a: {}
        }}
        Weights: *{} {{
            a: {}
        }}
        Transform: *16 {{
            a: {}
        }}
        TransformLink: *16 {{
            a: {}
        }}
    }}
    Model: {}, "Model::Bone{}", "LimbNode" {{
        Version: 232
        Properties70:  {{
            P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0
            P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0
            P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1
        }}
        Shading: T
        Culling: "CullingOff"
    }}
)",
            cluster_id, cluster_index, influence_count, indexes, influence_count, weights, identity_matrix, identity_matrix, bone_id, cluster_index);
    }

    result += "}\nConnections:  {\n";
    result += strex("    C: \"OO\",{},{}\n", geometry_id, mesh_model_id);
    result += strex("    C: \"OO\",{},0\n", mesh_model_id);
    result += strex("    C: \"OO\",{},{}\n", skin_id, geometry_id);

    for (size_t cluster_index = 0; cluster_index < cluster_weights.size(); cluster_index++) {
        int64_t cluster_id = first_cluster_id + numeric_cast<int64_t>(cluster_index);
        int64_t bone_id = first_bone_id + numeric_cast<int64_t>(cluster_index);
        result += strex("    C: \"OO\",{},0\n", bone_id);
        result += strex("    C: \"OO\",{},{}\n", cluster_id, skin_id);
        result += strex("    C: \"OO\",{},{}\n", bone_id, cluster_id);
    }

    result += "}\n";
    return result;
}

static auto MakeWideHierarchyAsciiFbx(string_view node_prefix, size_t child_count) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result = strex(R"(; FBX 7.4.0 project file
FBXHeaderExtension:  {{
    FBXHeaderVersion: 1003
    FBXVersion: 7400
    Creator: "FOnline ModelMeshBaker hierarchy limit test"
}}
Definitions:  {{
    Version: 100
    Count: {}
    ObjectType: "Model" {{
        Count: {}
    }}
}}
Objects:  {{
)",
        child_count, child_count)
                        .str();

    constexpr int64_t first_node_id = 1000;

    for (size_t child_index = 0; child_index < child_count; child_index++) {
        result += strex(R"(    Model: {}, "Model::{}{}", "Null" {{
        Version: 232
    }}
)",
            first_node_id + numeric_cast<int64_t>(child_index), node_prefix, child_index);
    }

    result += "}\nConnections:  {\n";

    for (size_t child_index = 0; child_index < child_count; child_index++) {
        result += strex("    C: \"OO\",{},0\n", first_node_id + numeric_cast<int64_t>(child_index));
    }

    result += "}\n";
    return result;
}
#endif

TEST_CASE("ModelBakers")
{
#if FO_ENABLE_3D
    using namespace BakerTests;

    TestRig rig;
    AddModelInfoMetadata(rig);

    auto bakers = MakeRequestedBakers({string(ModelMeshBaker::NAME), string(ModelInfoBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 2);
    CHECK(bakers[0]->GetName() == ModelMeshBaker::NAME);
    CHECK(bakers[0]->GetOrder() == 4);
    CHECK(bakers[1]->GetName() == ModelInfoBaker::NAME);
    CHECK(bakers[1]->GetOrder() == 6);
    CHECK_NOTHROW(bakers[0]->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
    CHECK_NOTHROW(bakers[1]->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));

    rig.AddSourceFile("Critters/TEMPLATE_Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Base\n", 2);
    rig.AddSourceFile("Critters/Test.fo3d", "Include TEMPLATE_Test.fo3d\nLayer 1\nValue 2\nAttach Hat.fbx Link Hand RotX 15 Texture 0 Hat.tga\n", 1);
    AddTestModel(rig, "Critters/Body.fbx", "Hand", true, {"Base"});
    AddTestModel(rig, "Critters/Hat.fbx", "Hat", true);
    rig.AddBakedFile("Critters/Hat.tga", "stub");

    ModelMeshBaker mesh_baker(rig.MakeContext());
    mesh_baker.BakeFiles(rig.GetAllSourceFiles(), "skip.bin");
    CHECK(rig.Outputs.empty());

    ModelInfoBaker info_baker(rig.MakeContext(), LoadTestModelSourceFixture);
    info_baker.BakeFiles(rig.GetAllSourceFiles(), "");

    REQUIRE(rig.Outputs.count("Critters/Test.fo3d") == 1);

    auto reader = data_reader({rig.Outputs.at("Critters/Test.fo3d").data(), rig.Outputs.at("Critters/Test.fo3d").size()});
    ReadSavedModelInfoHeader(reader);
    uint32_t model_name_len = reader.read<uint32_t>();
    string model_name;
    model_name.resize(model_name_len);
    reader.read_string_bytes(model_name);
    CHECK(model_name == "Critters/Body.fbx");
    CHECK(rig.Outputs.count("Critters/TEMPLATE_Test.fo3d") == 0);

    ModelInfoBaker skipped_info_baker(rig.MakeContext("TestPack", [](string_view, uint64_t) { return false; }), LoadTestModelSourceFixture);
    CHECK_NOTHROW(skipped_info_baker.BakeFiles(rig.GetAllSourceFiles(), ""));
    CHECK(rig.Outputs.empty());

    TestRig skip_rig;
    AddModelInfoMetadata(skip_rig);
    skip_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
    AddTestModel(skip_rig, "Critters/Body.fbx", "Body", true);
    ModelInfoBaker skipped_baker(skip_rig.MakeContext("TestPack", [](string_view, uint64_t) { return false; }), LoadTestModelSourceFixture);
    CHECK_NOTHROW(skipped_baker.BakeFiles(skip_rig.GetAllSourceFiles(), ""));
    CHECK(skip_rig.Outputs.empty());

    TestRig invalid_rig;
    AddModelInfoMetadata(invalid_rig);
    invalid_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 1\nValue 2\nAttach Hat.fbx Link MissingBone\n", 1);
    AddTestModel(invalid_rig, "Critters/Body.fbx", "Hand", true);
    AddTestModel(invalid_rig, "Critters/Hat.fbx", "Hat", true);

    ModelInfoBaker invalid_info_baker(invalid_rig.MakeContext(), LoadTestModelSourceFixture);
    CHECK_THROWS_AS(invalid_info_baker.BakeFiles(invalid_rig.GetAllSourceFiles(), ""), ModelInfoBakerException);
#endif
}

TEST_CASE("ModelBoneLookup")
{
#if FO_ENABLE_3D
    hash_storage hash_storage;
    hstring root_name = hash_storage.to_hashed_string("Root");
    hstring child_name = hash_storage.to_hashed_string("Child");
    hstring missing_name = hash_storage.to_hashed_string("Missing");

    ModelBone root;
    root.Name = root_name;
    root.Children.emplace_back(safe_alloc::make_unique<ModelBone>());
    root.Children.back()->Name = child_name;

    ptr<ModelBone> root_ptr = &root;
    CHECK(FindModelBone(root_ptr, root_name) == &root);
    CHECK(FindModelBone(root_ptr, child_name) == root.Children.back());
    CHECK(FindModelBone(root_ptr, missing_name) == nullptr);

    const ModelBone& const_root = root;
    ptr<const ModelBone> const_root_ptr = &const_root;
    CHECK(FindModelBone(const_root_ptr, child_name) == root.Children.back());
#endif
}

TEST_CASE("ModelMeshBakerOrchestration")
{
#if FO_ENABLE_3D
    using namespace BakerTests;

    SECTION("Uses bake checker for source and target model files")
    {
        TestRig rig;
        rig.AddSourceFile("Models/Ignored.txt", "not a model", 3);
        rig.AddSourceFile("Models/Broken.fbx", "not fbx data", 9);

        vector<pair<string, uint64_t>> checks;
        ModelMeshBaker baker(rig.MakeContext("TestPack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return false;
        }));

        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        CHECK(rig.Outputs.empty());

        REQUIRE(checks.size() == 1);
        CHECK(checks.front() == pair<string, uint64_t> {"Models/Broken.fbx", 9});

        checks.clear();
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/Broken.fbx"));
        CHECK(rig.Outputs.empty());

        REQUIRE(checks.size() == 1);
        CHECK(checks.front() == pair<string, uint64_t> {"Models/Broken.fbx", 9});
    }

    SECTION("Skips non model targets and missing target files")
    {
        TestRig rig;
        rig.AddSourceFile("Models/Broken.fbx", "not fbx data", 9);

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/Broken.txt"));
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/Missing.fbx"));
        CHECK(rig.Outputs.empty());
    }

    SECTION("Returns cleanly when source scan has no model mesh files")
    {
        TestRig rig;
        rig.AddSourceFile("Models/Readme.txt", "not a model", 3);

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        CHECK(rig.Outputs.empty());
    }

    SECTION("Bakes OBJ meshes from full source scan")
    {
        TestRig rig;
        rig.AddSourceFile("Models/Readme.txt", "not a model", 3);
        rig.AddSourceFile("Models/Triangle.obj", MakeMinimalObjMesh("ScannedBody"), 9);

        vector<pair<string, uint64_t>> checks;
        ModelMeshBaker baker(rig.MakeContext("TestPack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return true;
        }));

        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        REQUIRE(checks.size() == 1);
        CHECK(checks.front() == pair<string, uint64_t> {"Models/Triangle.obj", 9});
        REQUIRE(rig.Outputs.count("Models/Triangle.obj") == 1);
        BakedModelMeshSummary summary = ReadBakedModelMeshSummary(rig.Outputs.at("Models/Triangle.obj"));
        CHECK(summary.AttachedMeshes == 1);
        CHECK(summary.Vertices == 3);
        CHECK(summary.Indices == 3);
    }

    SECTION("Bakes a minimal OBJ mesh")
    {
        TestRig rig;
        rig.AddSourceFile("Models/Triangle.obj", MakeMinimalObjMesh("Body"), 9);

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/Triangle.obj"));

        REQUIRE(rig.Outputs.count("Models/Triangle.obj") == 1);
        BakedModelMeshSummary summary = ReadBakedModelMeshSummary(rig.Outputs.at("Models/Triangle.obj"));
        CHECK(summary.Bones >= 1);
        CHECK(summary.AttachedMeshes == 1);
        CHECK(summary.Vertices == 3);
        CHECK(summary.Indices == 3);
    }

    SECTION("Bakes position-only OBJ mesh with default vertex fields")
    {
        TestRig rig;
        rig.AddSourceFile("Models/PositionOnly.obj", MakeMinimalPositionOnlyObjMesh("PlainBody"), 9);

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/PositionOnly.obj"));

        REQUIRE(rig.Outputs.count("Models/PositionOnly.obj") == 1);
        BakedModelMeshSummary summary = ReadBakedModelMeshSummary(rig.Outputs.at("Models/PositionOnly.obj"));
        CHECK(summary.Bones >= 1);
        CHECK(summary.AttachedMeshes == 1);
        CHECK(summary.Vertices == 3);
        CHECK(summary.Indices == 3);
        CHECK(summary.SkinBones == 1);
        CHECK(summary.DiffuseTexture.empty());
        REQUIRE(summary.FirstVertex.has_value());
        CHECK(summary.FirstVertex->Color == ucolor {});
        CHECK(summary.FirstVertex->TexCoord[0] == 0.0f);
        CHECK(summary.FirstVertex->TexCoord[1] == 0.0f);
        CHECK(summary.FirstVertex->TexCoordBase[0] == 0.0f);
        CHECK(summary.FirstVertex->TexCoordBase[1] == 0.0f);
        CHECK(summary.FirstVertex->BlendIndices[0] == 0.0f);
        CHECK(summary.FirstVertex->BlendWeights[0] == 1.0f);
    }

    SECTION("Optimizes baked vertex cache and fetch order")
    {
        constexpr size_t strip_count = 8;
        constexpr size_t triangles_per_strip = 4;
        TestRig rig;
        rig.AddSourceFile("Models/InterleavedStrips.obj", MakeInterleavedStripObjMesh("InterleavedStrips", strip_count, triangles_per_strip), 9);

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/InterleavedStrips.obj"));

        REQUIRE(rig.Outputs.count("Models/InterleavedStrips.obj") == 1);
        BakedModelMeshSummary summary = ReadBakedModelMeshSummary(rig.Outputs.at("Models/InterleavedStrips.obj"));
        REQUIRE(summary.AttachedMeshes == 1);
        REQUIRE(summary.VertexData.size() == strip_count * (triangles_per_strip + 2));
        REQUIRE(summary.IndexData.size() == strip_count * triangles_per_strip * 3);
        REQUIRE(summary.IndexData.size() >= 6);

        bool first_triangles_share_vertex = false;
        for (size_t first = 0; first < 3; first++) {
            for (size_t second = 3; second < 6; second++) {
                first_triangles_share_vertex = first_triangles_share_vertex || summary.IndexData[first] == summary.IndexData[second];
            }
        }
        CHECK(first_triangles_share_vertex);

        vector<uint8_t> seen_vertices(summary.VertexData.size());
        size_t next_first_use = 0;

        for (vindex_t index : summary.IndexData) {
            REQUIRE(index < summary.VertexData.size());

            if (seen_vertices[index] == 0) {
                CHECK(index == next_first_use);
                seen_vertices[index] = 1;
                next_first_use++;
            }
        }

        CHECK(next_first_use == summary.VertexData.size());
    }

    SECTION("Normalizes the retained four skin influences")
    {
        TestRig rig;
        vector<array<float64_t, 3>> cluster_weights {
            {0.5, 0.5, 0.5},
            {0.25, 0.25, 0.25},
            {0.125, 0.125, 0.125},
            {0.0625, 0.0625, 0.0625},
            {0.03125, 0.03125, 0.03125},
        };
        rig.AddSourceFile("Models/FiveInfluences.fbx", MakeMinimalSkinnedAsciiFbx("SkinnedBody", cluster_weights), 9);

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/FiveInfluences.fbx"));

        REQUIRE(rig.Outputs.count("Models/FiveInfluences.fbx") == 1);
        BakedModelMeshSummary summary = ReadBakedModelMeshSummary(rig.Outputs.at("Models/FiveInfluences.fbx"));
        REQUIRE(summary.FirstVertex.has_value());
        CHECK(summary.SkinBones == cluster_weights.size());

        float32_t retained_weight_sum = 0.0f;

        for (size_t influence = 0; influence < MODEL_BONES_PER_VERTEX; influence++) {
            CHECK(summary.FirstVertex->BlendWeights[influence] >= 0.0f);
            CHECK(summary.FirstVertex->BlendIndices[influence] >= 0.0f);
            CHECK(summary.FirstVertex->BlendIndices[influence] < numeric_cast<float32_t>(cluster_weights.size()));
            retained_weight_sum += summary.FirstVertex->BlendWeights[influence];
        }

        CHECK(is_float_equal(retained_weight_sum, 1.0f, 1.0e-5f));
        CHECK(is_float_equal(summary.FirstVertex->BlendWeights[0], 0.5f / 0.9375f, 1.0e-5f));
        CHECK(is_float_equal(summary.FirstVertex->BlendWeights[3], 0.0625f / 0.9375f, 1.0e-5f));
    }

    SECTION("Rejects skinned vertices without retained influences")
    {
        TestRig rig;
        vector<array<float64_t, 3>> cluster_weights {{1.0, 1.0, 0.0}};
        rig.AddSourceFile("Models/UnweightedVertex.fbx", MakeMinimalSkinnedAsciiFbx("BrokenSkin", cluster_weights), 9);

        vector<string> captured_messages;
        set_log_callback("model-mesh-unweighted-vertex-test", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) { captured_messages.emplace_back(message); });
        auto remove_callback = scope_exit([]() noexcept { set_log_callback("model-mesh-unweighted-vertex-test", {}); });

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/UnweightedVertex.fbx"), Catch::Matchers::ContainsSubstring("Errors during model mesh baking"));
        CHECK(std::ranges::any_of(captured_messages, [](const string& message) { return message.find("no retained skin influences") != string::npos; }));
    }

    SECTION("Allows an index count above vindex max when every index value fits")
    {
        TestRig rig;
        size_t triangle_count = sizeof(vindex_t) == sizeof(uint16_t) ? numeric_cast<size_t>(std::numeric_limits<uint16_t>::max()) / 3 + 2 : 4;
        rig.AddSourceFile("Models/RepeatedTriangle.obj", MakeRepeatedTriangleObjMesh("RepeatedBody", triangle_count), 9);

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/RepeatedTriangle.obj"));

        REQUIRE(rig.Outputs.count("Models/RepeatedTriangle.obj") == 1);
        BakedModelMeshSummary summary = ReadBakedModelMeshSummary(rig.Outputs.at("Models/RepeatedTriangle.obj"));
        CHECK(summary.Vertices == 3);
        CHECK(summary.Indices == triangle_count * 3);
    }

    SECTION("Rejects a wide hierarchy above the Ozz joint limit during baking")
    {
        TestRig rig;
        rig.AddSourceFile("Models/TooWide.fbx", MakeWideHierarchyAsciiFbx("WideNode", MODEL_ANIMATION_RIG_MAX_JOINTS), 9);

        vector<string> captured_messages;
        set_log_callback("model-mesh-wide-hierarchy-test", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) { captured_messages.emplace_back(message); });
        auto remove_callback = scope_exit([]() noexcept { set_log_callback("model-mesh-wide-hierarchy-test", {}); });

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/TooWide.fbx"), Catch::Matchers::ContainsSubstring("Errors during model mesh baking"));
        CHECK(rig.Outputs.empty());
        string expected_diagnostic = "FBX hierarchy has too many joints";
        CHECK(std::ranges::any_of(captured_messages, [&expected_diagnostic](const string& message) { return message.find(expected_diagnostic) != string::npos && message.find(strex("{}", MODEL_ANIMATION_RIG_MAX_JOINTS + 1)) != string::npos && message.find(strex("{}", MODEL_ANIMATION_RIG_MAX_JOINTS)) != string::npos; }));
    }

    SECTION("Rejects non-finite source geometry with asset context")
    {
        TestRig rig;
        rig.AddSourceFile("Models/NonFinite.obj", R"(o BrokenBody
v nan 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)",
            9);

        vector<string> captured_messages;
        set_log_callback("model-mesh-non-finite-test", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) { captured_messages.emplace_back(message); });
        auto remove_callback = scope_exit([]() noexcept { set_log_callback("model-mesh-non-finite-test", {}); });

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/NonFinite.obj"), ModelMeshBakerException);
        CHECK(rig.Outputs.empty());

        auto diagnostic_it = std::ranges::find_if(captured_messages, [](const string& message) { return message.find("invalid numeric data") != string::npos; });
        REQUIRE(diagnostic_it != captured_messages.end());
        CHECK(diagnostic_it->find("Models/NonFinite.obj") != string::npos);
        CHECK(diagnostic_it->find("BrokenBody") != string::npos);
        CHECK(diagnostic_it->find("position") != string::npos);
    }

    SECTION("Aggregates invalid model mesh input errors")
    {
        TestRig rig;
        rig.AddSourceFile("Models/Broken.fbx", "not fbx data", 9);

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("Errors during model mesh baking"));
        CHECK(rig.Outputs.empty());
    }
#endif
}

TEST_CASE("ModelInfoBakerOrchestration")
{
#if FO_ENABLE_3D
    using namespace BakerTests;

    SECTION("Uses include-aware max write time in bake checker")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/TEMPLATE_Anim.fo3d", "Anim 0 1 ModelFile Idle\n", 50);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nInclude TEMPLATE_Anim.fo3d\n", 7);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true, {"Idle"});

        vector<pair<string, uint64_t>> checks;
        ModelInfoBaker baker(rig.MakeContext("TestPack",
                                 [&checks](string_view path, uint64_t write_time) {
                                     checks.emplace_back(string(path), write_time);
                                     return true;
                                 }),
            LoadTestModelSourceFixture);

        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        REQUIRE(checks.size() == 3);
        CHECK(std::ranges::count(checks, pair<string, uint64_t> {"ModelAnimationInfo.foinfo", 50}) == 1);
        CHECK(std::ranges::count(checks, pair<string, uint64_t> {"Critters/Test.fo3d", 50}) == 2);
        CHECK(rig.Outputs.count("ModelAnimationInfo.foinfo") == 1);
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
        CHECK(rig.Outputs.count("Critters/TEMPLATE_Anim.fo3d") == 0);
    }

    SECTION("Uses referenced model source max write time in bake checker")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/TEMPLATE_Anim.fo3d", "Anim 0 1 External.fbx Idle\n", 50);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nInclude TEMPLATE_Anim.fo3d\nLayer 1 Value 2 Attach Hat.fbx\nCut Gore.fbx All All - - -\n", 7);
        AddTestModelSource(rig, MakeTestModelSource("Critters/Body.fbx", "Body", {}), 60);
        AddTestModelSource(rig, MakeTestModelSource("Critters/External.fbx", "Body", {"Idle"}), 90);
        AddTestModelSource(rig, MakeTestModelSource("Critters/Hat.fbx", "Hat", {}), 80);
        AddTestModelSource(rig, MakeTestModelSource("Critters/Gore.fbx", "Gore", {}), 70);

        vector<pair<string, uint64_t>> checks;
        ModelInfoBaker baker(rig.MakeContext("TestPack",
                                 [&checks](string_view path, uint64_t write_time) {
                                     checks.emplace_back(string(path), write_time);
                                     return false;
                                 }),
            LoadTestModelSourceFixture);

        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        REQUIRE(checks.size() == 2);
        CHECK(std::ranges::count(checks, pair<string, uint64_t> {"ModelAnimationInfo.foinfo", 90}) == 1);
        CHECK(std::ranges::count(checks, pair<string, uint64_t> {"Critters/Test.fo3d", 90}) == 1);
        CHECK(rig.Outputs.empty());
    }

    SECTION("Derives dependencies from the final parsed description")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/TEMPLATE_Final.fo3d", "Model %Model%\nRotationBone Model\n", 5);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Superseded.fbx\nInclude TEMPLATE_Final.fo3d Model Body.fbx\nAnim 0 1 Selected.fbx Idle\nAnim 0 1 Duplicate.fbx Idle\n", 7);
        AddTestModelSource(rig, MakeTestModelSource("Critters/Body.fbx", "Model", {}), 30);
        AddTestModelSource(rig, MakeTestModelSource("Critters/Selected.fbx", "Model", {"Idle"}), 40);

        vector<pair<string, uint64_t>> checks;
        ModelInfoBaker baker(rig.MakeContext("TestPack",
                                 [&checks](string_view path, uint64_t write_time) {
                                     checks.emplace_back(string(path), write_time);
                                     return false;
                                 }),
            LoadTestModelSourceFixture);

        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        REQUIRE(checks.size() == 2);
        CHECK(std::ranges::count(checks, pair<string, uint64_t> {"ModelAnimationInfo.foinfo", 40}) == 1);
        CHECK(std::ranges::count(checks, pair<string, uint64_t> {"Critters/Test.fo3d", 40}) == 1);
        CHECK(rig.Outputs.empty());
    }

    SECTION("Skips non fo3d, template and missing target files")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/TEMPLATE_Test.fo3d", "Model Body.fbx\n", 1);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Missing.fbx\n", 2);

        ModelInfoBaker baker(rig.MakeContext(), LoadTestModelSourceFixture);
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Critters/Test.txt"));
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Critters/TEMPLATE_Test.fo3d"));
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Critters/Missing.fo3d"));
        CHECK(rig.Outputs.empty());
    }

    SECTION("Returns cleanly when normal model-info input has no bakeable descriptions")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Readme.txt", "ignored", 1);
        rig.AddSourceFile("Critters/TEMPLATE_Test.fo3d", "Model Body.fbx\n", 2);

        ModelInfoBaker baker(rig.MakeContext(), LoadTestModelSourceFixture);
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        CHECK(rig.Outputs.empty());
    }

    SECTION("Bakes a specific model description target")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Other.fo3d", "Model Missing.fbx\n", 10);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 20);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        ModelInfoBaker baker(rig.MakeContext(), LoadTestModelSourceFixture);
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Critters/Test.fo3d"));

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
    }

    SECTION("Returns cleanly for empty ModelAnimationInfo input and skipped output")
    {
        TestRig empty_rig;
        AddModelInfoMetadata(empty_rig);
        empty_rig.AddSourceFile("Critters/TEMPLATE_Test.fo3d", "Model Body.fbx\n", 1);

        ModelInfoBaker empty_baker(empty_rig.MakeContext("ArbitraryPack"), LoadTestModelSourceFixture);
        CHECK_NOTHROW(empty_baker.BakeFiles(empty_rig.GetAllSourceFiles(), ""));
        CHECK(empty_rig.Outputs.empty());

        TestRig skipped_rig;
        AddModelInfoMetadata(skipped_rig);
        skipped_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Idle\n", 10);
        AddTestModel(skipped_rig, "Critters/Body.fbx", "Body", true, {"Idle"});

        ModelInfoBaker skipped_baker(skipped_rig.MakeContext("ArbitraryPack", [](string_view, uint64_t) { return false; }), LoadTestModelSourceFixture);
        CHECK_NOTHROW(skipped_baker.BakeFiles(skipped_rig.GetAllSourceFiles(), ""));
        CHECK(skipped_rig.Outputs.empty());
    }

    SECTION("Writes model descriptions and ModelAnimationInfo config with speed-adjusted durations")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/TEMPLATE_Anim.fo3d", "Anim 0 1 ModelFile ~Idle\nAnim 0 1 ModelFile Duplicate\nAnim 0 3 ModelFile Base\n", 50);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nInclude TEMPLATE_Anim.fo3d\nAnimSpeed 0 1 2\nAnimSpeed 0 3 4\n", 7);
        rig.AddSourceFile("Critters/NoAnim.fo3d", "Model Body.fbx\n", 8);
        rig.AddSourceFile("Critters/Readme.txt", "ignored", 100);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true, {"Idle"});

        vector<pair<string, uint64_t>> checks;
        ModelInfoBaker baker(rig.MakeContext("ArbitraryPack",
                                 [&checks](string_view path, uint64_t write_time) {
                                     checks.emplace_back(string(path), write_time);
                                     return true;
                                 }),
            LoadTestModelSourceFixture);

        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        CHECK(std::ranges::find(checks, pair<string, uint64_t> {"ModelAnimationInfo.foinfo", 50}) != checks.end());
        REQUIRE(rig.Outputs.count("ModelAnimationInfo.foinfo") == 1);
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
        CHECK(rig.Outputs.count("Critters/NoAnim.fo3d") == 1);

        string config = rig.GetOutputText("ModelAnimationInfo.foinfo");
        CHECK(config.find("[Critters/Test.fo3d]\n") != string::npos);
        CHECK(config.find("StateAnimations = 0 0\n") != string::npos);
        CHECK(config.find("ActionAnimations = 1 3\n") != string::npos);
        CHECK(config.find("DurationsMs = 500 250\n") != string::npos);
        CHECK(config.find("[Critters/NoAnim.fo3d]\n") != string::npos);
    }

    SECTION("BakesSelectableModelLinkBoundsIntoDescription")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Idle\nLayer 1 Value 2 Attach Equipment.fo3d Link Hand MoveX 3\n", 7);
        rig.AddSourceFile("Critters/Equipment.fo3d", "Model Equipment.fbx\nMoveX 2\n", 6);
        AddTestModelSource(rig, MakeTestModelSourceWithChild("Critters/Body.fbx", "Body", "Hand", 3.0f, {"Idle"}));
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModelWithChildBone("Body", "Hand"));
        AddTestModel(rig, "Critters/Equipment.fbx", "Body", true, {}, {}, {"Body"});

        ModelInfoBaker baker(rig.MakeContext("ArbitraryPack"), LoadTestModelSourceFixture);
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        const vector<uint8_t>& output = rig.Outputs.at("Critters/Test.fo3d");
        auto reader = data_reader({output.data(), output.size()});
        ReadSavedModelInfoHeader(reader);
        CHECK(ReadSavedModelInfoString(reader) == "Critters/Body.fbx");
        (void)reader.read<uint8_t>(); // DisableAnimationInterpolation
        (void)reader.read<uint8_t>(); // DisableBackwardAnim
        (void)reader.read<uint8_t>(); // ShadowDisabled
        (void)reader.read<int32_t>(); // DrawWidth
        (void)reader.read<int32_t>(); // DrawHeight
        (void)reader.read<int32_t>(); // ViewWidth
        (void)reader.read<int32_t>(); // ViewHeight
        (void)ReadSavedModelInfoString(reader); // RotationBone
        CHECK_FALSE(ReadSavedModelInfoLink(reader).Bounds);

        REQUIRE(reader.read<uint32_t>() == 1);
        SavedModelInfoLink equipment_link = ReadSavedModelInfoLink(reader);
        CHECK(equipment_link.LinkBone == "Hand");
        REQUIRE(equipment_link.Bounds);
        CHECK(IsValidModelBounds(*equipment_link.Bounds));
        CHECK(HasModelBoundsExtent(*equipment_link.Bounds));
        CHECK(equipment_link.Bounds->Max.x > 5.0f);

        // The per-clip boxes are what keeps a frame off the attachment's whole-animation envelope, so the link
        // has to carry them and each one has to stay inside the envelope it was folded into
        CHECK_FALSE(equipment_link.AnimationBounds.empty());

        for (const auto& [state_anim, action_anim, clip_bounds] : equipment_link.AnimationBounds) {
            CHECK(IsValidModelBounds(clip_bounds));
            CHECK(HasModelBoundsExtent(clip_bounds));
            CHECK(clip_bounds.Min.x >= equipment_link.Bounds->Min.x);
            CHECK(clip_bounds.Max.x <= equipment_link.Bounds->Max.x);
        }

        string config = rig.GetOutputText("ModelAnimationInfo.foinfo");
        CHECK(config.find("BoundsVersion = 2\n") != string::npos);
        CHECK(config.find("LinkBounds") == string::npos);
    }

    SECTION("Materializes model animation aliases with client lookup semantics")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", R"(Model Body.fbx
Anim 1 3 ModelFile Base
Anim 0 3 ModelFile Base
Anim 1 5 ModelFile Base
AnimSpeed 1 3 2
AnimSpeed 0 3 4
AnimSpeed 1 5 5
StateAnimEqual 0 1
ActionAnimEqual 3 5
ActionAnimEqual 5 3
ActionAnimEqual 4 6
)",
            7);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true, {"Base"});

        ModelInfoBaker baker(rig.MakeContext("ArbitraryPack"), LoadTestModelSourceFixture);
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        string config = rig.GetOutputText("ModelAnimationInfo.foinfo");
        CHECK(config.find("StateAnimations = 1 0 1 0\n") != string::npos);
        CHECK(config.find("ActionAnimations = 5 5 3 3\n") != string::npos);
        CHECK(config.find("DurationsMs = 500 500 200 200\n") != string::npos);
        CHECK(config.find("ActionAnimations = 4") == string::npos);
        CHECK(config.find("DurationsMs = 250") == string::npos);
    }

    SECTION("Rejects ModelAnimationInfo millisecond overflow from duration or speed")
    {
        TestRig duration_rig;
        AddModelInfoMetadata(duration_rig);
        duration_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Idle\n", 1);
        ModelSourceAsset duration_asset = MakeTestModelSource("Critters/Body.fbx", "Body", {"Idle"});
        duration_asset.Animations.front().Duration = std::numeric_limits<float32_t>::max();
        AddTestModelSource(duration_rig, duration_asset);

        ModelInfoBaker duration_baker(duration_rig.MakeContext("ArbitraryPack"), LoadTestModelSourceFixture);
        CHECK_THROWS_AS(duration_baker.BakeFiles(duration_rig.GetAllSourceFiles(), ""), ModelInfoBakerException);

        TestRig speed_rig;
        AddModelInfoMetadata(speed_rig);
        speed_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Idle\nAnimSpeed 0 1 1e-30\n", 1);
        AddTestModelSource(speed_rig, MakeTestModelSource("Critters/Body.fbx", "Body", {"Idle"}));

        ModelInfoBaker speed_baker(speed_rig.MakeContext("ArbitraryPack"), LoadTestModelSourceFixture);
        CHECK_THROWS_AS(speed_baker.BakeFiles(speed_rig.GetAllSourceFiles(), ""), ModelInfoBakerException);
    }

    SECTION("Rejects ModelAnimationInfo effective duration that rounds below one millisecond")
    {
        // This speed rounds the clip to a zero millisecond count, and the runtime load requires a positive
        // duration, so baking must fail rather than emit a manifest the client cannot load
        TestRig rounding_rig;
        AddModelInfoMetadata(rounding_rig);
        rounding_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Idle\nAnimSpeed 0 1 1e6\n", 1);
        AddTestModelSource(rounding_rig, MakeTestModelSource("Critters/Body.fbx", "Body", {"Idle"}));

        ModelInfoBaker rounding_baker(rounding_rig.MakeContext("ArbitraryPack"), LoadTestModelSourceFixture);
        CHECK_THROWS_AS(rounding_baker.BakeFiles(rounding_rig.GetAllSourceFiles(), ""), ModelInfoBakerException);
    }

    SECTION("Applies include replacements in model descriptions")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/TEMPLATE_Test.fo3d", "Model %ModelName%\nLayer true Value 2 Root Link %RootBone%\n", 50);
        rig.AddSourceFile("Critters/Test.fo3d", "Include TEMPLATE_Test.fo3d ModelName Body.fbx RootBone Body\n", 7);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
        REQUIRE(rig.Outputs.count("Critters/Test.fo3d") == 1);

        const vector<uint8_t>& output = rig.Outputs.at("Critters/Test.fo3d");
        auto reader = data_reader({output.data(), output.size()});
        ReadSavedModelInfoHeader(reader);
        CHECK(ReadSavedModelInfoString(reader) == "Critters/Body.fbx");
        (void)reader.read<uint8_t>(); // DisableAnimationInterpolation
        (void)reader.read<uint8_t>(); // DisableBackwardAnim
        (void)reader.read<uint8_t>(); // ShadowDisabled
        (void)reader.read<int32_t>(); // DrawWidth
        (void)reader.read<int32_t>(); // DrawHeight
        (void)reader.read<int32_t>(); // ViewWidth
        (void)reader.read<int32_t>(); // ViewHeight
        CHECK(ReadSavedModelInfoString(reader).empty());
        (void)ReadSavedModelInfoLink(reader);

        REQUIRE(reader.read<uint32_t>() == 1);
        SavedModelInfoLink root_link = ReadSavedModelInfoLink(reader);
        CHECK(root_link.Layer == 1);
        CHECK(root_link.LayerValue == 2);
        CHECK(root_link.LinkBone == "Body");
    }

    SECTION("Honors ModelAnimationInfo target filtering")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Missing.fbx\nAnim 0 1 ModelFile Idle\n", 7);

        ModelInfoBaker baker(rig.MakeContext("ArbitraryPack"), LoadTestModelSourceFixture);
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Other.foinfo"));
        CHECK(rig.Outputs.empty());
    }

    SECTION("Skips model description after second bake checker pass")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 7);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        size_t checks = 0;
        ModelInfoBaker baker(rig.MakeContext("TestPack",
                                 [&checks](string_view, uint64_t) {
                                     checks++;
                                     return checks == 1;
                                 }),
            LoadTestModelSourceFixture);

        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Critters/Test.fo3d"));
        CHECK(checks == 2);
        CHECK(rig.Outputs.empty());
    }
#endif
}

TEST_CASE("ModelInfoBakerValidations")
{
#if FO_ENABLE_3D
    using namespace BakerTests;

    SECTION("Rejects model meshes without draw bones")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", false);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing default diffuse textures")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true, {}, "Body.tga");

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing explicit textures")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nTexture 0 Missing.tga\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects texture mesh references that are not drawable")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nMesh MissingMesh Texture 0 Body.tga\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);
        rig.AddBakedFile("Critters/Body.tga", "stub");

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects out of range layer constants")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 100\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects out of range texture indexes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nTexture 100 Body.tga\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects obsolete draw sizes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nDrawSize 10 0\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects obsolete view sizes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nViewSize 10 0\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing rotation bones")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nRotationBone MissingBone\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects negative speed adjustments")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nSpeed -0.25\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing effects")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nEffect Effects/Missing.fofx\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing particle attachments")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 1\nValue 2\nAttachParticles Particles/Missing.fope Link Hand\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Hand", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing attached model descriptions")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 1\nValue 2\nAttach Missing.fo3d Link Hand\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Hand", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Accepts attached model description references")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 1\nValue 2\nAttach Child.fo3d Link Hand\n", 1);
        rig.AddSourceFile("Critters/Child.fo3d", "Model Child.fbx\n", 2);
        AddTestModel(rig, "Critters/Body.fbx", "Hand", true);
        AddTestModel(rig, "Critters/Child.fbx", "Child", true);

        ModelInfoBaker baker(rig.MakeContext(), LoadTestModelSourceFixture);
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Critters/Test.fo3d"));
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
    }

    SECTION("Accepts baked-only attached model descriptions")
    {
        TestRig child_rig;
        AddModelInfoMetadata(child_rig);
        child_rig.AddSourceFile("Critters/Child.fo3d", "Model Child.fbx\n", 1);
        AddTestModel(child_rig, "Critters/Child.fbx", "Child", true);
        REQUIRE_NOTHROW(BakeModelInfoFiles(child_rig, "Critters/Child.fo3d"));
        REQUIRE(child_rig.Outputs.count("Critters/Child.fo3d") == 1);

        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 1\nValue 2\nAttach Child.fo3d Link Hand\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Hand", true);
        rig.AddBakedFile("Critters/Child.fo3d", child_rig.Outputs.at("Critters/Child.fo3d"));
        rig.AddBakedFile("Critters/Child.fbx", MakeTestBakedModel("Child", true));

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
    }

    SECTION("Rejects direct attached models with embedded animation clips")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 1\nValue 2\nAttach AnimatedHat.fbx Link Hand\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Hand", true);
        AddTestModel(rig, "Critters/AnimatedHat.fbx", "Hat", true, {"Idle"});

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing baked model meshes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Missing.fbx\n", 1);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing animation stacks")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Run\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true, {"Idle"});

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects drawable geometry in external animation models")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 External.fbx Idle\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Root", true);
        AddTestModel(rig, "Critters/External.fbx", "Root", true, {"Idle"});

        string diagnostic = CaptureModelInfoBakingError(rig);
        CHECK(diagnostic.find("External animation model contains drawable mesh nodes") != string::npos);
        CHECK(diagnostic.find("Root") != string::npos);
        CHECK(diagnostic.find("AllowAnimationGeometry line") != string::npos);
    }

    SECTION("Requires explicit temporary exceptions for known external animation geometry")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAllowAnimationGeometry External.fbx\nAnim 0 1 External.fbx Idle\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Root", true);
        AddTestModel(rig, "Critters/External.fbx", "Root", true, {"Idle"});

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
    }

    SECTION("Resolves animation geometry exceptions like Anim paths from the final description")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Templates/TEMPLATE_Anim.fo3d", "AllowAnimationGeometry External.fbx\nAnim 0 1 External.fbx Idle\n", 2);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nInclude Templates/TEMPLATE_Anim.fo3d\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Root", true);
        AddTestModel(rig, "Critters/External.fbx", "Root", true, {"Idle"});

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
    }

    SECTION("Rejects duplicate animation geometry exceptions")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAllowAnimationGeometry External.fbx\nAllowAnimationGeometry External.fbx\nAnim 0 1 External.fbx Idle\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Root", true);
        AddTestModel(rig, "Critters/External.fbx", "Root", true, {"Idle"});

        CHECK_THROWS_WITH(BakeModelInfoFiles(rig), Catch::Matchers::ContainsSubstring("Duplicate animation-geometry exception") && Catch::Matchers::ContainsSubstring("External.fbx") && Catch::Matchers::ContainsSubstring("- 3"));
    }

    SECTION("Rejects animation geometry exceptions with duplicate resolved targets")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAllowAnimationGeometry External.fbx\nAllowAnimationGeometry Temporary/../External.fbx\nAnim 0 1 External.fbx Idle\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Root", true);
        AddTestModel(rig, "Critters/External.fbx", "Root", true, {"Idle"});

        string diagnostic = CaptureModelInfoBakingError(rig);
        CHECK(diagnostic.find("Critters/External.fbx") != string::npos);
        CHECK(diagnostic.find("keep exactly one AllowAnimationGeometry line") != string::npos);
    }

    SECTION("Rejects animation geometry exceptions for non-selected duplicate mappings")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAllowAnimationGeometry Ignored.fbx\nAnim 0 1 Selected.fbx Idle\nAnim 0 1 Ignored.fbx Idle\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Root", true);
        AddTestModel(rig, "Critters/Selected.fbx", "Root", false, {"Idle"});
        AddTestModel(rig, "Critters/Ignored.fbx", "Root", true, {"Idle"});

        string diagnostic = CaptureModelInfoBakingError(rig);
        CHECK(diagnostic.find("Critters/Ignored.fbx") != string::npos);
        CHECK(diagnostic.find("does not match a selected external Anim source") != string::npos);
    }

    SECTION("Rejects animation geometry exceptions after geometry removal")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAllowAnimationGeometry External.fbx\nAnim 0 1 External.fbx Idle\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Root", true);
        AddTestModel(rig, "Critters/External.fbx", "Root", false, {"Idle"});

        string diagnostic = CaptureModelInfoBakingError(rig);
        CHECK(diagnostic.find("Critters/External.fbx") != string::npos);
        CHECK(diagnostic.find("is stale because the selected external animation no longer contains drawable meshes") != string::npos);
    }

    SECTION("Accepts animation joints contributed to the canonical skeleton")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 Extra.fbx Idle\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Root", true);
        AddTestAnimationModelWithChild(rig, "Critters/Extra.fbx", "Root", "Weapon", 0.25f);

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
    }

    SECTION("Classifies contributed joints independently of the first contributing clip")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ExtraA.fbx Idle\nAnim 1 1 ExtraB.fbx Idle\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Root", true);
        AddTestAnimationModelWithChild(rig, "Critters/ExtraA.fbx", "Root", "UnusedHelper", 0.0f);
        AddTestAnimationModelWithChild(rig, "Critters/ExtraB.fbx", "Root", "UnusedHelper", 0.0f);

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
    }

    SECTION("Reports rest-pose divergence without rejecting the animation mapping")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 Extra.fbx Idle\n", 1);
        AddTestModelWithChildBone(rig, "Critters/Body.fbx", "Root", "Spine");
        AddTestAnimationModelWithChild(rig, "Critters/Extra.fbx", "Root", "Spine", 0.25f);

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
    }

    SECTION("Rejects out of range animation enum constants")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 999 999 ModelFile Idle\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true, {"Idle"});
        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects non-positive animation speed constants")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnimSpeed 0 1 0\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Ignores duplicate animation mappings after the first mapping")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Idle\nAnim 0 1 Missing.fbx Missing\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true, {"Idle"});

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
    }

    SECTION("Rejects a root model whose aggregate bounds leave the authored world extent")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        OverrideSetting(rig.Settings.ModelAttachmentMaxExtent, 2.0f);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        AddTestModelSource(rig, MakeTestModelSource("Critters/Body.fbx", "Body", {}));
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Body", true, {}, {}, 100.0f));

        string diagnostic;
        bool threw = false;

        try {
            BakeModelInfoFiles(rig);
        }
        catch (const ModelInfoBakerException& ex) {
            threw = true;
            diagnostic = ex.what();
        }

        REQUIRE(threw);
        CHECK(diagnostic.find("Model bounds reach beyond the authored world extent") != string::npos);
        CHECK(diagnostic.find("Critters/Test.fo3d") != string::npos);
    }

    SECTION("Rejects a targeted root model whose aggregate bounds leave the authored world extent")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        OverrideSetting(rig.Settings.ModelAttachmentMaxExtent, 2.0f);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        AddTestModelSource(rig, MakeTestModelSource("Critters/Body.fbx", "Body", {}));
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Body", true, {}, {}, 100.0f));

        string diagnostic = CaptureModelInfoBakingError(rig, "Critters/Test.fo3d");
        CHECK(diagnostic.find("Model bounds reach beyond the authored world extent") != string::npos);
        CHECK(diagnostic.find("Critters/Test.fo3d") != string::npos);
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 0);
    }

    SECTION("Rejects missing cut shapes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nCut Cut.fbx 0 MissingShape - - -\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);
        AddTestModel(rig, "Critters/Cut.fbx", "CutShape", true);

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Accepts baked-only cut meshes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nCut Cut.fbx 0 CutShape - - -\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);
        rig.AddBakedFile("Critters/Cut.fbx", MakeTestBakedModel("CutShape", true));

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
    }

    SECTION("Rejects cut meshes older than a present raw source")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nCut Cut.fbx 0 CutShape - - -\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true);
        AddTestModelSource(rig, MakeTestModelSource("Critters/Cut.fbx", "CutShape", {}), 2);
        rig.AddBakedFile("Critters/Cut.fbx", MakeTestBakedModel("CutShape", true), 1);

        string diagnostic = CaptureModelInfoBakingError(rig);
        CHECK(diagnostic.find("is older than its source") != string::npos);
        CHECK(diagnostic.find("run ModelMesh before ModelInfo") != string::npos);
    }

    SECTION("Rejects skin bone references outside the baked hierarchy")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true, {}, {}, {"MissingSkin"});

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects baked model meshes above the safe hierarchy depth")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model TooDeep.fbx\n", 1);
        AddTestModelSource(rig, MakeTestModelSource("Critters/TooDeep.fbx", "Bone0", {}));
        rig.AddBakedFile("Critters/TooDeep.fbx", MakeTestBakedModelChain(MODEL_MESH_MAX_HIERARCHY_DEPTH + 1));

        string diagnostic = CaptureModelInfoBakingError(rig);
        CHECK(diagnostic.find("Invalid baked model mesh") != string::npos);
        CHECK(diagnostic.find("hierarchy depth") != string::npos);
        CHECK(diagnostic.find(strex("{}", MODEL_MESH_MAX_HIERARCHY_DEPTH)) != string::npos);
    }

    SECTION("Rejects include graph and argument parse errors")
    {
        TestRig recursive_include_rig;
        AddModelInfoMetadata(recursive_include_rig);
        recursive_include_rig.AddSourceFile("Critters/Test.fo3d", "Include Loop.fo3d\nModel Body.fbx\n", 1);
        recursive_include_rig.AddSourceFile("Critters/Loop.fo3d", "Include Test.fo3d\n", 2);
        CHECK_THROWS_AS(BakeModelInfoFiles(recursive_include_rig), ModelInfoBakerException);

        TestRig missing_include_rig;
        AddModelInfoMetadata(missing_include_rig);
        missing_include_rig.AddSourceFile("Critters/Test.fo3d", "Include\nModel Body.fbx\n", 1);
        CHECK_THROWS_AS(BakeModelInfoFiles(missing_include_rig), ModelInfoBakerException);

        TestRig missing_include_file_rig;
        AddModelInfoMetadata(missing_include_file_rig);
        missing_include_file_rig.AddSourceFile("Critters/Test.fo3d", "Include Missing.fo3d\nModel Body.fbx\n", 1);
        CHECK_THROWS_AS(BakeModelInfoFiles(missing_include_file_rig), ModelInfoBakerException);

        TestRig unpaired_include_args_rig;
        AddModelInfoMetadata(unpaired_include_args_rig);
        unpaired_include_args_rig.AddSourceFile("Critters/Test.fo3d", "Include TEMPLATE_Test.fo3d NameOnly\nModel Body.fbx\n", 1);
        unpaired_include_args_rig.AddSourceFile("Critters/TEMPLATE_Test.fo3d", "Model %NameOnly%\n", 2);
        CHECK_THROWS_AS(BakeModelInfoFiles(unpaired_include_args_rig), ModelInfoBakerException);

        TestRig missing_token_arg_rig;
        AddModelInfoMetadata(missing_token_arg_rig);
        missing_token_arg_rig.AddSourceFile("Critters/Test.fo3d", "Model\n", 1);
        CHECK_THROWS_AS(BakeModelInfoFiles(missing_token_arg_rig), ModelInfoBakerException);
    }

    SECTION("Rejects numeric parse errors")
    {
        TestRig invalid_float_rig;
        AddModelInfoMetadata(invalid_float_rig);
        invalid_float_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nRotX nope\n", 1);
        CHECK_THROWS_AS(BakeModelInfoFiles(invalid_float_rig), ModelInfoBakerException);

        TestRig non_finite_float_rig;
        AddModelInfoMetadata(non_finite_float_rig);
        non_finite_float_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nRotX 1e999\n", 1);
        CHECK_THROWS_AS(BakeModelInfoFiles(non_finite_float_rig), ModelInfoBakerException);

        TestRig invalid_enum_rig;
        AddModelInfoMetadata(invalid_enum_rig);
        invalid_enum_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer NotAnEnum\n", 1);
        CHECK_THROWS_AS(BakeModelInfoFiles(invalid_enum_rig), ModelInfoBakerException);
    }

    SECTION("Rejects invalid cut unskin combinations")
    {
        TestRig missing_unskin_pair_rig;
        AddModelInfoMetadata(missing_unskin_pair_rig);
        missing_unskin_pair_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nCut Cut.fbx 0 CutShape Body - -\n", 1);
        AddTestModel(missing_unskin_pair_rig, "Critters/Body.fbx", "Body", true);
        AddTestModel(missing_unskin_pair_rig, "Critters/Cut.fbx", "CutShape", true);
        CHECK_THROWS_AS(BakeModelInfoFiles(missing_unskin_pair_rig), ModelInfoBakerException);

        TestRig shape_without_unskin_bones_rig;
        AddModelInfoMetadata(shape_without_unskin_bones_rig);
        shape_without_unskin_bones_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nCut Cut.fbx 0 CutShape - - CutShape\n", 1);
        AddTestModel(shape_without_unskin_bones_rig, "Critters/Body.fbx", "Body", true);
        AddTestModel(shape_without_unskin_bones_rig, "Critters/Cut.fbx", "CutShape", true);
        CHECK_THROWS_AS(BakeModelInfoFiles(shape_without_unskin_bones_rig), ModelInfoBakerException);
    }

    SECTION("Reads baked mesh edge metadata")
    {
        TestRig fallback_skin_bone_rig;
        AddModelInfoMetadata(fallback_skin_bone_rig);
        fallback_skin_bone_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        AddTestModel(fallback_skin_bone_rig, "Critters/Body.fbx", "Body", true, {}, {}, {""});
        CHECK_NOTHROW(BakeModelInfoFiles(fallback_skin_bone_rig));

        TestRig child_bone_rig;
        AddModelInfoMetadata(child_bone_rig);
        child_bone_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nRotationBone Child\n", 1);
        AddTestModelWithChildBone(child_bone_rig, "Critters/Body.fbx", "Body", "Child");
        CHECK_NOTHROW(BakeModelInfoFiles(child_bone_rig));

        TestRig mismatched_skin_offsets_rig;
        AddModelInfoMetadata(mismatched_skin_offsets_rig);
        mismatched_skin_offsets_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        AddTestModelSource(mismatched_skin_offsets_rig, MakeTestModelSource("Critters/Body.fbx", "Body", {}));
        mismatched_skin_offsets_rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModelWithMismatchedSkinOffsets("Body", "Body"));
        CHECK_THROWS_AS(BakeModelInfoFiles(mismatched_skin_offsets_rig), ModelInfoBakerException);

        TestRig truncated_mesh_rig;
        AddModelInfoMetadata(truncated_mesh_rig);
        truncated_mesh_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        AddTestModelSource(truncated_mesh_rig, MakeTestModelSource("Critters/Body.fbx", "Body", {}));
        truncated_mesh_rig.AddBakedFile("Critters/Body.fbx", vector<uint8_t> {1, 2, 3});
        CHECK_THROWS_AS(BakeModelInfoFiles(truncated_mesh_rig), ModelInfoBakerException);
    }

    SECTION("Rejects baked meshes older than their raw source")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        AddTestModelSource(rig, MakeTestModelSource("Critters/Body.fbx", "Body", {}), 2);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Body", true), 1);

        string diagnostic = CaptureModelInfoBakingError(rig);
        CHECK(diagnostic.find("is older than its source") != string::npos);
        CHECK(diagnostic.find("run ModelMesh before ModelInfo") != string::npos);
    }

    SECTION("Parses model description option tokens into saved description")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", R"(Model Body.fbx ; semicolon comment
Subset LegacySubset
Root Link Body Mesh Body Texture 0 Body.tga Effect Effects/Test.fofx
RotX 1 RotY+ 2 RotZ* 3 MoveX 4 MoveY+ 5 MoveZ* 6 Scale 2 Scale+ 0.5 ScaleX* 2 ScaleY 7 ScaleZ+ 8 Scale* 2 Speed 1.25 Speed+ 0.75 Speed* 2
DisableLayer 2-3 DisableMesh All-Body Cut Cut.fbx All CutShape Body Body ~CutShape
RotationBone Body
FastTransitionBone Body
DisableShadow
DisableAnimationInterpolation
DisableBackwardAnim
Anim 0 1 ModelFile ~iDlE
Anim 1 0 ModelFile Base
Anim 1 1 ModelFile IDLE
Anim 0 0 Anims/Extra.fbx eXtErNaL
AnimSpeed 0 1 2
AnimLayerValue 0 1 2 7
StateAnimEqual 0 0
ActionAnimEqual 1 1
Layer 1 Value 2 Root Link Body
Layer 2 Value 3 AttachParticles Particles/Test.fope Link Body
Layer 3 Value 4 Attach Hat.fbx Link Body Texture 0 Parent_Body Effect Parent_Body
)",
            1);
        AddTestModel(rig, "Critters/Body.fbx", "Body", true, {"Idle"});
        AddTestModel(rig, "Critters/Anims/Extra.fbx", "Body", false, {"External"});
        AddTestModel(rig, "Critters/Hat.fbx", "Hat", true);
        AddTestModel(rig, "Critters/Cut.fbx", "CutShape", true);
        rig.AddBakedFile("Critters/Body.tga", "stub");
        rig.AddBakedFile("Effects/Test.fofx", "stub");
        rig.AddBakedFile("Particles/Test.fope", "stub");

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
        REQUIRE(rig.Outputs.count("Critters/Test.fo3d") == 1);

        const vector<uint8_t>& output = rig.Outputs.at("Critters/Test.fo3d");
        auto reader = data_reader({output.data(), output.size()});
        ReadSavedModelInfoHeader(reader);

        CHECK(ReadSavedModelInfoString(reader) == "Critters/Body.fbx");
        CHECK(reader.read<uint8_t>() == 1);
        CHECK(reader.read<uint8_t>() == 1);
        CHECK(reader.read<uint8_t>() == 1);
        CHECK(reader.read<int32_t>() == 0);
        CHECK(reader.read<int32_t>() == 0);
        CHECK(reader.read<int32_t>() == 0);
        CHECK(reader.read<int32_t>() == 0);
        CHECK(ReadSavedModelInfoString(reader) == "Body");

        SavedModelInfoLink default_link = ReadSavedModelInfoLink(reader);
        CHECK(default_link.LinkBone.empty());
        CHECK(default_link.RotX == Catch::Approx(1.0f));
        CHECK(default_link.RotY == Catch::Approx(2.0f));
        CHECK(default_link.RotZ == Catch::Approx(3.0f));
        CHECK(default_link.MoveX == Catch::Approx(4.0f));
        CHECK(default_link.MoveY == Catch::Approx(5.0f));
        CHECK(default_link.MoveZ == Catch::Approx(6.0f));
        CHECK(default_link.ScaleX == Catch::Approx(10.0f));
        CHECK(default_link.ScaleY == Catch::Approx(14.0f));
        CHECK(default_link.ScaleZ == Catch::Approx(21.0f));
        CHECK(default_link.SpeedAdjust == Catch::Approx(4.0f));
        CHECK(default_link.DisabledLayerCount == 2);
        CHECK(default_link.DisabledMeshCount == 2);
        CHECK(default_link.TextureInfoCount == 1);
        CHECK(default_link.EffectInfoCount == 1);
        CHECK(default_link.CutInfoCount == 1);

        REQUIRE(reader.read<uint32_t>() == 3);
        SavedModelInfoLink root_link = ReadSavedModelInfoLink(reader);
        CHECK(root_link.Layer == 1);
        CHECK(root_link.LayerValue == 2);
        CHECK(root_link.LinkBone == "Body");
        CHECK(root_link.ChildName.empty());

        SavedModelInfoLink particles_link = ReadSavedModelInfoLink(reader);
        CHECK(particles_link.Layer == 2);
        CHECK(particles_link.LayerValue == 3);
        CHECK(particles_link.ChildName == "Particles/Test.fope");
        CHECK(particles_link.IsParticles);

        SavedModelInfoLink attached_link = ReadSavedModelInfoLink(reader);
        CHECK(attached_link.Layer == 3);
        CHECK(attached_link.LayerValue == 4);
        CHECK(attached_link.ChildName == "Critters/Hat.fbx");
        CHECK(!attached_link.IsParticles);
        CHECK(attached_link.TextureInfoCount == 1);
        CHECK(attached_link.EffectInfoCount == 1);

        REQUIRE(reader.read<uint32_t>() == 4);
        CHECK(reader.read<int32_t>() == 0);
        CHECK(reader.read<int32_t>() == 1);
        CHECK(ReadSavedModelInfoString(reader) == "ModelFile");
        CHECK(ReadSavedModelInfoString(reader) == "~iDlE");
        CHECK(reader.read<int32_t>() == 1);
        CHECK(reader.read<int32_t>() == 0);
        CHECK(ReadSavedModelInfoString(reader) == "ModelFile");
        CHECK(ReadSavedModelInfoString(reader) == "Base");
        CHECK(reader.read<int32_t>() == 1);
        CHECK(reader.read<int32_t>() == 1);
        CHECK(ReadSavedModelInfoString(reader) == "ModelFile");
        CHECK(ReadSavedModelInfoString(reader) == "IDLE");
        CHECK(reader.read<int32_t>() == 0);
        CHECK(reader.read<int32_t>() == 0);
        CHECK(ReadSavedModelInfoString(reader) == "Anims/Extra.fbx");
        CHECK(ReadSavedModelInfoString(reader) == "eXtErNaL");

        REQUIRE(reader.read<uint32_t>() == 1);
        CHECK(reader.read<int32_t>() == 0);
        CHECK(reader.read<int32_t>() == 1);
        CHECK(reader.read<float32_t>() == Catch::Approx(2.0f));

        REQUIRE(reader.read<uint32_t>() == 1);
        CHECK(reader.read<int32_t>() == 0);
        CHECK(reader.read<int32_t>() == 1);
        CHECK(reader.read<int32_t>() == 2);
        CHECK(reader.read<int32_t>() == 7);

        REQUIRE(reader.read<uint32_t>() == 1);
        CHECK(ReadSavedModelInfoString(reader) == "Body");

        REQUIRE(reader.read<uint32_t>() == 1);
        CHECK(reader.read<int32_t>() == 0);
        CHECK(reader.read<int32_t>() == 0);

        REQUIRE(reader.read<uint32_t>() == 1);
        CHECK(reader.read<int32_t>() == 1);
        CHECK(reader.read<int32_t>() == 1);

        uint64_t animation_rig_data_size = reader.read<uint64_t>();
        REQUIRE(animation_rig_data_size != 0);
        unique_ptr<ModelAnimationRuntimeRig> animation_rig = LoadModelAnimationRuntimeRig(reader.read_bytes(numeric_cast<size_t>(animation_rig_data_size)), "Critters/Test.fo3d", "Critters/Body.fbx", true);
        CHECK(animation_rig->GetJointCount() == 1);
        REQUIRE(animation_rig->GetBaseJointMapping().size() == 1);
        CHECK(animation_rig->GetBaseJointMapping()[0] == 0);
        REQUIRE(animation_rig->GetClipCount() == 2);
        REQUIRE(animation_rig->GetBindings().size() == 4);
        auto external_binding = animation_rig->FindBinding(0, 0);
        auto reverse_idle_binding = animation_rig->FindBinding(0, 1);
        auto base_binding = animation_rig->FindBinding(1, 0);
        auto case_binding = animation_rig->FindBinding(1, 1);
        REQUIRE(external_binding);
        REQUIRE(reverse_idle_binding);
        REQUIRE(base_binding);
        REQUIRE(case_binding);
        CHECK(animation_rig->GetClip(external_binding->ClipIndex).GetSourceFile() == "Critters/Anims/Extra.fbx");
        CHECK(animation_rig->GetClip(external_binding->ClipIndex).GetClipName() == "External");
        CHECK_FALSE(external_binding->Reversed);
        CHECK(animation_rig->GetClip(reverse_idle_binding->ClipIndex).GetSourceFile() == "Critters/Body.fbx");
        CHECK(animation_rig->GetClip(reverse_idle_binding->ClipIndex).GetClipName() == "Idle");
        CHECK(reverse_idle_binding->Reversed);
        CHECK(base_binding->ClipIndex == reverse_idle_binding->ClipIndex);
        CHECK(case_binding->ClipIndex == reverse_idle_binding->ClipIndex);
        CHECK_FALSE(base_binding->Reversed);
        CHECK_FALSE(case_binding->Reversed);

        CHECK_NOTHROW(reader.verify_end());
    }

    SECTION("Rejects parser-only model description errors")
    {
        TestRig missing_model_rig;
        AddModelInfoMetadata(missing_model_rig);
        missing_model_rig.AddSourceFile("Critters/Test.fo3d", "DrawSize 1 1\n", 1);
        CHECK_THROWS_AS(BakeModelInfoFiles(missing_model_rig), ModelInfoBakerException);

        TestRig root_without_value_rig;
        AddModelInfoMetadata(root_without_value_rig);
        root_without_value_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 1\nRoot\n", 1);
        CHECK_THROWS_AS(BakeModelInfoFiles(root_without_value_rig), ModelInfoBakerException);

        TestRig attach_without_value_rig;
        AddModelInfoMetadata(attach_without_value_rig);
        attach_without_value_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAttach Hat.fbx\n", 1);
        CHECK_THROWS_AS(BakeModelInfoFiles(attach_without_value_rig), ModelInfoBakerException);

        TestRig unknown_token_rig;
        AddModelInfoMetadata(unknown_token_rig);
        unknown_token_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nUnexpectedToken\n", 1);
        CHECK_THROWS_AS(BakeModelInfoFiles(unknown_token_rig), ModelInfoBakerException);
    }

    SECTION("Rejects parent-only texture and effect without an attached model context")
    {
        TestRig texture_rig;
        AddModelInfoMetadata(texture_rig);
        texture_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nTexture 0 Parent_Body\n", 1);
        AddTestModel(texture_rig, "Critters/Body.fbx", "Body", true);
        CHECK_THROWS_AS(BakeModelInfoFiles(texture_rig), ModelInfoBakerException);

        TestRig effect_rig;
        AddModelInfoMetadata(effect_rig);
        effect_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nEffect Parent_Body\n", 1);
        AddTestModel(effect_rig, "Critters/Body.fbx", "Body", true);
        CHECK_THROWS_AS(BakeModelInfoFiles(effect_rig), ModelInfoBakerException);
    }
#endif
}

#if FO_ENABLE_3D

// Two bones sharing one blended vertex: the exact envelope follows the blended path, while the per-bone
// envelope plan measures each bone's box separately and must stay a superset of it
static auto MakeBoundsPlanTestModel() -> ModelMeshData
{
    FO_STACK_TRACE_ENTRY();

    ModelMeshData data;
    data.RootBone = safe_alloc::make_unique<ModelMeshBoneData>();
    data.RootBone->Name = "Root";
    data.RootBone->TransformationMatrix = mat44 {1.0f};
    data.RootBone->GlobalTransformationMatrix = mat44 {1.0f};

    ModelMeshGeometryData& mesh = data.RootBone->AttachedMesh.emplace();
    mesh.SkinBoneNames = {"Root", "Arm"};
    mesh.SkinBoneOffsets = {mat44 {1.0f}, mat44 {1.0f}};

    ModelMeshVertexData& blended = mesh.Vertices.emplace_back();
    blended.Position = vec3 {2.0f, 0.0f, 0.0f};
    blended.BlendWeights[0] = 0.5f;
    blended.BlendWeights[1] = 0.5f;
    blended.BlendIndices[0] = 0.0f;
    blended.BlendIndices[1] = 1.0f;

    ModelMeshVertexData& anchor = mesh.Vertices.emplace_back();
    anchor.Position = vec3 {0.0f, 0.0f, 0.0f};
    anchor.BlendWeights[0] = 1.0f;
    anchor.BlendIndices[0] = 0.0f;

    mesh.Indices = {0, 1, 0};

    auto arm = safe_alloc::make_unique<ModelMeshBoneData>();
    arm->Name = "Arm";
    arm->TransformationMatrix = mat44 {1.0f};
    arm->GlobalTransformationMatrix = mat44 {1.0f};
    data.RootBone->Children.emplace_back(std::move(arm));

    return data;
}

// Quarter turn about Z across the clip, so the posed geometry sweeps an arc rather than sitting still
static auto MakeBoundsPlanTestAnimation() -> ModelAnimationSource
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationSource animation;
    animation.FileName = "Critters/Body.fbx";
    animation.Name = "Turn";
    animation.Duration = 1.0f;

    ModelAnimationJointSource& arm = animation.Joints.emplace_back();
    arm.OutputName = "Arm";
    arm.Hierarchy = {"Root", "Arm"};
    arm.Translation = ModelAnimationVec3Track {{0.0f, 1.0f}, {vec3 {0.0f}, vec3 {0.0f}}};
    arm.Rotation = ModelAnimationQuaternionTrack {{0.0f, 1.0f}, {glm::angleAxis(0.0f, vec3 {0.0f, 0.0f, 1.0f}), glm::angleAxis(glm::radians(90.0f), vec3 {0.0f, 0.0f, 1.0f})}};
    arm.Scale = ModelAnimationVec3Track {{0.0f, 1.0f}, {vec3 {1.0f}, vec3 {1.0f}}};
    return animation;
}

static auto ContainsModelBounds(const ModelBounds3D& outer, const ModelBounds3D& inner) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return outer.Min.x <= inner.Min.x && outer.Min.y <= inner.Min.y && outer.Min.z <= inner.Min.z && outer.Max.x >= inner.Max.x && outer.Max.y >= inner.Max.y && outer.Max.z >= inner.Max.z;
}

TEST_CASE("ModelBoundsMeasurementModes")
{
    SECTION("Per-bone envelopes cover the exact posed geometry")
    {
        ModelMeshData data = MakeBoundsPlanTestModel();
        ModelAnimationSource animation = MakeBoundsPlanTestAnimation();

        optional<ModelBounds3D> precise = ModelBoundsSampler {data, {}, ModelBoundsMeasurement::PerVertex}.CalculateAnimationBounds(animation, false);
        optional<ModelBounds3D> fast = ModelBoundsSampler {data, {}, ModelBoundsMeasurement::PerBoneEnvelope}.CalculateAnimationBounds(animation, false);

        REQUIRE(precise);
        REQUIRE(fast);
        CHECK(ContainsModelBounds(*fast, *precise));

        // The blended vertex reaches only the midpoint of the two bones while the rotated bone's envelope
        // reaches its own corner, so a collapsed difference means the boxes are no longer measured at all
        CHECK(fast->Max.y > precise->Max.y + 0.5f);
    }

    SECTION("Per-bone envelopes cover the exact geometry of a reversed clip")
    {
        ModelMeshData data = MakeBoundsPlanTestModel();
        ModelAnimationSource animation = MakeBoundsPlanTestAnimation();

        optional<ModelBounds3D> precise = ModelBoundsSampler {data, {}, ModelBoundsMeasurement::PerVertex}.CalculateAnimationBounds(animation, true);
        optional<ModelBounds3D> fast = ModelBoundsSampler {data, {}, ModelBoundsMeasurement::PerBoneEnvelope}.CalculateAnimationBounds(animation, true);

        REQUIRE(precise);
        REQUIRE(fast);
        CHECK(ContainsModelBounds(*fast, *precise));
    }

    SECTION("Per-vertex measurement follows the swept arc of a blended vertex")
    {
        ModelMeshData data = MakeBoundsPlanTestModel();
        ModelAnimationSource animation = MakeBoundsPlanTestAnimation();

        optional<ModelBounds3D> precise = ModelBoundsSampler {data, {}, ModelBoundsMeasurement::PerVertex}.CalculateAnimationBounds(animation, false);

        REQUIRE(precise);

        // Blend of the resting vertex with its rotated self: x runs from 2 down to 1, y up to 1
        CHECK(precise->Max.x == Catch::Approx(2.0f).margin(0.05));
        CHECK(precise->Max.y == Catch::Approx(1.0f).margin(0.05));
        CHECK(precise->Min.x == Catch::Approx(0.0f).margin(0.05));
        CHECK(precise->Min.z == Catch::Approx(0.0f).margin(0.05));
    }

    SECTION("A sampled bone track folds an attachment like the bone transform it carries")
    {
        ModelMeshData data = MakeBoundsPlanTestModel();
        ModelAnimationSource animation = MakeBoundsPlanTestAnimation();
        ModelBoundsSampler sampler {data, {}, ModelBoundsMeasurement::PerVertex};

        optional<vector<mat44>> track = sampler.SampleBoneTransforms(animation, false, "Arm");

        REQUIRE(track);
        REQUIRE(!track->empty());

        constexpr ModelBounds3D attachment {.Min = vec3 {1.0f, 0.0f, 0.0f}, .Max = vec3 {1.0f, 0.0f, 0.0f}};
        optional<ModelBounds3D> folded = CalculateRigidAttachmentBounds(*track, attachment, mat44 {1.0f});

        REQUIRE(folded);

        // The same track answers every attachment on the bone, so a second box must land on the same sweep
        // scaled by its own offset rather than on a re-walked hierarchy
        constexpr ModelBounds3D far_attachment {.Min = vec3 {2.0f, 0.0f, 0.0f}, .Max = vec3 {2.0f, 0.0f, 0.0f}};
        optional<ModelBounds3D> far_folded = CalculateRigidAttachmentBounds(*track, far_attachment, mat44 {1.0f});

        REQUIRE(far_folded);
        CHECK(folded->Max.x == Catch::Approx(1.0f).margin(0.01));
        CHECK(folded->Max.y == Catch::Approx(1.0f).margin(0.01));
        CHECK(far_folded->Max.x == Catch::Approx(2.0f).margin(0.01));
        CHECK(far_folded->Max.y == Catch::Approx(2.0f).margin(0.01));
    }

    SECTION("Missing bones and unmeasurable selections stay absent rather than guessed")
    {
        ModelMeshData data = MakeBoundsPlanTestModel();
        ModelAnimationSource animation = MakeBoundsPlanTestAnimation();
        ModelBoundsSampler sampler {data, {}, ModelBoundsMeasurement::PerVertex};

        CHECK_FALSE(sampler.SampleBoneTransforms(animation, false, "NoSuchBone"));
        CHECK_FALSE(sampler.GetBindPoseBoneTransform("NoSuchBone"));
        REQUIRE(sampler.GetBindPoseBoneTransform("Arm"));

        ModelBoundsSampler disabled {data, vector<string> {"Root"}, ModelBoundsMeasurement::PerVertex};
        CHECK_FALSE(disabled.IsMeasurable());
        CHECK_FALSE(disabled.CalculateStaticBounds());
        CHECK_FALSE(disabled.CalculateAnimationBounds(animation, false));

        // The hierarchy still answers a link bone, which is what a rigid attachment on a hidden mesh needs
        CHECK(disabled.SampleBoneTransforms(animation, false, "Arm"));
    }
}

#endif

FO_END_NAMESPACE

#endif
