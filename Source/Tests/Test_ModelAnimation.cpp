// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#if FO_ENABLE_3D

#include "ModelAnimation.h"

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

static void ConfigureTrack(ModelAnimationController& controller, int32_t track, int32_t animation, float32_t position, float32_t speed, float32_t weight, nptr<const unordered_set<hstring>> allowed_bones = nullptr)
{
    controller.SetTrackAnimation(track, animation, allowed_bones);
    controller.SetTrackEnable(track, true);
    controller.SetTrackPosition(track, timespan {iround<int64_t>(numeric_cast<float64_t>(position) * 1e9)});
    controller.SetTrackSpeed(track, speed);
    controller.AddEventWeight(track, weight, timespan {std::chrono::milliseconds {0}}, timespan {std::chrono::milliseconds {0}});
    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {0}});
}

#endif

TEST_CASE("Model animation controller copies share immutable metadata and own timeline state")
{
#if FO_ENABLE_3D
    hash_storage hashes {};
    hstring bone {hashes.to_hashed_string("Bone")};
    ModelAnimationController prototype {1};
    int32_t first_index {prototype.RegisterAnimation(3, timespan {std::chrono::seconds {2}}, false, {bone})};
    int32_t second_index {prototype.RegisterAnimation(7, timespan {std::chrono::seconds {4}}, true, {bone})};

    CHECK(prototype.GetAnimationsCount() == 2);
    CHECK(prototype.GetAnimDuration(first_index).to_sec<float64_t>() == Catch::Approx(2.0f));
    CHECK(prototype.GetAnimDuration(second_index).to_sec<float64_t>() == Catch::Approx(4.0f));

    ModelAnimationController first_instance {prototype.Copy()};
    ModelAnimationController second_instance {prototype.Copy()};
    ConfigureTrack(first_instance, 0, first_index, 0.25f, 1.0f, 1.0f);
    ConfigureTrack(second_instance, 0, second_index, 1.0f, 2.0f, 0.5f);
    first_instance.AdvanceTimeline(timespan {std::chrono::milliseconds {500}});
    second_instance.AdvanceTimeline(timespan {std::chrono::milliseconds {1000}});

    ModelAnimationController::TrackState first_track {first_instance.GetTrackState(0)};
    ModelAnimationController::TrackState second_track {second_instance.GetTrackState(0)};
    CHECK(first_track.Position.to_sec<float64_t>() == Catch::Approx(0.75f));
    CHECK(first_track.ClipIndex == 3);
    CHECK_FALSE(first_track.Reversed);
    CHECK(second_track.Position.to_sec<float64_t>() == Catch::Approx(3.0f));
    CHECK(second_track.ClipIndex == 7);
    CHECK(second_track.Reversed);
    CHECK(first_instance.IsTrackBoneEnabled(0, bone));
    CHECK(second_instance.IsTrackBoneEnabled(0, bone));
#endif
}

TEST_CASE("Model animation timelines retain nanoseconds and reject partial advancement")
{
#if FO_ENABLE_3D
    ModelAnimationController controller {2};
    controller.SetTrackEnable(0, true);
    controller.SetTrackEnable(1, true);
    controller.SetTrackSpeed(0, 1.0f);
    controller.SetTrackSpeed(1, 1.0f);
    controller.SetTrackPosition(0, timespan {std::chrono::hours {24}});
    controller.SetTrackPosition(1, timespan {std::numeric_limits<int64_t>::max()});
    controller.AddEventEnable(0, false, timespan {2});

    CHECK_THROWS(controller.AdvanceTimeline(timespan {1}));
    CHECK(controller.GetTrackPosition(0) == timespan {std::chrono::hours {24}});
    CHECK(controller.GetTrackPosition(1) == timespan {std::numeric_limits<int64_t>::max()});
    CHECK(controller.GetTrackEnable(0));

    controller.SetTrackPosition(1, {});
    controller.AdvanceTimeline(timespan {1});
    CHECK(controller.GetTrackEnable(0));
    CHECK(controller.GetTrackPosition(0) == timespan {std::chrono::hours {24}} + timespan {1});
    controller.AdvanceTimeline(timespan {1});
    CHECK_FALSE(controller.GetTrackEnable(0));
    CHECK(controller.GetTrackPosition(0) == timespan {std::chrono::hours {24}} + timespan {2});

    CHECK(ScaleModelAnimationTime(timespan {std::numeric_limits<int64_t>::max()}, 1.0) == timespan {std::numeric_limits<int64_t>::max()});
    CHECK_THROWS(ScaleModelAnimationTime(timespan {std::numeric_limits<int64_t>::max()}, 2.0));
    CHECK_THROWS(controller.RegisterAnimation(0, {}, false, {}));
#endif
}

TEST_CASE("Model animation callback phases preserve endpoint-cycle semantics")
{
#if FO_ENABLE_3D
    CHECK(IsModelAnimationCallbackReached(timespan {4}, timespan {5}, timespan {10}, 0.5f));
    CHECK_FALSE(IsModelAnimationCallbackReached(timespan {5}, timespan {6}, timespan {10}, 0.5f));
    CHECK(IsModelAnimationCallbackReached(timespan {6}, timespan {24}, timespan {10}, 0.5f));
    CHECK(IsModelAnimationCallbackReached(timespan {4}, timespan {16}, timespan {10}, 0.5f));
    CHECK(IsModelAnimationCallbackReached(timespan {-6}, timespan {-5}, timespan {10}, 0.5f));
    CHECK_FALSE(IsModelAnimationCallbackReached(timespan {6}, timespan {4}, timespan {10}, 0.5f));
    CHECK_FALSE(IsModelAnimationCallbackReached(timespan {5}, timespan {5}, timespan {10}, 0.5f));
    CHECK(IsModelAnimationCallbackReached(timespan {9}, timespan {10}, timespan {10}, 0.0f));
    CHECK(IsModelAnimationCallbackReached(timespan {9}, timespan {10}, timespan {10}, 1.0f));

    constexpr int64_t maximum {std::numeric_limits<int64_t>::max()};
    CHECK(IsModelAnimationCallbackReached(timespan {maximum - 1}, timespan {maximum}, timespan {maximum}, 1.0f));
    CHECK(IsModelAnimationCallbackReached(timespan {-1}, timespan {0}, timespan {maximum}, 0.0f));
    CHECK(IsModelAnimationCallbackReached(timespan {std::numeric_limits<int64_t>::min()}, timespan {maximum}, timespan {maximum}, 0.5f));
    CHECK(IsModelAnimationCallbackReached(timespan {std::numeric_limits<int64_t>::min()}, timespan {maximum / 2 + 1}, timespan {maximum}, 0.5f));
#endif
}

TEST_CASE("Model animation controller timeline resolves events for runtime track state")
{
#if FO_ENABLE_3D
    hash_storage hashes {};
    hstring bone {hashes.to_hashed_string("Bone")};
    ModelAnimationController controller {1};
    int32_t animation_index {controller.RegisterAnimation(5, timespan {std::chrono::seconds {2}}, true, {bone})};
    controller.SetTrackAnimation(0, animation_index, nullptr);
    controller.SetTrackEnable(0, true);
    controller.SetTrackSpeed(0, 1.0f);
    controller.AddEventSpeed(0, 3.0f, timespan {std::chrono::milliseconds {0}}, timespan {std::chrono::milliseconds {1000}});
    controller.AddEventWeight(0, 1.0f, timespan {std::chrono::milliseconds {0}}, timespan {std::chrono::milliseconds {1000}});

    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {500}});
    ModelAnimationController::TrackState track {controller.GetTrackState(0)};
    CHECK(track.Enabled);
    CHECK(track.Speed == Catch::Approx(2.0f));
    CHECK(track.Weight == Catch::Approx(0.5f));
    CHECK(track.Position.to_sec<float64_t>() == Catch::Approx(0.75f));
    CHECK(track.ClipIndex == 5);
    CHECK(track.Reversed);

    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {500}});
    track = controller.GetTrackState(0);
    CHECK(track.Speed == Catch::Approx(3.0f));
    CHECK(track.Weight == Catch::Approx(1.0f));
    CHECK(track.Position.to_sec<float64_t>() == Catch::Approx(2.0f));

    controller.ResetEvents();
    controller.AddEventEnable(0, false, timespan {std::chrono::milliseconds {250}});
    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {250}});
    CHECK_FALSE(controller.GetTrackEnable(0));
    CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(2.75f));
#endif
}

TEST_CASE("Model animation timeline consumes the active interval before stopping")
{
#if FO_ENABLE_3D
    ModelAnimationController controller {1};
    controller.SetTrackEnable(0, true);
    controller.SetTrackSpeed(0, 1.0f);

    SECTION("Speed stop retains the elapsed clip phase")
    {
        controller.AddEventSpeed(0, 0.0f, timespan {std::chrono::milliseconds {600}}, timespan {std::chrono::milliseconds {0}});
        controller.AdvanceTimeline(timespan {std::chrono::milliseconds {800}});

        CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(0.6f));
        CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() >= 0.3f);
        CHECK(controller.GetTrackSpeed(0) == 0.0f);
        CHECK(controller.GetTrackEnable(0));

        controller.AdvanceTimeline(timespan {std::chrono::milliseconds {1000}});

        CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(0.6f));
    }

    SECTION("Disable retains time before its boundary")
    {
        controller.AddEventEnable(0, false, timespan {std::chrono::milliseconds {600}});
        controller.AdvanceTimeline(timespan {std::chrono::milliseconds {800}});

        CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(0.6f));
        CHECK_FALSE(controller.GetTrackEnable(0));
    }

    SECTION("Enable excludes time before its boundary")
    {
        controller.SetTrackEnable(0, false);
        controller.SetTrackSpeed(0, 2.0f);
        controller.AddEventEnable(0, true, timespan {std::chrono::milliseconds {200}});
        controller.AdvanceTimeline(timespan {std::chrono::milliseconds {500}});

        CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(0.6f));
        CHECK(controller.GetTrackEnable(0));
    }
#endif
}

TEST_CASE("Model animation timeline integrates speed ramps independently of frame size")
{
#if FO_ENABLE_3D
    ModelAnimationController controller {1};
    controller.SetTrackEnable(0, true);
    controller.SetTrackSpeed(0, 1.0f);
    controller.AddEventSpeed(0, 3.0f, timespan {std::chrono::milliseconds {0}}, timespan {std::chrono::milliseconds {1000}});

    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {500}});

    CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(0.75f));
    CHECK(controller.GetTrackSpeed(0) == Catch::Approx(2.0f));

    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {500}});

    CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(2.0f));
    CHECK(controller.GetTrackSpeed(0) == Catch::Approx(3.0f));
#endif
}

TEST_CASE("Model animation timeline preserves a stop across different frame partitions")
{
#if FO_ENABLE_3D
    ModelAnimationController controller {1};
    controller.SetTrackEnable(0, true);
    controller.SetTrackSpeed(0, 1.0f);
    controller.AddEventSpeed(0, 0.0f, timespan {std::chrono::milliseconds {600}}, timespan {std::chrono::milliseconds {0}});

    ModelAnimationController partitioned = controller.Copy();
    partitioned.SetTrackEnable(0, true);
    partitioned.SetTrackSpeed(0, 1.0f);
    partitioned.AddEventSpeed(0, 0.0f, timespan {std::chrono::milliseconds {600}}, timespan {std::chrono::milliseconds {0}});

    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {800}});

    partitioned.AdvanceTimeline(timespan {std::chrono::milliseconds {200}});
    partitioned.AdvanceTimeline(timespan {std::chrono::milliseconds {200}});
    partitioned.AdvanceTimeline(timespan {std::chrono::milliseconds {400}});

    CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(0.6f));
    CHECK(partitioned.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(0.6f));
    CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(partitioned.GetTrackPosition(0).to_sec<float64_t>()));
#endif
}

TEST_CASE("Model animation timeline replaces active ramps at their actual boundaries")
{
#if FO_ENABLE_3D
    ModelAnimationController controller {1};
    controller.SetTrackEnable(0, true);
    controller.SetTrackSpeed(0, 1.0f);
    controller.AddEventSpeed(0, 3.0f, timespan {std::chrono::milliseconds {0}}, timespan {std::chrono::milliseconds {1000}});
    controller.AddEventSpeed(0, 2.0f, timespan {std::chrono::milliseconds {500}}, timespan {std::chrono::milliseconds {0}});

    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {750}});

    CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(1.25f));
    CHECK(controller.GetTrackSpeed(0) == Catch::Approx(2.0f));

    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {500}});

    CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(2.25f));
    CHECK(controller.GetTrackSpeed(0) == Catch::Approx(2.0f));
#endif
}

TEST_CASE("Model animation timeline copies bindings into fresh independent playback")
{
#if FO_ENABLE_3D
    ModelAnimationController controller {1};
    controller.SetTrackEnable(0, true);
    controller.SetTrackSpeed(0, 1.0f);
    controller.AddEventSpeed(0, 3.0f, timespan {std::chrono::milliseconds {0}}, timespan {std::chrono::milliseconds {1000}});
    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {250}});

    ModelAnimationController copy = controller.Copy();
    CHECK_FALSE(copy.GetTrackEnable(0));
    CHECK(copy.GetTrackPosition(0).to_sec<float64_t>() == 0.0f);
    CHECK(copy.GetTrackSpeed(0) == 0.0f);

    copy.SetTrackEnable(0, true);
    copy.SetTrackSpeed(0, 2.0f);
    copy.AddEventSpeed(0, 0.0f, timespan {std::chrono::milliseconds {500}}, timespan {std::chrono::milliseconds {0}});

    controller.ResetEvents();
    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {750}});

    CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(1.4375f));
    CHECK(controller.GetTrackSpeed(0) == Catch::Approx(1.5f));
    CHECK(copy.GetTrackPosition(0).to_sec<float64_t>() == 0.0f);

    copy.AdvanceTimeline(timespan {std::chrono::milliseconds {750}});

    CHECK(copy.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(1.0f));
    CHECK(copy.GetTrackSpeed(0) == 0.0f);
    CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(1.4375f));
#endif
}

TEST_CASE("Model animation timeline applies zero-delta and simultaneous events once")
{
#if FO_ENABLE_3D
    ModelAnimationController controller {1};
    controller.SetTrackEnable(0, true);
    controller.SetTrackSpeed(0, 1.0f);
    controller.AddEventSpeed(0, 2.0f, timespan {std::chrono::milliseconds {0}}, timespan {std::chrono::milliseconds {0}});
    controller.AddEventWeight(0, 1.0f, timespan {std::chrono::milliseconds {0}}, timespan {std::chrono::milliseconds {1000}});

    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {0}});
    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {0}});

    CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == 0.0f);
    CHECK(controller.GetTrackSpeed(0) == 2.0f);
    CHECK(controller.GetTrackState(0).Weight == 0.0f);

    controller.AddEventSpeed(0, 4.0f, timespan {std::chrono::milliseconds {250}}, timespan {std::chrono::milliseconds {0}});
    controller.AddEventSpeed(0, 3.0f, timespan {std::chrono::milliseconds {250}}, timespan {std::chrono::milliseconds {0}});
    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {500}});

    CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(1.25f));
    CHECK(controller.GetTrackSpeed(0) == 3.0f);
    CHECK(controller.GetTrackState(0).Weight == Catch::Approx(0.5f));
#endif
}

TEST_CASE("Model animation callbacks distinguish one-shot endpoints and loop crossings")
{
#if FO_ENABLE_3D
    timespan duration {std::chrono::seconds {1}};
    timespan zero {};
    timespan four_fifths {std::chrono::milliseconds {800}};
    timespan one_and_two_fifths {std::chrono::milliseconds {1400}};

    CHECK(IsModelAnimationCallbackReached(four_fifths, duration, duration, 1.0f, false));
    CHECK_FALSE(IsModelAnimationCallbackReached(four_fifths, duration, duration, 0.0f, false));
    CHECK_FALSE(IsModelAnimationCallbackReached(duration, duration, duration, 1.0f, false));
    CHECK(IsModelAnimationCallbackReached(four_fifths, duration, duration, 0.0f, true));
    CHECK(IsModelAnimationCallbackReached(four_fifths, one_and_two_fifths, duration, 0.3f, true));
    CHECK_FALSE(IsModelAnimationCallbackReached(zero, zero, duration, 0.0f, true));
#endif
}

TEST_CASE("Model animation timelines preserve reverse advancement and event boundaries near the time limit")
{
#if FO_ENABLE_3D
    SECTION("Negative elapsed preserves reverse advancement")
    {
        ModelAnimationController controller {1};
        controller.SetTrackEnable(0, true);
        controller.SetTrackSpeed(0, 2.0f);

        controller.AdvanceTimeline(timespan {std::chrono::milliseconds {-250}});
        CHECK(controller.GetTrackPosition(0) == timespan {std::chrono::milliseconds {-500}});

        controller.AdvanceTimeline(timespan {std::chrono::milliseconds {250}});
        CHECK(controller.GetTrackPosition(0) == timespan::zero);
    }

    SECTION("A reversed clock does not start a ramp before its boundary")
    {
        ModelAnimationController controller {1};
        controller.SetTrackEnable(0, true);
        controller.SetTrackSpeed(0, 1.0f);
        controller.AddEventSpeed(0, 3.0f, timespan {std::chrono::milliseconds {200}}, timespan {std::chrono::seconds {1}});

        controller.AdvanceTimeline(timespan {std::chrono::milliseconds {700}});
        CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(0.95));
        CHECK(controller.GetTrackSpeed(0) == Catch::Approx(2.0f));

        controller.AdvanceTimeline(timespan {std::chrono::milliseconds {-600}});
        controller.AdvanceTimeline(timespan {std::chrono::milliseconds {50}});
        CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(-0.15));
        CHECK(controller.GetTrackSpeed(0) == Catch::Approx(2.0f));

        controller.AdvanceTimeline(timespan {std::chrono::milliseconds {100}});
        CHECK(controller.GetTrackPosition(0).to_sec<float64_t>() == Catch::Approx(0.0025));
        CHECK(controller.GetTrackSpeed(0) == Catch::Approx(1.1f));
    }

    SECTION("An event extending beyond the clock limit does not overflow early")
    {
        constexpr int64_t maximum = std::numeric_limits<int64_t>::max();
        ModelAnimationController controller {1};
        controller.SetTrackEnable(0, true);
        controller.SetTrackSpeed(0, 0.0f);
        controller.AddEventWeight(0, 1.0f, timespan {maximum - 2}, timespan {10});

        controller.AdvanceTimeline(timespan {maximum - 3});
        controller.AdvanceTimeline(timespan {2});

        CHECK(controller.GetTrackState(0).Weight == Catch::Approx(0.1f));
        CHECK_THROWS(controller.AdvanceTimeline(timespan {2}));
        CHECK(controller.GetTrackState(0).Weight == Catch::Approx(0.1f));
        CHECK(controller.GetTrackPosition(0) == timespan::zero);
    }
#endif
}

TEST_CASE("Model animation track bindings preserve allowed masks and transition suppression")
{
#if FO_ENABLE_3D
    hash_storage hashes {};
    hstring bone {hashes.to_hashed_string("Bone")};
    hstring other {hashes.to_hashed_string("Other")};
    hstring missing {hashes.to_hashed_string("Missing")};
    ModelAnimationController controller {2};
    int32_t first_index {controller.RegisterAnimation(11, timespan {std::chrono::seconds {2}}, true, {bone, other})};
    int32_t second_index {controller.RegisterAnimation(13, timespan {std::chrono::seconds {2}}, false, {bone, other})};

    unordered_set<hstring> allowed_bones {bone};
    controller.SetTrackAnimation(0, first_index, &allowed_bones);
    allowed_bones.clear();
    allowed_bones.emplace(other);
    controller.SetTrackEnable(0, true);
    controller.SetTrackPosition(0, timespan {std::chrono::milliseconds {750}});
    controller.SetTrackSpeed(0, 1.5f);
    controller.AddEventWeight(0, 0.75f, timespan {std::chrono::milliseconds {0}}, timespan {std::chrono::milliseconds {0}});
    controller.SetTrackAnimation(1, second_index, nullptr);
    controller.SetTrackEnable(1, true);
    controller.AddEventWeight(1, 1.0f, timespan {std::chrono::milliseconds {0}}, timespan {std::chrono::milliseconds {0}});
    controller.AdvanceTimeline(timespan {std::chrono::milliseconds {0}});

    ModelAnimationController::TrackState first_track {controller.GetTrackState(0)};
    CHECK(first_track.Enabled);
    CHECK(first_track.Speed == Catch::Approx(1.5f));
    CHECK(first_track.Weight == Catch::Approx(0.75f));
    CHECK(first_track.Position.to_sec<float64_t>() == Catch::Approx(0.75f));
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

    unordered_set<hstring> no_allowed_bones {};
    controller.SetTrackAnimation(1, second_index, &no_allowed_bones);
    CHECK_FALSE(controller.IsTrackBoneEnabled(1, bone));
    CHECK_FALSE(controller.IsTrackBoneEnabled(1, other));
#endif
}

TEST_CASE("Model animation bone masks use exact runtime binding names")
{
#if FO_ENABLE_3D
    hash_storage hashes {};
    hstring source_root {hashes.to_hashed_string("SourceRoot")};
    hstring runtime_root {hashes.to_hashed_string("ModelFile.fbx")};
    hstring child {hashes.to_hashed_string("Child")};
    hstring contributed {hashes.to_hashed_string("Contributed")};
    hstring absent {hashes.to_hashed_string("Absent")};
    vector<hstring> canonical_names {source_root, child, contributed, absent};
    vector<hstring> runtime_names {runtime_root, child, contributed, absent};
    vector<uint8_t> canonical_joint_present {1, 1, 1, 0};
    unordered_set<hstring> renamed_root_bindings {BuildModelAnimationBoundBones(canonical_names, runtime_names, canonical_joint_present, "renamed root test")};

    CHECK_FALSE(renamed_root_bindings.contains(source_root));
    CHECK_FALSE(renamed_root_bindings.contains(runtime_root));
    CHECK(renamed_root_bindings.contains(child));
    CHECK(renamed_root_bindings.contains(contributed));
    CHECK_FALSE(renamed_root_bindings.contains(absent));

    ModelAnimationController renamed_root_controller {1};
    int32_t renamed_root_animation {renamed_root_controller.RegisterAnimation(17, timespan {std::chrono::seconds {2}}, false, std::move(renamed_root_bindings))};
    renamed_root_controller.SetTrackAnimation(0, renamed_root_animation, nullptr);
    CHECK_FALSE(renamed_root_controller.IsTrackBoneEnabled(0, runtime_root));
    CHECK_FALSE(renamed_root_controller.IsTrackBoneEnabled(0, source_root));
    CHECK(renamed_root_controller.IsTrackBoneEnabled(0, child));
    CHECK(renamed_root_controller.IsTrackBoneEnabled(0, contributed));
    CHECK_FALSE(renamed_root_controller.IsTrackBoneEnabled(0, absent));

    vector<hstring> matching_runtime_names {source_root};
    vector<uint8_t> matching_presence {1};
    ModelAnimationController matching_root_controller {1};
    int32_t matching_root_animation {matching_root_controller.RegisterAnimation(19, timespan {std::chrono::seconds {2}}, false, BuildModelAnimationBoundBones(const_span<hstring> {matching_runtime_names}, const_span<hstring> {matching_runtime_names}, const_span<uint8_t> {matching_presence}, "matching root test"))};
    matching_root_controller.SetTrackAnimation(0, matching_root_animation, nullptr);
    CHECK(matching_root_controller.IsTrackBoneEnabled(0, source_root));
    CHECK_FALSE(matching_root_controller.IsTrackBoneEnabled(0, runtime_root));
#endif
}

FO_END_NAMESPACE

#endif
