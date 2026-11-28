// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "ImageImport.h"

FO_BEGIN_NAMESPACE

namespace ImageBakerFofrm
{
    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> ImageImport::FrameCollection;

    // Root Effect path declared by one descriptor, empty when it declares none
    auto ReadEffect(string_view fname, string_view text) -> string;

    // Messages for every authored descriptor whose Effect is missing from the baked output or cannot draw a sprite
    auto CheckEffects(const FileCollection& sources, const FileSystem& baked) -> vector<string>;
}

FO_END_NAMESPACE
