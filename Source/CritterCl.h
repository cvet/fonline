#ifndef __CRITTER_CL__
#define __CRITTER_CL__

#include "NetProtocol.h"
#include "Common.h"
#include "SpriteManager.h"
#include "BufferManager.h"
#include "Item.h"
#include "3dStuff.h"
#include "Entity.h"
#include "CritterData.h"

class CritterCl: public Entity
{
public:
    // Properties
    PROPERTIES_HEADER();
    // Non sync
    CLASS_PROPERTY( ushort, HexX );
    CLASS_PROPERTY( ushort, HexY );
    CLASS_PROPERTY( uchar, Dir );
    CLASS_PROPERTY( uint, CrType );
    CLASS_PROPERTY( uint, CrTypeAlias );
    CLASS_PROPERTY( int, Cond );
    CLASS_PROPERTY( int, MultihexBase );
    CLASS_PROPERTY( uint, Anim1Life );
    CLASS_PROPERTY( uint, Anim1Knockout );
    CLASS_PROPERTY( uint, Anim1Dead );
    CLASS_PROPERTY( uint, Anim2Life );
    CLASS_PROPERTY( uint, Anim2Knockout );
    CLASS_PROPERTY( uint, Anim2Dead );
    CLASS_PROPERTY( uint, Anim2KnockoutEnd );
    // Core
    CLASS_PROPERTY( hash, ScriptId );
    CLASS_PROPERTY( uint, LookDistance );
    CLASS_PROPERTY( ScriptArray *, Anim3dLayer );
    CLASS_PROPERTY( hash, DialogId );
    CLASS_PROPERTY( uint, FollowCrit );
    CLASS_PROPERTY( bool, IsNoTalk );
    CLASS_PROPERTY( bool, IsNoBarter );
    CLASS_PROPERTY( uint, TalkDistance );
    CLASS_PROPERTY( int, CurrentHp );
    CLASS_PROPERTY( int, CurrentAp );
    CLASS_PROPERTY( int, ActionPoints );
    CLASS_PROPERTY( int, MoveAp );
    CLASS_PROPERTY( int, MaxMoveAp );
    CLASS_PROPERTY( bool, IsNoWalk );
    CLASS_PROPERTY( bool, IsNoRun );
    CLASS_PROPERTY( int, WalkTime );
    CLASS_PROPERTY( int, RunTime );
    CLASS_PROPERTY( int, ScaleFactor );
    CLASS_PROPERTY( uint, TimeoutBattle );
    CLASS_PROPERTY( uint, TimeoutTransfer );
    CLASS_PROPERTY( uint, TimeoutRemoveFromGame );
    CLASS_PROPERTY( bool, IsNoLoot );
    CLASS_PROPERTY( bool, IsHide );
    CLASS_PROPERTY( bool, IsEndCombat );
    CLASS_PROPERTY( hash, HandsItemProtoId );
    CLASS_PROPERTY( uchar, HandsItemMode );
    // Exclude
    CLASS_PROPERTY( uint, BaseCrType );         // Mapper character base type
    CLASS_PROPERTY( int, Experience );          // Craft
    CLASS_PROPERTY( uint, TimeoutSkScience );   // Craft
    CLASS_PROPERTY( uint, TimeoutSkRepair );    // Craft
    CLASS_PROPERTY( int, ReplicationMoney );    // GUI
    CLASS_PROPERTY( int, ReplicationCost );     // GUI
    CLASS_PROPERTY( int, ReplicationCount );    // GUI
    CLASS_PROPERTY( char, Gender );             // GUI
    CLASS_PROPERTY( bool, IsNoPush );           // GUI
    CLASS_PROPERTY( bool, IsNoAim );            // GUI
    CLASS_PROPERTY( int, KarmaVoting );         // Migrate karma voting to scripts
    CLASS_PROPERTY( uint, TimeoutKarmaVoting ); // Migrate karma voting to scripts
    CLASS_PROPERTY( int, CarryWeight );         // Overweight checking
    CLASS_PROPERTY( bool, IsUnlimitedAmmo );    // Play shoot sound
    CLASS_PROPERTY( bool, IsNoFlatten );        // Draw order (migrate to proto? to critter type option?)
    CLASS_PROPERTY( bool, IsDamagedEye );
    CLASS_PROPERTY( bool, IsDamagedRightArm );
    CLASS_PROPERTY( bool, IsDamagedLeftArm );
    CLASS_PROPERTY( bool, IsDamagedRightLeg );
    CLASS_PROPERTY( bool, IsDamagedLeftLeg );
    CLASS_PROPERTY( uchar, PerkQuickPockets );
    CLASS_PROPERTY( uchar, PerkMasterTrader );
    CLASS_PROPERTY( uchar, PerkSilentRunning );

    // Data
    uint          NameColor;
    uint          ContourColor;
    UShortVec     LastHexX, LastHexY;
    uint          Flags;
    uint          ApRegenerationTick;
    Effect*       DrawEffect;

    ScriptString* Name;
    ScriptString* NameOnHead;
    ScriptString* Avatar;

    ItemVec       InvItems;
    Item*         DefItemSlotHand;
    Item*         DefItemSlotArmor;
    Item*         ItemSlotMain;
    Item*         ItemSlotExt;
    Item*         ItemSlotArmor;

    static bool   SlotEnabled[ 0x100 ];
    static IntSet RegProperties;

    CritterCl( uint id, ProtoCritter* proto );
    ~CritterCl();
    void Init();
    void Finish();

    uint        GetId()   { return Id; }
    const char* GetInfo() { return Name->c_str(); }
    bool        IsLastHexes();
    void        FixLastHexes();
    ushort      PopLastHexX();
    ushort      PopLastHexY();
    void        ChangeCrType( uint type );
    void        ChangeDir( uchar dir, bool animate = true );

    void Animate( uint anim1, uint anim2, Item* item );
    void AnimateStay();
    void Action( int action, int action_ext, Item* item, bool local_call = true );
    void Process();
    void DrawStay( Rect r );

    const char* GetName()    { return Name->c_str(); }
    bool        IsNpc()      { return FLAG( Flags, FCRIT_NPC ); }
    bool        IsPlayer()   { return FLAG( Flags, FCRIT_PLAYER ); }
    bool        IsChosen()   { return FLAG( Flags, FCRIT_CHOSEN ); }
    bool        IsGmapRule() { return FLAG( Flags, FCRIT_RULEGROUP ); }
    bool        IsOnline()   { return !FLAG( Flags, FCRIT_DISCONNECT ); }
    bool        IsOffline()  { return FLAG( Flags, FCRIT_DISCONNECT ); }
    bool        IsLife()     { return GetCond() == COND_LIFE; }
    bool        IsKnockout() { return GetCond() == COND_KNOCKOUT; }
    bool        IsDead()     { return GetCond() == COND_DEAD; }
    bool        IsCanTalk();
    bool        IsCombatMode();
    bool        IsTurnBased();
    bool        CheckFind( int find_type );

    uint GetAttackDist();
    uint GetUseDist();
    uint GetMultihex();
    uint GetMaxWeightKg();
    uint GetMaxVolume();
    bool IsDmgLeg();
    bool IsDmgTwoLeg();
    bool IsDmgArm();
    bool IsDmgTwoArm();
    int  GetRealAp();
    int  GetAllAp();
    void SubAp( int val );
    void SubMoveAp( int val );

    // Items
public:
    void  AddItem( Item* item );
    void  DeleteItem( Item* item, bool animate );
    void  DeleteAllItems();
    Item* GetItem( uint item_id );
    Item* GetItemByPid( hash item_pid );
    Item* GetItemByPidInvPriority( hash item_pid );
    Item* GetItemByPidSlot( hash item_pid, int slot );
    Item* GetAmmo( uint caliber );
    Item* GetItemSlot( int slot );
    void  GetItemsSlot( int slot, ItemVec& items );
    void  GetItemsType( int slot, ItemVec& items );
    uint  CountItemPid( hash item_pid );
    uint  CountItemType( uchar type );
    bool  IsCanSortItems();
    Item* GetItemHighSortValue();
    Item* GetItemLowSortValue();
    void  GetInvItems( ItemVec& items );
    uint  GetItemsCount();
    uint  GetItemsCountInv();
    uint  GetItemsWeight();
    uint  GetItemsWeightKg();
    uint  GetItemsVolume();
    int   GetFreeWeight();
    int   GetFreeVolume();
    bool  IsHaveLightSources();
    Item* GetSlotUse( uchar num_slot, uchar& use );
    bool  IsItemAim( uchar num_slot );
    uchar GetUse()      { return ItemSlotMain->GetMode() & 0xF; }
    uchar GetFullRate() { return ItemSlotMain->GetMode(); }
    void  NextRateItem( bool prev );
    uchar GetAim() { return ( ItemSlotMain->GetMode() >> 4 ) & 0xF; }
    bool  IsAim()  { return GetAim() > 0; }
    void  SetAim( uchar hit_location );
    uint  GetUseApCost( Item* item, uchar rate );
    Item* GetAmmoAvialble( Item* weap );
    bool  IsOverweight()       { return (int) GetItemsWeight() > GetCarryWeight(); }
    bool  IsDoubleOverweight() { return (int) GetItemsWeight() > GetCarryWeight() * 2; }

    // Moving
public:
    bool          IsRunning;
    UShortPairVec MoveSteps;
    int           CurMoveStep;
    bool IsNeedMove() { return MoveSteps.size() && !IsWalkAnim(); }
    void ZeroSteps()
    {
        MoveSteps.clear();
        CurMoveStep = 0;
    }
    void Move( int dir );

    // ReSet
private:
    bool needReSet;
    uint reSetTick;

public:
    bool IsNeedReSet() { return needReSet && Timer::GameTick() >= reSetTick; }
    void ReSetOk()     { needReSet = false; }

    // Time
public:
    uint TickCount;
    uint StartTick;

    void TickStart( uint ms );
    void TickNull();
    bool IsFree();

    // Animation
public:
    uint GetAnim1( Item* anim_item = nullptr );
    uint GetAnim2();
    void ProcessAnim( bool animate_stay, bool is2d, uint anim1, uint anim2, Item* item );
    int* GetLayers3dData();
    bool IsAnimAviable( uint anim1, uint anim2 );

private:
    uint curSpr, lastEndSpr;
    uint animStartTick;
    int  anim3dLayers[ LAYERS3D_COUNT ];

    struct CritterAnim
    {
        AnyFrames* Anim;
        uint       AnimTick;
        int        BeginFrm;
        int        EndFrm;
        bool       MoveText;
        int        DirOffs;
        uint       IndCrType, IndAnim1, IndAnim2;
        Item*      ActiveItem;
        CritterAnim() {}
        CritterAnim( AnyFrames* anim, uint tick, int beg_frm, int end_frm, bool move_text, int dir_offs, uint ind_crtype, uint ind_anim1, uint ind_anim2, Item* item ): Anim( anim ), AnimTick( tick ), BeginFrm( beg_frm ), EndFrm( end_frm ), MoveText( move_text ), DirOffs( dir_offs ),
                                                                                                                                                                        IndCrType( ind_crtype ), IndAnim1( ind_anim1 ), IndAnim2( ind_anim2 ), ActiveItem( item ) {}
    };
    typedef vector< CritterAnim > CritterAnimVec;

    CritterAnimVec animSequence;
    CritterAnim    stayAnim;

    CritterAnim* GetCurAnim() { return IsAnim() ? &animSequence[ 0 ] : nullptr; }
    void         NextAnim( bool erase_front );

public:
    Animation3d* Anim3d;
    Animation3d* Anim3dStay;
    bool         Visible;
    uchar        Alpha;
    Rect         DRect;
    bool         SprDrawValid;
    Sprite*      SprDraw;
    uint         SprId;
    short        SprOx, SprOy;
    // Extra offsets
    short        OxExtI, OyExtI;
    float        OxExtF, OyExtF;
    float        OxExtSpeed, OyExtSpeed;
    uint         OffsExtNextTick;

    void         SetSprRect();
    bool         Is3dAnim()  { return Anim3d != nullptr; }
    Animation3d* GetAnim3d() { return Anim3d; }
    bool         IsAnim()    { return animSequence.size() > 0; }
    bool         IsWalkAnim();
    void         ClearAnim();

    void SetOffs( short set_ox, short set_oy, bool move_text );
    void ChangeOffs( short change_ox, short change_oy, bool move_text );
    void AddOffsExt( short ox, short oy );
    void GetWalkHexOffsets( int dir, int& ox, int& oy );

    // Stay sprite
private:
    int  staySprDir;
    uint staySprTick;

    // Finish
private:
    uint finishingTime;

public:
    bool IsFinishing() { return finishingTime != 0; }
    bool IsFinish()    { return ( finishingTime && Timer::GameTick() > finishingTime ); }

    // Fade
private:
    bool fadingEnable;
    bool fadeUp;

    void  SetFade( bool fade_up );
    uchar GetFadeAlpha();

public:
    uint FadingTick;

    // Text
public:
    Rect GetTextRect();
    void SetText( const char* str, uint color, uint text_delay );
    void DrawTextOnHead();
    void GetNameTextInfo( bool& nameVisible, int& x, int& y, int& w, int& h, int& lines );

private:
    Rect   textRect;
    uint   tickFidget;
    string strTextOnHead;
    uint   tickStartText;
    uint   tickTextDelay;
    uint   textOnHeadColor;

    // Ap cost
public:
    int GetApCostCritterMove( bool is_run );
    int GetApCostMoveItemContainer();
    int GetApCostMoveItemInventory();
    int GetApCostPickItem();
    int GetApCostDropItem();
    int GetApCostPickCritter();
    int GetApCostUseSkill();
};

typedef map< uint, CritterCl* > CritMap;
typedef vector< CritterCl* >    CritVec;
typedef CritterCl*              CritterClPtr;

#endif // __CRITTER_CL__
