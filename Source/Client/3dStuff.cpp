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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "GenericUtils.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"

void MeshData::Load(DataReader& reader, NameResolver& name_resolver)
{
    STACK_TRACE_ENTRY();

    uint len = 0;
    reader.ReadPtr(&len, sizeof(len));
    Vertices.resize(len);
    reader.ReadPtr(Vertices.data(), len * sizeof(Vertices[0]));
    reader.ReadPtr(&len, sizeof(len));
    Indices.resize(len);
    reader.ReadPtr(Indices.data(), len * sizeof(Indices[0]));
    reader.ReadPtr(&len, sizeof(len));
    DiffuseTexture.resize(len);
    reader.ReadPtr(DiffuseTexture.data(), len);
    reader.ReadPtr(&len, sizeof(len));
    SkinBones.resize(len);
    SkinBoneNames.resize(len);
    string tmp;
    for (uint i = 0, j = len; i < j; i++) {
        reader.ReadPtr(&len, sizeof(len));
        tmp.resize(len);
        reader.ReadPtr(tmp.data(), len);
        SkinBoneNames[i] = name_resolver.ToHashedString(tmp);
    }
    reader.ReadPtr(&len, sizeof(len));
    SkinBoneOffsets.resize(len);
    reader.ReadPtr(SkinBoneOffsets.data(), len * sizeof(SkinBoneOffsets[0]));
}

void ModelBone::Load(DataReader& reader, NameResolver& name_resolver)
{
    STACK_TRACE_ENTRY();

    uint len = 0;
    reader.ReadPtr(&len, sizeof(len));
    string tmp;
    tmp.resize(len);
    reader.ReadPtr(tmp.data(), len);
    Name = name_resolver.ToHashedString(tmp);
    reader.ReadPtr(&TransformationMatrix, sizeof(TransformationMatrix));
    reader.ReadPtr(&GlobalTransformationMatrix, sizeof(GlobalTransformationMatrix));
    if (reader.Read<uchar>() != 0u) {
        AttachedMesh = std::make_unique<MeshData>();
        AttachedMesh->Load(reader, name_resolver);
        AttachedMesh->Owner = this;
    }
    else {
        AttachedMesh = nullptr;
    }
    reader.ReadPtr(&len, sizeof(len));
    for (uint i = 0; i < len; i++) {
        auto child = std::make_unique<ModelBone>();
        child->Load(reader, name_resolver);
        Children.push_back(std::move(child));
    }
    CombinedTransformationMatrix = mat44();
}

void ModelBone::FixAfterLoad(ModelBone* root_bone)
{
    STACK_TRACE_ENTRY();

    if (AttachedMesh) {
        auto* mesh = AttachedMesh.get();
        for (size_t i = 0, j = mesh->SkinBoneNames.size(); i < j; i++) {
            if (mesh->SkinBoneNames[i]) {
                mesh->SkinBones[i] = root_bone->Find(mesh->SkinBoneNames[i]);
            }
            else {
                mesh->SkinBones[i] = mesh->Owner;
            }
        }
    }

    for (auto&& child : Children) {
        child->FixAfterLoad(root_bone);
    }
}

auto ModelBone::Find(hstring bone_name) -> ModelBone*
{
    STACK_TRACE_ENTRY();

    if (Name == bone_name) {
        return this;
    }

    for (auto&& child : Children) {
        auto* bone = child->Find(bone_name);
        if (bone != nullptr) {
            return bone;
        }
    }
    return nullptr;
}

ModelManager::ModelManager(RenderSettings& settings, FileSystem& resources, EffectManager& effect_mngr, GameTimer& game_time, NameResolver& name_resolver, AnimationResolver& anim_name_resolver, TextureLoader tex_loader) :
    _settings {settings}, //
    _resources {resources},
    _effectMngr {effect_mngr},
    _gameTime {game_time},
    _nameResolver {name_resolver},
    _animNameResolver {anim_name_resolver},
    _textureLoader {tex_loader},
    _geometry(settings),
    _particleMngr(settings, effect_mngr, resources, std::move(tex_loader))
{
    STACK_TRACE_ENTRY();

    _moveTransitionTime = static_cast<float>(_settings.Animation3dSmoothTime) / 1000.0f;
    if (_moveTransitionTime < 0.001f) {
        _moveTransitionTime = 0.001f;
    }

    if (_settings.Animation3dFPS != 0u) {
        _animDelay = 1000 / _settings.Animation3dFPS;
    }

    _headBone = GetBoneHashedString(settings.HeadBone);

    for (const auto& bone_name : settings.LegBones) {
        _legBones.emplace(GetBoneHashedString(bone_name));
    }
}

auto ModelManager::GetBoneHashedString(string_view name) const -> hstring
{
    STACK_TRACE_ENTRY();

    return _nameResolver.ToHashedString(name);
}

auto ModelManager::LoadModel(string_view fname) -> ModelBone*
{
    STACK_TRACE_ENTRY();

    // Find already loaded
    auto name_hashed = _nameResolver.ToHashedString(fname);
    for (auto&& root_bone : _loadedModels) {
        if (root_bone->Name == name_hashed) {
            return root_bone.get();
        }
    }

    // Add to already processed
    if (_processedFiles.count(name_hashed) != 0u) {
        return nullptr;
    }
    _processedFiles.emplace(name_hashed);

    // Load file data
    const auto file = _resources.ReadFile(fname);
    if (!file) {
        WriteLog("3d file '{}' not found", fname);
        return nullptr;
    }

    // Load bones
    auto root_bone = std::make_unique<ModelBone>();
    auto reader = DataReader({file.GetBuf(), file.GetSize()});

    root_bone->Load(reader, _nameResolver);
    root_bone->FixAfterLoad(root_bone.get());

    // Load animations
    const auto anim_sets_count = reader.Read<uint>();
    for (uint i = 0; i < anim_sets_count; i++) {
        auto anim_set = std::make_unique<ModelAnimation>();
        anim_set->Load(reader, _nameResolver);
        _loadedAnimSets.push_back(std::move(anim_set));
    }

    reader.VerifyEnd();

    // Add to collection
    root_bone->Name = name_hashed;
    _loadedModels.emplace_back(std::move(root_bone));
    return _loadedModels.back().get();
}

auto ModelManager::LoadAnimation(string_view anim_fname, string_view anim_name) -> ModelAnimation*
{
    STACK_TRACE_ENTRY();

    // Find in already loaded
    const auto take_first = (anim_name == "Base");
    const auto name_hashed = _nameResolver.ToHashedString(anim_fname);
    for (auto&& anim_set : _loadedAnimSets) {
        if (_str(anim_set->GetFileName()).compareIgnoreCase(anim_fname) && (take_first || _str(anim_set->GetName()).compareIgnoreCase(anim_name))) {
            return anim_set.get();
        }
    }

    // Check maybe file already processed and nothing founded
    if (_processedFiles.count(name_hashed) != 0u) {
        return nullptr;
    }

    // File not processed, load and recheck animations
    if (LoadModel(anim_fname) != nullptr) {
        return LoadAnimation(anim_fname, anim_name);
    }

    _processedFiles.emplace(name_hashed);

    return nullptr;
}

auto ModelManager::LoadTexture(string_view texture_name, string_view model_path) const -> MeshTexture*
{
    STACK_TRACE_ENTRY();

    // Skip empty
    if (texture_name.empty()) {
        return nullptr;
    }

    // Create new
    const auto tex_path = _str(model_path).extractDir().combinePath(texture_name);
    auto&& [tex, tex_data] = _textureLoader(tex_path);
    if (tex == nullptr) {
        return nullptr;
    }

    auto* mesh_tex = new MeshTexture();
    mesh_tex->Name = _nameResolver.ToHashedString(texture_name);
    mesh_tex->MainTex = tex;
    mesh_tex->AtlasOffsetData[0] = tex_data[0];
    mesh_tex->AtlasOffsetData[1] = tex_data[1];
    mesh_tex->AtlasOffsetData[2] = tex_data[2];
    mesh_tex->AtlasOffsetData[3] = tex_data[3];

    return mesh_tex;
}

auto ModelManager::CreateModel(string_view name) -> ModelInstance*
{
    STACK_TRACE_ENTRY();

    auto* model_info = GetInformation(name);
    if (model_info == nullptr) {
        return nullptr;
    }

    auto* model = model_info->CreateInstance();
    if (model == nullptr) {
        return nullptr;
    }

    // Create mesh instances
    model->_allMeshes.resize(model_info->_hierarchy->_allDrawBones.size());
    model->_allMeshesDisabled.resize(model->_allMeshes.size());
    for (size_t i = 0, j = model_info->_hierarchy->_allDrawBones.size(); i < j; i++) {
        auto* mesh_instance = model->_allMeshes[i] = new MeshInstance();
        auto* mesh = model_info->_hierarchy->_allDrawBones[i]->AttachedMesh.get();
        mesh_instance->Mesh = mesh;
        const auto* tex_name = (mesh->DiffuseTexture.length() != 0u ? mesh->DiffuseTexture.c_str() : nullptr);
        mesh_instance->CurTexures[0] = mesh_instance->DefaultTexures[0] = (tex_name != nullptr ? model_info->_hierarchy->GetTexture(tex_name) : nullptr);
        mesh_instance->CurEffect = mesh_instance->DefaultEffect = (!mesh->EffectName.empty() ? model_info->_hierarchy->GetEffect(mesh->EffectName) : nullptr);
    }

    // Set default data
    model->SetAnimation(0, 0, nullptr, ANIMATION_INIT);
    return model;
}

void ModelManager::PreloadModel(string_view name)
{
    STACK_TRACE_ENTRY();

    const auto* model_info = GetInformation(name);
    UNUSED_VARIABLE(model_info);
}

auto ModelManager::GetInformation(string_view name) -> ModelInformation*
{
    STACK_TRACE_ENTRY();

    // Try find instance
    for (auto&& model_info : _allModelInfos) {
        if (model_info->_fileName == name) {
            return model_info.get();
        }
    }

    // Create new instance
    auto model_info = std::make_unique<ModelInformation>(*this);
    if (!model_info->Load(name)) {
        return nullptr;
    }

    _allModelInfos.push_back(std::move(model_info));
    return _allModelInfos.back().get();
}

auto ModelManager::GetHierarchy(string_view name) -> ModelHierarchy*
{
    STACK_TRACE_ENTRY();

    for (auto&& model_hierarchy : _hierarchyFiles) {
        if (model_hierarchy->_fileName == name) {
            return model_hierarchy.get();
        }
    }

    // Load
    auto* root_bone = LoadModel(name);
    if (root_bone == nullptr) {
        WriteLog("Unable to load model hierarchy file '{}'", name);
        return nullptr;
    }

    auto model_hierarchy = std::make_unique<ModelHierarchy>(*this);
    model_hierarchy->_fileName = name;
    model_hierarchy->_rootBone = root_bone;
    model_hierarchy->SetupBones();

    _hierarchyFiles.emplace_back(std::move(model_hierarchy));
    return _hierarchyFiles.back().get();
}

ModelInstance::ModelInstance(ModelManager& model_mngr) : _modelMngr(model_mngr)
{
    STACK_TRACE_ENTRY();

    _speedAdjustBase = 1.0f;
    _speedAdjustCur = 1.0f;
    _speedAdjustLink = 1.0f;
    _lookDirAngle = GameSettings::HEXAGONAL_GEOMETRY ? 150.0f : 135.0f;
    _moveDirAngle = _lookDirAngle;
    _targetMoveDirAngle = _moveDirAngle;
    _childChecker = true;
    _useGameTimer = true;
    mat44::RotationX(_modelMngr._settings.MapCameraAngle * PI_FLOAT / 180.0f, _matRot);
}

ModelInstance::~ModelInstance()
{
    STACK_TRACE_ENTRY();

    delete _bodyAnimController;
    delete _moveAnimController;

    for (const auto* anim : _children) {
        delete anim;
    }
    for (const auto* mesh : _allMeshes) {
        delete mesh;
    }
    for (const auto* mesh : _combinedMeshes) {
        delete mesh->MeshBuf;
        delete mesh;
    }
}

void ModelInstance::SetupFrame()
{
    STACK_TRACE_ENTRY();

    auto draw_width = _modelInfo->_drawWidth;
    auto draw_height = _modelInfo->_drawHeight;

    const auto draw_size_scale = std::max(std::max(_matScale.a1, _matScale.b2), _matScale.c3);
    draw_width = iround(static_cast<float>(draw_width) * draw_size_scale);
    draw_height = iround(static_cast<float>(draw_height) * draw_size_scale);

    _frameWidth = draw_width * FRAME_SCALE;
    _frameHeight = draw_height * FRAME_SCALE;

    // Projection
    const auto frame_ratio = static_cast<float>(_frameWidth) / static_cast<float>(_frameHeight);
    const auto proj_height = static_cast<float>(_frameHeight) * (1.0f / _modelMngr._settings.ModelProjFactor);
    const auto proj_width = proj_height * frame_ratio;
    MatrixHelper::MatrixOrtho(_frameProjRowMaj[0], 0.0f, proj_width, 0.0f, proj_height, -10.0f, 10.0f);
    _frameProjColMaj = _frameProjRowMaj;
    _frameProjColMaj.Transpose();
}

auto ModelInstance::Convert3dTo2d(vec3 pos) const -> IPoint
{
    STACK_TRACE_ENTRY();

    const int viewport[4] = {0, 0, _frameWidth, _frameHeight};
    vec3 out;
    MatrixHelper::MatrixProject(pos.x, pos.y, pos.z, mat44().Transpose()[0], _frameProjColMaj[0], viewport, &out.x, &out.y, &out.z);
    return {iround(out.x / static_cast<float>(FRAME_SCALE)), iround(out.y / static_cast<float>(FRAME_SCALE))};
}

auto ModelInstance::Convert2dTo3d(int x, int y) const -> vec3
{
    STACK_TRACE_ENTRY();

    const int viewport[4] = {0, 0, _frameWidth, _frameHeight};
    const auto xf = static_cast<float>(x) * static_cast<float>(FRAME_SCALE);
    const auto yf = static_cast<float>(y) * static_cast<float>(FRAME_SCALE);
    vec3 out;
    MatrixHelper::MatrixUnproject(xf, static_cast<float>(_frameHeight) - yf, 0.0f, mat44().Transpose()[0], _frameProjColMaj[0], viewport, &out.x, &out.y, &out.z);
    out.z = 0.0f;
    return out;
}

void ModelInstance::StartMeshGeneration()
{
    STACK_TRACE_ENTRY();

    if (!_allowMeshGeneration) {
        _allowMeshGeneration = true;
        GenerateCombinedMeshes();
    }
}

auto ModelInstance::SetAnimation(uint anim1, uint anim2, const int* layers, uint flags) -> bool
{
    STACK_TRACE_ENTRY();

    _curAnim1 = anim1;
    _curAnim2 = anim2;

    // Restore rotation
    if (const auto no_rotate = IsBitSet(flags, ANIMATION_NO_ROTATE); no_rotate != _noRotate) {
        _noRotate = no_rotate;

        if (_noRotate) {
            _deferredLookDirAngle = _lookDirAngle;
        }
        else if (!Math::FloatCompare(_deferredLookDirAngle, _lookDirAngle)) {
            _lookDirAngle = _deferredLookDirAngle;
            RefreshMoveAnimation();
        }
    }

    // Get animation index
    const auto anim_pair = static_cast<int>((anim1 << 16) | anim2);
    auto speed = 1.0f;
    auto index = 0;
    auto period_proc = 0.0f;
    if (!IsBitSet(flags, ANIMATION_INIT)) {
        if (anim1 == 0u) {
            index = _modelInfo->_renderAnim;
            period_proc = static_cast<float>(anim2) / 10.0f;
        }
        else {
            index = _modelInfo->GetAnimationIndex(anim1, anim2, &speed, _isCombatMode);
        }
    }

    if (IsBitSet(flags, ANIMATION_PERIOD(0))) {
        period_proc = static_cast<float>(flags >> 16);
    }
    period_proc = std::clamp(period_proc, 0.0f, 99.9f);

    // Check animation changes
    int new_layers[LAYERS3D_COUNT];
    if (layers != nullptr) {
        std::memcpy(new_layers, layers, sizeof(int) * LAYERS3D_COUNT);
    }
    else {
        std::memcpy(new_layers, _currentLayers, sizeof(int) * LAYERS3D_COUNT);
    }

    // Animation layers
    if (auto it = _modelInfo->_animLayerValues.find(anim_pair); it != _modelInfo->_animLayerValues.end()) {
        for (auto&& [index, value] : it->second) {
            new_layers[index] = value;
        }
    }

    // Check for change
    auto layer_changed = IsBitSet(flags, ANIMATION_INIT);
    if (!layer_changed) {
        for (auto i = 0; i < LAYERS3D_COUNT; i++) {
            if (new_layers[i] != _currentLayers[i]) {
                layer_changed = true;
                break;
            }
        }
    }

    // Is not one time play and same anim
    if (!IsBitSet(flags, ANIMATION_INIT | ANIMATION_ONE_TIME) && _currentLayers[LAYERS3D_COUNT] == anim_pair && !layer_changed) {
        return false;
    }

    std::memcpy(_currentLayers, new_layers, sizeof(int) * LAYERS3D_COUNT);
    _currentLayers[LAYERS3D_COUNT] = anim_pair;

    auto mesh_changed = false;
    vector<hstring> fast_transition_bones;

    if (layer_changed) {
        // Store previous cuts
        auto old_cuts = _allCuts;

        // Store disabled meshes
        for (size_t i = 0; i < _allMeshes.size(); i++) {
            _allMeshesDisabled[i] = _allMeshes[i]->Disabled;
        }

        // Set base data
        SetAnimData(_modelInfo->_animDataDefault, true);

        // Append linked data
        if (_parentBone != nullptr) {
            SetAnimData(_animLink, false);
        }

        // Mark animations as unused
        for (auto* child_anim : _children) {
            child_anim->_childChecker = false;
        }

        // Get unused layers and meshes
        bool unused_layers[LAYERS3D_COUNT] = {};
        for (auto i = 0; i < LAYERS3D_COUNT; i++) {
            if (new_layers[i] == 0) {
                continue;
            }

            for (const auto& link : _modelInfo->_animData) {
                if (link.Layer == i && link.LayerValue == new_layers[i] && link.ChildName.empty()) {
                    for (auto j : link.DisabledLayer) {
                        unused_layers[j] = true;
                    }
                    for (const auto disabled_mesh_name : link.DisabledMesh) {
                        for (auto* mesh : _allMeshes) {
                            if (!disabled_mesh_name || disabled_mesh_name == mesh->Mesh->Owner->Name) {
                                mesh->Disabled = true;
                            }
                        }
                    }
                }
            }
        }

        if (_parentBone != nullptr) {
            for (auto j : _animLink.DisabledLayer) {
                unused_layers[j] = true;
            }
            for (auto disabled_mesh_name : _animLink.DisabledMesh) {
                for (auto* mesh : _allMeshes) {
                    if (!disabled_mesh_name || disabled_mesh_name == mesh->Mesh->Owner->Name) {
                        mesh->Disabled = true;
                    }
                }
            }
        }

        // Append animations
        set<uint> keep_alive_particles;

        for (auto i = 0; i < LAYERS3D_COUNT; i++) {
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

                        for (auto&& particle_system : _particleSystems) {
                            if (particle_system.Id == link.Id) {
                                available = true;
                                break;
                            }
                        }

                        if (!available) {
                            if (const auto* to_bone = FindBone(link.LinkBone)) {
                                if (auto&& particles = _modelMngr._particleMngr.CreateParticles(link.ChildName)) {
                                    _particleSystems.push_back({link.Id, std::move(particles), to_bone, vec3(link.MoveX, link.MoveY, link.MoveZ), link.RotY});
                                }
                            }
                        }

                        keep_alive_particles.insert(link.Id);
                    }
                    else {
                        bool available = false;

                        for (auto* model : _children) {
                            if (model->_animLink.Id == link.Id) {
                                model->_childChecker = true;
                                available = true;
                                break;
                            }
                        }

                        if (!available) {
                            ModelInstance* model = nullptr;

                            // Link to main bone
                            if (link.LinkBone) {
                                auto* to_bone = _modelInfo->_hierarchy->_rootBone->Find(link.LinkBone);
                                if (to_bone != nullptr) {
                                    model = _modelMngr.CreateModel(link.ChildName);
                                    if (model != nullptr) {
                                        mesh_changed = true;
                                        model->_parent = this;
                                        model->_parentBone = to_bone;
                                        model->_animLink = link;
                                        model->SetAnimData(link, false);
                                        _children.push_back(model);
                                    }

                                    if (_modelInfo->_fastTransitionBones.count(link.LinkBone) != 0u) {
                                        fast_transition_bones.push_back(link.LinkBone);
                                    }
                                }
                            }
                            // Link all bones
                            else {
                                model = _modelMngr.CreateModel(link.ChildName);
                                if (model != nullptr) {
                                    for (auto* child_bone : model->_modelInfo->_hierarchy->_allBones) {
                                        auto* root_bone = _modelInfo->_hierarchy->_rootBone->Find(child_bone->Name);
                                        if (root_bone != nullptr) {
                                            model->_linkBones.push_back(root_bone);
                                            model->_linkBones.push_back(child_bone);
                                        }
                                    }

                                    mesh_changed = true;
                                    model->_parent = this;
                                    model->_parentBone = _modelInfo->_hierarchy->_rootBone;
                                    model->_animLink = link;
                                    model->SetAnimData(link, false);
                                    _children.push_back(model);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Erase unused animations
        for (auto it = _children.begin(); it != _children.end();) {
            auto* model = *it;
            if (!model->_childChecker) {
                mesh_changed = true;
                delete model;
                it = _children.erase(it);
            }
            else {
                ++it;
            }
        }

        for (auto it = _particleSystems.begin(); it != _particleSystems.end();) {
            if (it->Id != 0u && keep_alive_particles.count(it->Id) == 0u) {
                _particleSystems.erase(it);
            }
            else {
                ++it;
            }
        }

        // Compare changed effect
        if (!mesh_changed) {
            for (size_t i = 0; i < _allMeshes.size() && !mesh_changed; i++) {
                mesh_changed = (_allMeshes[i]->LastEffect != _allMeshes[i]->CurEffect);
            }
        }

        // Compare changed textures
        if (!mesh_changed) {
            for (size_t i = 0; i < _allMeshes.size() && !mesh_changed; i++) {
                for (size_t k = 0; k < EFFECT_TEXTURES && !mesh_changed; k++) {
                    mesh_changed = (_allMeshes[i]->LastTexures[k] != _allMeshes[i]->CurTexures[k]);
                }
            }
        }

        // Compare disabled meshes
        if (!mesh_changed) {
            for (size_t i = 0; i < _allMeshes.size() && !mesh_changed; i++) {
                mesh_changed = (_allMeshesDisabled[i] != _allMeshes[i]->Disabled);
            }
        }

        // Affect cut
        if (!mesh_changed) {
            mesh_changed = (_allCuts != old_cuts);
        }
    }

    RefreshMoveAnimation();

    if (_bodyAnimController != nullptr && index >= 0) {
        // Get the animation set from the controller
        const auto* set = _bodyAnimController->GetAnimationSet(index);

        // Alternate tracks
        uint new_track = (_currentTrack == 0 ? 1 : 0);
        auto period = set->GetDuration();
        _animPosPeriod = period;
        if (IsBitSet(flags, ANIMATION_INIT)) {
            period = 0.0002f;
        }

        // Assign to our track
        _bodyAnimController->SetTrackAnimationSet(new_track, set, nullptr);

        // Turn off fast transition bones on other tracks
        if (!fast_transition_bones.empty()) {
            _bodyAnimController->ResetBonesTransition(new_track, fast_transition_bones);
        }

        // Prepare to new tracking
        _bodyAnimController->Reset();

        // Smooth time
        auto smooth_time = (IsBitSet(flags, ANIMATION_NO_SMOOTH | ANIMATION_STAY | ANIMATION_INIT) ? 0.0001f : _modelMngr._moveTransitionTime);
        auto start_time = period * period_proc / 100.0f;
        if (IsBitSet(flags, ANIMATION_STAY)) {
            period = start_time + 0.0002f;
        }

        // Disable current track
        _bodyAnimController->AddEventEnable(_currentTrack, false, smooth_time);
        _bodyAnimController->AddEventSpeed(_currentTrack, 0.0f, 0.0f, smooth_time);
        _bodyAnimController->AddEventWeight(_currentTrack, 0.0f, 0.0f, smooth_time);

        // Enable the new track
        _bodyAnimController->SetTrackEnable(new_track, true);
        _bodyAnimController->SetTrackPosition(new_track, 0.0f);
        _bodyAnimController->AddEventSpeed(new_track, 1.0f, 0.0f, smooth_time);
        if (IsBitSet(flags, ANIMATION_ONE_TIME | ANIMATION_STAY | ANIMATION_INIT)) {
            _bodyAnimController->AddEventSpeed(new_track, 0.0f, period - 0.0001f, 0.0f);
        }
        _bodyAnimController->AddEventWeight(new_track, 1.0f, 0.0f, smooth_time);

        _bodyAnimController->AdvanceTime(start_time != 0.0f ? start_time : 0.0001f);

        if ((_isMoving || _playTurnAnimation) && _moveAnimController != nullptr) {
            _moveAnimController->AdvanceTime(0.0001f);
        }

        // Remember current track
        _currentTrack = new_track;

        // Speed
        _speedAdjustCur = speed;

        // End time
        const auto tick = GetTick();
        if (IsBitSet(flags, ANIMATION_ONE_TIME)) {
            _endTick = tick + static_cast<uint>(period / GetSpeed() * 1000.0f);
        }
        else {
            _endTick = 0;
        }

        // Force redraw
        _lastDrawTick = 0;
    }

    // Set animation for children
    for (auto* child : _children) {
        if (child->SetAnimation(anim1, anim2, layers, flags)) {
            mesh_changed = true;
        }
    }

    // Regenerate mesh for drawing
    if (_parentBone == nullptr && mesh_changed) {
        GenerateCombinedMeshes();
    }

    return mesh_changed;
}

void ModelInstance::MoveModel(int ox, int oy)
{
    STACK_TRACE_ENTRY();

    const vec3 pos_zero = Convert2dTo3d(0, 0);
    const vec3 pos = Convert2dTo3d(ox, oy);
    const vec3 diff = pos - pos_zero;

    _moveOffset += diff;
    _forceRedraw = true;
}

void ModelInstance::SetMoving(bool enabled, uint speed)
{
    STACK_TRACE_ENTRY();

    _isMoving = enabled;

    if (_isMoving) {
        RUNTIME_ASSERT(speed > 0);

        const auto walk_speed = _modelMngr._settings.AnimWalkSpeed;
        const auto run_speed = _modelMngr._settings.AnimRunSpeed;
        RUNTIME_ASSERT(run_speed >= walk_speed);

        if (speed < walk_speed + (run_speed - walk_speed) / 2) {
            _isRunning = false;
            _movingSpeedFactor = static_cast<float>(speed) / static_cast<float>(walk_speed);
        }
        else {
            _isRunning = true;
            _movingSpeedFactor = static_cast<float>(speed) / static_cast<float>(run_speed);
        }
    }

    RefreshMoveAnimation();
}

auto ModelInstance::IsCombatMode() const -> bool
{
    STACK_TRACE_ENTRY();

    return _isCombatMode;
}

void ModelInstance::SetCombatMode(bool enabled)
{
    STACK_TRACE_ENTRY();

    _isCombatMode = enabled;
}

void ModelInstance::RefreshMoveAnimation()
{
    STACK_TRACE_ENTRY();

    if (_moveAnimController == nullptr) {
        return;
    }

    uint anim1 = ANIM1_UNARMED;
    uint anim2 = ANIM2_IDLE;

    if (_isMoving) {
        const auto angle_diff = _modelMngr._geometry.GetDirAngleDiff(_targetMoveDirAngle, _lookDirAngle);
        if ((!_isMovingBack && angle_diff <= 95.0f) || (_isMovingBack && angle_diff <= 85.0f)) {
            _isMovingBack = false;
            anim2 = _isRunning ? ANIM2_RUN : ANIM2_WALK;
        }
        else {
            _isMovingBack = true;
            anim2 = _isRunning ? ANIM2_RUN_BACK : ANIM2_WALK_BACK;
        }

        _playTurnAnimation = false;
    }
    else {
        if (_isMovingBack) {
            _moveDirAngle = _targetMoveDirAngle = _lookDirAngle;
        }

        anim1 = _curAnim1;
        _isMovingBack = false;

        const auto angle_diff = _modelMngr._geometry.GetDirAngleDiffSided(_targetMoveDirAngle, _lookDirAngle);
        if (std::abs(angle_diff) > _modelMngr._settings.CritterTurnAngle) {
            _targetMoveDirAngle = _lookDirAngle;

            if (_playTurnAnimation) {
                return;
            }

            anim2 = (angle_diff < 0.0f ? ANIM2_TURN_RIGHT : ANIM2_TURN_LEFT);
            _playTurnAnimation = true;
        }
        else if (_playTurnAnimation) {
            return;
        }
    }

    float speed = 1.0f;
    const auto index = _modelInfo->GetAnimationIndex(anim1, anim2, &speed, _isCombatMode);
    if (index == _curMovingAnim) {
        return;
    }

    _curMovingAnim = index;

    if (_isMoving) {
        speed *= _movingSpeedFactor;
    }

    constexpr float smooth_time = 0.0001f;

    if (index >= 0) {
        const auto* anim_set = _moveAnimController->GetAnimationSet(index);
        const uint new_track = _currentMoveTrack == 0 ? 1 : 0;

        _moveAnimController->SetTrackAnimationSet(new_track, anim_set, &_modelMngr._legBones);
        _moveAnimController->Reset();

        _moveAnimController->AddEventEnable(_currentMoveTrack, false, smooth_time);
        _moveAnimController->AddEventSpeed(_currentMoveTrack, 0.0f, 0.0f, smooth_time);
        _moveAnimController->AddEventWeight(_currentMoveTrack, 0.0f, 0.0f, smooth_time);

        _moveAnimController->SetTrackEnable(new_track, true);
        _moveAnimController->SetTrackPosition(new_track, 0.0f);

        _moveAnimController->AddEventSpeed(new_track, speed, 0.0f, smooth_time);
        _moveAnimController->AddEventWeight(new_track, 1.0f, 0.0f, smooth_time);

        if (_playTurnAnimation) {
            _moveAnimController->AddEventEnable(new_track, false, anim_set->GetDuration() / speed);
        }

        _currentMoveTrack = new_track;
    }
    else {
        _moveAnimController->Reset();

        _moveAnimController->AddEventEnable(_currentMoveTrack, false, smooth_time);
        _moveAnimController->AddEventSpeed(_currentMoveTrack, 0.0f, 0.0f, smooth_time);
        _moveAnimController->AddEventWeight(_currentMoveTrack, 0.0f, 0.0f, smooth_time);
    }
}

auto ModelInstance::HasAnimation(uint anim1, uint anim2) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto index = static_cast<int>((anim1 << 16) | anim2);
    const auto it = _modelInfo->_animIndexes.find(index);
    return it != _modelInfo->_animIndexes.end();
}

auto ModelInstance::ResolveAnimation(uint& anim1, uint& anim2) const -> bool
{
    STACK_TRACE_ENTRY();

    return _modelInfo->GetAnimationIndex(anim1, anim2, nullptr, _isCombatMode) != -1;
}

auto ModelInstance::GetAnim1() const -> uint
{
    STACK_TRACE_ENTRY();

    return _currentLayers[LAYERS3D_COUNT] >> 16;
}

auto ModelInstance::GetAnim2() const -> uint
{
    STACK_TRACE_ENTRY();

    return _currentLayers[LAYERS3D_COUNT] & 0xFFFF;
}

auto ModelInstance::IsAnimationPlaying() const -> bool
{
    STACK_TRACE_ENTRY();

    return GetTick() < _endTick;
}

auto ModelInstance::GetRenderFramesData() const -> tuple<float, int, int, int>
{
    STACK_TRACE_ENTRY();

    auto period = 0.0f;
    if (_bodyAnimController != nullptr) {
        const auto* anim_set = _bodyAnimController->GetAnimationSet(_modelInfo->_renderAnim);
        if (anim_set != nullptr) {
            period = anim_set->GetDuration();
        }
    }

    const auto proc_from = _modelInfo->_renderAnimProcFrom;
    const auto proc_to = _modelInfo->_renderAnimProcTo;
    const auto dir = _modelInfo->_renderAnimDir;

    return tuple {period, proc_from, proc_to, dir};
}

auto ModelInstance::GetDrawSize() const -> tuple<int, int>
{
    STACK_TRACE_ENTRY();

    return {_frameWidth / FRAME_SCALE, _frameHeight / FRAME_SCALE};
}

auto ModelInstance::GetViewSize() const -> tuple<int, int>
{
    STACK_TRACE_ENTRY();

    const auto draw_size_scale = std::max(std::max(_matScale.a1, _matScale.b2), _matScale.c3);

    const auto view_width = iround(static_cast<float>(_modelInfo->_viewWidth) * draw_size_scale);
    const auto view_height = iround(static_cast<float>(_modelInfo->_viewHeight) * draw_size_scale);

    return {view_width, view_height};
}

auto ModelInstance::GetSpeed() const -> float
{
    STACK_TRACE_ENTRY();

    return _speedAdjustCur * _speedAdjustBase * _speedAdjustLink * _modelMngr._globalSpeedAdjust;
}

auto ModelInstance::GetTick() const -> uint
{
    STACK_TRACE_ENTRY();

    if (_useGameTimer) {
        return _modelMngr._gameTime.GameTick();
    }
    return _modelMngr._gameTime.FrameTick();
}

void ModelInstance::SetAnimData(ModelAnimationData& data, bool clear)
{
    STACK_TRACE_ENTRY();

    // Transformations
    if (clear) {
        _matScaleBase = mat44();
        _matRotBase = mat44();
        _matTransBase = mat44();
    }

    mat44 mat_tmp;
    if (data.ScaleX != 0.0f) {
        _matScaleBase = _matScaleBase * mat44::Scaling(vec3(data.ScaleX, 1.0f, 1.0f), mat_tmp);
    }
    if (data.ScaleY != 0.0f) {
        _matScaleBase = _matScaleBase * mat44::Scaling(vec3(1.0f, data.ScaleY, 1.0f), mat_tmp);
    }
    if (data.ScaleZ != 0.0f) {
        _matScaleBase = _matScaleBase * mat44::Scaling(vec3(1.0f, 1.0f, data.ScaleZ), mat_tmp);
    }
    if (data.RotX != 0.0f) {
        _matRotBase = _matRotBase * mat44::RotationX(-data.RotX * PI_FLOAT / 180.0f, mat_tmp);
    }
    if (data.RotY != 0.0f) {
        _matRotBase = _matRotBase * mat44::RotationY(data.RotY * PI_FLOAT / 180.0f, mat_tmp);
    }
    if (data.RotZ != 0.0f) {
        _matRotBase = _matRotBase * mat44::RotationZ(data.RotZ * PI_FLOAT / 180.0f, mat_tmp);
    }
    if (data.MoveX != 0.0f) {
        _matTransBase = _matTransBase * mat44::Translation(vec3(data.MoveX, 0.0f, 0.0f), mat_tmp);
    }
    if (data.MoveY != 0.0f) {
        _matTransBase = _matTransBase * mat44::Translation(vec3(0.0f, data.MoveY, 0.0f), mat_tmp);
    }
    if (data.MoveZ != 0.0f) {
        _matTransBase = _matTransBase * mat44::Translation(vec3(0.0f, 0.0f, -data.MoveZ), mat_tmp);
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
        for (auto* mesh : _allMeshes) {
            mesh->Disabled = false;
            std::memcpy(mesh->LastTexures, mesh->CurTexures, sizeof(mesh->LastTexures));
            std::memcpy(mesh->CurTexures, mesh->DefaultTexures, sizeof(mesh->CurTexures));
        }
    }

    if (!data.TextureInfo.empty()) {
        for (auto&& [tex_name, mesh_name, tex_num] : data.TextureInfo) {
            MeshTexture* texture = nullptr;

            // Evaluate texture
            if (_str(tex_name).startsWith("Parent")) { // Parent_MeshName
                if (_parent != nullptr) {
                    const auto* parent_mesh_name = tex_name.c_str() + 6;
                    if (parent_mesh_name[0] == '_') {
                        parent_mesh_name++;
                    }
                    const auto parent_mesh_name_hashed = (*parent_mesh_name != 0 ? _modelMngr.GetBoneHashedString(parent_mesh_name) : hstring());
                    for (const auto* mesh : _parent->_allMeshes) {
                        if (!parent_mesh_name_hashed || parent_mesh_name_hashed == mesh->Mesh->Owner->Name) {
                            texture = mesh->CurTexures[tex_num];
                            break;
                        }
                    }
                }
            }
            else {
                texture = _modelInfo->_hierarchy->GetTexture(tex_name);
            }

            // Assign it
            for (auto* mesh : _allMeshes) {
                if (!mesh_name || mesh_name == mesh->Mesh->Owner->Name) {
                    mesh->CurTexures[tex_num] = texture;
                }
            }
        }
    }

    // Effects
    if (clear) {
        for (auto* mesh : _allMeshes) {
            mesh->LastEffect = mesh->CurEffect;
            mesh->CurEffect = mesh->DefaultEffect;
        }
    }

    if (!data.EffectInfo.empty()) {
        for (const auto& eff_info : data.EffectInfo) {
            RenderEffect* effect = nullptr;

            // Get effect
            if (_str(std::get<0>(eff_info)).startsWith("Parent")) { // Parent_MeshName
                if (_parent != nullptr) {
                    const auto* mesh_name = std::get<0>(eff_info).c_str() + 6;
                    if (mesh_name[0] == '_') {
                        mesh_name++;
                    }
                    const auto mesh_name_hashed = (*mesh_name != 0 ? _modelMngr.GetBoneHashedString(mesh_name) : hstring());
                    for (const auto* mesh : _parent->_allMeshes) {
                        if (!mesh_name_hashed || mesh_name_hashed == mesh->Mesh->Owner->Name) {
                            effect = mesh->CurEffect;
                            break;
                        }
                    }
                }
            }
            else {
                effect = _modelInfo->_hierarchy->GetEffect(std::get<0>(eff_info));
            }

            // Assign it
            const auto mesh_name = std::get<1>(eff_info);
            for (auto* mesh : _allMeshes) {
                if (!mesh_name || mesh_name == mesh->Mesh->Owner->Name) {
                    mesh->CurEffect = effect;
                }
            }
        }
    }

    // Cut
    if (clear) {
        _allCuts.clear();
    }
    for (auto& cut_info : data.CutInfo) {
        _allCuts.push_back(cut_info);
    }
}

void ModelInstance::SetDir(uchar dir, bool smooth_rotation)
{
    STACK_TRACE_ENTRY();

    const auto dir_angle = _modelMngr._geometry.DirToAngle(dir);

    SetMoveDirAngle(dir_angle, smooth_rotation);
    SetLookDirAngle(dir_angle);
}

void ModelInstance::SetLookDirAngle(int dir_angle)
{
    STACK_TRACE_ENTRY();

    const auto new_angle = static_cast<float>(180 - dir_angle);

    if (!_noRotate) {
        if (!Math::FloatCompare(new_angle, _lookDirAngle)) {
            _lookDirAngle = new_angle;
            RefreshMoveAnimation();
        }
        else {
            _deferredLookDirAngle = new_angle;
        }
    }
}

void ModelInstance::SetMoveDirAngle(int dir_angle, bool smooth_rotation)
{
    STACK_TRACE_ENTRY();

    const auto new_angle = static_cast<float>(180 - dir_angle);

    if (!Math::FloatCompare(new_angle, _targetMoveDirAngle)) {
        _targetMoveDirAngle = new_angle;

        if (!smooth_rotation) {
            _moveDirAngle = _targetMoveDirAngle;
        }

        RefreshMoveAnimation();
    }

    if (!_modelInfo->_rotationBone) {
        SetLookDirAngle(dir_angle);
    }
}

void ModelInstance::SetRotation(float rx, float ry, float rz)
{
    STACK_TRACE_ENTRY();

    mat44 my;
    mat44 mx;
    mat44 mz;
    mat44::RotationX(rx, mx);
    mat44::RotationY(ry, my);
    mat44::RotationZ(rz, mz);

    _matRot = mx * my * mz;
}

void ModelInstance::SetScale(float sx, float sy, float sz)
{
    STACK_TRACE_ENTRY();

    mat44::Scaling(vec3(sx, sy, sz), _matScale);
}

void ModelInstance::SetSpeed(float speed)
{
    STACK_TRACE_ENTRY();

    _speedAdjustBase = speed;
}

void ModelInstance::SetTimer(bool use_game_timer)
{
    STACK_TRACE_ENTRY();

    _useGameTimer = use_game_timer;
}

void ModelInstance::GenerateCombinedMeshes()
{
    STACK_TRACE_ENTRY();

    // Generation disabled
    if (!_allowMeshGeneration) {
        return;
    }

    // Clean up buffers
    for (size_t i = 0; i < _combinedMeshesSize; i++) {
        auto* combined_mesh = _combinedMeshes[i];
        combined_mesh->EncapsulatedMeshCount = 0;
        combined_mesh->CurBoneMatrix = 0;
        combined_mesh->Meshes.clear();
        combined_mesh->MeshIndices.clear();
        combined_mesh->MeshVertices.clear();
        combined_mesh->MeshAnimLayers.clear();
        combined_mesh->MeshBuf->Vertices3D.clear();
        combined_mesh->MeshBuf->Indices.clear();
    }
    _combinedMeshesSize = 0;

    // Combine meshes recursively
    FillCombinedMeshes(this);

    // Cut
    _disableCulling = false;
    CutCombinedMeshes(this);

    // Finalize meshes
    for (size_t i = 0; i < _combinedMeshesSize; i++) {
        _combinedMeshes[i]->MeshBuf->StaticDataChanged = true;
    }
}

void ModelInstance::FillCombinedMeshes(const ModelInstance* cur)
{
    STACK_TRACE_ENTRY();

    // Combine meshes
    for (const auto* mesh : cur->_allMeshes) {
        CombineMesh(mesh, cur->_parentBone != nullptr ? cur->_animLink.Layer : 0);
    }

    // Fill child
    for (const auto* child_anim : cur->_children) {
        FillCombinedMeshes(child_anim);
    }
}

void ModelInstance::CombineMesh(const MeshInstance* mesh_instance, int anim_layer)
{
    STACK_TRACE_ENTRY();

    // Skip disabled meshes
    if (mesh_instance->Disabled) {
        return;
    }

    // Try encapsulate mesh instance to current combined mesh
    for (size_t i = 0; i < _combinedMeshesSize; i++) {
        if (CanBatchCombinedMesh(_combinedMeshes[i], mesh_instance)) {
            BatchCombinedMesh(_combinedMeshes[i], mesh_instance, anim_layer);
            return;
        }
    }

    // Create new combined mesh
    if (_combinedMeshesSize >= _combinedMeshes.size()) {
        auto* combined_mesh = new CombinedMesh();
        combined_mesh->MeshBuf = App->Render.CreateDrawBuffer(true);
        combined_mesh->SkinBones.resize(MODEL_MAX_BONES);
        combined_mesh->SkinBoneOffsets.resize(MODEL_MAX_BONES);
        _combinedMeshes.emplace_back(combined_mesh);
    }
    BatchCombinedMesh(_combinedMeshes[_combinedMeshesSize], mesh_instance, anim_layer);
    _combinedMeshesSize++;
}

auto ModelInstance::CanBatchCombinedMesh(const CombinedMesh* combined_mesh, const MeshInstance* mesh_instance) const -> bool
{
    STACK_TRACE_ENTRY();

    if (combined_mesh->EncapsulatedMeshCount == 0) {
        return true;
    }
    if (combined_mesh->DrawEffect != mesh_instance->CurEffect) {
        return false;
    }
    for (auto i = 0; i < EFFECT_TEXTURES; i++) {
        if ((combined_mesh->Textures[i] != nullptr) && (mesh_instance->CurTexures[i] != nullptr) && combined_mesh->Textures[i]->MainTex != mesh_instance->CurTexures[i]->MainTex) {
            return false;
        }
    }
    return combined_mesh->CurBoneMatrix + mesh_instance->Mesh->SkinBones.size() <= combined_mesh->SkinBones.size();
}

void ModelInstance::BatchCombinedMesh(CombinedMesh* combined_mesh, const MeshInstance* mesh_instance, int anim_layer)
{
    STACK_TRACE_ENTRY();

    auto* mesh_data = mesh_instance->Mesh;
    auto& vertices = combined_mesh->MeshBuf->Vertices3D;
    auto& indices = combined_mesh->MeshBuf->Indices;
    const auto vertices_old_size = vertices.size();
    const auto indices_old_size = indices.size();

    // Set or add data
    if (combined_mesh->EncapsulatedMeshCount == 0) {
        vertices = mesh_data->Vertices;
        indices = mesh_data->Indices;
        combined_mesh->DrawEffect = mesh_instance->CurEffect;
        std::memset(combined_mesh->SkinBones.data(), 0, combined_mesh->SkinBones.size() * sizeof(void*));
        std::memset(combined_mesh->Textures, 0, sizeof(combined_mesh->Textures));
        combined_mesh->CurBoneMatrix = 0;
    }
    else {
        vertices.insert(vertices.end(), mesh_data->Vertices.begin(), mesh_data->Vertices.end());
        indices.insert(indices.end(), mesh_data->Indices.begin(), mesh_data->Indices.end());

        // Add indices offset
        const auto index_offset = static_cast<ushort>(vertices.size() - mesh_data->Vertices.size());
        const auto start_index = indices.size() - mesh_data->Indices.size();
        for (auto i = start_index, j = indices.size(); i < j; i++) {
            indices[i] += index_offset;
        }

        // Add bones matrices offset
        const auto bone_index_offset = static_cast<float>(combined_mesh->CurBoneMatrix);
        const auto start_vertex = vertices.size() - mesh_data->Vertices.size();
        for (auto i = start_vertex, j = vertices.size(); i < j; i++) {
            for (auto& blend_index : vertices[i].BlendIndices) {
                blend_index += bone_index_offset;
            }
        }
    }

    // Set mesh transform and anim layer
    combined_mesh->Meshes.push_back(mesh_data);
    combined_mesh->MeshVertices.push_back(static_cast<uint>(vertices.size() - vertices_old_size));
    combined_mesh->MeshIndices.push_back(static_cast<uint>(indices.size() - indices_old_size));
    combined_mesh->MeshAnimLayers.push_back(anim_layer);

    // Add bones matrices
    for (size_t i = 0; i < mesh_data->SkinBones.size(); i++) {
        combined_mesh->SkinBones[combined_mesh->CurBoneMatrix + i] = mesh_data->SkinBones[i];
        combined_mesh->SkinBoneOffsets[combined_mesh->CurBoneMatrix + i] = mesh_data->SkinBoneOffsets[i];
    }
    combined_mesh->CurBoneMatrix += mesh_data->SkinBones.size();

    // Add textures
    for (auto i = 0; i < EFFECT_TEXTURES; i++) {
        if (combined_mesh->Textures[i] == nullptr && mesh_instance->CurTexures[i] != nullptr) {
            combined_mesh->Textures[i] = mesh_instance->CurTexures[i];
        }
    }

    // Fix texture coords
    // Todo: move texcoord offset calculation to gpu
    if (mesh_instance->CurTexures[0] != nullptr) {
        const auto* mesh_tex = mesh_instance->CurTexures[0];
        for (auto i = vertices_old_size, j = vertices.size(); i < j; i++) {
            vertices[i].TexCoord[0] = (vertices[i].TexCoord[0] * mesh_tex->AtlasOffsetData[2]) + mesh_tex->AtlasOffsetData[0];
            vertices[i].TexCoord[1] = (vertices[i].TexCoord[1] * mesh_tex->AtlasOffsetData[3]) + mesh_tex->AtlasOffsetData[1];
        }
    }

    // Increment mesh count
    combined_mesh->EncapsulatedMeshCount++;
}

void ModelInstance::CutCombinedMeshes(const ModelInstance* cur)
{
    STACK_TRACE_ENTRY();

    // Cut meshes
    if (!cur->_allCuts.empty()) {
        for (const auto* cut : cur->_allCuts) {
            for (size_t i = 0; i < _combinedMeshesSize; i++) {
                CutCombinedMesh(_combinedMeshes[i], cut);
            }
        }
        _disableCulling = true;
    }

    // Fill child
    for (const auto* child_anim : cur->_children) {
        CutCombinedMeshes(child_anim);
    }
}

// -2 - ignore
// -1 - inside
// 0 - outside
// 1 - one point
static auto SphereLineIntersection(const Vertex3D& p1, const Vertex3D& p2, const vec3& sp, float r, Vertex3D& in) -> int
{
    STACK_TRACE_ENTRY();

    auto sq = [](float f) -> float { return f * f; };
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

void ModelInstance::CutCombinedMesh(CombinedMesh* combined_mesh, const ModelCutData* cut)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto& vertices = combined_mesh->MeshBuf->Vertices3D;
    auto& indices = combined_mesh->MeshBuf->Indices;
    for (const auto& shape : cut->Shapes) {
        vector<Vertex3D> result_vertices;
        vector<ushort> result_indices;
        vector<uint> result_mesh_vertices;
        vector<uint> result_mesh_indices;

        result_vertices.reserve(vertices.size());
        result_indices.reserve(indices.size());
        result_mesh_vertices.reserve(combined_mesh->MeshVertices.size());
        result_mesh_indices.reserve(combined_mesh->MeshIndices.size());

        uint i_pos = 0;
        uint i_count = 0;
        for (size_t k = 0, l = combined_mesh->MeshIndices.size(); k < l; k++) {
            // Move shape to face space
            auto mesh_transform = combined_mesh->Meshes[k]->Owner->GlobalTransformationMatrix;
            auto sm = mesh_transform.Inverse() * shape.GlobalTransformationMatrix;
            vec3 ss;
            vec3 sp;
            quaternion sr;
            sm.Decompose(ss, sr, sp);

            // Check anim layer
            auto mesh_anim_layer = combined_mesh->MeshAnimLayers[k];
            auto skip = (std::find(cut->Layers.begin(), cut->Layers.end(), mesh_anim_layer) == cut->Layers.end());

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
                    result_vertices.push_back(v1);
                    result_vertices.push_back(v2);
                    result_vertices.push_back(v3);
                    result_indices.push_back(static_cast<ushort>(result_indices.size()));
                    result_indices.push_back(static_cast<ushort>(result_indices.size()));
                    result_indices.push_back(static_cast<ushort>(result_indices.size()));
                    continue;
                }

                if (shape.IsSphere) {
                    // Find intersections
                    Vertex3D i1;
                    int r1 = SphereLineIntersection(v1, v2, sp, shape.SphereRadius * ss.x, i1);
                    Vertex3D i2;
                    int r2 = SphereLineIntersection(v2, v3, sp, shape.SphereRadius * ss.x, i2);
                    Vertex3D i3;
                    int r3 = SphereLineIntersection(v3, v1, sp, shape.SphereRadius * ss.x, i3);

                    // Process intersections
                    bool outside = (r1 == 0 && r2 == 0 && r3 == 0);
                    bool ignore = (r1 == -2 || r2 == -2 || r3 == -2);
                    int sum = r1 + r2 + r3;
                    if (!ignore && sum == 2) {
                        // 1 1 0, corner in
                        const Vertex3D& vv1 = (r1 == 0 ? v1 : (r2 == 0 ? v2 : v3));
                        const Vertex3D& vv2 = (r1 == 0 ? v2 : (r2 == 0 ? v3 : v1));
                        const Vertex3D& vv3 = (r1 == 0 ? i3 : (r2 == 0 ? i1 : i2));
                        const Vertex3D& vv4 = (r1 == 0 ? i2 : (r2 == 0 ? i3 : i1));

                        // First face
                        result_vertices.push_back(vv1);
                        result_vertices.push_back(vv2);
                        result_vertices.push_back(vv3);
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));

                        // Second face
                        result_vertices.push_back(vv3);
                        result_vertices.push_back(vv2);
                        result_vertices.push_back(vv4);
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));
                    }
                    else if (!ignore && sum == 1) {
                        // 1 1 -1, corner out
                        const Vertex3D& vv1 = (r1 == -1 ? i3 : (r2 == -1 ? v1 : i1));
                        const Vertex3D& vv2 = (r1 == -1 ? i2 : (r2 == -1 ? i1 : v2));
                        const Vertex3D& vv3 = (r1 == -1 ? v3 : (r2 == -1 ? i3 : i2));

                        // One face
                        result_vertices.push_back(vv1);
                        result_vertices.push_back(vv2);
                        result_vertices.push_back(vv3);
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));
                    }
                    else if (ignore || outside) {
                        if (ignore && sum == 0) {
                            // 1 1 -2
                            continue;
                        }

                        result_vertices.push_back(v1);
                        result_vertices.push_back(v2);
                        result_vertices.push_back(v3);
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));
                        result_indices.push_back(static_cast<ushort>(result_indices.size()));
                    }
                }
                else {
                    if ((v1.Position.x > shape.BBoxMin.x && v1.Position.y > shape.BBoxMin.y && v1.Position.z > shape.BBoxMin.z && v1.Position.x < shape.BBoxMax.x && v1.Position.y < shape.BBoxMax.y && v1.Position.z > shape.BBoxMax.z) || //
                        (v2.Position.x > shape.BBoxMin.x && v2.Position.y > shape.BBoxMin.y && v2.Position.z > shape.BBoxMin.z && v2.Position.x < shape.BBoxMax.x && v2.Position.y < shape.BBoxMax.y && v2.Position.z > shape.BBoxMax.z) || //
                        (v3.Position.x > shape.BBoxMin.x && v3.Position.y > shape.BBoxMin.y && v3.Position.z > shape.BBoxMin.z && v3.Position.x < shape.BBoxMax.x && v3.Position.y < shape.BBoxMax.y && v3.Position.z > shape.BBoxMax.z)) {
                        continue;
                    }

                    result_vertices.push_back(v1);
                    result_vertices.push_back(v2);
                    result_vertices.push_back(v3);
                    result_indices.push_back(static_cast<ushort>(result_indices.size()));
                    result_indices.push_back(static_cast<ushort>(result_indices.size()));
                    result_indices.push_back(static_cast<ushort>(result_indices.size()));
                }
            }

            result_mesh_vertices.push_back(static_cast<uint>(result_vertices.size() - vertices_old_size));
            result_mesh_indices.push_back(static_cast<uint>(result_indices.size() - indices_old_size));
        }
        vertices = result_vertices;
        indices = result_indices;
        combined_mesh->MeshVertices = result_mesh_vertices;
        combined_mesh->MeshIndices = result_mesh_indices;
    }

    // Unskin
    if (cut->UnskinBone1 && cut->UnskinBone2) {
        // Find unskin bones
        ModelBone* unskin_bone1 = nullptr;
        float unskin_bone1_index = 0.0f;
        ModelBone* unskin_bone2 = nullptr;
        float unskin_bone2_index = 0.0f;
        for (size_t i = 0; i < combined_mesh->CurBoneMatrix; i++) {
            if (combined_mesh->SkinBones[i]->Name == cut->UnskinBone1) {
                unskin_bone1 = combined_mesh->SkinBones[i];
                unskin_bone1_index = static_cast<float>(i);
            }
            if (combined_mesh->SkinBones[i]->Name == cut->UnskinBone2) {
                unskin_bone2 = combined_mesh->SkinBones[i];
                unskin_bone2_index = static_cast<float>(i);
            }
            if (unskin_bone1 != nullptr && unskin_bone2 != nullptr) {
                break;
            }
        }

        // Unskin
        if (unskin_bone1 != nullptr && unskin_bone2 != nullptr) {
            // Process meshes
            size_t v_pos = 0;
            size_t v_count = 0;
            for (size_t i = 0, j = combined_mesh->MeshVertices.size(); i < j; i++) {
                // Check anim layer
                if (std::find(cut->Layers.begin(), cut->Layers.end(), combined_mesh->MeshAnimLayers[i]) == cut->Layers.end()) {
                    v_count += combined_mesh->MeshVertices[i];
                    v_pos = v_count;
                    continue;
                }

                // Move shape to face space
                auto mesh_transform = combined_mesh->Meshes[i]->Owner->GlobalTransformationMatrix;
                auto sm = mesh_transform.Inverse() * cut->UnskinShape.GlobalTransformationMatrix;
                vec3 ss;
                vec3 sp;
                quaternion sr;
                sm.Decompose(ss, sr, sp);
                auto sphere_square_radius = powf(cut->UnskinShape.SphereRadius * ss.x, 2.0f);
                auto revert_shape = cut->RevertUnskinShape;

                // Process mesh vertices
                v_count += combined_mesh->MeshVertices[i];
                for (; v_pos < v_count; v_pos++) {
                    auto& v = vertices[v_pos];

                    // Get vertex side
                    auto v_side = ((v.Position - sp).SquareLength() <= sphere_square_radius);
                    if (revert_shape) {
                        v_side = !v_side;
                    }

                    // Check influences
                    for (auto b = 0; b < BONES_PER_VERTEX; b++) {
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
                        bool influence_side = unskin_bone1->Find(combined_mesh->SkinBones[static_cast<int>(v.BlendIndices[b])]->Name) != nullptr;
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
}

auto ModelInstance::NeedDraw() const -> bool
{
    STACK_TRACE_ENTRY();

    if (_forceRedraw) {
        return true;
    }

    return _combinedMeshesSize != 0u && (_lastDrawTick == 0u || GetTick() - _lastDrawTick >= _modelMngr._animDelay);
}

void ModelInstance::Draw()
{
    STACK_TRACE_ENTRY();

    _forceRedraw = false;

    // Move timer
    const auto tick = GetTick();
    const auto elapsed = _lastDrawTick != 0u ? 0.001f * static_cast<float>(tick - _lastDrawTick) : 0.0f;
    _lastDrawTick = tick;

    // Move animation
    const auto w = _frameWidth / FRAME_SCALE;
    const auto h = _frameHeight / FRAME_SCALE;
    ProcessAnimation(elapsed, w / 2, h - h / 4, static_cast<float>(FRAME_SCALE));

    if (_combinedMeshesSize != 0u) {
        for (size_t i = 0; i < _combinedMeshesSize; i++) {
            DrawCombinedMesh(_combinedMeshes[i], _shadowDisabled || _modelInfo->_shadowDisabled);
        }
    }

    DrawAllParticles();
}

void ModelInstance::ProcessAnimation(float elapsed, int x, int y, float scale)
{
    STACK_TRACE_ENTRY();

    // Update world matrix, only for root
    if (_parentBone == nullptr) {
        const auto pos = Convert2dTo3d(x, y);
        mat44 mat_rot_y;
        mat44 mat_scale;
        mat44 mat_trans;
        mat44::Scaling(vec3(scale, scale, scale), mat_scale);
        mat44::RotationY((_moveDirAngle + (_isMovingBack ? 180.0f : 0.0f)) * PI_FLOAT / 180.0f, mat_rot_y);
        mat44::Translation(pos, mat_trans);
        _parentMatrix = mat_trans * _matTransBase * _matRot * mat_rot_y * _matRotBase * mat_scale * _matScale * _matScaleBase;
        _groundPos.x = _parentMatrix.a4;
        _groundPos.y = _parentMatrix.b4;
        _groundPos.z = _parentMatrix.c4;
    }

    // Rotate body
    if (!Math::FloatCompare(_moveDirAngle, _targetMoveDirAngle)) {
        const auto diff = _modelMngr._geometry.GetDirAngleDiffSided(_moveDirAngle, _targetMoveDirAngle);
        _moveDirAngle += std::clamp(diff * elapsed * 10.0f, -std::abs(diff), std::abs(diff));
    }

    // Advance animation time
    auto prev_track_pos = 0.0f;
    auto new_track_pos = 0.0f;
    if (_bodyAnimController != nullptr && elapsed >= 0.0f) {
        prev_track_pos = _bodyAnimController->GetTrackPosition(_currentTrack);

        _bodyAnimController->AdvanceTime(elapsed * GetSpeed());

        if ((_isMoving || _playTurnAnimation) && _moveAnimController != nullptr) {
            _moveAnimController->AdvanceTime(elapsed * GetSpeed());

            if (_playTurnAnimation && !_moveAnimController->GetTrackEnable(_currentMoveTrack)) {
                _playTurnAnimation = false;
                RefreshMoveAnimation();
            }
        }

        new_track_pos = _bodyAnimController->GetTrackPosition(_currentTrack);

        if (_animPosPeriod > 0.0f) {
            _animPosProc = new_track_pos / _animPosPeriod;
            if (_animPosProc >= 1.0f) {
                _animPosProc = std::fmod(_animPosProc, 1.0f);
            }
            _animPosTime = new_track_pos;
            if (_animPosTime >= _animPosPeriod) {
                _animPosTime = std::fmod(_animPosTime, _animPosPeriod);
            }
        }
    }

    // Update matrices
    UpdateBoneMatrices(_modelInfo->_hierarchy->_rootBone, &_parentMatrix);

    // Update linked matrices
    if ((_parentBone != nullptr) && !_linkBones.empty()) {
        for (uint i = 0, j = static_cast<uint>(_linkBones.size()) / 2; i < j; i++) {
            _linkBones[i * 2 + 1]->CombinedTransformationMatrix = _linkBones[i * 2]->CombinedTransformationMatrix;
        }
    }

    // Update world matrices for children
    for (auto* child : _children) {
        child->_groundPos = _groundPos;
        child->_parentMatrix = child->_parentBone->CombinedTransformationMatrix * child->_matTransBase * child->_matRotBase * child->_matScaleBase;
    }

    // Particles
    for (auto&& particle_system : _particleSystems) {
        if (particle_system.Id == 0u) {
            particle_system.Particles->Update(elapsed, particle_system.Bone->CombinedTransformationMatrix, particle_system.Move, particle_system.Rot, _moveOffset);
        }
        else {
            particle_system.Particles->Update(elapsed, particle_system.Bone->CombinedTransformationMatrix, particle_system.Move, particle_system.Rot + _lookDirAngle, _moveOffset);
        }
    }

    for (auto it = _particleSystems.begin(); it != _particleSystems.end();) {
        if (!it->Particles->IsActive()) {
            it = _particleSystems.erase(it);
        }
        else {
            ++it;
        }
    }

    // Move child animations
    for (auto* child_anim : _children) {
        child_anim->ProcessAnimation(elapsed, x, y, 1.0f);
    }

    // Animation callbacks
    if (_bodyAnimController != nullptr && elapsed >= 0.0f && _animPosPeriod > 0.0f) {
        for (auto& callback : AnimationCallbacks) {
            if (((callback.Anim1 == 0u) || callback.Anim1 == _curAnim1) && ((callback.Anim2 == 0u) || callback.Anim2 == _curAnim2)) {
                const auto fire_track_pos1 = floorf(prev_track_pos / _animPosPeriod) * _animPosPeriod + callback.NormalizedTime * _animPosPeriod;
                const auto fire_track_pos2 = floorf(new_track_pos / _animPosPeriod) * _animPosPeriod + callback.NormalizedTime * _animPosPeriod;
                if ((prev_track_pos < fire_track_pos1 && new_track_pos >= fire_track_pos1) || (prev_track_pos < fire_track_pos2 && new_track_pos >= fire_track_pos2)) {
                    callback.Callback();
                }
            }
        }
    }
}

void ModelInstance::UpdateBoneMatrices(ModelBone* bone, const mat44* parent_matrix)
{
    STACK_TRACE_ENTRY();

    if (_modelInfo->_rotationBone && bone->Name == _modelInfo->_rotationBone && !Math::FloatCompare(_lookDirAngle, _moveDirAngle)) {
        mat44 mat_rot;
        mat44::RotationX((_modelMngr._geometry.GetDirAngleDiffSided(_lookDirAngle + (_isMovingBack ? 180.0f : 0.0f), _moveDirAngle) * -_modelMngr._settings.CritterBodyTurnFactor) * PI_FLOAT / 180.0f, mat_rot);
        bone->CombinedTransformationMatrix = (*parent_matrix) * mat_rot * bone->TransformationMatrix;
    }
    else if (_modelInfo->_rotationBone && bone->Name == _modelMngr._headBone && !Math::FloatCompare(_lookDirAngle, _moveDirAngle)) {
        mat44 mat_rot;
        mat44::RotationX((_modelMngr._geometry.GetDirAngleDiffSided(_lookDirAngle + (_isMovingBack ? 180.0f : 0.0f), _moveDirAngle) * -_modelMngr._settings.CritterHeadTurnFactor) * PI_FLOAT / 180.0f, mat_rot);
        bone->CombinedTransformationMatrix = (*parent_matrix) * mat_rot * bone->TransformationMatrix;
    }
    else {
        bone->CombinedTransformationMatrix = (*parent_matrix) * bone->TransformationMatrix;
    }

    // Update child
    for (auto it = bone->Children.begin(), end = bone->Children.end(); it != end; ++it) {
        UpdateBoneMatrices(it->get(), &bone->CombinedTransformationMatrix);
    }
}

void ModelInstance::DrawCombinedMesh(const CombinedMesh* combined_mesh, bool shadow_disabled)
{
    STACK_TRACE_ENTRY();

    auto* effect = combined_mesh->DrawEffect != nullptr ? combined_mesh->DrawEffect : _modelMngr._effectMngr.Effects.SkinnedModel;

    effect->ProjBuf = RenderEffect::ProjBuffer();
    std::memcpy(effect->ProjBuf->ProjMatrix, _frameProjColMaj[0], 16 * sizeof(float));

    effect->MainTex = combined_mesh->Textures[0] != nullptr ? combined_mesh->Textures[0]->MainTex : nullptr;

    if (effect->MainTexBuf) {
        std::memcpy(effect->MainTexBuf->MainTexSize, effect->MainTex->SizeData, 4 * sizeof(float));
    }

    // Todo: merge all bones in one hierarchy and disable offset copying
    auto* wm = effect->ModelBuf->WorldMatrices;
    for (size_t i = 0; i < combined_mesh->CurBoneMatrix; i++) {
        auto m = combined_mesh->SkinBones[i]->CombinedTransformationMatrix * combined_mesh->SkinBoneOffsets[i];
        m.Transpose(); // Convert to column major order
        std::memcpy(wm, m[0], 16 * sizeof(float));
        wm += 16;
    }
    effect->MatrixCount = combined_mesh->CurBoneMatrix;

    std::memcpy(effect->ModelBuf->GroundPosition, &_groundPos, 3 * sizeof(float));
    effect->ModelBuf->GroundPosition[3] = 0.0f;

    std::memcpy(effect->ModelBuf->LightColor, &_modelMngr._lightColor, 4 * sizeof(float));

    if (effect->CustomTexBuf) {
        for (auto i = 0; i < EFFECT_TEXTURES; i++) {
            if (combined_mesh->Textures[i] != nullptr) {
                effect->CustomTex[i] = combined_mesh->Textures[i]->MainTex;
                std::memcpy(&effect->CustomTexBuf->AtlasOffset[i * 4 * sizeof(float)], combined_mesh->Textures[i]->AtlasOffsetData, 4 * sizeof(float));
                std::memcpy(&effect->CustomTexBuf->Size[i * 4 * sizeof(float)], combined_mesh->Textures[i]->MainTex->SizeData, 4 * sizeof(float));
            }
            else {
                effect->CustomTex[i] = nullptr;
            }
        }
    }

    if (effect->AnimBuf) {
        effect->AnimBuf->NormalizedTime = _animPosProc;
        effect->AnimBuf->AbsoluteTime = _animPosTime;
    }

    effect->DisableCulling = _disableCulling;

    combined_mesh->MeshBuf->Upload(effect->Usage);
    effect->DrawBuffer(combined_mesh->MeshBuf);
}

void ModelInstance::DrawAllParticles()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto&& particle_system : _particleSystems) {
        particle_system.Particles->Draw(_frameProjColMaj, _moveOffset, _modelMngr._settings.MapCameraAngle);
    }

    for (auto* child : _children) {
        child->DrawAllParticles();
    }
}

auto ModelInstance::FindBone(hstring bone_name) const -> const ModelBone*
{
    STACK_TRACE_ENTRY();

    const auto* bone = _modelInfo->_hierarchy->_rootBone->Find(bone_name);
    if (bone == nullptr) {
        for (const auto* child : _children) {
            bone = child->_modelInfo->_hierarchy->_rootBone->Find(bone_name);
            if (bone != nullptr) {
                break;
            }
        }
    }

    return bone;
}

auto ModelInstance::GetBonePos(hstring bone_name) const -> optional<tuple<int, int>>
{
    STACK_TRACE_ENTRY();

    const auto* bone = FindBone(bone_name);
    if (bone == nullptr) {
        return std::nullopt;
    }

    vec3 pos;
    quaternion rot;
    bone->CombinedTransformationMatrix.DecomposeNoScaling(rot, pos);

    const auto p = Convert3dTo2d(pos);
    const auto x = p.X - _frameWidth / FRAME_SCALE / 2;
    const auto y = -(p.Y - _frameHeight / FRAME_SCALE / 4);
    return tuple {x, y};
}

auto ModelInstance::GetAnimDuration() const -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_animPosPeriod * 1000.0f);
}

void ModelInstance::RunParticles(string_view particles_name, hstring bone_name, vec3 move)
{
    STACK_TRACE_ENTRY();

    if (const auto* to_bone = FindBone(bone_name)) {
        if (auto&& particles = _modelMngr._particleMngr.CreateParticles(particles_name)) {
            _particleSystems.push_back({0u, std::move(particles), to_bone, move, _lookDirAngle});
        }
    }
}

ModelInformation::ModelInformation(ModelManager& model_mngr) : _modelMngr {model_mngr}
{
    STACK_TRACE_ENTRY();

    _drawWidth = _modelMngr._settings.DefaultModelDrawWidth;
    _drawHeight = _modelMngr._settings.DefaultModelDrawHeight;
    _viewWidth = _modelMngr._settings.DefaultModelViewWidth != 0 ? _modelMngr._settings.DefaultModelViewWidth : _drawWidth / 4;
    _viewHeight = _modelMngr._settings.DefaultModelViewHeight != 0 ? _modelMngr._settings.DefaultModelViewHeight : _drawHeight / 2;
}

auto ModelInformation::Load(string_view name) -> bool
{
    STACK_TRACE_ENTRY();

    string ext = _str(name).getFileExtension();
    if (ext.empty()) {
        return false;
    }

    // Load fonline 3d file
    if (ext == "fo3d") {
        // Load main fo3d file
        auto fo3d = _modelMngr._resources.ReadFile(name);
        if (!fo3d) {
            return false;
        }

        // Parse
        string file_buf = fo3d.GetStr();
        string model;
        string render_fname;
        string render_anim;
        auto disable_animation_interpolation = false;
        auto convert_value_fail = false;

        hstring mesh;
        auto layer = -1;
        auto layer_val = 0;

        ModelAnimationData dummy_link {};
        auto* link = &_animDataDefault;
        static uint link_id = 0;

        auto* istr = new istringstream(file_buf);
        auto closed = false;
        string token;
        string buf;
        auto valuei = 0;
        auto valuef = 0.0f;

        struct AnimEntry
        {
            int Index;
            string FileName;
            string Name;
        };
        vector<AnimEntry> anims;

        while (!istr->eof()) {
            (*istr) >> token;
            if (istr->fail()) {
                break;
            }

            auto comment = token.find(';');
            if (comment == string::npos) {
                comment = token.find('#');
            }
            if (comment != string::npos) {
                token = token.substr(0, comment);

                string line;
                std::getline(*istr, line, '\n');
                if (token.empty()) {
                    continue;
                }
            }

            if (closed) {
                if (token == "ContinueParsing") {
                    closed = false;
                }
                continue;
            }

            if (token == "StopParsing") {
                closed = true;
            }
            else if (token == "Model") {
                (*istr) >> buf;
                model = _str(name).extractDir().combinePath(buf);
            }
            else if (token == "Include") {
                // Get swapped words
                vector<string> templates;
                string line;
                std::getline(*istr, line, '\n');
                templates = _str(line).trim().split(' ');
                if (templates.empty()) {
                    continue;
                }

                for (size_t i = 1; i < templates.size() - 1; i += 2) {
                    templates[i] = _str("%{}%", templates[i]);
                }

                // Include file path
                string fname = _str(name).extractDir().combinePath(templates[0]);
                auto fo3d_ex = _modelMngr._resources.ReadFile(fname);
                if (!fo3d_ex) {
                    WriteLog("Include file '{}' not found", fname);
                    continue;
                }

                // Words swapping
                string new_content = fo3d_ex.GetStr();
                if (templates.size() > 2) {
                    for (size_t i = 1; i < templates.size() - 1; i += 2) {
                        new_content = _str(new_content).replace(templates[i], templates[i + 1]);
                    }
                }

                // Insert new buffer
                file_buf = _str("{}\n{}", new_content, !istr->eof() ? file_buf.substr(static_cast<size_t>(istr->tellg())) : "");

                // Reinitialize stream
                delete istr;
                istr = new istringstream(file_buf);
            }
            else if (token == "Mesh") {
                (*istr) >> buf;
                if (buf != "All") {
                    mesh = _modelMngr.GetBoneHashedString(buf);
                }
                else {
                    mesh = hstring();
                }
            }
            else if (token == "Subset") {
                (*istr) >> buf;
                WriteLog("Tag 'Subset' obsolete, use 'Mesh' instead");
            }
            else if (token == "Layer" || token == "Value") {
                (*istr) >> buf;
                if (token == "Layer") {
                    layer = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);
                }
                else {
                    layer_val = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);
                }

                link = &dummy_link;
                mesh = hstring();
            }
            else if (token == "Root") {
                if (layer == -1) {
                    link = &_animDataDefault;
                }
                else if (layer_val == 0) {
                    WriteLog("Wrong layer '{}' zero value", layer);
                    link = &dummy_link;
                }
                else {
                    link = &_animData.emplace_back();
                    link->Id = ++link_id;
                    link->Layer = layer;
                    link->LayerValue = layer_val;
                }

                mesh = hstring();
            }
            else if (token == "Attach") {
                (*istr) >> buf;
                if (layer < 0 || layer_val == 0) {
                    continue;
                }

                link = &_animData.emplace_back();
                link->Id = ++link_id;
                link->Layer = layer;
                link->LayerValue = layer_val;

                string fname = _str(name).extractDir().combinePath(buf);
                link->ChildName = fname;
                link->IsParticles = false;

                mesh = hstring();
            }
            else if (token == "AttachParticles") {
                (*istr) >> buf;
                if (layer < 0 || layer_val == 0) {
                    continue;
                }

                link = &_animData.emplace_back();
                link->Id = ++link_id;
                link->Layer = layer;
                link->LayerValue = layer_val;

                link->ChildName = buf;
                link->IsParticles = true;

                mesh = hstring();
            }
            else if (token == "Link") {
                (*istr) >> buf;
                if (link->Id != 0u) {
                    link->LinkBone = _modelMngr.GetBoneHashedString(buf);
                }
            }
            else if (token == "Cut") {
                (*istr) >> buf;
                string fname = _str(name).extractDir().combinePath(buf);
                auto* area = _modelMngr.GetHierarchy(fname);
                if (area != nullptr) {
                    // Add cut
                    auto* cut = new ModelCutData();
                    link->CutInfo.push_back(cut);

                    // Layers
                    (*istr) >> buf;
                    auto cur_layer_names = _str(buf).split('-');

                    for (auto& cut_layer_name : cur_layer_names) {
                        if (cut_layer_name != "All") {
                            const auto cut_layer = _modelMngr._nameResolver.ResolveGenericValue(cut_layer_name, &convert_value_fail);
                            cut->Layers.push_back(cut_layer);
                        }
                        else {
                            for (auto i = 0; i < LAYERS3D_COUNT; i++) {
                                if (i != layer) {
                                    cut->Layers.push_back(i);
                                }
                            }
                        }
                    }

                    // Shapes
                    (*istr) >> buf;
                    auto shapes = _str(buf).split('-');

                    // Unskin bones
                    (*istr) >> buf;
                    cut->UnskinBone1 = buf != "-" ? _modelMngr.GetBoneHashedString(buf) : hstring();

                    (*istr) >> buf;
                    cut->UnskinBone2 = buf != "-" ? _modelMngr.GetBoneHashedString(buf) : hstring();

                    // Unskin shape
                    (*istr) >> buf;
                    hstring unskin_shape_name;
                    cut->RevertUnskinShape = false;
                    if (cut->UnskinBone1 && cut->UnskinBone2) {
                        cut->RevertUnskinShape = !buf.empty() && buf[0] == '~';
                        unskin_shape_name = _modelMngr.GetBoneHashedString(!buf.empty() && buf[0] == '~' ? buf.substr(1) : buf);
                        for (auto* bone : area->_allDrawBones) {
                            if (unskin_shape_name == bone->Name) {
                                cut->UnskinShape = CreateCutShape(bone->AttachedMesh.get());
                            }
                        }
                    }

                    // Parse shapes
                    for (auto& shape : shapes) {
                        auto shape_name = _modelMngr.GetBoneHashedString(shape);
                        if (shape == "All") {
                            shape_name = hstring();
                        }
                        for (auto* bone : area->_allDrawBones) {
                            if ((!shape_name || shape_name == bone->Name) && bone->Name != unskin_shape_name) {
                                cut->Shapes.push_back(CreateCutShape(bone->AttachedMesh.get()));
                            }
                        }
                    }
                }
                else {
                    WriteLog("Cut file '{}' not found", fname);
                    (*istr) >> buf;
                    (*istr) >> buf;
                    (*istr) >> buf;
                    (*istr) >> buf;
                    (*istr) >> buf;
                }
            }
            else if (token == "RotX") {
                (*istr) >> link->RotX;
            }
            else if (token == "RotY") {
                (*istr) >> link->RotY;
            }
            else if (token == "RotZ") {
                (*istr) >> link->RotZ;
            }
            else if (token == "MoveX") {
                (*istr) >> link->MoveX;
            }
            else if (token == "MoveY") {
                (*istr) >> link->MoveY;
            }
            else if (token == "MoveZ") {
                (*istr) >> link->MoveZ;
            }
            else if (token == "ScaleX") {
                (*istr) >> link->ScaleX;
            }
            else if (token == "ScaleY") {
                (*istr) >> link->ScaleY;
            }
            else if (token == "ScaleZ") {
                (*istr) >> link->ScaleZ;
            }
            else if (token == "Scale") {
                (*istr) >> valuef;
                link->ScaleX = link->ScaleY = link->ScaleZ = valuef;
            }
            else if (token == "Speed") {
                (*istr) >> link->SpeedAjust;
            }
            else if (token == "RotX+") {
                (*istr) >> valuef;
                link->RotX = (link->RotX == 0.0f ? valuef : link->RotX + valuef);
            }
            else if (token == "RotY+") {
                (*istr) >> valuef;
                link->RotY = (link->RotY == 0.0f ? valuef : link->RotY + valuef);
            }
            else if (token == "RotZ+") {
                (*istr) >> valuef;
                link->RotZ = (link->RotZ == 0.0f ? valuef : link->RotZ + valuef);
            }
            else if (token == "MoveX+") {
                (*istr) >> valuef;
                link->MoveX = (link->MoveX == 0.0f ? valuef : link->MoveX + valuef);
            }
            else if (token == "MoveY+") {
                (*istr) >> valuef;
                link->MoveY = (link->MoveY == 0.0f ? valuef : link->MoveY + valuef);
            }
            else if (token == "MoveZ+") {
                (*istr) >> valuef;
                link->MoveZ = (link->MoveZ == 0.0f ? valuef : link->MoveZ + valuef);
            }
            else if (token == "ScaleX+") {
                (*istr) >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX + valuef);
            }
            else if (token == "ScaleY+") {
                (*istr) >> valuef;
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY + valuef);
            }
            else if (token == "ScaleZ+") {
                (*istr) >> valuef;
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ + valuef);
            }
            else if (token == "Scale+") {
                (*istr) >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX + valuef);
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY + valuef);
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ + valuef);
            }
            else if (token == "Speed+") {
                (*istr) >> valuef;
                link->SpeedAjust = (link->SpeedAjust == 0.0f ? valuef : link->SpeedAjust * valuef);
            }
            else if (token == "RotX*") {
                (*istr) >> valuef;
                link->RotX = (link->RotX == 0.0f ? valuef : link->RotX * valuef);
            }
            else if (token == "RotY*") {
                (*istr) >> valuef;
                link->RotY = (link->RotY == 0.0f ? valuef : link->RotY * valuef);
            }
            else if (token == "RotZ*") {
                (*istr) >> valuef;
                link->RotZ = (link->RotZ == 0.0f ? valuef : link->RotZ * valuef);
            }
            else if (token == "MoveX*") {
                (*istr) >> valuef;
                link->MoveX = (link->MoveX == 0.0f ? valuef : link->MoveX * valuef);
            }
            else if (token == "MoveY*") {
                (*istr) >> valuef;
                link->MoveY = (link->MoveY == 0.0f ? valuef : link->MoveY * valuef);
            }
            else if (token == "MoveZ*") {
                (*istr) >> valuef;
                link->MoveZ = (link->MoveZ == 0.0f ? valuef : link->MoveZ * valuef);
            }
            else if (token == "ScaleX*") {
                (*istr) >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX * valuef);
            }
            else if (token == "ScaleY*") {
                (*istr) >> valuef;
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY * valuef);
            }
            else if (token == "ScaleZ*") {
                (*istr) >> valuef;
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ * valuef);
            }
            else if (token == "Scale*") {
                (*istr) >> valuef;
                link->ScaleX = (link->ScaleX == 0.0f ? valuef : link->ScaleX * valuef);
                link->ScaleY = (link->ScaleY == 0.0f ? valuef : link->ScaleY * valuef);
                link->ScaleZ = (link->ScaleZ == 0.0f ? valuef : link->ScaleZ * valuef);
            }
            else if (token == "Speed*") {
                (*istr) >> valuef;
                link->SpeedAjust = (link->SpeedAjust == 0.0f ? valuef : link->SpeedAjust * valuef);
            }
            else if (token == "DisableLayer") {
                (*istr) >> buf;
                const auto disabled_layers = _str(buf).split('-');

                for (const auto& disabled_layer_name : disabled_layers) {
                    const auto disabled_layer = _modelMngr._nameResolver.ResolveGenericValue(disabled_layer_name, &convert_value_fail);
                    if (disabled_layer >= 0 && disabled_layer < LAYERS3D_COUNT) {
                        link->DisabledLayer.push_back(disabled_layer);
                    }
                }
            }
            else if (token == "DisableSubset") {
                (*istr) >> buf;
                WriteLog("Tag 'DisableSubset' obsolete, use 'DisableMesh' instead");
            }
            else if (token == "DisableMesh") {
                (*istr) >> buf;
                const auto disabled_mesh_names = _str(buf).split('-');

                for (const auto& disabled_mesh_name : disabled_mesh_names) {
                    hstring disabled_mesh;

                    if (disabled_mesh_name != "All") {
                        disabled_mesh = _modelMngr.GetBoneHashedString(disabled_mesh_name);
                    }

                    link->DisabledMesh.push_back(disabled_mesh);
                }
            }
            else if (token == "Texture") {
                (*istr) >> buf;
                auto index = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);

                (*istr) >> buf;
                if (index >= 0 && index < EFFECT_TEXTURES) {
                    link->TextureInfo.emplace_back(buf, mesh, index);
                }
            }
            else if (token == "Effect") {
                (*istr) >> buf;

                link->EffectInfo.emplace_back(buf, mesh);
            }
            else if (token == "Anim" || token == "AnimSpeed" || token == "AnimExt" || token == "AnimSpeedExt") {
                // Index animation
                (*istr) >> buf;
                const auto ind1 = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);
                (*istr) >> buf;
                const auto ind2 = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);

                if (token == "Anim" || token == "AnimExt") {
                    // Todo: add reverse playing of 3d animation

                    string a1;
                    string a2;
                    (*istr) >> a1 >> a2;
                    anims.push_back({(ind1 << 16) | ind2, a1, a2});

                    if (token == "AnimExt") {
                        (*istr) >> a1 >> a2;
                        anims.push_back({(ind1 << 16) | (ind2 | 0x8000), a1, a2});
                    }
                }
                else {
                    (*istr) >> valuef;
                    _animSpeed.emplace((ind1 << 16) | ind2, valuef);

                    if (token == "AnimSpeedExt") {
                        (*istr) >> valuef;
                        _animSpeed.emplace((ind1 << 16) | (ind2 | 0x8000), valuef);
                    }
                }
            }
            else if (token == "AnimLayerValue") {
                (*istr) >> buf;
                const auto ind1 = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);
                (*istr) >> buf;
                const auto ind2 = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);

                (*istr) >> buf;
                const auto anim_layer = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);
                (*istr) >> buf;
                const auto anim_layer_value = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);

                uint index = (ind1 << 16) | ind2;
                if (_animLayerValues.count(index) == 0u) {
                    _animLayerValues.emplace(index, vector<pair<int, int>>());
                }
                _animLayerValues[index].emplace_back(anim_layer, anim_layer_value);
            }
            else if (token == "FastTransitionBone") {
                (*istr) >> buf;
                _fastTransitionBones.insert(_modelMngr.GetBoneHashedString(buf));
            }
            else if (token == "AnimEqual") {
                (*istr) >> valuei;

                auto ind1 = 0;
                auto ind2 = 0;
                (*istr) >> buf;
                ind1 = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);
                (*istr) >> buf;
                ind2 = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);

                if (valuei == 1) {
                    _anim1Equals.emplace(ind1, ind2);
                }
                else if (valuei == 2) {
                    _anim2Equals.emplace(ind1, ind2);
                }
            }
            else if (token == "RenderFrame" || token == "RenderFrames") {
                anims.push_back({0, render_fname, render_anim});

                (*istr) >> _renderAnimProcFrom;

                // One frame
                _renderAnimProcTo = _renderAnimProcFrom;

                // Many frames
                if (token == "RenderFrames") {
                    (*istr) >> _renderAnimProcTo;
                }

                // Check
                _renderAnimProcFrom = std::clamp(_renderAnimProcFrom, 0, 100);
                _renderAnimProcTo = std::clamp(_renderAnimProcTo, 0, 100);
            }
            else if (token == "RenderDir") {
                (*istr) >> buf;

                _renderAnimDir = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);
            }
            else if (token == "DisableShadow") {
                _shadowDisabled = true;
            }
            else if (token == "DrawSize") {
                (*istr) >> buf;
                _drawWidth = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);
                (*istr) >> buf;
                _drawHeight = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);
            }
            else if (token == "ViewSize") {
                (*istr) >> buf;
                _viewWidth = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);
                (*istr) >> buf;
                _viewHeight = _modelMngr._nameResolver.ResolveGenericValue(buf, &convert_value_fail);
            }
            else if (token == "DisableAnimationInterpolation") {
                disable_animation_interpolation = true;
            }
            else if (token == "RotationBone") {
                (*istr) >> buf;

                _rotationBone = _modelMngr.GetBoneHashedString(buf);
            }
            else {
                WriteLog("Unknown token '{}' in file '{}'", token, name);
            }
        }

        // Process pathes
        if (model.empty()) {
            WriteLog("'Model' section not found in file '{}'", name);
            return false;
        }

        // Check for correct param values
        if (convert_value_fail) {
            WriteLog("Invalid param values for file '{}'", name);
            return false;
        }

        // Load x file
        auto* xfile = _modelMngr.GetHierarchy(model);
        if (xfile == nullptr) {
            return false;
        }

        // Create animation
        _fileName = name;
        _hierarchy = xfile;

        // Single frame render
        if (!render_fname.empty() && !render_anim.empty()) {
            anims.push_back({-1, render_fname, render_anim});
        }

        // Create animation controller
        if (!anims.empty()) {
            _animController = std::make_unique<ModelAnimationController>(2);
        }

        // Parse animations
        if (_animController) {
            for (const auto& anim : anims) {
                string anim_path;
                if (anim.FileName == "ModelFile") {
                    anim_path = model;
                }
                else {
                    anim_path = _str(name).extractDir().combinePath(anim.FileName);
                }

                auto* set = _modelMngr.LoadAnimation(anim_path, anim.Name);
                if (set != nullptr) {
                    _animController->RegisterAnimationSet(set);
                    auto set_index = _animController->GetNumAnimationSets() - 1;

                    if (anim.Index == -1) {
                        _renderAnim = static_cast<int>(set_index);
                    }
                    else if (anim.Index != 0) {
                        _animIndexes.emplace(anim.Index, set_index);
                    }
                }
            }

            _numAnimationSets = _animController->GetNumAnimationSets();
            if (_numAnimationSets > 0) {
                // Add animation bones, not included to base hierarchy
                // All bones linked with animation in SetupAnimationOutput
                for (uint i = 0; i < _numAnimationSets; i++) {
                    const auto* set = _animController->GetAnimationSet(i);
                    const auto& bones_hierarchy = set->GetBonesHierarchy();
                    for (const auto& bone_hierarchy : bones_hierarchy) {
                        auto* bone = _hierarchy->_rootBone;
                        for (size_t b = 1; b < bone_hierarchy.size(); b++) {
                            auto* child = bone->Find(bone_hierarchy[b]);
                            if (child == nullptr) {
                                child = new ModelBone();
                                child->Name = bone_hierarchy[b];
                                bone->Children.push_back(unique_ptr<ModelBone>(child));
                            }
                            bone = child;
                        }
                    }
                }

                _animController->SetInterpolation(!disable_animation_interpolation);
                _hierarchy->SetupAnimationOutput(_animController.get());
            }
            else {
                _animController = nullptr;
            }
        }
    }
    // Load just x file
    else {
        auto* xfile = _modelMngr.GetHierarchy(name);
        if (xfile == nullptr) {
            return false;
        }

        // Create animation
        _fileName = name;
        _hierarchy = xfile;

        // Todo: process default animations
    }

    return true;
}

auto ModelInformation::GetAnimationIndex(uint& anim1, uint& anim2, float* speed, bool combat_first) const -> int
{
    STACK_TRACE_ENTRY();

    // Find index
    auto index = -1;
    if (combat_first) {
        index = GetAnimationIndexEx(anim1, anim2 | 0x8000, speed);
    }
    if (index == -1) {
        index = GetAnimationIndexEx(anim1, anim2, speed);
    }
    if (!combat_first && index == -1) {
        index = GetAnimationIndexEx(anim1, anim2 | 0x8000, speed);
    }
    if (index != -1) {
        return index;
    }

    // Find substitute animation
    const auto base_model_name = _modelMngr._nameResolver.ToHashedString(_fileName);
    const auto anim1_base = anim1;
    const auto anim2_base = anim2;
    while (index == -1) {
        auto model_name = base_model_name;
        const auto anim1_ = anim1;
        const auto anim2_ = anim2;
        if (_modelMngr._animNameResolver.ResolveCritterAnimationSubstitute(base_model_name, anim1_base, anim2_base, model_name, anim1, anim2) && (anim1 != anim1_ || anim2 != anim2_)) {
            index = GetAnimationIndexEx(anim1, anim2, speed);
        }
        else {
            break;
        }
    }

    return index;
}

auto ModelInformation::GetAnimationIndexEx(uint anim1, uint anim2, float* speed) const -> int
{
    STACK_TRACE_ENTRY();

    // Check equals
    const auto it1 = _anim1Equals.find(anim1);
    if (it1 != _anim1Equals.end()) {
        anim1 = (*it1).second;
    }
    const auto it2 = _anim2Equals.find(anim2 & 0x7FFF);
    if (it2 != _anim2Equals.end()) {
        anim2 = ((*it2).second | (anim2 & 0x8000));
    }

    // Make index
    const auto index = static_cast<int>((anim1 << 16) | anim2);

    // Speed
    if (speed != nullptr) {
        const auto it = _animSpeed.find(index);
        if (it != _animSpeed.end()) {
            *speed = it->second;
        }
        else {
            *speed = 1.0f;
        }
    }

    // Find number of animation
    const auto it = _animIndexes.find(index);
    if (it != _animIndexes.end()) {
        return it->second;
    }

    return -1;
}

auto ModelInformation::CreateInstance() -> ModelInstance*
{
    STACK_TRACE_ENTRY();

    auto* model = new ModelInstance(_modelMngr);
    model->_modelInfo = this;
    model->SetupFrame();

    if (_animController) {
        model->_bodyAnimController = _animController->Clone();

        if (_rotationBone) {
            model->_moveAnimController = _animController->Clone();
        }
    }

    return model;
}

auto ModelInformation::CreateCutShape(MeshData* mesh) const -> ModelCutData::Shape
{
    STACK_TRACE_ENTRY();

    ModelCutData::Shape shape;
    shape.GlobalTransformationMatrix = mesh->Owner->GlobalTransformationMatrix;

    if (mesh->Vertices.size() != 36) {
        shape.IsSphere = true;

        // Evaluate sphere radius
        auto vmin = mesh->Vertices[0].Position.x;
        auto vmax = mesh->Vertices[0].Position.x;

        for (const auto i : xrange(mesh->Vertices)) {
            const auto& v = mesh->Vertices[i];
            if (v.Position.x < vmin) {
                vmin = v.Position.x;
            }
            if (v.Position.x > vmax) {
                vmax = v.Position.x;
            }
        }

        shape.SphereRadius = (vmax - vmin) / 2.0f;
    }
    else {
        shape.IsSphere = false;

        // Calculate bounding box size
        float vmin[3];
        float vmax[3];

        for (size_t i = 0, j = mesh->Vertices.size(); i < j; i++) {
            const Vertex3D& v = mesh->Vertices[i];
            if (i == 0) {
                vmin[0] = vmax[0] = v.Position.x;
                vmin[1] = vmax[1] = v.Position.y;
                vmin[2] = vmax[2] = v.Position.z;
            }
            else {
                if (vmin[0] < v.Position.x) {
                    vmin[0] = v.Position.x;
                }
                if (vmin[1] < v.Position.y) {
                    vmin[1] = v.Position.y;
                }
                if (vmin[2] < v.Position.z) {
                    vmin[2] = v.Position.z;
                }
                if (vmax[0] > v.Position.x) {
                    vmax[0] = v.Position.x;
                }
                if (vmax[1] > v.Position.y) {
                    vmax[1] = v.Position.y;
                }
                if (vmax[2] > v.Position.z) {
                    vmax[2] = v.Position.z;
                }
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

ModelHierarchy::ModelHierarchy(ModelManager& model_mngr) : _modelMngr {model_mngr}
{
    STACK_TRACE_ENTRY();
}

void SetupBonesExt(multimap<uint, ModelBone*>& bones, ModelBone* bone, uint depth)
{
    STACK_TRACE_ENTRY();

    bones.emplace(depth, bone);

    for (auto&& child : bone->Children) {
        SetupBonesExt(bones, child.get(), depth + 1);
    }
}

void ModelHierarchy::SetupBones()
{
    STACK_TRACE_ENTRY();

    multimap<uint, ModelBone*> bones;
    SetupBonesExt(bones, _rootBone, 0);

    for (auto&& [id, bone] : bones) {
        _allBones.push_back(bone);
        if (bone->AttachedMesh) {
            _allDrawBones.push_back(bone);
        }
    }
}

static void SetupAnimationOutputExt(ModelAnimationController* anim_controller, ModelBone* bone)
{
    STACK_TRACE_ENTRY();

    anim_controller->RegisterAnimationOutput(bone->Name, bone->TransformationMatrix);

    for (auto& it : bone->Children) {
        SetupAnimationOutputExt(anim_controller, it.get());
    }
}

void ModelHierarchy::SetupAnimationOutput(ModelAnimationController* anim_controller)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    SetupAnimationOutputExt(anim_controller, _rootBone);
}

auto ModelHierarchy::GetTexture(string_view tex_name) -> MeshTexture*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto* texture = _modelMngr.LoadTexture(tex_name, _fileName);
    if (texture == nullptr) {
        WriteLog("Can't load texture '{}'", tex_name);
    }
    return texture;
}

auto ModelHierarchy::GetEffect(string_view name) -> RenderEffect*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto* effect = _modelMngr._effectMngr.LoadEffect(EffectUsage::Model, name);
    if (effect == nullptr) {
        WriteLog("Can't load effect '{}'", name);
    }
    return effect;
}

#endif
