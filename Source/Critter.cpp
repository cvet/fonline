#include "StdAfx.h"
#include "Critter.h"
#include "MapManager.h"
#include "ItemManager.h"
#include "CritterType.h"
#include "Access.h"
#include "CritterManager.h"

const char* CritterEventFuncName[CRITTER_EVENT_MAX]=
{
	{"void %s(Critter&)"}, // CRITTER_EVENT_IDLE
	{"void %s(Critter&,bool)"}, // CRITTER_EVENT_FINISH
	{"void %s(Critter&,Critter@)"}, // CRITTER_EVENT_DEAD
	{"void %s(Critter&)"}, // CRITTER_EVENT_RESPAWN
	{"void %s(Critter&,Critter&)"}, // CRITTER_EVENT_SHOW_CRITTER
	{"void %s(Critter&,Critter&)"}, // CRITTER_EVENT_SHOW_CRITTER_1
	{"void %s(Critter&,Critter&)"}, // CRITTER_EVENT_SHOW_CRITTER_2
	{"void %s(Critter&,Critter&)"}, // CRITTER_EVENT_SHOW_CRITTER_3
	{"void %s(Critter&,Critter&)"}, // CRITTER_EVENT_HIDE_CRITTER
	{"void %s(Critter&,Critter&)"}, // CRITTER_EVENT_HIDE_CRITTER_1
	{"void %s(Critter&,Critter&)"}, // CRITTER_EVENT_HIDE_CRITTER_2
	{"void %s(Critter&,Critter&)"}, // CRITTER_EVENT_HIDE_CRITTER_3
	{"void %s(Critter&,Item&,bool,Critter@)"}, // CRITTER_EVENT_SHOW_ITEM_ON_MAP
	{"void %s(Critter&,Item&)"}, // CRITTER_EVENT_CHANGE_ITEM_ON_MAP
	{"void %s(Critter&,Item&,bool,Critter@)"}, // CRITTER_EVENT_HIDE_ITEM_ON_MAP
	{"bool %s(Critter&,Critter&)"}, // CRITTER_EVENT_ATTACK
	{"bool %s(Critter&,Critter&)"}, // CRITTER_EVENT_ATTACKED
	{"void %s(Critter&,Critter&,bool,Item&,uint)"}, // CRITTER_EVENT_STEALING
	{"void %s(Critter&,Critter&,int,int)"}, // CRITTER_EVENT_MESSAGE
	{"bool %s(Critter&,Item&,Critter@,Item@,Scenery@)"}, // CRITTER_EVENT_USE_ITEM
	{"bool %s(Critter&,int,Critter@,Item@,Scenery@)"}, // CRITTER_EVENT_USE_SKILL
	{"void %s(Critter&,Item&)"}, // CRITTER_EVENT_DROP_ITEM
	{"void %s(Critter&,Item&,uint8)"}, // CRITTER_EVENT_MOVE_ITEM
	{"void %s(Critter&,bool,uint,uint)"}, // CRITTER_EVENT_KNOCKOUT
	{"void %s(Critter&,Critter&,Critter@)"}, // CRITTER_EVENT_SMTH_DEAD
	{"void %s(Critter&,Critter&,Critter&,bool,Item&,uint)"}, // CRITTER_EVENT_SMTH_STEALING
	{"void %s(Critter&,Critter&,Critter&)"}, // CRITTER_EVENT_SMTH_ATTACK
	{"void %s(Critter&,Critter&,Critter&)"}, // CRITTER_EVENT_SMTH_ATTACKED
	{"void %s(Critter&,Critter&,Item&,Critter@,Item@,Scenery@)"}, // CRITTER_EVENT_SMTH_USE_ITEM
	{"void %s(Critter&,Critter&,int,Critter@,Item@,Scenery@)"}, // CRITTER_EVENT_SMTH_USE_SKILL
	{"void %s(Critter&,Critter&,Item&)"}, // CRITTER_EVENT_SMTH_DROP_ITEM
	{"void %s(Critter&,Critter&,Item&,uint8)"}, // CRITTER_EVENT_SMTH_MOVE_ITEM
	{"void %s(Critter&,Critter&,bool,uint,uint)"}, // CRITTER_EVENT_SMTH_KNOCKOUT
	{"int %s(Critter&,NpcPlane&,int,Critter@,Item@)"}, // CRITTER_EVENT_PLANE_BEGIN
	{"int %s(Critter&,NpcPlane&,int,Critter@,Item@)"}, // CRITTER_EVENT_PLANE_END
	{"int %s(Critter&,NpcPlane&,int,uint&,uint&,uint&)"}, // CRITTER_EVENT_PLANE_RUN
	{"bool %s(Critter&,Critter&,bool,uint)"}, // CRITTER_EVENT_BARTER
	{"bool %s(Critter&,Critter&,bool,uint)"}, // CRITTER_EVENT_TALK
	{"bool %s(Critter&,int,Critter@[]&,Item@,uint&,uint&,uint&,uint&,uint&,uint&,bool&)"}, // CRITTER_EVENT_GLOBAL_PROCESS
	{"bool %s(Critter&,Critter@[]&,Item@,uint,int,uint&,uint16&,uint16&,uint8&)"}, // CRITTER_EVENT_GLOBAL_INVITE
	{"void %s(Critter&,Map&,bool)"}, // CRITTER_EVENT_TURN_BASED_PROCESS
	{"void %s(Critter&,Critter&,Map&,bool)"}, // CRITTER_EVENT_SMTH_TURN_BASED_PROCESS
};

/************************************************************************/
/*                                                                      */
/************************************************************************/

WSASendCallback Critter::SendDataCallback=NULL;
bool Critter::ParamsRegEnabled[MAX_PARAMS]={0};
DWORD Critter::ParamsSendMsgLen=sizeof(Critter::ParamsSendCount);
WORD Critter::ParamsSendCount=0;
WordVec Critter::ParamsSend;
bool Critter::ParamsSendEnabled[MAX_PARAMS]={0};
int Critter::ParamsSendConditionIndex[MAX_PARAMS]={0};
int Critter::ParamsSendConditionMask[MAX_PARAMS]={0};
int Critter::ParamsChangeScript[MAX_PARAMS]={0};
int Critter::ParamsGetScript[MAX_PARAMS]={0};
bool Critter::SlotDataSendEnabled[0x100]={false};
int Critter::SlotDataSendConditionIndex[0x100]={0};
int Critter::SlotDataSendConditionMask[0x100]={0};
DWORD Critter::ParametersMin[MAX_PARAMETERS_ARRAYS]={0};
DWORD Critter::ParametersMax[MAX_PARAMETERS_ARRAYS]={MAX_PARAMS-1};
bool Critter::ParametersOffset[MAX_PARAMETERS_ARRAYS]={false};
bool Critter::SlotEnabled[0x100]={true,true,true,true,false};
Item* Critter::SlotEnabledCachePid[0x100]={NULL};
Item* Critter::SlotEnabledCacheData[0x100]={NULL};

Critter::Critter():
CritterIsNpc(false),RefCounter(1),IsNotValid(false),NameStrRefCounter(0x80000000),
GroupMove(NULL),PrevHexTick(0),PrevHexX(0),PrevHexY(0),
ItemSlotMain(&defItemSlotMain),ItemSlotExt(&defItemSlotExt),ItemSlotArmor(&defItemSlotArmor),
startBreakTime(0),breakTime(0),waitEndTick(0),KnockoutAp(0),LastHealTick(0),
Flags(0),AccessContainerId(0),TryingGoHomeTick(0),ApRegenerationTick(0),GlobalIdleNextTick(0),LockMapTransfers(0),
ViewMapId(0),ViewMapPid(0),ViewMapLook(0),ViewMapHx(0),ViewMapHy(0),ViewMapDir(0),
DisableSend(0)
{
	ZeroMemory(&Data,sizeof(Data));
	DataExt=NULL;
	ZeroMemory(FuncId,sizeof(FuncId));
	GroupSelf=new GlobalMapGroup();
	GroupSelf->WXf=Data.WorldX;
	GroupSelf->WYf=Data.WorldY;
	GroupSelf->WXi=Data.WorldX;
	GroupSelf->WYi=Data.WorldY;
	GroupSelf->MoveX=Data.WorldX;
	GroupSelf->MoveY=Data.WorldY;
	GroupSelf->Rule=this;
	ZeroMemory(ParamsIsChanged,sizeof(ParamsIsChanged));
	ParamsChanged.reserve(10);
	ParamLocked=-1;
	for(int i=0;i<MAX_PARAMETERS_ARRAYS;i++) ThisPtr[i]=this;
}

Critter::~Critter()
{
	SAFEDEL(GroupSelf);
}

CritDataExt* Critter::GetDataExt()
{
	if(!DataExt)
	{
		DataExt=new CritDataExt();
		ZeroMemory(DataExt,sizeof(CritDataExt));
		GMapFog.Create(GM__MAXZONEX,GM__MAXZONEY,DataExt->GlobalMapFog);

#ifdef MEMORY_DEBUG
		if(IsNpc())
		{
			MEMORY_PROCESS(MEMORY_NPC,sizeof(CritDataExt));
		}
		else
		{
			MEMORY_PROCESS(MEMORY_CLIENT,sizeof(CritDataExt));
		}
#endif
	}
	return DataExt;
}

void Critter::FullClear()
{
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		item->Accessory=0xB0;
		ItemMngr.ItemToGarbage(item,0x4);
	}
	invItems.clear();

	IsNotValid=true;
}

int Critter::GetLook()
{
	int look=GameOpt.LookNormal+GetParam(ST_PERCEPTION)*3+GetParam(ST_BONUS_LOOK);
	if(look<(int)GameOpt.LookMinimum) look=GameOpt.LookMinimum;
	return look;
}

DWORD Critter::GetTalkDistance()
{
	int dist=GetParam(ST_TALK_DISTANCE);
	if(dist<=0) dist=GameOpt.TalkDistance;
	return dist;
}

int Critter::GetAttackMaxDist(Item* weap, int use)
{
	if(!weap->IsWeapon()) return 0;
	int dist=weap->Proto->Weapon.MaxDist[use];
	if(weap->Proto->Weapon.Skill[use]==SKILL_OFFSET(SK_THROWING)) dist=min(dist,3*min(10,GetParam(ST_STRENGTH)+2*GetPerk(PE_HEAVE_HO)));
	if(weap->WeapIsHtHAttack(use) && IsPerk(MODE_RANGE_HTH)) dist++;
	return dist;
}

bool Critter::CheckFind(int find_type)
{
	if(IsNpc())
	{
		if(FLAG(find_type,FIND_ONLY_PLAYERS)) return false;
	}
	else
	{
		if(FLAG(find_type,FIND_ONLY_NPC)) return false;
	}
	return
		(FLAG(find_type,FIND_LIFE) && IsLife()) ||
		(FLAG(find_type,FIND_KO) && IsKnockout()) ||
		(FLAG(find_type,FIND_DEAD) && IsDead());
}

void Critter::SetLexems(const char* lexems)
{
	if(lexems)
	{
		DWORD len=strlen(lexems);
		if(!len)
		{
			Data.Lexems[0]=0;
			return;
		}

		if(len+1>=LEXEMS_SIZE) return;
		memcpy(Data.Lexems,lexems,len+1);
	}
	else
	{
		Data.Lexems[0]=0;
	}
}

void Critter::ProcessVisibleCritters()
{
	if(IsNotValid) return;

	// Global map
	if(!GetMap())
	{
		if(!GroupMove)
		{
			WriteLog(__FUNCTION__" - GroupMove nullptr, critter<%s>. Creating dump file.\n",GetInfo());
			CreateDump("ProcessVisibleCritters");
			return;
		}

		if(IsPlayer())
		{
			Client* cl=(Client*)this;
			for(CrVecIt it=GroupMove->CritMove.begin(),end=GroupMove->CritMove.end();it!=end;++it)
			{
				Critter* cr=*it;
				if(this==cr)
				{
					SETFLAG(Flags,FCRIT_CHOSEN);
					cl->Send_AddCritter(this);
					UNSETFLAG(Flags,FCRIT_CHOSEN);
					cl->Send_AllParams();
					cl->Send_AddAllItems();
				}
				else
				{
					cl->Send_AddCritter(cr);
				}
			}
		}
		return;
	}

	// Local map
	int vis;
	int look_base_self=GetLook();
	int dir_self=GetDir();
	bool show_cr1=((FuncId[CRITTER_EVENT_SHOW_CRITTER_1]>0 || FuncId[CRITTER_EVENT_HIDE_CRITTER_1]>0) && Data.ShowCritterDist1>0);
	bool show_cr2=((FuncId[CRITTER_EVENT_SHOW_CRITTER_2]>0 || FuncId[CRITTER_EVENT_HIDE_CRITTER_2]>0) && Data.ShowCritterDist2>0);
	bool show_cr3=((FuncId[CRITTER_EVENT_SHOW_CRITTER_3]>0 || FuncId[CRITTER_EVENT_HIDE_CRITTER_3]>0) && Data.ShowCritterDist3>0);
	bool show_cr=(show_cr1 || show_cr2 || show_cr3);
	// Sneak self
	int sneak_base_self=GetSkill(SK_SNEAK);
	if(FLAG(GameOpt.LookChecks,LOOK_CHECK_SNEAK_WEIGHT)) sneak_base_self-=GetItemsWeight()/GameOpt.LookWeight;

	Map* map=MapMngr.GetMap(GetMap());
	if(!map) return;

	CrVec crits=map->GetCritters();
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr==this || cr->IsNotValid) continue;

		int dist=DistGame(GetHexX(),GetHexY(),cr->GetHexX(),cr->GetHexY());

		if(FLAG(GameOpt.LookChecks,LOOK_CHECK_SCRIPT))
		{
			bool allow_self=true;
			if(Script::PrepareContext(ServerFunctions.CheckLook,CALL_FUNC_STR,GetInfo()))
			{
				Script::SetArgObject(map);
				Script::SetArgObject(this);
				Script::SetArgObject(cr);
				if(Script::RunPrepared()) allow_self=Script::GetReturnedBool();
			}
			bool allow_opp=true;
			if(Script::PrepareContext(ServerFunctions.CheckLook,CALL_FUNC_STR,GetInfo()))
			{
				Script::SetArgObject(map);
				Script::SetArgObject(cr);
				Script::SetArgObject(this);
				if(Script::RunPrepared()) allow_opp=Script::GetReturnedBool();
			}

			if(allow_self)
			{
				if(cr->AddCrIntoVisVec(this))
				{
					Send_AddCritter(cr);
					EventShowCritter(cr);
				}
			}
			else
			{
				if(cr->DelCrFromVisVec(this))
				{
					Send_RemoveCritter(cr);
					EventHideCritter(cr);
				}
			}

			if(allow_opp)
			{
				if(AddCrIntoVisVec(cr))
				{
					cr->Send_AddCritter(this);
					cr->EventShowCritter(this);
				}
			}
			else
			{
				if(DelCrFromVisVec(cr))
				{
					cr->Send_RemoveCritter(this);
					cr->EventHideCritter(this);
				}
			}

			if(show_cr)
			{
				if(show_cr1)
				{
					if(Data.ShowCritterDist1>=dist){ if(AddCrIntoVisSet1(cr->GetId())) EventShowCritter1(cr); }
					else { if(DelCrFromVisSet1(cr->GetId())) EventHideCritter1(cr); }
				}
				if(show_cr2)
				{
					if(Data.ShowCritterDist2>=dist){ if(AddCrIntoVisSet2(cr->GetId())) EventShowCritter2(cr); }
					else { if(DelCrFromVisSet2(cr->GetId())) EventHideCritter2(cr); }
				}
				if(show_cr3)
				{
					if(Data.ShowCritterDist3>=dist){ if(AddCrIntoVisSet3(cr->GetId())) EventShowCritter3(cr); }
					else { if(DelCrFromVisSet3(cr->GetId())) EventHideCritter3(cr); }
				}
			}

			if((cr->FuncId[CRITTER_EVENT_SHOW_CRITTER_1]>0 || cr->FuncId[CRITTER_EVENT_HIDE_CRITTER_1]>0) && cr->Data.ShowCritterDist1>0)
			{
				if(cr->Data.ShowCritterDist1>=dist){ if(cr->AddCrIntoVisSet1(GetId())) cr->EventShowCritter1(this); }
				else { if(cr->DelCrFromVisSet1(GetId())) cr->EventHideCritter1(this); }
			}
			if((cr->FuncId[CRITTER_EVENT_SHOW_CRITTER_2]>0 || cr->FuncId[CRITTER_EVENT_HIDE_CRITTER_2]>0) && cr->Data.ShowCritterDist2>0)
			{
				if(cr->Data.ShowCritterDist2>=dist){ if(cr->AddCrIntoVisSet2(GetId())) cr->EventShowCritter2(this); }
				else { if(cr->DelCrFromVisSet2(GetId())) cr->EventHideCritter2(this); }
			}
			if((cr->FuncId[CRITTER_EVENT_SHOW_CRITTER_3]>0 || cr->FuncId[CRITTER_EVENT_HIDE_CRITTER_3]>0) && cr->Data.ShowCritterDist3>0)
			{
				if(cr->Data.ShowCritterDist3>=dist){ if(cr->AddCrIntoVisSet3(GetId())) cr->EventShowCritter3(this); }
				else { if(cr->DelCrFromVisSet3(GetId())) cr->EventHideCritter3(this); }
			}
			continue;
		}

		int look_self=look_base_self;
		int look_opp=cr->GetLook();

		// Dir modifier
		if(FLAG(GameOpt.LookChecks,LOOK_CHECK_DIR))
		{
			// Self
			int real_dir=GetFarDir(GetHexX(),GetHexY(),cr->GetHexX(),cr->GetHexY());
			int i=(dir_self>real_dir?dir_self-real_dir:real_dir-dir_self);
			if(i>3) i=6-i;
			look_self-=look_self*GameOpt.LookDir[i]/100;
			// Opponent
			int dir_opp=cr->GetDir();
			real_dir=((real_dir+3)%6);
			i=(dir_opp>real_dir?dir_opp-real_dir:real_dir-dir_opp);
			if(i>3) i=6-i;
			look_opp-=look_opp*GameOpt.LookDir[i]/100;
		}

		if(dist>look_self && dist>look_opp) dist=MAX_INT;

		// Trace
		if(FLAG(GameOpt.LookChecks,LOOK_CHECK_TRACE) && dist!=MAX_INT)
		{
			TraceData trace;
			trace.TraceMap=map;
			trace.BeginHx=GetHexX();
			trace.BeginHy=GetHexY();
			trace.EndHx=cr->GetHexX();
			trace.EndHy=cr->GetHexY();
			MapMngr.TraceBullet(trace);
			if(!trace.IsFullTrace) dist=MAX_INT;
		}

		// Self
		if(cr->IsPerk(MODE_HIDE) && dist!=MAX_INT)
		{
			int sneak_opp=cr->GetSkill(SK_SNEAK);
			if(FLAG(GameOpt.LookChecks,LOOK_CHECK_SNEAK_WEIGHT)) sneak_opp-=cr->GetItemsWeight()/GameOpt.LookWeight;
			if(FLAG(GameOpt.LookChecks,LOOK_CHECK_SNEAK_DIR))
			{
				int real_dir=GetFarDir(GetHexX(),GetHexY(),cr->GetHexX(),cr->GetHexY());
				int i=(dir_self>real_dir?dir_self-real_dir:real_dir-dir_self);
				if(i>3) i=6-i;
				sneak_opp-=sneak_opp*GameOpt.LookSneakDir[i]/100;
			}
			sneak_opp/=(int)GameOpt.SneakDivider;
			if(sneak_opp<0) sneak_opp=0;
			vis=look_self-sneak_opp;
		}
		else
		{
			vis=look_self;
		}
		if(vis<(int)GameOpt.LookMinimum) vis=GameOpt.LookMinimum;

		if(vis>=dist)
		{
			if(cr->AddCrIntoVisVec(this))
			{
				Send_AddCritter(cr);
				EventShowCritter(cr);
			}
		}
		else
		{
			if(cr->DelCrFromVisVec(this))
			{
				Send_RemoveCritter(cr);
				EventHideCritter(cr);
			}
		}

		if(show_cr1)
		{
			if(Data.ShowCritterDist1>=dist){ if(AddCrIntoVisSet1(cr->GetId())) EventShowCritter1(cr); }
			else { if(DelCrFromVisSet1(cr->GetId())) EventHideCritter1(cr); }
		}
		if(show_cr2)
		{
			if(Data.ShowCritterDist2>=dist){ if(AddCrIntoVisSet2(cr->GetId())) EventShowCritter2(cr); }
			else { if(DelCrFromVisSet2(cr->GetId())) EventHideCritter2(cr); }
		}
		if(show_cr3)
		{
			if(Data.ShowCritterDist3>=dist){ if(AddCrIntoVisSet3(cr->GetId())) EventShowCritter3(cr); }
			else { if(DelCrFromVisSet3(cr->GetId())) EventHideCritter3(cr); }
		}

		// Opponent
		if(IsPerk(MODE_HIDE) && dist!=MAX_INT)
		{
			int sneak_self=sneak_base_self;
			if(FLAG(GameOpt.LookChecks,LOOK_CHECK_SNEAK_DIR))
			{
				int dir_opp=cr->GetDir();
				int real_dir=GetFarDir(cr->GetHexX(),cr->GetHexY(),GetHexX(),GetHexY());
				int i=(dir_opp>real_dir?dir_opp-real_dir:real_dir-dir_opp);
				if(i>3) i=6-i;
				sneak_self-=sneak_self*GameOpt.LookSneakDir[i]/100;
			}
			sneak_self/=(int)GameOpt.SneakDivider;
			if(sneak_self<0) sneak_self=0;
			vis=look_opp-sneak_self;
		}
		else
		{
			vis=look_opp;
		}
		if(vis<(int)GameOpt.LookMinimum) vis=GameOpt.LookMinimum;

		if(vis>=dist)
		{
			if(AddCrIntoVisVec(cr))
			{
				cr->Send_AddCritter(this);
				cr->EventShowCritter(this);
			}
		}
		else
		{
			if(DelCrFromVisVec(cr))
			{
				cr->Send_RemoveCritter(this);
				cr->EventHideCritter(this);
			}
		}

		if((cr->FuncId[CRITTER_EVENT_SHOW_CRITTER_1]>0 || cr->FuncId[CRITTER_EVENT_HIDE_CRITTER_1]>0) && cr->Data.ShowCritterDist1>0)
		{
			if(cr->Data.ShowCritterDist1>=dist){ if(cr->AddCrIntoVisSet1(GetId())) cr->EventShowCritter1(this); }
			else { if(cr->DelCrFromVisSet1(GetId())) cr->EventHideCritter1(this); }
		}
		if((cr->FuncId[CRITTER_EVENT_SHOW_CRITTER_2]>0 || cr->FuncId[CRITTER_EVENT_HIDE_CRITTER_2]>0) && cr->Data.ShowCritterDist2>0)
		{
			if(cr->Data.ShowCritterDist2>=dist){ if(cr->AddCrIntoVisSet2(GetId())) cr->EventShowCritter2(this); }
			else { if(cr->DelCrFromVisSet2(GetId())) cr->EventHideCritter2(this); }
		}
		if((cr->FuncId[CRITTER_EVENT_SHOW_CRITTER_3]>0 || cr->FuncId[CRITTER_EVENT_HIDE_CRITTER_3]>0) && cr->Data.ShowCritterDist3>0)
		{
			if(cr->Data.ShowCritterDist3>=dist){ if(cr->AddCrIntoVisSet3(GetId())) cr->EventShowCritter3(this); }
			else { if(cr->DelCrFromVisSet3(GetId())) cr->EventHideCritter3(this); }
		}
	}
}

void Critter::ProcessVisibleItems()
{
	Map* map=MapMngr.GetMap(GetMap());
	if(!map) return;
	int look=GetLook();
	ItemPtrVec items=map->HexItems;
	for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=*it;

		if(item->IsHidden()) continue;
		else if(item->IsAlwaysView())
		{
			if(AddIdVisItem(item->GetId()))
			{
				Send_AddItemOnMap(item);
				EventShowItemOnMap(item,item->ViewPlaceOnMap,item->ViewByCritter);
			}
		}
		else
		{
			int dist=DistGame(Data.HexX,Data.HexY,item->ACC_HEX.HexX,item->ACC_HEX.HexY);
			if(item->IsTrap()) dist+=item->TrapGetValue();

			if(look>=dist)
			{
				if(AddIdVisItem(item->GetId()))
				{
					Send_AddItemOnMap(item);
					EventShowItemOnMap(item,item->ViewPlaceOnMap,item->ViewByCritter);
				}
			}
			else
			{
				if(DelIdVisItem(item->GetId()))
				{
					Send_EraseItemFromMap(item);
					EventHideItemOnMap(item,item->ViewPlaceOnMap,item->ViewByCritter);
				}
			}
		}
	}
}

void Critter::ViewMap(Map* map, int look, WORD hx, WORD hy, int dir)
{
	Send_GameInfo(map);

	// Critters
	int vis;
	CrVec crits=map->GetCritters();
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr==this || cr->IsNotValid) continue;

		if(FLAG(GameOpt.LookChecks,LOOK_CHECK_SCRIPT))
		{
			if(Script::PrepareContext(ServerFunctions.CheckLook,CALL_FUNC_STR,GetInfo()))
			{
				Script::SetArgObject(map);
				Script::SetArgObject(this);
				Script::SetArgObject(cr);
				if(Script::RunPrepared() && Script::GetReturnedBool()) Send_AddCritter(cr);
			}
			continue;
		}

		int dist=DistGame(hx,hy,cr->Data.HexX,cr->Data.HexY);
		int look_self=look;

		// Dir modifier
		if(FLAG(GameOpt.LookChecks,LOOK_CHECK_DIR))
		{
			int real_dir=GetFarDir(hx,hy,cr->GetHexX(),cr->GetHexY());
			int i=(dir>real_dir?dir-real_dir:real_dir-dir);
			if(i>3) i=6-i;
			look_self-=look_self*GameOpt.LookDir[i]/100;
		}

		if(dist>look_self) continue;

		// Trace
		if(FLAG(GameOpt.LookChecks,LOOK_CHECK_TRACE) && dist!=MAX_INT)
		{
			TraceData trace;
			trace.TraceMap=map;
			trace.BeginHx=hx;
			trace.BeginHy=hy;
			trace.EndHx=cr->GetHexX();
			trace.EndHy=cr->GetHexY();
			MapMngr.TraceBullet(trace);
			if(!trace.IsFullTrace) continue;
		}

		// Hide modifier
		if(cr->IsPerk(MODE_HIDE))
		{
			int sneak_opp=cr->GetSkill(SK_SNEAK);
			if(FLAG(GameOpt.LookChecks,LOOK_CHECK_SNEAK_WEIGHT)) sneak_opp-=cr->GetItemsWeight()/GameOpt.LookWeight;
			if(FLAG(GameOpt.LookChecks,LOOK_CHECK_SNEAK_DIR))
			{
				int real_dir=GetFarDir(hx,hy,cr->GetHexX(),cr->GetHexY());
				int i=(dir>real_dir?dir-real_dir:real_dir-dir);
				if(i>3) i=6-i;
				sneak_opp-=sneak_opp*GameOpt.LookSneakDir[i]/100;
			}
			sneak_opp/=(int)GameOpt.SneakDivider;
			if(sneak_opp<0) sneak_opp=0;
			vis=look_self-sneak_opp;
		}
		else
		{
			vis=look_self;
		}
		if(vis<(int)GameOpt.LookMinimum) vis=GameOpt.LookMinimum;
		if(vis>=dist) Send_AddCritter(cr);
	}

	// Items
	for(ItemPtrVecIt it=map->HexItems.begin(),end=map->HexItems.end();it!=end;++it)
	{
		Item* item=*it;

		if(item->IsHidden()) continue;
		else if(item->IsAlwaysView()) Send_AddItemOnMap(item);
		else
		{
			int dist=DistGame(hx,hy,item->ACC_HEX.HexX,item->ACC_HEX.HexY);
			if(item->IsTrap()) dist+=item->TrapGetValue();

			if(look>=dist) Send_AddItemOnMap(item);
		}
	}
}

void Critter::ClearVisible()
{
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		CrVecIt it_=std::find(cr->VisCrSelf.begin(),cr->VisCrSelf.end(),this);
		if(it_!=cr->VisCrSelf.end()) cr->VisCrSelf.erase(it_);
		cr->Send_RemoveCritter(this);
	}
	VisCr.clear();

	for(CrVecIt it=VisCrSelf.begin(),end=VisCrSelf.end();it!=end;++it)
	{
		Critter* cr=*it;
		CrVecIt it_=std::find(cr->VisCr.begin(),cr->VisCr.end(),this);
		if(it_!=cr->VisCr.end()) cr->VisCr.erase(it_);
	}
	VisCrSelf.clear();

	VisCr1.clear();
	VisCr2.clear();
	VisCr3.clear();
	VisItem.clear();
}

Critter* Critter::GetCritSelf(DWORD crid)
{
	for(CrVecIt it=VisCrSelf.begin(),end=VisCrSelf.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->GetId()==crid) return cr;
	}
	return NULL;
}

void Critter::GetCrFromVisCr(CrVec& crits, int find_type)
{
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->CheckFind(find_type) && std::find(crits.begin(),crits.end(),cr)==crits.end()) crits.push_back(cr);
	}
}

void Critter::GetCrFromVisCrSelf(CrVec& crits, int find_type)
{
	for(CrVecIt it=VisCrSelf.begin(),end=VisCrSelf.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->CheckFind(find_type) && std::find(crits.begin(),crits.end(),cr)==crits.end()) crits.push_back(cr);
	}
}

bool Critter::AddCrIntoVisVec(Critter* add_cr)
{
	if(std::find(VisCr.begin(),VisCr.end(),add_cr)!=VisCr.end()) return false;
	VisCr.push_back(add_cr);
	add_cr->VisCrSelf.push_back(this);
	return true;
}

bool Critter::DelCrFromVisVec(Critter* del_cr)
{
	CrVecIt it=std::find(VisCr.begin(),VisCr.end(),del_cr);
	if(it==VisCr.end()) return false;
	VisCr.erase(it);
	it=std::find(del_cr->VisCrSelf.begin(),del_cr->VisCrSelf.end(),this);
	if(it!=del_cr->VisCrSelf.end()) del_cr->VisCrSelf.erase(it);
	return true;
}

bool Critter::AddCrIntoVisSet1(DWORD crid)
{
	if(VisCr1.count(crid)) return false;
	VisCr1.insert(crid);
	return true;
}

bool Critter::AddCrIntoVisSet2(DWORD crid)
{
	if(VisCr2.count(crid)) return false;
	VisCr2.insert(crid);
	return true;
}

bool Critter::AddCrIntoVisSet3(DWORD crid)
{
	if(VisCr3.count(crid)) return false;
	VisCr3.insert(crid);
	return true;
}

bool Critter::DelCrFromVisSet1(DWORD crid)
{
	return VisCr1.erase(crid)!=0;
}

bool Critter::DelCrFromVisSet2(DWORD crid)
{
	return VisCr2.erase(crid)!=0;
}

bool Critter::DelCrFromVisSet3(DWORD crid)
{
	return VisCr3.erase(crid)!=0;
}

#pragma MESSAGE("Optimize set checking.")
bool Critter::AddIdVisItem(DWORD item_id)
{
//	return (*VisItem.insert(item_id)).second;
	if(!VisItem.count(item_id))
	{
		VisItem.insert(item_id);
		return true;
	}
	return false;
}

bool Critter::DelIdVisItem(DWORD item_id)
{
//	return VisItem.erase(item_id);
	if(VisItem.count(item_id))
	{
		VisItem.erase(item_id);
		return true;
	}
	return false;
}

bool Critter::SetDefaultItems(ProtoItem* proto_hand1, ProtoItem* proto_hand2, ProtoItem* proto_armor)
{
	if(!proto_hand1 || !proto_hand2 || !proto_armor) return false;

	ZeroMemory(&defItemSlotMain,sizeof(Item));
	ZeroMemory(&defItemSlotExt,sizeof(Item));
	ZeroMemory(&defItemSlotArmor,sizeof(Item));

	defItemSlotMain.Init(proto_hand1);
	defItemSlotExt.Init(proto_hand2);
	defItemSlotArmor.Init(proto_armor);

	defItemSlotMain.Accessory=ITEM_ACCESSORY_CRITTER;
	defItemSlotExt.Accessory=ITEM_ACCESSORY_CRITTER;
	defItemSlotArmor.Accessory=ITEM_ACCESSORY_CRITTER;

	defItemSlotMain.ACC_CRITTER.Id=GetId();
	defItemSlotExt.ACC_CRITTER.Id=GetId();
	defItemSlotArmor.ACC_CRITTER.Id=GetId();

	defItemSlotMain.ACC_CRITTER.Slot=SLOT_HAND1;
	defItemSlotExt.ACC_CRITTER.Slot=SLOT_HAND2;
	defItemSlotArmor.ACC_CRITTER.Slot=SLOT_ARMOR;
	return true;
}

void Critter::AddItem(Item*& item, bool send)
{
	// Check
	if(!item || !item->Proto)
	{
		WriteLog(__FUNCTION__" - Item null ptr, critter<%s>.\n",GetInfo());
		return;
	}

	// Add
	if(item->IsGrouped())
	{
		Item* item_already=GetItemByPid(item->GetProtoId());
		if(item_already)
		{
			item_already->Count_Add(item->GetCount());
			ItemMngr.ItemToGarbage(item,0x8);
			item=item_already;
			if(send) SendAA_ItemData(item);
			return;
		}
	}

	item->SetSortValue(invItems);
	SetItem(item);

	// Npc battle weapon
	if((item->IsWeapon() || item->IsAmmo()) && IsNpc()) ((Npc*)this)->NullLastBattleWeapon();

	// Send
	if(send)
	{
		Send_AddItem(item);
		if(item->ACC_CRITTER.Slot!=SLOT_INV) SendAA_MoveItem(item,ACTION_REFRESH,0);
	}

	// Change item
	if(Script::PrepareContext(ServerFunctions.CritterChangeItem,CALL_FUNC_STR,GetInfo()))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(item);
		Script::SetArgByte(SLOT_GROUND);
		Script::RunPrepared();
	}
}

void Critter::SetItem(Item* item)
{
	if(!item || !item->Proto)
	{
		WriteLog(__FUNCTION__" - Item null ptr, critter<%s>.\n",GetInfo());
		return;
	}

	if(item->IsCar() && GroupMove && !GroupMove->CarId) GroupMove->CarId=item->GetId();
	invItems.push_back(item);
	item->ACC_CRITTER.Id=GetId();

	if(item->Accessory!=ITEM_ACCESSORY_CRITTER)
	{
		item->Accessory=ITEM_ACCESSORY_CRITTER;
		item->ACC_CRITTER.Slot=SLOT_INV;
	}

	switch(item->ACC_CRITTER.Slot)
	{
	case SLOT_INV:
label_InvSlot:
		item->ACC_CRITTER.Slot=SLOT_INV;
		break;
	case SLOT_HAND1:
		if(ItemSlotMain->GetId()) goto label_InvSlot;
		if(item->IsWeapon() && !CritType::IsAnim1(GetCrType(),item->Proto->Weapon.Anim1)) goto label_InvSlot;
		ItemSlotMain=item;
		break;
	case SLOT_HAND2:
		if(ItemSlotExt->GetId()) goto label_InvSlot;
		ItemSlotExt=item;
		break;
	case SLOT_ARMOR:
		if(ItemSlotArmor->GetId()) goto label_InvSlot;
		if(!item->IsArmor()) goto label_InvSlot;
		if(!CritType::IsCanArmor(GetCrType())) goto label_InvSlot;
		ItemSlotArmor=item;
		break;
	default:
		break;
	}
}

void Critter::EraseItem(Item* item, bool send)
{
	if(!item)
	{
		WriteLog(__FUNCTION__" - Item null ptr, critter<%s>.\n",GetInfo());
		return;
	}

	ItemPtrVecIt it=std::find(invItems.begin(),invItems.end(),item);
	if(it!=invItems.end()) invItems.erase(it);
	else WriteLog(__FUNCTION__" - Item not found, id<%u>, pid<%u>, critter<%s>.\n",item->GetId(),item->GetProtoId(),GetInfo());

	if(!GetMap() && GroupMove && GroupMove->CarId==item->GetId())
	{
		GroupMove->CarId=0;
		SendA_GlobalInfo(GroupMove,GM_INFO_GROUP_PARAM);
	}

	item->Accessory=0xd0;
	TakeDefaultItem(item->ACC_CRITTER.Slot);
	if(send) Send_EraseItem(item);
	if(item->ACC_CRITTER.Slot!=SLOT_INV) SendAA_MoveItem(item,ACTION_REFRESH,0);

	BYTE from_slot=item->ACC_CRITTER.Slot;
	item->ACC_CRITTER.Slot=SLOT_GROUND;
	if(Script::PrepareContext(ServerFunctions.CritterChangeItem,CALL_FUNC_STR,GetInfo()))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(item);
		Script::SetArgByte(from_slot);
		Script::RunPrepared();
	}
}

Item* Critter::GetItem(DWORD item_id, bool skip_hide)
{
	if(!item_id) return NULL;
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
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

Item* Critter::GetInvItem(DWORD item_id, int transfer_type)
{
	if(transfer_type==TRANSFER_CRIT_LOOT && IsPerk(MODE_NO_LOOT)) return NULL;
	if(transfer_type==TRANSFER_CRIT_STEAL && IsPerk(MODE_NO_STEAL)) return NULL;

	Item* item=GetItem(item_id,true);
	if(!item || item->ACC_CRITTER.Slot!=SLOT_INV ||
		(transfer_type==TRANSFER_CRIT_LOOT && item->IsNoLoot()) ||
		(transfer_type==TRANSFER_CRIT_STEAL && item->IsNoSteal())) return NULL;
	return item;
}

void Critter::GetInvItems(ItemPtrVec& items, int transfer_type)
{
	if(transfer_type==TRANSFER_CRIT_LOOT && IsPerk(MODE_NO_LOOT)) return;
	if(transfer_type==TRANSFER_CRIT_STEAL && IsPerk(MODE_NO_STEAL)) return;

	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		if(item->ACC_CRITTER.Slot==SLOT_INV && !item->IsHidden() &&
			!(transfer_type==TRANSFER_CRIT_LOOT && item->IsNoLoot()) &&
			!(transfer_type==TRANSFER_CRIT_STEAL && item->IsNoSteal())) items.push_back(item);
	}
}

Item* Critter::GetItemByPid(WORD item_pid)
{
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		if(item->GetProtoId()==item_pid) return item;
	}
	return NULL;
}

Item* Critter::GetItemByPidInvPriority(WORD item_pid)
{
	ProtoItem* proto_item=ItemMngr.GetProtoItem(item_pid);
	if(!proto_item) return NULL;

	if(proto_item->IsGrouped())
	{
		for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
		{
			Item* item=*it;
			if(item->GetProtoId()==item_pid) return item;
		}
	}
	else
	{
		Item* another_slot=NULL;
		for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
		{
			Item* item=*it;
			if(item->GetProtoId()==item_pid)
			{
				if(item->ACC_CRITTER.Slot==SLOT_INV) return item;
				another_slot=item;
			}
		}
		return another_slot;
	}
	return NULL;
}

Item* Critter::GetAmmoForWeapon(Item* weap)
{
	if(!weap->IsWeapon()) return NULL;

	Item* ammo=GetItemByPid(weap->Data.TechInfo.AmmoPid);
	if(ammo) return ammo;
	ammo=GetItemByPid(weap->Proto->Weapon.DefAmmo);
	if(ammo) return ammo;
	ammo=GetAmmo(weap->Proto->Weapon.Caliber);
	return ammo;
}

Item* Critter::GetAmmo(DWORD caliber)
{
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		if(item->GetType()==ITEM_TYPE_AMMO && item->Proto->Ammo.Caliber==caliber) return item;
	}
	return NULL;
}

Item* Critter::GetItemCar()
{
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		if(item->IsCar()) return item;
	}
	return NULL;
}

Item* Critter::GetItemSlot(int slot)
{
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		if(item->ACC_CRITTER.Slot==slot) return item;
	}
	return NULL;
}

void Critter::GetItemsSlot(int slot, ItemPtrVec& items)
{
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		if(slot<0 || item->ACC_CRITTER.Slot==slot) items.push_back(item);
	}
}

void Critter::GetItemsType(int type, ItemPtrVec& items)
{
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		if(item->GetType()==type) items.push_back(item);
	}
}

DWORD Critter::CountItemPid(WORD pid)
{
	DWORD res=0;
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		if(item->GetProtoId()==pid) res+=item->GetCount();
	}
	return res;
}

bool Critter::MoveItem(BYTE from_slot, BYTE to_slot, DWORD item_id, DWORD count)
{
	if(!item_id)
	{
		switch(from_slot)
		{
		case SLOT_HAND1: item_id=ItemSlotMain->GetId(); break;
		case SLOT_HAND2: item_id=ItemSlotExt->GetId(); break;
		case SLOT_ARMOR: item_id=ItemSlotArmor->GetId(); break;
		default: break;
		}
	}

	if(!item_id)
	{
		WriteLog(__FUNCTION__" - Item id is zero, from slot<%u>, to slot<%u>, critter<%s>.\n",from_slot,to_slot,GetInfo());
		return false;
	}

	Item* item=GetItem(item_id,IsPlayer());
	if(!item)
	{
		WriteLog(__FUNCTION__" - Item not found, item id<%u>, critter<%s>.\n",item_id,GetInfo());
		return false;
	}

	if(item->ACC_CRITTER.Slot!=from_slot || from_slot==to_slot)
	{
		WriteLog(__FUNCTION__" - Wrong slots, from slot<%u>, from slot real<%u>, to slot<%u>, item id<%u>, critter<%s>.\n",from_slot,item->ACC_CRITTER.Slot,to_slot,item_id,GetInfo());
		return false;
	}

	if(to_slot==SLOT_GROUND)
	{
		if(!count || count>item->GetCount())
		{
			Send_AddItem(item); // Refresh
			return false;
		}

		bool full_drop=(!item->IsGrouped() || count>=item->GetCount());
		if(!full_drop)
		{
			if(GetMap())
			{
				Item* item_=ItemMngr.SplitItem(item,count);
				if(!item_)
				{
					Send_AddItem(item); // Refresh
					return false;
				}
				SendAA_ItemData(item);
				item=item_;
			}
			else
			{
				item->Count_Sub(count);
				SendAA_ItemData(item);
				item=NULL;
			}
		}
		else
		{
			EraseItem(item,false);
			if(!GetMap())
			{
				//item->EventDrop(GetScriptContext(),this);
				ItemMngr.ItemToGarbage(item,0x10);
				item=NULL;
			}
		}

		if(!item) return true;

		Map* map=MapMngr.GetMap(GetMap());
		if(!map)
		{
			WriteLog(__FUNCTION__" - Map not found, map id<%u>, critter<%s>.\n",GetMap(),GetInfo());
			ItemMngr.ItemToGarbage(item,0x20);
			return true;
		}

		SendAA_Action(ACTION_DROP_ITEM,from_slot,item);
		item->ViewByCritter=this;
		map->AddItem(item,GetHexX(),GetHexY(),true);
		item->ViewByCritter=NULL;
		item->EventDrop(this);
		EventDropItem(item);
		return true;
	}

	bool is_castling=((from_slot==SLOT_HAND1 && to_slot==SLOT_HAND2) || (from_slot==SLOT_HAND2 && to_slot==SLOT_HAND1));

	if(!SlotEnabled[to_slot])
	{
		WriteLog(__FUNCTION__" - Slot<%u> is not allowed, critter<%s>.\n",to_slot,GetInfo());
		return false;
	}

	if(to_slot!=SLOT_INV && !is_castling && GetItemSlot(to_slot))
	{
		WriteLog(__FUNCTION__" - To slot is busy, critter<%s>.\n",GetInfo());
		return false;
	}

	if(to_slot>SLOT_ARMOR && to_slot!=item->Proto->Slot)
	{
		WriteLog(__FUNCTION__" - Wrong slot<%u> for item pid<%u>, critter<%s>.\n",to_slot,item->GetProtoId(),GetInfo());
		return false;
	}

	if(to_slot==SLOT_HAND1 && item->IsWeapon() && !CritType::IsAnim1(GetCrType(),item->Proto->Weapon.Anim1))
	{
		WriteLog(__FUNCTION__" - Critter<%s> not have animations for anim1 index<%u>.\n",GetInfo(),item->Proto->Weapon.Anim1);
		return false;
	}

	if(to_slot==SLOT_ARMOR && (!item->IsArmor() || item->Proto->Slot || !CritType::IsCanArmor(GetCrType())))
	{
		WriteLog(__FUNCTION__" - Critter<%s> can't change armor.\n",GetInfo());
		return false;
	}

	if(is_castling && ItemSlotMain->GetId() && ItemSlotExt->GetId())
	{
		Item* tmp_item=ItemSlotExt;
		ItemSlotExt=ItemSlotMain;
		ItemSlotMain=tmp_item;
		ItemSlotExt->ACC_CRITTER.Slot=SLOT_HAND2;
		ItemSlotMain->ACC_CRITTER.Slot=SLOT_HAND1;
		SendAA_MoveItem(ItemSlotMain,ACTION_SHOW_ITEM,SLOT_HAND2);
		ItemSlotMain->EventMove(this,SLOT_HAND2);
		//ItemSlotExt->EventMove(this,SLOT_HAND1);
		EventMoveItem(ItemSlotMain,SLOT_HAND2);
		//EventMoveItem(ItemSlotExt,SLOT_HAND1);
	}
	else
	{
		int action=ACTION_MOVE_ITEM;
		if(to_slot==SLOT_HAND1) action=ACTION_SHOW_ITEM;
		else if(from_slot==SLOT_HAND1) action=ACTION_HIDE_ITEM;

		TakeDefaultItem(from_slot);
		if(to_slot==SLOT_HAND1) ItemSlotMain=item;
		else if(to_slot==SLOT_HAND2) ItemSlotExt=item;
		else if(to_slot==SLOT_ARMOR) ItemSlotArmor=item;
		item->ACC_CRITTER.Slot=to_slot;

		SendAA_MoveItem(item,action,from_slot);
		item->EventMove(this,from_slot);
		EventMoveItem(item,from_slot);
	}

	return true;
}

void Critter::TakeDefaultItem(BYTE slot)
{
	switch(slot)
	{
	case SLOT_HAND1: ItemSlotMain=&defItemSlotMain; break;
	case SLOT_HAND2: ItemSlotExt=&defItemSlotExt; break;
	case SLOT_ARMOR: ItemSlotArmor=&defItemSlotArmor; break;
	default: break;
	}
}

DWORD Critter::CountItems()
{
	DWORD count=0;
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
		count+=(*it)->GetCount();
	return count;
}

bool Critter::InHandsOnlyHtHWeapon()
{
	return (!ItemSlotMain->GetId() || !ItemSlotMain->IsWeapon() || ItemSlotMain->WeapIsHtHAttack(USE_PRIMARY)) && (!ItemSlotExt->GetId() || !ItemSlotExt->IsWeapon() || ItemSlotExt->WeapIsHtHAttack(USE_PRIMARY));
}

bool Critter::IsHaveGeckItem()
{
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
		if((*it)->IsGeck()) return true;
	return false;
}

void Critter::ToKnockout(bool face_up, DWORD lose_ap, WORD knock_hx, WORD knock_hy)
{
	Data.Cond=COND_KNOCKOUT;
	Data.CondExt=(face_up?COND_KNOCKOUT_FRONT:COND_KNOCKOUT_BACK);
	KnockoutAp=lose_ap;
	Send_Knockout(this,face_up,knock_hx,knock_hy);
	SendA_Knockout(face_up,knock_hx,knock_hy);
}

void Critter::TryUpOnKnockout()
{
	if(GetParam(ST_CURRENT_HP)<=0) return;
	if(KnockoutAp)
	{
		int cur_ap=GetAp();
		if(cur_ap<=0) return;
		int ap=min(KnockoutAp,cur_ap);
		SubAp(ap);
		KnockoutAp-=ap;
		if(KnockoutAp) return;
	}
	else if(GetAp()<0) return;
	Data.Cond=COND_LIFE;
	Data.CondExt=COND_LIFE_NONE;
	SendAA_Action(ACTION_STANDUP,0,NULL);
	SetBreakTime(GameOpt.Breaktime);
}

void Critter::ToDead(BYTE dead_type, bool send_all)
{
	bool already_dead=IsDead();

	if(GetParam(ST_CURRENT_HP)>0) Data.Params[ST_CURRENT_HP]=0;
	Data.Cond=COND_DEAD;
	Data.CondExt=dead_type;

	Item* item=ItemSlotMain;
	if(item->GetId()) MoveItem(SLOT_HAND1,SLOT_INV,item->GetId(),item->GetCount());
	item=ItemSlotExt;
	if(item->GetId()) MoveItem(SLOT_HAND2,SLOT_INV,item->GetId(),item->GetCount());
	//if(ItemSlotArmor->GetId()) MoveItem(SLOT_ARMOR,SLOT_INV,ItemSlotArmor->GetId(),ItemSlotArmor->GetCount());

	if(send_all)
	{
		SendAA_Action(ACTION_DEAD,dead_type,NULL);
		if(IsPlayer()) Send_AllParams();
	}

	if(!already_dead)
	{
		Map* map=MapMngr.GetMap(GetMap());
		if(map)
		{
			map->UnSetFlagCritter(GetHexX(),GetHexY(),false);
			map->SetFlagCritter(GetHexX(),GetHexY(),true);
		}
	}
}

void Critter::Heal()
{
	if(IsDead() || IsPerk(MODE_NO_HEAL) || GetTimeout(TO_BATTLE) || GetParam(ST_CURRENT_HP)>=GetParam(ST_MAX_LIFE))
	{
		LastHealTick=Timer::FastTick();
		return;
	}

	if(Timer::FastTick()-LastHealTick<CRIT_HEAL_TIME) return;

	ChangeParam(ST_CURRENT_HP);
	Data.Params[ST_CURRENT_HP]+=GetParam(ST_HEALING_RATE);
	LastHealTick=Timer::FastTick();
}

bool Critter::ParseScript(const char* script, bool first_time)
{
	if(script)
	{
		DWORD func_num=Script::GetScriptFuncNum(script,"void %s(Critter&,bool)");
		if(!func_num)
		{
			WriteLog(__FUNCTION__" - Script<%s> bind fail, critter<%s>.\n",script,GetInfo());
			return false;
		}
		Data.ScriptId=func_num;
	}

	if(Data.ScriptId && Script::PrepareContext(Script::GetScriptFuncBindId(Data.ScriptId),CALL_FUNC_STR,GetInfo()))
	{
		Script::SetArgObject(this);
		Script::SetArgBool(first_time);
		Script::RunPrepared();
	}
	return true;
}

bool Critter::PrepareScriptFunc(int num_scr_func)
{
	if(num_scr_func>=CRITTER_EVENT_MAX) return false;
	if(FuncId[num_scr_func]<=0) return false;
	return Script::PrepareContext(FuncId[num_scr_func],CALL_FUNC_STR,GetInfo());
}

void Critter::EventIdle()
{
	if(!PrepareScriptFunc(CRITTER_EVENT_IDLE)) return;
	Script::SetArgObject(this);
	Script::RunPrepared();
}

void Critter::EventFinish(bool deleted)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_FINISH)) return;
	Script::SetArgObject(this);
	Script::SetArgBool(deleted);
	Script::RunPrepared();
}

void Critter::EventDead(Critter* killer)
{
	if(PrepareScriptFunc(CRITTER_EVENT_DEAD))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(killer);
		Script::RunPrepared();
	}

	CrVec crits=VisCr;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->FuncId[CRITTER_EVENT_SMTH_DEAD]>0) cr->EventSmthDead(this,killer);
	}
}

void Critter::EventRespawn()
{
	if(!PrepareScriptFunc(CRITTER_EVENT_RESPAWN)) return;
	Script::SetArgObject(this);
	Script::RunPrepared();
}

void Critter::EventShowCritter(Critter* cr)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SHOW_CRITTER)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(cr);
	Script::RunPrepared();
}

void Critter::EventShowCritter1(Critter* cr)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SHOW_CRITTER_1)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(cr);
	Script::RunPrepared();
}

void Critter::EventShowCritter2(Critter* cr)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SHOW_CRITTER_2)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(cr);
	Script::RunPrepared();
}

void Critter::EventShowCritter3(Critter* cr)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SHOW_CRITTER_3)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(cr);
	Script::RunPrepared();
}

void Critter::EventHideCritter(Critter* cr)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_HIDE_CRITTER)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(cr);
	Script::RunPrepared();
}

void Critter::EventHideCritter1(Critter* cr)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_HIDE_CRITTER_1)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(cr);
	Script::RunPrepared();
}

void Critter::EventHideCritter2(Critter* cr)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_HIDE_CRITTER_2)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(cr);
	Script::RunPrepared();
}

void Critter::EventHideCritter3(Critter* cr)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_HIDE_CRITTER_3)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(cr);
	Script::RunPrepared();
}

void Critter::EventShowItemOnMap(Item* item, bool added, Critter* dropper)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SHOW_ITEM_ON_MAP)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(item);
	Script::SetArgBool(added);
	Script::SetArgObject(dropper);
	Script::RunPrepared();
}

void Critter::EventChangeItemOnMap(Item* item)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_CHANGE_ITEM_ON_MAP)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(item);
	Script::RunPrepared();
}

void Critter::EventHideItemOnMap(Item* item, bool removed, Critter* picker)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_HIDE_ITEM_ON_MAP)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(item);
	Script::SetArgBool(removed);
	Script::SetArgObject(picker);
	Script::RunPrepared();
}

bool Critter::EventAttack(Critter* target)
{
	bool result=false;
	if(PrepareScriptFunc(CRITTER_EVENT_ATTACK))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(target);
		if(Script::RunPrepared()) result=Script::GetReturnedBool();
	}

	CrVec crits=VisCr;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->FuncId[CRITTER_EVENT_SMTH_ATTACK]>0) cr->EventSmthAttack(this,target);
	}
	return result;
}

bool Critter::EventAttacked(Critter* attacker)
{
	bool result=false;
	if(PrepareScriptFunc(CRITTER_EVENT_ATTACKED))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(attacker);
		if(Script::RunPrepared()) result=Script::GetReturnedBool();
	}

	CrVec crits=VisCr;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->FuncId[CRITTER_EVENT_SMTH_ATTACKED]>0) cr->EventSmthAttacked(this,attacker);
	}

	if(!result && attacker)
	{
		if(Script::PrepareContext(ServerFunctions.CritterAttacked,CALL_FUNC_STR,GetInfo()))
		{
			Script::SetArgObject(this);
			Script::SetArgObject(attacker);
			Script::RunPrepared();
		}
	}
	return result;
}

bool Critter::EventStealing(Critter* thief, Item* item, DWORD count)
{
	bool success=false;
	if(Script::PrepareContext(ServerFunctions.CritterStealing,CALL_FUNC_STR,GetInfo()))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(thief);
		Script::SetArgObject(item);
		Script::SetArgDword(count);
		if(Script::RunPrepared()) success=Script::GetReturnedBool();
	}

	if(PrepareScriptFunc(CRITTER_EVENT_STEALING))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(thief);
		Script::SetArgBool(success);
		Script::SetArgObject(item);
		Script::SetArgDword(count);
		Script::RunPrepared();
	}

	CrVec crits=VisCr;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->FuncId[CRITTER_EVENT_SMTH_STEALING]>0) cr->EventSmthStealing(this,thief,success,item,count);
	}

	return success;
}

void Critter::EventMessage(Critter* from_cr, int num, int val)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_MESSAGE)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(from_cr);
	Script::SetArgDword(num);
	Script::SetArgDword(val);
	Script::RunPrepared();
}

bool Critter::EventUseItem(Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery)
{
	bool result=false;
	if(PrepareScriptFunc(CRITTER_EVENT_USE_ITEM))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(item);
		Script::SetArgObject(on_critter);
		Script::SetArgObject(on_item);
		Script::SetArgObject(on_scenery);
		if(Script::RunPrepared()) result=Script::GetReturnedBool();
	}

	CrVec crits=VisCr;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->FuncId[CRITTER_EVENT_SMTH_USE_ITEM]>0) cr->EventSmthUseItem(this,item,on_critter,on_item,on_scenery);
	}

	return result;
}

bool Critter::EventUseSkill(int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery)
{
	bool result=false;
	if(PrepareScriptFunc(CRITTER_EVENT_USE_SKILL))
	{
		Script::SetArgObject(this);
		Script::SetArgDword(skill<0?skill:SKILL_OFFSET(skill));
		Script::SetArgObject(on_critter);
		Script::SetArgObject(on_item);
		Script::SetArgObject(on_scenery);
		if(Script::RunPrepared()) result=Script::GetReturnedBool();
	}

	CrVec crits=VisCr;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->FuncId[CRITTER_EVENT_SMTH_USE_SKILL]>0) cr->EventSmthUseSkill(this,skill,on_critter,on_item,on_scenery);
	}

	return result;
}

void Critter::EventDropItem(Item* item)
{
	if(PrepareScriptFunc(CRITTER_EVENT_DROP_ITEM))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(item);
		Script::RunPrepared();
	}

	CrVec crits=VisCr;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->FuncId[CRITTER_EVENT_SMTH_DROP_ITEM]>0) cr->EventSmthDropItem(this,item);
	}
}

void Critter::EventMoveItem(Item* item, BYTE from_slot)
{
	if(Script::PrepareContext(ServerFunctions.CritterChangeItem,CALL_FUNC_STR,GetInfo()))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(item);
		Script::SetArgByte(from_slot);
		Script::RunPrepared();
	}

	if(PrepareScriptFunc(CRITTER_EVENT_MOVE_ITEM))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(item);
		Script::SetArgByte(from_slot);
		Script::RunPrepared();
	}

	CrVec crits=VisCr;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->FuncId[CRITTER_EVENT_SMTH_MOVE_ITEM]>0) cr->EventSmthMoveItem(this,item,from_slot);
	}
}

void Critter::EventKnockout(bool face_up, DWORD lost_ap, DWORD knock_dist)
{
	if(PrepareScriptFunc(CRITTER_EVENT_KNOCKOUT))
	{
		Script::SetArgObject(this);
		Script::SetArgBool(face_up);
		Script::SetArgDword(lost_ap);
		Script::SetArgDword(knock_dist);
		Script::RunPrepared();
	}

	CrVec crits=VisCr;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->FuncId[CRITTER_EVENT_SMTH_KNOCKOUT]>0) cr->EventSmthKnockout(this,face_up,lost_ap,knock_dist);
	}
}

void Critter::EventSmthDead(Critter* from_cr, Critter* killer)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SMTH_DEAD)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(from_cr);
	Script::SetArgObject(killer);
	Script::RunPrepared();
}

void Critter::EventSmthStealing(Critter* from_cr, Critter* thief, bool success, Item* item, DWORD count)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SMTH_STEALING)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(from_cr);
	Script::SetArgObject(thief);
	Script::SetArgBool(success);
	Script::SetArgObject(item);
	Script::SetArgDword(count);
	Script::RunPrepared();
}

void Critter::EventSmthAttack(Critter* from_cr, Critter* target)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SMTH_ATTACK)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(from_cr);
	Script::SetArgObject(target);
	Script::RunPrepared();
}

void Critter::EventSmthAttacked(Critter* from_cr, Critter* attacker)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SMTH_ATTACKED)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(from_cr);
	Script::SetArgObject(attacker);
	Script::RunPrepared();
}

void Critter::EventSmthUseItem(Critter* from_cr, Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SMTH_USE_ITEM)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(from_cr);
	Script::SetArgObject(item);
	Script::SetArgObject(on_critter);
	Script::SetArgObject(on_item);
	Script::SetArgObject(on_scenery);
	Script::RunPrepared();
}

void Critter::EventSmthUseSkill(Critter* from_cr, int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SMTH_USE_SKILL)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(from_cr);
	Script::SetArgDword(skill<0?skill:SKILL_OFFSET(skill));
	Script::SetArgObject(on_critter);
	Script::SetArgObject(on_item);
	Script::SetArgObject(on_scenery);
	Script::RunPrepared();
}

void Critter::EventSmthDropItem(Critter* from_cr, Item* item)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SMTH_DROP_ITEM)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(from_cr);
	Script::SetArgObject(item);
	Script::RunPrepared();
}

void Critter::EventSmthMoveItem(Critter* from_cr, Item* item, BYTE from_slot)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SMTH_MOVE_ITEM)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(from_cr);
	Script::SetArgObject(item);
	Script::SetArgByte(from_slot);
	Script::RunPrepared();
}

void Critter::EventSmthKnockout(Critter* from_cr, bool face_up, DWORD lost_ap, DWORD knock_dist)
{
	if(!PrepareScriptFunc(CRITTER_EVENT_SMTH_KNOCKOUT)) return;
	Script::SetArgObject(this);
	Script::SetArgObject(from_cr);
	Script::SetArgBool(face_up);
	Script::SetArgDword(lost_ap);
	Script::SetArgDword(knock_dist);
	Script::RunPrepared();
}

int Critter::EventPlaneBegin(AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item)
{
	if(PrepareScriptFunc(CRITTER_EVENT_PLANE_BEGIN))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(plane);
		Script::SetArgDword(reason);
		Script::SetArgObject(some_cr);
		Script::SetArgObject(some_item);
		if(Script::RunPrepared()) return Script::GetReturnedDword();
	}
	return PLANE_RUN_GLOBAL;
}

int Critter::EventPlaneEnd(AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item)
{
	if(PrepareScriptFunc(CRITTER_EVENT_PLANE_END))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(plane);
		Script::SetArgDword(reason);
		Script::SetArgObject(some_cr);
		Script::SetArgObject(some_item);
		if(Script::RunPrepared()) return Script::GetReturnedDword();
	}
	return PLANE_RUN_GLOBAL;
}

int Critter::EventPlaneRun(AIDataPlane* plane, int reason, DWORD& p0, DWORD& p1, DWORD& p2)
{
	if(PrepareScriptFunc(CRITTER_EVENT_PLANE_RUN))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(plane);
		Script::SetArgDword(reason);
		Script::SetArgAddress(&p0);
		Script::SetArgAddress(&p1);
		Script::SetArgAddress(&p2);
		if(Script::RunPrepared()) return Script::GetReturnedDword();
	}
	return PLANE_RUN_GLOBAL;
}

bool Critter::EventBarter(Critter* cr_barter, bool attach, DWORD barter_count)
{
	if(FuncId[CRITTER_EVENT_BARTER]<=0) return true;
	if(!PrepareScriptFunc(CRITTER_EVENT_BARTER)) return false;
	Script::SetArgObject(this);
	Script::SetArgObject(cr_barter);
	Script::SetArgBool(attach);
	Script::SetArgDword(barter_count);
	if(Script::RunPrepared()) return Script::GetReturnedBool();
	return false;
}

bool Critter::EventTalk(Critter* cr_talk, bool attach, DWORD talk_count)
{
	if(FuncId[CRITTER_EVENT_TALK]<=0) return true;
	if(!PrepareScriptFunc(CRITTER_EVENT_TALK)) return false;
	Script::SetArgObject(this);
	Script::SetArgObject(cr_talk);
	Script::SetArgBool(attach);
	Script::SetArgDword(talk_count);
	if(Script::RunPrepared()) return Script::GetReturnedBool();
	return false;
}

bool Critter::EventGlobalProcess(int type, asIScriptArray* group, Item* car, DWORD& x, DWORD& y, DWORD& to_x, DWORD& to_y, DWORD& speed, DWORD& encounter_descriptor, bool& wait_for_answer)
{
	bool result=false;
	if(PrepareScriptFunc(CRITTER_EVENT_GLOBAL_PROCESS))
	{
		Script::SetArgObject(this);
		Script::SetArgDword(type);
		Script::SetArgObject(group);
		Script::SetArgObject(car);
		Script::SetArgAddress(&x);
		Script::SetArgAddress(&y);
		Script::SetArgAddress(&to_x);
		Script::SetArgAddress(&to_y);
		Script::SetArgAddress(&speed);
		Script::SetArgAddress(&encounter_descriptor);
		Script::SetArgAddress(&wait_for_answer);
		if(Script::RunPrepared()) result=Script::GetReturnedBool();
	}
	return result;
}

bool Critter::EventGlobalInvite(asIScriptArray* group, Item* car, DWORD encounter_descriptor, int combat_mode, DWORD& map_id, WORD& hx, WORD& hy, BYTE& dir)
{
	bool result=false;
	if(PrepareScriptFunc(CRITTER_EVENT_GLOBAL_INVITE))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(group);
		Script::SetArgObject(car);
		Script::SetArgDword(encounter_descriptor);
		Script::SetArgDword(combat_mode);
		Script::SetArgAddress(&map_id);
		Script::SetArgAddress(&hx);
		Script::SetArgAddress(&hy);
		Script::SetArgAddress(&dir);
		if(Script::RunPrepared()) result=Script::GetReturnedBool();
	}
	return result;
}

void Critter::EventTurnBasedProcess(Map* map, bool begin_turn)
{
	if(PrepareScriptFunc(CRITTER_EVENT_TURN_BASED_PROCESS))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(map);
		Script::SetArgBool(begin_turn);
		Script::RunPrepared();
	}

	CrVec crits=VisCr;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->FuncId[CRITTER_EVENT_SMTH_TURN_BASED_PROCESS]>0) cr->EventSmthTurnBasedProcess(this,map,begin_turn);
	}
}

void Critter::EventSmthTurnBasedProcess(Critter* from_cr, Map* map, bool begin_turn)
{
	if(PrepareScriptFunc(CRITTER_EVENT_SMTH_TURN_BASED_PROCESS))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(from_cr);
		Script::SetArgObject(map);
		Script::RunPrepared();
	}
}

void Critter::Send_Move(Critter* from_cr, WORD move_params){if(IsPlayer()) ((Client*)this)->Send_Move(from_cr,move_params);}
void Critter::Send_Dir(Critter* from_cr){if(IsPlayer()) ((Client*)this)->Send_Dir(from_cr);}
void Critter::Send_AddCritter(Critter* cr){if(IsPlayer()) ((Client*)this)->Send_AddCritter(cr);}
void Critter::Send_RemoveCritter(Critter* cr){if(IsPlayer()) ((Client*)this)->Send_RemoveCritter(cr);}
void Critter::Send_LoadMap(Map* map){if(IsPlayer()) ((Client*)this)->Send_LoadMap(map);}
void Critter::Send_XY(Critter* cr){if(IsPlayer()) ((Client*)this)->Send_XY(cr);}
void Critter::Send_AddItemOnMap(Item* item){if(IsPlayer()) ((Client*)this)->Send_AddItemOnMap(item);}
void Critter::Send_ChangeItemOnMap(Item* item){if(IsPlayer()) ((Client*)this)->Send_ChangeItemOnMap(item);}
void Critter::Send_EraseItemFromMap(Item* item){if(IsPlayer()) ((Client*)this)->Send_EraseItemFromMap(item);}
void Critter::Send_AnimateItem(Item* item, BYTE from_frm, BYTE to_frm){if(IsPlayer()) ((Client*)this)->Send_AnimateItem(item,from_frm,to_frm);}
void Critter::Send_AddItem(Item* item){if(IsPlayer()) ((Client*)this)->Send_AddItem(item);}
void Critter::Send_EraseItem(Item* item){if(IsPlayer()) ((Client*)this)->Send_EraseItem(item);}
void Critter::Send_ContainerInfo(){if(IsPlayer()) ((Client*)this)->Send_ContainerInfo();}
void Critter::Send_ContainerInfo(Item* item_cont, BYTE transfer_type, bool open_screen){if(IsPlayer()) ((Client*)this)->Send_ContainerInfo(item_cont,transfer_type,open_screen);}
void Critter::Send_ContainerInfo(Critter* cr_cont, BYTE transfer_type, bool open_screen){if(IsPlayer()) ((Client*)this)->Send_ContainerInfo(cr_cont,transfer_type,open_screen);}
void Critter::Send_GlobalInfo(BYTE flags){if(IsPlayer()) ((Client*)this)->Send_GlobalInfo(flags);}
void Critter::Send_GlobalLocation(Location* loc, bool add){if(IsPlayer()) ((Client*)this)->Send_GlobalLocation(loc,add);}
void Critter::Send_GlobalMapFog(WORD zx, WORD zy, BYTE fog){if(IsPlayer()) ((Client*)this)->Send_GlobalMapFog(zx,zy,fog);}
void Critter::Send_AllParams(){if(IsPlayer()) ((Client*)this)->Send_AllParams();}
void Critter::Send_Param(WORD num_param){if(IsPlayer()) ((Client*)this)->Send_Param(num_param);}
void Critter::Send_ParamOther(WORD num_param, int val){if(IsPlayer()) ((Client*)this)->Send_ParamOther(num_param,val);}
void Critter::Send_CritterParam(Critter* cr, WORD num_param, int other_val){if(IsPlayer()) ((Client*)this)->Send_CritterParam(cr,num_param,other_val);}
void Critter::Send_Talk(){if(IsPlayer()) ((Client*)this)->Send_Talk();}
void Critter::Send_GameInfo(Map* map){if(IsPlayer()) ((Client*)this)->Send_GameInfo(map);}
void Critter::Send_Text(Critter* from_cr, const char* s_str, BYTE how_say){if(IsPlayer()) ((Client*)this)->Send_Text(from_cr,s_str,how_say);}
void Critter::Send_TextEx(DWORD from_id, const char* s_str, WORD str_len, BYTE how_say, WORD intellect, bool unsafe_text){if(IsPlayer()) ((Client*)this)->Send_TextEx(from_id,s_str,str_len,how_say,intellect,unsafe_text);}
void Critter::Send_TextMsg(Critter* from_cr, DWORD str_num, BYTE how_say, WORD num_msg){if(IsPlayer()) ((Client*)this)->Send_TextMsg(from_cr,str_num,how_say,num_msg);}
void Critter::Send_TextMsgLex(Critter* from_cr, DWORD num_str, BYTE how_say, WORD num_msg, const char* lexems){if(IsPlayer()) ((Client*)this)->Send_TextMsgLex(from_cr,num_str,how_say,num_msg,lexems);}
void Critter::Send_Action(Critter* from_cr, int action, int action_ext, Item* item){if(IsPlayer()) ((Client*)this)->Send_Action(from_cr,action,action_ext,item);}
void Critter::Send_Knockout(Critter* from_cr, bool face_up, WORD knock_hx, WORD knock_hy){if(IsPlayer()) ((Client*)this)->Send_Knockout(from_cr,face_up,knock_hx,knock_hy);}
void Critter::Send_MoveItem(Critter* from_cr, Item* item, BYTE action, BYTE prev_slot){if(IsPlayer()) ((Client*)this)->Send_MoveItem(from_cr,item,action,prev_slot);}
void Critter::Send_ItemData(Critter* from_cr, BYTE slot, Item* item, bool full){if(IsPlayer()) ((Client*)this)->Send_ItemData(from_cr,slot,item,full);}
void Critter::Send_Animate(Critter* from_cr, DWORD anim1, DWORD anim2, Item* item, bool clear_sequence, bool delay_play){if(IsPlayer()) ((Client*)this)->Send_Animate(from_cr,anim1,anim2,item,clear_sequence,delay_play);}
void Critter::Send_CombatResult(DWORD* combat_res, DWORD len){if(IsPlayer()) ((Client*)this)->Send_CombatResult(combat_res,len);}
void Critter::Send_Quest(DWORD num){if(IsPlayer()) ((Client*)this)->Send_Quest(num);}
void Critter::Send_Quests(DwordVec& nums){if(IsPlayer()) ((Client*)this)->Send_Quests(nums);}
void Critter::Send_HoloInfo(BYTE clear, WORD offset, WORD count){if(IsPlayer()) ((Client*)this)->Send_HoloInfo(clear,offset,count);}
void Critter::Send_Follow(DWORD rule, BYTE follow_type, WORD map_pid, DWORD follow_wait){if(IsPlayer()) ((Client*)this)->Send_Follow(rule,follow_type,map_pid,follow_wait);}
void Critter::Send_Effect(WORD eff_pid, WORD hx, WORD hy, WORD radius){if(IsPlayer()) ((Client*)this)->Send_Effect(eff_pid,hx,hy,radius);}
void Critter::Send_FlyEffect(WORD eff_pid, DWORD from_crid, DWORD to_crid, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy){if(IsPlayer()) ((Client*)this)->Send_FlyEffect(eff_pid,from_crid,to_crid,from_hx,from_hy,to_hx,to_hy);}
void Critter::Send_PlaySound(DWORD crid_synchronize, const char* sound_name){if(IsPlayer()) ((Client*)this)->Send_PlaySound(crid_synchronize,sound_name);}
void Critter::Send_PlaySoundType(DWORD crid_synchronize, BYTE sound_type, BYTE sound_type_ext, BYTE sound_id, BYTE sound_id_ext){if(IsPlayer()) ((Client*)this)->Send_PlaySoundType(crid_synchronize,sound_type,sound_type_ext,sound_id,sound_id_ext);}
void Critter::Send_CritterLexems(Critter* cr){if(IsPlayer()) ((Client*)this)->Send_CritterLexems(cr);}

void Critter::SendA_Move(WORD move_params)
{
	//Data.Dir=FLAG(move_params,BIN8(00000111));
//	if(FLAG(move_params,0x38)==0x38) //проверка по окончании пути
//		Send_XY();

	if(VisCr.empty()) return;
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer()) cr->Send_Move(this,move_params);
	}
}

void Critter::SendA_XY()
{
	if(VisCr.empty()) return;
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer()) cr->Send_XY(this);
	}
}

void Critter::SendA_Action(int action, int action_ext, Item* item)
{
	if(VisCr.empty()) return;
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer()) cr->Send_Action(this,action,action_ext,item);
	}
}

void Critter::SendAA_Action(int action, int action_ext, Item* item)
{
	if(IsPlayer()) Send_Action(this,action,action_ext,item);

	if(VisCr.empty()) return;
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer()) cr->Send_Action(this,action,action_ext,item);
	}
}

void Critter::SendA_Knockout(bool face_up, WORD knock_hx, WORD knock_hy)
{
	if(VisCr.empty()) return;
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer()) cr->Send_Knockout(this,face_up,knock_hx,knock_hy);
	}
}

void Critter::SendAA_MoveItem(Item* item, BYTE action, BYTE prev_slot)
{
	if(IsPlayer()) Send_MoveItem(this,item,action,prev_slot);

	if(VisCr.empty()) return;
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer()) cr->Send_MoveItem(this,item,action,prev_slot);
	}
}

void Critter::SendAA_ItemData(Item* item)
{
	if(IsPlayer()) Send_AddItem(item);

	BYTE slot=item->ACC_CRITTER.Slot;
	if(!VisCr.empty() && slot!=SLOT_INV && SlotEnabled[slot])
	{
		if(SlotDataSendEnabled[slot])
		{
			int condition_index=SlotDataSendConditionIndex[slot];
			if(condition_index==-1)
			{
				// Send all data
				for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
				{
					Critter* cr=*it;
					if(cr->IsPlayer()) cr->Send_ItemData(this,slot,item,true);
				}
			}
			else
			{
				// Send all data if condition passably, else send half
				for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
				{
					Critter* cr=*it;
					if(cr->IsPlayer())
					{
						if((cr->Data.Params[condition_index]&SlotDataSendConditionMask[slot])!=0)
							cr->Send_ItemData(this,slot,item,true);
						else
							cr->Send_ItemData(this,slot,item,false);
					}
				}
			}
		}
		else
		{
			// Send half data
			for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
			{
				Critter* cr=*it;
				if(cr->IsPlayer()) cr->Send_ItemData(this,slot,item,false);
			}
		}
	}
}

void Critter::SendAA_Animate(DWORD anim1, DWORD anim2, Item* item, bool clear_sequence, bool delay_play)
{
	if(IsPlayer()) Send_Animate(this,anim1,anim2,item,clear_sequence,delay_play);

	if(VisCr.empty()) return;
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer()) cr->Send_Animate(this,anim1,anim2,item,clear_sequence,delay_play);
	}
}

void Critter::SendA_GlobalInfo(GlobalMapGroup* group, BYTE info_flags)
{
	if(!group) return;
	for(CrVecIt it=group->CritMove.begin(),end=group->CritMove.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer()) cr->Send_GlobalInfo(info_flags);
	}
}

void Critter::SendAA_Text(CrVec& to_cr, const char* str, BYTE how_say, bool unsafe_text)
{
	if(!str || !str[0]) return;

	WORD str_len=strlen(str);
	DWORD from_id=GetId();
	WORD intellect=(how_say>=SAY_NORM && how_say<=SAY_RADIO?GetSayIntellect():0);

	if(IsPlayer()) Send_TextEx(from_id,str,str_len,how_say,intellect,unsafe_text);

	if(to_cr.empty()) return;
	for(CrVecIt it=to_cr.begin(),end=to_cr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr==this || !cr->IsPlayer()) continue;

		if(how_say!=SAY_WHISP && how_say!=SAY_WHISP_ON_HEAD)
			cr->Send_TextEx(from_id,str,str_len,how_say,intellect,unsafe_text);
		else if(CheckDist(Data.HexX,Data.HexY,cr->Data.HexX,cr->Data.HexY,GameOpt.WhisperDist))
			cr->Send_TextEx(from_id,str,str_len,how_say,intellect,unsafe_text);
	}
}

void Critter::SendAA_Msg(CrVec& to_cr, DWORD num_str, BYTE how_say, WORD num_msg)
{
	if(IsPlayer()) Send_TextMsg(this,num_str,how_say,num_msg);

	if(to_cr.empty()) return;
	for(CrVecIt it=to_cr.begin(),end=to_cr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr==this) continue;
		if(!cr->IsPlayer()) continue;

		if(how_say!=SAY_WHISP && how_say!=SAY_WHISP_ON_HEAD)
			cr->Send_TextMsg(this,num_str,how_say,num_msg);
		else if(CheckDist(Data.HexX,Data.HexY,cr->Data.HexX,cr->Data.HexY,GameOpt.WhisperDist))
			cr->Send_TextMsg(this,num_str,how_say,num_msg);
	}
}

void Critter::SendAA_MsgLex(CrVec& to_cr, DWORD num_str, BYTE how_say, WORD num_msg, const char* lexems)
{
	if(IsPlayer()) Send_TextMsgLex(this,num_str,how_say,num_msg,lexems);

	if(to_cr.empty()) return;
	for(CrVecIt it=to_cr.begin(),end=to_cr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr==this) continue;
		if(!cr->IsPlayer()) continue;

		if(how_say!=SAY_WHISP && how_say!=SAY_WHISP_ON_HEAD)
			cr->Send_TextMsgLex(this,num_str,how_say,num_msg,lexems);
		else if(CheckDist(Data.HexX,Data.HexY,cr->Data.HexX,cr->Data.HexY,GameOpt.WhisperDist))
			cr->Send_TextMsgLex(this,num_str,how_say,num_msg,lexems);
	}
}

void Critter::SendA_Dir()
{
	if(VisCr.empty()) return;
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer()) cr->Send_Dir(this);
	}
}

void Critter::SendA_Follow(BYTE follow_type, WORD map_pid, DWORD follow_wait)
{
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(!cr->IsPlayer()) continue;
		if(cr->GetFollowCrId()!=GetId()) continue;
		if(cr->IsDead() || cr->IsKnockout()) continue;
		if(!CheckDist(Data.LastHexX,Data.LastHexY,cr->GetHexX(),cr->GetHexY(),FOLLOW_DIST)) continue;
		cr->Send_Follow(GetId(),follow_type,map_pid,follow_wait);
	}
}

void Critter::SendA_Effect(CrVec& crits, WORD eff_pid, WORD hx, WORD hy, WORD radius)
{
	if(crits.empty()) return;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer()) cr->Send_Effect(eff_pid,hx,hy,radius);
	}
}

void Critter::SendA_FlyEffect(CrVec& crits, WORD eff_pid, Critter* cr1, Critter* cr2)
{
	if(crits.empty()) return;
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer()) cr->Send_FlyEffect(eff_pid,cr1->GetId(),cr2->GetId(),cr1->GetHexX(),cr1->GetHexY(),cr2->GetHexX(),cr2->GetHexY());
	}
}

void Critter::SendA_ParamOther(WORD num_param, int val)
{
	if(VisCr.empty()) return;
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(cr->IsPlayer())
			cr->Send_CritterParam(this,num_param,val);
	}
}

void Critter::SendA_ParamCheck(WORD num_param)
{
	if(VisCr.empty()) return;

	int condition_index=ParamsSendConditionIndex[num_param];
	if(condition_index==-1)
	{
		for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
		{
			Critter* cr=*it;
			if(cr->IsPlayer()) cr->Send_CritterParam(this,num_param,0);
		}
	}
	else
	{
		for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
		{
			Critter* cr=*it;
			if(cr->IsPlayer() && (cr->Data.Params[condition_index]&ParamsSendConditionMask[num_param])!=0)
				cr->Send_CritterParam(this,num_param,0);
		}
	}
}

void Critter::Send_AddAllItems()
{
	if(IsPlayer())
	{
		Client* cl=(Client*)this;
		BOUT_BEGIN(cl);
		cl->Bout << NETMSG_CLEAR_ITEMS;
		BOUT_END(cl);
	}

	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it) Send_AddItem(*it);
}

void Critter::Send_AllQuests()
{
	VarsVec& vars=VarMngr.GetQuestVars();
	DwordVec quests;
	for(VarsVecIt it=vars.begin(),end=vars.end();it!=end;++it)
	{
		GameVar* var=*it;
		if(var && var->GetMasterId()==GetId()) quests.push_back(VAR_CALC_QUEST(var->VarTemplate->TempId,var->VarValue));
	}
	Send_Quests(quests);
}

void Critter::SendMessage(int num, int val, int to)
{
	switch(to)
	{
	case MESSAGE_TO_VISIBLE_ME:
		{
			for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
			{
				(*it)->EventMessage(this,num,val);
			}
		}
		break;
	case MESSAGE_TO_IAM_VISIBLE:
		{
			for(CrVecIt it=VisCrSelf.begin(),end=VisCrSelf.end();it!=end;++it)
			{
				(*it)->EventMessage(this,num,val);
			}
		}
		break;
	case MESSAGE_TO_ALL_ON_MAP:
		{
			Map* m=MapMngr.GetMap(GetMap());
			if(!m) return;

			CrVec crits=m->GetCritters();
			for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
			{
				Critter* cr=*it;
				cr->EventMessage(this,num,val);
			}
		}
		break;
	default:
		break;
	}
}

const char* Critter::GetInfo()
{
//	static char buf[1024];
//	sprintf(buf,"Name<%s>, Id<%u>, MapPid<%u>, HexX<%u>, HexY<%u>",GetName(),GetId(),GetProtoMap(),GetHexX(),GetHexY());
//	return buf;
	return GetName();
}

int Critter::GetParam(DWORD index)
{
	if(ParamsGetScript[index] && Script::PrepareContext(ParamsGetScript[index],CALL_FUNC_STR,""))
	{
		Script::SetArgObject(this);
		Script::SetArgDword(index-(ParametersOffset[index]?ParametersMin[index]:0));
		if(Script::RunPrepared()) return Script::GetReturnedDword();
	}

	switch(index)
	{
	case ST_STRENGTH: return GetStrength();
	case ST_PERCEPTION: return GetPerception();
	case ST_ENDURANCE: return GetEndurance();
	case ST_CHARISMA: return GetCharisma();
	case ST_INTELLECT: return GetIntellegence();
	case ST_AGILITY: return GetAgility();
	case ST_LUCK: return GetLuck();
	case ST_MAX_LIFE: return GetMaxLife();
	case ST_SEQUENCE: return GetSequence();
	case ST_ACTION_POINTS: return GetMaxAp();
	case ST_ARMOR_CLASS: return GetAc();
	case ST_MELEE_DAMAGE: return GetMeleeDmg();
	case ST_CARRY_WEIGHT: return GetMaxWeight();
	case ST_HEALING_RATE: return GetHealingRate();
	case ST_CRITICAL_CHANCE: return GetCriticalChance();
	case ST_MAX_CRITICAL: return GetMaxCritical();
	case ST_RADIATION_RESISTANCE: return GetRadiationResist();
	case ST_POISON_RESISTANCE: return GetPoisonResist();
	case ST_CURRENT_AP: return GetAp();
	default: break;
	}

	if(index>=ST_NORMAL_RESIST && index<=ST_EXPLODE_RESIST) return GetDamageResistance(index-ST_NORMAL_RESIST+1);
	if(index>=ST_NORMAL_ABSORB && index<=ST_EXPLODE_ABSORB) return GetDamageThreshold(index-ST_NORMAL_ABSORB+1);
	if(index>=TIMEOUT_BEGIN && index<=TIMEOUT_END) return GetTimeout(index);
	if(index>=REPUTATION_BEGIN && index<=REPUTATION_END) return GetReputation(index);
	return Data.Params[index];
}

void Critter::ChangeParam(DWORD index)
{
	if(!ParamsIsChanged[index] && ParamLocked!=index)
	{
		ParamsChanged.push_back(index);
		ParamsChanged.push_back(Data.Params[index]);
		ParamsIsChanged[index]=true;
	}
}

IntVec CallChange;
void Critter::ProcessChangedParams()
{
	if(ParamsChanged.size())
	{
		CallChange.clear();
		for(size_t i=0,j=ParamsChanged.size();i<j;i+=2)
		{
			int index=ParamsChanged[i];
			int old_val=ParamsChanged[i+1];
			if(Data.Params[index]!=old_val)
			{
				if(ParamsChangeScript[index])
				{
					CallChange.push_back(ParamsChangeScript[index]);
					CallChange.push_back(index);
					CallChange.push_back(old_val);
				}
				else
				{
					Send_Param(index);
					if(ParamsSendEnabled[index]) SendA_ParamCheck(index);
				}
			}
			ParamsIsChanged[index]=false;
		}
		ParamsChanged.clear();

		if(CallChange.size())
		{
			for(size_t i=0,j=CallChange.size();i<j;i+=3)
			{
				DWORD index=CallChange[i+1];
				ParamLocked=index;
				if(Script::PrepareContext(CallChange[i],CALL_FUNC_STR,GetInfo()))
				{
					Script::SetArgObject(this);
					Script::SetArgDword(index-(ParametersOffset[index]?ParametersMin[index]:0));
					Script::SetArgDword(CallChange[i+2]);
					Script::RunPrepared();
				}
				ParamLocked=-1;
				Send_Param(index);
				if(ParamsSendEnabled[index]) SendA_ParamCheck(index);
			}
		}
	}
}

int Critter::GetAc()
{
	int val=Data.Params[ST_ARMOR_CLASS]+Data.Params[ST_ARMOR_CLASS_EXT]+GetAgility()+Data.Params[ST_TURN_BASED_AC];
	Item* armor=ItemSlotArmor;
	if(!armor->GetId() || !armor->IsArmor()) armor=NULL;
	int wearProc=(armor?100-armor->GetWearProc():0);
	if(armor) val+=armor->Proto->Armor.AC*wearProc/100;
	return CLAMP(val,0,90);
}

int Critter::GetDamageResistance(int dmg_type)
{
	Item* armor=ItemSlotArmor;
	if(!armor->GetId() || !armor->IsArmor()) armor=NULL;
	int wearProc=(armor?100-armor->GetWearProc():0);

	int val=0;
	switch(dmg_type)
	{
	case DAMAGE_TYPE_UNCALLED: break;
	case DAMAGE_TYPE_NORMAL:   val=Data.Params[ST_NORMAL_RESIST] +Data.Params[ST_NORMAL_RESIST_EXT] +(armor?armor->Proto->Armor.DRNormal*wearProc/100:0); break;
	case DAMAGE_TYPE_LASER:    val=Data.Params[ST_LASER_RESIST]  +Data.Params[ST_LASER_RESIST_EXT]  +(armor?armor->Proto->Armor.DRLaser*wearProc/100:0); break;
	case DAMAGE_TYPE_FIRE:     val=Data.Params[ST_FIRE_RESIST]   +Data.Params[ST_FIRE_RESIST_EXT]   +(armor?armor->Proto->Armor.DRFire*wearProc/100:0); break;
	case DAMAGE_TYPE_PLASMA:   val=Data.Params[ST_PLASMA_RESIST] +Data.Params[ST_PLASMA_RESIST_EXT] +(armor?armor->Proto->Armor.DRPlasma*wearProc/100:0); break;
	case DAMAGE_TYPE_ELECTR:   val=Data.Params[ST_ELECTRO_RESIST]+Data.Params[ST_ELECTRO_RESIST_EXT]+(armor?armor->Proto->Armor.DRElectr*wearProc/100:0); break;
	case DAMAGE_TYPE_EMP:      val=Data.Params[ST_EMP_RESIST]    +Data.Params[ST_EMP_RESIST_EXT]    +(armor?armor->Proto->Armor.DREmp:0); break;
	case DAMAGE_TYPE_EXPLODE:  val=Data.Params[ST_EXPLODE_RESIST]+Data.Params[ST_EXPLODE_RESIST_EXT]+(armor?armor->Proto->Armor.DRExplode*wearProc/100:0); break;
	default: break;
	}

	if(dmg_type==DAMAGE_TYPE_EMP) return CLAMP(val,0,999);
	return CLAMP(val,0,90);
}

int Critter::GetDamageThreshold(int dmg_type)
{
	Item* armor=ItemSlotArmor;
	if(!armor->GetId() || !armor->IsArmor()) armor=NULL;
	int wearProc=(armor?100-armor->GetWearProc():0);

	int val=0;
	switch(dmg_type)
	{
	case DAMAGE_TYPE_UNCALLED: break;
	case DAMAGE_TYPE_NORMAL:   val=Data.Params[ST_NORMAL_ABSORB] +Data.Params[ST_NORMAL_ABSORB_EXT] +(armor?armor->Proto->Armor.DTNormal*wearProc/100:0); break;
	case DAMAGE_TYPE_LASER:    val=Data.Params[ST_LASER_ABSORB]  +Data.Params[ST_LASER_ABSORB_EXT]  +(armor?armor->Proto->Armor.DTLaser*wearProc/100:0); break;
	case DAMAGE_TYPE_FIRE:     val=Data.Params[ST_FIRE_ABSORB]   +Data.Params[ST_FIRE_ABSORB_EXT]   +(armor?armor->Proto->Armor.DTFire*wearProc/100:0); break;
	case DAMAGE_TYPE_PLASMA:   val=Data.Params[ST_PLASMA_ABSORB] +Data.Params[ST_PLASMA_ABSORB_EXT] +(armor?armor->Proto->Armor.DTPlasma*wearProc/100:0); break;
	case DAMAGE_TYPE_ELECTR:   val=Data.Params[ST_ELECTRO_ABSORB]+Data.Params[ST_ELECTRO_ABSORB_EXT]+(armor?armor->Proto->Armor.DTElectr*wearProc/100:0); break;
	case DAMAGE_TYPE_EMP:      val=Data.Params[ST_EMP_ABSORB]    +Data.Params[ST_EMP_ABSORB_EXT]    +(armor?armor->Proto->Armor.DTEmp:0); break;
	case DAMAGE_TYPE_EXPLODE:  val=Data.Params[ST_EXPLODE_ABSORB]+Data.Params[ST_EXPLODE_ABSORB_EXT]+(armor?armor->Proto->Armor.DTExplode*wearProc/100:0); break;
	default: break;
	}

	return CLAMP(val,0,999);
}

int Critter::GetNightPersonBonus()
{
	if(GameOpt.Hour<6 || GameOpt.Hour>18) return 1;
	if(GameOpt.Hour==6 && GameOpt.Minute==0) return 1;
	if(GameOpt.Hour==18 && GameOpt.Minute>0) return 1;
	return -1;
}

int Critter::GetReputation(DWORD index)
{
	if(Data.Params[index]==0x80000000) Data.Params[index]=0;
	return Data.Params[index];
}

DWORD Critter::GetTimeWalk()
{
	if(IsDmgTwoLeg()) return GameOpt.Breaktime;
	return CritType::GetTimeWalk(GetCrType());
}

DWORD Critter::GetTimeRun()
{
	if(IsDmgTwoLeg()) return GameOpt.Breaktime;
	return CritType::GetTimeRun(GetCrType());
}

DWORD Critter::GetItemsWeight()
{
	DWORD res=0;
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		if(!item->IsHidden()) res+=item->GetWeight();
	}
	return res;
}

DWORD Critter::GetItemsVolume()
{
	DWORD res=0;
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		if(!item->IsHidden()) res+=item->GetVolume();
	}
	return res;
}

int Critter::GetFreeWeight()
{
	int cur_weight=GetItemsWeight();
	int max_weight=GetParam(ST_CARRY_WEIGHT);
	return max_weight-cur_weight;
}

int Critter::GetFreeVolume()
{
	int cur_volume=GetItemsVolume();
	int max_volume=CRITTER_INV_VOLUME;
	return max_volume-cur_volume;
}

/*
void Critter::SetPerkSafe(DWORD index, int val)
{
	if(Data.Pe[num]==val) return;

	if(num>=0 && num<PERK_COUNT)
	{
		// Up
		if(Data.Pe[num]<val)
		{
			val=val-Data.Pe[num];
			while(val)
			{
				Perks::Up(this,num,PERK_ALL,true);
				val--;
			}
		}
		// Down
		else
		{
			val=Data.Pe[num]-val;
			while(val)
			{
				Perks::Down(this,num);
				val--;
			}
		}
	}
	else if(num>=DAMAGE_BEGIN && num<=DAMAGE_END)
	{
		// Injure, uninjure
		Data.Pe[num]=val;
		SendPerk(num);

		if(num==PE_DAMAGE_EYE) ProcessVisibleCritters();
	}
	else if(num>=MODE_BEGIN && num<=MODE_END)
	{
		// Modes
		Data.Pe[num]=val;
		SendPerk(num);

		if(num==MODE_HIDE && GetMap()) ProcessVisibleCritters();
	}
	else if(num>=TRAIT_BEGIN && num<=TRAIT_END)
	{
		// Trait
		Data.Pe[num]=val;
		SendPerk(num);
	}
}*/


bool Critter::CheckMyTurn(Map* map)
{
	if(!IsTurnBased()) return true;
	if(!map) map=MapMngr.GetMap(GetMap());
	if(!map || !map->IsTurnBasedOn || map->IsCritterTurn(this)) return true;
	return false;
}

/************************************************************************/
/* Timeouts                                                             */
/************************************************************************/

void Critter::SetTimeout(int timeout, DWORD game_minutes)
{
	ChangeParam(timeout);
	if(game_minutes) Data.Params[timeout]=GameOpt.FullMinute+game_minutes;
	else Data.Params[timeout]=0;
}

bool Critter::IsTransferTimeouts(bool send)
{
	if(GetTimeout(TO_TRANSFER))
	{
		if(send) Send_TextMsg(this,STR_TIMEOUT_TRANSFER_WAIT,SAY_NETMSG,TEXTMSG_GAME);
		return true;
	}
	if(GetTimeout(TO_BATTLE))
	{
		if(send) Send_TextMsg(this,STR_TIMEOUT_BATTLE_WAIT,SAY_NETMSG,TEXTMSG_GAME);
		return true;
	}
	return false;
}

DWORD Critter::GetTimeout(int timeout)
{
	return Data.Params[timeout]>GameOpt.FullMinute?Data.Params[timeout]-GameOpt.FullMinute:0;
}

/************************************************************************/
/* Enemy stack                                                          */
/************************************************************************/

void Critter::AddEnemyInStack(DWORD crid)
{
	// Find already
	int stack_count=min(Data.EnemyStackCount,MAX_ENEMY_STACK);
	for(int i=0;i<stack_count;i++)
	{
		if(Data.EnemyStack[i]==crid)
		{
			for(int j=i;j<stack_count-1;j++)
			{
				Data.EnemyStack[j]=Data.EnemyStack[j+1];
			}
			Data.EnemyStack[stack_count-1]=crid;
			return;
		}
	}
	// Add
	for(int i=0;i<stack_count-1;i++)
	{
		Data.EnemyStack[i]=Data.EnemyStack[i+1];
	}
	Data.EnemyStack[stack_count-1]=crid;
}

bool Critter::CheckEnemyInStack(DWORD crid)
{
	if(IsPerk(MODE_NO_ENEMY_STACK)) return false;

	int stack_count=min(Data.EnemyStackCount,MAX_ENEMY_STACK);
	for(int i=0;i<stack_count;i++)
	{
		if(Data.EnemyStack[i]==crid) return true;
	}
	return false;
}

void Critter::EraseEnemyInStack(DWORD crid)
{
	int stack_count=min(Data.EnemyStackCount,MAX_ENEMY_STACK);
	for(int i=0;i<stack_count;i++)
	{
		if(Data.EnemyStack[i]==crid)
		{
			for(int j=i;j>0;j--)
			{
				Data.EnemyStack[j]=Data.EnemyStack[j-1];
			}
			Data.EnemyStack[0]=0;
			break;
		}
	}
}

Critter* Critter::ScanEnemyStack()
{
	if(IsPerk(MODE_NO_ENEMY_STACK)) return NULL;

	int stack_count=min(Data.EnemyStackCount,MAX_ENEMY_STACK);
	for(int i=0;i<stack_count;i++)
	{
		DWORD crid=Data.EnemyStack[i];
		if(crid)
		{
			Critter* enemy=GetCritSelf(crid);
			if(enemy && !enemy->IsDead()) return enemy;
		}
	}
	return NULL;
}

/************************************************************************/
/* Misc events                                                          */
/************************************************************************/

void Critter::AddCrTimeEvent(DWORD func_num, DWORD rate, DWORD duration, int identifier)
{
	if(duration) duration+=GameOpt.FullMinute;
	CrTimeEventVecIt it=CrTimeEvents.begin(),end=CrTimeEvents.end();
	for(;it!=end;++it)
	{
		CrTimeEvent& cte=*it;
		if(duration<cte.NextTime) break;
	}

	CrTimeEvent cte;
	cte.FuncNum=func_num;
	cte.Rate=rate;
	cte.NextTime=duration;
	cte.Identifier=identifier;
	CrTimeEvents.insert(it,cte);
	Data.CrTimeEventFullMinute=GameOpt.FullMinute;
}

void Critter::EraseCrTimeEvent(int index)
{
	if(index>=CrTimeEvents.size()) return;
	CrTimeEvents.erase(CrTimeEvents.begin()+index);
}

void Critter::ContinueTimeEvents(int offs_time)
{
	for(CrTimeEventVecIt it=CrTimeEvents.begin(),end=CrTimeEvents.end();it!=end;++it)
	{
		CrTimeEvent& cte=*it;
		if(cte.NextTime) cte.NextTime+=offs_time;
	}
}

void Critter::Delete()
{
	if(IsPlayer())
		delete (Client*)this;
	else
		delete (Npc*)this;
}

/************************************************************************/
/* Client                                                               */
/************************************************************************/

Client::Client():
ZstrmInit(false),Access(ACCESS_DEFAULT),pingOk(true),LanguageMsg(0),
NetState(STATE_DISCONNECT),IsDisconnected(false),DisableZlib(false),
LastSendScoresTick(0),LastSendCraftTick(0),LastSendEntrancesTick(0),LastSendEntrancesLocId(0),
ScreenCallbackBindId(0),ConnectTime(0),LastSendedMapTick(0)
{
	CritterIsNpc=false;
	MEMORY_PROCESS(MEMORY_CLIENT,sizeof(Client)+sizeof(GlobalMapGroup)+40+WSA_BUF_SIZE*2);

	SETFLAG(Flags,FCRIT_PLAYER);
	Sock=INVALID_SOCKET;
	ZeroMemory(Name,sizeof(Name));
	ZeroMemory(Pass,sizeof(Pass));
	StringCopy(Name,"err");
	pingNextTick=Timer::FastTick()+PING_CLIENT_LIFE_TIME;
	Talk.Clear();
	talkNextTick=Timer::FastTick()+PROCESS_TALK_TICK;
	ConnectTime=Timer::FastTick();
	LastSay[0]=0;
	LastSayEqualCount=0;
	ZeroMemory(UID,sizeof(UID));

	InitializeCriticalSection(&ShutdownCS);
	WSAIn=new WSAOVERLAPPED_EX;
	ZeroMemory(WSAIn,sizeof(WSAOVERLAPPED_EX));
	WSAIn->Client=this;
	WSAIn->Buffer.buf=new char[WSA_BUF_SIZE];
	WSAIn->Buffer.len=WSA_BUF_SIZE;
	WSAIn->Operation=WSAOP_RECV;
	InitializeCriticalSection(&WSAIn->CS);
	WSAOut=new WSAOVERLAPPED_EX;
	ZeroMemory(WSAOut,sizeof(WSAOVERLAPPED_EX));
	WSAOut->Client=this;
	WSAOut->Buffer.buf=new char[WSA_BUF_SIZE];
	WSAOut->Buffer.len=0;
	WSAOut->Operation=WSAOP_FREE;
	InitializeCriticalSection(&WSAOut->CS);
}

Client::~Client()
{
	MEMORY_PROCESS(MEMORY_CLIENT,-(int)(sizeof(Client)+sizeof(GlobalMapGroup)+40+WSA_BUF_SIZE*2));
	if(DataExt)
	{
		MEMORY_PROCESS(MEMORY_CLIENT,-(int)sizeof(CritDataExt));
		SAFEDEL(DataExt);
	}
	DeleteCriticalSection(&ShutdownCS);
	if(WSAIn)
	{
		DeleteCriticalSection(&WSAIn->CS);
		delete[] WSAIn->Buffer.buf;
		delete WSAIn;
		WSAIn=NULL;
	}
	if(WSAOut)
	{
		DeleteCriticalSection(&WSAOut->CS);
		delete[] WSAOut->Buffer.buf;
		delete WSAOut;
		WSAOut=NULL;
	}
	if(ZstrmInit) deflateEnd(&Zstrm);
}

void Client::Shutdown()
{
	EnterCriticalSection(&ShutdownCS);
	if(Sock!=INVALID_SOCKET)
	{
		shutdown(Sock,SD_BOTH);
		closesocket(Sock);
		Sock=INVALID_SOCKET;
	}
	Bin.LockReset();
	Bout.LockReset();
	if(!IsOffline()) Disconnect();
	LeaveCriticalSection(&ShutdownCS);
}

void Client::PingClient()
{
	if(!pingOk)
	{
		// WriteLog(__FUNCTION__" - Client is drop, disconnect, info<%s>.\n",GetInfo());
		Disconnect();
		Bout.LockReset();
		return;
	}

	BOUT_BEGIN(this);
	Bout << NETMSG_PING;
	Bout << (BYTE)PING_CLIENT;
	BOUT_END(this);

	pingNextTick=Timer::FastTick()+PING_CLIENT_LIFE_TIME;
	pingOk=false;
}

void Client::Send_AddCritter(Critter* cr)
{
	if(IsSendDisabled() || IsOffline()) return;

	bool is_npc=cr->IsNpc();
	MSGTYPE msg=(is_npc?NETMSG_ADD_NPC:NETMSG_ADD_PLAYER);
	DWORD msg_len=sizeof(msg)+sizeof(msg_len)+sizeof(DWORD)+sizeof(DWORD)+sizeof(WORD)*2+
		sizeof(BYTE)+sizeof(BYTE)*2+sizeof(DWORD)+(is_npc?sizeof(WORD)+sizeof(DWORD):MAX_NAME)+ParamsSendMsgLen;
	int dialog_id=(is_npc?cr->GetParam(ST_DIALOG_ID):0);

	BOUT_BEGIN(this);
	Bout << msg;
	Bout << msg_len;
	Bout << cr->GetId();
	Bout << cr->Data.BaseType;
	Bout << cr->GetHexX();
	Bout << cr->GetHexY();
	Bout << cr->GetDir();
	Bout << cr->Data.Cond;
	Bout << cr->Data.CondExt;
	Bout << cr->Flags;

	if(is_npc)
	{
		Npc* npc=(Npc*)cr;
		Bout << npc->GetProtoId();
		Bout << dialog_id;
	}
	else
	{
		Client* cl=(Client*)cr;
		Bout.Push(cl->Name,MAX_NAME);
	}

	Bout << ParamsSendCount;
	for(WordVecIt it=ParamsSend.begin(),end=ParamsSend.end();it!=end;++it)
	{
		WORD index=*it;
		Bout << index;

		int condition_index=ParamsSendConditionIndex[index];
		if(condition_index==-1 || (Data.Params[condition_index]&ParamsSendConditionMask[index])!=0)
			Bout << cr->Data.Params[index];
		else
			Bout << (int)0;
	}
	BOUT_END(this);

	if(cr!=this) Send_MoveItem(cr,NULL,ACTION_REFRESH,0);
	if(cr->IsLexems()) Send_CritterLexems(cr);
}

void Client::Send_RemoveCritter(Critter* cr) //Oleg
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_REMOVE_CRITTER;
	Bout << cr->GetId();
	BOUT_END(this);
}

void Client::Send_LoadMap(Map* map)
{
	if(IsSendDisabled() || IsOffline()) return;

	WORD pid_map=0;
	int map_time=-1;
	BYTE map_rain=0;
	DWORD hash_tiles=0;
	DWORD hash_walls=0;
	DWORD hash_scen=0;

	if(!map) map=MapMngr.GetMap(GetMap());
	if(map)
	{
		pid_map=map->GetPid();
		map_time=map->GetTime();
		map_rain=map->GetRain();
		hash_tiles=map->Proto->HashTiles;
		hash_walls=map->Proto->HashWalls;
		hash_scen=map->Proto->HashScen;
	}

	BOUT_BEGIN(this);
	Bout << NETMSG_LOADMAP;
	Bout << pid_map;
	Bout << map_time;
	Bout << map_rain;
	Bout << hash_tiles;
	Bout << hash_walls;
	Bout << hash_scen;
	BOUT_END(this);

	InterlockedExchange(&NetState,STATE_LOGINOK); // TODO:
}

void Client::Send_Move(Critter* from_cr, WORD move_params)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_CRITTER_MOVE; 
	Bout << from_cr->GetId();
	Bout << move_params;
	Bout << from_cr->GetHexX();
	Bout << from_cr->GetHexY();
	BOUT_END(this);
}

void Client::Send_Dir(Critter* from_cr)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_CRITTER_DIR;
	Bout << from_cr->GetId();
	Bout << from_cr->GetDir();
	BOUT_END(this);
}

void Client::Send_Action(Critter* from_cr, int action, int action_ext, Item* item)
{
	if(IsSendDisabled() || IsOffline()) return;
	Send_XY(from_cr);
	if(item) Send_SomeItem(item);

	BOUT_BEGIN(this);
	Bout << NETMSG_CRITTER_ACTION;
	Bout << from_cr->GetId();
	Bout << action;
	Bout << action_ext;
	Bout << (bool)(item?true:false);
	BOUT_END(this);
}

void Client::Send_Knockout(Critter* from_cr, bool face_up, WORD knock_hx, WORD knock_hy)
{
	if(IsSendDisabled() || IsOffline()) return;
	Send_XY(from_cr);

	BOUT_BEGIN(this);
	Bout << NETMSG_CRITTER_KNOCKOUT;
	Bout << from_cr->GetId();
	Bout << face_up;
	Bout << knock_hx;
	Bout << knock_hy;
	BOUT_END(this);
}

void Client::Send_MoveItem(Critter* from_cr, Item* item, BYTE action, BYTE prev_slot)
{
	if(IsSendDisabled() || IsOffline()) return;
	Send_XY(from_cr);
	if(item) Send_SomeItem(item);

	MSGTYPE msg=NETMSG_CRITTER_MOVE_ITEM;
	DWORD msg_len=sizeof(msg)+sizeof(msg_len)+sizeof(DWORD)+sizeof(action)+sizeof(prev_slot)+sizeof(bool);

	BYTE slots_pid_count=0;
	BYTE slots_data_count=0;
	ItemPtrVec& inv=from_cr->GetInventory();
	for(ItemPtrVecIt it=inv.begin(),end=inv.end();it!=end;++it)
	{
		Item* item_=*it;
		if(item_->ACC_CRITTER.Slot!=SLOT_INV && SlotEnabled[item_->ACC_CRITTER.Slot])
		{
			int condition_index=SlotDataSendConditionIndex[item_->ACC_CRITTER.Slot];
			if(SlotDataSendEnabled[item_->ACC_CRITTER.Slot] &&
				(condition_index==-1 || (Data.Params[condition_index]&SlotDataSendConditionMask[item_->ACC_CRITTER.Slot])!=0))
			{
				SlotEnabledCacheData[slots_data_count]=item_;
				slots_data_count++;
			}
			else
			{
				SlotEnabledCachePid[slots_pid_count]=item_;
				slots_pid_count++;
			}
		}
	}
	msg_len+=sizeof(slots_pid_count)+(sizeof(BYTE)+sizeof(WORD)+7)*slots_pid_count;
	msg_len+=sizeof(slots_data_count)+(sizeof(BYTE)+sizeof(DWORD)+sizeof(WORD)+sizeof(Item::ItemData))*slots_data_count;

	BOUT_BEGIN(this);
	Bout << NETMSG_CRITTER_MOVE_ITEM;
	Bout << msg_len;
	Bout << from_cr->GetId();
	Bout << action;
	Bout << prev_slot;
	Bout << (bool)(item?true:false);

	// Slots
	Bout << slots_pid_count;
	for(BYTE i=0;i<slots_pid_count;i++)
	{
		Item* item_=SlotEnabledCachePid[i];
		Bout << item_->ACC_CRITTER.Slot;
		Bout << item_->GetProtoId();
		Bout.Push((char*)&item_->Data.LightIntensity,7); // Only light
	}
	Bout << slots_data_count;
	for(BYTE i=0;i<slots_data_count;i++)
	{
		Item* item_=SlotEnabledCacheData[i];
		Bout << item_->ACC_CRITTER.Slot;
		Bout << item_->GetId();
		Bout << item_->GetProtoId();
		Bout.Push((char*)&item_->Data,sizeof(item_->Data));
	}
	BOUT_END(this);

	// Lexems
	for(BYTE i=0;i<slots_data_count;i++) if(SlotEnabledCacheData[i]->PLexems) Send_ItemLexems(SlotEnabledCacheData[i]);
}

void Client::Send_ItemData(Critter* from_cr, BYTE slot, Item* item, bool full)
{
	if(IsSendDisabled() || IsOffline()) return;

	if(full)
	{
		// Send full item data
		BOUT_BEGIN(this);
		Bout << NETMSG_CRITTER_ITEM_DATA;
		Bout << from_cr->GetId();
		Bout << slot;
		Bout.Push((char*)&item->Data,sizeof(item->Data));
		BOUT_END(this);
	}
	else
	{
		// Send only light
		BOUT_BEGIN(this);
		Bout << NETMSG_CRITTER_ITEM_DATA_HALF;
		Bout << from_cr->GetId();
		Bout << slot;
		Bout.Push((char*)&item->Data.LightIntensity,7);
		BOUT_END(this);
	}
}

void Client::Send_Animate(Critter* from_cr, DWORD anim1, DWORD anim2, Item* item, bool clear_sequence, bool delay_play)
{
	if(IsSendDisabled() || IsOffline()) return;
	if(clear_sequence) Send_XY(from_cr);
	if(item) Send_SomeItem(item);

	BOUT_BEGIN(this);
	Bout << NETMSG_CRITTER_ANIMATE;
	Bout << from_cr->GetId();
	Bout << anim1;
	Bout << anim2;
	Bout << (bool)(item?true:false);
	Bout << clear_sequence;
	Bout << delay_play;
	BOUT_END(this);
}

void Client::Send_AddItemOnMap(Item* item)
{
	if(IsSendDisabled() || IsOffline()) return;

	BYTE is_added=item->ViewPlaceOnMap;

	BOUT_BEGIN(this);
	Bout << NETMSG_ADD_ITEM_ON_MAP;
	Bout << item->GetId();
	Bout << item->GetProtoId();
	Bout << item->ACC_HEX.HexX;
	Bout << item->ACC_HEX.HexY;
	Bout << is_added;
	Bout.Push((char*)&item->Data,sizeof(item->Data));
	BOUT_END(this);

	if(item->PLexems) Send_ItemLexems(item);
}

void Client::Send_ChangeItemOnMap(Item* item)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_CHANGE_ITEM_ON_MAP;
	Bout << item->GetId();
	Bout.Push((char*)&item->Data,sizeof(item->Data));
	BOUT_END(this);

	if(item->PLexems) Send_ItemLexems(item);
}

void Client::Send_EraseItemFromMap(Item* item)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_ERASE_ITEM_FROM_MAP;
	Bout << item->GetId();
	Bout << item->ViewPlaceOnMap;
	BOUT_END(this);
}

void Client::Send_AnimateItem(Item* item, BYTE from_frm, BYTE to_frm)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_ANIMATE_ITEM;
	Bout << item->GetId();
	Bout << from_frm;
	Bout << to_frm;
	BOUT_END(this);
}

void Client::Send_AddItem(Item* item)
{
	if(IsSendDisabled() || IsOffline()) return;
	if(item->IsHidden()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_ADD_ITEM;
	Bout << item->GetId();
	Bout << item->GetProtoId();
	Bout << item->ACC_CRITTER.Slot;
	Bout.Push((char*)&item->Data,sizeof(item->Data));
	BOUT_END(this);

	if(item->PLexems) Send_ItemLexems(item);
}

void Client::Send_EraseItem(Item* item)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_REMOVE_ITEM;
	Bout << item->GetId();
	BOUT_END(this);
}

void Client::Send_ContainerInfo()
{
	if(IsSendDisabled() || IsOffline()) return;

	BYTE transfer_type=TRANSFER_CLOSE;
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(transfer_type);

	BOUT_BEGIN(this);
	Bout << NETMSG_CONTAINER_INFO;
	Bout << msg_len;
	Bout << transfer_type;
	BOUT_END(this);

	AccessContainerId=0;
}

void Client::Send_ContainerInfo(Item* item_cont, BYTE transfer_type, bool open_screen)
{
	if(IsSendDisabled() || IsOffline()) return;
	if(item_cont->GetType()!=ITEM_TYPE_CONTAINER) return;

	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(BYTE)+sizeof(DWORD)+sizeof(DWORD)+sizeof(WORD)+sizeof(DWORD);
	ItemPtrVec items;
	item_cont->ContGetAllItems(items,true);
	if(items.size()) msg_len+=items.size()*(sizeof(DWORD)+sizeof(WORD)+sizeof(Item::ItemData));
	if(open_screen) SETFLAG(transfer_type,0x80);

	if(items.size()>=100)
	{
		Map* map=MapMngr.GetMap(item_cont->ACC_HEX.MapId);
		WriteLog(__FUNCTION__" - Sending too much items<%u>, container pid<%u>, map pid<%u>, hx<%u>, hy<%u>.\n",items.size(),item_cont->GetProtoId(),map?map->GetPid():0,item_cont->ACC_HEX.HexX,item_cont->ACC_HEX.HexY);
	}

	BOUT_BEGIN(this);
	Bout << NETMSG_CONTAINER_INFO;
	Bout << msg_len;
	Bout << transfer_type;
	Bout << DWORD(0);
	Bout << item_cont->GetId();
	Bout << item_cont->GetProtoId();
	Bout << DWORD(items.size());

	for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=*it;
		Bout << item->GetId();
		Bout << item->GetProtoId();
		Bout.Push((char*)&item->Data,sizeof(item->Data));
	}
	BOUT_END(this);

	for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=*it;
		if(item->PLexems) Send_ItemLexems(item);
	}

	AccessContainerId=item_cont->GetId();
}

void Client::Send_ContainerInfo(Critter* cr_cont, BYTE transfer_type, bool open_screen)
{
	if(IsSendDisabled() || IsOffline()) return;

	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(BYTE)+sizeof(DWORD)+sizeof(DWORD)+sizeof(WORD)+sizeof(DWORD);
	ItemPtrVec items;
	cr_cont->GetInvItems(items,transfer_type);
	WORD barter_k=0;
	if(transfer_type==TRANSFER_CRIT_BARTER)
	{
		if(cr_cont->GetSkill(SK_BARTER)>GetSkill(SK_BARTER)) barter_k=cr_cont->GetSkill(SK_BARTER)-GetSkill(SK_BARTER);
		barter_k=CLAMP(barter_k,5,95);
		if(cr_cont->GetParam(ST_FREE_BARTER_PLAYER)==GetId()) barter_k=0;
	}

	if(open_screen) SETFLAG(transfer_type,0x80);
	msg_len+=items.size()*(sizeof(DWORD)+sizeof(WORD)+sizeof(Item::ItemData));

	if(items.size()>=1000) WriteLog(__FUNCTION__" - Sending too much items<%u>, from critter<%s>, to critter<%s>.\n",items.size(),cr_cont->GetInfo(),GetInfo());

	BOUT_BEGIN(this);
	Bout << NETMSG_CONTAINER_INFO;
	Bout << msg_len;
	Bout << transfer_type;
	Bout << (DWORD)Talk.TalkTime;
	Bout << cr_cont->GetId();
	Bout << barter_k;
	Bout << (DWORD)items.size();

	for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=*it;
		Bout << item->GetId();
		Bout << item->GetProtoId();
		Bout.Push((char*)&item->Data,sizeof(item->Data));
	}
	BOUT_END(this);

	for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=*it;
		if(item->PLexems) Send_ItemLexems(item);
	}

	AccessContainerId=cr_cont->GetId();
}

#define SEND_LOCATION_SIZE            (16)
void Client::Send_GlobalInfo(BYTE info_flags)
{
	if(IsSendDisabled() || IsOffline()) return;

	CritDataExt* data_ext=GetDataExt();
	if(!data_ext) return;

	Item* car=(FLAG(info_flags,GM_INFO_GROUP_PARAM)?GroupMove->GetCar():NULL);

	// Calculate length of message
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(info_flags);

	WORD loc_count=data_ext->LocationsCount;
	if(FLAG(info_flags,GM_INFO_LOCATIONS))
		msg_len+=sizeof(loc_count)+SEND_LOCATION_SIZE*loc_count;

	if(FLAG(info_flags,GM_INFO_GROUP_PARAM))
	{
		msg_len+=sizeof(WORD)*4+sizeof(int)*2+sizeof(BYTE)+sizeof(WORD); //xi, yi, move_x, move_y, speed_x and speed_y, wait, car_pid
		if(GroupMove->CarId) msg_len+=sizeof(DWORD)+sizeof(WORD)*4; //car_master, fuel, fuel_tank, wear, run_to_break
	}

	if(FLAG(info_flags,GM_INFO_ZONES_FOG)) msg_len+=GM_ZONES_FOG_SIZE;

	// Parse message
	BOUT_BEGIN(this);
	Bout << NETMSG_GLOBAL_INFO;
	Bout << msg_len;
	Bout << info_flags;

	if(FLAG(info_flags,GM_INFO_LOCATIONS))
	{
		Bout << loc_count;

		char empty_loc[SEND_LOCATION_SIZE]={0,0,0,0};
		for(int i=0;i<data_ext->LocationsCount;)
		{
			DWORD loc_id=data_ext->LocationsId[i];
			Location* loc=MapMngr.GetLocation(loc_id);
			if(loc && !loc->Data.ToGarbage)
			{
				i++;
				Bout << loc_id;
				Bout << loc->GetPid();
				Bout << loc->Data.WX;
				Bout << loc->Data.WY;
				Bout << loc->Data.Radius;
				Bout << loc->Data.Color;
			}
			else
			{
				EraseKnownLoc(loc_id);
				Bout.Push(empty_loc,sizeof(empty_loc));
			}
		}
	}

	if(FLAG(info_flags,GM_INFO_GROUP_PARAM))
	{
		int speed_x=(GroupMove->SpeedX*1000000);
		int speed_y=(GroupMove->SpeedY*1000000);
		BYTE wait=(GroupMove->EncounterDescriptor?1:0);

		Bout << (WORD)(GroupMove->WXi);
		Bout << (WORD)(GroupMove->WYi);
		Bout << (WORD)(GroupMove->MoveX);
		Bout << (WORD)(GroupMove->MoveY);
		Bout << speed_x;
		Bout << speed_y;
		Bout << wait;

		if(car) Bout << car->ACC_CRITTER.Id;
		else Bout << (DWORD)0;
	}

	if(FLAG(info_flags,GM_INFO_ZONES_FOG))
	{
		Bout.Push((char*)data_ext->GlobalMapFog,GM_ZONES_FOG_SIZE);
	}
	BOUT_END(this);

	if(FLAG(info_flags,GM_INFO_CRITTERS)) ProcessVisibleCritters();
	if(FLAG(info_flags,GM_INFO_GROUP_PARAM) && car) Send_AddItemOnMap(car);
}

void Client::Send_GlobalLocation(Location* loc, bool add)
{
	if(IsSendDisabled() || IsOffline()) return;

	BYTE info_flags=GM_INFO_LOCATION;
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(info_flags)+sizeof(add)+SEND_LOCATION_SIZE;

	BOUT_BEGIN(this);
	Bout << NETMSG_GLOBAL_INFO;
	Bout << msg_len;
	Bout << info_flags;
	Bout << add;
	Bout << loc->GetId();
	Bout << loc->GetPid();
	Bout << loc->Data.WX;
	Bout << loc->Data.WY;
	Bout << loc->Data.Radius;
	Bout << loc->Data.Color;
	BOUT_END(this);
}

void Client::Send_GlobalMapFog(WORD zx, WORD zy, BYTE fog)
{
	if(IsSendDisabled() || IsOffline()) return;

	BYTE info_flags=GM_INFO_FOG;
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(info_flags)+sizeof(zx)+sizeof(zy)+sizeof(fog);

	BOUT_BEGIN(this);
	Bout << NETMSG_GLOBAL_INFO;
	Bout << msg_len;
	Bout << info_flags;
	Bout << zx;
	Bout << zy;
	Bout << fog;
	BOUT_END(this);
}

void Client::Send_XY(Critter* cr)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_CRITTER_XY;
	Bout << cr->GetId();
	Bout << cr->Data.HexX;
	Bout << cr->Data.HexY;
	Bout << cr->Data.Dir;
	BOUT_END(this);
}

void Client::Send_AllParams()
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_ALL_PARAMS;
	Bout.Push((char*)Data.Params,sizeof(Data.Params));
	BOUT_END(this);
}

void Client::Send_Param(WORD num_param)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_PARAM;
	Bout << num_param;
	Bout << Data.Params[num_param];
	BOUT_END(this);
}

void Client::Send_ParamOther(WORD num_param, int val)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_PARAM;
	Bout << num_param;
	Bout << val;
	BOUT_END(this);
}

void Client::Send_CritterParam(Critter* cr, WORD num_param, int other_val)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_CRITTER_PARAM;
	Bout << cr->GetId();
	Bout << num_param;
	if(num_param<MAX_PARAMS) Bout << cr->Data.Params[num_param];
	else Bout << other_val;
	BOUT_END(this);
}

void Client::Send_Talk()
{
	if(IsSendDisabled() || IsOffline()) return;

	bool close=(Talk.TalkType==TALK_NONE);
	BYTE is_npc=(Talk.TalkType==TALK_WITH_NPC);
	DWORD talk_id=(is_npc?Talk.TalkNpc:Talk.DialogPackId);
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(is_npc)+sizeof(talk_id)+sizeof(BYTE);

	BOUT_BEGIN(this);
	Bout << NETMSG_TALK_NPC;

	if(close)
	{
		Bout << msg_len;
		Bout << is_npc;
		Bout << talk_id;
		Bout << BYTE(0);
	}
	else
	{
		BYTE all_answers=(BYTE)Talk.CurDialog.Answers.size();
		msg_len+=sizeof(DWORD)+sizeof(DWORD)*all_answers+sizeof(DWORD)+sizeof(WORD)+Talk.Lexems.length();

		Bout << msg_len;
		Bout << is_npc;
		Bout << talk_id;
		Bout << all_answers;
		Bout << (WORD)Talk.Lexems.length(); //Lexems length
		if(Talk.Lexems.length()) Bout.Push(Talk.Lexems.c_str(),Talk.Lexems.length()); //Lexems string
		Bout << Talk.CurDialog.TextId; //Main text_id
		for(AnswersVecIt it=Talk.CurDialog.Answers.begin(),end=Talk.CurDialog.Answers.end();it!=end;++it) Bout << (*it).TextId; //Answers text_id
		Bout << DWORD(Talk.TalkTime-(Timer::FastTick()-Talk.StartTick)); //Talk time
	}
	BOUT_END(this);
}

void Client::Send_GameInfo(Map* map)
{
	if(IsSendDisabled() || IsOffline()) return;

	int time=(map?map->GetTime():-1);
	BYTE rain=(map?map->GetRain():0);
	bool turn_based=(map?map->IsTurnBasedOn:false);
	bool no_log_out=(map?map->IsNoLogOut():true);
	static const int day_time_dummy[4]={0};
	static const BYTE day_color_dummy[12]={0};
	const int* day_time=(map?map->Data.MapDayTime:day_time_dummy);
	const BYTE* day_color=(map?map->Data.MapDayColor:day_color_dummy);

	BOUT_BEGIN(this);
	Bout << NETMSG_GAME_INFO;
	Bout << GameOpt.Year;
	Bout << GameOpt.Month;
	Bout << GameOpt.Day;
	Bout << GameOpt.Hour;
	Bout << GameOpt.Minute;
	Bout << GameOpt.TimeMultiplier;
	Bout << time;
	Bout << rain;
	Bout << turn_based;
	Bout << no_log_out;
	Bout.Push((const char*)day_time,sizeof(day_time_dummy));
	Bout.Push((const char*)day_color,sizeof(day_color_dummy));
	BOUT_END(this);
}

void Client::Send_Text(Critter* from_cr, const char* s_str, BYTE how_say)
{
	if(IsSendDisabled() || IsOffline()) return;
	if(!s_str || !s_str[0]) return;
	WORD s_len=strlen(s_str);
	DWORD from_id=(from_cr?from_cr->GetId():0);
	WORD intellect=(from_cr && how_say>=SAY_NORM && how_say<=SAY_RADIO?from_cr->GetSayIntellect():0);
	Send_TextEx(from_id,s_str,s_len,how_say,intellect,false);
}

void Client::Send_TextEx(DWORD from_id, const char* s_str, WORD str_len, BYTE how_say, WORD intellect, bool unsafe_text)
{
	if(IsSendDisabled() || IsOffline()) return;

	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(from_id)+sizeof(how_say)+
		sizeof(intellect)+sizeof(unsafe_text)+sizeof(str_len)+str_len;

	BOUT_BEGIN(this);
	Bout << NETMSG_CRITTER_TEXT;
	Bout << msg_len;
	Bout << from_id;
	Bout << how_say;
	Bout << intellect;
	Bout << unsafe_text;
	Bout << str_len;
	Bout.Push(s_str,str_len);
	BOUT_END(this);
}

void Client::Send_TextMsg(Critter* from_cr, DWORD num_str, BYTE how_say, WORD num_msg)
{
	if(IsSendDisabled() || IsOffline()) return;
	if(!num_str) return;
	DWORD from_id=(from_cr?from_cr->GetId():0);

	BOUT_BEGIN(this);
	Bout << NETMSG_MSG;
	Bout << from_id;
	Bout << how_say;
	Bout << num_msg;
	Bout << num_str;
	BOUT_END(this);
}

void Client::Send_TextMsgLex(Critter* from_cr, DWORD num_str, BYTE how_say, WORD num_msg, const char* lexems)
{
	if(IsSendDisabled() || IsOffline()) return;
	if(!num_str) return;

	WORD lex_len=strlen(lexems);
	if(!lex_len || lex_len>MAX_DLG_LEXEMS_TEXT)
	{
		Send_TextMsg(from_cr,num_str,how_say,num_msg);
		return;
	}

	DWORD from_id=(from_cr?from_cr->GetId():0);
	DWORD msg_len=NETMSG_MSG_SIZE+sizeof(lex_len)+lex_len;

	BOUT_BEGIN(this);
	Bout << NETMSG_MSG_LEX;
	Bout << msg_len;
	Bout << from_id;
	Bout << how_say;
	Bout << num_msg;
	Bout << num_str;
	Bout << lex_len;
	Bout.Push(lexems,lex_len);
	BOUT_END(this);
}

void Client::Send_MapText(WORD hx, WORD hy, DWORD color, const char* text)
{
	if(IsSendDisabled() || IsOffline()) return;

	WORD text_len=strlen(text);
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(hx)+sizeof(hy)+sizeof(color)+sizeof(text_len)+text_len;

	BOUT_BEGIN(this);
	Bout << NETMSG_MAP_TEXT;
	Bout << msg_len;
	Bout << hx;
	Bout << hy;
	Bout << color;
	Bout << text_len;
	Bout.Push(text,text_len);
	BOUT_END(this);
}

void Client::Send_MapTextMsg(WORD hx, WORD hy, DWORD color, WORD num_msg, DWORD num_str)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_MAP_TEXT_MSG;
	Bout << hx;
	Bout << hy;
	Bout << color;
	Bout << num_msg;
	Bout << num_str;
	BOUT_END(this);
}

void Client::Send_MapTextMsgLex(WORD hx, WORD hy, DWORD color, WORD num_msg, DWORD num_str, const char* lexems)
{
	if(IsSendDisabled() || IsOffline()) return;

	WORD lexems_len=strlen(lexems);
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(WORD)*2+sizeof(DWORD)+sizeof(WORD)+sizeof(DWORD)+sizeof(lexems_len)+lexems_len;

	BOUT_BEGIN(this);
	Bout << NETMSG_MAP_TEXT_MSG_LEX;
	Bout << msg_len;
	Bout << hx;
	Bout << hy;
	Bout << color;
	Bout << num_msg;
	Bout << num_str;
	Bout << lexems_len;
	Bout.Push(lexems,lexems_len);
	BOUT_END(this);
}

void Client::Send_CombatResult(DWORD* combat_res, DWORD len)
{
	if(IsSendDisabled() || IsOffline()) return;
	if(!combat_res || len>GameOpt.FloodSize/sizeof(DWORD)) return;

	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(len)+len*sizeof(DWORD);

	BOUT_BEGIN(this);
	Bout << NETMSG_COMBAT_RESULTS;
	Bout << msg_len;
	Bout << len;
	if(len) Bout.Push((char*)combat_res,len*sizeof(DWORD));
	BOUT_END(this);
}

void Client::Send_Quest(DWORD num)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_QUEST;
	Bout << num;
	BOUT_END(this);
}

void Client::Send_Quests(DwordVec& nums)
{
	if(IsSendDisabled() || IsOffline()) return;

	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(DWORD)+sizeof(DWORD)*nums.size();

	BOUT_BEGIN(this);
	Bout << NETMSG_QUESTS;
	Bout << msg_len;
	Bout << (DWORD)nums.size();
	if(nums.size()) Bout.Push((char*)&nums[0],nums.size()*sizeof(DWORD));
	BOUT_END(this);
}

void Client::Send_HoloInfo(BYTE clear, WORD offset, WORD count)
{
	if(IsSendDisabled() || IsOffline()) return;
	if(offset>=MAX_HOLO_INFO || count>MAX_HOLO_INFO || offset+count>MAX_HOLO_INFO) return;

	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(clear)+sizeof(offset)+sizeof(count)+count*sizeof(DWORD);

	BOUT_BEGIN(this);
	Bout << NETMSG_HOLO_INFO;
	Bout << msg_len;
	Bout << clear;
	Bout << offset;
	Bout << count;
	if(count) Bout.Push((char*)&Data.HoloInfo[offset],count*sizeof(DWORD));
	BOUT_END(this);
}

void Client::Send_UserHoloStr(DWORD str_num, const char* text, WORD text_len)
{
	if(IsSendDisabled() || IsOffline()) return;

	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(str_num)+sizeof(text_len)+text_len;

	BOUT_BEGIN(this);
	Bout << NETMSG_USER_HOLO_STR;
	Bout << msg_len;
	Bout << str_num;
	Bout << text_len;
	Bout.Push(text,text_len);
	BOUT_END(this);
}

void Client::Send_Follow(DWORD rule, BYTE follow_type, WORD map_pid, DWORD follow_wait)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_FOLLOW;
	Bout << rule;
	Bout << follow_type;
	Bout << map_pid;
	Bout << follow_wait;
	BOUT_END(this);
}

void Client::Send_Effect(WORD eff_pid, WORD hx, WORD hy, WORD radius)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_EFFECT;
	Bout << eff_pid;
	Bout << hx;
	Bout << hy;
	Bout << radius;
	BOUT_END(this);
}

void Client::Send_FlyEffect(WORD eff_pid, DWORD from_crid, DWORD to_crid, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_FLY_EFFECT;
	Bout << eff_pid;
	Bout << from_crid;
	Bout << to_crid;
	Bout << from_hx;
	Bout << from_hy;
	Bout << to_hx;
	Bout << to_hy;
	BOUT_END(this);
}

void Client::Send_PlaySound(DWORD crid_synchronize, const char* sound_name)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_PLAY_SOUND;
	Bout << crid_synchronize;
	Bout.Push(sound_name,16);
	BOUT_END(this);
}

void Client::Send_PlaySoundType(DWORD crid_synchronize, BYTE sound_type, BYTE sound_type_ext, BYTE sound_id, BYTE sound_id_ext)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_PLAY_SOUND_TYPE;
	Bout << crid_synchronize;
	Bout << sound_type;
	Bout << sound_type_ext;
	Bout << sound_id;
	Bout << sound_id_ext;
	BOUT_END(this);
}

void Client::Send_CritterLexems(Critter* cr)
{
	if(IsSendDisabled() || IsOffline()) return;

	DWORD critter_id=cr->GetId();
	WORD lexems_len=strlen(cr->Data.Lexems);
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(critter_id)+sizeof(lexems_len)+lexems_len;

	BOUT_BEGIN(this);
	Bout << NETMSG_CRITTER_LEXEMS;
	Bout << msg_len;
	Bout << critter_id;
	Bout << lexems_len;
	Bout.Push(cr->Data.Lexems,lexems_len);
	BOUT_END(this);
}

void Client::Send_PlayersBarter(BYTE barter, DWORD param, DWORD param_ext)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_PLAYERS_BARTER;
	Bout << barter;
	Bout << param;
	Bout << param_ext;
	BOUT_END(this);
}

void Client::Send_PlayersBarterSetHide(Item* item, DWORD count)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_PLAYERS_BARTER_SET_HIDE;
	Bout << item->GetId();
	Bout << item->GetProtoId();
	Bout << count;
	Bout.Push((char*)&item->Data,sizeof(item->Data));
	BOUT_END(this);

	if(item->PLexems) Send_ItemLexems(item);
}

void Client::Send_ShowScreen(int screen_type, DWORD param, bool need_answer)
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_SHOW_SCREEN;
	Bout << screen_type;
	Bout << param;
	Bout << need_answer;
	BOUT_END(this);
}

void Client::Send_RunClientScript(const char* func_name, int p0, int p1, int p2, const char* p3, DwordVec& p4)
{
	if(IsSendDisabled() || IsOffline()) return;

	WORD func_name_len=strlen(func_name);
	WORD p3len=(p3?strlen(p3):0);
	WORD p4size=p4.size();
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(func_name_len)+func_name_len+sizeof(p0)+sizeof(p1)+sizeof(p2)+sizeof(p3len)+p3len+sizeof(p4size)+p4size*sizeof(DWORD);

	BOUT_BEGIN(this);
	Bout << NETMSG_RUN_CLIENT_SCRIPT;
	Bout << msg_len;
	Bout << func_name_len;
	Bout.Push(func_name,func_name_len);
	Bout << p0;
	Bout << p1;
	Bout << p2;
	Bout << p3len;
	if(p3len) Bout.Push(p3,p3len);
	Bout << p4size;
	if(p4size) Bout.Push((char*)&p4[0],p4size*sizeof(DWORD));
	BOUT_END(this);
}

void Client::Send_DropTimers()
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_DROP_TIMERS;
	BOUT_END(this);
}

void Client::Send_ViewMap()
{
	if(IsSendDisabled() || IsOffline()) return;

	BOUT_BEGIN(this);
	Bout << NETMSG_VIEW_MAP;
	Bout << ViewMapHx;
	Bout << ViewMapHy;
	Bout << ViewMapLocId;
	Bout << ViewMapLocEnt;
	BOUT_END(this);
}

void Client::Send_ItemLexems(Item* item) // Already checks for client offline and lexems valid
{
	if(IsSendDisabled()) return;

	DWORD item_id=item->GetId();
	WORD lexems_len=strlen(item->PLexems);
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(item_id)+sizeof(lexems_len)+lexems_len;

	BOUT_BEGIN(this);
	Bout << NETMSG_ITEM_LEXEMS;
	Bout << msg_len;
	Bout << item_id;
	Bout << lexems_len;
	Bout.Push(item->PLexems,lexems_len);
	BOUT_END(this);
}

void Client::Send_ItemLexemsNull(Item* item) // Already checks for client offline and lexems valid
{
	if(IsSendDisabled()) return;

	DWORD item_id=item->GetId();
	WORD lexems_len=0;
	DWORD msg_len=sizeof(MSGTYPE)+sizeof(msg_len)+sizeof(item_id)+sizeof(lexems_len);

	BOUT_BEGIN(this);
	Bout << NETMSG_ITEM_LEXEMS;
	Bout << msg_len;
	Bout << item_id;
	Bout << lexems_len;
	BOUT_END(this);
}

void Client::Send_CheckUIDS()
{
	if(IsSendDisabled()) return;

	const MSGTYPE uid_msg[5]={NETMSG_CHECK_UID0,NETMSG_CHECK_UID1,NETMSG_CHECK_UID2,NETMSG_CHECK_UID3,NETMSG_CHECK_UID4};
	MSGTYPE msg=uid_msg[Random(0,4)];
	BYTE rnd_count=Random(1,21);
	BYTE rnd_count2=Random(2,12);
	DWORD uidxor[5]={Random(1,0xFFFFFFFF),Random(1,0xFFFFFFFF),Random(1,0xFFFFFFFF),Random(1,0xFFFFFFFF),Random(1,0xFFFFFFFF)};
	DWORD uid[5]={UID[0]^uidxor[0],UID[1]^uidxor[1],UID[2]^uidxor[2],UID[3]^uidxor[3],UID[4]^uidxor[4]};
	DWORD msg_len=sizeof(msg)+sizeof(msg_len)+sizeof(uid)+sizeof(uidxor)+sizeof(rnd_count)*2+rnd_count+rnd_count2;

	BOUT_BEGIN(this);
	Bout << msg;
	Bout << msg_len;
	Bout << uid[3];
	Bout << uidxor[0];
	Bout << rnd_count;
	Bout << uid[1];
	Bout << uidxor[2];
	for(int i=0;i<rnd_count;i++) Bout << (BYTE)Random(0,255);
	Bout << uid[2];
	Bout << uidxor[1];
	Bout << uid[4];
	Bout << rnd_count2;
	Bout << uidxor[3];
	Bout << uidxor[4];
	Bout << uid[0];
	for(int i=0;i<rnd_count2;i++) Bout << (BYTE)Random(0,255);
	BOUT_END(this);
}

void Client::Send_SomeItem(Item* item)
{
	BOUT_BEGIN(this);
	Bout << NETMSG_SOME_ITEM;
	Bout << item->GetId();
	Bout << item->GetProtoId();
	Bout << item->ACC_CRITTER.Slot;
	Bout.Push((char*)&item->Data,sizeof(item->Data));
	BOUT_END(this);
}

/************************************************************************/
/* Radio                                                                */
/************************************************************************/

Item* Client::GetRadio()
{
	if(ItemSlotMain->IsRadio()) return ItemSlotMain;
	if(ItemSlotExt->IsRadio()) return ItemSlotExt;
	return NULL;
}

WORD Client::GetRadioChannel()
{
	if(ItemSlotMain->IsRadio()) return ItemSlotMain->RadioGetChannel();
	if(ItemSlotExt->IsRadio()) return ItemSlotExt->RadioGetChannel();
	return 0;
}

/************************************************************************/
/* Params                                                               */
/************************************************************************/

bool Client::IsImplementor()
{
	return FLAG(Access,ACCESS_IMPLEMENTOR);
}

/************************************************************************/
/* Locations                                                            */
/************************************************************************/

bool Client::CheckKnownLocById(DWORD loc_id)
{
	if(!loc_id) return false;
	CritDataExt* data_ext=GetDataExt();
	if(!data_ext) return false;

	for(int i=0;i<data_ext->LocationsCount;i++)
		if(data_ext->LocationsId[i]==loc_id) return true;
	return false;
}

bool Client::CheckKnownLocByPid(WORD loc_pid)
{
	if(!loc_pid) return false;
	CritDataExt* data_ext=GetDataExt();
	if(!data_ext) return false;

	for(int i=0;i<data_ext->LocationsCount;i++)
	{
		Location* loc=MapMngr.GetLocation(data_ext->LocationsId[i]);
		if(loc && loc->GetPid()==loc_pid) return true;
	}
	return false;
}

void Client::AddKnownLoc(DWORD loc_id)
{
	CritDataExt* data_ext=GetDataExt();
	if(!data_ext) return;

	if(data_ext->LocationsCount>=MAX_STORED_LOCATIONS)
	{
		// Erase 100..199
		memcpy(&data_ext->LocationsId[100],&data_ext->LocationsId[200],MAX_STORED_LOCATIONS-100);
		data_ext->LocationsCount=MAX_STORED_LOCATIONS-100;
	}

	for(int i=0;i<data_ext->LocationsCount;i++)
		if(data_ext->LocationsId[i]==loc_id) return;

	data_ext->LocationsId[data_ext->LocationsCount]=loc_id;
	data_ext->LocationsCount++;
}

void Client::EraseKnownLoc(DWORD loc_id)
{
	if(!loc_id) return;
	CritDataExt* data_ext=GetDataExt();
	if(!data_ext) return;

	for(int i=0;i<data_ext->LocationsCount;i++)
	{
		DWORD& cur_loc_id=data_ext->LocationsId[i];
		if(cur_loc_id==loc_id)
		{
			// Swap
			DWORD& last_loc_id=data_ext->LocationsId[data_ext->LocationsCount-1];
			if(i!=data_ext->LocationsCount-1) cur_loc_id=last_loc_id;
			data_ext->LocationsCount--;
			return;
		}
	}
}

/************************************************************************/
/* Players barter                                                       */
/************************************************************************/

Client* Client::BarterGetOpponent(DWORD opponent_id)
{
	if(!GetMap() && !GroupMove)
	{
		BarterEnd();
		return NULL;
	}

	if(BarterOpponent && BarterOpponent!=opponent_id)
	{
		Critter* cr=(GetMap()?GetCritSelf(BarterOpponent):GroupMove->GetCritter(BarterOpponent));
		if(cr && cr->IsPlayer()) ((Client*)cr)->BarterEnd();
		BarterEnd();
	}

	Critter* cr=(GetMap()?GetCritSelf(opponent_id):GroupMove->GetCritter(opponent_id));
	if(!cr || !cr->IsPlayer() || !cr->IsLife() || cr->IsBusy() || cr->GetTimeout(TO_BATTLE) || ((Client*)cr)->IsOffline() ||
	  (GetMap() && !CheckDist(GetHexX(),GetHexY(),cr->GetHexX(),cr->GetHexY(),BARTER_DIST)))
	{
		if(cr && cr->IsPlayer() && BarterOpponent==cr->GetId()) ((Client*)cr)->BarterEnd();
		BarterEnd();
		return NULL;
	}

	BarterOpponent=cr->GetId();
	return (Client*)cr;
}

void Client::BarterEnd()
{
	BarterOpponent=0;
	Send_PlayersBarter(BARTER_END,0,0);
}

void Client::BarterRefresh(Client* opponent)
{
	Send_PlayersBarter(BARTER_REFRESH,opponent->GetId(),opponent->BarterHide);
	Send_PlayersBarter(BARTER_OFFER,BarterOffer,false);
	Send_PlayersBarter(BARTER_OFFER,opponent->BarterOffer,true);
	if(!opponent->BarterHide) Send_ContainerInfo(opponent,TRANSFER_CRIT_BARTER,false);
	for(BarterItemVecIt it=BarterItems.begin();it!=BarterItems.end();)
	{
		BarterItem& barter_item=*it;
		if(!GetItem(barter_item.Id,true)) it=BarterItems.erase(it);
		else
		{
			Send_PlayersBarter(BARTER_SET_SELF,barter_item.Id,barter_item.Count);
			++it;
		}
	}
	for(BarterItemVecIt it=opponent->BarterItems.begin();it!=opponent->BarterItems.end();)
	{
		BarterItem& barter_item=*it;
		if(!opponent->GetItem(barter_item.Id,true)) it=opponent->BarterItems.erase(it);
		else
		{
			if(opponent->BarterHide) Send_PlayersBarterSetHide(opponent->GetItem(barter_item.Id,true),barter_item.Count);
			else Send_PlayersBarter(BARTER_SET_OPPONENT,barter_item.Id,barter_item.Count);
			++it;
		}
	}
}

Client::BarterItem* Client::BarterGetItem(DWORD item_id)
{
	BarterItemVecIt it=std::find(BarterItems.begin(),BarterItems.end(),item_id);
	return it!=BarterItems.end()?&(*it):NULL;
}

void Client::BarterEraseItem(DWORD item_id)
{
	BarterItemVecIt it=std::find(BarterItems.begin(),BarterItems.end(),item_id);
	if(it!=BarterItems.end()) BarterItems.erase(it);
}

/************************************************************************/
/* Timers                                                               */
/************************************************************************/

void Client::DropTimers(bool send)
{
	LastSendScoresTick=0;
	LastSendCraftTick=0;
	LastSendEntrancesTick=0;
	LastSendEntrancesLocId=0;
	if(send) Send_DropTimers();
}

/************************************************************************/
/* Dialogs                                                              */
/************************************************************************/

void Client::ProcessTalk(bool force)
{
	if(!force && Timer::FastTick()<talkNextTick) return;
	talkNextTick=Timer::FastTick()+PROCESS_TALK_TICK;
	if(Talk.TalkType==TALK_NONE) return;

	// Check time of talk
	if(Timer::FastTick()-Talk.StartTick>Talk.TalkTime)
	{
		CloseTalk();
		return;
	}

	// Check npc
	Npc* npc=NULL;
	if(Talk.TalkType==TALK_WITH_NPC)
	{
		npc=CrMngr.GetNpc(Talk.TalkNpc);
		if(!npc)
		{
			CloseTalk();
			return;
		}

		if(!npc->IsLife())
		{
			Send_TextMsg(this,STR_DIALOG_NPC_NOT_LIFE,SAY_NETMSG,TEXTMSG_GAME);
			CloseTalk();
			return;
		}

		if(npc->IsPlaneNoTalk())
		{
			Send_TextMsg(this,STR_DIALOG_NPC_BUSY,SAY_NETMSG,TEXTMSG_GAME);
			CloseTalk();
			return;
		}
	}

	// Check distance
	if(!Talk.IgnoreDistance)
	{
		DWORD map_id;
		WORD hx,hy;
		DWORD talk_distance;
		if(Talk.TalkType==TALK_WITH_NPC)
		{
			map_id=npc->GetMap();
			hx=npc->GetHexX();
			hy=npc->GetHexY();
			talk_distance=npc->GetTalkDistance();
		}
		else if(Talk.TalkType==TALK_WITH_HEX)
		{
			map_id=Talk.TalkHexMap;
			hx=Talk.TalkHexX;
			hy=Talk.TalkHexY;
			talk_distance=GameOpt.TalkDistance;
		}

		if(GetMap()!=map_id || !CheckDist(GetHexX(),GetHexY(),hx,hy,talk_distance))
		{
			Send_TextMsg(this,STR_DIALOG_DIST_TOO_LONG,SAY_NETMSG,TEXTMSG_GAME);
			CloseTalk();
		}
	}
}

void Client::CloseTalk()
{
	if(Talk.TalkType!=TALK_NONE)
	{
		Npc* npc=NULL;
		if(Talk.TalkType==TALK_WITH_NPC)
		{
			Talk.TalkType=TALK_NONE;
			npc=CrMngr.GetNpc(Talk.TalkNpc);
			if(npc)
			{
				if(Talk.Barter) npc->EventBarter(this,false,npc->GetBarterPlayers());
				npc->EventTalk(this,false,npc->GetTalkedPlayers());
			}
		}

		int func_id=Talk.CurDialog.DlgScript;
		switch(func_id)
		{
		case NOT_ANSWER_CLOSE_DIALOG:
			break;
		case NOT_ANSWER_BEGIN_BATTLE:
			{
				if(!npc) break;
				npc->SetTarget(REASON_FROM_DIALOG,this,GameOpt.DeadHitPoints,false);
			}
			break;
		default:
			if(func_id<=0) break;
			if(!Script::PrepareContext(func_id,CALL_FUNC_STR,GetInfo())) break;
			Script::SetArgObject(this);
			Script::SetArgObject(npc);
			Script::SetArgObject(NULL);
			Talk.Locked=true;
			Script::RunPrepared();
			Talk.Locked=false;
			break;
		}
	}

	Talk.Clear();
	Send_Talk();
}

/************************************************************************/
/* NPC                                                                  */
/************************************************************************/

Npc::Npc():
NextRefreshBagTick(0),lastBattleWeaponId(0),lastBattleWeaponUse(0)
{
	CritterIsNpc=true;
	MEMORY_PROCESS(MEMORY_NPC,sizeof(Npc)+sizeof(GlobalMapGroup)+40);
	SETFLAG(Flags,FCRIT_NPC);
}

Npc::~Npc()
{
	MEMORY_PROCESS(MEMORY_NPC,-(int)(sizeof(Npc)+sizeof(GlobalMapGroup)+40));
	if(DataExt)
	{
		MEMORY_PROCESS(MEMORY_NPC,-(int)sizeof(CritDataExt));
		SAFEDEL(DataExt);
	}
	DropPlanes();
}

void Npc::RefreshBag()
{
	if(Data.BagRefreshTime<0) return;
	NextRefreshBagTick=Timer::FastTick()+(Data.BagRefreshTime?Data.BagRefreshTime:GameOpt.BagRefreshTime)*60*1000;

	// Collect pids and count
	static DWORD pids[MAX_ITEM_PROTOTYPES];
	ZeroMemory(&pids,sizeof(pids));

	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		pids[item->GetProtoId()]+=item->GetCount();
		// Repair weared bag_item
		if(item->IsWeared() && item->IsBroken()) item->Repair();
		// Load weapon holder
		if(item->IsWeapon() && item->WeapGetMaxAmmoCount() && item->WeapIsEmpty()) item->WeapLoadHolder();
	}

	// Item garbager
	if(!IsPerk(MODE_NO_ITEM_GARBAGER))
	{
		// Erase not grouped items
		int need_count=Random(2,4);
		for(int i=0;i<MAX_ITEM_PROTOTYPES;i++)
		{
			if(pids[i]<=need_count) continue;
			ProtoItem* proto_item=ItemMngr.GetProtoItem(i);
			if(!proto_item || proto_item->IsGrouped()) continue;
			ItemMngr.SetItemCritter(this,i,need_count);
			pids[i]=need_count;
			need_count=Random(2,4);
		}
	}

	// Refresh bag
	bool drop_last_weapon=false;

	// Internal bag
	if(!GetParam(ST_BAG_ID))
	{
		 if(!Data.BagSize) return;

		 // Update cur items
		 for(int k=0;k<Data.BagSize;k++)
		 {
			 NpcBagItem& item=Data.Bag[k];
			 DWORD count=item.MinCnt;
			 if(pids[item.ItemPid]>count) continue;
			 if(item.MinCnt!=item.MaxCnt) count=Random(item.MinCnt,item.MaxCnt);
			 ItemMngr.SetItemCritter(this,item.ItemPid,count);
			 drop_last_weapon=true;
		 }
	}
	// External bags
	else
	{
		NpcBag& bag=AIMngr.GetBag(GetParam(ST_BAG_ID));
		if(bag.empty()) return;

		// Check combinations, update or create new
		for(int i=0;i<bag.size();i++)
		{
			NpcBagCombination& comb=bag[i];
			bool create=true;
			if(i<MAX_NPC_BAGS_PACKS && Data.BagCurrentSet[i]<comb.size())
			{
				NpcBagItems& items=comb[Data.BagCurrentSet[i]];
				for(int l=0;l<items.size();l++)
				{
					NpcBagItem& item=items[l];
					if(pids[item.ItemPid]>0)
					{
						// Update cur items
						for(int k=0;k<items.size();k++)
						{
							NpcBagItem& item_=items[k];
							DWORD count=item_.MinCnt;
							if(pids[item_.ItemPid]>count) continue;
							if(item_.MinCnt!=item_.MaxCnt) count=Random(item_.MinCnt,item_.MaxCnt);
							ItemMngr.SetItemCritter(this,item_.ItemPid,count);
							drop_last_weapon=true;
						}

						create=false;
						goto label_EndCycles; // Force end of cycle
					}
				}
			}

label_EndCycles:
			// Create new combination
			if(create && comb.size())
			{
				int rnd=Random(0,comb.size()-1);
				Data.BagCurrentSet[i]=rnd;
				NpcBagItems& items=comb[rnd];
				for(int k=0;k<items.size();k++)
				{
					NpcBagItem& bag_item=items[k];
					DWORD count=bag_item.MinCnt;
					if(bag_item.MinCnt!=bag_item.MaxCnt) count=Random(bag_item.MinCnt,bag_item.MaxCnt);
					//if(pids[bag_item.ItemPid]<count) TODO: need???
					if(ItemMngr.SetItemCritter(this,bag_item.ItemPid,count))
					{
						// Move bag_item
						if(bag_item.ItemSlot!=SLOT_INV)
						{
							if(!GetItemSlot(bag_item.ItemSlot))
							{
								Item* item=GetItemByPid(bag_item.ItemPid);
								if(item && MoveItem(SLOT_INV,bag_item.ItemSlot,item->GetId(),item->GetCount()))
									Data.FavoriteItemPid[bag_item.ItemSlot]=bag_item.ItemPid;
							}
						}
						drop_last_weapon=true;
					}
				}
			}
		}
	}

	if(drop_last_weapon && Data.Params[ST_LAST_WEAPON_ID])
	{
		ChangeParam(ST_LAST_WEAPON_ID);
		Data.Params[ST_LAST_WEAPON_ID]=0;
	}
}

bool Npc::AddPlane(int reason, AIDataPlane* plane, bool is_child, Critter* some_cr, Item* some_item)
{
	if(is_child && aiPlanes.empty())
	{
		plane->Assigned=false;
		plane->Release();
		return false;
	}

	if(plane->Type==AI_PLANE_ATTACK)
	{
		for(AIDataPlaneVecIt it=aiPlanes.begin(),end=aiPlanes.end();it!=end;++it)
		{
			AIDataPlane* p=*it;
			if(p->Type==AI_PLANE_ATTACK && p->Attack.TargId==plane->Attack.TargId)
			{
				p->Assigned=false;
				p->Release();
				aiPlanes.erase(it);
				break;
			}
		}
	}

	DWORD child_index=(is_child?aiPlanes[0]->GetChildsCount()+1:0);
	int result=EventPlaneBegin(plane,reason,some_cr,some_item);
	if(result==PLANE_RUN_GLOBAL && Script::PrepareContext(ServerFunctions.NpcPlaneBegin,CALL_FUNC_STR,GetInfo()))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(plane);
		Script::SetArgDword(reason);
		Script::SetArgObject(some_cr);
		Script::SetArgObject(some_item);
		if(Script::RunPrepared()) result=(Script::GetReturnedBool()?PLANE_KEEP:PLANE_DISCARD);
	}

	if(result==PLANE_DISCARD)
	{
		plane->Release();
		return false;
	}

	// Add
	plane->Assigned=true;
	if(is_child)
	{
		AIDataPlane* parent=aiPlanes[0]->GetCurPlane();
		parent->ChildPlane=plane;
		plane->Priority=parent->Priority;
	}
	else
	{
		if(!plane->Priority)
		{
			aiPlanes.push_back(plane);
		}
		else
		{
			int i=0;
			for(;i<aiPlanes.size();i++)
				if(plane->Priority>aiPlanes[i]->Priority) break;
			aiPlanes.insert(aiPlanes.begin()+i,plane);
			if(i && plane->Priority==aiPlanes[0]->Priority) SetBestCurPlane();
		}
	}
	return true;
}

void Npc::NextPlane(int reason, Critter* some_cr, Item* some_item)
{
	if(aiPlanes.empty()) return;
	SendA_XY();

	AIDataPlane* cur=aiPlanes[0];
	AIDataPlane* last=aiPlanes[0]->GetCurPlane();

	aiPlanes.erase(aiPlanes.begin());
	SetBestCurPlane();

	int result=EventPlaneEnd(last,reason,some_cr,some_item);
	if(result==PLANE_RUN_GLOBAL && Script::PrepareContext(ServerFunctions.NpcPlaneEnd,CALL_FUNC_STR,GetInfo()))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(last);
		Script::SetArgDword(reason);
		Script::SetArgObject(some_cr);
		Script::SetArgObject(some_item);
		if(Script::RunPrepared()) result=(Script::GetReturnedBool()?PLANE_DISCARD:PLANE_KEEP);
	}

	if(result==PLANE_KEEP)
	{
		int place=0;
		if(aiPlanes.size())
		{
			place=1;
			for(int i=aiPlanes.size();place<i;place++) if(cur->Priority>aiPlanes[place]->Priority) break;
		}
		aiPlanes.insert(aiPlanes.begin()+place,cur);
	}
	else
	{
		if(cur!=last) // Child
		{
			cur->DeleteLast();
			aiPlanes.insert(aiPlanes.begin(),cur);
		}
		else // Main
		{
			cur->Assigned=false;
			cur->Release();
		}
	}
}

bool Npc::RunPlane(int reason, DWORD& r0, DWORD& r1, DWORD& r2)
{
	AIDataPlane* cur=aiPlanes[0];
	AIDataPlane* last=aiPlanes[0]->GetCurPlane();
//	aiPlanes.erase(aiPlanes.begin());

	int result=EventPlaneRun(last,reason,r0,r1,r2);;
	if(result==PLANE_RUN_GLOBAL && Script::PrepareContext(ServerFunctions.NpcPlaneRun,CALL_FUNC_STR,GetInfo()))
	{
		Script::SetArgObject(this);
		Script::SetArgObject(last);
		Script::SetArgDword(reason);
		Script::SetArgAddress(&r0);
		Script::SetArgAddress(&r1);
		Script::SetArgAddress(&r2);
		if(Script::RunPrepared()) result=(Script::GetReturnedBool()?PLANE_KEEP:PLANE_DISCARD);
	}

	return result==PLANE_KEEP;

	/*if(result==PLANE_DISCARD)
	{
		if(cur!=last) // Child
		{
			cur->DeleteLast();
			aiPlanes.insert(aiPlanes.begin(),cur);
		}
		else // Main
		{
			cur->Assigned=false;
			cur->Release();
		}
		return false;
	}

	aiPlanes.insert(aiPlanes.begin(),cur);
	return true;*/
}

bool Npc::IsPlaneAviable(int plane_type)
{
	for(int i=0,j=aiPlanes.size();i<j;i++)
		if(aiPlanes[i]->IsSelfOrHas(plane_type)) return true;
	return false;
}

void Npc::DropPlanes()
{
	for(int i=0,j=aiPlanes.size();i<j;i++)
	{
		aiPlanes[i]->Assigned=false;
		aiPlanes[i]->Release();
	}
	aiPlanes.clear();
}

void Npc::SetBestCurPlane()
{
	if(aiPlanes.size()<2) return;

	DWORD type=aiPlanes[0]->Type;
	DWORD priority=aiPlanes[0]->Priority;

	if(type!=AI_PLANE_ATTACK) return;

	int sort_cnt=1;
	for(int i=1,j=aiPlanes.size();i<j;i++)
	{
		if(aiPlanes[i]->Priority==priority) sort_cnt++;
		else break;
	}

	if(sort_cnt<2) return;

	// Find critter with smallest dist
	int hx=GetHexX();
	int hy=GetHexY();
	int best_plane=-1;
	int best_dist=-1;
	for(int i=0;i<sort_cnt;i++)
	{
		Critter* cr=GetCritSelf(aiPlanes[i]->Attack.TargId);
		if(!cr) continue;

		int dist=DistGame(hx,hy,cr->GetHexX(),cr->GetHexY());
		if(best_dist>=0 && dist>=best_dist) continue;
		else if(dist==best_dist)
		{
			Critter* cr2=GetCritSelf(aiPlanes[best_plane]->Attack.TargId);
			if(!cr || cr->GetParam(ST_CURRENT_HP)>=cr2->GetParam(ST_CURRENT_HP)) continue;
		}

		best_dist=dist;
		best_plane=i;
	}

	if(best_plane>0) std::swap(aiPlanes[0],aiPlanes[best_plane]);
}

DWORD Npc::GetTalkedPlayers()
{
	DWORD talk=0;
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		if(!(*it)->IsPlayer()) continue;
		Client* cl=(Client*)(*it);
		if(cl->Talk.TalkType==TALK_WITH_NPC && cl->Talk.TalkNpc==GetId()) talk++;
	}
	return talk;
}

bool Npc::IsTalkedPlayers()
{
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		if(!(*it)->IsPlayer()) continue;
		Client* cl=(Client*)(*it);
		if(cl->Talk.TalkType==TALK_WITH_NPC && cl->Talk.TalkNpc==GetId()) return true;
	}
	return false;
}

DWORD Npc::GetBarterPlayers()
{
	DWORD barter=0;
	for(CrVecIt it=VisCr.begin(),end=VisCr.end();it!=end;++it)
	{
		if(!(*it)->IsPlayer()) continue;
		Client* cl=(Client*)(*it);
		if(cl->Talk.TalkType==TALK_WITH_NPC && cl->Talk.TalkNpc==GetId() && cl->Talk.Barter) barter++;
	}
	return barter;
}

bool Npc::IsFreeToTalk()
{
	DWORD max_talkers=1;
	DialogPack* dlg=DlgMngr.GetDialogPack(GetParam(ST_DIALOG_ID));
	if(dlg) max_talkers=dlg->MaxTalk;
	return GetTalkedPlayers()<max_talkers;
}

bool Npc::IsPlaneNoTalk()
{
	AIDataPlane* p=GetCurPlane();
	if(!p) return false;

	switch(p->Type)
	{
	case AI_PLANE_WALK:
	case AI_PLANE_ATTACK:
	case AI_PLANE_PICK:
		return true;
	default:
		break;
	}

	return false;
}

bool Npc::CheckBattleWeapon(Item* weap)
{
	if(!weap->IsWeapon()) return false;
	if(!CritType::IsAnim1(GetCrType(),weap->Proto->Weapon.Anim1)) return false;
	if(weap->IsBroken()) return false;
	if(weap->IsTwoHands() && IsDmgArm()) return false;
	if(!IsPerk(MODE_UNLIMITED_AMMO) && weap->WeapGetMaxAmmoCount() && weap->WeapIsEmpty() && !GetAmmoForWeapon(weap)) return false;
	return true;
}

bool BestSkillGreather(const IntPair& l, const IntPair& r){return l.second>r.second;}
Item* Npc::GetBattleWeapon(int& use, Critter* targ)
{
	// Unarmed protos
	ProtoItem* punch=ItemMngr.GetProtoItem(UNARMED_PUNCH);
	ProtoItem* kick=ItemMngr.GetProtoItem(UNARMED_KICK);
	ProtoItem* unarmed=(Random(0,4)?punch:kick);
	if(defItemSlotMain.Proto!=unarmed) defItemSlotMain.Init(unarmed);
	use=USE_PRIMARY;

	// Damaged two arms
	if(IsDmgTwoArm()) return &defItemSlotMain;

	// Get last battle weapon
	use=lastBattleWeaponUse;
	Item* weap=(lastBattleWeaponId==MAX_DWORD?&defItemSlotMain:GetItem(lastBattleWeaponId,false));
	if(weap && CheckBattleWeapon(weap)) return weap;

	// Get new weapon
	lastBattleWeaponId=0;
	lastBattleWeaponUse=0;

	// Choose all valid weapons
typedef pair<Item*,int> ItemIntPair;
typedef vector<ItemIntPair> ItemIntPairVec;
typedef vector<ItemIntPair>::value_type ItemIntPairVecVal;

	ItemIntPairVec skill_weap[6];
	for(ItemPtrVecIt it=invItems.begin(),end=invItems.end();it!=end;++it)
	{
		Item* item=*it;
		if(!CheckBattleWeapon(item)) continue;

		for(int i=0;i<MAX_USES;i++)
		{
			if(!item->WeapIsUseAviable(i)) continue;
			int sk=item->Proto->Weapon.Skill[i];
			if(sk<SKILL_BEGIN) sk+=SKILL_BEGIN;
			if(sk>=SK_SMALL_GUNS && sk<=SK_THROWING) skill_weap[sk-SKILL_BEGIN].push_back(ItemIntPairVecVal(item,i));
		}
	}

	// Sort skills
	IntPairVec best_sk(6);
	best_sk.push_back(IntPairVecVal(SK_SMALL_GUNS,GetSkill(SK_SMALL_GUNS)));
	best_sk.push_back(IntPairVecVal(SK_BIG_GUNS,GetSkill(SK_BIG_GUNS)));
	best_sk.push_back(IntPairVecVal(SK_ENERGY_WEAPONS,GetSkill(SK_ENERGY_WEAPONS)));
	best_sk.push_back(IntPairVecVal(SK_UNARMED,GetSkill(SK_UNARMED)));
	best_sk.push_back(IntPairVecVal(SK_MELEE_WEAPONS,GetSkill(SK_MELEE_WEAPONS)));
	best_sk.push_back(IntPairVecVal(SK_THROWING,GetSkill(SK_THROWING)));
	// Sort by greather
	std::sort(best_sk.begin(),best_sk.end(),BestSkillGreather);

	// Choose one weapon
	for(int i=0;i<6;i++)
	{
		// Find by skill, Weapon+Use
		ItemIntPairVec& wu=skill_weap[best_sk[i].first-SKILL_BEGIN];
		if(wu.empty()) continue;

		// Sort weapons
		if(wu.size()>1)
		{
			//TODO:
			std::random_shuffle(wu.begin(),wu.end());
		}

		// Set
		ItemIntPair& f=wu[0];
		lastBattleWeaponId=f.first->GetId();
		lastBattleWeaponUse=f.second;
		use=f.second;
		return f.first;
	}

	// Not found, set hands
	use=USE_PRIMARY;
	lastBattleWeaponId=MAX_DWORD;
	lastBattleWeaponUse=use;
	return &defItemSlotMain;
}

void Npc::MoveToHex(int reason, WORD hx, WORD hy, BYTE ori, bool is_run, BYTE cut)
{
	AIDataPlane* plane=new AIDataPlane(AI_PLANE_WALK,AI_PLANE_WALK_PRIORITY);
	if(!plane) return;
	plane->Walk.HexX=hx;
	plane->Walk.HexY=hy;
	plane->Walk.Dir=ori;
	plane->Walk.IsRun=is_run;
	plane->Walk.Cut=cut;
	AddPlane(reason,plane,false);
}

void Npc::SetTarget(int reason, Critter* target, int min_hp, bool is_gag)
{
	for(int i=0,j=aiPlanes.size();i<j;i++)
	{
		AIDataPlane* pp=aiPlanes[i];
		if(pp->Type==AI_PLANE_ATTACK && pp->Attack.TargId==target->GetId())
		{
			if(i && pp->Priority==aiPlanes[0]->Priority) SetBestCurPlane();
			return;
		}
	}

	AIDataPlane* plane=new AIDataPlane(AI_PLANE_ATTACK,AI_PLANE_ATTACK_PRIORITY);
	if(!plane) return;
	plane->Attack.TargId=target->GetId();
	plane->Attack.MinHp=min_hp;
	plane->Attack.LastHexX=(target->GetMap()?target->GetHexX():0);
	plane->Attack.LastHexY=(target->GetMap()?target->GetHexY():0);
	plane->Attack.IsGag=is_gag;
	plane->Attack.GagHexX=target->GetHexX();
	plane->Attack.GagHexY=target->GetHexY();
	plane->Attack.IsRun=(IsTurnBased()?GameOpt.TbAlwaysRun:GameOpt.RtAlwaysRun);
	AddPlane(reason,plane,false,target,NULL);
}

