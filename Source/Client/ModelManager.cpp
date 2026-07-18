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

#include "ModelManager.h"

#if FO_ENABLE_3D

#include "Application.h"
#include "EngineBase.h"
#include "ModelAnimation.h"
#include "ModelBakedData.h"
#include "ModelHierarchy.h"
#include "ModelInformation.h"
#include "ModelInstance.h"
#include "ModelMeshData.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

static auto LoadModelBone(DataReader& reader, HashResolver& hash_resolver, string_view context, uint32_t depth, uint32_t& joint_count) -> unique_ptr<ModelBone>;
static void FixModelBoneAfterLoad(ptr<ModelBone> bone, ptr<ModelBone> root_bone);

static constexpr size_t BAKED_STRING_MIN_SIZE = sizeof(uint32_t);
static constexpr size_t BAKED_MODEL_MESH_BONE_MIN_SIZE = BAKED_STRING_MIN_SIZE + 2 * sizeof(mat44) + sizeof(uint8_t) + sizeof(uint32_t);

ModelManager::ModelManager(ptr<RenderSettings> settings, ptr<FileSystem> resources, ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<GameTimer> game_time, ptr<HashResolver> hash_resolver, ptr<NameResolver> name_resolver, ptr<AnimationResolver> anim_name_resolver, TextureLoader tex_loader) :
    _settings {settings},
    _resources {resources},
    _effectMngr {effect_mngr},
    _render {render},
    _gameTime {game_time},
    _hashResolver {hash_resolver},
    _nameResolver {name_resolver},
    _animNameResolver {anim_name_resolver},
    _textureLoader {tex_loader},
    _particleMngr(settings, effect_mngr, render, resources, game_time, std::move(tex_loader))
{
    FO_STACK_TRACE_ENTRY();

    _moveTransitionTime = numeric_cast<float32_t>(_settings->Animation3dSmoothTime) / 1000.0f;
    _moveTransitionTime = std::max(_moveTransitionTime, 0.001f);

    if (_settings->Animation3dFPS != 0) {
        _animUpdateThreshold = iround<int32_t>(1000.0f / numeric_cast<float32_t>(_settings->Animation3dFPS));
    }

    _headBone = GetBoneHashedString(settings->HeadBone);

    for (const auto& bone_name : settings->LegBones) {
        _legBones.emplace(GetBoneHashedString(bone_name));
    }
}

ModelManager::~ModelManager() = default;

auto ModelManager::GetBoneHashedString(string_view name) const -> hstring
{
    FO_STACK_TRACE_ENTRY();

    return _hashResolver->ToHashedString(name);
}

auto ModelManager::LoadModel(string_view fname) -> nptr<ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    // Find already loaded
    auto name_hashed = _hashResolver->ToHashedString(fname);

    for (size_t i = 0; i != _loadedModels.size(); ++i) {
        auto root_bone = _loadedModels[i].as_ptr();

        if (root_bone->Name == name_hashed) {
            return root_bone;
        }
    }

    // Add to already processed
    if (_processedFiles.count(name_hashed) != 0) {
        return nullptr;
    }

    _processedFiles.emplace(name_hashed);

    // File maybe not exists, it's not an error
    if (!_resources->IsFileExists(fname)) {
        return nullptr;
    }

    // Load file data
    const auto file = _resources->ReadFile(fname);
    FO_VERIFY_AND_THROW(file, "3D model loader could not read model resource", fname);

    // Load bones
    auto reader = DataReader(file.GetDataSpan());
    auto root_bone = [&]() -> unique_ptr<ModelBone> {
        try {
            ReadModelMeshHeader(reader, fname);
            uint32_t joint_count = 0;
            auto loaded_root_bone = LoadModelBone(reader, *_hashResolver, fname, 0, joint_count);
            FixModelBoneAfterLoad(loaded_root_bone, loaded_root_bone);
            reader.VerifyEnd();
            return loaded_root_bone;
        }
        catch (const DataReadingException& ex) {
            throw DataReadingException("Invalid baked model mesh", fname, ex.what());
        }
    }();

    // Add to collection
    root_bone->Name = name_hashed;
    _loadedModels.emplace_back(std::move(root_bone));
    return _loadedModels.back();
}

auto ModelManager::CreateModel(string_view name) -> unique_nptr<ModelInstance>
{
    FO_STACK_TRACE_ENTRY();

    auto model_info = GetInformation(name);

    if (!model_info) {
        return nullptr;
    }

    auto model = model_info->CreateInstance();

    // Create mesh instances
    model->_allMeshes.reserve(model_info->_hierarchy->_allDrawBones.size());
    model->_allMeshesDisabled.resize(model_info->_hierarchy->_allDrawBones.size());

    for (size_t i = 0, j = model_info->_hierarchy->_allDrawBones.size(); i < j; i++) {
        auto bone = model_info->_hierarchy->_allDrawBones[i];
        auto mesh = bone->AttachedMesh ? make_nptr(&*bone->AttachedMesh) : nullptr;
        FO_VERIFY_AND_THROW(mesh, "Mesh is null");
        auto new_mesh_instance = SafeAlloc::MakeUnique<MeshInstance>(mesh);
        const string_view tex_name = mesh->DiffuseTexture;
        new_mesh_instance->CurTexures[0] = new_mesh_instance->DefaultTexures[0] = !tex_name.empty() ? nptr<MeshTexture>(model_info->_hierarchy->GetTexture(tex_name)) : nullptr;
        new_mesh_instance->CurEffect = new_mesh_instance->DefaultEffect = !mesh->EffectName.empty() ? nptr<RenderEffect>(model_info->_hierarchy->GetEffect(mesh->EffectName)) : nullptr;
        model->_allMeshes.emplace_back(std::move(new_mesh_instance));
    }

    model->PlayAnim(CritterStateAnim::None, CritterActionAnim::None, nullptr, 0.0f, ModelAnimFlags::Init);

    return model;
}

void ModelManager::PreloadModel(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    auto model_info = GetInformation(name);
    ignore_unused(model_info);
}

auto ModelManager::GetInformation(string_view name) -> nptr<ModelInformation>
{
    FO_STACK_TRACE_ENTRY();

    // Try to find instance
    for (size_t i = 0; i != _allModelInfos.size(); ++i) {
        auto model_info = _allModelInfos[i].as_ptr();

        if (model_info->_fileName == name) {
            return model_info;
        }
    }

    // Create new instance
    auto model_info = SafeAlloc::MakeUnique<ModelInformation>(this);

    if (!model_info->Load(name)) {
        return nullptr;
    }

    _allModelInfos.push_back(std::move(model_info));
    return _allModelInfos.back();
}

auto ModelManager::GetHierarchy(string_view name) -> nptr<ModelHierarchy>
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i != _hierarchyFiles.size(); ++i) {
        auto model_hierarchy = _hierarchyFiles[i].as_ptr();

        if (model_hierarchy->_fileName == name) {
            return model_hierarchy;
        }
    }

    // Load
    auto root_bone = LoadModel(name);

    if (!root_bone) {
        WriteLog("Unable to load model hierarchy file '{}'", name);
        return nullptr;
    }

    auto model_hierarchy = SafeAlloc::MakeUnique<ModelHierarchy>(this, string {name}, root_bone);
    model_hierarchy->SetupBones();

    _hierarchyFiles.emplace_back(std::move(model_hierarchy));
    return _hierarchyFiles.back();
}

static auto ReadModelBoneAttachedMesh(DataReader& reader, HashResolver& hash_resolver, ptr<ModelBone> owner, string_view context) -> MeshData
{
    FO_STACK_TRACE_ENTRY();

    auto mesh = MeshData {
        .Owner = owner,
    };

    const uint32_t vertices_count = reader.Read<uint32_t>();
    constexpr uint64_t max_vertex_count = uint64_t {std::numeric_limits<vindex_t>::max()} + uint64_t {1};

    if (vertices_count > max_vertex_count) {
        throw DataReadingException("Baked model mesh bone vertex count exceeds maximum addressable count", context, owner->Name, vertices_count, max_vertex_count);
    }

    VerifyModelBakedCountFitsData(reader, vertices_count, sizeof(Vertex3D), "mesh vertices", context);
    mesh.Vertices.resize(vertices_count);
    reader.ReadObjectArray(span {mesh.Vertices});

    const uint32_t indices_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, indices_count, sizeof(vindex_t), "mesh indices", context);
    mesh.Indices.resize(indices_count);
    reader.ReadObjectArray(span {mesh.Indices});

    for (size_t index_pos = 0; index_pos < mesh.Indices.size(); index_pos++) {
        if (numeric_cast<size_t>(mesh.Indices[index_pos]) >= mesh.Vertices.size()) {
            throw DataReadingException("Baked model mesh bone has vertex index outside vertex count", context, owner->Name, mesh.Indices[index_pos], index_pos, mesh.Vertices.size());
        }
    }

    mesh.DiffuseTexture = reader.ReadString();

    const uint32_t skin_bones_count = reader.Read<uint32_t>();

    if (skin_bones_count > MODEL_MAX_BONES) {
        throw DataReadingException("Baked model mesh bone skin bone count exceeds maximum", context, owner->Name, skin_bones_count, MODEL_MAX_BONES);
    }

    VerifyModelBakedCountFitsData(reader, skin_bones_count, BAKED_STRING_MIN_SIZE, "skin bone names", context);
    mesh.SkinBones.resize(skin_bones_count);
    mesh.SkinBoneNames.resize(skin_bones_count);

    for (uint32_t i = 0; i < skin_bones_count; i++) {
        const string tmp = reader.ReadString();
        mesh.SkinBoneNames[i] = hash_resolver.ToHashedString(tmp);
    }

    const uint32_t skin_bone_offsets_count = reader.Read<uint32_t>();

    if (skin_bone_offsets_count != skin_bones_count) {
        throw DataReadingException("Baked model mesh bone skin bone offset count mismatch", context, owner->Name, skin_bone_offsets_count, skin_bones_count);
    }

    VerifyModelBakedCountFitsData(reader, skin_bone_offsets_count, sizeof(mat44), "skin bone offsets", context);
    mesh.SkinBoneOffsets.resize(skin_bone_offsets_count);
    reader.ReadObjectArray(span {mesh.SkinBoneOffsets});

    for (size_t vertex_index = 0; vertex_index < mesh.Vertices.size(); vertex_index++) {
        const Vertex3D& vertex = mesh.Vertices[vertex_index];
        float32_t total_weight = 0.0f;

        for (size_t influence = 0; influence < MODEL_BONES_PER_VERTEX; influence++) {
            const float32_t weight = vertex.BlendWeights[influence];
            const float32_t index = vertex.BlendIndices[influence];

            if (!std::isfinite(weight)) {
                throw DataReadingException("Baked model mesh bone has non-finite skin weight", context, owner->Name, vertex_index, influence);
            }
            if (weight < 0.0f || weight > 1.0f) {
                throw DataReadingException("Baked model mesh bone has skin weight outside [0, 1]", context, owner->Name, weight, vertex_index, influence);
            }
            if (!std::isfinite(index)) {
                throw DataReadingException("Baked model mesh bone has non-finite skin index", context, owner->Name, vertex_index, influence);
            }
            if (index != std::trunc(index)) {
                throw DataReadingException("Baked model mesh bone has non-integral skin index", context, owner->Name, index, vertex_index, influence);
            }
            if (index < 0.0f || index >= numeric_cast<float32_t>(skin_bones_count)) {
                throw DataReadingException("Baked model mesh bone has skin index outside valid range", context, owner->Name, index, skin_bones_count, vertex_index, influence);
            }

            total_weight += weight;
        }

        if (!std::isfinite(total_weight) || !is_float_equal(total_weight, 1.0f, MODEL_MESH_SKIN_WEIGHT_SUM_TOLERANCE)) {
            throw DataReadingException("Baked model mesh bone has skin-weight sum that is not 1", context, owner->Name, total_weight, vertex_index);
        }
    }

    return mesh;
}

static auto LoadModelBone(DataReader& reader, HashResolver& hash_resolver, string_view context, uint32_t depth, uint32_t& joint_count) -> unique_ptr<ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    if (depth >= MODEL_MESH_MAX_HIERARCHY_DEPTH) {
        throw DataReadingException("Baked model mesh hierarchy depth exceeds maximum joints", context, MODEL_MESH_MAX_HIERARCHY_DEPTH);
    }
    if (joint_count >= MODEL_ANIMATION_MAX_JOINTS) {
        throw DataReadingException("Baked model mesh hierarchy exceeds maximum joints", context, MODEL_ANIMATION_MAX_JOINTS);
    }

    joint_count++;
    auto bone = SafeAlloc::MakeUnique<ModelBone>();

    const string tmp = reader.ReadString();
    const hstring source_name = hash_resolver.ToHashedString(tmp);
    bone->Name = source_name;
    bone->SourceName = source_name;

    bone->RestLocalTransform = reader.Read<mat44>();
    bone->TransformationMatrix = bone->RestLocalTransform;
    bone->GlobalTransformationMatrix = reader.Read<mat44>();

    if (reader.Read<uint8_t>() != 0) {
        bone->AttachedMesh.emplace(ReadModelBoneAttachedMesh(reader, hash_resolver, bone, context));
    }

    const uint32_t children_count = reader.Read<uint32_t>();

    if (children_count > MODEL_ANIMATION_MAX_JOINTS) {
        throw DataReadingException("Baked model mesh bone child count exceeds maximum", context, bone->Name, children_count, MODEL_ANIMATION_MAX_JOINTS);
    }

    VerifyModelBakedCountFitsData(reader, children_count, BAKED_MODEL_MESH_BONE_MIN_SIZE, "child bones", context);

    for (uint32_t i = 0; i < children_count; i++) {
        bone->Children.emplace_back(LoadModelBone(reader, hash_resolver, context, depth + 1, joint_count));
    }

    bone->CombinedTransformationMatrix = mat44();
    return bone;
}

// ReSharper disable once CppMemberFunctionMayBeConst
static void FixModelBoneAfterLoad(ptr<ModelBone> bone, ptr<ModelBone> root_bone)
{
    FO_STACK_TRACE_ENTRY();

    if (bone->AttachedMesh) {
        for (size_t i = 0; i < bone->AttachedMesh->SkinBoneNames.size(); i++) {
            if (bone->AttachedMesh->SkinBoneNames[i]) {
                auto skin_bone = FindModelBone(root_bone, bone->AttachedMesh->SkinBoneNames[i]);
                FO_VERIFY_AND_THROW(skin_bone, "Skin bone was not found in a model", bone->AttachedMesh->SkinBoneNames[i], bone->Name);
                bone->AttachedMesh->SkinBones[i] = skin_bone;
            }
            else {
                bone->AttachedMesh->SkinBones[i] = bone->AttachedMesh->Owner;
            }
        }
    }

    for (size_t i = 0; i != bone->Children.size(); ++i) {
        FixModelBoneAfterLoad(bone->Children[i], root_bone);
    }
}

FO_END_NAMESPACE

#endif
