//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "Common.h"
#include "ModelMeshData.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

static constexpr uint32_t MODEL_MESH_TEST_PAYLOAD = 0x78563412U;

static auto MakeModelMeshRoundTripData() -> ModelMeshData
{
    FO_STACK_TRACE_ENTRY();

    ModelMeshData data {.RootBone = SafeAlloc::MakeUnique<ModelMeshBoneData>()};
    data.RootBone->Name = "Root";
    data.RootBone->TransformationMatrix = mat44 {1.0f};
    data.RootBone->GlobalTransformationMatrix = mat44 {1.0f};
    data.RootBone->AttachedMesh.emplace();
    auto& mesh = *data.RootBone->AttachedMesh;
    auto& vertex = mesh.Vertices.emplace_back();
    vertex.Position = vec3 {1.0f, 2.0f, 3.0f};
    vertex.BlendWeights[0] = 1.0f;
    mesh.Indices.emplace_back(ModelMeshIndexData {0});
    mesh.DiffuseTexture = "Test.png";
    mesh.SkinBoneNames.emplace_back("Root");
    mesh.SkinBoneOffsets.emplace_back(mat44 {1.0f});

    auto child = SafeAlloc::MakeUnique<ModelMeshBoneData>();
    child->Name = "Child";
    child->TransformationMatrix = mat44 {1.0f};
    child->GlobalTransformationMatrix = mat44 {1.0f};
    data.RootBone->Children.emplace_back(std::move(child));
    return data;
}

static auto MakeModelMeshTestData(const array<uint8_t, 8>& magic, uint16_t schema, uint16_t flags) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    DataWriter writer {data};
    writer.WriteBytes({magic.data(), magic.size()});
    writer.Write<uint16_t>(schema);
    writer.Write<uint16_t>(flags);
    writer.Write<uint32_t>(MODEL_MESH_TEST_PAYLOAD);
    return data;
}

TEST_CASE("ModelMeshDataWireHeader")
{
    SECTION("Round-trips and leaves the reader at the mesh payload")
    {
        vector<uint8_t> data;
        DataWriter writer {data};
        WriteModelMeshHeader(writer);
        writer.Write<uint32_t>(MODEL_MESH_TEST_PAYLOAD);

        REQUIRE(data.size() == MODEL_MESH_HEADER_SIZE + sizeof(uint32_t));
        CHECK(std::equal(MODEL_MESH_MAGIC.begin(), MODEL_MESH_MAGIC.end(), data.begin()));

        DataReader reader {{data.data(), data.size()}};
        REQUIRE_NOTHROW(ReadModelMeshHeader(reader, "Models/Test.fbx"));
        CHECK(reader.Read<uint32_t>() == MODEL_MESH_TEST_PAYLOAD);
        CHECK_NOTHROW(reader.VerifyEnd());
    }

    SECTION("Rejects wrong magic")
    {
        array<uint8_t, 8> magic = MODEL_MESH_MAGIC;
        magic.front() = uint8_t {'X'};
        vector<uint8_t> data = MakeModelMeshTestData(magic, MODEL_MESH_SCHEMA_VERSION, MODEL_MESH_SUPPORTED_FLAGS);
        DataReader reader {{data.data(), data.size()}};
        CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/WrongMagic.fbx"), ModelMeshDataException);
        CHECK(reader.Read<uint8_t>() == magic.front());
    }

    SECTION("Rejects unsupported schema")
    {
        vector<uint8_t> data = MakeModelMeshTestData(MODEL_MESH_MAGIC, uint16_t {MODEL_MESH_SCHEMA_VERSION + 1}, MODEL_MESH_SUPPORTED_FLAGS);
        DataReader reader {{data.data(), data.size()}};
        CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/WrongSchema.fbx"), ModelMeshDataException);
    }

    SECTION("Rejects unsupported flags")
    {
        vector<uint8_t> data = MakeModelMeshTestData(MODEL_MESH_MAGIC, MODEL_MESH_SCHEMA_VERSION, uint16_t {1});
        DataReader reader {{data.data(), data.size()}};
        CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/WrongFlags.fbx"), ModelMeshDataException);
    }

    SECTION("Rejects every truncated header length")
    {
        vector<uint8_t> complete_data = MakeModelMeshTestData(MODEL_MESH_MAGIC, MODEL_MESH_SCHEMA_VERSION, MODEL_MESH_SUPPORTED_FLAGS);

        for (size_t size = 0; size < MODEL_MESH_HEADER_SIZE; size++) {
            DataReader reader {{complete_data.data(), size}};
            CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/Truncated.fbx"), ModelMeshDataException);
        }
    }

    SECTION("Rejects the removed headerless mesh layout")
    {
        vector<uint8_t> legacy_data;
        DataWriter writer {legacy_data};
        writer.WriteString("Root");
        writer.Write<mat44>(mat44 {1.0f});

        DataReader reader {{legacy_data.data(), legacy_data.size()}};
        CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/Legacy.fbx"), ModelMeshDataException);
    }
}

TEST_CASE("ModelMeshDataWirePayload")
{
    SECTION("Round-trips the complete mesh hierarchy")
    {
        ModelMeshData source = MakeModelMeshRoundTripData();
        vector<uint8_t> bytes;
        DataWriter writer {bytes};
        REQUIRE_NOTHROW(WriteModelMeshData(writer, source, "Models/Test.fbx"));

        DataReader reader {{bytes.data(), bytes.size()}};
        ModelMeshData decoded;
        REQUIRE_NOTHROW(decoded = ReadModelMeshData(reader, "Models/Test.fbx"));
        REQUIRE(decoded.RootBone);
        CHECK(decoded.RootBone->Name == "Root");
        REQUIRE(decoded.RootBone->AttachedMesh);
        CHECK(decoded.RootBone->AttachedMesh->Vertices.size() == 1);
        vec3 expected_position {1.0f, 2.0f, 3.0f};
        CHECK(decoded.RootBone->AttachedMesh->Vertices.front().Position == expected_position);
        CHECK(decoded.RootBone->AttachedMesh->Indices == vector<ModelMeshIndexData> {0});
        CHECK(decoded.RootBone->AttachedMesh->DiffuseTexture == "Test.png");
        CHECK(decoded.RootBone->AttachedMesh->SkinBoneNames == vector<string> {"Root"});
        CHECK(decoded.RootBone->AttachedMesh->SkinBoneOffsets.size() == 1);
        REQUIRE(decoded.RootBone->Children.size() == 1);
        CHECK(decoded.RootBone->Children.front()->Name == "Child");
        CHECK(reader.GetUnreadSize() == 0);
    }

    SECTION("Preserves the schema-1 byte layout")
    {
        ModelMeshData source = MakeModelMeshRoundTripData();
        vector<uint8_t> codec_bytes;
        DataWriter codec_writer {codec_bytes};
        WriteModelMeshData(codec_writer, source, "Models/Test.fbx");

        vector<uint8_t> legacy_writer_bytes;
        DataWriter legacy_writer {legacy_writer_bytes};
        WriteModelMeshHeader(legacy_writer);
        legacy_writer.WriteString(source.RootBone->Name);
        legacy_writer.Write<mat44>(source.RootBone->TransformationMatrix);
        legacy_writer.Write<mat44>(source.RootBone->GlobalTransformationMatrix);
        legacy_writer.Write<uint8_t>(uint8_t {1});
        const ModelMeshGeometryData& mesh = *source.RootBone->AttachedMesh;
        legacy_writer.Write<uint32_t>(numeric_cast<uint32_t>(mesh.Vertices.size()));
        legacy_writer.WriteObjectVector(mesh.Vertices);
        legacy_writer.Write<uint32_t>(numeric_cast<uint32_t>(mesh.Indices.size()));
        legacy_writer.WriteObjectVector(mesh.Indices);
        legacy_writer.WriteString(mesh.DiffuseTexture);
        legacy_writer.Write<uint32_t>(numeric_cast<uint32_t>(mesh.SkinBoneNames.size()));

        for (const string& skin_bone : mesh.SkinBoneNames) {
            legacy_writer.WriteString(skin_bone);
        }

        legacy_writer.Write<uint32_t>(numeric_cast<uint32_t>(mesh.SkinBoneOffsets.size()));
        legacy_writer.WriteObjectVector(mesh.SkinBoneOffsets);
        legacy_writer.Write<uint32_t>(uint32_t {1});
        legacy_writer.WriteString(source.RootBone->Children.front()->Name);
        legacy_writer.Write<mat44>(source.RootBone->Children.front()->TransformationMatrix);
        legacy_writer.Write<mat44>(source.RootBone->Children.front()->GlobalTransformationMatrix);
        legacy_writer.Write<uint8_t>(uint8_t {0});
        legacy_writer.Write<uint32_t>(uint32_t {0});

        CHECK(codec_bytes == legacy_writer_bytes);
    }

    SECTION("Rejects invalid structure before writing")
    {
        ModelMeshData data = MakeModelMeshRoundTripData();
        data.RootBone->AttachedMesh->Indices.front() = ModelMeshIndexData {1};
        vector<uint8_t> bytes;
        DataWriter writer {bytes};
        CHECK_THROWS_AS(WriteModelMeshData(writer, data, "Models/BadIndex.fbx"), ModelMeshDataException);
        CHECK(bytes.empty());
    }

    SECTION("Rejects trailing payload data")
    {
        ModelMeshData source = MakeModelMeshRoundTripData();
        vector<uint8_t> bytes;
        DataWriter writer {bytes};
        WriteModelMeshData(writer, source, "Models/Trailing.fbx");
        writer.Write<uint8_t>(uint8_t {0});

        DataReader reader {{bytes.data(), bytes.size()}};
        CHECK_THROWS_AS(ReadModelMeshData(reader, "Models/Trailing.fbx"), ModelMeshDataException);
    }

    SECTION("Rejects mismatched skin palette data")
    {
        ModelMeshData data = MakeModelMeshRoundTripData();
        data.RootBone->AttachedMesh->SkinBoneOffsets.clear();
        CHECK_THROWS_AS(ValidateModelMeshData(data, "Models/BadSkinPalette.fbx"), ModelMeshDataException);
    }

    SECTION("Rejects skin bones outside the hierarchy")
    {
        ModelMeshData data = MakeModelMeshRoundTripData();
        data.RootBone->AttachedMesh->SkinBoneNames.front() = "Missing";
        CHECK_THROWS_AS(ValidateModelMeshData(data, "Models/MissingSkinBone.fbx"), ModelMeshDataException);
    }
}

FO_END_NAMESPACE

#endif
