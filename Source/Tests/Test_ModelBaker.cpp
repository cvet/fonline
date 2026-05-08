//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "Common.h"

#if FO_ENABLE_3D
#include "ModelInfoBaker.h"
#include "ModelMeshBaker.h"
#include "Test_BakerHelpers.h"
#endif

FO_BEGIN_NAMESPACE

static void WriteTestModelString(DataWriter& writer, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    writer.Write<uint32_t>(numeric_cast<uint32_t>(value.length()));
    writer.WritePtr(value.data(), value.length());
}

static void WriteTestModelBone(DataWriter& writer, string_view name, bool attached_mesh, string_view diffuse_texture, initializer_list<string_view> skin_bone_names)
{
    FO_STACK_TRACE_ENTRY();

    WriteTestModelString(writer, name);

    const mat44 matrix {1.0f};
    writer.WritePtr(&matrix, sizeof(matrix));
    writer.WritePtr(&matrix, sizeof(matrix));
    writer.Write<uint8_t>(attached_mesh ? uint8_t {1} : uint8_t {0});

    if (attached_mesh) {
        writer.Write<uint32_t>(uint32_t {0}); // Vertices
        writer.Write<uint32_t>(uint32_t {0}); // Indices
        WriteTestModelString(writer, diffuse_texture);

        writer.Write<uint32_t>(numeric_cast<uint32_t>(skin_bone_names.size())); // Skin bones
        for (string_view skin_bone_name : skin_bone_names) {
            WriteTestModelString(writer, skin_bone_name);
        }

        writer.Write<uint32_t>(numeric_cast<uint32_t>(skin_bone_names.size())); // Skin bone offsets
        for (size_t i = 0; i < skin_bone_names.size(); i++) {
            writer.WritePtr(&matrix, sizeof(matrix));
        }
    }

    writer.Write<uint32_t>(uint32_t {0}); // Children
}

static auto MakeTestBakedModel(string_view file_name, string_view root_bone, bool attached_mesh, initializer_list<string_view> anim_names, string_view diffuse_texture = {}, initializer_list<string_view> skin_bone_names = {}, initializer_list<string_view> animation_output_bones = {}) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    auto writer = DataWriter(data);

    WriteTestModelBone(writer, root_bone, attached_mesh, diffuse_texture, skin_bone_names);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(anim_names.size()));

    for (string_view anim_name : anim_names) {
        WriteTestModelString(writer, file_name);
        WriteTestModelString(writer, anim_name);
        writer.Write<float32_t>(1.0f);
        writer.Write<uint32_t>(uint32_t {0}); // Bones hierarchy

        writer.Write<uint32_t>(numeric_cast<uint32_t>(animation_output_bones.size())); // Bone outputs
        for (string_view bone_name : animation_output_bones) {
            WriteTestModelString(writer, bone_name);
            writer.Write<uint32_t>(uint32_t {0}); // Position times
            writer.Write<uint32_t>(uint32_t {0}); // Rotation times
            writer.Write<uint32_t>(uint32_t {0}); // Scale times
        }
    }

    return data;
}

#if FO_ENABLE_3D
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
    reader.ReadPtr(model_name.data(), model_name_len);
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

    SECTION("Rejects non-positive draw sizes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nDrawSize 10 0\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
    }

    SECTION("Rejects non-positive view sizes")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nViewSize 10 0\n", 1);
        rig.AddBakedFile("Critters/Body.fbx", MakeTestBakedModel("Critters/Body.fbx", "Body", true, {}));

        CHECK_THROWS_AS(BakeModelInfoFiles(rig), ModelInfoBakerException);
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

    SECTION("Rejects missing animation stacks")
    {
        TestRig rig;
        AddModelInfoMetadata(rig);
        rig.AddSourceFile("Critters/Test.fo3d", "Model Body.fbx\nAnim 0 1 ModelFile Run\n", 1);
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
#endif
}

FO_END_NAMESPACE
