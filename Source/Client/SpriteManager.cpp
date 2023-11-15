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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "DiskFileSystem.h"
#include "Log.h"
#include "StringUtils.h"

static auto ApplyColorBrightness(uint color, int brightness) -> uint
{
    STACK_TRACE_ENTRY();

    if (brightness != 0) {
        const auto r = std::clamp(((color >> 16) & 0xFF) + brightness, 0u, 255u);
        const auto g = std::clamp(((color >> 8) & 0xFF) + brightness, 0u, 255u);
        const auto b = std::clamp(((color >> 0) & 0xFF) + brightness, 0u, 255u);
        return COLOR_RGBA((color >> 24) & 0xFF, r, g, b);
    }
    else {
        return color;
    }
}

Sprite::Sprite(SpriteManager& spr_mngr) :
    _sprMngr {spr_mngr}
{
    STACK_TRACE_ENTRY();
}

auto Sprite::IsHitTest(int x, int y) const -> bool
{
    UNUSED_VARIABLE(x, y);

    return false;
}

void Sprite::StartUpdate()
{
    STACK_TRACE_ENTRY();
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
    STACK_TRACE_ENTRY();

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
    _rtMain = _rtMngr.CreateRenderTarget(false, RenderTarget::SizeType::Screen, 0, 0, true);
#endif
    _rtContours = _rtMngr.CreateRenderTarget(false, RenderTarget::SizeType::Map, 0, 0, false);
    _rtContoursMid = _rtMngr.CreateRenderTarget(false, RenderTarget::SizeType::Map, 0, 0, false);

    _eventUnsubscriber += App->OnLowMemory += [this] { CleanupSpriteCache(); };
}

SpriteManager::~SpriteManager()
{
    STACK_TRACE_ENTRY();

    _window->Destroy();
}

auto SpriteManager::GetWindowSize() const -> tuple<int, int>
{
    STACK_TRACE_ENTRY();

    return _window->GetSize();
}

void SpriteManager::SetWindowSize(int w, int h)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _window->SetSize(w, h);
}

auto SpriteManager::GetScreenSize() const -> tuple<int, int>
{
    STACK_TRACE_ENTRY();

    return _window->GetScreenSize();
}

void SpriteManager::SetScreenSize(int w, int h)
{
    STACK_TRACE_ENTRY();

    const auto diff_w = w - App->Settings.ScreenWidth;
    const auto diff_h = h - App->Settings.ScreenHeight;

    if (!IsFullscreen()) {
        const auto [x, y] = _window->GetPosition();
        _window->SetPosition(x - diff_w / 2, y - diff_h / 2);
    }
    else {
        _windowSizeDiffX += diff_w / 2;
        _windowSizeDiffY += diff_h / 2;
    }

    _window->SetScreenSize(w, h);
}

void SpriteManager::ToggleFullscreen()
{
    STACK_TRACE_ENTRY();

    if (!IsFullscreen()) {
        _window->ToggleFullscreen(true);
    }
    else {
        if (_window->ToggleFullscreen(false)) {
            if (_windowSizeDiffX != 0 || _windowSizeDiffY != 0) {
                const auto [x, y] = _window->GetPosition();
                _window->SetPosition(x - _windowSizeDiffX, y - _windowSizeDiffY);
                _windowSizeDiffX = 0;
                _windowSizeDiffY = 0;
            }
        }
    }
}

void SpriteManager::SetMousePosition(int x, int y)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    App->Input.SetMousePosition(x, y, _window);
}

auto SpriteManager::IsFullscreen() const -> bool
{
    STACK_TRACE_ENTRY();

    return _window->IsFullscreen();
}

auto SpriteManager::IsWindowFocused() const -> bool
{
    STACK_TRACE_ENTRY();

    return _window->IsFocused();
}

void SpriteManager::MinimizeWindow()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _window->Minimize();
}

void SpriteManager::BlinkWindow()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _window->Blink();
}

void SpriteManager::SetAlwaysOnTop(bool enable)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    _window->AlwaysOnTop(enable);
}

void SpriteManager::RegisterSpriteFactory(unique_ptr<SpriteFactory>&& factory)
{
    STACK_TRACE_ENTRY();

    for (auto&& ext : factory->GetExtensions()) {
        _spriteFactoryMap[ext] = factory.get();
    }

    _spriteFactories.emplace_back(std::move(factory));
}

void SpriteManager::BeginScene(uint clear_color)
{
    STACK_TRACE_ENTRY();

    if (_rtMain != nullptr) {
        _rtMngr.PushRenderTarget(_rtMain);
        _rtMngr.ClearCurrentRenderTarget(clear_color);
    }

    for (auto&& spr_factory : _spriteFactories) {
        spr_factory->Update();
    }

    for (auto it = _updateSprites.begin(); it != _updateSprites.end();) {
        if (auto&& spr = it->second.lock(); spr && spr->Update()) {
            ++it;
        }
        else {
            it = _updateSprites.erase(it);
        }
    }
}

void SpriteManager::EndScene()
{
    STACK_TRACE_ENTRY();

    Flush();

    if (_rtMain != nullptr) {
        _rtMngr.PopRenderTarget();
        DrawRenderTarget(_rtMain, false);
    }
}

void SpriteManager::DrawTexture(const RenderTexture* tex, bool alpha_blend, const IRect* region_from, const IRect* region_to, RenderEffect* custom_effect)
{
    STACK_TRACE_ENTRY();

    Flush();

    const auto& rt_stack = _rtMngr.GetRenderTargetStack();
    const auto flipped_height = tex->FlippedHeight;
    const auto width_from_i = tex->Width;
    const auto height_from_i = tex->Height;
    const auto width_to_i = rt_stack.empty() ? _settings.ScreenWidth : rt_stack.back()->MainTex->Width;
    const auto height_to_i = rt_stack.empty() ? _settings.ScreenHeight : rt_stack.back()->MainTex->Height;
    const auto width_from_f = static_cast<float>(width_from_i);
    const auto height_from_f = static_cast<float>(height_from_i);
    const auto width_to_f = static_cast<float>(width_to_i);
    const auto height_to_f = static_cast<float>(height_to_i);

    if (region_from == nullptr && region_to == nullptr) {
        auto& vbuf = _flushDrawBuf->Vertices;
        auto pos = 0;

        vbuf[pos].PosX = 0.0f;
        vbuf[pos].PosY = flipped_height ? height_to_f : 0.0f;
        vbuf[pos].TexU = 0.0f;
        vbuf[pos].TexV = 0.0f;
        vbuf[pos++].EggTexU = 0.0f;

        vbuf[pos].PosX = 0.0f;
        vbuf[pos].PosY = flipped_height ? 0.0f : height_to_f;
        vbuf[pos].TexU = 0.0f;
        vbuf[pos].TexV = 1.0f;
        vbuf[pos++].EggTexU = 0.0f;

        vbuf[pos].PosX = width_to_f;
        vbuf[pos].PosY = flipped_height ? 0.0f : height_to_f;
        vbuf[pos].TexU = 1.0f;
        vbuf[pos].TexV = 1.0f;
        vbuf[pos++].EggTexU = 0.0f;

        vbuf[pos].PosX = width_to_f;
        vbuf[pos].PosY = flipped_height ? height_to_f : 0.0f;
        vbuf[pos].TexU = 1.0f;
        vbuf[pos].TexV = 0.0f;
        vbuf[pos].EggTexU = 0.0f;
    }
    else {
        const FRect rect_from = region_from != nullptr ? *region_from : IRect(0, 0, width_from_i, height_from_i);
        const FRect rect_to = region_to != nullptr ? *region_to : IRect(0, 0, width_to_i, height_to_i);

        auto& vbuf = _flushDrawBuf->Vertices;
        auto pos = 0;

        vbuf[pos].PosX = rect_to.Left;
        vbuf[pos].PosY = flipped_height ? rect_to.Bottom : rect_to.Top;
        vbuf[pos].TexU = rect_from.Left / width_from_f;
        vbuf[pos].TexV = flipped_height ? 1.0f - rect_from.Bottom / height_from_f : rect_from.Top / height_from_f;
        vbuf[pos++].EggTexU = 0.0f;

        vbuf[pos].PosX = rect_to.Left;
        vbuf[pos].PosY = flipped_height ? rect_to.Top : rect_to.Bottom;
        vbuf[pos].TexU = rect_from.Left / width_from_f;
        vbuf[pos].TexV = flipped_height ? 1.0f - rect_from.Top / height_from_f : rect_from.Bottom / height_from_f;
        vbuf[pos++].EggTexU = 0.0f;

        vbuf[pos].PosX = rect_to.Right;
        vbuf[pos].PosY = flipped_height ? rect_to.Top : rect_to.Bottom;
        vbuf[pos].TexU = rect_from.Right / width_from_f;
        vbuf[pos].TexV = flipped_height ? 1.0f - rect_from.Top / height_from_f : rect_from.Bottom / height_from_f;
        vbuf[pos++].EggTexU = 0.0f;

        vbuf[pos].PosX = rect_to.Right;
        vbuf[pos].PosY = flipped_height ? rect_to.Bottom : rect_to.Top;
        vbuf[pos].TexU = rect_from.Right / width_from_f;
        vbuf[pos].TexV = flipped_height ? 1.0f - rect_from.Bottom / height_from_f : rect_from.Top / height_from_f;
        vbuf[pos].EggTexU = 0.0f;
    }

    auto* effect = custom_effect != nullptr ? custom_effect : _effectMngr.Effects.FlushRenderTarget;

    effect->MainTex = tex;
    effect->DisableBlending = !alpha_blend;
    _flushDrawBuf->Upload(effect->Usage);
    effect->DrawBuffer(_flushDrawBuf);
}

void SpriteManager::DrawRenderTarget(const RenderTarget* rt, bool alpha_blend, const IRect* region_from, const IRect* region_to)
{
    STACK_TRACE_ENTRY();

    DrawTexture(rt->MainTex.get(), alpha_blend, region_from, region_to, rt->CustomDrawEffect);
}

void SpriteManager::PushScissor(int l, int t, int r, int b)
{
    STACK_TRACE_ENTRY();

    Flush();

    _scissorStack.push_back(l);
    _scissorStack.push_back(t);
    _scissorStack.push_back(r);
    _scissorStack.push_back(b);

    RefreshScissor();
}

void SpriteManager::PopScissor()
{
    STACK_TRACE_ENTRY();

    if (!_scissorStack.empty()) {
        Flush();

        _scissorStack.resize(_scissorStack.size() - 4);

        RefreshScissor();
    }
}

void SpriteManager::RefreshScissor()
{
    STACK_TRACE_ENTRY();

    if (!_scissorStack.empty()) {
        _scissorRect.Left = _scissorStack[0];
        _scissorRect.Top = _scissorStack[1];
        _scissorRect.Right = _scissorStack[2];
        _scissorRect.Bottom = _scissorStack[3];

        for (size_t i = 4; i < _scissorStack.size(); i += 4) {
            if (_scissorStack[i + 0] > _scissorRect.Left) {
                _scissorRect.Left = _scissorStack[i + 0];
            }
            if (_scissorStack[i + 1] > _scissorRect.Top) {
                _scissorRect.Top = _scissorStack[i + 1];
            }
            if (_scissorStack[i + 2] < _scissorRect.Right) {
                _scissorRect.Right = _scissorStack[i + 2];
            }
            if (_scissorStack[i + 3] < _scissorRect.Bottom) {
                _scissorRect.Bottom = _scissorStack[i + 3];
            }
        }

        if (_scissorRect.Left > _scissorRect.Right) {
            _scissorRect.Left = _scissorRect.Right;
        }
        if (_scissorRect.Top > _scissorRect.Bottom) {
            _scissorRect.Top = _scissorRect.Bottom;
        }
    }
}

void SpriteManager::EnableScissor()
{
    STACK_TRACE_ENTRY();

    const auto& rt_stack = _rtMngr.GetRenderTargetStack();

    if (!_scissorStack.empty() && !rt_stack.empty() && rt_stack.back() == _rtMain) {
        App->Render.EnableScissor(_scissorRect.Left, _scissorRect.Top, _scissorRect.Width(), _scissorRect.Height());
    }
}

void SpriteManager::DisableScissor()
{
    STACK_TRACE_ENTRY();

    const auto& rt_stack = _rtMngr.GetRenderTargetStack();

    if (!_scissorStack.empty() && !rt_stack.empty() && rt_stack.back() == _rtMain) {
        App->Render.DisableScissor();
    }
}

auto SpriteManager::LoadSprite(string_view path, AtlasType atlas_type, bool no_warn_if_not_exists) -> shared_ptr<Sprite>
{
    STACK_TRACE_ENTRY();

    return LoadSprite(_hashResolver.ToHashedString(path), atlas_type, no_warn_if_not_exists);
}

auto SpriteManager::LoadSprite(hstring path, AtlasType atlas_type, bool no_warn_if_not_exists) -> shared_ptr<Sprite>
{
    STACK_TRACE_ENTRY();

    if (!path) {
        return nullptr;
    }

    if (const auto it = _copyableSpriteCache.find({path, atlas_type}); it != _copyableSpriteCache.end()) {
        return it->second->MakeCopy();
    }

    if (_nonFoundSprites.count(path) != 0) {
        return nullptr;
    }

    const string ext = _str(path).getFileExtension();
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

    auto&& spr = it->second->LoadSprite(path, atlas_type);
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
    STACK_TRACE_ENTRY();

    for (auto&& spr_factory : _spriteFactories) {
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

void SpriteManager::SetSpritesZoom(float zoom) noexcept
{
    STACK_TRACE_ENTRY();

    _spritesZoom = zoom;
}

void SpriteManager::Flush()
{
    STACK_TRACE_ENTRY();

    if (_spritesDrawBuf->VertCount == 0) {
        return;
    }

    _spritesDrawBuf->Upload(EffectUsage::QuadSprite);

    EnableScissor();

    size_t ipos = 0;

    for (const auto& dip : _dipQueue) {
        RUNTIME_ASSERT(dip.SourceEffect->Usage == EffectUsage::QuadSprite);

        if (_sprEgg) {
            dip.SourceEffect->EggTex = _sprEgg->Atlas->MainTex;
        }

        dip.SourceEffect->DrawBuffer(_spritesDrawBuf, ipos, dip.IndCount, dip.MainTex);

        ipos += dip.IndCount;
    }

    DisableScissor();

    _dipQueue.clear();

    RUNTIME_ASSERT(ipos == _spritesDrawBuf->IndCount);

    _spritesDrawBuf->VertCount = 0;
    _spritesDrawBuf->IndCount = 0;
}

void SpriteManager::DrawSprite(const Sprite* spr, int x, int y, uint color)
{
    STACK_TRACE_ENTRY();

    auto* effect = spr->DrawEffect != nullptr ? spr->DrawEffect : _effectMngr.Effects.Iface;
    RUNTIME_ASSERT(effect);

    if (color == 0) {
        color = COLOR_SPRITE;
    }
    color = ApplyColorBrightness(color, _settings.Brightness);
    color = COLOR_SWAP_RB(color);

    const auto ind_count = spr->FillData(_spritesDrawBuf, IRect {x, y, x + spr->Width, y + spr->Height}, {color, color});

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

void SpriteManager::DrawSpriteSize(const Sprite* spr, int x, int y, int w, int h, bool zoom_up, bool center, uint color)
{
    STACK_TRACE_ENTRY();

    DrawSpriteSizeExt(spr, x, y, w, h, zoom_up, center, false, color);
}

void SpriteManager::DrawSpriteSizeExt(const Sprite* spr, int x, int y, int w, int h, bool zoom_up, bool center, bool stretch, uint color)
{
    STACK_TRACE_ENTRY();

    auto xf = static_cast<float>(x);
    auto yf = static_cast<float>(y);
    auto wf = static_cast<float>(spr->Width);
    auto hf = static_cast<float>(spr->Height);
    const auto k = std::min(static_cast<float>(w) / wf, static_cast<float>(h) / hf);

    if (!stretch) {
        if (k < 1.0f || (k > 1.0f && zoom_up)) {
            wf = std::floorf(wf * k + 0.5f);
            hf = std::floorf(hf * k + 0.5f);
        }
        if (center) {
            xf += std::floorf((static_cast<float>(w) - wf) / 2.0f + 0.5f);
            yf += std::floorf((static_cast<float>(h) - hf) / 2.0f + 0.5f);
        }
    }
    else if (zoom_up) {
        wf = std::floorf(wf * k + 0.5f);
        hf = std::floorf(hf * k + 0.5f);
        if (center) {
            xf += std::floorf((static_cast<float>(w) - wf) / 2.0f + 0.5f);
            yf += std::floorf((static_cast<float>(h) - hf) / 2.0f + 0.5f);
        }
    }
    else {
        wf = static_cast<float>(w);
        hf = static_cast<float>(h);
    }

    auto* effect = spr->DrawEffect != nullptr ? spr->DrawEffect : _effectMngr.Effects.Iface;
    RUNTIME_ASSERT(effect);

    if (color == 0) {
        color = COLOR_SPRITE;
    }
    color = ApplyColorBrightness(color, _settings.Brightness);
    color = COLOR_SWAP_RB(color);

    const auto ind_count = spr->FillData(_spritesDrawBuf, {xf, yf, xf + wf, yf + hf}, {color, color});

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

void SpriteManager::DrawSpritePattern(const Sprite* spr, int x, int y, int w, int h, int spr_width, int spr_height, uint color)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(w > 0);
    RUNTIME_ASSERT(h > 0);

    const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(spr);
    if (atlas_spr == nullptr) {
        return;
    }

    auto width = static_cast<float>(atlas_spr->Width);
    auto height = static_cast<float>(atlas_spr->Height);

    if (spr_width != 0 && spr_height != 0) {
        width = static_cast<float>(spr_width);
        height = static_cast<float>(spr_height);
    }
    else if (spr_width != 0) {
        const auto ratio = static_cast<float>(spr_width) / width;
        width = static_cast<float>(spr_width);
        height *= ratio;
    }
    else if (spr_height != 0) {
        const auto ratio = static_cast<float>(spr_height) / height;
        height = static_cast<float>(spr_height);
        width *= ratio;
    }

    if (color == 0) {
        color = COLOR_SPRITE;
    }
    color = ApplyColorBrightness(color, _settings.Brightness);
    color = COLOR_SWAP_RB(color);

    auto* effect = atlas_spr->DrawEffect != nullptr ? atlas_spr->DrawEffect : _effectMngr.Effects.Iface;
    RUNTIME_ASSERT(effect);

    const auto last_right_offs = (atlas_spr->AtlasRect.Right - atlas_spr->AtlasRect.Left) / width;
    const auto last_bottom_offs = (atlas_spr->AtlasRect.Bottom - atlas_spr->AtlasRect.Top) / height;

    for (auto yy = static_cast<float>(y), end_y = static_cast<float>(y + h); yy < end_y;) {
        const auto last_y = yy + height >= end_y;

        for (auto xx = static_cast<float>(x), end_x = static_cast<float>(x + w); xx < end_x;) {
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

            ibuf[ipos++] = static_cast<uint16>(vpos + 0);
            ibuf[ipos++] = static_cast<uint16>(vpos + 1);
            ibuf[ipos++] = static_cast<uint16>(vpos + 3);
            ibuf[ipos++] = static_cast<uint16>(vpos + 1);
            ibuf[ipos++] = static_cast<uint16>(vpos + 2);
            ibuf[ipos++] = static_cast<uint16>(vpos + 3);

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

void SpriteManager::PrepareSquare(vector<PrimitivePoint>& points, const IRect& r, uint color)
{
    STACK_TRACE_ENTRY();

    points.push_back({r.Left, r.Bottom, color});
    points.push_back({r.Left, r.Top, color});
    points.push_back({r.Right, r.Bottom, color});
    points.push_back({r.Left, r.Top, color});
    points.push_back({r.Right, r.Top, color});
    points.push_back({r.Right, r.Bottom, color});
}

void SpriteManager::PrepareSquare(vector<PrimitivePoint>& points, IPoint lt, IPoint rt, IPoint lb, IPoint rb, uint color)
{
    STACK_TRACE_ENTRY();

    points.push_back({lb.X, lb.Y, color});
    points.push_back({lt.X, lt.Y, color});
    points.push_back({rb.X, rb.Y, color});
    points.push_back({lt.X, lt.Y, color});
    points.push_back({rt.X, rt.Y, color});
    points.push_back({rb.X, rb.Y, color});
}

void SpriteManager::InitializeEgg(hstring egg_name, AtlasType atlas_type)
{
    STACK_TRACE_ENTRY();

    _eggValid = false;
    _eggHx = 0;
    _eggHy = 0;
    _eggX = 0;
    _eggY = 0;

    auto&& spr = LoadSprite(egg_name, atlas_type);
    RUNTIME_ASSERT(spr);

    _sprEgg = dynamic_pointer_cast<AtlasSprite>(spr);
    RUNTIME_ASSERT(_sprEgg);

    const auto x = iround(_sprEgg->Atlas->MainTex->SizeData[0] * _sprEgg->AtlasRect.Left);
    const auto y = iround(_sprEgg->Atlas->MainTex->SizeData[1] * _sprEgg->AtlasRect.Top);
    _eggData = _sprEgg->Atlas->MainTex->GetTextureRegion(x, y, _sprEgg->Width, _sprEgg->Height);
}

auto SpriteManager::CheckEggAppearence(uint16 hx, uint16 hy, EggAppearenceType egg_appearence) const -> bool
{
    STACK_TRACE_ENTRY();

    if (egg_appearence == EggAppearenceType::None) {
        return false;
    }
    if (egg_appearence == EggAppearenceType::Always) {
        return true;
    }

    if (_eggHy == hy && (hx % 2) != 0 && (_eggHx % 2) == 0) {
        hy--;
    }

    switch (egg_appearence) {
    case EggAppearenceType::ByX:
        if (hx >= _eggHx) {
            return true;
        }
        break;
    case EggAppearenceType::ByY:
        if (hy >= _eggHy) {
            return true;
        }
        break;
    case EggAppearenceType::ByXAndY:
        if (hx >= _eggHx || hy >= _eggHy) {
            return true;
        }
        break;
    case EggAppearenceType::ByXOrY:
        if (hx >= _eggHx && hy >= _eggHy) {
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

void SpriteManager::SetEgg(uint16 hx, uint16 hy, const MapSprite* mspr)
{
    STACK_TRACE_ENTRY();

    const auto* spr = mspr->PSpr != nullptr ? *mspr->PSpr : mspr->Spr;
    RUNTIME_ASSERT(spr);

    _eggX = mspr->ScrX + spr->OffsX - _sprEgg->Width / 2 + *mspr->OffsX + *mspr->PScrX;
    _eggY = mspr->ScrY - spr->Height / 2 + spr->OffsY - _sprEgg->Height / 2 + *mspr->OffsY + *mspr->PScrY;
    _eggHx = hx;
    _eggHy = hy;
    _eggValid = true;
}

void SpriteManager::DrawSprites(MapSpriteList& mspr_list, bool collect_contours, bool use_egg, DrawOrderType draw_oder_from, DrawOrderType draw_oder_to, uint color)
{
    STACK_TRACE_ENTRY();

    if (mspr_list.RootSprite() == nullptr) {
        return;
    }

    vector<PrimitivePoint> borders;

    if (!_eggValid) {
        use_egg = false;
    }

    const auto ex = _eggX + _settings.ScrOx;
    const auto ey = _eggY + _settings.ScrOy;

    for (const auto* mspr = mspr_list.RootSprite(); mspr != nullptr; mspr = mspr->ChainChild) {
        RUNTIME_ASSERT(mspr->Valid);

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
        auto x = mspr->ScrX - spr->Width / 2 + spr->OffsX + *mspr->PScrX;
        auto y = mspr->ScrY - spr->Height + spr->OffsY + *mspr->PScrY;
        x += _settings.ScrOx;
        y += _settings.ScrOy;
        if (mspr->OffsX != nullptr) {
            x += *mspr->OffsX;
        }
        if (mspr->OffsY != nullptr) {
            y += *mspr->OffsY;
        }

        const auto zoom = _spritesZoom;

        // Base color
        uint color_r = 0;
        uint color_l = 0;
        if (mspr->Color != 0) {
            color_r = color_l = mspr->Color | 0xFF000000;
        }
        else {
            color_r = color_l = color;
        }

        // Light
        if (mspr->Light != nullptr) {
            static auto light_func = [](uint& c, const uint8* l, const uint8* l2) {
                const int lr = *l;
                const int lg = *(l + 1);
                const int lb = *(l + 2);
                const int lr2 = *l2;
                const int lg2 = *(l2 + 1);
                const int lb2 = *(l2 + 2);
                auto& r = reinterpret_cast<uint8*>(&c)[2];
                auto& g = reinterpret_cast<uint8*>(&c)[1];
                auto& b = reinterpret_cast<uint8*>(&c)[0];
                const auto ir = static_cast<int>(r) + (lr + lr2) / 2;
                const auto ig = static_cast<int>(g) + (lg + lg2) / 2;
                const auto ib = static_cast<int>(b) + (lb + lb2) / 2;
                r = static_cast<uint8>(std::min(ir, 255));
                g = static_cast<uint8>(std::min(ig, 255));
                b = static_cast<uint8>(std::min(ib, 255));
            };
            light_func(color_r, mspr->Light, mspr->LightRight);
            light_func(color_l, mspr->Light, mspr->LightLeft);
        }

        // Alpha
        if (mspr->Alpha != nullptr) {
            reinterpret_cast<uint8*>(&color_r)[3] = *mspr->Alpha;
            reinterpret_cast<uint8*>(&color_l)[3] = *mspr->Alpha;
        }

        // Fix color
        color_r = ApplyColorBrightness(color_r, _settings.Brightness);
        color_l = ApplyColorBrightness(color_l, _settings.Brightness);
        color_r = COLOR_SWAP_RB(color_r);
        color_l = COLOR_SWAP_RB(color_l);

        // Check borders
        if (static_cast<float>(x) / zoom > static_cast<float>(_settings.ScreenWidth) || static_cast<float>(x + spr->Width) / zoom < 0.0f || //
            static_cast<float>(y) / zoom > static_cast<float>(_settings.ScreenHeight - _settings.ScreenHudHeight) || static_cast<float>(y + spr->Height) / zoom < 0.0f) {
            continue;
        }

        // Choose effect
        auto* effect = mspr->DrawEffect != nullptr ? *mspr->DrawEffect : nullptr;
        if (effect == nullptr) {
            effect = spr->DrawEffect != nullptr ? spr->DrawEffect : _effectMngr.Effects.Generic;
        }
        RUNTIME_ASSERT(effect);

        // Fill buffer
        const auto xf = static_cast<float>(x) / zoom;
        const auto yf = static_cast<float>(y) / zoom;
        const auto wf = static_cast<float>(spr->Width) / zoom;
        const auto hf = static_cast<float>(spr->Height) / zoom;

        const auto ind_count = spr->FillData(_spritesDrawBuf, {xf, yf, xf + wf, yf + hf}, {color_l, color_r});

        // Setup egg
        if (use_egg && ind_count == 6 && CheckEggAppearence(mspr->HexX, mspr->HexY, mspr->EggAppearence)) {
            auto x1 = x - ex;
            auto y1 = y - ey;
            auto x2 = x1 + spr->Width;
            auto y2 = y1 + spr->Height;

            if (x1 < _sprEgg->Width - 100 && y1 < _sprEgg->Height - 100 && x2 >= 100 && y2 >= 100) {
                if (const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(spr); atlas_spr != nullptr) {
                    x1 = std::max(x1, 0);
                    y1 = std::max(y1, 0);
                    x2 = std::min(x2, _sprEgg->Width);
                    y2 = std::min(y2, _sprEgg->Height);

                    const auto x1_f = _sprEgg->AtlasRect.Left + static_cast<float>(x1) / _sprEgg->Atlas->MainTex->SizeData[0];
                    const auto x2_f = _sprEgg->AtlasRect.Left + static_cast<float>(x2) / _sprEgg->Atlas->MainTex->SizeData[0];
                    const auto y1_f = _sprEgg->AtlasRect.Top + static_cast<float>(y1) / _sprEgg->Atlas->MainTex->SizeData[1];
                    const auto y2_f = _sprEgg->AtlasRect.Top + static_cast<float>(y2) / _sprEgg->Atlas->MainTex->SizeData[1];

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
                PrepareSquare(corner, FRect(xf + cx - 2.0f, yf + hf - 50.0f, xf + cx + 2.0f, yf + hf), 0x5FFFFF00);
                break;
            case EggAppearenceType::ByX:
                PrepareSquare(corner, FPoint(xf + cx - 5.0f, yf + hf - 55.0f), FPoint(xf + cx + 5.0f, yf + hf - 45.0f), FPoint(xf + cx - 5.0f, yf + hf - 5.0f), FPoint(xf + cx + 5.0f, yf + hf + 5.0f), 0x5F00AF00);
                break;
            case EggAppearenceType::ByY:
                PrepareSquare(corner, FPoint(xf + cx - 5.0f, yf + hf - 49.0f), FPoint(xf + cx + 5.0f, yf + hf - 52.0f), FPoint(xf + cx - 5.0f, yf + hf + 1.0f), FPoint(xf + cx + 5.0f, yf + hf - 2.0f), 0x5F00FF00);
                break;
            case EggAppearenceType::ByXAndY:
                PrepareSquare(corner, FPoint(xf + cx - 10.0f, yf + hf - 49.0f), FPoint(xf + cx, yf + hf - 52.0f), FPoint(xf + cx - 10.0f, yf + hf + 1.0f), FPoint(xf + cx, yf + hf - 2.0f), 0x5FFF0000);
                PrepareSquare(corner, FPoint(xf + cx, yf + hf - 55.0f), FPoint(xf + cx + 10.0f, yf + hf - 45.0f), FPoint(xf + cx, yf + hf - 5.0f), FPoint(xf + cx + 10.0f, yf + hf + 5.0f), 0x5FFF0000);
                break;
            case EggAppearenceType::ByXOrY:
                PrepareSquare(corner, FPoint(xf + cx, yf + hf - 49.0f), FPoint(xf + cx + 10.0f, yf + hf - 52.0f), FPoint(xf + cx, yf + hf + 1.0f), FPoint(xf + cx + 10.0f, yf + hf - 2.0f), 0x5FAF0000);
                PrepareSquare(corner, FPoint(xf + cx - 10.0f, yf + hf - 55.0f), FPoint(xf + cx, yf + hf - 45.0f), FPoint(xf + cx - 10.0f, yf + hf - 5.0f), FPoint(xf + cx, yf + hf + 5.0f), 0x5FAF0000);
                break;
            default:
                break;
            }

            DrawPoints(corner, RenderPrimitiveType::TriangleList);
        }

        // Draw order
        if (_settings.ShowDrawOrder) {
            const auto x1 = iround(static_cast<float>(mspr->ScrX + _settings.ScrOx) / zoom);
            auto y1 = iround(static_cast<float>(mspr->ScrY + _settings.ScrOy) / zoom);

            if (mspr->DrawOrder < DrawOrderType::NormalBegin || mspr->DrawOrder > DrawOrderType::NormalEnd) {
                y1 -= iround(40.0f / zoom);
            }

            DrawStr(IRect(x1, y1, x1 + 100, y1 + 100), _str("{}", mspr->TreeIndex), 0, 0, -1);
        }

        // Process contour effect
        if (collect_contours && mspr->Contour != ContourType::None) {
            uint contour_color = 0xFF0000FF;

            switch (mspr->Contour) {
            case ContourType::Red:
                contour_color = 0xFFAF0000;
                break;
            case ContourType::Yellow:
                contour_color = 0x00AFAF00;
                break;
            case ContourType::Custom:
                contour_color = mspr->ContourColor;
                break;
            default:
                break;
            }

            CollectContour(x, y, spr, contour_color);
        }

        if (_settings.ShowSpriteBorders && mspr->DrawOrder > DrawOrderType::Tile4) {
            auto rect = mspr->GetViewRect();

            rect.Left += _settings.ScrOx;
            rect.Right += _settings.ScrOx;
            rect.Top += _settings.ScrOy;
            rect.Bottom += _settings.ScrOy;

            PrepareSquare(borders, rect, COLOR_RGBA(50, 0, 0, 255));
        }
    }

    Flush();

    if (_settings.ShowSpriteBorders) {
        DrawPoints(borders, RenderPrimitiveType::TriangleList);
    }
}

auto SpriteManager::SpriteHitTest(const Sprite* spr, int spr_x, int spr_y, bool with_zoom) const -> bool
{
    STACK_TRACE_ENTRY();

    if (spr_x < 0 || spr_y < 0) {
        return false;
    }

    if (with_zoom && _spritesZoom != 1.0f) {
        const auto zoomed_spr_x = iround(static_cast<float>(spr_x) * _spritesZoom);
        const auto zoomed_spr_y = iround(static_cast<float>(spr_y) * _spritesZoom);

        return spr->IsHitTest(zoomed_spr_x, zoomed_spr_y);
    }
    else {
        return spr->IsHitTest(spr_x, spr_y);
    }
}

auto SpriteManager::IsEggTransp(int pix_x, int pix_y) const -> bool
{
    STACK_TRACE_ENTRY();

    if (!_eggValid) {
        return false;
    }

    const auto ex = _eggX + _settings.ScrOx;
    const auto ey = _eggY + _settings.ScrOy;
    auto ox = pix_x - iround(static_cast<float>(ex) / _spritesZoom);
    auto oy = pix_y - iround(static_cast<float>(ey) / _spritesZoom);

    if (ox < 0 || oy < 0 || //
        ox >= iround(static_cast<float>(_sprEgg->Width) / _spritesZoom) || //
        oy >= iround(static_cast<float>(_sprEgg->Height) / _spritesZoom)) {
        return false;
    }

    ox = iround(static_cast<float>(ox) * _spritesZoom);
    oy = iround(static_cast<float>(oy) * _spritesZoom);

    const auto egg_color = _eggData.at(oy * _sprEgg->Width + ox);
    return (egg_color >> 24) < 127;
}

void SpriteManager::DrawPoints(const vector<PrimitivePoint>& points, RenderPrimitiveType prim, const float* zoom, const FPoint* offset, RenderEffect* custom_effect)
{
    STACK_TRACE_ENTRY();

    if (points.empty()) {
        return;
    }

    Flush();

    auto* effect = custom_effect != nullptr ? custom_effect : _effectMngr.Effects.Primitive;
    RUNTIME_ASSERT(effect);

    // Check primitives
    const auto count = points.size();
    auto prim_count = static_cast<int>(count);
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

    _primitiveDrawBuf->CheckAllocBuf(count, count);

    auto& vbuf = _primitiveDrawBuf->Vertices;
    auto& vpos = _primitiveDrawBuf->VertCount;
    auto& ibuf = _primitiveDrawBuf->Indices;
    auto& ipos = _primitiveDrawBuf->IndCount;

    vpos = count;
    ipos = count;

    for (size_t i = 0; i < count; i++) {
        const auto& point = points[i];

        auto x = static_cast<float>(point.PointX);
        auto y = static_cast<float>(point.PointY);

        if (point.PointOffsX != nullptr) {
            x += static_cast<float>(*point.PointOffsX);
        }
        if (point.PointOffsY != nullptr) {
            y += static_cast<float>(*point.PointOffsY);
        }
        if (zoom != nullptr) {
            x /= *zoom;
            y /= *zoom;
        }
        if (offset != nullptr) {
            x += offset->X;
            y += offset->Y;
        }

        uint color = point.PointColor;
        color = ApplyColorBrightness(color, _settings.Brightness);
        color = COLOR_SWAP_RB(color);

        vbuf[i].PosX = x;
        vbuf[i].PosY = y;
        vbuf[i].Color = color;

        ibuf[i] = static_cast<uint16>(i);
    }

    _primitiveDrawBuf->PrimType = prim;
    _primitiveDrawBuf->PrimZoomed = _spritesZoom != 1.0f;

    _primitiveDrawBuf->Upload(effect->Usage, count, count);
    EnableScissor();
    effect->DrawBuffer(_primitiveDrawBuf, 0, count);
    DisableScissor();
}

void SpriteManager::DrawContours()
{
    STACK_TRACE_ENTRY();

    if (_contoursAdded && _rtContours != nullptr && _rtContoursMid != nullptr) {
        // Draw collected contours
        DrawRenderTarget(_rtContours, true);

        // Clean render targets
        _rtMngr.PushRenderTarget(_rtContours);
        _rtMngr.ClearCurrentRenderTarget(0);
        _rtMngr.PopRenderTarget();

        if (_contourClearMid) {
            _contourClearMid = false;

            _rtMngr.PushRenderTarget(_rtContoursMid);
            _rtMngr.ClearCurrentRenderTarget(0);
            _rtMngr.PopRenderTarget();
        }

        _contoursAdded = false;
    }
}

void SpriteManager::CollectContour(int x, int y, const Sprite* spr, uint contour_color)
{
    STACK_TRACE_ENTRY();

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

    auto borders = IRect(x - 1, y - 1, x + as->Width + 1, y + as->Height + 1);
    auto* texture = as->Atlas->MainTex;
    FRect textureuv;
    FRect sprite_border;

    const auto zoomed_screen_width = iround(static_cast<float>(_settings.ScreenWidth) * _spritesZoom);
    const auto zoomed_screen_height = iround(static_cast<float>(_settings.ScreenHeight - _settings.ScreenHudHeight) * _spritesZoom);
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
            const float txw = texture->SizeData[2];
            const float txh = texture->SizeData[3];
            textureuv = FRect(sr.Left - txw, sr.Top - txh, sr.Right + txw, sr.Bottom + txh);
            sprite_border = sr;
        }
    }
    else {
        const auto& sr = as->AtlasRect;
        const auto zoomed_x = iround(static_cast<float>(x) / _spritesZoom);
        const auto zoomed_y = iround(static_cast<float>(y) / _spritesZoom);
        const auto zoomed_x2 = iround(static_cast<float>(x + as->Width) / _spritesZoom);
        const auto zoomed_y2 = iround(static_cast<float>(y + as->Height) / _spritesZoom);
        borders = {zoomed_x, zoomed_y, zoomed_x2, zoomed_y2};
        const auto bordersf = FRect(borders);
        const auto mid_height = _rtContoursMid->MainTex->SizeData[1];
        const auto flipped_height = _rtContoursMid->MainTex->FlippedHeight;

        _rtMngr.PushRenderTarget(_rtContoursMid);
        _contourClearMid = true;

        auto& vbuf = _flushDrawBuf->Vertices;
        auto pos = 0;

        vbuf[pos].PosX = bordersf.Left;
        vbuf[pos].PosY = flipped_height ? mid_height - bordersf.Bottom : bordersf.Bottom;
        vbuf[pos].TexU = sr.Left;
        vbuf[pos].TexV = sr.Bottom;
        vbuf[pos++].EggTexU = 0.0f;

        vbuf[pos].PosX = bordersf.Left;
        vbuf[pos].PosY = flipped_height ? mid_height - bordersf.Top : bordersf.Top;
        vbuf[pos].TexU = sr.Left;
        vbuf[pos].TexV = sr.Top;
        vbuf[pos++].EggTexU = 0.0f;

        vbuf[pos].PosX = bordersf.Right;
        vbuf[pos].PosY = flipped_height ? mid_height - bordersf.Top : bordersf.Top;
        vbuf[pos].TexU = sr.Right;
        vbuf[pos].TexV = sr.Top;
        vbuf[pos++].EggTexU = 0.0f;

        vbuf[pos].PosX = bordersf.Right;
        vbuf[pos].PosY = flipped_height ? mid_height - bordersf.Bottom : bordersf.Bottom;
        vbuf[pos].TexU = sr.Right;
        vbuf[pos].TexV = sr.Bottom;
        vbuf[pos].EggTexU = 0.0f;

        _flushDrawBuf->Upload(_effectMngr.Effects.FlushRenderTarget->Usage);
        _effectMngr.Effects.FlushRenderTarget->DrawBuffer(_flushDrawBuf);

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

    contour_color = ApplyColorBrightness(contour_color, _settings.Brightness);
    contour_color = COLOR_SWAP_RB(contour_color);

    const auto bordersf = FRect(borders);

    _rtMngr.PushRenderTarget(_rtContours);

    auto& vbuf = _contourDrawBuf->Vertices;
    auto pos = 0;

    vbuf[pos].PosX = bordersf.Left;
    vbuf[pos].PosY = bordersf.Bottom;
    vbuf[pos].TexU = textureuv.Left;
    vbuf[pos].TexV = textureuv.Bottom;
    vbuf[pos].EggTexU = 0.0f;
    vbuf[pos++].Color = contour_color;

    vbuf[pos].PosX = bordersf.Left;
    vbuf[pos].PosY = bordersf.Top;
    vbuf[pos].TexU = textureuv.Left;
    vbuf[pos].TexV = textureuv.Top;
    vbuf[pos].EggTexU = 0.0f;
    vbuf[pos++].Color = contour_color;

    vbuf[pos].PosX = bordersf.Right;
    vbuf[pos].PosY = bordersf.Top;
    vbuf[pos].TexU = textureuv.Right;
    vbuf[pos].TexV = textureuv.Top;
    vbuf[pos].EggTexU = 0.0f;
    vbuf[pos++].Color = contour_color;

    vbuf[pos].PosX = bordersf.Right;
    vbuf[pos].PosY = bordersf.Bottom;
    vbuf[pos].TexU = textureuv.Right;
    vbuf[pos].TexV = textureuv.Bottom;
    vbuf[pos].EggTexU = 0.0f;
    vbuf[pos].Color = contour_color;

    auto&& contour_buf = contour_effect->ContourBuf = RenderEffect::ContourBuffer();

    contour_buf->SpriteBorder[0] = sprite_border[0];
    contour_buf->SpriteBorder[1] = sprite_border[1];
    contour_buf->SpriteBorder[2] = sprite_border[2];
    contour_buf->SpriteBorder[3] = sprite_border[3];

    _contourDrawBuf->Upload(contour_effect->Usage);
    contour_effect->DrawBuffer(_contourDrawBuf, 0, static_cast<size_t>(-1), texture);

    _rtMngr.PopRenderTarget();
    _contoursAdded = true;
}

auto SpriteManager::GetFont(int num) -> FontData*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (num < 0) {
        num = _defFontIndex;
    }
    if (num < 0 || num >= static_cast<int>(_allFonts.size())) {
        return nullptr;
    }
    return _allFonts[num].get();
}

void SpriteManager::ClearFonts()
{
    STACK_TRACE_ENTRY();

    _allFonts.clear();
}

void SpriteManager::SetDefaultFont(int index, uint color)
{
    STACK_TRACE_ENTRY();

    _defFontIndex = index;
    _defFontColor = color;
}

void SpriteManager::SetFontEffect(int index, RenderEffect* effect)
{
    STACK_TRACE_ENTRY();

    auto* font = GetFont(index);
    if (font != nullptr) {
        font->DrawEffect = effect != nullptr ? effect : _effectMngr.Effects.Font;
    }
}

void SpriteManager::BuildFont(int index)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

#define PIXEL_AT(tex_data, width, x, y) (*(reinterpret_cast<uint*>(tex_data.data()) + (y) * (width) + (x)))

    auto& font = *_allFonts[index];

    // Fix texture coordinates
    const auto* atlas_spr = font.ImageNormal.get();
    auto tex_w = static_cast<float>(atlas_spr->Atlas->Width);
    auto tex_h = static_cast<float>(atlas_spr->Atlas->Height);
    auto image_x = tex_w * atlas_spr->AtlasRect.Left;
    auto image_y = tex_h * atlas_spr->AtlasRect.Top;
    auto max_h = 0;
    for (auto& [letter_index, letter] : font.Letters) {
        const auto x = static_cast<float>(letter.PosX);
        const auto y = static_cast<float>(letter.PosY);
        const auto w = static_cast<float>(letter.Width);
        const auto h = static_cast<float>(letter.Height);
        letter.TexPos[0] = (image_x + x - 1.0f) / tex_w;
        letter.TexPos[1] = (image_y + y - 1.0f) / tex_h;
        letter.TexPos[2] = (image_x + x + w + 1.0f) / tex_w;
        letter.TexPos[3] = (image_y + y + h + 1.0f) / tex_h;
        if (letter.Height > max_h) {
            max_h = letter.Height;
        }
    }

    // Fill data
    font.FontTex = atlas_spr->Atlas->MainTex;
    if (font.LineHeight == 0) {
        font.LineHeight = max_h;
    }
    if (font.Letters.count(' ') != 0) {
        font.SpaceWidth = font.Letters[' '].XAdvance;
    }

    const auto* si_bordered = dynamic_cast<const AtlasSprite*>(font.ImageBordered ? font.ImageBordered.get() : nullptr);
    font.FontTexBordered = si_bordered != nullptr ? si_bordered->Atlas->MainTex : nullptr;

    const auto normal_ox = iround(tex_w * atlas_spr->AtlasRect.Left);
    const auto normal_oy = iround(tex_h * atlas_spr->AtlasRect.Top);
    const auto bordered_ox = (si_bordered != nullptr ? iround(static_cast<float>(si_bordered->Atlas->Width) * si_bordered->AtlasRect.Left) : 0);
    const auto bordered_oy = (si_bordered != nullptr ? iround(static_cast<float>(si_bordered->Atlas->Height) * si_bordered->AtlasRect.Top) : 0);

    // Read texture data
    auto data_normal = atlas_spr->Atlas->MainTex->GetTextureRegion(normal_ox, normal_oy, atlas_spr->Width, atlas_spr->Height);

    vector<uint> data_bordered;
    if (si_bordered != nullptr) {
        data_bordered = si_bordered->Atlas->MainTex->GetTextureRegion(bordered_ox, bordered_oy, si_bordered->Width, si_bordered->Height);
    }

    // Normalize color to gray
    if (font.MakeGray) {
        for (auto y = 0; y < atlas_spr->Height; y++) {
            for (auto x = 0; x < atlas_spr->Width; x++) {
                const auto a = reinterpret_cast<uint8*>(&PIXEL_AT(data_normal, atlas_spr->Width, x, y))[3];
                if (a != 0) {
                    PIXEL_AT(data_normal, atlas_spr->Width, x, y) = COLOR_RGBA(a, 128, 128, 128);
                    if (si_bordered != nullptr) {
                        PIXEL_AT(data_bordered, si_bordered->Width, x, y) = COLOR_RGBA(a, 128, 128, 128);
                    }
                }
                else {
                    PIXEL_AT(data_normal, atlas_spr->Width, x, y) = COLOR_RGBA(0, 0, 0, 0);
                    if (si_bordered != nullptr) {
                        PIXEL_AT(data_bordered, si_bordered->Width, x, y) = COLOR_RGBA(0, 0, 0, 0);
                    }
                }
            }
        }

        const auto r = IRect(normal_ox, normal_oy, normal_ox + atlas_spr->Width, normal_oy + atlas_spr->Height);
        atlas_spr->Atlas->MainTex->UpdateTextureRegion(r, data_normal.data());
    }

    // Fill border
    if (si_bordered != nullptr) {
        for (auto y = 1; y < si_bordered->Height - 2; y++) {
            for (auto x = 1; x < si_bordered->Width - 2; x++) {
                if (PIXEL_AT(data_normal, atlas_spr->Width, x, y)) {
                    for (auto xx = -1; xx <= 1; xx++) {
                        for (auto yy = -1; yy <= 1; yy++) {
                            const auto ox = x + xx;
                            const auto oy = y + yy;
                            if (!PIXEL_AT(data_bordered, si_bordered->Width, ox, oy)) {
                                PIXEL_AT(data_bordered, si_bordered->Width, ox, oy) = COLOR_RGB(0, 0, 0);
                            }
                        }
                    }
                }
            }
        }

        const auto r_bordered = IRect(bordered_ox, bordered_oy, bordered_ox + si_bordered->Width, bordered_oy + si_bordered->Height);
        si_bordered->Atlas->MainTex->UpdateTextureRegion(r_bordered, data_bordered.data());

        // Fix texture coordinates on bordered texture
        tex_w = static_cast<float>(si_bordered->Atlas->Width);
        tex_h = static_cast<float>(si_bordered->Atlas->Height);
        image_x = tex_w * si_bordered->AtlasRect.Left;
        image_y = tex_h * si_bordered->AtlasRect.Top;
        for (auto&& [letter_index, letter] : font.Letters) {
            const auto x = static_cast<float>(letter.PosX);
            const auto y = static_cast<float>(letter.PosY);
            const auto w = static_cast<float>(letter.Width);
            const auto h = static_cast<float>(letter.Height);
            letter.TexBorderedPos[0] = (image_x + x - 1.0f) / tex_w;
            letter.TexBorderedPos[1] = (image_y + y - 1.0f) / tex_h;
            letter.TexBorderedPos[2] = (image_x + x + w + 1.0f) / tex_w;
            letter.TexBorderedPos[3] = (image_y + y + h + 1.0f) / tex_h;
        }
    }

#undef PIXEL_AT
}

auto SpriteManager::LoadFontFO(int index, string_view font_name, AtlasType atlas_type, bool not_bordered, bool skip_if_loaded /* = true */) -> bool
{
    STACK_TRACE_ENTRY();

    // Skip if loaded
    if (skip_if_loaded && index < static_cast<int>(_allFonts.size()) && _allFonts[index]) {
        return true;
    }

    // Load font data
    string fname = _str("Fonts/{}.fofnt", font_name);
    auto file = _resources.ReadFile(fname);
    if (!file) {
        WriteLog("File '{}' not found", fname);
        return false;
    }

    auto&& font = std::make_unique<FontData>();
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

            uint letter_len = 0;
            auto letter = utf8::Decode(letter_buf.c_str() + utf8_letter_begin, &letter_len);
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
            str >> cur_letter->PosX;
        }
        else if (key == "PositionY") {
            str >> cur_letter->PosY;
        }
        else if (key == "Width") {
            str >> cur_letter->Width;
        }
        else if (key == "Height") {
            str >> cur_letter->Height;
        }
        else if (key == "OffsetX") {
            str >> cur_letter->OffsX;
        }
        else if (key == "OffsetY") {
            str >> cur_letter->OffsY;
        }
        else if (key == "XAdvance") {
            str >> cur_letter->XAdvance;
        }
    }

    auto make_gray = false;
    if (image_name.back() == '*') {
        make_gray = true;
        image_name = image_name.substr(0, image_name.size() - 1);
    }
    font->MakeGray = make_gray;

    // Load image
    {
        image_name.insert(0, "Fonts/");

        font->ImageNormal = dynamic_pointer_cast<AtlasSprite>(LoadSprite(_hashResolver.ToHashedString(image_name), atlas_type));
        if (!font->ImageNormal) {
            WriteLog("Image file '{}' not found", image_name);
            return false;
        }

        _copyableSpriteCache.erase({_hashResolver.ToHashedString(image_name), atlas_type});
    }

    // Create bordered instance
    if (!not_bordered) {
        font->ImageBordered = dynamic_pointer_cast<AtlasSprite>(LoadSprite(_hashResolver.ToHashedString(image_name), atlas_type));
        if (!font->ImageBordered) {
            WriteLog("Can't load twice file '{}'", image_name);
            return false;
        }

        _copyableSpriteCache.erase({_hashResolver.ToHashedString(image_name), atlas_type});
    }

    // Register
    if (index >= static_cast<int>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }
    _allFonts[index] = std::move(font);

    BuildFont(index);

    return true;
}

static constexpr auto MAKEUINT(uint8 ch0, uint8 ch1, uint8 ch2, uint8 ch3) -> uint
{
    return ch0 | ch1 << 8 | ch2 << 16 | ch3 << 24;
}

auto SpriteManager::LoadFontBmf(int index, string_view font_name, AtlasType atlas_type) -> bool
{
    STACK_TRACE_ENTRY();

    if (index < 0) {
        WriteLog("Invalid index");
        return false;
    }

    auto&& font = std::make_unique<FontData>();
    font->DrawEffect = _effectMngr.Effects.Font;

    auto file = _resources.ReadFile(_str("Fonts/{}.fnt", font_name));
    if (!file) {
        WriteLog("Font file '{}.fnt' not found", font_name);
        return false;
    }

    const auto signature = file.GetLEUInt();
    if (signature != MAKEUINT('B', 'M', 'F', 3)) {
        WriteLog("Invalid signature of font '{}'", font_name);
        return false;
    }

    // Info
    file.GoForward(1);
    auto block_len = file.GetLEUInt();
    auto next_block = block_len + file.GetCurPos() + 1;

    file.GoForward(7);
    if (file.GetBEUInt() != 0x01010101u) {
        WriteLog("Wrong padding in font '{}'", font_name);
        return false;
    }

    // Common
    file.SetCurPos(next_block);
    block_len = file.GetLEUInt();
    next_block = block_len + file.GetCurPos() + 1;

    const int line_height = file.GetLEUShort();
    const int base_height = file.GetLEUShort();
    file.GoForward(2); // Texture width
    file.GoForward(2); // Texture height

    if (file.GetLEUShort() != 1) {
        WriteLog("Texture for font '{}' must be one", font_name);
        return false;
    }

    // Pages
    file.SetCurPos(next_block);
    block_len = file.GetLEUInt();
    next_block = block_len + file.GetCurPos() + 1;

    // Image name
    auto image_name = file.GetStrNT();
    image_name.insert(0, "Fonts/");

    // Chars
    file.SetCurPos(next_block);
    const auto count = file.GetLEUInt() / 20;
    for ([[maybe_unused]] const auto i : xrange(count)) {
        // Read data
        const auto id = file.GetLEUInt();
        const auto x = file.GetLEUShort();
        const auto y = file.GetLEUShort();
        const auto w = file.GetLEUShort();
        const auto h = file.GetLEUShort();
        const auto ox = file.GetLEUShort();
        const auto oy = file.GetLEUShort();
        const auto xa = file.GetLEUShort();
        file.GoForward(2);

        // Fill data
        auto& let = font->Letters[id];
        let.PosX = static_cast<int16>(x + 1);
        let.PosY = static_cast<int16>(y + 1);
        let.Width = static_cast<int16>(w - 2);
        let.Height = static_cast<int16>(h - 2);
        let.OffsX = static_cast<int16>(-static_cast<int>(ox));
        let.OffsY = static_cast<int16>(-static_cast<int>(oy) + (line_height - base_height));
        let.XAdvance = static_cast<int16>(xa + 1);
    }

    font->LineHeight = font->Letters.count('W') != 0 ? font->Letters['W'].Height : base_height;
    font->YAdvance = font->LineHeight / 2;
    font->MakeGray = true;

    // Load image
    {
        font->ImageNormal = dynamic_pointer_cast<AtlasSprite>(LoadSprite(_hashResolver.ToHashedString(image_name), atlas_type));
        if (!font->ImageNormal) {
            WriteLog("Image file '{}' not found", image_name);
            return false;
        }

        _copyableSpriteCache.erase({_hashResolver.ToHashedString(image_name), atlas_type});
    }

    // Create bordered instance
    {
        font->ImageBordered = dynamic_pointer_cast<AtlasSprite>(LoadSprite(_hashResolver.ToHashedString(image_name), atlas_type));
        if (!font->ImageBordered) {
            WriteLog("Can't load twice file '{}'", image_name);
            return false;
        }

        _copyableSpriteCache.erase({_hashResolver.ToHashedString(image_name), atlas_type});
    }

    // Register
    if (index >= static_cast<int>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }
    _allFonts[index] = std::move(font);

    BuildFont(index);

    return true;
}

static void StrCopy(char* to, size_t size, string_view from)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(to);
    RUNTIME_ASSERT(size > 0);

    if (from.length() == 0) {
        to[0] = 0;
        return;
    }

    if (from.length() >= size) {
        std::memcpy(to, from.data(), size - 1);
        to[size - 1] = 0;
    }
    else {
        std::memcpy(to, from.data(), from.length());
        to[from.length()] = 0;
    }
}

template<int Size>
static void StrCopy(char (&to)[Size], string_view from)
{
    STACK_TRACE_ENTRY();

    return StrCopy(to, Size, from);
}

static void StrGoTo(char*& str, char ch)
{
    STACK_TRACE_ENTRY();

    while (*str != 0 && *str != ch) {
        ++str;
    }
}

static void StrEraseInterval(char* str, uint len)
{
    STACK_TRACE_ENTRY();

    if (str == nullptr || len == 0) {
        return;
    }

    auto* str2 = str + len;
    while (*str2 != 0) {
        *str = *str2;
        ++str;
        ++str2;
    }

    *str = 0;
}

static void StrInsert(char* to, const char* from, uint from_len)
{
    STACK_TRACE_ENTRY();

    if (to == nullptr || from == nullptr) {
        return;
    }

    if (from_len == 0) {
        from_len = static_cast<uint>(string_view(from).length());
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

void SpriteManager::FormatText(FontFormatInfo& fi, int fmt_type)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    fi.PStr = fi.Str;

    auto* str = fi.PStr;
    auto flags = fi.Flags;
    auto* font = fi.CurFont;
    auto& r = fi.Region;
    auto infinity_w = (r.Left == r.Right);
    auto infinity_h = (r.Top == r.Bottom);
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
    uint* dots = nullptr;
    uint d_offs = 0;
    auto* str_ = str;
    string buf;
    if (fmt_type == FORMAT_TYPE_DRAW && !IsBitSet(flags, FT_NO_COLORIZE)) {
        dots = fi.ColorDots;
    }

    constexpr uint dots_history_len = 10;
    uint dots_history[dots_history_len] = {fi.DefColor};
    uint dots_history_cur = 0;

    while (*str_ != 0) {
        auto* s0 = str_;
        StrGoTo(str_, '|');
        auto* s1 = str_;
        StrGoTo(str_, ' ');
        auto* s2 = str_;

        if (dots != nullptr) {
            size_t d_len = static_cast<uint>(s2 - s1) + 1;
            uint d;

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
                    d = static_cast<uint>(std::strtoul(s1 + 2, nullptr, 16));
                }
                else {
                    d = static_cast<uint>(std::strtoul(s1 + 1, nullptr, 0));
                }

                if (dots_history_cur < dots_history_len - 1) {
                    dots_history[++dots_history_cur] = d;
                }
            }

            dots[static_cast<uint>(s1 - str) - d_offs] = d;
            d_offs += static_cast<uint>(d_len);
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
        uint letter_len = 0;
        auto letter = utf8::Decode(&str[i], &letter_len);
        i_advance = static_cast<int>(letter_len);

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
            if (curx > fi.MaxCurX) {
                fi.MaxCurX = curx;
            }

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
                letter = static_cast<uint8>(str[i]);
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
                //	if(fmt_type==FORMAT_TYPE_DRAW)
                //		for(int k=0,l=MAX_FOTEXT-(i+1);k<l;k++) fi.ColorDots[k]=fi.ColorDots[k+i+1];
                i = 0;
                i_advance = 0;
            }

            if (curx > fi.MaxCurX) {
                fi.MaxCurX = curx;
            }
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
    if (curx > fi.MaxCurX) {
        fi.MaxCurX = curx;
    }

    if (skip_line_end != 0) {
        auto len = static_cast<int>(string_view(str).length());
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
        auto j = 0;
        auto line_cur = 0;
        auto last_col = 0;
        for (; str[j] != 0; ++j) {
            if (str[j] == '\n') {
                line_cur++;
                if (line_cur >= fi.LinesAll - fi.LinesInRect) {
                    break;
                }
            }

            if (fi.ColorDots[j] != 0) {
                last_col = static_cast<int>(fi.ColorDots[j]);
            }
        }

        if (!IsBitSet(flags, FT_NO_COLORIZE)) {
            offs_col += j + 1;
            if (last_col != 0 && fi.ColorDots[j + 1] == 0) {
                fi.ColorDots[j + 1] = last_col;
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
        fi.LineWidth[i] = static_cast<int16>(curx);
    }

    auto can_count = false;
    auto spaces = 0;
    auto curstr = 0;
    for (auto i = 0, i_advance = 1;; i += i_advance) {
        uint letter_len = 0;
        auto letter = utf8::Decode(&str[i], &letter_len);
        i_advance = static_cast<int>(letter_len);

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
            fi.LineWidth[curstr] = static_cast<int16>(curx);
            cury += font->LineHeight + font->YAdvance;
            curx = r.Left;

            // Erase last spaces
            /*for(int j=i-1;spaces>0 && j>=0;j--)
               {
               if(str[j]==' ')
               {
               spaces--;
               str[j]='\r';
               }
               else if(str[j]!='\r') break;
               }*/

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
            // if(curx>fi.LineWidth[curstr]) fi.LineWidth[curstr]=curx;
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

void SpriteManager::DrawStr(const IRect& r, string_view str, uint flags, uint color /* = 0 */, int num_font /* = -1 */)
{
    STACK_TRACE_ENTRY();

    // Check
    if (str.empty()) {
        return;
    }

    // Get font
    auto* font = GetFont(num_font);
    if (font == nullptr) {
        return;
    }

    // FormatBuf
    if (color == 0 && _defFontColor != 0) {
        color = _defFontColor;
    }
    color = ApplyColorBrightness(color, _settings.Brightness);

    auto& fi = _fontFormatInfoBuf = {font, flags, r};
    StrCopy(fi.Str, str);
    fi.DefColor = color;
    FormatText(fi, FORMAT_TYPE_DRAW);
    if (fi.IsError) {
        return;
    }

    color = COLOR_SWAP_RB(color);

    const auto* str_ = fi.PStr;
    const auto offs_col = fi.OffsColDots;
    auto curx = fi.CurX;
    auto cury = fi.CurY;
    auto curstr = 0;
    auto* texture = IsBitSet(flags, FT_BORDERED) && font->FontTexBordered != nullptr ? font->FontTexBordered : font->FontTex;

    if (!IsBitSet(flags, FT_NO_COLORIZE)) {
        for (auto i = offs_col; i >= 0; i--) {
            if (fi.ColorDots[i] != 0) {
                if ((fi.ColorDots[i] & 0xFF000000) != 0) {
                    color = fi.ColorDots[i]; // With alpha
                }
                else {
                    color = (color & 0xFF000000) | (fi.ColorDots[i] & 0x00FFFFFF); // Still old alpha
                }
                color = ApplyColorBrightness(color, _settings.Brightness);
                color = COLOR_SWAP_RB(color);
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
    int i_advance;
    for (auto i = 0; str_[i] != 0; i += i_advance) {
        if (!IsBitSet(flags, FT_NO_COLORIZE)) {
            const auto new_color = fi.ColorDots[i + offs_col];
            if (new_color != 0) {
                if ((new_color & 0xFF000000) != 0) {
                    color = new_color; // With alpha
                }
                else {
                    color = (color & 0xFF000000) | (new_color & 0x00FFFFFF); // Still old alpha
                }
                color = ApplyColorBrightness(color, _settings.Brightness);
                color = COLOR_SWAP_RB(color);
            }
        }

        uint letter_len = 0;
        auto letter = utf8::Decode(&str_[i], &letter_len);
        i_advance = static_cast<int>(letter_len);

        switch (letter) {
        case ' ':
            curx += variable_space ? fi.LineSpaceWidth[curstr] : font->SpaceWidth;
            continue;
        case '\t':
            curx += font->SpaceWidth * 4;
            continue;
        case '\n':
            cury += font->LineHeight + font->YAdvance;
            curx = r.Left;
            curstr++;
            variable_space = false;
            if (IsBitSet(flags, FT_CENTERX)) {
                curx += (r.Right - fi.LineWidth[curstr]) / 2;
            }
            else if (IsBitSet(flags, FT_CENTERR)) {
                curx += r.Right - fi.LineWidth[curstr];
            }
            continue;
        case '\r':
            continue;
        default:
            auto it = font->Letters.find(letter);
            if (it == font->Letters.end()) {
                continue;
            }

            auto& l = it->second;

            const auto x = curx - l.OffsX - 1;
            const auto y = cury - l.OffsY - 1;
            const auto w = l.Width + 2;
            const auto h = l.Height + 2;

            auto& texture_uv = IsBitSet(flags, FT_BORDERED) ? l.TexBorderedPos : l.TexPos;
            const auto x1 = texture_uv[0];
            const auto y1 = texture_uv[1];
            const auto x2 = texture_uv[2];
            const auto y2 = texture_uv[3];

            ibuf[ipos++] = static_cast<uint16>(vpos + 0);
            ibuf[ipos++] = static_cast<uint16>(vpos + 1);
            ibuf[ipos++] = static_cast<uint16>(vpos + 3);
            ibuf[ipos++] = static_cast<uint16>(vpos + 1);
            ibuf[ipos++] = static_cast<uint16>(vpos + 2);
            ibuf[ipos++] = static_cast<uint16>(vpos + 3);

            auto& v0 = vbuf[vpos++];
            v0.PosX = static_cast<float>(x);
            v0.PosY = static_cast<float>(y + h);
            v0.TexU = x1;
            v0.TexV = y2;
            v0.EggTexU = 0.0f;
            v0.Color = color;

            auto& v1 = vbuf[vpos++];
            v1.PosX = static_cast<float>(x);
            v1.PosY = static_cast<float>(y);
            v1.TexU = x1;
            v1.TexV = y1;
            v1.EggTexU = 0.0f;
            v1.Color = color;

            auto& v2 = vbuf[vpos++];
            v2.PosX = static_cast<float>(x + w);
            v2.PosY = static_cast<float>(y);
            v2.TexU = x2;
            v2.TexV = y1;
            v2.EggTexU = 0.0f;
            v2.Color = color;

            auto& v3 = vbuf[vpos++];
            v3.PosX = static_cast<float>(x + w);
            v3.PosY = static_cast<float>(y + h);
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

auto SpriteManager::GetLinesCount(int width, int height, string_view str, int num_font /* = -1 */) -> int
{
    STACK_TRACE_ENTRY();

    if (width <= 0 || height <= 0) {
        return 0;
    }

    auto* font = GetFont(num_font);
    if (font == nullptr) {
        return 0;
    }

    if (str.empty()) {
        return height / (font->LineHeight + font->YAdvance);
    }

    const auto r = IRect(0, 0, width != 0 ? width : _settings.ScreenWidth, height != 0 ? height : _settings.ScreenHeight);
    auto& fi = _fontFormatInfoBuf = {font, 0, r};
    StrCopy(fi.Str, str);
    FormatText(fi, FORMAT_TYPE_LCOUNT);
    if (fi.IsError) {
        return 0;
    }

    return fi.LinesInRect;
}

auto SpriteManager::GetLinesHeight(int width, int height, string_view str, int num_font /* = -1 */) -> int
{
    STACK_TRACE_ENTRY();

    if (width <= 0 || height <= 0) {
        return 0;
    }

    const auto* font = GetFont(num_font);
    if (font == nullptr) {
        return 0;
    }

    const auto cnt = GetLinesCount(width, height, str, num_font);
    if (cnt <= 0) {
        return 0;
    }

    return cnt * font->LineHeight + (cnt - 1) * font->YAdvance;
}

auto SpriteManager::GetLineHeight(int num_font) -> int
{
    STACK_TRACE_ENTRY();

    const auto* font = GetFont(num_font);
    if (font == nullptr) {
        return 0;
    }

    return font->LineHeight;
}

auto SpriteManager::GetTextInfo(int width, int height, string_view str, int num_font, uint flags, int& tw, int& th, int& lines) -> bool
{
    STACK_TRACE_ENTRY();

    tw = th = lines = 0;

    auto* font = GetFont(num_font);
    if (font == nullptr) {
        return false;
    }

    if (str.empty()) {
        tw = width;
        th = height;
        lines = height / (font->LineHeight + font->YAdvance);
        return true;
    }

    auto& fi = _fontFormatInfoBuf = {font, flags, IRect(0, 0, width, height)};
    StrCopy(fi.Str, str);
    FormatText(fi, FORMAT_TYPE_LCOUNT);
    if (fi.IsError) {
        return false;
    }

    lines = fi.LinesInRect;
    th = fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance;
    tw = fi.MaxCurX - fi.Region.Left;
    return true;
}

auto SpriteManager::SplitLines(const IRect& r, string_view cstr, int num_font) -> vector<string>
{
    STACK_TRACE_ENTRY();

    vector<string> result;

    if (cstr.empty()) {
        return {};
    }

    auto* font = GetFont(num_font);
    if (font == nullptr) {
        return {};
    }

    auto& fi = _fontFormatInfoBuf = {font, 0, r};
    StrCopy(fi.Str, cstr);
    fi.StrLines = &result;
    FormatText(fi, FORMAT_TYPE_SPLIT);
    if (fi.IsError) {
        return {};
    }

    return result;
}

auto SpriteManager::HaveLetter(int num_font, uint letter) -> bool
{
    STACK_TRACE_ENTRY();

    const auto* font = GetFont(num_font);
    if (font == nullptr) {
        return false;
    }
    return font->Letters.count(letter) > 0;
}
