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

auto PathFinding::CheckHexWithMultihex(mpos hex, mdir dir, int32_t multihex, msize map_size, const function<HexBlockResult(mpos)>& check_hex) -> HexBlockResult
{
    FO_STACK_TRACE_ENTRY();

    // Single hex: just check center
    auto worst = check_hex(hex);

    if (worst == HexBlockResult::Blocked || multihex == 0) {
        return worst;
    }

    auto update_worst = [&worst](HexBlockResult result) -> bool {
        if (result == HexBlockResult::Blocked) {
            worst = result;
            return true; // Short-circuit
        }
        if (static_cast<int8_t>(result) > static_cast<int8_t>(worst)) {
            worst = result;
        }
        return false;
    };

    // Extend base hex in movement direction
    ipos32 raw_extended = ipos32 {hex.x, hex.y};

    for (int32_t k = 0; k < multihex; k++) {
        GeometryHelper::MoveHexByDirUnsafe(raw_extended, dir);
    }

    if (!map_size.is_valid_pos(raw_extended)) {
        return HexBlockResult::Blocked;
    }

    if (update_worst(check_hex(map_size.from_raw_pos(raw_extended)))) {
        return worst;
    }

    // CW/CCW perimeter from extended hex
    bool is_square_corner = (dir.hex().value() % 2) != 0 && !GameSettings::HEXAGONAL_GEOMETRY;
    int32_t steps_count = is_square_corner ? multihex * 2 : multihex;

    // Clockwise
    {
        mdir cw_dir;

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            cw_dir = dir.rotateHex(4);
        }
        else {
            cw_dir = dir.rotateHex(6);
        }

        if (is_square_corner) {
            cw_dir = cw_dir.rotateHex(1);
        }

        ipos32 raw_hex = raw_extended;

        for (int32_t k = 0; k < steps_count; k++) {
            GeometryHelper::MoveHexByDirUnsafe(raw_hex, cw_dir);

            if (!map_size.is_valid_pos(raw_hex)) {
                return HexBlockResult::Blocked;
            }

            if (update_worst(check_hex(map_size.from_raw_pos(raw_hex)))) {
                return worst;
            }
        }
    }

    // Counter-clockwise
    {
        mdir ccw_dir;

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            ccw_dir = dir.rotateHex(2);
        }
        else {
            ccw_dir = dir.rotateHex(2);
        }

        if (is_square_corner) {
            ccw_dir = ccw_dir.rotateHex(7);
        }

        ipos32 raw_hex = raw_extended;

        for (int32_t k = 0; k < steps_count; k++) {
            GeometryHelper::MoveHexByDirUnsafe(raw_hex, ccw_dir);

            if (!map_size.is_valid_pos(raw_hex)) {
                return HexBlockResult::Blocked;
            }

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

    msize map_size = input.MapSize;

    if (!map_size.is_valid_pos(input.FromHex) || (!input.CheckTarget && !map_size.is_valid_pos(input.ToHex))) {
        output.Result = FindPathOutput::ResultType::InvalidHexes;
        return output;
    }

    if ((input.CheckTarget && input.CheckTarget(input.FromHex)) || (!input.CheckTarget && GeometryHelper::CheckDist(input.FromHex, input.ToHex, input.Cut))) {
        output.Result = FindPathOutput::ResultType::AlreadyHere;

        if (input.CheckTarget) {
            output.NewToHex = input.FromHex;
        }

        return output;
    }

    // Prepare grid
    int32_t max_len = input.MaxLength;
    auto grid_side = numeric_cast<size_t>(max_len * 2 + 2);
    vector<int16_t> grid_buffer;
    vector<mpos> next_hexes;
    vector<mpos> gag_hexes;
    vector<mpos> cr_hexes;
    grid_buffer.assign(grid_side * grid_side, 0);
    next_hexes.reserve(1024);
    gag_hexes.reserve(128);
    cr_hexes.reserve(128);

    mpos grid_offset = input.FromHex;
    auto grid_at = [&](mpos hex) -> ptr<int16_t> { return make_ptr(&grid_buffer[((max_len + 1) + hex.y - grid_offset.y) * numeric_cast<int32_t>(grid_side) + ((max_len + 1) + hex.x - grid_offset.x)]); };

    size_t next_hexes_read = 0;
    size_t gag_hexes_read = 0;
    size_t cr_hexes_read = 0;

    // Begin BFS
    mpos to_hex = input.ToHex;
    *grid_at(input.FromHex) = 1;
    next_hexes.emplace_back(input.FromHex);

    while (true) {
        bool find_ok = false;
        size_t round_begin = next_hexes_read;
        auto round_end = next_hexes.size();
        FO_VERIFY_AND_THROW(round_end > round_begin, "Pathfinding breadth-first search exhausted its frontier before finding a path", input.FromHex, input.ToHex, input.Cut, round_begin, round_end, next_hexes.size());

        for (size_t i = round_begin; i < round_end; i++) {
            auto cur_hex = next_hexes[i];

            if ((input.CheckTarget && input.CheckTarget(cur_hex)) || (!input.CheckTarget && GeometryHelper::CheckDist(cur_hex, to_hex, input.Cut))) {
                to_hex = cur_hex;
                find_ok = true;
                break;
            }

            int16_t next_hex_index = numeric_cast<int16_t>(*grid_at(cur_hex) + 1);

            if (next_hex_index > max_len) {
                output.Result = FindPathOutput::ResultType::TooFar;
                return output;
            }

            for (int32_t j = 0; j < GameSettings::MAP_DIR_COUNT; j++) {
                ipos32 raw_next_hex = ipos32 {cur_hex.x, cur_hex.y};
                GeometryHelper::MoveHexByDirUnsafe(raw_next_hex, hdir(j));

                if (!map_size.is_valid_pos(raw_next_hex)) {
                    continue;
                }

                mpos next_hex = map_size.from_raw_pos(raw_next_hex);
                auto grid_cell = grid_at(next_hex);

                if (*grid_cell != 0) {
                    continue;
                }

                auto block = CheckHexWithMultihex(next_hex, hdir(j), input.Multihex, map_size, input.CheckHex);

                if (block == HexBlockResult::Passable) {
                    next_hexes.emplace_back(next_hex);
                    *grid_cell = next_hex_index;
                }
                else if (block == HexBlockResult::DeferGag) {
                    *grid_cell = numeric_cast<int16_t>(next_hex_index | 0x4000);
                    gag_hexes.emplace_back(next_hex);
                }
                else if (block == HexBlockResult::DeferCritter) {
                    *grid_cell = numeric_cast<int16_t>(next_hex_index | 0x4000);
                    cr_hexes.emplace_back(next_hex);
                }
                else {
                    *grid_cell = -1;
                }
            }
        }

        if (find_ok) {
            break;
        }

        // Consume the round's prefix without shifting the vector.
        next_hexes_read = round_end;

        // Add gag hex after some distance
        if (gag_hexes_read < gag_hexes.size() && next_hexes_read < next_hexes.size()) {
            int16_t last_index = *grid_at(next_hexes.back());
            const auto& gag_hex = gag_hexes[gag_hexes_read];
            int16_t gag_index = numeric_cast<int16_t>(*grid_at(gag_hex) ^ 0x4000);

            if (gag_index + 10 < last_index) {
                *grid_at(gag_hex) = gag_index;
                next_hexes.emplace_back(gag_hex);
                gag_hexes_read++;
            }
        }

        // If no way then route through gag/critter
        if (next_hexes_read >= next_hexes.size()) {
            if (gag_hexes_read < gag_hexes.size()) {
                auto& gag_hex = gag_hexes[gag_hexes_read];
                *grid_at(gag_hex) ^= 0x4000;
                next_hexes.emplace_back(gag_hex);
                gag_hexes_read++;
            }
            else if (cr_hexes_read < cr_hexes.size()) {
                auto& cr_hex = cr_hexes[cr_hexes_read];
                *grid_at(cr_hex) ^= 0x4000;
                next_hexes.emplace_back(cr_hex);
                cr_hexes_read++;
            }
        }

        if (next_hexes_read >= next_hexes.size()) {
            output.Result = FindPathOutput::ResultType::NoWay;
            return output;
        }
    }

    // Reconstruct path (backtrack from target to source using angle-based direction selection)
    vector<mdir> raw_steps;
    int16_t hex_index = *grid_at(to_hex);
    mpos cur_hex = to_hex;
    raw_steps.resize(hex_index - 1);
    float32_t base_angle = GeometryHelper::GetDirAngle(to_hex, input.FromHex);

    while (hex_index > 1) {
        hex_index--;

        mdir best_step_dir;
        mpos best_step_hex;
        bool step_ok = false;
        float32_t best_step_angle_diff = 0.0f;

        auto check_hex = [&](mdir dir, ipos32 step_raw_hex) {
            if (!map_size.is_valid_pos(step_raw_hex)) {
                return;
            }

            mpos step_hex = map_size.from_raw_pos(step_raw_hex);

            if (*grid_at(step_hex) != hex_index) {
                return;
            }

            float32_t angle = GeometryHelper::GetDirAngle(step_hex, input.FromHex);
            float32_t angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);

            if (!step_ok) {
                best_step_dir = dir;
                best_step_hex = step_hex;
                best_step_angle_diff = angle_diff;
                step_ok = true;
            }
            else if (angle_diff < best_step_angle_diff) {
                best_step_dir = dir;
                best_step_hex = step_hex;
                best_step_angle_diff = angle_diff;
            }
        };

        for (int32_t dir_value = 0; dir_value < GameSettings::MAP_DIR_COUNT; dir_value++) {
            mdir dir = hdir(dir_value);
            ipos32 step_raw_hex {cur_hex.x, cur_hex.y};
            GeometryHelper::MoveHexByDirUnsafe(step_raw_hex, dir.reverse());
            check_hex(dir, step_raw_hex);
        }

        if (!step_ok) {
            output.Result = FindPathOutput::ResultType::BacktraceError;
            return output;
        }

        raw_steps[hex_index - 1] = best_step_dir;
        cur_hex = best_step_hex;
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

            for (int32_t i = numeric_cast<int32_t>(raw_steps.size()) - 1; i >= 0; i--) {
                LineTracer tracer(trace_hex, trace_hex2, 0.0f, map_size);
                mpos next_hex = trace_hex;
                small_vector<mdir, 64> direct_steps;
                bool failed = false;

                while (true) {
                    auto dir = tracer.GetNextHex(next_hex);

                    if (!dir.has_value()) {
                        failed = true;
                        break;
                    }

                    direct_steps.emplace_back(dir.value());

                    if (next_hex == trace_hex2) {
                        break;
                    }

                    if (*grid_at(next_hex) <= 0) {
                        failed = true;
                        break;
                    }
                }

                if (failed) {
                    FO_VERIFY_AND_THROW(i > 0, "I must be positive", i);
                    GeometryHelper::MoveHexByDir(trace_hex2, raw_steps[i].reverse(), map_size);
                    continue;
                }

                for (const auto& ds : direct_steps) {
                    output.Steps.emplace_back(ds);
                }

                output.ControlSteps.emplace_back(numeric_cast<uint16_t>(output.Steps.size()));

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
            auto cur_dir = raw_steps[i];
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

            output.ControlSteps.emplace_back(numeric_cast<uint16_t>(output.Steps.size()));
        }
    }

    FO_VERIFY_AND_THROW(!output.Steps.empty(), "Pathfinding produced no movement steps for an otherwise successful path", input.FromHex, input.ToHex, input.Cut);
    FO_VERIFY_AND_THROW(!output.ControlSteps.empty(), "Pathfinding produced no control steps for an otherwise successful path", input.FromHex, input.ToHex, output.Steps.size());

    output.Result = FindPathOutput::ResultType::Ok;
    output.NewToHex = to_hex;

    if (input.FreeMovement) {
        mpos target_hex = input.CheckTarget ? output.NewToHex : input.ToHex;
        ipos16 target_hex_offset = input.CheckTarget ? ipos16 {} : input.ToHexOffset;
        auto end_offset = EvaluateFreeMovementEndOffset(output.NewToHex, target_hex, target_hex_offset);
        output.EndHexOffset = end_offset.value_or(input.FromHexOffset);
    }

    return output;
}

auto PathFinding::TraceLine(const TraceLineInput& input) -> TraceLineOutput
{
    FO_STACK_TRACE_ENTRY();

    TraceLineOutput output;

    int32_t dist = input.MaxDist != 0 ? input.MaxDist : GeometryHelper::GetDistance(input.StartHex, input.TargetHex);
    auto tracer = LineTracer(input.StartHex, input.TargetHex, input.Angle, input.MapSize);
    mpos next_hex = input.StartHex;
    mpos prev_hex = next_hex;
    bool last_passed_ok = false;

    for (int32_t i = 0;; i++) {
        if (i >= dist) {
            output.FullyTraced = true;
            break;
        }

        if (!tracer.GetNextHex(next_hex).has_value()) {
            break;
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

auto PathFinding::EvaluateFreeMovementEndOffset(mpos new_to_hex, mpos to_hex, ipos16 to_hex_offset) -> optional<ipos16>
{
    FO_STACK_TRACE_ENTRY();

    // Work in map pixel space with the final hex center as the origin.
    // C = final hex center -> target hex center; the real target adds the target's own sub-hex offset.
    ipos32 center_to_hex = GeometryHelper::GetHexOffset(new_to_hex, to_hex);
    float32_t target_x = numeric_cast<float32_t>(center_to_hex.x + to_hex_offset.x);
    float32_t target_y = numeric_cast<float32_t>(center_to_hex.y + to_hex_offset.y);

    // Distance uses the camera Y projection (same metric as MovingContext segment distances).
    float32_t y_proj = GeometryHelper::GetYProj();
    float32_t target_proj_y = target_y * y_proj;
    float32_t target_len = std::sqrt(target_x * target_x + target_proj_y * target_proj_y);

    constexpr float32_t min_len = 0.5f;

    if (target_len < min_len) {
        // Already on the target.
        return std::nullopt;
    }

    // Gap to preserve = continuous distance between the final hex center and the target hex center (the "cut" gap).
    float32_t gap_x = numeric_cast<float32_t>(center_to_hex.x);
    float32_t gap_proj_y = numeric_cast<float32_t>(center_to_hex.y) * y_proj;
    float32_t gap_len = std::sqrt(gap_x * gap_x + gap_proj_y * gap_proj_y);

    // Stand at gap_len from the real target, on the final-hex side of it.
    float32_t factor = 1.0f - gap_len / target_len;
    int32_t ox = iround<int32_t>(factor * target_x);
    int32_t oy = iround<int32_t>(factor * target_y);

    constexpr int32_t half_w = GameSettings::MAP_HEX_WIDTH / 2;
    constexpr int32_t half_h = GameSettings::MAP_HEX_HEIGHT / 2;

    int16_t clamped_ox = numeric_cast<int16_t>(std::clamp(ox, -half_w, half_w));
    int16_t clamped_oy = numeric_cast<int16_t>(std::clamp(oy, -half_h, half_h));
    return ipos16 {clamped_ox, clamped_oy};
}

FO_END_NAMESPACE
