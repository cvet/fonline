#ifndef __ITEM__
#define __ITEM__

#include "Common.h"
#include "FileManager.h"
#include "Text.h"
#include "Crypt.h"

class Critter;
class MapObject;

#define ITEM_EVENT_FINISH             (0)
#define ITEM_EVENT_ATTACK             (1)
#define ITEM_EVENT_USE                (2)
#define ITEM_EVENT_USE_ON_ME          (3)
#define ITEM_EVENT_SKILL              (4)
#define ITEM_EVENT_DROP               (5)
#define ITEM_EVENT_MOVE               (6)
#define ITEM_EVENT_WALK               (7)
#define ITEM_EVENT_MAX                (8)
extern const char* ItemEventFuncName[ITEM_EVENT_MAX];

// Prototypes
#define MAX_ITEM_PROTOTYPES         (12000)
#define PROTO_ITEM_DEFAULT_EXT      ".pro"
#define PROTO_ITEM_FILENAME         "proto.fopro_"

// F2 types
#define F2_TYPE_ITEMS               (0)
#define F2_TYPE_CRITTERS            (1)
#define F2_TYPE_SCENERY             (2)
#define F2_TYPE_WALLS               (3)
#define F2_TYPE_TILES               (4)
#define F2_TYPE_MISC                (5)
#define F2_ARMOR					(0)
#define F2_CONTAINER				(1)
#define F2_DRUG						(2)
#define F2_WEAPON					(3)
#define F2_AMMO						(4)
#define F2_MISCITEM					(5)
#define F2_KEY						(6)
#define F2_PORTAL					(0)
#define F2_STAIRS					(1)
#define F2_ELEVATOR					(2)
#define F2_LADDERBOTTOM				(3)
#define F2_LADDERTOP				(4)
#define F2_GENERICSCENERY			(5)
#define F2_EXITGRID					(0)
#define F2_GENERICMISC				(1)

// Types
#define ITEM_TYPE_RESERVED          (0)  // Disabled in LogIn
#define ITEM_TYPE_ARMOR             (1)  // item			weared
#define ITEM_TYPE_DRUG              (2)  // item grouped
#define ITEM_TYPE_WEAPON            (3)	 // item			weared
#define ITEM_TYPE_AMMO              (4)  // item grouped
#define ITEM_TYPE_MISC              (5)  // item grouped
#define ITEM_TYPE_MISC_EX           (6)  // item
#define ITEM_TYPE_KEY               (7)  // item
#define ITEM_TYPE_CONTAINER         (8)  // item
#define ITEM_TYPE_DOOR              (9)  // item
#define ITEM_TYPE_GRID              (10) // scenery
#define ITEM_TYPE_GENERIC           (11) // scenery
#define ITEM_TYPE_WALL              (12) // scenery
#define ITEM_MAX_TYPES              (13)

// Grid Types
#define GRID_EXITGRID               (1)
#define GRID_STAIRS                 (2)
#define GRID_LADDERBOT              (3)
#define GRID_LADDERTOP              (4)
#define GRID_ELEVATOR               (5)

// Accessory
#define ITEM_ACCESSORY_NONE         (0)
#define ITEM_ACCESSORY_CRITTER      (1)
#define ITEM_ACCESSORY_HEX          (2)
#define ITEM_ACCESSORY_CONTAINER    (3)

// Damage types
#define DAMAGE_TYPE_UNCALLED        (0)
#define DAMAGE_TYPE_NORMAL          (1)
#define DAMAGE_TYPE_LASER           (2)
#define DAMAGE_TYPE_FIRE            (3)
#define DAMAGE_TYPE_PLASMA          (4)
#define DAMAGE_TYPE_ELECTR          (5)
#define DAMAGE_TYPE_EMP             (6)
#define DAMAGE_TYPE_EXPLODE         (7)

// Uses
#define USE_PRIMARY                 (0)
#define USE_SECONDARY               (1)
#define USE_THIRD                   (2)
#define USE_RELOAD                  (3)
#define USE_USE                     (4)
#define MAX_USES					(3)
#define USE_NONE                    (15)

// Corner type
#define CORNER_NORTH_SOUTH          (0)
#define CORNER_WEST                 (1)
#define CORNER_EAST                 (2)
#define CORNER_SOUTH                (3)
#define CORNER_NORTH                (4)
#define CORNER_EAST_WEST            (5)

// Flags
#define ITEM_HIDDEN                 (0x00000001)
#define ITEM_FLAT                   (0x00000002)
#define ITEM_NO_BLOCK               (0x00000004)
#define ITEM_SHOOT_THRU             (0x00000008)
#define ITEM_LIGHT_THRU             (0x00000010)
#define ITEM_MULTI_HEX              (0x00000020)
#define ITEM_WALL_TRANS_END         (0x00000040)
#define ITEM_TWO_HANDS              (0x00000080)
#define ITEM_BIG_GUN                (0x00000100)
#define ITEM_ALWAYS_VIEW            (0x00000200)
#define ITEM_HAS_TIMER              (0x00000400)
#define ITEM_BAD_ITEM               (0x00000800)
#define ITEM_NO_HIGHLIGHT           (0x00001000)
#define ITEM_SHOW_ANIM              (0x00002000)
#define ITEM_SHOW_ANIM_EXT          (0x00004000)
#define ITEM_LIGHT                  (0x00008000)
#define ITEM_GECK                   (0x00010000)
#define ITEM_TRAP                   (0x00020000)
#define ITEM_NO_LIGHT_INFLUENCE     (0x00040000)
#define ITEM_NO_LOOT                (0x00080000)
#define ITEM_NO_STEAL               (0x00100000)
#define ITEM_GAG                    (0x00200000)
#define ITEM_COLORIZE               (0x00400000)
#define ITEM_COLORIZE_INV           (0x00800000)
#define ITEM_CAN_USE_ON_SMTH        (0x01000000)
#define ITEM_CAN_LOOK               (0x02000000)
#define ITEM_CAN_TALK               (0x04000000)
#define ITEM_CAN_PICKUP             (0x08000000)
#define ITEM_CAN_USE                (0x10000000)
#define ITEM_CACHED                 (0x80000000)

// Material
#define MATERIAL_GLASS				(0)
#define MATERIAL_METAL				(1)
#define MATERIAL_PLASTIC			(2)
#define MATERIAL_WOOD				(3)
#define MATERIAL_DIRT				(4)
#define MATERIAL_STONE				(5)
#define MATERIAL_CEMENT				(6)
#define MATERIAL_LEATHER			(7)

// Broken info
#define WEAR_MAX					(30000)
#define BROKEN_MAX					(10)
	// Flags
#define BI_LOWBROKEN				(0x01)
#define BI_NORMBROKEN				(0x02)
#define BI_HIGHBROKEN				(0x04)
#define BI_NOTRESC					(0x08)
#define BI_BROKEN					(0x0F)
#define BI_SERVICE					(0x10)
#define BI_SERVICE_EXT				(0x20)
#define BI_ETERNAL					(0x40)

// Offsets by original pids
#define F2PROTO_OFFSET_ITEM			(0)
#define F2PROTO_OFFSET_SCENERY		(2000)
#define F2PROTO_OFFSET_MISC			(4000)
#define F2PROTO_OFFSET_WALL			(5000)
#define F2PROTO_OFFSET_TILE			(8000)
#define MISC_MAX					(200)

// Car
#define CAR_MAX_BLOCKS              (80)
#define CAR_MAX_BAG_POSITION        (12)
#define CAR_MAX_BAGS                (3)

// Light flags
#define LIGHT_DISABLE_DIR(dir)      (1<<CLAMP(dir,0,5))
#define LIGHT_GLOBAL                (0x40)
#define LIGHT_INVERSE               (0x80)

// Item data masks
#define ITEM_DATA_MASK_CHOSEN       (0)
#define ITEM_DATA_MASK_CRITTER      (1)
#define ITEM_DATA_MASK_CRITTER_EXT  (2)
#define ITEM_DATA_MASK_CONTAINER    (3)
#define ITEM_DATA_MASK_MAP          (4)
#define ITEM_DATA_MASK_MAX          (5)

// Special item pids
#define SP_SCEN_LIGHT				(F2PROTO_OFFSET_SCENERY+141) // Light Source
#define SP_SCEN_LIGHT_STOP          (4592)
#define SP_SCEN_BLOCK				(F2PROTO_OFFSET_SCENERY+67) // Secret Blocking Hex
#define SP_SCEN_IBLOCK				(F2PROTO_OFFSET_SCENERY+344) // Block Hex Auto Inviso
#define SP_SCEN_TRIGGER				(3852)
#define SP_WALL_BLOCK_LIGHT			(F2PROTO_OFFSET_WALL+621) // Wall s.t. with light
#define SP_WALL_BLOCK				(F2PROTO_OFFSET_WALL+622) // Wall s.t.
#define SP_GRID_EXITGRID			(F2PROTO_OFFSET_SCENERY+49) // Exit Grid Map Marker
#define SP_GRID_ENTIRE				(3853)
#define SP_MISC_SCRBLOCK			(F2PROTO_OFFSET_MISC+12) // Scroll block
#define SP_MISC_GRID_MAP_BEG		(F2PROTO_OFFSET_MISC+16) // Grid to LocalMap begin
#define SP_MISC_GRID_MAP_END		(F2PROTO_OFFSET_MISC+23) // Grid to LocalMap end
#define SP_MISC_GRID_MAP(pid)		((pid)>=SP_MISC_GRID_MAP_BEG && (pid)<=SP_MISC_GRID_MAP_END)
#define SP_MISC_GRID_GM_BEG			(F2PROTO_OFFSET_MISC+31) // Grid to GlobalMap begin
#define SP_MISC_GRID_GM_END			(F2PROTO_OFFSET_MISC+46) // Grid to GlobalMap end
#define SP_MISC_GRID_GM(pid)		((pid)>=SP_MISC_GRID_GM_BEG && (pid)<=SP_MISC_GRID_GM_END)

// Slot protos offsets
// 1000 - 1100 protos reserved
#define SLOT_MAIN_PROTO_OFFSET	    (1000)
#define SLOT_EXT_PROTO_OFFSET	    (1030)
#define SLOT_ARMOR_PROTO_OFFSET	    (1060)

/************************************************************************/
/* ProtoItem                                                         */
/************************************************************************/

class ProtoItem
{
public:
	WORD Pid;
	BYTE Type;
	BYTE Slot;
	DWORD Flags;
	BYTE Corner;
	bool DisableEgg;
	short Dir;
	DWORD PicMapHash;
	DWORD PicInvHash;
	DWORD Weight;
	DWORD Volume;
	DWORD Cost;
	BYTE SoundId;
	BYTE Material;

	BYTE LightFlags;
	BYTE LightDistance;
	char LightIntensity;
	DWORD LightColor;

	WORD AnimWaitBase;
	WORD AnimWaitRndMin;
	WORD AnimWaitRndMax;
	BYTE AnimStay[2];
	BYTE AnimShow[2];
	BYTE AnimHide[2];
	char DrawPosOffsY;

	union
	{
		struct
		{
			BYTE Anim0Male;
			BYTE Anim0Female;
			WORD AC;
			BYTE Perk;

			WORD DRNormal;
			WORD DRLaser;
			WORD DRFire;
			WORD DRPlasma;
			WORD DRElectr;
			WORD DREmp;
			WORD DRExplode;

			WORD DTNormal;
			WORD DTLaser;
			WORD DTFire;
			WORD DTPlasma;
			WORD DTElectr;
			WORD DTEmp;
			WORD DTExplode;
		} Armor;

		struct
		{
			bool NoWear;
			bool IsNeedAct;

			bool IsUnarmed;
			BYTE UnarmedTree;
			BYTE UnarmedPriority;
			BYTE UnarmedCriticalBonus;
			bool UnarmedArmorPiercing;
			BYTE UnarmedMinAgility;
			BYTE UnarmedMinUnarmed;
			BYTE UnarmedMinLevel;

			BYTE Anim1;
			WORD VolHolder;
			WORD Caliber;
			WORD DefAmmo;
			BYTE ReloadAp;
			BYTE CrFailture;
			BYTE MinSt;
			BYTE Perk;

			BYTE CountAttack;
			BYTE Skill[MAX_USES];
			BYTE DmgType[MAX_USES];
			BYTE Anim2[MAX_USES];
			WORD PicDeprecated[MAX_USES];
			DWORD PicHash[MAX_USES];
			WORD DmgMin[MAX_USES];
			WORD DmgMax[MAX_USES];
			WORD MaxDist[MAX_USES];
			WORD Effect[MAX_USES];
			WORD Round[MAX_USES];
			DWORD Time[MAX_USES];
			bool Aim[MAX_USES];
			bool Remove[MAX_USES];
			BYTE SoundId[MAX_USES];

			BYTE Weapon_CurrentUse;
			WORD Weapon_MaxDist;
			WORD Weapon_DmgMin;
			WORD Weapon_DmgMax;
			BYTE Weapon_Skill;
			BYTE Weapon_DmgType;
			BYTE Weapon_Anim2;
			BYTE Weapon_ApCost;
			BYTE Weapon_SoundId;
			bool Weapon_Remove;
			WORD Weapon_Round;
			WORD Weapon_Effect;
			bool Weapon_Aim;
		} Weapon;

		struct
		{
			DWORD StartCount;
			DWORD Caliber;
			int ACMod;
			int DRMod;
			DWORD DmgMult;
			DWORD DmgDiv;
		} Ammo;

		struct
		{
			DWORD StartVal1;
			DWORD StartVal2;
			DWORD StartVal3;

			bool IsCar;

			struct _CAR
			{
				BYTE Speed; 
				BYTE Negotiability;
				BYTE WearConsumption;
				BYTE CritCapacity;
				WORD FuelTank;
				WORD RunToBreak;
				BYTE Entire;
				BYTE WalkType;
				BYTE FuelConsumption;

				BYTE Bag0[CAR_MAX_BAG_POSITION/2]; // 6
				BYTE Bag1[CAR_MAX_BAG_POSITION/2]; // 6
				BYTE Blocks[CAR_MAX_BLOCKS/2]; // 40

				BYTE GetBag0Dir(int num){return ((num&1)?(Bag0[num/2]&0xF):(Bag0[num/2]>>4));}
				BYTE GetBag1Dir(int num){return ((num&1)?(Bag1[num/2]&0xF):(Bag1[num/2]>>4));}
				BYTE GetBlockDir(int num){return ((num&1)?(Blocks[num/2]&0xF):(Blocks[num/2]>>4));}
			} Car;
		} MiscEx;

		struct
		{
			DWORD ContVolume;
			DWORD CannotPickUp;
			DWORD MagicHandsGrnd;
			DWORD Changeble;
			bool IsNoOpen;
		} Container;
		
		struct
		{
			DWORD WalkThru;
			bool IsNoOpen;
			bool NoBlockMove;
			bool NoBlockShoot;
			bool NoBlockLight;
		} Door;

		struct
		{
			BYTE Type;
		} Grid;
	};

	void AddRef(){}
	void Release(){}

	void Clear(){ZeroMemory(this,sizeof(ProtoItem));}
	bool IsInit(){return Pid!=0;}
	WORD GetPid(){return Pid;}
	DWORD GetInfo(){return Pid*100;}
	DWORD GetHash(){return Crypt.Crc32((BYTE*)this,sizeof(ProtoItem));}
	void SetPid(WORD val){Pid=val;}
	void SetType(BYTE val){Type=val;}
	BYTE GetType(){return Type;}

	bool IsItem(){return Type>=ITEM_TYPE_ARMOR && Type<=ITEM_TYPE_DOOR;}
	bool IsScen(){return Type==ITEM_TYPE_GENERIC;}
	bool IsWall(){return Type==ITEM_TYPE_WALL;}
	bool IsArmor(){return Type==ITEM_TYPE_ARMOR;}
	bool IsDrug(){return Type==ITEM_TYPE_DRUG;}
	bool IsWeapon(){return Type==ITEM_TYPE_WEAPON;}
	bool IsAmmo(){return Type==ITEM_TYPE_AMMO;}
	bool IsMisc(){return Type==ITEM_TYPE_MISC;}
	bool IsMisc2(){return Type==ITEM_TYPE_MISC_EX;}
	bool IsKey(){return Type==ITEM_TYPE_KEY;}
	bool IsContainer(){return Type==ITEM_TYPE_CONTAINER;}
	bool IsDoor(){return Type==ITEM_TYPE_DOOR;}
	bool IsGrid(){return Type==ITEM_TYPE_GRID;}
	bool IsGeneric(){return Type==ITEM_TYPE_GENERIC;}
	bool IsCar(){return Type==ITEM_TYPE_MISC_EX && MiscEx.IsCar;}

	bool LockerIsChangeble(){if(IsDoor()) return true; if(IsContainer()) return Container.Changeble!=0; return false;}
	bool IsCanPickUp(){return FLAG(Flags,ITEM_CAN_PICKUP);}
	bool IsGrouped(){return IsDrug() || IsAmmo() || IsMisc() || (IsWeapon() && WeapIsGrouped());}
	bool IsWeared(){return Type==ITEM_TYPE_ARMOR || (Type==ITEM_TYPE_WEAPON && WeapIsWeared());}

	bool WeapIsNeedAct(){return Weapon.IsNeedAct;}
	bool WeapIsWeared(){return !Weapon.NoWear;}
	bool WeapIsGrouped(){return Weapon.NoWear;}

	bool operator==(const WORD& _r){return (Pid==_r);}
	ProtoItem(){Clear();}

	void Weapon_SetUse(BYTE use)
	{
		if(use>=MAX_USES) use=USE_PRIMARY;
		Weapon.Weapon_CurrentUse=use;
		Weapon.Weapon_Skill=Weapon.Skill[use];
		Weapon.Weapon_DmgType=Weapon.DmgType[use];
		Weapon.Weapon_Anim2=Weapon.Anim2[use];
		Weapon.Weapon_DmgMin=Weapon.DmgMin[use];
		Weapon.Weapon_DmgMax=Weapon.DmgMax[use];
		Weapon.Weapon_MaxDist=Weapon.MaxDist[use];
		Weapon.Weapon_Effect=Weapon.Effect[use];
		Weapon.Weapon_Round=Weapon.Round[use];
		Weapon.Weapon_ApCost=Weapon.Time[use];
		Weapon.Weapon_SoundId=Weapon.SoundId[use];
		Weapon.Weapon_Remove=Weapon.Remove[use];
		Weapon.Weapon_Aim=Weapon.Aim[use];
	}

	bool Container_IsGroundLevel()
	{
		bool is_ground=true;
		if(IsContainer()) is_ground=(Container.MagicHandsGrnd?true:false);
		else if(IsDoor() || IsCar() || IsScen() || IsGrid() || !IsCanPickUp()) is_ground=false;
		return is_ground;
	}

#ifdef FONLINE_OBJECT_EDITOR
	string ScriptModule;
	string ScriptFunc;
	string PicMapStr;
	string PicInvStr;
	string WeaponPicStr[MAX_USES];
#endif
};

typedef vector<ProtoItem> ProtoItemVec;
typedef vector<ProtoItem>::iterator ProtoItemVecIt;

class Item;
typedef map<DWORD,Item*,less<DWORD>> ItemPtrMap;
typedef map<DWORD,Item*,less<DWORD>>::iterator ItemPtrMapIt;
typedef map<DWORD,Item*,less<DWORD>>::value_type ItemPtrMapVal;
typedef vector<Item*> ItemPtrVec;
typedef vector<Item*>::iterator ItemPtrVecIt;
typedef vector<Item*>::value_type ItemPtrVecVal;
typedef vector<Item> ItemVec;
typedef vector<Item>::iterator ItemVecIt;

/************************************************************************/
/* Item                                                              */
/************************************************************************/

class Item
{
public:
	DWORD Id;
	ProtoItem* Proto;
	int From;
	BYTE Accessory;
	bool ViewPlaceOnMap;
	short Reserved0;

	union // 8
	{
		struct
		{
			DWORD MapId;
			WORD HexX;
			WORD HexY;
		} ACC_HEX;

		struct
		{
			DWORD Id;
			BYTE Slot;
		} ACC_CRITTER;

		struct
		{
			DWORD ContainerId;
			DWORD SpecialId;
		} ACC_CONTAINER;

		struct
		{
			DWORD Buffer[2];
		} ACC_BUFFER;
	};

	struct ItemData // 92, NetProto.h
	{
		static char SendMask[ITEM_DATA_MASK_MAX][92];

		WORD SortValue;
		BYTE Info;
		BYTE Reserved0;
		DWORD PicMapHash;
		DWORD PicInvHash;
		WORD AnimWaitBase;
		BYTE AnimStay[2];
		BYTE AnimShow[2];
		BYTE AnimHide[2];
		DWORD Flags;
		BYTE Rate;
		char LightIntensity;
		BYTE LightDistance;
		BYTE LightFlags;
		DWORD LightColor;
		WORD ScriptId;
		short TrapValue;
		DWORD Count;
		DWORD Cost;
		int ScriptValues[10];

		union // 8
		{
			struct
			{
				BYTE DeteorationFlags;
				BYTE DeteorationCount;
				WORD DeteorationValue;
				WORD AmmoPid;
				WORD AmmoCount;
			} TechInfo;

			struct
			{
				DWORD DoorId;
				WORD Condition;
				WORD Complexity;
			} Locker;

			struct
			{
				DWORD DoorId;
				WORD Fuel;
				WORD Deteoration;
			} Car;

			struct
			{
				DWORD Number;
			} Holodisk;

			struct
			{
				WORD Channel;
			} Radio;
		};
	} Data;

	short RefCounter;
	bool IsNotValid;
	bool Reserved1;

#ifdef FONLINE_SERVER
	int FuncId[ITEM_EVENT_MAX];
	Critter* ViewByCritter;
	ItemPtrVec* ChildItems;
	char* PLexems;
#endif
#ifdef FONLINE_CLIENT
	DWORD Dummy[3];
	string Lexems;
	int LexemsRefCounter;
#endif

	void AddRef(){RefCounter++;}
	void Release(){RefCounter--; if(RefCounter<=0) delete this;}

#ifdef FONLINE_SERVER
	void FullClear();

	bool ParseScript(const char* script, bool first_time);
	bool PrepareScriptFunc(int num_scr_func);
	void EventFinish(bool deleted);
	bool EventAttack(Critter* cr, Critter* target);
	bool EventUse(Critter* cr, Critter* on_critter, Item* on_item, MapObject* on_scenery);
	bool EventUseOnMe(Critter* cr, Item* used_item);
	bool EventSkill(Critter* cr, int skill);
	void EventDrop(Critter* cr);
	void EventMove(Critter* cr, BYTE from_slot);
	void EventWalk(Critter* cr, bool entered, BYTE dir);
#endif // FONLINE_SERVER

	void Init(ProtoItem* proto);
	void Clear();
	Item* Clone();
	void SetSortValue(ItemPtrVec& items);
	static void SortItems(ItemVec& items);

	// All
	DWORD GetId(){return Id;}
	WORD GetProtoId(){return Proto->GetPid();}
	DWORD GetInfo(){return Proto->GetInfo()+Data.Info;}
	DWORD GetPicMap(){return Data.PicMapHash?Data.PicMapHash:Proto->PicMapHash;}
	DWORD GetPicInv(){return Data.PicInvHash?Data.PicInvHash:Proto->PicInvHash;}
	bool IsValidAccessory(){return Accessory==ITEM_ACCESSORY_CRITTER || Accessory==ITEM_ACCESSORY_HEX || Accessory==ITEM_ACCESSORY_CONTAINER;}

	DWORD GetCount();
	void Count_Set(DWORD val);
	void Count_Add(DWORD val);
	void Count_Sub(DWORD val);

	BYTE GetType(){return Proto->GetType();}
	void SetRate(BYTE rate);
	WORD GetSortValue(){return Data.SortValue;}
	bool IsGrouped(){return Proto->IsGrouped();}

	bool IsPassed(){return FLAG(Data.Flags,ITEM_NO_BLOCK) && FLAG(Data.Flags,ITEM_SHOOT_THRU);}
	bool IsRaked(){return FLAG(Data.Flags,ITEM_SHOOT_THRU);}
	bool IsFlat(){return FLAG(Data.Flags,ITEM_FLAT);}
	bool IsHidden(){return FLAG(Data.Flags,ITEM_HIDDEN);}
	bool IsCanPickUp(){return FLAG(Data.Flags,ITEM_CAN_PICKUP);}
	bool IsCanTalk(){return FLAG(Data.Flags,ITEM_CAN_TALK);}
	bool IsCanUse(){return FLAG(Data.Flags,ITEM_CAN_USE);}
	bool IsCanUseOnSmth(){return FLAG(Data.Flags,ITEM_CAN_USE_ON_SMTH);}
	bool IsHasTimer(){return FLAG(Data.Flags,ITEM_HAS_TIMER);}
	bool IsBadItem(){return FLAG(Data.Flags,ITEM_BAD_ITEM);}
	bool IsTwoHands(){return FLAG(Data.Flags,ITEM_TWO_HANDS);}
	bool IsBigGun(){return FLAG(Data.Flags,ITEM_BIG_GUN);}
	bool IsNoHighlight(){return FLAG(Data.Flags,ITEM_NO_HIGHLIGHT);}
	bool IsShowAnim(){return FLAG(Data.Flags,ITEM_SHOW_ANIM);}
	bool IsShowAnimExt(){return FLAG(Data.Flags,ITEM_SHOW_ANIM_EXT);}
	bool IsLightThru(){return FLAG(Data.Flags,ITEM_LIGHT_THRU);}
	bool IsAlwaysView(){return FLAG(Data.Flags,ITEM_ALWAYS_VIEW);}
	bool IsGeck(){return FLAG(Data.Flags,ITEM_GECK);}
	bool IsNoLightInfluence(){return FLAG(Data.Flags,ITEM_NO_LIGHT_INFLUENCE);}
	bool IsNoLoot(){return FLAG(Data.Flags,ITEM_NO_LOOT);}
	bool IsNoSteal(){return FLAG(Data.Flags,ITEM_NO_STEAL);}
	bool IsGag(){return FLAG(Data.Flags,ITEM_GAG);}

	DWORD GetVolume(){return GetCount()*Proto->Volume;}
	DWORD GetVolume1st(){return Proto->Volume;}
	DWORD GetWeight(){return GetCount()*Proto->Weight;}
	DWORD GetWeight1st(){return Proto->Weight;}
	DWORD GetCost(){return GetCount()*GetCost1st();}
	DWORD GetCost1st();
	//DWORD GetCost1st(){return Data.Cost?Data.Cost:Proto->Cost;}

#ifdef FONLINE_SERVER
	void SetLexems(const char* lexems);
#endif

/************************************************************************/
/* WEAR                                                                 */
/************************************************************************/

#ifdef FONLINE_SERVER
	bool Wear(int count);
	void Repair();
#endif

	bool IsWeared()     {return GetId() && Proto->IsWeared();}
	bool IsBroken()     {return (IsWeared()?FLAG(Data.TechInfo.DeteorationFlags,BI_BROKEN):false);}
	bool IsNoResc()     {return (IsWeared()?FLAG(Data.TechInfo.DeteorationFlags,BI_NOTRESC):false);}
	bool IsService()    {return (IsWeared()?FLAG(Data.TechInfo.DeteorationFlags,BI_SERVICE):false);}
	bool IsServiceExt() {return (IsWeared()?FLAG(Data.TechInfo.DeteorationFlags,BI_SERVICE_EXT):false);}
	bool IsEternal()    {return (IsWeared()?FLAG(Data.TechInfo.DeteorationFlags,BI_ETERNAL):false);}
	int GetBrokenCount(){return (IsWeared()?Data.TechInfo.DeteorationCount:0);}
	int GetWear()       {return (IsWeared()?Data.TechInfo.DeteorationValue:0);}
	int GetMaxWear()    {return WEAR_MAX;}
	int GetWearProc()   {int val=GetWear()*100/WEAR_MAX; return CLAMP(val,0,100);}
	void SetWear(WORD wear){if(IsWeared()) Data.TechInfo.DeteorationValue=wear;}

/************************************************************************/
/* TYPE METHODS                                                         */
/************************************************************************/

	// Armor
	bool IsArmor(){return GetType()==ITEM_TYPE_ARMOR;}

	// Weapon
	bool IsWeapon(){return GetType()==ITEM_TYPE_WEAPON;}
	bool WeapIsEmpty(){return !Data.TechInfo.AmmoCount;}
	bool WeapIsFull(){return Data.TechInfo.AmmoCount>=Proto->Weapon.VolHolder;}
	DWORD WeapGetAmmoCount(){return Data.TechInfo.AmmoCount;}
	DWORD WeapGetAmmoPid(){return Data.TechInfo.AmmoPid;}
	DWORD WeapGetMaxAmmoCount(){return Proto->Weapon.VolHolder;}
	int WeapGetAmmoCaliber(){return Proto->Weapon.Caliber;}
	bool WeapIsWeared(){return Proto->WeapIsWeared();}
	bool WeapIsGrouped(){return Proto->WeapIsGrouped();}
	bool WeapIsEffect(int use){return Proto->Weapon.Effect[use]!=0;}
	WORD WeapGetEffectPid(int use){return Proto->Weapon.Effect[use];}
	int WeapGetNeedStrength(){return Proto->Weapon.MinSt;}
	bool WeapIsUseAviable(int use){BYTE ca=Proto->Weapon.CountAttack;if(use==USE_PRIMARY)return FLAG(ca,1);if(use==USE_SECONDARY)return FLAG(ca,2);if(use==USE_THIRD)return FLAG(ca,4);return false;}
	bool WeapIsCanAim(int use){return use<MAX_USES && Proto->Weapon.Aim[use];}
	void WeapRefreshProtoUse();
	bool WeapIsHtHAttack(int use);
	bool WeapIsGunAttack(int use);
	bool WeapIsRangedAttack(int use);
	void WeapLoadHolder();
	bool WeapIsFastReload(){return Proto->Weapon.Perk==WEAPON_PERK_FAST_RELOAD;}

	// Container
	bool IsContainer(){return Proto->IsContainer();}
	bool ContIsCannotPickUp(){return Proto->Container.CannotPickUp!=0;}
	bool ContIsMagicHandsGrnd(){return Proto->Container.MagicHandsGrnd!=0;}
	bool ContIsChangeble(){return Proto->Container.Changeble!=0;}
#ifdef FONLINE_SERVER
	void ContAddItem(Item*& item, DWORD special_id);
	void ContSetItem(Item* item);
	void ContEraseItem(Item* item);
	void ContErsAllItems();
	Item* ContGetItem(DWORD item_id, bool skip_hide);
	void ContGetAllItems(ItemPtrVec& items, bool skip_hide);
	Item* ContGetItemByPid(WORD pid, DWORD special_id);
	void ContGetItems(ItemPtrVec& items, DWORD special_id);
	int ContGetFreeVolume(DWORD special_id);
#endif

	// Door
	bool IsDoor(){return GetType()==ITEM_TYPE_DOOR;}

	// Locker
	bool IsHasLocker(){return IsDoor() || IsContainer();}
	DWORD LockerDoorId(){return Data.Locker.DoorId;}
	bool LockerIsOpen(){return FLAG(Data.Locker.Condition,LOCKER_ISOPEN);}
	bool LockerIsClose(){return !LockerIsOpen();}
	bool LockerIsChangeble(){return Proto->LockerIsChangeble();}
	int LockerComplexity(){return Data.Locker.Complexity;}

	// Ammo
	bool IsAmmo(){return Proto->IsAmmo();}
	int AmmoGetCaliber(){return Proto->Ammo.Caliber;}

	// Key
	bool IsKey(){return Proto->IsKey();}
	DWORD KeyDoorId(){return Data.Locker.DoorId;}

	// Drug
	bool IsDrug(){return Proto->IsDrug();}

	// Misc
	bool IsMisc(){return Proto->IsMisc();}

	// Colorize
	bool IsColorize(){return FLAG(Data.Flags,ITEM_COLORIZE);}
	DWORD GetColor(){return (Data.LightColor?Data.LightColor:Proto->LightColor)&0xFFFFFF;}
	BYTE GetAlpha(){return (Data.LightColor?Data.LightColor:Proto->LightColor)>>24;}
	DWORD GetInvColor(){return FLAG(Data.Flags,ITEM_COLORIZE_INV)?(Data.LightColor?Data.LightColor:Proto->LightColor):0;}

	// Light
	bool IsLight(){return FLAG(Data.Flags,ITEM_LIGHT);}
	DWORD LightGetHash(){if(!IsLight()) return 0; if(Data.LightIntensity) return Crypt.Crc32((BYTE*)&Data.LightIntensity,7)+FLAG(Data.Flags,ITEM_LIGHT); return (DWORD)Proto;}
	int LightGetIntensity(){return Data.LightIntensity?Data.LightIntensity:Proto->LightIntensity;}
	int LightGetDistance(){return Data.LightDistance?Data.LightDistance:Proto->LightDistance;}
	int LightGetFlags(){return Data.LightFlags?Data.LightFlags:Proto->LightFlags;}
	DWORD LightGetColor(){return (Data.LightColor?Data.LightColor:Proto->LightColor)&0xFFFFFF;}

/************************************************************************/
/* MiscEx SUBTYPES METHODS                                               */
/************************************************************************/

	// Radio
	bool IsRadio(){return GetProtoId()==PID_RADIO;}
	WORD RadioGetChannel(){return Data.Radio.Channel;}
	void RadioSetChannel(WORD chan){Data.Radio.Channel=chan;}

	// Car
	bool IsCar(){return Proto->IsCar();}
	DWORD CarGetDoorId(){return Data.Car.DoorId;}
	WORD CarGetFuel(){return Data.Car.Fuel;}
	WORD CarGetFuelTank(){return Proto->MiscEx.Car.FuelTank;}
	BYTE CarGetFuelConsumption(){return Proto->MiscEx.Car.FuelConsumption;}
	WORD CarGetWear(){return Data.Car.Deteoration;}
	WORD CarGetRunToBreak(){return Proto->MiscEx.Car.RunToBreak;}
	BYTE CarGetWearConsumption(){return Proto->MiscEx.Car.WearConsumption;}
	DWORD CarGetSpeed(){return Proto->MiscEx.Car.Speed;}
	BYTE CarGetCritCapacity(){return Proto->MiscEx.Car.CritCapacity;}

#ifdef FONLINE_SERVER
	Item* CarGetBag(int num_bag);
#endif

	// Holodisk
	bool IsHolodisk(){return GetProtoId()==PID_HOLODISK;}
	DWORD HolodiskGetNum(){return Data.Holodisk.Number;}
	void HolodiskSetNum(DWORD num){Data.Holodisk.Number=num;}

	// Trap
	bool IsTrap(){return FLAG(Data.Flags,ITEM_TRAP);}
	void TrapSetValue(int val){Data.TrapValue=val;}
	int TrapGetValue(){return Data.TrapValue;}

/************************************************************************/
/*                                                                      */
/************************************************************************/

	bool operator==(const DWORD& _id){return (Id==_id);}

#ifdef FONLINE_SERVER
	Item(){ZeroMemory(this,sizeof(Item)); RefCounter=1; IsNotValid=false; MEMORY_PROCESS(MEMORY_ITEM,sizeof(Item));}
	~Item(){Proto=NULL; if(PLexems) MEMORY_PROCESS(MEMORY_ITEM,-LEXEMS_SIZE); SAFEDELA(PLexems); MEMORY_PROCESS(MEMORY_ITEM,-(int)sizeof(Item));}
#elif FONLINE_CLIENT
	Item(){RefCounter=1;IsNotValid=false;}
	virtual ~Item(){Proto=NULL;}
#endif
};

/************************************************************************/
/*                                                                      */
/************************************************************************/

#endif // __ITEM__

