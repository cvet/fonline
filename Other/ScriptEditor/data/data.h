#include <windows.h>
#include <stdio.h>
#include <string>
using namespace std;

#include "FOdefines.h"

#define int8 char
#define uint8 unsigned char
#define int16 short
#define uint16 unsigned short
#define int32 long
#define uint32 unsigned long
#define uint unsigned int

#define CrID		DWORD
#define CrTYPE		BYTE

/************************************************************************/
/* Prototypes info                                                      */
/************************************************************************/

#define VERSION_PROTOTYPE_OBJECT	(4)
#define MAX_PROTOTYPES				(12000)
#define PROTO_OBJECT_SIZE			(176)
#define POBJ_DEFAULT_EXT			".pro"
#define POBJ_FILENAME				"proto.fopro"

//Item==Scenery
//but Item is Dynamic object
//Scenery is Static object

//subtype
const BYTE OBJ_SUBTYPE_NONE			=0;
const BYTE OBJ_SUBTYPE_ITEM			=1;
const BYTE OBJ_SUBTYPE_SCEN			=2;
const BYTE OBJ_SUBTYPE_WALL			=3;
const BYTE OBJ_SUBTYPE_TILE			=4;

//f2 type
const int F2_TYPE_ITEMS				=0;
const int F2_TYPE_CRITTERS			=1;
const int F2_TYPE_SCENERY			=2;
const int F2_TYPE_WALLS				=3;
const int F2_TYPE_TILES				=4;
const int F2_TYPE_MISC				=5;

#define F2_ARMOR					0
#define F2_CONTAINER				1
#define F2_DRUG						2
#define F2_WEAPON					3
#define F2_AMMO						4
#define F2_MISCITEM					5
#define F2_KEY						6

#define F2_PORTAL					0
#define F2_STAIRS					1
#define F2_ELEVATOR					2
#define F2_LADDERBOTTOM				3
#define F2_LADDERTOP				4
#define F2_GENERICSCENERY			5

#define F2_EXITGRID					0
#define F2_GENERICMISC				1

//type
const BYTE OBJ_TYPE_NONE			=0;

const BYTE OBJ_TYPE_ARMOR			=1; //item			weared
const BYTE OBJ_TYPE_DRUG			=2; //item grouped
const BYTE OBJ_TYPE_WEAPON			=3;	//item			weared
const BYTE OBJ_TYPE_AMMO			=4; //item grouped
const BYTE OBJ_TYPE_MISC			=5; //item grouped
const BYTE OBJ_TYPE_MISC2			=6; //item
const BYTE OBJ_TYPE_KEY				=7; //item
const BYTE OBJ_TYPE_CONTAINER		=8; //item
const BYTE OBJ_TYPE_DOOR			=9; //item

const BYTE OBJ_TYPE_GRID			=10; //scenery
const BYTE OBJ_TYPE_GENERIC			=11; //scenery

const BYTE OBJ_TYPE_WALL			=12; //wall

const BYTE OBJ_TYPE_TILE			=13; //tile

//Grid Types
const BYTE GRID_EXITGRID			=1;
const BYTE GRID_STAIRS				=2;
const BYTE GRID_LADDERBOT			=3;
const BYTE GRID_LADDERTOP			=4;
const BYTE GRID_ELEVATOR			=5;

//Принадлежность
const BYTE OBJ_ACCESSORY_CRIT		=1;
const BYTE OBJ_ACCESSORY_HEX		=2;
const BYTE OBJ_ACCESSORY_CONT		=3;

//Типы повреждений
const BYTE DAMAGE_TYPE_NONE			=0;
const BYTE DAMAGE_TYPE_NORMAL		=1;
const BYTE DAMAGE_TYPE_LASER		=2;
const BYTE DAMAGE_TYPE_FIRE			=3;
const BYTE DAMAGE_TYPE_PLASMA		=4;
const BYTE DAMAGE_TYPE_ELECTR		=5;
const BYTE DAMAGE_TYPE_EMP			=6;
const BYTE DAMAGE_TYPE_EXPLODE		=7;

//область применения итемов
const BYTE OBJ_USE_ON_CRITTER		=1;
const BYTE OBJ_USE_ON_ITEM			=2;
const BYTE OBJ_USE_ON_SCENERY		=3;

//Holder
const BYTE HOLDER_NONE				=0;
const BYTE HOLDER_MAIN				=1;
const BYTE HOLDER_EXT				=2;

//Uses
const int MAX_USES					=3;

const BYTE USE_PRIMARY				=0;
const BYTE USE_SECONDARY			=1;
const BYTE USE_THIRD				=2;

const BYTE USE_RELOAD_MAIN			=3;
const BYTE USE_RELOAD_EXT			=4;
const BYTE USE_LOAD_MAIN			=5;
const BYTE USE_LOAD_EXT				=6;
const BYTE USE_UNLOAD_MAIN			=7;
const BYTE USE_UNLOAD_EXT			=8;

const BYTE USE_USE					=10;
const BYTE USE_REPAIR				=11;

//Light type
#define LIGTH_NORTH_SOUTH			(0) // 0x00000000 - North/South
#define LIGTH_WEST					(1) // 0x80000000 - West Corner
#define LIGTH_EAST					(2) // 0x40000000 - East Corner
#define LIGTH_SOUTH					(3) // 0x20000000 - South Corner
#define LIGTH_NORTH					(4) // 0x10000000 - North Corner
#define LIGTH_EAST_WEST				(5) // 0x08000000 - East/West

//Action flags
#define FLAG_CAN_USE_ON_SMTH		(0x10) //	0x00001000 - Use On Smth (объект можно использовать на что-либо)
#define FLAG_CAN_LOOK				(0x20) //	0x00002000 - Look (объект можно осмотреть)
#define FLAG_CAN_TALK				(0x40) //	0x00004000 - Talk (с объектом можно поговорить)
#define FLAG_CAN_PICKUP				(0x80) //	0x00008000 - PickUp (объект можно поднять)
#define FLAG_CAN_USE				(0x08) //	0x00000800 - Use (объект можно использовать)

//Transparent
#define TRANS_NONE					(0)
#define TRANS_WALL					(1)
#define TRANS_GLASS					(2)
#define TRANS_STEAM					(3)
#define TRANS_ENERGY				(4)
#define TRANS_VALUE					(5)

//Material
#define MATERIAL_GLASS				(0)
#define MATERIAL_METAL				(1)
#define MATERIAL_PLASTIC			(2)
#define MATERIAL_WOOD				(3)
#define MATERIAL_DIRT				(4)
#define MATERIAL_STONE				(5)
#define MATERIAL_CEMENT				(6)
#define MATERIAL_LEATHER			(7)

/************************************************************************/
/* Broken info                                                          */
/************************************************************************/
//flags		wear_count	break_count
//12		34			5678

//Main
#define WEAR_MAX					(10000)
#define BROKEN_MAX					(20)

//Flags
#define BI_LOWBROKEN				BIN8(00000001) //Cash*80/100
#define BI_NORMBROKEN				BIN8(00000010) //Cash*65/100
#define BI_HIGHBROKEN				BIN8(00000100) //Cash*50/100
#define BI_NOTRESC					BIN8(00001000) //Cash*10/100
#define BI_BROKEN					BIN8(00001111)
#define BI_SERVICE					BIN8(00010000)
#define BI_SERVICE_EXT				BIN8(00100000)
#define BI_ETERNAL					BIN8(01000000)

//Service
//#define WEAR_SERVICE_SUCC(repair)	(repair>0?repair*30:10)
#define WEAR_SERVICE_FAIL			(Random(WEAR_MAX*10/100,WEAR_MAX*20/100)) //1000..2000

/************************************************************************/
/* Doors and Container conditions                                       */
/************************************************************************/

const WORD LOCKER_JAMMED			=BIN16(00000000,00000001);
const WORD LOCKER_BROKEN			=BIN16(00000000,00000010);

/************************************************************************/
/* Offsets by original pids                                             */
/************************************************************************/

#define F2PROTO_OFFSET_ITEM			(0)
#define F2PROTO_OFFSET_SCENERY		(2000)
#define F2PROTO_OFFSET_MISC			(4000)
#define F2PROTO_OFFSET_WALL			(5000)
#define F2PROTO_OFFSET_TILE			(8000)

#define MISC_MAX					(200)

/************************************************************************/
/* Car                                                                  */
/************************************************************************/

#define CAR_MAX_BLOCKS				(80)

/************************************************************************/
/*                                                                      */
/************************************************************************/

class CProtoObject //size 176 bytes
{
private:
	WORD Pid; //0x0
	BYTE Type; //0x2
	BYTE SubType; //0x3

public:
//Main
	short OffsTimeShow; //0x4
	short OffsTimeHide; //0x6

//Light
	BYTE Light; //0x8
	BYTE NoHighlight; //0x9
	BYTE LightType; //0xA
	BYTE DistanceLight; //0xB
	DWORD IntensityLight; //0xC

//Flags
	BYTE Hidden; //0x10
	BYTE Flat; //0x11
	BYTE NoBlock; //0x12
	BYTE MultiHex; //0x13
	BYTE ShootThru; //0x14
	BYTE LightThru; //0x15
	BYTE WallTransEnd; //0x16
	BYTE TwoHands; //0x17
	BYTE BigGun; //0x18

//Transparent
	BYTE TransType; //0x19
	BYTE TransRed; //0x1A
	BYTE TransVal; //0x1B

//Action Flags
	BYTE CanUse; //0x1C
	BYTE CanPickUp; //0x1D
	BYTE CanUseSmth; //0x1E
	BYTE CanLook; //0x1F
	BYTE CanTalk; //0x20

	BYTE Reserved0; //0x21
	BYTE Reserved1; //0x22
	BYTE Reserved2; //0x23

	DWORD Weight; //0x24
	BYTE Size; //0x28

	BYTE Reserved3; //0x29

	WORD PicMap; //0x2A
	WORD PicInv; //0x2C

	BYTE SoundId; //0x2E

	BYTE Reserved10; //0x2F

	DWORD Cost; //0x30
	BYTE Material; //0x34

	BYTE Reserved4; //0x35
	WORD Reserved5; //0x36
	DWORD Reserved6; //0x38
	DWORD Reserved7; //0x3C
	DWORD Reserved8; //0x40
	DWORD Reserved9; //0x44

	union //0x48, 96 bytes to do aviable
	{
//Items
		struct
		{
			BYTE Anim0Male; //{Индекс анимации дюда male}
			BYTE Anim0Female; //{Индекс анимации дюда female}

			WORD AC; //{Армор класс}

			DWORD TimeMove; //{Время хотьбы}
			DWORD TimeRun; //{Время бега}

			WORD DRNormal; //{Сопротивление нормальное}
			WORD DRLaser; //{Сопротивление лазеру}
			WORD DRFire; //{Сопротивление огню}
			WORD DRPlasma; //{Сопротивление плазме}
			WORD DRElectr; //{Сопротивление электричеству}
			WORD DREmp; //{Сопротивление емп}
			WORD DRExplode; //{Сопротивление взрыву}

			WORD DTNormal; //{Порог повреждения нормальное}
			WORD DTLaser; //{Порог повреждения лазеру}
			WORD DTFire; //{Порог повреждения огню}
			WORD DTPlasma; //{Порог повреждения плазме}
			WORD DTElectr; //{Порог повреждения электричеству}
			WORD DTEmp; //{Порог повреждения емп}
			WORD DTExplode; //{Порог повреждения взрыву}

			BYTE Perk; //{Перк на броне}
		} ARMOR;

		struct
		{
			DWORD Stat0; //{Стат персонажа, изменяющаяся сразу после использования наркотика}
			DWORD Stat1; //{Стат персонажа, изменяющаяся сразу после использования наркотика}
			DWORD Stat2; //{Стат персонажа, изменяющаяся сразу после использования наркотика}

			int Amount0S0; //{Модификатор для стата}
			int Amount0S1; //{Модификатор для стата}
			int Amount0S2; //{Модификатор для стата}

			DWORD Duration1; //{Задержка до повторного эффекта}
			int Amount1S0; //{Модификатор для стата}
			int Amount1S1; //{Модификатор для стата}
			int Amount1S2; //{Модификатор для стата}

			DWORD Duration2; //{Задержка до повторного эффекта}
			int Amount2S0; //{Модификатор для стата}
			int Amount2S1; //{Модификатор для стата}
			int Amount2S2; //{Модификатор для стата}

			DWORD Addiction; //{Процент привыкания}
			DWORD WEffectPerk; //{Перк за привыкание}
			DWORD WOnset; //{Начало привыкания}
		} DRUG;

		struct
		{
			short OffsTimeActiv; //{Время подготовки к использованию}
			short OffsTimeUnactiv; //{Время отмены использования}

			WORD VolHolder; //{Емкость основной обоймы}
			WORD VolHolderExt; //{Емкость дополнительной обоймы}
			WORD Caliber; //{Калибр у основной обоймы}
			WORD CaliberExt; //{Калибр у дополнительной обоймы}
			WORD DefAmmo;
			WORD DefAmmoExt;

			BYTE Anim1; //{Индекс анимации объекта}

			BYTE CrFailture; //{Критическая неудача при использовании}

			BYTE MinSt;
			BYTE MinIn;

			WORD Reserved3;

			BYTE Perk;

			BYTE CountAttack; //{Количество атак 0x1-PA, 0x2-SA, 0x4-TA}

			BYTE aSkill[MAX_USES]; //{Скилл отвечающий за использование}
			BYTE aDmgType[MAX_USES]; //{Тип атаки}
			BYTE aHolder[MAX_USES]; //{Используемая обойма}
			BYTE aAnim2[MAX_USES]; //{Анимация атаки}
			WORD aPic[MAX_USES]; //{Рисунок использования}
			WORD aDmgMin[MAX_USES]; //{Минимальное повреждения}
			WORD aDmgMax[MAX_USES]; //{Максимальное повреждение}
			WORD aMaxDist[MAX_USES]; //{Максимальная дистанция}
			WORD aEffect[MAX_USES]; //{Эффективная дистанция}
			WORD aRound[MAX_USES]; //{Расход патронов за атаку}
			DWORD aTime[MAX_USES]; //{Базовое время атаки}
			BYTE aAim[MAX_USES]; //{Наличие у атаки прицельного выстрела}
			BYTE aRemove[MAX_USES]; //{Удаление предмета после атаки}
			BYTE aSound[MAX_USES]; //{Звук при использовании}
			BYTE aReserved[MAX_USES];

			BYTE NoWear;

			BYTE Reserved0;
			WORD Reserved1;
			DWORD Reserved2;
//End of structure 176 bytes
		} WEAPON;

		struct
		{
			DWORD StartCount; //{Стартовое колличество}
			DWORD Caliber; //{Калибр}

			int ACMod; //{модификатор класса брони}
			int DRMod; //{модификатор DR}
			DWORD DmgMult; //{множитель для DR??????}
			DWORD DmgDiv; //{делитель для DR}
		} AMMO;

		struct
		{
			DWORD PowerPid;
			DWORD PowerCaliber;
			DWORD Charges;
		} MISC;

		struct
		{
			DWORD StartVal1;
			DWORD StartVal2;
			DWORD StartVal3;

			BYTE IsCar;
			BYTE Reserved0;
			WORD Reserved1;

			struct _CAR //64 bytes
			{
				BYTE CapacityCrit;
				BYTE Reserved0;
				WORD ItemSize;
				DWORD ItemWeight;
				WORD FuelTank;
				BYTE ChargeFuel;
				BYTE Reserved1;
				WORD RunToBreak;

				WORD Reserved2;
				DWORD Reserved3;

				char Door[2];
				char Carrier[2];
				BYTE Blocks[CAR_MAX_BLOCKS/2]; //40

				BYTE GetBlockDir(int num){return num%2?Blocks[num/2]&0xF:Blocks[num/2]>>4;}
//				void SetBlockDir(int num, BYTE dir){if(num%2) Blocks[num/2]=(Blocks[num/2]&0xF0)|dir; else Blocks[num/2]=(Blocks[num/2]&0xF)|(dir<<4);}
			} CAR;
		} MISC2;

		struct
		{
			DWORD Reserved;
		} KEY;

		struct
		{
			DWORD Size; //{Объем контейнера}
			BOOL CannotPickUp;
			BOOL MagicHandsGrnd;
			BOOL Changeble;
		} CONTAINER;
		
		struct
		{
			BOOL WalkThru;
			DWORD Unknown;
		} DOOR;

//Scenery
		struct
		{
			BYTE Type;
		} GRID;

		struct
		{
			DWORD Reserved;
		} GENERIC;

//Wall
		struct
		{
			DWORD Reserved;
		} WALL;

//Tile
		struct
		{
			DWORD Flags;
			DWORD FlagsExt;
			DWORD Unknown;
		} TILE;
	};

	void AddRef(){};
	void Release(){};

	BYTE GetType(){return Type;}
	BYTE GetSubType(){return SubType;}
	WORD GetPid(){return Pid;}
};

typedef vector<CProtoObject> PObjVec;
typedef vector<CProtoObject>::iterator PObjVecIt;

class CObject;
typedef map<DWORD, CObject*, less<DWORD> > ObjectMap;
typedef ObjectMap::iterator ObjMapIt;
typedef ObjectMap::value_type ObjMapVal;
typedef vector<CObject*> ObjectVec;
typedef vector<CObject*>::iterator ObjectVecIt;
typedef vector<CObject*>::value_type ObjectVecVal;
typedef multimap<DWORD, CObject*, less<DWORD> > ObjectMulMap;
typedef multimap<DWORD, CObject*, less<DWORD> >::iterator ObjectMulMapIt;
typedef multimap<DWORD, CObject*, less<DWORD> >::value_type ObjectMulMapVal;

class CObject
{
public:
	DWORD Id;
	CProtoObject* proto;

	BYTE accessory;
	union
	{
		struct
		{
			WORD Map;
			WORD X;
			WORD Y;
		} ACC_HEX;

		struct
		{
			CrID Id;
			BYTE Slot;
		} ACC_CRITTER;

		struct
		{
			DWORD Id;
		} ACC_CONTAINER;
	};

	union
	{
		struct
		{
			DWORD SpFlags;
			DWORD BrokenInfo;
		} ARMOR;

		struct
		{
			DWORD DoorId;
			WORD Condition;
			BYTE IsOpen;

			ObjectMap* Objects;
			DWORD CurCount;
			WORD CurSize;
			DWORD CurWeight;
		} CONTAINER;

		struct
		{
			DWORD Count;
		} DRUG;

		struct
		{
			DWORD SpFlags;
			DWORD BrokenInfo; //Or Count for Grouped/NoWeared weapon

			WORD AmmoPid;      // Прототип заряженных патронов.
			WORD AmmoCount;    // Количество заряженных патронов
			WORD AmmoPidExt;      // Прототип заряженных патронов.
			WORD AmmoCountExt;    // Количество заряженных патронов
		} WEAPON;

		struct
		{
			DWORD Count;        // кол-во патронов в обойме
		} AMMO;

		struct
		{
			DWORD Count;		// Текущее количество
		} MISC;

		struct
		{
			DWORD Val1;			// Look in end of this file
			DWORD Val2;			// Look in end of this file
			DWORD Val3;			// Look in end of this file
		} MISC2;

		struct
		{
			DWORD DoorId;     // Код двери, которую открывает ключ, Door or Container
		} KEY;

		struct
		{
			DWORD DoorId;
			BYTE IsOpen;
			WORD Condition;
		} DOOR;
	};

	void AddRef(){};
	void Release(){};

	DWORD GetId(){return Id;}
	WORD GetProtoId(){return proto->GetPid();}
	BYTE GetType(){return proto->GetType();}
	BYTE GetSubType(){return proto->GetSubType();}
};

class CCritInfo
{
private:
	DWORD mapId;
	WORD MapPid;

public:
	CrID Id;

	CrTYPE BaseType;

	WORD HexX;
	WORD HexY;

	WORD WorldX;
	WORD WorldY;

	BYTE Ori;
	char Name[MAX_NAME+1];
	char Cases[5][MAX_NAME+1];

	BYTE Cond;
	BYTE CondExt;
	WORD Flags;

	int St[MAX_STATS ];
	short Sk[MAX_SKILLS];
	BYTE Pe[MAX_PERKS ];
};

class CCritter
{
public:
	CCritInfo Info;

	void AddRef(){};
	void Release(){};

	DWORD GetMap(){return 0;}
	WORD GetProtoMap(){return 0;}

	int IsFree(){return 0;};
	int IsBusy(){return 0;};
	void BreakTime(unsigned int ms){};

	int CreateTimeEvent(WORD num, char month, char day, char hour, char minute,  int loop_period,  string& module, string& funcName){return 0;};
	int ChangeTimeEventA(int num, string& module, string& func_name){return 0;}
	int ChangeTimeEventB(int num, char month, char day, char hour, char minute){return 0;}
	int ChangeTimeEventC(int num, int loop_period){return 0;}

	int EraseTimeEvent(WORD num){return 0;};

	DWORD CountObj(){return 0;}
	DWORD CountObjByPid(WORD obj_pid){return 0;}
	DWORD WeightObj(){return 0;}
	WORD SizeObj(){return 0;}

	void Send_Param(BYTE type_param, WORD num_param){};
	void SendMessage(int num, int to){}

	BOOL CheckKnownLocByPid(WORD loc_pid){return TRUE;}
	void AddKnownLocByPid(WORD loc_pid){}

	void SetBreakTime(UINT ms){}
};

namespace Script
{
	void Log(string& str){}
}

class CServer
{
public:
	static int Crit_MoveRandom(CCritter* cr){return 0;}
	static int Crit_MoveToDir(CCritter* cr, BYTE direction){return 0;}
	static int Crit_MoveToHex(CCritter* npc, WORD hx, WORD hy, BYTE ori){return 0;}

	static void Crit_Say(CCritter* cr, BYTE how_say, string& text){}
	static void Crit_SetDir(CCritter* cr, BYTE dir){}
	static int Crit_SetTarget(CCritter* npc, CCritter* targ, DWORD min_hp){return 0;}

	static CObject* CreateObjToCr(CCritter* cr, WORD pid, DWORD count){return 0;}
	static CObject* CreateObjToHex(CCritter* cr, WORD hx, WORD hy, WORD pid, DWORD count){return 0;}
	static BOOL DeleteObjFromCr(CCritter* cr, WORD pid, DWORD count){return 0;}
};

int Random(int minimum, int maximum){return 0;}

WORD Game_Year;
BYTE Game_Month;
BYTE Game_Day;
BYTE Game_Hour;
BYTE Game_Minute;
BYTE Game_TimeMultiplier;
DWORD Game_ForceDialog;