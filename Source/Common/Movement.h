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

#pragma once

#include "Common.h"

#include "Geometry.h"

FO_BEGIN_NAMESPACE

///@ ExportEnum
enum class MovingState : uint8_t
{
    InProgress = 0,
    Success = 1,
    TargetNotFound = 2,
    CantMove = 3,
    GagCritter = 4,
    GagItem = 5,
    GenericError = 6,
    HexTooFar = 7,
    HexBusy = 8,
    Deadlock = 10,
    TraceFailed = 11,
    NotAlive = 12,
    Attached = 13,
    Stopped = 14,
};

struct MovingMetrics
{
    mpos EndHex {};
    float32_t WholeTime {};
    float32_t WholeDist {};
};

struct MovingProgress
{
    mpos Hex {};
    ipos16 HexOffset {};
    mdir Dir {};
    bool Completed {};
};

struct MovingRawProgress
{
    mpos Hex {};
    mpos SegmentStartHex {};
    ipos32 SegmentOffset {};
    float32_t NormalizedDist {};
    bool Completed {};
    bool Found {};
};

///@ ExportRefType Common RefCounted Export = GetSpeed, GetStartHex, GetEndHex, GetPreBlockHex, GetBlockHex, GetWholeTime, GetWholeDist, GetElapsedTime, IsCompleted, GetCompleteReason, EvaluateProjectedHex, EvaluateNearestPathHex, EvaluatePathHexes
class MovingContext final : public RefCounted<MovingContext>
{
public:
    explicit MovingContext(msize map_size, uint16_t speed, vector<mdir> steps, vector<uint16_t> control_steps, nanotime start_time, timespan offset_time, mpos start_hex, ipos16 start_hex_offset, ipos16 end_hex_offset);
    explicit MovingContext(msize map_size, uint16_t speed, vector<mdir> steps, vector<uint16_t> control_steps, nanotime start_time, timespan offset_time, mpos start_hex, ipos16 start_hex_offset, ipos16 end_hex_offset, float32_t whole_time);
    MovingContext(const MovingContext&) = delete;
    MovingContext(MovingContext&&) noexcept = delete;
    auto operator=(const MovingContext&) -> MovingContext& = delete;
    auto operator=(MovingContext&&) noexcept -> MovingContext& = delete;
    ~MovingContext() = default;

    [[nodiscard]] auto GetMapSize() const noexcept -> msize { return _mapSize; }
    [[nodiscard]] auto GetSpeed() const noexcept -> uint16_t { return _speed; }
    [[nodiscard]] auto GetSteps() const noexcept -> const vector<mdir>& { return _steps; }
    [[nodiscard]] auto GetControlSteps() const noexcept -> const vector<uint16_t>& { return _controlSteps; }
    [[nodiscard]] auto GetStartHex() const noexcept -> mpos { return _startHex; }
    [[nodiscard]] auto GetEndHex() const noexcept -> mpos { return _endHex; }
    [[nodiscard]] auto GetPreBlockHex() const noexcept -> mpos { return _preBlockHex; }
    [[nodiscard]] auto GetBlockHex() const noexcept -> mpos { return _blockHex; }
    [[nodiscard]] auto GetWholeTime() const noexcept -> float32_t { return _wholeTime; }
    [[nodiscard]] auto GetWholeDist() const noexcept -> float32_t { return _wholeDist; }
    [[nodiscard]] auto GetStartHexOffset() const noexcept -> ipos16 { return _startHexOffset; }
    [[nodiscard]] auto GetEndHexOffset() const noexcept -> ipos16 { return _endHexOffset; }
    [[nodiscard]] auto GetElapsedTime() const noexcept -> float32_t { return _elapsedTime; }
    [[nodiscard]] auto GetRuntimeElapsedTime(nanotime current_time) const noexcept -> float32_t;
    [[nodiscard]] auto IsCompleted() const noexcept -> bool { return _completed; }
    [[nodiscard]] auto GetCompleteReason() const noexcept -> MovingState { return _completeReason; }

    [[nodiscard]] auto EvaluateMetrics() const -> MovingMetrics;
    [[nodiscard]] auto EvaluateProjectedHex(float32_t look_ahead_ms) const -> mpos;
    [[nodiscard]] auto EvaluateNearestPathHex(mpos current_hex, mpos from_hex, mpos fallback_hex) const -> mpos;
    [[nodiscard]] auto EvaluatePathHexes(mpos current_hex) const -> vector<mpos>;
    [[nodiscard]] auto EvaluateProgress() const -> MovingProgress;
    [[nodiscard]] auto EvaluateProgress(mpos current_hex) const -> MovingProgress;

    void UpdateCurrentTime(nanotime current_time);
    void UpdateCurrentTimeToNextHex(nanotime current_time, mpos current_hex);
    void ChangeSpeed(uint16_t speed, nanotime current_time);
    void Complete(MovingState reason) noexcept;
    void SetBlockHexes(mpos pre_block_hex, mpos block_hex) noexcept;
    void ValidateRuntimeState() const;

private:
    auto EvaluateRawProgress(float32_t elapsed_time_ms) const -> MovingRawProgress;
    auto BuildProgress(const MovingRawProgress& raw_progress, mpos current_hex) const -> MovingProgress;
    void EvaluateSegment(uint16_t control_step_begin, uint16_t control_step_end, mpos segment_start_hex, bool is_last, mpos& segment_end_hex, ipos32& offset, float32_t& dist) const;
    void RecalculateMetrics();

    msize _mapSize {};
    uint16_t _speed {};
    vector<mdir> _steps {};
    vector<uint16_t> _controlSteps {};
    nanotime _startTime {};
    timespan _offsetTime {};
    mpos _startHex {};
    mpos _endHex {};
    float32_t _wholeTime {};
    float32_t _wholeDist {};
    float32_t _elapsedTime {};
    bool _completed {};
    MovingState _completeReason {MovingState::InProgress};
    ipos16 _startHexOffset {};
    ipos16 _endHexOffset {};
    mpos _preBlockHex {};
    mpos _blockHex {};
};

FO_END_NAMESPACE
