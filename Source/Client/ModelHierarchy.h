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
    nptr<MeshTexture> CurTexures[MODEL_MAX_TEXTURES] {};
    nptr<MeshTexture> DefaultTexures[MODEL_MAX_TEXTURES] {};
    nptr<MeshTexture> LastTexures[MODEL_MAX_TEXTURES] {};
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
    auto GetTexture(string_view tex_name) -> ptr<MeshTexture>;
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
