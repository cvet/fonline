#include "StdAfx.h"
#include "Server.h"

/************************************************************************/
/* Create / Delete game_items from critters and hexes, uses in scripts  */
/************************************************************************/

Item* FOServer::CreateItemOnHex(Map* map, WORD hx, WORD hy, WORD pid, DWORD count)
{
	ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
	if(!proto_item || !count) return NULL;

	if(proto_item->IsCar() && !map->IsPlaceForCar(hx,hy,proto_item)) return NULL;

	Item* item=ItemMngr.CreateItem(pid,count);
	if(!item) return NULL;

	if(!map->AddItem(item,hx,hy))
	{
		ItemMngr.ItemToGarbage(item);
		return NULL;
	}

	// Create car bags
	if(item->IsCar())
	{
		ProtoItem* pbag;
		WORD bag_pid;
		for(int i=0;i<CAR_MAX_BAGS;i++)
		{
			if(i==0) bag_pid=item->Proto->MiscEx.StartVal1;
			else if(i==1) bag_pid=item->Proto->MiscEx.StartVal2;
			else if(i==2) bag_pid=item->Proto->MiscEx.StartVal3;
			else break;

			if(!bag_pid) continue;
			pbag=ItemMngr.GetProtoItem(bag_pid);
			if(!pbag || pbag->IsCar()) continue;

			WORD bag_hx=hx;
			WORD bag_hy=hy;

			map->GetCarBagPos(hx,hy,item->Proto,i);
			CreateItemOnHex(map,hx,hy,bag_pid,1);
		}
	}

	return item;
}

Item* FOServer::CreateItemToHexCr(Critter* cr, WORD hx, WORD hy, WORD pid, DWORD count)
{
	if(!cr) return NULL;
	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map || hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) return NULL;
	return CreateItemOnHex(map,hx,hy,pid,count);
}

bool FOServer::TransferAllItems()
{
	WriteLog("Transfer all items to npc, maps and containers...\n");

	if(!ItemMngr.IsInit())
	{
		WriteLog("Item manager is not init.\n");
		return false;
	}

	// Set default items
	CrMap critters=CrMngr.GetCrittersNoLock();
	for(CrMapIt it=critters.begin(),end=critters.end();it!=end;++it)
	{
		Critter* cr=(*it).second;

		if(!cr->SetDefaultItems(ItemMngr.GetProtoItem(ITEM_DEF_SLOT),
			ItemMngr.GetProtoItem(ITEM_DEF_SLOT),
			ItemMngr.GetProtoItem(ITEM_DEF_ARMOR)))
		{
			WriteLog("Unable to set default game_items to critter<%s>.\n",cr->GetInfo());
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
					WriteLog("Item<%u> npc not found, id<%u>.\n",item->GetId(),item->ACC_CRITTER.Id);
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
					WriteLog("Item<%u> map not found, map id<%u>, hx<%u>, hy<%u>.\n",item->GetId(),item->ACC_HEX.MapId,item->ACC_HEX.HexX,item->ACC_HEX.HexY);
					bad_items.push_back(item);
					continue;
				}

				if(item->ACC_HEX.HexX>=map->GetMaxHexX() || item->ACC_HEX.HexY>=map->GetMaxHexY())
				{
					WriteLog("Item<%u> invalid hex position, hx<%u>, hy<%u>.\n",item->GetId(),item->ACC_HEX.HexX,item->ACC_HEX.HexY);
					bad_items.push_back(item);
					continue;
				}

				if(!item->Proto->IsItem())
				{
					WriteLog("Item<%u> is not item type<%u>.\n",item->GetId(),item->GetType());
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
					WriteLog("Item<%u> container not found, container id<%u>.\n",item->GetId(),item->ACC_CONTAINER.ContainerId);
					bad_items.push_back(item);
					continue;
				}

				if(!cont->IsContainer())
				{
					WriteLog("Find item is not container, id<%u>, type<%u>, id_cont<%u>, type_cont<%u>.\n",item->GetId(),item->GetType(),cont->GetId(),cont->GetType());
					bad_items.push_back(item);
					continue;
				}

				cont->ContSetItem(item);
			}
			break;
		default:
			WriteLog("Unknown accessory id<%u>, acc<%u>.\n",item->Id,item->Accessory);
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

	WriteLog("Transfer game items complete.\n");
	return true;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/