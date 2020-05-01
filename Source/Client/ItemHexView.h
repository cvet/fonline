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

#include "EffectManager.h"
#include "Entity.h"
#include "ItemView.h"
#include "ResourceManager.h"
#include "Timer.h"

class ItemHexView;
using ItemHexViewVec = vector<ItemHexView*>;
using ItemHexViewMap = map<uint, ItemHexView*>;

class ItemHexView : public ItemView
{
public:
    ItemHexView(uint id, ProtoItem* proto, Properties& props, ResourceManager& res_mngr, EffectManager& effect_mngr,
        GameTimer& game_time);
    ItemHexView(uint id, ProtoItem* proto, UCharVecVec* props_data, ResourceManager& res_mngr,
        EffectManager& effect_mngr, GameTimer& game_time);
    ItemHexView(uint id, ProtoItem* proto, UCharVecVec* props_data, int hx, int hy, int* hex_scr_x, int* hex_scr_y,
        ResourceManager& res_mngr, EffectManager& effect_mngr, GameTimer& game_time);
    bool IsAnimated() { return isAnimated; }
    bool IsDrawContour() { return /*IsFocused && */ !IsAnyScenery() && !GetIsNoHighlight() && !GetIsBadItem(); }
    bool IsTransparent() { return maxAlpha < 0xFF; }
    bool IsFullyTransparent() { return maxAlpha == 0; }
    void RefreshAnim();
    void RestoreAlpha() { Alpha = maxAlpha; }
    void RefreshAlpha() { maxAlpha = (IsColorize() ? GetAlpha() : 0xFF); }
    void SetSprite(Sprite* spr);
    int GetEggType();
    void Finish();
    bool IsFinishing();
    bool IsFinish();
    void StopFinishing();
    void Process();
    bool IsDynamicEffect() { return isEffect && (effSx || effSy); }
    void SetEffect(float sx, float sy, uint dist, int dir);
    UShortPair GetEffectStep();
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
    UShortPairVec EffSteps {};

private:
    ItemHexView(uint id, ProtoItem* proto, ResourceManager& res_mngr, EffectManager& effect_mngr, GameTimer& game_time);
    void AfterConstruction();
    void SetFade(bool fade_up);

    ResourceManager& resMngr;
    EffectManager& effectMngr;
    GameTimer& gameTime;
    int curSpr {};
    int begSpr {};
    int endSpr {};
    uint animBegSpr {};
    uint animEndSpr {};
    uint animTick {};
    uchar maxAlpha {0xFF};
    bool isAnimated {};
    uint animNextTick {};
    bool finishing {};
    uint finishingTime {};
    bool isEffect {};
    float effSx {};
    float effSy {};
    int effStartX {};
    int effStartY {};
    float effCurX {};
    float effCurY;
    uint effDist {};
    uint effLastTick {};
    int effDir {};
    bool fading {};
    uint fadingTick {};
    bool fadeUp {};
};
