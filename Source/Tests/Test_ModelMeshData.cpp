//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "Common.h"

#if FO_ENABLE_3D

#include "ModelMeshData.h"

FO_BEGIN_NAMESPACE

static constexpr uint32_t MODEL_MESH_TEST_PAYLOAD = 0x78563412U;

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
        const vector<uint8_t> data = MakeModelMeshTestData(magic, MODEL_MESH_SCHEMA_VERSION, MODEL_MESH_SUPPORTED_FLAGS);
        DataReader reader {{data.data(), data.size()}};
        CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/WrongMagic.fbx"), ModelMeshDataException);
        CHECK(reader.Read<uint8_t>() == magic.front());
    }

    SECTION("Rejects unsupported schema")
    {
        const vector<uint8_t> data = MakeModelMeshTestData(MODEL_MESH_MAGIC, uint16_t {MODEL_MESH_SCHEMA_VERSION + 1}, MODEL_MESH_SUPPORTED_FLAGS);
        DataReader reader {{data.data(), data.size()}};
        CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/WrongSchema.fbx"), ModelMeshDataException);
    }

    SECTION("Rejects unsupported flags")
    {
        const vector<uint8_t> data = MakeModelMeshTestData(MODEL_MESH_MAGIC, MODEL_MESH_SCHEMA_VERSION, uint16_t {1});
        DataReader reader {{data.data(), data.size()}};
        CHECK_THROWS_AS(ReadModelMeshHeader(reader, "Models/WrongFlags.fbx"), ModelMeshDataException);
    }

    SECTION("Rejects every truncated header length")
    {
        const vector<uint8_t> complete_data = MakeModelMeshTestData(MODEL_MESH_MAGIC, MODEL_MESH_SCHEMA_VERSION, MODEL_MESH_SUPPORTED_FLAGS);

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

FO_END_NAMESPACE

#endif
