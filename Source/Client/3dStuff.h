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

#include "3dAnimation.h"
#include "EffectManager.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "Settings.h"
#include "VisualParticles.h"

FO_BEGIN_NAMESPACE

class IAppRender;

enum class ModelAnimFlags : uint8_t
{
    None = 0x00,
    Init = 0x01,
    Freeze = 0x02,
    PlayOnce = 0x04,
    NoSmooth = 0x08,
    NoRotate = 0x10,
};

struct ModelBone;
class ModelInstance;
class ModelInformation;
class ModelHierarchy;

struct MeshTexture
{
    hstring Name {};
    ptr<RenderTexture> MainTex;
    frect32 AtlasOffsetData {};
};

struct MeshData
{
    void Load(DataReader& reader, HashResolver& hash_resolver);

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
    void Load(DataReader& reader, HashResolver& hash_resolver);
    void FixAfterLoad(ptr<ModelBone> root_bone);
    auto Find(hstring bone_name) const noexcept -> nptr<const ModelBone>;
    auto Find(hstring bone_name) noexcept -> nptr<ModelBone>;
    [[nodiscard]] auto GetAttachedMesh() const noexcept -> nptr<const MeshData> { return AttachedMesh ? nptr<const MeshData> {&*AttachedMesh} : nullptr; }
    [[nodiscard]] auto GetAttachedMesh() noexcept -> nptr<MeshData> { return AttachedMesh ? nptr<MeshData> {&*AttachedMesh} : nullptr; }

    hstring Name {};
    mat44 TransformationMatrix {};
    mat44 GlobalTransformationMatrix {};
    optional<MeshData> AttachedMesh {};
    vector<unique_ptr<ModelBone>> Children {};
    mat44 CombinedTransformationMatrix {};
};

struct ModelCutData
{
    struct Shape
    {
        mat44 GlobalTransformationMatrix {};
        bool IsSphere {};
        float32_t SphereRadius {};
        vec3 BBoxMin {};
        vec3 BBoxMax {};
    };

    vector<Shape> Shapes {};
    vector<int32_t> Layers {};
    hstring UnskinBone1 {};
    hstring UnskinBone2 {};
    Shape UnskinShape {};
    bool RevertUnskinShape {};
};

struct ModelAnimationData
{
    uint32_t Id {};
    int32_t Layer {};
    int32_t LayerValue {};
    hstring LinkBone {};
    string ChildName {};
    bool IsParticles {};
    float32_t RotX {};
    float32_t RotY {};
    float32_t RotZ {};
    float32_t MoveX {};
    float32_t MoveY {};
    float32_t MoveZ {};
    float32_t ScaleX {};
    float32_t ScaleY {};
    float32_t ScaleZ {};
    float32_t SpeedAjust {};
    vector<int32_t> DisabledLayer {};
    vector<hstring> DisabledMesh {};
    vector<tuple<string, hstring, int32_t>> TextureInfo {}; // Name, mesh, num
    vector<tuple<string, hstring>> EffectInfo {}; // Name, mesh
    vector<ptr<ModelCutData>> CutInfo {};
};

struct ModelParticleSystem
{
    uint32_t Id {};
    unique_ptr<ParticleSystem> Particle;
    ptr<const ModelBone> Bone;
    vec3 Move {};
    float32_t Rot {};
};

struct ModelAnimationCallback
{
    CritterStateAnim StateAnim {};
    CritterActionAnim ActionAnim {};
    float32_t NormalizedTime {};
    function<void()> Callback {};
};

class ModelManager final
{
    friend class ModelInstance;
    friend class ModelInformation;
    friend class ModelHierarchy;

public:
    using TextureLoader = function<pair<nptr<RenderTexture>, frect32>(string_view)>;

    ModelManager() = delete;
    ModelManager(ptr<RenderSettings> settings, ptr<FileSystem> resources, ptr<EffectManager> effect_mngr, ptr<IAppRender> render, ptr<GameTimer> game_time, ptr<HashResolver> hash_resolver, ptr<NameResolver> name_resolver, ptr<AnimationResolver> anim_name_resolver, TextureLoader tex_loader);
    ModelManager(const ModelManager&) = delete;
    ModelManager(ModelManager&&) noexcept = delete;
    auto operator=(const ModelManager&) = delete;
    auto operator=(ModelManager&&) noexcept = delete;
    ~ModelManager() = default;

    [[nodiscard]] auto GetBoneHashedString(string_view name) const -> hstring;

    [[nodiscard]] auto CreateModel(string_view name) -> unique_nptr<ModelInstance>;
    [[nodiscard]] auto LoadAnimation(string_view anim_fname, string_view anim_name) -> nptr<ModelAnimation>;
    [[nodiscard]] auto LoadTexture(string_view texture_name, string_view model_path) -> nptr<MeshTexture>;

    void PreloadModel(string_view name);

private:
    [[nodiscard]] auto LoadModel(string_view fname) -> nptr<ModelBone>;
    [[nodiscard]] auto GetInformation(string_view name) -> nptr<ModelInformation>;
    [[nodiscard]] auto GetHierarchy(string_view name) -> nptr<ModelHierarchy>;

    ptr<RenderSettings> _settings;
    ptr<FileSystem> _resources;
    ptr<EffectManager> _effectMngr;
    ptr<IAppRender> _render;
    ptr<GameTimer> _gameTime;
    mutable ptr<HashResolver> _hashResolver;
    ptr<NameResolver> _nameResolver;
    ptr<AnimationResolver> _animNameResolver;
    TextureLoader _textureLoader;
    ParticleManager _particleMngr;
    set<hstring> _processedFiles {};
    vector<unique_ptr<ModelBone>> _loadedModels {};
    vector<unique_ptr<ModelAnimation>> _loadedAnims {};
    vector<unique_ptr<ModelInformation>> _allModelInfos {};
    vector<unique_ptr<ModelHierarchy>> _hierarchyFiles {};
    float32_t _moveTransitionTime {0.25f};
    float32_t _globalSpeedAdjust {1.0f};
    int32_t _animUpdateThreshold {};
    color4 _lightColor {};
    hstring _headBone {};
    unordered_set<hstring> _legBones {};
    uint32_t _linkId {};
};

class ModelInstance final
{
    friend class ModelManager;
    friend class ModelInformation;
    friend class ModelHierarchy;

public:
    constexpr static int32_t FRAME_SCALE = 2;

    ModelInstance() = delete;
    ModelInstance(ptr<ModelManager> model_mngr, ptr<ModelInformation> info);
    ModelInstance(const ModelInstance&) = delete;
    ModelInstance(ModelInstance&&) noexcept = delete;
    auto operator=(const ModelInstance&) = delete;
    auto operator=(ModelInstance&&) noexcept = delete;
    ~ModelInstance() = default;

    [[nodiscard]] auto Convert2dTo3d(ipos32 pos) const -> vec3;
    [[nodiscard]] auto Convert3dTo2d(vec3 pos) const -> ipos32;
    [[nodiscard]] auto HasAnimation(CritterStateAnim state_anim, CritterActionAnim action_anim) const noexcept -> bool;
    [[nodiscard]] auto GetStateAnim() const noexcept -> CritterStateAnim { return _curStateAnim; }
    [[nodiscard]] auto GetActionAnim() const noexcept -> CritterActionAnim { return _curActionAnim; }
    [[nodiscard]] auto GetMovingAnim() const noexcept -> CritterActionAnim;
    [[nodiscard]] auto ResolveAnimation(CritterStateAnim& state_anim, CritterActionAnim& action_anim) -> bool;
    [[nodiscard]] auto NeedForceDraw() const noexcept -> bool { return _forceDraw; }
    [[nodiscard]] auto NeedDraw() const -> bool;
    [[nodiscard]] auto IsAnimationPlaying() const -> bool;
    [[nodiscard]] auto GetDrawSize() const -> isize32;
    [[nodiscard]] auto GetViewSize() const -> isize32;
    [[nodiscard]] auto FindBone(hstring bone_name) const noexcept -> nptr<const ModelBone>;
    [[nodiscard]] auto GetBonePos(hstring bone_name) const -> optional<ipos32>;
    [[nodiscard]] auto GetAnimDuration() const -> timespan;
    [[nodiscard]] auto GetAnimDuration(CritterStateAnim state_anim, CritterActionAnim action_anim) -> timespan;
    [[nodiscard]] auto HasBodyRotation() const { return !!_moveAnimController; }
    [[nodiscard]] auto GetMoveDirAngle() const noexcept -> float32_t { return _moveDirAngle; }

    void SetupFrame(isize32 draw_size);
    void StartMeshGeneration();
    void PrewarmParticles();
    auto PlayAnim(CritterStateAnim state_anim, CritterActionAnim action_anim, nptr<const int32_t> layers, float32_t ntime, ModelAnimFlags flags) -> bool;
    void SetDir(mdir dir, bool smooth_rotation);
    void SetLookDir(mdir dir);
    void SetMoveDir(mdir dir, bool smooth_rotation);
    void SetRotation(float32_t rx, float32_t ry, float32_t rz);
    void SetScale(float32_t sx, float32_t sy, float32_t sz);
    void SetSpeed(float32_t speed);
    void EnableShadow(bool enabled) { _shadowDisabled = !enabled; }
    void Draw();
    void Draw(const mat44& proj, float32_t scale);
    void MoveModel(ipos32 offset);
    void UpdatePose(bool staying_pose, bool moving, int32_t moving_speed);
    void SetAnimInitCallback(function<void(CritterStateAnim&, CritterActionAnim&)> anim_init);
    void RunParticle(string_view particle_name, hstring bone_name, vec3 move);

    void AddAnimationCallback(ModelAnimationCallback callback);
    void SetAnimationCallbacks(vector<ModelAnimationCallback> callbacks);
    auto TakeAnimationCallbacks() -> vector<ModelAnimationCallback>;
    void ClearAnimationCallbacks();

private:
    struct CombinedMesh
    {
        nptr<RenderEffect> DrawEffect {};
        unique_ptr<RenderDrawBuffer> MeshBuf;
        size_t EncapsulatedMeshCount {};
        vector<ptr<MeshData>> Meshes {};
        vector<uint32_t> MeshVertices {};
        vector<uint32_t> MeshIndices {};
        vector<int32_t> MeshAnimLayers {};
        size_t CurBoneMatrix {};
        vector<nptr<ModelBone>> SkinBones {};
        vector<mat44> SkinBoneOffsets {};
        nptr<const MeshTexture> Textures[MODEL_MAX_TEXTURES] {};
    };

    [[nodiscard]] auto CreateCombinedMesh() -> unique_ptr<CombinedMesh>;
    [[nodiscard]] auto CanBatchCombinedMesh(ptr<const CombinedMesh> combined_mesh, ptr<const MeshInstance> mesh_instance) const -> bool;
    [[nodiscard]] auto ProjectPoint(vec3 obj_pos, const mat44& model_matrix, const mat44& proj_matrix, const int32_t viewport[4], vec3& out_pos) const -> bool;
    [[nodiscard]] auto UnprojectPoint(vec3 win_pos, const mat44& model_matrix, const mat44& proj_matrix, const int32_t viewport[4], vec3& out_pos) const -> bool;
    [[nodiscard]] auto GetSpeed() const -> float32_t;
    [[nodiscard]] auto GetMovementSpeed() const -> float32_t;
    [[nodiscard]] auto GetTime() const -> nanotime;

    void GenerateCombinedMeshes();
    void FillCombinedMeshes(ptr<const ModelInstance> cur);
    void CombineMesh(ptr<const MeshInstance> mesh_instance, int32_t anim_layer);
    void BatchCombinedMesh(ptr<CombinedMesh> combined_mesh, ptr<const MeshInstance> mesh_instance, int32_t anim_layer);
    void CutCombinedMeshes(ptr<const ModelInstance> cur);
    void CutCombinedMesh(ptr<CombinedMesh> combined_mesh, ptr<const ModelCutData> cut);
    void ProcessAnimation(float32_t elapsed, ipos32 pos, float32_t scale);
    void DrawFrame(const mat44& proj, float32_t scale, bool direct_scene, bool draw_particles);
    void UpdateBoneMatrices(ptr<ModelBone> bone, ptr<const mat44> parent_matrix);
    void DrawCombinedMesh(ptr<CombinedMesh> combined_mesh, bool shadow_disabled);
    void DrawAllParticles();
    void SetAnimData(ModelAnimationData& data, bool clear);
    void RefreshMoveAnimation();

    ptr<ModelManager> _modelMngr;
    isize32 _frameSize {};
    mat44 _frameProj {};
    mat44 _drawProj {};
    CritterStateAnim _curStateAnim {};
    CritterActionAnim _curActionAnim {};
    vector<unique_ptr<CombinedMesh>> _combinedMeshes {};
    size_t _actualCombinedMeshesCount {};
    bool _disableCulling {};
    vector<unique_ptr<MeshInstance>> _allMeshes {};
    vector<bool> _allMeshesDisabled {};
    ptr<ModelInformation> _modelInfo;
    optional<ModelAnimationController> _bodyAnimController {};
    optional<ModelAnimationController> _moveAnimController {};
    int32_t _curLayers[MODEL_LAYERS_COUNT] {};
    int32_t _curTrack {};
    nanotime _lastDrawTime {};
    mat44 _matRot {};
    mat44 _matScale {};
    mat44 _matScaleBase {};
    mat44 _matRotBase {};
    mat44 _matTransBase {};
    float32_t _speedAdjustBase {};
    float32_t _speedAdjustCur {};
    float32_t _speedAdjustLink {};
    bool _shadowDisabled {};
    float32_t _lookDirAngle {};
    float32_t _moveDirAngle {};
    float32_t _targetMoveDirAngle {};
    vec3 _groundPos {};
    float32_t _animPosProc {};
    float32_t _animPosTime {};
    float32_t _animDuration {};
    bool _allowMeshGeneration {};
    vector<ptr<ModelCutData>> _allCuts {};
    function<void(CritterStateAnim&, CritterActionAnim&)> _animInitCallback {};
    bool _isStayingPose {};
    bool _isMoving {};
    bool _isMovingBack {};
    int32_t _curMovingAnimIndex {-1};
    CritterActionAnim _curMovingAnim {};
    bool _turnAnimPlaying {};
    int32_t _curMoveTrack {};
    float32_t _movingSpeedFactor {};
    bool _isRunning {};
    bool _noRotate {};
    float32_t _deferredLookDirAngle {};
    vector<ModelParticleSystem> _modelParticles {};
    vec3 _moveOffset {};
    bool _forceDraw {};
    bool _directSceneDraw {};
    vector<ModelAnimationCallback> _animationCallbacks {};

    // Derived animations
    vector<unique_ptr<ModelInstance>> _children {};
    nptr<ModelInstance> _parent {};
    nptr<ModelBone> _parentBone {};
    mat44 _parentMatrix {};
    vector<ptr<ModelBone>> _linkBones {};
    ModelAnimationData _animLink {};
    bool _childChecker {};
};

class ModelInformation final
{
    friend class ModelManager;
    friend class ModelInstance;
    friend class ModelHierarchy;

public:
    ModelInformation() = delete;
    explicit ModelInformation(ptr<ModelManager> model_mngr);
    ModelInformation(const ModelInformation&) = delete;
    ModelInformation(ModelInformation&&) noexcept = default;
    auto operator=(const ModelInformation&) = delete;
    auto operator=(ModelInformation&&) noexcept = delete;
    ~ModelInformation() = default;

private:
    struct BakedModelDescriptionCutInfo
    {
        string FileName {};
        vector<int32_t> Layers {};
        vector<string> Shapes {};
        string UnskinBone1 {};
        string UnskinBone2 {};
        string UnskinShape {};
        bool RevertUnskinShape {};
    };

    struct BakedModelDescriptionLink
    {
        ModelAnimationData Data {};
        vector<BakedModelDescriptionCutInfo> CutInfo {};
    };

    struct BakedModelDescriptionAnimEntry
    {
        int32_t StateAnim {};
        int32_t ActionAnim {};
        string FileName {};
        string Name {};
    };

    struct BakedModelDescriptionAnimLayerValue
    {
        int32_t StateAnim {};
        int32_t ActionAnim {};
        int32_t Layer {};
        int32_t LayerValue {};
    };

    [[nodiscard]] auto Load(string_view name) -> bool;
    [[nodiscard]] auto LoadBaked(string_view name, DataReader& reader) -> bool;
    [[nodiscard]] auto ReadBakedModelDescriptionLink(DataReader& reader) const -> BakedModelDescriptionLink;
    [[nodiscard]] auto ReadBakedModelDescriptionCutInfo(DataReader& reader) const -> BakedModelDescriptionCutInfo;
    [[nodiscard]] auto ReadBakedModelDescriptionAnimEntry(DataReader& reader) const -> BakedModelDescriptionAnimEntry;
    [[nodiscard]] auto ReadBakedModelDescriptionAnimLayerValue(DataReader& reader) const -> BakedModelDescriptionAnimLayerValue;

    [[nodiscard]] auto CreateCutShape(ptr<const MeshData> mesh) const -> ModelCutData::Shape;
    [[nodiscard]] auto GetAnimationIndex(CritterStateAnim& state_anim, CritterActionAnim& action_anim, nptr<float32_t> speed) -> int32_t;
    [[nodiscard]] auto GetAnimationIndexEx(CritterStateAnim state_anim, CritterActionAnim action_anim, nptr<float32_t> speed) const -> int32_t;
    [[nodiscard]] auto CreateInstance() -> unique_ptr<ModelInstance>;

    ptr<ModelManager> _modelMngr;
    string _fileName {};
    string _pathName {};
    nptr<ModelHierarchy> _hierarchy {};
    optional<ModelAnimationController> _animController {};
    unordered_map<CritterStateAnim, CritterStateAnim> _stateAnimEquals {};
    unordered_map<CritterActionAnim, CritterActionAnim> _actionAnimEquals {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, int32_t> _animIndexes {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, float32_t> _animSpeed {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, vector<pair<int32_t, int32_t>>> _animLayerValues {};
    unordered_set<hstring> _fastTransitionBones {};
    ModelAnimationData _animDataDefault {};
    vector<ModelAnimationData> _animData {};
    bool _shadowDisabled {};
    bool _disableBackwardAnim {};
    isize32 _drawSize {};
    isize32 _viewSize {};
    hstring _rotationBone {};
};

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
    ~ModelHierarchy() = default;

private:
    void SetupBones();
    void SetupAnimationOutput(ptr<ModelAnimationController> anim_controller);
    auto GetTexture(string_view tex_name) -> ptr<MeshTexture>;
    auto GetEffect(string_view name) -> ptr<RenderEffect>;

    ptr<ModelManager> _modelMngr;
    string _fileName {};
    ptr<ModelBone> _rootBone;
    vector<ptr<ModelBone>> _allBones {};
    vector<ptr<ModelBone>> _allDrawBones {};
};

FO_END_NAMESPACE

#endif
