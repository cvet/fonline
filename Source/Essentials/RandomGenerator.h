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

#pragma once

#include "BasicCore.h"
#include "ExceptionHandling.h"

FO_BEGIN_NAMESPACE

// xoshiro256++ over the standard Mersenne engine: 32 bytes of state instead of 5000, and no seeding pass.
// The bounded draws are ours because std::uniform_int_distribution maps a sequence differently per platform
class random_generator
{
public:
    // Draws its seed from the OS, so two default-constructed generators diverge
    random_generator();

    explicit random_generator(uint64_t seed_value) noexcept { seed(seed_value); }

    // A zero seed would leave the state at its fixed point, so the seed is expanded through SplitMix64
    void seed(uint64_t seed_value) noexcept
    {
        for (uint64_t& part : _state) {
            part = mix_seed(seed_value);
        }
    }

    [[nodiscard]] auto next() noexcept -> uint64_t
    {
        uint64_t result = rotate_left(_state[0] + _state[3], 23) + _state[0];
        uint64_t shifted = _state[1] << 17;

        _state[2] ^= _state[0];
        _state[3] ^= _state[1];
        _state[1] ^= _state[2];
        _state[0] ^= _state[3];
        _state[2] ^= shifted;
        _state[3] = rotate_left(_state[3], 45);

        return result;
    }

    [[nodiscard]] auto operator()() noexcept -> uint64_t { return next(); }

    // Uniform over [0, bound), by Lemire's multiply-shift: one 64-bit product, and the rejection branch is
    // entered about once every 2^32 / bound draws
    [[nodiscard]] auto next_below(uint32_t bound) -> uint32_t
    {
        FO_VERIFY_AND_THROW(bound != 0, "Random bound must be positive");

        uint64_t product = static_cast<uint64_t>(static_cast<uint32_t>(next())) * bound;
        uint32_t low = static_cast<uint32_t>(product);

        if (low < bound) {
            uint32_t threshold = (~bound + 1) % bound;

            while (low < threshold) {
                product = static_cast<uint64_t>(static_cast<uint32_t>(next())) * bound;
                low = static_cast<uint32_t>(product);
            }
        }

        return static_cast<uint32_t>(product >> 32);
    }

    // Uniform over [min_value, max_value], both ends included
    [[nodiscard]] auto next_between(int32_t min_value, int32_t max_value) -> int32_t
    {
        FO_VERIFY_AND_THROW(min_value <= max_value, "Random range must not be inverted", min_value, max_value);

        uint64_t span = static_cast<uint64_t>(static_cast<int64_t>(max_value) - static_cast<int64_t>(min_value)) + 1;

        if (span > std::numeric_limits<uint32_t>::max()) {
            return static_cast<int32_t>(static_cast<uint32_t>(next()));
        }

        return static_cast<int32_t>(static_cast<int64_t>(min_value) + static_cast<int64_t>(next_below(static_cast<uint32_t>(span))));
    }

    // Uniform over [0, 1), with the 53 bits a double can hold exactly
    [[nodiscard]] auto next_normalized() noexcept -> float64_t { return static_cast<float64_t>(next() >> 11) * 0x1.0p-53; }

private:
    [[nodiscard]] static auto rotate_left(uint64_t value, int32_t bits) noexcept -> uint64_t { return (value << bits) | (value >> (64 - bits)); }

    [[nodiscard]] static auto mix_seed(uint64_t& state) noexcept -> uint64_t
    {
        state += 0x9E3779B97F4A7C15ULL;

        uint64_t mixed = state;
        mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9ULL;
        mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EBULL;

        return mixed ^ (mixed >> 31);
    }

    std::array<uint64_t, 4> _state {};
};

FO_END_NAMESPACE
