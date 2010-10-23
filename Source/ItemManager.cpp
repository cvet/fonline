#include "StdAfx.h"
#include "ItemManager.h"
#include "Log.h"
#include "Text.h"
#include "Names.h"

#ifdef FONLINE_SERVER
#include "Critter.h"
#include "Map.h"
#include "CritterManager.h"
#include "MapManager.h"
#endif

ItemManager ItemMngr;

bool ItemManager::Init()
{
	WriteLog("Item manager initialization...\n");

	if(IsInit())
	{
		WriteLog("already init.\n");
		return true;
	}

	Clear();
	ClearProtos();

	isActive=true;
	WriteLog("Item manager initialization complete.\n");
	return true;
}

void ItemManager::Finish()
{
	WriteLog("Item manager finish...\n");

	if(!isActive)
	{
		WriteLog("already finish or not init.\n");
		return;
	}

#ifdef FONLINE_SERVER
	ItemGarbager();

	ItemPtrMap items=gameItems;
	for(ItemPtrMapIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=(*it).second;
		item->EventFinish(false);
		item->Release();
	}
	gameItems.clear();
#endif

	Clear();
	ClearProtos();

	isActive=false;
	WriteLog("Item manager finish complete.\n");
}

void ItemManager::Clear()
{
#ifdef FONLINE_SERVER
	for(ItemPtrMapIt it=gameItems.begin(),end=gameItems.end();it!=end;++it)
		SAFEREL((*it).second);
	gameItems.clear();
	radioItems.clear();
	itemToDelete.clear();
	lastItemId=0;
#endif

	for(int i=0;i<MAX_ITEM_PROTOTYPES;i++) itemCount[i]=0;
}

#if defined(FONLINE_SERVER) || defined(FONLINE_OBJECT_EDITOR) || defined(FONLINE_MAPPER)
#define PROTO_APP "Proto"
int ItemManager::GetProtoValue(const char* key)
{
	char str[128];
	if(!txtFile.GetStr(PROTO_APP,key,"",str)) return 0;
	return FONames::GetDefineValue(str);
}

bool ItemManager::SerializeTextProto(bool save, ProtoItem& proto_item, FILE* f, ProtoItemVec* protos)
{
	char str[MAX_FOPATH];

	if(save)
	{
#ifndef FONLINE_OBJECT_EDITOR
		return false;
#endif
		fprintf(f,"[%s]\n",PROTO_APP);
	}
	else
	{
		if(!txtFile.GotoNextApp(PROTO_APP)) return false;
		proto_item.Clear();
	}

#define SERIALIZE_PROTO(field,def) if(save && proto_item.field) fprintf(f,"%s=%d\n",#field,proto_item.field); else if(!save) proto_item.field=GetProtoValue(#field)
#define SERIALIZE_PROTOB(field,def) if(save && proto_item.field) fprintf(f,"%s=%d\n",#field,proto_item.field?1:0); else if(!save) proto_item.field=GetProtoValue(#field)!=0
#define SERIALIZE_PROTO_(field,field_,def) if(save && proto_item.field_) fprintf(f,"%s=%d\n",#field,proto_item.field_); else if(!save) proto_item.field_=GetProtoValue(#field)
#define SERIALIZE_PROTOB_(field,field_,def) if(save && proto_item.field_) fprintf(f,"%s=%d\n",#field,proto_item.field_?1:0); else if(!save) proto_item.field_=GetProtoValue(#field)!=0
	SERIALIZE_PROTO(Pid,0);
	if(!proto_item.Pid || proto_item.Pid>=MAX_ITEM_PROTOTYPES) return false;

	SERIALIZE_PROTO(Type,0);

	// Pictures
	if(save)
	{
#ifdef FONLINE_OBJECT_EDITOR
		if(proto_item.PicMapStr.length()) fprintf(f,"PicMapName=%s\n",proto_item.PicMapStr.c_str());
		if(proto_item.PicInvStr.length()) fprintf(f,"PicInvName=%s\n",proto_item.PicInvStr.c_str());
#endif
	}
	else
	{
#ifdef FONLINE_OBJECT_EDITOR
		if(txtFile.GetStr(PROTO_APP,"PicMapName","",str)) proto_item.PicMapStr=str;
		else if(txtFile.GetStr(PROTO_APP,"PicMap","",str)) proto_item.PicMapStr=Deprecated_GetPicName(proto_item.Pid,proto_item.Type,atoi(str));
		if(txtFile.GetStr(PROTO_APP,"PicInvName","",str)) proto_item.PicInvStr=str;
		else if(txtFile.GetStr(PROTO_APP,"PicInv","",str)) proto_item.PicInvStr=Deprecated_GetPicName(-3,0,atoi(str));
#else
		if(txtFile.GetStr(PROTO_APP,"PicMapName","",str)) proto_item.PicMapHash=Str::GetHash(str);
		else if(txtFile.GetStr(PROTO_APP,"PicMap","",str)) proto_item.PicMapHash=Str::GetHash(Deprecated_GetPicName(proto_item.Pid,proto_item.Type,atoi(str)).c_str());
		if(txtFile.GetStr(PROTO_APP,"PicInvName","",str)) proto_item.PicInvHash=Str::GetHash(str);
		else if(txtFile.GetStr(PROTO_APP,"PicInv","",str)) proto_item.PicInvHash=Str::GetHash(Deprecated_GetPicName(-3,0,atoi(str)).c_str());
#endif
	}

	// Script
	if(save)
	{
#ifdef FONLINE_OBJECT_EDITOR
		if(proto_item.ScriptModule.length()) fprintf(f,"ScriptModule=%s\n",proto_item.ScriptModule.c_str());
		if(proto_item.ScriptFunc.length()) fprintf(f,"ScriptFunc=%s\n",proto_item.ScriptFunc.c_str());
#endif
	}
	else
	{
#ifdef FONLINE_OBJECT_EDITOR
		txtFile.GetStr(PROTO_APP,"ScriptModule","",str);
		proto_item.ScriptModule=str;
		txtFile.GetStr(PROTO_APP,"ScriptFunc","",str);
		proto_item.ScriptFunc=str;
#else
		SAFEDELA(protoScript[proto_item.Pid]);
		char script_module[MAX_SCRIPT_NAME+1];
		char script_func[MAX_SCRIPT_NAME+1];
		if(txtFile.GetStr(PROTO_APP,"ScriptModule","",script_module) && script_module[0] &&
			txtFile.GetStr(PROTO_APP,"ScriptFunc","",script_func) && script_func[0])
			protoScript[proto_item.Pid]=StringDuplicate(Str::Format("%s@%s",script_module,script_func));
#endif

	}

	SERIALIZE_PROTO(Slot,0);
	SERIALIZE_PROTO(Corner,0);
	SERIALIZE_PROTO(LightDistance,0);
	SERIALIZE_PROTO(LightIntensity,0);
	SERIALIZE_PROTO(LightColor,0);
	SERIALIZE_PROTO(LightFlags,0);
	SERIALIZE_PROTO(Flags,0);
	SERIALIZE_PROTOB(DisableEgg,0);

	if(!save && txtFile.IsKey(PROTO_APP,"TransType")) // Deprecated
	{
		int alpha=GetProtoValue("TransVal");
		int trans=GetProtoValue("TransType");
		int corner=GetProtoValue("LightType");

		if(trans!=0) proto_item.DisableEgg=true; // Egg
		else proto_item.DisableEgg=false;

		proto_item.LightColor=0;
		if(trans==3) proto_item.LightColor=0x7F000000; // Glass
		else if(trans==4) proto_item.LightColor=0x6FFFFFFF; // Steam
		else if(trans==5) proto_item.LightColor=0x7FFFFF00; // Energy
		else if(trans==6) proto_item.LightColor=0x7FFF0000; // Red
		else proto_item.LightColor=((0xFF-CLAMP(alpha,0,0xFF))<<24);
		if(trans>=3 && trans<=6) SETFLAG(proto_item.Flags,ITEM_COLORIZE);

		if(corner==0x80) proto_item.Corner=CORNER_WEST;
		else if(corner==0x40) proto_item.Corner=CORNER_EAST;
		else if(corner==0x20) proto_item.Corner=CORNER_SOUTH;
		else if(corner==0x10) proto_item.Corner=CORNER_NORTH;
		else if(corner==0x08) proto_item.Corner=CORNER_EAST_WEST;
		else proto_item.Corner=CORNER_NORTH_SOUTH;

		SERIALIZE_PROTO_(DistanceLight,LightDistance,0);
		int inten=GetProtoValue("IntensityLight");
		if(inten<0 || inten>100) inten=50;
		proto_item.LightIntensity=inten;
		if(proto_item.LightIntensity || proto_item.LightDistance)
		{
			SETFLAG(proto_item.Flags,ITEM_LIGHT);
			if(!proto_item.LightIntensity) proto_item.LightIntensity=50;
			if(!proto_item.LightDistance) proto_item.LightDistance=1;
		}
	}

	if(!save && proto_item.LightColor==0xFF000000 && !FLAG(proto_item.Flags,ITEM_LIGHT|ITEM_COLORIZE|ITEM_COLORIZE)) proto_item.LightColor=0;

	SERIALIZE_PROTO(Weight,0);
	SERIALIZE_PROTO(Volume,0);
	SERIALIZE_PROTO(SoundId,0);
	SERIALIZE_PROTO(Cost,0);
	SERIALIZE_PROTO(Material,0);

	SERIALIZE_PROTO(AnimWaitBase,0);
	SERIALIZE_PROTO(AnimWaitRndMin,0);
	SERIALIZE_PROTO(AnimWaitRndMax,0);
	SERIALIZE_PROTO_(AnimStay_0,AnimStay[0],0);
	SERIALIZE_PROTO_(AnimStay_1,AnimStay[1],0);
	SERIALIZE_PROTO_(AnimShow_0,AnimShow[0],0);
	SERIALIZE_PROTO_(AnimShow_1,AnimShow[1],0);
	SERIALIZE_PROTO_(AnimHide_0,AnimHide[0],0);
	SERIALIZE_PROTO_(AnimHide_1,AnimHide[1],0);
	SERIALIZE_PROTO(DrawPosOffsY,0);

	SERIALIZE_PROTO(RadioChannel,0);
	SERIALIZE_PROTO(RadioFlags,0);
	SERIALIZE_PROTO(RadioBroadcastSend,0);
	SERIALIZE_PROTO(RadioBroadcastRecv,0);

	SERIALIZE_PROTO(IndicatorStart,0);
	SERIALIZE_PROTO(IndicatorMax,0);

	SERIALIZE_PROTO(HolodiskNum,0);

	switch(proto_item.Type)
	{
	case ITEM_TYPE_ARMOR:
		if(!save && txtFile.IsKey(PROTO_APP,"ARMOR.Anim0Male")) // Deprecated
		{
			SERIALIZE_PROTO_(ARMOR.Anim0Male,Armor.Anim0Male,0);
			SERIALIZE_PROTO_(ARMOR.Anim0Female,Armor.Anim0Female,0);
			SERIALIZE_PROTO_(ARMOR.AC,Armor.AC,0);
			SERIALIZE_PROTO_(ARMOR.DRNormal,Armor.DRNormal,0);
			SERIALIZE_PROTO_(ARMOR.DRLaser,Armor.DRLaser,0);
			SERIALIZE_PROTO_(ARMOR.DRFire,Armor.DRFire,0);
			SERIALIZE_PROTO_(ARMOR.DRPlasma,Armor.DRPlasma,0);
			SERIALIZE_PROTO_(ARMOR.DRElectr,Armor.DRElectr,0);
			SERIALIZE_PROTO_(ARMOR.DREmp,Armor.DREmp,0);
			SERIALIZE_PROTO_(ARMOR.DRExplode,Armor.DRExplode,0);
			SERIALIZE_PROTO_(ARMOR.DTNormal,Armor.DTNormal,0);
			SERIALIZE_PROTO_(ARMOR.DTLaser,Armor.DTLaser,0);
			SERIALIZE_PROTO_(ARMOR.DTFire,Armor.DTFire,0);
			SERIALIZE_PROTO_(ARMOR.DTPlasma,Armor.DTPlasma,0);
			SERIALIZE_PROTO_(ARMOR.DTElectr,Armor.DTElectr,0);
			SERIALIZE_PROTO_(ARMOR.DTEmp,Armor.DTEmp,0);
			SERIALIZE_PROTO_(ARMOR.DTExplode,Armor.DTExplode,0);
			SERIALIZE_PROTO_(ARMOR.Perk,Armor.Perk,0);
		}
		else
		{
			SERIALIZE_PROTO(Armor.Anim0Male,0);
			SERIALIZE_PROTO(Armor.Anim0Female,0);
			SERIALIZE_PROTO(Armor.AC,0);
			SERIALIZE_PROTO(Armor.DRNormal,0);
			SERIALIZE_PROTO(Armor.DRLaser,0);
			SERIALIZE_PROTO(Armor.DRFire,0);
			SERIALIZE_PROTO(Armor.DRPlasma,0);
			SERIALIZE_PROTO(Armor.DRElectr,0);
			SERIALIZE_PROTO(Armor.DREmp,0);
			SERIALIZE_PROTO(Armor.DRExplode,0);
			SERIALIZE_PROTO(Armor.DTNormal,0);
			SERIALIZE_PROTO(Armor.DTLaser,0);
			SERIALIZE_PROTO(Armor.DTFire,0);
			SERIALIZE_PROTO(Armor.DTPlasma,0);
			SERIALIZE_PROTO(Armor.DTElectr,0);
			SERIALIZE_PROTO(Armor.DTEmp,0);
			SERIALIZE_PROTO(Armor.DTExplode,0);
			SERIALIZE_PROTO(Armor.Perk,0);
		}
		break;
	case ITEM_TYPE_WEAPON:
		if(!save && txtFile.IsKey(PROTO_APP,"WEAPON.IsNeedAct")) // Deprecated
		{
			SERIALIZE_PROTOB_(WEAPON.IsNeedAct,Weapon.IsNeedAct,0);
			SERIALIZE_PROTOB_(WEAPON.IsUnarmed,Weapon.IsUnarmed,0);
			SERIALIZE_PROTO_(WEAPON.UnarmedTree,Weapon.UnarmedTree,0);
			SERIALIZE_PROTO_(WEAPON.UnarmedPriority,Weapon.UnarmedPriority,0);
			SERIALIZE_PROTO_(WEAPON.UnarmedCriticalBonus,Weapon.UnarmedCriticalBonus,0);
			SERIALIZE_PROTOB_(WEAPON.UnarmedArmorPiercing,Weapon.UnarmedArmorPiercing,0);
			SERIALIZE_PROTO_(WEAPON.UnarmedMinAgility,Weapon.UnarmedMinAgility,0);
			SERIALIZE_PROTO_(WEAPON.UnarmedMinUnarmed,Weapon.UnarmedMinUnarmed,0);
			SERIALIZE_PROTO_(WEAPON.UnarmedMinLevel,Weapon.UnarmedMinLevel,0);
			SERIALIZE_PROTO_(WEAPON.UnarmedTree,Weapon.UnarmedTree,0);
			SERIALIZE_PROTO_(WEAPON.UnarmedPriority,Weapon.UnarmedPriority,0);
			SERIALIZE_PROTO_(WEAPON.VolHolder,Weapon.VolHolder,0);
			SERIALIZE_PROTO_(WEAPON.Caliber,Weapon.Caliber,0);
			SERIALIZE_PROTO_(WEAPON.DefAmmo,Weapon.DefAmmo,0);
			SERIALIZE_PROTO_(WEAPON.ReloadAp,Weapon.ReloadAp,0);
			SERIALIZE_PROTO_(WEAPON.Anim1,Weapon.Anim1,0);
			SERIALIZE_PROTO_(WEAPON.CrFailture,Weapon.CrFailture,0);
			SERIALIZE_PROTO_(WEAPON.MinSt,Weapon.MinSt,0);
			SERIALIZE_PROTO_(WEAPON.Perk,Weapon.Perk,0);
			SERIALIZE_PROTOB_(WEAPON.NoWear,Weapon.NoWear,0);
			SERIALIZE_PROTO_(WEAPON.CountAttack,Weapon.Uses,0);
			SERIALIZE_PROTO_(WEAPON.aSkill_0,Weapon.Skill[0],0);
			SERIALIZE_PROTO_(WEAPON.aSkill_1,Weapon.Skill[1],0);
			SERIALIZE_PROTO_(WEAPON.aSkill_2,Weapon.Skill[2],0);
			SERIALIZE_PROTO_(WEAPON.aDmgType_0,Weapon.DmgType[0],0);
			SERIALIZE_PROTO_(WEAPON.aDmgType_1,Weapon.DmgType[1],0);
			SERIALIZE_PROTO_(WEAPON.aDmgType_2,Weapon.DmgType[2],0);
			SERIALIZE_PROTO_(WEAPON.aAnim2_0,Weapon.Anim2[0],0);
			SERIALIZE_PROTO_(WEAPON.aAnim2_1,Weapon.Anim2[1],0);
			SERIALIZE_PROTO_(WEAPON.aAnim2_2,Weapon.Anim2[2],0);

#ifdef FONLINE_OBJECT_EDITOR
			if(txtFile.GetStr(PROTO_APP,"WEAPON.aPic_0","",str)) proto_item.WeaponPicStr[0]=Deprecated_GetPicName(-1,0,atoi(str));
			if(txtFile.GetStr(PROTO_APP,"WEAPON.aPic_1","",str)) proto_item.WeaponPicStr[1]=Deprecated_GetPicName(-1,0,atoi(str));
			if(txtFile.GetStr(PROTO_APP,"WEAPON.aPic_2","",str)) proto_item.WeaponPicStr[2]=Deprecated_GetPicName(-1,0,atoi(str));
#else
			if(txtFile.GetStr(PROTO_APP,"WEAPON.aPic_0","",str)) proto_item.Weapon.PicHash[0]=Str::GetHash(Deprecated_GetPicName(-1,0,atoi(str)).c_str());
			if(txtFile.GetStr(PROTO_APP,"WEAPON.aPic_1","",str)) proto_item.Weapon.PicHash[1]=Str::GetHash(Deprecated_GetPicName(-1,0,atoi(str)).c_str());
			if(txtFile.GetStr(PROTO_APP,"WEAPON.aPic_2","",str)) proto_item.Weapon.PicHash[2]=Str::GetHash(Deprecated_GetPicName(-1,0,atoi(str)).c_str());
#endif

			SERIALIZE_PROTO_(WEAPON.aDmgMin_0,Weapon.DmgMin[0],0);
			SERIALIZE_PROTO_(WEAPON.aDmgMin_1,Weapon.DmgMin[1],0);
			SERIALIZE_PROTO_(WEAPON.aDmgMin_2,Weapon.DmgMin[2],0);
			SERIALIZE_PROTO_(WEAPON.aDmgMax_0,Weapon.DmgMax[0],0);
			SERIALIZE_PROTO_(WEAPON.aDmgMax_1,Weapon.DmgMax[1],0);
			SERIALIZE_PROTO_(WEAPON.aDmgMax_2,Weapon.DmgMax[2],0);
			SERIALIZE_PROTO_(WEAPON.aMaxDist_0,Weapon.MaxDist[0],0);
			SERIALIZE_PROTO_(WEAPON.aMaxDist_1,Weapon.MaxDist[1],0);
			SERIALIZE_PROTO_(WEAPON.aMaxDist_2,Weapon.MaxDist[2],0);
			SERIALIZE_PROTO_(WEAPON.aEffect_0,Weapon.Effect[0],0);
			SERIALIZE_PROTO_(WEAPON.aEffect_1,Weapon.Effect[1],0);
			SERIALIZE_PROTO_(WEAPON.aEffect_2,Weapon.Effect[2],0);
			SERIALIZE_PROTO_(WEAPON.aRound_0,Weapon.Round[0],0);
			SERIALIZE_PROTO_(WEAPON.aRound_1,Weapon.Round[1],0);
			SERIALIZE_PROTO_(WEAPON.aRound_2,Weapon.Round[2],0);
			SERIALIZE_PROTO_(WEAPON.aTime_0,Weapon.ApCost[0],0);
			SERIALIZE_PROTO_(WEAPON.aTime_1,Weapon.ApCost[1],0);
			SERIALIZE_PROTO_(WEAPON.aTime_2,Weapon.ApCost[2],0);
			SERIALIZE_PROTOB_(WEAPON.aAim_0,Weapon.Aim[0],0);
			SERIALIZE_PROTOB_(WEAPON.aAim_1,Weapon.Aim[1],0);
			SERIALIZE_PROTOB_(WEAPON.aAim_2,Weapon.Aim[2],0);
			SERIALIZE_PROTOB_(WEAPON.aRemove_0,Weapon.Remove[0],0);
			SERIALIZE_PROTOB_(WEAPON.aRemove_1,Weapon.Remove[1],0);
			SERIALIZE_PROTOB_(WEAPON.aRemove_2,Weapon.Remove[2],0);
			SERIALIZE_PROTO_(WEAPON.aSound_0,Weapon.SoundId[0],0);
			SERIALIZE_PROTO_(WEAPON.aSound_1,Weapon.SoundId[1],0);
			SERIALIZE_PROTO_(WEAPON.aSound_2,Weapon.SoundId[2],0);
		}
		else
		{
			SERIALIZE_PROTOB(Weapon.IsNeedAct,0);
			SERIALIZE_PROTOB(Weapon.IsUnarmed,0);
			SERIALIZE_PROTO(Weapon.UnarmedTree,0);
			SERIALIZE_PROTO(Weapon.UnarmedPriority,0);
			SERIALIZE_PROTO(Weapon.UnarmedCriticalBonus,0);
			SERIALIZE_PROTOB(Weapon.UnarmedArmorPiercing,0);
			SERIALIZE_PROTO(Weapon.UnarmedMinAgility,0);
			SERIALIZE_PROTO(Weapon.UnarmedMinUnarmed,0);
			SERIALIZE_PROTO(Weapon.UnarmedMinLevel,0);
			SERIALIZE_PROTO(Weapon.UnarmedTree,0);
			SERIALIZE_PROTO(Weapon.UnarmedPriority,0);
			SERIALIZE_PROTO(Weapon.VolHolder,0);
			SERIALIZE_PROTO(Weapon.Caliber,0);
			SERIALIZE_PROTO(Weapon.DefAmmo,0);
			SERIALIZE_PROTO(Weapon.ReloadAp,0);
			SERIALIZE_PROTO(Weapon.Anim1,0);
			SERIALIZE_PROTO(Weapon.CrFailture,0);
			SERIALIZE_PROTO(Weapon.MinSt,0);
			SERIALIZE_PROTO(Weapon.Perk,0);
			SERIALIZE_PROTOB(Weapon.NoWear,0);
			SERIALIZE_PROTO_(Weapon.CountAttack,Weapon.Uses,0);
			SERIALIZE_PROTO_(Weapon.Skill_0,Weapon.Skill[0],0);
			SERIALIZE_PROTO_(Weapon.Skill_1,Weapon.Skill[1],0);
			SERIALIZE_PROTO_(Weapon.Skill_2,Weapon.Skill[2],0);
			SERIALIZE_PROTO_(Weapon.DmgType_0,Weapon.DmgType[0],0);
			SERIALIZE_PROTO_(Weapon.DmgType_1,Weapon.DmgType[1],0);
			SERIALIZE_PROTO_(Weapon.DmgType_2,Weapon.DmgType[2],0);
			SERIALIZE_PROTO_(Weapon.Anim2_0,Weapon.Anim2[0],0);
			SERIALIZE_PROTO_(Weapon.Anim2_1,Weapon.Anim2[1],0);
			SERIALIZE_PROTO_(Weapon.Anim2_2,Weapon.Anim2[2],0);

			if(save)
			{
#ifdef FONLINE_OBJECT_EDITOR
				if(proto_item.WeaponPicStr[0].length()) fprintf(f,"Weapon.PicName_0=%s\n",proto_item.WeaponPicStr[0].c_str());
				if(proto_item.WeaponPicStr[1].length()) fprintf(f,"Weapon.PicName_1=%s\n",proto_item.WeaponPicStr[1].c_str());
				if(proto_item.WeaponPicStr[2].length()) fprintf(f,"Weapon.PicName_2=%s\n",proto_item.WeaponPicStr[2].c_str());
#endif
			}
			else
			{
#ifdef FONLINE_OBJECT_EDITOR
				if(txtFile.GetStr(PROTO_APP,"Weapon.PicName_0","",str)) proto_item.WeaponPicStr[0]=str;
				if(txtFile.GetStr(PROTO_APP,"Weapon.PicName_1","",str)) proto_item.WeaponPicStr[1]=str;
				if(txtFile.GetStr(PROTO_APP,"Weapon.PicName_2","",str)) proto_item.WeaponPicStr[2]=str;
#else
				if(txtFile.GetStr(PROTO_APP,"Weapon.PicName_0","",str)) proto_item.Weapon.PicHash[0]=Str::GetHash(str);
				if(txtFile.GetStr(PROTO_APP,"Weapon.PicName_1","",str)) proto_item.Weapon.PicHash[1]=Str::GetHash(str);
				if(txtFile.GetStr(PROTO_APP,"Weapon.PicName_2","",str)) proto_item.Weapon.PicHash[2]=Str::GetHash(str);
#endif
			}

			SERIALIZE_PROTO_(Weapon.DmgMin_0,Weapon.DmgMin[0],0);
			SERIALIZE_PROTO_(Weapon.DmgMin_1,Weapon.DmgMin[1],0);
			SERIALIZE_PROTO_(Weapon.DmgMin_2,Weapon.DmgMin[2],0);
			SERIALIZE_PROTO_(Weapon.DmgMax_0,Weapon.DmgMax[0],0);
			SERIALIZE_PROTO_(Weapon.DmgMax_1,Weapon.DmgMax[1],0);
			SERIALIZE_PROTO_(Weapon.DmgMax_2,Weapon.DmgMax[2],0);
			SERIALIZE_PROTO_(Weapon.MaxDist_0,Weapon.MaxDist[0],0);
			SERIALIZE_PROTO_(Weapon.MaxDist_1,Weapon.MaxDist[1],0);
			SERIALIZE_PROTO_(Weapon.MaxDist_2,Weapon.MaxDist[2],0);
			SERIALIZE_PROTO_(Weapon.Effect_0,Weapon.Effect[0],0);
			SERIALIZE_PROTO_(Weapon.Effect_1,Weapon.Effect[1],0);
			SERIALIZE_PROTO_(Weapon.Effect_2,Weapon.Effect[2],0);
			SERIALIZE_PROTO_(Weapon.Round_0,Weapon.Round[0],0);
			SERIALIZE_PROTO_(Weapon.Round_1,Weapon.Round[1],0);
			SERIALIZE_PROTO_(Weapon.Round_2,Weapon.Round[2],0);
			SERIALIZE_PROTO_(Weapon.Time_0,Weapon.ApCost[0],0);
			SERIALIZE_PROTO_(Weapon.Time_1,Weapon.ApCost[1],0);
			SERIALIZE_PROTO_(Weapon.Time_2,Weapon.ApCost[2],0);
			SERIALIZE_PROTOB_(Weapon.Aim_0,Weapon.Aim[0],0);
			SERIALIZE_PROTOB_(Weapon.Aim_1,Weapon.Aim[1],0);
			SERIALIZE_PROTOB_(Weapon.Aim_2,Weapon.Aim[2],0);
			SERIALIZE_PROTOB_(Weapon.Remove_0,Weapon.Remove[0],0);
			SERIALIZE_PROTOB_(Weapon.Remove_1,Weapon.Remove[1],0);
			SERIALIZE_PROTOB_(Weapon.Remove_2,Weapon.Remove[2],0);
			SERIALIZE_PROTO_(Weapon.SoundId_0,Weapon.SoundId[0],0);
			SERIALIZE_PROTO_(Weapon.SoundId_1,Weapon.SoundId[1],0);
			SERIALIZE_PROTO_(Weapon.SoundId_2,Weapon.SoundId[2],0);
		}
		break;
	case ITEM_TYPE_AMMO:
		if(!save && txtFile.IsKey(PROTO_APP,"AMMO.StartCount")) // Deprecated
		{
			SERIALIZE_PROTO_(AMMO.StartCount,Ammo.StartCount,0);
			SERIALIZE_PROTO_(AMMO.Caliber,Ammo.Caliber,0);
			SERIALIZE_PROTO_(AMMO.ACMod,Ammo.ACMod,0);
			SERIALIZE_PROTO_(AMMO.DRMod,Ammo.DRMod,0);
			SERIALIZE_PROTO_(AMMO.DmgMult,Ammo.DmgMult,0);
			SERIALIZE_PROTO_(AMMO.DmgDiv,Ammo.DmgDiv,0);
		}
		else
		{
			SERIALIZE_PROTO(Ammo.StartCount,0);
			SERIALIZE_PROTO(Ammo.Caliber,0);
			SERIALIZE_PROTO(Ammo.ACMod,0);
			SERIALIZE_PROTO(Ammo.DRMod,0);
			SERIALIZE_PROTO(Ammo.DmgMult,0);
			SERIALIZE_PROTO(Ammo.DmgDiv,0);
		}
		break;
	case ITEM_TYPE_MISC:
		break;
	case ITEM_TYPE_MISC_EX:
//===============================================================================================
#define PARSE_CAR_BLOCKS(data) \
	memset(proto_item.MiscEx.Car.data,0xFF,sizeof(proto_item.MiscEx.Car.data));\
	for(int i=0,j=strlen(buf);i<j;i++)\
	{\
		BYTE dir=buf[i]-'0';\
		if(i&1) proto_item.MiscEx.Car.data[i/2]=(proto_item.MiscEx.Car.data[i/2]&0xF0)|dir;\
		else proto_item.MiscEx.Car.data[i/2]=(proto_item.MiscEx.Car.data[i/2]&0x0F)|(dir<<4);\
	}
//===============================================================================================

		if(!save && txtFile.IsKey(PROTO_APP,"MISC2.StartVal1")) // Deprecated
		{
			SERIALIZE_PROTO_(MISC2.StartVal1,MiscEx.StartVal1,0);
			SERIALIZE_PROTO_(MISC2.StartVal2,MiscEx.StartVal2,0);
			SERIALIZE_PROTO_(MISC2.StartVal3,MiscEx.StartVal3,0);
			SERIALIZE_PROTOB_(MISC2.IsCar,MiscEx.IsCar,0);

			if(proto_item.MiscEx.IsCar)
			{
				SERIALIZE_PROTO_(MISC2.CAR.Speed,MiscEx.Car.Speed,0);
				SERIALIZE_PROTO_(MISC2.CAR.Negotiability,MiscEx.Car.Negotiability,0);
				SERIALIZE_PROTO_(MISC2.CAR.WearConsumption,MiscEx.Car.WearConsumption,0);
				SERIALIZE_PROTO_(MISC2.CAR.CritCapacity,MiscEx.Car.CritCapacity,0);
				SERIALIZE_PROTO_(MISC2.CAR.FuelTank,MiscEx.Car.FuelTank,0);
				SERIALIZE_PROTO_(MISC2.CAR.RunToBreak,MiscEx.Car.RunToBreak,0);
				SERIALIZE_PROTO_(MISC2.CAR.Entire,MiscEx.Car.Entire,0);
				SERIALIZE_PROTO_(MISC2.CAR.WalkType,MiscEx.Car.WalkType,0);
				SERIALIZE_PROTO_(MISC2.CAR.FuelConsumption,MiscEx.Car.FuelConsumption,0);

				char buf[256];
				txtFile.GetStr(PROTO_APP,"MISC2.CAR.Bag_0","",buf);
				PARSE_CAR_BLOCKS(Bag0);
				txtFile.GetStr(PROTO_APP,"MISC2.CAR.Bag_1","",buf);
				PARSE_CAR_BLOCKS(Bag1);
				txtFile.GetStr(PROTO_APP,"MISC2.CAR.Blocks","",buf);
				PARSE_CAR_BLOCKS(Blocks);
			}
		}
		else
		{
			SERIALIZE_PROTO(MiscEx.StartVal1,0);
			SERIALIZE_PROTO(MiscEx.StartVal2,0);
			SERIALIZE_PROTO(MiscEx.StartVal3,0);
			SERIALIZE_PROTOB(MiscEx.IsCar,0);

			if(proto_item.MiscEx.IsCar)
			{
				SERIALIZE_PROTO(MiscEx.Car.Speed,0);
				SERIALIZE_PROTO(MiscEx.Car.Negotiability,0);
				SERIALIZE_PROTO(MiscEx.Car.WearConsumption,0);
				SERIALIZE_PROTO(MiscEx.Car.CritCapacity,0);
				SERIALIZE_PROTO(MiscEx.Car.FuelTank,0);
				SERIALIZE_PROTO(MiscEx.Car.RunToBreak,0);
				SERIALIZE_PROTO(MiscEx.Car.Entire,0);
				SERIALIZE_PROTO(MiscEx.Car.WalkType,0);
				SERIALIZE_PROTO(MiscEx.Car.FuelConsumption,0);

				if(save)
				{
					char buf[256]={0};
					for(int i=0;i<CAR_MAX_BAG_POSITION;i++) if(proto_item.MiscEx.Car.GetBag0Dir(i)<10) StringAppend(buf,Str::Format("%d",proto_item.MiscEx.Car.GetBag0Dir(i)));
					fprintf(f,"MiscEx.Car.Bag_0=%s\n",buf);
					buf[0]=0;
					for(int i=0;i<CAR_MAX_BAG_POSITION;i++) if(proto_item.MiscEx.Car.GetBag1Dir(i)<10) StringAppend(buf,Str::Format("%d",proto_item.MiscEx.Car.GetBag1Dir(i)));
					fprintf(f,"MiscEx.Car.Bag_1=%s\n",buf);
					buf[0]=0;
					for(int i=0;i<CAR_MAX_BLOCKS;i++) if(proto_item.MiscEx.Car.GetBlockDir(i)<10) StringAppend(buf,Str::Format("%d",proto_item.MiscEx.Car.GetBlockDir(i)));
					fprintf(f,"MiscEx.Car.Blocks=%s\n",buf);
				}
				else
				{
					char buf[256];
					txtFile.GetStr(PROTO_APP,"MiscEx.Car.Bag_0","",buf);
					PARSE_CAR_BLOCKS(Bag0);
					txtFile.GetStr(PROTO_APP,"MiscEx.Car.Bag_1","",buf);
					PARSE_CAR_BLOCKS(Bag1);
					txtFile.GetStr(PROTO_APP,"MiscEx.Car.Blocks","",buf);
					PARSE_CAR_BLOCKS(Blocks);
				}
			}
		}
		break;
	case ITEM_TYPE_KEY:
		break;
	case ITEM_TYPE_CONTAINER:
		if(!save && txtFile.IsKey(PROTO_APP,"CONTAINER.ContVolume")) // Deprecated
		{
			SERIALIZE_PROTO_(CONTAINER.ContVolume,Container.ContVolume,0);
			SERIALIZE_PROTO_(CONTAINER.CannotPickUp,Container.CannotPickUp,0);
			SERIALIZE_PROTO_(CONTAINER.MagicHandsGrnd,Container.MagicHandsGrnd,0);
			SERIALIZE_PROTO_(CONTAINER.Changeble,Container.Changeble,0);
			SERIALIZE_PROTOB_(CONTAINER.IsNoOpen,Container.IsNoOpen,0);
		}
		else
		{
			SERIALIZE_PROTO(Container.ContVolume,0);
			SERIALIZE_PROTO(Container.CannotPickUp,0);
			SERIALIZE_PROTO(Container.MagicHandsGrnd,0);
			SERIALIZE_PROTO(Container.Changeble,0);
			SERIALIZE_PROTOB(Container.IsNoOpen,0);
		}
		break;
	case ITEM_TYPE_DOOR:
		if(!save && txtFile.IsKey(PROTO_APP,"DOOR.WalkThru")) // Deprecated
		{
			SERIALIZE_PROTO_(DOOR.WalkThru,Door.WalkThru,0);
			SERIALIZE_PROTOB_(DOOR.IsNoOpen,Door.IsNoOpen,0);
		}
		else
		{
			SERIALIZE_PROTO(Door.WalkThru,0);
			SERIALIZE_PROTOB(Door.IsNoOpen,0);
		}
		break;
	case ITEM_TYPE_GRID:
		if(!save && txtFile.IsKey(PROTO_APP,"GRID.Type")) // Deprecated
		{
			SERIALIZE_PROTO_(GRID.Type,Grid.Type,0);
		}
		else
		{
			SERIALIZE_PROTO(Grid.Type,0);
		}
		break;
	case ITEM_TYPE_GENERIC:
		break;
	case ITEM_TYPE_WALL:
		break;
	default:
		break;
	}

	if(save && f) fprintf(f,"\n");
	if(!save && protos && proto_item.Pid) protos->push_back(proto_item);
	return true;
}

bool ItemManager::LoadProtos()
{
	WriteLog("Loading items prototypes...\n");

	FileManager fm;
	if(!fm.LoadFile("items.lst",PT_SERVER_PRO_ITEMS))
	{
		WriteLog("Can't open \"items.lst\".\n");
		return false;
	}

	char buf[256];
	StrVec fnames;
	int count=0;
	while(fm.GetLine(buf,sizeof(buf)))
	{
		fnames.push_back(string(buf));
		count++;
	}
	fm.UnloadFile();

	DWORD loaded=0;
	ProtoItemVec item_protos;
	ClearProtos();
	for(int i=0;i<count;i++)
	{
		char fname[MAX_FOPATH];
		StringCopy(fname,FileManager::GetFullPath(fnames[i].c_str(),PT_SERVER_PRO_ITEMS));
		if(LoadProtos(item_protos,fname))
		{
			ParseProtos(item_protos);
			loaded+=item_protos.size();
		}
	}

	WriteLog("Items prototypes successfully loaded, count<%u>.\n",loaded);
	return true;
}

bool ItemManager::LoadProtos(ProtoItemVec& protos, const char* fname)
{
	protos.clear();
	if(!txtFile.LoadFile(fname,PT_SERVER_ROOT))
	{
		WriteLog(__FUNCTION__" - File<%s> not found.\n",FileManager::GetFullPath(fname,PT_SERVER_ROOT));
		return false;
	}

	while(true)
	{
		ProtoItem proto_item;
		if(!SerializeTextProto(false,proto_item,NULL,&protos)) break;
	}
	txtFile.UnloadFile();

	if(protos.empty())
	{
		WriteLog(__FUNCTION__" - Proto items not found<%s>.\n",fname);
		return false;
	}
	return true;
}
#endif

#ifdef FONLINE_OBJECT_EDITOR
void ItemManager::SaveProtos(const char* full_path)
{
	ProtoItemVec protos;
	protos.reserve(MAX_ITEM_PROTOTYPES);
	for(int i=1;i<MAX_ITEM_PROTOTYPES;i++)
	{
		ProtoItem* proto_item=GetProtoItem(i);
		if(proto_item) protos.push_back(*proto_item);
	}
	SaveProtos(protos,full_path);
}

void ItemManager::SaveProtos(ProtoItemVec& protos, const char* full_path)
{
	WriteLog("Preservation of item prototypes in a new format in <%s>.\n",full_path);

	FILE* f=NULL;
	if(fopen_s(&f,full_path,"wt"))
	{
		WriteLog("Unable to open file.\n");
		return;
	}

	for(size_t i=0,j=protos.size();i<j;i++)
	{
		ProtoItem& proto_item=protos[i];
		SerializeTextProto(true,proto_item,f,NULL);
	}

	fclose(f);
	WriteLog("Preservation of item prototypes in a new format is completed.\n");
}
#endif

bool CompProtoByPid(ProtoItem o1, ProtoItem o2){return o1.GetPid()<o2.GetPid();}
void ItemManager::ParseProtos(ProtoItemVec& protos)
{
	if(protos.empty())
	{
		WriteLog(__FUNCTION__" - List is empty, parsing ended.\n");
		return;
	}

	for(size_t i=0,j=protos.size();i<j;i++)
	{
		ProtoItem& proto_item=protos[i];
		WORD pid=proto_item.GetPid();

		if(!pid || pid>=MAX_ITEM_PROTOTYPES)
		{
			WriteLog(__FUNCTION__" - Invalid pid<%u> of prototype.\n",pid);
			continue;
		}

		if(IsInitProto(pid))
		{
			//WriteLog(__FUNCTION__" - Prototype is already parsed, pid<%u>. Rewrite.\n",pid);
			ClearProto(pid);
		}

		BYTE type=protos[i].GetType();
		if(type>=ITEM_MAX_TYPES)
		{
			WriteLog(__FUNCTION__" - Unknown type<%u> of prototype<%u>.\n",type,pid);
			continue;
		}

		typeProto[type].push_back(proto_item);
		allProto[pid]=proto_item;
	}

	for(int i=0;i<ITEM_MAX_TYPES;i++)
	{
		protoHash[i]=0;
		if(typeProto[i].size())
		{
			std::sort(typeProto[i].begin(),typeProto[i].end(),CompProtoByPid);
			protoHash[i]=Crypt.Crc32((BYTE*)&typeProto[i][0],sizeof(ProtoItem)*typeProto[i].size());
		}
	}
}

void ItemManager::PrintProtosHash()
{
	WriteLog("Hashes of prototypes:\n"
		"\t\tCRC32 of protos Armor: <%u>\n"
		"\t\tCRC32 of protos Drug: <%u>\n"
		"\t\tCRC32 of protos Weapon: <%u>\n"
		"\t\tCRC32 of protos Ammo: <%u>\n"
		"\t\tCRC32 of protos Misc: <%u>\n"
		"\t\tCRC32 of protos MiscEx: <%u>\n"
		"\t\tCRC32 of protos Key: <%u>\n"
		"\t\tCRC32 of protos Container: <%u>\n"
		"\t\tCRC32 of protos Door: <%u>\n"
		"\t\tCRC32 of protos Grid: <%u>\n"
		"\t\tCRC32 of protos Generic: <%u>\n"
		"\t\tCRC32 of protos Wall: <%u>\n",
		protoHash[ITEM_TYPE_ARMOR],protoHash[ITEM_TYPE_DRUG],protoHash[ITEM_TYPE_WEAPON],protoHash[ITEM_TYPE_AMMO],
		protoHash[ITEM_TYPE_MISC],protoHash[ITEM_TYPE_MISC_EX],protoHash[ITEM_TYPE_KEY],protoHash[ITEM_TYPE_CONTAINER],
		protoHash[ITEM_TYPE_DOOR],protoHash[ITEM_TYPE_GRID],protoHash[ITEM_TYPE_GENERIC],protoHash[ITEM_TYPE_WALL]);
}

ProtoItem* ItemManager::GetProtoItem(WORD pid)
{
	if(!IsInitProto(pid)) return NULL;
	return &allProto[pid];
}

void ItemManager::GetAllProtos(ProtoItemVec& protos)
{
	for(int i=0;i<ITEM_MAX_TYPES;i++)
	{
		for(int j=0;j<typeProto[i].size();j++)
		{
			protos.push_back(typeProto[i][j]);
		}
	}
}

bool ItemManager::IsInitProto(WORD pid)
{
	if(!pid || pid>=MAX_ITEM_PROTOTYPES) return false;
	return allProto[pid].IsInit();
}

const char* ItemManager::GetProtoScript(WORD pid)
{
	if(!IsInitProto(pid)) return NULL;
	return protoScript[pid];
}

void ItemManager::ClearProtos(BYTE type /* = 0xFF */ )
{
	if(type==0xFF)
	{
		for(int i=0;i<ITEM_MAX_TYPES;i++)
		{
			protoHash[i]=0;
			typeProto[i].clear();
		}
		ZeroMemory(allProto,sizeof(allProto));
		for(int i=0;i<MAX_ITEM_PROTOTYPES;i++) SAFEDELA(protoScript[i]);
	}
	else if(type<ITEM_MAX_TYPES)
	{
		for(int i=0;i<typeProto[type].size();++i)
		{
			WORD pid=(typeProto[type])[i].GetPid();
			if(IsInitProto(pid))
			{
				allProto[pid].Clear();
				SAFEDELA(protoScript[pid]);
			}
		}
		typeProto[type].clear();
		protoHash[type]=0;
	}
	else
	{
		WriteLog(__FUNCTION__" - Wrong proto type<%u>.\n",type);
	}
}

void ItemManager::ClearProto(WORD pid)
{
	ProtoItem* proto_item=GetProtoItem(pid);
	if(!proto_item) return;

	ProtoItemVec& protos=typeProto[proto_item->GetType()];
	DWORD& hash=protoHash[proto_item->GetType()];

	allProto[pid].Clear();

	ProtoItemVecIt it=std::find(protos.begin(),protos.end(),pid);
	if(it!=protos.end()) protos.erase(it);

	hash=Crypt.Crc32((BYTE*)&protos[0],sizeof(ProtoItem)*protos.size());
}

#ifdef FONLINE_SERVER
void ItemManager::SaveAllItemsFile(void(*save_func)(void*,size_t))
{
	DWORD count=gameItems.size();
	save_func(&count,sizeof(count));
	for(ItemPtrMapIt it=gameItems.begin(),end=gameItems.end();it!=end;++it)
	{
		Item* item=(*it).second;
		save_func(&item->Id,sizeof(item->Id));
		save_func(&item->Proto->Pid,sizeof(item->Proto->Pid));
		save_func(&item->Accessory,sizeof(item->Accessory));
		save_func(&item->ACC_BUFFER.Buffer[0],sizeof(item->ACC_BUFFER.Buffer[0]));
		save_func(&item->ACC_BUFFER.Buffer[1],sizeof(item->ACC_BUFFER.Buffer[1]));
		save_func(&item->Data,sizeof(item->Data));
		if(item->PLexems)
		{
			BYTE lex_len=strlen(item->PLexems);
			save_func(&lex_len,sizeof(lex_len));
			save_func(item->PLexems,lex_len);
		}
		else
		{
			BYTE zero=0;
			save_func(&zero,sizeof(zero));
		}
	}
}

bool ItemManager::LoadAllItemsFile(FILE* f, int version)
{
	WriteLog("Load items...");

	lastItemId=0;

	DWORD count;
	fread(&count,sizeof(count),1,f);
	if(!count)
	{
		WriteLog("items not found.\n");
		return true;
	}

	int errors=0;
	for(DWORD i=0;i<count;++i)
	{
		DWORD id;
		fread(&id,sizeof(id),1,f);
		WORD pid;
		fread(&pid,sizeof(pid),1,f);
		BYTE acc;
		fread(&acc,sizeof(acc),1,f);
		DWORD acc0;
		fread(&acc0,sizeof(acc0),1,f);
		DWORD acc1;
		fread(&acc1,sizeof(acc1),1,f);
		Item::ItemData data;
		fread(&data,sizeof(data),1,f);
		BYTE lex_len;
		char lexems[1024]={0};
		fread(&lex_len,sizeof(lex_len),1,f);
		if(lex_len)
		{
			fread(lexems,lex_len,1,f);
			lexems[lex_len]=0;
		}

		Item* item=CreateItem(pid,0,id);
		if(!item)
		{
			WriteLog("Create item error id<%u>, pid<%u>.\n",id,pid);
			errors++;
			continue;
		}
		if(id>lastItemId) lastItemId=id;

		item->Accessory=acc;
		item->ACC_BUFFER.Buffer[0]=acc0;
		item->ACC_BUFFER.Buffer[1]=acc1;
		item->Data=data;
		if(lexems[0]) item->SetLexems(lexems);

		AddItemStatistics(pid,item->GetCount());

		// Radio collection
		if(item->IsRadio()) RadioRegister(item,true);

		// Patches
		if(version<WORLD_SAVE_V11)
		{
			if(item->GetProtoId()==PID_RADIO && FLAG(item->Proto->Flags,ITEM_RADIO)) SETFLAG(item->Data.Flags,ITEM_RADIO);
			else if(item->GetProtoId()==PID_HOLODISK && FLAG(item->Proto->Flags,ITEM_HOLODISK)) SETFLAG(item->Data.Flags,ITEM_HOLODISK);
		}
	}
	if(errors) return false;

	WriteLog("complete, count<%u>.\n",count);
	return true;
}

#pragma MESSAGE("Add item proto functions checker.")
bool ItemManager::CheckProtoFunctions()
{
	for(int i=0;i<MAX_ITEM_PROTOTYPES;i++)
	{
		const char* script=protoScript[i];
		// Check for items
		// Check for scenery
	}
	return true;
}

void ItemManager::RunInitScriptItems()
{
	ItemPtrMap items=gameItems;
	for(ItemPtrMapIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=(*it).second;
		if(item->Data.ScriptId) item->ParseScript(NULL,false);
	}
}

void ItemManager::GetGameItems(ItemPtrVec& items)
{
	SCOPE_LOCK(itemLocker);
	items.reserve(gameItems.size());
	for(ItemPtrMapIt it=gameItems.begin(),end=gameItems.end();it!=end;++it)
	{
		Item* item=(*it).second;
		items.push_back(item);
	}
}

DWORD ItemManager::GetItemsCount()
{
	SCOPE_LOCK(itemLocker);
	DWORD count=gameItems.size();
	return count;
}

void ItemManager::SetCritterItems(Critter* cr)
{
	ItemPtrVec items;
	DWORD crid=cr->GetId();

	itemLocker.Lock();
	for(ItemPtrMapIt it=gameItems.begin(),end=gameItems.end();it!=end;++it)
	{
		Item* item=(*it).second;
		if(item->Accessory==ITEM_ACCESSORY_CRITTER && item->ACC_CRITTER.Id==crid)
			items.push_back(item);
	}
	itemLocker.Unlock();

	for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=*it;
		SYNC_LOCK(item);

		if(item->Accessory==ITEM_ACCESSORY_CRITTER && item->ACC_CRITTER.Id==crid)
		{
			cr->SetItem(item);
			if(item->IsRadio()) RadioRegister(item,true);
		}
	}
}

void ItemManager::GetItemIds(DwordSet& item_ids)
{
	SCOPE_LOCK(itemLocker);

	for(ItemPtrMapIt it=gameItems.begin(),end=gameItems.end();it!=end;++it)
		item_ids.insert((*it).second->GetId());
}

Item* ItemManager::CreateItem(WORD pid, DWORD count, DWORD item_id /* = 0 */)
{
	if(!IsInitProto(pid)) return NULL;

	Item* item=new Item();
	if(!item)
	{
		WriteLog(__FUNCTION__" - Allocation fail.\n");
		return NULL;
	}

	item->Init(&allProto[pid]);
	if(item_id)
	{
		SCOPE_LOCK(itemLocker);

		if(gameItems.count(item_id))
		{
			WriteLog(__FUNCTION__" - Item already created, id<%u>.\n",item_id);
			delete item;
			return NULL;
		}

		item->Id=item_id;
	}
	else
	{
		SCOPE_LOCK(itemLocker);

		item->Id=lastItemId+1;
		lastItemId++;
	}

	if(item->IsGrouped()) item->Count_Set(count);
	else AddItemStatistics(pid,1);

	SYNC_LOCK(item);

	// Main collection
	itemLocker.Lock();
	gameItems.insert(ItemPtrMapVal(item->Id,item));
	itemLocker.Unlock();

	// Radio collection
	if(item->IsRadio()) RadioRegister(item,true);

	// Prototype script
	if(!item_id && count && protoScript[pid]) // Only for new items
	{
		item->ParseScript(protoScript[pid],true);
		if(item->IsNotValid) return NULL;
	}
	return item;
}

Item* ItemManager::SplitItem(Item* item, DWORD count)
{
	if(!item->IsGrouped())
	{
		WriteLog(__FUNCTION__" - Splitted item is not grouped, id<%u>, pid<%u>.\n",item->GetId(),item->GetProtoId());
		return NULL;
	}

	DWORD item_count=item->GetCount();
	if(!count || count>=item_count)
	{
		WriteLog(__FUNCTION__" - Invalid count, id<%u>, pid<%u>, count<%u>, split count<%u>.\n",item->GetId(),item->GetProtoId(),item_count,count);
		return NULL;
	}

	Item* new_item=CreateItem(item->GetProtoId(),0); // Ignore init script
	if(!new_item)
	{
		WriteLog(__FUNCTION__" - Create item fail, pid<%u>, count<%u>.\n",item->GetProtoId(),count);
		return NULL;
	}

	item->Count_Sub(count);
	new_item->Data=item->Data;
	new_item->Data.Count=0;
	new_item->Count_Add(count);
	if(item->PLexems) new_item->SetLexems(item->PLexems);

	// Radio collection
	if(new_item->IsRadio()) RadioRegister(new_item,true);

	return new_item;
}

Item* ItemManager::GetItem(DWORD item_id, bool sync_lock)
{
	Item* item=NULL;

	itemLocker.Lock();
	ItemPtrMapIt it=gameItems.find(item_id);
	if(it!=gameItems.end()) item=(*it).second;
	itemLocker.Unlock();

	if(item && sync_lock) SYNC_LOCK(item);
	return item;
}

void ItemManager::ItemToGarbage(Item* item)
{
	SCOPE_LOCK(itemLocker);
	itemToDelete.push_back(item->GetId());
}

void ItemManager::ItemGarbager()
{
	if(!itemToDelete.empty())
	{
		itemLocker.Lock();
		DwordVec to_del=itemToDelete;
		itemToDelete.clear();
		itemLocker.Unlock();

		for(DwordVecIt it=to_del.begin(),end=to_del.end();it!=end;++it)
		{
			// Erase from main collection
			itemLocker.Lock();
			ItemPtrMapIt it_=gameItems.find(*it);
			if(it_==gameItems.end())
			{
				itemLocker.Unlock();
				continue;
			}
			Item* item=(*it_).second;
			gameItems.erase(it_);
			itemLocker.Unlock();

			// Synchronize
			SYNC_LOCK(item);

			// Call finish event
			if(item->IsValidAccessory()) EraseItemHolder(item);
			if(!item->IsNotValid && item->FuncId[ITEM_EVENT_FINISH]>0) item->EventFinish(true);

			item->IsNotValid=true;
			if(item->IsValidAccessory()) EraseItemHolder(item);

			// Erase from statistics
			if(item->IsGrouped()) item->Count_Set(0);
			else SubItemStatistics(item->GetProtoId(),1);

			// Erase from radio collection
			if(item->IsRadio()) RadioRegister(item,false);

			// Clear, release
			item->FullClear();
			Job::DeferredRelease(item);
		}
	}
}

void ItemManager::NotifyChangeItem(Item* item)
{
	switch(item->Accessory)
	{
	case ITEM_ACCESSORY_CRITTER:
		{
			Critter* cr=CrMngr.GetCritter(item->ACC_CRITTER.Id,false);
			if(cr) cr->SendAA_ItemData(item);
		}
		break;
	case ITEM_ACCESSORY_HEX:
		{
			Map* map=MapMngr.GetMap(item->ACC_HEX.MapId,false);
			if(map) map->ChangeDataItem(item);
		}
		break;
	default:
		break;
	}
}

void ItemManager::EraseItemHolder(Item* item)
{
	switch(item->Accessory)
	{
	case ITEM_ACCESSORY_CRITTER:
		{
			Critter* cr=CrMngr.GetCritter(item->ACC_CRITTER.Id,true);
			if(cr) cr->EraseItem(item,true);
			else if(item->IsRadio()) ItemMngr.RadioRegister(item,true);
		}
		break;
	case ITEM_ACCESSORY_HEX:
		{
			Map* map=MapMngr.GetMap(item->ACC_HEX.MapId,true);
			if(map) map->EraseItem(item->GetId());
		}
		break;
	case ITEM_ACCESSORY_CONTAINER:
		{
			Item* cont=ItemMngr.GetItem(item->ACC_CONTAINER.ContainerId,true);
			if(cont) cont->ContEraseItem(item);
		}
		break;
	default:
		break;
	}
	item->Accessory=45;
}

void ItemManager::MoveItem(Item* item, DWORD count, Critter* to_cr)
{
	if(item->Accessory==ITEM_ACCESSORY_CRITTER && item->ACC_CRITTER.Id==to_cr->GetId()) return;

	if(count>=item->GetCount() || !item->IsGrouped())
	{
		EraseItemHolder(item);
		to_cr->AddItem(item,true);
	}
	else
	{
		Item* item_=ItemMngr.SplitItem(item,count);
		if(item_)
		{
			NotifyChangeItem(item);
			to_cr->AddItem(item_,true);
		}
	}
}

void ItemManager::MoveItem(Item* item, DWORD count, Map* to_map, WORD to_hx, WORD to_hy)
{
	if(item->Accessory==ITEM_ACCESSORY_HEX && item->ACC_HEX.MapId==to_map->GetId() && item->ACC_HEX.HexX==to_hx && item->ACC_HEX.HexY==to_hy) return;

	if(count>=item->GetCount() || !item->IsGrouped())
	{
		EraseItemHolder(item);
		to_map->AddItem(item,to_hx,to_hy);
	}
	else
	{
		Item* item_=ItemMngr.SplitItem(item,count);
		if(item_)
		{
			NotifyChangeItem(item);
			to_map->AddItem(item_,to_hx,to_hy);
		}
	}
}

void ItemManager::MoveItem(Item* item, DWORD count, Item* to_cont, DWORD stack_id)
{
	if(item->Accessory==ITEM_ACCESSORY_CONTAINER && item->ACC_CONTAINER.ContainerId==to_cont->GetId() && item->ACC_CONTAINER.SpecialId==stack_id) return;

	if(count>=item->GetCount() || !item->IsGrouped())
	{
		EraseItemHolder(item);
		to_cont->ContAddItem(item,stack_id);
	}
	else
	{
		Item* item_=ItemMngr.SplitItem(item,count);
		if(item_)
		{
			NotifyChangeItem(item);
			to_cont->ContAddItem(item_,stack_id);
		}
	}
}

Item* ItemManager::AddItemContainer(Item* cont, WORD pid, DWORD count, DWORD stack_id)
{
	if(!cont || !cont->IsContainer()) return NULL;

	Item* item=cont->ContGetItemByPid(pid,stack_id);
	Item* result=NULL;

	if(item)
	{
		if(item->IsGrouped())
		{
			item->Count_Add(count);
			result=item;
		}
		else
		{
			if(count>MAX_ADDED_NOGROUP_ITEMS) count=MAX_ADDED_NOGROUP_ITEMS;
			for(int i=0;i<count;++i)
			{
				item=ItemMngr.CreateItem(pid,1);
				if(!item) continue;
				cont->ContAddItem(item,stack_id);
				result=item;
			}
		}
	}
	else
	{
		ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
		if(!proto_item) return result;

		if(proto_item->IsGrouped())
		{
			item=ItemMngr.CreateItem(pid,count);
			if(!item) return result;
			cont->ContAddItem(item,stack_id);
			result=item;
		}
		else
		{
			if(count>MAX_ADDED_NOGROUP_ITEMS) count=MAX_ADDED_NOGROUP_ITEMS;
			for(int i=0;i<count;++i)
			{
				item=ItemMngr.CreateItem(pid,1);
				if(!item) continue;
				cont->ContAddItem(item,stack_id);
				result=item;
			}
		}
	}

	return result;
}

Item* ItemManager::AddItemCritter(Critter* cr, WORD pid, DWORD count)
{
	if(!count) return NULL;

	Item* item=cr->GetItemByPid(pid);
	Item* result=NULL;

	if(item)
	{
		if(item->IsGrouped())
		{
			item->Count_Add(count);
			cr->SendAA_ItemData(item);
			result=item;
		}
		else
		{
			if(count>MAX_ADDED_NOGROUP_ITEMS) count=MAX_ADDED_NOGROUP_ITEMS;
			for(int i=0;i<count;++i)
			{
				item=ItemMngr.CreateItem(pid,1);
				if(!item) break;
				cr->AddItem(item,true);
				result=item;
			}
		}
	}
	else
	{
		ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
		if(!proto_item) return result;

		if(proto_item->IsGrouped())
		{
			item=ItemMngr.CreateItem(pid,count);
			if(!item) return result;
			cr->AddItem(item,true);
			result=item;
		}
		else
		{
			if(count>MAX_ADDED_NOGROUP_ITEMS) count=MAX_ADDED_NOGROUP_ITEMS;
			for(int i=0;i<count;++i)
			{
				item=ItemMngr.CreateItem(pid,1);
				if(!item) break;
				cr->AddItem(item,true);
				result=item;
			}
		}
	}

	return result;
}

bool ItemManager::SubItemCritter(Critter* cr, WORD pid, DWORD count, ItemPtrVec* erased_items)
{
	if(!count) return true;

	Item* item=cr->GetItemByPidInvPriority(pid);
	if(!item) return true;

	if(item->IsGrouped())
	{
		if(count>=item->GetCount())
		{
			cr->EraseItem(item,true);
			if(!erased_items) ItemMngr.ItemToGarbage(item);
			else erased_items->push_back(item);
		}
		else
		{
			if(erased_items)
			{
				Item* item_=ItemMngr.SplitItem(item,count);
				if(item_) erased_items->push_back(item_);
			}
			else
			{
				item->Count_Sub(count);
			}
			cr->SendAA_ItemData(item);
		}
	}
	else
	{
		for(int i=0;i<count;++i)
		{
			cr->EraseItem(item,true);
			if(!erased_items) ItemMngr.ItemToGarbage(item);
			else erased_items->push_back(item);
			item=cr->GetItemByPidInvPriority(pid);
			if(!item) return true;
		}
	}

	return true;
}

bool ItemManager::SetItemCritter(Critter* cr, WORD pid, DWORD count)
{
	DWORD cur_count=cr->CountItemPid(pid);
	if(cur_count>count) return SubItemCritter(cr,pid,cur_count-count);
	else if(cur_count<count) return AddItemCritter(cr,pid,count-cur_count)!=NULL;
	return true;
}

bool ItemManager::MoveItemCritters(Critter* from_cr, Critter* to_cr, DWORD item_id, DWORD count)
{
	Item* item=from_cr->GetItem(item_id,false);
	if(!item) return false;

	if(!count || count>item->GetCount()) count=item->GetCount();

	if(item->IsGrouped() && item->GetCount()>count)
	{
		Item* item_=to_cr->GetItemByPid(item->GetProtoId());
		if(!item_)
		{
			item_=ItemMngr.CreateItem(item->GetProtoId(),count);
			if(!item_)
			{
				WriteLog(__FUNCTION__" - Create item fail, pid<%u>.\n",item->GetProtoId());
				return false;
			}

			to_cr->AddItem(item_,true);
		}
		else
		{
			item_->Count_Add(count);
			to_cr->SendAA_ItemData(item_);
		}

		item->Count_Sub(count);
		from_cr->SendAA_ItemData(item);
	}
	else
	{
		from_cr->EraseItem(item,true);
		to_cr->AddItem(item,true);
	}

	return true;
}

bool ItemManager::MoveItemCritterToCont(Critter* from_cr, Item* to_cont, DWORD item_id, DWORD count, DWORD stack_id)
{
	if(!to_cont->IsContainer()) return false;

	Item* item=from_cr->GetItem(item_id,false);
	if(!item) return false;

	if(!count || count>item->GetCount()) count=item->GetCount();

	if(item->IsGrouped() && item->GetCount()>count)
	{
		Item* item_=to_cont->ContGetItemByPid(item->GetProtoId(),stack_id);
		if(!item_)
		{
			item_=ItemMngr.CreateItem(item->GetProtoId(),count);
			if(!item_)
			{
				WriteLog(__FUNCTION__" - Create item fail, pid<%u>.\n",item->GetProtoId());
				return false;
			}

			item_->ACC_CONTAINER.SpecialId=stack_id;
			to_cont->ContSetItem(item_);
		}
		else
		{
			item_->Count_Add(count);
		}

		item->Count_Sub(count);
		from_cr->SendAA_ItemData(item);
	}
	else
	{
		from_cr->EraseItem(item,true);
		to_cont->ContAddItem(item,stack_id);
	}

	return true;
}

bool ItemManager::MoveItemCritterFromCont(Item* from_cont, Critter* to_cr, DWORD item_id, DWORD count)
{
	if(!from_cont->IsContainer()) return false;

	Item* item=from_cont->ContGetItem(item_id,false);
	if(!item) return false;

	if(!count || count>item->GetCount()) count=item->GetCount();

	if(item->IsGrouped() && item->GetCount()>count)
	{
		Item* item_=to_cr->GetItemByPid(item->GetProtoId());
		if(!item_)
		{
			item_=ItemMngr.CreateItem(item->GetProtoId(),count);
			if(!item_)
			{
				WriteLog(__FUNCTION__" - Create item fail, pid<%u>.\n",item->GetProtoId());
				return false;
			}

			to_cr->AddItem(item_,true);
		}
		else
		{
			item_->Count_Add(count);
			to_cr->SendAA_ItemData(item_);
		}

		item->Count_Sub(count);
	}
	else
	{
		from_cont->ContEraseItem(item);
		to_cr->AddItem(item,true);
	}

	return true;
}

bool ItemManager::MoveItemsContainers(Item* from_cont, Item* to_cont, DWORD from_stack_id, DWORD to_stack_id)
{
	if(!from_cont->IsContainer() || !to_cont->IsContainer()) return false;
	if(!from_cont->ContIsItems()) return true;

	ItemPtrVec items;
	from_cont->ContGetItems(items,from_stack_id,true);
	for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=*it;
		from_cont->ContEraseItem(item);
		to_cont->ContAddItem(item,to_stack_id);
	}

	return true;
}

bool ItemManager::MoveItemsContToCritter(Item* from_cont, Critter* to_cr, DWORD stack_id)
{
	if(!from_cont->IsContainer()) return false;
	if(!from_cont->ContIsItems()) return true;

	ItemPtrVec items;
	from_cont->ContGetItems(items,stack_id,true);
	for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=*it;
		from_cont->ContEraseItem(item);
		to_cr->AddItem(item,true);
	}

	return true;
}

void ItemManager::RadioRegister(Item* radio, bool add)
{
	SCOPE_LOCK(radioItemsLocker);

	ItemPtrVecIt it=std::find(radioItems.begin(),radioItems.end(),radio);

	if(add)
	{
		if(it==radioItems.end()) radioItems.push_back(radio);
	}
	else
	{
		if(it!=radioItems.end()) radioItems.erase(it);
	}
}

void ItemManager::RadioSendText(Critter* cr, const char* text, WORD text_len, bool unsafe_text, WORD text_msg, DWORD num_str, WordVec& channels)
{
	ItemPtrVec radios;
	ItemPtrVec items=cr->GetItemsNoLock();
	for(ItemPtrVecIt it=items.begin(),end=items.end();it!=end;++it)
	{
		Item* item=*it;
		if(item->IsRadio() && item->RadioIsSendActive() &&
			std::find(channels.begin(),channels.end(),item->Data.Radio.Channel)==channels.end())
		{
			channels.push_back(item->Data.Radio.Channel);
			radios.push_back(item);

			if(radios.size()>100) break;
		}
	}

	for(size_t i=0,j=radios.size();i<j;i++)
	{
		RadioSendTextEx(channels[i],
			radios[i]->Data.Radio.BroadcastSend,cr->GetMap(),cr->Data.WorldX,cr->Data.WorldY,
			text,text_len,cr->IntellectCacheValue,unsafe_text,text_msg,num_str);
	}
}

void ItemManager::RadioSendTextEx(WORD channel, int broadcast_type, DWORD from_map_id, WORD from_wx, WORD from_wy,
								  const char* text, WORD text_len, WORD intellect, bool unsafe_text,
								  WORD text_msg, DWORD num_str)
{
	// Broadcast
	if(broadcast_type!=RADIO_BROADCAST_FORCE_ALL && broadcast_type!=RADIO_BROADCAST_WORLD &&
		broadcast_type!=RADIO_BROADCAST_MAP && broadcast_type!=RADIO_BROADCAST_LOCATION &&
		!(broadcast_type>=101 && broadcast_type<=200)/*RADIO_BROADCAST_ZONE*/) return;
	if((broadcast_type==RADIO_BROADCAST_MAP || broadcast_type==RADIO_BROADCAST_LOCATION) && !from_map_id) return;

	int broadcast=0;
	DWORD broadcast_map_id=0;
	DWORD broadcast_loc_id=0;

	// Get copy of all radios
	radioItemsLocker.Lock();
	ItemPtrVec radio_items=radioItems;
	radioItemsLocker.Unlock();

	// Multiple sending controlling
	// Not thread safe, but this not so important in this case
	static DWORD msg_count=0;
	msg_count++;

	// Send
	for(ItemPtrVecIt it=radio_items.begin(),end=radio_items.end();it!=end;++it)
	{
		Item* radio=*it;

		if(radio->Data.Radio.Channel==channel && radio->RadioIsRecvActive())
		{
			if(broadcast_type!=RADIO_BROADCAST_FORCE_ALL && radio->Data.Radio.BroadcastRecv!=RADIO_BROADCAST_FORCE_ALL)
			{
				if(broadcast_type==RADIO_BROADCAST_WORLD) broadcast=radio->Data.Radio.BroadcastRecv;
				else if(radio->Data.Radio.BroadcastRecv==RADIO_BROADCAST_WORLD) broadcast=broadcast_type;
				else broadcast=min(broadcast_type,radio->Data.Radio.BroadcastRecv);

				if(broadcast==RADIO_BROADCAST_WORLD) broadcast=RADIO_BROADCAST_FORCE_ALL;
				else if(broadcast==RADIO_BROADCAST_MAP || broadcast==RADIO_BROADCAST_LOCATION)
				{
					if(!broadcast_map_id)
					{
						Map* map=MapMngr.GetMap(from_map_id,false);
						if(!map) continue;
						broadcast_map_id=map->GetId();
						broadcast_loc_id=map->GetLocation(false)->GetId();
					}
				}
				else if(!(broadcast>=101 && broadcast<=200)/*RADIO_BROADCAST_ZONE*/) continue;
			}
			else
			{
				broadcast=RADIO_BROADCAST_FORCE_ALL;
			}

			if(radio->Accessory==ITEM_ACCESSORY_CRITTER)
			{
				Client* cl=CrMngr.GetPlayer(radio->ACC_CRITTER.Id,false);
				if(cl && cl->RadioMessageSended!=msg_count)
				{
					if(broadcast!=RADIO_BROADCAST_FORCE_ALL)
					{
						if(broadcast==RADIO_BROADCAST_MAP)
						{
							if(broadcast_map_id!=cl->GetMap()) continue;
						}
						else if(broadcast==RADIO_BROADCAST_LOCATION)
						{
							Map* map=MapMngr.GetMap(cl->GetMap(),false);
							if(!map || broadcast_loc_id!=map->GetLocation(false)->GetId()) continue;
						}
						else if(broadcast>=101 && broadcast<=200) // RADIO_BROADCAST_ZONE
						{
							if(!MapMngr.IsIntersectZone(from_wx,from_wy,0,cl->Data.WorldX,cl->Data.WorldY,0,broadcast-101)) continue;
						}
						else continue;
					}

					if(text) cl->Send_TextEx(radio->GetId(),text,text_len,SAY_RADIO,intellect,unsafe_text);
					else cl->Send_TextMsg(radio->GetId(),num_str,SAY_RADIO,text_msg);

					cl->RadioMessageSended=msg_count;
				}
			}
			else if(radio->Accessory==ITEM_ACCESSORY_HEX)
			{
				if(broadcast==RADIO_BROADCAST_MAP && broadcast_map_id!=radio->ACC_HEX.MapId) continue;

				Map* map=MapMngr.GetMap(radio->ACC_HEX.MapId,false);
				if(map)
				{
					if(broadcast!=RADIO_BROADCAST_FORCE_ALL && broadcast!=RADIO_BROADCAST_MAP)
					{
						if(broadcast==RADIO_BROADCAST_LOCATION)
						{
							Location* loc=map->GetLocation(false);
							if(broadcast_loc_id!=loc->GetId()) continue;
						}
						else if(broadcast>=101 && broadcast<=200) // RADIO_BROADCAST_ZONE
						{
							Location* loc=map->GetLocation(false);
							if(!MapMngr.IsIntersectZone(from_wx,from_wy,0,loc->Data.WX,loc->Data.WY,loc->GetRadius(),broadcast-101)) continue;
						}
						else continue;
					}

					if(text) map->SetText(radio->ACC_HEX.HexX,radio->ACC_HEX.HexY,0xFFFFFFFE,text,text_len,intellect,unsafe_text);
					else map->SetTextMsg(radio->ACC_HEX.HexX,radio->ACC_HEX.HexY,0xFFFFFFFE,text_msg,num_str);
				}
			}
		}
	}
}
#endif // FONLINE_SERVER

void ItemManager::AddItemStatistics(WORD pid, DWORD val)
{
	if(!IsInitProto(pid)) return;

	SCOPE_SPINLOCK(itemCountLocker);

	itemCount[pid]+=(__int64)val;
}

void ItemManager::SubItemStatistics(WORD pid, DWORD val)
{
	if(!IsInitProto(pid)) return;

	SCOPE_SPINLOCK(itemCountLocker);

	itemCount[pid]-=(__int64)val;
}

__int64 ItemManager::GetItemStatistics(WORD pid)
{
	SCOPE_SPINLOCK(itemCountLocker);

	__int64 count=itemCount[pid];
	return count;
}

string ItemManager::GetItemsStatistics()
{
	SCOPE_SPINLOCK(itemCountLocker);

	string result="Pid    Name                                     Count\n";
	if(IsInit())
	{
		char str[512];
		char str_[128];
		char str__[128];
		for(int i=0;i<MAX_ITEM_PROTOTYPES;i++)
		{
			ProtoItem* proto_item=GetProtoItem(i);
			if(proto_item && proto_item->IsItem())
			{
				char* s=(char*)FONames::GetItemName(i);
				sprintf(str,"%-6u %-40s %-20s\n",i,s?s:_itoa(i,str_,10),_i64toa(itemCount[i],str__,10));
				result+=str;
			}
		}
	}
	return result;
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================