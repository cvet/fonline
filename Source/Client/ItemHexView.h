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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
    ItemHexView(MapView* map, uint id, const ProtoItem* proto, const Properties& props);
    ItemHexView(MapView* map, uint id, const ProtoItem* proto, const vector<vector<uchar>>* props_data);
    ItemHexView(MapView* map, uint id, const ProtoItem* proto, const vector<vector<uchar>>* props_data, ushort hx, ushort hy);
    ItemHexView(const ItemHexView&) = delete;
    ItemHexView(ItemHexView&&) noexcept = delete;
    auto operator=(const ItemHexView&) = delete;
    auto operator=(ItemHexView&&) noexcept = delete;
    ~ItemHexView() override = default;

    [[nodiscard]] auto GetMap() -> MapView* { return _map; }
    [[nodiscard]] auto GetMap() const -> const MapView* { return _map; }
    [[nodiscard]] auto IsDrawContour() const -> bool { return /*IsFocused && */ !IsAnyScenery() && !GetIsNoHighlight() && !GetIsBadItem(); }
    [[nodiscard]] auto IsTransparent() const -> bool { return _maxAlpha < 0xFF; }
    [[nodiscard]] auto IsFullyTransparent() const -> bool { return _maxAlpha == 0; }
    [[nodiscard]] auto GetEggType() const -> EggAppearenceType;
    [[nodiscard]] auto IsFinishing() const -> bool { return _finishing; }
    [[nodiscard]] auto IsFinished() const -> bool;
    [[nodiscard]] auto IsNeedProcess() const -> bool { return _begFrm != _endFrm || (_isEffect && !_finishing) || _isShowAnim || (_isDynamicEffect && !_finishing) || _fading; }

    void RefreshAnim();
    void RestoreAlpha() { Alpha = _maxAlpha; }
    void RefreshAlpha() { _maxAlpha = IsColorize() ? GetAlpha() : 0xFF; }
    void SetSprite(Sprite* spr);
    void Finish();
    void StopFinishing();
    void Process();
    void SetEffect(ushort to_hx, ushort to_hy);
    void SkipFade();
    void SetAnimFromEnd();
    void SetAnimFromStart();
    void SetAnim(uint beg, uint end);
    void RefreshOffs();
    void SetStayAnim();
    void SetShowAnim();
    void SetHideAnim();

    uint SprId {};
    int ScrX {};
    int ScrY {};
    uchar Alpha {};

    RenderEffect* DrawEffect {};
    Sprite* SprDraw {};
    bool SprDrawValid {};

private:
    ItemHexView(MapView* map, uint id, const ProtoItem* proto);

    void AfterConstruction();
    void SetFade(bool fade_up);
    void SetSpr(uint num_spr);

    MapView* _map;

    AnyFrames* _anim {};
    uint _curFrm {};
    uint _begFrm {};
    uint _endFrm {};
    uint _animBegFrm {};
    uint _animEndFrm {};
    uint _animTick {};
    uchar _maxAlpha {0xFF};
    bool _isShowAnim {};
    uint _animNextTick {};

    bool _isEffect {};
    bool _isDynamicEffect {};
    float _effSx {};
    float _effSy {};
    int _effStartX {};
    int _effStartY {};
    float _effCurX {};
    float _effCurY {};
    uint _effDist {};
    uint _effLastTick {};
    int _effDir {};
    vector<pair<ushort, ushort>> _effSteps {};

    bool _fading {};
    uint _fadingEndTick {};
    bool _fadeUp {};

    bool _finishing {};
    uint _finishingTime {};
};
