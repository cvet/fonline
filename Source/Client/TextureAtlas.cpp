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
//

#include "TextureAtlas.h"
#include "Application.h"

FO_BEGIN_NAMESPACE

static constexpr int32_t ATLAS_SPRITES_PADDING = 1;
static constexpr ucolor ATLAS_DUMP_QUAD_COLOR {255, 255, 0, 255};
static constexpr ucolor ATLAS_DUMP_EMPTY_COLOR {255, 0, 0, 255};
static constexpr ucolor ATLAS_DUMP_MESH_COLOR {255, 0, 255, 255};
static constexpr ucolor ATLAS_DUMP_VERTEX_COLOR {0, 255, 255, 255};

TextureAtlas::TextureAtlas(AtlasType type, ptr<RenderTarget> rt) noexcept :
    _type {type},
    _rt {rt},
    _rootNode {nullptr, ipos32(), rt->GetSize()}
{
    _rt->GetTexture()->FlippedHeight = false;
}

TextureAtlas::SpaceNode::SpaceNode(nptr<SpaceNode> parent, ipos32 pos, isize32 size) :
    Parent {parent},
    Pos {pos},
    Size {size}
{
    FO_STACK_TRACE_ENTRY();
}

auto TextureAtlas::SpaceNode::IsBusyRecursively() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (Busy) {
        return true;
    }

    for (size_t i = 0; i < Children.size(); i++) {
        if (Children[i]->IsBusyRecursively()) {
            return true;
        }
    }

    return false;
}

auto TextureAtlas::SpaceNode::FindPosition(isize32 size) -> nptr<SpaceNode>
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i != Children.size(); ++i) {
        if (auto child_node = Children[i]->FindPosition(size)) {
            return child_node;
        }
    }

    if (!Busy && Size.width >= size.width && Size.height >= size.height) {
        Busy = true;

        if (Size.width == size.width && Size.height > size.height) {
            Children.emplace_back(SafeAlloc::MakeUnique<SpaceNode>(this, ipos32 {Pos.x, Pos.y + size.height}, isize32 {Size.width, Size.height - size.height}));
            Size.height = size.height;
        }
        else if (Size.height == size.height && Size.width > size.width) {
            Children.emplace_back(SafeAlloc::MakeUnique<SpaceNode>(this, ipos32 {Pos.x + size.width, Pos.y}, isize32 {Size.width - size.width, Size.height}));
            Size.width = size.width;
        }
        else if (Size.width > size.width && Size.height > size.height) {
            Children.emplace_back(SafeAlloc::MakeUnique<SpaceNode>(this, ipos32 {Pos.x + size.width, Pos.y}, isize32 {Size.width - size.width, size.height}));
            Children.emplace_back(SafeAlloc::MakeUnique<SpaceNode>(this, ipos32 {Pos.x, Pos.y + size.height}, isize32 {Size.width, Size.height - size.height}));
            Size.width = size.width;
            Size.height = size.height;
        }

        return this;
    }

    return nullptr;
}

void TextureAtlas::SpaceNode::Free() noexcept
{
    FO_STACK_TRACE_ENTRY();

    Busy = false;
    SpriteMesh = nullptr;

    // Collapse free children
    if (!Children.empty()) {
        bool all_children_free = true;

        for (size_t i = 0; i < Children.size(); i++) {
            if (Children[i]->IsBusyRecursively()) {
                all_children_free = false;
                break;
            }
        }

        if (all_children_free) {
            int32_t max_x = Pos.x + Size.width;
            int32_t max_y = Pos.y + Size.height;

            for (size_t i = 0; i < Children.size(); i++) {
                auto child = Children[i].as_ptr();

                max_x = std::max(max_x, child->Pos.x + child->Size.width);
                max_y = std::max(max_y, child->Pos.y + child->Size.height);
            }

            Size.width = max_x - Pos.x;
            Size.height = max_y - Pos.y;

            Children.clear();
        }
    }

    // Populate collapsing to root
    if (Children.empty() && Parent && !Parent->Busy) {
        Parent->Free();
    }
}

void TextureAtlas::SpaceNode::DrawDumpOverlay(span<ucolor> pixels, isize32 atlas_size) const
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(atlas_size.width > 0 && atlas_size.height > 0, "Atlas dump dimensions must be positive", atlas_size);
    FO_VERIFY_AND_THROW(pixels.size() == numeric_cast<size_t>(atlas_size.width) * atlas_size.height, "Atlas dump pixel count does not match atlas dimensions", pixels.size(), atlas_size);

    if (Busy) {
        const ipos32 sprite_origin = {Pos.x + ATLAS_SPRITES_PADDING, Pos.y + ATLAS_SPRITES_PADDING};
        const ipos32 sprite_end = {Pos.x + Size.width - ATLAS_SPRITES_PADDING, Pos.y + Size.height - ATLAS_SPRITES_PADDING};

        if (!SpriteMesh) {
            DrawAtlasDumpLine(pixels, atlas_size, sprite_origin, {sprite_end.x, sprite_origin.y}, ATLAS_DUMP_QUAD_COLOR);
            DrawAtlasDumpLine(pixels, atlas_size, {sprite_end.x, sprite_origin.y}, sprite_end, ATLAS_DUMP_QUAD_COLOR);
            DrawAtlasDumpLine(pixels, atlas_size, sprite_end, {sprite_origin.x, sprite_end.y}, ATLAS_DUMP_QUAD_COLOR);
            DrawAtlasDumpLine(pixels, atlas_size, {sprite_origin.x, sprite_end.y}, sprite_origin, ATLAS_DUMP_QUAD_COLOR);
        }
        else if (SpriteMesh->Indices.empty()) {
            DrawAtlasDumpLine(pixels, atlas_size, sprite_origin, sprite_end, ATLAS_DUMP_EMPTY_COLOR);
            DrawAtlasDumpLine(pixels, atlas_size, {sprite_end.x, sprite_origin.y}, {sprite_origin.x, sprite_end.y}, ATLAS_DUMP_EMPTY_COLOR);
        }
        else {
            for (size_t i = 0; i + 2 < SpriteMesh->Indices.size(); i += 3) {
                const uint16_t index_a = SpriteMesh->Indices[i];
                const uint16_t index_b = SpriteMesh->Indices[i + 1];
                const uint16_t index_c = SpriteMesh->Indices[i + 2];

                if (index_a >= SpriteMesh->Vertices.size() || index_b >= SpriteMesh->Vertices.size() || index_c >= SpriteMesh->Vertices.size()) {
                    continue;
                }

                const ipos32 a = sprite_origin + SpriteMesh->Vertices[index_a];
                const ipos32 b = sprite_origin + SpriteMesh->Vertices[index_b];
                const ipos32 c = sprite_origin + SpriteMesh->Vertices[index_c];
                DrawAtlasDumpLine(pixels, atlas_size, a, b, ATLAS_DUMP_MESH_COLOR);
                DrawAtlasDumpLine(pixels, atlas_size, b, c, ATLAS_DUMP_MESH_COLOR);
                DrawAtlasDumpLine(pixels, atlas_size, c, a, ATLAS_DUMP_MESH_COLOR);
            }

            for (const ipos32 vertex : SpriteMesh->Vertices) {
                const ipos32 vertex_pos = sprite_origin + vertex;
                if (atlas_size.is_valid_pos(vertex_pos)) {
                    pixels[numeric_cast<size_t>(vertex_pos.y) * atlas_size.width + vertex_pos.x] = ATLAS_DUMP_VERTEX_COLOR;
                }
            }
        }
    }

    for (size_t i = 0; i < Children.size(); i++) {
        Children[i]->DrawDumpOverlay(pixels, atlas_size);
    }
}

void TextureAtlas::SpaceNode::DrawAtlasDumpLine(span<ucolor> pixels, isize32 atlas_size, ipos32 from, ipos32 to, ucolor color) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32_t dx = std::abs(to.x - from.x);
    const int32_t sx = from.x < to.x ? 1 : -1;
    const int32_t dy = -std::abs(to.y - from.y);
    const int32_t sy = from.y < to.y ? 1 : -1;
    int32_t error = dx + dy;

    while (true) {
        if (atlas_size.is_valid_pos(from)) {
            pixels[numeric_cast<size_t>(from.y) * atlas_size.width + from.x] = color;
        }

        if (from == to) {
            break;
        }

        const int32_t double_error = error * 2;
        if (double_error >= dy) {
            error += dy;
            from.x += sx;
        }
        if (double_error <= dx) {
            error += dx;
            from.y += sy;
        }
    }
}

TextureAtlasManager::TextureAtlasManager(ptr<RenderSettings> settings, ptr<RenderTargetManager> rt_mngr) :
    _settings {settings},
    _rtMngr {rt_mngr}
{
}

auto TextureAtlasManager::CreateAtlas(AtlasType atlas_type, isize32 request_size) -> ptr<TextureAtlas>
{
    FO_STACK_TRACE_ENTRY();

    // Cleanup expired atlases
    for (auto it = _allAtlases.begin(); it != _allAtlases.end();) {
        if (it->get()->GetType() == AtlasType::OneImage && !it->get()->GetLayout()->Busy) {
            it = _allAtlases.erase(it);
        }
        else {
            ++it;
        }
    }

    // Create new
    FO_VERIFY_AND_THROW(request_size.width > 0, "Requested atlas width must be positive", request_size.width);
    FO_VERIFY_AND_THROW(request_size.height > 0, "Requested atlas height must be positive", request_size.height);

    isize32 result_size;

    switch (atlas_type) {
    case AtlasType::IfaceSprites:
        result_size.width = std::min(AppRender::MAX_ATLAS_WIDTH, 4096);
        result_size.height = std::min(AppRender::MAX_ATLAS_HEIGHT, 4096);
        break;
    case AtlasType::MapSprites:
        result_size.width = std::min(AppRender::MAX_ATLAS_WIDTH, 2048);
        result_size.height = std::min(AppRender::MAX_ATLAS_HEIGHT, 8192);
        break;
    case AtlasType::MeshTextures:
        result_size.width = std::min(AppRender::MAX_ATLAS_WIDTH, 1024);
        result_size.height = std::min(AppRender::MAX_ATLAS_HEIGHT, 2048);
        break;
    case AtlasType::OneImage:
        result_size = request_size;
        break;
    }

    FO_VERIFY_AND_THROW(result_size.width >= request_size.width, "Texture atlas width cannot satisfy requested image width", atlas_type, result_size.width, request_size.width);
    FO_VERIFY_AND_THROW(result_size.height >= request_size.height, "Texture atlas height cannot satisfy requested image height", atlas_type, result_size.height, request_size.height);

    auto rt = _rtMngr->CreateRenderTarget(false, result_size, _settings->AtlasLinearFiltration);
    auto created_atlas = SafeAlloc::MakeUnique<TextureAtlas>(atlas_type, rt);

    _allAtlases.push_back(std::move(created_atlas));
    return _allAtlases.back();
}

auto TextureAtlasManager::FindAtlasPlace(AtlasType atlas_type, isize32 size) -> tuple<ptr<TextureAtlas>, unique_del_ptr<TextureAtlas::SpaceNode>, ipos32>
{
    FO_STACK_TRACE_ENTRY();

    function<void(TextureAtlas::SpaceNode*)> free_node = [](TextureAtlas::SpaceNode* node) noexcept { node->Free(); };

    nptr<TextureAtlas> atlas {};
    unique_del_nptr<TextureAtlas::SpaceNode> atlas_node {};
    const isize32 size_with_padding = {size.width + ATLAS_SPRITES_PADDING * 2, size.height + ATLAS_SPRITES_PADDING * 2};

    if (atlas_type != AtlasType::OneImage) {
        for (auto& check_atlas : _allAtlases) {
            if (check_atlas->GetType() != atlas_type) {
                continue;
            }

            auto node = check_atlas->GetLayout()->FindPosition(size_with_padding);

            if (node) {
                atlas = check_atlas;
                atlas_node = unique_del_nptr<TextureAtlas::SpaceNode> {node.get(), std::move(free_node)};
                break;
            }
        }
    }

    // Create new
    if (!atlas) {
        auto new_atlas = CreateAtlas(atlas_type, size_with_padding);
        atlas = new_atlas;
        auto node = new_atlas->GetLayout()->FindPosition(size_with_padding);
        FO_VERIFY_AND_THROW(node, "Missing required atlas node");
        atlas_node = unique_del_nptr<TextureAtlas::SpaceNode> {node.get(), std::move(free_node)};
    }

    FO_VERIFY_AND_THROW(atlas, "Atlas placement is missing its atlas");
    FO_VERIFY_AND_THROW(atlas_node, "Atlas placement is missing its layout node");

    const ipos32 pos = {atlas_node->Pos.x + ATLAS_SPRITES_PADDING, atlas_node->Pos.y + ATLAS_SPRITES_PADDING};

    return {atlas, take_not_null(atlas_node), pos};
}

void TextureAtlasManager::DumpAtlases() const
{
    FO_STACK_TRACE_ENTRY();

    size_t atlases_memory_size = 0;

    for (size_t i = 0; i < _allAtlases.size(); i++) {
        atlases_memory_size += _allAtlases[i]->GetSize().square() * 4;
    }

    const auto time = nanotime::now().desc(true);
    const string dir = strex("TexDump_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}_{}.{:03}mb", //
        time.year, time.month, time.day, time.hour, time.minute, time.second, //
        atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000);

    size_t count = 1;

    for (size_t i = 0; i < _allAtlases.size(); i++) {
        auto atlas = _allAtlases[i].as_ptr();
        string atlas_type_name;

        switch (atlas->GetType()) {
        case AtlasType::IfaceSprites:
            atlas_type_name = "Static";
            break;
        case AtlasType::MapSprites:
            atlas_type_name = "MapSprites";
            break;
        case AtlasType::OneImage:
            atlas_type_name = "OneImage";
            break;
        case AtlasType::MeshTextures:
            atlas_type_name = "MeshTextures";
            break;
        }

        const string fname = strex("{}/{}_{}_{}x{}.tga", dir, atlas_type_name, count, atlas->GetSize().width, atlas->GetSize().height);
        auto tex_data = atlas->GetTexture()->GetTextureRegion({0, 0}, atlas->GetSize());
        atlas->GetLayout()->DrawDumpOverlay(tex_data, atlas->GetSize());
        WriteSimpleTga(fname, atlas->GetSize(), std::move(tex_data));
        count++;
    }
}

FO_END_NAMESPACE
