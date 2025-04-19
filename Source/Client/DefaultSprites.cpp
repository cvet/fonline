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

#include "DefaultSprites.h"
#include "GenericUtils.h"
#include "GeometryHelper.h"

AtlasSprite::AtlasSprite(SpriteManager& spr_mngr) :
    Sprite(spr_mngr)
{
    STACK_TRACE_ENTRY();
}

AtlasSprite::~AtlasSprite()
{
    STACK_TRACE_ENTRY();

#if 0 // For debug purposes
    if constexpr (FO_DEBUG) {
        try {
            const auto rnd_color = ucolor {static_cast<uint8>(GenericUtils::Random(0, 255)), static_cast<uint8>(GenericUtils::Random(0, 255)), static_cast<uint8>(GenericUtils::Random(0, 255))};

            vector<ucolor> color_data;
            color_data.resize(AtlasNode->Size.GetSquare());

            for (size_t i = 0; i < color_data.size(); i++) {
                color_data[i] = rnd_color;
            }

            Atlas->MainTex->UpdateTextureRegion(AtlasNode->Pos, AtlasNode->Size, color_data.data());
        }
        catch (...) {
        }
    }
#endif

    if (AtlasNode != nullptr) {
        AtlasNode->Free();
    }
}

auto AtlasSprite::IsHitTest(ipos pos) const -> bool
{
    NO_STACK_TRACE_ENTRY();

    if (!Size.IsValidPos(pos)) {
        return false;
    }

    if (!HitTestData.empty()) {
        return HitTestData[pos.y * Size.width + pos.x];
    }
    else {
        return false;
    }
}

auto AtlasSprite::MakeCopy() const -> shared_ptr<Sprite>
{
    STACK_TRACE_ENTRY();

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
    WholeTicks = ticks;

    DirCount = dirs;

    for (uint dir = 0; dir < dirs - 1; dir++) {
        Dirs[dir] = SafeAlloc::MakeShared<SpriteSheet>(_sprMngr, frames, ticks, 1);
    }
}

auto SpriteSheet::IsHitTest(ipos pos) const -> bool
{
    NO_STACK_TRACE_ENTRY();

    return GetCurSpr()->IsHitTest(pos);
}

auto SpriteSheet::GetBatchTex() const -> RenderTexture*
{
    NO_STACK_TRACE_ENTRY();

    return GetCurSpr()->GetBatchTex();
}

auto SpriteSheet::GetCurSpr() const -> const Sprite*
{
    NO_STACK_TRACE_ENTRY();

    return const_cast<SpriteSheet*>(this)->GetCurSpr();
}

auto SpriteSheet::GetCurSpr() -> Sprite*
{
    NO_STACK_TRACE_ENTRY();

    auto* dir_sheet = _curDir == 0 || !Dirs[_curDir - 1] ? this : Dirs[_curDir - 1].get();

    return dir_sheet->Spr[_curIndex].get();
}

auto SpriteSheet::MakeCopy() const -> shared_ptr<Sprite>
{
    STACK_TRACE_ENTRY();

    auto copy = SafeAlloc::MakeShared<SpriteSheet>(_sprMngr, CntFrm, WholeTicks, DirCount);

    copy->Size = Size;
    copy->Offset = Offset;

    for (size_t i = 0; i < Spr.size(); i++) {
        copy->Spr[i] = Spr[i]->MakeCopy();
        copy->SprOffset[i] = SprOffset[i];
        copy->StateAnim = StateAnim;
        copy->ActionAnim = ActionAnim;
    }

    for (uint i = 0; i < DirCount - 1; i++) {
        if (Dirs[i]) {
            copy->Dirs[i] = dynamic_ptr_cast<SpriteSheet>(Dirs[i]->MakeCopy());
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

    if (CntFrm == 1 || WholeTicks == 0) {
        return;
    }

    _playing = true;
    _looped = looped;
    _reversed = reversed;
    _startTick = _sprMngr.GetTimer().GetFrameTime();

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
        const auto cur_tick = _sprMngr.GetTimer().GetFrameTime();
        const auto dt = (cur_tick - _startTick).to_ms<int>();
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

    Size = cur_spr->Size;
    Offset = cur_spr->Offset;
}

auto SpriteSheet::GetSpr(uint num_frm) const -> const Sprite*
{
    NO_STACK_TRACE_ENTRY();

    return Spr[num_frm % CntFrm].get();
}

auto SpriteSheet::GetSpr(uint num_frm) -> Sprite*
{
    NO_STACK_TRACE_ENTRY();

    return Spr[num_frm % CntFrm].get();
}

auto SpriteSheet::GetDir(uint dir) const -> const SpriteSheet*
{
    NO_STACK_TRACE_ENTRY();

    return dir == 0 || DirCount == 1 ? this : Dirs[dir - 1].get();
}

auto SpriteSheet::GetDir(uint dir) -> SpriteSheet*
{
    NO_STACK_TRACE_ENTRY();

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
        auto anim = SafeAlloc::MakeShared<SpriteSheet>(_sprMngr, frames_count, ticks, dirs);

        for (uint16 dir = 0; dir < dirs; dir++) {
            auto* dir_anim = anim->GetDir(dir);
            const auto ox = file.GetLEShort();
            const auto oy = file.GetLEShort();

            dir_anim->Offset.x = ox;
            dir_anim->Offset.y = oy;

            for (uint16 i = 0; i < frames_count; i++) {
                const auto is_spr_ref = file.GetUChar();
                if (is_spr_ref == 0) {
                    const auto width = file.GetLEUShort();
                    const auto height = file.GetLEUShort();
                    const auto nx = file.GetLEShort();
                    const auto ny = file.GetLEShort();
                    const auto* data = file.GetCurBuf();

                    auto spr = SafeAlloc::MakeShared<AtlasSprite>(_sprMngr);

                    spr->Size.width = width;
                    spr->Size.height = height;
                    spr->Offset.x = ox;
                    spr->Offset.y = oy;
                    dir_anim->SprOffset[i].x = nx;
                    dir_anim->SprOffset[i].y = ny;

                    FillAtlas(spr.get(), atlas_type, reinterpret_cast<const ucolor*>(data));

                    if (i == 0) {
                        dir_anim->Size.width = width;
                        dir_anim->Size.height = height;
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

        auto spr = SafeAlloc::MakeShared<AtlasSprite>(_sprMngr);

        spr->Size.width = width;
        spr->Size.height = height;
        spr->Offset.x = ox;
        spr->Offset.y = oy;

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

    const auto size = atlas_spr->Size;

    RUNTIME_ASSERT(size.width > 0);
    RUNTIME_ASSERT(size.height > 0);

    auto&& [atlas, atlas_node, pos] = _sprMngr.GetAtlasMngr().FindAtlasPlace(atlas_type, size);

    // Refresh texture
    if (data != nullptr) {
        auto* tex = atlas->MainTex;
        tex->UpdateTextureRegion(pos, size, data);

        // 1px border for correct linear interpolation
        // Top
        tex->UpdateTextureRegion({pos.x, pos.y - 1}, {size.width, 1}, data);

        // Bottom
        tex->UpdateTextureRegion({pos.x, pos.y + size.height}, {size.width, 1}, data + static_cast<size_t>(size.height - 1) * size.width);

        // Left
        for (int i = 0; i < size.height; i++) {
            _borderBuf[i + 1] = *(data + static_cast<size_t>(i) * size.width);
        }
        _borderBuf[0] = _borderBuf[1];
        _borderBuf[size.height + 1] = _borderBuf[size.height];
        tex->UpdateTextureRegion({pos.x - 1, pos.y - 1}, {1, size.height + 2}, _borderBuf.data());

        // Right
        for (int i = 0; i < size.height; i++) {
            _borderBuf[i + 1] = *(data + static_cast<size_t>(i) * size.width + (size.width - 1));
        }
        _borderBuf[0] = _borderBuf[1];
        _borderBuf[size.height + 1] = _borderBuf[size.height];
        tex->UpdateTextureRegion({pos.x + size.width, pos.y - 1}, {1, size.height + 2}, _borderBuf.data());

        // Evaluate hit mask
        atlas_spr->HitTestData.resize(static_cast<size_t>(size.width) * size.height);
        for (size_t i = 0, j = static_cast<size_t>(size.width) * size.height; i < j; i++) {
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
    atlas_spr->AtlasRect.Left = static_cast<float>(pos.x) / static_cast<float>(atlas->Size.width);
    atlas_spr->AtlasRect.Top = static_cast<float>(pos.y) / static_cast<float>(atlas->Size.height);
    atlas_spr->AtlasRect.Right = static_cast<float>(pos.x + size.width) / static_cast<float>(atlas->Size.width);
    atlas_spr->AtlasRect.Bottom = static_cast<float>(pos.y + size.height) / static_cast<float>(atlas->Size.height);
}
