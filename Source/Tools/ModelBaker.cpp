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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ModelBaker.h"

#if FO_ENABLE_3D

#include "Application.h"
#include "Log.h"
#include "StringUtils.h"

#include "ufbx.h"

extern "C" void* ufbx_malloc(size_t size)
{
    constexpr SafeAllocator<uint8> allocator;
    return allocator.allocate(size);
}

extern "C" void* ufbx_realloc(void* ptr, size_t old_size, size_t new_size)
{
    constexpr SafeAllocator<uint8> allocator;
    auto* new_ptr = allocator.allocate(new_size);
    MemCopy(new_ptr, ptr, std::min(old_size, new_size));
    allocator.deallocate(static_cast<uint8*>(ptr), old_size);
    return new_ptr;
}

extern "C" void ufbx_free(void* ptr, size_t old_size)
{
    constexpr SafeAllocator<uint8> allocator;
    allocator.deallocate(static_cast<uint8*>(ptr), old_size);
}

struct BakerMeshData
{
    void Save(DataWriter& writer) const
    {
        STACK_TRACE_ENTRY();

        auto len = static_cast<uint>(Vertices.size());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(Vertices.data(), len * sizeof(Vertex3D));
        len = static_cast<uint>(Indices.size());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(Indices.data(), len * sizeof(vindex_t));
        len = static_cast<uint>(DiffuseTexture.length());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(DiffuseTexture.data(), len);
        len = static_cast<uint>(SkinBones.size());
        writer.WritePtr(&len, sizeof(len));
        for (const auto& bone_name : SkinBones) {
            len = static_cast<uint>(bone_name.length());
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(bone_name.data(), len);
        }
        len = static_cast<uint>(SkinBoneOffsets.size());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(SkinBoneOffsets.data(), len * sizeof(mat44));
    }

    vector<Vertex3D> Vertices {};
    vector<vindex_t> Indices {};
    string DiffuseTexture {};
    vector<string> SkinBones {};
    vector<mat44> SkinBoneOffsets {};
    string EffectName {};
};

struct BakerBone
{
    auto Find(const string& name) -> BakerBone*
    {
        STACK_TRACE_ENTRY();

        if (Name == name) {
            return this;
        }

        for (auto& child : Children) {
            if (BakerBone* bone = child->Find(name); bone != nullptr) {
                return bone;
            }
        }

        return nullptr;
    }

    void Save(DataWriter& writer) const
    {
        STACK_TRACE_ENTRY();

        writer.Write<uint>(static_cast<uint>(Name.length()));
        writer.WritePtr(Name.data(), Name.length());
        writer.WritePtr(&TransformationMatrix, sizeof(TransformationMatrix));
        writer.WritePtr(&GlobalTransformationMatrix, sizeof(GlobalTransformationMatrix));

        if (AttachedMesh) {
            writer.Write<uint8>(1);
            AttachedMesh->Save(writer);
        }
        else {
            writer.Write<uint8>(0);
        }

        writer.Write<uint>(static_cast<uint>(Children.size()));

        for (const auto& child : Children) {
            child->Save(writer);
        }
    }

    string Name {};
    mat44 TransformationMatrix {};
    mat44 GlobalTransformationMatrix {};
    unique_ptr<BakerMeshData> AttachedMesh {};
    vector<unique_ptr<BakerBone>> Children {};
    mat44 CombinedTransformationMatrix {};
};

struct BakerAnimSet
{
    struct BoneOutput
    {
        string Name {};
        vector<float> ScaleTime {};
        vector<vec3> ScaleValue {};
        vector<float> RotationTime {};
        vector<quaternion> RotationValue {};
        vector<float> TranslationTime {};
        vector<vec3> TranslationValue {};
    };

    void Save(DataWriter& writer) const
    {
        STACK_TRACE_ENTRY();

        auto len = static_cast<uint>(AnimFileName.length());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(AnimFileName.data(), len);
        len = static_cast<uint>(AnimName.length());
        writer.WritePtr(&len, sizeof(len));
        writer.WritePtr(AnimName.data(), len);
        writer.WritePtr(&Duration, sizeof(Duration));
        len = static_cast<uint>(BonesHierarchy.size());
        writer.WritePtr(&len, sizeof(len));
        for (const auto& i : BonesHierarchy) {
            len = static_cast<uint>(i.size());
            writer.WritePtr(&len, sizeof(len));
            for (const auto& bone_name : i) {
                len = static_cast<uint>(bone_name.length());
                writer.WritePtr(&len, sizeof(len));
                writer.WritePtr(bone_name.data(), len);
            }
        }
        len = static_cast<uint>(BoneOutputs.size());
        writer.WritePtr(&len, sizeof(len));
        for (const auto& o : BoneOutputs) {
            len = static_cast<uint>(o.Name.length());
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(o.Name.data(), len);
            len = static_cast<uint>(o.ScaleTime.size());
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(o.ScaleTime.data(), len * sizeof(o.ScaleTime[0]));
            writer.WritePtr(o.ScaleValue.data(), len * sizeof(o.ScaleValue[0]));
            len = static_cast<uint>(o.RotationTime.size());
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(o.RotationTime.data(), len * sizeof(o.RotationTime[0]));
            writer.WritePtr(o.RotationValue.data(), len * sizeof(o.RotationValue[0]));
            len = static_cast<uint>(o.TranslationTime.size());
            writer.WritePtr(&len, sizeof(len));
            writer.WritePtr(o.TranslationTime.data(), len * sizeof(o.TranslationTime[0]));
            writer.WritePtr(o.TranslationValue.data(), len * sizeof(o.TranslationValue[0]));
        }
    }

    string AnimFileName {};
    string AnimName {};
    float Duration {};
    vector<BoneOutput> BoneOutputs {};
    vector<vector<string>> BonesHierarchy {};
};

ModelBaker::ModelBaker(const BakerSettings& settings, BakeCheckerCallback bake_checker, WriteDataCallback write_data) :
    BaseBaker(settings, std::move(bake_checker), std::move(write_data))
{
    STACK_TRACE_ENTRY();
}

ModelBaker::~ModelBaker()
{
    STACK_TRACE_ENTRY();
}

void ModelBaker::BakeFiles(FileCollection files)
{
    STACK_TRACE_ENTRY();

    vector<std::future<void>> file_bakings;

    for (files.ResetCounter(); files.MoveNext();) {
        auto file_header = files.GetCurFileHeader();
        string ext = strex(file_header.GetPath()).getFileExtension();

        if (!IsExtSupported(ext)) {
            continue;
        }

        if (_bakeChecker && !_bakeChecker(file_header)) {
            continue;
        }

        file_bakings.emplace_back(std::async(GetAsyncMode(), [this, ext, file = files.GetCurFile()] {
            if (ext == "fo3d") {
                _writeData(file.GetPath(), file.GetData());
            }
            else {
                auto data = BakeFbxFile(file.GetPath(), file);
                _writeData(file.GetPath(), data);
            }
        }));
    }

    int errors = 0;

    for (auto& file_baking : file_bakings) {
        try {
            file_baking.get();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
        catch (...) {
            UNKNOWN_EXCEPTION();
        }
    }

    if (errors != 0) {
        throw ModelBakerException("Errors during effects baking", errors);
    }
}

static auto ConvertFbxHierarchy(const ufbx_node* fbx_node) -> unique_ptr<BakerBone>;
static void ConvertFbxMeshes(BakerBone* root_bone, BakerBone* bone, const ufbx_node* fbx_node);
static auto ConvertFbxAnimations(const ufbx_scene* fbx_scene, string_view fname) -> vector<unique_ptr<BakerAnimSet>>;
static auto ConvertFbxVec3(const ufbx_vec3& v) -> vec3;
static auto ConvertFbxQuat(const ufbx_quat& q) -> quaternion;
static auto ConvertFbxColor(const ufbx_vec4& c) -> ucolor;
static auto ConvertFbxMatrix(const ufbx_matrix& m) -> mat44;

auto ModelBaker::BakeFbxFile(string_view fname, const File& file) -> vector<uint8>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    ufbx_load_opts opts = {};
    opts.ignore_embedded = true;
    opts.evaluate_skinning = true;
    opts.ignore_missing_external_files = true;
    opts.clean_skin_weights = true;
    opts.generate_missing_normals = true;
    opts.normalize_normals = true;
    opts.normalize_tangents = true;

    ufbx_error fbx_error;
    ufbx_scene* fbx_scene = ufbx_load_memory(file.GetBuf(), file.GetSize(), &opts, &fbx_error);

    if (fbx_scene == nullptr) {
        throw ModelBakerException("Unable to load FBX", fbx_error.description.data);
    }

    auto fbx_scene_cleanup = ScopeCallback([fbx_scene]() noexcept { safe_call([&] { ufbx_free_scene(fbx_scene); }); });

    // Convert data
    auto root_bone = ConvertFbxHierarchy(fbx_scene->root_node);
    ConvertFbxMeshes(root_bone.get(), root_bone.get(), fbx_scene->root_node);
    const auto animations = ConvertFbxAnimations(fbx_scene, fname);

    // Write data
    vector<uint8> data;
    auto writer = DataWriter(data);

    root_bone->Save(writer);

    writer.Write<uint>(static_cast<uint>(animations.size()));

    for (const auto& loaded_animation : animations) {
        loaded_animation->Save(writer);
    }

    return data;
}

static auto ConvertFbxHierarchy(const ufbx_node* fbx_node) -> unique_ptr<BakerBone>
{
    STACK_TRACE_ENTRY();

    auto bone = SafeAlloc::MakeUnique<BakerBone>();

    bone->Name = fbx_node->name.data;
    bone->TransformationMatrix = ConvertFbxMatrix(fbx_node->node_to_parent);
    bone->GlobalTransformationMatrix = ConvertFbxMatrix(fbx_node->node_to_world);
    bone->CombinedTransformationMatrix = mat44();
    bone->Children.resize(fbx_node->children.count);

    for (size_t i = 0; i < fbx_node->children.count; i++) {
        bone->Children[i] = ConvertFbxHierarchy(fbx_node->children[i]);
    }

    return bone;
}

static void ConvertFbxMeshes(BakerBone* root_bone, BakerBone* bone, const ufbx_node* fbx_node)
{
    STACK_TRACE_ENTRY();

    if (fbx_node->mesh != nullptr && fbx_node->mesh->num_faces != 0) {
        bone->AttachedMesh = SafeAlloc::MakeUnique<BakerMeshData>();
        BakerMeshData* mesh = bone->AttachedMesh.get();
        const auto* fbx_mesh = fbx_node->mesh;
        RUNTIME_ASSERT(fbx_mesh->num_faces == fbx_mesh->num_triangles);
        const auto* fbx_skin = fbx_mesh->skin_deformers.count != 0 ? fbx_mesh->skin_deformers[0] : nullptr;

        mesh->Vertices.reserve(fbx_mesh->num_indices);

        for (const ufbx_mesh_part& fbx_mesh_part : fbx_mesh->material_parts) {
            vector<uint32_t> triangle_indices;
            triangle_indices.resize(fbx_mesh->max_face_triangles * 3);
            uint32_t mesh_triangles_count = 0;

            for (const uint32_t& face_index : fbx_mesh_part.face_indices) {
                const ufbx_face fbx_face = fbx_mesh->faces[face_index];
                const uint32_t triangles_count = ufbx_triangulate_face(triangle_indices.data(), triangle_indices.size(), fbx_mesh, fbx_face);

                mesh_triangles_count += triangles_count;
                RUNTIME_ASSERT(mesh_triangles_count <= std::numeric_limits<vindex_t>::max());

                for (size_t i = 0; i < static_cast<size_t>(triangles_count) * 3; i++) {
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
                        v.TexCoord[0] = static_cast<float>(fbx_mesh->vertex_uv[index].x);
                        v.TexCoord[1] = 1.0f - static_cast<float>(fbx_mesh->vertex_uv[index].y);
                        v.TexCoordBase[0] = v.TexCoord[0];
                        v.TexCoordBase[1] = v.TexCoord[1];
                    }

                    if (fbx_skin != nullptr) {
                        const uint32_t v_index = fbx_mesh->vertex_indices[index];
                        const ufbx_skin_vertex& fbx_skin_vertex = fbx_skin->vertices[v_index];
                        const size_t weights_count = std::min(static_cast<size_t>(fbx_skin_vertex.num_weights), BONES_PER_VERTEX);

                        float total_weight = 0.0f;

                        for (size_t w = 0; w < weights_count; w++) {
                            const ufbx_skin_weight skin_weight = fbx_skin->weights[fbx_skin_vertex.weight_begin + w];

                            v.BlendIndices[w] = static_cast<float>(skin_weight.cluster_index);
                            v.BlendWeights[w] = static_cast<float>(skin_weight.weight);

                            total_weight += static_cast<float>(skin_weight.weight);
                        }

                        for (size_t w = 0; w < weights_count; w++) {
                            v.BlendWeights[w] /= total_weight;
                        }
                    }
                }
            }

            RUNTIME_ASSERT(mesh_triangles_count == fbx_mesh_part.num_triangles);
        }

#if 1
        vector<uint32_t> indices;
        indices.resize(mesh->Vertices.size());

        ufbx_error fbx_generate_indices_error;
        const ufbx_vertex_stream fbx_vertex_stream[1] = {{mesh->Vertices.data(), mesh->Vertices.size(), sizeof(Vertex3D)}};
        const size_t result_vertices = ufbx_generate_indices(fbx_vertex_stream, 1, indices.data(), indices.size(), nullptr, &fbx_generate_indices_error);
        RUNTIME_ASSERT(fbx_generate_indices_error.type == UFBX_ERROR_NONE);
        mesh->Vertices.resize(result_vertices);

        mesh->Indices.resize(indices.size());
        std::transform(indices.begin(), indices.end(), mesh->Indices.begin(), [](const uint32_t index) { return static_cast<vindex_t>(index); });

#else
        for (size_t i = 0; i < mesh->Vertices.size(); i++) {
            mesh->Indices.emplace_back(static_cast<vindex_t>(mesh->Indices.size()));
        }
#endif

        if (fbx_skin != nullptr) {
            RUNTIME_ASSERT(fbx_skin->clusters.count <= MODEL_MAX_BONES);

            mesh->SkinBones.reserve(fbx_skin->clusters.count);
            mesh->SkinBoneOffsets.reserve(fbx_skin->clusters.count);

            for (const ufbx_skin_cluster* fbx_skin_cluster : fbx_skin->clusters) {
                const BakerBone* skin_bone = nullptr;
                const ufbx_node* fbx_skin_node = fbx_skin_cluster->bone_node;

                if (fbx_skin_node != nullptr) {
                    const string skin_bone_name = fbx_skin_node->name.data;
                    skin_bone = root_bone->Find(skin_bone_name);

                    if (skin_bone == nullptr) {
                        WriteLog("Skin bone '{}' for mesh '{}' not found", skin_bone_name, fbx_node->name.data);
                    }
                }
                else {
                    WriteLog("Empty skin bone in fbx cluster for mesh '{}' not found", fbx_node->name.data);
                }

                if (skin_bone == nullptr) {
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
            const ufbx_material* fbx_material = fbx_node->materials[0];

            for (const ufbx_material_texture& fbx_material_texture : fbx_material->textures) {
                if (string_view(fbx_material_texture.material_prop.data) == "DiffuseColor" && fbx_material_texture.texture != nullptr && fbx_material_texture.texture->type == UFBX_TEXTURE_FILE) {
                    mesh->DiffuseTexture = strex(fbx_material_texture.texture->filename.data).extractFileName();
                }
            }
        }
    }

    for (size_t i = 0; i < fbx_node->children.count; i++) {
        ConvertFbxMeshes(root_bone, bone->Children[i].get(), fbx_node->children[i]);
    }
}

static auto ConvertFbxAnimations(const ufbx_scene* fbx_scene, string_view fname) -> vector<unique_ptr<BakerAnimSet>>
{
    STACK_TRACE_ENTRY();

    vector<unique_ptr<BakerAnimSet>> result;

    for (const ufbx_anim_stack* fbx_anim_stack : fbx_scene->anim_stacks) {
        const ufbx_anim* fbx_anim = fbx_anim_stack->anim;

        ufbx_bake_opts fbx_bake_opts = {};
        fbx_bake_opts.trim_start_time = true;
        ufbx_error fbx_error;
        ufbx_baked_anim* fbx_baked_anim = ufbx_bake_anim(fbx_scene, fbx_anim, &fbx_bake_opts, &fbx_error);
        RUNTIME_ASSERT(fbx_baked_anim);

        auto fbx_baked_anim_cleanup = ScopeCallback([fbx_baked_anim]() noexcept { safe_call([&] { ufbx_free_baked_anim(fbx_baked_anim); }); });

        auto anim_set = SafeAlloc::MakeUnique<BakerAnimSet>();
        anim_set->AnimFileName = fname;
        anim_set->AnimName = fbx_anim_stack->name.data;
        anim_set->Duration = static_cast<float>(fbx_baked_anim->playback_duration);

        for (const ufbx_baked_node& fbx_baked_anim_node : fbx_baked_anim->nodes) {
            vector<float> tt;
            vector<vec3> tv;
            vector<float> rt;
            vector<quaternion> rv;
            vector<float> st;
            vector<vec3> sv;

            for (const ufbx_baked_vec3& translation_key : fbx_baked_anim_node.translation_keys) {
                tt.emplace_back(static_cast<float>(translation_key.time));
                tv.emplace_back(ConvertFbxVec3(translation_key.value));
            }
            for (const ufbx_baked_quat& rotation_key : fbx_baked_anim_node.rotation_keys) {
                rt.emplace_back(static_cast<float>(rotation_key.time));
                rv.emplace_back(ConvertFbxQuat(rotation_key.value));
            }
            for (const ufbx_baked_vec3& scale_key : fbx_baked_anim_node.scale_keys) {
                st.emplace_back(static_cast<float>(scale_key.time));
                sv.emplace_back(ConvertFbxVec3(scale_key.value));
            }

            vector<string> hierarchy;

            for (ufbx_node* fbx_node = fbx_scene->nodes[fbx_baked_anim_node.typed_id]; fbx_node != nullptr;) {
                hierarchy.insert(hierarchy.begin(), fbx_node->name.data);
                fbx_node = fbx_node->parent;
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
    NO_STACK_TRACE_ENTRY();

    vec3 result;

    result.x = static_cast<float>(v.x);
    result.y = static_cast<float>(v.y);
    result.z = static_cast<float>(v.z);

    return result;
}

static auto ConvertFbxQuat(const ufbx_quat& q) -> quaternion
{
    NO_STACK_TRACE_ENTRY();

    quaternion result;

    result.x = static_cast<float>(q.x);
    result.y = static_cast<float>(q.y);
    result.z = static_cast<float>(q.z);
    result.w = static_cast<float>(q.w);

    return result;
}

static auto ConvertFbxColor(const ufbx_vec4& c) -> ucolor
{
    NO_STACK_TRACE_ENTRY();

    ucolor color;

    color.comp.r = static_cast<uint8>(iround(c.x * 255.0));
    color.comp.g = static_cast<uint8>(iround(c.y * 255.0));
    color.comp.b = static_cast<uint8>(iround(c.z * 255.0));
    color.comp.a = static_cast<uint8>(iround(c.w * 255.0));

    return color;
}

static auto ConvertFbxMatrix(const ufbx_matrix& m) -> mat44
{
    NO_STACK_TRACE_ENTRY();

    mat44 result;

    result.a1 = static_cast<float>(m.m00);
    result.a2 = static_cast<float>(m.m01);
    result.a3 = static_cast<float>(m.m02);
    result.a4 = static_cast<float>(m.m03);
    result.b1 = static_cast<float>(m.m10);
    result.b2 = static_cast<float>(m.m11);
    result.b3 = static_cast<float>(m.m12);
    result.b4 = static_cast<float>(m.m13);
    result.c1 = static_cast<float>(m.m20);
    result.c2 = static_cast<float>(m.m21);
    result.c3 = static_cast<float>(m.m22);
    result.c4 = static_cast<float>(m.m23);
    result.d1 = 0.0f;
    result.d2 = 0.0f;
    result.d3 = 0.0f;
    result.d4 = 1.0f;

    return result;
}

#endif
