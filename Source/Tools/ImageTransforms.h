// FOnline Engine
// SPDX-License-Identifier: MIT

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
    optional<ipos32> Motion {};
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
