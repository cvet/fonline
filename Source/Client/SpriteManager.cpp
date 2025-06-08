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

#include "SpriteManager.h"
#include "DefaultSprites.h"

FO_BEGIN_NAMESPACE();

Sprite::Sprite(SpriteManager& spr_mngr) :
    _sprMngr {spr_mngr}
{
    FO_STACK_TRACE_ENTRY();
}

auto Sprite::IsHitTest(ipos pos) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(pos);

    return false;
}

void Sprite::StartUpdate()
{
    FO_STACK_TRACE_ENTRY();

    _sprMngr._updateSprites.emplace(this, weak_from_this());
}

SpriteManager::SpriteManager(RenderSettings& settings, AppWindow* window, FileSystem& resources, GameTimer& game_time, EffectManager& effect_mngr, HashResolver& hash_resolver) :
    _settings {settings},
    _window {window},
    _resources {resources},
    _gameTimer {game_time},
    _rtMngr(settings, window, [this] { Flush(); }),
    _atlasMngr(settings, _rtMngr),
    _effectMngr {effect_mngr},
    _hashResolver {hash_resolver}
{
    FO_STACK_TRACE_ENTRY();

    _flushVertCount = 4096;
    _dipQueue.reserve(1000);

    _spritesDrawBuf = App->Render.CreateDrawBuffer(false);
    _spritesDrawBuf->Vertices.resize(_flushVertCount + 1024);
    _spritesDrawBuf->Indices.resize(_flushVertCount / 4 * 6 + 1024);
    _primitiveDrawBuf = App->Render.CreateDrawBuffer(false);
    _primitiveDrawBuf->Vertices.resize(1024);
    _primitiveDrawBuf->Indices.resize(1024);
    _flushDrawBuf = App->Render.CreateDrawBuffer(false);
    _flushDrawBuf->Vertices.resize(4);
    _flushDrawBuf->VertCount = 4;
    _flushDrawBuf->Indices = {0, 1, 3, 1, 2, 3};
    _flushDrawBuf->IndCount = 6;
    _contourDrawBuf = App->Render.CreateDrawBuffer(false);
    _contourDrawBuf->Vertices.resize(4);
    _contourDrawBuf->VertCount = 4;
    _contourDrawBuf->Indices = {0, 1, 3, 1, 2, 3};
    _contourDrawBuf->IndCount = 6;

#if !FO_DIRECT_SPRITES_DRAW
    _rtMain = _rtMngr.CreateRenderTarget(false, RenderTarget::SizeKindType::Screen, {}, true);
#endif
    _rtContours = _rtMngr.CreateRenderTarget(false, RenderTarget::SizeKindType::Map, {}, false);
    _rtContoursMid = _rtMngr.CreateRenderTarget(false, RenderTarget::SizeKindType::Map, {}, false);

    _eventUnsubscriber += App->OnLowMemory += [this] { CleanupSpriteCache(); };
}

SpriteManager::~SpriteManager()
{
    FO_STACK_TRACE_ENTRY();

    _window->Destroy();
}

auto SpriteManager::GetWindowSize() const -> isize
{
    FO_STACK_TRACE_ENTRY();

    return _window->GetSize();
}

void SpriteManager::SetWindowSize(isize size)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    _window->SetSize(size);
}

auto SpriteManager::GetScreenSize() const -> isize
{
    FO_STACK_TRACE_ENTRY();

    return _window->GetScreenSize();
}

void SpriteManager::SetScreenSize(isize size)
{
    FO_STACK_TRACE_ENTRY();

    const auto diff_w = size.width - App->Settings.ScreenWidth;
    const auto diff_h = size.height - App->Settings.ScreenHeight;

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
            if (_windowSizeDiff != ipos {}) {
                const auto window_pos = _window->GetPosition();

                _window->SetPosition({window_pos.x - _windowSizeDiff.x, window_pos.y - _windowSizeDiff.y});
                _windowSizeDiff = {};
            }
        }
    }
}

void SpriteManager::SetMousePosition(ipos pos)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    App->Input.SetMousePosition(pos, _window);
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

    FO_NON_CONST_METHOD_HINT();

    _window->Minimize();
}

void SpriteManager::BlinkWindow()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    _window->Blink();
}

void SpriteManager::SetAlwaysOnTop(bool enable)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

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

    if (_rtMain != nullptr) {
        _rtMngr.PushRenderTarget(_rtMain);
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

    if (_rtMain != nullptr) {
        FO_RUNTIME_ASSERT(_rtMngr.GetCurrentRenderTarget() == _rtMain);
        _rtMngr.PopRenderTarget();
        DrawRenderTarget(_rtMain, false);
    }

    FO_RUNTIME_ASSERT(_rtMngr.GetRenderTargetStack().empty());
    FO_RUNTIME_ASSERT(_scissorStack.empty());
}

void SpriteManager::DrawTexture(const RenderTexture* tex, bool alpha_blend, const IRect* region_from, const IRect* region_to, RenderEffect* custom_effect)
{
    FO_STACK_TRACE_ENTRY();

    Flush();

    const auto& rt_stack = _rtMngr.GetRenderTargetStack();
    const auto flipped_height = tex->FlippedHeight;
    const auto width_from_i = tex->Size.width;
    const auto height_from_i = tex->Size.height;
    const auto width_to_i = rt_stack.empty() ? _settings.ScreenWidth : rt_stack.back()->MainTex->Size.width;
    const auto height_to_i = rt_stack.empty() ? _settings.ScreenHeight : rt_stack.back()->MainTex->Size.height;
    const auto width_from_f = numeric_cast<float32>(width_from_i);
    const auto height_from_f = numeric_cast<float32>(height_from_i);
    const auto width_to_f = numeric_cast<float32>(width_to_i);
    const auto height_to_f = numeric_cast<float32>(height_to_i);

    if (region_from == nullptr && region_to == nullptr) {
        auto& vbuf = _flushDrawBuf->Vertices;
        size_t vpos = 0;

        vbuf[vpos].PosX = 0.0f;
        vbuf[vpos].PosY = flipped_height ? height_to_f : 0.0f;
        vbuf[vpos].TexU = 0.0f;
        vbuf[vpos].TexV = 0.0f;
        vbuf[vpos++].EggTexU = 0.0f;

        vbuf[vpos].PosX = 0.0f;
        vbuf[vpos].PosY = flipped_height ? 0.0f : height_to_f;
        vbuf[vpos].TexU = 0.0f;
        vbuf[vpos].TexV = 1.0f;
        vbuf[vpos++].EggTexU = 0.0f;

        vbuf[vpos].PosX = width_to_f;
        vbuf[vpos].PosY = flipped_height ? 0.0f : height_to_f;
        vbuf[vpos].TexU = 1.0f;
        vbuf[vpos].TexV = 1.0f;
        vbuf[vpos++].EggTexU = 0.0f;

        vbuf[vpos].PosX = width_to_f;
        vbuf[vpos].PosY = flipped_height ? height_to_f : 0.0f;
        vbuf[vpos].TexU = 1.0f;
        vbuf[vpos].TexV = 0.0f;
        vbuf[vpos].EggTexU = 0.0f;
    }
    else {
        const FRect rect_from = region_from != nullptr ? *region_from : IRect(0, 0, width_from_i, height_from_i);
        const FRect rect_to = region_to != nullptr ? *region_to : IRect(0, 0, width_to_i, height_to_i);

        auto& vbuf = _flushDrawBuf->Vertices;
        size_t vpos = 0;

        vbuf[vpos].PosX = rect_to.Left;
        vbuf[vpos].PosY = flipped_height ? rect_to.Bottom : rect_to.Top;
        vbuf[vpos].TexU = rect_from.Left / width_from_f;
        vbuf[vpos].TexV = flipped_height ? 1.0f - rect_from.Bottom / height_from_f : rect_from.Top / height_from_f;
        vbuf[vpos++].EggTexU = 0.0f;

        vbuf[vpos].PosX = rect_to.Left;
        vbuf[vpos].PosY = flipped_height ? rect_to.Top : rect_to.Bottom;
        vbuf[vpos].TexU = rect_from.Left / width_from_f;
        vbuf[vpos].TexV = flipped_height ? 1.0f - rect_from.Top / height_from_f : rect_from.Bottom / height_from_f;
        vbuf[vpos++].EggTexU = 0.0f;

        vbuf[vpos].PosX = rect_to.Right;
        vbuf[vpos].PosY = flipped_height ? rect_to.Top : rect_to.Bottom;
        vbuf[vpos].TexU = rect_from.Right / width_from_f;
        vbuf[vpos].TexV = flipped_height ? 1.0f - rect_from.Top / height_from_f : rect_from.Bottom / height_from_f;
        vbuf[vpos++].EggTexU = 0.0f;

        vbuf[vpos].PosX = rect_to.Right;
        vbuf[vpos].PosY = flipped_height ? rect_to.Bottom : rect_to.Top;
        vbuf[vpos].TexU = rect_from.Right / width_from_f;
        vbuf[vpos].TexV = flipped_height ? 1.0f - rect_from.Bottom / height_from_f : rect_from.Top / height_from_f;
        vbuf[vpos].EggTexU = 0.0f;
    }

    auto* effect = custom_effect != nullptr ? custom_effect : _effectMngr.Effects.FlushRenderTarget;

    effect->MainTex = tex;
    effect->DisableBlending = !alpha_blend;
    _flushDrawBuf->Upload(effect->GetUsage());
    effect->DrawBuffer(_flushDrawBuf.get());
}

void SpriteManager::DrawRenderTarget(const RenderTarget* rt, bool alpha_blend, const IRect* region_from, const IRect* region_to)
{
    FO_STACK_TRACE_ENTRY();

    if (region_to == nullptr && rt->SizeKind == RenderTarget::SizeKindType::Map) {
        const IRect region_without_hud = {0, 0, _settings.ScreenWidth, _settings.ScreenHeight - _settings.ScreenHudHeight};
        DrawTexture(rt->MainTex.get(), alpha_blend, region_from, &region_without_hud, rt->CustomDrawEffect);
    }
    else {
        DrawTexture(rt->MainTex.get(), alpha_blend, region_from, region_to, rt->CustomDrawEffect);
    }
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
        int32 right = _scissorRect.x + _scissorRect.width;
        int32 bottom = _scissorRect.y + _scissorRect.height;

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
        App->Render.EnableScissor(_scissorRect);
    }
}

void SpriteManager::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    const auto& rt_stack = _rtMngr.GetRenderTargetStack();

    if (!_scissorStack.empty() && !rt_stack.empty() && rt_stack.back() == _rtMain) {
        App->Render.DisableScissor();
    }
}

auto SpriteManager::LoadSprite(string_view path, AtlasType atlas_type, bool no_warn_if_not_exists) -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    return LoadSprite(_hashResolver.ToHashedString(path), atlas_type, no_warn_if_not_exists);
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

    const string ext = strex(path).getFileExtension();

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

void SpriteManager::SetSpritesZoom(float32 zoom)
{
    FO_STACK_TRACE_ENTRY();

    _spritesZoom = zoom;
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

    for (const auto& dip : _dipQueue) {
        FO_RUNTIME_ASSERT(dip.SourceEffect->GetUsage() == EffectUsage::QuadSprite);

        if (_sprEgg) {
            dip.SourceEffect->EggTex = _sprEgg->Atlas->MainTex;
        }

        dip.SourceEffect->DrawBuffer(_spritesDrawBuf.get(), ipos, dip.IndCount, dip.MainTex);

        ipos += dip.IndCount;
    }

    DisableScissor();

    _dipQueue.clear();

    FO_RUNTIME_ASSERT(ipos == _spritesDrawBuf->IndCount);

    _spritesDrawBuf->VertCount = 0;
    _spritesDrawBuf->IndCount = 0;
}

void SpriteManager::DrawSprite(const Sprite* spr, ipos pos, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    auto* effect = spr->DrawEffect != nullptr ? spr->DrawEffect : _effectMngr.Effects.Iface;
    FO_RUNTIME_ASSERT(effect);

    color = ApplyColorBrightness(color);

    const auto ind_count = spr->FillData(_spritesDrawBuf.get(), IRect {pos.x, pos.y, pos.x + spr->Size.width, pos.y + spr->Size.height}, {color, color});

    if (ind_count != 0) {
        if (_dipQueue.empty() || _dipQueue.back().MainTex != spr->GetBatchTex() || _dipQueue.back().SourceEffect != effect) {
            _dipQueue.emplace_back(DipData {spr->GetBatchTex(), effect, ind_count});
        }
        else {
            _dipQueue.back().IndCount += ind_count;
        }

        if (_spritesDrawBuf->VertCount >= _flushVertCount) {
            Flush();
        }
    }
}

void SpriteManager::DrawSpriteSize(const Sprite* spr, ipos pos, isize size, bool fit, bool center, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    DrawSpriteSizeExt(spr, fpos(pos), fsize(size), fit, center, false, color);
}

void SpriteManager::DrawSpriteSizeExt(const Sprite* spr, fpos pos, fsize size, bool fit, bool center, bool stretch, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    auto xf = pos.x;
    auto yf = pos.y;
    auto wf = numeric_cast<float32>(spr->Size.width);
    auto hf = numeric_cast<float32>(spr->Size.height);
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

    auto* effect = spr->DrawEffect != nullptr ? spr->DrawEffect : _effectMngr.Effects.Iface;
    FO_RUNTIME_ASSERT(effect);

    color = ApplyColorBrightness(color);

    const auto ind_count = spr->FillData(_spritesDrawBuf.get(), {xf, yf, xf + wf, yf + hf}, {color, color});

    if (ind_count != 0) {
        if (_dipQueue.empty() || _dipQueue.back().MainTex != spr->GetBatchTex() || _dipQueue.back().SourceEffect != effect) {
            _dipQueue.emplace_back(DipData {spr->GetBatchTex(), effect, ind_count});
        }
        else {
            _dipQueue.back().IndCount += ind_count;
        }

        if (_spritesDrawBuf->VertCount >= _flushVertCount) {
            Flush();
        }
    }
}

void SpriteManager::DrawSpritePattern(const Sprite* spr, ipos pos, isize size, isize spr_size, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(size.width > 0);
    FO_RUNTIME_ASSERT(size.height > 0);

    const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(spr);
    if (atlas_spr == nullptr) {
        return;
    }

    auto width = numeric_cast<float32>(atlas_spr->Size.width);
    auto height = numeric_cast<float32>(atlas_spr->Size.height);

    if (spr_size.width != 0 && spr_size.height != 0) {
        width = numeric_cast<float32>(spr_size.width);
        height = numeric_cast<float32>(spr_size.height);
    }
    else if (spr_size.width != 0) {
        const auto ratio = numeric_cast<float32>(spr_size.width) / width;
        width = numeric_cast<float32>(spr_size.width);
        height *= ratio;
    }
    else if (spr_size.height != 0) {
        const auto ratio = numeric_cast<float32>(spr_size.height) / height;
        height = numeric_cast<float32>(spr_size.height);
        width *= ratio;
    }

    color = ApplyColorBrightness(color);

    auto* effect = atlas_spr->DrawEffect != nullptr ? atlas_spr->DrawEffect : _effectMngr.Effects.Iface;
    FO_RUNTIME_ASSERT(effect);

    const auto last_right_offs = (atlas_spr->AtlasRect.Right - atlas_spr->AtlasRect.Left) / width;
    const auto last_bottom_offs = (atlas_spr->AtlasRect.Bottom - atlas_spr->AtlasRect.Top) / height;

    for (auto yy = numeric_cast<float32>(pos.y), end_y = numeric_cast<float32>(pos.y + size.height); yy < end_y;) {
        const auto last_y = yy + height >= end_y;

        for (auto xx = numeric_cast<float32>(pos.x), end_x = numeric_cast<float32>(pos.x + size.width); xx < end_x;) {
            const auto last_x = xx + width >= end_x;

            const auto local_width = last_x ? end_x - xx : width;
            const auto local_height = last_y ? end_y - yy : height;
            const auto local_right = last_x ? atlas_spr->AtlasRect.Left + last_right_offs * local_width : atlas_spr->AtlasRect.Right;
            const auto local_bottom = last_y ? atlas_spr->AtlasRect.Top + last_bottom_offs * local_height : atlas_spr->AtlasRect.Bottom;

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
            vbuf[vpos].TexU = atlas_spr->AtlasRect.Left;
            vbuf[vpos].TexV = local_bottom;
            vbuf[vpos].EggTexU = 0.0f;
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx;
            vbuf[vpos].PosY = yy;
            vbuf[vpos].TexU = atlas_spr->AtlasRect.Left;
            vbuf[vpos].TexV = atlas_spr->AtlasRect.Top;
            vbuf[vpos].EggTexU = 0.0f;
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx + local_width;
            vbuf[vpos].PosY = yy;
            vbuf[vpos].TexU = local_right;
            vbuf[vpos].TexV = atlas_spr->AtlasRect.Top;
            vbuf[vpos].EggTexU = 0.0f;
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx + local_width;
            vbuf[vpos].PosY = yy + local_height;
            vbuf[vpos].TexU = local_right;
            vbuf[vpos].TexV = local_bottom;
            vbuf[vpos].EggTexU = 0.0f;
            vbuf[vpos++].Color = color;

            if (_dipQueue.empty() || _dipQueue.back().MainTex != atlas_spr->GetBatchTex() || _dipQueue.back().SourceEffect != effect) {
                _dipQueue.emplace_back(DipData {atlas_spr->GetBatchTex(), effect, 6});
            }
            else {
                _dipQueue.back().IndCount += 6;
            }

            if (_spritesDrawBuf->VertCount >= _flushVertCount) {
                Flush();
            }

            xx += width;
        }

        yy += height;
    }
}

void SpriteManager::PrepareSquare(vector<PrimitivePoint>& points, const IRect& r, ucolor color) const
{
    FO_STACK_TRACE_ENTRY();

    points.emplace_back(PrimitivePoint {{r.Left, r.Bottom}, color});
    points.emplace_back(PrimitivePoint {{r.Left, r.Top}, color});
    points.emplace_back(PrimitivePoint {{r.Right, r.Bottom}, color});
    points.emplace_back(PrimitivePoint {{r.Left, r.Top}, color});
    points.emplace_back(PrimitivePoint {{r.Right, r.Top}, color});
    points.emplace_back(PrimitivePoint {{r.Right, r.Bottom}, color});
}

void SpriteManager::PrepareSquare(vector<PrimitivePoint>& points, fpos lt, fpos rt, fpos lb, fpos rb, ucolor color) const
{
    FO_STACK_TRACE_ENTRY();

    points.emplace_back(PrimitivePoint {{iround<int32>(lb.x), iround<int32>(lb.y)}, color});
    points.emplace_back(PrimitivePoint {{iround<int32>(lt.x), iround<int32>(lt.y)}, color});
    points.emplace_back(PrimitivePoint {{iround<int32>(rb.x), iround<int32>(rb.y)}, color});
    points.emplace_back(PrimitivePoint {{iround<int32>(lt.x), iround<int32>(lt.y)}, color});
    points.emplace_back(PrimitivePoint {{iround<int32>(rt.x), iround<int32>(rt.y)}, color});
    points.emplace_back(PrimitivePoint {{iround<int32>(rb.x), iround<int32>(rb.y)}, color});
}

void SpriteManager::InitializeEgg(hstring egg_name, AtlasType atlas_type)
{
    FO_STACK_TRACE_ENTRY();

    _eggValid = false;
    _eggHex = {};
    _eggOffset = {};

    auto any_spr = LoadSprite(egg_name, atlas_type);
    FO_RUNTIME_ASSERT(any_spr);

    _sprEgg = dynamic_ptr_cast<AtlasSprite>(std::move(any_spr));
    FO_RUNTIME_ASSERT(_sprEgg);

    const auto x = iround<int32>(_sprEgg->Atlas->MainTex->SizeData[0] * _sprEgg->AtlasRect.Left);
    const auto y = iround<int32>(_sprEgg->Atlas->MainTex->SizeData[1] * _sprEgg->AtlasRect.Top);
    _eggData = _sprEgg->Atlas->MainTex->GetTextureRegion({x, y}, _sprEgg->Size);
}

auto SpriteManager::CheckEggAppearence(mpos hex, EggAppearenceType appearence) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (appearence == EggAppearenceType::None) {
        return false;
    }
    if (appearence == EggAppearenceType::Always) {
        return true;
    }

    if (_eggHex.y == hex.y && (hex.x % 2) != 0 && (_eggHex.x % 2) == 0) {
        hex.y--;
    }

    switch (appearence) {
    case EggAppearenceType::ByX:
        if (hex.x >= _eggHex.x) {
            return true;
        }
        break;
    case EggAppearenceType::ByY:
        if (hex.y >= _eggHex.y) {
            return true;
        }
        break;
    case EggAppearenceType::ByXAndY:
        if (hex.x >= _eggHex.x || hex.y >= _eggHex.y) {
            return true;
        }
        break;
    case EggAppearenceType::ByXOrY:
        if (hex.x >= _eggHex.x && hex.y >= _eggHex.y) {
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

void SpriteManager::SetEgg(mpos hex, const MapSprite* mspr)
{
    FO_STACK_TRACE_ENTRY();

    const auto* spr = mspr->PSpr != nullptr ? *mspr->PSpr : mspr->Spr;
    FO_RUNTIME_ASSERT(spr);

    _eggOffset.x = mspr->HexOffset.x + spr->Offset.x - _sprEgg->Size.width / 2 + mspr->SprOffset->x + mspr->PHexOffset->x;
    _eggOffset.y = mspr->HexOffset.y - spr->Size.height / 2 + spr->Offset.y - _sprEgg->Size.height / 2 + mspr->SprOffset->y + mspr->PHexOffset->y;
    _eggHex = hex;
    _eggValid = true;
}

void SpriteManager::DrawSprites(MapSpriteList& mspr_list, bool collect_contours, bool use_egg, DrawOrderType draw_oder_from, DrawOrderType draw_oder_to, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    if (mspr_list.RootSprite() == nullptr) {
        return;
    }

    vector<PrimitivePoint> borders;

    if (!_eggValid) {
        use_egg = false;
    }

    const int32 ex = _eggOffset.x + _settings.ScreenOffset.x;
    const int32 ey = _eggOffset.y + _settings.ScreenOffset.y;

    for (const auto* mspr = mspr_list.RootSprite(); mspr != nullptr; mspr = mspr->ChainChild) {
        FO_RUNTIME_ASSERT(mspr->Valid);

        if (mspr->IsHidden()) {
            continue;
        }
        if (mspr->DrawOrder < draw_oder_from) {
            continue;
        }
        if (mspr->DrawOrder > draw_oder_to) {
            continue;
        }

        const auto* spr = mspr->PSpr != nullptr ? *mspr->PSpr : mspr->Spr;
        if (spr == nullptr) {
            continue;
        }

        // Coords
        int32 x = mspr->HexOffset.x - spr->Size.width / 2 + spr->Offset.x + mspr->PHexOffset->x + _settings.ScreenOffset.x;
        int32 y = mspr->HexOffset.y - spr->Size.height + spr->Offset.y + mspr->PHexOffset->y + _settings.ScreenOffset.y;

        if (mspr->SprOffset != nullptr) {
            x += mspr->SprOffset->x;
            y += mspr->SprOffset->y;
        }

        const auto zoom = _spritesZoom;

        // Base color
        ucolor color_r;
        ucolor color_l;

        if (mspr->Color != ucolor::clear) {
            color_r = color_l = ucolor {mspr->Color, 255};
        }
        else {
            color_r = color_l = color;
        }

        // Light
        if (mspr->Light != nullptr) {
            const auto mix_light = [](ucolor& c, const ucolor* l, const ucolor* l2) {
                c.comp.r = numeric_cast<uint8>(std::min(c.comp.r + (l->comp.r + l2->comp.r) / 2, 255));
                c.comp.g = numeric_cast<uint8>(std::min(c.comp.g + (l->comp.g + l2->comp.g) / 2, 255));
                c.comp.b = numeric_cast<uint8>(std::min(c.comp.b + (l->comp.b + l2->comp.b) / 2, 255));
            };

            mix_light(color_r, mspr->Light, mspr->LightRight);
            mix_light(color_l, mspr->Light, mspr->LightLeft);
        }

        // Alpha
        if (mspr->Alpha != nullptr) {
            reinterpret_cast<uint8*>(&color_r)[3] = *mspr->Alpha;
            reinterpret_cast<uint8*>(&color_l)[3] = *mspr->Alpha;
        }

        // Fix color
        color_r = ApplyColorBrightness(color_r);
        color_l = ApplyColorBrightness(color_l);

        // Check borders
        if (numeric_cast<float32>(x) / zoom > numeric_cast<float32>(_settings.ScreenWidth) || numeric_cast<float32>(x + spr->Size.width) / zoom < 0.0f || //
            numeric_cast<float32>(y) / zoom > numeric_cast<float32>(_settings.ScreenHeight - _settings.ScreenHudHeight) || numeric_cast<float32>(y + spr->Size.height) / zoom < 0.0f) {
            continue;
        }

        // Choose effect
        auto* effect = mspr->DrawEffect != nullptr ? *mspr->DrawEffect : nullptr;
        if (effect == nullptr) {
            effect = spr->DrawEffect != nullptr ? spr->DrawEffect : _effectMngr.Effects.Generic;
        }
        FO_RUNTIME_ASSERT(effect);

        // Fill buffer
        const auto xf = numeric_cast<float32>(x) / zoom;
        const auto yf = numeric_cast<float32>(y) / zoom;
        const auto wf = numeric_cast<float32>(spr->Size.width) / zoom;
        const auto hf = numeric_cast<float32>(spr->Size.height) / zoom;

        const auto ind_count = spr->FillData(_spritesDrawBuf.get(), {xf, yf, xf + wf, yf + hf}, {color_l, color_r});

        // Setup egg
        if (use_egg && ind_count == 6 && CheckEggAppearence(mspr->Hex, mspr->EggAppearence)) {
            auto x1 = x - ex;
            auto y1 = y - ey;
            auto x2 = x1 + spr->Size.width;
            auto y2 = y1 + spr->Size.height;

            if (x1 < _sprEgg->Size.width - 100 && y1 < _sprEgg->Size.height - 100 && x2 >= 100 && y2 >= 100) {
                if (const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(spr); atlas_spr != nullptr) {
                    x1 = std::max(x1, 0);
                    y1 = std::max(y1, 0);
                    x2 = std::min(x2, _sprEgg->Size.width);
                    y2 = std::min(y2, _sprEgg->Size.height);

                    const auto x1_f = _sprEgg->AtlasRect.Left + numeric_cast<float32>(x1) / _sprEgg->Atlas->MainTex->SizeData[0];
                    const auto x2_f = _sprEgg->AtlasRect.Left + numeric_cast<float32>(x2) / _sprEgg->Atlas->MainTex->SizeData[0];
                    const auto y1_f = _sprEgg->AtlasRect.Top + numeric_cast<float32>(y1) / _sprEgg->Atlas->MainTex->SizeData[1];
                    const auto y2_f = _sprEgg->AtlasRect.Top + numeric_cast<float32>(y2) / _sprEgg->Atlas->MainTex->SizeData[1];

                    auto& vbuf = _spritesDrawBuf->Vertices;
                    const auto vpos = _spritesDrawBuf->VertCount;

                    vbuf[vpos - 4].EggTexU = x1_f;
                    vbuf[vpos - 4].EggTexV = y2_f;
                    vbuf[vpos - 3].EggTexU = x1_f;
                    vbuf[vpos - 3].EggTexV = y1_f;
                    vbuf[vpos - 2].EggTexU = x2_f;
                    vbuf[vpos - 2].EggTexV = y1_f;
                    vbuf[vpos - 1].EggTexU = x2_f;
                    vbuf[vpos - 1].EggTexV = y2_f;
                }
            }
        }

        if (ind_count != 0) {
            if (_dipQueue.empty() || _dipQueue.back().MainTex != spr->GetBatchTex() || _dipQueue.back().SourceEffect != effect) {
                _dipQueue.emplace_back(DipData {spr->GetBatchTex(), effect, ind_count});
            }
            else {
                _dipQueue.back().IndCount += ind_count;
            }

            if (_spritesDrawBuf->VertCount >= _flushVertCount) {
                Flush();
            }
        }

        // Corners indication
        if (_settings.ShowCorners && mspr->EggAppearence != EggAppearenceType::None) {
            vector<PrimitivePoint> corner;
            const auto cx = wf / 2.0f;

            switch (mspr->EggAppearence) {
            case EggAppearenceType::Always:
                PrepareSquare(corner, FRect(xf + cx - 2.0f, yf + hf - 50.0f, xf + cx + 2.0f, yf + hf), ucolor {0x5F00FFFF});
                break;
            case EggAppearenceType::ByX:
                PrepareSquare(corner, fpos {xf + cx - 5.0f, yf + hf - 55.0f}, fpos {xf + cx + 5.0f, yf + hf - 45.0f}, fpos {xf + cx - 5.0f, yf + hf - 5.0f}, fpos {xf + cx + 5.0f, yf + hf + 5.0f}, ucolor {0x5F00AF00});
                break;
            case EggAppearenceType::ByY:
                PrepareSquare(corner, fpos {xf + cx - 5.0f, yf + hf - 49.0f}, fpos {xf + cx + 5.0f, yf + hf - 52.0f}, fpos {xf + cx - 5.0f, yf + hf + 1.0f}, fpos {xf + cx + 5.0f, yf + hf - 2.0f}, ucolor {0x5F00FF00});
                break;
            case EggAppearenceType::ByXAndY:
                PrepareSquare(corner, fpos {xf + cx - 10.0f, yf + hf - 49.0f}, fpos {xf + cx, yf + hf - 52.0f}, fpos {xf + cx - 10.0f, yf + hf + 1.0f}, fpos {xf + cx, yf + hf - 2.0f}, ucolor {0x5F0000FF});
                PrepareSquare(corner, fpos {xf + cx, yf + hf - 55.0f}, fpos {xf + cx + 10.0f, yf + hf - 45.0f}, fpos {xf + cx, yf + hf - 5.0f}, fpos {xf + cx + 10.0f, yf + hf + 5.0f}, ucolor {0x5F0000FF});
                break;
            case EggAppearenceType::ByXOrY:
                PrepareSquare(corner, fpos {xf + cx, yf + hf - 49.0f}, fpos {xf + cx + 10.0f, yf + hf - 52.0f}, fpos {xf + cx, yf + hf + 1.0f}, fpos {xf + cx + 10.0f, yf + hf - 2.0f}, ucolor {0x5F0000AF});
                PrepareSquare(corner, fpos {xf + cx - 10.0f, yf + hf - 55.0f}, fpos {xf + cx, yf + hf - 45.0f}, fpos {xf + cx - 10.0f, yf + hf - 5.0f}, fpos {xf + cx, yf + hf + 5.0f}, ucolor {0x5F0000AF});
                break;
            default:
                break;
            }

            DrawPoints(corner, RenderPrimitiveType::TriangleList);
        }

        // Draw order
        if (_settings.ShowDrawOrder) {
            const auto x1 = iround<int32>(numeric_cast<float32>(mspr->HexOffset.x + _settings.ScreenOffset.x) / zoom);
            auto y1 = iround<int32>(numeric_cast<float32>(mspr->HexOffset.y + _settings.ScreenOffset.y) / zoom);

            if (mspr->DrawOrder < DrawOrderType::NormalBegin || mspr->DrawOrder > DrawOrderType::NormalEnd) {
                y1 -= iround<int32>(40.0f / zoom);
            }

            DrawText({x1, y1, 100, 100}, strex("{}", mspr->TreeIndex), 0, COLOR_TEXT, -1);
        }

        // Process contour effect
        if (collect_contours && mspr->Contour != ContourType::None) {
            auto contour_color = ucolor {0, 0, 255};

            switch (mspr->Contour) {
            case ContourType::Red:
                contour_color = ucolor {175, 0, 0};
                break;
            case ContourType::Yellow:
                contour_color = ucolor {175, 175, 0};
                break;
            case ContourType::Custom:
                contour_color = mspr->ContourColor;
                break;
            default:
                break;
            }

            if (contour_color != ucolor::clear) {
                CollectContour({x, y}, spr, contour_color);
            }
        }

        if (_settings.ShowSpriteBorders && mspr->DrawOrder > DrawOrderType::Tile4) {
            auto rect = mspr->GetViewRect();

            rect.Left += _settings.ScreenOffset.x;
            rect.Right += _settings.ScreenOffset.x;
            rect.Top += _settings.ScreenOffset.y;
            rect.Bottom += _settings.ScreenOffset.y;

            PrepareSquare(borders, rect, ucolor {0, 0, 255, 50});
        }
    }

    Flush();

    if (_settings.ShowSpriteBorders) {
        DrawPoints(borders, RenderPrimitiveType::TriangleList);
    }
}

auto SpriteManager::SpriteHitTest(const Sprite* spr, ipos pos, bool with_zoom) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (pos.x < 0 || pos.y < 0) {
        return false;
    }

    if (with_zoom && _spritesZoom != 1.0f) {
        const auto zoomed_spr_x = iround<int32>(numeric_cast<float32>(pos.x) * _spritesZoom);
        const auto zoomed_spr_y = iround<int32>(numeric_cast<float32>(pos.y) * _spritesZoom);

        return spr->IsHitTest({zoomed_spr_x, zoomed_spr_y});
    }
    else {
        return spr->IsHitTest(pos);
    }
}

auto SpriteManager::IsEggTransp(ipos pos) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_eggValid) {
        return false;
    }

    const auto ex = _eggHex.x + _settings.ScreenOffset.x;
    const auto ey = _eggHex.y + _settings.ScreenOffset.y;
    auto ox = pos.x - iround<int32>(numeric_cast<float32>(ex) / _spritesZoom);
    auto oy = pos.y - iround<int32>(numeric_cast<float32>(ey) / _spritesZoom);

    if (ox < 0 || oy < 0 || //
        ox >= iround<int32>(numeric_cast<float32>(_sprEgg->Size.width) / _spritesZoom) || //
        oy >= iround<int32>(numeric_cast<float32>(_sprEgg->Size.height) / _spritesZoom)) {
        return false;
    }

    ox = iround<int32>(numeric_cast<float32>(ox) * _spritesZoom);
    oy = iround<int32>(numeric_cast<float32>(oy) * _spritesZoom);

    const auto egg_color = _eggData.at(oy * _sprEgg->Size.width + ox);
    return egg_color.comp.a < 127;
}

void SpriteManager::DrawPoints(const vector<PrimitivePoint>& points, RenderPrimitiveType prim, const float32* zoom, const fpos* offset, RenderEffect* custom_effect)
{
    FO_STACK_TRACE_ENTRY();

    if (points.empty()) {
        return;
    }

    Flush();

    auto* effect = custom_effect != nullptr ? custom_effect : _effectMngr.Effects.Primitive;
    FO_RUNTIME_ASSERT(effect);

    // Check primitives
    const auto count = points.size();
    auto prim_count = numeric_cast<int32>(count);

    switch (prim) {
    case RenderPrimitiveType::PointList:
        break;
    case RenderPrimitiveType::LineList:
        prim_count /= 2;
        break;
    case RenderPrimitiveType::LineStrip:
        prim_count -= 1;
        break;
    case RenderPrimitiveType::TriangleList:
        prim_count /= 3;
        break;
    case RenderPrimitiveType::TriangleStrip:
        prim_count -= 2;
        break;
    }
    if (prim_count <= 0) {
        return;
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

        auto x = numeric_cast<float32>(point.PointPos.x);
        auto y = numeric_cast<float32>(point.PointPos.y);

        if (point.PointOffset != nullptr) {
            x += numeric_cast<float32>(point.PointOffset->x);
            y += numeric_cast<float32>(point.PointOffset->y);
        }
        if (zoom != nullptr) {
            x /= *zoom;
            y /= *zoom;
        }
        if (offset != nullptr) {
            x += offset->x;
            y += offset->y;
        }

        vbuf[i].PosX = x;
        vbuf[i].PosY = y;
        vbuf[i].Color = point.PPointColor != nullptr ? *point.PPointColor : point.PointColor;

        ibuf[i] = numeric_cast<vindex_t>(i);
    }

    _primitiveDrawBuf->PrimType = prim;
    _primitiveDrawBuf->PrimZoomed = _spritesZoom != 1.0f;

    _primitiveDrawBuf->Upload(effect->GetUsage(), count, count);
    EnableScissor();
    effect->DrawBuffer(_primitiveDrawBuf.get(), 0, count);
    DisableScissor();
}

void SpriteManager::DrawContours()
{
    FO_STACK_TRACE_ENTRY();

    if (_contoursAdded && _rtContours != nullptr && _rtContoursMid != nullptr) {
        // Draw collected contours
        DrawRenderTarget(_rtContours, true);

        // Clean render targets
        _rtMngr.PushRenderTarget(_rtContours);
        _rtMngr.ClearCurrentRenderTarget(ucolor::clear);
        _rtMngr.PopRenderTarget();

        if (_contourClearMid) {
            _contourClearMid = false;

            _rtMngr.PushRenderTarget(_rtContoursMid);
            _rtMngr.ClearCurrentRenderTarget(ucolor::clear);
            _rtMngr.PopRenderTarget();
        }

        _contoursAdded = false;
    }
}

void SpriteManager::CollectContour(ipos pos, const Sprite* spr, ucolor contour_color)
{
    FO_STACK_TRACE_ENTRY();

    if (_rtContours == nullptr || _rtContoursMid == nullptr) {
        return;
    }

    const auto* as = dynamic_cast<const AtlasSprite*>(spr);
    if (as == nullptr) {
        return;
    }

    const auto is_dynamic_draw = spr->IsDynamicDraw();

    auto* contour_effect = is_dynamic_draw ? _effectMngr.Effects.ContourDynamicSprite : _effectMngr.Effects.ContourStrictSprite;
    if (contour_effect == nullptr) {
        return;
    }

    auto borders = IRect(pos.x - 1, pos.y - 1, pos.x + as->Size.width + 1, pos.y + as->Size.height + 1);
    const auto* texture = as->Atlas->MainTex;
    FRect textureuv;
    FRect sprite_border;

    const auto zoomed_screen_width = iround<int32>(numeric_cast<float32>(_settings.ScreenWidth) * _spritesZoom);
    const auto zoomed_screen_height = iround<int32>(numeric_cast<float32>(_settings.ScreenHeight - _settings.ScreenHudHeight) * _spritesZoom);
    if (borders.Left >= zoomed_screen_width || borders.Right < 0 || borders.Top >= zoomed_screen_height || borders.Bottom < 0) {
        return;
    }

    if (_spritesZoom == 1.0f) {
        if (is_dynamic_draw) {
            const auto& sr = as->AtlasRect;
            textureuv = sr;
            sprite_border = sr;
            borders.Left++;
            borders.Top++;
            borders.Right--;
            borders.Bottom--;
        }
        else {
            const auto& sr = as->AtlasRect;
            const float32 txw = texture->SizeData[2];
            const float32 txh = texture->SizeData[3];
            textureuv = FRect(sr.Left - txw, sr.Top - txh, sr.Right + txw, sr.Bottom + txh);
            sprite_border = sr;
        }
    }
    else {
        const auto& sr = as->AtlasRect;
        const auto zoomed_x = iround<int32>(numeric_cast<float32>(pos.x) / _spritesZoom);
        const auto zoomed_y = iround<int32>(numeric_cast<float32>(pos.y) / _spritesZoom);
        const auto zoomed_x2 = iround<int32>(numeric_cast<float32>(pos.x + as->Size.width) / _spritesZoom);
        const auto zoomed_y2 = iround<int32>(numeric_cast<float32>(pos.y + as->Size.height) / _spritesZoom);
        borders = {zoomed_x, zoomed_y, zoomed_x2, zoomed_y2};
        const auto bordersf = FRect(borders);
        const auto mid_height = _rtContoursMid->MainTex->SizeData[1];
        const auto flipped_height = _rtContoursMid->MainTex->FlippedHeight;

        _rtMngr.PushRenderTarget(_rtContoursMid);
        _contourClearMid = true;

        auto& vbuf = _flushDrawBuf->Vertices;
        size_t vpos = 0;

        vbuf[vpos].PosX = bordersf.Left;
        vbuf[vpos].PosY = flipped_height ? mid_height - bordersf.Bottom : bordersf.Bottom;
        vbuf[vpos].TexU = sr.Left;
        vbuf[vpos].TexV = sr.Bottom;
        vbuf[vpos++].EggTexU = 0.0f;

        vbuf[vpos].PosX = bordersf.Left;
        vbuf[vpos].PosY = flipped_height ? mid_height - bordersf.Top : bordersf.Top;
        vbuf[vpos].TexU = sr.Left;
        vbuf[vpos].TexV = sr.Top;
        vbuf[vpos++].EggTexU = 0.0f;

        vbuf[vpos].PosX = bordersf.Right;
        vbuf[vpos].PosY = flipped_height ? mid_height - bordersf.Top : bordersf.Top;
        vbuf[vpos].TexU = sr.Right;
        vbuf[vpos].TexV = sr.Top;
        vbuf[vpos++].EggTexU = 0.0f;

        vbuf[vpos].PosX = bordersf.Right;
        vbuf[vpos].PosY = flipped_height ? mid_height - bordersf.Bottom : bordersf.Bottom;
        vbuf[vpos].TexU = sr.Right;
        vbuf[vpos].TexV = sr.Bottom;
        vbuf[vpos].EggTexU = 0.0f;

        _flushDrawBuf->Upload(_effectMngr.Effects.FlushRenderTarget->GetUsage());
        _effectMngr.Effects.FlushRenderTarget->DrawBuffer(_flushDrawBuf.get());

        _rtMngr.PopRenderTarget();

        texture = _rtContoursMid->MainTex.get();
        const auto tw = texture->SizeData[0];
        const auto th = texture->SizeData[1];
        borders.Left--;
        borders.Top--;
        borders.Right++;
        borders.Bottom++;

        textureuv = FRect(borders);
        textureuv = {textureuv.Left / tw, textureuv.Top / th, textureuv.Right / tw, textureuv.Bottom / th};
        sprite_border = textureuv;
    }

    contour_color = ApplyColorBrightness(contour_color);

    const auto bordersf = FRect(borders);

    _rtMngr.PushRenderTarget(_rtContours);

    auto& vbuf = _contourDrawBuf->Vertices;
    size_t vpos = 0;

    vbuf[vpos].PosX = bordersf.Left;
    vbuf[vpos].PosY = bordersf.Bottom;
    vbuf[vpos].TexU = textureuv.Left;
    vbuf[vpos].TexV = textureuv.Bottom;
    vbuf[vpos].EggTexU = 0.0f;
    vbuf[vpos++].Color = contour_color;

    vbuf[vpos].PosX = bordersf.Left;
    vbuf[vpos].PosY = bordersf.Top;
    vbuf[vpos].TexU = textureuv.Left;
    vbuf[vpos].TexV = textureuv.Top;
    vbuf[vpos].EggTexU = 0.0f;
    vbuf[vpos++].Color = contour_color;

    vbuf[vpos].PosX = bordersf.Right;
    vbuf[vpos].PosY = bordersf.Top;
    vbuf[vpos].TexU = textureuv.Right;
    vbuf[vpos].TexV = textureuv.Top;
    vbuf[vpos].EggTexU = 0.0f;
    vbuf[vpos++].Color = contour_color;

    vbuf[vpos].PosX = bordersf.Right;
    vbuf[vpos].PosY = bordersf.Bottom;
    vbuf[vpos].TexU = textureuv.Right;
    vbuf[vpos].TexV = textureuv.Bottom;
    vbuf[vpos].EggTexU = 0.0f;
    vbuf[vpos].Color = contour_color;

    auto& contour_buf = contour_effect->ContourBuf = RenderEffect::ContourBuffer();

    contour_buf->SpriteBorder[0] = sprite_border[0];
    contour_buf->SpriteBorder[1] = sprite_border[1];
    contour_buf->SpriteBorder[2] = sprite_border[2];
    contour_buf->SpriteBorder[3] = sprite_border[3];

    _contourDrawBuf->Upload(contour_effect->GetUsage());
    contour_effect->DrawBuffer(_contourDrawBuf.get(), 0, std::nullopt, texture);

    _rtMngr.PopRenderTarget();
    _contoursAdded = true;
}

auto SpriteManager::ApplyColorBrightness(ucolor color) const -> ucolor
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_settings.Brightness != 0) {
        const auto r = std::clamp(numeric_cast<int32>(color.comp.r) + _settings.Brightness, 0, 255);
        const auto g = std::clamp(numeric_cast<int32>(color.comp.g) + _settings.Brightness, 0, 255);
        const auto b = std::clamp(numeric_cast<int32>(color.comp.b) + _settings.Brightness, 0, 255);
        return ucolor {numeric_cast<uint8>(r), numeric_cast<uint8>(g), numeric_cast<uint8>(b), color.comp.a};
    }
    else {
        return color;
    }
}

auto SpriteManager::GetFont(int32 num) -> FontData*
{
    FO_STACK_TRACE_ENTRY();

    if (num < 0) {
        num = _defFontIndex;
    }
    if (num < 0 || num >= numeric_cast<int32>(_allFonts.size())) {
        return nullptr;
    }
    return _allFonts[num].get();
}

void SpriteManager::ClearFonts()
{
    FO_STACK_TRACE_ENTRY();

    _allFonts.clear();
}

void SpriteManager::SetDefaultFont(int32 index)
{
    FO_STACK_TRACE_ENTRY();

    _defFontIndex = index;
}

void SpriteManager::SetFontEffect(int32 index, RenderEffect* effect)
{
    FO_STACK_TRACE_ENTRY();

    auto* font = GetFont(index);
    if (font != nullptr) {
        font->DrawEffect = effect != nullptr ? effect : _effectMngr.Effects.Font;
    }
}

void SpriteManager::BuildFont(int32 index)
{
    FO_STACK_TRACE_ENTRY();

    auto& font = *_allFonts[index];

    // Fix texture coordinates
    const auto* atlas_spr = font.ImageNormal.get();
    auto tex_w = numeric_cast<float32>(atlas_spr->Atlas->Size.width);
    auto tex_h = numeric_cast<float32>(atlas_spr->Atlas->Size.height);
    auto image_x = tex_w * atlas_spr->AtlasRect.Left;
    auto image_y = tex_h * atlas_spr->AtlasRect.Top;
    auto max_h = 0;

    for (auto& [letter_index, letter] : font.Letters) {
        const auto x = numeric_cast<float32>(letter.Pos.x);
        const auto y = numeric_cast<float32>(letter.Pos.y);
        const auto w = numeric_cast<float32>(letter.Size.width);
        const auto h = numeric_cast<float32>(letter.Size.height);

        letter.TexPos[0] = (image_x + x - 1.0f) / tex_w;
        letter.TexPos[1] = (image_y + y - 1.0f) / tex_h;
        letter.TexPos[2] = (image_x + x + w + 1.0f) / tex_w;
        letter.TexPos[3] = (image_y + y + h + 1.0f) / tex_h;

        max_h = std::max(letter.Size.height, max_h);
    }

    // Fill data
    font.FontTex = atlas_spr->Atlas->MainTex;

    if (font.LineHeight == 0) {
        font.LineHeight = max_h;
    }
    if (font.Letters.count(numeric_cast<uint32>(' ')) != 0) {
        font.SpaceWidth = font.Letters[numeric_cast<uint32>(' ')].XAdvance;
    }

    const auto* si_bordered = dynamic_cast<const AtlasSprite*>(font.ImageBordered ? font.ImageBordered.get() : nullptr);
    font.FontTexBordered = si_bordered != nullptr ? si_bordered->Atlas->MainTex : nullptr;

    const auto normal_ox = iround<int32>(tex_w * atlas_spr->AtlasRect.Left);
    const auto normal_oy = iround<int32>(tex_h * atlas_spr->AtlasRect.Top);
    const auto bordered_ox = (si_bordered != nullptr ? iround<int32>(numeric_cast<float32>(si_bordered->Atlas->Size.width) * si_bordered->AtlasRect.Left) : 0);
    const auto bordered_oy = (si_bordered != nullptr ? iround<int32>(numeric_cast<float32>(si_bordered->Atlas->Size.height) * si_bordered->AtlasRect.Top) : 0);

    // Read texture data
    const auto pixel_at = [](vector<ucolor>& tex_data, int32 width, int32 x, int32 y) -> ucolor& { return tex_data[y * width + x]; };
    vector<ucolor> data_normal = atlas_spr->Atlas->MainTex->GetTextureRegion({normal_ox, normal_oy}, atlas_spr->Size);
    vector<ucolor> data_bordered;

    if (si_bordered != nullptr) {
        data_bordered = si_bordered->Atlas->MainTex->GetTextureRegion({bordered_ox, bordered_oy}, si_bordered->Size);
    }

    // Normalize color to gray
    if (font.MakeGray) {
        for (auto y = 0; y < atlas_spr->Size.height; y++) {
            for (auto x = 0; x < atlas_spr->Size.width; x++) {
                const auto a = pixel_at(data_normal, atlas_spr->Size.width, x, y).comp.a;

                if (a != 0) {
                    pixel_at(data_normal, atlas_spr->Size.width, x, y) = ucolor {128, 128, 128, a};

                    if (si_bordered != nullptr) {
                        pixel_at(data_bordered, si_bordered->Size.width, x, y) = ucolor {128, 128, 128, a};
                    }
                }
                else {
                    pixel_at(data_normal, atlas_spr->Size.width, x, y) = ucolor {0, 0, 0, 0};

                    if (si_bordered != nullptr) {
                        pixel_at(data_bordered, si_bordered->Size.width, x, y) = ucolor {0, 0, 0, 0};
                    }
                }
            }
        }

        atlas_spr->Atlas->MainTex->UpdateTextureRegion({normal_ox, normal_oy}, atlas_spr->Size, data_normal.data());
    }

    // Fill border
    if (si_bordered != nullptr) {
        for (auto y = 1; y < si_bordered->Size.height - 2; y++) {
            for (auto x = 1; x < si_bordered->Size.width - 2; x++) {
                if (pixel_at(data_normal, atlas_spr->Size.width, x, y) != ucolor::clear) {
                    for (auto xx = -1; xx <= 1; xx++) {
                        for (auto yy = -1; yy <= 1; yy++) {
                            const auto ox = x + xx;
                            const auto oy = y + yy;

                            if (pixel_at(data_bordered, si_bordered->Size.width, ox, oy) == ucolor::clear) {
                                pixel_at(data_bordered, si_bordered->Size.width, ox, oy) = ucolor {0, 0, 0, 255};
                            }
                        }
                    }
                }
            }
        }

        si_bordered->Atlas->MainTex->UpdateTextureRegion({bordered_ox, bordered_oy}, si_bordered->Size, data_bordered.data());

        // Fix texture coordinates on bordered texture
        tex_w = numeric_cast<float32>(si_bordered->Atlas->Size.width);
        tex_h = numeric_cast<float32>(si_bordered->Atlas->Size.height);
        image_x = tex_w * si_bordered->AtlasRect.Left;
        image_y = tex_h * si_bordered->AtlasRect.Top;

        for (auto&& [letter_index, letter] : font.Letters) {
            const auto x = numeric_cast<float32>(letter.Pos.x);
            const auto y = numeric_cast<float32>(letter.Pos.y);
            const auto w = numeric_cast<float32>(letter.Size.width);
            const auto h = numeric_cast<float32>(letter.Size.height);
            letter.TexBorderedPos[0] = (image_x + x - 1.0f) / tex_w;
            letter.TexBorderedPos[1] = (image_y + y - 1.0f) / tex_h;
            letter.TexBorderedPos[2] = (image_x + x + w + 1.0f) / tex_w;
            letter.TexBorderedPos[3] = (image_y + y + h + 1.0f) / tex_h;
        }
    }
}

auto SpriteManager::LoadFontFO(int32 index, string_view font_name, AtlasType atlas_type, bool not_bordered, bool skip_if_loaded /* = true */) -> bool
{
    FO_STACK_TRACE_ENTRY();

    // Skip if loaded
    if (skip_if_loaded && index < numeric_cast<int32>(_allFonts.size()) && _allFonts[index]) {
        return true;
    }

    // Load font data
    const string fname = strex("Fonts/{}.fofnt", font_name);
    const auto file = _resources.ReadFile(fname);

    if (!file) {
        WriteLog("File '{}' not found", fname);
        return false;
    }

    auto font = SafeAlloc::MakeUnique<FontData>();
    font->DrawEffect = _effectMngr.Effects.Font;

    string image_name;

    // Parse data
    istringstream str(file.GetStr());
    string key;
    string letter_buf;
    FontData::Letter* cur_letter = nullptr;
    auto version = -1;

    while (!str.eof() && !str.fail()) {
        // Get key
        str >> key;

        // Cut off comments
        auto comment = key.find('#');

        if (comment != string::npos) {
            key.erase(comment);
        }

        comment = key.find(';');

        if (comment != string::npos) {
            key.erase(comment);
        }

        // Check version
        if (version == -1) {
            if (key != "Version") {
                WriteLog("Font '{}' 'Version' signature not found (used deprecated format of 'fofnt')", fname);
                return false;
            }

            str >> version;

            if (version > 2) {
                WriteLog("Font '{}' version {} not supported (try update client)", fname, version);
                return false;
            }

            continue;
        }

        // Get value
        if (key == "Image") {
            str >> image_name;
        }
        else if (key == "LineHeight") {
            str >> font->LineHeight;
        }
        else if (key == "YAdvance") {
            str >> font->YAdvance;
        }
        else if (key == "End") {
            break;
        }
        else if (key == "Letter") {
            std::getline(str, letter_buf, '\n');
            auto utf8_letter_begin = letter_buf.find('\'');

            if (utf8_letter_begin == string::npos) {
                WriteLog("Font '{}' invalid letter specification", fname);
                return false;
            }

            utf8_letter_begin++;

            size_t letter_len = letter_buf.length() - utf8_letter_begin;
            auto letter = utf8::Decode(letter_buf.c_str() + utf8_letter_begin, letter_len);

            if (!utf8::IsValid(letter)) {
                WriteLog("Font '{}' invalid UTF8 letter at  '{}'", fname, letter_buf);
                return false;
            }

            cur_letter = &font->Letters[letter];
        }

        if (cur_letter == nullptr) {
            continue;
        }

        if (key == "PositionX") {
            str >> cur_letter->Pos.x;
        }
        else if (key == "PositionY") {
            str >> cur_letter->Pos.y;
        }
        else if (key == "Width") {
            str >> cur_letter->Size.width;
        }
        else if (key == "Height") {
            str >> cur_letter->Size.height;
        }
        else if (key == "OffsetX") {
            str >> cur_letter->Offset.x;
        }
        else if (key == "OffsetY") {
            str >> cur_letter->Offset.y;
        }
        else if (key == "XAdvance") {
            str >> cur_letter->XAdvance;
        }
    }

    bool make_gray = false;

    if (image_name.back() == '*') {
        make_gray = true;
        image_name = image_name.substr(0, image_name.size() - 1);
    }

    font->MakeGray = make_gray;

    // Load image
    {
        image_name.insert(0, "Fonts/");

        font->ImageNormal = dynamic_ptr_cast<AtlasSprite>(LoadSprite(_hashResolver.ToHashedString(image_name), atlas_type));

        if (!font->ImageNormal) {
            WriteLog("Image file '{}' not found", image_name);
            return false;
        }

        _copyableSpriteCache.erase({_hashResolver.ToHashedString(image_name), atlas_type});
    }

    // Create bordered instance
    if (!not_bordered) {
        font->ImageBordered = dynamic_ptr_cast<AtlasSprite>(LoadSprite(_hashResolver.ToHashedString(image_name), atlas_type));

        if (!font->ImageBordered) {
            WriteLog("Can't load twice file '{}'", image_name);
            return false;
        }

        _copyableSpriteCache.erase({_hashResolver.ToHashedString(image_name), atlas_type});
    }

    // Register
    if (index >= numeric_cast<int32>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }

    _allFonts[index] = std::move(font);

    BuildFont(index);

    return true;
}

static constexpr auto MAKEUINT(uint8 ch0, uint8 ch1, uint8 ch2, uint8 ch3) -> uint32
{
    return ch0 | ch1 << 8 | ch2 << 16 | ch3 << 24;
}

auto SpriteManager::LoadFontBmf(int32 index, string_view font_name, AtlasType atlas_type) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (index < 0) {
        WriteLog("Invalid index");
        return false;
    }

    auto font = SafeAlloc::MakeUnique<FontData>();
    font->DrawEffect = _effectMngr.Effects.Font;

    const auto file = _resources.ReadFile(strex("Fonts/{}.fnt", font_name));

    if (!file) {
        WriteLog("Font file '{}.fnt' not found", font_name);
        return false;
    }

    auto reader = file.GetReader();

    const auto signature = reader.GetLEUInt32();

    if (signature != MAKEUINT('B', 'M', 'F', 3)) {
        WriteLog("Invalid signature of font '{}'", font_name);
        return false;
    }

    // Info
    reader.GoForward(1);
    auto block_len = reader.GetLEUInt32();
    auto next_block = block_len + reader.GetCurPos() + 1;

    reader.GoForward(7);

    if (reader.GetBEUInt32() != 0x01010101u) {
        WriteLog("Wrong padding in font '{}'", font_name);
        return false;
    }

    // Common
    reader.SetCurPos(next_block);
    block_len = reader.GetLEUInt32();
    next_block = block_len + reader.GetCurPos() + 1;

    const int32 line_height = reader.GetLEUInt16();
    const int32 base_height = reader.GetLEUInt16();
    reader.GoForward(2); // Texture width
    reader.GoForward(2); // Texture height

    if (reader.GetLEUInt16() != 1) {
        WriteLog("Texture for font '{}' must be one", font_name);
        return false;
    }

    // Pages
    reader.SetCurPos(next_block);
    block_len = reader.GetLEUInt32();
    next_block = block_len + reader.GetCurPos() + 1;

    // Image name
    auto image_name = reader.GetStrNT();
    image_name.insert(0, "Fonts/");

    // Chars
    reader.SetCurPos(next_block);
    const auto count = reader.GetLEUInt32() / 20;

    for ([[maybe_unused]] const auto i : xrange(count)) {
        // Read data
        const auto id = reader.GetLEUInt32();
        const auto x = reader.GetLEUInt16();
        const auto y = reader.GetLEUInt16();
        const auto w = reader.GetLEUInt16();
        const auto h = reader.GetLEUInt16();
        const auto ox = reader.GetLEUInt16();
        const auto oy = reader.GetLEUInt16();
        const auto xa = reader.GetLEUInt16();
        reader.GoForward(2);

        // Fill data
        auto& let = font->Letters[id];
        let.Pos.x = x + 1;
        let.Pos.y = y + 1;
        let.Size.width = w - 2;
        let.Size.height = h - 2;
        let.Offset.x = -numeric_cast<int32>(ox);
        let.Offset.y = -numeric_cast<int32>(oy) + (line_height - base_height);
        let.XAdvance = xa + 1;
    }

    font->LineHeight = font->Letters.count(numeric_cast<uint32>('W')) != 0 ? font->Letters[numeric_cast<uint32>('W')].Size.height : base_height;
    font->YAdvance = font->LineHeight / 2;
    font->MakeGray = true;

    // Load image
    {
        font->ImageNormal = dynamic_ptr_cast<AtlasSprite>(LoadSprite(_hashResolver.ToHashedString(image_name), atlas_type));

        if (!font->ImageNormal) {
            WriteLog("Image file '{}' not found", image_name);
            return false;
        }

        _copyableSpriteCache.erase({_hashResolver.ToHashedString(image_name), atlas_type});
    }

    // Create bordered instance
    {
        font->ImageBordered = dynamic_ptr_cast<AtlasSprite>(LoadSprite(_hashResolver.ToHashedString(image_name), atlas_type));

        if (!font->ImageBordered) {
            WriteLog("Can't load twice file '{}'", image_name);
            return false;
        }

        _copyableSpriteCache.erase({_hashResolver.ToHashedString(image_name), atlas_type});
    }

    // Register
    if (index >= numeric_cast<int32>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }

    _allFonts[index] = std::move(font);

    BuildFont(index);

    return true;
}

static void StrCopy(char* to, size_t size, string_view from)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(to);
    FO_RUNTIME_ASSERT(size > 0);

    if (from.empty()) {
        to[0] = 0;
        return;
    }

    if (from.length() >= size) {
        MemCopy(to, from.data(), size - 1);
        to[size - 1] = 0;
    }
    else {
        MemCopy(to, from.data(), from.length());
        to[from.length()] = 0;
    }
}

template<int Size>
static void StrCopy(char (&to)[Size], string_view from)
{
    FO_STACK_TRACE_ENTRY();

    return StrCopy(to, Size, from);
}

static void StrGoTo(char*& str, char ch)
{
    FO_STACK_TRACE_ENTRY();

    while (*str != 0 && *str != ch) {
        ++str;
    }
}

static void StrEraseInterval(char* str, int32 len)
{
    FO_STACK_TRACE_ENTRY();

    if (str == nullptr || len == 0) {
        return;
    }

    const auto* str2 = str + len;
    while (*str2 != 0) {
        *str = *str2;
        ++str;
        ++str2;
    }

    *str = 0;
}

static void StrInsert(char* to, const char* from, int32 from_len)
{
    FO_STACK_TRACE_ENTRY();

    if (to == nullptr || from == nullptr) {
        return;
    }

    if (from_len == 0) {
        from_len = numeric_cast<int32>(string_view(from).length());
    }
    if (from_len == 0) {
        return;
    }

    auto* end_to = to;
    while (*end_to != 0) {
        ++end_to;
    }

    for (; end_to >= to; --end_to) {
        *(end_to + from_len) = *end_to;
    }

    while (from_len-- != 0) {
        *to = *from;
        ++to;
        ++from;
    }
}

void SpriteManager::FormatText(FontFormatInfo& fi, int32 fmt_type)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    fi.PStr = fi.Str;

    auto* str = fi.PStr;
    auto flags = fi.Flags;
    auto* font = fi.CurFont;
    auto r = IRect {fi.Rect.x, fi.Rect.y, fi.Rect.x + fi.Rect.width, fi.Rect.y + fi.Rect.height};
    auto infinity_w = r.Left == r.Right;
    auto infinity_h = r.Top == r.Bottom;
    auto curx = 0;
    auto cury = 0;
    auto& offs_col = fi.OffsColDots;

    if (fmt_type != FORMAT_TYPE_DRAW && fmt_type != FORMAT_TYPE_LCOUNT && fmt_type != FORMAT_TYPE_SPLIT) {
        fi.IsError = true;
        return;
    }

    if (fmt_type == FORMAT_TYPE_SPLIT && fi.StrLines == nullptr) {
        fi.IsError = true;
        return;
    }

    // Colorize
    ucolor* dots = nullptr;
    int32 d_offs = 0;
    string buf;

    if (fmt_type == FORMAT_TYPE_DRAW && !IsBitSet(flags, FT_NO_COLORIZE)) {
        dots = fi.ColorDots;
    }

    constexpr int32 dots_history_len = 10;
    ucolor dots_history[dots_history_len] = {fi.DefColor};
    int32 dots_history_cur = 0;

    for (auto* str_ = str; *str_ != 0;) {
        auto* s0 = str_;
        StrGoTo(str_, '|');
        auto* s1 = str_;
        StrGoTo(str_, ' ');
        auto* s2 = str_;

        if (dots != nullptr) {
            auto d_len = numeric_cast<int32>(s2 - s1) + 1;
            ucolor d;

            if (d_len == 2) {
                if (dots_history_cur > 0) {
                    d = dots_history[--dots_history_cur];
                }
                else {
                    d = fi.DefColor;
                }
            }
            else {
                if (*(s1 + 1) == 'x') {
                    d = ucolor {numeric_cast<uint32>(std::strtoul(s1 + 2, nullptr, 16))};
                }
                else {
                    d = ucolor {numeric_cast<uint32>(std::strtoul(s1 + 1, nullptr, 0))};
                }

                if (dots_history_cur < dots_history_len - 1) {
                    dots_history[++dots_history_cur] = d;
                }
            }

            dots[numeric_cast<int32>(s1 - str) - d_offs] = d;
            d_offs += d_len;
        }

        *s1 = 0;
        buf += s0;

        if (*str_ == 0) {
            break;
        }
        str_++;
    }

    StrCopy(fi.PStr, FONT_BUF_LEN, buf);

    // Skip lines
    auto skip_line = IsBitSet(flags, FT_SKIPLINES(0)) ? flags >> 16 : 0;
    auto skip_line_end = IsBitSet(flags, FT_SKIPLINES_END(0)) ? flags >> 16 : 0;

    // Format
    curx = r.Left;
    cury = r.Top;

    for (auto i = 0, i_advance = 1; str[i] != 0; i += i_advance) {
        auto letter_len = utf8::DecodeStrNtLen(&str[i]);
        auto letter = utf8::Decode(&str[i], letter_len);
        letter = utf8::IsValid(letter) ? letter : 0;
        i_advance = numeric_cast<int32>(letter_len);

        auto x_advance = 0;
        switch (letter) {
        case '\r':
            continue;
        case ' ':
            x_advance = font->SpaceWidth;
            break;
        case '\t':
            x_advance = font->SpaceWidth * 4;
            break;
        default:
            auto it = font->Letters.find(letter);
            if (it != font->Letters.end()) {
                x_advance = it->second.XAdvance;
            }
            else {
                x_advance = 0;
            }
            break;
        }

        if (!infinity_w && curx + x_advance > r.Right) {
            fi.MaxCurX = std::max(curx, fi.MaxCurX);

            if (fmt_type == FORMAT_TYPE_DRAW && IsBitSet(flags, FT_NOBREAK)) {
                str[i] = 0;
                break;
            }

            if (IsBitSet(flags, FT_NOBREAK_LINE)) {
                auto j = i;

                for (; str[j] != 0; j++) {
                    if (str[j] == '\n') {
                        break;
                    }
                }

                StrEraseInterval(&str[i], j - i);
                letter = numeric_cast<uint8>(str[i]);
                i_advance = 1;

                if (fmt_type == FORMAT_TYPE_DRAW) {
                    for (auto k = i, l = FONT_BUF_LEN - (j - i); k < l; k++) {
                        fi.ColorDots[k] = fi.ColorDots[k + (j - i)];
                    }
                }
            }
            else if (str[i] != '\n') {
                auto j = i;

                for (; j >= 0; j--) {
                    if (str[j] == ' ' || str[j] == '\t') {
                        str[j] = '\n';
                        i = j;
                        letter = '\n';
                        i_advance = 1;
                        break;
                    }
                    if (str[j] == '\n') {
                        j = -1;
                        break;
                    }
                }

                if (j < 0) {
                    letter = '\n';
                    i_advance = 1;
                    StrInsert(&str[i], "\n", 0);

                    if (fmt_type == FORMAT_TYPE_DRAW) {
                        for (auto k = FONT_BUF_LEN - 1; k > i; k--) {
                            fi.ColorDots[k] = fi.ColorDots[k - 1];
                        }
                    }
                }

                if (IsBitSet(flags, FT_ALIGN) && skip_line == 0) {
                    fi.LineSpaceWidth[fi.LinesAll - 1] = 1;

                    // Erase next first spaces
                    auto ii = i + i_advance;

                    for (j = ii;; j++) {
                        if (str[j] != ' ') {
                            break;
                        }
                    }

                    if (j > ii) {
                        StrEraseInterval(&str[ii], j - ii);
                        if (fmt_type == FORMAT_TYPE_DRAW) {
                            for (auto k = ii, l = FONT_BUF_LEN - (j - ii); k < l; k++) {
                                fi.ColorDots[k] = fi.ColorDots[k + (j - ii)];
                            }
                        }
                    }
                }
            }
        }

        switch (letter) {
        case '\n':
            if (skip_line == 0) {
                cury += font->LineHeight + font->YAdvance;
                if (!infinity_h && cury + font->LineHeight > r.Bottom && fi.LinesInRect == 0) {
                    fi.LinesInRect = fi.LinesAll;
                }

                if (fmt_type == FORMAT_TYPE_DRAW) {
                    if (fi.LinesInRect != 0 && !IsBitSet(flags, FT_UPPER)) {
                        // fi.LinesAll++;
                        str[i] = 0;
                        break;
                    }
                }
                else if (fmt_type == FORMAT_TYPE_SPLIT) {
                    if (fi.LinesInRect != 0 && fi.LinesAll % fi.LinesInRect == 0) {
                        str[i] = 0;
                        fi.StrLines->emplace_back(str);
                        str = &str[i + i_advance];
                        i = -i_advance;
                    }
                }

                if (str[i + i_advance] != 0) {
                    fi.LinesAll++;
                }
            }
            else {
                skip_line--;
                StrEraseInterval(str, i + i_advance);
                offs_col += i + i_advance;
                i = 0;
                i_advance = 0;
            }

            fi.MaxCurX = std::max(curx, fi.MaxCurX);
            curx = r.Left;
            continue;
        case 0:
            break;
        default:
            curx += x_advance;
            continue;
        }

        if (str[i] == 0) {
            break;
        }
    }

    fi.MaxCurX = std::max(curx, fi.MaxCurX);

    if (skip_line_end != 0) {
        auto len = numeric_cast<int32>(string_view(str).length());

        for (auto i = len - 2; i >= 0; i--) {
            if (str[i] == '\n') {
                str[i] = 0;
                fi.LinesAll--;
                if (--skip_line_end == 0) {
                    break;
                }
            }
        }

        if (skip_line_end != 0) {
            WriteLog("error3");
            fi.IsError = true;
            return;
        }
    }

    if (skip_line != 0) {
        WriteLog("error4");
        fi.IsError = true;
        return;
    }

    if (fi.LinesInRect == 0) {
        fi.LinesInRect = fi.LinesAll;
    }

    if (fi.LinesAll > FONT_MAX_LINES) {
        WriteLog("error5 {}", fi.LinesAll);
        fi.IsError = true;
        return;
    }

    if (fmt_type == FORMAT_TYPE_SPLIT) {
        fi.StrLines->emplace_back(str);
        return;
    }
    if (fmt_type == FORMAT_TYPE_LCOUNT) {
        return;
    }

    // Up text
    if (IsBitSet(flags, FT_UPPER) && fi.LinesAll > fi.LinesInRect) {
        int32 j = 0;
        int32 line_cur = 0;
        ucolor last_color;

        for (; str[j] != 0; ++j) {
            if (str[j] == '\n') {
                line_cur++;

                if (line_cur >= fi.LinesAll - fi.LinesInRect) {
                    break;
                }
            }

            if (fi.ColorDots[j] != ucolor::clear) {
                last_color = fi.ColorDots[j];
            }
        }

        if (!IsBitSet(flags, FT_NO_COLORIZE)) {
            offs_col += j + 1;

            if (last_color != ucolor::clear && fi.ColorDots[j + 1] == ucolor::clear) {
                fi.ColorDots[j + 1] = last_color;
            }
        }

        str = &str[j + 1];
        fi.PStr = str;

        fi.LinesAll = fi.LinesInRect;
    }

    // Align
    curx = r.Left;
    cury = r.Top;

    for (const auto i : xrange(fi.LinesAll)) {
        fi.LineWidth[i] = numeric_cast<int16>(curx);
    }

    bool can_count = false;
    int32 spaces = 0;
    int32 curstr = 0;

    for (auto i = 0, i_advance = 1;; i += i_advance) {
        auto letter_len = utf8::DecodeStrNtLen(&str[i]);
        auto letter = utf8::Decode(&str[i], letter_len);
        letter = utf8::IsValid(letter) ? letter : 0;
        i_advance = numeric_cast<int32>(letter_len);

        switch (letter) {
        case ' ':
            curx += font->SpaceWidth;
            if (can_count) {
                spaces++;
            }
            break;
        case '\t':
            curx += font->SpaceWidth * 4;
            break;
        case 0:
        case '\n':
            fi.LineWidth[curstr] = numeric_cast<int16>(curx);
            cury += font->LineHeight + font->YAdvance;
            curx = r.Left;

            // Align
            if (fi.LineSpaceWidth[curstr] == 1 && spaces > 0 && !infinity_w) {
                fi.LineSpaceWidth[curstr] = font->SpaceWidth + (r.Right - fi.LineWidth[curstr]) / spaces;
            }
            else {
                fi.LineSpaceWidth[curstr] = font->SpaceWidth;
            }

            curstr++;
            can_count = false;
            spaces = 0;
            break;
        case '\r':
            break;
        default:
            auto it = font->Letters.find(letter);
            if (it != font->Letters.end()) {
                curx += it->second.XAdvance;
            }
            can_count = true;
            break;
        }

        if (str[i] == 0) {
            break;
        }
    }

    // Initial position
    fi.CurX = r.Left;
    fi.CurY = r.Top;

    // Align X
    if (IsBitSet(flags, FT_CENTERX)) {
        fi.CurX += (r.Right - fi.LineWidth[0]) / 2;
    }
    else if (IsBitSet(flags, FT_CENTERR)) {
        fi.CurX += r.Right - fi.LineWidth[0];
    }
    // Align Y
    if (IsBitSet(flags, FT_CENTERY)) {
        fi.CurY = r.Top + (r.Bottom - r.Top + 1 - fi.LinesInRect * font->LineHeight - (fi.LinesInRect - 1) * font->YAdvance) / 2 + (IsBitSet(flags, FT_CENTERY_ENGINE) ? 1 : 0);
    }
    else if (IsBitSet(flags, FT_BOTTOM)) {
        fi.CurY = r.Bottom - (fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance);
    }
}

void SpriteManager::DrawText(irect32 rect, string_view str, uint32 flags, ucolor color, int32 num_font)
{
    FO_STACK_TRACE_ENTRY();

    // Check
    if (str.empty()) {
        return;
    }

    // Get font
    auto* font = GetFont(num_font);

    if (font == nullptr) {
        return;
    }

    // Format
    color = ApplyColorBrightness(color);

    auto& fi = _fontFormatInfoBuf = {font, flags, rect};
    StrCopy(fi.Str, str);
    fi.DefColor = color;

    FormatText(fi, FORMAT_TYPE_DRAW);

    if (fi.IsError) {
        return;
    }

    const auto* str_ = fi.PStr;
    const auto offs_col = fi.OffsColDots;
    auto curx = fi.CurX;
    auto cury = fi.CurY;
    auto curstr = 0;
    auto* texture = IsBitSet(flags, FT_BORDERED) && font->FontTexBordered != nullptr ? font->FontTexBordered : font->FontTex;

    if (!IsBitSet(flags, FT_NO_COLORIZE)) {
        for (auto i = offs_col; i >= 0; i--) {
            if (fi.ColorDots[i] != ucolor::clear) {
                if (fi.ColorDots[i].comp.a != 0) {
                    color = fi.ColorDots[i]; // With alpha
                }
                else {
                    color = ucolor {fi.ColorDots[i], color.comp.a};
                }
                color = ApplyColorBrightness(color);
                break;
            }
        }
    }

    const auto str_len = string_view(str_).length();
    _spritesDrawBuf->CheckAllocBuf(str_len * 4, str_len * 6);

    auto& vbuf = _spritesDrawBuf->Vertices;
    auto& vpos = _spritesDrawBuf->VertCount;
    auto& ibuf = _spritesDrawBuf->Indices;
    auto& ipos = _spritesDrawBuf->IndCount;

    const auto start_ipos = ipos;

    auto variable_space = false;
    int32 i_advance;

    for (auto i = 0; str_[i] != 0; i += i_advance) {
        if (!IsBitSet(flags, FT_NO_COLORIZE)) {
            const auto new_color = fi.ColorDots[i + offs_col];

            if (new_color != ucolor::clear) {
                if (new_color.comp.a != 0) {
                    color = new_color; // With alpha
                }
                else {
                    color = ucolor {new_color, color.comp.a};
                }

                color = ApplyColorBrightness(color);
            }
        }

        auto letter_len = utf8::DecodeStrNtLen(&str_[i]);
        auto letter = utf8::Decode(&str_[i], letter_len);
        letter = utf8::IsValid(letter) ? letter : 0;
        i_advance = numeric_cast<int32>(letter_len);

        switch (letter) {
        case ' ':
            curx += variable_space ? fi.LineSpaceWidth[curstr] : font->SpaceWidth;
            continue;
        case '\t':
            curx += font->SpaceWidth * 4;
            continue;
        case '\n':
            cury += font->LineHeight + font->YAdvance;
            curx = rect.x;
            curstr++;
            variable_space = false;
            if (IsBitSet(flags, FT_CENTERX)) {
                curx += (rect.x + rect.width - fi.LineWidth[curstr]) / 2;
            }
            else if (IsBitSet(flags, FT_CENTERR)) {
                curx += rect.x + rect.width - fi.LineWidth[curstr];
            }
            continue;
        case '\r':
            continue;
        default:
            auto it = font->Letters.find(letter);
            if (it == font->Letters.end()) {
                continue;
            }

            const auto& l = it->second;

            const auto x = curx - l.Offset.x - 1;
            const auto y = cury - l.Offset.y - 1;
            const auto w = l.Size.width + 2;
            const auto h = l.Size.height + 2;

            const auto& texture_uv = IsBitSet(flags, FT_BORDERED) ? l.TexBorderedPos : l.TexPos;
            const auto x1 = texture_uv[0];
            const auto y1 = texture_uv[1];
            const auto x2 = texture_uv[2];
            const auto y2 = texture_uv[3];

            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 0);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 2);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);

            auto& v0 = vbuf[vpos++];
            v0.PosX = numeric_cast<float32>(x);
            v0.PosY = numeric_cast<float32>(y + h);
            v0.TexU = x1;
            v0.TexV = y2;
            v0.EggTexU = 0.0f;
            v0.Color = color;

            auto& v1 = vbuf[vpos++];
            v1.PosX = numeric_cast<float32>(x);
            v1.PosY = numeric_cast<float32>(y);
            v1.TexU = x1;
            v1.TexV = y1;
            v1.EggTexU = 0.0f;
            v1.Color = color;

            auto& v2 = vbuf[vpos++];
            v2.PosX = numeric_cast<float32>(x + w);
            v2.PosY = numeric_cast<float32>(y);
            v2.TexU = x2;
            v2.TexV = y1;
            v2.EggTexU = 0.0f;
            v2.Color = color;

            auto& v3 = vbuf[vpos++];
            v3.PosX = numeric_cast<float32>(x + w);
            v3.PosY = numeric_cast<float32>(y + h);
            v3.TexU = x2;
            v3.TexV = y2;
            v3.EggTexU = 0.0f;
            v3.Color = color;

            curx += l.XAdvance;
            variable_space = true;
        }
    }

    const auto ind_count = ipos - start_ipos;

    if (ind_count != 0) {
        if (_dipQueue.empty() || _dipQueue.back().MainTex != texture || _dipQueue.back().SourceEffect != font->DrawEffect) {
            _dipQueue.emplace_back(DipData {texture, font->DrawEffect, ind_count});
        }
        else {
            _dipQueue.back().IndCount += ind_count;
        }

        if (_spritesDrawBuf->VertCount >= _flushVertCount) {
            Flush();
        }
    }
}

auto SpriteManager::GetLinesCount(isize size, string_view str, int32 num_font /* = -1 */) -> int32
{
    FO_STACK_TRACE_ENTRY();

    if (size.width <= 0 || size.height <= 0) {
        return 0;
    }

    auto* font = GetFont(num_font);

    if (font == nullptr) {
        return 0;
    }

    if (str.empty()) {
        return size.height / (font->LineHeight + font->YAdvance);
    }

    const auto width = size.width != 0 ? size.width : _settings.ScreenWidth;
    const auto height = size.height != 0 ? size.height : _settings.ScreenHeight;

    auto& fi = _fontFormatInfoBuf = {font, 0, {0, 0, width, height}};
    StrCopy(fi.Str, str);

    FormatText(fi, FORMAT_TYPE_LCOUNT);

    if (fi.IsError) {
        return 0;
    }

    return fi.LinesInRect;
}

auto SpriteManager::GetLinesHeight(isize size, string_view str, int32 num_font /* = -1 */) -> int32
{
    FO_STACK_TRACE_ENTRY();

    if (size.width <= 0 || size.height <= 0) {
        return 0;
    }

    const auto* font = GetFont(num_font);

    if (font == nullptr) {
        return 0;
    }

    const auto lines_count = GetLinesCount(size, str, num_font);

    if (lines_count <= 0) {
        return 0;
    }

    return lines_count * font->LineHeight + (lines_count - 1) * font->YAdvance;
}

auto SpriteManager::GetLineHeight(int32 num_font) -> int32
{
    FO_STACK_TRACE_ENTRY();

    const auto* font = GetFont(num_font);

    if (font == nullptr) {
        return 0;
    }

    return font->LineHeight;
}

auto SpriteManager::GetTextInfo(isize size, string_view str, int32 num_font, uint32 flags, isize& result_size, int32& lines) -> bool
{
    FO_STACK_TRACE_ENTRY();

    result_size = {};
    lines = {};

    auto* font = GetFont(num_font);

    if (font == nullptr) {
        return false;
    }

    if (str.empty()) {
        return true;
    }

    auto& fi = _fontFormatInfoBuf = {font, flags, {0, 0, size}};
    StrCopy(fi.Str, str);

    FormatText(fi, FORMAT_TYPE_LCOUNT);

    if (fi.IsError) {
        return false;
    }

    result_size = {fi.MaxCurX - fi.Rect.x, fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance};
    lines = fi.LinesInRect;

    return true;
}

auto SpriteManager::SplitLines(irect32 rect, string_view cstr, int32 num_font) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> result;

    if (cstr.empty()) {
        return {};
    }

    auto* font = GetFont(num_font);

    if (font == nullptr) {
        return {};
    }

    auto& fi = _fontFormatInfoBuf = {font, 0, rect};
    StrCopy(fi.Str, cstr);
    fi.StrLines = &result;

    FormatText(fi, FORMAT_TYPE_SPLIT);

    if (fi.IsError) {
        return {};
    }

    return result;
}

auto SpriteManager::HaveLetter(int32 num_font, uint32 letter) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto* font = GetFont(num_font);

    if (font == nullptr) {
        return false;
    }

    return font->Letters.count(letter) != 0;
}

FO_END_NAMESPACE();
