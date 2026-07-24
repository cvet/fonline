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

FO_BEGIN_NAMESPACE

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

struct MeshData;
struct ModelBone;
class ModelHierarchy;
class ModelInstance;
class ModelManager;

class ModelInformation final
{
    friend class ModelManager;
    friend class ModelInstance;
    friend class ModelHierarchy;

public:
    ModelInformation() = delete;
    explicit ModelInformation(ptr<ModelManager> model_mngr);
    ModelInformation(const ModelInformation&) = delete;
    ModelInformation(ModelInformation&&) noexcept;
    auto operator=(const ModelInformation&) = delete;
    auto operator=(ModelInformation&&) noexcept = delete;
    ~ModelInformation();

    [[nodiscard]] auto GetAvailableAnimations() const -> vector<pair<CritterStateAnim, CritterActionAnim>>;
    [[nodiscard]] auto GetRootBone() const -> ptr<const ModelBone>;

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

    struct BakedModelDescriptionAnimationEntry
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
    [[nodiscard]] auto ReadBakedModelDescriptionLink(DataReader& reader, string_view context) const -> BakedModelDescriptionLink;
    [[nodiscard]] auto ReadBakedModelDescriptionCutInfo(DataReader& reader) const -> BakedModelDescriptionCutInfo;
    [[nodiscard]] auto ReadBakedModelDescriptionAnimationEntry(DataReader& reader) const -> BakedModelDescriptionAnimationEntry;
    [[nodiscard]] auto ReadBakedModelDescriptionAnimLayerValue(DataReader& reader) const -> BakedModelDescriptionAnimLayerValue;

    void IndexDirectPoseJoints();
    void IndexAnimationPoseJoints(const ModelAnimationRuntimeRig& rig);
    void IndexPoseJointLookups();
    [[nodiscard]] auto CalculateHierarchyBounds() const -> optional<ModelBounds3D>;
    [[nodiscard]] auto CreateCutShape(ptr<const MeshData> mesh) const -> ModelCutData::Shape;
    [[nodiscard]] auto GetAnimationIndex(CritterStateAnim& state_anim, CritterActionAnim& action_anim, nptr<float32_t> speed) -> int32_t;
    [[nodiscard]] auto GetAnimationIndexEx(CritterStateAnim state_anim, CritterActionAnim action_anim, nptr<float32_t> speed) const -> int32_t;
    [[nodiscard]] auto CreateInstance() -> unique_ptr<ModelInstance>;

    ptr<ModelManager> _modelMngr;
    string _fileName {};
    string _pathName {};
    nptr<ModelHierarchy> _hierarchy {};
    optional<ModelAnimationController> _animController {};
    unique_nptr<ModelAnimationRuntimeRig> _animationRuntimeRig {};
    vector<nptr<const ModelBone>> _poseBones {}; // Physical base bones; null for animation-contributed canonical joints.
    vector<hstring> _poseJointCanonicalNames {}; // Immutable canonical source identity.
    vector<hstring> _poseJointRuntimeNames {}; // Exact legacy lookup identity; the base root uses its file alias.
    unordered_map<hstring, uint32_t> _poseJointIndexes {};
    unordered_map<ptr<const ModelBone>, uint32_t> _poseBoneJointIndexes {};
    vector<ModelPoseJoint> _restPoseJoints {};
    optional<uint32_t> _bodyRotationJointIndex {};
    optional<uint32_t> _headRotationJointIndex {};
    unordered_map<CritterStateAnim, CritterStateAnim> _stateAnimEquals {};
    unordered_map<CritterActionAnim, CritterActionAnim> _actionAnimEquals {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, int32_t> _animIndexes {};
    vector<optional<ModelBounds3D>> _animationBounds {};
    ModelBounds3D _modelBounds {};
    ModelBounds3D _viewBounds {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, float32_t> _animSpeed {};
    unordered_map<pair<CritterStateAnim, CritterActionAnim>, vector<pair<int32_t, int32_t>>> _animLayerValues {};
    unordered_set<hstring> _fastTransitionBones {};
    ModelAnimationData _animDataDefault {};
    vector<ModelAnimationData> _animData {};
    vector<unique_ptr<ModelCutData>> _cutData {};
    bool _shadowDisabled {};
    bool _disableBackwardAnim {};
    hstring _rotationBone {};
};

FO_END_NAMESPACE

#endif
