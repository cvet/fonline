#include "catch_amalgamated.hpp"

#include "Movement.h"

FO_BEGIN_NAMESPACE

namespace
{
    constexpr msize TEST_MAP_SIZE {40, 40};
    constexpr uint16 TEST_SPEED = 100;
    const vector<uint8> TEST_STEPS {0, 0, 1, 1, 2};
    const vector<uint16> TEST_CONTROL_STEPS {2, 4, 5};
    constexpr mpos TEST_START_HEX {20, 20};
    constexpr mpos TEST_UNKNOWN_HEX {0, 0};
    constexpr mpos TEST_FALLBACK_HEX {1, 1};

    static auto MakeMovingContext() -> std::unique_ptr<MovingContext>
    {
        return std::make_unique<MovingContext>(TEST_MAP_SIZE, TEST_SPEED, TEST_STEPS, TEST_CONTROL_STEPS, nanotime {}, timespan {}, TEST_START_HEX, ipos16 {}, ipos16 {});
    }

    static auto MakeTimePoint(int32 milliseconds) -> nanotime
    {
        return nanotime {timespan {std::chrono::milliseconds {milliseconds}}};
    }

    static auto FindExpectedNearestHex(const vector<mpos>& path_hexes, mpos from_hex, mpos fallback_hex) -> mpos
    {
        auto best_hex = fallback_hex;
        auto best_dist = GeometryHelper::GetDistance(from_hex, best_hex);

        for (const auto hex : path_hexes) {
            const auto dist = GeometryHelper::GetDistance(from_hex, hex);
            if (dist < best_dist) {
                best_dist = dist;
                best_hex = hex;
            }
        }

        return best_hex;
    }
}

TEST_CASE("MovingContext")
{
    SECTION("EvaluateMetricsMatchesPathEndAndLengths")
    {
        auto moving = MakeMovingContext();
        const auto metrics = moving->EvaluateMetrics();

        CHECK(metrics.EndHex == moving->GetEndHex());
        CHECK(metrics.WholeTime == moving->GetWholeTime());
        CHECK(metrics.WholeDist == moving->GetWholeDist());
        CHECK(metrics.WholeTime > 0.0f);
        CHECK(metrics.WholeDist > 0.0f);
    }

    SECTION("UpdateCurrentTimeUsesRuntimeElapsedTime")
    {
        auto moving = MakeMovingContext();

        moving->UpdateCurrentTime(MakeTimePoint(250));

        CHECK(moving->GetRuntimeElapsedTime(MakeTimePoint(250)) == moving->GetElapsedTime());
    }

    SECTION("EvaluateProjectedHexMatchesCurrentProgressForZeroLookAhead")
    {
        auto moving = MakeMovingContext();
        const auto current_time_ms = iround<int32>(moving->GetWholeTime() * 0.75f);

        moving->UpdateCurrentTime(MakeTimePoint(current_time_ms));

        CHECK(moving->EvaluateProjectedHex(0.0f) == moving->EvaluateProgress().Hex);
    }

    SECTION("EvaluateProjectedHexMatchesFutureProgressAcrossControlSegments")
    {
        auto moving = MakeMovingContext();
        auto future_moving = MakeMovingContext();
        const auto current_time_ms = iround<int32>(moving->GetWholeTime() * 0.2f);
        const auto look_ahead_ms = moving->GetWholeTime() * 0.65f;
        const auto future_time_ms = current_time_ms + iround<int32>(look_ahead_ms);

        moving->UpdateCurrentTime(MakeTimePoint(current_time_ms));
        future_moving->UpdateCurrentTime(MakeTimePoint(future_time_ms));

        CHECK(moving->EvaluateProjectedHex(look_ahead_ms) == future_moving->EvaluateProgress().Hex);
    }

    SECTION("EvaluateProgressOverloadsStayConsistentForCurrentHex")
    {
        auto moving = MakeMovingContext();
        const auto current_time_ms = iround<int32>(moving->GetWholeTime() * 0.45f);

        moving->UpdateCurrentTime(MakeTimePoint(current_time_ms));

        const auto progress = moving->EvaluateProgress();
        const auto same_hex_progress = moving->EvaluateProgress(progress.Hex);

        CHECK(same_hex_progress.Hex == progress.Hex);
        CHECK(same_hex_progress.HexOffset == progress.HexOffset);
        CHECK(same_hex_progress.DirAngle == progress.DirAngle);
        CHECK(same_hex_progress.Completed == progress.Completed);
    }

    SECTION("EvaluatePathHexesReturnsTailFromCurrentHex")
    {
        auto moving = MakeMovingContext();
        const auto current_time_ms = iround<int32>(moving->GetWholeTime() * 0.45f);

        moving->UpdateCurrentTime(MakeTimePoint(current_time_ms));

        const auto progress = moving->EvaluateProgress();
        const auto path_hexes = moving->EvaluatePathHexes(progress.Hex);

        REQUIRE(!path_hexes.empty());
        CHECK(path_hexes.front() == progress.Hex);
        CHECK(path_hexes.back() == moving->GetEndHex());
    }

    SECTION("EvaluateNearestPathHexMatchesPathTailSearch")
    {
        auto moving = MakeMovingContext();
        const auto current_time_ms = iround<int32>(moving->GetWholeTime() * 0.45f);

        moving->UpdateCurrentTime(MakeTimePoint(current_time_ms));

        const auto progress = moving->EvaluateProgress();
        const auto path_hexes = moving->EvaluatePathHexes(progress.Hex);
        const auto expected_hex = FindExpectedNearestHex(path_hexes, moving->GetEndHex(), TEST_FALLBACK_HEX);

        CHECK(moving->EvaluateNearestPathHex(progress.Hex, moving->GetEndHex(), TEST_FALLBACK_HEX) == expected_hex);
    }

    SECTION("PathTailHelpersReturnFallbackForUnknownHex")
    {
        auto moving = MakeMovingContext();

        CHECK(moving->EvaluatePathHexes(TEST_UNKNOWN_HEX).empty());
        CHECK(moving->EvaluateNearestPathHex(TEST_UNKNOWN_HEX, moving->GetEndHex(), TEST_FALLBACK_HEX) == TEST_FALLBACK_HEX);
    }

    SECTION("ChangeSpeedPreservesCurrentProgress")
    {
        auto moving = MakeMovingContext();
        const auto current_time_ms = iround<int32>(moving->GetWholeTime() * 0.45f);
        const auto before_whole_time = moving->GetWholeTime();

        moving->UpdateCurrentTime(MakeTimePoint(current_time_ms));

        const auto before_progress = moving->EvaluateProgress();
        moving->ChangeSpeed(TEST_SPEED * 2, MakeTimePoint(current_time_ms));
        const auto after_progress = moving->EvaluateProgress();

        CHECK(moving->GetSpeed() == TEST_SPEED * 2);
        CHECK(moving->GetWholeTime() < before_whole_time);
        CHECK(after_progress.Hex == before_progress.Hex);
        CHECK(after_progress.HexOffset == before_progress.HexOffset);
        CHECK(after_progress.DirAngle == before_progress.DirAngle);
    }

    SECTION("SetBlockHexesStoresProvidedHexes")
    {
        auto moving = MakeMovingContext();
        const mpos pre_block_hex {21, 20};
        const mpos block_hex {22, 20};

        moving->SetBlockHexes(pre_block_hex, block_hex);

        CHECK(moving->GetPreBlockHex() == pre_block_hex);
        CHECK(moving->GetBlockHex() == block_hex);
    }

    SECTION("CompleteSuccessMarksCompletedAndClampsElapsedTime")
    {
        auto moving = MakeMovingContext();
        const auto current_time_ms = iround<int32>(moving->GetWholeTime() * 0.3f);

        moving->UpdateCurrentTime(MakeTimePoint(current_time_ms));
        moving->Complete(MovingState::Success);

        CHECK(moving->IsCompleted());
        CHECK(moving->GetCompleteReason() == MovingState::Success);
        CHECK(moving->GetElapsedTime() >= moving->GetWholeTime());
    }

    SECTION("CompleteStoppedMarksCompletedWithoutReachingEnd")
    {
        auto moving = MakeMovingContext();
        const auto current_time_ms = iround<int32>(moving->GetWholeTime() * 0.3f);

        moving->UpdateCurrentTime(MakeTimePoint(current_time_ms));
        const auto elapsed_before_complete = moving->GetElapsedTime();
        moving->Complete(MovingState::Stopped);

        CHECK(moving->IsCompleted());
        CHECK(moving->GetCompleteReason() == MovingState::Stopped);
        CHECK(moving->GetElapsedTime() == elapsed_before_complete);
    }

    SECTION("UpdateCurrentTimeToNextHexStopsAtNextPathHex")
    {
        auto moving = MakeMovingContext();
        auto expected_next_hex = moving->GetStartHex();
        REQUIRE(GeometryHelper::MoveHexByDir(expected_next_hex, moving->GetSteps().front(), moving->GetMapSize()));

        moving->UpdateCurrentTimeToNextHex(MakeTimePoint(iround<int32>(moving->GetWholeTime())), moving->GetStartHex());

        CHECK(moving->EvaluateProgress().Hex == expected_next_hex);
        CHECK(moving->EvaluateProgress().Hex != moving->GetEndHex());
    }
}

FO_END_NAMESPACE