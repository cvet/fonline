#include "StdAfx.h"
#include "Server.h"

void FOServer::ProcessCritter(Critter* cr)
{
	if(cr->IsNotValid) return;
	if(Timer::IsGamePaused()) return;

	uint tick=Timer::GameTick();

	// Idle global function
	if(tick>=cr->GlobalIdleNextTick)
	{
		if(Script::PrepareContext(ServerFunctions.CritterIdle,_FUNC_,cr->GetInfo()))
		{
			Script::SetArgObject(cr);
			Script::RunPrepared();
		}
		cr->GlobalIdleNextTick=tick+GameOpt.CritterIdleTick;
	}

	// Ap regeneration
	int max_ap=cr->GetParam(ST_ACTION_POINTS)*AP_DIVIDER;
	if(cr->IsFree() && cr->GetRealAp()<max_ap && !cr->IsTurnBased())
	{
		if(!cr->ApRegenerationTick) cr->ApRegenerationTick=tick;
		else
		{
			uint delta=tick-cr->ApRegenerationTick;
			if(delta>=500)
			{
				cr->Data.Params[ST_CURRENT_AP]+=max_ap*delta/GameOpt.ApRegeneration;
				if(cr->Data.Params[ST_CURRENT_AP]>max_ap) cr->Data.Params[ST_CURRENT_AP]=max_ap;
				cr->ApRegenerationTick=tick;
				//if(cr->IsPlayer()) WriteLog("ap<%u.%u>\n",cr->Data.St[ST_CURRENT_AP]/AP_DIVIDER,cr->Data.St[ST_CURRENT_AP]%AP_DIVIDER);
			}
		}
	}
	if(cr->Data.Params[ST_CURRENT_AP]>max_ap) cr->Data.Params[ST_CURRENT_AP]=max_ap;

	// Internal misc/drugs time events
	// One event per cycle
	if(!cr->CrTimeEvents.empty())
	{
		uint next_time=cr->CrTimeEvents[0].NextTime;
		if(!next_time || (!cr->IsTurnBased() && GameOpt.FullSecond>=next_time))
		{
			Critter::CrTimeEvent me=cr->CrTimeEvents[0];
			cr->EraseCrTimeEvent(0);
			uint time=GameOpt.TimeMultiplier*1800; // 30 minutes on error
			if(Script::PrepareContext(Script::GetScriptFuncBindId(me.FuncNum),_FUNC_,cr->GetInfo()))
			{
				Script::SetArgObject(cr);
				Script::SetArgUInt(me.Identifier);
				Script::SetArgAddress(&me.Rate);
				if(Script::RunPrepared()) time=Script::GetReturnedUInt();
			}
			if(time) cr->AddCrTimeEvent(me.FuncNum,me.Rate,time,me.Identifier);
		}
	}

	// Global map
	if(!cr->GetMap() && cr->GroupMove && cr==cr->GroupMove->Rule) MapMngr.GM_GroupMove(cr->GroupMove);

	// Client
	if(cr->IsPlayer())
	{
		// Cast
		Client* cl=(Client*)cr;

		// Talk
		cl->ProcessTalk(false);

		// Ping client
		if(cl->IsToPing())
		{
#ifndef DEV_VESRION
			if(GameOpt.AccountPlayTime && Random(0,3)==0) cl->Send_CheckUIDS();
#endif
			cl->PingClient();
			if(cl->GetMap()) MapMngr.TryTransitCrGrid(cr,MapMngr.GetMap(cr->GetMap()),cr->GetHexX(),cr->GetHexY(),false);
		}

		// Idle
		if(cl->FuncId[CRITTER_EVENT_IDLE]>0 && cl->IsLife() && !cl->IsWait() && cl->IsFree())
		{
			cl->EventIdle();
			if(!cl->IsWait()) cl->SetWait(GameOpt.CritterIdleTick);
		}

		// Kick from game
		if(cl->IsOffline() && cl->IsLife() && !cl->GetParam(TO_BATTLE) &&
		   !cl->GetParam(TO_REMOVE_FROM_GAME) && cl->GetOfflineTime()>=GameOpt.MinimumOfflineTime &&
		   !MapMngr.IsProtoMapNoLogOut(cl->GetProtoMap()))
		{
			cl->RemoveFromGame();
		}

		// Cache intelligence for GetSayIntellect, every 3 seconds
		if(tick>=cl->CacheValuesNextTick)
		{
			cl->IntellectCacheValue=(tick&0xFFF0)|cl->GetParam(ST_INTELLECT);
			cl->LookCacheValue=cl->GetLook();
			cl->CacheValuesNextTick=tick+3000;
		}
	}
	// Npc
	else
	{
		// Cast
		Npc* npc=(Npc*)cr;

		// Process
		if(npc->IsLife())
		{
			ProcessAI(npc);
			if(npc->IsNeedRefreshBag()) npc->RefreshBag();
		}
	}

	// Knockout
	if(cr->IsKnockout()) cr->TryUpOnKnockout();

	// Process changed parameters
	cr->ProcessChangedParams();
}

void FOServer::SaveHoloInfoFile()
{
	uint count=HolodiskInfo.size();
	AddWorldSaveData(&count,sizeof(count));
	for(HoloInfoMapIt it=HolodiskInfo.begin(),end=HolodiskInfo.end();it!=end;++it)
	{
		uint id=(*it).first;
		HoloInfo* hi=(*it).second;
		AddWorldSaveData(&id,sizeof(id));
		AddWorldSaveData(&hi->CanRewrite,sizeof(hi->CanRewrite));
		ushort title_len=hi->Title.length();
		AddWorldSaveData(&title_len,sizeof(title_len));
		if(title_len) AddWorldSaveData((void*)hi->Title.c_str(),title_len);
		ushort text_len=hi->Text.length();
		AddWorldSaveData(&text_len,sizeof(text_len));
		if(text_len) AddWorldSaveData((void*)hi->Text.c_str(),text_len);
	}
}

bool FOServer::LoadHoloInfoFile(FILE* f)
{
	LastHoloId=USER_HOLO_START_NUM;

	uint count=0;
	if(!fread(&count,sizeof(count),1,f)) return false;
	for(uint i=0;i<count;i++)
	{
		uint id;
		if(!fread(&id,sizeof(id),1,f)) return false;
		bool can_rw;
		if(!fread(&can_rw,sizeof(can_rw),1,f)) return false;

		ushort title_len;
		char title[USER_HOLO_MAX_TITLE_LEN+1]={0};
		if(!fread(&title_len,sizeof(title_len),1,f)) return false;
		if(title_len>=USER_HOLO_MAX_TITLE_LEN) title_len=USER_HOLO_MAX_TITLE_LEN;
		if(title_len) fread(title,title_len,1,f);
		title[title_len]=0;

		ushort text_len;
		char text[USER_HOLO_MAX_LEN+1]={0};
		if(!fread(&text_len,sizeof(text_len),1,f)) return false;
		if(text_len>=USER_HOLO_MAX_LEN) text_len=USER_HOLO_MAX_LEN;
		if(text_len) fread(text,text_len,1,f);
		text[text_len]=0;

		HolodiskInfo.insert(HoloInfoMapVal(id,new HoloInfo(can_rw,title,text)));
		if(id>LastHoloId) LastHoloId=id;
	}
	return true;
}

void FOServer::AddPlayerHoloInfo(Critter* cr, uint holo_num, bool send)
{
	if(!holo_num)
	{
		if(send) cr->Send_TextMsg(cr,STR_HOLO_READ_FAIL,SAY_NETMSG,TEXTMSG_HOLO);
		return;
	}

	if(cr->Data.HoloInfoCount>=MAX_HOLO_INFO)
	{
		if(send) cr->Send_TextMsg(cr,STR_HOLO_READ_MEMORY_FULL,SAY_NETMSG,TEXTMSG_HOLO);
		return;
	}

	for(int i=0,j=cr->Data.HoloInfoCount;i<j;i++)
	{
		if(cr->Data.HoloInfo[i]==holo_num)
		{
			if(send) cr->Send_TextMsg(cr,STR_HOLO_READ_ALREADY,SAY_NETMSG,TEXTMSG_HOLO);
			return;
		}
	}

	cr->Data.HoloInfo[cr->Data.HoloInfoCount]=holo_num;
	cr->Data.HoloInfoCount++;

	if(send)
	{
		if(holo_num>=USER_HOLO_START_NUM) Send_PlayerHoloInfo(cr,holo_num,true);
		cr->Send_HoloInfo(false,cr->Data.HoloInfoCount-1,1);
		cr->Send_TextMsg(cr,STR_HOLO_READ_SUCC,SAY_NETMSG,TEXTMSG_HOLO);
	}

	// Cancel rewrite
	if(holo_num>=USER_HOLO_START_NUM)
	{
		SCOPE_LOCK(HolodiskLocker);

		HoloInfo* hi=GetHoloInfo(holo_num);
		if(hi && hi->CanRewrite) hi->CanRewrite=false;
	}
}

void FOServer::ErasePlayerHoloInfo(Critter* cr, uint index, bool send)
{
	if(index>=MAX_HOLO_INFO || index>=cr->Data.HoloInfoCount)
	{
		if(send) cr->Send_TextMsg(cr,STR_HOLO_ERASE_FAIL,SAY_NETMSG,TEXTMSG_HOLO);
		return;
	}

	cr->Data.HoloInfo[index]=0;
	for(int i=index,j=cr->Data.HoloInfoCount;i<j && i<MAX_HOLO_INFO-1;i++) cr->Data.HoloInfo[i]=cr->Data.HoloInfo[i+1];
	cr->Data.HoloInfoCount--;

	if(send)
	{
		cr->Send_HoloInfo(true,0,cr->Data.HoloInfoCount);
		cr->Send_TextMsg(cr,STR_HOLO_ERASE_SUCC,SAY_NETMSG,TEXTMSG_HOLO);
	}
}

void FOServer::Send_PlayerHoloInfo(Critter* cr, uint holo_num, bool send_text)
{
	if(!cr->IsPlayer()) return;

	HolodiskLocker.Lock();

	Client* cl=(Client*)cr;
	HoloInfo* hi=GetHoloInfo(holo_num);
	if(hi)
	{
		string str=(send_text?hi->Text:hi->Title); // Copy

		HolodiskLocker.Unlock();

		cl->Send_UserHoloStr(send_text?STR_HOLO_INFO_DESC_(holo_num):STR_HOLO_INFO_NAME_(holo_num),str.c_str(),str.length());
	}
	else
	{
		HolodiskLocker.Unlock();

		cl->Send_UserHoloStr(send_text?STR_HOLO_INFO_DESC_(holo_num):STR_HOLO_INFO_NAME_(holo_num),"Truncated",Str::Length("Truncated"));
	}
}

bool FOServer::Act_Move(Critter* cr, ushort hx, ushort hy, uint move_params)
{
	uint map_id=cr->GetMap();
	if(!map_id) return false;

	// Check run/walk
	bool is_run=FLAG(move_params,MOVE_PARAM_RUN);
	if(is_run && !cr->IsCanRun())
	{
		// Switch to walk
		move_params^=MOVE_PARAM_RUN;
		is_run=false;
	}
	if(!is_run && !cr->IsCanWalk()) return false;

	// Check
	Map* map=MapMngr.GetMap(map_id,true);
	if(!map || map_id!=cr->GetMap() || hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) return false;

	// Check turn based
	if(!cr->CheckMyTurn(map))
	{
		cr->Send_XY(cr);
		cr->Send_ParamOther(OTHER_YOU_TURN,0);
		WriteLogF(_FUNC_," - Is not critter<%s> turn.\n",cr->GetInfo());
		return false;
	}

	if(map->IsTurnBasedOn)
	{
		int ap_cost=cr->GetApCostCritterMove(is_run)/AP_DIVIDER;
		int move_ap=cr->Data.Params[ST_MOVE_AP];
		if(ap_cost)
		{
			if((cr->GetParam(ST_CURRENT_AP)+move_ap)/ap_cost<=0)
			{
				cr->Send_XY(cr);
				cr->Send_Param(ST_CURRENT_AP);
				cr->Send_Param(ST_MOVE_AP);
				return false;
			}
			int steps=(cr->GetParam(ST_CURRENT_AP)+move_ap)/ap_cost-1;
			if(steps<MOVE_PARAM_STEP_COUNT) move_params|=(MOVE_PARAM_STEP_DISALLOW<<(steps*MOVE_PARAM_STEP_BITS)); // Cut steps
			if(move_ap)
			{
				if(ap_cost>move_ap)
				{
					cr->SubMoveAp(move_ap);
					cr->SubAp(ap_cost-move_ap);
				}
				else cr->SubMoveAp(ap_cost);
			}
			else cr->SubAp(ap_cost);
			if(cr->GetAllAp()<=0) map->EndCritterTurn();
		}
	}
	else if(cr->GetParam(TO_BATTLE))
	{
		int ap_cost=cr->GetApCostCritterMove(is_run);
		if(cr->GetRealAp()<ap_cost && !Singleplayer)
		{
			cr->Send_XY(cr);
			cr->Send_Param(ST_CURRENT_AP);
			return false;
		}
		if(ap_cost)
		{
			int steps=cr->GetRealAp()/ap_cost-1;
			if(steps<MOVE_PARAM_STEP_COUNT) move_params|=(MOVE_PARAM_STEP_DISALLOW<<(steps*MOVE_PARAM_STEP_BITS)); // Cut steps
			cr->Data.Params[ST_CURRENT_AP]-=ap_cost;
			cr->ApRegenerationTick=0;
		}
	}

	// Check passed
	ushort fx=cr->GetHexX();
	ushort fy=cr->GetHexY();
	uchar dir=GetNearDir(fx,fy,hx,hy);
	uint multihex=cr->GetMultihex();

	if(!map->IsMovePassed(hx,hy,dir,multihex))
	{
		if(cr->IsPlayer())
		{
			cr->Send_XY(cr);
			Critter* cr_hex=map->GetHexCritter(hx,hy,false,false);
			if(cr_hex) cr->Send_XY(cr_hex);
		}
		return false;
	}

	// Set last move type
	cr->IsRuning=is_run;

	// Process step
	bool is_dead=cr->IsDead();
	map->UnsetFlagCritter(fx,fy,multihex,is_dead);
	cr->PrevHexX=fx;
	cr->PrevHexY=fy;
	cr->PrevHexTick=Timer::GameTick();
	cr->Data.HexX=hx;
	cr->Data.HexY=hy;
	map->SetFlagCritter(hx,hy,multihex,is_dead);

	// Set dir
	cr->Data.Dir=dir;

	if(is_run)
	{
		if(!cr->IsRawParam(PE_SILENT_RUNNING) && cr->IsRawParam(MODE_HIDE))
		{
			cr->ChangeParam(MODE_HIDE);
			cr->Data.Params[MODE_HIDE]=0;
		}
		cr->SetBreakTimeDelta(cr->GetTimeRun());
	}
	else
	{
		cr->SetBreakTimeDelta(cr->GetTimeWalk());
	}

	cr->SendA_Move(move_params);
	cr->ProcessVisibleCritters();
	cr->ProcessVisibleItems();

	if(cr->GetMap()==map->GetId())
	{
		// Triggers
		VerifyTrigger(map,cr,fx,fy,hx,hy,dir);

		// Out trap
		if(map->IsHexTrap(fx,fy))
		{
			ItemPtrVec traps;
			map->GetItemsTrap(fx,fy,traps,true);
			for(ItemPtrVecIt it=traps.begin(),end=traps.end();it!=end;++it)
				(*it)->EventWalk(cr,false,dir);
		}

		// In trap
		if(map->IsHexTrap(hx,hy))
		{
			ItemPtrVec traps;
			map->GetItemsTrap(hx,hy,traps,true);
			for(ItemPtrVecIt it=traps.begin(),end=traps.end();it!=end;++it)
				(*it)->EventWalk(cr,true,dir);
		}

		// Try transit
		MapMngr.TryTransitCrGrid(cr,map,cr->GetHexX(),cr->GetHexY(),false);
	}

	return true;
}

bool FOServer::Act_Attack(Critter* cr, uchar rate_weap, uint target_id)
{
/************************************************************************/
/* Check & Prepare                                                      */
/************************************************************************/
	cr->SetBreakTime(GameOpt.Breaktime);

	if(cr->GetId()==target_id)
	{
		WriteLogF(_FUNC_," - Critter<%s> self attack.\n",cr->GetInfo());
		return false;
	}

	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map)
	{
		WriteLogF(_FUNC_," - Map not found, map id<%u>, critter<%s>.\n",cr->GetMap(),cr->GetInfo());
		return false;
	}

	if(!cr->CheckMyTurn(map))
	{
		WriteLogF(_FUNC_," - Is not critter<%s> turn.\n",cr->GetInfo());
		return false;
	}

	Critter* t_cr=cr->GetCritSelf(target_id,true);
	if(!t_cr)
	{
		WriteLogF(_FUNC_," - Target critter not found, target id<%u>, critter<%s>.\n",target_id,cr->GetInfo());
		return false;
	}

	if(cr->GetMap()!=t_cr->GetMap())
	{
		WriteLogF(_FUNC_," - Other maps, critter<%s>, target critter<%s>.\n",cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	if(t_cr->IsDead())
	{
		//if(cr->IsPlayer()) WriteLogF(_FUNC_," - Target critter is dead, critter<%s>, target critter<%s>.\n",cr->GetInfo(),t_cr->GetInfo());
		cr->Send_AddCritter(t_cr); //Refresh
		return false;
	}

	int hx=cr->GetHexX();
	int hy=cr->GetHexY();
	int tx=t_cr->GetHexX();
	int ty=t_cr->GetHexY();

	// Get weapon, ammo and armor
	Item* weap=cr->ItemSlotMain;
	if(!weap->IsWeapon())
	{
		WriteLogF(_FUNC_," - Critter item is not weapon, critter<%s>, target critter<%s>.\n",cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	if(weap->IsBroken())
	{
		WriteLogF(_FUNC_," - Critter weapon is broken, critter<%s>, target critter<%s>.\n",cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	if(weap->IsTwoHands() && cr->IsDmgArm())
	{
		WriteLogF(_FUNC_," - Critter is damaged arm on two hands weapon, critter<%s>, target critter<%s>.\n",cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	if(cr->IsDmgTwoArm() && weap->GetId())
	{
		WriteLogF(_FUNC_," - Critter is damaged two arms on armed attack, critter<%s>, target critter<%s>.\n",cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	uchar aim=rate_weap>>4;
	uchar use=rate_weap&0xF;

	if(use>=MAX_USES)
	{
		WriteLogF(_FUNC_," - Use<%u> invalid value, critter<%s>, target critter<%s>.\n",use,cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	if(!(weap->Proto->Weapon_ActiveUses&(1<<use)))
	{
		WriteLogF(_FUNC_," - Use<%u> is not aviable, critter<%s>, target critter<%s>.\n",use,cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	if(aim>=MAX_HIT_LOCATION)
	{
		WriteLogF(_FUNC_," - Aim<%u> invalid value, critter<%s>, target critter<%s>.\n",aim,cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	if(aim && !CritType::IsCanAim(cr->GetCrType()))
	{
		WriteLogF(_FUNC_," - Aim is not available for this critter type, crtype<%u>, aim<%u>, critter<%s>, target critter<%s>.\n",cr->GetCrType(),aim,cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	if(aim && cr->IsRawParam(MODE_NO_AIM))
	{
		WriteLogF(_FUNC_," - Aim is not available with critter no aim mode, aim<%u>, critter<%s>, target critter<%s>.\n",aim,cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	if(aim && !weap->WeapIsCanAim(use))
	{
		WriteLogF(_FUNC_," - Aim is not available for this weapon, aim<%u>, weapon pid<%u>, critter<%s>, target critter<%s>.\n",aim,weap->GetProtoId(),cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	if(!CritType::IsAnim1(cr->GetCrType(),weap->Proto->Weapon_Anim1))
	{
		WriteLogF(_FUNC_," - Anim1 is not available for this critter type, crtype<%u>, anim1<%d>, critter<%s>, target critter<%s>.\n",cr->GetCrType(),weap->Proto->Weapon_Anim1,cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	uint max_dist=cr->GetAttackDist(weap,use)+t_cr->GetMultihex();
	if(!CheckDist(hx,hy,tx,ty,max_dist) && !(Timer::GameTick()<t_cr->PrevHexTick+500 && CheckDist(hx,hy,t_cr->PrevHexX,t_cr->PrevHexY,max_dist)))
	{
		cr->Send_XY(cr);
		cr->Send_XY(t_cr);
		return false;
	}

	TraceData trace;
	trace.TraceMap=map;
	trace.BeginHx=hx;
	trace.BeginHy=hy;
	trace.EndHx=tx;
	trace.EndHy=ty;
	trace.Dist=(weap->Proto->Weapon_MaxDist[use]>2?max_dist:0);
	trace.FindCr=t_cr;
	MapMngr.TraceBullet(trace);
	if(!trace.IsCritterFounded)
	{
		cr->Send_XY(cr);
		cr->Send_XY(t_cr);
		// if(cr->IsPlayer()) WriteLogF(_FUNC_," - Distance trace fail, critter<%s>, target critter<%s>.\n",cr->GetInfo(),t_cr->GetInfo());
		return false;
	}

	int ap_cost=(GameOpt.GetUseApCost?GameOpt.GetUseApCost(cr,weap,rate_weap):1);
	if(cr->GetParam(ST_CURRENT_AP)<ap_cost && !Singleplayer)
	{
		cr->Send_Param(ST_CURRENT_AP);
		WriteLogF(_FUNC_," - Not enough AP, critter<%s>, target critter<%s>.\n",cr->GetInfo(),t_cr->GetInfo());
		return false;
	}
	//WriteLog("Proto %u, ap %u.\n",weap->GetProtoId(),ap_cost);

	// Ammo
	ProtoItem* ammo=NULL;
	if(weap->WeapGetAmmoCaliber() && weap->WeapGetMaxAmmoCount())
	{
		ammo=ItemMngr.GetProtoItem(weap->Data.TechInfo.AmmoPid);
		if(!ammo)
		{
			weap->Data.TechInfo.AmmoPid=weap->Proto->Weapon_DefaultAmmoPid;
			ammo=ItemMngr.GetProtoItem(weap->Data.TechInfo.AmmoPid);
		}

		if(!ammo)
		{
			WriteLogF(_FUNC_," - Critter weapon ammo not found, critter<%s>, target critter<%s>.\n",cr->GetInfo(),t_cr->GetInfo());
			return false;
		}

		if(!ammo->IsAmmo())
		{
			WriteLogF(_FUNC_," - Critter weapon ammo is not ammo type, critter<%s>, target critter<%s>.\n",cr->GetInfo(),t_cr->GetInfo());
			return false;
		}
	}

	// No ammo
	if(weap->WeapGetMaxAmmoCount() && !cr->IsRawParam(MODE_UNLIMITED_AMMO))
	{
		if(!weap->Data.TechInfo.AmmoCount)
		{
			WriteLogF(_FUNC_," - Critter bullets count is zero, critter<%s>.\n",cr->GetInfo());
			return false;
		}
	}

	ushort ammo_round=weap->Proto->Weapon_Round[use];
	if(!ammo_round) ammo_round=1;

	// Script events
	bool event_result=(weap->GetId()?weap->EventAttack(cr,t_cr):false);
	if(!event_result) event_result=cr->EventAttack(t_cr);
	if(event_result)
	{
		//cr->SendAA_Action(ACTION_USE_WEAPON,rate_weap,weap);
		cr->SubAp(ap_cost);
		if(map->IsTurnBasedOn && !cr->GetAllAp()) map->EndCritterTurn();
		return true;
	}

	// Ap, Turn based
	cr->SubAp(ap_cost);
	if(map->IsTurnBasedOn && !cr->GetAllAp()) map->EndCritterTurn();

	// Run script
	if(Script::PrepareContext(ServerFunctions.CritterAttack,_FUNC_,cr->GetInfo()))
	{
		Script::SetArgObject(cr);
		Script::SetArgObject(t_cr);
		Script::SetArgObject(weap->Proto);
		Script::SetArgUChar(MAKE_ITEM_MODE(use,aim));
		Script::SetArgObject(ammo);
		Script::RunPrepared();
	}
	return true;
}

bool FOServer::Act_Reload(Critter* cr, uint weap_id, uint ammo_id)
{
	cr->SetBreakTime(GameOpt.Breaktime);

	if(!cr->CheckMyTurn(NULL))
	{
		WriteLogF(_FUNC_," - Is not critter<%s> turn.\n",cr->GetInfo());
		cr->Send_Param(ST_CURRENT_AP);
		return false;
	}

	Item* weap=cr->GetItem(weap_id,true);
	if(!weap)
	{
		WriteLogF(_FUNC_," - Unable to find weapon, id<%u>, critter<%s>.\n",weap_id,cr->GetInfo());
		return false;
	}

	if(!weap->IsWeapon())
	{
		WriteLogF(_FUNC_," - Invalid type of weapon<%u>, critter<%s>.\n",weap->GetType(),cr->GetInfo());
		return false;
	}

	if(!weap->WeapGetMaxAmmoCount())
	{
		WriteLogF(_FUNC_," - Weapon is not have holder, id<%u>, critter<%s>.\n",weap_id,cr->GetInfo());
		return false;
	}

	int ap_cost=(GameOpt.GetUseApCost?GameOpt.GetUseApCost(cr,weap,USE_RELOAD):1);
	if(cr->GetParam(ST_CURRENT_AP)<ap_cost && !Singleplayer)
	{
		WriteLogF(_FUNC_," - Not enough AP, critter<%s>.\n",cr->GetInfo());
		cr->Send_Param(ST_CURRENT_AP);
		return false;
	}
	cr->SubAp(ap_cost);

	Item* ammo=(ammo_id?cr->GetItem(ammo_id,true):NULL);
	if(ammo_id && !ammo)
	{
		WriteLogF(_FUNC_," - Unable to find ammo, id<%u>, critter<%s>.\n",ammo_id,cr->GetInfo());
		return false;
	}

	if(ammo && weap->WeapGetAmmoCaliber()!=ammo->AmmoGetCaliber())
	{
		WriteLogF(_FUNC_," - Different calibers, critter<%s>.\n",cr->GetInfo());
		return false;
	}

	if(ammo && weap->WeapGetAmmoPid()==ammo->GetProtoId() && weap->WeapIsFull())
	{
		WriteLogF(_FUNC_," - Weapon is full, id<%u>, critter<%s>.\n",weap_id,cr->GetInfo());
		return false;
	}

	if(!Script::PrepareContext(ServerFunctions.CritterReloadWeapon,_FUNC_,cr->GetInfo())) return false;
	Script::SetArgObject(cr);
	Script::SetArgObject(weap);
	Script::SetArgObject(ammo);
	if(!Script::RunPrepared()) return false;

	cr->SendAA_Action(ACTION_RELOAD_WEAPON,0,weap);
	Map* map=MapMngr.GetMap(cr->GetMap());
	if(map && map->IsTurnBasedOn && !cr->GetAllAp()) map->EndCritterTurn();
	return true;
}

#pragma MESSAGE("Add using hands/legs.")
bool FOServer::Act_Use(Critter* cr, uint item_id, int skill, int target_type, uint target_id, ushort target_pid, uint param)
{
	cr->SetBreakTime(GameOpt.Breaktime);

	Map* map=MapMngr.GetMap(cr->GetMap());
	if(map && !cr->CheckMyTurn(map))
	{
		cr->Send_Param(ST_CURRENT_AP);
		WriteLogF(_FUNC_," - Is not critter<%s> turn.\n",cr->GetInfo());
		return false;
	}

	Item* item=NULL;
	if(item_id)
	{
		item=cr->GetItem(item_id,cr->IsPlayer());
		if(!item)
		{
			WriteLogF(_FUNC_," - Item not found, id<%u>, critter<%s>.\n",item_id,cr->GetInfo());
			return false;
		}
	}

	int ap_cost=(item_id?(GameOpt.GetUseApCost?GameOpt.GetUseApCost(cr,item,USE_USE):1):cr->GetApCostUseSkill());
	if(cr->GetParam(ST_CURRENT_AP)<ap_cost && !Singleplayer)
	{
		cr->Send_Param(ST_CURRENT_AP);
		WriteLogF(_FUNC_," - Not enough AP, critter<%s>.\n",cr->GetInfo());
		return false;
	}
	cr->SubAp(ap_cost);

	if(item_id)
	{
		if(!item->GetCount())
		{
			WriteLogF(_FUNC_," - Error, count is zero, id<%u>, critter<%s>.\n",item->GetId(),cr->GetInfo());
			cr->EraseItem(item,true);
			ItemMngr.ItemToGarbage(item);
			return false;
		}
	}

	Critter* target_cr=NULL;
	Item* target_item=NULL;
	MapObject* target_scen=NULL;

	if(target_type==TARGET_CRITTER)
	{
		if(cr->GetId()!=target_id)
		{
			if(cr->GetMap())
			{
				target_cr=cr->GetCritSelf(target_id,true);
				if(!target_cr)
				{
					WriteLogF(_FUNC_," - Target critter not found, id<%u>, critter<%s>.\n",target_id,cr->GetInfo());
					return false;
				}

				if(!CheckDist(cr->GetHexX(),cr->GetHexY(),target_cr->GetHexX(),target_cr->GetHexY(),cr->GetUseDist()+target_cr->GetMultihex()))
				{
					cr->Send_XY(cr);
					cr->Send_XY(target_cr);
					WriteLogF(_FUNC_," - Target critter too far, id<%u>, critter<%s>.\n",target_id,cr->GetInfo());
					return false;
				}
			}
			else
			{
				if(!cr->GroupMove)
				{
					WriteLogF(_FUNC_," - Group move null ptr, critter<%s>.\n",cr->GetInfo());
					return false;
				}

				target_cr=cr->GroupMove->GetCritter(target_id);
				if(!target_cr)
				{
					WriteLogF(_FUNC_," - Target critter not found, id<%u>, critter<%s>.\n",target_id,cr->GetInfo());
					return false;
				}
			}
		}
	}
	else if(target_type==TARGET_SELF)
	{
		target_id=cr->GetId();
	}
	else if(target_type==TARGET_SELF_ITEM)
	{
		target_item=cr->GetItem(target_id,cr->IsPlayer());
		if(!target_item)
		{
			WriteLogF(_FUNC_," - Item not found in critter inventory, critter<%s>.\n",cr->GetInfo());
			return false;
		}
	}
	else if(target_type==TARGET_ITEM)
	{
		if(!cr->GetMap())
		{
			WriteLogF(_FUNC_," - Can't get item, critter<%s> on global map.\n",cr->GetInfo());
			return false;
		}

		if(!map)
		{
			WriteLogF(_FUNC_," - Map not found, map id<%u>, critter<%s>.\n",cr->GetMap(),cr->GetInfo());
			return false;
		}

		target_item=map->GetItem(target_id);
		if(!target_item)
		{
			WriteLogF(_FUNC_," - Target item not found, id<%u>, critter<%s>.\n",target_id,cr->GetInfo());
			return false;
		}

		if(target_item->IsHidden())
		{
			WriteLogF(_FUNC_," - Target item is hidden, id<%u>, critter<%s>.\n",target_id,cr->GetInfo());
			return false;
		}

		if(!CheckDist(cr->GetHexX(),cr->GetHexY(),target_item->ACC_HEX.HexX,target_item->ACC_HEX.HexY,cr->GetUseDist()))
		{
			cr->Send_XY(cr);
			WriteLogF(_FUNC_," - Target item too far, id<%u>, critter<%s>.\n",target_id,cr->GetInfo());
			return false;
		}
	}
	else if(target_type==TARGET_SCENERY)
	{
		if(!cr->GetMap())
		{
			WriteLogF(_FUNC_," - Can't get scenery, critter<%s> on global map.\n",cr->GetInfo());
			return false;
		}

		if(!map)
		{
			WriteLogF(_FUNC_," - Map not found_, map id<%u>, critter<%s>.\n",cr->GetMap(),cr->GetInfo());
			return false;
		}

		ushort hx=target_id>>16;
		ushort hy=target_id&0xFFFF;

		if(!CheckDist(cr->GetHexX(),cr->GetHexY(),hx,hy,cr->GetUseDist()))
		{
			cr->Send_XY(cr);
			WriteLogF(_FUNC_," - Target scenery too far, critter<%s>, hx<%u>, hy<%u>.\n",cr->GetInfo(),hx,hy);
			return false;
		}

		ProtoItem* proto_scenery=ItemMngr.GetProtoItem(target_pid);
		if(!proto_scenery)
		{
			WriteLogF(_FUNC_," - Proto of scenery not find. Critter<%s>, pid<%u>.\n",cr->GetInfo(),target_pid);
			return false;
		}

		if(!proto_scenery->IsGeneric())
		{
			WriteLogF(_FUNC_," - Target scenery is not generic. Critter<%s>.\n",cr->GetInfo());
			return false;
		}

		target_scen=map->Proto->GetMapScenery(hx,hy,target_pid);
		if(!target_scen)
		{
			WriteLogF(_FUNC_," - Scenery not find. Critter<%s>.\n",cr->GetInfo());
			return false;
		}
	}
	else
	{
		WriteLogF(_FUNC_," - Unknown target type<%d>, critter<%s>.\n",target_type,cr->GetInfo());
		return false;
	}

	// Send action
	if(cr->GetMap())
	{
		if(item)
		{
			cr->SendAA_Action(ACTION_USE_ITEM,0,item);
		}
		else
		{
			int skill_index=SKILL_OFFSET(skill);
			if(target_item) cr->SendAA_Action(ACTION_USE_SKILL,skill_index,target_item);
			else if(target_cr) cr->SendAA_Action(ACTION_USE_SKILL,skill_index,NULL);
			else if(target_scen)
			{
				Item item_scenery;
				item_scenery.Id=uint(-1);
				item_scenery.Init(ItemMngr.GetProtoItem(target_scen->ProtoId));
				cr->SendAA_Action(ACTION_USE_SKILL,skill_index,&item_scenery);
			}
		}
	}

	// Check turn based
	if(map && map->IsTurnBasedOn && !cr->GetAllAp()) map->EndCritterTurn();

	// Scenery
	if(target_scen && target_scen->RunTime.BindScriptId>0)
	{
		if(!Script::PrepareContext(target_scen->RunTime.BindScriptId,_FUNC_,cr->GetInfo())) return false;
		Script::SetArgObject(cr);
		Script::SetArgObject(target_scen);
		Script::SetArgUInt(item?SKILL_PICK_ON_GROUND:SKILL_OFFSET(skill));
		Script::SetArgObject(item);
		for(int i=0,j=MIN(target_scen->MScenery.ParamsCount,5);i<j;i++) Script::SetArgUInt(target_scen->MScenery.Param[i]);
		if(Script::RunPrepared() && Script::GetReturnedBool()) return true;
	}

	// Item
	if(item)
	{
		if(target_item && target_item->EventUseOnMe(cr,item)) return true;
		if(item->EventUse(cr,target_cr,target_item,target_scen)) return true;
		if(cr->EventUseItem(item,target_cr,target_item,target_scen)) return true;
		if(target_cr && target_cr->EventUseItemOnMe(cr,item)) return true;
		if(Script::PrepareContext(ServerFunctions.CritterUseItem,_FUNC_,cr->GetInfo()))
		{
			Script::SetArgObject(cr);
			Script::SetArgObject(item);
			Script::SetArgObject(target_cr);
			Script::SetArgObject(target_item);
			Script::SetArgObject(target_scen);
			Script::SetArgUInt(param);
			if(Script::RunPrepared() && Script::GetReturnedBool()) return true;
		}

		// Default process
		if(item->IsHolodisk() && target_type==TARGET_SELF && cr->IsPlayer())
		{
			AddPlayerHoloInfo((Client*)cr,item->HolodiskGetNum(),true);
		}
		// Nothing happens
		else
		{
			cr->Send_TextMsg(cr,STR_USE_NOTHING,SAY_NETMSG,TEXTMSG_GAME);
		}
	}
	// Skill
	else
	{
		if(target_item && target_item->EventSkill(cr,skill)) return true;
		if(cr->EventUseSkill(skill,target_cr,target_item,target_scen)) return true;
		if(target_cr && target_cr->EventUseSkillOnMe(cr,skill)) return true;
		if(!Script::PrepareContext(ServerFunctions.CritterUseSkill,_FUNC_,cr->GetInfo())) return false;
		Script::SetArgObject(cr);
		Script::SetArgUInt(SKILL_OFFSET(skill));
		Script::SetArgObject(target_cr);
		Script::SetArgObject(target_item);
		Script::SetArgObject(target_scen);
		if(Script::RunPrepared() && Script::GetReturnedBool()) return true;

		// Nothing happens
		cr->Send_TextMsg(cr,STR_USE_NOTHING,SAY_NETMSG,TEXTMSG_GAME);
	}

	return true;
}

bool FOServer::Act_PickItem(Critter* cr, ushort hx, ushort hy, ushort pid)
{
	cr->SetBreakTime(GameOpt.Breaktime);

	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map) return false;

	if(!cr->CheckMyTurn(map))
	{
		WriteLogF(_FUNC_," - Is not critter<%s> turn.\n",cr->GetInfo());
		cr->Send_Param(ST_CURRENT_AP);
		return false;
	}

	int ap_cost=cr->GetApCostPickItem();
	if(cr->GetParam(ST_CURRENT_AP)<ap_cost && !Singleplayer)
	{
		WriteLogF(_FUNC_," - Not enough AP, critter<%s>.\n",cr->GetInfo());
		cr->Send_Param(ST_CURRENT_AP);
		return false;
	}
	cr->SubAp(ap_cost);

	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) return false;

	if(!CheckDist(cr->GetHexX(),cr->GetHexY(),hx,hy,cr->GetUseDist()))
	{
		WriteLogF(_FUNC_," - Wrong distance, critter<%s>.\n",cr->GetInfo());
		cr->Send_XY(cr);
		return false;
	}

	ProtoItem* proto=ItemMngr.GetProtoItem(pid);
	if(!proto)
	{
		WriteLogF(_FUNC_," - Proto not find, pid<%u>, critter<%s>.\n",pid,cr->GetInfo());
		return false;
	}

	if(map->IsTurnBasedOn && !cr->GetAllAp()) map->EndCritterTurn();

	if(proto->IsItem())
	{
		Item* pick_item=map->GetItemHex(hx,hy,pid,cr->IsPlayer()?cr:NULL);
		if(!pick_item) return false;

		cr->SendAA_Action(ACTION_PICK_ITEM,0,pick_item);

		if(pick_item->EventSkill(cr,SKILL_PICK_ON_GROUND)) return true;
		if(cr->EventUseSkill(SKILL_PICK_ON_GROUND,NULL,pick_item,NULL)) return true;
		if(Script::PrepareContext(ServerFunctions.CritterUseSkill,_FUNC_,cr->GetInfo()))
		{
			Script::SetArgObject(cr);
			Script::SetArgUInt(SKILL_PICK_ON_GROUND);
			Script::SetArgObject(NULL);
			Script::SetArgObject(pick_item);
			Script::SetArgObject(NULL);
			if(Script::RunPrepared() && Script::GetReturnedBool()) return true;
		}

		cr->Send_TextMsg(cr,STR_USE_NOTHING,SAY_NETMSG,TEXTMSG_GAME);
	}
	else if(proto->IsScen())
	{
		MapObject* pick_scenery=map->Proto->GetMapScenery(hx,hy,pid);
		if(!pick_scenery)
		{
			cr->Send_Text(cr,"Scenery not found, maybe map outdated.",SAY_NETMSG);
			return false;
		}

		Item pick_item;
		pick_item.Id=uint(-1);
		pick_item.Init(proto);
		cr->SendAA_Action(ACTION_PICK_ITEM,0,&pick_item);

		if(proto->IsGeneric() && pick_scenery->RunTime.BindScriptId>0)
		{
			if(!Script::PrepareContext(pick_scenery->RunTime.BindScriptId,_FUNC_,cr->GetInfo())) return false;
			Script::SetArgObject(cr);
			Script::SetArgObject(pick_scenery);
			Script::SetArgUInt(SKILL_PICK_ON_GROUND);
			Script::SetArgObject(NULL);
			for(int i=0,j=MIN(pick_scenery->MScenery.ParamsCount,5);i<j;i++) Script::SetArgUInt(pick_scenery->MScenery.Param[i]);
			if(Script::RunPrepared() && Script::GetReturnedBool()) return true;
		}

		if(cr->EventUseSkill(SKILL_PICK_ON_GROUND,NULL,NULL,pick_scenery)) return true;
		if(Script::PrepareContext(ServerFunctions.CritterUseSkill,_FUNC_,cr->GetInfo()))
		{
			Script::SetArgObject(cr);
			Script::SetArgUInt(SKILL_PICK_ON_GROUND);
			Script::SetArgObject(NULL);
			Script::SetArgObject(NULL);
			Script::SetArgObject(pick_scenery);
			if(Script::RunPrepared() && Script::GetReturnedBool()) return true;
		}

		cr->Send_TextMsg(cr,STR_USE_NOTHING,SAY_NETMSG,TEXTMSG_GAME);
	}
	else if(proto->IsGrid())
	{
		switch(proto->Grid_Type)
		{
		case GRID_STAIRS:
		case GRID_LADDERBOT:
		case GRID_LADDERTOP:
		case GRID_ELEVATOR:
			{
				Item pick_item;
				pick_item.Id=uint(-1);
				pick_item.Init(proto);
				cr->SendAA_Action(ACTION_PICK_ITEM,0,&pick_item);

				MapMngr.TryTransitCrGrid(cr,map,hx,hy,false);
			}
			break;
		default:
			cr->Send_TextMsg(cr,STR_USE_NOTHING,SAY_NETMSG,TEXTMSG_GAME);
			break;
		}
	}
	else if(proto->IsWall())
	{
		cr->Send_TextMsg(cr,STR_USE_NOTHING,SAY_NETMSG,TEXTMSG_GAME);
		return false;
	}
	else
	{
		cr->Send_TextMsg(cr,STR_USE_NOTHING,SAY_NETMSG,TEXTMSG_GAME);
		return false;
	}
	return true;
}

void FOServer::KillCritter(Critter* cr, uint anim2, Critter* attacker)
{
	if(cr->Data.Params[MODE_INVULNERABLE]) return;

	// Close talk
	if(cr->IsPlayer())
	{
		Client* cl=(Client*)cr;
		if(cl->IsTalking()) cl->CloseTalk();
	}
	// Disable sneaking
	if(cr->Data.Params[MODE_HIDE])
	{
		cr->ChangeParam(MODE_HIDE);
		cr->Data.Params[MODE_HIDE]=0;
	}

	// Process dead
	cr->ToDead(anim2,true);
	cr->EventDead(attacker);
	Map* map=MapMngr.GetMap(cr->GetMap());
	if(map) map->EventCritterDead(cr,attacker);
	if(Script::PrepareContext(ServerFunctions.CritterDead,_FUNC_,cr->GetInfo()))
	{
		Script::SetArgObject(cr);
		Script::SetArgObject(attacker);
		Script::RunPrepared();
	}
}

void FOServer::RespawnCritter(Critter* cr)
{
	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map)
	{
		WriteLogF(_FUNC_," - Current map not found, continue dead. Critter<%s>.\n",cr->GetInfo());
		return;
	}

	ushort hx=cr->GetHexX();
	ushort hy=cr->GetHexY();
	uint multihex=cr->GetMultihex();
	if(!map->IsHexesPassed(hx,hy,multihex))
	{
		// WriteLogF(_FUNC_," - Live critter on hex, continue dead.\n");
		return;
	}

	map->UnsetFlagCritter(hx,hy,multihex,true);
	map->SetFlagCritter(hx,hy,multihex,false);

	cr->Data.Cond=COND_LIFE;
	if(cr->Data.Params[ST_CURRENT_HP]<1)
	{
		cr->ChangeParam(ST_CURRENT_HP);
		cr->Data.Params[ST_CURRENT_HP]=1;
	}
	cr->Send_Action(cr,ACTION_RESPAWN,0,NULL);
	cr->SendAA_Action(ACTION_RESPAWN,0,NULL);
	cr->EventRespawn();
	if(Script::PrepareContext(ServerFunctions.CritterRespawn,_FUNC_,cr->GetInfo()))
	{
		Script::SetArgObject(cr);
		Script::RunPrepared();
	}
}

void FOServer::KnockoutCritter(Critter* cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, ushort knock_hx, ushort knock_hy)
{
	// Close talk
	if(cr->IsPlayer())
	{
		Client* cl=(Client*)cr;
		if(cl->IsTalking()) cl->CloseTalk();
	}
	// Disable sneaking
	if(cr->Data.Params[MODE_HIDE])
	{
		cr->ChangeParam(MODE_HIDE);
		cr->Data.Params[MODE_HIDE]=0;
	}

	// Process knockout
	int x1=cr->GetHexX();
	int y1=cr->GetHexY();
	int x2=knock_hx;
	int y2=knock_hy;

	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map || x2>=map->GetMaxHexX() || y2>=map->GetMaxHexY()) return;

	TraceData td;
	td.TraceMap=map;
	td.FindCr=cr;
	td.BeginHx=x1;
	td.BeginHy=y1;
	td.EndHx=x2;
	td.EndHy=y2;
	td.HexCallback=FOServer::VerifyTrigger;
	MapMngr.TraceBullet(td);

	if(x1!=x2 || y1!=y2)
	{
		uint multihex=cr->GetMultihex();
		bool is_dead=cr->IsDead();
		map->UnsetFlagCritter(x1,y1,multihex,is_dead);
		cr->Data.HexX=x2;
		cr->Data.HexY=y2;
		map->SetFlagCritter(x2,y2,multihex,is_dead);
	}

	cr->ToKnockout(anim2begin,anim2idle,anim2end,lost_ap,knock_hx,knock_hy);
	cr->EventKnockout(anim2begin,anim2idle,anim2end,lost_ap,DistGame(x1,y1,x2,y2));
}

bool FOServer::MoveRandom(Critter* cr)
{
	UCharVec dirs(6);
	for(int i=0;i<6;i++) dirs[i]=i;
	std::random_shuffle(dirs.begin(),dirs.end());

	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map) return false;

	uint multihex=cr->GetMultihex();
	ushort maxhx=map->GetMaxHexX();
	ushort maxhy=map->GetMaxHexY();

	for(int i=0;i<6;i++)
	{
		uchar dir=dirs[i];
		ushort hx=cr->GetHexX();
		ushort hy=cr->GetHexY();
		if(MoveHexByDir(hx,hy,dir,maxhx,maxhy) && map->IsMovePassed(hx,hy,dir,multihex))
		{
			if(Act_Move(cr,hx,hy,0))
			{
				cr->Send_Move(cr,0);
				return true;
			}
			return false;
		}
	}
	return false;
}

bool FOServer::RegenerateMap(Map* map)
{
	// Copy need params
	uint map_id=map->GetId();
	ushort map_pid=map->GetPid();
	ProtoMap* map_proto=map->Proto;
	Location* map_loc=map->GetLocation(true);

	// Kick clients to global
	ClVec players;
	map->GetPlayers(players,true);
	for(ClVecIt it=players.begin(),end=players.end();it!=end;++it)
	{
		Client* cl=*it;
		if(!MapMngr.TransitToGlobal(cl,0,FOLLOW_FORCE,true))
		{
			WriteLogF(_FUNC_," - Can't kick client<%s> to global.\n",cl->GetInfo());
			return false;
		}
	}

	map->Clear(true);
	map->Init(map_proto,map_loc);
	map->SetId(map_id,map_pid);
	map->Generate();
	return true;
}

bool FOServer::VerifyTrigger(Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir)
{
	// Triggers
	bool result=false;
	if(map->IsHexTrigger(from_hx,from_hy) || map->IsHexTrigger(to_hx,to_hy))
	{
		MapObject* out_trigger=map->Proto->GetMapScenery(from_hx,from_hy,SP_SCEN_TRIGGER);
		MapObject* in_trigger=map->Proto->GetMapScenery(to_hx,to_hy,SP_SCEN_TRIGGER);
		if(!(out_trigger && in_trigger && out_trigger->MScenery.TriggerNum==in_trigger->MScenery.TriggerNum))
		{
			if(out_trigger && Script::PrepareContext(out_trigger->RunTime.BindScriptId,_FUNC_,cr->GetInfo()))
			{
				Script::SetArgObject(cr);
				Script::SetArgObject(out_trigger);
				Script::SetArgBool(false);
				Script::SetArgUChar(dir);
				for(int i=0,j=MIN(out_trigger->MScenery.ParamsCount,5);i<j;i++) Script::SetArgUInt(out_trigger->MScenery.Param[i]);
				if(Script::RunPrepared()) result=true;
			}
			if(in_trigger && Script::PrepareContext(in_trigger->RunTime.BindScriptId,_FUNC_,cr->GetInfo()))
			{
				Script::SetArgObject(cr);
				Script::SetArgObject(in_trigger);
				Script::SetArgBool(true);
				Script::SetArgUChar(dir);
				for(int i=0,j=MIN(in_trigger->MScenery.ParamsCount,5);i<j;i++) Script::SetArgUInt(in_trigger->MScenery.Param[i]);
				if(Script::RunPrepared()) result=true;
			}
		}
	}
	return result;
}

void FOServer::Process_CreateClient(Client* cl)
{
	// Check for ban by ip
	{
		SCOPE_LOCK(BannedLocker);

		uint ip=cl->GetIp();
		ClientBanned* ban=GetBanByIp(ip);
		if(ban && !Singleplayer)
		{
			cl->Send_TextMsg(cl,STR_NET_BANNED_IP,SAY_NETMSG,TEXTMSG_GAME);
			//cl->Send_TextMsgLex(cl,STR_NET_BAN_REASON,SAY_NETMSG,TEXTMSG_GAME,ban->GetBanLexems());
			cl->Send_TextMsgLex(cl,STR_NET_TIME_LEFT,SAY_NETMSG,TEXTMSG_GAME,Str::FormatBuf("$time%u",GetBanTime(*ban)));
			cl->Disconnect();
			return;
		}
	}

	uint msg_len;
	cl->Bin >> msg_len;

	// Protocol version
	ushort proto_ver=0;
	cl->Bin >> proto_ver;
	if(proto_ver!=FO_PROTOCOL_VERSION)
	{
		// WriteLogF(_FUNC_," - Wrong Protocol Version from SockId<%u>.\n",cl->Sock);
		cl->Send_TextMsg(cl,STR_NET_WRONG_NETPROTO,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	// Begin data encrypting
	cl->Bin.SetEncryptKey(1234567890);
	cl->Bout.SetEncryptKey(1234567890);

	// Name
	char name[MAX_NAME+1];
	cl->Bin.Pop(name,MAX_NAME);
	name[MAX_NAME]=0;
	Str::Copy(cl->Name,name);

	// Password
	cl->Bin.Pop(name,MAX_NAME);
	name[MAX_NAME]=0;
	Str::Copy(cl->Pass,name);

	// Receive params
	ushort params_count=0;
	cl->Bin >> params_count;

	if(params_count>MAX_PARAMS)
	{
		cl->Send_TextMsg(cl,STR_NET_DATATRANS_ERR,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	memzero(cl->Data.Params,sizeof(cl->Data.Params));
	for(ushort i=0;i<params_count;i++)
	{
		ushort index;
		int val;
		cl->Bin >> index;
		cl->Bin >> val;

		if(index>=MAX_PARAMS)
		{
			cl->Send_TextMsg(cl,STR_NET_DATATRANS_ERR,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			return;
		}

		cl->Data.Params[index]=val;
	}

	// Check net
	CHECK_IN_BUFF_ERROR_EX(cl,cl->Send_TextMsg(cl,STR_NET_DATATRANS_ERR,SAY_NETMSG,TEXTMSG_GAME));

	// Check data
	if(!CheckUserName(cl->Name) || (!Singleplayer && !CheckUserPass(cl->Pass)))
	{
		// WriteLogF(_FUNC_," - Error symbols in Name or Password.\n");
		cl->Send_TextMsg(cl,STR_NET_LOGINPASS_WRONG,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	uint name_len=Str::Length(cl->Name);
	if(name_len<MIN_NAME || name_len<GameOpt.MinNameLength ||
		name_len>MAX_NAME || name_len>GameOpt.MaxNameLength)
	{
		// WriteLog(_"Name or Password length too small.\n");
		cl->Send_TextMsg(cl,STR_NET_LOGINPASS_WRONG,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	uint pass_len=Str::Length(cl->Pass);
	if(!Singleplayer && (pass_len<MIN_NAME || pass_len<GameOpt.MinNameLength ||
		pass_len>MAX_NAME || pass_len>GameOpt.MaxNameLength))
	{
		// WriteLog(_"Name or Password length too small.\n");
		cl->Send_TextMsg(cl,STR_NET_LOGINPASS_WRONG,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	if(cl->Name[0]==' ' || cl->Name[name_len-1]==' ')
	{
		// WriteLog("Name begin or end space.\n");
		cl->Send_TextMsg(cl,STR_NET_BEGIN_END_SPACES,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	for(int i=0,j=name_len-1;i<j;i++)
	{
		if(cl->Name[i]==' ' && cl->Name[i+1]==' ')
		{
			// WriteLog("Name two space.\n");
			cl->Send_TextMsg(cl,STR_NET_TWO_SPACE,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			return;
		}
	}

	int letters_rus=0,letters_eng=0;
	for(int i=0,j=name_len;i<j;i++)
	{
		char c=cl->Name[i];
		if((c>='a' && c<='z') || (c>='A' && c<='Z')) letters_eng++;
		else if((c>='à' && c<='ÿ') || (c>='À' && c<='ß') || c=='¸' || c=='¨') letters_rus++;
	}

	if(letters_eng && letters_rus)
	{
		// WriteLog("Different language in Name.\n");
		cl->Send_TextMsg(cl,STR_NET_DIFFERENT_LANG,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	int letters_len=letters_eng+letters_rus;
	if(Procent(name_len,letters_len)<70)
	{
		// WriteLog("Many special symbols in Name.\n");
		cl->Send_TextMsg(cl,STR_NET_MANY_SYMBOLS,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	// Check for exist
	if(!Singleplayer)
	{
		SaveClientsLocker.Lock();
		bool exist=(GetClientData(cl->Name)!=NULL);
		SaveClientsLocker.Unlock();

		if(exist)
		{
			cl->Send_TextMsg(cl,STR_NET_ACCOUNT_ALREADY,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			return;
		}
	}

	// Check brute force registration
#ifndef DEV_VESRION
	if(GameOpt.RegistrationTimeout && !Singleplayer)
	{
		SCOPE_LOCK(RegIpLocker);

		uint ip=cl->GetIp();
		uint reg_tick=GameOpt.RegistrationTimeout*1000;
		UIntMapIt it=RegIp.find(ip);
		if(it!=RegIp.end())
		{
			uint& last_reg=(*it).second;
			uint tick=Timer::FastTick();
			if(tick-last_reg<reg_tick)
			{
				cl->Send_TextMsg(cl,STR_NET_REGISTRATION_IP_WAIT,SAY_NETMSG,TEXTMSG_GAME);
				cl->Send_TextMsgLex(cl,STR_NET_TIME_LEFT,SAY_NETMSG,TEXTMSG_GAME,Str::FormatBuf("$time%u",(reg_tick-(tick-last_reg))/60000+1));
				cl->Disconnect();
				return;
			}
			last_reg=tick;
		}
		else
		{
			RegIp.insert(UIntMapVal(ip,Timer::FastTick()));
		}
	}
#endif

	bool allow=false;
	uint disallow_msg_num=0,disallow_str_num=0;
	if(Script::PrepareContext(ServerFunctions.PlayerRegistration,_FUNC_,cl->Name))
	{
		CScriptString* name=new CScriptString(cl->Name);
		CScriptString* pass=new CScriptString(cl->Pass);
		Script::SetArgUInt(cl->GetIp());
		Script::SetArgObject(name);
		Script::SetArgObject(pass);
		Script::SetArgAddress(&disallow_msg_num);
		Script::SetArgAddress(&disallow_str_num);
		if(Script::RunPrepared() && Script::GetReturnedBool()) allow=true;
		name->Release();
		pass->Release();
	}
	if(!allow)
	{
		if(disallow_msg_num<TEXTMSG_COUNT && disallow_str_num)
			cl->Send_TextMsg(cl,disallow_str_num,SAY_NETMSG,disallow_msg_num);
		else
			cl->Send_TextMsg(cl,STR_NET_LOGIN_SCRIPT_FAIL,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	// Register
	cl->NameStr=cl->Name;
	cl->Data.HexX=0;
	cl->Data.HexY=0;
	cl->Data.Dir=0;
	cl->Data.WorldX=GM_MAXX/2;
	cl->Data.WorldY=GM_MAXY/2;
	cl->Data.Cond=COND_LIFE;
	cl->Data.Multihex=-1;

	CritDataExt* data_ext=cl->GetDataExt();
	data_ext->PlayIp[0]=cl->GetIp();

	if(!cl->SetDefaultItems(ItemMngr.GetProtoItem(ITEM_DEF_SLOT),ItemMngr.GetProtoItem(ITEM_DEF_ARMOR)))
	{
		WriteLogF(_FUNC_," - Error set default items.\n");
		cl->Send_TextMsg(cl,STR_NET_SETPROTO_ERR,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	if(Singleplayer) cl->Data.Id=1;
	else cl->Data.Id=++LastClientId;
	cl->Access=ACCESS_DEFAULT;

	if(!SaveClient(cl,false))
	{
		WriteLogF(_FUNC_," - First save fail.\n");
		cl->Send_TextMsg(cl,STR_NET_BD_ERROR,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	if(Singleplayer)
	{
		SynchronizeLogicThreads();
		SyncManager::GetForCurThread()->UnlockAll();
		SYNC_LOCK(cl);
		if(!NewWorld())
		{
			WriteLogF(_FUNC_," - Generate new world fail.\n");
			cl->Send_TextMsg(cl,STR_SP_NEW_GAME_FAIL,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			ResynchronizeLogicThreads();
			return;
		}
		ResynchronizeLogicThreads();
	}

	if(!Singleplayer) cl->Send_TextMsg(cl,STR_NET_REG_SUCCESS,SAY_NETMSG,TEXTMSG_GAME);
	else cl->Send_TextMsg(cl,STR_SP_NEW_GAME_SUCCESS,SAY_NETMSG,TEXTMSG_GAME);

	BOUT_BEGIN(cl);
	cl->Bout << (uint)NETMSG_REGISTER_SUCCESS;
	BOUT_END(cl);
	cl->Disconnect();

	cl->AddRef();
	CrMngr.AddCritter(cl);
	MapMngr.AddCrToMap(cl,NULL,0,0,0);
	Job::PushBack(JOB_CRITTER,cl);

	if(Script::PrepareContext(ServerFunctions.CritterInit,_FUNC_,cl->GetInfo()))
	{
		Script::SetArgObject(cl);
		Script::SetArgBool(true);
		Script::RunPrepared();
	}
	SaveClient(cl,false);

	if(!Singleplayer)
	{
		ClientData data;
		data.Clear();
		Str::Copy(data.ClientName,cl->Name);
		Str::Copy(data.ClientPass,cl->Pass);
		data.ClientId=cl->GetId();

		SCOPE_LOCK(ClientsDataLocker);
		ClientsData.push_back(data);
	}
	else
	{
		ClientsSaveDataCount=0;
		AddClientSaveData(cl);
		SingleplayerSave.Valid=true;
		SingleplayerSave.CrData=ClientsSaveData[0];
		SingleplayerSave.PicData.clear();
	}
}

void FOServer::Process_LogIn(ClientPtr& cl)
{
	// Net protocol
	ushort proto_ver=0;
	cl->Bin >> proto_ver;
	if(proto_ver!=FO_PROTOCOL_VERSION)
	{
		// WriteLogF(_FUNC_," - Wrong Protocol Version from SockId<%u>.\n",cl->Sock);
		cl->Send_TextMsg(cl,STR_NET_WRONG_NETPROTO,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	// UIDs
	uint uidxor,uidor,uidcalc;
	uint uid[5];
	cl->Bin >> uid[4];

	// Begin data encrypting
	cl->Bin.SetEncryptKey(uid[4]+12345);
	cl->Bout.SetEncryptKey(uid[4]+12345);

	// Login, password
	char name[MAX_NAME+1];
	cl->Bin.Pop(name,MAX_NAME);
	name[MAX_NAME]=0;
	Str::Copy(cl->Name,name);
	cl->Bin >> uid[1];
	cl->Bin.Pop(name,MAX_NAME);
	name[MAX_NAME]=0;
	Str::Copy(cl->Pass,name);

	if(Singleplayer)
	{
		memzero(cl->Name,sizeof(cl->Name));
		memzero(cl->Pass,sizeof(cl->Pass));
	}

	// Bin hashes
	uint msg_language;
	uint textmsg_hash[TEXTMSG_COUNT];
	uint item_hash[ITEM_MAX_TYPES];
	uchar default_combat_mode;

	cl->Bin >> msg_language;
	for(int i=0;i<TEXTMSG_COUNT;i++) cl->Bin >> textmsg_hash[i];
	cl->Bin >> uidxor;
	cl->Bin >> uid[3];
	cl->Bin >> uid[2];
	cl->Bin >> uidor;
	for(int i=0;i<ITEM_MAX_TYPES;i++) cl->Bin >> item_hash[i];
	cl->Bin >> uidcalc;
	cl->Bin >> default_combat_mode;
	cl->Bin >> uid[0];
	CHECK_IN_BUFF_ERROR_EX(cl,cl->Send_TextMsg(cl,STR_NET_DATATRANS_ERR,SAY_NETMSG,TEXTMSG_GAME));

	// Lang packs
	bool default_lang=false;
	LangPackVecIt it_l=std::find(LangPacks.begin(),LangPacks.end(),msg_language);
	if(it_l==LangPacks.end())
	{
		default_lang=true;
		it_l=LangPacks.begin();
	}

	LanguagePack& lang=*it_l;
	msg_language=lang.Name;
	cl->LanguageMsg=msg_language;
	for(int i=0;i<TEXTMSG_COUNT;i++)
	{
		if(lang.Msg[i].GetHash()!=textmsg_hash[i]) Send_MsgData(cl,msg_language,i,lang.Msg[i]);
	}

	if(default_lang) cl->Send_TextMsg(cl,STR_NET_LANGUAGE_NOT_SUPPORTED,SAY_NETMSG,TEXTMSG_GAME);

	// Proto item data
	for(int i=0;i<ITEM_MAX_TYPES;i++)
	{
		if(ItemMngr.GetProtosHash(i)!=item_hash[i]) Send_ProtoItemData(cl,i,ItemMngr.GetProtos(i),ItemMngr.GetProtosHash(i));
	}

	// If only cache checking than disconnect
	if(!Singleplayer && !name[0])
	{
		cl->Disconnect();
		return;
	}

	// Singleplayer
	if(Singleplayer && !SingleplayerSave.Valid)
	{
		WriteLogF(_FUNC_," - World not contain singleplayer data.\n");
		cl->Send_TextMsg(cl,STR_SP_SAVE_FAIL,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	// Check for ban by ip
	{
		SCOPE_LOCK(BannedLocker);

		uint ip=cl->GetIp();
		ClientBanned* ban=GetBanByIp(ip);
		if(ban && !Singleplayer)
		{
			cl->Send_TextMsg(cl,STR_NET_BANNED_IP,SAY_NETMSG,TEXTMSG_GAME);
			if(Str::CompareCase(ban->ClientName,cl->Name)) cl->Send_TextMsgLex(cl,STR_NET_BAN_REASON,SAY_NETMSG,TEXTMSG_GAME,ban->GetBanLexems());
			cl->Send_TextMsgLex(cl,STR_NET_TIME_LEFT,SAY_NETMSG,TEXTMSG_GAME,Str::FormatBuf("$time%u",GetBanTime(*ban)));
			cl->Disconnect();
			return;
		}
	}

	// Check login/password
	if(!Singleplayer)
	{
		uint name_len=Str::Length(cl->Name);
		if(name_len<MIN_NAME || name_len<GameOpt.MinNameLength || name_len>MAX_NAME || name_len>GameOpt.MaxNameLength)
		{
			cl->Send_TextMsg(cl,STR_NET_WRONG_LOGIN,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			return;
		}

		uint pass_len=Str::Length(cl->Pass);
		if(pass_len<MIN_NAME || pass_len<GameOpt.MinNameLength || pass_len>MAX_NAME || pass_len>GameOpt.MaxNameLength)
		{
			cl->Send_TextMsg(cl,STR_NET_WRONG_PASS,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			return;
		}

		if(!CheckUserName(cl->Name) || !CheckUserPass(cl->Pass))
		{
			// WriteLogF(_FUNC_," - Wrong chars: Name or Password, client<%s>.\n",cl->Name);
			cl->Send_TextMsg(cl,STR_NET_WRONG_DATA,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			return;
		}
	}

	// Get client account data
	ClientData data;
	if(!Singleplayer)
	{
		SCOPE_LOCK(ClientsDataLocker);

		ClientData* data_=GetClientData(cl->Name);
		if(!data_ || !Str::Compare(cl->Pass,data_->ClientPass))
		{
			// WriteLogF(_FUNC_," - Wrong name<%s> or password.\n",cl->Name);
			cl->Send_TextMsg(cl,STR_NET_LOGINPASS_WRONG,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			return;
		}

		data=*data_;
	}
	else
	{
		Str::Copy(data.ClientName,SingleplayerSave.CrData.Name);
		data.ClientId=SingleplayerSave.CrData.Data.Id;
	}

	// Check for ban by name
	{
		SCOPE_LOCK(BannedLocker);

		ClientBanned* ban=GetBanByName(data.ClientName);
		if(ban && !Singleplayer)
		{
			cl->Send_TextMsg(cl,STR_NET_BANNED,SAY_NETMSG,TEXTMSG_GAME);
			cl->Send_TextMsgLex(cl,STR_NET_BAN_REASON,SAY_NETMSG,TEXTMSG_GAME,ban->GetBanLexems());
			cl->Send_TextMsgLex(cl,STR_NET_TIME_LEFT,SAY_NETMSG,TEXTMSG_GAME,Str::FormatBuf("$time%u",GetBanTime(*ban)));
			cl->Disconnect();
			return;
		}
	}

	// Request script
	bool allow=false;
	uint disallow_msg_num=0,disallow_str_num=0;
	if(Script::PrepareContext(ServerFunctions.PlayerLogin,_FUNC_,data.ClientName))
	{
		CScriptString* name=new CScriptString(data.ClientName);
		CScriptString* pass=new CScriptString(data.ClientPass);
		Script::SetArgUInt(cl->GetIp());
		Script::SetArgObject(name);
		Script::SetArgObject(pass);
		Script::SetArgUInt(data.ClientId);
		Script::SetArgAddress(&disallow_msg_num);
		Script::SetArgAddress(&disallow_str_num);
		if(Script::RunPrepared() && Script::GetReturnedBool()) allow=true;
		name->Release();
		pass->Release();
	}
	if(!allow)
	{
		if(disallow_msg_num<TEXTMSG_COUNT && disallow_str_num)
			cl->Send_TextMsg(cl,disallow_str_num,SAY_NETMSG,disallow_msg_num);
		else
			cl->Send_TextMsg(cl,STR_NET_LOGIN_SCRIPT_FAIL,SAY_NETMSG,TEXTMSG_GAME);
		cl->Disconnect();
		return;
	}

	// Copy data
	uint id=data.ClientId;
	Str::Copy(cl->Name,data.ClientName);
	cl->NameStr=cl->Name;

	// Check UIDS
#ifndef DEV_VESRION
	if(GameOpt.AccountPlayTime && !Singleplayer)
	{
		int uid_zero=0;
		for(int i=0;i<5;i++) if(!uid[i]) uid_zero++;
		if(uid_zero>1)
		{
			WriteLogF(_FUNC_," - Received more than one zeros UIDs, client<%s>.\n",cl->Name);
			cl->Send_TextMsg(cl,STR_NET_UID_FAIL,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			return;
		}

		if((uid[0] && (!FLAG(uid[0],0x00800000) || FLAG(uid[0],0x00000400)))
		|| (uid[1] && (!FLAG(uid[1],0x04000000) || FLAG(uid[1],0x00200000)))
		|| (uid[2] && (!FLAG(uid[2],0x00000020) || FLAG(uid[2],0x00000800)))
		|| (uid[3] && (!FLAG(uid[3],0x80000000) || FLAG(uid[3],0x40000000)))
		|| (uid[4] && (!FLAG(uid[4],0x00000800) || FLAG(uid[4],0x00004000))))
		{
			WriteLogF(_FUNC_," - Invalid UIDs, client<%s>.\n",cl->Name);
			cl->Send_TextMsg(cl,STR_NET_UID_FAIL,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			return;
		}

		uint uidxor_=0xF145668A,uidor_=0,uidcalc_=0x45012345;
		for(int i=0;i<5;i++)
		{
			uidxor_^=uid[i];
			uidor_|=uid[i];
			uidcalc_+=uid[i];
		}

		if(uidxor!=uidxor_ || uidor!=uidor_ || uidcalc!=uidcalc_)
		{
			WriteLogF(_FUNC_," - Invalid UIDs hash, client<%s>.\n",cl->Name);
			cl->Send_TextMsg(cl,STR_NET_UID_FAIL,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			return;
		}

		SCOPE_LOCK(ClientsDataLocker);

		uint tick=Timer::FastTick();
		for(ClientDataVecIt it=ClientsData.begin(),end=ClientsData.end();it!=end;++it)
		{
			ClientData& cd=*it;
			if(cd.ClientId==data.ClientId) continue;

			int matches=0;
			if(!uid[0] || uid[0]==cd.UID[0]) matches++;
			//if(!uid[1] || uid[1]==cd.UID[1]) matches++; // Todo: Disabled, because it's very weak point, but not deleted
			if(!uid[2] || uid[2]==cd.UID[2]) matches++;
			if(!uid[3] || uid[3]==cd.UID[3]) matches++;
			if(!uid[4] || uid[4]==cd.UID[4]) matches++;

			if(matches>=4)
			{
				if((!cd.UIDEndTick || tick>=cd.UIDEndTick) && !CrMngr.GetCritter(cd.ClientId,false))
				{
					for(int i=0;i<5;i++) cd.UID[i]=0;
					cd.UIDEndTick=0;
				}
				else
				{
					WriteLogF(_FUNC_," - UID already used by critter<%s>, client<%s>.\n",cd.ClientName,cl->Name);
					cl->Send_TextMsg(cl,STR_NET_UID_FAIL,SAY_NETMSG,TEXTMSG_GAME);
					cl->Disconnect();
					return;
				}
			}
		}

		for(int i=0;i<5;i++)
		{
			if(data.UID[i]!=uid[i])
			{
				if(!data.UIDEndTick || tick>=data.UIDEndTick)
				{
					// Set new uids on this account and start play timeout
					ClientData* data_=GetClientData(cl->Name);
					for(int i=0;i<5;i++) data_->UID[i]=uid[i];
					data_->UIDEndTick=tick+GameOpt.AccountPlayTime*1000;
				}
				else
				{
					WriteLogF(_FUNC_," - Different UID, client<%s>.\n",cl->Name);
					cl->Send_TextMsg(cl,STR_NET_UID_FAIL,SAY_NETMSG,TEXTMSG_GAME);
					cl->Disconnect();
					return;
				}
				break;
			}
		}
	}
#endif

	// Find in players in game
	Client* cl_old=CrMngr.GetPlayer(id,true);
	if(cl==cl_old)
	{
		WriteLogF(_FUNC_," - Find same ptr, client<%s>.\n",cl->Name);
		cl->Disconnect();
		return;
	}

	// Avatar in game
	if(cl_old)
	{
		ConnectedClientsLocker.Lock();

		// Check founded critter for online
		if(std::find(ConnectedClients.begin(),ConnectedClients.end(),cl_old)!=ConnectedClients.end())
		{
			ConnectedClientsLocker.Unlock();
			cl_old->Send_TextMsg(cl,STR_NET_KNOCK_KNOCK,SAY_NETMSG,TEXTMSG_GAME);
			cl->Send_TextMsg(cl,STR_NET_PLAYER_IN_GAME,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			return;
		}

		// Find current client in online collection
		ClVecIt it=std::find(ConnectedClients.begin(),ConnectedClients.end(),cl);
		if(it==ConnectedClients.end())
		{
			ConnectedClientsLocker.Unlock();
			cl->Disconnect();
			return;
		}

		// Swap
		BIN_END(cl);
		cl->LockNetIO();
		cl_old->LockNetIO();

		cl_old->AddRef();
		(*it)=cl_old;

		std::swap(cl_old->NetIO,cl->NetIO);
		bufferevent_setcb(cl_old->NetIO,NetIO_Input,NetIO_Output,NetIO_Event,cl_old);

		std::swap(cl_old->NetState,cl->NetState);
		cl_old->Sock=cl->Sock;
		cl->Sock=INVALID_SOCKET;

		cl_old->Bin=cl->Bin;
		cl_old->Bout=cl->Bout;

		memcpy(&cl_old->From,&cl->From,sizeof(cl_old->From));
		cl_old->Data.Params[MODE_DEFAULT_COMBAT]=cl->Data.Params[MODE_DEFAULT_COMBAT];
		cl_old->ConnectTime=0;
		UNSETFLAG(cl_old->Flags,FCRIT_DISCONNECT);
		SETFLAG(cl->Flags,FCRIT_DISCONNECT);
		std::swap(cl_old->Zstrm,cl->Zstrm);

		cl->IsNotValid=true;
		cl->IsDisconnected=true;
		cl_old->IsNotValid=false;
		cl_old->IsDisconnected=false;

		cl->UnlockNetIO();

		Job::DeferredRelease(cl);
		cl=cl_old;

		cl->UnlockNetIO();
		ConnectedClientsLocker.Unlock();
		BIN_BEGIN(cl);

		// Erase from save
		EraseSaveClient(cl->GetId());

		cl->SendA_Action(ACTION_CONNECT,0,NULL);
	}
	// Avatar not in game
	else
	{
		cl->Data.Id=id;

		// Singleplayer data
		if(Singleplayer)
		{
			Str::Copy(cl->Name,SingleplayerSave.CrData.Name);
			cl->Data=SingleplayerSave.CrData.Data;
			*cl->GetDataExt()=SingleplayerSave.CrData.DataExt;
			cl->CrTimeEvents=SingleplayerSave.CrData.TimeEvents;
		}

		// Find in saved array
		SaveClientsLocker.Lock();
		Client* cl_saved=NULL;
		for(ClVecIt it=SaveClients.begin(),end=SaveClients.end();it!=end;++it)
		{
			Client* cl_=*it;
			if(cl_->GetId()==id)
			{
				cl_saved=cl_;
				SaveClients.erase(it);
				break;
			}
		}
		SaveClientsLocker.Unlock();

		if(!cl_saved && !LoadClient(cl))
		{
			WriteLogF(_FUNC_," - Error load from data base, client<%s>.\n",cl->GetInfo());
			cl->Send_TextMsg(cl,STR_NET_BD_ERROR,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			cl->Data.Id=0;
			cl->SetMaps(0,0);
			return;
		}

		if(cl_saved)
		{
			// Data
			CritDataExt* data_ext=cl->GetDataExt();
			if(!data_ext)
			{
				WriteLogF(_FUNC_," - Can't allocate extended data, client<%s>.\n",cl->GetInfo());
				cl->Send_TextMsg(cl,STR_NET_SETPROTO_ERR,SAY_NETMSG,TEXTMSG_GAME);
				cl->Disconnect();
				cl->Data.Id=0;
				cl->SetMaps(0,0);
				return;
			}
			memcpy(&cl->Data,&cl_saved->Data,sizeof(cl->Data));
			memcpy(data_ext,cl_saved->GetDataExt(),sizeof(CritDataExt));
			size_t te_count=cl_saved->CrTimeEvents.size();
			if(te_count)
			{
				cl->CrTimeEvents.resize(te_count);
				memcpy(&cl->CrTimeEvents[0],&cl_saved->CrTimeEvents[0],te_count*sizeof(Critter::CrTimeEvent));
			}

			cl_saved->Release();
		}

		// Unarmed
		if(!cl->SetDefaultItems(ItemMngr.GetProtoItem(ITEM_DEF_SLOT),ItemMngr.GetProtoItem(ITEM_DEF_ARMOR)))
		{
			WriteLogF(_FUNC_," - Error set default items, client<%s>.\n",cl->GetInfo());
			cl->Send_TextMsg(cl,STR_NET_SETPROTO_ERR,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			cl->Data.Id=0;
			cl->SetMaps(0,0);
			return;
		}

		// Find items
		ItemMngr.SetCritterItems(cl);

		// Add to map
		Map* map=MapMngr.GetMap(cl->GetMap());
		if(cl->GetMap())
		{
			if(!map || map->IsNoLogOut() || map->GetPid()!=cl->GetProtoMap())
			{
				ushort hx,hy;
				uchar dir;
				if(map && map->GetPid()==cl->GetProtoMap() && map->GetStartCoord(hx,hy,dir,ENTIRE_LOG_OUT))
				{
					cl->Data.HexX=hx;
					cl->Data.HexY=hy;
					cl->Data.Dir=dir;
				}
				else
				{
					map=NULL;
					cl->SetMaps(0,0);
					cl->Data.HexX=0;
					cl->Data.HexY=0;
				}
			}
		}

		if(!cl->GetMap() && (cl->GetHexX() || cl->GetHexY()))
		{
			uint rule_id=(cl->GetHexX()<<16)|cl->GetHexY();
			Critter* rule=CrMngr.GetCritter(rule_id,false);
			if(!rule || rule->GetMap() || cl->Data.GlobalGroupUid!=rule->Data.GlobalGroupUid)
			{
				cl->Data.HexX=0;
				cl->Data.HexY=0;
			}
		}

		if(!MapMngr.AddCrToMap(cl,map,cl->GetHexX(),cl->GetHexY(),1))
		{
			WriteLogF(_FUNC_," - Error add critter to map, client<%s>.\n",cl->GetInfo());
			cl->Send_TextMsg(cl,STR_NET_HEXES_BUSY,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			cl->Data.Id=0;
			cl->SetMaps(0,0);
			return;
		}

		// Map event
		if(map) map->AddCritterEvents(cl);

		// Add car, if on global
		if(!cl->GetMap() && !cl->GroupMove->CarId)
		{
			Item* car=cl->GetItemCar();
			if(car) cl->GroupMove->CarId=car->GetId();
		}

		cl->SetTimeout(TO_TRANSFER,0);
		cl->AddRef();
		CrMngr.AddCritter(cl);
		Job::PushBack(JOB_CRITTER,cl);

		cl->DisableSend++;
		if(cl->GetMap())
		{
			cl->ProcessVisibleCritters();
			cl->ProcessVisibleItems();
		}
		if(Script::PrepareContext(ServerFunctions.CritterInit,_FUNC_,cl->GetInfo()))
		{
			Script::SetArgObject(cl);
			Script::SetArgBool(false);
			Script::RunPrepared();
		}
		if(!cl_old) cl->ParseScript(NULL,false);
		cl->DisableSend--;
	}

	cl->Data.Params[MODE_DEFAULT_COMBAT]=default_combat_mode;
	for(int i=0;i<5;i++) cl->UID[i]=uid[i];

	// Play ip
	CritDataExt* data_ext=cl->GetDataExt();
	uint ip=cl->GetIp();
	bool ip_stored=false;
	for(int i=0;i<MAX_STORED_IP;i++)
	{
		if(data_ext->PlayIp[i]==ip)
		{
			ip_stored=true;
			break;
		}
	}
	if(!ip_stored)
	{
		// Check free slots
		if(data_ext->PlayIp[MAX_STORED_IP-1])
		{
			// Free 1/2 slots
			for(int i=MAX_STORED_IP/2;i<MAX_STORED_IP;i++)
				data_ext->PlayIp[i]=0;
		}

		// Store ip
		for(int i=0;i<MAX_STORED_IP;i++)
		{
			if(!data_ext->PlayIp[i])
			{
				data_ext->PlayIp[i]=ip;
				break;
			}
		}
	}

	// Login ok
	uint bin_seed=Random(100000,2000000000);
	uint bout_seed=Random(100000,2000000000);
	BOUT_BEGIN(cl);
	cl->Bout << NETMSG_LOGIN_SUCCESS;
	cl->Bout << bin_seed;
	cl->Bout << bout_seed;
	BOUT_END(cl);
	cl->Bin.SetEncryptKey(bin_seed);
	cl->Bout.SetEncryptKey(bout_seed);

	InterlockedExchange(&cl->NetState,STATE_LOGINOK);
	cl->Send_LoadMap(NULL);
}

void FOServer::Process_SingleplayerSaveLoad(Client* cl)
{
	if(!Singleplayer)
	{
		cl->Disconnect();
		return;
	}

	uint msg_len;
	bool save;
	ushort fname_len;
	char fname[MAX_FOTEXT];
	UCharVec pic_data;
	cl->Bin >> msg_len;
	cl->Bin >> save;
	cl->Bin >> fname_len;
	cl->Bin.Pop(fname,fname_len);
	fname[fname_len]=0;
	if(save)
	{
		uint pic_data_len;
		cl->Bin >> pic_data_len;
		pic_data.resize(pic_data_len);
		if(pic_data_len) cl->Bin.Pop((char*)&pic_data[0],pic_data_len);
	}

	CHECK_IN_BUFF_ERROR(cl);

	if(save)
	{
		ClientsSaveDataCount=0;
		AddClientSaveData(cl);
		SingleplayerSave.CrData=ClientsSaveData[0];
		SingleplayerSave.PicData=pic_data;

		SaveWorld(fname);
		cl->Send_TextMsg(cl,STR_SP_SAVE_SUCCESS,SAY_NETMSG,TEXTMSG_GAME);
	}
	else
	{
		SynchronizeLogicThreads();
		SyncManager::GetForCurThread()->UnlockAll();
		SYNC_LOCK(cl);
		if(!LoadWorld(fname))
		{
			WriteLogF(_FUNC_," - Unable load world from file<%s>.\n",fname);
			cl->Send_TextMsg(cl,STR_SP_LOAD_FAIL,SAY_NETMSG,TEXTMSG_GAME);
			cl->Disconnect();
			ResynchronizeLogicThreads();
			return;
		}
		ResynchronizeLogicThreads();

		cl->Send_TextMsg(cl,STR_SP_LOAD_SUCCESS,SAY_NETMSG,TEXTMSG_GAME);

		BOUT_BEGIN(cl);
		cl->Bout << (uint)NETMSG_REGISTER_SUCCESS;
		BOUT_END(cl);
		cl->Disconnect();
	}
}

void FOServer::Process_ParseToGame(Client* cl)
{
	if(!cl->GetId() || !CrMngr.GetPlayer(cl->GetId(),false)) return;
	cl->SetBreakTime(GameOpt.Breaktime);

#ifdef DEV_VESRION
	cl->Access=ACCESS_IMPLEMENTOR;
#endif

	InterlockedExchange(&cl->NetState,STATE_GAME);
	cl->PingOk(PING_CLIENT_LIFE_TIME*5);

	if(cl->ViewMapId)
	{
		Map* map=MapMngr.GetMap(cl->ViewMapId);
		cl->ViewMapId=0;
		if(map)
		{
			cl->ViewMap(map,cl->ViewMapLook,cl->ViewMapHx,cl->ViewMapHy,cl->ViewMapDir);
			cl->Send_ViewMap();
			return;
		}
	}

	// Parse
	Map* map=MapMngr.GetMap(cl->GetMap(),true);
	cl->Send_GameInfo(map);
	cl->DropTimers(true);

	// Parse to global
	if(!cl->GetMap())
	{
		if(!cl->GroupMove)
		{
			WriteLogF(_FUNC_," - Group nullptr, client<%s>.\n",cl->GetInfo());
			cl->Disconnect();
			return;
		}

		cl->Send_GlobalInfo(GM_INFO_ALL);
		for(CrVecIt it=cl->GroupMove->CritMove.begin(),end=cl->GroupMove->CritMove.end();it!=end;++it)
		{
			Critter* cr=*it;
			if(cr!=cl) cr->Send_CritterParam(cl,OTHER_FLAGS,cl->Flags);
		}
		cl->Send_AllQuests();
		cl->Send_HoloInfo(true,0,cl->Data.HoloInfoCount);
		cl->Send_AllAutomapsInfo();
		if(cl->Talk.TalkType!=TALK_NONE)
		{
			cl->ProcessTalk(true);
			cl->Send_Talk();
		}
		return;
	}

	if(!map)
	{
		WriteLogF(_FUNC_," - Map not found, client<%s>.\n",cl->GetInfo());
		cl->Disconnect();
		return;
	}

	// Parse to local
	SETFLAG(cl->Flags,FCRIT_CHOSEN);
	cl->Send_AddCritter(cl);
	UNSETFLAG(cl->Flags,FCRIT_CHOSEN);

	// Send all data
#pragma MESSAGE("Send all data by demand.")
	cl->Send_AllParams();
	cl->Send_AddAllItems();
	cl->Send_AllQuests();
	cl->Send_HoloInfo(true,0,cl->Data.HoloInfoCount);
	cl->Send_AllAutomapsInfo();

	// Send current critters
	CrVec critters=cl->VisCrSelf;
	for(CrVecIt it=critters.begin(),end=critters.end();it!=end;++it)
	{
		Critter* cr=*it;
		SYNC_LOCK(cr);
		cl->Send_AddCritter(cr);
	}

	// Send current items on map
	cl->VisItemLocker.Lock();
	UIntSet items=cl->VisItem;
	cl->VisItemLocker.Unlock();
	for(UIntSetIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=ItemMngr.GetItem(*it,false);
		if(item) cl->Send_AddItemOnMap(item);
	}

	// Check active talk
	if(cl->Talk.TalkType!=TALK_NONE)
	{
		cl->ProcessTalk(true);
		cl->Send_Talk();
	}

	// Turn based
	if(map->IsTurnBasedOn)
	{
		if(map->IsCritterTurn(cl)) cl->Send_ParamOther(OTHER_YOU_TURN,map->GetCritterTurnTime());
		else
		{
			Critter* cr=cl->GetCritSelf(map->GetCritterTurnId(),false);
			if(cr) cl->Send_CritterParam(cr,OTHER_YOU_TURN,map->GetCritterTurnTime());
		}
	}
	else if(TB_BATTLE_TIMEOUT_CHECK(cl->GetParam(TO_BATTLE))) cl->SetTimeout(TO_BATTLE,0);
}

void FOServer::Process_GiveMap(Client* cl)
{
	bool automap;
	ushort map_pid;
	uint loc_id;
	uint hash_tiles;
	uint hash_walls;
	uint hash_scen;

	cl->Bin >> automap;
	cl->Bin >> map_pid;
	cl->Bin >> loc_id;
	cl->Bin >> hash_tiles;
	cl->Bin >> hash_walls;
	cl->Bin >> hash_scen;
	CHECK_IN_BUFF_ERROR(cl);

	if(!Singleplayer)
	{
		uint tick=Timer::FastTick();
		if(tick-cl->LastSendedMapTick<GameOpt.Breaktime*3) cl->SetBreakTime(GameOpt.Breaktime*3);
		else cl->SetBreakTime(GameOpt.Breaktime);
		cl->LastSendedMapTick=tick;
	}

	ProtoMap* pmap=MapMngr.GetProtoMap(map_pid);
	if(!pmap)
	{
		WriteLogF(_FUNC_," - Map prototype not found, client<%s>.\n",cl->GetInfo());
		cl->Disconnect();
		return;
	}

	if(!automap && map_pid!=cl->GetProtoMap() && cl->ViewMapPid!=map_pid)
	{
		WriteLogF(_FUNC_," - Request for loading incorrect map, client<%s>.\n",cl->GetInfo());
		return;
	}

	if(automap)
	{
		if(!cl->CheckKnownLocById(loc_id))
		{
			WriteLogF(_FUNC_," - Request for loading unknown automap, client<%s>.\n",cl->GetInfo());
			return;
		}

		Location* loc=MapMngr.GetLocation(loc_id);
		if(!loc || !loc->IsAutomap(map_pid))
		{
			WriteLogF(_FUNC_," - Request for loading incorrect automap, client<%s>.\n",cl->GetInfo());
			return;
		}
	}

	uchar send_info=0;
	if(pmap->HashTiles!=hash_tiles) SETFLAG(send_info,SENDMAP_TILES);
	if(pmap->HashWalls!=hash_walls) SETFLAG(send_info,SENDMAP_WALLS);
	if(pmap->HashScen!=hash_scen) SETFLAG(send_info,SENDMAP_SCENERY);

	Send_MapData(cl,pmap,send_info);

	if(!automap)
	{
		Map* map=NULL;
		if(cl->ViewMapId) map=MapMngr.GetMap(cl->ViewMapId);
		cl->Send_LoadMap(map);
	}
}

void FOServer::Send_MapData(Client* cl, ProtoMap* pmap, uchar send_info)
{
	uint msg=NETMSG_MAP;
	ushort map_pid=pmap->GetPid();
	ushort maxhx=pmap->Header.MaxHexX;
	ushort maxhy=pmap->Header.MaxHexY;
	uint msg_len=sizeof(msg)+sizeof(msg_len)+sizeof(map_pid)+sizeof(maxhx)+sizeof(maxhy)+sizeof(send_info);

	if(FLAG(send_info,SENDMAP_TILES))
		msg_len+=sizeof(uint)+pmap->Tiles.size()*sizeof(ProtoMap::Tile);
	if(FLAG(send_info,SENDMAP_WALLS))
		msg_len+=sizeof(uint)+pmap->WallsToSend.size()*sizeof(SceneryCl);
	if(FLAG(send_info,SENDMAP_SCENERY))
		msg_len+=sizeof(uint)+pmap->SceneriesToSend.size()*sizeof(SceneryCl);

	// Header
	BOUT_BEGIN(cl);
	cl->Bout << msg;
	cl->Bout << msg_len;
	cl->Bout << map_pid;
	cl->Bout << maxhx;
	cl->Bout << maxhy;
	cl->Bout << send_info;

	// Tiles
	if(FLAG(send_info,SENDMAP_TILES))
	{
		cl->Bout << (uint)pmap->Tiles.size();
		if(pmap->Tiles.size())
			cl->Bout.Push((char*)&pmap->Tiles[0],pmap->Tiles.size()*sizeof(ProtoMap::Tile));
	}

	// Walls
	if(FLAG(send_info,SENDMAP_WALLS))
	{
		cl->Bout << (uint)pmap->WallsToSend.size();
		if(pmap->WallsToSend.size())
			cl->Bout.Push((char*)&pmap->WallsToSend[0],pmap->WallsToSend.size()*sizeof(SceneryCl));
	}

	// Scenery
	if(FLAG(send_info,SENDMAP_SCENERY))
	{
		cl->Bout << (uint)pmap->SceneriesToSend.size();
		if(pmap->SceneriesToSend.size())
			cl->Bout.Push((char*)&pmap->SceneriesToSend[0],pmap->SceneriesToSend.size()*sizeof(SceneryCl));
	}
	BOUT_END(cl);
}

void FOServer::Process_Move(Client* cl)
{
	uint move_params;
	ushort hx;
	ushort hy;

	cl->Bin >> move_params;
	cl->Bin >> hx;
	cl->Bin >> hy;
	CHECK_IN_BUFF_ERROR(cl);

	if(!cl->GetMap()) return;

	// The player informs that has stopped
	if(!FLAG(move_params,MOVE_PARAM_STEP_ALLOW))
	{
		//cl->Send_XY(cl);
		cl->SendA_XY();
		return;
	}

	// Check running availability
	bool is_run=FLAG(move_params,MOVE_PARAM_RUN);
	if(is_run)
	{
		if(!cl->IsCanRun() ||
			(GameOpt.RunOnCombat?0:cl->GetParam(TO_BATTLE)) ||
			(GameOpt.RunOnTransfer?0:cl->GetParam(TO_TRANSFER)))
		{
			// Switch to walk
			move_params^=MOVE_PARAM_RUN;
			is_run=false;
		}
	}

	// Check walking availability
	if(!is_run)
	{
		if(!cl->IsCanWalk())
		{
			cl->Send_XY(cl);
			return;
		}
	}

	// Overweight
	uint cur_weight=cl->GetItemsWeight();
	uint max_weight=cl->GetParam(ST_CARRY_WEIGHT);
	if(cur_weight>max_weight)
	{
		if(is_run || cur_weight>max_weight*2)
		{
			cl->Send_XY(cl);
			return;
		}
	}

	// Lame
	if(cl->IsDmgLeg() && is_run)
	{
		cl->Send_XY(cl);
		return;
	}

	// Check dist
	if(!CheckDist(cl->GetHexX(),cl->GetHexY(),hx,hy,1))
	{
		cl->Send_XY(cl);
		return;
	}

	// Try move
	Act_Move(cl,hx,hy,move_params);
}

void FOServer::Process_ChangeItem(Client* cl)
{
	uint item_id;
	uchar from_slot;
	uchar to_slot;
	uint count;

	cl->Bin >> item_id;
	cl->Bin >> from_slot;
	cl->Bin >> to_slot;
	cl->Bin >> count;
	CHECK_IN_BUFF_ERROR(cl);

	cl->SetBreakTime(GameOpt.Breaktime);

	if(!cl->CheckMyTurn(NULL))
	{
		WriteLogF(_FUNC_," - Is not client<%s> turn.\n",cl->GetInfo());
		cl->Send_Param(ST_CURRENT_AP);
		cl->Send_AddAllItems();
		return;
	}

	bool is_castling=((from_slot==SLOT_HAND1 && to_slot==SLOT_HAND2) || (from_slot==SLOT_HAND2 && to_slot==SLOT_HAND1));
	int ap_cost=(is_castling?0:cl->GetApCostMoveItemInventory());
	if(to_slot==0xFF) ap_cost=cl->GetApCostDropItem();
	if(cl->GetParam(ST_CURRENT_AP)<ap_cost && !Singleplayer)
	{
		WriteLogF(_FUNC_," - Not enough AP, client<%s>.\n",cl->GetInfo());
		cl->Send_Param(ST_CURRENT_AP);
		cl->Send_AddAllItems();
		return;
	}
	cl->SubAp(ap_cost);
	//cl->AccessContainerId=cl->GetId();

	Map* map=MapMngr.GetMap(cl->GetMap());
	if(map && map->IsTurnBasedOn && !cl->GetAllAp()) map->EndCritterTurn();

	// Move
	if(!cl->MoveItem(from_slot,to_slot,item_id,count))
	{
		WriteLogF(_FUNC_," - Move item fail, from<%u>, to<%u>, item_id<%u>, client<%s>.\n",from_slot,to_slot,item_id,cl->GetInfo());
		cl->Send_Param(ST_CURRENT_AP);
		cl->Send_AddAllItems();
	}
}

void FOServer::Process_RateItem(Client* cl)
{
	uint rate;
	cl->Bin >> rate;

	cl->ItemSlotMain->SetMode(rate&0xFF);
	if(!cl->ItemSlotMain->GetId())
	{
		cl->ChangeParam(ST_HANDS_ITEM_AND_MODE);
		cl->Data.Params[ST_HANDS_ITEM_AND_MODE]=rate;
	}
}

void FOServer::Process_SortValueItem(Client* cl)
{
	uint item_id;
	ushort sort_val;

	cl->Bin >> item_id;
	cl->Bin >> sort_val;
	CHECK_IN_BUFF_ERROR(cl);

	Item* item=cl->GetItem(item_id,true);
	if(!item)
	{
		WriteLogF(_FUNC_," - Item not found, client<%s>, item id<%u>.\n",cl->GetInfo(),item_id);
		return;
	}

	item->Data.SortValue=sort_val;
}

void FOServer::Process_UseItem(Client* cl)
{
	uint item_id;
	ushort item_pid;
	uchar rate;
	uchar target_type;
	uint target_id;
	ushort target_pid;
	uint param;

	cl->Bin >> item_id;
	cl->Bin >> item_pid;
	cl->Bin >> rate;
	cl->Bin >> target_type;
	cl->Bin >> target_id;
	cl->Bin >> target_pid;
	cl->Bin >> param;
	CHECK_IN_BUFF_ERROR(cl);

	if(!cl->IsLife()) return;

	uchar use=rate&0xF;
	Item* item=(item_id?cl->GetItem(item_id,true):cl->ItemSlotMain);
	if(!item)
	{
		WriteLogF(_FUNC_," - Item not found, item id<%u>, client<%s>.\n",item_id,cl->GetInfo());
		return;
	}

	if(item->IsWeapon() && use!=USE_USE)
	{
		switch(use)
		{
		case USE_PRIMARY:
		case USE_SECONDARY:
		case USE_THIRD:
			if(!cl->GetMap()) break;
			if(item!=cl->ItemSlotMain) break;

			// Unarmed
			if(!item->GetId())
			{
				if(use!=USE_PRIMARY) break;
				ProtoItem* unarmed=ItemMngr.GetProtoItem(item_pid);
				if(!unarmed || !unarmed->IsWeapon() || !unarmed->Weapon_IsUnarmed) break;
				if(cl->GetParam(ST_STRENGTH)<unarmed->Weapon_MinStrength || cl->GetParam(ST_AGILITY)<unarmed->Weapon_UnarmedMinAgility) break;
				if(cl->Data.Params[ST_LEVEL]<unarmed->Weapon_UnarmedMinLevel || cl->GetRawParam(SK_UNARMED)<unarmed->Weapon_UnarmedMinUnarmed) break;
				cl->ItemSlotMain->Init(unarmed);
			}

			Act_Attack(cl,rate,target_id);
			break;
		case USE_RELOAD:
			Act_Reload(cl,item->GetId(),target_id);
			break;
		default:
			cl->Send_TextMsg(cl,STR_USE_NOTHING,SAY_NETMSG,TEXTMSG_GAME);
			break;
		}
	}
	else if(use==USE_USE)
	{
		if(!item_id) return;
		if(target_type!=TARGET_CRITTER && target_type!=TARGET_ITEM && target_type!=TARGET_SCENERY &&
			target_type!=TARGET_SELF && target_type!=TARGET_SELF_ITEM) return;
		if(!cl->GetMap() && (target_type==TARGET_ITEM || target_type==TARGET_SCENERY)) return;
		if(!Act_Use(cl,item_id,-1,target_type,target_id,target_pid,param)) cl->Send_TextMsg(cl,STR_USE_NOTHING,SAY_NETMSG,TEXTMSG_GAME);
	}
	else
	{
		cl->Send_TextMsg(cl,STR_USE_NOTHING,SAY_NETMSG,TEXTMSG_GAME);
	}
}

void FOServer::Process_PickItem(Client* cl)
{
	ushort targ_x;
	ushort targ_y;
	ushort pid;

	cl->Bin >> targ_x;
	cl->Bin >> targ_y;
	cl->Bin >> pid;
	CHECK_IN_BUFF_ERROR(cl);

	Act_PickItem(cl,targ_x,targ_y,pid);
}

void FOServer::Process_PickCritter(Client* cl)
{
	uint crid;
	uchar pick_type;

	cl->Bin >> crid;
	cl->Bin >> pick_type;
	CHECK_IN_BUFF_ERROR(cl);

	cl->SetBreakTime(GameOpt.Breaktime);

	if(!cl->CheckMyTurn(NULL))
	{
		WriteLogF(_FUNC_," - Is not critter<%s> turn.\n",cl->GetInfo());
		cl->Send_Param(ST_CURRENT_AP);
		return;
	}

	int ap_cost=cl->GetApCostPickCritter();
	if(cl->GetParam(ST_CURRENT_AP)<ap_cost && !Singleplayer)
	{
		WriteLogF(_FUNC_," - Not enough AP, critter<%s>.\n",cl->GetInfo());
		cl->Send_Param(ST_CURRENT_AP);
		return;
	}
	cl->SubAp(ap_cost);

	Map* map=MapMngr.GetMap(cl->GetMap());
	if(map && map->IsTurnBasedOn && !cl->GetAllAp()) map->EndCritterTurn();

	Critter* cr=cl->GetCritSelf(crid,true);
	if(!cr) return;

	switch(pick_type)
	{
	case PICK_CRIT_LOOT:
		if(!cr->IsDead()) break;
		if(cr->IsRawParam(MODE_NO_LOOT)) break;
		if(!CheckDist(cl->GetHexX(),cl->GetHexY(),cr->GetHexX(),cr->GetHexY(),cl->GetUseDist()+cr->GetMultihex()))
		{
			cl->Send_XY(cl);
			cl->Send_XY(cr);
			break;
		}

		// Script events
		if(cl->EventUseSkill(SKILL_LOOT_CRITTER,cr,NULL,NULL)) return;
		if(cr->EventUseSkillOnMe(cl,SKILL_LOOT_CRITTER)) return;
		if(Script::PrepareContext(ServerFunctions.CritterUseSkill,_FUNC_,cl->GetInfo()))
		{
			Script::SetArgObject(cl);
			Script::SetArgUInt(SKILL_LOOT_CRITTER);
			Script::SetArgObject(cr);
			Script::SetArgObject(NULL);
			Script::SetArgObject(NULL);
			if(Script::RunPrepared() && Script::GetReturnedBool()) return;
		}

		// Default process
		cl->Send_ContainerInfo(cr,TRANSFER_CRIT_LOOT,true);
		break;
	case PICK_CRIT_PUSH:
		if(!cr->IsLife()) break;
		if(cr->IsRawParam(MODE_NO_PUSH)) break;
		if(!CheckDist(cl->GetHexX(),cl->GetHexY(),cr->GetHexX(),cr->GetHexY(),cl->GetUseDist()+cr->GetMultihex()))
		{
			cl->Send_XY(cl);
			cl->Send_XY(cr);
			break;
		}

		// Script events
		if(cl->EventUseSkill(SKILL_PUSH_CRITTER,cr,NULL,NULL)) return;
		if(cr->EventUseSkillOnMe(cl,SKILL_PUSH_CRITTER)) return;
		if(Script::PrepareContext(ServerFunctions.CritterUseSkill,_FUNC_,cl->GetInfo()))
		{
			Script::SetArgObject(cl);
			Script::SetArgUInt(SKILL_PUSH_CRITTER);
			Script::SetArgObject(cr);
			Script::SetArgObject(NULL);
			Script::SetArgObject(NULL);
			if(Script::RunPrepared() && Script::GetReturnedBool()) return;
		}
		break;
	default:
		break;
	}
}

void FOServer::Process_ContainerItem(Client* cl)
{
	uchar transfer_type;
	uint cont_id;
	uint item_id;
	uint item_count;
	uchar take_flags;

	cl->Bin >> transfer_type;
	cl->Bin >> cont_id;
	cl->Bin >> item_id;
	cl->Bin >> item_count;
	cl->Bin >> take_flags;
	CHECK_IN_BUFF_ERROR(cl);

	cl->SetBreakTime(GameOpt.Breaktime);

	if(!cl->CheckMyTurn(NULL))
	{
		WriteLogF(_FUNC_," - Is not client<%s> turn.\n",cl->GetInfo());
		cl->Send_Param(ST_CURRENT_AP);
		return;
	}

	if(cl->AccessContainerId!=cont_id)
	{
		WriteLogF(_FUNC_," - Try work with not accessed container, client<%s>.\n",cl->GetInfo());
		cl->Send_ContainerInfo();
		cl->Send_Param(ST_CURRENT_AP);
		return;
	}
	cl->ItemTransferCount=item_count;

	int ap_cost=cl->GetApCostMoveItemContainer();
	if(cl->GetParam(ST_CURRENT_AP)<ap_cost && !Singleplayer)
	{
		WriteLogF(_FUNC_," - Not enough AP, client<%s>.\n",cl->GetInfo());
		cl->Send_ContainerInfo();
		cl->Send_Param(ST_CURRENT_AP);
		return;
	}
	cl->SubAp(ap_cost);

	Map* map=MapMngr.GetMap(cl->GetMap());
	if(map && map->IsTurnBasedOn && !cl->GetAllAp()) map->EndCritterTurn();

	if(!cl->GetMap() && (transfer_type!=TRANSFER_CRIT_STEAL && transfer_type!=TRANSFER_FAR_CONT && transfer_type!=TRANSFER_FAR_CRIT
		&& transfer_type!=TRANSFER_CRIT_BARTER && transfer_type!=TRANSFER_SELF_CONT)) return;

/************************************************************************/
/* Item container                                                       */
/************************************************************************/
	if(transfer_type==TRANSFER_HEX_CONT_UP || transfer_type==TRANSFER_HEX_CONT_DOWN || transfer_type==TRANSFER_SELF_CONT || transfer_type==TRANSFER_FAR_CONT)
	{
		Item* cont;

		// Check and get hex cont
		if(transfer_type==TRANSFER_HEX_CONT_UP || transfer_type==TRANSFER_HEX_CONT_DOWN)
		{
			// Get item
			cont=ItemMngr.GetItem(cont_id,true);
			if(!cont || cont->Accessory!=ITEM_ACCESSORY_HEX || !cont->IsContainer())
			{
				cl->Send_ContainerInfo();
				WriteLogF(_FUNC_," - TRANSFER_HEX_CONT error.\n");
				return;
			}

			// Check map
			if(cont->ACC_HEX.MapId!=cl->GetMap())
			{
				cl->Send_ContainerInfo();
				WriteLogF(_FUNC_," - Attempt to take a subject from the container on other map.\n");
				return;
			}

			// Check dist
			if(!CheckDist(cl->GetHexX(),cl->GetHexY(),cont->ACC_HEX.HexX,cont->ACC_HEX.HexY,cl->GetUseDist()))
			{
				cl->Send_XY(cl);
				cl->Send_ContainerInfo();
				WriteLogF(_FUNC_," - Transfer item container. Client<%s> distance more than allowed.\n",cl->GetInfo());
				return;
			}

			// Check for close
			if(cont->Proto->Container_Changeble && !cont->LockerIsOpen())
			{
				cl->Send_ContainerInfo();
				WriteLogF(_FUNC_," - Container is not open.\n");
				return;
			}
		}
		// Get far container without checks
		else if(transfer_type==TRANSFER_FAR_CONT)
		{
			cont=ItemMngr.GetItem(cont_id,true);
			if(!cont || !cont->IsContainer())
			{
				cl->Send_ContainerInfo();
				WriteLogF(_FUNC_," - TRANSFER_FAR_CONT error.\n");
				return;
			}
		}
		// Check and get self cont
		else
		{
			// Get item
			cont=cl->GetItem(cont_id,true);
			if(!cont || !cont->IsContainer())
			{
				cl->Send_ContainerInfo();
				WriteLogF(_FUNC_," - TRANSFER_SELF_CONT error2.\n");
				return;
			}
		}

		// Process
		switch(take_flags)
		{
		case CONT_GET:
			{
				// Get item
				Item* item=cont->ContGetItem(item_id,true);
				if(!item)
				{
					cl->Send_ContainerInfo(cont,transfer_type,false);
					cl->Send_TextMsg(cl,STR_ITEM_NOT_FOUND,SAY_NETMSG,TEXTMSG_GAME);
					return;
				}

				// Send
				cl->SendAA_Action(ACTION_OPERATE_CONTAINER,transfer_type*10+0,item);

				// Check count
				if(!item_count || item->GetCount()<item_count)
				{
					cl->Send_ContainerInfo();
					cl->Send_Text(cl,"Error count.",SAY_NETMSG);
					return;
				}

				// Check weight
				if(cl->GetFreeWeight()<(int)(item->GetWeight1st()*item_count))
				{
					cl->Send_TextMsg(cl,STR_OVERWEIGHT,SAY_NETMSG,TEXTMSG_GAME);
					break;
				}

				// Check volume
				if(cl->GetFreeVolume()<(int)(item->GetVolume1st()*item_count))
				{
					cl->Send_TextMsg(cl,STR_OVERVOLUME,SAY_NETMSG,TEXTMSG_GAME);
					break;
				}

				// Script events
				if(item->EventSkill(cl,SKILL_TAKE_CONT)) return;
				if(cl->EventUseSkill(SKILL_TAKE_CONT,NULL,item,NULL)) return;
				if(Script::PrepareContext(ServerFunctions.CritterUseSkill,_FUNC_,cl->GetInfo()))
				{
					Script::SetArgObject(cl);
					Script::SetArgUInt(SKILL_TAKE_CONT);
					Script::SetArgObject(NULL);
					Script::SetArgObject(item);
					Script::SetArgObject(NULL);
					if(Script::RunPrepared() && Script::GetReturnedBool()) return;
				}

				// Transfer
				if(!ItemMngr.MoveItemCritterFromCont(cont,cl,item->GetId(),item_count))
					WriteLogF(_FUNC_," - Transfer item, from container to player (get), fail.\n");
			}
			break;
		case CONT_GETALL:
			{
				// Send
				cl->SendAA_Action(ACTION_OPERATE_CONTAINER,transfer_type*10+1,NULL);

				// Get items
				ItemPtrVec items;
				cont->ContGetAllItems(items,true,true);
				if(items.empty())
				{
					cl->Send_ContainerInfo();
					return;
				}

				// Check weight, volume
				uint weight=0,volume=0;
				for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
				{
					Item* item=*it;
					weight+=item->GetWeight();
					volume+=item->GetVolume();
				}

				if(cl->GetFreeWeight()<(int)weight)
				{
					cl->Send_TextMsg(cl,STR_OVERWEIGHT,SAY_NETMSG,TEXTMSG_GAME);
					break;
				}

				if(cl->GetFreeVolume()<(int)volume)
				{
					cl->Send_TextMsg(cl,STR_OVERVOLUME,SAY_NETMSG,TEXTMSG_GAME);
					break;
				}

				// Script events
				if(cont->EventSkill(cl,SKILL_TAKE_ALL_CONT)) return;
				if(cl->EventUseSkill(SKILL_TAKE_ALL_CONT,NULL,cont,NULL)) return;
				if(Script::PrepareContext(ServerFunctions.CritterUseSkill,_FUNC_,cl->GetInfo()))
				{
					Script::SetArgObject(cl);
					Script::SetArgUInt(SKILL_TAKE_ALL_CONT);
					Script::SetArgObject(NULL);
					Script::SetArgObject(cont);
					Script::SetArgObject(NULL);
					if(Script::RunPrepared() && Script::GetReturnedBool()) return;
				}

				// Transfer
				for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
				{
					Item* item=*it;
					if(!item->EventSkill(cl,SKILL_TAKE_CONT))
					{
						cont->ContEraseItem(item);
						cl->AddItem(item,true);
					}
				}
			}
			break;
		case CONT_PUT:
			{
				// Get item
				Item* item=cl->GetItem(item_id,true);
				if(!item || item->ACC_CRITTER.Slot!=SLOT_INV)
				{
					cl->Send_ContainerInfo();
					cl->Send_TextMsg(cl,STR_ITEM_NOT_FOUND,SAY_NETMSG,TEXTMSG_GAME);
					return;
				}

				// Send
				cl->SendAA_Action(ACTION_OPERATE_CONTAINER,transfer_type*10+2,item);

				// Check count
				if(!item_count || item->GetCount()<item_count)
				{
					cl->Send_ContainerInfo();
					cl->Send_Text(cl,"Error count.",SAY_NETMSG);
					return;
				}

				// Check slot
				if(item->ACC_CRITTER.Slot!=SLOT_INV)
				{
					cl->Send_ContainerInfo();
					cl->Send_Text(cl,"Cheat detected.",SAY_NETMSG);
					WriteLogF(_FUNC_," - Attempting to put in a container not from the inventory, client<%s>.",cl->GetInfo());
					return;
				}

				// Check volume
				if(cont->ContGetFreeVolume(0)<(int)(item->GetVolume1st()*item_count))
				{
					cl->Send_TextMsg(cl,STR_OVERVOLUME,SAY_NETMSG,TEXTMSG_GAME);
					break;
				}

				// Script events
				if(item->EventSkill(cl,SKILL_PUT_CONT)) return;
				if(cl->EventUseSkill(SKILL_PUT_CONT,NULL,item,NULL)) return;
				if(Script::PrepareContext(ServerFunctions.CritterUseSkill,_FUNC_,cl->GetInfo()))
				{
					Script::SetArgObject(cl);
					Script::SetArgUInt(SKILL_PUT_CONT);
					Script::SetArgObject(NULL);
					Script::SetArgObject(item);
					Script::SetArgObject(NULL);
					if(Script::RunPrepared() && Script::GetReturnedBool()) return;
				}

				// Transfer
				if(!ItemMngr.MoveItemCritterToCont(cl,cont,item->GetId(),item_count,0))
					WriteLogF(_FUNC_," - Transfer item, from player to container (put), fail.\n");
			}
			break;
		case CONT_PUTALL:
			{
				cl->Send_ContainerInfo();
			}
			break;
	//	case CONT_UNLOAD:
	//		{
	//			//TODO:
	//		}
	//		break;
		default:
			break;
		}

		cl->Send_ContainerInfo(cont,transfer_type,false);

	}
/************************************************************************/
/* Critter container                                                    */
/************************************************************************/
	else if(transfer_type==TRANSFER_CRIT_LOOT || transfer_type==TRANSFER_CRIT_STEAL || transfer_type==TRANSFER_FAR_CRIT)
	{
		bool is_far=(transfer_type==TRANSFER_FAR_CRIT);
		bool is_steal=(transfer_type==TRANSFER_CRIT_STEAL);
		bool is_loot=(transfer_type==TRANSFER_CRIT_LOOT);

		// Get critter target
		Critter* cr=(is_far?CrMngr.GetCritter(cont_id,true):(cl->GetMap()?cl->GetCritSelf(cont_id,true):cl->GroupMove->GetCritter(cont_id)));
		if(!cr)
		{
			cl->Send_ContainerInfo();
			WriteLogF(_FUNC_," - Critter not found.\n");
			return;
		}
		SYNC_LOCK(cr);

		// Check for self
		if(cl==cr)
		{
			cl->Send_ContainerInfo();
			WriteLogF(_FUNC_," - Critter<%s> self pick.\n",cl->GetInfo());
			return;
		}

		// Check NoSteal flag
		if(is_steal && cr->IsRawParam(MODE_NO_STEAL))
		{
			cl->Send_ContainerInfo();
			WriteLogF(_FUNC_," - Critter has NoSteal flag, critter<%s>.\n",cl->GetInfo());
			return;
		}

		// Check dist
		if(!is_far && !CheckDist(cl->GetHexX(),cl->GetHexY(),cr->GetHexX(),cr->GetHexY(),cl->GetUseDist()+cr->GetMultihex()))
		{
			cl->Send_XY(cl);
			cl->Send_XY(cr);
			cl->Send_ContainerInfo();
			WriteLogF(_FUNC_," - Transfer critter container. Client<%s> distance more than allowed.\n",cl->GetInfo());
			return;
		}

		// Check loot valid
		if(is_loot && !cr->IsDead())
		{
			cl->Send_ContainerInfo();
			WriteLogF(_FUNC_," - TRANSFER_CRIT_LOOT - Critter not dead.\n");
			return;
		}

		// Check steal valid
		if(is_steal)
		{
			// Player is offline
			/*if(cr->IsPlayer() && cr->State!=STATE_GAME)
			{
				cr->SendA_Text(&cr->VisCr,"Please, don't kill me",SAY_WHISP_ON_HEAD);
				cl->Send_ContainerInfo();
				WriteLog("Process_ContainerItem - TRANSFER_CRIT_STEAL - Critter not in game\n");
				return;
			}*/

			// Npc in battle
			if(cr->IsNpc() && ((Npc*)cr)->IsCurPlane(AI_PLANE_ATTACK))
			{
				cl->Send_ContainerInfo();
				return;
			}
		}

		// Process
		switch(take_flags)
		{
		case CONT_GET:
			{
				// Get item
				Item* item=cr->GetInvItem(item_id,transfer_type);
				if(!item)
				{
					cl->Send_ContainerInfo();
					cl->Send_TextMsg(cl,STR_ITEM_NOT_FOUND,SAY_NETMSG,TEXTMSG_GAME);
					return;
				}

				// Send
				cl->SendAA_Action(ACTION_OPERATE_CONTAINER,transfer_type*10+0,item);

				// Check count
				if(!item_count || item->GetCount()<item_count)
				{
					cl->Send_ContainerInfo();
					cl->Send_Text(cl,"Incorrect count.",SAY_NETMSG);
					return;
				}

				// Check weight
				if(cl->GetFreeWeight()<(int)(item->GetWeight1st()*item_count))
				{
					cl->Send_TextMsg(cl,STR_OVERWEIGHT,SAY_NETMSG,TEXTMSG_GAME);
					break;
				}

				// Check volume
				if(cl->GetFreeVolume()<(int)(item->GetVolume1st()*item_count))
				{
					cl->Send_TextMsg(cl,STR_OVERVOLUME,SAY_NETMSG,TEXTMSG_GAME);
					break;
				}

				// Process steal
				if(transfer_type==TRANSFER_CRIT_STEAL)
				{
					if(!cr->EventStealing(cl,item,item_count))
					{
						cr->Send_TextMsg(cl,STR_SKILL_STEAL_TRIED_GET,SAY_NETMSG,TEXTMSG_GAME);
						cl->Send_ContainerInfo();
						return;
					}
				}

				// Script events
				if(item->EventSkill(cl,SKILL_TAKE_CONT)) return;
				if(cl->EventUseSkill(SKILL_TAKE_CONT,NULL,item,NULL)) return;
				if(Script::PrepareContext(ServerFunctions.CritterUseSkill,_FUNC_,cl->GetInfo()))
				{
					Script::SetArgObject(cl);
					Script::SetArgUInt(SKILL_TAKE_CONT);
					Script::SetArgObject(NULL);
					Script::SetArgObject(item);
					Script::SetArgObject(NULL);
					if(Script::RunPrepared() && Script::GetReturnedBool()) return;
				}

				// Transfer
				if(!ItemMngr.MoveItemCritters(cr,cl,item->GetId(),item_count))
					WriteLogF(_FUNC_," - Transfer item, from player to player (get), fail.\n");
			}
			break;
		case CONT_GETALL:
			{
				// Check for steal
				if(is_steal)
				{
					cl->Send_ContainerInfo();
					return;
				}

				// Send
				cl->SendAA_Action(ACTION_OPERATE_CONTAINER,transfer_type*10+1,NULL);

				// Get items
				ItemPtrVec items;
				cr->GetInvItems(items,transfer_type,true);
				if(items.empty()) return;

				// Check weight, volume
				uint weight=0,volume=0;
				for(int i=0,j=items.size();i<j;++i)
				{
					weight+=items[i]->GetWeight();
					volume+=items[i]->GetVolume();
				}

				if(cl->GetFreeWeight()<(int)weight)
				{
					cl->Send_TextMsg(cl,STR_OVERWEIGHT,SAY_NETMSG,TEXTMSG_GAME);
					break;
				}

				if(cl->GetFreeVolume()<(int)volume)
				{
					cl->Send_TextMsg(cl,STR_OVERVOLUME,SAY_NETMSG,TEXTMSG_GAME);
					break;
				}

				// Script events
				if(cl->EventUseSkill(SKILL_TAKE_ALL_CONT,cr,NULL,NULL)) return;
				if(cr->EventUseSkillOnMe(cl,SKILL_TAKE_ALL_CONT)) return;
				if(Script::PrepareContext(ServerFunctions.CritterUseSkill,_FUNC_,cl->GetInfo()))
				{
					Script::SetArgObject(cl);
					Script::SetArgUInt(SKILL_TAKE_ALL_CONT);
					Script::SetArgObject(cr);
					Script::SetArgObject(NULL);
					Script::SetArgObject(NULL);
					if(Script::RunPrepared() && Script::GetReturnedBool()) return;
				}

				// Transfer
				for(uint i=0,j=items.size();i<j;++i)
				{
					if(!items[i]->EventSkill(cl,SKILL_TAKE_CONT))
					{
						cr->EraseItem(items[i],true);
						cl->AddItem(items[i],true);
					}
				}
			}
			break;
		case CONT_PUT:
			{
				// Get item
				Item* item=cl->GetItem(item_id,true);
				if(!item || item->ACC_CRITTER.Slot!=SLOT_INV)
				{
					cl->Send_ContainerInfo();
					cl->Send_TextMsg(cl,STR_ITEM_NOT_FOUND,SAY_NETMSG,TEXTMSG_GAME);
					return;
				}

				// Send
				cl->SendAA_Action(ACTION_OPERATE_CONTAINER,transfer_type*10+2,item);

				// Check count
				if(!item_count || item->GetCount()<item_count)
				{
					cl->Send_ContainerInfo();
					cl->Send_Text(cl,"Incorrect count.",SAY_NETMSG);
					return;
				}

				// Check slot
				if(item->ACC_CRITTER.Slot!=SLOT_INV)
				{
					cl->Send_ContainerInfo();
					cl->Send_Text(cl,"Cheat detected.",SAY_NETMSG);
					WriteLogF(_FUNC_," - Attempting to put in a container not from the inventory2, client<%s>.",cl->GetInfo());
					return;
				}

				// Check weight, volume
				if(cr->GetFreeWeight()<(int)(item->GetWeight1st()*item_count))
				{
					cl->Send_TextMsg(cl,STR_OVERWEIGHT,SAY_NETMSG,TEXTMSG_GAME);
					break;
				}
				if(cr->GetFreeVolume()<(int)(item->GetVolume1st()*item_count))
				{
					cl->Send_TextMsg(cl,STR_OVERVOLUME,SAY_NETMSG,TEXTMSG_GAME);
					break;
				}

				// Steal process
				if(transfer_type==TRANSFER_CRIT_STEAL)
				{
					if(!cr->EventStealing(cl,item,item_count))
					{
						cr->Send_TextMsg(cl,STR_SKILL_STEAL_TRIED_PUT,SAY_NETMSG,TEXTMSG_GAME);
						cl->Send_ContainerInfo();
						return;
					}
				}

				// Script events
				if(item->EventSkill(cl,SKILL_PUT_CONT)) return;
				if(cl->EventUseSkill(SKILL_PUT_CONT,NULL,item,NULL)) return;
				if(Script::PrepareContext(ServerFunctions.CritterUseSkill,_FUNC_,cl->GetInfo()))
				{
					Script::SetArgObject(cl);
					Script::SetArgUInt(SKILL_PUT_CONT);
					Script::SetArgObject(NULL);
					Script::SetArgObject(item);
					Script::SetArgObject(NULL);
					if(Script::RunPrepared() && Script::GetReturnedBool()) return;
				}

				// Transfer
				if(!ItemMngr.MoveItemCritters(cl,cr,item->GetId(),item_count))
					WriteLogF(_FUNC_," - transfer item, from player to player (put), fail.\n");
			}
			break;
		case CONT_PUTALL:
			{
				cl->Send_ContainerInfo();
			}
			break;
		default:
			break;
		}

		cl->Send_ContainerInfo(cr,transfer_type,false);
	}
}

void FOServer::Process_UseSkill(Client* cl)
{
	ushort skill;
	uchar targ_type;
	uint target_id;
	ushort target_pid;

	cl->Bin >> skill;
	cl->Bin >> targ_type;
	cl->Bin >> target_id;
	cl->Bin >> target_pid;
	CHECK_IN_BUFF_ERROR(cl);

	if(skill<SKILL_BEGIN || skill>SKILL_END)
	{
		WriteLogF(_FUNC_," - Invalid skill<%d>, client<%s>.\n",skill,cl->GetInfo());
		return;
	}

	if(targ_type>TARGET_SCENERY)
	{
		WriteLogF(_FUNC_," - Invalid target type<%u>, client<%s>.\n",targ_type,cl->GetInfo());
		return;
	}

	Act_Use(cl,0,skill,targ_type,target_id,target_pid,0);
}

void FOServer::Process_Dir(Client* cl)
{
	uchar dir;
	cl->Bin >> dir;
	CHECK_IN_BUFF_ERROR(cl);

	if(!cl->GetMap() || dir>=DIRS_COUNT || cl->GetDir()==dir || cl->IsTalking() || !cl->CheckMyTurn(NULL))
	{
		if(cl->GetDir()!=dir) cl->Send_Dir(cl);
		return;
	}

	cl->Data.Dir=dir;
	cl->SendA_Dir();
}

void FOServer::Process_SetUserHoloStr(Client* cl)
{
	uint msg_len;
	uint holodisk_id;
	ushort title_len;
	ushort text_len;
	char title[USER_HOLO_MAX_TITLE_LEN+1];
	char text[USER_HOLO_MAX_LEN+1];
	cl->Bin >> msg_len;
	cl->Bin >> holodisk_id;
	cl->Bin >> title_len;
	cl->Bin >> text_len;
	if(!title_len || !text_len || title_len>USER_HOLO_MAX_TITLE_LEN || text_len>USER_HOLO_MAX_LEN)
	{
		WriteLogF(_FUNC_," - Length of texts is greater of maximum or zero, title cur<%u>, title max<%u>, text cur<%u>, text max<%u>, client<%s>. Disconnect.\n",title_len,USER_HOLO_MAX_TITLE_LEN,text_len,USER_HOLO_MAX_LEN,cl->GetInfo());
		cl->Disconnect();
		return;
	}
	cl->Bin.Pop(title,title_len);
	title[title_len]='\0';
	cl->Bin.Pop(text,text_len);
	text[text_len]='\0';
	CHECK_IN_BUFF_ERROR(cl);

	cl->SetBreakTime(GameOpt.Breaktime);

	Item* holodisk=cl->GetItem(holodisk_id,true);
	if(!holodisk)
	{
		WriteLogF(_FUNC_," - Holodisk<%u> not found, client<%s>.\n",holodisk_id,cl->GetInfo());
		cl->Send_TextMsg(cl,STR_HOLO_WRITE_FAIL,SAY_NETMSG,TEXTMSG_HOLO);
		return;
	}

	cl->SendAA_Action(ACTION_USE_ITEM,0,holodisk);

#pragma MESSAGE("Check valid of received text.")
//	int invalid_chars=CheckStr(text);
//	if(invalid_chars>0) WriteLogF(_FUNC_," - Found invalid chars, count<%u>, client<%s>, changed on '_'.\n",invalid_chars,cl->GetInfo());

	HolodiskLocker.Lock();

	uint holo_id=holodisk->HolodiskGetNum();
	HoloInfo* hi=GetHoloInfo(holo_id);
	if(hi && hi->CanRewrite)
	{
		hi->Title=title;
		hi->Text=text;
	}
	else
	{
		holo_id=++LastHoloId;
		HolodiskInfo.insert(HoloInfoMapVal(holo_id,new HoloInfo(true,title,text)));
	}

	HolodiskLocker.Unlock();

	cl->Send_UserHoloStr(STR_HOLO_INFO_NAME_(holo_id),title,title_len);
	cl->Send_UserHoloStr(STR_HOLO_INFO_DESC_(holo_id),text,text_len);
	holodisk->HolodiskSetNum(holo_id);
	cl->SendAA_ItemData(holodisk);
	cl->Send_TextMsg(cl,STR_HOLO_WRITE_SUCC,SAY_NETMSG,TEXTMSG_HOLO);
}

#pragma MESSAGE("Check aviability of requested holodisk.")
void FOServer::Process_GetUserHoloStr(Client* cl)
{
	uint str_num;
	cl->Bin >> str_num;

	if(str_num/10<USER_HOLO_START_NUM)
	{
		WriteLogF(_FUNC_," - String value is less than users holo numbers, str num<%u>, client<%s>.\n",str_num,cl->GetInfo());
		return;
	}

	Send_PlayerHoloInfo(cl,str_num/10,(str_num%10)!=0);
}

void FOServer::Process_LevelUp(Client* cl)
{
	uint msg_len;
	ushort count_skill_up;
	UIntVec skills;
	ushort perk_up;

	cl->Bin >> msg_len;

	// Skills up
	cl->Bin >> count_skill_up;
	if(count_skill_up>SKILL_COUNT) count_skill_up=SKILL_COUNT;
	for(int i=0;i<count_skill_up;i++)
	{
		ushort num,val;
		cl->Bin >> num;
		cl->Bin >> val;
		skills.push_back(num);
		skills.push_back(val);
	}

	// Perk up
	cl->Bin >> perk_up;

	CHECK_IN_BUFF_ERROR(cl);

	for(int i=0;i<count_skill_up;i++)
	{
		if(skills[i*2]>=SKILL_BEGIN && skills[i*2]<=SKILL_END && skills[i*2+1] && cl->Data.Params[ST_UNSPENT_SKILL_POINTS]>0
		&& Script::PrepareContext(ServerFunctions.PlayerLevelUp,_FUNC_,cl->GetInfo()))
		{
			Script::SetArgObject(cl);
			Script::SetArgUInt(SKILL_OFFSET(skills[i*2]));
			Script::SetArgUInt(skills[i*2+1]);
			Script::SetArgUInt(-1);
			Script::RunPrepared();
		}
	}

	if(perk_up>=PERK_BEGIN && perk_up<=PERK_END && cl->Data.Params[ST_UNSPENT_PERKS]>0
	&& Script::PrepareContext(ServerFunctions.PlayerLevelUp,_FUNC_,cl->GetInfo()))
	{
		Script::SetArgObject(cl);
		Script::SetArgUInt(-1);
		Script::SetArgUInt(0);
		Script::SetArgUInt(PERK_OFFSET(perk_up));
		Script::RunPrepared();
	}

	cl->Send_Param(ST_UNSPENT_SKILL_POINTS);
	cl->Send_Param(ST_UNSPENT_PERKS);
}

void FOServer::Process_CraftAsk(Client* cl)
{
	uint tick=Timer::FastTick();
	if(tick<cl->LastSendCraftTick+CRAFT_SEND_TIME)
	{
		WriteLogF(_FUNC_," - Client<%s> ignore send craft timeout.\n",cl->GetInfo());
		return;
	}
	cl->LastSendCraftTick=tick;

	uint msg_len;
	ushort count;
	cl->Bin >> msg_len;
	cl->Bin >> count;

	UIntVec numbers;
	numbers.reserve(count);
	for(int i=0;i<count;i++)
	{
		uint craft_num;
		cl->Bin >> craft_num;
		if(MrFixit.IsShowCraft(cl,craft_num)) numbers.push_back(craft_num);
	}
	CHECK_IN_BUFF_ERROR(cl);

	uint msg=NETMSG_CRAFT_ASK;
	count=numbers.size();
	msg_len=sizeof(msg)+sizeof(msg_len)+sizeof(count)+sizeof(uint)*count;

	BOUT_BEGIN(cl);
	cl->Bout << msg;
	cl->Bout << msg_len;
	cl->Bout << count;
	for(int i=0,j=numbers.size();i<j;i++) cl->Bout << numbers[i];
	BOUT_END(cl);
}

void FOServer::Process_Craft(Client* cl)
{
	uint craft_num;

	cl->Bin >> craft_num;
	CHECK_IN_BUFF_ERROR(cl);

	uchar res=MrFixit.ProcessCraft(cl,craft_num);

	BOUT_BEGIN(cl);
	cl->Bout << NETMSG_CRAFT_RESULT;
	cl->Bout << res;
	BOUT_END(cl);
}

void FOServer::Process_Ping(Client* cl)
{
	uchar ping;

	cl->Bin >> ping;
	CHECK_IN_BUFF_ERROR(cl);

	if(ping==PING_CLIENT)
	{
		cl->PingOk(PING_CLIENT_LIFE_TIME);
		return;
	}
	else if(ping==PING_UID_FAIL)
	{
#ifndef DEV_VESRION
		SCOPE_LOCK(ClientsDataLocker);

		ClientData* data=GetClientData(cl->GetId());
		if(data)
		{
			WriteLogF(_FUNC_," - Wrong UID, client<%s>. Disconnect.\n",cl->GetInfo());
			for(int i=0;i<5;i++) data->UID[i]=Random(0,10000);
			data->UIDEndTick=Timer::FastTick()+GameOpt.AccountPlayTime*1000;
		}
		cl->Disconnect();
		return;
#endif
	}
	else if(ping==PING_PING || ping==PING_WAIT)
	{
		// Valid pings
	}
	else
	{
		WriteLog("Unknown ping<%u>, client<%s>.\n",ping,cl->GetInfo());
		return;
	}

	BOUT_BEGIN(cl);
	cl->Bout << NETMSG_PING;
	cl->Bout << ping;
	BOUT_END(cl);
}

void FOServer::Process_PlayersBarter(Client* cl)
{
	uchar barter;
	uint param;
	uint param_ext;

	cl->Bin >> barter;
	cl->Bin >> param;
	cl->Bin >> param_ext;
	CHECK_IN_BUFF_ERROR(cl);

//WriteLog("Barter<%s,%u,%u,%u>.\n",cl->GetName(),barter,param,param_ext);

	if(barter==BARTER_TRY || barter==BARTER_ACCEPTED)
	{
		if(!param) return;
		Client* opponent=cl->BarterGetOpponent(param);
		if(!opponent)
		{
			cl->Send_TextMsg(cl,STR_BARTER_BEGIN_FAIL,SAY_NETMSG,TEXTMSG_GAME);
			return;
		}

		cl->BarterOffer=false;
		cl->BarterHide=(param_ext!=0);
		cl->BarterItems.clear();

		if(barter==BARTER_TRY)
		{
			opponent->Send_PlayersBarter(BARTER_TRY,cl->GetId(),param_ext);
		}
		else if(opponent->BarterOpponent==cl->GetId()) //Accepted
		{
			param_ext=(cl->BarterHide?1:0)|(opponent->BarterHide?2:0);
			cl->Send_PlayersBarter(BARTER_BEGIN,opponent->GetId(),param_ext);
			if(!opponent->BarterHide) cl->Send_ContainerInfo(opponent,TRANSFER_CRIT_BARTER,false);
			param_ext=(opponent->BarterHide?1:0)|(cl->BarterHide?2:0);
			opponent->Send_PlayersBarter(BARTER_BEGIN,cl->GetId(),param_ext);
			if(!cl->BarterHide) opponent->Send_ContainerInfo(cl,TRANSFER_CRIT_BARTER,false);
		}
		return;
	}

	Client* opponent=cl->BarterGetOpponent(cl->BarterOpponent);
	if(!opponent)
	{
		cl->Send_TextMsg(cl,STR_BARTER_BEGIN_FAIL,SAY_NETMSG,TEXTMSG_GAME);
		return;
	}

	if(barter==BARTER_REFRESH)
	{
		cl->BarterRefresh(opponent);
	}
	else if(barter==BARTER_END)
	{
		cl->BarterEnd();
		opponent->BarterEnd();
	}
	else if(barter==BARTER_OFFER)
	{
		cl->BarterOffer=(param!=0);
		opponent->Send_PlayersBarter(BARTER_OFFER,param,true);

		// Deal, try transfer items
		if(cl->BarterOffer && opponent->BarterOffer)
		{
			bool is_succ=false;
			cl->BarterOffer=false;
			opponent->BarterOffer=false;

			// Check for weight
			// Player
			uint weigth=0;
			for(uint i=0;i<cl->BarterItems.size();i++)
			{
				Client::BarterItem& barter_item=cl->BarterItems[i];
				ProtoItem* proto_item=ItemMngr.GetProtoItem(barter_item.Pid);
				if(!proto_item) WriteLogF(_FUNC_," - proto item not found, pid<%u>.\n",barter_item.Pid);
				weigth+=proto_item->Weight*barter_item.Count;
			}
			// Opponent
			uint weigth_=0;
			for(uint i=0;i<opponent->BarterItems.size();i++)
			{
				Client::BarterItem& barter_item=opponent->BarterItems[i];
				ProtoItem* proto_item=ItemMngr.GetProtoItem(barter_item.Pid);
				if(!proto_item) WriteLogF(_FUNC_," - proto item not found_, pid<%u>.\n",barter_item.Pid);
				weigth_+=proto_item->Weight*barter_item.Count;
			}
			// Check
			if(cl->GetFreeWeight()+(int)weigth<(int)weigth_)
			{
				cl->Send_TextMsg(cl,STR_BARTER_OVERWEIGHT,SAY_NETMSG,TEXTMSG_GAME);
				goto label_EndOffer;
			}
			if(opponent->GetFreeWeight()+(int)weigth_<(int)weigth)
			{
				opponent->Send_TextMsg(opponent,STR_BARTER_OVERWEIGHT,SAY_NETMSG,TEXTMSG_GAME);
				goto label_EndOffer;
			}

			// Transfer
			// Player
			for(uint i=0;i<cl->BarterItems.size();i++)
			{
				Client::BarterItem& bitem=cl->BarterItems[i];
				if(!ItemMngr.MoveItemCritters(cl,opponent,bitem.Id,bitem.Count))
					WriteLogF(_FUNC_," - transfer item, from player to player_, fail.\n");
			}
			// Player_
			for(uint i=0;i<opponent->BarterItems.size();i++)
			{
				Client::BarterItem& bitem=opponent->BarterItems[i];
				if(!ItemMngr.MoveItemCritters(opponent,cl,bitem.Id,bitem.Count))
					WriteLogF(_FUNC_," - transfer item, from player_ to player, fail.\n");
			}

			is_succ=true;
			cl->BarterItems.clear();
			opponent->BarterItems.clear();

label_EndOffer:
			if(is_succ)
			{
				cl->SetBreakTime(GameOpt.Breaktime);
				cl->BarterRefresh(opponent);
				opponent->SetBreakTime(GameOpt.Breaktime);
				opponent->BarterRefresh(cl);
				if(cl->GetMap())
				{
					cl->Send_Action(cl,ACTION_BARTER,0,NULL);
					cl->SendAA_Action(ACTION_BARTER,0,NULL);
					opponent->Send_Action(opponent,ACTION_BARTER,0,NULL);
					opponent->SendAA_Action(ACTION_BARTER,0,NULL);
				}
			}
			else
			{
				cl->Send_PlayersBarter(BARTER_OFFER,false,false);
				cl->Send_PlayersBarter(BARTER_OFFER,false,true);
				opponent->Send_PlayersBarter(BARTER_OFFER,false,false);
				opponent->Send_PlayersBarter(BARTER_OFFER,false,true);
			}
		}
	}
	else
	{
		if(!param || !param_ext) return;

		bool is_set=(barter==BARTER_SET_SELF || barter==BARTER_SET_OPPONENT);
		Client* barter_cl;
		if(barter==BARTER_SET_SELF || barter==BARTER_UNSET_SELF) barter_cl=cl;
		else if(barter==BARTER_SET_OPPONENT || barter==BARTER_UNSET_OPPONENT) barter_cl=opponent;
		else return;

		if(barter_cl==opponent && opponent->BarterHide)
		{
			cl->Send_Text(cl,"Cheat fail.",SAY_NETMSG);
			WriteLogF(_FUNC_," - Player try operate opponent inventory in hide mode, player<%s>, opponent<%s>.\n",cl->GetInfo(),opponent->GetInfo());
			return;
		}

		Item* item=barter_cl->GetItem(param,true);
		Client::BarterItem* barter_item=barter_cl->BarterGetItem(param);

		if(!item || (barter_item && barter_item->Count>item->GetCount()))
		{
			barter_cl->BarterEraseItem(param);
			cl->BarterRefresh(opponent);
			opponent->BarterRefresh(cl);
			return;
		}

		if(is_set)
		{
			if(param_ext>item->GetCount()-(barter_item?barter_item->Count:0))
			{
				barter_cl->BarterEraseItem(param);
				cl->BarterRefresh(opponent);
				opponent->BarterRefresh(cl);
				return;
			}
			if(!barter_item) barter_cl->BarterItems.push_back(Client::BarterItem(item->GetId(),item->GetProtoId(),param_ext));
			else barter_item->Count+=param_ext;
		}
		else
		{
			if(!barter_item || param_ext>barter_item->Count)
			{
				barter_cl->BarterEraseItem(param);
				cl->BarterRefresh(opponent);
				opponent->BarterRefresh(cl);
				return;
			}
			barter_item->Count-=param_ext;
			if(!barter_item->Count) barter_cl->BarterEraseItem(param);
		}

		cl->BarterOffer=false;
		opponent->BarterOffer=false;
		if(barter==BARTER_SET_SELF) barter=BARTER_SET_OPPONENT;
		else if(barter==BARTER_SET_OPPONENT) barter=BARTER_SET_SELF;
		else if(barter==BARTER_UNSET_SELF) barter=BARTER_UNSET_OPPONENT;
		else if(barter==BARTER_UNSET_OPPONENT) barter=BARTER_UNSET_SELF;

		if(barter==BARTER_SET_OPPONENT && cl->BarterHide) opponent->Send_PlayersBarterSetHide(item,param_ext);
		else opponent->Send_PlayersBarter(barter,param,param_ext);
	}
}

void FOServer::Process_ScreenAnswer(Client* cl)
{
	uint answer_i;
	char answer_s[MAX_SAY_NPC_TEXT+1];
	cl->Bin >> answer_i;
	cl->Bin.Pop(answer_s,MAX_SAY_NPC_TEXT);
	answer_s[MAX_SAY_NPC_TEXT]=0;

	if(cl->ScreenCallbackBindId<=0)
	{
		WriteLogF(_FUNC_," - Client<%s> answered on not not specified screen.\n",cl->GetInfo());
		return;
	}

	int bind_id=cl->ScreenCallbackBindId;
	cl->ScreenCallbackBindId=0;

	if(!Script::PrepareContext(bind_id,_FUNC_,cl->GetInfo())) return;
	Script::SetArgObject(cl);
	Script::SetArgUInt(answer_i);
	CScriptString* lexems=new CScriptString(answer_s);
	Script::SetArgObject(lexems);
	Script::RunPrepared();
	lexems->Release();
}

void FOServer::Process_Combat(Client* cl)
{
	uchar type;
	int val;
	cl->Bin >> type;
	cl->Bin >> val;
	CHECK_IN_BUFF_ERROR(cl);

	if(type==COMBAT_TB_END_TURN)
	{
		Map* map=MapMngr.GetMap(cl->GetMap());
		if(!map)
		{
			WriteLogF(_FUNC_," - Map not found on end turn, client<%s>.\n",cl->GetInfo());
			return;
		}
		if(map->IsTurnBasedOn && map->IsCritterTurn(cl)) map->EndCritterTurn();
	}
	else if(type==COMBAT_TB_END_COMBAT)
	{
		Map* map=MapMngr.GetMap(cl->GetMap());
		if(!map)
		{
			WriteLogF(_FUNC_," - Map not found on end combat, client<%s>.\n",cl->GetInfo());
			return;
		}
		if(map->IsTurnBasedOn) cl->Data.Params[MODE_END_COMBAT]=(val?1:0);
	}
	else
	{
		WriteLogF(_FUNC_," - Unknown type<%u>, value<%d>, client<%s>.\n",type,val,cl->GetInfo());
	}
}

void FOServer::Process_RunServerScript(Client* cl)
{
	uint msg_len;
	bool unsafe=false;
	ushort script_name_len;
	char script_name[MAX_SCRIPT_NAME*2+2]={0};
	int p0,p1,p2;
	ushort p3len;
	char p3str[MAX_FOTEXT];
	CScriptString* p3=NULL;
	ushort p4size;
	CScriptArray* p4=NULL;

	cl->Bin >> msg_len;
	cl->Bin >> unsafe;
	if(!unsafe && !FLAG(cl->Access,ACCESS_MODER|ACCESS_ADMIN|ACCESS_IMPLEMENTOR))
	{
		WriteLogF(_FUNC_," - Attempt to execute script without privilege. Client<%s>.\n",cl->GetInfo());
		cl->Send_Text(cl,"Access denied. Disconnect.",SAY_NETMSG);
		cl->Disconnect();
		return;
	}

	cl->Bin >> script_name_len;
	if(script_name_len && script_name_len<MAX_SCRIPT_NAME*2+2)
	{
		cl->Bin.Pop(script_name,script_name_len);
		script_name[script_name_len]=0;
	}

	char module_name[MAX_SCRIPT_NAME+1]={0};
	char func_name[MAX_SCRIPT_NAME+1]={0};
	Script::ReparseScriptName(script_name,module_name,func_name);

	if(unsafe && (Str::Length(func_name)<=7 || !Str::CompareCount(func_name,"unsafe_",7))) // Check unsafe_ prefix
	{
		WriteLogF(_FUNC_," - Attempt to execute script without \"unsafe_\" prefix. Client<%s>.\n",cl->GetInfo());
		cl->Send_Text(cl,"Access denied. Disconnect.",SAY_NETMSG);
		cl->Disconnect();
		return;
	}

	cl->Bin >> p0;
	cl->Bin >> p1;
	cl->Bin >> p2;
	cl->Bin >> p3len;
	if(p3len && p3len<MAX_FOTEXT)
	{
		cl->Bin.Pop(p3str,p3len);
		p3str[p3len]=0;
		p3=new CScriptString(p3str);
	}
	cl->Bin >> p4size;
	if(p4size)
	{
		p4=Script::CreateArray("int[]");
		if(p4)
		{
			p4->Resize(p4size);
			cl->Bin.Pop((char*)p4->At(0),p4size*sizeof(uint));
		}
	}

	CHECK_IN_BUFF_ERROR(cl);

	int bind_id=Script::Bind(module_name,func_name,"void %s(Critter&,int,int,int,string@,int[]@)",true);
	if(bind_id>0 && Script::PrepareContext(bind_id,_FUNC_,cl->GetInfo()))
	{
		Script::SetArgObject(cl);
		Script::SetArgUInt(p0);
		Script::SetArgUInt(p1);
		Script::SetArgUInt(p2);
		Script::SetArgObject(p3);
		Script::SetArgObject(p4);
		Script::RunPrepared();
	}

	if(p3) p3->Release();
	if(p4) p4->Release();
}

void FOServer::Process_KarmaVoting(Client* cl)
{
	uint crid;
	bool is_up;
	cl->Bin >> crid;
	cl->Bin >> is_up;

	if(cl->GetId()==crid) return;
	if(cl->GetParam(TO_KARMA_VOTING)) return;

	Critter* cr=CrMngr.GetCritter(crid,true);
//	if(cl->GetMap()) cr=cl->GetCritSelf(crid);
//	else if(cl->GroupMove) cr=cl->GroupMove->GetCritter(crid);
	if(!cr)
	{
		cl->SetTimeout(TO_KARMA_VOTING,1*GameOpt.TimeMultiplier); // Wait 1 second
		return;
	}

	if(Script::PrepareContext(ServerFunctions.KarmaVoting,_FUNC_,cl->GetInfo()))
	{
		Script::SetArgObject(cl);
		Script::SetArgObject(cr);
		Script::SetArgBool(is_up);
		Script::RunPrepared();
	}
}

void FOServer::Process_GiveGlobalInfo(Client* cl)
{
	//	uchar info_flags;
	//	cl->Bin >> info_flags;
	//	cl->Send_GlobalInfo(info_flags);
}

void FOServer::Process_RuleGlobal(Client* cl)
{
	uchar command;
	uint param1;
	uint param2;
	cl->Bin >> command;
	cl->Bin >> param1;
	cl->Bin >> param2;

	switch(command)
	{
	case GM_CMD_FOLLOW_CRIT:
		{
			if(!param1) break;

			Critter* cr=cl->GetCritSelf(param1,false);
			if(!cr) break;

			if(cl->GetFollowCrId()==cr->GetId()) cl->SetFollowCrId(0);
			else cl->SetFollowCrId(cr->GetId());
		}
		break;
	case GM_CMD_SETMOVE:
		if(cl->GetMap() || !cl->GroupMove || cl!=cl->GroupMove->Rule) break;
		if(param1>=GM_MAXX || param2>=GM_MAXY) break;
		if(cl->GroupMove->EncounterDescriptor) break;
		cl->GroupMove->MoveX=param1;
		cl->GroupMove->MoveY=param2;
		MapMngr.GM_GlobalProcess(cl,cl->GroupMove,GLOBAL_PROCESS_SET_MOVE);
		break;
	case GM_CMD_STOP:
		break;
	case GM_CMD_TOLOCAL:
		if(cl->GetMap() || !cl->GroupMove || cl!=cl->GroupMove->Rule) break;
		if(cl->GroupMove->EncounterDescriptor) break;
		if(cl->GetParam(TO_TRANSFER))
		{
			cl->Send_TextMsg(cl,STR_TIMEOUT_TRANSFER_WAIT,SAY_NETMSG,TEXTMSG_GAME);
			break;
		}

		if(!param1)
		{
			if(cl->GetMap() || !cl->GroupMove || cl!=cl->GroupMove->Rule) break;
			cl->GroupMove->EncounterDescriptor=0;
			MapMngr.GM_GlobalProcess(cl,cl->GroupMove,GLOBAL_PROCESS_ENTER);
		}
		else
		{
			MapMngr.GM_GroupToLoc(cl,param1,param2);
		}
		return;
	case GM_CMD_ANSWER:
		if(cl->GetMap() || !cl->GroupMove || cl!=cl->GroupMove->Rule) break;
		if(!cl->GroupMove->EncounterDescriptor || cl->GroupMove->EncounterForce) break;

		if((int)param1>=0) // Yes
		{
			MapMngr.GM_GlobalInvite(cl->GroupMove,param1);
			return;
		}
		else if(cl->GroupMove->EncounterDescriptor) // No
		{
			cl->GroupMove->StartEncaunterTime(GameOpt.EncounterTime);
			cl->GroupMove->EncounterDescriptor=0;
			cl->SendA_GlobalInfo(cl->GroupMove,GM_INFO_GROUP_PARAM);
		}
		break;
	case GM_CMD_FOLLOW:
		{
			// Find rule
			Critter* rule=CrMngr.GetCritter(param1,true);
			if(!rule || rule->GetMap() || !rule->GroupMove || rule!=rule->GroupMove->Rule) break;

			// Check for follow
			if(!rule->GroupMove->CheckForFollow(cl)) break;
			if(!CheckDist(rule->Data.LastHexX,rule->Data.LastHexY,cl->GetHexX(),cl->GetHexY(),FOLLOW_DIST+rule->GetMultihex()+cl->GetMultihex())) break;

			// Transit
			if(cl->LockMapTransfers)
			{
				WriteLogF(_FUNC_," - Transfers locked, critter<%s>.\n",cl->GetInfo());
				return;
			}
			if(!MapMngr.TransitToGlobal(cl,param1,0,false)) break;
		}
		return;
	case GM_CMD_KICKCRIT:
		{
			if(cl->GetMap() || !cl->GroupMove || cl->GroupMove->GetSize()<2 || cl->GroupMove->EncounterDescriptor) break;
			if(MapMngr.GetGmRelief(cl->GroupMove->WXi,cl->GroupMove->WYi)==0xF) break;
			GlobalMapGroup* gr=cl->GroupMove;
			Critter* kick_cr;

			if(cl->GetId()==param1) // Kick self
			{
				if(cl==gr->Rule) break;
				kick_cr=cl;
			}
			else // Kick other
			{
				if(cl!=gr->Rule) break;
				kick_cr=gr->GetCritter(param1);
				if(!kick_cr) break;
			}

			MapMngr.GM_LeaveGroup(kick_cr);
		}
		break;
	case GM_CMD_GIVE_RULE:
		{
			// Check
			if(cl->GetId()==param1 || cl->GetMap() || !cl->GroupMove || cl!=cl->GroupMove->Rule || cl->GroupMove->EncounterDescriptor) break;
			Critter* new_rule=cl->GroupMove->GetCritter(param1);
			if(!new_rule || !new_rule->IsPlayer() || !((Client*)new_rule)->IsOnline()) break;

			MapMngr.GM_GiveRule(cl,new_rule);
			MapMngr.GM_StopGroup(cl);
		}
		break;
	case GM_CMD_ENTRANCES:
		{
			if(cl->GetMap() || !cl->GroupMove || cl->GroupMove->EncounterDescriptor) break;

			uint loc_id=param1;
			Location* loc=MapMngr.GetLocation(loc_id);
			if(!loc || DistSqrt(cl->GroupMove->WXi,cl->GroupMove->WYi,loc->Data.WX,loc->Data.WY)>loc->GetRadius()) break;

			uint tick=Timer::FastTick();
			if(cl->LastSendEntrancesLocId==loc_id && tick<cl->LastSendEntrancesTick+GM_ENTRANCES_SEND_TIME)
			{
				WriteLogF(_FUNC_," - Client<%s> ignore send entrances timeout.\n",cl->GetInfo());
				break;
			}
			cl->LastSendEntrancesLocId=loc_id;
			cl->LastSendEntrancesTick=tick;

			if(loc->Proto->ScriptBindId>0)
			{
				uchar count=0;
				uchar show[0x100];
				CScriptArray* arr=MapMngr.GM_CreateGroupArray(cl->GroupMove);
				if(!arr) break;
				for(uchar i=0,j=(uchar)loc->Proto->Entrance.size();i<j;i++)
				{
					if(MapMngr.GM_CheckEntrance(loc,arr,i))
					{
						show[count]=i;
						count++;
					}
				}
				arr->Release();

				uint msg=NETMSG_GLOBAL_ENTRANCES;
				uint msg_len=sizeof(msg)+sizeof(msg_len)+sizeof(loc_id)+sizeof(count)+sizeof(uchar)*count;

				BOUT_BEGIN(cl);
				cl->Bout << msg;
				cl->Bout << msg_len;
				cl->Bout << loc_id;
				cl->Bout << count;
				for(uchar i=0;i<count;i++) cl->Bout << show[i];
				BOUT_END(cl);
			}
			else
			{
				uint msg=NETMSG_GLOBAL_ENTRANCES;
				uchar count=(uchar)loc->Proto->Entrance.size();
				uint msg_len=sizeof(msg)+sizeof(msg_len)+sizeof(loc_id)+sizeof(count)+sizeof(uchar)*count;

				BOUT_BEGIN(cl);
				cl->Bout << msg;
				cl->Bout << msg_len;
				cl->Bout << loc_id;
				cl->Bout << count;
				for(uchar i=0;i<count;i++) cl->Bout << i;
				BOUT_END(cl);
			}
		}
		break;
	case GM_CMD_VIEW_MAP:
		{
			if(cl->GetMap() || !cl->GroupMove || cl->GroupMove->EncounterDescriptor) break;

			uint loc_id=param1;
			Location* loc=MapMngr.GetLocation(loc_id);
			if(!loc || DistSqrt(cl->GroupMove->WXi,cl->GroupMove->WYi,loc->Data.WX,loc->Data.WY)>loc->GetRadius()) break;

			uint entrance=param2;
			if(entrance>=loc->Proto->Entrance.size()) break;
			if(loc->Proto->ScriptBindId>0)
			{
				CScriptArray* arr=MapMngr.GM_CreateGroupArray(cl->GroupMove);
				if(!arr) break;
				bool result=MapMngr.GM_CheckEntrance(loc,arr,entrance);
				arr->Release();
				if(!result) break;
			}

			Map* map=loc->GetMap(loc->Proto->Entrance[entrance].first);
			if(!map) break;

			uchar dir;
			ushort hx,hy;
			if(!map->GetStartCoord(hx,hy,dir,loc->Proto->Entrance[entrance].second)) break;

			cl->Data.HexX=hx;
			cl->Data.HexY=hy;
			cl->Data.Dir=dir;
			cl->ViewMapId=map->GetId();
			cl->ViewMapPid=map->GetPid();
			cl->ViewMapLook=cl->GetLook();
			cl->ViewMapHx=hx;
			cl->ViewMapHy=hy;
			cl->ViewMapDir=dir;
			cl->ViewMapLocId=loc_id;
			cl->ViewMapLocEnt=entrance;
			cl->Send_LoadMap(map);
		}
		break;
	default:
		WriteLogF(_FUNC_," - Unknown command<%u>, from client<%s>.\n",cl->GetInfo());
		break;
	}

	cl->SetBreakTime(GameOpt.Breaktime);
}


void FOServer::Send_MsgData(Client* cl, uint lang, ushort num_msg, FOMsg& data_msg)
{
	if(cl->IsSendDisabled() || cl->IsOffline()) return;

	uint msg=NETMSG_MSG_DATA;
	uint msg_len=sizeof(msg)+sizeof(msg_len)+sizeof(lang)+sizeof(num_msg)+sizeof(uint)+data_msg.GetToSendLen();

	BOUT_BEGIN(cl);
	cl->Bout << msg;
	cl->Bout << msg_len;
	cl->Bout << lang;
	cl->Bout << num_msg;
	cl->Bout << data_msg.GetHash();
	cl->Bout.Push(data_msg.GetToSend(),data_msg.GetToSendLen());
	BOUT_END(cl);
}

void FOServer::Send_ProtoItemData(Client* cl, uchar type, ProtoItemVec& data, uint data_hash)
{
	if(cl->IsSendDisabled() || cl->IsOffline()) return;

	uint msg=NETMSG_ITEM_PROTOS;
	uint msg_len=sizeof(msg)+sizeof(msg_len)+sizeof(type)+sizeof(data_hash)+data.size()*sizeof(ProtoItem);

	BOUT_BEGIN(cl);
	cl->Bout << msg;
	cl->Bout << msg_len;
	cl->Bout << type;
	cl->Bout << data_hash;
	cl->Bout.Push((char*)&data[0],data.size()*sizeof(ProtoItem));
	BOUT_END(cl);
}

