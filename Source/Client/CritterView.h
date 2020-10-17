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

#include "3dStuff.h"
#include "Application.h"
#include "ClientScripting.h"
#include "EffectManager.h"
#include "Entity.h"
#include "GeometryHelper.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "Settings.h"
#include "SoundManager.h"
#include "SpriteManager.h"
#include "Timer.h"

#define FO_API_CRITTER_VIEW_HEADER
#include "ScriptApi.h"

class ItemView;

class CritterView final : public Entity
{
public:
    CritterView() = delete;
    CritterView(uint id, const ProtoCritter* proto, CritterViewSettings& settings, SpriteManager& spr_mngr, ResourceManager& res_mngr, EffectManager& effect_mngr, ClientScriptSystem& script_sys, GameTimer& game_timer, bool mapper_mode);
    CritterView(const CritterView&) = delete;
    CritterView(CritterView&&) noexcept = delete;
    auto operator=(const CritterView&) = delete;
    auto operator=(CritterView&&) noexcept = delete;
    ~CritterView() override;

    [[nodiscard]] auto IsNpc() const -> bool { return IsBitSet(Flags, FCRIT_NPC); }
    [[nodiscard]] auto IsPlayer() const -> bool { return IsBitSet(Flags, FCRIT_PLAYER); }
    [[nodiscard]] auto IsChosen() const -> bool { return IsBitSet(Flags, FCRIT_CHOSEN); }
    [[nodiscard]] auto IsOnline() const -> bool { return !IsBitSet(Flags, FCRIT_DISCONNECT); }
    [[nodiscard]] auto IsOffline() const -> bool { return IsBitSet(Flags, FCRIT_DISCONNECT); }
    [[nodiscard]] auto IsAlive() const -> bool { return GetCond() == COND_ALIVE; }
    [[nodiscard]] auto IsKnockout() const -> bool { return GetCond() == COND_KNOCKOUT; }
    [[nodiscard]] auto IsDead() const -> bool { return GetCond() == COND_DEAD; }
    [[nodiscard]] auto IsCombatMode() const -> bool;
    [[nodiscard]] auto CheckFind(uchar find_type) const -> bool;
    [[nodiscard]] auto IsLastHexes() const -> bool;
    [[nodiscard]] auto CountItemPid(hash item_pid) const -> uint;
    [[nodiscard]] auto IsHaveLightSources() const -> bool;
    [[nodiscard]] auto IsNeedMove() const -> bool { return !MoveSteps.empty() && !IsWalkAnim(); }
    [[nodiscard]] auto IsNeedReset() const -> bool;
    [[nodiscard]] auto IsFree() const -> bool;
    [[nodiscard]] auto GetAnim1() const -> uint;
    [[nodiscard]] auto GetAnim2() const -> uint;
    [[nodiscard]] auto IsAnimAvailable(uint anim1, uint anim2) const -> bool;
    [[nodiscard]] auto Is3dAnim() const -> bool { return Model != nullptr; }
    [[nodiscard]] auto GetModel() const -> ModelInstance* { return Model; }
    [[nodiscard]] auto IsAnim() const -> bool { return !_animSequence.empty(); }
    [[nodiscard]] auto IsWalkAnim() const -> bool;
    [[nodiscard]] auto GetWalkHexOffsets(uchar dir) const -> tuple<short, short>;
    [[nodiscard]] auto IsFinishing() const -> bool;
    [[nodiscard]] auto IsFinish() const -> bool;
    [[nodiscard]] auto GetTextRect() const -> IRect;
    [[nodiscard]] auto GetAttackDist() -> uint;
    [[nodiscard]] auto GetItem(uint item_id) -> ItemView*;
    [[nodiscard]] auto GetItemByPid(hash item_pid) -> ItemView*;
    [[nodiscard]] auto GetItemSlot(int slot) -> ItemView*;
    [[nodiscard]] auto GetItemsSlot(int slot) -> vector<ItemView*>;

    [[nodiscard]] auto PopLastHex() -> tuple<ushort, ushort>;

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
    void AddItem(ItemView* item);
    void DeleteItem(ItemView* item, bool animate);
    void DeleteAllItems();
    void Move(uchar dir);
    void TickStart(uint ms);
    void TickNull();
    void ProcessAnim(bool animate_stay, bool is2d, uint anim1, uint anim2, ItemView* item);
    void ResetOk();
    void SetSprRect();
    void ClearAnim();
    void SetOffs(short set_ox, short set_oy, bool move_text);
    void ChangeOffs(short change_ox, short change_oy, bool move_text);
    void AddOffsExt(short ox, short oy);
    void SetText(const char* str, uint color, uint text_delay);
    void DrawTextOnHead();
    void GetNameTextInfo(bool& name_visible, int& x, int& y, int& w, int& h, int& lines);
    void NextAnim(bool erase_front);

    uint ContourColor {};
    uint Flags {};
    RenderEffect* DrawEffect {};
    string AlternateName {};
    string Avatar {};
    vector<ItemView*> InvItems {};
    bool IsRunning {};
    vector<pair<ushort, ushort>> MoveSteps {};
    int CurMoveStep {};
    ModelInstance* Model {};
    bool Visible {true};
    uchar Alpha {};
    IRect DRect {};
    bool SprDrawValid {};
    Sprite* SprDraw {};
    uint SprId {};
    short SprOx {};
    short SprOy {};
    uint FadingTick {};

#define FO_API_CRITTER_VIEW_CLASS
#include "ScriptApi.h"

    PROPERTIES_HEADER();
#define FO_API_CRITTER_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"

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

    CritterViewSettings& _settings;
    GeometryHelper _geomHelper;
    SpriteManager& _sprMngr;
    ResourceManager& _resMngr;
    EffectManager& _effectMngr;
    ClientScriptSystem& _scriptSys;
    GameTimer& _gameTime;
    bool _mapperMode {};
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
    ModelInstance* _modelStay {};
    short _oxExtI {};
    short _oyExtI {};
    float _oxExtF {};
    float _oyExtF {};
    float _oxExtSpeed {};
    float _oyExtSpeed {};
    uint _offsExtNextTick {};
    uint _tickCount {};
    uint _startTick {};
    string _nameOnHead {};
    vector<tuple<ushort, ushort>> _lastHexes {};
    uint _nameColor {};
};
