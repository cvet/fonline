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

#if FO_ENABLE_3D
#include "ModelAnimation.h"
#include "ModelAnimationConverter.h"
#endif

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

struct ModelAnimationRuntimeProceduralTestFixture
{
    unique_nptr<ModelAnimationRuntimeRig> Rig {};
    array<mat44, 4> RestLocals {};
    size_t RootJoint {};
    size_t BodyJoint {};
    size_t HeadJoint {};
    size_t LeafJoint {};
};

static auto MakeModelAnimationRuntimeProceduralTestMatrix(const vec3& translation, const quaternion& rotation, const vec3& scale) noexcept -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    return glm::translate(mat44 {1.0f}, translation) * glm::mat4_cast(rotation) * glm::scale(mat44 {1.0f}, scale);
}

static auto ComposeModelAnimationRuntimeProceduralTestTransform(const ModelAnimationRuntimeTransform& transform) noexcept -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    return MakeModelAnimationRuntimeProceduralTestMatrix(transform.Translation, transform.Rotation, transform.Scale);
}

static auto MakeModelAnimationRuntimeProceduralTestJoint(string_view name, initializer_list<string_view> hierarchy, const mat44& rest_local) -> ModelSkeletonJoint
{
    FO_STACK_TRACE_ENTRY();

    ModelSkeletonJoint result;
    result.Name = name;
    result.RestLocalTransform = rest_local;

    for (string_view hierarchy_name : hierarchy) {
        result.Hierarchy.emplace_back(hierarchy_name);
    }

    return result;
}

static void CheckModelAnimationRuntimeProceduralTestMatrix(const mat44& actual, const mat44& expected, float32_t margin = 3.0e-4f)
{
    FO_STACK_TRACE_ENTRY();

    for (mat44::length_type column = 0; column < 4; column++) {
        for (mat44::length_type row = 0; row < 4; row++) {
            CAPTURE(column, row, actual[column][row], expected[column][row]);
            CHECK(actual[column][row] == Catch::Approx(expected[column][row]).margin(margin));
        }
    }
}

static void CheckModelAnimationRuntimeProceduralTestMatrixExact(const mat44& actual, const mat44& expected)
{
    FO_STACK_TRACE_ENTRY();

    for (mat44::length_type column = 0; column < 4; column++) {
        for (mat44::length_type row = 0; row < 4; row++) {
            CAPTURE(column, row, actual[column][row], expected[column][row]);
            CHECK(actual[column][row] == expected[column][row]);
        }
    }
}

static auto BuildModelAnimationRuntimeProceduralTestFixture() -> ModelAnimationRuntimeProceduralTestFixture
{
    FO_STACK_TRACE_ENTRY();

    const array<vec3, 4> translations {
        vec3 {1.0f, 2.0f, -1.0f},
        vec3 {2.0f, -1.0f, 3.0f},
        vec3 {-0.5f, 4.0f, 1.0f},
        vec3 {0.0f, 2.0f, 0.5f},
    };
    const array<quaternion, 4> rotations {
        glm::angleAxis(glm::radians(17.0f), glm::normalize(vec3 {0.0f, 1.0f, 1.0f})),
        glm::angleAxis(glm::radians(23.0f), vec3 {1.0f, 0.0f, 0.0f}),
        glm::angleAxis(glm::radians(-19.0f), vec3 {0.0f, 0.0f, 1.0f}),
        glm::angleAxis(glm::radians(11.0f), glm::normalize(vec3 {1.0f, 1.0f, 0.0f})),
    };
    const array<vec3, 4> scales {
        vec3 {1.25f, 0.8f, 1.1f},
        vec3 {2.0f, 0.5f, 1.5f},
        vec3 {0.75f, 1.25f, 0.6f},
        vec3 {1.0f},
    };

    const array<mat44, 4> source_rest_locals {
        MakeModelAnimationRuntimeProceduralTestMatrix(translations[0], rotations[0], scales[0]),
        MakeModelAnimationRuntimeProceduralTestMatrix(translations[1], rotations[1], scales[1]),
        MakeModelAnimationRuntimeProceduralTestMatrix(translations[2], rotations[2], scales[2]),
        MakeModelAnimationRuntimeProceduralTestMatrix(translations[3], rotations[3], scales[3]),
    };
    constexpr string_view model_description = "Models/RuntimeProcedural.fo3d";
    ModelSkeletonSource base_skeleton;
    base_skeleton.FileName = "Models/RuntimeProcedural.fbx";
    base_skeleton.Joints = {
        MakeModelAnimationRuntimeProceduralTestJoint("Root", {"Root"}, source_rest_locals[0]),
        MakeModelAnimationRuntimeProceduralTestJoint("Body", {"Root", "Body"}, source_rest_locals[1]),
        MakeModelAnimationRuntimeProceduralTestJoint("Head", {"Root", "Body", "Head"}, source_rest_locals[2]),
        MakeModelAnimationRuntimeProceduralTestJoint("Leaf", {"Root", "Body", "Head", "Leaf"}, source_rest_locals[3]),
    };
    const ModelSkeletonCompatibilityReport compatibility_report = BuildModelSkeletonCompatibilityReport(base_skeleton, {});
    ModelAnimationRigArtifacts artifacts = BuildModelAnimationRigArtifacts(model_description, base_skeleton, compatibility_report, {}, false);
    const ModelAnimationRigData rig_data = BuildModelAnimationRigData(std::move(artifacts), {});
    const vector<uint8_t> serialized = WriteModelAnimationRigData(rig_data, "procedural runtime pose test");

    ModelAnimationRuntimeProceduralTestFixture fixture;
    fixture.Rig = LoadModelAnimationRuntimeRig(serialized, model_description, base_skeleton.FileName, false);
    REQUIRE(fixture.Rig);
    REQUIRE(fixture.Rig->GetJointCount() == source_rest_locals.size());
    REQUIRE(fixture.Rig->GetBaseJointMapping().size() == source_rest_locals.size());
    REQUIRE(fixture.Rig->GetBaseJointPresence().size() == source_rest_locals.size());
    fixture.RootJoint = fixture.Rig->GetBaseJointMapping()[0];
    fixture.BodyJoint = fixture.Rig->GetBaseJointMapping()[1];
    fixture.HeadJoint = fixture.Rig->GetBaseJointMapping()[2];
    fixture.LeafJoint = fixture.Rig->GetBaseJointMapping()[3];
    REQUIRE(fixture.Rig->GetJointName(fixture.RootJoint) == "Root");
    REQUIRE(fixture.Rig->GetJointName(fixture.BodyJoint) == "Body");
    REQUIRE(fixture.Rig->GetJointName(fixture.HeadJoint) == "Head");
    REQUIRE(fixture.Rig->GetJointName(fixture.LeafJoint) == "Leaf");

    for (size_t source_joint = 0; source_joint < source_rest_locals.size(); source_joint++) {
        const size_t runtime_joint = fixture.Rig->GetBaseJointMapping()[source_joint];
        REQUIRE(fixture.Rig->GetBaseJointPresence()[runtime_joint] == 1);
        fixture.RestLocals[runtime_joint] = source_rest_locals[source_joint];
    }

    return fixture;
}

#endif

TEST_CASE("ModelAnimationRuntimePoseAppliesBoundedProceduralPreRotations")
{
#if FO_ENABLE_3D
    ModelAnimationRuntimeProceduralTestFixture fixture = BuildModelAnimationRuntimeProceduralTestFixture();
    ModelAnimationRuntimePose pose {fixture.Rig.get()};
    const mat44 root_matrix = MakeModelAnimationRuntimeProceduralTestMatrix(vec3 {100.0f, -20.0f, 7.0f}, glm::angleAxis(glm::radians(31.0f), glm::normalize(vec3 {1.0f, 2.0f, 1.0f})), vec3 {0.9f, 1.1f, 1.3f});
    const quaternion body_rotation = glm::angleAxis(glm::radians(37.0f), vec3 {0.0f, 1.0f, 0.0f});
    const quaternion head_rotation = glm::angleAxis(glm::radians(-29.0f), vec3 {1.0f, 0.0f, 0.0f});
    const array<ModelAnimationRuntimePose::ProceduralLocalRotation, 2> procedural_rotations {
        ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = fixture.BodyJoint, .Rotation = body_rotation},
        ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = fixture.HeadJoint, .Rotation = head_rotation},
    };

    pose.Evaluate({}, {}, root_matrix, procedural_rotations);

    const mat44 expected_root = root_matrix * fixture.RestLocals[fixture.RootJoint];
    const mat44 expected_body = expected_root * glm::mat4_cast(body_rotation) * fixture.RestLocals[fixture.BodyJoint];
    const mat44 expected_head = expected_body * glm::mat4_cast(head_rotation) * fixture.RestLocals[fixture.HeadJoint];
    const mat44 expected_leaf = expected_head * fixture.RestLocals[fixture.LeafJoint];
    array<mat44, 4> expected_final_locals {mat44 {1.0f}, mat44 {1.0f}, mat44 {1.0f}, mat44 {1.0f}};
    expected_final_locals[fixture.RootJoint] = fixture.RestLocals[fixture.RootJoint];
    expected_final_locals[fixture.BodyJoint] = glm::mat4_cast(body_rotation) * fixture.RestLocals[fixture.BodyJoint];
    expected_final_locals[fixture.HeadJoint] = glm::mat4_cast(head_rotation) * fixture.RestLocals[fixture.HeadJoint];
    expected_final_locals[fixture.LeafJoint] = fixture.RestLocals[fixture.LeafJoint];
    array<mat44, 4> expected_world {mat44 {1.0f}, mat44 {1.0f}, mat44 {1.0f}, mat44 {1.0f}};
    expected_world[fixture.RootJoint] = expected_root;
    expected_world[fixture.BodyJoint] = expected_body;
    expected_world[fixture.HeadJoint] = expected_head;
    expected_world[fixture.LeafJoint] = expected_leaf;
    REQUIRE(pose.GetJointCount() == expected_world.size());

    for (size_t joint = 0; joint < expected_world.size(); joint++) {
        CheckModelAnimationRuntimeProceduralTestMatrix(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetBodyLocalTransform(joint)), fixture.RestLocals[joint]);
        CheckModelAnimationRuntimeProceduralTestMatrix(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetFinalLocalTransform(joint)), expected_final_locals[joint]);
        CheckModelAnimationRuntimeProceduralTestMatrix(pose.GetWorldMatrices()[joint], expected_world[joint]);
    }

    const vector<mat44> first_evaluation {pose.GetWorldMatrices().begin(), pose.GetWorldMatrices().end()};
    pose.Evaluate({}, {}, root_matrix, procedural_rotations);

    for (size_t joint = 0; joint < expected_world.size(); joint++) {
        CheckModelAnimationRuntimeProceduralTestMatrix(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetBodyLocalTransform(joint)), fixture.RestLocals[joint]);
        CheckModelAnimationRuntimeProceduralTestMatrix(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetFinalLocalTransform(joint)), expected_final_locals[joint]);
        CheckModelAnimationRuntimeProceduralTestMatrix(pose.GetWorldMatrices()[joint], first_evaluation[joint]);
        CheckModelAnimationRuntimeProceduralTestMatrix(pose.GetWorldMatrices()[joint], expected_world[joint]);
    }
#else
    SUCCEED();
#endif
}

TEST_CASE("ModelAnimationRuntimePoseRejectsInvalidProceduralRotationsBeforeMutation")
{
#if FO_ENABLE_3D
    ModelAnimationRuntimeProceduralTestFixture fixture = BuildModelAnimationRuntimeProceduralTestFixture();
    ModelAnimationRuntimePose pose {fixture.Rig.get()};
    const mat44 root_matrix = glm::translate(mat44 {1.0f}, vec3 {10.0f, 20.0f, 30.0f});
    pose.Evaluate({}, {}, root_matrix);
    const vector<mat44> before_world {pose.GetWorldMatrices().begin(), pose.GetWorldMatrices().end()};
    vector<mat44> before_body_locals;
    vector<mat44> before_final_locals;
    before_body_locals.reserve(pose.GetJointCount());
    before_final_locals.reserve(pose.GetJointCount());

    for (size_t joint = 0; joint < pose.GetJointCount(); joint++) {
        before_body_locals.emplace_back(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetBodyLocalTransform(joint)));
        before_final_locals.emplace_back(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetFinalLocalTransform(joint)));
    }

    SECTION("bounded count")
    {
        const array<ModelAnimationRuntimePose::ProceduralLocalRotation, 3> rotations {
            ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = fixture.RootJoint},
            ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = fixture.BodyJoint},
            ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = fixture.HeadJoint},
        };
        CHECK_THROWS_WITH(pose.Evaluate({}, {}, root_matrix, rotations), Catch::Matchers::ContainsSubstring("maximum"));
    }

    SECTION("joint index")
    {
        const array<ModelAnimationRuntimePose::ProceduralLocalRotation, 1> rotations {
            ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = pose.GetJointCount()},
        };
        CHECK_THROWS_WITH(pose.Evaluate({}, {}, root_matrix, rotations), Catch::Matchers::ContainsSubstring("outside"));
    }

    SECTION("finite quaternion")
    {
        const array<ModelAnimationRuntimePose::ProceduralLocalRotation, 1> rotations {
            ModelAnimationRuntimePose::ProceduralLocalRotation {
                .JointIndex = fixture.BodyJoint,
                .Rotation = quaternion {std::numeric_limits<float32_t>::quiet_NaN(), 0.0f, 0.0f, 0.0f},
            },
        };
        CHECK_THROWS_WITH(pose.Evaluate({}, {}, root_matrix, rotations), Catch::Matchers::ContainsSubstring("non-finite"));
    }

    SECTION("normalized quaternion")
    {
        const array<ModelAnimationRuntimePose::ProceduralLocalRotation, 1> rotations {
            ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = fixture.BodyJoint, .Rotation = quaternion {2.0f, 0.0f, 0.0f, 0.0f}},
        };
        CHECK_THROWS_WITH(pose.Evaluate({}, {}, root_matrix, rotations), Catch::Matchers::ContainsSubstring("not normalized"));
    }

    SECTION("duplicate joint")
    {
        const array<ModelAnimationRuntimePose::ProceduralLocalRotation, 2> rotations {
            ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = fixture.HeadJoint},
            ModelAnimationRuntimePose::ProceduralLocalRotation {.JointIndex = fixture.HeadJoint},
        };
        CHECK_THROWS_WITH(pose.Evaluate({}, {}, root_matrix, rotations), Catch::Matchers::ContainsSubstring("duplicate"));
    }

    for (size_t joint = 0; joint < before_world.size(); joint++) {
        CheckModelAnimationRuntimeProceduralTestMatrixExact(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetBodyLocalTransform(joint)), before_body_locals[joint]);
        CheckModelAnimationRuntimeProceduralTestMatrixExact(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetFinalLocalTransform(joint)), before_final_locals[joint]);
        CheckModelAnimationRuntimeProceduralTestMatrixExact(pose.GetWorldMatrices()[joint], before_world[joint]);
    }
#else
    SUCCEED();
#endif
}

TEST_CASE("ModelAnimationRuntimePoseWorldOverrideDoesNotRecomputeDescendants")
{
#if FO_ENABLE_3D
    ModelAnimationRuntimeProceduralTestFixture fixture = BuildModelAnimationRuntimeProceduralTestFixture();
    ModelAnimationRuntimePose pose {fixture.Rig.get()};
    const mat44 root_matrix = glm::translate(mat44 {1.0f}, vec3 {10.0f, 20.0f, 30.0f});
    const array<ModelAnimationRuntimePose::ProceduralLocalRotation, 1> procedural_rotations {
        ModelAnimationRuntimePose::ProceduralLocalRotation {
            .JointIndex = fixture.HeadJoint,
            .Rotation = glm::angleAxis(glm::radians(27.0f), vec3 {0.0f, 1.0f, 0.0f}),
        },
    };
    pose.Evaluate({}, {}, root_matrix, procedural_rotations);
    const vector<mat44> before_world {pose.GetWorldMatrices().begin(), pose.GetWorldMatrices().end()};
    vector<mat44> before_body_locals;
    vector<mat44> before_final_locals;
    before_body_locals.reserve(pose.GetJointCount());
    before_final_locals.reserve(pose.GetJointCount());

    for (size_t joint = 0; joint < pose.GetJointCount(); joint++) {
        before_body_locals.emplace_back(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetBodyLocalTransform(joint)));
        before_final_locals.emplace_back(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetFinalLocalTransform(joint)));
    }

    const mat44 override_matrix = MakeModelAnimationRuntimeProceduralTestMatrix(vec3 {-50.0f, 80.0f, 12.0f}, glm::angleAxis(glm::radians(61.0f), glm::normalize(vec3 {1.0f, 1.0f, 2.0f})), vec3 {1.7f, 0.6f, 2.2f});
    pose.OverrideWorldMatrix(fixture.BodyJoint, override_matrix);
    CheckModelAnimationRuntimeProceduralTestMatrixExact(pose.GetWorldMatrices()[fixture.BodyJoint], override_matrix);

    for (size_t joint : {fixture.RootJoint, fixture.HeadJoint, fixture.LeafJoint}) {
        CheckModelAnimationRuntimeProceduralTestMatrixExact(pose.GetWorldMatrices()[joint], before_world[joint]);
    }

    for (size_t joint = 0; joint < pose.GetJointCount(); joint++) {
        CheckModelAnimationRuntimeProceduralTestMatrixExact(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetBodyLocalTransform(joint)), before_body_locals[joint]);
        CheckModelAnimationRuntimeProceduralTestMatrixExact(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetFinalLocalTransform(joint)), before_final_locals[joint]);
    }

    mat44 invalid_matrix = override_matrix;
    invalid_matrix[2][1] = std::numeric_limits<float32_t>::infinity();
    CHECK_THROWS_WITH(pose.OverrideWorldMatrix(fixture.BodyJoint, invalid_matrix), Catch::Matchers::ContainsSubstring("world override matrix"));
    CHECK_THROWS_WITH(pose.OverrideWorldMatrix(pose.GetJointCount(), override_matrix), Catch::Matchers::ContainsSubstring("outside"));
    CheckModelAnimationRuntimeProceduralTestMatrixExact(pose.GetWorldMatrices()[fixture.BodyJoint], override_matrix);

    for (size_t joint = 0; joint < pose.GetJointCount(); joint++) {
        CheckModelAnimationRuntimeProceduralTestMatrixExact(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetBodyLocalTransform(joint)), before_body_locals[joint]);
        CheckModelAnimationRuntimeProceduralTestMatrixExact(ComposeModelAnimationRuntimeProceduralTestTransform(pose.GetFinalLocalTransform(joint)), before_final_locals[joint]);
    }
#else
    SUCCEED();
#endif
}

FO_END_NAMESPACE

#endif
