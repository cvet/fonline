// FOnline Engine
// SPDX-License-Identifier: MIT

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
