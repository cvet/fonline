#include "Defines.h"

	// Options
	if(engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES,true)<0) BIND_ERROR;
	if(engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE,true)<0) BIND_ERROR;
	if(engine->SetEngineProperty(asEP_SCRIPT_SCANNER,0)<0) BIND_ERROR;

#if defined(BIND_CLIENT) || defined(BIND_SERVER)
	// Parameters arrays
	if(engine->RegisterObjectType("DataRef",0,asOBJ_REF|asOBJ_NOHANDLE)<0) BIND_ERROR;
#ifdef BIND_SERVER
	if(engine->RegisterObjectBehaviour("DataRef",asBEHAVE_INDEX,"int &f(uint)",asFUNCTION(BIND_CLASS DataRef_Index),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("DataRef",asBEHAVE_INDEX,"const int &f(uint) const",asFUNCTION(BIND_CLASS DataRef_Index),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
#else
	if(engine->RegisterObjectBehaviour("DataRef",asBEHAVE_INDEX,"const int f(uint) const",asFUNCTION(BIND_CLASS DataRef_Index),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
#endif
	if(engine->RegisterObjectType("DataVal",0,asOBJ_REF|asOBJ_NOHANDLE)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("DataVal",asBEHAVE_INDEX,"const int f(uint) const",asFUNCTION(BIND_CLASS DataVal_Index),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
#endif

	// Item prototype
	if(engine->RegisterObjectType("ProtoItem",0,asOBJ_REF)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("ProtoItem",asBEHAVE_ADDREF,"void f()",asMETHOD(ProtoItem,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("ProtoItem",asBEHAVE_RELEASE,"void f()",asMETHOD(ProtoItem,Release),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ProtoItem","uint8 GetType() const",asMETHOD(ProtoItem,GetType),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ProtoItem","uint16 GetProtoId() const",asMETHOD(ProtoItem,GetPid),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ProtoItem","bool IsGrouped() const",asMETHOD(ProtoItem,IsGrouped),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ProtoItem","bool IsWeared() const",asMETHOD(ProtoItem,IsWeared),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ProtoItem","void Weapon_SetUse(uint8 use) const",asMETHOD(ProtoItem,Weapon_SetUse),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ProtoItem","bool Container_IsGroundLevel() const",asMETHOD(ProtoItem,Container_IsGroundLevel),asCALL_THISCALL)<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("ProtoItem","const uint Flags",offsetof(ProtoItem,Flags))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Corner",offsetof(ProtoItem,Corner))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 LightDistance",offsetof(ProtoItem,LightDistance))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint LightIntensity",offsetof(ProtoItem,LightIntensity))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint LightColor",offsetof(ProtoItem,LightColor))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint Weight",offsetof(ProtoItem,Weight))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Volume",offsetof(ProtoItem,Volume))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint PicMap",offsetof(ProtoItem,PicMapHash))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint PicInv",offsetof(ProtoItem,PicInvHash))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Sound",offsetof(ProtoItem,SoundId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint Cost",offsetof(ProtoItem,Cost))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Material",offsetof(ProtoItem,Material))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Slot",offsetof(ProtoItem,Slot))<0) BIND_ERROR;

	// Misc2
	if(engine->RegisterObjectProperty("ProtoItem","const uint Misc2_StartVal1",offsetof(ProtoItem,MiscEx.StartVal1))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint Misc2_StartVal2",offsetof(ProtoItem,MiscEx.StartVal2))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint Misc2_StartVal3",offsetof(ProtoItem,MiscEx.StartVal3))<0) BIND_ERROR;

	// Car
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Car_Speed",offsetof(ProtoItem,MiscEx.Car.Speed))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Car_Negotiability",offsetof(ProtoItem,MiscEx.Car.Negotiability))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Car_WearConsumption",offsetof(ProtoItem,MiscEx.Car.WearConsumption))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Car_CritCapacity",offsetof(ProtoItem,MiscEx.Car.CritCapacity))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Car_FuelTank",offsetof(ProtoItem,MiscEx.Car.FuelTank))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Car_RunToBreak",offsetof(ProtoItem,MiscEx.Car.RunToBreak))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Car_WalkType",offsetof(ProtoItem,MiscEx.Car.WalkType))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Car_FuelConsumption",offsetof(ProtoItem,MiscEx.Car.FuelConsumption))<0) BIND_ERROR;

	// Armor
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Armor_CrTypeMale",offsetof(ProtoItem,Armor.Anim0Male))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Armor_CrTypeFemale",offsetof(ProtoItem,Armor.Anim0Female))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_AC",offsetof(ProtoItem,Armor.AC))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DRNormal",offsetof(ProtoItem,Armor.DRNormal))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DRLaser",offsetof(ProtoItem,Armor.DRLaser))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DRFire",offsetof(ProtoItem,Armor.DRFire))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DRPlasma",offsetof(ProtoItem,Armor.DRPlasma))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DRElectro",offsetof(ProtoItem,Armor.DRElectr))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DREmp",offsetof(ProtoItem,Armor.DREmp))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DRExplode",offsetof(ProtoItem,Armor.DRExplode))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DTNormal",offsetof(ProtoItem,Armor.DTNormal))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DTLaser",offsetof(ProtoItem,Armor.DTLaser))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DTFire",offsetof(ProtoItem,Armor.DTFire))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DTPlasma",offsetof(ProtoItem,Armor.DTPlasma))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DTElectro",offsetof(ProtoItem,Armor.DTElectr))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DTEmp",offsetof(ProtoItem,Armor.DTEmp))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Armor_DTExplode",offsetof(ProtoItem,Armor.DTExplode))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Armor_Perk",offsetof(ProtoItem,Armor.Perk))<0) BIND_ERROR;

	// Door
	if(engine->RegisterObjectProperty("ProtoItem","const bool Door_NoBlockMove",offsetof(ProtoItem,Door.NoBlockMove))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const bool Door_NoBlockShoot",offsetof(ProtoItem,Door.NoBlockShoot))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const bool Door_NoBlockLight",offsetof(ProtoItem,Door.NoBlockLight))<0) BIND_ERROR;

	// Container
	if(engine->RegisterObjectProperty("ProtoItem","const bool Container_IsChangeble",offsetof(ProtoItem,Container.Changeble))<0) BIND_ERROR;

	// Weapon
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_Uses",offsetof(ProtoItem,Weapon.CountAttack))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const bool Weapon_RequiresActivation",offsetof(ProtoItem,Weapon.IsNeedAct))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const bool Weapon_IsUnarmed",offsetof(ProtoItem,Weapon.IsUnarmed))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_UnarmedCriticalBonus",offsetof(ProtoItem,Weapon.UnarmedCriticalBonus))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const bool Weapon_UnarmedArmorPiercing",offsetof(ProtoItem,Weapon.UnarmedArmorPiercing))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_MinStrength",offsetof(ProtoItem,Weapon.MinSt))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_Perk",offsetof(ProtoItem,Weapon.Perk))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_Anim1",offsetof(ProtoItem,Weapon.Anim1))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Weapon_VolumeHolder",offsetof(ProtoItem,Weapon.VolHolder))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Weapon_Caliber",offsetof(ProtoItem,Weapon.Caliber))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_CriticalFailture",offsetof(ProtoItem,Weapon.CrFailture))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Weapon_DefAmmo",offsetof(ProtoItem,Weapon.DefAmmo))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_ReloadAp",offsetof(ProtoItem,Weapon.ReloadAp))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_Skill",offsetof(ProtoItem,Weapon.Weapon_Skill))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_DmgType",offsetof(ProtoItem,Weapon.Weapon_DmgType))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_Anim2",offsetof(ProtoItem,Weapon.Weapon_Anim2))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Weapon_DmgMin",offsetof(ProtoItem,Weapon.Weapon_DmgMin))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Weapon_DmgMax",offsetof(ProtoItem,Weapon.Weapon_DmgMax))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Weapon_MaxDist",offsetof(ProtoItem,Weapon.Weapon_MaxDist))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Weapon_Effect",offsetof(ProtoItem,Weapon.Weapon_Effect))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint16 Weapon_Round",offsetof(ProtoItem,Weapon.Weapon_Round))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_ApCost",offsetof(ProtoItem,Weapon.Weapon_ApCost))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_SoundId",offsetof(ProtoItem,Weapon.Weapon_SoundId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const bool Weapon_Remove",offsetof(ProtoItem,Weapon.Weapon_Remove))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const bool Weapon_Aim",offsetof(ProtoItem,Weapon.Weapon_Aim))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint8 Weapon_CurrentUse",offsetof(ProtoItem,Weapon.Weapon_CurrentUse))<0) BIND_ERROR;

	// Ammo
	if(engine->RegisterObjectProperty("ProtoItem","const uint Ammo_StartCount",offsetof(ProtoItem,Ammo.StartCount))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint Ammo_Caliber",offsetof(ProtoItem,Ammo.Caliber))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const int Ammo_AcMod",offsetof(ProtoItem,Ammo.ACMod))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const int Ammo_DrMod",offsetof(ProtoItem,Ammo.DRMod))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint Ammo_DmgMult",offsetof(ProtoItem,Ammo.DmgMult))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoItem","const uint Ammo_DmgDiv",offsetof(ProtoItem,Ammo.DmgDiv))<0) BIND_ERROR;

#ifdef BIND_SERVER
/************************************************************************/
/* Types                                                                */
/************************************************************************/
	if(engine->RegisterObjectType("GameVar",0,asOBJ_REF)<0) BIND_ERROR;
	if(engine->RegisterObjectType("NpcPlane",0,asOBJ_REF)<0) BIND_ERROR;
	if(engine->RegisterObjectType("Item",0,asOBJ_REF)<0) BIND_ERROR;
	if(engine->RegisterObjectType("Scenery",0,asOBJ_REF)<0) BIND_ERROR;
	if(engine->RegisterObjectType("Critter",0,asOBJ_REF)<0) BIND_ERROR;
	if(engine->RegisterObjectType("Map",0,asOBJ_REF)<0) BIND_ERROR;
	if(engine->RegisterObjectType("Location",0,asOBJ_REF)<0) BIND_ERROR;

/************************************************************************/
/* GameVar                                                              */
/************************************************************************/
	if(engine->RegisterObjectBehaviour("GameVar",asBEHAVE_ADDREF,"void f()",asMETHOD(GameVar,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("GameVar",asBEHAVE_RELEASE,"void f()",asMETHOD(GameVar,Release),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int GetValue() const",asMETHOD(GameVar,GetValue),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int GetMin() const",asMETHOD(GameVar,GetMin),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int GetMax() const",asMETHOD(GameVar,GetMax),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","bool IsQuest() const",asMETHOD(GameVar,IsQuest),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","uint GetQuestStr() const",asMETHOD(GameVar,GetQuestStr),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","GameVar& opAddAssign(const int)",asMETHODPR(GameVar,operator+=,(const int),GameVar&),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","GameVar& opSubAssign(const int)",asMETHODPR(GameVar,operator-=,(const int),GameVar&),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","GameVar& opMulAssign(const int)",asMETHODPR(GameVar,operator*=,(const int),GameVar&),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","GameVar& opDivAssign(const int)",asMETHODPR(GameVar,operator/=,(const int),GameVar&),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","GameVar& opAssign(const int)",asMETHODPR(GameVar,operator=,(const int),GameVar&),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","GameVar& opAddAssign(const GameVar&)",asMETHODPR(GameVar,operator+=,(const GameVar&),GameVar&),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","GameVar& opSubAssign(const GameVar&)",asMETHODPR(GameVar,operator-=,(const GameVar&),GameVar&),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","GameVar& opMulAssign(const GameVar&)",asMETHODPR(GameVar,operator*=,(const GameVar&),GameVar&),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","GameVar& opDivAssign(const GameVar&)",asMETHODPR(GameVar,operator/=,(const GameVar&),GameVar&),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","GameVar& opAssign(const GameVar&)",asMETHODPR(GameVar,operator=,(const GameVar&),GameVar&),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int opAdd(const int)",asFUNCTION(GameVarAddInt),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int opSub(const int)",asFUNCTION(GameVarSubInt),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int opMul(const int)",asFUNCTION(GameVarMulInt),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int opDiv(const int)",asFUNCTION(GameVarDivInt),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int opAdd(const GameVar&)",asFUNCTION(GameVarAddGameVar),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int opSub(const GameVar&)",asFUNCTION(GameVarSubGameVar),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int opMul(const GameVar&)",asFUNCTION(GameVarMulGameVar),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int opDiv(const GameVar&)",asFUNCTION(GameVarDivGameVar),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","bool opEquals(const int)",asFUNCTION(GameVarEqualInt),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int opCmp(const int)",asFUNCTION(GameVarCmpInt),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","bool opEquals(const GameVar&)",asFUNCTION(GameVarEqualGameVar),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("GameVar","int opCmp(const GameVar&)",asFUNCTION(GameVarCmpGameVar),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

/************************************************************************/
/* NpcPlane                                                             */
/************************************************************************/
	if(engine->RegisterObjectBehaviour("NpcPlane",asBEHAVE_ADDREF,"void f()",asMETHOD(AIDataPlane,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("NpcPlane",asBEHAVE_RELEASE,"void f()",asMETHOD(AIDataPlane,Release),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","int Type",offsetof(AIDataPlane,Type))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint Priority",offsetof(AIDataPlane,Priority))<0) BIND_ERROR;
	//if(engine->RegisterObjectProperty("NpcPlane","NpcPlane@ Child",offsetof(AIDataPlane,Type))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","int Identifier",offsetof(AIDataPlane,Identifier))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint IdentifierExt",offsetof(AIDataPlane,IdentifierExt))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","bool Run",offsetof(AIDataPlane,Pick.IsRun))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint Misc_WaitMinute",offsetof(AIDataPlane,Misc.WaitMinute))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","int Misc_ScriptId",offsetof(AIDataPlane,Misc.ScriptBindId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint Attack_TargId",offsetof(AIDataPlane,Attack.TargId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","int Attack_MinHp",offsetof(AIDataPlane,Attack.MinHp))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","bool Attack_IsGag",offsetof(AIDataPlane,Attack.IsGag))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint16 Attack_GagHexX",offsetof(AIDataPlane,Attack.GagHexX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint16 Attack_GagHexY",offsetof(AIDataPlane,Attack.GagHexY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint16 Attack_LastHexX",offsetof(AIDataPlane,Attack.LastHexX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint16 Attack_LastHexY",offsetof(AIDataPlane,Attack.LastHexY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint16 Walk_HexX",offsetof(AIDataPlane,Walk.HexX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint16 Walk_HexY",offsetof(AIDataPlane,Walk.HexY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint8 Walk_Dir",offsetof(AIDataPlane,Walk.Dir))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint8 Walk_Cut",offsetof(AIDataPlane,Walk.Cut))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint16 Pick_HexX",offsetof(AIDataPlane,Pick.HexX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint16 Pick_HexY",offsetof(AIDataPlane,Pick.HexY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint16 Pick_Pid",offsetof(AIDataPlane,Pick.Pid))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","uint Pick_UseItemId",offsetof(AIDataPlane,Pick.UseItemId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("NpcPlane","bool Pick_ToOpen",offsetof(AIDataPlane,Pick.ToOpen))<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("NpcPlane","NpcPlane@ GetCopy() const",asFUNCTION(BIND_CLASS NpcPlane_GetCopy),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("NpcPlane","NpcPlane@+ SetChild(NpcPlane& child)",asFUNCTION(BIND_CLASS NpcPlane_SetChild),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("NpcPlane","NpcPlane@+ GetChild(uint index) const",asFUNCTION(BIND_CLASS NpcPlane_GetChild),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	//if(engine->RegisterObjectMethod("NpcPlane","uint GetIndex() const",asFUNCTION(BIND_CLASS NpcPlane_GetIndex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("NpcPlane","bool Misc_SetScript(string& funcName)",asFUNCTION(BIND_CLASS NpcPlane_Misc_SetScript),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

/************************************************************************/
/* Object                                                               */
/************************************************************************/
	// Methods
	if(engine->RegisterObjectBehaviour("Item",asBEHAVE_ADDREF,"void f()",asMETHOD(Item,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("Item",asBEHAVE_RELEASE,"void f()",asMETHOD(Item,Release),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","bool IsGrouped() const",asFUNCTION(BIND_CLASS Item_IsGrouped),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","bool IsWeared() const",asFUNCTION(BIND_CLASS Item_IsWeared),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","bool SetScript(string@+ script)",asFUNCTION(BIND_CLASS Item_SetScript),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","uint GetScriptId() const",asFUNCTION(BIND_CLASS Item_GetScriptId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","bool SetEvent(int eventType, string@+ funcName)",asFUNCTION(BIND_CLASS Item_SetEvent),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","uint8 GetType() const",asFUNCTION(BIND_CLASS Item_GetType),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","uint16 GetProtoId() const",asFUNCTION(BIND_CLASS Item_GetProtoId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","uint GetCount() const",asFUNCTION(BIND_CLASS Item_GetCount),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","void SetCount(uint count)",asFUNCTION(BIND_CLASS Item_SetCount),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","uint GetCost() const",asFUNCTION(BIND_CLASS Item_GetCost),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","Item@+ AddItem(uint16 protoId, uint count, uint specialId)",asFUNCTION(BIND_CLASS Container_AddItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","Item@+ GetItem(uint16 protoId, uint specialId) const",asFUNCTION(BIND_CLASS Container_GetItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","uint GetItems(uint specialId, Item@[]@+ items) const",asFUNCTION(BIND_CLASS Container_GetItems),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","Map@+ GetMapPosition(uint16& hexX, uint16& hexY) const",asFUNCTION(BIND_CLASS Item_GetMapPosition),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","bool ChangeProto(uint16 protoId) const",asFUNCTION(BIND_CLASS Item_ChangeProto),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","void Update() const",asFUNCTION(BIND_CLASS Item_Update),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","void Animate(uint8 fromFrame, uint8 toFrame)",asFUNCTION(BIND_CLASS Item_Animate),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","void SetLexems(string@+ lexems)",asFUNCTION(BIND_CLASS Item_SetLexems),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Item","void EventFinish(bool deleted)",asFUNCTION(BIND_CLASS Item_EventFinish),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","bool EventAttack(Critter& attacker, Critter& target)",asFUNCTION(BIND_CLASS Item_EventAttack),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","bool EventUse(Critter& cr, Critter@+ onCritter, Item@+ onItem, Scenery@+ onScenery)",asFUNCTION(BIND_CLASS Item_EventUse),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","bool EventUseOnMe(Critter& cr, Item@+ usedItem)",asFUNCTION(BIND_CLASS Item_EventUseOnMe),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","bool EventSkill(Critter& cr, int skill)",asFUNCTION(BIND_CLASS Item_EventSkill),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","void EventDrop(Critter& cr)",asFUNCTION(BIND_CLASS Item_EventDrop),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","void EventMove(Critter& cr, uint8 fromSlot)",asFUNCTION(BIND_CLASS Item_EventMove),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","void EventWalk(Critter& cr, bool entered, uint8 dir)",asFUNCTION(BIND_CLASS Item_EventWalk),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	// Parameters
	if(engine->RegisterObjectProperty("Item","const uint Id",offsetof(Item,Id))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","const ProtoItem@ Proto",offsetof(Item,Proto))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","const uint8 Accessory",offsetof(Item,Accessory))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","const uint MapId",offsetof(Item,ACC_HEX.MapId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","const uint16 HexX",offsetof(Item,ACC_HEX.HexX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","const uint16 HexY",offsetof(Item,ACC_HEX.HexY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","const uint CritId",offsetof(Item,ACC_CRITTER.Id))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","const uint8 CritSlot",offsetof(Item,ACC_CRITTER.Slot))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","const uint ContainerId",offsetof(Item,ACC_CONTAINER.ContainerId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","const uint StackId",offsetof(Item,ACC_CONTAINER.SpecialId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","const bool IsNotValid",offsetof(Item,IsNotValid))<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("Item","uint16 SortValue",offsetof(Item,Data.SortValue))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_SortValue(uint16 val)",asFUNCTION(BIND_CLASS Item_set_SortValue),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint16 get_SortValue() const",asFUNCTION(BIND_CLASS Item_get_SortValue),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint8 Info",offsetof(Item,Data.Info))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Info(uint8 val)",asFUNCTION(BIND_CLASS Item_set_Info),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_Info() const",asFUNCTION(BIND_CLASS Item_get_Info),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint PicMap",offsetof(Item,Data.PicMapHash))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_PicMap(uint val)",asFUNCTION(BIND_CLASS Item_set_PicMap),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint get_PicMap() const",asFUNCTION(BIND_CLASS Item_get_PicMap),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint PicInv",offsetof(Item,Data.PicInvHash))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_PicInv(uint val)",asFUNCTION(BIND_CLASS Item_set_PicInv),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint get_PicInv() const",asFUNCTION(BIND_CLASS Item_get_PicInv),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint16 AnimWaitBase",offsetof(Item,Data.AnimWaitBase))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_AnimWaitBase(uint16 val)",asFUNCTION(BIND_CLASS Item_set_AnimWaitBase),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_AnimWaitBase() const",asFUNCTION(BIND_CLASS Item_get_AnimWaitBase),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint8 AnimStayBegin",offsetof(Item,Data.AnimStay[0]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_AnimStayBegin(uint8 val)",asFUNCTION(BIND_CLASS Item_set_AnimStayBegin),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_AnimStayBegin() const",asFUNCTION(BIND_CLASS Item_get_AnimStayBegin),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint8 AnimStayEnd",offsetof(Item,Data.AnimStay[1]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_AnimStayEnd(uint8 val)",asFUNCTION(BIND_CLASS Item_set_AnimStayEnd),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_AnimStayEnd() const",asFUNCTION(BIND_CLASS Item_get_AnimStayEnd),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint8 AnimShowBegin",offsetof(Item,Data.AnimShow[0]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_AnimShowBegin(uint8 val)",asFUNCTION(BIND_CLASS Item_set_AnimShowBegin),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_AnimShowBegin() const",asFUNCTION(BIND_CLASS Item_get_AnimShowBegin),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint8 AnimShowEnd",offsetof(Item,Data.AnimShow[1]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_AnimShowEnd(uint8 val)",asFUNCTION(BIND_CLASS Item_set_AnimShowEnd),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_AnimShowEnd() const",asFUNCTION(BIND_CLASS Item_get_AnimShowEnd),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint8 AnimHideBegin",offsetof(Item,Data.AnimHide[0]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_AnimHideBegin(uint8 val)",asFUNCTION(BIND_CLASS Item_set_AnimHideBegin),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_AnimHideBegin() const",asFUNCTION(BIND_CLASS Item_get_AnimHideBegin),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint8 AnimHideEnd",offsetof(Item,Data.AnimHide[1]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_AnimHideEnd(uint8 val)",asFUNCTION(BIND_CLASS Item_set_AnimHideEnd),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_AnimHideEnd() const",asFUNCTION(BIND_CLASS Item_get_AnimHideEnd),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Item","void set_Flags(uint value)",asFUNCTION(BIND_CLASS Item_set_Flags),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","uint get_Flags() const",asFUNCTION(BIND_CLASS Item_get_Flags),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","void set_TrapValue(int16 val)",asFUNCTION(BIND_CLASS Item_set_TrapValue),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","int16 get_TrapValue() const",asFUNCTION(BIND_CLASS Item_get_TrapValue),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

//	if(engine->RegisterObjectProperty("Item","uint Flags",offsetof(Item,Data.Flags))<0) BIND_ERROR;
//	if(engine->RegisterObjectProperty("Item","int16 TrapValue",offsetof(Item,Data.TrapValue))<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("Item","uint Cost",offsetof(Item,Data.Cost))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Cost(uint val)",asFUNCTION(BIND_CLASS Item_set_Cost),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint get_Cost() const",asFUNCTION(BIND_CLASS Item_get_Cost),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","int Val0",offsetof(Item,Data.ScriptValues[0]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Val0(int val)",asFUNCTION(BIND_CLASS Item_set_Val0),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","int get_Val0() const",asFUNCTION(BIND_CLASS Item_get_Val0),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","int Val1",offsetof(Item,Data.ScriptValues[1]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Val1(int val)",asFUNCTION(BIND_CLASS Item_set_Val1),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","int get_Val1() const",asFUNCTION(BIND_CLASS Item_get_Val1),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","int Val2",offsetof(Item,Data.ScriptValues[2]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Val2(int val)",asFUNCTION(BIND_CLASS Item_set_Val2),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","int get_Val2() const",asFUNCTION(BIND_CLASS Item_get_Val2),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","int Val3",offsetof(Item,Data.ScriptValues[3]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Val3(int val)",asFUNCTION(BIND_CLASS Item_set_Val3),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","int get_Val3() const",asFUNCTION(BIND_CLASS Item_get_Val3),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","int Val4",offsetof(Item,Data.ScriptValues[4]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Val4(int val)",asFUNCTION(BIND_CLASS Item_set_Val4),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","int get_Val4() const",asFUNCTION(BIND_CLASS Item_get_Val4),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","int Val5",offsetof(Item,Data.ScriptValues[5]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Val5(int val)",asFUNCTION(BIND_CLASS Item_set_Val5),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","int get_Val5() const",asFUNCTION(BIND_CLASS Item_get_Val5),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","int Val6",offsetof(Item,Data.ScriptValues[6]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Val6(int val)",asFUNCTION(BIND_CLASS Item_set_Val6),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","int get_Val6() const",asFUNCTION(BIND_CLASS Item_get_Val6),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","int Val7",offsetof(Item,Data.ScriptValues[7]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Val7(int val)",asFUNCTION(BIND_CLASS Item_set_Val7),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","int get_Val7() const",asFUNCTION(BIND_CLASS Item_get_Val7),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","int Val8",offsetof(Item,Data.ScriptValues[8]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Val8(int val)",asFUNCTION(BIND_CLASS Item_set_Val8),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","int get_Val8() const",asFUNCTION(BIND_CLASS Item_get_Val8),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","int Val9",offsetof(Item,Data.ScriptValues[9]))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_Val9(int val)",asFUNCTION(BIND_CLASS Item_set_Val9),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","int get_Val9() const",asFUNCTION(BIND_CLASS Item_get_Val9),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","int8 LightIntensity",offsetof(Item,Data.LightIntensity))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_LightIntensity(int8 val)",asFUNCTION(BIND_CLASS Item_set_LightIntensity),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","int8 get_LightIntensity() const",asFUNCTION(BIND_CLASS Item_get_LightIntensity),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint8 LightDistance",offsetof(Item,Data.LightDistance))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_LightDistance(uint8 val)",asFUNCTION(BIND_CLASS Item_set_LightDistance),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_LightDistance() const",asFUNCTION(BIND_CLASS Item_get_LightDistance),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint8 LightFlags",offsetof(Item,Data.LightFlags))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_LightFlags(uint8 val)",asFUNCTION(BIND_CLASS Item_set_LightFlags),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_LightFlags() const",asFUNCTION(BIND_CLASS Item_get_LightFlags),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint LightColor",offsetof(Item,Data.LightColor))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_LightColor(uint val)",asFUNCTION(BIND_CLASS Item_set_LightColor),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint get_LightColor() const",asFUNCTION(BIND_CLASS Item_get_LightColor),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	// Tech info
	if(engine->RegisterObjectProperty("Item","uint8 BrokenFlags",offsetof(Item,Data.TechInfo.DeteorationFlags))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_BrokenFlags(uint8 val)",asFUNCTION(BIND_CLASS Item_set_BrokenFlags),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_BrokenFlags() const",asFUNCTION(BIND_CLASS Item_get_BrokenFlags),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint8 BrokenCount",offsetof(Item,Data.TechInfo.DeteorationCount))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_BrokenCount(uint8 val)",asFUNCTION(BIND_CLASS Item_set_BrokenCount),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint8 get_BrokenCount() const",asFUNCTION(BIND_CLASS Item_get_BrokenCount),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint16 BrokenWear",offsetof(Item,Data.TechInfo.DeteorationValue))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_BrokenWear(uint16 val)",asFUNCTION(BIND_CLASS Item_set_BrokenWear),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint16 get_BrokenWear() const",asFUNCTION(BIND_CLASS Item_get_BrokenWear),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint16 WeaponAmmoPid",offsetof(Item,Data.TechInfo.AmmoPid))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_WeaponAmmoPid(uint16 val)",asFUNCTION(BIND_CLASS Item_set_WeaponAmmoPid),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint16 get_WeaponAmmoPid() const",asFUNCTION(BIND_CLASS Item_get_WeaponAmmoPid),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint16 WeaponAmmoCount",offsetof(Item,Data.TechInfo.AmmoCount))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_WeaponAmmoCount(uint16 val)",asFUNCTION(BIND_CLASS Item_set_WeaponAmmoCount),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint16 get_WeaponAmmoCount() const",asFUNCTION(BIND_CLASS Item_get_WeaponAmmoCount),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	// Locker
	if(engine->RegisterObjectMethod("Item","bool LockerOpen()",asFUNCTION(BIND_CLASS Item_LockerOpen),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","bool LockerClose()",asFUNCTION(BIND_CLASS Item_LockerClose),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint LockerId",offsetof(Item,Data.Locker.DoorId))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_LockerId(uint val)",asFUNCTION(BIND_CLASS Item_set_LockerId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint get_LockerId() const",asFUNCTION(BIND_CLASS Item_get_LockerId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint16 LockerCondition",offsetof(Item,Data.Locker.Condition))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_LockerCondition(uint16 val)",asFUNCTION(BIND_CLASS Item_set_LockerCondition),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint16 get_LockerCondition() const",asFUNCTION(BIND_CLASS Item_get_LockerCondition),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint16 LockerComplexity",offsetof(Item,Data.Locker.Complexity))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_LockerComplexity(uint16 val)",asFUNCTION(BIND_CLASS Item_set_LockerComplexity),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint16 get_LockerComplexity() const",asFUNCTION(BIND_CLASS Item_get_LockerComplexity),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	// Car
	if(engine->RegisterObjectMethod("Item","bool IsCar()",asFUNCTION(BIND_CLASS Item_IsCar),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Item","Item@+ CarGetBag(int numBag)",asFUNCTION(BIND_CLASS Item_CarGetBag),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint16 CarFuel",offsetof(Item,Data.Car.Fuel))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_CarFuel(uint16 val)",asFUNCTION(BIND_CLASS Item_set_CarFuel),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint16 get_CarFuel() const",asFUNCTION(BIND_CLASS Item_get_CarFuel),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Item","uint16 CarDeteoration",offsetof(Item,Data.Car.Deteoration))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_CarDeteoration(uint16 val)",asFUNCTION(BIND_CLASS Item_set_CarDeteoration),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint16 get_CarDeteoration() const",asFUNCTION(BIND_CLASS Item_get_CarDeteoration),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	// Radio
	if(engine->RegisterObjectProperty("Item","uint16 RadioChannel",offsetof(Item,Data.Radio.Channel))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_RadioChannel(uint16 val)",asFUNCTION(BIND_CLASS Item_set_RadioChannel),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint16 get_RadioChannel() const",asFUNCTION(BIND_CLASS Item_get_RadioChannel),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	// Holodisk
	if(engine->RegisterObjectProperty("Item","uint HolodiskNumber",offsetof(Item,Data.Holodisk.Number))<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","void set_HolodiskNumber(uint val)",asFUNCTION(BIND_CLASS Item_set_HolodiskNumber),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Item","uint get_HolodiskNumber()",asFUNCTION(BIND_CLASS Item_get_HolodiskNumber),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

/************************************************************************/
/* Scenery                                                              */
/************************************************************************/
	if(engine->RegisterObjectBehaviour("Scenery",asBEHAVE_ADDREF,"void f()",asMETHOD(MapObject,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("Scenery",asBEHAVE_RELEASE,"void f()",asMETHOD(MapObject,Release),asCALL_THISCALL)<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("Scenery","const uint16 ProtoId",offsetof(MapObject,ProtoId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Scenery","const uint16 HexX",offsetof(MapObject,MapX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Scenery","const uint16 HexY",offsetof(MapObject,MapY))<0) BIND_ERROR;

/************************************************************************/
/* Critter                                                              */
/************************************************************************/
	if(engine->RegisterObjectBehaviour("Critter",asBEHAVE_ADDREF,"void f()",asMETHOD(Critter,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("Critter",asBEHAVE_RELEASE,"void f()",asMETHOD(Critter,Release),asCALL_THISCALL)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","bool IsPlayer() const",asFUNCTION(BIND_CLASS Crit_IsPlayer),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsNpc() const",asFUNCTION(BIND_CLASS Crit_IsNpc),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsCanWalk() const",asFUNCTION(BIND_CLASS Crit_IsCanWalk),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsCanRun() const",asFUNCTION(BIND_CLASS Crit_IsCanRun),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsCanRotate() const",asFUNCTION(BIND_CLASS Crit_IsCanRotate),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsCanAim() const",asFUNCTION(BIND_CLASS Crit_IsCanAim),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsAnim1(uint index) const",asFUNCTION(BIND_CLASS Crit_IsAnim1),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsAnim3d() const",asFUNCTION(BIND_CLASS Crit_IsAnim3d),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int GetAccess() const",asFUNCTION(BIND_CLASS Cl_GetAccess),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","bool SetEvent(int eventType, string@+ funcName)",asFUNCTION(BIND_CLASS Crit_SetEvent),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SetLexems(string@+ lexems)",asFUNCTION(BIND_CLASS Crit_SetLexems),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","Map@+ GetMap() const",asFUNCTION(BIND_CLASS Crit_GetMap),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetMapId() const",asFUNCTION(BIND_CLASS Crit_GetMapId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint16 GetMapProtoId() const",asFUNCTION(BIND_CLASS Crit_GetMapProtoId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SetHomePos(uint16 hexX, uint16 hexY, uint8 dir)",asFUNCTION(BIND_CLASS Crit_SetHomePos),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void GetHomePos(uint& mapId, uint16& hexX, uint16& hexY, uint8& dir)",asFUNCTION(BIND_CLASS Crit_GetHomePos),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool ChangeCrType(uint newType)",asFUNCTION(BIND_CLASS Crit_ChangeCrType),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void DropTimers()",asFUNCTION(BIND_CLASS Cl_DropTimers),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","bool MoveRandom()",asFUNCTION(BIND_CLASS Crit_MoveRandom),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool MoveToDir(uint8 dir)",asFUNCTION(BIND_CLASS Crit_MoveToDir),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool TransitToHex(uint16 hexX, uint16 hexY, uint8 dir)",asFUNCTION(BIND_CLASS Crit_TransitToHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool TransitToMap(uint mapId, uint16 hexX, uint16 hexY, uint8 dir)",asFUNCTION(BIND_CLASS Crit_TransitToMapHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool TransitToMap(uint mapId, uint8 entireNum)",asFUNCTION(BIND_CLASS Crit_TransitToMapEntire),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool TransitToGlobal(bool requestGroup)",asFUNCTION(BIND_CLASS Crit_TransitToGlobal),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool TransitToGlobal(Critter@[]& group)",asFUNCTION(BIND_CLASS Crit_TransitToGlobalWithGroup),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool TransitToGlobalGroup(uint critterId)",asFUNCTION(BIND_CLASS Crit_TransitToGlobalGroup),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","void AddScore(int score, int val)",asFUNCTION(BIND_CLASS Crit_AddScore),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	//if(engine->RegisterObjectMethod("Critter","void ClearScore(int score)",asFUNCTION(BIND_CLASS Cl_AddScore),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","void AddHolodiskInfo(uint holodiskNum)",asFUNCTION(BIND_CLASS Crit_AddHolodiskInfo),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EraseHolodiskInfo(uint holodiskNum)",asFUNCTION(BIND_CLASS Crit_EraseHolodiskInfo),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsHolodiskInfo(uint holodiskNum) const",asFUNCTION(BIND_CLASS Crit_IsHolodiskInfo),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","bool IsLife() const",asFUNCTION(BIND_CLASS Crit_IsLife),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsKnockout() const",asFUNCTION(BIND_CLASS Crit_IsKnockout),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsDead() const",asFUNCTION(BIND_CLASS Crit_IsDead),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsFree() const",asFUNCTION(BIND_CLASS Crit_IsFree),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsBusy() const",asFUNCTION(BIND_CLASS Crit_IsBusy),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void Wait(uint ms)",asFUNCTION(BIND_CLASS Crit_Wait),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void ToDead(uint8 deadType, Critter@+ killer)",asFUNCTION(BIND_CLASS Crit_ToDead),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool ToLife()",asFUNCTION(BIND_CLASS Crit_ToLife),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool ToKnockout(bool faceUp, uint lostAp, uint16 knockHx, uint16 knockHy)",asFUNCTION(BIND_CLASS Crit_ToKnockout),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Critter","void GetProtoData(int[]& data)",asFUNCTION(BIND_CLASS Npc_GetProtoData),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void RefreshVisible()",asFUNCTION(BIND_CLASS Crit_RefreshVisible),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void ViewMap(Map& map, uint look, uint16 hx, uint16 hy, uint8 dir)",asFUNCTION(BIND_CLASS Crit_ViewMap),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Critter","void Mute(uint ms)",asFUNCTION(BIND_CLASS Crit_Mute),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Critter","void Ban(uint ms)",asFUNCTION(BIND_CLASS Crit_Mute),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","Item@+ AddItem(uint16 protoId, uint count)",asFUNCTION(BIND_CLASS Crit_AddItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool DeleteItem(uint16 protoId, uint count)",asFUNCTION(BIND_CLASS Crit_DeleteItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint ItemsCount() const",asFUNCTION(BIND_CLASS Crit_ItemsCount),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint ItemsWeight() const",asFUNCTION(BIND_CLASS Crit_ItemsWeight),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint ItemsVolume() const",asFUNCTION(BIND_CLASS Crit_ItemsVolume),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint CountItem(uint16 protoId) const",asFUNCTION(BIND_CLASS Crit_CountItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","Item@+ GetItem(uint16 protoId, int slot) const",asFUNCTION(BIND_CLASS Crit_GetItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","Item@+ GetItemById(uint itemId) const",asFUNCTION(BIND_CLASS Crit_GetItemById),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetItems(int slot, Item@[]@+ items) const",asFUNCTION(BIND_CLASS Crit_GetItems),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetItemsByType(int type, Item@[]@+ items) const",asFUNCTION(BIND_CLASS Crit_GetItemsByType),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","ProtoItem@+ GetSlotProto(int slot) const",asFUNCTION(BIND_CLASS Crit_GetSlotProto),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool MoveItem(uint itemId, uint count, uint8 toSlot)",asFUNCTION(BIND_CLASS Crit_MoveItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool PickItem(uint16 hexX, uint16 hexY, uint16 protoId)",asFUNCTION(BIND_CLASS Crit_PickItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SetFavoriteItem(int slot, uint16 pid)",asFUNCTION(BIND_CLASS Crit_SetFavoriteItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint16 GetFavoriteItem(int slot)",asFUNCTION(BIND_CLASS Crit_GetFavoriteItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","uint GetCritters(bool lookOnMe, int findType, Critter@[]@+ critters) const",asFUNCTION(BIND_CLASS Crit_GetCritters),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetFollowGroup(int findType, Critter@[]@+ critters) const",asFUNCTION(BIND_CLASS Crit_GetFollowGroup),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","Critter@+ GetFollowLeader() const",asFUNCTION(BIND_CLASS Crit_GetFollowLeader),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","Critter@[]@ GetGlobalGroup() const",asFUNCTION(BIND_CLASS Crit_GetGlobalGroup),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsGlobalGroupLeader()",asFUNCTION(BIND_CLASS Crit_IsGlobalGroupLeader),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void LeaveGlobalGroup()",asFUNCTION(BIND_CLASS Crit_LeaveGlobalGroup),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void GiveGlobalGroupLead(Critter& toCr)",asFUNCTION(BIND_CLASS Crit_GiveGlobalGroupLead),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetTalkedPlayers(Critter@[]@+ players) const",asFUNCTION(BIND_CLASS Npc_GetTalkedPlayers),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsSee(Critter& cr) const",asFUNCTION(BIND_CLASS Crit_IsSeeCr),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsSeenBy(Critter& cr) const",asFUNCTION(BIND_CLASS Crit_IsSeenByCr),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsSee(Item& item) const",asFUNCTION(BIND_CLASS Crit_IsSeeItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","void Say(uint8 howSay, string& text)",asFUNCTION(BIND_CLASS Crit_Say),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SayMsg(uint8 howSay, uint16 textMsg, uint strNum)",asFUNCTION(BIND_CLASS Crit_SayMsg),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SayMsg(uint8 howSay, uint16 textMsg, uint strNum, string& lexems)",asFUNCTION(BIND_CLASS Crit_SayMsgLex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SetDir(uint8 dir)",asFUNCTION(BIND_CLASS Crit_SetDir),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","uint ErasePlane(int planeType, bool all)",asFUNCTION(BIND_CLASS Npc_ErasePlane),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool ErasePlane(uint index)",asFUNCTION(BIND_CLASS Npc_ErasePlaneIndex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void DropPlanes()",asFUNCTION(BIND_CLASS Npc_DropPlanes),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsNoPlanes() const",asFUNCTION(BIND_CLASS Npc_IsNoPlanes),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsCurPlane(int planeType) const",asFUNCTION(BIND_CLASS Npc_IsCurPlane),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","NpcPlane@+ GetCurPlane() const",asFUNCTION(BIND_CLASS Npc_GetCurPlane),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetPlanes(NpcPlane@[]@+ planes) const",asFUNCTION(BIND_CLASS Npc_GetPlanes),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetPlanes(int identifier, NpcPlane@[]@+ planes) const",asFUNCTION(BIND_CLASS Npc_GetPlanesIdentifier),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetPlanes(int identifier, uint identifierExt, NpcPlane@[]@+ planes) const",asFUNCTION(BIND_CLASS Npc_GetPlanesIdentifier2),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool AddPlane(NpcPlane& plane)",asFUNCTION(BIND_CLASS Npc_AddPlane),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","void SendMessage(int num, int val, int to)",asFUNCTION(BIND_CLASS Crit_SendMessage),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void Action(int action, int actionExt, Item@+ item)",asFUNCTION(BIND_CLASS Crit_Action),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void Animate(uint anim1, uint anim2, Item@+ item, bool clearSequence, bool delayPlay)",asFUNCTION(BIND_CLASS Crit_Animate),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void PlaySound(string& soundName, bool sendSelf)",asFUNCTION(BIND_CLASS Crit_PlaySound),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void PlaySound(uint8 soundType, uint8 soundTypeExt, uint8 soundId, uint8 soundIdExt, bool sendSelf)",asFUNCTION(BIND_CLASS Crit_PlaySoundType),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","void SendQuestVar(GameVar& var)",asFUNCTION(BIND_CLASS Cl_SendQuestVar),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SendCombatResult(uint[]& combatResult)",asFUNCTION(BIND_CLASS Crit_SendCombatResult),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool IsKnownLoc(bool byId, uint locNum) const",asFUNCTION(BIND_CLASS Cl_IsKnownLoc),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool SetKnownLoc(bool byId, uint locNum)",asFUNCTION(BIND_CLASS Cl_SetKnownLoc),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool UnsetKnownLoc(bool byId, uint locNum)",asFUNCTION(BIND_CLASS Cl_UnsetKnownLoc),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SetFog(uint16 zoneX, uint16 zoneY, int fog)",asFUNCTION(BIND_CLASS Cl_SetFog),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int GetFog(uint16 zoneX, uint16 zoneY)",asFUNCTION(BIND_CLASS Cl_GetFog),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","void ShowContainer(Critter@+ contCr, Item@+ contItem, uint8 transferType)",asFUNCTION(BIND_CLASS Cl_ShowContainer),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void ShowScreen(int screenType, uint param, string& funcName)",asFUNCTION(BIND_CLASS Cl_ShowScreen),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void RunClientScript(string& funcName, int p0, int p1, int p2, string@+ p3, int[]@+ p4)",asFUNCTION(BIND_CLASS Cl_RunClientScript),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void Disconnect()",asFUNCTION(BIND_CLASS Cl_Disconnect),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","bool SetScript(string@+ script)",asFUNCTION(BIND_CLASS Crit_SetScript),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetScriptId() const",asFUNCTION(BIND_CLASS Crit_GetScriptId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SetBagRefreshTime(uint realMinutes)",asFUNCTION(BIND_CLASS Crit_SetBagRefreshTime),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetBagRefreshTime() const",asFUNCTION(BIND_CLASS Crit_GetBagRefreshTime),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SetInternalBag(uint16[]& pids, uint[]@+ minCounts, uint[]@+ maxCounts, int[]@+ slots)",asFUNCTION(BIND_CLASS Crit_SetInternalBag),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetInternalBag(uint16[]@+ pids, uint[]@+ minCounts, uint[]@+ maxCounts, int[]@+ slots) const",asFUNCTION(BIND_CLASS Crit_GetInternalBag),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint16 GetProtoId() const",asFUNCTION(BIND_CLASS Crit_GetProtoId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","void AddEnemyInStack(uint critterId)",asFUNCTION(BIND_CLASS Crit_AddEnemyInStack),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool CheckEnemyInStack(uint critterId) const",asFUNCTION(BIND_CLASS Crit_CheckEnemyInStack),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EraseEnemyFromStack(uint critterId)",asFUNCTION(BIND_CLASS Crit_EraseEnemyFromStack),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void ChangeEnemyStackSize(uint newSize)",asFUNCTION(BIND_CLASS Crit_ChangeEnemyStackSize),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void GetEnemyStack(uint[]& enemyStack) const",asFUNCTION(BIND_CLASS Crit_GetEnemyStack),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void ClearEnemyStack()",asFUNCTION(BIND_CLASS Crit_ClearEnemyStack),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void ClearEnemyStackNpc()",asFUNCTION(BIND_CLASS Crit_ClearEnemyStackNpc),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","bool AddTimeEvent(string& funcName, uint duration, int identifier)",asFUNCTION(BIND_CLASS Crit_AddTimeEvent),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool AddTimeEvent(string& funcName, uint duration, int identifier, uint rate)",asFUNCTION(BIND_CLASS Crit_AddTimeEventRate),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetTimeEvents(int identifier, uint[]@+ indexes, uint[]@+ durations, uint[]@+ rates) const",asFUNCTION(BIND_CLASS Crit_GetTimeEvents),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint GetTimeEvents(int[]& findIdentifiers, int[]@+ identifiers, uint[]@+ indexes, uint[]@+ durations, uint[]@+ rates) const",asFUNCTION(BIND_CLASS Crit_GetTimeEventsArr),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void ChangeTimeEvent(uint index, uint newDuration, uint newRate)",asFUNCTION(BIND_CLASS Crit_ChangeTimeEvent),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EraseTimeEvent(uint index)",asFUNCTION(BIND_CLASS Crit_EraseTimeEvent),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint EraseTimeEvents(int identifier)",asFUNCTION(BIND_CLASS Crit_EraseTimeEvents),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint EraseTimeEvents(int[]& identifiers)",asFUNCTION(BIND_CLASS Crit_EraseTimeEventsArr),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","void EventIdle()",asFUNCTION(BIND_CLASS Crit_EventIdle),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventFinish(bool deleted)",asFUNCTION(BIND_CLASS Crit_EventFinish),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventDead(Critter@+ killer)",asFUNCTION(BIND_CLASS Crit_EventDead),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventRespawn()",asFUNCTION(BIND_CLASS Crit_EventRespawn),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventShowCritter(Critter& cr)",asFUNCTION(BIND_CLASS Crit_EventShowCritter),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventShowCritter1(Critter& cr)",asFUNCTION(BIND_CLASS Crit_EventShowCritter1),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventShowCritter2(Critter& cr)",asFUNCTION(BIND_CLASS Crit_EventShowCritter2),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventShowCritter3(Critter& cr)",asFUNCTION(BIND_CLASS Crit_EventShowCritter3),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventHideCritter(Critter& cr)",asFUNCTION(BIND_CLASS Crit_EventHideCritter),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventHideCritter1(Critter& cr)",asFUNCTION(BIND_CLASS Crit_EventHideCritter1),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventHideCritter2(Critter& cr)",asFUNCTION(BIND_CLASS Crit_EventHideCritter2),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventHideCritter3(Critter& cr)",asFUNCTION(BIND_CLASS Crit_EventHideCritter3),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventShowItemOnMap(Item& showItem, bool added, Critter@+ dropper)",asFUNCTION(BIND_CLASS Crit_EventShowItemOnMap),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventChangeItemOnMap(Item& item)",asFUNCTION(BIND_CLASS Crit_EventChangeItemOnMap),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventHideItemOnMap(Item& hideItem, bool removed, Critter@+ picker)",asFUNCTION(BIND_CLASS Crit_EventHideItemOnMap),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool EventAttack(Critter& target)",asFUNCTION(BIND_CLASS Crit_EventAttack),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool EventAttacked(Critter@+ attacker)",asFUNCTION(BIND_CLASS Crit_EventAttacked),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool EventStealing(Critter& thief, Item& item, uint count)",asFUNCTION(BIND_CLASS Crit_EventStealing),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventMessage(Critter& fromCr, int message, int value)",asFUNCTION(BIND_CLASS Crit_EventMessage),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool EventUseItem(Item& item, Critter@+ onCritter, Item@+ onItem, Scenery@+ onScenery)",asFUNCTION(BIND_CLASS Crit_EventUseItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool EventUseSkill(int skill, Critter@+ onCritter, Item@+ onItem, Scenery@+ onScenery)",asFUNCTION(BIND_CLASS Crit_EventUseSkill),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventDropItem(Item& item)",asFUNCTION(BIND_CLASS Crit_EventDropItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventMoveItem(Item& item, uint8 fromSlot)",asFUNCTION(BIND_CLASS Crit_EventMoveItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventKnockout(uint breaktime, bool faceUp, uint knockDist)",asFUNCTION(BIND_CLASS Crit_EventKnockout),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventSmthDead(Critter& fromCr, Critter@+ killer)",asFUNCTION(BIND_CLASS Crit_EventSmthDead),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventSmthStealing(Critter& fromCr, Critter& thief, bool success, Item& item, uint count)",asFUNCTION(BIND_CLASS Crit_EventSmthStealing),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventSmthAttack(Critter& fromCr, Critter& target)",asFUNCTION(BIND_CLASS Crit_EventSmthAttack),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventSmthAttacked(Critter& fromCr, Critter@+ attacker)",asFUNCTION(BIND_CLASS Crit_EventSmthAttacked),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventSmthUseItem(Critter& fromCr, Item& item, Critter@+ onCritter, Item@+ onItem, Scenery@+ onScenery)",asFUNCTION(BIND_CLASS Crit_EventSmthUseItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventSmthUseSkill(Critter& fromCr, int skill, Critter@+ onCritter, Item@+ onItem, Scenery@+ onScenery)",asFUNCTION(BIND_CLASS Crit_EventSmthUseSkill),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventSmthDropItem(Critter& fromCr, Item& item)",asFUNCTION(BIND_CLASS Crit_EventSmthDropItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventSmthMoveItem(Critter& fromCr, Item& item, uint8 fromSlot)",asFUNCTION(BIND_CLASS Crit_EventSmthMoveItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventSmthKnockout(Critter& fromCr, uint breaktime, bool faceUp, uint knockDist)",asFUNCTION(BIND_CLASS Crit_EventSmthKnockout),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int EventPlaneBegin(NpcPlane& plane, int reason, Critter@+ someCr, Item@+ someItem)",asFUNCTION(BIND_CLASS Crit_EventPlaneBegin),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int EventPlaneEnd(NpcPlane& plane, int reason, Critter@+ someCr, Item@+ someItem)",asFUNCTION(BIND_CLASS Crit_EventPlaneEnd),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int EventPlaneRun(NpcPlane& plane, int reason, uint& p0, uint& p1, uint& p2)",asFUNCTION(BIND_CLASS Crit_EventPlaneRun),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool EventBarter(Critter& barterCr, bool attach, uint barterCount)",asFUNCTION(BIND_CLASS Crit_EventBarter),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool EventTalk(Critter& talkCr, bool attach, uint talkCount)",asFUNCTION(BIND_CLASS Crit_EventTalk),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool EventGlobalProcess(int type, Critter@[]& group, Item@ car, uint& x, uint& y, uint& toX, uint& toY, uint& speed, uint& encounterDescriptor, bool& waitForAnswer)",asFUNCTION(BIND_CLASS Crit_EventGlobalProcess),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","bool EventGlobalInvite(Critter@[]& group, Item@ car, uint encounterDescriptor, int combatMode, uint& mapId, uint16& hexX, uint16& hexY, uint8& dir)",asFUNCTION(BIND_CLASS Crit_EventGlobalInvite),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventTurnBasedProcess(Map& map, bool beginTurn)",asFUNCTION(BIND_CLASS Crit_EventTurnBasedProcess),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void EventSmthTurnBasedProcess(Critter& fromCr, Map& map, bool beginTurn)",asFUNCTION(BIND_CLASS Crit_EventSmthTurnBasedProcess),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	// Parameters
	if(engine->RegisterObjectProperty("Critter","const uint Id",offsetof(Critter,Data)+offsetof(CritData,Id))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint CrType",offsetof(Critter,Data)+offsetof(CritData,BaseType))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint16 HexX",offsetof(Critter,Data)+offsetof(CritData,HexX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint16 HexY",offsetof(Critter,Data)+offsetof(CritData,HexY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint16 WorldX",offsetof(Critter,Data)+offsetof(CritData,WorldX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint16 WorldY",offsetof(Critter,Data)+offsetof(CritData,WorldY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint8 Dir",offsetof(Critter,Data)+offsetof(CritData,Dir))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint8 Cond",offsetof(Critter,Data)+offsetof(CritData,Cond))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint8 CondExt",offsetof(Critter,Data)+offsetof(CritData,CondExt))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint16 FlagsExt",offsetof(Critter,Flags)+2)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const string Name",offsetof(Critter,NameStr))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","uint ShowCritterDist1",offsetof(Critter,Data)+offsetof(CritData,ShowCritterDist1))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","uint ShowCritterDist2",offsetof(Critter,Data)+offsetof(CritData,ShowCritterDist2))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","uint ShowCritterDist3",offsetof(Critter,Data)+offsetof(CritData,ShowCritterDist3))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","bool IsRuning",offsetof(Critter,IsRuning))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const bool IsNotValid",offsetof(Critter,IsNotValid))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const int Ref",offsetof(Critter,RefCounter))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","DataVal Param",offsetof(Critter,ThisPtr[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","DataRef ParamBase",offsetof(Critter,ThisPtr[0]))<0) BIND_ERROR;

/************************************************************************/
/* Map                                                                  */
/************************************************************************/
	if(engine->RegisterObjectBehaviour("Map",asBEHAVE_ADDREF,"void f()",asMETHOD(Map,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("Map",asBEHAVE_RELEASE,"void f()",asMETHOD(Map,Release),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Map","const bool IsNotValid",offsetof(Map,IsNotValid))<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint16 GetProtoId() const",asFUNCTION(BIND_CLASS Map_GetProtoId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","Location@+ GetLocation() const",asFUNCTION(BIND_CLASS Map_GetLocation),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","bool SetScript(string@+ script)",asFUNCTION(BIND_CLASS Map_SetScript),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetScriptId() const",asFUNCTION(BIND_CLASS Map_GetScriptId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","bool SetEvent(int eventType, string@+ funcName)",asFUNCTION(BIND_CLASS Map_SetEvent),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void SetLoopTime(uint numLoop, uint ms)",asFUNCTION(BIND_CLASS Map_SetLoopTime),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint8 GetRain() const",asFUNCTION(BIND_CLASS Map_GetRain),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void SetRain(uint8 capacity)",asFUNCTION(BIND_CLASS Map_SetRain),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","int GetTime() const",asFUNCTION(BIND_CLASS Map_GetTime),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void SetTime(int time)",asFUNCTION(BIND_CLASS Map_SetTime),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetDayTime(uint dayPart) const",asFUNCTION(BIND_CLASS Map_GetDayTime),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void SetDayTime(uint dayPart, uint time)",asFUNCTION(BIND_CLASS Map_SetDayTime),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void GetDayColor(uint dayPart, uint8& r, uint8& g, uint8& b) const",asFUNCTION(BIND_CLASS Map_GetDayColor),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void SetDayColor(uint dayPart, uint8 r, uint8 g, uint8 b)",asFUNCTION(BIND_CLASS Map_SetDayColor),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void SetTurnBasedAvailability(bool value)",asFUNCTION(BIND_CLASS Map_SetTurnBasedAvailability),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","bool IsTurnBasedAvailability() const",asFUNCTION(BIND_CLASS Map_IsTurnBasedAvailability),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void BeginTurnBased(Critter@+ firstTurnCrit)",asFUNCTION(BIND_CLASS Map_BeginTurnBased),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","bool IsTurnBased() const",asFUNCTION(BIND_CLASS Map_IsTurnBased),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EndTurnBased()",asFUNCTION(BIND_CLASS Map_EndTurnBased),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","int GetTurnBasedSequence(uint[]& crittersIds) const",asFUNCTION(BIND_CLASS Map_GetTurnBasedSequence),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Map","void SetTurnBasedSequence(uint[]& crittersIds)",asFUNCTION(BIND_CLASS Map_SetTurnBasedSequence),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void SetData(uint index, int value)",asFUNCTION(BIND_CLASS Map_SetData),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","int GetData(uint index) const",asFUNCTION(BIND_CLASS Map_GetData),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","Item@+ AddItem(uint16 hexX, uint16 hexY, uint16 protoId, uint count)",asFUNCTION(BIND_CLASS Map_AddItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
//	if(engine->RegisterObjectMethod("Map","Item@+ AddItemFromScenery(uint16 hexX, uint16 hexY, uint16 protoId)",asFUNCTION(BIND_CLASS Map_AddItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","Item@+ GetItem(uint itemId) const",asFUNCTION(BIND_CLASS Map_GetItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","Item@+ GetItem(uint16 hexX, uint16 hexY, uint16 protoId) const",asFUNCTION(BIND_CLASS Map_GetItemHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetItems(uint16 hexX, uint16 hexY, Item@[]@+ items) const",asFUNCTION(BIND_CLASS Map_GetItemsHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetItems(uint16 hexX, uint16 hexY, uint radius, uint16 protoId, Item@[]@+ items) const",asFUNCTION(BIND_CLASS Map_GetItemsHexEx),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetItems(uint16 protoId, Item@[]@+ items) const",asFUNCTION(BIND_CLASS Map_GetItemsByPid),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetItemsByType(int type, Item@[]@+ items) const",asFUNCTION(BIND_CLASS Map_GetItemsByType),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","Item@+ GetDoor(uint16 hexX, uint16 hexY) const",asFUNCTION(BIND_CLASS Map_GetDoor),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","Item@+ GetCar(uint16 hexX, uint16 hexY) const",asFUNCTION(BIND_CLASS Map_GetCar),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","Scenery@+ GetScenery(uint16 hexX, uint16 hexY, uint16 protoId) const",asFUNCTION(BIND_CLASS Map_GetSceneryHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetSceneries(uint16 hexX, uint16 hexY, Scenery@[]@+ sceneries) const",asFUNCTION(BIND_CLASS Map_GetSceneriesHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetSceneries(uint16 hexX, uint16 hexY, uint radius, uint16 protoId, Scenery@[]@+ sceneries) const",asFUNCTION(BIND_CLASS Map_GetSceneriesHexEx),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetSceneries(uint16 protoId, Scenery@[]@+ sceneries) const",asFUNCTION(BIND_CLASS Map_GetSceneriesByPid),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","Critter@+ GetCritter(uint16 hexX, uint16 hexY) const",asFUNCTION(BIND_CLASS Map_GetCritterHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","Critter@+ GetCritter(uint critterId) const",asFUNCTION(BIND_CLASS Map_GetCritterById),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetCrittersHex(uint16 hexX, uint16 hexY, uint radius, int findType, Critter@[]@+ critters) const",asFUNCTION(BIND_CLASS Map_GetCritters),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetCritters(uint16 pid, int findType, Critter@[]@+ critters) const",asFUNCTION(BIND_CLASS Map_GetCrittersByPids),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, Critter@[]@+ critters) const",asFUNCTION(BIND_CLASS Map_GetCrittersInPath),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, Critter@[]@+ critters, uint16& preBlockHx, uint16& preBlockHy, uint16& blockHx, uint16& blockHy) const",asFUNCTION(BIND_CLASS Map_GetCrittersInPathBlock),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetCrittersWhoViewPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, int findType, Critter@[]@+ critters) const",asFUNCTION(BIND_CLASS Map_GetCrittersWhoViewPath),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetCrittersSeeing(Critter@[]& critters, bool lookOnThem, int find_type, Critter@[]@+ critters_result) const",asFUNCTION(BIND_CLASS Map_GetCrittersSeeing),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist) const",asFUNCTION(BIND_CLASS Map_GetHexInPath),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void GetHexCoordWall(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist) const",asFUNCTION(BIND_CLASS Map_GetHexInPathWall),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut) const",asFUNCTION(BIND_CLASS Map_GetPathLength),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","bool VerifyTrigger(Critter& cr, uint16 hexX, uint16 hexY, uint8 dir)",asFUNCTION(BIND_CLASS Map_VerifyTrigger),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","Critter@+ AddNpc(uint16 protoId, uint16 hexX, uint16 hexY, uint8 dir, int[]@+ params, int[]@+ items, string@+ script)",asFUNCTION(BIND_CLASS Map_AddNpc),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetNpcCount(int npcRole, int findType) const",asFUNCTION(BIND_CLASS Map_GetNpcCount),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","Critter@+ GetNpc(int npcRole, int findType, uint skipCount) const",asFUNCTION(BIND_CLASS Map_GetNpc),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint CountEntire(int entire) const",asFUNCTION(BIND_CLASS Map_CountEntire),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint GetEntires(int entire, uint[]@+ entires, uint16[]@+ hexX, uint16[]@+ hexY) const",asFUNCTION(BIND_CLASS Map_GetEntires),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","bool GetEntireCoords(int entire, uint skip, uint16& hexX, uint16& hexY) const",asFUNCTION(BIND_CLASS Map_GetEntireCoords),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","bool GetNearEntireCoords(int& entire, uint16& hexX, uint16& hexY) const",asFUNCTION(BIND_CLASS Map_GetNearEntireCoords),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","bool IsHexPassed(uint16 hexX, uint16 hexY) const",asFUNCTION(BIND_CLASS Map_IsHexPassed),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","bool IsHexRaked(uint16 hexX, uint16 hexY) const",asFUNCTION(BIND_CLASS Map_IsHexRaked),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void SetText(uint16 hexX, uint16 hexY, uint color, string& text)",asFUNCTION(BIND_CLASS Map_SetText),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void SetTextMsg(uint16 hexX, uint16 hexY, uint color, uint16 textMsg, uint strNum)",asFUNCTION(BIND_CLASS Map_SetTextMsg),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void SetTextMsg(uint16 hexX, uint16 hexY, uint color, uint16 textMsg, uint strNum, string& lexems)",asFUNCTION(BIND_CLASS Map_SetTextMsgLex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void RunEffect(uint16 effectPid, uint16 hexX, uint16 hexY, uint16 radius)",asFUNCTION(BIND_CLASS Map_RunEffect),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void RunFlyEffect(uint16 effectPid, Critter@+ fromCr, Critter@+ toCr, uint16 fromX, uint16 fromY, uint16 toX, uint16 toY)",asFUNCTION(BIND_CLASS Map_RunFlyEffect),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","bool CheckPlaceForCar(uint16 hexX, uint16 hexY, uint16 protoCar) const",asFUNCTION(BIND_CLASS Map_CheckPlaceForCar),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void BlockHex(uint16 hexX, uint16 hexY, bool full)",asFUNCTION(BIND_CLASS Map_BlockHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void UnblockHex(uint16 hexX, uint16 hexY)",asFUNCTION(BIND_CLASS Map_UnblockHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void PlaySound(string& soundName)",asFUNCTION(BIND_CLASS Map_PlaySound),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void PlaySound(string& soundName, uint16 hexX, uint16 hexY, uint radius)",asFUNCTION(BIND_CLASS Map_PlaySoundRadius),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","bool Reload()",asFUNCTION(BIND_CLASS Map_Reload),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint16 GetWidth()",asFUNCTION(BIND_CLASS Map_GetWidth),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","uint16 GetHeight()",asFUNCTION(BIND_CLASS Map_GetHeight),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps)",asFUNCTION(BIND_CLASS Map_MoveHexByDir),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Map","void EventFinish(bool deleted)",asFUNCTION(BIND_CLASS Map_EventFinish),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EventLoop0()",asFUNCTION(BIND_CLASS Map_EventLoop0),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EventLoop1()",asFUNCTION(BIND_CLASS Map_EventLoop1),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EventLoop2()",asFUNCTION(BIND_CLASS Map_EventLoop2),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EventLoop3()",asFUNCTION(BIND_CLASS Map_EventLoop3),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EventLoop4()",asFUNCTION(BIND_CLASS Map_EventLoop4),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EventInCritter(Critter& cr)",asFUNCTION(BIND_CLASS Map_EventInCritter),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EventOutCritter(Critter& cr)",asFUNCTION(BIND_CLASS Map_EventOutCritter),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EventCritterDead(Critter& cr, Critter@+ killer)",asFUNCTION(BIND_CLASS Map_EventCritterDead),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EventTurnBasedBegin()",asFUNCTION(BIND_CLASS Map_EventTurnBasedBegin),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EventTurnBasedEnd()",asFUNCTION(BIND_CLASS Map_EventTurnBasedEnd),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Map","void EventTurnBasedProcess(Critter& cr, bool beginTurn)",asFUNCTION(BIND_CLASS Map_EventTurnBasedProcess),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("Map","const uint Id",offsetof(Map,Data.MapId))<0) BIND_ERROR;
	//if(engine->RegisterObjectProperty("Map","bool CombatRunning",offsetof(Map,MapId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Map","const uint TurnBasedRound",offsetof(Map,TurnBasedRound))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Map","const uint TurnBasedTurn",offsetof(Map,TurnBasedTurn))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Map","const uint TurnBasedWholeTurn",offsetof(Map,TurnBasedWholeTurn))<0) BIND_ERROR;

/************************************************************************/
/* Location                                                             */
/************************************************************************/
	if(engine->RegisterObjectBehaviour("Location",asBEHAVE_ADDREF,"void f()",asMETHOD(Location,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("Location",asBEHAVE_RELEASE,"void f()",asMETHOD(Location,Release),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Location","uint16 GetProtoId() const",asFUNCTION(BIND_CLASS Location_GetProtoId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Location","uint GetMapCount() const",asFUNCTION(BIND_CLASS Location_GetMapCount),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Location","Map@+ GetMap(uint16 mapPid) const",asFUNCTION(BIND_CLASS Location_GetMap),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Location","Map@+ GetMapByIndex(uint index) const",asFUNCTION(BIND_CLASS Location_GetMapByIndex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Location","uint GetMaps(Map@[]& maps) const",asFUNCTION(BIND_CLASS Location_GetMaps),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Location","bool Reload()",asFUNCTION(BIND_CLASS Location_Reload),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Location","void Update()",asFUNCTION(BIND_CLASS Location_Update),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("Location","const uint Id",offsetof(Location,Data.LocId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Location","uint16 WorldX",offsetof(Location,Data.WX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Location","uint16 WorldY",offsetof(Location,Data.WY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Location","bool Visible",offsetof(Location,Data.Visible))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Location","bool GeckEnabled",offsetof(Location,Data.GeckEnabled))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Location","bool AutoGarbage",offsetof(Location,Data.AutoGarbage))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Location","int GeckCount",offsetof(Location,GeckCount))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Location","uint16 Radius",offsetof(Location,Data.Radius))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Location","uint Color",offsetof(Location,Data.Color))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Location","const bool IsNotValid",offsetof(Location,IsNotValid))<0) BIND_ERROR;

/************************************************************************/
/* Global                                                               */
/************************************************************************/
	if(engine->RegisterGlobalFunction("GameVar@+ GetGlobalVar(uint16 varId) const",asFUNCTION(BIND_CLASS Global_GetGlobalVar),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("GameVar@+ GetLocalVar(uint16 varId, uint masterId) const",asFUNCTION(BIND_CLASS Global_GetLocalVar),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("GameVar@+ GetUnicumVar(uint16 varId, uint masterId, uint slaveId) const",asFUNCTION(BIND_CLASS Global_GetUnicumVar),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint DeleteVars(uint id)",asFUNCTION(BIND_CLASS Global_DeleteVars),asCALL_CDECL)<0) BIND_ERROR;	if(engine->RegisterGlobalFunction("Item@+ GetItem(uint itemId)",asFUNCTION(BIND_CLASS Global_GetItem),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void MoveItem(Item& item, uint count, Critter& toCr)",asFUNCTION(BIND_CLASS Global_MoveItemCr),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void MoveItem(Item& item, uint count, Item& toCont, uint stackId)",asFUNCTION(BIND_CLASS Global_MoveItemCont),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void MoveItem(Item& item, uint count, Map& toMap, uint16 toHx, uint16 toHy)",asFUNCTION(BIND_CLASS Global_MoveItemMap),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void MoveItems(Item@[]& items, Critter& toCr)",asFUNCTION(BIND_CLASS Global_MoveItemsCr),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void MoveItems(Item@[]& items, Item& toCont, uint stackId)",asFUNCTION(BIND_CLASS Global_MoveItemsCont),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void MoveItems(Item@[]& items, Map& toMap, uint16 toHx, uint16 toHy)",asFUNCTION(BIND_CLASS Global_MoveItemsMap),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DeleteItem(Item& item)",asFUNCTION(BIND_CLASS Global_DeleteItem),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DeleteItems(Item@[]& items)",asFUNCTION(BIND_CLASS Global_DeleteItems),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DeleteNpc(Critter& npc)",asFUNCTION(BIND_CLASS Global_DeleteNpc),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DeleteNpcForce(Critter& npc)",asFUNCTION(BIND_CLASS Global_DeleteNpcForce),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetCrittersDistantion(Critter& cr1, Critter& cr2) const",asFUNCTION(BIND_CLASS Global_GetCrittersDistantion),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void RadioMessage(uint16 channel, string& text)",asFUNCTION(BIND_CLASS Global_RadioMessage),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void RadioMessageMsg(uint16 channel, uint16 textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_RadioMessageMsg),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint CreateLocation(uint16 locPid, uint16 worldX, uint16 worldY, Critter@[]@+ critters)",asFUNCTION(BIND_CLASS Global_CreateLocation),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DeleteLocation(uint locId)",asFUNCTION(BIND_CLASS Global_DeleteLocation),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void GetProtoCritter(uint16 protoId, int[]& data) const",asFUNCTION(BIND_CLASS Global_GetProtoCritter),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("Critter@+ GetCritter(uint critterId) const",asFUNCTION(BIND_CLASS Global_GetCritter),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("Critter@+ GetPlayer(string& name) const",asFUNCTION(BIND_CLASS Global_GetPlayer),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetPlayerId(string& name) const",asFUNCTION(BIND_CLASS Global_GetPlayerId),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ GetPlayerName(uint playerId) const",asFUNCTION(BIND_CLASS Global_GetPlayerName),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint CreateTimeEvent(uint beginMinute, string& funcName, bool save)",asFUNCTION(BIND_CLASS Global_CreateTimeEventEmpty),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint CreateTimeEvent(uint beginMinute, string& funcName, int value, bool save)",asFUNCTION(BIND_CLASS Global_CreateTimeEventDw),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint CreateTimeEvent(uint beginMinute, string& funcName, uint value, bool save)",asFUNCTION(BIND_CLASS Global_CreateTimeEventDw),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint CreateTimeEvent(uint beginMinute, string& funcName, uint[]& values, bool save)",asFUNCTION(BIND_CLASS Global_CreateTimeEventDws),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint CreateTimeEvent(uint beginMinute, string& funcName, int[]& values, bool save)",asFUNCTION(BIND_CLASS Global_CreateTimeEventDws),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint CreateTimeEvent(uint beginMinute, string& funcName, Critter& cr, bool save)",asFUNCTION(BIND_CLASS Global_CreateTimeEventCr),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint CreateTimeEvent(uint beginMinute, string& funcName, Item& item, bool save)",asFUNCTION(BIND_CLASS Global_CreateTimeEventItem),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint CreateTimeEvent(uint beginMinute, string& funcName, Critter@[]@+ critters, Item@[]@+ items, bool save)",asFUNCTION(BIND_CLASS Global_CreateTimeEventArr),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void EraseTimeEvent(uint num)",asFUNCTION(BIND_CLASS Global_EraseTimeEvent),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, int64[]& data)",asFUNCTION(BIND_CLASS Global_SetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, int32[]& data)",asFUNCTION(BIND_CLASS Global_SetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, int16[]& data)",asFUNCTION(BIND_CLASS Global_SetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, int8[]& data)",asFUNCTION(BIND_CLASS Global_SetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, uint64[]& data)",asFUNCTION(BIND_CLASS Global_SetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, uint32[]& data)",asFUNCTION(BIND_CLASS Global_SetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, uint16[]& data)",asFUNCTION(BIND_CLASS Global_SetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, uint8[]& data)",asFUNCTION(BIND_CLASS Global_SetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, int64[]& data, uint dataSize)",asFUNCTION(BIND_CLASS Global_SetAnyDataSize),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, int32[]& data, uint dataSize)",asFUNCTION(BIND_CLASS Global_SetAnyDataSize),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, int16[]& data, uint dataSize)",asFUNCTION(BIND_CLASS Global_SetAnyDataSize),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, int8[]& data, uint dataSize)",asFUNCTION(BIND_CLASS Global_SetAnyDataSize),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, uint64[]& data, uint dataSize)",asFUNCTION(BIND_CLASS Global_SetAnyDataSize),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, uint32[]& data, uint dataSize)",asFUNCTION(BIND_CLASS Global_SetAnyDataSize),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, uint16[]& data, uint dataSize)",asFUNCTION(BIND_CLASS Global_SetAnyDataSize),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetAnyData(string& name, uint8[]& data, uint dataSize)",asFUNCTION(BIND_CLASS Global_SetAnyDataSize),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetAnyData(string& name, int64[]& data)",asFUNCTION(BIND_CLASS Global_GetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetAnyData(string& name, int32[]& data)",asFUNCTION(BIND_CLASS Global_GetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetAnyData(string& name, int16[]& data)",asFUNCTION(BIND_CLASS Global_GetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetAnyData(string& name, int8[]& data)",asFUNCTION(BIND_CLASS Global_GetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetAnyData(string& name, uint64[]& data)",asFUNCTION(BIND_CLASS Global_GetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetAnyData(string& name, uint32[]& data)",asFUNCTION(BIND_CLASS Global_GetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetAnyData(string& name, uint16[]& data)",asFUNCTION(BIND_CLASS Global_GetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetAnyData(string& name, uint8[]& data)",asFUNCTION(BIND_CLASS Global_GetAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool IsAnyData(string& name) const",asFUNCTION(BIND_CLASS Global_IsAnyData),asCALL_CDECL)<0) BIND_ERROR;
	//if(engine->RegisterGlobalFunction("bool AnyDataClass(?& storedClass, ?[]& array)",asFUNCTION(BIND_CLASS Global_AnyDataClass),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void EraseAnyData(string& name)",asFUNCTION(BIND_CLASS Global_EraseAnyData),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void ArrayPushBack(Critter@[]& arr, Critter@+ cr)",asFUNCTION(BIND_CLASS Global_ArrayPushBackCritter),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void ArrayPushBack(int64[]& arr, int64& value)",asFUNCTION(BIND_CLASS Global_ArrayPushBackInteger),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void ArrayPushBack(int32[]& arr, int32& value)",asFUNCTION(BIND_CLASS Global_ArrayPushBackInteger),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void ArrayPushBack(int16[]& arr, int16& value)",asFUNCTION(BIND_CLASS Global_ArrayPushBackInteger),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void ArrayPushBack(int8[]& arr, int8& value)",asFUNCTION(BIND_CLASS Global_ArrayPushBackInteger),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void ArrayPushBack(uint64[]& arr, uint64& value)",asFUNCTION(BIND_CLASS Global_ArrayPushBackInteger),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void ArrayPushBack(uint32[]& arr, uint32& value)",asFUNCTION(BIND_CLASS Global_ArrayPushBackInteger),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void ArrayPushBack(uint16[]& arr, uint16& value)",asFUNCTION(BIND_CLASS Global_ArrayPushBackInteger),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void ArrayPushBack(uint8[]& arr, uint8& value)",asFUNCTION(BIND_CLASS Global_ArrayPushBackInteger),asCALL_CDECL)<0) BIND_ERROR;
	//if(engine->RegisterGlobalFunction("void ArrayErase(uint[]& arr, uint index)",asFUNCTION(BIND_CLASS Global_ArrayErase),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("Map@+ GetMap(uint mapId) const",asFUNCTION(BIND_CLASS Global_GetMap),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("Map@+ GetMapByPid(uint16 mapPid, uint skipCount) const",asFUNCTION(BIND_CLASS Global_GetMapByPid),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("Location@+ GetLocation(uint locId) const",asFUNCTION(BIND_CLASS Global_GetLocation),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("Location@+ GetLocationByPid(uint16 locPid, uint skipCount) const",asFUNCTION(BIND_CLASS Global_GetLocationByPid),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetLocations(uint16 worldX, uint16 worldY, uint radius, Location@[]@+ locations) const",asFUNCTION(BIND_CLASS Global_GetLocations),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool RunDialog(Critter& player, Critter& npc, bool ignoreDistance)",asFUNCTION(BIND_CLASS Global_RunDialogNpc),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool RunDialog(Critter& player, Critter& npc, uint dialogPack, bool ignoreDistance)",asFUNCTION(BIND_CLASS Global_RunDialogNpcDlgPack),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool RunDialog(Critter& player, uint dialogPack, uint16 hexX, uint16 hexY, bool ignoreDistance)",asFUNCTION(BIND_CLASS Global_RunDialogHex),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int64 WorldItemCount(uint16 protoId) const",asFUNCTION(BIND_CLASS Global_WorldItemCount),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void SetBestScore(int score, Critter@+ player, string& name)",asFUNCTION(BIND_CLASS Global_SetBestScore),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool AddTextListener(int sayType, string& firstStr, uint16 parameter, string& scriptName)",asFUNCTION(BIND_CLASS Global_AddTextListener),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void EraseTextListener(int sayType, string& firstStr, uint16 parameter)",asFUNCTION(BIND_CLASS Global_EraseTextListener),asCALL_CDECL)<0) BIND_ERROR;
	//if(engine->RegisterGlobalFunction("uint8 ReverseDir(uint8 dir)",asFUNCTION(BIND_CLASS Global_ReverseDir),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("NpcPlane@ CreatePlane()",asFUNCTION(BIND_CLASS Global_CreatePlane),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetBagItems(uint bagId, uint16[]@+ pids, uint[]@+ minCounts, uint[]@+ maxCounts, int[]@+ slots) const",asFUNCTION(BIND_CLASS Global_GetBagItems),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void SetSendParameter(int index, bool enabled)",asFUNCTION(BIND_CLASS Global_SetSendParameter),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void SetSendParameter(int index, bool enabled, int conditionIndex, int conditionMask)",asFUNCTION(BIND_CLASS Global_SetSendParameterCond),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SwapCritters(Critter& cr1, Critter& cr2, bool withInventory, bool withVars)",asFUNCTION(BIND_CLASS Global_SwapCritters),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetAllItems(uint16 pid, Item@[]@+ items) const",asFUNCTION(BIND_CLASS Global_GetAllItems),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetAllNpc(uint16 pid, Critter@[]@+ npc) const",asFUNCTION(BIND_CLASS Global_GetAllNpc),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetAllMaps(uint16 pid, Map@[]@+ maps) const",asFUNCTION(BIND_CLASS Global_GetAllMaps),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetAllLocations(uint16 pid, Location@[]@+ locations) const",asFUNCTION(BIND_CLASS Global_GetAllLocations),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetScriptId(string& scriptName, string& funcDeclaration) const",asFUNCTION(BIND_CLASS Global_GetScriptId),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ GetScriptName(uint scriptId) const",asFUNCTION(BIND_CLASS Global_GetScriptName),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int8[]@ GetItemDataMask(int maskType) const",asFUNCTION(BIND_CLASS Global_GetItemDataMask),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetItemDataMask(int maskType, int8[]& mask)",asFUNCTION(BIND_CLASS Global_SetItemDataMask),asCALL_CDECL)<0) BIND_ERROR;
#endif

#ifdef BIND_CLIENT
	if(engine->RegisterObjectType("CritterCl",0,asOBJ_REF)<0) BIND_ERROR;
	if(engine->RegisterObjectType("ItemCl",0,asOBJ_REF)<0) BIND_ERROR;

	if(engine->RegisterObjectBehaviour("CritterCl",asBEHAVE_ADDREF,"void f()",asMETHOD(CritterCl,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("CritterCl",asBEHAVE_RELEASE,"void f()",asMETHOD(CritterCl,Release),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool IsChosen() const",asFUNCTION(BIND_CLASS Crit_IsChosen),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool IsPlayer() const",asFUNCTION(BIND_CLASS Crit_IsPlayer),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool IsNpc() const",asFUNCTION(BIND_CLASS Crit_IsNpc),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool IsLife() const",asFUNCTION(BIND_CLASS Crit_IsLife),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool IsKnockout() const",asFUNCTION(BIND_CLASS Crit_IsKnockout),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool IsDead() const",asFUNCTION(BIND_CLASS Crit_IsDead),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool IsFree() const",asFUNCTION(BIND_CLASS Crit_IsFree),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool IsBusy() const",asFUNCTION(BIND_CLASS Crit_IsBusy),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool IsAnim3d() const",asFUNCTION(BIND_CLASS Crit_IsAnim3d),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool IsAnimAviable(uint anim1, uint anim2) const",asFUNCTION(BIND_CLASS Crit_IsAnimAviable),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool IsAnimPlaying() const",asFUNCTION(BIND_CLASS Crit_IsAnimPlaying),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","uint GetAnim1() const",asFUNCTION(BIND_CLASS Crit_GetAnim1),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","void Animate(uint anim1, uint anim2)",asFUNCTION(BIND_CLASS Crit_Animate),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","void Animate(uint anim1, uint anim2, ItemCl@+ item)",asFUNCTION(BIND_CLASS Crit_AnimateEx),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","void ClearAnim()",asFUNCTION(BIND_CLASS Crit_ClearAnim),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","void Wait(uint ms)",asFUNCTION(BIND_CLASS Crit_Wait),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","uint ItemsCount() const",asFUNCTION(BIND_CLASS Crit_ItemsCount),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","uint ItemsWeight() const",asFUNCTION(BIND_CLASS Crit_ItemsWeight),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","uint ItemsVolume() const",asFUNCTION(BIND_CLASS Crit_ItemsVolume),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","uint CountItem(uint16 protoId) const",asFUNCTION(BIND_CLASS Crit_CountItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","ItemCl@+ GetItem(uint16 protoId, int slot) const",asFUNCTION(BIND_CLASS Crit_GetItem),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","uint GetItems(int slot, ItemCl@[]@+ items) const",asFUNCTION(BIND_CLASS Crit_GetItems),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","uint GetItemsByType(int type, ItemCl@[]@+ items) const",asFUNCTION(BIND_CLASS Crit_GetItemsByType),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","ProtoItem@+ GetSlotProto(int slot) const",asFUNCTION(BIND_CLASS Crit_GetSlotProto),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","void SetVisible(bool visible)",asFUNCTION(BIND_CLASS Crit_SetVisible),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","bool GetVisible() const",asFUNCTION(BIND_CLASS Crit_GetVisible),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	//if(engine->RegisterObjectMethod("CritterCl","bool IsMyTurn() const",asFUNCTION(BIND_CLASS Crit_CheckKey),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	//	if(engine->RegisterObjectMethod("CritterCl","bool MoveToDir(uint8 dir)",asFUNCTION(BIND_CLASS Crit_MoveToDir),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	//	if(engine->RegisterObjectMethod("CritterCl","bool MoveToHex(uint16 hexX, uint16 hexY)",asFUNCTION(BIND_CLASS Crit_MoveToDir),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	//	if(engine->RegisterObjectMethod("CritterCl","void Say(uint8 howSay, string& text)",asFUNCTION(BIND_CLASS Crit_Say),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	//	if(engine->RegisterObjectMethod("CritterCl","void SetDir(uint8 dir)",asFUNCTION(BIND_CLASS Crit_SetDir),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","void set_ContourColor(uint value)",asFUNCTION(BIND_CLASS Crit_set_ContourColor),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("CritterCl","uint get_ContourColor() const",asFUNCTION(BIND_CLASS Crit_get_ContourColor),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("CritterCl","const uint Id",offsetof(CritterCl,Id))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const uint16 Pid",offsetof(CritterCl,Pid))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const uint CrType",offsetof(CritterCl,BaseType))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const uint CrTypeAlias",offsetof(CritterCl,BaseTypeAlias))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const uint16 HexX",offsetof(CritterCl,HexX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const uint16 HexY",offsetof(CritterCl,HexY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const uint8 Dir",offsetof(CritterCl,CrDir))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const uint8 Cond",offsetof(CritterCl,Cond))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const uint8 CondExt",offsetof(CritterCl,CondExt))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const uint Flags",offsetof(CritterCl,Flags))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const string Name",offsetof(CritterCl,Name))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const string Lexems",offsetof(CritterCl,Lexems))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","uint NameColor",offsetof(CritterCl,NameColor))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","const int16 Ref",offsetof(CritterCl,RefCounter))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","DataVal Param",offsetof(CritterCl,ThisPtr[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","DataRef ParamBase",offsetof(CritterCl,ThisPtr[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("CritterCl","int[]@ Anim3dLayer",offsetof(CritterCl,Layers3d))<0) BIND_ERROR;

	if(engine->RegisterObjectBehaviour("ItemCl",asBEHAVE_ADDREF,"void f()",asMETHOD(Item,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("ItemCl",asBEHAVE_RELEASE,"void f()",asMETHOD(Item,Release),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ItemCl","bool IsGrouped() const",asFUNCTION(BIND_CLASS Item_IsGrouped),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ItemCl","bool IsWeared() const",asFUNCTION(BIND_CLASS Item_IsWeared),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ItemCl","uint GetScriptId() const",asFUNCTION(BIND_CLASS Item_GetScriptId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ItemCl","uint8 GetType() const",asFUNCTION(BIND_CLASS Item_GetType),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ItemCl","uint16 GetProtoId() const",asFUNCTION(BIND_CLASS Item_GetProtoId),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ItemCl","uint GetCount() const",asFUNCTION(BIND_CLASS Item_GetCount),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ItemCl","bool GetMapPosition(uint16& hexX, uint16& hexY) const",asFUNCTION(BIND_CLASS Item_GetMapPosition),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ItemCl","void Animate(uint8 fromFrame, uint8 toFrame)",asFUNCTION(BIND_CLASS Item_Animate),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ItemCl","bool IsCar() const",asFUNCTION(BIND_CLASS Item_IsCar),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ItemCl","ItemCl@+ CarGetBag(int numBag) const",asFUNCTION(BIND_CLASS Item_CarGetBag),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint Id",offsetof(Item,Id))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const ProtoItem@ Proto",offsetof(Item,Proto))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const string Lexems",offsetof(Item,Lexems))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 Accessory",offsetof(Item,Accessory))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint MapId",offsetof(Item,ACC_HEX.MapId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint16 HexX",offsetof(Item,ACC_HEX.HexX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint16 HexY",offsetof(Item,ACC_HEX.HexY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint CritId",offsetof(Item,ACC_CRITTER.Id))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 CritSlot",offsetof(Item,ACC_CRITTER.Slot))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint ContainerId",offsetof(Item,ACC_CONTAINER.ContainerId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint SpecialId",offsetof(Item,ACC_CONTAINER.SpecialId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint16 SortValue",offsetof(Item,Data.SortValue))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 Info",offsetof(Item,Data.Info))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint PicMap",offsetof(Item,Data.PicMapHash))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint PicInv",offsetof(Item,Data.PicInvHash))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint16 AnimWaitBase",offsetof(Item,Data.AnimWaitBase))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 AnimStayBegin",offsetof(Item,Data.AnimStay[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 AnimStayEnd",offsetof(Item,Data.AnimStay[1]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 AnimShowBegin",offsetof(Item,Data.AnimShow[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 AnimShowEnd",offsetof(Item,Data.AnimShow[1]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 AnimHideBegin",offsetof(Item,Data.AnimHide[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 AnimHideEnd",offsetof(Item,Data.AnimHide[1]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint Flags",offsetof(Item,Data.Flags))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int Val0",offsetof(Item,Data.ScriptValues[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int Val1",offsetof(Item,Data.ScriptValues[1]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int Val2",offsetof(Item,Data.ScriptValues[2]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int Val3",offsetof(Item,Data.ScriptValues[3]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int Val4",offsetof(Item,Data.ScriptValues[4]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int Val5",offsetof(Item,Data.ScriptValues[5]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int Val6",offsetof(Item,Data.ScriptValues[6]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int Val7",offsetof(Item,Data.ScriptValues[7]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int Val8",offsetof(Item,Data.ScriptValues[8]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int Val9",offsetof(Item,Data.ScriptValues[9]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int8 LightIntensity",offsetof(Item,Data.LightIntensity))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 LightDistance",offsetof(Item,Data.LightDistance))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 LightFlags",offsetof(Item,Data.LightFlags))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint LightColor",offsetof(Item,Data.LightColor))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const int16 TrapValue",offsetof(Item,Data.TrapValue))<0) BIND_ERROR;
	// Tech info
	if(engine->RegisterObjectProperty("ItemCl","const uint8 BrokenFlags",offsetof(Item,Data.TechInfo.DeteorationFlags))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint8 BrokenCount",offsetof(Item,Data.TechInfo.DeteorationCount))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint16 BrokenWear",offsetof(Item,Data.TechInfo.DeteorationValue))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint16 WeaponAmmoPid",offsetof(Item,Data.TechInfo.AmmoPid))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint16 WeaponAmmoCount",offsetof(Item,Data.TechInfo.AmmoCount))<0) BIND_ERROR;
	// Locker
	if(engine->RegisterObjectProperty("ItemCl","const uint LockerId",offsetof(Item,Data.Locker.DoorId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint16 LockerCondition",offsetof(Item,Data.Locker.Condition))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint16 LockerComplexity",offsetof(Item,Data.Locker.Complexity))<0) BIND_ERROR;
	// Car
	if(engine->RegisterObjectProperty("ItemCl","const uint16 CarFuel",offsetof(Item,Data.Car.Fuel))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ItemCl","const uint16 CarDeteoration",offsetof(Item,Data.Car.Deteoration))<0) BIND_ERROR;
	// Radio
	if(engine->RegisterObjectProperty("ItemCl","const uint16 RadioChannel",offsetof(Item,Data.Radio.Channel))<0) BIND_ERROR;
	// Holodisk
	if(engine->RegisterObjectProperty("ItemCl","const uint HolodiskNumber",offsetof(Item,Data.Holodisk.Number))<0) BIND_ERROR;

	if(engine->RegisterGlobalFunction("CritterCl@+ GetChosen()",asFUNCTION(BIND_CLASS Global_GetChosen),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("ItemCl@+ GetItem(uint itemId)",asFUNCTION(BIND_CLASS Global_GetItem),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetCrittersDistantion(CritterCl& cr1, CritterCl& cr2)",asFUNCTION(BIND_CLASS Global_GetCrittersDistantion),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("CritterCl@+ GetCritter(uint critterId)",asFUNCTION(BIND_CLASS Global_GetCritter),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetCrittersHex(uint16 hexX, uint16 hexY, uint radius, int findType, CritterCl@[]@+ critters) const",asFUNCTION(BIND_CLASS Global_GetCritters),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetCritters(uint16 pid, int findType, CritterCl@[]@+ critters) const",asFUNCTION(BIND_CLASS Global_GetCrittersByPids),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, CritterCl@[]@+ critters) const",asFUNCTION(BIND_CLASS Global_GetCrittersInPath),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, CritterCl@[]@+ critters, uint16& preBlockHx, uint16& preBlockHy, uint16& blockHx, uint16& blockHy) const",asFUNCTION(BIND_CLASS Global_GetCrittersInPathBlock),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist) const",asFUNCTION(BIND_CLASS Global_GetHexInPath),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut) const",asFUNCTION(BIND_CLASS Global_GetPathLength),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void FlushScreen(uint fromColor, uint toColor, uint timeMs)",asFUNCTION(BIND_CLASS Global_FlushScreen),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void QuakeScreen(uint noise, uint timeMs)",asFUNCTION(BIND_CLASS Global_QuakeScreen),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void PlaySound(string& soundName)",asFUNCTION(BIND_CLASS Global_PlaySound),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void PlaySound(uint8 soundType, uint8 soundTypeExt, uint8 soundId, uint8 soundIdExt)",asFUNCTION(BIND_CLASS Global_PlaySoundType),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void PlayMusic(string& musicName, uint pos, uint repeat)",asFUNCTION(BIND_CLASS Global_PlayMusic),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void PlayVideo(string& videoName, bool canStop)",asFUNCTION(BIND_CLASS Global_PlayVideo),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool IsTurnBased()",asFUNCTION(BIND_CLASS Global_IsTurnBased),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint16 GetCurrentMapPid()",asFUNCTION(BIND_CLASS Global_GetCurrentMapPid),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetMessageFilters(int[]@+ filters) const",asFUNCTION(BIND_CLASS Global_GetMessageFilters),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void SetMessageFilters(int[]@+ filters)",asFUNCTION(BIND_CLASS Global_SetMessageFilters),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void Message(string& text)",asFUNCTION(BIND_CLASS Global_Message),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void Message(string& text, int type)",asFUNCTION(BIND_CLASS Global_MessageType),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void Message(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_MessageMsg),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void Message(int textMsg, uint strNum, int type)",asFUNCTION(BIND_CLASS Global_MessageMsgType),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void MapMessage(string& text, uint16 hx, uint16 hy, uint timeMs, uint color, bool fade, int offsX, int offsY)",asFUNCTION(BIND_CLASS Global_MapMessage),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ GetMsgStr(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_GetMsgStr),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ GetMsgStr(int textMsg, uint strNum, uint skipCount)",asFUNCTION(BIND_CLASS Global_GetMsgStrSkip),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetMsgStrNumUpper(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_GetMsgStrNumUpper),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetMsgStrNumLower(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_GetMsgStrNumLower),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetMsgStrCount(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_GetMsgStrCount),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool IsMsgStr(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_IsMsgStr),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ ReplaceText(const string& text, const string& replace, const string& str)",asFUNCTION(BIND_CLASS Global_ReplaceTextStr),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ ReplaceText(const string& text, const string& replace, int i)",asFUNCTION(BIND_CLASS Global_ReplaceTextInt),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ FormatTags(const string& text, const string@+ lexems)",asFUNCTION(BIND_CLASS Global_FormatTags),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetSomeValue(int var)",asFUNCTION(BIND_CLASS Global_GetSomeValue),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool LoadDat(string& datName)",asFUNCTION(BIND_CLASS Global_LoadDat),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void MoveScreen(uint16 hexX, uint16 hexY, uint speed)",asFUNCTION(BIND_CLASS Global_MoveScreen),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void LockScreenScroll(CritterCl@+ cr)",asFUNCTION(BIND_CLASS Global_LockScreenScroll),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetFog(uint16 zoneX, uint16 zoneY)",asFUNCTION(BIND_CLASS Global_GetFog),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void RefreshItemsCollection(int collection)",asFUNCTION(BIND_CLASS Global_RefreshItemsCollection),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetScroll(int scrollElement) const",asFUNCTION(BIND_CLASS Global_GetScroll),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void SetScroll(int scrollElement, int value)",asFUNCTION(BIND_CLASS Global_SetScroll),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetDayTime(uint dayPart) const",asFUNCTION(BIND_CLASS Global_GetDayTime),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void GetDayColor(uint dayPart, uint8& r, uint8& g, uint8& b) const",asFUNCTION(BIND_CLASS Global_GetDayColor),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void RunServerScript(string& funcName, int p0, int p1, int p2, string@+ p3, int[]@+ p4)",asFUNCTION(BIND_CLASS Global_RunServerScript),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void RunServerScriptUnsafe(string& funcName, int p0, int p1, int p2, string@+ p3, int[]@+ p4)",asFUNCTION(BIND_CLASS Global_RunServerScriptUnsafe),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint LoadSprite(string& name, int pathIndex)",asFUNCTION(BIND_CLASS Global_LoadSprite),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint LoadSprite(uint nameHash, uint8 dir)",asFUNCTION(BIND_CLASS Global_LoadSpriteHash),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetSpriteWidth(uint sprId, int sprIndex)",asFUNCTION(BIND_CLASS Global_GetSpriteWidth),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetSpriteHeight(uint sprId, int sprIndex)",asFUNCTION(BIND_CLASS Global_GetSpriteHeight),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetSpriteCount(uint sprId)",asFUNCTION(BIND_CLASS Global_GetSpriteCount),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawSprite(uint sprId, int sprIndex, int x, int y, uint color)",asFUNCTION(BIND_CLASS Global_DrawSprite),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawSprite(uint sprId, int sprIndex, int x, int y, int w, int h, bool scratch, bool center, uint color)",asFUNCTION(BIND_CLASS Global_DrawSpriteSize),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawText(string& text, int x, int y, int w, int h, uint color, int font, int flags)",asFUNCTION(BIND_CLASS Global_DrawText),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawPrimitive(int primitiveType, int[]& data)",asFUNCTION(BIND_CLASS Global_DrawPrimitive),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawMapSprite(uint16 hx, uint16 hy, uint16 effectPid, uint sprId, int sprIndex, int offsX, int offsY)",asFUNCTION(BIND_CLASS Global_DrawMapSprite),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawCritter2d(uint crType, uint anim1, uint anim2, uint8 dir, int l, int t, int r, int b, bool scratch, bool center, uint color)",asFUNCTION(BIND_CLASS Global_DrawCritter2d),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawCritter3d(uint instance, uint crType, uint anim1, uint anim2, int[]@+ layers, float[]@+ position, uint color)",asFUNCTION(BIND_CLASS Global_DrawCritter3d),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void ShowScreen(int screen, int p0, int p1, int p2)",asFUNCTION(BIND_CLASS Global_ShowScreen),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void HideScreen(int screen, int p0, int p1, int p2)",asFUNCTION(BIND_CLASS Global_HideScreen),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void GetHardcodedScreenPos(int screen, int& x, int& y)",asFUNCTION(BIND_CLASS Global_GetHardcodedScreenPos),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawHardcodedScreen(int screen)",asFUNCTION(BIND_CLASS Global_DrawHardcodedScreen),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetKeybLang() const",asFUNCTION(BIND_CLASS Global_GetKeybLang),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetHexPos(uint16 hx, uint16 hy, int& x, int& y) const",asFUNCTION(BIND_CLASS Global_GetHexPos),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetMonitorHex(int x, int y, uint16& hx, uint16& hy) const",asFUNCTION(BIND_CLASS Global_GetMonitorHex),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void GetMousePosition(int& x, int& y) const",asFUNCTION(BIND_CLASS Global_GetMousePosition),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("ItemCl@+ GetMonitorItem(int x, int y)",asFUNCTION(BIND_CLASS Global_GetMonitorItem),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("CritterCl@+ GetMonitorCritter(int x, int y)",asFUNCTION(BIND_CLASS Global_GetMonitorCritter),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint16 GetMapWidth()",asFUNCTION(BIND_CLASS Global_GetMapWidth),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint16 GetMapHeight()",asFUNCTION(BIND_CLASS Global_GetMapHeight),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetCurrentCursor() const",asFUNCTION(BIND_CLASS Global_GetCurrentCursor),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetLastCursor() const",asFUNCTION(BIND_CLASS Global_GetLastCursor),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void ChangeCursor(int cursor)",asFUNCTION(BIND_CLASS Global_ChangeCursor),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps)",asFUNCTION(BIND_CLASS Global_MoveHexByDir),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ GetIfaceIniStr(string& key)",asFUNCTION(BIND_CLASS Global_GetIfaceIniStr),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool Load3dFile(string& fileName, int pathType)",asFUNCTION(BIND_CLASS Global_Load3dFile),asCALL_CDECL)<0) BIND_ERROR;

	if(engine->RegisterGlobalProperty("uint8 __RoofAlpha",&OptRoofAlpha)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __HideCursor",&OptHideCursor)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const int __ScreenWidth",&OptScreenWidth)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const int __ScreenHeight",&OptScreenHeight)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const int __MultiSampling",&OptMultiSampling)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const bool __SoftwareSkinning",&OptSoftwareSkinning)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DisableLMenu",&OptDisableLMenu)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DisableMouseEvents",&OptDisableMouseEvents)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DisableKeyboardEvents",&OptDisableKeyboardEvents)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __PlayerOffAppendix",&OptPlayerOffAppendix)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __DamageHitDelay",&OptDamageHitDelay)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __CombatMessagesType",&OptCombatMessagesType)) BIND_ERROR;

	if(engine->RegisterGlobalProperty("bool __DiLeft",&CmnDiLeft)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DiRight",&CmnDiRight)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DiUp",&CmnDiUp)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DiDown",&CmnDiDown)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DiMleft",&CmnDiMleft)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DiMright",&CmnDiMright)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DiMup",&CmnDiMup)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DiMdown",&CmnDiMdown)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __ShowGroups",&OptShowGroups)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __HelpInfo",&OptHelpInfo)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __FullScr",&OptFullScr)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __VSync",&OptVSync)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __FlushVal",&OptFlushVal)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __BaseTex",&OptBaseTex)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __ScreenClear",&OptScreenClear)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __Light",&OptLight)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __ScrollDelay",&OptScrollDelay)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __ScrollStep",&OptScrollStep)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __MouseScroll",&OptMouseScroll)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __ScrollCheck",&OptScrollCheck)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __MouseSpeed",&OptMouseSpeed)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __GlobalSound",&OptGlobalSound)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __MasterPath",&OptMasterPath)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __CritterPath",&OptCritterPath)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __FoPatchPath",&OptFoPatchPath)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __FoDataPath",&OptFoDataPath)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __Host",&OptHost)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __Port",&OptPort)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ProxyType",&OptProxyType)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __ProxyHost",&OptProxyHost)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ProxyPort",&OptProxyPort)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __ProxyUser",&OptProxyUser)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __ProxyPass",&OptProxyPass)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __Name",&OptName)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __Pass",&OptPass)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TextDelay",&OptTextDelay)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __AlwaysOnTop",&OptAlwaysOnTop)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __Sleep",&OptSleep)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __MsgboxInvert",&OptMsgboxInvert)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __ChangeLang",&OptChangeLang)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint8 __DefaultCombatMode",&OptDefaultCombatMode)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __MessNotify",&OptMessNotify)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __SoundNotify",&OptSoundNotify)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __IndicatorType",&OptIndicatorType)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __DoubleClickTime",&OptDoubleClickTime)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __RunModMul",&GameOpt.RunModMul)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __RunModDiv",&GameOpt.RunModDiv)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __RunModAdd",&GameOpt.RunModAdd)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DisableDrawScreens",&GameOpt.DisableDrawScreens)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __Animation3dSmoothTime",&GameOpt.Animation3dSmoothTime)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __Animation3dFPS",&GameOpt.Animation3dFPS)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("float __SpritesZoom",&SpritesZoom)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("float __SpritesZoomMin",&SpritesZoomMin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("float __SpritesZoomMax",&SpritesZoomMax)) BIND_ERROR;
#endif

#if defined(BIND_CLIENT) || defined(BIND_SERVER)
	if(engine->RegisterGlobalFunction("uint GetFullMinute(uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute) const",asFUNCTION(BIND_CLASS Global_GetFullMinute),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void GetTime(uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second, uint16& milliseconds) const",asFUNCTION(BIND_CLASS Global_GetTime),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void GetGameTime(uint fullMinute, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute) const",asFUNCTION(BIND_CLASS Global_GetGameTime),asCALL_CDECL)<0) BIND_ERROR;
	//if(engine->RegisterGlobalFunction("void GetVersion(uint& server, uint& client, uint& net) const",asFUNCTION(BIND_CLASS Global_GetVersion),asCALL_CDECL)<0) BIND_ERROR;
//	if(engine->RegisterGlobalFunction("void Sort(int[]& array, string& func)",asFUNCTION(BIND_CLASS Global_Sort),asCALL_CDECL)<0) BIND_ERROR;
//	if(engine->RegisterGlobalFunction("void Sort(uint[]& array, string& func)",asFUNCTION(BIND_CLASS Global_Sort),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetParameterGetBehaviour(uint index, string& funcName)",asFUNCTION(BIND_CLASS Global_SetParameterGetBehaviour),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SetParameterChangeBehaviour(uint index, string& funcName)",asFUNCTION(BIND_CLASS Global_SetParameterChangeBehaviour),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void AllowSlot(uint8 index, string& iniOption)",asFUNCTION(BIND_CLASS Global_AllowSlot),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void SetRegistrationParameter(uint index, bool enabled)",asFUNCTION(BIND_CLASS Global_SetRegistrationParam),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool IsCritterCanWalk(uint crType) const",asFUNCTION(BIND_CLASS Global_IsCritterCanWalk),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool IsCritterCanRun(uint crType) const",asFUNCTION(BIND_CLASS Global_IsCritterCanRun),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool IsCritterCanRotate(uint crType) const",asFUNCTION(BIND_CLASS Global_IsCritterCanRotate),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool IsCritterCanAim(uint crType) const",asFUNCTION(BIND_CLASS Global_IsCritterCanAim),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool IsCritterAnim1(uint crType, uint anim1) const",asFUNCTION(BIND_CLASS Global_IsCritterAnim1),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool IsCritterAnim3d(uint crType) const",asFUNCTION(BIND_CLASS Global_IsCritterAnim3d),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetGlobalMapRelief(uint x, uint y) const",asFUNCTION(BIND_CLASS Global_GetGlobalMapRelief),asCALL_CDECL)<0) BIND_ERROR;

	if(engine->RegisterGlobalProperty("const uint16 __Year",&GameOpt.Year)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const uint16 __Month",&GameOpt.Month)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const uint16 __Day",&GameOpt.Day)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const uint16 __Hour",&GameOpt.Hour)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const uint16 __Minute",&GameOpt.Minute)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const uint16 __TimeMultiplier",&GameOpt.TimeMultiplier)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const uint __FullMinute",&GameOpt.FullMinute)) BIND_ERROR;

	if(engine->RegisterGlobalProperty("bool __DisableTcpNagle",&GameOpt.DisableTcpNagle)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DisableZlibCompression",&GameOpt.DisableZlibCompression)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __FloodSize",&GameOpt.FloodSize)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __FreeExp",&GameOpt.FreeExp)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __RegulatePvP",&GameOpt.RegulatePvP)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __NoAnswerShuffle",&GameOpt.NoAnswerShuffle)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ForceDialog",&GameOpt.ForceDialog)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __DialogDemandRecheck",&GameOpt.DialogDemandRecheck)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __FixBoyDefaultExperience",&GameOpt.FixBoyDefaultExperience)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __SneakDivider",&GameOpt.SneakDivider)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LevelCap",&GameOpt.LevelCap)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __LevelCapAddExperience",&GameOpt.LevelCapAddExperience)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LookNormal",&GameOpt.LookNormal)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LookMinimum",&GameOpt.LookMinimum)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __GlobalMapMaxGroupCount",&GameOpt.GlobalMapMaxGroupCount)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __CritterIdleTick",&GameOpt.CritterIdleTick)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TurnBasedTick",&GameOpt.TurnBasedTick)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("string __UserInterface",&GameOpt.UserInterface)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __DeadHitPoints",&GameOpt.DeadHitPoints)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __Breaktime",&GameOpt.Breaktime)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TimeoutTransfer",&GameOpt.TimeoutTransfer)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TimeoutBattle",&GameOpt.TimeoutBattle)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ApRegeneration",&GameOpt.ApRegeneration)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __RtApCostCritterWalk",&GameOpt.RtApCostCritterWalk)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __RtApCostCritterRun",&GameOpt.RtApCostCritterRun)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __RtApCostMoveItemContainer",&GameOpt.RtApCostMoveItemContainer)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __RtApCostMoveItemInventory",&GameOpt.RtApCostMoveItemInventory)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __RtApCostPickItem",&GameOpt.RtApCostPickItem)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __RtApCostDropItem",&GameOpt.RtApCostDropItem)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __RtApCostReloadWeapon",&GameOpt.RtApCostReloadWeapon)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __RtApCostPickCritter",&GameOpt.RtApCostPickCritter)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __RtApCostUseItem",&GameOpt.RtApCostUseItem)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __RtApCostUseSkill",&GameOpt.RtApCostUseSkill)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __RtAlwaysRun",&GameOpt.RtAlwaysRun)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TbApCostCritterMove",&GameOpt.TbApCostCritterMove)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TbApCostMoveItemContainer",&GameOpt.TbApCostMoveItemContainer)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TbApCostMoveItemInventory",&GameOpt.TbApCostMoveItemInventory)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TbApCostPickItem",&GameOpt.TbApCostPickItem)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TbApCostDropItem",&GameOpt.TbApCostDropItem)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TbApCostReloadWeapon",&GameOpt.TbApCostReloadWeapon)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TbApCostPickCritter",&GameOpt.TbApCostPickCritter)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TbApCostUseItem",&GameOpt.TbApCostUseItem)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TbApCostUseSkill",&GameOpt.TbApCostUseSkill)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ApCostAimEyes",&GameOpt.ApCostAimEyes)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ApCostAimHead",&GameOpt.ApCostAimHead)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ApCostAimGroin",&GameOpt.ApCostAimGroin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ApCostAimTorso",&GameOpt.ApCostAimTorso)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ApCostAimArms",&GameOpt.ApCostAimArms)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ApCostAimLegs",&GameOpt.ApCostAimLegs)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __TbAlwaysRun",&GameOpt.TbAlwaysRun)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __HitAimEyes",&GameOpt.HitAimEyes)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __HitAimHead",&GameOpt.HitAimHead)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __HitAimGroin",&GameOpt.HitAimGroin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __HitAimTorso",&GameOpt.HitAimTorso)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __HitAimArms",&GameOpt.HitAimArms)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __HitAimLegs",&GameOpt.HitAimLegs)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __RunOnCombat",&GameOpt.RunOnCombat)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __RunOnTransfer",&GameOpt.RunOnTransfer)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __GlobalMapWidth",&GameOpt.GlobalMapWidth)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __GlobalMapHeight",&GameOpt.GlobalMapHeight)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __GlobalMapZoneLength",&GameOpt.GlobalMapZoneLength)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __BagRefreshTime",&GameOpt.BagRefreshTime)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __AttackAnimationsMinDist",&GameOpt.AttackAnimationsMinDist)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __WisperDist",&GameOpt.WhisperDist)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __LookChecks",&GameOpt.LookChecks)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LookDir0",&GameOpt.LookDir[0])) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LookDir1",&GameOpt.LookDir[1])) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LookDir2",&GameOpt.LookDir[2])) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LookDir3",&GameOpt.LookDir[3])) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LookSneakDir0",&GameOpt.LookSneakDir[0])) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LookSneakDir1",&GameOpt.LookSneakDir[1])) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LookSneakDir2",&GameOpt.LookSneakDir[2])) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LookSneakDir3",&GameOpt.LookSneakDir[3])) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __LookWeight",&GameOpt.LookWeight)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __CustomItemCost",&GameOpt.CustomItemCost)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __RegistrationTimeout",&GameOpt.RegistrationTimeout)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __AccountPlayTime",&GameOpt.AccountPlayTime)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __LoggingVars",&GameOpt.LoggingVars)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("bool __SkipScriptBinaries",&GameOpt.SkipScriptBinaries)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ScriptRunSuspendTimeout",&GameOpt.ScriptRunSuspendTimeout)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ScriptRunMessageTimeout",&GameOpt.ScriptRunMessageTimeout)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TalkDistance",&GameOpt.TalkDistance)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __MinNameLength",&GameOpt.MinNameLength)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __MaxNameLength",&GameOpt.MaxNameLength)) BIND_ERROR;

	if(engine->RegisterGlobalProperty("bool __AbsoluteOffsets",&GameOpt.AbsoluteOffsets)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __SkillBegin",&GameOpt.SkillBegin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __SkillEnd",&GameOpt.SkillEnd)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TimeoutBegin",&GameOpt.TimeoutBegin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TimeoutEnd",&GameOpt.TimeoutEnd)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __KillBegin",&GameOpt.KillBegin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __KillEnd",&GameOpt.KillEnd)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __PerkBegin",&GameOpt.PerkBegin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __PerkEnd",&GameOpt.PerkEnd)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __AddictionBegin",&GameOpt.AddictionBegin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __AddictionEnd",&GameOpt.AddictionEnd)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __KarmaBegin",&GameOpt.KarmaBegin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __KarmaEnd",&GameOpt.KarmaEnd)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __DamageBegin",&GameOpt.DamageBegin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __DamageEnd",&GameOpt.DamageEnd)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TraitBegin",&GameOpt.TraitBegin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __TraitEnd",&GameOpt.TraitEnd)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ReputationBegin",&GameOpt.ReputationBegin)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint __ReputationEnd",&GameOpt.ReputationEnd)) BIND_ERROR;

	if(engine->RegisterGlobalProperty("int __ReputationLoved",&GameOpt.ReputationLoved)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __ReputationLiked",&GameOpt.ReputationLiked)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __ReputationAccepted",&GameOpt.ReputationAccepted)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __ReputationNeutral",&GameOpt.ReputationNeutral)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __ReputationAntipathy",&GameOpt.ReputationAntipathy)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("int __ReputationHated",&GameOpt.ReputationHated)) BIND_ERROR;
#endif

#ifdef BIND_MAPPER
	if(engine->RegisterObjectType("MapperObject",0,asOBJ_REF)<0) BIND_ERROR;
	if(engine->RegisterObjectType("MapperMap",0,asOBJ_REF)<0) BIND_ERROR;

	// MapperObject
	if(engine->RegisterObjectBehaviour("MapperObject",asBEHAVE_ADDREF,"void f()",asMETHOD(MapObject,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("MapperObject",asBEHAVE_RELEASE,"void f()",asMETHOD(MapObject,Release),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","void Update() const",asFUNCTION(BIND_CLASS MapperObject_Update),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","MapperObject@+ AddChild(uint16 pid)",asFUNCTION(BIND_CLASS MapperObject_AddChild),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","uint GetChilds(MapperObject@[]@+ objects) const",asFUNCTION(BIND_CLASS MapperObject_Update),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","string@ get_ScriptName() const",asFUNCTION(BIND_CLASS MapperObject_get_ScriptName),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","void set_ScriptName(string@ name)",asFUNCTION(BIND_CLASS MapperObject_set_ScriptName),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","string@ get_FuncName() const",asFUNCTION(BIND_CLASS MapperObject_get_FuncName),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","void set_FuncName(string@ name)",asFUNCTION(BIND_CLASS MapperObject_set_FuncName),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","uint8 get_Critter_Cond() const",asFUNCTION(BIND_CLASS MapperObject_get_Critter_Cond),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","void set_Critter_Cond(uint8 value)",asFUNCTION(BIND_CLASS MapperObject_set_Critter_Cond),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","void MoveToHex(uint16 hexX, uint16 hexY)",asFUNCTION(BIND_CLASS MapperObject_MoveToHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","void MoveToHexOffset(int x, int y)",asFUNCTION(BIND_CLASS MapperObject_MoveToHexOffset),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","void MoveToDir(uint8 dir)",asFUNCTION(BIND_CLASS MapperObject_MoveToDir),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	// Generic
	if(engine->RegisterObjectProperty("MapperObject","const uint8 MapObjType",offsetof(MapObject,MapObjType))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","const uint16 ProtoId",offsetof(MapObject,ProtoId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","const uint16 MapX",offsetof(MapObject,MapX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","const uint16 MapY",offsetof(MapObject,MapY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 Dir",offsetof(MapObject,Dir))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint LightColor",offsetof(MapObject,LightColor))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 LightDay",offsetof(MapObject,LightDay))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 LightDirOff",offsetof(MapObject,LightDirOff))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 LightDistance",offsetof(MapObject,LightDistance))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int8 LightIntensity",offsetof(MapObject,LightIntensity))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int UserData0",offsetof(MapObject,UserData[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int UserData1",offsetof(MapObject,UserData[1]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int UserData2",offsetof(MapObject,UserData[2]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int UserData3",offsetof(MapObject,UserData[3]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int UserData4",offsetof(MapObject,UserData[4]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int UserData5",offsetof(MapObject,UserData[5]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int UserData6",offsetof(MapObject,UserData[6]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int UserData7",offsetof(MapObject,UserData[7]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int UserData8",offsetof(MapObject,UserData[8]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int UserData9",offsetof(MapObject,UserData[9]))<0) BIND_ERROR;
	// Critter
	if(engine->RegisterObjectProperty("MapperObject","uint8 Critter_CondExt",offsetof(MapObject,MCritter.CondExt))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex0",offsetof(MapObject,MCritter.ParamIndex[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex1",offsetof(MapObject,MCritter.ParamIndex[1]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex2",offsetof(MapObject,MCritter.ParamIndex[2]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex3",offsetof(MapObject,MCritter.ParamIndex[3]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex4",offsetof(MapObject,MCritter.ParamIndex[4]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex5",offsetof(MapObject,MCritter.ParamIndex[5]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex6",offsetof(MapObject,MCritter.ParamIndex[6]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex7",offsetof(MapObject,MCritter.ParamIndex[7]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex8",offsetof(MapObject,MCritter.ParamIndex[8]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex9",offsetof(MapObject,MCritter.ParamIndex[9]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex10",offsetof(MapObject,MCritter.ParamIndex[10]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex11",offsetof(MapObject,MCritter.ParamIndex[11]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex12",offsetof(MapObject,MCritter.ParamIndex[12]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex13",offsetof(MapObject,MCritter.ParamIndex[13]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Critter_ParamIndex14",offsetof(MapObject,MCritter.ParamIndex[14]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue0",offsetof(MapObject,MCritter.ParamValue[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue1",offsetof(MapObject,MCritter.ParamValue[1]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue2",offsetof(MapObject,MCritter.ParamValue[2]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue3",offsetof(MapObject,MCritter.ParamValue[3]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue4",offsetof(MapObject,MCritter.ParamValue[4]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue5",offsetof(MapObject,MCritter.ParamValue[5]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue6",offsetof(MapObject,MCritter.ParamValue[6]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue7",offsetof(MapObject,MCritter.ParamValue[7]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue8",offsetof(MapObject,MCritter.ParamValue[8]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue9",offsetof(MapObject,MCritter.ParamValue[9]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue10",offsetof(MapObject,MCritter.ParamValue[10]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue11",offsetof(MapObject,MCritter.ParamValue[11]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue12",offsetof(MapObject,MCritter.ParamValue[12]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue13",offsetof(MapObject,MCritter.ParamValue[13]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Critter_ParamValue14",offsetof(MapObject,MCritter.ParamValue[14]))<0) BIND_ERROR;
	// Item/critter shared parameters
	if(engine->RegisterObjectProperty("MapperObject","int16 OffsetX",offsetof(MapObject,MItem.OffsetX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 OffsetY",offsetof(MapObject,MItem.OffsetY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 AnimStayBegin",offsetof(MapObject,MItem.AnimStayBegin))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 AnimStayEnd",offsetof(MapObject,MItem.AnimStayEnd))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint16 AnimWait",offsetof(MapObject,MItem.AnimWait))<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","string@ get_PicMap() const",asFUNCTION(BIND_CLASS MapperObject_get_PicMap),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","void set_PicMap(string@ name)",asFUNCTION(BIND_CLASS MapperObject_set_PicMap),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","string@ get_PicInv() const",asFUNCTION(BIND_CLASS MapperObject_get_PicInv),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperObject","void set_PicInv(string@ name)",asFUNCTION(BIND_CLASS MapperObject_set_PicInv),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 InfoOffset",offsetof(MapObject,MItem.InfoOffset))<0) BIND_ERROR;
	// Item
	if(engine->RegisterObjectProperty("MapperObject","uint Item_Count",offsetof(MapObject,MItem.Count))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 Item_DeteorationFlags",offsetof(MapObject,MItem.DeteorationFlags))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 Item_DeteorationCount",offsetof(MapObject,MItem.DeteorationCount))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint16 Item_DeteorationValue",offsetof(MapObject,MItem.DeteorationValue))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","const bool Item_InContainer",offsetof(MapObject,MItem.InContainer))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 Item_ItemSlot",offsetof(MapObject,MItem.ItemSlot))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint16 Item_AmmoPid",offsetof(MapObject,MItem.AmmoPid))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint Item_AmmoCount",offsetof(MapObject,MItem.AmmoCount))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint Item_LockerDoorId",offsetof(MapObject,MItem.LockerDoorId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint16 Item_LockerCondition",offsetof(MapObject,MItem.LockerCondition))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint16 Item_LockerComplexity",offsetof(MapObject,MItem.LockerComplexity))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int16 Item_TrapValue",offsetof(MapObject,MItem.TrapValue))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Item_Val0",offsetof(MapObject,MItem.Val[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Item_Val1",offsetof(MapObject,MItem.Val[1]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Item_Val2",offsetof(MapObject,MItem.Val[2]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Item_Val3",offsetof(MapObject,MItem.Val[3]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Item_Val4",offsetof(MapObject,MItem.Val[4]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Item_Val5",offsetof(MapObject,MItem.Val[5]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Item_Val6",offsetof(MapObject,MItem.Val[6]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Item_Val7",offsetof(MapObject,MItem.Val[7]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Item_Val8",offsetof(MapObject,MItem.Val[8]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Item_Val9",offsetof(MapObject,MItem.Val[9]))<0) BIND_ERROR;
	// Scenery
	if(engine->RegisterObjectProperty("MapperObject","bool Scenery_CanUse",offsetof(MapObject,MScenery.CanUse))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","bool Scenery_CanTalk",offsetof(MapObject,MScenery.CanTalk))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint Scenery_TriggerNum",offsetof(MapObject,MScenery.TriggerNum))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 Scenery_ParamsCount",offsetof(MapObject,MScenery.ParamsCount))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Scenery_Param0",offsetof(MapObject,MScenery.Param[0]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Scenery_Param1",offsetof(MapObject,MScenery.Param[1]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Scenery_Param2",offsetof(MapObject,MScenery.Param[2]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Scenery_Param3",offsetof(MapObject,MScenery.Param[3]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","int Scenery_Param4",offsetof(MapObject,MScenery.Param[4]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint16 Scenery_ToMapPid",offsetof(MapObject,MScenery.ToMapPid))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint Scenery_ToEntire",offsetof(MapObject,MScenery.ToEntire))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint16 Scenery_ToMapX",offsetof(MapObject,MScenery.ToMapX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint16 Scenery_ToMapY",offsetof(MapObject,MScenery.ToMapY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("MapperObject","uint8 Scenery_ToDir",offsetof(MapObject,MScenery.ToDir))<0) BIND_ERROR;

	// MapperMap
	if(engine->RegisterObjectBehaviour("MapperMap",asBEHAVE_ADDREF,"void f()",asMETHOD(ProtoMap,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("MapperMap",asBEHAVE_RELEASE,"void f()",asMETHOD(ProtoMap,Release),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","MapperObject@+ AddObject(uint16 hexX, uint16 hexY, int mapObjType, uint16 pid)",asFUNCTION(BIND_CLASS MapperMap_AddObject),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","MapperObject@+ GetObject(uint16 hexX, uint16 hexY, int mapObjType, uint16 pid, uint skip) const",asFUNCTION(BIND_CLASS MapperMap_GetObject),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","uint GetObjects(uint16 hexX, uint16 hexY, uint radius, int mapObjType, uint16 pid, MapperObject@[]@+ objects) const",asFUNCTION(BIND_CLASS MapperMap_GetObjects),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","void UpdateObjects() const",asFUNCTION(BIND_CLASS MapperMap_UpdateObjects),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","void Resize(uint16 width, uint16 height)",asFUNCTION(BIND_CLASS MapperMap_Resize),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","uint GetTile(uint16 tileX, uint16 tileY, bool roof) const",asFUNCTION(BIND_CLASS MapperMap_GetTileHash),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","void SetTile(uint16 tileX, uint16 tileY, bool roof, uint picHash)",asFUNCTION(BIND_CLASS MapperMap_SetTileHash),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","string@ GetTileName(uint16 tileX, uint16 tileY, bool roof) const",asFUNCTION(BIND_CLASS MapperMap_GetTileName),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","void SetTileName(uint16 tileX, uint16 tileY, bool roof, string@+ picName)",asFUNCTION(BIND_CLASS MapperMap_SetTileName),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","string@ GetTerrainName(uint16 tileX, uint16 tileY) const",asFUNCTION(BIND_CLASS MapperMap_GetTerrainName),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","void SetTerrainName(uint16 tileX, uint16 tileY, string@+ terrainName)",asFUNCTION(BIND_CLASS MapperMap_SetTerrainName),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","uint GetDayTime(uint dayPart) const",asFUNCTION(BIND_CLASS MapperMap_GetDayTime),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","void SetDayTime(uint dayPart, uint time)",asFUNCTION(BIND_CLASS MapperMap_SetDayTime),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","void GetDayColor(uint dayPart, uint8& r, uint8& g, uint8& b) const",asFUNCTION(BIND_CLASS MapperMap_GetDayColor),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("MapperMap","void SetDayColor(uint dayPart, uint8 r, uint8 g, uint8 b)",asFUNCTION(BIND_CLASS MapperMap_SetDayColor),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	// Global
	if(engine->RegisterGlobalFunction("void SetDefaultCritterParam(uint index, int param)",asFUNCTION(BIND_CLASS Global_SetDefaultCritterParam),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetFastPrototypes(uint16[]@+ pids) const",asFUNCTION(BIND_CLASS Global_GetFastPrototypes),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void SetFastPrototypes(uint16[]@+ pids)",asFUNCTION(BIND_CLASS Global_SetFastPrototypes),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("MapperMap@+ LoadMap(string& fileName, int pathType)",asFUNCTION(BIND_CLASS Global_LoadMap),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void UnloadMap(MapperMap@+ map)",asFUNCTION(BIND_CLASS Global_UnloadMap),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool SaveMap(MapperMap@+ map, string& fileName, int pathType, bool text, bool pack)",asFUNCTION(BIND_CLASS Global_SaveMap),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool ShowMap(MapperMap@+ map)",asFUNCTION(BIND_CLASS Global_ShowMap),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetLoadedMaps(MapperMap@[]@+ maps) const",asFUNCTION(BIND_CLASS Global_GetLoadedMaps),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetMapFileNames(string@+ dir, string@[]@+ names) const",asFUNCTION(BIND_CLASS Global_GetMapFileNames),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DeleteObject(MapperObject@+ obj)",asFUNCTION(BIND_CLASS Global_DeleteObject),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DeleteObjects(MapperObject@[]& objects)",asFUNCTION(BIND_CLASS Global_DeleteObjects),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void SelectObject(MapperObject@+ obj, bool set)",asFUNCTION(BIND_CLASS Global_SelectObject),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void SelectObjects(MapperObject@[]& objects, bool set)",asFUNCTION(BIND_CLASS Global_SelectObjects),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("MapperObject@+ GetSelectedObject() const",asFUNCTION(BIND_CLASS Global_GetSelectedObject),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetSelectedObjects(MapperObject@[]@+ objects) const",asFUNCTION(BIND_CLASS Global_GetSelectedObjects),asCALL_CDECL)<0) BIND_ERROR;
#endif

#if /*defined(BIND_CLIENT) || */defined(BIND_MAPPER)
	if(engine->RegisterGlobalFunction("void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist) const",asFUNCTION(BIND_CLASS Global_GetHexInPath),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut) const",asFUNCTION(BIND_CLASS Global_GetPathLength),asCALL_CDECL)<0) BIND_ERROR;

	if(engine->RegisterGlobalFunction("void Message(string& text)",asFUNCTION(BIND_CLASS Global_Message),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void Message(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_MessageMsg),asCALL_CDECL)<0) BIND_ERROR;

	if(engine->RegisterGlobalFunction("void MapMessage(string& text, uint16 hx, uint16 hy, uint timeMs, uint color, bool fade, int offsX, int offsY)",asFUNCTION(BIND_CLASS Global_MapMessage),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ GetMsgStr(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_GetMsgStr),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ GetMsgStr(int textMsg, uint strNum, uint skipCount)",asFUNCTION(BIND_CLASS Global_GetMsgStrSkip),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetMsgStrNumUpper(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_GetMsgStrNumUpper),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetMsgStrNumLower(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_GetMsgStrNumLower),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetMsgStrCount(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_GetMsgStrCount),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool IsMsgStr(int textMsg, uint strNum)",asFUNCTION(BIND_CLASS Global_IsMsgStr),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ ReplaceText(const string& text, const string& replace, const string& str)",asFUNCTION(BIND_CLASS Global_ReplaceTextStr),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ ReplaceText(const string& text, const string& replace, int i)",asFUNCTION(BIND_CLASS Global_ReplaceTextInt),asCALL_CDECL)<0) BIND_ERROR;

	if(engine->RegisterGlobalFunction("bool LoadDat(string& datName)",asFUNCTION(BIND_CLASS Global_LoadDat),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void MoveScreen(uint16 hexX, uint16 hexY, uint speed)",asFUNCTION(BIND_CLASS Global_MoveScreen),asCALL_CDECL)<0) BIND_ERROR;

	if(engine->RegisterGlobalFunction("uint LoadSprite(string& name, int pathIndex)",asFUNCTION(BIND_CLASS Global_LoadSprite),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint LoadSprite(uint nameHash, uint8 dir)",asFUNCTION(BIND_CLASS Global_LoadSpriteHash),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetSpriteWidth(uint sprId, int sprIndex)",asFUNCTION(BIND_CLASS Global_GetSpriteWidth),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int GetSpriteHeight(uint sprId, int sprIndex)",asFUNCTION(BIND_CLASS Global_GetSpriteHeight),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetSpriteCount(uint sprId)",asFUNCTION(BIND_CLASS Global_GetSpriteCount),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawSprite(uint sprId, int sprIndex, int x, int y, uint color)",asFUNCTION(BIND_CLASS Global_DrawSprite),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawSprite(uint sprId, int sprIndex, int x, int y, int w, int h, bool scratch, bool center, uint color)",asFUNCTION(BIND_CLASS Global_DrawSpriteSize),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawText(string& text, int x, int y, int w, int h, uint color, int font, int flags)",asFUNCTION(BIND_CLASS Global_DrawText),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawPrimitive(int primitiveType, int[]& data)",asFUNCTION(BIND_CLASS Global_DrawPrimitive),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawMapSprite(uint16 hx, uint16 hy, uint16 effectPid, uint sprId, int sprIndex, int offsX, int offsY)",asFUNCTION(BIND_CLASS Global_DrawMapSprite),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawCritter2d(uint crType, uint anim1, uint anim2, uint8 dir, int l, int t, int r, int b, bool scratch, bool center, uint color)",asFUNCTION(BIND_CLASS Global_DrawCritter2d),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void DrawCritter3d(uint instance, uint crType, uint anim1, uint anim2, int[]@+ layers, float[]@+ position, uint color)",asFUNCTION(BIND_CLASS Global_DrawCritter3d),asCALL_CDECL)<0) BIND_ERROR;

	if(engine->RegisterGlobalFunction("int GetKeybLang() const",asFUNCTION(BIND_CLASS Global_GetKeybLang),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetHexPos(uint16 hx, uint16 hy, int& x, int& y) const",asFUNCTION(BIND_CLASS Global_GetHexPos),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool GetMonitorHex(int x, int y, uint16& hx, uint16& hy) const",asFUNCTION(BIND_CLASS Global_GetMonitorHex),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void GetMousePosition(int& x, int& y) const",asFUNCTION(BIND_CLASS Global_GetMousePosition),asCALL_CDECL)<0) BIND_ERROR;

	if(engine->RegisterGlobalFunction("void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps)",asFUNCTION(BIND_CLASS Global_MoveHexByDir),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ GetIfaceIniStr(string& key)",asFUNCTION(BIND_CLASS Global_GetIfaceIniStr),asCALL_CDECL)<0) BIND_ERROR;
#endif

	if(engine->RegisterGlobalFunction("void Log(string& text) const",asFUNCTION(BIND_CLASS Global_Log),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("string@ GetLastError() const",asFUNCTION(BIND_CLASS Global_GetLastError),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int Random(int minimum, int maximum) const",asFUNCTION(Random),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("ProtoItem@+ GetProtoItem(uint16 protoId) const",asFUNCTION(BIND_CLASS Global_GetProtoItem),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("bool StrToInt(string& text, int& result) const",asFUNCTION(BIND_CLASS Global_StrToInt),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetDistantion(uint16 hexX1, uint16 hexY1, uint16 hexX2, uint16 hexY2) const",asFUNCTION(BIND_CLASS Global_GetDistantion),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint8 GetDirection(uint16 fromX, uint16 fromY, uint16 toX, uint16 toY) const",asFUNCTION(BIND_CLASS Global_GetDirection),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint8 GetOffsetDir(uint16 hx, uint16 hy, uint16 tx, uint16 ty, float offset) const",asFUNCTION(BIND_CLASS Global_GetOffsetDir),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetTick() const",asFUNCTION(BIND_CLASS Global_GetTick),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetAngelScriptProperty(int property) const",asFUNCTION(BIND_CLASS Global_GetAngelScriptProperty),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("void SetAngelScriptProperty(int property, uint value)",asFUNCTION(BIND_CLASS Global_SetAngelScriptProperty),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("uint GetStrHash(string@+ str)",asFUNCTION(BIND_CLASS Global_GetStrHash),asCALL_CDECL)<0) BIND_ERROR;

/************************************************************************/
/*                                                                      */
/************************************************************************/
