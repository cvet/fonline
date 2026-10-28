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

#include "SpriteResource.h"
#include "FileSystem.h"

FO_BEGIN_NAMESPACE

static auto ReadSpriteFrameMesh(FileReader& reader, isize32 size) -> optional<SpriteMeshData>;

auto ReadSpriteResource(const_span<uint8_t> data) -> SpriteResourceData
{
    FO_TRACE_ZONE(Render);

    FileReader reader {data};

    uint8_t header_magic = reader.ReadUInt8();
    FO_VERIFY_AND_THROW(header_magic == SPRITE_RESOURCE_MAGIC, "Sprite resource header magic is invalid", header_magic, SPRITE_RESOURCE_MAGIC);

    uint8_t version = reader.ReadUInt8();
    FO_VERIFY_AND_THROW(version == SPRITE_RESOURCE_VERSION, "Sprite resource version is unsupported", version, SPRITE_RESOURCE_VERSION);

    SpriteResourceData resource;
    resource.Animation.Sprite.emplace();
    SpriteInfo& sprite_info = *resource.Animation.Sprite;
    int32_t frame_count = numeric_cast<int32_t>(reader.ReadLEUInt16());
    FO_VERIFY_AND_THROW(frame_count != 0, "Sprite resource contains no frames");
    sprite_info.FrameCount = frame_count;
    sprite_info.Duration = std::chrono::milliseconds {numeric_cast<std::chrono::milliseconds::rep>(reader.ReadLEUInt16())};

    int32_t direction_count = numeric_cast<int32_t>(reader.ReadUInt8());
    FO_VERIFY_AND_THROW(direction_count != 0, "Sprite resource contains no directions");
    resource.Directions.resize(numeric_cast<size_t>(direction_count));
    sprite_info.Directions.resize(numeric_cast<size_t>(direction_count));

    auto read_sprite_pixels = [&reader](isize32 size) -> vector<ucolor> {
        FO_VERIFY_AND_THROW(size.width > 0, "Sprite frame width must be positive", size.width);
        FO_VERIFY_AND_THROW(size.height > 0, "Sprite frame height must be positive", size.height);

        uint64_t pixel_count64 = numeric_cast<uint64_t>(size.width) * numeric_cast<uint64_t>(size.height);
        uint64_t pixel_data_size64 = pixel_count64 * numeric_cast<uint64_t>(sizeof(ucolor));
        size_t remaining_size = reader.GetSize() - reader.GetCurPos();
        FO_VERIFY_AND_THROW(numeric_cast<size_t>(pixel_data_size64) <= remaining_size, "Sprite frame pixel data is truncated", pixel_data_size64, remaining_size, size);

        vector<ucolor> pixels(numeric_cast<size_t>(pixel_count64));
        reader.ReadObjectArray(span<ucolor> {pixels});
        return pixels;
    };

    for (int32_t direction_index = 0; direction_index < direction_count; direction_index++) {
        SpriteResourceDirectionData& direction = resource.Directions[numeric_cast<size_t>(direction_index)];
        SpriteDirInfo& direction_info = sprite_info.Directions[numeric_cast<size_t>(direction_index)];
        direction.Frames.resize(numeric_cast<size_t>(frame_count));
        direction_info.Frames.resize(numeric_cast<size_t>(frame_count));

        for (int32_t frame_index = 0; frame_index < frame_count; frame_index++) {
            SpriteResourceFrameData& frame = direction.Frames[numeric_cast<size_t>(frame_index)];
            SpriteFrameInfo& frame_info = direction_info.Frames[numeric_cast<size_t>(frame_index)];
            uint8_t is_shared = reader.ReadUInt8();
            FO_VERIFY_AND_THROW(is_shared <= 1, "Sprite frame reference flag is invalid", is_shared, direction_index, frame_index);

            if (is_shared != 0) {
                int32_t shared_frame_index = numeric_cast<int32_t>(reader.ReadLEUInt16());
                FO_VERIFY_AND_THROW(shared_frame_index < frame_index, "Sprite frame reference points outside previously decoded frames", shared_frame_index, direction_index, frame_index);
                frame.SharedFrameIndex = shared_frame_index;
                frame_info = direction_info.Frames[numeric_cast<size_t>(shared_frame_index)];
                frame_info.SharedFrameIndex = shared_frame_index;
                continue;
            }

            frame.Offset.x = numeric_cast<int32_t>(reader.ReadLEInt16());
            frame.Offset.y = numeric_cast<int32_t>(reader.ReadLEInt16());
            frame.Size.width = numeric_cast<int32_t>(reader.ReadLEUInt16());
            frame.Size.height = numeric_cast<int32_t>(reader.ReadLEUInt16());
            frame.NextOffset.x = numeric_cast<int32_t>(reader.ReadLEInt16());
            frame.NextOffset.y = numeric_cast<int32_t>(reader.ReadLEInt16());
            frame_info.Offset = frame.Offset;
            frame_info.Size = frame.Size;
            frame_info.NextOffset = frame.NextOffset;
            frame.Pixels = read_sprite_pixels(frame.Size);
            frame.Mesh = ReadSpriteFrameMesh(reader, frame.Size);
        }
    }

    uint8_t footer_magic = reader.ReadUInt8();
    FO_VERIFY_AND_THROW(footer_magic == SPRITE_RESOURCE_MAGIC, "Sprite resource footer magic is invalid", footer_magic, SPRITE_RESOURCE_MAGIC);
    FO_VERIFY_AND_THROW(reader.GetCurPos() == reader.GetSize(), "Sprite resource contains trailing data", reader.GetCurPos(), reader.GetSize());

    return resource;
}

auto ExtractSpriteResourceFrameImage(SpriteResourceFrameData frame) -> SpriteResourceImageData
{
    FO_TRACE_ZONE(Render);

    FO_VERIFY_AND_THROW(!frame.SharedFrameIndex.has_value(), "Cannot extract image pixels from a shared sprite frame", frame.SharedFrameIndex.value_or(0));

    if (!frame.Mesh.has_value() || frame.Mesh->Indices.empty()) {
        return SpriteResourceImageData {.Size = frame.Size, .Pixels = std::move(frame.Pixels)};
    }

    const SpriteMeshData& mesh = *frame.Mesh;
    SpriteResourceImageData image;
    image.Size = mesh.SourceSize;
    uint64_t logical_pixel_count = numeric_cast<uint64_t>(image.Size.width) * numeric_cast<uint64_t>(image.Size.height);
    image.Pixels.resize(numeric_cast<size_t>(logical_pixel_count));

    int32_t source_begin_x = std::max(mesh.SourceOffset.x, 0);
    int32_t source_begin_y = std::max(mesh.SourceOffset.y, 0);
    int32_t frame_begin_x = std::max(-mesh.SourceOffset.x, 0);
    int32_t frame_begin_y = std::max(-mesh.SourceOffset.y, 0);
    int32_t copy_width = std::min(frame.Size.width - frame_begin_x, image.Size.width - source_begin_x);
    int32_t copy_height = std::min(frame.Size.height - frame_begin_y, image.Size.height - source_begin_y);
    FO_VERIFY_AND_THROW(copy_width > 0 && copy_height > 0, "Cropped sprite frame does not contain logical source pixels", frame.Size, mesh.SourceOffset, mesh.SourceSize);

    for (int32_t y = 0; y < copy_height; y++) {
        size_t frame_offset = numeric_cast<size_t>(numeric_cast<int64_t>(frame_begin_y + y) * numeric_cast<int64_t>(frame.Size.width) + numeric_cast<int64_t>(frame_begin_x));
        size_t image_offset = numeric_cast<size_t>(numeric_cast<int64_t>(source_begin_y + y) * numeric_cast<int64_t>(image.Size.width) + numeric_cast<int64_t>(source_begin_x));
        memory::copy(image.Pixels.data() + image_offset, frame.Pixels.data() + frame_offset, numeric_cast<size_t>(numeric_cast<int64_t>(copy_width) * numeric_cast<int64_t>(sizeof(ucolor))));
    }

    return image;
}

static auto ReadSpriteFrameMesh(FileReader& reader, isize32 size) -> optional<SpriteMeshData>
{
    uint8_t raw_kind = reader.ReadUInt8();

    if (raw_kind == static_cast<uint8_t>(SpriteMeshKind::Quad)) {
        return std::nullopt;
    }
    if (raw_kind == static_cast<uint8_t>(SpriteMeshKind::Empty)) {
        return SpriteMeshData {};
    }

    FO_VERIFY_AND_THROW(raw_kind == static_cast<uint8_t>(SpriteMeshKind::Mesh), "Sprite mesh kind is invalid", raw_kind);

    int32_t vertex_count = numeric_cast<int32_t>(reader.ReadLEUInt16());
    int64_t serialized_index_count = numeric_cast<int64_t>(reader.ReadLEUInt32());
    FO_VERIFY_AND_THROW(vertex_count >= 3, "Sprite mesh must contain at least three vertices", vertex_count);
    FO_VERIFY_AND_THROW(serialized_index_count >= 3 && serialized_index_count % 3 == 0, "Sprite mesh index count must contain complete triangles", serialized_index_count);
    FO_VERIFY_AND_THROW(serialized_index_count <= numeric_cast<int64_t>(vertex_count) * int64_t {6}, "Sprite mesh contains implausibly many indices", vertex_count, serialized_index_count);
    int32_t index_count = numeric_cast<int32_t>(serialized_index_count);

    constexpr int64_t mesh_header_size = numeric_cast<int64_t>(sizeof(uint16_t) * 2 + sizeof(int32_t) * 2);
    constexpr int64_t vertex_size = numeric_cast<int64_t>(sizeof(uint16_t) * 2);
    constexpr int64_t index_size = numeric_cast<int64_t>(sizeof(uint16_t));
    size_t payload_size = numeric_cast<size_t>(mesh_header_size + numeric_cast<int64_t>(vertex_count) * vertex_size + numeric_cast<int64_t>(index_count) * index_size);
    size_t remaining_size = reader.GetSize() - reader.GetCurPos();
    FO_VERIFY_AND_THROW(payload_size <= remaining_size, "Sprite mesh payload is truncated", payload_size, remaining_size, vertex_count, index_count);

    SpriteMeshData mesh;
    mesh.SourceSize.width = numeric_cast<int32_t>(reader.ReadLEUInt16());
    mesh.SourceSize.height = numeric_cast<int32_t>(reader.ReadLEUInt16());
    mesh.SourceOffset.x = reader.ReadLEInt32();
    mesh.SourceOffset.y = reader.ReadLEInt32();
    FO_VERIFY_AND_THROW(mesh.SourceSize.width > 0 && mesh.SourceSize.height > 0, "Sprite mesh logical source dimensions must be positive", mesh.SourceSize);

    int64_t source_end_x = numeric_cast<int64_t>(mesh.SourceOffset.x) + numeric_cast<int64_t>(size.width);
    int64_t source_end_y = numeric_cast<int64_t>(mesh.SourceOffset.y) + numeric_cast<int64_t>(size.height);
    FO_VERIFY_AND_THROW(mesh.SourceOffset.x < mesh.SourceSize.width && source_end_x > 0 && mesh.SourceOffset.y < mesh.SourceSize.height && source_end_y > 0, "Sprite mesh cropped frame does not intersect its logical source", mesh.SourceOffset, mesh.SourceSize, size);
    mesh.Vertices.reserve(numeric_cast<size_t>(vertex_count));

    ipos32 minimum_vertex {size.width, size.height};
    ipos32 maximum_vertex {};

    for (int32_t i = 0; i < vertex_count; i++) {
        int32_t x = numeric_cast<int32_t>(reader.ReadLEUInt16());
        int32_t y = numeric_cast<int32_t>(reader.ReadLEUInt16());
        FO_VERIFY_AND_THROW(x <= size.width && y <= size.height, "Sprite mesh vertex is outside the sprite frame", i, x, y, size);
        mesh.Vertices.emplace_back(x, y);
        minimum_vertex.x = std::min(minimum_vertex.x, x);
        minimum_vertex.y = std::min(minimum_vertex.y, y);
        maximum_vertex.x = std::max(maximum_vertex.x, x);
        maximum_vertex.y = std::max(maximum_vertex.y, y);
    }

    ipos32 expected_minimum_vertex {};
    ipos32 expected_maximum_vertex {size.width, size.height};
    FO_VERIFY_AND_THROW(minimum_vertex == expected_minimum_vertex && maximum_vertex == expected_maximum_vertex, "Sprite mesh vertices must occupy the complete cropped frame bounds", minimum_vertex, maximum_vertex, size);

    mesh.Indices.resize(numeric_cast<size_t>(index_count));

    for (int32_t i = 0; i < index_count; i++) {
        uint16_t index = reader.ReadLEUInt16();
        FO_VERIFY_AND_THROW(numeric_cast<int32_t>(index) < vertex_count, "Sprite mesh index is outside the vertex array", i, index, vertex_count);
        mesh.Indices[numeric_cast<size_t>(i)] = index;
    }

    int64_t winding = 0;

    for (size_t i = 0; i < mesh.Indices.size(); i += 3) {
        ipos32 a = mesh.Vertices[numeric_cast<size_t>(mesh.Indices[i])];
        ipos32 b = mesh.Vertices[numeric_cast<size_t>(mesh.Indices[i + 1])];
        ipos32 c = mesh.Vertices[numeric_cast<size_t>(mesh.Indices[i + 2])];
        int64_t ab_x = numeric_cast<int64_t>(b.x) - numeric_cast<int64_t>(a.x);
        int64_t ab_y = numeric_cast<int64_t>(b.y) - numeric_cast<int64_t>(a.y);
        int64_t ac_x = numeric_cast<int64_t>(c.x) - numeric_cast<int64_t>(a.x);
        int64_t ac_y = numeric_cast<int64_t>(c.y) - numeric_cast<int64_t>(a.y);
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
