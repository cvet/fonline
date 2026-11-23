// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ImageImport.h"

#if FO_IMAGE_FRM

FO_BEGIN_NAMESPACE

namespace ImageBakerFrm
{
    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> ImageImport::FrameCollection;
    auto LoadFrX(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> ImageImport::FrameCollection;
    auto ResolveOutput(string_view fname) -> string;
    auto DiscoverDependencies(string_view fname, const FileCollection& files) -> vector<ImageImport::DependencyReference>;
    auto DiscoverFrXDependencies(string_view fname, const FileCollection& files) -> vector<ImageImport::DependencyReference>;
}

FO_END_NAMESPACE

#endif
