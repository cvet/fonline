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

#include "Geometry.h"
#include "HexView.h"
#include "ItemView.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE();

class ItemHexView final : public ItemView, public HexView
{
public:
    ItemHexView() = delete;
    ItemHexView(MapView* map, ident_t id, const ProtoItem* proto, const Properties* props = nullptr);
    ItemHexView(const ItemHexView&) = delete;
    ItemHexView(ItemHexView&&) noexcept = delete;
    auto operator=(const ItemHexView&) = delete;
    auto operator=(ItemHexView&&) noexcept = delete;
    ~ItemHexView() override = default;

    [[nodiscard]] auto IsDrawContour() const noexcept -> bool { return !GetIsWall() && !GetIsScenery() && !GetNoHighlight() && !GetBadItem(); }
    [[nodiscard]] auto GetEggType() const noexcept -> EggAppearenceType;
    [[nodiscard]] auto IsNeedProcess() const -> bool { return _isMoving || IsFading(); }
    [[nodiscard]] auto GetAnim() const -> const Sprite* { return _anim.get(); }
    [[nodiscard]] auto IsMoving() const noexcept -> bool { return _isMoving; }
    [[nodiscard]] auto HasMultihexEntries() const noexcept -> bool { return !!_multihexEntries; }
    [[nodiscard]] auto GetMultihexEntries() const noexcept -> const vector<mpos>& { return *_multihexEntries; }

    void Init();
    void Process();
    void RefreshAlpha();
    void PlayAnim(hstring anim_name, bool looped, bool reversed);
    void StopAnim();
    void SetAnimTime(float32 normalized_time);
    void SetAnimDir(uint8 dir);
    void RefreshAnim();
    void MoveToHex(mpos hex, float32 speed);
    void RefreshOffs();
    void SetMultihexEntries(vector<mpos> entries);

private:
    void SetupSprite(MapSprite* mspr) override;

    shared_ptr<Sprite> _anim {};
    hstring _animName {};
    bool _animLooped {};
    bool _animReversed {};
    bool _animStopped {};
    float32 _animTime {};
    uint8 _animDir {};

    bool _isMoving {};
    float32 _moveSpeed {};
    fpos32 _moveStepOffset {};
    fpos32 _moveStartOffset {};
    fpos32 _moveCurOffset {};
    float32 _moveWholeDist {};
    nanotime _moveUpdateLastTime {};
    uint8 _moveDir {};
    vector<mpos> _moveSteps {};

    unique_ptr<vector<mpos>> _multihexEntries {};
};

FO_END_NAMESPACE();
