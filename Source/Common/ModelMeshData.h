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

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ModelMeshDataException);

inline constexpr array<uint8_t, 8> MODEL_MESH_MAGIC {'L', 'F', 'M', 'O', 'D', 'M', 'S', 'H'};
inline constexpr uint16_t MODEL_MESH_SCHEMA_VERSION = 1;
inline constexpr uint16_t MODEL_MESH_SUPPORTED_FLAGS = 0;
inline constexpr size_t MODEL_MESH_HEADER_SIZE = MODEL_MESH_MAGIC.size() + sizeof(uint16_t) + sizeof(uint16_t);
inline constexpr uint32_t MODEL_MESH_MAX_HIERARCHY_DEPTH = 128;
inline constexpr uint32_t MODEL_MESH_MAX_JOINTS = 1024;
inline constexpr size_t MODEL_MESH_MAX_SKIN_BONES = FO_MODEL_MAX_BONES;
inline constexpr size_t MODEL_MESH_BONES_PER_VERTEX = FO_MODEL_BONES_PER_VERTEX;
inline constexpr float32_t MODEL_MESH_SKIN_WEIGHT_SUM_TOLERANCE = 1.0e-4f;

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

// Schema 1 wire order: header, then one recursive root bone with its optional drawable mesh.
void WriteModelMeshHeader(DataWriter& writer);
void ReadModelMeshHeader(DataReader& reader, string_view context);
void ValidateModelMeshData(const ModelMeshData& data, string_view context);
void WriteModelMeshData(DataWriter& writer, const ModelMeshData& data, string_view context);
auto ReadModelMeshData(DataReader& reader, string_view context) -> ModelMeshData;

FO_END_NAMESPACE

#endif
