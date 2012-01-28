#include "StdAfx.h"
#include "Server.h"


Item* FOServer::CreateItemOnHex( Map* map, ushort hx, ushort hy, ushort pid, uint count, bool check_blocks /* = true */ )
{
    // Checks
    ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
    if( !proto_item || !count )
        return NULL;

    // Check blockers
    if( check_blocks && proto_item->IsBlocks() && !map->IsPlaceForItem( hx, hy, proto_item ) )
        return NULL;

    // Create instance
    Item* item = ItemMngr.CreateItem( pid, proto_item->Stackable ? count : 1 );
    if( !item )
        return NULL;

    // Add on map
    if( !map->AddItem( item, hx, hy ) )
    {
        ItemMngr.ItemToGarbage( item );
        return NULL;
    }

    // Create childs
    for( int i = 0; i < ITEM_MAX_CHILDS; i++ )
    {
        ushort child_pid = item->Proto->ChildPid[ i ];
        if( !child_pid )
            continue;

        ProtoItem* child = ItemMngr.GetProtoItem( child_pid );
        if( !child )
            continue;

        ushort child_hx = hx, child_hy = hy;
        FOREACH_PROTO_ITEM_LINES( item->Proto->ChildLines[ i ], child_hx, child_hy, map->GetMaxHexX(), map->GetMaxHexY(),;
                                  );

        CreateItemOnHex( map, child_hx, child_hy, child_pid, 1, false );
    }

    // Recursive non-stacked items
    if( !proto_item->Stackable && count > 1 )
        return CreateItemOnHex( map, hx, hy, pid, count - 1 );

    return item;
}

bool FOServer::TransferAllItems()
{
    WriteLog( "Transfer all items to npc, maps and containers...\n" );

    if( !ItemMngr.IsInit() )
    {
        WriteLog( "Item manager is not init.\n" );
        return false;
    }

    // Set default items
    CrMap critters = CrMngr.GetCrittersNoLock();
    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = ( *it ).second;

        if( !cr->SetDefaultItems( ItemMngr.GetProtoItem( ITEM_DEF_SLOT ), ItemMngr.GetProtoItem( ITEM_DEF_ARMOR ) ) )
        {
            WriteLog( "Unable to set default game_items to critter<%s>.\n", cr->GetInfo() );
            return false;
        }
    }

    // Transfer items
    ItemPtrVec bad_items;
    ItemPtrVec game_items;
    ItemMngr.GetGameItems( game_items );
    for( auto it = game_items.begin(), end = game_items.end(); it != end; ++it )
    {
        Item* item = *it;

        switch( item->Accessory )
        {
        case ITEM_ACCESSORY_CRITTER:
        {
            if( IS_USER_ID( item->AccCritter.Id ) )
                continue;                                                      // Skip player

            Critter* npc = CrMngr.GetNpc( item->AccCritter.Id, false );
            if( !npc )
            {
                WriteLog( "Item<%u> npc not found, id<%u>.\n", item->GetId(), item->AccCritter.Id );
                bad_items.push_back( item );
                continue;
            }

            npc->SetItem( item );
        }
        break;
        case ITEM_ACCESSORY_HEX:
        {
            Map* map = MapMngr.GetMap( item->AccHex.MapId, false );
            if( !map )
            {
                WriteLog( "Item<%u> map not found, map id<%u>, hx<%u>, hy<%u>.\n", item->GetId(), item->AccHex.MapId, item->AccHex.HexX, item->AccHex.HexY );
                bad_items.push_back( item );
                continue;
            }

            if( item->AccHex.HexX >= map->GetMaxHexX() || item->AccHex.HexY >= map->GetMaxHexY() )
            {
                WriteLog( "Item<%u> invalid hex position, hx<%u>, hy<%u>.\n", item->GetId(), item->AccHex.HexX, item->AccHex.HexY );
                bad_items.push_back( item );
                continue;
            }

            if( !item->Proto->IsItem() )
            {
                WriteLog( "Item<%u> is not item type<%u>.\n", item->GetId(), item->GetType() );
                bad_items.push_back( item );
                continue;
            }

            map->SetItem( item, item->AccHex.HexX, item->AccHex.HexY );
        }
        break;
        case ITEM_ACCESSORY_CONTAINER:
        {
            Item* cont = ItemMngr.GetItem( item->AccContainer.ContainerId, false );
            if( !cont )
            {
                WriteLog( "Item<%u> container not found, container id<%u>.\n", item->GetId(), item->AccContainer.ContainerId );
                bad_items.push_back( item );
                continue;
            }

            if( !cont->IsContainer() )
            {
                WriteLog( "Find item is not container, id<%u>, type<%u>, id_cont<%u>, type_cont<%u>.\n", item->GetId(), item->GetType(), cont->GetId(), cont->GetType() );
                bad_items.push_back( item );
                continue;
            }

            cont->ContSetItem( item );
        }
        break;
        default:
            WriteLog( "Unknown accessory id<%u>, acc<%u>.\n", item->Id, item->Accessory );
            bad_items.push_back( item );
            continue;
        }
    }

    // Garbage bad items
    for( auto it = bad_items.begin(), end = bad_items.end(); it != end; ++it )
    {
        Item* item = *it;
        ItemMngr.ItemToGarbage( item );
    }

    // Process visible for all npc
    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = ( *it ).second;
        cr->ProcessVisibleItems();
    }

    WriteLog( "Transfer game items complete.\n" );
    return true;
}
