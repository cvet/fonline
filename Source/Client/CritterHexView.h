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

#include "3dStuff.h"
#include "Application.h"
#include "CritterView.h"
#include "EntityProtos.h"
#include "SpriteManager.h"

class MapView;
class ItemView;

class CritterHexView final : public CritterView
{
public:
    CritterHexView() = delete;
    CritterHexView(MapView* map, uint id, const ProtoCritter* proto);
    CritterHexView(const CritterHexView&) = delete;
    CritterHexView(CritterHexView&&) noexcept = delete;
    auto operator=(const CritterHexView&) = delete;
    auto operator=(CritterHexView&&) noexcept = delete;
    ~CritterHexView() override;

    [[nodiscard]] auto GetMap() -> MapView* { return _map; }
    [[nodiscard]] auto GetMap() const -> const MapView* { return _map; }
    [[nodiscard]] auto IsCombatMode() const -> bool;
    [[nodiscard]] auto IsLastHexes() const -> bool;
    [[nodiscard]] auto IsHaveLightSources() const -> bool;
    [[nodiscard]] auto IsNeedMove() const -> bool { return !MoveSteps.empty() && !IsWalkAnim(); }
    [[nodiscard]] auto IsNeedReset() const -> bool;
    [[nodiscard]] auto GetAnim1() const -> uint;
    [[nodiscard]] auto GetAnim2() const -> uint;
    [[nodiscard]] auto IsAnimAvailable(uint anim1, uint anim2) const -> bool;
    [[nodiscard]] auto IsAnim() const -> bool { return !_animSequence.empty(); }
    [[nodiscard]] auto IsModel() const -> bool { return _model != nullptr; }
    [[nodiscard]] auto GetModel() -> ModelInstance* { NON_CONST_METHOD_HINT_ONELINE() return _model; }
    [[nodiscard]] auto IsWalkAnim() const -> bool;
    [[nodiscard]] auto GetWalkHexOffsets(uchar dir) const -> tuple<short, short>;
    [[nodiscard]] auto IsFinishing() const -> bool;
    [[nodiscard]] auto IsFinished() const -> bool;
    [[nodiscard]] auto GetTextRect() const -> IRect;
    [[nodiscard]] auto GetAttackDist() -> uint;

    [[nodiscard]] auto PopLastHex() -> tuple<ushort, ushort>;

    void AddItem(ItemView* item) override;
    void DeleteItem(ItemView* item, bool animate) override;
    void Init();
    void Finish();
    void FixLastHexes();
    void RefreshModel();
    void ChangeDir(uchar dir, bool animate);
    void Animate(uint anim1, uint anim2, ItemView* item);
    void AnimateStay();
    void Action(int action, int action_ext, ItemView* item, bool local_call);
    void Process();
    void DrawStay(IRect r);
    void Move(uchar dir);
    void ProcessAnim(bool animate_stay, bool is2d, uint anim1, uint anim2, ItemView* item);
    void ResetOk();
    void SetSprRect();
    void ClearAnim();
    void SetOffs(short set_ox, short set_oy, bool move_text);
    void ChangeOffs(short change_ox, short change_oy, bool move_text);
    void AddOffsExt(short ox, short oy);
    void SetText(string_view str, uint color, uint text_delay);
    void DrawTextOnHead();
    void GetNameTextInfo(bool& name_visible, int& x, int& y, int& w, int& h, int& lines) const;
    void NextAnim(bool erase_front);

    RenderEffect* DrawEffect {};
    bool IsRunning {};
    vector<pair<ushort, ushort>> MoveSteps {};
    int CurMoveStep {};
    bool Visible {true};
    uchar Alpha {};
    IRect DRect {};
    bool SprDrawValid {};
    Sprite* SprDraw {};
    uint SprId {};
    short SprOx {};
    short SprOy {};
    uint FadingTick {};

private:
    struct CritterAnim
    {
        AnyFrames* Anim {};
        uint AnimTick {};
        uint BeginFrm {};
        uint EndFrm {};
        bool MoveText {};
        int DirOffs {};
        uint IndAnim1 {};
        uint IndAnim2 {};
        ItemView* ActiveItem {};
    };

    [[nodiscard]] auto GetLayers3dData() -> int*;
    [[nodiscard]] auto GetFadeAlpha() -> uchar;
    [[nodiscard]] auto GetCurAnim() -> CritterAnim* { return IsAnim() ? &_animSequence[0] : nullptr; }

    void SetFade(bool fade_up);

    MapView* _map;
    bool _needReset {};
    uint _resetTick {};
    uint _curSpr {};
    uint _lastEndSpr {};
    uint _animStartTick {};
    int _modelLayers[LAYERS3D_COUNT] {};
    CritterAnim _stayAnim {};
    vector<CritterAnim> _animSequence {};
    uchar _staySprDir {};
    uint _staySprTick {};
    uint _finishingTime {};
    bool _fadingEnable {};
    bool _fadeUp {};
    IRect _textRect {};
    uint _tickFidget {};
    string _strTextOnHead {};
    uint _tickStartText {};
    uint _tickTextDelay {};
    uint _textOnHeadColor {COLOR_CRITTER_NAME};
    ModelInstance* _model {};
    ModelInstance* _modelStay {};
    short _oxExtI {};
    short _oyExtI {};
    float _oxExtF {};
    float _oyExtF {};
    float _oxExtSpeed {};
    float _oyExtSpeed {};
    uint _offsExtNextTick {};
    string _nameOnHead {};
    vector<tuple<ushort, ushort>> _lastHexes {};
};
