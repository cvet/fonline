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
    _layout {rt->GetSize()}
{
    FO_NO_STACK_TRACE_ENTRY();

    _rt->GetTexture()->FlippedHeight = false;
}

TextureAtlasLayout::TextureAtlasLayout(isize32 size) noexcept :
    _size {size}
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(size.width > 0, "Texture atlas layout width must be positive", size.width);
    FO_STRONG_ASSERT(size.height > 0, "Texture atlas layout height must be positive", size.height);

    _freeRectangles.emplace_back(ipos32 {}, size);
}

TextureAtlasLayout::Allocation::Allocation(ptr<TextureAtlasLayout> layout) noexcept :
    _layout {layout}
{
    FO_NO_STACK_TRACE_ENTRY();
}

auto TextureAtlasLayout::FindBestFitScore(isize32 size) -> optional<FitScore>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width > 0, "Texture atlas allocation width must be positive", size.width);
    FO_VERIFY_AND_THROW(size.height > 0, "Texture atlas allocation height must be positive", size.height);

    if (_freeRectanglesDirty) {
        RebuildFreeRectangles();
    }

    optional<Placement> placement = FindBestPlacement(size);
    return placement ? optional<FitScore> {placement->Score} : std::nullopt;
}

auto TextureAtlasLayout::Allocate(isize32 size) -> unique_del_nptr<Allocation>
{
    FO_STACK_TRACE_ENTRY();

    function<void(Allocation*)> free_allocation = [](Allocation* allocation) noexcept { allocation->Free(); };

    FO_VERIFY_AND_THROW(size.width > 0, "Texture atlas allocation width must be positive", size.width);
    FO_VERIFY_AND_THROW(size.height > 0, "Texture atlas allocation height must be positive", size.height);

    if (_freeRectanglesDirty) {
        RebuildFreeRectangles();
    }

    optional<Placement> placement = FindBestPlacement(size);

    if (!placement) {
        return {};
    }

    auto allocation = AcquireAllocation();
    SplitFreeRectangles(_freeRectangles, placement->Rectangle);

    allocation->_rectangle = placement->Rectangle;
    allocation->_spriteMesh = nullptr;
    allocation->_activeIndex = _activeAllocations.size();
    allocation->_active = true;
    _activeAllocations.emplace_back(allocation);
    _usedArea += GetArea(placement->Rectangle.size());
    return make_unique_del_ptr(allocation.as_nptr(), std::move(free_allocation));
}

void TextureAtlasLayout::Allocation::SetSpriteMesh(nptr<const SpriteMeshData> sprite_mesh) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_active, "Cannot attach sprite mesh metadata to an inactive atlas allocation");
    _spriteMesh = sprite_mesh;
}

void TextureAtlasLayout::Allocation::Free() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_active) {
        _layout->Release(this);
    }
}

void TextureAtlasLayout::DrawDumpOverlay(span<ucolor> pixels) const
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(pixels.size() == GetArea(_size), "Atlas dump pixel count does not match atlas dimensions", pixels.size(), _size);

    for (auto allocation : _activeAllocations) {
        DrawAllocationOverlay(*allocation, pixels);
    }
}

auto TextureAtlasLayout::FindBestPlacement(isize32 size) const noexcept -> optional<Placement>
{
    FO_NO_STACK_TRACE_ENTRY();

    optional<Placement> best_placement;

    for (irect32 free_rectangle : _freeRectangles) {
        if (free_rectangle.width < size.width || free_rectangle.height < size.height) {
            continue;
        }

        int32_t remaining_width = free_rectangle.width - size.width;
        int32_t remaining_height = free_rectangle.height - size.height;
        Placement candidate {
            .Rectangle = {free_rectangle.pos(), size},
            .Score =
                {
                    .ShortSideFit = std::min(remaining_width, remaining_height),
                    .LongSideFit = std::max(remaining_width, remaining_height),
                    .AreaWaste = GetArea(free_rectangle.size()) - GetArea(size),
                    .Y = free_rectangle.y,
                    .X = free_rectangle.x,
                    .FreeWidth = free_rectangle.width,
                    .FreeHeight = free_rectangle.height,
                },
        };

        auto candidate_score = std::tie(candidate.Score.ShortSideFit, candidate.Score.LongSideFit, candidate.Score.AreaWaste, candidate.Score.Y, candidate.Score.X, candidate.Score.FreeWidth, candidate.Score.FreeHeight);

        if (!best_placement) {
            best_placement = candidate;
        }
        else {
            auto best_score = std::tie(best_placement->Score.ShortSideFit, best_placement->Score.LongSideFit, best_placement->Score.AreaWaste, best_placement->Score.Y, best_placement->Score.X, best_placement->Score.FreeWidth, best_placement->Score.FreeHeight);

            if (candidate_score < best_score) {
                best_placement = candidate;
            }
        }
    }

    return best_placement;
}

auto TextureAtlasLayout::AcquireAllocation() -> ptr<Allocation>
{
    FO_STACK_TRACE_ENTRY();

    if (!_availableAllocations.empty()) {
        auto allocation = _availableAllocations.back();
        _availableAllocations.pop_back();
        return allocation;
    }

    _allocations.emplace_back(SafeAlloc::MakeUnique<Allocation>(this));
    return _allocations.back();
}

void TextureAtlasLayout::Release(ptr<Allocation> allocation) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(allocation->_layout == this, "Atlas allocation belongs to another layout");
    FO_STRONG_ASSERT(allocation->_active, "Atlas allocation is already inactive");
    FO_STRONG_ASSERT(allocation->_activeIndex < _activeAllocations.size() && _activeAllocations[allocation->_activeIndex] == allocation, "Atlas active allocation index is inconsistent", allocation->_activeIndex, _activeAllocations.size());
    size_t allocation_area = GetArea(allocation->_rectangle.size());
    FO_STRONG_ASSERT(_usedArea >= allocation_area, "Atlas used area underflow", _usedArea, allocation->_rectangle);

    size_t released_index = allocation->_activeIndex;
    auto moved_allocation = _activeAllocations.back();
    _activeAllocations[released_index] = moved_allocation;
    moved_allocation->_activeIndex = released_index;
    _activeAllocations.pop_back();

    _usedArea -= allocation_area;
    allocation->_active = false;
    allocation->_spriteMesh = nullptr;
    _availableAllocations.emplace_back(allocation);
    _freeRectanglesDirty = true;
}

void TextureAtlasLayout::RebuildFreeRectangles()
{
    FO_STACK_TRACE_ENTRY();

    vector<irect32> rebuilt_free_rectangles;
    rebuilt_free_rectangles.emplace_back(ipos32 {}, _size);

    vector<irect32> used_rectangles;
    used_rectangles.reserve(_activeAllocations.size());

    for (auto allocation : _activeAllocations) {
        used_rectangles.emplace_back(allocation->_rectangle);
    }

    std::sort(used_rectangles.begin(), used_rectangles.end());

    for (irect32 used_rectangle : used_rectangles) {
        SplitFreeRectangles(rebuilt_free_rectangles, used_rectangle);
    }

    _freeRectangles = std::move(rebuilt_free_rectangles);
    _freeRectanglesDirty = false;
}

void TextureAtlasLayout::SplitFreeRectangles(vector<irect32>& free_rectangles, irect32 used_rectangle)
{
    FO_STACK_TRACE_ENTRY();

    vector<irect32> split_rectangles;
    split_rectangles.reserve(free_rectangles.size() * 2);

    for (irect32 free_rectangle : free_rectangles) {
        if (!Intersects(free_rectangle, used_rectangle)) {
            split_rectangles.emplace_back(free_rectangle);
            continue;
        }

        int32_t free_right = free_rectangle.x + free_rectangle.width;
        int32_t free_bottom = free_rectangle.y + free_rectangle.height;
        int32_t used_right = used_rectangle.x + used_rectangle.width;
        int32_t used_bottom = used_rectangle.y + used_rectangle.height;

        if (used_rectangle.y > free_rectangle.y && used_rectangle.y < free_bottom) {
            split_rectangles.emplace_back(free_rectangle.x, free_rectangle.y, free_rectangle.width, used_rectangle.y - free_rectangle.y);
        }
        if (used_bottom > free_rectangle.y && used_bottom < free_bottom) {
            split_rectangles.emplace_back(free_rectangle.x, used_bottom, free_rectangle.width, free_bottom - used_bottom);
        }
        if (used_rectangle.x > free_rectangle.x && used_rectangle.x < free_right) {
            split_rectangles.emplace_back(free_rectangle.x, free_rectangle.y, used_rectangle.x - free_rectangle.x, free_rectangle.height);
        }
        if (used_right > free_rectangle.x && used_right < free_right) {
            split_rectangles.emplace_back(used_right, free_rectangle.y, free_right - used_right, free_rectangle.height);
        }
    }

    free_rectangles = std::move(split_rectangles);
    PruneFreeRectangles(free_rectangles);
}

void TextureAtlasLayout::PruneFreeRectangles(vector<irect32>& free_rectangles)
{
    FO_STACK_TRACE_ENTRY();

    vector<irect32> pruned_rectangles;
    pruned_rectangles.reserve(free_rectangles.size());

    for (size_t i = 0; i < free_rectangles.size(); i++) {
        bool contained = false;

        for (size_t j = 0; j < free_rectangles.size(); j++) {
            if (i == j) {
                continue;
            }

            bool duplicate_with_earlier = free_rectangles[i] == free_rectangles[j] && j < i;

            if (Contains(free_rectangles[j], free_rectangles[i]) && (free_rectangles[i] != free_rectangles[j] || duplicate_with_earlier)) {
                contained = true;
                break;
            }
        }

        if (!contained) {
            pruned_rectangles.emplace_back(free_rectangles[i]);
        }
    }

    free_rectangles = std::move(pruned_rectangles);
}

void TextureAtlasLayout::DrawAllocationOverlay(const Allocation& allocation, span<ucolor> pixels) const
{
    FO_STACK_TRACE_ENTRY();

    ipos32 allocation_pos = allocation.GetPosition();
    isize32 allocation_size = allocation.GetSize();
    ipos32 sprite_origin = {allocation_pos.x + ATLAS_SPRITES_PADDING, allocation_pos.y + ATLAS_SPRITES_PADDING};
    ipos32 sprite_end = {allocation_pos.x + allocation_size.width - ATLAS_SPRITES_PADDING, allocation_pos.y + allocation_size.height - ATLAS_SPRITES_PADDING};
    auto sprite_mesh = allocation.GetSpriteMesh();

    if (!sprite_mesh) {
        DrawAtlasDumpLine(pixels, _size, sprite_origin, {sprite_end.x, sprite_origin.y}, ATLAS_DUMP_QUAD_COLOR);
        DrawAtlasDumpLine(pixels, _size, {sprite_end.x, sprite_origin.y}, sprite_end, ATLAS_DUMP_QUAD_COLOR);
        DrawAtlasDumpLine(pixels, _size, sprite_end, {sprite_origin.x, sprite_end.y}, ATLAS_DUMP_QUAD_COLOR);
        DrawAtlasDumpLine(pixels, _size, {sprite_origin.x, sprite_end.y}, sprite_origin, ATLAS_DUMP_QUAD_COLOR);
    }
    else if (sprite_mesh->Indices.empty()) {
        DrawAtlasDumpLine(pixels, _size, sprite_origin, sprite_end, ATLAS_DUMP_EMPTY_COLOR);
        DrawAtlasDumpLine(pixels, _size, {sprite_end.x, sprite_origin.y}, {sprite_origin.x, sprite_end.y}, ATLAS_DUMP_EMPTY_COLOR);
    }
    else {
        for (size_t i = 0; i + 2 < sprite_mesh->Indices.size(); i += 3) {
            uint16_t index_a = sprite_mesh->Indices[i];
            uint16_t index_b = sprite_mesh->Indices[i + 1];
            uint16_t index_c = sprite_mesh->Indices[i + 2];

            if (index_a >= sprite_mesh->Vertices.size() || index_b >= sprite_mesh->Vertices.size() || index_c >= sprite_mesh->Vertices.size()) {
                continue;
            }

            ipos32 a = sprite_origin + sprite_mesh->Vertices[index_a];
            ipos32 b = sprite_origin + sprite_mesh->Vertices[index_b];
            ipos32 c = sprite_origin + sprite_mesh->Vertices[index_c];
            DrawAtlasDumpLine(pixels, _size, a, b, ATLAS_DUMP_MESH_COLOR);
            DrawAtlasDumpLine(pixels, _size, b, c, ATLAS_DUMP_MESH_COLOR);
            DrawAtlasDumpLine(pixels, _size, c, a, ATLAS_DUMP_MESH_COLOR);
        }

        for (ipos32 vertex : sprite_mesh->Vertices) {
            ipos32 vertex_pos = sprite_origin + vertex;

            if (_size.is_valid_pos(vertex_pos)) {
                pixels[numeric_cast<size_t>(vertex_pos.y) * _size.width + vertex_pos.x] = ATLAS_DUMP_VERTEX_COLOR;
            }
        }
    }
}

void TextureAtlasLayout::DrawAtlasDumpLine(span<ucolor> pixels, isize32 atlas_size, ipos32 from, ipos32 to, ucolor color) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    int32_t dx = std::abs(to.x - from.x);
    int32_t sx = from.x < to.x ? 1 : -1;
    int32_t dy = -std::abs(to.y - from.y);
    int32_t sy = from.y < to.y ? 1 : -1;
    int32_t error = dx + dy;

    while (true) {
        if (atlas_size.is_valid_pos(from)) {
            pixels[numeric_cast<size_t>(from.y) * atlas_size.width + from.x] = color;
        }

        if (from == to) {
            break;
        }

        int32_t double_error = error * 2;

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

auto TextureAtlasLayout::Intersects(irect32 first, irect32 second) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return first.x < second.x + second.width && first.x + first.width > second.x && first.y < second.y + second.height && first.y + first.height > second.y;
}

auto TextureAtlasLayout::Contains(irect32 outer, irect32 inner) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return outer.x <= inner.x && outer.y <= inner.y && outer.x + outer.width >= inner.x + inner.width && outer.y + outer.height >= inner.y + inner.height;
}

auto TextureAtlasLayout::GetArea(isize32 size) noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
}

TextureAtlasManager::TextureAtlasManager(ptr<RenderSettings> settings, ptr<RenderTargetManager> rt_mngr) :
    _settings {settings},
    _rtMngr {rt_mngr}
{
    FO_NO_STACK_TRACE_ENTRY();
}

auto TextureAtlasManager::CreateAtlas(AtlasType atlas_type, isize32 request_size) -> ptr<TextureAtlas>
{
    FO_STACK_TRACE_ENTRY();

    // Cleanup expired atlases
    for (auto it = _allAtlases.begin(); it != _allAtlases.end();) {
        if (it->get()->GetType() == AtlasType::OneImage && it->get()->GetLayout()->IsEmpty()) {
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

auto TextureAtlasManager::FindAtlasPlace(AtlasType atlas_type, isize32 size) -> tuple<ptr<TextureAtlas>, unique_del_ptr<TextureAtlasLayout::Allocation>, ipos32>
{
    FO_STACK_TRACE_ENTRY();

    nptr<TextureAtlas> atlas {};
    unique_del_nptr<TextureAtlasLayout::Allocation> atlas_allocation {};
    optional<TextureAtlasLayout::FitScore> best_fit;

    FO_VERIFY_AND_THROW(size.width > 0, "Requested atlas allocation width must be positive", size.width);
    FO_VERIFY_AND_THROW(size.height > 0, "Requested atlas allocation height must be positive", size.height);
    FO_VERIFY_AND_THROW(size.width <= std::numeric_limits<int32_t>::max() - ATLAS_SPRITES_PADDING * 2, "Requested atlas allocation width overflows padding", size.width);
    FO_VERIFY_AND_THROW(size.height <= std::numeric_limits<int32_t>::max() - ATLAS_SPRITES_PADDING * 2, "Requested atlas allocation height overflows padding", size.height);

    isize32 size_with_padding = {size.width + ATLAS_SPRITES_PADDING * 2, size.height + ATLAS_SPRITES_PADDING * 2};

    if (atlas_type != AtlasType::OneImage) {
        for (auto& check_atlas : _allAtlases) {
            if (check_atlas->GetType() != atlas_type) {
                continue;
            }

            optional<TextureAtlasLayout::FitScore> check_fit = check_atlas->GetLayout()->FindBestFitScore(size_with_padding);

            if (!check_fit) {
                continue;
            }

            auto check_score = std::tie(check_fit->ShortSideFit, check_fit->LongSideFit, check_fit->AreaWaste);

            if (!best_fit) {
                atlas = check_atlas;
                best_fit = check_fit;
            }
            else {
                auto best_score = std::tie(best_fit->ShortSideFit, best_fit->LongSideFit, best_fit->AreaWaste);

                if (check_score < best_score) {
                    atlas = check_atlas;
                    best_fit = check_fit;
                }
            }
        }

        if (atlas) {
            atlas_allocation = atlas->GetLayout()->Allocate(size_with_padding);
            FO_VERIFY_AND_THROW(atlas_allocation, "Evaluated atlas placement is no longer available");
        }
    }

    // Create new
    if (!atlas) {
        auto new_atlas = CreateAtlas(atlas_type, size_with_padding);
        atlas = new_atlas;
        atlas_allocation = new_atlas->GetLayout()->Allocate(size_with_padding);
        FO_VERIFY_AND_THROW(atlas_allocation, "Missing required atlas allocation");
    }

    FO_VERIFY_AND_THROW(atlas, "Atlas placement is missing its atlas");
    FO_VERIFY_AND_THROW(atlas_allocation, "Atlas placement is missing its allocation");

    ipos32 allocation_pos = atlas_allocation->GetPosition();
    ipos32 pos = {allocation_pos.x + ATLAS_SPRITES_PADDING, allocation_pos.y + ATLAS_SPRITES_PADDING};

    return {atlas, take_not_null(atlas_allocation), pos};
}

void TextureAtlasManager::DumpAtlases() const
{
    FO_STACK_TRACE_ENTRY();

    size_t atlases_memory_size = 0;

    for (size_t i = 0; i < _allAtlases.size(); i++) {
        isize32 atlas_size = _allAtlases[i]->GetSize();
        atlases_memory_size += numeric_cast<size_t>(atlas_size.width) * numeric_cast<size_t>(atlas_size.height) * sizeof(ucolor);
    }

    time_desc_t time = nanotime::now().desc(true);
    string dir = strex("TexDump_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}_{}.{:03}mb", //
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

        string fname = strex("{}/{}_{}_{}x{}.tga", dir, atlas_type_name, count, atlas->GetSize().width, atlas->GetSize().height);
        auto tex_data = atlas->GetTexture()->GetTextureRegion({0, 0}, atlas->GetSize());
        atlas->GetLayout()->DrawDumpOverlay(tex_data);
        WriteSimpleTga(fname, atlas->GetSize(), std::move(tex_data));
        count++;
    }
}

FO_END_NAMESPACE
