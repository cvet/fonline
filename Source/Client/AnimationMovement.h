// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

namespace AnimationMovement
{
    auto AccumulateFrameOffsets(const_span<ipos32> offsets, int32_t frame) -> ipos32;
    auto TransferWalkPhase(const_span<ipos32> previous_offsets, ipos32 pos, ipos32 anchor, ipos32 total) -> ipos32;
    void EvaluateWalkFrame(const_span<ipos32> offsets, ipos32 total, ipos32 pos, ipos32 anchor, int32_t& frame_index, ipos32& offset);
}

FO_END_NAMESPACE
