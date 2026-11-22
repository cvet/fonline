// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

// Minimal PNG encoders for the images the engine produces itself: screenshots, render-target and atlas dumps.
// Pixels come in ucolor's R, G, B, A order, which is PNG's own, rows top to bottom
namespace ImageWriter
{
    void WritePng(string_view fname, isize32 size, const_span<ucolor> data);
    // Drops alpha and filters every row, for an image that is sent somewhere rather than dumped to disk
    auto EncodeCompactPng(isize32 size, const_span<ucolor> data) -> vector<uint8_t>;
}

FO_END_NAMESPACE
