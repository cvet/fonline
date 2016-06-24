#include "Common.h"
#include "Server.h"

#define CHECK_NPC_AP( npc, map, need_ap )                                                                                        \
    do                                                                                                                           \
    {                                                                                                                            \
        if( npc->GetCurrentAp() / AP_DIVIDER < (int) ( need_ap ) )                                                               \
        {                                                                                                                        \
            uint ap_regeneration = npc->GetApRegenerationTime();                                                                 \
            if( !ap_regeneration )                                                                                               \
                ap_regeneration = GameOpt.ApRegeneration;                                                                        \
            npc->SetWait( ap_regeneration / npc->GetActionPoints() * ( (int) ( need_ap ) - npc->GetCurrentAp() / AP_DIVIDER ) ); \
            return;                                                                                                              \
        }                                                                                                                        \
    } while( 0 )
#define CHECK_NPC_REAL_AP( npc, map, need_ap )                                                                                \
    do                                                                                                                        \
    {                                                                                                                         \
        if( npc->GetRealAp() < (int) ( need_ap ) )                                                                            \
        {                                                                                                                     \
            uint ap_regeneration = npc->GetApRegenerationTime();                                                              \
            if( !ap_regeneration )                                                                                            \
                ap_regeneration = GameOpt.ApRegeneration;                                                                     \
            npc->SetWait( ap_regeneration / npc->GetActionPoints() * ( (int) ( need_ap ) - npc->GetRealAp() ) / AP_DIVIDER ); \
            return;                                                                                                           \
        }                                                                                                                     \
    } while( 0 )
#define CHECK_NPC_AP_R0( npc, map, need_ap )                                                                                     \
    do                                                                                                                           \
    {                                                                                                                            \
        if( npc->GetCurrentAp() / AP_DIVIDER < (int) ( need_ap ) )                                                               \
        {                                                                                                                        \
            uint ap_regeneration = npc->GetApRegenerationTime();                                                                 \
            if( !ap_regeneration )                                                                                               \
                ap_regeneration = GameOpt.ApRegeneration;                                                                        \
            npc->SetWait( ap_regeneration / npc->GetActionPoints() * ( (int) ( need_ap ) - npc->GetCurrentAp() / AP_DIVIDER ) ); \
            return false;                                                                                                        \
        }                                                                                                                        \
    } while( 0 )

void FOServer::ProcessAI( Npc* npc )
{
    // Check busy
    if( npc->IsBusy() )
        return;

    // Global map
    if( !npc->GetMapId() )
        return;

    // Get map
    Map* map = MapMngr.GetMap( npc->GetMapId() );
    if( !map )
        return;

    // Check wait
    if( npc->IsWait() )
        return;

    // Get current plane
    AIDataPlane* plane = npc->GetCurPlane();

    // Begin
    if( !plane )
    {
        // Check talking
        if( npc->GetTalkedPlayers() )
            return;

        // Enemy stack
        Critter* enemy = npc->ScanEnemyStack();
        if( enemy )
        {
            npc->SetTarget( REASON_FOUND_IN_ENEMY_STACK, enemy, GameOpt.DeadHitPoints, false );
            if( npc->GetCurPlane() || npc->IsBusy() || npc->IsWait() )
                return;
        }

        // Go home
        if( !npc->GetIsNoHome() && map->GetId() == npc->GetHomeMapId() && ( npc->GetHexX() != npc->GetHomeHexX() || npc->GetHexY() != npc->GetHomeHexY() ) )
        {
            if( !npc->GetIsNoWalk() )
            {
                uint tick = Timer::GameTick();
                if( tick < npc->TryingGoHomeTick )
                {
                    npc->SetWait( npc->TryingGoHomeTick - tick );
                    return;
                }

                npc->TryingGoHomeTick = tick + NPC_GO_HOME_WAIT_TICK;
                npc->MoveToHex( REASON_GO_HOME, npc->GetHomeHexX(), npc->GetHomeHexY(), npc->GetHomeDir(), false, 0 );
                return;
            }
            else if( map->IsHexesPassed( npc->GetHomeHexX(), npc->GetHomeHexY(), npc->GetMultihex() ) )
            {
                MapMngr.Transit( npc, map, npc->GetHomeHexX(), npc->GetHomeHexY(), npc->GetDir(), 2, 0, true );
                return;
            }
        }
        // Set home direction
        else if( !npc->GetIsNoHome() && !npc->GetIsNoRotate() && map->GetId() == npc->GetHomeMapId() && npc->GetDir() != npc->GetHomeDir() )
        {
            npc->SetDir( npc->GetHomeDir() );
            npc->SendA_Dir();
            npc->SetWait( 200 );
            return;
        }
        else if( !npc->GetIsNoFavoriteItem() )
        {
            hash  favor_item_pid;
            Item* favor_item;
            // Set favorite item to slot1
            favor_item_pid = npc->GetFavoriteItemPid( SLOT_HAND1 );
            if( favor_item_pid != npc->ItemSlotMain->GetProtoId() )
            {
                if( npc->ItemSlotMain->GetId() )
                {
                    AI_MoveItem( npc, map, npc->ItemSlotMain->GetCritSlot(), SLOT_INV, npc->ItemSlotMain->GetId(), npc->ItemSlotMain->GetCount() );
                    return;
                }
                else if( favor_item_pid && ( favor_item = npc->GetItemByPid( favor_item_pid ) ) )
                {
                    AI_MoveItem( npc, map, favor_item->GetCritSlot(), SLOT_HAND1, favor_item->GetId(), favor_item->GetCount() );
                    return;
                }
            }
            // Set favorite item to slot2
            favor_item_pid = npc->GetFavoriteItemPid( SLOT_HAND2 );
            if( favor_item_pid != npc->ItemSlotExt->GetProtoId() )
            {
                if( npc->ItemSlotExt->GetId() )
                {
                    AI_MoveItem( npc, map, npc->ItemSlotExt->GetCritSlot(), SLOT_INV, npc->ItemSlotExt->GetId(), npc->ItemSlotExt->GetCount() );
                    return;
                }
                else if( favor_item_pid && ( favor_item = npc->GetItemByPid( favor_item_pid ) ) )
                {
                    AI_MoveItem( npc, map, favor_item->GetCritSlot(), SLOT_HAND2, favor_item->GetId(), favor_item->GetCount() );
                    return;
                }
            }
            // Set favorite item to slot armor
            favor_item_pid = npc->GetFavoriteItemPid( SLOT_ARMOR );
            if( favor_item_pid != npc->ItemSlotArmor->GetProtoId() )
            {
                if( npc->ItemSlotArmor->GetId() )
                {
                    AI_MoveItem( npc, map, npc->ItemSlotArmor->GetCritSlot(), SLOT_INV, npc->ItemSlotArmor->GetId(), npc->ItemSlotArmor->GetCount() );
                    return;
                }
                else if( favor_item_pid && ( favor_item = npc->GetItemByPid( favor_item_pid ) ) && favor_item->IsArmor() && !favor_item->GetSlot() )
                {
                    AI_MoveItem( npc, map, favor_item->GetCritSlot(), SLOT_ARMOR, favor_item->GetId(), favor_item->GetCount() );
                    return;
                }
            }
        }

        // Idle
        if( npc->GetCurPlane() || npc->IsBusy() || npc->IsWait() )
            return;

        // Wait
        npc->SetWait( GameOpt.CritterIdleTick );
        return;
    }

    // Process move
    if( plane->IsMove )
    {
        // Check run availability
        if( plane->Move.IsRun )
        {
            if( npc->IsDmgLeg() )
                plane->Move.IsRun = false;                           // Clipped legs
            else if( npc->IsOverweight() )
                plane->Move.IsRun = false;                           // Overweight
        }

        // Check ap availability
        int ap_move = npc->GetApCostCritterMove( plane->Move.IsRun );
        plane->IsMove = false;
        CHECK_NPC_REAL_AP( npc, map, ap_move );
        plane->IsMove = true;

        // Check
        if( plane->Move.PathNum && plane->Move.TargId )
        {
            Critter* targ = npc->GetCritSelf( plane->Move.TargId, true );
            if( !targ || ( ( plane->Attack.LastHexX || plane->Attack.LastHexY ) && !CheckDist( targ->GetHexX(), targ->GetHexY(), plane->Attack.LastHexX, plane->Attack.LastHexY, 0 ) ) )
            {
                plane->Move.PathNum = 0;              // PathSafeFinish(*step);
                // npc->SendA_XY();
            }
        }

        // Find path if not exist
        if( !plane->Move.PathNum )
        {
            ushort   hx;
            ushort   hy;
            uint     cut;
            uint     trace;
            Critter* trace_cr;
            bool     check_cr = false;

            if( plane->Move.TargId )
            {
                Critter* targ = npc->GetCritSelf( plane->Move.TargId, true );
                if( !targ )
                {
                    plane->IsMove = false;
                    npc->SendA_XY();
                    return;
                }

                hx = targ->GetHexX();
                hy = targ->GetHexY();
                cut = plane->Move.Cut;
                trace = plane->Move.Trace;
                trace_cr = targ;
                check_cr = true;
            }
            else
            {
                hx = plane->Move.HexX;
                hy = plane->Move.HexY;
                cut = plane->Move.Cut;
                trace = 0;
                trace_cr = nullptr;
            }

            PathFindData pfd;
            pfd.Clear();
            pfd.MapId = npc->GetMapId();
            pfd.FromCritter = npc;
            pfd.FromX = npc->GetHexX();
            pfd.FromY = npc->GetHexY();
            pfd.ToX = hx;
            pfd.ToY = hy;
            pfd.IsRun = plane->Move.IsRun;
            pfd.Multihex = npc->GetMultihex();
            pfd.Cut = cut;
            pfd.Trace = trace;
            pfd.TraceCr = trace_cr;
            pfd.CheckCrit = true;
            pfd.CheckGagItems = true;

            if( pfd.IsRun && npc->GetIsNoRun() )
                pfd.IsRun = false;
            if( !pfd.IsRun && npc->GetIsNoWalk() )
            {
                plane->IsMove = false;
                return;
            }

            int result = MapMngr.FindPath( pfd );
            if( pfd.GagCritter )
            {
                plane->IsMove = false;
                npc->NextPlane( REASON_GAG_CRITTER, pfd.GagCritter, nullptr );
                return;
            }

            if( pfd.GagItem )
            {
                plane->IsMove = false;
                npc->NextPlane( REASON_GAG_ITEM, nullptr, pfd.GagItem );
                return;
            }

            // Failed
            if( result != FPATH_OK )
            {
                if( result == FPATH_ALREADY_HERE )
                {
                    plane->IsMove = false;
                    return;
                }

                int reason = 0;
                switch( result )
                {
                case FPATH_MAP_NOT_FOUND:
                    reason = REASON_FIND_PATH_ERROR;
                    break;
                case FPATH_TOOFAR:
                    reason = REASON_HEX_TOO_FAR;
                    break;
                case FPATH_ERROR:
                    reason = REASON_FIND_PATH_ERROR;
                    break;
                case FPATH_INVALID_HEXES:
                    reason = REASON_FIND_PATH_ERROR;
                    break;
                case FPATH_TRACE_TARG_NULL_PTR:
                    reason = REASON_FIND_PATH_ERROR;
                    break;
                case FPATH_HEX_BUSY:
                    reason = REASON_HEX_BUSY;
                    break;
                case FPATH_HEX_BUSY_RING:
                    reason = REASON_HEX_BUSY_RING;
                    break;
                case FPATH_DEADLOCK:
                    reason = REASON_DEADLOCK;
                    break;
                case FPATH_TRACE_FAIL:
                    reason = REASON_TRACE_FAIL;
                    break;
                default:
                    reason = REASON_FIND_PATH_ERROR;
                    break;
                }

                plane->IsMove = false;
                npc->SendA_XY();
                npc->NextPlane( reason );
                // if(!npc->GetCurPlane() && !npc->IsBusy() && !npc->IsWait()) npc->SetWait(Random(2000,3000));
                return;
            }

            // Success
            plane->Move.PathNum = pfd.PathNum;
            plane->Move.Iter = 0;
            npc->SetWait( 0 );
        }

        // Get path and move
        PathStepVec& path = MapMngr.GetPath( plane->Move.PathNum );
        if( plane->Move.PathNum && plane->Move.Iter < path.size() )
        {
            PathStep& ps = path[ plane->Move.Iter ];
            if( !CheckDist( npc->GetHexX(), npc->GetHexY(), ps.HexX, ps.HexY, 1 ) || !Act_Move( npc, ps.HexX, ps.HexY, ps.MoveParams ) )
            {
                // Error
                plane->Move.PathNum = 0;
                // plane->IsMove=false;
                npc->SendA_XY();
            }
            else
            {
                // Next
                plane->Move.Iter++;
            }
        }
        else
        {
            // Done
            plane->IsMove = false;
        }
    }

    bool is_busy = ( plane->IsMove || npc->IsBusy() || npc->IsWait() );

    // Process planes
    switch( plane->Type )
    {
/************************************************************************/
/* ==================================================================== */
/* ========================   Misc   ================================== */
/* ==================================================================== */
/************************************************************************/
    case AI_PLANE_MISC:
    {
        if( is_busy )
            break;

        uint wait = plane->Misc.WaitSecond;
        uint bind_id = plane->Misc.ScriptBindId;

        if( wait > GameOpt.FullSecond )
        {
            AI_Stay( npc, ( wait - GameOpt.FullSecond ) * 1000 / GameOpt.TimeMultiplier );
        }
        else if( bind_id )
        {
            plane->Misc.ScriptBindId = 0;
            if( Script::PrepareContext( bind_id, _FUNC_, npc->GetInfo() ) )
            {
                Script::SetArgEntity( npc );
                Script::RunPrepared();
            }
        }
        else
        {
            npc->NextPlane( REASON_SUCCESS );
        }
    }
    break;
/************************************************************************/
/* ==================================================================== */
/* ========================   Attack   ================================ */
/* ==================================================================== */
/************************************************************************/
    case AI_PLANE_ATTACK:
    {
        /************************************************************************/
        /* Target is visible                                                    */
        /************************************************************************/
        Critter* targ = npc->GetCritSelf( plane->Attack.TargId, true );
        if( targ )
        {
            /************************************************************************/
            /* Step 0: Check for success plane and continue target timeout          */
            /************************************************************************/

            if( plane->Attack.IsGag && ( targ->GetHexX() != plane->Attack.GagHexX || targ->GetHexY() != plane->Attack.GagHexY ) )
            {
                npc->NextPlane( REASON_SUCCESS );
                break;
            }

            bool attack_to_dead = ( plane->Attack.MinHp <= GameOpt.DeadHitPoints );
            if( !plane->Attack.IsGag && ( !attack_to_dead && targ->GetCurrentHp() <= plane->Attack.MinHp ) || ( attack_to_dead && targ->IsDead() ) )
            {
                npc->NextPlane( REASON_SUCCESS );
                break;
            }

            if( plane->IsMove && !plane->Attack.LastHexX && !plane->Attack.LastHexY )
            {
                plane->IsMove = false;
                npc->SendA_XY();
            }
            plane->Attack.LastHexX = targ->GetHexX();
            plane->Attack.LastHexY = targ->GetHexY();

            if( is_busy )
                break;

            /************************************************************************/
            /* Step 1: Choose weapon                                                */
            /************************************************************************/
            // Get battle weapon
            int   use;
            Item* weap = nullptr;
            uint  r0 = targ->GetId(), r1 = 0, r2 = 0;
            if( !npc->RunPlane( REASON_ATTACK_WEAPON, r0, r1, r2 ) )
            {
                WriteLog( "REASON_ATTACK_WEAPON fail. Skip attack.\n" );
                break;
            }
            if( plane != npc->GetCurPlane() )
                break;                                               // Validate plane

            if( r0 )
            {
                weap = npc->GetItem( r0, false );
            }
            else
            {
                if( npc->GetIsNoUnarmed() )
                {
                    npc->NextPlane( REASON_NO_UNARMED );
                    break;
                }

                ProtoItem* unarmed = ProtoMngr.GetProtoItem( r2 );
                if( !unarmed || !unarmed->GetWeapon_IsUnarmed() )
                    unarmed = ProtoMngr.GetProtoItem( ITEM_DEF_SLOT );

                Item* def_item_main = npc->GetHandsItem();
                if( def_item_main->Proto != unarmed )
                    def_item_main->SetProto( unarmed );

                weap = def_item_main;
            }
            use = r1;

            if( !weap || !weap->IsWeapon() || !weap->WeapIsUseAviable( use ) )
            {
                WriteLog( "REASON_ATTACK_WEAPON fail, debug values '%p' %d %d.\n", weap, weap ? weap->IsWeapon() : -1, weap ? weap->WeapIsUseAviable( use ) : -1 );
                break;
            }

            // Hide cur, show new
            if( weap != npc->ItemSlotMain )
            {
                // Hide cur item
                if( npc->ItemSlotMain->GetId() )                       // Is no hands
                {
                    Item* item_hand = npc->ItemSlotMain;
                    AI_MoveItem( npc, map, SLOT_HAND1, SLOT_INV, item_hand->GetId(), item_hand->GetCount() );
                    break;
                }

                // Show new
                if( weap->GetId() )                       // Is no hands
                {
                    AI_MoveItem( npc, map, weap->GetCritSlot(), SLOT_HAND1, weap->GetId(), weap->GetCount() );
                    break;
                }
            }
            npc->ItemSlotMain->SetWeaponMode( MAKE_ITEM_MODE( use, 0 ) );

            // Load weapon
            if( !npc->GetIsUnlimitedAmmo() && weap->GetWeapon_MaxAmmoCount() && weap->WeapIsEmpty() )
            {
                Item* ammo = npc->GetAmmoForWeapon( weap );
                if( !ammo )
                {
                    WriteLogF( _FUNC_, " - Ammo for weapon not found, full load, npc '%s'.\n", npc->GetInfo() );
                    weap->WeapLoadHolder();
                }
                else
                {
                    AI_ReloadWeapon( npc, map, weap, ammo->GetId() );
                    break;
                }
            }
            else if( npc->GetIsUnlimitedAmmo() && weap->GetWeapon_MaxAmmoCount() )
                weap->WeapLoadHolder();

            /************************************************************************/
            /* Step 2: Move to target                                               */
            /************************************************************************/
            bool is_can_walk = !npc->GetIsNoWalk();
            uint best_dist = 0, min_dist = 0, max_dist = 0;
            r0 = targ->GetId(), r1 = 0, r2 = 0;
            if( !npc->RunPlane( REASON_ATTACK_DISTANTION, r0, r1, r2 ) )
            {
                WriteLog( "REASON_ATTACK_DISTANTION fail. Skip attack.\n" );
                break;
            }
            if( plane != npc->GetCurPlane() )
                break;                                               // Validate plane

            best_dist = r0;
            min_dist = r1;
            max_dist = r2;

            if( r2 <= 0 )                 // Run away
            {
                npc->NextPlane( REASON_RUN_AWAY );
                break;
            }

            if( max_dist <= 0 )
            {
                uint look = npc->GetLookDistance();
                max_dist = npc->GetAttackDist( weap, use );
                if( max_dist > look )
                    max_dist = look;
            }
            if( min_dist <= 0 )
                min_dist = 1;

            if( min_dist > max_dist )
                min_dist = max_dist;
            best_dist = CLAMP( best_dist, min_dist, max_dist );

            ushort    hx = npc->GetHexX();
            ushort    hy = npc->GetHexY();
            ushort    t_hx = targ->GetHexX();
            ushort    t_hy = targ->GetHexY();
            ushort    res_hx = t_hx;
            ushort    res_hy = t_hy;
            bool      is_run = plane->Attack.IsRun;
            bool      is_range = ( ( use == 0 ? weap->GetWeapon_MaxDist_0() : ( use == 1 ? weap->GetWeapon_MaxDist_1() : weap->GetWeapon_MaxDist_2() ) ) > 2 );

            TraceData trace;
            trace.TraceMap = map;
            trace.BeginHx = hx;
            trace.BeginHy = hy;
            trace.EndHx = t_hx;
            trace.EndHy = t_hy;
            trace.Dist = max_dist;
            trace.FindCr = targ;

            // Dirt
            MapMngr.TraceBullet( trace );
            if( !trace.IsCritterFounded )
            {
                if( is_can_walk )
                    AI_MoveToCrit( npc, targ->GetId(), is_range ? 1 + npc->GetMultihex() : max_dist, max_dist + ( is_range ? 0 : 5 ), is_run );
                else
                    npc->NextPlane( REASON_CANT_WALK );
                break;
            }

            // Find better position
            if( is_range && is_can_walk )
            {
                trace.BeginHx = t_hx;
                trace.BeginHy = t_hy;
                trace.EndHx = hx;
                trace.EndHy = hy;
                trace.Dist = best_dist;
                trace.FindCr = npc;
                trace.IsCheckTeam = true;
                trace.BaseCrTeamId = npc->GetTeamId();
                MapMngr.TraceBullet( trace );
                if( !trace.IsCritterFounded )
                {
                    UShortPair last_passed;
                    trace.LastPassed = &last_passed;
                    trace.LastPassedSkipCritters = true;
                    trace.FindCr = nullptr;
                    trace.Dist = best_dist;

                    bool  find_ok = false;
                    float deq_step = 360.0f / float(max_dist * 6);
                    float deq = deq_step;
                    for( int i = 1, j = best_dist * 6; i <= j; i++ )                // Full round
                    {
                        trace.Angle = deq;
                        MapMngr.TraceBullet( trace );
                        if( ( trace.IsHaveLastPassed || ( last_passed.first == npc->GetHexX() && last_passed.second == npc->GetHexY() ) ) &&
                            DistGame( t_hx, t_hy, last_passed.first, last_passed.second ) >= min_dist )
                        {
                            res_hx = last_passed.first;
                            res_hy = last_passed.second;
                            find_ok = true;
                            break;
                        }

                        if( !( i & 1 ) )
                            deq = deq_step * float( ( i + 2 ) / 2 );
                        else
                            deq = -deq;
                    }

                    if( !find_ok )                           // Position not found
                    {
                        npc->NextPlane( REASON_POSITION_NOT_FOUND );
                        break;
                    }
                    else if( hx != res_hx || hy != res_hy )
                    {
                        if( is_can_walk )
                            AI_Move( npc, res_hx, res_hy, is_run, 0, 0 );
                        else
                            npc->NextPlane( REASON_CANT_WALK );
                        break;
                    }
                }
            }
            // Find precision HtH attack
            else if( !is_range )
            {
                if( !CheckDist( hx, hy, t_hx, t_hy, max_dist ) )
                {
                    if( !is_can_walk )
                        npc->NextPlane( REASON_CANT_WALK );
                    else if( max_dist > 1 && best_dist == 1 )                       // Check busy ring
                    {
                        short* rsx, * rsy;
                        GetHexOffsets( t_hx & 1, rsx, rsy );

                        int i;
                        for( i = 0; i < 6; i++, rsx++, rsy++ )
                            if( map->IsHexPassed( t_hx + *rsx, t_hy + *rsy ) )
                                break;

                        if( i == 6 )
                            AI_MoveToCrit( npc, targ->GetId(), max_dist, 0, is_run );
                        else
                            AI_MoveToCrit( npc, targ->GetId(), best_dist, 0, is_run );
                    }
                    else
                        AI_MoveToCrit( npc, targ->GetId(), best_dist, 0, is_run );
                    break;
                }
            }

            /************************************************************************/
            /* Step 3: Attack                                                       */
            /************************************************************************/

            r0 = targ->GetId();
            r1 = 0;
            r2 = 0;
            if( !npc->RunPlane( REASON_ATTACK_USE_AIM, r0, r1, r2 ) )
            {
                WriteLog( "REASON_ATTACK_USE_AIM fail. Skip attack.\n" );
                break;
            }

            if( r2 )
            {
                npc->SetWait( r2 );
                break;
            }

            if( r0 != (uint) use && weap->WeapIsUseAviable( r0 ) )
                use = r0;

            int aim = r1;

            weap->SetWeaponMode( MAKE_ITEM_MODE( use, aim ) );
            AI_Attack( npc, map, MAKE_ITEM_MODE( use, aim ), targ->GetId() );
        }
        /************************************************************************/
        /* Target not visible, try find by last stored position                 */
        /************************************************************************/
        else
        {
            if( is_busy )
                break;

            if( ( !plane->Attack.LastHexX && !plane->Attack.LastHexY ) || npc->GetIsNoWalk() )
            {
                Critter* targ_ = CrMngr.GetCritter( plane->Attack.TargId, true );
                npc->NextPlane( REASON_TARGET_DISAPPEARED, targ_, nullptr );
                break;
            }

            AI_Move( npc, plane->Attack.LastHexX, plane->Attack.LastHexY, plane->Attack.IsRun, 1 + npc->GetMultihex(), npc->GetLookDistance() / 2 );
            plane->Attack.LastHexX = 0;
            plane->Attack.LastHexY = 0;
        }
    }
    break;
/************************************************************************/
/* ==================================================================== */
/* ========================   Walk   ================================== */
/* ==================================================================== */
/************************************************************************/
    case AI_PLANE_WALK:
    {
        if( is_busy )
            break;

        if( CheckDist( npc->GetHexX(), npc->GetHexY(), plane->Walk.HexX, plane->Walk.HexY, plane->Walk.Cut ) )
        {
            if( plane->Walk.Dir < 6 )
            {
                npc->SetDir( plane->Walk.Dir );
                npc->SendA_Dir();
            }
            else if( plane->Walk.Cut )
            {
                npc->SetDir( GetFarDir( npc->GetHexX(), npc->GetHexY(), plane->Walk.HexX, plane->Walk.HexY ) );
                npc->SendA_Dir();
            }

            npc->NextPlane( REASON_SUCCESS );
        }
        else
        {
            if( !npc->GetIsNoWalk() )
                AI_Move( npc, plane->Walk.HexX, plane->Walk.HexY, plane->Walk.IsRun, plane->Walk.Cut, 0 );
            else
                npc->NextPlane( REASON_CANT_WALK );
        }
    }
    break;
/************************************************************************/
/* ==================================================================== */
/* ========================   Pick   ================================== */
/* ==================================================================== */
/************************************************************************/
    case AI_PLANE_PICK:
    {
        ushort hx = plane->Pick.HexX;
        ushort hy = plane->Pick.HexY;
        ushort pid = plane->Pick.Pid;
        uint   use_item_id = plane->Pick.UseItemId;
        bool   to_open = plane->Pick.ToOpen;
        bool   is_run = plane->Pick.IsRun;

        Item*  item = map->GetItemHex( hx, hy, pid, nullptr );         // Cheat
        if( !item || ( item->IsDoor() && ( to_open ? item->GetOpened() : !item->GetOpened() ) ) )
        {
            npc->NextPlane( REASON_SUCCESS );
            break;
        }

        if( use_item_id && !npc->GetItem( use_item_id, true ) )
        {
            npc->NextPlane( REASON_USE_ITEM_NOT_FOUND );
            break;
        }

        if( is_busy )
            break;

        ProtoItem* proto_item = ProtoMngr.GetProtoItem( pid );
        if( !proto_item )
        {
            npc->NextPlane( REASON_SUCCESS );
            break;
        }

        int use_dist = npc->GetUseDist();
        if( !CheckDist( npc->GetHexX(), npc->GetHexY(), hx, hy, use_dist ) )
        {
            AI_Move( npc, hx, hy, is_run, use_dist, 0 );
        }
        else
        {
            npc->SetDir( GetFarDir( npc->GetHexX(), npc->GetHexY(), plane->Walk.HexX, plane->Walk.HexY ) );
            npc->SendA_Dir();
            if( AI_PickItem( npc, map, hx, hy, pid, use_item_id ) )
                npc->NextPlane( REASON_SUCCESS );
        }
    }
    break;
/************************************************************************/
/* ==================================================================== */
/* ==================================================================== */
/* ==================================================================== */
/************************************************************************/
    default:
    {
        npc->NextPlane( REASON_SUCCESS );
    }
    break;
    }
}

bool FOServer::AI_Stay( Npc* npc, uint ms )
{
    npc->SetWait( ms );
    return true;
}

bool FOServer::AI_Move( Npc* npc, ushort hx, ushort hy, bool is_run, uint cut, uint trace )
{
    AIDataPlane* plane = npc->GetCurPlane();
    if( !plane )
        return false;
    plane->IsMove = true;
    plane->Move.HexX = hx;
    plane->Move.HexY = hy;
    plane->Move.IsRun = is_run;
    plane->Move.Cut = cut;
    plane->Move.Trace = trace;
    plane->Move.TargId = 0;
    plane->Move.PathNum = 0;
    plane->Move.Iter = 0;
    return true;
}

bool FOServer::AI_MoveToCrit( Npc* npc, uint targ_id, uint cut, uint trace, bool is_run )
{
    AIDataPlane* plane = npc->GetCurPlane();
    if( !plane )
        return false;
    plane->IsMove = true;
    plane->Move.TargId = targ_id;
    plane->Move.IsRun = is_run;
    plane->Move.Cut = cut;
    plane->Move.Trace = trace;
    plane->Move.HexX = 0;
    plane->Move.HexY = 0;
    plane->Move.PathNum = 0;
    plane->Move.Iter = 0;
    return true;
}

bool FOServer::AI_MoveItem( Npc* npc, Map* map, uchar from_slot, uchar to_slot, uint item_id, uint count )
{
    bool is_castling = ( ( from_slot == SLOT_HAND1 && to_slot == SLOT_HAND2 ) || ( from_slot == SLOT_HAND2 && to_slot == SLOT_HAND1 ) );
    uint ap_cost = ( is_castling ? 0 : npc->GetApCostMoveItemInventory() );
    if( to_slot == SLOT_GROUND )
        ap_cost = npc->GetApCostDropItem();
    CHECK_NPC_AP_R0( npc, map, ap_cost );

    npc->SetWait( GameOpt.Breaktime );
    return npc->MoveItem( from_slot, to_slot, item_id, count );
}

bool FOServer::AI_Attack( Npc* npc, Map* map, uchar mode, uint targ_id )
{
    int ap_cost = npc->GetUseApCost( npc->ItemSlotMain, mode );

    CHECK_NPC_AP_R0( npc, map, ap_cost );

    Critter* targ = npc->GetCritSelf( targ_id, false );
    if( !targ || !Act_Attack( npc, mode, targ_id ) )
        return false;

    return true;
}

bool FOServer::AI_PickItem( Npc* npc, Map* map, ushort hx, ushort hy, hash pid, uint use_item_id )
{
    CHECK_NPC_AP_R0( npc, map, npc->GetApCostPickItem() );
    return Act_PickItem( npc, hx, hy, pid );
}

bool FOServer::AI_ReloadWeapon( Npc* npc, Map* map, Item* weap, uint ammo_id )
{
    int ap_cost = npc->GetUseApCost( npc->ItemSlotMain, USE_RELOAD );
    CHECK_NPC_AP_R0( npc, map, ap_cost );
    return Act_Reload( npc, weap->GetId(), ammo_id );
}

bool FOServer::Dialog_Compile( Npc* npc, Client* cl, const Dialog& base_dlg, Dialog& compiled_dlg )
{
    if( base_dlg.Id < 2 )
    {
        WriteLogF( _FUNC_, " - Wrong dialog id %u.\n", base_dlg.Id );
        return false;
    }
    compiled_dlg = base_dlg;

    for( auto it_a = compiled_dlg.Answers.begin(); it_a != compiled_dlg.Answers.end();)
    {
        if( !Dialog_CheckDemand( npc, cl, *it_a, false ) )
            it_a = compiled_dlg.Answers.erase( it_a );
        else
            it_a++;
    }

    if( !GameOpt.NoAnswerShuffle && !compiled_dlg.IsNoShuffle() )
        std::random_shuffle( compiled_dlg.Answers.begin(), compiled_dlg.Answers.end() );
    return true;
}

bool FOServer::Dialog_CheckDemand( Npc* npc, Client* cl, DialogAnswer& answer, bool recheck )
{
    if( !answer.Demands.size() )
        return true;

    Critter* master;
    Critter* slave;

    for( auto it = answer.Demands.begin(), end = answer.Demands.end(); it != end; ++it )
    {
        DemandResult& demand = *it;
        if( recheck && demand.NoRecheck )
            continue;

        switch( demand.Who )
        {
        case DR_WHO_PLAYER:
            master = cl;
            slave = npc;
            break;
        case DR_WHO_NPC:
            master = npc;
            slave = cl;
            break;
        default:
            continue;
        }

        if( !master )
            continue;

        max_t index = demand.ParamId;
        switch( demand.Type )
        {
        case DR_PROP_GLOBAL:
        case DR_PROP_CRITTER:
        case DR_PROP_CRITTER_DICT:
        case DR_PROP_ITEM:
        case DR_PROP_LOCATION:
        case DR_PROP_MAP:
        {
            Entity*              entity = nullptr;
            PropertyRegistrator* prop_registrator = nullptr;
            if( demand.Type == DR_PROP_GLOBAL )
            {
                entity = master;
                prop_registrator = GlobalVars::PropertiesRegistrator;
            }
            else if( demand.Type == DR_PROP_CRITTER )
            {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if( demand.Type == DR_PROP_CRITTER_DICT )
            {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if( demand.Type == DR_PROP_ITEM )
            {
                entity = master->ItemSlotMain;
                prop_registrator = Item::PropertiesRegistrator;
            }
            else if( demand.Type == DR_PROP_LOCATION )
            {
                Map* map = MapMngr.GetMap( master->GetMapId(), false );
                entity = ( map ? map->GetLocation( false ) : nullptr );
                prop_registrator = Location::PropertiesRegistrator;
            }
            else if( demand.Type == DR_PROP_MAP )
            {
                entity = MapMngr.GetMap( master->GetMapId(), false );
                prop_registrator = Map::PropertiesRegistrator;
            }
            if( !entity )
                break;

            uint      prop_index = (uint) index;
            Property* prop = prop_registrator->Get( prop_index );
            int       val = 0;
            if( demand.Type == DR_PROP_CRITTER_DICT )
            {
                if( !slave )
                    break;

                ScriptDict* dict = (ScriptDict*) prop->GetValue< void* >( entity );
                uint        slave_id = slave->GetId();
                void*       pvalue = dict->GetDefault( &slave_id, nullptr );
                dict->Release();
                if( pvalue )
                {
                    int value_type_id = prop->GetASObjectType()->GetSubTypeId( 1 );
                    if( value_type_id == asTYPEID_BOOL )
                        val = (int) *(bool*) pvalue ? 1 : 0;
                    else if( value_type_id == asTYPEID_INT8 )
                        val = (int) *(char*) pvalue;
                    else if( value_type_id == asTYPEID_INT16 )
                        val = (int) *(short*) pvalue;
                    else if( value_type_id == asTYPEID_INT32 )
                        val = (int) *(char*) pvalue;
                    else if( value_type_id == asTYPEID_INT64 )
                        val = (int) *(int64*) pvalue;
                    else if( value_type_id == asTYPEID_UINT8 )
                        val = (int) *(uchar*) pvalue;
                    else if( value_type_id == asTYPEID_UINT16 )
                        val = (int) *(ushort*) pvalue;
                    else if( value_type_id == asTYPEID_UINT32 )
                        val = (int) *(uint*) pvalue;
                    else if( value_type_id == asTYPEID_UINT64 )
                        val = (int) *(uint64*) pvalue;
                    else if( value_type_id == asTYPEID_FLOAT )
                        val = (int) *(float*) pvalue;
                    else if( value_type_id == asTYPEID_DOUBLE )
                        val = (int) *(double*) pvalue;
                    else
                        RUNTIME_ASSERT( false );
                }
            }
            else
            {
                val = prop->GetPODValueAsInt( entity );
            }

            switch( demand.Op )
            {
            case '>':
                if( val > demand.Value )
                    continue;
                break;
            case '<':
                if( val < demand.Value )
                    continue;
                break;
            case '=':
                if( val == demand.Value )
                    continue;
                break;
            case '!':
                if( val != demand.Value )
                    continue;
                break;
            case '}':
                if( val >= demand.Value )
                    continue;
                break;
            case '{':
                if( val <= demand.Value )
                    continue;
                break;
            default:
                break;
            }
        }
        break;
        case DR_ITEM:
        {
            hash pid = (hash) index;
            switch( demand.Op )
            {
            case '>':
                if( (int) master->CountItemPid( pid ) > demand.Value )
                    continue;
                break;
            case '<':
                if( (int) master->CountItemPid( pid ) < demand.Value )
                    continue;
                break;
            case '=':
                if( (int) master->CountItemPid( pid ) == demand.Value )
                    continue;
                break;
            case '!':
                if( (int) master->CountItemPid( pid ) != demand.Value )
                    continue;
                break;
            case '}':
                if( (int) master->CountItemPid( pid ) >= demand.Value )
                    continue;
                break;
            case '{':
                if( (int) master->CountItemPid( pid ) <= demand.Value )
                    continue;
                break;
            default:
                break;
            }
        }
        break;
        case DR_SCRIPT:
        {
            GameOpt.DialogDemandRecheck = recheck;
            cl->Talk.Locked = true;
            if( DialogScriptDemand( demand, master, slave ) )
            {
                cl->Talk.Locked = false;
                continue;
            }
            cl->Talk.Locked = false;
        }
        break;
        case DR_OR:
            return true;
        default:
            continue;
        }

        bool or_mod = false;
        for( ; it != end; ++it )
        {
            if( it->Type == DR_OR )
            {
                or_mod = true;
                break;
            }
        }
        if( !or_mod )
            return false;
    }

    return true;
}

uint FOServer::Dialog_UseResult( Npc* npc, Client* cl, DialogAnswer& answer )
{
    if( !answer.Results.size() )
        return 0;

    uint     force_dialog = 0;
    Critter* master;
    Critter* slave;

    for( auto it = answer.Results.begin(), end = answer.Results.end(); it != end; ++it )
    {
        DemandResult& result = *it;

        switch( result.Who )
        {
        case DR_WHO_PLAYER:
            master = cl;
            slave = npc;
            break;
        case DR_WHO_NPC:
            master = npc;
            slave = cl;
            break;
        default:
            continue;
        }

        if( !master )
            continue;

        max_t index = result.ParamId;
        switch( result.Type )
        {
        case DR_PROP_GLOBAL:
        case DR_PROP_CRITTER:
        case DR_PROP_CRITTER_DICT:
        case DR_PROP_ITEM:
        case DR_PROP_LOCATION:
        case DR_PROP_MAP:
        {
            Entity*              entity = nullptr;
            PropertyRegistrator* prop_registrator = nullptr;
            if( result.Type == DR_PROP_GLOBAL )
            {
                entity = master;
                prop_registrator = GlobalVars::PropertiesRegistrator;
            }
            else if( result.Type == DR_PROP_CRITTER )
            {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if( result.Type == DR_PROP_CRITTER_DICT )
            {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if( result.Type == DR_PROP_ITEM )
            {
                entity = master->ItemSlotMain;
                prop_registrator = Item::PropertiesRegistrator;
            }
            else if( result.Type == DR_PROP_LOCATION )
            {
                Map* map = MapMngr.GetMap( master->GetMapId(), false );
                entity = ( map ? map->GetLocation( false ) : nullptr );
                prop_registrator = Location::PropertiesRegistrator;
            }
            else if( result.Type == DR_PROP_MAP )
            {
                entity = MapMngr.GetMap( master->GetMapId(), false );
                prop_registrator = Map::PropertiesRegistrator;
            }
            if( !entity )
                break;

            uint        prop_index = (uint) index;
            Property*   prop = prop_registrator->Get( prop_index );
            int         val = 0;
            ScriptDict* dict = nullptr;
            if( result.Type == DR_PROP_CRITTER_DICT )
            {
                if( !slave )
                    continue;

                dict = (ScriptDict*) prop->GetValue< void* >( master );
                uint  slave_id = slave->GetId();
                void* pvalue = dict->GetDefault( &slave_id, nullptr );
                if( pvalue )
                {
                    int value_type_id = prop->GetASObjectType()->GetSubTypeId( 1 );
                    if( value_type_id == asTYPEID_BOOL )
                        val = (int) *(bool*) pvalue ? 1 : 0;
                    else if( value_type_id == asTYPEID_INT8 )
                        val = (int) *(char*) pvalue;
                    else if( value_type_id == asTYPEID_INT16 )
                        val = (int) *(short*) pvalue;
                    else if( value_type_id == asTYPEID_INT32 )
                        val = (int) *(char*) pvalue;
                    else if( value_type_id == asTYPEID_INT64 )
                        val = (int) *(int64*) pvalue;
                    else if( value_type_id == asTYPEID_UINT8 )
                        val = (int) *(uchar*) pvalue;
                    else if( value_type_id == asTYPEID_UINT16 )
                        val = (int) *(ushort*) pvalue;
                    else if( value_type_id == asTYPEID_UINT32 )
                        val = (int) *(uint*) pvalue;
                    else if( value_type_id == asTYPEID_UINT64 )
                        val = (int) *(uint64*) pvalue;
                    else if( value_type_id == asTYPEID_FLOAT )
                        val = (int) *(float*) pvalue;
                    else if( value_type_id == asTYPEID_DOUBLE )
                        val = (int) *(double*) pvalue;
                    else
                        RUNTIME_ASSERT( false );
                }
            }
            else
            {
                val = prop->GetPODValueAsInt( entity );
            }

            switch( result.Op )
            {
            case '+':
                val += result.Value;
                break;
            case '-':
                val -= result.Value;
                break;
            case '*':
                val *= result.Value;
                break;
            case '/':
                val /= result.Value;
                break;
            case '=':
                val = result.Value;
                break;
            default:
                break;
            }

            if( result.Type == DR_PROP_CRITTER_DICT )
            {
                uint64 buf = 0;
                void*  pvalue = &buf;
                int    value_type_id = prop->GetASObjectType()->GetSubTypeId( 1 );
                if( value_type_id == asTYPEID_BOOL )
                    *(bool*) pvalue = val != 0;
                else if( value_type_id == asTYPEID_INT8 )
                    *(char*) pvalue = (char) val;
                else if( value_type_id == asTYPEID_INT16 )
                    *(short*) pvalue = (short) val;
                else if( value_type_id == asTYPEID_INT32 )
                    *(int*) pvalue = (int) val;
                else if( value_type_id == asTYPEID_INT64 )
                    *(int64*) pvalue = (int64) val;
                else if( value_type_id == asTYPEID_UINT8 )
                    *(uchar*) pvalue = (uchar) val;
                else if( value_type_id == asTYPEID_UINT16 )
                    *(ushort*) pvalue = (ushort) val;
                else if( value_type_id == asTYPEID_UINT32 )
                    *(uint*) pvalue = (uint) val;
                else if( value_type_id == asTYPEID_UINT64 )
                    *(uint64*) pvalue = (uint64) val;
                else if( value_type_id == asTYPEID_FLOAT )
                    *(float*) pvalue = (float) val;
                else if( value_type_id == asTYPEID_DOUBLE )
                    *(double*) pvalue = (double) val;
                else
                    RUNTIME_ASSERT( false );

                uint slave_id = slave->GetId();
                dict->Set( &slave_id, pvalue );
                prop->SetValue< void* >( entity, dict );
                dict->Release();
            }
            else
            {
                prop->SetPODValueAsInt( entity, val );
            }
        }
            continue;
        case DR_ITEM:
        {
            hash pid = (hash) index;
            int  cur_count = master->CountItemPid( pid );
            int  need_count = cur_count;

            switch( result.Op )
            {
            case '+':
                need_count += result.Value;
                break;
            case '-':
                need_count -= result.Value;
                break;
            case '*':
                need_count *= result.Value;
                break;
            case '/':
                need_count /= result.Value;
                break;
            case '=':
                need_count = result.Value;
                break;
            default:
                continue;
            }

            if( need_count < 0 )
                need_count = 0;
            if( cur_count == need_count )
                continue;
            ItemMngr.SetItemCritter( master, pid, need_count );
        }
            continue;
        case DR_SCRIPT:
        {
            cl->Talk.Locked = true;
            force_dialog = DialogScriptResult( result, master, slave );
            cl->Talk.Locked = false;
        }
            continue;
        default:
            continue;
        }
    }

    return force_dialog;
}

void FOServer::Dialog_Begin( Client* cl, Npc* npc, hash dlg_pack_id, ushort hx, ushort hy, bool ignore_distance )
{
    if( cl->Talk.Locked )
    {
        WriteLogF( _FUNC_, " - Dialog locked, client '%s'.\n", cl->GetInfo() );
        return;
    }
    if( cl->Talk.TalkType != TALK_NONE )
        cl->CloseTalk();

    DialogPack* dialog_pack = nullptr;
    DialogsVec* dialogs = nullptr;

    // Talk with npc
    if( npc )
    {
        if( npc->GetIsNoTalk() )
            return;

        if( !dlg_pack_id )
        {
            hash npc_dlg_id = npc->GetDialogId();
            if( !npc_dlg_id )
                return;
            dlg_pack_id = npc_dlg_id;
        }

        if( !ignore_distance )
        {
            if( cl->GetMapId() != npc->GetMapId() )
            {
                WriteLogF( _FUNC_, " - Different maps, npc '%s' %u, client '%s' %u.\n", npc->GetInfo(), npc->GetMapId(), cl->GetInfo(), cl->GetMapId() );
                return;
            }

            uint talk_distance = npc->GetTalkDistance();
            talk_distance = ( talk_distance ? talk_distance : GameOpt.TalkDistance ) + cl->GetMultihex();
            if( !CheckDist( cl->GetHexX(), cl->GetHexY(), npc->GetHexX(), npc->GetHexY(), talk_distance ) )
            {
                cl->Send_XY( cl );
                cl->Send_XY( npc );
                cl->Send_TextMsg( cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME );
                WriteLogF( _FUNC_, " - Wrong distance to npc '%s', client '%s'.\n", npc->GetInfo(), cl->GetInfo() );
                return;
            }

            Map* map = MapMngr.GetMap( cl->GetMapId() );
            if( !map )
            {
                cl->Send_TextMsg( cl, STR_FINDPATH_AIMBLOCK, SAY_NETMSG, TEXTMSG_GAME );
                return;
            }

            TraceData trace;
            trace.TraceMap = map;
            trace.BeginHx = cl->GetHexX();
            trace.BeginHy = cl->GetHexY();
            trace.EndHx = npc->GetHexX();
            trace.EndHy = npc->GetHexY();
            trace.Dist = talk_distance;
            trace.FindCr = npc;
            MapMngr.TraceBullet( trace );
            if( !trace.IsCritterFounded )
            {
                cl->Send_TextMsg( cl, STR_FINDPATH_AIMBLOCK, SAY_NETMSG, TEXTMSG_GAME );
                return;
            }
        }

        if( !npc->IsLife() )
        {
            cl->Send_TextMsg( cl, STR_DIALOG_NPC_NOT_LIFE, SAY_NETMSG, TEXTMSG_GAME );
            WriteLogF( _FUNC_, " - Npc '%s' bad condition, client '%s'.\n", npc->GetInfo(), cl->GetInfo() );
            return;
        }

        if( !npc->IsFreeToTalk() )
        {
            cl->Send_TextMsg( cl, STR_DIALOG_MANY_TALKERS, SAY_NETMSG, TEXTMSG_GAME );
            return;
        }

        if( npc->IsPlaneNoTalk() )
        {
            cl->Send_TextMsg( cl, STR_DIALOG_NPC_BUSY, SAY_NETMSG, TEXTMSG_GAME );
            return;
        }

        Script::RaiseInternalEvent( ServerFunctions.CritterTalk, npc, cl, true, npc->GetTalkedPlayers() + 1 );

        dialog_pack = DlgMngr.GetDialog( dlg_pack_id );
        dialogs = ( dialog_pack ? &dialog_pack->Dialogs : nullptr );
        if( !dialogs || !dialogs->size() )
            return;

        if( !ignore_distance )
        {
            int dir = GetFarDir( cl->GetHexX(), cl->GetHexY(), npc->GetHexX(), npc->GetHexY() );
            npc->SetDir( ReverseDir( dir ) );
            npc->SendA_Dir();
            cl->SetDir( dir );
            cl->SendA_Dir();
            cl->Send_Dir( cl );
        }
    }
    // Talk with hex
    else
    {
        if( !ignore_distance && !CheckDist( cl->GetHexX(), cl->GetHexY(), hx, hy, GameOpt.TalkDistance + cl->GetMultihex() ) )
        {
            cl->Send_XY( cl );
            cl->Send_TextMsg( cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME );
            WriteLogF( _FUNC_, " - Wrong distance to hexes, hx %u, hy %u, client '%s'.\n", hx, hy, cl->GetInfo() );
            return;
        }

        dialog_pack = DlgMngr.GetDialog( dlg_pack_id );
        dialogs = ( dialog_pack ? &dialog_pack->Dialogs : nullptr );
        if( !dialogs || !dialogs->size() )
        {
            WriteLogF( _FUNC_, " - No dialogs, hx %u, hy %u, client '%s'.\n", hx, hy, cl->GetInfo() );
            return;
        }
    }

    // Predialogue installations
    auto it_d = dialogs->begin();
    uint go_dialog = uint( -1 );
    auto it_a = ( *it_d ).Answers.begin();
    for( ; it_a != ( *it_d ).Answers.end(); ++it_a )
    {
        if( Dialog_CheckDemand( npc, cl, *it_a, false ) )
            go_dialog = ( *it_a ).Link;
        if( go_dialog != uint( -1 ) )
            break;
    }

    if( go_dialog == uint( -1 ) )
        return;

    // Use result
    uint force_dialog = Dialog_UseResult( npc, cl, ( *it_a ) );
    if( force_dialog )
    {
        if( force_dialog == uint( -1 ) )
            return;
        go_dialog = force_dialog;
    }

    // Find dialog
    it_d = std::find( dialogs->begin(), dialogs->end(), go_dialog );
    if( it_d == dialogs->end() )
    {
        cl->Send_TextMsg( cl, STR_DIALOG_FROM_LINK_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
        WriteLogF( _FUNC_, " - Dialog from link %u not found, client '%s', dialog pack %u.\n", go_dialog, cl->GetInfo(), dialog_pack->PackId );
        return;
    }

    // Compile
    if( !Dialog_Compile( npc, cl, *it_d, cl->Talk.CurDialog ) )
    {
        cl->Send_TextMsg( cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        WriteLogF( _FUNC_, " - Dialog compile fail, client '%s', dialog pack %u.\n", cl->GetInfo(), dialog_pack->PackId );
        return;
    }

    if( npc )
    {
        cl->Talk.TalkType = TALK_WITH_NPC;
        cl->Talk.TalkNpc = npc->GetId();
    }
    else
    {
        cl->Talk.TalkType = TALK_WITH_HEX;
        cl->Talk.TalkHexMap = cl->GetMapId();
        cl->Talk.TalkHexX = hx;
        cl->Talk.TalkHexY = hy;
    }

    cl->Talk.DialogPackId = dlg_pack_id;
    cl->Talk.LastDialogId = go_dialog;
    cl->Talk.StartTick = Timer::GameTick();
    cl->Talk.TalkTime = GameOpt.DlgTalkMinTime;
    cl->Talk.Barter = false;
    cl->Talk.IgnoreDistance = ignore_distance;

    // Get lexems
    cl->Talk.Lexems.clear();
    if( cl->Talk.CurDialog.DlgScript > NOT_ANSWER_BEGIN_BATTLE && Script::PrepareContext( cl->Talk.CurDialog.DlgScript, _FUNC_, cl->GetInfo() ) )
    {
        ScriptString* lexems = ScriptString::Create();
        Script::SetArgEntity( cl );
        Script::SetArgEntity( npc );
        Script::SetArgObject( lexems );
        cl->Talk.Locked = true;
        if( Script::RunPrepared() && lexems->length() <= MAX_DLG_LEXEMS_TEXT )
            cl->Talk.Lexems = lexems->c_str();
        else
            cl->Talk.Lexems = "";
        cl->Talk.Locked = false;
        lexems->Release();
    }

    // On head text
    if( !cl->Talk.CurDialog.Answers.size() )
    {
        if( npc )
        {
            npc->SendAA_MsgLex( npc->VisCr, cl->Talk.CurDialog.TextId, SAY_NORM_ON_HEAD, TEXTMSG_DLG, cl->Talk.Lexems.c_str() );
        }
        else
        {
            Map* map = MapMngr.GetMap( cl->GetMapId() );
            if( map )
                map->SetTextMsg( hx, hy, 0, TEXTMSG_DLG, cl->Talk.CurDialog.TextId );
        }

        cl->CloseTalk();
        return;
    }

    // Open dialog window
    cl->Send_Talk();
}

void FOServer::Process_Dialog( Client* cl, bool is_say )
{
    uchar is_npc;
    uint  id_npc_talk;
    uchar num_answer;
    char  str[ MAX_SAY_NPC_TEXT + 1 ];

    cl->Bin >> is_npc;
    cl->Bin >> id_npc_talk;

    if( !is_say )
    {
        cl->Bin >> num_answer;
    }
    else
    {
        cl->Bin.Pop( str, MAX_SAY_NPC_TEXT );
        str[ MAX_SAY_NPC_TEXT ] = 0;
        if( !Str::Length( str ) )
        {
            WriteLogF( _FUNC_, " - Say text length is zero, client '%s'.\n", cl->GetInfo() );
            return;
        }
    }

    if( cl->GetIsHide() )
        cl->SetIsHide( false );
    cl->ProcessTalk( true );

    Npc*        npc = nullptr;
    DialogPack* dialog_pack = nullptr;
    DialogsVec* dialogs = nullptr;

    if( is_npc )
    {
        // Find npc
        npc = CrMngr.GetNpc( id_npc_talk, true );
        if( !npc )
        {
            cl->Send_TextMsg( cl, STR_DIALOG_NPC_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
            cl->CloseTalk();
            WriteLogF( _FUNC_, " - Npc not found, client '%s'.\n", cl->GetInfo() );
            return;
        }

        // Close another talk
        if( cl->Talk.TalkType != TALK_NONE && ( cl->Talk.TalkType != TALK_WITH_NPC || cl->Talk.TalkNpc != npc->GetId() ) )
            cl->CloseTalk();

        // Begin dialog
        if( cl->Talk.TalkType == TALK_NONE )
        {
            if( num_answer != ANSWER_BEGIN )
                return;
            Dialog_Begin( cl, npc, 0, 0, 0, false );
            return;
        }

        // Set dialogs
        dialog_pack = DlgMngr.GetDialog( cl->Talk.DialogPackId );
        dialogs = ( dialog_pack ? &dialog_pack->Dialogs : nullptr );
        if( !dialogs || !dialogs->size() )
        {
            cl->CloseTalk();
            WriteLogF( _FUNC_, " - No dialogs, npc '%s', client '%s'.\n", npc->GetInfo(), cl->GetInfo() );
            return;
        }
    }
    else
    {
        // Set dialogs
        dialog_pack = DlgMngr.GetDialog( id_npc_talk );
        dialogs = ( dialog_pack ? &dialog_pack->Dialogs : nullptr );
        if( !dialogs || !dialogs->size() )
        {
            WriteLogF( _FUNC_, " - No dialogs, dialog '%s' hx %u, hy %u, client '%s'.\n", Str::GetName( id_npc_talk ), cl->Talk.TalkHexX, cl->Talk.TalkHexY, cl->GetInfo() );
            return;
        }
    }

    // Continue dialog
    Dialog*       cur_dialog = &cl->Talk.CurDialog;
    uint          last_dialog = cur_dialog->Id;
    uint          dlg_id;
    uint          force_dialog;
    DialogAnswer* answer;

    if( !cl->Talk.Barter )
    {
        // Say about
        if( is_say )
        {
            if( cur_dialog->DlgScript <= NOT_ANSWER_BEGIN_BATTLE )
                return;

            ScriptString* str_ = ScriptString::Create( str );
            if( !Script::PrepareContext( cur_dialog->DlgScript, _FUNC_, cl->GetInfo() ) )
                return;
            Script::SetArgEntity( cl );
            Script::SetArgEntity( npc );
            Script::SetArgObject( str_ );

            cl->Talk.Locked = true;
            force_dialog = 0;
            if( Script::RunPrepared() && cur_dialog->RetVal )
                force_dialog = Script::GetReturnedUInt();
            cl->Talk.Locked = false;
            str_->Release();

            if( force_dialog )
            {
                dlg_id = force_dialog;
                goto label_ForceDialog;
            }
            return;
        }

        // Barter
        if( num_answer == ANSWER_BARTER )
            goto label_Barter;

        // Refresh
        if( num_answer == ANSWER_BEGIN )
        {
            cl->Send_Talk();
            return;
        }

        // End
        if( num_answer == ANSWER_END )
        {
            cl->CloseTalk();
            return;
        }

        // Invalid answer
        if( num_answer >= cur_dialog->Answers.size() )
        {
            WriteLogF( _FUNC_, " - Wrong number of answer %u, client '%s'.\n", num_answer, cl->GetInfo() );
            cl->Send_Talk();             // Refresh
            return;
        }

        // Find answer
        answer = &( *( cur_dialog->Answers.begin() + num_answer ) );

        // Check demand again
        if( !Dialog_CheckDemand( npc, cl, *answer, true ) )
        {
            WriteLogF( _FUNC_, " - Secondary check of dialog demands fail, client '%s'.\n", cl->GetInfo() );
            cl->CloseTalk();             // End
            return;
        }

        // Use result
        force_dialog = Dialog_UseResult( npc, cl, *answer );
        if( force_dialog )
            dlg_id = force_dialog;
        else
            dlg_id = answer->Link;

label_ForceDialog:
        // Special links
        switch( dlg_id )
        {
        case -3:
        case DIALOG_BARTER:
label_Barter:
            if( !npc )
            {
                cl->Send_TextMsg( cl, STR_BARTER_NO_BARTER_MODE, SAY_DIALOG, TEXTMSG_GAME );
                return;
            }
            if( cur_dialog->DlgScript != NOT_ANSWER_CLOSE_DIALOG )
            {
                cl->Send_TextMsg( npc, STR_BARTER_NO_BARTER_NOW, SAY_DIALOG, TEXTMSG_GAME );
                return;
            }
            if( npc->GetIsNoBarter() )
            {
                cl->Send_TextMsg( npc, STR_BARTER_NO_BARTER_MODE, SAY_DIALOG, TEXTMSG_GAME );
                return;
            }
            if( !Script::RaiseInternalEvent( ServerFunctions.CritterBarter, npc, cl, true, npc->GetBarterPlayers() + 1 ) )
            {
                // Message must processed in script
                return;
            }

            cl->Talk.Barter = true;
            cl->Talk.StartTick = Timer::GameTick();
            cl->Talk.TalkTime = GameOpt.DlgBarterMinTime;
            cl->Send_ContainerInfo( npc, TRANSFER_CRIT_BARTER, true );
            return;
        case -2:
        case DIALOG_BACK:
            if( cl->Talk.LastDialogId )
            {
                dlg_id = cl->Talk.LastDialogId;
                break;
            }
        case -1:
        case DIALOG_END:
            cl->CloseTalk();
            return;
        case -4:
        case DIALOG_ATTACK:
            cl->CloseTalk();
            if( npc )
                npc->SetTarget( REASON_FROM_DIALOG, cl, GameOpt.DeadHitPoints, false );
            return;
        default:
            break;
        }
    }
    else
    {
        cl->Talk.Barter = false;
        dlg_id = cur_dialog->Id;
        if( npc )
            Script::RaiseInternalEvent( ServerFunctions.CritterBarter, npc, cl, false, npc->GetBarterPlayers() + 1 );
    }

    // Find dialog
    auto it_d = std::find( dialogs->begin(), dialogs->end(), dlg_id );
    if( it_d == dialogs->end() )
    {
        cl->CloseTalk();
        cl->Send_TextMsg( cl, STR_DIALOG_FROM_LINK_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
        WriteLogF( _FUNC_, " - Dialog from link %u not found, client '%s', dialog pack %u.\n", dlg_id, cl->GetInfo(), dialog_pack->PackId );
        return;
    }

    // Compile
    if( !Dialog_Compile( npc, cl, *it_d, cl->Talk.CurDialog ) )
    {
        cl->CloseTalk();
        cl->Send_TextMsg( cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        WriteLogF( _FUNC_, " - Dialog compile fail, client '%s', dialog pack %u.\n", cl->GetInfo(), dialog_pack->PackId );
        return;
    }

    if( dlg_id != last_dialog )
        cl->Talk.LastDialogId = last_dialog;

    // Get lexems
    cl->Talk.Lexems.clear();
    if( cl->Talk.CurDialog.DlgScript > NOT_ANSWER_BEGIN_BATTLE && Script::PrepareContext( cl->Talk.CurDialog.DlgScript, _FUNC_, cl->GetInfo() ) )
    {
        ScriptString* lexems = ScriptString::Create();
        Script::SetArgEntity( cl );
        Script::SetArgEntity( npc );
        Script::SetArgObject( lexems );
        cl->Talk.Locked = true;
        if( Script::RunPrepared() && lexems->length() <= MAX_DLG_LEXEMS_TEXT )
            cl->Talk.Lexems = lexems->c_str();
        else
            cl->Talk.Lexems = "";
        cl->Talk.Locked = false;
        lexems->Release();
    }

    // On head text
    if( !cl->Talk.CurDialog.Answers.size() )
    {
        if( npc )
        {
            npc->SendAA_MsgLex( npc->VisCr, cl->Talk.CurDialog.TextId, SAY_NORM_ON_HEAD, TEXTMSG_DLG, cl->Talk.Lexems.c_str() );
        }
        else
        {
            Map* map = MapMngr.GetMap( cl->GetMapId() );
            if( map )
                map->SetTextMsg( cl->Talk.TalkHexX, cl->Talk.TalkHexY, 0, TEXTMSG_DLG, cl->Talk.CurDialog.TextId );
        }

        cl->CloseTalk();
        return;
    }

    cl->Talk.StartTick = Timer::GameTick();
    cl->Talk.TalkTime = GameOpt.DlgTalkMinTime;
    cl->Send_Talk();
}

void FOServer::Process_Barter( Client* cl )
{
    uint    msg_len;
    uint    id_npc_talk;
    ushort  sale_count;
    UIntVec sale_item_id;
    UIntVec sale_item_count;
    ushort  buy_count;
    UIntVec buy_item_id;
    UIntVec buy_item_count;
    uint    item_id;
    uint    item_count;
    uint    same_id = 0;

    cl->Bin >> msg_len;
    cl->Bin >> id_npc_talk;

    cl->Bin >> sale_count;
    for( int i = 0; i < sale_count; ++i )
    {
        cl->Bin >> item_id;
        cl->Bin >> item_count;

        if( std::find( sale_item_id.begin(), sale_item_id.end(), item_id ) != sale_item_id.end() )
            same_id++;
        sale_item_id.push_back( item_id );
        sale_item_count.push_back( item_count );
    }

    cl->Bin >> buy_count;
    for( int i = 0; i < buy_count; ++i )
    {
        cl->Bin >> item_id;
        cl->Bin >> item_count;

        if( std::find( buy_item_id.begin(), buy_item_id.end(), item_id ) != buy_item_id.end() )
            same_id++;
        buy_item_id.push_back( item_id );
        buy_item_count.push_back( item_count );
    }

    // Check
    if( !cl->GetMapId() )
    {
        WriteLogF( _FUNC_, " - Player try to barter from global map, client '%s'.\n", cl->GetInfo() );
        cl->Send_ContainerInfo();
        return;
    }

    Npc* npc = CrMngr.GetNpc( id_npc_talk, true );
    if( !npc )
    {
        WriteLogF( _FUNC_, " - Npc not found, client '%s'.\n", cl->GetInfo() );
        cl->Send_ContainerInfo();
        return;
    }

    bool is_free = ( npc->GetFreeBarterPlayer() == cl->GetId() );
    if( !sale_count && !is_free )
    {
        WriteLogF( _FUNC_, " - Player nothing for sale, client '%s'.\n", cl->GetInfo() );
        cl->Send_ContainerInfo();
        return;
    }

    if( same_id )
    {
        WriteLogF( _FUNC_, " - Same item id found, client '%s', npc '%s', same count %u.\n", cl->GetInfo(), npc->GetInfo(), same_id );
        cl->Send_TextMsg( cl, STR_BARTER_BAD_OFFER, SAY_DIALOG, TEXTMSG_GAME );
        cl->Send_ContainerInfo( npc, TRANSFER_CRIT_BARTER, false );
        return;
    }

    if( !npc->IsLife() )
    {
        WriteLogF( _FUNC_, " - Npc busy or dead, client '%s', npc '%s'.\n", cl->GetInfo(), npc->GetInfo() );
        cl->Send_ContainerInfo();
        return;
    }

    if( npc->GetIsNoBarter() )
    {
        WriteLogF( _FUNC_, " - Npc has NoBarterMode, client '%s', npc '%s'.\n", cl->GetInfo(), npc->GetInfo() );
        cl->Send_ContainerInfo();
        return;
    }

    if( cl->GetMapId() != npc->GetMapId() )
    {
        WriteLogF( _FUNC_, " - Different maps, npc '%s' %u, client '%s' %u.\n", npc->GetInfo(), npc->GetMapId(), cl->GetInfo(), cl->GetMapId() );
        cl->Send_ContainerInfo();
        return;
    }

    uint talk_distance = npc->GetTalkDistance();
    talk_distance = ( talk_distance ? talk_distance : GameOpt.TalkDistance ) + cl->GetMultihex();
    if( !CheckDist( cl->GetHexX(), cl->GetHexY(), npc->GetHexX(), npc->GetHexY(), talk_distance ) )
    {
        WriteLogF( _FUNC_, " - Wrong distance, client '%s', npc '%s'.\n", cl->GetInfo(), npc->GetInfo() );
        cl->Send_XY( cl );
        cl->Send_XY( npc );
        cl->Send_ContainerInfo();
        return;
    }

    if( cl->Talk.TalkType != TALK_WITH_NPC || cl->Talk.TalkNpc != npc->GetId() || !cl->Talk.Barter )
    {
        WriteLogF( _FUNC_, " - Dialog is closed or not beginning, client '%s', npc '%s'.\n", cl->GetInfo(), npc->GetInfo() );
        cl->Send_ContainerInfo();
        return;
    }

    // Check cost
    int     barter_k = npc->GetBarterCoefficient() - cl->GetBarterCoefficient();
    barter_k = CLAMP( barter_k, 5, 95 );
    int     sale_cost = 0;
    int     buy_cost = 0;
    uint    sale_weight = 0;
    uint    buy_weight = 0;
    uint    sale_volume = 0;
    uint    buy_volume = 0;
    ItemVec sale_items;
    ItemVec buy_items;

    for( int i = 0; i < sale_count; ++i )
    {
        Item* item = cl->GetItem( sale_item_id[ i ], true );
        if( !item )
        {
            WriteLogF( _FUNC_, " - Sale item not found, id %u, client '%s', npc '%s'.\n", sale_item_id[ i ], cl->GetInfo(), npc->GetInfo() );
            cl->Send_TextMsg( cl, STR_BARTER_SALE_ITEM_NOT_FOUND, SAY_DIALOG, TEXTMSG_GAME );
            cl->Send_ContainerInfo( npc, TRANSFER_CRIT_BARTER, false );
            return;
        }

        if( item->GetIsBadItem() )
        {
            cl->Send_TextMsg( cl, STR_BARTER_BAD_OFFER, SAY_DIALOG, TEXTMSG_GAME );
            cl->Send_ContainerInfo( npc, TRANSFER_CRIT_BARTER, false );
            return;
        }

        if( !sale_item_count[ i ] || sale_item_count[ i ] > item->GetCount() )
        {
            WriteLogF( _FUNC_, " - Sale item count error, id %u, count %u, real count %u, client '%s', npc '%s'.\n", sale_item_id[ i ], sale_item_count[ i ], item->GetCount(), cl->GetInfo(), npc->GetInfo() );
            cl->Send_ContainerInfo();
            return;
        }

        if( sale_item_count[ i ] > 1 && !item->GetStackable() )
        {
            WriteLogF( _FUNC_, " - Sale item is not stackable, id %u, count %u, client '%s', npc '%s'.\n", sale_item_id[ i ], sale_item_count[ i ], cl->GetInfo(), npc->GetInfo() );
            cl->Send_ContainerInfo();
            return;
        }

        if( !ItemMngr.ItemCheckMove( item, sale_item_count[ i ], cl, npc ) )
        {
            WriteLogF( _FUNC_, " - Sale item is not allowed to move, id %u, count %u, client '%s', npc '%s'.\n", sale_item_id[ i ], sale_item_count[ i ], cl->GetInfo(), npc->GetInfo() );
            cl->Send_ContainerInfo();
            return;
        }

        uint base_cost = item->GetCost1st();
        base_cost = base_cost * ( 100 - barter_k ) / 100;
        if( !base_cost )
            base_cost++;

        sale_cost += base_cost * sale_item_count[ i ];
        sale_weight += item->GetWeight() * sale_item_count[ i ];
        sale_volume += item->GetVolume() * sale_item_count[ i ];
        sale_items.push_back( item );
    }

    for( int i = 0; i < buy_count; ++i )
    {
        Item* item = npc->GetItem( buy_item_id[ i ], true );
        if( !item )
        {
            WriteLogF( _FUNC_, " - Buy item not found, id %u, client '%s', npc '%s'.\n", buy_item_id[ i ], cl->GetInfo(), npc->GetInfo() );
            cl->Send_TextMsg( cl, STR_BARTER_BUY_ITEM_NOT_FOUND, SAY_DIALOG, TEXTMSG_GAME );
            cl->Send_ContainerInfo( npc, TRANSFER_CRIT_BARTER, false );
            return;
        }

        if( !buy_item_count[ i ] || buy_item_count[ i ] > item->GetCount() )
        {
            WriteLogF( _FUNC_, " - Buy item count error, id %u, count %u, real count %u, client '%s', npc '%s'.\n", buy_item_id[ i ], buy_item_count[ i ], item->GetCount(), cl->GetInfo(), npc->GetInfo() );
            cl->Send_ContainerInfo();
            return;
        }

        if( buy_item_count[ i ] > 1 && !item->GetStackable() )
        {
            WriteLogF( _FUNC_, " - Buy item is not stackable, id %u, count %u, client '%s', npc '%s'.\n", buy_item_id[ i ], buy_item_count[ i ], cl->GetInfo(), npc->GetInfo() );
            cl->Send_ContainerInfo();
            return;
        }

        if( !ItemMngr.ItemCheckMove( item, buy_item_count[ i ], npc, cl ) )
        {
            WriteLogF( _FUNC_, " - Buy item is not allowed to move, id %u, count %u, client '%s', npc '%s'.\n", buy_item_id[ i ], buy_item_count[ i ], cl->GetInfo(), npc->GetInfo() );
            cl->Send_ContainerInfo();
            return;
        }

        uint base_cost = item->GetCost1st();
        base_cost = base_cost * ( 100 + barter_k ) / 100;
        if( !base_cost )
            base_cost++;

        buy_cost += base_cost * buy_item_count[ i ];
        buy_weight += item->GetWeight() * buy_item_count[ i ];
        buy_volume += item->GetVolume() * buy_item_count[ i ];
        buy_items.push_back( item );

        if( buy_cost > sale_cost && !is_free )
        {
            WriteLogF( _FUNC_, " - Buy is > sale - ignore barter, client '%s', npc '%s'.\n", cl->GetInfo(), npc->GetInfo() );
            cl->Send_TextMsg( cl, STR_BARTER_BAD_OFFER, SAY_DIALOG, TEXTMSG_GAME );
            cl->Send_ContainerInfo( npc, TRANSFER_CRIT_BARTER, false );
            return;
        }
    }

    // Check weight
    if( cl->GetFreeWeight() + (int) sale_weight < (int) buy_weight )
    {
        WriteLogF( _FUNC_, " - Overweight - ignore barter, client '%s', npc '%s'.\n", cl->GetInfo(), npc->GetInfo() );
        cl->Send_TextMsg( cl, STR_BARTER_OVERWEIGHT, SAY_DIALOG, TEXTMSG_GAME );
        cl->Send_ContainerInfo( npc, TRANSFER_CRIT_BARTER, false );
        return;
    }

    // Check volume
    if( cl->GetFreeVolume() + (int) sale_volume < (int) buy_volume )
    {
        WriteLogF( _FUNC_, " - Oversize - ignore barter, client '%s', npc '%s'.\n", cl->GetInfo(), npc->GetInfo() );
        cl->Send_TextMsg( cl, STR_BARTER_OVERSIZE, SAY_DIALOG, TEXTMSG_GAME );
        cl->Send_ContainerInfo( npc, TRANSFER_CRIT_BARTER, false );
        return;
    }

    // Barter script
    ScriptArray* sale_items_ = Script::CreateArray( "Item@[]" );
    ScriptArray* sale_items_count_ = Script::CreateArray( "uint[]" );
    ScriptArray* buy_items_ = Script::CreateArray( "Item@[]" );
    ScriptArray* buy_items_count_ = Script::CreateArray( "uint[]" );
    Script::AppendVectorToArrayRef( sale_items, sale_items_ );
    Script::AppendVectorToArray( sale_item_count, sale_items_count_ );
    Script::AppendVectorToArrayRef( buy_items, buy_items_ );
    Script::AppendVectorToArray( buy_item_count, buy_items_count_ );

    bool result = Script::RaiseInternalEvent( ServerFunctions.ItemsBarter, sale_items_, sale_items_count_, buy_items_, buy_items_count_, cl, npc );

    sale_items_->Release();
    sale_items_count_->Release();
    buy_items_->Release();
    buy_items_count_->Release();

    if( !result )
    {
        cl->Send_TextMsg( cl, STR_BARTER_BAD_OFFER, SAY_DIALOG, TEXTMSG_GAME );
        return;
    }

    // Transfer
    // From Player to Npc
    for( int i = 0; i < sale_count; ++i )
    {
        Item* item = cl->GetItem( sale_item_id[ i ], true );
        if( !ItemMngr.MoveItemCritters( cl, npc, item, sale_item_count[ i ] ) )
            WriteLogF( _FUNC_, " - Transfer item, from player to npc, fail, client '%s', npc '%s'.\n", cl->GetInfo(), npc->GetInfo() );
    }

    // From Npc to Player
    for( int i = 0; i < buy_count; ++i )
    {
        Item* item = npc->GetItem( buy_item_id[ i ], true );
        if( !ItemMngr.MoveItemCritters( npc, cl, item, buy_item_count[ i ] ) )
            WriteLogF( _FUNC_, " - Transfer item, from player to npc, fail, client '%s', npc '%s'.\n", cl->GetInfo(), npc->GetInfo() );
    }

    cl->Talk.StartTick = Timer::GameTick();
    cl->Talk.TalkTime = GameOpt.DlgBarterMinTime;
    if( !is_free )
        cl->Send_TextMsg( cl, STR_BARTER_GOOD_OFFER, SAY_DIALOG, TEXTMSG_GAME );
    cl->Send_ContainerInfo( npc, TRANSFER_CRIT_BARTER, false );
}
