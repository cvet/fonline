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

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

#include "3dAnimation.h"
#include "Application.h"
#include "EffectManager.h"
#include "FileSystem.h"
#include "GeometryHelper.h"
#include "Settings.h"
#include "VisualParticles.h"

FO_BEGIN_NAMESPACE();

// Todo: remove unnecessary allocations from 3d

static constexpr size_t MODEL_LAYERS_COUNT = 30;

constexpr uint32 ANIMATION_STAY = 0x01;
constexpr uint32 ANIMATION_ONE_TIME = 0x02;
constexpr auto ANIMATION_PERIOD(uint32 proc) -> uint32
{
    return 0x04 | proc << 16;
}
constexpr uint32 ANIMATION_NO_SMOOTH = 0x08;
constexpr uint32 ANIMATION_INIT = 0x10;
constexpr uint32 ANIMATION_NO_ROTATE = 0x20;

struct ModelBone;
class ModelInstance;
class ModelInformation;
class ModelHierarchy;

struct MeshTexture
{
    hstring Name {};
    RenderTexture* MainTex {};
    float32 AtlasOffsetData[4] {};
};

struct MeshData
{
    void Load(DataReader& reader, HashResolver& hash_resolver);

    ModelBone* Owner {};
    vector<Vertex3D> Vertices {};
    vector<vindex_t> Indices {};
    string DiffuseTexture {};
    vector<hstring> SkinBoneNames {};
    vector<mat44> SkinBoneOffsets {};
    vector<ModelBone*> SkinBones {};
    string EffectName {};
};

struct MeshInstance
{
    MeshData* Mesh {};
    bool Disabled {};
    MeshTexture* CurTexures[MODEL_MAX_TEXTURES] {};
    MeshTexture* DefaultTexures[MODEL_MAX_TEXTURES] {};
    MeshTexture* LastTexures[MODEL_MAX_TEXTURES] {};
    RenderEffect* CurEffect {};
    RenderEffect* DefaultEffect {};
    RenderEffect* LastEffect {};
};

struct ModelBone
{
    void Load(DataReader& reader, HashResolver& hash_resolver);
    void FixAfterLoad(ModelBone* root_bone);
    auto Find(hstring bone_name) noexcept -> ModelBone*;

    hstring Name {};
    mat44 TransformationMatrix {};
    mat44 GlobalTransformationMatrix {};
    unique_ptr<MeshData> AttachedMesh {};
    vector<unique_ptr<ModelBone>> Children {};
    mat44 CombinedTransformationMatrix {};
};

struct ModelCutData
{
    struct Shape
    {
        mat44 GlobalTransformationMatrix {};
        bool IsSphere {};
        float32 SphereRadius {};
        vec3 BBoxMin {};
        vec3 BBoxMax {};
    };

    vector<Shape> Shapes {};
    vector<int32> Layers {};
    hstring UnskinBone1 {};
    hstring UnskinBone2 {};
    Shape UnskinShape {};
    bool RevertUnskinShape {};
};

struct ModelAnimationData
{
    uint32 Id {};
    int32 Layer {};
    int32 LayerValue {};
    hstring LinkBone {};
    string ChildName {};
    bool IsParticles {};
    float32 RotX {};
    float32 RotY {};
    float32 RotZ {};
    float32 MoveX {};
    float32 MoveY {};
    float32 MoveZ {};
    float32 ScaleX {};
    float32 ScaleY {};
    float32 ScaleZ {};
    float32 SpeedAjust {};
    vector<int32> DisabledLayer {};
    vector<hstring> DisabledMesh {};
    vector<tuple<string, hstring, int32>> TextureInfo {}; // Name, mesh, num
    vector<tuple<string, hstring>> EffectInfo {}; // Name, mesh
    vector<ModelCutData*> CutInfo {};
};

struct ModelParticleSystem
{
    uint32 Id {};
    unique_ptr<ParticleSystem> Particle {};
    const ModelBone* Bone {};
    vec3 Move {};
    float32 Rot {};
};

struct ModelAnimationCallback
{
    CritterStateAnim StateAnim {};
    CritterActionAnim ActionAnim {};
    float32 NormalizedTime {};
    std::function<void()> Callback {};
};

class ModelManager final
{
    friend class ModelInstance;
    friend class ModelInformation;
    friend class ModelHierarchy;

public:
    using TextureLoader = std::function<pair<RenderTexture*, FRect>(string_view)>;

    ModelManager() = delete;
    ModelManager(RenderSettings& settings, FileSystem& resources, EffectManager& effect_mngr, GameTimer& game_time, HashResolver& hash_resolver, NameResolver& name_resolver, AnimationResolver& anim_name_resolver, TextureLoader tex_loader);
    ModelManager(const ModelManager&) = delete;
    ModelManager(ModelManager&&) noexcept = delete;
    auto operator=(const ModelManager&) = delete;
    auto operator=(ModelManager&&) noexcept = delete;
    ~ModelManager() = default;

    [[nodiscard]] auto GetBoneHashedString(string_view name) const -> hstring;

    [[nodiscard]] auto CreateModel(string_view name) -> unique_ptr<ModelInstance>;
    [[nodiscard]] auto LoadAnimation(string_view anim_fname, string_view anim_name) -> ModelAnimation*;
    [[nodiscard]] auto LoadTexture(string_view texture_name, string_view model_path) -> MeshTexture*;

    void PreloadModel(string_view name);

private:
    [[nodiscard]] auto LoadModel(string_view fname) -> ModelBone*;
    [[nodiscard]] auto GetInformation(string_view name) -> ModelInformation*;
    [[nodiscard]] auto GetHierarchy(string_view name) -> ModelHierarchy*;

    static auto MatrixProject(float32 objx, float32 objy, float32 objz, const float32 model_matrix[16], const float32 proj_matrix[16], const int32 viewport[4], float32* winx, float32* winy, float32* winz) -> bool;
    static auto MatrixUnproject(float32 winx, float32 winy, float32 winz, const float32 model_matrix[16], const float32 proj_matrix[16], const int32 viewport[4], float32* objx, float32* objy, float32* objz) -> bool;

    RenderSettings& _settings;
    FileSystem& _resources;
    EffectManager& _effectMngr;
    GameTimer& _gameTime;
    HashResolver& _hashResolver;
    NameResolver& _nameResolver;
    AnimationResolver& _animNameResolver;
    TextureLoader _textureLoader;
    GeometryHelper _geometry;
    ParticleManager _particleMngr;
    set<hstring> _processedFiles {};
    vector<unique_ptr<ModelBone>> _loadedModels {};
    vector<unique_ptr<ModelAnimation>> _loadedAnimSets {};
    vector<unique_ptr<ModelInformation>> _allModelInfos {};
    vector<unique_ptr<ModelHierarchy>> _hierarchyFiles {};
    float32 _moveTransitionTime {0.25f};
    float32 _globalSpeedAdjust {1.0f};
    int32 _animUpdateThreshold {};
    color4 _lightColor {};
    hstring _headBone {};
    unordered_set<hstring> _legBones {};
    uint32 _linkId {};
    bool _nonConstHelper {};
};

class ModelInstance final
{
    friend class ModelManager;
    friend class ModelInformation;
    friend class ModelHierarchy;

public:
    constexpr static int32 FRAME_SCALE = 2;

    ModelInstance() = delete;
    ModelInstance(ModelManager& model_mngr, ModelInformation* info);
    ModelInstance(const ModelInstance&) = delete;
    ModelInstance(ModelInstance&&) noexcept = delete;
    auto operator=(const ModelInstance&) = delete;
    auto operator=(ModelInstance&&) noexcept = delete;
    ~ModelInstance() = default;

    [[nodiscard]] auto Convert2dTo3d(ipos pos) const -> vec3;
    [[nodiscard]] auto Convert3dTo2d(vec3 pos) const -> ipos;
    [[nodiscard]] auto HasAnimation(CritterStateAnim state_anim, CritterActionAnim action_anim) const noexcept -> bool;
    [[nodiscard]] auto GetStateAnim() const noexcept -> CritterStateAnim { return _curStateAnim; }
    [[nodiscard]] auto GetActionAnim() const noexcept -> CritterActionAnim { return _curActionAnim; }
    [[nodiscard]] auto GetMovingAnim() const noexcept -> CritterActionAnim;
    [[nodiscard]] auto ResolveAnimation(CritterStateAnim& state_anim, CritterActionAnim& action_anim) const -> bool;
    [[nodiscard]] auto NeedForceDraw() const noexcept -> bool { return _forceDraw; }
    [[nodiscard]] auto NeedDraw() const -> bool;
    [[nodiscard]] auto IsAnimationPlaying() const -> bool;
    [[nodiscard]] auto GetRenderFramesData() const -> tuple<float32, int32, int32, int32>;
    [[nodiscard]] auto GetDrawSize() const -> isize;
    [[nodiscard]] auto GetViewSize() const -> isize;
    [[nodiscard]] auto FindBone(hstring bone_name) const noexcept -> const ModelBone*;
    [[nodiscard]] auto GetBonePos(hstring bone_name) const -> optional<ipos>;
    [[nodiscard]] auto GetAnimDuration() const -> timespan;
    [[nodiscard]] auto IsCombatMode() const noexcept -> bool;

    void SetupFrame(isize draw_size);
    void StartMeshGeneration();
    void PrewarmParticles();
    auto SetAnimation(CritterStateAnim state_anim, CritterActionAnim action_anim, const int32* layers, uint32 flags) -> bool;
    void SetDir(uint8 dir, bool smooth_rotation);
    void SetLookDirAngle(int32 dir_angle);
    void SetMoveDirAngle(int32 dir_angle, bool smooth_rotation);
    void SetRotation(float32 rx, float32 ry, float32 rz);
    void SetScale(float32 sx, float32 sy, float32 sz);
    void SetSpeed(float32 speed);
    void EnableShadow(bool enabled) { _shadowDisabled = !enabled; }
    void Draw();
    void MoveModel(ipos offset);
    void SetMoving(bool enabled, int32 speed = 0);
    void SetCombatMode(bool enabled);
    void RunParticle(string_view particle_name, hstring bone_name, vec3 move);

    // Todo: incapsulate model animation callbacks
    vector<ModelAnimationCallback> AnimationCallbacks {};

private:
    struct CombinedMesh
    {
        RenderEffect* DrawEffect {};
        unique_ptr<RenderDrawBuffer> MeshBuf {};
        size_t EncapsulatedMeshCount {};
        vector<MeshData*> Meshes {};
        vector<uint32> MeshVertices {};
        vector<uint32> MeshIndices {};
        vector<int32> MeshAnimLayers {};
        size_t CurBoneMatrix {};
        vector<ModelBone*> SkinBones {};
        vector<mat44> SkinBoneOffsets {};
        MeshTexture* Textures[MODEL_MAX_TEXTURES] {};
    };

    [[nodiscard]] auto CanBatchCombinedMesh(const CombinedMesh* combined_mesh, const MeshInstance* mesh_instance) const -> bool;
    [[nodiscard]] auto GetSpeed() const -> float32;
    [[nodiscard]] auto GetTime() const -> nanotime;

    void GenerateCombinedMeshes();
    void FillCombinedMeshes(const ModelInstance* cur);
    void CombineMesh(const MeshInstance* mesh_instance, int32 anim_layer);
    void BatchCombinedMesh(CombinedMesh* combined_mesh, const MeshInstance* mesh_instance, int32 anim_layer);
    void CutCombinedMeshes(const ModelInstance* cur);
    void CutCombinedMesh(CombinedMesh* combined_mesh, const ModelCutData* cut);
    void ProcessAnimation(float32 elapsed, ipos pos, float32 scale);
    void UpdateBoneMatrices(ModelBone* bone, const mat44* parent_matrix);
    void DrawCombinedMesh(CombinedMesh* combined_mesh, bool shadow_disabled);
    void DrawAllParticles();
    void SetAnimData(ModelAnimationData& data, bool clear);
    void RefreshMoveAnimation();

    ModelManager& _modelMngr;
    isize _frameSize {};
    mat44 _frameProj {};
    mat44 _frameProjColMaj {};
    CritterStateAnim _curStateAnim {};
    CritterActionAnim _curActionAnim {};
    vector<unique_ptr<CombinedMesh>> _combinedMeshes {};
    size_t _actualCombinedMeshesCount {};
    bool _disableCulling {};
    vector<unique_ptr<MeshInstance>> _allMeshes {};
    vector<bool> _allMeshesDisabled {};
    ModelInformation* _modelInfo {};
    unique_ptr<ModelAnimationController> _bodyAnimController {};
    unique_ptr<ModelAnimationController> _moveAnimController {};
    int32 _currentLayers[MODEL_LAYERS_COUNT] {};
    uint32 _currentTrack {};
    nanotime _lastDrawTime {};
    nanotime _endTime {};
    mat44 _matRot {};
    mat44 _matScale {};
    mat44 _matScaleBase {};
    mat44 _matRotBase {};
    mat44 _matTransBase {};
    float32 _speedAdjustBase {};
    float32 _speedAdjustCur {};
    float32 _speedAdjustLink {};
    bool _shadowDisabled {};
    float32 _lookDirAngle {};
    float32 _moveDirAngle {};
    float32 _targetMoveDirAngle {};
    vec3 _groundPos {};
    float32 _animPosProc {};
    float32 _animPosTime {};
    float32 _animPosPeriod {};
    bool _allowMeshGeneration {};
    vector<ModelCutData*> _allCuts {};
    bool _isMoving {};
    bool _isMovingBack {};
    int32 _curMovingAnimIndex {-1};
    CritterActionAnim _curMovingAnim {};
    bool _playTurnAnimation {};
    bool _isCombatMode {};
    uint32 _currentMoveTrack {};
    float32 _movingSpeedFactor {};
    bool _isRunning {};
    bool _noRotate {};
    float32 _deferredLookDirAngle {};
    vector<ModelParticleSystem> _modelParticles {};
    vec3 _moveOffset {};
    bool _forceDraw {};

    // Derived animations
    vector<unique_ptr<ModelInstance>> _children {};
    ModelInstance* _parent {};
    raw_ptr<ModelBone> _parentBone {};
    mat44 _parentMatrix {};
    vector<ModelBone*> _linkBones {};
    vector<mat44> _linkMatricles {};
    ModelAnimationData _animLink {};
    bool _childChecker {};

    bool _nonConstHelper {};
};

class ModelInformation final
{
    friend class ModelManager;
    friend class ModelInstance;
    friend class ModelHierarchy;

public:
    ModelInformation() = delete;
    explicit ModelInformation(ModelManager& model_mngr);
    ModelInformation(const ModelInformation&) = delete;
    ModelInformation(ModelInformation&&) noexcept = default;
    auto operator=(const ModelInformation&) = delete;
    auto operator=(ModelInformation&&) noexcept = delete;
    ~ModelInformation() = default;

private:
    [[nodiscard]] auto GetAnimationIndex(CritterStateAnim& state_anim, CritterActionAnim& action_anim, float32* speed, bool combat_first) const -> int32;
    [[nodiscard]] auto GetAnimationIndexEx(CritterStateAnim state_anim, CritterActionAnim action_anim, bool combat, float32* speed) const -> int32;
    [[nodiscard]] auto CreateCutShape(MeshData* mesh) const -> ModelCutData::Shape;

    [[nodiscard]] auto Load(string_view name) -> bool;
    [[nodiscard]] auto CreateInstance() -> ModelInstance*;

    ModelManager& _modelMngr;
    string _fileName {};
    string _pathName {};
    ModelHierarchy* _hierarchy {};
    unique_ptr<ModelAnimationController> _animController {};
    size_t _numAnimationSets {};
    unordered_map<CritterStateAnim, CritterStateAnim> _stateAnimEquals {};
    unordered_map<CritterActionAnim, CritterActionAnim> _actionAnimEquals {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, int32> _animIndexes {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, int32> _animCombatIndexes {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, float32> _animSpeed {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, vector<pair<int32, int32>>> _animLayerValues {};
    unordered_set<hstring> _fastTransitionBones {};
    ModelAnimationData _animDataDefault {};
    vector<ModelAnimationData> _animData {};
    int32 _renderAnim {};
    int32 _renderAnimProcFrom {};
    int32 _renderAnimProcTo {100};
    int32 _renderAnimDir {};
    bool _shadowDisabled {};
    isize _drawSize {};
    isize _viewSize {};
    hstring _rotationBone {};
};

class ModelHierarchy final
{
    friend class ModelManager;
    friend class ModelInstance;
    friend class ModelInformation;

public:
    ModelHierarchy() = delete;
    explicit ModelHierarchy(ModelManager& model_mngr);
    ModelHierarchy(const ModelHierarchy&) = delete;
    ModelHierarchy(ModelHierarchy&&) noexcept = default;
    auto operator=(const ModelHierarchy&) = delete;
    auto operator=(ModelHierarchy&&) noexcept = delete;
    ~ModelHierarchy() = default;

private:
    void SetupBones();
    void SetupAnimationOutput(ModelAnimationController* anim_controller);
    auto GetTexture(string_view tex_name) -> MeshTexture*;
    auto GetEffect(string_view name) -> RenderEffect*;

    ModelManager& _modelMngr;
    string _fileName {};
    ModelBone* _rootBone {};
    vector<ModelBone*> _allBones {};
    vector<ModelBone*> _allDrawBones {};
    bool _nonConstHelper {};
};

FO_END_NAMESPACE();

#endif
