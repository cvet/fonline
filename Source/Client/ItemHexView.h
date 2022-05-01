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

#pragma once

#include "Common.h"

#include "ItemView.h"
#include "ResourceManager.h"

class MapView;

class ItemHexView final : public ItemView
{
public:
    ItemHexView() = delete;
    ItemHexView(MapView* map, uint id, const ProtoItem* proto);
    ItemHexView(MapView* map, uint id, const ProtoItem* proto, const Properties& props);
    ItemHexView(MapView* map, uint id, const ProtoItem* proto, vector<vector<uchar>>* props_data);
    ItemHexView(MapView* map, uint id, const ProtoItem* proto, vector<vector<uchar>>* props_data, ushort hx, ushort hy, int* hex_scr_x, int* hex_scr_y);
    ItemHexView(const ItemHexView&) = delete;
    ItemHexView(ItemHexView&&) noexcept = delete;
    auto operator=(const ItemHexView&) = delete;
    auto operator=(ItemHexView&&) noexcept = delete;
    ~ItemHexView() override = default;

    [[nodiscard]] auto GetMap() -> MapView* { return _map; }
    [[nodiscard]] auto GetMap() const -> const MapView* { return _map; }
    [[nodiscard]] auto IsAnimated() const -> bool { return _isAnimated; }
    [[nodiscard]] auto IsDrawContour() const -> bool { return /*IsFocused && */ !IsAnyScenery() && !GetIsNoHighlight() && !GetIsBadItem(); }
    [[nodiscard]] auto IsTransparent() const -> bool { return _maxAlpha < 0xFF; }
    [[nodiscard]] auto IsFullyTransparent() const -> bool { return _maxAlpha == 0; }
    [[nodiscard]] auto GetEggType() const -> int;
    [[nodiscard]] auto IsFinishing() const -> bool;
    [[nodiscard]] auto IsFinished() const -> bool;
    [[nodiscard]] auto IsDynamicEffect() const -> bool { return _isEffect && (_effSx != 0.0f || _effSy != 0.0f); }
    [[nodiscard]] auto GetEffectStep() const -> pair<ushort, ushort>;

    void RefreshAnim();
    void RestoreAlpha() { Alpha = _maxAlpha; }
    void RefreshAlpha() { _maxAlpha = IsColorize() ? GetAlpha() : 0xFF; }
    void SetSprite(Sprite* spr);
    void Finish();
    void StopFinishing();
    void Process();
    void SetEffect(float sx, float sy, uint dist, int dir);
    void SkipFade();
    void StartAnimate();
    void StopAnimate();
    void SetAnimFromEnd();
    void SetAnimFromStart();
    void SetAnim(uint beg, uint end);
    void SetSprStart();
    void SetSprEnd();
    void SetSpr(uint num_spr);
    void SetAnimOffs();
    void SetStayAnim();
    void SetShowAnim();
    void SetHideAnim();

    uint SprId {};
    short ScrX {};
    short ScrY {};
    int* HexScrX {};
    int* HexScrY {};
    uchar Alpha {};
    AnyFrames* Anim {};
    bool SprDrawValid {};
    Sprite* SprDraw {};
    Sprite* SprTemp {};
    RenderEffect* DrawEffect {};
    float EffOffsX {};
    float EffOffsY {};
    vector<pair<ushort, ushort>> EffSteps {};

private:
    void AfterConstruction();
    void SetFade(bool fade_up);

    MapView* _map;
    uint _curSpr {};
    uint _begSpr {};
    uint _endSpr {};
    uint _animBegSpr {};
    uint _animEndSpr {};
    uint _animTick {};
    uchar _maxAlpha {0xFF};
    bool _isAnimated {};
    uint _animNextTick {};
    bool _finishing {};
    uint _finishingTime {};
    bool _isEffect {};
    float _effSx {};
    float _effSy {};
    int _effStartX {};
    int _effStartY {};
    float _effCurX {};
    float _effCurY {};
    uint _effDist {};
    uint _effLastTick {};
    int _effDir {};
    bool _fading {};
    uint _fadingTick {};
    bool _fadeUp {};
};
