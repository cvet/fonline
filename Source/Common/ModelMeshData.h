// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ModelMeshDataException);

inline constexpr array<uint8_t, 8> MODEL_MESH_MAGIC {'L', 'F', 'M', 'O', 'D', 'M', 'S', 'H'};
inline constexpr uint16_t MODEL_MESH_SCHEMA_VERSION {1};
inline constexpr uint16_t MODEL_MESH_SUPPORTED_FLAGS {0};
inline constexpr size_t MODEL_MESH_HEADER_SIZE {MODEL_MESH_MAGIC.size() + sizeof(uint16_t) + sizeof(uint16_t)};
inline constexpr uint32_t MODEL_MESH_MAX_HIERARCHY_DEPTH {128};
inline constexpr uint32_t MODEL_MESH_MAX_JOINTS {1024};
inline constexpr size_t MODEL_MESH_MAX_SKIN_BONES {FO_MODEL_MAX_BONES};
inline constexpr size_t MODEL_MESH_BONES_PER_VERTEX {FO_MODEL_BONES_PER_VERTEX};
inline constexpr float32_t MODEL_MESH_SKIN_WEIGHT_SUM_TOLERANCE {1.0e-4f};

using ModelMeshIndexData = std::conditional_t<FO_RENDER_32BIT_INDEX, uint32_t, uint16_t>;

struct ModelMeshVertexData
{
    vec3 Position {};
    vec3 Normal {};
    float32_t TexCoord[2] {};
    float32_t TexCoordBase[2] {};
    vec3 Tangent {};
    vec3 Bitangent {};
    float32_t BlendWeights[MODEL_MESH_BONES_PER_VERTEX] {};
    float32_t BlendIndices[MODEL_MESH_BONES_PER_VERTEX] {};
    ucolor Color {};
};
static_assert(std::is_standard_layout_v<ModelMeshVertexData>);
static_assert(std::is_trivially_copyable_v<ModelMeshVertexData>);
static_assert(sizeof(ModelMeshVertexData) == 68 + 8 * MODEL_MESH_BONES_PER_VERTEX);

struct ModelMeshGeometryData
{
    vector<ModelMeshVertexData> Vertices {};
    vector<ModelMeshIndexData> Indices {};
    string DiffuseTexture {};
    vector<string> SkinBoneNames {};
    vector<mat44> SkinBoneOffsets {};
};

struct ModelMeshBoneData
{
    string Name {};
    mat44 TransformationMatrix {};
    mat44 GlobalTransformationMatrix {};
    optional<ModelMeshGeometryData> AttachedMesh {};
    vector<unique_ptr<ModelMeshBoneData>> Children {};
};

struct ModelMeshData
{
    unique_nptr<ModelMeshBoneData> RootBone {};
};

// Schema 1 wire order: header, then one recursive root bone with its optional drawable mesh
void WriteModelMeshHeader(data_writer& writer);
void ReadModelMeshHeader(data_reader& reader, string_view context);
void ValidateModelMeshData(const ModelMeshData& data, string_view context);
void WriteModelMeshData(data_writer& writer, const ModelMeshData& data, string_view context);
auto ReadModelMeshData(data_reader& reader, string_view context) -> ModelMeshData;

FO_END_NAMESPACE

#endif
