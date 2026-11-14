// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ImageImport.h"

#if FO_IMAGE_RIX

FO_BEGIN_NAMESPACE

namespace ImageBakerRix
{
    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> ImageImport::FrameCollection;
}

FO_END_NAMESPACE

#endif
