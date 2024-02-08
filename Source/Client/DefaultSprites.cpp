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

#include "DefaultSprites.h"
#include "GenericUtils.h"
#include "GeometryHelper.h"

AtlasSprite::AtlasSprite(SpriteManager& spr_mngr) :
    Sprite(spr_mngr)
{
}

AtlasSprite::~AtlasSprite()
{
    STACK_TRACE_ENTRY();

#if 0 // For debug purposes
    if constexpr (FO_DEBUG) {
        try {
            const auto rnd_color = COLOR_RGB(GenericUtils::Random(0, 255), GenericUtils::Random(0, 255), GenericUtils::Random(0, 255));

            vector<uint> color_data;
            color_data.resize(static_cast<size_t>(AtlasNode->Width) * AtlasNode->Height);
            for (size_t i = 0; i < color_data.size(); i++) {
                color_data[i] = rnd_color;
            }

            Atlas->MainTex->UpdateTextureRegion({AtlasNode->PosX, AtlasNode->PosY, AtlasNode->PosX + AtlasNode->Width, AtlasNode->PosY + AtlasNode->Height}, color_data.data());
        }
        catch (...) {
        }
    }
#endif

    if (AtlasNode != nullptr) {
        AtlasNode->Free();
    }
}

auto AtlasSprite::IsHitTest(int x, int y) const -> bool
{
    STACK_TRACE_ENTRY();

    if (x < 0 || y < 0 || x >= Width || y >= Height) {
        return false;
    }

    if (!HitTestData.empty()) {
        return HitTestData[y * Width + x];
    }
    else {
        return false;
    }
}

auto AtlasSprite::MakeCopy() const -> shared_ptr<Sprite>
{
    return const_cast<AtlasSprite*>(this)->shared_from_this();
}

auto AtlasSprite::FillData(RenderDrawBuffer* dbuf, const FRect& pos, const tuple<ucolor, ucolor>& colors) const -> size_t
{
    STACK_TRACE_ENTRY();

    dbuf->CheckAllocBuf(4, 6);

    auto& vbuf = dbuf->Vertices;
    auto& vpos = dbuf->VertCount;
    auto& ibuf = dbuf->Indices;
    auto& ipos = dbuf->IndCount;

    ibuf[ipos++] = static_cast<vindex_t>(vpos + 0);
    ibuf[ipos++] = static_cast<vindex_t>(vpos + 1);
    ibuf[ipos++] = static_cast<vindex_t>(vpos + 3);
    ibuf[ipos++] = static_cast<vindex_t>(vpos + 1);
    ibuf[ipos++] = static_cast<vindex_t>(vpos + 2);
    ibuf[ipos++] = static_cast<vindex_t>(vpos + 3);

    auto& v0 = vbuf[vpos++];
    v0.PosX = pos.Left;
    v0.PosY = pos.Bottom;
    v0.TexU = AtlasRect.Left;
    v0.TexV = AtlasRect.Bottom;
    v0.EggTexU = 0.0f;
    v0.Color = std::get<0>(colors);

    auto& v1 = vbuf[vpos++];
    v1.PosX = pos.Left;
    v1.PosY = pos.Top;
    v1.TexU = AtlasRect.Left;
    v1.TexV = AtlasRect.Top;
    v1.EggTexU = 0.0f;
    v1.Color = std::get<0>(colors);

    auto& v2 = vbuf[vpos++];
    v2.PosX = pos.Right;
    v2.PosY = pos.Top;
    v2.TexU = AtlasRect.Right;
    v2.TexV = AtlasRect.Top;
    v2.EggTexU = 0.0f;
    v2.Color = std::get<1>(colors);

    auto& v3 = vbuf[vpos++];
    v3.PosX = pos.Right;
    v3.PosY = pos.Bottom;
    v3.TexU = AtlasRect.Right;
    v3.TexV = AtlasRect.Bottom;
    v3.EggTexU = 0.0f;
    v3.Color = std::get<1>(colors);

    return 6;
}

SpriteSheet::SpriteSheet(SpriteManager& spr_mngr, uint frames, uint ticks, uint dirs) :
    Sprite(spr_mngr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(frames > 0);
    RUNTIME_ASSERT(dirs == 1 || dirs == GameSettings::MAP_DIR_COUNT);

    Spr.resize(frames);
    SprOffset.resize(frames);
    CntFrm = frames;
    WholeTicks = ticks != 0 ? ticks : frames * 100;

    DirCount = dirs;

    for (uint dir = 0; dir < dirs - 1; dir++) {
        Dirs[dir] = std::make_shared<SpriteSheet>(_sprMngr, frames, ticks, 1);
    }
}

auto SpriteSheet::IsHitTest(int x, int y) const -> bool
{
    STACK_TRACE_ENTRY();

    return GetCurSpr()->IsHitTest(x, y);
}

auto SpriteSheet::GetBatchTex() const -> RenderTexture*
{
    STACK_TRACE_ENTRY();

    return GetCurSpr()->GetBatchTex();
}

auto SpriteSheet::GetCurSpr() const -> const Sprite*
{
    return const_cast<SpriteSheet*>(this)->GetCurSpr();
}

auto SpriteSheet::GetCurSpr() -> Sprite*
{
    const auto* dir_sheet = _curDir == 0 || !Dirs[_curDir - 1] ? this : Dirs[_curDir - 1].get();
    return dir_sheet->Spr[_curIndex].get();
}

auto SpriteSheet::MakeCopy() const -> shared_ptr<Sprite>
{
    STACK_TRACE_ENTRY();

    auto&& copy = std::make_shared<SpriteSheet>(_sprMngr, CntFrm, WholeTicks, DirCount);

    copy->Width = Width;
    copy->Height = Height;
    copy->OffsX = OffsX;
    copy->OffsY = OffsY;

    for (size_t i = 0; i < Spr.size(); i++) {
        copy->Spr[i] = Spr[i]->MakeCopy();
        copy->SprOffset[i] = SprOffset[i];
        copy->StateAnim = StateAnim;
        copy->ActionAnim = ActionAnim;
    }

    for (uint i = 0; i < DirCount - 1; i++) {
        if (Dirs[i]) {
            copy->Dirs[i] = dynamic_pointer_cast<SpriteSheet>(Dirs[i]->MakeCopy());
        }
    }

    return copy;
}

auto SpriteSheet::FillData(RenderDrawBuffer* dbuf, const FRect& pos, const tuple<ucolor, ucolor>& colors) const -> size_t
{
    STACK_TRACE_ENTRY();

    const auto* dir_sheet = _curDir == 0 || !Dirs[_curDir - 1] ? this : Dirs[_curDir - 1].get();
    const auto* spr = dir_sheet->Spr[_curIndex].get();

    return spr->FillData(dbuf, pos, colors);
}

void SpriteSheet::Prewarm()
{
    STACK_TRACE_ENTRY();

    _curIndex = GenericUtils::Random(0u, CntFrm - 1);

    RefreshParams();
}

void SpriteSheet::SetTime(float normalized_time)
{
    STACK_TRACE_ENTRY();

    _curIndex = CntFrm > 1 ? iround(normalized_time * static_cast<float>(CntFrm - 1)) : 0;

    RefreshParams();
}

void SpriteSheet::SetDir(uint8 dir)
{
    STACK_TRACE_ENTRY();

    _curDir = dir;
}

void SpriteSheet::SetDirAngle(short dir_angle)
{
    STACK_TRACE_ENTRY();

    _curDir = GeometryHelper::AngleToDir(dir_angle);
}

void SpriteSheet::Play(hstring anim_name, bool looped, bool reversed)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(anim_name);

    _playing = true;
    _looped = looped;
    _reversed = reversed;
    _startTick = _sprMngr.GetTimer().GetTime(_useGameplayTimer);

    StartUpdate();
}

void SpriteSheet::Stop()
{
    STACK_TRACE_ENTRY();

    _playing = false;
}

auto SpriteSheet::Update() -> bool
{
    STACK_TRACE_ENTRY();

    if (_playing) {
        const auto cur_tick = _sprMngr.GetTimer().GetTime(_useGameplayTimer);
        const auto dt = time_duration_to_ms<int>(cur_tick - _startTick);
        const auto frm_count = static_cast<int>(CntFrm);
        const auto ticks_per_frame = static_cast<int>(WholeTicks) / frm_count;
        const auto frames_passed = dt / ticks_per_frame;

        if (frames_passed > 0) {
            _startTick += std::chrono::milliseconds {frames_passed * ticks_per_frame};

            auto index = static_cast<int>(_curIndex) + (_reversed ? -frames_passed : frames_passed);

            if (_looped) {
                if (index < 0 || index >= frm_count) {
                    index %= frm_count;
                }
            }
            else {
                if (index < 0) {
                    index = 0;
                    _playing = false;
                }
                else if (index >= frm_count) {
                    index = frm_count - 1;
                    _playing = false;
                }
            }

            RUNTIME_ASSERT(index >= 0 && index < frm_count);
            _curIndex = static_cast<uint>(index);

            RefreshParams();
        }

        return _playing;
    }

    return false;
}

void SpriteSheet::RefreshParams()
{
    STACK_TRACE_ENTRY();

    const auto* cur_spr = GetCurSpr();

    Width = cur_spr->Width;
    Height = cur_spr->Height;
    OffsX = cur_spr->OffsX;
    OffsY = cur_spr->OffsY;
}

auto SpriteSheet::GetSpr(uint num_frm) const -> const Sprite*
{
    STACK_TRACE_ENTRY();

    return Spr[num_frm % CntFrm].get();
}

auto SpriteSheet::GetSpr(uint num_frm) -> Sprite*
{
    STACK_TRACE_ENTRY();

    return Spr[num_frm % CntFrm].get();
}

auto SpriteSheet::GetDir(uint dir) const -> const SpriteSheet*
{
    STACK_TRACE_ENTRY();

    return dir == 0 || DirCount == 1 ? this : Dirs[dir - 1].get();
}

auto SpriteSheet::GetDir(uint dir) -> SpriteSheet*
{
    STACK_TRACE_ENTRY();

    return dir == 0 || DirCount == 1 ? this : Dirs[dir - 1].get();
}

DefaultSpriteFactory::DefaultSpriteFactory(SpriteManager& spr_mngr) :
    _sprMngr {spr_mngr}
{
    STACK_TRACE_ENTRY();

    _borderBuf.resize(AppRender::MAX_ATLAS_SIZE);
}

auto DefaultSpriteFactory::LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite>
{
    STACK_TRACE_ENTRY();

    auto file = _sprMngr.GetResources().ReadFile(path);
    if (!file) {
        BreakIntoDebugger();
        return nullptr;
    }

    const auto check_number = file.GetUChar();
    RUNTIME_ASSERT(check_number == 42);
    const auto frames_count = file.GetLEUShort();
    RUNTIME_ASSERT(frames_count != 0);
    const auto ticks = file.GetLEUShort();
    const auto dirs = file.GetUChar();
    RUNTIME_ASSERT(dirs != 0);

    if (frames_count > 1 || dirs > 1) {
        auto&& anim = std::make_shared<SpriteSheet>(_sprMngr, frames_count, ticks, dirs);

        for (uint16 dir = 0; dir < dirs; dir++) {
            auto* dir_anim = anim->GetDir(dir);
            const auto ox = file.GetLEShort();
            const auto oy = file.GetLEShort();

            dir_anim->OffsX = ox;
            dir_anim->OffsY = oy;

            for (uint16 i = 0; i < frames_count; i++) {
                const auto is_spr_ref = file.GetUChar();
                if (is_spr_ref == 0) {
                    const auto width = file.GetLEUShort();
                    const auto height = file.GetLEUShort();
                    const auto nx = file.GetLEShort();
                    const auto ny = file.GetLEShort();
                    const auto* data = file.GetCurBuf();

                    auto&& spr = std::make_shared<AtlasSprite>(_sprMngr);

                    spr->Width = width;
                    spr->Height = height;
                    spr->OffsX = ox;
                    spr->OffsY = oy;
                    dir_anim->SprOffset[i].X = nx;
                    dir_anim->SprOffset[i].Y = ny;

                    FillAtlas(spr.get(), atlas_type, reinterpret_cast<const ucolor*>(data));

                    if (i == 0) {
                        dir_anim->Width = width;
                        dir_anim->Height = height;
                    }

                    dir_anim->Spr[i] = spr;

                    file.GoForward(static_cast<size_t>(width) * height * 4);
                }
                else {
                    const auto index = file.GetLEUShort();

                    dir_anim->Spr[i] = dir_anim->GetSpr(index)->MakeCopy();
                    dir_anim->SprOffset[i] = dir_anim->SprOffset[index];
                }
            }
        }

        const auto check_number2 = file.GetUChar();
        RUNTIME_ASSERT(check_number2 == 42);

        return anim;
    }
    else {
        const auto ox = file.GetLEShort();
        const auto oy = file.GetLEShort();

        const auto is_spr_ref = file.GetUChar();
        RUNTIME_ASSERT(is_spr_ref == 0);

        const auto width = file.GetLEUShort();
        const auto height = file.GetLEUShort();
        const auto nx = file.GetLEShort();
        const auto ny = file.GetLEShort();
        const auto* data = file.GetCurBuf();

        UNUSED_VARIABLE(nx);
        UNUSED_VARIABLE(ny);

        auto&& spr = std::make_shared<AtlasSprite>(_sprMngr);

        spr->Width = width;
        spr->Height = height;
        spr->OffsX = ox;
        spr->OffsY = oy;

        FillAtlas(spr.get(), atlas_type, reinterpret_cast<const ucolor*>(data));

        file.GoForward(static_cast<size_t>(width) * height * 4);

        const auto check_number2 = file.GetUChar();
        RUNTIME_ASSERT(check_number2 == 42);

        return spr;
    }
}

void DefaultSpriteFactory::FillAtlas(AtlasSprite* atlas_spr, AtlasType atlas_type, const ucolor* data)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(atlas_spr);

    const auto width = atlas_spr->Width;
    const auto height = atlas_spr->Height;

    RUNTIME_ASSERT(width > 0);
    RUNTIME_ASSERT(height > 0);

    int x = 0;
    int y = 0;
    auto&& [atlas, atlas_node] = _sprMngr.GetAtlasMngr().FindAtlasPlace(atlas_type, width, height, x, y);

    // Refresh texture
    if (data != nullptr) {
        auto* tex = atlas->MainTex;
        tex->UpdateTextureRegion(IRect(x, y, x + width, y + height), data);

        // 1px border for correct linear interpolation
        // Top
        tex->UpdateTextureRegion(IRect(x, y - 1, x + width, y), data);

        // Bottom
        tex->UpdateTextureRegion(IRect(x, y + height, x + width, y + height + 1), data + static_cast<size_t>(height - 1) * width);

        // Left
        for (int i = 0; i < height; i++) {
            _borderBuf[i + 1] = *(data + static_cast<size_t>(i) * width);
        }
        _borderBuf[0] = _borderBuf[1];
        _borderBuf[height + 1] = _borderBuf[height];
        tex->UpdateTextureRegion(IRect(x - 1, y - 1, x, y + height + 1), _borderBuf.data());

        // Right
        for (int i = 0; i < height; i++) {
            _borderBuf[i + 1] = *(data + static_cast<size_t>(i) * width + (width - 1));
        }
        _borderBuf[0] = _borderBuf[1];
        _borderBuf[height + 1] = _borderBuf[height];
        tex->UpdateTextureRegion(IRect(x + width, y - 1, x + width + 1, y + height + 1), _borderBuf.data());

        // Evaluate hit mask
        atlas_spr->HitTestData.resize(static_cast<size_t>(width) * height);
        for (size_t i = 0, j = static_cast<size_t>(width) * height; i < j; i++) {
            atlas_spr->HitTestData[i] = data[i].comp.a > 0;
        }
    }

    // Invalidate last pixel color picking
    if (!atlas->RTarg->LastPixelPicks.empty()) {
        atlas->RTarg->LastPixelPicks.clear();
    }

    // Set parameters
    atlas_spr->Atlas = atlas;
    atlas_spr->AtlasNode = atlas_node;
    atlas_spr->AtlasRect.Left = static_cast<float>(x) / static_cast<float>(atlas->Width);
    atlas_spr->AtlasRect.Top = static_cast<float>(y) / static_cast<float>(atlas->Height);
    atlas_spr->AtlasRect.Right = static_cast<float>(x + width) / static_cast<float>(atlas->Width);
    atlas_spr->AtlasRect.Bottom = static_cast<float>(y + height) / static_cast<float>(atlas->Height);
}
