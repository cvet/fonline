//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Testing.h"
#include "Timer.h"

constexpr int SPRITES_BUFFER_SIZE = 10000;
constexpr int ATLAS_SPRITES_PADDING = 1;

TextureAtlas::SpaceNode::SpaceNode(int x, int y, uint w, uint h) : PosX {x}, PosY {y}, Width {w}, Height {h}
{
}

bool TextureAtlas::SpaceNode::FindPosition(uint w, uint h, int& x, int& y)
{
    bool result = false;

    if (Child1)
        result = Child1->FindPosition(w, h, x, y);
    if (!result && Child2)
        result = Child2->FindPosition(w, h, x, y);

    if (!result && !Busy && Width >= w && Height >= h)
    {
        result = true;
        Busy = true;

        x = PosX;
        y = PosY;

        if (Width == w && Height > h)
        {
            Child1 = std::make_unique<SpaceNode>(PosX, PosY + h, Width, Height - h);
        }
        else if (Height == h && Width > w)
        {
            Child1 = std::make_unique<SpaceNode>(PosX + w, PosY, Width - w, Height);
        }
        else if (Width > w && Height > h)
        {
            Child1 = std::make_unique<SpaceNode>(PosX + w, PosY, Width - w, h);
            Child2 = std::make_unique<SpaceNode>(PosX, PosY + h, Width, Height - h);
        }
    }
    return result;
}

uint AnyFrames::GetSprId(uint num_frm)
{
    return Ind[num_frm % CntFrm];
}

short AnyFrames::GetNextX(uint num_frm)
{
    return NextX[num_frm % CntFrm];
}

short AnyFrames::GetNextY(uint num_frm)
{
    return NextY[num_frm % CntFrm];
}

uint AnyFrames::GetCurSprId()
{
    return CntFrm > 1 ? Ind[((Timer::GameTick() % Ticks) * 100 / Ticks) * CntFrm / 100] : Ind[0];
}

uint AnyFrames::GetCurSprIndex()
{
    return CntFrm > 1 ? ((Timer::GameTick() % Ticks) * 100 / Ticks) * CntFrm / 100 : 0;
}

AnyFrames* AnyFrames::GetDir(int dir)
{
    return dir == 0 || DirCount == 1 ? this : Dirs[dir - 1];
}

SpriteManager::SpriteManager(
    RenderSettings& sett, FileManager& file_mngr, EffectManager& effect_mngr, ClientScriptSystem& script_sys) :
    settings {sett}, fileMngr {file_mngr}, effectMngr {effect_mngr}
{
    baseColor = COLOR_RGBA(255, 128, 128, 128);
    drawQuadCount = 1024;
    allAtlases.reserve(100);
    dipQueue.reserve(1000);
    vBuffer.resize(drawQuadCount * 4);
    sprData.resize(SPRITES_BUFFER_SIZE);
    effectMngr.LoadMinimalEffects();

    MatrixHelper::MatrixOrtho(
        projectionMatrixCM[0], 0.0f, (float)settings.ScreenWidth, (float)settings.ScreenHeight, 0.0f, -1.0f, 1.0f);
    projectionMatrixCM.Transpose(); // Convert to column major order

    rtMain = CreateRenderTarget(false, false, true, 0, 0, true);
    rtContours = CreateRenderTarget(false, false, true, 0, 0, false);
    rtContoursMid = CreateRenderTarget(false, false, true, 0, 0, false);
    if (rtMain)
        PushRenderTarget(rtMain);

    DummyAnimation = new AnyFrames();
    DummyAnimation->CntFrm = 1;
    DummyAnimation->Ticks = 100;

    if (settings.Enable3dRendering)
    {
        anim3dMngr = std::make_unique<Animation3dManager>(
            settings, fileMngr, effectMngr, script_sys, [this](MeshTexture* mesh_tex) {
                PushAtlasType(AtlasType::MeshTextures);
                AnyFrames* anim = LoadAnimation(_str(mesh_tex->ModelPath).extractDir() + mesh_tex->Name);
                PopAtlasType();
                if (anim)
                {
                    SpriteInfo* si = GetSpriteInfo(anim->Ind[0]);
                    mesh_tex->MainTex = si->Atlas->MainTex;
                    mesh_tex->AtlasOffsetData[0] = si->SprRect[0];
                    mesh_tex->AtlasOffsetData[1] = si->SprRect[1];
                    mesh_tex->AtlasOffsetData[2] = si->SprRect[2] - si->SprRect[0];
                    mesh_tex->AtlasOffsetData[3] = si->SprRect[3] - si->SprRect[1];
                    DestroyAnyFrames(anim);
                }
            });
    }
}

SpriteManager::~SpriteManager()
{
    delete DummyAnimation;

    for (auto it = sprData.begin(), end = sprData.end(); it != end; ++it)
        delete *it;
}

void SpriteManager::GetWindowSize(int& w, int& h)
{
    App::Window::GetSize(w, h);
}

void SpriteManager::SetWindowSize(int w, int h)
{
    App::Window::SetSize(w, h);
}

void SpriteManager::GetWindowPosition(int& x, int& y)
{
    App::Window::GetPosition(x, y);
}

void SpriteManager::SetWindowPosition(int x, int y)
{
    App::Window::SetPosition(x, y);
}

void SpriteManager::GetMousePosition(int& x, int& y)
{
    App::Window::GetMousePosition(x, y);
}

void SpriteManager::SetMousePosition(int x, int y)
{
    App::Window::SetMousePosition(x, y);
}

bool SpriteManager::IsWindowFocused()
{
    return App::Window::IsFocused();
}

void SpriteManager::MinimizeWindow()
{
    App::Window::Minimize();
}

bool SpriteManager::EnableFullscreen()
{
    return App::Window::ToggleFullscreen(true);
}

bool SpriteManager::DisableFullscreen()
{
    return App::Window::ToggleFullscreen(false);
}

void SpriteManager::BlinkWindow()
{
    App::Window::Blink();
}

void SpriteManager::SetAlwaysOnTop(bool enable)
{
    App::Window::AlwaysOnTop(enable);
}

void SpriteManager::Preload3dModel(const string& model_name)
{
    RUNTIME_ASSERT(anim3dMngr);
    anim3dMngr->PreloadEntity(model_name);
}

void SpriteManager::BeginScene(uint clear_color)
{
    if (rtMain)
        PushRenderTarget(rtMain);

    // Render 3d animations
    if (settings.Enable3dRendering && !autoRedrawAnim3d.empty())
    {
        for (auto it = autoRedrawAnim3d.begin(), end = autoRedrawAnim3d.end(); it != end; ++it)
        {
            Animation3d* anim3d = *it;
            if (anim3d->NeedDraw())
                Render3d(anim3d);
        }
    }

    // Clear window
    if (clear_color)
        ClearCurrentRenderTarget(clear_color);
}

void SpriteManager::EndScene()
{
    Flush();

    if (rtMain)
    {
        PopRenderTarget();
        DrawRenderTarget(rtMain, false);
    }
}

void SpriteManager::OnResolutionChanged()
{
    // Resize fullscreen render targets
    for (auto& rt : rtAll)
    {
        if (!rt->ScreenSized)
            continue;

        rt->MainTex = nullptr; // Clean up previous resources first
        rt->MainTex = unique_ptr<RenderTexture>(App::Render::CreateTexture(settings.ScreenWidth, settings.ScreenHeight,
            rt->MainTex->LinearFiltered, rt->MainTex->Multisampled, rt->MainTex->WithDepth));
    }
}

RenderTarget* SpriteManager::CreateRenderTarget(
    bool with_depth, bool multisampled, bool screen_sized, uint width, uint height, bool linear_filtered)
{
    Flush();

    width = (screen_sized ? settings.ScreenWidth : width);
    height = (screen_sized ? settings.ScreenHeight : height);
    if (!effectMngr.Effects.FlushRenderTargetMS)
        multisampled = false;

    RenderTarget* rt = new RenderTarget();
    rt->ScreenSized = screen_sized;
    rt->MainTex =
        unique_ptr<RenderTexture>(App::Render::CreateTexture(width, height, linear_filtered, multisampled, with_depth));

    if (!multisampled)
        rt->DrawEffect = effectMngr.Effects.FlushRenderTarget;
    else
        rt->DrawEffect = effectMngr.Effects.FlushRenderTargetMS;

    RenderTexture* prev_tex = App::Render::GetRenderTarget();
    App::Render::SetRenderTarget(rt->MainTex.get());
    App::Render::ClearRenderTarget(0);
    if (with_depth)
        App::Render::ClearRenderTargetDepth();
    App::Render::SetRenderTarget(prev_tex);

    rtAll.push_back(unique_ptr<RenderTarget>(rt));
    return rt;
}

void SpriteManager::PushRenderTarget(RenderTarget* rt)
{
    Flush();

    bool redundant = (!rtStack.empty() && rtStack.back() == rt);
    rtStack.push_back(rt);
    if (!redundant)
    {
        Flush();
        App::Render::SetRenderTarget(rt->MainTex.get());
        rt->LastPixelPicks.clear();
    }
}

void SpriteManager::PopRenderTarget()
{
    bool redundant = (rtStack.size() > 2 && rtStack.back() == rtStack[rtStack.size() - 2]);
    rtStack.pop_back();

    if (!redundant)
    {
        Flush();

        if (!rtStack.empty())
            App::Render::SetRenderTarget(rtStack.back()->MainTex.get());
        else
            App::Render::SetRenderTarget(nullptr);
    }
}

void SpriteManager::DrawRenderTarget(RenderTarget* rt, bool alpha_blend, const Rect* region_from, const Rect* region_to)
{
    Flush();

    if (!region_from && !region_to)
    {
        float w = (float)rt->MainTex->Width;
        float h = (float)rt->MainTex->Height;
        uint pos = 0;
        vBuffer[pos].X = 0.0f;
        vBuffer[pos].Y = h;
        vBuffer[pos].TU = 0.0f;
        vBuffer[pos++].TV = 0.0f;
        vBuffer[pos].X = 0.0f;
        vBuffer[pos].Y = 0.0f;
        vBuffer[pos].TU = 0.0f;
        vBuffer[pos++].TV = 1.0f;
        vBuffer[pos].X = w;
        vBuffer[pos].Y = 0.0f;
        vBuffer[pos].TU = 1.0f;
        vBuffer[pos++].TV = 1.0f;
        vBuffer[pos].X = w;
        vBuffer[pos].Y = h;
        vBuffer[pos].TU = 1.0f;
        vBuffer[pos++].TV = 0.0f;
    }
    else
    {
        RectF regionf = (region_from ? *region_from : Rect(0, 0, rt->MainTex->Width, rt->MainTex->Height));
        RectF regiont =
            (region_to ? *region_to : Rect(0, 0, rtStack.back()->MainTex->Width, rtStack.back()->MainTex->Height));
        float wf = (float)rt->MainTex->Width;
        float hf = (float)rt->MainTex->Height;
        uint pos = 0;
        vBuffer[pos].X = regiont.L;
        vBuffer[pos].Y = regiont.B;
        vBuffer[pos].TU = regionf.L / wf;
        vBuffer[pos++].TV = 1.0f - regionf.B / hf;
        vBuffer[pos].X = regiont.L;
        vBuffer[pos].Y = regiont.T;
        vBuffer[pos].TU = regionf.L / wf;
        vBuffer[pos++].TV = 1.0f - regionf.T / hf;
        vBuffer[pos].X = regiont.R;
        vBuffer[pos].Y = regiont.T;
        vBuffer[pos].TU = regionf.R / wf;
        vBuffer[pos++].TV = 1.0f - regionf.T / hf;
        vBuffer[pos].X = regiont.R;
        vBuffer[pos].Y = regiont.B;
        vBuffer[pos].TU = regionf.R / wf;
        vBuffer[pos++].TV = 1.0f - regionf.B / hf;
    }

    curDrawQuad = 1;
    dipQueue.push_back({rt->MainTex.get(), rt->DrawEffect, 1});

    // rt->DrawEffect->DisableBlending = !alpha_blend;
    // if (!alpha_blend)
    //    GL(glDisable(GL_BLEND));
    // Flush();
    // if (!alpha_blend)
    //    GL(glEnable(GL_BLEND));
}

uint SpriteManager::GetRenderTargetPixel(RenderTarget* rt, int x, int y)
{
    // Try find in last picks
    for (auto& pix : rt->LastPixelPicks)
        if (std::get<0>(pix) && std::get<1>(pix))
            return std::get<2>(pix);

    // Read one pixel
    uint color = App::Render::GetTexturePixel(rt->MainTex.get(), x, y);

    // Refresh picks
    rt->LastPixelPicks.emplace(rt->LastPixelPicks.begin(), x, y, color);
    if (rt->LastPixelPicks.size() > MAX_STORED_PIXEL_PICKS)
        rt->LastPixelPicks.pop_back();

    return color;
}

void SpriteManager::ClearCurrentRenderTarget(uint color)
{
    App::Render::ClearRenderTarget(color);
}

void SpriteManager::ClearCurrentRenderTargetDepth()
{
    App::Render::ClearRenderTargetDepth();
}

void SpriteManager::PushScissor(int l, int t, int r, int b)
{
    Flush();
    scissorStack.push_back(l);
    scissorStack.push_back(t);
    scissorStack.push_back(r);
    scissorStack.push_back(b);
    RefreshScissor();
}

void SpriteManager::PopScissor()
{
    if (!scissorStack.empty())
    {
        Flush();
        scissorStack.resize(scissorStack.size() - 4);
        RefreshScissor();
    }
}

void SpriteManager::RefreshScissor()
{
    if (!scissorStack.empty())
    {
        scissorRect.L = scissorStack[0];
        scissorRect.T = scissorStack[1];
        scissorRect.R = scissorStack[2];
        scissorRect.B = scissorStack[3];
        for (size_t i = 4; i < scissorStack.size(); i += 4)
        {
            if (scissorStack[i + 0] > scissorRect.L)
                scissorRect.L = scissorStack[i + 0];
            if (scissorStack[i + 1] > scissorRect.T)
                scissorRect.T = scissorStack[i + 1];
            if (scissorStack[i + 2] < scissorRect.R)
                scissorRect.R = scissorStack[i + 2];
            if (scissorStack[i + 3] < scissorRect.B)
                scissorRect.B = scissorStack[i + 3];
        }
        if (scissorRect.L > scissorRect.R)
            scissorRect.L = scissorRect.R;
        if (scissorRect.T > scissorRect.B)
            scissorRect.T = scissorRect.B;
    }
}

void SpriteManager::EnableScissor()
{
    if (!scissorStack.empty() && !rtStack.empty() && rtStack.back() == rtMain)
    {
        int x = scissorRect.L;
        int y = rtStack.back()->MainTex->Height - scissorRect.B;
        uint w = scissorRect.R - scissorRect.L;
        uint h = scissorRect.B - scissorRect.T;
        App::Render::EnableScissor(x, y, w, h);
    }
}

void SpriteManager::DisableScissor()
{
    if (!scissorStack.empty() && !rtStack.empty() && rtStack.back() == rtMain)
        App::Render::DisableScissor();
}

void SpriteManager::PushAtlasType(AtlasType atlas_type, bool one_image)
{
    atlasStack.push_back({atlas_type, one_image});
}

void SpriteManager::PopAtlasType()
{
    atlasStack.pop_back();
}

void SpriteManager::AccumulateAtlasData()
{
    accumulatorActive = true;
}

void SpriteManager::FlushAccumulatedAtlasData()
{
    accumulatorActive = false;
    if (accumulatorSprInfo.empty())
        return;

    // Sort by size
    std::sort(accumulatorSprInfo.begin(), accumulatorSprInfo.end(),
        [](SpriteInfo* si1, SpriteInfo* si2) { return si1->Width * si1->Height > si2->Width * si2->Height; });

    for (auto it = accumulatorSprInfo.begin(), end = accumulatorSprInfo.end(); it != end; ++it)
        FillAtlas(*it);
    accumulatorSprInfo.clear();
}

bool SpriteManager::IsAccumulateAtlasActive()
{
    return accumulatorActive;
}

TextureAtlas* SpriteManager::CreateAtlas(int w, int h)
{
    auto atlas = std::make_unique<TextureAtlas>();
    atlas->Type = std::get<0>(atlasStack.back());

    if (!std::get<1>(atlasStack.back()))
    {
        w = App::Render::MaxAtlasWidth;
        h = App::Render::MaxAtlasHeight;
    }

    atlas->RT = CreateRenderTarget(false, false, false, w, h, true);
    atlas->RT->LastPixelPicks.reserve(MAX_STORED_PIXEL_PICKS);
    atlas->MainTex = atlas->RT->MainTex.get();
    atlas->Width = w;
    atlas->Height = h;
    atlas->RootNode = (accumulatorActive ? std::make_unique<TextureAtlas::SpaceNode>(0, 0, w, h) : nullptr);

    allAtlases.push_back(std::move(atlas));
    return allAtlases.back().get();
}

TextureAtlas* SpriteManager::FindAtlasPlace(SpriteInfo* si, int& x, int& y)
{
    // Find place in already created atlas
    TextureAtlas* atlas = nullptr;
    AtlasType atlas_type = std::get<0>(atlasStack.back());
    uint w = si->Width + ATLAS_SPRITES_PADDING * 2;
    uint h = si->Height + ATLAS_SPRITES_PADDING * 2;
    for (auto& a : allAtlases)
    {
        if (a->Type != atlas_type)
            continue;

        if (a->RootNode)
        {
            if (a->RootNode->FindPosition(w, h, x, y))
                atlas = a.get();
        }
        else
        {
            if (w <= a->LineW && a->LineCurH + h <= a->LineMaxH)
            {
                x = a->CurX - a->LineW;
                y = a->CurY + a->LineCurH;
                a->LineCurH += h;
                atlas = a.get();
            }
            else if (a->Width - a->CurX >= w && a->Height - a->CurY >= h)
            {
                x = a->CurX;
                y = a->CurY;
                a->CurX += w;
                a->LineW = w;
                a->LineCurH = h;
                a->LineMaxH = MAX(a->LineMaxH, h);
                atlas = a.get();
            }
            else if (a->Width >= w && a->Height - a->CurY - a->LineMaxH >= h)
            {
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

        if (atlas)
            break;
    }

    // Create new
    if (!atlas)
    {
        atlas = CreateAtlas(w, h);
        if (atlas->RootNode)
        {
            atlas->RootNode->FindPosition(w, h, x, y);
        }
        else
        {
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
    for (auto it = allAtlases.begin(); it != allAtlases.end();)
    {
        auto& atlas = *it;
        if (atlas->Type == atlas_type)
        {
            for (auto it_ = sprData.begin(), end_ = sprData.end(); it_ != end_; ++it_)
            {
                SpriteInfo* si = *it_;
                if (si && si->Atlas == atlas.get())
                {
                    if (si->Anim3d)
                        si->Anim3d->SprId = 0;

                    delete si;
                    (*it_) = nullptr;
                }
            }

            it = allAtlases.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SpriteManager::DumpAtlases()
{
    uint atlases_memory_size = 0;
    for (auto& atlas : allAtlases)
        atlases_memory_size += atlas->Width * atlas->Height * 4;

    string path = _str(
        "./{}_{}.{:03}mb/", (uint)time(nullptr), atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000);

    int cnt = 0;
    for (auto& atlas : allAtlases)
    {
        string fname = _str("{}{}_{}_{}x{}.png", path, cnt, atlas->Type, atlas->Width, atlas->Height);
        // Todo: restore texture saving
        // SaveTexture(atlas->MainTex, fname, false);
        cnt++;
    }
}

uint SpriteManager::RequestFillAtlas(SpriteInfo* si, uint w, uint h, uchar* data)
{
    // Sprite info
    if (!si)
        si = new SpriteInfo();

    // Get width, height
    si->Data = data;
    si->DataAtlasType = std::get<0>(atlasStack.back());
    si->DataAtlasOneImage = std::get<1>(atlasStack.back());
    si->Width = w;
    si->Height = h;

    // Find place on atlas
    if (accumulatorActive)
        accumulatorSprInfo.push_back(si);
    else
        FillAtlas(si);

    // Store sprite
    uint index = 1;
    for (uint j = (uint)sprData.size(); index < j; index++)
        if (!sprData[index])
            break;
    if (index < (uint)sprData.size())
        sprData[index] = si;
    else
        sprData.push_back(si);
    return index;
}

void SpriteManager::FillAtlas(SpriteInfo* si)
{
    uint* data = (uint*)si->Data;
    uint w = si->Width;
    uint h = si->Height;

    si->Data = nullptr;

    PushAtlasType(si->DataAtlasType, si->DataAtlasOneImage);
    int x, y;
    TextureAtlas* atlas = FindAtlasPlace(si, x, y);
    PopAtlasType();

    // Refresh texture
    if (data)
    {
        // Whole image
        RenderTexture* tex = atlas->MainTex;
        App::Render::UpdateTextureRegion(tex, Rect(x, y, x + w - 1, y + h - 1), data);

        // 1px border for correct linear interpolation
        // Top
        App::Render::UpdateTextureRegion(tex, Rect(x, y - 1, x + w - 1, y - 1), data);

        // Bottom
        App::Render::UpdateTextureRegion(tex, Rect(x, y + h, x + w - 1, y + h), data + (h - 1) * w);

        // Left
        uint data_border[App::Render::MAX_ATLAS_SIZE];
        for (uint i = 0; i < h; i++)
            data_border[i + 1] = *(uint*)(data + i * w * 4);
        data_border[0] = data_border[1];
        data_border[h + 1] = data_border[h];
        App::Render::UpdateTextureRegion(tex, Rect(x - 1, y - 1, x - 1, y + h), data_border);

        // Right
        for (uint i = 0; i < h; i++)
            data_border[i + 1] = *(uint*)(data + i * w * 4 + (w - 1) * 4);
        data_border[0] = data_border[1];
        data_border[h + 1] = data_border[h];
        App::Render::UpdateTextureRegion(tex, Rect(x + w, y - 1, x + w, y + h), data_border);

        // Invalidate last pixel color picking
        if (!atlas->RT->LastPixelPicks.empty())
            atlas->RT->LastPixelPicks.clear();
    }

    // Set parameters
    si->Atlas = atlas;
    si->SprRect.L = float(x) / float(atlas->Width);
    si->SprRect.T = float(y) / float(atlas->Height);
    si->SprRect.R = float(x + w) / float(atlas->Width);
    si->SprRect.B = float(y + h) / float(atlas->Height);

    // Delete data
    SAFEDELA(data);
}

AnyFrames* SpriteManager::LoadAnimation(const string& fname, bool use_dummy, bool frm_anim_pix)
{
    AnyFrames* dummy = (use_dummy ? DummyAnimation : nullptr);

    if (fname.empty())
        return dummy;

    string ext = _str(fname).getFileExtension();
    if (ext.empty())
    {
        WriteLog("Extension not found, file '{}'.\n", fname);
        return dummy;
    }

    AnyFrames* result = nullptr;
    if (Is3dExtensionSupported(ext))
        result = LoadAnimation3d(fname);
    else
        result = LoadAnimation2d(fname);

    return result ? result : dummy;
}

AnyFrames* SpriteManager::LoadAnimation2d(const string& fname)
{
    File file = fileMngr.ReadFile(fname);
    if (!file)
        return nullptr;

    RUNTIME_ASSERT(file.GetUChar() == 42);
    ushort frames_count = file.GetBEUShort();
    uint ticks = file.GetBEUInt();
    ushort dirs = file.GetBEUShort();

    AnyFrames* anim = CreateAnyFrames(frames_count, ticks);
    if (dirs > 1)
        CreateAnyFramesDirAnims(anim, dirs);

    for (ushort dir = 0; dir < dirs; dir++)
    {
        AnyFrames* dir_anim = anim->GetDir(dir);
        short ox = file.GetBEShort();
        short oy = file.GetBEShort();
        for (ushort i = 0; i < frames_count; i++)
        {
            if (!file.GetUChar())
            {
                SpriteInfo* si = new SpriteInfo();
                si->OffsX = ox;
                si->OffsY = oy;
                ushort w = file.GetBEUShort();
                ushort h = file.GetBEUShort();
                dir_anim->NextX[i] = file.GetBEShort();
                dir_anim->NextY[i] = file.GetBEShort();
                uchar* data = file.GetCurBuf();
                dir_anim->Ind[i] = RequestFillAtlas(si, w, h, data);
                file.GoForward(w * h * 4);
            }
            else
            {
                ushort index = file.GetBEUShort();
                dir_anim->Ind[i] = dir_anim->Ind[index];
            }
        }
    }

    RUNTIME_ASSERT(file.GetUChar() == 42);
    return anim;
}

AnyFrames* SpriteManager::ReloadAnimation(AnyFrames* anim, const string& fname)
{
    if (fname.empty())
        return anim;

    // Release old images
    if (anim)
    {
        for (uint i = 0; i < anim->CntFrm; i++)
        {
            SpriteInfo* si = GetSpriteInfo(anim->Ind[i]);
            if (si)
                DestroyAtlases(si->Atlas->Type);
        }
        DestroyAnyFrames(anim);
    }

    // Load fresh
    return LoadAnimation(fname);
}

AnyFrames* SpriteManager::LoadAnimation3d(const string& fname)
{
    if (!settings.Enable3dRendering)
        return nullptr;

    // Load 3d animation
    RUNTIME_ASSERT(anim3dMngr);
    Animation3d* anim3d = anim3dMngr->GetAnimation(fname, false);
    if (!anim3d)
        return nullptr;
    anim3d->StartMeshGeneration();

    // Get animation data
    float period;
    int proc_from, proc_to;
    int dir;
    anim3d->GetRenderFramesData(period, proc_from, proc_to, dir);

    // Set fir
    if (dir < 0)
        anim3d->SetDirAngle(-dir);
    else
        anim3d->SetDir(dir);

    // If no animations available than render just one
    if (period == 0.0f || proc_from == proc_to)
    {
    label_LoadOneSpr:
        anim3d->SetAnimation(0, proc_from * 10, nullptr, ANIMATION_ONE_TIME | ANIMATION_STAY);
        Render3d(anim3d);

        AnyFrames* anim = CreateAnyFrames(1, 100);
        anim->Ind[0] = anim3d->SprId;

        SAFEDEL(anim3d);
        return anim;
    }

    // Calculate need information
    float frame_time = 1.0f / (float)(settings.Animation3dFPS ? settings.Animation3dFPS : 10); // 1 second / fps
    float period_from = period * (float)proc_from / 100.0f;
    float period_to = period * (float)proc_to / 100.0f;
    float period_len = fabs(period_to - period_from);
    float proc_step = (float)(proc_to - proc_from) / (period_len / frame_time);
    int frames_count = (int)ceil(period_len / frame_time);

    if (frames_count <= 1)
        goto label_LoadOneSpr;

    AnyFrames* anim = CreateAnyFrames(frames_count, (uint)(period_len * 1000.0f));

    float cur_proc = (float)proc_from;
    int prev_cur_proci = -1;
    for (int i = 0; i < frames_count; i++)
    {
        int cur_proci = (proc_to > proc_from ? (int)(10.0f * cur_proc + 0.5) : (int)(10.0f * cur_proc));

        // Previous frame is different
        if (cur_proci != prev_cur_proci)
        {
            anim3d->SetAnimation(0, cur_proci, nullptr, ANIMATION_ONE_TIME | ANIMATION_STAY);
            Render3d(anim3d);

            anim->Ind[i] = anim3d->SprId;
        }
        // Previous frame is same
        else
        {
            anim->Ind[i] = anim->Ind[i - 1];
        }

        cur_proc += proc_step;
        prev_cur_proci = cur_proci;
    }

    SAFEDEL(anim3d);
    return anim;
}

bool SpriteManager::Render3d(Animation3d* anim3d)
{
    // Find place for render
    if (!anim3d->SprId)
        RefreshPure3dAnimationSprite(anim3d);

    // Find place for render
    SpriteInfo* si = sprData[anim3d->SprId];
    RenderTarget* rt = nullptr;
    for (auto it = rt3D.begin(), end = rt3D.end(); it != end; ++it)
    {
        RenderTarget* rt_ = *it;
        if (rt_->MainTex->Width == si->Width && rt_->MainTex->Height == si->Height)
        {
            rt = rt_;
            break;
        }
    }
    if (!rt)
    {
        rt = CreateRenderTarget(true, true, false, si->Width, si->Height, false);
        rt3D.push_back(rt);
    }

    PushRenderTarget(rt);
    ClearCurrentRenderTarget(0);
    ClearCurrentRenderTargetDepth();

    RUNTIME_ASSERT(anim3dMngr);
    anim3dMngr->SetScreenSize(rt->MainTex->Width, rt->MainTex->Height);

    // Draw model
    anim3d->Draw(0, 0);

    // Restore render target
    PopRenderTarget();

    // Copy render
    Rect region_to((int)(si->SprRect.L * (float)si->Atlas->Width + 0.5f),
        (int)((1.0f - si->SprRect.T) * (float)si->Atlas->Height + 0.5f),
        (int)(si->SprRect.R * (float)si->Atlas->Width + 0.5f),
        (int)((1.0f - si->SprRect.B) * (float)si->Atlas->Height + 0.5f));
    PushRenderTarget(si->Atlas->RT);
    DrawRenderTarget(rt, false, nullptr, &region_to);
    PopRenderTarget();

    return true;
}

bool SpriteManager::Draw3d(int x, int y, Animation3d* anim3d, uint color)
{
    if (!settings.Enable3dRendering)
        return false;

    anim3d->StartMeshGeneration();
    Render3d(anim3d);

    SpriteInfo* si = sprData[anim3d->SprId];
    DrawSprite(anim3d->SprId, x - si->Width / 2 + si->OffsX, y - si->Height + si->OffsY, color);

    return true;
}

Animation3d* SpriteManager::LoadPure3dAnimation(const string& fname, bool auto_redraw)
{
    if (!settings.Enable3dRendering)
        return nullptr;

    // Fill data
    RUNTIME_ASSERT(anim3dMngr);
    Animation3d* anim3d = anim3dMngr->GetAnimation(fname, false);
    if (!anim3d)
        return nullptr;

    // Create render sprite
    anim3d->SprId = 0;
    anim3d->SprAtlasType = (int)std::get<0>(atlasStack.back());
    if (auto_redraw)
    {
        RefreshPure3dAnimationSprite(anim3d);
        autoRedrawAnim3d.push_back(anim3d);
    }
    return anim3d;
}

void SpriteManager::RefreshPure3dAnimationSprite(Animation3d* anim3d)
{
    // Free old place
    if (anim3d->SprId)
    {
        sprData[anim3d->SprId]->Anim3d = nullptr;
        anim3d->SprId = 0;
    }

    // Render size
    uint draw_width, draw_height;
    anim3d->GetDrawSize(draw_width, draw_height);

    // Find already created place for rendering
    uint index = 0;
    for (size_t i = 0, j = sprData.size(); i < j; i++)
    {
        SpriteInfo* si = sprData[i];
        if (si && si->UsedForAnim3d && !si->Anim3d && (uint)si->Width == draw_width &&
            (uint)si->Height == draw_height && si->Atlas->Type == (AtlasType)anim3d->SprAtlasType)
        {
            index = (uint)i;
            break;
        }
    }

    // Create new place for rendering
    if (!index)
    {
        PushAtlasType((AtlasType)anim3d->SprAtlasType);
        index = RequestFillAtlas(nullptr, draw_width, draw_height, nullptr);
        PopAtlasType();
        SpriteInfo* si = sprData[index];
        si->OffsY = draw_height / 4;
        si->UsedForAnim3d = true;
    }

    // Cross links
    anim3d->SprId = index;
    sprData[index]->Anim3d = anim3d;
}

void SpriteManager::FreePure3dAnimation(Animation3d* anim3d)
{
    RUNTIME_ASSERT(anim3d);

    auto it = std::find(autoRedrawAnim3d.begin(), autoRedrawAnim3d.end(), anim3d);
    if (it != autoRedrawAnim3d.end())
        autoRedrawAnim3d.erase(it);

    if (anim3d->SprId)
        sprData[anim3d->SprId]->Anim3d = nullptr;

    delete anim3d;
}

AnyFrames* SpriteManager::CreateAnyFrames(uint frames, uint ticks)
{
    AnyFrames* anim = (AnyFrames*)anyFramesPool.Get();
    memzero(anim, sizeof(AnyFrames));
    anim->CntFrm = MIN(frames, MAX_FRAMES);
    anim->Ticks = (ticks ? ticks : frames * 100);
    return anim;
}

void SpriteManager::CreateAnyFramesDirAnims(AnyFrames* anim, uint dirs)
{
    RUNTIME_ASSERT(dirs > 1);
    RUNTIME_ASSERT((dirs == 6 || dirs == 8));
    anim->DirCount = dirs;
    for (uint dir = 0; dir < dirs - 1; dir++)
        anim->Dirs[dir] = CreateAnyFrames(anim->CntFrm, anim->Ticks);
}

void SpriteManager::DestroyAnyFrames(AnyFrames* anim)
{
    if (!anim || anim == DummyAnimation)
        return;

    for (int dir = 1; dir < anim->DirCount; dir++)
        anyFramesPool.Put(anim->GetDir(dir));
    anyFramesPool.Put(anim);
}

bool SpriteManager::Flush()
{
    if (!curDrawQuad)
        return true;

    // Todo: finish rendering
    /*EnableVertexArray(quadsVertexArray, 4 * curDrawQuad);
    EnableScissor();

    uint pos = 0;
    for (auto& dip : dipQueue)
    {
        for (size_t pass = 0; pass < dip.SourceEffect->Passes.size(); pass++)
        {
            EffectPass& effect_pass = dip.SourceEffect->Passes[pass];

            GL(glUseProgram(effect_pass.Program));

            if (IS_EFFECT_VALUE(effect_pass.ZoomFactor))
                GL(glUniform1f(effect_pass.ZoomFactor, settings.SpritesZoom));
            if (IS_EFFECT_VALUE(effect_pass.ProjectionMatrix))
                GL(glUniformMatrix4fv(effect_pass.ProjectionMatrix, 1, GL_FALSE, projectionMatrixCM[0]));
            if (IS_EFFECT_VALUE(effect_pass.ColorMap) && dip.MainTex)
            {
                if (dip.MainTex->Samples == 0.0f)
                {
                    GL(glBindTexture(GL_TEXTURE_2D, dip.MainTex->Id));
                }
                else
                {
                    GL(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, dip.MainTex->Id));
                    if (IS_EFFECT_VALUE(effect_pass.ColorMapSamples))
                        GL(glUniform1f(effect_pass.ColorMapSamples, dip.MainTex->Samples));
                }
                GL(glUniform1i(effect_pass.ColorMap, 0));
                if (IS_EFFECT_VALUE(effect_pass.ColorMapSize))
                    GL(glUniform4fv(effect_pass.ColorMapSize, 1, dip.MainTex->SizeData));
            }
            if (IS_EFFECT_VALUE(effect_pass.EggMap) && sprEgg)
            {
                GL(glActiveTexture(GL_TEXTURE1));
                GL(glBindTexture(GL_TEXTURE_2D, sprEgg->Atlas->MainTex->Id));
                GL(glActiveTexture(GL_TEXTURE0));
                GL(glUniform1i(effect_pass.EggMap, 1));
                if (IS_EFFECT_VALUE(effect_pass.EggMapSize))
                    GL(glUniform4fv(effect_pass.EggMapSize, 1, sprEgg->Atlas->MainTex->SizeData));
            }
            if (IS_EFFECT_VALUE(effect_pass.SpriteBorder))
                GL(glUniform4f(effect_pass.SpriteBorder, dip.SpriteBorder.L, dip.SpriteBorder.T, dip.SpriteBorder.R,
                    dip.SpriteBorder.B));

            if (effect_pass.IsNeedProcess)
                effectMngr.EffectProcessVariables(effect_pass, true);

            GLsizei count = 6 * dip.SpritesCount;
            GL(glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (void*)(size_t)(pos * 2)));

            if (effect_pass.IsNeedProcess)
                effectMngr.EffectProcessVariables(effect_pass, false);
        }

        pos += 6 * dip.SpritesCount;
    }
    dipQueue.clear();
    curDrawQuad = 0;

    GL(glUseProgram(0));
    DisableVertexArray(quadsVertexArray);
    DisableScissor();*/

    return true;
}

bool SpriteManager::DrawSprite(uint id, int x, int y, uint color)
{
    if (!id)
        return false;

    SpriteInfo* si = sprData[id];
    if (!si)
        return false;

    RenderEffect* effect = (si->DrawEffect ? si->DrawEffect : effectMngr.Effects.Iface);
    if (dipQueue.empty() || dipQueue.back().SourceEffect->CanBatch(effect))
        dipQueue.push_back({si->Atlas->MainTex, effect, 1});
    else
        dipQueue.back().SpritesCount++;

    int pos = curDrawQuad * 4;

    if (!color)
        color = COLOR_IFACE;
    color = COLOR_SWAP_RB(color);

    vBuffer[pos].X = (float)x;
    vBuffer[pos].Y = (float)y + si->Height;
    vBuffer[pos].TU = si->SprRect.L;
    vBuffer[pos].TV = si->SprRect.B;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = (float)x;
    vBuffer[pos].Y = (float)y;
    vBuffer[pos].TU = si->SprRect.L;
    vBuffer[pos].TV = si->SprRect.T;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = (float)x + si->Width;
    vBuffer[pos].Y = (float)y;
    vBuffer[pos].TU = si->SprRect.R;
    vBuffer[pos].TV = si->SprRect.T;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = (float)x + si->Width;
    vBuffer[pos].Y = (float)y + si->Height;
    vBuffer[pos].TU = si->SprRect.R;
    vBuffer[pos].TV = si->SprRect.B;
    vBuffer[pos].Diffuse = color;

    if (++curDrawQuad == drawQuadCount)
        Flush();
    return true;
}

bool SpriteManager::DrawSprite(AnyFrames* frames, int x, int y, uint color)
{
    if (frames && frames != DummyAnimation)
        return DrawSprite(frames->GetCurSprId(), x, y, color);
    return false;
}

bool SpriteManager::DrawSpriteSize(uint id, int x, int y, int w, int h, bool zoom_up, bool center, uint color)
{
    return DrawSpriteSizeExt(id, x, y, w, h, zoom_up, center, false, color);
}

bool SpriteManager::DrawSpriteSize(AnyFrames* frames, int x, int y, int w, int h, bool zoom_up, bool center, uint color)
{
    if (frames && frames != DummyAnimation)
        return DrawSpriteSize(frames->GetCurSprId(), x, y, w, h, zoom_up, center, color);
    return false;
}

bool SpriteManager::DrawSpriteSizeExt(
    uint id, int x, int y, int w, int h, bool zoom_up, bool center, bool stretch, uint color)
{
    if (!id)
        return false;

    SpriteInfo* si = sprData[id];
    if (!si)
        return false;

    float xf = (float)x;
    float yf = (float)y;
    float wf = (float)si->Width;
    float hf = (float)si->Height;
    float k = MIN(w / wf, h / hf);
    if (!stretch)
    {
        if (k < 1.0f || (k > 1.0f && zoom_up))
        {
            wf = floorf(wf * k + 0.5f);
            hf = floorf(hf * k + 0.5f);
        }
        if (center)
        {
            xf += floorf(((float)w - wf) / 2.0f + 0.5f);
            yf += floorf(((float)h - hf) / 2.0f + 0.5f);
        }
    }
    else if (zoom_up)
    {
        wf = floorf(wf * k + 0.5f);
        hf = floorf(hf * k + 0.5f);
        if (center)
        {
            xf += floorf(((float)w - wf) / 2.0f + 0.5f);
            yf += floorf(((float)h - hf) / 2.0f + 0.5f);
        }
    }
    else
    {
        wf = (float)w;
        hf = (float)h;
    }

    RenderEffect* effect = (si->DrawEffect ? si->DrawEffect : effectMngr.Effects.Iface);
    if (dipQueue.empty() || dipQueue.back().MainTex != si->Atlas->MainTex || dipQueue.back().SourceEffect != effect)
        dipQueue.push_back({si->Atlas->MainTex, effect, 1});
    else
        dipQueue.back().SpritesCount++;

    int pos = curDrawQuad * 4;

    if (!color)
        color = COLOR_IFACE;
    color = COLOR_SWAP_RB(color);

    vBuffer[pos].X = xf;
    vBuffer[pos].Y = yf + hf;
    vBuffer[pos].TU = si->SprRect.L;
    vBuffer[pos].TV = si->SprRect.B;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = xf;
    vBuffer[pos].Y = yf;
    vBuffer[pos].TU = si->SprRect.L;
    vBuffer[pos].TV = si->SprRect.T;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = xf + wf;
    vBuffer[pos].Y = yf;
    vBuffer[pos].TU = si->SprRect.R;
    vBuffer[pos].TV = si->SprRect.T;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = xf + wf;
    vBuffer[pos].Y = yf + hf;
    vBuffer[pos].TU = si->SprRect.R;
    vBuffer[pos].TV = si->SprRect.B;
    vBuffer[pos].Diffuse = color;

    if (++curDrawQuad == drawQuadCount)
        Flush();
    return true;
}

bool SpriteManager::DrawSpritePattern(uint id, int x, int y, int w, int h, int spr_width, int spr_height, uint color)
{
    if (!id)
        return false;

    if (!w || !h)
        return false;

    SpriteInfo* si = sprData[id];
    if (!si)
        return false;

    float width = (float)si->Width;
    float height = (float)si->Height;

    if (spr_width && spr_height)
    {
        width = (float)spr_width;
        height = (float)spr_height;
    }
    else if (spr_width)
    {
        float ratio = (float)spr_width / width;
        width = (float)spr_width;
        height *= ratio;
    }
    else if (spr_height)
    {
        float ratio = (float)spr_height / height;
        height = (float)spr_height;
        width *= ratio;
    }

    if (!color)
        color = COLOR_IFACE;
    color = COLOR_SWAP_RB(color);

    RenderEffect* effect = (si->DrawEffect ? si->DrawEffect : effectMngr.Effects.Iface);

    float last_right_offs = (si->SprRect.R - si->SprRect.L) / width;
    float last_bottom_offs = (si->SprRect.B - si->SprRect.T) / height;

    for (float yy = (float)y, end_y = (float)y + h; yy < end_y; yy += height)
    {
        bool last_y = yy + height >= end_y;
        for (float xx = (float)x, end_x = (float)x + w; xx < end_x; xx += width)
        {
            bool last_x = xx + width >= end_x;

            if (dipQueue.empty() || dipQueue.back().MainTex != si->Atlas->MainTex ||
                dipQueue.back().SourceEffect != effect)
                dipQueue.push_back({si->Atlas->MainTex, effect, 1});
            else
                dipQueue.back().SpritesCount++;

            int pos = curDrawQuad * 4;

            float local_width = last_x ? (end_x - xx) : width;
            float local_height = last_y ? (end_y - yy) : height;
            float local_right = last_x ? si->SprRect.L + last_right_offs * local_width : si->SprRect.R;
            float local_bottom = last_y ? si->SprRect.T + last_bottom_offs * local_height : si->SprRect.B;

            vBuffer[pos].X = xx;
            vBuffer[pos].Y = yy + local_height;
            vBuffer[pos].TU = si->SprRect.L;
            vBuffer[pos].TV = local_bottom;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = xx;
            vBuffer[pos].Y = yy;
            vBuffer[pos].TU = si->SprRect.L;
            vBuffer[pos].TV = si->SprRect.T;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = xx + local_width;
            vBuffer[pos].Y = yy;
            vBuffer[pos].TU = local_right;
            vBuffer[pos].TV = si->SprRect.T;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = xx + local_width;
            vBuffer[pos].Y = yy + local_height;
            vBuffer[pos].TU = local_right;
            vBuffer[pos].TV = local_bottom;
            vBuffer[pos].Diffuse = color;

            if (++curDrawQuad == drawQuadCount)
                Flush();
        }
    }

    DisableScissor();
    return true;
}

void SpriteManager::PrepareSquare(PointVec& points, Rect r, uint color)
{
    points.push_back({r.L, r.T, color});
    points.push_back({r.R, r.B, color});
    points.push_back({r.L, r.T, color});
    points.push_back({r.R, r.T, color});
    points.push_back({r.R, r.B, color});
}

void SpriteManager::PrepareSquare(PointVec& points, Point lt, Point rt, Point lb, Point rb, uint color)
{
    points.push_back({lb.X, lb.Y, color});
    points.push_back({lt.X, lt.Y, color});
    points.push_back({rb.X, rb.Y, color});
    points.push_back({lt.X, lt.Y, color});
    points.push_back({rt.X, rt.Y, color});
    points.push_back({rb.X, rb.Y, color});
}

uint SpriteManager::PackColor(int r, int g, int b, int a)
{
    r = CLAMP(r, 0, 255);
    g = CLAMP(g, 0, 255);
    b = CLAMP(b, 0, 255);
    return COLOR_RGBA(a, r, g, b);
}

void SpriteManager::GetDrawRect(Sprite* prep, Rect& rect)
{
    uint id = (prep->PSprId ? *prep->PSprId : prep->SprId);
    if (id >= sprData.size())
        return;
    SpriteInfo* si = sprData[id];
    if (!si)
        return;

    int x = prep->ScrX - si->Width / 2 + si->OffsX + *prep->PScrX;
    int y = prep->ScrY - si->Height + si->OffsY + *prep->PScrY;
    if (prep->OffsX)
        x += *prep->OffsX;
    if (prep->OffsY)
        y += *prep->OffsY;
    rect.L = x;
    rect.T = y;
    rect.R = x + si->Width;
    rect.B = y + si->Height;
}

void SpriteManager::InitializeEgg(const string& egg_name)
{
    eggValid = false;
    eggHx = eggHy = eggX = eggY = 0;
    AnyFrames* egg_frames = LoadAnimation(egg_name);
    if (egg_frames)
    {
        sprEgg = GetSpriteInfo(egg_frames->Ind[0]);
        DestroyAnyFrames(egg_frames);
        eggSprWidth = sprEgg->Width;
        eggSprHeight = sprEgg->Height;
        eggAtlasWidth = (float)App::Render::MaxAtlasWidth;
        eggAtlasHeight = (float)App::Render::MaxAtlasHeight;

        int x = (int)(sprEgg->Atlas->MainTex->SizeData[0] * sprEgg->SprRect.L);
        int y = (int)(sprEgg->Atlas->MainTex->SizeData[1] * sprEgg->SprRect.T);
        eggData = App::Render::GetTextureRegion(sprEgg->Atlas->MainTex, x, y, eggSprWidth, eggSprHeight);
    }
    else
    {
        WriteLog("Load sprite '{}' fail. Egg disabled.\n", egg_name);
    }
}

bool SpriteManager::CompareHexEgg(ushort hx, ushort hy, int egg_type)
{
    if (egg_type == EGG_ALWAYS)
        return true;
    if (eggHy == hy && hx & 1 && !(eggHx & 1))
        hy--;
    switch (egg_type)
    {
    case EGG_X:
        if (hx >= eggHx)
            return true;
        break;
    case EGG_Y:
        if (hy >= eggHy)
            return true;
        break;
    case EGG_X_AND_Y:
        if (hx >= eggHx || hy >= eggHy)
            return true;
        break;
    case EGG_X_OR_Y:
        if (hx >= eggHx && hy >= eggHy)
            return true;
        break;
    default:
        break;
    }
    return false;
}

void SpriteManager::SetEgg(ushort hx, ushort hy, Sprite* spr)
{
    if (!sprEgg)
        return;

    uint id = (spr->PSprId ? *spr->PSprId : spr->SprId);
    SpriteInfo* si = sprData[id];
    if (!si)
        return;

    eggX = spr->ScrX + si->OffsX - sprEgg->Width / 2 + *spr->OffsX + *spr->PScrX;
    eggY = spr->ScrY - si->Height / 2 + si->OffsY - sprEgg->Height / 2 + *spr->OffsY + *spr->PScrY;
    eggHx = hx;
    eggHy = hy;
    eggValid = true;
}

bool SpriteManager::DrawSprites(Sprites& dtree, bool collect_contours, bool use_egg, int draw_oder_from,
    int draw_oder_to, bool prerender, int prerender_ox, int prerender_oy)
{
    if (!dtree.Size())
        return true;

    PointVec borders;

    if (!eggValid)
        use_egg = false;
    int ex = eggX + settings.ScrOx;
    int ey = eggY + settings.ScrOy;
    uint cur_tick = Timer::FastTick();

    for (Sprite* spr = dtree.RootSprite(); spr; spr = spr->ChainChild)
    {
        RUNTIME_ASSERT(spr->Valid);

        if (spr->DrawOrderType < draw_oder_from)
            continue;
        if (spr->DrawOrderType > draw_oder_to)
            break;

        uint id = (spr->PSprId ? *spr->PSprId : spr->SprId);
        SpriteInfo* si = sprData[id];
        if (!si)
            continue;

        // Coords
        int x = spr->ScrX - si->Width / 2 + si->OffsX + *spr->PScrX;
        int y = spr->ScrY - si->Height + si->OffsY + *spr->PScrY;
        if (!prerender)
        {
            x += settings.ScrOx;
            y += settings.ScrOy;
        }
        if (spr->OffsX)
            x += *spr->OffsX;
        if (spr->OffsY)
            y += *spr->OffsY;
        float zoom = settings.SpritesZoom;

        // Base color
        uint color_r, color_l;
        if (spr->Color)
            color_r = color_l = (spr->Color | 0xFF000000);
        else
            color_r = color_l = baseColor;

        // Light
        if (spr->Light)
        {
            static auto light_func = [](uint& c, uchar* l, uchar* l2) {
                int lr = *l;
                int lg = *(l + 1);
                int lb = *(l + 2);
                int lr2 = *l2;
                int lg2 = *(l2 + 1);
                int lb2 = *(l2 + 2);
                uchar& r = ((uchar*)&c)[2];
                uchar& g = ((uchar*)&c)[1];
                uchar& b = ((uchar*)&c)[0];
                int ir = (int)r + (lr + lr2) / 2;
                int ig = (int)g + (lg + lg2) / 2;
                int ib = (int)b + (lb + lb2) / 2;
                r = MIN(ir, 255);
                g = MIN(ig, 255);
                b = MIN(ib, 255);
            };
            light_func(color_r, spr->Light, spr->LightRight);
            light_func(color_l, spr->Light, spr->LightLeft);
        }

        // Alpha
        if (spr->Alpha)
        {
            ((uchar*)&color_r)[3] = *spr->Alpha;
            ((uchar*)&color_l)[3] = *spr->Alpha;
        }

        // Process flashing
        if (spr->FlashMask)
        {
            static int cnt = 0;
            static uint tick = cur_tick + 100;
            static bool add = true;
            if (cur_tick >= tick)
            {
                cnt += (add ? 10 : -10);
                if (cnt > 40)
                {
                    cnt = 40;
                    add = false;
                }
                else if (cnt < -40)
                {
                    cnt = -40;
                    add = true;
                }
                tick = cur_tick + 100;
            }
            static auto flash_func = [](uint& c, int cnt, uint mask) {
                int r = ((c >> 16) & 0xFF) + cnt;
                int g = ((c >> 8) & 0xFF) + cnt;
                int b = (c & 0xFF) + cnt;
                r = CLAMP(r, 0, 0xFF);
                g = CLAMP(g, 0, 0xFF);
                b = CLAMP(b, 0, 0xFF);
                ((uchar*)&c)[2] = r;
                ((uchar*)&c)[1] = g;
                ((uchar*)&c)[0] = b;
                c &= mask;
            };
            flash_func(color_r, cnt, spr->FlashMask);
            flash_func(color_l, cnt, spr->FlashMask);
        }

        // Fix color
        color_r = COLOR_SWAP_RB(color_r);
        color_l = COLOR_SWAP_RB(color_l);

        // Check borders
        if (!prerender)
        {
            if (x / zoom > settings.ScreenWidth || (x + si->Width) / zoom < 0 || y / zoom > settings.ScreenHeight ||
                (y + si->Height) / zoom < 0)
                continue;
        }

        // Egg process
        bool egg_added = false;
        if (use_egg && spr->EggType && CompareHexEgg(spr->HexX, spr->HexY, spr->EggType))
        {
            int x1 = x - ex;
            int y1 = y - ey;
            int x2 = x1 + si->Width;
            int y2 = y1 + si->Height;

            if (spr->CutType)
            {
                x1 += (int)spr->CutX;
                x2 = x1 + (int)spr->CutW;
            }

            if (!(x1 >= eggSprWidth || y1 >= eggSprHeight || x2 < 0 || y2 < 0))
            {
                x1 = MAX(x1, 0);
                y1 = MAX(y1, 0);
                x2 = MIN(x2, eggSprWidth);
                y2 = MIN(y2, eggSprHeight);

                float x1f = (float)(x1 + ATLAS_SPRITES_PADDING);
                float x2f = (float)(x2 + ATLAS_SPRITES_PADDING);
                float y1f = (float)(y1 + ATLAS_SPRITES_PADDING);
                float y2f = (float)(y2 + ATLAS_SPRITES_PADDING);

                int pos = curDrawQuad * 4;
                vBuffer[pos + 0].TUEgg = x1f / eggAtlasWidth;
                vBuffer[pos + 0].TVEgg = y2f / eggAtlasHeight;
                vBuffer[pos + 1].TUEgg = x1f / eggAtlasWidth;
                vBuffer[pos + 1].TVEgg = y1f / eggAtlasHeight;
                vBuffer[pos + 2].TUEgg = x2f / eggAtlasWidth;
                vBuffer[pos + 2].TVEgg = y1f / eggAtlasHeight;
                vBuffer[pos + 3].TUEgg = x2f / eggAtlasWidth;
                vBuffer[pos + 3].TVEgg = y2f / eggAtlasHeight;

                egg_added = true;
            }
        }

        // Choose effect
        RenderEffect* effect = (spr->DrawEffect ? *spr->DrawEffect : nullptr);
        if (!effect)
            effect = (si->DrawEffect ? si->DrawEffect : effectMngr.Effects.Generic);

        // Choose atlas
        if (dipQueue.empty() || dipQueue.back().MainTex != si->Atlas->MainTex || dipQueue.back().SourceEffect != effect)
            dipQueue.push_back({si->Atlas->MainTex, effect, 1});
        else
            dipQueue.back().SpritesCount++;

        // Casts
        float xf = (float)x / zoom + (prerender ? (float)prerender_ox : 0.0f);
        float yf = (float)y / zoom + (prerender ? (float)prerender_oy : 0.0f);
        float wf = (float)si->Width / zoom;
        float hf = (float)si->Height / zoom;

        // Fill buffer
        int pos = curDrawQuad * 4;

        vBuffer[pos].X = xf;
        vBuffer[pos].Y = yf + hf;
        vBuffer[pos].TU = si->SprRect.L;
        vBuffer[pos].TV = si->SprRect.B;
        vBuffer[pos++].Diffuse = color_l;

        vBuffer[pos].X = xf;
        vBuffer[pos].Y = yf;
        vBuffer[pos].TU = si->SprRect.L;
        vBuffer[pos].TV = si->SprRect.T;
        vBuffer[pos++].Diffuse = color_l;

        vBuffer[pos].X = xf + wf;
        vBuffer[pos].Y = yf;
        vBuffer[pos].TU = si->SprRect.R;
        vBuffer[pos].TV = si->SprRect.T;
        vBuffer[pos++].Diffuse = color_r;

        vBuffer[pos].X = xf + wf;
        vBuffer[pos].Y = yf + hf;
        vBuffer[pos].TU = si->SprRect.R;
        vBuffer[pos].TV = si->SprRect.B;
        vBuffer[pos++].Diffuse = color_r;

        // Cutted sprite
        if (spr->CutType)
        {
            xf = (float)(x + spr->CutX) / zoom;
            wf = spr->CutW / zoom;
            vBuffer[pos - 4].X = xf;
            vBuffer[pos - 4].TU = spr->CutTexL;
            vBuffer[pos - 3].X = xf;
            vBuffer[pos - 3].TU = spr->CutTexL;
            vBuffer[pos - 2].X = xf + wf;
            vBuffer[pos - 2].TU = spr->CutTexR;
            vBuffer[pos - 1].X = xf + wf;
            vBuffer[pos - 1].TU = spr->CutTexR;
        }

        // Set default texture coordinates for egg texture
        if (!egg_added && vBuffer[pos - 1].TUEgg != -1.0f)
        {
            vBuffer[pos - 1].TUEgg = -1.0f;
            vBuffer[pos - 2].TUEgg = -1.0f;
            vBuffer[pos - 3].TUEgg = -1.0f;
            vBuffer[pos - 4].TUEgg = -1.0f;
        }

        // Draw
        if (++curDrawQuad == drawQuadCount)
            Flush();

        // Corners indication
        if (settings.ShowCorners && spr->EggType)
        {
            PointVec corner;
            float cx = wf / 2.0f;

            switch (spr->EggType)
            {
            case EGG_ALWAYS:
                PrepareSquare(corner, RectF(xf + cx - 2.0f, yf + hf - 50.0f, xf + cx + 2.0f, yf + hf), 0x5FFFFF00);
                break;
            case EGG_X:
                PrepareSquare(corner, PointF(xf + cx - 5.0f, yf + hf - 55.0f), PointF(xf + cx + 5.0f, yf + hf - 45.0f),
                    PointF(xf + cx - 5.0f, yf + hf - 5.0f), PointF(xf + cx + 5.0f, yf + hf + 5.0f), 0x5F00AF00);
                break;
            case EGG_Y:
                PrepareSquare(corner, PointF(xf + cx - 5.0f, yf + hf - 49.0f), PointF(xf + cx + 5.0f, yf + hf - 52.0f),
                    PointF(xf + cx - 5.0f, yf + hf + 1.0f), PointF(xf + cx + 5.0f, yf + hf - 2.0f), 0x5F00FF00);
                break;
            case EGG_X_AND_Y:
                PrepareSquare(corner, PointF(xf + cx - 10.0f, yf + hf - 49.0f), PointF(xf + cx, yf + hf - 52.0f),
                    PointF(xf + cx - 10.0f, yf + hf + 1.0f), PointF(xf + cx, yf + hf - 2.0f), 0x5FFF0000);
                PrepareSquare(corner, PointF(xf + cx, yf + hf - 55.0f), PointF(xf + cx + 10.0f, yf + hf - 45.0f),
                    PointF(xf + cx, yf + hf - 5.0f), PointF(xf + cx + 10.0f, yf + hf + 5.0f), 0x5FFF0000);
                break;
            case EGG_X_OR_Y:
                PrepareSquare(corner, PointF(xf + cx, yf + hf - 49.0f), PointF(xf + cx + 10.0f, yf + hf - 52.0f),
                    PointF(xf + cx, yf + hf + 1.0f), PointF(xf + cx + 10.0f, yf + hf - 2.0f), 0x5FAF0000);
                PrepareSquare(corner, PointF(xf + cx - 10.0f, yf + hf - 55.0f), PointF(xf + cx, yf + hf - 45.0f),
                    PointF(xf + cx - 10.0f, yf + hf - 5.0f), PointF(xf + cx, yf + hf + 5.0f), 0x5FAF0000);
            default:
                break;
            }

            DrawPoints(corner, RenderPrimitiveType::TriangleList);
        }

        // Cuts
        if (settings.ShowSpriteCuts && spr->CutType)
        {
            PointVec cut;
            float z = zoom;
            float oy = (spr->CutType == SPRITE_CUT_HORIZONTAL ? 3.0f : -5.2f) / z;
            float x1 = (float)(spr->ScrX - si->Width / 2 + spr->CutX + settings.ScrOx + 1.0f) / z;
            float y1 = (float)(spr->ScrY + spr->CutOyL + settings.ScrOy) / z;
            float x2 = (float)(spr->ScrX - si->Width / 2 + spr->CutX + spr->CutW + settings.ScrOx - 1.0f) / z;
            float y2 = (float)(spr->ScrY + spr->CutOyR + settings.ScrOy) / z;
            PrepareSquare(cut, PointF(x1, y1 - 80.0f / z + oy), PointF(x2, y2 - 80.0f / z - oy), PointF(x1, y1 + oy),
                PointF(x2, y2 - oy), 0x4FFFFF00);
            PrepareSquare(cut, RectF(xf, yf, xf + 1.0f, yf + hf), 0x4F000000);
            PrepareSquare(cut, RectF(xf + wf, yf, xf + wf + 1.0f, yf + hf), 0x4F000000);
            DrawPoints(cut, RenderPrimitiveType::TriangleList);
        }

        // Draw order
        if (settings.ShowDrawOrder)
        {
            float z = zoom;
            int x1, y1;

            if (!spr->CutType)
            {
                x1 = (int)((spr->ScrX + settings.ScrOx) / z);
                y1 = (int)((spr->ScrY + settings.ScrOy) / z);
            }
            else
            {
                x1 = (int)((spr->ScrX - si->Width / 2 + spr->CutX + settings.ScrOx + 1.0f) / z);
                y1 = (int)((spr->ScrY + spr->CutOyL + settings.ScrOy) / z);
            }

            if (spr->DrawOrderType >= DRAW_ORDER_FLAT && spr->DrawOrderType < DRAW_ORDER)
                y1 -= (int)(40.0f / z);

            DrawStr(Rect(x1, y1, x1 + 100, y1 + 100), _str("{}", spr->TreeIndex), 0);
        }

        // Process contour effect
        if (collect_contours && spr->ContourType)
            CollectContour(x, y, si, spr);
    }

    Flush();

    if (settings.ShowSpriteBorders)
        DrawPoints(borders, RenderPrimitiveType::TriangleList);
    return true;
}

bool SpriteManager::IsPixNoTransp(uint spr_id, int offs_x, int offs_y, bool with_zoom)
{
    uint color = GetPixColor(spr_id, offs_x, offs_y, with_zoom);
    return (color & 0xFF000000) != 0;
}

uint SpriteManager::GetPixColor(uint spr_id, int offs_x, int offs_y, bool with_zoom)
{
    if (offs_x < 0 || offs_y < 0)
        return 0;
    SpriteInfo* si = GetSpriteInfo(spr_id);
    if (!si)
        return 0;

    // 2d animation
    if (with_zoom && (offs_x > si->Width / settings.SpritesZoom || offs_y > si->Height / settings.SpritesZoom))
        return 0;
    if (!with_zoom && (offs_x > si->Width || offs_y > si->Height))
        return 0;

    if (with_zoom)
    {
        offs_x = (int)(offs_x * settings.SpritesZoom);
        offs_y = (int)(offs_y * settings.SpritesZoom);
    }

    offs_x += (int)(si->Atlas->MainTex->SizeData[0] * si->SprRect.L);
    offs_y += (int)(si->Atlas->MainTex->SizeData[1] * si->SprRect.T);
    return GetRenderTargetPixel(si->Atlas->RT, offs_x, offs_y);
}

bool SpriteManager::IsEggTransp(int pix_x, int pix_y)
{
    if (!eggValid)
        return false;

    int ex = eggX + settings.ScrOx;
    int ey = eggY + settings.ScrOy;
    int ox = pix_x - (int)(ex / settings.SpritesZoom);
    int oy = pix_y - (int)(ey / settings.SpritesZoom);

    if (ox < 0 || oy < 0 || ox >= int(eggSprWidth / settings.SpritesZoom) ||
        oy >= int(eggSprHeight / settings.SpritesZoom))
        return false;

    ox = (int)(ox * settings.SpritesZoom);
    oy = (int)(oy * settings.SpritesZoom);

    uint egg_color = *(eggData.data() + oy * eggSprWidth + ox);
    return (egg_color >> 24) < 127;
}

bool SpriteManager::DrawPoints(
    PointVec& points, RenderPrimitiveType prim, float* zoom, PointF* offset, RenderEffect* custom_effect)
{
    if (points.empty())
        return true;

    Flush();

    RenderEffect* effect = custom_effect;
    if (!effect)
        effect = effectMngr.Effects.Primitive;

    // Check primitives
    uint count = (uint)points.size();
    int prim_count = (int)count;
    switch (prim)
    {
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
    case RenderPrimitiveType::TriangleFan:
        prim_count -= 2;
        break;
    default:
        throw UnreachablePlaceException(LINE_STR);
    }
    if (prim_count <= 0)
        return false;

    // Resize buffers
    if (vBuffer.size() < count)
        vBuffer.resize(count);

    // Collect data
    for (uint i = 0; i < count; i++)
    {
        PrepPoint& point = points[i];
        float x = (float)point.PointX;
        float y = (float)point.PointY;
        if (point.PointOffsX)
            x += (float)*point.PointOffsX;
        if (point.PointOffsY)
            y += (float)*point.PointOffsY;
        if (zoom)
            x /= *zoom, y /= *zoom;
        if (offset)
            x += offset->X, y += offset->Y;

        memzero(&vBuffer[i], sizeof(Vertex2D));
        vBuffer[i].X = x;
        vBuffer[i].Y = y;
        vBuffer[i].Diffuse = COLOR_SWAP_RB(point.PointColor);
    }

    /*// Enable smooth
#ifndef FO_OPENGL_ES
    if (zoom && *zoom != 1.0f)
        GL(glEnable(prim == PRIMITIVE_POINTLIST ? GL_POINT_SMOOTH : GL_LINE_SMOOTH));
#endif

    // Draw
    EnableVertexArray(pointsVertexArray, count);
    EnableScissor();

    for (size_t pass = 0; pass < draw_effect->Passes.size(); pass++)
    {
        EffectPass& effect_pass = draw_effect->Passes[pass];

        GL(glUseProgram(effect_pass.Program));

        if (IS_EFFECT_VALUE(effect_pass.ProjectionMatrix))
            GL(glUniformMatrix4fv(effect_pass.ProjectionMatrix, 1, GL_FALSE, projectionMatrixCM[0]));

        if (effect_pass.IsNeedProcess)
            effectMngr.EffectProcessVariables(effect_pass, true);

        GL(glDrawElements(prim_type, count, GL_UNSIGNED_SHORT, (void*)0));

        if (effect_pass.IsNeedProcess)
            effectMngr.EffectProcessVariables(effect_pass, false);
    }

    GL(glUseProgram(0));
    DisableVertexArray(pointsVertexArray);
    DisableScissor();

// Disable smooth
#ifndef FO_OPENGL_ES
    if (zoom && *zoom != 1.0f)
        GL(glDisable(prim == PRIMITIVE_POINTLIST ? GL_POINT_SMOOTH : GL_LINE_SMOOTH));
#endif*/

    return true;
}

bool SpriteManager::DrawContours()
{
    if (contoursAdded && rtContours && rtContoursMid)
    {
        // Draw collected contours
        DrawRenderTarget(rtContours, true);

        // Clean render targets
        PushRenderTarget(rtContours);
        ClearCurrentRenderTarget(0);
        PopRenderTarget();
        PushRenderTarget(rtContoursMid);
        ClearCurrentRenderTarget(0);
        PopRenderTarget();
        contoursAdded = false;
    }
    return true;
}

bool SpriteManager::CollectContour(int x, int y, SpriteInfo* si, Sprite* spr)
{
    if (!rtContours || !rtContoursMid || !effectMngr.Effects.Contour)
        return true;

    Rect borders = Rect(x - 1, y - 1, x + si->Width + 1, y + si->Height + 1);
    RenderTexture* texture = si->Atlas->MainTex;
    RectF textureuv;
    RectF sprite_border;

    if (borders.L >= settings.ScreenWidth * settings.SpritesZoom || borders.R < 0 ||
        borders.T >= settings.ScreenHeight * settings.SpritesZoom || borders.B < 0)
        return true;

    if (settings.SpritesZoom == 1.0f)
    {
        RectF& sr = si->SprRect;
        float txw = texture->SizeData[2];
        float txh = texture->SizeData[3];
        textureuv(sr.L - txw, sr.T - txh, sr.R + txw, sr.B + txh);
        sprite_border = textureuv;
    }
    else
    {
        RectF& sr = si->SprRect;
        borders((int)(x / settings.SpritesZoom), (int)(y / settings.SpritesZoom),
            (int)((x + si->Width) / settings.SpritesZoom), (int)((y + si->Height) / settings.SpritesZoom));
        RectF bordersf((float)borders.L, (float)borders.T, (float)borders.R, (float)borders.B);
        float mid_height = rtContoursMid->MainTex->SizeData[1];

        PushRenderTarget(rtContoursMid);

        uint pos = 0;
        vBuffer[pos].X = bordersf.L;
        vBuffer[pos].Y = mid_height - bordersf.B;
        vBuffer[pos].TU = sr.L;
        vBuffer[pos++].TV = sr.B;
        vBuffer[pos].X = bordersf.L;
        vBuffer[pos].Y = mid_height - bordersf.T;
        vBuffer[pos].TU = sr.L;
        vBuffer[pos++].TV = sr.T;
        vBuffer[pos].X = bordersf.R;
        vBuffer[pos].Y = mid_height - bordersf.T;
        vBuffer[pos].TU = sr.R;
        vBuffer[pos++].TV = sr.T;
        vBuffer[pos].X = bordersf.R;
        vBuffer[pos].Y = mid_height - bordersf.B;
        vBuffer[pos].TU = sr.R;
        vBuffer[pos++].TV = sr.B;

        curDrawQuad = 1;
        dipQueue.push_back({texture, effectMngr.Effects.FlushRenderTarget, 1});
        Flush();

        PopRenderTarget();

        texture = rtContoursMid->MainTex.get();
        float tw = texture->SizeData[0];
        float th = texture->SizeData[1];
        borders.L--, borders.T--, borders.R++, borders.B++;
        textureuv((float)borders.L / tw, (float)borders.T / th, (float)borders.R / tw, (float)borders.B / th);
        sprite_border = textureuv;
    }

    uint contour_color = 0;
    if (spr->ContourType == CONTOUR_RED)
        contour_color = 0xFFAF0000;
    else if (spr->ContourType == CONTOUR_YELLOW)
        contour_color = 0x00AFAF00; // Disable flashing by passing alpha == 0.0
    else if (spr->ContourType == CONTOUR_CUSTOM)
        contour_color = spr->ContourColor;
    else
        contour_color = 0xFFAFAFAF;
    contour_color = COLOR_SWAP_RB(contour_color);

    RectF borders_pos((float)borders.L, (float)borders.T, (float)borders.R, (float)borders.B);

    PushRenderTarget(rtContours);

    uint pos = 0;
    vBuffer[pos].X = borders_pos.L;
    vBuffer[pos].Y = borders_pos.B;
    vBuffer[pos].TU = textureuv.L;
    vBuffer[pos].TV = textureuv.B;
    vBuffer[pos++].Diffuse = contour_color;
    vBuffer[pos].X = borders_pos.L;
    vBuffer[pos].Y = borders_pos.T;
    vBuffer[pos].TU = textureuv.L;
    vBuffer[pos].TV = textureuv.T;
    vBuffer[pos++].Diffuse = contour_color;
    vBuffer[pos].X = borders_pos.R;
    vBuffer[pos].Y = borders_pos.T;
    vBuffer[pos].TU = textureuv.R;
    vBuffer[pos].TV = textureuv.T;
    vBuffer[pos++].Diffuse = contour_color;
    vBuffer[pos].X = borders_pos.R;
    vBuffer[pos].Y = borders_pos.B;
    vBuffer[pos].TU = textureuv.R;
    vBuffer[pos].TV = textureuv.B;
    vBuffer[pos++].Diffuse = contour_color;

    curDrawQuad = 1;
    dipQueue.push_back({texture, effectMngr.Effects.Contour, 1});
    dipQueue.back().SpriteBorder = sprite_border;
    Flush();

    PopRenderTarget();
    contoursAdded = true;
    return true;
}

SpriteManager::FontData* SpriteManager::GetFont(int num)
{
    if (num < 0)
        num = defFontIndex;
    if (num < 0 || num >= (int)allFonts.size())
        return nullptr;
    return allFonts[num].get();
}

void SpriteManager::ClearFonts()
{
    allFonts.clear();
}

void SpriteManager::SetDefaultFont(int index, uint color)
{
    defFontIndex = index;
    defFontColor = color;
}

void SpriteManager::SetFontEffect(int index, RenderEffect* effect)
{
    FontData* font = GetFont(index);
    if (font)
        font->DrawEffect = (effect ? effect : effectMngr.Effects.Font);
}

void SpriteManager::BuildFonts()
{
    for (size_t i = 0; i < allFonts.size(); i++)
        if (allFonts[i] && !allFonts[i]->Builded)
            BuildFont((int)i);
}

void SpriteManager::BuildFont(int index)
{
#define PIXEL_AT(tex_data, width, x, y) (*((uint*)tex_data.data() + y * width + x))

    FontData& font = *allFonts[index];
    font.Builded = true;

    // Fix texture coordinates
    SpriteInfo* si = GetSpriteInfo(font.ImageNormal->GetSprId(0));
    float tex_w = (float)si->Atlas->Width;
    float tex_h = (float)si->Atlas->Height;
    float image_x = tex_w * si->SprRect.L;
    float image_y = tex_h * si->SprRect.T;
    int max_h = 0;
    for (auto it = font.Letters.begin(), end = font.Letters.end(); it != end; ++it)
    {
        FontData::Letter& l = it->second;
        float x = (float)l.PosX;
        float y = (float)l.PosY;
        float w = (float)l.W;
        float h = (float)l.H;
        l.TexUV[0] = (image_x + x - 1.0f) / tex_w;
        l.TexUV[1] = (image_y + y - 1.0f) / tex_h;
        l.TexUV[2] = (image_x + x + w + 1.0f) / tex_w;
        l.TexUV[3] = (image_y + y + h + 1.0f) / tex_h;
        if (l.H > max_h)
            max_h = l.H;
    }

    // Fill data
    font.FontTex = si->Atlas->MainTex;
    if (font.LineHeight == 0)
        font.LineHeight = max_h;
    if (font.Letters.count(' '))
        font.SpaceWidth = font.Letters[' '].XAdvance;

    SpriteInfo* si_bordered = (font.ImageBordered ? GetSpriteInfo(font.ImageBordered->GetSprId(0)) : nullptr);
    font.FontTexBordered = (si_bordered ? si_bordered->Atlas->MainTex : nullptr);

    uint normal_ox = (uint)(tex_w * si->SprRect.L);
    uint normal_oy = (uint)(tex_h * si->SprRect.T);
    uint bordered_ox = (si_bordered ? (uint)((float)si_bordered->Atlas->Width * si_bordered->SprRect.L) : 0);
    uint bordered_oy = (si_bordered ? (uint)((float)si_bordered->Atlas->Height * si_bordered->SprRect.T) : 0);

    // Read texture data
    vector<uint> data_normal =
        App::Render::GetTextureRegion(si->Atlas->MainTex, normal_ox, normal_oy, si->Width, si->Height);

    vector<uint> data_bordered;
    if (si_bordered)
    {
        data_bordered = App::Render::GetTextureRegion(
            si_bordered->Atlas->MainTex, bordered_ox, bordered_oy, si_bordered->Width, si_bordered->Height);
    }

    // Normalize color to gray
    if (font.MakeGray)
    {
        for (uint y = 0; y < (uint)si->Height; y++)
        {
            for (uint x = 0; x < (uint)si->Width; x++)
            {
                uchar a = ((uchar*)&PIXEL_AT(data_normal, si->Width, x, y))[3];
                if (a)
                {
                    PIXEL_AT(data_normal, si->Width, x, y) = COLOR_RGBA(a, 128, 128, 128);
                    if (si_bordered)
                        PIXEL_AT(data_bordered, si_bordered->Width, x, y) = COLOR_RGBA(a, 128, 128, 128);
                }
                else
                {
                    PIXEL_AT(data_normal, si->Width, x, y) = COLOR_RGBA(0, 0, 0, 0);
                    if (si_bordered)
                        PIXEL_AT(data_bordered, si_bordered->Width, x, y) = COLOR_RGBA(0, 0, 0, 0);
                }
            }
        }

        Rect r = Rect(normal_ox, normal_oy, normal_ox + si->Width - 1, normal_oy + si->Height - 1);
        App::Render::UpdateTextureRegion(si->Atlas->MainTex, r, data_normal.data());
    }

    // Fill border
    if (si_bordered)
    {
        for (uint y = 1; y < (uint)si_bordered->Height - 2; y++)
        {
            for (uint x = 1; x < (uint)si_bordered->Width - 2; x++)
            {
                if (PIXEL_AT(data_normal, si->Width, x, y))
                {
                    for (int xx = -1; xx <= 1; xx++)
                    {
                        for (int yy = -1; yy <= 1; yy++)
                        {
                            uint ox = x + xx;
                            uint oy = y + yy;
                            if (!PIXEL_AT(data_bordered, si_bordered->Width, ox, oy))
                                PIXEL_AT(data_bordered, si_bordered->Width, ox, oy) = COLOR_RGB(0, 0, 0);
                        }
                    }
                }
            }
        }

        Rect r_bordered =
            Rect(bordered_ox, bordered_oy, bordered_ox + si_bordered->Width - 1, bordered_oy + si_bordered->Height - 1);
        App::Render::UpdateTextureRegion(si_bordered->Atlas->MainTex, r_bordered, data_bordered.data());

        // Fix texture coordinates on bordered texture
        tex_w = (float)si_bordered->Atlas->Width;
        tex_h = (float)si_bordered->Atlas->Height;
        image_x = tex_w * si_bordered->SprRect.L;
        image_y = tex_h * si_bordered->SprRect.T;
        for (auto it = font.Letters.begin(), end = font.Letters.end(); it != end; ++it)
        {
            FontData::Letter& l = it->second;
            float x = (float)l.PosX;
            float y = (float)l.PosY;
            float w = (float)l.W;
            float h = (float)l.H;
            l.TexBorderedUV[0] = (image_x + x - 1.0f) / tex_w;
            l.TexBorderedUV[1] = (image_y + y - 1.0f) / tex_h;
            l.TexBorderedUV[2] = (image_x + x + w + 1.0f) / tex_w;
            l.TexBorderedUV[3] = (image_y + y + h + 1.0f) / tex_h;
        }
    }

#undef PIXEL_AT
}

bool SpriteManager::LoadFontFO(int index, const string& font_name, bool not_bordered, bool skip_if_loaded /* = true */)
{
    // Skip if loaded
    if (skip_if_loaded && index < (int)allFonts.size() && allFonts[index])
        return true;

    // Load font data
    string fname = _str("Fonts/{}.fofnt", font_name);
    File file = fileMngr.ReadFile(fname);
    if (!file)
    {
        WriteLog("File '{}' not found.\n", fname);
        return false;
    }

    FontData font {effectMngr.Effects.Font};
    string image_name;

    // Parse data
    istringstream str((char*)file.GetBuf());
    string key;
    string letter_buf;
    FontData::Letter* cur_letter = nullptr;
    int version = -1;
    while (!str.eof() && !str.fail())
    {
        // Get key
        str >> key;

        // Cut off comments
        size_t comment = key.find('#');
        if (comment != string::npos)
            key.erase(comment);
        comment = key.find(';');
        if (comment != string::npos)
            key.erase(comment);

        // Check version
        if (version == -1)
        {
            if (key != "Version")
            {
                WriteLog("Font '{}' 'Version' signature not found (used deprecated format of 'fofnt').\n", fname);
                return false;
            }
            str >> version;
            if (version > 2)
            {
                WriteLog("Font '{}' version {} not supported (try update client).\n", fname, version);
                return false;
            }
            continue;
        }

        // Get value
        if (key == "Image")
        {
            str >> image_name;
        }
        else if (key == "LineHeight")
        {
            str >> font.LineHeight;
        }
        else if (key == "YAdvance")
        {
            str >> font.YAdvance;
        }
        else if (key == "End")
        {
            break;
        }
        else if (key == "Letter")
        {
            std::getline(str, letter_buf, '\n');
            size_t utf8_letter_begin = letter_buf.find('\'');
            if (utf8_letter_begin == string::npos)
            {
                WriteLog("Font '{}' invalid letter specification.\n", fname);
                return false;
            }
            utf8_letter_begin++;

            uint letter_len;
            uint letter = utf8::Decode(letter_buf.c_str() + utf8_letter_begin, &letter_len);
            if (!utf8::IsValid(letter))
            {
                WriteLog("Font '{}' invalid UTF8 letter at  '{}'.\n", fname, letter_buf);
                return false;
            }

            cur_letter = &font.Letters[letter];
        }

        if (!cur_letter)
            continue;

        if (key == "PositionX")
            str >> cur_letter->PosX;
        else if (key == "PositionY")
            str >> cur_letter->PosY;
        else if (key == "Width")
            str >> cur_letter->W;
        else if (key == "Height")
            str >> cur_letter->H;
        else if (key == "OffsetX")
            str >> cur_letter->OffsX;
        else if (key == "OffsetY")
            str >> cur_letter->OffsY;
        else if (key == "XAdvance")
            str >> cur_letter->XAdvance;
    }

    bool make_gray = false;
    if (image_name.back() == '*')
    {
        make_gray = true;
        image_name = image_name.substr(0, image_name.size() - 1);
    }
    font.MakeGray = make_gray;

    // Load image
    image_name.insert(0, "Fonts/");
    AnyFrames* image_normal = LoadAnimation(image_name);
    if (!image_normal)
    {
        WriteLog("Image file '{}' not found.\n", image_name);
        return false;
    }
    font.ImageNormal = image_normal;

    // Create bordered instance
    if (!not_bordered)
    {
        AnyFrames* image_bordered = LoadAnimation(image_name);
        if (!image_bordered)
        {
            WriteLog("Can't load twice file '{}'.\n", image_name);
            return false;
        }
        font.ImageBordered = image_bordered;
    }

    // Register
    if (index >= (int)allFonts.size())
        allFonts.resize(index + 1);
    allFonts[index] = std::make_unique<FontData>(font);

    return true;
}

bool SpriteManager::LoadFontBMF(int index, const string& font_name)
{
    if (index < 0)
    {
        WriteLog("Invalid index.\n");
        return false;
    }

    FontData font {effectMngr.Effects.Font};
    File file = fileMngr.ReadFile(_str("Fonts/{}.fnt", font_name));
    if (!file)
    {
        WriteLog("Font file '{}.fnt' not found.\n", font_name);
        return false;
    }

    uint signature = file.GetLEUInt();
    if (signature != MAKEUINT('B', 'M', 'F', 3))
    {
        WriteLog("Invalid signature of font '{}'.\n", font_name);
        return false;
    }

    // Info
    file.GetUChar();
    uint block_len = file.GetLEUInt();
    uint next_block = block_len + file.GetCurPos() + 1;

    file.GoForward(7);
    if (file.GetUChar() != 1 || file.GetUChar() != 1 || file.GetUChar() != 1 || file.GetUChar() != 1)
    {
        WriteLog("Wrong padding in font '{}'.\n", font_name);
        return false;
    }

    // Common
    file.SetCurPos(next_block);
    block_len = file.GetLEUInt();
    next_block = block_len + file.GetCurPos() + 1;

    int line_height = file.GetLEUShort();
    int base_height = file.GetLEUShort();
    file.GetLEUShort(); // Texture width
    file.GetLEUShort(); // Texture height

    if (file.GetLEUShort() != 1)
    {
        WriteLog("Texture for font '{}' must be one.\n", font_name);
        return false;
    }

    // Pages
    file.SetCurPos(next_block);
    block_len = file.GetLEUInt();
    next_block = block_len + file.GetCurPos() + 1;

    // Image name
    string image_name = file.GetStrNT();
    image_name.insert(0, "Fonts/");

    // Chars
    file.SetCurPos(next_block);
    int count = file.GetLEUInt() / 20;
    for (int i = 0; i < count; i++)
    {
        // Read data
        uint id = file.GetLEUInt();
        int x = file.GetLEUShort();
        int y = file.GetLEUShort();
        int w = file.GetLEUShort();
        int h = file.GetLEUShort();
        int ox = file.GetLEUShort();
        int oy = file.GetLEUShort();
        int xa = file.GetLEUShort();
        file.GoForward(2);

        // Fill data
        FontData::Letter& let = font.Letters[id];
        let.PosX = x + 1;
        let.PosY = y + 1;
        let.W = w - 2;
        let.H = h - 2;
        let.OffsX = -ox;
        let.OffsY = -oy + (line_height - base_height);
        let.XAdvance = xa + 1;
    }

    font.LineHeight = (font.Letters.count('W') ? font.Letters['W'].H : base_height);
    font.YAdvance = font.LineHeight / 2;
    font.MakeGray = true;

    // Load image
    AnyFrames* image_normal = LoadAnimation(image_name);
    if (!image_normal)
    {
        WriteLog("Image file '{}' not found.\n", image_name);
        return false;
    }
    font.ImageNormal = image_normal;

    // Create bordered instance
    AnyFrames* image_bordered = LoadAnimation(image_name);
    if (!image_bordered)
    {
        WriteLog("Can't load twice file '{}'.\n", image_name);
        return false;
    }
    font.ImageBordered = image_bordered;

    // Register
    if (index >= (int)allFonts.size())
        allFonts.resize(index + 1);
    allFonts[index] = std::make_unique<FontData>(font);

    return true;
}

static void Str_GoTo(char*& str, char ch, bool skip_char = false)
{
    while (*str && *str != ch)
        ++str;
    if (skip_char && *str)
        ++str;
}

static void Str_EraseInterval(char* str, uint len)
{
    if (!str || !len)
        return;

    char* str2 = str + len;
    while (*str2)
    {
        *str = *str2;
        ++str;
        ++str2;
    }

    *str = 0;
}

static void Str_Insert(char* to, const char* from, uint from_len)
{
    if (!to || !from)
        return;

    if (!from_len)
        from_len = (uint)strlen(from);
    if (!from_len)
        return;

    char* end_to = to;
    while (*end_to)
        ++end_to;

    for (; end_to >= to; --end_to)
        *(end_to + from_len) = *end_to;

    while (from_len--)
    {
        *to = *from;
        ++to;
        ++from;
    }
}

void SpriteManager::FormatText(FontFormatInfo& fi, int fmt_type)
{
    char* str = fi.PStr;
    uint flags = fi.Flags;
    FontData* font = fi.CurFont;
    Rect& r = fi.Region;
    bool infinity_w = (r.L == r.R);
    bool infinity_h = (r.T == r.B);
    int curx = 0;
    int cury = 0;
    uint& offs_col = fi.OffsColDots;

    if (fmt_type != FORMAT_TYPE_DRAW && fmt_type != FORMAT_TYPE_LCOUNT && fmt_type != FORMAT_TYPE_SPLIT)
    {
        fi.IsError = true;
        return;
    }

    if (fmt_type == FORMAT_TYPE_SPLIT && !fi.StrLines)
    {
        fi.IsError = true;
        return;
    }

    // Colorize
    uint* dots = nullptr;
    uint d_offs = 0;
    char* str_ = str;
    string buf;
    if (fmt_type == FORMAT_TYPE_DRAW && !FLAG(flags, FT_NO_COLORIZE))
        dots = fi.ColorDots;

    while (*str_)
    {
        char* s0 = str_;
        Str_GoTo(str_, '|');
        char* s1 = str_;
        Str_GoTo(str_, ' ');
        char* s2 = str_;

        // TODO: optimize
        // if(!_str[0] && !*s1) break;
        if (dots)
        {
            size_t d_len = (uint)((size_t)s2 - (size_t)s1) + 1;
            uint d = (uint)strtoul(s1 + 1, nullptr, 0);

            dots[(uint)((size_t)s1 - (size_t)str) - d_offs] = d;
            d_offs += (uint)d_len;
        }

        *s1 = 0;
        buf += s0;

        if (!*str_)
            break;
        str_++;
    }

    Str::Copy(str, FONT_BUF_LEN, buf.c_str());

    // Skip lines
    uint skip_line = (FLAG(flags, FT_SKIPLINES(0)) ? flags >> 16 : 0);
    uint skip_line_end = (FLAG(flags, FT_SKIPLINES_END(0)) ? flags >> 16 : 0);

    // Format
    curx = r.L;
    cury = r.T;
    for (int i = 0, i_advance = 1; str[i]; i += i_advance)
    {
        uint letter_len;
        uint letter = utf8::Decode(&str[i], &letter_len);
        i_advance = letter_len;

        int x_advance;
        switch (letter)
        {
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
            if (it != font->Letters.end())
                x_advance = it->second.XAdvance;
            else
                x_advance = 0;
            break;
        }

        if (!infinity_w && curx + x_advance > r.R)
        {
            if (curx > fi.MaxCurX)
                fi.MaxCurX = curx;

            if (fmt_type == FORMAT_TYPE_DRAW && FLAG(flags, FT_NOBREAK))
            {
                str[i] = 0;
                break;
            }
            else if (FLAG(flags, FT_NOBREAK_LINE))
            {
                int j = i;
                for (; str[j]; j++)
                    if (str[j] == '\n')
                        break;

                Str_EraseInterval(&str[i], j - i);
                letter = str[i];
                i_advance = 1;
                if (fmt_type == FORMAT_TYPE_DRAW)
                    for (int k = i, l = MAX_FOTEXT - (j - i); k < l; k++)
                        fi.ColorDots[k] = fi.ColorDots[k + (j - i)];
            }
            else if (str[i] != '\n')
            {
                int j = i;
                for (; j >= 0; j--)
                {
                    if (str[j] == ' ' || str[j] == '\t')
                    {
                        str[j] = '\n';
                        i = j;
                        letter = '\n';
                        i_advance = 1;
                        break;
                    }
                    else if (str[j] == '\n')
                    {
                        j = -1;
                        break;
                    }
                }

                if (j < 0)
                {
                    letter = '\n';
                    i_advance = 1;
                    Str_Insert(&str[i], "\n", 0);
                    if (fmt_type == FORMAT_TYPE_DRAW)
                        for (int k = MAX_FOTEXT - 1; k > i; k--)
                            fi.ColorDots[k] = fi.ColorDots[k - 1];
                }

                if (FLAG(flags, FT_ALIGN) && !skip_line)
                {
                    fi.LineSpaceWidth[fi.LinesAll - 1] = 1;
                    // Erase next first spaces
                    int ii = i + i_advance;
                    for (j = ii;; j++)
                        if (str[j] != ' ')
                            break;
                    if (j > ii)
                    {
                        Str_EraseInterval(&str[ii], j - ii);
                        if (fmt_type == FORMAT_TYPE_DRAW)
                            for (int k = ii, l = MAX_FOTEXT - (j - ii); k < l; k++)
                                fi.ColorDots[k] = fi.ColorDots[k + (j - ii)];
                    }
                }
            }
        }

        switch (letter)
        {
        case '\n':
            if (!skip_line)
            {
                cury += font->LineHeight + font->YAdvance;
                if (!infinity_h && cury + font->LineHeight > r.B && !fi.LinesInRect)
                    fi.LinesInRect = fi.LinesAll;

                if (fmt_type == FORMAT_TYPE_DRAW)
                {
                    if (fi.LinesInRect && !FLAG(flags, FT_UPPER))
                    {
                        // fi.LinesAll++;
                        str[i] = 0;
                        break;
                    }
                }
                else if (fmt_type == FORMAT_TYPE_SPLIT)
                {
                    if (fi.LinesInRect && !(fi.LinesAll % fi.LinesInRect))
                    {
                        str[i] = 0;
                        (*fi.StrLines).push_back(str);
                        str = &str[i + i_advance];
                        i = -i_advance;
                    }
                }

                if (str[i + i_advance])
                    fi.LinesAll++;
            }
            else
            {
                skip_line--;
                Str_EraseInterval(str, i + i_advance);
                offs_col += i + i_advance;
                //	if(fmt_type==FORMAT_TYPE_DRAW)
                //		for(int k=0,l=MAX_FOTEXT-(i+1);k<l;k++) fi.ColorDots[k]=fi.ColorDots[k+i+1];
                i = 0;
                i_advance = 0;
            }

            if (curx > fi.MaxCurX)
                fi.MaxCurX = curx;
            curx = r.L;
            continue;
        case 0:
            break;
        default:
            curx += x_advance;
            continue;
        }

        if (!str[i])
            break;
    }
    if (curx > fi.MaxCurX)
        fi.MaxCurX = curx;

    if (skip_line_end)
    {
        int len = (int)strlen(str);
        for (int i = len - 2; i >= 0; i--)
        {
            if (str[i] == '\n')
            {
                str[i] = 0;
                fi.LinesAll--;
                if (!--skip_line_end)
                    break;
            }
        }

        if (skip_line_end)
        {
            WriteLog("error3\n");
            fi.IsError = true;
            return;
        }
    }

    if (skip_line)
    {
        WriteLog("error4\n");
        fi.IsError = true;
        return;
    }

    if (!fi.LinesInRect)
        fi.LinesInRect = fi.LinesAll;

    if (fi.LinesAll > FONT_MAX_LINES)
    {
        WriteLog("error5 {}\n", fi.LinesAll);
        fi.IsError = true;
        return;
    }

    if (fmt_type == FORMAT_TYPE_SPLIT)
    {
        (*fi.StrLines).push_back(string(str));
        return;
    }
    else if (fmt_type == FORMAT_TYPE_LCOUNT)
    {
        return;
    }

    // Up text
    if (FLAG(flags, FT_UPPER) && fi.LinesAll > fi.LinesInRect)
    {
        uint j = 0;
        uint line_cur = 0;
        uint last_col = 0;
        for (; str[j]; ++j)
        {
            if (str[j] == '\n')
            {
                line_cur++;
                if (line_cur >= (fi.LinesAll - fi.LinesInRect))
                    break;
            }

            if (fi.ColorDots[j])
                last_col = fi.ColorDots[j];
        }

        if (!FLAG(flags, FT_NO_COLORIZE))
        {
            offs_col += j + 1;
            if (last_col && !fi.ColorDots[j + 1])
                fi.ColorDots[j + 1] = last_col;
        }

        str = &str[j + 1];
        fi.PStr = str;

        fi.LinesAll = fi.LinesInRect;
    }

    // Align
    curx = r.L;
    cury = r.T;

    for (uint i = 0; i < fi.LinesAll; i++)
        fi.LineWidth[i] = curx;

    bool can_count = false;
    int spaces = 0;
    int curstr = 0;
    for (int i = 0, i_advance = 1;; i += i_advance)
    {
        uint letter_len;
        uint letter = utf8::Decode(&str[i], &letter_len);
        i_advance = letter_len;

        switch (letter)
        {
        case ' ':
            curx += font->SpaceWidth;
            if (can_count)
                spaces++;
            break;
        case '\t':
            curx += font->SpaceWidth * 4;
            break;
        case 0:
        case '\n':
            fi.LineWidth[curstr] = curx;
            cury += font->LineHeight + font->YAdvance;
            curx = r.L;

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
            if (fi.LineSpaceWidth[curstr] == 1 && spaces > 0 && !infinity_w)
                fi.LineSpaceWidth[curstr] = font->SpaceWidth + (r.R - fi.LineWidth[curstr]) / spaces;
            else
                fi.LineSpaceWidth[curstr] = font->SpaceWidth;

            curstr++;
            can_count = false;
            spaces = 0;
            break;
        case '\r':
            break;
        default:
            auto it = font->Letters.find(letter);
            if (it != font->Letters.end())
                curx += it->second.XAdvance;
            // if(curx>fi.LineWidth[curstr]) fi.LineWidth[curstr]=curx;
            can_count = true;
            break;
        }

        if (!str[i])
            break;
    }

    // Initial position
    fi.CurX = r.L;
    fi.CurY = r.T;

    // Align X
    if (FLAG(flags, FT_CENTERX))
        fi.CurX += (r.R - fi.LineWidth[0]) / 2;
    else if (FLAG(flags, FT_CENTERR))
        fi.CurX += r.R - fi.LineWidth[0];
    // Align Y
    if (FLAG(flags, FT_CENTERY))
        fi.CurY = r.T +
            (int)((r.B - r.T + 1) - fi.LinesInRect * font->LineHeight - (fi.LinesInRect - 1) * font->YAdvance) / 2 +
            (FLAG(flags, FT_CENTERY_ENGINE) ? 1 : 0);
    else if (FLAG(flags, FT_BOTTOM))
        fi.CurY = r.B - (int)(fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance);
}

bool SpriteManager::DrawStr(const Rect& r, const string& str, uint flags, uint color /* = 0 */, int num_font /* = -1 */)
{
    // Check
    if (str.empty())
        return false;

    // Get font
    FontData* font = GetFont(num_font);
    if (!font)
        return false;

    // FormatBuf
    if (!color && defFontColor)
        color = defFontColor;
    color = COLOR_SWAP_RB(color);

    FontFormatInfo fi {font, flags, r};
    Str::Copy(fi.Str, str.c_str());
    fi.DefColor = color;
    FormatText(fi, FORMAT_TYPE_DRAW);
    if (fi.IsError)
        return false;

    char* str_ = fi.PStr;
    uint offs_col = fi.OffsColDots;
    int curx = fi.CurX;
    int cury = fi.CurY;
    int curstr = 0;
    RenderTexture* texture =
        (FLAG(flags, FT_BORDERED) && font->FontTexBordered ? font->FontTexBordered : font->FontTex);

    if (curDrawQuad)
        Flush();

    if (!FLAG(flags, FT_NO_COLORIZE))
    {
        for (int i = offs_col; i >= 0; i--)
        {
            if (fi.ColorDots[i])
            {
                if (fi.ColorDots[i] & 0xFF000000)
                    color = fi.ColorDots[i]; // With alpha
                else
                    color = (color & 0xFF000000) | (fi.ColorDots[i] & 0x00FFFFFF); // Still old alpha
                color = COLOR_SWAP_RB(color);
                break;
            }
        }
    }

    bool variable_space = false;
    for (int i = 0, i_advance = 1; str_[i]; i += i_advance)
    {
        if (!FLAG(flags, FT_NO_COLORIZE))
        {
            uint new_color = fi.ColorDots[i + offs_col];
            if (new_color)
            {
                if (new_color & 0xFF000000)
                    color = new_color; // With alpha
                else
                    color = (color & 0xFF000000) | (new_color & 0x00FFFFFF); // Still old alpha
                color = COLOR_SWAP_RB(color);
            }
        }

        uint letter_len;
        uint letter = utf8::Decode(&str_[i], &letter_len);
        i_advance = letter_len;

        switch (letter)
        {
        case ' ':
            curx += (variable_space ? fi.LineSpaceWidth[curstr] : font->SpaceWidth);
            continue;
        case '\t':
            curx += font->SpaceWidth * 4;
            continue;
        case '\n':
            cury += font->LineHeight + font->YAdvance;
            curx = r.L;
            curstr++;
            variable_space = false;
            if (FLAG(flags, FT_CENTERX))
                curx += (r.R - fi.LineWidth[curstr]) / 2;
            else if (FLAG(flags, FT_CENTERR))
                curx += r.R - fi.LineWidth[curstr];
            continue;
        case '\r':
            continue;
        default:
            auto it = font->Letters.find(letter);
            if (it == font->Letters.end())
                continue;

            FontData::Letter& l = it->second;

            int pos = curDrawQuad * 4;
            int x = curx - l.OffsX - 1;
            int y = cury - l.OffsY - 1;
            int w = l.W + 2;
            int h = l.H + 2;

            RectF& texture_uv = (FLAG(flags, FT_BORDERED) ? l.TexBorderedUV : l.TexUV);
            float x1 = texture_uv[0];
            float y1 = texture_uv[1];
            float x2 = texture_uv[2];
            float y2 = texture_uv[3];

            vBuffer[pos].X = (float)x;
            vBuffer[pos].Y = (float)y + h;
            vBuffer[pos].TU = x1;
            vBuffer[pos].TV = y2;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = (float)x;
            vBuffer[pos].Y = (float)y;
            vBuffer[pos].TU = x1;
            vBuffer[pos].TV = y1;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = (float)x + w;
            vBuffer[pos].Y = (float)y;
            vBuffer[pos].TU = x2;
            vBuffer[pos].TV = y1;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = (float)x + w;
            vBuffer[pos].Y = (float)y + h;
            vBuffer[pos].TU = x2;
            vBuffer[pos].TV = y2;
            vBuffer[pos].Diffuse = color;

            if (++curDrawQuad == drawQuadCount)
            {
                dipQueue.push_back({texture, font->DrawEffect, 1});
                dipQueue.back().SpritesCount = curDrawQuad;
                Flush();
            }

            curx += l.XAdvance;
            variable_space = true;
        }
    }

    if (curDrawQuad)
    {
        dipQueue.push_back({texture, font->DrawEffect, 1});
        dipQueue.back().SpritesCount = curDrawQuad;
        Flush();
    }

    return true;
}

int SpriteManager::GetLinesCount(int width, int height, const string& str, int num_font /* = -1 */)
{
    if (width <= 0 || height <= 0)
        return 0;

    FontData* font = GetFont(num_font);
    if (!font)
        return 0;

    if (str.empty())
        return height / (font->LineHeight + font->YAdvance);

    Rect r = Rect(0, 0, width ? width : settings.ScreenWidth, height ? height : settings.ScreenHeight);
    FontFormatInfo fi {font, 0, r};
    Str::Copy(fi.Str, str.c_str());
    FormatText(fi, FORMAT_TYPE_LCOUNT);
    if (fi.IsError)
        return 0;

    return fi.LinesInRect;
}

int SpriteManager::GetLinesHeight(int width, int height, const string& str, int num_font /* = -1 */)
{
    if (width <= 0 || height <= 0)
        return 0;

    FontData* font = GetFont(num_font);
    if (!font)
        return 0;

    int cnt = GetLinesCount(width, height, str, num_font);
    if (cnt <= 0)
        return 0;

    return cnt * font->LineHeight + (cnt - 1) * font->YAdvance;
}

int SpriteManager::GetLineHeight(int num_font)
{
    FontData* font = GetFont(num_font);
    if (!font)
        return 0;

    return font->LineHeight;
}

void SpriteManager::GetTextInfo(
    int width, int height, const string& str, int num_font, uint flags, int& tw, int& th, int& lines)
{
    tw = th = lines = 0;

    FontData* font = GetFont(num_font);
    if (!font)
        return;

    if (str.empty())
    {
        tw = width;
        th = height;
        lines = height / (font->LineHeight + font->YAdvance);
        return;
    }

    FontFormatInfo fi {font, flags, Rect(0, 0, width, height)};
    Str::Copy(fi.Str, str.c_str());
    FormatText(fi, FORMAT_TYPE_LCOUNT);
    if (fi.IsError)
        return;

    lines = fi.LinesInRect;
    th = fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance;
    tw = fi.MaxCurX - fi.Region.L;
}

int SpriteManager::SplitLines(const Rect& r, const string& cstr, int num_font, StrVec& str_vec)
{
    str_vec.clear();
    if (cstr.empty())
        return 0;

    FontData* font = GetFont(num_font);
    if (!font)
        return 0;

    FontFormatInfo fi {font, 0, r};
    Str::Copy(fi.Str, cstr.c_str());
    fi.StrLines = &str_vec;
    FormatText(fi, FORMAT_TYPE_SPLIT);
    if (fi.IsError)
        return 0;

    return (int)str_vec.size();
}

bool SpriteManager::HaveLetter(int num_font, uint letter)
{
    FontData* font = GetFont(num_font);
    if (!font)
        return false;
    return font->Letters.count(letter) > 0;
}
