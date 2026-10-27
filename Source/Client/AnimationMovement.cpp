//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
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

#include "AnimationMovement.h"

FO_BEGIN_NAMESPACE

namespace AnimationMovement
{
    static auto ProjectAnimationOffset(ipos32 offset, ipos32 axis) -> int64_t;
    static auto WrapWalkPhase(int64_t projection, int64_t length) -> int64_t;
    static auto ScaleWalkPhase(int64_t phase, int64_t length, int32_t component) -> int32_t;

    auto AccumulateFrameOffsets(const_span<ipos32> offsets, int32_t frame) -> ipos32
    {
        int64_t x = 0;
        int64_t y = 0;

        for (int32_t i = 0; i <= frame; i++) {
            x += offsets[i].x;
            y += offsets[i].y;
        }

        return {numeric_cast<int32_t>(x), numeric_cast<int32_t>(y)};
    }

    auto TransferWalkPhase(const_span<ipos32> previous_offsets, ipos32 pos, ipos32 anchor, ipos32 total) -> ipos32
    {
        ipos32 old_total = AccumulateFrameOffsets(previous_offsets, numeric_cast<int32_t>(previous_offsets.size()) - 1);
        int64_t old_length = ProjectAnimationOffset(old_total, old_total);

        if (old_length == 0) {
            return {};
        }

        int64_t old_phase = WrapWalkPhase(ProjectAnimationOffset(pos - anchor, old_total), old_length);
        return {ScaleWalkPhase(old_phase, old_length, total.x), ScaleWalkPhase(old_phase, old_length, total.y)};
    }

    void EvaluateWalkFrame(const_span<ipos32> offsets, ipos32 total, ipos32 pos, ipos32 anchor, int32_t& frame_index, ipos32& offset)
    {
        frame_index = 0;
        offset = {};

        int64_t length = ProjectAnimationOffset(total, total);

        if (length != 0) {
            int64_t projection = ProjectAnimationOffset(pos - anchor, total);
            int64_t phase = WrapWalkPhase(projection, length);
            uint64_t best_diff = std::numeric_limits<uint64_t>::max();
            int64_t x = 0;
            int64_t y = 0;
            ipos32 frame_offset {};
            int32_t frames_count = numeric_cast<int32_t>(offsets.size());

            for (int32_t i = 0; i < frames_count; i++) {
                x += offsets[i].x;
                y += offsets[i].y;
                ipos32 accum {numeric_cast<int32_t>(x), numeric_cast<int32_t>(y)};
                int64_t frame_projection = ProjectAnimationOffset(accum, total);
                uint64_t diff = frame_projection >= phase ? numeric_cast<uint64_t>(frame_projection - phase) : numeric_cast<uint64_t>(phase) - static_cast<uint64_t>(frame_projection);

                if (diff < best_diff) {
                    best_diff = diff;
                    frame_index = i;
                    frame_offset = accum;
                }
            }

            int64_t cycle = projection / length;

            if (projection % length < 0) {
                cycle--;
            }

            ipos32 cycle_offset {numeric_cast<int32_t>(cycle * total.x), numeric_cast<int32_t>(cycle * total.y)};
            // Cancel smooth interpolation within a pose; this changes presentation only
            offset = anchor + cycle_offset + frame_offset - pos;
        }
    }

    static auto ProjectAnimationOffset(ipos32 offset, ipos32 axis) -> int64_t
    {
        int64_t x = numeric_cast<int64_t>(offset.x) * axis.x;
        int64_t y = numeric_cast<int64_t>(offset.y) * axis.y;
        FO_VERIFY_AND_THROW(y <= 0 || x <= std::numeric_limits<int64_t>::max() - y, "Animation offset projection overflow");
        FO_VERIFY_AND_THROW(y >= 0 || x >= std::numeric_limits<int64_t>::min() - y, "Animation offset projection overflow");
        return x + y;
    }

    static auto WrapWalkPhase(int64_t projection, int64_t length) -> int64_t
    {
        int64_t phase = projection % length;
        return phase < 0 ? phase + length : phase;
    }

    // Multiply a proper nonnegative fraction by a signed component without overflowing its numerator
    static auto ScaleWalkPhase(int64_t phase, int64_t length, int32_t component) -> int32_t
    {
        uint64_t numerator = numeric_cast<uint64_t>(phase);
        uint64_t denominator = numeric_cast<uint64_t>(length);
        uint64_t magnitude = numeric_cast<uint64_t>(component < 0 ? -numeric_cast<int64_t>(component) : component);
        uint64_t quotient = 0;
        uint64_t remainder = 0;

        for (int32_t bit = 31; bit >= 0; bit--) {
            quotient *= 2;
            remainder *= 2;

            if (remainder >= denominator) {
                remainder -= denominator;
                quotient++;
            }

            if ((magnitude & (uint64_t {1} << bit)) != 0) {
                remainder += numerator;

                if (remainder >= denominator) {
                    remainder -= denominator;
                    quotient++;
                }
            }
        }

        if (remainder >= (denominator + 1) / 2) {
            quotient++;
        }

        int64_t result = numeric_cast<int64_t>(quotient);
        return numeric_cast<int32_t>(component < 0 ? -result : result);
    }
}

FO_END_NAMESPACE
