#include "Common.h"
#include "Server.h"

Item* FOServer::CreateItemOnHex( Map* map, ushort hx, ushort hy, hash pid, uint count, Properties* props, bool check_blocks )
{
    // Checks
    ProtoItem* proto_item = ProtoMngr.GetProtoItem( pid );
    if( !proto_item || !count )
        return nullptr;

    // Check blockers
    if( check_blocks && !map->IsPlaceForProtoItem( hx, hy, proto_item ) )
        return nullptr;

    // Create instance
    Item* item = ItemMngr.CreateItem( pid, count, props );
    if( !item )
        return nullptr;

    // Add on map
    if( !map->AddItem( item, hx, hy ) )
    {
        ItemMngr.DeleteItem( item );
        return nullptr;
    }

    // Create childs
    for( int i = 0; i < ITEM_MAX_CHILDS; i++ )
    {
        hash child_pid = item->GetChildPid( i );
        if( !child_pid )
            continue;

        ProtoItem* child = ProtoMngr.GetProtoItem( child_pid );
        if( !child )
            continue;

        ushort child_hx = hx, child_hy = hy;
        FOREACH_PROTO_ITEM_LINES( item->GetChildLinesStr( i ), child_hx, child_hy, map->GetWidth(), map->GetHeight() );

        CreateItemOnHex( map, child_hx, child_hy, child_pid, 1, nullptr, false );
    }

    // Recursive non-stacked items
    if( !proto_item->GetStackable() && count > 1 )
        return CreateItemOnHex( map, hx, hy, pid, count - 1, props, true );

    return item;
}

void FOServer::OnSendItemValue( Entity* entity, Property* prop )
{
    Item* item = (Item*) entity;
    #pragma MESSAGE( "Clean up server 0 and -1 item ids" )
    if( item->Id && item->Id != uint( -1 ) )
    {
        bool is_public = ( prop->GetAccess() & Property::PublicMask ) != 0;
        bool is_protected = ( prop->GetAccess() & Property::ProtectedMask ) != 0;
        if( item->GetAccessory() == ITEM_ACCESSORY_CRITTER )
        {
            if( is_public || is_protected )
            {
                Critter* cr = CrMngr.GetCritter( item->GetCritId(), false );
                if( cr )
                {
                    if( is_public || is_protected )
                        cr->Send_Property( NetProperty::ChosenItem, prop, item );
                    if( is_public )
                        cr->SendA_Property( NetProperty::CritterItem, prop, item );
                }
            }
        }
        else if( item->GetAccessory() == ITEM_ACCESSORY_HEX )
        {
            if( is_public )
            {
                Map* map = MapMngr.GetMap( item->GetMapId(), false );
                if( map )
                    map->SendProperty( NetProperty::MapItem, prop, item );
            }
        }
        else if( item->GetAccessory() == ITEM_ACCESSORY_CONTAINER )
        {
            #pragma MESSAGE( "Add container properties changing notifications." )
            // Item* cont = ItemMngr.GetItem( item->GetContainerId() );
        }
    }
}

void FOServer::OnSetItemCount( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) entity;
    uint  cur = *(uint*) cur_value;
    uint  old = *(uint*) old_value;
    if( (int) cur > 0 && ( item->GetStackable() || cur == 1 ) )
    {
        int diff = (int) item->GetCount() - (int) old;
        ItemMngr.ChangeItemStatistics( item->GetProtoId(), diff );
    }
    else
    {
        item->SetCount( old );
        if( !item->GetStackable() )
            SCRIPT_ERROR_R( "Trying to change count of not stackable item." );
        else
            SCRIPT_ERROR_R( "Item count can't be zero or negative (%d).", (int) cur );
    }
}

void FOServer::OnSetItemChangeView( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    // IsHidden, IsAlwaysView, IsTrap, TrapValue
    Item* item = (Item*) entity;

    if( item->GetAccessory() == ITEM_ACCESSORY_HEX )
    {
        Map* map = MapMngr.GetMap( item->GetMapId() );
        if( map )
            map->ChangeViewItem( item );
    }
    else if( item->GetAccessory() == ITEM_ACCESSORY_CRITTER )
    {
        Critter* cr = CrMngr.GetCritter( item->GetCritId(), false );
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

void FOServer::OnSetItemRecacheHex( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    // IsNoBlock, IsShootThru, IsGag
    Item* item = (Item*) entity;
    bool  value = *(bool*) cur_value;

    if( item->GetAccessory() == ITEM_ACCESSORY_HEX )
    {
        Map* map = MapMngr.GetMap( item->GetMapId() );
        if( map )
        {
            bool recache_block = false;
            bool recache_shoot = false;

            if( prop == Item::PropertyIsNoBlock )
            {
                if( value )
                    recache_block = true;
                else
                    map->SetHexFlag( item->GetHexX(), item->GetHexY(), FH_BLOCK_ITEM );
            }

            else if( prop == Item::PropertyIsShootThru )
            {
                if( value )
                    recache_shoot = true;
                else
                    map->SetHexFlag( item->GetHexX(), item->GetHexY(), FH_NRAKE_ITEM );
            }
            else if( prop == Item::PropertyIsGag )
            {
                if( !value )
                    recache_block = true;
                else
                    map->SetHexFlag( item->GetHexX(), item->GetHexY(), FH_GAG_ITEM );
            }

            if( recache_block && recache_shoot )
                map->RecacheHexBlockShoot( item->GetHexX(), item->GetHexY() );
            else if( recache_block )
                map->RecacheHexBlock( item->GetHexX(), item->GetHexY() );
            else if( recache_shoot )
                map->RecacheHexShoot( item->GetHexX(), item->GetHexY() );
        }
    }
}

void FOServer::OnSetItemIsGeck( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) entity;
    bool  value = *(bool*) cur_value;

    if( item->GetAccessory() == ITEM_ACCESSORY_HEX )
    {
        Map* map = MapMngr.GetMap( item->GetMapId() );
        if( map )
            map->GetLocation( false )->GeckCount += ( value ? 1 : -1 );
    }
}

void FOServer::OnSetItemIsRadio( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) entity;
    bool  value = *(bool*) cur_value;

    ItemMngr.RadioRegister( item, value );
}

void FOServer::OnSetItemOpened( Entity* entity, Property* prop, void* cur_value, void* old_value )
{
    Item* item = (Item*) entity;
    bool  cur = *(bool*) cur_value;
    bool  old = *(bool*) old_value;

    if( item->IsDoor() || item->IsContainer() )
    {
        if( !old && cur )
        {
            item->SetIsLightThru( true );

            if( item->GetAccessory() == ITEM_ACCESSORY_HEX )
            {
                Map* map = MapMngr.GetMap( item->GetMapId() );
                if( map )
                    map->RecacheHexBlockShoot( item->GetHexX(), item->GetHexY() );
            }
        }
        if( old && !cur )
        {
            item->SetIsLightThru( false );

            if( item->GetAccessory() == ITEM_ACCESSORY_HEX )
            {
                Map* map = MapMngr.GetMap( item->GetMapId() );
                if( map )
                {
                    map->SetHexFlag( item->GetHexX(), item->GetHexY(), FH_BLOCK_ITEM );
                    map->SetHexFlag( item->GetHexX(), item->GetHexY(), FH_NRAKE_ITEM );
                }
            }
        }
    }
}
