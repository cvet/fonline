#include "Common.h"
#include "Critter.h"
#include "Script.h"
#include "MapManager.h"
#include "ItemManager.h"
#include "CritterType.h"
#include "Access.h"
#include "CritterManager.h"
#include "ProtoManager.h"

ProtoCritter::ProtoCritter( hash pid ): ProtoEntity( pid, Critter::PropertiesRegistrator ) {}

const char* CritterEventFuncName[ CRITTER_EVENT_MAX ] =
{
    "void %s(Critter&)",                                                          // CRITTER_EVENT_IDLE
    "void %s(Critter&,bool)",                                                     // CRITTER_EVENT_FINISH
    "void %s(Critter&,Critter@)",                                                 // CRITTER_EVENT_DEAD
    "void %s(Critter&)",                                                          // CRITTER_EVENT_RESPAWN
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_SHOW_CRITTER
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_SHOW_CRITTER_1
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_SHOW_CRITTER_2
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_SHOW_CRITTER_3
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_HIDE_CRITTER
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_HIDE_CRITTER_1
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_HIDE_CRITTER_2
    "void %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_HIDE_CRITTER_3
    "void %s(Critter&,Item&,bool,Critter@)",                                      // CRITTER_EVENT_SHOW_ITEM_ON_MAP
    "void %s(Critter&,Item&)",                                                    // CRITTER_EVENT_CHANGE_ITEM_ON_MAP
    "void %s(Critter&,Item&,bool,Critter@)",                                      // CRITTER_EVENT_HIDE_ITEM_ON_MAP
    "bool %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_ATTACK
    "bool %s(Critter&,Critter&)",                                                 // CRITTER_EVENT_ATTACKED
    "void %s(Critter&,Critter&,bool,Item&,uint)",                                 // CRITTER_EVENT_STEALING
    "void %s(Critter&,Critter&,int,int)",                                         // CRITTER_EVENT_MESSAGE
    "bool %s(Critter&,Item&,Critter@,Item@,Item@)",                               // CRITTER_EVENT_USE_ITEM
    "bool %s(Critter&,Critter&,Item&)",                                           // CRITTER_EVENT_USE_ITEM_ON_ME
    "bool %s(Critter&,CritterProperty,Critter@,Item@,Item@)",                     // CRITTER_EVENT_USE_SKILL
    "bool %s(Critter&,Critter&,CritterProperty)",                                 // CRITTER_EVENT_USE_SKILL_ON_ME
    "void %s(Critter&,Item&)",                                                    // CRITTER_EVENT_DROP_ITEM
    "void %s(Critter&,Item&,uint8)",                                              // CRITTER_EVENT_MOVE_ITEM
    "void %s(Critter&,uint,uint,uint,uint,uint)",                                 // CRITTER_EVENT_KNOCKOUT
    "void %s(Critter&,Critter&,Critter@)",                                        // CRITTER_EVENT_SMTH_DEAD
    "void %s(Critter&,Critter&,Critter&,bool,Item&,uint)",                        // CRITTER_EVENT_SMTH_STEALING
    "void %s(Critter&,Critter&,Critter&)",                                        // CRITTER_EVENT_SMTH_ATTACK
    "void %s(Critter&,Critter&,Critter&)",                                        // CRITTER_EVENT_SMTH_ATTACKED
    "void %s(Critter&,Critter&,Item&,Critter@,Item@,Item@)",                      // CRITTER_EVENT_SMTH_USE_ITEM
    "void %s(Critter&,Critter&,CritterProperty,Critter@,Item@,Item@)",            // CRITTER_EVENT_SMTH_USE_SKILL
    "void %s(Critter&,Critter&,Item&)",                                           // CRITTER_EVENT_SMTH_DROP_ITEM
    "void %s(Critter&,Critter&,Item&,uint8)",                                     // CRITTER_EVENT_SMTH_MOVE_ITEM
    "void %s(Critter&,Critter&,uint,uint,uint,uint,uint)",                        // CRITTER_EVENT_SMTH_KNOCKOUT
    "int %s(Critter&,NpcPlane&,int,Critter@,Item@)",                              // CRITTER_EVENT_PLANE_BEGIN
    "int %s(Critter&,NpcPlane&,int,Critter@,Item@)",                              // CRITTER_EVENT_PLANE_END
    "int %s(Critter&,NpcPlane&,int,uint&,uint&,uint&)",                           // CRITTER_EVENT_PLANE_RUN
    "bool %s(Critter&,Critter&,bool,uint)",                                       // CRITTER_EVENT_BARTER
    "bool %s(Critter&,Critter&,bool,uint)",                                       // CRITTER_EVENT_TALK
    "bool %s(Critter&,int,Item@,float&,float&,float&,float&,float&,uint&,bool&)", // CRITTER_EVENT_GLOBAL_PROCESS
    "bool %s(Critter&,Item@,uint,int,uint&,uint16&,uint16&,uint8&)",              // CRITTER_EVENT_GLOBAL_INVITE
    "void %s(Critter&,Map&,bool)",                                                // CRITTER_EVENT_TURN_BASED_PROCESS
    "void %s(Critter&,Critter&,Map&,bool)",                                       // CRITTER_EVENT_SMTH_TURN_BASED_PROCESS
};

/************************************************************************/
/*                                                                      */
/************************************************************************/

bool   Critter::SlotEnabled[ 0x100 ];
bool   Critter::SlotDataSendEnabled[ 0x100 ];
IntSet Critter::RegProperties;

// Properties
PROPERTIES_IMPL( Critter );
CLASS_PROPERTY_IMPL( Critter, MapId );
CLASS_PROPERTY_IMPL( Critter, MapPid );
CLASS_PROPERTY_IMPL( Critter, HexX );
CLASS_PROPERTY_IMPL( Critter, HexY );
CLASS_PROPERTY_IMPL( Critter, Dir );
CLASS_PROPERTY_IMPL( Critter, PassHash );
CLASS_PROPERTY_IMPL( Critter, CrType );
CLASS_PROPERTY_IMPL( Critter, Cond );
CLASS_PROPERTY_IMPL( Critter, ClientToDelete );
CLASS_PROPERTY_IMPL( Critter, MultihexBase );
CLASS_PROPERTY_IMPL( Critter, WorldX );
CLASS_PROPERTY_IMPL( Critter, WorldY );
CLASS_PROPERTY_IMPL( Critter, GlobalGroupRuleId );
CLASS_PROPERTY_IMPL( Critter, GlobalGroupUid );
CLASS_PROPERTY_IMPL( Critter, LastMapHexX );
CLASS_PROPERTY_IMPL( Critter, LastMapHexY );
CLASS_PROPERTY_IMPL( Critter, Anim1Life );
CLASS_PROPERTY_IMPL( Critter, Anim1Knockout );
CLASS_PROPERTY_IMPL( Critter, Anim1Dead );
CLASS_PROPERTY_IMPL( Critter, Anim2Life );
CLASS_PROPERTY_IMPL( Critter, Anim2Knockout );
CLASS_PROPERTY_IMPL( Critter, Anim2Dead );
CLASS_PROPERTY_IMPL( Critter, Anim2KnockoutEnd );
CLASS_PROPERTY_IMPL( Critter, GlobalMapFog );
CLASS_PROPERTY_IMPL( Critter, TE_FuncNum );
CLASS_PROPERTY_IMPL( Critter, TE_Rate );
CLASS_PROPERTY_IMPL( Critter, TE_NextTime );
CLASS_PROPERTY_IMPL( Critter, TE_Identifier );
CLASS_PROPERTY_IMPL( Critter, LookDistance );
CLASS_PROPERTY_IMPL( Critter, Charisma );
CLASS_PROPERTY_IMPL( Critter, Experience );
CLASS_PROPERTY_IMPL( Critter, DialogId );
CLASS_PROPERTY_IMPL( Critter, BagId );
CLASS_PROPERTY_IMPL( Critter, NpcRole );
CLASS_PROPERTY_IMPL( Critter, TeamId );
CLASS_PROPERTY_IMPL( Critter, FollowCrit );
CLASS_PROPERTY_IMPL( Critter, FreeBarterPlayer );
CLASS_PROPERTY_IMPL( Critter, LastWeaponId );
CLASS_PROPERTY_IMPL( Critter, HandsItemProtoId );
CLASS_PROPERTY_IMPL( Critter, HandsItemMode );
CLASS_PROPERTY_IMPL( Critter, KarmaVoting );
CLASS_PROPERTY_IMPL( Critter, MaxTalkers );
CLASS_PROPERTY_IMPL( Critter, TalkDistance );
CLASS_PROPERTY_IMPL( Critter, CarryWeight );
CLASS_PROPERTY_IMPL( Critter, CurrentHp );
CLASS_PROPERTY_IMPL( Critter, ActionPoints );
CLASS_PROPERTY_IMPL( Critter, CurrentAp );
CLASS_PROPERTY_IMPL( Critter, ApRegenerationTime );
CLASS_PROPERTY_IMPL( Critter, MaxMoveAp );
CLASS_PROPERTY_IMPL( Critter, MoveAp );
CLASS_PROPERTY_IMPL( Critter, TurnBasedAc );
CLASS_PROPERTY_IMPL( Critter, ReplicationTime );
CLASS_PROPERTY_IMPL( Critter, WalkTime );
CLASS_PROPERTY_IMPL( Critter, RunTime );
CLASS_PROPERTY_IMPL( Critter, ScaleFactor );
CLASS_PROPERTY_IMPL( Critter, SneakCoefficient );
CLASS_PROPERTY_IMPL( Critter, BarterCoefficient );
CLASS_PROPERTY_IMPL( Critter, TimeoutBattle );
CLASS_PROPERTY_IMPL( Critter, TimeoutTransfer );
CLASS_PROPERTY_IMPL( Critter, TimeoutRemoveFromGame );
CLASS_PROPERTY_IMPL( Critter, TimeoutKarmaVoting );
CLASS_PROPERTY_IMPL( Critter, TimeoutSkScience );
CLASS_PROPERTY_IMPL( Critter, TimeoutSkRepair );
CLASS_PROPERTY_IMPL( Critter, DefaultCombat );
CLASS_PROPERTY_IMPL( Critter, IsUnlimitedAmmo );
CLASS_PROPERTY_IMPL( Critter, IsNoUnarmed );
CLASS_PROPERTY_IMPL( Critter, IsNoFavoriteItem );
CLASS_PROPERTY_IMPL( Critter, IsNoPush );
CLASS_PROPERTY_IMPL( Critter, IsNoItemGarbager );
CLASS_PROPERTY_IMPL( Critter, IsNoEnemyStack );
CLASS_PROPERTY_IMPL( Critter, IsGeck );
CLASS_PROPERTY_IMPL( Critter, IsNoLoot );
CLASS_PROPERTY_IMPL( Critter, IsNoSteal );
CLASS_PROPERTY_IMPL( Critter, IsNoHome );
CLASS_PROPERTY_IMPL( Critter, IsNoWalk );
CLASS_PROPERTY_IMPL( Critter, IsNoRun );
CLASS_PROPERTY_IMPL( Critter, IsNoTalk );
CLASS_PROPERTY_IMPL( Critter, IsHide );
CLASS_PROPERTY_IMPL( Critter, IsNoFlatten );
CLASS_PROPERTY_IMPL( Critter, IsNoAim );
CLASS_PROPERTY_IMPL( Critter, IsNoBarter );
CLASS_PROPERTY_IMPL( Critter, IsEndCombat );
CLASS_PROPERTY_IMPL( Critter, IsDamagedEye );
CLASS_PROPERTY_IMPL( Critter, IsDamagedRightArm );
CLASS_PROPERTY_IMPL( Critter, IsDamagedLeftArm );
CLASS_PROPERTY_IMPL( Critter, IsDamagedRightLeg );
CLASS_PROPERTY_IMPL( Critter, IsDamagedLeftLeg );
CLASS_PROPERTY_IMPL( Critter, PerkQuickPockets );
CLASS_PROPERTY_IMPL( Critter, PerkMasterTrader );
CLASS_PROPERTY_IMPL( Critter, PerkSilentRunning );
CLASS_PROPERTY_IMPL( Critter, KnownLocations );
CLASS_PROPERTY_IMPL( Critter, ConnectionIp );
CLASS_PROPERTY_IMPL( Critter, ConnectionPort );
CLASS_PROPERTY_IMPL( Critter, HoloInfo );
CLASS_PROPERTY_IMPL( Critter, HomeMapId );
CLASS_PROPERTY_IMPL( Critter, HomeHexX );
CLASS_PROPERTY_IMPL( Critter, HomeHexY );
CLASS_PROPERTY_IMPL( Critter, HomeDir );
CLASS_PROPERTY_IMPL( Critter, ShowCritterDist1 );
CLASS_PROPERTY_IMPL( Critter, ShowCritterDist2 );
CLASS_PROPERTY_IMPL( Critter, ShowCritterDist3 );
CLASS_PROPERTY_IMPL( Critter, ScriptId );
CLASS_PROPERTY_IMPL( Critter, EnemyStack );
CLASS_PROPERTY_IMPL( Critter, InternalBagItemPid );
CLASS_PROPERTY_IMPL( Critter, InternalBagItemCount );
CLASS_PROPERTY_IMPL( Critter, ExternalBagCurrentSet );
CLASS_PROPERTY_IMPL( Critter, FavoriteItemPid );

Critter::Critter( uint id, EntityType type, ProtoCritter* proto ): Entity( id, type, PropertiesRegistrator, proto )
{
    CritterIsNpc = false;
    GroupMove = nullptr;
    PrevHexTick = 0;
    PrevHexX = PrevHexY = 0;
    startBreakTime = 0;
    breakTime = 0;
    waitEndTick = 0;
    KnockoutAp = 0;
    CacheValuesNextTick = 0;
    Flags = 0;
    AccessContainerId = 0;
    ItemTransferCount = 0;
    TryingGoHomeTick = 0;
    ApRegenerationTick = 0;
    GlobalIdleNextTick = 0;
    LockMapTransfers = 0;
    ViewMapId = 0;
    ViewMapPid = 0;
    ViewMapLook = 0;
    ViewMapHx = ViewMapHy = 0;
    ViewMapDir = 0;
    DisableSend = 0;
    CanBeRemoved = false;
    NameStr = ScriptString::Create();
    memzero( FuncId, sizeof( FuncId ) );
    GroupSelf = new GlobalMapGroup();
    GroupSelf->CurX = (float) GetWorldX();
    GroupSelf->CurY = (float) GetWorldY();
    GroupSelf->ToX = GroupSelf->CurX;
    GroupSelf->ToY = GroupSelf->CurY;
    GroupSelf->Speed = 0.0f;
    GroupSelf->Rule = this;
    ItemSlotMain = ItemSlotExt = defItemSlotHand = new Item( 0, ProtoMngr.GetProtoItem( ITEM_DEF_SLOT ) );
    ItemSlotArmor = defItemSlotArmor = new Item( 0, ProtoMngr.GetProtoItem( ITEM_DEF_ARMOR ) );
    defItemSlotHand->SetAccessory( ITEM_ACCESSORY_CRITTER );
    defItemSlotArmor->SetAccessory( ITEM_ACCESSORY_CRITTER );
    defItemSlotHand->SetCritId( id );
    defItemSlotArmor->SetCritId( id );
    defItemSlotHand->SetCritSlot( SLOT_HAND1 );
    defItemSlotArmor->SetCritSlot( SLOT_ARMOR );
}

Critter::~Critter()
{
    SAFEDEL( GroupSelf );
    SAFEREL( defItemSlotHand );
    SAFEREL( defItemSlotArmor );
}

void Critter::SetBreakTime( uint ms )
{
    breakTime = ms;
    startBreakTime = Timer::GameTick();
    ApRegenerationTick = 0;
}

void Critter::SetBreakTimeDelta( uint ms )
{
    uint dt = ( Timer::GameTick() - startBreakTime );
    if( dt > breakTime )
        dt -= breakTime;
    else
        dt = 0;
    if( dt > ms )
        dt = 0;
    SetBreakTime( ms - dt );
}

void Critter::DeleteInventory()
{
    while( !invItems.empty() )
        ItemMngr.DeleteItem( *invItems.begin() );
}

uint Critter::GetUseApCost( Item* weap, int use )
{
    if( Script::PrepareContext( ServerFunctions.GetUseApCost, _FUNC_, GetInfo() ) )
    {
        Script::SetArgEntityOK( this );
        Script::SetArgEntityOK( weap );
        Script::SetArgUChar( use );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return 1;
}

uint Critter::GetAttackDist( Item* weap, int use )
{
    if( Script::PrepareContext( ServerFunctions.GetAttackDistantion, _FUNC_, GetInfo() ) )
    {
        Script::SetArgEntityOK( this );
        Script::SetArgEntityOK( weap );
        Script::SetArgUChar( use );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return 0;
}

uint Critter::GetUseDist()
{
    return 1 + GetMultihex();
}

uint Critter::GetMultihex()
{
    uint mh = GetMultihexBase();
    if( mh == 0 )
        mh = CritType::GetMultihex( GetCrType() );
    return CLAMP( mh, 0, MAX_HEX_OFFSET );
}

bool Critter::IsLife()
{
    return GetCond() == COND_LIFE;
}

bool Critter::IsDead()
{
    return GetCond() == COND_DEAD;
}

bool Critter::IsKnockout()
{
    return GetCond() == COND_KNOCKOUT;
}

bool Critter::CheckFind( int find_type )
{
    if( IsNpc() )
    {
        if( FLAG( find_type, FIND_ONLY_PLAYERS ) )
            return false;
    }
    else
    {
        if( FLAG( find_type, FIND_ONLY_NPC ) )
            return false;
    }
    return ( FLAG( find_type, FIND_LIFE ) && IsLife() ) ||
           ( FLAG( find_type, FIND_KO ) && IsKnockout() ) ||
           ( FLAG( find_type, FIND_DEAD ) && IsDead() );
}

int Critter::GetRealAp()
{
    return GetCurrentAp();
}

int Critter::GetAllAp()
{
    return GetCurrentAp() / AP_DIVIDER + GetMoveAp();
}

void Critter::SubAp( int val )
{
    SetCurrentAp( GetCurrentAp() - val * AP_DIVIDER );
    ApRegenerationTick = 0;
}

void Critter::SubMoveAp( int val )
{
    SetMoveAp( GetMoveAp() - val );
}

bool Critter::IsDmgLeg()
{
    return GetIsDamagedRightLeg() || GetIsDamagedLeftLeg();
}

bool Critter::IsDmgTwoLeg()
{
    return GetIsDamagedRightLeg() && GetIsDamagedLeftLeg();
}

bool Critter::IsDmgArm()
{
    return GetIsDamagedRightArm() || GetIsDamagedLeftArm();
}

bool Critter::IsDmgTwoArm()
{
    return GetIsDamagedRightArm() && GetIsDamagedLeftArm();
}

hash Critter::GetFavoriteItemPid( uchar slot )
{
    ScriptArray* pids = GetFavoriteItemPid();
    hash         result = 0;
    if( slot < pids->GetSize() )
        result = *(hash*) pids->At( slot );
    SAFEREL( pids );
    return result;
}

void Critter::SetFavoriteItemPid( uchar slot, hash pid )
{
    ScriptArray* pids = GetFavoriteItemPid();
    if( slot >= pids->GetSize() )
        pids->Resize( slot + 1 );
    if( *(hash*) pids->At( slot ) != pid )
    {
        pids->SetValue( slot, &pid );
        SetFavoriteItemPid( pids );
    }
    SAFEREL( pids );
}

void Critter::SyncLockCritters( bool self_critters, bool only_players )
{
    if( LogicMT )
    {
        CrVec& critters = ( self_critters ? VisCrSelf : VisCr );

        // Collect critters
        CrVec find_critters;
        find_critters.reserve( critters.size() );
        if( only_players )
        {
            for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
                if( ( *it )->IsPlayer() )
                    find_critters.push_back( *it );
        }
        else
        {
            find_critters = critters;
        }

        // Synchronize
        for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck after synchronization
        CrVec find_critters2;
        find_critters2.reserve( find_critters.size() );
        if( only_players )
        {
            for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
                if( ( *it )->IsPlayer() )
                    find_critters2.push_back( *it );
        }
        else
        {
            find_critters2 = critters;
        }

        if( !CompareContainers( find_critters, find_critters2 ) )
            SyncLockCritters( self_critters, only_players );
    }
}

void Critter::ProcessVisibleCritters()
{
    if( IsDestroyed )
        return;

    // Global map
    if( !GetMapId() )
    {
        RUNTIME_ASSERT( GroupMove );

        if( IsPlayer() )
        {
            Client* cl = (Client*) this;
            for( auto it = GroupMove->CritMove.begin(), end = GroupMove->CritMove.end(); it != end; ++it )
            {
                Critter* cr = *it;
                if( this == cr )
                {
                    SETFLAG( Flags, FCRIT_CHOSEN );
                    cl->Send_AddCritter( this );
                    UNSETFLAG( Flags, FCRIT_CHOSEN );
                    cl->Send_AddAllItems();
                }
                else
                {
                    cl->Send_AddCritter( cr );
                }
            }
        }

        return;
    }

    // Local map
    int  vis;
    int  look_base_self = GetLookDistance();
    int  dir_self = GetDir();
    int  dirs_count = DIRS_COUNT;
    bool is_show_cr_func1 = ( FuncId[ CRITTER_EVENT_SHOW_CRITTER_1 ] > 0 || FuncId[ CRITTER_EVENT_HIDE_CRITTER_1 ] > 0 );
    bool is_show_cr_func2 = ( FuncId[ CRITTER_EVENT_SHOW_CRITTER_2 ] > 0 || FuncId[ CRITTER_EVENT_HIDE_CRITTER_2 ] > 0 );
    bool is_show_cr_func3 = ( FuncId[ CRITTER_EVENT_SHOW_CRITTER_3 ] > 0 || FuncId[ CRITTER_EVENT_HIDE_CRITTER_3 ] > 0 );
    uint show_cr_dist1 = ( is_show_cr_func1 ? GetShowCritterDist1() : 0 );
    uint show_cr_dist2 = ( is_show_cr_func2 ? GetShowCritterDist2() : 0 );
    uint show_cr_dist3 = ( is_show_cr_func3 ? GetShowCritterDist3() : 0 );
    bool show_cr1 = ( is_show_cr_func1 && show_cr_dist1 > 0 );
    bool show_cr2 = ( is_show_cr_func2 && show_cr_dist2 > 0 );
    bool show_cr3 = ( is_show_cr_func3 && show_cr_dist3 > 0 );

    bool show_cr = ( show_cr1 || show_cr2 || show_cr3 );
    // Sneak self
    int  sneak_base_self = GetSneakCoefficient();
    if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_WEIGHT ) )
        sneak_base_self -= GetItemsWeight() / GameOpt.LookWeight;

    Map* map = MapMngr.GetMap( GetMapId() );
    if( !map )
        return;

    CrVec critters;
    map->GetCritters( critters, true );

    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this || cr->IsDestroyed )
            continue;

        int dist = DistGame( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY() );

        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SCRIPT ) )
        {
            bool allow_self = true;
            if( Script::PrepareContext( ServerFunctions.CheckLook, _FUNC_, GetInfo() ) )
            {
                Script::SetArgEntityOK( map );
                Script::SetArgEntityOK( this );
                Script::SetArgEntityOK( cr );
                if( Script::RunPrepared() )
                    allow_self = Script::GetReturnedBool();
            }
            bool allow_opp = true;
            if( Script::PrepareContext( ServerFunctions.CheckLook, _FUNC_, GetInfo() ) )
            {
                Script::SetArgEntityOK( map );
                Script::SetArgEntityOK( cr );
                Script::SetArgEntityOK( this );
                if( Script::RunPrepared() )
                    allow_opp = Script::GetReturnedBool();
            }

            if( allow_self )
            {
                if( cr->AddCrIntoVisVec( this ) )
                {
                    Send_AddCritter( cr );
                    EventShowCritter( cr );
                }
            }
            else
            {
                if( cr->DelCrFromVisVec( this ) )
                {
                    Send_RemoveCritter( cr );
                    EventHideCritter( cr );
                }
            }

            if( allow_opp )
            {
                if( AddCrIntoVisVec( cr ) )
                {
                    cr->Send_AddCritter( this );
                    cr->EventShowCritter( this );
                }
            }
            else
            {
                if( DelCrFromVisVec( cr ) )
                {
                    cr->Send_RemoveCritter( this );
                    cr->EventHideCritter( this );
                }
            }

            if( show_cr )
            {
                if( show_cr1 )
                {
                    if( (int) show_cr_dist1 >= dist )
                    {
                        if( AddCrIntoVisSet1( cr->GetId() ) )
                            EventShowCritter1( cr );
                    }
                    else
                    {
                        if( DelCrFromVisSet1( cr->GetId() ) )
                            EventHideCritter1( cr );
                    }
                }
                if( show_cr2 )
                {
                    if( (int) show_cr_dist2 >= dist )
                    {
                        if( AddCrIntoVisSet2( cr->GetId() ) )
                            EventShowCritter2( cr );
                    }
                    else
                    {
                        if( DelCrFromVisSet2( cr->GetId() ) )
                            EventHideCritter2( cr );
                    }
                }
                if( show_cr3 )
                {
                    if( (int) show_cr_dist3 >= dist )
                    {
                        if( AddCrIntoVisSet3( cr->GetId() ) )
                            EventShowCritter3( cr );
                    }
                    else
                    {
                        if( DelCrFromVisSet3( cr->GetId() ) )
                            EventHideCritter3( cr );
                    }
                }
            }

            if( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_1 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_1 ] > 0 )
            {
                uint cr_dist = cr->GetShowCritterDist1();
                if( cr_dist )
                {
                    if( (int) cr_dist >= dist )
                    {
                        if( cr->AddCrIntoVisSet1( GetId() ) )
                            cr->EventShowCritter1( this );
                    }
                    else
                    {
                        if( cr->DelCrFromVisSet1( GetId() ) )
                            cr->EventHideCritter1( this );
                    }
                }
            }
            if( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_2 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_2 ] > 0 )
            {
                uint cr_dist = cr->GetShowCritterDist2();
                if( cr_dist )
                {
                    if( (int) cr_dist >= dist )
                    {
                        if( cr->AddCrIntoVisSet2( GetId() ) )
                            cr->EventShowCritter2( this );
                    }
                    else
                    {
                        if( cr->DelCrFromVisSet2( GetId() ) )
                            cr->EventHideCritter2( this );
                    }
                }
            }
            if( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_3 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_3 ] > 0 )
            {
                uint cr_dist = cr->GetShowCritterDist3();
                if( cr_dist )
                {
                    if( (int) cr_dist >= dist )
                    {
                        if( cr->AddCrIntoVisSet3( GetId() ) )
                            cr->EventShowCritter3( this );
                    }
                    else
                    {
                        if( cr->DelCrFromVisSet3( GetId() ) )
                            cr->EventHideCritter3( this );
                    }
                }
            }
            continue;
        }

        int look_self = look_base_self;
        int look_opp = cr->GetLookDistance();

        // Dir modifier
        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_DIR ) )
        {
            // Self
            int real_dir = GetFarDir( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY() );
            int i = ( dir_self > real_dir ? dir_self - real_dir : real_dir - dir_self );
            if( i > dirs_count / 2 )
                i = dirs_count - i;
            look_self -= look_self * GameOpt.LookDir[ i ] / 100;
            // Opponent
            int dir_opp = cr->GetDir();
            real_dir = ( ( real_dir + dirs_count / 2 ) % dirs_count );
            i = ( dir_opp > real_dir ? dir_opp - real_dir : real_dir - dir_opp );
            if( i > dirs_count / 2 )
                i = dirs_count - i;
            look_opp -= look_opp * GameOpt.LookDir[ i ] / 100;
        }

        if( dist > look_self && dist > look_opp )
            dist = MAX_INT;

        // Trace
        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_TRACE ) && dist != MAX_INT )
        {
            TraceData trace;
            trace.TraceMap = map;
            trace.BeginHx = GetHexX();
            trace.BeginHy = GetHexY();
            trace.EndHx = cr->GetHexX();
            trace.EndHy = cr->GetHexY();
            MapMngr.TraceBullet( trace );
            if( !trace.IsFullTrace )
                dist = MAX_INT;
        }

        // Self
        if( cr->GetIsHide() && dist != MAX_INT )
        {
            int sneak_opp = cr->GetSneakCoefficient();
            if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_WEIGHT ) )
                sneak_opp -= cr->GetItemsWeight() / GameOpt.LookWeight;
            if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_DIR ) )
            {
                int real_dir = GetFarDir( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY() );
                int i = ( dir_self > real_dir ? dir_self - real_dir : real_dir - dir_self );
                if( i > dirs_count / 2 )
                    i = dirs_count - i;
                sneak_opp -= sneak_opp * GameOpt.LookSneakDir[ i ] / 100;
            }
            sneak_opp /= (int) GameOpt.SneakDivider;
            if( sneak_opp < 0 )
                sneak_opp = 0;
            vis = look_self - sneak_opp;
        }
        else
        {
            vis = look_self;
        }
        if( vis < (int) GameOpt.LookMinimum )
            vis = GameOpt.LookMinimum;

        if( vis >= dist )
        {
            if( cr->AddCrIntoVisVec( this ) )
            {
                Send_AddCritter( cr );
                EventShowCritter( cr );
            }
        }
        else
        {
            if( cr->DelCrFromVisVec( this ) )
            {
                Send_RemoveCritter( cr );
                EventHideCritter( cr );
            }
        }

        if( show_cr1 )
        {
            if( (int) show_cr_dist1 >= dist )
            {
                if( AddCrIntoVisSet1( cr->GetId() ) )
                    EventShowCritter1( cr );
            }
            else
            {
                if( DelCrFromVisSet1( cr->GetId() ) )
                    EventHideCritter1( cr );
            }
        }
        if( show_cr2 )
        {
            if( (int) show_cr_dist2 >= dist )
            {
                if( AddCrIntoVisSet2( cr->GetId() ) )
                    EventShowCritter2( cr );
            }
            else
            {
                if( DelCrFromVisSet2( cr->GetId() ) )
                    EventHideCritter2( cr );
            }
        }
        if( show_cr3 )
        {
            if( (int) show_cr_dist3 >= dist )
            {
                if( AddCrIntoVisSet3( cr->GetId() ) )
                    EventShowCritter3( cr );
            }
            else
            {
                if( DelCrFromVisSet3( cr->GetId() ) )
                    EventHideCritter3( cr );
            }
        }

        // Opponent
        if( GetIsHide() && dist != MAX_INT )
        {
            int sneak_self = sneak_base_self;
            if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_DIR ) )
            {
                int dir_opp = cr->GetDir();
                int real_dir = GetFarDir( cr->GetHexX(), cr->GetHexY(), GetHexX(), GetHexY() );
                int i = ( dir_opp > real_dir ? dir_opp - real_dir : real_dir - dir_opp );
                if( i > dirs_count / 2 )
                    i = dirs_count - i;
                sneak_self -= sneak_self * GameOpt.LookSneakDir[ i ] / 100;
            }
            sneak_self /= (int) GameOpt.SneakDivider;
            if( sneak_self < 0 )
                sneak_self = 0;
            vis = look_opp - sneak_self;
        }
        else
        {
            vis = look_opp;
        }
        if( vis < (int) GameOpt.LookMinimum )
            vis = GameOpt.LookMinimum;

        if( vis >= dist )
        {
            if( AddCrIntoVisVec( cr ) )
            {
                cr->Send_AddCritter( this );
                cr->EventShowCritter( this );
            }
        }
        else
        {
            if( DelCrFromVisVec( cr ) )
            {
                cr->Send_RemoveCritter( this );
                cr->EventHideCritter( this );
            }
        }

        if( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_1 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_1 ] > 0 )
        {
            uint cr_dist = cr->GetShowCritterDist1();
            if( cr_dist )
            {
                if( (int) cr_dist >= dist )
                {
                    if( cr->AddCrIntoVisSet1( GetId() ) )
                        cr->EventShowCritter1( this );
                }
                else
                {
                    if( cr->DelCrFromVisSet1( GetId() ) )
                        cr->EventHideCritter1( this );
                }
            }
        }
        if( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_2 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_2 ] > 0 )
        {
            uint cr_dist = cr->GetShowCritterDist2();
            if( cr_dist )
            {
                if( (int) cr_dist >= dist )
                {
                    if( cr->AddCrIntoVisSet2( GetId() ) )
                        cr->EventShowCritter2( this );
                }
                else
                {
                    if( cr->DelCrFromVisSet2( GetId() ) )
                        cr->EventHideCritter2( this );
                }
            }
        }
        if( cr->FuncId[ CRITTER_EVENT_SHOW_CRITTER_3 ] > 0 || cr->FuncId[ CRITTER_EVENT_HIDE_CRITTER_3 ] > 0 )
        {
            uint cr_dist = cr->GetShowCritterDist3();
            if( cr_dist )
            {
                if( (int) cr_dist >= dist )
                {
                    if( cr->AddCrIntoVisSet3( GetId() ) )
                        cr->EventShowCritter3( this );
                }
                else
                {
                    if( cr->DelCrFromVisSet3( GetId() ) )
                        cr->EventHideCritter3( this );
                }
            }
        }
    }
}

void Critter::ProcessVisibleItems()
{
    if( IsDestroyed )
        return;

    Map* map = MapMngr.GetMap( GetMapId() );
    if( !map )
        return;

    int     look = GetLookDistance();
    ItemVec items = map->GetItemsNoLock();
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;

        if( item->GetIsHidden() )
            continue;
        else if( item->GetIsAlwaysView() )
        {
            if( AddIdVisItem( item->GetId() ) )
            {
                Send_AddItemOnMap( item );
                EventShowItemOnMap( item, item->ViewPlaceOnMap, item->ViewByCritter );
            }
        }
        else
        {
            bool allowed = false;
            if( item->GetIsTrap() && FLAG( GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT ) )
            {
                if( Script::PrepareContext( ServerFunctions.CheckTrapLook, _FUNC_, GetInfo() ) )
                {
                    Script::SetArgEntityOK( map );
                    Script::SetArgEntityOK( this );
                    Script::SetArgEntityOK( item );
                    if( Script::RunPrepared() )
                        allowed = Script::GetReturnedBool();
                }
            }
            else
            {
                int dist = DistGame( GetHexX(), GetHexY(), item->GetHexX(), item->GetHexY() );
                if( item->GetIsTrap() )
                    dist += item->GetTrapValue();
                allowed = look >= dist;
            }

            if( allowed )
            {
                if( AddIdVisItem( item->GetId() ) )
                {
                    Send_AddItemOnMap( item );
                    EventShowItemOnMap( item, item->ViewPlaceOnMap, item->ViewByCritter );
                }
            }
            else
            {
                if( DelIdVisItem( item->GetId() ) )
                {
                    Send_EraseItemFromMap( item );
                    EventHideItemOnMap( item, item->ViewPlaceOnMap, item->ViewByCritter );
                }
            }
        }
    }
}

void Critter::ViewMap( Map* map, int look, ushort hx, ushort hy, int dir )
{
    if( IsDestroyed )
        return;

    Send_GameInfo( map );

    // Critters
    int   dirs_count = DIRS_COUNT;
    int   vis;
    CrVec critters;
    map->GetCritters( critters, true );

    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this || cr->IsDestroyed )
            continue;

        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SCRIPT ) )
        {
            if( Script::PrepareContext( ServerFunctions.CheckLook, _FUNC_, GetInfo() ) )
            {
                Script::SetArgEntityOK( map );
                Script::SetArgEntityOK( this );
                Script::SetArgEntityOK( cr );
                if( Script::RunPrepared() && Script::GetReturnedBool() )
                    Send_AddCritter( cr );
            }
            continue;
        }

        int dist = DistGame( hx, hy, cr->GetHexX(), cr->GetHexY() );
        int look_self = look;

        // Dir modifier
        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_DIR ) )
        {
            int real_dir = GetFarDir( hx, hy, cr->GetHexX(), cr->GetHexY() );
            int i = ( dir > real_dir ? dir - real_dir : real_dir - dir );
            if( i > dirs_count / 2 )
                i = dirs_count - i;
            look_self -= look_self * GameOpt.LookDir[ i ] / 100;
        }

        if( dist > look_self )
            continue;

        // Trace
        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_TRACE ) && dist != MAX_INT )
        {
            TraceData trace;
            trace.TraceMap = map;
            trace.BeginHx = hx;
            trace.BeginHy = hy;
            trace.EndHx = cr->GetHexX();
            trace.EndHy = cr->GetHexY();
            MapMngr.TraceBullet( trace );
            if( !trace.IsFullTrace )
                continue;
        }

        // Hide modifier
        if( cr->GetIsHide() )
        {
            int sneak_opp = cr->GetSneakCoefficient();
            if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_WEIGHT ) )
                sneak_opp -= cr->GetItemsWeight() / GameOpt.LookWeight;
            if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SNEAK_DIR ) )
            {
                int real_dir = GetFarDir( hx, hy, cr->GetHexX(), cr->GetHexY() );
                int i = ( dir > real_dir ? dir - real_dir : real_dir - dir );
                if( i > dirs_count / 2 )
                    i = dirs_count - i;
                sneak_opp -= sneak_opp * GameOpt.LookSneakDir[ i ] / 100;
            }
            sneak_opp /= (int) GameOpt.SneakDivider;
            if( sneak_opp < 0 )
                sneak_opp = 0;
            vis = look_self - sneak_opp;
        }
        else
        {
            vis = look_self;
        }
        if( vis < (int) GameOpt.LookMinimum )
            vis = GameOpt.LookMinimum;
        if( vis >= dist )
            Send_AddCritter( cr );
    }

    // Items
    ItemVec items = map->GetItemsNoLock();
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;

        if( item->GetIsHidden() )
            continue;
        else if( item->GetIsAlwaysView() )
            Send_AddItemOnMap( item );
        else
        {
            bool allowed = false;
            if( item->GetIsTrap() && FLAG( GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT ) )
            {
                if( Script::PrepareContext( ServerFunctions.CheckTrapLook, _FUNC_, GetInfo() ) )
                {
                    Script::SetArgEntityOK( map );
                    Script::SetArgEntityOK( this );
                    Script::SetArgEntityOK( item );
                    if( Script::RunPrepared() )
                        allowed = Script::GetReturnedBool();
                }
            }
            else
            {
                int dist = DistGame( hx, hy, item->GetHexX(), item->GetHexY() );
                if( item->GetIsTrap() )
                    dist += item->GetTrapValue();
                allowed = look >= dist;
            }


            if( allowed )
                Send_AddItemOnMap( item );
        }
    }
}

void Critter::ClearVisible()
{
    SyncLockCritters( false, false );
    SyncLockCritters( true, false );

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        auto     it_ = std::find( cr->VisCrSelf.begin(), cr->VisCrSelf.end(), this );
        if( it_ != cr->VisCrSelf.end() )
        {
            cr->VisCrSelf.erase( it_ );
            cr->VisCrSelfMap.erase( this->GetId() );
        }
        cr->Send_RemoveCritter( this );
    }
    VisCr.clear();
    VisCrMap.clear();

    for( auto it = VisCrSelf.begin(), end = VisCrSelf.end(); it != end; ++it )
    {
        Critter* cr = *it;
        auto     it_ = std::find( cr->VisCr.begin(), cr->VisCr.end(), this );
        if( it_ != cr->VisCr.end() )
        {
            cr->VisCr.erase( it_ );
            cr->VisCrMap.erase( this->GetId() );
        }
    }
    VisCrSelf.clear();
    VisCrSelfMap.clear();

    VisCr1.clear();
    VisCr2.clear();
    VisCr3.clear();

    VisItemLocker.Lock();
    VisItem.clear();
    VisItemLocker.Unlock();
}

Critter* Critter::GetCritSelf( uint crid, bool sync_lock )
{
    Critter* cr = nullptr;

    auto     it = VisCrSelfMap.find( crid );
    if( it != VisCrSelfMap.end() )
        cr = it->second;

    if( cr && sync_lock )
    {
        // Synchronize
        SYNC_LOCK( cr );

        // Recheck
        it = VisCrSelfMap.find( crid );
        if( it == VisCrSelfMap.end() )
            return GetCritSelf( crid, sync_lock );
    }

    return cr;
}

void Critter::GetCrFromVisCr( CrVec& critters, int find_type, bool vis_cr_self, bool sync_lock )
{
    CrVec& vis_cr = ( vis_cr_self ? VisCrSelf : VisCr );

    CrVec  find_critters;
    for( auto it = vis_cr.begin(), end = vis_cr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->CheckFind( find_type ) && std::find( critters.begin(), critters.end(), cr ) == critters.end() )
            find_critters.push_back( cr );
    }

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck after synchronization
        CrVec find_critters2;
        for( auto it = vis_cr.begin(), end = vis_cr.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr->CheckFind( find_type ) && std::find( critters.begin(), critters.end(), cr ) == critters.end() )
                find_critters2.push_back( cr );
        }

        // Search again, if have difference
        if( !CompareContainers( find_critters, find_critters2 ) )
        {
            GetCrFromVisCr( critters, find_type, vis_cr_self, sync_lock );
            return;
        }
    }

    critters.reserve( critters.size() + find_critters.size() );
    for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
        critters.push_back( *it );
}

bool Critter::AddCrIntoVisVec( Critter* add_cr )
{
    if( VisCrMap.count( add_cr->GetId() ) )
        return false;

    VisCr.push_back( add_cr );
    VisCrMap.insert( PAIR( add_cr->GetId(), add_cr ) );

    add_cr->VisCrSelf.push_back( this );
    add_cr->VisCrSelfMap.insert( PAIR( this->GetId(), this ) );
    return true;
}

bool Critter::DelCrFromVisVec( Critter* del_cr )
{
    auto it_map = VisCrMap.find( del_cr->GetId() );
    if( it_map == VisCrMap.end() )
        return false;

    VisCrMap.erase( it_map );
    VisCr.erase( std::find( VisCr.begin(), VisCr.end(), del_cr ) );

    auto it = std::find( del_cr->VisCrSelf.begin(), del_cr->VisCrSelf.end(), this );
    if( it != del_cr->VisCrSelf.end() )
    {
        del_cr->VisCrSelf.erase( it );
        del_cr->VisCrSelfMap.erase( this->GetId() );
    }
    return true;
}

bool Critter::AddCrIntoVisSet1( uint crid )
{
    return VisCr1.insert( crid ).second;
}

bool Critter::AddCrIntoVisSet2( uint crid )
{
    return VisCr2.insert( crid ).second;
}

bool Critter::AddCrIntoVisSet3( uint crid )
{
    return VisCr3.insert( crid ).second;
}

bool Critter::DelCrFromVisSet1( uint crid )
{
    return VisCr1.erase( crid ) != 0;
}

bool Critter::DelCrFromVisSet2( uint crid )
{
    return VisCr2.erase( crid ) != 0;
}

bool Critter::DelCrFromVisSet3( uint crid )
{
    return VisCr3.erase( crid ) != 0;
}

bool Critter::AddIdVisItem( uint item_id )
{
    VisItemLocker.Lock();
    bool result = VisItem.insert( item_id ).second;
    VisItemLocker.Unlock();
    return result;
}

bool Critter::DelIdVisItem( uint item_id )
{
    VisItemLocker.Lock();
    bool result = VisItem.erase( item_id ) != 0;
    VisItemLocker.Unlock();
    return result;
}

bool Critter::CountIdVisItem( uint item_id )
{
    VisItemLocker.Lock();
    bool result = VisItem.count( item_id ) != 0;
    VisItemLocker.Unlock();
    return result;
}

void Critter::SyncLockItems()
{
    ItemVec inv_items = invItems;
    for( auto it = inv_items.begin(), end = inv_items.end(); it != end; ++it )
        SYNC_LOCK( *it );
    if( !CompareContainers( inv_items, invItems ) )
        SyncLockItems();
}

void Critter::AddItem( Item*& item, bool send )
{
    RUNTIME_ASSERT( item );

    // Add
    if( item->GetStackable() )
    {
        Item* item_already = GetItemByPid( item->GetProtoId() );
        if( item_already )
        {
            uint count = item->GetCount();
            ItemMngr.DeleteItem( item );
            item = item_already;
            item->ChangeCount( count );
            return;
        }
    }

    item->SetSortValue( invItems );
    SetItem( item );

    // Send
    if( send )
    {
        Send_AddItem( item );
        if( item->GetCritSlot() != SLOT_INV )
            SendAA_MoveItem( item, ACTION_REFRESH, 0 );
    }

    // Change item
    if( Script::PrepareContext( ServerFunctions.CritterMoveItem, _FUNC_, GetInfo() ) )
    {
        Script::SetArgEntityOK( this );
        Script::SetArgEntityOK( item );
        Script::SetArgUChar( SLOT_GROUND );
        Script::RunPrepared();
    }
}

void Critter::SetItem( Item* item )
{
    RUNTIME_ASSERT( item );

    if( item->IsCar() && GroupMove && !GroupMove->CarId )
        GroupMove->CarId = item->GetId();
    invItems.push_back( item );

    if( item->GetAccessory() != ITEM_ACCESSORY_CRITTER )
        item->SetCritSlot( SLOT_INV );
    item->SetAccessory( ITEM_ACCESSORY_CRITTER );
    item->SetCritId( Id );

    switch( item->GetCritSlot() )
    {
    case SLOT_INV:
        break;
    case SLOT_HAND1:
        RUNTIME_ASSERT( !ItemSlotMain->GetId() );
        RUNTIME_ASSERT( !( item->IsWeapon() && !CritType::IsAnim1( GetCrType(), item->GetWeapon_Anim1() ) ) );
        ItemSlotMain = item;
        break;
    case SLOT_HAND2:
        RUNTIME_ASSERT( !ItemSlotExt->GetId() );
        ItemSlotExt = item;
        break;
    case SLOT_ARMOR:
        RUNTIME_ASSERT( !ItemSlotArmor->GetId() );
        RUNTIME_ASSERT( item->IsArmor() );
        RUNTIME_ASSERT( CritType::IsCanArmor( GetCrType() ) );
        ItemSlotArmor = item;
        break;
    default:
        break;
    }
}

void Critter::EraseItem( Item* item, bool send )
{
    RUNTIME_ASSERT( item );

    auto it = std::find( invItems.begin(), invItems.end(), item );
    RUNTIME_ASSERT( it != invItems.end() );
    invItems.erase( it );

    if( !GetMapId() && GroupMove && GroupMove->CarId == item->GetId() )
    {
        GroupMove->CarId = 0;
        SendA_GlobalInfo( GroupMove, GM_INFO_GROUP_PARAM );
    }

    item->SetAccessory( ITEM_ACCESSORY_NONE );
    TakeDefaultItem( item->GetCritSlot() );
    if( send )
        Send_EraseItem( item );
    if( item->GetCritSlot() != SLOT_INV )
        SendAA_MoveItem( item, ACTION_REFRESH, 0 );

    uchar from_slot = item->GetCritSlot();
    item->SetCritSlot( SLOT_GROUND );
    if( Script::PrepareContext( ServerFunctions.CritterMoveItem, _FUNC_, GetInfo() ) )
    {
        Script::SetArgEntityOK( this );
        Script::SetArgEntityOK( item );
        Script::SetArgUChar( from_slot );
        Script::RunPrepared();
    }
}

Item* Critter::GetItem( uint item_id, bool skip_hide )
{
    if( !item_id )
        return nullptr;

    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetId() == item_id )
        {
            if( skip_hide && item->GetIsHidden() )
                return nullptr;

            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

Item* Critter::GetInvItem( uint item_id, int transfer_type )
{
    if( transfer_type == TRANSFER_CRIT_LOOT && GetIsNoLoot() )
        return nullptr;
    if( transfer_type == TRANSFER_CRIT_STEAL && GetIsNoSteal() )
        return nullptr;

    Item* item = GetItem( item_id, true );
    if( !item || item->GetCritSlot() != SLOT_INV ||
        ( transfer_type == TRANSFER_CRIT_LOOT && item->GetIsNoLoot() ) ||
        ( transfer_type == TRANSFER_CRIT_STEAL && item->GetIsNoSteal() ) )
        return nullptr;
    return item;
}

void Critter::GetInvItems( ItemVec& items, int transfer_type, bool lock )
{
    if( transfer_type == TRANSFER_CRIT_LOOT && GetIsNoLoot() )
        return;
    if( transfer_type == TRANSFER_CRIT_STEAL && GetIsNoSteal() )
        return;

    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetCritSlot() == SLOT_INV && !item->GetIsHidden() &&
            !( transfer_type == TRANSFER_CRIT_LOOT && item->GetIsNoLoot() ) &&
            !( transfer_type == TRANSFER_CRIT_STEAL && item->GetIsNoSteal() ) )
            items.push_back( item );
    }

    if( lock )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

#pragma MESSAGE("Add explicit sync lock.")
Item* Critter::GetItemByPid( hash item_pid )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == item_pid )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

Item* Critter::GetItemByPidSlot( hash item_pid, int slot )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == item_pid && item->GetCritSlot() == slot )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

Item* Critter::GetItemByPidInvPriority( hash item_pid )
{
    ProtoItem* proto_item = ProtoMngr.GetProtoItem( item_pid );
    if( !proto_item )
        return nullptr;

    if( proto_item->GetStackable() )
    {
        for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
        {
            Item* item = *it;
            if( item->GetProtoId() == item_pid )
            {
                SYNC_LOCK( item );
                return item;
            }
        }
    }
    else
    {
        Item* another_slot = nullptr;
        for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
        {
            Item* item = *it;
            if( item->GetProtoId() == item_pid )
            {
                if( item->GetCritSlot() == SLOT_INV )
                {
                    SYNC_LOCK( item );
                    return item;
                }
                another_slot = item;
            }
        }
        if( another_slot )
            SYNC_LOCK( another_slot );
        return another_slot;
    }
    return nullptr;
}

Item* Critter::GetAmmoForWeapon( Item* weap )
{
    if( !weap->IsWeapon() )
        return nullptr;

    Item* ammo = GetItemByPid( weap->GetAmmoPid() );
    if( ammo )
        return ammo;
    ammo = GetItemByPid( weap->GetWeapon_DefaultAmmoPid() );
    if( ammo )
        return ammo;
    ammo = GetAmmo( weap->GetWeapon_Caliber() );

    // Already synchronized
    return ammo;
}

Item* Critter::GetAmmo( int caliber )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetType() == ITEM_TYPE_AMMO && item->GetAmmo_Caliber() == caliber )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

Item* Critter::GetItemCar()
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->IsCar() )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

Item* Critter::GetItemSlot( int slot )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetCritSlot() == slot )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

void Critter::GetItemsSlot( int slot, ItemVec& items, bool lock )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( slot < 0 || item->GetCritSlot() == slot )
            items.push_back( item );
    }

    if( lock )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

void Critter::GetItemsType( int type, ItemVec& items, bool lock )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetType() == type )
            items.push_back( item );
    }

    if( lock )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

uint Critter::CountItemPid( hash pid )
{
    uint res = 0;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == pid )
            res += item->GetCount();
    }
    return res;
}

bool Critter::MoveItem( uchar from_slot, uchar to_slot, uint item_id, uint count )
{
    if( !item_id )
    {
        switch( from_slot )
        {
        case SLOT_HAND1:
            item_id = ItemSlotMain->GetId();
            break;
        case SLOT_HAND2:
            item_id = ItemSlotExt->GetId();
            break;
        case SLOT_ARMOR:
            item_id = ItemSlotArmor->GetId();
            break;
        default:
            break;
        }
    }

    if( !item_id )
    {
        WriteLogF( _FUNC_, " - Item id is zero, from slot %u, to slot %u, critter '%s'.\n", from_slot, to_slot, GetInfo() );
        return false;
    }

    Item* item = GetItem( item_id, IsPlayer() );
    if( !item )
    {
        WriteLogF( _FUNC_, " - Item not found, item id %u, critter '%s'.\n", item_id, GetInfo() );
        return false;
    }

    if( item->GetCritSlot() != from_slot || from_slot == to_slot )
    {
        WriteLogF( _FUNC_, " - Wrong slots, from slot %u, from slot real %u, to slot %u, item id %u, critter '%s'.\n", from_slot, item->GetCritSlot(), to_slot, item_id, GetInfo() );
        return false;
    }

    if( to_slot != SLOT_GROUND && !SlotEnabled[ to_slot ] )
    {
        WriteLogF( _FUNC_, " - Slot %u is not allowed, critter '%s'.\n", to_slot, GetInfo() );
        return false;
    }

    if( to_slot == SLOT_GROUND && !ItemMngr.ItemCheckMove( item, item->GetCount(), this, this ) )
    {
        WriteLogF( _FUNC_, " - Move item is not allwed, critter '%s'.\n", to_slot, GetInfo() );
        return false;
    }

    Item* item_swap = ( ( to_slot != SLOT_INV && to_slot != SLOT_GROUND ) ? GetItemSlot( to_slot ) : nullptr );
    bool  allow = false;
    if( Script::PrepareContext( ServerFunctions.CritterCheckMoveItem, _FUNC_, GetInfo() ) )
    {
        Script::SetArgEntityOK( this );
        Script::SetArgEntityOK( item );
        Script::SetArgUChar( to_slot );
        Script::SetArgEntityOK( item_swap );
        if( Script::RunPrepared() )
            allow = Script::GetReturnedBool();
    }
    if( !allow )
    {
        if( IsPlayer() )
        {
            Send_AddItem( item );
            WriteLogF( _FUNC_, " - Can't move item with pid %u to slot %u, player '%s'.\n", item->GetProtoId(), to_slot, GetInfo() );
        }
        return false;
    }

    if( to_slot == SLOT_GROUND )
    {
        if( !count || count > item->GetCount() )
        {
            Send_AddItem( item );
            return false;
        }

        bool full_drop = ( !item->GetStackable() || count >= item->GetCount() );
        if( !full_drop )
        {
            if( GetMapId() )
            {
                Item* item_ = ItemMngr.SplitItem( item, count );
                if( !item_ )
                {
                    Send_AddItem( item );
                    return false;
                }
                item = item_;
            }
            else
            {
                item->ChangeCount( -(int) count );
                item = nullptr;
            }
        }
        else
        {
            EraseItem( item, false );
            if( !GetMapId() )
            {
                ItemMngr.DeleteItem( item );
                item = nullptr;
            }
        }

        if( !item )
            return true;

        Map* map = MapMngr.GetMap( GetMapId() );
        if( !map )
        {
            WriteLogF( _FUNC_, " - Map not found, map id %u, critter '%s'.\n", GetMapId(), GetInfo() );
            ItemMngr.DeleteItem( item );
            return true;
        }

        SendAA_Action( ACTION_DROP_ITEM, from_slot, item );
        item->ViewByCritter = this;
        map->AddItem( item, GetHexX(), GetHexY() );
        item->ViewByCritter = nullptr;
        item->EventDrop( this );
        EventDropItem( item );
        return true;
    }

    TakeDefaultItem( from_slot );
    TakeDefaultItem( to_slot );
    if( to_slot == SLOT_HAND1 )
        ItemSlotMain = item;
    else if( to_slot == SLOT_HAND2 )
        ItemSlotExt = item;
    else if( to_slot == SLOT_ARMOR )
        ItemSlotArmor = item;
    if( item_swap )
    {
        if( from_slot == SLOT_HAND1 )
            ItemSlotMain = item_swap;
        else if( from_slot == SLOT_HAND2 )
            ItemSlotExt = item_swap;
        else if( from_slot == SLOT_ARMOR )
            ItemSlotArmor = item_swap;
    }

    item->SetCritSlot( to_slot );
    if( item_swap )
        item_swap->SetCritSlot( from_slot );

    SendAA_MoveItem( item, ACTION_MOVE_ITEM, from_slot );
    item->EventMove( this, from_slot );
    EventMoveItem( item, from_slot );
    if( item_swap )
    {
        SendAA_MoveItem( item, ACTION_MOVE_ITEM_SWAP, to_slot );
        item->EventMove( this, to_slot );
        EventMoveItem( item, to_slot );
    }

    return true;
}

void Critter::TakeDefaultItem( uchar slot )
{
    switch( slot )
    {
    case SLOT_HAND1:
        ItemSlotMain = defItemSlotHand;
        break;
    case SLOT_HAND2:
        ItemSlotExt = defItemSlotHand;
        break;
    case SLOT_ARMOR:
        ItemSlotArmor = defItemSlotArmor;
        break;
    default:
        break;
    }

    if( slot == SLOT_HAND1 || slot == SLOT_HAND2 )
    {
        hash       hands_pid = GetHandsItemProtoId();
        ProtoItem* proto_hand = ( hands_pid ? ProtoMngr.GetProtoItem( hands_pid ) : nullptr );
        if( !proto_hand )
            proto_hand = ProtoMngr.GetProtoItem( ITEM_DEF_SLOT );
        RUNTIME_ASSERT( proto_hand );

        defItemSlotHand->SetProto( proto_hand );
        defItemSlotHand->SetMode( GetHandsItemMode() );
    }
    else if( slot == SLOT_ARMOR )
    {
        hash       armor_pid = ITEM_DEF_ARMOR;
        ProtoItem* proto_armor = ( armor_pid ? ProtoMngr.GetProtoItem( armor_pid ) : nullptr );
        if( !proto_armor )
            proto_armor = ProtoMngr.GetProtoItem( ITEM_DEF_ARMOR );
        RUNTIME_ASSERT( proto_armor );

        defItemSlotArmor->SetProto( proto_armor );
    }
}

uint Critter::CountItems()
{
    uint count = 0;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
        count += ( *it )->GetCount();
    return count;
}

ItemVec& Critter::GetInventory()
{
    SyncLockItems();
    return invItems;
}

bool Critter::IsHaveGeckItem()
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetIsGeck() )
            return true;
    }
    return false;
}

void Critter::ToKnockout( uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, ushort knock_hx, ushort knock_hy )
{
    SetCond( COND_KNOCKOUT );
    SetAnim2Knockout( anim2idle );
    SetAnim2KnockoutEnd( anim2end );
    KnockoutAp = lost_ap;
    Send_Knockout( this, anim2begin, anim2idle, knock_hx, knock_hy );
    SendA_Knockout( anim2begin, anim2idle, knock_hx, knock_hy );
}

void Critter::TryUpOnKnockout()
{
    // Critter lie on ground
    if( GetCurrentHp() <= 0 )
        return;

    // Subtract knockout ap
    if( KnockoutAp )
    {
        int cur_ap = GetCurrentAp() / AP_DIVIDER;
        if( cur_ap <= 0 )
            return;
        int ap = MIN( (int) KnockoutAp, cur_ap );
        SubAp( ap );
        KnockoutAp -= ap;
        if( KnockoutAp )
            return;
    }

    // Wait when ap regenerated to zero
    if( GetCurrentAp() < 0 )
        return;

    // Stand up
    SetCond( COND_LIFE );
    SendAA_Action( ACTION_STANDUP, GetAnim2KnockoutEnd(), nullptr );
    SetBreakTime( GameOpt.Breaktime );
}

void Critter::ToDead( uint anim2, bool send_all )
{
    bool already_dead = IsDead();

    if( GetCurrentHp() > 0 )
        SetCurrentHp( 0 );
    SetCond( COND_DEAD );
    SetAnim2Dead( anim2 );

    if( send_all )
        SendAA_Action( ACTION_DEAD, anim2, nullptr );

    if( !already_dead )
    {
        Map* map = MapMngr.GetMap( GetMapId() );
        if( map )
        {
            uint multihex = GetMultihex();
            map->UnsetFlagCritter( GetHexX(), GetHexY(), multihex, false );
            map->SetFlagCritter( GetHexX(), GetHexY(), multihex, true );
        }
    }
}

bool Critter::SetScript( const char* script_name, bool first_time )
{
    hash func_num = 0;
    if( script_name && script_name[ 0 ] )
    {
        func_num = Script::BindScriptFuncNumByScriptName( script_name, "void %s(Critter&,bool)" );
        if( !func_num )
        {
            WriteLogF( _FUNC_, " - Script '%s' bind fail, critter '%s'.\n", script_name, GetInfo() );
            return false;
        }
        SetScriptId( func_num );
    }
    else
    {
        func_num = GetScriptId();
    }

    if( func_num && Script::PrepareScriptFuncContext( func_num, _FUNC_, GetInfo() ) )
    {
        Script::SetArgEntity( this );
        Script::SetArgBool( first_time );
        Script::RunPrepared();
    }
    return true;
}

bool Critter::PrepareScriptFunc( int num_scr_func )
{
    if( num_scr_func >= CRITTER_EVENT_MAX )
        return false;
    if( FuncId[ num_scr_func ] <= 0 )
        return false;
    return Script::PrepareContext( FuncId[ num_scr_func ], _FUNC_, Str::FormatBuf( "Critter '%s', func '%s'", GetInfo(), Script::GetBindFuncName( FuncId[ num_scr_func ] ).c_str() ) );
}

void Critter::EventIdle()
{
    if( !PrepareScriptFunc( CRITTER_EVENT_IDLE ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::RunPrepared();
}

void Critter::EventFinish( bool deleted )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_FINISH ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgBool( deleted );
    Script::RunPrepared();
}

void Critter::EventDead( Critter* killer )
{
    if( PrepareScriptFunc( CRITTER_EVENT_DEAD ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( killer );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_DEAD ] > 0 )
            cr->EventSmthDead( this, killer );
    }
}

void Critter::EventRespawn()
{
    if( !PrepareScriptFunc( CRITTER_EVENT_RESPAWN ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::RunPrepared();
}

void Critter::EventShowCritter( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SHOW_CRITTER ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( cr );
    Script::RunPrepared();
}

void Critter::EventShowCritter1( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SHOW_CRITTER_1 ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( cr );
    Script::RunPrepared();
}

void Critter::EventShowCritter2( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SHOW_CRITTER_2 ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( cr );
    Script::RunPrepared();
}

void Critter::EventShowCritter3( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SHOW_CRITTER_3 ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( cr );
    Script::RunPrepared();
}

void Critter::EventHideCritter( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_HIDE_CRITTER ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( cr );
    Script::RunPrepared();
}

void Critter::EventHideCritter1( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_HIDE_CRITTER_1 ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( cr );
    Script::RunPrepared();
}

void Critter::EventHideCritter2( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_HIDE_CRITTER_2 ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( cr );
    Script::RunPrepared();
}

void Critter::EventHideCritter3( Critter* cr )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_HIDE_CRITTER_3 ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( cr );
    Script::RunPrepared();
}

void Critter::EventShowItemOnMap( Item* item, bool added, Critter* dropper )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SHOW_ITEM_ON_MAP ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( item );
    Script::SetArgBool( added );
    Script::SetArgEntityEvent( dropper );
    Script::RunPrepared();
}

void Critter::EventChangeItemOnMap( Item* item )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_CHANGE_ITEM_ON_MAP ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( item );
    Script::RunPrepared();
}

void Critter::EventHideItemOnMap( Item* item, bool removed, Critter* picker )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_HIDE_ITEM_ON_MAP ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( item );
    Script::SetArgBool( removed );
    Script::SetArgEntityEvent( picker );
    Script::RunPrepared();
}

bool Critter::EventAttack( Critter* target )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_ATTACK ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( target );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_ATTACK ] > 0 )
            cr->EventSmthAttack( this, target );
    }
    return result;
}

bool Critter::EventAttacked( Critter* attacker )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_ATTACKED ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( attacker );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_ATTACKED ] > 0 )
            cr->EventSmthAttacked( this, attacker );
    }

    if( !result && attacker )
    {
        if( Script::PrepareContext( ServerFunctions.CritterAttacked, _FUNC_, GetInfo() ) )
        {
            Script::SetArgEntity( this );
            Script::SetArgEntity( attacker );
            Script::RunPrepared();
        }
    }
    return result;
}

bool Critter::EventStealing( Critter* thief, Item* item, uint count )
{
    bool success = false;
    if( Script::PrepareContext( ServerFunctions.CritterStealing, _FUNC_, GetInfo() ) )
    {
        Script::SetArgEntity( this );
        Script::SetArgEntity( thief );
        Script::SetArgEntity( item );
        Script::SetArgUInt( count );
        if( Script::RunPrepared() )
            success = Script::GetReturnedBool();
    }

    if( PrepareScriptFunc( CRITTER_EVENT_STEALING ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( thief );
        Script::SetArgBool( success );
        Script::SetArgEntityEvent( item );
        Script::SetArgUInt( count );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_STEALING ] > 0 )
            cr->EventSmthStealing( this, thief, success, item, count );
    }

    return success;
}

void Critter::EventMessage( Critter* from_cr, int num, int val )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_MESSAGE ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( from_cr );
    Script::SetArgUInt( num );
    Script::SetArgUInt( val );
    Script::RunPrepared();
}

bool Critter::EventUseItem( Item* item, Critter* on_critter, Item* on_item, Item* on_scenery )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_USE_ITEM ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( item );
        Script::SetArgEntityEvent( on_critter );
        Script::SetArgEntityEvent( on_item );
        Script::SetArgEntityEvent( on_scenery );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_USE_ITEM ] > 0 )
            cr->EventSmthUseItem( this, item, on_critter, on_item, on_scenery );
    }

    return result;
}

bool Critter::EventUseItemOnMe( Critter* who_use, Item* item )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_USE_ITEM_ON_ME ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( who_use );
        Script::SetArgEntityEvent( item );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

bool Critter::EventUseSkill( int skill, Critter* on_critter, Item* on_item, Item* on_scenery )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_USE_SKILL ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgUInt( skill );
        Script::SetArgEntityEvent( on_critter );
        Script::SetArgEntityEvent( on_item );
        Script::SetArgEntityEvent( on_scenery );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_USE_SKILL ] > 0 )
            cr->EventSmthUseSkill( this, skill, on_critter, on_item, on_scenery );
    }

    return result;
}

bool Critter::EventUseSkillOnMe( Critter* who_use, int skill )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_USE_SKILL_ON_ME ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( who_use );
        Script::SetArgUInt( skill );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

void Critter::EventDropItem( Item* item )
{
    if( PrepareScriptFunc( CRITTER_EVENT_DROP_ITEM ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( item );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_DROP_ITEM ] > 0 )
            cr->EventSmthDropItem( this, item );
    }
}

void Critter::EventMoveItem( Item* item, uchar from_slot )
{
    if( Script::PrepareContext( ServerFunctions.CritterMoveItem, _FUNC_, GetInfo() ) )
    {
        Script::SetArgEntityOK( this );
        Script::SetArgEntityOK( item );
        Script::SetArgUChar( from_slot );
        Script::RunPrepared();
    }

    if( PrepareScriptFunc( CRITTER_EVENT_MOVE_ITEM ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( item );
        Script::SetArgUChar( from_slot );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_MOVE_ITEM ] > 0 )
            cr->EventSmthMoveItem( this, item, from_slot );
    }
}

void Critter::EventKnockout( uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, uint knock_dist )
{
    if( PrepareScriptFunc( CRITTER_EVENT_KNOCKOUT ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgUInt( anim2begin );
        Script::SetArgUInt( anim2idle );
        Script::SetArgUInt( anim2end );
        Script::SetArgUInt( lost_ap );
        Script::SetArgUInt( knock_dist );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_KNOCKOUT ] > 0 )
            cr->EventSmthKnockout( this, anim2begin, anim2idle, anim2end, lost_ap, knock_dist );
    }
}

void Critter::EventSmthDead( Critter* from_cr, Critter* killer )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_DEAD ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( from_cr );
    Script::SetArgEntityEvent( killer );
    Script::RunPrepared();
}

void Critter::EventSmthStealing( Critter* from_cr, Critter* thief, bool success, Item* item, uint count )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_STEALING ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( from_cr );
    Script::SetArgEntityEvent( thief );
    Script::SetArgBool( success );
    Script::SetArgEntityEvent( item );
    Script::SetArgUInt( count );
    Script::RunPrepared();
}

void Critter::EventSmthAttack( Critter* from_cr, Critter* target )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_ATTACK ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( from_cr );
    Script::SetArgEntityEvent( target );
    Script::RunPrepared();
}

void Critter::EventSmthAttacked( Critter* from_cr, Critter* attacker )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_ATTACKED ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( from_cr );
    Script::SetArgEntityEvent( attacker );
    Script::RunPrepared();
}

void Critter::EventSmthUseItem( Critter* from_cr, Item* item, Critter* on_critter, Item* on_item, Item* on_scenery )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_USE_ITEM ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( from_cr );
    Script::SetArgEntityEvent( item );
    Script::SetArgEntityEvent( on_critter );
    Script::SetArgEntityEvent( on_item );
    Script::SetArgEntityEvent( on_scenery );
    Script::RunPrepared();
}

void Critter::EventSmthUseSkill( Critter* from_cr, int skill, Critter* on_critter, Item* on_item, Item* on_scenery )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_USE_SKILL ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( from_cr );
    Script::SetArgUInt( skill );
    Script::SetArgEntityEvent( on_critter );
    Script::SetArgEntityEvent( on_item );
    Script::SetArgEntityEvent( on_scenery );
    Script::RunPrepared();
}

void Critter::EventSmthDropItem( Critter* from_cr, Item* item )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_DROP_ITEM ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( from_cr );
    Script::SetArgEntityEvent( item );
    Script::RunPrepared();
}

void Critter::EventSmthMoveItem( Critter* from_cr, Item* item, uchar from_slot )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_MOVE_ITEM ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( from_cr );
    Script::SetArgEntityEvent( item );
    Script::SetArgUChar( from_slot );
    Script::RunPrepared();
}

void Critter::EventSmthKnockout( Critter* from_cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, uint knock_dist )
{
    if( !PrepareScriptFunc( CRITTER_EVENT_SMTH_KNOCKOUT ) )
        return;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( from_cr );
    Script::SetArgUInt( anim2begin );
    Script::SetArgUInt( anim2idle );
    Script::SetArgUInt( anim2end );
    Script::SetArgUInt( lost_ap );
    Script::SetArgUInt( knock_dist );
    Script::RunPrepared();
}

int Critter::EventPlaneBegin( AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item )
{
    if( PrepareScriptFunc( CRITTER_EVENT_PLANE_BEGIN ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgPtr( plane );
        Script::SetArgUInt( reason );
        Script::SetArgEntityEvent( some_cr );
        Script::SetArgEntityEvent( some_item );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return PLANE_RUN_GLOBAL;
}

int Critter::EventPlaneEnd( AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item )
{
    if( PrepareScriptFunc( CRITTER_EVENT_PLANE_END ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgPtr( plane );
        Script::SetArgUInt( reason );
        Script::SetArgEntityEvent( some_cr );
        Script::SetArgEntityEvent( some_item );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return PLANE_RUN_GLOBAL;
}

int Critter::EventPlaneRun( AIDataPlane* plane, int reason, uint& p0, uint& p1, uint& p2 )
{
    if( PrepareScriptFunc( CRITTER_EVENT_PLANE_RUN ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgPtr( plane );
        Script::SetArgUInt( reason );
        Script::SetArgAddress( &p0 );
        Script::SetArgAddress( &p1 );
        Script::SetArgAddress( &p2 );
        if( Script::RunPrepared() )
            return Script::GetReturnedUInt();
    }
    return PLANE_RUN_GLOBAL;
}

bool Critter::EventBarter( Critter* cr_barter, bool attach, uint barter_count )
{
    if( FuncId[ CRITTER_EVENT_BARTER ] <= 0 )
        return true;
    if( !PrepareScriptFunc( CRITTER_EVENT_BARTER ) )
        return false;
    Script::SetArgEntity( this );
    Script::SetArgEntity( cr_barter );
    Script::SetArgBool( attach );
    Script::SetArgUInt( barter_count );
    if( Script::RunPrepared() )
        return Script::GetReturnedBool();
    return false;
}

bool Critter::EventTalk( Critter* cr_talk, bool attach, uint talk_count )
{
    if( FuncId[ CRITTER_EVENT_TALK ] <= 0 )
        return true;
    if( !PrepareScriptFunc( CRITTER_EVENT_TALK ) )
        return false;
    Script::SetArgEntityEvent( this );
    Script::SetArgEntityEvent( cr_talk );
    Script::SetArgBool( attach );
    Script::SetArgUInt( talk_count );
    if( Script::RunPrepared() )
        return Script::GetReturnedBool();
    return false;
}

bool Critter::EventGlobalProcess( int type, Item* car, float& x, float& y, float& to_x, float& to_y, float& speed, uint& encounter_descriptor, bool& wait_for_answer )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_GLOBAL_PROCESS ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgUInt( type );
        Script::SetArgEntityEvent( car );
        Script::SetArgAddress( &x );
        Script::SetArgAddress( &y );
        Script::SetArgAddress( &to_x );
        Script::SetArgAddress( &to_y );
        Script::SetArgAddress( &speed );
        Script::SetArgAddress( &encounter_descriptor );
        Script::SetArgAddress( &wait_for_answer );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

bool Critter::EventGlobalInvite( Item* car, uint encounter_descriptor, int combat_mode, uint& map_id, ushort& hx, ushort& hy, uchar& dir )
{
    bool result = false;
    if( PrepareScriptFunc( CRITTER_EVENT_GLOBAL_INVITE ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( car );
        Script::SetArgUInt( encounter_descriptor );
        Script::SetArgUInt( combat_mode );
        Script::SetArgAddress( &map_id );
        Script::SetArgAddress( &hx );
        Script::SetArgAddress( &hy );
        Script::SetArgAddress( &dir );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

void Critter::EventTurnBasedProcess( Map* map, bool begin_turn )
{
    if( PrepareScriptFunc( CRITTER_EVENT_TURN_BASED_PROCESS ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( map );
        Script::SetArgBool( begin_turn );
        Script::RunPrepared();
    }

    CrVec crits = VisCr;
    for( auto it = crits.begin(), end = crits.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        if( cr->FuncId[ CRITTER_EVENT_SMTH_TURN_BASED_PROCESS ] > 0 )
            cr->EventSmthTurnBasedProcess( this, map, begin_turn );
    }
}

void Critter::EventSmthTurnBasedProcess( Critter* from_cr, Map* map, bool begin_turn )
{
    if( PrepareScriptFunc( CRITTER_EVENT_SMTH_TURN_BASED_PROCESS ) )
    {
        Script::SetArgEntityEvent( this );
        Script::SetArgEntityEvent( from_cr );
        Script::SetArgEntityEvent( map );
        Script::SetArgBool( begin_turn );
        Script::RunPrepared();
    }
}

void Critter::Send_Property( NetProperty::Type type, Property* prop, Entity* entity )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Property( type, prop, entity );
}
void Critter::Send_Move( Critter* from_cr, uint move_params )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Move( from_cr, move_params );
}
void Critter::Send_Dir( Critter* from_cr )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Dir( from_cr );
}
void Critter::Send_AddCritter( Critter* cr )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AddCritter( cr );
}
void Critter::Send_RemoveCritter( Critter* cr )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_RemoveCritter( cr );
}
void Critter::Send_LoadMap( Map* map )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_LoadMap( map );
}
void Critter::Send_XY( Critter* cr )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_XY( cr );
}
void Critter::Send_AddItemOnMap( Item* item )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AddItemOnMap( item );
}
void Critter::Send_EraseItemFromMap( Item* item )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_EraseItemFromMap( item );
}
void Critter::Send_AnimateItem( Item* item, uchar from_frm, uchar to_frm )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AnimateItem( item, from_frm, to_frm );
}
void Critter::Send_AddItem( Item* item )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AddItem( item );
}
void Critter::Send_EraseItem( Item* item )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_EraseItem( item );
}
void Critter::Send_ContainerInfo()
{
    if( IsPlayer() )
        ( (Client*) this )->Send_ContainerInfo();
}
void Critter::Send_ContainerInfo( Item* item_cont, uchar transfer_type, bool open_screen )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_ContainerInfo( item_cont, transfer_type, open_screen );
}
void Critter::Send_ContainerInfo( Critter* cr_cont, uchar transfer_type, bool open_screen )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_ContainerInfo( cr_cont, transfer_type, open_screen );
}
void Critter::Send_GlobalInfo( uchar flags )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_GlobalInfo( flags );
}
void Critter::Send_GlobalLocation( Location* loc, bool add )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_GlobalLocation( loc, add );
}
void Critter::Send_GlobalMapFog( ushort zx, ushort zy, uchar fog )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_GlobalMapFog( zx, zy, fog );
}
void Critter::Send_AllProperties()
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AllProperties();
}
void Critter::Send_CustomCommand( Critter* cr, ushort cmd, int val )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_CustomCommand( cr, cmd, val );
}
void Critter::Send_Talk()
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Talk();
}
void Critter::Send_GameInfo( Map* map )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_GameInfo( map );
}
void Critter::Send_Text( Critter* from_cr, const char* s_str, uchar how_say )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Text( from_cr, s_str, how_say );
}
void Critter::Send_TextEx( uint from_id, const char* s_str, ushort str_len, uchar how_say, bool unsafe_text )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_TextEx( from_id, s_str, str_len, how_say, unsafe_text );
}
void Critter::Send_TextMsg( Critter* from_cr, uint str_num, uchar how_say, ushort num_msg )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_TextMsg( from_cr, str_num, how_say, num_msg );
}
void Critter::Send_TextMsg( uint from_id, uint str_num, uchar how_say, ushort num_msg )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_TextMsg( from_id, str_num, how_say, num_msg );
}
void Critter::Send_TextMsgLex( Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_TextMsgLex( from_cr, num_str, how_say, num_msg, lexems );
}
void Critter::Send_TextMsgLex( uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_TextMsgLex( from_id, num_str, how_say, num_msg, lexems );
}
void Critter::Send_Action( Critter* from_cr, int action, int action_ext, Item* item )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Action( from_cr, action, action_ext, item );
}
void Critter::Send_Knockout( Critter* from_cr, uint anim2begin, uint anim2idle, ushort knock_hx, ushort knock_hy )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Knockout( from_cr, anim2begin, anim2idle, knock_hx, knock_hy );
}
void Critter::Send_MoveItem( Critter* from_cr, Item* item, uchar action, uchar prev_slot )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_MoveItem( from_cr, item, action, prev_slot );
}
void Critter::Send_Animate( Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Animate( from_cr, anim1, anim2, item, clear_sequence, delay_play );
}
void Critter::Send_SetAnims( Critter* from_cr, int cond, uint anim1, uint anim2 )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_SetAnims( from_cr, cond, anim1, anim2 );
}
void Critter::Send_CombatResult( uint* combat_res, uint len )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_CombatResult( combat_res, len );
}
void Critter::Send_HoloInfo( bool clear, ushort offset, ushort count )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_HoloInfo( clear, offset, count );
}
void Critter::Send_AutomapsInfo( void* locs_vec, Location* loc )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AutomapsInfo( locs_vec, loc );
}
void Critter::Send_Follow( uint rule, uchar follow_type, hash map_pid, uint follow_wait )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Follow( rule, follow_type, map_pid, follow_wait );
}
void Critter::Send_Effect( hash eff_pid, ushort hx, ushort hy, ushort radius )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Effect( eff_pid, hx, hy, radius );
}
void Critter::Send_FlyEffect( hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_FlyEffect( eff_pid, from_crid, to_crid, from_hx, from_hy, to_hx, to_hy );
}
void Critter::Send_PlaySound( uint crid_synchronize, const char* sound_name )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_PlaySound( crid_synchronize, sound_name );
}
void Critter::Send_PlaySoundType( uint crid_synchronize, uchar sound_type, uchar sound_type_ext, uchar sound_id, uchar sound_id_ext )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_PlaySoundType( crid_synchronize, sound_type, sound_type_ext, sound_id, sound_id_ext );
}

void Critter::SendA_Property( NetProperty::Type type, Property* prop, Entity* entity )
{
    if( VisCr.empty() )
        return;

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Property( type, prop, entity );
    }
}

void Critter::SendA_Move( uint move_params )
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Move( this, move_params );
    }
}

void Critter::SendA_XY()
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_XY( this );
    }
}

void Critter::SendA_Action( int action, int action_ext, Item* item )
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Action( this, action, action_ext, item );
    }
}

void Critter::SendAA_Action( int action, int action_ext, Item* item )
{
    if( IsPlayer() )
        Send_Action( this, action, action_ext, item );

    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Action( this, action, action_ext, item );
    }
}

void Critter::SendA_Knockout( uint anim2begin, uint anim2idle, ushort knock_hx, ushort knock_hy )
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Knockout( this, anim2begin, anim2idle, knock_hx, knock_hy );
    }
}

void Critter::SendAA_MoveItem( Item* item, uchar action, uchar prev_slot )
{
    if( IsPlayer() )
        Send_MoveItem( this, item, action, prev_slot );

    if( VisCr.empty() )
        return;
    SyncLockCritters( false, true );

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_MoveItem( this, item, action, prev_slot );
    }
}

void Critter::SendAA_Animate( uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play )
{
    if( IsPlayer() )
        Send_Animate( this, anim1, anim2, item, clear_sequence, delay_play );

    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Animate( this, anim1, anim2, item, clear_sequence, delay_play );
    }
}

void Critter::SendAA_SetAnims( int cond, uint anim1, uint anim2 )
{
    if( IsPlayer() )
        Send_SetAnims( this, cond, anim1, anim2 );

    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_SetAnims( this, cond, anim1, anim2 );
    }
}

void Critter::SendA_GlobalInfo( GlobalMapGroup* group, uchar info_flags )
{
    if( !group )
        return;

    CrVec cr_group = group->CritMove;
    for( auto it = cr_group.begin(), end = cr_group.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
        {
            SYNC_LOCK( cr );
            cr->Send_GlobalInfo( info_flags );
        }
    }
}

void Critter::SendAA_Text( CrVec& to_cr, const char* str, uchar how_say, bool unsafe_text )
{
    if( !str || !str[ 0 ] )
        return;

    ushort str_len = Str::Length( str );
    uint   from_id = GetId();

    if( IsPlayer() )
        Send_TextEx( from_id, str, str_len, how_say, unsafe_text );

    if( to_cr.empty() )
        return;

    int dist = -1;
    if( how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD )
        dist = GameOpt.ShoutDist + GetMultihex();
    else if( how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD )
        dist = GameOpt.WhisperDist + GetMultihex();

    for( auto it = to_cr.begin(), end = to_cr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this || !cr->IsPlayer() )
            continue;

        // SYNC_LOCK(cr);

        if( dist == -1 )
            cr->Send_TextEx( from_id, str, str_len, how_say, unsafe_text );
        else if( CheckDist( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex() ) )
            cr->Send_TextEx( from_id, str, str_len, how_say, unsafe_text );
    }
}

void Critter::SendAA_Msg( CrVec& to_cr, uint num_str, uchar how_say, ushort num_msg )
{
    if( IsPlayer() )
        Send_TextMsg( this, num_str, how_say, num_msg );

    if( to_cr.empty() )
        return;

    int dist = -1;
    if( how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD )
        dist = GameOpt.ShoutDist + GetMultihex();
    else if( how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD )
        dist = GameOpt.WhisperDist + GetMultihex();

    for( auto it = to_cr.begin(), end = to_cr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this )
            continue;
        if( !cr->IsPlayer() )
            continue;

        // SYNC_LOCK(cr);

        if( dist == -1 )
            cr->Send_TextMsg( this, num_str, how_say, num_msg );
        else if( CheckDist( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex() ) )
            cr->Send_TextMsg( this, num_str, how_say, num_msg );
    }
}

void Critter::SendAA_MsgLex( CrVec& to_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsPlayer() )
        Send_TextMsgLex( this, num_str, how_say, num_msg, lexems );

    if( to_cr.empty() )
        return;

    int dist = -1;
    if( how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD )
        dist = GameOpt.ShoutDist + GetMultihex();
    else if( how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD )
        dist = GameOpt.WhisperDist + GetMultihex();

    for( auto it = to_cr.begin(), end = to_cr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this )
            continue;
        if( !cr->IsPlayer() )
            continue;

        // SYNC_LOCK(cr);

        if( dist == -1 )
            cr->Send_TextMsgLex( this, num_str, how_say, num_msg, lexems );
        else if( CheckDist( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex() ) )
            cr->Send_TextMsgLex( this, num_str, how_say, num_msg, lexems );
    }
}

void Critter::SendA_Dir()
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Dir( this );
    }
}

void Critter::SendA_Follow( uchar follow_type, hash map_pid, uint follow_wait )
{
    if( VisCr.empty() )
        return;
    SyncLockCritters( false, true );

    int dist = FOLLOW_DIST + GetMultihex();
    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !cr->IsPlayer() )
            continue;
        if( cr->GetFollowCrit() != GetId() )
            continue;
        if( cr->IsDead() || cr->IsKnockout() )
            continue;
        if( !CheckDist( GetLastMapHexX(), GetLastMapHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex() ) )
            continue;
        cr->Send_Follow( GetId(), follow_type, map_pid, follow_wait );
    }
}

void Critter::SendA_CustomCommand( ushort num_param, int val )
{
    if( VisCr.empty() )
        return;
    // SyncLockCritters(true);

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_CustomCommand( this, num_param, val );
    }
}

void Critter::Send_AddAllItems()
{
    if( !IsPlayer() )
        return;

    SyncLockItems();

    Client* cl = (Client*) this;
    BOUT_BEGIN( cl );
    cl->Bout << NETMSG_CLEAR_ITEMS;
    BOUT_END( cl );

    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
        Send_AddItem( *it );

    BOUT_BEGIN( cl );
    cl->Bout << NETMSG_ALL_ITEMS_SEND;
    BOUT_END( cl );
}

void Critter::Send_AllAutomapsInfo()
{
    if( !IsPlayer() )
        return;

    LocVec       locs;
    ScriptArray* known_locs = GetKnownLocations();
    for( uint i = 0, j = known_locs->GetSize(); i < j; i++ )
    {
        uint      loc_id = *(uint*) known_locs->At( i );
        Location* loc = MapMngr.GetLocation( loc_id );
        if( loc && loc->IsNonEmptyAutomaps() )
            locs.push_back( loc );
    }
    SAFEREL( known_locs );

    Send_AutomapsInfo( &locs, nullptr );
}

void Critter::SendMessage( int num, int val, int to )
{
    switch( to )
    {
    case MESSAGE_TO_VISIBLE_ME:
    {
        CrVec critters = VisCr;
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            SYNC_LOCK( cr );
            cr->EventMessage( this, num, val );
        }
    }
    break;
    case MESSAGE_TO_IAM_VISIBLE:
    {
        CrVec critters = VisCrSelf;
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            SYNC_LOCK( cr );
            cr->EventMessage( this, num, val );
        }
    }
    break;
    case MESSAGE_TO_ALL_ON_MAP:
    {
        Map* map = MapMngr.GetMap( GetMapId() );
        if( !map )
            return;

        CrVec critters;
        map->GetCritters( critters, true );
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            cr->EventMessage( this, num, val );
        }
    }
    break;
    default:
        break;
    }
}

void Critter::RefreshName()
{
    if( IsPlayer() )
    {
        *NameStr = ( (Client*) this )->Name;
    }
    else
    {
        hash        dlg_pack_id = GetDialogId();
        DialogPack* dlg_pack = ( dlg_pack_id ? DlgMngr.GetDialog( dlg_pack_id ) : nullptr );
        char        buf[ MAX_FOTEXT ];
        if( dlg_pack )
            *NameStr = Str::Format( buf, "%s (Npc)", dlg_pack->PackName.c_str(), GetId() );
        else
            *NameStr = Str::Format( buf, "%u (Npc)", GetId() );
    }
}

const char* Critter::GetInfo()
{
    return GetName();
}

bool Critter::IsCanWalk()
{
    return CritType::IsCanWalk( GetCrType() ) && !GetIsNoWalk();
}

bool Critter::IsCanRun()
{
    return CritType::IsCanRun( GetCrType() ) && !GetIsNoRun();
}

uint Critter::GetTimeWalk()
{
    int walk_time = GetWalkTime();
    if( walk_time <= 0 )
        walk_time = CritType::GetTimeWalk( GetCrType() );
    return walk_time > 0 ? walk_time : 400;
}

uint Critter::GetTimeRun()
{
    int walk_time = GetRunTime();
    if( walk_time <= 0 )
        walk_time = CritType::GetTimeRun( GetCrType() );
    return walk_time > 0 ? walk_time : 200;
}

uint Critter::GetItemsWeight()
{
    uint res = 0;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( !item->GetIsHidden() )
            res += item->GetWholeWeight();
    }
    return res;
}

uint Critter::GetItemsVolume()
{
    uint res = 0;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( !item->GetIsHidden() )
            res += item->GetWholeVolume();
    }
    return res;
}

bool Critter::IsOverweight()
{
    return (int) GetItemsWeight() > GetCarryWeight();
}

int Critter::GetFreeWeight()
{
    int cur_weight = GetItemsWeight();
    int max_weight = GetCarryWeight();
    return max_weight - cur_weight;
}

int Critter::GetFreeVolume()
{
    int cur_volume = GetItemsVolume();
    int max_volume = CRITTER_INV_VOLUME;
    return max_volume - cur_volume;
}

bool Critter::IsTurnBased()
{
    return TB_BATTLE_TIMEOUT_CHECK( GetTimeoutBattle() );
}

bool Critter::CheckMyTurn( Map* map )
{
    if( !IsTurnBased() )
        return true;
    if( !map )
        map = MapMngr.GetMap( GetMapId() );
    if( !map || !map->IsTurnBasedOn || map->IsCritterTurn( this ) )
        return true;
    return false;
}

int Critter::GetApCostCritterMove( bool is_run )
{
    if( IsTurnBased() )
        return GameOpt.TbApCostCritterMove * AP_DIVIDER * ( IsDmgTwoLeg() ? 4 : ( IsDmgLeg() ? 2 : 1 ) );
    else
        return IS_TIMEOUT( GetTimeoutBattle() ) ? ( is_run ? GameOpt.RtApCostCritterRun : GameOpt.RtApCostCritterWalk ) : 0;
}

int Critter::GetApCostMoveItemContainer()
{
    return IsTurnBased() ? GameOpt.TbApCostMoveItemContainer : GameOpt.RtApCostMoveItemContainer;
}

int Critter::GetApCostMoveItemInventory()
{
    int val = IsTurnBased() ? GameOpt.TbApCostMoveItemInventory : GameOpt.RtApCostMoveItemInventory;
    if( GetPerkQuickPockets() )
        val /= 2;
    return val;
}

int Critter::GetApCostPickItem()
{
    return IsTurnBased() ? GameOpt.TbApCostPickItem : GameOpt.RtApCostPickItem;
}

int Critter::GetApCostDropItem()
{
    return IsTurnBased() ? GameOpt.TbApCostDropItem : GameOpt.RtApCostDropItem;
}

int Critter::GetApCostPickCritter()
{
    return IsTurnBased() ? GameOpt.TbApCostPickCritter : GameOpt.RtApCostPickCritter;
}

int Critter::GetApCostUseSkill()
{
    return IsTurnBased() ? GameOpt.TbApCostUseSkill : GameOpt.RtApCostUseSkill;
}

void Critter::SetHome( uint map_id, ushort hx, ushort hy, uchar dir )
{
    SetHomeMapId( map_id );
    SetHomeHexX( hx );
    SetHomeHexY( hy );
    SetHomeDir( dir );
}

bool Critter::IsInHome()
{
    if( !GetMapId() )
        return true;
    return GetHomeDir() == GetDir() && GetHomeHexX() == GetHexX() && GetHomeHexY() == GetHexY() && GetHomeMapId() == GetMapId();
}

/************************************************************************/
/* Timeouts                                                             */
/************************************************************************/

bool Critter::IsTransferTimeouts( bool send )
{
    if( IS_TIMEOUT( GetTimeoutTransfer() ) )
    {
        if( send )
            Send_TextMsg( this, STR_TIMEOUT_TRANSFER_WAIT, SAY_NETMSG, TEXTMSG_GAME );
        return true;
    }
    if( IS_TIMEOUT( GetTimeoutBattle() ) )
    {
        if( send )
            Send_TextMsg( this, STR_TIMEOUT_BATTLE_WAIT, SAY_NETMSG, TEXTMSG_GAME );
        return true;
    }
    return false;
}

/************************************************************************/
/* Enemy stack                                                          */
/************************************************************************/

void Critter::AddEnemyToStack( uint crid )
{
    if( !crid )
        return;

    // Find already
    ScriptArray* enemy_stack = GetEnemyStack();
    uint         stack_count = enemy_stack->GetSize();
    for( uint i = 0; i < stack_count; i++ )
    {
        if( *(uint*) enemy_stack->At( i ) == crid )
        {
            if( i < stack_count - 1 )
            {
                enemy_stack->RemoveAt( i );
                enemy_stack->InsertLast( &crid );
                SetEnemyStack( enemy_stack );
            }
            SAFEREL( enemy_stack );
            return;
        }
    }

    // Add
    enemy_stack->InsertLast( &crid );
    SetEnemyStack( enemy_stack );
    SAFEREL( enemy_stack );
}

bool Critter::CheckEnemyInStack( uint crid )
{
    if( GetIsNoEnemyStack() )
        return false;

    ScriptArray* enemy_stack = GetEnemyStack();
    uint         stack_count = enemy_stack->GetSize();
    for( uint i = 0; i < stack_count; i++ )
    {
        if( *(uint*) enemy_stack->At( i ) == crid )
        {
            SAFEREL( enemy_stack );
            return true;
        }
    }
    SAFEREL( enemy_stack );
    return false;
}

void Critter::EraseEnemyInStack( uint crid )
{
    ScriptArray* enemy_stack = GetEnemyStack();
    uint         stack_count = enemy_stack->GetSize();
    for( uint i = 0; i < stack_count; i++ )
    {
        if( *(uint*) enemy_stack->At( i ) == crid )
        {
            enemy_stack->RemoveAt( i );
            SetEnemyStack( enemy_stack );
            break;
        }
    }
    SAFEREL( enemy_stack );
}

Critter* Critter::ScanEnemyStack()
{
    if( GetIsNoEnemyStack() )
        return nullptr;

    ScriptArray* enemy_stack = GetEnemyStack();
    for( int i = (int) enemy_stack->GetSize() - 1; i >= 0; i-- )
    {
        Critter* enemy = GetCritSelf( *(uint*) enemy_stack->At( i ), true );
        if( enemy && !enemy->IsDead() )
        {
            SAFEREL( enemy_stack );
            return enemy;
        }
    }
    SAFEREL( enemy_stack );
    return nullptr;
}

/************************************************************************/
/* Locations                                                            */
/************************************************************************/

bool Critter::CheckKnownLocById( uint loc_id )
{
    ScriptArray* known_locs = GetKnownLocations();
    for( uint i = 0, j = known_locs->GetSize(); i < j; i++ )
    {
        if( *(uint*) known_locs->At( i ) == loc_id )
        {
            SAFEREL( known_locs );
            return true;
        }
    }
    SAFEREL( known_locs );
    return false;
}

bool Critter::CheckKnownLocByPid( hash loc_pid )
{
    ScriptArray* known_locs = GetKnownLocations();
    SAFEREL( known_locs );
    for( uint i = 0, j = known_locs->GetSize(); i < j; i++ )
    {
        Location* loc = MapMngr.GetLocation( *(uint*) known_locs->At( i ) );
        if( loc && loc->GetProtoId() == loc_pid )
        {
            SAFEREL( known_locs );
            return true;
        }
    }
    SAFEREL( known_locs );
    return false;
}

void Critter::AddKnownLoc( uint loc_id )
{
    if( CheckKnownLocById( loc_id ) )
        return;

    ScriptArray* known_locs = GetKnownLocations();
    known_locs->InsertLast( &loc_id );
    SetKnownLocations( known_locs );
    SAFEREL( known_locs );
}

void Critter::EraseKnownLoc( uint loc_id )
{
    ScriptArray* known_locs = GetKnownLocations();
    for( uint i = 0, j = known_locs->GetSize(); i < j; i++ )
    {
        if( *(uint*) known_locs->At( i ) == loc_id )
        {
            known_locs->RemoveAt( i );
            SetKnownLocations( known_locs );
            break;
        }
    }
    SAFEREL( known_locs );
}

/************************************************************************/
/* Misc events                                                          */
/************************************************************************/

void Critter::AddCrTimeEvent( hash func_num, uint rate, uint duration, int identifier )
{
    if( duration )
        duration += GameOpt.FullSecond;

    ScriptArray* te_next_time = GetTE_NextTime();
    ScriptArray* te_func_num = GetTE_FuncNum();
    ScriptArray* te_rate = GetTE_Rate();
    ScriptArray* te_identifier = GetTE_Identifier();
    RUNTIME_ASSERT( te_next_time->GetSize() == te_func_num->GetSize() );
    RUNTIME_ASSERT( te_func_num->GetSize() == te_rate->GetSize() );
    RUNTIME_ASSERT( te_rate->GetSize() == te_identifier->GetSize() );

    uint i = 0;
    for( uint j = te_func_num->GetSize(); i < j; i++ )
        if( duration < *(uint*) te_next_time->At( i ) )
            break;

    te_next_time->InsertAt( i, &duration );
    te_func_num->InsertAt( i, &func_num );
    te_rate->InsertAt( i, &rate );
    te_identifier->InsertAt( i, &identifier );

    SetTE_NextTime( te_next_time );
    SetTE_FuncNum( te_func_num );
    SetTE_Rate( te_rate );
    SetTE_Identifier( te_identifier );

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();
}

void Critter::EraseCrTimeEvent( int index )
{
    ScriptArray* te_next_time = GetTE_NextTime();
    ScriptArray* te_func_num = GetTE_FuncNum();
    ScriptArray* te_rate = GetTE_Rate();
    ScriptArray* te_identifier = GetTE_Identifier();
    RUNTIME_ASSERT( te_next_time->GetSize() == te_func_num->GetSize() );
    RUNTIME_ASSERT( te_func_num->GetSize() == te_rate->GetSize() );
    RUNTIME_ASSERT( te_rate->GetSize() == te_identifier->GetSize() );

    if( index < (int) te_next_time->GetSize() )
    {
        te_next_time->RemoveAt( index );
        te_func_num->RemoveAt( index );
        te_rate->RemoveAt( index );
        te_identifier->RemoveAt( index );

        SetTE_NextTime( te_next_time );
        SetTE_FuncNum( te_func_num );
        SetTE_Rate( te_rate );
        SetTE_Identifier( te_identifier );
    }

    te_next_time->Release();
    te_func_num->Release();
    te_rate->Release();
    te_identifier->Release();
}

void Critter::ContinueTimeEvents( int offs_time )
{
    ScriptArray* te_next_time = GetTE_NextTime();
    if( te_next_time->GetSize() > 0 )
    {
        for( uint i = 0, j = te_next_time->GetSize(); i < j; i++ )
            *(uint*) te_next_time->At( i ) += offs_time;

        SetTE_NextTime( te_next_time );
    }
    te_next_time->Release();
}

/************************************************************************/
/* Client                                                               */
/************************************************************************/

#if !defined ( USE_LIBEVENT ) || defined ( LIBEVENT_TIMEOUTS_WORKAROUND )
Client::SendCallback Client::SendData = nullptr;
#endif

Client::Client( ProtoCritter* proto ): Critter( 0, EntityType::Client, proto )
{
    ZstrmInit = false;
    Access = ACCESS_DEFAULT;
    pingOk = true;
    LanguageMsg = 0;
    GameState = STATE_NONE;
    IsDisconnected = false;
    DisconnectTick = 0;
    DisableZlib = false;
    LastSendCraftTick = 0;
    LastSendEntrancesTick = 0;
    LastSendEntrancesLocId = 0;
    LastSendedMapTick = 0;
    RadioMessageSended = 0;
    UpdateFileIndex = -1;
    UpdateFilePortion = 0;

    CritterIsNpc = false;
    MEMORY_PROCESS( MEMORY_CLIENT, sizeof( Client ) + sizeof( GlobalMapGroup ) + 40 + sizeof( Item ) * 2 );

    SETFLAG( Flags, FCRIT_PLAYER );
    Sock = INVALID_SOCKET;
    memzero( Name, sizeof( Name ) );
    Str::Copy( Name, "err" );
    pingNextTick = Timer::FastTick() + PING_CLIENT_LIFE_TIME;
    Talk.Clear();
    talkNextTick = Timer::GameTick() + PROCESS_TALK_TICK;
    LastActivityTime = Timer::FastTick();
    LastSay[ 0 ] = 0;
    LastSayEqualCount = 0;
    memzero( UID, sizeof( UID ) );

    #if defined ( USE_LIBEVENT )
    NetIOArgPtr = nullptr;
    #else // IOCP
    MEMORY_PROCESS( MEMORY_CLIENT, WSA_BUF_SIZE * 2 );
    NetIOIn = new NetIOArg();
    memzero( &NetIOIn->OV, sizeof( NetIOIn->OV ) );
    NetIOIn->PClient = this;
    NetIOIn->Buffer.buf = new char[ WSA_BUF_SIZE ];
    NetIOIn->Buffer.len = WSA_BUF_SIZE;
    NetIOIn->Operation = WSAOP_RECV;
    NetIOIn->Flags = 0;
    NetIOIn->Bytes = 0;
    NetIOOut = new NetIOArg();
    memzero( &NetIOOut->OV, sizeof( NetIOOut->OV ) );
    NetIOOut->PClient = this;
    NetIOOut->Buffer.buf = new char[ WSA_BUF_SIZE ];
    NetIOOut->Buffer.len = WSA_BUF_SIZE;
    NetIOOut->Operation = WSAOP_FREE;
    NetIOOut->Flags = 0;
    NetIOOut->Bytes = 0;
    #endif
}

Client::~Client()
{
    MEMORY_PROCESS( MEMORY_CLIENT, -(int) ( sizeof( Client ) + sizeof( GlobalMapGroup ) + 40 + sizeof( Item ) * 2 ) );
    if( ZstrmInit )
    {
        deflateEnd( &Zstrm );
        ZstrmInit = false;
    }

    #if defined ( USE_LIBEVENT )
    SAFEDEL( NetIOArgPtr );
    #else // IOCP
    MEMORY_PROCESS( MEMORY_CLIENT, -(int) ( WSA_BUF_SIZE * 2 ) );
    if( NetIOIn )
    {
        SAFEDELA( NetIOIn->Buffer.buf );
        SAFEDEL( NetIOIn );
    }
    if( NetIOOut )
    {
        SAFEDELA( NetIOOut->Buffer.buf );
        SAFEDEL( NetIOOut );
    }
    #endif
}

void Client::Shutdown()
{
    if( Sock == INVALID_SOCKET )
        return;

    #if defined ( USE_LIBEVENT )
    # if defined ( LIBEVENT_TIMEOUTS_WORKAROUND )
    NetIOArgPtr->BEVLocker.Lock();
    bufferevent_free( NetIOArgPtr->BEV );
    NetIOArgPtr->BEV = nullptr;
    NetIOArgPtr->BEVLocker.Unlock();
    # else
    bufferevent_free( NetIOArgPtr->BEV );
    # endif

    shutdown( Sock, SD_BOTH );
    closesocket( Sock );
    Sock = INVALID_SOCKET;

    Release();
    #else
    NetIOIn->Locker.Lock();
    NetIOOut->Locker.Lock();
    shutdown( Sock, SD_BOTH );
    closesocket( Sock );
    Sock = INVALID_SOCKET;
    NetIOOut->Locker.Unlock();
    NetIOIn->Locker.Unlock();
    #endif
}

uint Client::GetIp()
{
    return From.sin_addr.s_addr;
}

const char* Client::GetIpStr()
{
    return inet_ntoa( From.sin_addr );
}

ushort Client::GetPort()
{
    return From.sin_port;
}

bool Client::IsOnline()
{
    return !IsDisconnected;
}

bool Client::IsOffline()
{
    return IsDisconnected;
}

void Client::Disconnect()
{
    IsDisconnected = true;
    if( !DisconnectTick )
        DisconnectTick = Timer::FastTick();
}

void Client::RemoveFromGame()
{
    CanBeRemoved = true;
}

uint Client::GetOfflineTime()
{
    return Timer::FastTick() - DisconnectTick;
}

const char* Client::GetBinPassHash()
{
    static THREAD char pass_hash[ PASS_HASH_SIZE ];
    ScriptString*      str = GetPassHash();
    RUNTIME_ASSERT( str->length() == PASS_HASH_SIZE * 2 );
    for( asUINT i = 0, j = str->length(); i < j; i += 2 )
        pass_hash[ i / 2 ] = Str::StrToHex( str->c_str() + i );
    str->Release();
    return pass_hash;
}

void Client::SetBinPassHash( const char* pass_hash )
{
    ScriptString* str = ScriptString::Create();
    str->rawResize( PASS_HASH_SIZE * 2 );
    for( uint i = 0; i < PASS_HASH_SIZE; i++ )
        Str::HexToStr( (uchar) pass_hash[ i ], (char*) str->c_str() + i * 2 );
    SetPassHash( str );
    str->Release();
}

bool Client::IsToPing()
{
    return GameState == STATE_PLAYING && Timer::FastTick() >= pingNextTick && !IS_TIMEOUT( GetTimeoutTransfer() ) && !Singleplayer;
}

void Client::PingClient()
{
    if( !pingOk )
    {
        Disconnect();
        Bout.LockReset();
        return;
    }

    BOUT_BEGIN( this );
    Bout << NETMSG_PING;
    Bout << (uchar) PING_CLIENT;
    BOUT_END( this );

    pingNextTick = Timer::FastTick() + PING_CLIENT_LIFE_TIME;
    pingOk = false;
}

void Client::PingOk( uint next_ping )
{
    pingOk = true;
    pingNextTick = Timer::FastTick() + next_ping;
}

void Client::Send_AddCritter( Critter* cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    bool is_npc = cr->IsNpc();
    uint msg = ( is_npc ? NETMSG_ADD_NPC : NETMSG_ADD_PLAYER );
    uint msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( uint ) + sizeof( uint ) + sizeof( ushort ) * 2 +
                   sizeof( uchar ) + sizeof( int ) + sizeof( uint ) * 6 + sizeof( uint ) + sizeof( short ) +
                   ( is_npc ? sizeof( hash ) : UTF8_BUF_SIZE( MAX_NAME ) );

    PUCharVec* data;
    UIntVec*   data_sizes;
    uint       whole_data_size = cr->Props.StoreData( FLAG( cr->Flags, FCRIT_CHOSEN ) ? true : false, &data, &data_sizes );
    msg_len += sizeof( ushort ) + whole_data_size;

    BOUT_BEGIN( this );
    Bout << msg;
    Bout << msg_len;
    Bout << cr->GetId();
    Bout << cr->GetCrType();
    Bout << cr->GetHexX();
    Bout << cr->GetHexY();
    Bout << cr->GetDir();
    Bout << cr->GetCond();
    Bout << cr->GetAnim1Life();
    Bout << cr->GetAnim1Knockout();
    Bout << cr->GetAnim1Dead();
    Bout << cr->GetAnim2Life();
    Bout << cr->GetAnim2Knockout();
    Bout << cr->GetAnim2Dead();
    Bout << cr->Flags;
    Bout << cr->GetMultihexBase();

    if( is_npc )
    {
        Npc* npc = (Npc*) cr;
        Bout << npc->GetProtoId();
    }
    else
    {
        Client* cl = (Client*) cr;
        Bout.Push( cl->Name, UTF8_BUF_SIZE( MAX_NAME ) );
    }

    NET_WRITE_PROPERTIES( Bout, data, data_sizes );

    BOUT_END( this );

    if( cr != this )
        Send_MoveItem( cr, nullptr, ACTION_REFRESH, 0 );
}

void Client::Send_RemoveCritter( Critter* cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_REMOVE_CRITTER;
    Bout << cr->GetId();
    BOUT_END( this );
}

void Client::Send_LoadMap( Map* map )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    Location* loc = nullptr;
    hash      pid_map = 0;
    hash      pid_loc = 0;
    uchar     map_index_in_loc = 0;
    int       map_time = -1;
    uchar     map_rain = 0;
    hash      hash_tiles = 0;
    hash      hash_scen = 0;

    if( !map )
        map = MapMngr.GetMap( GetMapId(), false );
    if( map )
    {
        loc = map->GetLocation( false );
        pid_map = map->GetProtoId();
        pid_loc = loc->GetProtoId();
        map_index_in_loc = (uchar) loc->GetMapIndex( pid_map );
        map_time = map->GetCurDayTime();
        map_rain = map->GetRainCapacity();
        hash_tiles = map->GetProtoMap()->HashTiles;
        hash_scen = map->GetProtoMap()->HashScen;
    }

    uint       msg_len = sizeof( uint ) + sizeof( uint ) + sizeof( hash ) * 2 + sizeof( uchar ) + sizeof( int ) + sizeof( uchar ) + sizeof( hash ) * 2;
    PUCharVec* map_data;
    UIntVec*   map_data_sizes;
    PUCharVec* loc_data;
    UIntVec*   loc_data_sizes;
    if( map )
    {
        uint map_whole_data_size = map->Props.StoreData( false, &map_data, &map_data_sizes );
        uint loc_whole_data_size = loc->Props.StoreData( false, &loc_data, &loc_data_sizes );
        msg_len += sizeof( ushort ) + map_whole_data_size + sizeof( ushort ) + loc_whole_data_size;
    }

    BOUT_BEGIN( this );
    Bout << NETMSG_LOADMAP;
    Bout << msg_len;
    Bout << pid_map;
    Bout << pid_loc;
    Bout << map_index_in_loc;
    Bout << map_time;
    Bout << map_rain;
    Bout << hash_tiles;
    Bout << hash_scen;
    if( map )
    {
        NET_WRITE_PROPERTIES( Bout, map_data, map_data_sizes );
        NET_WRITE_PROPERTIES( Bout, loc_data, loc_data_sizes );
    }
    BOUT_END( this );

    GameState = STATE_TRANSFERRING;
}

void Client::Send_Property( NetProperty::Type type, Property* prop, Entity* entity )
{
    RUNTIME_ASSERT( entity );

    if( IsSendDisabled() || IsOffline() )
        return;

    uint additional_args = 0;
    switch( type )
    {
    case NetProperty::Critter:
        additional_args = 1;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        break;
    case NetProperty::CritterItem:
        additional_args = 2;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        break;
    default:
        break;
    }

    uint  data_size;
    void* data = prop->GetRawData( entity, data_size );

    bool  is_pod = prop->IsPOD();
    if( is_pod )
    {
        BOUT_BEGIN( this );
        Bout << NETMSG_POD_PROPERTY( data_size, additional_args );
    }
    else
    {
        uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( char ) + additional_args * sizeof( uint ) + sizeof( ushort ) + data_size;
        BOUT_BEGIN( this );
        Bout << NETMSG_COMPLEX_PROPERTY;
        Bout << msg_len;
    }

    Bout << (char) type;

    switch( type )
    {
    case NetProperty::CritterItem:
        Bout << ( (Item*) entity )->GetCritId();
        Bout << entity->Id;
        break;
    case NetProperty::Critter:
        Bout << entity->Id;
        break;
    case NetProperty::MapItem:
        Bout << entity->Id;
        break;
    case NetProperty::ChosenItem:
        Bout << entity->Id;
        break;
    default:
        break;
    }

    if( is_pod )
    {
        Bout << (ushort) prop->GetRegIndex();
        Bout.Push( (char*) data, data_size );
        BOUT_END( this );
    }
    else
    {
        Bout << (ushort) prop->GetRegIndex();
        if( data_size )
            Bout.Push( (char*) data, data_size );
        BOUT_END( this );
    }
}

void Client::Send_Move( Critter* from_cr, uint move_params )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_MOVE;
    Bout << from_cr->GetId();
    Bout << move_params;
    Bout << from_cr->GetHexX();
    Bout << from_cr->GetHexY();
    BOUT_END( this );
}

void Client::Send_Dir( Critter* from_cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_DIR;
    Bout << from_cr->GetId();
    Bout << from_cr->GetDir();
    BOUT_END( this );
}

void Client::Send_Action( Critter* from_cr, int action, int action_ext, Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    Send_XY( from_cr );

    if( item )
        Send_SomeItem( item );

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_ACTION;
    Bout << from_cr->GetId();
    Bout << action;
    Bout << action_ext;
    Bout << (bool) ( item ? true : false );
    BOUT_END( this );
}

void Client::Send_Knockout( Critter* from_cr, uint anim2begin, uint anim2idle, ushort knock_hx, ushort knock_hy )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    Send_XY( from_cr );

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_KNOCKOUT;
    Bout << from_cr->GetId();
    Bout << anim2begin;
    Bout << anim2idle;
    Bout << knock_hx;
    Bout << knock_hy;
    BOUT_END( this );
}

void Client::Send_MoveItem( Critter* from_cr, Item* item, uchar action, uchar prev_slot )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    if( item )
        Send_SomeItem( item );

    uint     msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uint ) + sizeof( action ) + sizeof( prev_slot ) + sizeof( bool );

    ItemVec& inv = from_cr->GetInventory();
    ItemVec  items;
    items.reserve( inv.size() );
    for( auto it = inv.begin(), end = inv.end(); it != end; ++it )
    {
        Item* item_ = *it;
        uchar slot = item_->GetCritSlot();
        if( SlotEnabled[ slot ] && SlotDataSendEnabled[ slot ] )
            items.push_back( item_ );
    }

    msg_len += sizeof( ushort );
    vector< PUCharVec* > items_data( items.size() );
    vector< UIntVec* >   items_data_sizes( items.size() );
    for( size_t i = 0, j = items.size(); i < j; i++ )
    {
        uint whole_data_size = items[ i ]->Props.StoreData( false, &items_data[ i ], &items_data_sizes[ i ] );
        msg_len += sizeof( uchar ) + sizeof( uint ) + sizeof( hash ) + sizeof( ushort ) + whole_data_size;
    }

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_MOVE_ITEM;
    Bout << msg_len;
    Bout << from_cr->GetId();
    Bout << action;
    Bout << prev_slot;
    Bout << (bool) ( item ? true : false );
    Bout << (ushort) items.size();
    for( size_t i = 0, j = items.size(); i < j; i++ )
    {
        Item* item_ = items[ i ];
        Bout << item_->GetCritSlot();
        Bout << item_->GetId();
        Bout << item_->GetProtoId();
        NET_WRITE_PROPERTIES( Bout, items_data[ i ], items_data_sizes[ i ] );
    }
    BOUT_END( this );
}

void Client::Send_Animate( Critter* from_cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( clear_sequence )
        Send_XY( from_cr );
    if( item )
        Send_SomeItem( item );

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_ANIMATE;
    Bout << from_cr->GetId();
    Bout << anim1;
    Bout << anim2;
    Bout << (bool) ( item ? true : false );
    Bout << clear_sequence;
    Bout << delay_play;
    BOUT_END( this );
}

void Client::Send_SetAnims( Critter* from_cr, int cond, uint anim1, uint anim2 )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_SET_ANIMS;
    Bout << from_cr->GetId();
    Bout << cond;
    Bout << anim1;
    Bout << anim2;
    BOUT_END( this );
}

void Client::Send_AddItemOnMap( Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    bool       is_added = item->ViewPlaceOnMap;
    uint       msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uint ) + sizeof( hash ) + sizeof( ushort ) * 2 + sizeof( bool );

    PUCharVec* data;
    UIntVec*   data_sizes;
    uint       whole_data_size = item->Props.StoreData( false, &data, &data_sizes );
    msg_len += sizeof( ushort ) + whole_data_size;

    BOUT_BEGIN( this );
    Bout << NETMSG_ADD_ITEM_ON_MAP;
    Bout << msg_len;
    Bout << item->GetId();
    Bout << item->GetProtoId();
    Bout << item->GetHexX();
    Bout << item->GetHexY();
    Bout << is_added;
    NET_WRITE_PROPERTIES( Bout, data, data_sizes );
    BOUT_END( this );
}

void Client::Send_EraseItemFromMap( Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_ERASE_ITEM_FROM_MAP;
    Bout << item->GetId();
    Bout << item->ViewPlaceOnMap;
    BOUT_END( this );
}

void Client::Send_AnimateItem( Item* item, uchar from_frm, uchar to_frm )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_ANIMATE_ITEM;
    Bout << item->GetId();
    Bout << from_frm;
    Bout << to_frm;
    BOUT_END( this );
}

void Client::Send_AddItem( Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( item->GetIsHidden() )
        return;

    uint       msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uint ) + sizeof( hash ) + sizeof( uchar );
    PUCharVec* data;
    UIntVec*   data_sizes;
    uint       whole_data_size = item->Props.StoreData( true, &data, &data_sizes );
    msg_len += sizeof( ushort ) + whole_data_size;

    BOUT_BEGIN( this );
    Bout << NETMSG_ADD_ITEM;
    Bout << msg_len;
    Bout << item->GetId();
    Bout << item->GetProtoId();
    Bout << item->GetCritSlot();
    NET_WRITE_PROPERTIES( Bout, data, data_sizes );
    BOUT_END( this );
}

void Client::Send_EraseItem( Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_REMOVE_ITEM;
    Bout << item->GetId();
    BOUT_END( this );
}

void Client::Send_ContainerInfo()
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uchar transfer_type = TRANSFER_CLOSE;
    uint  msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( transfer_type );

    BOUT_BEGIN( this );
    Bout << NETMSG_CONTAINER_INFO;
    Bout << msg_len;
    Bout << transfer_type;
    BOUT_END( this );

    AccessContainerId = 0;
}

void Client::Send_ContainerInfo( Item* item_cont, uchar transfer_type, bool open_screen )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( item_cont->GetType() != ITEM_TYPE_CONTAINER )
        return;

    uint                 msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uchar ) + sizeof( uint ) + sizeof( uint ) + sizeof( max_t ) + sizeof( uint );
    ItemVec              items;
    item_cont->ContGetAllItems( items, true, true );
    vector< PUCharVec* > items_data( items.size() );
    vector< UIntVec* >   items_data_sizes( items.size() );
    for( size_t i = 0, j = items.size(); i < j; i++ )
    {
        uint whole_data_size = items[ i ]->Props.StoreData( false, &items_data[ i ], &items_data_sizes[ i ] );
        msg_len += sizeof( uint ) + sizeof( hash ) + sizeof( ushort ) + whole_data_size;
    }
    if( open_screen )
        SETFLAG( transfer_type, 0x80 );

    BOUT_BEGIN( this );
    Bout << NETMSG_CONTAINER_INFO;
    Bout << msg_len;
    Bout << transfer_type;
    Bout << uint( 0 );
    Bout << item_cont->GetId();
    Bout << (max_t) item_cont->GetProtoId();
    Bout << uint( items.size() );

    for( size_t i = 0, j = items.size(); i < j; i++ )
    {
        Item* item = items[ i ];
        Bout << item->GetId();
        Bout << item->GetProtoId();
        NET_WRITE_PROPERTIES( Bout, items_data[ i ], items_data_sizes[ i ] );
    }
    BOUT_END( this );

    AccessContainerId = item_cont->GetId();
}

void Client::Send_ContainerInfo( Critter* cr_cont, uchar transfer_type, bool open_screen )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint    msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uchar ) + sizeof( uint ) + sizeof( uint ) + sizeof( max_t ) + sizeof( uint );
    ItemVec items;
    cr_cont->GetInvItems( items, transfer_type, true );
    ushort  barter_k = 0;
    if( transfer_type == TRANSFER_CRIT_BARTER )
    {
        int cr_cont_k = cr_cont->GetBarterCoefficient();
        int k = GetBarterCoefficient();
        if( cr_cont_k > k )
            barter_k = cr_cont_k - k;
        barter_k = CLAMP( barter_k, 5, 95 );
        if( cr_cont->GetFreeBarterPlayer() == GetId() )
            barter_k = 0;
    }

    if( open_screen )
        SETFLAG( transfer_type, 0x80 );

    vector< PUCharVec* > items_data( items.size() );
    vector< UIntVec* >   items_data_sizes( items.size() );
    for( size_t i = 0, j = items.size(); i < j; i++ )
    {
        uint whole_data_size = items[ i ]->Props.StoreData( false, &items_data[ i ], &items_data_sizes[ i ] );
        msg_len += sizeof( uint ) + sizeof( hash ) + sizeof( ushort ) + whole_data_size;
    }

    BOUT_BEGIN( this );
    Bout << NETMSG_CONTAINER_INFO;
    Bout << msg_len;
    Bout << transfer_type;
    Bout << (uint) Talk.TalkTime;
    Bout << cr_cont->GetId();
    Bout << (max_t) barter_k;
    Bout << (uint) items.size();

    for( size_t i = 0, j = items.size(); i < j; i++ )
    {
        Item* item = items[ i ];
        Bout << item->GetId();
        Bout << item->GetProtoId();
        NET_WRITE_PROPERTIES( Bout, items_data[ i ], items_data_sizes[ i ] );
    }
    BOUT_END( this );

    AccessContainerId = cr_cont->GetId();
}

#define SEND_LOCATION_SIZE    ( sizeof( uint ) + sizeof( hash ) + sizeof( ushort ) * 2 + sizeof( ushort ) + sizeof( uint ) + sizeof( uchar ) )
void Client::Send_GlobalInfo( uchar info_flags )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( LockMapTransfers )
        return;

    Item*        car = ( FLAG( info_flags, GM_INFO_GROUP_PARAM ) ? GroupMove->GetCar() : nullptr );
    ScriptArray* known_locs = GetKnownLocations();

    // Calculate length of message
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( info_flags );

    ushort loc_count = (ushort) known_locs->GetSize();
    if( FLAG( info_flags, GM_INFO_LOCATIONS ) )
        msg_len += sizeof( loc_count ) + SEND_LOCATION_SIZE * loc_count;

    if( FLAG( info_flags, GM_INFO_GROUP_PARAM ) )
        msg_len += sizeof( ushort ) * 4 + sizeof( uint ) + sizeof( bool ) + sizeof( uint );

    if( FLAG( info_flags, GM_INFO_ZONES_FOG ) )
        msg_len += GM_ZONES_FOG_SIZE;

    // Parse message
    BOUT_BEGIN( this );
    Bout << NETMSG_GLOBAL_INFO;
    Bout << msg_len;
    Bout << info_flags;

    if( FLAG( info_flags, GM_INFO_LOCATIONS ) )
    {
        Bout << loc_count;

        char empty_loc[ SEND_LOCATION_SIZE ] = { 0, 0, 0, 0 };
        for( ushort i = 0; i < loc_count;)
        {
            uint      loc_id = *(uint*) known_locs->At( i );
            Location* loc = MapMngr.GetLocation( loc_id );
            if( loc && !loc->GetToGarbage() )
            {
                i++;
                Bout << loc_id;
                Bout << loc->GetProtoId();
                Bout << loc->GetWorldX();
                Bout << loc->GetWorldY();
                Bout << loc->GetRadius();
                Bout << loc->GetColor();
                uchar count = 0;
                if( loc->IsNonEmptyMapEntrances() )
                {
                    ScriptArray* map_entrances = loc->GetMapEntrances();
                    count = (uchar) ( map_entrances->GetSize() / 2 );
                    map_entrances->Release();
                }
                Bout << count;
            }
            else
            {
                loc_count--;
                EraseKnownLoc( loc_id );
                Bout.Push( empty_loc, sizeof( empty_loc ) );
            }
        }
    }
    SAFEREL( known_locs );

    if( FLAG( info_flags, GM_INFO_GROUP_PARAM ) )
    {
        ushort cur_x = (ushort) GroupMove->CurX;
        ushort cur_y = (ushort) GroupMove->CurY;
        ushort to_x = (ushort) GroupMove->ToX;
        ushort to_y = (ushort) GroupMove->ToY;
        uint   speed = (uint) ( GroupMove->Speed * 1000000.0f );
        bool   wait = ( GroupMove->EncounterDescriptor ? true : false );

        Bout << cur_x;
        Bout << cur_y;
        Bout << to_x;
        Bout << to_y;
        Bout << speed;
        Bout << wait;

        if( car )
            Bout << car->GetCritId();
        else
            Bout << (uint) 0;
    }

    if( FLAG( info_flags, GM_INFO_ZONES_FOG ) )
    {
        ScriptArray* gmap_fog = GetGlobalMapFog();
        if( gmap_fog->GetSize() != GM_ZONES_FOG_SIZE )
            gmap_fog->Resize( GM_ZONES_FOG_SIZE );
        Bout.Push( (char*) gmap_fog->At( 0 ), GM_ZONES_FOG_SIZE );
        gmap_fog->Release();
    }
    BOUT_END( this );

    if( FLAG( info_flags, GM_INFO_CRITTERS ) )
        ProcessVisibleCritters();
    if( FLAG( info_flags, GM_INFO_GROUP_PARAM ) && car )
        Send_AddItemOnMap( car );
}

void Client::Send_GlobalLocation( Location* loc, bool add )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uchar info_flags = GM_INFO_LOCATION;
    uint  msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( info_flags ) + sizeof( add ) + SEND_LOCATION_SIZE;

    BOUT_BEGIN( this );
    Bout << NETMSG_GLOBAL_INFO;
    Bout << msg_len;
    Bout << info_flags;
    Bout << add;
    Bout << loc->GetId();
    Bout << loc->GetProtoId();
    Bout << loc->GetWorldX();
    Bout << loc->GetWorldY();
    Bout << loc->GetRadius();
    Bout << loc->GetColor();
    uchar count = 0;
    if( loc->IsNonEmptyMapEntrances() )
    {
        ScriptArray* map_entrances = loc->GetMapEntrances();
        count = (uchar) ( map_entrances->GetSize() / 2 );
        map_entrances->Release();
    }
    Bout << count;
    BOUT_END( this );
}

void Client::Send_GlobalMapFog( ushort zx, ushort zy, uchar fog )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uchar info_flags = GM_INFO_FOG;
    uint  msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( info_flags ) + sizeof( zx ) + sizeof( zy ) + sizeof( fog );

    BOUT_BEGIN( this );
    Bout << NETMSG_GLOBAL_INFO;
    Bout << msg_len;
    Bout << info_flags;
    Bout << zx;
    Bout << zy;
    Bout << fog;
    BOUT_END( this );
}

void Client::Send_XY( Critter* cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_XY;
    Bout << cr->GetId();
    Bout << cr->GetHexX();
    Bout << cr->GetHexY();
    Bout << cr->GetDir();
    BOUT_END( this );
}

void Client::Send_AllProperties()
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint       msg_len = sizeof( uint ) + sizeof( uint );

    PUCharVec* data;
    UIntVec*   data_sizes;
    uint       whole_data_size = Props.StoreData( true, &data, &data_sizes );
    msg_len += sizeof( ushort ) + whole_data_size;

    BOUT_BEGIN( this );
    Bout << NETMSG_ALL_PROPERTIES;
    Bout << msg_len;
    NET_WRITE_PROPERTIES( Bout, data, data_sizes );
    BOUT_END( this );
}

void Client::Send_CustomCommand( Critter* cr, ushort cmd, int val )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_CUSTOM_COMMAND;
    Bout << cr->GetId();
    Bout << cmd;
    Bout << val;
    BOUT_END( this );
}

void Client::Send_Talk()
{
    if( IsSendDisabled() || IsOffline() )
        return;

    bool  close = ( Talk.TalkType == TALK_NONE );
    uchar is_npc = ( Talk.TalkType == TALK_WITH_NPC );
    max_t talk_id = ( is_npc ? Talk.TalkNpc : Talk.DialogPackId );
    uint  msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( is_npc ) + sizeof( talk_id ) + sizeof( uchar );

    BOUT_BEGIN( this );
    Bout << NETMSG_TALK_NPC;

    if( close )
    {
        Bout << msg_len;
        Bout << is_npc;
        Bout << talk_id;
        Bout << uchar( 0 );
    }
    else
    {
        uchar all_answers = (uchar) Talk.CurDialog.Answers.size();
        msg_len += sizeof( uint ) + sizeof( uint ) * all_answers + sizeof( uint ) + sizeof( ushort ) + (uint) Talk.Lexems.length();

        Bout << msg_len;
        Bout << is_npc;
        Bout << talk_id;
        Bout << all_answers;
        Bout << (ushort) Talk.Lexems.length();                                  // Lexems length
        if( Talk.Lexems.length() )
            Bout.Push( Talk.Lexems.c_str(), (uint) Talk.Lexems.length() );      // Lexems string
        Bout << Talk.CurDialog.TextId;                                          // Main text_id
        for( auto it = Talk.CurDialog.Answers.begin(), end = Talk.CurDialog.Answers.end(); it != end; ++it )
            Bout << it->TextId;                                                 // Answers text_id
        Bout << uint( Talk.TalkTime - ( Timer::GameTick() - Talk.StartTick ) ); // Talk time
    }
    BOUT_END( this );
}

void Client::Send_GameInfo( Map* map )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    if( map )
        map->Lock();

    int          time = ( map ? map->GetCurDayTime() : -1 );
    uchar        rain = ( map ? map->GetRainCapacity() : 0 );
    bool         turn_based = ( map ? map->IsTurnBasedOn : false );
    bool         no_log_out = ( map ? map->GetIsNoLogOut() : true );

    int          day_time[ 4 ];
    uchar        day_color[ 12 ];
    ScriptArray* day_time_arr = ( map ? map->GetDayTime() : nullptr );
    ScriptArray* day_color_arr = ( map ? map->GetDayColor() : nullptr );
    RUNTIME_ASSERT( !day_time_arr || day_time_arr->GetSize() == 0 || day_time_arr->GetSize() == 4 );
    RUNTIME_ASSERT( !day_color_arr || day_color_arr->GetSize() == 0 || day_color_arr->GetSize() == 12 );
    if( day_time_arr && day_time_arr->GetSize() > 0 )
        memcpy( day_time, day_time_arr->At( 0 ), sizeof( day_time ) );
    else
        memzero( day_time, sizeof( day_time ) );
    if( day_color_arr && day_color_arr->GetSize() > 0 )
        memcpy( day_color, day_color_arr->At( 0 ), sizeof( day_color ) );
    else
        memzero( day_color, sizeof( day_color ) );
    if( day_time_arr )
        day_time_arr->Release();
    if( day_color_arr )
        day_color_arr->Release();

    if( map )
        map->Unlock();

    BOUT_BEGIN( this );
    Bout << NETMSG_GAME_INFO;
    Bout << GameOpt.YearStart;
    Bout << GameOpt.Year;
    Bout << GameOpt.Month;
    Bout << GameOpt.Day;
    Bout << GameOpt.Hour;
    Bout << GameOpt.Minute;
    Bout << GameOpt.Second;
    Bout << GameOpt.TimeMultiplier;
    Bout << time;
    Bout << rain;
    Bout << turn_based;
    Bout << no_log_out;
    Bout.Push( (const char*) day_time, sizeof( day_time ) );
    Bout.Push( (const char*) day_color, sizeof( day_color ) );
    BOUT_END( this );
}

void Client::Send_Text( Critter* from_cr, const char* s_str, uchar how_say )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !s_str || !s_str[ 0 ] )
        return;
    ushort s_len = Str::Length( s_str );
    uint   from_id = ( from_cr ? from_cr->GetId() : 0 );
    Send_TextEx( from_id, s_str, s_len, how_say, false );
}

void Client::Send_TextEx( uint from_id, const char* s_str, ushort str_len, uchar how_say, bool unsafe_text )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( from_id ) + sizeof( how_say ) +
                   sizeof( unsafe_text ) + sizeof( str_len ) + str_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_CRITTER_TEXT;
    Bout << msg_len;
    Bout << from_id;
    Bout << how_say;
    Bout << unsafe_text;
    Bout << str_len;
    Bout.Push( s_str, str_len );
    BOUT_END( this );
}

void Client::Send_TextMsg( Critter* from_cr, uint num_str, uchar how_say, ushort num_msg )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !num_str )
        return;
    uint from_id = ( from_cr ? from_cr->GetId() : 0 );

    BOUT_BEGIN( this );
    Bout << NETMSG_MSG;
    Bout << from_id;
    Bout << how_say;
    Bout << num_msg;
    Bout << num_str;
    BOUT_END( this );
}

void Client::Send_TextMsg( uint from_id, uint num_str, uchar how_say, ushort num_msg )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !num_str )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_MSG;
    Bout << from_id;
    Bout << how_say;
    Bout << num_msg;
    Bout << num_str;
    BOUT_END( this );
}

void Client::Send_TextMsgLex( Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !num_str )
        return;

    ushort lex_len = Str::Length( lexems );
    if( !lex_len || lex_len > MAX_DLG_LEXEMS_TEXT )
    {
        Send_TextMsg( from_cr, num_str, how_say, num_msg );
        return;
    }

    uint from_id = ( from_cr ? from_cr->GetId() : 0 );
    uint msg_len = NETMSG_MSG_SIZE + sizeof( lex_len ) + lex_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_MSG_LEX;
    Bout << msg_len;
    Bout << from_id;
    Bout << how_say;
    Bout << num_msg;
    Bout << num_str;
    Bout << lex_len;
    Bout.Push( lexems, lex_len );
    BOUT_END( this );
}

void Client::Send_TextMsgLex( uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !num_str )
        return;

    ushort lex_len = Str::Length( lexems );
    if( !lex_len || lex_len > MAX_DLG_LEXEMS_TEXT )
    {
        Send_TextMsg( from_id, num_str, how_say, num_msg );
        return;
    }

    uint msg_len = NETMSG_MSG_SIZE + sizeof( lex_len ) + lex_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_MSG_LEX;
    Bout << msg_len;
    Bout << from_id;
    Bout << how_say;
    Bout << num_msg;
    Bout << num_str;
    Bout << lex_len;
    Bout.Push( lexems, lex_len );
    BOUT_END( this );
}

void Client::Send_MapText( ushort hx, ushort hy, uint color, const char* text, ushort text_len, bool unsafe_text )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( hx ) + sizeof( hy ) + sizeof( color ) +
                   sizeof( text_len ) + text_len + sizeof( unsafe_text );

    BOUT_BEGIN( this );
    Bout << NETMSG_MAP_TEXT;
    Bout << msg_len;
    Bout << hx;
    Bout << hy;
    Bout << color;
    Bout << text_len;
    Bout.Push( text, text_len );
    Bout << unsafe_text;
    BOUT_END( this );
}

void Client::Send_MapTextMsg( ushort hx, ushort hy, uint color, ushort num_msg, uint num_str )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_MAP_TEXT_MSG;
    Bout << hx;
    Bout << hy;
    Bout << color;
    Bout << num_msg;
    Bout << num_str;
    BOUT_END( this );
}

void Client::Send_MapTextMsgLex( ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, const char* lexems, ushort lexems_len )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( ushort ) * 2 + sizeof( uint ) + sizeof( ushort ) + sizeof( uint ) + sizeof( lexems_len ) + lexems_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_MAP_TEXT_MSG_LEX;
    Bout << msg_len;
    Bout << hx;
    Bout << hy;
    Bout << color;
    Bout << num_msg;
    Bout << num_str;
    Bout << lexems_len;
    if( lexems_len )
        Bout.Push( lexems, lexems_len );
    BOUT_END( this );
}

void Client::Send_CombatResult( uint* combat_res, uint len )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !combat_res || len > GameOpt.FloodSize / sizeof( uint ) )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( len ) + len * sizeof( uint );

    BOUT_BEGIN( this );
    Bout << NETMSG_COMBAT_RESULTS;
    Bout << msg_len;
    Bout << len;
    if( len )
        Bout.Push( (char*) combat_res, len * sizeof( uint ) );
    BOUT_END( this );
}

void Client::Send_HoloInfo( bool clear, ushort offset, ushort count )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    ScriptArray* holo_info = GetHoloInfo();
    count = ( count ? count : holo_info->GetSize() );
    uint         msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( clear ) + sizeof( offset ) + sizeof( count ) + count * sizeof( uint );

    BOUT_BEGIN( this );
    Bout << NETMSG_HOLO_INFO;
    Bout << msg_len;
    Bout << clear;
    Bout << offset;
    Bout << count;
    if( count )
        Bout.Push( (char*) holo_info->At( offset ), count * sizeof( uint ) );
    BOUT_END( this );

    SAFEREL( holo_info );
}

void Client::Send_UserHoloStr( uint str_num, const char* text, ushort text_len )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( str_num ) + sizeof( text_len ) + text_len;

    BOUT_BEGIN( this );
    Bout << NETMSG_USER_HOLO_STR;
    Bout << msg_len;
    Bout << str_num;
    Bout << text_len;
    Bout.Push( text, text_len );
    BOUT_END( this );
}

void Client::Send_AutomapsInfo( void* locs_vec, Location* loc )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    if( locs_vec )
    {
        LocVec* locs = (LocVec*) locs_vec;
        uint    msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( bool ) + sizeof( ushort );
        for( uint i = 0, j = (uint) locs->size(); i < j; i++ )
        {
            Location* loc_ = ( *locs )[ i ];
            msg_len += sizeof( uint ) + sizeof( hash ) + sizeof( ushort );
            if( loc_->IsNonEmptyAutomaps() )
            {
                ScriptArray* automaps = loc_->GetAutomaps();
                msg_len += ( sizeof( hash ) + sizeof( uchar ) ) * (uint) automaps->GetSize();
                automaps->Release();
            }
        }

        BOUT_BEGIN( this );
        Bout << NETMSG_AUTOMAPS_INFO;
        Bout << msg_len;
        Bout << (bool) true;        // Clear list
        Bout << (ushort) locs->size();
        for( uint i = 0, j = (uint) locs->size(); i < j; i++ )
        {
            Location* loc_ = ( *locs )[ i ];
            Bout << loc_->GetId();
            Bout << loc_->GetProtoId();
            if( loc_->IsNonEmptyAutomaps() )
            {
                ScriptArray* automaps = loc_->GetAutomaps();
                Bout << (ushort) automaps->GetSize();
                for( uint k = 0, l = (uint) automaps->GetSize(); k < l; k++ )
                {
                    hash pid = *(hash*) automaps->At( k );
                    Bout << pid;
                    Bout << (uchar) loc_->GetMapIndex( pid );
                }
                automaps->Release();
            }
            else
            {
                Bout << (ushort) 0;
            }

        }
        BOUT_END( this );
    }

    if( loc )
    {
        uint         msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( bool ) + sizeof( ushort ) +
                               sizeof( uint ) + sizeof( hash ) + sizeof( ushort );
        ScriptArray* automaps = ( loc->IsNonEmptyAutomaps() ? loc->GetAutomaps() : nullptr );
        if( automaps )
            msg_len += ( sizeof( hash ) + sizeof( uchar ) ) * (uint) automaps->GetSize();

        BOUT_BEGIN( this );
        Bout << NETMSG_AUTOMAPS_INFO;
        Bout << msg_len;
        Bout << (bool) false;        // Append this information
        Bout << (ushort) 1;
        Bout << loc->GetId();
        Bout << loc->GetProtoId();
        Bout << (ushort) ( automaps ? automaps->GetSize() : 0 );
        if( automaps )
        {
            for( uint i = 0, j = (uint) automaps->GetSize(); i < j; i++ )
            {
                hash pid = *(hash*) automaps->At( i );
                Bout << pid;
                Bout << (uchar) loc->GetMapIndex( pid );
            }
        }
        BOUT_END( this );

        if( automaps )
            automaps->Release();
    }
}

void Client::Send_Follow( uint rule, uchar follow_type, hash map_pid, uint follow_wait )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_FOLLOW;
    Bout << rule;
    Bout << follow_type;
    Bout << map_pid;
    Bout << follow_wait;
    BOUT_END( this );
}

void Client::Send_Effect( hash eff_pid, ushort hx, ushort hy, ushort radius )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_EFFECT;
    Bout << eff_pid;
    Bout << hx;
    Bout << hy;
    Bout << radius;
    BOUT_END( this );
}

void Client::Send_FlyEffect( hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_FLY_EFFECT;
    Bout << eff_pid;
    Bout << from_crid;
    Bout << to_crid;
    Bout << from_hx;
    Bout << from_hy;
    Bout << to_hx;
    Bout << to_hy;
    BOUT_END( this );
}

void Client::Send_PlaySound( uint crid_synchronize, const char* sound_name )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_PLAY_SOUND;
    Bout << crid_synchronize;
    Bout.Push( sound_name, 100 );
    BOUT_END( this );
}

void Client::Send_PlaySoundType( uint crid_synchronize, uchar sound_type, uchar sound_type_ext, uchar sound_id, uchar sound_id_ext )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_PLAY_SOUND_TYPE;
    Bout << crid_synchronize;
    Bout << sound_type;
    Bout << sound_type_ext;
    Bout << sound_id;
    Bout << sound_id_ext;
    BOUT_END( this );
}

void Client::Send_PlayersBarter( uchar barter, uint param, uint param_ext )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_PLAYERS_BARTER;
    Bout << barter;
    Bout << param;
    Bout << param_ext;
    BOUT_END( this );
}

void Client::Send_PlayersBarterSetHide( Item* item, uint count )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint       msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uint ) + sizeof( hash ) + sizeof( uint );
    PUCharVec* data;
    UIntVec*   data_sizes;
    uint       whole_data_size = item->Props.StoreData( false, &data, &data_sizes );
    msg_len += sizeof( ushort ) + whole_data_size;

    BOUT_BEGIN( this );
    Bout << NETMSG_PLAYERS_BARTER_SET_HIDE;
    Bout << item->GetId();
    Bout << item->GetProtoId();
    Bout << count;
    NET_WRITE_PROPERTIES( Bout, data, data_sizes );
    BOUT_END( this );
}

void Client::Send_RunClientScript( const char* func_name, int p0, int p1, int p2, const char* p3, UIntVec& p4 )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    char script_name[ MAX_FOTEXT ];
    Script::MakeScriptNameInRuntime( func_name, script_name );

    ushort script_name_len = Str::Length( script_name );
    ushort p3len = ( p3 ? Str::Length( p3 ) : 0 );
    ushort p4size = (ushort) p4.size();
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( script_name_len ) + script_name_len + sizeof( p0 ) + sizeof( p1 ) + sizeof( p2 ) + sizeof( p3len ) + p3len + sizeof( p4size ) + p4size * sizeof( uint );

    BOUT_BEGIN( this );
    Bout << NETMSG_RUN_CLIENT_SCRIPT;
    Bout << msg_len;
    Bout << script_name_len;
    Bout.Push( script_name, script_name_len );
    Bout << p0;
    Bout << p1;
    Bout << p2;
    Bout << p3len;
    if( p3len )
        Bout.Push( p3, p3len );
    Bout << p4size;
    if( p4size )
        Bout.Push( (char*) &p4[ 0 ], p4size * sizeof( uint ) );
    BOUT_END( this );
}

void Client::Send_DropTimers()
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_DROP_TIMERS;
    BOUT_END( this );
}

void Client::Send_ViewMap()
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << NETMSG_VIEW_MAP;
    Bout << ViewMapHx;
    Bout << ViewMapHy;
    Bout << ViewMapLocId;
    Bout << ViewMapLocEnt;
    BOUT_END( this );
}

void Client::Send_CheckUIDS()
{
    if( IsSendDisabled() )
        return;

    const uint uid_msg[ 5 ] = { NETMSG_CHECK_UID0, NETMSG_CHECK_UID1, NETMSG_CHECK_UID2, NETMSG_CHECK_UID3, NETMSG_CHECK_UID4 };
    uint       msg = uid_msg[ Random( 0, 4 ) ];
    uchar      rnd_count = Random( 1, 21 );
    uchar      rnd_count2 = Random( 2, 12 );
    uint       uidxor[ 5 ] = { (uint) Random( 1, 0x6FFFFFFF ), (uint) Random( 1, 0x6FFFFFFF ), (uint) Random( 1, 0x6FFFFFFF ), (uint) Random( 1, 0x6FFFFFFF ), (uint) Random( 1, 0x6FFFFFFF ) };
    uint       uid[ 5 ] = { UID[ 0 ] ^ uidxor[ 0 ], UID[ 1 ] ^ uidxor[ 1 ], UID[ 2 ] ^ uidxor[ 2 ], UID[ 3 ] ^ uidxor[ 3 ], UID[ 4 ] ^ uidxor[ 4 ] };
    uint       msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( uid ) + sizeof( uidxor ) + sizeof( rnd_count ) * 2 + rnd_count + rnd_count2;

    BOUT_BEGIN( this );
    Bout << msg;
    Bout << msg_len;
    Bout << uid[ 3 ];
    Bout << uidxor[ 0 ];
    Bout << rnd_count;
    Bout << uid[ 1 ];
    Bout << uidxor[ 2 ];
    for( int i = 0; i < rnd_count; i++ )
        Bout << (uchar) Random( 0, 255 );
    Bout << uid[ 2 ];
    Bout << uidxor[ 1 ];
    Bout << uid[ 4 ];
    Bout << rnd_count2;
    Bout << uidxor[ 3 ];
    Bout << uidxor[ 4 ];
    Bout << uid[ 0 ];
    for( int i = 0; i < rnd_count2; i++ )
        Bout << (uchar) Random( 0, 255 );
    BOUT_END( this );
}

void Client::Send_SomeItem( Item* item )
{
    uint       msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uint ) + sizeof( hash ) + sizeof( uchar );
    PUCharVec* data;
    UIntVec*   data_sizes;
    uint       whole_data_size = item->Props.StoreData( false, &data, &data_sizes );
    msg_len += sizeof( ushort ) + whole_data_size;

    BOUT_BEGIN( this );
    Bout << NETMSG_SOME_ITEM;
    Bout << msg_len;
    Bout << item->GetId();
    Bout << item->GetProtoId();
    Bout << item->GetCritSlot();
    NET_WRITE_PROPERTIES( Bout, data, data_sizes );
    BOUT_END( this );
}

void Client::Send_CustomMessage( uint msg )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Bout << msg;
    BOUT_END( this );
}

void Client::Send_CustomMessage( uint msg, uchar* data, uint data_size )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( msg ) + sizeof( msg_len ) + data_size;

    BOUT_BEGIN( this );
    Bout << msg;
    Bout << msg_len;
    if( data_size )
        Bout.Push( (char*) data, data_size );
    BOUT_END( this );
}

/************************************************************************/
/* Players barter                                                       */
/************************************************************************/

Client* Client::BarterGetOpponent( uint opponent_id )
{
    if( !GetMapId() && !GroupMove )
    {
        BarterEnd();
        return nullptr;
    }

    if( BarterOpponent && BarterOpponent != opponent_id )
    {
        Critter* cr = ( GetMapId() ? GetCritSelf( BarterOpponent, true ) : GroupMove->GetCritter( BarterOpponent ) );
        if( cr && cr->IsPlayer() )
            ( (Client*) cr )->BarterEnd();
        BarterEnd();
    }

    Critter* cr = ( GetMapId() ? GetCritSelf( opponent_id, true ) : GroupMove->GetCritter( opponent_id ) );
    if( !cr || !cr->IsPlayer() || !cr->IsLife() || cr->IsBusy() || IS_TIMEOUT( cr->GetTimeoutBattle() ) || ( (Client*) cr )->IsOffline() ||
        ( GetMapId() && !CheckDist( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), BARTER_DIST + GetMultihex() + cr->GetMultihex() ) ) )
    {
        if( cr && cr->IsPlayer() && BarterOpponent == cr->GetId() )
            ( (Client*) cr )->BarterEnd();
        BarterEnd();
        return nullptr;
    }

    BarterOpponent = cr->GetId();
    return (Client*) cr;
}

void Client::BarterEnd()
{
    BarterOpponent = 0;
    Send_PlayersBarter( BARTER_END, 0, 0 );
}

void Client::BarterRefresh( Client* opponent )
{
    Send_PlayersBarter( BARTER_REFRESH, opponent->GetId(), opponent->BarterHide );
    Send_PlayersBarter( BARTER_OFFER, BarterOffer, false );
    Send_PlayersBarter( BARTER_OFFER, opponent->BarterOffer, true );
    if( !opponent->BarterHide )
        Send_ContainerInfo( opponent, TRANSFER_CRIT_BARTER, false );
    for( auto it = BarterItems.begin(); it != BarterItems.end();)
    {
        BarterItem& barter_item = *it;
        if( !GetItem( barter_item.Id, true ) )
            it = BarterItems.erase( it );
        else
        {
            Send_PlayersBarter( BARTER_SET_SELF, barter_item.Id, barter_item.Count );
            ++it;
        }
    }
    for( auto it = opponent->BarterItems.begin(); it != opponent->BarterItems.end();)
    {
        BarterItem& barter_item = *it;
        if( !opponent->GetItem( barter_item.Id, true ) )
            it = opponent->BarterItems.erase( it );
        else
        {
            if( opponent->BarterHide )
                Send_PlayersBarterSetHide( opponent->GetItem( barter_item.Id, true ), barter_item.Count );
            else
                Send_PlayersBarter( BARTER_SET_OPPONENT, barter_item.Id, barter_item.Count );
            ++it;
        }
    }
}

Client::BarterItem* Client::BarterGetItem( uint item_id )
{
    auto it = std::find( BarterItems.begin(), BarterItems.end(), item_id );
    return it != BarterItems.end() ? &( *it ) : nullptr;
}

void Client::BarterEraseItem( uint item_id )
{
    auto it = std::find( BarterItems.begin(), BarterItems.end(), item_id );
    if( it != BarterItems.end() )
        BarterItems.erase( it );
}

/************************************************************************/
/* Timers                                                               */
/************************************************************************/

void Client::DropTimers( bool send )
{
    LastSendCraftTick = 0;
    LastSendEntrancesTick = 0;
    LastSendEntrancesLocId = 0;
    if( send )
        Send_DropTimers();
}

/************************************************************************/
/* Dialogs                                                              */
/************************************************************************/

void Client::ProcessTalk( bool force )
{
    if( !force && Timer::GameTick() < talkNextTick )
        return;
    talkNextTick = Timer::GameTick() + PROCESS_TALK_TICK;
    if( Talk.TalkType == TALK_NONE )
        return;

    // Check time of talk
    if( Timer::GameTick() - Talk.StartTick > Talk.TalkTime )
    {
        CloseTalk();
        return;
    }

    // Check npc
    Npc* npc = nullptr;
    if( Talk.TalkType == TALK_WITH_NPC )
    {
        npc = CrMngr.GetNpc( Talk.TalkNpc, true );
        if( !npc )
        {
            CloseTalk();
            return;
        }

        if( !npc->IsLife() )
        {
            Send_TextMsg( this, STR_DIALOG_NPC_NOT_LIFE, SAY_NETMSG, TEXTMSG_GAME );
            CloseTalk();
            return;
        }

        if( npc->IsPlaneNoTalk() )
        {
            Send_TextMsg( this, STR_DIALOG_NPC_BUSY, SAY_NETMSG, TEXTMSG_GAME );
            CloseTalk();
            return;
        }
    }

    // Check distance
    if( !Talk.IgnoreDistance )
    {
        uint   map_id = 0;
        ushort hx = 0, hy = 0;
        uint   talk_distance = 0;
        if( Talk.TalkType == TALK_WITH_NPC )
        {
            map_id = npc->GetMapId();
            hx = npc->GetHexX();
            hy = npc->GetHexY();
            talk_distance = npc->GetTalkDistance();
            talk_distance = ( talk_distance ? talk_distance : GameOpt.TalkDistance ) + GetMultihex();
        }
        else if( Talk.TalkType == TALK_WITH_HEX )
        {
            map_id = Talk.TalkHexMap;
            hx = Talk.TalkHexX;
            hy = Talk.TalkHexY;
            talk_distance = GameOpt.TalkDistance + GetMultihex();
        }

        if( GetMapId() != map_id || !CheckDist( GetHexX(), GetHexY(), hx, hy, talk_distance ) )
        {
            Send_TextMsg( this, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME );
            CloseTalk();
        }
    }
}

void Client::CloseTalk()
{
    if( Talk.TalkType != TALK_NONE )
    {
        Npc* npc = nullptr;
        if( Talk.TalkType == TALK_WITH_NPC )
        {
            Talk.TalkType = TALK_NONE;
            npc = CrMngr.GetNpc( Talk.TalkNpc, true );
            if( npc )
            {
                if( Talk.Barter )
                    npc->EventBarter( this, false, npc->GetBarterPlayers() );
                npc->EventTalk( this, false, npc->GetTalkedPlayers() );
            }
        }

        int func_id = Talk.CurDialog.DlgScript;
        switch( func_id )
        {
        case NOT_ANSWER_CLOSE_DIALOG:
            break;
        case NOT_ANSWER_BEGIN_BATTLE:
        {
            if( !npc )
                break;
            npc->SetTarget( REASON_FROM_DIALOG, this, GameOpt.DeadHitPoints, false );
        }
        break;
        default:
            if( func_id <= 0 )
                break;
            if( !Script::PrepareContext( func_id, _FUNC_, GetInfo() ) )
                break;
            Script::SetArgEntity( this );
            Script::SetArgEntity( npc );
            Script::SetArgEntity( nullptr );
            Talk.Locked = true;
            Script::RunPrepared();
            Talk.Locked = false;
            break;
        }
    }

    Talk.Clear();
    Send_Talk();
}

/************************************************************************/
/* NPC                                                                  */
/************************************************************************/

Npc::Npc( uint id, ProtoCritter* proto ): Critter( id, EntityType::Npc, proto )
{
    NextRefreshBagTick = 0;
    CritterIsNpc = true;
    MEMORY_PROCESS( MEMORY_NPC, sizeof( Npc ) + sizeof( GlobalMapGroup ) + 40 + sizeof( Item ) * 2 );
    SETFLAG( Flags, FCRIT_NPC );
}

Npc::~Npc()
{
    MEMORY_PROCESS( MEMORY_NPC, -(int) ( sizeof( Npc ) + sizeof( GlobalMapGroup ) + 40 + sizeof( Item ) * 2 ) );
    DropPlanes();
}

void Npc::RefreshBag()
{
    NextRefreshBagTick = Timer::GameTick() + GameOpt.BagRefreshTime * 60 * 1000;

    SyncLockItems();

    // Collect pids and count
    HashUIntMap pids;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( pids.count( item->GetProtoId() ) )
            pids[ item->GetProtoId() ] += item->GetCount();
        else
            pids[ item->GetProtoId() ] = item->GetCount();

        // Repair/reload item in slots
        if( item->GetCritSlot() != SLOT_INV )
        {
            if( item->GetDeteriorable() && item->IsBroken() )
                item->Repair();
            if( item->IsWeapon() && item->WeapGetMaxAmmoCount() && item->WeapIsEmpty() )
                item->WeapLoadHolder();
        }
    }

    // Item garbager
    if( !GetIsNoItemGarbager() )
    {
        // Erase not grouped items
        uint need_count = Random( 2, 4 );
        for( auto it = pids.begin(), end = pids.end(); it != end; ++it )
        {
            if( it->second <= need_count )
                continue;
            ProtoItem* proto_item = ProtoMngr.GetProtoItem( it->first );
            if( !proto_item || proto_item->GetStackable() )
                continue;
            ItemMngr.SetItemCritter( this, it->first, need_count );
            it->second = need_count;
            need_count = Random( 2, 4 );
        }
    }

    // Refresh bag
    bool drop_last_weapon = false;

    // Internal bag
    if( !GetBagId() )
    {
        ScriptArray* item_pid = GetInternalBagItemPid();
        ScriptArray* item_count = GetInternalBagItemCount();
        RUNTIME_ASSERT( item_pid->GetSize() == item_count->GetSize() );

        // Update cur items
        uint bag_size = item_pid->GetSize();
        for( uint k = 0; k < bag_size; k++ )
        {
            uint count = *(uint*) item_count->At( k );
            auto it = pids.find( *(uint*) item_pid->At( k ) );
            if( it != pids.end() && it->second > count )
                continue;
            ItemMngr.SetItemCritter( this, *(uint*) item_pid->At( k ), count );
            drop_last_weapon = true;
        }
        SAFEREL( item_pid );
        SAFEREL( item_count );
    }
    // External bags
    else
    {
        NpcBag& bag = AIMngr.GetBag( GetBagId() );
        if( bag.empty() )
            return;

        ScriptArray* current_set = GetExternalBagCurrentSet();
        if( current_set->GetSize() != bag.size() )
        {
            current_set->Resize( (uint) bag.size() );
            SetExternalBagCurrentSet( current_set );
        }

        // Check combinations, update or create new
        bool set_current_set = false;
        for( uint i = 0; i < bag.size(); i++ )
        {
            NpcBagCombination& comb = bag[ i ];
            bool               create = true;
            if( *(uchar*) current_set->At( i ) < (uchar) comb.size() )
            {
                NpcBagItems& items = comb[ *(uchar*) current_set->At( i ) ];
                for( uint l = 0; l < items.size(); l++ )
                {
                    NpcBagItem& item = items[ l ];
                    auto        it = pids.find( item.ItemPid );
                    if( it != pids.end() && it->second > 0 )
                    {
                        // Update cur items
                        for( uint k = 0; k < items.size(); k++ )
                        {
                            NpcBagItem& item_ = items[ k ];
                            uint        count = item_.MinCnt;
                            auto        it_ = pids.find( item_.ItemPid );
                            if( it_ != pids.end() && ( *it_ ).second > count )
                                continue;
                            if( item_.MinCnt != item_.MaxCnt )
                                count = Random( item_.MinCnt, item_.MaxCnt );
                            ItemMngr.SetItemCritter( this, item_.ItemPid, count );
                            drop_last_weapon = true;
                        }

                        create = false;
                        goto label_EndCycles;                         // Force end of cycle
                    }
                }
            }

label_EndCycles:
            // Create new combination
            if( create && comb.size() )
            {
                uchar rnd = Random( 0, (uchar) comb.size() - 1 );
                current_set->SetValue( i, &rnd );
                set_current_set = true;
                NpcBagItems& items = comb[ rnd ];
                for( uint k = 0; k < items.size(); k++ )
                {
                    NpcBagItem& bag_item = items[ k ];
                    uint        count = bag_item.MinCnt;
                    if( bag_item.MinCnt != bag_item.MaxCnt )
                        count = Random( bag_item.MinCnt, bag_item.MaxCnt );
                    if( ItemMngr.SetItemCritter( this, bag_item.ItemPid, count ) )
                    {
                        // Move bag_item
                        if( bag_item.ItemSlot != SLOT_INV )
                        {
                            if( !GetItemSlot( bag_item.ItemSlot ) )
                            {
                                Item* item = GetItemByPid( bag_item.ItemPid );
                                if( item && MoveItem( SLOT_INV, bag_item.ItemSlot, item->GetId(), item->GetCount() ) )
                                    SetFavoriteItemPid( bag_item.ItemSlot, bag_item.ItemPid );
                            }
                        }
                        drop_last_weapon = true;
                    }
                }
            }
        }

        if( set_current_set )
            SetExternalBagCurrentSet( current_set );
        SAFEREL( current_set );
    }

    if( drop_last_weapon && GetLastWeaponId() )
        SetLastWeaponId( 0 );
}

bool Npc::AddPlane( int reason, AIDataPlane* plane, bool is_child, Critter* some_cr, Item* some_item )
{
    if( is_child && aiPlanes.empty() )
    {
        plane->Assigned = false;
        plane->Release();
        return false;
    }

    if( plane->Type == AI_PLANE_ATTACK )
    {
        for( auto it = aiPlanes.begin(), end = aiPlanes.end(); it != end; ++it )
        {
            AIDataPlane* p = *it;
            if( p->Type == AI_PLANE_ATTACK && p->Attack.TargId == plane->Attack.TargId )
            {
                p->Assigned = false;
                p->Release();
                aiPlanes.erase( it );
                break;
            }
        }
    }

    int result = EventPlaneBegin( plane, reason, some_cr, some_item );
    if( result == PLANE_RUN_GLOBAL && Script::PrepareContext( ServerFunctions.NpcPlaneBegin, _FUNC_, GetInfo() ) )
    {
        Script::SetArgEntity( this );
        Script::SetArgPtr( plane );
        Script::SetArgUInt( reason );
        Script::SetArgEntity( some_cr );
        Script::SetArgEntity( some_item );
        if( Script::RunPrepared() )
            result = ( Script::GetReturnedBool() ? PLANE_KEEP : PLANE_DISCARD );
    }

    if( result == PLANE_DISCARD )
    {
        plane->Release();
        return false;
    }

    // Add
    plane->Assigned = true;
    if( is_child )
    {
        AIDataPlane* parent = aiPlanes[ 0 ]->GetCurPlane();
        parent->ChildPlane = plane;
        plane->Priority = parent->Priority;
    }
    else
    {
        if( !plane->Priority )
        {
            aiPlanes.push_back( plane );
        }
        else
        {
            uint i = 0;
            for( ; i < aiPlanes.size(); i++ )
                if( plane->Priority > aiPlanes[ i ]->Priority )
                    break;
            aiPlanes.insert( aiPlanes.begin() + i, plane );
            if( i && plane->Priority == aiPlanes[ 0 ]->Priority )
                SetBestCurPlane();
        }
    }
    return true;
}

void Npc::NextPlane( int reason, Critter* some_cr, Item* some_item )
{
    if( aiPlanes.empty() )
        return;
    SendA_XY();

    AIDataPlane* cur = aiPlanes[ 0 ];
    AIDataPlane* last = aiPlanes[ 0 ]->GetCurPlane();

    aiPlanes.erase( aiPlanes.begin() );
    SetBestCurPlane();

    int result = EventPlaneEnd( last, reason, some_cr, some_item );
    if( result == PLANE_RUN_GLOBAL && Script::PrepareContext( ServerFunctions.NpcPlaneEnd, _FUNC_, GetInfo() ) )
    {
        Script::SetArgEntity( this );
        Script::SetArgPtr( last );
        Script::SetArgUInt( reason );
        Script::SetArgEntity( some_cr );
        Script::SetArgEntity( some_item );
        if( Script::RunPrepared() )
            result = ( Script::GetReturnedBool() ? PLANE_DISCARD : PLANE_KEEP );
    }

    if( result == PLANE_KEEP )
    {
        uint place = 0;
        if( aiPlanes.size() )
        {
            place = 1;
            for( uint i = (uint) aiPlanes.size(); place < i; place++ )
                if( cur->Priority > aiPlanes[ place ]->Priority )
                    break;
        }
        aiPlanes.insert( aiPlanes.begin() + place, cur );
    }
    else
    {
        if( cur != last )     // Child
        {
            cur->DeleteLast();
            aiPlanes.insert( aiPlanes.begin(), cur );
        }
        else         // Main
        {
            cur->Assigned = false;
            cur->Release();
        }
    }
}

bool Npc::RunPlane( int reason, uint& r0, uint& r1, uint& r2 )
{
    AIDataPlane* cur = aiPlanes[ 0 ];
    AIDataPlane* last = aiPlanes[ 0 ]->GetCurPlane();

    int          result = EventPlaneRun( last, reason, r0, r1, r2 );
    if( result == PLANE_RUN_GLOBAL && Script::PrepareContext( ServerFunctions.NpcPlaneRun, _FUNC_, GetInfo() ) )
    {
        Script::SetArgEntity( this );
        Script::SetArgPtr( last );
        Script::SetArgUInt( reason );
        Script::SetArgAddress( &r0 );
        Script::SetArgAddress( &r1 );
        Script::SetArgAddress( &r2 );
        if( Script::RunPrepared() )
            result = ( Script::GetReturnedBool() ? PLANE_KEEP : PLANE_DISCARD );
    }

    return result == PLANE_KEEP;
}

bool Npc::IsPlaneAviable( int plane_type )
{
    for( uint i = 0, j = (uint) aiPlanes.size(); i < j; i++ )
        if( aiPlanes[ i ]->IsSelfOrHas( plane_type ) )
            return true;
    return false;
}

bool Npc::IsCurPlane( int plane_type )
{
    AIDataPlane* p = GetCurPlane();
    return p ? p->Type == plane_type : false;
}

void Npc::DropPlanes()
{
    for( uint i = 0, j = (uint) aiPlanes.size(); i < j; i++ )
    {
        aiPlanes[ i ]->Assigned = false;
        aiPlanes[ i ]->Release();
    }
    aiPlanes.clear();
}

void Npc::SetBestCurPlane()
{
    if( aiPlanes.size() < 2 )
        return;

    uint type = aiPlanes[ 0 ]->Type;
    uint priority = aiPlanes[ 0 ]->Priority;

    if( type != AI_PLANE_ATTACK )
        return;

    int sort_cnt = 1;
    for( uint i = 1, j = (uint) aiPlanes.size(); i < j; i++ )
    {
        if( aiPlanes[ i ]->Priority == priority )
            sort_cnt++;
        else
            break;
    }

    if( sort_cnt < 2 )
        return;

    // Find critter with smallest dist
    int hx = GetHexX();
    int hy = GetHexY();
    int best_plane = -1;
    int best_dist = -1;
    for( int i = 0; i < sort_cnt; i++ )
    {
        Critter* cr = GetCritSelf( aiPlanes[ i ]->Attack.TargId, false );
        if( !cr )
            continue;

        int dist = DistGame( hx, hy, cr->GetHexX(), cr->GetHexY() );
        if( best_dist >= 0 && dist >= best_dist )
            continue;
        else if( dist == best_dist )
        {
            Critter* cr2 = GetCritSelf( aiPlanes[ best_plane ]->Attack.TargId, false );
            if( !cr || cr->GetCurrentHp() >= cr2->GetCurrentHp() )
                continue;
        }

        best_dist = dist;
        best_plane = i;
    }

    if( best_plane > 0 )
        std::swap( aiPlanes[ 0 ], aiPlanes[ best_plane ] );
}

uint Npc::GetTalkedPlayers()
{
    uint talk = 0;
    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !cr->IsPlayer() )
            continue;
        Client* cl = (Client*) cr;
        if( cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == GetId() )
            talk++;
    }
    return talk;
}

bool Npc::IsTalkedPlayers()
{
    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !cr->IsPlayer() )
            continue;
        Client* cl = (Client*) cr;
        if( cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == GetId() )
            return true;
    }
    return false;
}

uint Npc::GetBarterPlayers()
{
    uint barter = 0;
    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !cr->IsPlayer() )
            continue;
        Client* cl = (Client*) cr;
        if( cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == GetId() && cl->Talk.Barter )
            barter++;
    }
    return barter;
}

bool Npc::IsFreeToTalk()
{
    int max_talkers = GetMaxTalkers();
    if( max_talkers < 0 )
        return false;
    else if( !max_talkers )
        max_talkers = (uint) GameOpt.NpcMaxTalkers;
    return GetTalkedPlayers() < (uint) max_talkers;
}

bool Npc::IsPlaneNoTalk()
{
    AIDataPlane* p = GetCurPlane();
    if( !p )
        return false;

    switch( p->Type )
    {
    case AI_PLANE_WALK:
    case AI_PLANE_ATTACK:
    case AI_PLANE_PICK:
        return true;
    default:
        break;
    }

    return false;
}

void Npc::MoveToHex( int reason, ushort hx, ushort hy, uchar ori, bool is_run, uchar cut )
{
    AIDataPlane* plane = new AIDataPlane( AI_PLANE_WALK, AI_PLANE_WALK_PRIORITY );
    if( !plane )
        return;
    plane->Walk.HexX = hx;
    plane->Walk.HexY = hy;
    plane->Walk.Dir = ori;
    plane->Walk.IsRun = is_run;
    plane->Walk.Cut = cut;
    AddPlane( reason, plane, false );
}

void Npc::SetTarget( int reason, Critter* target, int min_hp, bool is_gag )
{
    for( uint i = 0, j = (uint) aiPlanes.size(); i < j; i++ )
    {
        AIDataPlane* pp = aiPlanes[ i ];
        if( pp->Type == AI_PLANE_ATTACK && pp->Attack.TargId == target->GetId() )
        {
            if( i && pp->Priority == aiPlanes[ 0 ]->Priority )
                SetBestCurPlane();
            return;
        }
    }

    AIDataPlane* plane = new AIDataPlane( AI_PLANE_ATTACK, AI_PLANE_ATTACK_PRIORITY );
    if( !plane )
        return;
    plane->Attack.TargId = target->GetId();
    plane->Attack.MinHp = min_hp;
    plane->Attack.LastHexX = ( target->GetMapId() ? target->GetHexX() : 0 );
    plane->Attack.LastHexY = ( target->GetMapId() ? target->GetHexY() : 0 );
    plane->Attack.IsGag = is_gag;
    plane->Attack.GagHexX = target->GetHexX();
    plane->Attack.GagHexY = target->GetHexY();
    plane->Attack.IsRun = ( IsTurnBased() ? GameOpt.TbAlwaysRun : GameOpt.RtAlwaysRun );
    AddPlane( reason, plane, false, target, nullptr );
}
