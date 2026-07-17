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

#include "ModelAnimation.h"

#if FO_ENABLE_3D

#include "ModelAnimationData.h"
#include "ModelBakedData.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/skeleton_utils.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/transform.h"
#include "ozz/base/span.h"

FO_BEGIN_NAMESPACE

auto BuildModelAnimationBoundBones(const_span<hstring> canonical_joint_names, const_span<hstring> runtime_joint_names, const_span<uint8_t> canonical_joint_present, string_view context) -> unordered_set<hstring>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(canonical_joint_names.size() == runtime_joint_names.size() && canonical_joint_names.size() == canonical_joint_present.size(), "Animation joint metadata counts differ", context, canonical_joint_names.size(), runtime_joint_names.size(), canonical_joint_present.size());

    unordered_set<hstring> bound_bones;
    bound_bones.reserve(canonical_joint_names.size());

    for (size_t joint_index = 0; joint_index < canonical_joint_names.size(); joint_index++) {
        if (canonical_joint_present[joint_index] != 0 && canonical_joint_names[joint_index] == runtime_joint_names[joint_index]) {
            bound_bones.emplace(runtime_joint_names[joint_index]);
        }
    }

    return bound_bones;
}

ModelAnimationController::ModelAnimationController(int32_t track_count)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track_count >= 0, "Track count is negative", track_count);

    if (track_count != 0) {
        _animationBindings = SafeAlloc::MakeShared<vector<AnimationBinding>>();
        _tracks.resize(track_count);
    }
}

auto ModelAnimationController::Copy() const -> ModelAnimationController
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationController clone {0};
    clone._animationBindings = _animationBindings;
    clone._tracks.resize(_tracks.size());
    clone._eventsTime = 0.0f;
    return clone;
}

auto ModelAnimationController::RegisterAnimation(uint32_t clip_index, float32_t duration, bool reversed, unordered_set<hstring> bound_bones) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_animationBindings, "Animation controller has no tracks");
    FO_VERIFY_AND_THROW(clip_index <= numeric_cast<uint32_t>(std::numeric_limits<int32_t>::max()), "Animation clip index exceeds controller range", clip_index);
    FO_VERIFY_AND_THROW(std::isfinite(duration) && duration > 0.0f && std::isfinite(1.0f / duration), "Animation duration is invalid", duration);
    FO_VERIFY_AND_THROW(_animationBindings->size() < numeric_cast<size_t>(std::numeric_limits<int32_t>::max()), "Animation controller table is full", _animationBindings->size());

    const int32_t animation_index = numeric_cast<int32_t>(_animationBindings->size());
    _animationBindings->emplace_back(AnimationBinding {.ClipIndex = clip_index, .Duration = duration, .Reversed = reversed, .BoundBones = std::move(bound_bones)});
    return animation_index;
}

auto ModelAnimationController::GetAnimationDuration(int32_t index) const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_animationBindings, "Animation controller has no tracks");
    FO_VERIFY_AND_THROW(index >= 0, "Index is negative", index);
    FO_VERIFY_AND_THROW(index < numeric_cast<int32_t>(_animationBindings->size()), "Animation index is outside animation table bounds", index, _animationBindings->size());

    return (*_animationBindings)[index].Duration;
}

auto ModelAnimationController::GetTrackEnable(int32_t track) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    return _tracks[track].Enabled;
}

auto ModelAnimationController::GetTrackSpeed(int32_t track) const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    return _tracks[track].Speed;
}

auto ModelAnimationController::GetTrackPosition(int32_t track) const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    return _tracks[track].Position;
}

auto ModelAnimationController::GetTrackState(int32_t track) const -> TrackState
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    const Track& track_data = _tracks[track];
    int32_t clip_index = -1;
    bool reversed = false;

    if (track_data.AnimationIndex >= 0) {
        FO_STRONG_ASSERT(_animationBindings && track_data.AnimationIndex < numeric_cast<int32_t>(_animationBindings->size()), "Animation track references invalid controller metadata", track, track_data.AnimationIndex, _animationBindings ? _animationBindings->size() : 0);
        const AnimationBinding& animation = (*_animationBindings)[track_data.AnimationIndex];
        clip_index = numeric_cast<int32_t>(animation.ClipIndex);
        reversed = animation.Reversed;
    }

    return TrackState {
        .Enabled = track_data.Enabled,
        .Speed = track_data.Speed,
        .Weight = track_data.Weight,
        .Position = track_data.Position,
        .ClipIndex = clip_index,
        .Reversed = reversed,
    };
}

auto ModelAnimationController::IsTrackBoneEnabled(int32_t track, hstring bone_name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    const Track& track_data = _tracks[track];

    if (track_data.AnimationIndex < 0) {
        return false;
    }

    FO_STRONG_ASSERT(_animationBindings && track_data.AnimationIndex < numeric_cast<int32_t>(_animationBindings->size()), "Animation track references invalid controller metadata", track, track_data.AnimationIndex, _animationBindings ? _animationBindings->size() : 0);
    const AnimationBinding& animation = (*_animationBindings)[track_data.AnimationIndex];
    return animation.BoundBones.count(bone_name) != 0 && (!track_data.AllowedBones || track_data.AllowedBones->count(bone_name) != 0) && track_data.SuppressedBones.count(bone_name) == 0;
}

auto ModelAnimationController::GetAnimationsCount() const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    return _animationBindings ? numeric_cast<int32_t>(_animationBindings->size()) : 0;
}

void ModelAnimationController::SetTrackAnimation(int32_t track, int32_t anim_index, nptr<const unordered_set<hstring>> allowed_bones)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());
    FO_VERIFY_AND_THROW(_animationBindings, "Animation controller has no tracks");
    FO_VERIFY_AND_THROW(anim_index >= 0, "Animation index is negative", anim_index);
    FO_VERIFY_AND_THROW(anim_index < numeric_cast<int32_t>(_animationBindings->size()), "Animation index is outside animation table bounds", anim_index, _animationBindings->size());

    optional<unordered_set<hstring>> filtered_allowed_bones;

    if (allowed_bones) {
        const AnimationBinding& animation = (*_animationBindings)[anim_index];
        filtered_allowed_bones.emplace();
        filtered_allowed_bones->reserve(std::min(animation.BoundBones.size(), allowed_bones->size()));

        for (hstring bone_name : animation.BoundBones) {
            if (allowed_bones->count(bone_name) != 0) {
                filtered_allowed_bones->emplace(bone_name);
            }
        }
    }

    Track& track_data = _tracks[track];
    track_data.AllowedBones = std::move(filtered_allowed_bones);
    track_data.SuppressedBones.clear();
    track_data.AnimationIndex = anim_index;
}

void ModelAnimationController::ResetBonesTransition(int32_t skip_track, const vector<hstring>& bone_names)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(skip_track >= 0, "Skip track is negative", skip_track);
    FO_VERIFY_AND_THROW(skip_track < numeric_cast<int32_t>(_tracks.size()), "Skipped animation track index is outside track table bounds", skip_track, _tracks.size());

    vector<unordered_set<hstring>> suppressed_bones;
    suppressed_bones.reserve(_tracks.size());

    for (const Track& track : _tracks) {
        suppressed_bones.emplace_back(track.SuppressedBones);
    }

    const size_t skipped_track = numeric_cast<size_t>(skip_track);

    for (size_t i = 0; i < _tracks.size(); i++) {
        if (i == skipped_track) {
            continue;
        }

        for (hstring bone_name : bone_names) {
            suppressed_bones[i].emplace(bone_name);
        }
    }

    for (size_t i = 0; i < _tracks.size(); i++) {
        if (i != skipped_track) {
            _tracks[i].SuppressedBones = std::move(suppressed_bones[i]);
        }
    }
}

void ModelAnimationController::ResetEvents()
{
    FO_STACK_TRACE_ENTRY();

    _eventsTime = 0.0f;

    for (auto& t : _tracks) {
        t.Events.clear();
    }
}

void ModelAnimationController::AddEventEnable(int32_t track, bool enable, float32_t start_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Events.emplace_back(Track::Event {.Type = Track::EventType::Enable, .ValueTo = enable ? 1.0f : -1.0f, .StartTime = start_time, .SmoothTime = 0.0f});
}

void ModelAnimationController::AddEventSpeed(int32_t track, float32_t speed, float32_t start_time, float32_t smooth_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Events.emplace_back(Track::Event {.Type = Track::EventType::Speed, .ValueTo = speed, .StartTime = start_time, .SmoothTime = smooth_time});
}

void ModelAnimationController::AddEventWeight(int32_t track, float32_t weight, float32_t start_time, float32_t smooth_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Events.emplace_back(Track::Event {.Type = Track::EventType::Weight, .ValueTo = weight, .StartTime = start_time, .SmoothTime = smooth_time});
}

void ModelAnimationController::SetTrackEnable(int32_t track, bool enable)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Enabled = enable;
}

void ModelAnimationController::SetTrackPosition(int32_t track, float32_t position)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Position = position;
}

void ModelAnimationController::SetTrackSpeed(int32_t track, float32_t speed)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(track >= 0, "Track is negative", track);
    FO_VERIFY_AND_THROW(track < numeric_cast<int32_t>(_tracks.size()), "Animation track index is outside track table bounds", track, _tracks.size());

    _tracks[track].Speed = speed;
}

void ModelAnimationController::AdvanceTimeline(float32_t time)
{
    FO_STACK_TRACE_ENTRY();

    _eventsTime += time;

    for (auto& track : _tracks) {
        for (auto it = track.Events.begin(); it != track.Events.end();) {
            auto event = make_ptr(&*it);

            if (_eventsTime >= event->StartTime) {
                bool erase = false;
                float32_t value;

                if (_eventsTime < event->StartTime + event->SmoothTime) {
                    FO_VERIFY_AND_THROW(event->SmoothTime > 0.0f, "Event smooth time must be positive");

                    if (!event->ValueFrom.has_value()) {
                        if (event->Type == Track::EventType::Speed) {
                            event->ValueFrom = track.Speed;
                        }
                        else if (event->Type == Track::EventType::Weight) {
                            event->ValueFrom = track.Weight;
                        }
                        else {
                            event->ValueFrom = 0.0f;
                        }
                    }

                    value = lerp(event->ValueFrom.value(), event->ValueTo, (_eventsTime - event->StartTime) / event->SmoothTime);
                }
                else {
                    erase = true;
                    value = event->ValueTo;
                }

                if (event->Type == Track::EventType::Enable) {
                    track.Enabled = value > 0.0f;
                }
                else if (event->Type == Track::EventType::Speed) {
                    track.Speed = value;
                }
                else if (event->Type == Track::EventType::Weight) {
                    track.Weight = value;
                }

                if (erase) {
                    it = track.Events.erase(it);
                    continue;
                }
            }

            ++it;
        }

        // Add track time
        if (track.Enabled) {
            track.Position += time * track.Speed;
        }
    }
}

static_assert(MODEL_ANIMATION_MAX_JOINTS == MODEL_ANIMATION_RIG_MAX_JOINTS);

template<typename T>
static auto DeserializeModelAnimationRuntimeObject(const_span<uint8_t> payload, string_view context) -> T;
static void ValidateModelAnimationRuntimeSkeleton(const ozz::animation::Skeleton& skeleton, string_view context);
static void ValidateModelAnimationRuntimeTransform(const ozz::math::Transform& transform, size_t joint_index, string_view context);
static void ValidateModelAnimationRuntimeJointList(const_span<ModelAnimationRuntimeJoint> joints, string_view context);
static void ValidateModelAnimationRuntimeRestPose(const ModelAnimationRuntimeJoint& joint, const ozz::animation::Skeleton& skeleton, uint32_t canonical_index, string_view context);
static auto ComposeModelAnimationRuntimeTransform(const ozz::math::Transform& transform) noexcept -> mat44;
static auto ConvertModelAnimationRuntimeMatrix(const mat44& matrix) noexcept -> ozz::math::Float4x4;
static auto ConvertModelAnimationRuntimeMatrix(const ozz::math::Float4x4& matrix) noexcept -> mat44;
static auto ExtractModelAnimationRuntimeTransform(ozz::span<const ozz::math::SoaTransform> locals, size_t joint_index) noexcept -> ModelAnimationRuntimeTransform;
static void ValidateModelAnimationRuntimePoseRig(const ModelAnimationRuntimeRig& rig);
static void ValidateModelAnimationRuntimeMatrix(const mat44& matrix, string_view context);
static void ValidateModelAnimationRuntimeProceduralRotations(const_span<ModelAnimationRuntimePose::ProceduralLocalRotation> procedural_rotations, size_t joint_count);
static void ApplyModelAnimationRuntimeProceduralRotations(const_span<ModelAnimationRuntimePose::ProceduralLocalRotation> procedural_rotations, ozz::span<ozz::math::SoaTransform> locals) noexcept;

struct ModelAnimationRuntimeResolvedTrackInput
{
    nptr<const ModelAnimationRuntimeClip> Clip {};
    bool Active {};
    bool Nearest {};
    float32_t SampleRatio {};
    float32_t Weight {};
    const_span<uint8_t> JointMask {};
};

static auto ResolveModelAnimationRuntimeTrackInput(const ModelAnimationRuntimeRig& rig, const ModelAnimationRuntimePose::TrackInput& input, size_t joint_count, string_view context) -> ModelAnimationRuntimeResolvedTrackInput;
static auto SnapModelAnimationRuntimeSampleTime(float32_t source_time, const_span<float32_t> sample_times) noexcept -> float32_t;
static auto IsModelAnimationRuntimeJointPresent(const ModelAnimationRuntimeResolvedTrackInput& track, size_t joint) noexcept -> bool;
static void SampleModelAnimationRuntimeTrack(const ModelAnimationRuntimeResolvedTrackInput& track, ozz::animation::SamplingJob::Context& context, ozz::span<ozz::math::SoaTransform> locals);
static void BlendModelAnimationRuntimeTracks(const array<ModelAnimationRuntimeResolvedTrackInput, 2>& tracks, size_t joint_count, ozz::span<const ozz::math::SoaTransform> rest_pose, ozz::span<const ozz::math::SoaTransform> track_locals0, ozz::span<const ozz::math::SoaTransform> track_locals1, ozz::span<ozz::math::SimdFloat4> joint_weights0, ozz::span<ozz::math::SimdFloat4> joint_weights1, ozz::span<ozz::math::SoaTransform> output);
static void SelectModelAnimationRuntimeMovementPose(const array<ModelAnimationRuntimeResolvedTrackInput, 2>& movement_tracks, size_t joint_count, ozz::span<const ozz::math::SoaTransform> body_locals, ozz::span<const ozz::math::SoaTransform> movement_locals, ozz::span<ozz::math::SimdFloat4> body_joint_weights, ozz::span<ozz::math::SimdFloat4> movement_joint_weights, ozz::span<ozz::math::SoaTransform> output);

class ModelAnimationRuntimeClip::Impl final
{
public:
    Impl(string source_file, string clip_name, uint64_t source_signature, ozz::animation::Animation animation, ModelAnimationJointRemap joint_remap) :
        SourceFile {std::move(source_file)},
        ClipName {std::move(clip_name)},
        SourceSignature {source_signature},
        Duration {animation.duration()},
        Animation {std::move(animation)},
        JointRemap {std::move(joint_remap)}
    {
        FO_STACK_TRACE_ENTRY();
    }

    string SourceFile {};
    string ClipName {};
    uint64_t SourceSignature {};
    float32_t Duration {};
    ozz::animation::Animation Animation {};
    ModelAnimationJointRemap JointRemap {};
};

class ModelAnimationRuntimeRig::Impl final
{
public:
    Impl(uint64_t rig_signature, uint64_t cache_signature, ozz::animation::Skeleton skeleton, ModelAnimationJointRemap base_joint_remap, vector<ModelAnimationRuntimeClip> clips, vector<ModelAnimationRigBinding> bindings) :
        RigSignature {rig_signature},
        CacheSignature {cache_signature},
        JointCount {numeric_cast<size_t>(skeleton.num_joints())},
        Skeleton {std::move(skeleton)},
        BaseJointRemap {std::move(base_joint_remap)},
        Clips {std::move(clips)}
    {
        FO_STACK_TRACE_ENTRY();

        Bindings.reserve(bindings.size());

        for (const ModelAnimationRigBinding& binding : bindings) {
            Bindings.emplace_back(ModelAnimationRuntimeBinding {binding.StateAnim, binding.ActionAnim, binding.ClipIndex, binding.Reversed});
        }
    }

    uint64_t RigSignature {};
    uint64_t CacheSignature {};
    size_t JointCount {};
    ozz::animation::Skeleton Skeleton {};
    ModelAnimationJointRemap BaseJointRemap {};
    vector<ModelAnimationRuntimeClip> Clips {};
    vector<ModelAnimationRuntimeBinding> Bindings {};
};

class ModelAnimationRuntimeAccess final
{
public:
    [[nodiscard]] static auto CreateClip(string source_file, string clip_name, uint64_t source_signature, ozz::animation::Animation animation, ModelAnimationJointRemap joint_remap) -> ModelAnimationRuntimeClip
    {
        FO_STACK_TRACE_ENTRY();

        return ModelAnimationRuntimeClip {SafeAlloc::MakeUnique<ModelAnimationRuntimeClip::Impl>(std::move(source_file), std::move(clip_name), source_signature, std::move(animation), std::move(joint_remap))};
    }

    [[nodiscard]] static auto CreateRig(uint64_t rig_signature, uint64_t cache_signature, ozz::animation::Skeleton skeleton, ModelAnimationJointRemap base_joint_remap, vector<ModelAnimationRuntimeClip> clips, vector<ModelAnimationRigBinding> bindings) -> unique_ptr<ModelAnimationRuntimeRig>
    {
        FO_STACK_TRACE_ENTRY();

        ValidateModelAnimationRuntimeSkeleton(skeleton, "runtime rig");
        auto rig = SafeAlloc::MakeUnique<ModelAnimationRuntimeRig>(SafeAlloc::MakeUnique<ModelAnimationRuntimeRig::Impl>(rig_signature, cache_signature, std::move(skeleton), std::move(base_joint_remap), std::move(clips), std::move(bindings)));
        ValidateModelAnimationRuntimePoseRig(*rig);
        return rig;
    }

    [[nodiscard]] static auto GetAnimation(const ModelAnimationRuntimeClip& clip) noexcept -> const ozz::animation::Animation&
    {
        FO_NO_STACK_TRACE_ENTRY();

        return clip._impl->Animation;
    }

    [[nodiscard]] static auto GetJointRemap(const ModelAnimationRuntimeClip& clip) noexcept -> const ModelAnimationJointRemap&
    {
        FO_NO_STACK_TRACE_ENTRY();

        return clip._impl->JointRemap;
    }

    [[nodiscard]] static auto GetSkeleton(const ModelAnimationRuntimeRig& rig) noexcept -> const ozz::animation::Skeleton&
    {
        FO_NO_STACK_TRACE_ENTRY();

        return rig._impl->Skeleton;
    }

    [[nodiscard]] static auto GetBaseJointRemap(const ModelAnimationRuntimeRig& rig) noexcept -> const ModelAnimationJointRemap&
    {
        FO_NO_STACK_TRACE_ENTRY();

        return rig._impl->BaseJointRemap;
    }
};

class ModelAnimationRuntimePose::Impl final
{
    friend class ModelAnimationRuntimePose;

public:
    explicit Impl(ptr<const ModelAnimationRuntimeRig> rig);
    Impl(const Impl&) = delete;
    Impl(Impl&&) noexcept = delete;
    auto operator=(const Impl&) = delete;
    auto operator=(Impl&&) noexcept = delete;
    ~Impl() = default;

private:
    ptr<const ModelAnimationRuntimeRig> _rig;
    ozz::animation::SamplingJob::Context _bodyContext0;
    ozz::animation::SamplingJob::Context _bodyContext1;
    ozz::animation::SamplingJob::Context _movementContext0;
    ozz::animation::SamplingJob::Context _movementContext1;
    ozz::vector<ozz::math::SoaTransform> _bodyTrackLocals0 {};
    ozz::vector<ozz::math::SoaTransform> _bodyTrackLocals1 {};
    ozz::vector<ozz::math::SoaTransform> _movementTrackLocals0 {};
    ozz::vector<ozz::math::SoaTransform> _movementTrackLocals1 {};
    ozz::vector<ozz::math::SoaTransform> _bodyLocals {};
    ozz::vector<ozz::math::SoaTransform> _movementLocals {};
    ozz::vector<ozz::math::SoaTransform> _finalLocals {};
    ozz::vector<ozz::math::SimdFloat4> _jointWeights0 {};
    ozz::vector<ozz::math::SimdFloat4> _jointWeights1 {};
    ozz::vector<ozz::math::Float4x4> _modelMatrices {};
    vector<mat44> _worldMatrices {};
};

ModelAnimationRuntimePose::Impl::Impl(ptr<const ModelAnimationRuntimeRig> rig) :
    _rig {rig}
{
    FO_STACK_TRACE_ENTRY();

    InitializeModelAnimationMemory();
    const ozz::animation::Skeleton& skeleton = ModelAnimationRuntimeAccess::GetSkeleton(*rig);
    const size_t joint_count = numeric_cast<size_t>(skeleton.num_joints());
    const size_t soa_joint_count = numeric_cast<size_t>(skeleton.num_soa_joints());
    _bodyContext0.Resize(skeleton.num_joints());
    _bodyContext1.Resize(skeleton.num_joints());
    _movementContext0.Resize(skeleton.num_joints());
    _movementContext1.Resize(skeleton.num_joints());
    _bodyTrackLocals0.resize(soa_joint_count);
    _bodyTrackLocals1.resize(soa_joint_count);
    _movementTrackLocals0.resize(soa_joint_count);
    _movementTrackLocals1.resize(soa_joint_count);
    _bodyLocals.resize(soa_joint_count);
    _movementLocals.resize(soa_joint_count);
    _finalLocals.resize(soa_joint_count);
    _jointWeights0.resize(soa_joint_count);
    _jointWeights1.resize(soa_joint_count);
    _modelMatrices.resize(joint_count);
    _worldMatrices.resize(joint_count);
}

auto BuildModelPoseJointNameIndex(const_span<hstring> runtime_names, string_view context) -> unordered_map<hstring, uint32_t>
{
    FO_STACK_TRACE_ENTRY();

    unordered_map<hstring, uint32_t> joint_indexes;
    joint_indexes.reserve(runtime_names.size());

    for (size_t joint_index = 0; joint_index < runtime_names.size(); joint_index++) {
        const hstring runtime_name = runtime_names[joint_index];

        if (runtime_name && !joint_indexes.emplace(runtime_name, numeric_cast<uint32_t>(joint_index)).second) {
            throw ModelAnimationRuntimeException(strex("Model '{}' has duplicate runtime lookup name '{}'", context, runtime_name));
        }
    }

    return joint_indexes;
}

auto ResolveModelPoseJointLinks(const unordered_map<hstring, uint32_t>& parent_joint_indexes, const_span<hstring> child_runtime_names) -> vector<ModelPoseJointLink>
{
    FO_STACK_TRACE_ENTRY();

    vector<ModelPoseJointLink> links;
    links.reserve(child_runtime_names.size());

    for (size_t child_joint_index = 0; child_joint_index < child_runtime_names.size(); child_joint_index++) {
        const auto parent_joint_it = parent_joint_indexes.find(child_runtime_names[child_joint_index]);

        if (parent_joint_it != parent_joint_indexes.end()) {
            links.emplace_back(ModelPoseJointLink {parent_joint_it->second, numeric_cast<uint32_t>(child_joint_index)});
        }
    }

    return links;
}

void BuildModelRestWorldMatrices(const_span<ModelPoseJoint> joints, const mat44& root_matrix, span<mat44> world_matrices)
{
    FO_STACK_TRACE_ENTRY();

    if (joints.empty()) {
        throw ModelAnimationRuntimeException("Rest-pose hierarchy has no joints");
    }
    if (world_matrices.size() != joints.size()) {
        throw ModelAnimationRuntimeException(strex("Rest-pose output has {} matrices; expected {}", world_matrices.size(), joints.size()));
    }

    ValidateModelAnimationRuntimeMatrix(root_matrix, "rest-pose root matrix");

    for (size_t joint_index = 0; joint_index < joints.size(); joint_index++) {
        const ModelPoseJoint& joint = joints[joint_index];
        ValidateModelAnimationRuntimeMatrix(joint.RestLocalTransform, strex("rest-pose local matrix at joint {}", joint_index));

        if ((joint_index == 0 && joint.ParentIndex != -1) || (joint_index != 0 && (joint.ParentIndex < 0 || numeric_cast<size_t>(joint.ParentIndex) >= joint_index))) {
            throw ModelAnimationRuntimeException(strex("Rest-pose joint {} has invalid parent {}; joints must be in parent-before-child order", joint_index, joint.ParentIndex));
        }
    }

    for (size_t joint_index = 0; joint_index < joints.size(); joint_index++) {
        const ModelPoseJoint& joint = joints[joint_index];
        const mat44& parent_matrix = joint.ParentIndex >= 0 ? world_matrices[numeric_cast<size_t>(joint.ParentIndex)] : root_matrix;
        const mat44 world_matrix = parent_matrix * joint.RestLocalTransform;
        ValidateModelAnimationRuntimeMatrix(world_matrix, strex("rest-pose world matrix at joint {}", joint_index));
        world_matrices[joint_index] = world_matrix;
    }
}

ModelAnimationRuntimeClip::ModelAnimationRuntimeClip(unique_ptr<Impl> impl) :
    _impl {std::move(impl)}
{
    FO_STACK_TRACE_ENTRY();
}

ModelAnimationRuntimeClip::ModelAnimationRuntimeClip(ModelAnimationRuntimeClip&&) noexcept = default;
ModelAnimationRuntimeClip::~ModelAnimationRuntimeClip() = default;

auto ModelAnimationRuntimeClip::GetSourceFile() const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->SourceFile;
}

auto ModelAnimationRuntimeClip::GetClipName() const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->ClipName;
}

auto ModelAnimationRuntimeClip::GetSourceSignature() const noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->SourceSignature;
}

auto ModelAnimationRuntimeClip::GetDuration() const noexcept -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->Duration;
}

auto ModelAnimationRuntimeClip::GetJointPresence() const noexcept -> const_span<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->JointRemap.CanonicalJointPresent;
}

ModelAnimationRuntimeRig::ModelAnimationRuntimeRig(unique_ptr<Impl> impl) :
    _impl {std::move(impl)}
{
    FO_STACK_TRACE_ENTRY();
}

ModelAnimationRuntimeRig::ModelAnimationRuntimeRig(ModelAnimationRuntimeRig&&) noexcept = default;
ModelAnimationRuntimeRig::~ModelAnimationRuntimeRig() = default;

auto ModelAnimationRuntimeRig::GetRigSignature() const noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->RigSignature;
}

auto ModelAnimationRuntimeRig::GetCacheSignature() const noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->CacheSignature;
}

auto ModelAnimationRuntimeRig::GetJointCount() const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->JointCount;
}

auto ModelAnimationRuntimeRig::GetJointName(size_t index) const -> string_view
{
    FO_STACK_TRACE_ENTRY();

    const auto joint_names = _impl->Skeleton.joint_names();
    FO_VERIFY_AND_THROW(index < joint_names.size(), "Model animation runtime joint index is outside the rig", index, joint_names.size());
    return joint_names[index];
}

auto ModelAnimationRuntimeRig::GetBaseJointMapping() const noexcept -> const_span<uint32_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->BaseJointRemap.SourceToCanonicalJointIndices;
}

auto ModelAnimationRuntimeRig::GetBaseJointPresence() const noexcept -> const_span<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->BaseJointRemap.CanonicalJointPresent;
}

auto ModelAnimationRuntimeRig::GetClipCount() const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->Clips.size();
}

auto ModelAnimationRuntimeRig::GetClip(size_t index) const -> const ModelAnimationRuntimeClip&
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(index < _impl->Clips.size(), "Animation runtime clip index is outside the rig", index, _impl->Clips.size());
    return _impl->Clips[index];
}

auto ModelAnimationRuntimeRig::FindClip(string_view source_file, string_view clip_name) const noexcept -> nptr<const ModelAnimationRuntimeClip>
{
    FO_NO_STACK_TRACE_ENTRY();

    for (const ModelAnimationRuntimeClip& clip : _impl->Clips) {
        if (clip.GetSourceFile() == source_file && clip.GetClipName() == clip_name) {
            return &clip;
        }
    }

    return nullptr;
}

auto ModelAnimationRuntimeRig::GetBindings() const noexcept -> const_span<ModelAnimationRuntimeBinding>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->Bindings;
}

auto ModelAnimationRuntimeRig::FindBinding(int32_t state_anim, int32_t action_anim) const noexcept -> nptr<const ModelAnimationRuntimeBinding>
{
    FO_NO_STACK_TRACE_ENTRY();

    for (const ModelAnimationRuntimeBinding& binding : _impl->Bindings) {
        if (binding.StateAnim == state_anim && binding.ActionAnim == action_anim) {
            return &binding;
        }
    }

    return nullptr;
}

ModelAnimationRuntimePose::ModelAnimationRuntimePose(ptr<const ModelAnimationRuntimeRig> rig) :
    _impl {SafeAlloc::MakeUnique<Impl>(rig)}
{
    FO_STACK_TRACE_ENTRY();

    ResetLocalsToRestPose();
    BuildModelMatrices(mat44 {1.0f});
}

ModelAnimationRuntimePose::~ModelAnimationRuntimePose() = default;

auto ModelAnimationRuntimePose::GetJointCount() const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->_worldMatrices.size();
}

auto ModelAnimationRuntimePose::GetBodyLocalTransform(size_t joint_index) const -> ModelAnimationRuntimeTransform
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(joint_index < GetJointCount(), "Body-local animation joint index is outside the pose", joint_index, GetJointCount());
    return ExtractModelAnimationRuntimeTransform(ozz::make_span(_impl->_bodyLocals), joint_index);
}

auto ModelAnimationRuntimePose::GetFinalLocalTransform(size_t joint_index) const -> ModelAnimationRuntimeTransform
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(joint_index < GetJointCount(), "Final-local animation joint index is outside the pose", joint_index, GetJointCount());
    return ExtractModelAnimationRuntimeTransform(ozz::make_span(_impl->_finalLocals), joint_index);
}

auto ModelAnimationRuntimePose::GetWorldMatrices() const noexcept -> const_span<mat44>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->_worldMatrices;
}

void ModelAnimationRuntimePose::ResetLocalsToRestPose()
{
    FO_STACK_TRACE_ENTRY();

    const ozz::span<const ozz::math::SoaTransform> rest_poses = ModelAnimationRuntimeAccess::GetSkeleton(*_impl->_rig).joint_rest_poses();
    const auto reset_locals = [rest_poses](ozz::vector<ozz::math::SoaTransform>& locals) { std::copy(rest_poses.begin(), rest_poses.end(), locals.begin()); };
    reset_locals(_impl->_bodyTrackLocals0);
    reset_locals(_impl->_bodyTrackLocals1);
    reset_locals(_impl->_movementTrackLocals0);
    reset_locals(_impl->_movementTrackLocals1);
    reset_locals(_impl->_bodyLocals);
    reset_locals(_impl->_movementLocals);
    reset_locals(_impl->_finalLocals);
    _impl->_bodyContext0.Invalidate();
    _impl->_bodyContext1.Invalidate();
    _impl->_movementContext0.Invalidate();
    _impl->_movementContext1.Invalidate();
}

void ModelAnimationRuntimePose::BuildModelMatrices(const mat44& root_matrix)
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationRuntimeMatrix(root_matrix, "root matrix");

    const ozz::math::Float4x4 ozz_root_matrix = ConvertModelAnimationRuntimeMatrix(root_matrix);
    ozz::animation::LocalToModelJob local_to_model_job;
    local_to_model_job.skeleton = &ModelAnimationRuntimeAccess::GetSkeleton(*_impl->_rig);
    local_to_model_job.root = &ozz_root_matrix;
    local_to_model_job.input = ozz::make_span(_impl->_finalLocals);
    local_to_model_job.output = ozz::make_span(_impl->_modelMatrices);
    FO_STRONG_ASSERT(local_to_model_job.Run(), "Invalid animation runtime pose local-to-model job");

    for (size_t joint = 0; joint < _impl->_modelMatrices.size(); joint++) {
        _impl->_worldMatrices[joint] = ConvertModelAnimationRuntimeMatrix(_impl->_modelMatrices[joint]);
    }
}

void ModelAnimationRuntimePose::Evaluate(const array<TrackInput, 2>& body_tracks, const array<TrackInput, 2>& movement_tracks, const mat44& root_matrix, const_span<ProceduralLocalRotation> procedural_rotations)
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationRuntimeMatrix(root_matrix, "root matrix");
    const size_t joint_count = GetJointCount();
    ValidateModelAnimationRuntimeProceduralRotations(procedural_rotations, joint_count);
    const array<ModelAnimationRuntimeResolvedTrackInput, 2> resolved_body {
        ResolveModelAnimationRuntimeTrackInput(*_impl->_rig, body_tracks[0], joint_count, "body track 0"),
        ResolveModelAnimationRuntimeTrackInput(*_impl->_rig, body_tracks[1], joint_count, "body track 1"),
    };
    const array<ModelAnimationRuntimeResolvedTrackInput, 2> resolved_movement {
        ResolveModelAnimationRuntimeTrackInput(*_impl->_rig, movement_tracks[0], joint_count, "movement track 0"),
        ResolveModelAnimationRuntimeTrackInput(*_impl->_rig, movement_tracks[1], joint_count, "movement track 1"),
    };

    SampleModelAnimationRuntimeTrack(resolved_body[0], _impl->_bodyContext0, ozz::make_span(_impl->_bodyTrackLocals0));
    SampleModelAnimationRuntimeTrack(resolved_body[1], _impl->_bodyContext1, ozz::make_span(_impl->_bodyTrackLocals1));
    BlendModelAnimationRuntimeTracks(resolved_body, joint_count, ModelAnimationRuntimeAccess::GetSkeleton(*_impl->_rig).joint_rest_poses(), ozz::make_span(_impl->_bodyTrackLocals0), ozz::make_span(_impl->_bodyTrackLocals1), ozz::make_span(_impl->_jointWeights0), ozz::make_span(_impl->_jointWeights1), ozz::make_span(_impl->_bodyLocals));

    SampleModelAnimationRuntimeTrack(resolved_movement[0], _impl->_movementContext0, ozz::make_span(_impl->_movementTrackLocals0));
    SampleModelAnimationRuntimeTrack(resolved_movement[1], _impl->_movementContext1, ozz::make_span(_impl->_movementTrackLocals1));
    BlendModelAnimationRuntimeTracks(resolved_movement, joint_count, ModelAnimationRuntimeAccess::GetSkeleton(*_impl->_rig).joint_rest_poses(), ozz::make_span(_impl->_movementTrackLocals0), ozz::make_span(_impl->_movementTrackLocals1), ozz::make_span(_impl->_jointWeights0), ozz::make_span(_impl->_jointWeights1), ozz::make_span(_impl->_movementLocals));
    SelectModelAnimationRuntimeMovementPose(resolved_movement, joint_count, ozz::make_span(_impl->_bodyLocals), ozz::make_span(_impl->_movementLocals), ozz::make_span(_impl->_jointWeights0), ozz::make_span(_impl->_jointWeights1), ozz::make_span(_impl->_finalLocals));
    ApplyModelAnimationRuntimeProceduralRotations(procedural_rotations, ozz::make_span(_impl->_finalLocals));
    BuildModelMatrices(root_matrix);
}

void ModelAnimationRuntimePose::OverrideWorldMatrix(size_t joint_index, const mat44& world_matrix)
{
    FO_STACK_TRACE_ENTRY();

    if (joint_index >= GetJointCount()) {
        throw ModelAnimationRuntimeException(strex("Animation runtime pose world override joint {} is outside [0, {})", joint_index, GetJointCount()));
    }

    ValidateModelAnimationRuntimeMatrix(world_matrix, "world override matrix");
    const ozz::math::Float4x4 ozz_world_matrix = ConvertModelAnimationRuntimeMatrix(world_matrix);
    _impl->_modelMatrices[joint_index] = ozz_world_matrix;
    _impl->_worldMatrices[joint_index] = world_matrix;
}

auto LoadModelAnimationRuntimeRig(const_span<uint8_t> data, string_view model_description, string_view base_model, bool nearest_sampling) -> unique_ptr<ModelAnimationRuntimeRig>
{
    FO_STACK_TRACE_ENTRY();

    InitializeModelAnimationMemory();
    ModelAnimationRigData rig_data;

    try {
        rig_data = ReadModelAnimationRigData(data, model_description);
    }
    catch (const ModelAnimationRigDataException& ex) {
        throw ModelAnimationRuntimeException(strex("Can't load animation rig data for '{}': {}", model_description, ex.what()));
    }

    if (rig_data.Skeleton.Metadata.SourceAsset != model_description) {
        throw ModelAnimationRuntimeException(strex("Animation rig skeleton source '{}' does not match model description '{}'", rig_data.Skeleton.Metadata.SourceAsset, model_description));
    }
    if (rig_data.BaseJointRemap.Metadata.SourceAsset != base_model) {
        throw ModelAnimationRuntimeException(strex("Animation rig base-remap source '{}' does not match model '{}' for '{}'", rig_data.BaseJointRemap.Metadata.SourceAsset, base_model, model_description));
    }

    ozz::animation::Skeleton skeleton = DeserializeModelAnimationRuntimeObject<ozz::animation::Skeleton>(rig_data.Skeleton.Payload, strex("canonical skeleton for '{}'", model_description));
    const uint32_t canonical_joint_count = numeric_cast<uint32_t>(skeleton.num_joints());
    ModelAnimationJointRemap base_joint_remap;

    try {
        base_joint_remap = ReadModelAnimationJointRemapPayload(rig_data.BaseJointRemap.Payload, strex("base joint remap for '{}'", model_description));
    }
    catch (const ModelAnimationRigDataException& ex) {
        throw ModelAnimationRuntimeException(strex("Can't load base joint remap for '{}': {}", model_description, ex.what()));
    }

    if (base_joint_remap.CanonicalJointCount != canonical_joint_count || base_joint_remap.Duration != 0.0f || !base_joint_remap.NearestSampleTimes.empty()) {
        throw ModelAnimationRuntimeException(strex("Invalid base joint remap for '{}': {} canonical joints, duration {}, {} nearest times; skeleton has {} joints", model_description, base_joint_remap.CanonicalJointCount, base_joint_remap.Duration, base_joint_remap.NearestSampleTimes.size(), canonical_joint_count));
    }

    vector<ModelAnimationRuntimeClip> clips;
    clips.reserve(rig_data.Clips.size());

    for (ModelAnimationRigClipData& clip_data : rig_data.Clips) {
        const string& source_file = clip_data.Animation.Metadata.SourceAsset;
        const string& clip_name = clip_data.Animation.Metadata.ObjectName;
        const string clip_context = strex("animation '{}#{}' for '{}'", source_file, clip_name, model_description);
        ozz::animation::Animation animation = DeserializeModelAnimationRuntimeObject<ozz::animation::Animation>(clip_data.Animation.Payload, clip_context);
        ModelAnimationJointRemap joint_remap;

        try {
            joint_remap = ReadModelAnimationJointRemapPayload(clip_data.JointRemap.Payload, clip_context);
        }
        catch (const ModelAnimationRigDataException& ex) {
            throw ModelAnimationRuntimeException(strex("Can't load joint remap for {}: {}", clip_context, ex.what()));
        }

        if (animation.num_tracks() != skeleton.num_joints()) {
            throw ModelAnimationRuntimeException(strex("Ozz {} has {} tracks; canonical skeleton has {} joints", clip_context, animation.num_tracks(), skeleton.num_joints()));
        }
        if (!std::isfinite(animation.duration()) || animation.duration() <= 0.0f || !std::isfinite(1.0f / animation.duration())) {
            throw ModelAnimationRuntimeException(strex("Ozz {} has invalid duration {}", clip_context, animation.duration()));
        }
        if (animation.name() == nullptr || string_view {animation.name()} != clip_name) {
            throw ModelAnimationRuntimeException(strex("Ozz {} contains object name '{}'", clip_context, animation.name() != nullptr ? animation.name() : "<null>"));
        }
        if (joint_remap.CanonicalJointCount != canonical_joint_count || joint_remap.Duration != animation.duration()) {
            throw ModelAnimationRuntimeException(strex("Ozz {} remap has {} canonical joints and duration {}; expected {} and {}", clip_context, joint_remap.CanonicalJointCount, joint_remap.Duration, canonical_joint_count, animation.duration()));
        }
        const bool has_nearest_timeline = !joint_remap.NearestSampleTimes.empty();

        if (has_nearest_timeline != nearest_sampling) {
            throw ModelAnimationRuntimeException(strex("Ozz {} nearest-sampling timeline does not match the model interpolation policy", clip_context));
        }

        clips.emplace_back(ModelAnimationRuntimeAccess::CreateClip(source_file, clip_name, clip_data.Animation.Metadata.SourceSignature, std::move(animation), std::move(joint_remap)));
    }

    return ModelAnimationRuntimeAccess::CreateRig(rig_data.RigSignature, rig_data.CacheSignature, std::move(skeleton), std::move(base_joint_remap), std::move(clips), std::move(rig_data.Bindings));
}

void ValidateModelAnimationRuntimeBaseJoints(const ModelAnimationRuntimeRig& rig, const_span<ModelAnimationRuntimeJoint> source_joints, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationRuntimeJointList(source_joints, context);
    const ModelAnimationJointRemap& remap = ModelAnimationRuntimeAccess::GetBaseJointRemap(rig);

    if (source_joints.size() != remap.SourceToCanonicalJointIndices.size()) {
        throw ModelAnimationRuntimeException(strex("Animation base-joint remap count {} does not match source hierarchy count {} in {}", remap.SourceToCanonicalJointIndices.size(), source_joints.size(), context));
    }

    const ozz::animation::Skeleton& skeleton = ModelAnimationRuntimeAccess::GetSkeleton(rig);
    const ozz::span<const char* const> canonical_names = skeleton.joint_names();
    const ozz::span<const int16_t> canonical_parents = skeleton.joint_parents();

    for (size_t source_index = 0; source_index < source_joints.size(); source_index++) {
        const ModelAnimationRuntimeJoint& source_joint = source_joints[source_index];
        const uint32_t canonical_index = remap.SourceToCanonicalJointIndices[source_index];

        if (source_joint.Name != canonical_names[canonical_index]) {
            throw ModelAnimationRuntimeException(strex("Animation base-joint name mismatch at source joint {} in {}: source '{}', canonical '{}' at {}", source_index, context, source_joint.Name, canonical_names[canonical_index], canonical_index));
        }

        const int16_t canonical_parent = canonical_parents[canonical_index];

        if (source_joint.ParentIndex < 0) {
            if (canonical_parent != ozz::animation::Skeleton::kNoParent) {
                throw ModelAnimationRuntimeException(strex("Animation base root '{}' in {} maps to canonical joint {} with parent {}", source_joint.Name, context, canonical_index, canonical_parent));
            }
        }
        else {
            const uint32_t mapped_parent = remap.SourceToCanonicalJointIndices[numeric_cast<size_t>(source_joint.ParentIndex)];

            if (canonical_parent < 0 || mapped_parent != numeric_cast<uint32_t>(canonical_parent)) {
                throw ModelAnimationRuntimeException(strex("Animation base-joint parent mismatch for '{}' at source joint {} in {}: source parent {} maps to {}, canonical parent is {}", source_joint.Name, source_index, context, source_joint.ParentIndex, mapped_parent, canonical_parent));
            }
        }

        ValidateModelAnimationRuntimeRestPose(source_joint, skeleton, canonical_index, context);
    }
}

auto ResolveModelAnimationRuntimeCanonicalJoints(const ModelAnimationRuntimeRig& rig, const_span<ModelAnimationRuntimeJoint> hierarchy_joints, string_view context) -> vector<uint32_t>
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationRuntimeJointList(hierarchy_joints, context);
    map<pair<int32_t, string_view>, uint32_t> hierarchy_index_by_parent_and_name;

    for (size_t hierarchy_index = 0; hierarchy_index < hierarchy_joints.size(); hierarchy_index++) {
        const ModelAnimationRuntimeJoint& hierarchy_joint = hierarchy_joints[hierarchy_index];
        const pair<int32_t, string_view> joint_key {hierarchy_joint.ParentIndex, hierarchy_joint.Name};

        if (!hierarchy_index_by_parent_and_name.emplace(joint_key, numeric_cast<uint32_t>(hierarchy_index)).second) {
            throw ModelAnimationRuntimeException(strex("Runtime model hierarchy in {} contains duplicate direct child '{}' under joint {}", context, hierarchy_joint.Name, hierarchy_joint.ParentIndex));
        }
    }

    const ozz::animation::Skeleton& skeleton = ModelAnimationRuntimeAccess::GetSkeleton(rig);
    const ozz::span<const char* const> canonical_names = skeleton.joint_names();
    const ozz::span<const int16_t> canonical_parents = skeleton.joint_parents();
    vector<uint32_t> canonical_to_hierarchy;
    canonical_to_hierarchy.reserve(numeric_cast<size_t>(skeleton.num_joints()));

    for (int canonical_index = 0; canonical_index < skeleton.num_joints(); canonical_index++) {
        const string_view canonical_name {canonical_names[canonical_index]};
        const int16_t canonical_parent = canonical_parents[canonical_index];
        int32_t hierarchy_parent = -1;

        if (canonical_parent != ozz::animation::Skeleton::kNoParent) {
            FO_STRONG_ASSERT(canonical_parent >= 0 && numeric_cast<size_t>(canonical_parent) < canonical_to_hierarchy.size(), "Canonical animation skeleton parent order is invalid", context, canonical_index, canonical_parent);
            hierarchy_parent = numeric_cast<int32_t>(canonical_to_hierarchy[numeric_cast<size_t>(canonical_parent)]);
        }

        const auto hierarchy_it = hierarchy_index_by_parent_and_name.find(pair<int32_t, string_view> {hierarchy_parent, canonical_name});

        if (hierarchy_it == hierarchy_index_by_parent_and_name.end()) {
            throw ModelAnimationRuntimeException(strex("Canonical animation joint '{}' at {} under resolved parent {} is absent from runtime model hierarchy in {}", canonical_name, canonical_index, hierarchy_parent, context));
        }

        const uint32_t hierarchy_index = hierarchy_it->second;
        const ModelAnimationRuntimeJoint& hierarchy_joint = hierarchy_joints[hierarchy_index];
        FO_STRONG_ASSERT(hierarchy_joint.ParentIndex == hierarchy_parent, "Resolved animation canonical joint has an unexpected runtime parent", context, canonical_index, hierarchy_index, hierarchy_joint.ParentIndex, hierarchy_parent);

        ValidateModelAnimationRuntimeRestPose(hierarchy_joint, skeleton, numeric_cast<uint32_t>(canonical_index), context);
        canonical_to_hierarchy.emplace_back(hierarchy_index);
    }

    return canonical_to_hierarchy;
}

template<typename T>
static auto DeserializeModelAnimationRuntimeObject(const_span<uint8_t> payload, string_view context) -> T
{
    FO_STACK_TRACE_ENTRY();

    if (payload.empty() || payload.size() > numeric_cast<size_t>(std::numeric_limits<int>::max())) {
        throw ModelAnimationRuntimeException(strex("Invalid serialized ozz payload size {} for {}", payload.size(), context));
    }

    ozz::io::MemoryStream stream;

    if (stream.Write(payload.data(), payload.size()) != payload.size() || stream.Seek(0, ozz::io::Stream::kSet) != 0) {
        throw ModelAnimationRuntimeException(strex("Can't stage serialized ozz payload for {}", context));
    }

    T result;

    {
        ozz::io::IArchive archive {&stream};

        if (!archive.TestTag<T>()) {
            throw ModelAnimationRuntimeException(strex("Serialized ozz payload has the wrong type tag for {}", context));
        }

        archive >> result;
    }

    if (stream.Tell() < 0 || numeric_cast<size_t>(stream.Tell()) != payload.size()) {
        throw ModelAnimationRuntimeException(strex("Serialized ozz payload for {} was not consumed exactly: {} of {} bytes", context, stream.Tell(), payload.size()));
    }

    return result;
}

static void ValidateModelAnimationRuntimeSkeleton(const ozz::animation::Skeleton& skeleton, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (skeleton.num_joints() <= 0 || skeleton.num_joints() > numeric_cast<int>(MODEL_ANIMATION_MAX_JOINTS)) {
        throw ModelAnimationRuntimeException(strex("Ozz skeleton for '{}' has invalid joint count {}", context, skeleton.num_joints()));
    }

    const ozz::span<const int16_t> parents = skeleton.joint_parents();
    const ozz::span<const char* const> names = skeleton.joint_names();

    if (parents.size() != numeric_cast<size_t>(skeleton.num_joints()) || names.size() != numeric_cast<size_t>(skeleton.num_joints())) {
        throw ModelAnimationRuntimeException(strex("Ozz skeleton for '{}' has inconsistent parent/name arrays", context));
    }

    for (int joint = 0; joint < skeleton.num_joints(); joint++) {
        const int16_t parent = parents[numeric_cast<size_t>(joint)];

        if ((joint == 0 && parent != ozz::animation::Skeleton::kNoParent) || (joint != 0 && (parent < 0 || parent >= joint))) {
            throw ModelAnimationRuntimeException(strex("Ozz skeleton for '{}' has invalid parent {} for joint {}", context, parent, joint));
        }
        if (names[numeric_cast<size_t>(joint)] == nullptr) {
            throw ModelAnimationRuntimeException(strex("Ozz skeleton for '{}' has a null name for joint {}", context, joint));
        }

        const string_view name {names[numeric_cast<size_t>(joint)]};

        if ((joint != 0 && name.empty()) || !strvex(name).is_valid_utf8()) {
            throw ModelAnimationRuntimeException(strex("Ozz skeleton for '{}' has an invalid name for joint {}", context, joint));
        }

        ValidateModelAnimationRuntimeTransform(ozz::animation::GetJointLocalRestPose(skeleton, joint), numeric_cast<size_t>(joint), context);
    }
}

static void ValidateModelAnimationRuntimeTransform(const ozz::math::Transform& transform, size_t joint_index, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    const bool finite = std::isfinite(transform.translation.x) && std::isfinite(transform.translation.y) && std::isfinite(transform.translation.z) && std::isfinite(transform.rotation.x) && std::isfinite(transform.rotation.y) && std::isfinite(transform.rotation.z) && std::isfinite(transform.rotation.w) && std::isfinite(transform.scale.x) && std::isfinite(transform.scale.y) && std::isfinite(transform.scale.z);

    const float32_t rotation_norm_squared = transform.rotation.x * transform.rotation.x + transform.rotation.y * transform.rotation.y + transform.rotation.z * transform.rotation.z + transform.rotation.w * transform.rotation.w;
    const bool valid_scale = float_abs(transform.scale.x) > std::numeric_limits<float32_t>::epsilon() && float_abs(transform.scale.y) > std::numeric_limits<float32_t>::epsilon() && float_abs(transform.scale.z) > std::numeric_limits<float32_t>::epsilon();

    if (!finite || !valid_scale || !std::isfinite(rotation_norm_squared) || float_abs(rotation_norm_squared - 1.0f) > 1.0e-4f) {
        throw ModelAnimationRuntimeException(strex("Ozz skeleton for '{}' has an invalid rest transform at joint {}", context, joint_index));
    }
}

static void ValidateModelAnimationRuntimeJointList(const_span<ModelAnimationRuntimeJoint> joints, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (joints.empty() || joints.size() > numeric_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        throw ModelAnimationRuntimeException(strex("Runtime model hierarchy in {} has invalid joint count {}", context, joints.size()));
    }

    for (size_t i = 0; i < joints.size(); i++) {
        const ModelAnimationRuntimeJoint& joint = joints[i];

        if (!strvex(joint.Name).is_valid_utf8() || joint.Name.find('\0') != string_view::npos || (i != 0 && joint.Name.empty())) {
            throw ModelAnimationRuntimeException(strex("Runtime model hierarchy in {} has invalid joint name at {}", context, i));
        }
        if ((i == 0 && joint.ParentIndex != -1) || (i != 0 && (joint.ParentIndex < 0 || numeric_cast<size_t>(joint.ParentIndex) >= i))) {
            throw ModelAnimationRuntimeException(strex("Runtime model hierarchy in {} has invalid parent {} at joint {} ('{}')", context, joint.ParentIndex, i, joint.Name));
        }
    }
}

static void ValidateModelAnimationRuntimeRestPose(const ModelAnimationRuntimeJoint& joint, const ozz::animation::Skeleton& skeleton, uint32_t canonical_index, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    const mat44 canonical_rest = ComposeModelAnimationRuntimeTransform(ozz::animation::GetJointLocalRestPose(skeleton, numeric_cast<int>(canonical_index)));

    for (mat44::length_type column = 0; column < 4; column++) {
        for (mat44::length_type row = 0; row < 4; row++) {
            const float32_t source_value = joint.RestLocalTransform[column][row];
            const float32_t canonical_value = canonical_rest[column][row];

            if (!std::isfinite(source_value) || !std::isfinite(canonical_value)) {
                throw ModelAnimationRuntimeException(strex("Runtime/Ozz rest pose for joint '{}' at canonical index {} in {} contains a non-finite matrix component", joint.Name, canonical_index, context));
            }

            const float32_t difference = float_abs(source_value - canonical_value);
            const float32_t tolerance = MODEL_ANIMATION_RIG_MATRIX_ABSOLUTE_TOLERANCE + MODEL_ANIMATION_RIG_MATRIX_RELATIVE_TOLERANCE * std::max(float_abs(source_value), float_abs(canonical_value));

            if (difference > tolerance) {
                throw ModelAnimationRuntimeException(strex("Runtime/Ozz rest-pose mismatch for joint '{}' at canonical index {} in {} at [{}][{}]: runtime {}, canonical {}, delta {}, tolerance {}", joint.Name, canonical_index, context, column, row, source_value, canonical_value, difference, tolerance));
            }
        }
    }
}

static void ValidateModelAnimationRuntimePoseRig(const ModelAnimationRuntimeRig& rig)
{
    FO_STACK_TRACE_ENTRY();

    const size_t joint_count = rig.GetJointCount();

    if (rig.GetClipCount() > numeric_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        throw ModelAnimationRuntimeException(strex("Animation runtime pose rig has too many clips: {}", rig.GetClipCount()));
    }

    for (size_t clip_index = 0; clip_index < rig.GetClipCount(); clip_index++) {
        const ModelAnimationRuntimeClip& clip = rig.GetClip(clip_index);
        const ozz::animation::Animation& animation = ModelAnimationRuntimeAccess::GetAnimation(clip);
        const ModelAnimationJointRemap& remap = ModelAnimationRuntimeAccess::GetJointRemap(clip);
        const string context = strex("runtime pose clip {} ('{}#{}')", clip_index, clip.GetSourceFile(), clip.GetClipName());

        if (animation.num_tracks() != numeric_cast<int>(joint_count) || !std::isfinite(animation.duration()) || animation.duration() <= 0.0f || !std::isfinite(1.0f / animation.duration())) {
            throw ModelAnimationRuntimeException(strex("Invalid animation {} tracks/duration: {}/{}", context, animation.num_tracks(), animation.duration()));
        }

        try {
            ValidateModelAnimationJointRemap(remap, context);
        }
        catch (const ModelAnimationRigDataException& ex) {
            throw ModelAnimationRuntimeException(strex("Invalid animation {} remap: {}", context, ex.what()));
        }

        if (remap.CanonicalJointCount != joint_count || remap.Duration != animation.duration()) {
            throw ModelAnimationRuntimeException(strex("Invalid animation {} remap skeleton/duration: {}/{} joints, {}/{} duration", context, remap.CanonicalJointCount, joint_count, remap.Duration, animation.duration()));
        }
    }
}

static void ValidateModelAnimationRuntimeMatrix(const mat44& matrix, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    for (mat44::length_type column = 0; column < 4; column++) {
        for (mat44::length_type row = 0; row < 4; row++) {
            if (!std::isfinite(matrix[column][row])) {
                throw ModelAnimationRuntimeException(strex("Animation runtime pose {} contains a non-finite component at [{}][{}]", context, column, row));
            }
        }
    }
}

static void ValidateModelAnimationRuntimeProceduralRotations(const_span<ModelAnimationRuntimePose::ProceduralLocalRotation> procedural_rotations, size_t joint_count)
{
    FO_STACK_TRACE_ENTRY();

    if (procedural_rotations.size() > ModelAnimationRuntimePose::MAX_PROCEDURAL_ROTATIONS) {
        throw ModelAnimationRuntimeException(strex("Animation runtime pose has {} procedural rotations; maximum is {}", procedural_rotations.size(), ModelAnimationRuntimePose::MAX_PROCEDURAL_ROTATIONS));
    }

    for (size_t i = 0; i < procedural_rotations.size(); i++) {
        const ModelAnimationRuntimePose::ProceduralLocalRotation& procedural_rotation = procedural_rotations[i];

        if (procedural_rotation.JointIndex >= joint_count) {
            throw ModelAnimationRuntimeException(strex("Animation runtime pose procedural rotation joint {} at input {} is outside [0, {})", procedural_rotation.JointIndex, i, joint_count));
        }

        const quaternion& rotation = procedural_rotation.Rotation;

        if (!std::isfinite(rotation.w) || !std::isfinite(rotation.x) || !std::isfinite(rotation.y) || !std::isfinite(rotation.z)) {
            throw ModelAnimationRuntimeException(strex("Animation runtime pose procedural rotation at input {} contains a non-finite quaternion", i));
        }

        const float32_t length_squared = glm::dot(rotation, rotation);

        if (float_abs(length_squared - 1.0f) > 1.0e-4f) {
            throw ModelAnimationRuntimeException(strex("Animation runtime pose procedural rotation at input {} is not normalized: squared length {}", i, length_squared));
        }

        for (size_t previous = 0; previous < i; previous++) {
            if (procedural_rotations[previous].JointIndex == procedural_rotation.JointIndex) {
                throw ModelAnimationRuntimeException(strex("Animation runtime pose has duplicate procedural rotation joint {} at inputs {} and {}", procedural_rotation.JointIndex, previous, i));
            }
        }
    }
}

static void ApplyModelAnimationRuntimeProceduralRotations(const_span<ModelAnimationRuntimePose::ProceduralLocalRotation> procedural_rotations, ozz::span<ozz::math::SoaTransform> locals) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    for (const ModelAnimationRuntimePose::ProceduralLocalRotation& procedural_rotation : procedural_rotations) {
        const size_t soa_joint = procedural_rotation.JointIndex / 4;
        const size_t lane = procedural_rotation.JointIndex % 4;
        ozz::math::SoaTransform& local = locals[soa_joint];
        array<float32_t, 4> translation_x {};
        array<float32_t, 4> translation_y {};
        array<float32_t, 4> translation_z {};
        array<float32_t, 4> rotation_x {};
        array<float32_t, 4> rotation_y {};
        array<float32_t, 4> rotation_z {};
        array<float32_t, 4> rotation_w {};
        ozz::math::StorePtrU(local.translation.x, translation_x.data());
        ozz::math::StorePtrU(local.translation.y, translation_y.data());
        ozz::math::StorePtrU(local.translation.z, translation_z.data());
        ozz::math::StorePtrU(local.rotation.x, rotation_x.data());
        ozz::math::StorePtrU(local.rotation.y, rotation_y.data());
        ozz::math::StorePtrU(local.rotation.z, rotation_z.data());
        ozz::math::StorePtrU(local.rotation.w, rotation_w.data());

        const vec3 sampled_translation {translation_x[lane], translation_y[lane], translation_z[lane]};
        const quaternion sampled_rotation {rotation_w[lane], rotation_x[lane], rotation_y[lane], rotation_z[lane]};
        const vec3 rotated_translation = procedural_rotation.Rotation * sampled_translation;
        const quaternion rotated_rotation = procedural_rotation.Rotation * sampled_rotation;
        translation_x[lane] = rotated_translation.x;
        translation_y[lane] = rotated_translation.y;
        translation_z[lane] = rotated_translation.z;
        rotation_x[lane] = rotated_rotation.x;
        rotation_y[lane] = rotated_rotation.y;
        rotation_z[lane] = rotated_rotation.z;
        rotation_w[lane] = rotated_rotation.w;
        local.translation.x = ozz::math::simd_float4::LoadPtrU(translation_x.data());
        local.translation.y = ozz::math::simd_float4::LoadPtrU(translation_y.data());
        local.translation.z = ozz::math::simd_float4::LoadPtrU(translation_z.data());
        local.rotation.x = ozz::math::simd_float4::LoadPtrU(rotation_x.data());
        local.rotation.y = ozz::math::simd_float4::LoadPtrU(rotation_y.data());
        local.rotation.z = ozz::math::simd_float4::LoadPtrU(rotation_z.data());
        local.rotation.w = ozz::math::simd_float4::LoadPtrU(rotation_w.data());
    }
}

static auto ResolveModelAnimationRuntimeTrackInput(const ModelAnimationRuntimeRig& rig, const ModelAnimationRuntimePose::TrackInput& input, size_t joint_count, string_view context) -> ModelAnimationRuntimeResolvedTrackInput
{
    FO_STACK_TRACE_ENTRY();

    if (!std::isfinite(input.Position) || input.Position < 0.0f) {
        throw ModelAnimationRuntimeException(strex("Animation runtime pose {} has invalid position {}", context, input.Position));
    }
    if (!std::isfinite(input.Weight) || input.Weight < 0.0f || input.Weight > 1.0f) {
        throw ModelAnimationRuntimeException(strex("Animation runtime pose {} has invalid weight {}", context, input.Weight));
    }
    if (input.ClipIndex < -1 || (input.ClipIndex >= 0 && numeric_cast<size_t>(input.ClipIndex) >= rig.GetClipCount())) {
        throw ModelAnimationRuntimeException(strex("Animation runtime pose {} clip index {} is outside [-1, {})", context, input.ClipIndex, rig.GetClipCount()));
    }
    if (!input.JointMask.empty() && input.JointMask.size() != joint_count) {
        throw ModelAnimationRuntimeException(strex("Animation runtime pose {} joint mask has {} entries; expected {} or empty", context, input.JointMask.size(), joint_count));
    }

    for (size_t joint = 0; joint < input.JointMask.size(); joint++) {
        if (input.JointMask[joint] > 1) {
            throw ModelAnimationRuntimeException(strex("Animation runtime pose {} joint mask has invalid value {} at {}", context, input.JointMask[joint], joint));
        }
    }

    ModelAnimationRuntimeResolvedTrackInput result;
    result.Weight = input.Weight;
    result.JointMask = input.JointMask;

    if (input.ClipIndex < 0) {
        return result;
    }

    const ModelAnimationRuntimeClip& clip = rig.GetClip(numeric_cast<size_t>(input.ClipIndex));
    result.Clip = &clip;
    result.Active = input.Enabled && input.Weight > 0.0f;
    const ModelAnimationJointRemap& joint_remap = ModelAnimationRuntimeAccess::GetJointRemap(clip);
    result.Nearest = !joint_remap.NearestSampleTimes.empty();

    if (result.Active) {
        const float32_t duration = clip.GetDuration();
        const float32_t loop_position = std::fmod(input.Position, duration);
        float32_t source_time = input.Reversed ? duration - loop_position : loop_position;

        if (result.Nearest) {
            source_time = SnapModelAnimationRuntimeSampleTime(source_time, joint_remap.NearestSampleTimes);
        }

        result.SampleRatio = source_time / duration;
        FO_STRONG_ASSERT(std::isfinite(result.SampleRatio) && result.SampleRatio >= 0.0f && result.SampleRatio <= 1.0f, "Resolved animation runtime sample ratio is invalid", context, input.Position, duration, source_time, result.SampleRatio);
    }

    return result;
}

static auto SnapModelAnimationRuntimeSampleTime(float32_t source_time, const_span<float32_t> sample_times) noexcept -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto next = std::lower_bound(sample_times.begin(), sample_times.end(), source_time);

    if (next == sample_times.begin()) {
        return sample_times.front();
    }
    if (next == sample_times.end()) {
        return sample_times.back();
    }

    const float32_t previous_time = *(next - 1);
    return source_time - previous_time < *next - source_time ? previous_time : *next;
}

static auto IsModelAnimationRuntimeJointPresent(const ModelAnimationRuntimeResolvedTrackInput& track, size_t joint) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return track.Active && ModelAnimationRuntimeAccess::GetJointRemap(*track.Clip).CanonicalJointPresent[joint] != 0 && (track.JointMask.empty() || track.JointMask[joint] != 0);
}

static void SampleModelAnimationRuntimeTrack(const ModelAnimationRuntimeResolvedTrackInput& track, ozz::animation::SamplingJob::Context& context, ozz::span<ozz::math::SoaTransform> locals)
{
    FO_STACK_TRACE_ENTRY();

    if (!track.Active) {
        return;
    }

    ozz::animation::SamplingJob sampling_job;
    sampling_job.animation = &ModelAnimationRuntimeAccess::GetAnimation(*track.Clip);
    sampling_job.context = &context;
    sampling_job.ratio = track.SampleRatio;
    sampling_job.output = locals;
    FO_STRONG_ASSERT(sampling_job.Run(), "Invalid animation runtime pose sampling job", track.Clip->GetSourceFile(), track.Clip->GetClipName(), track.SampleRatio);
}

static void BlendModelAnimationRuntimeTracks(const array<ModelAnimationRuntimeResolvedTrackInput, 2>& tracks, size_t joint_count, ozz::span<const ozz::math::SoaTransform> rest_pose, ozz::span<const ozz::math::SoaTransform> track_locals0, ozz::span<const ozz::math::SoaTransform> track_locals1, ozz::span<ozz::math::SimdFloat4> joint_weights0, ozz::span<ozz::math::SimdFloat4> joint_weights1, ozz::span<ozz::math::SoaTransform> output)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(joint_weights0.size() == rest_pose.size() && joint_weights1.size() == rest_pose.size(), "Animation runtime pose blend-weight buffers have invalid sizes");
    const bool nearest_transition = tracks[0].Nearest || tracks[1].Nearest;

    for (size_t soa_joint = 0; soa_joint < rest_pose.size(); soa_joint++) {
        array<float32_t, 4> weights0 {};
        array<float32_t, 4> weights1 {};

        for (size_t lane = 0; lane < 4; lane++) {
            const size_t joint = soa_joint * 4 + lane;

            if (joint >= joint_count) {
                continue;
            }

            const bool present0 = IsModelAnimationRuntimeJointPresent(tracks[0], joint);
            const bool present1 = IsModelAnimationRuntimeJointPresent(tracks[1], joint);

            if (present0 && present1) {
                if (nearest_transition) {
                    weights1[lane] = tracks[1].Weight >= 0.5f ? 1.0f : 0.0f;
                    weights0[lane] = 1.0f - weights1[lane];
                }
                else {
                    weights1[lane] = tracks[1].Weight;
                    weights0[lane] = 1.0f - weights1[lane];
                }
            }
            else if (present0) {
                weights0[lane] = 1.0f;
            }
            else if (present1) {
                weights1[lane] = 1.0f;
            }
        }

        joint_weights0[soa_joint] = ozz::math::simd_float4::LoadPtrU(weights0.data());
        joint_weights1[soa_joint] = ozz::math::simd_float4::LoadPtrU(weights1.data());
    }

    array<ozz::animation::BlendingJob::Layer, 2> layers {};
    layers[0].weight = 1.0f;
    layers[0].transform = track_locals0;
    layers[0].joint_weights = joint_weights0;
    layers[1].weight = 1.0f;
    layers[1].transform = track_locals1;
    layers[1].joint_weights = joint_weights1;
    ozz::animation::BlendingJob blending_job;
    blending_job.layers = ozz::make_span(layers);
    blending_job.rest_pose = rest_pose;
    blending_job.output = output;
    FO_STRONG_ASSERT(blending_job.Run(), "Invalid animation runtime pose blending job");

    const auto unpack_rotations = [](const ozz::math::SoaTransform& transform) {
        array<float32_t, 4> x {};
        array<float32_t, 4> y {};
        array<float32_t, 4> z {};
        array<float32_t, 4> w {};
        ozz::math::StorePtrU(transform.rotation.x, x.data());
        ozz::math::StorePtrU(transform.rotation.y, y.data());
        ozz::math::StorePtrU(transform.rotation.z, z.data());
        ozz::math::StorePtrU(transform.rotation.w, w.data());
        array<quaternion, 4> result {};

        for (size_t lane = 0; lane < 4; lane++) {
            result[lane] = quaternion {w[lane], x[lane], y[lane], z[lane]};
        }

        return result;
    };
    const auto pack_rotations = [](const array<quaternion, 4>& rotations, ozz::math::SoaTransform& transform) {
        array<float32_t, 4> x {};
        array<float32_t, 4> y {};
        array<float32_t, 4> z {};
        array<float32_t, 4> w {};

        for (size_t lane = 0; lane < 4; lane++) {
            x[lane] = rotations[lane].x;
            y[lane] = rotations[lane].y;
            z[lane] = rotations[lane].z;
            w[lane] = rotations[lane].w;
        }

        transform.rotation.x = ozz::math::simd_float4::LoadPtrU(x.data());
        transform.rotation.y = ozz::math::simd_float4::LoadPtrU(y.data());
        transform.rotation.z = ozz::math::simd_float4::LoadPtrU(z.data());
        transform.rotation.w = ozz::math::simd_float4::LoadPtrU(w.data());
    };

    for (size_t soa_joint = 0; soa_joint < output.size(); soa_joint++) {
        const array<quaternion, 4> rest_rotations = unpack_rotations(rest_pose[soa_joint]);
        const array<quaternion, 4> rotations0 = unpack_rotations(track_locals0[soa_joint]);
        const array<quaternion, 4> rotations1 = unpack_rotations(track_locals1[soa_joint]);
        array<quaternion, 4> blended_rotations = rest_rotations;

        for (size_t lane = 0; lane < 4; lane++) {
            const size_t joint = soa_joint * 4 + lane;

            if (joint >= joint_count) {
                continue;
            }

            const bool present0 = IsModelAnimationRuntimeJointPresent(tracks[0], joint);
            const bool present1 = IsModelAnimationRuntimeJointPresent(tracks[1], joint);

            if (present0 && present1) {
                blended_rotations[lane] = nearest_transition ? (tracks[1].Weight >= 0.5f ? rotations1[lane] : rotations0[lane]) : glm::normalize(glm::slerp(rotations0[lane], rotations1[lane], tracks[1].Weight));
            }
            else if (present0) {
                blended_rotations[lane] = rotations0[lane];
            }
            else if (present1) {
                blended_rotations[lane] = rotations1[lane];
            }
        }

        pack_rotations(blended_rotations, output[soa_joint]);
    }
}

static void SelectModelAnimationRuntimeMovementPose(const array<ModelAnimationRuntimeResolvedTrackInput, 2>& movement_tracks, size_t joint_count, ozz::span<const ozz::math::SoaTransform> body_locals, ozz::span<const ozz::math::SoaTransform> movement_locals, ozz::span<ozz::math::SimdFloat4> body_joint_weights, ozz::span<ozz::math::SimdFloat4> movement_joint_weights, ozz::span<ozz::math::SoaTransform> output)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(body_joint_weights.size() == body_locals.size() && movement_joint_weights.size() == body_locals.size(), "Animation runtime pose selection-weight buffers have invalid sizes");

    for (size_t soa_joint = 0; soa_joint < body_locals.size(); soa_joint++) {
        array<float32_t, 4> body_weights {1.0f, 1.0f, 1.0f, 1.0f};
        array<float32_t, 4> movement_weights {};

        for (size_t lane = 0; lane < 4; lane++) {
            const size_t joint = soa_joint * 4 + lane;

            if (joint < joint_count && (IsModelAnimationRuntimeJointPresent(movement_tracks[0], joint) || IsModelAnimationRuntimeJointPresent(movement_tracks[1], joint))) {
                body_weights[lane] = 0.0f;
                movement_weights[lane] = 1.0f;
            }
        }

        body_joint_weights[soa_joint] = ozz::math::simd_float4::LoadPtrU(body_weights.data());
        movement_joint_weights[soa_joint] = ozz::math::simd_float4::LoadPtrU(movement_weights.data());
    }

    array<ozz::animation::BlendingJob::Layer, 2> layers {};
    layers[0].weight = 1.0f;
    layers[0].transform = body_locals;
    layers[0].joint_weights = body_joint_weights;
    layers[1].weight = 1.0f;
    layers[1].transform = movement_locals;
    layers[1].joint_weights = movement_joint_weights;
    ozz::animation::BlendingJob blending_job;
    blending_job.layers = ozz::make_span(layers);
    blending_job.rest_pose = body_locals;
    blending_job.output = output;
    FO_STRONG_ASSERT(blending_job.Run(), "Invalid animation runtime pose movement-selection job");
}

static auto ComposeModelAnimationRuntimeTransform(const ozz::math::Transform& transform) noexcept -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    const vec3 translation {transform.translation.x, transform.translation.y, transform.translation.z};
    const quaternion rotation {transform.rotation.w, transform.rotation.x, transform.rotation.y, transform.rotation.z};
    const vec3 scale {transform.scale.x, transform.scale.y, transform.scale.z};
    return glm::translate(mat44 {1.0f}, translation) * glm::mat4_cast(rotation) * glm::scale(mat44 {1.0f}, scale);
}

static auto ExtractModelAnimationRuntimeTransform(ozz::span<const ozz::math::SoaTransform> locals, size_t joint_index) noexcept -> ModelAnimationRuntimeTransform
{
    FO_NO_STACK_TRACE_ENTRY();

    const size_t soa_joint = joint_index / 4;
    const size_t lane = joint_index % 4;
    const ozz::math::SoaTransform& local = locals[soa_joint];
    array<float32_t, 4> translation_x {};
    array<float32_t, 4> translation_y {};
    array<float32_t, 4> translation_z {};
    array<float32_t, 4> rotation_x {};
    array<float32_t, 4> rotation_y {};
    array<float32_t, 4> rotation_z {};
    array<float32_t, 4> rotation_w {};
    array<float32_t, 4> scale_x {};
    array<float32_t, 4> scale_y {};
    array<float32_t, 4> scale_z {};
    ozz::math::StorePtrU(local.translation.x, translation_x.data());
    ozz::math::StorePtrU(local.translation.y, translation_y.data());
    ozz::math::StorePtrU(local.translation.z, translation_z.data());
    ozz::math::StorePtrU(local.rotation.x, rotation_x.data());
    ozz::math::StorePtrU(local.rotation.y, rotation_y.data());
    ozz::math::StorePtrU(local.rotation.z, rotation_z.data());
    ozz::math::StorePtrU(local.rotation.w, rotation_w.data());
    ozz::math::StorePtrU(local.scale.x, scale_x.data());
    ozz::math::StorePtrU(local.scale.y, scale_y.data());
    ozz::math::StorePtrU(local.scale.z, scale_z.data());
    return ModelAnimationRuntimeTransform {
        vec3 {translation_x[lane], translation_y[lane], translation_z[lane]},
        glm::normalize(quaternion {rotation_w[lane], rotation_x[lane], rotation_y[lane], rotation_z[lane]}),
        vec3 {scale_x[lane], scale_y[lane], scale_z[lane]},
    };
}

static auto ConvertModelAnimationRuntimeMatrix(const mat44& matrix) noexcept -> ozz::math::Float4x4
{
    FO_NO_STACK_TRACE_ENTRY();

    ozz::math::Float4x4 result;

    for (mat44::length_type column = 0; column < 4; column++) {
        array<float32_t, 4> values {};

        for (mat44::length_type row = 0; row < 4; row++) {
            values[numeric_cast<size_t>(row)] = matrix[column][row];
        }

        result.cols[column] = ozz::math::simd_float4::LoadPtrU(values.data());
    }

    return result;
}

static auto ConvertModelAnimationRuntimeMatrix(const ozz::math::Float4x4& matrix) noexcept -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    mat44 result {1.0f};

    for (mat44::length_type column = 0; column < 4; column++) {
        array<float32_t, 4> values {};
        ozz::math::StorePtrU(matrix.cols[column], values.data());

        for (mat44::length_type row = 0; row < 4; row++) {
            result[column][row] = values[numeric_cast<size_t>(row)];
        }
    }

    return result;
}

FO_END_NAMESPACE

#endif
