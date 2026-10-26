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

#include "SpriteGeometry.h"

FO_BEGIN_NAMESPACE

auto GetPreparedSpriteVertexCount(const PreparedSpriteGeometry& geometry) -> size_t
{
    return geometry.Mesh ? (geometry.Mesh->Indices.empty() ? 0 : geometry.Mesh->Vertices.size()) : 4;
}

auto GetPreparedSpriteIndexCount(const PreparedSpriteGeometry& geometry) -> size_t
{
    return geometry.Mesh ? geometry.Mesh->Indices.size() : 6;
}

void WritePreparedSpriteGeometry(const PreparedSpriteGeometry& geometry, span<Vertex2D> vertices, span<vindex_t> indices, size_t base_vertex)
{
    size_t vertex_count = GetPreparedSpriteVertexCount(geometry);
    size_t index_count = GetPreparedSpriteIndexCount(geometry);
    FO_VERIFY_AND_THROW(vertices.size() == vertex_count && indices.size() == index_count, "Prepared sprite output ranges do not match geometry", vertices.size(), indices.size(), vertex_count, index_count);

    if (index_count == 0) {
        return;
    }

    size_t max_index = std::numeric_limits<vindex_t>::max();
    FO_VERIFY_AND_THROW(vertex_count > 0 && base_vertex <= max_index && vertex_count - 1 <= max_index - base_vertex, "Prepared sprite vertex range exceeds the index type", base_vertex, vertex_count);

    if (geometry.Mesh) {
        const auto& mesh = *geometry.Mesh;
        FO_VERIFY_AND_THROW(mesh.SourceSize.width > 0 && mesh.SourceSize.height > 0 && geometry.FrameSize.width > 0 && geometry.FrameSize.height > 0, "Prepared sprite mesh dimensions must be positive");

        for (uint16_t index : mesh.Indices) {
            FO_VERIFY_AND_THROW(index < vertex_count, "Prepared sprite mesh index is outside its vertex range", index, vertex_count);
        }

        for (ipos32 vertex : mesh.Vertices) {
            int64_t source_x = numeric_cast<int64_t>(vertex.x) + mesh.SourceOffset.x;
            int64_t source_y = numeric_cast<int64_t>(vertex.y) + mesh.SourceOffset.y;
            (void)numeric_cast<int32_t>(source_x);
            (void)numeric_cast<int32_t>(source_y);
        }
    }

    const frect32& pos = geometry.DrawRect;
    size_t vpos = 0;
    size_t ipos = 0;

    if (geometry.Mesh) {
        const auto& mesh = *geometry.Mesh;
        isize32 frame_size = geometry.FrameSize;
        float32_t frame_width = numeric_cast<float32_t>(frame_size.width);
        float32_t frame_height = numeric_cast<float32_t>(frame_size.height);
        float32_t logical_width = numeric_cast<float32_t>(mesh.SourceSize.width);
        float32_t logical_height = numeric_cast<float32_t>(mesh.SourceSize.height);
        ucolor color_left = geometry.ColorLeft;
        ucolor color_right = geometry.ColorRight;
        uint32_t color_width = numeric_cast<uint32_t>(mesh.SourceSize.width);

        for (ipos32 local_pos : mesh.Vertices) {
            ipos32 source_pos = local_pos + mesh.SourceOffset;
            float32_t logical_x = numeric_cast<float32_t>(source_pos.x) / logical_width;
            float32_t logical_y = numeric_cast<float32_t>(source_pos.y) / logical_height;
            float32_t frame_x = numeric_cast<float32_t>(local_pos.x) / frame_width;
            float32_t frame_y = numeric_cast<float32_t>(local_pos.y) / frame_height;
            int32_t source_x = std::clamp(source_pos.x, 0, mesh.SourceSize.width);
            uint32_t color_x = numeric_cast<uint32_t>(source_x);
            auto interpolate_component = [color_x, color_width](uint8_t left_component, uint8_t right_component) noexcept -> uint8_t {
                uint32_t weighted = numeric_cast<uint32_t>(left_component) * (color_width - color_x) + numeric_cast<uint32_t>(right_component) * color_x;
                return numeric_cast<uint8_t>((weighted + color_width / 2) / color_width);
            };
            auto& vertex = vertices[vpos++];

            vertex.PosX = pos.x + pos.width * logical_x;
            vertex.PosY = pos.y + pos.height * logical_y;
            vertex.PosZ = 0.0f;
            vertex.TexU = geometry.TextureRect.x + geometry.TextureRect.width * frame_x;
            vertex.TexV = geometry.TextureRect.y + geometry.TextureRect.height * frame_y;
            vertex.EggFlags[0] = 0.0f;
            vertex.EggFlags[1] = 0.0f;
            vertex.Color = {
                interpolate_component(color_left.comp.r, color_right.comp.r),
                interpolate_component(color_left.comp.g, color_right.comp.g),
                interpolate_component(color_left.comp.b, color_right.comp.b),
                interpolate_component(color_left.comp.a, color_right.comp.a),
            };
        }

        for (uint16_t local_index : mesh.Indices) {
            indices[ipos++] = numeric_cast<vindex_t>(base_vertex + local_index);
        }
    }
    else {
        indices[ipos++] = numeric_cast<vindex_t>(base_vertex + 0);
        indices[ipos++] = numeric_cast<vindex_t>(base_vertex + 1);
        indices[ipos++] = numeric_cast<vindex_t>(base_vertex + 3);
        indices[ipos++] = numeric_cast<vindex_t>(base_vertex + 1);
        indices[ipos++] = numeric_cast<vindex_t>(base_vertex + 2);
        indices[ipos++] = numeric_cast<vindex_t>(base_vertex + 3);

        auto& v0 = vertices[vpos++];
        v0.PosX = pos.x;
        v0.PosY = pos.y + pos.height;
        v0.PosZ = 0.0f;
        v0.TexU = geometry.TextureRect.x;
        v0.TexV = geometry.TextureRect.y + geometry.TextureRect.height;
        v0.EggFlags[0] = 0.0f;
        v0.EggFlags[1] = 0.0f;
        v0.Color = geometry.ColorLeft;

        auto& v1 = vertices[vpos++];
        v1.PosX = pos.x;
        v1.PosY = pos.y;
        v1.PosZ = 0.0f;
        v1.TexU = geometry.TextureRect.x;
        v1.TexV = geometry.TextureRect.y;
        v1.EggFlags[0] = 0.0f;
        v1.EggFlags[1] = 0.0f;
        v1.Color = geometry.ColorLeft;

        auto& v2 = vertices[vpos++];
        v2.PosX = pos.x + pos.width;
        v2.PosY = pos.y;
        v2.PosZ = 0.0f;
        v2.TexU = geometry.TextureRect.x + geometry.TextureRect.width;
        v2.TexV = geometry.TextureRect.y;
        v2.EggFlags[0] = 0.0f;
        v2.EggFlags[1] = 0.0f;
        v2.Color = geometry.ColorRight;

        auto& v3 = vertices[vpos++];
        v3.PosX = pos.x + pos.width;
        v3.PosY = pos.y + pos.height;
        v3.PosZ = 0.0f;
        v3.TexU = geometry.TextureRect.x + geometry.TextureRect.width;
        v3.TexV = geometry.TextureRect.y + geometry.TextureRect.height;
        v3.EggFlags[0] = 0.0f;
        v3.EggFlags[1] = 0.0f;
        v3.Color = geometry.ColorRight;
    }

    TransformPreparedSpriteGeometry(geometry.Transform, vertices);
}

void TransformPreparedSpriteGeometry(const PreparedSpriteTransform& transform, span<Vertex2D> vertices)
{
    if (!transform.MapSprite) {
        return;
    }

    for (auto& vertex : vertices) {
        vertex.PosZ = transform.ProjectionZ;

        if (transform.Rotate) {
            float32_t dx = vertex.PosX - transform.Center.x;
            float32_t dy = vertex.PosY - transform.Center.y;
            vertex.PosX = transform.Center.x + dx * transform.CosAngle - dy * transform.SinAngle;
            vertex.PosY = transform.Center.y + (dx * transform.SinAngle + dy * transform.CosAngle) * transform.YScale;
        }

        if (transform.Standing) {
            float32_t map_y = transform.ProjectionY + (vertex.PosY - transform.SceneRootY);
            vertex.PosZ = transform.ProjectionZ - (map_y - transform.ProjectionY) * transform.DepthSlope;
        }

        if (transform.EggFlags[0] != 0.0f || transform.EggFlags[1] != 0.0f) {
            vertex.EggFlags[0] = transform.EggFlags[0];
            vertex.EggFlags[1] = transform.EggFlags[1];
        }
    }
}

FO_END_NAMESPACE

