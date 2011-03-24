#include "StdAfx.h"
#include "Server.h"


Item* FOServer::CreateItemOnHex(Map* map, ushort hx, ushort hy, ushort pid, uint count, bool check_blocks /* = true */)
{
	// Checks
	ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
	if(!proto_item || !count) return NULL;

	// Check blockers
	if(check_blocks && proto_item->IsBlocks() && !map->IsPlaceForItem(hx,hy,proto_item)) return NULL;

	// Create instance
	Item* item=ItemMngr.CreateItem(pid,proto_item->Stackable?count:1);
	if(!item) return NULL;

	// Add on map
	if(!map->AddItem(item,hx,hy))
	{
		ItemMngr.ItemToGarbage(item);
		return NULL;
	}

	// Create childs
	for(int i=0;i<ITEM_MAX_CHILDS;i++)
	{
		ushort child_pid=item->Proto->ChildPid[i];
		if(!child_pid) continue;

		ProtoItem* child=ItemMngr.GetProtoItem(child_pid);
		if(!child) continue;

		ushort child_hx=hx,child_hy=hy;
		FOREACH_PROTO_ITEM_LINES(item->Proto->ChildLines[i],child_hx,child_hy,map->GetMaxHexX(),map->GetMaxHexY(),;);

		CreateItemOnHex(map,child_hx,child_hy,child_pid,1,false);
	}

	// Recursive non-stacked items
	if(!proto_item->Stackable && count>1) return CreateItemOnHex(map,hx,hy,pid,count-1);

	return item;
}

bool FOServer::TransferAllItems()
{
	WriteLog(NULL,"Transfer all items to npc, maps and containers...\n");

	if(!ItemMngr.IsInit())
	{
		WriteLog(NULL,"Item manager is not init.\n");
		return false;
	}

	// Set default items
	CrMap critters=CrMngr.GetCrittersNoLock();
	for(CrMapIt it=critters.begin(),end=critters.end();it!=end;++it)
	{
		Critter* cr=(*it).second;

		if(!cr->SetDefaultItems(ItemMngr.GetProtoItem(ITEM_DEF_SLOT),ItemMngr.GetProtoItem(ITEM_DEF_ARMOR)))
		{
			WriteLog(NULL,"Unable to set default game_items to critter<%s>.\n",cr->GetInfo());
			return false;
		}
	}

	// Transfer items
	ItemPtrVec bad_items;
	ItemPtrVec game_items;
	ItemMngr.GetGameItems(game_items);
	for(ItemPtrVecIt it=game_items.begin(),end=game_items.end();it!=end;++it)
	{
		Item* item=*it;

		switch(item->Accessory)
		{
		case ITEM_ACCESSORY_CRITTER:
			{
				if(IS_USER_ID(item->ACC_CRITTER.Id)) continue; // Skip player

				Critter* npc=CrMngr.GetNpc(item->ACC_CRITTER.Id,false);
				if(!npc)
				{
					WriteLog(NULL,"Item<%u> npc not found, id<%u>.\n",item->GetId(),item->ACC_CRITTER.Id);
					bad_items.push_back(item);
					continue;
				}

				npc->SetItem(item);
			}
			break;
		case ITEM_ACCESSORY_HEX:
			{
				Map* map=MapMngr.GetMap(item->ACC_HEX.MapId,false);
				if(!map)
				{
					WriteLog(NULL,"Item<%u> map not found, map id<%u>, hx<%u>, hy<%u>.\n",item->GetId(),item->ACC_HEX.MapId,item->ACC_HEX.HexX,item->ACC_HEX.HexY);
					bad_items.push_back(item);
					continue;
				}

				if(item->ACC_HEX.HexX>=map->GetMaxHexX() || item->ACC_HEX.HexY>=map->GetMaxHexY())
				{
					WriteLog(NULL,"Item<%u> invalid hex position, hx<%u>, hy<%u>.\n",item->GetId(),item->ACC_HEX.HexX,item->ACC_HEX.HexY);
					bad_items.push_back(item);
					continue;
				}

				if(!item->Proto->IsItem())
				{
					WriteLog(NULL,"Item<%u> is not item type<%u>.\n",item->GetId(),item->GetType());
					bad_items.push_back(item);
					continue;
				}

				map->SetItem(item,item->ACC_HEX.HexX,item->ACC_HEX.HexY);
			}
			break;
		case ITEM_ACCESSORY_CONTAINER:
			{
				Item* cont=ItemMngr.GetItem(item->ACC_CONTAINER.ContainerId,false);
				if(!cont)
				{
					WriteLog(NULL,"Item<%u> container not found, container id<%u>.\n",item->GetId(),item->ACC_CONTAINER.ContainerId);
					bad_items.push_back(item);
					continue;
				}

				if(!cont->IsContainer())
				{
					WriteLog(NULL,"Find item is not container, id<%u>, type<%u>, id_cont<%u>, type_cont<%u>.\n",item->GetId(),item->GetType(),cont->GetId(),cont->GetType());
					bad_items.push_back(item);
					continue;
				}

				cont->ContSetItem(item);
			}
			break;
		default:
			WriteLog(NULL,"Unknown accessory id<%u>, acc<%u>.\n",item->Id,item->Accessory);
			bad_items.push_back(item);
			continue;
		}
	}

	// Garbage bad items
	for(ItemPtrVecIt it=bad_items.begin(),end=bad_items.end();it!=end;++it)
	{
		Item* item=*it;
		ItemMngr.ItemToGarbage(item);
	}

	// Process visible for all npc
	for(CrMapIt it=critters.begin(),end=critters.end();it!=end;++it)
	{
		Critter* cr=(*it).second;
		cr->ProcessVisibleItems();
	}

	WriteLog(NULL,"Transfer game items complete.\n");
	return true;
}
