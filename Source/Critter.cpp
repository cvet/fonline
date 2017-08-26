#include "Common.h"
#include "Critter.h"
#include "Script.h"
#include "MapManager.h"
#include "ItemManager.h"
#include "Access.h"
#include "CritterManager.h"
#include "ProtoManager.h"

ProtoCritter::ProtoCritter( hash pid ): ProtoEntity( pid, Critter::PropertiesRegistrator ) {}
CLASS_PROPERTY_ALIAS_IMPL( ProtoCritter, Critter, uint, Multihex );

/************************************************************************/
/*                                                                      */
/************************************************************************/

bool Critter::SlotEnabled[ 0x100 ];
bool Critter::SlotDataSendEnabled[ 0x100 ];

// Properties
PROPERTIES_IMPL( Critter );
CLASS_PROPERTY_IMPL( Critter, ModelName );
CLASS_PROPERTY_IMPL( Critter, MapId );
CLASS_PROPERTY_IMPL( Critter, RefMapId );
CLASS_PROPERTY_IMPL( Critter, RefMapPid );
CLASS_PROPERTY_IMPL( Critter, RefLocationId );
CLASS_PROPERTY_IMPL( Critter, RefLocationPid );
CLASS_PROPERTY_IMPL( Critter, HexX );
CLASS_PROPERTY_IMPL( Critter, HexY );
CLASS_PROPERTY_IMPL( Critter, Dir );
CLASS_PROPERTY_IMPL( Critter, Password );
CLASS_PROPERTY_IMPL( Critter, Cond );
CLASS_PROPERTY_IMPL( Critter, ClientToDelete );
CLASS_PROPERTY_IMPL( Critter, Multihex );
CLASS_PROPERTY_IMPL( Critter, WorldX );
CLASS_PROPERTY_IMPL( Critter, WorldY );
CLASS_PROPERTY_IMPL( Critter, GlobalMapLeaderId );
CLASS_PROPERTY_IMPL( Critter, GlobalMapTripId );
CLASS_PROPERTY_IMPL( Critter, RefGlobalMapLeaderId );
CLASS_PROPERTY_IMPL( Critter, RefGlobalMapTripId );
CLASS_PROPERTY_IMPL( Critter, LastMapHexX );
CLASS_PROPERTY_IMPL( Critter, LastMapHexY );
CLASS_PROPERTY_IMPL( Critter, Anim1Life );
CLASS_PROPERTY_IMPL( Critter, Anim1Knockout );
CLASS_PROPERTY_IMPL( Critter, Anim1Dead );
CLASS_PROPERTY_IMPL( Critter, Anim2Life );
CLASS_PROPERTY_IMPL( Critter, Anim2Knockout );
CLASS_PROPERTY_IMPL( Critter, Anim2Dead );
CLASS_PROPERTY_IMPL( Critter, GlobalMapFog );
CLASS_PROPERTY_IMPL( Critter, TE_FuncNum );
CLASS_PROPERTY_IMPL( Critter, TE_Rate );
CLASS_PROPERTY_IMPL( Critter, TE_NextTime );
CLASS_PROPERTY_IMPL( Critter, TE_Identifier );
CLASS_PROPERTY_IMPL( Critter, LookDistance );
CLASS_PROPERTY_IMPL( Critter, DialogId );
CLASS_PROPERTY_IMPL( Critter, NpcRole );
CLASS_PROPERTY_IMPL( Critter, MaxTalkers );
CLASS_PROPERTY_IMPL( Critter, TalkDistance );
CLASS_PROPERTY_IMPL( Critter, CurrentHp );
CLASS_PROPERTY_IMPL( Critter, WalkTime );
CLASS_PROPERTY_IMPL( Critter, RunTime );
CLASS_PROPERTY_IMPL( Critter, ScaleFactor );
CLASS_PROPERTY_IMPL( Critter, SneakCoefficient );
CLASS_PROPERTY_IMPL( Critter, TimeoutBattle );
CLASS_PROPERTY_IMPL( Critter, TimeoutTransfer );
CLASS_PROPERTY_IMPL( Critter, TimeoutRemoveFromGame );
CLASS_PROPERTY_IMPL( Critter, IsNoUnarmed );
CLASS_PROPERTY_IMPL( Critter, IsGeck );
CLASS_PROPERTY_IMPL( Critter, IsNoHome );
CLASS_PROPERTY_IMPL( Critter, IsNoWalk );
CLASS_PROPERTY_IMPL( Critter, IsNoRun );
CLASS_PROPERTY_IMPL( Critter, IsNoRotate );
CLASS_PROPERTY_IMPL( Critter, IsNoTalk );
CLASS_PROPERTY_IMPL( Critter, IsHide );
CLASS_PROPERTY_IMPL( Critter, KnownLocations );
CLASS_PROPERTY_IMPL( Critter, ConnectionIp );
CLASS_PROPERTY_IMPL( Critter, ConnectionPort );
CLASS_PROPERTY_IMPL( Critter, HomeMapId );
CLASS_PROPERTY_IMPL( Critter, HomeHexX );
CLASS_PROPERTY_IMPL( Critter, HomeHexY );
CLASS_PROPERTY_IMPL( Critter, HomeDir );
CLASS_PROPERTY_IMPL( Critter, ShowCritterDist1 );
CLASS_PROPERTY_IMPL( Critter, ShowCritterDist2 );
CLASS_PROPERTY_IMPL( Critter, ShowCritterDist3 );
CLASS_PROPERTY_IMPL( Critter, ScriptId );

Critter::Critter( uint id, EntityType type, ProtoCritter* proto ): Entity( id, type, PropertiesRegistrator, proto )
{
    CritterIsNpc = false;
    GlobalMapGroup = nullptr;
    startBreakTime = 0;
    breakTime = 0;
    waitEndTick = 0;
    CacheValuesNextTick = 0;
    Flags = 0;
    LockMapTransfers = 0;
    ViewMapId = 0;
    ViewMapPid = 0;
    ViewMapLook = 0;
    ViewMapHx = ViewMapHy = 0;
    ViewMapDir = 0;
    DisableSend = 0;
    CanBeRemoved = false;
    Name = "";
    memzero( &Moving, sizeof( Moving ) );
    Moving.State = 1;
}

Critter::~Critter()
{
    RUNTIME_ASSERT( !GlobalMapGroup );
}

void Critter::SetBreakTime( uint ms )
{
    breakTime = ms;
    startBreakTime = Timer::GameTick();
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

uint Critter::GetAttackDist( Item* weap, int use )
{
    uint dist = 1;
    Script::RaiseInternalEvent( ServerFunctions.CritterGetAttackDistantion, this, weap, use, &dist );
    return dist;
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

Map* Critter::GetMap()
{
    Map* map = nullptr;
    uint map_id = GetMapId();
    if( map_id )
    {
        map = MapMngr.GetMap( map_id );
        RUNTIME_ASSERT( map );
    }
    return map;
}

void Critter::ProcessVisibleCritters()
{
    if( IsDestroyed )
        return;

    Map* map = GetMap();

    // Global map
    if( !map )
    {
        RUNTIME_ASSERT( GlobalMapGroup );

        if( IsPlayer() )
        {
            Client* cl = (Client*) this;
            for( auto it = GlobalMapGroup->begin(), end = GlobalMapGroup->end(); it != end; ++it )
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
    int   vis;
    int   look_base_self = GetLookDistance();
    int   dir_self = GetDir();
    int   dirs_count = DIRS_COUNT;
    uint  show_cr_dist1 = GetShowCritterDist1();
    uint  show_cr_dist2 = GetShowCritterDist2();
    uint  show_cr_dist3 = GetShowCritterDist3();
    bool  show_cr1 = ( show_cr_dist1 > 0 );
    bool  show_cr2 = ( show_cr_dist2 > 0 );
    bool  show_cr3 = ( show_cr_dist3 > 0 );

    bool  show_cr = ( show_cr1 || show_cr2 || show_cr3 );
    // Sneak self
    int   sneak_base_self = GetSneakCoefficient();

    CrVec critters;
    map->GetCritters( critters );

    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this || cr->IsDestroyed )
            continue;

        int dist = DistGame( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY() );

        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SCRIPT ) )
        {
            bool allow_self = Script::RaiseInternalEvent( ServerFunctions.MapCheckLook, map, this, cr );
            bool allow_opp = Script::RaiseInternalEvent( ServerFunctions.MapCheckLook, map, cr, this );

            if( allow_self )
            {
                if( cr->AddCrIntoVisVec( this ) )
                {
                    Send_AddCritter( cr );
                    Script::RaiseInternalEvent( ServerFunctions.CritterShow, this, cr );
                }
            }
            else
            {
                if( cr->DelCrFromVisVec( this ) )
                {
                    Send_RemoveCritter( cr );
                    Script::RaiseInternalEvent( ServerFunctions.CritterHide, this, cr );
                }
            }

            if( allow_opp )
            {
                if( AddCrIntoVisVec( cr ) )
                {
                    cr->Send_AddCritter( this );
                    Script::RaiseInternalEvent( ServerFunctions.CritterShow, cr, this );
                }
            }
            else
            {
                if( DelCrFromVisVec( cr ) )
                {
                    cr->Send_RemoveCritter( this );
                    Script::RaiseInternalEvent( ServerFunctions.CritterHide, cr, this );
                }
            }

            if( show_cr )
            {
                if( show_cr1 )
                {
                    if( (int) show_cr_dist1 >= dist )
                    {
                        if( AddCrIntoVisSet1( cr->GetId() ) )
                            Script::RaiseInternalEvent( ServerFunctions.CritterShowDist1, this, cr );
                    }
                    else
                    {
                        if( DelCrFromVisSet1( cr->GetId() ) )
                            Script::RaiseInternalEvent( ServerFunctions.CritterHideDist1, this, cr );
                    }
                }
                if( show_cr2 )
                {
                    if( (int) show_cr_dist2 >= dist )
                    {
                        if( AddCrIntoVisSet2( cr->GetId() ) )
                            Script::RaiseInternalEvent( ServerFunctions.CritterShowDist2, this, cr );
                    }
                    else
                    {
                        if( DelCrFromVisSet2( cr->GetId() ) )
                            Script::RaiseInternalEvent( ServerFunctions.CritterHideDist2, this, cr );
                    }
                }
                if( show_cr3 )
                {
                    if( (int) show_cr_dist3 >= dist )
                    {
                        if( AddCrIntoVisSet3( cr->GetId() ) )
                            Script::RaiseInternalEvent( ServerFunctions.CritterShowDist3, this, cr );
                    }
                    else
                    {
                        if( DelCrFromVisSet3( cr->GetId() ) )
                            Script::RaiseInternalEvent( ServerFunctions.CritterHideDist3, this, cr );
                    }
                }
            }

            uint cr_dist = cr->GetShowCritterDist1();
            if( cr_dist )
            {
                if( (int) cr_dist >= dist )
                {
                    if( cr->AddCrIntoVisSet1( GetId() ) )
                        Script::RaiseInternalEvent( ServerFunctions.CritterShowDist1, cr, this );
                }
                else
                {
                    if( cr->DelCrFromVisSet1( GetId() ) )
                        Script::RaiseInternalEvent( ServerFunctions.CritterHideDist1, cr, this );
                }
            }
            cr_dist = cr->GetShowCritterDist2();
            if( cr_dist )
            {
                if( (int) cr_dist >= dist )
                {
                    if( cr->AddCrIntoVisSet2( GetId() ) )
                        Script::RaiseInternalEvent( ServerFunctions.CritterShowDist2, cr, this );
                }
                else
                {
                    if( cr->DelCrFromVisSet2( GetId() ) )
                        Script::RaiseInternalEvent( ServerFunctions.CritterHideDist2, cr, this );
                }
            }
            cr_dist = cr->GetShowCritterDist3();
            if( cr_dist )
            {
                if( (int) cr_dist >= dist )
                {
                    if( cr->AddCrIntoVisSet3( GetId() ) )
                        Script::RaiseInternalEvent( ServerFunctions.CritterShowDist1, cr, this );
                }
                else
                {
                    if( cr->DelCrFromVisSet3( GetId() ) )
                        Script::RaiseInternalEvent( ServerFunctions.CritterHideDist3, cr, this );
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
                Script::RaiseInternalEvent( ServerFunctions.CritterShow, this, cr );
            }
        }
        else
        {
            if( cr->DelCrFromVisVec( this ) )
            {
                Send_RemoveCritter( cr );
                Script::RaiseInternalEvent( ServerFunctions.CritterHide, this, cr );
            }
        }

        if( show_cr1 )
        {
            if( (int) show_cr_dist1 >= dist )
            {
                if( AddCrIntoVisSet1( cr->GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterShowDist1, this, cr );
            }
            else
            {
                if( DelCrFromVisSet1( cr->GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterHideDist1, this, cr );
            }
        }
        if( show_cr2 )
        {
            if( (int) show_cr_dist2 >= dist )
            {
                if( AddCrIntoVisSet2( cr->GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterShowDist2, this, cr );
            }
            else
            {
                if( DelCrFromVisSet2( cr->GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterHideDist2, this, cr );
            }
        }
        if( show_cr3 )
        {
            if( (int) show_cr_dist3 >= dist )
            {
                if( AddCrIntoVisSet3( cr->GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterShowDist3, this, cr );
            }
            else
            {
                if( DelCrFromVisSet3( cr->GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterHideDist3, this, cr );
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
                Script::RaiseInternalEvent( ServerFunctions.CritterShow, cr, this );
            }
        }
        else
        {
            if( DelCrFromVisVec( cr ) )
            {
                cr->Send_RemoveCritter( this );
                Script::RaiseInternalEvent( ServerFunctions.CritterHide, cr, this );
            }
        }

        uint cr_dist = cr->GetShowCritterDist1();
        if( cr_dist )
        {
            if( (int) cr_dist >= dist )
            {
                if( cr->AddCrIntoVisSet1( GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterShowDist1, cr, this );
            }
            else
            {
                if( cr->DelCrFromVisSet1( GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterHideDist1, cr, this );
            }
        }
        cr_dist = cr->GetShowCritterDist2();
        if( cr_dist )
        {
            if( (int) cr_dist >= dist )
            {
                if( cr->AddCrIntoVisSet2( GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterShowDist2, cr, this );
            }
            else
            {
                if( cr->DelCrFromVisSet2( GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterHideDist2, cr, this );
            }
        }
        cr_dist = cr->GetShowCritterDist3();
        if( cr_dist )
        {
            if( (int) cr_dist >= dist )
            {
                if( cr->AddCrIntoVisSet3( GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterShowDist3, cr, this );
            }
            else
            {
                if( cr->DelCrFromVisSet3( GetId() ) )
                    Script::RaiseInternalEvent( ServerFunctions.CritterHideDist3, cr, this );
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
        {
            continue;
        }
        else if( item->GetIsAlwaysView() )
        {
            if( AddIdVisItem( item->GetId() ) )
            {
                Send_AddItemOnMap( item );
                Script::RaiseInternalEvent( ServerFunctions.CritterShowItemOnMap, this, item, item->ViewPlaceOnMap, item->ViewByCritter );
            }
        }
        else
        {
            bool allowed = false;
            if( item->GetIsTrap() && FLAG( GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT ) )
            {
                allowed = Script::RaiseInternalEvent( ServerFunctions.MapCheckTrapLook, map, this, item );
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
                    Script::RaiseInternalEvent( ServerFunctions.CritterShowItemOnMap, this, item, item->ViewPlaceOnMap, item->ViewByCritter );
                }
            }
            else
            {
                if( DelIdVisItem( item->GetId() ) )
                {
                    Send_EraseItemFromMap( item );
                    Script::RaiseInternalEvent( ServerFunctions.CritterHideItemOnMap, this, item, item->ViewPlaceOnMap, item->ViewByCritter );
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
    map->GetCritters( critters );

    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr == this || cr->IsDestroyed )
            continue;

        if( FLAG( GameOpt.LookChecks, LOOK_CHECK_SCRIPT ) )
        {
            if( Script::RaiseInternalEvent( ServerFunctions.MapCheckLook, map, this, cr ) )
                Send_AddCritter( cr );
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
        {
            continue;
        }
        else if( item->GetIsAlwaysView() )
        {
            Send_AddItemOnMap( item );
        }
        else
        {
            bool allowed = false;
            if( item->GetIsTrap() && FLAG( GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT ) )
            {
                allowed = Script::RaiseInternalEvent( ServerFunctions.MapCheckTrapLook, map, this, item );
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

Critter* Critter::GetCritSelf( uint crid )
{
    auto it = VisCrSelfMap.find( crid );
    return it != VisCrSelfMap.end() ? it->second : nullptr;
}

void Critter::GetCrFromVisCr( CrVec& critters, int find_type, bool vis_cr_self )
{
    CrVec& vis_cr = ( vis_cr_self ? VisCrSelf : VisCr );

    CrVec  find_critters;
    for( auto it = vis_cr.begin(), end = vis_cr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->CheckFind( find_type ) && std::find( critters.begin(), critters.end(), cr ) == critters.end() )
            find_critters.push_back( cr );
    }

    critters.reserve( critters.size() + find_critters.size() );
    for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
        critters.push_back( *it );
}

Critter* Critter::GetGlobalMapCritter( uint cr_id )
{
    RUNTIME_ASSERT( GlobalMapGroup );
    auto it = std::find_if( GlobalMapGroup->begin(), GlobalMapGroup->end(), [ &cr_id ] ( Critter * other )
                            {
                                return other->Id == cr_id;
                            } );
    return it != GlobalMapGroup->end() ? *it : nullptr;
}

bool Critter::AddCrIntoVisVec( Critter* add_cr )
{
    if( VisCrMap.count( add_cr->GetId() ) )
        return false;

    VisCr.push_back( add_cr );
    VisCrMap.insert( std::make_pair( add_cr->GetId(), add_cr ) );

    add_cr->VisCrSelf.push_back( this );
    add_cr->VisCrSelfMap.insert( std::make_pair( this->GetId(), this ) );
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
        if( item->GetCritSlot() )
            SendAA_MoveItem( item, ACTION_REFRESH, 0 );
    }

    // Change item
    Script::RaiseInternalEvent( ServerFunctions.CritterMoveItem, this, item, -1 );
}

void Critter::SetItem( Item* item )
{
    RUNTIME_ASSERT( item );

    invItems.push_back( item );

    if( item->GetAccessory() != ITEM_ACCESSORY_CRITTER )
        item->SetCritSlot( 0 );
    item->SetAccessory( ITEM_ACCESSORY_CRITTER );
    item->SetCritId( Id );
}

void Critter::EraseItem( Item* item, bool send )
{
    RUNTIME_ASSERT( item );

    auto it = std::find( invItems.begin(), invItems.end(), item );
    RUNTIME_ASSERT( it != invItems.end() );
    invItems.erase( it );

    item->SetAccessory( ITEM_ACCESSORY_NONE );

    if( send )
        Send_EraseItem( item );
    if( item->GetCritSlot() )
        SendAA_MoveItem( item, ACTION_REFRESH, 0 );

    Script::RaiseInternalEvent( ServerFunctions.CritterMoveItem, this, item, item->GetCritSlot() );
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
            return item;
        }
    }
    return nullptr;
}

Item* Critter::GetItemByPid( hash item_pid )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == item_pid )
            return item;
    }
    return nullptr;
}

Item* Critter::GetItemByPidSlot( hash item_pid, int slot )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == item_pid && item->GetCritSlot() == slot )
            return item;
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
                return item;
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
                if( !item->GetCritSlot() )
                    return item;
                another_slot = item;
            }
        }
        return another_slot;
    }
    return nullptr;
}

Item* Critter::GetItemCar()
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->IsCar() )
            return item;
    }
    return nullptr;
}

Item* Critter::GetItemSlot( int slot )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetCritSlot() == slot )
            return item;
    }
    return nullptr;
}

void Critter::GetItemsSlot( int slot, ItemVec& items )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( slot < 0 || item->GetCritSlot() == slot )
            items.push_back( item );
    }
}

void Critter::GetItemsType( int type, ItemVec& items )
{
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetType() == type )
            items.push_back( item );
    }
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

uint Critter::CountItems()
{
    uint count = 0;
    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
        count += ( *it )->GetCount();
    return count;
}

ItemVec& Critter::GetInventory()
{
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

bool Critter::SetScript( asIScriptFunction* func, bool first_time )
{
    hash func_num = 0;
    if( func )
    {
        func_num = Script::BindScriptFuncNumByFunc( func );
        if( !func_num )
        {
            WriteLog( "Script bind fail, critter '{}'.\n", GetName() );
            return false;
        }
        SetScriptId( func_num );
    }
    else
    {
        func_num = GetScriptId();
    }

    if( func_num )
    {
        Script::PrepareScriptFuncContext( func_num, GetName() );
        Script::SetArgEntity( this );
        Script::SetArgBool( first_time );
        Script::RunPrepared();
    }
    return true;
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
void Critter::Send_Text( Critter* from_cr, const string& text, uchar how_say )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_Text( from_cr, text, how_say );
}
void Critter::Send_TextEx( uint from_id, const string& text, uchar how_say, bool unsafe_text )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_TextEx( from_id, text, how_say, unsafe_text );
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
void Critter::Send_AutomapsInfo( void* locs_vec, Location* loc )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_AutomapsInfo( locs_vec, loc );
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
void Critter::Send_PlaySound( uint crid_synchronize, const string& sound_name )
{
    if( IsPlayer() )
        ( (Client*) this )->Send_PlaySound( crid_synchronize, sound_name );
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

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Action( this, action, action_ext, item );
    }
}

void Critter::SendAA_MoveItem( Item* item, uchar action, uchar prev_slot )
{
    if( IsPlayer() )
        Send_MoveItem( this, item, action, prev_slot );

    if( VisCr.empty() )
        return;

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

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_SetAnims( this, cond, anim1, anim2 );
    }
}

void Critter::SendAA_Text( CrVec& to_cr, const string& text, uchar how_say, bool unsafe_text )
{
    if( text.empty() )
        return;

    uint from_id = GetId();

    if( IsPlayer() )
        Send_TextEx( from_id, text, how_say, unsafe_text );

    if( to_cr.empty() )
        return;

    int dist = -1;
    if( how_say == SAY_SHOUT || how_say == SAY_SHOUT_ON_HEAD )
        dist = GameOpt.ShoutDist + GetMultihex();
    else if( how_say == SAY_WHISP || how_say == SAY_WHISP_ON_HEAD )
        dist = GameOpt.WhisperDist + GetMultihex();

    for( Critter* cr : to_cr )
    {
        if( cr == this || !cr->IsPlayer() )
            continue;

        if( dist == -1 )
            cr->Send_TextEx( from_id, text, how_say, unsafe_text );
        else if( CheckDist( GetHexX(), GetHexY(), cr->GetHexX(), cr->GetHexY(), dist + cr->GetMultihex() ) )
            cr->Send_TextEx( from_id, text, how_say, unsafe_text );
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

    for( auto it = VisCr.begin(), end = VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() )
            cr->Send_Dir( this );
    }
}

void Critter::SendA_CustomCommand( ushort num_param, int val )
{
    if( VisCr.empty() )
        return;

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

    Client* cl = (Client*) this;
    BOUT_BEGIN( cl );
    cl->Connection->Bout << NETMSG_CLEAR_ITEMS;
    BOUT_END( cl );

    for( auto it = invItems.begin(), end = invItems.end(); it != end; ++it )
        Send_AddItem( *it );

    BOUT_BEGIN( cl );
    cl->Connection->Bout << NETMSG_ALL_ITEMS_SEND;
    BOUT_END( cl );
}

void Critter::Send_AllAutomapsInfo()
{
    if( !IsPlayer() )
        return;

    LocVec        locs;
    CScriptArray* known_locs = GetKnownLocations();
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
            Script::RaiseInternalEvent( ServerFunctions.CritterMessage, cr, this, num, val );
        }
    }
    break;
    case MESSAGE_TO_IAM_VISIBLE:
    {
        CrVec critters = VisCrSelf;
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            Script::RaiseInternalEvent( ServerFunctions.CritterMessage, cr, this, num, val );
        }
    }
    break;
    case MESSAGE_TO_ALL_ON_MAP:
    {
        Map* map = GetMap();
        if( !map )
            break;

        CrVec critters;
        map->GetCritters( critters );
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            Script::RaiseInternalEvent( ServerFunctions.CritterMessage, cr, this, num, val );
        }
    }
    break;
    default:
        break;
    }
}

void Critter::RefreshName()
{
    if( !IsPlayer() )
    {
        hash        dlg_pack_id = GetDialogId();
        DialogPack* dlg_pack = ( dlg_pack_id ? DlgMngr.GetDialog( dlg_pack_id ) : nullptr );
        if( dlg_pack )
            Name = _str( "{} {} (Npc)", dlg_pack->PackName, GetId() );
        else
            Name = _str( "{} (Npc)", GetId() );
    }
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
/* Locations                                                            */
/************************************************************************/

bool Critter::CheckKnownLocById( uint loc_id )
{
    CScriptArray* known_locs = GetKnownLocations();
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
    CScriptArray* known_locs = GetKnownLocations();
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

    CScriptArray* known_locs = GetKnownLocations();
    known_locs->InsertLast( &loc_id );
    SetKnownLocations( known_locs );
    SAFEREL( known_locs );
}

void Critter::EraseKnownLoc( uint loc_id )
{
    CScriptArray* known_locs = GetKnownLocations();
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

    CScriptArray* te_next_time = GetTE_NextTime();
    CScriptArray* te_func_num = GetTE_FuncNum();
    CScriptArray* te_rate = GetTE_Rate();
    CScriptArray* te_identifier = GetTE_Identifier();
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
    CScriptArray* te_next_time = GetTE_NextTime();
    CScriptArray* te_func_num = GetTE_FuncNum();
    CScriptArray* te_rate = GetTE_Rate();
    CScriptArray* te_identifier = GetTE_Identifier();
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
    CScriptArray* te_next_time = GetTE_NextTime();
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

Client::Client( NetConnection* conn, ProtoCritter* proto ): Critter( 0, EntityType::Client, proto )
{
    Connection = conn;
    Access = ACCESS_DEFAULT;
    pingOk = true;
    LanguageMsg = 0;
    GameState = STATE_NONE;
    LastSendedMapTick = 0;
    RadioMessageSended = 0;
    UpdateFileIndex = -1;
    UpdateFilePortion = 0;

    CritterIsNpc = false;
    MEMORY_PROCESS( MEMORY_CLIENT, sizeof( Client ) + 40 + sizeof( Item ) * 2 );

    SETFLAG( Flags, FCRIT_PLAYER );
    pingNextTick = Timer::FastTick() + PING_CLIENT_LIFE_TIME;
    Talk.Clear();
    talkNextTick = Timer::GameTick() + PROCESS_TALK_TICK;
    LastActivityTime = Timer::FastTick();
    LastSay[ 0 ] = 0;
    LastSayEqualCount = 0;
    memzero( UID, sizeof( UID ) );
}

Client::~Client()
{
    SAFEDEL( Connection );
}

uint Client::GetIp()
{
    return Connection->Ip;
}

const char* Client::GetIpStr()
{
    return Connection->Host.c_str();
}

ushort Client::GetPort()
{
    return Connection->Port;
}

bool Client::IsOnline()
{
    return !Connection->IsDisconnected;
}

bool Client::IsOffline()
{
    return Connection->IsDisconnected;
}

void Client::Disconnect()
{
    Connection->Disconnect();
}

void Client::RemoveFromGame()
{
    CanBeRemoved = true;
}

uint Client::GetOfflineTime()
{
    return Timer::FastTick() - Connection->DisconnectTick;
}

bool Client::IsToPing()
{
    return GameState == STATE_PLAYING && Timer::FastTick() >= pingNextTick && !IS_TIMEOUT( GetTimeoutTransfer() ) && !Singleplayer;
}

void Client::PingClient()
{
    if( !pingOk )
    {
        Connection->Disconnect();
        return;
    }

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_PING;
    Connection->Bout << (uchar) PING_CLIENT;
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
    uint msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( uint ) + sizeof( ushort ) * 2 +
                   sizeof( uchar ) + sizeof( int ) + sizeof( uint ) * 6 + sizeof( uint ) +
                   ( is_npc ? sizeof( hash ) : UTF8_BUF_SIZE( MAX_NAME ) );

    PUCharVec* data;
    UIntVec*   data_sizes;
    uint       whole_data_size = cr->Props.StoreData( FLAG( cr->Flags, FCRIT_CHOSEN ) ? true : false, &data, &data_sizes );
    msg_len += sizeof( ushort ) + whole_data_size;

    BOUT_BEGIN( this );
    Connection->Bout << msg;
    Connection->Bout << msg_len;
    Connection->Bout << cr->GetId();
    Connection->Bout << cr->GetHexX();
    Connection->Bout << cr->GetHexY();
    Connection->Bout << cr->GetDir();
    Connection->Bout << cr->GetCond();
    Connection->Bout << cr->GetAnim1Life();
    Connection->Bout << cr->GetAnim1Knockout();
    Connection->Bout << cr->GetAnim1Dead();
    Connection->Bout << cr->GetAnim2Life();
    Connection->Bout << cr->GetAnim2Knockout();
    Connection->Bout << cr->GetAnim2Dead();
    Connection->Bout << cr->Flags;

    if( is_npc )
    {
        Npc* npc = (Npc*) cr;
        Connection->Bout << npc->GetProtoId();
    }
    else
    {
        Client* cl = (Client*) cr;
        Connection->Bout.Push( cl->Name.c_str(), UTF8_BUF_SIZE( MAX_NAME ) );
    }

    NET_WRITE_PROPERTIES( Connection->Bout, data, data_sizes );

    BOUT_END( this );

    if( cr != this )
        Send_MoveItem( cr, nullptr, ACTION_REFRESH, 0 );
}

void Client::Send_RemoveCritter( Critter* cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_REMOVE_CRITTER;
    Connection->Bout << cr->GetId();
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
        map = GetMap();

    if( map )
    {
        loc = map->GetLocation();
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
    Connection->Bout << NETMSG_LOADMAP;
    Connection->Bout << msg_len;
    Connection->Bout << pid_map;
    Connection->Bout << pid_loc;
    Connection->Bout << map_index_in_loc;
    Connection->Bout << map_time;
    Connection->Bout << map_rain;
    Connection->Bout << hash_tiles;
    Connection->Bout << hash_scen;
    if( map )
    {
        NET_WRITE_PROPERTIES( Connection->Bout, map_data, map_data_sizes );
        NET_WRITE_PROPERTIES( Connection->Bout, loc_data, loc_data_sizes );
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
        Connection->Bout << NETMSG_POD_PROPERTY( data_size, additional_args );
    }
    else
    {
        uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( char ) + additional_args * sizeof( uint ) + sizeof( ushort ) + data_size;
        BOUT_BEGIN( this );
        Connection->Bout << NETMSG_COMPLEX_PROPERTY;
        Connection->Bout << msg_len;
    }

    Connection->Bout << (char) type;

    switch( type )
    {
    case NetProperty::CritterItem:
        Connection->Bout << ( (Item*) entity )->GetCritId();
        Connection->Bout << entity->Id;
        break;
    case NetProperty::Critter:
        Connection->Bout << entity->Id;
        break;
    case NetProperty::MapItem:
        Connection->Bout << entity->Id;
        break;
    case NetProperty::ChosenItem:
        Connection->Bout << entity->Id;
        break;
    default:
        break;
    }

    if( is_pod )
    {
        Connection->Bout << (ushort) prop->GetRegIndex();
        Connection->Bout.Push( data, data_size );
        BOUT_END( this );
    }
    else
    {
        Connection->Bout << (ushort) prop->GetRegIndex();
        if( data_size )
            Connection->Bout.Push( data, data_size );
        BOUT_END( this );
    }
}

void Client::Send_Move( Critter* from_cr, uint move_params )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_CRITTER_MOVE;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << move_params;
    Connection->Bout << from_cr->GetHexX();
    Connection->Bout << from_cr->GetHexY();
    BOUT_END( this );
}

void Client::Send_Dir( Critter* from_cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_CRITTER_DIR;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << from_cr->GetDir();
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
    Connection->Bout << NETMSG_CRITTER_ACTION;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << action;
    Connection->Bout << action_ext;
    Connection->Bout << (bool) ( item ? true : false );
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
    Connection->Bout << NETMSG_CRITTER_MOVE_ITEM;
    Connection->Bout << msg_len;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << action;
    Connection->Bout << prev_slot;
    Connection->Bout << (bool) ( item ? true : false );
    Connection->Bout << (ushort) items.size();
    for( size_t i = 0, j = items.size(); i < j; i++ )
    {
        Item* item_ = items[ i ];
        Connection->Bout << item_->GetCritSlot();
        Connection->Bout << item_->GetId();
        Connection->Bout << item_->GetProtoId();
        NET_WRITE_PROPERTIES( Connection->Bout, items_data[ i ], items_data_sizes[ i ] );
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
    Connection->Bout << NETMSG_CRITTER_ANIMATE;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << anim1;
    Connection->Bout << anim2;
    Connection->Bout << (bool) ( item ? true : false );
    Connection->Bout << clear_sequence;
    Connection->Bout << delay_play;
    BOUT_END( this );
}

void Client::Send_SetAnims( Critter* from_cr, int cond, uint anim1, uint anim2 )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_CRITTER_SET_ANIMS;
    Connection->Bout << from_cr->GetId();
    Connection->Bout << cond;
    Connection->Bout << anim1;
    Connection->Bout << anim2;
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
    Connection->Bout << NETMSG_ADD_ITEM_ON_MAP;
    Connection->Bout << msg_len;
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    Connection->Bout << item->GetHexX();
    Connection->Bout << item->GetHexY();
    Connection->Bout << is_added;
    NET_WRITE_PROPERTIES( Connection->Bout, data, data_sizes );
    BOUT_END( this );
}

void Client::Send_EraseItemFromMap( Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_ERASE_ITEM_FROM_MAP;
    Connection->Bout << item->GetId();
    Connection->Bout << item->ViewPlaceOnMap;
    BOUT_END( this );
}

void Client::Send_AnimateItem( Item* item, uchar from_frm, uchar to_frm )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_ANIMATE_ITEM;
    Connection->Bout << item->GetId();
    Connection->Bout << from_frm;
    Connection->Bout << to_frm;
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
    Connection->Bout << NETMSG_ADD_ITEM;
    Connection->Bout << msg_len;
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    Connection->Bout << item->GetCritSlot();
    NET_WRITE_PROPERTIES( Connection->Bout, data, data_sizes );
    BOUT_END( this );
}

void Client::Send_EraseItem( Item* item )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_REMOVE_ITEM;
    Connection->Bout << item->GetId();
    BOUT_END( this );
}

void Client::Send_SomeItems( CScriptArray* items_arr, int param )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint    msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( param ) + sizeof( bool ) + sizeof( uint );

    ItemVec items;
    if( items_arr )
        Script::AssignScriptArrayInVector( items, items_arr );
    vector< PUCharVec* > items_data( items.size() );
    vector< UIntVec* >   items_data_sizes( items.size() );
    for( size_t i = 0, j = items.size(); i < j; i++ )
    {
        uint whole_data_size = items[ i ]->Props.StoreData( false, &items_data[ i ], &items_data_sizes[ i ] );
        msg_len += sizeof( uint ) + sizeof( hash ) + sizeof( ushort ) + whole_data_size;
    }

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_SOME_ITEMS;
    Connection->Bout << msg_len;
    Connection->Bout << param;
    Connection->Bout << ( items_arr == nullptr );
    Connection->Bout << (uint) items.size();
    for( size_t i = 0, j = items.size(); i < j; i++ )
    {
        Item* item = items[ i ];
        Connection->Bout << item->GetId();
        Connection->Bout << item->GetProtoId();
        NET_WRITE_PROPERTIES( Connection->Bout, items_data[ i ], items_data_sizes[ i ] );
    }
    BOUT_END( this );
}

#define SEND_LOCATION_SIZE    ( sizeof( uint ) + sizeof( hash ) + sizeof( ushort ) * 2 + sizeof( ushort ) + sizeof( uint ) + sizeof( uchar ) )
void Client::Send_GlobalInfo( uchar info_flags )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( LockMapTransfers )
        return;

    CScriptArray* known_locs = GetKnownLocations();

    // Calculate length of message
    uint   msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( info_flags );

    ushort loc_count = (ushort) known_locs->GetSize();
    if( FLAG( info_flags, GM_INFO_LOCATIONS ) )
        msg_len += sizeof( loc_count ) + SEND_LOCATION_SIZE * loc_count;

    if( FLAG( info_flags, GM_INFO_ZONES_FOG ) )
        msg_len += GM_ZONES_FOG_SIZE;

    // Parse message
    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_GLOBAL_INFO;
    Connection->Bout << msg_len;
    Connection->Bout << info_flags;

    if( FLAG( info_flags, GM_INFO_LOCATIONS ) )
    {
        Connection->Bout << loc_count;

        char empty_loc[ SEND_LOCATION_SIZE ] = { 0, 0, 0, 0 };
        for( ushort i = 0; i < loc_count;)
        {
            uint      loc_id = *(uint*) known_locs->At( i );
            Location* loc = MapMngr.GetLocation( loc_id );
            if( loc && !loc->GetToGarbage() )
            {
                i++;
                Connection->Bout << loc_id;
                Connection->Bout << loc->GetProtoId();
                Connection->Bout << loc->GetWorldX();
                Connection->Bout << loc->GetWorldY();
                Connection->Bout << loc->GetRadius();
                Connection->Bout << loc->GetColor();
                uchar count = 0;
                if( loc->IsNonEmptyMapEntrances() )
                {
                    CScriptArray* map_entrances = loc->GetMapEntrances();
                    count = (uchar) ( map_entrances->GetSize() / 2 );
                    map_entrances->Release();
                }
                Connection->Bout << count;
            }
            else
            {
                loc_count--;
                EraseKnownLoc( loc_id );
                Connection->Bout.Push( empty_loc, sizeof( empty_loc ) );
            }
        }
    }
    SAFEREL( known_locs );

    if( FLAG( info_flags, GM_INFO_ZONES_FOG ) )
    {
        CScriptArray* gmap_fog = GetGlobalMapFog();
        if( gmap_fog->GetSize() != GM_ZONES_FOG_SIZE )
            gmap_fog->Resize( GM_ZONES_FOG_SIZE );
        Connection->Bout.Push( gmap_fog->At( 0 ), GM_ZONES_FOG_SIZE );
        gmap_fog->Release();
    }
    BOUT_END( this );

    if( FLAG( info_flags, GM_INFO_CRITTERS ) )
        ProcessVisibleCritters();
}

void Client::Send_GlobalLocation( Location* loc, bool add )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uchar info_flags = GM_INFO_LOCATION;
    uint  msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( info_flags ) + sizeof( add ) + SEND_LOCATION_SIZE;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_GLOBAL_INFO;
    Connection->Bout << msg_len;
    Connection->Bout << info_flags;
    Connection->Bout << add;
    Connection->Bout << loc->GetId();
    Connection->Bout << loc->GetProtoId();
    Connection->Bout << loc->GetWorldX();
    Connection->Bout << loc->GetWorldY();
    Connection->Bout << loc->GetRadius();
    Connection->Bout << loc->GetColor();
    uchar count = 0;
    if( loc->IsNonEmptyMapEntrances() )
    {
        CScriptArray* map_entrances = loc->GetMapEntrances();
        count = (uchar) ( map_entrances->GetSize() / 2 );
        map_entrances->Release();
    }
    Connection->Bout << count;
    BOUT_END( this );
}

void Client::Send_GlobalMapFog( ushort zx, ushort zy, uchar fog )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uchar info_flags = GM_INFO_FOG;
    uint  msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( info_flags ) + sizeof( zx ) + sizeof( zy ) + sizeof( fog );

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_GLOBAL_INFO;
    Connection->Bout << msg_len;
    Connection->Bout << info_flags;
    Connection->Bout << zx;
    Connection->Bout << zy;
    Connection->Bout << fog;
    BOUT_END( this );
}

void Client::Send_XY( Critter* cr )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_CRITTER_XY;
    Connection->Bout << cr->GetId();
    Connection->Bout << cr->GetHexX();
    Connection->Bout << cr->GetHexY();
    Connection->Bout << cr->GetDir();
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
    Connection->Bout << NETMSG_ALL_PROPERTIES;
    Connection->Bout << msg_len;
    NET_WRITE_PROPERTIES( Connection->Bout, data, data_sizes );
    BOUT_END( this );
}

void Client::Send_CustomCommand( Critter* cr, ushort cmd, int val )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_CUSTOM_COMMAND;
    Connection->Bout << cr->GetId();
    Connection->Bout << cmd;
    Connection->Bout << val;
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
    Connection->Bout << NETMSG_TALK_NPC;

    if( close )
    {
        Connection->Bout << msg_len;
        Connection->Bout << is_npc;
        Connection->Bout << talk_id;
        Connection->Bout << uchar( 0 );
    }
    else
    {
        uchar all_answers = (uchar) Talk.CurDialog.Answers.size();
        msg_len += sizeof( uint ) + sizeof( uint ) * all_answers + sizeof( uint ) + sizeof( ushort ) + (uint) Talk.Lexems.length();

        uint talk_time = Talk.TalkTime;
        if( talk_time )
        {
            uint diff = Timer::GameTick() - Talk.StartTick;
            talk_time = ( diff < talk_time ? talk_time - diff : 1 );
        }

        Connection->Bout << msg_len;
        Connection->Bout << is_npc;
        Connection->Bout << talk_id;
        Connection->Bout << all_answers;
        Connection->Bout << (ushort) Talk.Lexems.length();                              // Lexems length
        if( Talk.Lexems.length() )
            Connection->Bout.Push( Talk.Lexems.c_str(), (uint) Talk.Lexems.length() );  // Lexems string
        Connection->Bout << Talk.CurDialog.TextId;                                      // Main text_id
        for( auto it = Talk.CurDialog.Answers.begin(), end = Talk.CurDialog.Answers.end(); it != end; ++it )
            Connection->Bout << it->TextId;                                             // Answers text_id
        Connection->Bout << talk_time;                                                  // Talk time
    }
    BOUT_END( this );
}

void Client::Send_GameInfo( Map* map )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    int           time = ( map ? map->GetCurDayTime() : -1 );
    uchar         rain = ( map ? map->GetRainCapacity() : 0 );
    bool          no_log_out = ( map ? map->GetIsNoLogOut() : true );

    int           day_time[ 4 ];
    uchar         day_color[ 12 ];
    CScriptArray* day_time_arr = ( map ? map->GetDayTime() : nullptr );
    CScriptArray* day_color_arr = ( map ? map->GetDayColor() : nullptr );
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

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_GAME_INFO;
    Connection->Bout << GameOpt.YearStart;
    Connection->Bout << GameOpt.Year;
    Connection->Bout << GameOpt.Month;
    Connection->Bout << GameOpt.Day;
    Connection->Bout << GameOpt.Hour;
    Connection->Bout << GameOpt.Minute;
    Connection->Bout << GameOpt.Second;
    Connection->Bout << GameOpt.TimeMultiplier;
    Connection->Bout << time;
    Connection->Bout << rain;
    Connection->Bout << no_log_out;
    Connection->Bout.Push( day_time, sizeof( day_time ) );
    Connection->Bout.Push( day_color, sizeof( day_color ) );
    BOUT_END( this );
}

void Client::Send_Text( Critter* from_cr, const string& text, uchar how_say )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( text.empty() )
        return;

    uint from_id = ( from_cr ? from_cr->GetId() : 0 );
    Send_TextEx( from_id, text, how_say, false );
}

void Client::Send_TextEx( uint from_id, const string& text, uchar how_say, bool unsafe_text )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( from_id ) + sizeof( how_say ) +
                   BufferManager::StringLenSize + (uint) text.length() + sizeof( unsafe_text );

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_CRITTER_TEXT;
    Connection->Bout << msg_len;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << text;
    Connection->Bout << unsafe_text;
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
    Connection->Bout << NETMSG_MSG;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    BOUT_END( this );
}

void Client::Send_TextMsg( uint from_id, uint num_str, uchar how_say, ushort num_msg )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !num_str )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_MSG;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    BOUT_END( this );
}

void Client::Send_TextMsgLex( Critter* from_cr, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !num_str )
        return;

    ushort lex_len = (ushort) strlen( lexems );
    if( !lex_len || lex_len > MAX_DLG_LEXEMS_TEXT )
    {
        Send_TextMsg( from_cr, num_str, how_say, num_msg );
        return;
    }

    uint from_id = ( from_cr ? from_cr->GetId() : 0 );
    uint msg_len = NETMSG_MSG_SIZE + sizeof( lex_len ) + lex_len;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_MSG_LEX;
    Connection->Bout << msg_len;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lex_len;
    Connection->Bout.Push( lexems, lex_len );
    BOUT_END( this );
}

void Client::Send_TextMsgLex( uint from_id, uint num_str, uchar how_say, ushort num_msg, const char* lexems )
{
    if( IsSendDisabled() || IsOffline() )
        return;
    if( !num_str )
        return;

    ushort lex_len = (ushort) strlen( lexems );
    if( !lex_len || lex_len > MAX_DLG_LEXEMS_TEXT )
    {
        Send_TextMsg( from_id, num_str, how_say, num_msg );
        return;
    }

    uint msg_len = NETMSG_MSG_SIZE + sizeof( lex_len ) + lex_len;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_MSG_LEX;
    Connection->Bout << msg_len;
    Connection->Bout << from_id;
    Connection->Bout << how_say;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lex_len;
    Connection->Bout.Push( lexems, lex_len );
    BOUT_END( this );
}

void Client::Send_MapText( ushort hx, ushort hy, uint color, const string& text, bool unsafe_text )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( hx ) + sizeof( hy ) + sizeof( color ) +
                   BufferManager::StringLenSize + (uint) text.length() + sizeof( unsafe_text );

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_MAP_TEXT;
    Connection->Bout << msg_len;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << text;
    Connection->Bout << unsafe_text;
    BOUT_END( this );
}

void Client::Send_MapTextMsg( ushort hx, ushort hy, uint color, ushort num_msg, uint num_str )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_MAP_TEXT_MSG;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    BOUT_END( this );
}

void Client::Send_MapTextMsgLex( ushort hx, ushort hy, uint color, ushort num_msg, uint num_str, const char* lexems, ushort lexems_len )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( ushort ) * 2 + sizeof( uint ) + sizeof( ushort ) + sizeof( uint ) + sizeof( lexems_len ) + lexems_len;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_MAP_TEXT_MSG_LEX;
    Connection->Bout << msg_len;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << color;
    Connection->Bout << num_msg;
    Connection->Bout << num_str;
    Connection->Bout << lexems_len;
    if( lexems_len )
        Connection->Bout.Push( lexems, lexems_len );
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
    Connection->Bout << NETMSG_COMBAT_RESULTS;
    Connection->Bout << msg_len;
    Connection->Bout << len;
    if( len )
        Connection->Bout.Push( combat_res, len * sizeof( uint ) );
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
                CScriptArray* automaps = loc_->GetAutomaps();
                msg_len += ( sizeof( hash ) + sizeof( uchar ) ) * (uint) automaps->GetSize();
                automaps->Release();
            }
        }

        BOUT_BEGIN( this );
        Connection->Bout << NETMSG_AUTOMAPS_INFO;
        Connection->Bout << msg_len;
        Connection->Bout << (bool) true;        // Clear list
        Connection->Bout << (ushort) locs->size();
        for( uint i = 0, j = (uint) locs->size(); i < j; i++ )
        {
            Location* loc_ = ( *locs )[ i ];
            Connection->Bout << loc_->GetId();
            Connection->Bout << loc_->GetProtoId();
            if( loc_->IsNonEmptyAutomaps() )
            {
                CScriptArray* automaps = loc_->GetAutomaps();
                Connection->Bout << (ushort) automaps->GetSize();
                for( uint k = 0, l = (uint) automaps->GetSize(); k < l; k++ )
                {
                    hash pid = *(hash*) automaps->At( k );
                    Connection->Bout << pid;
                    Connection->Bout << (uchar) loc_->GetMapIndex( pid );
                }
                automaps->Release();
            }
            else
            {
                Connection->Bout << (ushort) 0;
            }

        }
        BOUT_END( this );
    }

    if( loc )
    {
        uint          msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( bool ) + sizeof( ushort ) +
                                sizeof( uint ) + sizeof( hash ) + sizeof( ushort );
        CScriptArray* automaps = ( loc->IsNonEmptyAutomaps() ? loc->GetAutomaps() : nullptr );
        if( automaps )
            msg_len += ( sizeof( hash ) + sizeof( uchar ) ) * (uint) automaps->GetSize();

        BOUT_BEGIN( this );
        Connection->Bout << NETMSG_AUTOMAPS_INFO;
        Connection->Bout << msg_len;
        Connection->Bout << (bool) false;        // Append this information
        Connection->Bout << (ushort) 1;
        Connection->Bout << loc->GetId();
        Connection->Bout << loc->GetProtoId();
        Connection->Bout << (ushort) ( automaps ? automaps->GetSize() : 0 );
        if( automaps )
        {
            for( uint i = 0, j = (uint) automaps->GetSize(); i < j; i++ )
            {
                hash pid = *(hash*) automaps->At( i );
                Connection->Bout << pid;
                Connection->Bout << (uchar) loc->GetMapIndex( pid );
            }
        }
        BOUT_END( this );

        if( automaps )
            automaps->Release();
    }
}

void Client::Send_Effect( hash eff_pid, ushort hx, ushort hy, ushort radius )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_EFFECT;
    Connection->Bout << eff_pid;
    Connection->Bout << hx;
    Connection->Bout << hy;
    Connection->Bout << radius;
    BOUT_END( this );
}

void Client::Send_FlyEffect( hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_FLY_EFFECT;
    Connection->Bout << eff_pid;
    Connection->Bout << from_crid;
    Connection->Bout << to_crid;
    Connection->Bout << from_hx;
    Connection->Bout << from_hy;
    Connection->Bout << to_hx;
    Connection->Bout << to_hy;
    BOUT_END( this );
}

void Client::Send_PlaySound( uint crid_synchronize, const string& sound_name )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( crid_synchronize ) +
                   BufferManager::StringLenSize + (uint) sound_name.length();

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_PLAY_SOUND;
    Connection->Bout << msg_len;
    Connection->Bout << crid_synchronize;
    Connection->Bout << sound_name;
    BOUT_END( this );
}

void Client::Send_ViewMap()
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << NETMSG_VIEW_MAP;
    Connection->Bout << ViewMapHx;
    Connection->Bout << ViewMapHy;
    Connection->Bout << ViewMapLocId;
    Connection->Bout << ViewMapLocEnt;
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
    Connection->Bout << msg;
    Connection->Bout << msg_len;
    Connection->Bout << uid[ 3 ];
    Connection->Bout << uidxor[ 0 ];
    Connection->Bout << rnd_count;
    Connection->Bout << uid[ 1 ];
    Connection->Bout << uidxor[ 2 ];
    for( int i = 0; i < rnd_count; i++ )
        Connection->Bout << (uchar) Random( 0, 255 );
    Connection->Bout << uid[ 2 ];
    Connection->Bout << uidxor[ 1 ];
    Connection->Bout << uid[ 4 ];
    Connection->Bout << rnd_count2;
    Connection->Bout << uidxor[ 3 ];
    Connection->Bout << uidxor[ 4 ];
    Connection->Bout << uid[ 0 ];
    for( int i = 0; i < rnd_count2; i++ )
        Connection->Bout << (uchar) Random( 0, 255 );
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
    Connection->Bout << NETMSG_SOME_ITEM;
    Connection->Bout << msg_len;
    Connection->Bout << item->GetId();
    Connection->Bout << item->GetProtoId();
    NET_WRITE_PROPERTIES( Connection->Bout, data, data_sizes );
    BOUT_END( this );
}

void Client::Send_CustomMessage( uint msg )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    BOUT_BEGIN( this );
    Connection->Bout << msg;
    BOUT_END( this );
}

void Client::Send_CustomMessage( uint msg, uchar* data, uint data_size )
{
    if( IsSendDisabled() || IsOffline() )
        return;

    uint msg_len = sizeof( msg ) + sizeof( msg_len ) + data_size;

    BOUT_BEGIN( this );
    Connection->Bout << msg;
    Connection->Bout << msg_len;
    if( data_size )
        Connection->Bout.Push( data, data_size );
    BOUT_END( this );
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
    if( Talk.TalkTime && Timer::GameTick() - Talk.StartTick > Talk.TalkTime )
    {
        CloseTalk();
        return;
    }

    // Check npc
    Npc* npc = nullptr;
    if( Talk.TalkType == TALK_WITH_NPC )
    {
        npc = CrMngr.GetNpc( Talk.TalkNpc );
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

        // Todo: IsPlaneNoTalk
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
            npc = CrMngr.GetNpc( Talk.TalkNpc );
            if( npc )
            {
                if( Talk.Barter )
                    Script::RaiseInternalEvent( ServerFunctions.CritterBarter, this, npc, false, npc->GetBarterPlayers() );
                Script::RaiseInternalEvent( ServerFunctions.CritterTalk, this, npc, false, npc->GetTalkedPlayers() );
            }
        }

        if( Talk.CurDialog.DlgScript )
        {
            Script::PrepareContext( Talk.CurDialog.DlgScript, GetName() );
            Script::SetArgEntity( this );
            Script::SetArgEntity( npc );
            Script::SetArgEntity( nullptr );
            Talk.Locked = true;
            Script::RunPrepared();
            Talk.Locked = false;
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
    CritterIsNpc = true;
    MEMORY_PROCESS( MEMORY_NPC, sizeof( Npc ) + 40 + sizeof( Item ) * 2 );
    SETFLAG( Flags, FCRIT_NPC );
}

Npc::~Npc()
{
    MEMORY_PROCESS( MEMORY_NPC, -(int) ( sizeof( Npc ) + 40 + sizeof( Item ) * 2 ) );
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
