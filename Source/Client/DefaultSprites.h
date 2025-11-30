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

#pragma once

#include "Common.h"

#include "Rendering.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE();

class AtlasSprite : public Sprite
{
public:
    explicit AtlasSprite(SpriteManager& spr_mngr, isize32 size, ipos32 offset, TextureAtlas* atlas, TextureAtlas::SpaceNode* atlas_node, frect32 atlas_rect, vector<bool>&& hit_data);
    AtlasSprite(const AtlasSprite&) = delete;
    AtlasSprite(AtlasSprite&&) noexcept = default;
    auto operator=(const AtlasSprite&) = delete;
    auto operator=(AtlasSprite&&) noexcept -> AtlasSprite& = delete;
    ~AtlasSprite() override;

    [[nodiscard]] auto IsHitTest(ipos32 pos) const -> bool override;
    [[nodiscard]] auto GetAtlas() const -> const TextureAtlas* { return _atlas.get(); }
    [[nodiscard]] auto GetAtlas() -> TextureAtlas* { return _atlas.get(); }
    [[nodiscard]] auto GetBatchTexture() const -> const RenderTexture* override { return _atlas->GetTexture(); }
    [[nodiscard]] auto IsCopyable() const -> bool override { return true; }
    [[nodiscard]] auto MakeCopy() const -> shared_ptr<Sprite> override;
    [[nodiscard]] auto GetAtlasRect() const -> const frect32& { return _atlasRect; }

    auto FillData(RenderDrawBuffer* dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override;

protected:
    raw_ptr<TextureAtlas> _atlas {};
    raw_ptr<TextureAtlas::SpaceNode> _atlasNode {};
    frect32 _atlasRect {};
    vector<bool> _hitTestData {};
};

class SpriteSheet final : public Sprite
{
    friend class DefaultSpriteFactory;
    friend class ResourceManager;

public:
    SpriteSheet(SpriteManager& spr_mngr, int32 frames, int32 ticks, int32 dirs);
    SpriteSheet(const SpriteSheet&) = delete;
    SpriteSheet(SpriteSheet&&) noexcept = default;
    auto operator=(const SpriteSheet&) = delete;
    auto operator=(SpriteSheet&&) noexcept -> SpriteSheet& = delete;
    ~SpriteSheet() override = default;

    [[nodiscard]] auto IsHitTest(ipos32 pos) const -> bool override;
    [[nodiscard]] auto GetBatchTexture() const -> const RenderTexture* override;
    [[nodiscard]] auto IsCopyable() const -> bool override { return true; }
    [[nodiscard]] auto MakeCopy() const -> shared_ptr<Sprite> override;
    [[nodiscard]] auto GetCurSpr() const -> const Sprite*;
    [[nodiscard]] auto GetCurSpr() -> Sprite*;
    [[nodiscard]] auto GetSpr(int32 num_frm) const -> const Sprite*;
    [[nodiscard]] auto GetSpr(int32 num_frm) -> Sprite*;
    [[nodiscard]] auto GetDir(int32 dir) const -> const SpriteSheet*;
    [[nodiscard]] auto GetDir(int32 dir) -> SpriteSheet*;
    [[nodiscard]] auto IsPlaying() const -> bool override { return _playing; }
    [[nodiscard]] auto GetTime() const -> float32 override;
    [[nodiscard]] auto GetWholeTicks() const noexcept -> int32 { return _wholeTicks; }
    [[nodiscard]] auto GetFramesCount() const noexcept -> int32 { return _framesCount; }
    [[nodiscard]] auto GetStateAnim() const noexcept -> CritterStateAnim { return _stateAnim; }
    [[nodiscard]] auto GetActionAnim() const noexcept -> CritterActionAnim { return _actionAnim; }
    [[nodiscard]] auto GetDirCount() const noexcept -> int32 { return _dirCount; }
    [[nodiscard]] auto GetSprOffset() const noexcept -> const vector<ipos32>& { return _sprOffset; }

    auto FillData(RenderDrawBuffer* dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override;
    void Prewarm() override;
    void SetTime(float32 normalized_time) override;
    void SetDir(uint8 dir) override;
    void SetDirAngle(int16 dir_angle) override;
    void Play(hstring anim_name, bool looped, bool reversed) override;
    void Stop() override;
    auto Update() -> bool override;

private:
    void RefreshParams();

    vector<shared_ptr<Sprite>> _spr {};
    vector<ipos32> _sprOffset {};
    int32 _framesCount {};
    int32 _wholeTicks {};
    CritterStateAnim _stateAnim {};
    CritterActionAnim _actionAnim {};
    int32 _dirCount {};
    shared_ptr<SpriteSheet> _dirs[GameSettings::MAP_DIR_COUNT - 1] {};

    uint8 _curDir {};
    int32 _curIndex {};
    bool _playing {};
    bool _looped {};
    bool _reversed {};
    nanotime _startTick {};
};

class DefaultSpriteFactory : public SpriteFactory
{
public:
    explicit DefaultSpriteFactory(SpriteManager& spr_mngr);
    DefaultSpriteFactory(const DefaultSpriteFactory&) = delete;
    DefaultSpriteFactory(DefaultSpriteFactory&&) noexcept = default;
    auto operator=(const DefaultSpriteFactory&) = delete;
    auto operator=(DefaultSpriteFactory&&) noexcept -> DefaultSpriteFactory& = delete;
    ~DefaultSpriteFactory() override = default;

    [[nodiscard]] auto GetExtensions() const -> vector<string> override { return {"fofrm", "frm", "fr0", "rix", "art", "zar", "til", "mos", "bam", "png", "tga"}; }

    auto LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite> override;

private:
    auto FillAtlas(AtlasType atlas_type, isize32 size, ipos32 offset, const ucolor* data) -> shared_ptr<AtlasSprite>;

    raw_ptr<SpriteManager> _sprMngr;
    vector<ucolor> _borderBuf {};
};

FO_END_NAMESPACE();
