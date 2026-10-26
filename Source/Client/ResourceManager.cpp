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

#include "ResourceManager.h"

#include "Application.h"
#include "DefaultSprites.h"

FO_BEGIN_NAMESPACE

static constexpr timespan DUMMY_ANIMATION_FRAME_DURATION {std::chrono::milliseconds {100}};

ResourceManager::ResourceManager(ptr<FileSystem> resources, ptr<SpriteManager> spr_mngr, nptr<WorkScheduler> work_scheduler, ResourcePreparation::Decoder resource_decoder) :
    _resources {resources},
    _sprMngr {spr_mngr},
    _workScheduler {work_scheduler},
    _resourceDecoder {std::move(resource_decoder)}
{
    _eventUnsubscriber += (*_sprMngr->GetOnBeginScene()) += [this]() FO_DEFERRED { UpdateFactories(); };
    _eventUnsubscriber += (*_sprMngr->GetWindow()->GetOnLowMemory()) += [this]() FO_DEFERRED { CleanupSpriteCache(); };
}

ResourceManager::~ResourceManager()
{
    ShutdownResourcePreparation();
}

void ResourceManager::UnsubscribeEvents() noexcept
{
    _eventUnsubscriber.Unsubscribe();
}

void ResourceManager::UpdateFactories()
{
    for (size_t i = 0; i < _spriteFactories.size(); ++i) {
        _spriteFactories[i]->Update();
    }
}

void ResourceManager::RegisterSpriteFactory(unique_ptr<SpriteFactory> factory)
{
    DiscardPreparedSpriteResources();

    for (const auto& ext : factory->GetExtensions()) {
        _spriteFactoryMap.insert_or_assign(ext, factory);
    }

    _spriteFactories.emplace_back(std::move(factory));
}

auto ResourceManager::GetSpriteFactory(std::type_index ti) -> nptr<SpriteFactory>
{
    for (size_t i = 0; i != _spriteFactories.size(); ++i) {
        auto factory = _spriteFactories[i].as_ptr();
        SpriteFactory& factory_ref = *factory;

        if (std::type_index(typeid(factory_ref)) == ti) {
            return factory;
        }
    }

    return nullptr;
}

auto ResourceManager::LoadSpriteAsQuad(hstring path, AtlasType atlas_type) -> shared_ptr<AtlasSprite>
{
    auto factory = GetSpriteFactory(typeid(DefaultSpriteFactory)).dyn_cast<DefaultSpriteFactory>();
    FO_VERIFY_AND_THROW(factory, "Default sprite factory is not registered", path);
    return factory->LoadSpriteAsQuad(path, atlas_type);
}

void ResourceManager::LoadDefaultSprites(const RenderSettings& settings)
{
    FO_TRACE_ZONE(Render);

    auto any_spr = !settings.Render.CritterStubSpriteName.empty() ? LoadSprite(settings.Render.CritterStubSpriteName, AtlasType::MapSprites, true) : shared_ptr<Sprite> {};

    if (!any_spr) {
        any_spr = DefaultSpriteFactory::CreateDummySprite(_sprMngr, AtlasType::MapSprites);
    }

    auto atlas_spr = any_spr.dyn_cast<AtlasSprite>();
    FO_VERIFY_AND_THROW(atlas_spr, "Missing required atlas sprite");
    _critterDummyAnimFrames = safe_alloc::make_shared<SpriteSheet>(_sprMngr, 1, DUMMY_ANIMATION_FRAME_DURATION, 1);
    ipos32 dummy_offset = atlas_spr->GetOffset();

    _critterDummyAnimFrames->SetFrame(0, std::move(atlas_spr), dummy_offset, ipos32 {});
    FO_VERIFY_AND_THROW(_critterDummyAnimFrames, "Critter dummy animation frames are null");

    _itemHexDummyAnim = !settings.Render.ItemStubSpriteName.empty() ? LoadSprite(settings.Render.ItemStubSpriteName, AtlasType::MapSprites, true) : nullptr;

    if (!_itemHexDummyAnim) {
        _itemHexDummyAnim = DefaultSpriteFactory::CreateDummySprite(_sprMngr, AtlasType::MapSprites);
    }

    FO_VERIFY_AND_THROW(_itemHexDummyAnim, "Item hex dummy animation is null");
}

auto ResourceManager::GetItemDefaultSpr() -> shared_ptr<Sprite>
{
    return _itemHexDummyAnim;
}

auto ResourceManager::GetCritterDummyFrames() -> ptr<const SpriteSheet>
{
    return _critterDummyAnimFrames;
}

auto ResourceManager::CanPrepareSpriteResources() const -> bool
{
    return !_resourcePreparationStopped && _workScheduler && _workScheduler->IsParallel();
}

auto ResourceManager::GetResourcePreparationDiagnostics() const -> ResourcePreparation::Diagnostics
{
    return _resourcePreparation ? _resourcePreparation->GetDiagnostics() : ResourcePreparation::Diagnostics {};
}

auto ResourceManager::PrefetchSpriteResource(hstring path, AtlasType atlas_type, bool as_quad) -> bool
{
    FO_TRACE_ZONE(Render);

    if (!CanPrepareSpriteResources() || !path || path.as_str().size() > ResourcePreparation::Limits {}.PathBytes) {
        return false;
    }

    if (!as_quad && (_copyableSpriteCache.count({path, atlas_type}) != 0 || _nonFoundSprites.count(path) != 0)) {
        return false;
    }

    if (as_quad) {
        if (!GetSpriteFactory(typeid(DefaultSpriteFactory))) {
            return false;
        }
    }
    else {
        auto factory = _spriteFactoryMap.find(strex(path).get_file_extension());

        if (factory == _spriteFactoryMap.end() || typeid(*factory->second) != typeid(DefaultSpriteFactory)) {
            return false;
        }
    }

    if (!_resourcePreparation) {
        _resourcePreparation = safe_alloc::make_unique<ResourcePreparation>(_workScheduler, ResourcePreparation::Limits {}, std::move(_resourceDecoder));
    }

    ResourcePreparation::Key key {string(path.as_str()), as_quad ? ResourcePreparation::Kind::Quad : ResourcePreparation::Kind::Sprite, static_cast<uint32_t>(atlas_type), _resourceGeneration, _resourceEpoch};

    if (_resourcePreparation->FindRequest(key) != 0) {
        return true;
    }

    if (!_resourcePreparation->HasAvailableRequestSlot()) {
        return false;
    }

    File file;
    std::exception_ptr error;

    try {
        auto header = _resources->ReadFileHeader(path.as_str());

        if (header && header.GetSize() > ResourcePreparation::Limits {}.EncodedBytes) {
            return false;
        }

        file = _resources->ReadFile(path);
    }
    catch (const std::exception& ex) {
        ignore_unused(ex);
        error = std::current_exception();
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }

    ResourcePreparation::RequestResult result;

    if (file && !error) {
        result = _resourcePreparation->Request(std::move(key), file.GetDataSpan());
    }
    else {
        ResourcePreparation::Input input;
        input.Identity = std::move(key);
        input.Missing = !error;
        input.AcquisitionError = std::move(error);
        result = _resourcePreparation->Request(std::move(input));
    }

    return result.Status == ResourcePreparation::Admission::Accepted || result.Status == ResourcePreparation::Admission::Coalesced;
}

auto ResourceManager::ConsumePreparedSpriteResource(hstring path, AtlasType atlas_type, bool as_quad, const function<void(SpriteResourceData&&)>& publish) -> bool
{
    if (!_resourcePreparation || _resourcePreparationStopped) {
        return false;
    }

    ResourcePreparation::Key key {string(path.as_str()), as_quad ? ResourcePreparation::Kind::Quad : ResourcePreparation::Kind::Sprite, static_cast<uint32_t>(atlas_type), _resourceGeneration, _resourceEpoch};
    uint64_t ticket = _resourcePreparation->FindRequest(key);
    return ticket != 0 && _resourcePreparation->Consume(ticket, publish);
}

void ResourceManager::DrainPreparedSpriteResources()
{
    if (_resourcePreparation) {
        _resourcePreparation->Drain();
    }
}

void ResourceManager::DiscardPreparedSpriteResources()
{
    FO_VERIFY_AND_THROW(_resourceEpoch != std::numeric_limits<uint64_t>::max(), "Sprite preparation epoch exhausted");
    _resourceEpoch++;

    if (_resourcePreparation) {
        _resourcePreparation->CancelAll();
    }
}

void ResourceManager::AdvanceResourcePreparationGeneration()
{
    FO_VERIFY_AND_THROW(_resourceGeneration != std::numeric_limits<uint64_t>::max(), "Sprite preparation generation exhausted");
    uint64_t generation = _resourceGeneration++;

    if (_resourcePreparation) {
        _resourcePreparation->CancelGeneration(generation);
    }
}

void ResourceManager::ShutdownResourcePreparation()
{
    _resourcePreparationStopped = true;

    if (_resourcePreparation) {
        _resourcePreparation->Shutdown();
    }
}

auto ResourceManager::LoadSprite(string_view path, AtlasType atlas_type, bool no_warn_if_not_exists) -> shared_ptr<Sprite>
{
    return LoadSprite(_sprMngr->ToHashedString(path), atlas_type, no_warn_if_not_exists);
}

auto ResourceManager::LoadSprite(hstring path, AtlasType atlas_type, bool no_warn_if_not_exists) -> shared_ptr<Sprite>
{
    FO_TRACE_ZONE(Render);

    if (!path) {
        return nullptr;
    }

    if (auto it = _copyableSpriteCache.find({path, atlas_type}); it != _copyableSpriteCache.end()) {
        return it->second->MakeCopy();
    }

    if (_nonFoundSprites.count(path) != 0) {
        return nullptr;
    }

    string ext = strex(path).get_file_extension();

    if (ext.empty()) {
        break_into_debugger();
        logging::write(logging::type::warning, "Extension not found, file '{}'", path);
        _nonFoundSprites.emplace(path);
        return nullptr;
    }

    auto it = _spriteFactoryMap.find(ext);

    if (it == _spriteFactoryMap.end()) {
        break_into_debugger();
        logging::write(logging::type::warning, "Unknown extension, file '{}'", path);
        _nonFoundSprites.emplace(path);
        return nullptr;
    }

    auto spr = it->second->LoadSprite(path, atlas_type);

    if (!spr) {
        if (!no_warn_if_not_exists) {
            break_into_debugger();
            logging::write(logging::type::warning, "Sprite not found: '{}'", path);
        }

        _nonFoundSprites.emplace(path);
        return nullptr;
    }

    if (spr->IsCopyable()) {
        auto copy = spr->MakeCopy();
        _copyableSpriteCache.emplace(pair {path, atlas_type}, spr);
        return copy;
    }

    return spr;
}

void ResourceManager::ForgetFailedSprite(string_view path)
{
    _nonFoundSprites.erase(_sprMngr->ToHashedString(path));
}

void ResourceManager::InvalidateSpriteResource(string_view path)
{
    FO_TRACE_ZONE(Render);

    DiscardPreparedSpriteResources();

    hstring hashed_path = _sprMngr->ToHashedString(path);
    _nonFoundSprites.erase(hashed_path);

    for (auto it = _copyableSpriteCache.begin(); it != _copyableSpriteCache.end();) {
        if (it->first.first == hashed_path) {
            it = _copyableSpriteCache.erase(it);
        }
        else {
            ++it;
        }
    }

    for (auto& sprite_factory : _spriteFactories) {
        sprite_factory->InvalidateResource(hashed_path);
    }
}

void ResourceManager::RetryFailedSpriteLoads()
{
    for (auto& sprite_factory : _spriteFactories) {
        sprite_factory->RetryFailedLoads();
    }
}

void ResourceManager::CleanupSpriteCache()
{
    FO_TRACE_ZONE(Render);

    DiscardPreparedSpriteResources();

    for (size_t i = 0; i != _spriteFactories.size(); ++i) {
        _spriteFactories[i]->ClenupCache();
    }

    for (auto it = _copyableSpriteCache.begin(); it != _copyableSpriteCache.end();) {
        if (it->second.use_count() == 1) {
            it = _copyableSpriteCache.erase(it);
        }
        else {
            ++it;
        }
    }

    _sprMngr->GetAtlasMngr()->CleanupAtlases();
}

FO_END_NAMESPACE
