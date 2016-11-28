#include "Common.h"
#include "Server.h"

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
        // Check for path recalculation
        if( plane->Move.PathNum && plane->Move.TargId )
        {
            Critter* targ = npc->GetCritSelf( plane->Move.TargId );
            if( !targ || ( ( plane->Attack.LastHexX || plane->Attack.LastHexY ) && !CheckDist( targ->GetHexX(), targ->GetHexY(), plane->Attack.LastHexX, plane->Attack.LastHexY, 0 ) ) )
                plane->Move.PathNum = 0;
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
                Critter* targ = npc->GetCritSelf( plane->Move.TargId );
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
            Script::PrepareContext( bind_id, npc->GetInfo() );
            Script::SetArgEntity( npc );
            Script::RunPrepared();
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
        Critter* targ = npc->GetCritSelf( plane->Attack.TargId );
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
            if( npc->IsWait() )
                break;
            if( plane != npc->GetCurPlane() )
                break;                                               // Validate plane

            if( r0 )
            {
                weap = npc->GetItem( r0, false );
                RUNTIME_ASSERT( weap );
                weap->AddRef();
            }
            else
            {
                if( npc->GetIsNoUnarmed() )
                {
                    npc->NextPlane( REASON_NO_UNARMED );
                    break;
                }

                ProtoItem* unarmed = ProtoMngr.GetProtoItem( r2 );
                RUNTIME_ASSERT( unarmed );
                weap = new Item( 0, unarmed );
            }
            use = r1;
            weap->SetMode( MAKE_ITEM_MODE( use, 0 ) );

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
            if( npc->IsWait() )
                break;
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
            bool      is_range = ( max_dist > 2 );

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
            if( npc->IsWait() )
                break;

            r2 = targ->GetId();
            if( !npc->RunPlane( REASON_ATTACK_SHOOT, r0, r1, r2 ) )
            {
                WriteLog( "REASON_ATTACK_SHOOT fail. Skip attack.\n" );
                break;
            }
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
                Critter* targ_ = CrMngr.GetCritter( plane->Attack.TargId );
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

        uint   r0 = 0, r1 = 0, r2 = 0;
        if( !npc->RunPlane( REASON_DO, r0, r1, r2 ) )
        {
            WriteLog( "REASON_ATTACK_USE_AIM fail. Skip attack.\n" );
            break;
        }
        if( npc->IsWait() )
            break;
        if( plane != npc->GetCurPlane() )
            break;

        npc->NextPlane( REASON_SUCCESS );
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

bool FOServer::Dialog_Compile( Npc* npc, Client* cl, const Dialog& base_dlg, Dialog& compiled_dlg )
{
    if( base_dlg.Id < 2 )
    {
        WriteLog( "Wrong dialog id {}.\n", base_dlg.Id );
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
                entity = master->GetItemSlot( 1 );
                prop_registrator = Item::PropertiesRegistrator;
            }
            else if( demand.Type == DR_PROP_LOCATION )
            {
                Map* map = MapMngr.GetMap( master->GetMapId() );
                entity = ( map ? map->GetLocation() : nullptr );
                prop_registrator = Location::PropertiesRegistrator;
            }
            else if( demand.Type == DR_PROP_MAP )
            {
                entity = MapMngr.GetMap( master->GetMapId() );
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

                CScriptDict* dict = (CScriptDict*) prop->GetValue< void* >( entity );
                uint         slave_id = slave->GetId();
                void*        pvalue = dict->GetDefault( &slave_id, nullptr );
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
                entity = master->GetItemSlot( 1 );
                prop_registrator = Item::PropertiesRegistrator;
            }
            else if( result.Type == DR_PROP_LOCATION )
            {
                Map* map = MapMngr.GetMap( master->GetMapId() );
                entity = ( map ? map->GetLocation() : nullptr );
                prop_registrator = Location::PropertiesRegistrator;
            }
            else if( result.Type == DR_PROP_MAP )
            {
                entity = MapMngr.GetMap( master->GetMapId() );
                prop_registrator = Map::PropertiesRegistrator;
            }
            if( !entity )
                break;

            uint         prop_index = (uint) index;
            Property*    prop = prop_registrator->Get( prop_index );
            int          val = 0;
            CScriptDict* dict = nullptr;
            if( result.Type == DR_PROP_CRITTER_DICT )
            {
                if( !slave )
                    continue;

                dict = (CScriptDict*) prop->GetValue< void* >( master );
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
        WriteLog( "Dialog locked, client '{}'.\n", cl->GetInfo() );
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
                WriteLog( "Different maps, npc '{}' {}, client '{}' {}.\n", npc->GetInfo(), npc->GetMapId(), cl->GetInfo(), cl->GetMapId() );
                return;
            }

            uint talk_distance = npc->GetTalkDistance();
            talk_distance = ( talk_distance ? talk_distance : GameOpt.TalkDistance ) + cl->GetMultihex();
            if( !CheckDist( cl->GetHexX(), cl->GetHexY(), npc->GetHexX(), npc->GetHexY(), talk_distance ) )
            {
                cl->Send_XY( cl );
                cl->Send_XY( npc );
                cl->Send_TextMsg( cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME );
                WriteLog( "Wrong distance to npc '{}', client '{}'.\n", npc->GetInfo(), cl->GetInfo() );
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
            WriteLog( "Npc '{}' bad condition, client '{}'.\n", npc->GetInfo(), cl->GetInfo() );
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

        Script::RaiseInternalEvent( ServerFunctions.CritterTalk, cl, npc, true, npc->GetTalkedPlayers() + 1 );

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
            WriteLog( "Wrong distance to hexes, hx {}, hy {}, client '{}'.\n", hx, hy, cl->GetInfo() );
            return;
        }

        dialog_pack = DlgMngr.GetDialog( dlg_pack_id );
        dialogs = ( dialog_pack ? &dialog_pack->Dialogs : nullptr );
        if( !dialogs || !dialogs->size() )
        {
            WriteLog( "No dialogs, hx {}, hy {}, client '{}'.\n", hx, hy, cl->GetInfo() );
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
        WriteLog( "Dialog from link {} not found, client '{}', dialog pack {}.\n", go_dialog, cl->GetInfo(), dialog_pack->PackId );
        return;
    }

    // Compile
    if( !Dialog_Compile( npc, cl, *it_d, cl->Talk.CurDialog ) )
    {
        cl->Send_TextMsg( cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        WriteLog( "Dialog compile fail, client '{}', dialog pack {}.\n", cl->GetInfo(), dialog_pack->PackId );
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
    if( cl->Talk.CurDialog.DlgScript > NOT_ANSWER_BEGIN_BATTLE )
    {
        string lexems;
        Script::PrepareContext( cl->Talk.CurDialog.DlgScript, cl->GetInfo() );
        Script::SetArgEntity( cl );
        Script::SetArgEntity( npc );
        Script::SetArgObject( &lexems );
        cl->Talk.Locked = true;
        if( Script::RunPrepared() && lexems.length() <= MAX_DLG_LEXEMS_TEXT )
            cl->Talk.Lexems = lexems;
        else
            cl->Talk.Lexems = "";
        cl->Talk.Locked = false;
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

void FOServer::Process_Dialog( Client* cl )
{
    uchar is_npc;
    uint  talk_id;
    uchar num_answer;

    cl->Bin >> is_npc;
    cl->Bin >> talk_id;
    cl->Bin >> num_answer;

    if( ( is_npc && ( cl->Talk.TalkType != TALK_WITH_NPC || cl->Talk.TalkNpc != talk_id ) ) ||
        ( !is_npc && ( cl->Talk.TalkType != TALK_WITH_HEX || cl->Talk.DialogPackId != talk_id ) ) )
    {
        cl->CloseTalk();
        WriteLog( "Invalid talk id {} {}, client '{}'.\n", is_npc, talk_id, cl->GetInfo() );
        return;
    }

    if( cl->GetIsHide() )
        cl->SetIsHide( false );
    cl->ProcessTalk( true );

    Npc*        npc = nullptr;
    DialogPack* dialog_pack = nullptr;
    DialogsVec* dialogs = nullptr;

    // Find npc
    if( is_npc )
    {
        npc = CrMngr.GetNpc( talk_id );
        if( !npc )
        {
            cl->Send_TextMsg( cl, STR_DIALOG_NPC_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
            cl->CloseTalk();
            WriteLog( "Npc with id {} not found, client '{}'.\n", talk_id, cl->GetInfo() );
            return;
        }
    }

    // Set dialogs
    dialog_pack = DlgMngr.GetDialog( cl->Talk.DialogPackId );
    dialogs = ( dialog_pack ? &dialog_pack->Dialogs : nullptr );
    if( !dialogs || !dialogs->size() )
    {
        cl->CloseTalk();
        WriteLog( "No dialogs, npc '{}', client '{}'.\n", npc->GetInfo(), cl->GetInfo() );
        return;
    }

    // Continue dialog
    Dialog*       cur_dialog = &cl->Talk.CurDialog;
    uint          last_dialog = cur_dialog->Id;
    uint          dlg_id;
    uint          force_dialog;
    DialogAnswer* answer;

    if( !cl->Talk.Barter )
    {
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
            WriteLog( "Wrong number of answer {}, client '{}'.\n", num_answer, cl->GetInfo() );
            cl->Send_Talk();             // Refresh
            return;
        }

        // Find answer
        answer = &( *( cur_dialog->Answers.begin() + num_answer ) );

        // Check demand again
        if( !Dialog_CheckDemand( npc, cl, *answer, true ) )
        {
            WriteLog( "Secondary check of dialog demands fail, client '{}'.\n", cl->GetInfo() );
            cl->CloseTalk();             // End
            return;
        }

        // Use result
        force_dialog = Dialog_UseResult( npc, cl, *answer );
        if( force_dialog )
            dlg_id = force_dialog;
        else
            dlg_id = answer->Link;

        // Special links
        switch( dlg_id )
        {
        case -3:
        case DIALOG_BARTER:
label_Barter:
            if( cur_dialog->DlgScript != NOT_ANSWER_CLOSE_DIALOG )
            {
                cl->Send_TextMsg( npc, STR_BARTER_NO_BARTER_NOW, SAY_DIALOG, TEXTMSG_GAME );
                return;
            }

            if( Script::RaiseInternalEvent( ServerFunctions.CritterBarter, cl, npc, true, npc->GetBarterPlayers() + 1 ) )
            {
                cl->Talk.Barter = true;
                cl->Talk.StartTick = Timer::GameTick();
                cl->Talk.TalkTime = GameOpt.DlgBarterMinTime;
            }
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
            Script::RaiseInternalEvent( ServerFunctions.CritterBarter, cl, npc, false, npc->GetBarterPlayers() + 1 );
    }

    // Find dialog
    auto it_d = std::find( dialogs->begin(), dialogs->end(), dlg_id );
    if( it_d == dialogs->end() )
    {
        cl->CloseTalk();
        cl->Send_TextMsg( cl, STR_DIALOG_FROM_LINK_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
        WriteLog( "Dialog from link {} not found, client '{}', dialog pack {}.\n", dlg_id, cl->GetInfo(), dialog_pack->PackId );
        return;
    }

    // Compile
    if( !Dialog_Compile( npc, cl, *it_d, cl->Talk.CurDialog ) )
    {
        cl->CloseTalk();
        cl->Send_TextMsg( cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        WriteLog( "Dialog compile fail, client '{}', dialog pack {}.\n", cl->GetInfo(), dialog_pack->PackId );
        return;
    }

    if( dlg_id != last_dialog )
        cl->Talk.LastDialogId = last_dialog;

    // Get lexems
    cl->Talk.Lexems.clear();
    if( cl->Talk.CurDialog.DlgScript > NOT_ANSWER_BEGIN_BATTLE )
    {
        string lexems;
        Script::PrepareContext( cl->Talk.CurDialog.DlgScript, cl->GetInfo() );
        Script::SetArgEntity( cl );
        Script::SetArgEntity( npc );
        Script::SetArgObject( &lexems );
        cl->Talk.Locked = true;
        if( Script::RunPrepared() && lexems.length() <= MAX_DLG_LEXEMS_TEXT )
            cl->Talk.Lexems = lexems;
        else
            cl->Talk.Lexems = "";
        cl->Talk.Locked = false;
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
