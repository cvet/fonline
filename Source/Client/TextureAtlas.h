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

#pragma once

#include "Common.h"
#include "RenderTarget.h"
#include "Rendering.h"
#include "SpriteResource.h"

FO_BEGIN_NAMESPACE

enum class AtlasType : uint8_t
{
    IfaceSprites,
    MapSprites,
    MeshTextures,
    OneImage,
};

class TextureAtlasLayout final
{
public:
    struct FitScore
    {
        int32_t ShortSideFit {};
        int32_t LongSideFit {};
        size_t AreaWaste {};
        int32_t Y {};
        int32_t X {};
        int32_t FreeWidth {};
        int32_t FreeHeight {};
    };

    class Allocation final
    {
        friend class SafeAlloc;
        friend class TextureAtlasLayout;

    public:
        Allocation(const Allocation&) = delete;
        Allocation(Allocation&&) noexcept = delete;
        auto operator=(const Allocation&) = delete;
        auto operator=(Allocation&&) noexcept -> Allocation& = delete;
        ~Allocation() = default;

        [[nodiscard]] auto GetPosition() const noexcept -> ipos32 { return _rectangle.pos(); }
        [[nodiscard]] auto GetSize() const noexcept -> isize32 { return _rectangle.size(); }
        [[nodiscard]] auto IsActive() const noexcept -> bool { return _active; }
        [[nodiscard]] auto GetSpriteMesh() const noexcept -> nptr<const SpriteMeshData> { return _spriteMesh; }

        void SetSpriteMesh(nptr<const SpriteMeshData> sprite_mesh) noexcept;

    private:
        explicit Allocation(ptr<TextureAtlasLayout> layout) noexcept;
        void Free() noexcept;

        ptr<TextureAtlasLayout> _layout;
        irect32 _rectangle {};
        nptr<const SpriteMeshData> _spriteMesh {};
        size_t _activeIndex {};
        bool _active {};
    };

    explicit TextureAtlasLayout(isize32 size) noexcept;
    TextureAtlasLayout(const TextureAtlasLayout&) = delete;
    TextureAtlasLayout(TextureAtlasLayout&&) noexcept = delete;
    auto operator=(const TextureAtlasLayout&) = delete;
    auto operator=(TextureAtlasLayout&&) noexcept -> TextureAtlasLayout& = delete;
    ~TextureAtlasLayout() = default;

    [[nodiscard]] auto GetSize() const noexcept -> isize32 { return _size; }
    [[nodiscard]] auto IsEmpty() const noexcept -> bool { return _usedArea == 0; }
    [[nodiscard]] auto GetUsedArea() const noexcept -> size_t { return _usedArea; }

    auto FindBestFitScore(isize32 size) -> optional<FitScore>;
    auto Allocate(isize32 size) -> unique_del_nptr<Allocation>;
    void DrawDumpOverlay(span<ucolor> pixels) const;

private:
    struct Placement
    {
        irect32 Rectangle {};
        FitScore Score {};
    };

    auto FindBestPlacement(isize32 size) const noexcept -> optional<Placement>;
    auto AcquireAllocation() -> ptr<Allocation>;
    void Release(ptr<Allocation> allocation) noexcept;
    void RebuildFreeRectangles();
    void DrawAllocationOverlay(const Allocation& allocation, span<ucolor> pixels) const;

    static void SplitFreeRectangles(vector<irect32>& free_rectangles, irect32 used_rectangle);
    static void PruneFreeRectangles(vector<irect32>& free_rectangles);
    static void DrawAtlasDumpLine(span<ucolor> pixels, isize32 atlas_size, ipos32 from, ipos32 to, ucolor color) noexcept;
    static auto Intersects(irect32 first, irect32 second) noexcept -> bool;
    static auto Contains(irect32 outer, irect32 inner) noexcept -> bool;
    static auto GetArea(isize32 size) noexcept -> size_t;

    isize32 _size;
    vector<irect32> _freeRectangles {};
    vector<unique_ptr<Allocation>> _allocations {};
    vector<ptr<Allocation>> _activeAllocations {};
    vector<ptr<Allocation>> _availableAllocations {};
    size_t _usedArea {};
    bool _freeRectanglesDirty {};
};

class TextureAtlas final
{
public:
    explicit TextureAtlas(AtlasType type, ptr<RenderTarget> rt) noexcept;
    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas(TextureAtlas&&) noexcept = delete;
    auto operator=(const TextureAtlas&) = delete;
    auto operator=(TextureAtlas&&) noexcept -> TextureAtlas& = delete;
    ~TextureAtlas() = default;

    [[nodiscard]] auto GetType() const noexcept -> AtlasType { return _type; }
    [[nodiscard]] auto GetSize() const noexcept -> isize32 { return _rt->GetSize(); }
    [[nodiscard]] auto GetRenderTarget() const noexcept -> ptr<const RenderTarget> { return _rt; }
    [[nodiscard]] auto GetRenderTarget() noexcept -> ptr<RenderTarget> { return _rt; }
    [[nodiscard]] auto GetTexture() const noexcept -> ptr<const RenderTexture> { return _rt->GetTexture(); }
    [[nodiscard]] auto GetTexture() noexcept -> ptr<RenderTexture> { return _rt->GetTexture(); }
    [[nodiscard]] auto GetLayout() const noexcept -> ptr<const TextureAtlasLayout> { return &_layout; }
    [[nodiscard]] auto GetLayout() noexcept -> ptr<TextureAtlasLayout> { return &_layout; }

private:
    AtlasType _type;
    ptr<RenderTarget> _rt;
    TextureAtlasLayout _layout;
};

class TextureAtlasManager
{
public:
    TextureAtlasManager(ptr<RenderSettings> settings, ptr<RenderTargetManager> rt_mngr);
    TextureAtlasManager(const TextureAtlasManager&) = delete;
    TextureAtlasManager(TextureAtlasManager&&) noexcept = default;
    auto operator=(const TextureAtlasManager&) = delete;
    auto operator=(TextureAtlasManager&&) noexcept -> TextureAtlasManager& = delete;
    ~TextureAtlasManager() = default;

    auto FindAtlasPlace(AtlasType atlas_type, isize32 size) -> tuple<ptr<TextureAtlas>, unique_del_ptr<TextureAtlasLayout::Allocation>, ipos32>;
    void DumpAtlases() const;

private:
    auto CreateAtlas(AtlasType atlas_type, isize32 request_size) -> ptr<TextureAtlas>;

    ptr<RenderSettings> _settings;
    ptr<RenderTargetManager> _rtMngr;
    vector<unique_ptr<TextureAtlas>> _allAtlases {};
};

FO_END_NAMESPACE
