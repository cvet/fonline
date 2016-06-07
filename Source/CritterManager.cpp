#include "CritterManager.h"
#include "Script.h"
#include "MapManager.h"
#include "EntityManager.h"
#include "ProtoManager.h"

CritterManager CrMngr;

void CritterManager::DeleteNpc( Critter* cr )
{
    SYNC_LOCK( cr );
    RUNTIME_ASSERT( cr->IsNpc() );

    // Redundant calls
    if( cr->IsDestroying || cr->IsDestroyed )
        return;
    cr->IsDestroying = true;

    // Finish event
    Script::RaiseInternalEvent( ServerFunctions.CritterFinish, cr, true );

    // Tear off from environment
    cr->LockMapTransfers++;
    while( cr->GetMapId() || cr->GlobalMapGroup || cr->RealCountItems() )
    {
        // Delete inventory
        cr->DeleteInventory();

        // Delete from maps
        if( cr->GetMapId() )
        {
            Map* map = MapMngr.GetMap( cr->GetMapId() );
            RUNTIME_ASSERT( map );
            MapMngr.EraseCrFromMap( cr, map );
        }
        else if( cr->GlobalMapGroup )
        {
            MapMngr.EraseCrFromMap( cr, nullptr );
        }
    }
    cr->LockMapTransfers--;

    // Erase from main collection
    EntityMngr.UnregisterEntity( cr );

    // Invalidate for use
    cr->IsDestroyed = true;

    // Release after some time
    Job::DeferredRelease( cr );
}

Npc* CritterManager::CreateNpc( hash proto_id, Properties* props, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy )
{
    ProtoCritter* proto = ProtoMngr.GetProtoCritter( proto_id );
    if( !proto )
    {
        WriteLogF( _FUNC_, " - Critter proto '%s' not found.\n", Str::GetName( proto_id ) );
        return nullptr;
    }

    if( !map || hx >= map->GetWidth() || hy >= map->GetHeight() )
    {
        WriteLogF( _FUNC_, " - Wrong map values, hx %u, hy %u, map is nullptr '%s'.\n", hx, hy, !map ? "true" : "false" );
        return nullptr;
    }

    if( !map->IsHexPassed( hx, hy ) )
    {
        if( accuracy )
        {
            WriteLogF( _FUNC_, " - Accuracy position busy, map '%s', hx %u, hy %u.\n", map->GetName(), hx, hy );
            return nullptr;
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
                return nullptr;
            }
            cur_step++;
            if( cur_step >= 18 )
                cur_step = 0;
            if( hx_ + sx[ cur_step ] < 0 || hx_ + sx[ cur_step ] >= map->GetWidth() )
                continue;
            if( hy_ + sy[ cur_step ] < 0 || hy_ + sy[ cur_step ] >= map->GetHeight() )
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

    Npc* npc = new Npc( 0, proto );
    if( props )
        npc->Props = *props;

    SYNC_LOCK( npc );
    EntityMngr.RegisterEntity( npc );
    Job::PushBack( JOB_CRITTER, npc );

    npc->SetCond( COND_LIFE );

    // Flags and coords
    Location* loc = map->GetLocation( true );

    if( dir >= DIRS_COUNT )
        dir = Random( 0, DIRS_COUNT - 1 );
    npc->SetWorldX( loc ? loc->GetWorldX() : 100 );
    npc->SetWorldY( loc ? loc->GetWorldY() : 100 );
    npc->SetHome( map->GetId(), hx, hy, dir );
    npc->SetMapId( map->GetId() );
    npc->SetMapPid( map->GetProtoId() );
    npc->SetHexX( hx );
    npc->SetHexY( hy );
    npc->RefreshName();

    bool can = MapMngr.CanAddCrToMap( npc, map, hx, hy, 0 );
    RUNTIME_ASSERT( can );
    MapMngr.AddCrToMap( npc, map, hx, hy, dir, 0 );

    Script::RaiseInternalEvent( ServerFunctions.CritterInit, npc, true );
    npc->SetScript( nullptr, true );

    npc->RefreshBag();
    npc->ProcessVisibleCritters();
    npc->ProcessVisibleItems();
    return npc;
}

bool CritterManager::RestoreNpc( uint id, hash proto_id, const StrMap& props_data )
{
    ProtoCritter* proto = ProtoMngr.GetProtoCritter( proto_id );
    if( !proto )
    {
        WriteLog( "Proto critter '%s' is not loaded.\n", Str::GetName( proto_id ) );
        return false;
    }

    Npc* npc = new Npc( id, proto );
    if( !npc->Props.LoadFromText( props_data ) )
    {
        WriteLog( "Fail to restore properties for critter '%s' (%u).\n", Str::GetName( proto_id ), id );
        npc->Release();
        return false;
    }

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
        if( !cr->GetMapId() && DistSqrt( (int) cr->GetWorldX(), (int) cr->GetWorldY(), wx, wy ) <= radius && cr->CheckFind( find_type ) )
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
            if( !cr->GetMapId() && DistSqrt( (int) cr->GetWorldX(), (int) cr->GetWorldY(), wx, wy ) <= radius && cr->CheckFind( find_type ) )
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
        return nullptr;

    Client* cl = (Client*) EntityMngr.GetEntity( crid, EntityType::Client );
    if( !cl )
        return nullptr;
    if( sync_lock )
        SYNC_LOCK( cl );
    return cl;
}

Client* CritterManager::GetPlayer( const char* name, bool sync_lock )
{
    EntityVec entities;
    EntityMngr.GetEntities( EntityType::Client, entities );

    Client* cl = nullptr;
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
        return nullptr;

    Npc* npc = (Npc*) EntityMngr.GetEntity( crid, EntityType::Npc );
    if( !npc )
        return nullptr;
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
