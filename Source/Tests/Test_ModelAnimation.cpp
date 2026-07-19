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

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

static void ConfigureTrack(ModelAnimationController& controller, int32_t track, int32_t animation, float32_t position, float32_t speed, float32_t weight, nptr<const unordered_set<hstring>> allowed_bones = nullptr)
{
    FO_STACK_TRACE_ENTRY();

    controller.SetTrackAnimation(track, animation, allowed_bones);
    controller.SetTrackEnable(track, true);
    controller.SetTrackPosition(track, position);
    controller.SetTrackSpeed(track, speed);
    controller.AddEventWeight(track, weight, 0.0f, 0.0f);
    controller.AdvanceTimeline(0.0f);
}

#endif

TEST_CASE("Model animation controller copies share immutable metadata and own timeline state")
{
#if FO_ENABLE_3D
    HashStorage hashes;
    const hstring bone = hashes.ToHashedString("Bone");
    ModelAnimationController prototype {1};
    const int32_t first_index = prototype.RegisterAnimation(3, 2.0f, false, {bone});
    const int32_t second_index = prototype.RegisterAnimation(7, 4.0f, true, {bone});

    CHECK(prototype.GetAnimationsCount() == 2);
    CHECK(prototype.GetAnimationDuration(first_index) == Catch::Approx(2.0f));
    CHECK(prototype.GetAnimationDuration(second_index) == Catch::Approx(4.0f));

    ModelAnimationController first_instance = prototype.Copy();
    ModelAnimationController second_instance = prototype.Copy();
    ConfigureTrack(first_instance, 0, first_index, 0.25f, 1.0f, 1.0f);
    ConfigureTrack(second_instance, 0, second_index, 1.0f, 2.0f, 0.5f);
    first_instance.AdvanceTimeline(0.5f);
    second_instance.AdvanceTimeline(1.0f);

    const ModelAnimationController::TrackState first_track = first_instance.GetTrackState(0);
    const ModelAnimationController::TrackState second_track = second_instance.GetTrackState(0);
    CHECK(first_track.Position == Catch::Approx(0.75f));
    CHECK(first_track.ClipIndex == 3);
    CHECK_FALSE(first_track.Reversed);
    CHECK(second_track.Position == Catch::Approx(3.0f));
    CHECK(second_track.ClipIndex == 7);
    CHECK(second_track.Reversed);
    CHECK(first_instance.IsTrackBoneEnabled(0, bone));
    CHECK(second_instance.IsTrackBoneEnabled(0, bone));
#endif
}

TEST_CASE("Model animation controller timeline resolves events for runtime track state")
{
#if FO_ENABLE_3D
    HashStorage hashes;
    const hstring bone = hashes.ToHashedString("Bone");
    ModelAnimationController controller {1};
    const int32_t animation_index = controller.RegisterAnimation(5, 2.0f, true, {bone});
    controller.SetTrackAnimation(0, animation_index, nullptr);
    controller.SetTrackEnable(0, true);
    controller.SetTrackSpeed(0, 1.0f);
    controller.AddEventSpeed(0, 3.0f, 0.0f, 1.0f);
    controller.AddEventWeight(0, 1.0f, 0.0f, 1.0f);

    controller.AdvanceTimeline(0.5f);
    ModelAnimationController::TrackState track = controller.GetTrackState(0);
    CHECK(track.Enabled);
    CHECK(track.Speed == Catch::Approx(2.0f));
    CHECK(track.Weight == Catch::Approx(0.5f));
    CHECK(track.Position == Catch::Approx(1.0f));
    CHECK(track.ClipIndex == 5);
    CHECK(track.Reversed);

    controller.AdvanceTimeline(0.5f);
    track = controller.GetTrackState(0);
    CHECK(track.Speed == Catch::Approx(3.0f));
    CHECK(track.Weight == Catch::Approx(1.0f));
    CHECK(track.Position == Catch::Approx(2.5f));

    controller.ResetEvents();
    controller.AddEventEnable(0, false, 0.25f);
    controller.AdvanceTimeline(0.25f);
    CHECK_FALSE(controller.GetTrackEnable(0));
    CHECK(controller.GetTrackPosition(0) == Catch::Approx(2.5f));
#endif
}

TEST_CASE("Model animation track bindings preserve allowed masks and transition suppression")
{
#if FO_ENABLE_3D
    HashStorage hashes;
    const hstring bone = hashes.ToHashedString("Bone");
    const hstring other = hashes.ToHashedString("Other");
    const hstring missing = hashes.ToHashedString("Missing");
    ModelAnimationController controller {2};
    const int32_t first_index = controller.RegisterAnimation(11, 2.0f, true, {bone, other});
    const int32_t second_index = controller.RegisterAnimation(13, 2.0f, false, {bone, other});

    unordered_set<hstring> allowed_bones {bone};
    controller.SetTrackAnimation(0, first_index, &allowed_bones);
    allowed_bones.clear();
    allowed_bones.emplace(other);
    controller.SetTrackEnable(0, true);
    controller.SetTrackPosition(0, 0.75f);
    controller.SetTrackSpeed(0, 1.5f);
    controller.AddEventWeight(0, 0.75f, 0.0f, 0.0f);
    controller.SetTrackAnimation(1, second_index, nullptr);
    controller.SetTrackEnable(1, true);
    controller.AddEventWeight(1, 1.0f, 0.0f, 0.0f);
    controller.AdvanceTimeline(0.0f);

    const ModelAnimationController::TrackState first_track = controller.GetTrackState(0);
    CHECK(first_track.Enabled);
    CHECK(first_track.Speed == Catch::Approx(1.5f));
    CHECK(first_track.Weight == Catch::Approx(0.75f));
    CHECK(first_track.Position == Catch::Approx(0.75f));
    CHECK(first_track.ClipIndex == 11);
    CHECK(first_track.Reversed);
    CHECK(controller.IsTrackBoneEnabled(0, bone));
    CHECK_FALSE(controller.IsTrackBoneEnabled(0, other));
    CHECK_FALSE(controller.IsTrackBoneEnabled(0, missing));
    CHECK(controller.IsTrackBoneEnabled(1, bone));
    CHECK(controller.IsTrackBoneEnabled(1, other));
    CHECK_FALSE(controller.IsTrackBoneEnabled(1, missing));

    controller.ResetBonesTransition(0, {bone});
    CHECK(controller.IsTrackBoneEnabled(0, bone));
    CHECK_FALSE(controller.IsTrackBoneEnabled(1, bone));
    CHECK(controller.IsTrackBoneEnabled(1, other));

    controller.SetTrackAnimation(1, second_index, nullptr);
    CHECK(controller.IsTrackBoneEnabled(1, bone));
    CHECK(controller.IsTrackBoneEnabled(1, other));

    const unordered_set<hstring> no_allowed_bones;
    controller.SetTrackAnimation(1, second_index, &no_allowed_bones);
    CHECK_FALSE(controller.IsTrackBoneEnabled(1, bone));
    CHECK_FALSE(controller.IsTrackBoneEnabled(1, other));
#endif
}

TEST_CASE("Model animation bone masks use exact runtime binding names")
{
#if FO_ENABLE_3D
    HashStorage hashes;
    const hstring source_root = hashes.ToHashedString("SourceRoot");
    const hstring runtime_root = hashes.ToHashedString("ModelFile.fbx");
    const hstring child = hashes.ToHashedString("Child");
    const hstring contributed = hashes.ToHashedString("Contributed");
    const hstring absent = hashes.ToHashedString("Absent");
    const vector<hstring> canonical_names {source_root, child, contributed, absent};
    const vector<hstring> runtime_names {runtime_root, child, contributed, absent};
    const vector<uint8_t> canonical_joint_present {1, 1, 1, 0};
    unordered_set<hstring> renamed_root_bindings = BuildModelAnimationBoundBones(canonical_names, runtime_names, canonical_joint_present, "renamed root test");

    CHECK_FALSE(renamed_root_bindings.contains(source_root));
    CHECK_FALSE(renamed_root_bindings.contains(runtime_root));
    CHECK(renamed_root_bindings.contains(child));
    CHECK(renamed_root_bindings.contains(contributed));
    CHECK_FALSE(renamed_root_bindings.contains(absent));

    ModelAnimationController renamed_root_controller {1};
    const int32_t renamed_root_animation = renamed_root_controller.RegisterAnimation(17, 2.0f, false, std::move(renamed_root_bindings));
    renamed_root_controller.SetTrackAnimation(0, renamed_root_animation, nullptr);
    CHECK_FALSE(renamed_root_controller.IsTrackBoneEnabled(0, runtime_root));
    CHECK_FALSE(renamed_root_controller.IsTrackBoneEnabled(0, source_root));
    CHECK(renamed_root_controller.IsTrackBoneEnabled(0, child));
    CHECK(renamed_root_controller.IsTrackBoneEnabled(0, contributed));
    CHECK_FALSE(renamed_root_controller.IsTrackBoneEnabled(0, absent));

    const vector<hstring> matching_runtime_names {source_root};
    const vector<uint8_t> matching_presence {1};
    ModelAnimationController matching_root_controller {1};
    const int32_t matching_root_animation = matching_root_controller.RegisterAnimation(19, 2.0f, false, BuildModelAnimationBoundBones(const_span<hstring> {matching_runtime_names}, const_span<hstring> {matching_runtime_names}, const_span<uint8_t> {matching_presence}, "matching root test"));
    matching_root_controller.SetTrackAnimation(0, matching_root_animation, nullptr);
    CHECK(matching_root_controller.IsTrackBoneEnabled(0, source_root));
    CHECK_FALSE(matching_root_controller.IsTrackBoneEnabled(0, runtime_root));
#endif
}

FO_END_NAMESPACE

#endif
