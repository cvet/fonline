//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include "Common.h"
#include "ConfigFile.h"
#include "ImageImport.h"

FO_BEGIN_NAMESPACE

enum class ImageResample : uint8_t
{
    Nearest,
    Bilinear,
    Area
};

enum class ImagePixelUsage : uint8_t
{
    Color,
    Data,
    Normal
};

enum class ImageColorSpace : uint8_t
{
    Encoded,
    Linear
};

struct ImageTransform
{
    optional<int32_t> InputDir {};
    optional<array<int32_t, 4>> Crop {};
    optional<isize32> Grid {};
    int32_t Flip {};
    int32_t Rotate {};
    optional<isize32> Size {};
    optional<isize32> Canvas {};
    ipos32 CanvasOffset {};
    optional<ipos32> Move {};
    ImageResample Resample {ImageResample::Bilinear};
    ImagePixelUsage PixelUsage {ImagePixelUsage::Color};
    ImageColorSpace ColorSpace {ImageColorSpace::Encoded};
};

auto ReadImageIntegers(string_view value, string_view fname, string_view section, string_view key) -> vector<int32_t>;
auto ReadImageTransforms(const ConfigFile& recipe, string_view section, size_t reference_count, string_view fname, bool source) -> vector<ImageTransform>;
auto SelectImageSequence(const ImageImport::FrameCollection& source, optional<int32_t> direction, string_view fname) -> const ImageImport::FrameSequence&;
auto TransformImageFrame(const ImageImport::FrameShot& source, const ImageTransform& transform, string_view fname) -> vector<ImageImport::FrameShot>;
auto SelectImageFrames(vector<ImageImport::FrameShot> frames, const ConfigFile& recipe, string_view section, string_view fname, bool inherit) -> vector<ImageImport::FrameShot>;

FO_END_NAMESPACE
