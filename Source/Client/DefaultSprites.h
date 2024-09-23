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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

class AtlasSprite : public Sprite
{
public:
    explicit AtlasSprite(SpriteManager& spr_mngr);
    AtlasSprite(const AtlasSprite&) = delete;
    AtlasSprite(AtlasSprite&&) noexcept = default;
    auto operator=(const AtlasSprite&) = delete;
    auto operator=(AtlasSprite&&) noexcept -> AtlasSprite& = delete;
    ~AtlasSprite() override;

    [[nodiscard]] auto IsHitTest(int x, int y) const -> bool override;
    [[nodiscard]] auto GetBatchTex() const -> RenderTexture* override { return Atlas->MainTex; }
    [[nodiscard]] auto IsCopyable() const -> bool override { return true; }
    [[nodiscard]] auto MakeCopy() const -> shared_ptr<Sprite> override;

    auto FillData(RenderDrawBuffer* dbuf, const FRect& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override;

    TextureAtlas* Atlas {};
    TextureAtlas::SpaceNode* AtlasNode {};
    FRect AtlasRect {};
    vector<bool> HitTestData {};
};

class SpriteSheet final : public Sprite
{
public:
    SpriteSheet(SpriteManager& spr_mngr, uint frames, uint ticks, uint dirs);
    SpriteSheet(const SpriteSheet&) = delete;
    SpriteSheet(SpriteSheet&&) noexcept = default;
    auto operator=(const SpriteSheet&) = delete;
    auto operator=(SpriteSheet&&) noexcept -> SpriteSheet& = delete;
    ~SpriteSheet() override = default;

    [[nodiscard]] auto IsHitTest(int x, int y) const -> bool override;
    [[nodiscard]] auto GetBatchTex() const -> RenderTexture* override;
    [[nodiscard]] auto IsCopyable() const -> bool override { return true; }
    [[nodiscard]] auto MakeCopy() const -> shared_ptr<Sprite> override;
    [[nodiscard]] auto GetCurSpr() const -> const Sprite*;
    [[nodiscard]] auto GetCurSpr() -> Sprite*;
    [[nodiscard]] auto GetSpr(uint num_frm) const -> const Sprite*;
    [[nodiscard]] auto GetSpr(uint num_frm) -> Sprite*;
    [[nodiscard]] auto GetDir(uint dir) const -> const SpriteSheet*;
    [[nodiscard]] auto GetDir(uint dir) -> SpriteSheet*;
    [[nodiscard]] auto IsPlaying() const -> bool override { return _playing; }

    auto FillData(RenderDrawBuffer* dbuf, const FRect& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override;
    void Prewarm() override;
    void SetTime(float normalized_time) override;
    void SetDir(uint8 dir) override;
    void SetDirAngle(short dir_angle) override;
    void Play(hstring anim_name, bool looped, bool reversed) override;
    void Stop() override;
    auto Update() -> bool override;

    // Todo: incapsulate sprite sheet data
    vector<shared_ptr<Sprite>> Spr {};
    vector<IPoint> SprOffset {};
    uint CntFrm {}; // Todo: Spr.size()
    uint WholeTicks {};
    CritterStateAnim StateAnim {};
    CritterActionAnim ActionAnim {};
    uint DirCount {};
    shared_ptr<SpriteSheet> Dirs[GameSettings::MAP_DIR_COUNT - 1] {};

private:
    void RefreshParams();

    uint8 _curDir {};
    uint _curIndex {};
    bool _playing {};
    bool _looped {};
    bool _reversed {};
    time_point _startTick {};
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
    void FillAtlas(AtlasSprite* atlas_spr, AtlasType atlas_type, const ucolor* data);

    SpriteManager& _sprMngr;
    vector<ucolor> _borderBuf {};
};
