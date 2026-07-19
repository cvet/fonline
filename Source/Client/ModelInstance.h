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

#include "ModelAnimation.h"
#include "ModelBounds.h"
#include "ModelHierarchy.h"
#include "ModelInformation.h"
#include "ModelSpriteLayout.h"
#include "VisualParticles.h"

FO_BEGIN_NAMESPACE

class ModelInstance;
class ModelManager;

struct ModelParticleSystem
{
    uint32_t Id {};
    unique_ptr<ParticleSystem> Particle;
    nptr<const ModelInstance> Owner {};
    uint32_t JointIndex {};
    vec3 Move {};
    float32_t Rot {};
};

class ModelInstance final
{
    friend class ModelManager;
    friend class ModelInformation;
    friend class ModelHierarchy;

public:
    constexpr static int32_t FRAME_SCALE = MODEL_SPRITE_FRAME_SCALE;

    ModelInstance() = delete;
    ModelInstance(ptr<ModelManager> model_mngr, ptr<ModelInformation> info);
    ModelInstance(const ModelInstance&) = delete;
    ModelInstance(ModelInstance&&) noexcept = delete;
    auto operator=(const ModelInstance&) = delete;
    auto operator=(ModelInstance&&) noexcept = delete;
    ~ModelInstance();

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
    [[nodiscard]] auto GetLightingSize() const noexcept -> isize32 { return _lightingDrawSize; }
    [[nodiscard]] auto GetSpriteBounds() const -> optional<ModelSpriteBounds>;
    [[nodiscard]] auto GetViewRect() const -> irect32;
    [[nodiscard]] auto FindBone(hstring bone_name) const noexcept -> nptr<const ModelBone>;
    [[nodiscard]] auto GetBonePos(hstring bone_name) const -> optional<ipos32>;
    [[nodiscard]] auto GetAnimDuration() const -> timespan;
    [[nodiscard]] auto GetAnimDuration(CritterStateAnim state_anim, CritterActionAnim action_anim) -> timespan;
    [[nodiscard]] auto HasBodyRotation() const { return !!_moveAnimController; }
    [[nodiscard]] auto GetMoveDirAngle() const noexcept -> float32_t { return _moveDirAngle; }

    void SetupFrame(isize32 draw_size);
    void PrepareFrameLayout();
    void RequestRedraw() noexcept;
    void StartMeshGeneration();
    void PrewarmParticles();
    auto PlayAnim(CritterStateAnim state_anim, CritterActionAnim action_anim, nptr<const int32_t> layers, float32_t ntime, ModelAnimFlags flags) -> bool;
    void SetDir(mdir dir, bool smooth_rotation);
    void SetLookDir(mdir dir);
    void SetMoveDir(mdir dir, bool smooth_rotation);
    void SetRotation(float32_t rx, float32_t ry, float32_t rz);
    void SetScale(float32_t sx, float32_t sy, float32_t sz);
    void SetSpeed(float32_t speed);
    void EnableShadow(bool enabled);
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
    struct PoseJointBinding
    {
        nptr<const ModelInstance> Owner {};
        uint32_t JointIndex {};
        nptr<const ModelBone> SourceBone {};
    };

    struct SkinBinding
    {
        nptr<const ModelInstance> Owner {};
        uint32_t JointIndex {};
        nptr<ModelBone> SourceBone {};
        mat44 InverseBindMatrix {1.0f};
    };

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
        vector<SkinBinding> SkinBindings {};
        vector<vindex_t> SpriteVertices {};
        bool SpriteBoundsValid {};
        bool HasSpriteGeometry {};
        nptr<const MeshTexture> Textures[MODEL_MAX_TEXTURES] {};
    };

    [[nodiscard]] auto CreateCombinedMesh() -> unique_ptr<CombinedMesh>;
    [[nodiscard]] auto CanBatchCombinedMesh(ptr<const CombinedMesh> combined_mesh, ptr<const MeshInstance> mesh_instance) const -> bool;
    [[nodiscard]] auto ProjectPoint(vec3 obj_pos, const mat44& model_matrix, const mat44& proj_matrix, const int32_t viewport[4], vec3& out_pos) const -> bool;
    [[nodiscard]] auto UnprojectPoint(vec3 win_pos, const mat44& model_matrix, const mat44& proj_matrix, const int32_t viewport[4], vec3& out_pos) const -> bool;
    [[nodiscard]] auto GetSpeed() const -> float32_t;
    [[nodiscard]] auto GetMovementSpeed() const -> float32_t;
    [[nodiscard]] auto GetTime() const -> nanotime;
    [[nodiscard]] auto FindPoseJoint(hstring bone_name) const noexcept -> optional<PoseJointBinding>;
    [[nodiscard]] auto GetPoseJointIndex(ptr<const ModelBone> bone) const -> uint32_t;
    [[nodiscard]] auto GetWorldMatrix(uint32_t joint_index) const -> const mat44&;
    [[nodiscard]] auto GetProceduralJointRotationAngle(uint32_t joint_index) const noexcept -> optional<float32_t>;
    [[nodiscard]] auto FillAnimationProceduralRotations(array<ModelAnimationRuntimePose::ProceduralLocalRotation, ModelAnimationRuntimePose::MAX_PROCEDURAL_ROTATIONS>& procedural_rotations) const -> size_t;
    [[nodiscard]] auto MakeRootTransformation(ipos32 pos, float32_t scale, bool direct_scene) const -> mat44;

    void GenerateCombinedMeshes();
    void InvalidateCombinedMeshes() noexcept;
    void FillCombinedMeshes(ptr<const ModelInstance> cur);
    void CombineMesh(ptr<const ModelInstance> owner, ptr<const MeshInstance> mesh_instance, int32_t anim_layer);
    void BatchCombinedMesh(ptr<CombinedMesh> combined_mesh, ptr<const ModelInstance> owner, ptr<const MeshInstance> mesh_instance, int32_t anim_layer);
    void CutCombinedMeshes(ptr<const ModelInstance> cur);
    void CutCombinedMesh(ptr<CombinedMesh> combined_mesh, ptr<const ModelCutData> cut);
    void ProcessAnimation(float32_t elapsed, ipos32 pos, float32_t scale);
    void DrawFrame(const mat44& proj, float32_t scale, bool direct_scene, bool draw_particles);
    void FillAnimationTrackInputs(nptr<const ModelAnimationController> controller, bool active, array<vector<uint8_t>, 2>& joint_masks, array<ModelAnimationRuntimePose::TrackInput, 2>& track_inputs) const;
    void SnapshotAnimationWorldMatrices();
    void BuildRestWorldMatrices();
    void DrawCombinedMesh(ptr<CombinedMesh> combined_mesh, bool shadow_disabled);
    void DrawAllParticles();
    void SetAnimData(ModelAnimationData& data, bool clear);
    void RefreshMoveAnimation();
    void RefreshFrameLayout();
    void RefreshConfigurationLayout();

    ptr<ModelManager> _modelMngr;
    isize32 _frameSize {};
    isize32 _layoutDrawSize {};
    isize32 _lightingDrawSize {};
    irect32 _viewRect {};
    optional<ModelBounds3D> _configurationModelBounds {};
    optional<ModelBounds3D> _configurationViewBounds {};
    uint64_t _configurationLayoutRevision {};
    mat44 _frameProj {};
    mat44 _drawProj {};
    CritterStateAnim _curStateAnim {};
    CritterActionAnim _curActionAnim {};
    vector<unique_ptr<CombinedMesh>> _combinedMeshes {};
    size_t _actualCombinedMeshesCount {};
    uint64_t _combinedMeshGenerationRevision {};
    bool _disableCulling {};
    vector<unique_ptr<MeshInstance>> _allMeshes {};
    vector<bool> _allMeshesDisabled {};
    ptr<ModelInformation> _modelInfo;
    optional<ModelAnimationController> _bodyAnimController {};
    optional<ModelAnimationController> _moveAnimController {};
    unique_nptr<ModelAnimationRuntimePose> _animationRuntimePose {};
    vector<mat44> _worldMatrices {};
    array<vector<uint8_t>, 2> _animationBodyJointMasks {};
    array<vector<uint8_t>, 2> _animationMovementJointMasks {};
    int32_t _curLayers[MODEL_LAYERS_COUNT] {};
    int32_t _curTrack {};
    nanotime _lastDrawTime {};
    mat44 _matRot {};
    mat44 _matScale {1.0f};
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
    bool _frameLayoutDirty {};
    bool _spriteBoundsPoseReady {};
    bool _directSceneDraw {};
    vector<ModelAnimationCallback> _animationCallbacks {};

    // Derived animations
    vector<unique_ptr<ModelInstance>> _children {};
    nptr<ModelInstance> _parent {};
    uint32_t _parentJointIndex {std::numeric_limits<uint32_t>::max()};
    mat44 _parentMatrix {};
    vector<ModelPoseJointLink> _linkJoints {};
    ModelAnimationData _animLink {};
    bool _childChecker {};
};

FO_END_NAMESPACE

#endif
