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

#include "ModelMeshData.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

static constexpr size_t MODEL_MESH_STRING_MIN_SIZE = sizeof(uint32_t);
static constexpr size_t MODEL_MESH_BONE_MIN_SIZE = MODEL_MESH_STRING_MIN_SIZE + 2 * sizeof(mat44) + sizeof(uint8_t) + sizeof(uint32_t);

static void ValidateModelMeshBone(const ModelMeshBoneData& bone, string_view context, uint32_t depth, uint32_t& joint_count, unordered_set<string>& bone_names, vector<pair<string, string>>& skin_bone_refs);
static void ValidateModelMeshGeometry(const ModelMeshGeometryData& mesh, string_view owner_bone, string_view context);
static void WriteModelMeshBone(DataWriter& writer, const ModelMeshBoneData& bone);
static void WriteModelMeshGeometry(DataWriter& writer, const ModelMeshGeometryData& mesh);
static auto ReadModelMeshBone(DataReader& reader, string_view context, uint32_t depth, uint32_t& joint_count) -> unique_ptr<ModelMeshBoneData>;
static auto ReadModelMeshGeometry(DataReader& reader, string_view owner_bone, string_view context) -> ModelMeshGeometryData;
static void VerifyModelMeshCountFitsData(const DataReader& reader, size_t count, size_t min_element_size, string_view field, string_view context, string_view owner_bone);

void WriteModelMeshHeader(DataWriter& writer)
{
    FO_STACK_TRACE_ENTRY();

    writer.WriteBytes({MODEL_MESH_MAGIC.data(), MODEL_MESH_MAGIC.size()});
    writer.Write<uint16_t>(MODEL_MESH_SCHEMA_VERSION);
    writer.Write<uint16_t>(MODEL_MESH_SUPPORTED_FLAGS);
}

void ReadModelMeshHeader(DataReader& reader, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    DataReader source_reader = reader;
    const_span<uint8_t> header_data;

    try {
        header_data = source_reader.ReadBytes(MODEL_MESH_HEADER_SIZE);
    }
    catch (const std::exception& ex) {
        throw ModelMeshDataException("Truncated baked model mesh header", context, ex.what());
    }

    DataReader header_reader {header_data};
    const_span<uint8_t> magic = header_reader.ReadBytes(MODEL_MESH_MAGIC.size());

    if (!std::equal(magic.begin(), magic.end(), MODEL_MESH_MAGIC.begin())) {
        throw ModelMeshDataException("Invalid baked model mesh magic; expected 'LFMODMSH'", context);
    }

    uint16_t schema = header_reader.Read<uint16_t>();

    if (schema != MODEL_MESH_SCHEMA_VERSION) {
        throw ModelMeshDataException("Unsupported baked model mesh schema", context, MODEL_MESH_SCHEMA_VERSION, schema);
    }

    uint16_t flags = header_reader.Read<uint16_t>();

    if ((flags & ~MODEL_MESH_SUPPORTED_FLAGS) != 0) {
        throw ModelMeshDataException("Baked model mesh contains unsupported flags", context, flags);
    }

    header_reader.VerifyEnd();
    (void)reader.ReadBytes(MODEL_MESH_HEADER_SIZE);
}

void ValidateModelMeshData(const ModelMeshData& data, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (!data.RootBone) {
        throw ModelMeshDataException("Baked model mesh has no root bone", context);
    }

    uint32_t joint_count = 0;
    unordered_set<string> bone_names;
    vector<pair<string, string>> skin_bone_refs;
    ValidateModelMeshBone(*data.RootBone, context, 0, joint_count, bone_names, skin_bone_refs);

    for (const auto& [owner_bone, skin_bone] : skin_bone_refs) {
        if (!skin_bone.empty() && bone_names.count(skin_bone) == 0) {
            throw ModelMeshDataException("Baked model mesh references a skin bone outside its hierarchy", context, owner_bone, skin_bone);
        }
    }
}

void WriteModelMeshData(DataWriter& writer, const ModelMeshData& data, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelMeshData(data, context);
    WriteModelMeshHeader(writer);
    WriteModelMeshBone(writer, *data.RootBone);
}

auto ReadModelMeshData(DataReader& reader, string_view context) -> ModelMeshData
{
    FO_STACK_TRACE_ENTRY();

    DataReader source_reader = reader;
    ModelMeshData data;

    try {
        ReadModelMeshHeader(source_reader, context);
        uint32_t joint_count = 0;
        data.RootBone = ReadModelMeshBone(source_reader, context, 0, joint_count);
        source_reader.VerifyEnd();
        ValidateModelMeshData(data, context);
    }
    catch (const ModelMeshDataException&) {
        throw;
    }
    catch (const std::exception& ex) {
        throw ModelMeshDataException("Invalid baked model mesh payload", context, ex.what());
    }

    reader = source_reader;
    return data;
}

static void ValidateModelMeshBone(const ModelMeshBoneData& bone, string_view context, uint32_t depth, uint32_t& joint_count, unordered_set<string>& bone_names, vector<pair<string, string>>& skin_bone_refs)
{
    FO_STACK_TRACE_ENTRY();

    if (depth >= MODEL_MESH_MAX_HIERARCHY_DEPTH) {
        throw ModelMeshDataException("Baked model mesh hierarchy depth exceeds the safe limit", context, bone.Name, MODEL_MESH_MAX_HIERARCHY_DEPTH);
    }
    if (joint_count >= MODEL_MESH_MAX_JOINTS) {
        throw ModelMeshDataException("Baked model mesh hierarchy exceeds the joint limit", context, bone.Name, MODEL_MESH_MAX_JOINTS);
    }
    if (bone.Name.size() > std::numeric_limits<uint32_t>::max()) {
        throw ModelMeshDataException("Baked model mesh bone name exceeds the wire length limit", context, bone.Name.size(), std::numeric_limits<uint32_t>::max());
    }
    if (bone.Children.size() > std::numeric_limits<uint32_t>::max()) {
        throw ModelMeshDataException("Baked model mesh child count exceeds the wire count limit", context, bone.Name, bone.Children.size(), std::numeric_limits<uint32_t>::max());
    }

    joint_count++;

    if (!bone.Name.empty()) {
        bone_names.emplace(bone.Name);
    }

    if (bone.AttachedMesh) {
        ValidateModelMeshGeometry(*bone.AttachedMesh, bone.Name, context);

        for (const string& skin_bone : bone.AttachedMesh->SkinBoneNames) {
            skin_bone_refs.emplace_back(bone.Name, skin_bone);
        }
    }

    for (const auto& child : bone.Children) {
        ValidateModelMeshBone(*child, context, depth + 1, joint_count, bone_names, skin_bone_refs);
    }
}

static void ValidateModelMeshGeometry(const ModelMeshGeometryData& mesh, string_view owner_bone, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    constexpr uint64_t max_vertex_count = uint64_t {std::numeric_limits<ModelMeshIndexData>::max()} + uint64_t {1};

    if (mesh.Vertices.size() > max_vertex_count) {
        throw ModelMeshDataException("Baked model mesh bone vertex count exceeds maximum addressable count", context, owner_bone, mesh.Vertices.size(), max_vertex_count);
    }
    if (mesh.Vertices.size() > std::numeric_limits<uint32_t>::max() || mesh.Indices.size() > std::numeric_limits<uint32_t>::max()) {
        throw ModelMeshDataException("Baked model mesh geometry count exceeds the wire count limit", context, owner_bone, mesh.Vertices.size(), mesh.Indices.size(), std::numeric_limits<uint32_t>::max());
    }
    if (mesh.DiffuseTexture.size() > std::numeric_limits<uint32_t>::max()) {
        throw ModelMeshDataException("Baked model mesh texture name exceeds the wire length limit", context, owner_bone, mesh.DiffuseTexture.size(), std::numeric_limits<uint32_t>::max());
    }
    if (mesh.SkinBoneNames.size() > MODEL_MESH_MAX_SKIN_BONES) {
        throw ModelMeshDataException("Baked model mesh bone skin bone count exceeds maximum", context, owner_bone, mesh.SkinBoneNames.size(), MODEL_MESH_MAX_SKIN_BONES);
    }
    if (mesh.SkinBoneNames.size() != mesh.SkinBoneOffsets.size()) {
        throw ModelMeshDataException("Baked model mesh bone skin bone offset count mismatch", context, owner_bone, mesh.SkinBoneOffsets.size(), mesh.SkinBoneNames.size());
    }

    for (const string& skin_bone : mesh.SkinBoneNames) {
        if (skin_bone.size() > std::numeric_limits<uint32_t>::max()) {
            throw ModelMeshDataException("Baked model mesh skin bone name exceeds the wire length limit", context, owner_bone, skin_bone.size(), std::numeric_limits<uint32_t>::max());
        }
    }

    for (size_t index_position = 0; index_position < mesh.Indices.size(); index_position++) {
        if (numeric_cast<size_t>(mesh.Indices[index_position]) >= mesh.Vertices.size()) {
            throw ModelMeshDataException("Baked model mesh bone has vertex index outside vertex count", context, owner_bone, mesh.Indices[index_position], index_position, mesh.Vertices.size());
        }
    }

    for (size_t vertex_index = 0; vertex_index < mesh.Vertices.size(); vertex_index++) {
        const ModelMeshVertexData& vertex = mesh.Vertices[vertex_index];
        float32_t total_weight = 0.0f;

        for (size_t influence = 0; influence < MODEL_MESH_BONES_PER_VERTEX; influence++) {
            float32_t weight = vertex.BlendWeights[influence];
            float32_t index = vertex.BlendIndices[influence];

            if (!std::isfinite(weight)) {
                throw ModelMeshDataException("Baked model mesh bone has non-finite skin weight", context, owner_bone, vertex_index, influence);
            }
            if (weight < 0.0f || weight > 1.0f) {
                throw ModelMeshDataException("Baked model mesh bone has skin weight outside [0, 1]", context, owner_bone, weight, vertex_index, influence);
            }
            if (!std::isfinite(index)) {
                throw ModelMeshDataException("Baked model mesh bone has non-finite skin index", context, owner_bone, vertex_index, influence);
            }
            if (index != std::trunc(index)) {
                throw ModelMeshDataException("Baked model mesh bone has non-integral skin index", context, owner_bone, index, vertex_index, influence);
            }
            if (index < 0.0f || index >= numeric_cast<float32_t>(mesh.SkinBoneNames.size())) {
                throw ModelMeshDataException("Baked model mesh bone has skin index outside valid range", context, owner_bone, index, mesh.SkinBoneNames.size(), vertex_index, influence);
            }

            total_weight += weight;
        }

        if (!std::isfinite(total_weight) || !is_float_equal(total_weight, 1.0f, MODEL_MESH_SKIN_WEIGHT_SUM_TOLERANCE)) {
            throw ModelMeshDataException("Baked model mesh bone has skin-weight sum that is not 1", context, owner_bone, total_weight, vertex_index);
        }
    }
}

static void WriteModelMeshBone(DataWriter& writer, const ModelMeshBoneData& bone)
{
    FO_STACK_TRACE_ENTRY();

    writer.WriteString(bone.Name);
    writer.Write<mat44>(bone.TransformationMatrix);
    writer.Write<mat44>(bone.GlobalTransformationMatrix);
    writer.Write<uint8_t>(bone.AttachedMesh ? uint8_t {1} : uint8_t {0});

    if (bone.AttachedMesh) {
        WriteModelMeshGeometry(writer, *bone.AttachedMesh);
    }

    writer.Write<uint32_t>(numeric_cast<uint32_t>(bone.Children.size()));

    for (const auto& child : bone.Children) {
        WriteModelMeshBone(writer, *child);
    }
}

static void WriteModelMeshGeometry(DataWriter& writer, const ModelMeshGeometryData& mesh)
{
    FO_STACK_TRACE_ENTRY();

    writer.Write<uint32_t>(numeric_cast<uint32_t>(mesh.Vertices.size()));
    writer.WriteObjectVector(mesh.Vertices);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(mesh.Indices.size()));
    writer.WriteObjectVector(mesh.Indices);
    writer.WriteString(mesh.DiffuseTexture);
    writer.WriteStringVector(mesh.SkinBoneNames);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(mesh.SkinBoneOffsets.size()));
    writer.WriteObjectVector(mesh.SkinBoneOffsets);
}

static auto ReadModelMeshBone(DataReader& reader, string_view context, uint32_t depth, uint32_t& joint_count) -> unique_ptr<ModelMeshBoneData>
{
    FO_STACK_TRACE_ENTRY();

    if (depth >= MODEL_MESH_MAX_HIERARCHY_DEPTH) {
        throw ModelMeshDataException("Baked model mesh hierarchy depth exceeds the safe limit", context, MODEL_MESH_MAX_HIERARCHY_DEPTH);
    }
    if (joint_count >= MODEL_MESH_MAX_JOINTS) {
        throw ModelMeshDataException("Baked model mesh hierarchy exceeds the joint limit", context, MODEL_MESH_MAX_JOINTS);
    }

    joint_count++;
    auto bone = SafeAlloc::MakeUnique<ModelMeshBoneData>();
    bone->Name = reader.ReadString();
    bone->TransformationMatrix = reader.Read<mat44>();
    bone->GlobalTransformationMatrix = reader.Read<mat44>();
    uint8_t has_attached_mesh = reader.Read<uint8_t>();

    if (has_attached_mesh > 1) {
        throw ModelMeshDataException("Baked model mesh has an invalid attached-mesh flag", context, bone->Name, has_attached_mesh);
    }
    if (has_attached_mesh != 0) {
        bone->AttachedMesh.emplace(ReadModelMeshGeometry(reader, bone->Name, context));
    }

    uint32_t children_count = reader.Read<uint32_t>();

    if (children_count > MODEL_MESH_MAX_JOINTS - joint_count) {
        throw ModelMeshDataException("Baked model mesh bone child count exceeds maximum", context, bone->Name, children_count, MODEL_MESH_MAX_JOINTS - joint_count);
    }

    VerifyModelMeshCountFitsData(reader, children_count, MODEL_MESH_BONE_MIN_SIZE, "child bones", context, bone->Name);
    bone->Children.reserve(children_count);

    for (uint32_t i = 0; i < children_count; i++) {
        bone->Children.emplace_back(ReadModelMeshBone(reader, context, depth + 1, joint_count));
    }

    return bone;
}

static auto ReadModelMeshGeometry(DataReader& reader, string_view owner_bone, string_view context) -> ModelMeshGeometryData
{
    FO_STACK_TRACE_ENTRY();

    ModelMeshGeometryData mesh;
    uint32_t vertices_count = reader.Read<uint32_t>();
    constexpr uint64_t max_vertex_count = uint64_t {std::numeric_limits<ModelMeshIndexData>::max()} + uint64_t {1};

    if (vertices_count > max_vertex_count) {
        throw ModelMeshDataException("Baked model mesh bone vertex count exceeds maximum addressable count", context, owner_bone, vertices_count, max_vertex_count);
    }

    VerifyModelMeshCountFitsData(reader, vertices_count, sizeof(ModelMeshVertexData), "mesh vertices", context, owner_bone);
    mesh.Vertices.resize(vertices_count);
    reader.ReadObjectArray(span {mesh.Vertices});

    uint32_t indices_count = reader.Read<uint32_t>();
    VerifyModelMeshCountFitsData(reader, indices_count, sizeof(ModelMeshIndexData), "mesh indices", context, owner_bone);
    mesh.Indices.resize(indices_count);
    reader.ReadObjectArray(span {mesh.Indices});
    mesh.DiffuseTexture = reader.ReadString();

    uint32_t skin_bones_count = reader.Read<uint32_t>();

    if (skin_bones_count > MODEL_MESH_MAX_SKIN_BONES) {
        throw ModelMeshDataException("Baked model mesh bone skin bone count exceeds maximum", context, owner_bone, skin_bones_count, MODEL_MESH_MAX_SKIN_BONES);
    }

    VerifyModelMeshCountFitsData(reader, skin_bones_count, MODEL_MESH_STRING_MIN_SIZE, "skin bone names", context, owner_bone);
    mesh.SkinBoneNames.reserve(skin_bones_count);

    for (uint32_t i = 0; i < skin_bones_count; i++) {
        mesh.SkinBoneNames.emplace_back(reader.ReadString());
    }

    uint32_t skin_bone_offsets_count = reader.Read<uint32_t>();

    if (skin_bone_offsets_count != skin_bones_count) {
        throw ModelMeshDataException("Baked model mesh bone skin bone offset count mismatch", context, owner_bone, skin_bone_offsets_count, skin_bones_count);
    }

    VerifyModelMeshCountFitsData(reader, skin_bone_offsets_count, sizeof(mat44), "skin bone offsets", context, owner_bone);
    mesh.SkinBoneOffsets.resize(skin_bone_offsets_count);
    reader.ReadObjectArray(span {mesh.SkinBoneOffsets});
    return mesh;
}

static void VerifyModelMeshCountFitsData(const DataReader& reader, size_t count, size_t min_element_size, string_view field, string_view context, string_view owner_bone)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(min_element_size != 0, "Model mesh minimum serialized element size must be non-zero", field);

    if (count > reader.GetUnreadSize() / min_element_size) {
        throw ModelMeshDataException("Baked model mesh count exceeds the remaining payload", context, owner_bone, field, count, reader.GetUnreadSize(), min_element_size);
    }
}

FO_END_NAMESPACE

#endif
