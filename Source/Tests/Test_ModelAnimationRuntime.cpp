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
#include "ModelHierarchy.h"

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

static void CheckModelAnimationRuntimeMatrix(const mat44& actual, const mat44& expected)
{
    FO_STACK_TRACE_ENTRY();

    for (mat44::length_type column = 0; column < actual.length(); column++) {
        for (mat44::length_type row = 0; row < actual[column].length(); row++) {
            CHECK(actual[column][row] == Catch::Approx(expected[column][row]).margin(1.0e-4f));
        }
    }
}

#endif

TEST_CASE("Direct model rest pose builds independent hierarchical world matrices")
{
#if FO_ENABLE_3D
    const mat44 root_local = glm::translate(mat44 {1.0f}, vec3 {1.0f, 2.0f, 3.0f}) * glm::rotate(mat44 {1.0f}, glm::radians(90.0f), vec3 {0.0f, 0.0f, 1.0f}) * glm::scale(mat44 {1.0f}, vec3 {2.0f, 3.0f, 4.0f});
    const mat44 child_local = glm::translate(mat44 {1.0f}, vec3 {4.0f, 5.0f, 6.0f}) * glm::rotate(mat44 {1.0f}, glm::radians(90.0f), vec3 {1.0f, 0.0f, 0.0f}) * glm::scale(mat44 {1.0f}, vec3 {0.5f, 2.0f, 1.0f});
    const mat44 leaf_local = glm::translate(mat44 {1.0f}, vec3 {-2.0f, 1.0f, 0.5f});
    const vector<ModelPoseJoint> joints {
        ModelPoseJoint {-1, root_local},
        ModelPoseJoint {0, child_local},
        ModelPoseJoint {1, leaf_local},
    };
    const mat44 first_root = glm::translate(mat44 {1.0f}, vec3 {100.0f, -20.0f, 5.0f}) * glm::rotate(mat44 {1.0f}, glm::radians(30.0f), vec3 {0.0f, 1.0f, 0.0f});
    const mat44 second_root = glm::translate(mat44 {1.0f}, vec3 {-50.0f, 30.0f, 9.0f}) * glm::scale(mat44 {1.0f}, vec3 {2.0f});
    vector<mat44> first_world(joints.size());
    vector<mat44> second_world(joints.size());

    BuildModelRestWorldMatrices(const_span<ModelPoseJoint> {joints}, first_root, span<mat44> {first_world});
    const vector<mat44> first_snapshot = first_world;
    BuildModelRestWorldMatrices(const_span<ModelPoseJoint> {joints}, second_root, span<mat44> {second_world});

    REQUIRE(first_world.data() != second_world.data());
    CheckModelAnimationRuntimeMatrix(first_world[0], first_root * root_local);
    CheckModelAnimationRuntimeMatrix(first_world[1], first_root * root_local * child_local);
    CheckModelAnimationRuntimeMatrix(first_world[2], first_root * root_local * child_local * leaf_local);
    CheckModelAnimationRuntimeMatrix(second_world[0], second_root * root_local);
    CheckModelAnimationRuntimeMatrix(second_world[1], second_root * root_local * child_local);

    for (size_t joint_index = 0; joint_index < first_world.size(); joint_index++) {
        CheckModelAnimationRuntimeMatrix(first_world[joint_index], first_snapshot[joint_index]);
    }
#endif
}

TEST_CASE("Direct model rest pose rejects invalid hierarchy and matrices")
{
#if FO_ENABLE_3D
    const mat44 identity {1.0f};

    SECTION("Empty hierarchy")
    {
        const vector<ModelPoseJoint> joints;
        vector<mat44> world_matrices;
        CHECK_THROWS_WITH(BuildModelRestWorldMatrices(const_span<ModelPoseJoint> {joints}, identity, span<mat44> {world_matrices}), Catch::Matchers::ContainsSubstring("has no joints"));
    }

    SECTION("Output count")
    {
        const vector<ModelPoseJoint> joints {ModelPoseJoint {-1, identity}};
        vector<mat44> world_matrices(2);
        CHECK_THROWS_WITH(BuildModelRestWorldMatrices(const_span<ModelPoseJoint> {joints}, identity, span<mat44> {world_matrices}), Catch::Matchers::ContainsSubstring("output matrix count does not match joint count"));
    }

    SECTION("Root parent")
    {
        const vector<ModelPoseJoint> joints {ModelPoseJoint {0, identity}};
        vector<mat44> world_matrices(1);
        CHECK_THROWS_WITH(BuildModelRestWorldMatrices(const_span<ModelPoseJoint> {joints}, identity, span<mat44> {world_matrices}), Catch::Matchers::ContainsSubstring("invalid parent"));
    }

    SECTION("Parent order")
    {
        const vector<ModelPoseJoint> joints {
            ModelPoseJoint {-1, identity},
            ModelPoseJoint {1, identity},
        };
        vector<mat44> world_matrices(joints.size());
        CHECK_THROWS_WITH(BuildModelRestWorldMatrices(const_span<ModelPoseJoint> {joints}, identity, span<mat44> {world_matrices}), Catch::Matchers::ContainsSubstring("parent-before-child"));
    }

    SECTION("Root matrix")
    {
        const vector<ModelPoseJoint> joints {ModelPoseJoint {-1, identity}};
        vector<mat44> world_matrices(1);
        mat44 invalid_root = identity;
        invalid_root[0][0] = std::numeric_limits<float32_t>::infinity();
        CHECK_THROWS_WITH(BuildModelRestWorldMatrices(const_span<ModelPoseJoint> {joints}, invalid_root, span<mat44> {world_matrices}), Catch::Matchers::ContainsSubstring("root matrix"));
    }

    SECTION("Rest local matrix")
    {
        mat44 invalid_rest = identity;
        invalid_rest[2][1] = std::numeric_limits<float32_t>::quiet_NaN();
        const vector<ModelPoseJoint> joints {ModelPoseJoint {-1, invalid_rest}};
        vector<mat44> world_matrices(1);
        CHECK_THROWS_WITH(BuildModelRestWorldMatrices(const_span<ModelPoseJoint> {joints}, identity, span<mat44> {world_matrices}), Catch::Matchers::ContainsSubstring("local matrix"));
    }

    SECTION("Computed world matrix")
    {
        mat44 large_root = identity;
        large_root[0][0] = std::numeric_limits<float32_t>::max();
        mat44 expanding_rest = identity;
        expanding_rest[0][0] = 2.0f;
        const vector<ModelPoseJoint> joints {ModelPoseJoint {-1, expanding_rest}};
        vector<mat44> world_matrices(1);
        CHECK_THROWS_WITH(BuildModelRestWorldMatrices(const_span<ModelPoseJoint> {joints}, large_root, span<mat44> {world_matrices}), Catch::Matchers::ContainsSubstring("world matrix"));
    }
#endif
}

TEST_CASE("Model pose canonical names resolve contributed joints without physical bones")
{
#if FO_ENABLE_3D
    HashStorage hashes;
    const hstring source_root = hashes.ToHashedString("SourceRoot");
    const hstring runtime_root = hashes.ToHashedString("ModelFile.fbx");
    const hstring spine = hashes.ToHashedString("Spine");
    const hstring contributed = hashes.ToHashedString("ContributedSocket");
    const vector<hstring> runtime_names {runtime_root, spine, contributed};
    ModelBone root_bone;
    ModelBone spine_bone;
    const vector<nptr<const ModelBone>> physical_bones {&root_bone, &spine_bone, nullptr};

    const unordered_map<hstring, uint32_t> joint_indexes = BuildModelPoseJointNameIndex(runtime_names, "canonical-test");
    REQUIRE(joint_indexes.at(contributed) == 2);
    CHECK_FALSE(physical_bones[joint_indexes.at(contributed)]);
    CHECK(joint_indexes.count(runtime_root) == 1);
    CHECK(joint_indexes.count(source_root) == 0);

    const hstring child_root = hashes.ToHashedString("ChildFile.fbx");
    const hstring child_only = hashes.ToHashedString("ChildOnly");
    const vector<hstring> child_runtime_names {child_root, contributed, spine, child_only};
    const vector<ModelPoseJointLink> links = ResolveModelPoseJointLinks(joint_indexes, child_runtime_names);
    REQUIRE(links.size() == 2);
    CHECK(links[0].ParentJointIndex == 2);
    CHECK(links[0].ChildJointIndex == 1);
    CHECK(links[1].ParentJointIndex == 1);
    CHECK(links[1].ChildJointIndex == 2);

    const vector<hstring> duplicate_names {spine, spine};
    CHECK_THROWS_WITH(BuildModelPoseJointNameIndex(duplicate_names, "duplicate-test"), Catch::Matchers::ContainsSubstring("duplicate runtime lookup name"));
#endif
}

FO_END_NAMESPACE

#endif
