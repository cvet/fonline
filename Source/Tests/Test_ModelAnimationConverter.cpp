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

#include "catch_amalgamated.hpp"

#include "Common.h"

#if FO_ENABLE_3D

#include "ModelAnimation.h"
#include "ModelAnimationConverter.h"

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

static auto MakeModelAnimationTestJoint(string_view name, initializer_list<string_view> hierarchy, const mat44& rest = mat44 {1.0f}) -> ModelSkeletonJoint
{
    FO_STACK_TRACE_ENTRY();

    ModelSkeletonJoint result;
    result.Name = name;
    result.RestLocalTransform = rest;

    for (string_view hierarchy_name : hierarchy) {
        result.Hierarchy.emplace_back(hierarchy_name);
    }

    return result;
}

static auto MakeModelAnimationTestAnimationJoint(string_view output_name, initializer_list<string_view> hierarchy, const vector<float32_t>& times, const vector<vec3>& translations, const vector<quaternion>& rotations = {}, const vector<vec3>& scales = {}) -> ModelAnimationJointSource
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationJointSource result;
    result.OutputName = output_name;

    for (string_view hierarchy_name : hierarchy) {
        result.Hierarchy.emplace_back(hierarchy_name);
    }

    result.Translation.Times = times;
    result.Translation.Values = translations;
    result.Rotation.Times = times;
    result.Rotation.Values = !rotations.empty() ? rotations : vector<quaternion>(times.size(), quaternion {1.0f, 0.0f, 0.0f, 0.0f});
    result.Scale.Times = times;
    result.Scale.Values = !scales.empty() ? scales : vector<vec3>(times.size(), vec3 {1.0f});
    return result;
}

static auto LoadModelAnimationRuntimeTestRig(string_view model_description, string_view base_model, ModelAnimationRigArtifacts artifacts, bool nearest_sampling) -> unique_ptr<ModelAnimationRuntimeRig>
{
    FO_STACK_TRACE_ENTRY();

    vector<ModelAnimationRigBindingSource> bindings;
    bindings.reserve(artifacts.Clips.size());

    for (size_t clip_index = 0; clip_index < artifacts.Clips.size(); clip_index++) {
        const ModelAnimationClipArtifact& clip = artifacts.Clips[clip_index];
        bindings.emplace_back(ModelAnimationRigBindingSource {numeric_cast<int32_t>(clip_index + 1), 0, clip.SourceFile, clip.ClipName, false});
    }

    const ModelAnimationRigData rig_data = BuildModelAnimationRigData(std::move(artifacts), bindings);
    const vector<uint8_t> serialized = WriteModelAnimationRigData(rig_data, strex("runtime test for '{}'", model_description));
    return LoadModelAnimationRuntimeRig(serialized, model_description, base_model, nearest_sampling);
}

static auto SampleModelAnimationRuntimeTestMatrices(const ModelAnimationRuntimeRig& rig, size_t clip_index, float32_t ratio) -> vector<mat44>
{
    FO_STACK_TRACE_ENTRY();

    const ModelAnimationRuntimeClip& clip = rig.GetClip(clip_index);
    const float32_t duration = clip.GetDuration();
    const float32_t position = ratio < 1.0f ? ratio * duration : std::nextafter(duration, 0.0f);
    ModelAnimationRuntimePose pose {&rig};
    const array<ModelAnimationRuntimePose::TrackInput, 2> body {
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = numeric_cast<int32_t>(clip_index), .Enabled = true, .Position = position, .Weight = 1.0f},
        ModelAnimationRuntimePose::TrackInput {},
    };
    pose.Evaluate(body, {}, mat44 {1.0f});
    return {pose.GetWorldMatrices().begin(), pose.GetWorldMatrices().end()};
}

static auto GetModelAnimationRuntimeTestRestMatrices(const ModelAnimationRuntimeRig& rig) -> vector<mat44>
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationRuntimePose pose {&rig};
    pose.BuildModelMatrices(mat44 {1.0f});
    return {pose.GetWorldMatrices().begin(), pose.GetWorldMatrices().end()};
}

static auto GetModelAnimationTestColumn(const mat44& matrix, size_t column) -> array<float32_t, 4>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(column < 4, "Matrix column {} is outside test matrix", column);
    const mat44::length_type matrix_column = numeric_cast<mat44::length_type>(column);
    return {matrix[matrix_column][0], matrix[matrix_column][1], matrix[matrix_column][2], matrix[matrix_column][3]};
}

static auto BuildModelAnimationProductionTestRigData() -> ModelAnimationRigData
{
    FO_STACK_TRACE_ENTRY();

    ModelSkeletonSource base;
    base.FileName = "Models/ProductionBody.fbx";
    base.Joints = {MakeModelAnimationTestJoint("Root", {"Root"})};
    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, {});

    ModelAnimationSource idle;
    idle.FileName = base.FileName;
    idle.Name = "Idle";
    idle.Duration = 1.0f;
    idle.Joints = {MakeModelAnimationTestAnimationJoint("Root", {"Root"}, {0.0f, 1.0f}, {vec3 {0.0f}, vec3 {0.0f}})};

    ModelAnimationSource move;
    move.FileName = "Animations/ProductionBody.fbx";
    move.Name = "Move";
    move.Duration = 1.0f;
    move.Joints = {MakeModelAnimationTestAnimationJoint("Root", {"Root"}, {0.0f, 1.0f}, {vec3 {1.0f, 0.0f, 0.0f}, vec3 {3.0f, 0.0f, 0.0f}})};

    const vector<ModelAnimationSource> animations {move, idle};
    ModelAnimationRigArtifacts artifacts = BuildModelAnimationRigArtifacts("Models/ProductionBody.fo3d", base, report, animations, false);
    const vector<ModelAnimationRigBindingSource> bindings {
        ModelAnimationRigBindingSource {20, 4, move.FileName, move.Name, true},
        ModelAnimationRigBindingSource {10, 2, idle.FileName, idle.Name, false},
    };
    return BuildModelAnimationRigData(std::move(artifacts), bindings);
}

static auto BuildModelAnimationRuntimeEvaluationTestRig(bool nearest_sampling) -> unique_ptr<ModelAnimationRuntimeRig>
{
    FO_STACK_TRACE_ENTRY();

    ModelSkeletonSource base;
    base.FileName = "Models/RuntimeEvaluation.fbx";
    base.Joints = {
        MakeModelAnimationTestJoint("Root", {"Root"}, glm::translate(mat44 {1.0f}, vec3 {1.0f, 0.0f, 0.0f})),
        MakeModelAnimationTestJoint("Arm", {"Root", "Arm"}, glm::translate(mat44 {1.0f}, vec3 {2.0f, 0.0f, 0.0f})),
        MakeModelAnimationTestJoint("Leg", {"Root", "Leg"}, glm::translate(mat44 {1.0f}, vec3 {4.0f, 0.0f, 0.0f})),
        MakeModelAnimationTestJoint("Rest", {"Root", "Rest"}, glm::translate(mat44 {1.0f}, vec3 {6.0f, 0.0f, 0.0f})),
    };

    ModelSkeletonClipSource contribution;
    contribution.FileName = "Animations/RuntimeEvaluationHelper.fbx";
    contribution.ClipName = "HelperContribution";
    contribution.Joints = base.Joints;
    contribution.Joints.emplace_back(MakeModelAnimationTestJoint("Helper", {"Root", "Helper"}));
    contribution.AnimatedJointHierarchies = {{"Root", "Helper"}};
    const vector<ModelSkeletonClipSource> contributions {contribution};
    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, contributions);
    const vector<float32_t> times = nearest_sampling ? vector<float32_t> {0.0f, 0.5f, 1.0f} : vector<float32_t> {0.0f, 1.0f};
    const auto constant_values = [&times](float32_t x) { return vector<vec3>(times.size(), vec3 {x, 0.0f, 0.0f}); };

    ModelAnimationSource body0;
    body0.FileName = "Animations/RuntimeEvaluation.fbx";
    body0.Name = "Body0";
    body0.Duration = 1.0f;
    body0.Joints = {
        MakeModelAnimationTestAnimationJoint("Root", {"Root"}, times, constant_values(10.0f)),
        MakeModelAnimationTestAnimationJoint("Arm", {"Root", "Arm"}, times, constant_values(20.0f)),
    };

    ModelAnimationSource body1;
    body1.FileName = body0.FileName;
    body1.Name = "Body1";
    body1.Duration = 1.0f;
    body1.Joints = {
        MakeModelAnimationTestAnimationJoint("Root", {"Root"}, times, constant_values(30.0f)),
        MakeModelAnimationTestAnimationJoint("Leg", {"Root", "Leg"}, times, constant_values(40.0f)),
    };

    ModelAnimationSource movement0;
    movement0.FileName = body0.FileName;
    movement0.Name = "Movement0";
    movement0.Duration = 1.0f;
    movement0.Joints = {
        MakeModelAnimationTestAnimationJoint("Arm", {"Root", "Arm"}, times, constant_values(200.0f)),
        MakeModelAnimationTestAnimationJoint("Leg", {"Root", "Leg"}, times, constant_values(300.0f)),
    };

    ModelAnimationSource movement1;
    movement1.FileName = body0.FileName;
    movement1.Name = "Movement1";
    movement1.Duration = 1.0f;
    movement1.Joints = {
        MakeModelAnimationTestAnimationJoint("Leg", {"Root", "Leg"}, times, constant_values(500.0f)),
    };

    ModelAnimationSource curve;
    curve.FileName = body0.FileName;
    curve.Name = "Curve";
    curve.Duration = 1.0f;
    curve.Joints = {
        MakeModelAnimationTestAnimationJoint("Root", {"Root"}, times, nearest_sampling ? vector<vec3> {vec3 {0.0f}, vec3 {10.0f, 0.0f, 0.0f}, vec3 {20.0f, 0.0f, 0.0f}} : vector<vec3> {vec3 {0.0f}, vec3 {20.0f, 0.0f, 0.0f}}),
    };

    ModelAnimationSource rotation0;
    rotation0.FileName = body0.FileName;
    rotation0.Name = "Rotation0";
    rotation0.Duration = 1.0f;
    rotation0.Joints = {
        MakeModelAnimationTestAnimationJoint("Root", {"Root"}, times, constant_values(1.0f), vector<quaternion>(times.size(), quaternion {1.0f, 0.0f, 0.0f, 0.0f})),
        MakeModelAnimationTestAnimationJoint("Arm", {"Root", "Arm"}, times, constant_values(2.0f), vector<quaternion>(times.size(), quaternion {1.0f, 0.0f, 0.0f, 0.0f})),
    };

    ModelAnimationSource rotation1;
    rotation1.FileName = body0.FileName;
    rotation1.Name = "Rotation1";
    rotation1.Duration = 1.0f;
    rotation1.Joints = {
        MakeModelAnimationTestAnimationJoint("Root", {"Root"}, times, constant_values(1.0f), vector<quaternion>(times.size(), glm::angleAxis(glm::radians(136.0f), vec3 {0.0f, 1.0f, 0.0f}))),
        MakeModelAnimationTestAnimationJoint("Arm", {"Root", "Arm"}, times, constant_values(2.0f), vector<quaternion>(times.size(), glm::angleAxis(glm::radians(179.0f), glm::normalize(vec3 {0.2f, 0.7f, 0.3f})))),
    };

    const vector<ModelAnimationSource> animations {body0, body1, movement0, movement1, curve, rotation0, rotation1};
    ModelAnimationRigArtifacts artifacts = BuildModelAnimationRigArtifacts("Models/RuntimeEvaluation.fo3d", base, report, animations, nearest_sampling);
    const vector<ModelAnimationRigBindingSource> bindings {
        ModelAnimationRigBindingSource {1, 0, body0.FileName, body0.Name, false},
        ModelAnimationRigBindingSource {2, 0, body1.FileName, body1.Name, false},
        ModelAnimationRigBindingSource {3, 0, movement0.FileName, movement0.Name, false},
        ModelAnimationRigBindingSource {4, 0, movement1.FileName, movement1.Name, false},
        ModelAnimationRigBindingSource {5, 0, curve.FileName, curve.Name, false},
        ModelAnimationRigBindingSource {6, 0, rotation0.FileName, rotation0.Name, false},
        ModelAnimationRigBindingSource {7, 0, rotation1.FileName, rotation1.Name, false},
    };
    const ModelAnimationRigData rig_data = BuildModelAnimationRigData(std::move(artifacts), bindings);
    const vector<uint8_t> serialized = WriteModelAnimationRigData(rig_data, "runtime evaluation test");
    return LoadModelAnimationRuntimeRig(serialized, "Models/RuntimeEvaluation.fo3d", base.FileName, nearest_sampling);
}

static auto GetModelAnimationRuntimeEvaluationClip(const ModelAnimationRuntimeRig& rig, int32_t state_anim) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto binding = rig.FindBinding(state_anim, 0);

    if (!binding) {
        throw std::runtime_error("Missing runtime-evaluation binding");
    }

    return numeric_cast<int32_t>(binding->ClipIndex);
}

static auto GetModelAnimationRuntimeEvaluationJoint(const ModelAnimationRuntimeRig& rig, string_view name) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    for (size_t joint = 0; joint < rig.GetJointCount(); joint++) {
        if (name == rig.GetJointName(joint)) {
            return joint;
        }
    }

    throw std::runtime_error("Missing runtime-evaluation joint");
}

TEST_CASE("ModelAnimationConverterProductionRigDataLoadsFromUnalignedOwnedBytes")
{
    const ModelAnimationRigData rig_data = BuildModelAnimationProductionTestRigData();
    unique_ptr<ModelAnimationRuntimeRig> runtime_rig = [&rig_data] {
        vector<uint8_t> serialized = WriteModelAnimationRigData(rig_data, "production runtime test");
        vector<uint8_t> unaligned;
        unaligned.reserve(serialized.size() + 1);
        unaligned.emplace_back(uint8_t {0xa5});
        unaligned.insert(unaligned.end(), serialized.begin(), serialized.end());
        unique_ptr<ModelAnimationRuntimeRig> loaded = LoadModelAnimationRuntimeRig(span(unaligned).subspan(1), "Models/ProductionBody.fo3d", "Models/ProductionBody.fbx", false);
        std::fill(serialized.begin(), serialized.end(), uint8_t {0});
        std::fill(unaligned.begin(), unaligned.end(), uint8_t {0});
        return loaded;
    }();

    REQUIRE(runtime_rig->GetJointCount() == 1);
    REQUIRE(runtime_rig->GetClipCount() == 2);
    REQUIRE(runtime_rig->GetBaseJointMapping().size() == 1);
    CHECK(runtime_rig->GetBaseJointMapping()[0] == 0);
    REQUIRE(runtime_rig->GetBaseJointPresence().size() == 1);
    CHECK(runtime_rig->GetBaseJointPresence()[0] == uint8_t {1});

    const auto idle_binding = runtime_rig->FindBinding(10, 2);
    const auto move_binding = runtime_rig->FindBinding(20, 4);
    REQUIRE(idle_binding);
    REQUIRE(move_binding);
    CHECK_FALSE(idle_binding->Reversed);
    CHECK(move_binding->Reversed);
    CHECK_FALSE(runtime_rig->FindBinding(20, 5));

    const ModelAnimationRuntimeClip& idle_clip = runtime_rig->GetClip(idle_binding->ClipIndex);
    const ModelAnimationRuntimeClip& move_clip = runtime_rig->GetClip(move_binding->ClipIndex);
    CHECK(idle_clip.GetSourceFile() == "Models/ProductionBody.fbx");
    CHECK(idle_clip.GetClipName() == "Idle");
    CHECK(move_clip.GetSourceFile() == "Animations/ProductionBody.fbx");
    CHECK(move_clip.GetClipName() == "Move");
    CHECK(move_clip.GetDuration() == Catch::Approx(1.0f));
    REQUIRE(runtime_rig->FindClip(move_clip.GetSourceFile(), move_clip.GetClipName()));
    REQUIRE(move_clip.GetJointPresence().size() == 1);
    CHECK(move_clip.GetJointPresence()[0] == uint8_t {1});

    const auto matrices = SampleModelAnimationRuntimeTestMatrices(*runtime_rig, move_binding->ClipIndex, 0.5f);
    REQUIRE(matrices.size() == 1);
    const auto translation = GetModelAnimationTestColumn(matrices.front(), 3);
    CHECK(translation[0] == Catch::Approx(2.0f).margin(1.0e-3f));
    CHECK(translation[1] == Catch::Approx(0.0f).margin(1.0e-3f));
    CHECK(translation[2] == Catch::Approx(0.0f).margin(1.0e-3f));
}

TEST_CASE("ModelAnimationRuntimePoseOwnsIndependentHierarchicalRestMatrices")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<ModelAnimationRuntimePose>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<ModelAnimationRuntimePose>);

    const quaternion root_rotation = glm::angleAxis(glm::radians(31.0f), glm::normalize(vec3 {1.0f, 2.0f, 3.0f}));
    const quaternion child_rotation = glm::angleAxis(glm::radians(-47.0f), glm::normalize(vec3 {2.0f, 1.0f, 0.5f}));
    const mat44 root_rest = glm::translate(mat44 {1.0f}, vec3 {1.0f, 2.0f, 3.0f}) * glm::mat4_cast(root_rotation) * glm::scale(mat44 {1.0f}, vec3 {2.0f, 1.5f, 0.75f});
    const mat44 child_rest = glm::translate(mat44 {1.0f}, vec3 {-2.0f, 4.0f, 1.0f}) * glm::mat4_cast(child_rotation) * glm::scale(mat44 {1.0f}, vec3 {0.5f, 1.25f, 3.0f});
    const mat44 leaf_rest = glm::translate(mat44 {1.0f}, vec3 {0.0f, 1.0f, 5.0f});

    ModelSkeletonSource base;
    base.FileName = "Models/RuntimePose.fbx";
    base.Joints = {
        MakeModelAnimationTestJoint("Root", {"Root"}, root_rest),
        MakeModelAnimationTestJoint("Child", {"Root", "Child"}, child_rest),
        MakeModelAnimationTestJoint("Leaf", {"Root", "Child", "Leaf"}, leaf_rest),
    };
    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, {});
    ModelAnimationRigArtifacts artifacts = BuildModelAnimationRigArtifacts("Models/RuntimePose.fo3d", base, report, {}, false);
    const ModelAnimationRigData rig_data = BuildModelAnimationRigData(std::move(artifacts), {});
    const vector<uint8_t> serialized = WriteModelAnimationRigData(rig_data, "runtime pose test");
    const unique_ptr<ModelAnimationRuntimeRig> runtime_rig = LoadModelAnimationRuntimeRig(serialized, "Models/RuntimePose.fo3d", base.FileName, false);

    ModelAnimationRuntimePose first {runtime_rig.get()};
    ModelAnimationRuntimePose second {runtime_rig.get()};
    REQUIRE(first.GetJointCount() == 3);
    REQUIRE(first.GetWorldMatrices().size() == first.GetJointCount());
    CHECK(first.GetWorldMatrices().data() != second.GetWorldMatrices().data());

    const mat44 first_root = glm::translate(mat44 {1.0f}, vec3 {10.0f, -3.0f, 7.0f}) * glm::rotate(mat44 {1.0f}, glm::radians(23.0f), glm::normalize(vec3 {0.0f, 1.0f, 1.0f})) * glm::scale(mat44 {1.0f}, vec3 {1.25f, 0.8f, 1.5f});
    const mat44 second_root = glm::translate(mat44 {1.0f}, vec3 {-20.0f, 5.0f, 2.0f});
    first.BuildModelMatrices(first_root);
    second.BuildModelMatrices(second_root);
    const vector<mat44> second_world_before {second.GetWorldMatrices().begin(), second.GetWorldMatrices().end()};
    first.BuildModelMatrices(glm::translate(mat44 {1.0f}, vec3 {100.0f, 200.0f, 300.0f}));

    for (size_t joint = 0; joint < second.GetJointCount(); joint++) {
        for (mat44::length_type column = 0; column < 4; column++) {
            for (mat44::length_type row = 0; row < 4; row++) {
                CHECK(second.GetWorldMatrices()[joint][column][row] == second_world_before[joint][column][row]);
            }
        }
    }

    first.BuildModelMatrices(first_root);
    const array<mat44, 3> expected_world {
        first_root * root_rest,
        first_root * root_rest * child_rest,
        first_root * root_rest * child_rest * leaf_rest,
    };

    for (size_t joint = 0; joint < expected_world.size(); joint++) {
        for (mat44::length_type column = 0; column < 4; column++) {
            for (mat44::length_type row = 0; row < 4; row++) {
                CHECK(first.GetWorldMatrices()[joint][column][row] == Catch::Approx(expected_world[joint][column][row]).margin(3.0e-4f));
            }
        }
    }
}

TEST_CASE("ModelAnimationConverterRejectsEmptyCanonicalRig")
{
    ModelSkeletonSource base;
    base.FileName = "Models/Empty.fbx";
    ModelSkeletonCompatibilityReport report;
    report.BaseFile = base.FileName;
    CHECK_THROWS_WITH(BuildModelAnimationRigArtifacts("Models/Empty.fo3d", base, report, {}, false), Catch::Matchers::ContainsSubstring("has no joints"));
}

TEST_CASE("ModelAnimationRuntimePoseBlendsPartialBodyAndMaskedMovement")
{
    const unique_ptr<ModelAnimationRuntimeRig> rig = BuildModelAnimationRuntimeEvaluationTestRig(false);
    const size_t root = GetModelAnimationRuntimeEvaluationJoint(*rig, "Root");
    const size_t arm = GetModelAnimationRuntimeEvaluationJoint(*rig, "Arm");
    const size_t leg = GetModelAnimationRuntimeEvaluationJoint(*rig, "Leg");
    const size_t rest = GetModelAnimationRuntimeEvaluationJoint(*rig, "Rest");
    const size_t helper = GetModelAnimationRuntimeEvaluationJoint(*rig, "Helper");
    const int32_t body0_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 1);
    const int32_t body1_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 2);
    const int32_t movement0_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 3);
    const int32_t movement1_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 4);
    ModelAnimationRuntimePose first {rig.get()};
    ModelAnimationRuntimePose second {rig.get()};
    array<ModelAnimationRuntimePose::TrackInput, 2> body {
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = body0_clip, .Enabled = true, .Position = 0.5f, .Weight = 0.6f},
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = body1_clip, .Enabled = true, .Position = 0.5f, .Weight = 0.25f},
    };
    array<ModelAnimationRuntimePose::TrackInput, 2> movement {};
    first.Evaluate(body, movement, mat44 {1.0f});

    const auto check_local_x = [&first](size_t joint, float32_t expected) { CHECK(first.GetFinalLocalTransform(joint).Translation.x == Catch::Approx(expected).margin(2.0e-2f)); };
    check_local_x(root, 15.0f);
    check_local_x(arm, 20.0f);
    check_local_x(leg, 40.0f);
    check_local_x(rest, 6.0f);
    check_local_x(helper, 0.0f);
    CHECK(first.GetBodyLocalTransform(root).Translation.x == Catch::Approx(15.0f).margin(2.0e-2f));

    vector<uint8_t> leg_mask(first.GetJointCount(), uint8_t {0});
    leg_mask[leg] = uint8_t {1};
    movement = {
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = movement0_clip, .Enabled = true, .Position = 0.5f, .Weight = 0.8f, .JointMask = leg_mask},
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = movement1_clip, .Enabled = true, .Position = 0.5f, .Weight = 0.25f, .JointMask = leg_mask},
    };
    first.Evaluate(body, movement, mat44 {1.0f});
    check_local_x(root, 15.0f);
    check_local_x(arm, 20.0f);
    check_local_x(leg, 350.0f);
    check_local_x(rest, 6.0f);
    check_local_x(helper, 0.0f);

    vector<uint8_t> suppress_old_root(first.GetJointCount(), uint8_t {1});
    suppress_old_root[root] = uint8_t {0};
    body[0].JointMask = suppress_old_root;
    body[1].Weight = 0.1f;
    movement = {};
    first.Evaluate(body, movement, mat44 {1.0f});
    check_local_x(root, 30.0f);
    check_local_x(arm, 20.0f);
    check_local_x(leg, 40.0f);

    array<ModelAnimationRuntimePose::TrackInput, 2> inactive_body {
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = body0_clip, .Enabled = true, .Position = 0.5f, .Weight = 0.0f},
        ModelAnimationRuntimePose::TrackInput {},
    };
    first.Evaluate(inactive_body, {}, mat44 {1.0f});
    check_local_x(root, 1.0f);
    check_local_x(arm, 2.0f);
    check_local_x(leg, 4.0f);
    check_local_x(rest, 6.0f);
    check_local_x(helper, 0.0f);

    const mat44 second_root = glm::translate(mat44 {1.0f}, vec3 {100.0f, 200.0f, 300.0f});
    second.Evaluate(body, movement, second_root);
    const vector<mat44> second_world_before {second.GetWorldMatrices().begin(), second.GetWorldMatrices().end()};
    first.Evaluate(body, movement, mat44 {1.0f});
    REQUIRE(first.GetWorldMatrices().data() != second.GetWorldMatrices().data());

    for (size_t joint = 0; joint < second.GetJointCount(); joint++) {
        for (mat44::length_type column = 0; column < 4; column++) {
            for (mat44::length_type row = 0; row < 4; row++) {
                CHECK(second.GetWorldMatrices()[joint][column][row] == second_world_before[joint][column][row]);
            }
        }
    }
}

TEST_CASE("ModelAnimationRuntimePoseUsesLegacySlerpForLargeAngleCrossfades")
{
    const unique_ptr<ModelAnimationRuntimeRig> rig = BuildModelAnimationRuntimeEvaluationTestRig(false);
    const size_t root = GetModelAnimationRuntimeEvaluationJoint(*rig, "Root");
    const size_t arm = GetModelAnimationRuntimeEvaluationJoint(*rig, "Arm");
    const int32_t rotation0_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 6);
    const int32_t rotation1_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 7);
    ModelAnimationRuntimePose pose {rig.get()};
    array<ModelAnimationRuntimePose::TrackInput, 2> body {
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = rotation0_clip, .Enabled = true, .Position = 0.5f, .Weight = 0.37f},
        ModelAnimationRuntimePose::TrackInput {},
    };

    pose.Evaluate(body, {}, mat44 {1.0f});
    const array<quaternion, 2> rotation0 {pose.GetBodyLocalTransform(root).Rotation, pose.GetBodyLocalTransform(arm).Rotation};
    body = {
        ModelAnimationRuntimePose::TrackInput {},
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = rotation1_clip, .Enabled = true, .Position = 0.5f, .Weight = 1.0f},
    };
    pose.Evaluate(body, {}, mat44 {1.0f});
    const array<quaternion, 2> rotation1 {pose.GetBodyLocalTransform(root).Rotation, pose.GetBodyLocalTransform(arm).Rotation};
    const array<size_t, 2> joints {root, arm};

    for (float32_t weight : array<float32_t, 4> {0.2113f, 0.25f, 0.75f, 0.7887f}) {
        body = {
            ModelAnimationRuntimePose::TrackInput {.ClipIndex = rotation0_clip, .Enabled = true, .Position = 0.5f, .Weight = 0.37f},
            ModelAnimationRuntimePose::TrackInput {.ClipIndex = rotation1_clip, .Enabled = true, .Position = 0.5f, .Weight = weight},
        };
        pose.Evaluate(body, {}, mat44 {1.0f});

        for (size_t i = 0; i < joints.size(); i++) {
            const quaternion expected = glm::normalize(glm::slerp(rotation0[i], rotation1[i], weight));
            const quaternion actual = pose.GetBodyLocalTransform(joints[i]).Rotation;
            CHECK(float_abs(glm::dot(actual, expected)) == Catch::Approx(1.0f).margin(2.0e-5f));
        }
    }
}

TEST_CASE("ModelAnimationRuntimePosePreservesReverseLoopBoundaries")
{
    const unique_ptr<ModelAnimationRuntimeRig> rig = BuildModelAnimationRuntimeEvaluationTestRig(false);
    const size_t root = GetModelAnimationRuntimeEvaluationJoint(*rig, "Root");
    const int32_t curve_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 5);
    ModelAnimationRuntimePose pose {rig.get()};
    array<ModelAnimationRuntimePose::TrackInput, 2> body {
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = curve_clip, .Enabled = true, .Reversed = true, .Position = 0.0f, .Weight = 1.0f},
        ModelAnimationRuntimePose::TrackInput {},
    };

    pose.Evaluate(body, {}, mat44 {1.0f});
    CHECK(pose.GetFinalLocalTransform(root).Translation.x == Catch::Approx(20.0f).margin(2.0e-2f));
    body[0].Position = 0.5f;
    pose.Evaluate(body, {}, mat44 {1.0f});
    CHECK(pose.GetFinalLocalTransform(root).Translation.x == Catch::Approx(10.0f).margin(2.0e-2f));
    body[0].Position = 1.0f;
    pose.Evaluate(body, {}, mat44 {1.0f});
    CHECK(pose.GetFinalLocalTransform(root).Translation.x == Catch::Approx(20.0f).margin(2.0e-2f));
}

TEST_CASE("ModelAnimationRuntimePoseUsesNearestTimelineAndCrossfadeTie")
{
    const unique_ptr<ModelAnimationRuntimeRig> rig = BuildModelAnimationRuntimeEvaluationTestRig(true);
    const size_t root = GetModelAnimationRuntimeEvaluationJoint(*rig, "Root");
    const int32_t body0_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 1);
    const int32_t body1_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 2);
    const int32_t curve_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 5);
    ModelAnimationRuntimePose pose {rig.get()};
    array<ModelAnimationRuntimePose::TrackInput, 2> body {
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = curve_clip, .Enabled = true, .Position = 0.249f, .Weight = 1.0f},
        ModelAnimationRuntimePose::TrackInput {},
    };

    pose.Evaluate(body, {}, mat44 {1.0f});
    CHECK(pose.GetFinalLocalTransform(root).Translation.x == Catch::Approx(0.0f).margin(2.0e-2f));
    body[0].Position = 0.25f;
    pose.Evaluate(body, {}, mat44 {1.0f});
    CHECK(pose.GetFinalLocalTransform(root).Translation.x == Catch::Approx(10.0f).margin(2.0e-2f));

    body = {
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = body0_clip, .Enabled = true, .Position = 0.0f, .Weight = 1.0f},
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = body1_clip, .Enabled = true, .Position = 0.0f, .Weight = 0.499f},
    };
    pose.Evaluate(body, {}, mat44 {1.0f});
    CHECK(pose.GetFinalLocalTransform(root).Translation.x == Catch::Approx(10.0f).margin(2.0e-2f));
    body[1].Weight = 0.5f;
    pose.Evaluate(body, {}, mat44 {1.0f});
    CHECK(pose.GetFinalLocalTransform(root).Translation.x == Catch::Approx(30.0f).margin(2.0e-2f));
}

TEST_CASE("ModelAnimationRuntimePoseNearestReversePlaybackSelectsNextAuthoredTimeAtTies")
{
    const unique_ptr<ModelAnimationRuntimeRig> rig = BuildModelAnimationRuntimeEvaluationTestRig(true);
    const size_t root = GetModelAnimationRuntimeEvaluationJoint(*rig, "Root");
    const int32_t curve_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 5);
    ModelAnimationRuntimePose pose {rig.get()};
    array<ModelAnimationRuntimePose::TrackInput, 2> body {
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = curve_clip, .Enabled = true, .Reversed = true, .Weight = 1.0f},
        ModelAnimationRuntimePose::TrackInput {},
    };
    const array<pair<float32_t, float32_t>, 7> cases {
        pair {0.249f, 20.0f},
        pair {0.25f, 20.0f},
        pair {0.251f, 10.0f},
        pair {0.749f, 10.0f},
        pair {0.75f, 10.0f},
        pair {0.751f, 0.0f},
        pair {1.0f, 20.0f},
    };

    for (const auto& [position, expected] : cases) {
        CAPTURE(position, expected);
        body[0].Position = position;
        pose.Evaluate(body, {}, mat44 {1.0f});
        CHECK(pose.GetFinalLocalTransform(root).Translation.x == Catch::Approx(expected).margin(2.0e-2f));
    }
}

TEST_CASE("ModelAnimationRuntimePoseKeepsPublicBufferStorageStableAcrossEvaluate")
{
    const unique_ptr<ModelAnimationRuntimeRig> rig = BuildModelAnimationRuntimeEvaluationTestRig(false);
    const size_t root = GetModelAnimationRuntimeEvaluationJoint(*rig, "Root");
    const size_t arm = GetModelAnimationRuntimeEvaluationJoint(*rig, "Arm");
    const size_t leg = GetModelAnimationRuntimeEvaluationJoint(*rig, "Leg");
    const int32_t body0_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 1);
    const int32_t body1_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 2);
    const int32_t movement0_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 3);
    const int32_t movement1_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 4);
    ModelAnimationRuntimePose pose {rig.get()};
    vector<uint8_t> leg_mask(pose.GetJointCount(), uint8_t {0});
    leg_mask[leg] = uint8_t {1};

    const mat44* const world_matrices_data = pose.GetWorldMatrices().data();
    const size_t world_matrices_extent = pose.GetWorldMatrices().size();

    REQUIRE(world_matrices_data != nullptr);

    for (size_t iteration = 0; iteration < 128; iteration++) {
        CAPTURE(iteration);
        const float32_t position = numeric_cast<float32_t>(iteration) * 0.03125f;
        const mat44 root_matrix = glm::translate(mat44 {1.0f}, vec3 {position, -position * 0.5f, position * 0.25f}) * glm::rotate(mat44 {1.0f}, position * 0.1f, vec3 {0.0f, 0.0f, 1.0f});
        array<ModelAnimationRuntimePose::TrackInput, 2> body {};
        array<ModelAnimationRuntimePose::TrackInput, 2> movement {};

        switch (iteration % 4) {
        case 0:
            body[0] = ModelAnimationRuntimePose::TrackInput {.ClipIndex = body0_clip, .Enabled = true, .Position = position, .Weight = 1.0f};
            pose.Evaluate(body, movement, root_matrix);
            break;
        case 1: {
            body = {
                ModelAnimationRuntimePose::TrackInput {.ClipIndex = body0_clip, .Enabled = true, .Position = position, .Weight = 0.65f},
                ModelAnimationRuntimePose::TrackInput {.ClipIndex = body1_clip, .Enabled = true, .Reversed = true, .Position = position, .Weight = 0.35f},
            };
            const array<ModelAnimationRuntimePose::ProceduralLocalRotation, 1> procedural {
                ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = root, .Rotation = glm::angleAxis(glm::radians(17.0f), vec3 {0.0f, 1.0f, 0.0f})},
            };
            pose.Evaluate(body, movement, root_matrix, procedural);
            break;
        }
        case 2: {
            body[0] = ModelAnimationRuntimePose::TrackInput {.ClipIndex = body1_clip, .Enabled = true, .Position = position, .Weight = 1.0f};
            movement = {
                ModelAnimationRuntimePose::TrackInput {.ClipIndex = movement0_clip, .Enabled = true, .Position = position, .Weight = 0.7f, .JointMask = leg_mask},
                ModelAnimationRuntimePose::TrackInput {.ClipIndex = movement1_clip, .Enabled = true, .Reversed = true, .Position = position, .Weight = 0.3f, .JointMask = leg_mask},
            };
            const array<ModelAnimationRuntimePose::ProceduralLocalRotation, 2> procedural {
                ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = arm, .Rotation = glm::angleAxis(glm::radians(-23.0f), vec3 {1.0f, 0.0f, 0.0f})},
                ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = leg, .Rotation = glm::angleAxis(glm::radians(11.0f), vec3 {0.0f, 0.0f, 1.0f})},
            };
            pose.Evaluate(body, movement, root_matrix, procedural);
            break;
        }
        default:
            pose.Evaluate(body, movement, root_matrix);
            break;
        }

        CHECK(pose.GetWorldMatrices().data() == world_matrices_data);
        CHECK(pose.GetWorldMatrices().size() == world_matrices_extent);
    }
}

TEST_CASE("ModelAnimationRuntimePoseRejectsInvalidEvaluationInput")
{
    const unique_ptr<ModelAnimationRuntimeRig> rig = BuildModelAnimationRuntimeEvaluationTestRig(false);
    const int32_t body0_clip = GetModelAnimationRuntimeEvaluationClip(*rig, 1);
    ModelAnimationRuntimePose pose {rig.get()};
    const array<ModelAnimationRuntimePose::TrackInput, 2> valid_body {
        ModelAnimationRuntimePose::TrackInput {.ClipIndex = body0_clip, .Enabled = true, .Position = 0.5f, .Weight = 1.0f},
        ModelAnimationRuntimePose::TrackInput {},
    };

    SECTION("position")
    {
        auto body = valid_body;
        body[0].Position = -1.0f;
        CHECK_THROWS_WITH(pose.Evaluate(body, {}, mat44 {1.0f}), Catch::Matchers::ContainsSubstring("position"));
        body[0].Position = std::numeric_limits<float32_t>::quiet_NaN();
        CHECK_THROWS_WITH(pose.Evaluate(body, {}, mat44 {1.0f}), Catch::Matchers::ContainsSubstring("position"));
    }

    SECTION("weight")
    {
        auto body = valid_body;
        body[0].Weight = 1.01f;
        CHECK_THROWS_WITH(pose.Evaluate(body, {}, mat44 {1.0f}), Catch::Matchers::ContainsSubstring("weight"));
        body[0].Weight = std::numeric_limits<float32_t>::quiet_NaN();
        CHECK_THROWS_WITH(pose.Evaluate(body, {}, mat44 {1.0f}), Catch::Matchers::ContainsSubstring("weight"));
    }

    SECTION("clip index")
    {
        auto body = valid_body;
        body[0].ClipIndex = -2;
        CHECK_THROWS_WITH(pose.Evaluate(body, {}, mat44 {1.0f}), Catch::Matchers::ContainsSubstring("clip index"));
        body[0].ClipIndex = numeric_cast<int32_t>(rig->GetClipCount());
        CHECK_THROWS_WITH(pose.Evaluate(body, {}, mat44 {1.0f}), Catch::Matchers::ContainsSubstring("clip index"));
    }

    SECTION("joint mask")
    {
        auto body = valid_body;
        vector<uint8_t> short_mask(pose.GetJointCount() - 1, uint8_t {1});
        body[0].JointMask = short_mask;
        CHECK_THROWS_WITH(pose.Evaluate(body, {}, mat44 {1.0f}), Catch::Matchers::ContainsSubstring("joint mask"));
        vector<uint8_t> invalid_mask(pose.GetJointCount(), uint8_t {1});
        invalid_mask.back() = uint8_t {2};
        body[0].JointMask = invalid_mask;
        CHECK_THROWS_WITH(pose.Evaluate(body, {}, mat44 {1.0f}), Catch::Matchers::ContainsSubstring("joint mask"));
    }

    SECTION("root matrix")
    {
        mat44 root_matrix {1.0f};
        root_matrix[2][1] = std::numeric_limits<float32_t>::infinity();
        CHECK_THROWS_WITH(pose.Evaluate(valid_body, {}, root_matrix), Catch::Matchers::ContainsSubstring("root matrix"));
    }
}

TEST_CASE("ModelAnimationConverterProductionRigDataRejectsWrongSkeletonOzzTypeTag")
{
    ModelAnimationRigData rig_data = BuildModelAnimationProductionTestRigData();
    REQUIRE_FALSE(rig_data.Clips.empty());
    rig_data.Skeleton.Payload = rig_data.Clips.front().Animation.Payload;
    const vector<uint8_t> serialized = WriteModelAnimationRigData(rig_data, "wrong skeleton type test");
    CHECK_THROWS_WITH(LoadModelAnimationRuntimeRig(serialized, "Models/ProductionBody.fo3d", "Models/ProductionBody.fbx", false), Catch::Matchers::ContainsSubstring("wrong type tag"));
}

TEST_CASE("ModelAnimationConverterProductionRigDataRejectsInterpolationPolicyMismatch")
{
    const ModelAnimationRigData rig_data = BuildModelAnimationProductionTestRigData();
    const vector<uint8_t> serialized = WriteModelAnimationRigData(rig_data, "interpolation policy mismatch test");
    CHECK_THROWS_WITH(LoadModelAnimationRuntimeRig(serialized, "Models/ProductionBody.fo3d", "Models/ProductionBody.fbx", true), Catch::Matchers::ContainsSubstring("nearest-sampling timeline"));
}

TEST_CASE("ModelAnimationRuntimeResolvesCanonicalRigAgainstSourceDfsOrder")
{
    const mat44 root_rest = glm::translate(mat44 {1.0f}, vec3 {1.0f, 2.0f, 3.0f});
    const mat44 z_branch_rest = glm::translate(mat44 {1.0f}, vec3 {0.0f, 4.0f, 0.0f});
    const mat44 z_child_rest = glm::translate(mat44 {1.0f}, vec3 {0.0f, 0.0f, 5.0f});
    const mat44 a_branch_rest = glm::translate(mat44 {1.0f}, vec3 {6.0f, 0.0f, 0.0f});
    const mat44 z_leaf_rest = glm::translate(mat44 {1.0f}, vec3 {0.0f, 7.0f, 0.0f});

    ModelSkeletonSource base;
    base.FileName = "Models/HierarchyOrder.fbx";
    base.Joints = {
        MakeModelAnimationTestJoint("Root", {"Root"}, root_rest),
        MakeModelAnimationTestJoint("ZBranch", {"Root", "ZBranch"}, z_branch_rest),
        MakeModelAnimationTestJoint("ZChild", {"Root", "ZBranch", "ZChild"}, z_child_rest),
        MakeModelAnimationTestJoint("ABranch", {"Root", "ABranch"}, a_branch_rest),
        MakeModelAnimationTestJoint("ZLeaf", {"Root", "ABranch", "ZLeaf"}, z_leaf_rest),
    };

    ModelSkeletonClipSource contributed;
    contributed.FileName = "Animations/HierarchyOrder.fbx";
    contributed.ClipName = "HelperOnly";
    contributed.Joints = {
        MakeModelAnimationTestJoint("Root", {"Root"}, root_rest),
        MakeModelAnimationTestJoint("ABranch", {"Root", "ABranch"}, a_branch_rest),
        MakeModelAnimationTestJoint("Helper", {"Root", "ABranch", "Helper"}),
    };
    contributed.AnimatedJointHierarchies = {{"Root", "ABranch", "Helper"}};

    const vector<ModelSkeletonClipSource> contributed_clips {contributed};
    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, contributed_clips);
    ModelAnimationRigArtifacts artifacts = BuildModelAnimationRigArtifacts("Models/HierarchyOrder.fo3d", base, report, {}, false);
    CHECK(artifacts.BaseJointRemap.SourceToCanonicalJointIndices == vector<uint32_t> {0, 4, 5, 1, 3});
    CHECK(artifacts.BaseJointRemap.CanonicalJointPresent == vector<uint8_t> {1, 1, 0, 1, 1, 1});

    const ModelAnimationRigData rig_data = BuildModelAnimationRigData(std::move(artifacts), {});
    const vector<uint8_t> serialized = WriteModelAnimationRigData(rig_data, "runtime hierarchy-order test");
    const unique_ptr<ModelAnimationRuntimeRig> runtime_rig = LoadModelAnimationRuntimeRig(serialized, "Models/HierarchyOrder.fo3d", base.FileName, false);
    REQUIRE(runtime_rig->GetJointCount() == 6);
    vector<string> joint_names;
    joint_names.reserve(runtime_rig->GetJointCount());

    for (size_t joint = 0; joint < runtime_rig->GetJointCount(); joint++) {
        joint_names.emplace_back(runtime_rig->GetJointName(joint));
    }

    CHECK(joint_names == vector<string> {"Root", "ABranch", "Helper", "ZLeaf", "ZBranch", "ZChild"});

    const vector<ModelAnimationRuntimeJoint> source_joints {
        ModelAnimationRuntimeJoint {"Root", -1, root_rest},
        ModelAnimationRuntimeJoint {"ZBranch", 0, z_branch_rest},
        ModelAnimationRuntimeJoint {"ZChild", 1, z_child_rest},
        ModelAnimationRuntimeJoint {"ABranch", 0, a_branch_rest},
        ModelAnimationRuntimeJoint {"ZLeaf", 3, z_leaf_rest},
    };
    CHECK_NOTHROW(ValidateModelAnimationRuntimeBaseJoints(*runtime_rig, source_joints, "source DFS fixture"));

    vector<ModelAnimationRuntimeJoint> materialized_joints = source_joints;
    materialized_joints.emplace_back(ModelAnimationRuntimeJoint {"Helper", 3, mat44 {1.0f}});
    materialized_joints.emplace_back(ModelAnimationRuntimeJoint {"Helper", 1, mat44 {1.0f}});
    CHECK(ResolveModelAnimationRuntimeCanonicalJoints(*runtime_rig, materialized_joints, "materialized fixture") == vector<uint32_t> {0, 3, 5, 4, 1, 2});

    SECTION("duplicate direct child is rejected")
    {
        vector<ModelAnimationRuntimeJoint> invalid = materialized_joints;
        invalid.emplace_back(ModelAnimationRuntimeJoint {"Helper", 3, mat44 {1.0f}});
        CHECK_THROWS_WITH(ResolveModelAnimationRuntimeCanonicalJoints(*runtime_rig, invalid, "duplicate direct child fixture"), Catch::Matchers::ContainsSubstring("duplicate direct child"));
    }

    SECTION("equal count with wrong name is rejected")
    {
        vector<ModelAnimationRuntimeJoint> invalid = source_joints;
        invalid[2].Name = "WrongZChild";
        CHECK_THROWS_WITH(ValidateModelAnimationRuntimeBaseJoints(*runtime_rig, invalid, "wrong name fixture"), Catch::Matchers::ContainsSubstring("name mismatch"));
    }

    SECTION("wrong parent is rejected")
    {
        vector<ModelAnimationRuntimeJoint> invalid = source_joints;
        invalid[2].ParentIndex = 0;
        CHECK_THROWS_WITH(ValidateModelAnimationRuntimeBaseJoints(*runtime_rig, invalid, "wrong parent fixture"), Catch::Matchers::ContainsSubstring("parent mismatch"));
    }

    SECTION("wrong rest pose is rejected")
    {
        vector<ModelAnimationRuntimeJoint> invalid = source_joints;
        invalid[4].RestLocalTransform[3][0] += 1.0f;
        CHECK_THROWS_WITH(ValidateModelAnimationRuntimeBaseJoints(*runtime_rig, invalid, "wrong rest fixture"), Catch::Matchers::ContainsSubstring("rest-pose matrix component mismatch"));
    }
}

TEST_CASE("ModelAnimationConverterBuildsCanonicalRigRemapPresenceAndFallbacks")
{
    const mat44 root_rest = glm::translate(mat44 {1.0f}, vec3 {10.0f, 0.0f, 0.0f});
    const mat44 z_base_rest = glm::translate(mat44 {1.0f}, vec3 {0.0f, 0.0f, 3.0f});
    ModelSkeletonSource base;
    base.FileName = "Models/Body.fbx";
    base.Joints = {
        MakeModelAnimationTestJoint("", {""}, root_rest),
        MakeModelAnimationTestJoint("ZBase", {"", "ZBase"}, z_base_rest),
    };

    ModelSkeletonClipSource clip_source;
    clip_source.FileName = "Animations/Body.fbx";
    clip_source.ClipName = "Idle";
    clip_source.Joints = {
        MakeModelAnimationTestJoint("Armature", {"Armature"}),
        MakeModelAnimationTestJoint("A", {"Armature", "A"}, glm::translate(mat44 {1.0f}, vec3 {40.0f, 0.0f, 0.0f})),
        MakeModelAnimationTestJoint("Leaf", {"Armature", "A", "Leaf"}, glm::translate(mat44 {1.0f}, vec3 {50.0f, 0.0f, 0.0f})),
    };
    clip_source.AnimatedJointHierarchies = {{"Armature", "A", "Leaf"}};
    const vector<ModelSkeletonClipSource> clip_sources {clip_source};
    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, clip_sources);

    ModelAnimationSource animation;
    animation.FileName = clip_source.FileName;
    animation.Name = clip_source.ClipName;
    animation.Duration = 1.0f;
    animation.Joints = {
        MakeModelAnimationTestAnimationJoint("Leaf", {"Armature", "A", "Leaf"}, {0.0f, 1.0f}, {vec3 {0.0f, 2.0f, 0.0f}, vec3 {0.0f, 2.0f, 0.0f}}),
    };

    ModelAnimationSource base_animation;
    base_animation.FileName = base.FileName;
    base_animation.Name = "BaseIdle";
    base_animation.Duration = 1.0f;
    base_animation.Joints = {
        MakeModelAnimationTestAnimationJoint("ZBase", {"", "ZBase"}, {0.0f, 1.0f}, {vec3 {0.0f}, vec3 {0.0f}}),
    };

    const vector<ModelAnimationSource> animations {animation, base_animation};
    const vector<ModelAnimationSource> reordered_animations {base_animation, animation};

    ModelAnimationRigArtifacts artifacts = BuildModelAnimationRigArtifacts("Models/Body.fo3d", base, report, animations, false);
    const ModelAnimationRigArtifacts reordered = BuildModelAnimationRigArtifacts("Models/Body.fo3d", base, report, reordered_animations, false);
    REQUIRE(artifacts.Clips.size() == 2);
    REQUIRE(reordered.Clips.size() == artifacts.Clips.size());
    CHECK(artifacts.RigSignature == reordered.RigSignature);
    CHECK(artifacts.CacheSignature == reordered.CacheSignature);
    CHECK(artifacts.SkeletonMetadata.SourceSignature == reordered.SkeletonMetadata.SourceSignature);
    CHECK(artifacts.SkeletonArchive == reordered.SkeletonArchive);
    CHECK(artifacts.BaseJointRemapArchive == reordered.BaseJointRemapArchive);

    for (size_t i = 0; i < artifacts.Clips.size(); i++) {
        CHECK(artifacts.Clips[i].SourceFile == reordered.Clips[i].SourceFile);
        CHECK(artifacts.Clips[i].ClipName == reordered.Clips[i].ClipName);
        CHECK(artifacts.Clips[i].AnimationMetadata.SourceSignature == reordered.Clips[i].AnimationMetadata.SourceSignature);
        CHECK(artifacts.Clips[i].AnimationArchive == reordered.Clips[i].AnimationArchive);
        CHECK(artifacts.Clips[i].JointRemapArchive == reordered.Clips[i].JointRemapArchive);
    }

    CHECK(artifacts.Clips.front().SourceFile == animation.FileName);
    CHECK(artifacts.Clips.back().SourceFile == base_animation.FileName);
    CHECK(artifacts.Clips.front().JointRemap.SourceToCanonicalJointIndices == vector<uint32_t> {2});
    CHECK(artifacts.Clips.front().JointRemap.CanonicalJointPresent == vector<uint8_t> {0, 0, 1, 0});
    CHECK(artifacts.Clips.back().JointRemap.SourceToCanonicalJointIndices == vector<uint32_t> {3});
    CHECK(artifacts.Clips.back().JointRemap.CanonicalJointPresent == vector<uint8_t> {0, 0, 0, 1});
    CHECK(artifacts.BaseJointRemap.SourceToCanonicalJointIndices == vector<uint32_t> {0, 3});
    CHECK(artifacts.BaseJointRemap.CanonicalJointPresent == vector<uint8_t> {1, 0, 0, 1});

    ModelAnimationSource changed_animation = animation;
    changed_animation.Joints.front().Translation.Values.front().y = 3.0f;
    const ModelAnimationRigArtifacts changed_source = BuildModelAnimationRigArtifacts("Models/Body.fo3d", base, report, vector<ModelAnimationSource> {changed_animation, base_animation}, false);
    CHECK(artifacts.Clips.front().AnimationMetadata.SourceSignature != changed_source.Clips.front().AnimationMetadata.SourceSignature);
    CHECK(artifacts.Clips.front().AnimationArchive != changed_source.Clips.front().AnimationArchive);

    const ModelAnimationRigArtifacts nearest_policy = BuildModelAnimationRigArtifacts("Models/Body.fo3d", base, report, animations, true);
    CHECK(artifacts.CacheSignature != nearest_policy.CacheSignature);

    const unique_ptr<ModelAnimationRuntimeRig> runtime_rig = LoadModelAnimationRuntimeTestRig("Models/Body.fo3d", base.FileName, std::move(artifacts), false);
    REQUIRE(runtime_rig->GetJointCount() == 4);
    CHECK(runtime_rig->GetJointName(0).empty());
    CHECK(runtime_rig->GetJointName(1) == "A");
    CHECK(runtime_rig->GetJointName(2) == "Leaf");
    CHECK(runtime_rig->GetJointName(3) == "ZBase");
    const auto matrices = SampleModelAnimationRuntimeTestMatrices(*runtime_rig, 0, 0.5f);
    const auto root_translation = GetModelAnimationTestColumn(matrices[0], 3);
    const auto contributed_parent_translation = GetModelAnimationTestColumn(matrices[1], 3);
    const auto leaf_translation = GetModelAnimationTestColumn(matrices[2], 3);
    const auto base_child_translation = GetModelAnimationTestColumn(matrices[3], 3);
    CHECK(root_translation[0] == Catch::Approx(10.0f));
    CHECK(contributed_parent_translation[0] == Catch::Approx(10.0f));
    CHECK(leaf_translation[0] == Catch::Approx(10.0f).margin(1.0e-3f));
    CHECK(leaf_translation[1] == Catch::Approx(2.0f).margin(1.0e-3f));
    CHECK(base_child_translation[0] == Catch::Approx(10.0f).margin(1.0e-3f));
    CHECK(base_child_translation[2] == Catch::Approx(3.0f).margin(1.0e-3f));
}

TEST_CASE("ModelAnimationConverterPreservesSignedRestAndPlaybackBoundaries")
{
    const quaternion rest_rotation = glm::angleAxis(glm::radians(37.0f), glm::normalize(vec3 {1.0f, 2.0f, 3.0f}));
    const mat44 mirrored_rest = glm::translate(mat44 {1.0f}, vec3 {4.0f, 5.0f, 6.0f}) * glm::mat4_cast(rest_rotation) * glm::scale(mat44 {1.0f}, vec3 {-2.0f, 3.0f, 4.0f});
    ModelSkeletonSource base;
    base.FileName = "Models/Mirrored.fbx";
    base.Joints = {MakeModelAnimationTestJoint("Root", {"Root"}, mirrored_rest)};
    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, {});

    ModelAnimationSource animation;
    animation.FileName = base.FileName;
    animation.Name = "Boundary";
    animation.Duration = 1.0f;
    const quaternion rotation_start = quaternion {1.0f, 0.0f, 0.0f, 0.0f};
    const quaternion rotation_end = glm::angleAxis(glm::radians(136.0f), vec3 {0.0f, 0.0f, 1.0f});
    ModelAnimationJointSource root_output;
    root_output.OutputName = "Root";
    root_output.Hierarchy = {"Root"};
    root_output.Translation.Times = {-1.0f, 0.5f, 2.0f};
    root_output.Translation.Values = {vec3 {0.0f}, vec3 {30.0f, 0.0f, 0.0f}, vec3 {40.0f, 0.0f, 0.0f}};
    root_output.Rotation.Times = {0.0f, 1.0f};
    root_output.Rotation.Values = {rotation_start, rotation_end};
    root_output.Scale.Times = {-1.0f, 0.5f, 2.0f};
    root_output.Scale.Values = {vec3 {1.0f}, vec3 {3.0f}, vec3 {2.0f}};
    animation.Joints = {root_output};
    const vector<ModelAnimationSource> animations {animation};
    ModelAnimationRigArtifacts artifacts = BuildModelAnimationRigArtifacts("Models/Mirrored.fo3d", base, report, animations, false);
    const unique_ptr<ModelAnimationRuntimeRig> runtime_rig = LoadModelAnimationRuntimeTestRig("Models/Mirrored.fo3d", base.FileName, std::move(artifacts), false);

    const auto start_matrix = SampleModelAnimationRuntimeTestMatrices(*runtime_rig, 0, 0.0f).front();
    const auto quarter_matrix = SampleModelAnimationRuntimeTestMatrices(*runtime_rig, 0, 0.25f).front();
    const auto middle_matrix = SampleModelAnimationRuntimeTestMatrices(*runtime_rig, 0, 0.5f).front();
    const auto end_matrix = SampleModelAnimationRuntimeTestMatrices(*runtime_rig, 0, 1.0f).front();
    const auto start_translation = GetModelAnimationTestColumn(start_matrix, 3);
    const auto quarter_translation = GetModelAnimationTestColumn(quarter_matrix, 3);
    const auto middle_translation = GetModelAnimationTestColumn(middle_matrix, 3);
    const auto end_translation = GetModelAnimationTestColumn(end_matrix, 3);
    const auto start_x_axis = GetModelAnimationTestColumn(start_matrix, 0);
    const auto quarter_x_axis = GetModelAnimationTestColumn(quarter_matrix, 0);
    const auto middle_x_axis = GetModelAnimationTestColumn(middle_matrix, 0);
    const auto end_x_axis = GetModelAnimationTestColumn(end_matrix, 0);
    const float32_t start_scale = glm::length(vec3 {start_x_axis[0], start_x_axis[1], start_x_axis[2]});
    const float32_t quarter_scale = glm::length(vec3 {quarter_x_axis[0], quarter_x_axis[1], quarter_x_axis[2]});
    const float32_t middle_scale = glm::length(vec3 {middle_x_axis[0], middle_x_axis[1], middle_x_axis[2]});
    const float32_t end_scale = glm::length(vec3 {end_x_axis[0], end_x_axis[1], end_x_axis[2]});
    CHECK(start_translation[0] == Catch::Approx(20.0f).margin(2.0e-2f));
    CHECK(quarter_translation[0] == Catch::Approx(25.0f).margin(2.0e-2f));
    CHECK(middle_translation[0] == Catch::Approx(30.0f).margin(2.0e-2f));
    CHECK(end_translation[0] == Catch::Approx(33.333333f).margin(2.0e-2f));
    CHECK(start_scale == Catch::Approx(2.333333f).margin(3.0e-3f));
    CHECK(quarter_scale == Catch::Approx(2.666667f).margin(3.0e-3f));
    CHECK(middle_scale == Catch::Approx(3.0f).margin(3.0e-3f));
    CHECK(end_scale == Catch::Approx(2.666667f).margin(3.0e-3f));
    CHECK(quarter_x_axis[0] / quarter_scale == Catch::Approx(std::cos(glm::radians(34.0f))).margin(3.0e-3f));
    CHECK(quarter_x_axis[1] / quarter_scale == Catch::Approx(std::sin(glm::radians(34.0f))).margin(3.0e-3f));

    ModelAnimationSource rest_only;
    rest_only.FileName = base.FileName;
    rest_only.Name = "RestOnly";
    rest_only.Duration = 1.0f;
    const vector<ModelAnimationSource> rest_animations {rest_only};
    ModelAnimationRigArtifacts rest_artifacts = BuildModelAnimationRigArtifacts("Models/MirroredRest.fo3d", base, report, rest_animations, false);
    const unique_ptr<ModelAnimationRuntimeRig> rest_runtime_rig = LoadModelAnimationRuntimeTestRig("Models/MirroredRest.fo3d", base.FileName, std::move(rest_artifacts), false);
    const auto skeleton_rest_matrix = GetModelAnimationRuntimeTestRestMatrices(*rest_runtime_rig).front();
    const auto rest_matrix = SampleModelAnimationRuntimeTestMatrices(*rest_runtime_rig, 0, 0.5f).front();

    for (mat44::length_type column = 0; column < 4; column++) {
        const auto skeleton_rest_column = GetModelAnimationTestColumn(skeleton_rest_matrix, numeric_cast<size_t>(column));
        const auto actual_column = GetModelAnimationTestColumn(rest_matrix, numeric_cast<size_t>(column));

        for (mat44::length_type row = 0; row < 4; row++) {
            CHECK(skeleton_rest_column[numeric_cast<size_t>(row)] == Catch::Approx(mirrored_rest[column][row]).margin(3.0e-3f));
            CHECK(actual_column[numeric_cast<size_t>(row)] == Catch::Approx(mirrored_rest[column][row]).margin(3.0e-3f));
        }
    }
}

TEST_CASE("ModelAnimationConverterBoundsRuntimeRotationErrorAgainstLegacySlerp")
{
    ModelSkeletonSource base;
    base.FileName = "Models/Rotation.fbx";
    base.Joints = {MakeModelAnimationTestJoint("Root", {"Root"})};
    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, {});
    const quaternion rotation_start {1.0f, 0.0f, 0.0f, 0.0f};
    const vector<pair<string, quaternion>> rotation_cases {
        {"wide arc", glm::angleAxis(glm::radians(136.0f), vec3 {0.0f, 0.0f, 1.0f})},
        {"near 180 with negated representation", -glm::angleAxis(glm::radians(179.0f), glm::normalize(vec3 {1.0f, 2.0f, 3.0f}))},
    };

    for (const auto& [case_name, rotation_end] : rotation_cases) {
        DYNAMIC_SECTION(case_name)
        {
            ModelAnimationSource animation;
            animation.FileName = base.FileName;
            animation.Name = "Rotate";
            animation.Duration = 1.0f;
            animation.Joints = {MakeModelAnimationTestAnimationJoint("Root", {"Root"}, {0.0f, 1.0f}, {vec3 {0.0f}, vec3 {0.0f}}, {rotation_start, rotation_end})};
            ModelAnimationRigArtifacts artifacts = BuildModelAnimationRigArtifacts("Models/Rotation.fo3d", base, report, vector<ModelAnimationSource> {animation}, false);
            const unique_ptr<ModelAnimationRuntimeRig> runtime_rig = LoadModelAnimationRuntimeTestRig("Models/Rotation.fo3d", base.FileName, std::move(artifacts), false);
            const ModelAnimationRuntimeClip& runtime_clip = runtime_rig->GetClip(0);
            ModelAnimationRuntimePose pose {runtime_rig.get()};
            array<ModelAnimationRuntimePose::TrackInput, 2> body {
                ModelAnimationRuntimePose::TrackInput {.ClipIndex = 0, .Enabled = true, .Weight = 1.0f},
                ModelAnimationRuntimePose::TrackInput {},
            };
            constexpr size_t sample_count = 97;

            for (size_t sample = 1; sample < sample_count; sample++) {
                const float32_t ratio = numeric_cast<float32_t>(sample) / numeric_cast<float32_t>(sample_count);
                const quaternion expected = glm::normalize(glm::slerp(rotation_start, rotation_end, ratio));
                body[0].Position = ratio * runtime_clip.GetDuration();
                pose.Evaluate(body, {}, mat44 {1.0f});
                const quaternion actual = pose.GetFinalLocalTransform(0).Rotation;
                const float64_t dot = numeric_cast<float64_t>(expected.w) * numeric_cast<float64_t>(actual.w) + numeric_cast<float64_t>(expected.x) * numeric_cast<float64_t>(actual.x) + numeric_cast<float64_t>(expected.y) * numeric_cast<float64_t>(actual.y) + numeric_cast<float64_t>(expected.z) * numeric_cast<float64_t>(actual.z);
                const float64_t cosine = std::clamp(std::abs(dot), 0.0, 1.0);
                const float64_t angular_error = 2.0 * std::acos(cosine);
                CAPTURE(case_name, ratio, angular_error);
                CHECK(angular_error <= glm::radians(0.1));
            }
        }
    }
}

TEST_CASE("ModelAnimationConverterValidatesNearestTimelineRootAliasAndNumericLimits")
{
    ModelSkeletonSource base;
    base.FileName = "Models/Body.fbx";
    base.Joints = {
        MakeModelAnimationTestJoint("", {""}),
        MakeModelAnimationTestJoint("Root", {"", "Root"}),
    };

    ModelSkeletonClipSource clip_source;
    clip_source.FileName = "Animations/Turret.fbx";
    clip_source.ClipName = "Idle";
    clip_source.Joints = base.Joints;
    clip_source.AnimatedJointHierarchies = {{"", "Root"}};
    const vector<ModelSkeletonClipSource> clip_sources {clip_source};
    const ModelSkeletonCompatibilityReport report = BuildModelSkeletonCompatibilityReport(base, clip_sources);

    ModelAnimationSource nearest;
    nearest.FileName = clip_source.FileName;
    nearest.Name = clip_source.ClipName;
    nearest.Duration = 1.0f;
    nearest.Joints = {MakeModelAnimationTestAnimationJoint("Root", {"", "Root"}, {0.0f, 0.25f, 1.0f}, {vec3 {0.0f}, vec3 {1.0f}, vec3 {2.0f}})};
    const vector<ModelAnimationSource> nearest_animations {nearest};
    const ModelAnimationRigArtifacts artifacts = BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, nearest_animations, true);
    REQUIRE(artifacts.Clips.size() == 1);
    CHECK(artifacts.Clips.front().JointRemap.NearestSampleTimes == vector<float32_t> {0.0f, 0.25f, 1.0f});
    CHECK(artifacts.Clips.front().JointRemap.CanonicalJointPresent == vector<uint8_t> {0, 1});

    ModelAnimationSource cropped_nearest = nearest;
    cropped_nearest.Joints.front().Translation.Times = {-1.0f, 0.5f, 2.0f};
    cropped_nearest.Joints.front().Rotation.Times = {-1.0f, 0.5f, 2.0f};
    cropped_nearest.Joints.front().Scale.Times = {-1.0f, 0.5f, 2.0f};
    CHECK_THROWS_AS(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {cropped_nearest}, true), ModelAnimationConverterException);

    ModelAnimationSource mismatched = nearest;
    mismatched.Joints.front().Rotation.Times = {0.0f, 0.5f, 1.0f};
    CHECK_THROWS_AS(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {mismatched}, true), ModelAnimationConverterException);

    ModelAnimationSource overflow = nearest;
    overflow.Joints.front().Translation.Values[1].x = 70000.0f;
    CHECK_THROWS_AS(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {overflow}, false), ModelAnimationConverterException);

    ModelAnimationSource non_unit = nearest;
    non_unit.Joints.front().Rotation.Values[1] = quaternion {2.0f, 0.0f, 0.0f, 0.0f};
    CHECK_THROWS_AS(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {non_unit}, false), ModelAnimationConverterException);

    ModelAnimationSource scale_overflow = nearest;
    scale_overflow.Joints.front().Scale.Values[1].x = 70000.0f;
    CHECK_THROWS_AS(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {scale_overflow}, false), ModelAnimationConverterException);

    ModelAnimationSource reciprocal_overflow = nearest;
    reciprocal_overflow.Duration = std::numeric_limits<float32_t>::denorm_min();
    CHECK_THROWS_WITH(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {reciprocal_overflow}, false), Catch::Matchers::ContainsSubstring("invalid duration"));

    const float32_t first_collapsed_time = std::numeric_limits<float32_t>::denorm_min();
    const float32_t second_collapsed_time = std::nextafter(first_collapsed_time, std::numeric_limits<float32_t>::infinity());
    const float32_t collapsed_duration = std::numeric_limits<float32_t>::max();
    const float32_t inverse_collapsed_duration = 1.0f / collapsed_duration;
    REQUIRE(second_collapsed_time > first_collapsed_time);
    REQUIRE(first_collapsed_time * inverse_collapsed_duration == second_collapsed_time * inverse_collapsed_duration);
    ModelAnimationSource collapsed_ratios = nearest;
    collapsed_ratios.Name = "CollapsedRatios";
    collapsed_ratios.Duration = collapsed_duration;
    collapsed_ratios.Joints = {MakeModelAnimationTestAnimationJoint("Root", {"", "Root"}, {0.0f, first_collapsed_time, second_collapsed_time, collapsed_duration}, {vec3 {0.0f}, vec3 {1.0f}, vec3 {2.0f}, vec3 {3.0f}})};
    CHECK_THROWS_WITH(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {collapsed_ratios}, true), Catch::Matchers::ContainsSubstring("timepoint ratio"));

    const auto make_smooth_timeline = [&base](string_view name, const vector<float32_t>& timeline) {
        ModelAnimationSource result;
        result.FileName = base.FileName;
        result.Name = name;
        result.Duration = 1.0f;
        result.Joints = {MakeModelAnimationTestAnimationJoint("Root", {"", "Root"}, timeline, vector<vec3>(timeline.size(), vec3 {0.0f}))};
        return result;
    };

    const ModelAnimationSource leading_gap = make_smooth_timeline("LeadingGap", {0.25f, 0.75f});
    CHECK_THROWS_AS(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {leading_gap}, false), ModelAnimationConverterException);

    const ModelAnimationSource wholly_before = make_smooth_timeline("WhollyBefore", {-2.0f, -1.0f});
    const ModelAnimationSource wholly_after = make_smooth_timeline("WhollyAfter", {2.0f, 3.0f});
    const ModelAnimationSource single_key = make_smooth_timeline("SingleKey", {0.5f});
    CHECK_NOTHROW(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {wholly_before}, false));
    CHECK_NOTHROW(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {wholly_after}, false));
    CHECK_NOTHROW(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {single_key}, false));

    ModelSkeletonSource non_affine = base;
    non_affine.Joints.front().RestLocalTransform[0][3] = 0.25f;
    CHECK_THROWS_AS(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", non_affine, report, {}, false), ModelAnimationConverterException);

    ModelSkeletonSource shear = base;
    shear.Joints.front().RestLocalTransform[1][0] = 0.25f;
    CHECK_THROWS_AS(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", shear, report, {}, false), ModelAnimationConverterException);

    ModelSkeletonSource zero_scale = base;
    zero_scale.Joints.front().RestLocalTransform[0][0] = 0.0f;
    CHECK_THROWS_AS(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", zero_scale, report, {}, false), ModelAnimationConverterException);

    ModelAnimationSource too_many_times = nearest;
    too_many_times.Name = "TooManyTimes";
    const size_t time_count = numeric_cast<size_t>(std::numeric_limits<uint16_t>::max()) + 1;
    vector<float32_t> times;
    times.reserve(time_count);

    for (size_t i = 0; i < time_count; i++) {
        times.emplace_back(numeric_cast<float32_t>(i) / numeric_cast<float32_t>(time_count - 1));
    }

    too_many_times.Joints.front().Translation.Times = times;
    too_many_times.Joints.front().Translation.Values.assign(time_count, vec3 {0.0f});
    too_many_times.Joints.front().Rotation.Times = times;
    too_many_times.Joints.front().Rotation.Values.assign(time_count, quaternion {1.0f, 0.0f, 0.0f, 0.0f});
    too_many_times.Joints.front().Scale.Times = times;
    too_many_times.Joints.front().Scale.Values.assign(time_count, vec3 {1.0f});
    CHECK_THROWS_WITH(BuildModelAnimationRigArtifacts("Models/Turret.fo3d", base, report, vector<ModelAnimationSource> {too_many_times}, true), Catch::Matchers::ContainsSubstring("65535"));

    ModelSkeletonClipSource alias_source;
    alias_source.FileName = "Animations/Alias.fbx";
    alias_source.ClipName = "RootMotion";
    alias_source.Joints = {MakeModelAnimationTestJoint("Armature", {"Armature"})};
    alias_source.AnimatedJointHierarchies = {{"Armature"}};
    const vector<ModelSkeletonClipSource> alias_sources {alias_source};
    const ModelSkeletonCompatibilityReport alias_report = BuildModelSkeletonCompatibilityReport(base, alias_sources);
    ModelAnimationSource alias_animation;
    alias_animation.FileName = alias_source.FileName;
    alias_animation.Name = alias_source.ClipName;
    alias_animation.Duration = 1.0f;
    alias_animation.Joints = {MakeModelAnimationTestAnimationJoint("Armature", {"Armature"}, {0.0f, 1.0f}, {vec3 {0.0f}, vec3 {1.0f}})};
    CHECK_THROWS_AS(BuildModelAnimationRigArtifacts("Models/Alias.fo3d", base, alias_report, vector<ModelAnimationSource> {alias_animation}, false), ModelAnimationConverterException);
}

TEST_CASE("ModelAnimationConverterRejectsCanonicalRigAboveOzzJointLimit")
{
    ModelSkeletonSource base;
    base.FileName = "Models/TooMany.fbx";
    base.Joints.reserve(MODEL_ANIMATION_RIG_MAX_JOINTS + 1);
    base.Joints.emplace_back(MakeModelAnimationTestJoint("Root", {"Root"}));

    for (uint32_t i = 1; i <= MODEL_ANIMATION_RIG_MAX_JOINTS; i++) {
        const string name = strex("Joint{:04}", i);
        ModelSkeletonJoint joint;
        joint.Name = name;
        joint.Hierarchy = {"Root", name};
        base.Joints.emplace_back(std::move(joint));
    }

    ModelSkeletonCompatibilityReport report;
    report.BaseFile = base.FileName;
    report.CanonicalJoints = base.Joints;
    CHECK_THROWS_WITH(BuildModelAnimationRigArtifacts("Models/TooMany.fo3d", base, report, {}, false), Catch::Matchers::ContainsSubstring("1024"));
}

#endif

FO_END_NAMESPACE

#endif
