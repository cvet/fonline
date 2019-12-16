#include "Server.h"

Item* FOServer::CreateItemOnHex(
    Map* map, ushort hx, ushort hy, hash pid, uint count, Properties* props, bool check_blocks)
{
    // Checks
    ProtoItem* proto_item = ProtoMngr.GetProtoItem(pid);
    if (!proto_item || !count)
        return nullptr;

    // Check blockers
    if (check_blocks && !map->IsPlaceForProtoItem(hx, hy, proto_item))
        return nullptr;

    // Create instance
    Item* item = ItemMngr.CreateItem(pid, count, props);
    if (!item)
        return nullptr;

    // Add on map
    if (!map->AddItem(item, hx, hy))
    {
        ItemMngr.DeleteItem(item);
        return nullptr;
    }

    // Recursive non-stacked items
    if (!proto_item->GetStackable() && count > 1)
        return CreateItemOnHex(map, hx, hy, pid, count - 1, props, true);

    return item;
}

void FOServer::OnSendItemValue(Entity* entity, Property* prop)
{
    Item* item = (Item*)entity;
#pragma MESSAGE("Clean up server 0 and -1 item ids")
    if (item->Id && item->Id != uint(-1))
    {
        bool is_public = (prop->GetAccess() & Property::PublicMask) != 0;
        bool is_protected = (prop->GetAccess() & Property::ProtectedMask) != 0;
        if (item->GetAccessory() == ITEM_ACCESSORY_CRITTER)
        {
            if (is_public || is_protected)
            {
                Critter* cr = Self->CrMngr.GetCritter(item->GetCritId());
                if (cr)
                {
                    if (is_public || is_protected)
                        cr->Send_Property(NetProperty::ChosenItem, prop, item);
                    if (is_public)
                        cr->SendA_Property(NetProperty::CritterItem, prop, item);
                }
            }
        }
        else if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
        {
            if (is_public)
            {
                Map* map = Self->MapMngr.GetMap(item->GetMapId());
                if (map)
                    map->SendProperty(NetProperty::MapItem, prop, item);
            }
        }
        else if (item->GetAccessory() == ITEM_ACCESSORY_CONTAINER)
        {
#pragma MESSAGE("Add container properties changing notifications.")
            // Item* cont = ItemMngr.GetItem( item->GetContainerId() );
        }
    }
}

void FOServer::OnSetItemCount(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    Item* item = (Item*)entity;
    uint cur = *(uint*)cur_value;
    uint old = *(uint*)old_value;
    if ((int)cur > 0 && (item->GetStackable() || cur == 1))
    {
        int diff = (int)item->GetCount() - (int)old;
        Self->ItemMngr.ChangeItemStatistics(item->GetProtoId(), diff);
    }
    else
    {
        item->SetCount(old);
        if (!item->GetStackable())
            SCRIPT_ERROR_R("Trying to change count of not stackable item.");
        else
            SCRIPT_ERROR_R("Item count can't be zero or negative ({}).", (int)cur);
    }
}

void FOServer::OnSetItemChangeView(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    // IsHidden, IsAlwaysView, IsTrap, TrapValue
    Item* item = (Item*)entity;

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
    {
        Map* map = Self->MapMngr.GetMap(item->GetMapId());
        if (map)
        {
            map->ChangeViewItem(item);
            if (prop == Item::PropertyIsTrap)
                map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
        }
    }
    else if (item->GetAccessory() == ITEM_ACCESSORY_CRITTER)
    {
        Critter* cr = Self->CrMngr.GetCritter(item->GetCritId());
        if (cr)
        {
            bool value = *(bool*)cur_value;
            if (value)
                cr->Send_EraseItem(item);
            else
                cr->Send_AddItem(item);
            cr->SendAA_MoveItem(item, ACTION_REFRESH, 0);
        }
    }
}

void FOServer::OnSetItemRecacheHex(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    // IsNoBlock, IsShootThru, IsGag, IsTrigger
    Item* item = (Item*)entity;
    bool value = *(bool*)cur_value;

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
    {
        Map* map = Self->MapMngr.GetMap(item->GetMapId());
        if (map)
            map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
    }
}

void FOServer::OnSetItemBlockLines(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    // BlockLines
    Item* item = (Item*)entity;
    if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
    {
        Map* map = Self->MapMngr.GetMap(item->GetMapId());
        if (map)
        {
#pragma MESSAGE("Make BlockLines changable in runtime.")
        }
    }
}

void FOServer::OnSetItemIsGeck(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    Item* item = (Item*)entity;
    bool value = *(bool*)cur_value;

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
    {
        Map* map = Self->MapMngr.GetMap(item->GetMapId());
        if (map)
            map->GetLocation()->GeckCount += (value ? 1 : -1);
    }
}

void FOServer::OnSetItemIsRadio(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    Item* item = (Item*)entity;
    bool value = *(bool*)cur_value;

    Self->ItemMngr.RadioRegister(item, value);
}

void FOServer::OnSetItemOpened(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    Item* item = (Item*)entity;
    bool cur = *(bool*)cur_value;
    bool old = *(bool*)old_value;

    if (item->GetIsCanOpen())
    {
        if (!old && cur)
        {
            item->SetIsLightThru(true);

            if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
            {
                Map* map = Self->MapMngr.GetMap(item->GetMapId());
                if (map)
                    map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
            }
        }
        if (old && !cur)
        {
            item->SetIsLightThru(false);

            if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
            {
                Map* map = Self->MapMngr.GetMap(item->GetMapId());
                if (map)
                {
                    map->SetHexFlag(item->GetHexX(), item->GetHexY(), FH_BLOCK_ITEM);
                    map->SetHexFlag(item->GetHexX(), item->GetHexY(), FH_NRAKE_ITEM);
                }
            }
        }
    }
}
