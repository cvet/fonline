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

#include "DefaultSprites.h"
#include "Application.h"
#include "Geometry.h"

FO_BEGIN_NAMESPACE

AtlasSprite::AtlasSprite(ptr<SpriteManager> spr_mngr, isize32 size, ipos32 offset, nptr<TextureAtlas> atlas, unique_del_nptr<TextureAtlasLayout::Allocation> atlas_allocation, frect32 atlas_rect, vector<bool>&& hit_data, optional<SpriteMeshData> mesh_data) :
    Sprite(spr_mngr, size, offset),
    _atlas {atlas},
    _atlasRect {atlas_rect},
    _hitTestData {std::move(hit_data)},
    _meshData {std::move(mesh_data)},
    _atlasAllocation {std::move(atlas_allocation)}
{
    FO_STACK_TRACE_ENTRY();

    if (_atlasAllocation) {
        _atlasAllocation->SetSpriteMesh(_meshData ? nptr<const SpriteMeshData> {&*_meshData} : nullptr);
    }
}

AtlasSprite::AtlasSprite(AtlasSprite&& other) noexcept :
    Sprite(other._sprMngr, other._size, other._offset),
    _atlas {other._atlas},
    _atlasRect {other._atlasRect},
    _hitTestData {std::move(other._hitTestData)},
    _meshData {std::move(other._meshData)},
    _atlasAllocation {std::move(other._atlasAllocation)}
{
    FO_STACK_TRACE_ENTRY();

    _drawEffect = other._drawEffect;
    other._drawEffect = nullptr;
    other._atlas = nullptr;

    if (_atlasAllocation) {
        _atlasAllocation->SetSpriteMesh(_meshData ? nptr<const SpriteMeshData> {&*_meshData} : nullptr);
    }
}

AtlasSprite::~AtlasSprite()
{
    FO_STACK_TRACE_ENTRY();

#if 0 // For debug purposes
    if constexpr (FO_DEBUG) {
        try {
            const auto rnd_color = ucolor {numeric_cast<uint8_t>(_sprMngr->Random(0, 255)), numeric_cast<uint8_t>(_sprMngr->Random(0, 255)), numeric_cast<uint8_t>(_sprMngr->Random(0, 255))};

            vector<ucolor> color_data;
            color_data.resize(_atlasAllocation->GetSize().square());

            for (size_t i = 0; i < color_data.size(); i++) {
                color_data[i] = rnd_color;
            }

            _atlas->_mainTex->UpdateTextureRegion(_atlasAllocation->GetPosition(), _atlasAllocation->GetSize(), color_data);
        }
        catch (...) {
        }
    }
#endif
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

auto AtlasSprite::GetBatchTexture() const -> nptr<const RenderTexture>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_atlas, "Atlas is null");
    return _atlas->GetTexture();
}

auto AtlasSprite::MakeCopy() const -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    return shared_from_this().cast_no_const();
}

auto AtlasSprite::FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (_meshData) {
        const auto& mesh = *_meshData;

        if (mesh.Indices.empty()) {
            return 0;
        }

        dbuf->CheckAllocBuf(mesh.Vertices.size(), mesh.Indices.size());

        auto& vbuf = dbuf->Vertices;
        size_t& vpos = dbuf->VertCount;
        auto& ibuf = dbuf->Indices;
        size_t& ipos = dbuf->IndCount;
        size_t base_vpos = vpos;
        float32_t width = numeric_cast<float32_t>(_size.width);
        float32_t height = numeric_cast<float32_t>(_size.height);
        ucolor color_left = std::get<0>(colors);
        ucolor color_right = std::get<1>(colors);
        uint32_t color_width = numeric_cast<uint32_t>(mesh.SourceSize.width);

        for (ipos32 local_pos : mesh.Vertices) {
            float32_t nx = numeric_cast<float32_t>(local_pos.x) / width;
            float32_t ny = numeric_cast<float32_t>(local_pos.y) / height;
            int32_t source_x = std::clamp(local_pos.x + mesh.SourceOffset.x, 0, mesh.SourceSize.width);
            uint32_t color_x = numeric_cast<uint32_t>(source_x);
            auto interpolate_component = [color_x, color_width](uint8_t left_component, uint8_t right_component) noexcept -> uint8_t {
                uint32_t weighted = numeric_cast<uint32_t>(left_component) * (color_width - color_x) + numeric_cast<uint32_t>(right_component) * color_x;
                return numeric_cast<uint8_t>((weighted + color_width / 2) / color_width);
            };
            auto& vertex = vbuf[vpos++];

            vertex.PosX = pos.x + pos.width * nx;
            vertex.PosY = pos.y + pos.height * ny;
            vertex.PosZ = 0.0f;
            vertex.TexU = _atlasRect.x + _atlasRect.width * nx;
            vertex.TexV = _atlasRect.y + _atlasRect.height * ny;
            vertex.EggFlags[0] = 0.0f;
            vertex.EggFlags[1] = 0.0f;
            vertex.Color = {
                interpolate_component(color_left.comp.r, color_right.comp.r),
                interpolate_component(color_left.comp.g, color_right.comp.g),
                interpolate_component(color_left.comp.b, color_right.comp.b),
                interpolate_component(color_left.comp.a, color_right.comp.a),
            };
        }

        for (uint16_t local_index : mesh.Indices) {
            ibuf[ipos++] = numeric_cast<vindex_t>(base_vpos + local_index);
        }

        return mesh.Indices.size();
    }

    dbuf->CheckAllocBuf(4, 6);

    auto& vbuf = dbuf->Vertices;
    size_t& vpos = dbuf->VertCount;
    auto& ibuf = dbuf->Indices;
    size_t& ipos = dbuf->IndCount;

    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 0);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 2);
    ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);

    auto& v0 = vbuf[vpos++];
    v0.PosX = pos.x;
    v0.PosY = pos.y + pos.height;
    v0.PosZ = 0.0f;
    v0.TexU = _atlasRect.x;
    v0.TexV = _atlasRect.y + _atlasRect.height;
    v0.EggFlags[0] = 0.0f;
    v0.EggFlags[1] = 0.0f;
    v0.Color = std::get<0>(colors);

    auto& v1 = vbuf[vpos++];
    v1.PosX = pos.x;
    v1.PosY = pos.y;
    v1.PosZ = 0.0f;
    v1.TexU = _atlasRect.x;
    v1.TexV = _atlasRect.y;
    v1.EggFlags[0] = 0.0f;
    v1.EggFlags[1] = 0.0f;
    v1.Color = std::get<0>(colors);

    auto& v2 = vbuf[vpos++];
    v2.PosX = pos.x + pos.width;
    v2.PosY = pos.y;
    v2.PosZ = 0.0f;
    v2.TexU = _atlasRect.x + _atlasRect.width;
    v2.TexV = _atlasRect.y;
    v2.EggFlags[0] = 0.0f;
    v2.EggFlags[1] = 0.0f;
    v2.Color = std::get<1>(colors);

    auto& v3 = vbuf[vpos++];
    v3.PosX = pos.x + pos.width;
    v3.PosY = pos.y + pos.height;
    v3.PosZ = 0.0f;
    v3.TexU = _atlasRect.x + _atlasRect.width;
    v3.TexV = _atlasRect.y + _atlasRect.height;
    v3.EggFlags[0] = 0.0f;
    v3.EggFlags[1] = 0.0f;
    v3.Color = std::get<1>(colors);

    return 6;
}

SpriteSheet::SpriteSheet(ptr<SpriteManager> spr_mngr, int32_t frames, int32_t ticks, int32_t dirs) :
    Sprite(spr_mngr, {}, {})
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(frames > 0, "Sprite sheet must have at least one frame");
    FO_VERIFY_AND_THROW(ticks >= 0, "Sprite sheet animation duration must not be negative");
    FO_VERIFY_AND_THROW(dirs == 1 || dirs == GameSettings::MAP_DIR_COUNT, "Default sprite direction count is unsupported", dirs, GameSettings::MAP_DIR_COUNT);

    _spr.resize(frames);
    _sprOffset.resize(frames);
    _framesCount = frames;
    _wholeTicks = ticks;
    _dirCount = dirs;

    for (int32_t dir = 0; dir < dirs - 1; dir++) {
        _dirs[dir] = SafeAlloc::MakeShared<SpriteSheet>(_sprMngr, frames, ticks, 1);
    }
}

auto SpriteSheet::IsHitTest(ipos32 pos) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetCurSpr()->IsHitTest(pos);
}

auto SpriteSheet::GetBatchTexture() const -> nptr<const RenderTexture>
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetCurSpr()->GetBatchTexture();
}

auto SpriteSheet::GetCurSpr() const -> ptr<const Sprite>
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<const SpriteSheet> dir_sheet = this;

    if (_curDir != hdir::NorthEast && _dirs[_curDir.value() - 1]) {
        dir_sheet = _dirs[_curDir.value() - 1];
    }

    return dir_sheet->_spr[_curIndex];
}

auto SpriteSheet::GetCurSpr() -> ptr<Sprite>
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<SpriteSheet> dir_sheet = this;

    if (_curDir != hdir::NorthEast && _dirs[_curDir.value() - 1]) {
        dir_sheet = _dirs[_curDir.value() - 1];
    }

    return dir_sheet->_spr[_curIndex];
}

auto SpriteSheet::MakeCopy() const -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    auto copy = SafeAlloc::MakeShared<SpriteSheet>(_sprMngr, _framesCount, _wholeTicks, _dirCount);

    for (size_t i = 0; i < _spr.size(); i++) {
        copy->_spr[i] = _spr[i]->MakeCopy();
        copy->_sprOffset[i] = _sprOffset[i];
        copy->_stateAnim = _stateAnim;
        copy->_actionAnim = _actionAnim;
    }

    for (int32_t i = 0; i < _dirCount - 1; i++) {
        if (_dirs[i]) {
            copy->_dirs[i] = _dirs[i]->MakeCopy().dyn_cast<SpriteSheet>();
        }
    }

    return copy;
}

auto SpriteSheet::FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    ptr<const SpriteSheet> dir_sheet = this;

    if (_curDir != hdir::NorthEast && _dirs[_curDir.value() - 1]) {
        dir_sheet = _dirs[_curDir.value() - 1];
    }

    return dir_sheet->_spr[_curIndex]->FillData(dbuf, pos, colors);
}

void SpriteSheet::Prewarm()
{
    FO_STACK_TRACE_ENTRY();

    _curIndex = _sprMngr->Random(0, _framesCount - 1);

    RefreshParams();
}

auto SpriteSheet::GetTime() const -> float32_t
{
    if (_framesCount > 1) {
        return numeric_cast<float32_t>(_curIndex) / numeric_cast<float32_t>(_framesCount - 1);
    }
    else {
        return 0.0f;
    }
}

void SpriteSheet::SetTime(float32_t normalized_time)
{
    FO_STACK_TRACE_ENTRY();

    _curIndex = _framesCount > 1 ? iround<int32_t>(normalized_time * numeric_cast<float32_t>(_framesCount - 1)) : 0;

    RefreshParams();
}

void SpriteSheet::SetDir(mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    _curDir = dir.hex();
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
        nanotime cur_tick = _sprMngr->GetTimer().GetFrameTime();
        int32_t dt = (cur_tick - _startTick).to_ms<int32_t>();
        int32_t frm_count = numeric_cast<int32_t>(_framesCount);
        int32_t ticks_per_frame = numeric_cast<int32_t>(_wholeTicks) / frm_count;
        int32_t frames_passed = dt / ticks_per_frame;

        if (frames_passed > 0) {
            _startTick += std::chrono::milliseconds {frames_passed * ticks_per_frame};

            int32_t index = numeric_cast<int32_t>(_curIndex) + (_reversed ? -frames_passed : frames_passed);

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

            FO_VERIFY_AND_THROW(index >= 0 && index < frm_count, "Default sprite animation selected a frame outside the frame table", index, frm_count, _curIndex, frames_passed, _looped, _reversed, _playing, dt, _wholeTicks);
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

    auto cur_spr = GetCurSpr();

    _size = cur_spr->GetSize();
    _offset = cur_spr->GetOffset();
}

auto SpriteSheet::GetSpr(int32_t num_frm) const -> ptr<const Sprite>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _spr[num_frm % _framesCount];
}

auto SpriteSheet::GetSpr(int32_t num_frm) -> ptr<Sprite>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _spr[num_frm % _framesCount];
}

auto SpriteSheet::GetDir(mdir dir) const -> nptr<const SpriteSheet>
{
    FO_NO_STACK_TRACE_ENTRY();

    int8_t dir_value = dir.hex().value();
    if (dir_value == 0 || _dirCount == 1) {
        return this;
    }

    return _dirs[dir_value - 1];
}

auto SpriteSheet::GetDir(mdir dir) -> nptr<SpriteSheet>
{
    FO_NO_STACK_TRACE_ENTRY();

    int8_t dir_value = dir.hex().value();
    if (dir_value == 0 || _dirCount == 1) {
        return this;
    }

    return _dirs[dir_value - 1];
}

DefaultSpriteFactory::DefaultSpriteFactory(ptr<SpriteManager> spr_mngr) :
    _sprMngr {spr_mngr}
{
    FO_STACK_TRACE_ENTRY();

    _borderBuf.resize(AppRender::MAX_ATLAS_SIZE);
}

auto DefaultSpriteFactory::LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite>
{
    FO_STACK_TRACE_ENTRY();

    auto file = _sprMngr->GetResources()->ReadFile(path);

    if (!file) {
        return nullptr;
    }

    SpriteResourceData resource = ReadSpriteResource(file.GetDataSpan());
    FO_VERIFY_AND_THROW(resource.Animation.Sprite.has_value(), "Sprite resource has no sprite animation info", path);
    const SpriteInfo& sprite_info = *resource.Animation.Sprite;
    uint8_t direction_count = numeric_cast<uint8_t>(resource.Directions.size());
    FO_VERIFY_AND_THROW(direction_count == 1 || direction_count == GameSettings::MAP_DIR_COUNT, "Sprite file direction count is unsupported", direction_count, GameSettings::MAP_DIR_COUNT);

    shared_ptr<Sprite> result;

    if (sprite_info.FrameCount > 1 || direction_count > 1) {
        auto anim = SafeAlloc::MakeShared<SpriteSheet>(_sprMngr, sprite_info.FrameCount, sprite_info.Duration.to_ms<int32_t>(), direction_count);

        for (uint8_t i = 0; i < direction_count; i++) {
            mdir dir = hdir(i);
            auto dir_anim = anim->GetDir(dir);
            FO_VERIFY_AND_THROW(dir_anim, "Sprite sheet is missing the requested direction");
            SpriteResourceDirectionData& direction = resource.Directions[i];

            for (uint16_t j = 0; j < sprite_info.FrameCount; j++) {
                SpriteResourceFrameData& frame = direction.Frames[j];

                if (!frame.SharedFrameIndex.has_value()) {
                    dir_anim->_sprOffset[j] = frame.NextOffset;
                    auto spr = FillAtlas(atlas_type, frame.Size, frame.Offset, frame.Pixels.data(), std::move(frame.Mesh));

                    if (j == 0) {
                        dir_anim->_size = frame.Size;
                        dir_anim->_offset = frame.Offset;
                    }

                    dir_anim->_spr[j] = std::move(spr);
                }
                else {
                    uint16_t index = *frame.SharedFrameIndex;
                    dir_anim->_spr[j] = dir_anim->GetSpr(index)->MakeCopy();
                    dir_anim->_sprOffset[j] = dir_anim->_sprOffset[index];
                }
            }
        }

        result = std::move(anim);
    }
    else {
        SpriteResourceDirectionData& direction = resource.Directions.front();
        SpriteResourceFrameData& frame = direction.Frames.front();
        FO_VERIFY_AND_THROW(!frame.SharedFrameIndex.has_value(), "Single-frame sprite resource cannot contain a shared-frame reference");
        result = FillAtlas(atlas_type, frame.Size, frame.Offset, frame.Pixels.data(), std::move(frame.Mesh));
    }

    return result;
}

auto DefaultSpriteFactory::FillAtlas(AtlasType atlas_type, isize32 size, ipos32 offset, nptr<const ucolor> pixels, optional<SpriteMeshData> mesh_data) -> shared_ptr<AtlasSprite>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width > 0, "Atlas sprite width must be positive", size.width);
    FO_VERIFY_AND_THROW(size.height > 0, "Atlas sprite height must be positive", size.height);

    auto&& [atlas, atlas_allocation, pos] = _sprMngr->GetAtlasMngr()->FindAtlasPlace(atlas_type, size);

    vector<bool> hit_test_data;

    if (pixels) {
        size_t width = numeric_cast<size_t>(size.width);
        size_t height = numeric_cast<size_t>(size.height);
        auto pixel_ptr = pixels.as_ptr();
        auto pixel_data = make_span(pixel_ptr, width * height);
        auto tex = atlas->GetTexture();
        tex->UpdateTextureRegion(pos, size, pixel_data);

        // 1px border for correct linear interpolation
        // Top
        tex->UpdateTextureRegion({pos.x, pos.y - 1}, {size.width, 1}, pixel_data.subspan(0, width));

        // Bottom
        tex->UpdateTextureRegion({pos.x, pos.y + size.height}, {size.width, 1}, pixel_data.subspan((height - 1) * width, width));

        // Left
        for (int32_t i = 0; i < size.height; i++) {
            _borderBuf[i + 1] = pixel_data[numeric_cast<size_t>(i) * width];
        }

        _borderBuf[0] = _borderBuf[1];
        _borderBuf[size.height + 1] = _borderBuf[size.height];
        auto border_pixels = make_span(make_ptr(_borderBuf.data()), numeric_cast<size_t>(size.height + 2));
        tex->UpdateTextureRegion({pos.x - 1, pos.y - 1}, {1, size.height + 2}, border_pixels);

        // Right
        for (int32_t i = 0; i < size.height; i++) {
            _borderBuf[i + 1] = pixel_data[numeric_cast<size_t>(i) * width + (width - 1)];
        }

        _borderBuf[0] = _borderBuf[1];
        _borderBuf[size.height + 1] = _borderBuf[size.height];
        tex->UpdateTextureRegion({pos.x + size.width, pos.y - 1}, {1, size.height + 2}, border_pixels);

        // Evaluate hit mask
        hit_test_data.resize(numeric_cast<size_t>(size.width) * size.height);

        for (size_t i = 0, j = pixel_data.size(); i < j; i++) {
            hit_test_data[i] = _sprMngr->CheckHitTest(numeric_cast<int32_t>(pixel_data[i].comp.a));
        }
    }

    atlas->GetRenderTarget()->ClearLastPixelPicks();

    frect32 atlas_rect;
    atlas_rect.x = numeric_cast<float32_t>(pos.x) / numeric_cast<float32_t>(atlas->GetSize().width);
    atlas_rect.y = numeric_cast<float32_t>(pos.y) / numeric_cast<float32_t>(atlas->GetSize().height);
    atlas_rect.width = numeric_cast<float32_t>(size.width) / numeric_cast<float32_t>(atlas->GetSize().width);
    atlas_rect.height = numeric_cast<float32_t>(size.height) / numeric_cast<float32_t>(atlas->GetSize().height);
    return SafeAlloc::MakeShared<AtlasSprite>(_sprMngr, size, offset, atlas, std::move(atlas_allocation), atlas_rect, std::move(hit_test_data), std::move(mesh_data));
}

FO_END_NAMESPACE
