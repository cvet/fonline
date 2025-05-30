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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE();

static constexpr int32 ATLAS_SPRITES_PADDING = 1;

TextureAtlas::SpaceNode::SpaceNode(SpaceNode* parent, ipos pos, isize size) :
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

    for (const auto& child : Children) {
        if (child->IsBusyRecursively()) {
            return true;
        }
    }

    return false;
}

auto TextureAtlas::SpaceNode::FindPosition(isize size) -> SpaceNode*
{
    FO_STACK_TRACE_ENTRY();

    for (auto& child : Children) {
        if (auto* child_node = child->FindPosition(size); child_node != nullptr) {
            return child_node;
        }
    }

    if (!Busy && Size.width >= size.width && Size.height >= size.height) {
        Busy = true;

        if (Size.width == size.width && Size.height > size.height) {
            Children.emplace_back(SafeAlloc::MakeUnique<SpaceNode>(this, ipos {Pos.x, Pos.y + size.height}, isize {Size.width, Size.height - size.height}));
            Size.height = size.height;
        }
        else if (Size.height == size.height && Size.width > size.width) {
            Children.emplace_back(SafeAlloc::MakeUnique<SpaceNode>(this, ipos {Pos.x + size.width, Pos.y}, isize {Size.width - size.width, Size.height}));
            Size.width = size.width;
        }
        else if (Size.width > size.width && Size.height > size.height) {
            Children.emplace_back(SafeAlloc::MakeUnique<SpaceNode>(this, ipos {Pos.x + size.width, Pos.y}, isize {Size.width - size.width, size.height}));
            Children.emplace_back(SafeAlloc::MakeUnique<SpaceNode>(this, ipos {Pos.x, Pos.y + size.height}, isize {Size.width, Size.height - size.height}));
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

    // Collapse free children
    if (!Children.empty()) {
        bool all_children_free = true;

        for (const auto& child : Children) {
            if (child->IsBusyRecursively()) {
                all_children_free = false;
                break;
            }
        }

        if (all_children_free) {
            int32 max_x = Pos.x + Size.width;
            int32 max_y = Pos.y + Size.height;

            for (const auto& child : Children) {
                max_x = std::max(max_x, child->Pos.x + child->Size.width);
                max_y = std::max(max_y, child->Pos.y + child->Size.height);
            }

            Size.width = max_x - Pos.x;
            Size.height = max_y - Pos.y;

            Children.clear();
        }
    }

    // Populate collapsing to root
    if (Children.empty() && Parent != nullptr && !Parent->Busy) {
        Parent->Free();
    }
}

TextureAtlasManager::TextureAtlasManager(RenderSettings& settings, RenderTargetManager& rt_mngr) :
    _settings {settings},
    _rtMngr {rt_mngr}
{
}

auto TextureAtlasManager::CreateAtlas(AtlasType atlas_type, isize request_size) -> TextureAtlas*
{
    FO_STACK_TRACE_ENTRY();

    // Cleanup expired atlases
    for (auto it = _allAtlases.begin(); it != _allAtlases.end();) {
        if (it->get()->Type == AtlasType::OneImage && !it->get()->RootNode->Busy) {
            it = _allAtlases.erase(it);
        }
        else {
            ++it;
        }
    }

    // Create new
    FO_RUNTIME_ASSERT(request_size.width > 0);
    FO_RUNTIME_ASSERT(request_size.height > 0);

    auto atlas = SafeAlloc::MakeUnique<TextureAtlas>();
    atlas->Type = atlas_type;

    isize result_size;

    switch (atlas->Type) {
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

    FO_RUNTIME_ASSERT(result_size.width >= request_size.width);
    FO_RUNTIME_ASSERT(result_size.height >= request_size.height);

    atlas->RTarg = _rtMngr.CreateRenderTarget(false, RenderTarget::SizeKindType::Custom, result_size, _settings.AtlasLinearFiltration);
    atlas->MainTex = atlas->RTarg->MainTex.get();
    atlas->MainTex->FlippedHeight = false;
    atlas->Size = result_size;
    atlas->RootNode = SafeAlloc::MakeUnique<TextureAtlas::SpaceNode>(nullptr, ipos {0, 0}, result_size);

    _allAtlases.push_back(std::move(atlas));
    return _allAtlases.back().get();
}

auto TextureAtlasManager::FindAtlasPlace(AtlasType atlas_type, isize size) -> tuple<TextureAtlas*, TextureAtlas::SpaceNode*, ipos>
{
    FO_STACK_TRACE_ENTRY();

    // Find place in already created atlas
    TextureAtlas* atlas = nullptr;
    TextureAtlas::SpaceNode* atlas_node = nullptr;
    const isize size_with_padding = {size.width + ATLAS_SPRITES_PADDING * 2, size.height + ATLAS_SPRITES_PADDING * 2};

    if (atlas_type != AtlasType::OneImage) {
        for (auto& check_atlas : _allAtlases) {
            if (check_atlas->Type != atlas_type) {
                continue;
            }

            auto* node = check_atlas->RootNode->FindPosition(size_with_padding);

            if (node != nullptr) {
                atlas = check_atlas.get();
                atlas_node = node;
                break;
            }
        }
    }

    // Create new
    if (atlas == nullptr) {
        atlas = CreateAtlas(atlas_type, size_with_padding);
        atlas_node = atlas->RootNode->FindPosition(size_with_padding);
        FO_RUNTIME_ASSERT(atlas_node);
    }

    const ipos pos = {atlas_node->Pos.x + ATLAS_SPRITES_PADDING, atlas_node->Pos.y + ATLAS_SPRITES_PADDING};

    return {atlas, atlas_node, pos};
}

void TextureAtlasManager::DumpAtlases() const
{
    FO_STACK_TRACE_ENTRY();

    size_t atlases_memory_size = 0;

    for (const auto& atlas : _allAtlases) {
        atlases_memory_size += numeric_cast<size_t>(atlas->Size.width) * atlas->Size.height * 4;
    }

    const auto time = nanotime::now().desc(true);
    const string dir = strex("{:04}.{:02}.{:02}_{:02}-{:02}-{:02}_{}.{:03}mb", //
        time.year, time.month, time.day, time.hour, time.minute, time.second, //
        atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000);

    size_t count = 1;

    for (const auto& atlas : _allAtlases) {
        string atlas_type_name;
        switch (atlas->Type) {
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

        const string fname = strex("{}/{}_{}_{}x{}.tga", dir, atlas_type_name, count, atlas->Size.width, atlas->Size.height);
        auto tex_data = atlas->MainTex->GetTextureRegion({0, 0}, atlas->Size);
        GenericUtils::WriteSimpleTga(fname, atlas->Size, std::move(tex_data));
        count++;
    }
}

FO_END_NAMESPACE();
