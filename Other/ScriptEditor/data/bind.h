#include "FOdefines.h"

//PROTO_OBJECT *********************************************************************
	if(engine->RegisterObjectType("ProtoObject",sizeof(CProtoObject),asOBJ_REF)<0)
	{
		WriteLog("RegisterObjectType Object FALSE\n");
		BIND_ERROR;
	}

	if(engine->RegisterObjectBehaviour("ProtoObject",asBEHAVE_ADDREF,"void f()",asMETHOD(CProtoObject,AddRef), asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("ProtoObject",asBEHAVE_RELEASE,"void f()",asMETHOD(CProtoObject,Release), asCALL_THISCALL)<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("ProtoObject","const int16 OffsTimeShow",offsetof(CProtoObject,OffsTimeShow))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int16 OffsTimeHide",offsetof(CProtoObject,OffsTimeHide))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 Light",offsetof(CProtoObject,Light))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 NoHighlight",offsetof(CProtoObject,NoHighlight))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 LightType",offsetof(CProtoObject,LightType))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 DistanceLight",offsetof(CProtoObject,DistanceLight))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 IntensityLight",offsetof(CProtoObject,IntensityLight))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 Hidden",offsetof(CProtoObject,Hidden))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 Flat",offsetof(CProtoObject,Flat))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 NoBlock",offsetof(CProtoObject,NoBlock))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 MultiHex",offsetof(CProtoObject,MultiHex))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 ShootThru",offsetof(CProtoObject,ShootThru))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 LightThru",offsetof(CProtoObject,LightThru))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 WallTransEnd",offsetof(CProtoObject,WallTransEnd))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 TwoHands",offsetof(CProtoObject,TwoHands))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 BigGun",offsetof(CProtoObject,BigGun))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 TransType",offsetof(CProtoObject,TransType))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 TransRed",offsetof(CProtoObject,TransRed))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 TransVal",offsetof(CProtoObject,TransVal))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 CanUse",offsetof(CProtoObject,CanUse))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 CanPickUp",offsetof(CProtoObject,CanPickUp))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 CanUseSmth",offsetof(CProtoObject,CanUseSmth))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 CanLook",offsetof(CProtoObject,CanLook))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 CanTalk",offsetof(CProtoObject,CanTalk))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 Weight",offsetof(CProtoObject,Weight))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 Size",offsetof(CProtoObject,Size))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 PicMap",offsetof(CProtoObject,PicMap))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 PicInv",offsetof(CProtoObject,PicInv))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 Sound",offsetof(CProtoObject,SoundId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 Cost",offsetof(CProtoObject,Cost))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 Material",offsetof(CProtoObject,Material))<0) BIND_ERROR;

//armor
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 TypeMale",offsetof(CProtoObject,ARMOR.Anim0Male))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 TypeFemale",offsetof(CProtoObject,ARMOR.Anim0Female))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 TimeMove",offsetof(CProtoObject,ARMOR.TimeMove))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 TimeRun",offsetof(CProtoObject,ARMOR.TimeRun))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 AC",offsetof(CProtoObject,ARMOR.AC))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DrNormal",offsetof(CProtoObject,ARMOR.DRNormal))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DrLaser",offsetof(CProtoObject,ARMOR.DRLaser))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DrFire",offsetof(CProtoObject,ARMOR.DRFire))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DrPlasma",offsetof(CProtoObject,ARMOR.DRPlasma))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DrElectro",offsetof(CProtoObject,ARMOR.DRElectr))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DrEmp",offsetof(CProtoObject,ARMOR.DREmp))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DrExplode",offsetof(CProtoObject,ARMOR.DRExplode))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DtNormal",offsetof(CProtoObject,ARMOR.DTNormal))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DtLaser",offsetof(CProtoObject,ARMOR.DTLaser))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DtFire",offsetof(CProtoObject,ARMOR.DTFire))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DtPlasma",offsetof(CProtoObject,ARMOR.DTPlasma))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DtElectro",offsetof(CProtoObject,ARMOR.DTElectr))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DtEmp",offsetof(CProtoObject,ARMOR.DTEmp))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DtExplode",offsetof(CProtoObject,ARMOR.DTExplode))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 ArmPerk",offsetof(CProtoObject,ARMOR.Perk))<0) BIND_ERROR;

//container
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 ContSize",offsetof(CProtoObject,CONTAINER.Size))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 ContCannotPickUp",offsetof(CProtoObject,CONTAINER.CannotPickUp))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 ContMagicHandsGrnd",offsetof(CProtoObject,CONTAINER.MagicHandsGrnd))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 ContChangeble",offsetof(CProtoObject,CONTAINER.Changeble))<0) BIND_ERROR;

//drug
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 Stat1",offsetof(CProtoObject,DRUG.Stat0))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 Stat2",offsetof(CProtoObject,DRUG.Stat1))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 Stat3",offsetof(CProtoObject,DRUG.Stat2))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 Amount1Stat1",offsetof(CProtoObject,DRUG.Amount0S0))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 Amount1Stat2",offsetof(CProtoObject,DRUG.Amount0S1))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 Amount1Stat3",offsetof(CProtoObject,DRUG.Amount0S2))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 Duration1",offsetof(CProtoObject,DRUG.Duration1))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 Amount2Stat1",offsetof(CProtoObject,DRUG.Amount1S0))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 Amount2Stat2",offsetof(CProtoObject,DRUG.Amount1S1))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 Amount2Stat3",offsetof(CProtoObject,DRUG.Amount1S2))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 Duration2",offsetof(CProtoObject,DRUG.Duration2))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 Amount3Stat1",offsetof(CProtoObject,DRUG.Amount2S0))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 Amount3Stat2",offsetof(CProtoObject,DRUG.Amount2S1))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 Amount3Stat3",offsetof(CProtoObject,DRUG.Amount2S2))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 AddictionRate",offsetof(CProtoObject,DRUG.Addiction))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 WEffect",offsetof(CProtoObject,DRUG.WEffectPerk))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 WOnset",offsetof(CProtoObject,DRUG.WOnset))<0) BIND_ERROR;

//weapon
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 WeapAnim",offsetof(CProtoObject,WEAPON.Anim1))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int16 TimeActiv",offsetof(CProtoObject,WEAPON.OffsTimeActiv))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int16 TimeUnactiv",offsetof(CProtoObject,WEAPON.OffsTimeUnactiv))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 VolumeHolder",offsetof(CProtoObject,WEAPON.VolHolder))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 Caliber",offsetof(CProtoObject,WEAPON.Caliber))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 VolumeHolderExt",offsetof(CProtoObject,WEAPON.VolHolderExt))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 CaliberExt",offsetof(CProtoObject,WEAPON.CaliberExt))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 CriticalFailture",offsetof(CProtoObject,WEAPON.CrFailture))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DefAmmo",offsetof(CProtoObject,WEAPON.DefAmmo))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 DefAmmoExt",offsetof(CProtoObject,WEAPON.DefAmmoExt))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 CountAttack",offsetof(CProtoObject,WEAPON.CountAttack))<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("ProtoObject","const uint8 paSkill",offsetof(CProtoObject,WEAPON.aSkill[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 paDmgType",offsetof(CProtoObject,WEAPON.aDmgType[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 paNumHolder",offsetof(CProtoObject,WEAPON.aHolder[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 paTextPic",offsetof(CProtoObject,WEAPON.aPic[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 paDmgMin",offsetof(CProtoObject,WEAPON.aDmgMin[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 paDmgMax",offsetof(CProtoObject,WEAPON.aDmgMax[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 paMaxDist",offsetof(CProtoObject,WEAPON.aMaxDist[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 paEffect",offsetof(CProtoObject,WEAPON.aEffect[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 paAnim",offsetof(CProtoObject,WEAPON.aAnim2[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 paTime",offsetof(CProtoObject,WEAPON.aTime[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 paAim",offsetof(CProtoObject,WEAPON.aAim[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 paRound",offsetof(CProtoObject,WEAPON.aRound[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 paRemoveWeap",offsetof(CProtoObject,WEAPON.aRemove[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 paSound",offsetof(CProtoObject,WEAPON.aSound[USE_PRIMARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 saSkill",offsetof(CProtoObject,WEAPON.aSkill[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 saDmgType",offsetof(CProtoObject,WEAPON.aDmgType[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 saNumHolder",offsetof(CProtoObject,WEAPON.aHolder[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 saTextPic",offsetof(CProtoObject,WEAPON.aPic[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 saDmgMin",offsetof(CProtoObject,WEAPON.aDmgMin[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 saDmgMax",offsetof(CProtoObject,WEAPON.aDmgMax[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 saMaxDist",offsetof(CProtoObject,WEAPON.aMaxDist[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 saEffect",offsetof(CProtoObject,WEAPON.aEffect[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 saAnim",offsetof(CProtoObject,WEAPON.aAnim2[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 saTime",offsetof(CProtoObject,WEAPON.aTime[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 saAim",offsetof(CProtoObject,WEAPON.aAim[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 saRound",offsetof(CProtoObject,WEAPON.aRound[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 saRemoveWeap",offsetof(CProtoObject,WEAPON.aRemove[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 saSound",offsetof(CProtoObject,WEAPON.aSound[USE_SECONDARY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 taSkill",offsetof(CProtoObject,WEAPON.aSkill[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 taDmgType",offsetof(CProtoObject,WEAPON.aDmgType[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 taNumHolder",offsetof(CProtoObject,WEAPON.aHolder[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 taTextPic",offsetof(CProtoObject,WEAPON.aPic[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 taDmgMin",offsetof(CProtoObject,WEAPON.aDmgMin[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 taDmgMax",offsetof(CProtoObject,WEAPON.aDmgMax[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 taMaxDist",offsetof(CProtoObject,WEAPON.aMaxDist[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 taEffect",offsetof(CProtoObject,WEAPON.aEffect[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 taAnim",offsetof(CProtoObject,WEAPON.aAnim2[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 taTime",offsetof(CProtoObject,WEAPON.aTime[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 taAim",offsetof(CProtoObject,WEAPON.aAim[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint16 taRound",offsetof(CProtoObject,WEAPON.aRound[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 taRemoveWeap",offsetof(CProtoObject,WEAPON.aRemove[USE_THIRD]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint8 taSound",offsetof(CProtoObject,WEAPON.aSound[USE_THIRD]))<0) BIND_ERROR;

//ammo
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 StartCount",offsetof(CProtoObject,AMMO.StartCount))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 AmmoCaliber",offsetof(CProtoObject,AMMO.Caliber))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 AcMod",offsetof(CProtoObject,AMMO.ACMod))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const int32 DrMod",offsetof(CProtoObject,AMMO.DRMod))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 DmgMult",offsetof(CProtoObject,AMMO.DmgMult))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("ProtoObject","const uint32 DmgDiv",offsetof(CProtoObject,AMMO.DmgDiv))<0) BIND_ERROR;

//Методы
	if(engine->RegisterObjectMethod("ProtoObject","int32 GetType()",asMETHOD(CProtoObject,GetType),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("ProtoObject","uint16 GetProtoId()",asMETHOD(CProtoObject,GetPid),asCALL_THISCALL)<0) BIND_ERROR;


//OBJECT ***************************************************************************

	if(engine->RegisterObjectType("Object",sizeof(CObject),asOBJ_REF)<0)
	{
		WriteLog("RegisterObjectType Object FALSE\n");
		BIND_ERROR;
	}

	if(engine->RegisterObjectBehaviour("Object",asBEHAVE_ADDREF,"void f()",asMETHOD(CObject,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("Object",asBEHAVE_RELEASE,"void f()",asMETHOD(CObject,Release),asCALL_THISCALL)<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("Object","const uint32 Id",offsetof(CObject,Id))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","const ProtoObject@ Proto",offsetof(CObject,proto))<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("Object","const uint8 Accessory",offsetof(CObject,accessory))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","const uint16 Map",offsetof(CObject,ACC_HEX.Map))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","const uint16 HexX",offsetof(CObject,ACC_HEX.X))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","const uint16 HexY",offsetof(CObject,ACC_HEX.Y))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","const uint32 CritId",offsetof(CObject,ACC_CRITTER.Id))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","const uint8 CritSlot",offsetof(CObject,ACC_CRITTER.Slot))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","const uint32 ContId",offsetof(CObject,ACC_CONTAINER.Id))<0) BIND_ERROR;

//container
	if(engine->RegisterObjectProperty("Object","uint32 cDoorId",offsetof(CObject,CONTAINER.DoorId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint8 cIsOpened",offsetof(CObject,CONTAINER.IsOpen))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint16 cCondition",offsetof(CObject,CONTAINER.Condition))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint16 cCount",offsetof(CObject,CONTAINER.CurCount))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint16 cSize",offsetof(CObject,CONTAINER.CurSize))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint32 cWeight",offsetof(CObject,CONTAINER.CurWeight))<0) BIND_ERROR;

//weapon
	if(engine->RegisterObjectProperty("Object","uint16 wSpFlags",offsetof(CObject,WEAPON.SpFlags))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint16 wBrokenInfo",offsetof(CObject,WEAPON.BrokenInfo))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint16 wAmmoPid",offsetof(CObject,WEAPON.AmmoPid))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint16 wAmmoCount",offsetof(CObject,WEAPON.AmmoCount))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint16 wAmmoPidExt",offsetof(CObject,WEAPON.AmmoPidExt))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint16 wAmmoCountExt",offsetof(CObject,WEAPON.AmmoCountExt))<0) BIND_ERROR;

//ammo
	if(engine->RegisterObjectProperty("Object","uint16 aCount",offsetof(CObject,AMMO.Count))<0) BIND_ERROR;

//misc
	if(engine->RegisterObjectProperty("Object","uint16 mCount",offsetof(CObject,MISC.Count))<0) BIND_ERROR;

//Misc2
	if(engine->RegisterObjectProperty("Object","uint32 m2_Val1",offsetof(CObject,MISC2.Val1))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint32 m2_Val2",offsetof(CObject,MISC2.Val2))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint32 m2_Val3",offsetof(CObject,MISC2.Val3))<0) BIND_ERROR;

//Key
	if(engine->RegisterObjectProperty("Object","uint32 k_DoorId",offsetof(CObject,KEY.DoorId))<0) BIND_ERROR;

//door
	if(engine->RegisterObjectProperty("Object","uint32 dDoorId",offsetof(CObject,DOOR.DoorId))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint8 dIsOpened",offsetof(CObject,DOOR.IsOpen))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Object","uint16 dCondition",offsetof(CObject,DOOR.Condition))<0) BIND_ERROR;

//Методы
	if(engine->RegisterObjectMethod("Object","int32 GetType()",asMETHOD(CObject,GetType),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Object","uint16 GetProtoId()",asMETHOD(CObject,GetProtoId),asCALL_THISCALL)<0) BIND_ERROR;


//CRITTER **************************************************************************

	if(engine->RegisterObjectType("Critter",sizeof(CCritter),asOBJ_REF)<0)
	{
		WriteLog("RegisterObjectType Critter FALSE\n");
		BIND_ERROR;
	}

	if(engine->RegisterObjectBehaviour("Critter",asBEHAVE_ADDREF,"void f()",asMETHOD(CCritter,AddRef),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectBehaviour("Critter",asBEHAVE_RELEASE,"void f()",asMETHOD(CCritter,Release),asCALL_THISCALL)<0) BIND_ERROR;

	if(engine->RegisterObjectProperty("Critter","const uint32 Id",offsetof(CCritter,Info)+offsetof(CCritInfo,Id))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint8 BaseType",offsetof(CCritter,Info)+offsetof(CCritInfo,BaseType))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint16 HexX",offsetof(CCritter,Info)+offsetof(CCritInfo,HexX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint16 HexY",offsetof(CCritter,Info)+offsetof(CCritInfo,HexY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint16 WorldX",offsetof(CCritter,Info)+offsetof(CCritInfo,WorldX))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint16 WorldY",offsetof(CCritter,Info)+offsetof(CCritInfo,WorldY))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint8 Ori",offsetof(CCritter,Info)+offsetof(CCritInfo,Ori))<0) BIND_ERROR;
	//name, cases !!!!!
	if(engine->RegisterObjectProperty("Critter","const uint8 Cond",offsetof(CCritter,Info)+offsetof(CCritInfo,Cond))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint8 CondExt",offsetof(CCritter,Info)+offsetof(CCritInfo,CondExt))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","const uint16 Flags",offsetof(CCritter,Info)+offsetof(CCritInfo,Flags))<0) BIND_ERROR;

//Статы. Stats
	if(engine->RegisterObjectProperty("Critter","int32 Strenght",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_STRENGHT]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Perception",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_PERCEPTION]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Endurance",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_ENDURANCE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Charisma",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_CHARISMA]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Intellect",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_INTELLECT]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Agility",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_AGILITY]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Luck",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_LUCK]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 MaxHp",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_MAX_LIFE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 MaxCond",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_MAX_COND]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 ArmorClass",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_ARMOR_CLASS]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 MeleeDamage",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_MELEE_DAMAGE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 WeaponDamage",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_WEAPON_DAMAGE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 CarryWeight",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_CARRY_WEIGHT]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Sequence",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_SEQUENCE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 HealingRrate",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_HEALING_RATE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 CriticalChance",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_CRITICAL_CHANCE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 MaxCritical",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_MAX_CRITICAL]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 IngureAbsorb",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_INGURE_ABSORB]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 LaserAbsorb",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_LASER_ABSORB]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 FireAbsorb",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_FIRE_ABSORB]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 PlasmaAbsorb",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_PLASMA_ABSORB]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 ElectroAbsorb",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_ELECTRO_ABSORB]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 EmpAbsorb",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_EMP_ABSORB]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 BlastAbsorb",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_BLAST_ABSORB]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 IngureResist",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_INGURE_RESIST]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 LaserResist",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_LASER_RESIST]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 FireResist",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_FIRE_RESIST]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 PlasmaResist",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_PLASMA_RESIST]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 ElectroResist",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_ELECTRO_RESIST]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 EmpResist",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_EMP_RESIST]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 BlastResist",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_BLAST_RESIST]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 RadiationResistance",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_RADIATION_RESISTANCE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 PoisonResistance",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_POISON_RESISTANCE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Age",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_AGE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Gender",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_GENDER]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Hp",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_CURRENT_HP]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 PoisoningLevel",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_POISONING_LEVEL]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 RadiationLevel",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_RADIATION_LEVEL]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 CurrentStandart",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_CURRENT_STANDART]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Experience",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_EXPERIENCE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Level",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_LEVEL]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 UnspentSkillPoints",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_UNSPENT_SKILL_POINTS]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 UnspentPerks",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_UNSPENT_PERKS]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int32 Karma",offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_KARMA]))<0) BIND_ERROR;

	//Script Vars
	char name_var[32];
	for(int v=0;v<10;++v)
	{
		sprintf(name_var,"int32 Var%u",v);
		if(engine->RegisterObjectProperty("Critter",name_var,offsetof(CCritter,Info)+offsetof(CCritInfo,St[ST_SCRIPT_VAR0+v]))<0) BIND_ERROR;
	}

//Навыки. Skills
	if(engine->RegisterObjectProperty("Critter","int16 SmallGuns",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_SMALL_GUNS]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 BigGuns",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_BIG_GUNS]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 EnergyWeapons",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_ENERGY_WEAPONS]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Unarmed",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_UNARMED]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 MeleeWeapons",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_MELEE_WEAPONS]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Throwing",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_THROWING]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 FirstAid",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_FIRST_AID]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Doctor",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_DOCTOR]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Sneak",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_SNEAK]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Lockpick",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_LOCKPICK]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Steal",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_STEAL]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Traps",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_TRAPS]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Science",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_SCIENCE]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Repair",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_REPAIR]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Speech",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_SPEECH]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Barter",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_BARTER]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Gambling",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_GAMBLING]))<0) BIND_ERROR;
	if(engine->RegisterObjectProperty("Critter","int16 Outdoorsman",offsetof(CCritter,Info)+offsetof(CCritInfo,Sk[SK_OUTDOORSMAN]))<0) BIND_ERROR;

//Методы криттера
	if(engine->RegisterObjectMethod("Critter","uint32 GetMap()",asMETHOD(CCritter,GetMap),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint16 GetProtoMap()",asMETHOD(CCritter,GetProtoMap),asCALL_THISCALL)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","int32 IsFree()",asMETHOD(CCritter,IsFree),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int32 IsBusy()",asMETHOD(CCritter,IsBusy),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int32 Wait(uint32 ms)",asMETHOD(CCritter,SetBreakTime),asCALL_THISCALL)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","uint32 ItemsCount()",asMETHOD(CCritter,CountObj),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint32 ItemsWeight()",asMETHOD(CCritter,WeightObj),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint16 ItemsSize()",asMETHOD(CCritter,SizeObj),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","uint32 CountItem(uint16 pid)",asMETHOD(CCritter,CountObjByPid),asCALL_THISCALL)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","int32 CreateTimeEvent(int32 num, int8 month, int8 day, int8 hour, int8 minute, int32 loop_period,  string& module, string& func_name)",asMETHOD(CCritter,CreateTimeEvent),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int32 ChangeTimeEventA(int32 num, string& module, string& func_name)",asMETHOD(CCritter,ChangeTimeEventA),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int32 ChangeTimeEventB(int32 num, int8 month, int8 day, int8 hour, int8 minute)",asMETHOD(CCritter,ChangeTimeEventB),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int32 ChangeTimeEventC(int32 num, int32 loop_period)",asMETHOD(CCritter,ChangeTimeEventC),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int32 EraseTimeEvent(int32 num)",asMETHOD(CCritter,EraseTimeEvent),asCALL_THISCALL)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","void SendParam(uint8 type, uint16 num)",asMETHOD(CCritter,Send_Param),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SendMessage(int32 num, int32 to)",asMETHOD(CCritter,SendMessage),asCALL_THISCALL)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","int32 MoveRandom()",asFUNCTION(CServer::Crit_MoveRandom),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int32 MoveToDir(uint8 direction)",asFUNCTION(CServer::Crit_MoveToDir),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int32 MoveToHex(uint16 hex_x, uint16 hex_y, uint8 ori)",asFUNCTION(CServer::Crit_MoveToHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","Object& AddItem(uint16 pid, uint32 count)",asFUNCTION(CServer::CreateObjToCr),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","Object& AddItemHex(uint16 hx, uint16 hy, uint16 pid, uint32 count)",asFUNCTION(CServer::CreateObjToHex),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int32 DeleteItem(uint16 pid, uint32 count)",asFUNCTION(CServer::DeleteObjFromCr),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","void Say(uint8 how_say, string& text)",asFUNCTION(CServer::Crit_Say),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void SetDir(uint8 dir)",asFUNCTION(CServer::Crit_SetDir),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","void Attack(Critter& who, uint32 min_hp)",asFUNCTION(CServer::Crit_SetTarget),asCALL_CDECL_OBJFIRST)<0) BIND_ERROR;

	if(engine->RegisterObjectMethod("Critter","int32 IsKnownLoc(uint16 loc_pid)",asMETHOD(CCritter,CheckKnownLocByPid),asCALL_THISCALL)<0) BIND_ERROR;
	if(engine->RegisterObjectMethod("Critter","int32 SetKnownLoc(uint16 loc_pid)",asMETHOD(CCritter,AddKnownLocByPid),asCALL_THISCALL)<0) BIND_ERROR;

//GLOBAL *************************************************************************

	if(engine->RegisterGlobalProperty("const uint16 GameYear",&Game_Year)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const uint8 GameMonth",&Game_Month)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const uint8 GameDay",&Game_Day)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const uint8 GameHour",&Game_Hour)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const uint8 GameMinute",&Game_Minute)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("const uint8 GameTimeMultiplier",&Game_TimeMultiplier)) BIND_ERROR;
	if(engine->RegisterGlobalProperty("uint32 GameForceDialog",&Game_ForceDialog)) BIND_ERROR;

	if(engine->RegisterGlobalFunction("void Log(string &text)",asFUNCTION(Script::Log),asCALL_CDECL)<0) BIND_ERROR;
	if(engine->RegisterGlobalFunction("int Random(int minimum, int maximum)",asFUNCTION(Random),asCALL_CDECL)<0) BIND_ERROR;

//	if(engine->RegisterGlobalFunction("int FindTarget(Critter &inout, uint flags)",asFUNCTION(CServer::Npc_FindTarget),asCALL_CDECL)<0) BIND_ERROR;
//	if(engine->RegisterGlobalFunction("int IsSelfTarget(Critter &inout)",asFUNCTION(CServer::Crit_IsSelfTarget),asCALL_CDECL)<0) BIND_ERROR;

//	if(ss->RegisterGlobalFunction("void PushIntVar(Critter& crit, string& text)",asFUNCTION(CServer::Npc_PushIntVar),asCALL_CDECL)<0) return 0;

/*
	if(ss->RegisterGlobalFunction("int FindTarget(int crit, uint8 flags)",asMETHOD(CServer,Crit_FindTarget),asCALL_STDCALL)<0) return 0;
	if(ss->RegisterGlobalFunction("int FreeTarget(int crit)",asMETHOD(CServer,Crit_FreeTarget),asCALL_STDCALL)<0) return 0;
	if(ss->RegisterGlobalFunction("int IsTarget(int crit)",asMETHOD(CServer,Crit_IsTarget),asCALL_STDCALL)<0) return 0;
	if(ss->RegisterGlobalFunction("int IsSelfTarget(int crit)",asMETHOD(CServer,Crit_IsSelfTarget),asCALL_STDCALL)<0) return 0;
	if(ss->RegisterGlobalFunction("int MoveToTarget(int crit)",asMETHOD(CServer,Crit_MoveToTarget),asCALL_STDCALL)<0) return 0;
	if(ss->RegisterGlobalFunction("int PossibleToAttack(int crit)",asMETHOD(CServer,Crit_PossibleToAttack),asCALL_STDCALL)<0) return 0;
	if(ss->RegisterGlobalFunction("int AttackTarget(int crit)",asMETHOD(CServer,Crit_AttackTarget),asCALL_STDCALL)<0) return 0;
	if(ss->RegisterGlobalFunction("int IsBusy(int crit)",asMETHOD(CServer,Crit_IsBusy),asCALL_STDCALL)<0) return 0;
	if(ss->RegisterGlobalFunction("int Wait(int crit, int ms)",asMETHOD(CServer,Crit_Wait),asCALL_STDCALL)<0) return 0;
	if(ss->RegisterGlobalFunction("int MoveRandom(int crit)",asMETHOD(CServer,Crit_MoveRandom),asCALL_STDCALL)<0) return 0;
*/