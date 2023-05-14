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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "DiskFileSystem.h"
#include "Log.h"
#include "StringUtils.h"

constexpr int ATLAS_SPRITES_PADDING = 1;

static auto ApplyColorBrightness(uint color, int brightness) -> uint
{
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

TextureAtlas::SpaceNode::SpaceNode(int x, int y, int width, int height) :
    PosX {x},
    PosY {y},
    Width {width},
    Height {height}
{
    STACK_TRACE_ENTRY();
}

auto TextureAtlas::SpaceNode::FindPosition(int width, int height, int& x, int& y) -> bool
{
    STACK_TRACE_ENTRY();

    auto result = false;

    if (Child1) {
        result = Child1->FindPosition(width, height, x, y);
    }
    if (!result && Child2) {
        result = Child2->FindPosition(width, height, x, y);
    }

    if (!result && !Busy && Width >= width && Height >= height) {
        result = true;
        Busy = true;

        x = PosX;
        y = PosY;

        if (Width == width && Height > height) {
            Child1 = std::make_unique<SpaceNode>(PosX, PosY + height, Width, Height - height);
        }
        else if (Height == height && Width > width) {
            Child1 = std::make_unique<SpaceNode>(PosX + width, PosY, Width - width, Height);
        }
        else if (Width > width && Height > height) {
            Child1 = std::make_unique<SpaceNode>(PosX + width, PosY, Width - width, height);
            Child2 = std::make_unique<SpaceNode>(PosX, PosY + height, Width, Height - height);
        }
    }
    return result;
}

auto AtlasSprite::FillData(RenderDrawBuffer* dbuf, const FRect& pos, const tuple<uint, uint>& colors) const -> size_t
{
    NO_STACK_TRACE_ENTRY();

    dbuf->CheckAllocBuf(4, 6);

    auto& vbuf = dbuf->Vertices;
    auto& vpos = dbuf->VertCount;
    auto& ibuf = dbuf->Indices;
    auto& ipos = dbuf->IndCount;

    ibuf[ipos++] = static_cast<uint16>(vpos + 0);
    ibuf[ipos++] = static_cast<uint16>(vpos + 1);
    ibuf[ipos++] = static_cast<uint16>(vpos + 3);
    ibuf[ipos++] = static_cast<uint16>(vpos + 1);
    ibuf[ipos++] = static_cast<uint16>(vpos + 2);
    ibuf[ipos++] = static_cast<uint16>(vpos + 3);

    auto& v0 = vbuf[vpos++];
    v0.PosX = pos.Left;
    v0.PosY = pos.Bottom;
    v0.TexU = AtlasRect.Left;
    v0.TexV = AtlasRect.Bottom;
    v0.Color = std::get<0>(colors);

    auto& v1 = vbuf[vpos++];
    v1.PosX = pos.Left;
    v1.PosY = pos.Top;
    v1.TexU = AtlasRect.Left;
    v1.TexV = AtlasRect.Top;
    v1.Color = std::get<0>(colors);

    auto& v2 = vbuf[vpos++];
    v2.PosX = pos.Right;
    v2.PosY = pos.Top;
    v2.TexU = AtlasRect.Right;
    v2.TexV = AtlasRect.Top;
    v2.Color = std::get<1>(colors);

    auto& v3 = vbuf[vpos++];
    v3.PosX = pos.Right;
    v3.PosY = pos.Bottom;
    v3.TexU = AtlasRect.Right;
    v3.TexV = AtlasRect.Bottom;
    v3.Color = std::get<1>(colors);

    return 6;
}

auto AnyFrames::GetSpr(uint num_frm) const -> Sprite*
{
    STACK_TRACE_ENTRY();

    return Spr[num_frm % CntFrm];
}

auto AnyFrames::GetCurSpr(time_point time) const -> Sprite*
{
    STACK_TRACE_ENTRY();

    const auto ticks = time_duration_to_ms<size_t>(time.time_since_epoch());
    return CntFrm > 1 ? Spr[ticks % Ticks * 100 / Ticks * CntFrm / 100] : Spr[0];
}

auto AnyFrames::GetCurSprIndex(time_point time) const -> uint
{
    STACK_TRACE_ENTRY();

    const auto ticks = time_duration_to_ms<size_t>(time.time_since_epoch());
    return CntFrm > 1 ? ticks % Ticks * 100 / Ticks * CntFrm / 100 : 0;
}

auto AnyFrames::GetDir(uint dir) -> AnyFrames*
{
    STACK_TRACE_ENTRY();

    return dir == 0 || DirCount == 1 ? this : Dirs[dir - 1];
}

SpriteManager::SpriteManager(RenderSettings& settings, AppWindow* window, FileSystem& resources, EffectManager& effect_mngr) :
    _settings {settings},
    _window {window},
    _resources {resources},
    _effectMngr {effect_mngr}
{
    STACK_TRACE_ENTRY();

    _flushVertCount = 4096;
    _allAtlases.reserve(100);
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

    _borderBuf.resize(App->Render.MAX_ATLAS_SIZE);

#if !FO_DIRECT_SPRITES_DRAW
    _rtMain = CreateRenderTarget(false, RenderTarget::SizeType::Screen, 0, 0, true);
#endif
    _rtContours = CreateRenderTarget(false, RenderTarget::SizeType::Map, 0, 0, false);
    _rtContoursMid = CreateRenderTarget(false, RenderTarget::SizeType::Map, 0, 0, false);

    _dummyAnim = new AnyFrames();
    _dummyAnim->CntFrm = 1;
    _dummyAnim->Ticks = 100;

    _eventUnsubscriber += _window->OnScreenSizeChanged += [this] { OnScreenSizeChanged(); };
}

SpriteManager::~SpriteManager()
{
    STACK_TRACE_ENTRY();

    _window->Destroy();

    delete _dummyAnim;
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

    if (!App->Settings.Fullscreen) {
        const auto [x, y] = _window->GetPosition();
        _window->SetPosition(x - diff_w / 2, y - diff_h / 2);
    }
    else {
        _windowSizeDiffX += diff_w / 2;
        _windowSizeDiffY += diff_h / 2;
    }

    _window->SetScreenSize(w, h);
}

void SpriteManager::SwitchFullscreen()
{
    STACK_TRACE_ENTRY();

    if (!App->Settings.Fullscreen) {
        if (_window->ToggleFullscreen(true)) {
            App->Settings.Fullscreen = true;
        }
    }
    else {
        if (_window->ToggleFullscreen(false)) {
            App->Settings.Fullscreen = false;

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

auto SpriteManager::EnableFullscreen() -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _window->ToggleFullscreen(true);
}

auto SpriteManager::DisableFullscreen() -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _window->ToggleFullscreen(false);
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

void SpriteManager::BeginScene(uint clear_color)
{
    STACK_TRACE_ENTRY();

    if (_rtMain != nullptr) {
        PushRenderTarget(_rtMain);
        ClearCurrentRenderTarget(clear_color);
    }

    // Draw particles to atlas
    if (!_autoDrawParticles.empty()) {
        for (auto* particle_spr : _autoDrawParticles) {
            if (particle_spr->Particle->NeedForceDraw() || particle_spr->Particle->NeedDraw()) {
                DrawParticleToAtlas(particle_spr);
            }
        }
    }

#if FO_ENABLE_3D
    // Draw models to atlas
    if (!_autoDrawModels.empty()) {
        for (auto* model_spr : _autoDrawModels) {
            if (model_spr->Model->NeedForceDraw() || model_spr->Model->NeedDraw()) {
                DrawModelToAtlas(model_spr);
            }
        }
    }
#endif
}

void SpriteManager::EndScene()
{
    STACK_TRACE_ENTRY();

    Flush();

    if (_rtMain != nullptr) {
        PopRenderTarget();
        DrawRenderTarget(_rtMain, false);
    }
}

void SpriteManager::OnScreenSizeChanged()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    // Reallocate fullscreen render targets
    for (auto&& rt : _rtAll) {
        if (rt->Size != RenderTarget::SizeType::Custom) {
            AllocateRenderTargetTexture(rt.get(), rt->MainTex->LinearFiltered, rt->MainTex->WithDepth);
        }
    }
}

auto SpriteManager::CreateRenderTarget(bool with_depth, RenderTarget::SizeType size, int width, int height, bool linear_filtered) -> RenderTarget*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(width >= 0);
    RUNTIME_ASSERT(height >= 0);

    Flush();

    auto rt = std::make_unique<RenderTarget>();
    rt->Size = size;
    rt->BaseWidth = width;
    rt->BaseHeight = height;

    AllocateRenderTargetTexture(rt.get(), linear_filtered, with_depth);

    _rtAll.push_back(std::move(rt));
    return _rtAll.back().get();
}

void SpriteManager::AllocateRenderTargetTexture(RenderTarget* rt, bool linear_filtered, bool with_depth)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto tex_width = rt->BaseWidth;
    auto tex_height = rt->BaseHeight;

    if (rt->Size == RenderTarget::SizeType::Screen) {
        tex_width += _settings.ScreenWidth;
        tex_height += _settings.ScreenHeight;
    }
    else if (rt->Size == RenderTarget::SizeType::Map) {
        tex_width += _settings.ScreenWidth;
        tex_height += _settings.ScreenHeight - _settings.ScreenHudHeight;
    }

    tex_width = std::max(tex_width, 1);
    tex_height = std::max(tex_height, 1);

    RUNTIME_ASSERT(tex_width > 0);
    RUNTIME_ASSERT(tex_height > 0);

    rt->MainTex = unique_ptr<RenderTexture>(App->Render.CreateTexture(tex_width, tex_height, linear_filtered, with_depth));

    rt->MainTex->FlippedHeight = App->Render.IsRenderTargetFlipped();

    auto* prev_tex = App->Render.GetRenderTarget();
    App->Render.SetRenderTarget(rt->MainTex.get());
    App->Render.ClearRenderTarget(0, with_depth);
    App->Render.SetRenderTarget(prev_tex);
}

void SpriteManager::PushRenderTarget(RenderTarget* rt)
{
    STACK_TRACE_ENTRY();

    Flush();

    const auto redundant = !_rtStack.empty() && _rtStack.back() == rt;
    _rtStack.push_back(rt);
    if (!redundant) {
        Flush();
        App->Render.SetRenderTarget(rt->MainTex.get());
        rt->LastPixelPicks.clear();
    }
}

void SpriteManager::PopRenderTarget()
{
    STACK_TRACE_ENTRY();

    const auto redundant = _rtStack.size() > 2 && _rtStack.back() == _rtStack[_rtStack.size() - 2];
    _rtStack.pop_back();

    if (!redundant) {
        Flush();

        if (!_rtStack.empty()) {
            App->Render.SetRenderTarget(_rtStack.back()->MainTex.get());
        }
        else {
            App->Render.SetRenderTarget(nullptr);
        }
    }
}

void SpriteManager::DrawRenderTarget(const RenderTarget* rt, bool alpha_blend, const IRect* region_from, const IRect* region_to)
{
    STACK_TRACE_ENTRY();

    DrawTexture(rt->MainTex.get(), alpha_blend, region_from, region_to, rt->CustomDrawEffect);
}

void SpriteManager::DrawTexture(const RenderTexture* tex, bool alpha_blend, const IRect* region_from, const IRect* region_to, RenderEffect* custom_effect)
{
    STACK_TRACE_ENTRY();

    Flush();

    const auto flipped_height = tex->FlippedHeight;
    const auto width_from_i = tex->Width;
    const auto height_from_i = tex->Height;
    const auto width_to_i = _rtStack.empty() ? _settings.ScreenWidth : _rtStack.back()->MainTex->Width;
    const auto height_to_i = _rtStack.empty() ? _settings.ScreenHeight : _rtStack.back()->MainTex->Height;
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
        vbuf[pos++].TexV = 0.0f;

        vbuf[pos].PosX = 0.0f;
        vbuf[pos].PosY = flipped_height ? 0.0f : height_to_f;
        vbuf[pos].TexU = 0.0f;
        vbuf[pos++].TexV = 1.0f;

        vbuf[pos].PosX = width_to_f;
        vbuf[pos].PosY = flipped_height ? 0.0f : height_to_f;
        vbuf[pos].TexU = 1.0f;
        vbuf[pos++].TexV = 1.0f;

        vbuf[pos].PosX = width_to_f;
        vbuf[pos].PosY = flipped_height ? height_to_f : 0.0f;
        vbuf[pos].TexU = 1.0f;
        vbuf[pos].TexV = 0.0f;
    }
    else {
        const FRect rect_from = region_from != nullptr ? *region_from : IRect(0, 0, width_from_i, height_from_i);
        const FRect rect_to = region_to != nullptr ? *region_to : IRect(0, 0, width_to_i, height_to_i);

        auto& vbuf = _flushDrawBuf->Vertices;
        auto pos = 0;

        vbuf[pos].PosX = rect_to.Left;
        vbuf[pos].PosY = flipped_height ? rect_to.Bottom : rect_to.Top;
        vbuf[pos].TexU = rect_from.Left / width_from_f;
        vbuf[pos++].TexV = flipped_height ? 1.0f - rect_from.Bottom / height_from_f : rect_from.Top / height_from_f;

        vbuf[pos].PosX = rect_to.Left;
        vbuf[pos].PosY = flipped_height ? rect_to.Top : rect_to.Bottom;
        vbuf[pos].TexU = rect_from.Left / width_from_f;
        vbuf[pos++].TexV = flipped_height ? 1.0f - rect_from.Top / height_from_f : rect_from.Bottom / height_from_f;

        vbuf[pos].PosX = rect_to.Right;
        vbuf[pos].PosY = flipped_height ? rect_to.Top : rect_to.Bottom;
        vbuf[pos].TexU = rect_from.Right / width_from_f;
        vbuf[pos++].TexV = flipped_height ? 1.0f - rect_from.Top / height_from_f : rect_from.Bottom / height_from_f;

        vbuf[pos].PosX = rect_to.Right;
        vbuf[pos].PosY = flipped_height ? rect_to.Bottom : rect_to.Top;
        vbuf[pos].TexU = rect_from.Right / width_from_f;
        vbuf[pos].TexV = flipped_height ? 1.0f - rect_from.Bottom / height_from_f : rect_from.Top / height_from_f;
    }

    auto* effect = custom_effect != nullptr ? custom_effect : _effectMngr.Effects.FlushRenderTarget;

    effect->MainTex = tex;
    effect->DisableBlending = !alpha_blend;
    _flushDrawBuf->Upload(effect->Usage);
    effect->DrawBuffer(_flushDrawBuf);
}

auto SpriteManager::GetRenderTargetPixel(RenderTarget* rt, int x, int y) const -> uint
{
    STACK_TRACE_ENTRY();

#if FO_NO_TEXTURE_LOOKUP
    return 0xFFFFFFFF;

#else

    // Try find in last picks
    for (auto&& pix : rt->LastPixelPicks) {
        if (std::get<0>(pix) == x && std::get<1>(pix) == y) {
            return std::get<2>(pix);
        }
    }

    // Read one pixel
    auto color = rt->MainTex->GetTexturePixel(x, y);

    // Refresh picks
    rt->LastPixelPicks.emplace(rt->LastPixelPicks.begin(), x, y, color);
    if (rt->LastPixelPicks.size() > MAX_STORED_PIXEL_PICKS) {
        rt->LastPixelPicks.pop_back();
    }

    return color;
#endif
}

void SpriteManager::ClearCurrentRenderTarget(uint color, bool with_depth)
{
    STACK_TRACE_ENTRY();

    App->Render.ClearRenderTarget(color, with_depth);
}

void SpriteManager::DeleteRenderTarget(RenderTarget* rt)
{
    STACK_TRACE_ENTRY();

    const auto it = std::find_if(_rtAll.begin(), _rtAll.end(), [rt](auto&& check_rt) { return check_rt.get() == rt; });
    RUNTIME_ASSERT(it != _rtAll.end());
    _rtAll.erase(it);
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

    NON_CONST_METHOD_HINT();

    if (!_scissorStack.empty() && !_rtStack.empty() && _rtStack.back() == _rtMain) {
        App->Render.EnableScissor(_scissorRect.Left, _scissorRect.Top, _scissorRect.Width(), _scissorRect.Height());
    }
}

void SpriteManager::DisableScissor()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!_scissorStack.empty() && !_rtStack.empty() && _rtStack.back() == _rtMain) {
        App->Render.DisableScissor();
    }
}

void SpriteManager::AccumulateAtlasData()
{
    STACK_TRACE_ENTRY();

    _accumulatorActive = true;
}

void SpriteManager::FlushAccumulatedAtlasData()
{
    STACK_TRACE_ENTRY();

    _accumulatorActive = false;

    if (_spritesAccumulator.empty()) {
        return;
    }

    // Sort by size
    std::sort(_spritesAccumulator.begin(), _spritesAccumulator.end(), [](auto&& spr1, auto&& spr2) { return std::get<0>(spr1)->Width * std::get<0>(spr1)->Height > std::get<0>(spr2)->Width * std::get<0>(spr2)->Height; });

    for (auto&& [atlas_spr, data, atlas_type] : _spritesAccumulator) {
        FillAtlas(atlas_spr, data, atlas_type);
        delete[] data;
    }

    _spritesAccumulator.clear();
}

auto SpriteManager::IsAccumulateAtlasActive() const -> bool
{
    STACK_TRACE_ENTRY();

    return _accumulatorActive;
}

auto SpriteManager::CreateAtlas(int request_width, int request_height, AtlasType atlas_type) -> TextureAtlas*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(request_width > 0);
    RUNTIME_ASSERT(request_height > 0);

    auto result_width = request_width;
    auto result_height = request_height;

    auto atlas = std::make_unique<TextureAtlas>();
    atlas->Type = atlas_type;

    switch (atlas->Type) {
    case AtlasType::Static:
        result_width = std::min(AppRender::MAX_ATLAS_WIDTH, 4096);
        result_height = std::min(AppRender::MAX_ATLAS_HEIGHT, 4096);
        break;
    case AtlasType::Dynamic:
        result_width = std::min(AppRender::MAX_ATLAS_WIDTH, 2048);
        result_height = std::min(AppRender::MAX_ATLAS_HEIGHT, 8192);
        break;
    case AtlasType::MeshTextures:
        result_width = std::min(AppRender::MAX_ATLAS_WIDTH, 1024);
        result_height = std::min(AppRender::MAX_ATLAS_HEIGHT, 2048);
        break;
    case AtlasType::OneImage:
        break;
    }

    atlas->RTarg = CreateRenderTarget(false, RenderTarget::SizeType::Custom, result_width, result_height, _settings.AtlasLinearFiltration);
    atlas->RTarg->LastPixelPicks.reserve(MAX_STORED_PIXEL_PICKS);
    atlas->MainTex = atlas->RTarg->MainTex.get();
    atlas->MainTex->FlippedHeight = false;
    atlas->Width = result_width;
    atlas->Height = result_height;
    atlas->RootNode = _accumulatorActive ? std::make_unique<TextureAtlas::SpaceNode>(0, 0, result_width, result_height) : nullptr;

    _allAtlases.push_back(std::move(atlas));
    return _allAtlases.back().get();
}

auto SpriteManager::FindAtlasPlace(const AtlasSprite* atlas_spr, AtlasType atlas_type, int& x, int& y) -> TextureAtlas*
{
    STACK_TRACE_ENTRY();

    // Find place in already created atlas
    TextureAtlas* atlas = nullptr;

    const auto w = atlas_spr->Width + ATLAS_SPRITES_PADDING * 2;
    const auto h = atlas_spr->Height + ATLAS_SPRITES_PADDING * 2;

    if (atlas_type != AtlasType::OneImage) {
        for (auto& a : _allAtlases) {
            if (a->Type != atlas_type) {
                continue;
            }

            if (a->RootNode) {
                if (a->RootNode->FindPosition(w, h, x, y)) {
                    atlas = a.get();
                }
            }
            else {
                if (w <= a->LineW && a->LineCurH + h <= a->LineMaxH) {
                    x = a->CurX - a->LineW;
                    y = a->CurY + a->LineCurH;
                    a->LineCurH += h;
                    atlas = a.get();
                }
                else if (a->Width - a->CurX >= w && a->Height - a->CurY >= h) {
                    x = a->CurX;
                    y = a->CurY;
                    a->CurX += w;
                    a->LineW = w;
                    a->LineCurH = h;
                    a->LineMaxH = std::max(a->LineMaxH, h);
                    atlas = a.get();
                }
                else if (a->Width >= w && a->Height - a->CurY - a->LineMaxH >= h) {
                    x = 0;
                    y = a->CurY + a->LineMaxH;
                    a->CurX = w;
                    a->CurY = y;
                    a->LineW = w;
                    a->LineCurH = h;
                    a->LineMaxH = h;
                    atlas = a.get();
                }
            }

            if (atlas != nullptr) {
                break;
            }
        }
    }

    // Create new
    if (atlas == nullptr) {
        atlas = CreateAtlas(w, h, atlas_type);

        if (atlas->RootNode) {
            atlas->RootNode->FindPosition(w, h, x, y);
        }
        else {
            x = atlas->CurX;
            y = atlas->CurY;
            atlas->CurX += w;
            atlas->LineMaxH = h;
            atlas->LineCurH = h;
            atlas->LineW = w;
        }
    }

    x += ATLAS_SPRITES_PADDING;
    y += ATLAS_SPRITES_PADDING;

    return atlas;
}

static void WriteSimpleTga(string_view fname, int width, int height, vector<uint> data)
{
    STACK_TRACE_ENTRY();

    auto file = DiskFileSystem::OpenFile(fname, true);
    RUNTIME_ASSERT(file);

    const uint8 header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
        static_cast<uint8>(width % 256), static_cast<uint8>(width / 256), //
        static_cast<uint8>(height % 256), static_cast<uint8>(height / 256), 4 * 8, 0x20};
    file.Write(header);

    for (auto& c : data) {
        c = COLOR_SWAP_RB(c);
    }

    file.Write(data.data(), data.size() * sizeof(uint));
}

void SpriteManager::DumpAtlases() const
{
    STACK_TRACE_ENTRY();

    uint atlases_memory_size = 0;
    for (auto&& atlas : _allAtlases) {
        atlases_memory_size += atlas->Width * atlas->Height * 4;
    }

    const auto date = Timer::GetCurrentDateTime();
    const string dir = _str("{:04}.{:02}.{:02}_{:02}-{:02}-{:02}_{}.{:03}mb", //
        date.Year, date.Month, date.Day, date.Hour, date.Minute, date.Second, //
        atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000);

    const auto write_rt = [&dir](string_view name, const RenderTarget* rt) {
        if (rt != nullptr) {
            const string fname = _str("{}/{}_{}x{}.tga", dir, name, rt->MainTex->Width, rt->MainTex->Height);
            auto tex_data = rt->MainTex->GetTextureRegion(0, 0, rt->MainTex->Width, rt->MainTex->Height);
            WriteSimpleTga(fname, rt->MainTex->Width, rt->MainTex->Height, std::move(tex_data));
        }
    };

    write_rt("Main", _rtMain);
    write_rt("Contours", _rtContours);
    write_rt("ContoursMid", _rtContoursMid);

    auto cnt = 1;
    for (auto&& atlas : _allAtlases) {
        string atlas_type_name;
        switch (atlas->Type) {
        case AtlasType::Static:
            atlas_type_name = "Static";
            break;
        case AtlasType::Dynamic:
            atlas_type_name = "Dynamic";
            break;
        case AtlasType::OneImage:
            atlas_type_name = "OneImage";
            break;
        case AtlasType::MeshTextures:
            atlas_type_name = "MeshTextures";
            break;
        }

        const string fname = _str("{}/{}_{}_{}x{}.tga", dir, atlas_type_name, cnt, atlas->Width, atlas->Height);
        auto tex_data = atlas->MainTex->GetTextureRegion(0, 0, atlas->Width, atlas->Height);
        WriteSimpleTga(fname, atlas->Width, atlas->Height, std::move(tex_data));
        cnt++;
    }

#if FO_ENABLE_3D
    cnt = 1;
    for (const auto* rt : _rtIntermediate) {
        write_rt(_str("Model{}", cnt), rt);
        cnt++;
    }
#endif

    cnt = 1;
    for (auto&& rt : _rtAll) {
        write_rt(_str("All{}", cnt), rt.get());
        cnt++;
    }
}

void SpriteManager::RequestFillAtlas(AtlasSprite* atlas_spr, AtlasType atlas_type, int width, int height, const uint* data)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(width > 0);
    RUNTIME_ASSERT(height > 0);

    atlas_spr->Width = width;
    atlas_spr->Height = height;

    if (_accumulatorActive) {
        auto* acc_data = new uint[static_cast<size_t>(width * height)];
        std::memcpy(acc_data, data, static_cast<size_t>(width * height) * sizeof(uint));
        _spritesAccumulator.emplace_back(atlas_spr, acc_data, atlas_type);
    }
    else {
        FillAtlas(atlas_spr, data, atlas_type);
    }
}

void SpriteManager::FillAtlas(AtlasSprite* atlas_spr, const uint* data, AtlasType atlas_type)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(atlas_spr);
    RUNTIME_ASSERT(atlas_spr->Width > 0);
    RUNTIME_ASSERT(atlas_spr->Height > 0);

    const auto w = atlas_spr->Width;
    const auto h = atlas_spr->Height;

    int x = 0;
    int y = 0;
    auto* atlas = FindAtlasPlace(atlas_spr, atlas_type, x, y);

    // Refresh texture
    if (data != nullptr) {
        auto* tex = atlas->MainTex;
        tex->UpdateTextureRegion(IRect(x, y, x + w, y + h), data);

        // 1px border for correct linear interpolation
        // Top
        tex->UpdateTextureRegion(IRect(x, y - 1, x + w, y), data);

        // Bottom
        tex->UpdateTextureRegion(IRect(x, y + h, x + w, y + h + 1), data + static_cast<size_t>((h - 1) * w));

        // Left
        for (auto i = 0; i < h; i++) {
            _borderBuf[i + 1] = *(data + static_cast<size_t>(i * w));
        }
        _borderBuf[0] = _borderBuf[1];
        _borderBuf[h + 1] = _borderBuf[h];
        tex->UpdateTextureRegion(IRect(x - 1, y - 1, x, y + h + 1), _borderBuf.data());

        // Right
        for (auto i = 0; i < h; i++) {
            _borderBuf[i + 1] = *(data + static_cast<size_t>(i * w + (w - 1)));
        }
        _borderBuf[0] = _borderBuf[1];
        _borderBuf[h + 1] = _borderBuf[h];
        tex->UpdateTextureRegion(IRect(x + w, y - 1, x + w + 1, y + h + 1), _borderBuf.data());

        // Evaluate hit mask
        atlas_spr->HitTestData.resize(static_cast<size_t>(w * h));
        for (size_t i = 0, j = static_cast<size_t>(w * h); i < j; i++) {
            atlas_spr->HitTestData[i] = (data[i] >> 24) > 0;
        }
    }

    // Invalidate last pixel color picking
    if (!atlas->RTarg->LastPixelPicks.empty()) {
        atlas->RTarg->LastPixelPicks.clear();
    }

    // Set parameters
    atlas_spr->Atlas = atlas;
    atlas_spr->AtlasRect.Left = static_cast<float>(x) / static_cast<float>(atlas->Width);
    atlas_spr->AtlasRect.Top = static_cast<float>(y) / static_cast<float>(atlas->Height);
    atlas_spr->AtlasRect.Right = static_cast<float>(x + w) / static_cast<float>(atlas->Width);
    atlas_spr->AtlasRect.Bottom = static_cast<float>(y + h) / static_cast<float>(atlas->Height);
}

auto SpriteManager::LoadAnimation(string_view fname, AtlasType atlas_type, bool use_dummy_if_not_exists) -> AnyFrames*
{
    STACK_TRACE_ENTRY();

    auto* dummy = use_dummy_if_not_exists ? _dummyAnim : nullptr;

    if (fname.empty()) {
        return dummy;
    }

    const string ext = _str(fname).getFileExtension();
    if (ext.empty()) {
        WriteLog("Extension not found, file '{}'", fname);
        return dummy;
    }

    AnyFrames* result;
    if (ext == "fo3d" || ext == "fbx" || ext == "dae" || ext == "obj") {
#if FO_ENABLE_3D
        result = Load3dAnimation(fname, atlas_type);
#else
        throw NotEnabled3DException("Can't load animation, 3D submodule not enabled", fname);
#endif
    }
    else {
        result = Load2dAnimation(fname, atlas_type);
    }

    return result != nullptr ? result : dummy;
}

auto SpriteManager::Load2dAnimation(string_view fname, AtlasType atlas_type) -> AnyFrames*
{
    STACK_TRACE_ENTRY();

    auto file = _resources.ReadFile(fname);
    if (!file) {
        return nullptr;
    }

    const auto check_number = file.GetUChar();
    RUNTIME_ASSERT(check_number == 42);
    const auto frames_count = file.GetLEUShort();
    const auto ticks = file.GetLEUShort();
    const auto dirs = file.GetUChar();

    auto* anim = CreateAnyFrames(frames_count, ticks);
    if (dirs > 1) {
        CreateAnyFramesDirAnims(anim, dirs);
    }

    for (uint16 dir = 0; dir < dirs; dir++) {
        auto* dir_anim = anim->GetDir(dir);
        const auto ox = file.GetLEShort();
        const auto oy = file.GetLEShort();
        for (uint16 i = 0; i < frames_count; i++) {
            if (file.GetUChar() == 0) {
                auto* spr = new AtlasSprite();
                spr->OffsX = ox;
                spr->OffsY = oy;
                const auto w = file.GetLEUShort();
                const auto h = file.GetLEUShort();
                dir_anim->NextX[i] = file.GetLEShort();
                dir_anim->NextY[i] = file.GetLEShort();
                const auto* data = file.GetCurBuf();
                RequestFillAtlas(spr, atlas_type, w, h, reinterpret_cast<const uint*>(data));
                file.GoForward(w * h * 4);
                dir_anim->Spr[i] = spr;
            }
            else {
                const auto index = file.GetLEUShort();
                dir_anim->Spr[i] = dir_anim->Spr[index];
            }
        }
    }

    const auto check_number2 = file.GetUChar();
    RUNTIME_ASSERT(check_number2 == 42);
    return anim;
}

auto SpriteManager::LoadTexture(string_view path, unordered_map<string, const AtlasSprite*>& collection, AtlasType atlas_type) -> pair<RenderTexture*, FRect>
{
    STACK_TRACE_ENTRY();

    auto result = pair<RenderTexture*, FRect>();

    if (const auto it = collection.find(string(path)); it == collection.end()) {
        auto* anim = LoadAnimation(path, atlas_type, false);

        if (anim != nullptr) {
            const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(anim->Spr[0]);
            RUNTIME_ASSERT(atlas_spr);
            collection[string(path)] = atlas_spr;
            result = pair {atlas_spr->Atlas->MainTex, FRect {atlas_spr->AtlasRect[0], atlas_spr->AtlasRect[1], atlas_spr->AtlasRect[2] - atlas_spr->AtlasRect[0], atlas_spr->AtlasRect[3] - atlas_spr->AtlasRect[1]}};
            DestroyAnyFrames(anim);
        }
        else {
            BreakIntoDebugger();
            WriteLog("Texture '{}' not found", path);
            collection[string(path)] = nullptr;
        }
    }
    else if (const auto* atlas_spr = it->second; atlas_spr != nullptr) {
        result = pair {atlas_spr->Atlas->MainTex, FRect {atlas_spr->AtlasRect[0], atlas_spr->AtlasRect[1], atlas_spr->AtlasRect[2] - atlas_spr->AtlasRect[0], atlas_spr->AtlasRect[3] - atlas_spr->AtlasRect[1]}};
    }

    return result;
}

void SpriteManager::InitParticleSubsystem(GameTimer& game_time)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_particleMngr);

    _particleMngr = std::make_unique<ParticleManager>(_settings, _effectMngr, _resources, game_time, //
        [this](string_view path) { return LoadTexture(path, _loadedParticleTextures, AtlasType::Static); });
}

auto SpriteManager::LoadParticle(string_view name, AtlasType atlas_type) -> unique_del_ptr<ParticleSprite>
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_particleMngr);

    auto&& particle = _particleMngr->CreateParticle(name);
    if (particle == nullptr) {
        return nullptr;
    }

    auto&& [draw_width, draw_height] = particle->GetDrawSize();
    const auto frame_ratio = static_cast<float>(draw_width) / static_cast<float>(draw_height);
    const auto proj_height = static_cast<float>(draw_height) * (1.0f / _settings.ModelProjFactor);
    const auto proj_width = proj_height * frame_ratio;
    const mat44 proj = App->Render.CreateOrthoMatrix(0.0f, proj_width, 0.0f, proj_height, -10.0f, 10.0f);
    mat44 world;
    mat44::Translation({proj_width / 2.0f, proj_height / 4.0f, 0.0f}, world);

    particle->Setup(proj, world, {}, {}, {});

    auto* particle_spr = new ParticleSprite();
    particle_spr->Particle = std::move(particle);
    particle_spr->OffsY = static_cast<int16>(draw_height / 4);

    RequestFillAtlas(particle_spr, atlas_type, draw_width, draw_height, nullptr);

    _autoDrawParticles.push_back(particle_spr);

    return {particle_spr, [this](auto* ptr) {
                const auto it = std::find(_autoDrawParticles.begin(), _autoDrawParticles.end(), ptr);
                RUNTIME_ASSERT(it != _autoDrawParticles.end());
                _autoDrawParticles.erase(it);

                delete ptr;
            }};
}

void SpriteManager::DrawParticleToAtlas(ParticleSprite* particle_spr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_particleMngr);

    // Find place for render
    const auto frame_width = particle_spr->Width;
    const auto frame_height = particle_spr->Height;

    RenderTarget* rt_intermediate = nullptr;
    for (auto* rt : _rtIntermediate) {
        if (rt->MainTex->Width == frame_width && rt->MainTex->Height == frame_height) {
            rt_intermediate = rt;
            break;
        }
    }
    if (rt_intermediate == nullptr) {
        rt_intermediate = CreateRenderTarget(true, RenderTarget::SizeType::Custom, frame_width, frame_height, true);
        _rtIntermediate.push_back(rt_intermediate);
    }

    PushRenderTarget(rt_intermediate);
    ClearCurrentRenderTarget(0, true);

    // Draw particles
    particle_spr->Particle->Draw();

    // Restore render target
    PopRenderTarget();

    // Copy render
    IRect region_to;

    // Render to atlas
    if (rt_intermediate->MainTex->FlippedHeight) {
        // Preserve flip
        const auto l = iround(particle_spr->AtlasRect.Left * static_cast<float>(particle_spr->Atlas->Width));
        const auto t = iround((1.0f - particle_spr->AtlasRect.Top) * static_cast<float>(particle_spr->Atlas->Height));
        const auto r = iround(particle_spr->AtlasRect.Right * static_cast<float>(particle_spr->Atlas->Width));
        const auto b = iround((1.0f - particle_spr->AtlasRect.Bottom) * static_cast<float>(particle_spr->Atlas->Height));
        region_to = IRect(l, t, r, b);
    }
    else {
        const auto l = iround(particle_spr->AtlasRect.Left * static_cast<float>(particle_spr->Atlas->Width));
        const auto t = iround(particle_spr->AtlasRect.Top * static_cast<float>(particle_spr->Atlas->Height));
        const auto r = iround(particle_spr->AtlasRect.Right * static_cast<float>(particle_spr->Atlas->Width));
        const auto b = iround(particle_spr->AtlasRect.Bottom * static_cast<float>(particle_spr->Atlas->Height));
        region_to = IRect(l, t, r, b);
    }

    PushRenderTarget(particle_spr->Atlas->RTarg);
    DrawRenderTarget(rt_intermediate, false, nullptr, &region_to);
    PopRenderTarget();
}

#if FO_ENABLE_3D
void SpriteManager::Init3dSubsystem(GameTimer& game_time, NameResolver& name_resolver, AnimationResolver& anim_name_resolver)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_modelMngr);

    _modelMngr = std::make_unique<ModelManager>(_settings, _resources, _effectMngr, game_time, name_resolver, anim_name_resolver, //
        [this](string_view path) { return LoadTexture(path, _loadedMeshTextures, AtlasType::MeshTextures); });
}

void SpriteManager::Preload3dModel(string_view model_name)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(_modelMngr);

    _modelMngr->PreloadModel(model_name);
}

auto SpriteManager::Load3dAnimation(string_view fname, AtlasType atlas_type) -> AnyFrames*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_modelMngr);

    // Load 3d animation
    auto&& model_spr = LoadModel(fname, atlas_type);
    if (!model_spr) {
        return nullptr;
    }

    auto&& model = model_spr->Model;

    model->PrewarmParticles();
    model->StartMeshGeneration();

    // Get animation data
    const auto [period, proc_from, proc_to, dir] = model->GetRenderFramesData();

    // Set fir
    if (dir < 0) {
        model->SetLookDirAngle(-dir);
        model->SetMoveDirAngle(-dir, false);
    }
    else {
        model->SetDir(static_cast<uint8>(dir), false);
    }

    // Calculate needed information
    const auto frame_time = 1.0f / static_cast<float>(_settings.Animation3dFPS != 0 ? _settings.Animation3dFPS : 10); // 1 second / fps
    const auto period_from = period * static_cast<float>(proc_from) / 100.0f;
    const auto period_to = period * static_cast<float>(proc_to) / 100.0f;
    const auto period_len = fabs(period_to - period_from);
    const auto proc_step = static_cast<float>(proc_to - proc_from) / (period_len / frame_time);
    const auto frames_count = iround(std::ceil(period_len / frame_time));

    // If no animations available than render just one
    if (period == 0.0f || proc_from == proc_to || frames_count <= 1) {
        model->SetAnimation(0, proc_from * 10, nullptr, ANIMATION_ONE_TIME | ANIMATION_STAY);
        DrawModelToAtlas(model_spr.get());

        auto* anim = CreateAnyFrames(1, 100);
        anim->Spr[0] = model_spr.get();
        return anim;
    }

    auto* anim = CreateAnyFrames(frames_count, static_cast<uint>(period_len * 1000.0f));
    auto cur_proc = static_cast<float>(proc_from);
    auto prev_cur_proci = -1;

    for (auto i = 0; i < frames_count; i++) {
        const auto cur_proci = proc_to > proc_from ? iround(10.0f * cur_proc + 0.5f) : iround(10.0f * cur_proc);

        // Previous frame is different
        if (cur_proci != prev_cur_proci) {
            model->SetAnimation(0, cur_proci, nullptr, ANIMATION_ONE_TIME | ANIMATION_STAY);
            DrawModelToAtlas(model_spr.get());

            anim->Spr[i] = model_spr.get();
        }
        // Previous frame is same
        else if (i > 0) {
            anim->Spr[i] = anim->Spr[i - 1];
        }

        cur_proc += proc_step;
        prev_cur_proci = cur_proci;
    }

    return anim;
}

void SpriteManager::DrawModel(int x, int y, ModelSprite* model_spr, uint color)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_modelMngr);

    auto&& model = model_spr->Model;

    model->PrewarmParticles();
    model->StartMeshGeneration();

    DrawModelToAtlas(model_spr);

    DrawSprite(model_spr, x - model_spr->Width / 2 + model_spr->OffsX, y - model_spr->Height + model_spr->OffsY, color);
}

auto SpriteManager::LoadModel(string_view fname, AtlasType atlas_type) -> unique_del_ptr<ModelSprite>
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_modelMngr);

    auto&& model = _modelMngr->CreateModel(fname);
    if (model == nullptr) {
        return nullptr;
    }

    model->SetupFrame();
    auto&& [draw_width, draw_height] = model->GetDrawSize();

    auto* model_spr = new ModelSprite();
    model_spr->Model = std::move(model);
    model_spr->OffsY = static_cast<int16>(draw_height / 4);

    RequestFillAtlas(model_spr, atlas_type, draw_width, draw_height, nullptr);

    _autoDrawModels.push_back(model_spr);

    return {model_spr, [this](auto* ptr) {
                const auto it = std::find(_autoDrawModels.begin(), _autoDrawModels.end(), ptr);
                RUNTIME_ASSERT(it != _autoDrawModels.end());
                _autoDrawModels.erase(it);

                delete ptr;
            }};
}

void SpriteManager::DrawModelToAtlas(ModelSprite* model_spr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_modelMngr);

    // Find place for render
    const auto frame_width = model_spr->Width * ModelInstance::FRAME_SCALE;
    const auto frame_height = model_spr->Height * ModelInstance::FRAME_SCALE;

    RenderTarget* rt_model = nullptr;
    for (auto* rt : _rtIntermediate) {
        if (rt->MainTex->Width == frame_width && rt->MainTex->Height == frame_height) {
            rt_model = rt;
            break;
        }
    }
    if (rt_model == nullptr) {
        rt_model = CreateRenderTarget(true, RenderTarget::SizeType::Custom, frame_width, frame_height, true);
        _rtIntermediate.push_back(rt_model);
    }

    PushRenderTarget(rt_model);
    ClearCurrentRenderTarget(0, true);

    // Draw model
    model_spr->Model->Draw();

    // Restore render target
    PopRenderTarget();

    // Copy render
    IRect region_to;

    // Render to atlas
    if (rt_model->MainTex->FlippedHeight) {
        // Preserve flip
        const auto l = iround(model_spr->AtlasRect.Left * static_cast<float>(model_spr->Atlas->Width));
        const auto t = iround((1.0f - model_spr->AtlasRect.Top) * static_cast<float>(model_spr->Atlas->Height));
        const auto r = iround(model_spr->AtlasRect.Right * static_cast<float>(model_spr->Atlas->Width));
        const auto b = iround((1.0f - model_spr->AtlasRect.Bottom) * static_cast<float>(model_spr->Atlas->Height));
        region_to = IRect(l, t, r, b);
    }
    else {
        const auto l = iround(model_spr->AtlasRect.Left * static_cast<float>(model_spr->Atlas->Width));
        const auto t = iround(model_spr->AtlasRect.Top * static_cast<float>(model_spr->Atlas->Height));
        const auto r = iround(model_spr->AtlasRect.Right * static_cast<float>(model_spr->Atlas->Width));
        const auto b = iround(model_spr->AtlasRect.Bottom * static_cast<float>(model_spr->Atlas->Height));
        region_to = IRect(l, t, r, b);
    }

    PushRenderTarget(model_spr->Atlas->RTarg);
    DrawRenderTarget(rt_model, false, nullptr, &region_to);
    PopRenderTarget();
}
#endif

auto SpriteManager::CreateAnyFrames(uint frames, uint ticks) -> AnyFrames*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(frames < MAX_FRAMES);
    auto* anim = _anyFramesPool.Get();
    *anim = {}; // Reset state
    anim->CntFrm = frames;
    anim->Ticks = ticks != 0 ? ticks : frames * 100;
    return anim;
}

void SpriteManager::CreateAnyFramesDirAnims(AnyFrames* anim, uint dirs)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(dirs > 1);
    RUNTIME_ASSERT(dirs == 6 || dirs == 8);
    anim->DirCount = dirs;
    for (uint dir = 0; dir < dirs - 1; dir++) {
        anim->Dirs[dir] = CreateAnyFrames(anim->CntFrm, anim->Ticks);
    }
}

void SpriteManager::DestroyAnyFrames(AnyFrames* anim)
{
    STACK_TRACE_ENTRY();

    if (anim == nullptr || anim == _dummyAnim) {
        return;
    }

    for (uint dir = 1; dir < anim->DirCount; dir++) {
        _anyFramesPool.Put(anim->GetDir(dir));
    }
    _anyFramesPool.Put(anim);
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

        if (_sprEgg != nullptr) {
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

    if (_dipQueue.empty() || _dipQueue.back().MainTex != spr->Atlas->MainTex || _dipQueue.back().SourceEffect != effect) {
        _dipQueue.emplace_back(DipData {spr->Atlas->MainTex, effect, ind_count});
    }
    else {
        _dipQueue.back().IndCount += ind_count;
    }

    if (_spritesDrawBuf->VertCount >= _flushVertCount) {
        Flush();
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
            wf = floorf(wf * k + 0.5f);
            hf = floorf(hf * k + 0.5f);
        }
        if (center) {
            xf += floorf((static_cast<float>(w) - wf) / 2.0f + 0.5f);
            yf += floorf((static_cast<float>(h) - hf) / 2.0f + 0.5f);
        }
    }
    else if (zoom_up) {
        wf = floorf(wf * k + 0.5f);
        hf = floorf(hf * k + 0.5f);
        if (center) {
            xf += floorf((static_cast<float>(w) - wf) / 2.0f + 0.5f);
            yf += floorf((static_cast<float>(h) - hf) / 2.0f + 0.5f);
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

    if (_dipQueue.empty() || _dipQueue.back().MainTex != spr->Atlas->MainTex || _dipQueue.back().SourceEffect != effect) {
        _dipQueue.emplace_back(DipData {spr->Atlas->MainTex, effect, ind_count});
    }
    else {
        _dipQueue.back().IndCount += ind_count;
    }

    if (_spritesDrawBuf->VertCount >= _flushVertCount) {
        Flush();
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
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx;
            vbuf[vpos].PosY = yy;
            vbuf[vpos].TexU = atlas_spr->AtlasRect.Left;
            vbuf[vpos].TexV = atlas_spr->AtlasRect.Top;
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx + local_width;
            vbuf[vpos].PosY = yy;
            vbuf[vpos].TexU = local_right;
            vbuf[vpos].TexV = atlas_spr->AtlasRect.Top;
            vbuf[vpos++].Color = color;

            vbuf[vpos].PosX = xx + local_width;
            vbuf[vpos].PosY = yy + local_height;
            vbuf[vpos].TexU = local_right;
            vbuf[vpos].TexV = local_bottom;
            vbuf[vpos++].Color = color;

            if (_dipQueue.empty() || _dipQueue.back().MainTex != atlas_spr->Atlas->MainTex || _dipQueue.back().SourceEffect != effect) {
                _dipQueue.emplace_back(DipData {atlas_spr->Atlas->MainTex, effect, 6});
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

auto SpriteManager::GetDrawRect(const MapSprite* mspr) const -> IRect
{
    STACK_TRACE_ENTRY();

    const auto* spr = mspr->PSpr != nullptr ? *mspr->PSpr : mspr->Spr;
    RUNTIME_ASSERT(spr);

    auto x = mspr->ScrX - spr->Width / 2 + spr->OffsX + *mspr->PScrX;
    auto y = mspr->ScrY - spr->Height + spr->OffsY + *mspr->PScrY;
    if (mspr->OffsX != nullptr) {
        x += *mspr->OffsX;
    }
    if (mspr->OffsY != nullptr) {
        y += *mspr->OffsY;
    }

    return {x, y, x + spr->Width, y + spr->Height};
}

auto SpriteManager::GetViewRect(const MapSprite* mspr) const -> IRect
{
    STACK_TRACE_ENTRY();

    auto rect = GetDrawRect(mspr);

    const auto* spr = mspr->PSpr != nullptr ? *mspr->PSpr : mspr->Spr;
    RUNTIME_ASSERT(spr);

#if FO_ENABLE_3D
    if (const auto* model_spr = dynamic_cast<const ModelSprite*>(spr); model_spr != nullptr) {
        auto&& [view_width, view_height] = model_spr->Model->GetViewSize();

        rect.Left = rect.CenterX() - view_width / 2;
        rect.Right = rect.Left + view_width;
        rect.Bottom = rect.Bottom - rect.Height() / 4 + _settings.MapHexHeight / 2;
        rect.Top = rect.Bottom - view_height;
    }
#endif

    return rect;
}

void SpriteManager::InitializeEgg(string_view egg_name, AtlasType atlas_type)
{
    STACK_TRACE_ENTRY();

    _eggValid = false;
    _eggHx = _eggHy = 0;
    _eggX = _eggY = 0;

    auto* egg_frames = LoadAnimation(egg_name, atlas_type, true);
    RUNTIME_ASSERT(egg_frames != nullptr);

    _sprEgg = dynamic_cast<AtlasSprite*>(egg_frames->Spr[0]);
    RUNTIME_ASSERT(_sprEgg);

    DestroyAnyFrames(egg_frames);

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

        const auto prev_vpos = _spritesDrawBuf->VertCount;

        const auto ind_count = spr->FillData(_spritesDrawBuf, {xf, yf, xf + wf, yf + hf}, {color_l, color_r});

        // Setup egg
        bool egg_added = false;

        if (use_egg && CheckEggAppearence(mspr->HexX, mspr->HexY, mspr->EggAppearence)) {
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

                    vbuf[prev_vpos + 0].EggTexU = x1_f;
                    vbuf[prev_vpos + 0].EggTexV = y2_f;
                    vbuf[prev_vpos + 1].EggTexU = x1_f;
                    vbuf[prev_vpos + 1].EggTexV = y1_f;
                    vbuf[prev_vpos + 2].EggTexU = x2_f;
                    vbuf[prev_vpos + 2].EggTexV = y1_f;
                    vbuf[prev_vpos + 3].EggTexU = x2_f;
                    vbuf[prev_vpos + 3].EggTexV = y2_f;

                    egg_added = true;
                }
            }
        }

        if (!egg_added) {
            auto& vbuf = _spritesDrawBuf->Vertices;

            for (size_t i = prev_vpos; i < _spritesDrawBuf->VertCount; i++) {
                vbuf[i].EggTexU = 0.0f;
            }
        }

        if (_dipQueue.empty() || _dipQueue.back().MainTex != spr->Atlas->MainTex || _dipQueue.back().SourceEffect != effect) {
            _dipQueue.emplace_back(DipData {spr->Atlas->MainTex, effect, ind_count});
        }
        else {
            _dipQueue.back().IndCount += ind_count;
        }

        if (_spritesDrawBuf->VertCount >= _flushVertCount) {
            Flush();
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
            auto rect = GetViewRect(mspr);

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

auto SpriteManager::IsPixNoTransp(const Sprite* spr, int offs_x, int offs_y, bool with_zoom) const -> bool
{
    STACK_TRACE_ENTRY();

    auto ox = offs_x;
    auto oy = offs_y;

    if (ox < 0 || oy < 0) {
        return false;
    }

    if (with_zoom && _spritesZoom != 1.0f) {
        ox = iround(static_cast<float>(ox) * _spritesZoom);
        oy = iround(static_cast<float>(oy) * _spritesZoom);
    }

    if (ox >= spr->Width || oy >= spr->Height) {
        return false;
    }

    if (const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(spr); atlas_spr != nullptr) {
        if (!atlas_spr->HitTestData.empty()) {
            return atlas_spr->HitTestData[oy * atlas_spr->Width + ox];
        }
        else {
            ox += iround(atlas_spr->Atlas->MainTex->SizeData[0] * atlas_spr->AtlasRect.Left);
            oy += iround(atlas_spr->Atlas->MainTex->SizeData[1] * atlas_spr->AtlasRect.Top);

            return (GetRenderTargetPixel(atlas_spr->Atlas->RTarg, ox, oy) >> 24) > 0;
        }
    }
    else {
        return false;
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
        PushRenderTarget(_rtContours);
        ClearCurrentRenderTarget(0);
        PopRenderTarget();

        if (_contourClearMid) {
            _contourClearMid = false;

            PushRenderTarget(_rtContoursMid);
            ClearCurrentRenderTarget(0);
            PopRenderTarget();
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

#if FO_ENABLE_3D
    const auto is_model_sprite = dynamic_cast<const ModelSprite*>(spr) != nullptr;
    auto* contour_effect = is_model_sprite ? _effectMngr.Effects.ContourModelSprite : _effectMngr.Effects.ContourSprite;
#else
    auto* contour_effect = _effectMngr.Effects.ContourSprite;
#endif
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
#if FO_ENABLE_3D
        if (is_model_sprite) {
            const auto& sr = as->AtlasRect;
            textureuv = sr;
            sprite_border = sr;
            borders.Left++;
            borders.Top++;
            borders.Right--;
            borders.Bottom--;
        }
        else
#endif
        {
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

        PushRenderTarget(_rtContoursMid);
        _contourClearMid = true;

        auto& vbuf = _flushDrawBuf->Vertices;
        auto pos = 0;

        vbuf[pos].PosX = bordersf.Left;
        vbuf[pos].PosY = flipped_height ? mid_height - bordersf.Bottom : bordersf.Bottom;
        vbuf[pos].TexU = sr.Left;
        vbuf[pos++].TexV = sr.Bottom;

        vbuf[pos].PosX = bordersf.Left;
        vbuf[pos].PosY = flipped_height ? mid_height - bordersf.Top : bordersf.Top;
        vbuf[pos].TexU = sr.Left;
        vbuf[pos++].TexV = sr.Top;

        vbuf[pos].PosX = bordersf.Right;
        vbuf[pos].PosY = flipped_height ? mid_height - bordersf.Top : bordersf.Top;
        vbuf[pos].TexU = sr.Right;
        vbuf[pos++].TexV = sr.Top;

        vbuf[pos].PosX = bordersf.Right;
        vbuf[pos].PosY = flipped_height ? mid_height - bordersf.Bottom : bordersf.Bottom;
        vbuf[pos].TexU = sr.Right;
        vbuf[pos].TexV = sr.Bottom;

        _flushDrawBuf->Upload(_effectMngr.Effects.FlushRenderTarget->Usage);
        _effectMngr.Effects.FlushRenderTarget->DrawBuffer(_flushDrawBuf);

        PopRenderTarget();

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

    PushRenderTarget(_rtContours);

    auto& vbuf = _contourDrawBuf->Vertices;
    auto pos = 0;

    vbuf[pos].PosX = bordersf.Left;
    vbuf[pos].PosY = bordersf.Bottom;
    vbuf[pos].TexU = textureuv.Left;
    vbuf[pos].TexV = textureuv.Bottom;
    vbuf[pos++].Color = contour_color;

    vbuf[pos].PosX = bordersf.Left;
    vbuf[pos].PosY = bordersf.Top;
    vbuf[pos].TexU = textureuv.Left;
    vbuf[pos].TexV = textureuv.Top;
    vbuf[pos++].Color = contour_color;

    vbuf[pos].PosX = bordersf.Right;
    vbuf[pos].PosY = bordersf.Top;
    vbuf[pos].TexU = textureuv.Right;
    vbuf[pos].TexV = textureuv.Top;
    vbuf[pos++].Color = contour_color;

    vbuf[pos].PosX = bordersf.Right;
    vbuf[pos].PosY = bordersf.Bottom;
    vbuf[pos].TexU = textureuv.Right;
    vbuf[pos].TexV = textureuv.Bottom;
    vbuf[pos].Color = contour_color;

    auto&& contour_buf = contour_effect->ContourBuf = RenderEffect::ContourBuffer();

    contour_buf->SpriteBorder[0] = sprite_border[0];
    contour_buf->SpriteBorder[1] = sprite_border[1];
    contour_buf->SpriteBorder[2] = sprite_border[2];
    contour_buf->SpriteBorder[3] = sprite_border[3];

    _contourDrawBuf->Upload(contour_effect->Usage);
    contour_effect->DrawBuffer(_contourDrawBuf, 0, static_cast<size_t>(-1), texture);

    PopRenderTarget();
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

void SpriteManager::BuildFonts()
{
    STACK_TRACE_ENTRY();

    for (size_t i = 0; i < _allFonts.size(); i++) {
        if (_allFonts[i] && !_allFonts[i]->Builded) {
            BuildFont(static_cast<int>(i));
        }
    }
}

void SpriteManager::BuildFont(int index)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

#define PIXEL_AT(tex_data, width, x, y) (*((uint*)(tex_data).data() + (y) * (width) + (x)))

    auto& font = *_allFonts[index];
    font.Builded = true;

    // Fix texture coordinates
    const auto* atlas_spr = dynamic_cast<const AtlasSprite*>(font.ImageNormal->GetSpr(0));
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
        letter.TexUV[0] = (image_x + x - 1.0f) / tex_w;
        letter.TexUV[1] = (image_y + y - 1.0f) / tex_h;
        letter.TexUV[2] = (image_x + x + w + 1.0f) / tex_w;
        letter.TexUV[3] = (image_y + y + h + 1.0f) / tex_h;
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

    const auto* si_bordered = dynamic_cast<const AtlasSprite*>(font.ImageBordered != nullptr ? font.ImageBordered->GetSpr(0) : nullptr);
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
            letter.TexBorderedUV[0] = (image_x + x - 1.0f) / tex_w;
            letter.TexBorderedUV[1] = (image_y + y - 1.0f) / tex_h;
            letter.TexBorderedUV[2] = (image_x + x + w + 1.0f) / tex_w;
            letter.TexBorderedUV[3] = (image_y + y + h + 1.0f) / tex_h;
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

    FontData font {_effectMngr.Effects.Font};
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
            str >> font.LineHeight;
        }
        else if (key == "YAdvance") {
            str >> font.YAdvance;
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

            cur_letter = &font.Letters[letter];
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
    font.MakeGray = make_gray;

    // Load image
    image_name.insert(0, "Fonts/");
    auto* image_normal = LoadAnimation(image_name, atlas_type, false);
    if (image_normal == nullptr) {
        WriteLog("Image file '{}' not found", image_name);
        return false;
    }
    font.ImageNormal = image_normal;

    // Create bordered instance
    if (!not_bordered) {
        auto* image_bordered = LoadAnimation(image_name, atlas_type, false);
        if (image_bordered == nullptr) {
            WriteLog("Can't load twice file '{}'", image_name);
            return false;
        }
        font.ImageBordered = image_bordered;
    }

    // Register
    if (index >= static_cast<int>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }
    _allFonts[index] = std::make_unique<FontData>(font);

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

    FontData font {_effectMngr.Effects.Font};
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
        auto& let = font.Letters[id];
        let.PosX = static_cast<int16>(x + 1);
        let.PosY = static_cast<int16>(y + 1);
        let.Width = static_cast<int16>(w - 2);
        let.Height = static_cast<int16>(h - 2);
        let.OffsX = static_cast<int16>(-static_cast<int>(ox));
        let.OffsY = static_cast<int16>(-static_cast<int>(oy) + (line_height - base_height));
        let.XAdvance = static_cast<int16>(xa + 1);
    }

    font.LineHeight = font.Letters.count('W') != 0 ? font.Letters['W'].Height : base_height;
    font.YAdvance = font.LineHeight / 2;
    font.MakeGray = true;

    // Load image
    auto* image_normal = LoadAnimation(image_name, atlas_type, false);
    if (image_normal == nullptr) {
        WriteLog("Image file '{}' not found", image_name);
        return false;
    }
    font.ImageNormal = image_normal;

    // Create bordered instance
    auto* image_bordered = LoadAnimation(image_name, atlas_type, false);
    if (image_bordered == nullptr) {
        WriteLog("Can't load twice file '{}'", image_name);
        return false;
    }
    font.ImageBordered = image_bordered;

    // Register
    if (index >= static_cast<int>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }
    _allFonts[index] = std::make_unique<FontData>(font);

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
                letter = static_cast<uint>(str[i]);
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

            auto& texture_uv = IsBitSet(flags, FT_BORDERED) ? l.TexBorderedUV : l.TexUV;
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
            v0.Color = color;

            auto& v1 = vbuf[vpos++];
            v1.PosX = static_cast<float>(x);
            v1.PosY = static_cast<float>(y);
            v1.TexU = x1;
            v1.TexV = y1;
            v1.Color = color;

            auto& v2 = vbuf[vpos++];
            v2.PosX = static_cast<float>(x + w);
            v2.PosY = static_cast<float>(y);
            v2.TexU = x2;
            v2.TexV = y1;
            v2.Color = color;

            auto& v3 = vbuf[vpos++];
            v3.PosX = static_cast<float>(x + w);
            v3.PosY = static_cast<float>(y + h);
            v3.TexU = x2;
            v3.TexV = y2;
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
