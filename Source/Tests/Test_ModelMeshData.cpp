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
#include "ModelMeshData.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

static constexpr uint32_t MODEL_MESH_TEST_PAYLOAD = 0x78563412U;

static auto MakeModelMeshRoundTripData() -> ModelMeshData
{
    FO_STACK_TRACE_ENTRY();

    ModelMeshData data {.RootBone = safe_alloc::make_unique<ModelMeshBoneData>()};
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

    auto child = safe_alloc::make_unique<ModelMeshBoneData>();
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
    data_writer writer {data};
    writer.write_bytes({magic.data(), magic.size()});
    writer.write<uint16_t>(schema);
    writer.write<uint16_t>(flags);
    writer.write<uint32_t>(MODEL_MESH_TEST_PAYLOAD);
    return data;
}

TEST_CASE("ModelMeshDataWireHeader")
{
    SECTION("Round-trips and leaves the reader at the mesh payload")
    {
        vector<uint8_t> data;
        data_writer writer {data};
        WriteModelMeshHeader(writer);
        writer.write<uint32_t>(MODEL_MESH_TEST_PAYLOAD);

        REQUIRE(data.size() == MODEL_MESH_HEADER_SIZE + sizeof(uint32_t));
        CHECK(std::equal(MODEL_MESH_MAGIC.begin(), MODEL_MESH_MAGIC.end(), data.begin()));

        data_reader reader {{data.data(), data.size()}};
        REQUIRE_NOTHROW(ReadModelMeshHeader(reader, "Models/Test.fbx"));
        CHECK(reader.read<uint32_t>() == MODEL_MESH_TEST_PAYLOAD);
        CHECK_NOTHROW(reader.verify_end());
    }

    SECTION("Rejects wrong magic")
    {
        array<uint8_t, 8> magic = MODEL_MESH_MAGIC;
        magic.front() = uint8_t {'X'};
        vector<uint8_t> data = MakeModelMeshTestData(magic, MODEL_MESH_SCHEMA_VERSION, MODEL_MESH_SUPPORTED_FLAGS);
        data_reader reader {{data.data(), data.size()}};
        CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/WrongMagic.fbx"), ModelMeshDataException);
        CHECK(reader.read<uint8_t>() == magic.front());
    }

    SECTION("Rejects unsupported schema")
    {
        vector<uint8_t> data = MakeModelMeshTestData(MODEL_MESH_MAGIC, uint16_t {MODEL_MESH_SCHEMA_VERSION + 1}, MODEL_MESH_SUPPORTED_FLAGS);
        data_reader reader {{data.data(), data.size()}};
        CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/WrongSchema.fbx"), ModelMeshDataException);
    }

    SECTION("Rejects unsupported flags")
    {
        vector<uint8_t> data = MakeModelMeshTestData(MODEL_MESH_MAGIC, MODEL_MESH_SCHEMA_VERSION, uint16_t {1});
        data_reader reader {{data.data(), data.size()}};
        CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/WrongFlags.fbx"), ModelMeshDataException);
    }

    SECTION("Rejects every truncated header length")
    {
        vector<uint8_t> complete_data = MakeModelMeshTestData(MODEL_MESH_MAGIC, MODEL_MESH_SCHEMA_VERSION, MODEL_MESH_SUPPORTED_FLAGS);

        for (size_t size = 0; size < MODEL_MESH_HEADER_SIZE; size++) {
            data_reader reader {{complete_data.data(), size}};
            CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/Truncated.fbx"), ModelMeshDataException);
        }
    }

    SECTION("Rejects the removed headerless mesh layout")
    {
        vector<uint8_t> legacy_data;
        data_writer writer {legacy_data};
        writer.write_string("Root");
        writer.write<mat44>(mat44 {1.0f});

        data_reader reader {{legacy_data.data(), legacy_data.size()}};
        CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/Legacy.fbx"), ModelMeshDataException);
    }
}

TEST_CASE("ModelMeshDataWirePayload")
{
    SECTION("Round-trips the complete mesh hierarchy")
    {
        ModelMeshData source = MakeModelMeshRoundTripData();
        vector<uint8_t> bytes;
        data_writer writer {bytes};
        REQUIRE_NOTHROW(WriteModelMeshData(writer, source, "Models/Test.fbx"));

        data_reader reader {{bytes.data(), bytes.size()}};
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
        CHECK(reader.get_unread_size() == 0);
    }

    SECTION("Preserves the schema-1 byte layout")
    {
        ModelMeshData source = MakeModelMeshRoundTripData();
        vector<uint8_t> codec_bytes;
        data_writer codec_writer {codec_bytes};
        WriteModelMeshData(codec_writer, source, "Models/Test.fbx");

        vector<uint8_t> legacy_writer_bytes;
        data_writer legacy_writer {legacy_writer_bytes};
        WriteModelMeshHeader(legacy_writer);
        legacy_writer.write_string(source.RootBone->Name);
        legacy_writer.write<mat44>(source.RootBone->TransformationMatrix);
        legacy_writer.write<mat44>(source.RootBone->GlobalTransformationMatrix);
        legacy_writer.write<uint8_t>(uint8_t {1});
        const ModelMeshGeometryData& mesh = *source.RootBone->AttachedMesh;
        legacy_writer.write<uint32_t>(numeric_cast<uint32_t>(mesh.Vertices.size()));
        legacy_writer.write_object_vector(mesh.Vertices);
        legacy_writer.write<uint32_t>(numeric_cast<uint32_t>(mesh.Indices.size()));
        legacy_writer.write_object_vector(mesh.Indices);
        legacy_writer.write_string(mesh.DiffuseTexture);
        legacy_writer.write<uint32_t>(numeric_cast<uint32_t>(mesh.SkinBoneNames.size()));

        for (const string& skin_bone : mesh.SkinBoneNames) {
            legacy_writer.write_string(skin_bone);
        }

        legacy_writer.write<uint32_t>(numeric_cast<uint32_t>(mesh.SkinBoneOffsets.size()));
        legacy_writer.write_object_vector(mesh.SkinBoneOffsets);
        legacy_writer.write<uint32_t>(uint32_t {1});
        legacy_writer.write_string(source.RootBone->Children.front()->Name);
        legacy_writer.write<mat44>(source.RootBone->Children.front()->TransformationMatrix);
        legacy_writer.write<mat44>(source.RootBone->Children.front()->GlobalTransformationMatrix);
        legacy_writer.write<uint8_t>(uint8_t {0});
        legacy_writer.write<uint32_t>(uint32_t {0});

        CHECK(codec_bytes == legacy_writer_bytes);
    }

    SECTION("Rejects invalid structure before writing")
    {
        ModelMeshData data = MakeModelMeshRoundTripData();
        data.RootBone->AttachedMesh->Indices.front() = ModelMeshIndexData {1};
        vector<uint8_t> bytes;
        data_writer writer {bytes};
        CHECK_THROWS_AS(WriteModelMeshData(writer, data, "Models/BadIndex.fbx"), ModelMeshDataException);
        CHECK(bytes.empty());
    }

    SECTION("Rejects trailing payload data")
    {
        ModelMeshData source = MakeModelMeshRoundTripData();
        vector<uint8_t> bytes;
        data_writer writer {bytes};
        WriteModelMeshData(writer, source, "Models/Trailing.fbx");
        writer.write<uint8_t>(uint8_t {0});

        data_reader reader {{bytes.data(), bytes.size()}};
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
