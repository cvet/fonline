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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "SpriteResource.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

static auto ReadSpriteFrameMesh(FileReader& reader, isize32 size) -> optional<SpriteMeshData>;

auto ReadSpriteResource(const_span<uint8_t> data) -> SpriteResourceData
{
    FO_STACK_TRACE_ENTRY();

    FileReader reader {data};

    uint8_t header_magic = reader.GetUInt8();
    FO_VERIFY_AND_THROW(header_magic == SPRITE_RESOURCE_MAGIC, "Sprite resource header magic is invalid", header_magic, SPRITE_RESOURCE_MAGIC);

    uint8_t version = reader.GetUInt8();
    FO_VERIFY_AND_THROW(version == SPRITE_RESOURCE_VERSION, "Sprite resource version is unsupported", version, SPRITE_RESOURCE_VERSION);

    SpriteResourceData resource;
    resource.Animation.Sprite.emplace();
    SpriteInfo& sprite_info = *resource.Animation.Sprite;
    sprite_info.FrameCount = reader.GetLEUInt16();
    FO_VERIFY_AND_THROW(sprite_info.FrameCount != 0, "Sprite resource contains no frames");
    sprite_info.Duration = std::chrono::milliseconds {reader.GetLEUInt16()};

    uint8_t direction_count = reader.GetUInt8();
    FO_VERIFY_AND_THROW(direction_count != 0, "Sprite resource contains no directions");
    resource.Directions.resize(direction_count);
    sprite_info.Directions.resize(direction_count);

    auto read_sprite_pixels = [&reader](isize32 size) -> vector<ucolor> {
        FO_VERIFY_AND_THROW(size.width > 0, "Sprite frame width must be positive", size.width);
        FO_VERIFY_AND_THROW(size.height > 0, "Sprite frame height must be positive", size.height);

        uint64_t pixel_count64 = numeric_cast<uint64_t>(size.width) * numeric_cast<uint64_t>(size.height);
        uint64_t pixel_data_size64 = pixel_count64 * sizeof(ucolor);
        size_t remaining_size = reader.GetSize() - reader.GetCurPos();
        FO_VERIFY_AND_THROW(pixel_data_size64 <= remaining_size, "Sprite frame pixel data is truncated", pixel_data_size64, remaining_size, size);

        vector<ucolor> pixels(numeric_cast<size_t>(pixel_count64));
        reader.ReadObjectArray(span<ucolor> {pixels});
        return pixels;
    };

    for (uint8_t direction_index = 0; direction_index < direction_count; direction_index++) {
        SpriteResourceDirectionData& direction = resource.Directions[direction_index];
        SpriteDirInfo& direction_info = sprite_info.Directions[direction_index];
        direction.Frames.resize(sprite_info.FrameCount);
        direction_info.Frames.resize(sprite_info.FrameCount);

        for (uint16_t frame_index = 0; frame_index < sprite_info.FrameCount; frame_index++) {
            SpriteResourceFrameData& frame = direction.Frames[frame_index];
            SpriteFrameInfo& frame_info = direction_info.Frames[frame_index];
            uint8_t is_shared = reader.GetUInt8();
            FO_VERIFY_AND_THROW(is_shared <= 1, "Sprite frame reference flag is invalid", is_shared, direction_index, frame_index);

            if (is_shared != 0) {
                uint16_t shared_frame_index = reader.GetLEUInt16();
                FO_VERIFY_AND_THROW(shared_frame_index < frame_index, "Sprite frame reference points outside previously decoded frames", shared_frame_index, direction_index, frame_index);
                frame.SharedFrameIndex = shared_frame_index;
                frame_info = direction_info.Frames[shared_frame_index];
                frame_info.SharedFrameIndex = shared_frame_index;
                continue;
            }

            frame.Offset.x = reader.GetLEInt16();
            frame.Offset.y = reader.GetLEInt16();
            frame.Size.width = reader.GetLEUInt16();
            frame.Size.height = reader.GetLEUInt16();
            frame.NextOffset.x = reader.GetLEInt16();
            frame.NextOffset.y = reader.GetLEInt16();
            frame_info.Offset = frame.Offset;
            frame_info.Size = frame.Size;
            frame_info.NextOffset = frame.NextOffset;
            frame.Pixels = read_sprite_pixels(frame.Size);
            frame.Mesh = ReadSpriteFrameMesh(reader, frame.Size);
        }
    }

    uint8_t footer_magic = reader.GetUInt8();
    FO_VERIFY_AND_THROW(footer_magic == SPRITE_RESOURCE_MAGIC, "Sprite resource footer magic is invalid", footer_magic, SPRITE_RESOURCE_MAGIC);
    FO_VERIFY_AND_THROW(reader.GetCurPos() == reader.GetSize(), "Sprite resource contains trailing data", reader.GetCurPos(), reader.GetSize());

    return resource;
}

auto ExtractSpriteResourceFrameImage(SpriteResourceFrameData frame) -> SpriteResourceImageData
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!frame.SharedFrameIndex.has_value(), "Cannot extract image pixels from a shared sprite frame", frame.SharedFrameIndex.value_or(0));

    if (!frame.Mesh.has_value() || frame.Mesh->Indices.empty()) {
        return SpriteResourceImageData {.Size = frame.Size, .Pixels = std::move(frame.Pixels)};
    }

    const SpriteMeshData& mesh = *frame.Mesh;
    SpriteResourceImageData image;
    image.Size = mesh.SourceSize;
    uint64_t logical_pixel_count = numeric_cast<uint64_t>(image.Size.width) * image.Size.height;
    image.Pixels.resize(numeric_cast<size_t>(logical_pixel_count));

    int32_t source_begin_x = std::max(mesh.SourceOffset.x, 0);
    int32_t source_begin_y = std::max(mesh.SourceOffset.y, 0);
    int32_t frame_begin_x = std::max(-mesh.SourceOffset.x, 0);
    int32_t frame_begin_y = std::max(-mesh.SourceOffset.y, 0);
    int32_t copy_width = std::min(frame.Size.width - frame_begin_x, image.Size.width - source_begin_x);
    int32_t copy_height = std::min(frame.Size.height - frame_begin_y, image.Size.height - source_begin_y);
    FO_VERIFY_AND_THROW(copy_width > 0 && copy_height > 0, "Cropped sprite frame does not contain logical source pixels", frame.Size, mesh.SourceOffset, mesh.SourceSize);

    for (int32_t y = 0; y < copy_height; y++) {
        size_t frame_offset = numeric_cast<size_t>(frame_begin_y + y) * frame.Size.width + frame_begin_x;
        size_t image_offset = numeric_cast<size_t>(source_begin_y + y) * image.Size.width + source_begin_x;
        MemCopy(image.Pixels.data() + image_offset, frame.Pixels.data() + frame_offset, numeric_cast<size_t>(copy_width) * sizeof(ucolor));
    }

    return image;
}

static auto ReadSpriteFrameMesh(FileReader& reader, isize32 size) -> optional<SpriteMeshData>
{
    FO_STACK_TRACE_ENTRY();

    uint8_t raw_kind = reader.GetUInt8();

    if (raw_kind == static_cast<uint8_t>(SpriteMeshKind::Quad)) {
        return std::nullopt;
    }
    if (raw_kind == static_cast<uint8_t>(SpriteMeshKind::Empty)) {
        return SpriteMeshData {};
    }

    FO_VERIFY_AND_THROW(raw_kind == static_cast<uint8_t>(SpriteMeshKind::Mesh), "Sprite mesh kind is invalid", raw_kind);

    uint16_t vertex_count = reader.GetLEUInt16();
    uint32_t index_count = reader.GetLEUInt32();
    FO_VERIFY_AND_THROW(vertex_count >= 3, "Sprite mesh must contain at least three vertices", vertex_count);
    FO_VERIFY_AND_THROW(index_count >= 3 && index_count % 3 == 0, "Sprite mesh index count must contain complete triangles", index_count);
    FO_VERIFY_AND_THROW(index_count <= numeric_cast<uint32_t>(vertex_count) * 6u, "Sprite mesh contains implausibly many indices", vertex_count, index_count);

    uint64_t payload_size = sizeof(uint16_t) * 2 + sizeof(int32_t) * 2 + numeric_cast<uint64_t>(vertex_count) * sizeof(uint16_t) * 2 + numeric_cast<uint64_t>(index_count) * sizeof(uint16_t);
    size_t remaining_size = reader.GetSize() - reader.GetCurPos();
    FO_VERIFY_AND_THROW(payload_size <= remaining_size, "Sprite mesh payload is truncated", payload_size, remaining_size, vertex_count, index_count);

    SpriteMeshData mesh;
    mesh.SourceSize.width = reader.GetLEUInt16();
    mesh.SourceSize.height = reader.GetLEUInt16();
    mesh.SourceOffset.x = reader.GetLEInt32();
    mesh.SourceOffset.y = reader.GetLEInt32();
    FO_VERIFY_AND_THROW(mesh.SourceSize.width > 0 && mesh.SourceSize.height > 0, "Sprite mesh logical source dimensions must be positive", mesh.SourceSize);

    int64_t source_end_x = numeric_cast<int64_t>(mesh.SourceOffset.x) + size.width;
    int64_t source_end_y = numeric_cast<int64_t>(mesh.SourceOffset.y) + size.height;
    FO_VERIFY_AND_THROW(mesh.SourceOffset.x < mesh.SourceSize.width && source_end_x > 0 && mesh.SourceOffset.y < mesh.SourceSize.height && source_end_y > 0, "Sprite mesh cropped frame does not intersect its logical source", mesh.SourceOffset, mesh.SourceSize, size);
    mesh.Vertices.reserve(vertex_count);

    ipos32 minimum_vertex {size.width, size.height};
    ipos32 maximum_vertex {};

    for (uint16_t i = 0; i < vertex_count; i++) {
        uint16_t x = reader.GetLEUInt16();
        uint16_t y = reader.GetLEUInt16();
        FO_VERIFY_AND_THROW(x <= size.width && y <= size.height, "Sprite mesh vertex is outside the sprite frame", i, x, y, size);
        mesh.Vertices.emplace_back(numeric_cast<int32_t>(x), numeric_cast<int32_t>(y));
        minimum_vertex.x = std::min(minimum_vertex.x, numeric_cast<int32_t>(x));
        minimum_vertex.y = std::min(minimum_vertex.y, numeric_cast<int32_t>(y));
        maximum_vertex.x = std::max(maximum_vertex.x, numeric_cast<int32_t>(x));
        maximum_vertex.y = std::max(maximum_vertex.y, numeric_cast<int32_t>(y));
    }

    ipos32 expected_minimum_vertex {};
    ipos32 expected_maximum_vertex {size.width, size.height};
    FO_VERIFY_AND_THROW(minimum_vertex == expected_minimum_vertex && maximum_vertex == expected_maximum_vertex, "Sprite mesh vertices must occupy the complete cropped frame bounds", minimum_vertex, maximum_vertex, size);

    mesh.Indices.resize(numeric_cast<size_t>(index_count));

    for (uint32_t i = 0; i < index_count; i++) {
        uint16_t index = reader.GetLEUInt16();
        FO_VERIFY_AND_THROW(index < vertex_count, "Sprite mesh index is outside the vertex array", i, index, vertex_count);
        mesh.Indices[i] = index;
    }

    int64_t winding = 0;

    for (size_t i = 0; i < mesh.Indices.size(); i += 3) {
        ipos32 a = mesh.Vertices[mesh.Indices[i]];
        ipos32 b = mesh.Vertices[mesh.Indices[i + 1]];
        ipos32 c = mesh.Vertices[mesh.Indices[i + 2]];
        int64_t ab_x = numeric_cast<int64_t>(b.x) - a.x;
        int64_t ab_y = numeric_cast<int64_t>(b.y) - a.y;
        int64_t ac_x = numeric_cast<int64_t>(c.x) - a.x;
        int64_t ac_y = numeric_cast<int64_t>(c.y) - a.y;
        int64_t twice_area = ab_x * ac_y - ab_y * ac_x;
        FO_VERIFY_AND_THROW(twice_area != 0, "Sprite mesh contains a degenerate triangle", i / 3, a, b, c);

        if (winding == 0) {
            winding = twice_area;
        }
        else {
            FO_VERIFY_AND_THROW((winding > 0) == (twice_area > 0), "Sprite mesh triangles have inconsistent winding", i / 3, winding, twice_area);
        }
    }

    return optional<SpriteMeshData> {std::move(mesh)};
}

FO_END_NAMESPACE
