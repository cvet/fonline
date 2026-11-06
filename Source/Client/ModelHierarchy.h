// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

#include "EffectManager.h"
#include "Geometry.h"
#include "ModelAnimation.h"

FO_BEGIN_NAMESPACE

class ModelManager;

struct ModelBone;

struct MeshTexture
{
    hstring Name {};
    ptr<RenderTexture> MainTex;
    frect32 AtlasOffsetData {};
};

struct MeshData
{
    ptr<ModelBone> Owner;
    vector<Vertex3D> Vertices {};
    vector<vindex_t> Indices {};
    string DiffuseTexture {};
    vector<hstring> SkinBoneNames {};
    vector<mat44> SkinBoneOffsets {};
    vector<nptr<ModelBone>> SkinBones {};
    string EffectName {};
};

struct MeshInstance
{
    ptr<MeshData> Mesh;
    bool Disabled {};
    nptr<const MeshTexture> CurTexures[MODEL_MAX_TEXTURES] {};
    nptr<const MeshTexture> DefaultTexures[MODEL_MAX_TEXTURES] {};
    nptr<const MeshTexture> LastTexures[MODEL_MAX_TEXTURES] {};
    nptr<RenderEffect> CurEffect {};
    nptr<RenderEffect> DefaultEffect {};
    nptr<RenderEffect> LastEffect {};
};

struct ModelBone
{
    hstring Name {};
    hstring SourceName {};
    mat44 RestLocalTransform {1.0f};
    mat44 TransformationMatrix {};
    mat44 GlobalTransformationMatrix {};
    optional<MeshData> AttachedMesh {};
    vector<unique_ptr<ModelBone>> Children {};
    mat44 CombinedTransformationMatrix {};
};

auto FindModelBone(ptr<ModelBone> bone, hstring bone_name) noexcept -> nptr<ModelBone>;
auto FindModelBone(ptr<const ModelBone> bone, hstring bone_name) noexcept -> nptr<const ModelBone>;

class ModelHierarchy final
{
    friend class ModelManager;
    friend class ModelInstance;
    friend class ModelInformation;

public:
    ModelHierarchy() = delete;
    ModelHierarchy(ptr<ModelManager> model_mngr, string file_name, ptr<ModelBone> root_bone);
    ModelHierarchy(const ModelHierarchy&) = delete;
    ModelHierarchy(ModelHierarchy&&) noexcept = default;
    auto operator=(const ModelHierarchy&) = delete;
    auto operator=(ModelHierarchy&&) noexcept = delete;
    ~ModelHierarchy();

private:
    void SetupBones();
    auto GetTexture(string_view tex_name) -> ptr<const MeshTexture>;
    auto GetEffect(string_view name) -> ptr<RenderEffect>;

    ptr<ModelManager> _modelMngr;
    string _fileName {};
    ptr<ModelBone> _rootBone;
    vector<ModelAnimationRuntimeJoint> _sourceJoints {};
    vector<ptr<ModelBone>> _sourceBones {};
    vector<ptr<ModelBone>> _allBones {};
    vector<ptr<ModelBone>> _allDrawBones {};
    vector<unique_ptr<MeshTexture>> _textures {};
};

FO_END_NAMESPACE

#endif
