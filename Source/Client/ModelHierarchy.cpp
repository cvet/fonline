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

#include "ModelHierarchy.h"

#if FO_ENABLE_3D

#include "Application.h"
#include "EngineBase.h"
#include "ModelAnimation.h"
#include "ModelInformation.h"
#include "ModelInstance.h"
#include "ModelManager.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

ModelHierarchy::ModelHierarchy(ptr<ModelManager> model_mngr, string file_name, ptr<ModelBone> root_bone) :
    _modelMngr {model_mngr},
    _fileName {std::move(file_name)},
    _rootBone {root_bone}
{
    FO_STACK_TRACE_ENTRY();
}

ModelHierarchy::~ModelHierarchy() = default;

static void SetupBonesExt(multimap<uint32_t, ptr<ModelBone>>& bones, vector<ModelAnimationRuntimeJoint>& source_joints, vector<ptr<ModelBone>>& source_bones, ptr<ModelBone> bone, int32_t parent_index, uint32_t depth)
{
    FO_STACK_TRACE_ENTRY();

    const int32_t source_index = numeric_cast<int32_t>(source_joints.size());
    source_joints.emplace_back(ModelAnimationRuntimeJoint {string {bone->SourceName.as_str()}, parent_index, bone->RestLocalTransform});
    source_bones.emplace_back(bone);
    bones.emplace(depth, bone);

    for (auto& child : bone->Children) {
        SetupBonesExt(bones, source_joints, source_bones, child, source_index, depth + 1);
    }
}

void ModelHierarchy::SetupBones()
{
    FO_STACK_TRACE_ENTRY();

    multimap<uint32_t, ptr<ModelBone>> bones;
    vector<ModelAnimationRuntimeJoint> source_joints;
    vector<ptr<ModelBone>> source_bones;
    vector<ptr<ModelBone>> all_bones;
    vector<ptr<ModelBone>> all_draw_bones;
    SetupBonesExt(bones, source_joints, source_bones, _rootBone, -1, 0);

    for (ptr<ModelBone> bone : bones | std::views::values) {
        all_bones.emplace_back(bone);

        if (bone->AttachedMesh) {
            all_draw_bones.emplace_back(bone);
        }
    }

    _sourceJoints = std::move(source_joints);
    _sourceBones = std::move(source_bones);
    _allBones = std::move(all_bones);
    _allDrawBones = std::move(all_draw_bones);
}

auto FindModelBone(ptr<ModelBone> bone, hstring bone_name) noexcept -> nptr<ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    if (bone->Name == bone_name) {
        return bone;
    }

    for (size_t i = 0; i < bone->Children.size(); i++) {
        auto child_bone = FindModelBone(bone->Children[i].as_ptr(), bone_name);

        if (child_bone) {
            return child_bone;
        }
    }

    return nullptr;
}

auto FindModelBone(ptr<const ModelBone> bone, hstring bone_name) noexcept -> nptr<const ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    if (bone->Name == bone_name) {
        return bone;
    }

    for (size_t i = 0; i < bone->Children.size(); i++) {
        const auto child_bone = FindModelBone(ptr<const ModelBone>(bone->Children[i]), bone_name);

        if (child_bone) {
            return child_bone;
        }
    }

    return nullptr;
}

auto ModelHierarchy::GetTexture(string_view tex_name) -> ptr<MeshTexture>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!tex_name.empty(), "Model texture request has an empty texture name", _fileName);

    const string tex_path = strex(_fileName).extract_dir().combine_path(tex_name);
    auto&& [tex, tex_data] = _modelMngr->_textureLoader(tex_path);
    FO_VERIFY_AND_THROW(tex, "Model texture could not be loaded", tex_name, _fileName);

    auto texture = SafeAlloc::MakeUnique<MeshTexture>(_modelMngr->_hashResolver->ToHashedString(tex_name), tex, tex_data);
    _textures.emplace_back(std::move(texture));

    return _textures.back();
}

auto ModelHierarchy::GetEffect(string_view name) -> ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!name.empty(), "Model effect request has an empty effect name", _fileName);
    auto effect = _modelMngr->_effectMngr->LoadEffect(EffectUsage::Model, name);
    FO_VERIFY_AND_THROW(effect, "Model effect could not be loaded", name, _fileName);

    return effect;
}

FO_END_NAMESPACE

#endif
