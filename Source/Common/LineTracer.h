// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Geometry.h"

FO_BEGIN_NAMESPACE

class LineTracer final
{
public:
    explicit LineTracer(mpos start_hex, mpos target_hex, float32_t dir_angle_offset, msize map_size, ipos16 start_offset = {}, ipos16 target_offset = {});
    explicit LineTracer(mpos start_hex, float32_t dir_angle, int32_t dist, msize map_size, ipos16 start_offset = {}, ipos16 target_offset = {});
    LineTracer(const LineTracer&) = delete;
    LineTracer(LineTracer&&) noexcept = default;
    auto operator=(const LineTracer&) -> LineTracer& = delete;
    auto operator=(LineTracer&&) noexcept -> LineTracer& = delete;
    ~LineTracer() = default;

    auto GetNextHex(mpos& hex) -> optional<mdir>;

private:
    void TraceInit(mpos start_hex, mpos target_hex, float32_t dir_angle_offset, ipos16 start_offset, ipos16 target_offset);
    auto GetLineDistance(mpos hex) const -> float64_t;

    msize _mapSize {};
    mpos _origin {};
    hdir _dirLeft {};
    hdir _dirRight {};
    float64_t _xStart {};
    float64_t _yStart {};
    float64_t _dx {};
    float64_t _dy {};
    int64_t _step {};
};

FO_END_NAMESPACE
