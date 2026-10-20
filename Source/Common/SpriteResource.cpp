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
    uint8_t magic = reader.GetUInt8();
    FO_VERIFY_AND_THROW(magic == SPRITE_RESOURCE_MAGIC, "Sprite resource header magic is invalid", magic, SPRITE_RESOURCE_MAGIC);
    uint8_t version = reader.GetUInt8();
    FO_VERIFY_AND_THROW(version == SPRITE_RESOURCE_VERSION, "Sprite resource version is unsupported", version, SPRITE_RESOURCE_VERSION);
    uint8_t count = reader.GetUInt8();
    FO_VERIFY_AND_THROW(count > 0, "Sprite resource contains no directions");
    SpriteResourceData result;
    result.Animation.Sprite.emplace();
    auto& info = *result.Animation.Sprite;
    result.Directions.resize(count);
    info.Directions.resize(count);

    for (uint8_t direction_index = 0; direction_index < count; direction_index++) {
        auto& direction = result.Directions[direction_index];
        auto& direction_info = info.Directions[direction_index];
        uint16_t frames = reader.GetLEUInt16();
        uint32_t total = reader.GetLEUInt32();
        FO_VERIFY_AND_THROW(frames > 0 && total >= frames, "Sprite sequence requires positive frames and durations", direction_index, frames, total);
        direction.Frames.resize(frames);
        direction_info.Frames.resize(frames);

        for (uint16_t frame_index = 0; frame_index < frames; frame_index++) {
            auto& frame = direction.Frames[frame_index];
            auto& frame_info = direction_info.Frames[frame_index];
            frame.DurationMs = reader.GetLEUInt32();
            FO_VERIFY_AND_THROW(frame.DurationMs > 0, "Sprite frame duration must be positive", direction_index, frame_index);
            frame.Offset = {reader.GetLEInt32(), reader.GetLEInt32()};
            frame.NextOffset = {reader.GetLEInt16(), reader.GetLEInt16()};
            uint8_t shared = reader.GetUInt8();
            FO_VERIFY_AND_THROW(shared <= 1, "Sprite frame reference flag is invalid", direction_index, frame_index, shared);

            if (shared != 0) {
                uint16_t index = reader.GetLEUInt16();
                FO_VERIFY_AND_THROW(index < frame_index, "Sprite frame reference points outside previously decoded frames", direction_index, frame_index, index);
                frame.SharedFrameIndex = index;
                frame.Size = direction.Frames[index].Size;
            }
            else {
                frame.Size = {reader.GetLEUInt16(), reader.GetLEUInt16()};
                FO_VERIFY_AND_THROW(frame.Size.width > 0 && frame.Size.height > 0, "Sprite frame dimensions must be positive", direction_index, frame_index, frame.Size);
                uint64_t pixels = numeric_cast<uint64_t>(frame.Size.width) * frame.Size.height;
                FO_VERIFY_AND_THROW(pixels * sizeof(ucolor) <= reader.GetSize() - reader.GetCurPos(), "Sprite frame pixel data is truncated", frame.Size);
                frame.Pixels.resize(numeric_cast<size_t>(pixels));
                reader.ReadObjectArray(span<ucolor> {frame.Pixels});
                frame.Mesh = ReadSpriteFrameMesh(reader, frame.Size);
            }

            frame_info = SpriteFrameInfo {.SharedFrameIndex = frame.SharedFrameIndex, .Offset = frame.Offset, .Size = frame.Size, .NextOffset = frame.NextOffset, .DurationMs = frame.DurationMs};
        }

        FO_VERIFY_AND_THROW(GetDurationMs(direction_info) == total, "Sprite sequence duration differs from its frame durations", direction_index, total);
    }

    uint8_t footer = reader.GetUInt8();
    FO_VERIFY_AND_THROW(footer == SPRITE_RESOURCE_MAGIC, "Sprite resource footer magic is invalid", footer, SPRITE_RESOURCE_MAGIC);
    FO_VERIFY_AND_THROW(reader.GetCurPos() == reader.GetSize(), "Sprite resource contains trailing data", reader.GetCurPos(), reader.GetSize());

    return result;
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
        memory::copy(image.Pixels.data() + image_offset, frame.Pixels.data() + frame_offset, numeric_cast<size_t>(copy_width) * sizeof(ucolor));
    }

    return image;
}

static auto ReadSpriteFrameMesh(FileReader& reader, isize32 size) -> optional<SpriteMeshData>
{
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
