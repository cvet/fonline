// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define FO_RESOURCE_HASH_EXPORT __declspec(dllexport)
#else
#define FO_RESOURCE_HASH_EXPORT __attribute__((visibility("default")))
#endif

extern "C" FO_RESOURCE_HASH_EXPORT auto FO_Fnv1a64(const uint8_t* data, size_t size, uint64_t seed) noexcept -> uint64_t;
