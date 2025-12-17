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
#include "Geometry.h"

FO_BEGIN_NAMESPACE();

AtlasSprite::AtlasSprite(SpriteManager& spr_mngr, isize32 size, ipos32 offset, TextureAtlas* atlas, TextureAtlas::SpaceNode* atlas_node, frect32 atlas_rect, vector<bool>&& hit_data) :
    Sprite(spr_mngr, size, offset),
    _atlas {atlas},
    _atlasNode {atlas_node},
    _atlasRect {atlas_rect},
    _hitTestData {std::move(hit_data)}
{
    FO_STACK_TRACE_ENTRY();
}

AtlasSprite::~AtlasSprite()
{
    FO_STACK_TRACE_ENTRY();

#if 0 // For debug purposes
    if constexpr (FO_DEBUG) {
        try {
            const auto rnd_color = ucolor {numeric_cast<uint8>(GenericUtils::Random(0, 255)), numeric_cast<uint8>(GenericUtils::Random(0, 255)), numeric_cast<uint8>(GenericUtils::Random(0, 255))};

            vector<ucolor> color_data;
            color_data.resize(_atlasNode->Size.square());

            for (size_t i = 0; i < color_data.size(); i++) {
                color_data[i] = rnd_color;
            }

            _atlas->_mainTex->UpdateTextureRegion(_atlasNode->Pos, _atlasNode->Size, color_data.data());
        }
        catch (...) {
        }
    }
#endif

    if (_atlasNode) {
        _atlasNode->Free();
    }
}

auto AtlasSprite::IsHitTest(ipos32 pos) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_size.is_valid_pos(pos)) {
        return false;
    }

    if (!_hitTestData.empty()) {
        return _hitTestData[pos.y * _size.width + pos.x];
    }
    else {
        return false;
    }
}

auto AtlasSprite::MakeCopy() const -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    return const_cast<AtlasSprite*>(this)->shared_from_this();
}

auto AtlasSprite::FillData(RenderDrawBuffer* dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    dbuf->CheckAllocBuf(4, 6);

    auto& vbuf = dbuf->Vertices;
    auto& vpos = dbuf->VertCount;
    auto& ibuf = dbuf->Indices;
    auto& ipos = dbuf->IndCount;

    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 0);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 2);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);

    auto& v0 = vbuf[vpos++];
    v0.PosX = pos.x;
    v0.PosY = pos.y + pos.height;
    v0.TexU = _atlasRect.x;
    v0.TexV = _atlasRect.y + _atlasRect.height;
    v0.EggTexU = 0.0f;
    v0.Color = std::get<0>(colors);

    auto& v1 = vbuf[vpos++];
    v1.PosX = pos.x;
    v1.PosY = pos.y;
    v1.TexU = _atlasRect.x;
    v1.TexV = _atlasRect.y;
    v1.EggTexU = 0.0f;
    v1.Color = std::get<0>(colors);

    auto& v2 = vbuf[vpos++];
    v2.PosX = pos.x + pos.width;
    v2.PosY = pos.y;
    v2.TexU = _atlasRect.x + _atlasRect.width;
    v2.TexV = _atlasRect.y;
    v2.EggTexU = 0.0f;
    v2.Color = std::get<1>(colors);

    auto& v3 = vbuf[vpos++];
    v3.PosX = pos.x + pos.width;
    v3.PosY = pos.y + pos.height;
    v3.TexU = _atlasRect.x + _atlasRect.width;
    v3.TexV = _atlasRect.y + _atlasRect.height;
    v3.EggTexU = 0.0f;
    v3.Color = std::get<1>(colors);

    return 6;
}

SpriteSheet::SpriteSheet(SpriteManager& spr_mngr, int32 frames, int32 ticks, int32 dirs) :
    Sprite(spr_mngr, {}, {})
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(frames > 0);
    FO_RUNTIME_ASSERT(ticks >= 0);
    FO_RUNTIME_ASSERT(dirs == 1 || dirs == GameSettings::MAP_DIR_COUNT);

    _spr.resize(frames);
    _sprOffset.resize(frames);
    _framesCount = frames;
    _wholeTicks = ticks;
    _dirCount = dirs;

    for (int32 dir = 0; dir < dirs - 1; dir++) {
        _dirs[dir] = SafeAlloc::MakeShared<SpriteSheet>(*_sprMngr, frames, ticks, 1);
    }
}

auto SpriteSheet::IsHitTest(ipos32 pos) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetCurSpr()->IsHitTest(pos);
}

auto SpriteSheet::GetBatchTexture() const -> const RenderTexture*
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetCurSpr()->GetBatchTexture();
}

auto SpriteSheet::GetCurSpr() const -> const Sprite*
{
    FO_NO_STACK_TRACE_ENTRY();

    return const_cast<SpriteSheet*>(this)->GetCurSpr();
}

auto SpriteSheet::GetCurSpr() -> Sprite*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* dir_sheet = _curDir == 0 || !_dirs[_curDir - 1] ? this : _dirs[_curDir - 1].get();

    return dir_sheet->_spr[_curIndex].get();
}

auto SpriteSheet::MakeCopy() const -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    auto copy = SafeAlloc::MakeShared<SpriteSheet>(*_sprMngr.get_no_const(), _framesCount, _wholeTicks, _dirCount);

    for (size_t i = 0; i < _spr.size(); i++) {
        copy->_spr[i] = _spr[i]->MakeCopy();
        copy->_sprOffset[i] = _sprOffset[i];
        copy->_stateAnim = _stateAnim;
        copy->_actionAnim = _actionAnim;
    }

    for (int32 i = 0; i < _dirCount - 1; i++) {
        if (_dirs[i]) {
            copy->_dirs[i] = dynamic_ptr_cast<SpriteSheet>(_dirs[i]->MakeCopy());
        }
    }

    return copy;
}

auto SpriteSheet::FillData(RenderDrawBuffer* dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    const auto* dir_sheet = _curDir == 0 || !_dirs[_curDir - 1] ? this : _dirs[_curDir - 1].get();
    const auto* spr = dir_sheet->_spr[_curIndex].get();

    return spr->FillData(dbuf, pos, colors);
}

void SpriteSheet::Prewarm()
{
    FO_STACK_TRACE_ENTRY();

    _curIndex = GenericUtils::Random(0, _framesCount - 1);

    RefreshParams();
}

auto SpriteSheet::GetTime() const -> float32
{
    if (_framesCount > 1) {
        return numeric_cast<float32>(_curIndex) / numeric_cast<float32>(_framesCount - 1);
    }
    else {
        return 0.0f;
    }
}

void SpriteSheet::SetTime(float32 normalized_time)
{
    FO_STACK_TRACE_ENTRY();

    _curIndex = _framesCount > 1 ? iround<int32>(normalized_time * numeric_cast<float32>(_framesCount - 1)) : 0;

    RefreshParams();
}

void SpriteSheet::SetDir(uint8 dir)
{
    FO_STACK_TRACE_ENTRY();

    _curDir = dir;
}

void SpriteSheet::SetDirAngle(int16 dir_angle)
{
    FO_STACK_TRACE_ENTRY();

    _curDir = GeometryHelper::AngleToDir(dir_angle);
}

void SpriteSheet::Play(hstring anim_name, bool looped, bool reversed)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(anim_name);

    if (_framesCount == 1 || _wholeTicks == 0) {
        return;
    }

    _playing = true;
    _looped = looped;
    _reversed = reversed;
    _startTick = _sprMngr->GetTimer().GetFrameTime();

    StartUpdate();
}

void SpriteSheet::Stop()
{
    FO_STACK_TRACE_ENTRY();

    _playing = false;
}

auto SpriteSheet::Update() -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_playing) {
        const auto cur_tick = _sprMngr->GetTimer().GetFrameTime();
        const auto dt = (cur_tick - _startTick).to_ms<int32>();
        const auto frm_count = numeric_cast<int32>(_framesCount);
        const auto ticks_per_frame = numeric_cast<int32>(_wholeTicks) / frm_count;
        const auto frames_passed = dt / ticks_per_frame;

        if (frames_passed > 0) {
            _startTick += std::chrono::milliseconds {frames_passed * ticks_per_frame};

            auto index = numeric_cast<int32>(_curIndex) + (_reversed ? -frames_passed : frames_passed);

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

            FO_RUNTIME_ASSERT(index >= 0 && index < frm_count);
            _curIndex = index;

            RefreshParams();
        }

        return _playing;
    }

    return false;
}

void SpriteSheet::RefreshParams()
{
    FO_STACK_TRACE_ENTRY();

    const auto* cur_spr = GetCurSpr();

    _size = cur_spr->GetSize();
    _offset = cur_spr->GetOffset();
}

auto SpriteSheet::GetSpr(int32 num_frm) const -> const Sprite*
{
    FO_NO_STACK_TRACE_ENTRY();

    return _spr[num_frm % _framesCount].get();
}

auto SpriteSheet::GetSpr(int32 num_frm) -> Sprite*
{
    FO_NO_STACK_TRACE_ENTRY();

    return _spr[num_frm % _framesCount].get();
}

auto SpriteSheet::GetDir(int32 dir) const -> const SpriteSheet*
{
    FO_NO_STACK_TRACE_ENTRY();

    return dir == 0 || _dirCount == 1 ? this : _dirs[dir - 1].get();
}

auto SpriteSheet::GetDir(int32 dir) -> SpriteSheet*
{
    FO_NO_STACK_TRACE_ENTRY();

    return dir == 0 || _dirCount == 1 ? this : _dirs[dir - 1].get();
}

DefaultSpriteFactory::DefaultSpriteFactory(SpriteManager& spr_mngr) :
    _sprMngr {&spr_mngr}
{
    FO_STACK_TRACE_ENTRY();

    _borderBuf.resize(AppRender::MAX_ATLAS_SIZE);
}

auto DefaultSpriteFactory::LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    const auto file = _sprMngr->GetResources().ReadFile(path);

    if (!file) {
        return nullptr;
    }

    auto reader = file.GetReader();

    const auto check_number = reader.GetUInt8();
    FO_RUNTIME_ASSERT(check_number == 42);
    const auto frames_count = reader.GetLEUInt16();
    FO_RUNTIME_ASSERT(frames_count != 0);
    const auto ticks = reader.GetLEUInt16();
    const auto dirs = reader.GetUInt8();
    FO_RUNTIME_ASSERT(dirs != 0);

    if (frames_count > 1 || dirs > 1) {
        auto anim = SafeAlloc::MakeShared<SpriteSheet>(*_sprMngr, frames_count, ticks, dirs);

        for (uint8 dir = 0; dir < dirs; dir++) {
            auto* dir_anim = anim->GetDir(dir);
            const auto ox = reader.GetLEInt16();
            const auto oy = reader.GetLEInt16();

            dir_anim->_offset.x = ox;
            dir_anim->_offset.y = oy;

            for (uint16 i = 0; i < frames_count; i++) {
                const auto is_spr_ref = reader.GetUInt8();

                if (is_spr_ref == 0) {
                    const auto width = reader.GetLEUInt16();
                    const auto height = reader.GetLEUInt16();
                    const auto nx = reader.GetLEInt16();
                    const auto ny = reader.GetLEInt16();
                    const auto* data = reader.GetCurBuf();

                    dir_anim->_sprOffset[i].x = nx;
                    dir_anim->_sprOffset[i].y = ny;

                    auto spr = FillAtlas(atlas_type, {width, height}, {ox, oy}, reinterpret_cast<const ucolor*>(data));

                    if (i == 0) {
                        dir_anim->_size.width = width;
                        dir_anim->_size.height = height;
                    }

                    dir_anim->_spr[i] = std::move(spr);

                    reader.GoForward(numeric_cast<size_t>(width) * height * 4);
                }
                else {
                    const auto index = reader.GetLEUInt16();

                    dir_anim->_spr[i] = dir_anim->GetSpr(index)->MakeCopy();
                    dir_anim->_sprOffset[i] = dir_anim->_sprOffset[index];
                }
            }
        }

        const auto check_number2 = reader.GetUInt8();
        FO_RUNTIME_ASSERT(check_number2 == 42);

        return anim;
    }
    else {
        const auto ox = reader.GetLEInt16();
        const auto oy = reader.GetLEInt16();

        const auto is_spr_ref = reader.GetUInt8();
        FO_RUNTIME_ASSERT(is_spr_ref == 0);

        const auto width = reader.GetLEUInt16();
        const auto height = reader.GetLEUInt16();
        const auto nx = reader.GetLEInt16();
        const auto ny = reader.GetLEInt16();
        const auto* data = reader.GetCurBuf();

        ignore_unused(nx);
        ignore_unused(ny);

        auto spr = FillAtlas(atlas_type, {width, height}, {ox, oy}, reinterpret_cast<const ucolor*>(data));

        reader.GoForward(numeric_cast<size_t>(width) * height * 4);

        const auto check_number2 = reader.GetUInt8();
        FO_RUNTIME_ASSERT(check_number2 == 42);

        return spr;
    }
}

auto DefaultSpriteFactory::FillAtlas(AtlasType atlas_type, isize32 size, ipos32 offset, const ucolor* data) -> shared_ptr<AtlasSprite>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(size.width > 0);
    FO_RUNTIME_ASSERT(size.height > 0);

    auto&& [atlas, atlas_node, pos] = _sprMngr->GetAtlasMngr().FindAtlasPlace(atlas_type, size);

    vector<bool> hit_test_data;

    if (data != nullptr) {
        auto* tex = atlas->GetTexture();
        tex->UpdateTextureRegion(pos, size, data);

        // 1px border for correct linear interpolation
        // Top
        tex->UpdateTextureRegion({pos.x, pos.y - 1}, {size.width, 1}, data);

        // Bottom
        tex->UpdateTextureRegion({pos.x, pos.y + size.height}, {size.width, 1}, data + numeric_cast<size_t>(size.height - 1) * size.width);

        // Left
        for (int32 i = 0; i < size.height; i++) {
            _borderBuf[i + 1] = *(data + numeric_cast<size_t>(i) * size.width);
        }

        _borderBuf[0] = _borderBuf[1];
        _borderBuf[size.height + 1] = _borderBuf[size.height];
        tex->UpdateTextureRegion({pos.x - 1, pos.y - 1}, {1, size.height + 2}, _borderBuf.data());

        // Right
        for (int32 i = 0; i < size.height; i++) {
            _borderBuf[i + 1] = *(data + numeric_cast<size_t>(i) * size.width + (size.width - 1));
        }

        _borderBuf[0] = _borderBuf[1];
        _borderBuf[size.height + 1] = _borderBuf[size.height];
        tex->UpdateTextureRegion({pos.x + size.width, pos.y - 1}, {1, size.height + 2}, _borderBuf.data());

        // Evaluate hit mask
        hit_test_data.resize(numeric_cast<size_t>(size.width) * size.height);

        for (size_t i = 0, j = numeric_cast<size_t>(size.width) * size.height; i < j; i++) {
            hit_test_data[i] = data[i].comp.a > 127;
        }
    }

    atlas->GetRenderTarget()->ClearLastPixelPicks();

    frect32 atlas_rect;
    atlas_rect.x = numeric_cast<float32>(pos.x) / numeric_cast<float32>(atlas->GetSize().width);
    atlas_rect.y = numeric_cast<float32>(pos.y) / numeric_cast<float32>(atlas->GetSize().height);
    atlas_rect.width = numeric_cast<float32>(size.width) / numeric_cast<float32>(atlas->GetSize().width);
    atlas_rect.height = numeric_cast<float32>(size.height) / numeric_cast<float32>(atlas->GetSize().height);
    auto atlas_spr = SafeAlloc::MakeShared<AtlasSprite>(*_sprMngr, size, offset, atlas, atlas_node, atlas_rect, std::move(hit_test_data));
    return atlas_spr;
}

FO_END_NAMESPACE();
