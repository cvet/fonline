// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ImageImport.h"

#if FO_IMAGE_ZAR || FO_IMAGE_TIL

FO_BEGIN_NAMESPACE

namespace ImageBakerZarDecoder
{
    auto Decode(string_view fname, FileReader& reader, bool swap_red_blue) -> ImageImport::FrameShot;
}

FO_END_NAMESPACE

#endif
