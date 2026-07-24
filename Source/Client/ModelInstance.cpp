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

#include "ModelInstance.h"

#if FO_ENABLE_3D

#include "Application.h"
#include "EngineBase.h"
#include "ModelAnimation.h"
#include "ModelHierarchy.h"
#include "ModelInformation.h"
#include "ModelManager.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

static constexpr int32_t SPRITE_BOUNDS_GUARD_PADDING = 2;
// Keep in sync with the default 3D_Skinned shadow pass.
static constexpr float32_t SHADOW_CAMERA_ANGLE_COS = 0.9010770213221f;
static constexpr float32_t SHADOW_CAMERA_ANGLE_SIN = 0.4336590845875f;
static constexpr float32_t SHADOW_ANGLE_TAN = 0.2548968037538f;

ModelInstance::ModelInstance(ptr<ModelManager> model_mngr, ptr<ModelInformation> info) :
    _modelMngr {model_mngr},
    _modelInfo {info}
{
    FO_STACK_TRACE_ENTRY();

    _speedAdjustBase = 1.0f;
    _speedAdjustCur = 1.0f;
    _speedAdjustLink = 1.0f;
    _lookDirAngle = GameSettings::HEXAGONAL_GEOMETRY ? 150.0f : 135.0f;
    _moveDirAngle = _lookDirAngle;
    _targetMoveDirAngle = _moveDirAngle;
    _childChecker = true;
    _matRot = glm::rotate(mat44 {1.0f}, _modelMngr->_settings->MapCameraAngle * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});
    _worldMatrices.assign(_modelInfo->_poseJointRuntimeNames.size(), mat44 {1.0f});

    for (auto& joint_mask : _animationBodyJointMasks) {
        joint_mask.assign(_worldMatrices.size(), 0);
    }
    for (auto& joint_mask : _animationMovementJointMasks) {
        joint_mask.assign(_worldMatrices.size(), 0);
    }

    _forceDraw = true;
    _lastDrawTime = GetTime();
    SetupFrame({4, 4}, {2, 2});
    _frameLayoutDirty = true;
}

ModelInstance::~ModelInstance()
{
    FO_NO_STACK_TRACE_ENTRY();

    InvalidateCombinedMeshes();
    _modelParticles.clear();
}

void ModelInstance::StartMeshGeneration()
{
    FO_STACK_TRACE_ENTRY();

    if (!_allowMeshGeneration) {
        _allowMeshGeneration = true;
        GenerateCombinedMeshes();
    }
}

void ModelInstance::PrewarmParticles()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& model_particle : _modelParticles) {
        model_particle.Particle->Prewarm();
    }
}

void ModelInstance::MoveModel(ipos32 offset)
{
    FO_STACK_TRACE_ENTRY();

    const vec3 pos_zero = Convert2dTo3d({0, 0});
    const vec3 pos = Convert2dTo3d(offset);
    const vec3 diff = pos - pos_zero;

    _moveOffset += diff;
    _forceDraw = true;
}

void ModelInstance::SetAnimData(ModelAnimationData& data, bool clear)
{
    FO_STACK_TRACE_ENTRY();

    // Transformations
    if (clear) {
        _matScaleBase = mat44 {1.0f};
        _matRotBase = mat44 {1.0f};
        _matTransBase = mat44 {1.0f};
    }

    if (data.ScaleX != 0.0f) {
        _matScaleBase *= glm::scale(mat44 {1.0f}, vec3 {data.ScaleX, 1.0f, 1.0f});
    }
    if (data.ScaleY != 0.0f) {
        _matScaleBase *= glm::scale(mat44 {1.0f}, vec3 {1.0f, data.ScaleY, 1.0f});
    }
    if (data.ScaleZ != 0.0f) {
        _matScaleBase *= glm::scale(mat44 {1.0f}, vec3 {1.0f, 1.0f, data.ScaleZ});
    }
    if (data.RotX != 0.0f) {
        _matRotBase *= glm::rotate(mat44 {1.0f}, -data.RotX * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});
    }
    if (data.RotY != 0.0f) {
        _matRotBase *= glm::rotate(mat44 {1.0f}, data.RotY * DEG_TO_RAD_FLOAT, vec3 {0.0f, 1.0f, 0.0f});
    }
    if (data.RotZ != 0.0f) {
        _matRotBase *= glm::rotate(mat44 {1.0f}, data.RotZ * DEG_TO_RAD_FLOAT, vec3 {0.0f, 0.0f, 1.0f});
    }
    if (data.MoveX != 0.0f) {
        _matTransBase *= glm::translate(mat44 {1.0f}, vec3 {data.MoveX, 0.0f, 0.0f});
    }
    if (data.MoveY != 0.0f) {
        _matTransBase *= glm::translate(mat44 {1.0f}, vec3 {0.0f, data.MoveY, 0.0f});
    }
    if (data.MoveZ != 0.0f) {
        _matTransBase *= glm::translate(mat44 {1.0f}, vec3 {0.0f, 0.0f, -data.MoveZ});
    }

    // Speed
    if (clear) {
        _speedAdjustLink = 1.0f;
    }
    if (data.SpeedAjust != 0.0f) {
        _speedAdjustLink *= data.SpeedAjust;
    }

    // Textures
    if (clear) {
        // Enable all meshes, set default texture
        for (size_t mesh_index = 0; mesh_index != _allMeshes.size(); ++mesh_index) {
            auto mesh = _allMeshes[mesh_index].as_ptr();

            mesh->Disabled = false;

            for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
                mesh->LastTexures[i] = mesh->CurTexures[i];
                mesh->CurTexures[i] = mesh->DefaultTexures[i];
            }
        }
    }

    if (!data.TextureInfo.empty()) {
        for (auto&& [tex_name, mesh_name, tex_num] : data.TextureInfo) {
            nptr<MeshTexture> texture = nullptr;
            FO_VERIFY_AND_THROW(tex_num >= 0 && tex_num < numeric_cast<int32_t>(MODEL_MAX_TEXTURES), "Texture index is out of range", tex_num);

            // Evaluate texture
            if (strex(tex_name).starts_with("Parent")) { // Parent_MeshName
                FO_VERIFY_AND_THROW(_parent, "Parent texture was requested without a parent model", tex_name);

                string_view parent_mesh_name = tex_name;
                parent_mesh_name.remove_prefix(6);

                if (!parent_mesh_name.empty() && parent_mesh_name.front() == '_') {
                    parent_mesh_name.remove_prefix(1);
                }

                const auto parent_mesh_name_hashed = !parent_mesh_name.empty() ? _modelMngr->GetBoneHashedString(parent_mesh_name) : hstring();

                for (size_t mesh_index = 0; mesh_index != _parent->_allMeshes.size(); ++mesh_index) {
                    auto mesh = _parent->_allMeshes[mesh_index].as_ptr();

                    if (!parent_mesh_name_hashed || parent_mesh_name_hashed == mesh->Mesh->Owner->Name) {
                        texture = mesh->CurTexures[tex_num];
                        break;
                    }
                }

                FO_VERIFY_AND_THROW(texture, "Parent texture was not found", tex_name);
            }
            else {
                texture = _modelInfo->_hierarchy->GetTexture(tex_name);
            }

            FO_VERIFY_AND_THROW(texture, "Texture was not loaded", tex_name);

            // Assign it
            size_t assigned_meshes = 0;

            for (size_t mesh_index = 0; mesh_index != _allMeshes.size(); ++mesh_index) {
                auto mesh = _allMeshes[mesh_index].as_ptr();

                if (!mesh_name || mesh_name == mesh->Mesh->Owner->Name) {
                    mesh->CurTexures[tex_num] = texture;
                    assigned_meshes++;
                }
            }

            FO_VERIFY_AND_THROW(assigned_meshes != 0, "Texture target mesh was not found", tex_name, mesh_name);
        }
    }

    // Effects
    if (clear) {
        for (size_t mesh_index = 0; mesh_index != _allMeshes.size(); ++mesh_index) {
            auto mesh = _allMeshes[mesh_index].as_ptr();

            mesh->LastEffect = mesh->CurEffect;
            mesh->CurEffect = mesh->DefaultEffect;
        }
    }

    if (!data.EffectInfo.empty()) {
        for (const auto& eff_info : data.EffectInfo) {
            nptr<RenderEffect> effect = nullptr;

            // Get effect
            if (strex(std::get<0>(eff_info)).starts_with("Parent")) { // Parent_MeshName
                FO_VERIFY_AND_THROW(_parent, "Parent effect was requested without a parent model", std::get<0>(eff_info));

                string_view parent_mesh_name = std::get<0>(eff_info);
                parent_mesh_name.remove_prefix(6);

                if (!parent_mesh_name.empty() && parent_mesh_name.front() == '_') {
                    parent_mesh_name.remove_prefix(1);
                }

                const auto mesh_name_hashed = !parent_mesh_name.empty() ? _modelMngr->GetBoneHashedString(parent_mesh_name) : hstring();

                for (size_t mesh_index = 0; mesh_index != _parent->_allMeshes.size(); ++mesh_index) {
                    auto mesh = _parent->_allMeshes[mesh_index].as_ptr();

                    if (!mesh_name_hashed || mesh_name_hashed == mesh->Mesh->Owner->Name) {
                        effect = mesh->CurEffect;
                        break;
                    }
                }

                FO_VERIFY_AND_THROW(effect, "Parent effect was not found", std::get<0>(eff_info));
            }
            else {
                effect = _modelInfo->_hierarchy->GetEffect(std::get<0>(eff_info));
            }
            FO_VERIFY_AND_THROW(effect, "Effect was not loaded", std::get<0>(eff_info));

            // Assign it
            const auto mesh_name = std::get<1>(eff_info);
            size_t assigned_meshes = 0;

            for (size_t mesh_index = 0; mesh_index != _allMeshes.size(); ++mesh_index) {
                auto mesh = _allMeshes[mesh_index].as_ptr();

                if (!mesh_name || mesh_name == mesh->Mesh->Owner->Name) {
                    mesh->CurEffect = effect;
                    assigned_meshes++;
                }
            }

            FO_VERIFY_AND_THROW(assigned_meshes != 0, "Effect target mesh was not found", std::get<0>(eff_info), mesh_name);
        }
    }

    // Cut
    if (clear) {
        _allCuts.clear();
    }
    for (auto& cut_info : data.CutInfo) {
        _allCuts.emplace_back(cut_info);
    }
}

void ModelInstance::SetDir(mdir dir, bool smooth_rotation)
{
    FO_STACK_TRACE_ENTRY();

    SetMoveDir(dir, smooth_rotation);
    SetLookDir(dir);
}

void ModelInstance::SetLookDir(mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    const auto new_angle = numeric_cast<float32_t>(180 - dir.angle());

    if (!_noRotate) {
        if (!is_float_equal(new_angle, _lookDirAngle)) {
            _lookDirAngle = new_angle;
            RefreshMoveAnimation();
        }
    }
    else {
        _deferredLookDirAngle = new_angle;
    }
}

void ModelInstance::SetMoveDir(mdir dir, bool smooth_rotation)
{
    FO_STACK_TRACE_ENTRY();

    const auto new_angle = numeric_cast<float32_t>(180 - dir.angle());

    if (!is_float_equal(new_angle, _targetMoveDirAngle) || (!smooth_rotation && !is_float_equal(new_angle, _moveDirAngle))) {
        _targetMoveDirAngle = new_angle;

        if (!smooth_rotation) {
            _moveDirAngle = _targetMoveDirAngle;
            _turnAnimPlaying = false;
        }

        RefreshMoveAnimation();
    }

    if (!_modelInfo->_rotationBone || _modelInfo->_disableBackwardAnim) {
        SetLookDir(dir);
    }
}

void ModelInstance::SetRotation(float32_t rx, float32_t ry, float32_t rz)
{
    FO_STACK_TRACE_ENTRY();

    const auto mx = glm::rotate(mat44 {1.0f}, rx, vec3 {1.0f, 0.0f, 0.0f});
    const auto my = glm::rotate(mat44 {1.0f}, ry, vec3 {0.0f, 1.0f, 0.0f});
    const auto mz = glm::rotate(mat44 {1.0f}, rz, vec3 {0.0f, 0.0f, 1.0f});

    _matRot = mx * my * mz;
    RefreshFrameLayout();
}

void ModelInstance::SetScale(float32_t sx, float32_t sy, float32_t sz)
{
    FO_STACK_TRACE_ENTRY();

    _matScale = glm::scale(mat44 {1.0f}, vec3 {sx, sy, sz});
    RefreshFrameLayout();
}

void ModelInstance::EnableShadow(bool enabled)
{
    FO_STACK_TRACE_ENTRY();

    const bool shadow_disabled = !enabled;

    if (_shadowDisabled == shadow_disabled) {
        return;
    }

    _shadowDisabled = shadow_disabled;
    RefreshFrameLayout();
    _forceDraw = true;
}

void ModelInstance::SetSpeed(float32_t speed)
{
    FO_STACK_TRACE_ENTRY();

    _speedAdjustBase = speed;
}

auto ModelInstance::FindBone(hstring bone_name) const noexcept -> nptr<const ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    const auto binding = FindPoseJoint(bone_name);
    return binding ? binding->SourceBone : nullptr;
}

auto ModelInstance::FindPoseJoint(hstring bone_name) const noexcept -> optional<PoseJointBinding>
{
    FO_STACK_TRACE_ENTRY();

    if (const auto joint_it = _modelInfo->_poseJointIndexes.find(bone_name); joint_it != _modelInfo->_poseJointIndexes.end()) {
        FO_STRONG_ASSERT(joint_it->second < _modelInfo->_poseBones.size(), "Resolved model pose joint index is outside the physical bone map", _modelInfo->_fileName, joint_it->second, _modelInfo->_poseBones.size());
        return PoseJointBinding {this, joint_it->second, _modelInfo->_poseBones[joint_it->second]};
    }

    for (size_t i = 0; i < _children.size(); i++) {
        const auto child = _children[i].as_ptr();

        if (const auto joint_it = child->_modelInfo->_poseJointIndexes.find(bone_name); joint_it != child->_modelInfo->_poseJointIndexes.end()) {
            FO_STRONG_ASSERT(joint_it->second < child->_modelInfo->_poseBones.size(), "Resolved child model pose joint index is outside the physical bone map", child->_modelInfo->_fileName, joint_it->second, child->_modelInfo->_poseBones.size());
            return PoseJointBinding {child, joint_it->second, child->_modelInfo->_poseBones[joint_it->second]};
        }
    }

    return std::nullopt;
}

void ModelInstance::RunParticle(string_view particle_name, hstring bone_name, vec3 move)
{
    FO_STACK_TRACE_ENTRY();

    if (const auto target_joint = FindPoseJoint(bone_name); target_joint) {
        if (optional<ParticleSystem> particle = _modelMngr->_particleMngr.CreateParticle(particle_name); particle) {
            _modelParticles.emplace_back(ModelParticleSystem {0, SafeAlloc::MakeUnique<ParticleSystem>(std::move(*particle)), target_joint->Owner, target_joint->JointIndex, move, _lookDirAngle});
        }
    }
}

auto ModelInstance::PlayAnim(CritterStateAnim state_anim, CritterActionAnim action_anim, nptr<const int32_t> layers, float32_t ntime, ModelAnimFlags flags) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto prev_state_anim = _curStateAnim;
    const auto prev_action_anim = _curActionAnim;

    _curStateAnim = state_anim;
    _curActionAnim = action_anim;

    // Restore rotation
    if (const auto no_rotate = IsEnumSet(flags, ModelAnimFlags::NoRotate); no_rotate != _noRotate) {
        _noRotate = no_rotate;

        if (_noRotate) {
            _deferredLookDirAngle = _lookDirAngle;
        }
        else if (!is_float_equal(_deferredLookDirAngle, _lookDirAngle)) {
            _lookDirAngle = _deferredLookDirAngle;
            RefreshMoveAnimation();
        }
    }

    // Get animation index
    const auto anim_pair = std::make_pair(state_anim, action_anim);
    float32_t speed = 1.0f;
    int32_t anim_index = 0;

    if (!IsEnumSet(flags, ModelAnimFlags::Init)) {
        anim_index = _modelInfo->GetAnimationIndex(state_anim, action_anim, &speed);
    }

    // Check animation changes
    int32_t new_layers[MODEL_LAYERS_COUNT];

    if (layers) {
        MemCopy(new_layers, layers, sizeof(_curLayers));
    }
    else {
        MemCopy(new_layers, _curLayers, sizeof(_curLayers));
    }

    // Animation layers
    if (const auto it = _modelInfo->_animLayerValues.find(anim_pair); it != _modelInfo->_animLayerValues.end()) {
        for (auto&& [layer_index, value] : it->second) {
            new_layers[layer_index] = value;
        }
    }

    const bool layers_changed = !MemCompare(new_layers, _curLayers, sizeof(new_layers));

    // Try skip redundant calls
    const bool may_skip_redundant = !IsEnumSet(flags, ModelAnimFlags::Init) && !IsEnumSet(flags, ModelAnimFlags::PlayOnce);

    if (may_skip_redundant && prev_state_anim == _curStateAnim && prev_action_anim == _curActionAnim && !layers_changed) {
        return false;
    }

    MemCopy(_curLayers, new_layers, sizeof(_curLayers));

    bool mesh_changed = false;
    vector<hstring> fast_transition_bones;

    if (layers_changed || IsEnumSet(flags, ModelAnimFlags::Init)) {
        // Store data to compare later
        const auto old_cuts = _allCuts;

        for (size_t i = 0; i < _allMeshes.size(); i++) {
            _allMeshesDisabled[i] = _allMeshes[i]->Disabled;
        }

        // Set anim data
        SetAnimData(_modelInfo->_animDataDefault, true);

        if (_parent) {
            SetAnimData(_animLink, false);
        }

        // Mark animations as unused
        for (size_t i = 0; i != _children.size(); ++i) {
            _children[i]->_childChecker = false;
        }

        // Get unused layers and meshes
        bool unused_layers[MODEL_LAYERS_COUNT] = {};

        for (int32_t i = 0; i < numeric_cast<int32_t>(MODEL_LAYERS_COUNT); i++) {
            if (new_layers[i] == 0) {
                continue;
            }

            for (const auto& link : _modelInfo->_animData) {
                if (link.Layer == i && link.LayerValue == new_layers[i] && link.ChildName.empty()) {
                    for (const auto j : link.DisabledLayer) {
                        unused_layers[j] = true;
                    }
                    for (const auto disabled_mesh_name : link.DisabledMesh) {
                        for (size_t mesh_index = 0; mesh_index != _allMeshes.size(); ++mesh_index) {
                            auto mesh = _allMeshes[mesh_index].as_ptr();

                            if (!disabled_mesh_name || disabled_mesh_name == mesh->Mesh->Owner->Name) {
                                mesh->Disabled = true;
                            }
                        }
                    }
                }
            }
        }

        if (_parent) {
            for (const auto j : _animLink.DisabledLayer) {
                unused_layers[j] = true;
            }
            for (auto disabled_mesh_name : _animLink.DisabledMesh) {
                for (size_t mesh_index = 0; mesh_index != _allMeshes.size(); ++mesh_index) {
                    auto mesh = _allMeshes[mesh_index].as_ptr();

                    if (!disabled_mesh_name || disabled_mesh_name == mesh->Mesh->Owner->Name) {
                        mesh->Disabled = true;
                    }
                }
            }
        }

        // Append animations
        set<uint32_t> keep_alive_particles;

        for (int32_t i = 0; i < numeric_cast<int32_t>(MODEL_LAYERS_COUNT); i++) {
            if (unused_layers[i] || new_layers[i] == 0) {
                continue;
            }

            for (auto& link : _modelInfo->_animData) {
                if (link.Layer == i && link.LayerValue == new_layers[i]) {
                    if (link.ChildName.empty()) {
                        SetAnimData(link, false);
                        continue;
                    }

                    if (link.IsParticles) {
                        bool available = false;

                        for (const auto& model_particle : _modelParticles) {
                            if (model_particle.Id == link.Id) {
                                available = true;
                                break;
                            }
                        }

                        if (!available) {
                            FO_VERIFY_AND_THROW(link.LinkBone, "Particle model link has no target bone");
                            const auto target_joint = FindPoseJoint(link.LinkBone);
                            FO_VERIFY_AND_THROW(target_joint, "Particle model link target bone not found");

                            optional<ParticleSystem> particle = _modelMngr->_particleMngr.CreateParticle(link.ChildName);
                            FO_VERIFY_AND_THROW(particle, "Particle was not found for a model link", link.ChildName);
                            _modelParticles.emplace_back(ModelParticleSystem {link.Id, SafeAlloc::MakeUnique<ParticleSystem>(std::move(*particle)), target_joint->Owner, target_joint->JointIndex, vec3(link.MoveX, link.MoveY, link.MoveZ), link.RotY});
                        }

                        keep_alive_particles.insert(link.Id);
                    }
                    else {
                        bool available = false;

                        for (size_t child_index = 0; child_index != _children.size(); ++child_index) {
                            auto child = _children[child_index].as_ptr();

                            if (child->_animLink.Id == link.Id) {
                                child->_childChecker = true;
                                available = true;
                                break;
                            }
                        }

                        if (!available) {
                            const auto create_child_model = [this, &link]() -> unique_ptr<ModelInstance> {
                                auto model = _modelMngr->CreateModel(link.ChildName);
                                FO_VERIFY_AND_THROW(model, "Child model was not found for a model link", link.ChildName);
                                return model.take_not_null();
                            };

                            // Link to main bone
                            if (link.LinkBone) {
                                const auto target_joint = _modelInfo->_poseJointIndexes.find(link.LinkBone);
                                FO_VERIFY_AND_THROW(target_joint != _modelInfo->_poseJointIndexes.end(), "Model link target joint not found", link.LinkBone, _modelInfo->_fileName);

                                auto model = create_child_model();

                                mesh_changed = true;
                                model->_parent = this;
                                model->_parentJointIndex = target_joint->second;
                                model->_animLink = link;
                                model->SetAnimData(link, false);
                                _children.emplace_back(std::move(model));

                                if (_modelInfo->_fastTransitionBones.count(link.LinkBone) != 0) {
                                    fast_transition_bones.emplace_back(link.LinkBone);
                                }
                            }
                            // Link all bones
                            else {
                                auto model = create_child_model();
                                model->_linkJoints = ResolveModelPoseJointLinks(_modelInfo->_poseJointIndexes, model->_modelInfo->_poseJointRuntimeNames);

                                FO_VERIFY_AND_THROW(!model->_linkJoints.empty(), "Child model has no common bones for a model link", link.ChildName);

                                mesh_changed = true;
                                model->_parent = this;
                                model->_parentJointIndex = GetPoseJointIndex(_modelInfo->_hierarchy->_rootBone);
                                model->_animLink = link;
                                model->SetAnimData(link, false);
                                _children.emplace_back(std::move(model));
                            }
                        }
                    }
                }
            }
        }

        // Erase unused stuff
        for (auto it = _children.begin(); it != _children.end();) {
            auto child = it->as_ptr();

            if (!child->_childChecker) {
                mesh_changed = true;
                InvalidateCombinedMeshes();

                std::erase_if(_modelParticles, [child](const ModelParticleSystem& particle) { return particle.Owner == child; });
                it = _children.erase(it);
            }
            else {
                ++it;
            }
        }

        for (auto it = _modelParticles.begin(); it != _modelParticles.end();) {
            if (it->Id != 0 && keep_alive_particles.count(it->Id) == 0) {
                it = _modelParticles.erase(it);
            }
            else {
                ++it;
            }
        }

        // Check for mesh changes
        if (!mesh_changed) {
            for (size_t i = 0; i < _allMeshes.size() && !mesh_changed; i++) {
                mesh_changed = _allMeshes[i]->LastEffect != _allMeshes[i]->CurEffect;
            }
        }
        if (!mesh_changed) {
            for (size_t i = 0; i < _allMeshes.size() && !mesh_changed; i++) {
                for (size_t k = 0; k < MODEL_MAX_TEXTURES && !mesh_changed; k++) {
                    mesh_changed = _allMeshes[i]->LastTexures[k] != _allMeshes[i]->CurTexures[k];
                }
            }
        }
        if (!mesh_changed) {
            for (size_t i = 0; i < _allMeshes.size() && !mesh_changed; i++) {
                mesh_changed = _allMeshesDisabled[i] != _allMeshes[i]->Disabled;
            }
        }

        // Affect cut
        if (!mesh_changed) {
            mesh_changed = _allCuts != old_cuts;
        }
    }

    if (_bodyAnimController && anim_index >= 0) {
        _playOnceAnimPlaying = IsEnumSet(flags, ModelAnimFlags::PlayOnce);
    }

    RefreshMoveAnimation();

    if (_bodyAnimController && anim_index >= 0) {
        const auto new_track = _curTrack == 0 ? 1 : 0;
        _animDuration = _bodyAnimController->GetAnimDuration(anim_index);

        // Turn off fast transition bones on other tracks
        if (!fast_transition_bones.empty()) {
            _bodyAnimController->ResetBonesTransition(new_track, fast_transition_bones);
        }

        _bodyAnimController->ResetEvents();

        const auto no_smooth = IsEnumSet(flags, ModelAnimFlags::NoSmooth) || IsEnumSet(flags, ModelAnimFlags::Freeze) || IsEnumSet(flags, ModelAnimFlags::Init);
        const auto smooth_time = no_smooth ? 0.0f : _modelMngr->_moveTransitionTime;
        const auto anim_start_time = std::min(_animDuration * ntime, _animDuration - 0.001f);
        const auto anim_duration = IsEnumSet(flags, ModelAnimFlags::Freeze) || IsEnumSet(flags, ModelAnimFlags::Init) ? 0.0f : _animDuration - anim_start_time;

        // Disable current track
        if (no_smooth) {
            _bodyAnimController->SetTrackEnable(_curTrack, false);
        }
        else {
            _bodyAnimController->AddEventEnable(_curTrack, false, smooth_time);
            _bodyAnimController->AddEventSpeed(_curTrack, 0.0f, 0.0f, smooth_time);
            _bodyAnimController->AddEventWeight(_curTrack, 0.0f, 0.0f, smooth_time);
        }

        // Enable the new track
        _curTrack = new_track;
        _speedAdjustCur = speed;

        _bodyAnimController->SetTrackEnable(new_track, true);
        _bodyAnimController->SetTrackAnimation(new_track, anim_index, nullptr);
        _bodyAnimController->SetTrackPosition(new_track, anim_start_time);
        _bodyAnimController->AddEventSpeed(new_track, 1.0f, 0.0f, 0.0f);

        if (IsEnumSet(flags, ModelAnimFlags::PlayOnce) || IsEnumSet(flags, ModelAnimFlags::Freeze) || IsEnumSet(flags, ModelAnimFlags::Init)) {
            _bodyAnimController->AddEventSpeed(new_track, 0.0f, anim_duration, 0.0f);
        }

        _bodyAnimController->AddEventWeight(new_track, 1.0f, 0.0f, smooth_time);
        _bodyAnimController->AdvanceTimeline(0.0f);

        if ((_isMoving || _turnAnimPlaying) && _moveAnimController) {
            _moveAnimController->AdvanceTimeline(0.0f);
        }

        // Force redraw
        _forceDraw = true;
    }

    // Set animation for children
    for (size_t i = 0; i != _children.size(); ++i) {
        if (_children[i]->PlayAnim(state_anim, action_anim, layers, ntime, flags)) {
            mesh_changed = true;
        }
    }

    // Regenerate mesh for drawing
    if (!_parent && mesh_changed) {
        GenerateCombinedMeshes();
    }

    if (!_parent) {
        RefreshFrameLayout();
    }

    return mesh_changed;
}

void ModelInstance::UpdatePose(bool staying_pose, bool moving, int32_t moving_speed)
{
    FO_STACK_TRACE_ENTRY();

    _isStayingPose = staying_pose;
    _isMoving = staying_pose && moving;

    if (_isMoving) {
        if (moving_speed < _modelMngr->_settings->RunAnimStartSpeed) {
            _isRunning = false;
            _movingSpeedFactor = numeric_cast<float32_t>(moving_speed) / numeric_cast<float32_t>(_modelMngr->_settings->WalkAnimBaseSpeed);
        }
        else {
            _isRunning = true;
            _movingSpeedFactor = numeric_cast<float32_t>(moving_speed) / numeric_cast<float32_t>(_modelMngr->_settings->RunAnimBaseSpeed);
        }
    }

    if (!_isStayingPose) {
        if (_curMovingAnimIndex != -1) {
            _moveAnimController->ResetEvents();
            _moveAnimController->SetTrackEnable(_curMoveTrack, false);
            _curMovingAnim = CritterActionAnim::None;
            _curMovingAnimIndex = -1;
        }

        _turnAnimPlaying = false;
    }

    RefreshMoveAnimation();
}

void ModelInstance::RefreshMoveAnimation()
{
    FO_STACK_TRACE_ENTRY();

    if (!_moveAnimController) {
        return;
    }
    if (!_isStayingPose) {
        return;
    }

    // A one-shot body animation owns the whole skeleton while the critter
    // stands still: release the movement-layer leg override (idle/turn on
    // LegBones) until it finishes, otherwise full-body clips play only above
    // the waist. Movement keeps the leg layer as usual.
    if (!_isMoving && _playOnceAnimPlaying) {
        if (_curMovingAnimIndex != -1) {
            _moveAnimController->ResetEvents();
            _moveAnimController->SetTrackEnable(_curMoveTrack, false);
            _curMovingAnim = CritterActionAnim::None;
            _curMovingAnimIndex = -1;
        }

        _turnAnimPlaying = false;
        return;
    }

    auto state_anim = CritterStateAnim::None;
    auto action_anim = CritterActionAnim::Idle;

    if (_isMoving) {
        const auto angle_diff = GeometryHelper::GetDirAngleDiff(_targetMoveDirAngle, _lookDirAngle);
        const bool forbid_back = _modelInfo->_disableBackwardAnim;

        if (forbid_back || (!_isMovingBack && angle_diff <= 95.0f) || (_isMovingBack && angle_diff <= 85.0f)) {
            _isMovingBack = false;
            action_anim = _isRunning ? CritterActionAnim::Run : CritterActionAnim::Walk;
        }
        else {
            _isMovingBack = true;
            action_anim = _isRunning ? CritterActionAnim::RunBack : CritterActionAnim::WalkBack;
        }

        _turnAnimPlaying = false;
    }
    else {
        if (_isMovingBack) {
            _moveDirAngle = _targetMoveDirAngle = _lookDirAngle;
        }

        state_anim = _curStateAnim;
        _isMovingBack = false;

        const auto angle_diff = GeometryHelper::GetDirAngleDiffSided(_targetMoveDirAngle, _lookDirAngle);

        if (std::abs(angle_diff) > _modelMngr->_settings->CritterTurnAngle) {
            _targetMoveDirAngle = _lookDirAngle;

            if (_turnAnimPlaying) {
                return;
            }

            action_anim = angle_diff < 0.0f ? CritterActionAnim::TurnRight : CritterActionAnim::TurnLeft;
            _turnAnimPlaying = true;
        }
        else if (_turnAnimPlaying) {
            return;
        }
    }

    if (_animInitCallback) {
        _animInitCallback(state_anim, action_anim);
    }

    _curMovingAnim = action_anim;

    float32_t speed = 1.0f;
    const auto anim_index = _modelInfo->GetAnimationIndex(state_anim, action_anim, &speed);

    if (_isMoving) {
        speed *= _movingSpeedFactor;
    }

    if (anim_index == _curMovingAnimIndex) {
        if (_isMoving && !is_float_equal(_moveAnimController->GetTrackSpeed(_curMoveTrack), speed)) {
            _moveAnimController->SetTrackSpeed(_curMoveTrack, speed);
            _forceDraw = true;
        }

        return;
    }

    _curMovingAnimIndex = anim_index;
    _frameLayoutDirty = true;
    _forceDraw = true;

    constexpr float32_t smooth_time = 0.001f;

    if (anim_index != -1) {
        const auto new_track = _curMoveTrack == 0 ? 1 : 0;

        _moveAnimController->ResetEvents();

        _moveAnimController->AddEventEnable(_curMoveTrack, false, smooth_time);
        _moveAnimController->AddEventSpeed(_curMoveTrack, 0.0f, 0.0f, smooth_time);
        _moveAnimController->AddEventWeight(_curMoveTrack, 0.0f, 0.0f, smooth_time);

        _moveAnimController->SetTrackEnable(new_track, true);
        _moveAnimController->SetTrackAnimation(new_track, anim_index, &_modelMngr->_legBones);
        _moveAnimController->SetTrackPosition(new_track, 0.0f);

        _moveAnimController->AddEventSpeed(new_track, speed, 0.0f, smooth_time);
        _moveAnimController->AddEventWeight(new_track, 1.0f, 0.0f, smooth_time);

        if (_turnAnimPlaying) {
            const auto anim_duration = _moveAnimController->GetAnimDuration(anim_index);
            _moveAnimController->AddEventEnable(new_track, false, anim_duration / speed);
        }

        _curMoveTrack = new_track;
    }
    else {
        _moveAnimController->ResetEvents();

        _moveAnimController->AddEventEnable(_curMoveTrack, false, smooth_time);
        _moveAnimController->AddEventSpeed(_curMoveTrack, 0.0f, 0.0f, smooth_time);
        _moveAnimController->AddEventWeight(_curMoveTrack, 0.0f, 0.0f, smooth_time);
    }
}

void ModelInstance::SetAnimInitCallback(function<void(CritterStateAnim&, CritterActionAnim&)> anim_init)
{
    FO_STACK_TRACE_ENTRY();

    _animInitCallback = std::move(anim_init);
}

void ModelInstance::AddAnimationCallback(ModelAnimationCallback callback)
{
    FO_STACK_TRACE_ENTRY();

    _animationCallbacks.emplace_back(std::move(callback));
}

void ModelInstance::SetAnimationCallbacks(vector<ModelAnimationCallback> callbacks)
{
    FO_STACK_TRACE_ENTRY();

    _animationCallbacks = std::move(callbacks);
}

auto ModelInstance::TakeAnimationCallbacks() -> vector<ModelAnimationCallback>
{
    FO_STACK_TRACE_ENTRY();

    return std::move(_animationCallbacks);
}

void ModelInstance::ClearAnimationCallbacks()
{
    FO_STACK_TRACE_ENTRY();

    _animationCallbacks.clear();
    RequestRedraw();
}

auto ModelInstance::HasAnimation(CritterStateAnim state_anim, CritterActionAnim action_anim) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto index = std::make_pair(state_anim, action_anim);
    const auto it = _modelInfo->_animIndexes.find(index);

    return it != _modelInfo->_animIndexes.end();
}

auto ModelInstance::ResolveAnimation(CritterStateAnim& state_anim, CritterActionAnim& action_anim) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _modelInfo->GetAnimationIndex(state_anim, action_anim, nullptr) != -1;
}

auto ModelInstance::GetMovingAnim() const noexcept -> CritterActionAnim
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_curMovingAnimIndex != -1) {
        return _curMovingAnim;
    }
    else {
        return _isRunning ? CritterActionAnim::Run : CritterActionAnim::Walk;
    }
}

auto ModelInstance::IsAnimationPlaying() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_bodyAnimController) {
        const auto track0_playing = _bodyAnimController->GetTrackEnable(0) && _bodyAnimController->GetTrackSpeed(0) > 0.0f;
        const auto track1_playing = _bodyAnimController->GetTrackEnable(1) && _bodyAnimController->GetTrackSpeed(1) > 0.0f;
        return track0_playing || track1_playing;
    }
    else {
        return false;
    }
}

auto ModelInstance::GetDrawSize() const -> isize32
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_frameSize.width % FRAME_SCALE == 0, "3D model frame width is not aligned to the frame scale", _frameSize.width, FRAME_SCALE);
    FO_VERIFY_AND_THROW(_frameSize.height % FRAME_SCALE == 0, "3D model frame height is not aligned to the frame scale", _frameSize.height, FRAME_SCALE);

    return {_frameSize.width / FRAME_SCALE, _frameSize.height / FRAME_SCALE};
}

auto ModelInstance::GetSpriteBounds() const -> optional<ModelSpriteBounds>
{
    FO_STACK_TRACE_ENTRY();

    if (_frameSize.width <= 0 || _frameSize.height <= 0 || _frameSize.width % FRAME_SCALE != 0 || _frameSize.height % FRAME_SCALE != 0) {
        return std::nullopt;
    }

    bool force_full_frame = _modelMngr->_effectMngr->Effects.SkinnedModel != _modelMngr->_effectMngr->Effects.SkinnedModelDefault;
    const int32_t frame_width = _frameSize.width / FRAME_SCALE;
    const int32_t frame_height = _frameSize.height / FRAME_SCALE;
    const ipos32 root_pos = _framePivot;
    const mat44 root_transformation = _spriteBoundsPoseReady ? _parentMatrix : MakeRootTransformation(root_pos, const_numeric_cast<float32_t>(FRAME_SCALE), false);
    const vec3 ground_pos = _spriteBoundsPoseReady ? _groundPos : vec3 {root_transformation[3][0], root_transformation[3][1], root_transformation[3][2]};
    const bool include_shadow = !_shadowDisabled && !_modelInfo->_shadowDisabled;

    if (include_shadow && (!std::isfinite(ground_pos.x) || !std::isfinite(ground_pos.y) || !std::isfinite(ground_pos.z))) {
        return std::nullopt;
    }

    const int32_t viewport[4] = {0, 0, _frameSize.width, _frameSize.height};
    const mat44 identity {1.0f};
    const float32_t frame_scale = const_numeric_cast<float32_t>(FRAME_SCALE);
    bool has_projected_point = false;
    float32_t min_x {};
    float32_t min_y {};
    float32_t max_x {};
    float32_t max_y {};

    const auto include_projected_point = [&](vec3 world_pos) -> bool {
        vec3 projected_pos {};
        if (!ProjectPoint(world_pos, identity, _frameProj, viewport, projected_pos) || !std::isfinite(projected_pos.x) || !std::isfinite(projected_pos.y)) {
            return false;
        }

        const float32_t sprite_x = projected_pos.x / frame_scale;
        const float32_t sprite_y = (numeric_cast<float32_t>(_frameSize.height) - projected_pos.y) / frame_scale;

        if (!has_projected_point) {
            min_x = max_x = sprite_x;
            min_y = max_y = sprite_y;
            has_projected_point = true;
        }
        else {
            min_x = std::min(min_x, sprite_x);
            min_y = std::min(min_y, sprite_y);
            max_x = std::max(max_x, sprite_x);
            max_y = std::max(max_y, sprite_y);
        }

        return true;
    };
    const auto include_world_point = [&](vec3 world_pos) -> bool {
        if (!include_projected_point(world_pos)) {
            return false;
        }

        if (include_shadow) {
            auto shadow_pos = world_pos;
            auto shadow_distance = (shadow_pos.y - ground_pos.y) * SHADOW_CAMERA_ANGLE_COS;
            shadow_distance -= (ground_pos.z - shadow_pos.z) * SHADOW_CAMERA_ANGLE_SIN;
            shadow_pos.y -= shadow_distance * SHADOW_CAMERA_ANGLE_COS;
            shadow_distance *= SHADOW_ANGLE_TAN;
            shadow_pos.y += shadow_distance * SHADOW_CAMERA_ANGLE_SIN;
            shadow_pos.z -= 10.0f;

            if (!include_projected_point(shadow_pos)) {
                return false;
            }
        }

        return true;
    };

    // All-facings frame envelope. The model turns to face any hex direction at draw time, but its sprite frame must
    // stay fixed while it turns. Rotating a world point about the model's facing axis maps it to the same point at
    // another facing, so projecting each mesh vertex at the current facing plus +90/+180/+270 and keeping the union
    // yields a frame that already covers every direction - attached gear (a backpack, a weapon) that only widens the
    // silhouette at some facings no longer resizes the frame on a turn. Runtime layer/equipment meshes are not in the
    // baked animation bounds, so this is what keeps their contribution direction-independent.
    const mat44 facing_prefix = glm::translate(mat44 {1.0f}, Convert2dTo3d(_framePivot)) * _matTransBase * _matRot;
    const mat44 facing_prefix_inverse = glm::inverse(facing_prefix);
    const auto facing_delta_matrix = [&](float32_t degrees) -> mat44 {
        return facing_prefix * glm::rotate(mat44 {1.0f}, degrees * DEG_TO_RAD_FLOAT, vec3 {0.0f, 1.0f, 0.0f}) * facing_prefix_inverse;
    };
    const mat44 facing_rotation_90 = facing_delta_matrix(90.0f);
    const mat44 facing_rotation_180 = facing_delta_matrix(180.0f);
    // A point's projected coordinate traces a sinusoid as the model turns; sampling it at facing, +90 and +180 and
    // taking the harmonic range yields its continuous min/max over every facing, independent of which facing the
    // model currently holds (four discrete samples would give a facing-dependent union). This mirrors the layout's
    // own direction sweep, applied here to the actual runtime geometry.
    const auto harmonic_range = [](float32_t value_0, float32_t value_90, float32_t value_180) -> pair<float32_t, float32_t> {
        const float64_t center = (numeric_cast<float64_t>(value_0) + numeric_cast<float64_t>(value_180)) * 0.5;
        const float64_t cosine = (numeric_cast<float64_t>(value_0) - numeric_cast<float64_t>(value_180)) * 0.5;
        const float64_t sine = numeric_cast<float64_t>(value_90) - center;
        const float64_t radius = std::hypot(cosine, sine);
        return {numeric_cast<float32_t>(center - radius), numeric_cast<float32_t>(center + radius)};
    };
    bool has_all_facings_point = false;
    float32_t all_facings_min_x {};
    float32_t all_facings_min_y {};
    float32_t all_facings_max_x {};
    float32_t all_facings_max_y {};
    const auto include_mesh_all_facings = [&](vec3 world_pos) {
        vec3 projected_0 {};
        vec3 projected_90 {};
        vec3 projected_180 {};

        if (!ProjectPoint(world_pos, identity, _frameProj, viewport, projected_0) || //
            !ProjectPoint(vec3 {facing_rotation_90 * glm::vec4 {world_pos, 1.0f}}, identity, _frameProj, viewport, projected_90) || //
            !ProjectPoint(vec3 {facing_rotation_180 * glm::vec4 {world_pos, 1.0f}}, identity, _frameProj, viewport, projected_180)) {
            return;
        }
        if (!std::isfinite(projected_0.x) || !std::isfinite(projected_0.y) || !std::isfinite(projected_90.x) || !std::isfinite(projected_90.y) || !std::isfinite(projected_180.x) || !std::isfinite(projected_180.y)) {
            return;
        }

        const float32_t frame_height_sprite = numeric_cast<float32_t>(_frameSize.height);
        const auto [x_min, x_max] = harmonic_range(projected_0.x / frame_scale, projected_90.x / frame_scale, projected_180.x / frame_scale);
        const auto [y_min, y_max] = harmonic_range((frame_height_sprite - projected_0.y) / frame_scale, (frame_height_sprite - projected_90.y) / frame_scale, (frame_height_sprite - projected_180.y) / frame_scale);

        if (!has_all_facings_point) {
            all_facings_min_x = x_min;
            all_facings_max_x = x_max;
            all_facings_min_y = y_min;
            all_facings_max_y = y_max;
            has_all_facings_point = true;
        }
        else {
            all_facings_min_x = std::min(all_facings_min_x, x_min);
            all_facings_max_x = std::max(all_facings_max_x, x_max);
            all_facings_min_y = std::min(all_facings_min_y, y_min);
            all_facings_max_y = std::max(all_facings_max_y, y_max);
        }
    };

    bool has_geometry = false;

    if (_spriteBoundsPoseReady) {
        for (size_t mesh_index = 0; mesh_index < _actualCombinedMeshesCount; mesh_index++) {
            const auto combined_mesh = _combinedMeshes[mesh_index].as_ptr();

            if (!combined_mesh->SpriteBoundsValid) {
                return std::nullopt;
            }
            if (!combined_mesh->HasSpriteGeometry) {
                continue;
            }

            force_full_frame = force_full_frame || combined_mesh->DrawEffect;
            has_geometry = true;
            array<mat44, MODEL_MAX_BONES> skin_matrices {};

            for (size_t bone_index = 0; bone_index < combined_mesh->CurBoneMatrix; bone_index++) {
                const SkinBinding& binding = combined_mesh->SkinBindings[bone_index];

                if (!binding.Owner || !binding.SourceBone) {
                    return std::nullopt;
                }

                skin_matrices[bone_index] = binding.Owner->GetWorldMatrix(binding.JointIndex) * binding.InverseBindMatrix;
            }

            for (const vindex_t vertex_index : combined_mesh->SpriteVertices) {
                if (numeric_cast<size_t>(vertex_index) >= combined_mesh->MeshBuf->Vertices3D.size()) {
                    return std::nullopt;
                }

                const Vertex3D& vertex = combined_mesh->MeshBuf->Vertices3D[vertex_index];
                glm::vec4 transformed_pos {};

                for (size_t influence = 0; influence < MODEL_BONES_PER_VERTEX; influence++) {
                    const float32_t weight = vertex.BlendWeights[influence];

                    if (weight <= 0.0f) {
                        continue;
                    }

                    const size_t bone_index = numeric_cast<size_t>(iround<int32_t>(vertex.BlendIndices[influence]));

                    if (bone_index >= combined_mesh->CurBoneMatrix) {
                        return std::nullopt;
                    }

                    transformed_pos += skin_matrices[bone_index] * glm::vec4 {vertex.Position, 1.0f} * weight;
                }

                if (!std::isfinite(transformed_pos.x) || !std::isfinite(transformed_pos.y) || !std::isfinite(transformed_pos.z) || !is_float_equal(transformed_pos.w, 1.0f) || !include_world_point(vec3 {transformed_pos})) {
                    return std::nullopt;
                }

                include_mesh_all_facings(vec3 {transformed_pos});
            }
        }
    }

    const auto include_track_bounds = [&](const ModelAnimationController& controller) -> bool {
        for (int32_t track = 0; track < 2; track++) {
            const ModelAnimationController::TrackState state = controller.GetTrackState(track);

            if (!state.Enabled) {
                continue;
            }

            const bool has_baked_bounds = state.ClipIndex >= 0 && numeric_cast<size_t>(state.ClipIndex) < _modelInfo->_animationBounds.size() && _modelInfo->_animationBounds[numeric_cast<size_t>(state.ClipIndex)].has_value();

            if (!has_baked_bounds) {
                if (!_spriteBoundsPoseReady) {
                    return false;
                }

                continue;
            }

            const ModelBounds3D& bounds = *_modelInfo->_animationBounds[numeric_cast<size_t>(state.ClipIndex)];
            has_geometry = true;

            for (uint32_t corner_index = 0; corner_index < 8; corner_index++) {
                const vec3 root_pos_3d {
                    (corner_index & 1U) != 0 ? bounds.Max.x : bounds.Min.x,
                    (corner_index & 2U) != 0 ? bounds.Max.y : bounds.Min.y,
                    (corner_index & 4U) != 0 ? bounds.Max.z : bounds.Min.z,
                };
                const glm::vec4 transformed_pos = root_transformation * glm::vec4 {root_pos_3d, 1.0f};

                if (!std::isfinite(transformed_pos.x) || !std::isfinite(transformed_pos.y) || !std::isfinite(transformed_pos.z) || !is_float_equal(transformed_pos.w, 1.0f) || !include_world_point(vec3 {transformed_pos})) {
                    return false;
                }
            }
        }

        return true;
    };

    if ((_bodyAnimController && !include_track_bounds(*_bodyAnimController)) || (_moveAnimController && !include_track_bounds(*_moveAnimController))) {
        return std::nullopt;
    }

    const auto include_particle_bounds = [&](ptr<const ModelInstance> model, const auto& recurse) -> bool {
        for (const auto& model_particle : model->_modelParticles) {
            // A particle only affects the sprite frame while it is actually emitting. A dormant system reserves no
            // space, so an idle effect (e.g. furnace smoke that is not currently puffing) does not inflate the
            // frame; when it starts emitting, the render's frame-expansion pass grows the frame to fit the live
            // particles. Reserving the full particle sprite for a non-emitting system would bloat every model that
            // carries an occasional effect.
            const optional<ParticleBounds3D> live_bounds = model_particle.Particle->GetLiveBounds();

            if (!live_bounds) {
                continue;
            }

            for (uint32_t corner_index = 0; corner_index < 8; corner_index++) {
                const vec3 corner {
                    (corner_index & 1U) != 0 ? live_bounds->Max.x : live_bounds->Min.x,
                    (corner_index & 2U) != 0 ? live_bounds->Max.y : live_bounds->Min.y,
                    (corner_index & 4U) != 0 ? live_bounds->Max.z : live_bounds->Min.z,
                };

                if (!include_projected_point(corner)) {
                    return false;
                }

                // Keep the emitting effect (e.g. furnace smoke) inside the frame at every facing, not just the
                // current one, so a turn does not clip it - same all-facings envelope the mesh geometry uses.
                include_mesh_all_facings(corner);
            }

            has_geometry = true;
            force_full_frame = true;
        }

        for (const auto& child : model->_children) {
            if (!recurse(child.as_ptr(), recurse)) {
                return false;
            }
        }

        return true;
    };

    if (!include_particle_bounds(this, include_particle_bounds) || !has_geometry || !has_projected_point) {
        return std::nullopt;
    }

    const float32_t frame_width_float = numeric_cast<float32_t>(frame_width);
    const float32_t frame_height_float = numeric_cast<float32_t>(frame_height);
    const float32_t guard_padding = const_numeric_cast<float32_t>(SPRITE_BOUNDS_GUARD_PADDING);

    // The frame must contain the current-facing crop (min_x..max_x, which also carries the shadow and any live
    // particles) and the mesh geometry across all facings, so it stays a fixed size while the critter turns.
    float32_t frame_min_x = min_x;
    float32_t frame_min_y = min_y;
    float32_t frame_max_x = max_x;
    float32_t frame_max_y = max_y;

    if (has_all_facings_point) {
        frame_min_x = std::min(frame_min_x, all_facings_min_x);
        frame_min_y = std::min(frame_min_y, all_facings_min_y);
        frame_max_x = std::max(frame_max_x, all_facings_max_x);
        frame_max_y = std::max(frame_max_y, all_facings_max_y);
    }

    optional<isize32> required_frame_size = CalculateModelSpriteFrameSize(frame_min_x - numeric_cast<float32_t>(root_pos.x) - guard_padding, frame_min_y - numeric_cast<float32_t>(root_pos.y) - guard_padding, frame_max_x - numeric_cast<float32_t>(root_pos.x) + guard_padding, frame_max_y - numeric_cast<float32_t>(root_pos.y) + guard_padding);

    if (!required_frame_size) {
        return std::nullopt;
    }

    required_frame_size->width = std::max(required_frame_size->width, _layoutDrawSize.width);
    required_frame_size->height = std::max(required_frame_size->height, _layoutDrawSize.height);

    // Where the model origin sits inside a frame grown to RequiredFrameSize: its offset from the all-facings
    // envelope's top-left (the required frame's top-left), so a frame expansion keeps the origin anchored.
    const int32_t required_pivot_x = force_full_frame ? root_pos.x : root_pos.x - iround<int32_t>(std::floor(frame_min_x) - guard_padding);
    const int32_t required_pivot_y = force_full_frame ? root_pos.y : root_pos.y - iround<int32_t>(std::floor(frame_min_y) - guard_padding);
    const ipos32 required_pivot = {std::clamp(required_pivot_x, 0, required_frame_size->width), std::clamp(required_pivot_y, 0, required_frame_size->height)};

    const int32_t left = force_full_frame ? 0 : iround<int32_t>(std::clamp(std::floor(min_x) - guard_padding, 0.0f, frame_width_float));
    const int32_t top = force_full_frame ? 0 : iround<int32_t>(std::clamp(std::floor(min_y) - guard_padding, 0.0f, frame_height_float));
    const int32_t right = force_full_frame ? frame_width : iround<int32_t>(std::clamp(std::ceil(max_x) + guard_padding, 0.0f, frame_width_float));
    const int32_t bottom = force_full_frame ? frame_height : iround<int32_t>(std::clamp(std::ceil(max_y) + guard_padding, 0.0f, frame_height_float));

    if (right <= left || bottom <= top) {
        return std::nullopt;
    }

    const auto collect_enabled_clip_indices = [](const optional<ModelAnimationController>& controller) -> pair<array<int32_t, 2>, uint8_t> {
        array<int32_t, 2> clip_indices {-1, -1};
        uint8_t clip_count = 0;

        if (controller) {
            for (int32_t track = 0; track < 2; track++) {
                const ModelAnimationController::TrackState state = controller->GetTrackState(track);

                if (!state.Enabled || (clip_count != 0 && clip_indices[0] == state.ClipIndex)) {
                    continue;
                }

                clip_indices[clip_count++] = state.ClipIndex;
            }
        }

        if (clip_count == 2 && clip_indices[1] < clip_indices[0]) {
            std::swap(clip_indices[0], clip_indices[1]);
        }

        return {clip_indices, clip_count};
    };

    const auto [body_animation_indices, body_animation_count] = collect_enabled_clip_indices(_bodyAnimController);
    const auto [move_animation_indices, move_animation_count] = collect_enabled_clip_indices(_moveAnimController);

    return ModelSpriteBounds {
        .Rect = {left, top, right - left, bottom - top},
        .RequiredFrameSize = *required_frame_size,
        .Pivot = required_pivot,
        .EnvelopeId =
            {
                .BodyAnimationIndices = body_animation_indices,
                .MoveAnimationIndices = move_animation_indices,
                .CombinedMeshGenerationRevision = _combinedMeshGenerationRevision,
                .BodyAnimationCount = body_animation_count,
                .MoveAnimationCount = move_animation_count,
                .ShadowEnabled = include_shadow,
                .FullFrame = force_full_frame,
            },
    };
}

auto ModelInstance::GetViewRect() const -> irect32
{
    FO_NO_STACK_TRACE_ENTRY();

    return _viewRect;
}

auto ModelInstance::GetSpeed() const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    return _speedAdjustCur * _speedAdjustBase * _speedAdjustLink * _modelMngr->_globalSpeedAdjust;
}

auto ModelInstance::GetMovementSpeed() const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    return _speedAdjustBase * _speedAdjustLink * _modelMngr->_globalSpeedAdjust;
}

auto ModelInstance::GetTime() const -> nanotime
{
    FO_STACK_TRACE_ENTRY();

    return _modelMngr->_gameTime->GetFrameTime();
}

auto ModelInstance::GetPoseJointIndex(ptr<const ModelBone> bone) const -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _modelInfo->_poseBoneJointIndexes.find(bone);
    FO_VERIFY_AND_THROW(it != _modelInfo->_poseBoneJointIndexes.end(), "Model bone is absent from the canonical pose", _modelInfo->_fileName, bone->Name);
    return it->second;
}

auto ModelInstance::GetWorldMatrix(uint32_t joint_index) const -> const mat44&
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(joint_index < _worldMatrices.size(), "Model pose joint index is outside the instance world-matrix snapshot", _modelInfo->_fileName, joint_index, _worldMatrices.size());
    return _worldMatrices[joint_index];
}

auto ModelInstance::GetProceduralJointRotationAngle(uint32_t joint_index) const noexcept -> optional<float32_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_modelInfo->_rotationBone || is_float_equal(_lookDirAngle, _moveDirAngle)) {
        return std::nullopt;
    }
    if (_modelInfo->_bodyRotationJointIndex && joint_index == *_modelInfo->_bodyRotationJointIndex) {
        return (GeometryHelper::GetDirAngleDiffSided(_lookDirAngle + (_isMovingBack ? 180.0f : 0.0f), _moveDirAngle) * -_modelMngr->_settings->CritterBodyTurnFactor) * DEG_TO_RAD_FLOAT;
    }
    if (_modelInfo->_headRotationJointIndex && joint_index == *_modelInfo->_headRotationJointIndex) {
        return (GeometryHelper::GetDirAngleDiffSided(_lookDirAngle + (_isMovingBack ? 180.0f : 0.0f), _moveDirAngle) * -_modelMngr->_settings->CritterHeadTurnFactor) * DEG_TO_RAD_FLOAT;
    }

    return std::nullopt;
}

auto ModelInstance::FillAnimationProceduralRotations(array<ModelAnimationRuntimePose::ProceduralLocalRotation, ModelAnimationRuntimePose::MAX_PROCEDURAL_ROTATIONS>& procedural_rotations) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    size_t procedural_rotation_count = 0;
    const auto append_rotation = [this, &procedural_rotations, &procedural_rotation_count](uint32_t joint_index) {
        const auto angle = GetProceduralJointRotationAngle(joint_index);

        if (!angle) {
            return;
        }

        FO_VERIFY_AND_THROW(procedural_rotation_count < procedural_rotations.size(), "Model pose resolves more procedural body/head joints than the animation runtime supports", _modelInfo->_fileName, procedural_rotation_count + 1, procedural_rotations.size());
        procedural_rotations[procedural_rotation_count] = ModelAnimationRuntimePose::ProceduralLocalRotation {
            .JointIndex = joint_index,
            .Rotation = glm::angleAxis(*angle, vec3 {1.0f, 0.0f, 0.0f}),
        };
        procedural_rotation_count++;
    };

    if (_modelInfo->_bodyRotationJointIndex) {
        append_rotation(*_modelInfo->_bodyRotationJointIndex);
    }
    if (_modelInfo->_headRotationJointIndex && _modelInfo->_headRotationJointIndex != _modelInfo->_bodyRotationJointIndex) {
        append_rotation(*_modelInfo->_headRotationJointIndex);
    }

    return procedural_rotation_count;
}

void ModelInstance::FillAnimationTrackInputs(nptr<const ModelAnimationController> controller, bool active, array<vector<uint8_t>, 2>& joint_masks, array<ModelAnimationRuntimePose::TrackInput, 2>& track_inputs) const
{
    FO_STACK_TRACE_ENTRY();

    for (size_t track_index = 0; track_index < track_inputs.size(); track_index++) {
        vector<uint8_t>& joint_mask = joint_masks[track_index];
        FO_STRONG_ASSERT(joint_mask.size() == _modelInfo->_poseJointRuntimeNames.size(), "Animation runtime track mask does not match the canonical model pose", _modelInfo->_fileName, track_index, joint_mask.size(), _modelInfo->_poseJointRuntimeNames.size());
        ModelAnimationRuntimePose::TrackInput& input = track_inputs[track_index];
        input.JointMask = joint_mask;

        if (!controller) {
            std::ranges::fill(joint_mask, uint8_t {});
            continue;
        }

        const int32_t track = numeric_cast<int32_t>(track_index);
        const ModelAnimationController::TrackState state = controller->GetTrackState(track);

        for (size_t joint_index = 0; joint_index < joint_mask.size(); joint_index++) {
            // Bindings use the runtime name; the model root can intentionally differ from its authored source name.
            joint_mask[joint_index] = controller->IsTrackBoneEnabled(track, _modelInfo->_poseJointRuntimeNames[joint_index]) ? 1 : 0;
        }

        input.ClipIndex = state.ClipIndex;
        input.Enabled = active && state.Enabled;
        input.Reversed = state.Reversed;
        input.Position = state.Position;
        input.Weight = state.Weight;
    }
}

void ModelInstance::ProcessAnimation(float32_t elapsed, ipos32 pos, float32_t scale)
{
    FO_STACK_TRACE_ENTRY();

    // Update world matrix, only for root
    if (!_parent) {
        _parentMatrix = MakeRootTransformation(pos, scale, _directSceneDraw);
        _groundPos = vec3 {_parentMatrix[3][0], _parentMatrix[3][1], _parentMatrix[3][2]};
    }

    // Rotate body
    if (!is_float_equal(_moveDirAngle, _targetMoveDirAngle)) {
        const auto diff = GeometryHelper::GetDirAngleDiffSided(_moveDirAngle, _targetMoveDirAngle);
        _moveDirAngle += std::clamp(diff * elapsed * 10.0f, -std::abs(diff), std::abs(diff));
    }

    // Advance animation time
    float32_t prev_track_pos = 0.0f;
    float32_t new_track_pos = 0.0f;

    if (_bodyAnimController && elapsed >= 0.0f) {
        prev_track_pos = _bodyAnimController->GetTrackPosition(_curTrack);

        _bodyAnimController->AdvanceTimeline(elapsed * GetSpeed());

        if ((_isMoving || _turnAnimPlaying) && _moveAnimController) {
            _moveAnimController->AdvanceTimeline(elapsed * GetMovementSpeed());

            if (_turnAnimPlaying && !_moveAnimController->GetTrackEnable(_curMoveTrack)) {
                _turnAnimPlaying = false;
                RefreshMoveAnimation();
            }
        }

        new_track_pos = _bodyAnimController->GetTrackPosition(_curTrack);

        if (_animDuration > 0.0f) {
            _animPosProc = new_track_pos / _animDuration;

            if (_animPosProc >= 1.0f) {
                _animPosProc = std::fmod(_animPosProc, 1.0f);
            }

            _animPosTime = new_track_pos;

            if (_animPosTime >= _animDuration) {
                _animPosTime = std::fmod(_animPosTime, _animDuration);
            }
        }
    }

    if (_animationRuntimePose) {
        array<ModelAnimationRuntimePose::TrackInput, 2> body_tracks {};
        array<ModelAnimationRuntimePose::TrackInput, 2> movement_tracks {};

        if (_bodyAnimController) {
            FillAnimationTrackInputs(&*_bodyAnimController, true, _animationBodyJointMasks, body_tracks);
            FillAnimationTrackInputs(_moveAnimController ? make_nptr(&*_moveAnimController) : nullptr, _isMoving || _turnAnimPlaying, _animationMovementJointMasks, movement_tracks);
        }

        array<ModelAnimationRuntimePose::ProceduralLocalRotation, ModelAnimationRuntimePose::MAX_PROCEDURAL_ROTATIONS> procedural_rotations {};
        const size_t procedural_rotation_count = FillAnimationProceduralRotations(procedural_rotations);
        _animationRuntimePose->Evaluate(body_tracks, movement_tracks, _parentMatrix, const_span<ModelAnimationRuntimePose::ProceduralLocalRotation> {procedural_rotations.data(), procedural_rotation_count});
        SnapshotAnimationWorldMatrices();
    }
    else {
        BuildRestWorldMatrices();
    }

    // Update linked matrices
    if (_parent && !_linkJoints.empty()) {
        for (const ModelPoseJointLink& binding : _linkJoints) {
            FO_STRONG_ASSERT(binding.ChildJointIndex < _worldMatrices.size(), "Linked model child joint index is outside its world-matrix snapshot", _modelInfo->_fileName, binding.ChildJointIndex, _worldMatrices.size());
            const mat44& parent_world_matrix = _parent->GetWorldMatrix(binding.ParentJointIndex);

            if (_animationRuntimePose) {
                _animationRuntimePose->OverrideWorldMatrix(binding.ChildJointIndex, parent_world_matrix);
                _worldMatrices[binding.ChildJointIndex] = _animationRuntimePose->GetWorldMatrices()[binding.ChildJointIndex];
            }
            else {
                _worldMatrices[binding.ChildJointIndex] = parent_world_matrix;
            }
        }
    }

    // Update world matrices for children
    for (size_t i = 0; i != _children.size(); ++i) {
        auto child = _children[i].as_ptr();

        child->_groundPos = _groundPos;
        child->_parentMatrix = GetWorldMatrix(child->_parentJointIndex) * child->_matTransBase * child->_matRotBase * child->_matScaleBase;
    }

    for (auto& model_particle : _modelParticles) {
        const mat44& proj = _directSceneDraw ? _drawProj : _frameProj;
        const vec3 view_offset = _directSceneDraw ? vec3 {} : _moveOffset;
        // The camera tilt is always supplied by the transform the particle inherits from the model, never by the
        // particle's own view matrix: in the atlas path it is baked into the bone world matrix (MakeRootTransformation
        // applies _matRot with the root world placement outermost), and in the direct-scene path it lives in _drawProj.
        // Re-applying it in the view matrix would double-tilt the effect and, worse for the atlas path, rotate the
        // frame-size-dependent root placement so it no longer cancels in _frameProj - the frame-sizing loop then chases
        // an ever-growing projected box across the shared model sprite frame and never converges.
        const bool tilt_in_proj = true;
        FO_VERIFY_AND_THROW(model_particle.Owner, "Model particle has no pose owner", model_particle.Id);
        const mat44& bone_world_matrix = model_particle.Owner->GetWorldMatrix(model_particle.JointIndex);

        if (model_particle.Id == 0) {
            model_particle.Particle->Setup(proj, bone_world_matrix, model_particle.Move, model_particle.Rot, view_offset, tilt_in_proj);
        }
        else {
            model_particle.Particle->Setup(proj, bone_world_matrix, model_particle.Move, model_particle.Rot + _lookDirAngle, view_offset, tilt_in_proj);
        }
    }

    for (auto it = _modelParticles.begin(); it != _modelParticles.end();) {
        if (!it->Particle->IsActive()) {
            it = _modelParticles.erase(it);
        }
        else {
            ++it;
        }
    }

    // Move child animations
    for (size_t i = 0; i != _children.size(); ++i) {
        _children[i]->ProcessAnimation(elapsed, pos, 1.0f);
    }

    if (!_parent) {
        RefreshConfigurationLayout();
    }

    // Animation callbacks
    if (_bodyAnimController && elapsed >= 0.0f && _animDuration > 0.0f) {
        for (auto& callback : _animationCallbacks) {
            if ((callback.StateAnim == CritterStateAnim::None || callback.StateAnim == _curStateAnim) && (callback.ActionAnim == CritterActionAnim::None || callback.ActionAnim == _curActionAnim)) {
                const auto fire_track_pos1 = floorf(prev_track_pos / _animDuration) * _animDuration + callback.NormalizedTime * _animDuration;
                const auto fire_track_pos2 = floorf(new_track_pos / _animDuration) * _animDuration + callback.NormalizedTime * _animDuration;

                if ((prev_track_pos < fire_track_pos1 && new_track_pos >= fire_track_pos1) || (prev_track_pos < fire_track_pos2 && new_track_pos >= fire_track_pos2)) {
                    callback.Callback();
                }
            }
        }
    }
}

void ModelInstance::SnapshotAnimationWorldMatrices()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_animationRuntimePose, "Model instance has no animation runtime pose to snapshot", _modelInfo->_fileName);
    const const_span<mat44> animation_world_matrices = _animationRuntimePose->GetWorldMatrices();
    FO_STRONG_ASSERT(animation_world_matrices.size() == _worldMatrices.size(), "Animation world-matrix count does not match the model instance snapshot", _modelInfo->_fileName, animation_world_matrices.size(), _worldMatrices.size());
    std::ranges::copy(animation_world_matrices, _worldMatrices.begin());
}

void ModelInstance::BuildRestWorldMatrices()
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!_animationRuntimePose, "Runtime-animated model entered the direct-model rest-pose path", _modelInfo->_fileName);
    BuildModelRestWorldMatrices(const_span<ModelPoseJoint> {_modelInfo->_restPoseJoints}, _parentMatrix, span<mat44> {_worldMatrices});

    if (!_modelInfo->_rotationBone || is_float_equal(_lookDirAngle, _moveDirAngle)) {
        return;
    }

    // Preserve the legacy procedural body/head rotations for static models.
    // The validated helper establishes the base rest pose; this parent-ordered
    // pass is the safe adapter for the two optional procedural joints.
    for (size_t joint_index = 0; joint_index < _worldMatrices.size(); joint_index++) {
        const ModelPoseJoint& joint = _modelInfo->_restPoseJoints[joint_index];
        const mat44& parent_matrix = joint.ParentIndex >= 0 ? _worldMatrices[numeric_cast<size_t>(joint.ParentIndex)] : _parentMatrix;

        if (const auto procedural_rotation_angle = GetProceduralJointRotationAngle(numeric_cast<uint32_t>(joint_index)); procedural_rotation_angle) {
            const mat44 procedural_rotation = glm::rotate(mat44 {1.0f}, *procedural_rotation_angle, vec3 {1.0f, 0.0f, 0.0f});
            _worldMatrices[joint_index] = parent_matrix * procedural_rotation * joint.RestLocalTransform;
        }
        else {
            _worldMatrices[joint_index] = parent_matrix * joint.RestLocalTransform;
        }
    }
}

auto ModelInstance::GetAnimDuration() const -> timespan
{
    FO_STACK_TRACE_ENTRY();

    return std::chrono::milliseconds(iround<int32_t>(_animDuration * 1000.0f));
}

auto ModelInstance::GetAnimDuration(CritterStateAnim state_anim, CritterActionAnim action_anim) -> timespan
{
    FO_STACK_TRACE_ENTRY();

    if (!_bodyAnimController) {
        return {};
    }

    float32_t speed = 1.0f;
    const auto anim_index = _modelInfo->GetAnimationIndex(state_anim, action_anim, &speed);

    if (anim_index < 0) {
        return {};
    }

    auto duration = _bodyAnimController->GetAnimDuration(anim_index);

    if (speed > 0.0f) {
        duration /= speed;
    }

    return std::chrono::milliseconds(iround<int32_t>(duration * 1000.0f));
}

void ModelInstance::GenerateCombinedMeshes()
{
    FO_STACK_TRACE_ENTRY();

    _spriteBoundsPoseReady = false;

    // Generation disabled
    if (!_allowMeshGeneration) {
        return;
    }

    // Clean up buffers
    for (size_t i = 0; i != _combinedMeshes.size(); ++i) {
        auto combined_mesh = _combinedMeshes[i].as_ptr();

        combined_mesh->EncapsulatedMeshCount = 0;
        combined_mesh->CurBoneMatrix = 0;
        combined_mesh->Meshes.clear();
        combined_mesh->MeshIndices.clear();
        combined_mesh->MeshVertices.clear();
        combined_mesh->MeshAnimLayers.clear();
        combined_mesh->MeshBuf->Vertices3D.clear();
        combined_mesh->MeshBuf->VertCount = 0;
        combined_mesh->MeshBuf->Indices.clear();
        combined_mesh->MeshBuf->IndCount = 0;
        std::ranges::fill(combined_mesh->SkinBindings, SkinBinding {});
        combined_mesh->SpriteVertices.clear();
        combined_mesh->SpriteBoundsValid = false;
        combined_mesh->HasSpriteGeometry = false;
    }

    _actualCombinedMeshesCount = 0;

    // Combine meshes recursively
    FillCombinedMeshes(this);

    // Cut
    _disableCulling = false;
    CutCombinedMeshes(this);

    // Finalize meshes
    for (size_t i = 0; i < _actualCombinedMeshesCount; i++) {
        auto combined_mesh = _combinedMeshes[i].as_ptr();
        const auto& vertices = combined_mesh->MeshBuf->Vertices3D;
        const auto& indices = combined_mesh->MeshBuf->Indices;

        combined_mesh->SpriteBoundsValid = true;
        vector<bool> included_vertices(vertices.size());

        for (const vindex_t vertex_index : indices) {
            if (numeric_cast<size_t>(vertex_index) >= vertices.size()) {
                combined_mesh->SpriteBoundsValid = false;
                break;
            }

            const Vertex3D& vertex = vertices[vertex_index];

            if (!std::isfinite(vertex.Position.x) || !std::isfinite(vertex.Position.y) || !std::isfinite(vertex.Position.z)) {
                combined_mesh->SpriteBoundsValid = false;
                break;
            }

            if (!included_vertices[vertex_index]) {
                combined_mesh->SpriteVertices.emplace_back(vertex_index);
                included_vertices[vertex_index] = true;
            }

            float32_t total_weight = 0.0f;

            for (size_t influence = 0; influence < MODEL_BONES_PER_VERTEX; influence++) {
                const float32_t weight = vertex.BlendWeights[influence];

                if (!std::isfinite(weight)) {
                    combined_mesh->SpriteBoundsValid = false;
                    break;
                }
                if (weight <= 0.0f) {
                    continue;
                }

                const float32_t bone_index_value = vertex.BlendIndices[influence];

                if (!std::isfinite(bone_index_value) || bone_index_value < 0.0f || bone_index_value >= numeric_cast<float32_t>(combined_mesh->CurBoneMatrix)) {
                    combined_mesh->SpriteBoundsValid = false;
                    break;
                }

                const size_t bone_index = numeric_cast<size_t>(iround<int32_t>(bone_index_value));

                if (numeric_cast<float32_t>(bone_index) != bone_index_value) {
                    combined_mesh->SpriteBoundsValid = false;
                    break;
                }

                total_weight += weight;
            }

            if (!combined_mesh->SpriteBoundsValid) {
                break;
            }
            if (!is_float_equal(total_weight, 1.0f)) {
                combined_mesh->SpriteBoundsValid = false;
                break;
            }

            combined_mesh->HasSpriteGeometry = true;
        }

        combined_mesh->MeshBuf->StaticDataChanged = true;
    }

    _combinedMeshGenerationRevision++;
}

void ModelInstance::InvalidateCombinedMeshes() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    ModelInstance* root = this;

    while (root->_parent) {
        root = root->_parent.get();
    }

    root->_actualCombinedMeshesCount = 0;

    for (auto& combined_mesh : root->_combinedMeshes) {
        combined_mesh->CurBoneMatrix = 0;
        std::ranges::fill(combined_mesh->SkinBindings, SkinBinding {});
    }
}

void ModelInstance::FillCombinedMeshes(ptr<const ModelInstance> cur)
{
    FO_STACK_TRACE_ENTRY();

    // Combine meshes
    for (size_t i = 0; i < cur->_allMeshes.size(); i++) {
        CombineMesh(cur, cur->_allMeshes[i], cur->_parent ? cur->_animLink.Layer : 0);
    }

    // Fill child
    for (size_t i = 0; i < cur->_children.size(); i++) {
        FillCombinedMeshes(cur->_children[i]);
    }
}

auto ModelInstance::CreateCombinedMesh() -> unique_ptr<CombinedMesh>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<CombinedMesh>(CombinedMesh {
        .MeshBuf = _modelMngr->_render->CreateDrawBuffer(true),
        .SkinBindings = vector<SkinBinding>(MODEL_MAX_BONES),
    });
}

void ModelInstance::CombineMesh(ptr<const ModelInstance> owner, ptr<const MeshInstance> mesh_instance, int32_t anim_layer)
{
    FO_STACK_TRACE_ENTRY();

    // Skip disabled meshes
    if (mesh_instance->Disabled) {
        return;
    }

    // Try to encapsulate mesh instance to current combined mesh
    for (size_t i = 0; i < _actualCombinedMeshesCount; i++) {
        if (CanBatchCombinedMesh(_combinedMeshes[i], mesh_instance)) {
            BatchCombinedMesh(_combinedMeshes[i], owner, mesh_instance, anim_layer);
            return;
        }
    }

    // Create new combined mesh
    if (_actualCombinedMeshesCount >= _combinedMeshes.size()) {
        _combinedMeshes.emplace_back(CreateCombinedMesh());
    }

    BatchCombinedMesh(_combinedMeshes[_actualCombinedMeshesCount], owner, mesh_instance, anim_layer);
    _actualCombinedMeshesCount++;
}

auto ModelInstance::CanBatchCombinedMesh(ptr<const CombinedMesh> combined_mesh, ptr<const MeshInstance> mesh_instance) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (combined_mesh->EncapsulatedMeshCount == 0) {
        return true;
    }
    if (combined_mesh->DrawEffect != mesh_instance->CurEffect) {
        return false;
    }
    for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
        if (combined_mesh->Textures[i] && mesh_instance->CurTexures[i] && combined_mesh->Textures[i]->MainTex != mesh_instance->CurTexures[i]->MainTex) {
            return false;
        }
    }
    return combined_mesh->CurBoneMatrix + mesh_instance->Mesh->SkinBones.size() <= combined_mesh->SkinBindings.size();
}

void ModelInstance::BatchCombinedMesh(ptr<CombinedMesh> combined_mesh, ptr<const ModelInstance> owner, ptr<const MeshInstance> mesh_instance, int32_t anim_layer)
{
    FO_STACK_TRACE_ENTRY();

    ptr<MeshData> mesh_data = mesh_instance->Mesh.get_no_const();
    auto& vertices = combined_mesh->MeshBuf->Vertices3D;
    auto& indices = combined_mesh->MeshBuf->Indices;
    const auto vertices_old_size = vertices.size();
    const bool replace_combined_mesh = combined_mesh->EncapsulatedMeshCount == 0;
    const auto index_offset = replace_combined_mesh ? vindex_t {} : numeric_cast<vindex_t>(vertices_old_size);
    const auto mesh_vertices_count = numeric_cast<uint32_t>(mesh_data->Vertices.size());
    const auto mesh_indices_count = numeric_cast<uint32_t>(mesh_data->Indices.size());
    const size_t first_skin_binding = replace_combined_mesh ? 0 : combined_mesh->CurBoneMatrix;

    FO_VERIFY_AND_THROW(mesh_data->SkinBones.size() == mesh_data->SkinBoneOffsets.size(), "Mesh skin bones and offsets count mismatch", owner->_modelInfo->_fileName, mesh_data->Owner->Name, mesh_data->SkinBones.size(), mesh_data->SkinBoneOffsets.size());
    FO_VERIFY_AND_THROW(first_skin_binding <= combined_mesh->SkinBindings.size() && mesh_data->SkinBones.size() <= combined_mesh->SkinBindings.size() - first_skin_binding, "Combined mesh skin bindings exceed capacity", owner->_modelInfo->_fileName, mesh_data->Owner->Name, first_skin_binding, mesh_data->SkinBones.size(), combined_mesh->SkinBindings.size());

    vector<SkinBinding> skin_bindings;
    skin_bindings.reserve(mesh_data->SkinBones.size());

    for (size_t i = 0; i < mesh_data->SkinBones.size(); i++) {
        const auto source_bone = mesh_data->SkinBones[i];
        FO_VERIFY_AND_THROW(source_bone, "Mesh skin binding has no source bone", owner->_modelInfo->_fileName, mesh_data->Owner->Name, i);
        skin_bindings.emplace_back(SkinBinding {owner, owner->GetPoseJointIndex(source_bone), source_bone, mesh_data->SkinBoneOffsets[i]});
    }

    // Set or add data
    if (replace_combined_mesh) {
        vertices = mesh_data->Vertices;
        indices = mesh_data->Indices;
        combined_mesh->DrawEffect = mesh_instance->CurEffect;
        std::ranges::fill(combined_mesh->SkinBindings, SkinBinding {});
        std::ranges::for_each(combined_mesh->Textures, [](auto&& tex) { tex = nullptr; });
        combined_mesh->CurBoneMatrix = 0;
    }
    else {
        vertices.insert(vertices.end(), mesh_data->Vertices.begin(), mesh_data->Vertices.end());
        indices.insert(indices.end(), mesh_data->Indices.begin(), mesh_data->Indices.end());

        // Add indices offset
        const auto start_index = indices.size() - mesh_data->Indices.size();
        for (auto i = start_index, j = indices.size(); i < j; i++) {
            indices[i] += index_offset;
        }

        // Add bones matrices offset
        const auto bone_index_offset = numeric_cast<float32_t>(combined_mesh->CurBoneMatrix);
        const auto start_vertex = vertices.size() - mesh_data->Vertices.size();
        for (auto i = start_vertex, j = vertices.size(); i < j; i++) {
            for (auto& blend_index : vertices[i].BlendIndices) {
                blend_index += bone_index_offset;
            }
        }
    }

    // Set mesh transform and anim layer
    combined_mesh->Meshes.emplace_back(mesh_data);
    combined_mesh->MeshVertices.emplace_back(mesh_vertices_count);
    combined_mesh->MeshIndices.emplace_back(mesh_indices_count);
    combined_mesh->MeshAnimLayers.emplace_back(anim_layer);

    // Add bones matrices
    for (size_t i = 0; i < skin_bindings.size(); i++) {
        combined_mesh->SkinBindings[first_skin_binding + i] = skin_bindings[i];
    }

    combined_mesh->CurBoneMatrix = first_skin_binding + skin_bindings.size();

    // Add textures
    for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
        if (!combined_mesh->Textures[i] && mesh_instance->CurTexures[i]) {
            combined_mesh->Textures[i] = mesh_instance->CurTexures[i];
        }
    }

    // Fix texture coords
    if (mesh_instance->CurTexures[0]) {
        const auto mesh_tex = mesh_instance->CurTexures[0];

        for (auto i = vertices_old_size, j = vertices.size(); i < j; i++) {
            vertices[i].TexCoord[0] = (vertices[i].TexCoord[0] * mesh_tex->AtlasOffsetData.width) + mesh_tex->AtlasOffsetData.x;
            vertices[i].TexCoord[1] = (vertices[i].TexCoord[1] * mesh_tex->AtlasOffsetData.height) + mesh_tex->AtlasOffsetData.y;
        }
    }

    // Increment mesh count
    combined_mesh->EncapsulatedMeshCount++;

    combined_mesh->MeshBuf->VertCount = combined_mesh->MeshBuf->Vertices3D.size();
    combined_mesh->MeshBuf->IndCount = combined_mesh->MeshBuf->Indices.size();
}

void ModelInstance::CutCombinedMeshes(ptr<const ModelInstance> cur)
{
    FO_STACK_TRACE_ENTRY();

    // Cut meshes
    if (!cur->_allCuts.empty()) {
        for (ptr<const ModelCutData> cut : cur->_allCuts) {
            for (size_t i = 0; i < _actualCombinedMeshesCount; i++) {
                CutCombinedMesh(_combinedMeshes[i], cut);
            }
        }

        _disableCulling = true;
    }

    // Fill child
    for (size_t i = 0; i < cur->_children.size(); i++) {
        CutCombinedMeshes(cur->_children[i]);
    }
}

// -2 - ignore
// -1 - inside
// 0 - outside
// 1 - one point
static auto SphereLineIntersection(const Vertex3D& p1, const Vertex3D& p2, const vec3& sp, float32_t r, Vertex3D& in) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto sq = [](float32_t f) -> float32_t { return f * f; };
    const auto a = sq(p2.Position.x - p1.Position.x) + sq(p2.Position.y - p1.Position.y) + sq(p2.Position.z - p1.Position.z);
    const auto b = 2 * ((p2.Position.x - p1.Position.x) * (p1.Position.x - sp.x) + (p2.Position.y - p1.Position.y) * (p1.Position.y - sp.y) + (p2.Position.z - p1.Position.z) * (p1.Position.z - sp.z));
    const auto c = sq(sp.x) + sq(sp.y) + sq(sp.z) + sq(p1.Position.x) + sq(p1.Position.y) + sq(p1.Position.z) - 2 * (sp.x * p1.Position.x + sp.y * p1.Position.y + sp.z * p1.Position.z) - sq(r);
    const auto i = sq(b) - 4 * a * c;

    if (i > 0.0f) {
        const auto sqrt_i = sqrt(i);
        const auto mu1 = (-b + sqrt_i) / (2 * a);
        const auto mu2 = (-b - sqrt_i) / (2 * a);

        // Line segment doesn't intersect and on outside of sphere, in which case both values of u wll either be less
        // than 0 or greater than 1
        if ((mu1 < 0.0f && mu2 < 0.0f) || (mu1 > 1.0f && mu2 > 1.0f)) {
            return 0;
        }

        // Line segment doesn't intersect and is inside sphere, in which case one value of u will be negative and the
        // other greater than 1
        if ((mu1 < 0.0f && mu2 > 1.0f) || (mu2 < 0.0f && mu1 > 1.0f)) {
            return -1;
        }

        // Line segment intersects at one point, in which case one value of u will be between 0 and 1 and the other not
        if ((mu1 >= 0.0f && mu1 <= 1.0f && (mu2 < 0.0f || mu2 > 1.0f)) || (mu2 >= 0.0f && mu2 <= 1.0f && (mu1 < 0.0f || mu1 > 1.0f))) {
            const auto& mu = ((mu1 >= 0.0f && mu1 <= 1.0f) ? mu1 : mu2);
            in = p1;
            in.Position.x = p1.Position.x + mu * (p2.Position.x - p1.Position.x);
            in.Position.y = p1.Position.y + mu * (p2.Position.y - p1.Position.y);
            in.Position.z = p1.Position.z + mu * (p2.Position.z - p1.Position.z);
            in.TexCoord[0] = p1.TexCoord[0] + mu * (p2.TexCoord[0] - p1.TexCoord[0]);
            in.TexCoord[1] = p1.TexCoord[1] + mu * (p2.TexCoord[1] - p1.TexCoord[1]);
            in.TexCoordBase[0] = p1.TexCoordBase[0] + mu * (p2.TexCoordBase[0] - p1.TexCoordBase[0]);
            in.TexCoordBase[1] = p1.TexCoordBase[1] + mu * (p2.TexCoordBase[1] - p1.TexCoordBase[1]);
            return 1;
        }

        // Line segment intersects at two points, in which case both values of u will be between 0 and 1
        if (mu1 >= 0.0f && mu1 <= 1.0f && mu2 >= 0.0f && mu2 <= 1.0f) {
            // Ignore
            return -2;
        }
    }
    else if (i == 0.0f) {
        // Ignore
        return -2;
    }
    return 0;
}

void ModelInstance::CutCombinedMesh(ptr<CombinedMesh> combined_mesh, ptr<const ModelCutData> cut)
{
    FO_STACK_TRACE_ENTRY();

    auto& vertices = combined_mesh->MeshBuf->Vertices3D;
    auto& indices = combined_mesh->MeshBuf->Indices;

    for (const auto& shape : cut->Shapes) {
        vector<Vertex3D> result_vertices;
        vector<vindex_t> result_indices;
        vector<uint32_t> result_mesh_vertices;
        vector<uint32_t> result_mesh_indices;

        result_vertices.reserve(vertices.size());
        result_indices.reserve(indices.size());
        result_mesh_vertices.reserve(combined_mesh->MeshVertices.size());
        result_mesh_indices.reserve(combined_mesh->MeshIndices.size());

        uint32_t i_pos = 0;
        uint32_t i_count = 0;

        for (size_t k = 0, l = combined_mesh->MeshIndices.size(); k < l; k++) {
            // Move shape to face space
            const auto mesh_transform = combined_mesh->Meshes[k]->Owner->GlobalTransformationMatrix;
            const auto sm = glm::inverse(mesh_transform) * shape.GlobalTransformationMatrix;
            vec3 ss {};
            vec3 sp {};
            quaternion sr {};
            vec3 skew {};
            glm::vec<4, float32_t, glm::defaultp> perspective {};
            glm::decompose(sm, ss, sr, sp, skew, perspective);

            // Check anim layer
            const auto mesh_anim_layer = combined_mesh->MeshAnimLayers[k];
            const auto skip = std::ranges::find(cut->Layers, mesh_anim_layer) == cut->Layers.end();

            // Process faces
            i_count += combined_mesh->MeshIndices[k];
            auto vertices_old_size = result_vertices.size();
            auto indices_old_size = result_indices.size();

            for (; i_pos < i_count; i_pos += 3) {
                // Face points
                const auto& v1 = vertices[indices[i_pos + 0]];
                const auto& v2 = vertices[indices[i_pos + 1]];
                const auto& v3 = vertices[indices[i_pos + 2]];

                // Skip mesh
                if (skip) {
                    result_vertices.emplace_back(v1);
                    result_vertices.emplace_back(v2);
                    result_vertices.emplace_back(v3);
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    continue;
                }

                if (shape.IsSphere) {
                    // Find intersections
                    Vertex3D i1;
                    int32_t r1 = SphereLineIntersection(v1, v2, sp, shape.SphereRadius * ss.x, i1);
                    Vertex3D i2;
                    int32_t r2 = SphereLineIntersection(v2, v3, sp, shape.SphereRadius * ss.x, i2);
                    Vertex3D i3;
                    int32_t r3 = SphereLineIntersection(v3, v1, sp, shape.SphereRadius * ss.x, i3);

                    // Process intersections
                    bool outside = (r1 == 0 && r2 == 0 && r3 == 0);
                    bool ignore = (r1 == -2 || r2 == -2 || r3 == -2);
                    int32_t sum = r1 + r2 + r3;

                    if (!ignore && sum == 2) {
                        // 1 1 0, corner in
                        const Vertex3D& vv1 = (r1 == 0 ? v1 : (r2 == 0 ? v2 : v3));
                        const Vertex3D& vv2 = (r1 == 0 ? v2 : (r2 == 0 ? v3 : v1));
                        const Vertex3D& vv3 = (r1 == 0 ? i3 : (r2 == 0 ? i1 : i2));
                        const Vertex3D& vv4 = (r1 == 0 ? i2 : (r2 == 0 ? i3 : i1));

                        // First face
                        result_vertices.emplace_back(vv1);
                        result_vertices.emplace_back(vv2);
                        result_vertices.emplace_back(vv3);
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));

                        // Second face
                        result_vertices.emplace_back(vv3);
                        result_vertices.emplace_back(vv2);
                        result_vertices.emplace_back(vv4);
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    }
                    else if (!ignore && sum == 1) {
                        // 1 1 -1, corner out
                        const Vertex3D& vv1 = (r1 == -1 ? i3 : (r2 == -1 ? v1 : i1));
                        const Vertex3D& vv2 = (r1 == -1 ? i2 : (r2 == -1 ? i1 : v2));
                        const Vertex3D& vv3 = (r1 == -1 ? v3 : (r2 == -1 ? i3 : i2));

                        // One face
                        result_vertices.emplace_back(vv1);
                        result_vertices.emplace_back(vv2);
                        result_vertices.emplace_back(vv3);
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    }
                    else if (ignore || outside) {
                        if (ignore && sum == 0) {
                            // 1 1 -2
                            continue;
                        }

                        result_vertices.emplace_back(v1);
                        result_vertices.emplace_back(v2);
                        result_vertices.emplace_back(v3);
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                        result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    }
                }
                else {
                    if ((v1.Position.x > shape.BBoxMin.x && v1.Position.y > shape.BBoxMin.y && v1.Position.z > shape.BBoxMin.z && v1.Position.x < shape.BBoxMax.x && v1.Position.y < shape.BBoxMax.y && v1.Position.z > shape.BBoxMax.z) || //
                        (v2.Position.x > shape.BBoxMin.x && v2.Position.y > shape.BBoxMin.y && v2.Position.z > shape.BBoxMin.z && v2.Position.x < shape.BBoxMax.x && v2.Position.y < shape.BBoxMax.y && v2.Position.z > shape.BBoxMax.z) || //
                        (v3.Position.x > shape.BBoxMin.x && v3.Position.y > shape.BBoxMin.y && v3.Position.z > shape.BBoxMin.z && v3.Position.x < shape.BBoxMax.x && v3.Position.y < shape.BBoxMax.y && v3.Position.z > shape.BBoxMax.z)) {
                        continue;
                    }

                    result_vertices.emplace_back(v1);
                    result_vertices.emplace_back(v2);
                    result_vertices.emplace_back(v3);
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                    result_indices.emplace_back(numeric_cast<vindex_t>(result_indices.size()));
                }
            }

            result_mesh_vertices.emplace_back(numeric_cast<uint32_t>(result_vertices.size() - vertices_old_size));
            result_mesh_indices.emplace_back(numeric_cast<uint32_t>(result_indices.size() - indices_old_size));
        }

        vertices = result_vertices;
        indices = result_indices;
        combined_mesh->MeshVertices = result_mesh_vertices;
        combined_mesh->MeshIndices = result_mesh_indices;

        combined_mesh->MeshBuf->VertCount = combined_mesh->MeshBuf->Vertices3D.size();
        combined_mesh->MeshBuf->IndCount = combined_mesh->MeshBuf->Indices.size();
    }

    // Unskin
    if (cut->UnskinBone1 && cut->UnskinBone2) {
        // Find unskin bones
        nptr<ModelBone> unskin_bone1;
        float32_t unskin_bone1_index = 0.0f;
        nptr<ModelBone> unskin_bone2;
        float32_t unskin_bone2_index = 0.0f;

        for (size_t i = 0; i < combined_mesh->CurBoneMatrix; i++) {
            const auto source_bone = combined_mesh->SkinBindings[i].SourceBone;
            FO_VERIFY_AND_THROW(source_bone, "Combined mesh skin binding has no source bone", i, combined_mesh->CurBoneMatrix);

            if (source_bone->Name == cut->UnskinBone1) {
                unskin_bone1 = source_bone;
                unskin_bone1_index = numeric_cast<float32_t>(i);
            }
            if (source_bone->Name == cut->UnskinBone2) {
                unskin_bone2 = source_bone;
                unskin_bone2_index = numeric_cast<float32_t>(i);
            }
            if (unskin_bone1 && unskin_bone2) {
                break;
            }
        }

        // Unskin
        if (unskin_bone1 && unskin_bone2) {
            // Process meshes
            size_t v_pos = 0;
            size_t v_count = 0;

            for (size_t i = 0, j = combined_mesh->MeshVertices.size(); i < j; i++) {
                // Check anim layer
                if (std::ranges::find(cut->Layers, combined_mesh->MeshAnimLayers[i]) == cut->Layers.end()) {
                    v_count += combined_mesh->MeshVertices[i];
                    v_pos = v_count;
                    continue;
                }

                // Move shape to face space
                const auto mesh_transform = combined_mesh->Meshes[i]->Owner->GlobalTransformationMatrix;
                const auto sm = glm::inverse(mesh_transform) * cut->UnskinShape.GlobalTransformationMatrix;
                vec3 ss {};
                vec3 sp {};
                quaternion sr {};
                vec3 skew {};
                glm::vec<4, float32_t, glm::defaultp> perspective {};
                glm::decompose(sm, ss, sr, sp, skew, perspective);
                auto sphere_square_radius = powf(cut->UnskinShape.SphereRadius * ss.x, 2.0f);
                auto revert_shape = cut->RevertUnskinShape;

                // Process mesh vertices
                v_count += combined_mesh->MeshVertices[i];

                for (; v_pos < v_count; v_pos++) {
                    auto& v = vertices[v_pos];

                    // Get vertex side
                    const auto diff = v.Position - sp;
                    auto v_side = (glm::dot(diff, diff) <= sphere_square_radius);
                    if (revert_shape) {
                        v_side = !v_side;
                    }

                    // Check influences
                    for (size_t b = 0; b < MODEL_BONES_PER_VERTEX; b++) {
                        // No influence
                        auto w = v.BlendWeights[b];

                        if (w < 0.00001f) {
                            continue;
                        }

                        // Last influence, don't reskin
                        if (w > 1.0f - 0.00001f) {
                            break;
                        }

                        // Skip equal influence side
                        const auto influence_bone = combined_mesh->SkinBindings[iround<int32_t>(v.BlendIndices[b])].SourceBone;
                        FO_VERIFY_AND_THROW(influence_bone, "Combined mesh cut influence has no source bone");
                        bool influence_side = !!FindModelBone(unskin_bone1.as_ptr(), influence_bone->Name);

                        if (v_side == influence_side) {
                            continue;
                        }

                        // Last influence, don't reskin
                        if (w > 1.0f - 0.00001f) {
                            v.BlendIndices[b] = (influence_side ? unskin_bone2_index : unskin_bone1_index);
                            v.BlendWeights[b] = 1.0f;
                            break;
                        }

                        // Move influence to other bones
                        v.BlendWeights[b] = 0.0f;
                        for (auto& blend_weight : v.BlendWeights) {
                            blend_weight += blend_weight / (1.0f - w) * w;
                        }
                    }
                }
            }
        }
    }

    combined_mesh->MeshBuf->VertCount = combined_mesh->MeshBuf->Vertices3D.size();
    combined_mesh->MeshBuf->IndCount = combined_mesh->MeshBuf->Indices.size();
}

void ModelInstance::SetupFrame(isize32 draw_size, ipos32 frame_pivot)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(draw_size.width > 0 && draw_size.width <= std::numeric_limits<int32_t>::max() / FRAME_SCALE, "3D model frame width is out of range", draw_size.width);
    FO_VERIFY_AND_THROW(draw_size.height > 0 && draw_size.height <= std::numeric_limits<int32_t>::max() / FRAME_SCALE, "3D model frame height is out of range", draw_size.height);

    optional<vec3> old_root_pos;

    if (_frameSize.width > 0 && _frameSize.height > 0 && _frameSize.width % FRAME_SCALE == 0 && _frameSize.height % FRAME_SCALE == 0) {
        old_root_pos = Convert2dTo3d(_framePivot);
    }

    _spriteBoundsPoseReady = false;
    _frameSize.width = draw_size.width * FRAME_SCALE;
    _frameSize.height = draw_size.height * FRAME_SCALE;

    // Projection
    const auto frame_ratio = numeric_cast<float32_t>(_frameSize.width) / numeric_cast<float32_t>(_frameSize.height);
    const auto proj_height = numeric_cast<float32_t>(_frameSize.height) * (1.0f / _modelMngr->_settings->ModelProjFactor);
    const auto proj_width = proj_height * frame_ratio;

    _frameProj = _modelMngr->_render->CreateOrthoMatrix(0.0f, proj_width, 0.0f, proj_height, -10.0f, 10.0f);

    if (old_root_pos) {
        const vec3 new_root_pos = Convert2dTo3d(frame_pivot);
        const vec3 rebase_delta = new_root_pos - *old_root_pos;
        const auto rebase_particles = [&rebase_delta](ptr<ModelInstance> model, const auto& recurse) noexcept -> void {
            for (auto& model_particle : model->_modelParticles) {
                model_particle.Particle->RebaseWorldParticles(rebase_delta);
            }

            for (auto& child : model->_children) {
                recurse(child.as_ptr(), recurse);
            }
        };
        rebase_particles(this, rebase_particles);
    }

    _framePivot = frame_pivot;
}

void ModelInstance::PrepareFrameLayout()
{
    FO_STACK_TRACE_ENTRY();

    if (_frameLayoutDirty) {
        RefreshFrameLayout();
    }
}

void ModelInstance::RequestRedraw() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _forceDraw = true;
}

void ModelInstance::RefreshFrameLayout()
{
    FO_STACK_TRACE_ENTRY();

    const mat44 post_direction_transform = _matTransBase * _matRot;
    const mat44 pre_direction_transform = _matRotBase * _matScale * _matScaleBase;
    optional<ModelBounds3D> active_bounds;
    const auto include_active_tracks = [this, &active_bounds](const optional<ModelAnimationController>& controller) {
        if (!controller) {
            return;
        }

        for (int32_t track = 0; track < 2; track++) {
            const ModelAnimationController::TrackState state = controller->GetTrackState(track);

            if (!state.Enabled || state.ClipIndex < 0 || numeric_cast<size_t>(state.ClipIndex) >= _modelInfo->_animationBounds.size() || !_modelInfo->_animationBounds[numeric_cast<size_t>(state.ClipIndex)]) {
                continue;
            }

            const ModelBounds3D& bounds = *_modelInfo->_animationBounds[numeric_cast<size_t>(state.ClipIndex)];
            FO_STRONG_ASSERT(IncludeModelBounds(active_bounds, bounds), "Active animation bounds are invalid", _modelInfo->_fileName, state.ClipIndex);
        }
    };
    include_active_tracks(_bodyAnimController);
    include_active_tracks(_moveAnimController);

    const ModelBounds3D& draw_bounds = active_bounds ? *active_bounds : _modelInfo->_modelBounds;
    const optional<ModelSpriteLayout> draw_layout = CalculateModelSpriteLayout(draw_bounds, post_direction_transform, pre_direction_transform, _modelMngr->_settings->ModelProjFactor, !_shadowDisabled && !_modelInfo->_shadowDisabled);
    FO_STRONG_ASSERT(draw_layout, "Model sprite layout could not be calculated", _modelInfo->_fileName, draw_bounds.Min.x, draw_bounds.Min.y, draw_bounds.Min.z, draw_bounds.Max.x, draw_bounds.Max.y, draw_bounds.Max.z);
    _layoutDrawSize = draw_layout->DrawSize;
    _drawRect = draw_layout->DrawRect;

    const optional<ModelSpriteLayout> lighting_layout = CalculateModelSpriteLayout(_modelInfo->_modelBounds, post_direction_transform, pre_direction_transform, _modelMngr->_settings->ModelProjFactor, false);
    FO_STRONG_ASSERT(lighting_layout, "Model sprite lighting layout could not be calculated", _modelInfo->_fileName);
    _lightingDrawSize = lighting_layout->DrawSize;

    // The model origin sits at its real projected position inside the tight frame (top-left of the draw rect is
    // the frame's top-left), not at a fixed quarter fraction.
    const ipos32 frame_pivot = {-_drawRect.x, -_drawRect.y};

    if (_frameSize.width != _layoutDrawSize.width * FRAME_SCALE || _frameSize.height != _layoutDrawSize.height * FRAME_SCALE) {
        SetupFrame(_layoutDrawSize, frame_pivot);
        _forceDraw = true;
    }

    _framePivot = frame_pivot;
    _frameLayoutDirty = false;
}

void ModelInstance::RefreshConfigurationLayout()
{
    FO_STACK_TRACE_ENTRY();

    if (_parent) {
        return;
    }

    const auto is_finite_matrix = [](const mat44& matrix) noexcept -> bool {
        const ptr<const float32_t> values = glm::value_ptr(matrix);

        for (size_t i = 0; i < 16; i++) {
            if (!std::isfinite(values[i])) {
                return false;
            }
        }

        return true;
    };
    const mat44 root_inverse = glm::inverse(_parentMatrix);

    if (!is_finite_matrix(root_inverse)) {
        return;
    }

    optional<ModelBounds3D> current_model_bounds;
    optional<ModelBounds3D> current_view_bounds;
    const auto include_model_tree = [&](ptr<const ModelInstance> model, const auto& recurse) noexcept -> bool {
        const bool has_visible_mesh = std::ranges::any_of(model->_allMeshes, [](const auto& mesh) noexcept { return !mesh->Disabled; });

        if (has_visible_mesh) {
            const mat44 relative_transform = root_inverse * model->_parentMatrix;
            const ModelBounds3D& model_view_bounds = model == this ? model->_modelInfo->_viewBounds : model->_modelInfo->_modelBounds;

            if (!IncludeTransformedModelBounds(current_model_bounds, model->_modelInfo->_modelBounds, relative_transform) || !IncludeTransformedModelBounds(current_view_bounds, model_view_bounds, relative_transform)) {
                return false;
            }
        }

        for (const auto& child : model->_children) {
            if (!recurse(child.as_ptr(), recurse)) {
                return false;
            }
        }

        return true;
    };

    if (!include_model_tree(this, include_model_tree)) {
        return;
    }

    if (!current_model_bounds || !current_view_bounds) {
        current_model_bounds = _modelInfo->_modelBounds;
        current_view_bounds = _modelInfo->_viewBounds;
    }

    if (_configurationLayoutRevision != _combinedMeshGenerationRevision || !_configurationModelBounds || !_configurationViewBounds) {
        _configurationModelBounds = *current_model_bounds;
        _configurationViewBounds = *current_view_bounds;
        _configurationLayoutRevision = _combinedMeshGenerationRevision;
    }
    else if (!IncludeModelBounds(_configurationModelBounds, *current_model_bounds) || !IncludeModelBounds(_configurationViewBounds, *current_view_bounds)) {
        return;
    }

    const mat44 post_direction_transform = _matTransBase * _matRot;
    const mat44 pre_direction_transform = _matRotBase * _matScale * _matScaleBase;
    const optional<ModelSpriteLayout> lighting_layout = CalculateModelSpriteLayout(*_configurationModelBounds, post_direction_transform, pre_direction_transform, _modelMngr->_settings->ModelProjFactor, false);
    const optional<ModelSpriteLayout> view_layout = CalculateModelSpriteLayout(*_configurationViewBounds, post_direction_transform, pre_direction_transform, _modelMngr->_settings->ModelProjFactor, false);

    if (!lighting_layout || !view_layout) {
        return;
    }

    _lightingDrawSize = lighting_layout->DrawSize;

    constexpr int32_t view_ground_margin = 8;
    irect32 view_rect = view_layout->ViewRect;
    const int64_t computed_bottom = numeric_cast<int64_t>(view_rect.y) + view_rect.height;
    const int64_t view_bottom = std::max(computed_bottom, numeric_cast<int64_t>(view_ground_margin));
    const int64_t view_height = view_bottom - view_rect.y;

    if (view_height <= 0 || view_height > std::numeric_limits<int32_t>::max()) {
        return;
    }

    view_rect.height = numeric_cast<int32_t>(view_height);
    _viewRect = view_rect;
}

auto ModelInstance::Convert3dTo2d(vec3 pos) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    const int32_t viewport[4] = {0, 0, _frameSize.width, _frameSize.height};
    vec3 out {};
    const mat44 identity {1.0f};

    if (!ProjectPoint(pos, identity, _frameProj, viewport, out)) {
        return {};
    }

    return {iround<int32_t>(out.x / const_numeric_cast<float32_t>(FRAME_SCALE)), iround<int32_t>(out.y / const_numeric_cast<float32_t>(FRAME_SCALE))};
}

auto ModelInstance::Convert2dTo3d(ipos32 pos) const -> vec3
{
    FO_STACK_TRACE_ENTRY();

    const int32_t viewport[4] = {0, 0, _frameSize.width, _frameSize.height};
    const auto xf = numeric_cast<float32_t>(pos.x) * numeric_cast<float32_t>(FRAME_SCALE);
    const auto yf = numeric_cast<float32_t>(pos.y) * numeric_cast<float32_t>(FRAME_SCALE);
    vec3 out {};
    const mat44 identity {1.0f};

    if (!UnprojectPoint(vec3 {xf, numeric_cast<float32_t>(_frameSize.height) - yf, 0.0f}, identity, _frameProj, viewport, out)) {
        return {};
    }

    out.z = 0.0f;
    return out;
}

auto ModelInstance::ProjectPoint(vec3 obj_pos, const mat44& model_matrix, const mat44& proj_matrix, const int32_t viewport[4], vec3& out_pos) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const glm::vec<4, float32_t, glm::defaultp> clip_pos = proj_matrix * model_matrix * glm::vec<4, float32_t, glm::defaultp> {obj_pos.x, obj_pos.y, obj_pos.z, 1.0f};

    if (clip_pos.w == 0.0f) {
        return false;
    }

    const vec3 ndc_pos {clip_pos.x / clip_pos.w, clip_pos.y / clip_pos.w, clip_pos.z / clip_pos.w};

    out_pos.x = (ndc_pos.x * 0.5f + 0.5f) * numeric_cast<float32_t>(viewport[2]) + numeric_cast<float32_t>(viewport[0]);
    out_pos.y = (ndc_pos.y * 0.5f + 0.5f) * numeric_cast<float32_t>(viewport[3]) + numeric_cast<float32_t>(viewport[1]);
    out_pos.z = ndc_pos.z * 0.5f + 0.5f;
    return true;
}

auto ModelInstance::UnprojectPoint(vec3 win_pos, const mat44& model_matrix, const mat44& proj_matrix, const int32_t viewport[4], vec3& out_pos) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const float32_t ndc_x = (win_pos.x - numeric_cast<float32_t>(viewport[0])) / numeric_cast<float32_t>(viewport[2]) * 2.0f - 1.0f;
    const float32_t ndc_y = (win_pos.y - numeric_cast<float32_t>(viewport[1])) / numeric_cast<float32_t>(viewport[3]) * 2.0f - 1.0f;
    const float32_t ndc_z = win_pos.z * 2.0f - 1.0f;
    const mat44 clip_to_model = glm::inverse(proj_matrix * model_matrix);
    const glm::vec<4, float32_t, glm::defaultp> obj_pos = clip_to_model * glm::vec<4, float32_t, glm::defaultp> {ndc_x, ndc_y, ndc_z, 1.0f};

    if (obj_pos.w == 0.0f) {
        return false;
    }

    out_pos = vec3 {obj_pos.x / obj_pos.w, obj_pos.y / obj_pos.w, obj_pos.z / obj_pos.w};
    return true;
}

auto ModelInstance::MakeRootTransformation(ipos32 pos, float32_t scale, bool direct_scene) const -> mat44
{
    FO_STACK_TRACE_ENTRY();

    const vec3 pos3d = direct_scene ? vec3 {} : Convert2dTo3d(pos);
    const auto mat_scale = glm::scale(mat44 {1.0f}, vec3 {scale, scale, scale});
    const auto mat_rot_y = glm::rotate(mat44 {1.0f}, (_moveDirAngle + (_isMovingBack ? 180.0f : 0.0f)) * DEG_TO_RAD_FLOAT, vec3 {0.0f, 1.0f, 0.0f});
    const auto mat_trans = glm::translate(mat44 {1.0f}, pos3d);
    const mat44 mat_camera_tilt = direct_scene ? mat44 {1.0f} : _matRot;
    return mat_trans * _matTransBase * mat_camera_tilt * mat_rot_y * _matRotBase * mat_scale * _matScale * _matScaleBase;
}

auto ModelInstance::NeedDraw() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetTime() - _lastDrawTime >= std::chrono::milliseconds(_modelMngr->_animUpdateThreshold);
}

void ModelInstance::PoseForSpriteFrame(bool advance_animation)
{
    FO_STACK_TRACE_ENTRY();

    // Pose the model into its sprite frame without rendering. GetSpriteBounds derives the frame extent from the posed
    // skeleton and the baked particle box, never from rendered pixels, so DrawModelToAtlas sizes the frame from this
    // pose and only then renders once (RenderSpriteFrame) at the final size - it never renders just to measure.
    _drawProj = _frameProj;
    _directSceneDraw = false;
    PoseModel(const_numeric_cast<float32_t>(FRAME_SCALE), advance_animation);
}

void ModelInstance::RenderSpriteFrame()
{
    FO_STACK_TRACE_ENTRY();

    // Render the pose established by the preceding PoseForSpriteFrame into the currently bound sprite render target.
    RenderModel(true);
}

void ModelInstance::Draw(const mat44& proj, float32_t scale)
{
    FO_STACK_TRACE_ENTRY();

    DrawFrame(proj, scale, true, true, true);
}

void ModelInstance::DrawFrame(const mat44& proj, float32_t scale, bool direct_scene, bool draw_particles, bool advance_animation)
{
    FO_STACK_TRACE_ENTRY();

    _drawProj = proj;
    _directSceneDraw = direct_scene;
    const auto restore_direct_scene = scope_exit([this]() noexcept { _directSceneDraw = false; });

    PoseModel(scale, advance_animation);
    RenderModel(draw_particles);
}

void ModelInstance::PoseModel(float32_t scale, bool advance_animation)
{
    FO_STACK_TRACE_ENTRY();

    _spriteBoundsPoseReady = false;

    // Advance time only on a fresh logical frame. The atlas frame-sizing loop poses the model up to a few times to
    // converge on a stable frame size; those re-poses must not step the animation, so each frame-size measurement is
    // taken against one stable pose and the animation advances once per displayed frame. Truncating the delta to whole
    // milliseconds used to freeze the pose across re-poses by accident; a full-resolution delta does not, so they are
    // frozen explicitly here.
    float32_t dt = 0.0f;

    if (advance_animation) {
        const auto time = GetTime();

        // Full-resolution delta: truncating to whole milliseconds drops sub-millisecond frames, so on an uncapped
        // viewer running at a very high frame rate the animation never accumulates time and looks frozen until a
        // slower frame crosses the 1 ms boundary.
        dt = numeric_cast<float32_t>((time - _lastDrawTime).nanoseconds()) * 1e-9f;
        _lastDrawTime = time;
    }

    _forceDraw = false;

    // Move animation
    ProcessAnimation(dt, _framePivot, scale);

    _spriteBoundsPoseReady = !_directSceneDraw;
}

void ModelInstance::RenderModel(bool draw_particles)
{
    FO_STACK_TRACE_ENTRY();

    if (_actualCombinedMeshesCount != 0) {
        for (size_t i = 0; i < _actualCombinedMeshesCount; i++) {
            DrawCombinedMesh(_combinedMeshes[i], _shadowDisabled || _modelInfo->_shadowDisabled);
        }
    }

    if (draw_particles) {
        DrawAllParticles();
    }
}

void ModelInstance::DrawCombinedMesh(ptr<CombinedMesh> combined_mesh, bool shadow_disabled)
{
    FO_STACK_TRACE_ENTRY();

    auto effect = combined_mesh->DrawEffect ? combined_mesh->DrawEffect : _modelMngr->_effectMngr->Effects.SkinnedModel;
    FO_VERIFY_AND_THROW(effect, "Combined mesh has no draw effect");

    auto& proj_buf = effect->ProjBuf = RenderEffect::ProjBuffer();
    ptr<float32_t> proj_matrix = proj_buf->ProjMatrix;
    auto draw_projection_values = make_ptr(glm::value_ptr(_drawProj));
    MemCopy(proj_matrix, draw_projection_values, 16 * sizeof(float32_t));

    if (combined_mesh->Textures[0]) {
        effect->MainTex = combined_mesh->Textures[0]->MainTex;
    }
    else {
        effect->MainTex = nullptr;
    }

    auto& model_buf = effect->ModelBuf = RenderEffect::ModelBuffer();
    constexpr size_t MATRIX_VALUE_COUNT = 16;
    constexpr size_t WORLD_MATRIX_VALUE_COUNT = MODEL_MAX_BONES * MATRIX_VALUE_COUNT;
    ptr<float32_t> world_matrices_values = model_buf->WorldMatrices;
    span<float32_t> world_matrices = make_span(world_matrices_values, WORLD_MATRIX_VALUE_COUNT);

    for (size_t i = 0; i < combined_mesh->CurBoneMatrix; i++) {
        const SkinBinding& binding = combined_mesh->SkinBindings[i];
        FO_VERIFY_AND_THROW(binding.Owner && binding.SourceBone, "Combined mesh contains an incomplete skin binding", i, combined_mesh->CurBoneMatrix);
        const auto m = binding.Owner->GetWorldMatrix(binding.JointIndex) * binding.InverseBindMatrix;
        const size_t matrix_offset = i * MATRIX_VALUE_COUNT;
        span<float32_t> world_matrix = world_matrices.subspan(matrix_offset, MATRIX_VALUE_COUNT);
        auto source_matrix_values = make_ptr(glm::value_ptr(m));
        const_span<float32_t> source_matrix = make_span(source_matrix_values, MATRIX_VALUE_COUNT);
        std::ranges::copy(source_matrix, world_matrix.begin());
    }

    effect->MatrixCount = combined_mesh->CurBoneMatrix;

    ptr<float32_t> ground_position = model_buf->GroundPosition;
    auto ground_position_values = make_ptr(glm::value_ptr(_groundPos));
    MemCopy(ground_position, ground_position_values, 3 * sizeof(float32_t));
    model_buf->GroundPosition[3] = 0.0f;

    ptr<float32_t> light_color = model_buf->LightColor;
    auto light_color_values = make_ptr(glm::value_ptr(_modelMngr->_lightColor));
    MemCopy(light_color, light_color_values, 4 * sizeof(float32_t));

    if (effect->IsNeedModelTexBuf()) {
        auto& custom_tex_buf = effect->ModelTexBuf = RenderEffect::ModelTexBuffer();

        for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
            if (combined_mesh->Textures[i]) {
                effect->ModelTex[i] = combined_mesh->Textures[i]->MainTex;
                const size_t texture_uniform_offset = i * 4 * sizeof(float32_t);
                MemCopy(&custom_tex_buf->TexAtlasOffset[texture_uniform_offset], &combined_mesh->Textures[i]->AtlasOffsetData, 4 * sizeof(float32_t));

                auto texture_size = make_ptr(&custom_tex_buf->TexSize[texture_uniform_offset]);
                ptr<const float32_t> texture_size_data = combined_mesh->Textures[i]->MainTex->SizeData;
                MemCopy(texture_size, texture_size_data, 4 * sizeof(float32_t));
            }
            else {
                effect->ModelTex[i] = nullptr;
            }
        }
    }

    if (effect->IsNeedModelAnimBuf()) {
        auto& anim_buf = effect->ModelAnimBuf = RenderEffect::ModelAnimBuffer();

        anim_buf->AnimNormalizedTime[0] = _animPosProc;
        anim_buf->AnimAbsoluteTime[0] = _animPosTime;
    }

    effect->DisableCulling = _disableCulling;
    effect->DisableShadow = shadow_disabled || _directSceneDraw;

    combined_mesh->MeshBuf->Upload(effect->GetUsage());
    effect->DrawBuffer(combined_mesh->MeshBuf);
}

void ModelInstance::DrawAllParticles()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& model_particle : _modelParticles) {
        model_particle.Particle->Draw();
    }

    for (size_t i = 0; i != _children.size(); ++i) {
        _children[i]->DrawAllParticles();
    }
}

auto ModelInstance::GetBonePos(hstring bone_name) const -> optional<ipos32>
{
    FO_STACK_TRACE_ENTRY();

    const auto binding = FindPoseJoint(bone_name);

    if (!binding) {
        return std::nullopt;
    }

    vec3 pos {};
    quaternion rot {};
    vec3 scale {};
    vec3 skew {};
    glm::vec<4, float32_t, glm::defaultp> perspective {};
    FO_VERIFY_AND_THROW(binding->Owner, "Resolved model pose joint has no owner", bone_name);
    glm::decompose(binding->Owner->GetWorldMatrix(binding->JointIndex), scale, rot, pos, skew, perspective);

    const auto p = Convert3dTo2d(pos);
    // Convert3dTo2d gives a sprite-space point measured from the bottom, so the origin's row from the bottom is
    // (frame_height - pivot.y). The bone offset is taken relative to the exact origin pivot, not a fixed fraction.
    const int32_t frame_height = _frameSize.height / FRAME_SCALE;
    const auto x = p.x - _framePivot.x;
    const auto y = -(p.y - (frame_height - _framePivot.y));

    return ipos32 {x, y};
}

auto ModelInstance::GetBoneSpritePos(hstring bone_name) const -> optional<ipos32>
{
    FO_STACK_TRACE_ENTRY();

    const auto binding = FindPoseJoint(bone_name);

    if (!binding) {
        return std::nullopt;
    }
    if (_frameSize.width <= 0 || _frameSize.height <= 0 || _frameSize.width % FRAME_SCALE != 0 || _frameSize.height % FRAME_SCALE != 0) {
        return std::nullopt;
    }

    const auto bounds = GetSpriteBounds();

    if (!bounds) {
        return std::nullopt;
    }

    FO_VERIFY_AND_THROW(binding->Owner, "Resolved model pose joint has no owner", bone_name);
    const mat44& world = binding->Owner->GetWorldMatrix(binding->JointIndex);
    const vec3 bone_world = {world[3][0], world[3][1], world[3][2]};

    const int32_t viewport[4] = {0, 0, _frameSize.width, _frameSize.height};
    const mat44 identity {1.0f};
    vec3 projected {};

    if (!ProjectPoint(bone_world, identity, _frameProj, viewport, projected) || !std::isfinite(projected.x) || !std::isfinite(projected.y)) {
        return std::nullopt;
    }

    // Match GetSpriteBounds' sprite-space convention (Y measured from the frame
    // bottom, then divided down by the render supersample) so the point lands in
    // the same logical frame the crop rect is taken from, then localise it to
    // the cropped sprite by subtracting the crop origin.
    const float32_t frame_scale = const_numeric_cast<float32_t>(FRAME_SCALE);
    const int32_t sprite_x = iround<int32_t>(projected.x / frame_scale);
    const int32_t sprite_y = iround<int32_t>((numeric_cast<float32_t>(_frameSize.height) - projected.y) / frame_scale);

    return ipos32 {sprite_x - bounds->Rect.x, sprite_y - bounds->Rect.y};
}

FO_END_NAMESPACE

#endif
