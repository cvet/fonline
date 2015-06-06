#include "Common.h"
#include "Server.h"


Item* FOServer::CreateItemOnHex( Map* map, ushort hx, ushort hy, hash pid, uint count, bool check_blocks /* = true */ )
{
    // Checks
    ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
    if( !proto_item || !count )
        return NULL;

    // Check blockers
    if( check_blocks && proto_item->IsBlockLines() && !map->IsPlaceForItem( hx, hy, proto_item ) )
        return NULL;

    // Create instance
    Item* item = ItemMngr.CreateItem( pid, count );
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
        hash child_pid = item->Proto->GetChildPid( i );
        if( !child_pid )
            continue;

        ProtoItem* child = ItemMngr.GetProtoItem( child_pid );
        if( !child )
            continue;

        ushort child_hx = hx, child_hy = hy;
        FOREACH_PROTO_ITEM_LINES( item->Proto->GetChildLinesStr( i ), child_hx, child_hy, map->GetMaxHexX(), map->GetMaxHexY() );

        CreateItemOnHex( map, child_hx, child_hy, child_pid, 1, false );
    }

    // Recursive non-stacked items
    if( !proto_item->GetStackable() && count > 1 )
        return CreateItemOnHex( map, hx, hy, pid, count - 1 );

    return item;
}

bool FOServer::TransferAllItems()
{
    WriteLog( "Transfer all items to npc, maps and containers...\n" );

    // Transfer items
    CrMap   critters = CrMngr.GetCrittersNoLock();
    ItemVec bad_items;
    ItemVec game_items;
    ItemMngr.GetGameItems( game_items );
    for( auto it = game_items.begin(), end = game_items.end(); it != end; ++it )
    {
        Item* item = *it;

        switch( item->Accessory )
        {
        case ITEM_ACCESSORY_CRITTER:
        {
            if( IS_CLIENT_ID( item->AccCritter.Id ) )
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

void FOServer::OnSendItemValue( void* obj, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) obj;
    #pragma MESSAGE( "Clean up server 0 and -1 item ids" )
    if( item->Id && item->Id != uint( -1 ) )
    {
        bool is_public = ( prop->GetAccess() & Property::PublicMask ) != 0;
        bool is_protected = ( prop->GetAccess() & Property::ProtectedMask ) != 0;
        if( item->Accessory == ITEM_ACCESSORY_CRITTER )
        {
            if( is_public || is_protected )
            {
                Critter* cr = CrMngr.GetCritter( item->AccCritter.Id, false );
                if( cr )
                {
                    if( is_public || is_protected )
                        cr->Send_Property( NetProperty::ChosenItem, prop, item );
                    if( is_public )
                        cr->SendA_Property( NetProperty::CritterItem, prop, item );
                }
            }
        }
        else if( item->Accessory == ITEM_ACCESSORY_HEX )
        {
            if( is_public )
            {
                Map* map = MapMngr.GetMap( item->AccHex.MapId, false );
                if( map )
                    map->SendProperty( NetProperty::MapItem, prop, item );
            }
        }
        else if( item->Accessory == ITEM_ACCESSORY_CONTAINER )
        {
            #pragma MESSAGE( "Add container properties changing notifications." )
            // Item* cont = ItemMngr.GetItem( item->AccContainer.ContainerId );
        }
    }
}

void FOServer::OnSetItemCount( void* obj, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) obj;
    uint  cur = *(uint*) cur_value;
    uint  old = *(uint*) old_value;
    if( (int) cur > 0 && ( item->IsStackable() || cur == 1 ) )
    {
        int diff = (int) item->GetCount() - (int) old;
        ItemMngr.ChangeItemStatistics( item->GetProtoId(), diff );
    }
    else
    {
        item->SetCount( old );
        if( !item->IsStackable() )
            SCRIPT_ERROR_R( "Trying to change count of not stackable item." );
        else
            SCRIPT_ERROR_R( "Item count can't be zero or negative (%d).", (int) cur );
    }
}

void FOServer::OnSetItemChangeView( void* obj, Property* prop, void* cur_value, void* old_value )
{
    // IsHidden, IsAlwaysView, IsTrap, TrapValue
    Item* item = (Item*) obj;

    if( item->Accessory == ITEM_ACCESSORY_HEX )
    {
        Map* map = MapMngr.GetMap( item->AccHex.MapId );
        if( map )
            map->ChangeViewItem( item );
    }
    else if( item->Accessory == ITEM_ACCESSORY_CRITTER && prop == Item::PropertyIsHidden )
    {
        Critter* cr = CrMngr.GetCritter( item->AccCritter.Id, false );
        if( cr )
        {
            bool value = *(bool*) cur_value;
            if( value )
                cr->Send_EraseItem( item );
            else
                cr->Send_AddItem( item );
            cr->SendAA_MoveItem( item, ACTION_REFRESH, 0 );
        }
    }
}

void FOServer::OnSetItemRecacheHex( void* obj, Property* prop, void* cur_value, void* old_value )
{
    // IsNoBlock, IsShootThru, IsGag
    Item* item = (Item*) obj;
    bool  value = *(bool*) cur_value;

    if( item->Accessory == ITEM_ACCESSORY_HEX )
    {
        Map* map = MapMngr.GetMap( item->AccHex.MapId );
        if( map )
        {
            bool recache_block = false;
            bool recache_shoot = false;

            if( prop == Item::PropertyIsNoBlock )
            {
                if( value )
                    recache_block = true;
                else
                    map->SetHexFlag( item->AccHex.HexX, item->AccHex.HexY, FH_BLOCK_ITEM );
            }

            else if( prop == Item::PropertyIsShootThru )
            {
                if( value )
                    recache_shoot = true;
                else
                    map->SetHexFlag( item->AccHex.HexX, item->AccHex.HexY, FH_NRAKE_ITEM );
            }
            else if( prop == Item::PropertyIsGag )
            {
                if( !value )
                    recache_block = true;
                else
                    map->SetHexFlag( item->AccHex.HexX, item->AccHex.HexY, FH_GAG_ITEM );
            }

            if( recache_block && recache_shoot )
                map->RecacheHexBlockShoot( item->AccHex.HexX, item->AccHex.HexY );
            else if( recache_block )
                map->RecacheHexBlock( item->AccHex.HexX, item->AccHex.HexY );
            else if( recache_shoot )
                map->RecacheHexShoot( item->AccHex.HexX, item->AccHex.HexY );
        }
    }
}

void FOServer::OnSetItemIsGeck( void* obj, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) obj;
    bool  value = *(bool*) cur_value;

    if( item->Accessory == ITEM_ACCESSORY_HEX )
    {
        Map* map = MapMngr.GetMap( item->AccHex.MapId );
        if( map )
            map->GetLocation( false )->GeckCount += ( value ? 1 : -1 );
    }
}

void FOServer::OnSetItemIsRadio( void* obj, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) obj;
    bool  value = *(bool*) cur_value;

    ItemMngr.RadioRegister( item, value );
}
