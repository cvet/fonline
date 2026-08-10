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

FO_BEGIN_NAMESPACE

enum class ModelAnimFlags : uint8_t
{
    None = 0x00,
    Init = 0x01,
    Freeze = 0x02,
    PlayOnce = 0x04,
    NoSmooth = 0x08,
    NoRotate = 0x10,
};

struct ModelAnimationCallback
{
    CritterStateAnim StateAnim {};
    CritterActionAnim ActionAnim {};
    float32_t NormalizedTime {};
    function<void()> Callback {};
};

auto BuildModelAnimationBoundBones(const_span<hstring> canonical_joint_names, const_span<hstring> runtime_joint_names, const_span<uint8_t> canonical_joint_present, string_view context) -> unordered_set<hstring>;

class ModelAnimationController final
{
public:
    struct TrackState
    {
        bool Enabled {};
        float32_t Speed {};
        float32_t Weight {};
        float32_t Position {};
        int32_t ClipIndex {-1};
        bool Reversed {};
    };

    explicit ModelAnimationController(int32_t track_count);
    ModelAnimationController(const ModelAnimationController&) = delete;
    ModelAnimationController(ModelAnimationController&&) noexcept = default;
    auto operator=(const ModelAnimationController&) = delete;
    auto operator=(ModelAnimationController&&) noexcept = delete;
    ~ModelAnimationController() = default;

    [[nodiscard]] auto Copy() const -> ModelAnimationController;
    [[nodiscard]] auto GetAnimDuration(int32_t index) const -> float32_t;
    [[nodiscard]] auto GetTrackEnable(int32_t track) const -> bool;
    [[nodiscard]] auto GetTrackPosition(int32_t track) const -> float32_t;
    [[nodiscard]] auto GetTrackSpeed(int32_t track) const -> float32_t;
    [[nodiscard]] auto GetTrackState(int32_t track) const -> TrackState;
    [[nodiscard]] auto IsTrackBoneEnabled(int32_t track, hstring bone_name) const -> bool;
    [[nodiscard]] auto GetAnimationsCount() const -> int32_t;

    auto RegisterAnimation(uint32_t clip_index, float32_t duration, bool reversed, unordered_set<hstring> bound_bones) -> int32_t;
    void SetTrackAnimation(int32_t track, int32_t anim_index, nptr<const unordered_set<hstring>> allowed_bones);
    void ResetBonesTransition(int32_t skip_track, const vector<hstring>& bone_names);
    void ResetEvents();
    void AddEventEnable(int32_t track, bool enable, float32_t start_time);
    void AddEventSpeed(int32_t track, float32_t speed, float32_t start_time, float32_t smooth_time);
    void AddEventWeight(int32_t track, float32_t weight, float32_t start_time, float32_t smooth_time);
    void SetTrackEnable(int32_t track, bool enable);
    void SetTrackPosition(int32_t track, float32_t position);
    void SetTrackSpeed(int32_t track, float32_t speed);
    void AdvanceTimeline(float32_t time);

private:
    struct AnimationBinding
    {
        uint32_t ClipIndex {};
        float32_t Duration {};
        bool Reversed {};
        unordered_set<hstring> BoundBones {};
    };

    struct Track
    {
        enum class EventType : uint8_t
        {
            Enable,
            Speed,
            Weight
        };

        struct Event
        {
            EventType Type {};
            float32_t ValueTo {};
            float32_t StartTime {};
            float32_t SmoothTime {};
            optional<float32_t> ValueFrom {};
        };

        bool Enabled {};
        float32_t Speed {};
        float32_t Weight {};
        float32_t Position {};
        int32_t AnimationIndex {-1};
        optional<unordered_set<hstring>> AllowedBones {};
        unordered_set<hstring> SuppressedBones {};
        vector<Event> Events {};
    };

    shared_ptr<vector<AnimationBinding>> _animationBindings {};
    vector<Track> _tracks {};
    float32_t _eventsTime {};
};

FO_DECLARE_EXCEPTION(ModelAnimationRuntimeException);

struct ModelAnimationRuntimeJoint
{
    string Name {};
    int32_t ParentIndex {-1};
    mat44 RestLocalTransform {1.0f};
};

struct ModelAnimationRuntimeBinding
{
    int32_t StateAnim {};
    int32_t ActionAnim {};
    uint32_t ClipIndex {};
    bool Reversed {};
};

struct ModelAnimationRuntimeTransform
{
    vec3 Translation {};
    quaternion Rotation {1.0f, 0.0f, 0.0f, 0.0f};
    vec3 Scale {1.0f, 1.0f, 1.0f};
};

struct ModelPoseJoint
{
    int32_t ParentIndex {-1};
    mat44 RestLocalTransform {1.0f};
};

struct ModelPoseJointLink
{
    uint32_t ParentJointIndex {};
    uint32_t ChildJointIndex {};
};

class ModelAnimationRuntimeClip final
{
    friend class ModelAnimationRuntimeAccess;
    friend class SafeAlloc;

public:
    ModelAnimationRuntimeClip() = delete;
    ModelAnimationRuntimeClip(const ModelAnimationRuntimeClip&) = delete;
    ModelAnimationRuntimeClip(ModelAnimationRuntimeClip&&) noexcept;
    auto operator=(const ModelAnimationRuntimeClip&) = delete;
    auto operator=(ModelAnimationRuntimeClip&&) noexcept = delete;
    ~ModelAnimationRuntimeClip();

    [[nodiscard]] auto GetSourceFile() const noexcept -> string_view;
    [[nodiscard]] auto GetClipName() const noexcept -> string_view;
    [[nodiscard]] auto GetSourceSignature() const noexcept -> uint64_t;
    [[nodiscard]] auto GetDuration() const noexcept -> float32_t;
    [[nodiscard]] auto GetJointPresence() const noexcept -> const_span<uint8_t>;

private:
    class Impl;
    explicit ModelAnimationRuntimeClip(unique_ptr<Impl> impl);

    unique_ptr<Impl> _impl;
};

class ModelAnimationRuntimeRig final
{
    friend class ModelAnimationRuntimeAccess;
    friend class SafeAlloc;

public:
    ModelAnimationRuntimeRig() = delete;
    ModelAnimationRuntimeRig(const ModelAnimationRuntimeRig&) = delete;
    ModelAnimationRuntimeRig(ModelAnimationRuntimeRig&&) noexcept;
    auto operator=(const ModelAnimationRuntimeRig&) = delete;
    auto operator=(ModelAnimationRuntimeRig&&) noexcept = delete;
    ~ModelAnimationRuntimeRig();

    [[nodiscard]] auto GetRigSignature() const noexcept -> uint64_t;
    [[nodiscard]] auto GetCacheSignature() const noexcept -> uint64_t;
    [[nodiscard]] auto GetJointCount() const noexcept -> size_t;
    [[nodiscard]] auto GetJointName(size_t index) const -> string_view;
    [[nodiscard]] auto GetBaseJointMapping() const noexcept -> const_span<uint32_t>;
    [[nodiscard]] auto GetBaseJointPresence() const noexcept -> const_span<uint8_t>;
    [[nodiscard]] auto GetClipCount() const noexcept -> size_t;
    [[nodiscard]] auto GetClip(size_t index) const -> const ModelAnimationRuntimeClip&;
    [[nodiscard]] auto FindClip(string_view source_file, string_view clip_name) const noexcept -> nptr<const ModelAnimationRuntimeClip>;
    [[nodiscard]] auto GetBindings() const noexcept -> const_span<ModelAnimationRuntimeBinding>;
    [[nodiscard]] auto FindBinding(int32_t state_anim, int32_t action_anim) const noexcept -> nptr<const ModelAnimationRuntimeBinding>;

private:
    class Impl;
    explicit ModelAnimationRuntimeRig(unique_ptr<Impl> impl);

    unique_ptr<Impl> _impl;
};

class ModelAnimationRuntimePose final
{
public:
    static constexpr size_t MAX_PROCEDURAL_ROTATIONS = 2;

    struct TrackInput
    {
        int32_t ClipIndex {-1};
        bool Enabled {};
        bool Reversed {};
        float32_t Position {};
        float32_t Weight {};
        const_span<uint8_t> JointMask {};
    };

    struct ProceduralLocalRotation
    {
        size_t JointIndex {};
        quaternion Rotation {1.0f, 0.0f, 0.0f, 0.0f};
    };

    ModelAnimationRuntimePose() = delete;
    explicit ModelAnimationRuntimePose(ptr<const ModelAnimationRuntimeRig> rig);
    ModelAnimationRuntimePose(const ModelAnimationRuntimePose&) = delete;
    ModelAnimationRuntimePose(ModelAnimationRuntimePose&&) noexcept = delete;
    auto operator=(const ModelAnimationRuntimePose&) = delete;
    auto operator=(ModelAnimationRuntimePose&&) noexcept = delete;
    ~ModelAnimationRuntimePose();

    [[nodiscard]] auto GetJointCount() const noexcept -> size_t;
    [[nodiscard]] auto GetBodyLocalTransform(size_t joint_index) const -> ModelAnimationRuntimeTransform;
    [[nodiscard]] auto GetFinalLocalTransform(size_t joint_index) const -> ModelAnimationRuntimeTransform;
    [[nodiscard]] auto GetWorldMatrices() const noexcept -> const_span<mat44>;

    void BuildModelMatrices(const mat44& root_matrix);
    void Evaluate(const array<TrackInput, 2>& body_tracks, const array<TrackInput, 2>& movement_tracks, const mat44& root_matrix, const_span<ProceduralLocalRotation> procedural_rotations = {});
    void OverrideWorldMatrix(size_t joint_index, const mat44& world_matrix);

private:
    class Impl;
    void ResetLocalsToRestPose();

    unique_ptr<Impl> _impl;
};

[[nodiscard]] auto LoadModelAnimationRuntimeRig(const_span<uint8_t> data, string_view model_description, string_view base_model, bool nearest_sampling) -> unique_ptr<ModelAnimationRuntimeRig>;
void ValidateModelAnimationRuntimeBaseJoints(const ModelAnimationRuntimeRig& rig, const_span<ModelAnimationRuntimeJoint> source_joints, string_view context);
[[nodiscard]] auto ResolveModelAnimationRuntimeCanonicalJoints(const ModelAnimationRuntimeRig& rig, const_span<ModelAnimationRuntimeJoint> hierarchy_joints, string_view context) -> vector<uint32_t>;
[[nodiscard]] auto BuildModelPoseJointNameIndex(const_span<hstring> runtime_names, string_view context) -> unordered_map<hstring, uint32_t>;
[[nodiscard]] auto ResolveModelPoseJointLinks(const unordered_map<hstring, uint32_t>& parent_joint_indexes, const_span<hstring> child_runtime_names) -> vector<ModelPoseJointLink>;
void BuildModelRestWorldMatrices(const_span<ModelPoseJoint> joints, const mat44& root_matrix, span<mat44> world_matrices);

FO_END_NAMESPACE

#endif
