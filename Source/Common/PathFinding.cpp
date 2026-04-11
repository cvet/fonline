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

#include "PathFinding.h"

FO_BEGIN_NAMESPACE

auto PathFinding::CheckHexWithMultihex(mpos hex, uint8 dir, int32 multihex, msize map_size, const function<HexBlockResult(mpos)>& check_hex) -> HexBlockResult
{
    FO_STACK_TRACE_ENTRY();

    // Single hex: just check center
    auto worst = check_hex(hex);

    if (worst == HexBlockResult::Blocked || multihex == 0) {
        return worst;
    }

    const auto update_worst = [&worst](HexBlockResult result) -> bool {
        if (result == HexBlockResult::Blocked) {
            worst = result;
            return true; // Short-circuit
        }
        if (static_cast<int8>(result) > static_cast<int8>(worst)) {
            worst = result;
        }
        return false;
    };

    // Extend base hex in movement direction
    auto raw_extended = ipos32 {hex.x, hex.y};

    for (int32 k = 0; k < multihex; k++) {
        GeometryHelper::MoveHexByDirUnsafe(raw_extended, dir);
    }

    if (!map_size.is_valid_pos(raw_extended)) {
        return HexBlockResult::Blocked;
    }

    if (update_worst(check_hex(map_size.from_raw_pos(raw_extended)))) {
        return worst;
    }

    // CW/CCW perimeter from extended hex
    const bool is_square_corner = (dir % 2) != 0 && !GameSettings::HEXAGONAL_GEOMETRY;
    const int32 steps_count = is_square_corner ? multihex * 2 : multihex;

    // Clockwise
    {
        uint8 cw_dir;

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            cw_dir = (dir + 4) % 6;
        }
        else {
            cw_dir = (dir + 6) % 8;
        }

        if (is_square_corner) {
            cw_dir = (cw_dir + 1) % 8;
        }

        auto raw_hex = raw_extended;

        for (int32 k = 0; k < steps_count; k++) {
            GeometryHelper::MoveHexByDirUnsafe(raw_hex, cw_dir);
            FO_RUNTIME_ASSERT(map_size.is_valid_pos(raw_hex));

            if (update_worst(check_hex(map_size.from_raw_pos(raw_hex)))) {
                return worst;
            }
        }
    }

    // Counter-clockwise
    {
        uint8 ccw_dir;

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            ccw_dir = (dir + 2) % 6;
        }
        else {
            ccw_dir = (dir + 2) % 8;
        }

        if (is_square_corner) {
            ccw_dir = (ccw_dir + 7) % 8;
        }

        auto raw_hex = raw_extended;

        for (int32 k = 0; k < steps_count; k++) {
            GeometryHelper::MoveHexByDirUnsafe(raw_hex, ccw_dir);
            FO_RUNTIME_ASSERT(map_size.is_valid_pos(raw_hex));

            if (update_worst(check_hex(map_size.from_raw_pos(raw_hex)))) {
                return worst;
            }
        }
    }

    return worst;
}

auto PathFinding::FindPath(const FindPathInput& input) -> FindPathOutput
{
    FO_STACK_TRACE_ENTRY();

    FindPathOutput output;

    const auto map_size = input.MapSize;

    if (!map_size.is_valid_pos(input.FromHex) || !map_size.is_valid_pos(input.ToHex)) {
        output.Result = FindPathOutput::ResultType::InvalidHexes;
        return output;
    }

    if (GeometryHelper::CheckDist(input.FromHex, input.ToHex, input.Cut)) {
        output.Result = FindPathOutput::ResultType::AlreadyHere;
        return output;
    }

    // Prepare grid
    const auto max_len = input.MaxLength;
    const auto grid_side = numeric_cast<size_t>(max_len * 2 + 2);
    vector<int16> grid_buffer;
    grid_buffer.assign(grid_side * grid_side, 0);

    const auto grid_offset = input.FromHex;
    const auto grid_at = [&](mpos hex) -> int16& { return grid_buffer[((max_len + 1) + hex.y - grid_offset.y) * numeric_cast<int32>(grid_side) + ((max_len + 1) + hex.x - grid_offset.x)]; };

    vector<mpos> next_hexes;
    vector<mpos> gag_hexes;
    vector<mpos> cr_hexes;
    next_hexes.reserve(1024);
    gag_hexes.reserve(64);
    cr_hexes.reserve(64);

    // Begin BFS
    auto to_hex = input.ToHex;
    grid_at(input.FromHex) = 1;
    next_hexes.emplace_back(input.FromHex);

    while (true) {
        bool find_ok = false;
        const auto next_hexes_round = next_hexes.size();
        FO_RUNTIME_ASSERT(next_hexes_round != 0);

        for (size_t i = 0; i < next_hexes_round; i++) {
            const auto cur_hex = next_hexes[i];

            if (GeometryHelper::CheckDist(cur_hex, to_hex, input.Cut)) {
                to_hex = cur_hex;
                find_ok = true;
                break;
            }

            const auto next_hex_index = numeric_cast<int16>(grid_at(cur_hex) + 1);

            if (next_hex_index > max_len) {
                output.Result = FindPathOutput::ResultType::TooFar;
                return output;
            }

            for (int32 j = 0; j < GameSettings::MAP_DIR_COUNT; j++) {
                auto raw_next_hex = ipos32 {cur_hex.x, cur_hex.y};
                GeometryHelper::MoveHexByDirUnsafe(raw_next_hex, static_cast<uint8>(j));

                if (!map_size.is_valid_pos(raw_next_hex)) {
                    continue;
                }

                const auto next_hex = map_size.from_raw_pos(raw_next_hex);
                auto& grid_cell = grid_at(next_hex);

                if (grid_cell != 0) {
                    continue;
                }

                const auto block = CheckHexWithMultihex(next_hex, static_cast<uint8>(j), input.Multihex, map_size, input.CheckHex);

                if (block == HexBlockResult::Passable) {
                    next_hexes.emplace_back(next_hex);
                    grid_cell = next_hex_index;
                }
                else if (block == HexBlockResult::DeferGag) {
                    grid_cell = numeric_cast<int16>(next_hex_index | 0x4000);
                    gag_hexes.emplace_back(next_hex);
                }
                else if (block == HexBlockResult::DeferCritter) {
                    grid_cell = numeric_cast<int16>(next_hex_index | 0x4000);
                    cr_hexes.emplace_back(next_hex);
                }
                else {
                    grid_cell = -1;
                }
            }
        }

        if (find_ok) {
            break;
        }

        next_hexes.erase(next_hexes.begin(), next_hexes.begin() + static_cast<ptrdiff_t>(next_hexes_round));

        // Add gag hex after some distance
        if (!gag_hexes.empty() && !next_hexes.empty()) {
            const auto last_index = grid_at(next_hexes.back());
            const auto& gag_hex = gag_hexes.front();
            const auto gag_index = numeric_cast<int16>(grid_at(gag_hex) ^ 0x4000);

            if (gag_index + 10 < last_index) {
                grid_at(gag_hex) = gag_index;
                next_hexes.emplace_back(gag_hex);
                gag_hexes.erase(gag_hexes.begin());
            }
        }

        // If no way then route through gag/critter
        if (next_hexes.empty()) {
            if (!gag_hexes.empty()) {
                auto& gag_hex = gag_hexes.front();
                grid_at(gag_hex) ^= 0x4000;
                next_hexes.emplace_back(gag_hex);
                gag_hexes.erase(gag_hexes.begin());
            }
            else if (!cr_hexes.empty()) {
                auto& cr_hex = cr_hexes.front();
                grid_at(cr_hex) ^= 0x4000;
                next_hexes.emplace_back(cr_hex);
                cr_hexes.erase(cr_hexes.begin());
            }
        }

        if (next_hexes.empty()) {
            output.Result = FindPathOutput::ResultType::NoWay;
            return output;
        }
    }

    // Reconstruct path (backtrack from target to source using angle-based direction selection)
    vector<uint8> raw_steps;
    auto hex_index = grid_at(to_hex);
    auto cur_hex = to_hex;
    raw_steps.resize(hex_index - 1);
    float32 base_angle = GeometryHelper::GetDirAngle(to_hex, input.FromHex);

    while (hex_index > 1) {
        hex_index--;

        int32 best_step_dir = -1;
        float32 best_step_angle_diff = 0.0f;

        const auto check_hex = [&](int32 dir, ipos32 step_raw_hex) {
            if (!map_size.is_valid_pos(step_raw_hex)) {
                return;
            }

            const auto step_hex = map_size.from_raw_pos(step_raw_hex);

            if (grid_at(step_hex) != hex_index) {
                return;
            }

            const float32 angle = GeometryHelper::GetDirAngle(step_hex, input.FromHex);
            const float32 angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);

            if (best_step_dir == -1 || hex_index == 0) {
                best_step_dir = dir;
                best_step_angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);
            }
            else if (angle_diff < best_step_angle_diff) {
                best_step_dir = dir;
                best_step_angle_diff = angle_diff;
            }
        };

        bool step_ok = false;

        if ((cur_hex.x % 2) != 0) {
            check_hex(3, ipos32 {cur_hex.x - 1, cur_hex.y - 1});
            check_hex(2, ipos32 {cur_hex.x, cur_hex.y - 1});
            check_hex(5, ipos32 {cur_hex.x, cur_hex.y + 1});
            check_hex(0, ipos32 {cur_hex.x + 1, cur_hex.y});
            check_hex(4, ipos32 {cur_hex.x - 1, cur_hex.y});
            check_hex(1, ipos32 {cur_hex.x + 1, cur_hex.y - 1});

            if (best_step_dir == 3) {
                raw_steps[hex_index - 1] = 3;
                cur_hex.x--;
                cur_hex.y--;
                step_ok = true;
            }
            else if (best_step_dir == 2) {
                raw_steps[hex_index - 1] = 2;
                cur_hex.y--;
                step_ok = true;
            }
            else if (best_step_dir == 5) {
                raw_steps[hex_index - 1] = 5;
                cur_hex.y++;
                step_ok = true;
            }
            else if (best_step_dir == 0) {
                raw_steps[hex_index - 1] = 0;
                cur_hex.x++;
                step_ok = true;
            }
            else if (best_step_dir == 4) {
                raw_steps[hex_index - 1] = 4;
                cur_hex.x--;
                step_ok = true;
            }
            else if (best_step_dir == 1) {
                raw_steps[hex_index - 1] = 1;
                cur_hex.x++;
                cur_hex.y--;
                step_ok = true;
            }
        }
        else {
            check_hex(3, ipos32 {cur_hex.x - 1, cur_hex.y});
            check_hex(2, ipos32 {cur_hex.x, cur_hex.y - 1});
            check_hex(5, ipos32 {cur_hex.x, cur_hex.y + 1});
            check_hex(0, ipos32 {cur_hex.x + 1, cur_hex.y + 1});
            check_hex(4, ipos32 {cur_hex.x - 1, cur_hex.y + 1});
            check_hex(1, ipos32 {cur_hex.x + 1, cur_hex.y});

            if (best_step_dir == 3) {
                raw_steps[hex_index - 1] = 3;
                cur_hex.x--;
                step_ok = true;
            }
            else if (best_step_dir == 2) {
                raw_steps[hex_index - 1] = 2;
                cur_hex.y--;
                step_ok = true;
            }
            else if (best_step_dir == 5) {
                raw_steps[hex_index - 1] = 5;
                cur_hex.y++;
                step_ok = true;
            }
            else if (best_step_dir == 0) {
                raw_steps[hex_index - 1] = 0;
                cur_hex.x++;
                cur_hex.y++;
                step_ok = true;
            }
            else if (best_step_dir == 4) {
                raw_steps[hex_index - 1] = 4;
                cur_hex.x--;
                cur_hex.y++;
                step_ok = true;
            }
            else if (best_step_dir == 1) {
                raw_steps[hex_index - 1] = 1;
                cur_hex.x++;
                step_ok = true;
            }
        }

        if (!step_ok) {
            output.Result = FindPathOutput::ResultType::BacktraceError;
            return output;
        }
    }

    if (raw_steps.empty()) {
        output.Result = FindPathOutput::ResultType::AlreadyHere;
        return output;
    }

    // Compile steps with control points
    if (input.FreeMovement) {
        mpos trace_hex = input.FromHex;

        while (true) {
            mpos trace_hex2 = to_hex;

            for (auto i = numeric_cast<int32>(raw_steps.size()) - 1; i >= 0; i--) {
                LineTracer tracer(trace_hex, trace_hex2, 0.0f, map_size);
                mpos next_hex = trace_hex;
                vector<uint8> direct_steps;
                bool failed = false;

                while (true) {
                    uint8 dir = tracer.GetNextHex(next_hex);
                    direct_steps.emplace_back(dir);

                    if (next_hex == trace_hex2) {
                        break;
                    }

                    if (grid_at(next_hex) <= 0) {
                        failed = true;
                        break;
                    }
                }

                if (failed) {
                    FO_RUNTIME_ASSERT(i > 0);
                    GeometryHelper::MoveHexByDir(trace_hex2, GeometryHelper::ReverseDir(raw_steps[i]), map_size);
                    continue;
                }

                for (const auto& ds : direct_steps) {
                    output.Steps.emplace_back(ds);
                }

                output.ControlSteps.emplace_back(numeric_cast<uint16>(output.Steps.size()));

                trace_hex = trace_hex2;
                break;
            }

            if (trace_hex2 == to_hex) {
                break;
            }
        }
    }
    else {
        for (size_t i = 0; i < raw_steps.size(); i++) {
            const auto cur_dir = raw_steps[i];
            output.Steps.emplace_back(cur_dir);

            for (size_t j = i + 1; j < raw_steps.size(); j++) {
                if (raw_steps[j] == cur_dir) {
                    output.Steps.emplace_back(cur_dir);
                    i++;
                }
                else {
                    break;
                }
            }

            output.ControlSteps.emplace_back(numeric_cast<uint16>(output.Steps.size()));
        }
    }

    FO_RUNTIME_ASSERT(!output.Steps.empty());
    FO_RUNTIME_ASSERT(!output.ControlSteps.empty());

    output.Result = FindPathOutput::ResultType::Ok;
    output.NewToHex = to_hex;
    return output;
}

auto PathFinding::TraceLine(const TraceLineInput& input) -> TraceLineOutput
{
    FO_STACK_TRACE_ENTRY();

    TraceLineOutput output;

    const auto dist = input.MaxDist != 0 ? input.MaxDist : GeometryHelper::GetDistance(input.StartHex, input.TargetHex);
    auto tracer = LineTracer(input.StartHex, input.TargetHex, input.Angle, input.MapSize);
    auto next_hex = input.StartHex;
    auto prev_hex = next_hex;
    bool last_passed_ok = false;

    for (int32 i = 0;; i++) {
        if (i >= dist) {
            output.IsFullTrace = true;
            break;
        }

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            tracer.GetNextHex(next_hex);
        }
        else {
            tracer.GetNextSquare(next_hex);
        }

        if (input.CheckLastMovable && !last_passed_ok) {
            if (input.IsHexMovable && input.IsHexMovable(next_hex)) {
                output.LastMovable = next_hex;
                output.HasLastMovable = true;
            }
            else {
                last_passed_ok = true;
            }
        }

        if (input.IsHexBlocked(next_hex)) {
            break;
        }

        prev_hex = next_hex;
    }

    output.PreBlock = prev_hex;
    output.Block = next_hex;
    return output;
}

FO_END_NAMESPACE
