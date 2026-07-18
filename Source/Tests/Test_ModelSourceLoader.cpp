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
#include "ModelMeshData.h"
#include "ModelSourceLoader.h"
#include "Test_BakerHelpers.h"
#endif

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

static auto MakeValidModelSourceAsset(string_view path, uint64_t write_time) -> ModelSourceAsset
{
    FO_STACK_TRACE_ENTRY();

    ModelSourceAsset result;
    result.FileName = path;
    result.WriteTime = write_time;
    result.Skeleton.FileName = path;
    result.Skeleton.Joints = {
        ModelSkeletonJoint {.Name = "Root", .Hierarchy = {"Root"}, .RestLocalTransform = mat44 {1.0f}},
        ModelSkeletonJoint {.Name = "Bone", .Hierarchy = {"Root", "Bone"}, .RestLocalTransform = mat44 {1.0f}},
    };

    ModelAnimationJointSource joint;
    joint.OutputName = "Bone";
    joint.Hierarchy = {"Root", "Bone"};
    joint.Translation.Times = {0.0f, 1.0f};
    joint.Translation.Values = {vec3 {0.0f}, vec3 {1.0f, 2.0f, 3.0f}};
    joint.Rotation.Times = {0.0f, 1.0f};
    joint.Rotation.Values = {quaternion {1.0f, 0.0f, 0.0f, 0.0f}, quaternion {1.0f, 0.0f, 0.0f, 0.0f}};
    joint.Scale.Times = {0.0f, 1.0f};
    joint.Scale.Values = {vec3 {1.0f}, vec3 {1.0f}};

    ModelAnimationSource animation;
    animation.FileName = path;
    animation.Name = "Take 001";
    animation.Duration = 1.0f;
    animation.Joints.emplace_back(std::move(joint));
    result.Animations.emplace_back(std::move(animation));
    return result;
}

TEST_CASE("ModelSourceAssetValidationRejectsInvalidSourceData")
{
    const ModelSourceAsset valid = MakeValidModelSourceAsset("Models/Test.fbx", 17);
    REQUIRE_NOTHROW(ValidateModelSourceAsset(valid));

    SECTION("NonFiniteRestTransform")
    {
        ModelSourceAsset asset = valid;
        asset.Skeleton.Joints[1].RestLocalTransform[3][0] = std::numeric_limits<float32_t>::quiet_NaN();
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("non-finite rest transform"));
    }

    SECTION("DuplicateHierarchy")
    {
        ModelSourceAsset asset = valid;
        asset.Skeleton.Joints.emplace_back(asset.Skeleton.Joints.back());
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("duplicate joint hierarchy"));
    }

    SECTION("DuplicateJointName")
    {
        ModelSourceAsset asset = valid;
        asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "Bone", .Hierarchy = {"Root", "Other", "Bone"}, .RestLocalTransform = mat44 {1.0f}});
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("duplicate joint name"));
    }

    SECTION("MultipleRootJoints")
    {
        ModelSourceAsset asset = valid;
        asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "OtherRoot", .Hierarchy = {"OtherRoot"}, .RestLocalTransform = mat44 {1.0f}});
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("multiple root joints"));
    }

    SECTION("MissingRootJoint")
    {
        ModelSourceAsset asset = valid;
        asset.Skeleton.Joints.front().Hierarchy = {"Parent", "Root"};
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("no root joint"));
    }

    SECTION("HierarchyNameMismatch")
    {
        ModelSourceAsset asset = valid;
        asset.Skeleton.Joints[1].Hierarchy.back() = "Other";
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("hierarchy ends with"));
    }

    SECTION("EmptyNonRootJointName")
    {
        ModelSourceAsset asset = valid;
        asset.Skeleton.Joints[1].Name.clear();
        asset.Skeleton.Joints[1].Hierarchy.back().clear();
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("Empty name"));
    }

    SECTION("EmptyJointHierarchy")
    {
        ModelSourceAsset asset = valid;
        asset.Skeleton.Joints[1].Hierarchy.clear();
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("empty hierarchy"));
    }

    SECTION("MissingHierarchyParent")
    {
        ModelSourceAsset asset = valid;
        asset.Skeleton.Joints[1].Hierarchy = {"Root", "Missing", "Bone"};
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("without a parent hierarchy"));
    }

    SECTION("HierarchyDepthLimit")
    {
        ModelSourceAsset asset = valid;
        vector<string> deep_hierarchy(MODEL_MESH_MAX_HIERARCHY_DEPTH + 1, "Bone");
        deep_hierarchy.front() = "Root";
        asset.Skeleton.Joints[1].Name = deep_hierarchy.back();
        asset.Skeleton.Joints[1].Hierarchy = deep_hierarchy;
        asset.Animations.front().Joints.front().OutputName = deep_hierarchy.back();
        asset.Animations.front().Joints.front().Hierarchy = deep_hierarchy;
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("joint hierarchy is too deep"));
    }

    SECTION("DuplicateClipName")
    {
        ModelSourceAsset asset = valid;
        ModelAnimationSource duplicate = asset.Animations.front();
        duplicate.Name = "take 001";
        asset.Animations.emplace_back(std::move(duplicate));
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("case-insensitive duplicate animation names"));
    }

    SECTION("InvalidDuration")
    {
        ModelSourceAsset asset = valid;
        asset.Animations.front().Duration = 0.0f;
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("invalid duration"));
    }

    SECTION("NonAscendingKeyTimes")
    {
        ModelSourceAsset asset = valid;
        asset.Animations.front().Joints.front().Translation.Times[1] = 0.0f;
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("non-ascending track times"));
    }

    SECTION("InvalidKeyTimeIntervals")
    {
        ModelSourceAsset overflow = valid;
        overflow.Animations.front().Joints.front().Translation.Times = {std::numeric_limits<float32_t>::lowest(), std::numeric_limits<float32_t>::max()};
        CHECK_THROWS_WITH(ValidateModelSourceAsset(overflow), Catch::Matchers::ContainsSubstring("invalid track time interval"));

        ModelSourceAsset reciprocal_overflow = valid;
        reciprocal_overflow.Animations.front().Joints.front().Translation.Times = {0.0f, std::numeric_limits<float32_t>::denorm_min()};
        CHECK_THROWS_WITH(ValidateModelSourceAsset(reciprocal_overflow), Catch::Matchers::ContainsSubstring("invalid track time interval"));
    }

    SECTION("NonFiniteKey")
    {
        ModelSourceAsset asset = valid;
        asset.Animations.front().Joints.front().Translation.Values[1].x = std::numeric_limits<float32_t>::infinity();
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("non-finite track value"));
    }

    SECTION("Fp16Overflow")
    {
        ModelSourceAsset asset = valid;
        asset.Animations.front().Joints.front().Scale.Values[1].z = MODEL_ANIMATION_HALF_MAX + 1.0f;
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("outside the ozz FP16 range"));
    }

    SECTION("NonUnitQuaternion")
    {
        ModelSourceAsset asset = valid;
        asset.Animations.front().Joints.front().Rotation.Values[1] = quaternion {2.0f, 0.0f, 0.0f, 0.0f};
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("non-unit rotation value"));
    }

    SECTION("AnimationHierarchyAbsentFromSkeleton")
    {
        ModelSourceAsset asset = valid;
        asset.Animations.front().Joints.front().OutputName = "Other";
        asset.Animations.front().Joints.front().Hierarchy = {"Root", "Other"};
        CHECK_THROWS_WITH(ValidateModelSourceAsset(asset), Catch::Matchers::ContainsSubstring("absent from its source skeleton"));
    }
}

TEST_CASE("ModelSourceAssetCacheLoadsOneSharedAssetForParallelCallers")
{
    BakerTests::MemoryFileSet source_files {"ModelSourceCache"};
    source_files.AddTextFile("Models/Test.fbx", "callback fixture", 73);
    const FileCollection files = source_files.GetFileSystem().GetAllFiles();
    std::atomic<size_t> load_count {};
    std::promise<void> load_entered_promise;
    std::future<void> load_entered = load_entered_promise.get_future();
    std::promise<void> release_load_promise;
    const std::shared_future<void> release_load = release_load_promise.get_future().share();
    bool load_released = false;
    const auto release_load_guard = scope_exit([&]() noexcept {
        if (!load_released) {
            safe_call([&] { release_load_promise.set_value(); });
        }
    });
    ModelSourceAssetCache cache {files, [&](string_view path, const File& file) {
                                     if (load_count.fetch_add(1) == 0) {
                                         load_entered_promise.set_value();
                                     }

                                     release_load.wait();
                                     return MakeValidModelSourceAsset(path, file.GetWriteTime());
                                 }};

    vector<std::future<shared_ptr<const ModelSourceAsset>>> loads;

    for (size_t i = 0; i < 8; i++) {
        loads.emplace_back(run_async(launch_async_only, strex("ModelSourceCacheSuccess{}", i), [&cache] { return cache.Get("Models/Test.fbx"); }));
    }

    REQUIRE(load_entered.wait_for(std::chrono::seconds {10}) == std::future_status::ready);
    release_load_promise.set_value();
    load_released = true;
    const shared_ptr<const ModelSourceAsset> first = loads.front().get();
    REQUIRE(first);
    CHECK(first->FileName == "Models/Test.fbx");
    CHECK(first->WriteTime == 73);

    for (size_t i = 1; i < loads.size(); i++) {
        CHECK(loads[i].get() == first);
    }

    CHECK(cache.Get("Models/Test.fbx") == first);
    CHECK(load_count.load() == 1);
}

TEST_CASE("ModelSourceAssetCacheFansOutOneLoadException")
{
    BakerTests::MemoryFileSet source_files {"ModelSourceCacheFailure"};
    source_files.AddTextFile("Models/Broken.fbx", "callback fixture", 11);
    const FileCollection files = source_files.GetFileSystem().GetAllFiles();
    std::atomic<size_t> load_count {};
    std::promise<void> load_entered_promise;
    std::future<void> load_entered = load_entered_promise.get_future();
    std::promise<void> release_load_promise;
    const std::shared_future<void> release_load = release_load_promise.get_future().share();
    bool load_released = false;
    const auto release_load_guard = scope_exit([&]() noexcept {
        if (!load_released) {
            safe_call([&] { release_load_promise.set_value(); });
        }
    });
    ModelSourceAssetCache cache {files, [&](string_view path, const File& file) -> ModelSourceAsset {
                                     ignore_unused(path, file);

                                     if (load_count.fetch_add(1) == 0) {
                                         load_entered_promise.set_value();
                                     }

                                     release_load.wait();
                                     throw ModelSourceLoaderException("synthetic model source load failure");
                                 }};

    vector<std::future<string>> loads;

    for (size_t i = 0; i < 6; i++) {
        loads.emplace_back(run_async(launch_async_only, strex("ModelSourceCacheFailure{}", i), [&cache] {
            try {
                (void)cache.Get("Models/Broken.fbx");
                return string {};
            }
            catch (const std::exception& ex) {
                return string {ex.what()};
            }
        }));
    }

    REQUIRE(load_entered.wait_for(std::chrono::seconds {10}) == std::future_status::ready);
    release_load_promise.set_value();
    load_released = true;

    for (auto& load : loads) {
        CHECK(load.get().find("synthetic model source load failure") != string::npos);
    }

    CHECK_THROWS_WITH(cache.Get("Models/Broken.fbx"), Catch::Matchers::ContainsSubstring("synthetic model source load failure"));
    CHECK(load_count.load() == 1);
}

TEST_CASE("ModelSourceAssetCacheRejectsMissingPathBeforeCallback")
{
    const FileCollection files {vector<FileHeader> {}};
    size_t load_count = 0;
    ModelSourceAssetCache cache {files, [&](string_view path, const File& file) {
                                     load_count++;
                                     return MakeValidModelSourceAsset(path, file.GetWriteTime());
                                 }};

    CHECK_THROWS_WITH(cache.Get("Models/Missing.fbx"), Catch::Matchers::ContainsSubstring("was not found"));
    CHECK_THROWS_WITH(cache.Get("Models/Missing.fbx"), Catch::Matchers::ContainsSubstring("was not found"));
    CHECK(load_count == 0);
}

TEST_CASE("LoadModelSourceAssetRejectsMalformedInput")
{
    BakerTests::MemoryFileSet source_files {"ModelSourceMalformed"};
    source_files.AddTextFile("Models/Malformed.fbx", "this is not an FBX file", 5);
    const File file = source_files.ReadFile("Models/Malformed.fbx");

    REQUIRE(file);
    CHECK_THROWS_WITH(LoadModelSourceAsset("Models/Malformed.fbx", file), Catch::Matchers::ContainsSubstring("Unable to load model source"));
}

TEST_CASE("LoadModelSourceAssetExtractsMinimalObjHierarchy")
{
    constexpr string_view source = R"(o Triangle
v 0.0 0.0 0.0
v 1.0 0.0 0.0
v 0.0 1.0 0.0
f 1 2 3
)";
    BakerTests::MemoryFileSet source_files {"ModelSourceObj"};
    source_files.AddTextFile("Models/Triangle.obj", source, 29);
    const File file = source_files.ReadFile("Models/Triangle.obj");

    REQUIRE(file);
    const ModelSourceAsset asset = LoadModelSourceAsset("Models/Triangle.obj", file);
    CHECK(asset.FileName == "Models/Triangle.obj");
    CHECK(asset.WriteTime == 29);
    CHECK(asset.Skeleton.FileName == asset.FileName);
    REQUIRE_FALSE(asset.Skeleton.Joints.empty());
    CHECK(asset.Skeleton.Joints.front().Hierarchy.size() == 1);
    CHECK(asset.Animations.empty());
}

TEST_CASE("LoadModelSourceAssetExtractsMinimalAsciiFbxHierarchy")
{
    constexpr string_view source = R"(; FBX 7.4.0 project file
FBXHeaderExtension:  {
    FBXHeaderVersion: 1003
    FBXVersion: 7400
    Creator: "FOnline ModelSourceLoader test"
}
Definitions:  {
    Version: 100
    Count: 1
    ObjectType: "Model" {
        Count: 1
    }
}
Objects:  {
    Model: 1000, "Model::TestRoot", "Null" {
        Version: 232
        Properties70:  {
            P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0
            P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0
            P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1
        }
        Shading: T
        Culling: "CullingOff"
    }
}
Connections:  {
    C: "OO",1000,0
}
)";
    BakerTests::MemoryFileSet source_files {"ModelSourceAsciiFbx"};
    source_files.AddTextFile("Models/TestRoot.fbx", source, 31);
    const File file = source_files.ReadFile("Models/TestRoot.fbx");

    REQUIRE(file);
    const ModelSourceAsset asset = LoadModelSourceAsset("Models/TestRoot.fbx", file);
    REQUIRE(asset.Skeleton.Joints.size() == 2);
    CHECK(asset.Skeleton.Joints[0].Hierarchy == vector<string> {""});
    CHECK(asset.Skeleton.Joints[1].Name == "TestRoot");
    CHECK(asset.Skeleton.Joints[1].Hierarchy == vector<string> {"", "TestRoot"});
    CHECK(asset.Animations.empty());
}

#endif

FO_END_NAMESPACE

#endif
