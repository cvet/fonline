#include "StdAfx.h"
#include "Server.h"

#define CHECK_NPC_AP(npc,map,need_ap) do{if(npc->GetParam(ST_CURRENT_AP)<(int)(need_ap)){if(map->IsTurnBasedOn) {if(map->IsCritterTurn(npc)) map->EndCritterTurn();} else npc->SetWait(GameOpt.ApRegeneration/npc->GetParam(ST_ACTION_POINTS)*((int)(need_ap)-npc->GetParam(ST_CURRENT_AP))); return;}}while(0)
#define CHECK_NPC_REAL_AP(npc,map,need_ap) do{if(npc->GetRealAp()<(int)(need_ap)){if(map->IsTurnBasedOn) {if(map->IsCritterTurn(npc)) map->EndCritterTurn();} else npc->SetWait(GameOpt.ApRegeneration/npc->GetParam(ST_ACTION_POINTS)*((int)(need_ap)-npc->GetRealAp())/AP_DIVIDER); return;}}while(0)
#define CHECK_NPC_AP_R0(npc,map,need_ap) do{if(npc->GetParam(ST_CURRENT_AP)<(int)(need_ap)){if(map->IsTurnBasedOn) {if(map->IsCritterTurn(npc)) map->EndCritterTurn();} else npc->SetWait(GameOpt.ApRegeneration/npc->GetParam(ST_ACTION_POINTS)*((int)(need_ap)-npc->GetParam(ST_CURRENT_AP))); return false;}}while(0)
void FOServer::ProcessAI(Npc* npc)
{
	// Check busy
	if(npc->IsBusy()) return;

	// Global map
	if(!npc->GetMap())
	{
		if(!npc->GroupMove) return;
		MapMngr.GM_GlobalProcess(npc,npc->GroupMove,GLOBAL_PROCESS_NPC_IDLE);
		if(!npc->GetMap() && !npc->IsWait()) npc->SetWait(GameOpt.CritterIdleTick);
		return;
	}

	// Get map
	Map* map=MapMngr.GetMap(npc->GetMap());
	if(!map) return;

	// Check turn based
	if(map->IsTurnBasedOn && !map->IsCritterTurn(npc)) return;

	// Check wait
	if(npc->IsWait())
	{
		if(map->IsTurnBasedOn) map->EndCritterTurn();
		return;
	}

	// Get current plane
	AIDataPlane* plane=npc->GetCurPlane();

	// Begin
	if(!plane)
	{
		// Check talking
		if(npc->GetTalkedPlayers()) return;

		// Enemy stack
		Critter* enemy=npc->ScanEnemyStack();
		if(enemy)
		{
			npc->SetTarget(REASON_FOUND_IN_ENEMY_STACK,enemy,GameOpt.DeadHitPoints,false);
			if(npc->GetCurPlane() || npc->IsBusy() || npc->IsWait()) return;
		}

		// Go home
		if(!npc->IsRawParam(MODE_NO_HOME) && map->GetId()==npc->GetHomeMap() && (npc->GetHexX()!=npc->GetHomeX() || npc->GetHexY()!=npc->GetHomeY()))
		{
			if(CritType::IsCanWalk(npc->GetCrType()))
			{
				DWORD tick=Timer::GameTick();
				if(tick<npc->TryingGoHomeTick)
				{
					npc->SetWait(npc->TryingGoHomeTick-tick);
					return;
				}

				npc->TryingGoHomeTick=tick+NPC_GO_HOME_WAIT_TICK;
				npc->MoveToHex(REASON_GO_HOME,npc->GetHomeX(),npc->GetHomeY(),npc->GetHomeOri(),false,0);
				return;
			}
			else if(map->IsHexesPassed(npc->GetHomeX(),npc->GetHomeY(),npc->GetMultihex()))
			{
				MapMngr.Transit(npc,map,npc->GetHomeX(),npc->GetHomeY(),npc->GetDir(),2,true);
				return;
			}
		}
		// Set home direction
		else if(!npc->IsRawParam(MODE_NO_HOME) && CritType::IsCanRotate(npc->GetCrType()) && map->GetId()==npc->GetHomeMap() && npc->GetDir()!=npc->GetHomeOri())
		{
			npc->Data.Dir=npc->GetHomeOri();
			npc->SendA_Dir();
			npc->SetWait(200);
			return;
		}
		else if(!npc->IsRawParam(MODE_NO_FAVORITE_ITEM))
		{
			WORD favor_item_pid;
			Item* favor_item;
			// Set favorite item to slot1
			favor_item_pid=npc->Data.FavoriteItemPid[SLOT_HAND1];
			if(favor_item_pid!=npc->ItemSlotMain->GetProtoId())
			{
				if(npc->ItemSlotMain->GetId())
				{
					AI_MoveItem(npc,map,npc->ItemSlotMain->ACC_CRITTER.Slot,SLOT_INV,npc->ItemSlotMain->GetId(),npc->ItemSlotMain->GetCount());
					return;
				}
				else if(favor_item_pid && (favor_item=npc->GetItemByPid(favor_item_pid)) && (!favor_item->IsWeapon() || CritType::IsAnim1(npc->GetCrType(),favor_item->Proto->Weapon.Anim1)))
				{
					AI_MoveItem(npc,map,favor_item->ACC_CRITTER.Slot,SLOT_HAND1,favor_item->GetId(),favor_item->GetCount());
					return;
				}
			}
			// Set favorite item to slot2
			favor_item_pid=npc->Data.FavoriteItemPid[SLOT_HAND2];
			if(favor_item_pid!=npc->ItemSlotExt->GetProtoId())
			{
				if(npc->ItemSlotExt->GetId()) {AI_MoveItem(npc,map,npc->ItemSlotExt->ACC_CRITTER.Slot,SLOT_INV,npc->ItemSlotExt->GetId(),npc->ItemSlotExt->GetCount()); return;}
				else if(favor_item_pid && (favor_item=npc->GetItemByPid(favor_item_pid))) {AI_MoveItem(npc,map,favor_item->ACC_CRITTER.Slot,SLOT_HAND2,favor_item->GetId(),favor_item->GetCount()); return;}
			}
			// Set favorite item to slot armor
			favor_item_pid=npc->Data.FavoriteItemPid[SLOT_ARMOR];
			if(favor_item_pid!=npc->ItemSlotArmor->GetProtoId())
			{
				if(npc->ItemSlotArmor->GetId()) {AI_MoveItem(npc,map,npc->ItemSlotArmor->ACC_CRITTER.Slot,SLOT_INV,npc->ItemSlotArmor->GetId(),npc->ItemSlotArmor->GetCount()); return;}
				else if(favor_item_pid && (favor_item=npc->GetItemByPid(favor_item_pid)) && favor_item->IsArmor() && !favor_item->Proto->Slot) {AI_MoveItem(npc,map,favor_item->ACC_CRITTER.Slot,SLOT_ARMOR,favor_item->GetId(),favor_item->GetCount()); return;}
			}
		}

		// Idle
		npc->EventIdle();
		if(npc->GetCurPlane() || npc->IsBusy() || npc->IsWait()) return;

		// Wait
		npc->SetWait(GameOpt.CritterIdleTick);
		if(map->IsTurnBasedOn && map->IsCritterTurn(npc)) map->EndCritterTurn();
		return;
	}

	// Process move
	if(plane->IsMove)
	{
		// Check run availability
		if(plane->Move.IsRun)
		{
			if(npc->IsDmgLeg()) plane->Move.IsRun=false; // Clipped legs
			else if(npc->IsOverweight()) plane->Move.IsRun=false; // Overweight
		}

		// Check ap availability
		int ap_move=npc->GetApCostCritterMove(plane->Move.IsRun);
		if(map->IsTurnBasedOn) ap_move+=npc->GetParam(ST_MOVE_AP);
		plane->IsMove=false;
		CHECK_NPC_REAL_AP(npc,map,ap_move);
		plane->IsMove=true;

		// Check
		if(plane->Move.PathNum && plane->Move.TargId)
		{
			Critter* targ=npc->GetCritSelf(plane->Move.TargId,true);
			if(!targ || ((plane->Attack.LastHexX || plane->Attack.LastHexY) && !CheckDist(targ->GetHexX(),targ->GetHexY(),plane->Attack.LastHexX,plane->Attack.LastHexY,0)))
			{
				plane->Move.PathNum=0;//PathSafeFinish(*step);
				//npc->SendA_XY();
			}
		}

		// Find path if not exist
		if(!plane->Move.PathNum)
		{
			WORD hx;
			WORD hy;
			DWORD cut;
			DWORD trace;
			Critter* trace_cr;
			bool check_cr=false;

			if(plane->Move.TargId)
			{
				Critter* targ=npc->GetCritSelf(plane->Move.TargId,true);
				if(!targ)
				{
					plane->IsMove=false;
					npc->SendA_XY();
					if(map->IsTurnBasedOn && map->IsCritterTurn(npc)) map->EndCritterTurn();
					return;
				}

				hx=targ->GetHexX();
				hy=targ->GetHexY();
				cut=plane->Move.Cut;
				trace=plane->Move.Trace;
				trace_cr=targ;
				check_cr=true;
			}
			else
			{
				hx=plane->Move.HexX;
				hy=plane->Move.HexY;
				cut=plane->Move.Cut;
				trace=0;
				trace_cr=NULL;
			}

			PathFindData pfd;
			pfd.Clear();
			pfd.MapId=npc->GetMap();
			pfd.FromCritter=npc;
			pfd.FromX=npc->GetHexX();
			pfd.FromY=npc->GetHexY();
			pfd.ToX=hx;
			pfd.ToY=hy;
			pfd.IsRun=plane->Move.IsRun;
			pfd.Multihex=npc->GetMultihex();
			pfd.Cut=cut;
			pfd.Trace=trace;
			pfd.TraceCr=trace_cr;
			pfd.CheckCrit=true;
			pfd.CheckGagItems=true;

			if(pfd.IsRun && !CritType::IsCanRun(npc->GetCrType())) pfd.IsRun=false;
			if(!pfd.IsRun && !CritType::IsCanWalk(npc->GetCrType()))
			{
				plane->IsMove=false;
				if(map->IsTurnBasedOn && map->IsCritterTurn(npc)) map->EndCritterTurn();
				return;
			}

			int result=MapMngr.FindPath(pfd);
			if(pfd.GagCritter)
			{
				plane->IsMove=false;
				npc->NextPlane(REASON_GAG_CRITTER,pfd.GagCritter,NULL);
				return;
			}

			if(pfd.GagItem)
			{
				plane->IsMove=false;
				npc->NextPlane(REASON_GAG_ITEM,NULL,pfd.GagItem);
				return;
			}

			// Failed
			if(result!=FPATH_OK)
			{
				if(result==FPATH_ALREADY_HERE)
				{
					plane->IsMove=false;
					return;
				}

				int reason=0;
				switch(result)
				{
				case FPATH_MAP_NOT_FOUND: reason=REASON_FIND_PATH_ERROR; break;
				case FPATH_TOOFAR: reason=REASON_HEX_TOO_FAR; break;
				case FPATH_ERROR: reason=REASON_FIND_PATH_ERROR; break;
				case FPATH_INVALID_HEXES: reason=REASON_FIND_PATH_ERROR; break;
				case FPATH_TRACE_TARG_NULL_PTR: reason=REASON_FIND_PATH_ERROR; break;
				case FPATH_HEX_BUSY: reason=REASON_HEX_BUSY; break;
				case FPATH_HEX_BUSY_RING: reason=REASON_HEX_BUSY_RING; break;
				case FPATH_DEADLOCK: reason=REASON_DEADLOCK; break;
				case FPATH_TRACE_FAIL: reason=REASON_TRACE_FAIL; break;
				case FPATH_ALLOC_FAIL: reason=REASON_FIND_PATH_ERROR; break;
				default: reason=REASON_FIND_PATH_ERROR; break;
				}

				plane->IsMove=false;
				npc->SendA_XY();
				npc->NextPlane(reason);
				//if(!npc->GetCurPlane() && !npc->IsBusy() && !npc->IsWait()) npc->SetWait(Random(2000,3000));
				return;
			}

			// Success
			plane->Move.PathNum=pfd.PathNum;
			plane->Move.Iter=0;
			npc->SetWait(0);
		}

		// Get path and move
		PathStepVec& path=MapMngr.GetPath(plane->Move.PathNum);
		if(plane->Move.PathNum && plane->Move.Iter<path.size())
		{
			PathStep& ps=path[plane->Move.Iter];
			if(!CheckDist(npc->GetHexX(),npc->GetHexY(),ps.HexX,ps.HexY,1) || !Act_Move(npc,ps.HexX,ps.HexY,ps.MoveParams))
			{
				// Error
				plane->Move.PathNum=0;
				//plane->IsMove=false;
				npc->SendA_XY();
			}
			else
			{
				// Next
				plane->Move.Iter++;
			}
		}
		else
		{
			// Done
			plane->IsMove=false;
		}
	}

	bool is_busy=(plane->IsMove || npc->IsBusy() || npc->IsWait());

	// Process planes
	switch(plane->Type)
	{
/************************************************************************/
/* ==================================================================== */
/* ========================   Misc   ================================== */
/* ==================================================================== */
/************************************************************************/
	case AI_PLANE_MISC:
		{
			if(is_busy) break;

			DWORD wait=plane->Misc.WaitSecond;
			int bind_id=plane->Misc.ScriptBindId;

			if(wait>GameOpt.FullSecond)
			{
				AI_Stay(npc,(wait-GameOpt.FullSecond)*1000/GameOpt.TimeMultiplier);
			}
			else if(bind_id>0)
			{
				if(Script::PrepareContext(bind_id,CALL_FUNC_STR,npc->GetInfo()))
				{
					Script::SetArgObject(npc);
					Script::RunPrepared();
				}
				plane->Misc.ScriptBindId=0;
			}
			else
			{
				npc->NextPlane(REASON_SUCCESS);
			}
		}
		break;
/************************************************************************/
/* ==================================================================== */
/* ========================   Attack   ================================ */
/* ==================================================================== */
/************************************************************************/
	case AI_PLANE_ATTACK:
		{
			if(map->Data.IsTurnBasedAviable && !map->IsTurnBasedOn) map->BeginTurnBased(npc);

		/************************************************************************/
		/* Target is visible                                                    */
		/************************************************************************/
			Critter* targ=npc->GetCritSelf(plane->Attack.TargId,true);
			if(targ)
			{
			/************************************************************************/
			/* Step 0: Check for success plane and continue target timeout          */
			/************************************************************************/

				if(plane->Attack.IsGag && (targ->GetHexX()!=plane->Attack.GagHexX || targ->GetHexY()!=plane->Attack.GagHexY))
				{
					npc->NextPlane(REASON_SUCCESS);
					break;
				}

				bool attack_to_dead=(plane->Attack.MinHp<=GameOpt.DeadHitPoints);
				if(!plane->Attack.IsGag && (!attack_to_dead && targ->GetParam(ST_CURRENT_HP)<=plane->Attack.MinHp) || (attack_to_dead && targ->IsDead()))
				{
					npc->NextPlane(REASON_SUCCESS);
					break;
				}

				if(plane->IsMove && !plane->Attack.LastHexX && !plane->Attack.LastHexY)
				{
					plane->IsMove=false;
					npc->SendA_XY();
				}
				plane->Attack.LastHexX=targ->GetHexX();
				plane->Attack.LastHexY=targ->GetHexY();

				if(is_busy) break;

			/************************************************************************/
			/* Step 1: Choose weapon                                                */
			/************************************************************************/
				// Get battle weapon
				int use;
				Item* weap=NULL;
				DWORD r0=targ->GetId(),r1=0,r2=0;
				int sss=0;
				if(!npc->RunPlane(REASON_ATTACK_WEAPON,r0,r1,r2))
				{
					WriteLog("REASON_ATTACK_WEAPON fail. Skip attack.\n");
					break;
				}
				if(plane!=npc->GetCurPlane()) break; // Validate plane

				if(r0)
				{
					weap=npc->GetItem(r0,false);
					SETFLAG(sss,0x000001);
				}
				else
				{
					if(npc->IsRawParam(MODE_NO_UNARMED))
					{
						npc->NextPlane(REASON_NO_UNARMED);
						break;
					}

					SETFLAG(sss,0x000002);
					ProtoItem* unarmed=ItemMngr.GetProtoItem(r2);
					if(unarmed && unarmed->Weapon.IsUnarmed)
					{
						SETFLAG(sss,0x000004);
						Item* def_item_main=npc->GetDefaultItemSlotMain();
						if(def_item_main->Proto!=unarmed) def_item_main->Init(unarmed);
						weap=def_item_main;
					}
				}
				use=r1;

				if(!weap || !weap->IsWeapon() || !weap->WeapIsUseAviable(use))
				{
					WriteLog("REASON_ATTACK_WEAPON fail, debug values<%u><%p><%d><%d>.\n",sss,weap,weap?weap->IsWeapon():-1,weap?weap->WeapIsUseAviable(use):-1);
					break;
				}

				// Hide cur, show new
				if(weap!=npc->ItemSlotMain)
				{
					// Hide cur item
					if(npc->ItemSlotMain->GetId()) // Is no hands
					{
						Item* item_hand=npc->ItemSlotMain;
						AI_MoveItem(npc,map,SLOT_HAND1,SLOT_INV,item_hand->GetId(),item_hand->GetCount());
						break;
					}

					// Show new
					if(weap->GetId()) // Is no hands
					{
						AI_MoveItem(npc,map,weap->ACC_CRITTER.Slot,SLOT_HAND1,weap->GetId(),weap->GetCount());
						break;
					}
				}
				npc->ItemSlotMain->SetMode(MAKE_ITEM_MODE(use,0));

				// Load weapon
				if(!npc->IsRawParam(MODE_UNLIMITED_AMMO) && weap->WeapGetMaxAmmoCount() && weap->WeapIsEmpty())
				{
					Item* ammo=npc->GetAmmoForWeapon(weap);
					if(!ammo)
					{
						WriteLog(__FUNCTION__" - Ammo for weapon not found, full load, npc<%s>.\n",npc->GetInfo());
						weap->WeapLoadHolder();
					}
					else
					{
						AI_ReloadWeapon(npc,map,weap,ammo->GetId());
						break;
					}
				}
				else if(npc->IsRawParam(MODE_UNLIMITED_AMMO) && weap->WeapGetMaxAmmoCount()) weap->WeapLoadHolder();

			/************************************************************************/
			/* Step 2: Move to target                                               */
			/************************************************************************/
				bool is_can_walk=CritType::IsCanWalk(npc->GetCrType());
				DWORD best_dist=0,min_dist=0,max_dist=0;
				r0=targ->GetId(),r1=0,r2=0;
				if(!npc->RunPlane(REASON_ATTACK_DISTANTION,r0,r1,r2))
				{
					WriteLog("REASON_ATTACK_DISTANTION fail. Skip attack.\n");
					break;
				}
				if(plane!=npc->GetCurPlane()) break; // Validate plane

				best_dist=r0;
				min_dist=r1;
				max_dist=r2;

				if(r2<=0 || r1<0 || r0<0) // Run away
				{
					npc->NextPlane(REASON_RUN_AWAY);
					break;
				}

				if(max_dist<=0)
				{
					DWORD look=npc->GetLook();
					max_dist=npc->GetAttackDist(weap,use);
					if(max_dist>look) max_dist=look;
				}
				if(min_dist<=0) min_dist=1;

				if(min_dist>max_dist) min_dist=max_dist;
				best_dist=CLAMP(best_dist,min_dist,max_dist);

				WORD hx=npc->GetHexX();
				WORD hy=npc->GetHexY();
				WORD t_hx=targ->GetHexX();
				WORD t_hy=targ->GetHexY();
				WORD res_hx=t_hx;
				WORD res_hy=t_hy;
				bool is_run=plane->Attack.IsRun;
				bool is_range=(weap->Proto->Weapon.MaxDist[use]>2);

				TraceData trace;
				trace.TraceMap=map;
				trace.BeginHx=hx;
				trace.BeginHy=hy;
				trace.EndHx=t_hx;
				trace.EndHy=t_hy;
				trace.Dist=max_dist;
				trace.FindCr=targ;

				// Dirt
				MapMngr.TraceBullet(trace);
				if(!trace.IsCritterFounded)
				{
					if(is_can_walk) AI_MoveToCrit(npc,targ->GetId(),is_range?1+npc->GetMultihex():max_dist,max_dist+(is_range?0:5),is_run);
					else npc->NextPlane(REASON_CANT_WALK);
					break;
				}

				// Find better position
				if(is_range && is_can_walk)
				{
					trace.BeginHx=t_hx;
					trace.BeginHy=t_hy;
					trace.EndHx=hx;
					trace.EndHy=hy;
					trace.Dist=best_dist;
					trace.FindCr=npc;
					trace.IsCheckTeam=true;
					trace.BaseCrTeamId=npc->Data.Params[ST_TEAM_ID];
					MapMngr.TraceBullet(trace);
					if(!trace.IsCritterFounded)
					{
						WordPair last_passed;
						trace.LastPassed=&last_passed;
						trace.LastPassedSkipCritters=true;
						trace.FindCr=NULL;
						trace.Dist=best_dist;

						bool find_ok=false;
						float deq_step=360.0f/float(max_dist*6);
						float deq=deq_step;
						for(int i=1,j=best_dist*6;i<=j;i++) // Full round
						{
							trace.Angle=deq;
							MapMngr.TraceBullet(trace);
							if((trace.IsHaveLastPassed || (last_passed.first==npc->GetHexX() && last_passed.second==npc->GetHexY())) &&
								DistGame(t_hx,t_hy,last_passed.first,last_passed.second)>=min_dist)
							{
								res_hx=last_passed.first;
								res_hy=last_passed.second;
								find_ok=true;
								break;
							}

							if(!(i&1)) deq=deq_step*float((i+2)/2);
							else deq=-deq;
						}

						if(!find_ok) // Position not found
						{
							npc->NextPlane(REASON_POSITION_NOT_FOUND);
							break;
						}
						else if(hx!=res_hx || hy!=res_hy)
						{
							if(is_can_walk) AI_Move(npc,res_hx,res_hy,is_run,0,0);
							else npc->NextPlane(REASON_CANT_WALK);
							break;
						}
						
					}
				}
				// Find precision HtH attack
				else if(!is_range)
				{
					if(!CheckDist(hx,hy,t_hx,t_hy,max_dist))
					{
						if(!is_can_walk) npc->NextPlane(REASON_CANT_WALK);
						else if(max_dist>1 && best_dist==1) // Check busy ring
						{
							bool odd=(t_hx&1)!=0;
							short* rsx=(odd?SXOdd:SXEven);
							short* rsy=(odd?SYOdd:SYEven);
							int i;
							for(i=0;i<6;i++,rsx++,rsy++) if(map->IsHexPassed(t_hx+*rsx,t_hy+*rsy)) break;

							if(i==6) AI_MoveToCrit(npc,targ->GetId(),max_dist,0,is_run);
							else AI_MoveToCrit(npc,targ->GetId(),best_dist,0,is_run);
						}
						else AI_MoveToCrit(npc,targ->GetId(),best_dist,0,is_run);
						break;
					}
				}

			/************************************************************************/
			/* Step 3: Attack                                                       */
			/************************************************************************/

				r0=targ->GetId();
				r1=0;
				r2=0;
				if(!npc->RunPlane(REASON_ATTACK_USE_AIM,r0,r1,r2))
				{
					WriteLog("REASON_ATTACK_USE_AIM fail. Skip attack.\n");
					break;
				}

				if(r2)
				{
					npc->SetWait(r2);
					break;
				}

				if(r0!=use && weap->WeapIsUseAviable(r0)) use=r0;

				int aim=r1;
				if(!(CritType::IsCanAim(npc->GetCrType()) && !npc->IsRawParam(MODE_NO_AIM) && weap->WeapIsCanAim(use))) aim=0;

				weap->SetMode(MAKE_ITEM_MODE(use,aim));
				AI_Attack(npc,map,MAKE_ITEM_MODE(use,aim),targ->GetId());
			}
		/************************************************************************/
		/* Target not visible, try find by last stored position                 */
		/************************************************************************/
			else
			{
				if(is_busy) break;

				if((!plane->Attack.LastHexX && !plane->Attack.LastHexY) || !CritType::IsCanWalk(npc->GetCrType()))
				{
					Critter* targ_=CrMngr.GetCritter(plane->Attack.TargId,true);
					npc->NextPlane(REASON_TARGET_DISAPPEARED,targ_,NULL);
					break;
				}

				AI_Move(npc,plane->Attack.LastHexX,plane->Attack.LastHexY,plane->Attack.IsRun,1+npc->GetMultihex(),npc->GetLook()/2);
				plane->Attack.LastHexX=0;
				plane->Attack.LastHexY=0;
			}
		}
		break;
/************************************************************************/
/* ==================================================================== */
/* ========================   Walk   ================================== */
/* ==================================================================== */
/************************************************************************/
	case AI_PLANE_WALK:
		{
			if(is_busy) break;

			if(CheckDist(npc->GetHexX(),npc->GetHexY(),plane->Walk.HexX,plane->Walk.HexY,plane->Walk.Cut))
			{
				if(plane->Walk.Dir<6)
				{
					npc->Data.Dir=plane->Walk.Dir;
					npc->SendA_Dir();
				}
				else if(plane->Walk.Cut)
				{
					npc->Data.Dir=GetFarDir(npc->GetHexX(),npc->GetHexY(),plane->Walk.HexX,plane->Walk.HexY);
					npc->SendA_Dir();
				}

				npc->NextPlane(REASON_SUCCESS);
			}
			else
			{
				if(CritType::IsCanWalk(npc->GetCrType())) AI_Move(npc,plane->Walk.HexX,plane->Walk.HexY,plane->Walk.IsRun,plane->Walk.Cut,0);
				else npc->NextPlane(REASON_CANT_WALK);
			}
		}
		break;
/************************************************************************/
/* ==================================================================== */
/* ========================   Pick   ================================== */
/* ==================================================================== */
/************************************************************************/
	case AI_PLANE_PICK:
		{
			WORD hx=plane->Pick.HexX;
			WORD hy=plane->Pick.HexY;
			WORD pid=plane->Pick.Pid;
			DWORD use_item_id=plane->Pick.UseItemId;
			bool to_open=plane->Pick.ToOpen;
			bool is_run=plane->Pick.IsRun;

			Item* item=map->GetItemHex(hx,hy,pid,NULL); // Cheat
			if(!item || (item->IsDoor() && (to_open?item->LockerIsOpen():item->LockerIsClose())))
			{
				npc->NextPlane(REASON_SUCCESS);
				break;
			}

			if(use_item_id && !npc->GetItem(use_item_id,true))
			{
				npc->NextPlane(REASON_USE_ITEM_NOT_FOUND);
				break;
			}

			if(is_busy) break;

			ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
			if(!proto_item)
			{
				npc->NextPlane(REASON_SUCCESS);
				break;
			}

			int use_dist=npc->GetUseDist();
			if(!CheckDist(npc->GetHexX(),npc->GetHexY(),hx,hy,use_dist))
			{
				AI_Move(npc,hx,hy,is_run,use_dist,0);
			}
			else
			{
				npc->Data.Dir=GetFarDir(npc->GetHexX(),npc->GetHexY(),plane->Walk.HexX,plane->Walk.HexY);
				npc->SendA_Dir();
				if(AI_PickItem(npc,map,hx,hy,pid,use_item_id)) npc->NextPlane(REASON_SUCCESS);
			}
		}
		break;
/************************************************************************/
/* ==================================================================== */
/* ==================================================================== */
/* ==================================================================== */
/************************************************************************/
	default:
		{
			npc->NextPlane(REASON_SUCCESS);
		}
		break;
	}

//	if(map->IsTurnBasedOn && map->IsCritterTurn(npc) && !is_busy && (npc->IsNoPlanes() || !npc->GetCurPlane()->IsMove) && !npc->IsBusy() && !npc->IsWait()) map->EndCritterTurn();
}

bool FOServer::AI_Stay(Npc* npc, DWORD ms)
{
	npc->SetWait(ms);
	return true;
}

bool FOServer::AI_Move(Npc* npc, WORD hx, WORD hy, bool is_run, DWORD cut, DWORD trace)
{
	AIDataPlane* plane=npc->GetCurPlane();
	if(!plane) return false;
	plane->IsMove=true;
	plane->Move.HexX=hx;
	plane->Move.HexY=hy;
	plane->Move.IsRun=is_run;
	plane->Move.Cut=cut;
	plane->Move.Trace=trace;
	plane->Move.TargId=0;
	plane->Move.PathNum=0;
	plane->Move.Iter=0;
	return true;
}

bool FOServer::AI_MoveToCrit(Npc* npc, DWORD targ_id, DWORD cut, DWORD trace, bool is_run)
{
	AIDataPlane* plane=npc->GetCurPlane();
	if(!plane) return false;
	plane->IsMove=true;
	plane->Move.TargId=targ_id;
	plane->Move.IsRun=is_run;
	plane->Move.Cut=cut;
	plane->Move.Trace=trace;
	plane->Move.HexX=0;
	plane->Move.HexY=0;
	plane->Move.PathNum=0;
	plane->Move.Iter=0;
	return true;
}

bool FOServer::AI_MoveItem(Npc* npc, Map* map, BYTE from_slot, BYTE to_slot, DWORD item_id, DWORD count)
{
	bool is_castling=((from_slot==SLOT_HAND1 && to_slot==SLOT_HAND2) || (from_slot==SLOT_HAND2 && to_slot==SLOT_HAND1));
	DWORD ap_cost=(is_castling?0:npc->GetApCostMoveItemInventory());
	if(to_slot==0xFF) ap_cost=npc->GetApCostDropItem();
	CHECK_NPC_AP_R0(npc,map,ap_cost);

	npc->SetWait(GameOpt.Breaktime);
	return npc->MoveItem(from_slot,to_slot,item_id,count);
}

bool FOServer::AI_Attack(Npc* npc, Map* map, BYTE mode, DWORD targ_id)
{
	int ap_cost=(GameOpt.GetUseApCost?GameOpt.GetUseApCost(npc,npc->ItemSlotMain,mode):1);

	CHECK_NPC_AP_R0(npc,map,ap_cost);

	Critter* targ=npc->GetCritSelf(targ_id,false);
	if(!targ || !Act_Attack(npc,mode,targ_id)) return false;

	return true;
}

bool FOServer::AI_PickItem(Npc* npc, Map* map, WORD hx, WORD hy, WORD pid, DWORD use_item_id)
{
	CHECK_NPC_AP_R0(npc,map,npc->GetApCostPickItem());
	return Act_PickItem(npc,hx,hy,pid);
}

bool FOServer::AI_ReloadWeapon(Npc* npc, Map* map, Item* weap, DWORD ammo_id)
{
	int ap_cost=(GameOpt.GetUseApCost?GameOpt.GetUseApCost(npc,npc->ItemSlotMain,USE_RELOAD):1);
	CHECK_NPC_AP_R0(npc,map,ap_cost);
	return Act_Reload(npc,weap->GetId(),ammo_id);
}

bool FOServer::TransferAllNpc()
{
	WriteLog("Transfer all npc to game...\n");

	int errors=0;
	CrMap critters=CrMngr.GetCrittersNoLock();
	CrVec critters_groups;
	critters_groups.reserve(critters.size());

	// Move all critters to local maps and global map rules
	for(CrMapIt it=critters.begin(),end=critters.end();it!=end;++it)
	{
		Critter* cr=(*it).second;
		if(!cr->GetMap() && (cr->GetHexX() || cr->GetHexY()))
		{
			critters_groups.push_back(cr);
			continue;
		}

		Map* map=MapMngr.GetMap(cr->GetMap());

		if(cr->GetMap() && !map)
		{
			WriteLog("Map not found, critter<%s>, map id<%u>, hx<%u>, hy<%u>. Transfered to global map.\n",cr->GetInfo(),cr->GetMap(),cr->GetHexX(),cr->GetHexY());
			errors++;
			cr->SetMaps(0,0);
			cr->Data.HexX=0;
			cr->Data.HexY=0;
		}

		if(!MapMngr.AddCrToMap(cr,map,cr->GetHexX(),cr->GetHexY(),2))
		{
			WriteLog("Error parsing npc to map, critter<%s>, map id<%u>, hx<%u>, hy<%u>.\n",cr->GetInfo(),cr->GetMap(),cr->GetHexX(),cr->GetHexY());
			errors++;
			continue;
		}
		cr->Data.GlobalGroupUid--; // Restore group uid
	}

	// Move critters to global groups
	for(CrVecIt it=critters_groups.begin(),end=critters_groups.end();it!=end;++it)
	{
		Critter* cr=*it;
		if(!MapMngr.AddCrToMap(cr,NULL,cr->GetHexX(),cr->GetHexY(),2))
		{
			WriteLog("Error parsing npc to global group, critter<%s>, map id<%u>, hx<%u>, hy<%u>.\n",cr->GetInfo(),cr->GetMap(),cr->GetHexX(),cr->GetHexY());
			errors++;
			continue;
		}
	}

	// Process critters visible
	for(CrMapIt it=critters.begin(),end=critters.end();it!=end;++it)
	{
		Critter* cr=(*it).second;
		cr->ProcessVisibleItems();
	}

	WriteLog("Transfer npc complete. Errors<%d>.\n",errors);
	return true;
}

bool FOServer::Dialog_Compile(Npc* npc, Client* cl, const Dialog& base_dlg, Dialog& compiled_dlg)
{
	if(base_dlg.Id<2)
	{
		WriteLog(__FUNCTION__" - Wrong dialog id<%u>.\n",base_dlg.Id);
		return false;
	}
	compiled_dlg=base_dlg;

	for(AnswersVecIt it_a=compiled_dlg.Answers.begin();it_a!=compiled_dlg.Answers.end();)
	{
		if(!Dialog_CheckDemand(npc,cl,*it_a,false)) 
			it_a=compiled_dlg.Answers.erase(it_a);
		else
			it_a++;
	}

	if(!GameOpt.NoAnswerShuffle && !compiled_dlg.IsNoShuffle()) std::random_shuffle(compiled_dlg.Answers.begin(),compiled_dlg.Answers.end());
	return true;
}


bool FOServer::Dialog_CheckDemand(Npc* npc, Client* cl, DialogAnswer& answer, bool recheck)
{
	if(!answer.Demands.size()) return true;

	Critter* master;
	Critter* slave;

	for(DemandResultVecIt it=answer.Demands.begin(),end=answer.Demands.end();it!=end;++it)
	{
		DemandResult& demand=*it;
		if(recheck && demand.NoRecheck) continue;

		switch(demand.Who)
		{
		case 'p': master=cl; slave=npc; break;
		case 'n': master=npc; slave=cl; break;
		default: continue;
		}

		if(!master) continue;

		WORD index=demand.ParamId;
		switch(demand.Type)
		{
		case DR_PARAM:
			{
				int val=master->GetParam(index);
				if(index==ST_INTELLECT) val+=master->GetRawParam(PE_SMOOTH_TALKER)*2;
				if(index>=REPUTATION_BEGIN && index<=REPUTATION_END && master->Data.Params[index]==0x80000000) master->Data.Params[index]=0;
				switch(demand.Op)
				{
				case '>': if(val >demand.Value) continue; break;
				case '<': if(val <demand.Value) continue; break;
				case '=': if(val==demand.Value) continue; break;
				case '!': if(val!=demand.Value) continue; break;
				case '}': if(val>=demand.Value) continue; break;
				case '{': if(val<=demand.Value) continue; break;
				default: break;
				}
			}
			break; // or
		case DR_VAR:
			{
				TemplateVar* tvar=VarMngr.GetTemplateVar(index);
				if(!tvar) break;

				DWORD master_id=0,slave_id=0;
				if(tvar->Type==VAR_LOCAL) master_id=master->GetId();
				else if(tvar->Type==VAR_UNICUM)
				{
					master_id=master->GetId();
					slave_id=(slave?slave->GetId():0);
				}
				else if(tvar->Type==VAR_LOCAL_LOCATION)
				{
					Map* map=MapMngr.GetMap(master->GetMap(),false);
					if(map) master_id=map->GetLocation(false)->GetId();
				}
				else if(tvar->Type==VAR_LOCAL_MAP) master_id=master->GetMap();
				else if(tvar->Type==VAR_LOCAL_ITEM) master_id=master->ItemSlotMain->GetId();

				if(VarMngr.CheckVar(index,master_id,slave_id,demand.Op,demand.Value)) continue;
			}
			break; // or
		case DR_ITEM:
			switch(demand.Op)
			{
			case '>': if(master->CountItemPid(index) >demand.Value) continue; break;
			case '<': if(master->CountItemPid(index) <demand.Value) continue; break;
			case '=': if(master->CountItemPid(index)==demand.Value) continue; break;
			case '!': if(master->CountItemPid(index)!=demand.Value) continue; break;
			case '}': if(master->CountItemPid(index)>=demand.Value) continue; break;
			case '{': if(master->CountItemPid(index)<=demand.Value) continue; break;
			default: break;
			}
			break; // or
		case DR_SCRIPT:
			GameOpt.DialogDemandRecheck=recheck;
			cl->Talk.Locked=true;
			if(DialogScriptDemand(demand,master,slave))
			{
				cl->Talk.Locked=false;
				continue;
			}
			cl->Talk.Locked=false;
			break; // or
		case DR_OR:
			return true;
		default:
			continue;
		}

		bool or=false;
		for(;it!=end;++it)
		{
			if((*it).Type==DR_OR)
			{
				or=true;
				break;
			}
		}
		if(!or) return false;
	}

	return true;
} 

DWORD FOServer::Dialog_UseResult(Npc* npc, Client* cl, DialogAnswer& answer)
{
	if(!answer.Results.size()) return 0;

	DWORD force_dialog=0;
	Critter* master;
	Critter* slave;

	for(DemandResultVecIt it=answer.Results.begin(),end=answer.Results.end();it!=end;++it)
	{
		DemandResult& result=*it;

		switch(result.Who)
		{
		case 'p': master=cl; slave=npc; break;
		case 'n': master=npc; slave=cl; break;
		default: continue;
		}

		if(!master) continue;

		WORD index=result.ParamId;
		switch(result.Type)
		{
		case DR_PARAM:
			master->ChangeParam(index);
			if(index>=REPUTATION_BEGIN && index<=REPUTATION_END && master->Data.Params[index]==0x80000000) master->Data.Params[index]=0;
			switch(result.Op)
			{
			case '+': master->Data.Params[index]+=result.Value; break;
			case '-': master->Data.Params[index]-=result.Value; break;
			case '*': master->Data.Params[index]*=result.Value; break;
			case '/': master->Data.Params[index]/=result.Value; break;
			case '=': master->Data.Params[index] =result.Value; break;
			default: break;
			}
			continue;
		case DR_VAR:
			{
				TemplateVar* tvar=VarMngr.GetTemplateVar(index);
				if(!tvar) break;

				DWORD master_id=0,slave_id=0;
				if(tvar->Type==VAR_LOCAL) master_id=master->GetId();
				else if(tvar->Type==VAR_UNICUM)
				{
					master_id=master->GetId();
					slave_id=(slave?slave->GetId():0);
				}
				else if(tvar->Type==VAR_LOCAL_LOCATION)
				{
					Map* map=MapMngr.GetMap(master->GetMap(),false);
					if(map) master_id=map->GetLocation(false)->GetId();
				}
				else if(tvar->Type==VAR_LOCAL_MAP) master_id=master->GetMap();
				else if(tvar->Type==VAR_LOCAL_ITEM) master_id=master->ItemSlotMain->GetId();

				VarMngr.ChangeVar(index,master_id,slave_id,result.Op,result.Value);
			}
			continue;
		case DR_ITEM:
			{
				int cur_count=master->CountItemPid(index);
				int need_count=cur_count;

				switch(result.Op)
				{
				case '+': need_count+=result.Value; break;
				case '-': need_count-=result.Value; break;
				case '*': need_count*=result.Value; break;
				case '/': need_count/=result.Value; break;
				case '=': need_count =result.Value; break;
				default: continue;
				}

				if(need_count<0) need_count=0;
				if(cur_count==need_count) continue;
				ItemMngr.SetItemCritter(master,index,need_count);
			}
			continue;
		case DR_SCRIPT:
			cl->Talk.Locked=true;
			force_dialog=DialogScriptResult(result,master,slave);
			cl->Talk.Locked=false;
			continue;
		default:
			continue;
		}
	}

	return force_dialog;
}

void FOServer::Dialog_Begin(Client* cl, Npc* npc, DWORD dlg_pack_id, WORD hx, WORD hy, bool ignore_distance)
{
	if(cl->Talk.Locked)
	{
		WriteLog(__FUNCTION__" - Dialog locked, client<%s>.\n",cl->GetInfo());
		return;
	}
	if(cl->Talk.TalkType!=TALK_NONE) cl->CloseTalk();

	DialogPack* dialog_pack=NULL;
	DialogsVec* dialogs=NULL;

	// Talk with npc
	if(npc)
	{
		if(!dlg_pack_id)
		{
			int npc_dlg_id=npc->Data.Params[ST_DIALOG_ID];
			if(npc_dlg_id<=0) return;
			dlg_pack_id=npc_dlg_id;
		}

		if(!ignore_distance)
		{
			if(cl->GetMap()!=npc->GetMap())
			{
				//	cl->Send_Text(cl,"Differense maps, you are hack the game?.",SAY_NETMSG);
				WriteLog(__FUNCTION__" - Difference maps, npc<%s>, client<%s>.\n",npc->GetInfo(),cl->GetInfo());
				return;
			}

			DWORD talk_dist=npc->GetTalkDistance(cl);
			if(!CheckDist(cl->GetHexX(),cl->GetHexY(),npc->GetHexX(),npc->GetHexY(),talk_dist))
			{
				cl->Send_XY(cl);
				cl->Send_XY(npc);
				cl->Send_TextMsg(cl,STR_DIALOG_DIST_TOO_LONG,SAY_NETMSG,TEXTMSG_GAME);
				WriteLog(__FUNCTION__" - Wrong distance to npc<%s>, client<%s>.\n",npc->GetInfo(),cl->GetInfo());
				return;
			}

			Map* map=MapMngr.GetMap(cl->GetMap());
			if(!map)
			{
				cl->Send_TextMsg(cl,STR_FINDPATH_AIMBLOCK,SAY_NETMSG,TEXTMSG_GAME);
				return;
			}

			if(map->IsTurnBasedOn)
			{
				cl->Send_TextMsg(cl,STR_DIALOG_NPC_BUSY,SAY_NETMSG,TEXTMSG_GAME);
				WriteLog(__FUNCTION__" - Map is in turn based state, npc<%s>, client<%s>.\n",npc->GetInfo(),cl->GetInfo());
				return;
			}

			TraceData trace;
			trace.TraceMap=map;
			trace.BeginHx=cl->GetHexX();
			trace.BeginHy=cl->GetHexY();
			trace.EndHx=npc->GetHexX();
			trace.EndHy=npc->GetHexY();
			trace.Dist=talk_dist;
			trace.FindCr=npc;
			MapMngr.TraceBullet(trace);
			if(!trace.IsCritterFounded)
			{
				cl->Send_TextMsg(cl,STR_FINDPATH_AIMBLOCK,SAY_NETMSG,TEXTMSG_GAME);
				return;
			}
		}

		if(!npc->IsLife())
		{
			cl->Send_TextMsg(cl,STR_DIALOG_NPC_NOT_LIFE,SAY_NETMSG,TEXTMSG_GAME);
			WriteLog(__FUNCTION__" - Npc<%s> bad condition, client<%s>.\n",npc->GetInfo(),cl->GetInfo());
			return;
		}

		if(!npc->IsFreeToTalk())
		{
			cl->Send_TextMsg(cl,STR_DIALOG_MANY_TALKERS,SAY_NETMSG,TEXTMSG_GAME);
			return;
		}

		if(!npc->EventTalk(cl,true,npc->GetTalkedPlayers()+1))
		{
			// Message must processed in script
			return;
		}

		if(npc->IsPlaneNoTalk())
		{
			cl->Send_TextMsg(cl,STR_DIALOG_NPC_BUSY,SAY_NETMSG,TEXTMSG_GAME);
			return;
		}

		dialog_pack=DlgMngr.GetDialogPack(dlg_pack_id);
		dialogs=(dialog_pack?&dialog_pack->Dialogs:NULL);
		if(!dialogs || !dialogs->size()) return;
// 		{
// 			WriteLog(__FUNCTION__" - No dialogs, client<%s>.\n",cl->GetInfo());
// 			return;
// 		}

		if(!ignore_distance)
		{
			int dir=GetFarDir(cl->GetHexX(),cl->GetHexY(),npc->GetHexX(),npc->GetHexY());
			npc->Data.Dir=ReverseDir(dir);
			npc->SendA_Dir();
			cl->Data.Dir=dir;
			cl->SendA_Dir();
			cl->Send_Dir(cl);
		}
	}
	// Talk with hex
	else
	{
//		Map* map=MapMngr.GetMap(cl->GetMap());
//		if(!map || hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY())
//		{
//			WriteLog(__FUNCTION__" - Invalid hexes value, hx<%u>, hy<%u>, client<%s>.\n",hx,hy,cl->GetInfo());
//			return;
//		}

		if(!ignore_distance && !CheckDist(cl->GetHexX(),cl->GetHexY(),hx,hy,GameOpt.TalkDistance+cl->GetMultihex()))
		{
			cl->Send_XY(cl);
			cl->Send_TextMsg(cl,STR_DIALOG_DIST_TOO_LONG,SAY_NETMSG,TEXTMSG_GAME);
			WriteLog(__FUNCTION__" - Wrong distance to hexes, hx<%u>, hy<%u>, client<%s>.\n",hx,hy,cl->GetInfo());
			return;
		}

		dialog_pack=DlgMngr.GetDialogPack(dlg_pack_id);
		dialogs=(dialog_pack?&dialog_pack->Dialogs:NULL);
		if(!dialogs || !dialogs->size())
		{
			//	Map* map=MapMngr.GetMap(cl->GetMap());
			//	if(map) map->SetTextMsg(hx,hy,0,TEXTMSG_GAME,STR_DIALOG_NO_DIALOGS);
			WriteLog(__FUNCTION__" - No dialogs, hx<%u>, hy<%u>, client<%s>.\n",hx,hy,cl->GetInfo());
			return;
		}
	}

	// Predialogue installations
	DialogsVecIt it_d=dialogs->begin();
	DWORD go_dialog=DWORD(-1);
	AnswersVecIt it_a=(*it_d).Answers.begin();
	for(;it_a!=(*it_d).Answers.end();++it_a)
	{
		if(Dialog_CheckDemand(npc,cl,*it_a,false)) go_dialog=(*it_a).Link;
		if(go_dialog!=DWORD(-1)) break;
	}

	if(go_dialog==DWORD(-1))
	{
		// cl->Send_Str(cl,STR_DIALOG_PRE_INST_FAIL,SAY_NETMSG,TEXTMSG_GAME);
		// WriteLog(__FUNCTION__" - Invalid predialogue installations, client<%s>, dialog pack<%u>.\n",cl->GetInfo(),dialog_pack->PackId);
		return;
	}

	// Use result
	DWORD force_dialog=Dialog_UseResult(npc,cl,(*it_a));
	if(force_dialog)
	{
		if(force_dialog==DWORD(-1)) return;
		go_dialog=force_dialog;
	}

	// Find dialog
	it_d=std::find(dialogs->begin(),dialogs->end(),go_dialog);
	if(it_d==dialogs->end())
	{
		cl->Send_TextMsg(cl,STR_DIALOG_FROM_LINK_NOT_FOUND,SAY_NETMSG,TEXTMSG_GAME);
		WriteLog(__FUNCTION__" - Dialog from link<%u> not found, client<%s>, dialog pack<%u>.\n",go_dialog,cl->GetInfo(),dialog_pack->PackId);
		return;
	}

	// Compile
	if(!Dialog_Compile(npc,cl,*it_d,cl->Talk.CurDialog))
	{
		cl->Send_TextMsg(cl,STR_DIALOG_COMPILE_FAIL,SAY_NETMSG,TEXTMSG_GAME);
		WriteLog(__FUNCTION__" - Dialog compile fail, client<%s>, dialog pack<%u>.\n",cl->GetInfo(),dialog_pack->PackId);
		return;
	}

	if(npc)
	{
		cl->Talk.TalkType=TALK_WITH_NPC;
		cl->Talk.TalkNpc=npc->GetId();
	}
	else
	{
		cl->Talk.TalkType=TALK_WITH_HEX;
		cl->Talk.TalkHexMap=cl->GetMap();
		cl->Talk.TalkHexX=hx;
		cl->Talk.TalkHexY=hy;
	}

	cl->Talk.DialogPackId=dlg_pack_id;
	cl->Talk.LastDialogId=go_dialog;
	cl->Talk.StartTick=Timer::GameTick();
	cl->Talk.TalkTime=max(cl->GetRawParam(SK_SPEECH)*1000,GameOpt.DlgTalkMinTime);
	cl->Talk.Barter=false;
	cl->Talk.IgnoreDistance=ignore_distance;

	// Get lexems
	cl->Talk.Lexems.clear();
	if(cl->Talk.CurDialog.DlgScript>NOT_ANSWER_BEGIN_BATTLE && Script::PrepareContext(cl->Talk.CurDialog.DlgScript,CALL_FUNC_STR,cl->GetInfo()))
	{
		CScriptString* lexems=new CScriptString();
		Script::SetArgObject(cl);
		Script::SetArgObject(npc);
		Script::SetArgObject(lexems);
		cl->Talk.Locked=true;
		if(Script::RunPrepared() && lexems->length()<=MAX_DLG_LEXEMS_TEXT) cl->Talk.Lexems=lexems->c_str();
		else cl->Talk.Lexems="";
		cl->Talk.Locked=false;
		lexems->Release();
	}

	// On head text
	if(!cl->Talk.CurDialog.Answers.size())
	{
		if(npc)
		{
			npc->SendAA_MsgLex(npc->VisCr,cl->Talk.CurDialog.TextId,SAY_NORM_ON_HEAD,TEXTMSG_DLG,cl->Talk.Lexems.c_str());
		}
		else
		{
			Map* map=MapMngr.GetMap(cl->GetMap());
			if(map) map->SetTextMsg(hx,hy,0,TEXTMSG_DLG,cl->Talk.CurDialog.TextId);
		}

		cl->CloseTalk();
		return;
	}

	// Open dialog window
	cl->Send_Talk();
	SetScore(SCORE_SPEAKER,cl,5);
}

void FOServer::Process_Dialog(Client* cl, bool is_say)
{
	BYTE is_npc;
	DWORD id_npc_talk;
	BYTE num_answer;
	char str[MAX_SAY_NPC_TEXT+1];

	cl->Bin >> is_npc;
	cl->Bin >> id_npc_talk;

	if(!is_say)
	{
		cl->Bin >> num_answer;
	}
	else
	{
		cl->Bin.Pop(str,MAX_SAY_NPC_TEXT);
		str[MAX_SAY_NPC_TEXT]=0;
		if(!strlen(str))
		{
			WriteLog(__FUNCTION__" - Say text length is zero, client<%s>.\n",cl->GetInfo());
			return;
		}
	}

	if(cl->IsRawParam(MODE_HIDE))
	{
		cl->ChangeParam(MODE_HIDE);
		cl->Data.Params[MODE_HIDE]=0;
	}
	cl->ProcessTalk(true);

	Npc* npc=NULL;
	DialogPack* dialog_pack=NULL;
	DialogsVec* dialogs=NULL;

	if(is_npc)
	{
		// Find npc
		npc=CrMngr.GetNpc(id_npc_talk,true);
		if(!npc)
		{
			cl->Send_TextMsg(cl,STR_DIALOG_NPC_NOT_FOUND,SAY_NETMSG,TEXTMSG_GAME);
			cl->CloseTalk();
			WriteLog(__FUNCTION__" - Npc not found, client<%s>.\n",cl->GetInfo());
			return;
		}

		// Close another talk
		if(cl->Talk.TalkType!=TALK_WITH_NPC || cl->Talk.TalkNpc!=npc->GetId()) cl->CloseTalk();

		// Begin dialog
		if(cl->Talk.TalkType==TALK_NONE)
		{
			Dialog_Begin(cl,npc,0,0,0,false);
			return;
		}

		// Set dialogs
		dialog_pack=DlgMngr.GetDialogPack(cl->Talk.DialogPackId);
		dialogs=dialog_pack?&dialog_pack->Dialogs:NULL;
		if(!dialogs || !dialogs->size())
		{
			cl->CloseTalk();
			npc->SendAA_Msg(npc->VisCr,STR_DIALOG_NO_DIALOGS,SAY_NORM,TEXTMSG_GAME);
			WriteLog(__FUNCTION__" - No dialogs, npc<%s>, client<%s>.\n",npc->GetInfo(),cl->GetInfo());
			return;
		}
	}
	else
	{
		// Set dialogs
		dialog_pack=DlgMngr.GetDialogPack(id_npc_talk);
		dialogs=dialog_pack?&dialog_pack->Dialogs:NULL;
		if(!dialogs || !dialogs->size())
		{
			Map* map=MapMngr.GetMap(cl->GetMap());
			if(map) map->SetTextMsg(cl->Talk.TalkHexX,cl->Talk.TalkHexY,0,TEXTMSG_GAME,STR_DIALOG_NO_DIALOGS);
			WriteLog(__FUNCTION__" - No dialogs, hx<%u>, hy<%u>, client<%s>.\n",cl->Talk.TalkHexX,cl->Talk.TalkHexY,cl->GetInfo());
			return;
		}
	}

	// Continue dialog
	Dialog* cur_dialog=&cl->Talk.CurDialog;
	DWORD last_dialog=cur_dialog->Id;
	DWORD dlg_id;

	if(!cl->Talk.Barter)
	{
		// Say about
		if(is_say)
		{
			if(cur_dialog->DlgScript<=NOT_ANSWER_BEGIN_BATTLE) return;

			CScriptString* str_=new CScriptString(str);
			if(!Script::PrepareContext(cur_dialog->DlgScript,CALL_FUNC_STR,cl->GetInfo())) return;
			Script::SetArgObject(cl);
			Script::SetArgObject(npc);
			Script::SetArgObject(str_);

			cl->Talk.Locked=true;
			DWORD force_dialog=0;
			if(Script::RunPrepared() && cur_dialog->RetVal) force_dialog=Script::GetReturnedDword();
			cl->Talk.Locked=false;
			str_->Release();

			if(force_dialog)
			{
				dlg_id=force_dialog;
				goto label_ForceDialog;
			}
			return;
		}

		// Barter
		if(num_answer==ANSWER_BARTER) goto label_Barter;

		// Refresh
		if(num_answer==ANSWER_BEGIN)
		{
			cl->Send_Talk();
			return;
		}

		// End
		if(num_answer==ANSWER_END)
		{
			cl->CloseTalk();
			return;
		}

		// Invalid answer
		if(num_answer>=cur_dialog->Answers.size())
		{
			WriteLog(__FUNCTION__" - Wrong number of answer<%u>, client<%s>.\n",num_answer,cl->GetInfo());
			cl->Send_Talk(); // Refresh
			return;
		}

		// Find answer
		DialogAnswer& answer=*(cur_dialog->Answers.begin()+num_answer);

		// Check demand again
		if(!Dialog_CheckDemand(npc,cl,answer,true))
		{
			WriteLog(__FUNCTION__" - Secondary check of dialog demands fail, client<%s>.\n",cl->GetInfo());
			cl->CloseTalk(); // End
			return;
		}

		// Use result
		DWORD force_dialog=Dialog_UseResult(npc,cl,answer);
		if(force_dialog) dlg_id=force_dialog;
		else dlg_id=answer.Link;

label_ForceDialog:
		// Special links
		switch(dlg_id)
		{
		case -3:
		case DIALOG_BARTER:
label_Barter:
			if(!npc)
			{
				cl->Send_TextMsg(cl,STR_BARTER_NO_BARTER_MODE,SAY_DIALOG,TEXTMSG_GAME);
				return;
			}
			if(cur_dialog->DlgScript!=NOT_ANSWER_CLOSE_DIALOG && !npc->IsRawParam(MODE_DLG_SCRIPT_BARTER))
			{
				cl->Send_TextMsg(npc,STR_BARTER_NO_BARTER_NOW,SAY_DIALOG,TEXTMSG_GAME);
				return;
			}
			if(npc->IsRawParam(MODE_NO_BARTER))
			{
				cl->Send_TextMsg(npc,STR_BARTER_NO_BARTER_MODE,SAY_DIALOG,TEXTMSG_GAME);
				return;
			}
			if(!npc->EventBarter(cl,true,npc->GetBarterPlayers()+1))
			{
				// Message must processed in script
				return;
			}

			cl->Talk.Barter=true;
			cl->Talk.StartTick=Timer::GameTick();
			cl->Talk.TalkTime=max(cl->GetRawParam(SK_BARTER)*1000,GameOpt.DlgBarterMinTime);
			cl->Send_ContainerInfo(npc,TRANSFER_CRIT_BARTER,true);
			return;
		case -2:
		case DIALOG_BACK:
			if(cl->Talk.LastDialogId)
			{
				dlg_id=cl->Talk.LastDialogId;
				break;
			}
		case -1:
		case DIALOG_END:
			cl->CloseTalk();
			return;
		case -4:
		case DIALOG_ATTACK:
			cl->CloseTalk();
			if(npc) npc->SetTarget(REASON_FROM_DIALOG,cl,GameOpt.DeadHitPoints,false);
			return;
		default:
			break;
		}
	}
	else
	{
		cl->Talk.Barter=false;
		dlg_id=cur_dialog->Id;
		if(npc) npc->EventBarter(cl,false,npc->GetBarterPlayers());
	}

	// Find dialog
	DialogsVecIt it_d=std::find(dialogs->begin(),dialogs->end(),dlg_id);
	if(it_d==dialogs->end())
	{
		cl->CloseTalk();
		cl->Send_TextMsg(cl,STR_DIALOG_FROM_LINK_NOT_FOUND,SAY_NETMSG,TEXTMSG_GAME);
		WriteLog(__FUNCTION__" - Dialog from link<%u> not found, client<%s>, dialog pack<%u>.\n",dlg_id,cl->GetInfo(),dialog_pack->PackId);
		return;
	}

	// Compile
	if(!Dialog_Compile(npc,cl,*it_d,cl->Talk.CurDialog))
	{
		cl->CloseTalk();
		cl->Send_TextMsg(cl,STR_DIALOG_COMPILE_FAIL,SAY_NETMSG,TEXTMSG_GAME);
		WriteLog(__FUNCTION__" - Dialog compile fail, client<%s>, dialog pack<%u>.\n",cl->GetInfo(),dialog_pack->PackId);
		return;
	}

	if(dlg_id!=last_dialog) cl->Talk.LastDialogId=last_dialog;

	// Get lexems
	cl->Talk.Lexems.clear();
	if(cl->Talk.CurDialog.DlgScript>NOT_ANSWER_BEGIN_BATTLE && Script::PrepareContext(cl->Talk.CurDialog.DlgScript,CALL_FUNC_STR,cl->GetInfo()))
	{
		CScriptString* lexems=new CScriptString();
		Script::SetArgObject(cl);
		Script::SetArgObject(npc);
		Script::SetArgObject(lexems);
		cl->Talk.Locked=true;
		if(Script::RunPrepared() && lexems->length()<=MAX_DLG_LEXEMS_TEXT) cl->Talk.Lexems=lexems->c_str();
		else cl->Talk.Lexems="";
		cl->Talk.Locked=false;
		lexems->Release();
	}

	// On head text
	if(!cl->Talk.CurDialog.Answers.size())
	{
		if(npc)
		{
			npc->SendAA_MsgLex(npc->VisCr,cl->Talk.CurDialog.TextId,SAY_NORM_ON_HEAD,TEXTMSG_DLG,cl->Talk.Lexems.c_str());
		}
		else
		{
			Map* map=MapMngr.GetMap(cl->GetMap());
			if(map) map->SetTextMsg(cl->Talk.TalkHexX,cl->Talk.TalkHexY,0,TEXTMSG_DLG,cl->Talk.CurDialog.TextId);
		}

		cl->CloseTalk();
		return;
	}

	cl->Talk.StartTick=Timer::GameTick();
	cl->Talk.TalkTime=max(cl->GetRawParam(SK_SPEECH)*1000,GameOpt.DlgTalkMinTime);
	cl->Send_Talk();
	SetScore(SCORE_SPEAKER,cl,5);
}

void FOServer::Process_Barter(Client* cl)
{
	DWORD msg_len;
	DWORD id_npc_talk;
	WORD sale_count;
	DwordVec sale_item_id;
	DwordVec sale_item_count;
	WORD buy_count;
	DwordVec buy_item_id;
	DwordVec buy_item_count;
	DWORD item_id;
	DWORD item_count;
	DWORD same_id=0;

	cl->Bin >> msg_len;
	cl->Bin >> id_npc_talk;

	cl->Bin >> sale_count;
	for(int i=0;i<sale_count;++i)
	{
		cl->Bin >> item_id;
		cl->Bin >> item_count;

		if(std::find(sale_item_id.begin(),sale_item_id.end(),item_id)!=sale_item_id.end()) same_id++;
		sale_item_id.push_back(item_id);
		sale_item_count.push_back(item_count);
	}

	cl->Bin >> buy_count;
	for(int i=0;i<buy_count;++i)
	{
		cl->Bin >> item_id;
		cl->Bin >> item_count;

		if(std::find(buy_item_id.begin(),buy_item_id.end(),item_id)!=buy_item_id.end()) same_id++;
		buy_item_id.push_back(item_id);
		buy_item_count.push_back(item_count);
	}

	// Check
	if(!cl->GetMap())
	{
		WriteLog(__FUNCTION__" - Player try to barter from global map, client<%s>.\n",cl->GetInfo());
		cl->Send_ContainerInfo();
		return;
	}

	Npc* npc=CrMngr.GetNpc(id_npc_talk,true);
	if(!npc)
	{
		WriteLog(__FUNCTION__" - Npc not found, client<%s>.\n",cl->GetInfo());
		cl->Send_ContainerInfo();
		return;
	}

	bool is_free=(npc->Data.Params[ST_FREE_BARTER_PLAYER]==cl->GetId());
	if(!sale_count && !is_free)
	{
		WriteLog(__FUNCTION__" - Player nothing for sale, client<%s>.\n",cl->GetInfo());
		cl->Send_ContainerInfo();
		return;
	}

	if(same_id)
	{
		WriteLog(__FUNCTION__" - Same item id found, client<%s>, npc<%s>, same count<%u>.\n",cl->GetInfo(),npc->GetInfo(),same_id);
		cl->Send_TextMsg(cl,STR_BARTER_BAD_OFFER,SAY_DIALOG,TEXTMSG_GAME);
		cl->Send_ContainerInfo(npc,TRANSFER_CRIT_BARTER,false);
		return;
	}

	if(!npc->IsLife())
	{
		WriteLog(__FUNCTION__" - Npc busy or dead, client<%s>, npc<%s>.\n",cl->GetInfo(),npc->GetInfo());
		cl->Send_ContainerInfo();
		return;
	}

	if(npc->IsRawParam(MODE_NO_BARTER))
	{
		WriteLog(__FUNCTION__" - Npc has NoBarterMode, client<%s>, npc<%s>.\n",cl->GetInfo(),npc->GetInfo());
		cl->Send_ContainerInfo();
		return;
	}

	if(cl->GetMap()!=npc->GetMap())
	{
		WriteLog(__FUNCTION__" - Difference maps, client<%s>, npc<%s>.\n",cl->GetInfo(),npc->GetInfo());
		cl->Send_ContainerInfo();
		return;
	}

	if(!CheckDist(cl->GetHexX(),cl->GetHexY(),npc->GetHexX(),npc->GetHexY(),npc->GetTalkDistance(cl)))
	{
		WriteLog(__FUNCTION__" - Wrong distance, client<%s>, npc<%s>.\n",cl->GetInfo(),npc->GetInfo());
		cl->Send_XY(cl);
		cl->Send_XY(npc);
		cl->Send_ContainerInfo();
		return;
	}

	if(cl->Talk.TalkType!=TALK_WITH_NPC || cl->Talk.TalkNpc!=npc->GetId() || !cl->Talk.Barter)
	{
		WriteLog(__FUNCTION__" - Dialog is closed or not beginning, client<%s>, npc<%s>.\n",cl->GetInfo(),npc->GetInfo());
		cl->Send_ContainerInfo();
		return;
	}

	// Check cost
	int barter_k=npc->GetRawParam(SK_BARTER)-cl->GetRawParam(SK_BARTER);
	barter_k=CLAMP(barter_k,5,95);
	int sale_cost=0;
	int buy_cost=0;
	DWORD sale_weight=0;
	DWORD buy_weight=0;
	DWORD sale_volume=0;
	DWORD buy_volume=0;
	ItemPtrVec sale_items;
	ItemPtrVec buy_items;

	for(int i=0;i<sale_count;++i)
	{
		Item* item=cl->GetItem(sale_item_id[i],true);
		if(!item)
		{
			WriteLog(__FUNCTION__" - Sale item not found, id<%u>, client<%s>, npc<%s>.\n",sale_item_id[i],cl->GetInfo(),npc->GetInfo());
			cl->Send_TextMsg(cl,STR_BARTER_SALE_ITEM_NOT_FOUND,SAY_DIALOG,TEXTMSG_GAME);
			cl->Send_ContainerInfo(npc,TRANSFER_CRIT_BARTER,false);
			return;
		}

		if(item->IsBadItem())
		{
			cl->Send_TextMsg(cl,STR_BARTER_BAD_OFFER,SAY_DIALOG,TEXTMSG_GAME);
			cl->Send_ContainerInfo(npc,TRANSFER_CRIT_BARTER,false);
			return;
		}

		if(!sale_item_count[i] || sale_item_count[i]>item->GetCount())
		{
			WriteLog(__FUNCTION__" - Sale item count error, id<%u>, count<%u>, real count<%u>, client<%s>, npc<%s>.\n",sale_item_id[i],sale_item_count[i],item->GetCount(),cl->GetInfo(),npc->GetInfo());
			cl->Send_ContainerInfo();
			return;
		}

		if(sale_item_count[i]>1 && !item->IsGrouped())
		{
			WriteLog(__FUNCTION__" - Sale item is not grouped, id<%u>, count<%u>, client<%s>, npc<%s>.\n",sale_item_id[i],sale_item_count[i],cl->GetInfo(),npc->GetInfo());
			cl->Send_ContainerInfo();
			return;
		}

		DWORD base_cost=item->GetCost1st();
		if(GameOpt.CustomItemCost)
		{
			if(Script::PrepareContext(ServerFunctions.ItemCost,CALL_FUNC_STR,cl->GetInfo()))
			{
				Script::SetArgObject(item);
				Script::SetArgObject(cl);
				Script::SetArgObject(npc);
				Script::SetArgBool(true);
				if(Script::RunPrepared()) base_cost=Script::GetReturnedDword();
			}
		}
		else
		{
			base_cost=base_cost*(100-barter_k)/100;
			if(!base_cost) base_cost++;
		}
		sale_cost+=base_cost*sale_item_count[i];
		sale_weight+=item->Proto->Weight*sale_item_count[i];
		sale_volume+=item->Proto->Volume*sale_item_count[i];
		sale_items.push_back(item);
	}

	for(int i=0;i<buy_count;++i)
	{
		Item* item=npc->GetItem(buy_item_id[i],true);
		if(!item)
		{
			WriteLog(__FUNCTION__" - Buy item not found, id<%u>, client<%s>, npc<%s>.\n",buy_item_id[i],cl->GetInfo(),npc->GetInfo());
			cl->Send_TextMsg(cl,STR_BARTER_BUY_ITEM_NOT_FOUND,SAY_DIALOG,TEXTMSG_GAME);
			cl->Send_ContainerInfo(npc,TRANSFER_CRIT_BARTER,false);
			return;
		}

		if(!buy_item_count[i] || buy_item_count[i]>item->GetCount())
		{
			WriteLog(__FUNCTION__" - Buy item count error, id<%u>, count<%u>, real count<%u>, client<%s>, npc<%s>.\n",buy_item_id[i],buy_item_count[i],item->GetCount(),cl->GetInfo(),npc->GetInfo());
			cl->Send_ContainerInfo();
			return;
		}

		if(buy_item_count[i]>1 && !item->IsGrouped())
		{
			WriteLog(__FUNCTION__" - Buy item is not grouped, id<%u>, count<%u>, client<%s>, npc<%s>.\n",buy_item_id[i],buy_item_count[i],cl->GetInfo(),npc->GetInfo());
			cl->Send_ContainerInfo();
			return;
		}

		DWORD base_cost=item->GetCost1st();
		if(GameOpt.CustomItemCost)
		{
			if(Script::PrepareContext(ServerFunctions.ItemCost,CALL_FUNC_STR,cl->GetInfo()))
			{
				Script::SetArgObject(item);
				Script::SetArgObject(cl);
				Script::SetArgObject(npc);
				Script::SetArgBool(false);
				if(Script::RunPrepared()) base_cost=Script::GetReturnedDword();
			}
		}
		else
		{
			base_cost=base_cost*(100+(cl->IsRawParam(PE_MASTER_TRADER)?0:barter_k))/100;
			if(!base_cost) base_cost++;
		}
		buy_cost+=base_cost*buy_item_count[i];
		buy_weight+=item->Proto->Weight*buy_item_count[i];
		buy_volume+=item->Proto->Volume*buy_item_count[i];
		buy_items.push_back(item);

		if(buy_cost>sale_cost && !is_free)
		{
			WriteLog(__FUNCTION__" - Buy is > sale - ignore barter, client<%s>, npc<%s>.\n",cl->GetInfo(),npc->GetInfo());
			cl->Send_TextMsg(cl,STR_BARTER_BAD_OFFER,SAY_DIALOG,TEXTMSG_GAME);
			cl->Send_ContainerInfo(npc,TRANSFER_CRIT_BARTER,false);
			return;
		}
	}

	// Check weight
	if(cl->GetFreeWeight()+sale_weight<(int)buy_weight)
	{
		WriteLog(__FUNCTION__" - Overweight - ignore barter, client<%s>, npc<%s>.\n",cl->GetInfo(),npc->GetInfo());
		cl->Send_TextMsg(cl,STR_BARTER_OVERWEIGHT,SAY_DIALOG,TEXTMSG_GAME);
		cl->Send_ContainerInfo(npc,TRANSFER_CRIT_BARTER,false);
		return;
	}

	// Check volume
	if(cl->GetFreeVolume()+sale_volume<(int)buy_volume)
	{
		WriteLog(__FUNCTION__" - Oversize - ignore barter, client<%s>, npc<%s>.\n",cl->GetInfo(),npc->GetInfo());
		cl->Send_TextMsg(cl,STR_BARTER_OVERSIZE,SAY_DIALOG,TEXTMSG_GAME);
		cl->Send_ContainerInfo(npc,TRANSFER_CRIT_BARTER,false);
		return;
	}

	// Barter script
	bool result=false;
	if(Script::PrepareContext(ServerFunctions.ItemsBarter,CALL_FUNC_STR,cl->GetInfo()))
	{
		CScriptArray* sale_items_=Script::CreateArray("Item@[]");
		CScriptArray* sale_items_count_=Script::CreateArray("uint[]");
		CScriptArray* buy_items_=Script::CreateArray("Item@[]");
		CScriptArray* buy_items_count_=Script::CreateArray("uint[]");
		Script::AppendVectorToArrayRef(sale_items,sale_items_);
		Script::AppendVectorToArray(sale_item_count,sale_items_count_);
		Script::AppendVectorToArrayRef(buy_items,buy_items_);
		Script::AppendVectorToArray(buy_item_count,buy_items_count_);

		Script::SetArgObject(sale_items_);
		Script::SetArgObject(sale_items_count_);
		Script::SetArgObject(buy_items_);
		Script::SetArgObject(buy_items_count_);
		Script::SetArgObject(cl);
		Script::SetArgObject(npc);
		if(Script::RunPrepared()) result=Script::GetReturnedBool();

		sale_items_->Release();
		sale_items_count_->Release();
		buy_items_->Release();
		buy_items_count_->Release();
	}

	if(!result)
	{
		cl->Send_TextMsg(cl,STR_BARTER_BAD_OFFER,SAY_DIALOG,TEXTMSG_GAME);
		return;
	}

	// Transfer
	// From Player to Npc
	for(int i=0;i<sale_count;++i)
	{
		if(!ItemMngr.MoveItemCritters(cl,npc,sale_item_id[i],sale_item_count[i]))
			WriteLog(__FUNCTION__" - Transfer item, from player to npc, fail, client<%s>, npc<%s>.\n",cl->GetInfo(),npc->GetInfo());
	}

	// From Npc to Player
	for(int i=0;i<buy_count;++i)
	{
		if(!ItemMngr.MoveItemCritters(npc,cl,buy_item_id[i],buy_item_count[i]))
			WriteLog(__FUNCTION__" - Transfer item, from player to npc, fail, client<%s>, npc<%s>.\n",cl->GetInfo(),npc->GetInfo());
	}

	cl->Talk.StartTick=Timer::GameTick();
	cl->Talk.TalkTime=max(cl->GetRawParam(SK_BARTER)*1000,GameOpt.DlgBarterMinTime);
	if(!is_free) cl->Send_TextMsg(cl,STR_BARTER_GOOD_OFFER,SAY_DIALOG,TEXTMSG_GAME);
	cl->Send_ContainerInfo(npc,TRANSFER_CRIT_BARTER,false);
	SetScore(SCORE_TRADER,cl,1);
}