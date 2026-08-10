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

#include "ModelAnimationConverter.h"

#if FO_ENABLE_3D

#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/skeleton_utils.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/maths/transform.h"
#include "ozz/base/memory/unique_ptr.h"

FO_BEGIN_NAMESPACE

static constexpr float32_t REST_TRANSFORM_TOLERANCE = 1.0e-4f;

struct IndexedSkeletonSource
{
    map<vector<string>, size_t> JointByHierarchy {};
    map<string, vector<string>> HierarchyByName {};
    vector<string> RootHierarchy {};
};

static auto IndexSkeletonJoints(string_view source_file, const vector<ModelSkeletonJoint>& joints) -> IndexedSkeletonSource;
static void ValidateJoint(const ModelSkeletonJoint& joint, string_view source_file);
static auto NormalizeJointHierarchy(const vector<string>& hierarchy, string_view canonical_root) -> vector<string>;
static auto FormatJointHierarchy(const vector<string>& hierarchy) -> string;
static auto RestTransformsEqual(const mat44& first, const mat44& second) noexcept -> bool;
static auto GetMaxRestTransformDifference(const mat44& first, const mat44& second) noexcept -> float32_t;
static auto JointDfsOrderLess(const ModelSkeletonJoint& first, const ModelSkeletonJoint& second) noexcept -> bool;
static auto ClipSourceIndexLess(const ModelSkeletonClipSource& first, const ModelSkeletonClipSource& second) noexcept -> bool;
static auto RootAliasLess(const ModelSkeletonRootAlias& first, const ModelSkeletonRootAlias& second) noexcept -> bool;
static auto RestPoseDivergenceLess(const ModelSkeletonRestPoseDivergence& first, const ModelSkeletonRestPoseDivergence& second) noexcept -> bool;

auto BuildModelSkeletonCompatibilityReport(const ModelSkeletonSource& base_skeleton, const_span<ModelSkeletonClipSource> clips) -> ModelSkeletonCompatibilityReport
{
    FO_STACK_TRACE_ENTRY();

    IndexedSkeletonSource base_index = IndexSkeletonJoints(base_skeleton.FileName, base_skeleton.Joints);
    FO_VERIFY_AND_THROW(!base_index.RootHierarchy.empty(), "Base model skeleton has no root joint", base_skeleton.FileName);

    ModelSkeletonCompatibilityReport report;
    report.BaseFile = base_skeleton.FileName;

    map<vector<string>, ModelSkeletonJoint> canonical_joints;
    map<string, vector<string>> canonical_hierarchy_by_name;
    map<vector<string>, string> canonical_source_by_hierarchy;

    for (const ModelSkeletonJoint& joint : base_skeleton.Joints) {
        canonical_joints.emplace(joint.Hierarchy, joint);
        canonical_hierarchy_by_name.emplace(joint.Name, joint.Hierarchy);
        canonical_source_by_hierarchy.emplace(joint.Hierarchy, base_skeleton.FileName);
    }

    vector<size_t> clip_indices;
    clip_indices.reserve(clips.size());

    for (size_t i = 0; i < clips.size(); i++) {
        clip_indices.emplace_back(i);
    }

    std::sort(clip_indices.begin(), clip_indices.end(), [&clips](size_t first, size_t second) { return ClipSourceIndexLess(clips[first], clips[second]); });

    string previous_file;
    string previous_clip;

    for (size_t clip_index : clip_indices) {
        const ModelSkeletonClipSource& clip = clips[clip_index];

        if (clip.FileName == previous_file && clip.ClipName == previous_clip) {
            continue;
        }

        previous_file = clip.FileName;
        previous_clip = clip.ClipName;

        IndexedSkeletonSource clip_skeleton_index = IndexSkeletonJoints(clip.FileName, clip.Joints);
        const string& source_root = clip_skeleton_index.RootHierarchy.front();
        const string& canonical_root = base_index.RootHierarchy.front();

        if (source_root != canonical_root) {
            report.RootAliases.emplace_back(ModelSkeletonRootAlias {clip.FileName, clip.ClipName, source_root, canonical_root});
        }

        set<vector<string>> animated_hierarchies;
        set<vector<string>> checked_canonical_hierarchies;

        for (const vector<string>& animated_hierarchy : clip.AnimatedJointHierarchies) {
            if (animated_hierarchy.empty()) {
                throw ModelSkeletonCompatibilityException("Animation contains an empty joint hierarchy", clip.ClipName, clip.FileName);
            }
            if (clip_skeleton_index.JointByHierarchy.count(animated_hierarchy) == 0) {
                throw ModelSkeletonCompatibilityException("Animation references joint hierarchy that is absent from its source skeleton", clip.ClipName, clip.FileName, FormatJointHierarchy(animated_hierarchy));
            }

            animated_hierarchies.emplace(animated_hierarchy);
        }

        for (const vector<string>& animated_hierarchy : animated_hierarchies) {
            for (size_t depth = 1; depth <= animated_hierarchy.size(); depth++) {
                vector<string> source_hierarchy(animated_hierarchy.begin(), animated_hierarchy.begin() + numeric_cast<ptrdiff_t>(depth));
                auto source_joint_it = clip_skeleton_index.JointByHierarchy.find(source_hierarchy);

                if (source_joint_it == clip_skeleton_index.JointByHierarchy.end()) {
                    throw ModelSkeletonCompatibilityException("Animation has a discontinuous joint hierarchy whose ancestor is absent from its source skeleton", clip.ClipName, clip.FileName, FormatJointHierarchy(animated_hierarchy), FormatJointHierarchy(source_hierarchy));
                }

                const ModelSkeletonJoint& source_joint = clip.Joints[source_joint_it->second];
                vector<string> canonical_hierarchy = NormalizeJointHierarchy(source_hierarchy, canonical_root);

                if (!checked_canonical_hierarchies.emplace(canonical_hierarchy).second) {
                    continue;
                }

                string canonical_name = canonical_hierarchy.back();
                auto canonical_joint_it = canonical_joints.find(canonical_hierarchy);

                if (canonical_joint_it != canonical_joints.end()) {
                    if (!RestTransformsEqual(canonical_joint_it->second.RestLocalTransform, source_joint.RestLocalTransform)) {
                        report.RestPoseDivergences.emplace_back(ModelSkeletonRestPoseDivergence {
                            canonical_source_by_hierarchy.at(canonical_hierarchy),
                            clip.FileName,
                            clip.ClipName,
                            canonical_hierarchy,
                            GetMaxRestTransformDifference(canonical_joint_it->second.RestLocalTransform, source_joint.RestLocalTransform),
                        });
                    }
                    continue;
                }

                if (auto name_it = canonical_hierarchy_by_name.find(canonical_name); name_it != canonical_hierarchy_by_name.end()) {
                    throw ModelSkeletonCompatibilityException("Joint parent conflict between canonical and requested joint paths", canonical_name, clip.ClipName, clip.FileName, FormatJointHierarchy(name_it->second), FormatJointHierarchy(canonical_hierarchy));
                }

                ModelSkeletonJoint canonical_joint = source_joint;
                canonical_joint.Name = canonical_name;
                canonical_joint.Hierarchy = canonical_hierarchy;
                canonical_joints.emplace(canonical_hierarchy, canonical_joint);
                canonical_hierarchy_by_name.emplace(canonical_name, canonical_hierarchy);
                canonical_source_by_hierarchy.emplace(canonical_hierarchy, strex("{}#{}", clip.FileName, clip.ClipName));
                report.ContributedJoints.emplace_back(ModelSkeletonJointContribution {clip.FileName, clip.ClipName, std::move(canonical_joint)});
            }
        }
    }

    report.CanonicalJoints.reserve(canonical_joints.size());

    for (auto& [hierarchy, joint] : canonical_joints) {
        ignore_unused(hierarchy);
        report.CanonicalJoints.emplace_back(std::move(joint));
    }

    std::sort(report.CanonicalJoints.begin(), report.CanonicalJoints.end(), JointDfsOrderLess);
    std::sort(report.ContributedJoints.begin(), report.ContributedJoints.end(), [](const ModelSkeletonJointContribution& first, const ModelSkeletonJointContribution& second) { return JointDfsOrderLess(first.Joint, second.Joint); });
    std::sort(report.RootAliases.begin(), report.RootAliases.end(), RootAliasLess);
    std::sort(report.RestPoseDivergences.begin(), report.RestPoseDivergences.end(), RestPoseDivergenceLess);
    return report;
}

auto FormatModelSkeletonCompatibilityReport(const ModelSkeletonCompatibilityReport& report) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result = strex("base='{}' canonical_joints={} contributed_joints={} root_aliases={} rest_pose_divergences={} animation_data_issues={}", report.BaseFile, report.CanonicalJoints.size(), report.ContributedJoints.size(), report.RootAliases.size(), report.RestPoseDivergences.size(), report.AnimationDataIssues.size());

    if (!report.ContributedJoints.empty()) {
        result += " contributions=[";

        for (size_t i = 0; i < report.ContributedJoints.size(); i++) {
            if (i != 0) {
                result += ", ";
            }

            const ModelSkeletonJointContribution& contribution = report.ContributedJoints[i];
            result += strex("{} <- {}#{}", FormatJointHierarchy(contribution.Joint.Hierarchy), contribution.FileName, contribution.ClipName);
        }

        result += "]";
    }

    if (!report.RootAliases.empty()) {
        result += " root_alias_mappings=[";

        for (size_t i = 0; i < report.RootAliases.size(); i++) {
            if (i != 0) {
                result += ", ";
            }

            const ModelSkeletonRootAlias& alias = report.RootAliases[i];
            result += strex("{} -> {} for {}#{}", alias.SourceRoot, alias.CanonicalRoot, alias.FileName, alias.ClipName);
        }

        result += "]";
    }

    if (!report.RestPoseDivergences.empty()) {
        set<pair<string, string>> divergent_clips;
        set<vector<string>> divergent_joints;
        size_t worst_index = 0;

        for (size_t i = 0; i < report.RestPoseDivergences.size(); i++) {
            const ModelSkeletonRestPoseDivergence& divergence = report.RestPoseDivergences[i];
            divergent_clips.emplace(divergence.FileName, divergence.ClipName);
            divergent_joints.emplace(divergence.Hierarchy);

            if (divergence.MaxDifference > report.RestPoseDivergences[worst_index].MaxDifference) {
                worst_index = i;
            }
        }

        const ModelSkeletonRestPoseDivergence& worst = report.RestPoseDivergences[worst_index];
        result += strex(" rest_pose_metrics=[divergent_clips={} divergent_joints={} max_delta={} worst={} <- {}#{} canonical_source={}]", divergent_clips.size(), divergent_joints.size(), worst.MaxDifference, FormatJointHierarchy(worst.Hierarchy), worst.FileName, worst.ClipName, worst.CanonicalSource);
    }

    if (!report.AnimationDataIssues.empty()) {
        size_t non_finite_keys = 0;
        size_t zero_rotation_keys = 0;

        for (const ModelSkeletonAnimationDataIssue& issue : report.AnimationDataIssues) {
            non_finite_keys += issue.NonFiniteScaleKeys + issue.NonFiniteRotationKeys + issue.NonFiniteTranslationKeys;
            zero_rotation_keys += issue.ZeroRotationKeys;
        }

        result += strex(" animation_data_metrics=[non_finite_keys={} zero_rotation_keys={} joints=[", non_finite_keys, zero_rotation_keys);

        for (size_t i = 0; i < report.AnimationDataIssues.size(); i++) {
            if (i != 0) {
                result += ", ";
            }

            const ModelSkeletonAnimationDataIssue& issue = report.AnimationDataIssues[i];
            result += strex("{} <- {}#{} scale={} rotation={} zero_rotation={} translation={}", FormatJointHierarchy(issue.Hierarchy), issue.FileName, issue.ClipName, issue.NonFiniteScaleKeys, issue.NonFiniteRotationKeys, issue.ZeroRotationKeys, issue.NonFiniteTranslationKeys);
        }

        result += "]]";
    }

    return result;
}

static auto IndexSkeletonJoints(string_view source_file, const vector<ModelSkeletonJoint>& joints) -> IndexedSkeletonSource
{
    FO_STACK_TRACE_ENTRY();

    if (joints.empty()) {
        throw ModelSkeletonCompatibilityException("Skeleton source has no joints", source_file);
    }

    IndexedSkeletonSource result;

    for (size_t i = 0; i < joints.size(); i++) {
        const ModelSkeletonJoint& joint = joints[i];
        ValidateJoint(joint, source_file);

        if (!result.JointByHierarchy.emplace(joint.Hierarchy, i).second) {
            throw ModelSkeletonCompatibilityException("Skeleton source has duplicate joint hierarchy", source_file, FormatJointHierarchy(joint.Hierarchy));
        }

        const auto [name_it, name_inserted] = result.HierarchyByName.emplace(joint.Name, joint.Hierarchy);

        if (!name_inserted) {
            throw ModelSkeletonCompatibilityException("Skeleton source has duplicate joint name at two hierarchies", source_file, joint.Name, FormatJointHierarchy(name_it->second), FormatJointHierarchy(joint.Hierarchy));
        }

        if (joint.Hierarchy.size() == 1) {
            if (!result.RootHierarchy.empty()) {
                throw ModelSkeletonCompatibilityException("Skeleton source has multiple root joints", source_file, FormatJointHierarchy(result.RootHierarchy), FormatJointHierarchy(joint.Hierarchy));
            }

            result.RootHierarchy = joint.Hierarchy;
        }
    }

    if (result.RootHierarchy.empty()) {
        throw ModelSkeletonCompatibilityException("Skeleton source has no root joint", source_file);
    }

    for (const ModelSkeletonJoint& joint : joints) {
        if (joint.Hierarchy.size() <= 1) {
            continue;
        }

        vector<string> parent_hierarchy(joint.Hierarchy.begin(), joint.Hierarchy.end() - 1);

        if (result.JointByHierarchy.count(parent_hierarchy) == 0) {
            throw ModelSkeletonCompatibilityException("Skeleton source has joint without parent hierarchy", source_file, FormatJointHierarchy(joint.Hierarchy), FormatJointHierarchy(parent_hierarchy));
        }
    }

    return result;
}

static void ValidateJoint(const ModelSkeletonJoint& joint, string_view source_file)
{
    FO_STACK_TRACE_ENTRY();

    if (joint.Hierarchy.empty()) {
        throw ModelSkeletonCompatibilityException("Skeleton source contains joint with an empty hierarchy", source_file, joint.Name);
    }
    if (joint.Hierarchy.back() != joint.Name) {
        throw ModelSkeletonCompatibilityException("Skeleton source contains joint whose hierarchy does not end with its name", source_file, joint.Name, joint.Hierarchy.back());
    }
    if (joint.Name.empty() && joint.Hierarchy.size() != 1) {
        throw ModelSkeletonCompatibilityException("Skeleton source contains an empty non-root joint name", source_file, FormatJointHierarchy(joint.Hierarchy));
    }

    for (mat44::length_type column = 0; column < 4; column++) {
        for (mat44::length_type row = 0; row < 4; row++) {
            if (!std::isfinite(joint.RestLocalTransform[column][row])) {
                throw ModelSkeletonCompatibilityException("Skeleton source contains non-finite rest transform at joint", source_file, FormatJointHierarchy(joint.Hierarchy));
            }
        }
    }
}

static auto NormalizeJointHierarchy(const vector<string>& hierarchy, string_view canonical_root) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!hierarchy.empty(), "Can't normalize an empty joint hierarchy");
    vector<string> result = hierarchy;
    result.front() = canonical_root;
    return result;
}

static auto FormatJointHierarchy(const vector<string>& hierarchy) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;

    for (size_t i = 0; i < hierarchy.size(); i++) {
        if (i != 0) {
            result += '/';
        }

        const string& name = hierarchy[i];
        result += !name.empty() ? name : (i == 0 ? "<empty-root>" : "<empty>");
    }

    return result;
}

static auto RestTransformsEqual(const mat44& first, const mat44& second) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    for (mat44::length_type column = 0; column < 4; column++) {
        for (mat44::length_type row = 0; row < 4; row++) {
            if (!is_float_equal(first[column][row], second[column][row], REST_TRANSFORM_TOLERANCE)) {
                return false;
            }
        }
    }

    return true;
}

static auto GetMaxRestTransformDifference(const mat44& first, const mat44& second) noexcept -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    float32_t max_difference = 0.0f;

    for (mat44::length_type column = 0; column < 4; column++) {
        for (mat44::length_type row = 0; row < 4; row++) {
            max_difference = std::max(max_difference, float_abs(first[column][row] - second[column][row]));
        }
    }

    return max_difference;
}

static auto JointDfsOrderLess(const ModelSkeletonJoint& first, const ModelSkeletonJoint& second) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return first.Hierarchy < second.Hierarchy;
}

static auto ClipSourceIndexLess(const ModelSkeletonClipSource& first, const ModelSkeletonClipSource& second) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::tie(first.FileName, first.ClipName) < std::tie(second.FileName, second.ClipName);
}

static auto RootAliasLess(const ModelSkeletonRootAlias& first, const ModelSkeletonRootAlias& second) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::tie(first.FileName, first.ClipName, first.SourceRoot, first.CanonicalRoot) < std::tie(second.FileName, second.ClipName, second.SourceRoot, second.CanonicalRoot);
}

static auto RestPoseDivergenceLess(const ModelSkeletonRestPoseDivergence& first, const ModelSkeletonRestPoseDivergence& second) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::tie(first.Hierarchy, first.FileName, first.ClipName, first.CanonicalSource, first.MaxDifference) < std::tie(second.Hierarchy, second.FileName, second.ClipName, second.CanonicalSource, second.MaxDifference);
}

static constexpr uint64_t MODEL_ANIMATION_HASH_OFFSET = 14695981039346656037ULL;
static constexpr uint64_t MODEL_ANIMATION_HASH_PRIME = 1099511628211ULL;
static constexpr float32_t MODEL_ANIMATION_AFFINE_TOLERANCE = 1.0e-5f;
static constexpr float32_t MODEL_ANIMATION_ORTHOGONAL_TOLERANCE = 1.0e-4f;
static constexpr float32_t MODEL_ANIMATION_ROTATION_REFINEMENT_ERROR_RADIANS = 0.000872664626f; // 0.05 degree; leaves room for runtime quantization under the 0.1-degree parity budget.
static constexpr size_t MODEL_ANIMATION_ROTATION_SUBDIVISION_MAX_DEPTH = 16;

static_assert(MODEL_ANIMATION_RIG_MAX_JOINTS == static_cast<uint32_t>(ozz::animation::Skeleton::kMaxJoints));

struct ModelAnimationCanonicalRig
{
    vector<ozz::math::Transform> RestTransforms {};
    vector<int16_t> Parents {};
    map<vector<string>, uint32_t> JointByHierarchy {};
    vector<uint8_t> BaseJointPresent {};
};

static auto BuildModelAnimationCanonicalRig(const ModelSkeletonSource& base_skeleton, const ModelSkeletonCompatibilityReport& compatibility_report) -> ModelAnimationCanonicalRig;
static auto BuildModelAnimationRuntimeSkeleton(const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig) -> ozz::unique_ptr<ozz::animation::Skeleton>;
static void FillModelAnimationRawSkeletonJoint(uint32_t joint_index, const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig, const vector<vector<uint32_t>>& children, ozz::animation::offline::RawSkeleton::Joint& raw_joint);
static auto DecomposeModelAnimationTransform(const mat44& matrix, string_view context) -> ozz::math::Transform;
static auto ComposeModelAnimationTransform(const ozz::math::Transform& transform) -> mat44;
static auto BuildModelAnimationClipArtifact(const ModelAnimationSource& animation, const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig, uint64_t rig_signature, uint64_t cache_signature, bool nearest_sampling) -> ModelAnimationClipArtifact;
static auto BuildModelAnimationRawAnimation(const ModelAnimationSource& animation, const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig, ModelAnimationJointRemap& joint_remap, bool nearest_sampling) -> ozz::animation::offline::RawAnimation;
static void FillModelAnimationFallbackTrack(const ozz::math::Transform& transform, float32_t duration, string_view context, ozz::animation::offline::RawAnimation::JointTrack& track);
static void FillModelAnimationAuthoredTrack(const ModelAnimationJointSource& source, float32_t duration, bool nearest_sampling, string_view animation_context, ozz::animation::offline::RawAnimation::JointTrack& track);
static void RefineModelAnimationRotationTrack(ozz::animation::offline::RawAnimation::JointTrack::Rotations& rotations, string_view context);
static void AppendModelAnimationRefinedRotationSegment(const ozz::animation::offline::RawAnimation::RotationKey& first, const ozz::animation::offline::RawAnimation::RotationKey& second, size_t depth, string_view context, ozz::animation::offline::RawAnimation::JointTrack::Rotations& result);
static auto GetModelAnimationNlerpError(const ozz::math::Quaternion& first, const ozz::math::Quaternion& second, float32_t factor) -> float32_t;
static auto BuildModelAnimationNearestTimeline(const ModelAnimationSource& animation) -> vector<float32_t>;
static auto CropModelAnimationTimeline(const vector<float32_t>& times, float32_t duration) -> vector<float32_t>;
static auto EvaluateModelAnimationLegacyTrack(const ModelAnimationVec3Track& track, float32_t time, bool nearest_sampling, string_view context) -> vec3;
static auto EvaluateModelAnimationLegacyTrack(const ModelAnimationQuaternionTrack& track, float32_t time, bool nearest_sampling, string_view context) -> quaternion;
static void ValidateModelAnimationVec3Track(const ModelAnimationVec3Track& track, string_view context, bool half_range);
static void ValidateModelAnimationQuaternionTrack(const ModelAnimationQuaternionTrack& track, string_view context);
static auto NormalizeModelAnimationHierarchy(const vector<string>& hierarchy, string_view canonical_root) -> vector<string>;
static auto FormatModelAnimationHierarchy(const vector<string>& hierarchy) -> string;
static void ValidateModelAnimationJointName(string_view name, string_view context, bool root);
template<typename T>
static auto SerializeModelAnimationObject(const T& object, string_view context) -> vector<uint8_t>;
template<typename T>
static auto DeserializeModelAnimationObject(const_span<uint8_t> payload, string_view context) -> T;
static auto WrapAndValidateModelAnimationArchive(const ModelAnimationArchiveMetadata& metadata, const_span<uint8_t> payload) -> vector<uint8_t>;
static void ValidateModelAnimationSkeletonRoundTrip(const ozz::animation::Skeleton& skeleton, const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig, string_view context);
static void ValidateModelAnimationRoundTrip(const ozz::animation::Animation& animation, const ModelAnimationSource& source, size_t canonical_joint_count, string_view context);
static auto HashModelAnimationRig(const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig) -> uint64_t;
static auto HashModelAnimationBaseSource(const ModelSkeletonSource& base_skeleton, const ModelAnimationJointRemap& remap) -> uint64_t;
static auto HashModelAnimationSource(const ozz::animation::offline::RawAnimation& animation, const ModelAnimationJointRemap& remap) -> uint64_t;
static auto HashModelAnimationJointRemap(const ModelAnimationJointRemap& remap) -> uint64_t;
static auto HashModelAnimationConverterPolicy(bool nearest_sampling) -> uint64_t;
static void HashModelAnimationByte(uint64_t& hash, uint8_t value);
template<typename T>
static void HashModelAnimationUnsigned(uint64_t& hash, T value);
static void HashModelAnimationFloat(uint64_t& hash, float32_t value);
static void HashModelAnimationString(uint64_t& hash, string_view value);
static auto FinishModelAnimationHash(uint64_t hash) -> uint64_t;

auto BuildModelAnimationRigArtifacts(string_view model_description, const ModelSkeletonSource& base_skeleton, const ModelSkeletonCompatibilityReport& compatibility_report, const_span<ModelAnimationSource> animations, bool nearest_sampling) -> ModelAnimationRigArtifacts
{
    FO_STACK_TRACE_ENTRY();

    InitializeModelAnimationMemory();

    if (model_description.empty()) {
        throw ModelAnimationConverterException("Can't build canonical animation artifacts for an empty model-description path");
    }
    if (!strvex(model_description).is_valid_utf8() || model_description.find('\0') != string_view::npos) {
        throw ModelAnimationConverterException("Model-description path is not a valid animation archive identity", model_description);
    }
    if (compatibility_report.BaseFile != base_skeleton.FileName) {
        throw ModelAnimationConverterException("Canonical animation report/base file mismatch", model_description, compatibility_report.BaseFile, base_skeleton.FileName);
    }

    ModelAnimationCanonicalRig canonical_rig = BuildModelAnimationCanonicalRig(base_skeleton, compatibility_report);
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton = BuildModelAnimationRuntimeSkeleton(compatibility_report, canonical_rig);

    ModelAnimationRigArtifacts result;
    result.RigSignature = HashModelAnimationRig(compatibility_report, canonical_rig);
    result.CacheSignature = HashModelAnimationConverterPolicy(nearest_sampling);
    result.BaseJointRemap.CanonicalJointCount = numeric_cast<uint32_t>(compatibility_report.CanonicalJoints.size());
    result.BaseJointRemap.CanonicalJointPresent = canonical_rig.BaseJointPresent;
    result.BaseJointRemap.SourceToCanonicalJointIndices.reserve(base_skeleton.Joints.size());

    for (const ModelSkeletonJoint& base_joint : base_skeleton.Joints) {
        auto canonical_it = canonical_rig.JointByHierarchy.find(base_joint.Hierarchy);

        if (canonical_it == canonical_rig.JointByHierarchy.end()) {
            throw ModelAnimationConverterException("Base joint is absent from canonical animation rig", FormatModelAnimationHierarchy(base_joint.Hierarchy), base_skeleton.FileName, model_description);
        }

        result.BaseJointRemap.SourceToCanonicalJointIndices.emplace_back(canonical_it->second);
    }

    uint64_t skeleton_source_signature = HashModelAnimationBaseSource(base_skeleton, result.BaseJointRemap);
    result.SkeletonMetadata = ModelAnimationArchiveMetadata {
        ModelAnimationArchiveKind::Skeleton,
        MODEL_ANIMATION_ARCHIVE_SUPPORTED_FLAGS,
        result.RigSignature,
        skeleton_source_signature,
        result.CacheSignature,
        string {model_description},
        "CanonicalSkeleton",
    };

    vector<uint8_t> skeleton_payload = SerializeModelAnimationObject(*skeleton, strex("canonical skeleton for '{}'", model_description));
    result.SkeletonArchive = WrapAndValidateModelAnimationArchive(result.SkeletonMetadata, skeleton_payload);
    ModelAnimationArchive loaded_skeleton_archive = ReadModelAnimationArchive(result.SkeletonArchive, result.SkeletonMetadata);
    ozz::animation::Skeleton loaded_skeleton = DeserializeModelAnimationObject<ozz::animation::Skeleton>(loaded_skeleton_archive.Payload, strex("canonical skeleton for '{}'", model_description));
    ValidateModelAnimationSkeletonRoundTrip(loaded_skeleton, compatibility_report, canonical_rig, model_description);

    result.BaseJointRemapMetadata = ModelAnimationArchiveMetadata {
        ModelAnimationArchiveKind::JointRemap,
        MODEL_ANIMATION_ARCHIVE_SUPPORTED_FLAGS,
        result.RigSignature,
        HashModelAnimationJointRemap(result.BaseJointRemap),
        result.CacheSignature,
        base_skeleton.FileName,
        "BaseJointRemap",
    };

    vector<uint8_t> base_remap_payload = WriteModelAnimationJointRemapPayload(result.BaseJointRemap, strex("base remap for '{}'", model_description));
    result.BaseJointRemapArchive = WrapAndValidateModelAnimationArchive(result.BaseJointRemapMetadata, base_remap_payload);
    ModelAnimationArchive loaded_base_remap_archive = ReadModelAnimationArchive(result.BaseJointRemapArchive, result.BaseJointRemapMetadata);
    ModelAnimationJointRemap loaded_base_remap = ReadModelAnimationJointRemapPayload(loaded_base_remap_archive.Payload, strex("base remap for '{}'", model_description));

    if (loaded_base_remap.Duration != result.BaseJointRemap.Duration || loaded_base_remap.SourceToCanonicalJointIndices != result.BaseJointRemap.SourceToCanonicalJointIndices || loaded_base_remap.CanonicalJointPresent != result.BaseJointRemap.CanonicalJointPresent || loaded_base_remap.NearestSampleTimes != result.BaseJointRemap.NearestSampleTimes) {
        throw ModelAnimationConverterException("Base-joint remap round-trip mismatch", model_description);
    }

    map<pair<string, string>, size_t> unique_animations;

    for (size_t i = 0; i < animations.size(); i++) {
        const ModelAnimationSource& animation = animations[i];
        unique_animations.try_emplace({animation.FileName, animation.Name}, i);
    }

    result.Clips.reserve(unique_animations.size());

    for (const auto& [identity, animation_index] : unique_animations) {
        ignore_unused(identity);
        result.Clips.emplace_back(BuildModelAnimationClipArtifact(animations[animation_index], compatibility_report, canonical_rig, result.RigSignature, result.CacheSignature, nearest_sampling));
    }

    return result;
}

auto BuildModelAnimationRigData(ModelAnimationRigArtifacts artifacts, const_span<ModelAnimationRigBindingSource> bindings) -> ModelAnimationRigData
{
    FO_STACK_TRACE_ENTRY();

    auto take_archive_payload = [](ModelAnimationArchiveMetadata& metadata, vector<uint8_t>& archive_data) -> ModelAnimationRigArchiveData {
        ModelAnimationArchive archive = ReadModelAnimationArchive(archive_data, metadata);
        return ModelAnimationRigArchiveData {std::move(metadata), std::move(archive.Payload)};
    };

    ModelAnimationRigData result;
    result.RigSignature = artifacts.RigSignature;
    result.CacheSignature = artifacts.CacheSignature;
    result.Skeleton = take_archive_payload(artifacts.SkeletonMetadata, artifacts.SkeletonArchive);
    result.BaseJointRemap = take_archive_payload(artifacts.BaseJointRemapMetadata, artifacts.BaseJointRemapArchive);
    map<pair<string, string>, uint32_t> clip_indices;
    result.Clips.reserve(artifacts.Clips.size());

    for (ModelAnimationClipArtifact& artifact : artifacts.Clips) {
        uint32_t clip_index = numeric_cast<uint32_t>(result.Clips.size());

        if (!clip_indices.emplace(std::make_pair(artifact.SourceFile, artifact.ClipName), clip_index).second) {
            throw ModelAnimationConverterException("Duplicate animation clip artifact while building production rig data", artifact.SourceFile, artifact.ClipName);
        }

        ModelAnimationRigClipData& clip = result.Clips.emplace_back();
        clip.Animation = take_archive_payload(artifact.AnimationMetadata, artifact.AnimationArchive);
        clip.JointRemap = take_archive_payload(artifact.JointRemapMetadata, artifact.JointRemapArchive);
    }

    map<pair<int32_t, int32_t>, ptr<const ModelAnimationRigBindingSource>> sorted_bindings;

    for (const ModelAnimationRigBindingSource& binding : bindings) {
        if (!sorted_bindings.emplace(std::make_pair(binding.StateAnim, binding.ActionAnim), &binding).second) {
            throw ModelAnimationConverterException("Duplicate animation binding while building production rig data", binding.StateAnim, binding.ActionAnim);
        }
    }

    result.Bindings.reserve(sorted_bindings.size());

    for (const auto& [animation_pair, binding] : sorted_bindings) {
        auto clip_it = clip_indices.find({binding->SourceFile, binding->ClipName});

        if (clip_it == clip_indices.end()) {
            throw ModelAnimationConverterException("Animation binding references missing clip", animation_pair.first, animation_pair.second, binding->SourceFile, binding->ClipName);
        }

        result.Bindings.emplace_back(ModelAnimationRigBinding {
            animation_pair.first,
            animation_pair.second,
            clip_it->second,
            binding->Reversed,
        });
    }

    return result;
}

static auto BuildModelAnimationCanonicalRig(const ModelSkeletonSource& base_skeleton, const ModelSkeletonCompatibilityReport& compatibility_report) -> ModelAnimationCanonicalRig
{
    FO_STACK_TRACE_ENTRY();

    if (compatibility_report.CanonicalJoints.empty()) {
        throw ModelAnimationConverterException("Canonical animation rig has no joints", base_skeleton.FileName);
    }
    if (compatibility_report.CanonicalJoints.size() > static_cast<size_t>(ozz::animation::Skeleton::kMaxJoints)) {
        throw ModelAnimationConverterException("Canonical animation rig has more joints than ozz supports", base_skeleton.FileName, compatibility_report.CanonicalJoints.size(), ozz::animation::Skeleton::kMaxJoints);
    }

    ModelAnimationCanonicalRig result;
    size_t joint_count = compatibility_report.CanonicalJoints.size();
    result.RestTransforms.resize(joint_count);
    result.Parents.resize(joint_count, static_cast<int16_t>(ozz::animation::Skeleton::kNoParent));
    result.BaseJointPresent.resize(joint_count, uint8_t {0});

    for (size_t i = 0; i < joint_count; i++) {
        const ModelSkeletonJoint& joint = compatibility_report.CanonicalJoints[i];

        if (joint.Hierarchy.empty() || joint.Hierarchy.back() != joint.Name) {
            throw ModelAnimationConverterException("Canonical animation joint has an invalid hierarchy", i, base_skeleton.FileName);
        }
        if (i != 0 && !(compatibility_report.CanonicalJoints[i - 1].Hierarchy < joint.Hierarchy)) {
            throw ModelAnimationConverterException("Canonical animation joints are not in strict deterministic hierarchy order", base_skeleton.FileName, FormatModelAnimationHierarchy(joint.Hierarchy));
        }

        ValidateModelAnimationJointName(joint.Name, strex("canonical joint '{}' from '{}'", FormatModelAnimationHierarchy(joint.Hierarchy), base_skeleton.FileName), joint.Hierarchy.size() == 1);
        uint32_t canonical_index = numeric_cast<uint32_t>(i);

        if (!result.JointByHierarchy.emplace(joint.Hierarchy, canonical_index).second) {
            throw ModelAnimationConverterException("Canonical animation rig contains duplicate hierarchy", base_skeleton.FileName, FormatModelAnimationHierarchy(joint.Hierarchy));
        }

        if (joint.Hierarchy.size() == 1) {
            if (i != 0) {
                throw ModelAnimationConverterException("Canonical animation rig contains more than one root", base_skeleton.FileName);
            }
        }
        else {
            vector<string> parent_hierarchy(joint.Hierarchy.begin(), joint.Hierarchy.end() - 1);
            auto parent_it = result.JointByHierarchy.find(parent_hierarchy);

            if (parent_it == result.JointByHierarchy.end()) {
                throw ModelAnimationConverterException("Canonical animation joint has no parent hierarchy", FormatModelAnimationHierarchy(joint.Hierarchy), base_skeleton.FileName, FormatModelAnimationHierarchy(parent_hierarchy));
            }

            result.Parents[i] = numeric_cast<int16_t>(parent_it->second);
        }
    }

    map<vector<string>, ptr<const ModelSkeletonJoint>> base_joint_by_hierarchy;

    for (const ModelSkeletonJoint& base_joint : base_skeleton.Joints) {
        if (!base_joint_by_hierarchy.emplace(base_joint.Hierarchy, &base_joint).second) {
            throw ModelAnimationConverterException("Base skeleton contains duplicate hierarchy while building animation rig", base_skeleton.FileName, FormatModelAnimationHierarchy(base_joint.Hierarchy));
        }
    }

    for (size_t i = 0; i < joint_count; i++) {
        const ModelSkeletonJoint& canonical_joint = compatibility_report.CanonicalJoints[i];
        auto base_it = base_joint_by_hierarchy.find(canonical_joint.Hierarchy);

        if (base_it != base_joint_by_hierarchy.end()) {
            result.RestTransforms[i] = DecomposeModelAnimationTransform(base_it->second->RestLocalTransform, strex("base rest joint '{}' from '{}'", FormatModelAnimationHierarchy(canonical_joint.Hierarchy), base_skeleton.FileName));
            result.BaseJointPresent[i] = uint8_t {1};
        }
        else {
            // Legacy materializes animation-only hierarchy nodes as ModelBone{},
            // whose GLM-initialized local matrix is identity.
            result.RestTransforms[i] = ozz::math::Transform::identity();
        }
    }

    return result;
}

static auto BuildModelAnimationRuntimeSkeleton(const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig) -> ozz::unique_ptr<ozz::animation::Skeleton>
{
    FO_STACK_TRACE_ENTRY();

    vector<vector<uint32_t>> children(compatibility_report.CanonicalJoints.size());

    for (size_t i = 1; i < canonical_rig.Parents.size(); i++) {
        int16_t parent = canonical_rig.Parents[i];

        if (parent < 0 || numeric_cast<size_t>(parent) >= i) {
            throw ModelAnimationConverterException("Canonical ozz parent index is invalid for joint", parent, i, compatibility_report.BaseFile);
        }

        children[numeric_cast<size_t>(parent)].emplace_back(numeric_cast<uint32_t>(i));
    }

    ozz::animation::offline::RawSkeleton raw_skeleton;
    raw_skeleton.roots.resize(1);
    FillModelAnimationRawSkeletonJoint(0, compatibility_report, canonical_rig, children, raw_skeleton.roots[0]);

    if (!raw_skeleton.Validate() || raw_skeleton.num_joints() != numeric_cast<int>(compatibility_report.CanonicalJoints.size())) {
        throw ModelAnimationConverterException("Raw canonical ozz skeleton validation failed", compatibility_report.BaseFile);
    }

    ozz::animation::offline::SkeletonBuilder builder;
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton = builder(raw_skeleton);

    if (!skeleton) {
        throw ModelAnimationConverterException("Ozz SkeletonBuilder failed", compatibility_report.BaseFile, compatibility_report.CanonicalJoints.size());
    }

    ValidateModelAnimationSkeletonRoundTrip(*skeleton, compatibility_report, canonical_rig, compatibility_report.BaseFile);
    return skeleton;
}

static void FillModelAnimationRawSkeletonJoint(uint32_t joint_index, const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig, const vector<vector<uint32_t>>& children, ozz::animation::offline::RawSkeleton::Joint& raw_joint)
{
    FO_STACK_TRACE_ENTRY();

    const ModelSkeletonJoint& joint = compatibility_report.CanonicalJoints[joint_index];
    raw_joint.name = joint.Name.c_str();
    raw_joint.transform = canonical_rig.RestTransforms[joint_index];
    raw_joint.children.resize(children[joint_index].size());

    for (size_t i = 0; i < children[joint_index].size(); i++) {
        FillModelAnimationRawSkeletonJoint(children[joint_index][i], compatibility_report, canonical_rig, children, raw_joint.children[i]);
    }
}

static auto DecomposeModelAnimationTransform(const mat44& matrix, string_view context) -> ozz::math::Transform
{
    FO_STACK_TRACE_ENTRY();

    for (mat44::length_type column = 0; column < 4; column++) {
        for (mat44::length_type row = 0; row < 4; row++) {
            if (!std::isfinite(matrix[column][row])) {
                throw ModelAnimationConverterException("Non-finite matrix component", column, row, context);
            }
        }
    }

    if (float_abs(matrix[0][3]) > MODEL_ANIMATION_AFFINE_TOLERANCE || float_abs(matrix[1][3]) > MODEL_ANIMATION_AFFINE_TOLERANCE || float_abs(matrix[2][3]) > MODEL_ANIMATION_AFFINE_TOLERANCE || float_abs(matrix[3][3] - 1.0f) > MODEL_ANIMATION_AFFINE_TOLERANCE) {
        throw ModelAnimationConverterException("Non-affine rest matrix", context);
    }

    vec3 source_axis_x {matrix[0]};
    vec3 source_axis_y {matrix[1]};
    vec3 source_axis_z {matrix[2]};
    float32_t scale_x = glm::length(source_axis_x);
    float32_t scale_y = glm::length(source_axis_y);
    float32_t scale_z = glm::length(source_axis_z);

    if (!std::isfinite(scale_x) || !std::isfinite(scale_y) || !std::isfinite(scale_z) || scale_x <= std::numeric_limits<float32_t>::epsilon() || scale_y <= std::numeric_limits<float32_t>::epsilon() || scale_z <= std::numeric_limits<float32_t>::epsilon()) {
        throw ModelAnimationConverterException("Zero or invalid rest scale", scale_x, scale_y, scale_z, context);
    }

    vec3 axis_x = source_axis_x / scale_x;
    vec3 axis_y = source_axis_y / scale_y;
    vec3 axis_z = source_axis_z / scale_z;
    float32_t xy_dot = glm::dot(axis_x, axis_y);
    float32_t xz_dot = glm::dot(axis_x, axis_z);
    float32_t yz_dot = glm::dot(axis_y, axis_z);

    if (float_abs(xy_dot) > MODEL_ANIMATION_ORTHOGONAL_TOLERANCE || float_abs(xz_dot) > MODEL_ANIMATION_ORTHOGONAL_TOLERANCE || float_abs(yz_dot) > MODEL_ANIMATION_ORTHOGONAL_TOLERANCE) {
        throw ModelAnimationConverterException("Rest matrix contains shear; normalized axis dots are non-zero", context, xy_dot, xz_dot, yz_dot);
    }

    float32_t determinant = glm::dot(glm::cross(axis_x, axis_y), axis_z);

    if (!std::isfinite(determinant) || float_abs(float_abs(determinant) - 1.0f) > MODEL_ANIMATION_ORTHOGONAL_TOLERANCE) {
        throw ModelAnimationConverterException("Rest matrix has invalid normalized determinant", determinant, context);
    }
    if (determinant < 0.0f) {
        scale_x = -scale_x;
        axis_x = -axis_x;
    }

    glm::mat<3, 3, float32_t, glm::defaultp> rotation_matrix {1.0f};
    rotation_matrix[0] = axis_x;
    rotation_matrix[1] = axis_y;
    rotation_matrix[2] = axis_z;
    quaternion rotation = glm::normalize(glm::quat_cast(rotation_matrix));

    if (!std::isfinite(rotation.x) || !std::isfinite(rotation.y) || !std::isfinite(rotation.z) || !std::isfinite(rotation.w)) {
        throw ModelAnimationConverterException("Rest matrix produced a non-finite rotation", context);
    }
    if (rotation.w < 0.0f) {
        rotation = -rotation;
    }

    ozz::math::Transform result;
    result.translation = ozz::math::Float3 {matrix[3][0], matrix[3][1], matrix[3][2]};
    result.rotation = ozz::math::Quaternion {rotation.x, rotation.y, rotation.z, rotation.w};
    result.scale = ozz::math::Float3 {scale_x, scale_y, scale_z};

    mat44 round_trip = ComposeModelAnimationTransform(result);

    for (mat44::length_type column = 0; column < 4; column++) {
        for (mat44::length_type row = 0; row < 4; row++) {
            float32_t source_value = matrix[column][row];
            float32_t round_trip_value = round_trip[column][row];
            float32_t difference = float_abs(source_value - round_trip_value);
            float32_t tolerance = MODEL_ANIMATION_RIG_MATRIX_ABSOLUTE_TOLERANCE + MODEL_ANIMATION_RIG_MATRIX_RELATIVE_TOLERANCE * std::max(float_abs(source_value), float_abs(round_trip_value));

            if (difference > tolerance) {
                throw ModelAnimationConverterException("Rest matrix TRS round-trip failed", column, row, context, source_value, round_trip_value, difference, tolerance);
            }
        }
    }

    return result;
}

static auto ComposeModelAnimationTransform(const ozz::math::Transform& transform) -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    vec3 translation {transform.translation.x, transform.translation.y, transform.translation.z};
    quaternion rotation {transform.rotation.w, transform.rotation.x, transform.rotation.y, transform.rotation.z};
    vec3 scale {transform.scale.x, transform.scale.y, transform.scale.z};
    return glm::translate(mat44 {1.0f}, translation) * glm::mat4_cast(rotation) * glm::scale(mat44 {1.0f}, scale);
}

static auto BuildModelAnimationClipArtifact(const ModelAnimationSource& animation, const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig, uint64_t rig_signature, uint64_t cache_signature, bool nearest_sampling) -> ModelAnimationClipArtifact
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationClipArtifact result;
    result.SourceFile = animation.FileName;
    result.ClipName = animation.Name;
    ozz::animation::offline::RawAnimation raw_animation = BuildModelAnimationRawAnimation(animation, compatibility_report, canonical_rig, result.JointRemap, nearest_sampling);
    uint64_t source_signature = HashModelAnimationSource(raw_animation, result.JointRemap);

    ozz::animation::offline::AnimationBuilder builder;
    ozz::unique_ptr<ozz::animation::Animation> runtime_animation = builder(raw_animation);

    if (!runtime_animation) {
        throw ModelAnimationConverterException("Ozz AnimationBuilder failed", animation.FileName, animation.Name, raw_animation.tracks.size());
    }

    result.AnimationMetadata = ModelAnimationArchiveMetadata {
        ModelAnimationArchiveKind::Animation,
        MODEL_ANIMATION_ARCHIVE_SUPPORTED_FLAGS,
        rig_signature,
        source_signature,
        cache_signature,
        animation.FileName,
        animation.Name,
    };

    string animation_context = strex("animation '{}#{}'", animation.FileName, animation.Name);
    vector<uint8_t> animation_payload = SerializeModelAnimationObject(*runtime_animation, animation_context);
    result.AnimationArchive = WrapAndValidateModelAnimationArchive(result.AnimationMetadata, animation_payload);
    ModelAnimationArchive loaded_animation_archive = ReadModelAnimationArchive(result.AnimationArchive, result.AnimationMetadata);
    ozz::animation::Animation loaded_animation = DeserializeModelAnimationObject<ozz::animation::Animation>(loaded_animation_archive.Payload, animation_context);
    ValidateModelAnimationRoundTrip(loaded_animation, animation, compatibility_report.CanonicalJoints.size(), animation_context);

    result.JointRemapMetadata = ModelAnimationArchiveMetadata {
        ModelAnimationArchiveKind::JointRemap,
        MODEL_ANIMATION_ARCHIVE_SUPPORTED_FLAGS,
        rig_signature,
        HashModelAnimationJointRemap(result.JointRemap),
        cache_signature,
        animation.FileName,
        strex("{}:JointRemap", animation.Name),
    };

    vector<uint8_t> remap_payload = WriteModelAnimationJointRemapPayload(result.JointRemap, animation_context);
    result.JointRemapArchive = WrapAndValidateModelAnimationArchive(result.JointRemapMetadata, remap_payload);
    ModelAnimationArchive loaded_remap_archive = ReadModelAnimationArchive(result.JointRemapArchive, result.JointRemapMetadata);
    ModelAnimationJointRemap loaded_remap = ReadModelAnimationJointRemapPayload(loaded_remap_archive.Payload, animation_context);

    if (loaded_remap.Duration != result.JointRemap.Duration || loaded_remap.SourceToCanonicalJointIndices != result.JointRemap.SourceToCanonicalJointIndices || loaded_remap.CanonicalJointPresent != result.JointRemap.CanonicalJointPresent || loaded_remap.NearestSampleTimes != result.JointRemap.NearestSampleTimes) {
        throw ModelAnimationConverterException("Joint-remap round-trip mismatch", animation.FileName, animation.Name);
    }

    return result;
}

static auto BuildModelAnimationRawAnimation(const ModelAnimationSource& animation, const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig, ModelAnimationJointRemap& joint_remap, bool nearest_sampling) -> ozz::animation::offline::RawAnimation
{
    FO_STACK_TRACE_ENTRY();

    if (animation.FileName.empty() || animation.Name.empty() || animation.FileName.find('\0') != string::npos || animation.Name.find('\0') != string::npos) {
        throw ModelAnimationConverterException("Animation identity is invalid for ozz conversion", animation.FileName, animation.Name);
    }
    if (!std::isfinite(animation.Duration) || animation.Duration <= 0.0f || !std::isfinite(1.0f / animation.Duration)) {
        throw ModelAnimationConverterException("Animation has invalid duration for ozz conversion", animation.FileName, animation.Name, animation.Duration);
    }

    size_t canonical_joint_count = compatibility_report.CanonicalJoints.size();
    ModelAnimationJointRemap built_remap;
    built_remap.Duration = animation.Duration;
    built_remap.CanonicalJointCount = numeric_cast<uint32_t>(canonical_joint_count);
    built_remap.SourceToCanonicalJointIndices.resize(animation.Joints.size());
    built_remap.CanonicalJointPresent.resize(canonical_joint_count, uint8_t {0});
    const string& canonical_root = compatibility_report.CanonicalJoints.front().Hierarchy.front();

    if (nearest_sampling) {
        built_remap.NearestSampleTimes = BuildModelAnimationNearestTimeline(animation);
    }

    ozz::animation::offline::RawAnimation result;
    result.name = animation.Name.c_str();
    result.duration = animation.Duration;
    result.tracks.resize(canonical_joint_count);

    for (size_t i = 0; i < canonical_joint_count; i++) {
        FillModelAnimationFallbackTrack(canonical_rig.RestTransforms[i], animation.Duration, strex("'{}#{}' fallback joint '{}'", animation.FileName, animation.Name, FormatModelAnimationHierarchy(compatibility_report.CanonicalJoints[i].Hierarchy)), result.tracks[i]);
    }

    for (size_t source_index = 0; source_index < animation.Joints.size(); source_index++) {
        const ModelAnimationJointSource& source_joint = animation.Joints[source_index];

        if (source_joint.Hierarchy.size() == 1 && source_joint.Hierarchy.front() != canonical_root) {
            throw ModelAnimationConverterException("Animation has an animated aliased root that legacy name binding cannot safely map to the canonical root", animation.FileName, animation.Name, source_joint.Hierarchy.front(), canonical_root);
        }

        vector<string> normalized_hierarchy = NormalizeModelAnimationHierarchy(source_joint.Hierarchy, canonical_root);
        auto canonical_it = canonical_rig.JointByHierarchy.find(normalized_hierarchy);

        if (canonical_it == canonical_rig.JointByHierarchy.end()) {
            throw ModelAnimationConverterException("Animation output does not map to canonical hierarchy", animation.FileName, animation.Name, source_joint.OutputName, FormatModelAnimationHierarchy(normalized_hierarchy));
        }

        uint32_t canonical_index = canonical_it->second;

        if (built_remap.CanonicalJointPresent[canonical_index] != 0) {
            throw ModelAnimationConverterException("Animation maps more than one output to a canonical joint", animation.FileName, animation.Name, FormatModelAnimationHierarchy(normalized_hierarchy));
        }

        built_remap.SourceToCanonicalJointIndices[source_index] = canonical_index;
        built_remap.CanonicalJointPresent[canonical_index] = uint8_t {1};
        FillModelAnimationAuthoredTrack(source_joint, animation.Duration, nearest_sampling, strex("'{}#{}' output '{}'", animation.FileName, animation.Name, FormatModelAnimationHierarchy(normalized_hierarchy)), result.tracks[canonical_index]);
    }

    if (!result.Validate()) {
        throw ModelAnimationConverterException("Raw ozz animation validation failed", animation.FileName, animation.Name);
    }

    set<float32_t> time_points;

    for (const ozz::animation::offline::RawAnimation::JointTrack& track : result.tracks) {
        for (const auto& key : track.translations) {
            time_points.emplace(key.time);
        }
        for (const auto& key : track.rotations) {
            time_points.emplace(key.time);
        }
        for (const auto& key : track.scales) {
            time_points.emplace(key.time);
        }
    }

    if (time_points.size() > std::numeric_limits<uint16_t>::max()) {
        throw ModelAnimationConverterException("Animation has more unique ozz time points than the maximum", animation.FileName, animation.Name, time_points.size(), std::numeric_limits<uint16_t>::max());
    }

    joint_remap = std::move(built_remap);
    return result;
}

static void FillModelAnimationFallbackTrack(const ozz::math::Transform& transform, float32_t duration, string_view context, ozz::animation::offline::RawAnimation::JointTrack& track)
{
    FO_STACK_TRACE_ENTRY();

    if (float_abs(transform.translation.x) > MODEL_ANIMATION_HALF_MAX || float_abs(transform.translation.y) > MODEL_ANIMATION_HALF_MAX || float_abs(transform.translation.z) > MODEL_ANIMATION_HALF_MAX || float_abs(transform.scale.x) > MODEL_ANIMATION_HALF_MAX || float_abs(transform.scale.y) > MODEL_ANIMATION_HALF_MAX || float_abs(transform.scale.z) > MODEL_ANIMATION_HALF_MAX) {
        throw ModelAnimationConverterException("Fallback transform exceeds ozz FP16 range", context, MODEL_ANIMATION_HALF_MAX);
    }

    track.translations = {
        {0.0f, transform.translation},
        {duration, transform.translation},
    };
    track.rotations = {
        {0.0f, transform.rotation},
        {duration, transform.rotation},
    };
    track.scales = {
        {0.0f, transform.scale},
        {duration, transform.scale},
    };
}

static void FillModelAnimationAuthoredTrack(const ModelAnimationJointSource& source, float32_t duration, bool nearest_sampling, string_view animation_context, ozz::animation::offline::RawAnimation::JointTrack& track)
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationVec3Track(source.Translation, strex("{} translation", animation_context), true);
    ValidateModelAnimationQuaternionTrack(source.Rotation, strex("{} rotation", animation_context));
    ValidateModelAnimationVec3Track(source.Scale, strex("{} scale", animation_context), true);

    if (!nearest_sampling) {
        auto validate_smooth_window_start = [&](const vector<float32_t>& times, string_view track_name) {
            if (times.size() > 1 && times.front() > 0.0f && times.front() <= duration) {
                throw ModelAnimationConverterException("Smooth track starts inside the playback window where legacy sampling has a discontinuity that ozz interpolation cannot preserve", track_name, animation_context, times.front(), duration);
            }
        };
        validate_smooth_window_start(source.Translation.Times, "translation");
        validate_smooth_window_start(source.Rotation.Times, "rotation");
        validate_smooth_window_start(source.Scale.Times, "scale");
    }

    track.translations.clear();
    track.rotations.clear();
    track.scales.clear();

    vec3 translation_start = EvaluateModelAnimationLegacyTrack(source.Translation, 0.0f, nearest_sampling, strex("{} translation", animation_context));
    vec3 translation_end = EvaluateModelAnimationLegacyTrack(source.Translation, duration, nearest_sampling, strex("{} translation", animation_context));
    track.translations.push_back({0.0f, ozz::math::Float3 {translation_start.x, translation_start.y, translation_start.z}});

    for (size_t i = 0; i < source.Translation.Times.size(); i++) {
        float32_t time = source.Translation.Times[i];

        if (time > 0.0f && time < duration) {
            const vec3& value = source.Translation.Values[i];
            track.translations.push_back({time, ozz::math::Float3 {value.x, value.y, value.z}});
        }
    }

    track.translations.push_back({duration, ozz::math::Float3 {translation_end.x, translation_end.y, translation_end.z}});

    quaternion rotation_start = EvaluateModelAnimationLegacyTrack(source.Rotation, 0.0f, nearest_sampling, strex("{} rotation", animation_context));
    quaternion rotation_end = EvaluateModelAnimationLegacyTrack(source.Rotation, duration, nearest_sampling, strex("{} rotation", animation_context));
    track.rotations.push_back({0.0f, ozz::math::Quaternion {rotation_start.x, rotation_start.y, rotation_start.z, rotation_start.w}});

    for (size_t i = 0; i < source.Rotation.Times.size(); i++) {
        float32_t time = source.Rotation.Times[i];

        if (time > 0.0f && time < duration) {
            quaternion value = glm::normalize(source.Rotation.Values[i]);
            track.rotations.push_back({time, ozz::math::Quaternion {value.x, value.y, value.z, value.w}});
        }
    }

    track.rotations.push_back({duration, ozz::math::Quaternion {rotation_end.x, rotation_end.y, rotation_end.z, rotation_end.w}});

    if (!nearest_sampling) {
        RefineModelAnimationRotationTrack(track.rotations, strex("{} rotation", animation_context));
    }

    vec3 scale_start = EvaluateModelAnimationLegacyTrack(source.Scale, 0.0f, nearest_sampling, strex("{} scale", animation_context));
    vec3 scale_end = EvaluateModelAnimationLegacyTrack(source.Scale, duration, nearest_sampling, strex("{} scale", animation_context));
    track.scales.push_back({0.0f, ozz::math::Float3 {scale_start.x, scale_start.y, scale_start.z}});

    for (size_t i = 0; i < source.Scale.Times.size(); i++) {
        float32_t time = source.Scale.Times[i];

        if (time > 0.0f && time < duration) {
            const vec3& value = source.Scale.Values[i];
            track.scales.push_back({time, ozz::math::Float3 {value.x, value.y, value.z}});
        }
    }

    track.scales.push_back({duration, ozz::math::Float3 {scale_end.x, scale_end.y, scale_end.z}});
}

static void RefineModelAnimationRotationTrack(ozz::animation::offline::RawAnimation::JointTrack::Rotations& rotations, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (rotations.size() < 2) {
        throw ModelAnimationConverterException("Ozz rotation refinement requires at least two keys", context);
    }

    ozz::animation::offline::RawAnimation::JointTrack::Rotations original = rotations;
    ozz::animation::offline::RawAnimation::JointTrack::Rotations refined;
    refined.reserve(original.size());
    refined.emplace_back(original.front());

    for (size_t i = 0; i + 1 < original.size(); i++) {
        AppendModelAnimationRefinedRotationSegment(original[i], original[i + 1], 0, context, refined);
    }

    rotations = std::move(refined);
}

static void AppendModelAnimationRefinedRotationSegment(const ozz::animation::offline::RawAnimation::RotationKey& first, const ozz::animation::offline::RawAnimation::RotationKey& second, size_t depth, string_view context, ozz::animation::offline::RawAnimation::JointTrack::Rotations& result)
{
    FO_STACK_TRACE_ENTRY();

    float32_t max_error = std::max({
        GetModelAnimationNlerpError(first.value, second.value, 0.211324865f),
        GetModelAnimationNlerpError(first.value, second.value, 0.25f),
        GetModelAnimationNlerpError(first.value, second.value, 0.75f),
        GetModelAnimationNlerpError(first.value, second.value, 0.788675135f),
    });

    if (max_error <= MODEL_ANIMATION_ROTATION_REFINEMENT_ERROR_RADIANS) {
        result.emplace_back(second);
        return;
    }
    if (depth >= MODEL_ANIMATION_ROTATION_SUBDIVISION_MAX_DEPTH) {
        throw ModelAnimationConverterException("Ozz NLERP refinement still has radian error after maximum subdivisions", context, max_error, depth);
    }

    float32_t middle_time = first.time + (second.time - first.time) * 0.5f;

    if (!(middle_time > first.time && middle_time < second.time)) {
        throw ModelAnimationConverterException("Ozz NLERP refinement time collapsed between keys", first.time, second.time, context);
    }

    quaternion first_rotation {first.value.w, first.value.x, first.value.y, first.value.z};
    quaternion second_rotation {second.value.w, second.value.x, second.value.y, second.value.z};
    quaternion middle_rotation = glm::normalize(glm::slerp(first_rotation, second_rotation, 0.5f));
    ozz::animation::offline::RawAnimation::RotationKey middle {
        middle_time,
        ozz::math::Quaternion {middle_rotation.x, middle_rotation.y, middle_rotation.z, middle_rotation.w},
    };

    AppendModelAnimationRefinedRotationSegment(first, middle, depth + 1, context, result);
    AppendModelAnimationRefinedRotationSegment(middle, second, depth + 1, context, result);
}

static auto GetModelAnimationNlerpError(const ozz::math::Quaternion& first, const ozz::math::Quaternion& second, float32_t factor) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    using DoubleQuaternion = glm::qua<float64_t, glm::defaultp>;
    DoubleQuaternion first_rotation = glm::normalize(DoubleQuaternion {numeric_cast<float64_t>(first.w), numeric_cast<float64_t>(first.x), numeric_cast<float64_t>(first.y), numeric_cast<float64_t>(first.z)});
    DoubleQuaternion second_rotation = glm::normalize(DoubleQuaternion {numeric_cast<float64_t>(second.w), numeric_cast<float64_t>(second.x), numeric_cast<float64_t>(second.y), numeric_cast<float64_t>(second.z)});

    if (glm::dot(first_rotation, second_rotation) < 0.0f) {
        second_rotation = -second_rotation;
    }

    float64_t double_factor = numeric_cast<float64_t>(factor);
    DoubleQuaternion legacy_rotation = glm::normalize(glm::slerp(first_rotation, second_rotation, double_factor));
    DoubleQuaternion ozz_rotation = glm::normalize(first_rotation * (1.0 - double_factor) + second_rotation * double_factor);
    float64_t cosine = std::clamp(std::abs(glm::dot(legacy_rotation, ozz_rotation)), 0.0, 1.0);
    return numeric_cast<float32_t>(2.0 * std::acos(cosine));
}

static auto BuildModelAnimationNearestTimeline(const ModelAnimationSource& animation) -> vector<float32_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<float32_t> result;

    for (const ModelAnimationJointSource& joint : animation.Joints) {
        ValidateModelAnimationVec3Track(joint.Translation, strex("'{}#{}' output '{}' translation", animation.FileName, animation.Name, joint.OutputName), true);
        ValidateModelAnimationQuaternionTrack(joint.Rotation, strex("'{}#{}' output '{}' rotation", animation.FileName, animation.Name, joint.OutputName));
        ValidateModelAnimationVec3Track(joint.Scale, strex("'{}#{}' output '{}' scale", animation.FileName, animation.Name, joint.OutputName), true);

        auto validate_boundary_keys = [&](const vector<float32_t>& times, string_view track_name) {
            if (!std::binary_search(times.begin(), times.end(), 0.0f) || !std::binary_search(times.begin(), times.end(), animation.Duration)) {
                throw ModelAnimationConverterException("Nearest-sampled animation track must contain exact keys at playback boundaries 0 and duration", animation.FileName, animation.Name, joint.OutputName, track_name, animation.Duration);
            }
        };
        validate_boundary_keys(joint.Translation.Times, "translation");
        validate_boundary_keys(joint.Rotation.Times, "rotation");
        validate_boundary_keys(joint.Scale.Times, "scale");

        array<vector<float32_t>, 3> timelines {
            CropModelAnimationTimeline(joint.Translation.Times, animation.Duration),
            CropModelAnimationTimeline(joint.Rotation.Times, animation.Duration),
            CropModelAnimationTimeline(joint.Scale.Times, animation.Duration),
        };

        for (const vector<float32_t>& timeline : timelines) {
            if (result.empty()) {
                result = timeline;
            }
            else if (timeline != result) {
                throw ModelAnimationConverterException("Nearest-sampled animation does not use one shared S/R/T timeline at output", animation.FileName, animation.Name, joint.OutputName);
            }
        }
    }

    if (result.empty()) {
        result = {0.0f, animation.Duration};
    }

    return result;
}

static auto CropModelAnimationTimeline(const vector<float32_t>& times, float32_t duration) -> vector<float32_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<float32_t> result;
    result.reserve(times.size() + 2);
    result.emplace_back(0.0f);

    for (float32_t time : times) {
        if (time > 0.0f && time < duration) {
            result.emplace_back(time);
        }
    }

    result.emplace_back(duration);
    return result;
}

static auto EvaluateModelAnimationLegacyTrack(const ModelAnimationVec3Track& track, float32_t time, bool nearest_sampling, string_view context) -> vec3
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationVec3Track(track, context, true);

    for (size_t i = 0; i < track.Times.size(); i++) {
        if (i + 1 < track.Times.size() && time >= track.Times[i] && time < track.Times[i + 1]) {
            float32_t factor = (time - track.Times[i]) / (track.Times[i + 1] - track.Times[i]);

            if (nearest_sampling) {
                return factor >= 0.5f ? track.Values[i + 1] : track.Values[i];
            }

            return track.Values[i] + (track.Values[i + 1] - track.Values[i]) * factor;
        }
        if (i + 1 == track.Times.size()) {
            return track.Values[i];
        }
    }

    throw ModelAnimationConverterException("Can't evaluate empty legacy vector track", context);
}

static auto EvaluateModelAnimationLegacyTrack(const ModelAnimationQuaternionTrack& track, float32_t time, bool nearest_sampling, string_view context) -> quaternion
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationQuaternionTrack(track, context);

    for (size_t i = 0; i < track.Times.size(); i++) {
        if (i + 1 < track.Times.size() && time >= track.Times[i] && time < track.Times[i + 1]) {
            float32_t factor = (time - track.Times[i]) / (track.Times[i + 1] - track.Times[i]);
            quaternion value = nearest_sampling && factor < 0.5f ? track.Values[i] : (nearest_sampling ? track.Values[i + 1] : glm::slerp(track.Values[i], track.Values[i + 1], factor));
            return glm::normalize(value);
        }
        if (i + 1 == track.Times.size()) {
            return glm::normalize(track.Values[i]);
        }
    }

    throw ModelAnimationConverterException("Can't evaluate empty legacy rotation track", context);
}

static void ValidateModelAnimationVec3Track(const ModelAnimationVec3Track& track, string_view context, bool half_range)
{
    FO_STACK_TRACE_ENTRY();

    if (track.Times.empty() || track.Times.size() != track.Values.size()) {
        throw ModelAnimationConverterException("Invalid vector track sizes", context, track.Times.size(), track.Values.size());
    }

    for (size_t i = 0; i < track.Times.size(); i++) {
        if (!std::isfinite(track.Times[i]) || (i != 0 && track.Times[i] <= track.Times[i - 1])) {
            throw ModelAnimationConverterException("Invalid vector key time", track.Times[i], i, context);
        }

        const vec3& value = track.Values[i];

        if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
            throw ModelAnimationConverterException("Non-finite vector key", i, context);
        }
        if (half_range && (float_abs(value.x) > MODEL_ANIMATION_HALF_MAX || float_abs(value.y) > MODEL_ANIMATION_HALF_MAX || float_abs(value.z) > MODEL_ANIMATION_HALF_MAX)) {
            throw ModelAnimationConverterException("Vector key exceeds ozz FP16 range", i, context, MODEL_ANIMATION_HALF_MAX, value.x, value.y, value.z);
        }
    }
}

static void ValidateModelAnimationQuaternionTrack(const ModelAnimationQuaternionTrack& track, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (track.Times.empty() || track.Times.size() != track.Values.size()) {
        throw ModelAnimationConverterException("Invalid rotation track sizes", context, track.Times.size(), track.Values.size());
    }

    for (size_t i = 0; i < track.Times.size(); i++) {
        if (!std::isfinite(track.Times[i]) || (i != 0 && track.Times[i] <= track.Times[i - 1])) {
            throw ModelAnimationConverterException("Invalid rotation key time", track.Times[i], i, context);
        }

        const quaternion& value = track.Values[i];

        if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) || !std::isfinite(value.w)) {
            throw ModelAnimationConverterException("Non-finite rotation key", i, context);
        }

        float32_t norm_squared = glm::dot(value, value);

        if (!std::isfinite(norm_squared) || norm_squared <= std::numeric_limits<float32_t>::epsilon()) {
            throw ModelAnimationConverterException("Zero rotation key", i, context);
        }
        if (float_abs(norm_squared - 1.0f) > MODEL_ANIMATION_QUATERNION_NORM_TOLERANCE) {
            throw ModelAnimationConverterException("Non-unit rotation key", i, context, norm_squared);
        }
    }
}

static auto NormalizeModelAnimationHierarchy(const vector<string>& hierarchy, string_view canonical_root) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    if (hierarchy.empty()) {
        throw ModelAnimationConverterException("Can't normalize an empty animation hierarchy for ozz conversion");
    }

    vector<string> result = hierarchy;
    result.front() = canonical_root;
    return result;
}

static auto FormatModelAnimationHierarchy(const vector<string>& hierarchy) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;

    for (size_t i = 0; i < hierarchy.size(); i++) {
        if (i != 0) {
            result += '/';
        }

        result += !hierarchy[i].empty() ? hierarchy[i] : (i == 0 ? "<empty-root>" : "<empty>");
    }

    return result;
}

static void ValidateModelAnimationJointName(string_view name, string_view context, bool root)
{
    FO_STACK_TRACE_ENTRY();

    if (!root && name.empty()) {
        throw ModelAnimationConverterException("Empty non-root ozz joint name", context);
    }
    if (name.find('\0') != string_view::npos) {
        throw ModelAnimationConverterException("Embedded NUL in ozz joint name", context);
    }
    if (!strvex(name).is_valid_utf8()) {
        throw ModelAnimationConverterException("Invalid UTF-8 ozz joint name", context);
    }
}

template<typename T>
static auto SerializeModelAnimationObject(const T& object, string_view context) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    ozz::io::MemoryStream stream;

    {
        ozz::io::OArchive archive {&stream, ozz::kLittleEndian};
        archive << object;
    }

    if (stream.Size() == 0 || stream.Size() > numeric_cast<size_t>(std::numeric_limits<int>::max())) {
        throw ModelAnimationConverterException("Invalid serialized ozz object size", stream.Size(), context);
    }
    if (stream.Seek(0, ozz::io::Stream::kSet) != 0) {
        throw ModelAnimationConverterException("Can't rewind serialized ozz object", context);
    }

    vector<uint8_t> result(stream.Size());

    if (stream.Read(result.data(), result.size()) != result.size()) {
        throw ModelAnimationConverterException("Can't read complete serialized ozz object", context);
    }

    return result;
}

template<typename T>
static auto DeserializeModelAnimationObject(const_span<uint8_t> payload, string_view context) -> T
{
    FO_STACK_TRACE_ENTRY();

    if (payload.empty() || payload.size() > numeric_cast<size_t>(std::numeric_limits<int>::max())) {
        throw ModelAnimationConverterException("Invalid serialized ozz payload size", payload.size(), context);
    }

    ozz::io::MemoryStream stream;

    if (stream.Write(payload.data(), payload.size()) != payload.size() || stream.Seek(0, ozz::io::Stream::kSet) != 0) {
        throw ModelAnimationConverterException("Can't stage serialized ozz payload", context);
    }

    T result;

    {
        ozz::io::IArchive archive {&stream};

        if (!archive.TestTag<T>()) {
            throw ModelAnimationConverterException("Serialized ozz payload has the wrong type tag", context);
        }

        archive >> result;
    }

    if (stream.Tell() < 0 || numeric_cast<size_t>(stream.Tell()) != payload.size()) {
        throw ModelAnimationConverterException("Serialized ozz payload was not consumed exactly", context, stream.Tell(), payload.size());
    }

    return result;
}

static auto WrapAndValidateModelAnimationArchive(const ModelAnimationArchiveMetadata& metadata, const_span<uint8_t> payload) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    try {
        vector<uint8_t> result = WriteModelAnimationArchive(metadata, payload);
        (void)ReadModelAnimationArchive(result, metadata);
        return result;
    }
    catch (const ModelAnimationArchiveException& ex) {
        throw ModelAnimationConverterException("LF model animation archive validation failed", metadata.SourceAsset, metadata.ObjectName, ex.what());
    }
}

static void ValidateModelAnimationSkeletonRoundTrip(const ozz::animation::Skeleton& skeleton, const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (skeleton.num_joints() != numeric_cast<int>(compatibility_report.CanonicalJoints.size())) {
        throw ModelAnimationConverterException("Canonical ozz skeleton joint-count mismatch", context, compatibility_report.CanonicalJoints.size(), skeleton.num_joints());
    }

    auto names = skeleton.joint_names();
    auto parents = skeleton.joint_parents();

    for (size_t i = 0; i < compatibility_report.CanonicalJoints.size(); i++) {
        if (string_view {names[i]} != compatibility_report.CanonicalJoints[i].Name) {
            throw ModelAnimationConverterException("Canonical ozz skeleton name mismatch at joint", context, i, compatibility_report.CanonicalJoints[i].Name, names[i]);
        }
        if (parents[i] != canonical_rig.Parents[i]) {
            throw ModelAnimationConverterException("Canonical ozz skeleton parent mismatch at joint", context, i, canonical_rig.Parents[i], parents[i]);
        }

        ozz::math::Transform actual_rest = ozz::animation::GetJointLocalRestPose(skeleton, numeric_cast<int>(i));
        const ozz::math::Transform& expected_rest = canonical_rig.RestTransforms[i];
        float32_t rotation_dot = actual_rest.rotation.x * expected_rest.rotation.x + actual_rest.rotation.y * expected_rest.rotation.y + actual_rest.rotation.z * expected_rest.rotation.z + actual_rest.rotation.w * expected_rest.rotation.w;

        if (!is_float_equal(actual_rest.translation.x, expected_rest.translation.x, 1.0e-6f) || !is_float_equal(actual_rest.translation.y, expected_rest.translation.y, 1.0e-6f) || !is_float_equal(actual_rest.translation.z, expected_rest.translation.z, 1.0e-6f) || !is_float_equal(float_abs(rotation_dot), 1.0f, 1.0e-6f) || !is_float_equal(actual_rest.scale.x, expected_rest.scale.x, 1.0e-6f) || !is_float_equal(actual_rest.scale.y, expected_rest.scale.y, 1.0e-6f) || !is_float_equal(actual_rest.scale.z, expected_rest.scale.z, 1.0e-6f)) {
            throw ModelAnimationConverterException("Canonical ozz skeleton rest-pose mismatch at joint", context, i, compatibility_report.CanonicalJoints[i].Name);
        }
    }
}

static void ValidateModelAnimationRoundTrip(const ozz::animation::Animation& animation, const ModelAnimationSource& source, size_t canonical_joint_count, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (animation.num_tracks() != numeric_cast<int>(canonical_joint_count)) {
        throw ModelAnimationConverterException("Ozz animation track-count mismatch", context, canonical_joint_count, animation.num_tracks());
    }
    if (string_view {animation.name()} != source.Name) {
        throw ModelAnimationConverterException("Ozz animation name mismatch", context, source.Name, animation.name());
    }
    if (!is_float_equal(animation.duration(), source.Duration, 1.0e-6f)) {
        throw ModelAnimationConverterException("Ozz animation duration mismatch", context, source.Duration, animation.duration());
    }

    auto timepoints = animation.timepoints();

    if (timepoints.size() < 2 || timepoints.front() != 0.0f || !is_float_equal(timepoints.back(), 1.0f, 1.0e-6f)) {
        throw ModelAnimationConverterException("Ozz animation timepoint endpoints are invalid", context, timepoints.size(), timepoints.empty() ? 0.0f : timepoints.front(), timepoints.empty() ? 0.0f : timepoints.back());
    }

    for (size_t i = 0; i < timepoints.size(); i++) {
        float32_t ratio = timepoints[i];

        if (!std::isfinite(ratio) || ratio < 0.0f || ratio > 1.0f + 1.0e-6f || (i != 0 && ratio <= timepoints[i - 1])) {
            throw ModelAnimationConverterException("Invalid ozz animation timepoint ratio", ratio, i, context);
        }
    }
}

static auto HashModelAnimationRig(const ModelSkeletonCompatibilityReport& compatibility_report, const ModelAnimationCanonicalRig& canonical_rig) -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t hash = MODEL_ANIMATION_HASH_OFFSET;
    HashModelAnimationString(hash, "LF canonical ozz rig v1");
    HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(compatibility_report.CanonicalJoints.size()));

    for (size_t i = 0; i < compatibility_report.CanonicalJoints.size(); i++) {
        const ModelSkeletonJoint& joint = compatibility_report.CanonicalJoints[i];
        HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(joint.Hierarchy.size()));

        for (const string& hierarchy_name : joint.Hierarchy) {
            HashModelAnimationString(hash, hierarchy_name);
        }

        HashModelAnimationUnsigned(hash, std::bit_cast<uint16_t>(canonical_rig.Parents[i]));
        const ozz::math::Transform& transform = canonical_rig.RestTransforms[i];
        HashModelAnimationFloat(hash, transform.translation.x);
        HashModelAnimationFloat(hash, transform.translation.y);
        HashModelAnimationFloat(hash, transform.translation.z);
        HashModelAnimationFloat(hash, transform.rotation.x);
        HashModelAnimationFloat(hash, transform.rotation.y);
        HashModelAnimationFloat(hash, transform.rotation.z);
        HashModelAnimationFloat(hash, transform.rotation.w);
        HashModelAnimationFloat(hash, transform.scale.x);
        HashModelAnimationFloat(hash, transform.scale.y);
        HashModelAnimationFloat(hash, transform.scale.z);
        HashModelAnimationByte(hash, canonical_rig.BaseJointPresent[i]);
    }

    return FinishModelAnimationHash(hash);
}

static auto HashModelAnimationBaseSource(const ModelSkeletonSource& base_skeleton, const ModelAnimationJointRemap& remap) -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t hash = MODEL_ANIMATION_HASH_OFFSET;
    HashModelAnimationString(hash, "LF ozz base source v1");
    HashModelAnimationString(hash, base_skeleton.FileName);
    HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(base_skeleton.Joints.size()));

    for (const ModelSkeletonJoint& joint : base_skeleton.Joints) {
        HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(joint.Hierarchy.size()));

        for (const string& hierarchy_name : joint.Hierarchy) {
            HashModelAnimationString(hash, hierarchy_name);
        }
        for (mat44::length_type column = 0; column < 4; column++) {
            for (mat44::length_type row = 0; row < 4; row++) {
                HashModelAnimationFloat(hash, joint.RestLocalTransform[column][row]);
            }
        }
    }

    HashModelAnimationUnsigned(hash, HashModelAnimationJointRemap(remap));
    return FinishModelAnimationHash(hash);
}

static auto HashModelAnimationSource(const ozz::animation::offline::RawAnimation& animation, const ModelAnimationJointRemap& remap) -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t hash = MODEL_ANIMATION_HASH_OFFSET;
    HashModelAnimationString(hash, "LF cropped ozz animation source v1");
    HashModelAnimationString(hash, animation.name.c_str());
    HashModelAnimationFloat(hash, animation.duration);
    HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(animation.tracks.size()));

    for (const ozz::animation::offline::RawAnimation::JointTrack& track : animation.tracks) {
        HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(track.translations.size()));
        for (const auto& key : track.translations) {
            HashModelAnimationFloat(hash, key.time);
            HashModelAnimationFloat(hash, key.value.x);
            HashModelAnimationFloat(hash, key.value.y);
            HashModelAnimationFloat(hash, key.value.z);
        }
        HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(track.rotations.size()));
        for (const auto& key : track.rotations) {
            HashModelAnimationFloat(hash, key.time);
            HashModelAnimationFloat(hash, key.value.x);
            HashModelAnimationFloat(hash, key.value.y);
            HashModelAnimationFloat(hash, key.value.z);
            HashModelAnimationFloat(hash, key.value.w);
        }
        HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(track.scales.size()));
        for (const auto& key : track.scales) {
            HashModelAnimationFloat(hash, key.time);
            HashModelAnimationFloat(hash, key.value.x);
            HashModelAnimationFloat(hash, key.value.y);
            HashModelAnimationFloat(hash, key.value.z);
        }
    }

    HashModelAnimationUnsigned(hash, HashModelAnimationJointRemap(remap));
    return FinishModelAnimationHash(hash);
}

static auto HashModelAnimationJointRemap(const ModelAnimationJointRemap& remap) -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t hash = MODEL_ANIMATION_HASH_OFFSET;
    HashModelAnimationString(hash, "LF ozz joint remap v1");
    HashModelAnimationFloat(hash, remap.Duration);
    HashModelAnimationUnsigned(hash, remap.CanonicalJointCount);
    HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(remap.SourceToCanonicalJointIndices.size()));
    HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(remap.CanonicalJointPresent.size()));
    HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(remap.NearestSampleTimes.size()));

    for (uint32_t canonical_index : remap.SourceToCanonicalJointIndices) {
        HashModelAnimationUnsigned(hash, canonical_index);
    }
    for (uint8_t present : remap.CanonicalJointPresent) {
        HashModelAnimationByte(hash, present);
    }
    for (float32_t time : remap.NearestSampleTimes) {
        HashModelAnimationFloat(hash, time);
    }

    return FinishModelAnimationHash(hash);
}

static auto HashModelAnimationConverterPolicy(bool nearest_sampling) -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t hash = MODEL_ANIMATION_HASH_OFFSET;
    // Stable schema-1 hash domain; its legacy spelling is part of baked cache identity.
    HashModelAnimationString(hash, "LF ModelOzzConverter v1; retention=base-plus-selected-output-ancestors; contributed-rest=identity; crop=legacy-boundaries; rotation-refinement-error-radians=0.000872664626; runtime-rotation-budget-radians=0.001745329252; nearest=shared-timeline-with-boundary-keys; optimizer=off; endian=little");
    HashModelAnimationString(hash, MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION);
    HashModelAnimationByte(hash, nearest_sampling ? uint8_t {1} : uint8_t {0});
    return FinishModelAnimationHash(hash);
}

static void HashModelAnimationByte(uint64_t& hash, uint8_t value)
{
    FO_NO_STACK_TRACE_ENTRY();

    hash ^= value;
    hash *= MODEL_ANIMATION_HASH_PRIME;
}

template<typename T>
static void HashModelAnimationUnsigned(uint64_t& hash, T value)
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(std::is_unsigned_v<T>);

    for (size_t i = 0; i < sizeof(T); i++) {
        HashModelAnimationByte(hash, static_cast<uint8_t>((value >> (i * 8)) & static_cast<T>(0xFF)));
    }
}

static void HashModelAnimationFloat(uint64_t& hash, float32_t value)
{
    FO_NO_STACK_TRACE_ENTRY();

    HashModelAnimationUnsigned(hash, std::bit_cast<uint32_t>(value));
}

static void HashModelAnimationString(uint64_t& hash, string_view value)
{
    FO_NO_STACK_TRACE_ENTRY();

    HashModelAnimationUnsigned(hash, numeric_cast<uint64_t>(value.size()));

    for (char value_char : value) {
        HashModelAnimationByte(hash, static_cast<uint8_t>(value_char));
    }
}

static auto FinishModelAnimationHash(uint64_t hash) -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return hash != 0 ? hash : uint64_t {1};
}

FO_END_NAMESPACE

#endif
