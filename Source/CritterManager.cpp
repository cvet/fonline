#include "StdAfx.h"
#include "CritterManager.h"
#include "Log.h"
#include "Names.h"
#include "ItemManager.h"

#ifdef FONLINE_SERVER
#include "MapManager.h"
#endif

CritterManager CrMngr;

#define CRPROTO_APP   "Critter proto"

//***************************************************************************************
//   CritterManager
//***************************************************************************************

bool CritterManager::Init()
{
	WriteLog("Critter manager initialization...\n");

	if(isActive)
	{
		WriteLog("already initialized.\n");
		return true;
	}

	if(!ItemMngr.IsInit())
	{
		WriteLog("Error, Items manager not init.\n");
		return false;
	}

	ZeroMemory(allProtos,sizeof(allProtos));
	Clear();

	isActive=true;
	WriteLog("Critter manager initialization complete.\n");
	return true;
}

void CritterManager::Finish()
{
	WriteLog("Critter manager finish...\n");

#ifdef FONLINE_SERVER
	CritterGarbager(0);
#endif

	ZeroMemory(allProtos,sizeof(allProtos));
	Clear();

	isActive=false;
	WriteLog("Critter manager finish complete.\n");
}

void CritterManager::Clear()
{
#ifdef FONLINE_SERVER
	for(CrMapIt it=allCritters.begin(),end=allCritters.end();it!=end;++it)
		SAFEREL((*it).second);
	allCritters.clear();
	crToDelete.clear();
	playersCount=0;
	npcCount=0;
	lastNpcId=NPC_START_ID;
#endif
}

bool CritterManager::LoadProtos()
{
	WriteLog("Loading critters prototypes...\n");

	if(!IsInit())
	{
		WriteLog("Critters manager is not init.\n");
		return false;
	}

	// Get names of proto
	if(!fileMngr.LoadFile("critters.lst",PT_SERVER_PRO_CRITTERS))
	{
		WriteLog("Cannot open \"critters.lst\".\n");
		return false;
	}

	char buf[256];
	StrVec fnames;
	while(fileMngr.GetLine(buf,sizeof(buf))) fnames.push_back(string(buf));
	fileMngr.UnloadFile();

	/*FILE* f=fopen("crproto.txt","wb");
	for(int i=0,k=fnames.size();i<k;i++)
	{
		if(!fileMngr.LoadFile(fnames[i].c_str(),PT_PRO_CRIT)) continue;
		LoadFromFileF2(&fileMngr,f);
	}
	fclose(f);
	return false;*/

	// Load protos
	IniParser protos_txt;
	int loaded_count=0;
	int errors=0;
	for(int i=0,k=fnames.size();i<k;i++)
	{
		const char* fname=fnames[i].c_str();
		if(protos_txt.LoadFile(fname,PT_SERVER_PRO_CRITTERS))
		{
			while(protos_txt.GotoNextApp(CRPROTO_APP))
			{
				int pid=-1;
				int params[MAX_PARAMS];
				ZeroMemory(params,sizeof(params));

				StrVec lines;
				protos_txt.GetAppLines(lines);

				for(int j=0;j<lines.size();j++)
				{
					char line[256];
					char indent[256];
					int value;
					StringCopy(line,lines[j].c_str());
					Str::Replacement(line,'=',' ');
					if(sscanf(line,"%s%d",indent,&value)==2)
					{
						if(!strcmp(indent,"Pid")) pid=value;
						else
						{
							int param_id=FONames::GetParamId(indent);
							if(param_id>=0 && param_id<MAX_PARAMS) params[param_id]=value;
							else
							{
								WriteLog("Unknown parameter<%s> in<%s>.\n",indent,fname);
								errors++;
							}
						}
					}
				}

				if(pid>0 && pid<MAX_CRIT_PROTOS)
				{
					CritData& proto=allProtos[pid];
					if(!proto.ProtoId) loaded_count++;
					//else WriteLog("Prototype is already parsed, pid<%u>. Rewrite.\n",pid);
					proto.ProtoId=pid;
					memcpy(proto.Params,params,sizeof(params));
				}
				else
				{
					WriteLog("Invalid pid<%d> in<%s>.\n",pid,fname);
					errors++;
				}
			}
		}
	}

	WriteLog("Loaded<%d> critter protos, errors<%d>.\n",loaded_count,errors);
	return errors==0;
}

CritData* CritterManager::GetProto(WORD proto_id)
{
	if(!proto_id || proto_id>=MAX_CRIT_PROTOS || !allProtos[proto_id].ProtoId) return NULL;
	return &allProtos[proto_id];
}

#ifdef FONLINE_SERVER
void CritterManager::SaveCrittersFile(void(*save_func)(void*,size_t))
{
	CrVec crits;
	for(CrMapIt it=allCritters.begin(),end=allCritters.end();it!=end;++it)
	{
		Critter* cr=(*it).second;
		if(cr->IsNpc()) crits.push_back(cr);
	}

	DWORD count=crits.size(); // npcCount
	save_func(&count,sizeof(count));
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		cr->Data.IsDataExt=(cr->DataExt?true:false);
		save_func(&cr->Data,sizeof(cr->Data));
		if(cr->Data.IsDataExt) save_func(cr->DataExt,sizeof(CritDataExt));
		DWORD te_count=cr->CrTimeEvents.size();
		save_func(&te_count,sizeof(te_count));
		if(te_count) save_func(&cr->CrTimeEvents[0],te_count*sizeof(Critter::CrTimeEvent));
	}
}

bool CritterManager::LoadCrittersFile(FILE* f)
{
	WriteLog("Load npc...");

	lastNpcId=0;

	DWORD count;
	fread(&count,sizeof(count),1,f);
	if(!count)
	{
		WriteLog("critters not found.\n");
		return true;
	}

	for(DWORD i=0;i<count;i++)
	{
		CritData data;
		fread(&data,sizeof(data),1,f);

		CritDataExt data_ext;
		if(data.IsDataExt) fread(&data_ext,sizeof(data_ext),1,f);

		Critter::CrTimeEventVec tevents;
		DWORD te_count;
		fread(&te_count,sizeof(te_count),1,f);
		if(te_count)
		{
			tevents.resize(te_count);
			fread(&tevents[0],te_count*sizeof(Critter::CrTimeEvent),1,f);
		}

		Npc* npc=CreateNpc(data.ProtoId,false);
		if(!npc)
		{
			WriteLog("create npc with id<%u>, pid<%u> failture.\n",data.Id,data.ProtoId);
			return false;
		}
		if(data.Id>lastNpcId) lastNpcId=data.Id;
		npc->Data=data;
		if(data.IsDataExt)
		{
			CritDataExt* pdata_ext=npc->GetDataExt();
			if(pdata_ext) *pdata_ext=data_ext;
		}
		if(te_count) npc->CrTimeEvents=tevents;

		npc->NextRefreshBagTick=Timer::GameTick()+(npc->Data.BagRefreshTime?npc->Data.BagRefreshTime:GameOpt.BagRefreshTime)*60*1000;
		npc->NameStr="Npc_";
		npc->NameStr+=Str::Format("%u",npc->GetId());
		AddCritter(npc);
	}

	WriteLog("complete, count<%u>.\n",count);
	return true;	
}

void CritterManager::RunInitScriptCritters()
{
	for(CrMapIt it=allCritters.begin(),end=allCritters.end();it!=end;++it)
	{
		Critter* cr=(*it).second;
		if(Script::PrepareContext(ServerFunctions.CritterInit,CALL_FUNC_STR,cr->GetInfo()))
		{
			Script::SetArgObject(cr);
			Script::SetArgBool(false);
			Script::RunPrepared();
		}
		if(cr->Data.ScriptId) cr->ParseScript(NULL,false);
	}
}

void CritterManager::CritterToGarbage(Critter* cr)
{
	crToDelete.push_back(cr->GetId());
	cr->IsNotValid=true;
}

void CritterManager::CritterGarbager(DWORD cycle_tick)
{
	if(!crToDelete.empty())
	{
		// Check performance
		if(cycle_tick && Timer::FastTick()-cycle_tick>100) return;

		DwordVec to_del=crToDelete;
		crToDelete.clear();
		for(DwordVecIt it=to_del.begin(),end=to_del.end();it!=end;++it)
		{
			// Check performance
			if(cycle_tick && Timer::FastTick()-cycle_tick>100)
			{
				for(;it!=end;++it) crToDelete.push_back(*it);
				return;
			}

			DWORD crid=*it;
			Critter* npc=GetCritter(crid);
			if(!npc) continue;

			if(!npc->IsNpc())
			{
				WriteLog(__FUNCTION__" - Critter<%s> is not npc.\n",npc->GetInfo());
				continue;
			}

			npc->EventFinish(true);
			if(Script::PrepareContext(ServerFunctions.CritterFinish,CALL_FUNC_STR,npc->GetInfo()))
			{
				Script::SetArgObject(npc);
				Script::SetArgBool(true);
				Script::RunPrepared();
			}

			Map* map=MapMngr.GetMap(npc->GetMap());
			if(map)
			{
				npc->ClearVisible();
				map->EraseCritter(npc);
				map->UnsetFlagCritter(npc->GetHexX(),npc->GetHexY(),npc->GetMultihex(),npc->IsDead());
				if(map->MapLocation->IsToGarbage()) MapMngr.RunLocGarbager();
			}
			else if(npc->GroupMove)
			{
				GlobalMapGroup* group=npc->GroupMove;
				group->EraseCrit(npc);
				if(npc==group->Rule)
				{
					for(CrVecIt it_=group->CritMove.begin(),end_=group->CritMove.end();it_!=end_;++it_)
					{
						Critter* cr=*it_;
						MapMngr.GM_GroupStartMove(cr,true);
					}
				}
				else
				{
					for(CrVecIt it_=group->CritMove.begin(),end_=group->CritMove.end();it_!=end_;++it_)
					{
						Critter* cr=*it_;
						cr->Send_RemoveCritter(npc);
					}

					Item* car=npc->GetItemCar();
					if(car && car->GetId()==group->CarId)
					{
						group->CarId=0;
						MapMngr.GM_GroupSetMove(group,group->MoveX,group->MoveY,0); // Stop others
					}
				}

				npc->GroupMove=NULL;
			}

			npc->FullClear();
			DeleteCritter(npc);
		}
	}
}

Npc* CritterManager::CreateNpc(WORD proto_id, DWORD params_count, int* params, DWORD items_count, int* items, const char* script, Map* map, WORD hx, WORD hy, BYTE dir, bool accuracy)
{
	if(!map || hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY())
	{
		WriteLog(__FUNCTION__" - Wrong map values, hx<%u>, hy<%u>, map is nullptr<%s>.\n",hx,hy,!map?"true":"false");
		return NULL;
	}

	if(!map->IsHexPassed(hx,hy))
	{
		if(accuracy)
		{
			WriteLog(__FUNCTION__" - Accuracy position busy, map pid<%u>, hx<%u>, hy<%u>.\n",map->GetPid(),hx,hy);
			return NULL;
		}

		static int cur_step=0;
		short hx_=hx;
		short hy_=hy;
		bool odd=(hx&1)!=0;
		short* sx=(odd?SXOdd:SXEven);
		short* sy=(odd?SYOdd:SYEven);

		// Find in 2 hex radius
		for(int i=0;;i++)
		{
			if(i>=18)
			{
				WriteLog(__FUNCTION__" - All positions busy, map pid<%u>, hx<%u>, hy<%u>.\n",map->GetPid(),hx,hy);
				return NULL;
			}
			cur_step++;
			if(cur_step>=18) cur_step=0;
			if(hx_+sx[cur_step]<0 || hx_+sx[cur_step]>=map->GetMaxHexX()) continue;
			if(hy_+sy[cur_step]<0 || hy_+sy[cur_step]>=map->GetMaxHexY()) continue;
			if(!map->IsHexPassed(hx_+sx[cur_step],hy_+sy[cur_step])) continue;
			break;
		}

		hx_+=sx[cur_step];
		hy_+=sy[cur_step];
		hx=hx_;
		hy=hy_;
	}

	Npc* npc=CrMngr.CreateNpc(proto_id,true);
	if(!npc)
	{
		WriteLog(__FUNCTION__" - Create npc with pid <%u> failture.\n",proto_id);
		return NULL;
	}
	npc->Data.Id=lastNpcId+1;
	lastNpcId++;

	// Flags and coords
	Location* loc=map->MapLocation;

	if(dir>5) dir=Random(0,5);
	npc->Data.Dir=dir;
	npc->Data.WorldX=(loc?loc->Data.WX:100);
	npc->Data.WorldY=(loc?loc->Data.WY:100);
	npc->SetHome(map->GetId(),hx,hy,dir);
	npc->SetMaps(map->GetId(),map->GetPid());
	npc->Data.HexX=hx;
	npc->Data.HexY=hy;
	npc->NameStr="Npc_";
	npc->NameStr+=Str::Format("%u",npc->GetId());

	for(DWORD i=0;i<params_count;i++)
	{
		WORD index=params[i*2];
		int value=params[i*2+1];
		if(index>=0 && index<MAX_PARAMS) npc->Data.Params[index]=value;
	}

	map->AddCritter(npc);
	CrMngr.AddCritter(npc);

	for(DWORD i=0;i<items_count;i++)
	{
		int pid=items[i*3];
		int count=items[i*3+1];
		int slot=items[i*3+2];
		if(pid>0 && count>0)
		{
			Item* item=ItemMngr.AddItemCritter(npc,pid,count);
			if(item && slot!=SLOT_INV) npc->MoveItem(SLOT_INV,slot,item->GetId(),item->GetCount());
		}
	}

	if(Script::PrepareContext(ServerFunctions.CritterInit,CALL_FUNC_STR,npc->GetInfo()))
	{
		Script::SetArgObject(npc);
		Script::SetArgBool(true);
		Script::RunPrepared();
	}
	if(script) npc->ParseScript(script,true);
	map->AddCritterEvents(npc);

	npc->RefreshBag();
	npc->ProcessVisibleCritters();
	npc->ProcessVisibleItems();
	return npc;
}

Npc* CritterManager::CreateNpc(WORD proto_id, bool copy_data)
{
	if(!IsInit())
	{
		WriteLog(__FUNCTION__" - Critter Manager is not initialized.\n");
		return false;
	}

	CritData* data=GetProto(proto_id);
	if(!data)
	{
		WriteLog(__FUNCTION__" - Critter data not found, proto id<%u>.\n",proto_id);
		return NULL;
	}

	Npc* npc=new Npc();
	if(!npc)
	{
		WriteLog(__FUNCTION__" - Allocation fail, proto id<%u>.\n",proto_id);
		return NULL;
	}

	if(!npc->SetDefaultItems(ItemMngr.GetProtoItem(ITEM_DEF_SLOT),
		ItemMngr.GetProtoItem(ITEM_DEF_SLOT),
		ItemMngr.GetProtoItem(ITEM_DEF_ARMOR)))
	{
		delete npc;
		WriteLog(__FUNCTION__" - Can't set default items of proto id<%u>.\n",proto_id);
		return NULL;
	}

	if(copy_data) npc->Data=*data;
	npc->Data.EnemyStackCount=MAX_ENEMY_STACK;
	npc->Data.ProtoId=proto_id;
	npc->Data.Cond=COND_LIFE;
	npc->Data.CondExt=COND_LIFE_NONE;
	npc->Data.Multihex=-1;
	return npc;
}

void CritterManager::AddCritter(Critter* cr)
{
	allCritters.insert(CrMapVal(cr->GetId(),cr));
	if(cr->IsPlayer()) playersCount++;
	else npcCount++;
}

void CritterManager::GetCopyNpcs(PcVec& npcs)
{
	npcs.reserve(npcCount);
	for(CrMapIt it=allCritters.begin(),end=allCritters.end();it!=end;++it)
	{
		Critter* cr=(*it).second;
		if(cr->IsNpc()) npcs.push_back((Npc*)cr);
	}
}

void CritterManager::GetCopyPlayers(ClVec& players)
{
	players.reserve(playersCount);
	for(CrMapIt it=allCritters.begin(),end=allCritters.end();it!=end;++it)
	{
		Critter* cr=(*it).second;
		if(cr->IsPlayer()) players.push_back((Client*)cr);
	}
}

Critter* CritterManager::GetCritter(DWORD crid)
{
	CrMapIt it=allCritters.find(crid);
	return it!=allCritters.end()?(*it).second:NULL;
}

Client* CritterManager::GetPlayer(DWORD crid)
{
	Critter* cr=GetCritter(crid);
	return cr && cr->IsPlayer()?(Client*)cr:NULL;
}

Client* CritterManager::GetPlayer(const char* name)
{
	for(CrMapIt it=allCritters.begin(),end=allCritters.end();it!=end;++it)
	{
		Critter* cr=(*it).second;
		if(cr->IsPlayer() && !_stricmp(name,cr->GetName())) return (Client*)cr;
	}
	return NULL;
}

Npc* CritterManager::GetNpc(DWORD crid)
{
	Critter* cr=GetCritter(crid);
	return cr && cr->IsNpc()?(Npc*)cr:NULL;
}

void CritterManager::EraseCritter(Critter* cr)
{
	CrMapIt it=allCritters.find(cr->GetId());
	if(it!=allCritters.end())
	{
		if(cr->IsPlayer()) playersCount--;
		else npcCount--;
		allCritters.erase(it);
	}
}

void CritterManager::DeleteCritter(Critter* cr)
{
	cr->IsNotValid=true;
	CrMapIt it=allCritters.find(cr->GetId());
	if(it==allCritters.end()) return;
	Critter* cr_=(*it).second;
	if(cr_->IsPlayer()) playersCount--;
	else npcCount--;
	cr_->IsNotValid=true;
	cr_->Release();
	allCritters.erase(it);
}

DWORD CritterManager::PlayersInGame()
{
	return playersCount;
}

DWORD CritterManager::NpcInGame()
{
	return npcCount;
}

DWORD CritterManager::CrittersInGame()
{
	return allCritters.size();
}

void CritterManager::ProcessCrittersVisible()
{
	for(CrMapIt it=allCritters.begin(),end=allCritters.end();it!=end;++it) (*it).second->ProcessVisibleCritters();
}

void CritterManager::ProcessItemsVisible()
{
	for(CrMapIt it=allCritters.begin(),end=allCritters.end();it!=end;++it) (*it).second->ProcessVisibleItems();
}
#endif
