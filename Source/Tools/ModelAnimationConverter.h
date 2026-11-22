// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

#include "ModelAnimationData.h"
#include "ModelSourceLoader.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ModelSkeletonCompatibilityException);

struct ModelSkeletonJointContribution
{
    string FileName {};
    string ClipName {};
    ModelSkeletonJoint Joint {};
};

struct ModelSkeletonRootAlias
{
    string FileName {};
    string ClipName {};
    string SourceRoot {};
    string CanonicalRoot {};
};

struct ModelSkeletonRestPoseDivergence
{
    string CanonicalSource {};
    string FileName {};
    string ClipName {};
    vector<string> Hierarchy {};
    float32_t MaxDifference {};
};

struct ModelSkeletonAnimationDataIssue
{
    string FileName {};
    string ClipName {};
    string JointName {};
    vector<string> Hierarchy {};
    size_t NonFiniteScaleKeys {};
    size_t NonFiniteRotationKeys {};
    size_t ZeroRotationKeys {};
    size_t NonFiniteTranslationKeys {};
};

struct ModelSkeletonCompatibilityReport
{
    string BaseFile {};
    vector<ModelSkeletonJoint> CanonicalJoints {};
    vector<ModelSkeletonJointContribution> ContributedJoints {};
    vector<ModelSkeletonRootAlias> RootAliases {};
    vector<ModelSkeletonRestPoseDivergence> RestPoseDivergences {};
    vector<ModelSkeletonAnimationDataIssue> AnimationDataIssues {};
};

auto BuildModelSkeletonCompatibilityReport(const ModelSkeletonSource& base_skeleton, const_span<ModelSkeletonClipSource> clips) -> ModelSkeletonCompatibilityReport;
auto FormatModelSkeletonCompatibilityReport(const ModelSkeletonCompatibilityReport& report) -> string;

FO_DECLARE_EXCEPTION(ModelAnimationConverterException);

struct ModelAnimationClipArtifact
{
    string SourceFile {};
    string ClipName {};
    ModelAnimationJointRemap JointRemap {};
    ModelAnimationArchiveMetadata AnimationMetadata {};
    vector<uint8_t> AnimationArchive {};
    ModelAnimationArchiveMetadata JointRemapMetadata {};
    vector<uint8_t> JointRemapArchive {};
};

struct ModelAnimationRigArtifacts
{
    uint64_t RigSignature {};
    uint64_t CacheSignature {};
    ModelAnimationJointRemap BaseJointRemap {};
    ModelAnimationArchiveMetadata SkeletonMetadata {};
    vector<uint8_t> SkeletonArchive {};
    ModelAnimationArchiveMetadata BaseJointRemapMetadata {};
    vector<uint8_t> BaseJointRemapArchive {};
    vector<ModelAnimationClipArtifact> Clips {};
};

struct ModelAnimationRigBindingSource
{
    int32_t StateAnim {};
    int32_t ActionAnim {};
    string SourceFile {};
    string ClipName {};
    bool Reversed {};
};

// Every archive is read back before returning, so BuildModelAnimationRigData starts from artifacts that are
// already proven loadable
auto BuildModelAnimationRigArtifacts(string_view model_description, const ModelSkeletonSource& base_skeleton, const ModelSkeletonCompatibilityReport& compatibility_report, const_span<ModelAnimationSource> animations, bool nearest_sampling) -> ModelAnimationRigArtifacts;
auto BuildModelAnimationRigData(ModelAnimationRigArtifacts artifacts, const_span<ModelAnimationRigBindingSource> bindings) -> ModelAnimationRigData;

FO_END_NAMESPACE

#endif
