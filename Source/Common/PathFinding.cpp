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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

// A gag costs as much as a detour this many steps long, so a short way around is preferred to it
static constexpr int32_t GAG_DETOUR_STEPS = 10;
// Critters walked through count above every step, so any route with fewer of them is cheaper
static constexpr int64_t CRITTER_COST = int64_t {1} << 32;
// A step raises an estimate by its cost and by at most one hex of distance, so every push lands within the ring
static constexpr int32_t OPEN_BUCKETS = 16;
static constexpr int32_t SEARCH_BLOCK_SHIFT = 4;
static constexpr int32_t SEARCH_BLOCK_SIDE = 1 << SEARCH_BLOCK_SHIFT;
static constexpr int32_t SEARCH_BLOCK_CELLS = SEARCH_BLOCK_SIDE * SEARCH_BLOCK_SIDE;

static_assert(1 + GAG_DETOUR_STEPS + 1 < OPEN_BUCKETS);

// One hex of a search, zeroed when its block is first touched
struct PathSearchCell
{
    int64_t Cost {}; // Route cost from the start: steps and gag detours in the low half, critters in the high half
    int32_t Steps {};
    uint8_t Answer {}; // CheckHex answer plus two, zero until asked
    bool Reached {};
    bool Closed {};
    bool Probed {};
};

struct PathSearchNode
{
    int64_t Estimate {};
    uint32_t Order {};
    mpos Hex {};
};

// Open list of a search whose estimate never falls below the one taken last: one critter tier sits in a ring of
// buckets taken in push order, and only a costlier tier waits in a heap, so a request pops the same way everywhere
class PathOpenList
{
public:
    [[nodiscard]] auto IsEmpty() const -> bool { return _bucketed == 0 && _later.empty(); }

    void Push(int64_t estimate, mpos hex);
    auto Pop() -> PathSearchNode;

private:
    void AdmitLater();

    array<vector<PathSearchNode>, OPEN_BUCKETS> _buckets {};
    vector<PathSearchNode> _later {};
    int64_t _front {};
    size_t _frontRead {};
    size_t _bucketed {};
    uint32_t _order {};
};

// Search state for the hexes a search touches, allocated in square blocks on first touch, so a search along
// a corridor pays for the corridor rather than for the whole map
class PathSearchGrid
{
public:
    explicit PathSearchGrid(msize map_size);

    [[nodiscard]] auto FindCell(mpos hex) const -> nptr<const PathSearchCell>;
    // An index stays valid for the whole search, a reference only until a new block is touched
    [[nodiscard]] auto GetCellIndex(mpos hex) -> int32_t;
    [[nodiscard]] auto GetCell(int32_t index) -> PathSearchCell& { return _cells[index]; }
    [[nodiscard]] auto GetCell(mpos hex) -> PathSearchCell& { return _cells[GetCellIndex(hex)]; }

private:
    auto GetBlockSlot(mpos hex) const -> int32_t { return (hex.y >> SEARCH_BLOCK_SHIFT) * _blocksPerRow + (hex.x >> SEARCH_BLOCK_SHIFT); }
    static auto GetCellSlot(int32_t block, mpos hex) -> int32_t { return block * SEARCH_BLOCK_CELLS + (hex.y & (SEARCH_BLOCK_SIDE - 1)) * SEARCH_BLOCK_SIDE + (hex.x & (SEARCH_BLOCK_SIDE - 1)); }

    int32_t _blocksPerRow;
    vector<int32_t> _blockIndex {};
    vector<PathSearchCell> _cells {};
};

// A* over the hexes CheckHex lets the mover enter. A step costs one, a gag GAG_DETOUR_STEPS more, a critter more
// than any route without one, and no route may run longer than MaxLength steps
class PathSearch
{
public:
    explicit PathSearch(const FindPathInput& input);

    [[nodiscard]] auto GetGoal() const -> mpos { return _goal; }
    [[nodiscard]] auto IsLineHexBlocked(mpos hex, mdir dir) -> bool;

    auto Run() -> FindPathOutput::ResultType;
    auto Backtrack(vector<mdir>& raw_steps) -> bool;

private:
    auto GetDeviation(mpos hex) const -> int32_t;
    auto CheckEntry(mpos hex, mdir dir) -> HexBlockResult;
    auto CheckHexOnce(mpos hex) -> HexBlockResult;
    auto IsTargetWalledOff() -> bool;
    auto VisitProbeHex(mpos hex, vector<mpos>& region) -> bool;
    void Expand(mpos hex, int64_t cost, int32_t steps);

    ptr<const FindPathInput> _input;
    PathSearchGrid _grid;
    function<HexBlockResult(mpos)> _checkHexOnce;
    PathOpenList _open {};
    ipos32 _lineStart {};
    ipos32 _line {};
    size_t _reached {};
    bool _cutByLength {};
    mpos _goal {};
};

static auto IsPathGoal(const FindPathInput& input, mpos hex) -> bool;
static auto GetStepsToGoal(const FindPathInput& input, mpos hex) -> int32_t;
static auto GetEntryCost(HexBlockResult block) -> int64_t;
static auto GetPlanePos(mpos hex) -> ipos32;
static auto GetBucket(int64_t estimate) -> size_t;
static auto IsNodeAfter(const PathSearchNode& a, const PathSearchNode& b) -> bool;

auto PathFinding::CheckHexWithMultihex(mpos hex, mdir dir, int32_t multihex, msize map_size, const function<HexBlockResult(mpos)>& check_hex) -> HexBlockResult
{
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
    FO_TRACE_ZONE(Map);

    FindPathOutput output;

    msize map_size = input.MapSize;

    if (!map_size.is_valid_pos(input.FromHex) || (!input.CheckTarget && !map_size.is_valid_pos(input.ToHex))) {
        output.Result = FindPathOutput::ResultType::InvalidHexes;
        return output;
    }

    if (IsPathGoal(input, input.FromHex)) {
        output.Result = FindPathOutput::ResultType::AlreadyHere;

        if (input.CheckTarget) {
            output.NewToHex = input.FromHex;
        }

        return output;
    }

    // A goal further than the limit in a straight line needs no search to be refused
    if (GetStepsToGoal(input, input.FromHex) > input.MaxLength) {
        output.Result = FindPathOutput::ResultType::TooFar;
        return output;
    }

    PathSearch search {input};
    output.Result = search.Run();

    if (output.Result != FindPathOutput::ResultType::Ok) {
        return output;
    }

    mpos to_hex = search.GetGoal();
    vector<mdir> raw_steps;

    if (!search.Backtrack(raw_steps)) {
        output.Result = FindPathOutput::ResultType::BacktraceError;
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

                    if (search.IsLineHexBlocked(next_hex, dir.value())) {
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
    FO_TRACE_ZONE(Map);

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
    // Work in map pixel space with the final hex center as the origin.
    // C = final hex center -> target hex center; the real target adds the target's own sub-hex offset
    ipos32 center_to_hex = GeometryHelper::GetHexOffset(new_to_hex, to_hex);
    float32_t target_x = numeric_cast<float32_t>(center_to_hex.x + to_hex_offset.x);
    float32_t target_y = numeric_cast<float32_t>(center_to_hex.y + to_hex_offset.y);

    // Distance uses the camera Y projection (same metric as MovingContext segment distances)
    float32_t y_proj = GeometryHelper::GetYProj();
    float32_t target_proj_y = target_y * y_proj;
    float32_t target_len = std::sqrt(target_x * target_x + target_proj_y * target_proj_y);

    constexpr float32_t min_len = 0.5f;

    if (target_len < min_len) {
        // Already on the target
        return std::nullopt;
    }

    // Gap to preserve = continuous distance between the final hex center and the target hex center (the "cut" gap)
    float32_t gap_x = numeric_cast<float32_t>(center_to_hex.x);
    float32_t gap_proj_y = numeric_cast<float32_t>(center_to_hex.y) * y_proj;
    float32_t gap_len = std::sqrt(gap_x * gap_x + gap_proj_y * gap_proj_y);

    // Stand at gap_len from the real target, on the final-hex side of it
    float32_t factor = 1.0f - gap_len / target_len;
    int32_t ox = iround<int32_t>(factor * target_x);
    int32_t oy = iround<int32_t>(factor * target_y);

    constexpr int32_t half_w = GameSettings::MAP_HEX_WIDTH / 2;
    constexpr int32_t half_h = GameSettings::MAP_HEX_HEIGHT / 2;

    int16_t clamped_ox = numeric_cast<int16_t>(std::clamp(ox, -half_w, half_w));
    int16_t clamped_oy = numeric_cast<int16_t>(std::clamp(oy, -half_h, half_h));
    return ipos16 {clamped_ox, clamped_oy};
}

PathSearch::PathSearch(const FindPathInput& input) :
    _input {&input},
    _grid {input.MapSize},
    _checkHexOnce {[this](mpos hex) -> HexBlockResult { return CheckHexOnce(hex); }},
    _lineStart {GetPlanePos(input.FromHex)}
{
    if (!input.CheckTarget) {
        ipos32 to_pos = GetPlanePos(input.ToHex);
        _line = ipos32 {to_pos.x - _lineStart.x, to_pos.y - _lineStart.y};
    }
}

auto PathSearch::Run() -> FindPathOutput::ResultType
{
    PathSearchCell& start = _grid.GetCell(_input->FromHex);
    start.Reached = true;
    _reached = 1;
    _open.Push(GetStepsToGoal(*_input, _input->FromHex), _input->FromHex);

    bool enclosure_probed = _input->CheckTarget || _input->EnclosureProbeLimit <= 0;
    int64_t critters_allowed = 0;
    bool goal_found = false;
    int64_t goal_cost = 0;
    int32_t goal_deviation = 0;

    while (!_open.IsEmpty()) {
        PathSearchNode node = _open.Pop();
        PathSearchCell& cell = _grid.GetCell(node.Hex);

        // A cheaper entry for the same hex always comes out first, so a hex already closed means a stale entry
        if (cell.Closed) {
            continue;
        }

        // Past the cheapest goal the search still settles every hex as cheap, so the route traced back chooses
        // among all the cheapest routes and not only among the ones reached first
        if (goal_found) {
            if (node.Estimate > goal_cost) {
                break;
            }
        }
        else if (cell.Cost / CRITTER_COST > critters_allowed) {
            // Walking through one more critter is the last resort, and it is not taken while the length limit has
            // cut off a route that might have gone around
            if (_cutByLength) {
                return FindPathOutput::ResultType::TooFar;
            }

            critters_allowed = cell.Cost / CRITTER_COST;
        }

        cell.Closed = true;
        int64_t cost = cell.Cost;
        int32_t steps = cell.Steps;

        if (IsPathGoal(*_input, node.Hex)) {
            // Of the cheapest goals, the one nearest the straight line to the target
            int32_t deviation = GetDeviation(node.Hex);

            if (!goal_found || deviation < goal_deviation) {
                _goal = node.Hex;
                goal_cost = cost;
                goal_deviation = deviation;
                goal_found = true;
            }

            continue;
        }

        Expand(node.Hex, cost, steps);

        // Only a search that has already outgrown the budget pays for flooding back from the target
        if (!goal_found && !enclosure_probed && _reached > numeric_cast<size_t>(_input->EnclosureProbeLimit)) {
            enclosure_probed = true;

            if (IsTargetWalledOff()) {
                return FindPathOutput::ResultType::NoWay;
            }
        }
    }

    if (goal_found) {
        return FindPathOutput::ResultType::Ok;
    }

    return _cutByLength ? FindPathOutput::ResultType::TooFar : FindPathOutput::ResultType::NoWay;
}

// Walks back from the goal over reached hexes whose cost leads exactly to the next one, preferring at every step
// the hex nearest the straight line back to the start, so of all the cheapest routes the straightest is taken
auto PathSearch::Backtrack(vector<mdir>& raw_steps) -> bool
{
    msize map_size = _input->MapSize;
    mpos cur_hex = _goal;
    int32_t cur_steps = _grid.GetCell(_goal).Steps;
    int64_t cur_cost = _grid.GetCell(_goal).Cost;
    float32_t base_angle = GeometryHelper::GetDirAngle(_goal, _input->FromHex);
    raw_steps.resize(numeric_cast<size_t>(cur_steps));

    while (cur_steps > 0) {
        mdir best_step_dir;
        mpos best_step_hex;
        int64_t best_step_cost = 0;
        bool step_ok = false;
        float32_t best_step_angle_diff = 0.0f;

        for (int32_t dir_value = 0; dir_value < GameSettings::MAP_DIR_COUNT; dir_value++) {
            mdir dir = hdir(dir_value);
            ipos32 step_raw_hex {cur_hex.x, cur_hex.y};
            GeometryHelper::MoveHexByDirUnsafe(step_raw_hex, dir.reverse());

            if (!map_size.is_valid_pos(step_raw_hex)) {
                continue;
            }

            mpos step_hex = map_size.from_raw_pos(step_raw_hex);
            nptr<const PathSearchCell> step_cell = _grid.FindCell(step_hex);

            if (!step_cell || !step_cell->Reached || step_cell->Steps != cur_steps - 1) {
                continue;
            }

            int64_t step_cost = step_cell->Cost;
            HexBlockResult block = CheckEntry(cur_hex, dir);

            if (block == HexBlockResult::Blocked || step_cost + GetEntryCost(block) != cur_cost) {
                continue;
            }

            float32_t angle = GeometryHelper::GetDirAngle(step_hex, _input->FromHex);
            float32_t angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);

            if (!step_ok || angle_diff < best_step_angle_diff) {
                best_step_dir = dir;
                best_step_hex = step_hex;
                best_step_cost = step_cost;
                best_step_angle_diff = angle_diff;
                step_ok = true;
            }
        }

        if (!step_ok) {
            return false;
        }

        raw_steps[numeric_cast<size_t>(cur_steps - 1)] = best_step_dir;
        cur_hex = best_step_hex;
        cur_cost = best_step_cost;
        cur_steps--;
    }

    return true;
}

// A straightened stretch may cross any hex the mover could enter from that side, reached by the search or not
auto PathSearch::IsLineHexBlocked(mpos hex, mdir dir) -> bool
{
    return CheckEntry(hex, dir) == HexBlockResult::Blocked;
}

auto PathSearch::GetDeviation(mpos hex) const -> int32_t
{
    ipos32 hex_pos = GetPlanePos(hex);
    int32_t dx = hex_pos.x - _lineStart.x;
    int32_t dy = hex_pos.y - _lineStart.y;

    return std::abs(dx * _line.y - dy * _line.x);
}

auto PathSearch::CheckEntry(mpos hex, mdir dir) -> HexBlockResult
{
    // A footprint takes different hexes depending on the side it enters from, so only single hexes are cached
    if (_input->Multihex != 0) {
        return PathFinding::CheckHexWithMultihex(hex, dir, _input->Multihex, _input->MapSize, _checkHexOnce);
    }

    return CheckHexOnce(hex);
}

auto PathSearch::CheckHexOnce(mpos hex) -> HexBlockResult
{
    PathSearchCell& cell = _grid.GetCell(hex);

    if (cell.Answer == 0) {
        cell.Answer = numeric_cast<uint8_t>(static_cast<int8_t>(_input->CheckHex(hex)) + 2);
    }

    return static_cast<HexBlockResult>(cell.Answer - 2);
}

void PathSearch::Expand(mpos hex, int64_t cost, int32_t steps)
{
    msize map_size = _input->MapSize;

    for (int32_t dir_value = 0; dir_value < GameSettings::MAP_DIR_COUNT; dir_value++) {
        mdir dir = hdir(dir_value);
        ipos32 raw_next_hex = ipos32 {hex.x, hex.y};
        GeometryHelper::MoveHexByDirUnsafe(raw_next_hex, dir);

        if (!map_size.is_valid_pos(raw_next_hex)) {
            continue;
        }

        mpos next_hex = map_size.from_raw_pos(raw_next_hex);
        int32_t next_index = _grid.GetCellIndex(next_hex);

        if (_grid.GetCell(next_index).Closed) {
            continue;
        }

        HexBlockResult block = CheckEntry(next_hex, dir);

        if (block == HexBlockResult::Blocked) {
            continue;
        }

        int64_t next_cost = cost + GetEntryCost(block);
        PathSearchCell& next_cell = _grid.GetCell(next_index);

        if (next_cell.Reached && next_cell.Cost <= next_cost) {
            continue;
        }

        int32_t remaining = GetStepsToGoal(*_input, next_hex);

        if (int64_t {steps} + 1 + remaining > _input->MaxLength) {
            _cutByLength = true;
            continue;
        }

        if (!next_cell.Reached) {
            next_cell.Reached = true;
            _reached++;
        }

        next_cell.Cost = next_cost;
        next_cell.Steps = steps + 1;
        _open.Push(next_cost + remaining, next_hex);
    }
}

// Floods back from the goal within the probe budget over every hex the search could ever enter, deferred gags
// and critters included and multihex footprints ignored, so a goal side that closes first cannot be reached
auto PathSearch::IsTargetWalledOff() -> bool
{
    msize map_size = _input->MapSize;
    size_t limit = numeric_cast<size_t>(_input->EnclosureProbeLimit);
    int32_t goal_hexes = GeometryHelper::HexesInRadius(std::max(_input->Cut, 0));

    if (numeric_cast<size_t>(goal_hexes) > limit) {
        return false;
    }

    vector<mpos> region;
    region.reserve(limit);

    for (int32_t i = 0; i < goal_hexes; i++) {
        mpos goal_hex = _input->ToHex;

        if (GeometryHelper::MoveHexAroundAway(goal_hex, i, map_size) && !VisitProbeHex(goal_hex, region)) {
            return false;
        }
    }

    for (size_t i = 0; i < region.size(); i++) {
        mpos cur_hex = region[i];

        for (int32_t dir_value = 0; dir_value < GameSettings::MAP_DIR_COUNT; dir_value++) {
            ipos32 raw_hex = ipos32 {cur_hex.x, cur_hex.y};
            GeometryHelper::MoveHexByDirUnsafe(raw_hex, hdir(dir_value));

            if (map_size.is_valid_pos(raw_hex) && !VisitProbeHex(map_size.from_raw_pos(raw_hex), region)) {
                return false;
            }
        }

        if (region.size() > limit) {
            return false;
        }
    }

    return true;
}

// Answers false when the hex was reached by the search, which proves the goal side is not closed off
auto PathSearch::VisitProbeHex(mpos hex, vector<mpos>& region) -> bool
{
    PathSearchCell& cell = _grid.GetCell(hex);

    if (cell.Reached) {
        return false;
    }
    if (cell.Probed) {
        return true;
    }

    cell.Probed = true;

    if (CheckHexOnce(hex) != HexBlockResult::Blocked) {
        region.emplace_back(hex);
    }

    return true;
}

void PathOpenList::Push(int64_t estimate, mpos hex)
{
    if (_order == 0) {
        _front = estimate;
    }

    FO_VERIFY_AND_THROW(estimate >= _front, "Path search estimate fell below the one taken last", estimate, _front);

    PathSearchNode node {.Estimate = estimate, .Order = _order++, .Hex = hex};

    if (estimate < _front + OPEN_BUCKETS) {
        _buckets[GetBucket(estimate)].emplace_back(node);
        _bucketed++;
    }
    else {
        _later.emplace_back(node);
        std::ranges::push_heap(_later, IsNodeAfter);
    }
}

auto PathOpenList::Pop() -> PathSearchNode
{
    FO_VERIFY_AND_THROW(!IsEmpty(), "Path search open list popped while empty");

    while (true) {
        auto& bucket = _buckets[GetBucket(_front)];

        if (_frontRead < bucket.size()) {
            _bucketed--;
            return bucket[_frontRead++];
        }

        bucket.clear();
        _frontRead = 0;

        // A costlier tier starts where its cheapest entry is, not one bucket at a time
        if (_bucketed == 0) {
            _front = _later.front().Estimate;
        }
        else {
            _front++;
        }

        AdmitLater();
    }
}

void PathOpenList::AdmitLater()
{
    while (!_later.empty() && _later.front().Estimate < _front + OPEN_BUCKETS) {
        std::ranges::pop_heap(_later, IsNodeAfter);
        _buckets[GetBucket(_later.back().Estimate)].emplace_back(_later.back());
        _later.pop_back();
        _bucketed++;
    }
}

PathSearchGrid::PathSearchGrid(msize map_size) :
    _blocksPerRow {(numeric_cast<int32_t>(map_size.width) + SEARCH_BLOCK_SIDE - 1) >> SEARCH_BLOCK_SHIFT}
{
    int32_t block_rows = (numeric_cast<int32_t>(map_size.height) + SEARCH_BLOCK_SIDE - 1) >> SEARCH_BLOCK_SHIFT;
    _blockIndex.assign(numeric_cast<size_t>(_blocksPerRow) * numeric_cast<size_t>(block_rows), -1);
}

auto PathSearchGrid::FindCell(mpos hex) const -> nptr<const PathSearchCell>
{
    int32_t block = _blockIndex[GetBlockSlot(hex)];

    if (block < 0) {
        return nullptr;
    }

    return &_cells[GetCellSlot(block, hex)];
}

auto PathSearchGrid::GetCellIndex(mpos hex) -> int32_t
{
    int32_t& block = _blockIndex[GetBlockSlot(hex)];

    if (block < 0) {
        block = numeric_cast<int32_t>(_cells.size()) / SEARCH_BLOCK_CELLS;
        _cells.resize(_cells.size() + numeric_cast<size_t>(SEARCH_BLOCK_CELLS));
    }

    return GetCellSlot(block, hex);
}

static auto IsPathGoal(const FindPathInput& input, mpos hex) -> bool
{
    if (input.CheckTarget) {
        return input.CheckTarget(hex);
    }

    return GeometryHelper::CheckDist(hex, input.ToHex, input.Cut);
}

// Steps still needed at the least, the A* estimate; a multi-target search has no single goal to measure to
static auto GetStepsToGoal(const FindPathInput& input, mpos hex) -> int32_t
{
    if (input.CheckTarget) {
        return 0;
    }

    return std::max(GeometryHelper::GetDistance(hex, input.ToHex) - input.Cut, 0);
}

static auto GetEntryCost(HexBlockResult block) -> int64_t
{
    switch (block) {
    case HexBlockResult::DeferGag:
        return 1 + GAG_DETOUR_STEPS;
    case HexBlockResult::DeferCritter:
        return 1 + CRITTER_COST;
    default:
        return 1;
    }
}

// Hex centres on a plane where distances keep their proportions, as GetDirAngle measures them: odd columns sit
// half a row lower, so rows count in halves
static auto GetPlanePos(mpos hex) -> ipos32
{
    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        return ipos32 {hex.x, hex.y * 2 - hex.x % 2};
    }
    else {
        return ipos32 {hex.x, hex.y};
    }
}

static auto GetBucket(int64_t estimate) -> size_t
{
    return numeric_cast<size_t>(estimate & (OPEN_BUCKETS - 1));
}

static auto IsNodeAfter(const PathSearchNode& a, const PathSearchNode& b) -> bool
{
    if (a.Estimate != b.Estimate) {
        return a.Estimate > b.Estimate;
    }

    return a.Order > b.Order;
}

FO_END_NAMESPACE
