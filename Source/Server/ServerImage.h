// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "FileSystem.h"

FO_BEGIN_NAMESPACE

// Decoded pixels of one baked single-frame sprite resource for server-side lookups such as world-map masks
///@ ExportRefType Server RefCounted Export = GetSize, GetColor
class ServerImage final : public refcounted<ServerImage>
{
public:
    explicit ServerImage(isize32 size, vector<ucolor> pixels);
    ServerImage(const ServerImage&) = delete;
    ServerImage(ServerImage&&) noexcept = delete;
    auto operator=(const ServerImage&) = delete;
    auto operator=(ServerImage&&) noexcept = delete;
    ~ServerImage() = default;

    // Returns the image width and height in pixels
    [[nodiscard]] auto GetSize() const noexcept -> isize32 { return _size; }
    // Returns the color of one pixel and throws for a position outside the image
    [[nodiscard]] auto GetColor(ipos32 pos) const -> ucolor;

private:
    isize32 _size;
    vector<ucolor> _pixels;
};

auto LoadServerImage(const FileSystem& resources, string_view image_name) -> refcount_ptr<ServerImage>;

FO_END_NAMESPACE
