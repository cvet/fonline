#include "Server.h"
#include "Log.h"
#include "Testing.h"
#include "Timer.h"
#include "Settings.h"
#include <random>

void FOServer::ProcessMove( Critter* cr )
{
    #define MOVING_SUCCESS             ( 1 )
    #define MOVING_TARGET_NOT_FOUND    ( 2 )
    #define MOVING_CANT_MOVE           ( 3 )
    #define MOVING_GAG_CRITTER         ( 4 )
    #define MOVING_GAG_ITEM            ( 5 )
    #define MOVING_FIND_PATH_ERROR     ( 6 )
    #define MOVING_HEX_TOO_FAR         ( 7 )
    #define MOVING_HEX_BUSY            ( 8 )
    #define MOVING_HEX_BUSY_RING       ( 9 )
    #define MOVING_DEADLOCK            ( 10 )
    #define MOVING_TRACE_FAIL          ( 11 )

    if( cr->Moving.State )
        return;
    if( cr->IsBusy() || cr->IsWait() )
        return;

    // Check for path recalculation
    if( cr->Moving.PathNum && cr->Moving.TargId )
    {
        Critter* targ = cr->GetCritSelf( cr->Moving.TargId );
        if( !targ || ( ( cr->Moving.HexX || cr->Moving.HexY ) && !CheckDist( targ->GetHexX(), targ->GetHexY(), cr->Moving.HexX, cr->Moving.HexY, 0 ) ) )
            cr->Moving.PathNum = 0;
    }

    // Find path if not exist
    if( !cr->Moving.PathNum )
    {
        ushort   hx;
        ushort   hy;
        uint     cut;
        uint     trace;
        Critter* trace_cr;
        bool     check_cr = false;

        if( cr->Moving.TargId )
        {
            Critter* targ = cr->GetCritSelf( cr->Moving.TargId );
            if( !targ )
            {
                cr->Moving.State = MOVING_TARGET_NOT_FOUND;
                cr->SendA_XY();
                return;
            }

            hx = targ->GetHexX();
            hy = targ->GetHexY();
            cut = cr->Moving.Cut;
            trace = cr->Moving.Trace;
            trace_cr = targ;
            check_cr = true;
        }
        else
        {
            hx = cr->Moving.HexX;
            hy = cr->Moving.HexY;
            cut = cr->Moving.Cut;
            trace = 0;
            trace_cr = nullptr;
        }

        PathFindData pfd;
        pfd.Clear();
        pfd.MapId = cr->GetMapId();
        pfd.FromCritter = cr;
        pfd.FromX = cr->GetHexX();
        pfd.FromY = cr->GetHexY();
        pfd.ToX = hx;
        pfd.ToY = hy;
        pfd.IsRun = cr->Moving.IsRun;
        pfd.Multihex = cr->GetMultihex();
        pfd.Cut = cut;
        pfd.Trace = trace;
        pfd.TraceCr = trace_cr;
        pfd.CheckCrit = true;
        pfd.CheckGagItems = true;

        if( pfd.IsRun && cr->GetIsNoRun() )
            pfd.IsRun = false;
        if( !pfd.IsRun && cr->GetIsNoWalk() )
        {
            cr->Moving.State = MOVING_CANT_MOVE;
            return;
        }

        int result = MapMngr.FindPath( pfd );
        if( pfd.GagCritter )
        {
            cr->Moving.State = MOVING_GAG_CRITTER;
            cr->Moving.GagEntityId = pfd.GagCritter->GetId();
            return;
        }

        if( pfd.GagItem )
        {
            cr->Moving.State = MOVING_GAG_ITEM;
            cr->Moving.GagEntityId = pfd.GagItem->GetId();
            return;
        }

        // Failed
        if( result != FPATH_OK )
        {
            if( result == FPATH_ALREADY_HERE )
            {
                cr->Moving.State = MOVING_SUCCESS;
                return;
            }

            int reason = 0;
            switch( result )
            {
            case FPATH_MAP_NOT_FOUND:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FPATH_TOOFAR:
                reason = MOVING_HEX_TOO_FAR;
                break;
            case FPATH_ERROR:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FPATH_INVALID_HEXES:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FPATH_TRACE_TARG_NULL_PTR:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FPATH_HEX_BUSY:
                reason = MOVING_HEX_BUSY;
                break;
            case FPATH_HEX_BUSY_RING:
                reason = MOVING_HEX_BUSY_RING;
                break;
            case FPATH_DEADLOCK:
                reason = MOVING_DEADLOCK;
                break;
            case FPATH_TRACE_FAIL:
                reason = MOVING_TRACE_FAIL;
                break;
            default:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            }

            cr->Moving.State = reason;
            cr->SendA_XY();
            return;
        }

        // Success
        cr->Moving.PathNum = pfd.PathNum;
        cr->Moving.Iter = 0;
        cr->SetWait( 0 );
    }

    // Get path and move
    PathStepVec& path = MapMngr.GetPath( cr->Moving.PathNum );
    if( cr->Moving.PathNum && cr->Moving.Iter < path.size() )
    {
        PathStep& ps = path[ cr->Moving.Iter ];
        if( !CheckDist( cr->GetHexX(), cr->GetHexY(), ps.HexX, ps.HexY, 1 ) || !Act_Move( cr, ps.HexX, ps.HexY, ps.MoveParams ) )
        {
            // Error
            cr->Moving.PathNum = 0;
            cr->SendA_XY();
        }
        else
        {
            // Next
            cr->Moving.Iter++;
        }
    }
    else
    {
        // Done
        cr->Moving.State = MOVING_SUCCESS;
    }
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
    {
        static std::random_device rd;
        static std::mt19937       g( rd() );
        std::shuffle( compiled_dlg.Answers.begin(), compiled_dlg.Answers.end(), g );
    }
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
        WriteLog( "Dialog locked, client '{}'.\n", cl->GetName() );
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
                WriteLog( "Different maps, npc '{}' {}, client '{}' {}.\n", npc->GetName(), npc->GetMapId(), cl->GetName(), cl->GetMapId() );
                return;
            }

            uint talk_distance = npc->GetTalkDistance();
            talk_distance = ( talk_distance ? talk_distance : GameOpt.TalkDistance ) + cl->GetMultihex();
            if( !CheckDist( cl->GetHexX(), cl->GetHexY(), npc->GetHexX(), npc->GetHexY(), talk_distance ) )
            {
                cl->Send_XY( cl );
                cl->Send_XY( npc );
                cl->Send_TextMsg( cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME );
                WriteLog( "Wrong distance to npc '{}', client '{}'.\n", npc->GetName(), cl->GetName() );
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
            WriteLog( "Npc '{}' bad condition, client '{}'.\n", npc->GetName(), cl->GetName() );
            return;
        }

        if( !npc->IsFreeToTalk() )
        {
            cl->Send_TextMsg( cl, STR_DIALOG_MANY_TALKERS, SAY_NETMSG, TEXTMSG_GAME );
            return;
        }

        // Todo: IsPlaneNoTalk

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
            WriteLog( "Wrong distance to hexes, hx {}, hy {}, client '{}'.\n", hx, hy, cl->GetName() );
            return;
        }

        dialog_pack = DlgMngr.GetDialog( dlg_pack_id );
        dialogs = ( dialog_pack ? &dialog_pack->Dialogs : nullptr );
        if( !dialogs || !dialogs->size() )
        {
            WriteLog( "No dialogs, hx {}, hy {}, client '{}'.\n", hx, hy, cl->GetName() );
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
        WriteLog( "Dialog from link {} not found, client '{}', dialog pack {}.\n", go_dialog, cl->GetName(), dialog_pack->PackId );
        return;
    }

    // Compile
    if( !Dialog_Compile( npc, cl, *it_d, cl->Talk.CurDialog ) )
    {
        cl->Send_TextMsg( cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        WriteLog( "Dialog compile fail, client '{}', dialog pack {}.\n", cl->GetName(), dialog_pack->PackId );
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
    if( cl->Talk.CurDialog.DlgScript )
    {
        string lexems;
        Script::PrepareContext( cl->Talk.CurDialog.DlgScript, cl->GetName() );
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

    cl->Connection->Bin >> is_npc;
    cl->Connection->Bin >> talk_id;
    cl->Connection->Bin >> num_answer;

    if( ( is_npc && ( cl->Talk.TalkType != TALK_WITH_NPC || cl->Talk.TalkNpc != talk_id ) ) ||
        ( !is_npc && ( cl->Talk.TalkType != TALK_WITH_HEX || cl->Talk.DialogPackId != talk_id ) ) )
    {
        cl->CloseTalk();
        WriteLog( "Invalid talk id {} {}, client '{}'.\n", is_npc, talk_id, cl->GetName() );
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
            WriteLog( "Npc with id {} not found, client '{}'.\n", talk_id, cl->GetName() );
            return;
        }
    }

    // Set dialogs
    dialog_pack = DlgMngr.GetDialog( cl->Talk.DialogPackId );
    dialogs = ( dialog_pack ? &dialog_pack->Dialogs : nullptr );
    if( !dialogs || !dialogs->size() )
    {
        cl->CloseTalk();
        WriteLog( "No dialogs, npc '{}', client '{}'.\n", npc->GetName(), cl->GetName() );
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
            WriteLog( "Wrong number of answer {}, client '{}'.\n", num_answer, cl->GetName() );
            cl->Send_Talk();             // Refresh
            return;
        }

        // Find answer
        answer = &( *( cur_dialog->Answers.begin() + num_answer ) );

        // Check demand again
        if( !Dialog_CheckDemand( npc, cl, *answer, true ) )
        {
            WriteLog( "Secondary check of dialog demands fail, client '{}'.\n", cl->GetName() );
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
        case uint( -3 ):
        case DIALOG_BARTER:
label_Barter:
            if( cur_dialog->DlgScript )
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
        case uint( -2 ):
        case DIALOG_BACK:
            if( cl->Talk.LastDialogId )
            {
                dlg_id = cl->Talk.LastDialogId;
                break;
            }
        case uint( -1 ):
        case DIALOG_END:
            cl->CloseTalk();
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
        WriteLog( "Dialog from link {} not found, client '{}', dialog pack {}.\n", dlg_id, cl->GetName(), dialog_pack->PackId );
        return;
    }

    // Compile
    if( !Dialog_Compile( npc, cl, *it_d, cl->Talk.CurDialog ) )
    {
        cl->CloseTalk();
        cl->Send_TextMsg( cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        WriteLog( "Dialog compile fail, client '{}', dialog pack {}.\n", cl->GetName(), dialog_pack->PackId );
        return;
    }

    if( dlg_id != last_dialog )
        cl->Talk.LastDialogId = last_dialog;

    // Get lexems
    cl->Talk.Lexems.clear();
    if( cl->Talk.CurDialog.DlgScript )
    {
        string lexems;
        Script::PrepareContext( cl->Talk.CurDialog.DlgScript, cl->GetName() );
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
