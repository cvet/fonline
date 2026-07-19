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
#include "ModelHierarchy.h"
#include "ModelInformation.h"
#include "ModelInstance.h"
#include "ModelMeshData.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

static auto ConvertModelMeshBone(ModelMeshBoneData&& source, HashResolver& hash_resolver) -> unique_ptr<ModelBone>;
static void FixModelBoneAfterLoad(ptr<ModelBone> bone, ptr<ModelBone> root_bone);

static_assert(std::same_as<ModelMeshIndexData, vindex_t>);
static_assert(MODEL_MESH_BONES_PER_VERTEX == MODEL_BONES_PER_VERTEX);
static_assert(sizeof(ModelMeshVertexData) == sizeof(Vertex3D));
static_assert(alignof(ModelMeshVertexData) == alignof(Vertex3D));
static_assert(offsetof(ModelMeshVertexData, Position) == offsetof(Vertex3D, Position));
static_assert(offsetof(ModelMeshVertexData, Normal) == offsetof(Vertex3D, Normal));
static_assert(offsetof(ModelMeshVertexData, TexCoord) == offsetof(Vertex3D, TexCoord));
static_assert(offsetof(ModelMeshVertexData, TexCoordBase) == offsetof(Vertex3D, TexCoordBase));
static_assert(offsetof(ModelMeshVertexData, Tangent) == offsetof(Vertex3D, Tangent));
static_assert(offsetof(ModelMeshVertexData, Bitangent) == offsetof(Vertex3D, Bitangent));
static_assert(offsetof(ModelMeshVertexData, BlendWeights) == offsetof(Vertex3D, BlendWeights));
static_assert(offsetof(ModelMeshVertexData, BlendIndices) == offsetof(Vertex3D, BlendIndices));
static_assert(offsetof(ModelMeshVertexData, Color) == offsetof(Vertex3D, Color));

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
            ModelMeshData mesh_data = ReadModelMeshData(reader, fname);
            FO_VERIFY_AND_THROW(mesh_data.RootBone, "Decoded model mesh has no root bone", fname);
            auto loaded_root_bone = ConvertModelMeshBone(std::move(*mesh_data.RootBone), *_hashResolver);
            FixModelBoneAfterLoad(loaded_root_bone, loaded_root_bone);
            return loaded_root_bone;
        }
        catch (const std::exception& ex) {
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

static auto ConvertModelMeshGeometry(ModelMeshGeometryData&& source, HashResolver& hash_resolver, ptr<ModelBone> owner) -> MeshData
{
    FO_STACK_TRACE_ENTRY();

    auto mesh = MeshData {
        .Owner = owner,
    };

    mesh.Vertices.reserve(source.Vertices.size());

    for (const ModelMeshVertexData& source_vertex : source.Vertices) {
        auto& vertex = mesh.Vertices.emplace_back();
        vertex.Position = source_vertex.Position;
        vertex.Normal = source_vertex.Normal;
        std::ranges::copy(source_vertex.TexCoord, vertex.TexCoord);
        std::ranges::copy(source_vertex.TexCoordBase, vertex.TexCoordBase);
        vertex.Tangent = source_vertex.Tangent;
        vertex.Bitangent = source_vertex.Bitangent;
        std::ranges::copy(source_vertex.BlendWeights, vertex.BlendWeights);
        std::ranges::copy(source_vertex.BlendIndices, vertex.BlendIndices);
        vertex.Color = source_vertex.Color;
    }

    mesh.Indices = std::move(source.Indices);
    mesh.DiffuseTexture = std::move(source.DiffuseTexture);
    mesh.SkinBones.resize(source.SkinBoneNames.size());
    mesh.SkinBoneNames.reserve(source.SkinBoneNames.size());

    for (const string& skin_bone_name : source.SkinBoneNames) {
        mesh.SkinBoneNames.emplace_back(hash_resolver.ToHashedString(skin_bone_name));
    }

    mesh.SkinBoneOffsets = std::move(source.SkinBoneOffsets);
    return mesh;
}

static auto ConvertModelMeshBone(ModelMeshBoneData&& source, HashResolver& hash_resolver) -> unique_ptr<ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    auto bone = SafeAlloc::MakeUnique<ModelBone>();
    const hstring source_name = hash_resolver.ToHashedString(source.Name);
    bone->Name = source_name;
    bone->SourceName = source_name;
    bone->RestLocalTransform = source.TransformationMatrix;
    bone->TransformationMatrix = bone->RestLocalTransform;
    bone->GlobalTransformationMatrix = source.GlobalTransformationMatrix;

    if (source.AttachedMesh) {
        bone->AttachedMesh.emplace(ConvertModelMeshGeometry(std::move(*source.AttachedMesh), hash_resolver, bone));
    }

    bone->Children.reserve(source.Children.size());

    for (auto& child : source.Children) {
        bone->Children.emplace_back(ConvertModelMeshBone(std::move(*child), hash_resolver));
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
