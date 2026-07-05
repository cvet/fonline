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
static constexpr float32_t MAP_LAYER_DEPTH_BIAS_PIXEL_BUDGET = 0.5f;
static constexpr size_t MAP_LAYER_DEPTH_BIAS_STEPS = static_cast<size_t>(DrawOrderType::Last) + 1;
static constexpr float32_t MAP_LAYER_DEPTH_BIAS = MAP_LAYER_DEPTH_BIAS_PIXEL_BUDGET / numeric_cast<float32_t>(MAP_LAYER_DEPTH_BIAS_STEPS);

Sprite::Sprite(ptr<SpriteManager> spr_mngr, isize32 size, ipos32 offset) :
    _sprMngr {spr_mngr},
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

    _sprMngr->_updateSprites.emplace(ptr<const Sprite>(this), weak_from_this());
}

SpriteManager::SpriteManager(ptr<RenderSettings> settings, ptr<IAppWindow> window, ptr<FileSystem> resources, ptr<GameTimer> game_time, ptr<EffectManager> effect_mngr, ptr<HashResolver> hash_resolver) :
    _settings {settings},
    _window {window},
    _resources {resources},
    _gameTimer {game_time},
    _rtMngr(window->GetRender(), [this]() FO_DEFERRED { Flush(); }),
    _atlasMngr(settings, &_rtMngr),
    _render {window->GetRender()},
    _input {window->GetInput()},
    _effectMngr {effect_mngr},
    _hashResolver {hash_resolver},
    _spritesDrawBuf {window->GetRender()->CreateDrawBuffer(false)},
    _primitiveDrawBuf {window->GetRender()->CreateDrawBuffer(false)},
    _flushDrawBuf {window->GetRender()->CreateDrawBuffer(false)},
    _spriteEffectDrawBuf {window->GetRender()->CreateDrawBuffer(false)}
{
    FO_STACK_TRACE_ENTRY();

    _flushVertCount = 4096;
    _dipQueue.reserve(1000);

    _spritesDrawBuf->Vertices.resize(_flushVertCount + 1024);
    _spritesDrawBuf->Indices.resize(_flushVertCount / 4 * 6 + 1024);
    _primitiveDrawBuf->Vertices.resize(1024);
    _primitiveDrawBuf->Indices.resize(1024);
    _flushDrawBuf->Vertices.resize(4);
    _flushDrawBuf->VertCount = 4;
    _flushDrawBuf->Indices = {0, 1, 3, 1, 2, 3};
    _flushDrawBuf->IndCount = 6;
    _spriteEffectDrawBuf->Vertices.resize(4);
    _spriteEffectDrawBuf->VertCount = 4;
    _spriteEffectDrawBuf->Indices = {0, 1, 3, 1, 2, 3};
    _spriteEffectDrawBuf->IndCount = 6;

    const isize32 screen_size = _window->GetScreenSize();

#if !FO_DIRECT_SPRITES_DRAW
    _rtMain = _rtMngr.CreateRenderTarget(false, screen_size, true);
#endif

    _eventUnsubscriber += (*_window->GetOnLowMemory()) += [this]() FO_DEFERRED { CleanupSpriteCache(); };
    _eventUnsubscriber += (*_window->GetOnScreenSizeChanged()) += [this]() FO_DEFERRED {
        const isize32 new_screen_size = _window->GetScreenSize();

        if (_rtMain) {
            auto rt_main = _rtMain.as_ptr();
            _rtMngr.ResizeRenderTarget(rt_main, new_screen_size);
        }
    };
}

auto SpriteManager::Random(int32_t min_value, int32_t max_value) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(min_value <= max_value, "Sprite random integer range has an inverted min/max", min_value, max_value);

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

    if (IsFullscreen() && !_window->IsVirtual()) {
        _pendingWindowedSize = size;
        return;
    }

    _pendingWindowedSize.reset();
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

    const isize32 current_screen_size = GetScreenSize();
    const int32_t diff_w = size.width - current_screen_size.width;
    const int32_t diff_h = size.height - current_screen_size.height;

    if (!_window->IsVirtual()) {
        if (!IsFullscreen()) {
            const ipos32 window_pos = _window->GetPosition();

            _window->SetPosition({window_pos.x - diff_w / 2, window_pos.y - diff_h / 2});
        }
        else {
            _windowSizeDiff.x += diff_w / 2;
            _windowSizeDiff.y += diff_h / 2;
        }
    }

    if (_window->IsVirtual()) {
        _settings->ScreenWidth = size.width;
        _settings->ScreenHeight = size.height;
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
            if (_pendingWindowedSize.has_value()) {
                _window->SetSize(_pendingWindowedSize.value());
                _pendingWindowedSize.reset();
            }

            if (_windowSizeDiff != ipos32 {}) {
                const ipos32 window_pos = _window->GetPosition();

                _window->SetPosition({window_pos.x - _windowSizeDiff.x, window_pos.y - _windowSizeDiff.y});
                _windowSizeDiff = {};
            }
        }
    }
}

void SpriteManager::SetMousePosition(ipos32 pos)
{
    FO_STACK_TRACE_ENTRY();

    _input->SetMousePosition(pos, _window);
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

void SpriteManager::RegisterSpriteFactory(unique_ptr<SpriteFactory> factory)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& ext : factory->GetExtensions()) {
        _spriteFactoryMap.insert_or_assign(ext, factory);
    }

    _spriteFactories.emplace_back(std::move(factory));
}

auto SpriteManager::GetSpriteFactory(std::type_index ti) -> nptr<SpriteFactory>
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i != _spriteFactories.size(); ++i) {
        auto factory = _spriteFactories[i].as_ptr();
        SpriteFactory& factory_ref = *factory;

        if (std::type_index(typeid(factory_ref)) == ti) {
            return factory;
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
        auto rt_main = _rtMain.as_ptr();
        _rtMngr.PushRenderTarget(rt_main);
        _rtMngr.ClearCurrentRenderTarget(ucolor::clear);
    }

    for (size_t i = 0; i != _spriteFactories.size(); ++i) {
        _spriteFactories[i]->Update();
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
        FO_VERIFY_AND_THROW(_rtMain == _rtMngr.GetCurrentRenderTarget(), "Sprite manager render target was changed unexpectedly");
        auto rt_main = _rtMain.as_ptr();
        _rtMngr.PopRenderTarget();

        if (_window->IsVirtual()) {
            const irect32 region_to = MakeAspectFitRect(rt_main->GetTexture()->Size, _window->GetSize());
            DrawRenderTarget(rt_main, false, nullptr, &region_to);
        }
        else {
            DrawRenderTarget(rt_main, false);
        }
    }

    FO_VERIFY_AND_THROW(_rtMngr.GetRenderTargetStack().empty(), "Sprite drawing left render targets on the render-target stack", _rtMngr.GetRenderTargetStack().size());
    FO_VERIFY_AND_THROW(_scissorStack.empty(), "Scissor stack must be empty before this operation");
}

auto SpriteManager::MakeAspectFitRect(isize32 source_size, isize32 target_size) const -> irect32
{
    FO_STACK_TRACE_ENTRY();

    if (source_size.width <= 0 || source_size.height <= 0 || target_size.width <= 0 || target_size.height <= 0) {
        return {};
    }

    const float32_t source_aspect = checked_div<float32_t>(numeric_cast<float32_t>(source_size.width), numeric_cast<float32_t>(source_size.height));
    const float32_t target_aspect = checked_div<float32_t>(numeric_cast<float32_t>(target_size.width), numeric_cast<float32_t>(target_size.height));
    const int32_t width = iround<int32_t>(source_aspect <= target_aspect ? numeric_cast<float32_t>(target_size.height) * source_aspect : numeric_cast<float32_t>(target_size.width));
    const int32_t height = iround<int32_t>(source_aspect <= target_aspect ? numeric_cast<float32_t>(target_size.height) : numeric_cast<float32_t>(target_size.width) / source_aspect);

    return {
        (target_size.width - width) / 2,
        (target_size.height - height) / 2,
        width,
        height,
    };
}

void SpriteManager::DrawTexture(ptr<const RenderTexture> tex, bool alpha_blend, nptr<const frect32> region_from, nptr<const irect32> region_to, nptr<RenderEffect> custom_effect)
{
    FO_STACK_TRACE_ENTRY();

    Flush();

    const_span<ptr<RenderTarget>> rt_stack = _rtMngr.GetRenderTargetStack();
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

    if (!region_from && !region_to) {
        auto& vbuf = _flushDrawBuf->Vertices;
        size_t vpos = 0;

        vbuf[vpos].PosX = 0.0f;
        vbuf[vpos].PosY = flip_to ? height_to_f : 0.0f;
        vbuf[vpos].PosZ = 0.0f;
        vbuf[vpos].TexU = 0.0f;
        vbuf[vpos].TexV = flip_from ? 1.0f : 0.0f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = 0.0f;
        vbuf[vpos].PosY = flip_to ? 0.0f : height_to_f;
        vbuf[vpos].PosZ = 0.0f;
        vbuf[vpos].TexU = 0.0f;
        vbuf[vpos].TexV = flip_from ? 0.0f : 1.0f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = width_to_f;
        vbuf[vpos].PosY = flip_to ? 0.0f : height_to_f;
        vbuf[vpos].PosZ = 0.0f;
        vbuf[vpos].TexU = 1.0f;
        vbuf[vpos].TexV = flip_from ? 0.0f : 1.0f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = width_to_f;
        vbuf[vpos].PosY = flip_to ? height_to_f : 0.0f;
        vbuf[vpos].PosZ = 0.0f;
        vbuf[vpos].TexU = 1.0f;
        vbuf[vpos].TexV = flip_from ? 1.0f : 0.0f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos].EggFlags[1] = 0.0f;
    }
    else {
        const auto rect_from = region_from ? *region_from : frect32(0.0f, 0.0f, width_from_f, height_from_f);
        const auto rect_to = frect32(region_to ? *region_to : irect32(0, 0, width_to_i, height_to_i));

        auto& vbuf = _flushDrawBuf->Vertices;
        size_t vpos = 0;

        vbuf[vpos].PosX = rect_to.x;
        vbuf[vpos].PosY = flip_to ? height_to_f - rect_to.y : rect_to.y;
        vbuf[vpos].PosZ = 0.0f;
        vbuf[vpos].TexU = rect_from.x / width_from_f;
        vbuf[vpos].TexV = flip_from ? 1.0f - (rect_from.y) / height_from_f : rect_from.y / height_from_f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = rect_to.x;
        vbuf[vpos].PosY = flip_to ? height_to_f - (rect_to.y + rect_to.height) : rect_to.y + rect_to.height;
        vbuf[vpos].PosZ = 0.0f;
        vbuf[vpos].TexU = rect_from.x / width_from_f;
        vbuf[vpos].TexV = flip_from ? 1.0f - (rect_from.y + rect_from.height) / height_from_f : (rect_from.y + rect_from.height) / height_from_f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = rect_to.x + rect_to.width;
        vbuf[vpos].PosY = flip_to ? height_to_f - (rect_to.y + rect_to.height) : rect_to.y + rect_to.height;
        vbuf[vpos].PosZ = 0.0f;
        vbuf[vpos].TexU = (rect_from.x + rect_from.width) / width_from_f;
        vbuf[vpos].TexV = flip_from ? 1.0f - (rect_from.y + rect_from.height) / height_from_f : (rect_from.y + rect_from.height) / height_from_f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos++].EggFlags[1] = 0.0f;

        vbuf[vpos].PosX = rect_to.x + rect_to.width;
        vbuf[vpos].PosY = flip_to ? height_to_f - rect_to.y : rect_to.y;
        vbuf[vpos].PosZ = 0.0f;
        vbuf[vpos].TexU = (rect_from.x + rect_from.width) / width_from_f;
        vbuf[vpos].TexV = flip_from ? 1.0f - (rect_from.y) / height_from_f : rect_from.y / height_from_f;
        vbuf[vpos].EggFlags[0] = 0.0f;
        vbuf[vpos].EggFlags[1] = 0.0f;
    }

    auto effect = custom_effect ? custom_effect.as_ptr() : _effectMngr->Effects.FlushRenderTarget.as_ptr();

    effect->MainTex = tex;
    effect->DisableBlending = !alpha_blend;
    _flushDrawBuf->Upload(effect->GetUsage());
    effect->DrawBuffer(_flushDrawBuf);
}

void SpriteManager::DrawRenderTarget(ptr<RenderTarget> rt, bool alpha_blend, nptr<const frect32> region_from, nptr<const irect32> region_to)
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

    const_span<ptr<RenderTarget>> rt_stack = _rtMngr.GetRenderTargetStack();

    if (!_scissorStack.empty() && !rt_stack.empty() && rt_stack.back() == _rtMain) {
        _render->EnableScissor(_scissorRect);
    }
}

void SpriteManager::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    const_span<ptr<RenderTarget>> rt_stack = _rtMngr.GetRenderTargetStack();

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

    shared_ptr<Sprite> spr = it->second->LoadSprite(path, atlas_type);

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
        FO_VERIFY_AND_THROW(dip.SourceEffect->GetUsage() == EffectUsage::QuadSprite, "Draw primitive source effect is not a quad sprite effect");

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

        dip.SourceEffect->DrawBuffer(_spritesDrawBuf, ipos, dip.IndicesCount, dip.MainTexture);

        ipos += dip.IndicesCount;
    }

    DisableScissor();

    _dipQueue.clear();

    FO_VERIFY_AND_THROW(ipos == _spritesDrawBuf->IndCount, "Sprite index buffer position is out of sync with draw buffer");

    _spritesDrawBuf->VertCount = 0;
    _spritesDrawBuf->IndCount = 0;
}

void SpriteManager::DrawSprite(ptr<const Sprite> spr, ipos32 pos, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    auto effect = spr->GetDrawEffectOr(_effectMngr->Effects.Iface.as_ptr());

    color = ApplyColorBrightness(color);

    const auto ind_count = spr->FillData(_spritesDrawBuf, frect32(fpos32(pos), fsize32(spr->GetSize())), {color, color});

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

void SpriteManager::DrawSpriteSize(ptr<const Sprite> spr, ipos32 pos, isize32 size, bool fit, bool center, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    DrawSpriteSizeExt(spr, fpos32(pos), fsize32(size), fit, center, false, color);
}

void SpriteManager::DrawSpriteSizeExt(ptr<const Sprite> spr, fpos32 pos, fsize32 size, bool fit, bool center, bool stretch, ucolor color)
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

    auto effect = spr->GetDrawEffectOr(_effectMngr->Effects.Iface.as_ptr());

    color = ApplyColorBrightness(color);

    const auto ind_count = spr->FillData(_spritesDrawBuf, {xf, yf, wf, hf}, {color, color});

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

auto SpriteManager::DrawSpriteRegion(ptr<const Sprite> spr, fpos32 uv0, fpos32 uv1, fpos32 pos, fsize32 size, ucolor color) -> bool
{
    FO_STACK_TRACE_ENTRY();

    ptr<const Sprite> source_spr = spr;

    if (nptr<const SpriteSheet> nullable_sheet = spr.dyn_cast<const SpriteSheet>()) {
        auto sheet = nullable_sheet.as_ptr();
        source_spr = sheet->GetCurSpr();
    }

    auto atlas_spr = source_spr.dyn_cast<const AtlasSprite>();

    if (!atlas_spr) {
        return false;
    }

    auto effect = spr->GetDrawEffectOr(_effectMngr->Effects.Iface.as_ptr());

    color = ApplyColorBrightness(color);

    const frect32 atlas_rect = atlas_spr->GetAtlasRect();
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
    v0.PosZ = 0.0f;
    v0.TexU = tex_left;
    v0.TexV = tex_bottom;
    v0.EggFlags[0] = 0.0f;
    v0.EggFlags[1] = 0.0f;
    v0.Color = color;

    auto& v1 = vbuf[vpos++];
    v1.PosX = pos.x;
    v1.PosY = pos.y;
    v1.PosZ = 0.0f;
    v1.TexU = tex_left;
    v1.TexV = tex_top;
    v1.EggFlags[0] = 0.0f;
    v1.EggFlags[1] = 0.0f;
    v1.Color = color;

    auto& v2 = vbuf[vpos++];
    v2.PosX = pos.x + size.width;
    v2.PosY = pos.y;
    v2.PosZ = 0.0f;
    v2.TexU = tex_right;
    v2.TexV = tex_top;
    v2.EggFlags[0] = 0.0f;
    v2.EggFlags[1] = 0.0f;
    v2.Color = color;

    auto& v3 = vbuf[vpos++];
    v3.PosX = pos.x + size.width;
    v3.PosY = pos.y + size.height;
    v3.PosZ = 0.0f;
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

void SpriteManager::DrawSpritePattern(ptr<const Sprite> spr, ipos32 pos, isize32 size, isize32 spr_size, ucolor color)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width > 0, "Size width must be positive", size.width);
    FO_VERIFY_AND_THROW(size.height > 0, "Size height must be positive", size.height);

    auto atlas_spr = spr.dyn_cast<const AtlasSprite>();
    if (!atlas_spr) {
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

    auto effect = atlas_spr->GetDrawEffectOr(_effectMngr->Effects.Iface.as_ptr());

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
            vbuf[vpos].PosZ = 0.0f;
            vbuf[vpos].TexU = atlas_spr->GetAtlasRect().x;
            vbuf[vpos].TexV = local_bottom;
            vbuf[vpos].EggFlags[0] = 0.0f;
            vbuf[vpos].EggFlags[1] = 0.0f;
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx;
            vbuf[vpos].PosY = yy;
            vbuf[vpos].PosZ = 0.0f;
            vbuf[vpos].TexU = atlas_spr->GetAtlasRect().x;
            vbuf[vpos].TexV = atlas_spr->GetAtlasRect().y;
            vbuf[vpos].EggFlags[0] = 0.0f;
            vbuf[vpos].EggFlags[1] = 0.0f;
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx + local_width;
            vbuf[vpos].PosY = yy;
            vbuf[vpos].PosZ = 0.0f;
            vbuf[vpos].TexU = local_right;
            vbuf[vpos].TexV = atlas_spr->GetAtlasRect().y;
            vbuf[vpos].EggFlags[0] = 0.0f;
            vbuf[vpos].EggFlags[1] = 0.0f;
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx + local_width;
            vbuf[vpos].PosY = yy + local_height;
            vbuf[vpos].PosZ = 0.0f;
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

void SpriteManager::SetEgg(TransparentEggSlot slot, mpos hex, nptr<const MapSprite> mspr)
{
    FO_STACK_TRACE_ENTRY();

    const auto slot_index = static_cast<size_t>(slot);

    if (!mspr) {
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

    for (auto& egg : _eggSlots) {
        egg.DrawOffset = {numeric_cast<float32_t>(draw_area.x), numeric_cast<float32_t>(draw_area.y)};
    }

    mspr_list.SortIfNeeded();

    const auto [range_begin, range_end] = mspr_list.GetDrawOrderRange(draw_oder_from, draw_oder_to);
    const_span<unique_ptr<MapSprite>> sprites = mspr_list.GetActiveSprites();
    const bool apply_brightness = _settings->Brightness != 0;

    const auto get_map_sprite_proj = [](const MapSprite* mspr) -> vec3 {
        const float32_t elevation = numeric_cast<float32_t>(mspr->GetElevation());
        return GeometryHelper::ProjectWorldToMap(GeometryHelper::GetHexWorldPos(mspr->GetHex(), mspr->GetMapRootOffset(), elevation));
    };
    const auto is_standing_sprite = [](DrawOrderType draw_order) -> bool { //
        return draw_order >= DrawOrderType::NormalBegin && draw_order <= DrawOrderType::NormalEnd;
    };

    _directDrawSprites.clear();

    for (uint32_t i = range_begin; i < range_end; i++) {
        const auto& mspr = sprites[i];
        FO_VERIFY_AND_THROW(mspr->IsValid(), "Map sprite is invalid");

        if (mspr->IsHidden()) {
            continue;
        }

        auto nullable_spr = mspr->GetSprite();
        FO_VERIFY_AND_THROW(nullable_spr, "Missing required sprite");
        auto spr = nullable_spr.as_ptr();

        irect32 mspr_rect = mspr->GetDrawRect();
        mspr_rect.x -= draw_area.x;
        mspr_rect.y -= draw_area.y;

        // Skip not visible
        if (mspr_rect.x > draw_area.width || mspr_rect.x + mspr_rect.width < 0 || mspr_rect.y > draw_area.height || mspr_rect.y + mspr_rect.height < 0) {
            continue;
        }

        if (spr->IsDirectDraw()) {
            const vec3 map_proj = get_map_sprite_proj(mspr.get());
            // Direct-draw sprites contain real scene geometry; keep only a tiny ground separation instead of
            // inheriting their late draw-order bias, otherwise particles become closer than critters/scenery.
            // Proxy-geometry map sprites write unbiased world depth, so one step is enough to avoid terrain
            // z-fighting without shifting the particle anchor toward the camera.
            const float32_t direct_layer_bias = MAP_LAYER_DEPTH_BIAS;
            const float32_t depth = map_proj.z + direct_layer_bias;
            // scene_pos == GetDrawRootPos() - draw_area == mspr_rect.pos + sprite root offset (already computed
            // by GetDrawRect above), so reuse mspr_rect instead of calling GetDrawRootPos a second time.
            const ipos32 root_offset = mspr->GetSpriteRootOffset();
            const fpos32 scene_pos = {numeric_cast<float32_t>(mspr_rect.x + root_offset.x), numeric_cast<float32_t>(mspr_rect.y + root_offset.y)};
            _directDrawSprites.emplace_back(DirectDrawSprite {.Spr = spr, .ScenePos = scene_pos, .Depth = depth});
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
        auto nullable_light = mspr->GetLight();

        if (nullable_light) {
            const auto mix_light = [](ucolor& c, ptr<const ucolor> l, nptr<const ucolor> nullable_l2) {
                ptr<const ucolor> l2 = l;

                if (nullable_l2) {
                    l2 = nullable_l2.as_ptr();
                }

                c.comp.r = numeric_cast<uint8_t>(std::min(c.comp.r + (l->comp.r + l2->comp.r) / 2, 255));
                c.comp.g = numeric_cast<uint8_t>(std::min(c.comp.g + (l->comp.g + l2->comp.g) / 2, 255));
                c.comp.b = numeric_cast<uint8_t>(std::min(c.comp.b + (l->comp.b + l2->comp.b) / 2, 255));
            };

            auto light = nullable_light.as_ptr();
            mix_light(color_r, light, mspr->GetLightRight());
            mix_light(color_l, light, mspr->GetLightLeft());
        }

        // Alpha
        auto alpha = mspr->GetAlpha();

        if (alpha) {
            color_r.comp.a = *alpha;
            color_l.comp.a = *alpha;
        }

        // Fix color
        if (apply_brightness) {
            color_r = ApplyColorBrightness(color_r);
            color_l = ApplyColorBrightness(color_l);
        }

        // Choose effect
        auto effect = mspr->GetDrawEffect();

        if (!effect) {
            effect = spr->GetDrawEffectOr(_effectMngr->Effects.Generic.as_ptr());
        }

        // Fill buffer
        const float32_t xf = numeric_cast<float32_t>(mspr_rect.x);
        const float32_t yf = numeric_cast<float32_t>(mspr_rect.y);
        const float32_t wf = numeric_cast<float32_t>(spr->GetSize().width);
        const float32_t hf = numeric_cast<float32_t>(spr->GetSize().height);
        const size_t start_vpos = _spritesDrawBuf->VertCount;
        const size_t ind_count = spr->FillData(_spritesDrawBuf, {xf, yf, wf, hf}, {color_l, color_r});

        auto& vbuf = _spritesDrawBuf->Vertices;
        const DrawOrderType draw_order = mspr->GetDrawOrder();
        const bool standing_sprite = is_standing_sprite(draw_order);
        const vec3 sprite_proj = get_map_sprite_proj(mspr.get());
        const float32_t pos_z = sprite_proj.z;

        for (size_t j = start_vpos; j < _spritesDrawBuf->VertCount; j++) {
            vbuf[j].PosZ = pos_z;
        }

        // Rotation and map-projected flattening.
        const int16_t angle_deg = mspr->GetAngle();
        const bool use_map_projected = mspr->GetMapProjected();

        if (angle_deg != 0 || use_map_projected) {
            const float32_t rad = numeric_cast<float32_t>(angle_deg) * (3.14159265f / 180.0f);
            const float32_t cs = angle_deg != 0 ? std::cos(rad) : 1.0f;
            const float32_t sn = angle_deg != 0 ? std::sin(rad) : 0.0f;
            const float32_t y_scale = use_map_projected ? std::cos(_settings->MapCameraAngle * (3.14159265f / 180.0f)) : 1.0f;
            const float32_t cx = xf + wf * 0.5f;
            const float32_t cy = yf + hf * 0.5f;

            for (size_t j = start_vpos; j < _spritesDrawBuf->VertCount; j++) {
                const float32_t dx = vbuf[j].PosX - cx;
                const float32_t dy = vbuf[j].PosY - cy;
                vbuf[j].PosX = cx + dx * cs - dy * sn;
                vbuf[j].PosY = cy + (dx * sn + dy * cs) * y_scale;
            }
        }

        if (standing_sprite) {
            const float32_t scene_pos_y = numeric_cast<float32_t>(mspr_rect.y + mspr->GetSpriteRootOffset().y - mspr->GetRootOffset().y);
            const float32_t angle_rad = GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT;
            const float32_t sin_a = std::sin(angle_rad);
            const float32_t cos_a = std::cos(angle_rad);
            const float32_t tan_a = sin_a / cos_a;

            for (size_t j = start_vpos; j < _spritesDrawBuf->VertCount; j++) {
                const float32_t map_y = sprite_proj.y + (vbuf[j].PosY - scene_pos_y);
                vbuf[j].PosZ = sprite_proj.z - (map_y - sprite_proj.y) * tan_a;
            }
        }

        // Setup eggs
        const bool use_first_egg = use_egg && CheckEggAppearence(TransparentEggSlot::Primary, mspr->GetHex(), mspr->GetEggAppearence());
        const bool use_second_egg = use_egg && CheckEggAppearence(TransparentEggSlot::Secondary, mspr->GetHex(), mspr->GetEggAppearence());

        if (use_first_egg || use_second_egg) {
            for (size_t j = start_vpos; j < _spritesDrawBuf->VertCount; j++) {
                vbuf[j].EggFlags[0] = use_first_egg ? EGG_ENABLED_FLAG : 0.0f;
                vbuf[j].EggFlags[1] = use_second_egg ? EGG_ENABLED_FLAG : 0.0f;
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
    }

    Flush();

    for (const auto& dd : _directDrawSprites) {
        dd.Spr->DrawInScene(dd.ScenePos, dd.Depth);
    }

    _directDrawSprites.clear();
}

auto SpriteManager::SpriteHitTest(ptr<const Sprite> spr, ipos32 pos) const -> bool
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

void SpriteManager::DrawPoints(const_span<PrimitivePoint> points, RenderPrimitiveType prim, nptr<const irect32> draw_area, nptr<RenderEffect> custom_effect)
{
    FO_STACK_TRACE_ENTRY();

    if (points.empty()) {
        return;
    }

    Flush();

    auto effect = custom_effect ? custom_effect.as_ptr() : _effectMngr->Effects.Primitive.as_ptr();

    // Check primitives
    const auto count = points.size();

    if (count == 0) {
        return;
    }

    switch (prim) {
    case RenderPrimitiveType::PointList:
        break;
    case RenderPrimitiveType::LineList:
        FO_VERIFY_AND_THROW(count % 2 == 0, "LineList primitive requires an even number of sprite vertices", count, prim);
        break;
    case RenderPrimitiveType::LineStrip:
        FO_VERIFY_AND_THROW(count > 1, "Sprite corner count is too small", count);
        break;
    case RenderPrimitiveType::TriangleList:
        FO_VERIFY_AND_THROW(count % 3 == 0, "TriangleList primitive requires sprite vertices grouped by triples", count, prim);
        break;
    case RenderPrimitiveType::TriangleStrip:
        FO_VERIFY_AND_THROW(count > 2, "Sprite triangle corner count is too small", count);
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

    const fpos32 draw_area_offset = draw_area ? fpos32 {numeric_cast<float32_t>(draw_area->x), numeric_cast<float32_t>(draw_area->y)} : fpos32 {};

    for (size_t i = 0; i < count; i++) {
        const auto& point = points[i];
        ipos32 pos = point.PointPos;

        if (point.PointOffset) {
            pos += *point.PointOffset;
        }
        if (draw_area) {
            pos -= ipos32(draw_area->x, draw_area->y);
        }

        vbuf[i].PosX = numeric_cast<float32_t>(pos.x);
        vbuf[i].PosY = numeric_cast<float32_t>(pos.y);
        vbuf[i].PosZ = point.PointPosZ;
        vbuf[i].Color = point.PPointColor ? *point.PPointColor : point.PointColor;

        // TexU/TexV = caller's PrimitivePoint::TexUV + draw_area top-left.
        vbuf[i].TexU = point.TexUV.x + draw_area_offset.x;
        vbuf[i].TexV = point.TexUV.y + draw_area_offset.y;
        // Free-form per-vertex data forwarded verbatim to location 3 (InTexEggCoord).
        vbuf[i].EggFlags[0] = point.EggData.x;
        vbuf[i].EggFlags[1] = point.EggData.y;

        ibuf[i] = numeric_cast<vindex_t>(i);
    }

    _primitiveDrawBuf->PrimType = prim;
    _primitiveDrawBuf->Upload(effect->GetUsage(), count, count);

    EnableScissor();
    effect->DrawBuffer(_primitiveDrawBuf, 0, count);
    DisableScissor();
}

void SpriteManager::DrawSpriteWithEffect(ptr<const Sprite> spr, ipos32 pos, ucolor color, ptr<RenderEffect> effect, int32_t padding)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(padding >= 0, "Padding is negative");
    FO_VERIFY_AND_THROW(effect->GetUsage() == EffectUsage::QuadSprite, "Effect usage is not quad sprite");

    ptr<const Sprite> source_spr = spr;

    if (nptr<const SpriteSheet> nullable_sheet = spr.dyn_cast<const SpriteSheet>()) {
        auto sheet = nullable_sheet.as_ptr();
        source_spr = sheet->GetCurSpr();
    }

    auto atlas_spr = source_spr.dyn_cast<const AtlasSprite>();

    if (!atlas_spr) {
        return;
    }

    Flush();

    auto texture = atlas_spr->GetAtlas()->GetTexture();
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
    vbuf[vpos].PosZ = 0.0f;
    vbuf[vpos].TexU = textureuv.x;
    vbuf[vpos].TexV = textureuv.y + textureuv.height;
    vbuf[vpos].EggFlags[0] = 0.0f;
    vbuf[vpos].EggFlags[1] = 0.0f;
    vbuf[vpos++].Color = color;

    vbuf[vpos].PosX = borders.x;
    vbuf[vpos].PosY = borders.y;
    vbuf[vpos].PosZ = 0.0f;
    vbuf[vpos].TexU = textureuv.x;
    vbuf[vpos].TexV = textureuv.y;
    vbuf[vpos].EggFlags[0] = 0.0f;
    vbuf[vpos].EggFlags[1] = 0.0f;
    vbuf[vpos++].Color = color;

    vbuf[vpos].PosX = borders.x + borders.width;
    vbuf[vpos].PosY = borders.y;
    vbuf[vpos].PosZ = 0.0f;
    vbuf[vpos].TexU = textureuv.x + textureuv.width;
    vbuf[vpos].TexV = textureuv.y;
    vbuf[vpos].EggFlags[0] = 0.0f;
    vbuf[vpos].EggFlags[1] = 0.0f;
    vbuf[vpos++].Color = color;

    vbuf[vpos].PosX = borders.x + borders.width;
    vbuf[vpos].PosY = borders.y + borders.height;
    vbuf[vpos].PosZ = 0.0f;
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
    effect->DrawBuffer(_spriteEffectDrawBuf, 0, std::nullopt, texture);
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
