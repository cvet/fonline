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

#include "ufbx.h"

extern "C" void* ufbx_malloc(size_t size)
{
    FO_USING_NAMESPACE();
    constexpr SafeAllocator<uint8_t> allocator;
    ptr<uint8_t> bytes = allocator.allocate(size);
    return bytes.get_no_const();
}

extern "C" void* ufbx_realloc(void* memory, size_t old_size, size_t new_size)
{
    FO_USING_NAMESPACE();
    constexpr SafeAllocator<uint8_t> allocator;
    ptr<uint8_t> new_ptr = allocator.allocate(new_size);
    auto nullable_old_data = nptr<void> {memory}.reinterpret_as<uint8_t>();

    if (const size_t copy_size = std::min(old_size, new_size); copy_size != 0) {
        FO_STRONG_ASSERT(nullable_old_data, "Reallocation requested a copy but the previous block pointer is null");
        auto old_data = nullable_old_data.as_ptr();
        MemCopy(new_ptr.get_no_const(), old_data.get(), copy_size);
    }

    allocator.deallocate(nullable_old_data.get(), old_size);
    return new_ptr.get_no_const();
}

extern "C" void ufbx_free(void* ptr, size_t old_size)
{
    FO_USING_NAMESPACE();
    constexpr SafeAllocator<uint8_t> allocator;
    allocator.deallocate(nptr<void> {ptr}.reinterpret_as<uint8_t>().get(), old_size);
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
            auto child = Children[i].as_ptr();

            if (nptr<BakerBone> bone = child->Find(name); bone) {
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

struct BakerAnimSet
{
    struct BoneOutput
    {
        string Name {};
        vector<float32_t> ScaleTime {};
        vector<vec3> ScaleValue {};
        vector<float32_t> RotationTime {};
        vector<quaternion> RotationValue {};
        vector<float32_t> TranslationTime {};
        vector<vec3> TranslationValue {};
    };

    void Save(DataWriter& writer) const
    {
        FO_STACK_TRACE_ENTRY();

        auto len = numeric_cast<uint32_t>(AnimFileName.length());
        writer.Write<uint32_t>(len);
        writer.WriteStringBytes(AnimFileName);
        len = numeric_cast<uint32_t>(AnimName.length());
        writer.Write<uint32_t>(len);
        writer.WriteStringBytes(AnimName);
        writer.Write<float32_t>(Duration);
        len = numeric_cast<uint32_t>(BonesHierarchy.size());
        writer.Write<uint32_t>(len);
        for (const auto& i : BonesHierarchy) {
            len = numeric_cast<uint32_t>(i.size());
            writer.Write<uint32_t>(len);
            for (const auto& bone_name : i) {
                len = numeric_cast<uint32_t>(bone_name.length());
                writer.Write<uint32_t>(len);
                writer.WriteStringBytes(bone_name);
            }
        }
        len = numeric_cast<uint32_t>(BoneOutputs.size());
        writer.Write<uint32_t>(len);
        for (const auto& o : BoneOutputs) {
            len = numeric_cast<uint32_t>(o.Name.length());
            writer.Write<uint32_t>(len);
            writer.WriteStringBytes(o.Name);
            FO_VERIFY_AND_THROW(o.ScaleTime.size() == o.ScaleValue.size(), "Model bone scale keyframe times and values have different sizes", o.Name, o.ScaleTime.size(), o.ScaleValue.size());
            FO_VERIFY_AND_THROW(o.RotationTime.size() == o.RotationValue.size(), "Model bone rotation keyframe times and values have different sizes", o.Name, o.RotationTime.size(), o.RotationValue.size());
            FO_VERIFY_AND_THROW(o.TranslationTime.size() == o.TranslationValue.size(), "Model bone translation keyframe times and values have different sizes", o.Name, o.TranslationTime.size(), o.TranslationValue.size());
            len = numeric_cast<uint32_t>(o.ScaleTime.size());
            writer.Write<uint32_t>(len);
            writer.WriteObjectVector(o.ScaleTime);
            writer.WriteObjectVector(o.ScaleValue);
            len = numeric_cast<uint32_t>(o.RotationTime.size());
            writer.Write<uint32_t>(len);
            writer.WriteObjectVector(o.RotationTime);
            writer.WriteObjectVector(o.RotationValue);
            len = numeric_cast<uint32_t>(o.TranslationTime.size());
            writer.Write<uint32_t>(len);
            writer.WriteObjectVector(o.TranslationTime);
            writer.WriteObjectVector(o.TranslationValue);
        }
    }

    string AnimFileName {};
    string AnimName {};
    float32_t Duration {};
    vector<BoneOutput> BoneOutputs {};
    vector<vector<string>> BonesHierarchy {};
};

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

static auto ConvertFbxHierarchy(ptr<const ufbx_node> fbx_node) -> unique_ptr<BakerBone>;
static void ConvertFbxMeshes(ptr<BakerBone> root_bone, ptr<BakerBone> bone, ptr<const ufbx_node> fbx_node);
static auto ConvertFbxAnimations(ptr<const ufbx_scene> fbx_scene, string_view fname) -> vector<unique_ptr<BakerAnimSet>>;
static void CleanupUfbxScene(ufbx_scene* raw_scene) noexcept;
static auto MakeUfbxSceneHolder(ptr<ufbx_scene> scene) -> unique_del_ptr<ufbx_scene>;
static void CleanupUfbxBakedAnim(ufbx_baked_anim* raw_anim) noexcept;
static auto MakeUfbxBakedAnimHolder(ptr<ufbx_baked_anim> anim) -> unique_del_ptr<ufbx_baked_anim>;
static auto ConvertFbxVec3(const ufbx_vec3& v) -> vec3;
static auto ConvertFbxQuat(const ufbx_quat& q) -> quaternion;
static auto ConvertFbxColor(const ufbx_vec4& c) -> ucolor;
static auto ConvertFbxMatrix(const ufbx_matrix& m) -> mat44;

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
    nptr<const uint8_t> nullable_file_data = file_data.data();
    FO_VERIFY_AND_THROW(file_data.empty() || nullable_file_data, "Non-empty FBX file data has a null buffer pointer");
    nptr<const uint8_t> file_data_bytes = nullable_file_data;
    nptr<ufbx_scene> nullable_fbx_scene = ufbx_load_memory(file_data_bytes.get(), file_data.size(), &opts, &fbx_error);

    if (!nullable_fbx_scene) {
        throw ModelMeshBakerException("Unable to load FBX", fname, fbx_error.description.data);
    }

    auto fbx_scene = nullable_fbx_scene.as_ptr();
    auto fbx_scene_holder = MakeUfbxSceneHolder(fbx_scene);

    // Convert data
    auto root_bone = ConvertFbxHierarchy(fbx_scene->root_node);
    ConvertFbxMeshes(root_bone.as_ptr(), root_bone.as_ptr(), fbx_scene->root_node);
    const auto animations = ConvertFbxAnimations(fbx_scene, fname);

    // Write data
    vector<uint8_t> data;
    auto writer = DataWriter(data);

    root_bone->Save(writer);

    writer.Write<uint32_t>(numeric_cast<uint32_t>(animations.size()));

    for (const auto& loaded_animation : animations) {
        loaded_animation->Save(writer);
    }

    return data;
}

static void CleanupUfbxScene(ufbx_scene* raw_scene) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (raw_scene != nullptr) {
        ptr<ufbx_scene> scene = raw_scene;
        ufbx_free_scene(scene.get());
    }
}

static auto MakeUfbxSceneHolder(ptr<ufbx_scene> scene) -> unique_del_ptr<ufbx_scene>
{
    FO_STACK_TRACE_ENTRY();

    return unique_del_ptr<ufbx_scene> {scene.get_no_const(), CleanupUfbxScene};
}

static void CleanupUfbxBakedAnim(ufbx_baked_anim* raw_anim) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (raw_anim != nullptr) {
        ptr<ufbx_baked_anim> anim = raw_anim;
        ufbx_free_baked_anim(anim.get());
    }
}

static auto MakeUfbxBakedAnimHolder(ptr<ufbx_baked_anim> anim) -> unique_del_ptr<ufbx_baked_anim>
{
    FO_STACK_TRACE_ENTRY();

    return unique_del_ptr<ufbx_baked_anim> {anim.get_no_const(), CleanupUfbxBakedAnim};
}

static auto ConvertFbxHierarchy(ptr<const ufbx_node> fbx_node) -> unique_ptr<BakerBone>
{
    FO_STACK_TRACE_ENTRY();

    unique_ptr<BakerBone> bone = SafeAlloc::MakeUnique<BakerBone>();

    bone->Name = fbx_node->name.data;
    bone->TransformationMatrix = ConvertFbxMatrix(fbx_node->node_to_parent);
    bone->GlobalTransformationMatrix = ConvertFbxMatrix(fbx_node->node_to_world);
    bone->CombinedTransformationMatrix = mat44();
    bone->Children.reserve(fbx_node->children.count);

    for (size_t i = 0; i < fbx_node->children.count; i++) {
        bone->Children.emplace_back(ConvertFbxHierarchy(fbx_node->children[i]));
    }

    return bone;
}

static void ConvertFbxMeshes(ptr<BakerBone> root_bone, ptr<BakerBone> bone, ptr<const ufbx_node> fbx_node)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const ufbx_mesh> nullable_fbx_mesh = fbx_node->mesh;

    if (nullable_fbx_mesh && nullable_fbx_mesh->num_faces != 0) {
        bone->AttachedMesh.emplace();
        ptr<BakerMeshData> mesh = &*bone->AttachedMesh;
        auto fbx_mesh = nullable_fbx_mesh.as_ptr();
        FO_VERIFY_AND_THROW(fbx_mesh->num_faces == fbx_mesh->num_triangles, "FBX mesh contains non-triangle faces", fbx_mesh->num_faces, fbx_mesh->num_triangles);
        nptr<const ufbx_skin_deformer> nullable_fbx_skin = fbx_mesh->skin_deformers.count != 0 ? fbx_mesh->skin_deformers[0] : nullptr;

        mesh->Vertices.reserve(fbx_mesh->num_indices);

        for (const ufbx_mesh_part& fbx_mesh_part : fbx_mesh->material_parts) {
            vector<uint32_t> triangle_indices;
            triangle_indices.resize(fbx_mesh->max_face_triangles * 3);
            uint32_t mesh_triangles_count = 0;

            for (const uint32_t& face_index : fbx_mesh_part.face_indices) {
                const ufbx_face fbx_face = fbx_mesh->faces[face_index];
                FO_VERIFY_AND_THROW(!triangle_indices.empty(), "Triangulation buffer is empty");
                ptr<uint32_t> triangle_indices_data = triangle_indices.data();
                const uint32_t triangles_count = ufbx_triangulate_face(triangle_indices_data.get_no_const(), triangle_indices.size(), fbx_mesh.get(), fbx_face);

                mesh_triangles_count += triangles_count;

                for (size_t i = 0; i < numeric_cast<size_t>(triangles_count) * 3; i++) {
                    const uint32_t index = triangle_indices[i];
                    auto& v = mesh->Vertices.emplace_back();

                    v.Position = ConvertFbxVec3(fbx_mesh->vertex_position[index]);

                    if (fbx_mesh->vertex_normal.exists) {
                        v.Normal = ConvertFbxVec3(fbx_mesh->vertex_normal[index]);
                    }

                    if (fbx_mesh->vertex_tangent.exists) {
                        v.Tangent = ConvertFbxVec3(fbx_mesh->vertex_tangent[index]);
                    }

                    if (fbx_mesh->vertex_bitangent.exists) {
                        v.Bitangent = ConvertFbxVec3(fbx_mesh->vertex_bitangent[index]);
                    }

                    if (fbx_mesh->vertex_color.exists) {
                        v.Color = ConvertFbxColor(fbx_mesh->vertex_color[index]);
                    }

                    if (fbx_mesh->vertex_uv.exists) {
                        v.TexCoord[0] = numeric_cast<float32_t>(fbx_mesh->vertex_uv[index].x);
                        v.TexCoord[1] = 1.0f - numeric_cast<float32_t>(fbx_mesh->vertex_uv[index].y);
                        v.TexCoordBase[0] = v.TexCoord[0];
                        v.TexCoordBase[1] = v.TexCoord[1];
                    }

                    if (nullable_fbx_skin) {
                        auto fbx_skin = nullable_fbx_skin.as_ptr();
                        const uint32_t v_index = fbx_mesh->vertex_indices[index];
                        const ufbx_skin_vertex& fbx_skin_vertex = fbx_skin->vertices[v_index];
                        const size_t weights_count = std::min(numeric_cast<size_t>(fbx_skin_vertex.num_weights), MODEL_BONES_PER_VERTEX);

                        float32_t total_weight = 0.0f;

                        for (size_t w = 0; w < weights_count; w++) {
                            const ufbx_skin_weight skin_weight = fbx_skin->weights[fbx_skin_vertex.weight_begin + w];

                            v.BlendIndices[w] = numeric_cast<float32_t>(skin_weight.cluster_index);
                            v.BlendWeights[w] = numeric_cast<float32_t>(skin_weight.weight);

                            total_weight += numeric_cast<float32_t>(skin_weight.weight);
                        }

                        if (total_weight > 0.0f) {
                            for (size_t w = 0; w < weights_count; w++) {
                                v.BlendWeights[w] /= total_weight;
                            }
                        }
                        else if (weights_count != 0) {
                            v.BlendIndices[0] = 0.0f;
                            v.BlendWeights[0] = 1.0f;

                            for (size_t w = 1; w < weights_count; w++) {
                                v.BlendIndices[w] = 0.0f;
                                v.BlendWeights[w] = 0.0f;
                            }
                        }
                    }
                }
            }

            FO_VERIFY_AND_THROW(mesh_triangles_count == fbx_mesh_part.num_triangles, "Baked mesh triangle count does not match FBX mesh part", mesh_triangles_count, fbx_mesh_part.num_triangles);
        }

        vector<uint32_t> indices;
        indices.resize(mesh->Vertices.size());

        ufbx_error fbx_generate_indices_error;
        FO_VERIFY_AND_THROW(!mesh->Vertices.empty(), "Baked mesh has no vertices");
        FO_VERIFY_AND_THROW(!indices.empty(), "Baked mesh has no indices");
        ptr<const Vertex3D> mesh_vertices_data = mesh->Vertices.data();
        ptr<uint32_t> indices_data = indices.data();
        ptr<void> mesh_vertices_raw_data = cast_to_void(mesh_vertices_data.get());
        const ufbx_vertex_stream fbx_vertex_stream[1] = {{mesh_vertices_raw_data.get(), mesh->Vertices.size(), sizeof(Vertex3D)}};
        const size_t result_vertices = ufbx_generate_indices(fbx_vertex_stream, 1, indices_data.get(), indices.size(), nullptr, &fbx_generate_indices_error);

        if (fbx_generate_indices_error.type != UFBX_ERROR_NONE) {
            throw ModelMeshBakerException(strex("FBX index generation failed for mesh '{}': {}", fbx_node->name.data, fbx_generate_indices_error.description.data));
        }
        if (indices.size() > std::numeric_limits<vindex_t>::max()) {
            throw ModelMeshBakerException(strex("Mesh '{}' has {} indices, exceeds vindex_t limit {}", fbx_node->name.data, indices.size(), std::numeric_limits<vindex_t>::max()));
        }

        mesh->Vertices.resize(result_vertices);
        mesh->Indices.resize(indices.size());
        std::ranges::transform(indices, mesh->Indices.begin(), [](const uint32_t index) { return numeric_cast<vindex_t>(index); });

        if (nullable_fbx_skin) {
            auto fbx_skin = nullable_fbx_skin.as_ptr();

            if (fbx_skin->clusters.count > MODEL_MAX_BONES) {
                throw ModelMeshBakerException(strex("Mesh '{}' has {} skin clusters, exceeds MODEL_MAX_BONES limit {}", fbx_node->name.data, fbx_skin->clusters.count, MODEL_MAX_BONES));
            }

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

                mesh->SkinBones.emplace_back(skin_bone->Name);
                mesh->SkinBoneOffsets.emplace_back(ConvertFbxMatrix(fbx_skin_cluster->geometry_to_bone));
            }
        }
        else {
            mesh->SkinBones.emplace_back();
            mesh->SkinBoneOffsets.emplace_back(ConvertFbxMatrix(fbx_node->geometry_to_node));

            for (auto& v : mesh->Vertices) {
                v.BlendIndices[0] = 0.0f;
                v.BlendWeights[0] = 1.0f;
            }
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
        ConvertFbxMeshes(root_bone, bone->Children[i].as_ptr(), fbx_node->children[i]);
    }
}

static auto ConvertFbxAnimations(ptr<const ufbx_scene> fbx_scene, string_view fname) -> vector<unique_ptr<BakerAnimSet>>
{
    FO_STACK_TRACE_ENTRY();

    vector<unique_ptr<BakerAnimSet>> result;

    for (ptr<const ufbx_anim_stack> fbx_anim_stack : fbx_scene->anim_stacks) {
        ptr<const ufbx_anim> fbx_anim = fbx_anim_stack->anim;

        ufbx_bake_opts fbx_bake_opts = {};
        fbx_bake_opts.trim_start_time = true;
        ufbx_error fbx_error;
        nptr<ufbx_baked_anim> nullable_fbx_baked_anim = ufbx_bake_anim(fbx_scene.get(), fbx_anim.get(), &fbx_bake_opts, &fbx_error);
        FO_VERIFY_AND_THROW(nullable_fbx_baked_anim, "Missing required fbx baked animation");
        auto fbx_baked_anim = nullable_fbx_baked_anim.as_ptr();
        auto fbx_baked_anim_holder = MakeUfbxBakedAnimHolder(fbx_baked_anim);

        unique_ptr<BakerAnimSet> anim_set = SafeAlloc::MakeUnique<BakerAnimSet>();
        anim_set->AnimFileName = fname;
        anim_set->AnimName = fbx_anim_stack->name.data;
        anim_set->Duration = numeric_cast<float32_t>(fbx_baked_anim->playback_duration);

        for (const ufbx_baked_node& fbx_baked_anim_node : fbx_baked_anim->nodes) {
            vector<float32_t> tt;
            vector<vec3> tv;
            vector<float32_t> rt;
            vector<quaternion> rv;
            vector<float32_t> st;
            vector<vec3> sv;

            for (const ufbx_baked_vec3& translation_key : fbx_baked_anim_node.translation_keys) {
                tt.emplace_back(numeric_cast<float32_t>(translation_key.time));
                tv.emplace_back(ConvertFbxVec3(translation_key.value));
            }
            for (const ufbx_baked_quat& rotation_key : fbx_baked_anim_node.rotation_keys) {
                rt.emplace_back(numeric_cast<float32_t>(rotation_key.time));
                rv.emplace_back(ConvertFbxQuat(rotation_key.value));
            }
            for (const ufbx_baked_vec3& scale_key : fbx_baked_anim_node.scale_keys) {
                st.emplace_back(numeric_cast<float32_t>(scale_key.time));
                sv.emplace_back(ConvertFbxVec3(scale_key.value));
            }

            vector<string> hierarchy;

            for (nptr<const ufbx_node> nullable_fbx_node = fbx_scene->nodes[fbx_baked_anim_node.typed_id]; nullable_fbx_node;) {
                auto fbx_node = nullable_fbx_node.as_ptr();
                hierarchy.insert(hierarchy.begin(), fbx_node->name.data);
                nullable_fbx_node = fbx_node->parent;
            }

            BakerAnimSet::BoneOutput& bone_output = anim_set->BoneOutputs.emplace_back();
            bone_output.Name = hierarchy.back();
            bone_output.TranslationTime = tt;
            bone_output.TranslationValue = tv;
            bone_output.RotationTime = rt;
            bone_output.RotationValue = rv;
            bone_output.ScaleTime = st;
            bone_output.ScaleValue = sv;

            anim_set->BonesHierarchy.emplace_back(hierarchy);
        }

        result.emplace_back(std::move(anim_set));
    }

    return result;
}

static auto ConvertFbxVec3(const ufbx_vec3& v) -> vec3
{
    FO_NO_STACK_TRACE_ENTRY();

    vec3 result;

    result.x = numeric_cast<float32_t>(v.x);
    result.y = numeric_cast<float32_t>(v.y);
    result.z = numeric_cast<float32_t>(v.z);

    return result;
}

static auto ConvertFbxQuat(const ufbx_quat& q) -> quaternion
{
    FO_NO_STACK_TRACE_ENTRY();

    quaternion result;

    result.x = numeric_cast<float32_t>(q.x);
    result.y = numeric_cast<float32_t>(q.y);
    result.z = numeric_cast<float32_t>(q.z);
    result.w = numeric_cast<float32_t>(q.w);

    return result;
}

static auto ConvertFbxColor(const ufbx_vec4& c) -> ucolor
{
    FO_NO_STACK_TRACE_ENTRY();

    ucolor color;

    color.comp.r = numeric_cast<uint8_t>(iround<int32_t>(c.x * 255.0));
    color.comp.g = numeric_cast<uint8_t>(iround<int32_t>(c.y * 255.0));
    color.comp.b = numeric_cast<uint8_t>(iround<int32_t>(c.z * 255.0));
    color.comp.a = numeric_cast<uint8_t>(iround<int32_t>(c.w * 255.0));

    return color;
}

static auto ConvertFbxMatrix(const ufbx_matrix& m) -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    mat44 result {1.0f};

    result[0][0] = numeric_cast<float32_t>(m.m00);
    result[1][0] = numeric_cast<float32_t>(m.m01);
    result[2][0] = numeric_cast<float32_t>(m.m02);
    result[3][0] = numeric_cast<float32_t>(m.m03);
    result[0][1] = numeric_cast<float32_t>(m.m10);
    result[1][1] = numeric_cast<float32_t>(m.m11);
    result[2][1] = numeric_cast<float32_t>(m.m12);
    result[3][1] = numeric_cast<float32_t>(m.m13);
    result[0][2] = numeric_cast<float32_t>(m.m20);
    result[1][2] = numeric_cast<float32_t>(m.m21);
    result[2][2] = numeric_cast<float32_t>(m.m22);
    result[3][2] = numeric_cast<float32_t>(m.m23);
    result[0][3] = 0.0f;
    result[1][3] = 0.0f;
    result[2][3] = 0.0f;
    result[3][3] = 1.0f;

    return result;
}

FO_END_NAMESPACE

#endif
