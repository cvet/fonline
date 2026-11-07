// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ServerImage.h"

#include "ScriptSystem.h"
#include "SpriteResource.h"

FO_BEGIN_NAMESPACE

ServerImage::ServerImage(isize32 size, vector<ucolor> pixels) :
    _size {size},
    _pixels {std::move(pixels)}
{
    FO_VERIFY_AND_THROW(_size.width > 0 && _size.height > 0, "Server image size must be positive", _size.width, _size.height);
    FO_VERIFY_AND_THROW(numeric_cast<int64_t>(_pixels.size()) == numeric_cast<int64_t>(_size.width) * _size.height, "Server image pixel count does not match its size", _pixels.size(), _size.width, _size.height);
}

auto ServerImage::GetColor(ipos32 pos) const -> ucolor
{
    if (pos.x < 0 || pos.y < 0 || pos.x >= _size.width || pos.y >= _size.height) {
        throw ScriptException("Image position is outside the image", pos.x, pos.y, _size.width, _size.height);
    }

    size_t index {numeric_cast<size_t>(pos.y) * numeric_cast<size_t>(_size.width) + numeric_cast<size_t>(pos.x)};

    return _pixels[index];
}

auto LoadServerImage(const FileSystem& resources, string_view image_name) -> refcount_ptr<ServerImage>
{
    FO_TRACE_ZONE(FileSystem);

    if (image_name.empty()) {
        throw ScriptException("Image name is empty");
    }

    auto file {resources.ReadFile(image_name)};

    if (!file) {
        throw ScriptException("Image file not found", image_name);
    }

    SpriteResourceData resource {ReadSpriteResource(file.GetDataSpan())};

    if (resource.Directions.size() != 1) {
        throw ScriptException("Image must contain exactly one direction", image_name, resource.Directions.size());
    }

    if (resource.Directions.front().Frames.size() != 1) {
        throw ScriptException("Image must contain exactly one frame", image_name, resource.Directions.front().Frames.size());
    }

    SpriteResourceFrameData& frame {resource.Directions.front().Frames.front()};

    if (frame.SharedFrameIndex.has_value()) {
        throw ScriptException("Image frame must not reference another frame", image_name);
    }

    SpriteResourceImageData image {ExtractSpriteResourceFrameImage(std::move(frame))};

    if (image.Size.width <= 0 || image.Size.height <= 0) {
        throw ScriptException("Image frame is empty", image_name);
    }

    return safe_alloc::make_refcounted<ServerImage>(image.Size, std::move(image.Pixels));
}

FO_END_NAMESPACE
