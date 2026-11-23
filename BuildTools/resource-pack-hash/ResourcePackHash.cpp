// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ResourcePackHash.h"

auto FO_Fnv1a64(const uint8_t* data, size_t size, uint64_t seed) noexcept -> uint64_t
{
    for (size_t i {0}; i < size; ++i) {
        seed = (seed ^ data[i]) * UINT64_C(0x100000001b3);
    }

    return seed;
}
