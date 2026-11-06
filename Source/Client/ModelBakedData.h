// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

inline constexpr uint32_t MODEL_ANIMATION_MAX_JOINTS {1024};

void VerifyModelBakedCountFitsData(const data_reader& reader, size_t count, size_t min_element_size, string_view field, string_view context);

FO_END_NAMESPACE

#endif
