#include "Common.h"
#include "Server.h"

void FOServer::ProcessCritter( Critter* cr )
{
    if( cr->CanBeRemoved || cr->IsDestroyed )
        return;
    if( Timer::IsGamePaused() )
        return;

    uint tick = Timer::GameTick();

    // Idle global function
    if( tick >= cr->GlobalIdleNextTick )
    {
        if( Script::PrepareContext( ServerFunctions.CritterIdle, _FUNC_, cr->GetInfo() ) )
        {
            Script::SetArgEntity( cr );
            Script::RunPrepared();
        }
        cr->GlobalIdleNextTick = tick + GameOpt.CritterIdleTick;
    }

    // Ap regeneration
    int max_ap = cr->GetActionPoints() * AP_DIVIDER;
    if( cr->IsFree() && cr->GetRealAp() < max_ap && !cr->IsTurnBased() )
    {
        if( !cr->ApRegenerationTick )
            cr->ApRegenerationTick = tick;
        else
        {
            uint delta = tick - cr->ApRegenerationTick;
            if( delta >= 500 )
            {
                cr->SetCurrentAp( cr->GetCurrentAp() + max_ap * delta / GameOpt.ApRegeneration );
                if( cr->GetCurrentAp() > max_ap )
                    cr->SetCurrentAp( max_ap );
                cr->ApRegenerationTick = tick;
            }
        }
    }
    if( cr->GetCurrentAp() > max_ap )
        cr->SetCurrentAp( max_ap );

    // Internal misc/drugs time events
    // One event per cycle
    if( cr->IsTE_FuncNum() )
    {
        ScriptArray* te_next_time = cr->GetTE_NextTime();
        uint         next_time = *(uint*) te_next_time->At( 0 );
        if( !next_time || ( !cr->IsTurnBased() && GameOpt.FullSecond >= next_time ) )
        {
            ScriptArray* te_func_num = cr->GetTE_FuncNum();
            ScriptArray* te_rate = cr->GetTE_Rate();
            ScriptArray* te_identifier = cr->GetTE_Identifier();
            RUNTIME_ASSERT( te_next_time->GetSize() == te_func_num->GetSize() );
            RUNTIME_ASSERT( te_func_num->GetSize() == te_rate->GetSize() );
            RUNTIME_ASSERT( te_rate->GetSize() == te_identifier->GetSize() );
            hash func_num = *(hash*) te_func_num->At( 0 );
            uint rate = *(hash*) te_rate->At( 0 );
            int  identifier = *(hash*) te_identifier->At( 0 );
            te_func_num->Release();
            te_rate->Release();
            te_identifier->Release();

            cr->EraseCrTimeEvent( 0 );

            uint time = GameOpt.TimeMultiplier * 1800;             // 30 minutes on error
            if( Script::PrepareScriptFuncContext( func_num, _FUNC_, cr->GetInfo() ) )
            {
                Script::SetArgEntity( cr );
                Script::SetArgUInt( identifier );
                Script::SetArgAddress( &rate );
                if( Script::RunPrepared() )
                    time = Script::GetReturnedUInt();
            }
            if( time )
                cr->AddCrTimeEvent( func_num, rate, time, identifier );
        }
        te_next_time->Release();
    }

    // Global map
    if( !cr->GetMapId() && cr->GroupMove && cr == cr->GroupMove->Rule )
        MapMngr.GM_GroupMove( cr->GroupMove );

    // Client
    if( cr->IsPlayer() )
    {
        // Cast
        Client* cl = (Client*) cr;

        // Talk
        cl->ProcessTalk( false );

        // Ping client
        if( cl->IsToPing() )
        {
            #ifndef DEV_VERSION
            if( GameOpt.AccountPlayTime && Random( 0, 3 ) == 0 )
                cl->Send_CheckUIDS();
            #endif
            cl->PingClient();
            if( cl->GetMapId() )
                MapMngr.TryTransitCrGrid( cr, MapMngr.GetMap( cr->GetMapId() ), cr->GetHexX(), cr->GetHexY(), false );
        }

        // Idle
        if( cl->FuncId[ CRITTER_EVENT_IDLE ] > 0 && cl->IsLife() && !cl->IsWait() && cl->IsFree() )
        {
            cl->EventIdle();
            if( !cl->IsWait() )
                cl->SetWait( GameOpt.CritterIdleTick );
        }

        // Kick from game
        if( cl->IsOffline() && cl->IsLife() && !IS_TIMEOUT( cl->GetTimeoutBattle() ) &&
            !cl->GetTimeoutRemoveFromGame() && cl->GetOfflineTime() >= GameOpt.MinimumOfflineTime &&
            !MapMngr.IsProtoMapNoLogOut( cl->GetMapPid() ) )
        {
            cl->RemoveFromGame();
        }

        // Cache intelligence for GetSayIntellect, every 3 seconds
        if( tick >= cl->CacheValuesNextTick )
        {
            cl->IntellectCacheValue = ( tick & 0xFFF0 ) | cl->GetIntellect();
            cl->LookCacheValue = cl->GetLookDistance();
            cl->CacheValuesNextTick = tick + 3000;
        }
    }
    // Npc
    else
    {
        // Cast
        Npc* npc = (Npc*) cr;

        // Process
        if( npc->IsLife() )
        {
            ProcessAI( npc );
            if( npc->IsNeedRefreshBag() )
                npc->RefreshBag();
        }
    }

    // Knockout
    if( cr->IsKnockout() )
        cr->TryUpOnKnockout();
}

void FOServer::SaveHoloInfoFile( IniParser& data )
{
    data.SetStr( "GeneralSettings", "LastHoloId", Str::UItoA( LastHoloId ) );

    for( auto& holo : HolodiskInfo )
    {
        StrMap& kv = data.SetApp( "HoloInfo" );
        kv[ "Id" ] = holo.first;
        kv[ "CanWrite" ] = holo.second->CanRewrite;
        kv[ "Title" ] = holo.second->Title;
        kv[ "Text" ] = holo.second->Text;
    }
}

bool FOServer::LoadHoloInfoFile( IniParser& data )
{
    LastHoloId = Str::AtoUI( data.GetStr( "GeneralSettings", "LastHoloId" ) );

    PStrMapVec holos;
    data.GetApps( "HoloInfo", holos );
    for( auto& pkv : holos )
    {
        auto&       kv = *pkv;
        uint        id = Str::AtoUI( kv[ "Id" ].c_str() );
        bool        can_rw = Str::CompareCase( kv[ "CanWrite" ].c_str(), "True" );
        const char* title = kv[ "Title" ].c_str();
        const char* text = kv[ "Text" ].c_str();
        HolodiskInfo.insert( PAIR( id, new HoloInfo( can_rw, title, text ) ) );
    }
    return true;
}

void FOServer::AddPlayerHoloInfo( Critter* cr, uint holo_num, bool send )
{
    if( !holo_num )
    {
        if( send )
            cr->Send_TextMsg( cr, STR_HOLO_READ_FAIL, SAY_NETMSG, TEXTMSG_HOLO );
        return;
    }

    ScriptArray* holo_info = cr->GetHoloInfo();
    uint         holo_count = holo_info->GetSize();
    if( holo_count >= MAX_HOLO_INFO )
    {
        if( send )
            cr->Send_TextMsg( cr, STR_HOLO_READ_MEMORY_FULL, SAY_NETMSG, TEXTMSG_HOLO );
        SAFEREL( holo_info );
        return;
    }

    for( uint i = 0; i < holo_count; i++ )
    {
        if( *(uint*) holo_info->At( i ) == holo_num )
        {
            if( send )
                cr->Send_TextMsg( cr, STR_HOLO_READ_ALREADY, SAY_NETMSG, TEXTMSG_HOLO );
            SAFEREL( holo_info );
            return;
        }
    }

    holo_info->InsertLast( &holo_num );
    cr->SetHoloInfo( holo_info );
    SAFEREL( holo_info );

    if( send )
    {
        if( holo_num >= USER_HOLO_START_NUM )
            Send_PlayerHoloInfo( cr, holo_num, true );
        cr->Send_HoloInfo( false, holo_count, 1 );
        cr->Send_TextMsg( cr, STR_HOLO_READ_SUCC, SAY_NETMSG, TEXTMSG_HOLO );
    }

    // Cancel rewrite
    if( holo_num >= USER_HOLO_START_NUM )
    {
        SCOPE_LOCK( HolodiskLocker );

        HoloInfo* hi = GetHoloInfo( holo_num );
        if( hi && hi->CanRewrite )
            hi->CanRewrite = false;
    }
}

void FOServer::ErasePlayerHoloInfo( Critter* cr, uint index, bool send )
{
    ScriptArray* holo_info = cr->GetHoloInfo();
    if( index >= holo_info->GetSize() )
    {
        if( send )
            cr->Send_TextMsg( cr, STR_HOLO_ERASE_FAIL, SAY_NETMSG, TEXTMSG_HOLO );
        SAFEREL( holo_info );
        return;
    }

    holo_info->RemoveAt( index );
    cr->SetHoloInfo( holo_info );
    SAFEREL( holo_info );

    if( send )
    {
        cr->Send_HoloInfo( true, 0, 0 );
        cr->Send_TextMsg( cr, STR_HOLO_ERASE_SUCC, SAY_NETMSG, TEXTMSG_HOLO );
    }
}

void FOServer::Send_PlayerHoloInfo( Critter* cr, uint holo_num, bool send_text )
{
    if( !cr->IsPlayer() )
        return;

    HolodiskLocker.Lock();

    Client*   cl = (Client*) cr;
    HoloInfo* hi = GetHoloInfo( holo_num );
    if( hi )
    {
        string str = ( send_text ? hi->Text : hi->Title ); // Copy

        HolodiskLocker.Unlock();

        cl->Send_UserHoloStr( send_text ? STR_HOLO_INFO_DESC( holo_num ) : STR_HOLO_INFO_NAME( holo_num ), str.c_str(), (ushort) str.length() );
    }
    else
    {
        HolodiskLocker.Unlock();

        cl->Send_UserHoloStr( send_text ? STR_HOLO_INFO_DESC( holo_num ) : STR_HOLO_INFO_NAME( holo_num ), "Truncated", Str::Length( "Truncated" ) );
    }
}

bool FOServer::Act_Move( Critter* cr, ushort hx, ushort hy, uint move_params )
{
    uint map_id = cr->GetMapId();
    if( !map_id )
        return false;

    // Check run/walk
    bool is_run = FLAG( move_params, MOVE_PARAM_RUN );
    if( is_run && !cr->IsCanRun() )
    {
        // Switch to walk
        move_params ^= MOVE_PARAM_RUN;
        is_run = false;
    }
    if( !is_run && !cr->IsCanWalk() )
        return false;

    // Check
    Map* map = MapMngr.GetMap( map_id, true );
    if( !map || map_id != cr->GetMapId() || hx >= map->GetWidth() || hy >= map->GetHeight() )
        return false;

    // Check turn based
    if( !cr->CheckMyTurn( map ) )
    {
        cr->Send_XY( cr );
        cr->Send_CustomCommand( cr, OTHER_YOU_TURN, 0 );
        WriteLogF( _FUNC_, " - Is not critter '%s' turn.\n", cr->GetInfo() );
        return false;
    }

    if( map->IsTurnBasedOn )
    {
        int ap_cost = cr->GetApCostCritterMove( is_run ) / AP_DIVIDER;
        int move_ap = cr->GetMoveAp();
        if( ap_cost )
        {
            if( ( cr->GetCurrentAp() / AP_DIVIDER + move_ap ) / ap_cost <= 0 )
            {
                cr->Send_XY( cr );
                return false;
            }
            int steps = ( cr->GetCurrentAp() / AP_DIVIDER + move_ap ) / ap_cost - 1;
            if( steps < MOVE_PARAM_STEP_COUNT )
                move_params |= ( MOVE_PARAM_STEP_DISALLOW << ( steps * MOVE_PARAM_STEP_BITS ) );                               // Cut steps
            if( move_ap )
            {
                if( ap_cost > move_ap )
                {
                    cr->SubMoveAp( move_ap );
                    cr->SubAp( ap_cost - move_ap );
                }
                else
                    cr->SubMoveAp( ap_cost );
            }
            else
                cr->SubAp( ap_cost );
            if( cr->GetAllAp() <= 0 )
                map->EndCritterTurn();
        }
    }
    else if( IS_TIMEOUT( cr->GetTimeoutBattle() ) )
    {
        int ap_cost = cr->GetApCostCritterMove( is_run );
        if( cr->GetRealAp() < ap_cost && !Singleplayer )
        {
            cr->Send_XY( cr );
            return false;
        }
        if( ap_cost )
        {
            int steps = cr->GetRealAp() / ap_cost - 1;
            if( steps < MOVE_PARAM_STEP_COUNT )
                move_params |= ( MOVE_PARAM_STEP_DISALLOW << ( steps * MOVE_PARAM_STEP_BITS ) );                               // Cut steps
            cr->SetCurrentAp( cr->GetCurrentAp() - ap_cost );
            cr->ApRegenerationTick = 0;
        }
    }

    // Check passed
    ushort fx = cr->GetHexX();
    ushort fy = cr->GetHexY();
    uchar  dir = GetNearDir( fx, fy, hx, hy );
    uint   multihex = cr->GetMultihex();

    if( !map->IsMovePassed( hx, hy, dir, multihex ) )
    {
        if( cr->IsPlayer() )
        {
            cr->Send_XY( cr );
            Critter* cr_hex = map->GetHexCritter( hx, hy, false, false );
            if( cr_hex )
                cr->Send_XY( cr_hex );
        }
        return false;
    }

    // Set last move type
    cr->IsRuning = is_run;

    // Process step
    bool is_dead = cr->IsDead();
    map->UnsetFlagCritter( fx, fy, multihex, is_dead );
    cr->PrevHexX = fx;
    cr->PrevHexY = fy;
    cr->PrevHexTick = Timer::GameTick();
    cr->SetHexX( hx );
    cr->SetHexY( hy );
    map->SetFlagCritter( hx, hy, multihex, is_dead );

    // Set dir
    cr->SetDir( dir );

    if( is_run )
    {
        if( !cr->GetPerkSilentRunning() && cr->GetIsHide() )
            cr->SetIsHide( false );
        cr->SetBreakTimeDelta( cr->GetTimeRun() );
    }
    else
    {
        cr->SetBreakTimeDelta( cr->GetTimeWalk() );
    }

    cr->SendA_Move( move_params );
    cr->ProcessVisibleCritters();
    cr->ProcessVisibleItems();

    if( cr->GetMapId() == map->GetId() )
    {
        // Triggers
        VerifyTrigger( map, cr, fx, fy, hx, hy, dir );

        // Out trap
        if( map->IsHexTrap( fx, fy ) )
        {
            ItemVec traps;
            map->GetItemsTrap( fx, fy, traps, true );
            for( auto it = traps.begin(), end = traps.end(); it != end; ++it )
                ( *it )->EventWalk( cr, false, dir );
        }

        // In trap
        if( map->IsHexTrap( hx, hy ) )
        {
            ItemVec traps;
            map->GetItemsTrap( hx, hy, traps, true );
            for( auto it = traps.begin(), end = traps.end(); it != end; ++it )
                ( *it )->EventWalk( cr, true, dir );
        }

        // Try transit
        MapMngr.TryTransitCrGrid( cr, map, cr->GetHexX(), cr->GetHexY(), false );
    }

    return true;
}

bool FOServer::Act_Attack( Critter* cr, uchar rate_weap, uint target_id )
{
/************************************************************************/
/* Check & Prepare                                                      */
/************************************************************************/
    cr->SetBreakTime( GameOpt.Breaktime );

    if( cr->GetId() == target_id )
    {
        WriteLogF( _FUNC_, " - Critter '%s' self attack.\n", cr->GetInfo() );
        return false;
    }

    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( !map )
    {
        WriteLogF( _FUNC_, " - Map not found, map id %u, critter '%s'.\n", cr->GetMapId(), cr->GetInfo() );
        return false;
    }

    if( !cr->CheckMyTurn( map ) )
    {
        WriteLogF( _FUNC_, " - Is not critter '%s' turn.\n", cr->GetInfo() );
        return false;
    }

    Critter* t_cr = cr->GetCritSelf( target_id, true );
    if( !t_cr )
    {
        WriteLogF( _FUNC_, " - Target critter not found, target id %u, critter '%s'.\n", target_id, cr->GetInfo() );
        return false;
    }

    if( cr->GetMapId() != t_cr->GetMapId() )
    {
        WriteLogF( _FUNC_, " - Other maps, critter '%s', target critter '%s'.\n", cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    if( t_cr->IsDead() )
    {
        // if(cr->IsPlayer()) WriteLogF(_FUNC_," - Target critter is dead, critter '%s', target critter '%s'.\n",cr->GetInfo(),t_cr->GetInfo());
        cr->Send_AddCritter( t_cr );       // Refresh
        return false;
    }

    int hx = cr->GetHexX();
    int hy = cr->GetHexY();
    int tx = t_cr->GetHexX();
    int ty = t_cr->GetHexY();

    // Get weapon, ammo and armor
    Item* weap = cr->ItemSlotMain;
    if( !weap->IsWeapon() )
    {
        WriteLogF( _FUNC_, " - Critter item is not weapon, critter '%s', target critter '%s'.\n", cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    if( weap->IsBroken() )
    {
        WriteLogF( _FUNC_, " - Critter weapon is broken, critter '%s', target critter '%s'.\n", cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    if( weap->GetWeapon_IsTwoHanded() && cr->IsDmgArm() )
    {
        WriteLogF( _FUNC_, " - Critter is damaged arm on two hands weapon, critter '%s', target critter '%s'.\n", cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    if( cr->IsDmgTwoArm() && weap->GetId() )
    {
        WriteLogF( _FUNC_, " - Critter is damaged two arms on armed attack, critter '%s', target critter '%s'.\n", cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    uchar aim = rate_weap >> 4;
    uchar use = rate_weap & 0xF;

    if( use >= MAX_USES )
    {
        WriteLogF( _FUNC_, " - Use %u invalid value, critter '%s', target critter '%s'.\n", use, cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    if( !( weap->GetWeapon_ActiveUses() & ( 1 << use ) ) )
    {
        WriteLogF( _FUNC_, " - Use %u is not aviable, critter '%s', target critter '%s'.\n", use, cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    if( aim >= MAX_HIT_LOCATION )
    {
        WriteLogF( _FUNC_, " - Aim %u invalid value, critter '%s', target critter '%s'.\n", aim, cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    if( aim && !CritType::IsCanAim( cr->GetCrType() ) )
    {
        WriteLogF( _FUNC_, " - Aim is not available for this critter type, crtype %u, aim %u, critter '%s', target critter '%s'.\n", cr->GetCrType(), aim, cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    if( aim && cr->GetIsNoAim() )
    {
        WriteLogF( _FUNC_, " - Aim is not available with critter no aim mode, aim %u, critter '%s', target critter '%s'.\n", aim, cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    if( aim && !weap->WeapIsCanAim( use ) )
    {
        WriteLogF( _FUNC_, " - Aim is not available for this weapon, aim %u, weapon '%s', critter '%s', target critter '%s'.\n", aim, weap->GetName(), cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    if( !CritType::IsAnim1( cr->GetCrType(), weap->GetWeapon_Anim1() ) )
    {
        WriteLogF( _FUNC_, " - Anim1 is not available for this critter type, crtype %u, anim1 %d, critter '%s', target critter '%s'.\n", cr->GetCrType(), weap->GetWeapon_Anim1(), cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    uint max_dist = cr->GetAttackDist( weap, use ) + t_cr->GetMultihex();
    if( !CheckDist( hx, hy, tx, ty, max_dist ) && !( Timer::GameTick() < t_cr->PrevHexTick + 500 && CheckDist( hx, hy, t_cr->PrevHexX, t_cr->PrevHexY, max_dist ) ) )
    {
        cr->Send_XY( cr );
        cr->Send_XY( t_cr );
        return false;
    }

    TraceData trace;
    trace.TraceMap = map;
    trace.BeginHx = hx;
    trace.BeginHy = hy;
    trace.EndHx = tx;
    trace.EndHy = ty;
    trace.Dist = ( ( use == 0 ? weap->GetWeapon_MaxDist_0() : ( use == 1 ? weap->GetWeapon_MaxDist_1() : weap->GetWeapon_MaxDist_2() ) ) > 2 ? max_dist : 0 );
    trace.FindCr = t_cr;
    MapMngr.TraceBullet( trace );
    if( !trace.IsCritterFounded )
    {
        cr->Send_XY( cr );
        cr->Send_XY( t_cr );
        return false;
    }

    int ap_cost = cr->GetUseApCost( weap, rate_weap );
    if( cr->GetCurrentAp() / AP_DIVIDER < ap_cost && !Singleplayer )
    {
        WriteLogF( _FUNC_, " - Not enough AP, critter '%s', target critter '%s'.\n", cr->GetInfo(), t_cr->GetInfo() );
        return false;
    }

    // Ammo
    ProtoItem* ammo = nullptr;
    if( weap->WeapGetAmmoCaliber() && weap->WeapGetMaxAmmoCount() )
    {
        ammo = ProtoMngr.GetProtoItem( weap->GetAmmoPid() );
        if( !ammo )
        {
            weap->SetAmmoPid( weap->GetWeapon_DefaultAmmoPid() );
            ammo = ProtoMngr.GetProtoItem( weap->GetAmmoPid() );
        }

        if( !ammo )
        {
            WriteLogF( _FUNC_, " - Critter weapon ammo not found, critter '%s', target critter '%s'.\n", cr->GetInfo(), t_cr->GetInfo() );
            return false;
        }

        if( ammo->GetType() != ITEM_TYPE_AMMO )
        {
            WriteLogF( _FUNC_, " - Critter weapon ammo is not ammo type, critter '%s', target critter '%s'.\n", cr->GetInfo(), t_cr->GetInfo() );
            return false;
        }
    }

    // No ammo
    if( weap->WeapGetMaxAmmoCount() && !cr->GetIsUnlimitedAmmo() )
    {
        if( !weap->GetAmmoCount() )
        {
            WriteLogF( _FUNC_, " - Critter bullets count is zero, critter '%s'.\n", cr->GetInfo() );
            return false;
        }
    }

    ushort ammo_round = ( use == 0 ? weap->GetWeapon_Round_0() : ( use == 1 ? weap->GetWeapon_Round_1() : weap->GetWeapon_Round_2() ) );
    if( !ammo_round )
        ammo_round = 1;

    // Script events
    bool event_result = ( weap->GetId() ? weap->EventAttack( cr, t_cr ) : false );
    if( !event_result )
        event_result = cr->EventAttack( t_cr );
    if( event_result )
    {
        // cr->SendAA_Action(ACTION_USE_WEAPON,rate_weap,weap);
        cr->SubAp( ap_cost );
        if( map->IsTurnBasedOn && !cr->GetAllAp() )
            map->EndCritterTurn();
        return true;
    }

    // Ap, Turn based
    cr->SubAp( ap_cost );
    if( map->IsTurnBasedOn && !cr->GetAllAp() )
        map->EndCritterTurn();

    // Run script
    if( Script::PrepareContext( ServerFunctions.CritterAttack, _FUNC_, cr->GetInfo() ) )
    {
        Item* ammo_proto = new Item( 0, ammo );
        Script::SetArgEntity( cr );
        Script::SetArgEntity( t_cr );
        Script::SetArgEntity( weap );
        Script::SetArgUChar( MAKE_ITEM_MODE( use, aim ) );
        Script::SetArgEntity( ammo_proto );
        Script::RunPrepared();
        ammo_proto->Release();
    }
    return true;
}

bool FOServer::Act_Reload( Critter* cr, uint weap_id, uint ammo_id )
{
    cr->SetBreakTime( GameOpt.Breaktime );

    if( !cr->CheckMyTurn( nullptr ) )
    {
        WriteLogF( _FUNC_, " - Is not critter '%s' turn.\n", cr->GetInfo() );
        return false;
    }

    Item* weap = cr->GetItem( weap_id, true );
    if( !weap )
    {
        WriteLogF( _FUNC_, " - Unable to find weapon, id %u, critter '%s'.\n", weap_id, cr->GetInfo() );
        return false;
    }

    if( !weap->IsWeapon() )
    {
        WriteLogF( _FUNC_, " - Invalid type of weapon %u, critter '%s'.\n", weap->GetType(), cr->GetInfo() );
        return false;
    }

    if( !weap->WeapGetMaxAmmoCount() )
    {
        WriteLogF( _FUNC_, " - Weapon is not have holder, id %u, critter '%s'.\n", weap_id, cr->GetInfo() );
        return false;
    }

    int ap_cost = cr->GetUseApCost( weap, USE_RELOAD );
    if( cr->GetCurrentAp() / AP_DIVIDER < ap_cost && !Singleplayer )
    {
        WriteLogF( _FUNC_, " - Not enough AP, critter '%s'.\n", cr->GetInfo() );
        return false;
    }
    cr->SubAp( ap_cost );

    Item* ammo = ( ammo_id ? cr->GetItem( ammo_id, true ) : nullptr );
    if( ammo_id && !ammo )
    {
        WriteLogF( _FUNC_, " - Unable to find ammo, id %u, critter '%s'.\n", ammo_id, cr->GetInfo() );
        return false;
    }

    if( ammo && weap->WeapGetAmmoCaliber() != ammo->AmmoGetCaliber() )
    {
        WriteLogF( _FUNC_, " - Different calibers, critter '%s'.\n", cr->GetInfo() );
        return false;
    }

    if( ammo && weap->WeapGetAmmoPid() == ammo->GetProtoId() && weap->WeapIsFull() )
    {
        WriteLogF( _FUNC_, " - Weapon is full, id %u, critter '%s'.\n", weap_id, cr->GetInfo() );
        return false;
    }

    if( !Script::PrepareContext( ServerFunctions.CritterReloadWeapon, _FUNC_, cr->GetInfo() ) )
        return false;
    Script::SetArgEntity( cr );
    Script::SetArgEntity( weap );
    Script::SetArgEntity( ammo );
    if( !Script::RunPrepared() )
        return false;

    cr->SendAA_Action( ACTION_RELOAD_WEAPON, 0, weap );
    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( map && map->IsTurnBasedOn && !cr->GetAllAp() )
        map->EndCritterTurn();
    return true;
}

#pragma MESSAGE("Add using hands/legs.")
bool FOServer::Act_Use( Critter* cr, uint item_id, int skill, int target_type, uint target_id, hash target_pid, uint param )
{
    cr->SetBreakTime( GameOpt.Breaktime );

    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( map && !cr->CheckMyTurn( map ) )
    {
        WriteLogF( _FUNC_, " - Is not critter '%s' turn.\n", cr->GetInfo() );
        return false;
    }

    Item* item = nullptr;
    if( item_id )
    {
        item = cr->GetItem( item_id, cr->IsPlayer() );
        if( !item )
        {
            WriteLogF( _FUNC_, " - Item not found, id %u, critter '%s'.\n", item_id, cr->GetInfo() );
            return false;
        }
    }

    int ap_cost = ( item_id ? cr->GetUseApCost( item, USE_USE ) : cr->GetApCostUseSkill() );
    if( cr->GetCurrentAp() / AP_DIVIDER < ap_cost && !Singleplayer )
    {
        WriteLogF( _FUNC_, " - Not enough AP, critter '%s'.\n", cr->GetInfo() );
        return false;
    }
    cr->SubAp( ap_cost );

    Critter* target_cr = nullptr;
    Item*    target_item = nullptr;
    Item*    target_scen = nullptr;

    if( target_type == TARGET_CRITTER )
    {
        if( cr->GetId() != target_id )
        {
            if( cr->GetMapId() )
            {
                target_cr = cr->GetCritSelf( target_id, true );
                if( !target_cr )
                {
                    WriteLogF( _FUNC_, " - Target critter not found, id %u, critter '%s'.\n", target_id, cr->GetInfo() );
                    return false;
                }

                if( !CheckDist( cr->GetHexX(), cr->GetHexY(), target_cr->GetHexX(), target_cr->GetHexY(), cr->GetUseDist() + target_cr->GetMultihex() ) )
                {
                    cr->Send_XY( cr );
                    cr->Send_XY( target_cr );
                    WriteLogF( _FUNC_, " - Target critter too far, id %u, critter '%s'.\n", target_id, cr->GetInfo() );
                    return false;
                }
            }
            else
            {
                if( !cr->GroupMove )
                {
                    WriteLogF( _FUNC_, " - Group move null ptr, critter '%s'.\n", cr->GetInfo() );
                    return false;
                }

                target_cr = cr->GroupMove->GetCritter( target_id );
                if( !target_cr )
                {
                    WriteLogF( _FUNC_, " - Target critter not found, id %u, critter '%s'.\n", target_id, cr->GetInfo() );
                    return false;
                }
            }
        }
    }
    else if( target_type == TARGET_SELF )
    {
        target_id = cr->GetId();
    }
    else if( target_type == TARGET_SELF_ITEM )
    {
        target_item = cr->GetItem( target_id, cr->IsPlayer() );
        if( !target_item )
        {
            WriteLogF( _FUNC_, " - Item not found in critter inventory, critter '%s'.\n", cr->GetInfo() );
            return false;
        }
    }
    else if( target_type == TARGET_ITEM )
    {
        if( !cr->GetMapId() )
        {
            WriteLogF( _FUNC_, " - Can't get item, critter '%s' on global map.\n", cr->GetInfo() );
            return false;
        }

        if( !map )
        {
            WriteLogF( _FUNC_, " - Map not found, map id %u, critter '%s'.\n", cr->GetMapId(), cr->GetInfo() );
            return false;
        }

        target_item = map->GetItem( target_id );
        if( !target_item )
        {
            WriteLogF( _FUNC_, " - Target item not found, id %u, critter '%s'.\n", target_id, cr->GetInfo() );
            return false;
        }

        if( target_item->GetIsHidden() )
        {
            WriteLogF( _FUNC_, " - Target item is hidden, id %u, critter '%s'.\n", target_id, cr->GetInfo() );
            return false;
        }

        if( !CheckDist( cr->GetHexX(), cr->GetHexY(), target_item->GetHexX(), target_item->GetHexY(), cr->GetUseDist() ) )
        {
            cr->Send_XY( cr );
            WriteLogF( _FUNC_, " - Target item too far, id %u, critter '%s'.\n", target_id, cr->GetInfo() );
            return false;
        }
    }
    else if( target_type == TARGET_SCENERY )
    {
        if( !cr->GetMapId() )
        {
            WriteLogF( _FUNC_, " - Can't get scenery, critter '%s' on global map.\n", cr->GetInfo() );
            return false;
        }

        if( !map )
        {
            WriteLogF( _FUNC_, " - Map not found_, map id %u, critter '%s'.\n", cr->GetMapId(), cr->GetInfo() );
            return false;
        }

        ushort hx = target_id >> 16;
        ushort hy = target_id & 0xFFFF;

        if( !CheckDist( cr->GetHexX(), cr->GetHexY(), hx, hy, cr->GetUseDist() ) )
        {
            cr->Send_XY( cr );
            WriteLogF( _FUNC_, " - Target scenery too far, critter '%s', hx %u, hy %u.\n", cr->GetInfo(), hx, hy );
            return false;
        }

        ProtoItem* proto_scenery = ProtoMngr.GetProtoItem( target_pid );
        if( !proto_scenery )
        {
            WriteLogF( _FUNC_, " - Proto scenery '%s' not found, critter '%s'.\n", Str::GetName( target_pid ), cr->GetInfo() );
            return false;
        }

        if( !proto_scenery->IsGeneric() )
        {
            WriteLogF( _FUNC_, " - Target scenery '%s' is not scenery, critter '%s'.\n", Str::GetName( target_pid ), cr->GetInfo() );
            return false;
        }

        target_scen = map->GetProtoMap()->GetMapScenery( hx, hy, target_pid );
        if( !target_scen )
        {
            WriteLogF( _FUNC_, " - Scenery '%s' not found on map, critter '%s'.\n", Str::GetName( target_pid ), cr->GetInfo() );
            return false;
        }
    }
    else
    {
        WriteLogF( _FUNC_, " - Unknown target type %d, critter '%s'.\n", target_type, cr->GetInfo() );
        return false;
    }

    // Send action
    if( cr->GetMapId() )
    {
        if( item )
        {
            cr->SendAA_Action( ACTION_USE_ITEM, 0, item );
        }
        else
        {
            if( target_item )
                cr->SendAA_Action( ACTION_USE_SKILL, skill, target_item );
            else if( target_cr )
                cr->SendAA_Action( ACTION_USE_SKILL, skill, nullptr );
            else if( target_scen )
                cr->SendAA_Action( ACTION_USE_SKILL, skill, target_scen );
        }
    }

    // Check turn based
    if( map && map->IsTurnBasedOn && !cr->GetAllAp() )
        map->EndCritterTurn();

    // Scenery
    if( target_scen && target_scen->SceneryScriptBindId != 0 )
    {
        if( !Script::PrepareContext( target_scen->SceneryScriptBindId, _FUNC_, cr->GetInfo() ) )
            return false;
        Script::SetArgEntity( cr );
        Script::SetArgEntity( target_scen );
        Script::SetArgUInt( item ? SKILL_PICK_ON_GROUND : skill );
        Script::SetArgEntity( item );
        if( target_scen->IsSceneryParams() )
        {
            ScriptArray* scenery_params = target_scen->GetSceneryParams();
            for( int i = 0, j = scenery_params->GetSize(); i < j; i++ )
                Script::SetArgUInt( *(int*) scenery_params->At( i ) );
            scenery_params->Release();
        }
        if( Script::RunPrepared() && Script::GetReturnedBool() )
            return true;
    }

    // Item
    if( item )
    {
        if( target_item && target_item->EventUseOnMe( cr, item ) )
            return true;
        if( item->EventUse( cr, target_cr, target_item, target_scen ) )
            return true;
        if( cr->EventUseItem( item, target_cr, target_item, target_scen ) )
            return true;
        if( target_cr && target_cr->EventUseItemOnMe( cr, item ) )
            return true;
        if( Script::PrepareContext( ServerFunctions.CritterUseItem, _FUNC_, cr->GetInfo() ) )
        {
            Script::SetArgEntity( cr );
            Script::SetArgEntity( item );
            Script::SetArgEntity( target_cr );
            Script::SetArgEntity( target_item );
            Script::SetArgEntity( target_scen );
            Script::SetArgUInt( param );
            if( Script::RunPrepared() && Script::GetReturnedBool() )
                return true;
        }

        // Default process
        if( item->GetIsHolodisk() && target_type == TARGET_SELF && cr->IsPlayer() )
        {
            AddPlayerHoloInfo( (Client*) cr, item->GetHolodiskNum(), true );
        }
        // Nothing happens
        else
        {
            cr->Send_TextMsg( cr, STR_USE_NOTHING, SAY_NETMSG, TEXTMSG_GAME );
        }
    }
    // Skill
    else
    {
        if( target_item && target_item->EventSkill( cr, skill ) )
            return true;
        if( cr->EventUseSkill( skill, target_cr, target_item, target_scen ) )
            return true;
        if( target_cr && target_cr->EventUseSkillOnMe( cr, skill ) )
            return true;
        if( !Script::PrepareContext( ServerFunctions.CritterUseSkill, _FUNC_, cr->GetInfo() ) )
            return false;
        Script::SetArgEntity( cr );
        Script::SetArgUInt( skill );
        Script::SetArgEntity( target_cr );
        Script::SetArgEntity( target_item );
        Script::SetArgEntity( target_scen );
        if( Script::RunPrepared() && Script::GetReturnedBool() )
            return true;

        // Nothing happens
        cr->Send_TextMsg( cr, STR_USE_NOTHING, SAY_NETMSG, TEXTMSG_GAME );
    }

    return true;
}

bool FOServer::Act_PickItem( Critter* cr, ushort hx, ushort hy, hash pid )
{
    cr->SetBreakTime( GameOpt.Breaktime );

    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( !map )
        return false;

    if( !cr->CheckMyTurn( map ) )
    {
        WriteLogF( _FUNC_, " - Is not critter '%s' turn.\n", cr->GetInfo() );
        return false;
    }

    int ap_cost = cr->GetApCostPickItem();
    if( cr->GetCurrentAp() / AP_DIVIDER < ap_cost && !Singleplayer )
    {
        WriteLogF( _FUNC_, " - Not enough AP, critter '%s'.\n", cr->GetInfo() );
        return false;
    }
    cr->SubAp( ap_cost );

    if( hx >= map->GetWidth() || hy >= map->GetHeight() )
        return false;

    if( !CheckDist( cr->GetHexX(), cr->GetHexY(), hx, hy, cr->GetUseDist() ) )
    {
        WriteLogF( _FUNC_, " - Wrong distance, critter '%s'.\n", cr->GetInfo() );
        cr->Send_XY( cr );
        return false;
    }

    ProtoItem* proto = ProtoMngr.GetProtoItem( pid );
    if( !proto )
    {
        WriteLogF( _FUNC_, " - Proto item '%s' not found, critter '%s'.\n", Str::GetName( pid ), cr->GetInfo() );
        return false;
    }

    if( map->IsTurnBasedOn && !cr->GetAllAp() )
        map->EndCritterTurn();

    if( proto->IsItem() )
    {
        Item* pick_item = map->GetItemHex( hx, hy, pid, cr->IsPlayer() ? cr : nullptr );
        if( !pick_item )
            return false;

        cr->SendAA_Action( ACTION_PICK_ITEM, 0, pick_item );

        if( pick_item->EventSkill( cr, SKILL_PICK_ON_GROUND ) )
            return true;
        if( cr->EventUseSkill( SKILL_PICK_ON_GROUND, nullptr, pick_item, nullptr ) )
            return true;
        if( Script::PrepareContext( ServerFunctions.CritterUseSkill, _FUNC_, cr->GetInfo() ) )
        {
            Script::SetArgEntity( cr );
            Script::SetArgUInt( SKILL_PICK_ON_GROUND );
            Script::SetArgEntity( nullptr );
            Script::SetArgEntity( pick_item );
            Script::SetArgEntity( nullptr );
            if( Script::RunPrepared() && Script::GetReturnedBool() )
                return true;
        }

        cr->Send_TextMsg( cr, STR_USE_NOTHING, SAY_NETMSG, TEXTMSG_GAME );
    }
    else if( proto->IsGeneric() )
    {
        Item* pick_scenery = map->GetProtoMap()->GetMapScenery( hx, hy, pid );
        if( !pick_scenery )
        {
            cr->Send_Text( cr, "Scenery not found, maybe map outdated.", SAY_NETMSG );
            return false;
        }

        cr->SendAA_Action( ACTION_PICK_ITEM, 0, pick_scenery );

        if( pick_scenery->SceneryScriptBindId )
        {
            if( !Script::PrepareContext( pick_scenery->SceneryScriptBindId, _FUNC_, cr->GetInfo() ) )
                return false;
            Script::SetArgEntity( cr );
            Script::SetArgEntity( pick_scenery );
            Script::SetArgUInt( SKILL_PICK_ON_GROUND );
            Script::SetArgEntity( nullptr );
            if( pick_scenery->IsSceneryParams() )
            {
                ScriptArray* scenery_params = pick_scenery->GetSceneryParams();
                for( int i = 0, j = scenery_params->GetSize(); i < j; i++ )
                    Script::SetArgUInt( *(int*) scenery_params->At( i ) );
                scenery_params->Release();
            }
            if( Script::RunPrepared() && Script::GetReturnedBool() )
                return true;
        }

        if( cr->EventUseSkill( SKILL_PICK_ON_GROUND, nullptr, nullptr, pick_scenery ) )
            return true;
        if( Script::PrepareContext( ServerFunctions.CritterUseSkill, _FUNC_, cr->GetInfo() ) )
        {
            Script::SetArgEntity( cr );
            Script::SetArgUInt( SKILL_PICK_ON_GROUND );
            Script::SetArgEntity( nullptr );
            Script::SetArgEntity( nullptr );
            Script::SetArgEntity( pick_scenery );
            if( Script::RunPrepared() && Script::GetReturnedBool() )
                return true;
        }

        cr->Send_TextMsg( cr, STR_USE_NOTHING, SAY_NETMSG, TEXTMSG_GAME );
    }
    else if( proto->IsGrid() )
    {
        switch( proto->GetGrid_Type() )
        {
        case GRID_STAIRS:
        case GRID_LADDERBOT:
        case GRID_LADDERTOP:
        case GRID_ELEVATOR:
        {
            Item* pick_item = new Item( uint( -1 ), proto );
            cr->SendAA_Action( ACTION_PICK_ITEM, 0, pick_item );
            SAFEREL( pick_item );

            MapMngr.TryTransitCrGrid( cr, map, hx, hy, false );
        }
        break;
        default:
            cr->Send_TextMsg( cr, STR_USE_NOTHING, SAY_NETMSG, TEXTMSG_GAME );
            break;
        }
    }
    else if( proto->IsWall() )
    {
        cr->Send_TextMsg( cr, STR_USE_NOTHING, SAY_NETMSG, TEXTMSG_GAME );
        return false;
    }
    else
    {
        cr->Send_TextMsg( cr, STR_USE_NOTHING, SAY_NETMSG, TEXTMSG_GAME );
        return false;
    }
    return true;
}

void FOServer::KillCritter( Critter* cr, uint anim2, Critter* attacker )
{
    // Close talk
    if( cr->IsPlayer() )
    {
        Client* cl = (Client*) cr;
        if( cl->IsTalking() )
            cl->CloseTalk();
    }
    // Disable sneaking
    if( cr->GetIsHide() )
        cr->SetIsHide( false );

    // Process dead
    cr->ToDead( anim2, true );
    cr->EventDead( attacker );
    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( map )
        map->EventCritterDead( cr, attacker );
    if( Script::PrepareContext( ServerFunctions.CritterDead, _FUNC_, cr->GetInfo() ) )
    {
        Script::SetArgEntity( cr );
        Script::SetArgEntity( attacker );
        Script::RunPrepared();
    }
}

void FOServer::RespawnCritter( Critter* cr )
{
    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( !map )
    {
        WriteLogF( _FUNC_, " - Current map not found, continue dead. Critter '%s'.\n", cr->GetInfo() );
        return;
    }

    ushort hx = cr->GetHexX();
    ushort hy = cr->GetHexY();
    uint   multihex = cr->GetMultihex();
    if( !map->IsHexesPassed( hx, hy, multihex ) )
    {
        // WriteLogF(_FUNC_," - Live critter on hex, continue dead.\n");
        return;
    }

    map->UnsetFlagCritter( hx, hy, multihex, true );
    map->SetFlagCritter( hx, hy, multihex, false );

    cr->SetCond( COND_LIFE );
    if( cr->GetCurrentHp() < 1 )
        cr->SetCurrentHp( 1 );
    cr->Send_Action( cr, ACTION_RESPAWN, 0, nullptr );
    cr->SendAA_Action( ACTION_RESPAWN, 0, nullptr );
    cr->EventRespawn();
    if( Script::PrepareContext( ServerFunctions.CritterRespawn, _FUNC_, cr->GetInfo() ) )
    {
        Script::SetArgEntity( cr );
        Script::RunPrepared();
    }
}

void FOServer::KnockoutCritter( Critter* cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, ushort knock_hx, ushort knock_hy )
{
    // Close talk
    if( cr->IsPlayer() )
    {
        Client* cl = (Client*) cr;
        if( cl->IsTalking() )
            cl->CloseTalk();
    }

    // Disable sneaking
    if( cr->GetIsHide() )
        cr->SetIsHide( true );

    // Process knockout
    int  x1 = cr->GetHexX();
    int  y1 = cr->GetHexY();
    int  x2 = knock_hx;
    int  y2 = knock_hy;

    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( !map || x2 >= map->GetWidth() || y2 >= map->GetHeight() )
        return;

    TraceData td;
    td.TraceMap = map;
    td.FindCr = cr;
    td.BeginHx = x1;
    td.BeginHy = y1;
    td.EndHx = x2;
    td.EndHy = y2;
    td.HexCallback = FOServer::VerifyTrigger;
    MapMngr.TraceBullet( td );

    // Map can be changed on some trigger
    if( cr->GetMapId() != map->GetId() )
        return;

    if( x1 != x2 || y1 != y2 )
    {
        uint multihex = cr->GetMultihex();
        bool is_dead = cr->IsDead();
        map->UnsetFlagCritter( x1, y1, multihex, is_dead );
        cr->SetHexX( x2 );
        cr->SetHexY( y2 );
        map->SetFlagCritter( x2, y2, multihex, is_dead );
    }

    cr->ToKnockout( anim2begin, anim2idle, anim2end, lost_ap, knock_hx, knock_hy );
    cr->EventKnockout( anim2begin, anim2idle, anim2end, lost_ap, DistGame( x1, y1, x2, y2 ) );
}

bool FOServer::MoveRandom( Critter* cr )
{
    UCharVec dirs( 6 );
    for( int i = 0; i < 6; i++ )
        dirs[ i ] = i;
    std::random_shuffle( dirs.begin(), dirs.end() );

    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( !map )
        return false;

    uint   multihex = cr->GetMultihex();
    ushort maxhx = map->GetWidth();
    ushort maxhy = map->GetHeight();

    for( int i = 0; i < 6; i++ )
    {
        uchar  dir = dirs[ i ];
        ushort hx = cr->GetHexX();
        ushort hy = cr->GetHexY();
        if( MoveHexByDir( hx, hy, dir, maxhx, maxhy ) && map->IsMovePassed( hx, hy, dir, multihex ) )
        {
            if( Act_Move( cr, hx, hy, 0 ) )
            {
                cr->Send_Move( cr, 0 );
                return true;
            }
            return false;
        }
    }
    return false;
}

bool FOServer::RegenerateMap( Map* map )
{
    map->DeleteContent();
    map->Generate();
    return true;
}

bool FOServer::VerifyTrigger( Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir )
{
    // Triggers
    bool result = false;
    if( map->IsHexTrigger( from_hx, from_hy ) || map->IsHexTrigger( to_hx, to_hy ) )
    {
        Item* out_trigger = map->GetProtoMap()->GetMapScenery( from_hx, from_hy, SP_SCEN_TRIGGER );
        Item* in_trigger = map->GetProtoMap()->GetMapScenery( to_hx, to_hy, SP_SCEN_TRIGGER );
        if( !( out_trigger && in_trigger && out_trigger->GetTriggerNum() == in_trigger->GetTriggerNum() ) )
        {
            if( out_trigger && Script::PrepareContext( out_trigger->SceneryScriptBindId, _FUNC_, cr->GetInfo() ) )
            {
                Script::SetArgEntity( cr );
                Script::SetArgEntity( out_trigger );
                Script::SetArgBool( false );
                Script::SetArgUChar( dir );
                if( out_trigger->IsSceneryParams() )
                {
                    ScriptArray* scenery_params = out_trigger->GetSceneryParams();
                    for( int i = 0, j = scenery_params->GetSize(); i < j; i++ )
                        Script::SetArgUInt( *(int*) scenery_params->At( i ) );
                    scenery_params->Release();
                }
                if( Script::RunPrepared() )
                    result = true;
            }
            if( in_trigger && Script::PrepareContext( in_trigger->SceneryScriptBindId, _FUNC_, cr->GetInfo() ) )
            {
                Script::SetArgEntity( cr );
                Script::SetArgEntity( in_trigger );
                Script::SetArgBool( true );
                Script::SetArgUChar( dir );
                if( in_trigger->IsSceneryParams() )
                {
                    ScriptArray* scenery_params = in_trigger->GetSceneryParams();
                    for( int i = 0, j = scenery_params->GetSize(); i < j; i++ )
                        Script::SetArgUInt( *(int*) scenery_params->At( i ) );
                    scenery_params->Release();
                }
                if( Script::RunPrepared() )
                    result = true;
            }
        }
    }
    return result;
}

void FOServer::Process_Update( Client* cl )
{
    // Net protocol
    ushort proto_ver = 0;
    cl->Bin >> proto_ver;
    if( proto_ver != FONLINE_VERSION )
    {
        cl->Send_CustomMessage( NETMSG_WRONG_NET_PROTO );
        cl->Disconnect();
        return;
    }

    // Begin data encrypting
    uint encrypt_key;
    cl->Bin >> encrypt_key;
    cl->Bin.SetEncryptKey( encrypt_key + 521 );
    cl->Bout.SetEncryptKey( encrypt_key + 3491 );

    // Send update files list with global properties
    uint       msg_len = sizeof( NETMSG_UPDATE_FILES_LIST ) + sizeof( msg_len ) + sizeof( uint ) + (uint) UpdateFilesList.size();
    PUCharVec* global_vars_data;
    UIntVec*   global_vars_data_sizes;
    uint       whole_data_size = Globals->Props.StoreData( false, &global_vars_data, &global_vars_data_sizes );
    msg_len += sizeof( ushort ) + whole_data_size;
    BOUT_BEGIN( cl );
    cl->Bout << NETMSG_UPDATE_FILES_LIST;
    cl->Bout << msg_len;
    cl->Bout << (uint) UpdateFilesList.size();
    if( !UpdateFilesList.empty() )
        cl->Bout.Push( (char*) &UpdateFilesList[ 0 ], (uint) UpdateFilesList.size() );
    NET_WRITE_PROPERTIES( cl->Bout, global_vars_data, global_vars_data_sizes );
    BOUT_END( cl );
}

void FOServer::Process_UpdateFile( Client* cl )
{
    uint file_index;
    cl->Bin >> file_index;

    if( file_index >= (uint) UpdateFiles.size() )
    {
        WriteLogF( _FUNC_, " - Wrong file index %u, client ip '%s'.\n", file_index, cl->GetIpStr() );
        cl->Disconnect();
        return;
    }

    cl->UpdateFileIndex = file_index;
    cl->UpdateFilePortion = 0;
    Process_UpdateFileData( cl );
}

void FOServer::Process_UpdateFileData( Client* cl )
{
    if( cl->UpdateFileIndex == -1 )
    {
        WriteLogF( _FUNC_, " - Wrong update call, client ip '%s'.\n", cl->GetIpStr() );
        cl->Disconnect();
        return;
    }

    UpdateFile& update_file = UpdateFiles[ cl->UpdateFileIndex ];
    uint        offset = cl->UpdateFilePortion * FILE_UPDATE_PORTION;

    if( offset + FILE_UPDATE_PORTION < update_file.Size )
        cl->UpdateFilePortion++;
    else
        cl->UpdateFileIndex = -1;

    if( cl->IsSendDisabled() || cl->IsOffline() )
        return;

    uchar data[ FILE_UPDATE_PORTION ];
    uint  remaining_size = update_file.Size - offset;
    if( remaining_size >= sizeof( data ) )
    {
        memcpy( data, &update_file.Data[ offset ], sizeof( data ) );
    }
    else
    {
        memcpy( data, &update_file.Data[ offset ], remaining_size );
        memzero( &data[ remaining_size ], sizeof( data ) - remaining_size );
    }

    BOUT_BEGIN( cl );
    cl->Bout << NETMSG_UPDATE_FILE_DATA;
    cl->Bout.Push( (char*) data, sizeof( data ) );
    BOUT_END( cl );
}

void FOServer::Process_CreateClient( Client* cl )
{
    // Prevent brute force by ip
    if( CheckBruteForceIp( cl->GetIp() ) )
    {
        cl->Disconnect();
        return;
    }

    // Check for ban by ip
    {
        SCOPE_LOCK( BannedLocker );

        uint          ip = cl->GetIp();
        ClientBanned* ban = GetBanByIp( ip );
        if( ban && !Singleplayer )
        {
            cl->Send_TextMsg( cl, STR_NET_BANNED_IP, SAY_NETMSG, TEXTMSG_GAME );
            // cl->Send_TextMsgLex(cl,STR_NET_BAN_REASON,SAY_NETMSG,TEXTMSG_GAME,ban->GetBanLexems());
            cl->Send_TextMsgLex( cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, Str::FormatBuf( "$time%u", GetBanTime( *ban ) ) );
            cl->Disconnect();
            return;
        }
    }

    uint msg_len;
    cl->Bin >> msg_len;

    // Protocol version
    ushort proto_ver = 0;
    cl->Bin >> proto_ver;
    if( proto_ver != FONLINE_VERSION )
    {
        // WriteLogF(_FUNC_," - Wrong Protocol Version from SockId %u.\n", cl->Sock);
        cl->Send_TextMsg( cl, STR_NET_WRONG_NETPROTO, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    // Begin data encrypting
    cl->Bin.SetEncryptKey( 1234567890 );
    cl->Bout.SetEncryptKey( 1234567890 );

    // Name
    char name[ UTF8_BUF_SIZE( MAX_NAME ) ];
    cl->Bin.Pop( name, sizeof( name ) );
    name[ sizeof( name ) - 1 ] = 0;
    Str::Copy( cl->Name, name );

    // Password hash
    char pass_hash[ PASS_HASH_SIZE ];
    cl->Bin.Pop( pass_hash, PASS_HASH_SIZE );
    cl->SetBinPassHash( pass_hash );

    // Receive params
    ushort props_count = 0;
    cl->Bin >> props_count;

    if( props_count > ( ushort ) Critter::RegProperties.size() )
    {
        cl->Send_TextMsg( cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    for( ushort i = 0; i < props_count; i++ )
    {
        int enum_value;
        int value;
        cl->Bin >> enum_value;
        cl->Bin >> value;

        if( !Critter::RegProperties.count( enum_value ) )
        {
            cl->Send_TextMsg( cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        Property* prop = Critter::PropertiesRegistrator->FindByEnum( enum_value );
        if( !prop || !prop->IsPOD() )
        {
            WriteLogF( _FUNC_, " - Invalid allowed property '%s'.\n", prop ? prop->GetName() : Str::ItoA( enum_value ) );
            cl->Send_TextMsg( cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        Properties::SetValueAsInt( cl, enum_value, value );
    }

    // Check net
    CHECK_IN_BUFF_ERROR_EXT( cl, cl->Send_TextMsg( cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME ), return );

    // Check data
    if( !Str::IsValidUTF8( cl->Name ) || Str::Substring( cl->Name, "*" ) )
    {
        cl->Send_TextMsg( cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    uint name_len_utf8 = Str::LengthUTF8( cl->Name );
    if( name_len_utf8 < MIN_NAME || name_len_utf8 < GameOpt.MinNameLength ||
        name_len_utf8 > MAX_NAME || name_len_utf8 > GameOpt.MaxNameLength )
    {
        cl->Send_TextMsg( cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    // Check for exist
    uint id = MAKE_CLIENT_ID( name );
    RUNTIME_ASSERT( IS_CLIENT_ID( id ) );
    if( !Singleplayer )
    {
        SaveClientsLocker.Lock();
        bool exist = ( GetClientData( id ) != nullptr );
        SaveClientsLocker.Unlock();

        if( !exist )
        {
            // Avoid created files rewriting
            char fname[ MAX_FOPATH ];
            FileManager::GetWritePath( cl->Name, PT_SERVER_CLIENTS, fname );
            Str::Append( fname, ".foclient" );
            exist = FileExist( fname );
        }

        if( exist )
        {
            cl->Send_TextMsg( cl, STR_NET_ACCOUNT_ALREADY, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }
    }

    // Check brute force registration
    #ifndef DEV_VERSION
    if( GameOpt.RegistrationTimeout && !Singleplayer )
    {
        SCOPE_LOCK( RegIpLocker );

        uint ip = cl->GetIp();
        uint reg_tick = GameOpt.RegistrationTimeout * 1000;
        auto it = RegIp.find( ip );
        if( it != RegIp.end() )
        {
            uint& last_reg = it->second;
            uint  tick = Timer::FastTick();
            if( tick - last_reg < reg_tick )
            {
                cl->Send_TextMsg( cl, STR_NET_REGISTRATION_IP_WAIT, SAY_NETMSG, TEXTMSG_GAME );
                cl->Send_TextMsgLex( cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, Str::FormatBuf( "$time%u", ( reg_tick - ( tick - last_reg ) ) / 60000 + 1 ) );
                cl->Disconnect();
                return;
            }
            last_reg = tick;
        }
        else
        {
            RegIp.insert( PAIR( ip, Timer::FastTick() ) );
        }
    }
    #endif

    bool allow = false;
    uint disallow_msg_num = 0, disallow_str_num = 0;
    char disallow_lexems[ MAX_FOTEXT ];
    if( Script::PrepareContext( ServerFunctions.PlayerRegistration, _FUNC_, cl->Name ) )
    {
        ScriptString* name = ScriptString::Create( cl->Name );
        ScriptString* lexems = ScriptString::Create();
        Script::SetArgUInt( cl->GetIp() );
        Script::SetArgObject( name );
        Script::SetArgAddress( &disallow_msg_num );
        Script::SetArgAddress( &disallow_str_num );
        Script::SetArgObject( lexems );
        if( Script::RunPrepared() && Script::GetReturnedBool() )
            allow = true;
        Str::Copy( disallow_lexems, lexems->c_str() );
        name->Release();
        lexems->Release();
    }
    if( !allow )
    {
        if( disallow_msg_num < TEXTMSG_COUNT && disallow_str_num )
            cl->Send_TextMsgLex( cl, disallow_str_num, SAY_NETMSG, disallow_msg_num, disallow_lexems );
        else
            cl->Send_TextMsg( cl, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    // Register
    cl->SetId( id );
    cl->RefreshName();
    cl->SetWorldX( GM_MAXX / 2 );
    cl->SetWorldY( GM_MAXY / 2 );
    cl->SetCond( COND_LIFE );

    ScriptArray* arr_reg_ip = Script::CreateArray( "uint[]" );
    uint         reg_ip = cl->GetIp();
    arr_reg_ip->InsertLast( &reg_ip );
    cl->SetConnectionIp( arr_reg_ip );
    SAFEREL( arr_reg_ip );
    ScriptArray* arr_reg_port = Script::CreateArray( "uint16[]" );
    ushort       reg_port = cl->GetPort();
    arr_reg_port->InsertLast( &reg_port );
    cl->SetConnectionPort( arr_reg_port );
    SAFEREL( arr_reg_port );

    // Assign base access
    cl->Access = ACCESS_DEFAULT;

    // First save
    if( !SaveClient( cl ) )
    {
        WriteLogF( _FUNC_, " - First save fail.\n" );
        cl->Send_TextMsg( cl, STR_NET_BD_ERROR, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    // Add name in clients names cache file
    char  cache_fname[ MAX_FOPATH ];
    FileManager::GetWritePath( "ClientsList.txt", PT_SERVER_CACHE, cache_fname );
    void* cache_file = FileOpenForAppend( cache_fname );
    if( cache_file )
    {
        FileWrite( cache_file, cl->Name, Str::Length( cl->Name ) );
        FileWrite( cache_file, ".foclient\n", 10 );
        FileClose( cache_file );
    }
    else
    {
        FileClose( FileOpen( "cache_fail", true ) );
    }

    // Clear brute force ip and name, because client enter to game immediately after registration
    ClearBruteForceEntire( cl->GetIp(), cl->Name );

    // Load world
    if( Singleplayer )
    {
        SynchronizeLogicThreads();
        SyncManager::GetForCurThread()->UnlockAll();
        SYNC_LOCK( cl );
        if( !NewWorld() )
        {
            WriteLogF( _FUNC_, " - Generate new world fail.\n" );
            cl->Send_TextMsg( cl, STR_SP_NEW_GAME_FAIL, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            ResynchronizeLogicThreads();
            return;
        }
        ResynchronizeLogicThreads();
    }

    // Notify
    if( !Singleplayer )
        cl->Send_TextMsg( cl, STR_NET_REG_SUCCESS, SAY_NETMSG, TEXTMSG_GAME );
    else
        cl->Send_TextMsg( cl, STR_SP_NEW_GAME_SUCCESS, SAY_NETMSG, TEXTMSG_GAME );

    BOUT_BEGIN( cl );
    cl->Bout << (uint) NETMSG_REGISTER_SUCCESS;
    BOUT_END( cl );

    cl->Disconnect();

    cl->AddRef();
    EntityMngr.RegisterEntity( cl );
    MapMngr.AddCrToMap( cl, nullptr, 0, 0, 0, false );
    Job::PushBack( JOB_CRITTER, cl );

    if( Script::PrepareContext( ServerFunctions.CritterInit, _FUNC_, cl->GetInfo() ) )
    {
        Script::SetArgEntity( cl );
        Script::SetArgBool( true );
        Script::RunPrepared();
    }
    SaveClient( cl );

    if( !Singleplayer )
    {
        ClientData* data = new ClientData();
        memzero( data, sizeof( ClientData ) );
        Str::Copy( data->ClientName, cl->Name );
        memcpy( data->ClientPassHash, pass_hash, PASS_HASH_SIZE );

        SCOPE_LOCK( ClientsDataLocker );
        ClientsData.insert( PAIR( cl->GetId(), data ) );
    }
    else
    {
        SingleplayerSave.Valid = true;
        SingleplayerSave.Name = cl->Name;
        SingleplayerSave.CrProps = &cl->Props;
        SingleplayerSave.PicData.clear();
    }
}

void FOServer::Process_LogIn( ClientPtr& cl )
{
    // Prevent brute force by ip
    if( CheckBruteForceIp( cl->GetIp() ) )
    {
        cl->Disconnect();
        return;
    }

    // Net protocol
    ushort proto_ver = 0;
    cl->Bin >> proto_ver;
    if( proto_ver != FONLINE_VERSION )
    {
        cl->Send_CustomMessage( NETMSG_WRONG_NET_PROTO );
        cl->Disconnect();
        return;
    }

    // UIDs
    uint uidxor, uidor, uidcalc;
    uint uid[ 5 ];
    cl->Bin >> uid[ 4 ];

    // Begin data encrypting
    cl->Bin.SetEncryptKey( uid[ 4 ] + 12345 );
    cl->Bout.SetEncryptKey( uid[ 4 ] + 12345 );

    // Login, password hash
    char name[ UTF8_BUF_SIZE( MAX_NAME ) ];
    cl->Bin.Pop( name, sizeof( name ) );
    name[ sizeof( name ) - 1 ] = 0;
    Str::Copy( cl->Name, name );
    cl->Bin >> uid[ 1 ];
    char pass_hash[ PASS_HASH_SIZE ];
    cl->Bin.Pop( pass_hash, PASS_HASH_SIZE );

    if( Singleplayer )
    {
        memzero( cl->Name, sizeof( cl->Name ) );
        memzero( pass_hash, sizeof( pass_hash ) );
    }

    // Prevent brute force by name
    if( CheckBruteForceName( cl->Name ) )
    {
        cl->Disconnect();
        return;
    }

    // Bin hashes
    uint  msg_language;
    uint  textmsg_hash[ TEXTMSG_COUNT ];
    uint  item_hash[ ITEM_MAX_TYPES ];
    uchar default_combat_mode;

    cl->Bin >> msg_language;
    for( int i = 0; i < TEXTMSG_COUNT; i++ )
        cl->Bin >> textmsg_hash[ i ];
    cl->Bin >> uidxor;
    cl->Bin >> uid[ 3 ];
    cl->Bin >> uid[ 2 ];
    cl->Bin >> uidor;
    for( int i = 0; i < ITEM_MAX_TYPES; i++ )
        cl->Bin >> item_hash[ i ];
    cl->Bin >> uidcalc;
    cl->Bin >> default_combat_mode;
    cl->Bin >> uid[ 0 ];
    char dummy[ 100 ];
    cl->Bin.Pop( dummy, 100 );
    CHECK_IN_BUFF_ERROR_EXT( cl, cl->Send_TextMsg( cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME ), return );

    // Language
    auto it_l = std::find( LangPacks.begin(), LangPacks.end(), msg_language );
    if( it_l != LangPacks.end() )
        cl->LanguageMsg = msg_language;
    else
        cl->LanguageMsg = ( *LangPacks.begin() ).Name;

    // If only cache checking than disconnect
    if( !Singleplayer && !name[ 0 ] )
    {
        cl->Disconnect();
        return;
    }

    // Singleplayer
    if( Singleplayer && !SingleplayerSave.Valid )
    {
        WriteLogF( _FUNC_, " - World not contain singleplayer data.\n" );
        cl->Send_TextMsg( cl, STR_SP_SAVE_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    // Check for ban by ip
    {
        SCOPE_LOCK( BannedLocker );

        uint          ip = cl->GetIp();
        ClientBanned* ban = GetBanByIp( ip );
        if( ban && !Singleplayer )
        {
            cl->Send_TextMsg( cl, STR_NET_BANNED_IP, SAY_NETMSG, TEXTMSG_GAME );
            if( Str::CompareCaseUTF8( ban->ClientName, cl->Name ) )
                cl->Send_TextMsgLex( cl, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, ban->GetBanLexems() );
            cl->Send_TextMsgLex( cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, Str::FormatBuf( "$time%u", GetBanTime( *ban ) ) );
            cl->Disconnect();
            return;
        }
    }

    // Check login/password
    if( !Singleplayer )
    {
        uint name_len_utf8 = Str::LengthUTF8( cl->Name );
        if( name_len_utf8 < MIN_NAME || name_len_utf8 < GameOpt.MinNameLength || name_len_utf8 > MAX_NAME || name_len_utf8 > GameOpt.MaxNameLength )
        {
            cl->Send_TextMsg( cl, STR_NET_WRONG_LOGIN, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        if( !Str::IsValidUTF8( cl->Name ) || Str::Substring( cl->Name, "*" ) )
        {
            // WriteLogF(_FUNC_," - Wrong chars: Name or Password, client '%s'.\n",cl->Name);
            cl->Send_TextMsg( cl, STR_NET_WRONG_DATA, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }
    }

    // Get client account data
    uint       id = MAKE_CLIENT_ID( cl->Name );
    ClientData data;
    if( !Singleplayer )
    {
        SCOPE_LOCK( ClientsDataLocker );

        ClientData* data_ = GetClientData( id );
        if( !data_ || memcmp( pass_hash, data_->ClientPassHash, PASS_HASH_SIZE ) )
        {
            // WriteLogF(_FUNC_," - Wrong name '%s' or password.\n",cl->Name);
            cl->Send_TextMsg( cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        data = *data_;
    }
    else
    {
        Str::Copy( data.ClientName, SingleplayerSave.Name.c_str() );
        id = MAKE_CLIENT_ID( data.ClientName );
    }

    // Check for ban by name
    {
        SCOPE_LOCK( BannedLocker );

        ClientBanned* ban = GetBanByName( data.ClientName );
        if( ban && !Singleplayer )
        {
            cl->Send_TextMsg( cl, STR_NET_BANNED, SAY_NETMSG, TEXTMSG_GAME );
            cl->Send_TextMsgLex( cl, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, ban->GetBanLexems() );
            cl->Send_TextMsgLex( cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, Str::FormatBuf( "$time%u", GetBanTime( *ban ) ) );
            cl->Disconnect();
            return;
        }
    }

    // Request script
    bool allow = false;
    uint disallow_msg_num = 0, disallow_str_num = 0;
    char disallow_lexems[ MAX_FOTEXT ];
    if( Script::PrepareContext( ServerFunctions.PlayerLogin, _FUNC_, data.ClientName ) )
    {
        ScriptString* name = ScriptString::Create( data.ClientName );
        ScriptString* lexems = ScriptString::Create();
        Script::SetArgUInt( cl->GetIp() );
        Script::SetArgObject( name );
        Script::SetArgUInt( id );
        Script::SetArgAddress( &disallow_msg_num );
        Script::SetArgAddress( &disallow_str_num );
        Script::SetArgObject( lexems );
        if( Script::RunPrepared() && Script::GetReturnedBool() )
            allow = true;
        Str::Copy( disallow_lexems, lexems->c_str() );
        name->Release();
        lexems->Release();
    }
    if( !allow )
    {
        if( disallow_msg_num < TEXTMSG_COUNT && disallow_str_num )
            cl->Send_TextMsgLex( cl, disallow_str_num, SAY_NETMSG, disallow_msg_num, disallow_lexems );
        else
            cl->Send_TextMsg( cl, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    // Copy data
    Str::Copy( cl->Name, data.ClientName );
    cl->RefreshName();

    // Check UIDS
    #ifndef DEV_VERSION
    if( GameOpt.AccountPlayTime && !Singleplayer )
    {
        int uid_zero = 0;
        for( int i = 0; i < 5; i++ )
            if( !uid[ i ] )
                uid_zero++;
        if( uid_zero > 2 )
        {
            WriteLogF( _FUNC_, " - Received more than two zero UIDs, client '%s'.\n", cl->Name );
            cl->Send_TextMsg( cl, STR_NET_UID_FAIL, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        if( ( uid[ 0 ] && ( !FLAG( uid[ 0 ], 0x00800000 ) || FLAG( uid[ 0 ], 0x00000400 ) ) ) ||
            ( uid[ 1 ] && ( !FLAG( uid[ 1 ], 0x04000000 ) || FLAG( uid[ 1 ], 0x00200000 ) ) ) ||
            ( uid[ 2 ] && ( !FLAG( uid[ 2 ], 0x00000020 ) || FLAG( uid[ 2 ], 0x00000800 ) ) ) ||
            ( uid[ 3 ] && ( !FLAG( uid[ 3 ], 0x80000000 ) || FLAG( uid[ 3 ], 0x40000000 ) ) ) ||
            ( uid[ 4 ] && ( !FLAG( uid[ 4 ], 0x00000800 ) || FLAG( uid[ 4 ], 0x00004000 ) ) ) )
        {
            WriteLogF( _FUNC_, " - Invalid UIDs, client '%s'.\n", cl->Name );
            cl->Send_TextMsg( cl, STR_NET_UID_FAIL, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        uint uidxor_ = 0xF145668A, uidor_ = 0, uidcalc_ = 0x45012345;
        for( int i = 0; i < 5; i++ )
        {
            uidxor_ ^= uid[ i ];
            uidor_ |= uid[ i ];
            uidcalc_ += uid[ i ];
        }

        if( uidxor != uidxor_ || uidor != uidor_ || uidcalc != uidcalc_ )
        {
            WriteLogF( _FUNC_, " - Invalid UIDs hash, client '%s'.\n", cl->Name );
            cl->Send_TextMsg( cl, STR_NET_UID_FAIL, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        SCOPE_LOCK( ClientsDataLocker );

        uint tick = Timer::FastTick();
        for( auto it = ClientsData.begin(), end = ClientsData.end(); it != end; ++it )
        {
            ClientData* cd = it->second;
            if( it->first == id )
                continue;

            int matches = 0;
            if( !uid[ 0 ] || uid[ 0 ] == cd->UID[ 0 ] )
                matches++;
            if( !uid[ 1 ] || uid[ 1 ] == cd->UID[ 1 ] )
                matches++;
            if( !uid[ 2 ] || uid[ 2 ] == cd->UID[ 2 ] )
                matches++;
            if( !uid[ 3 ] || uid[ 3 ] == cd->UID[ 3 ] )
                matches++;
            if( !uid[ 4 ] || uid[ 4 ] == cd->UID[ 4 ] )
                matches++;

            if( matches >= 5 )
            {
                if( !cd->UIDEndTick || tick >= cd->UIDEndTick )
                {
                    for( int i = 0; i < 5; i++ )
                        cd->UID[ i ] = 0;
                    cd->UIDEndTick = 0;
                }
                else
                {
                    WriteLogF( _FUNC_, " - UID already used by critter '%s', client '%s'.\n", cd->ClientName, cl->Name );
                    cl->Send_TextMsg( cl, STR_NET_UID_FAIL, SAY_NETMSG, TEXTMSG_GAME );
                    cl->Disconnect();
                    return;
                }
            }
        }

        for( int i = 0; i < 5; i++ )
        {
            if( data.UID[ i ] != uid[ i ] )
            {
                if( !data.UIDEndTick || tick >= data.UIDEndTick )
                {
                    // Set new uids on this account and start play timeout
                    ClientData* data_ = GetClientData( id );
                    for( int j = 0; j < 5; j++ )
                        data_->UID[ j ] = uid[ j ];
                    data_->UIDEndTick = tick + GameOpt.AccountPlayTime * 1000;
                }
                else
                {
                    WriteLogF( _FUNC_, " - Different UID %d, client '%s'.\n", i, cl->Name );
                    cl->Send_TextMsg( cl, STR_NET_UID_FAIL, SAY_NETMSG, TEXTMSG_GAME );
                    cl->Disconnect();
                    return;
                }
                break;
            }
        }
    }
    #endif

    // Find in players in game
    Client* cl_old = CrMngr.GetPlayer( id, true );
    RUNTIME_ASSERT( cl != cl_old );

    // Avatar in game
    if( cl_old )
    {
        ConnectedClientsLocker.Lock();

        // Check founded critter for online
        if( std::find( ConnectedClients.begin(), ConnectedClients.end(), cl_old ) != ConnectedClients.end() )
        {
            ConnectedClientsLocker.Unlock();
            cl_old->Send_TextMsg( cl, STR_NET_KNOCK_KNOCK, SAY_NETMSG, TEXTMSG_GAME );
            cl->Send_TextMsg( cl, STR_NET_PLAYER_IN_GAME, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        // Find current client in online collection
        auto it = std::find( ConnectedClients.begin(), ConnectedClients.end(), cl );
        if( it == ConnectedClients.end() )
        {
            ConnectedClientsLocker.Unlock();
            cl->Disconnect();
            return;
        }

        // Swap
        BIN_END( cl );

        // Swap data that used in NetIO_* functions
        #if defined ( USE_LIBEVENT )
        Client::NetIOArg* io_arg = cl->NetIOArgPtr;
        io_arg->Locker.Lock();
        #else // IOCP
        cl->NetIOIn->Locker.Lock();
        cl->NetIOOut->Locker.Lock();
        cl_old->NetIOIn->Locker.Lock();
        cl_old->NetIOOut->Locker.Lock();
        #endif

        // Current critter dropped or previous still online
        // Cancel swapping
        if( cl->Sock == INVALID_SOCKET || cl_old->Sock != INVALID_SOCKET )
        {
            #if defined ( USE_LIBEVENT )
            io_arg->Locker.Unlock();
            #else // IOCP
            cl_old->NetIOOut->Locker.Unlock();
            cl_old->NetIOIn->Locker.Unlock();
            cl->NetIOOut->Locker.Unlock();
            cl->NetIOIn->Locker.Unlock();
            #endif
            ConnectedClientsLocker.Unlock();
            cl->Disconnect();
            BIN_BEGIN( cl );
            return;
        }

        // Assign to ConnectedClients
        cl_old->AddRef();
        *it = cl_old;

        // Swap net data
        cl_old->Bin = cl->Bin;
        cl_old->Bout = cl->Bout;
        std::swap( cl_old->IsDisconnected, cl->IsDisconnected );
        std::swap( cl_old->DisconnectTick, cl->DisconnectTick );
        cl_old->GameState = STATE_CONNECTED;
        cl->GameState = STATE_NONE;
        cl_old->Sock = cl->Sock;
        cl->Sock = INVALID_SOCKET;
        memcpy( &cl_old->From, &cl->From, sizeof( cl_old->From ) );
        cl_old->LastActivityTime = 0;
        UNSETFLAG( cl_old->Flags, FCRIT_DISCONNECT );
        SETFLAG( cl->Flags, FCRIT_DISCONNECT );
        std::swap( cl_old->Zstrm, cl->Zstrm );

        #if defined ( USE_LIBEVENT )
        // Assign for net arg old pointer
        // Current client delete NetIOArgPtr in destructor
        cl->Release();
        cl_old->AddRef();
        io_arg->PClient = cl_old;
        std::swap( cl_old->NetIOArgPtr, cl->NetIOArgPtr );
        #else // IOCP
        std::swap( cl_old->NetIOIn, cl->NetIOIn );
        std::swap( cl_old->NetIOOut, cl->NetIOOut );
        cl_old->NetIOIn->PClient = cl_old;
        cl_old->NetIOOut->PClient = cl_old;
        cl->NetIOIn->PClient = cl;
        cl->NetIOOut->PClient = cl;
        #endif

        cl->IsDestroyed = true;
        cl_old->IsDestroyed = false;

        #if defined ( USE_LIBEVENT )
        io_arg->Locker.Unlock();
        #else // IOCP
        cl->NetIOOut->Locker.Unlock();
        cl->NetIOIn->Locker.Unlock();
        cl_old->NetIOOut->Locker.Unlock();
        cl_old->NetIOIn->Locker.Unlock();
        #endif

        Job::DeferredRelease( cl );
        cl = cl_old;

        ConnectedClientsLocker.Unlock();
        BIN_BEGIN( cl );

        // Erase from save
        EraseSaveClient( cl->GetId() );

        cl->SendA_Action( ACTION_CONNECT, 0, nullptr );
    }
    // Avatar not in game
    else
    {
        cl->SetId( id );

        // Singleplayer data
        if( Singleplayer )
        {
            Str::Copy( cl->Name, SingleplayerSave.Name.c_str() );
            cl->Props = *SingleplayerSave.CrProps;
        }

        // Find in saved array
        SaveClientsLocker.Lock();
        Client* cl_saved = nullptr;
        for( auto it = SaveClients.begin(), end = SaveClients.end(); it != end; ++it )
        {
            Client* cl_ = *it;
            if( cl_->GetId() == id )
            {
                cl_saved = cl_;
                break;
            }
        }
        SaveClientsLocker.Unlock();

        if( !cl_saved && !LoadClient( cl ) )
        {
            WriteLogF( _FUNC_, " - Error load from data base, client '%s'.\n", cl->GetInfo() );
            cl->Send_TextMsg( cl, STR_NET_BD_ERROR, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        if( cl_saved )
            cl->Props = cl_saved->Props;

        // Find items
        ItemMngr.SetCritterItems( cl );

        // Add to map
        Map* map = MapMngr.GetMap( cl->GetMapId() );
        if( cl->GetMapId() )
        {
            if( !map || map->GetIsNoLogOut() || map->GetProtoId() != cl->GetMapPid() )
            {
                ushort hx, hy;
                uchar  dir;
                if( map && map->GetProtoId() == cl->GetMapPid() && map->GetStartCoord( hx, hy, dir, ENTIRE_LOG_OUT ) )
                {
                    cl->SetHexX( hx );
                    cl->SetHexY( hy );
                    cl->SetDir( dir );
                }
                else
                {
                    map = nullptr;
                    cl->SetMapId( 0 );
                    cl->SetMapPid( 0 );
                    cl->SetHexX( 0 );
                    cl->SetHexY( 0 );
                }
            }
        }

        if( !cl->GetMapId() && ( cl->GetHexX() || cl->GetHexY() ) )
        {
            Critter* rule = CrMngr.GetCritter( cl->GetGlobalGroupRuleId(), false );
            if( !rule || rule->GetMapId() || cl->GetGlobalGroupUid() != rule->GetGlobalGroupUid() )
                cl->SetGlobalGroupRuleId( 0 );
        }

        if( !MapMngr.AddCrToMap( cl, map, cl->GetHexX(), cl->GetHexY(), 1, false ) )
        {
            WriteLogF( _FUNC_, " - Error add critter to map, client '%s'.\n", cl->GetInfo() );
            cl->Send_TextMsg( cl, STR_NET_HEXES_BUSY, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        if( cl_saved )
            EraseSaveClient( id );

        // Map event
        if( map )
            map->AddCritterEvents( cl );

        // Add car, if on global
        if( !cl->GetMapId() && !cl->GroupMove->CarId )
        {
            Item* car = cl->GetItemCar();
            if( car )
                cl->GroupMove->CarId = car->GetId();
        }

        cl->SetTimeoutTransfer( 0 );
        cl->AddRef();
        EntityMngr.RegisterEntity( cl );
        Job::PushBack( JOB_CRITTER, cl );

        cl->DisableSend++;
        if( cl->GetMapId() )
        {
            cl->ProcessVisibleCritters();
            cl->ProcessVisibleItems();
        }
        if( Script::PrepareContext( ServerFunctions.CritterInit, _FUNC_, cl->GetInfo() ) )
        {
            Script::SetArgEntity( cl );
            Script::SetArgBool( false );
            Script::RunPrepared();
        }
        if( !cl_old )
            cl->SetScript( nullptr, false );
        cl->DisableSend--;
    }

    cl->SetDefaultCombat( default_combat_mode );
    for( int i = 0; i < 5; i++ )
        cl->UID[ i ] = uid[ i ];

    // Connection info
    uint         ip = cl->GetIp();
    ushort       port = cl->GetPort();
    ScriptArray* conn_ip = cl->GetConnectionIp();
    ScriptArray* conn_port = cl->GetConnectionPort();
    RUNTIME_ASSERT( conn_ip->GetSize() == conn_port->GetSize() );
    bool         ip_found = false;
    for( uint i = 0, j = conn_ip->GetSize(); i < j; i++ )
    {
        if( *(uint*) conn_ip->At( i ) == ip )
        {
            if( i < j - 1 )
            {
                conn_ip->RemoveAt( i );
                conn_ip->InsertLast( &ip );
                cl->SetConnectionIp( conn_ip );
                conn_port->RemoveAt( i );
                conn_port->InsertLast( &port );
                cl->SetConnectionPort( conn_port );
            }
            else if( *(ushort*) conn_port->At( j - 1 ) != port )
            {
                conn_port->SetValue( i, &port );
                cl->SetConnectionPort( conn_port );
            }
            ip_found = true;
            break;
        }
    }
    if( !ip_found )
    {
        conn_ip->InsertLast( &ip );
        cl->SetConnectionIp( conn_ip );
        conn_port->InsertLast( &port );
        cl->SetConnectionPort( conn_port );
    }
    SAFEREL( conn_ip );
    SAFEREL( conn_port );

    // Login ok
    uint       msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uint ) * 2;
    uint       bin_seed = Random( 100000, 2000000000 );
    uint       bout_seed = Random( 100000, 2000000000 );
    PUCharVec* global_vars_data;
    UIntVec*   global_vars_data_sizes;
    uint       whole_data_size = Globals->Props.StoreData( false, &global_vars_data, &global_vars_data_sizes );
    msg_len += sizeof( ushort ) + whole_data_size;
    BOUT_BEGIN( cl );
    cl->Bout << NETMSG_LOGIN_SUCCESS;
    cl->Bout << msg_len;
    cl->Bout << bin_seed;
    cl->Bout << bout_seed;
    NET_WRITE_PROPERTIES( cl->Bout, global_vars_data, global_vars_data_sizes );
    BOUT_END( cl );

    cl->Bin.SetEncryptKey( bin_seed );
    cl->Bout.SetEncryptKey( bout_seed );

    cl->Send_LoadMap( nullptr );
}

void FOServer::Process_SingleplayerSaveLoad( Client* cl )
{
    if( !Singleplayer )
    {
        cl->Disconnect();
        return;
    }

    uint     msg_len;
    bool     save;
    ushort   fname_len;
    char     fname[ MAX_FOTEXT ];
    UCharVec pic_data;
    cl->Bin >> msg_len;
    cl->Bin >> save;
    cl->Bin >> fname_len;
    cl->Bin.Pop( fname, fname_len );
    fname[ fname_len ] = 0;
    if( save )
    {
        uint pic_data_len;
        cl->Bin >> pic_data_len;
        pic_data.resize( pic_data_len );
        if( pic_data_len )
            cl->Bin.Pop( (char*) &pic_data[ 0 ], pic_data_len );
    }

    CHECK_IN_BUFF_ERROR( cl );

    if( save )
    {
        SingleplayerSave.PicData = pic_data;

        SaveWorld( fname );
        cl->Send_TextMsg( cl, STR_SP_SAVE_SUCCESS, SAY_NETMSG, TEXTMSG_GAME );
    }
    else
    {
        SynchronizeLogicThreads();
        SyncManager::GetForCurThread()->UnlockAll();
        SYNC_LOCK( cl );
        if( !LoadWorld( fname ) )
        {
            WriteLogF( _FUNC_, " - Unable load world from file '%s'.\n", fname );
            cl->Send_TextMsg( cl, STR_SP_LOAD_FAIL, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            ResynchronizeLogicThreads();
            return;
        }
        ResynchronizeLogicThreads();

        cl->Send_TextMsg( cl, STR_SP_LOAD_SUCCESS, SAY_NETMSG, TEXTMSG_GAME );

        BOUT_BEGIN( cl );
        cl->Bout << (uint) NETMSG_REGISTER_SUCCESS;
        BOUT_END( cl );
        cl->Disconnect();
    }
}

void FOServer::Process_ParseToGame( Client* cl )
{
    if( !cl->GetId() || !CrMngr.GetPlayer( cl->GetId(), false ) )
        return;
    cl->SetBreakTime( GameOpt.Breaktime );

    #ifdef DEV_VERSION
    cl->Access = ACCESS_ADMIN;
    #endif

    cl->GameState = STATE_PLAYING;
    cl->PingOk( PING_CLIENT_LIFE_TIME * 5 );

    if( cl->ViewMapId )
    {
        Map* map = MapMngr.GetMap( cl->ViewMapId );
        cl->ViewMapId = 0;
        if( map )
        {
            cl->ViewMap( map, cl->ViewMapLook, cl->ViewMapHx, cl->ViewMapHy, cl->ViewMapDir );
            cl->Send_ViewMap();
            return;
        }
    }

    // Parse
    Map* map = MapMngr.GetMap( cl->GetMapId(), true );
    cl->Send_GameInfo( map );
    cl->DropTimers( true );

    // Parse to global
    if( !cl->GetMapId() )
    {
        if( !cl->GroupMove )
        {
            WriteLogF( _FUNC_, " - Group nullptr, client '%s'.\n", cl->GetInfo() );
            cl->Disconnect();
            return;
        }

        cl->Send_GlobalInfo( GM_INFO_ALL );
        for( auto it = cl->GroupMove->CritMove.begin(), end = cl->GroupMove->CritMove.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr != cl )
                cr->Send_CustomCommand( cl, OTHER_FLAGS, cl->Flags );
        }
        cl->Send_HoloInfo( true, 0, 0 );
        cl->Send_AllAutomapsInfo();
        if( cl->Talk.TalkType != TALK_NONE )
        {
            cl->ProcessTalk( true );
            cl->Send_Talk();
        }
    }
    else
    {
        if( !map )
        {
            WriteLogF( _FUNC_, " - Map not found, client '%s'.\n", cl->GetInfo() );
            cl->Disconnect();
            return;
        }

        // Parse to local
        SETFLAG( cl->Flags, FCRIT_CHOSEN );
        cl->Send_AddCritter( cl );
        UNSETFLAG( cl->Flags, FCRIT_CHOSEN );

        // Send all data
        cl->Send_AddAllItems();
        cl->Send_HoloInfo( true, 0, 0 );
        cl->Send_AllAutomapsInfo();

        // Send current critters
        CrVec critters = cl->VisCrSelf;
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            SYNC_LOCK( cr );
            cl->Send_AddCritter( cr );
        }

        // Send current items on map
        cl->VisItemLocker.Lock();
        UIntSet items = cl->VisItem;
        cl->VisItemLocker.Unlock();
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
        {
            Item* item = ItemMngr.GetItem( *it, false );
            if( item )
                cl->Send_AddItemOnMap( item );
        }

        // Check active talk
        if( cl->Talk.TalkType != TALK_NONE )
        {
            cl->ProcessTalk( true );
            cl->Send_Talk();
        }

        // Turn based
        if( map->IsTurnBasedOn )
        {
            if( map->IsCritterTurn( cl ) )
            {
                cl->Send_CustomCommand( cl, OTHER_YOU_TURN, map->GetCritterTurnTime() );
            }
            else
            {
                Critter* cr = cl->GetCritSelf( map->GetCritterTurnId(), false );
                if( cr )
                    cl->Send_CustomCommand( cr, OTHER_YOU_TURN, map->GetCritterTurnTime() );
            }
        }
        else if( TB_BATTLE_TIMEOUT_CHECK( cl->GetTimeoutBattle() ) )
        {
            cl->SetTimeoutBattle( 0 );
        }
    }

    // Notify about end of parsing
    cl->Send_CustomMessage( NETMSG_END_PARSE_TO_GAME );
}

void FOServer::Process_GiveMap( Client* cl )
{
    bool automap;
    hash map_pid;
    uint loc_id;
    hash hash_tiles;
    hash hash_scen;

    cl->Bin >> automap;
    cl->Bin >> map_pid;
    cl->Bin >> loc_id;
    cl->Bin >> hash_tiles;
    cl->Bin >> hash_scen;
    CHECK_IN_BUFF_ERROR( cl );

    if( !Singleplayer )
    {
        uint tick = Timer::FastTick();
        if( tick - cl->LastSendedMapTick < GameOpt.Breaktime * 3 )
            cl->SetBreakTime( GameOpt.Breaktime * 3 );
        else
            cl->SetBreakTime( GameOpt.Breaktime );
        cl->LastSendedMapTick = tick;
    }

    ProtoMap* pmap = ProtoMngr.GetProtoMap( map_pid );
    if( !pmap )
    {
        WriteLogF( _FUNC_, " - Map prototype not found, client '%s'.\n", cl->GetInfo() );
        cl->Disconnect();
        return;
    }

    if( !automap && map_pid != cl->GetMapPid() && cl->ViewMapPid != map_pid )
    {
        WriteLogF( _FUNC_, " - Request for loading incorrect map, client '%s'.\n", cl->GetInfo() );
        return;
    }

    if( automap )
    {
        if( !cl->CheckKnownLocById( loc_id ) )
        {
            WriteLogF( _FUNC_, " - Request for loading unknown automap, client '%s'.\n", cl->GetInfo() );
            return;
        }

        Location* loc = MapMngr.GetLocation( loc_id );
        if( !loc )
        {
            WriteLogF( _FUNC_, " - Request for loading incorrect automap, client '%s'.\n", cl->GetInfo() );
            return;
        }

        bool         found = false;
        ScriptArray* automaps = ( loc->IsAutomaps() ? loc->GetAutomaps() : nullptr );
        if( automaps )
        {
            for( uint i = 0, j = automaps->GetSize(); i < j && !found; i++ )
                if( *(hash*) automaps->At( i ) == map_pid )
                    found = true;
        }
        if( !found )
        {
            WriteLogF( _FUNC_, " - Request for loading incorrect automap, client '%s'.\n", cl->GetInfo() );
            return;
        }
    }

    Send_MapData( cl, pmap, pmap->HashTiles != hash_tiles, pmap->HashScen != hash_scen );

    if( !automap )
    {
        Map* map = nullptr;
        if( cl->ViewMapId )
            map = MapMngr.GetMap( cl->ViewMapId );
        cl->Send_LoadMap( map );
    }
}

void FOServer::Send_MapData( Client* cl, ProtoMap* pmap, bool send_tiles, bool send_scenery )
{
    uint   msg = NETMSG_MAP;
    hash   map_pid = pmap->ProtoId;
    ushort maxhx = pmap->GetWidth();
    ushort maxhy = pmap->GetHeight();
    uint   msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( map_pid ) + sizeof( maxhx ) + sizeof( maxhy ) + sizeof( bool ) * 2;

    if( send_tiles )
        msg_len += sizeof( uint ) + (uint) pmap->Tiles.size() * sizeof( ProtoMap::Tile );
    if( send_scenery )
        msg_len += sizeof( uint ) + (uint) pmap->SceneryData.size();

    // Header
    BOUT_BEGIN( cl );
    cl->Bout << msg;
    cl->Bout << msg_len;
    cl->Bout << map_pid;
    cl->Bout << maxhx;
    cl->Bout << maxhy;
    cl->Bout << send_tiles;
    cl->Bout << send_scenery;
    if( send_tiles )
    {
        cl->Bout << (uint) pmap->Tiles.size() * sizeof( ProtoMap::Tile );
        if( pmap->Tiles.size() )
            cl->Bout.Push( (char*) &pmap->Tiles[ 0 ], (uint) pmap->Tiles.size() * sizeof( ProtoMap::Tile ) );
    }
    if( send_scenery )
    {
        cl->Bout << (uint) pmap->SceneryData.size();
        if( pmap->SceneryData.size() )
            cl->Bout.Push( (char*) &pmap->SceneryData[ 0 ], (uint) pmap->SceneryData.size() );
    }
    BOUT_END( cl );
}

void FOServer::Process_Move( Client* cl )
{
    uint   move_params;
    ushort hx;
    ushort hy;

    cl->Bin >> move_params;
    cl->Bin >> hx;
    cl->Bin >> hy;
    CHECK_IN_BUFF_ERROR( cl );

    if( !cl->GetMapId() )
        return;

    // The player informs that has stopped
    if( FLAG( move_params, MOVE_PARAM_STEP_DISALLOW ) )
    {
        // cl->Send_XY(cl);
        cl->SendA_XY();
        return;
    }

    // Check running availability
    bool is_run = FLAG( move_params, MOVE_PARAM_RUN );
    if( is_run )
    {
        if( !cl->IsCanRun() ||
            ( GameOpt.RunOnCombat ? false : IS_TIMEOUT( cl->GetTimeoutBattle() ) ) ||
            ( GameOpt.RunOnTransfer ? false : IS_TIMEOUT( cl->GetTimeoutTransfer() ) ) )
        {
            // Switch to walk
            move_params ^= MOVE_PARAM_RUN;
            is_run = false;
        }
    }

    // Check walking availability
    if( !is_run )
    {
        if( !cl->IsCanWalk() )
        {
            cl->Send_XY( cl );
            return;
        }
    }

    // Overweight
    uint cur_weight = cl->GetItemsWeight();
    uint max_weight = cl->GetCarryWeight();
    if( cur_weight > max_weight )
    {
        if( is_run || cur_weight > max_weight * 2 )
        {
            cl->Send_XY( cl );
            return;
        }
    }

    // Lame
    if( cl->IsDmgLeg() && is_run )
    {
        cl->Send_XY( cl );
        return;
    }

    // Check dist
    if( !CheckDist( cl->GetHexX(), cl->GetHexY(), hx, hy, 1 ) )
    {
        cl->Send_XY( cl );
        return;
    }

    // Try move
    Act_Move( cl, hx, hy, move_params );
}

void FOServer::Process_ChangeItem( Client* cl )
{
    uint  item_id;
    uchar from_slot;
    uchar to_slot;
    uint  count;

    cl->Bin >> item_id;
    cl->Bin >> from_slot;
    cl->Bin >> to_slot;
    cl->Bin >> count;
    CHECK_IN_BUFF_ERROR( cl );

    cl->SetBreakTime( GameOpt.Breaktime );

    if( !cl->CheckMyTurn( nullptr ) )
    {
        WriteLogF( _FUNC_, " - Is not client '%s' turn.\n", cl->GetInfo() );
        cl->Send_AddAllItems();
        return;
    }

    bool is_castling = ( ( from_slot == SLOT_HAND1 && to_slot == SLOT_HAND2 ) || ( from_slot == SLOT_HAND2 && to_slot == SLOT_HAND1 ) );
    int  ap_cost = ( is_castling ? 0 : cl->GetApCostMoveItemInventory() );
    if( to_slot == SLOT_GROUND )
        ap_cost = cl->GetApCostDropItem();
    if( cl->GetCurrentAp() / AP_DIVIDER < ap_cost && !Singleplayer )
    {
        WriteLogF( _FUNC_, " - Not enough AP, client '%s'.\n", cl->GetInfo() );
        cl->Send_AddAllItems();
        return;
    }
    cl->SubAp( ap_cost );

    Map* map = MapMngr.GetMap( cl->GetMapId() );
    if( map && map->IsTurnBasedOn && !cl->GetAllAp() )
        map->EndCritterTurn();

    // Move
    if( !cl->MoveItem( from_slot, to_slot, item_id, count ) )
    {
        WriteLogF( _FUNC_, " - Move item fail, from %u, to %u, item_id %u, client '%s'.\n", from_slot, to_slot, item_id, cl->GetInfo() );
        cl->Send_AddAllItems();
    }
}

void FOServer::Process_UseItem( Client* cl )
{
    uint  item_id;
    uchar rate;
    uchar target_type;
    uint  target_id;
    hash  target_pid;
    uint  param;

    cl->Bin >> item_id;
    cl->Bin >> rate;
    cl->Bin >> target_type;
    cl->Bin >> target_id;
    cl->Bin >> target_pid;
    cl->Bin >> param;
    CHECK_IN_BUFF_ERROR( cl );

    if( !cl->IsLife() )
        return;

    uchar use = rate & 0xF;
    Item* item = ( item_id ? cl->GetItem( item_id, true ) : cl->ItemSlotMain );
    if( !item )
    {
        WriteLogF( _FUNC_, " - Item not found, item id %u, client '%s'.\n", item_id, cl->GetInfo() );
        return;
    }

    if( item->IsWeapon() && use != USE_USE )
    {
        switch( use )
        {
        case USE_PRIMARY:
        case USE_SECONDARY:
        case USE_THIRD:
            if( !cl->GetMapId() )
                break;
            if( item != cl->ItemSlotMain )
                break;

            Act_Attack( cl, rate, target_id );
            break;
        case USE_RELOAD:
            Act_Reload( cl, item->GetId(), target_id );
            break;
        default:
            cl->Send_TextMsg( cl, STR_USE_NOTHING, SAY_NETMSG, TEXTMSG_GAME );
            break;
        }
    }
    else if( use == USE_USE )
    {
        if( !item_id )
            return;
        if( target_type != TARGET_CRITTER && target_type != TARGET_ITEM && target_type != TARGET_SCENERY &&
            target_type != TARGET_SELF && target_type != TARGET_SELF_ITEM )
            return;
        if( !cl->GetMapId() && ( target_type == TARGET_ITEM || target_type == TARGET_SCENERY ) )
            return;
        if( !Act_Use( cl, item_id, -1, target_type, target_id, target_pid, param ) )
            cl->Send_TextMsg( cl, STR_USE_NOTHING, SAY_NETMSG, TEXTMSG_GAME );
    }
    else
    {
        cl->Send_TextMsg( cl, STR_USE_NOTHING, SAY_NETMSG, TEXTMSG_GAME );
    }
}

void FOServer::Process_PickItem( Client* cl )
{
    ushort targ_x;
    ushort targ_y;
    hash   pid;

    cl->Bin >> targ_x;
    cl->Bin >> targ_y;
    cl->Bin >> pid;
    CHECK_IN_BUFF_ERROR( cl );

    Act_PickItem( cl, targ_x, targ_y, pid );
}

void FOServer::Process_PickCritter( Client* cl )
{
    uint  crid;
    uchar pick_type;

    cl->Bin >> crid;
    cl->Bin >> pick_type;
    CHECK_IN_BUFF_ERROR( cl );

    cl->SetBreakTime( GameOpt.Breaktime );

    if( !cl->CheckMyTurn( nullptr ) )
    {
        WriteLogF( _FUNC_, " - Is not critter '%s' turn.\n", cl->GetInfo() );
        return;
    }

    int ap_cost = cl->GetApCostPickCritter();
    if( cl->GetCurrentAp() / AP_DIVIDER < ap_cost && !Singleplayer )
    {
        WriteLogF( _FUNC_, " - Not enough AP, critter '%s'.\n", cl->GetInfo() );
        return;
    }
    cl->SubAp( ap_cost );

    Map* map = MapMngr.GetMap( cl->GetMapId() );
    if( map && map->IsTurnBasedOn && !cl->GetAllAp() )
        map->EndCritterTurn();

    Critter* cr = cl->GetCritSelf( crid, true );
    if( !cr )
        return;

    switch( pick_type )
    {
    case PICK_CRIT_LOOT:
        if( !cr->IsDead() )
            break;
        if( cr->GetIsNoLoot() )
            break;
        if( !CheckDist( cl->GetHexX(), cl->GetHexY(), cr->GetHexX(), cr->GetHexY(), cl->GetUseDist() + cr->GetMultihex() ) )
        {
            cl->Send_XY( cl );
            cl->Send_XY( cr );
            break;
        }

        // Script events
        if( cl->EventUseSkill( SKILL_LOOT_CRITTER, cr, nullptr, nullptr ) )
            return;
        if( cr->EventUseSkillOnMe( cl, SKILL_LOOT_CRITTER ) )
            return;
        if( Script::PrepareContext( ServerFunctions.CritterUseSkill, _FUNC_, cl->GetInfo() ) )
        {
            Script::SetArgEntity( cl );
            Script::SetArgUInt( SKILL_LOOT_CRITTER );
            Script::SetArgEntity( cr );
            Script::SetArgEntity( nullptr );
            Script::SetArgEntity( nullptr );
            if( Script::RunPrepared() && Script::GetReturnedBool() )
                return;
        }

        // Default process
        cl->Send_ContainerInfo( cr, TRANSFER_CRIT_LOOT, true );
        break;
    case PICK_CRIT_PUSH:
        if( !cr->IsLife() )
            break;
        if( cr->GetIsNoPush() )
            break;
        if( !CheckDist( cl->GetHexX(), cl->GetHexY(), cr->GetHexX(), cr->GetHexY(), cl->GetUseDist() + cr->GetMultihex() ) )
        {
            cl->Send_XY( cl );
            cl->Send_XY( cr );
            break;
        }

        // Script events
        if( cl->EventUseSkill( SKILL_PUSH_CRITTER, cr, nullptr, nullptr ) )
            return;
        if( cr->EventUseSkillOnMe( cl, SKILL_PUSH_CRITTER ) )
            return;
        if( Script::PrepareContext( ServerFunctions.CritterUseSkill, _FUNC_, cl->GetInfo() ) )
        {
            Script::SetArgEntity( cl );
            Script::SetArgUInt( SKILL_PUSH_CRITTER );
            Script::SetArgEntity( cr );
            Script::SetArgEntity( nullptr );
            Script::SetArgEntity( nullptr );
            if( Script::RunPrepared() && Script::GetReturnedBool() )
                return;
        }
        break;
    default:
        break;
    }
}

void FOServer::Process_ContainerItem( Client* cl )
{
    uchar transfer_type;
    uint  cont_id;
    uint  item_id;
    uint  item_count;
    uchar take_flags;

    cl->Bin >> transfer_type;
    cl->Bin >> cont_id;
    cl->Bin >> item_id;
    cl->Bin >> item_count;
    cl->Bin >> take_flags;
    CHECK_IN_BUFF_ERROR( cl );

    cl->SetBreakTime( GameOpt.Breaktime );

    if( !cl->CheckMyTurn( nullptr ) )
    {
        WriteLogF( _FUNC_, " - Is not client '%s' turn.\n", cl->GetInfo() );
        return;
    }

    if( cl->AccessContainerId != cont_id )
    {
        WriteLogF( _FUNC_, " - Try work with not accessed container, client '%s'.\n", cl->GetInfo() );
        cl->Send_ContainerInfo();
        return;
    }
    cl->ItemTransferCount = item_count;

    int ap_cost = cl->GetApCostMoveItemContainer();
    if( cl->GetCurrentAp() / AP_DIVIDER < ap_cost && !Singleplayer )
    {
        WriteLogF( _FUNC_, " - Not enough AP, client '%s'.\n", cl->GetInfo() );
        cl->Send_ContainerInfo();
        return;
    }
    cl->SubAp( ap_cost );

    Map* map = MapMngr.GetMap( cl->GetMapId() );
    if( map && map->IsTurnBasedOn && !cl->GetAllAp() )
        map->EndCritterTurn();

    if( !cl->GetMapId() && ( transfer_type != TRANSFER_CRIT_STEAL && transfer_type != TRANSFER_FAR_CONT && transfer_type != TRANSFER_FAR_CRIT &&
                             transfer_type != TRANSFER_CRIT_BARTER && transfer_type != TRANSFER_SELF_CONT ) )
        return;

/************************************************************************/
/* Item container                                                       */
/************************************************************************/
    if( transfer_type == TRANSFER_HEX_CONT_UP || transfer_type == TRANSFER_HEX_CONT_DOWN || transfer_type == TRANSFER_SELF_CONT || transfer_type == TRANSFER_FAR_CONT )
    {
        Item* cont;

        // Check and get hex cont
        if( transfer_type == TRANSFER_HEX_CONT_UP || transfer_type == TRANSFER_HEX_CONT_DOWN )
        {
            // Get item
            cont = ItemMngr.GetItem( cont_id, true );
            if( !cont || cont->GetAccessory() != ITEM_ACCESSORY_HEX || !cont->IsContainer() )
            {
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - TRANSFER_HEX_CONT error.\n" );
                return;
            }

            // Check map
            if( cont->GetMapId() != cl->GetMapId() )
            {
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - Attempt to take a subject from the container on other map.\n" );
                return;
            }

            // Check dist
            if( !CheckDist( cl->GetHexX(), cl->GetHexY(), cont->GetHexX(), cont->GetHexY(), cl->GetUseDist() ) )
            {
                cl->Send_XY( cl );
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - Transfer item container. Client '%s' distance more than allowed.\n", cl->GetInfo() );
                return;
            }

            // Check for close
            if( cont->GetContainer_Changeble() && !cont->LockerIsOpen() )
            {
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - Container is not open.\n" );
                return;
            }
        }
        // Get far container without checks
        else if( transfer_type == TRANSFER_FAR_CONT )
        {
            cont = ItemMngr.GetItem( cont_id, true );
            if( !cont || !cont->IsContainer() )
            {
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - TRANSFER_FAR_CONT error.\n" );
                return;
            }
        }
        // Check and get self cont
        else
        {
            // Get item
            cont = cl->GetItem( cont_id, true );
            if( !cont || !cont->IsContainer() )
            {
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - TRANSFER_SELF_CONT error2.\n" );
                return;
            }
        }

        // Process
        switch( take_flags )
        {
        case CONT_GET:
        {
            // Get item
            Item* item = cont->ContGetItem( item_id, true );
            if( !item )
            {
                cl->Send_ContainerInfo( cont, transfer_type, false );
                cl->Send_TextMsg( cl, STR_ITEM_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
                return;
            }

            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 0, item );

            // Check count
            if( !item_count || item->GetCount() < item_count )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Error count.", SAY_NETMSG );
                return;
            }

            // Check weight
            if( cl->GetFreeWeight() < (int) ( item->GetWeight() * item_count ) )
            {
                cl->Send_TextMsg( cl, STR_OVERWEIGHT, SAY_NETMSG, TEXTMSG_GAME );
                break;
            }

            // Check volume
            if( cl->GetFreeVolume() < (int) ( item->GetVolume() * item_count ) )
            {
                cl->Send_TextMsg( cl, STR_OVERVOLUME, SAY_NETMSG, TEXTMSG_GAME );
                break;
            }

            // Script events
            if( item->EventSkill( cl, SKILL_TAKE_CONT ) )
                return;
            if( cl->EventUseSkill( SKILL_TAKE_CONT, nullptr, item, nullptr ) )
                return;
            if( Script::PrepareContext( ServerFunctions.CritterUseSkill, _FUNC_, cl->GetInfo() ) )
            {
                Script::SetArgEntity( cl );
                Script::SetArgUInt( SKILL_TAKE_CONT );
                Script::SetArgEntity( nullptr );
                Script::SetArgEntity( item );
                Script::SetArgEntity( nullptr );
                if( Script::RunPrepared() && Script::GetReturnedBool() )
                    return;
            }

            // Transfer
            if( !ItemMngr.MoveItemCritterFromCont( cont, cl, item->GetId(), item_count ) )
                WriteLogF( _FUNC_, " - Transfer item, from container to player (get), fail.\n" );
        }
        break;
        case CONT_GETALL:
        {
            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 1, nullptr );

            // Get items
            ItemVec items;
            cont->ContGetAllItems( items, true, true );
            if( items.empty() )
            {
                cl->Send_ContainerInfo();
                return;
            }

            // Check weight, volume
            uint weight = 0, volume = 0;
            for( auto it = items.begin(), end = items.end(); it != end; ++it )
            {
                Item* item = *it;
                weight += item->GetWholeWeight();
                volume += item->GetWholeVolume();
            }

            if( cl->GetFreeWeight() < (int) weight )
            {
                cl->Send_TextMsg( cl, STR_OVERWEIGHT, SAY_NETMSG, TEXTMSG_GAME );
                break;
            }

            if( cl->GetFreeVolume() < (int) volume )
            {
                cl->Send_TextMsg( cl, STR_OVERVOLUME, SAY_NETMSG, TEXTMSG_GAME );
                break;
            }

            // Script events
            if( cont->EventSkill( cl, SKILL_TAKE_ALL_CONT ) )
                return;
            if( cl->EventUseSkill( SKILL_TAKE_ALL_CONT, nullptr, cont, nullptr ) )
                return;
            if( Script::PrepareContext( ServerFunctions.CritterUseSkill, _FUNC_, cl->GetInfo() ) )
            {
                Script::SetArgEntity( cl );
                Script::SetArgUInt( SKILL_TAKE_ALL_CONT );
                Script::SetArgEntity( nullptr );
                Script::SetArgEntity( cont );
                Script::SetArgEntity( nullptr );
                if( Script::RunPrepared() && Script::GetReturnedBool() )
                    return;
            }

            // Transfer
            for( auto it = items.begin(), end = items.end(); it != end; ++it )
            {
                Item* item = *it;
                if( !item->EventSkill( cl, SKILL_TAKE_CONT ) )
                {
                    cont->ContEraseItem( item );
                    cl->AddItem( item, true );
                }
            }
        }
        break;
        case CONT_PUT:
        {
            // Get item
            Item* item = cl->GetItem( item_id, true );
            if( !item || item->GetCritSlot() != SLOT_INV )
            {
                cl->Send_ContainerInfo();
                cl->Send_TextMsg( cl, STR_ITEM_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
                return;
            }

            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 2, item );

            // Check count
            if( !item_count || item->GetCount() < item_count )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Error count.", SAY_NETMSG );
                return;
            }

            // Check slot
            if( item->GetCritSlot() != SLOT_INV )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Cheat detected.", SAY_NETMSG );
                WriteLogF( _FUNC_, " - Attempting to put in a container not from the inventory, client '%s'.", cl->GetInfo() );
                return;
            }

            // Check volume
            if( !cont->ContHaveFreeVolume( 0, item->GetVolume() * item_count ) )
            {
                cl->Send_TextMsg( cl, STR_OVERVOLUME, SAY_NETMSG, TEXTMSG_GAME );
                break;
            }

            // Script events
            if( item->EventSkill( cl, SKILL_PUT_CONT ) )
                return;
            if( cl->EventUseSkill( SKILL_PUT_CONT, nullptr, item, nullptr ) )
                return;
            if( Script::PrepareContext( ServerFunctions.CritterUseSkill, _FUNC_, cl->GetInfo() ) )
            {
                Script::SetArgEntity( cl );
                Script::SetArgUInt( SKILL_PUT_CONT );
                Script::SetArgEntity( nullptr );
                Script::SetArgEntity( item );
                Script::SetArgEntity( nullptr );
                if( Script::RunPrepared() && Script::GetReturnedBool() )
                    return;
            }

            // Transfer
            if( !ItemMngr.MoveItemCritterToCont( cl, cont, item->GetId(), item_count, 0 ) )
                WriteLogF( _FUNC_, " - Transfer item, from player to container (put), fail.\n" );
        }
        break;
        case CONT_PUTALL:
        {
            cl->Send_ContainerInfo();
        }
        break;
        default:
            break;
        }

        cl->Send_ContainerInfo( cont, transfer_type, false );

    }
/************************************************************************/
/* Critter container                                                    */
/************************************************************************/
    else if( transfer_type == TRANSFER_CRIT_LOOT || transfer_type == TRANSFER_CRIT_STEAL || transfer_type == TRANSFER_FAR_CRIT )
    {
        bool is_far = ( transfer_type == TRANSFER_FAR_CRIT );
        bool is_steal = ( transfer_type == TRANSFER_CRIT_STEAL );
        bool is_loot = ( transfer_type == TRANSFER_CRIT_LOOT );

        // Get critter target
        Critter* cr = ( is_far ? CrMngr.GetCritter( cont_id, true ) : ( cl->GetMapId() ? cl->GetCritSelf( cont_id, true ) : cl->GroupMove->GetCritter( cont_id ) ) );
        if( !cr )
        {
            cl->Send_ContainerInfo();
            WriteLogF( _FUNC_, " - Critter not found.\n" );
            return;
        }
        SYNC_LOCK( cr );

        // Check for self
        if( cl == cr )
        {
            cl->Send_ContainerInfo();
            WriteLogF( _FUNC_, " - Critter '%s' self pick.\n", cl->GetInfo() );
            return;
        }

        // Check NoSteal flag
        if( is_steal && cr->GetIsNoSteal() )
        {
            cl->Send_ContainerInfo();
            WriteLogF( _FUNC_, " - Critter has NoSteal flag, critter '%s'.\n", cl->GetInfo() );
            return;
        }

        // Check dist
        if( !is_far && !CheckDist( cl->GetHexX(), cl->GetHexY(), cr->GetHexX(), cr->GetHexY(), cl->GetUseDist() + cr->GetMultihex() ) )
        {
            cl->Send_XY( cl );
            cl->Send_XY( cr );
            cl->Send_ContainerInfo();
            WriteLogF( _FUNC_, " - Transfer critter container. Client '%s' distance more than allowed.\n", cl->GetInfo() );
            return;
        }

        // Check loot valid
        if( is_loot && !cr->IsDead() )
        {
            cl->Send_ContainerInfo();
            WriteLogF( _FUNC_, " - TRANSFER_CRIT_LOOT - Critter not dead.\n" );
            return;
        }

        // Check steal valid
        if( is_steal )
        {
            // Player is offline
            /*if(cr->IsPlayer() && cr->State!=STATE_PLAYING)
               {
                    cr->SendA_Text(&cr->VisCr,"Please, don't kill me",SAY_WHISP_ON_HEAD);
                    cl->Send_ContainerInfo();
                    WriteLog("Process_ContainerItem - TRANSFER_CRIT_STEAL - Critter not in game\n");
                    return;
               }*/

            // Npc in battle
            if( cr->IsNpc() && ( (Npc*) cr )->IsCurPlane( AI_PLANE_ATTACK ) )
            {
                cl->Send_ContainerInfo();
                return;
            }
        }

        // Process
        switch( take_flags )
        {
        case CONT_GET:
        {
            // Get item
            Item* item = cr->GetInvItem( item_id, transfer_type );
            if( !item )
            {
                cl->Send_ContainerInfo();
                cl->Send_TextMsg( cl, STR_ITEM_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
                return;
            }

            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 0, item );

            // Check count
            if( !item_count || item->GetCount() < item_count )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Incorrect count.", SAY_NETMSG );
                return;
            }

            // Check weight
            if( cl->GetFreeWeight() < (int) ( item->GetWeight() * item_count ) )
            {
                cl->Send_TextMsg( cl, STR_OVERWEIGHT, SAY_NETMSG, TEXTMSG_GAME );
                break;
            }

            // Check volume
            if( cl->GetFreeVolume() < (int) ( item->GetVolume() * item_count ) )
            {
                cl->Send_TextMsg( cl, STR_OVERVOLUME, SAY_NETMSG, TEXTMSG_GAME );
                break;
            }

            // Process steal
            if( transfer_type == TRANSFER_CRIT_STEAL )
            {
                if( !cr->EventStealing( cl, item, item_count ) )
                {
                    cr->Send_TextMsg( cl, STR_SKILL_STEAL_TRIED_GET, SAY_NETMSG, TEXTMSG_GAME );
                    cl->Send_ContainerInfo();
                    return;
                }
            }

            // Script events
            if( item->EventSkill( cl, SKILL_TAKE_CONT ) )
                return;
            if( cl->EventUseSkill( SKILL_TAKE_CONT, nullptr, item, nullptr ) )
                return;
            if( Script::PrepareContext( ServerFunctions.CritterUseSkill, _FUNC_, cl->GetInfo() ) )
            {
                Script::SetArgEntity( cl );
                Script::SetArgUInt( SKILL_TAKE_CONT );
                Script::SetArgEntity( nullptr );
                Script::SetArgEntity( item );
                Script::SetArgEntity( nullptr );
                if( Script::RunPrepared() && Script::GetReturnedBool() )
                    return;
            }

            // Transfer
            if( !ItemMngr.MoveItemCritters( cr, cl, item->GetId(), item_count ) )
                WriteLogF( _FUNC_, " - Transfer item, from player to player (get), fail.\n" );
        }
        break;
        case CONT_GETALL:
        {
            // Check for steal
            if( is_steal )
            {
                cl->Send_ContainerInfo();
                return;
            }

            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 1, nullptr );

            // Get items
            ItemVec items;
            cr->GetInvItems( items, transfer_type, true );
            if( items.empty() )
                return;

            // Check weight, volume
            uint weight = 0, volume = 0;
            for( uint i = 0, j = (uint) items.size(); i < j; ++i )
            {
                weight += items[ i ]->GetWholeWeight();
                volume += items[ i ]->GetWholeVolume();
            }

            if( cl->GetFreeWeight() < (int) weight )
            {
                cl->Send_TextMsg( cl, STR_OVERWEIGHT, SAY_NETMSG, TEXTMSG_GAME );
                break;
            }

            if( cl->GetFreeVolume() < (int) volume )
            {
                cl->Send_TextMsg( cl, STR_OVERVOLUME, SAY_NETMSG, TEXTMSG_GAME );
                break;
            }

            // Script events
            if( cl->EventUseSkill( SKILL_TAKE_ALL_CONT, cr, nullptr, nullptr ) )
                return;
            if( cr->EventUseSkillOnMe( cl, SKILL_TAKE_ALL_CONT ) )
                return;
            if( Script::PrepareContext( ServerFunctions.CritterUseSkill, _FUNC_, cl->GetInfo() ) )
            {
                Script::SetArgEntity( cl );
                Script::SetArgUInt( SKILL_TAKE_ALL_CONT );
                Script::SetArgEntity( cr );
                Script::SetArgEntity( nullptr );
                Script::SetArgEntity( nullptr );
                if( Script::RunPrepared() && Script::GetReturnedBool() )
                    return;
            }

            // Transfer
            for( uint i = 0, j = (uint) items.size(); i < j; ++i )
            {
                if( !items[ i ]->EventSkill( cl, SKILL_TAKE_CONT ) )
                {
                    cr->EraseItem( items[ i ], true );
                    cl->AddItem( items[ i ], true );
                }
            }
        }
        break;
        case CONT_PUT:
        {
            // Get item
            Item* item = cl->GetItem( item_id, true );
            if( !item || item->GetCritSlot() != SLOT_INV )
            {
                cl->Send_ContainerInfo();
                cl->Send_TextMsg( cl, STR_ITEM_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
                return;
            }

            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 2, item );

            // Check count
            if( !item_count || item->GetCount() < item_count )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Incorrect count.", SAY_NETMSG );
                return;
            }

            // Check slot
            if( item->GetCritSlot() != SLOT_INV )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Cheat detected.", SAY_NETMSG );
                WriteLogF( _FUNC_, " - Attempting to put in a container not from the inventory2, client '%s'.", cl->GetInfo() );
                return;
            }

            // Check weight, volume
            if( cr->GetFreeWeight() < (int) ( item->GetWeight() * item_count ) )
            {
                cl->Send_TextMsg( cl, STR_OVERWEIGHT, SAY_NETMSG, TEXTMSG_GAME );
                break;
            }
            if( cr->GetFreeVolume() < (int) ( item->GetVolume() * item_count ) )
            {
                cl->Send_TextMsg( cl, STR_OVERVOLUME, SAY_NETMSG, TEXTMSG_GAME );
                break;
            }

            // Steal process
            if( transfer_type == TRANSFER_CRIT_STEAL )
            {
                if( !cr->EventStealing( cl, item, item_count ) )
                {
                    cr->Send_TextMsg( cl, STR_SKILL_STEAL_TRIED_PUT, SAY_NETMSG, TEXTMSG_GAME );
                    cl->Send_ContainerInfo();
                    return;
                }
            }

            // Script events
            if( item->EventSkill( cl, SKILL_PUT_CONT ) )
                return;
            if( cl->EventUseSkill( SKILL_PUT_CONT, nullptr, item, nullptr ) )
                return;
            if( Script::PrepareContext( ServerFunctions.CritterUseSkill, _FUNC_, cl->GetInfo() ) )
            {
                Script::SetArgEntity( cl );
                Script::SetArgUInt( SKILL_PUT_CONT );
                Script::SetArgEntity( nullptr );
                Script::SetArgEntity( item );
                Script::SetArgEntity( nullptr );
                if( Script::RunPrepared() && Script::GetReturnedBool() )
                    return;
            }

            // Transfer
            if( !ItemMngr.MoveItemCritters( cl, cr, item->GetId(), item_count ) )
                WriteLogF( _FUNC_, " - transfer item, from player to player (put), fail.\n" );
        }
        break;
        case CONT_PUTALL:
        {
            cl->Send_ContainerInfo();
        }
        break;
        default:
            break;
        }

        cl->Send_ContainerInfo( cr, transfer_type, false );
    }
}

void FOServer::Process_UseSkill( Client* cl )
{
    int   skill;
    uchar targ_type;
    uint  target_id;
    hash  target_pid;

    cl->Bin >> skill;
    cl->Bin >> targ_type;
    cl->Bin >> target_id;
    cl->Bin >> target_pid;
    CHECK_IN_BUFF_ERROR( cl );

    if( targ_type > TARGET_SCENERY )
    {
        WriteLogF( _FUNC_, " - Invalid target type %u, client '%s'.\n", targ_type, cl->GetInfo() );
        return;
    }

    Act_Use( cl, 0, skill, targ_type, target_id, target_pid, 0 );
}

void FOServer::Process_Dir( Client* cl )
{
    uchar dir;
    cl->Bin >> dir;
    CHECK_IN_BUFF_ERROR( cl );

    if( !cl->GetMapId() || dir >= DIRS_COUNT || cl->GetDir() == dir || cl->IsTalking() || !cl->CheckMyTurn( nullptr ) )
    {
        if( cl->GetDir() != dir )
            cl->Send_Dir( cl );
        return;
    }

    cl->SetDir( dir );
    cl->SendA_Dir();
}

void FOServer::Process_SetUserHoloStr( Client* cl )
{
    uint   msg_len;
    uint   holodisk_id;
    ushort title_len;
    ushort text_len;
    char   title[ USER_HOLO_MAX_TITLE_LEN + 1 ];
    char   text[ USER_HOLO_MAX_LEN + 1 ];
    cl->Bin >> msg_len;
    cl->Bin >> holodisk_id;
    cl->Bin >> title_len;
    cl->Bin >> text_len;
    if( !title_len || !text_len || title_len > USER_HOLO_MAX_TITLE_LEN || text_len > USER_HOLO_MAX_LEN )
    {
        WriteLogF( _FUNC_, " - Length of texts is greater of maximum or zero, title cur %u, title max %u, text cur %u, text max %u, client '%s'. Disconnect.\n", title_len, USER_HOLO_MAX_TITLE_LEN, text_len, USER_HOLO_MAX_LEN, cl->GetInfo() );
        cl->Disconnect();
        return;
    }
    cl->Bin.Pop( title, title_len );
    title[ title_len ] = '\0';
    cl->Bin.Pop( text, text_len );
    text[ text_len ] = '\0';
    CHECK_IN_BUFF_ERROR( cl );

    cl->SetBreakTime( GameOpt.Breaktime );

    Item* holodisk = cl->GetItem( holodisk_id, true );
    if( !holodisk )
    {
        WriteLogF( _FUNC_, " - Holodisk %u not found, client '%s'.\n", holodisk_id, cl->GetInfo() );
        cl->Send_TextMsg( cl, STR_HOLO_WRITE_FAIL, SAY_NETMSG, TEXTMSG_HOLO );
        return;
    }

    cl->SendAA_Action( ACTION_USE_ITEM, 0, holodisk );

    #pragma MESSAGE("Check valid of received text.")
//	int invalid_chars=CheckStr(text);
//	if(invalid_chars>0) WriteLogF(_FUNC_," - Found invalid chars, count %u, client '%s', changed on '_'.\n",invalid_chars,cl->GetInfo());

    HolodiskLocker.Lock();

    uint      holo_id = holodisk->GetHolodiskNum();
    HoloInfo* hi = GetHoloInfo( holo_id );
    if( hi && hi->CanRewrite )
    {
        hi->Title = title;
        hi->Text = text;
    }
    else
    {
        holo_id = ++LastHoloId;
        HolodiskInfo.insert( PAIR( holo_id, new HoloInfo( true, title, text ) ) );
    }

    HolodiskLocker.Unlock();

    cl->Send_UserHoloStr( STR_HOLO_INFO_NAME( holo_id ), title, title_len );
    cl->Send_UserHoloStr( STR_HOLO_INFO_DESC( holo_id ), text, text_len );
    holodisk->SetHolodiskNum( holo_id );
    cl->Send_TextMsg( cl, STR_HOLO_WRITE_SUCC, SAY_NETMSG, TEXTMSG_HOLO );
}

#pragma MESSAGE("Check aviability of requested holodisk.")
void FOServer::Process_GetUserHoloStr( Client* cl )
{
    uint str_num;
    cl->Bin >> str_num;

    if( str_num / 10 < USER_HOLO_START_NUM )
    {
        WriteLogF( _FUNC_, " - String value is less than users holo numbers, str num %u, client '%s'.\n", str_num, cl->GetInfo() );
        return;
    }

    Send_PlayerHoloInfo( cl, str_num / 10, ( str_num % 10 ) != 0 );
}

void FOServer::Process_LevelUp( Client* cl )
{
    uint   msg_len;
    ushort count_skill_up;
    IntVec skills;
    int    perk_up;

    cl->Bin >> msg_len;

    // Skills up
    cl->Bin >> count_skill_up;
    for( int i = 0; i < count_skill_up; i++ )
    {
        int num, val;
        cl->Bin >> num;
        cl->Bin >> val;
        skills.push_back( num );
        skills.push_back( val );
    }

    // Perk up
    cl->Bin >> perk_up;

    CHECK_IN_BUFF_ERROR( cl );

    for( int i = 0; i < count_skill_up; i++ )
    {
        if( Script::PrepareContext( ServerFunctions.PlayerLevelUp, _FUNC_, cl->GetInfo() ) )
        {
            Script::SetArgEntity( cl );
            Script::SetArgUInt( skills[ i * 2 ] );
            Script::SetArgUInt( skills[ i * 2 + 1 ] );
            Script::SetArgUInt( 0 );
            Script::RunPrepared();
        }
    }

    if( Script::PrepareContext( ServerFunctions.PlayerLevelUp, _FUNC_, cl->GetInfo() ) )
    {
        Script::SetArgEntity( cl );
        Script::SetArgUInt( 0 );
        Script::SetArgUInt( 0 );
        Script::SetArgUInt( perk_up );
        Script::RunPrepared();
    }
}

void FOServer::Process_CraftAsk( Client* cl )
{
    uint tick = Timer::FastTick();
    if( tick < cl->LastSendCraftTick + CRAFT_SEND_TIME )
    {
        WriteLogF( _FUNC_, " - Client '%s' ignore send craft timeout.\n", cl->GetInfo() );
        return;
    }
    cl->LastSendCraftTick = tick;

    uint   msg_len;
    ushort count;
    cl->Bin >> msg_len;
    cl->Bin >> count;

    UIntVec numbers;
    numbers.reserve( count );
    for( int i = 0; i < count; i++ )
    {
        uint craft_num;
        cl->Bin >> craft_num;
        if( MrFixit.IsShowCraft( cl, craft_num ) )
            numbers.push_back( craft_num );
    }
    CHECK_IN_BUFF_ERROR( cl );

    uint msg = NETMSG_CRAFT_ASK;
    count = (ushort) numbers.size();
    msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( count ) + sizeof( uint ) * count;

    BOUT_BEGIN( cl );
    cl->Bout << msg;
    cl->Bout << msg_len;
    cl->Bout << count;
    for( uint i = 0, j = (uint) numbers.size(); i < j; i++ )
        cl->Bout << numbers[ i ];
    BOUT_END( cl );
}

void FOServer::Process_Craft( Client* cl )
{
    uint craft_num;

    cl->Bin >> craft_num;
    CHECK_IN_BUFF_ERROR( cl );

    uchar res = MrFixit.ProcessCraft( cl, craft_num );

    BOUT_BEGIN( cl );
    cl->Bout << NETMSG_CRAFT_RESULT;
    cl->Bout << res;
    BOUT_END( cl );
}

void FOServer::Process_Ping( Client* cl )
{
    uchar ping;

    cl->Bin >> ping;
    CHECK_IN_BUFF_ERROR( cl );

    if( ping == PING_CLIENT )
    {
        cl->PingOk( PING_CLIENT_LIFE_TIME );
        return;
    }
    else if( ping == PING_UID_FAIL )
    {
        #ifndef DEV_VERSION
        SCOPE_LOCK( ClientsDataLocker );

        ClientData* data = GetClientData( cl->GetId() );
        if( data )
        {
            WriteLogF( _FUNC_, " - Wrong UID, client '%s'. Disconnect.\n", cl->GetInfo() );
            for( int i = 0; i < 5; i++ )
                data->UID[ i ] = Random( 0, 10000 );
            data->UIDEndTick = Timer::FastTick() + GameOpt.AccountPlayTime * 1000;
        }
        cl->Disconnect();
        return;
        #endif
    }
    else if( ping == PING_PING || ping == PING_WAIT )
    {
        // Valid pings
    }
    else
    {
        WriteLog( "Unknown ping %u, client '%s'.\n", ping, cl->GetInfo() );
        return;
    }

    BOUT_BEGIN( cl );
    cl->Bout << NETMSG_PING;
    cl->Bout << ping;
    BOUT_END( cl );
}

void FOServer::Process_PlayersBarter( Client* cl )
{
    uchar barter;
    uint  param;
    uint  param_ext;

    cl->Bin >> barter;
    cl->Bin >> param;
    cl->Bin >> param_ext;
    CHECK_IN_BUFF_ERROR( cl );

    if( barter == BARTER_TRY || barter == BARTER_ACCEPTED )
    {
        if( !param )
            return;
        Client* opponent = cl->BarterGetOpponent( param );
        if( !opponent )
        {
            cl->Send_TextMsg( cl, STR_BARTER_BEGIN_FAIL, SAY_NETMSG, TEXTMSG_GAME );
            return;
        }

        cl->BarterOffer = false;
        cl->BarterHide = ( param_ext != 0 );
        cl->BarterItems.clear();

        if( barter == BARTER_TRY )
        {
            opponent->Send_PlayersBarter( BARTER_TRY, cl->GetId(), param_ext );
        }
        else if( opponent->BarterOpponent == cl->GetId() )     // Accepted
        {
            param_ext = ( cl->BarterHide ? 1 : 0 ) | ( opponent->BarterHide ? 2 : 0 );
            cl->Send_PlayersBarter( BARTER_BEGIN, opponent->GetId(), param_ext );
            if( !opponent->BarterHide )
                cl->Send_ContainerInfo( opponent, TRANSFER_CRIT_BARTER, false );
            param_ext = ( opponent->BarterHide ? 1 : 0 ) | ( cl->BarterHide ? 2 : 0 );
            opponent->Send_PlayersBarter( BARTER_BEGIN, cl->GetId(), param_ext );
            if( !cl->BarterHide )
                opponent->Send_ContainerInfo( cl, TRANSFER_CRIT_BARTER, false );
        }
        return;
    }

    Client* opponent = cl->BarterGetOpponent( cl->BarterOpponent );
    if( !opponent )
    {
        cl->Send_TextMsg( cl, STR_BARTER_BEGIN_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        return;
    }

    if( barter == BARTER_REFRESH )
    {
        cl->BarterRefresh( opponent );
    }
    else if( barter == BARTER_END )
    {
        cl->BarterEnd();
        opponent->BarterEnd();
    }
    else if( barter == BARTER_OFFER )
    {
        cl->BarterOffer = ( param != 0 );
        opponent->Send_PlayersBarter( BARTER_OFFER, param, true );

        // Deal, try transfer items
        if( cl->BarterOffer && opponent->BarterOffer )
        {
            bool is_succ = false;
            cl->BarterOffer = false;
            opponent->BarterOffer = false;

            // Check for weight
            // Player
            uint weigth = 0;
            for( uint i = 0; i < cl->BarterItems.size(); i++ )
            {
                Client::BarterItem& barter_item = cl->BarterItems[ i ];
                Item*               item = cl->GetItem( barter_item.Id, true );
                weigth += item->GetWeight() * barter_item.Count;
            }
            // Opponent
            uint weigth_ = 0;
            for( uint i = 0; i < opponent->BarterItems.size(); i++ )
            {
                Client::BarterItem& barter_item = opponent->BarterItems[ i ];
                Item*               item = opponent->GetItem( barter_item.Id, true );
                weigth_ += item->GetWeight() * barter_item.Count;
            }
            // Check
            if( cl->GetFreeWeight() + (int) weigth < (int) weigth_ )
            {
                cl->Send_TextMsg( cl, STR_BARTER_OVERWEIGHT, SAY_NETMSG, TEXTMSG_GAME );
                goto label_EndOffer;
            }
            if( opponent->GetFreeWeight() + (int) weigth_ < (int) weigth )
            {
                opponent->Send_TextMsg( opponent, STR_BARTER_OVERWEIGHT, SAY_NETMSG, TEXTMSG_GAME );
                goto label_EndOffer;
            }

            // Transfer
            // Player
            for( uint i = 0; i < cl->BarterItems.size(); i++ )
            {
                Client::BarterItem& bitem = cl->BarterItems[ i ];
                if( !ItemMngr.MoveItemCritters( cl, opponent, bitem.Id, bitem.Count ) )
                    WriteLogF( _FUNC_, " - transfer item, from player to player_, fail.\n" );
            }
            // Player_
            for( uint i = 0; i < opponent->BarterItems.size(); i++ )
            {
                Client::BarterItem& bitem = opponent->BarterItems[ i ];
                if( !ItemMngr.MoveItemCritters( opponent, cl, bitem.Id, bitem.Count ) )
                    WriteLogF( _FUNC_, " - transfer item, from player_ to player, fail.\n" );
            }

            is_succ = true;
            cl->BarterItems.clear();
            opponent->BarterItems.clear();

label_EndOffer:
            if( is_succ )
            {
                cl->SetBreakTime( GameOpt.Breaktime );
                cl->BarterRefresh( opponent );
                opponent->SetBreakTime( GameOpt.Breaktime );
                opponent->BarterRefresh( cl );
                if( cl->GetMapId() )
                {
                    cl->Send_Action( cl, ACTION_BARTER, 0, nullptr );
                    cl->SendAA_Action( ACTION_BARTER, 0, nullptr );
                    opponent->Send_Action( opponent, ACTION_BARTER, 0, nullptr );
                    opponent->SendAA_Action( ACTION_BARTER, 0, nullptr );
                }
            }
            else
            {
                cl->Send_PlayersBarter( BARTER_OFFER, false, false );
                cl->Send_PlayersBarter( BARTER_OFFER, false, true );
                opponent->Send_PlayersBarter( BARTER_OFFER, false, false );
                opponent->Send_PlayersBarter( BARTER_OFFER, false, true );
            }
        }
    }
    else
    {
        if( !param || !param_ext )
            return;

        bool    is_set = ( barter == BARTER_SET_SELF || barter == BARTER_SET_OPPONENT );
        Client* barter_cl;
        if( barter == BARTER_SET_SELF || barter == BARTER_UNSET_SELF )
            barter_cl = cl;
        else if( barter == BARTER_SET_OPPONENT || barter == BARTER_UNSET_OPPONENT )
            barter_cl = opponent;
        else
            return;

        if( barter_cl == opponent && opponent->BarterHide )
        {
            cl->Send_Text( cl, "Cheat fail.", SAY_NETMSG );
            WriteLogF( _FUNC_, " - Player try operate opponent inventory in hide mode, player '%s', opponent '%s'.\n", cl->GetInfo(), opponent->GetInfo() );
            return;
        }

        Item*               item = barter_cl->GetItem( param, true );
        Client::BarterItem* barter_item = barter_cl->BarterGetItem( param );

        if( !item || ( barter_item && barter_item->Count > item->GetCount() ) )
        {
            barter_cl->BarterEraseItem( param );
            cl->BarterRefresh( opponent );
            opponent->BarterRefresh( cl );
            return;
        }

        if( is_set )
        {
            if( param_ext > item->GetCount() - ( barter_item ? barter_item->Count : 0 ) )
            {
                barter_cl->BarterEraseItem( param );
                cl->BarterRefresh( opponent );
                opponent->BarterRefresh( cl );
                return;
            }
            if( !barter_item )
                barter_cl->BarterItems.push_back( Client::BarterItem( item->GetId(), param_ext ) );
            else
                barter_item->Count += param_ext;
        }
        else
        {
            if( !barter_item || param_ext > barter_item->Count )
            {
                barter_cl->BarterEraseItem( param );
                cl->BarterRefresh( opponent );
                opponent->BarterRefresh( cl );
                return;
            }
            barter_item->Count -= param_ext;
            if( !barter_item->Count )
                barter_cl->BarterEraseItem( param );
        }

        cl->BarterOffer = false;
        opponent->BarterOffer = false;
        if( barter == BARTER_SET_SELF )
            barter = BARTER_SET_OPPONENT;
        else if( barter == BARTER_SET_OPPONENT )
            barter = BARTER_SET_SELF;
        else if( barter == BARTER_UNSET_SELF )
            barter = BARTER_UNSET_OPPONENT;
        else if( barter == BARTER_UNSET_OPPONENT )
            barter = BARTER_UNSET_SELF;

        if( barter == BARTER_SET_OPPONENT && cl->BarterHide )
            opponent->Send_PlayersBarterSetHide( item, param_ext );
        else
            opponent->Send_PlayersBarter( barter, param, param_ext );
    }
}

void FOServer::Process_ScreenAnswer( Client* cl )
{
    uint answer_i;
    char answer_s[ MAX_SAY_NPC_TEXT + 1 ];
    cl->Bin >> answer_i;
    cl->Bin.Pop( answer_s, MAX_SAY_NPC_TEXT );
    answer_s[ MAX_SAY_NPC_TEXT ] = 0;

    if( !cl->ScreenCallbackBindId )
    {
        WriteLogF( _FUNC_, " - Client '%s' answered on not not specified screen.\n", cl->GetInfo() );
        return;
    }

    uint bind_id = cl->ScreenCallbackBindId;
    cl->ScreenCallbackBindId = 0;

    if( !Script::PrepareContext( bind_id, _FUNC_, cl->GetInfo() ) )
        return;
    Script::SetArgEntity( cl );
    Script::SetArgUInt( answer_i );
    ScriptString* lexems = ScriptString::Create( answer_s );
    Script::SetArgObject( lexems );
    Script::RunPrepared();
    lexems->Release();
}

void FOServer::Process_Combat( Client* cl )
{
    uchar type;
    int   val;
    cl->Bin >> type;
    cl->Bin >> val;
    CHECK_IN_BUFF_ERROR( cl );

    if( type == COMBAT_TB_END_TURN )
    {
        Map* map = MapMngr.GetMap( cl->GetMapId() );
        if( !map )
        {
            WriteLogF( _FUNC_, " - Map not found on end turn, client '%s'.\n", cl->GetInfo() );
            return;
        }
        if( map->IsTurnBasedOn && map->IsCritterTurn( cl ) )
            map->EndCritterTurn();
    }
    else if( type == COMBAT_TB_END_COMBAT )
    {
        Map* map = MapMngr.GetMap( cl->GetMapId() );
        if( !map )
        {
            WriteLogF( _FUNC_, " - Map not found on end combat, client '%s'.\n", cl->GetInfo() );
            return;
        }
        if( map->IsTurnBasedOn )
            cl->SetIsEndCombat( val != 0 );
    }
    else
    {
        WriteLogF( _FUNC_, " - Unknown type %u, value %d, client '%s'.\n", type, val, cl->GetInfo() );
    }
}

void FOServer::Process_RunServerScript( Client* cl )
{
    uint          msg_len;
    bool          unsafe = false;
    ushort        script_name_len;
    char          script_name[ MAX_FOTEXT ];
    int           p0, p1, p2;
    ushort        p3len;
    char          p3str[ MAX_FOTEXT ];
    ScriptString* p3 = nullptr;
    ushort        p4size;
    ScriptArray*  p4 = nullptr;

    cl->Bin >> msg_len;
    cl->Bin >> unsafe;
    if( !unsafe && !( cl->Access == ACCESS_MODER || cl->Access == ACCESS_ADMIN ) )
    {
        WriteLogF( _FUNC_, " - Attempt to execute script without privilege. Client '%s'.\n", cl->GetInfo() );
        cl->Send_Text( cl, "Access denied. Disconnect.", SAY_NETMSG );
        cl->Disconnect();
        return;
    }

    cl->Bin >> script_name_len;
    if( script_name_len && script_name_len < MAX_FOTEXT )
    {
        cl->Bin.Pop( script_name, script_name_len );
        script_name[ script_name_len ] = 0;
    }

    char module_name[ MAX_FOTEXT ];
    char func_name[ MAX_FOTEXT ];
    if( !Script::ParseScriptName( script_name, module_name, func_name ) )
    {
        WriteLogF( _FUNC_, " - Attempt to execute invalid script '%s'. Client '%s'.\n", script_name, cl->GetInfo() );
        cl->Send_Text( cl, "Access denied. Disconnect.", SAY_NETMSG );
        cl->Disconnect();
        return;
    }

    if( unsafe && ( Str::Length( func_name ) <= 7 || !Str::CompareCount( func_name, "unsafe_", 7 ) ) ) // Check unsafe_ prefix
    {
        WriteLogF( _FUNC_, " - Attempt to execute '%s' script without 'unsafe_' prefix. Client '%s'.\n", func_name, cl->GetInfo() );
        cl->Send_Text( cl, "Access denied. Disconnect.", SAY_NETMSG );
        cl->Disconnect();
        return;
    }

    cl->Bin >> p0;
    cl->Bin >> p1;
    cl->Bin >> p2;
    cl->Bin >> p3len;
    if( p3len && p3len < MAX_FOTEXT )
    {
        cl->Bin.Pop( p3str, p3len );
        p3str[ p3len ] = 0;
        p3 = ScriptString::Create( p3str );
    }
    cl->Bin >> p4size;
    if( p4size )
    {
        p4 = Script::CreateArray( "int[]" );
        if( p4 )
        {
            p4->Resize( p4size );
            cl->Bin.Pop( (char*) p4->At( 0 ), p4size * sizeof( uint ) );
        }
    }

    CHECK_IN_BUFF_ERROR( cl );

    uint bind_id = Script::BindByModuleFuncName( module_name, func_name, "void %s(Critter&,int,int,int,string@,int[]@)", true );
    if( bind_id && Script::PrepareContext( bind_id, _FUNC_, cl->GetInfo() ) )
    {
        Script::SetArgEntity( cl );
        Script::SetArgUInt( p0 );
        Script::SetArgUInt( p1 );
        Script::SetArgUInt( p2 );
        Script::SetArgObject( p3 );
        Script::SetArgObject( p4 );
        Script::RunPrepared();
    }

    if( p3 )
        p3->Release();
    if( p4 )
        p4->Release();
}

void FOServer::Process_KarmaVoting( Client* cl )
{
    uint crid;
    bool is_up;
    cl->Bin >> crid;
    cl->Bin >> is_up;

    if( cl->GetId() == crid )
        return;
    if( cl->GetKarmaVoting() )
        return;

    Critter* cr = CrMngr.GetCritter( crid, true );
    if( !cr )
    {
        cl->SetTimeoutKarmaVoting( GameOpt.FullSecond + 1 * GameOpt.TimeMultiplier );    // Wait 1 second
        return;
    }

    if( Script::PrepareContext( ServerFunctions.KarmaVoting, _FUNC_, cl->GetInfo() ) )
    {
        Script::SetArgEntity( cl );
        Script::SetArgEntity( cr );
        Script::SetArgBool( is_up );
        Script::RunPrepared();
    }
}

void FOServer::Process_RuleGlobal( Client* cl )
{
    uchar command;
    uint  param1;
    uint  param2;
    cl->Bin >> command;
    cl->Bin >> param1;
    cl->Bin >> param2;

    switch( command )
    {
    case GM_CMD_FOLLOW_CRIT:
    {
        if( !param1 )
            break;

        Critter* cr = cl->GetCritSelf( param1, false );
        if( !cr )
            break;

        if( cl->GetFollowCrit() == cr->GetId() )
            cl->SetFollowCrit( 0 );
        else
            cl->SetFollowCrit( cr->GetId() );
    }
    break;
    case GM_CMD_SETMOVE:
        if( cl->GetMapId() || !cl->GroupMove || cl != cl->GroupMove->Rule )
            break;
        if( param1 >= GM_MAXX || param2 >= GM_MAXY )
            break;
        if( cl->GroupMove->EncounterDescriptor )
            break;
        cl->GroupMove->ToX = (float) param1;
        cl->GroupMove->ToY = (float) param2;
        MapMngr.GM_GlobalProcess( cl, cl->GroupMove, GLOBAL_PROCESS_SET_MOVE );
        break;
    case GM_CMD_STOP:
        break;
    case GM_CMD_TOLOCAL:
        if( cl->GetMapId() || !cl->GroupMove || cl != cl->GroupMove->Rule )
            break;
        if( cl->GroupMove->EncounterDescriptor )
            break;
        if( IS_TIMEOUT( cl->GetTimeoutTransfer() ) )
        {
            cl->Send_TextMsg( cl, STR_TIMEOUT_TRANSFER_WAIT, SAY_NETMSG, TEXTMSG_GAME );
            break;
        }

        if( !param1 )
        {
            if( cl->GetMapId() || !cl->GroupMove || cl != cl->GroupMove->Rule )
                break;
            cl->GroupMove->EncounterDescriptor = 0;
            MapMngr.GM_GlobalProcess( cl, cl->GroupMove, GLOBAL_PROCESS_ENTER );
        }
        else
        {
            MapMngr.GM_GroupToLoc( cl, param1, param2 );
        }
        return;
    case GM_CMD_ANSWER:
        if( cl->GetMapId() || !cl->GroupMove || cl != cl->GroupMove->Rule )
            break;
        if( !cl->GroupMove->EncounterDescriptor || cl->GroupMove->EncounterForce )
            break;

        if( (int) param1 >= 0 )    // Yes
        {
            MapMngr.GM_GlobalInvite( cl->GroupMove, param1 );
            return;
        }
        else if( cl->GroupMove->EncounterDescriptor )       // No
        {
            cl->GroupMove->EncounterDescriptor = 0;
            cl->SendA_GlobalInfo( cl->GroupMove, GM_INFO_GROUP_PARAM );
        }
        break;
    case GM_CMD_FOLLOW:
    {
        // Find rule
        Critter* rule = CrMngr.GetCritter( param1, true );
        if( !rule || rule->GetMapId() || !rule->GroupMove || rule != rule->GroupMove->Rule )
            break;

        // Check for follow
        if( !rule->GroupMove->CheckForFollow( cl ) )
            break;
        if( !CheckDist( rule->GetLastMapHexX(), rule->GetLastMapHexY(), cl->GetHexX(), cl->GetHexY(), FOLLOW_DIST + rule->GetMultihex() + cl->GetMultihex() ) )
            break;

        // Transit
        if( cl->LockMapTransfers )
        {
            WriteLogF( _FUNC_, " - Transfers locked, critter '%s'.\n", cl->GetInfo() );
            return;
        }
        if( !MapMngr.TransitToGlobal( cl, param1, 0, false ) )
            break;
    }
        return;
    case GM_CMD_KICKCRIT:
    {
        if( cl->GetMapId() || !cl->GroupMove || cl->GroupMove->GetSize() < 2 || cl->GroupMove->EncounterDescriptor )
            break;

        GlobalMapGroup* group = cl->GroupMove;
        Critter*        kick_cr;

        if( cl->GetId() == param1 )             // Kick self
        {
            if( cl == group->Rule )
                break;
            kick_cr = cl;
        }
        else                 // Kick other
        {
            if( cl != group->Rule )
                break;
            kick_cr = group->GetCritter( param1 );
            if( !kick_cr )
                break;
        }

        MapMngr.GM_GlobalProcess( kick_cr, cl->GroupMove, GLOBAL_PROCESS_KICK );
    }
    break;
    case GM_CMD_GIVE_RULE:
    {
        // Check
        if( cl->GetId() == param1 || cl->GetMapId() || !cl->GroupMove || cl != cl->GroupMove->Rule || cl->GroupMove->EncounterDescriptor )
            break;
        Critter* new_rule = cl->GroupMove->GetCritter( param1 );
        if( !new_rule || !new_rule->IsPlayer() || !( (Client*) new_rule )->IsOnline() )
            break;

        MapMngr.GM_GiveRule( cl, new_rule );
        MapMngr.GM_StopGroup( cl );
    }
    break;
    case GM_CMD_ENTRANCES:
    {
        if( cl->GetMapId() || !cl->GroupMove || cl->GroupMove->EncounterDescriptor )
            break;

        uint      loc_id = param1;
        Location* loc = MapMngr.GetLocation( loc_id );
        if( !loc || DistSqrt( (int) cl->GroupMove->CurX, (int) cl->GroupMove->CurY, loc->GetWorldX(), loc->GetWorldY() ) > loc->GetRadius() )
            break;

        uint tick = Timer::FastTick();
        if( cl->LastSendEntrancesLocId == loc_id && tick < cl->LastSendEntrancesTick + GM_ENTRANCES_SEND_TIME )
        {
            WriteLogF( _FUNC_, " - Client '%s' ignore send entrances timeout.\n", cl->GetInfo() );
            break;
        }
        cl->LastSendEntrancesLocId = loc_id;
        cl->LastSendEntrancesTick = tick;

        if( loc->EntranceScriptBindId )
        {
            uchar        count = 0;
            uchar        show[ 0x100 ];
            ScriptArray* arr = MapMngr.GM_CreateGroupArray( cl->GroupMove );
            if( !arr )
                break;
            ScriptArray* map_entrances = loc->GetMapEntrances();
            uchar        map_entrances_size = (uchar) ( map_entrances->GetSize() / 2 );
            map_entrances->Release();
            for( uchar i = 0; i < map_entrances_size; i++ )
            {
                if( MapMngr.GM_CheckEntrance( loc, arr, i ) )
                {
                    show[ count ] = i;
                    count++;
                }
            }
            arr->Release();

            uint msg = NETMSG_GLOBAL_ENTRANCES;
            uint msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( loc_id ) + sizeof( count ) + sizeof( uchar ) * count;

            BOUT_BEGIN( cl );
            cl->Bout << msg;
            cl->Bout << msg_len;
            cl->Bout << loc_id;
            cl->Bout << count;
            for( uchar i = 0; i < count; i++ )
                cl->Bout << show[ i ];
            BOUT_END( cl );
        }
        else
        {
            uint         msg = NETMSG_GLOBAL_ENTRANCES;
            ScriptArray* map_entrances = loc->GetMapEntrances();
            uchar        count = (uchar) ( map_entrances->GetSize() / 2 );
            map_entrances->Release();
            uint         msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( loc_id ) + sizeof( count ) + sizeof( uchar ) * count;

            BOUT_BEGIN( cl );
            cl->Bout << msg;
            cl->Bout << msg_len;
            cl->Bout << loc_id;
            cl->Bout << count;
            for( uchar i = 0; i < count; i++ )
                cl->Bout << i;
            BOUT_END( cl );
        }
    }
    break;
    case GM_CMD_VIEW_MAP:
    {
        if( cl->GetMapId() || !cl->GroupMove || cl->GroupMove->EncounterDescriptor )
            break;

        uint      loc_id = param1;
        Location* loc = MapMngr.GetLocation( loc_id );
        if( !loc || DistSqrt( (int) cl->GroupMove->CurX, (int) cl->GroupMove->CurY, loc->GetWorldX(), loc->GetWorldY() ) > loc->GetRadius() )
            break;

        ScriptArray* map_entrances = loc->GetMapEntrances();
        uchar        count = (uchar) ( map_entrances->GetSize() / 2 );
        uint         entrance = param2;
        if( entrance >= count )
        {
            map_entrances->Release();
            break;
        }
        hash entrance_map = *(hash*) map_entrances->At( entrance * 2 );
        hash entrance_entire = *(hash*) map_entrances->At( entrance * 2 + 1 );
        map_entrances->Release();

        if( loc->EntranceScriptBindId )
        {
            ScriptArray* arr = MapMngr.GM_CreateGroupArray( cl->GroupMove );
            if( !arr )
                break;
            bool result = MapMngr.GM_CheckEntrance( loc, arr, entrance );
            arr->Release();
            if( !result )
                break;
        }

        Map* map = loc->GetMapByPid( entrance_map );
        if( !map )
            break;

        uchar  dir;
        ushort hx, hy;
        if( !map->GetStartCoord( hx, hy, dir, entrance_entire ) )
            break;

        cl->SetHexX( hx );
        cl->SetHexY( hy );
        cl->SetDir( dir );
        cl->ViewMapId = map->GetId();
        cl->ViewMapPid = map->GetProtoId();
        cl->ViewMapLook = cl->GetLookDistance();
        cl->ViewMapHx = hx;
        cl->ViewMapHy = hy;
        cl->ViewMapDir = dir;
        cl->ViewMapLocId = loc_id;
        cl->ViewMapLocEnt = entrance;
        cl->Send_LoadMap( map );
    }
    break;
    default:
        WriteLogF( _FUNC_, " - Unknown command %u, from client '%s'.\n", cl->GetInfo() );
        break;
    }

    cl->SetBreakTime( GameOpt.Breaktime );
}

void FOServer::Process_Property( Client* cl, uint data_size )
{
    uint              msg_len = 0;
    NetProperty::Type type = NetProperty::None;
    uint              cr_id = 0;
    uint              item_id = 0;
    ushort            property_index = 0;

    if( data_size == 0 )
        cl->Bin >> msg_len;

    char type_ = 0;
    cl->Bin >> type_;
    type = (NetProperty::Type) type_;

    uint additional_args = 0;
    switch( type )
    {
    case NetProperty::CritterItem:
        additional_args = 2;
        cl->Bin >> cr_id;
        cl->Bin >> item_id;
        break;
    case NetProperty::Critter:
        additional_args = 1;
        cl->Bin >> cr_id;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        cl->Bin >> item_id;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        cl->Bin >> item_id;
        break;
    default:
        break;
    }

    cl->Bin >> property_index;

    CHECK_IN_BUFF_ERROR( cl );

    UCharVec data;
    if( data_size != 0 )
    {
        data.resize( data_size );
        cl->Bin.Pop( (char*) &data[ 0 ], data_size );
    }
    else
    {
        uint len = msg_len - sizeof( uint ) - sizeof( msg_len ) - sizeof( char ) - additional_args * sizeof( uint ) - sizeof( ushort );
        #pragma MESSAGE( "Control max size explicitly, add option to property registration" )
        if( len > 0xFFFF )         // For now 64Kb for all
            return;
        data.resize( len );
        cl->Bin.Pop( (char*) &data[ 0 ], len );
    }

    CHECK_IN_BUFF_ERROR( cl );

    bool      is_public = false;
    Property* prop = nullptr;
    Entity*   entity = nullptr;
    switch( type )
    {
    case NetProperty::Global:
        is_public = true;
        prop = GlobalVars::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = Globals;
        break;
    case NetProperty::Critter:
        is_public = true;
        prop = Critter::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = CrMngr.GetCritter( cr_id, true );
        break;
    case NetProperty::Chosen:
        prop = Critter::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = cl;
        break;
    case NetProperty::MapItem:
        is_public = true;
        prop = Item::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = ItemMngr.GetItem( item_id, true );
        break;
    case NetProperty::CritterItem:
        is_public = true;
        prop = Item::PropertiesRegistrator->Get( property_index );
        if( prop )
        {
            Critter* cr = CrMngr.GetCritter( cr_id, true );
            if( cr )
                entity = cr->GetItem( item_id, true );
        }
        break;
    case NetProperty::ChosenItem:
        prop = Item::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = cl->GetItem( item_id, true );
        break;
    case NetProperty::Map:
        is_public = true;
        prop = Map::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = MapMngr.GetMap( cl->GetMapId(), true );
        break;
    case NetProperty::Location:
        is_public = true;
        prop = Location::PropertiesRegistrator->Get( property_index );
        if( prop )
        {
            Map* map = MapMngr.GetMap( cl->GetMapId(), false );
            if( map )
                entity = map->GetLocation( true );
        }
        break;
    default:
        break;
    }
    if( !prop || !entity )
        return;

    Property::AccessType access = prop->GetAccess();
    if( is_public && !( access & Property::PublicMask ) )
        return;
    if( !is_public && !( access & ( Property::ProtectedMask | Property::PublicMask ) ) )
        return;
    if( !( access & Property::ModifiableMask ) )
        return;
    if( is_public && access != Property::PublicFullModifiable )
        return;
    if( prop->IsPOD() && data_size != prop->GetBaseSize() )
        return;
    if( !prop->IsPOD() && data_size != 0 )
        return;

    #pragma MESSAGE( "Disable send changing field by client to this client" )
    prop->SetData( entity, !data.empty() ? &data[ 0 ] : nullptr, (uint) data.size() );
}

void FOServer::OnSendGlobalValue( Entity* entity, Property* prop )
{
    if( ( prop->GetAccess() & Property::PublicMask ) != 0 )
    {
        ClVec players;
        CrMngr.GetClients( players, false );
        for( auto it = players.begin(); it != players.end(); ++it )
        {
            Client* cl = *it;
            cl->Send_Property( NetProperty::Global, prop, Globals );
        }
    }
}

void FOServer::OnSendCritterValue( Entity* entity, Property* prop )
{
    Critter* cr = (Critter*) entity;

    bool     is_public = ( prop->GetAccess() & Property::PublicMask ) != 0;
    bool     is_protected = ( prop->GetAccess() & Property::ProtectedMask ) != 0;
    if( is_public || is_protected )
        cr->Send_Property( NetProperty::Chosen, prop, cr );
    if( is_public )
        cr->SendA_Property( NetProperty::Critter, prop, cr );
}

void FOServer::OnSendMapValue( Entity* entity, Property* prop )
{
    if( ( prop->GetAccess() & Property::PublicMask ) != 0 )
    {
        Map* map = (Map*) entity;
        map->SendProperty( NetProperty::Map, prop, map );
    }
}

void FOServer::OnSendLocationValue( Entity* entity, Property* prop )
{
    if( ( prop->GetAccess() & Property::PublicMask ) != 0 )
    {
        Location* loc = (Location*) entity;
        MapVec    maps;
        loc->GetMaps( maps, false );
        for( auto it = maps.begin(); it != maps.end(); ++it )
        {
            Map* map = *it;
            map->SendProperty( NetProperty::Location, prop, loc );
        }
    }
}

void FOServer::OnSetCritterHandsItemProtoId( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    Critter*   cr = (Critter*) entity;
    hash       value = *(hash*) cur_value;

    ProtoItem* unarmed = ProtoMngr.GetProtoItem( value ? value : ITEM_DEF_SLOT );
    if( !unarmed )
        unarmed = ProtoMngr.GetProtoItem( ITEM_DEF_SLOT );
    RUNTIME_ASSERT( unarmed );

    cr->GetHandsItem()->SetProto( unarmed );
    cr->GetHandsItem()->SetMode( 0 );
}

void FOServer::OnSetCritterHandsItemMode( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    Critter* cr = (Critter*) entity;
    uchar    value = *(uchar*) cur_value;

    cr->GetHandsItem()->SetMode( value );
}
