#include "Common.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "IniParser.h"

#ifdef FONLINE_SERVER
# include "Script.h"
# include "MapManager.h"
# include "EntityManager.h"
#endif

CritterManager CrMngr;

#define CRPROTO_APP    "Critter proto"

bool CritterManager::Init()
{
    WriteLog( "Critter manager initialization...\n" );

    allProtos.clear();
    Clear();

    WriteLog( "Critter manager initialization complete.\n" );
    return true;
}

void CritterManager::Finish()
{
    WriteLog( "Critter manager finish...\n" );

    allProtos.clear();
    Clear();

    WriteLog( "Critter manager finish complete.\n" );
}

void CritterManager::Clear()
{
    // ...
}

bool CritterManager::LoadProtos()
{
    WriteLog( "Load critter prototypes...\n" );

    FilesCollection files = FilesCollection( "focr" );
    uint            files_loaded = 0;
    int             errors = 0;
    while( files.IsNextFile() )
    {
        const char*  file_name;
        FileManager& file = files.GetNextFile( &file_name );
        if( !file.IsLoaded() )
        {
            WriteLog( "Unable to open file '%s'.\n", file_name );
            errors++;
            continue;
        }

        uint pid = Str::GetHash( file_name );
        if( allProtos.count( pid ) )
        {
            WriteLog( "Proto critter '%s' already loaded.\n", file_name );
            errors++;
            continue;
        }

        ProtoCritter* proto = new ProtoCritter();
        proto->ProtoId = pid;

        IniParser focr;
        focr.LoadFilePtr( (char*) file.GetBuf(), file.GetFsize() );
        if( focr.GotoNextApp( "Critter" ) )
        {
            if( !proto->Props.LoadFromText( focr.GetApp( "Critter" ) ) )
                errors++;
        }

        // Texts
        focr.CacheApps();
        StrSet& apps = focr.GetCachedApps();
        for( auto it = apps.begin(), end = apps.end(); it != end; ++it )
        {
            const string& app_name = *it;
            if( !( app_name.size() == 9 && app_name.find( "Text_" ) == 0 ) )
                continue;

            char* app_content = focr.GetApp( app_name.c_str() );
            FOMsg temp_msg;
            if( !temp_msg.LoadFromString( app_content, Str::Length( app_content ) ) )
                errors++;
            SAFEDELA( app_content );

            FOMsg* msg = new FOMsg();
            uint   str_num = 0;
            while( str_num = temp_msg.GetStrNumUpper( str_num ) )
            {
                uint count = temp_msg.Count( str_num );
                uint new_str_num = CR_STR_ID( proto->ProtoId, str_num );
                for( uint n = 0; n < count; n++ )
                    msg->AddStr( new_str_num, temp_msg.GetStr( str_num, n ) );
            }

            proto->TextsLang.push_back( *(uint*) app_name.substr( 5 ).c_str() );
            proto->Texts.push_back( msg );
        }

        #ifdef FONLINE_MAPPER
        proto->CollectionName = "all";
        #endif

        allProtos.insert( PAIR( proto->ProtoId, proto ) );
        files_loaded++;
    }

    WriteLog( "Load critter prototypes complete, count %u.\n", files_loaded );
    return errors == 0;
}

ProtoCritter* CritterManager::GetProto( hash proto_id )
{
    auto it = allProtos.find( proto_id );
    return it != allProtos.end() ? ( *it ).second : NULL;
}

ProtoCritterMap& CritterManager::GetAllProtos()
{
    return allProtos;
}

#ifdef FONLINE_SERVER
void CritterManager::DeleteNpc( Critter* cr )
{
    SYNC_LOCK( cr );
    RUNTIME_ASSERT( cr->IsNpc() );

    // Redundant calls
    if( cr->IsDestroying || cr->IsDestroyed )
        return;
    cr->IsDestroying = true;

    // Finish events
    cr->EventFinish( true );
    if( Script::PrepareContext( ServerFunctions.CritterFinish, _FUNC_, cr->GetInfo() ) )
    {
        Script::SetArgObject( cr );
        Script::SetArgBool( true );
        Script::RunPrepared();
    }

    // Tear off from environment
    cr->LockMapTransfers++;
    while( cr->GetMapId() || cr->GroupMove || cr->RealCountItems() )
    {
        // Delete from map
        if( cr->GetMapId() )
        {
            Map* map = MapMngr.GetMap( cr->GetMapId() );
            if( map )
            {
                cr->ClearVisible();
                map->EraseCritter( cr );
                map->EraseCritterEvents( cr );
                map->UnsetFlagCritter( cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), cr->IsDead() );
            }
            cr->SetMaps( 0, 0 );
        }

        // Delete from global
        if( cr->GroupMove )
        {
            GlobalMapGroup* group = cr->GroupMove;
            group->EraseCrit( cr );
            if( cr == group->Rule )
            {
                for( auto it_ = group->CritMove.begin(), end_ = group->CritMove.end(); it_ != end_; ++it_ )
                {
                    Critter* cr_ = *it_;
                    MapMngr.GM_GroupStartMove( cr_ );
                }
            }
            else
            {
                for( auto it_ = group->CritMove.begin(), end_ = group->CritMove.end(); it_ != end_; ++it_ )
                {
                    Critter* cr_ = *it_;
                    cr_->Send_RemoveCritter( cr );
                }

                Item* car = cr->GetItemCar();
                if( car && car->GetId() == group->CarId )
                {
                    group->CarId = 0;
                    MapMngr.GM_GroupSetMove( group, group->ToX, group->ToY, 0.0f );                                      // Stop others
                }
            }
            cr->GroupMove = NULL;
        }

        // Delete inventory
        cr->DeleteInventory();
    }
    cr->LockMapTransfers--;

    // Erase from main collection
    EntityMngr.UnregisterEntity( cr );

    // Invalidate for use
    cr->IsDestroyed = true;

    // Release after some time
    Job::DeferredRelease( cr );
}

Npc* CritterManager::CreateNpc( hash proto_id, IntVec* props_data, IntVec* items_data, const char* script_name, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy )
{
    ProtoCritter* proto = GetProto( proto_id );
    if( !proto )
    {
        WriteLogF( _FUNC_, " - Critter proto '%s' not found.\n", Str::GetName( proto_id ) );
        return NULL;
    }

    if( !map || hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
    {
        WriteLogF( _FUNC_, " - Wrong map values, hx %u, hy %u, map is nullptr '%s'.\n", hx, hy, !map ? "true" : "false" );
        return NULL;
    }

    if( !map->IsHexPassed( hx, hy ) )
    {
        if( accuracy )
        {
            WriteLogF( _FUNC_, " - Accuracy position busy, map '%s', hx %u, hy %u.\n", map->GetName(), hx, hy );
            return NULL;
        }

        static THREAD int cur_step = 0;
        short             hx_ = hx;
        short             hy_ = hy;
        short*            sx, * sy;
        GetHexOffsets( hx & 1, sx, sy );

        // Find in 2 hex radius
        for( int i = 0; ; i++ )
        {
            if( i >= 18 )
            {
                WriteLogF( _FUNC_, " - All positions busy, map '%s', hx %u, hy %u.\n", map->GetName(), hx, hy );
                return NULL;
            }
            cur_step++;
            if( cur_step >= 18 )
                cur_step = 0;
            if( hx_ + sx[ cur_step ] < 0 || hx_ + sx[ cur_step ] >= map->GetMaxHexX() )
                continue;
            if( hy_ + sy[ cur_step ] < 0 || hy_ + sy[ cur_step ] >= map->GetMaxHexY() )
                continue;
            if( !map->IsHexPassed( hx_ + sx[ cur_step ], hy_ + sy[ cur_step ] ) )
                continue;
            break;
        }

        hx_ += sx[ cur_step ];
        hy_ += sy[ cur_step ];
        hx = hx_;
        hy = hy_;
    }

    Npc* npc = new Npc( 0 );

    SYNC_LOCK( npc );
    EntityMngr.RegisterEntity( npc );
    Job::PushBack( JOB_CRITTER, npc );

    npc->Props = proto->Props;
    npc->Data.ProtoId = proto_id;
    npc->Data.Cond = COND_LIFE;
    npc->Data.Multihex = -1;

    // Flags and coords
    Location* loc = map->GetLocation( true );

    if( dir >= DIRS_COUNT )
        dir = Random( 0, DIRS_COUNT - 1 );
    npc->Data.Dir = dir;
    npc->Data.WorldX = ( loc ? loc->Data.WX : 100 );
    npc->Data.WorldY = ( loc ? loc->Data.WY : 100 );
    npc->SetHome( map->GetId(), hx, hy, dir );
    npc->SetMaps( map->GetId(), map->GetPid() );
    npc->Data.HexX = hx;
    npc->Data.HexY = hy;
    npc->RefreshName();

    if( props_data )
    {
        for( size_t i = 0, j = props_data->size() / 2; i < j; i++ )
            Properties::SetValueAsInt( npc, props_data->at( i * 2 ), props_data->at( i * 2 + 1 ) );
    }

    map->AddCritter( npc );

    if( items_data )
    {
        for( size_t i = 0, j = items_data->size() / 3; i < j; i++ )
        {
            hash pid = items_data->at( i * 3 );
            int  count = items_data->at( i * 3 + 1 );
            int  slot = items_data->at( i * 3 + 2 );
            if( pid != 0 && count > 0 && slot >= 0 && slot < 255 && Critter::SlotEnabled[ slot ] )
            {
                Item* item = ItemMngr.AddItemCritter( npc, pid, count );
                if( item && slot != SLOT_INV )
                    npc->MoveItem( SLOT_INV, slot, item->GetId(), item->GetCount() );
            }
        }
    }

    if( Script::PrepareContext( ServerFunctions.CritterInit, _FUNC_, npc->GetInfo() ) )
    {
        Script::SetArgObject( npc );
        Script::SetArgBool( true );
        Script::RunPrepared();
    }
    if( script_name && script_name[ 0 ] )
        npc->SetScript( script_name, true );
    map->AddCritterEvents( npc );

    npc->RefreshBag();
    npc->ProcessVisibleCritters();
    npc->ProcessVisibleItems();
    return npc;
}

bool CritterManager::RestoreNpc( uint id, CritData& data, Properties& props, Critter::CrTimeEventVec time_events )
{
    ProtoCritter* proto = GetProto( data.ProtoId );
    if( !proto )
    {
        WriteLogF( _FUNC_, " - Critter proto '%s' not found.\n", Str::GetName( data.ProtoId ) );
        return false;
    }

    Npc* npc = new Npc( id );
    npc->Data = data;
    npc->Props = props;

    SYNC_LOCK( npc );
    EntityMngr.RegisterEntity( npc );
    Job::PushBack( JOB_CRITTER, npc );
    return true;
}

void CritterManager::GetCritters( CrVec& critters, bool sync_lock )
{
    CrVec all_critters;
    EntityMngr.GetCritters( all_critters );

    CrVec find_critters;
    find_critters.reserve( all_critters.size() );
    for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
        find_critters.push_back( *it );

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck
        EntityMngr.GetCritters( all_critters );

        CrVec find_critters2;
        find_critters2.reserve( find_critters.size() );
        for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
            find_critters2.push_back( *it );

        // Search again, if different
        if( !CompareContainers( find_critters, find_critters2 ) )
        {
            GetCritters( critters, sync_lock );
            return;
        }
    }

    critters = find_critters;
}

void CritterManager::GetNpcs( PcVec& npcs, bool sync_lock )
{
    CrVec all_critters;
    EntityMngr.GetCritters( all_critters );

    PcVec find_npcs;
    find_npcs.reserve( all_critters.size() );
    for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsNpc() )
            find_npcs.push_back( (Npc*) cr );
    }

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = find_npcs.begin(), end = find_npcs.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck
        EntityMngr.GetCritters( all_critters );

        PcVec find_npcs2;
        find_npcs2.reserve( find_npcs.size() );
        for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr->IsNpc() )
                find_npcs2.push_back( (Npc*) cr );
        }

        // Search again, if different
        if( !CompareContainers( find_npcs, find_npcs2 ) )
        {
            GetNpcs( npcs, sync_lock );
            return;
        }
    }

    npcs = find_npcs;
}

void CritterManager::GetClients( ClVec& players, bool sync_lock, bool on_global_map /* = false */ )
{
    CrVec all_critters;
    EntityMngr.GetCritters( all_critters );

    ClVec find_players;
    find_players.reserve( all_critters.size() );
    for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->IsPlayer() && ( !on_global_map || !cr->GetMapId() ) )
            find_players.push_back( (Client*) cr );
    }

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = find_players.begin(), end = find_players.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck
        EntityMngr.GetCritters( all_critters );

        ClVec find_players2;
        find_players2.reserve( find_players.size() );
        for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr->IsPlayer() && ( !on_global_map || !cr->GetMapId() ) )
                find_players2.push_back( (Client*) cr );
        }

        // Search again, if different
        if( !CompareContainers( find_players, find_players2 ) )
        {
            GetClients( players, sync_lock, on_global_map );
            return;
        }
    }

    players = find_players;
}

void CritterManager::GetGlobalMapCritters( ushort wx, ushort wy, uint radius, int find_type, CrVec& critters, bool sync_lock )
{
    CrVec all_critters;
    EntityMngr.GetCritters( all_critters );

    CrVec find_critters;
    find_critters.reserve( all_critters.size() );
    for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !cr->GetMapId() && cr->GroupMove && DistSqrt( (int) cr->GroupMove->CurX, (int) cr->GroupMove->CurY, wx, wy ) <= radius &&
            cr->CheckFind( find_type ) )
            find_critters.push_back( cr );
    }

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck
        EntityMngr.GetCritters( all_critters );

        CrVec find_critters2;
        find_critters2.reserve( find_critters.size() );
        for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( !cr->GetMapId() && cr->GroupMove && DistSqrt( (int) cr->GroupMove->CurX, (int) cr->GroupMove->CurY, wx, wy ) <= radius &&
                cr->CheckFind( find_type ) )
                find_critters2.push_back( cr );
        }

        // Search again, if different
        if( !CompareContainers( find_critters, find_critters2 ) )
        {
            GetGlobalMapCritters( wx, wy, radius, find_type, critters, sync_lock );
            return;
        }
    }

    critters = find_critters;
}

Critter* CritterManager::GetCritter( uint crid, bool sync_lock )
{
    Critter* cr = EntityMngr.GetCritter( crid );
    if( cr && sync_lock )
        SYNC_LOCK( cr );
    return cr;
}

Client* CritterManager::GetPlayer( uint crid, bool sync_lock )
{
    if( !IS_CLIENT_ID( crid ) )
        return NULL;

    Client* cl = (Client*) EntityMngr.GetEntity( crid, EntityType::Client );
    if( !cl )
        return NULL;
    if( sync_lock )
        SYNC_LOCK( cl );
    return cl;
}

Client* CritterManager::GetPlayer( const char* name, bool sync_lock )
{
    EntityVec entities;
    EntityMngr.GetEntities( EntityType::Client, entities );

    Client* cl = NULL;
    for( auto it = entities.begin(); it != entities.end(); ++it )
    {
        Client* cl_ = (Client*) *it;
        if( Str::CompareCaseUTF8( name, cl_->GetName() ) )
        {
            cl = cl_;
            break;
        }
    }

    if( cl && sync_lock )
        SYNC_LOCK( cl );
    return cl;
}

Npc* CritterManager::GetNpc( uint crid, bool sync_lock )
{
    if( IS_CLIENT_ID( crid ) )
        return NULL;

    Npc* npc = (Npc*) EntityMngr.GetEntity( crid, EntityType::Npc );
    if( !npc )
        return NULL;
    if( sync_lock )
        SYNC_LOCK( npc );
    return npc;
}

uint CritterManager::PlayersInGame()
{
    return EntityMngr.GetEntitiesCount( EntityType::Client );
}

uint CritterManager::NpcInGame()
{
    return EntityMngr.GetEntitiesCount( EntityType::Npc );
}

uint CritterManager::CrittersInGame()
{
    return PlayersInGame() + NpcInGame();
}

#endif
