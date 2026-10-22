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

#include "Common.h"

#include "Test_ClientEngineHelpers.h"

FO_BEGIN_NAMESPACE

using namespace ClientEngineTests;

TEST_CASE("ClientResourcesRecoverOutdatedInstalledMetadataFromWritableBase")
{
    string unique_name = strex("lf_client_metadata_mount_{}", std::chrono::steady_clock::now().time_since_epoch().count()).str();
    string writable_root = MakeTempClientResourceDir("writable_overlay");
    bool removed_base_before = fs::remove_dir_tree(unique_name);
    bool removed_writable_before = fs::remove_dir_tree(writable_root);
    ignore_unused(removed_base_before);
    ignore_unused(removed_writable_before);

    auto cleanup = scope_exit([&unique_name, &writable_root]() noexcept {
        fs::remove_dir_tree(unique_name);
        fs::remove_dir_tree(writable_root);
    });

    string pack_name = "ClientPack";
    string metadata_file = "Metadata.fometa-client";
    string writable_resources = fs::make_writable_path(writable_root, unique_name);

    STATIC_REQUIRE(METADATA_FILE_VERSION > 1);
    constexpr uint16_t outdated_file_version = METADATA_FILE_VERSION - 1;
    vector<uint8_t> outdated_metadata;
    data_writer outdated_writer {outdated_metadata};
    outdated_writer.write<uint32_t>(METADATA_FILE_MAGIC);
    outdated_writer.write<uint16_t>(outdated_file_version);
    outdated_writer.write<uint16_t>(numeric_cast<uint16_t>(BakerTests::TEST_METADATA_VERSION.length()));
    outdated_writer.write_string_bytes(BakerTests::TEST_METADATA_VERSION);
    outdated_writer.write<uint16_t>(uint16_t {0});

    vector<uint8_t> current_metadata = BakerTests::MakeEmptyMetadataBlob();
    WriteClientTestPack(unique_name, pack_name, {{metadata_file, string(outdated_metadata.begin(), outdated_metadata.end())}});
    WriteClientTestPack(writable_resources, pack_name, {{metadata_file, string(current_metadata.begin(), current_metadata.end())}});

    FileSystem install_resources;
    install_resources.AddCustomSource(safe_alloc::make_unique<ResourcePackSource>(strex(unique_name).combine_path(strex("{}.fores", pack_name)).str()));
    vector<uint8_t> installed_metadata = ReadMetadataBin(&install_resources, "Client");
    CHECK_THROWS_AS(ReadMetadataVersion(installed_metadata), MetadataOutdatedException);

    GlobalSettings settings = MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Common.Packaged, true);
    BakerTests::OverrideSetting(settings.Baking.ClientResources, unique_name);
    auto pack_config = ConfigFile(strex("[ResourcePack]\nName = {}\nClientOnly = True\n", pack_name).str());
    settings.ApplyConfigFile(pack_config, "");
    settings.ApplyWritableRoot(writable_root);

    FileSystem resources = GetClientResources(settings);
    vector<uint8_t> recovered_metadata = ReadMetadataBin(&resources, "Client");

    CHECK(ReadMetadataVersion(recovered_metadata) == BakerTests::TEST_METADATA_VERSION);

    EngineMetadata metadata {[] { }};
    metadata.RegisterSide(EngineSideKind::ClientSide);
    CHECK_NOTHROW(RegisterDynamicMetadata(&metadata, recovered_metadata));
}

TEST_CASE("ClientResourceIndexPreservesEmbeddedAndWritablePrecedence")
{
    if (!CanUseDirectoryBackedClientResourceFixtures()) {
        SKIP("Directory-backed prefix and overlay fixtures require an unpackaged test binary");
    }

    string dir = MakeTempClientResourceDir("merged_index");
    string install = strex("lf_client_index_{}", std::chrono::steady_clock::now().time_since_epoch().count()).str();
    auto cleanup = scope_exit([&dir, &install]() noexcept {
        (void)fs::remove_dir_tree(dir);
        (void)fs::remove_dir_tree(install);
    });
    string writable = strex(dir).combine_path("Writable").str();
    string overlay = fs::make_writable_path(writable, install);

    WriteClientTestPack(install, "Before", {{"Shared.txt", "before"}});
    REQUIRE(fs::write_file(strex(install).combine_path("Embedded/Shared.txt").str(), string_view {"embedded"}));
    REQUIRE(fs::write_file(strex(install).combine_path("Embedded/Bootstrap.txt").str(), string_view {"bootstrap"}));
    WriteClientTestPack(overlay, "Before", {{"Shared.txt", "before"}, {"Overlay.txt", "writable"}});

    {
        ResourcePackWriter writer {strex(install).combine_path("Art.fores").str()};
        string payload = "art";
        writer.AddFile("Shared.txt", {reinterpret_cast<const uint8_t*>(payload.data()), payload.size()});
        writer.AddFile("Overlay.txt", {reinterpret_cast<const uint8_t*>(payload.data()), payload.size()});
        writer.Finish();
    }

    GlobalSettings settings = MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Common.Packaged, true);
    BakerTests::OverrideSetting(settings.Baking.ClientResources, install);
    auto pack_config = ConfigFile("[ResourcePack]\nName = Before\nClientOnly = True\n[ResourcePack]\nName = Embedded\nClientOnly = True\n[ResourcePack]\nName = Art\nClientOnly = True\n");
    settings.ApplyConfigFile(pack_config, "");
    settings.ApplyWritableRoot(writable);

    vector<ResourceIndexPack> packs;
    vector<string> pack_paths;
    vector<string> indexed_names = GetResourceIndexPackNames(settings.GetClientResourcePacks());
    REQUIRE(ResolveResourceIndexPacks({install}, indexed_names, packs, pack_paths));
    string index_path = GetClientResourceIndexPath(settings);
    REQUIRE(fs::create_directories(strex(index_path).extract_dir().str()));
    BuildResourceIndex(index_path, pack_paths, packs, ResourcePackWriteSettings {0, 100});

    SECTION("MountsTheIndexWithoutDroppingBootstrapOrOverlayFiles")
    {
        FileSystem resources = GetClientResources(settings);
        CHECK(resources.ReadFileText("Shared.txt") == "art");
        CHECK(resources.ReadFileText("Bootstrap.txt") == "bootstrap");
        CHECK(resources.ReadFileText("Overlay.txt") == "art");
        CHECK(resources.ReadFileHeader("Shared.txt").GetDataSource()->GetPackName() == index_path);
    }

    SECTION("DiscardsACorruptDerivedIndexAndMountsThePacks")
    {
        auto original = fs::read_file(index_path);
        REQUIRE(original.has_value());
        vector<uint8_t> bytes(original->begin(), original->end());
        span_write_uint32(bytes, RESOURCE_INDEX_HEADER_SIZE + RESOURCE_INDEX_PACK_SIZE, std::numeric_limits<uint32_t>::max());
        REQUIRE(fs::write_file(index_path, bytes));

        FileSystem resources = GetClientResources(settings);
        CHECK(resources.ReadFileText("Shared.txt") == "art");
        CHECK(resources.ReadFileText("Bootstrap.txt") == "bootstrap");
        CHECK_FALSE(fs::exists(index_path));
    }
}

TEST_CASE("InstalledClientResourcesSelectWritableBaseWithoutOldCatalogLayering")
{
    string unique_name = strex("lf_client_pack_mount_{}", std::chrono::steady_clock::now().time_since_epoch().count()).str();
    string writable_root = MakeTempClientResourceDir("writable_overlay");
    bool removed_base_before = fs::remove_dir_tree(unique_name);
    bool removed_writable_before = fs::remove_dir_tree(writable_root);
    ignore_unused(removed_base_before);
    ignore_unused(removed_writable_before);

    auto cleanup = scope_exit([&unique_name, &writable_root]() noexcept {
        fs::remove_dir_tree(unique_name);
        fs::remove_dir_tree(writable_root);
    });

    string writable_resources = fs::make_writable_path(writable_root, unique_name);
    WriteClientTestPack(unique_name, "Main", {{"shared.txt", "install-base"}, {"base-only.txt", "base-only"}});
    WriteClientTestPack(unique_name, "Fallback", {{"fallback.txt", "base-fallback"}});
    WriteClientTestPack(writable_resources, "Main", {{"shared.txt", "writable-base"}, {"overlay-only.txt", "overlay-only"}});

    GlobalSettings settings = MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Common.Packaged, true);
    BakerTests::OverrideSetting(settings.Baking.ClientResources, unique_name);
    auto pack_config = ConfigFile("[ResourcePack]\nName = Main\nClientOnly = True\n[ResourcePack]\nName = Fallback\nClientOnly = True\n");
    settings.ApplyConfigFile(pack_config, "");
    settings.ApplyWritableRoot(writable_root);

    FileSystem resources = GetClientResources(settings);
    CHECK(resources.ReadFileText("shared.txt") == "writable-base");
    CHECK_FALSE(resources.IsFileExists("base-only.txt"));
    CHECK(resources.ReadFileText("overlay-only.txt") == "overlay-only");
    CHECK(resources.ReadFileText("fallback.txt") == "base-fallback");

    SECTION("MissingBaseDoesNotMountLooseFiles")
    {
        resources.CleanDataSources();
        REQUIRE(fs::remove_file(strex(unique_name).combine_path("Fallback.fores").str()));
        REQUIRE(fs::write_file(strex(unique_name).combine_path("Fallback/fallback.txt").str(), string_view {"loose"}));
        CHECK_THROWS(GetClientResources(settings));
    }
}

#if (FO_ENABLE_3D)
TEST_CASE("ClientEngineRejectsMultiFrameModelTextures")
{
    constexpr string_view model_path = "Models/MultiFrameTexture.fo3d";
    constexpr string_view mesh_path = "Models/MultiFrameTexture.fbx";
    vector<pair<string, vector<uint8_t>>> resources {{"Models/Cycle.fofrm", BakerTests::MakeMinimalBakedSprite()}};
    auto mesh = MakeRuntimeModelTriangleMesh();
    auto description = MakeRuntimeModelDescription(model_path, mesh_path, mesh, "Texture 0 Cycle.fofrm\n", {}, nullptr, false, resources);

    // The baker saw a one-frame resource, so the description is valid; only the mounted bytes become multi-frame
    resources.front().second = BakerTests::MakeMultiFrameBakedSprite(2, 1, 1, 200);
    resources.emplace_back(string {mesh_path}, std::move(mesh));
    resources.emplace_back(string {model_path}, std::move(description));
    resources.emplace_back("ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo(model_path));
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    REQUIRE_THROWS_WITH(factory->GetModelMngr()->CreateModel(model_path), Catch::Matchers::ContainsSubstring("Image resource must contain exactly one frame"));
}

TEST_CASE("ClientEngineOverridesConsumedImportedDiffuse")
{
#if FO_ENABLE_3D
    constexpr string_view model_path = "Models/EffectiveTexture.fo3d";
    constexpr string_view mesh_path = "Models/EffectiveTexture.fbx";
    auto mesh = MakeRuntimeModelTriangleMesh({}, "Consumed.png");
    vector<pair<string, vector<uint8_t>>> resources {{"Models/Effective.fofrm", BakerTests::MakeMinimalBakedSprite()}};
    auto description = MakeRuntimeModelDescription(model_path, mesh_path, mesh, "Texture 0 Effective.fofrm\n", {}, nullptr, false, resources);
    resources.emplace_back(string {mesh_path}, std::move(mesh));
    resources.emplace_back(string {model_path}, std::move(description));
    resources.emplace_back("ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo(model_path));
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    auto model = factory->GetModelMngr()->CreateModel(model_path);
    REQUIRE(model);
    model->StartMeshGeneration();
    REQUIRE(model->GetMeshTexture(0, 0));
    CHECK(model->GetMeshTexture(0, 0)->MainTex->Size.width > 0);
#endif
}

TEST_CASE("ClientEngineLoadsLargeFofrmModelTextures")
{
#if FO_ENABLE_3D
    constexpr string_view model_path = "Models/LargeTexture.fo3d";
    constexpr string_view mesh_path = "Models/LargeTexture.fbx";
    auto mesh = MakeRuntimeModelTriangleMesh();
    vector<pair<string, vector<uint8_t>>> resources {{"Models/Large.fofrm", BakerTests::MakeMinimalBakedSprite(1024, 1024)}};
    auto description = MakeRuntimeModelDescription(model_path, mesh_path, mesh, "Texture 0 Large.fofrm\n", {}, nullptr, false, resources);
    resources.emplace_back(string {mesh_path}, std::move(mesh));
    resources.emplace_back(string {model_path}, std::move(description));
    resources.emplace_back("ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo(model_path));
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    auto model = factory->GetModelMngr()->CreateModel(model_path);
    REQUIRE(model);
    model->StartMeshGeneration();
    auto texture = model->GetMeshTexture(0, 0);
    REQUIRE(texture);
    CHECK(texture->MainTex->Size.width >= 1026);
    CHECK(texture->MainTex->Size.height >= 1026);
#endif
}

TEST_CASE("ClientEngineRetriesMissingImportedDiffuse")
{
#if FO_ENABLE_3D
    constexpr string_view model_path = "Models/MissingTexture.fo3d";
    constexpr string_view mesh_path = "Models/MissingTexture.fbx";
    auto mesh = MakeRuntimeModelTriangleMesh({}, "Missing.png");
    vector<pair<string, vector<uint8_t>>> bake_resources {{"Models/Missing.png", BakerTests::MakeMinimalBakedSprite()}};
    auto description = MakeRuntimeModelDescription(model_path, mesh_path, mesh, "", {}, nullptr, false, bake_resources);
    vector<pair<string, vector<uint8_t>>> resources {{string {mesh_path}, std::move(mesh)}, {string {model_path}, std::move(description)}, {"ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo(model_path)}};
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    auto model = factory->GetModelMngr()->CreateModel(model_path);
    REQUIRE(model);
    REQUIRE_THROWS_WITH(model->StartMeshGeneration(), Catch::Matchers::ContainsSubstring("Model texture could not be loaded"));
    REQUIRE_THROWS_WITH(model->StartMeshGeneration(), Catch::Matchers::ContainsSubstring("Model texture could not be loaded"));
#endif
}

TEST_CASE("ClientEngineLoadsModelMeshBakerOutputThroughRuntimeParser")
{
    constexpr string_view model_path = "Models/RuntimeParserTriangle.obj";

    BakerTests::TestRig rig;
    rig.AddSourceFile(model_path, R"(o RuntimeParserTriangle
v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)");

    ModelMeshBaker baker(rig.MakeContext());
    baker.BakeFiles(rig.GetAllSourceFiles(), model_path);

    auto output_it = rig.Outputs.find(string(model_path));
    REQUIRE(output_it != rig.Outputs.end());

    auto resources = MakeClientTestResources({{string {model_path}, output_it->second}});
    REQUIRE(resources.IsFileExists(model_path));

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, std::move(resources));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    CHECK_NOTHROW(factory->GetModelMngr()->PreloadModel(model_path));
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ClientEngineRejectsMalformedBakedModelCountsAndBounds")
{
    static_assert(MODEL_DESCRIPTION_SCHEMA_VERSION == 5);

    vector<pair<string, vector<uint8_t>>> malformed_resources;

    malformed_resources.emplace_back("Models/VertexCountBomb.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/IndexCountBomb.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/IndexOutOfBounds.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        array<Vertex3D, 1> vertices {};
        writer.write<uint32_t>(numeric_cast<uint32_t>(vertices.size()));
        writer.write_object_array(const_span<Vertex3D> {vertices});
        array<vindex_t, 1> indices {vindex_t {1}};
        writer.write<uint32_t>(numeric_cast<uint32_t>(indices.size()));
        writer.write_object_array(const_span<vindex_t> {indices});
        writer.write_string({});
        writer.write<uint32_t>(uint32_t {1});
        writer.write_string({});
        writer.write<uint32_t>(uint32_t {1});
        writer.write<mat44>(mat44 {1.0f});
        writer.write<uint32_t>(uint32_t {0});
    }));

    malformed_resources.emplace_back("Models/SkinCountBomb.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write_string({});
        writer.write<uint32_t>(numeric_cast<uint32_t>(MODEL_MAX_BONES + 1));
    }));

    malformed_resources.emplace_back("Models/SkinOffsetMismatch.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", true);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write_string({});
        writer.write<uint32_t>(uint32_t {1});
        writer.write_string({});
        writer.write<uint32_t>(uint32_t {0});
    }));

    Vertex3D valid_skin_vertex {};
    valid_skin_vertex.BlendWeights[0] = 1.0f;

    Vertex3D non_finite_skin_weight = valid_skin_vertex;
    non_finite_skin_weight.BlendWeights[0] = std::numeric_limits<float32_t>::quiet_NaN();
    malformed_resources.emplace_back("Models/NonFiniteSkinWeight.fbx", MakeRuntimeModelMeshWithVertex(non_finite_skin_weight));

    Vertex3D out_of_range_skin_weight = valid_skin_vertex;
    out_of_range_skin_weight.BlendWeights[0] = -0.25f;
    malformed_resources.emplace_back("Models/OutOfRangeSkinWeight.fbx", MakeRuntimeModelMeshWithVertex(out_of_range_skin_weight));

    Vertex3D non_finite_skin_index = valid_skin_vertex;
    non_finite_skin_index.BlendIndices[0] = std::numeric_limits<float32_t>::infinity();
    malformed_resources.emplace_back("Models/NonFiniteSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(non_finite_skin_index));

    Vertex3D non_integral_skin_index = valid_skin_vertex;
    non_integral_skin_index.BlendIndices[0] = 0.5f;
    malformed_resources.emplace_back("Models/NonIntegralSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(non_integral_skin_index));

    Vertex3D out_of_range_skin_index = valid_skin_vertex;
    out_of_range_skin_index.BlendIndices[0] = 1.0f;
    malformed_resources.emplace_back("Models/OutOfRangeSkinIndex.fbx", MakeRuntimeModelMeshWithVertex(out_of_range_skin_index));

    Vertex3D invalid_skin_weight_sum = valid_skin_vertex;
    invalid_skin_weight_sum.BlendWeights[0] = 0.5f;
    malformed_resources.emplace_back("Models/InvalidSkinWeightSum.fbx", MakeRuntimeModelMeshWithVertex(invalid_skin_weight_sum));

    malformed_resources.emplace_back("Models/ChildCountBomb.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        WriteRuntimeModelBoneHeader(writer, "Root", false);
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
    }));

    malformed_resources.emplace_back("Models/HierarchyDepthBomb.fbx", MakeRuntimeModelMesh([](data_writer& writer) {
        for (uint32_t depth = 0; depth <= MODEL_MESH_MAX_HIERARCHY_DEPTH; depth++) {
            WriteRuntimeModelBoneHeader(writer, "Bone", false);
            writer.write<uint32_t>(depth < MODEL_MESH_MAX_HIERARCHY_DEPTH ? uint32_t {1} : uint32_t {0});
        }
    }));

    {
        FO_TEMPORARY_COMPAT(test_model_description_schema4, "2026-12-31");

        vector<uint8_t> data;
        data_writer writer {data};
        writer.write_bytes({MODEL_DESCRIPTION_MAGIC.data(), MODEL_DESCRIPTION_MAGIC.size()});
        writer.write<uint16_t>(uint16_t {4});
        writer.write<uint16_t>(MODEL_DESCRIPTION_SUPPORTED_FLAGS);
        malformed_resources.emplace_back("Models/DescriptionOldSchema.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data;
        data_writer writer {data};
        writer.write_bytes({MODEL_DESCRIPTION_MAGIC.data(), MODEL_DESCRIPTION_MAGIC.size()});
        writer.write<uint16_t>(MODEL_DESCRIPTION_SCHEMA_VERSION);
        writer.write<uint16_t>(MODEL_DESCRIPTION_SUPPORTED_FLAGS);
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionStringBomb.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data;
        data_writer writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLink(writer);
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionLinksBomb.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data;
        data_writer writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLinkPrefix(writer);
        writer.write<uint32_t>(std::numeric_limits<uint32_t>::max());
        malformed_resources.emplace_back("Models/DescriptionNestedCountBomb.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data;
        data_writer writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLinkPrefix(writer);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint8_t>(uint8_t {1});
        malformed_resources.emplace_back("Models/DescriptionGeometryFlagMismatch.fo3d", std::move(data));
    }

    {
        vector<uint8_t> data;
        data_writer writer {data};
        WriteRuntimeModelDescriptionPrefix(writer);
        WriteRuntimeModelDescriptionLinkPrefix(writer);
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint8_t>(uint8_t {2});
        malformed_resources.emplace_back("Models/DescriptionGeometryFlagOutOfRange.fo3d", std::move(data));
    }

    array<pair<string_view, string_view>, 19> expected_failures {{
        {"Models/VertexCountBomb.fbx", "vertex count exceeds maximum addressable count"},
        {"Models/IndexCountBomb.fbx", "mesh indices"},
        {"Models/IndexOutOfBounds.fbx", "outside vertex count"},
        {"Models/SkinCountBomb.fbx", "skin bone count exceeds maximum"},
        {"Models/SkinOffsetMismatch.fbx", "skin bone offset count mismatch"},
        {"Models/NonFiniteSkinWeight.fbx", "non-finite skin weight"},
        {"Models/OutOfRangeSkinWeight.fbx", "skin weight outside [0, 1]"},
        {"Models/NonFiniteSkinIndex.fbx", "non-finite skin index"},
        {"Models/NonIntegralSkinIndex.fbx", "non-integral skin index"},
        {"Models/OutOfRangeSkinIndex.fbx", "skin index outside valid range"},
        {"Models/InvalidSkinWeightSum.fbx", "skin-weight sum"},
        {"Models/ChildCountBomb.fbx", "child count exceeds maximum"},
        {"Models/HierarchyDepthBomb.fbx", "hierarchy depth"},
        {"Models/DescriptionOldSchema.fo3d", "Unsupported baked model description schema"},
        {"Models/DescriptionStringBomb.fo3d", "String length exceeds remaining buffer"},
        {"Models/DescriptionLinksBomb.fo3d", "links"},
        {"Models/DescriptionNestedCountBomb.fo3d", "disabled meshes"},
        {"Models/DescriptionGeometryFlagMismatch.fo3d", "geometry flag does not match"},
        {"Models/DescriptionGeometryFlagOutOfRange.fo3d", "geometry flag is not 0 or 1"},
    }};

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(malformed_resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    auto model_mngr = factory->GetModelMngr();

    for (const auto& [model_path, expected_failure] : expected_failures) {
        INFO(model_path);
        CHECK_THROWS_WITH(model_mngr->PreloadModel(model_path), Catch::Matchers::ContainsSubstring(std::string {expected_failure}));
    }
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ModelSpriteBoundsFollowEveryStateChangeThatMovesTheEnvelope")
{
    // Pinned differentially against a fresh instance rather than hardcoded rectangles, because the frame is derived
    // through per-instance state that can go stale, and a stale rectangle clips the model with nothing logged
    constexpr string_view model_path = "Models/SkinnedSpriteBounds.fbx";

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources({{string {model_path}, MakeSkinnedRuntimeModelMesh()}}));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    auto model_mngr = factory->GetModelMngr();

    struct SpriteBoundsStep
    {
        string_view Name {};
        void (*Apply)(ptr<ModelInstance>) {};
    };

    vector<SpriteBoundsStep> steps {
        {"rest pose", [](ptr<ModelInstance>) {}},
        {"scaled up", [](ptr<ModelInstance> model) { model->SetScale(1.7f, 1.7f, 1.7f); }},
        {"camera tilt", [](ptr<ModelInstance> model) { model->SetRotation(0.4f, 0.0f, 0.25f); }},
        {"turned", [](ptr<ModelInstance> model) { model->SetDir(mdir {120}, false); }},
        {"shadow off", [](ptr<ModelInstance> model) { model->EnableShadow(false); }},
        {"scaled back down", [](ptr<ModelInstance> model) { model->SetScale(0.6f, 0.6f, 0.6f); }},
    };

    // model_path is a constexpr string_view, but passing it by value and by reference odr-uses it, so it
    // has to be captured rather than relied on as a constant expression
    auto make_model = [&model_mngr, model_path]() {
        auto model = model_mngr->CreateModel(model_path);
        FO_VERIFY_AND_THROW(model, "Skinned test model was not created", model_path);
        auto owned = model.take_not_null();

        owned->StartMeshGeneration();
        owned->PrepareFrameLayout();
        owned->SetupFrame(owned->GetDrawSize(), owned->GetFramePivot());
        return owned;
    };

    auto measure = [](ptr<ModelInstance> model) -> optional<ModelSpriteBounds> {
        model->PoseSpriteFrame(false);
        return model->GetSpriteBounds();
    };

    auto same_bounds = [](const ModelSpriteBounds& first, const ModelSpriteBounds& second) { //
        return first.Rect == second.Rect && first.RequiredFrameSize == second.RequiredFrameSize && first.Pivot == second.Pivot;
    };

    auto warm_model = make_model();

    // The hit path on its own: measuring twice without touching anything must reproduce the first answer exactly
    optional<ModelSpriteBounds> first_bounds = measure(warm_model.as_ptr());
    optional<ModelSpriteBounds> repeated_bounds = measure(warm_model.as_ptr());

    REQUIRE(first_bounds);
    REQUIRE(repeated_bounds);
    CHECK(same_bounds(*first_bounds, *repeated_bounds));

    optional<ModelSpriteBounds> previous_bounds;
    size_t moved_steps = 0;

    for (size_t step_index = 0; step_index < steps.size(); step_index++) {
        INFO("step " << step_index << " (" << steps[step_index].Name << ")");

        steps[step_index].Apply(warm_model.as_ptr());
        optional<ModelSpriteBounds> warm_bounds = measure(warm_model.as_ptr());
        auto cold_model = make_model();

        for (size_t applied_index = 0; applied_index <= step_index; applied_index++) {
            steps[applied_index].Apply(cold_model.as_ptr());
        }

        optional<ModelSpriteBounds> cold_bounds = measure(cold_model.as_ptr());

        REQUIRE(warm_bounds);
        REQUIRE(cold_bounds);
        CHECK(same_bounds(*warm_bounds, *cold_bounds));

        if (previous_bounds && !same_bounds(*cold_bounds, *previous_bounds)) {
            moved_steps++;
        }

        previous_bounds = cold_bounds;
    }

    // Guard the guard: a step that leaves the envelope where it was cannot tell a stale cache from a correct one, so
    // the sequence above only tests anything as long as it keeps moving the rectangle
    CHECK(moved_steps + 1 >= steps.size() - 1);
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ModelDefaultLinkDisablesItsOwnMeshes")
{
    // Baked model and animation bounds are calculated with the default link's disabled meshes excluded, so the
    // runtime model state must agree with the geometry the bake budgeted for
    constexpr string_view MESH_PATH = "Models/DefaultLinkDisabled.fbx";
    constexpr string_view MODEL_PATH = "Models/DefaultLinkDisabled.fo3d";

    // Far outside the declared bounds; the test is about applying the authored disable state, not measuring vertices
    vector<uint8_t> mesh_blob = MakeRuntimeModelTriangleMesh(vec3 {12.0f, 12.0f, 0.0f});

    vector<pair<string, vector<uint8_t>>> model_resources;
    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {MESH_PATH}, mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh_blob, "DisableMesh All\n"));

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);

    auto model_mngr = factory->GetModelMngr();

    REQUIRE_NOTHROW(model_mngr->PreloadModel(MODEL_PATH));

    auto model = model_mngr->CreateModel(MODEL_PATH);
    REQUIRE(static_cast<bool>(model));

    // Creation leaves the root layout pending: _parent is assigned only after CreateModel returns, and an
    // attachment laid out as a root measures the joint it hangs from plus its shadow, past any frame it can hold
    CHECK(model->GetDrawSize() == isize32 {4, 4});

    model->StartMeshGeneration();
    model->PrepareFrameLayout();

    // Guard the fixture: the layout must come from the declared +/-1 bounds, or the far triangle is already
    // inside the frame and the check below would pass without proving anything
    isize32 layout_size = model->GetDrawSize();
    int32_t bounds_span_limit = iround<int32_t>(6.0f * client->Settings->Render.ModelProjFactor);
    REQUIRE(layout_size.width <= bounds_span_limit);
    REQUIRE(layout_size.height <= bounds_span_limit);

    model->SetupFrame(layout_size, model->GetFramePivot());
    model->PoseSpriteFrame(true);

    auto bounds = model->GetSpriteBounds();
    REQUIRE(bounds);
    CHECK(bounds->RequiredFrameSize == layout_size);
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ModelSpriteBoundsUseBakedBoundsForTheActiveGeometryLink")
{
    constexpr string_view BASE_MESH_PATH = "Models/BakedLinkBase.fbx";
    constexpr string_view LINK_MESH_PATH = "Models/BakedLinkEquipment.fbx";
    constexpr string_view MODEL_PATH = "Models/BakedLink.fo3d";
    vector<uint8_t> base_mesh_blob = MakeSkinnedRuntimeModelMesh();
    vector<uint8_t> link_mesh_blob = MakeSkinnedRuntimeModelMesh();
    vector<uint8_t> model_blob = MakeRuntimeModelDescription(MODEL_PATH, BASE_MESH_PATH, base_mesh_blob, "Layer 3\n Value 1 Attach BakedLinkEquipment.fbx MoveX 3\n", LINK_MESH_PATH, &link_mesh_blob, true);
    vector<pair<string, vector<uint8_t>>> model_resources;
    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {BASE_MESH_PATH}, base_mesh_blob);
    model_resources.emplace_back(string {LINK_MESH_PATH}, link_mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, std::move(model_blob));

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);
    auto model = factory->GetModelMngr()->CreateModel(MODEL_PATH);
    REQUIRE(static_cast<bool>(model));

    model->StartMeshGeneration();
    model->PrepareFrameLayout();
    isize32 base_size = model->GetDrawSize();
    array<int32_t, MODEL_LAYERS_COUNT> layers {};
    layers[3] = 1;
    REQUIRE(model->PlayAnim(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), layers.data(), 0.0f, ModelAnimFlags::None));
    isize32 equipped_size = model->GetDrawSize();
    CHECK(equipped_size.width > base_size.width);

    model->SetupFrame(equipped_size, model->GetFramePivot());
    model->PoseSpriteFrame(true);
    optional<ModelSpriteBounds> bounds = model->GetSpriteBounds();
    REQUIRE(bounds);
    CHECK(bounds->RequiredFrameSize == equipped_size);

    layers[3] = 0;
    REQUIRE(model->PlayAnim(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), layers.data(), 0.0f, ModelAnimFlags::None));
    isize32 unequipped_size = model->GetDrawSize();
    CHECK(unequipped_size.width < equipped_size.width);
    CHECK(unequipped_size.height < equipped_size.height);
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ModelManagerInstantiatesABakedModel")
{
    // The 3D instance surface was assumed to need a GPU, but the headless Null renderer serves it: with a
    // baked mesh and a valid baked description the manager builds a real ModelInstance
    constexpr string_view MESH_PATH = "Models/RuntimeInstance.fbx";
    constexpr string_view MODEL_PATH = "Models/RuntimeInstance.fo3d";

    Vertex3D vertex {};
    vertex.BlendWeights[0] = 1.0f;
    vertex.BlendIndices[0] = 0.0f;

    vector<pair<string, vector<uint8_t>>> model_resources;
    ignore_unused(vertex);
    vector<uint8_t> mesh_blob = MakeRuntimeModelTriangleMesh();

    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {MESH_PATH}, mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh_blob));

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto factory = client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>();
    REQUIRE(factory);

    auto model_mngr = factory->GetModelMngr();

    REQUIRE_NOTHROW(model_mngr->PreloadModel(MODEL_PATH));

    auto model = model_mngr->CreateModel(MODEL_PATH);
    REQUIRE(static_cast<bool>(model));

    SECTION("TheInstanceAnswersItsGeometryAndAnimationState")
    {
        ignore_unused(model->GetInformation());
        ignore_unused(model->GetStateAnim());
        ignore_unused(model->GetActionAnim());
        ignore_unused(model->GetMovingAnim());
        ignore_unused(model->GetDrawSize());
        ignore_unused(model->GetLightingSize());
        ignore_unused(model->GetSpriteBounds());
        ignore_unused(model->GetDrawRect());
        ignore_unused(model->GetFramePivot());
        ignore_unused(model->NeedForceDraw());
        ignore_unused(model->NeedDraw());
        ignore_unused(model->HasAnimation(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1)));

        ipos32 screen_pos = model->Convert3dTo2d(vec3 {0.0f, 0.0f, 0.0f});
        vec3 world_pos = model->Convert2dTo3d(screen_pos);
        CHECK(std::isfinite(world_pos.x));
        CHECK(std::isfinite(world_pos.y));
        CHECK(std::isfinite(world_pos.z));

        auto state_anim = static_cast<CritterStateAnim>(1);
        auto action_anim = static_cast<CritterActionAnim>(1);
        ignore_unused(model->ResolveAnimation(state_anim, action_anim));
    }

    SECTION("TheInstanceAcceptsOrientationScaleAndMovementState")
    {
        REQUIRE_NOTHROW(model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96}));
        REQUIRE_NOTHROW(model->PrepareFrameLayout());
        REQUIRE_NOTHROW(model->RequestRedraw());

        for (int32_t dir = 0; dir < 6; dir++) {
            REQUIRE_NOTHROW(model->SetDir(mdir {numeric_cast<uint8_t>(dir)}, false));
            REQUIRE_NOTHROW(model->SetLookDir(mdir {numeric_cast<uint8_t>(dir)}));
            REQUIRE_NOTHROW(model->SetMoveDir(mdir {numeric_cast<uint8_t>(dir)}, true));
        }

        REQUIRE_NOTHROW(model->SetRotation(0.0f, 30.0f, 0.0f));
        REQUIRE_NOTHROW(model->SetScale(1.0f, 1.0f, 1.0f));
        REQUIRE_NOTHROW(model->SetSpeed(2.0f));
        REQUIRE_NOTHROW(model->EnableShadow(true));
        REQUIRE_NOTHROW(model->EnableShadow(false));
        REQUIRE_NOTHROW(model->AddMoveOffset(ipos32 {4, 4}));
        REQUIRE_NOTHROW(model->SetMovementState(false, true, 10));
        REQUIRE_NOTHROW(model->SetMovementState(true, false, 0));

        ignore_unused(model->GetMoveDirAngle());
        ignore_unused(model->HasBodyRotation());
        ignore_unused(model->GetViewRect());
        ignore_unused(model->GetAttachPoints());
        ignore_unused(model->GetAnimDuration());
        ignore_unused(model->GetAnimDuration(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1)));
        ignore_unused(model->IsAnimationPlaying());

        hstring root_bone = client->Hashes.to_hashed_string("Root");
        ignore_unused(model->FindBone(root_bone));
        ignore_unused(model->GetBonePos(root_bone));
        ignore_unused(model->GetBoneSpritePos(root_bone));

        REQUIRE_NOTHROW(model->ClearAnimationCallbacks());
        REQUIRE_NOTHROW(model->SetAnimInitCallback([](CritterStateAnim&, CritterActionAnim&) { }));
    }

    SECTION("AFrameLargerThanTheMaximumRenderTextureIsRejected")
    {
        // Rejected while the model is still identifiable, instead of reaching the graphics API and failing there as
        // an anonymous invalid argument
        int32_t max_draw_width = AppRender::MAX_ATLAS_WIDTH / ModelInstance::FRAME_SCALE;
        int32_t max_draw_height = AppRender::MAX_ATLAS_HEIGHT / ModelInstance::FRAME_SCALE;

        REQUIRE(max_draw_width > 0);
        REQUIRE(max_draw_height > 0);
        REQUIRE_NOTHROW(model->SetupFrame(isize32 {max_draw_width, max_draw_height}, ipos32 {}));
        REQUIRE_THROWS(model->SetupFrame(isize32 {max_draw_width + 1, max_draw_height}, ipos32 {}));
        REQUIRE_THROWS(model->SetupFrame(isize32 {max_draw_width, max_draw_height + 1}, ipos32 {}));
    }

    SECTION("LayerValuesDriveTheAnimationDataAndLinkTransforms")
    {
        // Each link block applies only when its layer value is requested, so nothing under SetAnimData runs until
        // an animation plays with layers set
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});

        auto state_anim = static_cast<CritterStateAnim>(1);
        auto action_anim = static_cast<CritterActionAnim>(1);

        std::array<int32_t, MODEL_LAYERS_COUNT> layers {};

        auto play_with_layers = [&](ModelAnimFlags flags) {
            REQUIRE_NOTHROW(ignore_unused(model->PlayAnim(state_anim, action_anim, layers.data(), 0.0f, flags)));

            for (int32_t frame = 0; frame < 3; frame++) {
                REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
            }
        };

        // Layer 1 / value 1 is the block that links a bone and scales, rotates and moves it
        layers[1] = 1;
        play_with_layers(ModelAnimFlags::Init);

        // Layer 1 / value 2 disables the second layer and every mesh under it
        layers[1] = 2;
        layers[2] = 1;
        play_with_layers(ModelAnimFlags::None);

        // The same walk again with the smoothing and rotation paths switched off
        layers[1] = 1;
        layers[2] = 1;
        play_with_layers(ModelAnimFlags::NoSmooth);
        play_with_layers(ModelAnimFlags::NoRotate);
        play_with_layers(ModelAnimFlags::PlayOnce);
        play_with_layers(ModelAnimFlags::Freeze);

        // Clearing the layers takes the removal side of the same walk
        layers[1] = 0;
        layers[2] = 0;
        play_with_layers(ModelAnimFlags::None);

        // A speed override and a missing animation are the two remaining arms of PlayAnim
        REQUIRE_NOTHROW(ignore_unused(model->PlayAnim(state_anim, action_anim, layers.data(), 0.5f, ModelAnimFlags::None)));
        CHECK_FALSE(model->PlayAnim(static_cast<CritterStateAnim>(9), static_cast<CritterActionAnim>(9), layers.data(), 0.0f, ModelAnimFlags::None));

        // Switching between the authored animations is what runs the cross-fade and the per-animation
        // speed and layer-value overrides, none of which a single-animation model can reach
        for (CritterActionAnim authored : {CritterActionAnim::Idle, CritterActionAnim::Walk, CritterActionAnim::Run, CritterActionAnim::TurnRight, CritterActionAnim::Walk, CritterActionAnim::Idle}) {
            CHECK(model->HasAnimation(state_anim, authored));
            REQUIRE_NOTHROW(ignore_unused(model->PlayAnim(state_anim, authored, layers.data(), 0.0f, ModelAnimFlags::None)));

            for (int32_t frame = 0; frame < 3; frame++) {
                REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
            }

            ignore_unused(model->GetAnimDuration(state_anim, authored));
        }

        // Movement animations are chosen from the authored walk/run pair rather than played by name
        for (int32_t speed : {0, 5, 20}) {
            REQUIRE_NOTHROW(model->SetMovementState(true, speed != 0, speed));
            REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
        }

        REQUIRE_NOTHROW(model->SetMovementState(false, false, 0));

        ignore_unused(model->GetSpriteBounds());
        ignore_unused(model->GetAttachPoints());
    }

    SECTION("PosingAdvancesTheAnimationAndBuildsTheFrame")
    {
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});

        for (int32_t frame = 0; frame < 4; frame++) {
            REQUIRE_NOTHROW(model->PoseSpriteFrame(true));
            std::this_thread::sleep_for(std::chrono::milliseconds {8});
        }

        ignore_unused(model->GetSpriteBounds());
        ignore_unused(model->GetDrawSize());
        ignore_unused(model->NeedDraw());
    }

    SECTION("TheInstanceDrawsThroughTheHeadlessRenderer")
    {
        model->SetupFrame(isize32 {128, 128}, ipos32 {64, 96});
        model->StartMeshGeneration();
        model->PoseSpriteFrame(true);

        // The Null renderer accepts the draw calls, so the mesh combine and submit paths run for real
        REQUIRE_NOTHROW(model->DrawSpriteFrame());

        mat44 proj = client->SprMngr.GetRender().CreateOrthoMatrix(0.0f, 128.0f, 0.0f, 128.0f, -10.0f, 10.0f);
        REQUIRE_NOTHROW(model->DrawInScene(proj, 1.0f));

        REQUIRE_NOTHROW(model->PrewarmParticles());
        REQUIRE_NOTHROW(model->PoseSpriteFrame(false));
        REQUIRE_NOTHROW(model->DrawSpriteFrame());
    }

    SECTION("TheAnimationViewerPreviewsAModelBackedCritter")
    {
        // The viewer selects through a list click or through its persisted "last critter" setting, so the
        // setting is seeded and restored around the run
        string saved_selected_proto;

        {
            SettingsStorage viewer_settings {"AnimationViewer"};
            saved_selected_proto = viewer_settings.GetString("SelectedProto");
            viewer_settings.SetString("SelectedProto", "UnitTestModelCritter");
        }

        auto restore_viewer_settings = scope_exit([&saved_selected_proto]() noexcept {
            safe_call([&saved_selected_proto] {
                SettingsStorage viewer_settings {"AnimationViewer"};
                viewer_settings.SetString("SelectedProto", saved_selected_proto);
            });
        });

        REQUIRE(ImGui::GetCurrentContext() == nullptr);
        ImGuiExt::Init();

        auto destroy_context = scope_exit([]() noexcept {
            safe_call([] {
                if (ImGui::GetCurrentContext() != nullptr) {
                    ImGui::DestroyContext();
                }
            });
        });

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2 {1280.0f, 720.0f};
        io.DeltaTime = 1.0f / 60.0f;
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

        AnimationViewer viewer {client.as_ptr(), &client->SprMngr, &client->ResMngr, &client->GameTime};
        viewer.SetVisible(true);
        viewer.SetFillViewport(true);

        for (int32_t frame = 0; frame < 4; frame++) {
            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(viewer.Draw());
            ImGui::LogFinish();
            ImGui::Render();
            std::this_thread::sleep_for(std::chrono::milliseconds {8});
        }

        CHECK(viewer.IsVisible());
    }

    SECTION("TheSpriteFactoryWrapsTheModelAsASprite")
    {
        // Going through the sprite manager takes the model down the model-sprite path instead of the raw
        // instance one: atlas placement, per-frame update and the sprite-side draw
        shared_ptr<Sprite> sprite = client->SprMngr.LoadSprite(client->Hashes.to_hashed_string(MODEL_PATH), AtlasType::MapSprites, true);
        REQUIRE(static_cast<bool>(sprite));

        CHECK(sprite->GetSize().width > 0);
        CHECK(sprite->GetSize().height > 0);

        REQUIRE_NOTHROW(sprite->Prewarm());
        REQUIRE_NOTHROW(sprite->PlayDefault());

        for (int32_t frame = 0; frame < 4; frame++) {
            REQUIRE_NOTHROW(sprite->Update());
            std::this_thread::sleep_for(std::chrono::milliseconds {8});
        }

        ignore_unused(sprite->IsHitTest(ipos32 {0, 0}));
        ignore_unused(sprite->IsPlaying());
        REQUIRE_NOTHROW(sprite->Stop());
    }
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ModelSpritePreparesItsHitMaskDuringRenderUpdate")
{
    constexpr string_view MESH_PATH = "Models/InitialHitMask.fbx";
    constexpr string_view MODEL_PATH = "Models/InitialHitMask.fo3d";

    vector<uint8_t> mesh_blob = MakeRuntimeModelTriangleMesh();
    vector<pair<string, vector<uint8_t>>> model_resources;
    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {MESH_PATH}, mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh_blob));

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    shared_ptr<Sprite> sprite = client->SprMngr.LoadSprite(client->Hashes.to_hashed_string(MODEL_PATH), AtlasType::MapSprites, true);
    auto model_spr = sprite.dyn_cast<ModelSprite>();
    REQUIRE(static_cast<bool>(model_spr));
    REQUIRE(model_spr->Update());

    SECTION("FirstPictureIsPreparedBeforePicking")
    {
    }
    SECTION("PickingARedrawnPictureDoesNotStartAReadback")
    {
        model_spr->DrawToAtlas();
    }

    // Picking must use the completed mask even after a redraw. GPU work belongs to the render update
    ptr<RenderTexture> atlas_tex = model_spr->GetAtlas()->GetTexture();
    ipos32 origin {iround<int32_t>(atlas_tex->SizeData[0] * model_spr->GetAtlasRect().x), iround<int32_t>(atlas_tex->SizeData[1] * model_spr->GetAtlasRect().y)};
    atlas_tex->UpdateTextureRegion(origin, {1, 1}, vector<ucolor>(1, ucolor {255, 255, 255, 255}));

    CHECK_FALSE(model_spr->IsHitTest({0, 0}));
}
#endif

#if (FO_ENABLE_3D)
TEST_CASE("ModelSpriteHitTestReadsItsMaskFromTheAtlas")
{
    // A model sprite has no pixels on the CPU until its atlas picture comes back from the GPU. The null renderer reads
    // at once, which pins what the mask holds and when it is refreshed; the non-blocking backends only delay the answer
    constexpr string_view MESH_PATH = "Models/HitMask.fbx";
    constexpr string_view MODEL_PATH = "Models/HitMask.fo3d";

    vector<uint8_t> mesh_blob = MakeRuntimeModelTriangleMesh();
    vector<pair<string, vector<uint8_t>>> model_resources;
    model_resources.emplace_back(string {"ModelAnimationInfo.foinfo"}, MakeUnitTestModelAnimationInfo(MODEL_PATH));
    model_resources.emplace_back(string {MESH_PATH}, mesh_blob);
    model_resources.emplace_back(string {MODEL_PATH}, MakeRuntimeModelDescription(MODEL_PATH, MESH_PATH, mesh_blob));

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(model_resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    shared_ptr<Sprite> sprite = client->SprMngr.LoadSprite(client->Hashes.to_hashed_string(MODEL_PATH), AtlasType::MapSprites, true);
    auto model_spr = sprite.dyn_cast<ModelSprite>();
    REQUIRE(static_cast<bool>(model_spr));

    // The null renderer draws nothing, so each picture is painted into the atlas right after the draw that claims it
    auto draw_with_opaque_pixel = [&model_spr](ipos32 opaque_pos) {
        model_spr->DrawToAtlas();

        isize32 size = model_spr->GetSize();
        REQUIRE(size.width >= 2);
        REQUIRE(size.height >= 2);

        ptr<RenderTexture> atlas_tex = model_spr->GetAtlas()->GetTexture();
        ipos32 origin {iround<int32_t>(atlas_tex->SizeData[0] * model_spr->GetAtlasRect().x), iround<int32_t>(atlas_tex->SizeData[1] * model_spr->GetAtlasRect().y)};
        vector<ucolor> pixels(size.square(), ucolor {0, 0, 0, 0});
        pixels[numeric_cast<size_t>(opaque_pos.y) * numeric_cast<size_t>(size.width) + numeric_cast<size_t>(opaque_pos.x)] = ucolor {255, 255, 255, 255};
        atlas_tex->UpdateTextureRegion(origin, size, pixels);

        return origin;
    };

    ipos32 first {0, 0};
    ipos32 second {1, 1};
    ipos32 origin = draw_with_opaque_pixel(first);

    CHECK_FALSE(model_spr->IsHitTest(first));
    REQUIRE(model_spr->Update());
    CHECK(model_spr->IsHitTest(first));
    CHECK_FALSE(model_spr->IsHitTest(second));

    // Without a new picture the mask stands, so picking again reads nothing back, whatever the atlas holds now
    ptr<RenderTexture> atlas_tex = model_spr->GetAtlas()->GetTexture();
    atlas_tex->UpdateTextureRegion(origin + second, {1, 1}, vector<ucolor>(1, ucolor {255, 255, 255, 255}));

    CHECK(model_spr->IsHitTest(first));
    CHECK_FALSE(model_spr->IsHitTest(second));

    // A redraw leaves the mask stale; picking only requests a copy for the next render update
    coarse_sleep(std::chrono::milliseconds {110});
    client->GameTime.FrameAdvance(false);
    draw_with_opaque_pixel(second);

    CHECK(model_spr->IsHitTest(first));
    CHECK_FALSE(model_spr->IsHitTest(second));
    REQUIRE(model_spr->Update());
    CHECK_FALSE(model_spr->IsHitTest(first));
    CHECK(model_spr->IsHitTest(second));
    CHECK_FALSE(model_spr->IsHitTest({-1, 0}));
    CHECK_FALSE(model_spr->IsHitTest({model_spr->GetSize().width, 0}));
}
#endif

TEST_CASE("ClientEngineStartsAndRegistersEntities")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK_FALSE(static_cast<bool>(client->GetCurPlayer()));
    CHECK_FALSE(static_cast<bool>(client->GetCurLocation()));
    CHECK_FALSE(static_cast<bool>(client->GetCurMap()));

    hstring critter_pid = client->Hashes.to_hashed_string("UnitTestClientCritter");
    auto critter_proto = client->GetProtoCritter(critter_pid);
    REQUIRE(static_cast<bool>(critter_proto));

    auto player = safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001});
    auto critter = safe_alloc::make_refcounted<CritterView>(client, ident_t {1002}, critter_proto);

    REQUIRE(client->GetEntity(player->GetId()) == player);
    REQUIRE(client->GetEntity(critter->GetId()) == critter);
    CHECK(critter->GetProtoId() == critter_pid);
    CHECK(critter->GetName() == "UnitTestClientCritter_1002");

    critter->DestroySelf();
    player->DestroySelf();

    CHECK_FALSE(static_cast<bool>(client->GetEntity(ident_t {1002})));
    CHECK_FALSE(static_cast<bool>(client->GetEntity(ident_t {1001})));
}

TEST_CASE("ClientEngineScheduledCallbacksDoNotRunNestedZeroDelayInSamePass")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    int32_t callback_count = 0;

    client->ScheduleDelayedCallback(timespan::zero, [&client, &callback_count] {
        callback_count++;

        client->ScheduleDelayedCallback(timespan::zero, [&callback_count] { callback_count++; });
    });

    client->ProcessScheduledCallbacks();
    CHECK(callback_count == 1);

    client->ProcessScheduledCallbacks();
    CHECK(callback_count == 2);
}

TEST_CASE("ResourceManagerLoadsLegacyCritterAnimations")
{
    // The legacy path only runs under art/critters/ and builds sprite names by index letter, then casts the result
    // to a SpriteSheet — hence the multi-frame fixtures
    constexpr string_view FRM_IND = "_abcdefghijklmnopqrstuvwxyz0123456789";
    constexpr string_view MODEL_NAME = "art/critters/utxx.frm";
    constexpr string_view MODEL_STEM = "art/critters/ut";

    vector<pair<string, vector<uint8_t>>> anim_resources;

    for (int32_t state_anim = 0; state_anim < 14; state_anim++) {
        for (int32_t action_anim = 0; action_anim < 14; action_anim++) {
            string spr_name = strex("{}{}{}.fofrm", MODEL_STEM, FRM_IND[numeric_cast<size_t>(state_anim)], FRM_IND[numeric_cast<size_t>(action_anim)]).str();
            anim_resources.emplace_back(std::move(spr_name), BakerTests::MakeMultiFrameBakedSprite(3));
        }
    }

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(anim_resources)));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    hstring model_name = client->Hashes.to_hashed_string(MODEL_NAME);

    SECTION("AnimationPairsResolveThroughTheLegacyLoader")
    {
        int32_t resolved = 0;

        for (int32_t state_anim = 1; state_anim < 14; state_anim++) {
            for (int32_t action_anim = 1; action_anim < 14; action_anim++) {
                if (client->ResMngr.GetCritterAnimFrames(model_name, static_cast<CritterStateAnim>(state_anim), static_cast<CritterActionAnim>(action_anim), mdir {0})) {
                    resolved++;
                }
            }
        }

        CHECK(resolved > 0);
    }

    SECTION("RepeatedLookupsComeFromTheCache")
    {
        nptr<const SpriteSheet> first = client->ResMngr.GetCritterAnimFrames(model_name, static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), mdir {0});
        nptr<const SpriteSheet> second = client->ResMngr.GetCritterAnimFrames(model_name, static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), mdir {0});
        CHECK(first == second);
    }

    SECTION("AModelWithNoSpritesAnswersEmpty")
    {
        hstring missing = client->Hashes.to_hashed_string("art/critters/nosuchxx.frm");
        CHECK_FALSE(static_cast<bool>(client->ResMngr.GetCritterAnimFrames(missing, static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), mdir {0})));
    }
}

TEST_CASE("FontManagerMeasuresAndSplitsText")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources(MakeUnitTestFontResources()));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    client->FontMngr.BindFoFont(FontType::Default, "UnitTestFont.fofnt", AtlasType::IfaceSprites, false, false, 1.0f);

    SECTION("MetricsAnswerForABoundFont")
    {
        CHECK(client->FontMngr.GetLineHeight(FontType::Default) > 0);
        CHECK(client->FontMngr.GetLinesHeight({200, 100}, "one line", FontType::Default) > 0);
        CHECK(client->FontMngr.GetLinesCount({200, 100}, "one line", FontType::Default) == 1);
        CHECK(client->FontMngr.GetLinesCount({40, 200}, "a sentence long enough to wrap over several lines", FontType::Default) > 1);

        CHECK(client->FontMngr.HaveLetter(FontType::Default, uint32_t {'A'}));
        CHECK_FALSE(client->FontMngr.HaveLetter(FontType::Default, uint32_t {0x4E2D}));
    }

    SECTION("SplittingPaginatesWhatDoesNotFitTheRect")
    {
        // Splitting yields rect-sized pages, not individual lines, so text that fits comes back as one entry
        vector<string> single = client->FontMngr.SplitLines(irect32 {0, 0, 200, 100}, "one line", FontType::Default);
        CHECK(single.size() == 1);

        vector<string> explicit_breaks = client->FontMngr.SplitLines(irect32 {0, 0, 200, 10}, "first\nsecond\nthird", FontType::Default);
        CHECK(explicit_breaks.size() > 1);

        vector<string> wrapped = client->FontMngr.SplitLines(irect32 {0, 0, 40, 10}, "a sentence long enough to wrap over several lines", FontType::Default);
        CHECK(wrapped.size() > 1);

        vector<string> empty = client->FontMngr.SplitLines(irect32 {0, 0, 200, 100}, "", FontType::Default);
        CHECK(empty.empty());
    }

    SECTION("TheBinaryBmfLoaderProducesAUsableFont")
    {
        client->FontMngr.BindBmfFont(FontType::Default, "UnitTestFont.fnt", AtlasType::IfaceSprites, 1.0f);

        CHECK(client->FontMngr.GetLineHeight(FontType::Default) > 0);
        CHECK(client->FontMngr.HaveLetter(FontType::Default, uint32_t {'A'}));
        CHECK(client->FontMngr.GetLinesCount({200, 100}, "bmf line", FontType::Default) == 1);
    }

    SECTION("UnboundSlotsAndMissingFilesAreRejected")
    {
        CHECK_THROWS_AS(client->FontMngr.GetLineHeight(static_cast<FontType>(7)), FontManagerException);
        CHECK_THROWS_AS(client->FontMngr.BindFoFont(FontType::Default, "NoSuchFont.fofnt", AtlasType::IfaceSprites, false, false, 1.0f), FontManagerException);
        CHECK_THROWS_AS(client->FontMngr.BindBmfFont(FontType::Default, "NoSuchFont.fnt", AtlasType::IfaceSprites, 1.0f), FontManagerException);

        // Skip-if-loaded short-circuits before the resource is even looked up
        CHECK_NOTHROW(client->FontMngr.BindFoFont(FontType::Default, "NoSuchFont.fofnt", AtlasType::IfaceSprites, false, true, 1.0f));
    }

    SECTION("EffectOverrideAndTeardownAreAddressable")
    {
        CHECK_NOTHROW(client->FontMngr.SetFontEffect(FontType::Default, nullptr));
        CHECK_NOTHROW(client->FontMngr.ClearFonts());
        CHECK_THROWS_AS(client->FontMngr.GetLineHeight(FontType::Default), FontManagerException);
    }
}

TEST_CASE("MultiFrameSpritesPlayAndCopy")
{
    // A single-frame sprite resolves to an atlas sprite, so sheet playback has nothing to run on until the fixture
    // serves a real multi-frame one
    auto settings = MakeClientTestSettings();

    vector<pair<string, vector<uint8_t>>> sprite_resources;
    sprite_resources.emplace_back(string {"AnimSheet.png"}, BakerTests::MakeMultiFrameBakedSprite(4, 2, 2, 40));

    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(sprite_resources)));

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    auto sheet = client->SprMngr.LoadSprite(client->Hashes.to_hashed_string("AnimSheet.png"), AtlasType::IfaceSprites);
    REQUIRE(sheet);
    CHECK(sheet->GetSize() == isize32 {2, 2});

    SECTION("PlaybackAdvancesThroughTheFramesAndWrapsWhenLooped")
    {
        REQUIRE_NOTHROW(sheet->PlayDefault());
        CHECK(sheet->IsPlaying());

        // The frame clock is driven by the game time, so the update is run over enough frames to wrap
        for (int32_t frame = 0; frame < 24; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(sheet->Update());
        }

        CHECK(sheet->IsPlaying());

        // Seeking addresses a frame directly rather than waiting for the clock
        for (float32_t normalized_time : {0.0f, 0.25f, 0.5f, 0.99f, 1.0f}) {
            REQUIRE_NOTHROW(sheet->SetTime(normalized_time));
            ignore_unused(sheet->Update());
        }

        // A one-shot run stops at the end instead of wrapping
        REQUIRE_NOTHROW(sheet->Play({}, false, false));

        for (int32_t frame = 0; frame < 24; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(sheet->Update());
        }

        // Reversed playback walks the same frames the other way
        REQUIRE_NOTHROW(sheet->Play({}, true, true));

        for (int32_t frame = 0; frame < 12; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(sheet->Update());
        }

        REQUIRE_NOTHROW(sheet->Stop());
        CHECK_FALSE(sheet->IsPlaying());
    }

    SECTION("ACopyPlaysIndependentlyOfTheSpriteItWasMadeFrom")
    {
        auto copy = sheet->MakeCopy();
        REQUIRE(copy);
        CHECK(copy != sheet);

        copy->PlayDefault();
        CHECK(copy->IsPlaying());
        CHECK_FALSE(sheet->IsPlaying());

        for (int32_t frame = 0; frame < 12; frame++) {
            client->GameTime.FrameAdvance(false);
            ignore_unused(copy->Update());
        }

        CHECK(copy->IsPlaying());
    }
}

TEST_CASE("AtlasSpriteFillDataSupportsBakedMeshes")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    frect32 atlas_rect = {0.25f, 0.5f, 0.5f, 0.25f};
    frect32 draw_rect = {100.0f, 200.0f, 20.0f, 40.0f};
    ucolor color_left = {10, 20, 30, 40};
    ucolor color_right = {110, 120, 130, 140};

    SECTION("Absent mesh keeps the legacy quad")
    {
        auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

        size_t index_count = sprite->FillData(draw_buf, draw_rect, {color_left, color_right});

        REQUIRE(index_count == 6);
        REQUIRE(draw_buf->VertCount == 4);
        REQUIRE(draw_buf->IndCount == 6);
        CHECK(draw_buf->Indices[0] == 0);
        CHECK(draw_buf->Indices[1] == 1);
        CHECK(draw_buf->Indices[2] == 3);
        CHECK(draw_buf->Indices[3] == 1);
        CHECK(draw_buf->Indices[4] == 2);
        CHECK(draw_buf->Indices[5] == 3);
        CHECK(draw_buf->Vertices[0].Color == color_left);
        CHECK(draw_buf->Vertices[1].Color == color_left);
        CHECK(draw_buf->Vertices[2].Color == color_right);
        CHECK(draw_buf->Vertices[3].Color == color_right);
    }

    SECTION("Explicit empty mesh emits no draw data")
    {
        auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, SpriteMeshData {});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

        size_t index_count = sprite->FillData(draw_buf, draw_rect, {color_left, color_right});

        CHECK(index_count == 0);
        CHECK_FALSE(sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect).has_value());
        CHECK(draw_buf->VertCount == 0);
        CHECK(draw_buf->IndCount == 0);
    }

    SECTION("Mesh maps positions UVs indices and horizontal light colors")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 10}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
        draw_buf->Vertices.resize(2);
        draw_buf->VertCount = 2;
        draw_buf->Indices.resize(1);
        draw_buf->Indices[0] = 0;
        draw_buf->IndCount = 1;

        size_t index_count = sprite->FillData(draw_buf, draw_rect, {color_left, color_right});

        REQUIRE(index_count == 3);
        REQUIRE(draw_buf->VertCount == 5);
        REQUIRE(draw_buf->IndCount == 4);
        CHECK(draw_buf->Indices[1] == 2);
        CHECK(draw_buf->Indices[2] == 3);
        CHECK(draw_buf->Indices[3] == 4);

        const Vertex2D& left = draw_buf->Vertices[2];
        const Vertex2D& center = draw_buf->Vertices[3];
        const Vertex2D& right = draw_buf->Vertices[4];
        CHECK(left.PosX == Catch::Approx(100.0f));
        CHECK(left.PosY == Catch::Approx(200.0f));
        CHECK(left.TexU == Catch::Approx(0.25f));
        CHECK(left.TexV == Catch::Approx(0.5f));
        CHECK(left.Color == color_left);
        CHECK(center.PosX == Catch::Approx(110.0f));
        CHECK(center.PosY == Catch::Approx(240.0f));
        CHECK(center.TexU == Catch::Approx(0.5f));
        CHECK(center.TexV == Catch::Approx(0.75f));
        CHECK(center.Color == (ucolor {60, 70, 80, 90}));
        CHECK(right.PosX == Catch::Approx(120.0f));
        CHECK(right.PosY == Catch::Approx(200.0f));
        CHECK(right.TexU == Catch::Approx(0.75f));
        CHECK(right.TexV == Catch::Approx(0.5f));
        CHECK(right.Color == color_right);
    }

    SECTION("Cropped mesh preserves source-relative horizontal light colors")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 0};
        mesh.Vertices = {{0, 0}, {3, 10}, {6, 0}};
        mesh.Indices = {0, 1, 2};

        auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {6, 10}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

        CHECK(sprite->GetSize() == isize32 {10, 10});
        CHECK(sprite->GetOffset() == ipos32 {});
        REQUIRE(sprite->FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        REQUIRE(draw_buf->VertCount == 3);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[1].PosX == Catch::Approx(110.0f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(116.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[1].TexU == Catch::Approx(0.5f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[0].Color == (ucolor {30, 40, 50, 60}));
        CHECK(draw_buf->Vertices[1].Color == (ucolor {60, 70, 80, 90}));
        CHECK(draw_buf->Vertices[2].Color == (ucolor {90, 100, 110, 120}));
    }

    SECTION("Cropped mesh region preserves logical source coordinates")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 3};
        mesh.Vertices = {{0, 0}, {6, 0}, {0, 5}};
        mesh.Indices = {0, 1, 2};

        auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {6, 5}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
        optional<AtlasSpriteRegion> region = sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect);

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(104.0f));
        CHECK(region->DrawRect.y == Catch::Approx(212.0f));
        CHECK(region->DrawRect.width == Catch::Approx(12.0f));
        CHECK(region->DrawRect.height == Catch::Approx(20.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f));
        CHECK(region->TextureRect.width == Catch::Approx(0.5f));
        CHECK(region->TextureRect.height == Catch::Approx(0.25f));
        REQUIRE(sprite->FillRegionData(draw_buf, {0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect, color_left) == 6);
        REQUIRE(draw_buf->VertCount == 4);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[0].PosY == Catch::Approx(232.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[0].TexV == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[1].PosX == Catch::Approx(104.0f));
        CHECK(draw_buf->Vertices[1].PosY == Catch::Approx(212.0f));
        CHECK(draw_buf->Vertices[1].TexU == Catch::Approx(0.25f));
        CHECK(draw_buf->Vertices[1].TexV == Catch::Approx(0.5f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(116.0f));
        CHECK(draw_buf->Vertices[2].PosY == Catch::Approx(212.0f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.75f));
        CHECK(draw_buf->Vertices[2].TexV == Catch::Approx(0.5f));
    }

    SECTION("Expanded mesh region clips atlas padding outside the logical source")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {-2, -1};
        mesh.Vertices = {{0, 0}, {14, 0}, {0, 13}};
        mesh.Indices = {0, 1, 2};

        auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {14, 13}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
        optional<AtlasSpriteRegion> region = sprite->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect);

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(100.0f));
        CHECK(region->DrawRect.y == Catch::Approx(200.0f));
        CHECK(region->DrawRect.width == Catch::Approx(20.0f));
        CHECK(region->DrawRect.height == Catch::Approx(40.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f + 0.5f * 2.0f / 14.0f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f + 0.25f / 13.0f));
        CHECK(region->TextureRect.width == Catch::Approx(0.5f * 10.0f / 14.0f));
        CHECK(region->TextureRect.height == Catch::Approx(0.25f * 10.0f / 13.0f));
        REQUIRE(sprite->FillRegionData(draw_buf, {0.0f, 0.0f}, {1.0f, 1.0f}, draw_rect, color_left) == 6);
        REQUIRE(draw_buf->VertCount == 4);
        CHECK(draw_buf->Vertices[0].PosX == Catch::Approx(100.0f));
        CHECK(draw_buf->Vertices[0].PosY == Catch::Approx(240.0f));
        CHECK(draw_buf->Vertices[0].TexU == Catch::Approx(0.25f + 0.5f * 2.0f / 14.0f));
        CHECK(draw_buf->Vertices[0].TexV == Catch::Approx(0.5f + 0.25f * 11.0f / 13.0f));
        CHECK(draw_buf->Vertices[2].PosX == Catch::Approx(120.0f));
        CHECK(draw_buf->Vertices[2].PosY == Catch::Approx(200.0f));
        CHECK(draw_buf->Vertices[2].TexU == Catch::Approx(0.25f + 0.5f * 12.0f / 14.0f));
        CHECK(draw_buf->Vertices[2].TexV == Catch::Approx(0.5f + 0.25f / 13.0f));
    }

    SECTION("Partial logical region intersects the cropped atlas frame")
    {
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.SourceOffset = {2, 3};
        mesh.Vertices = {{0, 0}, {6, 0}, {0, 5}};
        mesh.Indices = {0, 1, 2};

        auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {6, 5}, ipos32 {}, nullptr, nullptr, atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
        optional<AtlasSpriteRegion> region = sprite->ResolveRegion({0.1f, 0.2f}, {0.5f, 0.6f}, draw_rect);

        REQUIRE(region.has_value());
        CHECK(region->DrawRect.x == Catch::Approx(105.0f));
        CHECK(region->DrawRect.y == Catch::Approx(210.0f));
        CHECK(region->DrawRect.width == Catch::Approx(15.0f));
        CHECK(region->DrawRect.height == Catch::Approx(30.0f));
        CHECK(region->TextureRect.x == Catch::Approx(0.25f));
        CHECK(region->TextureRect.y == Catch::Approx(0.5f));
        CHECK(region->TextureRect.width == Catch::Approx(0.25f));
        CHECK(region->TextureRect.height == Catch::Approx(0.15f));
    }

    SECTION("Live atlas allocation observes sprite mesh metadata for dump lifetime")
    {
        TextureAtlasLayout layout {{12, 12}};
        auto atlas_allocation = layout.Allocate({12, 12});
        REQUIRE(atlas_allocation);
        nptr<TextureAtlasLayout::Allocation> allocation_observer = atlas_allocation.as_nptr();
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 5}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        {
            auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, std::move(atlas_allocation), atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});
            auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);

            REQUIRE(allocation_observer->GetSpriteMesh());
            CHECK(allocation_observer->GetSpriteMesh()->Vertices.size() == 3);
            CHECK(sprite->FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        }

        CHECK_FALSE(allocation_observer->IsActive());
        CHECK_FALSE(allocation_observer->GetSpriteMesh());
    }

    SECTION("Moving an atlas sprite rebinds the allocation mesh observer")
    {
        TextureAtlasLayout layout {{12, 12}};
        auto atlas_allocation = layout.Allocate({12, 12});
        REQUIRE(atlas_allocation);
        nptr<TextureAtlasLayout::Allocation> allocation_observer = atlas_allocation.as_nptr();
        SpriteMeshData mesh;
        mesh.SourceSize = {10, 10};
        mesh.Vertices = {{0, 0}, {5, 5}, {10, 0}};
        mesh.Indices = {0, 1, 2};

        {
            AtlasSprite source {&client->SprMngr, isize32 {10, 10}, ipos32 {}, nullptr, std::move(atlas_allocation), atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)}};
            nptr<const SpriteMeshData> source_mesh = allocation_observer->GetSpriteMesh();
            AtlasSprite moved {std::move(source)};

            REQUIRE(allocation_observer->GetSpriteMesh());
            CHECK(allocation_observer->GetSpriteMesh() != source_mesh);
            auto draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
            CHECK(moved.FillData(draw_buf, draw_rect, {color_left, color_right}) == 3);
        }

        CHECK_FALSE(allocation_observer->IsActive());
        CHECK_FALSE(allocation_observer->GetSpriteMesh());
    }
}

TEST_CASE("DefaultSpriteFactoryValidatesBakedMeshPayload")
{
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    SpriteMeshData mesh;
    mesh.SourceSize = {2, 2};
    mesh.Vertices = {{0, 0}, {2, 0}, {0, 2}};
    mesh.Indices = {0, 1, 2};

    vector<uint8_t> valid_blob = BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Mesh, mesh);
    SpriteMeshData cropped_mesh;
    cropped_mesh.SourceSize = {4, 3};
    cropped_mesh.SourceOffset = {1, -1};
    cropped_mesh.Vertices = {{0, 0}, {3, 0}, {0, 3}};
    cropped_mesh.Indices = {0, 1, 2};
    static_assert(SPRITE_RESOURCE_VERSION == 3);
    constexpr size_t mesh_kind_offset = 30 + 2 * 2 * sizeof(ucolor);
    constexpr size_t mesh_vertex_count_offset = mesh_kind_offset + 1;
    constexpr size_t mesh_index_count_offset = mesh_vertex_count_offset + sizeof(uint16_t);
    constexpr size_t mesh_source_size_offset = mesh_index_count_offset + sizeof(uint32_t);
    constexpr size_t mesh_source_offset_offset = mesh_source_size_offset + sizeof(uint16_t) * 2;
    constexpr size_t mesh_vertices_offset = mesh_source_offset_offset + sizeof(int32_t) * 2;
    constexpr size_t mesh_indices_offset = mesh_vertices_offset + 3 * sizeof(uint16_t) * 2;

    auto write_u16 = [](vector<uint8_t>& data, size_t offset, uint16_t value) {
        data[offset] = numeric_cast<uint8_t>(value & 0xFF);
        data[offset + 1] = numeric_cast<uint8_t>(value >> 8);
    };
    auto write_u32 = [](vector<uint8_t>& data, size_t offset, uint32_t value) {
        data[offset] = numeric_cast<uint8_t>(value & 0xFF);
        data[offset + 1] = numeric_cast<uint8_t>((value >> 8) & 0xFF);
        data[offset + 2] = numeric_cast<uint8_t>((value >> 16) & 0xFF);
        data[offset + 3] = numeric_cast<uint8_t>(value >> 24);
    };

    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("PolygonSpriteResources");
    source->AddFile("Quad.png", BakerTests::MakeMinimalBakedSprite(2, 2));
    source->AddFile("Empty.png", BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Empty));
    source->AddFile("ValidMesh.png", valid_blob);
    source->AddFile("CroppedMesh.png", BakerTests::MakeMinimalBakedSprite(3, 3, SpriteMeshKind::Mesh, cropped_mesh));

    vector<uint8_t> bad_version = valid_blob;
    bad_version[1]++;
    source->AddFile("BadVersion.png", std::move(bad_version));

    vector<uint8_t> bad_kind = valid_blob;
    bad_kind[mesh_kind_offset] = 0xFF;
    source->AddFile("BadKind.png", std::move(bad_kind));

    vector<uint8_t> bad_vertex_count = valid_blob;
    write_u16(bad_vertex_count, mesh_vertex_count_offset, uint16_t {2});
    source->AddFile("BadVertexCount.png", std::move(bad_vertex_count));

    vector<uint8_t> bad_index_count = valid_blob;
    write_u32(bad_index_count, mesh_index_count_offset, uint32_t {4});
    source->AddFile("BadIndexCount.png", std::move(bad_index_count));

    vector<uint8_t> implausible_index_count = valid_blob;
    write_u32(implausible_index_count, mesh_index_count_offset, uint32_t {21});
    source->AddFile("ImplausibleIndexCount.png", std::move(implausible_index_count));

    vector<uint8_t> bad_source_size = valid_blob;
    write_u16(bad_source_size, mesh_source_size_offset, uint16_t {0});
    source->AddFile("BadSourceSize.png", std::move(bad_source_size));

    vector<uint8_t> bad_source_offset = valid_blob;
    write_u32(bad_source_offset, mesh_source_offset_offset, uint32_t {2});
    source->AddFile("BadSourceOffset.png", std::move(bad_source_offset));

    vector<uint8_t> bad_coordinate = valid_blob;
    write_u16(bad_coordinate, mesh_vertices_offset, uint16_t {3});
    source->AddFile("BadCoordinate.png", std::move(bad_coordinate));

    vector<uint8_t> bad_index = valid_blob;
    write_u16(bad_index, mesh_indices_offset, uint16_t {3});
    source->AddFile("BadIndex.png", std::move(bad_index));

    vector<uint8_t> degenerate_triangle = valid_blob;
    write_u16(degenerate_triangle, mesh_vertices_offset + 2 * sizeof(uint16_t) * 2, uint16_t {1});
    write_u16(degenerate_triangle, mesh_vertices_offset + 2 * sizeof(uint16_t) * 2 + sizeof(uint16_t), uint16_t {0});
    source->AddFile("DegenerateTriangle.png", std::move(degenerate_triangle));

    SpriteMeshData inconsistent_winding_mesh;
    inconsistent_winding_mesh.Vertices = {{0, 0}, {2, 0}, {0, 2}, {2, 2}};
    inconsistent_winding_mesh.Indices = {0, 1, 2, 1, 2, 3};
    source->AddFile("InconsistentWinding.png", BakerTests::MakeMinimalBakedSprite(2, 2, SpriteMeshKind::Mesh, inconsistent_winding_mesh));

    vector<uint8_t> trailing_data = valid_blob;
    trailing_data.emplace_back(uint8_t {0});
    source->AddFile("TrailingData.png", std::move(trailing_data));

    vector<uint8_t> truncated_payload = valid_blob;
    truncated_payload.resize(mesh_indices_offset);
    source->AddFile("TruncatedPayload.png", std::move(truncated_payload));

    client->SprMngr.GetResources()->AddCustomSource(std::move(source));
    DefaultSpriteFactory factory {&client->SprMngr};
    auto load = [&client, &factory](string_view path) { return factory.LoadSprite(client->Hashes.to_hashed_string(path), AtlasType::MapSprites); };

    auto valid_sprite = load("ValidMesh.png");
    REQUIRE(static_cast<bool>(valid_sprite));
    auto valid_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(valid_sprite->FillData(valid_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 3);

    auto cropped_sprite = load("CroppedMesh.png");
    REQUIRE(cropped_sprite);
    CHECK(cropped_sprite->GetSize() == cropped_mesh.SourceSize);
    CHECK(cropped_sprite->GetOffset() == ipos32 {0, 1});
    CHECK_FALSE(cropped_sprite->IsHitTest({0, 0}));
    CHECK(cropped_sprite->IsHitTest({1, 0}));
    CHECK(cropped_sprite->IsHitTest({3, 1}));
    CHECK_FALSE(cropped_sprite->IsHitTest({3, 2}));
    auto cropped_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    REQUIRE(cropped_sprite->FillData(cropped_draw_buf, frect32 {0.0f, 0.0f, 4.0f, 3.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 3);
    REQUIRE(cropped_draw_buf->VertCount == 3);
    CHECK(cropped_draw_buf->Vertices[0].PosX == Catch::Approx(1.0f));
    CHECK(cropped_draw_buf->Vertices[0].PosY == Catch::Approx(-1.0f));
    CHECK(cropped_draw_buf->Vertices[1].PosX == Catch::Approx(4.0f));
    CHECK(cropped_draw_buf->Vertices[1].PosY == Catch::Approx(-1.0f));
    CHECK(cropped_draw_buf->Vertices[2].PosX == Catch::Approx(1.0f));
    CHECK(cropped_draw_buf->Vertices[2].PosY == Catch::Approx(2.0f));

    auto restored_image = client->SprMngr.LoadSpriteAsQuad(client->Hashes.to_hashed_string("CroppedMesh.png"), AtlasType::IfaceSprites);
    REQUIRE(restored_image);
    CHECK(restored_image->GetSize() == cropped_mesh.SourceSize);
    CHECK(restored_image->GetAtlasRect().width * restored_image->GetAtlas()->GetTexture()->SizeData[0] == Catch::Approx(4.0f));
    CHECK(restored_image->GetAtlasRect().height * restored_image->GetAtlas()->GetTexture()->SizeData[1] == Catch::Approx(3.0f));
    optional<AtlasSpriteRegion> restored_region = restored_image->ResolveRegion({0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 4.0f, 3.0f});
    REQUIRE(restored_region.has_value());
    CHECK(restored_region->DrawRect == frect32 {0.0f, 0.0f, 4.0f, 3.0f});
    CHECK(restored_region->TextureRect == restored_image->GetAtlasRect());
    auto restored_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(restored_image->FillData(restored_draw_buf, frect32 {0.0f, 0.0f, 4.0f, 3.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 6);

    auto quad_sprite = load("Quad.png");
    REQUIRE(static_cast<bool>(quad_sprite));
    auto quad_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(quad_sprite->FillData(quad_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 6);

    auto empty_sprite = load("Empty.png");
    REQUIRE(static_cast<bool>(empty_sprite));
    auto empty_draw_buf = client->SprMngr.GetRender().CreateDrawBuffer(false);
    CHECK(empty_sprite->FillData(empty_draw_buf, frect32 {0.0f, 0.0f, 2.0f, 2.0f}, {ucolor {0, 0, 0}, ucolor {255, 255, 255}}) == 0);

    CHECK_THROWS(load("BadVersion.png"));
    CHECK_THROWS(load("BadKind.png"));
    CHECK_THROWS(load("BadVertexCount.png"));
    CHECK_THROWS(load("BadIndexCount.png"));
    CHECK_THROWS(load("ImplausibleIndexCount.png"));
    CHECK_THROWS(load("BadSourceSize.png"));
    CHECK_THROWS(load("BadSourceOffset.png"));
    CHECK_THROWS(load("BadCoordinate.png"));
    CHECK_THROWS(load("BadIndex.png"));
    CHECK_THROWS(load("DegenerateTriangle.png"));
    CHECK_THROWS(load("InconsistentWinding.png"));
    CHECK_THROWS(load("TrailingData.png"));
    CHECK_THROWS(load("TruncatedPayload.png"));
}

TEST_CASE("SpriteManagerMapsPolygonAtlasPatternsAndPaddedEffects")
{
    SpriteMeshData mesh;
    mesh.SourceSize = {4, 3};
    mesh.SourceOffset = {1, 0};
    mesh.Vertices = {{0, 0}, {3, 0}, {0, 3}};
    mesh.Indices = {0, 1, 2};

    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings, MakeClientTestResources({{"PatternMesh.png", BakerTests::MakeMinimalBakedSprite(3, 3, SpriteMeshKind::Mesh, mesh)}}));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    RecordingQuadEffect effect;

    auto sprite = client->SprMngr.LoadSprite("PatternMesh.png", AtlasType::IfaceSprites);
    REQUIRE(sprite);
    auto atlas_sprite = sprite.dyn_cast<AtlasSprite>();
    REQUIRE(atlas_sprite);
    sprite->SetDrawEffect(make_nptr(&effect));

    client->SprMngr.DrawSpritePattern(sprite, {10, 20}, {7, 5}, {4, 3}, ucolor {255, 255, 255, 255});
    client->SprMngr.Flush();

    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& pattern_draw = effect.Draws.front();
    REQUIRE(pattern_draw.Vertices.size() == 16);
    REQUIRE(pattern_draw.Indices.size() == 24);
    CHECK(pattern_draw.StartIndex == 0);
    REQUIRE(pattern_draw.IndicesToDraw.has_value());
    CHECK(*pattern_draw.IndicesToDraw == 24);
    CHECK(pattern_draw.CustomTexture == atlas_sprite->GetBatchTexture());

    frect32 atlas_rect = atlas_sprite->GetAtlasRect();
    auto check_quad = [](const RecordedQuadDraw& draw, size_t vertex, frect32 draw_rect, frect32 texture_rect) {
        REQUIRE(vertex + 3 < draw.Vertices.size());
        CHECK(draw.Vertices[vertex + 0].PosX == Catch::Approx(draw_rect.x));
        CHECK(draw.Vertices[vertex + 0].PosY == Catch::Approx(draw_rect.y + draw_rect.height));
        CHECK(draw.Vertices[vertex + 0].TexU == Catch::Approx(texture_rect.x));
        CHECK(draw.Vertices[vertex + 0].TexV == Catch::Approx(texture_rect.y + texture_rect.height));
        CHECK(draw.Vertices[vertex + 1].PosX == Catch::Approx(draw_rect.x));
        CHECK(draw.Vertices[vertex + 1].PosY == Catch::Approx(draw_rect.y));
        CHECK(draw.Vertices[vertex + 1].TexU == Catch::Approx(texture_rect.x));
        CHECK(draw.Vertices[vertex + 1].TexV == Catch::Approx(texture_rect.y));
        CHECK(draw.Vertices[vertex + 2].PosX == Catch::Approx(draw_rect.x + draw_rect.width));
        CHECK(draw.Vertices[vertex + 2].PosY == Catch::Approx(draw_rect.y));
        CHECK(draw.Vertices[vertex + 2].TexU == Catch::Approx(texture_rect.x + texture_rect.width));
        CHECK(draw.Vertices[vertex + 2].TexV == Catch::Approx(texture_rect.y));
        CHECK(draw.Vertices[vertex + 3].PosX == Catch::Approx(draw_rect.x + draw_rect.width));
        CHECK(draw.Vertices[vertex + 3].PosY == Catch::Approx(draw_rect.y + draw_rect.height));
        CHECK(draw.Vertices[vertex + 3].TexU == Catch::Approx(texture_rect.x + texture_rect.width));
        CHECK(draw.Vertices[vertex + 3].TexV == Catch::Approx(texture_rect.y + texture_rect.height));
    };

    check_quad(pattern_draw, 0, {11.0f, 20.0f, 3.0f, 3.0f}, atlas_rect);
    check_quad(pattern_draw, 4, {15.0f, 20.0f, 2.0f, 3.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width * 2.0f / 3.0f, atlas_rect.height});
    check_quad(pattern_draw, 8, {11.0f, 23.0f, 3.0f, 2.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width, atlas_rect.height * 2.0f / 3.0f});
    check_quad(pattern_draw, 12, {15.0f, 23.0f, 2.0f, 2.0f}, {atlas_rect.x, atlas_rect.y, atlas_rect.width * 2.0f / 3.0f, atlas_rect.height * 2.0f / 3.0f});

    effect.Draws.clear();
    REQUIRE(client->SprMngr.DrawSpriteRegion(sprite, {0.0f, 0.0f}, {1.0f, 1.0f}, {50.0f, 60.0f}, {4.0f, 3.0f}, ucolor {255, 255, 255, 255}));
    client->SprMngr.Flush();
    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& region_draw = effect.Draws.front();
    REQUIRE(region_draw.Vertices.size() == 4);
    REQUIRE(region_draw.Indices.size() == 6);
    check_quad(region_draw, 0, {51.0f, 60.0f, 3.0f, 3.0f}, atlas_rect);

    effect.Draws.clear();
    effect.SpriteBorderBuf.reset();
    constexpr int32_t padding = 2;
    client->SprMngr.DrawSpriteWithEffect(sprite, {100, 200}, ucolor {255, 255, 255, 255}, make_ptr(&effect), padding);

    REQUIRE(effect.Draws.size() == 1);
    const RecordedQuadDraw& effect_draw = effect.Draws.front();
    REQUIRE(effect_draw.Vertices.size() == 4);
    REQUIRE(effect_draw.Indices.size() == 6);
    CHECK(effect_draw.StartIndex == 0);
    CHECK_FALSE(effect_draw.IndicesToDraw.has_value());
    CHECK(effect_draw.CustomTexture == atlas_sprite->GetBatchTexture());

    float32_t texture_padding_x = atlas_sprite->GetAtlas()->GetTexture()->SizeData[2] * numeric_cast<float32_t>(padding);
    float32_t texture_padding_y = atlas_sprite->GetAtlas()->GetTexture()->SizeData[3] * numeric_cast<float32_t>(padding);
    frect32 effect_draw_rect {99.0f, 198.0f, 7.0f, 7.0f};
    frect32 effect_texture_rect {atlas_rect.x - texture_padding_x, atlas_rect.y - texture_padding_y, atlas_rect.width + texture_padding_x * 2.0f, atlas_rect.height + texture_padding_y * 2.0f};

    CHECK(effect_draw.Vertices[0].PosX == Catch::Approx(effect_draw_rect.x));
    CHECK(effect_draw.Vertices[0].PosY == Catch::Approx(effect_draw_rect.y + effect_draw_rect.height));
    CHECK(effect_draw.Vertices[0].TexU == Catch::Approx(effect_texture_rect.x));
    CHECK(effect_draw.Vertices[0].TexV == Catch::Approx(effect_texture_rect.y + effect_texture_rect.height));
    CHECK(effect_draw.Vertices[1].PosX == Catch::Approx(effect_draw_rect.x));
    CHECK(effect_draw.Vertices[1].PosY == Catch::Approx(effect_draw_rect.y));
    CHECK(effect_draw.Vertices[1].TexU == Catch::Approx(effect_texture_rect.x));
    CHECK(effect_draw.Vertices[1].TexV == Catch::Approx(effect_texture_rect.y));
    CHECK(effect_draw.Vertices[2].PosX == Catch::Approx(effect_draw_rect.x + effect_draw_rect.width));
    CHECK(effect_draw.Vertices[2].PosY == Catch::Approx(effect_draw_rect.y));
    CHECK(effect_draw.Vertices[2].TexU == Catch::Approx(effect_texture_rect.x + effect_texture_rect.width));
    CHECK(effect_draw.Vertices[2].TexV == Catch::Approx(effect_texture_rect.y));
    CHECK(effect_draw.Vertices[3].PosX == Catch::Approx(effect_draw_rect.x + effect_draw_rect.width));
    CHECK(effect_draw.Vertices[3].PosY == Catch::Approx(effect_draw_rect.y + effect_draw_rect.height));
    CHECK(effect_draw.Vertices[3].TexU == Catch::Approx(effect_texture_rect.x + effect_texture_rect.width));
    CHECK(effect_draw.Vertices[3].TexV == Catch::Approx(effect_texture_rect.y + effect_texture_rect.height));

    REQUIRE(effect.SpriteBorderBuf.has_value());
    CHECK(effect.SpriteBorderBuf->SpriteBorder[0] == Catch::Approx(atlas_rect.x));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[1] == Catch::Approx(atlas_rect.y));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[2] == Catch::Approx(atlas_rect.x + atlas_rect.width));
    CHECK(effect.SpriteBorderBuf->SpriteBorder[3] == Catch::Approx(atlas_rect.y + atlas_rect.height));
    sprite->SetDrawEffect(nullptr);
}

TEST_CASE("SpriteWireframeRendersThroughPrimitiveOverlay")
{
    auto settings = MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Render.DrawWireframe, true);
    auto client = MakeClientEngine(settings);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    SpriteMeshData mesh;
    mesh.SourceSize = {10, 10};
    mesh.Vertices = {{0, 0}, {5, 10}, {10, 0}};
    mesh.Indices = {0, 1, 2};

    auto [atlas, atlas_allocation, atlas_pos] = client->SprMngr.GetAtlasMngr()->FindAtlasPlace(AtlasType::OneImage, {10, 10});
    CHECK(atlas->GetSize() == isize32 {12, 12});
    CHECK(atlas_pos == ipos32 {1, 1});
    CHECK(atlas_allocation->GetPosition() == ipos32 {0, 0});
    CHECK(atlas_allocation->GetSize() == isize32 {12, 12});
    frect32 sprite_atlas_rect {
        numeric_cast<float32_t>(atlas_pos.x) / numeric_cast<float32_t>(atlas->GetSize().width),
        numeric_cast<float32_t>(atlas_pos.y) / numeric_cast<float32_t>(atlas->GetSize().height),
        10.0f / numeric_cast<float32_t>(atlas->GetSize().width),
        10.0f / numeric_cast<float32_t>(atlas->GetSize().height),
    };
    auto sprite = safe_alloc::make_shared<AtlasSprite>(&client->SprMngr, isize32 {10, 10}, ipos32 {}, atlas, std::move(atlas_allocation), sprite_atlas_rect, vector<bool> {}, optional<SpriteMeshData> {std::move(mesh)});

    client->SprMngr.DrawSprite(sprite, {2, 3}, ucolor {255, 255, 255});
    CHECK_NOTHROW(client->SprMngr.Flush());
}

FO_END_NAMESPACE
