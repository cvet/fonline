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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "DiskFileSystem.h"
#include "GenericUtils.h"
#include "StringUtils.h"
#include "Timer.h"

static constexpr int ATLAS_SPRITES_PADDING = 1;

TextureAtlas::SpaceNode::SpaceNode(SpaceNode* parent, int x, int y, int width, int height) :
    Parent {parent},
    PosX {x},
    PosY {y},
    Width {width},
    Height {height}
{
    STACK_TRACE_ENTRY();
}

auto TextureAtlas::SpaceNode::IsBusyRecursively() const noexcept -> bool
{
    STACK_TRACE_ENTRY();

    if (Busy) {
        return true;
    }

    for (auto&& child : Children) {
        if (child->IsBusyRecursively()) {
            return true;
        }
    }

    return false;
}

auto TextureAtlas::SpaceNode::FindPosition(int width, int height) -> SpaceNode*
{
    STACK_TRACE_ENTRY();

    for (auto&& child : Children) {
        if (auto* child_node = child->FindPosition(width, height); child_node != nullptr) {
            return child_node;
        }
    }

    if (!Busy && Width >= width && Height >= height) {
        Busy = true;

        if (Width == width && Height > height) {
            Children.emplace_back(std::make_unique<SpaceNode>(this, PosX, PosY + height, Width, Height - height));
            Height = height;
        }
        else if (Height == height && Width > width) {
            Children.emplace_back(std::make_unique<SpaceNode>(this, PosX + width, PosY, Width - width, Height));
            Width = width;
        }
        else if (Width > width && Height > height) {
            Children.emplace_back(std::make_unique<SpaceNode>(this, PosX + width, PosY, Width - width, height));
            Children.emplace_back(std::make_unique<SpaceNode>(this, PosX, PosY + height, Width, Height - height));
            Width = width;
            Height = height;
        }

        return this;
    }

    return nullptr;
}

void TextureAtlas::SpaceNode::Free() noexcept
{
    STACK_TRACE_ENTRY();

    Busy = false;

    // Collapse free children
    if (!Children.empty()) {
        bool all_children_free = true;

        for (auto&& child : Children) {
            if (child->IsBusyRecursively()) {
                all_children_free = false;
                break;
            }
        }

        if (all_children_free) {
            int max_x = PosX + Width;
            int max_y = PosY + Height;

            for (auto&& child : Children) {
                max_x = std::max(max_x, child->PosX + child->Width);
                max_y = std::max(max_y, child->PosY + child->Height);
            }

            Width = max_x - PosX;
            Height = max_y - PosY;

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

auto TextureAtlasManager::CreateAtlas(AtlasType atlas_type, int request_width, int request_height) -> TextureAtlas*
{
    STACK_TRACE_ENTRY();

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
    RUNTIME_ASSERT(request_width > 0);
    RUNTIME_ASSERT(request_height > 0);

    auto result_width = request_width;
    auto result_height = request_height;

    auto&& atlas = std::make_unique<TextureAtlas>();
    atlas->Type = atlas_type;

    switch (atlas->Type) {
    case AtlasType::IfaceSprites:
        result_width = std::min(AppRender::MAX_ATLAS_WIDTH, 4096);
        result_height = std::min(AppRender::MAX_ATLAS_HEIGHT, 4096);
        break;
    case AtlasType::MapSprites:
        result_width = std::min(AppRender::MAX_ATLAS_WIDTH, 2048);
        result_height = std::min(AppRender::MAX_ATLAS_HEIGHT, 8192);
        break;
    case AtlasType::MeshTextures:
        result_width = std::min(AppRender::MAX_ATLAS_WIDTH, 1024);
        result_height = std::min(AppRender::MAX_ATLAS_HEIGHT, 2048);
        break;
    case AtlasType::OneImage:
        break;
    }

    atlas->RTarg = _rtMngr.CreateRenderTarget(false, RenderTarget::SizeType::Custom, result_width, result_height, _settings.AtlasLinearFiltration);
    atlas->MainTex = atlas->RTarg->MainTex.get();
    atlas->MainTex->FlippedHeight = false;
    atlas->Width = result_width;
    atlas->Height = result_height;
    atlas->RootNode = std::make_unique<TextureAtlas::SpaceNode>(nullptr, 0, 0, result_width, result_height);

    _allAtlases.push_back(std::move(atlas));
    return _allAtlases.back().get();
}

auto TextureAtlasManager::FindAtlasPlace(AtlasType atlas_type, int width, int height, int& x, int& y) -> pair<TextureAtlas*, TextureAtlas::SpaceNode*>
{
    STACK_TRACE_ENTRY();

    // Find place in already created atlas
    TextureAtlas* atlas = nullptr;
    TextureAtlas::SpaceNode* atlas_node = nullptr;

    const auto request_width = width + ATLAS_SPRITES_PADDING * 2;
    const auto request_height = height + ATLAS_SPRITES_PADDING * 2;

    if (atlas_type != AtlasType::OneImage) {
        for (auto&& a : _allAtlases) {
            if (a->Type != atlas_type) {
                continue;
            }

            auto* node = a->RootNode->FindPosition(request_width, request_height);
            if (node != nullptr) {
                atlas = a.get();
                atlas_node = node;
                break;
            }
        }
    }

    // Create new
    if (atlas == nullptr) {
        atlas = CreateAtlas(atlas_type, request_width, request_height);
        atlas_node = atlas->RootNode->FindPosition(request_width, request_height);
    }

    x = atlas_node->PosX + ATLAS_SPRITES_PADDING;
    y = atlas_node->PosY + ATLAS_SPRITES_PADDING;

    return {atlas, atlas_node};
}

void TextureAtlasManager::DumpAtlases() const
{
    STACK_TRACE_ENTRY();

    uint atlases_memory_size = 0;
    for (auto&& atlas : _allAtlases) {
        atlases_memory_size += atlas->Width * atlas->Height * 4;
    }

    const auto date = Timer::GetCurrentDateTime();
    const string dir = _str("{:04}.{:02}.{:02}_{:02}-{:02}-{:02}_{}.{:03}mb", //
        date.Year, date.Month, date.Day, date.Hour, date.Minute, date.Second, //
        atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000);

    auto cnt = 1;
    for (auto&& atlas : _allAtlases) {
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

        const string fname = _str("{}/{}_{}_{}x{}.tga", dir, atlas_type_name, cnt, atlas->Width, atlas->Height);
        auto tex_data = atlas->MainTex->GetTextureRegion(0, 0, atlas->Width, atlas->Height);
        GenericUtils::WriteSimpleTga(fname, atlas->Width, atlas->Height, std::move(tex_data));
        cnt++;
    }
}
