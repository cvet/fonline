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

[[nodiscard]] auto BuildModelSkeletonCompatibilityReport(const ModelSkeletonSource& base_skeleton, const_span<ModelSkeletonClipSource> clips) -> ModelSkeletonCompatibilityReport;
[[nodiscard]] auto FormatModelSkeletonCompatibilityReport(const ModelSkeletonCompatibilityReport& report) -> string;

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

// Builds canonical runtime animation objects, wraps them in LF archives, and
// reads every archive back before returning. BuildModelAnimationRigData then
// turns these validated artifacts into the production model-description payload.
[[nodiscard]] auto BuildModelAnimationRigArtifacts(string_view model_description, const ModelSkeletonSource& base_skeleton, const ModelSkeletonCompatibilityReport& compatibility_report, const_span<ModelAnimationSource> animations, bool nearest_sampling) -> ModelAnimationRigArtifacts;
[[nodiscard]] auto BuildModelAnimationRigData(ModelAnimationRigArtifacts artifacts, const_span<ModelAnimationRigBindingSource> bindings) -> ModelAnimationRigData;

FO_END_NAMESPACE

#endif
