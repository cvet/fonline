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

FO_BEGIN_NAMESPACE

enum class AtlasType : uint8_t
{
    IfaceSprites,
    MapSprites,
    MeshTextures,
    OneImage,
};

class TextureAtlas final
{
public:
    class SpaceNode final
    {
    public:
        SpaceNode(nptr<SpaceNode> parent, ipos32 pos, isize32 size);
        SpaceNode(const SpaceNode&) = delete;
        SpaceNode(SpaceNode&&) noexcept = default;
        auto operator=(const SpaceNode&) = delete;
        auto operator=(SpaceNode&&) noexcept -> SpaceNode& = default;
        ~SpaceNode() = default;

        [[nodiscard]] auto IsBusyRecursively() const noexcept -> bool;

        auto FindPosition(isize32 size) -> nptr<SpaceNode>;
        void Free() noexcept;

        nptr<SpaceNode> Parent {};
        ipos32 Pos {};
        isize32 Size {};
        bool Busy {};
        vector<unique_ptr<SpaceNode>> Children {};
    };

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
    [[nodiscard]] auto GetLayout() noexcept -> ptr<SpaceNode> { return &_rootNode; }

private:
    AtlasType _type;
    ptr<RenderTarget> _rt;
    SpaceNode _rootNode;
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

    auto CreateAtlas(AtlasType atlas_type, isize32 request_size) -> ptr<TextureAtlas>;
    auto FindAtlasPlace(AtlasType atlas_type, isize32 size) -> tuple<ptr<TextureAtlas>, ptr<TextureAtlas::SpaceNode>, ipos32>;
    void DumpAtlases() const;

private:
    ptr<RenderSettings> _settings;
    ptr<RenderTargetManager> _rtMngr;
    vector<unique_ptr<TextureAtlas>> _allAtlases {};
};

FO_END_NAMESPACE
