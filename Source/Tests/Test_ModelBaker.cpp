//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "Common.h"

#if FO_ENABLE_3D
#include "3dStuff.h"
#include "ModelBounds.h"
#include "ModelBoundsCalculator.h"
#include "ModelInfoBaker.h"
#include "ModelMeshBaker.h"
#include "ModelSpriteLayout.h"
#include "Rendering.h"
#include "Test_BakerHelpers.h"
#endif

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

static void WriteTestModelBone(DataWriter& writer, string_view name, bool attached_mesh, string_view diffuse_texture, initializer_list<string_view> skin_bone_names)
{
    FO_STACK_TRACE_ENTRY();

    writer.WriteString(name);

    const mat44 matrix {1.0f};
    writer.Write<mat44>(matrix);
    writer.Write<mat44>(matrix);
    writer.Write<uint8_t>(attached_mesh ? uint8_t {1} : uint8_t {0});

    if (attached_mesh) {
        array<Vertex3D, 3> vertices {};
        vertices[0].Position = {0.0f, 0.0f, 0.0f};
        vertices[1].Position = {1.0f, 0.0f, 0.0f};
        vertices[2].Position = {0.0f, 1.0f, 0.0f};

        if (!skin_bone_names.empty()) {
            for (Vertex3D& vertex : vertices) {
                vertex.BlendWeights[0] = 1.0f;
                vertex.BlendIndices[0] = 0.0f;
            }
        }

        constexpr array<vindex_t, 3> indices {0, 1, 2};
        writer.Write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
        writer.WriteObjectArray(const_span<Vertex3D> {vertices});
        writer.Write<uint32_t>(numeric_cast<uint32_t>(indices.size()));
        writer.WriteObjectArray(const_span<vindex_t> {indices});
        writer.WriteString(diffuse_texture);

        writer.Write<uint32_t>(numeric_cast<uint32_t>(skin_bone_names.size())); // Skin bones
        for (string_view skin_bone_name : skin_bone_names) {
            writer.WriteString(skin_bone_name);
        }

        writer.Write<uint32_t>(numeric_cast<uint32_t>(skin_bone_names.size())); // Skin bone offsets
        for (size_t i = 0; i < skin_bone_names.size(); i++) {
            writer.Write<mat44>(matrix);
        }
    }

    writer.Write<uint32_t>(uint32_t {0}); // Children
}

static auto MakeTestBakedModel(string_view file_name, string_view root_bone, bool attached_mesh, initializer_list<string_view> anim_names, string_view diffuse_texture = {}, initializer_list<string_view> skin_bone_names = {}, initializer_list<string_view> animation_output_bones = {}, initializer_list<string_view> animation_hierarchy_bones = {}, float32_t animation_duration = 1.0f) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    auto writer = DataWriter(data);

    WriteTestModelBone(writer, root_bone, attached_mesh, diffuse_texture, skin_bone_names);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(anim_names.size()));

    for (string_view anim_name : anim_names) {
        writer.WriteString(file_name);
        writer.WriteString(anim_name);
        writer.Write<float32_t>(animation_duration);

        writer.Write<uint32_t>(animation_hierarchy_bones.size() == 0 ? uint32_t {0} : uint32_t {1}); // Bones hierarchy
        if (animation_hierarchy_bones.size() != 0) {
            writer.Write<uint32_t>(numeric_cast<uint32_t>(animation_hierarchy_bones.size()));
            for (string_view bone_name : animation_hierarchy_bones) {
                writer.WriteString(bone_name);
            }
        }

        writer.Write<uint32_t>(numeric_cast<uint32_t>(animation_output_bones.size())); // Bone outputs
        for (string_view bone_name : animation_output_bones) {
            writer.WriteString(bone_name);
            writer.Write<uint32_t>(uint32_t {0}); // Position times
            writer.Write<uint32_t>(uint32_t {0}); // Rotation times
            writer.Write<uint32_t>(uint32_t {0}); // Scale times
        }
    }

    return data;
}

static auto MakeTestBakedModelWithChildBone(string_view root_bone, string_view child_bone) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    auto writer = DataWriter(data);

    writer.WriteString(root_bone);

    const mat44 matrix {1.0f};
    writer.Write<mat44>(matrix);
    writer.Write<mat44>(matrix);
    writer.Write<uint8_t>(uint8_t {1});

    array<Vertex3D, 3> vertices {};
    vertices[0].Position = {0.0f, 0.0f, 0.0f};
    vertices[1].Position = {1.0f, 0.0f, 0.0f};
    vertices[2].Position = {0.0f, 1.0f, 0.0f};
    constexpr array<vindex_t, 3> indices {0, 1, 2};
    writer.Write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
    writer.WriteObjectArray(const_span<Vertex3D> {vertices});
    writer.Write<uint32_t>(numeric_cast<uint32_t>(indices.size()));
    writer.WriteObjectArray(const_span<vindex_t> {indices});
    writer.WriteString({});
    writer.Write<uint32_t>(uint32_t {0}); // Skin bones
    writer.Write<uint32_t>(uint32_t {0}); // Skin bone offsets
    writer.Write<uint32_t>(uint32_t {1}); // Children

    WriteTestModelBone(writer, child_bone, false, {}, {});
    writer.Write<uint32_t>(uint32_t {0}); // Animations

    return data;
}

static auto MakeTestBakedModelWithMismatchedSkinOffsets(string_view root_bone, string_view skin_bone) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    auto writer = DataWriter(data);

    writer.WriteString(root_bone);

    const mat44 matrix {1.0f};
    writer.Write<mat44>(matrix);
    writer.Write<mat44>(matrix);
    writer.Write<uint8_t>(uint8_t {1});
    writer.Write<uint32_t>(uint32_t {0}); // Vertices
    writer.Write<uint32_t>(uint32_t {0}); // Indices
    writer.WriteString({});
    writer.Write<uint32_t>(uint32_t {1}); // Skin bones
    writer.WriteString(skin_bone);
    writer.Write<uint32_t>(uint32_t {0}); // Skin bone offsets
    writer.Write<uint32_t>(uint32_t {0}); // Children
    writer.Write<uint32_t>(uint32_t {0}); // Animations

    return data;
}

static auto MakeTestBakedAnimatedTriangle(bool overflowing_scale = false) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    auto writer = DataWriter(data);

    writer.WriteString("Root");

    const mat44 identity {1.0f};
    writer.Write<mat44>(identity);
    writer.Write<mat44>(identity);
    writer.Write<uint8_t>(uint8_t {1});

    vector<Vertex3D> vertices(4);
    vertices[0].Position = vec3 {1.0f, 2.0f, 3.0f};
    vertices[0].BlendWeights[0] = 1.0f;
    vertices[1].Position = vec3 {-1.0f, 0.0f, 0.0f};
    vertices[1].BlendWeights[0] = 1.0f;
    vertices[2].Position = vec3 {0.0f, 0.0f, 0.0f};
    vertices[2].BlendWeights[0] = 1.0f;
    vertices[3].Position = vec3 {1000.0f, 1000.0f, 1000.0f}; // Unreferenced vertices must not affect draw bounds.
    vertices[3].BlendWeights[0] = 1.0f;
    writer.Write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
    writer.WriteObjectVector(vertices);
    const vector<vindex_t> indices {vindex_t {0}, vindex_t {1}, vindex_t {2}};
    writer.Write<uint32_t>(numeric_cast<uint32_t>(indices.size()));
    writer.WriteObjectVector(indices);
    writer.WriteString({});
    writer.Write<uint32_t>(uint32_t {1}); // Skin bones
    writer.WriteString({}); // Empty name resolves to the mesh owner bone.
    writer.Write<uint32_t>(uint32_t {1}); // Skin bone offsets
    writer.Write<mat44>(identity);
    writer.Write<uint32_t>(uint32_t {0}); // Children

    writer.Write<uint32_t>(uint32_t {1}); // Animations
    writer.WriteString("Body.fbx");
    writer.WriteString("Move");
    writer.Write<float32_t>(1.0f);
    writer.Write<uint32_t>(uint32_t {1}); // Bone hierarchies
    writer.Write<uint32_t>(uint32_t {1});
    writer.WriteString("Root");
    writer.Write<uint32_t>(uint32_t {1}); // Bone outputs
    writer.WriteString("Root");

    const vector<float32_t> times {0.0f, 1.0f};
    const vector<vec3> scales = overflowing_scale ? vector<vec3> {vec3 {std::numeric_limits<float32_t>::max()}, vec3 {std::numeric_limits<float32_t>::max()}} : vector<vec3> {vec3 {1.0f}, vec3 {2.0f, 1.0f, 1.0f}};
    const vector<quaternion> rotations {quaternion {1.0f, 0.0f, 0.0f, 0.0f}, quaternion {1.0f, 0.0f, 0.0f, 0.0f}};
    const vector<vec3> translations {vec3 {10.0f, 20.0f, 30.0f}, vec3 {12.0f, 20.0f, 30.0f}};

    writer.Write<uint32_t>(numeric_cast<uint32_t>(times.size()));
    writer.WriteObjectVector(times);
    writer.WriteObjectVector(scales);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(times.size()));
    writer.WriteObjectVector(times);
    writer.WriteObjectVector(rotations);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(times.size()));
    writer.WriteObjectVector(times);
    writer.WriteObjectVector(translations);

    return data;
}

static auto MakeTestBakedWeightedTriangle() -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    auto writer = DataWriter(data);
    const mat44 identity {1.0f};

    writer.WriteString("Root");
    writer.Write<mat44>(identity);
    writer.Write<mat44>(identity);
    writer.Write<uint8_t>(uint8_t {1});

    array<Vertex3D, 3> vertices {};
    vertices[0].Position = {0.0f, 0.0f, 0.0f};
    vertices[1].Position = {1.0f, 0.0f, 0.0f};
    vertices[2].Position = {0.0f, 1.0f, 0.0f};

    for (Vertex3D& vertex : vertices) {
        vertex.BlendIndices[0] = 0.0f;
        vertex.BlendIndices[1] = 1.0f;
        vertex.BlendWeights[0] = 0.99f;
        vertex.BlendWeights[1] = 0.01f;
    }

    constexpr array<vindex_t, 3> indices {0, 1, 2};
    writer.Write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
    writer.WriteObjectArray(const_span<Vertex3D> {vertices});
    writer.Write<uint32_t>(numeric_cast<uint32_t>(indices.size()));
    writer.WriteObjectArray(const_span<vindex_t> {indices});
    writer.WriteString({});
    writer.Write<uint32_t>(uint32_t {2});
    writer.WriteString("Root");
    writer.WriteString("Child");
    writer.Write<uint32_t>(uint32_t {2});
    writer.Write<mat44>(identity);
    writer.Write<mat44>(identity);

    writer.Write<uint32_t>(uint32_t {1});
    writer.WriteString("Child");
    const mat44 child_transform = glm::translate(identity, vec3 {100.0f, 0.0f, 0.0f});
    writer.Write<mat44>(child_transform);
    writer.Write<mat44>(child_transform);
    writer.Write<uint8_t>(uint8_t {0});
    writer.Write<uint32_t>(uint32_t {0});

    writer.Write<uint32_t>(uint32_t {0});
    return data;
}

static auto MakeTestModelAnimation(HashResolver& hash_resolver) -> ModelAnimation
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    auto writer = DataWriter(data);
    writer.WriteString("Test.fbx");
    writer.WriteString("Move");
    writer.Write<float32_t>(1.0f);
    writer.Write<uint32_t>(uint32_t {0}); // Bone hierarchies
    writer.Write<uint32_t>(uint32_t {1}); // Bone outputs
    writer.WriteString("Animated");

    const array<float32_t, 1> times {0.0f};
    const array<vec3, 1> scales {vec3 {1.0f}};
    const array<quaternion, 1> rotations {quaternion {1.0f, 0.0f, 0.0f, 0.0f}};
    const array<vec3, 1> translations {vec3 {10.0f, 0.0f, 0.0f}};
    writer.Write<uint32_t>(numeric_cast<uint32_t>(times.size()));
    writer.WriteObjectArray(const_span<float32_t> {times});
    writer.WriteObjectArray(const_span<vec3> {scales});
    writer.Write<uint32_t>(numeric_cast<uint32_t>(times.size()));
    writer.WriteObjectArray(const_span<float32_t> {times});
    writer.WriteObjectArray(const_span<quaternion> {rotations});
    writer.Write<uint32_t>(numeric_cast<uint32_t>(times.size()));
    writer.WriteObjectArray(const_span<float32_t> {times});
    writer.WriteObjectArray(const_span<vec3> {translations});

    auto reader = DataReader({data.data(), data.size()});
    ModelAnimation animation;
    animation.Load(reader, hash_resolver);
    reader.VerifyEnd();
    return animation;
}

static void AddModelInfoMetadata(BakerTests::TestRig& rig)
{
    FO_STACK_TRACE_ENTRY();

    rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeEmptyMetadataBlob());
}

static void BakeModelInfoFiles(BakerTests::TestRig& rig)
{
    FO_STACK_TRACE_ENTRY();

    ModelInfoBaker info_baker(rig.MakeContext());
    info_baker.BakeFiles(rig.GetAllSourceFiles(), "");
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
};

static auto ReadSavedModelInfoString(DataReader& reader) -> string
{
    FO_STACK_TRACE_ENTRY();

    const uint32_t len = reader.Read<uint32_t>();
    const_span<uint8_t> str_bytes = reader.ReadBytes(len);
    return !str_bytes.empty() ? string(reinterpret_cast<const char*>(str_bytes.data()), len) : string {};
}

static void SkipSavedModelInfoCut(DataReader& reader)
{
    FO_STACK_TRACE_ENTRY();

    (void)ReadSavedModelInfoString(reader);

    const uint32_t layer_count = reader.Read<uint32_t>();
    (void)reader.ReadBytes(numeric_cast<size_t>(layer_count) * sizeof(int32_t));

    const uint32_t shape_count = reader.Read<uint32_t>();
    for (uint32_t i = 0; i < shape_count; i++) {
        (void)ReadSavedModelInfoString(reader);
    }

    (void)ReadSavedModelInfoString(reader);
    (void)ReadSavedModelInfoString(reader);
    (void)ReadSavedModelInfoString(reader);
    (void)reader.Read<uint8_t>();
}

static auto ReadSavedModelInfoLink(DataReader& reader) -> SavedModelInfoLink
{
    FO_STACK_TRACE_ENTRY();

    SavedModelInfoLink link;
    link.Layer = reader.Read<int32_t>();
    link.LayerValue = reader.Read<int32_t>();
    link.LinkBone = ReadSavedModelInfoString(reader);
    link.ChildName = ReadSavedModelInfoString(reader);
    link.IsParticles = reader.Read<uint8_t>() != 0;
    link.RotX = reader.Read<float32_t>();
    link.RotY = reader.Read<float32_t>();
    link.RotZ = reader.Read<float32_t>();
    link.MoveX = reader.Read<float32_t>();
    link.MoveY = reader.Read<float32_t>();
    link.MoveZ = reader.Read<float32_t>();
    link.ScaleX = reader.Read<float32_t>();
    link.ScaleY = reader.Read<float32_t>();
    link.ScaleZ = reader.Read<float32_t>();
    link.SpeedAdjust = reader.Read<float32_t>();

    link.DisabledLayerCount = reader.Read<uint32_t>();
    (void)reader.ReadBytes(numeric_cast<size_t>(link.DisabledLayerCount) * sizeof(int32_t));

    link.DisabledMeshCount = reader.Read<uint32_t>();
    for (uint32_t i = 0; i < link.DisabledMeshCount; i++) {
        (void)ReadSavedModelInfoString(reader);
    }

    link.TextureInfoCount = reader.Read<uint32_t>();
    for (uint32_t i = 0; i < link.TextureInfoCount; i++) {
        (void)ReadSavedModelInfoString(reader);
        (void)ReadSavedModelInfoString(reader);
        (void)reader.Read<int32_t>();
    }

    link.EffectInfoCount = reader.Read<uint32_t>();
    for (uint32_t i = 0; i < link.EffectInfoCount; i++) {
        (void)ReadSavedModelInfoString(reader);
        (void)ReadSavedModelInfoString(reader);
    }

    link.CutInfoCount = reader.Read<uint32_t>();
    for (uint32_t i = 0; i < link.CutInfoCount; i++) {
        SkipSavedModelInfoCut(reader);
    }

    return link;
}

struct BakedModelMeshSummary
{
    uint32_t Bones {};
    uint32_t AttachedMeshes {};
    uint32_t Vertices {};
    uint32_t Indices {};
    uint32_t Animations {};
    uint32_t SkinBones {};
    string DiffuseTexture {};
    optional<Vertex3D> FirstVertex {};
};

static void SkipBakedModelMeshPayload(DataReader& reader, BakedModelMeshSummary& summary)
{
    FO_STACK_TRACE_ENTRY();

    const uint32_t vertex_count = reader.Read<uint32_t>();
    summary.Vertices += vertex_count;
    const_span<uint8_t> vertices = reader.ReadBytes(numeric_cast<size_t>(vertex_count) * sizeof(Vertex3D));
    if (vertex_count != 0 && !summary.FirstVertex) {
        Vertex3D first_vertex;
        MemCopy(&first_vertex, vertices.data(), sizeof(first_vertex));
        summary.FirstVertex = first_vertex;
    }

    const uint32_t index_count = reader.Read<uint32_t>();
    summary.Indices += index_count;
    (void)reader.ReadBytes(numeric_cast<size_t>(index_count) * sizeof(vindex_t));

    summary.DiffuseTexture = ReadSavedModelInfoString(reader);

    const uint32_t skin_bone_count = reader.Read<uint32_t>();
    summary.SkinBones += skin_bone_count;
    for (uint32_t i = 0; i < skin_bone_count; i++) {
        (void)ReadSavedModelInfoString(reader);
    }

    const uint32_t skin_bone_offset_count = reader.Read<uint32_t>();
    (void)reader.ReadBytes(numeric_cast<size_t>(skin_bone_offset_count) * sizeof(mat44));
}

static void ReadBakedModelMeshSummaryBone(DataReader& reader, BakedModelMeshSummary& summary)
{
    FO_STACK_TRACE_ENTRY();

    (void)ReadSavedModelInfoString(reader);
    summary.Bones++;

    (void)reader.ReadBytes(sizeof(mat44));
    (void)reader.ReadBytes(sizeof(mat44));

    if (reader.Read<uint8_t>() != 0) {
        summary.AttachedMeshes++;
        SkipBakedModelMeshPayload(reader, summary);
    }

    const uint32_t children_count = reader.Read<uint32_t>();
    for (uint32_t i = 0; i < children_count; i++) {
        ReadBakedModelMeshSummaryBone(reader, summary);
    }
}

static auto ReadBakedModelMeshSummary(const vector<uint8_t>& data) -> BakedModelMeshSummary
{
    FO_STACK_TRACE_ENTRY();

    auto reader = DataReader({data.data(), data.size()});
    BakedModelMeshSummary summary;

    ReadBakedModelMeshSummaryBone(reader, summary);
    summary.Animations = reader.Read<uint32_t>();

    CHECK_NOTHROW(reader.VerifyEnd());
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
#endif

TEST_CASE("ModelBakers")
{
#if FO_ENABLE_3D
    using namespace BakerTests;

    TestRig rig;
    AddModelInfoMetadata(rig);

    const auto bakers = MakeRequestedBakers({string(ModelMeshBaker::NAME), string(ModelInfoBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 2);
    CHECK(bakers[0]->GetName() == ModelMeshBaker::NAME);
    CHECK(bakers[0]->GetOrder() == 4);
    CHECK(bakers[1]->GetName() == ModelInfoBaker::NAME);
    CHECK(bakers[1]->GetOrder() == 5);
    CHECK_NOTHROW(bakers[0]->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));
    CHECK_NOTHROW(bakers[1]->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));

    rig.AddSourceFile("Critters/TEMPLATE_Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Base\n", 2);
    rig.AddSourceFile("Critters/Test.fo3d", "Include TEMPLATE_Test.fo3d\nLayer 1\nValue 2\nAttach Hat.fbx Link Hand RotX 15 Texture 0 Hat.tga\n", 1);
    rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Hand", true, {"Base"}));
    rig.AddBakedFile("Critters/Hat.fbx", MakeTestBakedModel("Critters/Hat.fbx", "Hat", true, {}));
    rig.AddBakedFile("Critters/Hat.tga", "stub");

    ModelMeshBaker mesh_baker(rig.MakeContext());
    mesh_baker.BakeFiles(rig.GetAllSourceFiles(), "");
    CHECK(rig.Outputs.empty());

    ModelInfoBaker info_baker(rig.MakeContext());
    info_baker.BakeFiles(rig.GetAllSourceFiles(), "");

    REQUIRE(rig.Outputs.count("Critters/Test.fo3d") == 1);

    auto reader = DataReader({rig.Outputs.at("Critters/Test.fo3d").data(), rig.Outputs.at("Critters/Test.fo3d").size()});
    const uint32_t model_name_len = reader.Read<uint32_t>();
    string model_name;
    model_name.resize(model_name_len);
    reader.ReadStringBytes(model_name);
    CHECK(model_name == "Critters/Body.fbx");
    CHECK(rig.Outputs.count("Critters/TEMPLATE_Test.fo3d") == 0);

    ModelInfoBaker skipped_info_baker(rig.MakeContext("TestPack", [](string_view, uint64_t) { return false; }));
    CHECK_NOTHROW(skipped_info_baker.BakeFiles(rig.GetAllSourceFiles(), ""));
    CHECK(rig.Outputs.empty());

    TestRig skip_rig;
    skip_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
    ModelInfoBaker skipped_without_metadata_baker(skip_rig.MakeContext("TestPack", [](string_view, uint64_t) { return false; }));
    CHECK_NOTHROW(skipped_without_metadata_baker.BakeFiles(skip_rig.GetAllSourceFiles(), ""));
    CHECK(skip_rig.Outputs.empty());

    TestRig invalid_rig;
    AddModelInfoMetadata(invalid_rig);
    invalid_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 1\nValue 2\nAttach Hat.fbx Link MissingBone\n", 1);
    invalid_rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Hand", true, {}));
    invalid_rig.AddBakedFile("Critters/Hat.fbx", MakeTestBakedModel("Critters/Hat.fbx", "Hat", true, {}));

    ModelInfoBaker invalid_info_baker(invalid_rig.MakeContext());
    CHECK_THROWS_AS(invalid_info_baker.BakeFiles(invalid_rig.GetAllSourceFiles(), ""), ModelInfoBakerException);
#endif
}

TEST_CASE("ModelBoneLookup")
{
#if FO_ENABLE_3D
    HashStorage hash_storage;
    const hstring root_name = hash_storage.ToHashedString("Root");
    const hstring child_name = hash_storage.ToHashedString("Child");
    const hstring missing_name = hash_storage.ToHashedString("Missing");

    ModelBone root;
    root.Name = root_name;
    root.Children.emplace_back(SafeAlloc::MakeUnique<ModelBone>());
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
        const BakedModelMeshSummary summary = ReadBakedModelMeshSummary(rig.Outputs.at("Models/Triangle.obj"));
        CHECK(summary.AttachedMeshes == 1);
        CHECK(summary.Vertices == 3);
        CHECK(summary.Indices == 3);
        CHECK(summary.Animations == 0);
    }

    SECTION("Bakes a minimal OBJ mesh")
    {
        TestRig rig;
        rig.AddSourceFile("Models/Triangle.obj", MakeMinimalObjMesh("Body"), 9);

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/Triangle.obj"));

        REQUIRE(rig.Outputs.count("Models/Triangle.obj") == 1);
        const BakedModelMeshSummary summary = ReadBakedModelMeshSummary(rig.Outputs.at("Models/Triangle.obj"));
        CHECK(summary.Bones >= 1);
        CHECK(summary.AttachedMeshes == 1);
        CHECK(summary.Vertices == 3);
        CHECK(summary.Indices == 3);
        CHECK(summary.Animations == 0);
    }

    SECTION("Bakes position-only OBJ mesh with default vertex fields")
    {
        TestRig rig;
        rig.AddSourceFile("Models/PositionOnly.obj", MakeMinimalPositionOnlyObjMesh("PlainBody"), 9);

        ModelMeshBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Models/PositionOnly.obj"));

        REQUIRE(rig.Outputs.count("Models/PositionOnly.obj") == 1);
        const BakedModelMeshSummary summary = ReadBakedModelMeshSummary(rig.Outputs.at("Models/PositionOnly.obj"));
        CHECK(summary.Bones >= 1);
        CHECK(summary.AttachedMeshes == 1);
        CHECK(summary.Vertices == 3);
        CHECK(summary.Indices == 3);
        CHECK(summary.Animations == 0);
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
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {"Idle"}));

        vector<pair<string, uint64_t>> checks;
        ModelInfoBaker baker(rig.MakeContext("TestPack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return true;
        }));

        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        REQUIRE(checks.size() == 3);
        CHECK(checks[0] == pair<string, uint64_t> {"ModelAnimInfo.foinfo", 50});
        CHECK(checks[1] == pair<string, uint64_t> {"Critters/Test.fo3d", 50});
        CHECK(checks[2] == pair<string, uint64_t> {"Critters/Test.fo3d", 50});
        CHECK(rig.Outputs.count("ModelAnimInfo.foinfo") == 1);
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
        CHECK(rig.Outputs.count("Critters/TEMPLATE_Anim.fo3d") == 0);
    }

    SECTION("Skips non fo3d, template and missing target files")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/TEMPLATE_Test.fo3d", "Model Body.fbx\n", 1);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Missing.fbx\n", 2);

        ModelInfoBaker baker(rig.MakeContext());
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

        ModelInfoBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        CHECK(rig.Outputs.empty());
    }

    SECTION("Bakes a specific model description target")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Other.fo3d", "Model Missing.fbx\n", 10);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 20);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        ModelInfoBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Critters/Test.fo3d"));

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
    }

    SECTION("Returns cleanly for empty ModelAnimInfo input and skipped output")
    {
        TestRig empty_rig;
        AddModelInfoMetadata(empty_rig);
        empty_rig.AddSourceFile("Critters/TEMPLATE_Test.fo3d", "Model Body.fbx\n", 1);

        ModelInfoBaker empty_baker(empty_rig.MakeContext("ArbitraryPack"));
        CHECK_NOTHROW(empty_baker.BakeFiles(empty_rig.GetAllSourceFiles(), ""));
        CHECK(empty_rig.Outputs.empty());

        TestRig skipped_rig;
        AddModelInfoMetadata(skipped_rig);
        skipped_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Idle\n", 10);
        skipped_rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {"Idle"}));

        ModelInfoBaker skipped_baker(skipped_rig.MakeContext("ArbitraryPack", [](string_view, uint64_t) { return false; }));
        CHECK_NOTHROW(skipped_baker.BakeFiles(skipped_rig.GetAllSourceFiles(), ""));
        CHECK(skipped_rig.Outputs.empty());
    }

    SECTION("Writes model descriptions and ModelAnimInfo config with speed-adjusted durations")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/TEMPLATE_Anim.fo3d", "Anim 0 1 ModelFile ~Idle\nAnim 0 1 ModelFile Duplicate\nAnim 0 3 ModelFile Base\n", 50);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nInclude TEMPLATE_Anim.fo3d\nAnimSpeed 0 1 2\nAnimSpeed 0 3 4\n", 7);
        rig.AddSourceFile("Critters/NoAnim.fo3d", "Model Body.fbx\n", 8);
        rig.AddSourceFile("Critters/Body.fbx", "referenced animation source", 170);
        rig.AddSourceFile("Critters/Readme.txt", "ignored", 100);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {"Idle"}, {}, {}, {}, {"Body"}));

        vector<pair<string, uint64_t>> checks;
        ModelInfoBaker baker(rig.MakeContext("ArbitraryPack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return true;
        }));

        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        CHECK(std::ranges::find(checks, pair<string, uint64_t> {"ModelAnimInfo.foinfo", 170}) != checks.end());
        REQUIRE(rig.Outputs.count("ModelAnimInfo.foinfo") == 1);
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
        CHECK(rig.Outputs.count("Critters/NoAnim.fo3d") == 1);

        const string config = rig.GetOutputText("ModelAnimInfo.foinfo");
        CHECK(config.find("[Critters/Test.fo3d]\n") != string::npos);
        CHECK(config.find("BoundsVersion = 2\n") != string::npos);
        CHECK(config.find("ModelBoundsMinX = ") != string::npos);
        CHECK(config.find("StateAnims = 0 0\n") != string::npos);
        CHECK(config.find("ActionAnims = 1 3\n") != string::npos);
        CHECK(config.find("DurationsMs = 500 250\n") != string::npos);
        CHECK(config.find("[Critters/NoAnim.fo3d]\n") != string::npos);
    }

    SECTION("Writes versioned root-space animation bounds without changing duration arrays")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Move\n", 7);
        rig.AddSourceFile("Critters/Body.fbx", "source animation dependency", 70);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedAnimatedTriangle());

        auto context = rig.MakeContext("ModelAnimInfo");
        const auto report = SafeAlloc::MakeShared<BakingReport>(make_ptr(static_cast<const BakingSettings*>(&rig.Settings)));
        context->Report = report;

        ModelInfoBaker baker(context);
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        const string config = rig.GetOutputText("ModelAnimInfo.foinfo");
        CHECK(config.find("StateAnims = 0\n") != string::npos);
        CHECK(config.find("ActionAnims = 1\n") != string::npos);
        CHECK(config.find("DurationsMs = 1000\n") != string::npos);
        CHECK(config.find("BoundsVersion = 2\n") != string::npos);
        CHECK(config.find("ModelBoundsMinX = 8.967") != string::npos);
        CHECK(config.find("ModelBoundsMaxX = 14.033") != string::npos);
        CHECK(config.find("ModelBoundsMaxZ =") != string::npos);
        CHECK(config.find("ViewBoundsMinX = 8.967") != string::npos);
        CHECK(config.find("ViewBoundsMaxX = 14.033") != string::npos);
        CHECK(config.find("BoundsStateAnims = 0\n") != string::npos);
        CHECK(config.find("BoundsActionAnims = 1\n") != string::npos);
        CHECK(config.find("BoundsMinX =") != string::npos);
        CHECK(config.find("BoundsMaxZ =") != string::npos);

        const string report_text = report->Serialize();
        CHECK(report_text.find("\"modelSections\": 1") != string::npos);
        CHECK(report_text.find("\"modelBounds\": 1") != string::npos);
        CHECK(report_text.find("\"animationEntries\": 1") != string::npos);
        CHECK(report_text.find("\"animationBounds\": 1") != string::npos);
        CHECK(report_text.find("\"animationBoundsOmitted\": 0") != string::npos);
        CHECK(report_text.find("\"boundsCalculations\": 1") != string::npos);
        CHECK(report_text.find("\"boundsCacheHits\": 0") != string::npos);
        CHECK(report_text.find("\"sectionsWithBounds\": 1") != string::npos);
        CHECK(report_text.find("\"sectionsWithoutBounds\": 0") != string::npos);
        CHECK(report_text.find("\"viewBoundsIdle\": 1") != string::npos);
        CHECK(report_text.find("\"viewBoundsFallback\": 0") != string::npos);
        CHECK(report_text.find("\"animationBoundsMaxExtent\"") != string::npos);
        CHECK(report_text.find("\"value\": \"5-10\"") != string::npos);
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
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {"Base"}));

        ModelInfoBaker baker(rig.MakeContext("ArbitraryPack"));
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        const string config = rig.GetOutputText("ModelAnimInfo.foinfo");
        CHECK(config.find("StateAnims = 1 0 1 0\n") != string::npos);
        CHECK(config.find("ActionAnims = 5 5 3 3\n") != string::npos);
        CHECK(config.find("DurationsMs = 500 500 200 200\n") != string::npos);
        CHECK(config.find("ActionAnims = 4") == string::npos);
        CHECK(config.find("DurationsMs = 250") == string::npos);
    }

    SECTION("Writes a bounds-only section for a model without animation entries")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/NoAnim.fo3d", "Model Body.fbx\n", 8);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        ModelInfoBaker baker(rig.MakeContext("ArbitraryPack"));
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        const string config = rig.GetOutputText("ModelAnimInfo.foinfo");
        CHECK(config.find("[Critters/NoAnim.fo3d]\n") != string::npos);
        CHECK(config.find("BoundsVersion = 2\n") != string::npos);
        CHECK(config.find("ModelBoundsMinX =") != string::npos);
        CHECK(config.find("ViewBoundsMaxZ =") != string::npos);

        // A model with no entries has nothing to time and no per-animation bounds, so the section carries the
        // static bounds alone. The unanchored substrings also cover the BoundsStateAnims/BoundsActionAnims keys.
        CHECK(config.find("StateAnims =") == string::npos);
        CHECK(config.find("ActionAnims =") == string::npos);
        CHECK(config.find("DurationsMs =") == string::npos);
        CHECK(config.find("\nBoundsMinX =") == string::npos);
    }

    SECTION("Writes animation bounds when every animation entry is aliased away")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", R"(Model Body.fbx
Anim 0 1 ModelFile Idle
StateAnimEqual 0 2
)",
            7);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {"Idle"}));

        ModelInfoBaker baker(rig.MakeContext("ArbitraryPack"));
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

        const string config = rig.GetOutputText("ModelAnimInfo.foinfo");
        CHECK(config.find("[Critters/Test.fo3d]\n") != string::npos);

        // State 0 aliases away to 2 and nothing aliases back to it, so no input pair resolves to a duration.
        // The leading newline keeps these from matching the Bounds* keys below.
        CHECK(config.find("\nStateAnims =") == string::npos);
        CHECK(config.find("\nActionAnims =") == string::npos);
        CHECK(config.find("DurationsMs =") == string::npos);

        // Bounds come from the raw entry rather than alias materialization, and the client hard-requires them
        CHECK(config.find("BoundsStateAnims = 0\n") != string::npos);
        CHECK(config.find("BoundsActionAnims = 1\n") != string::npos);
        CHECK(config.find("\nBoundsMinX =") != string::npos);
        CHECK(config.find("\nBoundsMaxZ =") != string::npos);
    }

    SECTION("Applies include replacements in model descriptions")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/TEMPLATE_Test.fo3d", "Model %ModelName%\nLayer true Value 2 Root Link %RootBone%\n", 50);
        rig.AddSourceFile("Critters/Test.fo3d", "Include TEMPLATE_Test.fo3d ModelName Body.fbx RootBone Body\n", 7);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
        REQUIRE(rig.Outputs.count("Critters/Test.fo3d") == 1);

        const vector<uint8_t>& output = rig.Outputs.at("Critters/Test.fo3d");
        auto reader = DataReader({output.data(), output.size()});
        CHECK(ReadSavedModelInfoString(reader) == "Critters/Body.fbx");
        (void)reader.Read<uint8_t>(); // DisableAnimationInterpolation
        (void)reader.Read<uint8_t>(); // DisableBackwardAnim
        (void)reader.Read<uint8_t>(); // ShadowDisabled
        CHECK(ReadSavedModelInfoString(reader).empty());
        (void)ReadSavedModelInfoLink(reader);

        REQUIRE(reader.Read<uint32_t>() == 1);
        const SavedModelInfoLink root_link = ReadSavedModelInfoLink(reader);
        CHECK(root_link.Layer == 1);
        CHECK(root_link.LayerValue == 2);
        CHECK(root_link.LinkBone == "Body");
    }

    SECTION("Honors ModelAnimInfo target filtering")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Missing.fbx\nAnim 0 1 ModelFile Idle\n", 7);

        ModelInfoBaker baker(rig.MakeContext("ArbitraryPack"));
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Other.foinfo"));
        CHECK(rig.Outputs.empty());
    }

    SECTION("Skips model description after second bake checker pass")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 7);

        size_t checks = 0;
        ModelInfoBaker baker(rig.MakeContext("TestPack", [&checks](string_view, uint64_t) {
            checks++;
            return checks == 1;
        }));

        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Critters/Test.fo3d"));
        CHECK(checks == 2);
        CHECK(rig.Outputs.empty());
    }
#endif
}

TEST_CASE("ModelBounds")
{
#if FO_ENABLE_3D
    STATIC_CHECK(MODEL_BOUNDS_VERSION == 2);

    const ModelBounds3D point_bounds {
        .Min = {1.0f, 2.0f, 3.0f},
        .Max = {1.0f, 2.0f, 3.0f},
    };
    CHECK(IsValidModelBounds(point_bounds));
    CHECK_FALSE(HasModelBoundsExtent(point_bounds));

    const ModelBounds3D line_bounds {
        .Min = {1.0f, 2.0f, 3.0f},
        .Max = {4.0f, 2.0f, 3.0f},
    };
    CHECK(IsValidModelBounds(line_bounds));
    CHECK(HasModelBoundsExtent(line_bounds));

    const ModelBounds3D plane_bounds {
        .Min = {-1.0f, -2.0f, 3.0f},
        .Max = {4.0f, 5.0f, 3.0f},
    };
    CHECK(IsValidModelBounds(plane_bounds));
    CHECK(HasModelBoundsExtent(plane_bounds));

    ModelBounds3D invalid_bounds = point_bounds;
    invalid_bounds.Min.x = 2.0f;
    CHECK_FALSE(IsValidModelBounds(invalid_bounds));
    invalid_bounds = point_bounds;
    invalid_bounds.Max.y = std::numeric_limits<float32_t>::infinity();
    CHECK_FALSE(IsValidModelBounds(invalid_bounds));
    invalid_bounds = point_bounds;
    invalid_bounds.Min.z = std::numeric_limits<float32_t>::quiet_NaN();
    CHECK_FALSE(IsValidModelBounds(invalid_bounds));

    optional<ModelBounds3D> accumulated;
    REQUIRE(IncludeModelBoundsPoint(accumulated, vec3 {2.0f, 3.0f, 4.0f}));
    REQUIRE(IncludeModelBoundsPoint(accumulated, vec3 {-1.0f, 5.0f, 1.0f}));
    REQUIRE(accumulated);
    CHECK(accumulated->Min == (vec3 {-1.0f, 3.0f, 1.0f}));
    CHECK(accumulated->Max == (vec3 {2.0f, 5.0f, 4.0f}));

    const ModelBounds3D included {
        .Min = {-4.0f, 4.0f, -2.0f},
        .Max = {-2.0f, 8.0f, 6.0f},
    };
    REQUIRE(IncludeModelBounds(accumulated, included));
    CHECK(accumulated->Min == (vec3 {-4.0f, 3.0f, -2.0f}));
    CHECK(accumulated->Max == (vec3 {2.0f, 8.0f, 6.0f}));

    const ModelBounds3D transformed_source {
        .Min = {-1.0f, -2.0f, -3.0f},
        .Max = {2.0f, 4.0f, 5.0f},
    };
    optional<ModelBounds3D> transformed;
    const mat44 negative_scale = glm::scale(mat44 {1.0f}, vec3 {-2.0f, 3.0f, -1.0f});
    REQUIRE(IncludeTransformedModelBounds(transformed, transformed_source, negative_scale));
    REQUIRE(transformed);
    CHECK(transformed->Min == (vec3 {-4.0f, -6.0f, -5.0f}));
    CHECK(transformed->Max == (vec3 {2.0f, 12.0f, 3.0f}));

    transformed.reset();
    const mat44 rotation = glm::rotate(mat44 {1.0f}, 90.0f * DEG_TO_RAD_FLOAT, vec3 {0.0f, 0.0f, 1.0f});
    REQUIRE(IncludeTransformedModelBounds(transformed, ModelBounds3D {.Min = {0.0f, 0.0f, 0.0f}, .Max = {2.0f, 1.0f, 1.0f}}, rotation));
    REQUIRE(transformed);
    CHECK(transformed->Min.x == Catch::Approx(-1.0f));
    CHECK(transformed->Min.y == Catch::Approx(0.0f).margin(0.0001f));
    CHECK(transformed->Max.x == Catch::Approx(0.0f).margin(0.0001f));
    CHECK(transformed->Max.y == Catch::Approx(2.0f));

    optional<ModelBounds3D> preserved = point_bounds;
    mat44 invalid_transform {1.0f};
    invalid_transform[3][3] = 0.0f;
    CHECK_FALSE(IncludeTransformedModelBounds(preserved, transformed_source, invalid_transform));
    REQUIRE(preserved);
    CHECK(preserved->Min == point_bounds.Min);
    CHECK(preserved->Max == point_bounds.Max);

    const optional<ModelBounds3D> guarded = CalculateGuardedModelBounds(point_bounds);
    REQUIRE(guarded);
    CHECK(guarded->Min.x == Catch::Approx(0.99f));
    CHECK(guarded->Min.y == Catch::Approx(1.99f));
    CHECK(guarded->Min.z == Catch::Approx(2.99f));
    CHECK(guarded->Max.x == Catch::Approx(1.01f));
    CHECK(guarded->Max.y == Catch::Approx(2.01f));
    CHECK(guarded->Max.z == Catch::Approx(3.01f));
#endif
}

TEST_CASE("ModelBoundsCalculator")
{
#if FO_ENABLE_3D
    const vector<uint8_t> data = MakeTestBakedAnimatedTriangle();
    const optional<ModelBounds3D> static_bounds = CalculateModelStaticBounds(data);
    REQUIRE(static_bounds);
    CHECK(static_bounds->Min.x < static_bounds->Max.x);
    CHECK(static_bounds->Min.y < static_bounds->Max.y);
    CHECK(static_bounds->Min.z < static_bounds->Max.z);
    CHECK_FALSE(CalculateModelStaticBounds(data, {string {}}));
    CHECK_FALSE(CalculateModelStaticBounds(data, {"Root"}));

    const optional<ModelBounds3D> bounds = CalculateModelAnimationBounds(data, data, "Move", false);

    REQUIRE(bounds);
    CHECK(bounds->Min.x == Catch::Approx(8.967f));
    CHECK(bounds->Min.y == Catch::Approx(19.967f));
    CHECK(bounds->Min.z == Catch::Approx(29.967f));
    CHECK(bounds->Max.x == Catch::Approx(14.033f));
    CHECK(bounds->Max.y == Catch::Approx(22.033f));
    CHECK(bounds->Max.z == Catch::Approx(33.033f));

    const optional<ModelBounds3D> reversed_bounds = CalculateModelAnimationBounds(data, data, "Move", true);
    REQUIRE(reversed_bounds);
    CHECK(reversed_bounds->Min.x == Catch::Approx(8.967f));
    CHECK(reversed_bounds->Max.x == Catch::Approx(17.033f));

    optional<ModelBounds3D> overflowing_bounds;
    const vector<uint8_t> overflowing_data = MakeTestBakedAnimatedTriangle(true);
    CHECK_NOTHROW(overflowing_bounds = CalculateModelAnimationBounds(overflowing_data, overflowing_data, "Move", false));
    CHECK_FALSE(overflowing_bounds);

    CHECK_THROWS_AS(CalculateModelAnimationBounds(data, data, "Missing", false), ModelBoundsException);

    const optional<ModelBounds3D> weighted_bounds = CalculateModelStaticBounds(MakeTestBakedWeightedTriangle());
    REQUIRE(weighted_bounds);
    CHECK(weighted_bounds->Min.x == Catch::Approx(0.99f));
    CHECK(weighted_bounds->Max.x == Catch::Approx(2.01f));
#endif
}

TEST_CASE("ModelSpriteLayout")
{
#if FO_ENABLE_3D
    const ModelBounds3D bounds {
        .Min = {-1.0f, 0.0f, -0.5f},
        .Max = {1.0f, 2.0f, 0.5f},
    };
    const mat44 identity {1.0f};
    const optional<ModelSpriteLayout> layout = CalculateModelSpriteLayout(bounds, identity, identity, 32.0f, false);

    REQUIRE(layout);
    CHECK(layout->DrawSize == (isize32 {128, 128}));
    CHECK(layout->ViewRect == (irect32 {-38, -66, 76, 68}));

    const optional<ModelSpriteLayout> shadow_layout = CalculateModelSpriteLayout(bounds, identity, identity, 32.0f, true);
    REQUIRE(shadow_layout);
    CHECK(shadow_layout->DrawSize.width >= layout->DrawSize.width);
    CHECK(shadow_layout->DrawSize.height >= layout->DrawSize.height);
    CHECK(shadow_layout->ViewRect == layout->ViewRect);

    const optional<isize32> asymmetric_frame = CalculateModelSpriteFrameSize(-10.0f, -90.0f, 20.0f, 15.0f);
    REQUIRE(asymmetric_frame);
    CHECK(*asymmetric_frame == (isize32 {64, 128}));
    CHECK_FALSE(CalculateModelSpriteFrameSize(-600000000.0f, -1.0f, 600000000.0f, 1.0f));
#endif
}

TEST_CASE("ModelAnimationControllerMissingOutputs")
{
#if FO_ENABLE_3D
    HashStorage hash_resolver;
    ModelAnimation animation = MakeTestModelAnimation(hash_resolver);
    const hstring animated_bone = hash_resolver.ToHashedString("Animated");
    const hstring missing_bone = hash_resolver.ToHashedString("Missing");

    const auto initialize_controller = [&](ModelAnimationController& controller, mat44& animated_matrix, mat44& missing_matrix) {
        controller.RegisterAnimationOutput(animated_bone, animated_matrix);
        controller.RegisterAnimationOutput(missing_bone, missing_matrix);
        const int32_t animation_index = controller.RegisterAnimation(make_ptr(&animation), false);
        controller.SetTrackAnimation(0, animation_index, nullptr);
        controller.SetTrackEnable(0, true);
        controller.AddEventWeight(0, 1.0f, 0.0f, 0.0f);
    };

    mat44 reset_animated = glm::translate(mat44 {1.0f}, vec3 {3.0f, 0.0f, 0.0f});
    mat44 reset_missing = glm::translate(mat44 {1.0f}, vec3 {4.0f, 0.0f, 0.0f});
    ModelAnimationController reset_controller {1};
    initialize_controller(reset_controller, reset_animated, reset_missing);
    reset_missing = glm::translate(mat44 {1.0f}, vec3 {99.0f, 0.0f, 0.0f});
    reset_controller.AdvanceTime(0.0f);
    CHECK(reset_animated[3][0] == Catch::Approx(10.0f));
    CHECK(reset_missing[3][0] == Catch::Approx(4.0f));

    mat44 preserve_animated = glm::translate(mat44 {1.0f}, vec3 {3.0f, 0.0f, 0.0f});
    mat44 preserve_missing = glm::translate(mat44 {1.0f}, vec3 {4.0f, 0.0f, 0.0f});
    ModelAnimationController preserve_controller {1};
    initialize_controller(preserve_controller, preserve_animated, preserve_missing);
    preserve_controller.SetPreserveUnwrittenOutputs(true);
    preserve_missing = glm::translate(mat44 {1.0f}, vec3 {99.0f, 0.0f, 0.0f});
    preserve_controller.AdvanceTime(0.0f);
    CHECK(preserve_animated[3][0] == Catch::Approx(10.0f));
    CHECK(preserve_missing[3][0] == Catch::Approx(99.0f));
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
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", false, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing default diffuse textures")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}, "Body.tga"));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing explicit textures")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nTexture 0 Missing.tga\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects texture mesh references that are not drawable")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nMesh MissingMesh Texture 0 Body.tga\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));
        rig.AddBakedFile("Critters/Body.tga", "stub");

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects out of range layer constants")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 100\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects out of range texture indexes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nTexture 100 Body.tga\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects removed authored draw sizes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        // Positive values were valid under the removed contract, so the throw can only mean the token itself is gone
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nDrawSize 64 96\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_WITH(BakeModelInfoFiles(rig), Catch::Matchers::ContainsSubstring("Unknown token"));
    }

    SECTION("Rejects removed authored view sizes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        // Positive values were valid under the removed contract, so the throw can only mean the token itself is gone
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nViewSize 33 44\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_WITH(BakeModelInfoFiles(rig), Catch::Matchers::ContainsSubstring("Unknown token"));
    }

    SECTION("Rejects missing rotation bones")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nRotationBone MissingBone\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects negative speed adjustments")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nSpeed -0.25\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing effects")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nEffect Effects/Missing.fofx\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing particle attachments")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 1\nValue 2\nAttachParticles Particles/Missing.fope Link Hand\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Hand", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects missing attached model descriptions")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 1\nValue 2\nAttach Missing.fo3d Link Hand\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Hand", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Accepts attached model description references")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nLayer 1\nValue 2\nAttach Child.fo3d Link Hand\n", 1);
        rig.AddSourceFile("Critters/Child.fo3d", "Model Child.fbx\n", 2);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Hand", true, {}));

        ModelInfoBaker baker(rig.MakeContext());
        CHECK_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), "Critters/Test.fo3d"));
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
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
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {"Idle"}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects disabled mesh references that are not drawable")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nDisableMesh MissingMesh\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects animation stacks with non-positive duration")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Idle\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {"Idle"}, {}, {}, {}, {}, 0.0f));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects out of range animation enum constants")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 999 999 ModelFile Idle\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {"Idle"}));
        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects non-positive animation speed constants")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnimSpeed 0 1 0\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Ignores duplicate animation mappings after the first mapping")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Idle\nAnim 0 1 Missing.fbx Missing\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {"Idle"}));

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
        CHECK(rig.Outputs.count("Critters/Test.fo3d") == 1);
    }

    SECTION("Rejects missing cut shapes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nCut Cut.fbx 0 MissingShape - - -\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));
        rig.AddBakedFile("Critters/Cut.fbx", MakeTestBakedModel("Critters/Cut.fbx", "CutShape", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects skin bone references outside the baked hierarchy")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}, {}, {"MissingSkin"}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects animation bone references outside the baked hierarchy")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Idle\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {"Idle"}, {}, {}, {"MissingAnimBone"}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
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
        missing_unskin_pair_rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));
        missing_unskin_pair_rig.AddBakedFile("Critters/Cut.fbx", MakeTestBakedModel("Critters/Cut.fbx", "CutShape", true, {}));
        CHECK_THROWS_AS(BakeModelInfoFiles(missing_unskin_pair_rig), ModelInfoBakerException);

        TestRig shape_without_unskin_bones_rig;
        AddModelInfoMetadata(shape_without_unskin_bones_rig);
        shape_without_unskin_bones_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nCut Cut.fbx 0 CutShape - - CutShape\n", 1);
        shape_without_unskin_bones_rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));
        shape_without_unskin_bones_rig.AddBakedFile("Critters/Cut.fbx", MakeTestBakedModel("Critters/Cut.fbx", "CutShape", true, {}));
        CHECK_THROWS_AS(BakeModelInfoFiles(shape_without_unskin_bones_rig), ModelInfoBakerException);
    }

    SECTION("Reads baked mesh edge metadata")
    {
        TestRig fallback_skin_bone_rig;
        AddModelInfoMetadata(fallback_skin_bone_rig);
        fallback_skin_bone_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        fallback_skin_bone_rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}, {}, {""}));
        CHECK_NOTHROW(BakeModelInfoFiles(fallback_skin_bone_rig));

        TestRig child_bone_rig;
        AddModelInfoMetadata(child_bone_rig);
        child_bone_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nRotationBone Child\n", 1);
        child_bone_rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModelWithChildBone("Body", "Child"));
        CHECK_NOTHROW(BakeModelInfoFiles(child_bone_rig));

        TestRig mismatched_skin_offsets_rig;
        AddModelInfoMetadata(mismatched_skin_offsets_rig);
        mismatched_skin_offsets_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        mismatched_skin_offsets_rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModelWithMismatchedSkinOffsets("Body", "Body"));
        CHECK_THROWS_AS(BakeModelInfoFiles(mismatched_skin_offsets_rig), ModelInfoBakerException);

        TestRig empty_animation_rig;
        AddModelInfoMetadata(empty_animation_rig);
        empty_animation_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        empty_animation_rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {""}));
        CHECK_THROWS_AS(BakeModelInfoFiles(empty_animation_rig), ModelInfoBakerException);

        TestRig truncated_mesh_rig;
        AddModelInfoMetadata(truncated_mesh_rig);
        truncated_mesh_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\n", 1);
        truncated_mesh_rig.AddBakedFile("Critters/Body.fbx", vector<uint8_t> {1, 2, 3});
        CHECK_THROWS_AS(BakeModelInfoFiles(truncated_mesh_rig), ModelInfoBakerException);
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
Anim 0 1 ModelFile ~Idle
AnimSpeed 0 1 2
AnimLayerValue 0 1 2 7
StateAnimEqual 0 0
ActionAnimEqual 1 1
Layer 1 Value 2 Root Link Body
Layer 2 Value 3 AttachParticles Particles/Test.fope Link Body
Layer 3 Value 4 Attach Hat.fbx Link Body Texture 0 Parent_Body Effect Parent_Body
)",
            1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {"Idle"}));
        rig.AddBakedFile("Critters/Hat.fbx", MakeTestBakedModel("Critters/Hat.fbx", "Hat", true, {}));
        rig.AddBakedFile("Critters/Cut.fbx", MakeTestBakedModel("Critters/Cut.fbx", "CutShape", true, {}));
        rig.AddBakedFile("Critters/Body.tga", "stub");
        rig.AddBakedFile("Effects/Test.fofx", "stub");
        rig.AddBakedFile("Particles/Test.fope", "stub");

        CHECK_NOTHROW(BakeModelInfoFiles(rig));
        REQUIRE(rig.Outputs.count("Critters/Test.fo3d") == 1);

        const vector<uint8_t>& output = rig.Outputs.at("Critters/Test.fo3d");
        auto reader = DataReader({output.data(), output.size()});

        CHECK(ReadSavedModelInfoString(reader) == "Critters/Body.fbx");
        CHECK(reader.Read<uint8_t>() == 1);
        CHECK(reader.Read<uint8_t>() == 1);
        CHECK(reader.Read<uint8_t>() == 1);
        CHECK(ReadSavedModelInfoString(reader) == "Body");

        const SavedModelInfoLink default_link = ReadSavedModelInfoLink(reader);
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

        REQUIRE(reader.Read<uint32_t>() == 3);
        const SavedModelInfoLink root_link = ReadSavedModelInfoLink(reader);
        CHECK(root_link.Layer == 1);
        CHECK(root_link.LayerValue == 2);
        CHECK(root_link.LinkBone == "Body");
        CHECK(root_link.ChildName.empty());

        const SavedModelInfoLink particles_link = ReadSavedModelInfoLink(reader);
        CHECK(particles_link.Layer == 2);
        CHECK(particles_link.LayerValue == 3);
        CHECK(particles_link.ChildName == "Particles/Test.fope");
        CHECK(particles_link.IsParticles);

        const SavedModelInfoLink attached_link = ReadSavedModelInfoLink(reader);
        CHECK(attached_link.Layer == 3);
        CHECK(attached_link.LayerValue == 4);
        CHECK(attached_link.ChildName == "Critters/Hat.fbx");
        CHECK(!attached_link.IsParticles);
        CHECK(attached_link.TextureInfoCount == 1);
        CHECK(attached_link.EffectInfoCount == 1);

        REQUIRE(reader.Read<uint32_t>() == 1);
        CHECK(reader.Read<int32_t>() == 0);
        CHECK(reader.Read<int32_t>() == 1);
        CHECK(ReadSavedModelInfoString(reader) == "ModelFile");
        CHECK(ReadSavedModelInfoString(reader) == "~Idle");

        REQUIRE(reader.Read<uint32_t>() == 1);
        CHECK(reader.Read<int32_t>() == 0);
        CHECK(reader.Read<int32_t>() == 1);
        CHECK(reader.Read<float32_t>() == Catch::Approx(2.0f));

        REQUIRE(reader.Read<uint32_t>() == 1);
        CHECK(reader.Read<int32_t>() == 0);
        CHECK(reader.Read<int32_t>() == 1);
        CHECK(reader.Read<int32_t>() == 2);
        CHECK(reader.Read<int32_t>() == 7);

        REQUIRE(reader.Read<uint32_t>() == 1);
        CHECK(ReadSavedModelInfoString(reader) == "Body");

        REQUIRE(reader.Read<uint32_t>() == 1);
        CHECK(reader.Read<int32_t>() == 0);
        CHECK(reader.Read<int32_t>() == 0);

        REQUIRE(reader.Read<uint32_t>() == 1);
        CHECK(reader.Read<int32_t>() == 1);
        CHECK(reader.Read<int32_t>() == 1);

        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("Rejects parser-only model description errors")
    {
        TestRig missing_model_rig;
        AddModelInfoMetadata(missing_model_rig);
        missing_model_rig.AddSourceFile("Critters/Test.fo3d", "DisableShadow\n", 1);
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
        texture_rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));
        CHECK_THROWS_AS(BakeModelInfoFiles(texture_rig), ModelInfoBakerException);

        TestRig effect_rig;
        AddModelInfoMetadata(effect_rig);
        effect_rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nEffect Parent_Body\n", 1);
        effect_rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));
        CHECK_THROWS_AS(BakeModelInfoFiles(effect_rig), ModelInfoBakerException);
    }
#endif
}

FO_END_NAMESPACE
