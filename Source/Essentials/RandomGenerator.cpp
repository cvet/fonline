// FOnline Engine
// SPDX-License-Identifier: MIT

#include "RandomGenerator.h"

FO_BEGIN_NAMESPACE

random_generator::random_generator()
{
    // std::random_device hands out 32 bits at a time, and the state wants a full word before mixing
    std::random_device entropy {};
    seed((static_cast<uint64_t>(entropy()) << 32) | entropy());
}

FO_END_NAMESPACE
