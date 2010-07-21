#include "StdAfx.h"
#include "Item.h"
#include "ItemManager.h"

#ifdef FONLINE_SERVER
#include "MapManager.h"
#include "CritterManager.h"
#include "AI.h"
#endif

const char* ItemEventFuncName[ITEM_EVENT_MAX]=
{
	{"void %s(Item&,bool)"}, // ITEM_EVENT_FINISH
	{"bool %s(Item&,Critter&,Critter&)"}, // ITEM_EVENT_ATTACK
	{"bool %s(Item&,Critter&,Critter@,Item@,Scenery@)"}, // ITEM_EVENT_USE
	{"bool %s(Item&,Critter&,Item@)"}, // ITEM_EVENT_USE_ON_ME
	{"bool %s(Item&,Critter&,int)"}, // ITEM_EVENT_SKILL
	{"void %s(Item&,Critter&)"}, // ITEM_EVENT_DROP
	{"void %s(Item&,Critter&,uint8)"}, // ITEM_EVENT_MOVE
	{"void %s(Item&,Critter&,bool,uint8)"}, // ITEM_EVENT_WALK
};

// Item default send data mask
char Item::ItemData::SendMask[ITEM_DATA_MASK_MAX][92]=
{
	// SortValue Info Reserved0 PicMapHash   PicInvHash   AnimWaitBase AnimStay[2] AnimShow[2] AnimHide[2] Flags        Rate LightIntensity LightDistance LightFlags LightColor   ScriptId TrapValue Count        Cost         ScriptValues[10]                                                                 Shared data 8 bytes
	// ITEM_DATA_MASK_CHOSEN                                                                                           ITEM_DATA_MASK_CHOSEN                                                                                                ITEM_DATA_MASK_CHOSEN                                                                                              
	{  -1,-1,     -1,    0,     -1,-1,-1,-1, -1,-1,-1,-1,      0,0,       0,0,        0,0,        0,0,     -1,-1,-1,-1,  -1,       -1,            -1,        -1,     -1,-1,-1,-1,   0,0,     0,0,    -1,-1,-1,-1, -1,-1,-1,-1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, -1,-1,-1,-1,-1,-1,-1,-1, },
	// ITEM_DATA_MASK_CRITTER                                                                                          ITEM_DATA_MASK_CRITTER                                                                                               ITEM_DATA_MASK_CRITTER                                                                        
	{    0,0,     -1,    0,         0,0,0,0,     0,0,0,0,      0,0,       0,0,        0,0,        0,0,     -1,-1,-1,-1,  -1,       -1,            -1,        -1,     -1,-1,-1,-1,   0,0,     0,0,        0,0,0,0,     0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,         0,0,0,0,0,0,0,0, },
	// ITEM_DATA_MASK_CRITTER_EXT                                                                                      ITEM_DATA_MASK_CRITTER_EXT                                                                                           ITEM_DATA_MASK_CRITTER_EXT                                                                    
	{    0,0,     -1,    0,         0,0,0,0,     0,0,0,0,      0,0,       0,0,        0,0,        0,0,     -1,-1,-1,-1,  -1,       -1,            -1,        -1,     -1,-1,-1,-1,   0,0,     0,0,    -1,-1,-1,-1,     0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, -1,-1,-1,-1,-1,-1,-1,-1, },
	// ITEM_DATA_MASK_CONTAINER                                                                                        ITEM_DATA_MASK_CONTAINER                                                                                             ITEM_DATA_MASK_CONTAINER                                                                      
	{  -1,-1,     -1,    0,         0,0,0,0, -1,-1,-1,-1,      0,0,       0,0,        0,0,        0,0,     -1,-1,-1,-1,  -1,       -1,            -1,        -1,     -1,-1,-1,-1,   0,0,     0,0,    -1,-1,-1,-1, -1,-1,-1,-1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, -1,-1,-1,-1,-1,-1,-1,-1, },
	// ITEM_DATA_MASK_MAP                                                                                              ITEM_DATA_MASK_MAP                                                                                                   ITEM_DATA_MASK_MAP                                                                            
	{  -1,-1,     -1,    0,     -1,-1,-1,-1,     0,0,0,0,    -1,-1,     -1,-1,      -1,-1,      -1,-1,     -1,-1,-1,-1,  -1,       -1,            -1,        -1,     -1,-1,-1,-1,   0,0,     0,0,        0,0,0,0,     0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,         0,0,0,0,0,0,0,0, },
};

/************************************************************************/
/* Item                                                                 */
/************************************************************************/

void Item::Init(ProtoItem* proto)
{
	if(!proto)
	{
		WriteLog(__FUNCTION__" - Proto is null ptr.\n");
		return;
	}

#ifdef FONLINE_SERVER
	ViewByCritter=NULL;
	ViewPlaceOnMap=false;
	ChildItems=NULL;
	From=0;
#endif

	ZeroMemory(&Data,sizeof(Data));
	Proto=proto;
	Accessory=ITEM_ACCESSORY_NONE;
	Data.SortValue=0x7FFF;
	Data.AnimWaitBase=Proto->AnimWaitBase;
	Data.AnimStay[0]=Proto->AnimStay[0];
	Data.AnimStay[1]=Proto->AnimStay[1];
	Data.AnimShow[0]=Proto->AnimShow[0];
	Data.AnimShow[1]=Proto->AnimShow[1];
	Data.AnimHide[0]=Proto->AnimHide[0];
	Data.AnimHide[1]=Proto->AnimHide[1];
	Data.Flags=Proto->Flags;

	switch(GetType())
	{
	case ITEM_TYPE_WEAPON:
		Data.TechInfo.AmmoCount=Proto->Weapon.VolHolder;
		Data.TechInfo.AmmoPid=Proto->Weapon.DefAmmo;
		break;
	case ITEM_TYPE_MISC_EX:
		Data.ScriptValues[1]=Proto->MiscEx.StartVal1;
		Data.ScriptValues[2]=Proto->MiscEx.StartVal2;
		Data.ScriptValues[3]=Proto->MiscEx.StartVal3;
#ifdef FONLINE_SERVER
		if(IsHolodisk()) Data.Holodisk.Number=Random(1,42);
#endif
		break;
	case ITEM_TYPE_DOOR:
		SETFLAG(Data.Flags,ITEM_GAG);
		if(!Proto->Door.NoBlockMove) UNSETFLAG(Data.Flags,ITEM_NO_BLOCK);
		if(!Proto->Door.NoBlockShoot) UNSETFLAG(Data.Flags,ITEM_SHOOT_THRU);
		if(!Proto->Door.NoBlockLight) UNSETFLAG(Data.Flags,ITEM_LIGHT_THRU);
		break;
	default:
		break;
	}

#ifdef FONLINE_CLIENT
	Lexems="";
	LexemsRefCounter=0x80000000;
#endif
}

void Item::Clear()
{
//	proto=NULL; TODO: keep proto for ItemMngr.EraseItem

	Accessory=0x20+GetType();
	IsNotValid=true;

	/*if(IsContainer() && ChildItems)
	{
		for(ItemPtrMapIt it=ChildItems->begin();it!=ChildItems->end();++it)
		{
			Item* item=(*it).second;
			item->Clear();
			ItemMngr.FullErase item
		}
		ChildItems->clear();
		SAFEDEL(ChildItems);
	}*/
}

Item* Item::Clone()
{
	Item* clone=new Item();
	clone->Id=Id;
	clone->Proto=Proto;
	clone->Accessory=Accessory;
	clone->ACC_BUFFER=ACC_BUFFER;
	clone->Data=Data;

#ifdef FONLINE_SERVER
	clone->ViewByCritter=NULL;
	clone->ViewPlaceOnMap=false;
	clone->ChildItems=NULL;
	clone->PLexems=NULL;
#endif
#ifdef FONLINE_CLIENT
	clone->Lexems=Lexems;
	clone->LexemsRefCounter=LexemsRefCounter;
#endif

	return clone;
}


#ifdef FONLINE_SERVER
void Item::FullClear()
{
	if(!Proto)
	{
		WriteLog(__FUNCTION__" - Proto null ptr.\n");
		return;
	}

	if(IsContainer() && ChildItems)
	{
		MEMORY_PROCESS(MEMORY_ITEM,-(int)(ChildItems->size()*(sizeof(Item*)+sizeof(DWORD))));
		MEMORY_PROCESS(MEMORY_ITEM,-(int)sizeof(ItemPtrMap));
		for(ItemPtrVecIt it=ChildItems->begin(),end=ChildItems->end();it!=end;++it)
		{
			Item* item=*it;
			item->Accessory=0xB1;
			ItemMngr.ItemToGarbage(item,0x10000);
		}
		ChildItems->clear();
		SAFEDEL(ChildItems);
	}

	Clear();
}

bool Item::ParseScript(const char* script, bool first_time)
{
	if(script)
	{
		DWORD func_num=ServerScript.GetScriptFuncNum(script,"void %s(Item&,bool)");
		if(!func_num)
		{
			WriteLog(__FUNCTION__" - Script<%s> bind fail, item pid<%u>.\n",script,GetProtoId());
			return false;
		}
		Data.ScriptId=func_num;
	}

	if(Data.ScriptId && ServerScript.PrepareContext(ServerScript.GetScriptFuncBindId(Data.ScriptId),CALL_FUNC_STR,Str::Format("Item id<%u>, pid<%u>",GetId(),GetProtoId())))
	{
		ServerScript.SetArgObject(this);
		ServerScript.SetArgBool(first_time);
		ServerScript.RunPrepared();
	}
	return true;
}

bool Item::PrepareScriptFunc(int num_scr_func)
{
	if(num_scr_func>=ITEM_EVENT_MAX) return false;
	if(FuncId[num_scr_func]<=0) return false;
	return ServerScript.PrepareContext(FuncId[num_scr_func],CALL_FUNC_STR,Str::Format("Item id<%u>, pid<%u>",GetId(),GetProtoId()));
}

void Item::EventFinish(bool deleted)
{
	if(PrepareScriptFunc(ITEM_EVENT_FINISH))
	{
		ServerScript.SetArgObject(this);
		ServerScript.SetArgBool(deleted);
		ServerScript.RunPrepared();
	}
}

bool Item::EventAttack(Critter* cr, Critter* target)
{
	bool result=false;
	if(PrepareScriptFunc(ITEM_EVENT_ATTACK))
	{
		ServerScript.SetArgObject(this);
		ServerScript.SetArgObject(cr);
		ServerScript.SetArgObject(target);
		if(ServerScript.RunPrepared()) result=ServerScript.GetReturnedBool();
	}
	return result;
}

bool Item::EventUse(Critter* cr, Critter* on_critter, Item* on_item, MapObject* on_scenery)
{
	bool result=false;
	if(PrepareScriptFunc(ITEM_EVENT_USE))
	{
		ServerScript.SetArgObject(this);
		ServerScript.SetArgObject(cr);
		ServerScript.SetArgObject(on_critter);
		ServerScript.SetArgObject(on_item);
		ServerScript.SetArgObject(on_scenery);
		if(ServerScript.RunPrepared()) result=ServerScript.GetReturnedBool();
	}
	return result;
}

bool Item::EventUseOnMe(Critter* cr, Item* used_item)
{
	bool result=false;
	if(PrepareScriptFunc(ITEM_EVENT_USE_ON_ME))
	{
		ServerScript.SetArgObject(this);
		ServerScript.SetArgObject(cr);
		ServerScript.SetArgObject(used_item);
		if(ServerScript.RunPrepared()) result=ServerScript.GetReturnedBool();
	}
	return result;
}

bool Item::EventSkill(Critter* cr, int skill)
{
	bool result=false;
	if(PrepareScriptFunc(ITEM_EVENT_SKILL))
	{
		ServerScript.SetArgObject(this);
		ServerScript.SetArgObject(cr);
		ServerScript.SetArgDword(skill<0?skill:SKILL_OFFSET(skill));
		if(ServerScript.RunPrepared()) result=ServerScript.GetReturnedBool();
	}
	return result;
}

void Item::EventDrop(Critter* cr)
{
	if(!PrepareScriptFunc(ITEM_EVENT_DROP)) return;
	ServerScript.SetArgObject(this);
	ServerScript.SetArgObject(cr);
	ServerScript.RunPrepared();
}

void Item::EventMove(Critter* cr, BYTE from_slot)
{
	if(!PrepareScriptFunc(ITEM_EVENT_MOVE)) return;
	ServerScript.SetArgObject(this);
	ServerScript.SetArgObject(cr);
	ServerScript.SetArgByte(from_slot);
	ServerScript.RunPrepared();
}

void Item::EventWalk(Critter* cr, bool entered, BYTE dir)
{
	if(!PrepareScriptFunc(ITEM_EVENT_WALK)) return;
	ServerScript.SetArgObject(this);
	ServerScript.SetArgObject(cr); // Saved in Act_Move
	ServerScript.SetArgBool(entered);
	ServerScript.SetArgByte(dir);
	ServerScript.RunPrepared();
}
#endif //FONLINE_SERVER

void Item::SetSortValue(ItemPtrVec& items)
{
	WORD sort_value=0x7FFF;
	for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=*it;
		if(item==this) continue;
		if(sort_value>=item->GetSortValue()) sort_value=item->GetSortValue()-1;
	}
	Data.SortValue=sort_value;
}

bool SortItemsFunc(const Item& l, const Item& r){return l.Data.SortValue<r.Data.SortValue;}
void Item::SortItems(ItemVec& items){std::sort(items.begin(),items.end(),SortItemsFunc);}

DWORD Item::GetCount()
{
	return IsGrouped()?Data.Count:1;
}

void Item::Count_Set(DWORD val)
{
	if(!IsGrouped()) return;

#ifdef ITEMS_STATISTICS
	if(Data.Count>val) ItemMngr.SubItemStatistics(GetProtoId(),Data.Count-val);
	else if(Data.Count<val) ItemMngr.AddItemStatistics(GetProtoId(),val-Data.Count);
#endif
	Data.Count=val;
}

void Item::Count_Add(DWORD val)
{
	if(!IsGrouped()) return;

	Data.Count+=val;
#ifdef ITEMS_STATISTICS
	ItemMngr.AddItemStatistics(GetProtoId(),val);
#endif
}

void Item::Count_Sub(DWORD val)
{
	if(!IsGrouped()) return;

	if(val>Data.Count) val=Data.Count;
	Data.Count-=val;
#ifdef ITEMS_STATISTICS
	ItemMngr.SubItemStatistics(GetProtoId(),val);
#endif
}

#ifdef FONLINE_SERVER
bool Item::Wear(int count)
{
	if(!IsWeared()) return false;
	if(count<0) return false;

	BYTE& flags=Data.TechInfo.DeteorationFlags;
	BYTE& broken_count=Data.TechInfo.DeteorationCount;
	WORD& wear_count=Data.TechInfo.DeteorationValue;

	if(FLAG(flags,BI_ETERNAL)) return false;
	if(FLAG(flags,BI_BROKEN)) return false;

	wear_count+=count;

	if(wear_count>=WEAR_MAX)
	{
		wear_count=WEAR_MAX;
		broken_count++;

		int broken_lvl=Random(0,broken_count/(BROKEN_MAX/4));

		if(broken_count>=BROKEN_MAX || broken_lvl>=3) SETFLAG(flags,BI_NOTRESC);
		else if(broken_lvl==2) SETFLAG(flags,BI_HIGHBROKEN);
		else if(broken_lvl==1) SETFLAG(flags,BI_NORMBROKEN);
		else SETFLAG(flags,BI_LOWBROKEN);
	}

	return true;
}

void Item::Repair()
{
	BYTE& flags=Data.TechInfo.DeteorationFlags;
	BYTE& broken_count=Data.TechInfo.DeteorationCount;
	WORD& wear_count=Data.TechInfo.DeteorationValue;

	if(FLAG(flags,BI_ETERNAL)) return;
	flags=0;
	wear_count=0;
}
#endif

void Item::SetRate(BYTE rate)
{
	if(!IsWeapon())
	{
		rate&=0xF;
		if(rate==USE_USE && !IsCanUse() && !IsCanUseOnSmth()) rate=USE_NONE;
		else if(IsCanUse() || IsCanUseOnSmth()) rate=USE_USE;
		else rate=USE_NONE;
	}
	else
	{
		BYTE use=(rate&0xF);
		BYTE aim=(rate>>4);

		switch(use)
		{
		case USE_PRIMARY: if(Proto->Weapon.CountAttack & 0x1) break; use=0xF; aim=0; break;
		case USE_SECONDARY: if(Proto->Weapon.CountAttack & 0x2) break; use=USE_PRIMARY; aim=0; break;
		case USE_THIRD: if(Proto->Weapon.CountAttack & 0x4) break; use=USE_PRIMARY; aim=0; break;
		case USE_RELOAD: aim=0; if(Proto->Weapon.VolHolder) break; use=USE_PRIMARY; break;
		case USE_USE: aim=0; if(IsCanUseOnSmth()) break; use=USE_PRIMARY; break;
		default: use=USE_PRIMARY; aim=0; break;
		}

		if(use<MAX_USES && aim && !Proto->Weapon.Aim[use]) aim=0;
		rate=(aim<<4)|(use&0xF);
	}
	Data.Rate=rate;
	if(IsWeapon()) WeapRefreshProtoUse();
}

DWORD Item::GetCost1st()
{
	DWORD cost=(Data.Cost?Data.Cost:Proto->Cost);
	//if(IsWeared()) cost-=cost*GetWearProc()/100;
	if(IsWeapon() && Data.TechInfo.AmmoCount)
	{
		ProtoItem* ammo=ItemMngr.GetProtoItem(Data.TechInfo.AmmoPid);
		if(ammo) cost+=ammo->Cost*Data.TechInfo.AmmoCount;
	}
	if(!cost) cost=1;
	return cost;
}

#ifdef FONLINE_SERVER
void Item::SetLexems(const char* lexems)
{
	if(lexems)
	{
		DWORD len=strlen(lexems);
		if(!len)
		{
			SAFEDELA(PLexems);
			return;
		}

		if(len+1>=LEXEMS_SIZE) return;

		if(!PLexems)
		{
			MEMORY_PROCESS(MEMORY_ITEM,LEXEMS_SIZE);
			PLexems=new char[LEXEMS_SIZE];
			if(!PLexems) return;
		}
		memcpy(PLexems,lexems,len+1);
	}
	else
	{
		if(PLexems) MEMORY_PROCESS(MEMORY_ITEM,-LEXEMS_SIZE);
		SAFEDELA(PLexems);
	}
}
#endif

void Item::WeapRefreshProtoUse()
{
	if(!IsWeapon()) return;
	int use=Data.Rate&0xF;
	if(use>=MAX_USES) use=0;
	Proto->Weapon_SetUse(use);
}

bool Item::WeapIsHtHAttack(int use)
{
	return Proto->Weapon.Skill[use]==SKILL_OFFSET(SK_UNARMED) ||
		Proto->Weapon.Skill[use]==SKILL_OFFSET(SK_MELEE_WEAPONS);
}

bool Item::WeapIsGunAttack(int use)
{
	return Proto->Weapon.Skill[use]==SKILL_OFFSET(SK_SMALL_GUNS) ||
		Proto->Weapon.Skill[use]==SKILL_OFFSET(SK_BIG_GUNS) ||
		Proto->Weapon.Skill[use]==SKILL_OFFSET(SK_ENERGY_WEAPONS);
}

bool Item::WeapIsRangedAttack(int use)
{
	return WeapIsGunAttack(use) || Proto->Weapon.Skill[use]==SKILL_OFFSET(SK_THROWING);
}


void Item::WeapLoadHolder()
{
	if(!Data.TechInfo.AmmoPid) Data.TechInfo.AmmoPid=Proto->Weapon.DefAmmo;
	Data.TechInfo.AmmoCount=Proto->Weapon.VolHolder;
}

#ifdef FONLINE_SERVER
void Item::ContAddItem(Item*& item, DWORD special_id)
{
	if(!IsContainer() || !item) return;

	if(!ChildItems)
	{
		MEMORY_PROCESS(MEMORY_ITEM,sizeof(ItemPtrMap));
		ChildItems=new ItemPtrVec;
	}
	if(!ChildItems)
	{
		WriteLog(__FUNCTION__" - Allocation fail.\n");
		return;
	}

	if(item->IsGrouped())
	{
		Item* item_=ContGetItemByPid(item->GetProtoId(),special_id);
		if(item_)
		{
			item_->Count_Add(item->GetCount());
			ItemMngr.ItemToGarbage(item,0x20000);
			item=item_;
			return;
		}
	}

	item->ACC_CONTAINER.SpecialId=special_id;
	item->SetSortValue(*ChildItems);
	ContSetItem(item);
}

void Item::ContSetItem(Item* item)
{
	if(!ChildItems)
	{
		MEMORY_PROCESS(MEMORY_ITEM,sizeof(ItemPtrMap));
		ChildItems=new ItemPtrVec;
	}
	if(!ChildItems)
	{
		WriteLog(__FUNCTION__" - Allocation fail.\n");
		return;
	}
	if(std::find(ChildItems->begin(),ChildItems->end(),item)!=ChildItems->end())
	{
		WriteLog(__FUNCTION__" - Item already added!\n");
		return;
	}

	MEMORY_PROCESS(MEMORY_ITEM,sizeof(Item*)+sizeof(DWORD));
	ChildItems->push_back(item);
	item->Accessory=ITEM_ACCESSORY_CONTAINER;
	item->ACC_CONTAINER.ContainerId=GetId();
}

void Item::ContEraseItem(Item* item)
{
	if(!IsContainer())
	{
		WriteLog(__FUNCTION__" - Item is not container.\n");
		return;
	}
	if(!ChildItems)
	{
		WriteLog(__FUNCTION__" - Container items null ptr.\n");
		return;
	}
	if(!item)
	{
		WriteLog(__FUNCTION__" - Item null ptr.\n");
		return;
	}

	ItemPtrVecIt it=std::find(ChildItems->begin(),ChildItems->end(),item);
	if(it!=ChildItems->end())
	{
		MEMORY_PROCESS(MEMORY_ITEM,-(int)(sizeof(Item*)+sizeof(DWORD)));
		ChildItems->erase(it);
	}
	else WriteLog(__FUNCTION__" - Item not found, id<%u>, pid<%u>, container<%u>.\n",item->GetId(),item->GetProtoId(),GetId());
	item->Accessory=0xd3;
	//if(ChildItems->empty()) SAFEDEL(items); // TODO:
}

void Item::ContErsAllItems()
{
	if(!IsContainer()) return;
	if(!ChildItems) return;
	MEMORY_PROCESS(MEMORY_ITEM,-(int)(ChildItems->size()*(sizeof(Item*)+sizeof(DWORD))));
	ChildItems->clear();
}

Item* Item::ContGetItem(DWORD item_id, bool skip_hide)
{
	if(!IsContainer() || !ChildItems || !item_id) return NULL;
	for(ItemPtrVecIt it=ChildItems->begin(),end=ChildItems->end();it!=end;++it)
	{
		Item* item=*it;
		if(item->GetId()==item_id)
		{
			if(skip_hide && item->IsHidden()) return NULL;
			return item;
		}
	}
	return NULL;
}

void Item::ContGetAllItems(ItemPtrVec& items, bool skip_hide)
{
	if(!IsContainer() || !ChildItems) return;
	for(ItemPtrVecIt it=ChildItems->begin(),end=ChildItems->end();it!=end;++it)
	{
		Item* item=*it;
		if(!skip_hide || !item->IsHidden()) items.push_back(item);
	}
}

Item* Item::ContGetItemByPid(WORD pid, DWORD special_id)
{
	if(!IsContainer() || !ChildItems) return NULL;
	for(ItemPtrVecIt it=ChildItems->begin(),end=ChildItems->end();it!=end;++it)
	{
		Item* item=*it;
		if(item->GetProtoId()==pid && (special_id==-1 || item->ACC_CONTAINER.SpecialId==special_id)) return item;
	}
	return NULL;	
}

void Item::ContGetItems(ItemPtrVec& items, DWORD special_id)
{
	if(!IsContainer() || !ChildItems) return;
	for(ItemPtrVecIt it=ChildItems->begin(),end=ChildItems->end();it!=end;++it)
	{
		Item* item=*it;
		if(special_id==-1 || item->ACC_CONTAINER.SpecialId==special_id) items.push_back(item);
	}
}

DWORD Item::ContGetFreeVolume(DWORD special_id)
{
	if(!IsContainer()) return 0;
	DWORD cur_volume=0;
	DWORD max_volume=Proto->Container.ContVolume;
	if(!ChildItems) return max_volume;
	for(ItemPtrVecIt it=ChildItems->begin(),end=ChildItems->end();it!=end;++it)
	{
		Item* item=*it;
		if(special_id==-1 || item->ACC_CONTAINER.SpecialId==special_id) cur_volume+=item->GetVolume();
	}
	if(cur_volume>=max_volume) return 0;
	return max_volume-cur_volume;
}

Item* Item::CarGetBag(int num_bag)
{
	if(!IsCar()) return NULL;
	if(Accessory==ITEM_ACCESSORY_HEX)
	{
		Map* map=MapMngr.GetMap(ACC_HEX.MapId);
		if(!map) return NULL;
		return map->GetCarBag(ACC_HEX.HexX,ACC_HEX.HexY,Proto,num_bag);
	}
	return NULL;
}
#endif // FONLINE_SERVER
