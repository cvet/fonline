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

#include "catch_amalgamated.hpp"

#include "Common.h"

#if FO_ENABLE_3D

#include "ModelAnimationConverter.h"

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

static auto MakeSkeletonJoint(string_view name, initializer_list<string_view> hierarchy, float32_t translation_x = 0.0f) -> ModelSkeletonJoint
{
    FO_STACK_TRACE_ENTRY();

    ModelSkeletonJoint joint;
    joint.Name = name;

    for (string_view hierarchy_name : hierarchy) {
        joint.Hierarchy.emplace_back(hierarchy_name);
    }

    joint.RestLocalTransform[3][0] = translation_x;
    return joint;
}

TEST_CASE("ModelSkeletonCompatibilityBuildsDeterministicCanonicalJointMap")
{
    ModelSkeletonSource base;
    base.FileName = "Models/Body.fbx";
    base.Joints = {
        MakeSkeletonJoint("Root", {"Root"}),
        MakeSkeletonJoint("Spine", {"Root", "Spine"}),
    };

    ModelSkeletonClipSource weapon_clip;
    weapon_clip.FileName = "Animations/Z_Weapon.fbx";
    weapon_clip.ClipName = "Idle";
    weapon_clip.Joints = {
        MakeSkeletonJoint("Root", {"Root"}),
        MakeSkeletonJoint("Spine", {"Root", "Spine"}),
        MakeSkeletonJoint("Weapon", {"Root", "Spine", "Weapon"}),
    };
    weapon_clip.AnimatedJointHierarchies = {{"Root", "Spine", "Weapon"}};

    ModelSkeletonClipSource pelvis_clip;
    pelvis_clip.FileName = "Animations/A_Pelvis.fbx";
    pelvis_clip.ClipName = "Walk";
    pelvis_clip.Joints = {
        MakeSkeletonJoint("Root", {"Root"}),
        MakeSkeletonJoint("Pelvis", {"Root", "Pelvis"}),
    };
    pelvis_clip.AnimatedJointHierarchies = {{"Root", "Pelvis"}};

    const vector<ModelSkeletonClipSource> forward_clips = {weapon_clip, pelvis_clip};
    const vector<ModelSkeletonClipSource> reverse_clips = {pelvis_clip, weapon_clip};
    const ModelSkeletonCompatibilityReport forward_report = BuildModelSkeletonCompatibilityReport(base, forward_clips);
    const ModelSkeletonCompatibilityReport reverse_report = BuildModelSkeletonCompatibilityReport(base, reverse_clips);

    REQUIRE(forward_report.CanonicalJoints.size() == 4);
    CHECK(forward_report.CanonicalJoints[0].Hierarchy == vector<string> {"Root"});
    CHECK(forward_report.CanonicalJoints[1].Hierarchy == vector<string> {"Root", "Pelvis"});
    CHECK(forward_report.CanonicalJoints[2].Hierarchy == vector<string> {"Root", "Spine"});
    CHECK(forward_report.CanonicalJoints[3].Hierarchy == vector<string> {"Root", "Spine", "Weapon"});

    REQUIRE(forward_report.ContributedJoints.size() == 2);
    CHECK(forward_report.ContributedJoints[0].Joint.Name == "Pelvis");
    CHECK(forward_report.ContributedJoints[1].Joint.Name == "Weapon");
    CHECK(FormatModelSkeletonCompatibilityReport(forward_report) == FormatModelSkeletonCompatibilityReport(reverse_report));
}

TEST_CASE("ModelSkeletonCompatibilityReportsRootAliases")
{
    ModelSkeletonSource base;
    base.FileName = "Models/Body.fbx";
    base.Joints = {
        MakeSkeletonJoint("Root", {"Root"}),
        MakeSkeletonJoint("Spine", {"Root", "Spine"}),
    };

    ModelSkeletonClipSource clip;
    clip.FileName = "Animations/Body.fbx";
    clip.ClipName = "Idle";
    clip.Joints = {
        MakeSkeletonJoint("Armature", {"Armature"}),
        MakeSkeletonJoint("Spine", {"Armature", "Spine"}),
    };
    clip.AnimatedJointHierarchies = {{"Armature", "Spine"}};

    const vector<ModelSkeletonClipSource> clips = {clip};
    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, clips);

    REQUIRE(report.RootAliases.size() == 1);
    CHECK(report.RootAliases.front().SourceRoot == "Armature");
    CHECK(report.RootAliases.front().CanonicalRoot == "Root");
    CHECK(report.ContributedJoints.empty());
}

TEST_CASE("ModelSkeletonCompatibilityUsesDeterministicDepthFirstJointOrder")
{
    ModelSkeletonSource base;
    base.FileName = "Models/Branches.fbx";
    base.Joints = {
        MakeSkeletonJoint("Root", {"Root"}),
        MakeSkeletonJoint("B", {"Root", "B"}),
    };

    ModelSkeletonClipSource clip;
    clip.FileName = "Animations/Branches.fbx";
    clip.ClipName = "Idle";
    clip.Joints = {
        MakeSkeletonJoint("Root", {"Root"}),
        MakeSkeletonJoint("A", {"Root", "A"}),
        MakeSkeletonJoint("AChild", {"Root", "A", "AChild"}),
        MakeSkeletonJoint("B", {"Root", "B"}),
    };
    clip.AnimatedJointHierarchies = {{"Root", "A", "AChild"}, {"Root", "B"}};

    const vector<ModelSkeletonClipSource> clips = {clip};
    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, clips);

    REQUIRE(report.CanonicalJoints.size() == 4);
    CHECK(report.CanonicalJoints[0].Hierarchy == vector<string> {"Root"});
    CHECK(report.CanonicalJoints[1].Hierarchy == vector<string> {"Root", "A"});
    CHECK(report.CanonicalJoints[2].Hierarchy == vector<string> {"Root", "A", "AChild"});
    CHECK(report.CanonicalJoints[3].Hierarchy == vector<string> {"Root", "B"});
}

TEST_CASE("ModelSkeletonCompatibilityAllowsEmptyTechnicalSceneRoot")
{
    ModelSkeletonSource base;
    base.FileName = "Models/TechnicalRoot.fbx";
    base.Joints = {
        MakeSkeletonJoint("", {""}),
        MakeSkeletonJoint("Body", {"", "Body"}),
    };

    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, {});

    REQUIRE(report.CanonicalJoints.size() == 2);
    CHECK(report.CanonicalJoints[0].Name.empty());
    CHECK(report.CanonicalJoints[0].Hierarchy == vector<string> {""});
    CHECK(report.CanonicalJoints[1].Hierarchy == vector<string> {"", "Body"});
}

TEST_CASE("ModelSkeletonCompatibilityFormatsEmptyTechnicalSceneRootUnambiguously")
{
    ModelSkeletonSource base;
    base.FileName = "Models/TechnicalRoot.fbx";
    base.Joints = {MakeSkeletonJoint("", {""})};

    ModelSkeletonClipSource clip;
    clip.FileName = "Animations/TechnicalRoot.fbx";
    clip.ClipName = "Idle";
    clip.Joints = {
        MakeSkeletonJoint("", {""}),
        MakeSkeletonJoint("Body", {"", "Body"}),
    };
    clip.AnimatedJointHierarchies = {{"", "Body"}};

    const vector<ModelSkeletonClipSource> clips = {clip};
    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, clips);
    const string formatted_report = FormatModelSkeletonCompatibilityReport(report);

    CHECK(formatted_report.find("<empty-root>/Body") != string::npos);
    CHECK(formatted_report.find("contributions=[Body <-") == string::npos);
}

TEST_CASE("ModelSkeletonCompatibilityFormatsAnimationDataIssues")
{
    ModelSkeletonCompatibilityReport report;
    report.BaseFile = "Models/Body.fbx";
    report.CanonicalJoints = {MakeSkeletonJoint("Root", {"Root"})};

    ModelSkeletonAnimationDataIssue issue;
    issue.FileName = "Animations/Broken.fbx";
    issue.ClipName = "Idle";
    issue.JointName = "UnusedHelper";
    issue.Hierarchy = {"Root", "UnusedHelper"};
    issue.NonFiniteRotationKeys = 2;
    issue.NonFiniteTranslationKeys = 1;
    report.AnimationDataIssues.emplace_back(std::move(issue));

    const string formatted_report = FormatModelSkeletonCompatibilityReport(report);
    CHECK(formatted_report.find("animation_data_issues=1") != string::npos);
    CHECK(formatted_report.find("non_finite_keys=3") != string::npos);
    CHECK(formatted_report.find("Root/UnusedHelper <- Animations/Broken.fbx#Idle") != string::npos);
}

TEST_CASE("ModelSkeletonCompatibilityRejectsEmptyNonRootJoint")
{
    ModelSkeletonSource base;
    base.FileName = "Models/EmptyChild.fbx";
    base.Joints = {
        MakeSkeletonJoint("Root", {"Root"}),
        MakeSkeletonJoint("", {"Root", ""}),
    };

    CHECK_THROWS_AS(BuildModelSkeletonCompatibilityReport(base, {}), ModelSkeletonCompatibilityException);
}

TEST_CASE("ModelSkeletonCompatibilityRejectsAmbiguousSkeletons")
{
    SECTION("Duplicate full hierarchy")
    {
        ModelSkeletonSource base;
        base.FileName = "Models/DuplicatePath.fbx";
        base.Joints = {
            MakeSkeletonJoint("Root", {"Root"}),
            MakeSkeletonJoint("Spine", {"Root", "Spine"}),
            MakeSkeletonJoint("Spine", {"Root", "Spine"}),
        };

        CHECK_THROWS_AS(BuildModelSkeletonCompatibilityReport(base, {}), ModelSkeletonCompatibilityException);
    }

    SECTION("Duplicate joint name")
    {
        ModelSkeletonSource base;
        base.FileName = "Models/DuplicateName.fbx";
        base.Joints = {
            MakeSkeletonJoint("Root", {"Root"}),
            MakeSkeletonJoint("Left", {"Root", "Left"}),
            MakeSkeletonJoint("Hand", {"Root", "Left", "Hand"}),
            MakeSkeletonJoint("Right", {"Root", "Right"}),
            MakeSkeletonJoint("Hand", {"Root", "Right", "Hand"}),
        };

        CHECK_THROWS_AS(BuildModelSkeletonCompatibilityReport(base, {}), ModelSkeletonCompatibilityException);
    }
}

TEST_CASE("ModelSkeletonCompatibilityRejectsParentConflictsAndReportsRestPoseDivergence")
{
    ModelSkeletonSource base;
    base.FileName = "Models/Body.fbx";
    base.Joints = {
        MakeSkeletonJoint("Root", {"Root"}),
        MakeSkeletonJoint("Left", {"Root", "Left"}),
        MakeSkeletonJoint("Hand", {"Root", "Left", "Hand"}),
        MakeSkeletonJoint("Spine", {"Root", "Spine"}),
    };

    SECTION("Parent conflict")
    {
        ModelSkeletonClipSource clip;
        clip.FileName = "Animations/ParentConflict.fbx";
        clip.ClipName = "Idle";
        clip.Joints = {
            MakeSkeletonJoint("Root", {"Root"}),
            MakeSkeletonJoint("Right", {"Root", "Right"}),
            MakeSkeletonJoint("Hand", {"Root", "Right", "Hand"}),
        };
        clip.AnimatedJointHierarchies = {{"Root", "Right", "Hand"}};

        const vector<ModelSkeletonClipSource> clips = {clip};
        CHECK_THROWS_AS(BuildModelSkeletonCompatibilityReport(base, clips), ModelSkeletonCompatibilityException);
    }

    SECTION("Rest-pose divergence")
    {
        ModelSkeletonClipSource clip;
        clip.FileName = "Animations/RestConflict.fbx";
        clip.ClipName = "Idle";
        clip.Joints = {
            MakeSkeletonJoint("Root", {"Root"}),
            MakeSkeletonJoint("Spine", {"Root", "Spine"}, 0.25f),
        };
        clip.AnimatedJointHierarchies = {{"Root", "Spine"}};

        const vector<ModelSkeletonClipSource> clips = {clip};
        const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, clips);

        REQUIRE(report.RestPoseDivergences.size() == 1);
        CHECK(report.RestPoseDivergences.front().Hierarchy == vector<string> {"Root", "Spine"});
        CHECK(report.RestPoseDivergences.front().FileName == clip.FileName);
        CHECK(report.RestPoseDivergences.front().ClipName == clip.ClipName);
        CHECK(report.RestPoseDivergences.front().MaxDifference == Catch::Approx(0.25f));
        CHECK(FormatModelSkeletonCompatibilityReport(report).find("rest_pose_divergences=1") != string::npos);
        CHECK(FormatModelSkeletonCompatibilityReport(report).find("max_delta=0.25") != string::npos);
    }

    SECTION("Rest-pose tolerance")
    {
        ModelSkeletonClipSource clip;
        clip.FileName = "Animations/RestTolerance.fbx";
        clip.ClipName = "Idle";
        clip.Joints = {
            MakeSkeletonJoint("Root", {"Root"}),
            MakeSkeletonJoint("Spine", {"Root", "Spine"}, 0.00005f),
        };
        clip.AnimatedJointHierarchies = {{"Root", "Spine"}};

        const vector<ModelSkeletonClipSource> clips = {clip};
        const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, clips);
        CHECK(report.RestPoseDivergences.empty());
    }
}

TEST_CASE("ModelSkeletonCompatibilityRejectsAnimationHierarchyOutsideSourceSkeleton")
{
    ModelSkeletonSource base;
    base.FileName = "Models/Body.fbx";
    base.Joints = {MakeSkeletonJoint("Root", {"Root"})};

    ModelSkeletonClipSource clip;
    clip.FileName = "Animations/Invalid.fbx";
    clip.ClipName = "Idle";
    clip.Joints = {MakeSkeletonJoint("Root", {"Root"})};
    clip.AnimatedJointHierarchies = {{"Root", "Missing"}};

    const vector<ModelSkeletonClipSource> clips = {clip};
    CHECK_THROWS_AS(BuildModelSkeletonCompatibilityReport(base, clips), ModelSkeletonCompatibilityException);
}

#endif

FO_END_NAMESPACE

#endif
