#include "CritterManager.h"
#include "Script.h"
#include "MapManager.h"
#include "EntityManager.h"
#include "ProtoManager.h"

CritterManager CrMngr;

void CritterManager::DeleteNpc( Critter* cr )
{
    RUNTIME_ASSERT( cr->IsNpc() );

    // Redundant calls
    if( cr->IsDestroying || cr->IsDestroyed )
        return;
    cr->IsDestroying = true;

    // Finish event
    Script::RaiseInternalEvent( ServerFunctions.CritterFinish, cr );

    // Tear off from environment
    cr->LockMapTransfers++;
    while( cr->GetMapId() || cr->GlobalMapGroup || cr->RealCountItems() )
    {
        // Delete inventory
        cr->DeleteInventory();

        // Delete from maps
        Map* map = cr->GetMap();
        if( map )
            MapMngr.EraseCrFromMap( cr, map );
        else if( cr->GlobalMapGroup )
            MapMngr.EraseCrFromMap( cr, nullptr );
    }
    cr->LockMapTransfers--;

    // Erase from main collection
    EntityMngr.UnregisterEntity( cr );

    // Invalidate for use
    cr->IsDestroyed = true;
    cr->Release();
}

Npc* CritterManager::CreateNpc( hash proto_id, Properties* props, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy )
{
    ProtoCritter* proto = ProtoMngr.GetProtoCritter( proto_id );
    if( !proto )
    {
        WriteLog( "Critter proto '{}' not found.\n", Str::GetName( proto_id ) );
        return nullptr;
    }

    if( !map || hx >= map->GetWidth() || hy >= map->GetHeight() )
    {
        WriteLog( "Wrong map values, hx {}, hy {}, map is nullptr '{}'.\n", hx, hy, !map ? "true" : "false" );
        return nullptr;
    }

    uint multihex;
    if( !props )
        multihex = proto->GetMultihex();
    else
        multihex = props->GetPropValue< uint >( Critter::PropertyMultihex );

    if( !map->IsHexesPassed( hx, hy, multihex ) )
    {
        if( accuracy )
        {
            WriteLog( "Accuracy position busy, map '{}', hx {}, hy {}.\n", map->GetName(), hx, hy );
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
                WriteLog( "All positions busy, map '{}', hx {}, hy {}.\n", map->GetName(), hx, hy );
                return nullptr;
            }
            cur_step++;
            if( cur_step >= 18 )
                cur_step = 0;
            if( hx_ + sx[ cur_step ] < 0 || hx_ + sx[ cur_step ] >= map->GetWidth() )
                continue;
            if( hy_ + sy[ cur_step ] < 0 || hy_ + sy[ cur_step ] >= map->GetHeight() )
                continue;
            if( !map->IsHexesPassed( hx_ + sx[ cur_step ], hy_ + sy[ cur_step ], multihex ) )
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

    EntityMngr.RegisterEntity( npc );

    npc->SetCond( COND_LIFE );

    // Flags and coords
    Location* loc = map->GetLocation();

    if( dir >= DIRS_COUNT )
        dir = Random( 0, DIRS_COUNT - 1 );
    npc->SetWorldX( loc ? loc->GetWorldX() : 0 );
    npc->SetWorldY( loc ? loc->GetWorldY() : 0 );
    npc->SetHomeMapId( map->GetId() );
    npc->SetHomeHexX( hx );
    npc->SetHomeHexY( hy );
    npc->SetHomeDir( dir );
    npc->SetHexX( hx );
    npc->SetHexY( hy );
    npc->RefreshName();

    bool can = MapMngr.CanAddCrToMap( npc, map, hx, hy, 0 );
    RUNTIME_ASSERT( can );
    MapMngr.AddCrToMap( npc, map, hx, hy, dir, 0 );

    Script::RaiseInternalEvent( ServerFunctions.CritterInit, npc, true );
    npc->SetScript( nullptr, true );

    npc->ProcessVisibleCritters();
    npc->ProcessVisibleItems();
    return npc;
}

bool CritterManager::RestoreNpc( uint id, hash proto_id, const StrMap& props_data )
{
    ProtoCritter* proto = ProtoMngr.GetProtoCritter( proto_id );
    if( !proto )
    {
        WriteLog( "Proto critter '{}' is not loaded.\n", Str::GetName( proto_id ) );
        return false;
    }

    Npc* npc = new Npc( id, proto );
    if( !npc->Props.LoadFromText( props_data ) )
    {
        WriteLog( "Fail to restore properties for critter '{}' ({}).\n", Str::GetName( proto_id ), id );
        npc->Release();
        return false;
    }

    EntityMngr.RegisterEntity( npc );
    return true;
}

void CritterManager::GetCritters( CrVec& critters )
{
    CrVec all_critters;
    EntityMngr.GetCritters( all_critters );

    CrVec find_critters;
    find_critters.reserve( all_critters.size() );
    for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
        find_critters.push_back( *it );

    critters = find_critters;
}

void CritterManager::GetNpcs( PcVec& npcs )
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

    npcs = find_npcs;
}

void CritterManager::GetClients( ClVec& players, bool on_global_map /* = false */ )
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

    players = find_players;
}

void CritterManager::GetGlobalMapCritters( ushort wx, ushort wy, uint radius, int find_type, CrVec& critters )
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

    critters = find_critters;
}

Critter* CritterManager::GetCritter( uint crid )
{
    return EntityMngr.GetCritter( crid );
}

Client* CritterManager::GetPlayer( uint crid )
{
    if( !IS_CLIENT_ID( crid ) )
        return nullptr;
    return (Client*) EntityMngr.GetEntity( crid, EntityType::Client );
}

Client* CritterManager::GetPlayer( const char* name )
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
    return cl;
}

Npc* CritterManager::GetNpc( uint crid )
{
    if( IS_CLIENT_ID( crid ) )
        return nullptr;
    return (Npc*) EntityMngr.GetEntity( crid, EntityType::Npc );
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
