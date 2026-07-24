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

#include "FogOfWar.h"
#include "LineTracer.h"

FO_BEGIN_NAMESPACE

namespace
{
    auto MakeInput(mpos chosen_hex, nanotime frame_time, int32_t transition_duration = 0) -> FogShape::Input
    {
        FogShape::Input input;
        input.FogExtraLength = 0;
        input.FogTransitionDuration = transition_duration;
        input.MapHexWidth = 10;
        input.MapHexHeight = 8;
        input.MapSize = {100, 100};
        input.FrameTime = frame_time;
        input.FogOrigin.Valid = true;
        input.FogOrigin.BaseHex = chosen_hex;
        input.FogOrigin.LookDistance = 1;
        input.TraceBulletToBlock = [](mpos, mpos target_hex, int32_t, bool) { return target_hex; };
        return input;
    }
}

TEST_CASE("FogOfWar")
{
    SECTION("Builds look border primitives for chosen critter")
    {
        FogShape fog;
        fog.RequestRebuild();

        auto input = MakeInput({10, 10}, nanotime {});
        fog.Prepare(input);

        CHECK(fog.GetPoints().size() == 10);
        // Geometry is the honest hexagon (the oval is shaped in the flush shader, not here).
        CHECK(fog.GetPoints().front().PointPos.x == 21);
        CHECK(fog.GetPoints().front().PointColor.comp.a == 0);
    }

    SECTION("Builds shoot border primitives with traced overlay input")
    {
        FogShape fog;
        fog.RequestRebuild();

        auto input = MakeInput({10, 10}, nanotime {});
        input.Distance = 3;
        input.TraceMode = FogShape::TraceModeType::Overlay;
        input.OverlayColor = ucolor {255, 96, 0, 255};
        input.CenterColor = ucolor {0, 0, 0, 255};
        fog.Prepare(input);

        CHECK(fog.GetPoints().size() == 10);
        CHECK(fog.GetPoints().front().PointColor.comp.a == 255);
    }

    SECTION("Builds observation border primitives with traced overlay tint")
    {
        FogShape fog;
        fog.RequestRebuild();

        auto input = MakeInput({10, 10}, nanotime {});
        input.Distance = 3;
        input.TraceMode = FogShape::TraceModeType::Overlay;
        input.OverlayColor = ucolor {96, 224, 96, 255};
        fog.Prepare(input);

        CHECK(fog.GetPoints().size() == 10);
        // Shared radial falloff: an edge one hex out (max_overlay_dist = min(1, 3) + 1 = 2, unblocked
        // result = 1) tints to overlay * (2 - 1) * 255 / 2 / 255 = overlay * 127 / 255.
        CHECK(static_cast<int>(fog.GetPoints().front().PointColor.comp.r) == 47);
        CHECK(static_cast<int>(fog.GetPoints().front().PointColor.comp.g) == 111);
        CHECK(static_cast<int>(fog.GetPoints().front().PointColor.comp.b) == 47);
        CHECK(static_cast<int>(fog.GetPoints().front().PointColor.comp.a) == 255);
        // The traced-overlay center carries the full overlay color (origin of the gradient).
        CHECK(static_cast<int>(fog.GetPoints()[2].PointColor.comp.r) == 96);
        CHECK(static_cast<int>(fog.GetPoints()[2].PointColor.comp.g) == 224);
        CHECK(static_cast<int>(fog.GetPoints()[2].PointColor.comp.b) == 96);
        CHECK(static_cast<int>(fog.GetPoints()[2].PointColor.comp.a) == 255);
    }

    SECTION("Builds traced overlay primitives with custom tint")
    {
        FogShape fog;
        fog.RequestRebuild();

        auto input = MakeInput({10, 10}, nanotime {});
        input.Distance = 3;
        input.TraceMode = FogShape::TraceModeType::Overlay;
        input.OverlayColor = ucolor {255, 96, 176, 255};
        fog.Prepare(input);

        CHECK(fog.GetPoints().size() == 10);
        // Edge one hex out tints to overlay * 127 / 255 along the shared falloff (see above).
        CHECK(static_cast<int>(fog.GetPoints().front().PointColor.comp.r) == 127);
        CHECK(static_cast<int>(fog.GetPoints().front().PointColor.comp.g) == 47);
        CHECK(static_cast<int>(fog.GetPoints().front().PointColor.comp.b) == 87);
        // The center carries the full custom overlay color.
        CHECK(static_cast<int>(fog.GetPoints()[2].PointColor.comp.r) == 255);
        CHECK(static_cast<int>(fog.GetPoints()[2].PointColor.comp.g) == 96);
        CHECK(static_cast<int>(fog.GetPoints()[2].PointColor.comp.b) == 176);
    }

    SECTION("Produces no points when disabled")
    {
        FogShape fog;
        fog.RequestRebuild();

        auto input = MakeInput({10, 10}, nanotime {});
        input.Enabled = false;
        fog.Prepare(input);

        CHECK(fog.GetPoints().empty());
    }

    SECTION("Keeps local fog geometry stable while origin changes")
    {
        FogShape fog;
        fog.RequestRebuild();

        auto input = MakeInput({10, 10}, nanotime {});
        fog.Prepare(input);
        int32_t initial_x = fog.GetPoints().front().PointPos.x;

        fog.RequestRebuild();
        input = MakeInput({20, 10}, nanotime {timespan {std::chrono::milliseconds {0}}}, 200);
        fog.Prepare(input);

        CHECK(fog.GetPoints().front().PointPos.x == initial_x);

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {100}}};
        fog.Prepare(input);
        int32_t mid_x = fog.GetPoints().front().PointPos.x;

        CHECK(mid_x == initial_x);

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);

        CHECK(fog.GetPoints().front().PointPos.x == initial_x);
    }

    SECTION("Collapses to origin when disabled")
    {
        FogShape fog;

        auto input = MakeInput({10, 10}, nanotime {timespan {std::chrono::milliseconds {0}}}, 200);
        fog.Prepare(input);
        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);

        CHECK(!fog.GetPoints().empty());
        int32_t edge_x = fog.GetPoints().front().PointPos.x;
        int32_t center_x = fog.GetPoints()[2].PointPos.x;
        CHECK(edge_x != center_x);

        input.Enabled = false;
        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);
        CHECK(!fog.GetPoints().empty());

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {300}}};
        fog.Prepare(input);
        CHECK(!fog.GetPoints().empty());
        int32_t mid_x = fog.GetPoints().front().PointPos.x;
        CHECK(mid_x != edge_x);

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {400}}};
        fog.Prepare(input);
        CHECK(fog.GetPoints().empty());
    }

    SECTION("Expands from origin when re-enabled")
    {
        FogShape fog;

        auto input = MakeInput({10, 10}, nanotime {timespan {std::chrono::milliseconds {0}}}, 200);
        fog.Prepare(input);
        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);
        int32_t final_edge_x = fog.GetPoints().front().PointPos.x;

        input.Enabled = false;
        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);
        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {400}}};
        fog.Prepare(input);
        CHECK(fog.GetPoints().empty());

        input.Enabled = true;
        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {400}}};
        fog.Prepare(input);
        CHECK(!fog.GetPoints().empty());
        int32_t center_x = fog.GetPoints()[2].PointPos.x;
        CHECK(fog.GetPoints().front().PointPos.x == center_x);

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {600}}};
        fog.Prepare(input);
        CHECK(fog.GetPoints().front().PointPos.x == final_edge_x);
    }

    SECTION("Interpolates center point attributes during transition")
    {
        FogShape fog;

        auto input = MakeInput({10, 10}, nanotime {timespan {std::chrono::milliseconds {0}}}, 200);
        input.CenterColor = ucolor {10, 20, 30, 40};
        fog.Prepare(input);
        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);
        REQUIRE(fog.GetPoints().size() > 2);
        CHECK(fog.GetPoints()[2].PointColor == ucolor {10, 20, 30, 40});

        input.CenterColor = ucolor {110, 120, 130, 140};
        fog.Prepare(input);

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {300}}};
        fog.Prepare(input);
        REQUIRE(fog.GetPoints().size() > 2);
        CHECK(fog.GetPoints()[2].PointColor == ucolor {60, 70, 80, 90});

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {400}}};
        fog.Prepare(input);
        CHECK(fog.GetPoints()[2].PointColor == ucolor {110, 120, 130, 140});
    }

    SECTION("First appearance expands from origin")
    {
        FogShape fog;

        auto input = MakeInput({10, 10}, nanotime {timespan {std::chrono::milliseconds {0}}}, 200);
        fog.Prepare(input);

        CHECK(!fog.GetPoints().empty());
        int32_t center_x = fog.GetPoints()[2].PointPos.x;
        CHECK(fog.GetPoints().front().PointPos.x == center_x);

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);
        CHECK(fog.GetPoints().front().PointPos.x != center_x);
    }

    SECTION("Blocked shoot edge points use base draw offset")
    {
        FogShape fog;
        fog.SetDrawOffset({100, 50});
        fog.SetBaseDrawOffset({100, 0});

        auto input = MakeInput({10, 10}, nanotime {});
        input.Distance = 1;
        input.TraceMode = FogShape::TraceModeType::Overlay;
        input.OverlayColor = ucolor {255, 96, 0, 255};
        input.CenterColor = ucolor {0, 0, 0, 255};
        input.TraceBulletToBlock = [](mpos start, mpos, int32_t, bool) { return start; };
        fog.Prepare(input);

        CHECK(!fog.GetPoints().empty());
        CHECK(fog.GetPoints()[0].PointOffset != nullptr);
        CHECK(*fog.GetPoints()[0].PointOffset == ipos32 {100, 0});
        CHECK(fog.GetPoints()[2].PointOffset != nullptr);
        CHECK(*fog.GetPoints()[2].PointOffset == ipos32 {100, 50});
    }

    SECTION("Blocked shoot rays fade along the shared overlay falloff")
    {
        FogShape fog;

        auto input = MakeInput({10, 10}, nanotime {});
        input.FogOrigin.LookDistance = 6;
        input.Distance = 6;
        input.TraceMode = FogShape::TraceModeType::Overlay;
        input.OverlayColor = ucolor {255, 96, 0, 255};
        input.CenterColor = ucolor {0, 0, 0, 255};
        input.TraceBulletToBlock = [map_size = input.MapSize](mpos start, mpos target, int32_t, bool) {
            constexpr int32_t block_dist = 3;

            if (GeometryHelper::GetDistance(start, target) <= block_dist) {
                return target;
            }

            mpos block_hex = start;
            LineTracer tracer(start, target, 0.0f, map_size);

            for (int32_t i = 0; i < block_dist; i++) {
                if (!tracer.GetNextHex(block_hex).has_value()) {
                    break;
                }
            }

            return block_hex;
        };
        fog.Prepare(input);

        CHECK(!fog.GetPoints().empty());
        // The ray to a hex 6 out (max_overlay_dist = min(6, 6) + 1 = 7) is blocked at distance 3, so it
        // fades to (7 - 3) * 255 / 7 along the one shared falloff — the same value an unobstructed ray
        // reaches at distance 3. Obstacles only shorten a ray along that gradient, not change its slope.
        CHECK(fog.GetPoints()[0].PointColor.comp.r == numeric_cast<uint8_t>((7 - 3) * 255 / 7));
        // Green stays proportional to the overlay tint (96 / 255 of the red strength).
        CHECK(fog.GetPoints()[0].PointColor.comp.g == numeric_cast<uint8_t>((numeric_cast<int32_t>(fog.GetPoints()[0].PointColor.comp.r) * 96) / 255));
    }

    SECTION("Point offsets stay valid after move")
    {
        vector<FogShape> fogs;
        fogs.reserve(1);

        fogs.emplace_back();
        fogs.front().SetDrawOffset({11, 22});
        fogs.front().SetBaseDrawOffset({33, 44});
        fogs.front().RequestRebuild();

        auto input = MakeInput({10, 10}, nanotime {});
        input.Distance = 1;
        input.TraceMode = FogShape::TraceModeType::Overlay;
        input.OverlayColor = ucolor {255, 96, 0, 255};
        input.CenterColor = ucolor {0, 0, 0, 255};
        input.TraceBulletToBlock = [](mpos start, mpos, int32_t, bool) { return start; };
        fogs.front().Prepare(input);

        REQUIRE(!fogs.front().GetPoints().empty());

        // Trigger reallocation that moves the existing element
        fogs.emplace_back();

        for (const auto& p : fogs.front().GetPoints()) {
            REQUIRE(p.PointOffset != nullptr);
            auto offset = *p.PointOffset;
            CHECK((offset == ipos32 {11, 22} || offset == ipos32 {33, 44}));
        }

        fogs.front().SetDrawOffset({77, 88});
        fogs.front().SetBaseDrawOffset({99, 100});

        bool draw_offset_seen = false;
        bool base_draw_offset_seen = false;

        for (const auto& p : fogs.front().GetPoints()) {
            REQUIRE(p.PointOffset != nullptr);
            auto offset = *p.PointOffset;
            if (offset == ipos32 {77, 88}) {
                draw_offset_seen = true;
            }
            else if (offset == ipos32 {99, 100}) {
                base_draw_offset_seen = true;
            }
            else {
                FAIL("Point offset stale after fog move");
            }
        }

        CHECK(draw_offset_seen);
        CHECK(base_draw_offset_seen);
    }
}

FO_END_NAMESPACE
