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

FO_BEGIN_NAMESPACE

namespace
{
    auto MakeInput(mpos chosen_hex, nanotime frame_time, int32 transition_duration = 0) -> FogOfWar::Input
    {
        FogOfWar::Input input;
        input.FogExtraLength = 0;
        input.FogTransitionDuration = transition_duration;
        input.MapHexWidth = 10;
        input.MapHexHeight = 8;
        input.MapSize = {100, 100};
        input.FrameTime = frame_time;
        input.FogOrigin.Valid = true;
        input.FogOrigin.BaseHex = chosen_hex;
        input.FogOrigin.LookDistance = 1;
        input.TraceBulletToBlock = [](mpos, mpos target_hex, int32, bool) { return target_hex; };
        return input;
    }
}

TEST_CASE("FogOfWar")
{
    SECTION("Builds look border primitives for chosen critter")
    {
        FogOfWar fog {FogOfWar::Kind::Look};
        fog.RequestRebuild();

        auto input = MakeInput({10, 10}, nanotime {});
        fog.Prepare(input);

        CHECK(fog.GetPoints().size() == 10);
        CHECK(fog.GetPoints().front().PointPos.x == 905);
        CHECK(fog.GetPoints().front().PointColor.comp.a == 0);
    }

    SECTION("Builds shoot border primitives with Kind::Shoot")
    {
        FogOfWar fog {FogOfWar::Kind::Shoot};
        fog.RequestRebuild();

        auto input = MakeInput({10, 10}, nanotime {});
        input.Distance = 3;
        fog.Prepare(input);

        CHECK(fog.GetPoints().size() == 10);
        CHECK(fog.GetPoints().front().PointColor.comp.a == 255);
    }

    SECTION("Produces no points when disabled")
    {
        FogOfWar fog {FogOfWar::Kind::Look};
        fog.RequestRebuild();

        auto input = MakeInput({10, 10}, nanotime {});
        input.Enabled = false;
        fog.Prepare(input);

        CHECK(fog.GetPoints().empty());
    }

    SECTION("Interpolates between previous and rebuilt fog")
    {
        FogOfWar fog {FogOfWar::Kind::Look};
        fog.RequestRebuild();

        auto input = MakeInput({10, 10}, nanotime {});
        fog.Prepare(input);
        const auto initial_x = fog.GetPoints().front().PointPos.x;

        fog.RequestRebuild();
        input = MakeInput({20, 10}, nanotime {timespan {std::chrono::milliseconds {0}}}, 200);
        fog.Prepare(input);

        CHECK(fog.GetPoints().front().PointPos.x == initial_x);

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {100}}};
        fog.Prepare(input);
        const auto mid_x = fog.GetPoints().front().PointPos.x;

        CHECK(mid_x > initial_x);
        CHECK(mid_x < 1905);

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);

        CHECK(fog.GetPoints().front().PointPos.x == 1905);
    }

    SECTION("Collapses to origin when disabled")
    {
        FogOfWar fog {FogOfWar::Kind::Look};

        auto input = MakeInput({10, 10}, nanotime {timespan {std::chrono::milliseconds {0}}}, 200);
        fog.Prepare(input);
        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);

        CHECK(!fog.GetPoints().empty());
        const auto edge_x = fog.GetPoints().front().PointPos.x;
        const auto center_x = fog.GetPoints()[2].PointPos.x;
        CHECK(edge_x != center_x);

        input.Enabled = false;
        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);
        CHECK(!fog.GetPoints().empty());

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {300}}};
        fog.Prepare(input);
        CHECK(!fog.GetPoints().empty());
        const auto mid_x = fog.GetPoints().front().PointPos.x;
        CHECK(mid_x != edge_x);

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {400}}};
        fog.Prepare(input);
        CHECK(fog.GetPoints().empty());
    }

    SECTION("Expands from origin when re-enabled")
    {
        FogOfWar fog {FogOfWar::Kind::Look};

        auto input = MakeInput({10, 10}, nanotime {timespan {std::chrono::milliseconds {0}}}, 200);
        fog.Prepare(input);
        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);
        const auto final_edge_x = fog.GetPoints().front().PointPos.x;

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
        const auto center_x = fog.GetPoints()[2].PointPos.x;
        CHECK(fog.GetPoints().front().PointPos.x == center_x);

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {600}}};
        fog.Prepare(input);
        CHECK(fog.GetPoints().front().PointPos.x == final_edge_x);
    }

    SECTION("First appearance expands from origin")
    {
        FogOfWar fog {FogOfWar::Kind::Look};

        auto input = MakeInput({10, 10}, nanotime {timespan {std::chrono::milliseconds {0}}}, 200);
        fog.Prepare(input);

        CHECK(!fog.GetPoints().empty());
        const auto center_x = fog.GetPoints()[2].PointPos.x;
        CHECK(fog.GetPoints().front().PointPos.x == center_x);

        input.FrameTime = nanotime {timespan {std::chrono::milliseconds {200}}};
        fog.Prepare(input);
        CHECK(fog.GetPoints().front().PointPos.x != center_x);
    }

    SECTION("Blocked edge points use base draw offset")
    {
        FogOfWar fog {FogOfWar::Kind::Look};
        fog.SetDrawOffset({100, 50});
        fog.SetBaseDrawOffset({100, 0});

        auto input = MakeInput({10, 10}, nanotime {});
        input.TraceBulletToBlock = [](mpos start, mpos, int32, bool) { return start; };
        fog.Prepare(input);

        CHECK(!fog.GetPoints().empty());
        CHECK(fog.GetPoints()[0].PointOffset != nullptr);
        CHECK(*fog.GetPoints()[0].PointOffset == ipos32 {100, 0});
        CHECK(fog.GetPoints()[2].PointOffset != nullptr);
        CHECK(*fog.GetPoints()[2].PointOffset == ipos32 {100, 50});
    }
}

FO_END_NAMESPACE
