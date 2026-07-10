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

#include "3dStuff.h"

#if FO_ENABLE_3D

#include "Application.h"
#include "EngineBase.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

static auto LoadModelBone(DataReader& reader, HashResolver& hash_resolver) -> unique_ptr<ModelBone>;
static void FixModelBoneAfterLoad(ptr<ModelBone> bone, ptr<ModelBone> root_bone);
static auto FindModelBone(ptr<const ModelBone> bone, hstring bone_name) noexcept -> nptr<const ModelBone>;
static auto FindModelBone(ptr<ModelBone> bone, hstring bone_name) noexcept -> nptr<ModelBone>;
static auto FindModelBone(nptr<const ModelBone> bone, hstring bone_name) noexcept -> nptr<const ModelBone>;
static auto FindModelBone(nptr<ModelBone> bone, hstring bone_name) noexcept -> nptr<ModelBone>;
static auto AddModelBoneChild(ptr<ModelBone> bone, unique_ptr<ModelBone> child) -> ptr<ModelBone>;
static auto GetModelBoneAttachedMesh(ptr<const ModelBone> bone) noexcept -> nptr<const MeshData>;
static auto GetModelBoneAttachedMesh(ptr<ModelBone> bone) noexcept -> nptr<MeshData>;
static void RegisterModelBoneAnimationOutput(ptr<ModelAnimationController> anim_controller, ptr<ModelBone> bone);

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
    auto root_bone = LoadModelBone(reader, *_hashResolver);

    FixModelBoneAfterLoad(root_bone, root_bone);

    // Load animations
    const auto anim_count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < anim_count; i++) {
        auto anim = SafeAlloc::MakeUnique<ModelAnimation>();
        anim->Load(reader, *_hashResolver);
        _loadedAnims.push_back(std::move(anim));
    }

    reader.VerifyEnd();

    // Add to collection
    root_bone->Name = name_hashed;
    _loadedModels.emplace_back(std::move(root_bone));
    return _loadedModels.back();
}

auto ModelManager::LoadAnimation(string_view anim_fname, string_view anim_name) -> nptr<ModelAnimation>
{
    FO_STACK_TRACE_ENTRY();

    // Find in already loaded
    const auto take_first = anim_name == "Base";
    const auto name_hashed = _hashResolver->ToHashedString(anim_fname);

    for (size_t i = 0; i != _loadedAnims.size(); ++i) {
        auto anim = _loadedAnims[i].as_ptr();

        if (strex(anim->GetFileName()).compare_ignore_case(anim_fname) && (take_first || strex(anim->GetName()).compare_ignore_case(anim_name))) {
            return anim;
        }
    }

    // Check maybe file already processed and nothing founded
    if (_processedFiles.count(name_hashed) != 0) {
        return nullptr;
    }

    // File not processed, load and recheck animations
    const auto root_bone = LoadModel(anim_fname);

    if (root_bone) {
        return LoadAnimation(anim_fname, anim_name);
    }

    _processedFiles.emplace(name_hashed);
    return nullptr;
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
        auto mesh = GetModelBoneAttachedMesh(model_info->_hierarchy->_allDrawBones[i]);
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
    _forceDraw = true;
    _lastDrawTime = GetTime();
    SetupFrame(_modelInfo->_drawSize);
}

void ModelInstance::SetupFrame(isize32 draw_size)
{
    _frameSize.width = draw_size.width * FRAME_SCALE;
    _frameSize.height = draw_size.height * FRAME_SCALE;

    // Projection
    const auto frame_ratio = numeric_cast<float32_t>(_frameSize.width) / numeric_cast<float32_t>(_frameSize.height);
    const auto proj_height = numeric_cast<float32_t>(_frameSize.height) * (1.0f / _modelMngr->_settings->ModelProjFactor);
    const auto proj_width = proj_height * frame_ratio;

    _frameProj = _modelMngr->_render->CreateOrthoMatrix(0.0f, proj_width, 0.0f, proj_height, -10.0f, 10.0f);
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

        if (_parentBone) {
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

        if (_parentBone) {
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
                            const auto to_bone = FindBone(link.LinkBone);
                            FO_VERIFY_AND_THROW(to_bone, "Particle model link target bone not found");

                            optional<ParticleSystem> particle = _modelMngr->_particleMngr.CreateParticle(link.ChildName);
                            FO_VERIFY_AND_THROW(particle, "Particle was not found for a model link", link.ChildName);
                            _modelParticles.emplace_back(ModelParticleSystem {link.Id, SafeAlloc::MakeUnique<ParticleSystem>(std::move(*particle)), to_bone, vec3(link.MoveX, link.MoveY, link.MoveZ), link.RotY});
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
                                auto to_bone = FindModelBone(_modelInfo->_hierarchy->_rootBone, link.LinkBone);
                                FO_VERIFY_AND_THROW(to_bone, "Model link target bone not found");

                                auto model = create_child_model();

                                mesh_changed = true;
                                model->_parent = this;
                                model->_parentBone = to_bone;
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

                                for (auto& child_bone : model->_modelInfo->_hierarchy->_allBones) {
                                    auto root_bone = FindModelBone(_modelInfo->_hierarchy->_rootBone, child_bone->Name);

                                    if (root_bone) {
                                        model->_linkBones.emplace_back(root_bone);
                                        model->_linkBones.emplace_back(child_bone);
                                    }
                                }

                                FO_VERIFY_AND_THROW(!model->_linkBones.empty(), "Child model has no common bones for a model link", link.ChildName);

                                mesh_changed = true;
                                model->_parent = this;
                                model->_parentBone = _modelInfo->_hierarchy->_rootBone;
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

    RefreshMoveAnimation();

    if (_bodyAnimController && anim_index >= 0) {
        const auto new_track = _curTrack == 0 ? 1 : 0;
        _animDuration = _bodyAnimController->GetAnimationDuration(anim_index);

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
        _bodyAnimController->AdvanceTime(0.0f);

        if ((_isMoving || _turnAnimPlaying) && _moveAnimController) {
            _moveAnimController->AdvanceTime(0.0f);
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
    if (!_parentBone && mesh_changed) {
        GenerateCombinedMeshes();
    }

    return mesh_changed;
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
            const auto anim_duration = _moveAnimController->GetAnimationDuration(anim_index);
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

auto ModelInstance::GetViewSize() const -> isize32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto draw_width_scale = numeric_cast<float32_t>(_frameSize.width / FRAME_SCALE) / numeric_cast<float32_t>(_modelInfo->_drawSize.width);
    const auto draw_height_scale = numeric_cast<float32_t>(_frameSize.height / FRAME_SCALE) / numeric_cast<float32_t>(_modelInfo->_drawSize.height);
    const auto view_width = iround<int32_t>(numeric_cast<float32_t>(_modelInfo->_viewSize.width) * draw_width_scale);
    const auto view_height = iround<int32_t>(numeric_cast<float32_t>(_modelInfo->_viewSize.height) * draw_height_scale);

    return {view_width, view_height};
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
                FO_VERIFY_AND_THROW(_parent != nullptr, "Parent texture was requested without a parent model", tex_name);

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
                FO_VERIFY_AND_THROW(_parent != nullptr, "Parent effect was requested without a parent model", std::get<0>(eff_info));

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
}

void ModelInstance::SetScale(float32_t sx, float32_t sy, float32_t sz)
{
    FO_STACK_TRACE_ENTRY();

    _matScale = glm::scale(mat44 {1.0f}, vec3 {sx, sy, sz});
}

void ModelInstance::SetSpeed(float32_t speed)
{
    FO_STACK_TRACE_ENTRY();

    _speedAdjustBase = speed;
}

void ModelInstance::GenerateCombinedMeshes()
{
    FO_STACK_TRACE_ENTRY();

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
    }

    _actualCombinedMeshesCount = 0;

    // Combine meshes recursively
    FillCombinedMeshes(this);

    // Cut
    _disableCulling = false;
    CutCombinedMeshes(this);

    // Finalize meshes
    for (size_t i = 0; i < _actualCombinedMeshesCount; i++) {
        _combinedMeshes[i]->MeshBuf->StaticDataChanged = true;
    }
}

void ModelInstance::FillCombinedMeshes(ptr<const ModelInstance> cur)
{
    FO_STACK_TRACE_ENTRY();

    // Combine meshes
    for (size_t i = 0; i < cur->_allMeshes.size(); i++) {
        CombineMesh(cur->_allMeshes[i], cur->_parentBone ? cur->_animLink.Layer : 0);
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
        .SkinBones = vector<nptr<ModelBone>>(MODEL_MAX_BONES),
        .SkinBoneOffsets = vector<mat44>(MODEL_MAX_BONES),
    });
}

void ModelInstance::CombineMesh(ptr<const MeshInstance> mesh_instance, int32_t anim_layer)
{
    FO_STACK_TRACE_ENTRY();

    // Skip disabled meshes
    if (mesh_instance->Disabled) {
        return;
    }

    // Try to encapsulate mesh instance to current combined mesh
    for (size_t i = 0; i < _actualCombinedMeshesCount; i++) {
        if (CanBatchCombinedMesh(_combinedMeshes[i], mesh_instance)) {
            BatchCombinedMesh(_combinedMeshes[i], mesh_instance, anim_layer);
            return;
        }
    }

    // Create new combined mesh
    if (_actualCombinedMeshesCount >= _combinedMeshes.size()) {
        _combinedMeshes.emplace_back(CreateCombinedMesh());
    }

    BatchCombinedMesh(_combinedMeshes[_actualCombinedMeshesCount], mesh_instance, anim_layer);
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
    return combined_mesh->CurBoneMatrix + mesh_instance->Mesh->SkinBones.size() <= combined_mesh->SkinBones.size();
}

void ModelInstance::BatchCombinedMesh(ptr<CombinedMesh> combined_mesh, ptr<const MeshInstance> mesh_instance, int32_t anim_layer)
{
    FO_STACK_TRACE_ENTRY();

    ptr<MeshData> mesh_data = mesh_instance->Mesh.get_no_const();
    auto& vertices = combined_mesh->MeshBuf->Vertices3D;
    auto& indices = combined_mesh->MeshBuf->Indices;
    const auto vertices_old_size = vertices.size();
    const auto indices_old_size = indices.size();

    // Set or add data
    if (combined_mesh->EncapsulatedMeshCount == 0) {
        vertices = mesh_data->Vertices;
        indices = mesh_data->Indices;
        combined_mesh->DrawEffect = mesh_instance->CurEffect;
        std::ranges::for_each(combined_mesh->SkinBones, [](auto&& bone) { bone = nullptr; });
        std::ranges::for_each(combined_mesh->Textures, [](auto&& tex) { tex = nullptr; });
        combined_mesh->CurBoneMatrix = 0;
    }
    else {
        vertices.insert(vertices.end(), mesh_data->Vertices.begin(), mesh_data->Vertices.end());
        indices.insert(indices.end(), mesh_data->Indices.begin(), mesh_data->Indices.end());

        // Add indices offset
        const auto index_offset = numeric_cast<vindex_t>(vertices.size() - mesh_data->Vertices.size());
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
    combined_mesh->MeshVertices.emplace_back(numeric_cast<uint32_t>(vertices.size() - vertices_old_size));
    combined_mesh->MeshIndices.emplace_back(numeric_cast<uint32_t>(indices.size() - indices_old_size));
    combined_mesh->MeshAnimLayers.emplace_back(anim_layer);

    // Add bones matrices
    for (size_t i = 0; i < mesh_data->SkinBones.size(); i++) {
        combined_mesh->SkinBones[combined_mesh->CurBoneMatrix + i] = mesh_data->SkinBones[i];
        combined_mesh->SkinBoneOffsets[combined_mesh->CurBoneMatrix + i] = mesh_data->SkinBoneOffsets[i];
    }

    combined_mesh->CurBoneMatrix += mesh_data->SkinBones.size();

    // Add textures
    for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
        if (!combined_mesh->Textures[i] && mesh_instance->CurTexures[i]) {
            combined_mesh->Textures[i] = mesh_instance->CurTexures[i];
        }
    }

    // Fix texture coords
    if (mesh_instance->CurTexures[0]) {
        auto mesh_tex = mesh_instance->CurTexures[0];
        FO_VERIFY_AND_THROW(mesh_tex, "Mesh texture is null");

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
    }

    // Unskin
    if (cut->UnskinBone1 && cut->UnskinBone2) {
        // Find unskin bones
        nptr<ModelBone> unskin_bone1;
        float32_t unskin_bone1_index = 0.0f;
        nptr<ModelBone> unskin_bone2;
        float32_t unskin_bone2_index = 0.0f;

        for (size_t i = 0; i < combined_mesh->CurBoneMatrix; i++) {
            if (combined_mesh->SkinBones[i]->Name == cut->UnskinBone1) {
                unskin_bone1 = combined_mesh->SkinBones[i];
                unskin_bone1_index = numeric_cast<float32_t>(i);
            }
            if (combined_mesh->SkinBones[i]->Name == cut->UnskinBone2) {
                unskin_bone2 = combined_mesh->SkinBones[i];
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
                        bool influence_side = !!FindModelBone(unskin_bone1, combined_mesh->SkinBones[iround<int32_t>(v.BlendIndices[b])]->Name);

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

auto ModelInstance::NeedDraw() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetTime() - _lastDrawTime >= std::chrono::milliseconds(_modelMngr->_animUpdateThreshold);
}

void ModelInstance::Draw()
{
    FO_STACK_TRACE_ENTRY();

    DrawFrame(_frameProj, const_numeric_cast<float32_t>(FRAME_SCALE), false, true);
}

void ModelInstance::Draw(const mat44& proj, float32_t scale)
{
    FO_STACK_TRACE_ENTRY();

    DrawFrame(proj, scale, true, true);
}

void ModelInstance::DrawFrame(const mat44& proj, float32_t scale, bool direct_scene, bool draw_particles)
{
    FO_STACK_TRACE_ENTRY();

    _drawProj = proj;
    _directSceneDraw = direct_scene;
    const auto restore_direct_scene = scope_exit([this]() noexcept { _directSceneDraw = false; });

    const auto time = GetTime();
    const auto dt = 0.001f * (time - _lastDrawTime).to_ms<float32_t>();

    _lastDrawTime = time;
    _forceDraw = false;

    // Move animation
    const auto w = _frameSize.width / FRAME_SCALE;
    const auto h = _frameSize.height / FRAME_SCALE;
    ProcessAnimation(dt, {w / 2, h - h / 4}, scale);

    if (_actualCombinedMeshesCount != 0) {
        for (size_t i = 0; i < _actualCombinedMeshesCount; i++) {
            DrawCombinedMesh(_combinedMeshes[i], _shadowDisabled || _modelInfo->_shadowDisabled);
        }
    }

    if (draw_particles) {
        DrawAllParticles();
    }
}

void ModelInstance::ProcessAnimation(float32_t elapsed, ipos32 pos, float32_t scale)
{
    FO_STACK_TRACE_ENTRY();

    // Update world matrix, only for root
    if (!_parentBone) {
        const vec3 pos3d = _directSceneDraw ? vec3 {} : Convert2dTo3d(pos);
        const auto mat_scale = glm::scale(mat44 {1.0f}, vec3 {scale, scale, scale});
        const auto mat_rot_y = glm::rotate(mat44 {1.0f}, (_moveDirAngle + (_isMovingBack ? 180.0f : 0.0f)) * DEG_TO_RAD_FLOAT, vec3 {0.0f, 1.0f, 0.0f});
        const auto mat_trans = glm::translate(mat44 {1.0f}, pos3d);
        const mat44 mat_camera_tilt = _directSceneDraw ? mat44 {1.0f} : _matRot;

        _parentMatrix = mat_trans * _matTransBase * mat_camera_tilt * mat_rot_y * _matRotBase * mat_scale * _matScale * _matScaleBase;
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

        _bodyAnimController->AdvanceTime(elapsed * GetSpeed());

        if ((_isMoving || _turnAnimPlaying) && _moveAnimController) {
            _moveAnimController->AdvanceTime(elapsed * GetMovementSpeed());

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

    // Update matrices
    UpdateBoneMatrices(_modelInfo->_hierarchy->_rootBone, &_parentMatrix);

    // Update linked matrices
    if (_parentBone && !_linkBones.empty()) {
        const auto link_bones_count = _linkBones.size() / 2;

        for (size_t i = 0; i < link_bones_count; i++) {
            _linkBones[i * 2 + 1]->CombinedTransformationMatrix = _linkBones[i * 2]->CombinedTransformationMatrix;
        }
    }

    // Update world matrices for children
    for (size_t i = 0; i != _children.size(); ++i) {
        auto child = _children[i].as_ptr();

        child->_groundPos = _groundPos;
        child->_parentMatrix = child->_parentBone->CombinedTransformationMatrix * child->_matTransBase * child->_matRotBase * child->_matScaleBase;
    }

    for (auto& model_particle : _modelParticles) {
        const mat44& proj = _directSceneDraw ? _drawProj : _frameProj;
        const vec3 view_offset = _directSceneDraw ? vec3 {} : _moveOffset;
        const bool tilt_in_proj = _directSceneDraw;

        if (model_particle.Id == 0) {
            model_particle.Particle->Setup(proj, model_particle.Bone->CombinedTransformationMatrix, model_particle.Move, model_particle.Rot, view_offset, tilt_in_proj);
        }
        else {
            model_particle.Particle->Setup(proj, model_particle.Bone->CombinedTransformationMatrix, model_particle.Move, model_particle.Rot + _lookDirAngle, view_offset, tilt_in_proj);
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

void ModelInstance::UpdateBoneMatrices(ptr<ModelBone> bone, ptr<const mat44> parent_matrix)
{
    FO_STACK_TRACE_ENTRY();

    if (_modelInfo->_rotationBone && bone->Name == _modelInfo->_rotationBone && !is_float_equal(_lookDirAngle, _moveDirAngle)) {
        const auto mat_rot = glm::rotate(mat44 {1.0f}, (GeometryHelper::GetDirAngleDiffSided(_lookDirAngle + (_isMovingBack ? 180.0f : 0.0f), _moveDirAngle) * -_modelMngr->_settings->CritterBodyTurnFactor) * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});
        bone->CombinedTransformationMatrix = *parent_matrix * mat_rot * bone->TransformationMatrix;
    }
    else if (_modelInfo->_rotationBone && bone->Name == _modelMngr->_headBone && !is_float_equal(_lookDirAngle, _moveDirAngle)) {
        const auto mat_rot = glm::rotate(mat44 {1.0f}, (GeometryHelper::GetDirAngleDiffSided(_lookDirAngle + (_isMovingBack ? 180.0f : 0.0f), _moveDirAngle) * -_modelMngr->_settings->CritterHeadTurnFactor) * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});
        bone->CombinedTransformationMatrix = *parent_matrix * mat_rot * bone->TransformationMatrix;
    }
    else {
        bone->CombinedTransformationMatrix = *parent_matrix * bone->TransformationMatrix;
    }

    // Update child
    for (size_t i = 0; i != bone->Children.size(); ++i) {
        UpdateBoneMatrices(bone->Children[i], &bone->CombinedTransformationMatrix);
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
        const auto m = combined_mesh->SkinBones[i]->CombinedTransformationMatrix * combined_mesh->SkinBoneOffsets[i];
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

auto ModelInstance::FindBone(hstring bone_name) const noexcept -> nptr<const ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    auto bone = FindModelBone(_modelInfo->_hierarchy->_rootBone, bone_name);

    if (!bone) {
        for (size_t i = 0; i < _children.size(); i++) {
            bone = FindModelBone(_children[i]->_modelInfo->_hierarchy->_rootBone, bone_name);

            if (bone) {
                break;
            }
        }
    }

    return bone;
}

auto ModelInstance::GetBonePos(hstring bone_name) const -> optional<ipos32>
{
    FO_STACK_TRACE_ENTRY();

    const auto bone = FindBone(bone_name);

    if (!bone) {
        return std::nullopt;
    }

    vec3 pos {};
    quaternion rot {};
    vec3 scale {};
    vec3 skew {};
    glm::vec<4, float32_t, glm::defaultp> perspective {};
    glm::decompose(bone->CombinedTransformationMatrix, scale, rot, pos, skew, perspective);

    const auto p = Convert3dTo2d(pos);
    const auto x = p.x - _frameSize.width / FRAME_SCALE / 2;
    const auto y = -(p.y - _frameSize.height / FRAME_SCALE / 4);

    return ipos32 {x, y};
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

    auto duration = _bodyAnimController->GetAnimationDuration(anim_index);

    if (speed > 0.0f) {
        duration /= speed;
    }

    return std::chrono::milliseconds(iround<int32_t>(duration * 1000.0f));
}

void ModelInstance::RunParticle(string_view particle_name, hstring bone_name, vec3 move)
{
    FO_STACK_TRACE_ENTRY();

    if (auto to_bone = FindBone(bone_name)) {
        if (optional<ParticleSystem> particle = _modelMngr->_particleMngr.CreateParticle(particle_name); particle) {
            _modelParticles.emplace_back(ModelParticleSystem {0, SafeAlloc::MakeUnique<ParticleSystem>(std::move(*particle)), to_bone, move, _lookDirAngle});
        }
    }
}

ModelInformation::ModelInformation(ptr<ModelManager> model_mngr) :
    _modelMngr {model_mngr}
{
    FO_STACK_TRACE_ENTRY();

    _drawSize.width = _modelMngr->_settings->DefaultModelDrawWidth;
    _drawSize.height = _modelMngr->_settings->DefaultModelDrawHeight;
    _viewSize.width = _modelMngr->_settings->DefaultModelViewWidth != 0 ? _modelMngr->_settings->DefaultModelViewWidth : _drawSize.width / 4;
    _viewSize.height = _modelMngr->_settings->DefaultModelViewHeight != 0 ? _modelMngr->_settings->DefaultModelViewHeight : _viewSize.height / 2;
}

auto ModelInformation::Load(string_view name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const string ext = strex(name).get_file_extension();

    if (ext.empty()) {
        return false;
    }

    // Load fonline 3d file
    if (ext == "fo3d") {
        // Load main fo3d file
        const auto fo3d = _modelMngr->_resources->ReadFile(name);

        if (!fo3d) {
            return false;
        }

        auto reader = DataReader(fo3d.GetDataSpan());
        FO_VERIFY_AND_THROW(LoadBaked(name, reader), "Failed to load baked 3D asset");
        return true;
    }

    // Load just model
    auto hierarchy = _modelMngr->GetHierarchy(name);

    if (!hierarchy) {
        return false;
    }

    // Create animation
    _fileName = name;
    _hierarchy = hierarchy;

    return true;
}

auto ModelInformation::LoadBaked(string_view name, DataReader& reader) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const string model = reader.ReadString();
    const bool disable_animation_interpolation = reader.Read<uint8_t>() != 0;
    _disableBackwardAnim = reader.Read<uint8_t>() != 0;
    _shadowDisabled = reader.Read<uint8_t>() != 0;

    const int32_t draw_width = reader.Read<int32_t>();
    const int32_t draw_height = reader.Read<int32_t>();
    const int32_t view_width = reader.Read<int32_t>();
    const int32_t view_height = reader.Read<int32_t>();

    FO_VERIFY_AND_THROW((draw_width == 0 && draw_height == 0) || (draw_width > 0 && draw_height > 0), "Baked model description contains invalid DrawSize", name, draw_width, draw_height);
    FO_VERIFY_AND_THROW((view_width == 0 && view_height == 0) || (view_width > 0 && view_height > 0), "Baked model description contains invalid ViewSize", name, view_width, view_height);

    if (draw_width != 0 && draw_height != 0) {
        _drawSize.width = draw_width;
        _drawSize.height = draw_height;
    }
    if (view_width != 0 && view_height != 0) {
        _viewSize.width = view_width;
        _viewSize.height = view_height;
    }

    const string rotation_bone = reader.ReadString();
    _rotationBone = !rotation_bone.empty() ? _modelMngr->GetBoneHashedString(rotation_bone) : hstring {};

    BakedModelDescriptionLink default_link = ReadBakedModelDescriptionLink(reader);

    const uint32_t links_count = reader.Read<uint32_t>();
    vector<BakedModelDescriptionLink> links;
    links.reserve(links_count);

    for (uint32_t i = 0; i < links_count; i++) {
        BakedModelDescriptionLink link = ReadBakedModelDescriptionLink(reader);
        FO_VERIFY_AND_THROW(link.Data.Layer >= 0 && link.Data.Layer < numeric_cast<int32_t>(MODEL_LAYERS_COUNT), "Baked model description contains a model link layer out of range", link.Data.Layer, name);
        FO_VERIFY_AND_THROW(link.Data.LayerValue != 0, "Baked model description contains a model link with zero layer value", name);
        links.emplace_back(std::move(link));
    }

    const uint32_t anim_entries_count = reader.Read<uint32_t>();
    vector<BakedModelDescriptionAnimEntry> anim_entries;
    anim_entries.reserve(anim_entries_count);

    for (uint32_t i = 0; i < anim_entries_count; i++) {
        anim_entries.emplace_back(ReadBakedModelDescriptionAnimEntry(reader));
    }

    const uint32_t anim_speed_count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < anim_speed_count; i++) {
        const int32_t state_anim = reader.Read<int32_t>();
        const int32_t action_anim = reader.Read<int32_t>();
        const float32_t speed = reader.Read<float32_t>();
        FO_VERIFY_AND_THROW(speed > 0.0f, "Baked model description contains a non-positive animation speed", state_anim, action_anim, name, speed);
        _animSpeed.emplace(std::make_pair(static_cast<CritterStateAnim>(state_anim), static_cast<CritterActionAnim>(action_anim)), speed);
    }

    const uint32_t anim_layer_values_count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < anim_layer_values_count; i++) {
        const BakedModelDescriptionAnimLayerValue value = ReadBakedModelDescriptionAnimLayerValue(reader);
        const auto index = std::make_pair(static_cast<CritterStateAnim>(value.StateAnim), static_cast<CritterActionAnim>(value.ActionAnim));
        FO_VERIFY_AND_THROW(value.Layer >= 0 && value.Layer < numeric_cast<int32_t>(MODEL_LAYERS_COUNT), "Baked model description contains an animation layer out of range", value.Layer, name);

        if (_animLayerValues.count(index) == 0) {
            _animLayerValues.emplace(index, vector<pair<int32_t, int32_t>>());
        }

        _animLayerValues[index].emplace_back(value.Layer, value.LayerValue);
    }

    const uint32_t fast_transition_bones_count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < fast_transition_bones_count; i++) {
        const string bone_name = reader.ReadString();
        _fastTransitionBones.insert(_modelMngr->GetBoneHashedString(bone_name));
    }

    const uint32_t state_anim_equals_count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < state_anim_equals_count; i++) {
        const int32_t from = reader.Read<int32_t>();
        const int32_t to = reader.Read<int32_t>();
        _stateAnimEquals.emplace(static_cast<CritterStateAnim>(from), static_cast<CritterStateAnim>(to));
    }

    const uint32_t action_anim_equals_count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < action_anim_equals_count; i++) {
        const int32_t from = reader.Read<int32_t>();
        const int32_t to = reader.Read<int32_t>();
        _actionAnimEquals.emplace(static_cast<CritterActionAnim>(from), static_cast<CritterActionAnim>(to));
    }

    reader.VerifyEnd();

    FO_VERIFY_AND_THROW(!model.empty(), "Baked model description has no Model section", name);

    auto hierarchy = _modelMngr->GetHierarchy(model);
    FO_VERIFY_AND_THROW(hierarchy, "Model hierarchy was not found for a baked model description", model, name);
    FO_VERIFY_AND_THROW(!hierarchy->_allDrawBones.empty(), "Model hierarchy has no drawable meshes for a baked model description", model, name);

    _fileName = name;
    _hierarchy = hierarchy;
    FO_VERIFY_AND_THROW(!_rotationBone || FindModelBone(_hierarchy->_rootBone, _rotationBone) != nullptr, "Rotation bone was not found in a baked model description", rotation_bone, name);

    for (const hstring bone_name : _fastTransitionBones) {
        FO_VERIFY_AND_THROW(FindModelBone(_hierarchy->_rootBone, bone_name) != nullptr, "Fast transition bone was not found in a baked model description", bone_name, name);
    }

    const auto append_cut_info = [this](ModelAnimationData& link, const BakedModelDescriptionCutInfo& raw_cut) {
        auto area = _modelMngr->GetHierarchy(raw_cut.FileName);
        FO_VERIFY_AND_THROW(area, "Cut file was not found", raw_cut.FileName);

        auto cut_holder = SafeAlloc::MakeUnique<ModelCutData>();
        auto cut = cut_holder.as_ptr();
        _cutData.emplace_back(std::move(cut_holder));

        link.CutInfo.emplace_back(cut);
        cut->Layers = raw_cut.Layers;
        cut->UnskinBone1 = !raw_cut.UnskinBone1.empty() ? _modelMngr->GetBoneHashedString(raw_cut.UnskinBone1) : hstring {};
        cut->UnskinBone2 = !raw_cut.UnskinBone2.empty() ? _modelMngr->GetBoneHashedString(raw_cut.UnskinBone2) : hstring {};
        cut->RevertUnskinShape = raw_cut.RevertUnskinShape;

        hstring unskin_shape_name;

        if (cut->UnskinBone1 && cut->UnskinBone2 && !raw_cut.UnskinShape.empty()) {
            unskin_shape_name = _modelMngr->GetBoneHashedString(raw_cut.UnskinShape);
            bool unskin_shape_found = false;

            for (ptr<ModelBone> bone : area->_allDrawBones) {
                if (unskin_shape_name == bone->Name) {
                    cut->UnskinShape = CreateCutShape(GetModelBoneAttachedMesh(bone));
                    unskin_shape_found = true;
                    break;
                }
            }

            FO_VERIFY_AND_THROW(unskin_shape_found, "Unskin shape was not found in a cut file", raw_cut.UnskinShape, raw_cut.FileName);
        }

        for (const string& shape : raw_cut.Shapes) {
            const hstring shape_name = !shape.empty() ? _modelMngr->GetBoneHashedString(shape) : hstring {};
            bool shape_found = false;

            for (ptr<ModelBone> bone : area->_allDrawBones) {
                if ((!shape_name || shape_name == bone->Name) && bone->Name != unskin_shape_name) {
                    cut->Shapes.emplace_back(CreateCutShape(GetModelBoneAttachedMesh(bone)));
                    shape_found = true;
                }
            }

            FO_VERIFY_AND_THROW(shape_found, "Cut shape was not found in a cut file", !shape.empty() ? shape : string {"All"}, raw_cut.FileName);
        }

        FO_VERIFY_AND_THROW(!cut->Shapes.empty(), "Cut file produced no cut shapes", raw_cut.FileName);
    };

    _animDataDefault = std::move(default_link.Data);

    for (const BakedModelDescriptionCutInfo& cut : default_link.CutInfo) {
        append_cut_info(_animDataDefault, cut);
    }

    _animData.reserve(links.size());

    for (BakedModelDescriptionLink& link : links) {
        link.Data.Id = ++_modelMngr->_linkId;

        for (const BakedModelDescriptionCutInfo& cut : link.CutInfo) {
            append_cut_info(link.Data, cut);
        }

        _animData.emplace_back(std::move(link.Data));
    }

    if (!anim_entries.empty()) {
        _animController.emplace(2);
    }

    if (_animController) {
        for (const BakedModelDescriptionAnimEntry& anim_entry : anim_entries) {
            const auto anim_pair = std::make_pair(static_cast<CritterStateAnim>(anim_entry.StateAnim), static_cast<CritterActionAnim>(anim_entry.ActionAnim));

            if (_animIndexes.count(anim_pair) != 0) {
                continue;
            }

            string anim_path;

            if (anim_entry.FileName == "ModelFile") {
                anim_path = model;
            }
            else {
                anim_path = strex(name).extract_dir().combine_path(anim_entry.FileName);
            }

            const bool reversed = anim_entry.Name.starts_with('~');
            const string anim_name = reversed ? anim_entry.Name.substr(1) : anim_entry.Name;
            auto anim = _modelMngr->LoadAnimation(anim_path, anim_name);
            FO_VERIFY_AND_THROW(anim, "Animation was not found for a baked model description", anim_entry.Name, anim_path, name);

            const int32_t anim_index = _animController->RegisterAnimation(anim, reversed);
            _animIndexes.emplace(anim_pair, anim_index);
        }

        const int32_t anim_count = _animController->GetAnimationsCount();
        FO_VERIFY_AND_THROW(anim_count != 0, "No animations registered for a baked model description", name);

        for (int32_t i = 0; i < anim_count; i++) {
            const_span<vector<hstring>> bones_hierarchy = _animController->GetAnimationBones(i);

            for (const vector<hstring>& bone_hierarchy : bones_hierarchy) {
                FO_VERIFY_AND_THROW(!bone_hierarchy.empty(), "Baked model animation contains an empty bone hierarchy", name, i, bones_hierarchy.size());
                auto bone = _hierarchy->_rootBone;

                for (size_t b = 1; b < bone_hierarchy.size(); b++) {
                    auto child = FindModelBone(bone, bone_hierarchy[b]);

                    if (!child) {
                        auto new_child = SafeAlloc::MakeUnique<ModelBone>();
                        new_child->Name = bone_hierarchy[b];
                        child = AddModelBoneChild(bone, std::move(new_child));
                    }

                    bone = child;
                }
            }
        }

        _animController->SetInterpolation(!disable_animation_interpolation);
        _hierarchy->SetupAnimationOutput(&*_animController);
    }

    return true;
}

auto ModelInformation::ReadBakedModelDescriptionLink(DataReader& reader) const -> ModelInformation::BakedModelDescriptionLink
{
    FO_STACK_TRACE_ENTRY();

    BakedModelDescriptionLink link;
    link.Data.Layer = reader.Read<int32_t>();
    link.Data.LayerValue = reader.Read<int32_t>();
    FO_VERIFY_AND_THROW(link.Data.Layer >= 0 && link.Data.Layer < numeric_cast<int32_t>(MODEL_LAYERS_COUNT), "Model link layer is out of range", link.Data.Layer);

    const string link_bone = reader.ReadString();
    link.Data.LinkBone = !link_bone.empty() ? _modelMngr->GetBoneHashedString(link_bone) : hstring {};
    link.Data.ChildName = reader.ReadString();
    link.Data.IsParticles = reader.Read<uint8_t>() != 0;
    link.Data.RotX = reader.Read<float32_t>();
    link.Data.RotY = reader.Read<float32_t>();
    link.Data.RotZ = reader.Read<float32_t>();
    link.Data.MoveX = reader.Read<float32_t>();
    link.Data.MoveY = reader.Read<float32_t>();
    link.Data.MoveZ = reader.Read<float32_t>();
    link.Data.ScaleX = reader.Read<float32_t>();
    link.Data.ScaleY = reader.Read<float32_t>();
    link.Data.ScaleZ = reader.Read<float32_t>();
    link.Data.SpeedAjust = reader.Read<float32_t>();
    FO_VERIFY_AND_THROW(link.Data.SpeedAjust >= 0.0f, "Model link speed adjust is negative");
    link.Data.DisabledLayer = reader.ReadSizedObjectVector<int32_t>();

    for (const int32_t disabled_layer : link.Data.DisabledLayer) {
        FO_VERIFY_AND_THROW(disabled_layer >= 0 && disabled_layer < numeric_cast<int32_t>(MODEL_LAYERS_COUNT), "Disabled model layer is out of range", disabled_layer);
    }

    const uint32_t disabled_mesh_count = reader.Read<uint32_t>();
    link.Data.DisabledMesh.reserve(disabled_mesh_count);

    for (uint32_t i = 0; i < disabled_mesh_count; i++) {
        const string mesh_name = reader.ReadString();
        link.Data.DisabledMesh.emplace_back(!mesh_name.empty() ? _modelMngr->GetBoneHashedString(mesh_name) : hstring {});
    }

    const uint32_t texture_count = reader.Read<uint32_t>();
    link.Data.TextureInfo.reserve(texture_count);

    for (uint32_t i = 0; i < texture_count; i++) {
        string texture_name = reader.ReadString();
        const string mesh_name = reader.ReadString();
        const int32_t texture_index = reader.Read<int32_t>();
        FO_VERIFY_AND_THROW(texture_index >= 0 && texture_index < numeric_cast<int32_t>(MODEL_MAX_TEXTURES), "Texture index is out of range", texture_index);
        link.Data.TextureInfo.emplace_back(std::move(texture_name), !mesh_name.empty() ? _modelMngr->GetBoneHashedString(mesh_name) : hstring {}, texture_index);
    }

    const uint32_t effect_count = reader.Read<uint32_t>();
    link.Data.EffectInfo.reserve(effect_count);

    for (uint32_t i = 0; i < effect_count; i++) {
        string effect_name = reader.ReadString();
        const string mesh_name = reader.ReadString();
        link.Data.EffectInfo.emplace_back(std::move(effect_name), !mesh_name.empty() ? _modelMngr->GetBoneHashedString(mesh_name) : hstring {});
    }

    const uint32_t cut_count = reader.Read<uint32_t>();
    link.CutInfo.reserve(cut_count);

    for (uint32_t i = 0; i < cut_count; i++) {
        link.CutInfo.emplace_back(ReadBakedModelDescriptionCutInfo(reader));
    }

    return link;
}

auto ModelInformation::ReadBakedModelDescriptionCutInfo(DataReader& reader) const -> ModelInformation::BakedModelDescriptionCutInfo
{
    FO_STACK_TRACE_ENTRY();

    BakedModelDescriptionCutInfo cut;
    cut.FileName = reader.ReadString();
    cut.Layers = reader.ReadSizedObjectVector<int32_t>();
    cut.Shapes = reader.ReadStringVector();
    cut.UnskinBone1 = reader.ReadString();
    cut.UnskinBone2 = reader.ReadString();
    cut.UnskinShape = reader.ReadString();
    cut.RevertUnskinShape = reader.Read<uint8_t>() != 0;
    return cut;
}

auto ModelInformation::ReadBakedModelDescriptionAnimEntry(DataReader& reader) const -> ModelInformation::BakedModelDescriptionAnimEntry
{
    FO_STACK_TRACE_ENTRY();

    BakedModelDescriptionAnimEntry anim_entry;
    anim_entry.StateAnim = reader.Read<int32_t>();
    anim_entry.ActionAnim = reader.Read<int32_t>();
    anim_entry.FileName = reader.ReadString();
    anim_entry.Name = reader.ReadString();
    return anim_entry;
}

auto ModelInformation::ReadBakedModelDescriptionAnimLayerValue(DataReader& reader) const -> ModelInformation::BakedModelDescriptionAnimLayerValue
{
    FO_STACK_TRACE_ENTRY();

    BakedModelDescriptionAnimLayerValue value;
    value.StateAnim = reader.Read<int32_t>();
    value.ActionAnim = reader.Read<int32_t>();
    value.Layer = reader.Read<int32_t>();
    value.LayerValue = reader.Read<int32_t>();
    FO_VERIFY_AND_THROW(value.Layer >= 0 && value.Layer < numeric_cast<int32_t>(MODEL_LAYERS_COUNT), "Animation layer is out of range", value.Layer);
    return value;
}

auto ModelInformation::GetAnimationIndex(CritterStateAnim& state_anim, CritterActionAnim& action_anim, nptr<float32_t> speed) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto anim_index = GetAnimationIndexEx(state_anim, action_anim, speed);

    if (anim_index != -1) {
        return anim_index;
    }

    // Find substitute animation
    const auto base_model_name = _modelMngr->_hashResolver->ToHashedString(_fileName);
    const auto base_state_anim = state_anim;
    const auto base_action_anim = action_anim;

    while (anim_index == -1) {
        auto model_name = base_model_name;
        const auto state_anim_ = state_anim;
        const auto action_anim_ = action_anim;

        if (_modelMngr->_animNameResolver->ResolveCritterAnimationSubstitute(base_model_name, base_state_anim, base_action_anim, model_name, state_anim, action_anim) && (state_anim != state_anim_ || action_anim != action_anim_)) {
            anim_index = GetAnimationIndexEx(state_anim, action_anim, speed);
        }
        else {
            break;
        }
    }

    return anim_index;
}

auto ModelInformation::GetAnimationIndexEx(CritterStateAnim state_anim, CritterActionAnim action_anim, nptr<float32_t> speed) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (const auto it1 = _stateAnimEquals.find(state_anim); it1 != _stateAnimEquals.end()) {
        state_anim = it1->second;
    }
    if (const auto it2 = _actionAnimEquals.find(action_anim); it2 != _actionAnimEquals.end()) {
        action_anim = it2->second;
    }

    const auto index = std::make_pair(state_anim, action_anim);

    if (speed) {
        const auto it = _animSpeed.find(index);

        if (it != _animSpeed.end()) {
            *speed = it->second;
        }
        else {
            *speed = 1.0f;
        }
    }

    if (const auto it = _animIndexes.find(index); it != _animIndexes.end()) {
        return it->second;
    }

    return -1;
}

auto ModelInformation::CreateInstance() -> unique_ptr<ModelInstance>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_hierarchy != nullptr, "Missing required hierarchy");
    FO_VERIFY_AND_THROW(_drawSize.width > 0 && _drawSize.height > 0, "Model has invalid draw size", _fileName, _drawSize);
    FO_VERIFY_AND_THROW(_viewSize.width > 0 && _viewSize.height > 0, "Model has invalid view size", _fileName, _viewSize);

    auto model = SafeAlloc::MakeUnique<ModelInstance>(_modelMngr, this);

    if (_animController) {
        model->_bodyAnimController.emplace(_animController->Copy());

        if (_rotationBone) {
            model->_moveAnimController.emplace(_animController->Copy());
        }
    }

    return model;
}

auto ModelInformation::CreateCutShape(ptr<const MeshData> mesh) const -> ModelCutData::Shape
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!mesh->Vertices.empty(), "Cut mesh has no vertices");

    ModelCutData::Shape shape;
    shape.GlobalTransformationMatrix = mesh->Owner->GlobalTransformationMatrix;

    if (mesh->Vertices.size() != 36) {
        shape.IsSphere = true;

        // Evaluate sphere radius
        auto vmin = mesh->Vertices[0].Position.x;
        auto vmax = mesh->Vertices[0].Position.x;

        for (const auto i : iterate_range(mesh->Vertices)) {
            const auto& v = mesh->Vertices[i];
            vmin = std::min(v.Position.x, vmin);
            vmax = std::max(v.Position.x, vmax);
        }

        shape.SphereRadius = (vmax - vmin) / 2.0f;
    }
    else {
        shape.IsSphere = false;

        // Calculate bounding box size
        float32_t vmin[3];
        float32_t vmax[3];

        for (size_t i = 0, j = mesh->Vertices.size(); i < j; i++) {
            const Vertex3D& v = mesh->Vertices[i];

            if (i == 0) {
                vmin[0] = vmax[0] = v.Position.x;
                vmin[1] = vmax[1] = v.Position.y;
                vmin[2] = vmax[2] = v.Position.z;
            }
            else {
                vmin[0] = std::max(vmin[0], v.Position.x);
                vmin[1] = std::max(vmin[1], v.Position.y);
                vmin[2] = std::max(vmin[2], v.Position.z);
                vmax[0] = std::min(vmax[0], v.Position.x);
                vmax[1] = std::min(vmax[1], v.Position.y);
                vmax[2] = std::min(vmax[2], v.Position.z);
            }
        }

        shape.BBoxMin.x = vmin[0];
        shape.BBoxMin.y = vmin[1];
        shape.BBoxMin.z = vmin[2];
        shape.BBoxMax.x = vmax[0];
        shape.BBoxMax.y = vmax[1];
        shape.BBoxMax.z = vmax[2];
    }

    return shape;
}

ModelHierarchy::ModelHierarchy(ptr<ModelManager> model_mngr, string file_name, ptr<ModelBone> root_bone) :
    _modelMngr {model_mngr},
    _fileName {std::move(file_name)},
    _rootBone {root_bone}
{
    FO_STACK_TRACE_ENTRY();
}

static void SetupBonesExt(multimap<uint32_t, ptr<ModelBone>>& bones, ptr<ModelBone> bone, uint32_t depth)
{
    FO_STACK_TRACE_ENTRY();

    bones.emplace(depth, bone);

    for (size_t i = 0; i != bone->Children.size(); ++i) {
        SetupBonesExt(bones, bone->Children[i], depth + 1);
    }
}

void ModelHierarchy::SetupBones()
{
    FO_STACK_TRACE_ENTRY();

    multimap<uint32_t, ptr<ModelBone>> bones;
    SetupBonesExt(bones, _rootBone, 0);

    for (ptr<ModelBone> bone : bones | std::views::values) {
        _allBones.emplace_back(bone);

        if (bone->AttachedMesh) {
            _allDrawBones.emplace_back(bone);
        }
    }
}

static void SetupAnimationOutputExt(ptr<ModelAnimationController> anim_controller, ptr<ModelBone> bone)
{
    FO_STACK_TRACE_ENTRY();

    RegisterModelBoneAnimationOutput(anim_controller, bone);

    for (size_t i = 0; i != bone->Children.size(); ++i) {
        SetupAnimationOutputExt(anim_controller, bone->Children[i]);
    }
}

void ModelHierarchy::SetupAnimationOutput(ptr<ModelAnimationController> anim_controller)
{
    FO_STACK_TRACE_ENTRY();

    SetupAnimationOutputExt(anim_controller, _rootBone);
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

static auto ReadModelBoneAttachedMesh(DataReader& reader, HashResolver& hash_resolver, ptr<ModelBone> owner) -> MeshData
{
    FO_STACK_TRACE_ENTRY();

    auto mesh = MeshData {
        .Owner = owner,
    };

    uint32_t len = 0;
    len = reader.Read<uint32_t>();
    mesh.Vertices.resize(len);
    reader.ReadObjectArray(span {mesh.Vertices});

    len = reader.Read<uint32_t>();
    mesh.Indices.resize(len);
    reader.ReadObjectArray(span {mesh.Indices});

    len = reader.Read<uint32_t>();
    mesh.DiffuseTexture.resize(len);
    reader.ReadStringBytes(mesh.DiffuseTexture);

    len = reader.Read<uint32_t>();
    mesh.SkinBones.resize(len);
    mesh.SkinBoneNames.resize(len);

    string tmp;

    for (uint32_t i = 0, j = len; i < j; i++) {
        len = reader.Read<uint32_t>();
        tmp.resize(len);
        reader.ReadStringBytes(tmp);
        mesh.SkinBoneNames[i] = hash_resolver.ToHashedString(tmp);
    }

    len = reader.Read<uint32_t>();
    mesh.SkinBoneOffsets.resize(len);
    reader.ReadObjectArray(span {mesh.SkinBoneOffsets});

    return mesh;
}

static auto LoadModelBone(DataReader& reader, HashResolver& hash_resolver) -> unique_ptr<ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    auto bone = SafeAlloc::MakeUnique<ModelBone>();

    uint32_t len = 0;
    len = reader.Read<uint32_t>();
    string tmp;
    tmp.resize(len);
    reader.ReadStringBytes(tmp);
    bone->Name = hash_resolver.ToHashedString(tmp);

    bone->TransformationMatrix = reader.Read<mat44>();
    bone->GlobalTransformationMatrix = reader.Read<mat44>();

    if (reader.Read<uint8_t>() != 0) {
        bone->AttachedMesh.emplace(ReadModelBoneAttachedMesh(reader, hash_resolver, bone));
    }

    len = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < len; i++) {
        bone->Children.emplace_back(LoadModelBone(reader, hash_resolver));
    }

    bone->CombinedTransformationMatrix = mat44();
    return bone;
}

static auto AddModelBoneChild(ptr<ModelBone> bone, unique_ptr<ModelBone> child) -> ptr<ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    bone->Children.emplace_back(std::move(child));
    return bone->Children.back();
}

static auto GetModelBoneAttachedMesh(ptr<const ModelBone> bone) noexcept -> nptr<const MeshData>
{
    FO_NO_STACK_TRACE_ENTRY();

    return bone->AttachedMesh ? make_nptr(&*bone->AttachedMesh) : nullptr;
}

static auto GetModelBoneAttachedMesh(ptr<ModelBone> bone) noexcept -> nptr<MeshData>
{
    FO_NO_STACK_TRACE_ENTRY();

    return bone->AttachedMesh ? make_nptr(&*bone->AttachedMesh) : nullptr;
}

static void RegisterModelBoneAnimationOutput(ptr<ModelAnimationController> anim_controller, ptr<ModelBone> bone)
{
    FO_STACK_TRACE_ENTRY();

    anim_controller->RegisterAnimationOutput(bone->Name, bone->TransformationMatrix);
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

static auto FindModelBone(ptr<const ModelBone> bone, hstring bone_name) noexcept -> nptr<const ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    if (bone->Name == bone_name) {
        return bone;
    }

    for (size_t i = 0; i < bone->Children.size(); i++) {
        auto child = bone->Children[i].as_ptr();
        const auto child_bone = FindModelBone(child, bone_name);

        if (child_bone) {
            return child_bone;
        }
    }

    return nullptr;
}

static auto FindModelBone(nptr<const ModelBone> bone, hstring bone_name) noexcept -> nptr<const ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    if (!bone) {
        return nullptr;
    }

    return FindModelBone(bone, bone_name);
}

static auto FindModelBone(ptr<ModelBone> bone, hstring bone_name) noexcept -> nptr<ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    if (bone->Name == bone_name) {
        return bone;
    }

    for (size_t i = 0; i != bone->Children.size(); ++i) {
        auto child = bone->Children[i].as_ptr();
        auto child_bone = FindModelBone(child, bone_name);

        if (child_bone) {
            return child_bone;
        }
    }

    return nullptr;
}

static auto FindModelBone(nptr<ModelBone> bone, hstring bone_name) noexcept -> nptr<ModelBone>
{
    FO_STACK_TRACE_ENTRY();

    if (!bone) {
        return nullptr;
    }

    return FindModelBone(bone, bone_name);
}

FO_END_NAMESPACE

#endif
