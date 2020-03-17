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
#include "Properties.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "Settings.h"
#include "SoundManager.h"
#include "SpriteManager.h"

#define FO_API_CRITTER_VIEW_HEADER
#include "ScriptApi.h"

class ItemView;
using ItemViewVec = vector<ItemView*>;
class CritterView;
using CritterViewMap = map<uint, CritterView*>;
using CritterViewVec = vector<CritterView*>;

class CritterView : public Entity
{
public:
    CritterView(uint id, ProtoCritter* proto, CritterViewSettings& sett, SpriteManager& spr_mngr,
        ResourceManager& res_mngr, EffectManager& effect_mngr, ClientScriptSystem& script_sys, bool mapper_mode);
    ~CritterView();
    void Init();
    void Finish();

    bool IsLastHexes();
    void FixLastHexes();
    ushort PopLastHexX();
    ushort PopLastHexY();
    void RefreshAnim();
    void ChangeDir(uchar dir, bool animate = true);

    void Animate(uint anim1, uint anim2, ItemView* item);
    void AnimateStay();
    void Action(int action, int action_ext, ItemView* item, bool local_call = true);
    void Process();
    void DrawStay(Rect r);

    string GetName() { return Name; }
    bool IsNpc() { return FLAG(Flags, FCRIT_NPC); }
    bool IsPlayer() { return FLAG(Flags, FCRIT_PLAYER); }
    bool IsChosen() { return FLAG(Flags, FCRIT_CHOSEN); }
    bool IsOnline() { return !FLAG(Flags, FCRIT_DISCONNECT); }
    bool IsOffline() { return FLAG(Flags, FCRIT_DISCONNECT); }
    bool IsLife() { return GetCond() == COND_LIFE; }
    bool IsKnockout() { return GetCond() == COND_KNOCKOUT; }
    bool IsDead() { return GetCond() == COND_DEAD; }
    bool IsCombatMode();
    bool CheckFind(int find_type);

    uint GetAttackDist();

    static bool SlotEnabled[0x100];

    uint NameColor {};
    uint ContourColor {};
    UShortVec LastHexX {};
    UShortVec LastHexY {};
    uint Flags {};
    RenderEffect* DrawEffect {};
    string Name {};
    string NameOnHead {};
    string Avatar {};
    ItemViewVec InvItems {};

private:
    CritterViewSettings& settings;
    GeometryHelper geomHelper;
    SpriteManager& sprMngr;
    ResourceManager& resMngr;
    EffectManager& effectMngr;
    ClientScriptSystem& scriptSys;
    bool mapperMode {};

    // Items
public:
    void AddItem(ItemView* item);
    void DeleteItem(ItemView* item, bool animate);
    void DeleteAllItems();
    ItemView* GetItem(uint item_id);
    ItemView* GetItemByPid(hash item_pid);
    ItemView* GetItemSlot(int slot);
    void GetItemsSlot(int slot, ItemViewVec& items);
    uint CountItemPid(hash item_pid);
    bool IsHaveLightSources();

    // Moving
public:
    bool IsRunning {};
    UShortPairVec MoveSteps {};
    int CurMoveStep {};

    bool IsNeedMove() { return MoveSteps.size() && !IsWalkAnim(); }
    void Move(int dir);

    // ReSet
private:
    bool needReSet {};
    uint reSetTick {};

public:
    bool IsNeedReSet();
    void ReSetOk();

    // Time
public:
    uint TickCount {};
    uint StartTick {};

    void TickStart(uint ms);
    void TickNull();
    bool IsFree();

    // Animation
public:
    uint GetAnim1();
    uint GetAnim2();
    void ProcessAnim(bool animate_stay, bool is2d, uint anim1, uint anim2, ItemView* item);
    int* GetLayers3dData();
    bool IsAnimAviable(uint anim1, uint anim2);

private:
    uint curSpr {};
    uint lastEndSpr {};
    uint animStartTick {};
    int anim3dLayers[LAYERS3D_COUNT] {};

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

    CritterAnim stayAnim {};
    vector<CritterAnim> animSequence {};

    CritterAnim* GetCurAnim() { return IsAnim() ? &animSequence[0] : nullptr; }
    void NextAnim(bool erase_front);

public:
    Animation3d* Anim3d {};
    Animation3d* Anim3dStay {};
    bool Visible {true};
    uchar Alpha {};
    Rect DRect {};
    bool SprDrawValid {};
    Sprite* SprDraw {};
    uint SprId {};
    short SprOx {};
    short SprOy {};
    short OxExtI {};
    short OyExtI {};
    float OxExtF {};
    float OyExtF {};
    float OxExtSpeed {};
    float OyExtSpeed {};
    uint OffsExtNextTick {};

    void SetSprRect();
    bool Is3dAnim() { return Anim3d != nullptr; }
    Animation3d* GetAnim3d() { return Anim3d; }
    bool IsAnim() { return animSequence.size() > 0; }
    bool IsWalkAnim();
    void ClearAnim();

    void SetOffs(short set_ox, short set_oy, bool move_text);
    void ChangeOffs(short change_ox, short change_oy, bool move_text);
    void AddOffsExt(short ox, short oy);
    void GetWalkHexOffsets(int dir, int& ox, int& oy);

    // Stay sprite
private:
    int staySprDir {};
    uint staySprTick {};

    // Finish
private:
    uint finishingTime {};

public:
    bool IsFinishing();
    bool IsFinish();

    // Fade
private:
    bool fadingEnable {};
    bool fadeUp {};

    void SetFade(bool fade_up);
    uchar GetFadeAlpha();

public:
    uint FadingTick {};

    // Text
public:
    Rect GetTextRect();
    void SetText(const char* str, uint color, uint text_delay);
    void DrawTextOnHead();
    void GetNameTextInfo(bool& nameVisible, int& x, int& y, int& w, int& h, int& lines);

private:
    Rect textRect {};
    uint tickFidget {};
    string strTextOnHead {};
    uint tickStartText {};
    uint tickTextDelay {};
    uint textOnHeadColor {COLOR_CRITTER_NAME};

public:
#define FO_API_CRITTER_VIEW_CLASS
#include "ScriptApi.h"

    PROPERTIES_HEADER();
#define FO_API_CRITTER_PROPERTY CLASS_PROPERTY
#include "ScriptApi.h"
};
