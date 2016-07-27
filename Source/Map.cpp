#include "Common.h"
#include "Map.h"
#include "Script.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "MapManager.h"

/************************************************************************/
/* Map                                                                  */
/************************************************************************/

PROPERTIES_IMPL( Map );
CLASS_PROPERTY_IMPL( Map, LoopTime1 );
CLASS_PROPERTY_IMPL( Map, LoopTime2 );
CLASS_PROPERTY_IMPL( Map, LoopTime3 );
CLASS_PROPERTY_IMPL( Map, LoopTime4 );
CLASS_PROPERTY_IMPL( Map, LoopTime5 );
CLASS_PROPERTY_IMPL( Map, Width );
CLASS_PROPERTY_IMPL( Map, Height );
CLASS_PROPERTY_IMPL( Map, WorkHexX );
CLASS_PROPERTY_IMPL( Map, WorkHexY );
CLASS_PROPERTY_IMPL( Map, LocId );
CLASS_PROPERTY_IMPL( Map, LocMapIndex );
CLASS_PROPERTY_IMPL( Map, RainCapacity );
CLASS_PROPERTY_IMPL( Map, CurDayTime );
CLASS_PROPERTY_IMPL( Map, ScriptId );
CLASS_PROPERTY_IMPL( Map, DayTime );
CLASS_PROPERTY_IMPL( Map, DayColor );
CLASS_PROPERTY_IMPL( Map, IsNoLogOut );

Map::Map( uint id, ProtoMap* proto, Location* location ): Entity( id, EntityType::Map, PropertiesRegistrator, proto )
{
    RUNTIME_ASSERT( proto );

    MEMORY_PROCESS( MEMORY_MAP, sizeof( Map ) );
    MEMORY_PROCESS( MEMORY_MAP_FIELD, GetWidth() * GetHeight() );

    mapLocation = location;

    hexFlags = nullptr;
    memzero( LoopLastTick, sizeof( LoopLastTick ) );

    hexFlagsSize = GetWidth() * GetHeight();
    hexFlags = new uchar[ hexFlagsSize ];
    memzero( hexFlags, hexFlagsSize );
}

Map::~Map()
{
    MEMORY_PROCESS( MEMORY_MAP, -(int) sizeof( Map ) );
    MEMORY_PROCESS( MEMORY_MAP_FIELD, -hexFlagsSize );
    SAFEDELA( hexFlags );
    hexFlagsSize = 0;
}

void Map::DeleteContent()
{
    while( !mapCritters.empty() || !hexItems.empty() )
    {
        // Transit players to global map
        KickPlayersToGlobalMap();

        // Delete npc
        dataLocker.Lock();
        PcVec del_npc = mapNpcs;
        dataLocker.Unlock();
        for( auto it = del_npc.begin(); it != del_npc.end(); ++it )
            CrMngr.DeleteNpc( *it );

        // Delete items
        dataLocker.Lock();
        ItemVec del_items = hexItems;
        dataLocker.Unlock();
        for( auto it = del_items.begin(); it != del_items.end(); ++it )
            ItemMngr.DeleteItem( *it );
    }
}

bool Map::Generate()
{
    UIntMap id_map;

    // Generate npc
    for( auto& base_cr : GetProtoMap()->CrittersVec )
    {
        // Create npc
        Npc* npc = CrMngr.CreateNpc( base_cr->GetProtoId(), &base_cr->Props, this, base_cr->GetHexX(), base_cr->GetHexY(), base_cr->GetDir(), true );
        if( !npc )
        {
            WriteLogF( _FUNC_, " - Create npc '%s' on map '%s' fail, continue generate.\n", base_cr->GetName(), GetName() );
            continue;
        }
        id_map.insert( PAIR( base_cr->GetId(), npc->GetId() ) );

        // Check condition
        if( npc->GetCond() != COND_LIFE )
        {
            if( npc->GetCond() == COND_DEAD )
            {
                npc->SetCurrentHp( GameOpt.DeadHitPoints - 1 );

                uint multihex = npc->GetMultihex();
                UnsetFlagCritter( npc->GetHexX(), npc->GetHexY(), multihex, false );
                SetFlagCritter( npc->GetHexX(), npc->GetHexY(), multihex, true );
            }
            else if( npc->GetCond() == COND_KNOCKOUT )
            {
                npc->KnockoutAp = 10000;
            }
        }
    }

    // Generate hex items
    for( auto& base_item : GetProtoMap()->HexItemsVec )
    {
        // Create item
        Item* item = ItemMngr.CreateItem( base_item->GetProtoId(), 0, &base_item->Props );
        if( !item )
        {
            WriteLogF( _FUNC_, " - Create item '%s' on map '%s' fail, continue generate.\n", base_item->GetName(), GetName() );
            continue;
        }
        id_map.insert( PAIR( base_item->GetId(), item->GetId() ) );

        // Other values
        if( item->IsDoor() && item->GetOpened() )
            item->SetIsLightThru( true );

        if( !AddItem( item, item->GetHexX(), item->GetHexY() ) )
        {
            WriteLogF( _FUNC_, " - Add item '%s' to map '%s' failure, continue generate.\n", item->GetName(), GetName() );
            ItemMngr.DeleteItem( item );
        }
    }

    // Add children items
    for( auto& base_item : GetProtoMap()->ChildItemsVec )
    {
        // Map id
        uint parent_id = 0;
        if( base_item->GetAccessory() == ITEM_ACCESSORY_CRITTER )
            parent_id = base_item->GetCritId();
        else if( base_item->GetAccessory() == ITEM_ACCESSORY_CONTAINER )
            parent_id = base_item->GetContainerId();
        else
            RUNTIME_ASSERT( !"Unreachable place" );

        if( !id_map.count( parent_id ) )
            continue;
        parent_id = id_map[ parent_id ];

        // Create item
        Item* item = ItemMngr.CreateItem( base_item->GetProtoId(), 0, &base_item->Props );
        if( !item )
        {
            WriteLogF( _FUNC_, " - Create item '%s' on map '%s' fail, continue generate.\n", base_item->GetName(), GetName() );
            continue;
        }

        // Add to parent
        if( base_item->GetAccessory() == ITEM_ACCESSORY_CRITTER )
        {
            Critter* cr_cont = GetCritter( parent_id, true );
            RUNTIME_ASSERT( cr_cont );
            cr_cont->AddItem( item, false );
        }
        else if( base_item->GetAccessory() == ITEM_ACCESSORY_CONTAINER )
        {
            Item* item_cont = GetItem( parent_id );
            RUNTIME_ASSERT( item_cont );
            item_cont->ContAddItem( item, 0 );
        }
        else
        {
            RUNTIME_ASSERT( !"Unreachable place" );
        }
    }

    // Npc initialization
    PcVec npcs;
    GetNpcs( npcs, true );

    // Generate internal bag
    for( auto& npc : npcs )
    {
        ItemVec& items = npc->GetInventory();
        if( !npc->GetBagId() && !items.empty() )
        {
            uint         bag_size = (uint) items.size();
            ScriptArray* item_pid = Script::CreateArray( "hash[]" );
            ScriptArray* item_count = Script::CreateArray( "uint[]" );

            for( uint i = 0; i < bag_size; i++ )
            {
                Item* item = items[ i ];
                hash  pid = item->GetProtoId();
                uint  count = item->GetCount();
                item_pid->InsertLast( &pid );
                item_count->InsertLast( &count );
            }

            npc->SetInternalBagItemPid( item_pid );
            npc->SetInternalBagItemCount( item_count );
            SAFEREL( item_pid );
            SAFEREL( item_count );
        }
    }

    // Visible
    for( auto& npc : npcs )
    {
        npc->ProcessVisibleCritters();
        npc->ProcessVisibleItems();
    }

    // Map script
    SetScript( nullptr, true );

    return true;
}

void Map::Process()
{
    uint tick = Timer::GameTick();
    ProcessLoop( 0, GetLoopTime1(), tick );
    ProcessLoop( 1, GetLoopTime2(), tick );
    ProcessLoop( 2, GetLoopTime3(), tick );
    ProcessLoop( 3, GetLoopTime4(), tick );
    ProcessLoop( 4, GetLoopTime5(), tick );
}

void Map::ProcessLoop( int index, uint time, uint tick )
{
    if( time && tick - LoopLastTick[ index ] >= time )
    {
        LoopLastTick[ index ] = tick;
        Script::RaiseInternalEvent( ServerFunctions.MapLoop, this, index + 1 );
    }
}

Location* Map::GetLocation( bool lock )
{
    if( lock )
        SYNC_LOCK( mapLocation );
    return mapLocation;
}

bool Map::GetStartCoord( ushort& hx, ushort& hy, uchar& dir, uint entire )
{
    ProtoMap::MapEntire* ent;
    ent = GetProtoMap()->GetEntireRandom( entire );

    if( !ent )
        return false;

    hx = ent->HexX;
    hy = ent->HexY;
    dir = ent->Dir;

    if( hx >= GetWidth() || hy >= GetHeight() )
        return false;
    if( dir >= DIRS_COUNT )
        dir = Random( 0, DIRS_COUNT - 1 );
    return true;
}

bool Map::GetStartCoordCar( ushort& hx, ushort& hy, Item* item )
{
    if( !item->IsCar() )
        return false;

    ProtoMap::EntiresVec entires;
    GetProtoMap()->GetEntires( item->GetCar_Entrance(), entires );
    std::random_shuffle( entires.begin(), entires.end() );

    for( int i = 0, j = (uint) entires.size(); i < j; i++ )
    {
        ProtoMap::MapEntire* ent = &entires[ i ];
        if( ent->HexX < GetWidth() && ent->HexY < GetHeight() && IsPlaceForItem( ent->HexX, ent->HexY, item ) )
        {
            hx = ent->HexX;
            hy = ent->HexY;
            return true;
        }
    }
    return false;
}

bool Map::FindStartHex( ushort& hx, ushort& hy, uint multihex, uint seek_radius, bool skip_unsafe )
{
    if( IsHexesPassed( hx, hy, multihex ) && !( skip_unsafe && ( IsHexTrigger( hx, hy ) || IsHexTrap( hx, hy ) ) ) )
        return true;
    if( !seek_radius )
        return false;
    if( seek_radius > MAX_HEX_OFFSET )
        seek_radius = MAX_HEX_OFFSET;

    static THREAD int cur_step = 0;
    short             hx_ = hx;
    short             hy_ = hy;
    short*            sx, * sy;
    GetHexOffsets( hx & 1, sx, sy );
    int               cnt = NumericalNumber( seek_radius ) * DIRS_COUNT;

    for( int i = 0; ; i++ )
    {
        if( i >= cnt )
            return false;

        cur_step++;
        if( cur_step >= cnt )
            cur_step = 0;
        short nx = hx_ + sx[ cur_step ];
        short ny = hy_ + sy[ cur_step ];

        if( nx < 0 || nx >= GetWidth() || ny < 0 || ny >= GetHeight() )
            continue;
        if( !IsHexesPassed( nx, ny, multihex ) )
            continue;
        if( skip_unsafe && ( IsHexTrigger( nx, ny ) || IsHexTrap( nx, ny ) ) )
            continue;
        break;
    }

    hx_ += sx[ cur_step ];
    hy_ += sy[ cur_step ];
    hx = hx_;
    hy = hy_;
    return true;
}

void Map::AddCritter( Critter* cr )
{
    // Add critter to collections
    {
        SCOPE_LOCK( dataLocker );

        if( std::find( mapCritters.begin(), mapCritters.end(), cr ) != mapCritters.end() )
        {
            WriteLogF( _FUNC_, " - Critter already added!\n" );
            return;
        }

        if( cr->IsPlayer() )
            mapPlayers.push_back( (Client*) cr );
        else
            mapNpcs.push_back( (Npc*) cr );
        mapCritters.push_back( cr );

        cr->SetMapId( GetId() );
        cr->SetMapPid( GetProtoId() );
    }

    SetFlagCritter( cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), cr->IsDead() );
    cr->SetTimeoutBattle( 0 );
}

void Map::EraseCritter( Critter* cr )
{
    // Erase critter from collections
    {
        SCOPE_LOCK( dataLocker );

        if( cr->IsPlayer() )
        {
            auto it = std::find( mapPlayers.begin(), mapPlayers.end(), (Client*) cr );
            RUNTIME_ASSERT( it != mapPlayers.end() );
            mapPlayers.erase( it );
        }
        else
        {
            auto it = std::find( mapNpcs.begin(), mapNpcs.end(), (Npc*) cr );
            RUNTIME_ASSERT( it != mapNpcs.end() );
            mapNpcs.erase( it );
        }

        auto it = std::find( mapCritters.begin(), mapCritters.end(), cr );
        RUNTIME_ASSERT( it != mapCritters.end() );
        mapCritters.erase( it );
    }

    cr->SetTimeoutBattle( 0 );

    MapMngr.RunGarbager();
}

void Map::KickPlayersToGlobalMap()
{
    ClVec players;
    GetPlayers( players, true );

    for( auto it = players.begin(), end = players.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        MapMngr.TransitToGlobal( cr, 0, true );
    }
}

bool Map::AddItem( Item* item, ushort hx, ushort hy )
{
    if( !item )
        return false;
    if( item->IsScenery() )
        return false;
    if( hx >= GetWidth() || hy >= GetHeight() )
        return false;

    SetItem( item, hx, hy );

    // Process critters view
    CrVec critters;
    GetCritters( critters, true );

    item->ViewPlaceOnMap = true;
    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !item->GetIsHidden() || item->GetIsAlwaysView() )
        {
            if( !item->GetIsAlwaysView() )           // Check distance for non-hide items
            {
                bool allowed = false;
                if( item->GetIsTrap() && FLAG( GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT ) )
                {
                    allowed = Script::RaiseInternalEvent( ServerFunctions.MapCheckTrapLook, this, cr, item );
                }
                else
                {
                    int dist = DistGame( cr->GetHexX(), cr->GetHexY(), hx, hy );
                    if( item->GetIsTrap() )
                        dist += item->GetTrapValue();
                    allowed = ( dist <= (int) cr->GetLookDistance() );
                }
                if( !allowed )
                    continue;
            }

            cr->AddIdVisItem( item->GetId() );
            cr->Send_AddItemOnMap( item );
            Script::RaiseInternalEvent( ServerFunctions.CritterShowItemOnMap, cr, item, false, nullptr );
        }
    }
    item->ViewPlaceOnMap = false;

    return true;
}

void Map::SetItem( Item* item, ushort hx, ushort hy )
{
    if( GetItem( item->GetId() ) )
    {
        WriteLogF( _FUNC_, " - Item already added!\n" );
        return;
    }

    item->SetAccessory( ITEM_ACCESSORY_HEX );
    item->SetMapId( Id );
    item->SetHexX( hx );
    item->SetHexY( hy );

    hexItems.push_back( item );
    SetHexFlag( hx, hy, FH_ITEM );

    if( !item->GetIsNoBlock() )
        SetHexFlag( hx, hy, FH_BLOCK_ITEM );
    if( !item->GetIsShootThru() )
        SetHexFlag( hx, hy, FH_NRAKE_ITEM );
    if( item->GetIsGag() )
        SetHexFlag( hx, hy, FH_GAG_ITEM );
    if( item->IsNonEmptyBlockLines() )
        PlaceItemBlocks( hx, hy, item );

    if( item->GetIsTrap() )
        SetHexFlag( hx, hy, FH_WALK_ITEM );
    if( item->GetIsGeck() )
        mapLocation->GeckCount++;
}

void Map::EraseItem( uint item_id )
{
    RUNTIME_ASSERT( item_id );

    auto it = hexItems.begin();
    auto end = hexItems.end();
    for( ; it != end; ++it )
        if( ( *it )->GetId() == item_id )
            break;
    if( it == hexItems.end() )
        return;

    Item* item = *it;
    hexItems.erase( it );

    ushort hx = item->GetHexX();
    ushort hy = item->GetHexY();

    item->SetAccessory( ITEM_ACCESSORY_NONE );
    item->SetMapId( 0 );
    item->SetHexX( 0 );
    item->SetHexY( 0 );

    if( item->GetIsGeck() )
        mapLocation->GeckCount--;
    if( !item->GetIsNoBlock() && !item->GetIsShootThru() )
        RecacheHexBlockShoot( hx, hy );
    else if( !item->GetIsNoBlock() )
        RecacheHexBlock( hx, hy );
    else if( !item->GetIsShootThru() )
        RecacheHexShoot( hx, hy );
    if( item->IsNonEmptyBlockLines() )
        ReplaceItemBlocks( hx, hy, item );

    // Process critters view
    CrVec critters;
    GetCritters( critters, true );

    item->ViewPlaceOnMap = true;
    for( auto it_ = critters.begin(), end_ = critters.end(); it_ != end_; ++it_ )
    {
        Critter* cr = *it_;

        if( cr->DelIdVisItem( item->GetId() ) )
        {
            cr->Send_EraseItemFromMap( item );
            Script::RaiseInternalEvent( ServerFunctions.CritterHideItemOnMap, cr, item, item->ViewPlaceOnMap, item->ViewByCritter );
        }
    }
    item->ViewPlaceOnMap = false;
}

void Map::SendProperty( NetProperty::Type type, Property* prop, Entity* entity )
{
    if( type == NetProperty::MapItem )
    {
        Item* item = (Item*) entity;
        CrVec critters;
        GetCritters( critters, true );
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr->CountIdVisItem( item->GetId() ) )
            {
                cr->Send_Property( type, prop, entity );
                Script::RaiseInternalEvent( ServerFunctions.CritterChangeItemOnMap, cr, item );
            }
        }
    }
    else if( type == NetProperty::Map || type == NetProperty::Location )
    {
        CrVec critters;
        GetCritters( critters, false );
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            cr->Send_Property( type, prop, entity );
            if( type == NetProperty::Map && ( prop == Map::PropertyDayTime || prop == Map::PropertyDayColor ) )
                cr->Send_GameInfo( nullptr );
        }
    }
    else
    {
        RUNTIME_ASSERT( false );
    }
}

void Map::ChangeViewItem( Item* item )
{
    CrVec critters;
    GetCritters( critters, true );

    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = *it;

        if( cr->CountIdVisItem( item->GetId() ) )
        {
            if( item->GetIsHidden() )
            {
                cr->DelIdVisItem( item->GetId() );
                cr->Send_EraseItemFromMap( item );
                Script::RaiseInternalEvent( ServerFunctions.CritterHideItemOnMap, cr, item, false, nullptr );
            }
            else if( !item->GetIsAlwaysView() )           // Check distance for non-hide items
            {
                bool allowed = false;
                if( item->GetIsTrap() && FLAG( GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT ) )
                {
                    allowed = Script::RaiseInternalEvent( ServerFunctions.MapCheckTrapLook, this, cr, item );
                }
                else
                {
                    int dist = DistGame( cr->GetHexX(), cr->GetHexY(), item->GetHexX(), item->GetHexY() );
                    if( item->GetIsTrap() )
                        dist += item->GetTrapValue();
                    allowed = ( dist <= (int) cr->GetLookDistance() );
                }
                if( !allowed )
                {
                    cr->DelIdVisItem( item->GetId() );
                    cr->Send_EraseItemFromMap( item );
                    Script::RaiseInternalEvent( ServerFunctions.CritterHideItemOnMap, cr, item, false, nullptr );
                }
            }
        }
        else if( !item->GetIsHidden() || item->GetIsAlwaysView() )
        {
            if( !item->GetIsAlwaysView() )           // Check distance for non-hide items
            {
                bool allowed = false;
                if( item->GetIsTrap() && FLAG( GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT ) )
                {
                    allowed = Script::RaiseInternalEvent( ServerFunctions.MapCheckTrapLook, this, cr, item );
                }
                else
                {
                    int dist = DistGame( cr->GetHexX(), cr->GetHexY(), item->GetHexX(), item->GetHexY() );
                    if( item->GetIsTrap() )
                        dist += item->GetTrapValue();
                    allowed = ( dist <= (int) cr->GetLookDistance() );
                }
                if( !allowed )
                    continue;
            }

            cr->AddIdVisItem( item->GetId() );
            cr->Send_AddItemOnMap( item );
            Script::RaiseInternalEvent( ServerFunctions.CritterShowItemOnMap, cr, item, false, nullptr );
        }
    }
}

void Map::AnimateItem( Item* item, uchar from_frm, uchar to_frm )
{
    SCOPE_LOCK( dataLocker );

    for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
    {
        Client* cl = *it;
        if( cl->CountIdVisItem( item->GetId() ) )
            cl->Send_AnimateItem( item, from_frm, to_frm );
    }
}

#pragma MESSAGE("Add explicit sync lock.")
Item* Map::GetItem( uint item_id )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetId() == item_id )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

Item* Map::GetItemHex( ushort hx, ushort hy, hash item_pid, Critter* picker )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetHexX() == hx && item->GetHexY() == hy && ( item_pid == 0 || item->GetProtoId() == item_pid ) &&
            ( !picker || ( !item->GetIsHidden() && picker->CountIdVisItem( item->GetId() ) ) ) )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

Item* Map::GetItemDoor( ushort hx, ushort hy )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetHexX() == hx && item->GetHexY() == hy && item->IsDoor() )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

Item* Map::GetItemCar( ushort hx, ushort hy )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetHexX() == hx && item->GetHexY() == hy && item->IsCar() )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

Item* Map::GetItemContainer( ushort hx, ushort hy )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetHexX() == hx && item->GetHexY() == hy && item->IsContainer() )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

Item* Map::GetItemGag( ushort hx, ushort hy )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetHexX() == hx && item->GetHexY() == hy && item->GetIsGag() )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

void Map::GetItemsHex( ushort hx, ushort hy, ItemVec& items, bool lock )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetHexX() == hx && item->GetHexY() == hy )
            items.push_back( item );
    }

    if( lock )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

void Map::GetItemsHexEx( ushort hx, ushort hy, uint radius, hash pid, ItemVec& items, bool lock )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( ( !pid || item->GetProtoId() == pid ) && DistGame( item->GetHexX(), item->GetHexY(), hx, hy ) <= radius )
            items.push_back( item );
    }

    if( lock )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

void Map::GetItemsPid( hash pid, ItemVec& items, bool lock )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( !pid || item->GetProtoId() == pid )
            items.push_back( item );
    }

    if( lock )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

void Map::GetItemsType( int type, ItemVec& items, bool lock )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetType() == type )
            items.push_back( item );
    }

    if( lock )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

void Map::GetItemsTrap( ushort hx, ushort hy, ItemVec& traps, bool lock )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetHexX() == hx && item->GetHexY() == hy && item->GetIsTrap() )
            traps.push_back( item );
    }
    if( !traps.size() )
        UnsetHexFlag( hx, hy, FH_WALK_ITEM );

    if( lock )
        for( auto it = traps.begin(), end = traps.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

void Map::RecacheHexBlock( ushort hx, ushort hy )
{
    UnsetHexFlag( hx, hy, FH_BLOCK_ITEM );
    UnsetHexFlag( hx, hy, FH_GAG_ITEM );
    bool is_block = false;
    bool is_gag = false;
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetHexX() == hx && item->GetHexY() == hy )
        {
            if( !is_block && !item->GetIsNoBlock() )
                is_block = true;
            if( !is_gag && item->GetIsGag() )
                is_gag = true;
            if( is_block && is_gag )
                break;
        }
    }
    if( is_block )
        SetHexFlag( hx, hy, FH_BLOCK_ITEM );
    if( is_gag )
        SetHexFlag( hx, hy, FH_GAG_ITEM );
}

void Map::RecacheHexShoot( ushort hx, ushort hy )
{
    UnsetHexFlag( hx, hy, FH_NRAKE_ITEM );
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetHexX() == hx && item->GetHexY() == hy && !item->GetIsShootThru() )
        {
            SetHexFlag( hx, hy, FH_NRAKE_ITEM );
            break;
        }
    }
}

void Map::RecacheHexBlockShoot( ushort hx, ushort hy )
{
    UnsetHexFlag( hx, hy, FH_BLOCK_ITEM );
    UnsetHexFlag( hx, hy, FH_NRAKE_ITEM );
    UnsetHexFlag( hx, hy, FH_GAG_ITEM );
    bool is_block = false;
    bool is_nrake = false;
    bool is_gag = false;
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetHexX() == hx && item->GetHexY() == hy )
        {
            if( !is_block && !item->GetIsNoBlock() )
                is_block = true;
            if( !is_nrake && !item->GetIsShootThru() )
                is_nrake = true;
            if( !is_gag && item->GetIsGag() )
                is_gag = true;
            if( is_block && is_nrake && is_gag )
                break;
        }
    }
    if( is_block )
        SetHexFlag( hx, hy, FH_BLOCK_ITEM );
    if( is_nrake )
        SetHexFlag( hx, hy, FH_NRAKE_ITEM );
    if( is_gag )
        SetHexFlag( hx, hy, FH_GAG_ITEM );
}

ushort Map::GetHexFlags( ushort hx, ushort hy )
{
    return ( hexFlags[ hy * GetWidth() + hx ] << 8 ) | GetProtoMap()->HexFlags[ hy * GetWidth() + hx ];
}

void Map::SetHexFlag( ushort hx, ushort hy, uchar flag )
{
    SETFLAG( hexFlags[ hy * GetWidth() + hx ], flag );
}

void Map::UnsetHexFlag( ushort hx, ushort hy, uchar flag )
{
    UNSETFLAG( hexFlags[ hy * GetWidth() + hx ], flag );
}

bool Map::IsHexPassed( ushort hx, ushort hy )
{
    return !FLAG( GetHexFlags( hx, hy ), FH_NOWAY );
}

bool Map::IsHexRaked( ushort hx, ushort hy )
{
    return !FLAG( GetHexFlags( hx, hy ), FH_NOSHOOT );
}

bool Map::IsHexesPassed( ushort hx, ushort hy, uint radius )
{
    // Base
    if( FLAG( GetHexFlags( hx, hy ), FH_NOWAY ) )
        return false;
    if( !radius )
        return true;

    // Neighbors
    short* sx, * sy;
    GetHexOffsets( hx & 1, sx, sy );
    uint   count = NumericalNumber( radius ) * DIRS_COUNT;
    short  maxhx = GetWidth();
    short  maxhy = GetHeight();
    for( uint i = 0; i < count; i++ )
    {
        short hx_ = (short) hx + sx[ i ];
        short hy_ = (short) hy + sy[ i ];
        if( hx_ >= 0 && hy_ >= 0 && hx_ < maxhx && hy_ < maxhy && FLAG( GetHexFlags( hx_, hy_ ), FH_NOWAY ) )
            return false;
    }
    return true;
}

bool Map::IsMovePassed( ushort hx, ushort hy, uchar dir, uint multihex )
{
    // Single hex
    if( !multihex )
        return IsHexPassed( hx, hy );

    // Multihex
    // Base hex
    int hx_ = hx, hy_ = hy;
    for( uint k = 0; k < multihex; k++ )
        MoveHexByDirUnsafe( hx_, hy_, dir );
    if( hx_ < 0 || hy_ < 0 || hx_ >= GetWidth() || hy_ >= GetHeight() )
        return false;
    if( !IsHexPassed( hx_, hy_ ) )
        return false;

    // Clock wise hexes
    bool is_square_corner = ( !GameOpt.MapHexagonal && IS_DIR_CORNER( dir ) );
    uint steps_count = ( is_square_corner ? multihex * 2 : multihex );
    int  dir_ = ( GameOpt.MapHexagonal ? ( ( dir + 2 ) % 6 ) : ( ( dir + 2 ) % 8 ) );
    if( is_square_corner )
        dir_ = ( dir_ + 1 ) % 8;
    int hx__ = hx_, hy__ = hy_;
    for( uint k = 0; k < steps_count; k++ )
    {
        MoveHexByDirUnsafe( hx__, hy__, dir_ );
        if( !IsHexPassed( hx__, hy__ ) )
            return false;
    }

    // Counter clock wise hexes
    dir_ = ( GameOpt.MapHexagonal ? ( ( dir + 4 ) % 6 ) : ( ( dir + 6 ) % 8 ) );
    if( is_square_corner )
        dir_ = ( dir_ + 7 ) % 8;
    hx__ = hx_, hy__ = hy_;
    for( uint k = 0; k < steps_count; k++ )
    {
        MoveHexByDirUnsafe( hx__, hy__, dir_ );
        if( !IsHexPassed( hx__, hy__ ) )
            return false;
    }
    return true;
}

bool Map::IsFlagCritter( ushort hx, ushort hy, bool dead )
{
    if( dead )
        return FLAG( hexFlags[ hy * GetWidth() + hx ], FH_DEAD_CRITTER );
    else
        return FLAG( hexFlags[ hy * GetWidth() + hx ], FH_CRITTER );
}

void Map::SetFlagCritter( ushort hx, ushort hy, uint multihex, bool dead )
{
    if( dead )
    {
        SetHexFlag( hx, hy, FH_DEAD_CRITTER );
    }
    else
    {
        SetHexFlag( hx, hy, FH_CRITTER );

        if( multihex )
        {
            short* sx, * sy;
            GetHexOffsets( hx & 1, sx, sy );
            int    count = NumericalNumber( multihex ) * DIRS_COUNT;
            short  maxhx = GetWidth();
            short  maxhy = GetHeight();
            for( int i = 0; i < count; i++ )
            {
                short hx_ = (short) hx + sx[ i ];
                short hy_ = (short) hy + sy[ i ];
                if( hx_ >= 0 && hy_ >= 0 && hx_ < maxhx && hy_ < maxhy )
                    SetHexFlag( hx_, hy_, FH_CRITTER );
            }
        }
    }
}

void Map::UnsetFlagCritter( ushort hx, ushort hy, uint multihex, bool dead )
{
    if( dead )
    {
        uint dead_count = 0;

        dataLocker.Lock();
        for( auto it = mapCritters.begin(), end = mapCritters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr->GetHexX() == hx && cr->GetHexY() == hy && cr->IsDead() )
                dead_count++;
        }
        dataLocker.Unlock();

        if( dead_count <= 1 )
            UnsetHexFlag( hx, hy, FH_DEAD_CRITTER );
    }
    else
    {
        UnsetHexFlag( hx, hy, FH_CRITTER );

        if( multihex > 0 )
        {
            short* sx, * sy;
            GetHexOffsets( hx & 1, sx, sy );
            int    count = NumericalNumber( multihex ) * DIRS_COUNT;
            short  maxhx = GetWidth();
            short  maxhy = GetHeight();
            for( int i = 0; i < count; i++ )
            {
                short hx_ = (short) hx + sx[ i ];
                short hy_ = (short) hy + sy[ i ];
                if( hx_ >= 0 && hy_ >= 0 && hx_ < maxhx && hy_ < maxhy )
                    UnsetHexFlag( hx_, hy_, FH_CRITTER );
            }
        }
    }
}

uint Map::GetNpcCount( int npc_role, int find_type )
{
    PcVec npcs;
    GetNpcs( npcs, true );

    uint result = 0;
    for( auto it = npcs.begin(), end = npcs.end(); it != end; ++it )
    {
        Npc* npc = *it;
        if( npc->GetNpcRole() == npc_role && npc->CheckFind( find_type ) )
            result++;
    }
    return result;
}

Critter* Map::GetCritter( uint crid, bool sync_lock )
{
    Critter* cr = nullptr;

    dataLocker.Lock();
    for( auto it = mapCritters.begin(), end = mapCritters.end(); it != end; ++it )
    {
        Critter* cr_ = *it;
        if( cr_->GetId() == crid )
        {
            cr = cr_;
            break;
        }
    }
    dataLocker.Unlock();

    if( cr && sync_lock && LogicMT )
    {
        // Synchronize
        SYNC_LOCK( cr );

        // Recheck
        if( cr->GetMapId() != GetId() )
            return nullptr;
    }

    return cr;
}

Critter* Map::GetNpc( int npc_role, int find_type, uint skip_count, bool sync_lock )
{
    Npc* npc = nullptr;

    dataLocker.Lock();
    for( auto it = mapNpcs.begin(), end = mapNpcs.end(); it != end; ++it )
    {
        Npc* npc_ = *it;
        if( npc_->GetNpcRole() == npc_role && npc_->CheckFind( find_type ) )
        {
            if( skip_count )
                skip_count--;
            else
            {
                npc = npc_;
                break;
            }
        }
    }
    dataLocker.Unlock();

    if( npc && sync_lock && LogicMT )
    {
        // Synchronize
        SYNC_LOCK( npc );

        // Recheck
        if( npc->GetMapId() != GetId() || npc->GetNpcRole() != npc_role || !npc->CheckFind( find_type ) )
            return GetNpc( npc_role, find_type, skip_count, sync_lock );
    }

    return npc;
}

Critter* Map::GetHexCritter( ushort hx, ushort hy, bool dead, bool sync_lock )
{
    if( !IsFlagCritter( hx, hy, dead ) )
        return nullptr;

    Critter* cr = nullptr;

    dataLocker.Lock();
    for( auto it = mapCritters.begin(), end = mapCritters.end(); it != end; ++it )
    {
        Critter* cr_ = *it;
        if( cr_->IsDead() == dead )
        {
            int mh = cr_->GetMultihex();
            if( !mh )
            {
                if( cr_->GetHexX() == hx && cr_->GetHexY() == hy )
                {
                    cr = cr_;
                    break;
                }
            }
            else
            {
                if( CheckDist( cr_->GetHexX(), cr_->GetHexY(), hx, hy, mh ) )
                {
                    cr = cr_;
                    break;
                }
            }
        }
    }
    dataLocker.Unlock();

    if( cr && sync_lock && LogicMT )
    {
        SYNC_LOCK( cr );
        if( cr->GetMapId() != GetId() || !CheckDist( cr->GetHexX(), cr->GetHexY(), hx, hy, cr->GetMultihex() ) )
            return GetHexCritter( hx, hy, dead, sync_lock );
    }

    return cr;
}

void Map::GetCrittersHex( ushort hx, ushort hy, uint radius, int find_type, CrVec& critters, bool sync_lock )
{
    dataLocker.Lock();
    CrVec find_critters;
    find_critters.reserve( mapCritters.size() );
    for( auto it = mapCritters.begin(), end = mapCritters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->CheckFind( find_type ) && CheckDist( hx, hy, cr->GetHexX(), cr->GetHexY(), radius + cr->GetMultihex() ) )
            find_critters.push_back( cr );
    }
    dataLocker.Unlock();

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck
        dataLocker.Lock();
        CrVec find_critters2;
        find_critters2.reserve( find_critters.size() );
        for( auto it = mapCritters.begin(), end = mapCritters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr->CheckFind( find_type ) && CheckDist( hx, hy, cr->GetHexX(), cr->GetHexY(), radius + cr->GetMultihex() ) )
                find_critters2.push_back( cr );
        }
        dataLocker.Unlock();

        // Search again
        if( !CompareContainers( find_critters, find_critters2 ) )
        {
            GetCrittersHex( hx, hy, radius, find_type, critters, sync_lock );
            return;
        }
    }

    // Store result, append
    if( find_critters.size() )
    {
        critters.reserve( critters.size() + find_critters.size() );
        critters.insert( critters.end(), find_critters.begin(), find_critters.end() );
    }
}

void Map::GetCritters( CrVec& critters, bool sync_lock )
{
    dataLocker.Lock();
    critters = mapCritters;
    dataLocker.Unlock();

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck after synchronization
        dataLocker.Lock();
        bool equal = CompareContainers( critters, mapCritters );
        dataLocker.Unlock();
        if( !equal )
        {
            critters.clear();
            GetCritters( critters, sync_lock );
        }
    }
}

void Map::GetPlayers( ClVec& players, bool sync_lock )
{
    dataLocker.Lock();
    players = mapPlayers;
    dataLocker.Unlock();

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = players.begin(), end = players.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck after synchronization
        dataLocker.Lock();
        bool equal = CompareContainers( players, mapPlayers );
        dataLocker.Unlock();
        if( !equal )
        {
            players.clear();
            GetPlayers( players, sync_lock );
        }
    }
}

void Map::GetNpcs( PcVec& npcs, bool sync_lock )
{
    dataLocker.Lock();
    npcs = mapNpcs;
    dataLocker.Unlock();

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = npcs.begin(), end = npcs.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck after synchronization
        dataLocker.Lock();
        bool equal = CompareContainers( npcs, mapNpcs );
        dataLocker.Unlock();
        if( !equal )
        {
            npcs.clear();
            GetNpcs( npcs, sync_lock );
        }
    }
}

uint Map::GetCrittersCount()
{
    SCOPE_LOCK( dataLocker );
    uint count = (uint) mapCritters.size();
    return count;
}

uint Map::GetPlayersCount()
{
    SCOPE_LOCK( dataLocker );
    uint count = (uint) mapPlayers.size();
    return count;
}

uint Map::GetNpcsCount()
{
    SCOPE_LOCK( dataLocker );
    uint count = (uint) mapNpcs.size();
    return count;
}

void Map::SendEffect( hash eff_pid, ushort hx, ushort hy, ushort radius )
{
    SCOPE_LOCK( dataLocker );

    for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
    {
        Client* cl = *it;
        if( CheckDist( cl->GetHexX(), cl->GetHexY(), hx, hy, cl->LookCacheValue + radius ) )
            cl->Send_Effect( eff_pid, hx, hy, radius );
    }
}

void Map::SendFlyEffect( hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy )
{
    SCOPE_LOCK( dataLocker );

    for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
    {
        Client* cl = *it;
        if( IntersectCircleLine( cl->GetHexX(), cl->GetHexY(), cl->LookCacheValue, from_hx, from_hy, to_hx, to_hy ) )
            cl->Send_FlyEffect( eff_pid, from_crid, to_crid, from_hx, from_hy, to_hx, to_hy );
    }
}

void Map::GetCritterCar( Critter* cr, Item* car )
{
    // Check
    if( !cr || !car || !car->IsCar() )
        return;

    // Car position
    ushort hx = car->GetHexX();
    ushort hy = car->GetHexY();

    // Move car from map to inventory
    EraseItem( car->GetId() );
    car->SetIsHidden( true );
    cr->AddItem( car, false );

    // Move car bags from map to inventory
    for( int i = 0; i < ITEM_MAX_CHILDS; i++ )
    {
        Item* child = GetItemChild( hx, hy, car, i );
        if( !child )
            continue;

        EraseItem( child->GetId() );
        child->SetIsHidden( true );
        cr->AddItem( child, false );
    }
}

void Map::SetCritterCar( ushort hx, ushort hy, Critter* cr, Item* car )
{
    // Check
    if( hx >= GetWidth() || hy >= GetHeight() || !cr || !car || !car->IsCar() )
    {
        WriteLogF( _FUNC_, " - Generic error, hx %u, hy %u, critter pointer '%p', car pointer '%p', is car %d.\n", hx, hy, cr, car, car && car->IsCar() ? 1 : 0 );
        return;
    }

    // Move car from inventory to map
    cr->EraseItem( car, false );
    car->SetIsHidden( false );
    AddItem( car, hx, hy );

    // Move car bags from inventory to map
    for( int i = 0; i < ITEM_MAX_CHILDS; i++ )
    {
        hash child_pid = car->GetChildPid( i );
        if( !child_pid )
            continue;

        Item* child = cr->GetItemByPid( child_pid );
        if( !child )
            continue;

        // Move to position
        ushort child_hx = hx;
        ushort child_hy = hy;
        FOREACH_PROTO_ITEM_LINES( car->GetChildLinesStr( i ), child_hx, child_hy, GetWidth(), GetHeight() );
        cr->EraseItem( child, false );
        child->SetIsHidden( false );
        AddItem( child, child_hx, child_hy );
    }
}

bool Map::IsPlaceForItem( ushort hx, ushort hy, Item* item )
{
    if( !IsHexPassed( hx, hy ) )
        return false;

    FOREACH_PROTO_ITEM_LINES_WORK( item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(),
                                   if( IsHexCritter( hx, hy ) )
                                       return false;
                                   // if( !IsHexPassed( hx, hy ) )
                                   //    return false;
                                   );

    return true;
}

bool Map::IsPlaceForProtoItem( ushort hx, ushort hy, ProtoItem* proto_item )
{
    if( !IsHexPassed( hx, hy ) )
        return false;

    FOREACH_PROTO_ITEM_LINES_WORK( proto_item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(),
                                   if( IsHexCritter( hx, hy ) )
                                       return false;
                                   // if( !IsHexPassed( hx, hy ) )
                                   //    return false;
                                   );
    return true;
}

void Map::PlaceItemBlocks( ushort hx, ushort hy, Item* item )
{
    bool raked = item->GetIsShootThru();
    FOREACH_PROTO_ITEM_LINES_WORK( item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(),
                                   SetHexFlag( hx, hy, FH_BLOCK_ITEM );
                                   if( !raked )
                                       SetHexFlag( hx, hy, FH_NRAKE_ITEM );
                                   );
}

void Map::ReplaceItemBlocks( ushort hx, ushort hy, Item* item )
{
    bool raked = item->GetIsShootThru();
    FOREACH_PROTO_ITEM_LINES_WORK( item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(),
                                   UnsetHexFlag( hx, hy, FH_BLOCK_ITEM );
                                   if( !raked )
                                   {
                                       UnsetHexFlag( hx, hy, FH_NRAKE_ITEM );
                                       if( IsHexItem( hx, hy ) )
                                           RecacheHexBlockShoot( hx, hy );
                                   }
                                   else
                                   {
                                       if( IsHexItem( hx, hy ) )
                                           RecacheHexBlock( hx, hy );
                                   }
                                   );
}

Item* Map::GetItemChild( ushort hx, ushort hy, Item* item, uint child_index )
{
    // Get child pid
    hash child_pid = item->GetChildPid( child_index );
    if( !child_pid )
        return nullptr;

    // Move to position
    FOREACH_PROTO_ITEM_LINES( item->GetChildLinesStr( child_index ), hx, hy, GetWidth(), GetHeight() );

    // Find on map
    return GetItemHex( hx, hy, child_pid, nullptr );
}

bool Map::SetScript( asIScriptFunction* func, bool first_time )
{
    if( func )
    {
        hash func_num = Script::BindScriptFuncNumByFunc( func );
        if( !func_num )
        {
            WriteLogF( _FUNC_, " - Script bind fail, map '%s'.\n", GetName() );
            return false;
        }
        SetScriptId( func_num );
    }

    if( GetScriptId() )
    {
        Script::PrepareScriptFuncContext( GetScriptId(), Str::FormatBuf( "Map '%s' (%u)", GetName(), GetId() ) );
        Script::SetArgEntity( this );
        Script::SetArgBool( first_time );
        Script::RunPrepared();
    }
    return true;
}

void Map::SetText( ushort hx, ushort hy, uint color, const char* text, ushort text_len, bool unsafe_text )
{
    if( hx >= GetWidth() || hy >= GetHeight() )
        return;

    SCOPE_LOCK( dataLocker );

    for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
    {
        Client* cl = *it;
        if( cl->LookCacheValue >= DistGame( hx, hy, cl->GetHexX(), cl->GetHexY() ) )
            cl->Send_MapText( hx, hy, color, text, text_len, unsafe_text );
    }
}

void Map::SetTextMsg( ushort hx, ushort hy, uint color, ushort text_msg, uint num_str )
{
    if( hx >= GetWidth() || hy >= GetHeight() || !num_str )
        return;

    SCOPE_LOCK( dataLocker );

    for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
    {
        Client* cl = *it;
        if( cl->LookCacheValue >= DistGame( hx, hy, cl->GetHexX(), cl->GetHexY() ) )
            cl->Send_MapTextMsg( hx, hy, color, text_msg, num_str );
    }
}

void Map::SetTextMsgLex( ushort hx, ushort hy, uint color, ushort text_msg, uint num_str, const char* lexems, ushort lexems_len )
{
    if( hx >= GetWidth() || hy >= GetHeight() || !num_str )
        return;

    SCOPE_LOCK( dataLocker );

    for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
    {
        Client* cl = *it;
        if( cl->LookCacheValue >= DistGame( hx, hy, cl->GetHexX(), cl->GetHexY() ) )
            cl->Send_MapTextMsgLex( hx, hy, color, text_msg, num_str, lexems, lexems_len );
    }
}

/************************************************************************/
/* Location                                                             */
/************************************************************************/

PROPERTIES_IMPL( Location );
CLASS_PROPERTY_IMPL( Location, MapProtos );
CLASS_PROPERTY_IMPL( Location, MapEntrances );
CLASS_PROPERTY_IMPL( Location, Automaps );
CLASS_PROPERTY_IMPL( Location, MaxPlayers );
CLASS_PROPERTY_IMPL( Location, AutoGarbage );
CLASS_PROPERTY_IMPL( Location, GeckVisible );
CLASS_PROPERTY_IMPL( Location, EntranceScript );
CLASS_PROPERTY_IMPL( Location, WorldX );
CLASS_PROPERTY_IMPL( Location, WorldY );
CLASS_PROPERTY_IMPL( Location, Radius );
CLASS_PROPERTY_IMPL( Location, Hidden );
CLASS_PROPERTY_IMPL( Location, ToGarbage );
CLASS_PROPERTY_IMPL( Location, Color );

Location::Location( uint id, ProtoLocation* proto ): Entity( id, EntityType::Location, PropertiesRegistrator, proto )
{
    RUNTIME_ASSERT( proto );
    GeckCount = 0;
}

Location::~Location()
{
    //
}

void Location::BindScript()
{
    EntranceScriptBindId = 0;
    if( GetEntranceScript() )
    {
        const char* func_name = Str::GetName( GetEntranceScript() );
        EntranceScriptBindId = Script::BindByFuncName( func_name, "bool %s(Location&, Critter@[]&, uint8 entranceIndex)", false );
    }
}

void Location::GetMaps( MapVec& maps, bool lock )
{
    maps = locMaps;
    if( lock )
        for( auto& map : maps )
            SYNC_LOCK( map );
}

Map* Location::GetMapByIndex( uint index )
{
    if( index >= locMaps.size() )
        return nullptr;
    Map* map = locMaps[ index ];
    SYNC_LOCK( map );
    return map;
}

Map* Location::GetMapByPid( hash map_pid )
{
    for( auto& map : locMaps )
    {
        if( map->GetProtoId() == map_pid )
        {
            SYNC_LOCK( map );
            return map;
        }
    }
    return nullptr;
}

uint Location::GetMapIndex( hash map_pid )
{
    uint index = 0;
    for( auto& map : locMaps )
    {
        if( map->GetProtoId() == map_pid )
            return index;
        index++;
    }
    return uint( -1 );
}

bool Location::GetTransit( Map* from_map, uint& id_map, ushort& hx, ushort& hy, uchar& dir )
{
    if( !from_map || hx >= from_map->GetWidth() || hy >= from_map->GetHeight() )
        return false;

    Item* grid = from_map->GetProtoMap()->GetMapGrid( hx, hy );
    if( !grid )
        return false;

    hash grid_to_map = grid->GetGrid_ToMap();
    if( grid_to_map == 0 )
    {
        id_map = 0;
        return true;
    }

    Map* to_map = nullptr;
    for( auto& map : locMaps )
    {
        if( map->GetProtoId() == grid_to_map )
        {
            to_map = map;
            break;
        }
    }

    if( !to_map )
        return false;
    SYNC_LOCK( to_map );

    id_map = to_map->GetId();

    hash                 grid_to_map_entire = grid->GetGrid_ToMapEntire();
    ProtoMap::MapEntire* ent = to_map->GetProtoMap()->GetEntire( grid_to_map_entire, 0 );
    if( !ent )
        return false;

    hx = ent->HexX;
    hy = ent->HexY;
    dir = ent->Dir;

    if( dir >= DIRS_COUNT )
        dir = Random( 0, DIRS_COUNT - 1 );
    if( hx >= to_map->GetWidth() || hy >= to_map->GetHeight() )
        return false;
    return true;
}

bool Location::IsCanEnter( uint players_count )
{
    uint max_palyers = GetMaxPlayers();
    if( !max_palyers )
        return true;

    for( auto& map : locMaps )
    {
        players_count += map->GetPlayersCount();
        if( players_count >= max_palyers )
            return false;
    }
    return true;
}

bool Location::IsNoCrit()
{
    for( auto& map : locMaps )
        if( map->GetCrittersCount() )
            return false;
    return true;
}

bool Location::IsNoPlayer()
{
    for( auto& map : locMaps )
        if( map->GetPlayersCount() )
            return false;
    return true;
}

bool Location::IsNoNpc()
{
    for( auto& map : locMaps )
        if( map->GetNpcsCount() )
            return false;
    return true;
}

bool Location::IsCanDelete()
{
    if( GeckCount > 0 )
        return false;

    // Check for players
    for( auto& map : locMaps )
        if( map->GetPlayersCount() )
            return false;

    // Check for npc
    MapVec maps = locMaps;
    for( auto& map : maps )
    {
        PcVec npcs;
        map->GetNpcs( npcs, true );

        for( auto it_ = npcs.begin(), end_ = npcs.end(); it_ != end_; ++it_ )
        {
            Npc* npc = *it_;
            if( npc->GetIsGeck() || ( !npc->GetIsNoHome() && npc->GetHomeMapId() != map->GetId() ) || npc->IsHaveGeckItem() )
                return false;
        }
    }
    return true;
}
