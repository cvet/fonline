#include "Common.h"
#include "Map.h"
#include "Script.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "MapManager.h"

const char* MapEventFuncName[ MAP_EVENT_MAX ] =
{
    "void %s(Map&,bool)",              // MAP_EVENT_FINISH
    "void %s(Map&)",                   // MAP_EVENT_LOOP_0
    "void %s(Map&)",                   // MAP_EVENT_LOOP_1
    "void %s(Map&)",                   // MAP_EVENT_LOOP_2
    "void %s(Map&)",                   // MAP_EVENT_LOOP_3
    "void %s(Map&)",                   // MAP_EVENT_LOOP_4
    "void %s(Map&,Critter&)",          // MAP_EVENT_IN_CRITTER
    "void %s(Map&,Critter&)",          // MAP_EVENT_OUT_CRITTER
    "void %s(Map&,Critter&,Critter@)", // MAP_EVENT_CRITTER_DEAD
    "void %s(Map&)",                   // MAP_EVENT_TURN_BASED_BEGIN
    "void %s(Map&)",                   // MAP_EVENT_TURN_BASED_END
    "void %s(Map&,Critter&,bool)",     // MAP_EVENT_TURN_BASED_PROCESS
};

/************************************************************************/
/* Map                                                                  */
/************************************************************************/

PROPERTIES_IMPL( Map );

Map::Map( uint id, ProtoMap* proto, Location* location ): Entity( id, EntityType::Map, PropertiesRegistrator )
{
    RUNTIME_ASSERT( proto );
    RUNTIME_ASSERT( location );

    MEMORY_PROCESS( MEMORY_MAP, sizeof( Map ) );
    MEMORY_PROCESS( MEMORY_MAP_FIELD, proto->Header.MaxHexX * proto->Header.MaxHexY );

    Proto = proto;
    mapLocation = location;

    hexFlags = NULL;
    NeedProcess = false;
    IsTurnBasedOn = false;
    TurnBasedEndTick = 0;
    TurnSequenceCur = 0;
    IsTurnBasedTimeout = false;
    TurnBasedBeginSecond = 0;
    NeedEndTurnBased = false;
    TurnBasedRound = 0;
    TurnBasedTurn = 0;
    TurnBasedWholeTurn = 0;

    memzero( FuncId, sizeof( FuncId ) );
    memzero( LoopEnabled, sizeof( LoopEnabled ) );
    memzero( LoopLastTick, sizeof( LoopLastTick ) );
    memzero( LoopWaitTick, sizeof( LoopWaitTick ) );

    hexFlags = new uchar[ proto->Header.MaxHexX * proto->Header.MaxHexY ];
    memzero( hexFlags, proto->Header.MaxHexX * proto->Header.MaxHexY );

    memzero( &Data, sizeof( Data ) );
    Data.MapPid = proto->GetPid();
    Data.MapTime = proto->Header.Time;
    Data.MapRain = 0;
    Data.IsTurnBasedAviable = false;

    for( int i = 0; i < MAP_LOOP_FUNC_MAX; i++ )
        LoopWaitTick[ i ] = MAP_LOOP_DEFAULT_TICK;
}

Map::~Map()
{
    MEMORY_PROCESS( MEMORY_MAP, -(int) sizeof( Map ) );
    MEMORY_PROCESS( MEMORY_MAP_FIELD, -( Proto->Header.MaxHexX * Proto->Header.MaxHexY ) );
    SAFEDELA( hexFlags );
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

struct ItemNpcPtr
{
    Item* item;
    Npc*  npc;
    ItemNpcPtr( Item* i, Npc* n ): item( i ), npc( n ) {}
};
typedef map< uint, ItemNpcPtr > UIDtoPtrMap;

bool Map::Generate()
{
    // Map data
    dataLocker.Lock();
    for( int i = 0; i < 4; i++ )
        Data.MapDayTime[ i ] = Proto->Header.DayTime[ i ];
    for( int i = 0; i < 12; i++ )
        Data.MapDayColor[ i ] = Proto->Header.DayColor[ i ];
    dataLocker.Unlock();

    // Associated UID -> Item or Critter
    UIDtoPtrMap UIDtoPtr;

    // Generate npc
    for( auto it = Proto->CrittersVec.begin(), end = Proto->CrittersVec.end(); it != end; ++it )
    {
        MapObject& mobj = *( *it );

        // Make script name
        char script_name[ MAX_FOTEXT ];
        Str::Copy( script_name, mobj.ScriptName );
        if( script_name[ 0 ] )
            Str::Append( script_name, "@" );
        Str::Append( script_name, mobj.FuncName );

        // Create npc
        Npc* npc = CrMngr.CreateNpc( mobj.ProtoId, mobj.Props, NULL, script_name[ 0 ] ? script_name : NULL, this, mobj.MapX, mobj.MapY, (uchar) mobj.MCritter.Dir, true );
        if( !npc )
        {
            WriteLogF( _FUNC_, " - Create npc '%s' on map '%s' fail, continue generate.\n", HASH_STR( mobj.ProtoId ), GetName() );
            continue;
        }

        // Check condition
        if( mobj.MCritter.Cond != COND_LIFE && npc->Data.Cond == COND_LIFE && npc->GetMapId() == GetId() )
        {
            npc->Data.Cond = CLAMP( mobj.MCritter.Cond, COND_LIFE, COND_DEAD );

            if( npc->Data.Cond == COND_DEAD )
            {
                npc->SetCurrentHp( GameOpt.DeadHitPoints - 1 );
                if( !npc->GetReplicationTime() )
                    npc->SetReplicationTime( -1 );

                uint multihex = npc->GetMultihex();
                UnsetFlagCritter( npc->GetHexX(), npc->GetHexY(), multihex, false );
                SetFlagCritter( npc->GetHexX(), npc->GetHexY(), multihex, true );
            }
            else if( npc->Data.Cond == COND_KNOCKOUT )
            {
                npc->KnockoutAp = 10000;
            }
        }

        // Set condition animation
        if( mobj.MCritter.Cond == npc->Data.Cond )
        {
            if( npc->Data.Cond == COND_LIFE )
            {
                npc->Data.Anim1Life = mobj.MCritter.Anim1;
                npc->Data.Anim2Life = mobj.MCritter.Anim2;
            }
            else if( npc->Data.Cond == COND_KNOCKOUT )
            {
                npc->Data.Anim1Knockout = mobj.MCritter.Anim1;
                npc->Data.Anim2Knockout = mobj.MCritter.Anim2;
            }
            else
            {
                npc->Data.Anim1Dead = mobj.MCritter.Anim1;
                npc->Data.Anim2Dead = mobj.MCritter.Anim2;
            }
        }

        // Store UID association
        if( mobj.UID )
            UIDtoPtr.insert( PAIR( mobj.UID, ItemNpcPtr( NULL, npc ) ) );
    }

    // Generate items
    for( auto it = Proto->ItemsVec.begin(), end = Proto->ItemsVec.end(); it != end; ++it )
    {
        MapObject& mobj = *( *it );
        hash       pid = mobj.ProtoId;
        ProtoItem* proto = ItemMngr.GetProtoItem( pid );
        if( !proto )
        {
            WriteLogF( _FUNC_, " - Proto item '%s' on map '%s' not found, continue generate.\n", HASH_STR( pid ), GetName() );
            continue;
        }

        if( !proto->IsItem() )
            continue;

        Npc*  cr_cont = NULL;
        Item* item_cont = NULL;

        // Find container
        if( mobj.ContainerUID )
        {
            auto it = UIDtoPtr.find( mobj.ContainerUID );
            if( it == UIDtoPtr.end() )
                continue;
            cr_cont = ( *it ).second.npc;
            item_cont = ( *it ).second.item;
        }

        // Create item
        Item* item = ItemMngr.CreateItem( pid );
        if( !item )
        {
            WriteLogF( _FUNC_, " - Create item '%s' on map '%s' fail, continue generate.\n", HASH_STR( pid ), GetName() );
            continue;
        }

        // Script values
        item->SetVal0( mobj.MItem.Val[ 0 ] );
        item->SetVal1( mobj.MItem.Val[ 1 ] );
        item->SetVal2( mobj.MItem.Val[ 2 ] );
        item->SetVal3( mobj.MItem.Val[ 3 ] );
        item->SetVal4( mobj.MItem.Val[ 4 ] );
        item->SetVal5( mobj.MItem.Val[ 5 ] );
        item->SetVal6( mobj.MItem.Val[ 6 ] );
        item->SetVal7( mobj.MItem.Val[ 7 ] );
        item->SetVal8( mobj.MItem.Val[ 8 ] );
        item->SetVal9( mobj.MItem.Val[ 9 ] );

        // Deterioration
        if( item->IsDeteriorable() )
        {
            item->SetBrokenFlags( mobj.MItem.BrokenFlags );
            item->SetBrokenCount( mobj.MItem.BrokenCount );
            item->SetDeterioration( mobj.MItem.Deterioration );
        }

        // Stacking
        if( item->IsStackable() )
        {
            if( mobj.MItem.Count > 1 )
                item->SetCount( mobj.MItem.Count );
        }

        // Trap value
        if( mobj.MItem.TrapValue )
            item->SetTrapValue( mobj.MItem.TrapValue );

        // Other values
        item->SetOffsetX( mobj.MItem.OffsetX );
        item->SetOffsetY( mobj.MItem.OffsetY );
        item->SetAmmoPid( mobj.MItem.AmmoPid );
        item->SetAmmoCount( mobj.MItem.AmmoCount );
        item->SetLockerId( mobj.MItem.LockerDoorId );
        item->SetLockerCondition( mobj.MItem.LockerCondition );
        item->SetLockerComplexity( mobj.MItem.LockerComplexity );
        if( item->IsDoor() && FLAG( item->GetLockerCondition(), LOCKER_ISOPEN ) )
        {
            if( !item->Proto->GetDoor_NoBlockMove() )
                item->SetIsNoBlock( true );
            if( !item->Proto->GetDoor_NoBlockShoot() )
                item->SetIsShootThru( true );
            if( !item->Proto->GetDoor_NoBlockLight() )
                item->SetIsLightThru( true );
        }

        // Mapper additional parameters
        if( mobj.MItem.PicMap )
            item->SetPicMap( mobj.MItem.PicMap );

        // Parse script
        char script_name[ MAX_FOTEXT ] = { 0 };
        Str::Copy( script_name, mobj.ScriptName );
        if( script_name[ 0 ] )
            Str::Append( script_name, "@" );
        Str::Append( script_name, mobj.FuncName );

        // Store UID association
        if( mobj.UID )
            UIDtoPtr.insert( PAIR( mobj.UID, ItemNpcPtr( item, NULL ) ) );

        // Transfer to container
        if( mobj.ContainerUID )
        {
            if( cr_cont )
            {
                cr_cont->AddItem( item, false );
                if( mobj.MItem.ItemSlot == SLOT_HAND1 )
                    cr_cont->SetFavoriteItemPid( SLOT_HAND1, item->GetProtoId() );
                else if( mobj.MItem.ItemSlot == SLOT_HAND2 )
                    cr_cont->SetFavoriteItemPid( SLOT_HAND2, item->GetProtoId() );
                else if( mobj.MItem.ItemSlot == SLOT_ARMOR )
                    cr_cont->SetFavoriteItemPid( SLOT_ARMOR, item->GetProtoId() );
            }
            else if( item_cont )
            {
                item_cont->ContAddItem( item, 0 );
            }
        }
        // Transfer to hex
        else
        {
            if( !AddItem( item, mobj.MapX, mobj.MapY ) )
            {
                WriteLogF( _FUNC_, " - Add item '%s' to map '%s' failure, continue generate.\n", HASH_STR( pid ), GetName() );
                ItemMngr.DeleteItem( item );
                continue;
            }
        }

        // Script
        if( script_name[ 0 ] )
            item->SetScript( script_name, true );
    }

    // Npc initialization
    PcVec npcs;
    GetNpcs( npcs, true );

    for( auto it = npcs.begin(), end = npcs.end(); it != end; ++it )
    {
        Npc* npc = *it;

        // Generate internal bag
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

        // Visible
        npc->ProcessVisibleCritters();
        npc->ProcessVisibleItems();
    }

    // Map script
    if( Proto->Header.ScriptModule[ 0 ] && Proto->Header.ScriptFunc[ 0 ] )
    {
        char script_name[ MAX_FOTEXT ];
        Str::Copy( script_name, Proto->Header.ScriptModule );
        Str::Append( script_name, "@" );
        Str::Append( script_name, Proto->Header.ScriptFunc );
        SetScript( script_name, true );
    }
    return true;
}

void Map::Process()
{
    if( IsTurnBasedOn )
        ProcessTurnBased();

    if( NeedProcess )
    {
        uint tick = Timer::GameTick();
        for( int i = 0; i < MAP_LOOP_FUNC_MAX; i++ )
        {
            if( LoopEnabled[ i ] && tick - LoopLastTick[ i ] >= LoopWaitTick[ i ] )
            {
                EventLoop( i );
                LoopLastTick[ i ] = tick;
            }
        }
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
    ent = Proto->GetEntireRandom( entire );

    if( !ent )
        return false;

    hx = ent->HexX;
    hy = ent->HexY;
    dir = ent->Dir;

    if( hx >= GetMaxHexX() || hy >= GetMaxHexY() )
        return false;
    if( dir >= DIRS_COUNT )
        dir = Random( 0, DIRS_COUNT - 1 );
    return true;
}

bool Map::GetStartCoordCar( ushort& hx, ushort& hy, ProtoItem* proto_item )
{
    if( !proto_item->IsCar() )
        return false;

    ProtoMap::EntiresVec entires;
    Proto->GetEntires( proto_item->GetCar_Entrance(), entires );
    std::random_shuffle( entires.begin(), entires.end() );

    for( int i = 0, j = (uint) entires.size(); i < j; i++ )
    {
        ProtoMap::MapEntire* ent = &entires[ i ];
        if( ent->HexX < GetMaxHexX() && ent->HexY < GetMaxHexY() && IsPlaceForItem( ent->HexX, ent->HexY, proto_item ) )
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

        if( nx < 0 || nx >= GetMaxHexX() || ny < 0 || ny >= GetMaxHexY() )
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

        cr->SetMaps( GetId(), GetPid() );
    }

    SetFlagCritter( cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), cr->IsDead() );
    cr->SetTimeoutBattle( IsTurnBasedOn ? TB_BATTLE_TIMEOUT : 0 );
}

void Map::AddCritterEvents( Critter* cr )
{
    cr->LockMapTransfers++;
    EventInCritter( cr );
    if( Script::PrepareContext( ServerFunctions.MapCritterIn, _FUNC_, cr->GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::RunPrepared();
    }
    cr->LockMapTransfers--;
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

void Map::EraseCritterEvents( Critter* cr )
{
    cr->LockMapTransfers++;
    EventOutCritter( cr );
    if( Script::PrepareContext( ServerFunctions.MapCritterOut, _FUNC_, cr->GetInfo() ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::RunPrepared();
    }
    cr->LockMapTransfers--;
}

void Map::KickPlayersToGlobalMap()
{
    ClVec players;
    GetPlayers( players, true );

    for( auto it = players.begin(), end = players.end(); it != end; ++it )
    {
        Critter* cr = *it;
        SYNC_LOCK( cr );
        MapMngr.TransitToGlobal( cr, 0, FOLLOW_FORCE, true );
    }
}

bool Map::AddItem( Item* item, ushort hx, ushort hy )
{
    if( !item )
        return false;
    if( !item->Proto->IsItem() )
        return false;
    if( hx >= GetMaxHexX() || hy >= GetMaxHexY() )
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
                    if( Script::PrepareContext( ServerFunctions.CheckTrapLook, _FUNC_, cr->GetInfo() ) )
                    {
                        Script::SetArgObject( this );
                        Script::SetArgObject( cr );
                        Script::SetArgObject( item );
                        if( Script::RunPrepared() )
                            allowed = Script::GetReturnedBool();
                    }
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
            cr->EventShowItemOnMap( item, false, NULL );
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

    item->Accessory = ITEM_ACCESSORY_HEX;
    item->AccHex.MapId = GetId();
    item->AccHex.HexX = hx;
    item->AccHex.HexY = hy;

    hexItems.push_back( item );
    SetHexFlag( hx, hy, FH_ITEM );

    if( !item->GetIsNoBlock() )
        SetHexFlag( hx, hy, FH_BLOCK_ITEM );
    if( !item->GetIsShootThru() )
        SetHexFlag( hx, hy, FH_NRAKE_ITEM );
    if( item->GetIsGag() )
        SetHexFlag( hx, hy, FH_GAG_ITEM );
    if( item->Proto->IsBlockLines() )
        PlaceItemBlocks( hx, hy, item->Proto );

    if( item->FuncId[ ITEM_EVENT_WALK ] > 0 )
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

    ushort hx = item->AccHex.HexX;
    ushort hy = item->AccHex.HexY;

    item->Accessory = ITEM_ACCESSORY_NONE;

    if( item->GetIsGeck() )
        mapLocation->GeckCount--;
    if( !item->GetIsNoBlock() && !item->GetIsShootThru() )
        RecacheHexBlockShoot( hx, hy );
    else if( !item->GetIsNoBlock() )
        RecacheHexBlock( hx, hy );
    else if( !item->GetIsShootThru() )
        RecacheHexShoot( hx, hy );
    if( item->Proto->IsBlockLines() )
        ReplaceItemBlocks( hx, hy, item->Proto );

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
            cr->EventHideItemOnMap( item, item->ViewPlaceOnMap, item->ViewByCritter );
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
                cr->EventChangeItemOnMap( item );
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
                cr->EventHideItemOnMap( item, false, NULL );
            }
            else if( !item->GetIsAlwaysView() )           // Check distance for non-hide items
            {
                bool allowed = false;
                if( item->GetIsTrap() && FLAG( GameOpt.LookChecks, LOOK_CHECK_ITEM_SCRIPT ) )
                {
                    if( Script::PrepareContext( ServerFunctions.CheckTrapLook, _FUNC_, cr->GetInfo() ) )
                    {
                        Script::SetArgObject( this );
                        Script::SetArgObject( cr );
                        Script::SetArgObject( item );
                        if( Script::RunPrepared() )
                            allowed = Script::GetReturnedBool();
                    }
                }
                else
                {
                    int dist = DistGame( cr->GetHexX(), cr->GetHexY(), item->AccHex.HexX, item->AccHex.HexY );
                    if( item->GetIsTrap() )
                        dist += item->GetTrapValue();
                    allowed = ( dist <= (int) cr->GetLookDistance() );
                }
                if( !allowed )
                {
                    cr->DelIdVisItem( item->GetId() );
                    cr->Send_EraseItemFromMap( item );
                    cr->EventHideItemOnMap( item, false, NULL );
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
                    if( Script::PrepareContext( ServerFunctions.CheckTrapLook, _FUNC_, cr->GetInfo() ) )
                    {
                        Script::SetArgObject( this );
                        Script::SetArgObject( cr );
                        Script::SetArgObject( item );
                        if( Script::RunPrepared() )
                            allowed = Script::GetReturnedBool();
                    }
                }
                else
                {
                    int dist = DistGame( cr->GetHexX(), cr->GetHexY(), item->AccHex.HexX, item->AccHex.HexY );
                    if( item->GetIsTrap() )
                        dist += item->GetTrapValue();
                    allowed = ( dist <= (int) cr->GetLookDistance() );
                }
                if( !allowed )
                    continue;
            }

            cr->AddIdVisItem( item->GetId() );
            cr->Send_AddItemOnMap( item );
            cr->EventShowItemOnMap( item, false, NULL );
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
    return NULL;
}

Item* Map::GetItemHex( ushort hx, ushort hy, hash item_pid, Critter* picker )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->AccHex.HexX == hx && item->AccHex.HexY == hy && ( item_pid == 0 || item->GetProtoId() == item_pid ) &&
            ( !picker || ( !item->GetIsHidden() && picker->CountIdVisItem( item->GetId() ) ) ) )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

Item* Map::GetItemDoor( ushort hx, ushort hy )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->AccHex.HexX == hx && item->AccHex.HexY == hy && item->IsDoor() )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

Item* Map::GetItemCar( ushort hx, ushort hy )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->AccHex.HexX == hx && item->AccHex.HexY == hy && item->IsCar() )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

Item* Map::GetItemContainer( ushort hx, ushort hy )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->AccHex.HexX == hx && item->AccHex.HexY == hy && item->IsContainer() )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

Item* Map::GetItemGag( ushort hx, ushort hy )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->AccHex.HexX == hx && item->AccHex.HexY == hy && item->GetIsGag() )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

void Map::GetItemsHex( ushort hx, ushort hy, ItemVec& items, bool lock )
{
    for( auto it = hexItems.begin(), end = hexItems.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->AccHex.HexX == hx && item->AccHex.HexY == hy )
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
        if( ( !pid || item->GetProtoId() == pid ) && DistGame( item->AccHex.HexX, item->AccHex.HexY, hx, hy ) <= radius )
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
        if( item->AccHex.HexX == hx && item->AccHex.HexY == hy && item->FuncId[ ITEM_EVENT_WALK ] > 0 )
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
        if( item->AccHex.HexX == hx && item->AccHex.HexY == hy )
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
        if( item->AccHex.HexX == hx && item->AccHex.HexY == hy && !item->GetIsShootThru() )
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
        if( item->AccHex.HexX == hx && item->AccHex.HexY == hy )
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
    return ( hexFlags[ hy * GetMaxHexX() + hx ] << 8 ) | Proto->HexFlags[ hy * GetMaxHexX() + hx ];
}

void Map::SetHexFlag( ushort hx, ushort hy, uchar flag )
{
    SETFLAG( hexFlags[ hy * GetMaxHexX() + hx ], flag );
}

void Map::UnsetHexFlag( ushort hx, ushort hy, uchar flag )
{
    UNSETFLAG( hexFlags[ hy * GetMaxHexX() + hx ], flag );
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
    short  maxhx = GetMaxHexX();
    short  maxhy = GetMaxHexY();
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
    if( hx_ < 0 || hy_ < 0 || hx_ >= GetMaxHexX() || hy_ >= GetMaxHexY() )
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
        return FLAG( hexFlags[ hy * GetMaxHexX() + hx ], FH_DEAD_CRITTER );
    else
        return FLAG( hexFlags[ hy * GetMaxHexX() + hx ], FH_CRITTER );
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
            short  maxhx = GetMaxHexX();
            short  maxhy = GetMaxHexY();
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
            short  maxhx = GetMaxHexX();
            short  maxhy = GetMaxHexY();
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
    Critter* cr = NULL;

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
            return NULL;
    }

    return cr;
}

Critter* Map::GetNpc( int npc_role, int find_type, uint skip_count, bool sync_lock )
{
    Npc* npc = NULL;

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
        return NULL;

    Critter* cr = NULL;

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
    ushort hx = car->AccHex.HexX;
    ushort hy = car->AccHex.HexY;

    // Move car from map to inventory
    EraseItem( car->GetId() );
    car->SetIsHidden( true );
    cr->AddItem( car, false );

    // Move car bags from map to inventory
    for( int i = 0; i < ITEM_MAX_CHILDS; i++ )
    {
        Item* child = GetItemChild( hx, hy, car->Proto, i );
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
    if( hx >= GetMaxHexX() || hy >= GetMaxHexY() || !cr || !car || !car->IsCar() )
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
        hash child_pid = car->Proto->GetChildPid( i );
        if( !child_pid )
            continue;

        Item* child = cr->GetItemByPid( child_pid );
        if( !child )
            continue;

        // Move to position
        ushort child_hx = hx;
        ushort child_hy = hy;
        FOREACH_PROTO_ITEM_LINES( car->Proto->GetChildLinesStr( i ), child_hx, child_hy, GetMaxHexX(), GetMaxHexY() );
        cr->EraseItem( child, false );
        child->SetIsHidden( false );
        AddItem( child, child_hx, child_hy );
    }
}

bool Map::IsPlaceForItem( ushort hx, ushort hy, ProtoItem* proto_item )
{
    if( !IsHexPassed( hx, hy ) )
        return false;

    FOREACH_PROTO_ITEM_LINES_WORK( proto_item->GetBlockLines(), hx, hy, GetMaxHexX(), GetMaxHexY(),
                                   if( IsHexCritter( hx, hy ) )
                                       return false;
                                   // if( !IsHexPassed( hx, hy ) )
                                   //    return false;
                                   );

    return true;
}

void Map::PlaceItemBlocks( ushort hx, ushort hy, ProtoItem* proto_item )
{
    bool raked = Item::PropertyIsShootThru->GetValue< bool >( &proto_item->ItemPropsEntity );
    FOREACH_PROTO_ITEM_LINES_WORK( proto_item->GetBlockLines(), hx, hy, GetMaxHexX(), GetMaxHexY(),
                                   SetHexFlag( hx, hy, FH_BLOCK_ITEM );
                                   if( !raked )
                                       SetHexFlag( hx, hy, FH_NRAKE_ITEM );
                                   );
}

void Map::ReplaceItemBlocks( ushort hx, ushort hy, ProtoItem* proto_item )
{
    bool raked = Item::PropertyIsShootThru->GetValue< bool >( &proto_item->ItemPropsEntity );
    FOREACH_PROTO_ITEM_LINES_WORK( proto_item->GetBlockLines(), hx, hy, GetMaxHexX(), GetMaxHexY(),
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

Item* Map::GetItemChild( ushort hx, ushort hy, ProtoItem* proto_item, uint child_index )
{
    // Get child pid
    hash child_pid = proto_item->GetChildPid( child_index );
    if( !child_pid )
        return NULL;

    // Move to position
    FOREACH_PROTO_ITEM_LINES( proto_item->GetChildLinesStr( child_index ), hx, hy, GetMaxHexX(), GetMaxHexY() );

    // Find on map
    return GetItemHex( hx, hy, child_pid, NULL );
}

bool Map::PrepareScriptFunc( int num_scr_func )
{
    if( FuncId[ num_scr_func ] <= 0 )
        return false;
    return Script::PrepareContext( FuncId[ num_scr_func ], _FUNC_, Str::FormatBuf( "Map '%s' (%u)", GetName(), GetId() ) );
}

bool Map::SetScript( const char* script_name, bool first_time )
{
    if( script_name && script_name[ 0 ] )
    {
        hash func_num = Script::BindScriptFuncNumByScriptName( script_name, "void %s(Map&,bool)" );
        if( !func_num )
        {
            WriteLogF( _FUNC_, " - Script '%s' bind fail, map '%s'.\n", script_name, GetName() );
            return false;
        }
        Data.ScriptId = func_num;
    }

    if( Data.ScriptId && Script::PrepareScriptFuncContext( Data.ScriptId, _FUNC_, Str::FormatBuf( "Map '%s' (%u)", GetName(), GetId() ) ) )
    {
        Script::SetArgObject( this );
        Script::SetArgBool( first_time );
        Script::RunPrepared();
    }
    return true;
}

void Map::EventFinish( bool to_delete )
{
    if( PrepareScriptFunc( MAP_EVENT_FINISH ) )
    {
        Script::SetArgObject( this );
        Script::SetArgBool( to_delete );
        Script::RunPrepared();
    }
}

void Map::EventLoop( int loop_num )
{
    if( PrepareScriptFunc( MAP_EVENT_LOOP_0 + loop_num ) )
    {
        Script::SetArgObject( this );
        Script::RunPrepared();
    }
}

void Map::EventInCritter( Critter* cr )
{
    if( PrepareScriptFunc( MAP_EVENT_IN_CRITTER ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::RunPrepared();
    }
}

void Map::EventOutCritter( Critter* cr )
{
    if( PrepareScriptFunc( MAP_EVENT_OUT_CRITTER ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::RunPrepared();
    }
}

void Map::EventCritterDead( Critter* cr, Critter* killer )
{
    if( PrepareScriptFunc( MAP_EVENT_CRITTER_DEAD ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::SetArgObject( killer );
        Script::RunPrepared();
    }
}

void Map::EventTurnBasedBegin()
{
    if( PrepareScriptFunc( MAP_EVENT_TURN_BASED_BEGIN ) )
    {
        Script::SetArgObject( this );
        Script::RunPrepared();
    }
}

void Map::EventTurnBasedEnd()
{
    if( PrepareScriptFunc( MAP_EVENT_TURN_BASED_BEGIN ) )
    {
        Script::SetArgObject( this );
        Script::RunPrepared();
    }
}

void Map::EventTurnBasedProcess( Critter* cr, bool begin_turn )
{
    if( PrepareScriptFunc( MAP_EVENT_TURN_BASED_BEGIN ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::SetArgBool( begin_turn );
        Script::RunPrepared();
    }
}

void Map::SetLoopTime( uint loop_num, uint ms )
{
    if( loop_num >= MAP_LOOP_FUNC_MAX )
        return;
    LoopWaitTick[ loop_num ] = ms;
}

uchar Map::GetRain()
{
    return Data.MapRain;
}

void Map::SetRain( uchar capacity )
{
    if( Data.MapRain == capacity )
        return;
    Data.MapRain = capacity;

    SCOPE_LOCK( dataLocker );

    for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
    {
        Client* cl = *it;
        cl->Send_GameInfo( this );
    }
}

int Map::GetTime()
{
    return Data.MapTime;
}

void Map::SetTime( int time )
{
    SCOPE_LOCK( dataLocker );

    if( Data.MapTime == time )
        return;
    Data.MapTime = time;

    for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
    {
        Client* cl = *it;
        cl->Send_GameInfo( this );
    }
}

uint Map::GetDayTime( uint day_part )
{
    SCOPE_LOCK( dataLocker );

    uint result = ( day_part < 4 ? Data.MapDayTime[ day_part ] : 0 );
    return result;
}

void Map::SetDayTime( uint day_part, uint time )
{
    if( day_part < 4 )
    {
        SCOPE_LOCK( dataLocker );

        if( time >= 1440 )
            time = 1439;
        Data.MapDayTime[ day_part ] = time;
        if( Data.MapDayTime[ 1 ] < Data.MapDayTime[ 0 ] )
            Data.MapDayTime[ 1 ] = Data.MapDayTime[ 0 ];
        if( Data.MapDayTime[ 2 ] < Data.MapDayTime[ 1 ] )
            Data.MapDayTime[ 2 ] = Data.MapDayTime[ 1 ];
        if( Data.MapDayTime[ 3 ] < Data.MapDayTime[ 2 ] )
            Data.MapDayTime[ 3 ] = Data.MapDayTime[ 2 ];

        for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
        {
            Client* cl = *it;
            cl->Send_GameInfo( this );
        }
    }
}

void Map::GetDayColor( uint day_part, uchar& r, uchar& g, uchar& b )
{
    if( day_part < 4 )
    {
        SCOPE_LOCK( dataLocker );

        r = Data.MapDayColor[ 0 + day_part ];
        g = Data.MapDayColor[ 4 + day_part ];
        b = Data.MapDayColor[ 8 + day_part ];
    }
}

void Map::SetDayColor( uint day_part, uchar r, uchar g, uchar b )
{
    if( day_part < 4 )
    {
        SCOPE_LOCK( dataLocker );

        Data.MapDayColor[ 0 + day_part ] = r;
        Data.MapDayColor[ 4 + day_part ] = g;
        Data.MapDayColor[ 8 + day_part ] = b;

        for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
        {
            Client* cl = *it;
            cl->Send_GameInfo( this );
        }
    }
}

int Map::GetData( uint index )
{
    SCOPE_LOCK( dataLocker );

    uint result = ( index < MAP_MAX_DATA ? Data.UserData[ index ] : 0 );
    return result;
}

void Map::SetData( uint index, int value )
{
    if( index < MAP_MAX_DATA )
    {
        SCOPE_LOCK( dataLocker );

        Data.UserData[ index ] = value;
    }
}

void Map::SetText( ushort hx, ushort hy, uint color, const char* text, ushort text_len, ushort intellect, bool unsafe_text )
{
    if( hx >= GetMaxHexX() || hy >= GetMaxHexY() )
        return;

    SCOPE_LOCK( dataLocker );

    for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
    {
        Client* cl = *it;
        if( cl->LookCacheValue >= DistGame( hx, hy, cl->GetHexX(), cl->GetHexY() ) )
            cl->Send_MapText( hx, hy, color, text, text_len, intellect, unsafe_text );
    }
}

void Map::SetTextMsg( ushort hx, ushort hy, uint color, ushort text_msg, uint num_str )
{
    if( hx >= GetMaxHexX() || hy >= GetMaxHexY() || !num_str )
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
    if( hx >= GetMaxHexX() || hy >= GetMaxHexY() || !num_str )
        return;

    SCOPE_LOCK( dataLocker );

    for( auto it = mapPlayers.begin(), end = mapPlayers.end(); it != end; ++it )
    {
        Client* cl = *it;
        if( cl->LookCacheValue >= DistGame( hx, hy, cl->GetHexX(), cl->GetHexY() ) )
            cl->Send_MapTextMsgLex( hx, hy, color, text_msg, num_str, lexems, lexems_len );
    }
}

void Map::BeginTurnBased( Critter* first_cr )
{
    if( IsTurnBasedOn )
        return;

    IsTurnBasedOn = true;
    NeedEndTurnBased = false;
    TurnBasedRound = 0;
    TurnBasedTurn = 0;
    TurnBasedWholeTurn = 0;
    TurnBasedBeginSecond = GameOpt.FullSecond;

    GenerateSequence( first_cr );

    CrVec critters;
    GetCritters( critters, true );
    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !first_cr || cr != first_cr )
        {
            if( cr->GetCurrentAp() > 0 )
                cr->SetCurrentAp( 0 );
            else
                cr->SetCurrentAp( cr->GetCurrentAp() / AP_DIVIDER * AP_DIVIDER );
        }

        cr->SetMoveAp( 0 );
        cr->SetIsEndCombat( false );
        cr->SetTurnBasedAc( 0 );
        cr->Send_GameInfo( this );
        cr->SetTimeoutBattle( TB_BATTLE_TIMEOUT );
        cr->SetTimeoutTransfer( 0 );
    }
    TurnSequenceCur = -1;
    IsTurnBasedTimeout = false;

    EventTurnBasedBegin();
    if( Script::PrepareContext( ServerFunctions.TurnBasedBegin, _FUNC_, Str::FormatBuf( "Map '%s' (%u)", GetName(), GetId() ) ) )
    {
        Script::SetArgObject( this );
        Script::RunPrepared();
    }

    if( NeedEndTurnBased || TurnSequence.empty() )
        EndTurnBased();
    else
        NextCritterTurn();
}

void Map::EndTurnBased()
{
    EventTurnBasedEnd();
    if( Script::PrepareContext( ServerFunctions.TurnBasedEnd, _FUNC_, Str::FormatBuf( "Map '%s' (%u)", GetName(), GetId() ) ) )
    {
        Script::SetArgObject( this );
        Script::RunPrepared();
    }

    IsTurnBasedOn = false;
    TurnBasedRound = 0;
    TurnBasedTurn = 0;
    TurnBasedWholeTurn = 0;
    TurnSequenceCur = -1;
    IsTurnBasedTimeout = false;

    CrVec critters;
    GetCritters( critters, true );
    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        cr->Send_GameInfo( this );
        cr->SetTimeoutBattle( 0 );
        cr->SetTurnBasedAc( 0 );

        // Continue time events
        if( GameOpt.FullSecond > TurnBasedBeginSecond )
            cr->ContinueTimeEvents( GameOpt.FullSecond - TurnBasedBeginSecond );
    }
}

void Map::ProcessTurnBased()
{
    if( !IsTurnBasedTimeout )
    {
        Critter* cr = GetCritter( GetCritterTurnId(), true );
        if( !cr || ( cr->IsDead() || cr->GetAllAp() <= 0 || cr->GetCurrentHp() <= 0 ) )
            EndCritterTurn();
    }

    if( Timer::GameTick() >= TurnBasedEndTick )
    {
        IsTurnBasedTimeout = !IsTurnBasedTimeout;
        if( IsTurnBasedTimeout )
            TurnBasedEndTick = Timer::GameTick() + TURN_BASED_TIMEOUT;
        else
            NextCritterTurn();
    }
}

bool Map::IsCritterTurn( Critter* cr )
{
    if( TurnSequenceCur >= (int) TurnSequence.size() )
        return false;
    return TurnSequence[ TurnSequenceCur ] == cr->GetId();
}

uint Map::GetCritterTurnId()
{
    if( TurnSequenceCur >= (int) TurnSequence.size() )
        return 0;
    return TurnSequence[ TurnSequenceCur ];
}

uint Map::GetCritterTurnTime()
{
    uint tick = Timer::GameTick();
    return TurnBasedEndTick > tick ? TurnBasedEndTick - tick : 0;
}

void Map::EndCritterTurn()
{
    TurnBasedEndTick = Timer::GameTick();
}

void Map::NextCritterTurn()
{
    // End previous turn
    if( TurnSequenceCur >= 0 )
    {
        Critter* cr = GetCritter( TurnSequence[ TurnSequenceCur ], true );
        if( cr )
        {
            cr->EventTurnBasedProcess( this, false );
            EventTurnBasedProcess( cr, false );
            if( Script::PrepareContext( ServerFunctions.TurnBasedProcess, _FUNC_, cr->GetInfo() ) )
            {
                Script::SetArgObject( this );
                Script::SetArgObject( cr );
                Script::SetArgBool( false );
                Script::RunPrepared();
            }

            if( cr->GetCurrentAp() > 0 )
                cr->SetCurrentAp( 0 );
        }
        else
        {
            TurnSequence.erase( TurnSequence.begin() + TurnSequenceCur );
            TurnSequenceCur--;
        }
    }

    // Begin next
    TurnSequenceCur++;
    if( TurnSequenceCur >= (int) TurnSequence.size() ) // Next round
    {
        // Next turn
        GenerateSequence( NULL );
        TurnSequenceCur = -1;
        TurnBasedRound++;
        TurnBasedTurn = 0;

        EventTurnBasedBegin();
        if( Script::PrepareContext( ServerFunctions.TurnBasedBegin, _FUNC_, Str::FormatBuf( "Map '%s' (%u)", GetName(), GetId() ) ) )
        {
            Script::SetArgObject( this );
            Script::RunPrepared();
        }

        if( NeedEndTurnBased || TurnSequence.empty() )
            EndTurnBased();
        else
            NextCritterTurn();
    }
    else     // Next critter turn
    {
        Critter* cr = GetCritter( TurnSequence[ TurnSequenceCur ], true );
        if( !cr || cr->IsDead() )
        {
            TurnSequence.erase( TurnSequence.begin() + TurnSequenceCur );
            TurnSequenceCur--;
            TurnBasedTurn++;
            TurnBasedWholeTurn++;
            NextCritterTurn();
            return;
        }

        if( cr->GetCurrentAp() >= 0 )
        {
            cr->SetCurrentAp( cr->GetActionPoints() * AP_DIVIDER );
        }
        else
        {
            cr->SetCurrentAp( cr->GetCurrentAp() + cr->GetActionPoints() * AP_DIVIDER );
            if( cr->GetCurrentAp() < 0 || ( cr->GetCurrentAp() == 0 && !cr->GetMaxMoveAp() ) )
            {
                TurnBasedTurn++;
                TurnBasedWholeTurn++;
                NextCritterTurn();
                return;
            }
        }

        if( cr->IsKnockout() )
        {
            cr->TryUpOnKnockout();
            if( cr->IsKnockout() )
            {
                TurnBasedTurn++;
                TurnBasedWholeTurn++;
                NextCritterTurn();
                return;
            }
        }

        cr->Send_CustomCommand( cr, OTHER_YOU_TURN, GameOpt.TurnBasedTick );
        cr->SendA_CustomCommand( OTHER_YOU_TURN, GameOpt.TurnBasedTick );
        TurnBasedEndTick = Timer::GameTick() + GameOpt.TurnBasedTick;

        cr->EventTurnBasedProcess( this, true );
        EventTurnBasedProcess( cr, true );
        if( Script::PrepareContext( ServerFunctions.TurnBasedProcess, _FUNC_, cr->GetInfo() ) )
        {
            Script::SetArgObject( this );
            Script::SetArgObject( cr );
            Script::SetArgBool( true );
            Script::RunPrepared();
        }
        if( NeedEndTurnBased )
            EndTurnBased();
        else
        {
            TurnBasedTurn++;
            TurnBasedWholeTurn++;
        }
    }
}

void Map::GenerateSequence( Critter* first_cr )
{
    // Collect all critters
    CrVec        critters;
    GetCritters( critters, true );
    ScriptArray* script_critters = Script::CreateArray( "Critter@[]" );
    Script::AppendVectorToArrayRef( critters, script_critters );

    // Pass to scripts
    if( Script::PrepareContext( ServerFunctions.TurnBasedSequence, _FUNC_, Str::FormatBuf( "Map '%s' (%u)", GetName(), GetId() ) ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( script_critters );
        Script::SetArgObject( first_cr );
        Script::RunPrepared();
    }

    // Fill result
    Script::AssignScriptArrayInVector( critters, script_critters );
    script_critters->Release();

    // Add only critters on this map
    TurnSequenceCur = 0;
    TurnSequence.clear();
    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->GetMapId() == GetId() )
            TurnSequence.push_back( cr->GetId() );
    }
}

/************************************************************************/
/* Location                                                             */
/************************************************************************/

const char* LocationEventFuncName[ LOCATION_EVENT_MAX ] =
{
    "void %s(Location&,bool)",                      // LOCATION_EVENT_FINISH
    "bool %s(Location&,Critter@[]&,uint8)",         // LOCATION_EVENT_ENTER
};

PROPERTIES_IMPL( Location );

Location::Location( uint id, ProtoLocation* proto, ushort wx, ushort wy ): Entity( id, EntityType::Location, PropertiesRegistrator )
{
    RUNTIME_ASSERT( proto );

    Proto = proto;
    memzero( &Data, sizeof( Data ) );
    memzero( FuncId, sizeof( FuncId ) );
    Data.LocPid = Proto->LocPid;
    Data.WX = wx;
    Data.WY = wy;
    Data.Radius = Proto->Radius;
    Data.Visible = Proto->Visible;
    Data.GeckVisible = Proto->GeckVisible;
    Data.AutoGarbage = Proto->AutoGarbage;
    GeckCount = 0;
}

Location::~Location()
{
    //
}

void Location::Update()
{
    ClVec players;
    CrMngr.GetClients( players, true );

    for( auto it = players.begin(), end = players.end(); it != end; ++it )
    {
        Client* cl = *it;
        if( !cl->GetMapId() && cl->CheckKnownLocById( GetId() ) )
            cl->Send_GlobalLocation( this, true );
    }
}

void Location::GetMaps( MapVec& maps, bool lock )
{
    maps = locMaps;
    if( lock )
        for( auto it = maps.begin(), end = maps.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

Map* Location::GetMap( uint count )
{
    if( count >= locMaps.size() )
        return NULL;
    Map* map = locMaps[ count ];
    SYNC_LOCK( map );
    return map;
}

uint Location::GetMapIndex( hash map_pid )
{
    auto it = std::find( Proto->ProtoMapPids.begin(), Proto->ProtoMapPids.end(), map_pid );
    return ( uint ) std::distance( Proto->ProtoMapPids.begin(), it );
}

bool Location::GetTransit( Map* from_map, uint& id_map, ushort& hx, ushort& hy, uchar& dir )
{
    if( !from_map || hx >= from_map->GetMaxHexX() || hy >= from_map->GetMaxHexY() )
        return false;

    MapObject* mobj = from_map->Proto->GetMapGrid( hx, hy );
    if( !mobj )
        return false;

    if( mobj->MScenery.ToMap == 0 )
    {
        id_map = 0;
        return true;
    }

    Map* to_map = NULL;
    for( auto it = locMaps.begin(), end = locMaps.end(); it != end; ++it )
    {
        Map* map = *it;
        if( map->GetPid() == mobj->MScenery.ToMap )
        {
            to_map = map;
            break;
        }
    }

    if( !to_map )
        return false;
    SYNC_LOCK( to_map );

    id_map = to_map->GetId();

    ProtoMap::MapEntire* ent = to_map->Proto->GetEntire( mobj->MScenery.ToEntire, 0 );
    if( !ent )
        return false;

    hx = ent->HexX;
    hy = ent->HexY;
    dir = ent->Dir;

    if( dir >= DIRS_COUNT )
        dir = Random( 0, DIRS_COUNT - 1 );
    if( hx >= to_map->GetMaxHexX() || hy >= to_map->GetMaxHexY() )
        return false;
    return true;
}

bool Location::IsCanEnter( uint players_count )
{
    if( !Proto->MaxPlayers )
        return true;

    for( auto it = locMaps.begin(), end = locMaps.end(); it != end; ++it )
    {
        Map* map = *it;
        players_count += map->GetPlayersCount();
    }
    return players_count <= Proto->MaxPlayers;
}

bool Location::IsNoCrit()
{
    for( auto it = locMaps.begin(), end = locMaps.end(); it != end; ++it )
    {
        Map* map = *it;
        if( map->GetCrittersCount() )
            return false;
    }
    return true;
}

bool Location::IsNoPlayer()
{
    for( auto it = locMaps.begin(), end = locMaps.end(); it != end; ++it )
    {
        Map* map = *it;
        if( map->GetPlayersCount() )
            return false;
    }
    return true;
}

bool Location::IsNoNpc()
{
    for( auto it = locMaps.begin(), end = locMaps.end(); it != end; ++it )
    {
        Map* map = *it;
        if( map->GetNpcsCount() )
            return false;
    }
    return true;
}

bool Location::IsCanDelete()
{
    if( GeckCount > 0 )
        return false;

    // Check for players
    for( auto it = locMaps.begin(), end = locMaps.end(); it != end; ++it )
    {
        Map* map = *it;
        if( map->GetPlayersCount() )
            return false;
    }

    // Check for npc
    MapVec maps = locMaps;
    for( auto it = maps.begin(), end = maps.end(); it != end; ++it )
    {
        Map*  map = *it;

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

bool Location::PrepareScriptFunc( int num_scr_func )
{
    if( FuncId[ num_scr_func ] <= 0 )
        return false;
    return Script::PrepareContext( FuncId[ num_scr_func ], _FUNC_, Str::FormatBuf( "Location '%s' (%u)", GetName(), GetId() ) );
}

void Location::EventFinish( bool to_delete )
{
    if( PrepareScriptFunc( LOCATION_EVENT_FINISH ) )
    {
        Script::SetArgObject( this );
        Script::SetArgBool( to_delete );
        Script::RunPrepared();
    }
}

bool Location::EventEnter( ScriptArray* group, uchar entrance )
{
    if( PrepareScriptFunc( LOCATION_EVENT_ENTER ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( group );
        Script::SetArgUChar( entrance );
        if( Script::RunPrepared() )
            return Script::GetReturnedBool();
    }

    // No event specified, we are good to enter
    return true;
}
