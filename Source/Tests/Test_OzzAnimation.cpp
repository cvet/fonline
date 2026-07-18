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

#include "ModelAnimationData.h"

#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/memory/allocator.h"
#include "ozz/base/memory/unique_ptr.h"

FO_BEGIN_NAMESPACE

TEST_CASE("Model animation allocator returns aligned storage")
{
#if FO_ENABLE_3D
    InitializeModelAnimationMemory();
    nptr<ozz::memory::Allocator> allocator {ozz::memory::default_allocator()};
    REQUIRE(allocator);

    constexpr array<size_t, 7> alignments {1, 2, 4, 8, 16, 32, 64};
    for (const size_t alignment : alignments) {
        nptr<void> block {allocator->Allocate(33, alignment)};
        REQUIRE(block);
        CHECK(block.as_uintptr() % alignment == 0);
        allocator->Deallocate(block.get());
    }
#else
    SUCCEED();
#endif
}

TEST_CASE("Ozz animation build archive sampling smoke")
{
#if FO_ENABLE_3D
    ozz::animation::offline::RawSkeleton raw_skeleton;
    raw_skeleton.roots.resize(1);
    ozz::animation::offline::RawSkeleton::Joint& root_joint = raw_skeleton.roots[0];
    root_joint.name = "Root";
    root_joint.children.resize(1);
    root_joint.children[0].name = "Child";

    ozz::animation::offline::SkeletonBuilder skeleton_builder;
    ozz::unique_ptr<ozz::animation::Skeleton> built_skeleton = skeleton_builder(raw_skeleton);
    REQUIRE(built_skeleton != nullptr);
    REQUIRE(built_skeleton->num_joints() == 2);

    ozz::animation::offline::RawAnimation raw_animation;
    raw_animation.name = "Translation";
    raw_animation.duration = 1.0f;
    raw_animation.tracks.resize(2);
    raw_animation.tracks[0].translations.push_back({0.0f, ozz::math::Float3 {0.0f, 0.0f, 0.0f}});
    raw_animation.tracks[0].translations.push_back({1.0f, ozz::math::Float3 {2.0f, 0.0f, 0.0f}});
    raw_animation.tracks[1].translations.push_back({0.0f, ozz::math::Float3 {0.0f, 3.0f, 0.0f}});

    ozz::animation::offline::AnimationBuilder animation_builder;
    ozz::unique_ptr<ozz::animation::Animation> built_animation = animation_builder(raw_animation);
    REQUIRE(built_animation != nullptr);
    REQUIRE(built_animation->num_tracks() == 2);

    ozz::io::MemoryStream stream;
    {
        ozz::io::OArchive output_archive {&stream};
        output_archive << *built_skeleton;
        output_archive << *built_animation;
    }
    REQUIRE(stream.Size() != 0);
    REQUIRE(stream.Seek(0, ozz::io::Stream::kSet) == 0);

    ozz::animation::Skeleton loaded_skeleton;
    ozz::animation::Animation loaded_animation;
    {
        ozz::io::IArchive input_archive {&stream};
        REQUIRE(input_archive.TestTag<ozz::animation::Skeleton>());
        input_archive >> loaded_skeleton;
        REQUIRE(input_archive.TestTag<ozz::animation::Animation>());
        input_archive >> loaded_animation;
    }

    REQUIRE(loaded_skeleton.num_joints() == 2);
    REQUIRE(loaded_animation.num_tracks() == 2);
    CHECK(string_view {loaded_skeleton.joint_names()[0]} == "Root");
    CHECK(string_view {loaded_skeleton.joint_names()[1]} == "Child");
    CHECK(string_view {loaded_animation.name()} == "Translation");

    ozz::animation::SamplingJob::Context context {loaded_animation.num_tracks()};
    ozz::vector<ozz::math::SoaTransform> local_transforms(numeric_cast<size_t>(loaded_skeleton.num_soa_joints()));
    ozz::animation::SamplingJob sampling_job;
    sampling_job.animation = &loaded_animation;
    sampling_job.context = &context;
    sampling_job.ratio = 0.5f;
    sampling_job.output = ozz::make_span(local_transforms);
    REQUIRE(sampling_job.Run());

    ozz::vector<ozz::math::Float4x4> model_matrices(numeric_cast<size_t>(loaded_skeleton.num_joints()));
    ozz::animation::LocalToModelJob local_to_model_job;
    local_to_model_job.skeleton = &loaded_skeleton;
    local_to_model_job.input = ozz::make_span(local_transforms);
    local_to_model_job.output = ozz::make_span(model_matrices);
    REQUIRE(local_to_model_job.Run());

    float32_t child_translation[4] {};
    ozz::math::StorePtrU(model_matrices[1].cols[3], child_translation);

    // Runtime animation keys are quantized by ozz, including after an archive round-trip.
    constexpr float32_t archive_compression_tolerance = 1.0e-3f;
    CHECK(child_translation[0] == Catch::Approx(1.0f).margin(archive_compression_tolerance));
    CHECK(child_translation[1] == Catch::Approx(3.0f).margin(archive_compression_tolerance));
    CHECK(child_translation[2] == Catch::Approx(0.0f).margin(archive_compression_tolerance));
    CHECK(child_translation[3] == Catch::Approx(1.0f).margin(archive_compression_tolerance));
#else
    SUCCEED();
#endif
}

FO_END_NAMESPACE

#endif
