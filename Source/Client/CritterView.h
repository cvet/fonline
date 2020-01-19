#pragma once

#include "Common.h"

#include "Entity.h"

class ResourceManager;
class SpriteManager;
class Sprite;
struct Effect;
class Animation3d;
struct AnyFrames;
class ItemView;
using ItemViewVec = vector<ItemView*>;

class CritterView : public Entity
{
public:
    // Properties
    PROPERTIES_HEADER();
    // Non sync
    CLASS_PROPERTY(ushort, HexX);
    CLASS_PROPERTY(ushort, HexY);
    CLASS_PROPERTY(uchar, Dir);
    CLASS_PROPERTY(int, Cond);
    CLASS_PROPERTY(uint, Multihex);
    CLASS_PROPERTY(uint, Anim1Life);
    CLASS_PROPERTY(uint, Anim1Knockout);
    CLASS_PROPERTY(uint, Anim1Dead);
    CLASS_PROPERTY(uint, Anim2Life);
    CLASS_PROPERTY(uint, Anim2Knockout);
    CLASS_PROPERTY(uint, Anim2Dead);
    // Core
    CLASS_PROPERTY(hash, ModelName);
    CLASS_PROPERTY(hash, ScriptId);
    CLASS_PROPERTY(uint, LookDistance);
    CLASS_PROPERTY(CScriptArray*, Anim3dLayer);
    CLASS_PROPERTY(hash, DialogId);
    CLASS_PROPERTY(bool, IsNoTalk);
    CLASS_PROPERTY(uint, TalkDistance);
    CLASS_PROPERTY(int, CurrentHp);
    CLASS_PROPERTY(bool, IsNoWalk);
    CLASS_PROPERTY(bool, IsNoRun);
    CLASS_PROPERTY(bool, IsNoRotate);
    CLASS_PROPERTY(uint, WalkTime);
    CLASS_PROPERTY(uint, RunTime);
    CLASS_PROPERTY(int, ScaleFactor);
    CLASS_PROPERTY(uint, TimeoutBattle);
    CLASS_PROPERTY(uint, TimeoutTransfer);
    CLASS_PROPERTY(uint, TimeoutRemoveFromGame);
    CLASS_PROPERTY(bool, IsHide);
    CLASS_PROPERTY(ushort, WorldX);
    CLASS_PROPERTY(ushort, WorldY);
    CLASS_PROPERTY(uint, GlobalMapLeaderId);
    // Exclude
    CLASS_PROPERTY(char, Gender); // GUI
    CLASS_PROPERTY(bool, IsNoFlatten); // Draw order (migrate to proto? to critter type option?)

    // Data
    uint NameColor;
    uint ContourColor;
    UShortVec LastHexX, LastHexY;
    uint Flags;
    Effect* DrawEffect;
    string Name;
    string NameOnHead;
    string Avatar;
    ItemViewVec InvItems;

private:
    SpriteManager& sprMngr;
    ResourceManager& resMngr;

public:
    static bool SlotEnabled[0x100];

    CritterView(uint id, ProtoCritter* proto, SpriteManager& spr_mngr, ResourceManager& res_mngr);
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
    bool IsRunning;
    UShortPairVec MoveSteps;
    int CurMoveStep;
    bool IsNeedMove() { return MoveSteps.size() && !IsWalkAnim(); }
    void Move(int dir);

    // ReSet
private:
    bool needReSet;
    uint reSetTick;

public:
    bool IsNeedReSet();
    void ReSetOk();

    // Time
public:
    uint TickCount;
    uint StartTick;

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
    uint curSpr, lastEndSpr;
    uint animStartTick;
    int anim3dLayers[LAYERS3D_COUNT];

    struct CritterAnim
    {
        AnyFrames* Anim;
        uint AnimTick;
        int BeginFrm;
        int EndFrm;
        bool MoveText;
        int DirOffs;
        uint IndAnim1;
        uint IndAnim2;
        ItemView* ActiveItem;
        CritterAnim() {}
        CritterAnim(AnyFrames* anim, uint tick, int beg_frm, int end_frm, bool move_text, int dir_offs, uint ind_anim1,
            uint ind_anim2, ItemView* item) :
            Anim(anim),
            AnimTick(tick),
            BeginFrm(beg_frm),
            EndFrm(end_frm),
            MoveText(move_text),
            DirOffs(dir_offs),
            IndAnim1(ind_anim1),
            IndAnim2(ind_anim2),
            ActiveItem(item)
        {
        }
    };
    typedef vector<CritterAnim> CritterAnimVec;

    CritterAnimVec animSequence;
    CritterAnim stayAnim;

    CritterAnim* GetCurAnim() { return IsAnim() ? &animSequence[0] : nullptr; }
    void NextAnim(bool erase_front);

public:
    Animation3d* Anim3d;
    Animation3d* Anim3dStay;
    bool Visible;
    uchar Alpha;
    Rect DRect;
    bool SprDrawValid;
    Sprite* SprDraw;
    uint SprId;
    short SprOx, SprOy;
    // Extra offsets
    short OxExtI, OyExtI;
    float OxExtF, OyExtF;
    float OxExtSpeed, OyExtSpeed;
    uint OffsExtNextTick;

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
    int staySprDir;
    uint staySprTick;

    // Finish
private:
    uint finishingTime;

public:
    bool IsFinishing();
    bool IsFinish();

    // Fade
private:
    bool fadingEnable;
    bool fadeUp;

    void SetFade(bool fade_up);
    uchar GetFadeAlpha();

public:
    uint FadingTick;

    // Text
public:
    Rect GetTextRect();
    void SetText(const char* str, uint color, uint text_delay);
    void DrawTextOnHead();
    void GetNameTextInfo(bool& nameVisible, int& x, int& y, int& w, int& h, int& lines);

private:
    Rect textRect;
    uint tickFidget;
    string strTextOnHead;
    uint tickStartText;
    uint tickTextDelay;
    uint textOnHeadColor;
};
