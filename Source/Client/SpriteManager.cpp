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
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"

constexpr int SPRITES_BUFFER_SIZE = 10000;
constexpr int ATLAS_SPRITES_PADDING = 1;

TextureAtlas::SpaceNode::SpaceNode(int x, int y, uint w, uint h) : PosX {x}, PosY {y}, Width {w}, Height {h}
{
}

auto TextureAtlas::SpaceNode::FindPosition(uint w, uint h, int& x, int& y) -> bool
{
    auto result = false;

    if (Child1) {
        result = Child1->FindPosition(w, h, x, y);
    }
    if (!result && Child2) {
        result = Child2->FindPosition(w, h, x, y);
    }

    if (!result && !Busy && Width >= w && Height >= h) {
        result = true;
        Busy = true;

        x = PosX;
        y = PosY;

        if (Width == w && Height > h) {
            Child1 = std::make_unique<SpaceNode>(PosX, PosY + h, Width, Height - h);
        }
        else if (Height == h && Width > w) {
            Child1 = std::make_unique<SpaceNode>(PosX + w, PosY, Width - w, Height);
        }
        else if (Width > w && Height > h) {
            Child1 = std::make_unique<SpaceNode>(PosX + w, PosY, Width - w, h);
            Child2 = std::make_unique<SpaceNode>(PosX, PosY + h, Width, Height - h);
        }
    }
    return result;
}

auto AnyFrames::GetSprId(uint num_frm) const -> uint
{
    return Ind[num_frm % CntFrm];
}

auto AnyFrames::GetNextX(uint num_frm) const -> short
{
    return NextX[num_frm % CntFrm];
}

auto AnyFrames::GetNextY(uint num_frm) const -> short
{
    return NextY[num_frm % CntFrm];
}

auto AnyFrames::GetCurSprId(uint tick) const -> uint
{
    return CntFrm > 1 ? Ind[tick % Ticks * 100 / Ticks * CntFrm / 100] : Ind[0];
}

auto AnyFrames::GetCurSprIndex(uint tick) const -> uint
{
    return CntFrm > 1 ? tick % Ticks * 100 / Ticks * CntFrm / 100 : 0;
}

auto AnyFrames::GetDir(int dir) -> AnyFrames*
{
    return dir == 0 || DirCount == 1 ? this : Dirs[dir - 1];
}

SpriteManager::SpriteManager(RenderSettings& settings, AppWindow* window, FileSystem& file_sys, EffectManager& effect_mngr) : _settings {settings}, _window {window}, _fileSys {file_sys}, _effectMngr {effect_mngr}
{
    _baseColor = COLOR_RGBA(255, 128, 128, 128);
    _drawQuadCount = 1024;
    _allAtlases.reserve(100);
    _dipQueue.reserve(1000);

    _spritesDrawBuf = App->Render.CreateDrawBuffer(false);
    _spritesDrawBuf->Vertices2D.resize(_drawQuadCount * 4);
    _primitiveDrawBuf = App->Render.CreateDrawBuffer(false);
    _primitiveDrawBuf->Vertices2D.resize(1024);
    _flushDrawBuf = App->Render.CreateDrawBuffer(false);
    _flushDrawBuf->Vertices2D.resize(4);
    _contourDrawBuf = App->Render.CreateDrawBuffer(false);
    _contourDrawBuf->Vertices2D.resize(4);

    _sprData.resize(SPRITES_BUFFER_SIZE);
    _borderBuf.resize(App->Render.MAX_ATLAS_SIZE);

    MatrixHelper::MatrixOrtho(_projectionMatrixCm[0], 0.0f, static_cast<float>(_settings.ScreenWidth), static_cast<float>(_settings.ScreenHeight), 0.0f, -1.0f, 1.0f);
    _projectionMatrixCm.Transpose(); // Convert to column major order

    _rtMain = CreateRenderTarget(false, RenderTarget::SizeType::Screen, 0, 0, true);
    _rtContours = CreateRenderTarget(false, RenderTarget::SizeType::Map, 0, 0, false);
    _rtContoursMid = CreateRenderTarget(false, RenderTarget::SizeType::Map, 0, 0, false);

    DummyAnimation = new AnyFrames();
    DummyAnimation->CntFrm = 1;
    DummyAnimation->Ticks = 100;

    _eventUnsubscriber += _window->OnWindowSizeChanged += [this] { OnWindowSizeChanged(); };
}

SpriteManager::~SpriteManager()
{
    _window->Destroy();

    delete DummyAnimation;

    for (const auto* it : _sprData) {
        delete it;
    }
}

auto SpriteManager::GetWindowSize() const -> tuple<int, int>
{
    return _window->GetSize();
}

void SpriteManager::SetWindowSize(int w, int h)
{
    _window->SetSize(w, h);
}

auto SpriteManager::GetWindowPosition() const -> tuple<int, int>
{
    return _window->GetPosition();
}

void SpriteManager::SetWindowPosition(int x, int y)
{
    _window->SetPosition(x, y);
}

void SpriteManager::SetMousePosition(int x, int y)
{
    App->Input.SetMousePosition(x, y, _window);
}

auto SpriteManager::IsWindowFocused() const -> bool
{
    return _window->IsFocused();
}

void SpriteManager::MinimizeWindow()
{
    _window->Minimize();
}

auto SpriteManager::EnableFullscreen() -> bool
{
    return _window->ToggleFullscreen(true);
}

auto SpriteManager::DisableFullscreen() -> bool
{
    return _window->ToggleFullscreen(false);
}

void SpriteManager::BlinkWindow()
{
    _window->Blink();
}

void SpriteManager::SetAlwaysOnTop(bool enable)
{
    _window->AlwaysOnTop(enable);
}

void SpriteManager::BeginScene(uint clear_color)
{
    if (_rtMain != nullptr) {
        PushRenderTarget(_rtMain);
    }

#if FO_ENABLE_3D
    // Render 3d animations
    if (!_autoRedrawModel.empty()) {
        for (auto* model : _autoRedrawModel) {
            if (model->NeedDraw()) {
                RenderModel(model);
            }
        }
    }
#endif

    // Clear window
    if (clear_color != 0u) {
        ClearCurrentRenderTarget(clear_color);
    }
}

void SpriteManager::EndScene()
{
    Flush();

    if (_rtMain != nullptr) {
        PopRenderTarget();
        DrawRenderTarget(_rtMain, false, nullptr, nullptr);
    }
}

void SpriteManager::OnWindowSizeChanged()
{
    NON_CONST_METHOD_HINT();

    // Reallocate fullscreen render targets
    for (auto&& rt : _rtAll) {
        if (rt->Size != RenderTarget::SizeType::Custom) {
            AllocateRenderTargetTexture(rt.get(), rt->MainTex->LinearFiltered, rt->MainTex->WithDepth);
        }
    }
}

auto SpriteManager::CreateRenderTarget(bool with_depth, RenderTarget::SizeType size, uint width, uint height, bool linear_filtered) -> RenderTarget*
{
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

    rt->MainTex = unique_ptr<RenderTexture>(App->Render.CreateTexture(tex_width, tex_height, linear_filtered, with_depth));

    auto* prev_tex = App->Render.GetRenderTarget();
    App->Render.SetRenderTarget(rt->MainTex.get());
    App->Render.ClearRenderTarget(0, with_depth);
    App->Render.SetRenderTarget(prev_tex);
}

void SpriteManager::PushRenderTarget(RenderTarget* rt)
{
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

void SpriteManager::DrawRenderTarget(RenderTarget* rt, bool alpha_blend, const IRect* region_from, const IRect* region_to)
{
    Flush();

    if (region_from == nullptr && region_to == nullptr) {
        const auto w = static_cast<float>(rt->MainTex->Width);
        const auto h = static_cast<float>(rt->MainTex->Height);

        auto& vbuf = _flushDrawBuf->Vertices2D;
        auto pos = 0;

        vbuf[pos].X = 0.0f;
        vbuf[pos].Y = h;
        vbuf[pos].TU = 0.0f;
        vbuf[pos++].TV = 0.0f;

        vbuf[pos].X = 0.0f;
        vbuf[pos].Y = 0.0f;
        vbuf[pos].TU = 0.0f;
        vbuf[pos++].TV = 1.0f;

        vbuf[pos].X = w;
        vbuf[pos].Y = 0.0f;
        vbuf[pos].TU = 1.0f;
        vbuf[pos++].TV = 1.0f;

        vbuf[pos].X = w;
        vbuf[pos].Y = h;
        vbuf[pos].TU = 1.0f;
        vbuf[pos].TV = 0.0f;
    }
    else {
        const FRect regionf = region_from != nullptr ? *region_from : IRect(0, 0, rt->MainTex->Width, rt->MainTex->Height);
        const FRect regiont = region_to != nullptr ? *region_to : IRect(0, 0, _rtStack.back()->MainTex->Width, _rtStack.back()->MainTex->Height);
        const auto wf = static_cast<float>(rt->MainTex->Width);
        const auto hf = static_cast<float>(rt->MainTex->Height);

        auto& vbuf = _flushDrawBuf->Vertices2D;
        auto pos = 0;

        vbuf[pos].X = regiont.Left;
        vbuf[pos].Y = regiont.Bottom;
        vbuf[pos].TU = regionf.Left / wf;
        vbuf[pos++].TV = 1.0f - regionf.Bottom / hf;

        vbuf[pos].X = regiont.Left;
        vbuf[pos].Y = regiont.Top;
        vbuf[pos].TU = regionf.Left / wf;
        vbuf[pos++].TV = 1.0f - regionf.Top / hf;

        vbuf[pos].X = regiont.Right;
        vbuf[pos].Y = regiont.Top;
        vbuf[pos].TU = regionf.Right / wf;
        vbuf[pos++].TV = 1.0f - regionf.Top / hf;

        vbuf[pos].X = regiont.Right;
        vbuf[pos].Y = regiont.Bottom;
        vbuf[pos].TU = regionf.Right / wf;
        vbuf[pos].TV = 1.0f - regionf.Bottom / hf;
    }

    auto* effect = rt->CustomDrawEffect != nullptr ? rt->CustomDrawEffect : _effectMngr.Effects.FlushRenderTarget;

    effect->MainTex = rt->MainTex.get();
    effect->DisableBlending = !alpha_blend;
    effect->DrawBuffer(_flushDrawBuf);
}

auto SpriteManager::GetRenderTargetPixel(RenderTarget* rt, int x, int y) const -> uint
{
    // Try find in last picks
    for (auto& pix : rt->LastPixelPicks) {
        if (std::get<0>(pix) != 0 && std::get<1>(pix) != 0) {
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
}

void SpriteManager::ClearCurrentRenderTarget(uint color, bool with_depth)
{
    App->Render.ClearRenderTarget(color, with_depth);
}

void SpriteManager::PushScissor(int l, int t, int r, int b)
{
    Flush();

    _scissorStack.push_back(l);
    _scissorStack.push_back(t);
    _scissorStack.push_back(r);
    _scissorStack.push_back(b);

    RefreshScissor();
}

void SpriteManager::PopScissor()
{
    if (!_scissorStack.empty()) {
        Flush();

        _scissorStack.resize(_scissorStack.size() - 4);

        RefreshScissor();
    }
}

void SpriteManager::RefreshScissor()
{
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
    NON_CONST_METHOD_HINT();

    if (!_scissorStack.empty() && !_rtStack.empty() && _rtStack.back() == _rtMain) {
        const auto x = _scissorRect.Left;
        const auto y = static_cast<int>(_rtStack.back()->MainTex->Height) - _scissorRect.Bottom;
        const auto w = static_cast<uint>(_scissorRect.Right - _scissorRect.Left);
        const auto h = static_cast<uint>(_scissorRect.Bottom - _scissorRect.Top);
        App->Render.EnableScissor(x, y, w, h);
    }
}

void SpriteManager::DisableScissor()
{
    NON_CONST_METHOD_HINT();

    if (!_scissorStack.empty() && !_rtStack.empty() && _rtStack.back() == _rtMain) {
        App->Render.DisableScissor();
    }
}

void SpriteManager::PushAtlasType(AtlasType atlas_type)
{
    PushAtlasType(atlas_type, false);
}

void SpriteManager::PushAtlasType(AtlasType atlas_type, bool one_image)
{
    _atlasStack.emplace_back(atlas_type, one_image);
}

void SpriteManager::PopAtlasType()
{
    _atlasStack.pop_back();
}

void SpriteManager::AccumulateAtlasData()
{
    _accumulatorActive = true;
}

void SpriteManager::FlushAccumulatedAtlasData()
{
    _accumulatorActive = false;
    if (_accumulatorSprInfo.empty()) {
        return;
    }

    // Sort by size
    std::sort(_accumulatorSprInfo.begin(), _accumulatorSprInfo.end(), [](SpriteInfo* si1, SpriteInfo* si2) { return si1->Width * si1->Height > si2->Width * si2->Height; });

    for (auto* spr_info : _accumulatorSprInfo) {
        FillAtlas(spr_info, spr_info->AccData);
        delete[] spr_info->AccData;
        spr_info->AccData = nullptr;
    }
    _accumulatorSprInfo.clear();
}

auto SpriteManager::IsAccumulateAtlasActive() const -> bool
{
    return _accumulatorActive;
}

auto SpriteManager::CreateAtlas(uint w, uint h) -> TextureAtlas*
{
    auto atlas = std::make_unique<TextureAtlas>();
    atlas->Type = std::get<0>(_atlasStack.back());

    if (!std::get<1>(_atlasStack.back())) {
        switch (atlas->Type) {
        case AtlasType::Static:
            w = std::min(AppRender::MAX_ATLAS_WIDTH, 4096u);
            h = std::min(AppRender::MAX_ATLAS_HEIGHT, 4096u);
            break;
        case AtlasType::Dynamic:
            w = std::min(AppRender::MAX_ATLAS_WIDTH, 2048u);
            h = std::max(AppRender::MAX_ATLAS_HEIGHT, 8192u);
            break;
        case AtlasType::MeshTextures:
            w = std::min(AppRender::MAX_ATLAS_WIDTH, 1024u);
            h = std::min(AppRender::MAX_ATLAS_HEIGHT, 2048u);
            break;
        case AtlasType::Splash:
            throw UnreachablePlaceException(LINE_STR);
        }
    }

    atlas->RT = CreateRenderTarget(false, RenderTarget::SizeType::Custom, w, h, true);
    atlas->RT->LastPixelPicks.reserve(MAX_STORED_PIXEL_PICKS);
    atlas->MainTex = atlas->RT->MainTex.get();
    atlas->Width = w;
    atlas->Height = h;
    atlas->RootNode = _accumulatorActive ? std::make_unique<TextureAtlas::SpaceNode>(0, 0, w, h) : nullptr;

    _allAtlases.push_back(std::move(atlas));
    return _allAtlases.back().get();
}

auto SpriteManager::FindAtlasPlace(SpriteInfo* si, int& x, int& y) -> TextureAtlas*
{
    // Find place in already created atlas
    TextureAtlas* atlas = nullptr;
    const auto atlas_type = std::get<0>(_atlasStack.back());
    const auto w = static_cast<uint>(si->Width + ATLAS_SPRITES_PADDING * 2);
    const auto h = static_cast<uint>(si->Height + ATLAS_SPRITES_PADDING * 2);

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
                x = a->CurX - static_cast<int>(a->LineW);
                y = a->CurY + static_cast<int>(a->LineCurH);
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
                y = a->CurY + static_cast<int>(a->LineMaxH);
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

    // Create new
    if (atlas == nullptr) {
        atlas = CreateAtlas(w, h);
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

    // Return parameters
    x += ATLAS_SPRITES_PADDING;
    y += ATLAS_SPRITES_PADDING;
    return atlas;
}

void SpriteManager::DestroyAtlases(AtlasType atlas_type)
{
    for (auto it = _allAtlases.begin(); it != _allAtlases.end();) {
        auto& atlas = *it;
        if (atlas->Type == atlas_type) {
            for (auto& it_ : _sprData) {
                const auto* si = it_;
                if (si != nullptr && si->Atlas == atlas.get()) {
#if FO_ENABLE_3D
                    if (si->Model != nullptr) {
                        si->Model->SprId = 0;
                    }
#endif

                    delete si;
                    it_ = nullptr;
                }
            }

            it = _allAtlases.erase(it);
        }
        else {
            ++it;
        }
    }
}

void SpriteManager::DumpAtlases()
{
    uint atlases_memory_size = 0;
    for (auto& atlas : _allAtlases) {
        atlases_memory_size += atlas->Width * atlas->Height * 4;
    }

    const string path = _str("./{}_{}.{:03}mb/", static_cast<uint>(time(nullptr)), atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000);

    auto cnt = 0;
    for (auto& atlas : _allAtlases) {
        string fname = _str("{}{}_{}_{}x{}.png", path, cnt, atlas->Type, atlas->Width, atlas->Height);
        // Todo: restore texture saving
        // SaveTexture(atlas->MainTex, fname, false);
        throw NotImplementedException(LINE_STR);
        cnt++;
    }
}

auto SpriteManager::RequestFillAtlas(SpriteInfo* si, uint w, uint h, const uint* data) -> uint
{
    // Get width, height
    si->DataAtlasType = std::get<0>(_atlasStack.back());
    si->DataAtlasOneImage = std::get<1>(_atlasStack.back());
    si->Width = static_cast<ushort>(w);
    si->Height = static_cast<ushort>(h);

    // Find place on atlas
    if (_accumulatorActive) {
        si->AccData = new uint[w * h];
        std::memcpy(si->AccData, data, w * h * sizeof(uint));
        _accumulatorSprInfo.push_back(si);
    }
    else {
        FillAtlas(si, data);
    }

    // Store sprite
    size_t index = 1;
    for (; index < _sprData.size(); index++) {
        if (_sprData[index] == nullptr) {
            break;
        }
    }

    if (index < _sprData.size()) {
        _sprData[index] = si;
    }
    else {
        _sprData.push_back(si);
    }
    return static_cast<uint>(index);
}

void SpriteManager::FillAtlas(SpriteInfo* si, const uint* data)
{
    const uint w = si->Width;
    const uint h = si->Height;

    PushAtlasType(si->DataAtlasType, si->DataAtlasOneImage);
    auto x = 0;
    auto y = 0;
    auto* atlas = FindAtlasPlace(si, x, y);
    PopAtlasType();

    // Refresh texture
    auto* tex = atlas->MainTex;
    tex->UpdateTextureRegion(IRect(x, y, x + w - 1, y + h - 1), data);

    // 1px border for correct linear interpolation
    // Top
    tex->UpdateTextureRegion(IRect(x, y - 1, x + w - 1, y - 1), data);

    // Bottom
    tex->UpdateTextureRegion(IRect(x, y + h, x + w - 1, y + h), data + (h - 1) * w);

    // Left
    for (uint i = 0; i < h; i++) {
        _borderBuf[i + 1] = *(data + i * w);
    }
    _borderBuf[0] = _borderBuf[1];
    _borderBuf[h + 1] = _borderBuf[h];
    tex->UpdateTextureRegion(IRect(x - 1, y - 1, x - 1, y + h), _borderBuf.data());

    // Right
    for (uint i = 0; i < h; i++) {
        _borderBuf[i + 1] = *(data + i * w + (w - 1));
    }
    _borderBuf[0] = _borderBuf[1];
    _borderBuf[h + 1] = _borderBuf[h];
    tex->UpdateTextureRegion(IRect(x + w, y - 1, x + w, y + h), _borderBuf.data());

    // Invalidate last pixel color picking
    if (!atlas->RT->LastPixelPicks.empty()) {
        atlas->RT->LastPixelPicks.clear();
    }

    // Set parameters
    si->Atlas = atlas;
    si->SprRect.Left = static_cast<float>(x) / static_cast<float>(atlas->Width);
    si->SprRect.Top = static_cast<float>(y) / static_cast<float>(atlas->Height);
    si->SprRect.Right = static_cast<float>(x + w) / static_cast<float>(atlas->Width);
    si->SprRect.Bottom = static_cast<float>(y + h) / static_cast<float>(atlas->Height);
}

auto SpriteManager::LoadAnimation(string_view fname, bool use_dummy) -> AnyFrames*
{
    auto* dummy = use_dummy ? DummyAnimation : nullptr;

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
        result = Load3dAnimation(fname);
#else
        throw NotEnabled3DException("Can't load animation, 3D submodule not enabled", fname);
#endif
    }
    else {
        result = Load2dAnimation(fname);
    }

    return result != nullptr ? result : dummy;
}

auto SpriteManager::Load2dAnimation(string_view fname) -> AnyFrames*
{
    auto file = _fileSys.ReadFile(fname);
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

    for (ushort dir = 0; dir < dirs; dir++) {
        auto* dir_anim = anim->GetDir(dir);
        const auto ox = file.GetLEShort();
        const auto oy = file.GetLEShort();
        for (ushort i = 0; i < frames_count; i++) {
            if (file.GetUChar() == 0u) {
                auto* si = new SpriteInfo();
                si->OffsX = ox;
                si->OffsY = oy;
                const auto w = file.GetLEUShort();
                const auto h = file.GetLEUShort();
                dir_anim->NextX[i] = file.GetLEShort();
                dir_anim->NextY[i] = file.GetLEShort();
                const auto* data = file.GetCurBuf();
                dir_anim->Ind[i] = RequestFillAtlas(si, w, h, reinterpret_cast<const uint*>(data));
                file.GoForward(w * h * 4);
            }
            else {
                const auto index = file.GetLEUShort();
                dir_anim->Ind[i] = dir_anim->Ind[index];
            }
        }
    }

    const auto check_number2 = file.GetUChar();
    RUNTIME_ASSERT(check_number2 == 42);
    return anim;
}

auto SpriteManager::ReloadAnimation(AnyFrames* anim, string_view fname) -> AnyFrames*
{
    if (fname.empty()) {
        return anim;
    }

    // Release old images
    if (anim != nullptr) {
        for (uint i = 0; i < anim->CntFrm; i++) {
            const auto* si = GetSpriteInfo(anim->Ind[i]);
            if (si != nullptr) {
                DestroyAtlases(si->Atlas->Type);
            }
        }
        DestroyAnyFrames(anim);
    }

    // Load fresh
    return LoadAnimation(fname, true);
}

#if FO_ENABLE_3D
void SpriteManager::Init3dSubsystem(GameTimer& game_time, NameResolver& name_resolver, AnimationResolver& anim_name_resolver)
{
    RUNTIME_ASSERT(!_modelMngr);

    _modelMngr = std::make_unique<ModelManager>(_settings, _fileSys, _effectMngr, game_time, name_resolver, anim_name_resolver, [this](MeshTexture* mesh_tex) {
        PushAtlasType(AtlasType::MeshTextures);
        auto* anim = LoadAnimation(_str("{}/{}", _str(mesh_tex->ModelPath).extractDir(), mesh_tex->Name), false);
        PopAtlasType();

        if (anim != nullptr) {
            const auto* si = GetSpriteInfo(anim->Ind[0]);
            mesh_tex->MainTex = si->Atlas->MainTex;
            mesh_tex->AtlasOffsetData[0] = si->SprRect[0];
            mesh_tex->AtlasOffsetData[1] = si->SprRect[1];
            mesh_tex->AtlasOffsetData[2] = si->SprRect[2] - si->SprRect[0];
            mesh_tex->AtlasOffsetData[3] = si->SprRect[3] - si->SprRect[1];
            DestroyAnyFrames(anim);
        }
    });
}

void SpriteManager::Preload3dModel(string_view model_name)
{
    NON_CONST_METHOD_HINT();
    RUNTIME_ASSERT(_modelMngr);

    _modelMngr->PreloadModel(model_name);
}

auto SpriteManager::Load3dAnimation(string_view fname) -> AnyFrames*
{
    RUNTIME_ASSERT(_modelMngr);

    // Load 3d animation
    auto&& model = unique_ptr<ModelInstance>(_modelMngr->CreateModel(fname));
    if (model == nullptr) {
        return nullptr;
    }

    model->StartMeshGeneration();

    // Get animation data
    const auto [period, proc_from, proc_to, dir] = model->GetRenderFramesData();

    // Set fir
    if (dir < 0) {
        model->SetLookDirAngle(-dir);
        model->SetMoveDirAngle(-dir, false);
    }
    else {
        model->SetDir(static_cast<uchar>(dir), false);
    }

    // Calculate needed information
    const auto frame_time = 1.0f / static_cast<float>(_settings.Animation3dFPS != 0u ? _settings.Animation3dFPS : 10); // 1 second / fps
    const auto period_from = period * static_cast<float>(proc_from) / 100.0f;
    const auto period_to = period * static_cast<float>(proc_to) / 100.0f;
    const auto period_len = fabs(period_to - period_from);
    const auto proc_step = static_cast<float>(proc_to - proc_from) / (period_len / frame_time);
    const auto frames_count = static_cast<int>(std::ceil(period_len / frame_time));

    // If no animations available than render just one
    if (period == 0.0f || proc_from == proc_to || frames_count <= 1) {
        model->SetAnimation(0, proc_from * 10, nullptr, ANIMATION_ONE_TIME | ANIMATION_STAY);
        RenderModel(model.get());

        auto* anim = CreateAnyFrames(1, 100);
        anim->Ind[0] = model->SprId;
        return anim;
    }

    auto* anim = CreateAnyFrames(frames_count, static_cast<uint>(period_len * 1000.0f));
    auto cur_proc = static_cast<float>(proc_from);
    auto prev_cur_proci = -1;

    for (auto i = 0; i < frames_count; i++) {
        const auto cur_proci = (proc_to > proc_from ? static_cast<int>(10.0f * cur_proc + 0.5f) : static_cast<int>(10.0f * cur_proc));

        // Previous frame is different
        if (cur_proci != prev_cur_proci) {
            model->SetAnimation(0, cur_proci, nullptr, ANIMATION_ONE_TIME | ANIMATION_STAY);
            RenderModel(model.get());

            anim->Ind[i] = model->SprId;
        }
        // Previous frame is same
        else if (i > 0) {
            anim->Ind[i] = anim->Ind[i - 1];
        }

        cur_proc += proc_step;
        prev_cur_proci = cur_proci;
    }

    return anim;
}

void SpriteManager::RenderModel(ModelInstance* model)
{
    RUNTIME_ASSERT(_modelMngr);

    // Find place for render
    if (model->SprId == 0u) {
        RefreshModelSprite(model);
    }

    // Find place for render
    const auto* si = _sprData[model->SprId];
    const uint frame_width = si->Width * ModelInstance::FRAME_SCALE;
    const uint frame_height = si->Height * ModelInstance::FRAME_SCALE;

    RenderTarget* rt = nullptr;
    for (auto* rt_ : _rt3D) {
        if (rt_->MainTex->Width == frame_width && rt_->MainTex->Height == frame_height) {
            rt = rt_;
            break;
        }
    }
    if (rt == nullptr) {
        rt = CreateRenderTarget(true, RenderTarget::SizeType::Custom, frame_width, frame_height, false);
        _rt3D.push_back(rt);
    }

    PushRenderTarget(rt);
    ClearCurrentRenderTarget(0, true);

    // Draw model
    model->Draw();

    // Restore render target
    PopRenderTarget();

    // Copy render
    const auto l = iround(si->SprRect.Left * static_cast<float>(si->Atlas->Width));
    const auto t = iround((1.0f - si->SprRect.Top) * static_cast<float>(si->Atlas->Height));
    const auto r = iround(si->SprRect.Right * static_cast<float>(si->Atlas->Width));
    const auto b = iround((1.0f - si->SprRect.Bottom) * static_cast<float>(si->Atlas->Height));
    const auto region_to = IRect(l, t, r, b);

    PushRenderTarget(si->Atlas->RT);
    DrawRenderTarget(rt, false, nullptr, &region_to);
    PopRenderTarget();
}

void SpriteManager::Draw3d(int x, int y, ModelInstance* model, uint color)
{
    RUNTIME_ASSERT(_modelMngr);

    model->StartMeshGeneration();
    RenderModel(model);

    const auto* si = _sprData[model->SprId];
    DrawSprite(model->SprId, x - si->Width / 2 + si->OffsX, y - si->Height + si->OffsY, color);
}

auto SpriteManager::LoadModel(string_view fname, bool auto_redraw) -> ModelInstance*
{
    RUNTIME_ASSERT(_modelMngr);

    // Fill data
    auto* model = _modelMngr->CreateModel(fname);
    if (model == nullptr) {
        return nullptr;
    }

    // Create render sprite
    model->SprId = 0;
    model->SprAtlasType = static_cast<int>(std::get<0>(_atlasStack.back()));
    if (auto_redraw) {
        RefreshModelSprite(model);
        _autoRedrawModel.push_back(model);
    }
    return model;
}

void SpriteManager::RefreshModelSprite(ModelInstance* model)
{
    RUNTIME_ASSERT(_modelMngr);

    // Free old place
    if (model->SprId != 0u) {
        _sprData[model->SprId]->Model = nullptr;
        model->SprId = 0;
    }

    // Render size
    model->SetupFrame();
    auto&& [draw_width, draw_height] = model->GetDrawSize();

    // Find already created place for rendering
    uint index = 0;
    for (size_t i = 0; i < _sprData.size(); i++) {
        const auto* si = _sprData[i];
        if (si != nullptr && si->UsedForModel && si->Model == nullptr && si->Width == draw_width && si->Height == draw_height && si->Atlas->Type == static_cast<AtlasType>(model->SprAtlasType)) {
            index = static_cast<uint>(i);
            break;
        }
    }

    // Create new place for rendering
    if (index == 0u) {
        PushAtlasType(static_cast<AtlasType>(model->SprAtlasType));
        index = RequestFillAtlas(nullptr, draw_width, draw_height, nullptr);
        PopAtlasType();

        auto* si = _sprData[index];
        si->OffsY = static_cast<short>(draw_height / 4);
        si->UsedForModel = true;
    }

    // Cross links
    model->SprId = index;
    _sprData[index]->Model = model;
}

void SpriteManager::FreeModel(ModelInstance* model)
{
    RUNTIME_ASSERT(_modelMngr);
    RUNTIME_ASSERT(model);

    const auto it = std::find(_autoRedrawModel.begin(), _autoRedrawModel.end(), model);
    if (it != _autoRedrawModel.end()) {
        _autoRedrawModel.erase(it);
    }

    if (model->SprId != 0u) {
        _sprData[model->SprId]->Model = nullptr;
    }

    delete model;
}
#endif

auto SpriteManager::CreateAnyFrames(uint frames, uint ticks) -> AnyFrames*
{
    auto* anim = static_cast<AnyFrames*>(_anyFramesPool.Get());
    std::memset(anim, 0, sizeof(AnyFrames));
    anim->CntFrm = std::min(frames, static_cast<uint>(MAX_FRAMES));
    anim->Ticks = ticks != 0u ? ticks : frames * 100;
    return anim;
}

void SpriteManager::CreateAnyFramesDirAnims(AnyFrames* anim, uint dirs)
{
    RUNTIME_ASSERT(dirs > 1);
    RUNTIME_ASSERT(dirs == 6 || dirs == 8);
    anim->DirCount = dirs;
    for (uint dir = 0; dir < dirs - 1; dir++) {
        anim->Dirs[dir] = CreateAnyFrames(anim->CntFrm, anim->Ticks);
    }
}

void SpriteManager::DestroyAnyFrames(AnyFrames* anim)
{
    if (anim == nullptr || anim == DummyAnimation) {
        return;
    }

    for (auto dir = 1; dir < anim->DirCount; dir++) {
        _anyFramesPool.Put(anim->GetDir(dir));
    }
    _anyFramesPool.Put(anim);
}

void SpriteManager::Flush()
{
    if (_curDrawQuad == 0) {
        return;
    }

    size_t pos = 0;
    for (const auto& dip : _dipQueue) {
        dip.SourceEffect->DrawBuffer(_spritesDrawBuf, pos, dip.SpritesCount * 6, dip.MainTex);
        pos += dip.SpritesCount * 6;
    }

    _dipQueue.clear();
    _curDrawQuad = 0;
}

void SpriteManager::DrawSprite(uint id, int x, int y, uint color)
{
    if (id == 0u) {
        return;
    }

    const auto* si = _sprData[id];
    if (si == nullptr) {
        return;
    }

    auto* effect = si->DrawEffect != nullptr ? si->DrawEffect : _effectMngr.Effects.Iface;

    if (_dipQueue.empty() || _dipQueue.back().SourceEffect->CanBatch(effect)) {
        _dipQueue.push_back(DipData {si->Atlas->MainTex, effect, 1});
    }
    else {
        _dipQueue.back().SpritesCount++;
    }

    if (color == 0u) {
        color = COLOR_IFACE;
    }
    color = COLOR_SWAP_RB(color);

    auto& vbuf = _spritesDrawBuf->Vertices2D;
    auto pos = _curDrawQuad * 4;

    vbuf[pos].X = static_cast<float>(x);
    vbuf[pos].Y = static_cast<float>(y + si->Height);
    vbuf[pos].TU = si->SprRect.Left;
    vbuf[pos].TV = si->SprRect.Bottom;
    vbuf[pos++].Diffuse = color;

    vbuf[pos].X = static_cast<float>(x);
    vbuf[pos].Y = static_cast<float>(y);
    vbuf[pos].TU = si->SprRect.Left;
    vbuf[pos].TV = si->SprRect.Top;
    vbuf[pos++].Diffuse = color;

    vbuf[pos].X = static_cast<float>(x + si->Width);
    vbuf[pos].Y = static_cast<float>(y);
    vbuf[pos].TU = si->SprRect.Right;
    vbuf[pos].TV = si->SprRect.Top;
    vbuf[pos++].Diffuse = color;

    vbuf[pos].X = static_cast<float>(x + si->Width);
    vbuf[pos].Y = static_cast<float>(y + si->Height);
    vbuf[pos].TU = si->SprRect.Right;
    vbuf[pos].TV = si->SprRect.Bottom;
    vbuf[pos].Diffuse = color;

    if (++_curDrawQuad == _drawQuadCount) {
        Flush();
    }
}

void SpriteManager::DrawSpriteSize(uint id, int x, int y, int w, int h, bool zoom_up, bool center, uint color)
{
    DrawSpriteSizeExt(id, x, y, w, h, zoom_up, center, false, color);
}

void SpriteManager::DrawSpriteSizeExt(uint id, int x, int y, int w, int h, bool zoom_up, bool center, bool stretch, uint color)
{
    if (id == 0u) {
        return;
    }

    const auto* si = _sprData[id];
    if (si == nullptr) {
        return;
    }

    auto xf = static_cast<float>(x);
    auto yf = static_cast<float>(y);
    auto wf = static_cast<float>(si->Width);
    auto hf = static_cast<float>(si->Height);
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

    auto* effect = si->DrawEffect != nullptr ? si->DrawEffect : _effectMngr.Effects.Iface;

    if (_dipQueue.empty() || _dipQueue.back().MainTex != si->Atlas->MainTex || _dipQueue.back().SourceEffect != effect) {
        _dipQueue.push_back(DipData {si->Atlas->MainTex, effect, 1});
    }
    else {
        _dipQueue.back().SpritesCount++;
    }

    if (color == 0u) {
        color = COLOR_IFACE;
    }
    color = COLOR_SWAP_RB(color);

    auto& vbuf = _spritesDrawBuf->Vertices2D;
    auto pos = _curDrawQuad * 4;

    vbuf[pos].X = xf;
    vbuf[pos].Y = yf + hf;
    vbuf[pos].TU = si->SprRect.Left;
    vbuf[pos].TV = si->SprRect.Bottom;
    vbuf[pos++].Diffuse = color;

    vbuf[pos].X = xf;
    vbuf[pos].Y = yf;
    vbuf[pos].TU = si->SprRect.Left;
    vbuf[pos].TV = si->SprRect.Top;
    vbuf[pos++].Diffuse = color;

    vbuf[pos].X = xf + wf;
    vbuf[pos].Y = yf;
    vbuf[pos].TU = si->SprRect.Right;
    vbuf[pos].TV = si->SprRect.Top;
    vbuf[pos++].Diffuse = color;

    vbuf[pos].X = xf + wf;
    vbuf[pos].Y = yf + hf;
    vbuf[pos].TU = si->SprRect.Right;
    vbuf[pos].TV = si->SprRect.Bottom;
    vbuf[pos].Diffuse = color;

    if (++_curDrawQuad == _drawQuadCount) {
        Flush();
    }
}

void SpriteManager::DrawSpritePattern(uint id, int x, int y, int w, int h, int spr_width, int spr_height, uint color)
{
    if (id == 0u) {
        return;
    }
    if (w == 0 || h == 0) {
        return;
    }

    const auto* si = _sprData[id];
    if (si == nullptr) {
        return;
    }

    auto width = static_cast<float>(si->Width);
    auto height = static_cast<float>(si->Height);

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

    if (color == 0u) {
        color = COLOR_IFACE;
    }
    color = COLOR_SWAP_RB(color);

    auto* effect = si->DrawEffect != nullptr ? si->DrawEffect : _effectMngr.Effects.Iface;

    const auto last_right_offs = (si->SprRect.Right - si->SprRect.Left) / width;
    const auto last_bottom_offs = (si->SprRect.Bottom - si->SprRect.Top) / height;

    for (auto yy = static_cast<float>(y), end_y = static_cast<float>(y + h); yy < end_y; yy += height) {
        const auto last_y = yy + height >= end_y;
        for (auto xx = static_cast<float>(x), end_x = static_cast<float>(x + w); xx < end_x; xx += width) {
            const auto last_x = xx + width >= end_x;

            if (_dipQueue.empty() || _dipQueue.back().MainTex != si->Atlas->MainTex || _dipQueue.back().SourceEffect != effect) {
                _dipQueue.push_back(DipData {si->Atlas->MainTex, effect, 1});
            }
            else {
                _dipQueue.back().SpritesCount++;
            }

            const auto local_width = (last_x ? end_x - xx : width);
            const auto local_height = (last_y ? end_y - yy : height);
            const auto local_right = (last_x ? si->SprRect.Left + last_right_offs * local_width : si->SprRect.Right);
            const auto local_bottom = (last_y ? si->SprRect.Top + last_bottom_offs * local_height : si->SprRect.Bottom);

            auto& vbuf = _spritesDrawBuf->Vertices2D;
            auto pos = _curDrawQuad * 4;

            vbuf[pos].X = xx;
            vbuf[pos].Y = yy + local_height;
            vbuf[pos].TU = si->SprRect.Left;
            vbuf[pos].TV = local_bottom;
            vbuf[pos++].Diffuse = color;

            vbuf[pos].X = xx;
            vbuf[pos].Y = yy;
            vbuf[pos].TU = si->SprRect.Left;
            vbuf[pos].TV = si->SprRect.Top;
            vbuf[pos++].Diffuse = color;

            vbuf[pos].X = xx + local_width;
            vbuf[pos].Y = yy;
            vbuf[pos].TU = local_right;
            vbuf[pos].TV = si->SprRect.Top;
            vbuf[pos++].Diffuse = color;

            vbuf[pos].X = xx + local_width;
            vbuf[pos].Y = yy + local_height;
            vbuf[pos].TU = local_right;
            vbuf[pos].TV = local_bottom;
            vbuf[pos].Diffuse = color;

            if (++_curDrawQuad == _drawQuadCount) {
                Flush();
            }
        }
    }

    DisableScissor();
}

void SpriteManager::PrepareSquare(vector<PrimitivePoint>& points, const IRect& r, uint color)
{
    points.push_back({r.Right, r.Bottom, color});
    points.push_back({r.Left, r.Top, color});
    points.push_back({r.Right, r.Bottom, color});
    points.push_back({r.Left, r.Top, color});
    points.push_back({r.Right, r.Top, color});
    points.push_back({r.Right, r.Bottom, color});
}

void SpriteManager::PrepareSquare(vector<PrimitivePoint>& points, IPoint lt, IPoint rt, IPoint lb, IPoint rb, uint color)
{
    points.push_back({lb.X, lb.Y, color});
    points.push_back({lt.X, lt.Y, color});
    points.push_back({rb.X, rb.Y, color});
    points.push_back({lt.X, lt.Y, color});
    points.push_back({rt.X, rt.Y, color});
    points.push_back({rb.X, rb.Y, color});
}

auto SpriteManager::GetDrawRect(Sprite* prep) const -> IRect
{
    const auto id = prep->PSprId != nullptr ? *prep->PSprId : prep->SprId;
    if (id >= _sprData.size()) {
        return IRect();
    }

    const auto* si = _sprData[id];
    if (si == nullptr) {
        return IRect();
    }

    auto x = prep->ScrX - si->Width / 2 + si->OffsX + *prep->PScrX;
    auto y = prep->ScrY - si->Height + si->OffsY + *prep->PScrY;
    if (prep->OffsX != nullptr) {
        x += *prep->OffsX;
    }
    if (prep->OffsY != nullptr) {
        y += *prep->OffsY;
    }

    return {x, y, x + si->Width, y + si->Height};
}

void SpriteManager::InitializeEgg(string_view egg_name)
{
    _eggValid = false;
    _eggHx = _eggHy = _eggX = _eggY = 0;
    if (auto* egg_frames = LoadAnimation(egg_name, true); egg_frames != nullptr) {
        _sprEgg = GetSpriteInfo(egg_frames->Ind[0]);

        DestroyAnyFrames(egg_frames);

        _eggSprWidth = _sprEgg->Width;
        _eggSprHeight = _sprEgg->Height;
        _eggAtlasWidth = static_cast<float>(App->Render.MAX_ATLAS_WIDTH);
        _eggAtlasHeight = static_cast<float>(App->Render.MAX_ATLAS_HEIGHT);

        const auto x = static_cast<int>(_sprEgg->Atlas->MainTex->SizeData[0] * _sprEgg->SprRect.Left);
        const auto y = static_cast<int>(_sprEgg->Atlas->MainTex->SizeData[1] * _sprEgg->SprRect.Top);
        _eggData = _sprEgg->Atlas->MainTex->GetTextureRegion(x, y, _eggSprWidth, _eggSprHeight);
    }
    else {
        WriteLog("Load sprite '{}' fail. Egg disabled", egg_name);
    }
}

auto SpriteManager::CompareHexEgg(ushort hx, ushort hy, int egg_type) const -> bool
{
    if (egg_type == EGG_ALWAYS) {
        return true;
    }
    if (_eggHy == hy && (hx & 1) != 0 && (_eggHx & 1) == 0) {
        hy--;
    }
    switch (egg_type) {
    case EGG_X:
        if (hx >= _eggHx) {
            return true;
        }
        break;
    case EGG_Y:
        if (hy >= _eggHy) {
            return true;
        }
        break;
    case EGG_X_AND_Y:
        if (hx >= _eggHx || hy >= _eggHy) {
            return true;
        }
        break;
    case EGG_X_OR_Y:
        if (hx >= _eggHx && hy >= _eggHy) {
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

void SpriteManager::SetEgg(ushort hx, ushort hy, Sprite* spr)
{
    if (_sprEgg == nullptr) {
        return;
    }

    const auto id = spr->PSprId != nullptr ? *spr->PSprId : spr->SprId;
    const auto* si = _sprData[id];
    if (si == nullptr) {
        return;
    }

    _eggX = spr->ScrX + si->OffsX - _sprEgg->Width / 2 + *spr->OffsX + *spr->PScrX;
    _eggY = spr->ScrY - si->Height / 2 + si->OffsY - _sprEgg->Height / 2 + *spr->OffsY + *spr->PScrY;
    _eggHx = hx;
    _eggHy = hy;
    _eggValid = true;
}

void SpriteManager::DrawSprites(Sprites& dtree, bool collect_contours, bool use_egg, int draw_oder_from, int draw_oder_to, bool prerender, int prerender_ox, int prerender_oy)
{
    if (dtree.Size() == 0u) {
        return;
    }

    vector<PrimitivePoint> borders;

    if (!_eggValid) {
        use_egg = false;
    }

    const auto ex = _eggX + _settings.ScrOx;
    const auto ey = _eggY + _settings.ScrOy;

    for (auto* spr = dtree.RootSprite(); spr != nullptr; spr = spr->ChainChild) {
        RUNTIME_ASSERT(spr->Valid);

        if (spr->DrawOrderType < draw_oder_from) {
            continue;
        }
        if (spr->DrawOrderType > draw_oder_to) {
            break;
        }

        const auto id = spr->PSprId != nullptr ? *spr->PSprId : spr->SprId;
        auto* si = _sprData[id];
        if (si == nullptr) {
            continue;
        }

        // Coords
        auto x = spr->ScrX - si->Width / 2 + si->OffsX + *spr->PScrX;
        auto y = spr->ScrY - si->Height + si->OffsY + *spr->PScrY;
        if (!prerender) {
            x += _settings.ScrOx;
            y += _settings.ScrOy;
        }
        if (spr->OffsX != nullptr) {
            x += *spr->OffsX;
        }
        if (spr->OffsY != nullptr) {
            y += *spr->OffsY;
        }

        const auto zoom = _settings.SpritesZoom;

        // Base color
        uint color_r = 0;
        uint color_l = 0;
        if (spr->Color != 0u) {
            color_r = color_l = spr->Color | 0xFF000000;
        }
        else {
            color_r = color_l = _baseColor;
        }

        // Light
        if (spr->Light != nullptr) {
            static auto light_func = [](uint& c, const uchar* l, const uchar* l2) {
                const int lr = *l;
                const int lg = *(l + 1);
                const int lb = *(l + 2);
                const int lr2 = *l2;
                const int lg2 = *(l2 + 1);
                const int lb2 = *(l2 + 2);
                auto& r = reinterpret_cast<uchar*>(&c)[2];
                auto& g = reinterpret_cast<uchar*>(&c)[1];
                auto& b = reinterpret_cast<uchar*>(&c)[0];
                const auto ir = static_cast<int>(r) + (lr + lr2) / 2;
                const auto ig = static_cast<int>(g) + (lg + lg2) / 2;
                const auto ib = static_cast<int>(b) + (lb + lb2) / 2;
                r = static_cast<uchar>(std::min(ir, 255));
                g = static_cast<uchar>(std::min(ig, 255));
                b = static_cast<uchar>(std::min(ib, 255));
            };
            light_func(color_r, spr->Light, spr->LightRight);
            light_func(color_l, spr->Light, spr->LightLeft);
        }

        // Alpha
        if (spr->Alpha != nullptr) {
            reinterpret_cast<uchar*>(&color_r)[3] = *spr->Alpha;
            reinterpret_cast<uchar*>(&color_l)[3] = *spr->Alpha;
        }

        // Fix color
        color_r = COLOR_SWAP_RB(color_r);
        color_l = COLOR_SWAP_RB(color_l);

        // Check borders
        if (!prerender) {
            if (static_cast<float>(x) / zoom > static_cast<float>(_settings.ScreenWidth) || static_cast<float>(x + si->Width) / zoom < 0.0f || //
                static_cast<float>(y) / zoom > static_cast<float>(_settings.ScreenHeight - _settings.ScreenHudHeight) || static_cast<float>(y + si->Height) / zoom < 0.0f) {
                continue;
            }
        }

        // Egg process
        auto egg_added = false;
        if (use_egg && spr->EggType != 0 && CompareHexEgg(spr->HexX, spr->HexY, spr->EggType)) {
            auto x1 = x - ex;
            auto y1 = y - ey;
            auto x2 = x1 + si->Width;
            auto y2 = y1 + si->Height;

            if (!(x1 >= _eggSprWidth || y1 >= _eggSprHeight || x2 < 0 || y2 < 0)) {
                x1 = std::max(x1, 0);
                y1 = std::max(y1, 0);
                x2 = std::min(x2, _eggSprWidth);
                y2 = std::min(y2, _eggSprHeight);

                const auto x1_f = static_cast<float>(x1 + ATLAS_SPRITES_PADDING);
                const auto x2_f = static_cast<float>(x2 + ATLAS_SPRITES_PADDING);
                const auto y1_f = static_cast<float>(y1 + ATLAS_SPRITES_PADDING);
                const auto y2_f = static_cast<float>(y2 + ATLAS_SPRITES_PADDING);

                auto& vbuf = _spritesDrawBuf->Vertices2D;
                const auto pos = _curDrawQuad * 4;

                vbuf[pos + 0].TUEgg = x1_f / _eggAtlasWidth;
                vbuf[pos + 0].TVEgg = y2_f / _eggAtlasHeight;
                vbuf[pos + 1].TUEgg = x1_f / _eggAtlasWidth;
                vbuf[pos + 1].TVEgg = y1_f / _eggAtlasHeight;
                vbuf[pos + 2].TUEgg = x2_f / _eggAtlasWidth;
                vbuf[pos + 2].TVEgg = y1_f / _eggAtlasHeight;
                vbuf[pos + 3].TUEgg = x2_f / _eggAtlasWidth;
                vbuf[pos + 3].TVEgg = y2_f / _eggAtlasHeight;

                egg_added = true;
            }
        }

        // Choose effect
        auto* effect = spr->DrawEffect != nullptr ? *spr->DrawEffect : nullptr;
        if (effect == nullptr) {
            effect = si->DrawEffect != nullptr ? si->DrawEffect : _effectMngr.Effects.Generic;
        }

        // Choose atlas
        if (_dipQueue.empty() || _dipQueue.back().MainTex != si->Atlas->MainTex || _dipQueue.back().SourceEffect != effect) {
            _dipQueue.push_back(DipData {si->Atlas->MainTex, effect, 1});
        }
        else {
            _dipQueue.back().SpritesCount++;
        }

        // Casts
        const auto xf = static_cast<float>(x) / zoom + (prerender ? static_cast<float>(prerender_ox) : 0.0f);
        const auto yf = static_cast<float>(y) / zoom + (prerender ? static_cast<float>(prerender_oy) : 0.0f);
        const auto wf = static_cast<float>(si->Width) / zoom;
        const auto hf = static_cast<float>(si->Height) / zoom;

        // Fill buffer
        auto& vbuf = _spritesDrawBuf->Vertices2D;
        auto pos = _curDrawQuad * 4;

        vbuf[pos].X = xf;
        vbuf[pos].Y = yf + hf;
        vbuf[pos].TU = si->SprRect.Left;
        vbuf[pos].TV = si->SprRect.Bottom;
        vbuf[pos++].Diffuse = color_l;

        vbuf[pos].X = xf;
        vbuf[pos].Y = yf;
        vbuf[pos].TU = si->SprRect.Left;
        vbuf[pos].TV = si->SprRect.Top;
        vbuf[pos++].Diffuse = color_l;

        vbuf[pos].X = xf + wf;
        vbuf[pos].Y = yf;
        vbuf[pos].TU = si->SprRect.Right;
        vbuf[pos].TV = si->SprRect.Top;
        vbuf[pos++].Diffuse = color_r;

        vbuf[pos].X = xf + wf;
        vbuf[pos].Y = yf + hf;
        vbuf[pos].TU = si->SprRect.Right;
        vbuf[pos].TV = si->SprRect.Bottom;
        vbuf[pos++].Diffuse = color_r;

        // Set default texture coordinates for egg texture
        if (!egg_added && vbuf[pos - 1].TUEgg != -1.0f) {
            vbuf[pos - 1].TUEgg = -1.0f;
            vbuf[pos - 2].TUEgg = -1.0f;
            vbuf[pos - 3].TUEgg = -1.0f;
            vbuf[pos - 4].TUEgg = -1.0f;
        }

        // Draw
        if (++_curDrawQuad == _drawQuadCount) {
            Flush();
        }

        // Corners indication
        if (_settings.ShowCorners && spr->EggType != 0) {
            vector<PrimitivePoint> corner;
            const auto cx = wf / 2.0f;

            switch (spr->EggType) {
            case EGG_ALWAYS:
                PrepareSquare(corner, FRect(xf + cx - 2.0f, yf + hf - 50.0f, xf + cx + 2.0f, yf + hf), 0x5FFFFF00);
                break;
            case EGG_X:
                PrepareSquare(corner, FPoint(xf + cx - 5.0f, yf + hf - 55.0f), FPoint(xf + cx + 5.0f, yf + hf - 45.0f), FPoint(xf + cx - 5.0f, yf + hf - 5.0f), FPoint(xf + cx + 5.0f, yf + hf + 5.0f), 0x5F00AF00);
                break;
            case EGG_Y:
                PrepareSquare(corner, FPoint(xf + cx - 5.0f, yf + hf - 49.0f), FPoint(xf + cx + 5.0f, yf + hf - 52.0f), FPoint(xf + cx - 5.0f, yf + hf + 1.0f), FPoint(xf + cx + 5.0f, yf + hf - 2.0f), 0x5F00FF00);
                break;
            case EGG_X_AND_Y:
                PrepareSquare(corner, FPoint(xf + cx - 10.0f, yf + hf - 49.0f), FPoint(xf + cx, yf + hf - 52.0f), FPoint(xf + cx - 10.0f, yf + hf + 1.0f), FPoint(xf + cx, yf + hf - 2.0f), 0x5FFF0000);
                PrepareSquare(corner, FPoint(xf + cx, yf + hf - 55.0f), FPoint(xf + cx + 10.0f, yf + hf - 45.0f), FPoint(xf + cx, yf + hf - 5.0f), FPoint(xf + cx + 10.0f, yf + hf + 5.0f), 0x5FFF0000);
                break;
            case EGG_X_OR_Y:
                PrepareSquare(corner, FPoint(xf + cx, yf + hf - 49.0f), FPoint(xf + cx + 10.0f, yf + hf - 52.0f), FPoint(xf + cx, yf + hf + 1.0f), FPoint(xf + cx + 10.0f, yf + hf - 2.0f), 0x5FAF0000);
                PrepareSquare(corner, FPoint(xf + cx - 10.0f, yf + hf - 55.0f), FPoint(xf + cx, yf + hf - 45.0f), FPoint(xf + cx - 10.0f, yf + hf - 5.0f), FPoint(xf + cx, yf + hf + 5.0f), 0x5FAF0000);
            default:
                break;
            }

            DrawPoints(corner, RenderPrimitiveType::TriangleList, nullptr, nullptr, nullptr);
        }

        // Draw order
        if (_settings.ShowDrawOrder) {
            const auto x1 = static_cast<int>(static_cast<float>(spr->ScrX + _settings.ScrOx) / zoom);
            auto y1 = static_cast<int>(static_cast<float>(spr->ScrY + _settings.ScrOy) / zoom);

            if (spr->DrawOrderType >= DRAW_ORDER_FLAT && spr->DrawOrderType < DRAW_ORDER) {
                y1 -= static_cast<int>(40.0f / zoom);
            }

            DrawStr(IRect(x1, y1, x1 + 100, y1 + 100), _str("{}", spr->TreeIndex), 0, 0, 0);
        }

        // Process contour effect
        if (collect_contours && spr->ContourType != 0) {
            CollectContour(x, y, si, spr);
        }
    }

    Flush();

    if (_settings.ShowSpriteBorders) {
        DrawPoints(borders, RenderPrimitiveType::TriangleList, nullptr, nullptr, nullptr);
    }
}

auto SpriteManager::IsPixNoTransp(uint spr_id, int offs_x, int offs_y, bool with_zoom) const -> bool
{
    const auto color = GetPixColor(spr_id, offs_x, offs_y, with_zoom);
    return (color & 0xFF000000) != 0;
}

auto SpriteManager::GetPixColor(uint spr_id, int offs_x, int offs_y, bool with_zoom) const -> uint
{
    if (offs_x < 0 || offs_y < 0) {
        return 0;
    }

    const auto* si = _sprData[spr_id];
    if (si == nullptr) {
        return 0;
    }

    // 2d animation
    if (with_zoom && (static_cast<float>(offs_x) > static_cast<float>(si->Width) / _settings.SpritesZoom || static_cast<float>(offs_y) > static_cast<float>(si->Height) / _settings.SpritesZoom)) {
        return 0;
    }
    if (!with_zoom && (offs_x > si->Width || offs_y > si->Height)) {
        return 0;
    }

    if (with_zoom) {
        offs_x = static_cast<int>(static_cast<float>(offs_x) * _settings.SpritesZoom);
        offs_y = static_cast<int>(static_cast<float>(offs_y) * _settings.SpritesZoom);
    }

    offs_x += static_cast<int>(si->Atlas->MainTex->SizeData[0] * si->SprRect.Left);
    offs_y += static_cast<int>(si->Atlas->MainTex->SizeData[1] * si->SprRect.Top);
    return GetRenderTargetPixel(si->Atlas->RT, offs_x, offs_y);
}

auto SpriteManager::IsEggTransp(int pix_x, int pix_y) const -> bool
{
    if (!_eggValid) {
        return false;
    }

    const auto ex = _eggX + _settings.ScrOx;
    const auto ey = _eggY + _settings.ScrOy;
    auto ox = pix_x - static_cast<int>(static_cast<float>(ex) / _settings.SpritesZoom);
    auto oy = pix_y - static_cast<int>(static_cast<float>(ey) / _settings.SpritesZoom);

    if (ox < 0 || oy < 0 || ox >= static_cast<int>(static_cast<float>(_eggSprWidth) / _settings.SpritesZoom) || oy >= static_cast<int>(static_cast<float>(_eggSprHeight) / _settings.SpritesZoom)) {
        return false;
    }

    ox = static_cast<int>(static_cast<float>(ox) * _settings.SpritesZoom);
    oy = static_cast<int>(static_cast<float>(oy) * _settings.SpritesZoom);

    const auto egg_color = _eggData.at(oy * _eggSprWidth + ox);
    return (egg_color >> 24) < 127;
}

void SpriteManager::DrawPoints(vector<PrimitivePoint>& points, RenderPrimitiveType prim, const float* zoom, FPoint* offset, RenderEffect* custom_effect)
{
    if (points.empty()) {
        return;
    }

    Flush();

    auto* effect = custom_effect != nullptr ? custom_effect : _effectMngr.Effects.Primitive;

    // Check primitives
    const auto count = static_cast<uint>(points.size());
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
        [[fallthrough]];
    case RenderPrimitiveType::TriangleFan:
        prim_count -= 2;
        break;
    }
    if (prim_count <= 0) {
        return;
    }

    // Resize buffers
    if (_primitiveDrawBuf->Vertices2D.size() < count) {
        _primitiveDrawBuf->Vertices2D.resize(count);
    }

    // Collect data
    for (uint i = 0; i < count; i++) {
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

        std::memset(&_primitiveDrawBuf->Vertices2D[i], 0, sizeof(Vertex2D));
        _primitiveDrawBuf->Vertices2D[i].X = x;
        _primitiveDrawBuf->Vertices2D[i].Y = y;
        _primitiveDrawBuf->Vertices2D[i].Diffuse = COLOR_SWAP_RB(point.PointColor);
    }

    _primitiveDrawBuf->DataChanged = true;
    _primitiveDrawBuf->PrimType = prim;

    effect->DrawBuffer(_primitiveDrawBuf);
}

void SpriteManager::DrawContours()
{
    if (_contoursAdded && _rtContours != nullptr && _rtContoursMid != nullptr) {
        // Draw collected contours
        DrawRenderTarget(_rtContours, true, nullptr, nullptr);

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

void SpriteManager::CollectContour(int x, int y, const SpriteInfo* si, const Sprite* spr)
{
    if (_rtContours == nullptr || _rtContoursMid == nullptr || _effectMngr.Effects.Contour == nullptr) {
        return;
    }

    auto borders = IRect(x - 1, y - 1, x + si->Width + 1, y + si->Height + 1);
    const auto* texture = si->Atlas->MainTex;
    FRect textureuv;
    FRect sprite_border;

    const auto zoomed_screen_width = static_cast<int>(static_cast<float>(_settings.ScreenWidth) * _settings.SpritesZoom);
    const auto zoomed_screen_height = static_cast<int>(static_cast<float>(_settings.ScreenHeight - _settings.ScreenHudHeight) * _settings.SpritesZoom);
    if (borders.Left >= zoomed_screen_width || borders.Right < 0 || borders.Top >= zoomed_screen_height || borders.Bottom < 0) {
        return;
    }

    if (_settings.SpritesZoom == 1.0f) {
#if FO_ENABLE_3D
        if (si->Model != nullptr) {
            const auto& sr = si->SprRect;
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
            const auto& sr = si->SprRect;
            const float txw = texture->SizeData[2];
            const float txh = texture->SizeData[3];
            textureuv = FRect(sr.Left - txw, sr.Top - txh, sr.Right + txw, sr.Bottom + txh);
            sprite_border = sr;
        }
    }
    else {
        const auto& sr = si->SprRect;
        const auto zoomed_x = static_cast<int>(static_cast<float>(x) / _settings.SpritesZoom);
        const auto zoomed_y = static_cast<int>(static_cast<float>(y) / _settings.SpritesZoom);
        const auto zoomed_x2 = static_cast<int>(static_cast<float>(x + si->Width) / _settings.SpritesZoom);
        const auto zoomed_y2 = static_cast<int>(static_cast<float>(y + si->Height) / _settings.SpritesZoom);
        borders = {zoomed_x, zoomed_y, zoomed_x2, zoomed_y2};
        const auto bordersf = FRect(borders);
        const auto mid_height = _rtContoursMid->MainTex->SizeData[1];

        PushRenderTarget(_rtContoursMid);
        _contourClearMid = true;

        auto& vbuf = _flushDrawBuf->Vertices2D;
        auto pos = 0;

        vbuf[pos].X = bordersf.Left;
        vbuf[pos].Y = mid_height - bordersf.Bottom;
        vbuf[pos].TU = sr.Left;
        vbuf[pos++].TV = sr.Bottom;

        vbuf[pos].X = bordersf.Left;
        vbuf[pos].Y = mid_height - bordersf.Top;
        vbuf[pos].TU = sr.Left;
        vbuf[pos++].TV = sr.Top;

        vbuf[pos].X = bordersf.Right;
        vbuf[pos].Y = mid_height - bordersf.Top;
        vbuf[pos].TU = sr.Right;
        vbuf[pos++].TV = sr.Top;

        vbuf[pos].X = bordersf.Right;
        vbuf[pos].Y = mid_height - bordersf.Bottom;
        vbuf[pos].TU = sr.Right;
        vbuf[pos].TV = sr.Bottom;

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

    uint contour_color;
    if (spr->ContourType == CONTOUR_RED) {
        contour_color = 0xFFAF0000;
    }
    else if (spr->ContourType == CONTOUR_YELLOW) {
        contour_color = 0x00AFAF00; // Disable flashing by passing alpha == 0.0
    }
    else if (spr->ContourType == CONTOUR_CUSTOM) {
        contour_color = spr->ContourColor;
    }
    else {
        contour_color = 0xFFAFAFAF;
    }
    contour_color = COLOR_SWAP_RB(contour_color);

    const auto bordersf = FRect(borders);

    PushRenderTarget(_rtContours);

    auto& vbuf = _contourDrawBuf->Vertices2D;
    auto pos = 0;

    vbuf[pos].X = bordersf.Left;
    vbuf[pos].Y = bordersf.Bottom;
    vbuf[pos].TU = textureuv.Left;
    vbuf[pos].TV = textureuv.Bottom;
    vbuf[pos++].Diffuse = contour_color;

    vbuf[pos].X = bordersf.Left;
    vbuf[pos].Y = bordersf.Top;
    vbuf[pos].TU = textureuv.Left;
    vbuf[pos].TV = textureuv.Top;
    vbuf[pos++].Diffuse = contour_color;

    vbuf[pos].X = bordersf.Right;
    vbuf[pos].Y = bordersf.Top;
    vbuf[pos].TU = textureuv.Right;
    vbuf[pos].TV = textureuv.Top;
    vbuf[pos++].Diffuse = contour_color;

    vbuf[pos].X = bordersf.Right;
    vbuf[pos].Y = bordersf.Bottom;
    vbuf[pos].TU = textureuv.Right;
    vbuf[pos].TV = textureuv.Bottom;
    vbuf[pos].Diffuse = contour_color;

    auto& border_buf = _effectMngr.Effects.Contour->BorderBuf->SpriteBorder;
    border_buf[0] = sprite_border[0];
    border_buf[1] = sprite_border[1];
    border_buf[2] = sprite_border[2];
    border_buf[3] = sprite_border[3];

    _effectMngr.Effects.Contour->DrawBuffer(_contourDrawBuf);

    PopRenderTarget();
    _contoursAdded = true;
}

auto SpriteManager::GetFont(int num) -> FontData*
{
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
    _allFonts.clear();
}

void SpriteManager::SetDefaultFont(int index, uint color)
{
    _defFontIndex = index;
    _defFontColor = color;
}

void SpriteManager::SetFontEffect(int index, RenderEffect* effect)
{
    auto* font = GetFont(index);
    if (font != nullptr) {
        font->DrawEffect = effect != nullptr ? effect : _effectMngr.Effects.Font;
    }
}

void SpriteManager::BuildFonts()
{
    for (size_t i = 0; i < _allFonts.size(); i++) {
        if (_allFonts[i] && !_allFonts[i]->Builded) {
            BuildFont(static_cast<int>(i));
        }
    }
}

void SpriteManager::BuildFont(int index)
{
#define PIXEL_AT(tex_data, width, x, y) (*((uint*)(tex_data).data() + (y) * (width) + (x)))

    auto& font = *_allFonts[index];
    font.Builded = true;

    // Fix texture coordinates
    const auto* si = GetSpriteInfo(font.ImageNormal->GetSprId(0));
    auto tex_w = static_cast<float>(si->Atlas->Width);
    auto tex_h = static_cast<float>(si->Atlas->Height);
    auto image_x = tex_w * si->SprRect.Left;
    auto image_y = tex_h * si->SprRect.Top;
    auto max_h = 0;
    for (auto& [index, letter] : font.Letters) {
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
    font.FontTex = si->Atlas->MainTex;
    if (font.LineHeight == 0) {
        font.LineHeight = max_h;
    }
    if (font.Letters.count(' ') != 0u) {
        font.SpaceWidth = font.Letters[' '].XAdvance;
    }

    const auto* si_bordered = (font.ImageBordered != nullptr ? GetSpriteInfo(font.ImageBordered->GetSprId(0)) : nullptr);
    font.FontTexBordered = si_bordered != nullptr ? si_bordered->Atlas->MainTex : nullptr;

    const auto normal_ox = static_cast<int>(tex_w * si->SprRect.Left);
    const auto normal_oy = static_cast<int>(tex_h * si->SprRect.Top);
    const auto bordered_ox = (si_bordered != nullptr ? static_cast<int>(static_cast<float>(si_bordered->Atlas->Width) * si_bordered->SprRect.Left) : 0);
    const auto bordered_oy = (si_bordered != nullptr ? static_cast<int>(static_cast<float>(si_bordered->Atlas->Height) * si_bordered->SprRect.Top) : 0);

    // Read texture data
    auto data_normal = si->Atlas->MainTex->GetTextureRegion(normal_ox, normal_oy, si->Width, si->Height);

    vector<uint> data_bordered;
    if (si_bordered != nullptr) {
        data_bordered = si_bordered->Atlas->MainTex->GetTextureRegion(bordered_ox, bordered_oy, si_bordered->Width, si_bordered->Height);
    }

    // Normalize color to gray
    if (font.MakeGray) {
        for (uint y = 0; y < static_cast<uint>(si->Height); y++) {
            for (uint x = 0; x < static_cast<uint>(si->Width); x++) {
                const auto a = reinterpret_cast<uchar*>(&PIXEL_AT(data_normal, si->Width, x, y))[3];
                if (a != 0u) {
                    PIXEL_AT(data_normal, si->Width, x, y) = COLOR_RGBA(a, 128, 128, 128);
                    if (si_bordered != nullptr) {
                        PIXEL_AT(data_bordered, si_bordered->Width, x, y) = COLOR_RGBA(a, 128, 128, 128);
                    }
                }
                else {
                    PIXEL_AT(data_normal, si->Width, x, y) = COLOR_RGBA(0, 0, 0, 0);
                    if (si_bordered != nullptr) {
                        PIXEL_AT(data_bordered, si_bordered->Width, x, y) = COLOR_RGBA(0, 0, 0, 0);
                    }
                }
            }
        }

        const auto r = IRect(normal_ox, normal_oy, normal_ox + si->Width - 1, normal_oy + si->Height - 1);
        si->Atlas->MainTex->UpdateTextureRegion(r, data_normal.data());
    }

    // Fill border
    if (si_bordered != nullptr) {
        for (uint y = 1; y < static_cast<uint>(si_bordered->Height) - 2; y++) {
            for (uint x = 1; x < static_cast<uint>(si_bordered->Width) - 2; x++) {
                if (PIXEL_AT(data_normal, si->Width, x, y)) {
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

        const auto r_bordered = IRect(bordered_ox, bordered_oy, bordered_ox + si_bordered->Width - 1, bordered_oy + si_bordered->Height - 1);
        si_bordered->Atlas->MainTex->UpdateTextureRegion(r_bordered, data_bordered.data());

        // Fix texture coordinates on bordered texture
        tex_w = static_cast<float>(si_bordered->Atlas->Width);
        tex_h = static_cast<float>(si_bordered->Atlas->Height);
        image_x = tex_w * si_bordered->SprRect.Left;
        image_y = tex_h * si_bordered->SprRect.Top;
        for (auto&& [index, letter] : font.Letters) {
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

auto SpriteManager::LoadFontFO(int index, string_view font_name, bool not_bordered, bool skip_if_loaded /* = true */) -> bool
{
    // Skip if loaded
    if (skip_if_loaded && index < static_cast<int>(_allFonts.size()) && _allFonts[index]) {
        return true;
    }

    // Load font data
    string fname = _str("Fonts/{}.fofnt", font_name);
    auto file = _fileSys.ReadFile(fname);
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
    auto* image_normal = LoadAnimation(image_name, false);
    if (image_normal == nullptr) {
        WriteLog("Image file '{}' not found", image_name);
        return false;
    }
    font.ImageNormal = image_normal;

    // Create bordered instance
    if (!not_bordered) {
        auto* image_bordered = LoadAnimation(image_name, false);
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

static constexpr auto MAKEUINT(uchar ch0, uchar ch1, uchar ch2, uchar ch3) -> uint
{
    return ch0 | ch1 << 8 | ch2 << 16 | ch3 << 24;
}

auto SpriteManager::LoadFontBmf(int index, string_view font_name) -> bool
{
    if (index < 0) {
        WriteLog("Invalid index");
        return false;
    }

    FontData font {_effectMngr.Effects.Font};
    auto file = _fileSys.ReadFile(_str("Fonts/{}.fnt", font_name));
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
    const auto count = file.GetLEUInt() / 20u;
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
        let.PosX = static_cast<short>(x + 1);
        let.PosY = static_cast<short>(y + 1);
        let.Width = static_cast<short>(w - 2);
        let.Height = static_cast<short>(h - 2);
        let.OffsX = static_cast<short>(-static_cast<int>(ox));
        let.OffsY = static_cast<short>(-static_cast<int>(oy) + (line_height - base_height));
        let.XAdvance = static_cast<short>(xa + 1);
    }

    font.LineHeight = font.Letters.count('W') != 0u ? font.Letters['W'].Height : base_height;
    font.YAdvance = font.LineHeight / 2;
    font.MakeGray = true;

    // Load image
    auto* image_normal = LoadAnimation(image_name, false);
    if (image_normal == nullptr) {
        WriteLog("Image file '{}' not found", image_name);
        return false;
    }
    font.ImageNormal = image_normal;

    // Create bordered instance
    auto* image_bordered = LoadAnimation(image_name, false);
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
    RUNTIME_ASSERT(to);
    RUNTIME_ASSERT(size > 0);

    if (from.length() == 0u) {
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
    return StrCopy(to, Size, from);
}

static void StrGoTo(char*& str, char ch, bool skip_char)
{
    while (*str != 0 && *str != ch) {
        ++str;
    }
    if (skip_char && *str != 0) {
        ++str;
    }
}

static void StrEraseInterval(char* str, uint len)
{
    if (str == nullptr || len == 0u) {
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
    if (to == nullptr || from == nullptr) {
        return;
    }

    if (from_len == 0u) {
        from_len = static_cast<uint>(string_view(from).length());
    }
    if (from_len == 0u) {
        return;
    }

    auto* end_to = to;
    while (*end_to != 0) {
        ++end_to;
    }

    for (; end_to >= to; --end_to) {
        *(end_to + from_len) = *end_to;
    }

    while (from_len-- != 0u) {
        *to = *from;
        ++to;
        ++from;
    }
}

void SpriteManager::FormatText(FontFormatInfo& fi, int fmt_type)
{
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
        StrGoTo(str_, '|', true);
        auto* s1 = str_;
        StrGoTo(str_, ' ', true);
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
        i_advance = letter_len;

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
                letter = str[i];
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

                if (IsBitSet(flags, FT_ALIGN) && skip_line == 0u) {
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
            if (skip_line == 0u) {
                cury += font->LineHeight + font->YAdvance;
                if (!infinity_h && cury + font->LineHeight > r.Bottom && fi.LinesInRect == 0u) {
                    fi.LinesInRect = fi.LinesAll;
                }

                if (fmt_type == FORMAT_TYPE_DRAW) {
                    if (fi.LinesInRect != 0u && !IsBitSet(flags, FT_UPPER)) {
                        // fi.LinesAll++;
                        str[i] = 0;
                        break;
                    }
                }
                else if (fmt_type == FORMAT_TYPE_SPLIT) {
                    if (fi.LinesInRect != 0u && fi.LinesAll % fi.LinesInRect == 0u) {
                        str[i] = 0;
                        (*fi.StrLines).push_back(str);
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

    if (skip_line_end != 0u) {
        auto len = static_cast<int>(string_view(str).length());
        for (auto i = len - 2; i >= 0; i--) {
            if (str[i] == '\n') {
                str[i] = 0;
                fi.LinesAll--;
                if (--skip_line_end == 0u) {
                    break;
                }
            }
        }

        if (skip_line_end != 0u) {
            WriteLog("error3");
            fi.IsError = true;
            return;
        }
    }

    if (skip_line != 0u) {
        WriteLog("error4");
        fi.IsError = true;
        return;
    }

    if (fi.LinesInRect == 0u) {
        fi.LinesInRect = fi.LinesAll;
    }

    if (fi.LinesAll > FONT_MAX_LINES) {
        WriteLog("error5 {}", fi.LinesAll);
        fi.IsError = true;
        return;
    }

    if (fmt_type == FORMAT_TYPE_SPLIT) {
        (*fi.StrLines).push_back(string(str));
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

            if (fi.ColorDots[j] != 0u) {
                last_col = fi.ColorDots[j];
            }
        }

        if (!IsBitSet(flags, FT_NO_COLORIZE)) {
            offs_col += j + 1;
            if (last_col != 0u && fi.ColorDots[j + 1] == 0u) {
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
        fi.LineWidth[i] = static_cast<short>(curx);
    }

    auto can_count = false;
    auto spaces = 0;
    auto curstr = 0;
    for (auto i = 0, i_advance = 1;; i += i_advance) {
        uint letter_len = 0;
        auto letter = utf8::Decode(&str[i], &letter_len);
        i_advance = letter_len;

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
            fi.LineWidth[curstr] = static_cast<short>(curx);
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
        fi.CurY = r.Top + static_cast<int>(r.Bottom - r.Top + 1 - fi.LinesInRect * font->LineHeight - (fi.LinesInRect - 1) * font->YAdvance) / 2 + (IsBitSet(flags, FT_CENTERY_ENGINE) ? 1 : 0);
    }
    else if (IsBitSet(flags, FT_BOTTOM)) {
        fi.CurY = r.Bottom - static_cast<int>(fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance);
    }
}

void SpriteManager::DrawStr(const IRect& r, string_view str, uint flags, uint color /* = 0 */, int num_font /* = -1 */)
{
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
    if (color == 0u && _defFontColor != 0u) {
        color = _defFontColor;
    }

    auto& fi = _fontFormatInfoBuf = {font, flags, r};
    StrCopy(fi.Str, str);
    fi.DefColor = color;
    FormatText(fi, FORMAT_TYPE_DRAW);
    if (fi.IsError) {
        return;
    }

    color = COLOR_SWAP_RB(color);

    auto* str_ = fi.PStr;
    const auto offs_col = fi.OffsColDots;
    auto curx = fi.CurX;
    auto cury = fi.CurY;
    auto curstr = 0;
    auto* texture = IsBitSet(flags, FT_BORDERED) && font->FontTexBordered != nullptr ? font->FontTexBordered : font->FontTex;

    if (_curDrawQuad != 0) {
        Flush();
    }

    if (!IsBitSet(flags, FT_NO_COLORIZE)) {
        for (auto i = offs_col; i >= 0; i--) {
            if (fi.ColorDots[i] != 0u) {
                if ((fi.ColorDots[i] & 0xFF000000) != 0u) {
                    color = fi.ColorDots[i]; // With alpha
                }
                else {
                    color = (color & 0xFF000000) | (fi.ColorDots[i] & 0x00FFFFFF); // Still old alpha
                }
                color = COLOR_SWAP_RB(color);
                break;
            }
        }
    }

    auto variable_space = false;
    int i_advance;
    for (auto i = 0; str_[i] != 0; i += i_advance) {
        if (!IsBitSet(flags, FT_NO_COLORIZE)) {
            const auto new_color = fi.ColorDots[i + offs_col];
            if (new_color != 0u) {
                if ((new_color & 0xFF000000) != 0u) {
                    color = new_color; // With alpha
                }
                else {
                    color = (color & 0xFF000000) | (new_color & 0x00FFFFFF); // Still old alpha
                }
                color = COLOR_SWAP_RB(color);
            }
        }

        uint letter_len = 0;
        auto letter = utf8::Decode(&str_[i], &letter_len);
        i_advance = letter_len;

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

            auto& vbuf = _spritesDrawBuf->Vertices2D;
            auto pos = _curDrawQuad * 4;

            vbuf[pos].X = static_cast<float>(x);
            vbuf[pos].Y = static_cast<float>(y + h);
            vbuf[pos].TU = x1;
            vbuf[pos].TV = y2;
            vbuf[pos++].Diffuse = color;

            vbuf[pos].X = static_cast<float>(x);
            vbuf[pos].Y = static_cast<float>(y);
            vbuf[pos].TU = x1;
            vbuf[pos].TV = y1;
            vbuf[pos++].Diffuse = color;

            vbuf[pos].X = static_cast<float>(x + w);
            vbuf[pos].Y = static_cast<float>(y);
            vbuf[pos].TU = x2;
            vbuf[pos].TV = y1;
            vbuf[pos++].Diffuse = color;

            vbuf[pos].X = static_cast<float>(x + w);
            vbuf[pos].Y = static_cast<float>(y + h);
            vbuf[pos].TU = x2;
            vbuf[pos].TV = y2;
            vbuf[pos].Diffuse = color;

            if (++_curDrawQuad == _drawQuadCount) {
                _dipQueue.push_back({texture, font->DrawEffect, 1});
                _dipQueue.back().SpritesCount = _curDrawQuad;
                Flush();
            }

            curx += l.XAdvance;
            variable_space = true;
        }
    }

    if (_curDrawQuad != 0) {
        _dipQueue.push_back({texture, font->DrawEffect, 1});
        _dipQueue.back().SpritesCount = _curDrawQuad;
        Flush();
    }
}

auto SpriteManager::GetLinesCount(int width, int height, string_view str, int num_font /* = -1 */) -> int
{
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
    if (width <= 0 || height <= 0) {
        return 0;
    }

    auto* font = GetFont(num_font);
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
    auto* font = GetFont(num_font);
    if (font == nullptr) {
        return 0;
    }

    return font->LineHeight;
}

auto SpriteManager::GetTextInfo(int width, int height, string_view str, int num_font, uint flags, int& tw, int& th, int& lines) -> bool
{
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
    auto* font = GetFont(num_font);
    if (font == nullptr) {
        return false;
    }
    return font->Letters.count(letter) > 0;
}
