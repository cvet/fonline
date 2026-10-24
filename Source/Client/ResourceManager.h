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

#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

class SpriteSheet;

class SpriteFactory
{
public:
    SpriteFactory() = default;
    SpriteFactory(const SpriteFactory&) = delete;
    SpriteFactory(SpriteFactory&&) noexcept = default;
    auto operator=(const SpriteFactory&) = delete;
    auto operator=(SpriteFactory&&) noexcept -> SpriteFactory& = delete;
    virtual ~SpriteFactory() = default;

    [[nodiscard]] virtual auto GetExtensions() const -> vector<string> = 0;

    virtual auto LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite> = 0;
    virtual void Update() { }
    virtual void RetryFailedLoads() { }
    virtual void InvalidateResource(hstring path) { ignore_unused(path); }
    virtual void ClenupCache() { }
};

class ResourceManager final
{
public:
    ResourceManager() = delete;
    ResourceManager(ptr<FileSystem> resources, ptr<SpriteManager> spr_mngr);
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) noexcept = delete;
    auto operator=(const ResourceManager&) = delete;
    auto operator=(ResourceManager&&) noexcept = delete;
    ~ResourceManager() = default;

    [[nodiscard]] auto GetResources() noexcept -> ptr<FileSystem> { return _resources; }
    [[nodiscard]] auto GetItemDefaultSpr() -> shared_ptr<Sprite>;
    [[nodiscard]] auto GetCritterDummyFrames() -> ptr<const SpriteSheet>;
    [[nodiscard]] auto GetSpriteFactory(std::type_index ti) -> nptr<SpriteFactory>;

    void RegisterSpriteFactory(unique_ptr<SpriteFactory> factory);
    auto LoadSprite(string_view path, AtlasType atlas_type, bool no_warn_if_not_exists = false) -> shared_ptr<Sprite>;
    auto LoadSprite(hstring path, AtlasType atlas_type, bool no_warn_if_not_exists = false) -> shared_ptr<Sprite>;
    auto LoadSpriteAsQuad(hstring path, AtlasType atlas_type) -> shared_ptr<AtlasSprite>;
    void LoadDefaultSprites(const RenderSettings& settings);
    void ForgetFailedSprite(string_view path);
    void InvalidateSpriteResource(string_view path);
    void RetryFailedSpriteLoads();
    void CleanupSpriteCache();
    void UnsubscribeEvents() noexcept;

private:
    void UpdateFactories();

    ptr<FileSystem> _resources;
    ptr<SpriteManager> _sprMngr;
    vector<unique_ptr<SpriteFactory>> _spriteFactories {};
    unordered_map<string, ptr<SpriteFactory>> _spriteFactoryMap {};
    unordered_set<hstring> _nonFoundSprites {};
    unordered_map<pair<hstring, AtlasType>, shared_ptr<Sprite>> _copyableSpriteCache {};
    shared_ptr<SpriteSheet> _critterDummyAnimFrames {};
    shared_ptr<Sprite> _itemHexDummyAnim {};
    EventUnsubscriber _eventUnsubscriber {};
};

FO_END_NAMESPACE
