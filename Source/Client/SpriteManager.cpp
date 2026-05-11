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

#include "SpriteManager.h"
#include "Application.h"
#include "DefaultSprites.h"

FO_BEGIN_NAMESPACE

static constexpr float32_t EGG_ENABLED_FLAG = 1.0f;

Sprite::Sprite(SpriteManager& spr_mngr, isize32 size, ipos32 offset) :
    _sprMngr {&spr_mngr},
    _size {size},
    _offset {offset}
{
    FO_STACK_TRACE_ENTRY();
}

auto Sprite::IsHitTest(ipos32 pos) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(pos);
    return false;
}

void Sprite::StartUpdate()
{
    FO_STACK_TRACE_ENTRY();

    _sprMngr->_updateSprites.emplace(this, weak_from_this());
}

SpriteManager::SpriteManager(RenderSettings& settings, IAppWindow& window, FileSystem& resources, GameTimer& game_time, EffectManager& effect_mngr, HashResolver& hash_resolver) :
    _settings {&settings},
    _window {&window},
    _resources {&resources},
    _gameTimer {&game_time},
    _rtMngr(_window->GetRender(), [this]() FO_DEFERRED { Flush(); }),
    _atlasMngr(settings, _rtMngr),
    _render {&_window->GetRender()},
    _input {&_window->GetInput()},
    _effectMngr {&effect_mngr},
    _hashResolver {&hash_resolver}
{
    FO_STACK_TRACE_ENTRY();

    _flushVertCount = 4096;
    _dipQueue.reserve(1000);

    _spritesDrawBuf = _render->CreateDrawBuffer(false);
    _spritesDrawBuf->Vertices.resize(_flushVertCount + 1024);
    _spritesDrawBuf->Indices.resize(_flushVertCount / 4 * 6 + 1024);
    _primitiveDrawBuf = _render->CreateDrawBuffer(false);
    _primitiveDrawBuf->Vertices.resize(1024);
    _primitiveDrawBuf->Indices.resize(1024);
    _flushDrawBuf = _render->CreateDrawBuffer(false);
    _flushDrawBuf->Vertices.resize(4);
    _flushDrawBuf->VertCount = 4;
    _flushDrawBuf->Indices = {0, 1, 3, 1, 2, 3};
    _flushDrawBuf->IndCount = 6;
    _spriteEffectDrawBuf = _render->CreateDrawBuffer(false);
    _spriteEffectDrawBuf->Vertices.resize(4);
    _spriteEffectDrawBuf->VertCount = 4;
    _spriteEffectDrawBuf->Indices = {0, 1, 3, 1, 2, 3};
    _spriteEffectDrawBuf->IndCount = 6;

    const auto window_size = _window->GetSize();

#if !FO_DIRECT_SPRITES_DRAW
    _rtMain = _rtMngr.CreateRenderTarget(false, window_size, true);
#endif

    _eventUnsubscriber += _window->GetOnLowMemory() += [this]() FO_DEFERRED { CleanupSpriteCache(); };
    _eventUnsubscriber += _window->GetOnScreenSizeChanged() += [this]() FO_DEFERRED {
        const auto new_window_size = _window->GetSize();

        if (_rtMain) {
            _rtMngr.ResizeRenderTarget(_rtMain.get(), new_window_size);
        }
    };
}

auto SpriteManager::Random(int32_t min_value, int32_t max_value) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(min_value <= max_value);

    return std::uniform_int_distribution<int32_t> {min_value, max_value}(_randomGenerator);
}

auto SpriteManager::GetWindowSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    return _window->GetSize();
}

void SpriteManager::SetWindowSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    _window->SetSize(size);
}

auto SpriteManager::GetScreenSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    return _window->GetScreenSize();
}

void SpriteManager::SetScreenSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    const auto diff_w = size.width - _settings->ScreenWidth;
    const auto diff_h = size.height - _settings->ScreenHeight;

    if (!IsFullscreen()) {
        const auto window_pos = _window->GetPosition();

        _window->SetPosition({window_pos.x - diff_w / 2, window_pos.y - diff_h / 2});
    }
    else {
        _windowSizeDiff.x += diff_w / 2;
        _windowSizeDiff.y += diff_h / 2;
    }

    _window->SetScreenSize(size);
}

void SpriteManager::ToggleFullscreen()
{
    FO_STACK_TRACE_ENTRY();

    if (!IsFullscreen()) {
        _window->ToggleFullscreen(true);
    }
    else {
        if (_window->ToggleFullscreen(false)) {
            if (_windowSizeDiff != ipos32 {}) {
                const auto window_pos = _window->GetPosition();

                _window->SetPosition({window_pos.x - _windowSizeDiff.x, window_pos.y - _windowSizeDiff.y});
                _windowSizeDiff = {};
            }
        }
    }
}

void SpriteManager::SetMousePosition(ipos32 pos)
{
    FO_STACK_TRACE_ENTRY();

    _input->SetMousePosition(pos, _window.get());
}

auto SpriteManager::IsFullscreen() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _window->IsFullscreen();
}

auto SpriteManager::IsWindowFocused() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _window->IsFocused();
}

void SpriteManager::MinimizeWindow()
{
    FO_STACK_TRACE_ENTRY();

    _window->Minimize();
}

void SpriteManager::BlinkWindow()
{
    FO_STACK_TRACE_ENTRY();

    _window->Blink();
}

void SpriteManager::SetAlwaysOnTop(bool enable)
{
    FO_STACK_TRACE_ENTRY();

    _window->AlwaysOnTop(enable);
}

void SpriteManager::RegisterSpriteFactory(unique_ptr<SpriteFactory>&& factory)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& ext : factory->GetExtensions()) {
        _spriteFactoryMap[ext] = factory.get();
    }

    _spriteFactories.emplace_back(std::move(factory));
}

auto SpriteManager::GetSpriteFactory(std::type_index ti) -> SpriteFactory*
{
    FO_STACK_TRACE_ENTRY();

    for (auto& factory : _spriteFactories) {
        if (const auto& factory_ref = *factory; std::type_index(typeid(factory_ref)) == ti) {
            return factory.get();
        }
    }

    return nullptr;
}

void SpriteManager::BeginScene()
{
    FO_STACK_TRACE_ENTRY();

    _rtMngr.ClearStack();
    _scissorStack.clear();

    if (_rtMain) {
        _rtMngr.PushRenderTarget(_rtMain.get());
        _rtMngr.ClearCurrentRenderTarget(ucolor::clear);
    }

    for (auto& spr_factory : _spriteFactories) {
        spr_factory->Update();
    }

    for (auto it = _updateSprites.begin(); it != _updateSprites.end();) {
        if (auto spr = it->second.lock(); spr && spr->Update()) {
            ++it;
        }
        else {
            it = _updateSprites.erase(it);
        }
    }
}

void SpriteManager::EndScene()
{
    FO_STACK_TRACE_ENTRY();

    Flush();

    if (_rtMain) {
        FO_RUNTIME_ASSERT(_rtMain == _rtMngr.GetCurrentRenderTarget());
        _rtMngr.PopRenderTarget();
        DrawRenderTarget(_rtMain.get(), false);
    }

    FO_RUNTIME_ASSERT(_rtMngr.GetRenderTargetStack().empty());
    FO_RUNTIME_ASSERT(_scissorStack.empty());
}

void SpriteManager::DrawTexture(const RenderTexture* tex, bool alpha_blend, const frect32* region_from, const irect32* region_to, RenderEffect* custom_effect)
{
    FO_STACK_TRACE_ENTRY();

    Flush();

    const auto& rt_stack = _rtMngr.GetRenderTargetStack();
    const auto width_from_i = tex->Size.width;
    const auto height_from_i = tex->Size.height;
    const auto width_to_i = rt_stack.empty() ? _settings->ScreenWidth : rt_stack.back()->GetTexture()->Size.width;
    const auto height_to_i = rt_stack.empty() ? _settings->ScreenHeight : rt_stack.back()->GetTexture()->Size.height;
    const auto width_from_f = numeric_cast<float32_t>(width_from_i);
    const auto height_from_f = numeric_cast<float32_t>(height_from_i);
    const auto width_to_f = numeric_cast<float32_t>(width_to_i);
    const auto height_to_f = numeric_cast<float32_t>(height_to_i);
    const auto flip_from = tex->FlippedHeight;
    const auto flip_to = _render->IsRenderTargetFlipped() && !rt_stack.empty() && !rt_stack.back()->GetTexture()->FlippedHeight;

    if (region_from == nullptr && region_to == nullptr) {
        auto& vbuf = _flushDrawBuf->Vertices;
        size_t vpos = 0;

        vbuf[vpos].PosX = 0.0f;
        vbuf[vpos].PosY = flip_to ? height_to_f : 0.0f;
        vbuf[vpos].TexU = 0.0f;
        vbuf[vpos].TexV = flip_from ? 1.0f : 0.0f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = 0.0f;
        vbuf[vpos].PosY = flip_to ? 0.0f : height_to_f;
        vbuf[vpos].TexU = 0.0f;
        vbuf[vpos].TexV = flip_from ? 0.0f : 1.0f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = width_to_f;
        vbuf[vpos].PosY = flip_to ? 0.0f : height_to_f;
        vbuf[vpos].TexU = 1.0f;
        vbuf[vpos].TexV = flip_from ? 0.0f : 1.0f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = width_to_f;
        vbuf[vpos].PosY = flip_to ? height_to_f : 0.0f;
        vbuf[vpos].TexU = 1.0f;
        vbuf[vpos].TexV = flip_from ? 1.0f : 0.0f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos].EggFlags[1] = 0.0f;
    }
    else {
        const auto rect_from = region_from != nullptr ? *region_from : frect32(0.0f, 0.0f, width_from_f, height_from_f);
        const auto rect_to = frect32(region_to != nullptr ? *region_to : irect32(0, 0, width_to_i, height_to_i));

        auto& vbuf = _flushDrawBuf->Vertices;
        size_t vpos = 0;

        vbuf[vpos].PosX = rect_to.x;
        vbuf[vpos].PosY = flip_to ? height_to_f - rect_to.y : rect_to.y;
        vbuf[vpos].TexU = rect_from.x / width_from_f;
        vbuf[vpos].TexV = flip_from ? 1.0f - (rect_from.y) / height_from_f : rect_from.y / height_from_f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = rect_to.x;
        vbuf[vpos].PosY = flip_to ? height_to_f - (rect_to.y + rect_to.height) : rect_to.y + rect_to.height;
        vbuf[vpos].TexU = rect_from.x / width_from_f;
        vbuf[vpos].TexV = flip_from ? 1.0f - (rect_from.y + rect_from.height) / height_from_f : (rect_from.y + rect_from.height) / height_from_f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = rect_to.x + rect_to.width;
        vbuf[vpos].PosY = flip_to ? height_to_f - (rect_to.y + rect_to.height) : rect_to.y + rect_to.height;
        vbuf[vpos].TexU = (rect_from.x + rect_from.width) / width_from_f;
        vbuf[vpos].TexV = flip_from ? 1.0f - (rect_from.y + rect_from.height) / height_from_f : (rect_from.y + rect_from.height) / height_from_f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = rect_to.x + rect_to.width;
        vbuf[vpos].PosY = flip_to ? height_to_f - rect_to.y : rect_to.y;
        vbuf[vpos].TexU = (rect_from.x + rect_from.width) / width_from_f;
        vbuf[vpos].TexV = flip_from ? 1.0f - (rect_from.y) / height_from_f : rect_from.y / height_from_f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos].EggFlags[1] = 0.0f;
    }

    auto* effect = custom_effect != nullptr ? custom_effect : _effectMngr->Effects.FlushRenderTarget.get();

    effect->MainTex = tex;
    effect->DisableBlending = !alpha_blend;
    _flushDrawBuf->Upload(effect->GetUsage());
    effect->DrawBuffer(_flushDrawBuf.get());
}

void SpriteManager::DrawRenderTarget(RenderTarget* rt, bool alpha_blend, const frect32* region_from, const irect32* region_to)
{
    FO_STACK_TRACE_ENTRY();

    DrawTexture(rt->GetTexture(), alpha_blend, region_from, region_to, rt->GetCustomDrawEffect());
}

void SpriteManager::PushScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    Flush();

    _scissorStack.emplace_back(rect);

    RefreshScissor();
}

void SpriteManager::PopScissor()
{
    FO_STACK_TRACE_ENTRY();

    if (!_scissorStack.empty()) {
        Flush();

        _scissorStack.pop_back();

        RefreshScissor();
    }
}

void SpriteManager::RefreshScissor()
{
    FO_STACK_TRACE_ENTRY();

    if (!_scissorStack.empty()) {
        _scissorRect = _scissorStack.front();
        int32_t right = _scissorRect.x + _scissorRect.width;
        int32_t bottom = _scissorRect.y + _scissorRect.height;

        for (size_t i = 1; i < _scissorStack.size(); i++) {
            _scissorRect.x = std::max(_scissorStack[i].x, _scissorRect.x);
            _scissorRect.y = std::max(_scissorStack[i].y, _scissorRect.y);
            right = std::min(_scissorStack[i].x + _scissorStack[i].width, right);
            bottom = std::min(_scissorStack[i].y + _scissorStack[i].height, bottom);
        }

        right = std::max(right, _scissorRect.x);
        bottom = std::max(bottom, _scissorRect.y);

        _scissorRect.width = right - _scissorRect.x;
        _scissorRect.height = bottom - _scissorRect.y;
    }
}

void SpriteManager::EnableScissor()
{
    FO_STACK_TRACE_ENTRY();

    const auto& rt_stack = _rtMngr.GetRenderTargetStack();

    if (!_scissorStack.empty() && !rt_stack.empty() && rt_stack.back() == _rtMain) {
        _render->EnableScissor(_scissorRect);
    }
}

void SpriteManager::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    const auto& rt_stack = _rtMngr.GetRenderTargetStack();

    if (!_scissorStack.empty() && !rt_stack.empty() && rt_stack.back() == _rtMain) {
        _render->DisableScissor();
    }
}

auto SpriteManager::LoadSprite(string_view path, AtlasType atlas_type, bool no_warn_if_not_exists) -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    return LoadSprite(_hashResolver->ToHashedString(path), atlas_type, no_warn_if_not_exists);
}

auto SpriteManager::LoadSprite(hstring path, AtlasType atlas_type, bool no_warn_if_not_exists) -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    if (!path) {
        return nullptr;
    }

    if (const auto it = _copyableSpriteCache.find({path, atlas_type}); it != _copyableSpriteCache.end()) {
        return it->second->MakeCopy();
    }

    if (_nonFoundSprites.count(path) != 0) {
        return nullptr;
    }

    const string ext = strex(path).get_file_extension();

    if (ext.empty()) {
        BreakIntoDebugger();
        WriteLog("Extension not found, file '{}'", path);
        _nonFoundSprites.emplace(path);
        return nullptr;
    }

    const auto it = _spriteFactoryMap.find(ext);

    if (it == _spriteFactoryMap.end()) {
        BreakIntoDebugger();
        WriteLog("Unknown extension, file '{}'", path);
        _nonFoundSprites.emplace(path);
        return nullptr;
    }

    auto spr = it->second->LoadSprite(path, atlas_type);

    if (!spr) {
        if (!no_warn_if_not_exists) {
            BreakIntoDebugger();
            WriteLog("Sprite not found: '{}'", path);
        }

        _nonFoundSprites.emplace(path);
        return nullptr;
    }

    if (spr->IsCopyable()) {
        _copyableSpriteCache.emplace(pair {path, atlas_type}, spr);
    }

    return spr;
}

void SpriteManager::CleanupSpriteCache()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& spr_factory : _spriteFactories) {
        spr_factory->ClenupCache();
    }

    for (auto it = _copyableSpriteCache.begin(); it != _copyableSpriteCache.end();) {
        if (it->second.use_count() == 1) {
            it = _copyableSpriteCache.erase(it);
        }
        else {
            ++it;
        }
    }
}

void SpriteManager::Flush()
{
    FO_STACK_TRACE_ENTRY();

    if (_spritesDrawBuf->VertCount == 0) {
        return;
    }

    _spritesDrawBuf->Upload(EffectUsage::QuadSprite);

    EnableScissor();

    size_t ipos = 0;

    for (auto& dip : _dipQueue) {
        FO_RUNTIME_ASSERT(dip.SourceEffect->GetUsage() == EffectUsage::QuadSprite);

        if (dip.SourceEffect->IsNeedEggBuf()) {
            auto& egg_buf = dip.SourceEffect->EggBuf = RenderEffect::EggBuffer();

            for (size_t slot_index = 0; slot_index < EGG_SLOT_COUNT; slot_index++) {
                const auto& egg = _eggSlots[slot_index];
                const auto data_index = slot_index * 4;

                egg_buf->EggData[data_index + 0] = egg.Center.x - egg.DrawOffset.x;
                egg_buf->EggData[data_index + 1] = egg.Center.y - egg.DrawOffset.y;
                egg_buf->EggData[data_index + 2] = egg.Valid ? egg.Radius.width : 0.0f;
                egg_buf->EggData[data_index + 3] = egg.Valid ? egg.Radius.height : 0.0f;
            }

            egg_buf->EggData[8] = std::clamp(_settings->EggTransparencyTransitionFactor, 0.0f, 0.9999f);
        }

        dip.SourceEffect->DrawBuffer(_spritesDrawBuf.get(), ipos, dip.IndicesCount, dip.MainTexture.get());

        ipos += dip.IndicesCount;
    }

    DisableScissor();

    _dipQueue.clear();

    FO_RUNTIME_ASSERT(ipos == _spritesDrawBuf->IndCount);

    _spritesDrawBuf->VertCount = 0;
    _spritesDrawBuf->IndCount = 0;
}

void SpriteManager::DrawSprite(const Sprite* spr, ipos32 pos, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    auto* effect = spr->GetDrawEffectOr(_effectMngr->Effects.Iface.get());
    FO_RUNTIME_ASSERT(effect);

    color = ApplyColorBrightness(color);

    const auto ind_count = spr->FillData(_spritesDrawBuf.get(), frect32(fpos32(pos), fsize32(spr->GetSize())), {color, color});

    if (ind_count != 0) {
        if (_dipQueue.empty() || _dipQueue.back().MainTexture != spr->GetBatchTexture() || _dipQueue.back().SourceEffect != effect) {
            _dipQueue.emplace_back(DipData {.MainTexture = spr->GetBatchTexture(), .SourceEffect = effect, .IndicesCount = ind_count});
        }
        else {
            _dipQueue.back().IndicesCount += ind_count;
        }

        if (_spritesDrawBuf->VertCount >= _flushVertCount) {
            Flush();
        }
    }
}

void SpriteManager::DrawSpriteSize(const Sprite* spr, ipos32 pos, isize32 size, bool fit, bool center, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    DrawSpriteSizeExt(spr, fpos32(pos), fsize32(size), fit, center, false, color);
}

void SpriteManager::DrawSpriteSizeExt(const Sprite* spr, fpos32 pos, fsize32 size, bool fit, bool center, bool stretch, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    auto xf = pos.x;
    auto yf = pos.y;
    auto wf = numeric_cast<float32_t>(spr->GetSize().width);
    auto hf = numeric_cast<float32_t>(spr->GetSize().height);
    const auto k = std::min(size.width / wf, size.height / hf);

    if (!stretch) {
        if (k < 1.0f || (k > 1.0f && fit)) {
            wf = std::floor(wf * k + 0.5f);
            hf = std::floor(hf * k + 0.5f);
        }

        if (center) {
            xf += std::floor((size.width - wf) / 2.0f + 0.5f);
            yf += std::floor((size.height - hf) / 2.0f + 0.5f);
        }
    }
    else if (fit) {
        wf = std::floor(wf * k + 0.5f);
        hf = std::floor(hf * k + 0.5f);

        if (center) {
            xf += std::floor((size.width - wf) / 2.0f + 0.5f);
            yf += std::floor((size.height - hf) / 2.0f + 0.5f);
        }
    }
    else {
        wf = size.width;
        hf = size.height;
    }

    auto* effect = spr->GetDrawEffectOr(_effectMngr->Effects.Iface.get());
    FO_RUNTIME_ASSERT(effect);

    color = ApplyColorBrightness(color);

    const auto ind_count = spr->FillData(_spritesDrawBuf.get(), {xf, yf, wf, hf}, {color, color});

    if (ind_count != 0) {
        if (_dipQueue.empty() || _dipQueue.back().MainTexture != spr->GetBatchTexture() || _dipQueue.back().SourceEffect != effect) {
            _dipQueue.emplace_back(DipData {.MainTexture = spr->GetBatchTexture(), .SourceEffect = effect, .IndicesCount = ind_count});
        }
        else {
            _dipQueue.back().IndicesCount += ind_count;
        }

        if (_spritesDrawBuf->VertCount >= _flushVertCount) {
            Flush();
        }
    }
}

auto SpriteManager::DrawSpriteRegion(const Sprite* spr, fpos32 uv0, fpos32 uv1, fpos32 pos, fsize32 size, ucolor color) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto* source_spr = spr;

    if (const auto* sheet = dynamic_cast<const SpriteSheet*>(spr); sheet != nullptr) {
        source_spr = sheet->GetCurSpr();
    }

    const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(source_spr);

    if (atlas_spr == nullptr) {
        return false;
    }

    auto* effect = spr->GetDrawEffectOr(_effectMngr->Effects.Iface.get());
    FO_RUNTIME_ASSERT(effect);

    color = ApplyColorBrightness(color);

    const auto& atlas_rect = atlas_spr->GetAtlasRect();
    const auto tex_left = atlas_rect.x + atlas_rect.width * uv0.x;
    const auto tex_top = atlas_rect.y + atlas_rect.height * uv0.y;
    const auto tex_right = atlas_rect.x + atlas_rect.width * uv1.x;
    const auto tex_bottom = atlas_rect.y + atlas_rect.height * uv1.y;

    _spritesDrawBuf->CheckAllocBuf(4, 6);

    auto& vbuf = _spritesDrawBuf->Vertices;
    auto& vpos = _spritesDrawBuf->VertCount;
    auto& ibuf = _spritesDrawBuf->Indices;
    auto& ipos = _spritesDrawBuf->IndCount;

    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 0);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 2);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);

    auto& v0 = vbuf[vpos++];
    v0.PosX = pos.x;
    v0.PosY = pos.y + size.height;
    v0.TexU = tex_left;
    v0.TexV = tex_bottom;
    v0.EggFlags[0] = 0.0f;
    v0.EggFlags[1] = 0.0f;
    v0.Color = color;

    auto& v1 = vbuf[vpos++];
    v1.PosX = pos.x;
    v1.PosY = pos.y;
    v1.TexU = tex_left;
    v1.TexV = tex_top;
    v1.EggFlags[0] = 0.0f;
    v1.EggFlags[1] = 0.0f;
    v1.Color = color;

    auto& v2 = vbuf[vpos++];
    v2.PosX = pos.x + size.width;
    v2.PosY = pos.y;
    v2.TexU = tex_right;
    v2.TexV = tex_top;
    v2.EggFlags[0] = 0.0f;
    v2.EggFlags[1] = 0.0f;
    v2.Color = color;

    auto& v3 = vbuf[vpos++];
    v3.PosX = pos.x + size.width;
    v3.PosY = pos.y + size.height;
    v3.TexU = tex_right;
    v3.TexV = tex_bottom;
    v3.EggFlags[0] = 0.0f;
    v3.EggFlags[1] = 0.0f;
    v3.Color = color;

    if (_dipQueue.empty() || _dipQueue.back().MainTexture != atlas_spr->GetBatchTexture() || _dipQueue.back().SourceEffect != effect) {
        _dipQueue.emplace_back(DipData {.MainTexture = atlas_spr->GetBatchTexture(), .SourceEffect = effect, .IndicesCount = 6});
    }
    else {
        _dipQueue.back().IndicesCount += 6;
    }

    if (_spritesDrawBuf->VertCount >= _flushVertCount) {
        Flush();
    }

    return true;
}

void SpriteManager::DrawSpritePattern(const Sprite* spr, ipos32 pos, isize32 size, isize32 spr_size, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(size.width > 0);
    FO_RUNTIME_ASSERT(size.height > 0);

    const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(spr);
    if (atlas_spr == nullptr) {
        return;
    }

    auto width = numeric_cast<float32_t>(atlas_spr->GetSize().width);
    auto height = numeric_cast<float32_t>(atlas_spr->GetSize().height);

    if (spr_size.width != 0 && spr_size.height != 0) {
        width = numeric_cast<float32_t>(spr_size.width);
        height = numeric_cast<float32_t>(spr_size.height);
    }
    else if (spr_size.width != 0) {
        const auto ratio = numeric_cast<float32_t>(spr_size.width) / width;
        width = numeric_cast<float32_t>(spr_size.width);
        height *= ratio;
    }
    else if (spr_size.height != 0) {
        const auto ratio = numeric_cast<float32_t>(spr_size.height) / height;
        height = numeric_cast<float32_t>(spr_size.height);
        width *= ratio;
    }

    color = ApplyColorBrightness(color);

    auto* effect = atlas_spr->GetDrawEffectOr(_effectMngr->Effects.Iface.get());
    FO_RUNTIME_ASSERT(effect);

    const auto last_right_offs = atlas_spr->GetAtlasRect().width / width;
    const auto last_bottom_offs = atlas_spr->GetAtlasRect().height / height;

    for (auto yy = numeric_cast<float32_t>(pos.y), end_y = numeric_cast<float32_t>(pos.y + size.height); yy < end_y;) {
        const auto last_y = yy + height >= end_y;

        for (auto xx = numeric_cast<float32_t>(pos.x), end_x = numeric_cast<float32_t>(pos.x + size.width); xx < end_x;) {
            const auto last_x = xx + width >= end_x;

            const auto local_width = last_x ? end_x - xx : width;
            const auto local_height = last_y ? end_y - yy : height;
            const auto local_right = last_x ? atlas_spr->GetAtlasRect().x + last_right_offs * local_width : atlas_spr->GetAtlasRect().x + atlas_spr->GetAtlasRect().width;
            const auto local_bottom = last_y ? atlas_spr->GetAtlasRect().y + last_bottom_offs * local_height : atlas_spr->GetAtlasRect().y + atlas_spr->GetAtlasRect().height;

            _spritesDrawBuf->CheckAllocBuf(4, 6);

            auto& vbuf = _spritesDrawBuf->Vertices;
            auto& vpos = _spritesDrawBuf->VertCount;
            auto& ibuf = _spritesDrawBuf->Indices;
            auto& ipos = _spritesDrawBuf->IndCount;

            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 0);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 2);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);

            vbuf[vpos].PosX = xx;
            vbuf[vpos].PosY = yy + local_height;
            vbuf[vpos].TexU = atlas_spr->GetAtlasRect().x;
            vbuf[vpos].TexV = local_bottom;
            vbuf[vpos].EggFlags[0] = 0.0f;
            vbuf[vpos].EggFlags[1] = 0.0f;
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx;
            vbuf[vpos].PosY = yy;
            vbuf[vpos].TexU = atlas_spr->GetAtlasRect().x;
            vbuf[vpos].TexV = atlas_spr->GetAtlasRect().y;
            vbuf[vpos].EggFlags[0] = 0.0f;
            vbuf[vpos].EggFlags[1] = 0.0f;
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx + local_width;
            vbuf[vpos].PosY = yy;
            vbuf[vpos].TexU = local_right;
            vbuf[vpos].TexV = atlas_spr->GetAtlasRect().y;
            vbuf[vpos].EggFlags[0] = 0.0f;
            vbuf[vpos].EggFlags[1] = 0.0f;
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx + local_width;
            vbuf[vpos].PosY = yy + local_height;
            vbuf[vpos].TexU = local_right;
            vbuf[vpos].TexV = local_bottom;
            vbuf[vpos].EggFlags[0] = 0.0f;
            vbuf[vpos].EggFlags[1] = 0.0f;
            vbuf[vpos++].Color = color;

            if (_dipQueue.empty() || _dipQueue.back().MainTexture != atlas_spr->GetBatchTexture() || _dipQueue.back().SourceEffect != effect) {
                _dipQueue.emplace_back(DipData {.MainTexture = atlas_spr->GetBatchTexture(), .SourceEffect = effect, .IndicesCount = 6});
            }
            else {
                _dipQueue.back().IndicesCount += 6;
            }

            if (_spritesDrawBuf->VertCount >= _flushVertCount) {
                Flush();
            }

            xx += width;
        }

        yy += height;
    }
}

void SpriteManager::PrepareSquare(vector<PrimitivePoint>& points, irect32 r, ucolor color) const
{
    FO_STACK_TRACE_ENTRY();

    points.emplace_back(PrimitivePoint {.PointPos = {r.x, r.y + r.height}, .PointColor = color});
    points.emplace_back(PrimitivePoint {.PointPos = {r.x, r.y}, .PointColor = color});
    points.emplace_back(PrimitivePoint {.PointPos = {r.x + r.width, r.y + r.height}, .PointColor = color});
    points.emplace_back(PrimitivePoint {.PointPos = {r.x, r.y}, .PointColor = color});
    points.emplace_back(PrimitivePoint {.PointPos = {r.x + r.width, r.y}, .PointColor = color});
    points.emplace_back(PrimitivePoint {.PointPos = {r.x + r.width, r.y + r.height}, .PointColor = color});
}

void SpriteManager::PrepareSquare(vector<PrimitivePoint>& points, fpos32 lt, fpos32 rt, fpos32 lb, fpos32 rb, ucolor color) const
{
    FO_STACK_TRACE_ENTRY();

    points.emplace_back(PrimitivePoint {.PointPos = {iround<int32_t>(lb.x), iround<int32_t>(lb.y)}, .PointColor = color});
    points.emplace_back(PrimitivePoint {.PointPos = {iround<int32_t>(lt.x), iround<int32_t>(lt.y)}, .PointColor = color});
    points.emplace_back(PrimitivePoint {.PointPos = {iround<int32_t>(rb.x), iround<int32_t>(rb.y)}, .PointColor = color});
    points.emplace_back(PrimitivePoint {.PointPos = {iround<int32_t>(lt.x), iround<int32_t>(lt.y)}, .PointColor = color});
    points.emplace_back(PrimitivePoint {.PointPos = {iround<int32_t>(rt.x), iround<int32_t>(rt.y)}, .PointColor = color});
    points.emplace_back(PrimitivePoint {.PointPos = {iround<int32_t>(rb.x), iround<int32_t>(rb.y)}, .PointColor = color});
}

void SpriteManager::InvalidateEgg(TransparentEggSlot slot)
{
    FO_STACK_TRACE_ENTRY();

    _eggSlots[static_cast<size_t>(slot)] = {};
}

void SpriteManager::InvalidateEgg()
{
    FO_STACK_TRACE_ENTRY();

    InvalidateEgg(TransparentEggSlot::Primary);
    InvalidateEgg(TransparentEggSlot::Secondary);
}

void SpriteManager::SetEgg(TransparentEggSlot slot, mpos hex, const MapSprite* mspr)
{
    FO_STACK_TRACE_ENTRY();

    const auto slot_index = static_cast<size_t>(slot);

    if (mspr == nullptr) {
        InvalidateEgg(slot);
        return;
    }

    const auto rect = mspr->GetViewRect();

    if (rect.width <= 0 || rect.height <= 0) {
        InvalidateEgg(slot);
        _eggSlots[slot_index].Hex = hex;
        return;
    }

    const auto rect_width = std::max(numeric_cast<float32_t>(rect.width), 1.0f);
    const auto rect_height = std::max(numeric_cast<float32_t>(rect.height), 1.0f);
    auto& egg = _eggSlots[slot_index];
    const auto egg_width = std::max(rect_width + numeric_cast<float32_t>(_settings->EggEllipseWidthExt), 1.0f);
    const auto egg_height = std::max(rect_height + numeric_cast<float32_t>(_settings->EggEllipseHeightExt), 1.0f);

    egg.Center.x = numeric_cast<float32_t>(rect.x) + rect_width * 0.5f;
    egg.Center.y = numeric_cast<float32_t>(rect.y) + rect_height * 0.5f;
    egg.Radius.width = egg_width * 0.5f;
    egg.Radius.height = egg_height * 0.5f;
    egg.Hex = hex;
    egg.Valid = true;
}

void SpriteManager::SetEgg(TransparentEggSlot slot, mpos hex, fpos32 center, fsize32 radius)
{
    FO_STACK_TRACE_ENTRY();

    const auto slot_index = static_cast<size_t>(slot);
    auto& egg = _eggSlots[slot_index];

    if (radius.width <= 0.0f || radius.height <= 0.0f) {
        InvalidateEgg(slot);
        egg.Hex = hex;
        return;
    }

    egg.Center = center;
    egg.Radius = radius;
    egg.Hex = hex;
    egg.Valid = true;
}

auto SpriteManager::CheckEggAppearence(TransparentEggSlot slot, mpos hex, EggAppearenceType appearence) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto& egg = _eggSlots[static_cast<size_t>(slot)];

    if (!egg.Valid) {
        return false;
    }

    if (appearence == EggAppearenceType::None) {
        return false;
    }
    if (appearence == EggAppearenceType::Always) {
        return true;
    }

    if (egg.Hex.y == hex.y && (hex.x % 2) != 0 && (egg.Hex.x % 2) == 0) {
        hex.y--;
    }

    switch (appearence) {
    case EggAppearenceType::ByX:
        if (hex.x >= egg.Hex.x) {
            return true;
        }
        break;
    case EggAppearenceType::ByY:
        if (hex.y >= egg.Hex.y) {
            return true;
        }
        break;
    case EggAppearenceType::ByXAndY:
        if (hex.x >= egg.Hex.x || hex.y >= egg.Hex.y) {
            return true;
        }
        break;
    case EggAppearenceType::ByXOrY:
        if (hex.x >= egg.Hex.x && hex.y >= egg.Hex.y) {
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

void SpriteManager::DrawSprites(MapSpriteList& mspr_list, irect32 draw_area, bool use_egg, DrawOrderType draw_oder_from, DrawOrderType draw_oder_to, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    vector<PrimitivePoint> debug_borders;

    for (auto& egg : _eggSlots) {
        egg.DrawOffset = {numeric_cast<float32_t>(draw_area.x), numeric_cast<float32_t>(draw_area.y)};
    }

    for (const auto& mspr : mspr_list.GetActiveSprites()) {
        FO_RUNTIME_ASSERT(mspr->IsValid());

        if (mspr->GetDrawOrder() < draw_oder_from) {
            continue;
        }
        if (mspr->GetDrawOrder() > draw_oder_to) {
            continue;
        }
        if (mspr->IsHidden()) {
            continue;
        }

        const Sprite* spr = mspr->GetSprite();
        FO_RUNTIME_ASSERT(spr);

        irect32 mspr_rect = mspr->GetDrawRect();
        mspr_rect.x -= draw_area.x;
        mspr_rect.y -= draw_area.y;

        // Skip not visible
        if (mspr_rect.x > draw_area.width || mspr_rect.x + mspr_rect.width < 0 || mspr_rect.y > draw_area.height || mspr_rect.y + mspr_rect.height < 0) {
            continue;
        }

        // Base color
        const ucolor spr_color = mspr->GetColor();
        ucolor color_r;
        ucolor color_l;

        if (spr_color != ucolor::clear) {
            color_r = color_l = ucolor(spr_color, 255);
        }
        else {
            color_r = color_l = color;
        }

        // Light
        const auto* light = mspr->GetLight();

        if (light != nullptr) {
            const auto mix_light = [](ucolor& c, const ucolor* l, const ucolor* l2) {
                const auto r2 = l2 != nullptr ? l2->comp.r : l->comp.r;
                const auto g2 = l2 != nullptr ? l2->comp.g : l->comp.g;
                const auto b2 = l2 != nullptr ? l2->comp.b : l->comp.b;
                c.comp.r = numeric_cast<uint8_t>(std::min(c.comp.r + (l->comp.r + r2) / 2, 255));
                c.comp.g = numeric_cast<uint8_t>(std::min(c.comp.g + (l->comp.g + g2) / 2, 255));
                c.comp.b = numeric_cast<uint8_t>(std::min(c.comp.b + (l->comp.b + b2) / 2, 255));
            };

            mix_light(color_r, light, mspr->GetLightRight());
            mix_light(color_l, light, mspr->GetLightLeft());
        }

        // Alpha
        const auto* alpha = mspr->GetAlpha();

        if (alpha != nullptr) {
            color_r.comp.a = *alpha;
            color_l.comp.a = *alpha;
        }

        // Fix color
        color_r = ApplyColorBrightness(color_r);
        color_l = ApplyColorBrightness(color_l);

        // Choose effect
        auto* spr_effect = mspr->GetDrawEffect();
        auto* effect = spr_effect != nullptr ? *spr_effect : nullptr;

        if (effect == nullptr) {
            effect = spr->GetDrawEffectOr(_effectMngr->Effects.Generic.get());
        }

        // Fill buffer
        const float32_t xf = numeric_cast<float32_t>(mspr_rect.x);
        const float32_t yf = numeric_cast<float32_t>(mspr_rect.y);
        const float32_t wf = numeric_cast<float32_t>(spr->GetSize().width);
        const float32_t hf = numeric_cast<float32_t>(spr->GetSize().height);
        const auto start_vpos = _spritesDrawBuf->VertCount;
        const auto ind_count = spr->FillData(_spritesDrawBuf.get(), {xf, yf, wf, hf}, {color_l, color_r});

        // Setup egg
        const bool use_first_egg = use_egg && CheckEggAppearence(TransparentEggSlot::Primary, mspr->GetHex(), mspr->GetEggAppearence());
        const bool use_second_egg = use_egg && CheckEggAppearence(TransparentEggSlot::Secondary, mspr->GetHex(), mspr->GetEggAppearence());

        if (use_first_egg || use_second_egg) {
            auto& vbuf = _spritesDrawBuf->Vertices;

            for (size_t i = start_vpos; i < _spritesDrawBuf->VertCount; i++) {
                vbuf[i].EggFlags[0] = use_first_egg ? EGG_ENABLED_FLAG : 0.0f;
                vbuf[i].EggFlags[1] = use_second_egg ? EGG_ENABLED_FLAG : 0.0f;
            }
        }

        if (ind_count != 0) {
            if (_dipQueue.empty() || _dipQueue.back().MainTexture != spr->GetBatchTexture() || _dipQueue.back().SourceEffect != effect) {
                _dipQueue.emplace_back(DipData {.MainTexture = spr->GetBatchTexture(), .SourceEffect = effect, .IndicesCount = ind_count});
            }
            else {
                _dipQueue.back().IndicesCount += ind_count;
            }

            if (_spritesDrawBuf->VertCount >= _flushVertCount) {
                Flush();
            }
        }

        // Corners indication
        if (_settings->ShowCorners && mspr->GetEggAppearence() != EggAppearenceType::None) {
            vector<PrimitivePoint> corner;
            const float32_t cx = wf / 2.0f;

            switch (mspr->GetEggAppearence()) {
            case EggAppearenceType::Always:
                PrepareSquare(corner, irect32(iround<int32_t>(xf + cx - 2.0f), iround<int32_t>(yf + hf - 50.0f), 4, 50), ucolor {0x5F00FFFF});
                break;
            case EggAppearenceType::ByX:
                PrepareSquare(corner, fpos32 {xf + cx - 5.0f, yf + hf - 55.0f}, fpos32 {xf + cx + 5.0f, yf + hf - 45.0f}, fpos32 {xf + cx - 5.0f, yf + hf - 5.0f}, fpos32 {xf + cx + 5.0f, yf + hf + 5.0f}, ucolor {0x5F00AF00});
                break;
            case EggAppearenceType::ByY:
                PrepareSquare(corner, fpos32 {xf + cx - 5.0f, yf + hf - 49.0f}, fpos32 {xf + cx + 5.0f, yf + hf - 52.0f}, fpos32 {xf + cx - 5.0f, yf + hf + 1.0f}, fpos32 {xf + cx + 5.0f, yf + hf - 2.0f}, ucolor {0x5F00FF00});
                break;
            case EggAppearenceType::ByXAndY:
                PrepareSquare(corner, fpos32 {xf + cx - 10.0f, yf + hf - 49.0f}, fpos32 {xf + cx, yf + hf - 52.0f}, fpos32 {xf + cx - 10.0f, yf + hf + 1.0f}, fpos32 {xf + cx, yf + hf - 2.0f}, ucolor {0x5F0000FF});
                PrepareSquare(corner, fpos32 {xf + cx, yf + hf - 55.0f}, fpos32 {xf + cx + 10.0f, yf + hf - 45.0f}, fpos32 {xf + cx, yf + hf - 5.0f}, fpos32 {xf + cx + 10.0f, yf + hf + 5.0f}, ucolor {0x5F0000FF});
                break;
            case EggAppearenceType::ByXOrY:
                PrepareSquare(corner, fpos32 {xf + cx, yf + hf - 49.0f}, fpos32 {xf + cx + 10.0f, yf + hf - 52.0f}, fpos32 {xf + cx, yf + hf + 1.0f}, fpos32 {xf + cx + 10.0f, yf + hf - 2.0f}, ucolor {0x5F0000AF});
                PrepareSquare(corner, fpos32 {xf + cx - 10.0f, yf + hf - 55.0f}, fpos32 {xf + cx, yf + hf - 45.0f}, fpos32 {xf + cx - 10.0f, yf + hf - 5.0f}, fpos32 {xf + cx, yf + hf + 5.0f}, ucolor {0x5F0000AF});
                break;
            default:
                break;
            }

            DrawPoints(corner, RenderPrimitiveType::TriangleList);
        }

        if (_settings->ShowSpriteBorders && mspr->GetDrawOrder() > DrawOrderType::Tile4) {
            irect32 rect = mspr->GetViewRect();
            rect.x -= draw_area.x;
            rect.y -= draw_area.y;
            PrepareSquare(debug_borders, rect, ucolor {0, 0, 255, 50});
        }
    }

    Flush();

    if (_settings->ShowSpriteBorders) {
        DrawPoints(debug_borders, RenderPrimitiveType::TriangleList);
    }
}

auto SpriteManager::SpriteHitTest(const Sprite* spr, ipos32 pos) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (pos.x < 0 || pos.y < 0) {
        return false;
    }

    return spr->IsHitTest(pos);
}

auto SpriteManager::IsEggTransp(ipos32 pos, mpos hex, EggAppearenceType appearence) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    for (size_t slot_index = 0; slot_index < EGG_SLOT_COUNT; slot_index++) {
        const auto& egg = _eggSlots[slot_index];
        const auto slot = slot_index == 0 ? TransparentEggSlot::Primary : TransparentEggSlot::Secondary;

        if (!egg.Valid) {
            continue;
        }
        if (!CheckEggAppearence(slot, hex, appearence)) {
            continue;
        }
        if (egg.Radius.width <= 0.0f || egg.Radius.height <= 0.0f) {
            continue;
        }

        const auto dx = (numeric_cast<float32_t>(pos.x) - egg.Center.x) / egg.Radius.width;
        const auto dy = (numeric_cast<float32_t>(pos.y) - egg.Center.y) / egg.Radius.height;
        const auto egg_alpha_raw = std::clamp(dx * dx + dy * dy, 0.0f, 1.0f);
        const auto transition_start = std::clamp(_settings->EggTransparencyTransitionFactor, 0.0f, 0.9999f);
        const auto egg_alpha = egg_alpha_raw <= transition_start ? 0.0f : (egg_alpha_raw - transition_start) / (1.0f - transition_start);

        if (!CheckHitTest(iround<int32_t>(egg_alpha * 255.0f))) {
            return true;
        }
    }

    return false;
}

void SpriteManager::DrawPoints(const vector<PrimitivePoint>& points, RenderPrimitiveType prim, const irect32* draw_area, RenderEffect* custom_effect)
{
    FO_STACK_TRACE_ENTRY();

    if (points.empty()) {
        return;
    }

    Flush();

    auto* effect = custom_effect != nullptr ? custom_effect : _effectMngr->Effects.Primitive.get();
    FO_RUNTIME_ASSERT(effect);

    // Check primitives
    const auto count = points.size();

    if (count == 0) {
        return;
    }

    switch (prim) {
    case RenderPrimitiveType::PointList:
        break;
    case RenderPrimitiveType::LineList:
        FO_RUNTIME_ASSERT(count % 2 == 0);
        break;
    case RenderPrimitiveType::LineStrip:
        FO_RUNTIME_ASSERT(count > 1);
        break;
    case RenderPrimitiveType::TriangleList:
        FO_RUNTIME_ASSERT(count % 3 == 0);
        break;
    case RenderPrimitiveType::TriangleStrip:
        FO_RUNTIME_ASSERT(count > 2);
        break;
    }

    auto& vbuf = _primitiveDrawBuf->Vertices;
    auto& vpos = _primitiveDrawBuf->VertCount;
    auto& ibuf = _primitiveDrawBuf->Indices;
    auto& ipos = _primitiveDrawBuf->IndCount;

    vpos = 0;
    ipos = 0;

    _primitiveDrawBuf->CheckAllocBuf(count, count);

    vpos = count;
    ipos = count;

    for (size_t i = 0; i < count; i++) {
        const auto& point = points[i];
        ipos32 pos = point.PointPos;

        if (point.PointOffset) {
            pos += *point.PointOffset;
        }
        if (draw_area != nullptr) {
            pos -= ipos32(draw_area->x, draw_area->y);
        }

        vbuf[i].PosX = numeric_cast<float32_t>(pos.x);
        vbuf[i].PosY = numeric_cast<float32_t>(pos.y);
        vbuf[i].Color = point.PPointColor ? *point.PPointColor : point.PointColor;

        ibuf[i] = numeric_cast<vindex_t>(i);
    }

    _primitiveDrawBuf->PrimType = prim;
    _primitiveDrawBuf->Upload(effect->GetUsage(), count, count);

    EnableScissor();
    effect->DrawBuffer(_primitiveDrawBuf.get(), 0, count);
    DisableScissor();
}

void SpriteManager::DrawSpriteWithEffect(const Sprite* spr, ipos32 pos, ucolor color, RenderEffect* effect, int32_t padding)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(spr);
    FO_RUNTIME_ASSERT(effect);
    FO_RUNTIME_ASSERT(padding >= 0);
    FO_RUNTIME_ASSERT(effect->GetUsage() == EffectUsage::QuadSprite);

    const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(spr);

    if (atlas_spr == nullptr) {
        return;
    }

    Flush();

    const auto* texture = atlas_spr->GetAtlas()->GetTexture();
    const frect32 sr = atlas_spr->GetAtlasRect();
    const float32_t padding_f = numeric_cast<float32_t>(padding);
    const float32_t txw = texture->SizeData[2] * padding_f;
    const float32_t txh = texture->SizeData[3] * padding_f;
    const frect32 textureuv = frect32(sr.x - txw, sr.y - txh, sr.width + txw * 2.0f, sr.height + txh * 2.0f);
    const frect32 borders = frect32(irect32(pos.x - padding, pos.y - padding, atlas_spr->GetSize().width + padding * 2, atlas_spr->GetSize().height + padding * 2));

    color = ApplyColorBrightness(color);

    auto& vbuf = _spriteEffectDrawBuf->Vertices;
    size_t vpos = 0;

    vbuf[vpos].PosX = borders.x;
    vbuf[vpos].PosY = borders.y + borders.height;
    vbuf[vpos].TexU = textureuv.x;
    vbuf[vpos].TexV = textureuv.y + textureuv.height;
    vbuf[vpos].EggFlags[0] = 0.0f;
    vbuf[vpos].EggFlags[1] = 0.0f;
    vbuf[vpos++].Color = color;

    vbuf[vpos].PosX = borders.x;
    vbuf[vpos].PosY = borders.y;
    vbuf[vpos].TexU = textureuv.x;
    vbuf[vpos].TexV = textureuv.y;
    vbuf[vpos].EggFlags[0] = 0.0f;
    vbuf[vpos].EggFlags[1] = 0.0f;
    vbuf[vpos++].Color = color;

    vbuf[vpos].PosX = borders.x + borders.width;
    vbuf[vpos].PosY = borders.y;
    vbuf[vpos].TexU = textureuv.x + textureuv.width;
    vbuf[vpos].TexV = textureuv.y;
    vbuf[vpos].EggFlags[0] = 0.0f;
    vbuf[vpos].EggFlags[1] = 0.0f;
    vbuf[vpos++].Color = color;

    vbuf[vpos].PosX = borders.x + borders.width;
    vbuf[vpos].PosY = borders.y + borders.height;
    vbuf[vpos].TexU = textureuv.x + textureuv.width;
    vbuf[vpos].TexV = textureuv.y + textureuv.height;
    vbuf[vpos].EggFlags[0] = 0.0f;
    vbuf[vpos].EggFlags[1] = 0.0f;
    vbuf[vpos].Color = color;

    if (effect->IsNeedSpriteBorderBuf()) {
        auto& sprite_border_buf = effect->SpriteBorderBuf = RenderEffect::SpriteBorderBuffer();

        sprite_border_buf->SpriteBorder[0] = sr.x;
        sprite_border_buf->SpriteBorder[1] = sr.y;
        sprite_border_buf->SpriteBorder[2] = sr.x + sr.width;
        sprite_border_buf->SpriteBorder[3] = sr.y + sr.height;
    }

    _spriteEffectDrawBuf->Upload(effect->GetUsage());
    effect->DrawBuffer(_spriteEffectDrawBuf.get(), 0, std::nullopt, texture);
}

auto SpriteManager::ApplyColorBrightness(ucolor color) const -> ucolor
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_settings->Brightness != 0) {
        const auto r = std::clamp(numeric_cast<int32_t>(color.comp.r) + _settings->Brightness, 0, 255);
        const auto g = std::clamp(numeric_cast<int32_t>(color.comp.g) + _settings->Brightness, 0, 255);
        const auto b = std::clamp(numeric_cast<int32_t>(color.comp.b) + _settings->Brightness, 0, 255);
        return ucolor {numeric_cast<uint8_t>(r), numeric_cast<uint8_t>(g), numeric_cast<uint8_t>(b), color.comp.a};
    }
    else {
        return color;
    }
}

FO_END_NAMESPACE
