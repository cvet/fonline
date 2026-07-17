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
//

#include "ModelMeshBaker.h"

#if FO_ENABLE_3D

#include "Application.h"
#include "ModelAnimationData.h"
#include "ModelMeshData.h"

#include "ufbx.h"

extern "C" void* ufbx_malloc(size_t size)
{
    FO_USING_NAMESPACE();
    constexpr SafeAllocator<uint8_t> allocator;
    ptr<uint8_t> bytes = allocator.allocate(size);
    return bytes.get();
}

extern "C" void* ufbx_realloc(void* memory, size_t old_size, size_t new_size)
{
    FO_USING_NAMESPACE();
    constexpr SafeAllocator<uint8_t> allocator;
    ptr<uint8_t> new_ptr = allocator.allocate(new_size);
    auto old_data = make_nptr(memory).reinterpret_as<uint8_t>();

    if (const size_t copy_size = std::min(old_size, new_size); copy_size != 0) {
        FO_STRONG_ASSERT(old_data, "Reallocation requested a copy but the previous block pointer is null");
        MemCopy(new_ptr, old_data, copy_size);
    }

    allocator.deallocate(old_data.get(), old_size);
    return new_ptr.get();
}

extern "C" void ufbx_free(void* ptr, size_t old_size)
{
    FO_USING_NAMESPACE();
    constexpr SafeAllocator<uint8_t> allocator;
    allocator.deallocate(make_nptr(ptr).reinterpret_as<uint8_t>().get(), old_size);
}

FO_BEGIN_NAMESPACE

struct BakerMeshData
{
    void Save(DataWriter& writer) const
    {
        FO_STACK_TRACE_ENTRY();

        FO_VERIFY_AND_THROW(SkinBones.size() == SkinBoneOffsets.size(), "Skin bone list and inverse-bind offset list have different sizes", SkinBones.size(), SkinBoneOffsets.size());

        auto len = numeric_cast<uint32_t>(Vertices.size());
        writer.Write<uint32_t>(len);
        writer.WriteObjectVector(Vertices);
        len = numeric_cast<uint32_t>(Indices.size());
        writer.Write<uint32_t>(len);
        writer.WriteObjectVector(Indices);
        len = numeric_cast<uint32_t>(DiffuseTexture.length());
        writer.Write<uint32_t>(len);
        writer.WriteStringBytes(DiffuseTexture);
        len = numeric_cast<uint32_t>(SkinBones.size());
        writer.Write<uint32_t>(len);
        for (const auto& bone_name : SkinBones) {
            len = numeric_cast<uint32_t>(bone_name.length());
            writer.Write<uint32_t>(len);
            writer.WriteStringBytes(bone_name);
        }
        len = numeric_cast<uint32_t>(SkinBoneOffsets.size());
        writer.Write<uint32_t>(len);
        writer.WriteObjectVector(SkinBoneOffsets);
    }

    vector<Vertex3D> Vertices {};
    vector<vindex_t> Indices {};
    string DiffuseTexture {};
    vector<string> SkinBones {};
    vector<mat44> SkinBoneOffsets {};
};

struct BakerBone
{
    auto Find(const string& name) -> nptr<BakerBone>
    {
        FO_STACK_TRACE_ENTRY();

        if (Name == name) {
            return this;
        }

        for (size_t i = 0; i != Children.size(); ++i) {
            if (auto bone = Children[i]->Find(name)) {
                return bone;
            }
        }

        return nullptr;
    }

    void Save(DataWriter& writer) const
    {
        FO_STACK_TRACE_ENTRY();

        writer.Write<uint32_t>(numeric_cast<uint32_t>(Name.length()));
        writer.WriteStringBytes(Name);
        writer.Write<mat44>(TransformationMatrix);
        writer.Write<mat44>(GlobalTransformationMatrix);

        if (AttachedMesh) {
            writer.Write<uint8_t>(const_numeric_cast<uint8_t>(1));
            AttachedMesh->Save(writer);
        }
        else {
            writer.Write<uint8_t>(const_numeric_cast<uint8_t>(0));
        }

        writer.Write<uint32_t>(numeric_cast<uint32_t>(Children.size()));

        for (const auto& child : Children) {
            child->Save(writer);
        }
    }

    string Name {};
    mat44 TransformationMatrix {};
    mat44 GlobalTransformationMatrix {};
    optional<BakerMeshData> AttachedMesh {};
    vector<unique_ptr<BakerBone>> Children {};
    mat44 CombinedTransformationMatrix {};
};

struct FbxValidationContext
{
    string_view FileName {};
    string_view ScopeName {};
    string_view NodeName {};
    string_view FieldName {};
    size_t ElementIndex {};
};

static constexpr float32_t FBX_SKIN_WEIGHT_SUM_TOLERANCE = 1.0e-4f;

ModelMeshBaker::ModelMeshBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx))
{
    FO_STACK_TRACE_ENTRY();
}

ModelMeshBaker::~ModelMeshBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void ModelMeshBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    // Collect files
    vector<File> filtered_files;

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            const string ext = strex(file_header.GetPath()).get_file_extension();

            if (ext != "fbx" && ext != "obj") {
                continue;
            }
            if (_context->BakeChecker && !_context->BakeChecker(file_header.GetPath(), file_header.GetWriteTime())) {
                continue;
            }

            filtered_files.emplace_back(File::Load(file_header));
        }
    }
    else {
        const string ext = strex(target_path).get_file_extension();

        if (ext != "fbx" && ext != "obj") {
            return;
        }

        auto file = files.FindFileByPath(target_path);

        if (!file) {
            return;
        }
        if (_context->BakeChecker && !_context->BakeChecker(file.GetPath(), file.GetWriteTime())) {
            return;
        }

        filtered_files.emplace_back(std::move(file));
    }

    if (filtered_files.empty()) {
        return;
    }

    // Process files
    vector<std::future<void>> file_bakings;

    for (auto& file_ : filtered_files) {
        const auto task_name = strex("BakeModelMesh-{}", file_.GetPath()).str();
        file_bakings.emplace_back(run_async(GetAsyncMode(), task_name, [this, file = std::move(file_)]() FO_DEFERRED {
            auto data = BakeFbxFile(file.GetPath(), file);
            _context->WriteData(file.GetPath(), data);
        }));
    }

    size_t errors = 0;

    for (auto& file_baking : file_bakings) {
        try {
            file_baking.get();
        }
        catch (const std::exception& ex) {
            WriteLog("Model mesh baking error: {}", ex.what());
            errors++;
        }
    }

    if (errors != 0) {
        throw ModelMeshBakerException("Errors during model mesh baking");
    }
}

static auto ConvertFbxHierarchy(ptr<const ufbx_node> fbx_node, string_view fname, uint32_t depth) -> unique_ptr<BakerBone>;
static void ConvertFbxMeshes(ptr<BakerBone> root_bone, ptr<BakerBone> bone, ptr<const ufbx_node> fbx_node, string_view fname);
static auto ConvertFbxFloat(double value, const FbxValidationContext& context, string_view component) -> float32_t;
static auto ConvertFbxVec3(const ufbx_vec3& value, const FbxValidationContext& context) -> vec3;
static auto ConvertFbxColorComponent(double value, const FbxValidationContext& context, string_view component) -> uint8_t;
static auto ConvertFbxColor(const ufbx_vec4& value, const FbxValidationContext& context) -> ucolor;
static auto ConvertFbxMatrix(const ufbx_matrix& value, const FbxValidationContext& context) -> mat44;
static void ValidateFbxVertex(const Vertex3D& vertex, size_t skin_bone_count, const FbxValidationContext& context);

auto ModelMeshBaker::BakeFbxFile(string_view fname, const File& file) const -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    ufbx_load_opts opts = {};
    opts.ignore_embedded = true;
    opts.evaluate_skinning = true;
    opts.ignore_missing_external_files = true;
    opts.clean_skin_weights = true;
    opts.generate_missing_normals = true;
    opts.normalize_normals = true;
    opts.normalize_tangents = true;

    ufbx_error fbx_error;
    const_span<uint8_t> file_data = file.GetDataSpan();
    auto file_data_bytes = make_nptr(file_data.data());
    FO_VERIFY_AND_THROW(file_data.empty() || file_data_bytes, "Non-empty FBX file data has a null buffer pointer");
    auto fbx_scene = make_nptr(ufbx_load_memory(file_data_bytes.get(), file_data.size(), &opts, &fbx_error));

    if (!fbx_scene) {
        throw ModelMeshBakerException("Unable to load FBX", fname, fbx_error.description.data);
    }

    auto fbx_scene_holder = make_unique_del_ptr(fbx_scene, [](ufbx_scene* raw_scene) noexcept { ufbx_free_scene(raw_scene); });

    if (fbx_scene->nodes.count > MODEL_ANIMATION_RIG_MAX_JOINTS) {
        throw ModelMeshBakerException(strex("FBX '{}' hierarchy has {} joints; maximum is {}", fname, fbx_scene->nodes.count, MODEL_ANIMATION_RIG_MAX_JOINTS));
    }

    // Convert data
    auto root_bone = ConvertFbxHierarchy(fbx_scene->root_node, fname, 0);
    ConvertFbxMeshes(root_bone, root_bone, fbx_scene->root_node, fname);

    // Write data
    vector<uint8_t> data;
    auto writer = DataWriter(data);

    WriteModelMeshHeader(writer);
    root_bone->Save(writer);

    return data;
}

static auto ConvertFbxHierarchy(ptr<const ufbx_node> fbx_node, string_view fname, uint32_t depth) -> unique_ptr<BakerBone>
{
    FO_STACK_TRACE_ENTRY();

    if (depth >= MODEL_MESH_MAX_HIERARCHY_DEPTH) {
        throw ModelMeshBakerException(strex("FBX '{}' hierarchy exceeds the safe depth limit {} at node '{}'", fname, MODEL_MESH_MAX_HIERARCHY_DEPTH, fbx_node->name.data));
    }

    auto bone = SafeAlloc::MakeUnique<BakerBone>();

    bone->Name = fbx_node->name.data;
    bone->TransformationMatrix = ConvertFbxMatrix(fbx_node->node_to_parent, FbxValidationContext {.FileName = fname, .ScopeName = "hierarchy", .NodeName = bone->Name, .FieldName = "node_to_parent"});
    bone->GlobalTransformationMatrix = ConvertFbxMatrix(fbx_node->node_to_world, FbxValidationContext {.FileName = fname, .ScopeName = "hierarchy", .NodeName = bone->Name, .FieldName = "node_to_world"});
    bone->CombinedTransformationMatrix = mat44();
    bone->Children.reserve(fbx_node->children.count);

    for (size_t i = 0; i < fbx_node->children.count; i++) {
        bone->Children.emplace_back(ConvertFbxHierarchy(fbx_node->children[i], fname, depth + 1));
    }

    return bone;
}

static void ConvertFbxMeshes(ptr<BakerBone> root_bone, ptr<BakerBone> bone, ptr<const ufbx_node> fbx_node, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const ufbx_mesh> fbx_mesh = fbx_node->mesh;

    if (fbx_mesh && fbx_mesh->num_faces != 0) {
        bone->AttachedMesh.emplace();
        auto mesh = make_ptr(&*bone->AttachedMesh);
        FO_VERIFY_AND_THROW(fbx_mesh->num_faces == fbx_mesh->num_triangles, "FBX mesh contains non-triangle faces", fbx_mesh->num_faces, fbx_mesh->num_triangles);
        nptr<const ufbx_skin_deformer> fbx_skin = fbx_mesh->skin_deformers.count != 0 ? fbx_mesh->skin_deformers[0] : nullptr;

        if (fbx_skin && fbx_skin->clusters.count == 0) {
            throw ModelMeshBakerException(strex("FBX '{}' mesh node '{}' has a skin deformer without bone clusters", fname, bone->Name));
        }
        if (fbx_skin && fbx_skin->clusters.count > MODEL_MAX_BONES) {
            throw ModelMeshBakerException(strex("Mesh '{}' has {} skin clusters, exceeds MODEL_MAX_BONES limit {}", fbx_node->name.data, fbx_skin->clusters.count, MODEL_MAX_BONES));
        }

        mesh->Vertices.reserve(fbx_mesh->num_indices);

        for (const ufbx_mesh_part& fbx_mesh_part : fbx_mesh->material_parts) {
            vector<uint32_t> triangle_indices;
            triangle_indices.resize(fbx_mesh->max_face_triangles * 3);
            uint32_t mesh_triangles_count = 0;

            for (const uint32_t& face_index : fbx_mesh_part.face_indices) {
                const ufbx_face fbx_face = fbx_mesh->faces[face_index];
                FO_VERIFY_AND_THROW(!triangle_indices.empty(), "Triangulation buffer is empty");
                auto triangle_indices_data = make_nptr(triangle_indices.data());
                const uint32_t triangles_count = ufbx_triangulate_face(triangle_indices_data.get(), triangle_indices.size(), fbx_mesh.get(), fbx_face);

                mesh_triangles_count += triangles_count;

                for (size_t i = 0; i < numeric_cast<size_t>(triangles_count) * 3; i++) {
                    const uint32_t index = triangle_indices[i];
                    auto& v = mesh->Vertices.emplace_back();

                    v.Position = ConvertFbxVec3(fbx_mesh->vertex_position[index], FbxValidationContext {.FileName = fname, .ScopeName = "geometry", .NodeName = bone->Name, .FieldName = "position", .ElementIndex = index});

                    if (fbx_mesh->vertex_normal.exists) {
                        v.Normal = ConvertFbxVec3(fbx_mesh->vertex_normal[index], FbxValidationContext {.FileName = fname, .ScopeName = "geometry", .NodeName = bone->Name, .FieldName = "normal", .ElementIndex = index});
                    }

                    if (fbx_mesh->vertex_tangent.exists) {
                        v.Tangent = ConvertFbxVec3(fbx_mesh->vertex_tangent[index], FbxValidationContext {.FileName = fname, .ScopeName = "geometry", .NodeName = bone->Name, .FieldName = "tangent", .ElementIndex = index});
                    }

                    if (fbx_mesh->vertex_bitangent.exists) {
                        v.Bitangent = ConvertFbxVec3(fbx_mesh->vertex_bitangent[index], FbxValidationContext {.FileName = fname, .ScopeName = "geometry", .NodeName = bone->Name, .FieldName = "bitangent", .ElementIndex = index});
                    }

                    if (fbx_mesh->vertex_color.exists) {
                        v.Color = ConvertFbxColor(fbx_mesh->vertex_color[index], FbxValidationContext {.FileName = fname, .ScopeName = "geometry", .NodeName = bone->Name, .FieldName = "color", .ElementIndex = index});
                    }

                    if (fbx_mesh->vertex_uv.exists) {
                        v.TexCoord[0] = ConvertFbxFloat(fbx_mesh->vertex_uv[index].x, FbxValidationContext {.FileName = fname, .ScopeName = "geometry", .NodeName = bone->Name, .FieldName = "uv", .ElementIndex = index}, "x");
                        v.TexCoord[1] = 1.0f - ConvertFbxFloat(fbx_mesh->vertex_uv[index].y, FbxValidationContext {.FileName = fname, .ScopeName = "geometry", .NodeName = bone->Name, .FieldName = "uv", .ElementIndex = index}, "y");
                        v.TexCoordBase[0] = v.TexCoord[0];
                        v.TexCoordBase[1] = v.TexCoord[1];
                    }

                    if (fbx_skin) {
                        const uint32_t v_index = fbx_mesh->vertex_indices[index];
                        FO_VERIFY_AND_THROW(v_index < fbx_skin->vertices.count, "FBX skin vertex index is outside the skin vertex table", fname, bone->Name, v_index, fbx_skin->vertices.count);
                        const ufbx_skin_vertex& fbx_skin_vertex = fbx_skin->vertices[v_index];
                        FO_VERIFY_AND_THROW(fbx_skin_vertex.weight_begin <= fbx_skin->weights.count && fbx_skin_vertex.num_weights <= fbx_skin->weights.count - fbx_skin_vertex.weight_begin, "FBX skin weight range is outside the skin weight table", fname, bone->Name, v_index, fbx_skin_vertex.weight_begin, fbx_skin_vertex.num_weights, fbx_skin->weights.count);
                        const size_t weights_count = std::min(numeric_cast<size_t>(fbx_skin_vertex.num_weights), MODEL_BONES_PER_VERTEX);

                        if (weights_count == 0) {
                            throw ModelMeshBakerException(strex("FBX '{}' mesh node '{}' has no retained skin influences at vertex {}", fname, bone->Name, v_index));
                        }

                        float32_t total_weight = 0.0f;

                        for (size_t w = 0; w < weights_count; w++) {
                            const ufbx_skin_weight skin_weight = fbx_skin->weights[fbx_skin_vertex.weight_begin + w];

                            if (skin_weight.cluster_index >= fbx_skin->clusters.count) {
                                throw ModelMeshBakerException(strex("FBX '{}' mesh node '{}' has skin cluster index {} outside [0, {}) at vertex {}, influence {}", fname, bone->Name, skin_weight.cluster_index, fbx_skin->clusters.count, v_index, w));
                            }

                            v.BlendIndices[w] = numeric_cast<float32_t>(skin_weight.cluster_index);
                            v.BlendWeights[w] = ConvertFbxFloat(skin_weight.weight, FbxValidationContext {.FileName = fname, .ScopeName = "geometry", .NodeName = bone->Name, .FieldName = "skin_weight", .ElementIndex = fbx_skin_vertex.weight_begin + w}, "weight");

                            if (v.BlendWeights[w] < 0.0f) {
                                throw ModelMeshBakerException(strex("FBX '{}' mesh node '{}' has negative retained skin weight {} at vertex {}, influence {}", fname, bone->Name, v.BlendWeights[w], v_index, w));
                            }

                            total_weight += v.BlendWeights[w];

                            if (!std::isfinite(total_weight)) {
                                throw ModelMeshBakerException(strex("FBX '{}' has a non-finite accumulated skin weight for node '{}' at vertex {}", fname, bone->Name, v_index));
                            }
                        }

                        if (!(total_weight > 0.0f)) {
                            throw ModelMeshBakerException(strex("FBX '{}' mesh node '{}' has zero or negative retained skin-weight total {} at vertex {}", fname, bone->Name, total_weight, v_index));
                        }

                        for (size_t w = 0; w < weights_count; w++) {
                            v.BlendWeights[w] /= total_weight;
                        }
                    }
                    else {
                        v.BlendIndices[0] = 0.0f;
                        v.BlendWeights[0] = 1.0f;
                    }
                }
            }

            FO_VERIFY_AND_THROW(mesh_triangles_count == fbx_mesh_part.num_triangles, "Baked mesh triangle count does not match FBX mesh part", mesh_triangles_count, fbx_mesh_part.num_triangles);
        }

        if (mesh->Vertices.size() > std::numeric_limits<uint32_t>::max()) {
            throw ModelMeshBakerException(strex("Mesh '{}' has {} unindexed vertices, exceeds the uint32 writer count limit {}", fbx_node->name.data, mesh->Vertices.size(), std::numeric_limits<uint32_t>::max()));
        }

        vector<uint32_t> indices;
        indices.resize(mesh->Vertices.size());
        size_t skin_bone_count = 1;

        if (fbx_skin) {
            skin_bone_count = fbx_skin->clusters.count;
        }

        for (size_t vertex_index = 0; vertex_index < mesh->Vertices.size(); vertex_index++) {
            ValidateFbxVertex(mesh->Vertices[vertex_index], skin_bone_count, FbxValidationContext {.FileName = fname, .ScopeName = "geometry", .NodeName = bone->Name, .FieldName = "serialized_vertex", .ElementIndex = vertex_index});
        }

        ufbx_error fbx_generate_indices_error;
        FO_VERIFY_AND_THROW(!mesh->Vertices.empty(), "Baked mesh has no vertices");
        FO_VERIFY_AND_THROW(!indices.empty(), "Baked mesh has no indices");
        auto mesh_vertices_data = make_nptr(mesh->Vertices.data());
        const ufbx_vertex_stream fbx_vertex_stream[1] = {{mesh_vertices_data.void_cast(), mesh->Vertices.size(), sizeof(Vertex3D)}};
        const size_t result_vertices = ufbx_generate_indices(fbx_vertex_stream, 1, indices.data(), indices.size(), nullptr, &fbx_generate_indices_error);

        if (fbx_generate_indices_error.type != UFBX_ERROR_NONE) {
            throw ModelMeshBakerException(strex("FBX index generation failed for mesh '{}': {}", fbx_node->name.data, fbx_generate_indices_error.description.data));
        }
        if (result_vertices == 0 || result_vertices > mesh->Vertices.size()) {
            throw ModelMeshBakerException(strex("FBX index generation returned invalid vertex count {} for mesh '{}' with {} source vertices", result_vertices, fbx_node->name.data, mesh->Vertices.size()));
        }
        if (result_vertices > std::numeric_limits<uint32_t>::max()) {
            throw ModelMeshBakerException(strex("Mesh '{}' has {} indexed vertices, exceeds the uint32 writer count limit {}", fbx_node->name.data, result_vertices, std::numeric_limits<uint32_t>::max()));
        }
        if (indices.size() > std::numeric_limits<uint32_t>::max()) {
            throw ModelMeshBakerException(strex("Mesh '{}' has {} indices, exceeds the uint32 writer count limit {}", fbx_node->name.data, indices.size(), std::numeric_limits<uint32_t>::max()));
        }

        for (size_t index_position = 0; index_position < indices.size(); index_position++) {
            if (indices[index_position] >= result_vertices || indices[index_position] > std::numeric_limits<vindex_t>::max()) {
                throw ModelMeshBakerException(strex("Mesh '{}' has generated index {} at position {} outside the indexed vertex/vindex_t bounds [0, {})/[0, {}]", fbx_node->name.data, indices[index_position], index_position, result_vertices, std::numeric_limits<vindex_t>::max()));
            }
        }

        mesh->Vertices.resize(result_vertices);
        mesh->Indices.resize(indices.size());
        std::ranges::transform(indices, mesh->Indices.begin(), [](const uint32_t index) { return numeric_cast<vindex_t>(index); });

        if (fbx_skin) {
            mesh->SkinBones.reserve(fbx_skin->clusters.count);
            mesh->SkinBoneOffsets.reserve(fbx_skin->clusters.count);

            for (ptr<const ufbx_skin_cluster> fbx_skin_cluster : fbx_skin->clusters) {
                nptr<const BakerBone> skin_bone;
                nptr<const ufbx_node> fbx_skin_node = fbx_skin_cluster->bone_node;

                if (fbx_skin_node) {
                    const string skin_bone_name = fbx_skin_node->name.data;
                    skin_bone = root_bone->Find(skin_bone_name);

                    if (!skin_bone) {
                        WriteLog("Skin bone '{}' for mesh '{}' not found", skin_bone_name, fbx_node->name.data);
                    }
                }
                else {
                    WriteLog("Empty skin bone in fbx cluster for mesh '{}' not found", fbx_node->name.data);
                }

                if (!skin_bone) {
                    skin_bone = bone;
                }

                FO_VERIFY_AND_THROW(skin_bone, "Skin bone must resolve to a found or fallback mesh bone");
                mesh->SkinBones.emplace_back(skin_bone->Name);
                mesh->SkinBoneOffsets.emplace_back(ConvertFbxMatrix(fbx_skin_cluster->geometry_to_bone, FbxValidationContext {.FileName = fname, .ScopeName = "geometry", .NodeName = bone->Name, .FieldName = "geometry_to_bone", .ElementIndex = mesh->SkinBoneOffsets.size()}));
            }
        }
        else {
            mesh->SkinBones.emplace_back();
            mesh->SkinBoneOffsets.emplace_back(ConvertFbxMatrix(fbx_node->geometry_to_node, FbxValidationContext {.FileName = fname, .ScopeName = "geometry", .NodeName = bone->Name, .FieldName = "geometry_to_node"}));
        }

        if (fbx_node->materials.count != 0) {
            ptr<const ufbx_material> fbx_material = fbx_node->materials[0];

            for (const ufbx_material_texture& fbx_material_texture : fbx_material->textures) {
                if (string_view(fbx_material_texture.material_prop.data) == "DiffuseColor" && fbx_material_texture.texture != nullptr && fbx_material_texture.texture->type == UFBX_TEXTURE_FILE) {
                    mesh->DiffuseTexture = strvex(fbx_material_texture.texture->filename.data).extract_file_name();
                }
            }
        }
    }

    for (size_t i = 0; i < fbx_node->children.count; i++) {
        ConvertFbxMeshes(root_bone, bone->Children[i], fbx_node->children[i], fname);
    }
}

static auto ConvertFbxFloat(double value, const FbxValidationContext& context, string_view component) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr double min_float = static_cast<double>(std::numeric_limits<float32_t>::lowest());
    constexpr double max_float = static_cast<double>(std::numeric_limits<float32_t>::max());

    if (!std::isfinite(value) || value < min_float || value > max_float) {
        throw ModelMeshBakerException(strex("FBX '{}' has invalid numeric data in scope '{}', node '{}', field '{}', element {}, component '{}': {} is not representable as a finite float", context.FileName, context.ScopeName, context.NodeName, context.FieldName, context.ElementIndex, component, value));
    }

    return numeric_cast<float32_t>(value);
}

static auto ConvertFbxVec3(const ufbx_vec3& value, const FbxValidationContext& context) -> vec3
{
    FO_NO_STACK_TRACE_ENTRY();

    vec3 result;

    result.x = ConvertFbxFloat(value.x, context, "x");
    result.y = ConvertFbxFloat(value.y, context, "y");
    result.z = ConvertFbxFloat(value.z, context, "z");

    return result;
}

static auto ConvertFbxColor(const ufbx_vec4& value, const FbxValidationContext& context) -> ucolor
{
    FO_NO_STACK_TRACE_ENTRY();

    ucolor color;

    color.comp.r = ConvertFbxColorComponent(value.x, context, "r");
    color.comp.g = ConvertFbxColorComponent(value.y, context, "g");
    color.comp.b = ConvertFbxColorComponent(value.z, context, "b");
    color.comp.a = ConvertFbxColorComponent(value.w, context, "a");

    return color;
}

static auto ConvertFbxColorComponent(double value, const FbxValidationContext& context, string_view component) -> uint8_t
{
    FO_NO_STACK_TRACE_ENTRY();

    (void)ConvertFbxFloat(value, context, component);

    if (value < 0.0 || value > 1.0) {
        throw ModelMeshBakerException(strex("FBX '{}' has an out-of-range color in scope '{}', node '{}', field '{}', element {}, component '{}': {} is outside [0, 1]", context.FileName, context.ScopeName, context.NodeName, context.FieldName, context.ElementIndex, component, value));
    }

    return numeric_cast<uint8_t>(iround<int32_t>(value * 255.0));
}

static auto ConvertFbxMatrix(const ufbx_matrix& value, const FbxValidationContext& context) -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    mat44 result {1.0f};

    result[0][0] = ConvertFbxFloat(value.m00, context, "m00");
    result[1][0] = ConvertFbxFloat(value.m01, context, "m01");
    result[2][0] = ConvertFbxFloat(value.m02, context, "m02");
    result[3][0] = ConvertFbxFloat(value.m03, context, "m03");
    result[0][1] = ConvertFbxFloat(value.m10, context, "m10");
    result[1][1] = ConvertFbxFloat(value.m11, context, "m11");
    result[2][1] = ConvertFbxFloat(value.m12, context, "m12");
    result[3][1] = ConvertFbxFloat(value.m13, context, "m13");
    result[0][2] = ConvertFbxFloat(value.m20, context, "m20");
    result[1][2] = ConvertFbxFloat(value.m21, context, "m21");
    result[2][2] = ConvertFbxFloat(value.m22, context, "m22");
    result[3][2] = ConvertFbxFloat(value.m23, context, "m23");
    result[0][3] = 0.0f;
    result[1][3] = 0.0f;
    result[2][3] = 0.0f;
    result[3][3] = 1.0f;

    return result;
}

static void ValidateFbxVertex(const Vertex3D& vertex, size_t skin_bone_count, const FbxValidationContext& context)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(skin_bone_count != 0, "FBX vertex validation has no available skin bones", context.FileName, context.NodeName, context.ElementIndex);

    const auto validate_vec3 = [&](const vec3& value, string_view field_name) {
        auto field_context = context;
        field_context.FieldName = field_name;
        (void)ConvertFbxFloat(value.x, field_context, "x");
        (void)ConvertFbxFloat(value.y, field_context, "y");
        (void)ConvertFbxFloat(value.z, field_context, "z");
    };

    validate_vec3(vertex.Position, "position");
    validate_vec3(vertex.Normal, "normal");
    validate_vec3(vertex.Tangent, "tangent");
    validate_vec3(vertex.Bitangent, "bitangent");

    for (size_t component = 0; component < 2; component++) {
        auto field_context = context;
        field_context.FieldName = "uv";
        (void)ConvertFbxFloat(vertex.TexCoord[component], field_context, component == 0 ? "x" : "y");
        field_context.FieldName = "base_uv";
        (void)ConvertFbxFloat(vertex.TexCoordBase[component], field_context, component == 0 ? "x" : "y");
    }

    float32_t total_weight = 0.0f;

    for (size_t component = 0; component < MODEL_BONES_PER_VERTEX; component++) {
        const string component_name = strex("{}", component);
        auto field_context = context;
        field_context.FieldName = "blend_weight";
        (void)ConvertFbxFloat(vertex.BlendWeights[component], field_context, component_name);
        field_context.FieldName = "blend_index";
        (void)ConvertFbxFloat(vertex.BlendIndices[component], field_context, component_name);

        const float32_t weight = vertex.BlendWeights[component];
        const float32_t index = vertex.BlendIndices[component];

        if (weight < 0.0f || weight > 1.0f) {
            throw ModelMeshBakerException(strex("FBX '{}' mesh node '{}' has normalized skin weight {} outside [0, 1] at serialized vertex {}, influence {}", context.FileName, context.NodeName, weight, context.ElementIndex, component));
        }
        if (index < 0.0f || index != std::floor(index) || index >= numeric_cast<float32_t>(skin_bone_count)) {
            throw ModelMeshBakerException(strex("FBX '{}' mesh node '{}' has normalized skin index {} outside integer range [0, {}) at serialized vertex {}, influence {}", context.FileName, context.NodeName, index, skin_bone_count, context.ElementIndex, component));
        }

        total_weight += weight;
    }

    if (!std::isfinite(total_weight) || !is_float_equal(total_weight, 1.0f, FBX_SKIN_WEIGHT_SUM_TOLERANCE)) {
        throw ModelMeshBakerException(strex("FBX '{}' mesh node '{}' has normalized skin-weight sum {} instead of 1 at serialized vertex {}", context.FileName, context.NodeName, total_weight, context.ElementIndex));
    }
}

FO_END_NAMESPACE

#endif
