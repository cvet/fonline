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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

static constexpr isize32 DUMMY_SPRITE_SIZE {1, 1};
static constexpr ucolor DUMMY_SPRITE_COLOR {255, 255, 255, 255};

static auto ResolveAtlasSpriteLogicalSize(isize32 frame_size, const optional<SpriteMeshData>& mesh_data) -> isize32
{
    return mesh_data && !mesh_data->Indices.empty() ? mesh_data->SourceSize : frame_size;
}

static auto ResolveAtlasSpriteLogicalOffset(isize32 frame_size, ipos32 frame_offset, const optional<SpriteMeshData>& mesh_data) -> ipos32
{
    if (!mesh_data || mesh_data->Indices.empty()) {
        return frame_offset;
    }

    isize32 logical_size = mesh_data->SourceSize;

    return {
        frame_offset.x - frame_size.width / 2 + logical_size.width / 2 - mesh_data->SourceOffset.x,
        frame_offset.y - frame_size.height + logical_size.height - mesh_data->SourceOffset.y,
    };
}

static auto ResolveAtlasSpriteFrameSize(const SpriteMeshData& mesh) -> isize32
{
    isize32 frame_size {};

    for (ipos32 vertex : mesh.Vertices) {
        frame_size.width = std::max(frame_size.width, vertex.x);
        frame_size.height = std::max(frame_size.height, vertex.y);
    }

    return frame_size;
}

AtlasSprite::AtlasSprite(ptr<SpriteManager> spr_mngr, isize32 size, ipos32 offset, nptr<TextureAtlas> atlas, unique_del_nptr<TextureAtlasLayout::Allocation> atlas_allocation, frect32 atlas_rect, vector<bool>&& hit_data, optional<SpriteMeshData> mesh_data) :
    Sprite(spr_mngr, ResolveAtlasSpriteLogicalSize(size, mesh_data), ResolveAtlasSpriteLogicalOffset(size, offset, mesh_data)),
    _atlas {atlas},
    _atlasRect {atlas_rect},
    _hitTestData {std::move(hit_data)},
    _meshData {std::move(mesh_data)},
    _atlasAllocation {std::move(atlas_allocation)}
{
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
    _drawEffect = other._drawEffect;
    other._drawEffect = nullptr;
    other._atlas = nullptr;

    if (_atlasAllocation) {
        _atlasAllocation->SetSpriteMesh(_meshData ? nptr<const SpriteMeshData> {&*_meshData} : nullptr);
    }
}

auto AtlasSprite::IsHitTest(ipos32 pos) const -> bool
{
    if (!_size.is_valid_pos(pos)) {
        return false;
    }

    if (_meshData.has_value() && !_meshData->Indices.empty()) {
        pos -= _meshData->SourceOffset;
    }

    isize32 frame_size = _meshData.has_value() && !_meshData->Indices.empty() ? ResolveAtlasSpriteFrameSize(*_meshData) : _size;

    if (!frame_size.is_valid_pos(pos)) {
        return false;
    }

    if (!_hitTestData.empty()) {
        return _hitTestData[pos.y * frame_size.width + pos.x];
    }
    else {
        return false;
    }
}

auto AtlasSprite::GetBatchTexture() const -> nptr<const RenderTexture>
{
    if (!_atlas) {
        return nullptr;
    }

    return _atlas->GetTexture();
}

auto AtlasSprite::MakeCopy() const -> shared_ptr<Sprite>
{
    return shared_from_this().cast_no_const();
}

auto AtlasSprite::FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t
{
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
        FO_VERIFY_AND_THROW(_size == mesh.SourceSize, "Atlas sprite logical size differs from its mesh source size", _size, mesh.SourceSize);

        isize32 frame_size = ResolveAtlasSpriteFrameSize(mesh);
        float32_t frame_width = numeric_cast<float32_t>(frame_size.width);
        float32_t frame_height = numeric_cast<float32_t>(frame_size.height);
        float32_t logical_width = numeric_cast<float32_t>(mesh.SourceSize.width);
        float32_t logical_height = numeric_cast<float32_t>(mesh.SourceSize.height);
        ucolor color_left = std::get<0>(colors);
        ucolor color_right = std::get<1>(colors);
        uint32_t color_width = numeric_cast<uint32_t>(mesh.SourceSize.width);

        for (ipos32 local_pos : mesh.Vertices) {
            ipos32 source_pos = local_pos + mesh.SourceOffset;
            float32_t logical_x = numeric_cast<float32_t>(source_pos.x) / logical_width;
            float32_t logical_y = numeric_cast<float32_t>(source_pos.y) / logical_height;
            float32_t frame_x = numeric_cast<float32_t>(local_pos.x) / frame_width;
            float32_t frame_y = numeric_cast<float32_t>(local_pos.y) / frame_height;
            int32_t source_x = std::clamp(source_pos.x, 0, mesh.SourceSize.width);
            uint32_t color_x = numeric_cast<uint32_t>(source_x);
            auto interpolate_component = [color_x, color_width](uint8_t left_component, uint8_t right_component) noexcept -> uint8_t {
                uint32_t weighted = numeric_cast<uint32_t>(left_component) * (color_width - color_x) + numeric_cast<uint32_t>(right_component) * color_x;
                return numeric_cast<uint8_t>((weighted + color_width / 2) / color_width);
            };
            auto& vertex = vbuf[vpos++];

            vertex.PosX = pos.x + pos.width * logical_x;
            vertex.PosY = pos.y + pos.height * logical_y;
            vertex.PosZ = 0.0f;
            vertex.TexU = _atlasRect.x + _atlasRect.width * frame_x;
            vertex.TexV = _atlasRect.y + _atlasRect.height * frame_y;
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

auto AtlasSprite::ResolveRegion(fpos32 uv0, fpos32 uv1, const frect32& pos) const -> optional<AtlasSpriteRegion>
{
    if (_meshData.has_value() && _meshData->Indices.empty()) {
        return std::nullopt;
    }

    isize32 source_size = _size;
    ipos32 frame_offset = _meshData.has_value() ? _meshData->SourceOffset : ipos32 {};
    isize32 frame_size = _meshData.has_value() ? ResolveAtlasSpriteFrameSize(*_meshData) : source_size;
    fpos32 requested_begin {
        uv0.x * numeric_cast<float32_t>(source_size.width),
        uv0.y * numeric_cast<float32_t>(source_size.height),
    };
    fpos32 requested_end {
        uv1.x * numeric_cast<float32_t>(source_size.width),
        uv1.y * numeric_cast<float32_t>(source_size.height),
    };
    fsize32 requested_size {
        requested_end.x - requested_begin.x,
        requested_end.y - requested_begin.y,
    };

    if (requested_size.width <= 0.0f || requested_size.height <= 0.0f) {
        return std::nullopt;
    }

    fpos32 clipped_begin {
        std::max(requested_begin.x, numeric_cast<float32_t>(frame_offset.x)),
        std::max(requested_begin.y, numeric_cast<float32_t>(frame_offset.y)),
    };
    fpos32 clipped_end {
        std::min(requested_end.x, numeric_cast<float32_t>(frame_offset.x + frame_size.width)),
        std::min(requested_end.y, numeric_cast<float32_t>(frame_offset.y + frame_size.height)),
    };

    if (clipped_end.x <= clipped_begin.x || clipped_end.y <= clipped_begin.y) {
        return std::nullopt;
    }

    float32_t destination_left = pos.x + pos.width * (clipped_begin.x - requested_begin.x) / requested_size.width;
    float32_t destination_top = pos.y + pos.height * (clipped_begin.y - requested_begin.y) / requested_size.height;
    float32_t destination_right = pos.x + pos.width * (clipped_end.x - requested_begin.x) / requested_size.width;
    float32_t destination_bottom = pos.y + pos.height * (clipped_end.y - requested_begin.y) / requested_size.height;
    float32_t frame_width = numeric_cast<float32_t>(frame_size.width);
    float32_t frame_height = numeric_cast<float32_t>(frame_size.height);
    float32_t texture_left = _atlasRect.x + _atlasRect.width * (clipped_begin.x - numeric_cast<float32_t>(frame_offset.x)) / frame_width;
    float32_t texture_top = _atlasRect.y + _atlasRect.height * (clipped_begin.y - numeric_cast<float32_t>(frame_offset.y)) / frame_height;
    float32_t texture_right = _atlasRect.x + _atlasRect.width * (clipped_end.x - numeric_cast<float32_t>(frame_offset.x)) / frame_width;
    float32_t texture_bottom = _atlasRect.y + _atlasRect.height * (clipped_end.y - numeric_cast<float32_t>(frame_offset.y)) / frame_height;

    return AtlasSpriteRegion {
        .DrawRect =
            {
                destination_left,
                destination_top,
                destination_right - destination_left,
                destination_bottom - destination_top,
            },
        .TextureRect =
            {
                texture_left,
                texture_top,
                texture_right - texture_left,
                texture_bottom - texture_top,
            },
    };
}

auto AtlasSprite::FillRegionData(ptr<RenderDrawBuffer> dbuf, fpos32 uv0, fpos32 uv1, const frect32& pos, ucolor color) const -> size_t
{
    optional<AtlasSpriteRegion> region = ResolveRegion(uv0, uv1, pos);

    if (!region.has_value()) {
        return 0;
    }

    const frect32& draw_rect = region->DrawRect;
    const frect32& texture_rect = region->TextureRect;

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
    v0.PosX = draw_rect.x;
    v0.PosY = draw_rect.y + draw_rect.height;
    v0.PosZ = 0.0f;
    v0.TexU = texture_rect.x;
    v0.TexV = texture_rect.y + texture_rect.height;
    v0.EggFlags[0] = 0.0f;
    v0.EggFlags[1] = 0.0f;
    v0.Color = color;

    auto& v1 = vbuf[vpos++];
    v1.PosX = draw_rect.x;
    v1.PosY = draw_rect.y;
    v1.PosZ = 0.0f;
    v1.TexU = texture_rect.x;
    v1.TexV = texture_rect.y;
    v1.EggFlags[0] = 0.0f;
    v1.EggFlags[1] = 0.0f;
    v1.Color = color;

    auto& v2 = vbuf[vpos++];
    v2.PosX = draw_rect.x + draw_rect.width;
    v2.PosY = draw_rect.y;
    v2.PosZ = 0.0f;
    v2.TexU = texture_rect.x + texture_rect.width;
    v2.TexV = texture_rect.y;
    v2.EggFlags[0] = 0.0f;
    v2.EggFlags[1] = 0.0f;
    v2.Color = color;

    auto& v3 = vbuf[vpos++];
    v3.PosX = draw_rect.x + draw_rect.width;
    v3.PosY = draw_rect.y + draw_rect.height;
    v3.PosZ = 0.0f;
    v3.TexU = texture_rect.x + texture_rect.width;
    v3.TexV = texture_rect.y + texture_rect.height;
    v3.EggFlags[0] = 0.0f;
    v3.EggFlags[1] = 0.0f;
    v3.Color = color;

    return 6;
}

static auto MakeUniformSpriteTimelines(int32_t frames, timespan duration, int32_t dirs) -> vector<vector<timespan>>
{
    constexpr timespan max_duration {std::chrono::milliseconds {std::numeric_limits<uint32_t>::max()}};

    FO_VERIFY_AND_THROW(frames > 0 && frames <= 65535, "Sprite sheet requires 1..65535 frames", frames);
    FO_VERIFY_AND_THROW((duration == timespan::zero || duration.nanoseconds() >= frames) && duration <= max_duration, "Sprite sheet duration must be zero or provide at least one nanosecond per frame and fit uint32 milliseconds", frames, duration);
    FO_VERIFY_AND_THROW(dirs == 1 || dirs == GameSettings::MAP_DIR_COUNT, "Default sprite direction count is unsupported", dirs);

    vector<timespan> timeline(numeric_cast<size_t>(frames));
    int64_t whole_ns = duration.nanoseconds() / frames;
    int64_t remainder_ns = duration.nanoseconds() % frames;
    int64_t previous_end_ns = 0;

    for (int32_t index = 0; index < frames; index++) {
        // Split before multiplying: the full duration times the frame count can exceed int64
        int64_t end_ns = (index + 1) * whole_ns + (index + 1) * remainder_ns / frames;
        timeline[index] = std::chrono::nanoseconds {end_ns - previous_end_ns};
        previous_end_ns = end_ns;
    }

    return vector<vector<timespan>>(numeric_cast<size_t>(dirs), timeline);
}

SpriteSheet::SpriteSheet(ptr<SpriteManager> spr_mngr, int32_t frames, timespan duration, int32_t dirs) :
    SpriteSheet(spr_mngr, MakeUniformSpriteTimelines(frames, duration, dirs))
{
}

SpriteSheet::SpriteSheet(ptr<SpriteManager> spr_mngr, const vector<vector<timespan>>& timelines) :
    Sprite(spr_mngr, {}, {})
{
    FO_VERIFY_AND_THROW(timelines.size() == 1 || timelines.size() == GameSettings::MAP_DIR_COUNT, "Default sprite direction count is unsupported", timelines.size());

    _dirCount = numeric_cast<int32_t>(timelines.size());

    if (_dirCount == 1) {
        SetFrameTimeline(timelines.front());
    }
    else {
        for (int32_t dir = 0; dir < _dirCount; dir++) {
            _dirs[dir] = safe_alloc::make_shared<SpriteSheet>(_sprMngr, vector<vector<timespan>> {timelines[dir]});
        }
    }
}

void SpriteSheet::SetFrameTimeline(vector<timespan> durations)
{
    FO_VERIFY_AND_THROW(!durations.empty() && durations.size() <= 65535, "Sprite timeline requires 1..65535 frames", durations.size());

    constexpr timespan max_duration {std::chrono::milliseconds {std::numeric_limits<uint32_t>::max()}};
    vector<timespan> ends;
    timespan total {};
    bool untimed = std::ranges::all_of(durations, [](timespan duration) { return duration == timespan::zero; });

    for (timespan duration : durations) {
        FO_VERIFY_AND_THROW(untimed || duration > timespan::zero, "Sprite timeline must be entirely untimed or contain only positive frame durations");
        FO_VERIFY_AND_THROW(duration <= max_duration - total, "Sprite duration exceeds uint32 milliseconds", total, duration);

        total += duration;
        ends.emplace_back(total);
    }

    _frameDurations = std::move(durations);
    _frameEnds = std::move(ends);
    _framesCount = numeric_cast<int32_t>(_frameDurations.size());
    _duration = total;
    _spr.resize(numeric_cast<size_t>(_framesCount));
    _sprOffset.resize(numeric_cast<size_t>(_framesCount));
    _sprMotion.resize(numeric_cast<size_t>(_framesCount));

    _phase = 0;
    _curIndex = 0;
}

auto SpriteSheet::GetCurrentDirection() const noexcept -> ptr<const SpriteSheet>
{
    return _dirCount == 1 ? ptr<const SpriteSheet> {this} : ptr<const SpriteSheet> {_dirs[_curDir.value()]};
}

auto SpriteSheet::GetCurrentDirection() noexcept -> ptr<SpriteSheet>
{
    return _dirCount == 1 ? ptr<SpriteSheet> {this} : ptr<SpriteSheet> {_dirs[_curDir.value()]};
}

auto SpriteSheet::IsHitTest(ipos32 pos) const -> bool
{
    return GetCurrentSprite()->IsHitTest(pos);
}

auto SpriteSheet::GetBatchTexture() const -> nptr<const RenderTexture>
{
    return GetCurrentSprite()->GetBatchTexture();
}

auto SpriteSheet::GetCurrentSprite() const -> ptr<const Sprite>
{
    return GetCurrentDirection()->_spr[_curIndex];
}

auto SpriteSheet::GetCurrentSprite() -> ptr<Sprite>
{
    return GetCurrentDirection()->_spr[_curIndex];
}

auto SpriteSheet::MakeCopy() const -> shared_ptr<Sprite>
{
    vector<vector<timespan>> timelines;
    timelines.reserve(numeric_cast<size_t>(_dirCount));

    for (int32_t dir = 0; dir < _dirCount; dir++) {
        timelines.emplace_back(GetDirection(hdir(dir))->_frameDurations);
    }

    auto copy = safe_alloc::make_shared<SpriteSheet>(_sprMngr, timelines);

    for (int32_t dir = 0; dir < _dirCount; dir++) {
        auto source = GetDirection(hdir(dir));
        auto target = copy->GetDirection(hdir(dir));

        for (int32_t frame = 0; frame < source->_framesCount; frame++) {
            target->_spr[frame] = source->_spr[frame]->MakeCopy();
            target->_sprOffset[frame] = source->_sprOffset[frame];
            target->_sprMotion[frame] = source->_sprMotion[frame];
        }

        target->_stateAnim = source->_stateAnim;
        target->_actionAnim = source->_actionAnim;
        target->RefreshParameters();
    }

    copy->_stateAnim = _stateAnim;
    copy->_actionAnim = _actionAnim;
    copy->RefreshParameters();

    return copy;
}

auto SpriteSheet::FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t
{
    return GetCurrentSprite()->FillData(dbuf, pos, colors);
}

void SpriteSheet::SetFrame(int32_t index, shared_ptr<Sprite> sprite, ipos32 offset, ipos32 motion)
{
    FO_VERIFY_AND_THROW(index >= 0 && index < _framesCount, "Sprite frame index is out of range", index, _framesCount);
    FO_VERIFY_AND_THROW(sprite, "Sprite frame is null");

    _spr[index] = std::move(sprite);
    _sprOffset[index] = offset;
    _sprMotion[index] = motion;

    if (index == _curIndex) {
        RefreshParameters();
    }
}

void SpriteSheet::Prewarm()
{
    auto direction = GetCurrentDirection();
    int32_t frame = _sprMngr->Random(0, direction->_framesCount - 1);
    timespan start = frame == 0 ? timespan::zero : direction->_frameEnds[frame - 1];
    if (direction->_duration > timespan::zero) {
        _phase = start.div<float64_t>(direction->_duration);
    }
    else {
        _phase = direction->_framesCount > 1 ? numeric_cast<float64_t>(frame) / (direction->_framesCount - 1) : 0.0;
    }
    RefreshParameters();
}

auto SpriteSheet::GetTime() const -> float32_t
{
    return numeric_cast<float32_t>(_phase);
}

void SpriteSheet::SetTime(float32_t normalized_time)
{
    FO_VERIFY_AND_THROW(std::isfinite(normalized_time), "Sprite normalized time must be finite");

    _phase = std::clamp(numeric_cast<float64_t>(normalized_time), 0.0, 1.0);
    _startTick = _sprMngr->GetTimer().GetFrameTime();
    RefreshParameters();
}

void SpriteSheet::SetDirection(mdir dir)
{
    if (_playing) {
        (void)Update();
    }

    _curDir = _dirCount == 1 ? hdir::NorthEast : dir.hex();
    RefreshParameters();
}

void SpriteSheet::Play(hstring anim_name, bool looped, bool reversed)
{
    ignore_unused(anim_name);
    bool animated = false;

    for (int32_t dir = 0; dir < _dirCount; dir++) {
        auto direction = GetDirection(hdir(dir));
        animated = animated || (direction->_framesCount > 1 && direction->_duration > timespan::zero);
    }

    if (!animated) {
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
    _playing = false;
}

auto SpriteSheet::Update() -> bool
{
    nanotime current = _sprMngr->GetTimer().GetFrameTime();
    timespan elapsed = current - _startTick;
    _startTick = current;

    return AdvanceTime(elapsed < timespan::zero ? timespan::zero : elapsed);
}

auto SpriteSheet::AdvanceTime(timespan elapsed_time) -> bool
{
    FO_VERIFY_AND_THROW(elapsed_time >= timespan::zero, "Sprite elapsed time must be nonnegative");

    if (!_playing) {
        return false;
    }

    _startTick = _sprMngr->GetTimer().GetFrameTime();
    timespan duration = GetDuration();

    if (duration == timespan::zero) {
        return true;
    }

    float64_t next = _phase + (_reversed ? -1.0 : 1.0) * elapsed_time.div<float64_t>(duration);

    if (_looped) {
        _phase = std::fmod(next, 1.0);

        if (_phase < 0) {
            _phase += 1.0;
        }
    }
    else {
        _phase = std::clamp(next, 0.0, 1.0);

        if ((_reversed && next <= 0) || (!_reversed && next >= 1.0)) {
            _playing = false;
        }
    }

    RefreshParameters();

    return _playing;
}

void SpriteSheet::RefreshParameters()
{
    auto direction = GetCurrentDirection();
    _curIndex = GetFrameIndexAtPhase(_phase, _playing && _reversed);
    _size = direction->_spr[_curIndex]->GetSize();
    _offset = direction->_sprOffset[_curIndex];
}

auto SpriteSheet::GetFrameIndexAtPhase(float64_t phase, bool reversed) const -> int32_t
{
    FO_VERIFY_AND_THROW(std::isfinite(phase) && phase >= 0 && phase <= 1, "Sprite phase must be finite and within 0..1", phase);

    auto direction = GetCurrentDirection();
    if (direction->_duration == timespan::zero) {
        return iround<int32_t>(phase * (direction->_framesCount - 1));
    }

    float64_t position = phase * numeric_cast<float64_t>(direction->_duration.nanoseconds());
    auto frame_before_position = [](timespan end, float64_t value) { return numeric_cast<float64_t>(end.nanoseconds()) < value; };
    auto position_before_frame = [](float64_t value, timespan end) { return value < numeric_cast<float64_t>(end.nanoseconds()); };
    auto found = reversed ? std::lower_bound(direction->_frameEnds.begin(), direction->_frameEnds.end(), position, frame_before_position) : std::upper_bound(direction->_frameEnds.begin(), direction->_frameEnds.end(), position, position_before_frame);

    return std::min(numeric_cast<int32_t>(std::distance(direction->_frameEnds.begin(), found)), direction->_framesCount - 1);
}

auto SpriteSheet::GetSprite(int32_t num_frm) const -> ptr<const Sprite>
{
    FO_VERIFY_AND_THROW(num_frm >= 0, "Sprite frame index must be nonnegative", num_frm);

    auto direction = GetCurrentDirection();

    return direction->_spr[num_frm % direction->_framesCount];
}

auto SpriteSheet::GetSprite(int32_t num_frm) -> ptr<Sprite>
{
    FO_VERIFY_AND_THROW(num_frm >= 0, "Sprite frame index must be nonnegative", num_frm);

    auto direction = GetCurrentDirection();

    return direction->_spr[num_frm % direction->_framesCount];
}

auto SpriteSheet::GetSpriteOffset(int32_t num_frm) const -> ipos32
{
    FO_VERIFY_AND_THROW(num_frm >= 0, "Sprite frame index must be nonnegative", num_frm);

    auto direction = GetCurrentDirection();

    return direction->_sprOffset[num_frm % direction->_framesCount];
}

auto SpriteSheet::GetDirection(mdir dir) const -> nptr<const SpriteSheet>
{
    return _dirCount == 1 ? nptr<const SpriteSheet> {this} : nptr<const SpriteSheet> {_dirs[dir.hex().value()]};
}

auto SpriteSheet::GetDirection(mdir dir) -> nptr<SpriteSheet>
{
    return _dirCount == 1 ? nptr<SpriteSheet> {this} : nptr<SpriteSheet> {_dirs[dir.hex().value()]};
}

DefaultSpriteFactory::DefaultSpriteFactory(ptr<SpriteManager> spr_mngr, ptr<ResourceManager> res_mngr) :
    _sprMngr {spr_mngr},
    _resMngr {res_mngr}
{
    _borderBuf.resize(AppRender::MAX_ATLAS_SIZE);
}

auto DefaultSpriteFactory::LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite>
{
    FO_TRACE_ZONE(Render);

    auto file = _resMngr->GetResources()->ReadFile(path);

    if (!file) {
        return nullptr;
    }

    SpriteResourceData resource = ReadSpriteResource(file.GetDataSpan());
    FO_VERIFY_AND_THROW(resource.Animation.Sprite.has_value(), "Sprite resource has no sprite animation info", path);
    const SpriteInfo& sprite_info = *resource.Animation.Sprite;
    uint8_t direction_count = numeric_cast<uint8_t>(resource.Directions.size());
    FO_VERIFY_AND_THROW(direction_count == 1 || direction_count == GameSettings::MAP_DIR_COUNT, "Sprite file direction count is unsupported", direction_count, GameSettings::MAP_DIR_COUNT);

    shared_ptr<Sprite> result;

    if (sprite_info.Directions.front().Frames.size() > 1 || direction_count > 1) {
        vector<vector<timespan>> timelines;

        for (const auto& direction : sprite_info.Directions) {
            vector<timespan> durations;

            for (const auto& frame : direction.Frames) {
                durations.emplace_back(frame.Duration);
            }

            timelines.emplace_back(std::move(durations));
        }

        auto anim = safe_alloc::make_shared<SpriteSheet>(_sprMngr, timelines);

        for (uint8_t i = 0; i < direction_count; i++) {
            mdir dir = hdir(i);
            auto dir_anim = anim->GetDirection(dir);
            FO_VERIFY_AND_THROW(dir_anim, "Sprite sheet is missing the requested direction");
            SpriteResourceDirectionData& direction = resource.Directions[i];

            for (size_t j = 0; j < direction.Frames.size(); j++) {
                SpriteResourceFrameData& frame = direction.Frames[j];

                if (!frame.SharedFrameIndex.has_value()) {
                    auto spr = FillAtlas(atlas_type, frame.Size, frame.Offset, frame.Pixels.data(), std::move(frame.Mesh));
                    ipos32 offset = spr->GetOffset();
                    dir_anim->SetFrame(numeric_cast<int32_t>(j), std::move(spr), offset, frame.NextOffset);
                }
                else {
                    uint16_t index = *frame.SharedFrameIndex;
                    ipos32 reference_offset = dir_anim->GetSpriteOffset(index);
                    ipos32 offset {
                        numeric_cast<int32_t>(numeric_cast<int64_t>(reference_offset.x) + frame.Offset.x - direction.Frames[index].Offset.x),
                        numeric_cast<int32_t>(numeric_cast<int64_t>(reference_offset.y) + frame.Offset.y - direction.Frames[index].Offset.y),
                    };

                    dir_anim->SetFrame(numeric_cast<int32_t>(j), dir_anim->GetSprite(index)->MakeCopy(), offset, frame.NextOffset);
                }
            }

            dir_anim->RefreshParameters();
        }

        anim->RefreshParameters();
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

auto DefaultSpriteFactory::LoadSpriteAsQuad(hstring path, AtlasType atlas_type) -> shared_ptr<AtlasSprite>
{
    FO_TRACE_ZONE(Render);

    auto file = _resMngr->GetResources()->ReadFile(path);

    if (!file) {
        return nullptr;
    }

    SpriteResourceData resource = ReadSpriteResource(file.GetDataSpan());
    FO_VERIFY_AND_THROW(resource.Directions.size() == 1, "Image resource must contain exactly one direction", path, resource.Directions.size());
    SpriteResourceDirectionData& direction = resource.Directions.front();
    FO_VERIFY_AND_THROW(direction.Frames.size() == 1, "Image resource must contain exactly one frame", path, direction.Frames.size());
    SpriteResourceFrameData& frame = direction.Frames.front();
    FO_VERIFY_AND_THROW(!frame.SharedFrameIndex.has_value(), "Single-frame image resource cannot contain a shared-frame reference", path);

    SpriteResourceImageData image = ExtractSpriteResourceFrameImage(std::move(frame));
    return FillAtlas(atlas_type, image.Size, {}, image.Pixels.data(), std::nullopt);
}

auto DefaultSpriteFactory::CreateDummySprite(ptr<SpriteManager> spr_mngr, AtlasType atlas_type) -> shared_ptr<AtlasSprite>
{
    auto [atlas, atlas_allocation, pos] = spr_mngr->GetAtlasMngr()->FindAtlasPlace(atlas_type, DUMMY_SPRITE_SIZE);
    auto tex = atlas->GetTexture();
    const_span<ucolor> dummy_color {&DUMMY_SPRITE_COLOR, 1};

    tex->UpdateTextureRegion(pos, DUMMY_SPRITE_SIZE, dummy_color);
    tex->UpdateTextureRegion({pos.x, pos.y - 1}, {1, 1}, dummy_color);
    tex->UpdateTextureRegion({pos.x, pos.y + 1}, {1, 1}, dummy_color);

    const ucolor vertical_border[3] = {DUMMY_SPRITE_COLOR, DUMMY_SPRITE_COLOR, DUMMY_SPRITE_COLOR};
    tex->UpdateTextureRegion({pos.x - 1, pos.y - 1}, {1, 3}, {vertical_border, 3});
    tex->UpdateTextureRegion({pos.x + 1, pos.y - 1}, {1, 3}, {vertical_border, 3});

    frect32 atlas_rect;
    atlas_rect.x = numeric_cast<float32_t>(pos.x) / numeric_cast<float32_t>(atlas->GetSize().width);
    atlas_rect.y = numeric_cast<float32_t>(pos.y) / numeric_cast<float32_t>(atlas->GetSize().height);
    atlas_rect.width = 1.0f / numeric_cast<float32_t>(atlas->GetSize().width);
    atlas_rect.height = 1.0f / numeric_cast<float32_t>(atlas->GetSize().height);

    vector<bool> hit_test_data(1, spr_mngr->CheckHitTest(numeric_cast<int32_t>(DUMMY_SPRITE_COLOR.comp.a)));
    return safe_alloc::make_shared<AtlasSprite>(spr_mngr, DUMMY_SPRITE_SIZE, ipos32 {}, atlas, std::move(atlas_allocation), atlas_rect, std::move(hit_test_data));
}

auto DefaultSpriteFactory::FillAtlas(AtlasType atlas_type, isize32 size, ipos32 offset, nptr<const ucolor> pixels, optional<SpriteMeshData> mesh_data) -> shared_ptr<AtlasSprite>
{
    FO_TRACE_ZONE(Render);

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

    frect32 atlas_rect;
    atlas_rect.x = numeric_cast<float32_t>(pos.x) / numeric_cast<float32_t>(atlas->GetSize().width);
    atlas_rect.y = numeric_cast<float32_t>(pos.y) / numeric_cast<float32_t>(atlas->GetSize().height);
    atlas_rect.width = numeric_cast<float32_t>(size.width) / numeric_cast<float32_t>(atlas->GetSize().width);
    atlas_rect.height = numeric_cast<float32_t>(size.height) / numeric_cast<float32_t>(atlas->GetSize().height);

    return safe_alloc::make_shared<AtlasSprite>(_sprMngr, size, offset, atlas, std::move(atlas_allocation), atlas_rect, std::move(hit_test_data), std::move(mesh_data));
}

FO_END_NAMESPACE

