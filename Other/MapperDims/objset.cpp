//////////////////////////////////////////////////////////////////////
// CObjSet Class
//////////////////////////////////////////////////////////////////////

#include <math.h>
#include "objset.h"
#include "mdi.h"
#include "main.h"
#include "script.h"
#include "utilites.h"
#include "macros.h"
#include "objtempl.h"

#define START_VEC_SIZE 500

int IDlen[6] = {-1, 128, -1, 88, 0, 0};
// Для Fallout 1 длинна объектов LadderUp и LadderDown меньше на 4 байта
int SubIDlen[6][7] = {88, 88, 88, 96, 92, 92, 92,
                     128,  0,  0,  0,  0,  0,  0,
                      92, 96, 96, 96, 96, 88,  0,
                      88,  0,  0,  0,  0,  0,  0,
                       0,  0,  0,  0,  0,  0,  0,
                      104, 88};

String _Types[6] = {"item", "critter", "scenery", "wall", "tile", "misc"};
String SubTypes[6][7]=
{
    {"Armor", "Container", "Drug", "Weapon", "Ammo", "MiscItem", "Key"},
    {"none" , ""         ,  ""   , ""      , ""    , ""        , "" },
    {"Portal", "Stair", "Elevator", "LadderBottom", "LadderUp", "Generic", ""},
    {"none"  , ""     , ""        , ""            , ""        , ""       , ""},
    {"none"  , ""     , ""        , ""            , ""        , ""       , ""},
    {"ExitGrid", "Generic", ""    , ""            , ""        , ""       , ""}
};
//"items", "critters", "scenery", "walls", "tiles", "misc"

String AdditionalParamsNames[6][7][6] =
{
	// Items
	{
		{"Износ", "Инфа состояния", "", "", "", ""},                               // Armor
		{"Номер замка", "Открыт", "Состояние", "", "", ""},                               // Container
		{"Кол-во", "", "", "", "", ""},                               // Drug
		{"Износ", "Инфа состояния", "Прототип потронов #1", "Кол-во патронов #1", "Прототип потронов #2", "Кол-во патронов #2" },       // Weapon
		{"Кол-во в обойме", "", "", "", "", ""},                // Ammo
		{"Кол-во зарядов", "Значение1", "Значение2", "Значение3", "", ""},                 // Misc
		{"Номер замка", "", "", "", "", ""}                        // Key
    },
    // Critters
    {
		{"Диалог", "Действие", "Команда", "AI", "Жизнь", "Отравление"},
		{"Отравление", "", "", "", "", ""},
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
	},
	// Scenery
	{
		{"Номер замка", "Состояние", "", "", "", ""},                                   // Портал
		{"ID карты", "Координата X", "Координата Y", "Ориентация", "", ""},             // Stair
		{"ID карты", "Координата X", "Координата Y", "Ориентация", "Тип лифта", ""},    // Elevator
		{"ID карты", "Координата X", "Координата Y", "Ориентация", "", ""},             // Ladder Bottom
		{"ID карты", "Координата X", "Координата Y", "Ориентация", "", ""},             // Ladder Top
		{"", "", "", "", "", ""},                                                       // Generic
		{"", "", "", "", "", ""},
    },
    // Walls
    {
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
	},
	// Misc
	{
		{"ID карты", "Координата X", "Координата Y", "Ориентация", "", ""},             // Exit Grid
		{"", "", "", "", "", ""},                                                       // Generic
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
		{"", "", "", "", "", ""},
        {"", "", "", "", "", ""},
    }
};

String GenericParamsNames[23] =
{
    "Тип объекта",
    "Подтип объекта",
	"Количество в инвентаре",
	"Max количество в инвентаре",
    "Координата X",
    "Координата Y",
    "Флаги",
	"Frame ID",
	"Frame Num",
    "Ориентация",
	"HEX смещение по X",
	"HEX смещение по Y",
	"HEX смещение2 по X",
	"HEX смещение2 по Y",
	"Proto ID",
	"Combat ID",
	"Идентификатор скрипта",
	"Script ID",
	"Освещаемый радиус",
	"Интенсивность",
	"Контур",
	"Unknown10",
	"Updated Flags"
};

String FlagsNames[32] =
{
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "В контейнере",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not use",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
    "Not used",
};

//---------------------------------------------------------------------------
// Name: GetObjectSubType
// Desc: Возвращает подтип объекта по типу и прототипу.
BYTE GetObjectSubType(BYTE nType, DWORD nProtoID)
{
    switch (nType)
    {
        case item_ID:
        case scenery_ID:
             return pProSet->GetSubType(nType, nProtoID);
        case critter_ID:
        case wall_ID:
        case tile_ID:
             return 0;       // Для криттеров, тайлов и стен нет подтипов.
        case misc_ID:
             switch (nProtoID)
             {
                case 12:
                case  1: return OSGenericMisc; break;
                default: return OSExitGrid;    break;
             }
        default: return 0xFF;
    }
}
//---------------------------------------------------------------------------
bool GetObjectType(BYTE * pObj, BYTE  * nType, BYTE  * nSubType, DWORD * nID)
{
    // pObj указывает на начало объекта
    // Получаем прототип
    DWORD nPID = pUtil->GetDW((DWORD *)(pObj + 0x2C));
    // Тип объекта
	*nType = (BYTE)(nPID >> 24);
    // ID объекта
    *nID = (DWORD)(nPID);
    // Получаем подтип объекта из прототипа
    pProSet->LoadPRO(*nType, nPID & 0x0000FFFF, true);

    *nSubType = GetObjectSubType(*nType, *nID & 0x0000FFFF);

    return true;
}
//---------------------------------------------------------------------------
void OBJECT::GetParamName(String & out, BYTE index)
{
	if (index < 23)
	{
		out = GenericParamsNames[index];
		return;
	} else if (index >= 23 && index <= 28)
	{
		out = AdditionalParamsNames[OBJECT_STRUCT.type][OBJECT_STRUCT.subType][index - 23];
		return;
	} out = "none";

    return;
}
//---------------------------------------------------------------------------
void OBJECT::GetFlagName(String & out, BYTE index)
{
    if (index > 31)
    {
        out = "none";
        return;
	}
	out = FlagsNames[index];
}
//---------------------------------------------------------------------------
DWORD OBJECT::GetParamValue(BYTE index)
{
    switch (index)
    {
        case  0: return OBJECT_STRUCT.type;
		case  1: return OBJECT_STRUCT.subType;
		case  2: return OBJECT_STRUCT.contSize;
		case  3: return OBJECT_STRUCT.contMax;
		case  4: return OBJECT_STRUCT.mapX;
		case  5: return OBJECT_STRUCT.mapY;
		case  6: return OBJECT_STRUCT.flags;
		case  7: return OBJECT_STRUCT.frameId;
		case  8: return OBJECT_STRUCT.frameNum;
		case  9: return OBJECT_STRUCT.dir;
		case 10: return OBJECT_STRUCT.offsetX;
		case 11: return OBJECT_STRUCT.offsetY;
		case 12: return OBJECT_STRUCT.offsetX2;
		case 13: return OBJECT_STRUCT.offsetY2;
		case 14: return OBJECT_STRUCT.protoId;
		case 15: return OBJECT_STRUCT.combatId;
		case 16: return OBJECT_STRUCT.indScript;
		case 17: return OBJECT_STRUCT.scriptId;
		case 18: return OBJECT_STRUCT.radius;
		case 19: return OBJECT_STRUCT.intensity;
		case 20: return OBJECT_STRUCT.contour;
		case 21: return OBJECT_STRUCT.unknown10;
		case 22: return OBJECT_STRUCT.updatedFlags;
	}

	switch (OBJECT_STRUCT.type)
	{
		case item_ID:
			if (OBJECT_STRUCT.subType == OSArmor)
			{
				if (index == 23) return OBJECT_STRUCT.ARMOR.timeWear;
				if (index == 24) return OBJECT_STRUCT.ARMOR.brokenInfo;
			}
			if (OBJECT_STRUCT.subType == OSWeapon)
			{
				if (index == 23) return OBJECT_STRUCT.WEAPON.timeWear;
				if (index == 24) return OBJECT_STRUCT.WEAPON.brokenInfo;
				if (index == 25) return OBJECT_STRUCT.WEAPON.ammoPid;
				if (index == 26) return OBJECT_STRUCT.WEAPON.ammoCount;
				if (index == 27) return OBJECT_STRUCT.WEAPON.ammoPidExt;
				if (index == 28) return OBJECT_STRUCT.WEAPON.ammoCountExt;
			}
			if (OBJECT_STRUCT.subType == OSAmmo)
			{
				if (index == 23) return OBJECT_STRUCT.AMMO.count;
			}
			if (OBJECT_STRUCT.subType == OSMiscItem)
			{
				if (index == 23) return OBJECT_STRUCT.ITEM_MISC.count;
				if (index == 24) return OBJECT_STRUCT.ITEM_MISC.val1;
				if (index == 25) return OBJECT_STRUCT.ITEM_MISC.val2;
				if (index == 26) return OBJECT_STRUCT.ITEM_MISC.val3;
			}
			if (OBJECT_STRUCT.subType == OSKey)
			{
				if (index == 23) return OBJECT_STRUCT.KEY.doorId;
            }
        break;
        case wall_ID:
		case critter_ID:
			if (index == 23) return OBJECT_STRUCT.CRITTER.dialogId;
			if (index == 24) return OBJECT_STRUCT.CRITTER.action;
			if (index == 25) return OBJECT_STRUCT.CRITTER.team;
			if (index == 26) return OBJECT_STRUCT.CRITTER.AI;
			if (index == 27) return OBJECT_STRUCT.CRITTER.life;
			if (index == 28) return OBJECT_STRUCT.CRITTER.radiation;
			if (index == 29) return OBJECT_STRUCT.CRITTER.poison;
			if (index == 30) return OBJECT_STRUCT.CRITTER.cdDmgLastTurn;
			if (index == 31) return OBJECT_STRUCT.CRITTER.cdCurrMp;
			if (index == 32) return OBJECT_STRUCT.CRITTER.cdResults;
			if (index == 33) return OBJECT_STRUCT.CRITTER.cdWhoHitMe;
			if (index == 34) return OBJECT_STRUCT.CRITTER.unknown2;
		break;
        case scenery_ID:
		{
			if (OBJECT_STRUCT.subType == OSPortal)
			{
				if (index == 23) return OBJECT_STRUCT.DOOR.doorId;
				if (index == 24) return OBJECT_STRUCT.DOOR.condition;
			}
			if (OBJECT_STRUCT.subType == OSStairs)
			{
				if (index == 23) return OBJECT_STRUCT.STAIR.mapId;
				if (index == 24) return OBJECT_STRUCT.STAIR.mapX;
				if (index == 25) return OBJECT_STRUCT.STAIR.mapY;
				if (index == 26) return OBJECT_STRUCT.STAIR.dir;
			}
			if (OBJECT_STRUCT.subType == OSElevator)
			{
				if (index == 23) return OBJECT_STRUCT.ELEVATOR.mapId;
				if (index == 24) return OBJECT_STRUCT.ELEVATOR.mapX;
				if (index == 25) return OBJECT_STRUCT.ELEVATOR.mapY;
				if (index == 26) return OBJECT_STRUCT.ELEVATOR.dir;
				if (index == 27) return OBJECT_STRUCT.ELEVATOR.type;
			}
			if (OBJECT_STRUCT.subType == OSLadderBottom || OBJECT_STRUCT.subType == OSLadderTop)
			{
				if (index == 23) return OBJECT_STRUCT.LADDER.mapId;
				if (index == 24) return OBJECT_STRUCT.LADDER.mapX;
				if (index == 25) return OBJECT_STRUCT.LADDER.mapY;
				if (index == 26) return OBJECT_STRUCT.LADDER.dir;
			}
		}
		break;
		case misc_ID:
			if (OBJECT_STRUCT.subType == OSExitGrid)
			{
				if (index == 23) return OBJECT_STRUCT.EXIT_GRID.mapId;
				if (index == 24) return OBJECT_STRUCT.EXIT_GRID.mapX;
				if (index == 25) return OBJECT_STRUCT.EXIT_GRID.mapY;
				if (index == 26) return OBJECT_STRUCT.EXIT_GRID.dir;
            }
        break;
    }

    return 0;
}
void OBJECT::SetParamValue(BYTE index, void * data)
{
    switch (index)
    {
        case  0:                            return;
        case  1:                            return;
		case  2: SetCount(*(WORD*)data);    return;
		case  3: OBJECT_STRUCT.contMax=*(DWORD*)data; return;
		case  4: SetHexX(*(WORD*)data);     return;
		case  5: SetHexY(*(WORD*)data);     return;
		case  6: OBJECT_STRUCT.flags = *(DWORD*)data;     return;
		case  7:                            return;
		case  8:                            return;
		case  9: SetDirection(*(BYTE*)data); return;
		case 10: OBJECT_STRUCT.offsetX = *(BYTE*)data;    return;
		case 11: OBJECT_STRUCT.offsetY = *(BYTE*)data;    return;
		case 12: OBJECT_STRUCT.offsetX2 = *(BYTE*)data;    return;
		case 13: OBJECT_STRUCT.offsetY2 = *(BYTE*)data;    return;
		case 14:                            return;
		case 15: OBJECT_STRUCT.combatId = *(DWORD*)data;  return;
		case 16: OBJECT_STRUCT.indScript = *(DWORD*)data;  return;
		case 17: OBJECT_STRUCT.scriptId = *(DWORD*)data;  return;
		case 18: OBJECT_STRUCT.radius   = *(BYTE*)data;   return;
		case 19: OBJECT_STRUCT.intensity = *(BYTE*)data;  return;
		case 20: OBJECT_STRUCT.contour = *(BYTE*)data;  return;
		case 21: OBJECT_STRUCT.unknown10 = *(DWORD*)data;  return;
		case 22: OBJECT_STRUCT.updatedFlags = *(DWORD*)data;  return;
    }

    switch (OBJECT_STRUCT.type)
	{
		case critter_ID:
			if (index == 23) { OBJECT_STRUCT.CRITTER.dialogId = *(DWORD*)data; return; }
			if (index == 24) { OBJECT_STRUCT.CRITTER.action = *(DWORD*)data; return; }
			if (index == 25) { OBJECT_STRUCT.CRITTER.team = *(DWORD*)data; return; }
			if (index == 26) { OBJECT_STRUCT.CRITTER.AI = *(DWORD*)data; return; }
			if (index == 27) { OBJECT_STRUCT.CRITTER.life = *(DWORD*)data; return; }
			if (index == 28) { OBJECT_STRUCT.CRITTER.radiation = *(DWORD*)data; return; }
			if (index == 29) { OBJECT_STRUCT.CRITTER.poison = *(DWORD*)data; return; }
			if (index == 30) { OBJECT_STRUCT.CRITTER.cdDmgLastTurn = *(DWORD*)data; return; }
			if (index == 31) { OBJECT_STRUCT.CRITTER.cdCurrMp = *(DWORD*)data; return; }
			if (index == 32) { OBJECT_STRUCT.CRITTER.cdResults = *(DWORD*)data; return; }
			if (index == 33) { OBJECT_STRUCT.CRITTER.cdWhoHitMe = *(DWORD*)data; return; }
			if (index == 34) { OBJECT_STRUCT.CRITTER.unknown2 = *(DWORD*)data; return; }
		break;
		case item_ID:
			if (OBJECT_STRUCT.subType == OSArmor)
			{
				if (index == 23) { OBJECT_STRUCT.ARMOR.timeWear = *(DWORD*)data; return; }
				if (index == 24) { OBJECT_STRUCT.ARMOR.brokenInfo = *(DWORD*)data; return; }
			}
			if (OBJECT_STRUCT.subType == OSWeapon)
			{
				if (index == 23) { OBJECT_STRUCT.WEAPON.timeWear = *(DWORD*)data; return; }
				if (index == 24) { OBJECT_STRUCT.WEAPON.brokenInfo = *(DWORD*)data; return; }
				if (index == 25) { OBJECT_STRUCT.WEAPON.ammoPid = *(WORD*)data; return; }
				if (index == 26) { OBJECT_STRUCT.WEAPON.ammoCount = *(DWORD*)data; return; }
				if (index == 27) { OBJECT_STRUCT.WEAPON.ammoPidExt = *(WORD*)data; return; }
				if (index == 28) { OBJECT_STRUCT.WEAPON.ammoCountExt = *(DWORD*)data; return; }
			}
			if (OBJECT_STRUCT.subType == OSAmmo)
			{
				if (index == 23) { OBJECT_STRUCT.AMMO.count = *(WORD*)data; return; }
			}
			if (OBJECT_STRUCT.subType == OSMiscItem)
			{
				if (index == 23) { OBJECT_STRUCT.ITEM_MISC.count = *(WORD*)data; return; }
				if (index == 24) { OBJECT_STRUCT.ITEM_MISC.val1 = *(DWORD*)data; return; }
				if (index == 25) { OBJECT_STRUCT.ITEM_MISC.val2 = *(DWORD*)data; return; }
				if (index == 26) { OBJECT_STRUCT.ITEM_MISC.val3 = *(DWORD*)data; return; }
			}
			if (OBJECT_STRUCT.subType == OSKey)
			{
				if (index == 23) { OBJECT_STRUCT.KEY.doorId = *(DWORD*)data; return; }
			}
		break;
		case scenery_ID:
		{
			if (OBJECT_STRUCT.subType == OSPortal)
			{
				if (index == 23) { OBJECT_STRUCT.DOOR.doorId = *(DWORD*)data; return; }
				if (index == 24) { OBJECT_STRUCT.DOOR.condition = *(DWORD*)data; return; }
			}
			if (OBJECT_STRUCT.subType == OSStairs)
			{
				if (index == 23) { OBJECT_STRUCT.STAIR.mapId = *(DWORD*)data; return; }
				if (index == 24) { OBJECT_STRUCT.STAIR.mapX = *(WORD*)data; return; }
				if (index == 25) { OBJECT_STRUCT.STAIR.mapY = *(WORD*)data; return; }
				if (index == 26) { OBJECT_STRUCT.STAIR.dir = *(BYTE*)data; return; }
			}
			if (OBJECT_STRUCT.subType == OSElevator)
			{
				if (index == 23) { OBJECT_STRUCT.ELEVATOR.mapId = *(DWORD*)data; return; }
				if (index == 24) { OBJECT_STRUCT.ELEVATOR.mapX = *(WORD*)data; return; }
				if (index == 25) { OBJECT_STRUCT.ELEVATOR.mapY = *(WORD*)data; return; }
				if (index == 26) { OBJECT_STRUCT.ELEVATOR.dir = *(BYTE*)data; return; }
				if (index == 27) { OBJECT_STRUCT.ELEVATOR.type = *(DWORD*)data; return; }
			}
			if (OBJECT_STRUCT.subType == OSLadderBottom || OBJECT_STRUCT.subType == OSLadderTop)
			{
				if (index == 23) { OBJECT_STRUCT.LADDER.mapId = *(DWORD*)data; return; }
				if (index == 24) { OBJECT_STRUCT.LADDER.mapX = *(WORD*)data; return; }
				if (index == 25) { OBJECT_STRUCT.LADDER.mapY = *(WORD*)data; return; }
				if (index == 26) { OBJECT_STRUCT.LADDER.dir = *(BYTE*)data; return; }
			}
		}
		break;
		case misc_ID:
			if (OBJECT_STRUCT.subType == OSExitGrid)
			{
				if (index == 23) { OBJECT_STRUCT.EXIT_GRID.mapId = *(DWORD*)data; return; }
				if (index == 24) { OBJECT_STRUCT.EXIT_GRID.mapX = *(WORD*)data; return; }
				if (index == 25) { OBJECT_STRUCT.EXIT_GRID.mapY = *(WORD*)data; return; }
				if (index == 26) { OBJECT_STRUCT.EXIT_GRID.dir = *(BYTE*)data; return; }
			}
		break;
	}
}

//---------------------------------------------------------------------------
OBJECT::OBJECT(void)
{
	memset(&OBJECT_STRUCT, 0, sizeof(OBJECT_STRUCT));
	// Резервируем память.
	childs.reserve(20);
}
//---------------------------------------------------------------------------
OBJECT::OBJECT(OBJECT * obj)
{
	// Тупо копирем содержимое объекта
	this->OBJECT_STRUCT = obj->OBJECT_STRUCT;
	OBJECT * pChild;
	OBJECT * pNewChild;
	// Копируем потомков
	for (int i = 0; i < obj->GetChildCount(); i++)
	{
        pChild = obj->GetChild(i);
        // Создаем копию чайлда
        pNewChild = new OBJECT(pChild);
        // Сохраняем копию чайлда
        this->AddChildObject(pNewChild);
    }
}
//---------------------------------------------------------------------------
void OBJECT::ReLoadPROTOs(void)
{
    // Загружаем прототип самого объекта
    pProSet->LoadPRO(OBJECT_STRUCT.type,GetProtoIndex(), true);
    OBJECT * pChild;
    // Загружаем прототипы потомков
    for (int i = 0; i < GetChildCount(); i++)
    {
        pChild = GetChild(i);
        // Загружаем прототип потомка
        pChild->ReLoadPROTOs();
        frmMDI->iPos++;
        Application->ProcessMessages();
    }
}

//---------------------------------------------------------------------------
void OBJECT::ReLoadFRMs(void)
{
    // Загружаем рисунок самого объекта
	pFrmSet->LoadFRM(OBJECT_STRUCT.type, OBJECT_STRUCT.frameId, true);
    OBJECT * pChild;
    // Загружаем рисунки потомков
    for (int i = 0; i < GetChildCount(); i++)
    {
        pChild = GetChild(i);
        // Загружаем прототип потомка
        pChild->ReLoadFRMs();
        frmMDI->iPos++;
        Application->ProcessMessages();
    }
}
//---------------------------------------------------------------------------
void OBJECT::DeleteChild(OBJECT * pChild)
{
    vector<OBJECT*>::iterator p  = childs.begin();
    while (p != childs.end())
    {
        if (*p == pChild)
        {
            // Ншали. Удаляем. Стираем.
            delete *p;
            childs.erase(p);
            return;
        }
        p++;
    }
}
//---------------------------------------------------------------------------
void OBJECT::ClearChilds(void)
{
    // Уничтожаем всех потомков
    int i;
    OBJECT * pChild;
    for (i = 0; i < GetChildCount(); i++)
    {
        pChild = GetChild(i);
        _RELEASE(pChild);
    }
    childs.clear();
}
//---------------------------------------------------------------------------
OBJECT::~OBJECT(void)
{
    // При уничтожении объекта очищаем потомков.
    ClearChilds();
}

//---------------------------------------------------------------------------
bool  OBJECT::SaveToDB()
{

}

//---------------------------------------------------------------------------
bool OBJECT::SaveToFile(HANDLE h_map)
{
    if (h_map == INVALID_HANDLE_VALUE || !h_map)
        return false;
    OBJECT * pChild;
    DWORD a;
    // Сохраняем сам объект
    WriteFile(h_map, &OBJECT_STRUCT, sizeof(OBJECT_STRUCT), &a, NULL);
    if (a != sizeof(OBJECT_STRUCT)) return false;

    // Сохраняем вместе с потомками.
    for (int i = 0; i < GetChildCount(); i++)
    {
        pChild = GetChild(i);
        pChild->SaveToFile(h_map);
    }
    return true;
};
//---------------------------------------------------------------------------
OBJECT * OBJECT::AddChildObject(BYTE nType, WORD nProtoIndex, DWORD count)
{
    OBJECT * pChild = new OBJECT;
    WORD hexX = this->GetHexX();
    WORD hexY = this->GetHexY();

    pChild->SetHexX(hexX);
    pChild->SetHexY(hexY);
    pChild->SetFlag(FL_OBJ_IS_CHILD);
    pChild->SetCount(count);

    pChild->OBJECT_STRUCT.type      = nType;
	pChild->OBJECT_STRUCT.subType   = GetObjectSubType(nType, nProtoIndex);
	pChild->OBJECT_STRUCT.protoId   = (nType << 24) | nProtoIndex;
	pChild->OBJECT_STRUCT.frameId   = pProSet->pPRO[nType][nProtoIndex].GetFrmID();
    // Добавляем потомка.
    childs.push_back(pChild);
    return pChild;
//    SelectObject(pSelObjs.size() - 1, false);
}

//---------------------------------------------------------------------------
void OBJECT::AddChildObject(OBJECT * pChild)
{
    childs.push_back(pChild);
}
//---------------------------------------------------------------------------
WORD OBJECT::SetHexX(WORD new_x)
{
	if (new_x >= FO_MAP_WIDTH * 2) return OBJECT_STRUCT.mapX;
    OBJECT * pChild;
    for (int i = 0; i < GetChildCount(); i++)
    {
        pChild = GetChild(i);
        pChild->SetHexX(new_x);
    }
	return OBJECT_STRUCT.mapX = new_x;
};
//---------------------------------------------------------------------------
WORD OBJECT::SetHexY(WORD new_y)
{
	if (new_y >= FO_MAP_HEIGHT * 2) return OBJECT_STRUCT.mapY;
    OBJECT * pChild;
	for (int i = 0; i < GetChildCount(); i++)
    {
        pChild = GetChild(i);
        pChild->SetHexY(new_y);
    }
	return OBJECT_STRUCT.mapY = new_y;
};
//---------------------------------------------------------------------------
CObjSet::CObjSet(CHeader * hdr)
{
    bError = false;

    pObjects.reserve(6*START_VEC_SIZE);
    pSelObjs.reserve(START_VEC_SIZE);

    pHeader         = hdr;
   // В Фолле1 объекты LadderUp и LadderDown на 4 байта меньше
   if (pHeader->GetVersion() == VER_FALLOUT1)
   {
      SubIDlen[2][3] = 92;
      SubIDlen[2][4] = 92;
   }
   else
   {
      SubIDlen[2][3] = 96;
      SubIDlen[2][4] = 96;
   }

    MinY = pHeader->GetHeight()-1;
    MaxX = 0;
};
//---------------------------------------------------------------------------
CObjSet::~CObjSet()
{
    for ( int j =0; j < pObjects.size(); j++)
        _RELEASE(pObjects[j]);
    pObjects.clear();
    pSelObjs.clear();
}

//---------------------------------------------------------------------------
void CObjSet::FindUpRightObj(void)
{
    DWORD i;
    OBJECT * pObj;
    if (!pObjects.size()){
        MinY = pHeader->GetHeight()-1;
        MaxX = 0;
        MaxY = 0;
        MinX = pHeader->GetWidth() - 1;
        return;
    } else
    {
        MaxY = MinY = pSelObjs[0]->GetHexY();
        MaxX = MinX = pSelObjs[0]->GetHexX();
    }
    for ( i = 0; i < pObjects.size(); i++)
    {
        pObj = pObjects[i];
        if (pObj->GetHexX() > MaxX)
            MaxX = pObj->GetHexX();
        if (pObj->GetHexX() < MinX)
            MinX = pObj->GetHexX();

        if (pObj->GetHexY() < MinY)
            MinY = pObj->GetHexY();
        if (pObj->GetHexY() > MaxY)
            MaxY = pObj->GetHexY();
    }

    width  = MaxX - MinX;
    height = MaxY - MinY;
}
//---------------------------------------------------------------------------
void CObjSet::MoveSelectedTo(int x, int y)
{
    DWORD i;
    OBJECT * pObj;
    OBJECT * pChild;
    
    if (x + (int)width < 0 || x + (int)width > (int)pHeader->GetWidth() - 1 ||
        y + (int)height < 0 || y + (int)height > (int)pHeader->GetHeight() - 1)
            return;

    int new_x, new_y;
    for ( i = 0; i < pSelObjs.size(); i++ )
    {
        pObj = pSelObjs[i];
        new_x = pObj->GetHexX() - MinX + x;
        new_y = pObj->GetHexY() - MinY + y;
        pObj->SetHexX(new_x);
        pObj->SetHexY(new_y);
    }

    MinX = x;
    MinY = y;
    MaxX = x + width;
    MaxY = y + height;
}
//---------------------------------------------------------------------------
DWORD CObjSet::LoadFromFile(HANDLE h_map)
{
    // В зависимости от версии Fallout вызываем соотв. ф-ию
    if (pHeader->GetVersion() == FO_MAP_VERSION)
    {
        return LoadFromFile_FOnline(h_map);
    } else
		return LoadFromFile_Fallout(h_map);
}

//---------------------------------------------------------------------------
int CObjSet::AddObject(OBJECT * pObj, bool sort)
{
	// Создаем копию.
	OBJECT * pNewObj = new OBJECT(pObj);
	// Сохраняем
	if (!pObj->IsChild())
	{
		if(sort==true)
		{
			for(int i=0,j=pObjects.size();i<j;++i)
			{
				if(pObjects[i]->IsChild()) continue;
				if(pObjects[i]->GetPos()<pNewObj->GetPos()) continue;

				pObjects.insert(pObjects.begin()+i,pNewObj);
				return i;
			}
		}

		pObjects.push_back(pNewObj);
	}
	else pObjects.push_back(pNewObj);

	return pObjects.size()-1;
}
//---------------------------------------------------------------------------
void CObjSet::CopySelectedTo(CObjSet * pOS, bool with_childs)
{
	if(with_childs==true)
	{
		for(int i=0,j=pSelObjs.size();i<j;i++)
			pOS->AddObject(pSelObjs[i],false);
	}
	else
	{
		for(int i=0,j=pSelObjs.size();i<j;i++)
			if(!pSelObjs[i]->IsChild())
				pOS->AddObject(pSelObjs[i],false);
	}
}

void CObjSet::ClearObjSet(void)
{
    OBJECT * pObj;
    for (int i = 0; i < pObjects.size(); i++)
    {
        pObj = pObjects[i];
        _RELEASE(pObj);
    }
    pSelObjs.clear();
    pObjects.clear();
}
//---------------------------------------------------------------------------
// Name: CopyTo
// Desc: Копирует все объекты в pOS
void CObjSet::CopyTo(CObjSet * pOS, bool with_childs)
{
	if(with_childs==true)
	{
		for(int i=0,j=pObjects.size();i<j;i++)
			pOS->AddObject(pObjects[i],false);
	}
	else
	{
		for(int i=0,j=pObjects.size();i<j;i++)
			if(!pObjects[i]->IsChild())
				pOS->AddObject(pObjects[i],false);
	}
}
//---------------------------------------------------------------------------
DWORD CObjSet::LoadFromFile_Fallout(HANDLE h_map)
{
    bError = true;
    DWORD nTotalCount = 0;
    // h_map указывает на начало области с объектами
    DWORD i;
    // Первые 4 байта содержат общее кол-во объектов на уровне
    ReadFile(h_map, &nTotalCount, 4, &i, NULL);
    if ( i != 4) return 0;
    nTotalCount = pUtil->GetDW(&nTotalCount);

    // Текущая позиция указателя файла
    DWORD dwPos = SetFilePointer(h_map, 0, 0, FILE_CURRENT);
    // Размер файла 
    DWORD dwFileSize = GetFileSize(h_map, NULL);
    // Вычисляем размер области с объектами
    DWORD dwBufSize = dwFileSize - dwPos;
    // Выделяем память под буфер
    BYTE * pStart = (BYTE*)malloc(dwBufSize);
    if (pStart == NULL) return 0;
    // Загружаем область с объектами в память
    // Приходится грузить все уровни (если их несколько)
    // т.к. размер объектов заранее не известен
    ReadFile(h_map, pStart, dwBufSize, &i, NULL);
    if (i != dwBufSize)
    {
        free(pStart);
        return 0;
    };

    // pStart указывает на начало области с объектами
    DWORD count = 0;                     // Число прочитанных объектов
    BYTE * pEnd = pStart + dwBufSize;    // Конец памяти.
    BYTE * pCur = pStart;                // Текущий указатель
    BYTE   nObjLen;                      // Длина тек. объекта
    BYTE   nType, nSubType;              // Тип, подтип
    DWORD  nID;                          // Идентификатор
//    WORD   nChildCount = 0;              // Число объектов в контейнере
//    bool   isChild = false;              // В контейнере или нет
    OBJECT * pLastContainer = NULL;      // Указатель на последний контейнер.
    OBJECT *pObj;
    DWORD  nLastChildCount = 0;
    BOOL   isChild;
    while ((count < nTotalCount || nLastChildCount != 0) && pCur < pEnd)
    {
        pObj = NULL;
		pObj = new OBJECT;

        // 1. Получаем тип, подтип, идентификатор объекта
        if (GetObjectType(pCur, &nType, & nSubType, &nID) == false)
        {
            free(pStart);
            return 0;
        };

        if (nLastChildCount) nLastChildCount--;
            else  pLastContainer = NULL;

        // 2. Необходимо узнать размер читаемого объекта
        nObjLen = SubIDlen[nType][nSubType];
        // Выделяем память под новый объект
        pObj->OBJECT_STRUCT.type      = nType;
		pObj->OBJECT_STRUCT.subType   = nSubType;
		pObj->OBJECT_STRUCT.protoId   = nID;
        // 3. Проверяем - объект в контейнере или нет
        DWORD pos = pUtil->GetDW((DWORD*)(pCur + 4));
        if (pos == 0 || pos == 0xFFFFFFFF)
        {
			pObj->OBJECT_STRUCT.mapX = pLastContainer->OBJECT_STRUCT.mapX;
			pObj->OBJECT_STRUCT.mapY = pLastContainer->OBJECT_STRUCT.mapY;
			isChild = true;
		} else
		{
			pObj->OBJECT_STRUCT.mapX = pos % 200;
			pObj->OBJECT_STRUCT.mapY = pos / 200;
            isChild = false;
            count++;
            frmMDI->iPos++;
            Application->ProcessMessages();
         }
        // 4. Если объект является контейнером, то получаем число объектов в нем
        if ((nType == critter_ID) || (nType == item_ID && nSubType == OSContainer))
        {
            // Объект - контейнер
            pLastContainer = pObj;          // СОхраняем указатель на него.
            nLastChildCount = pUtil->GetDW((DWORD *)(pCur + 0x48));
        }

        // 5. Сохраняем общие параметры объекта.
		pObj->OBJECT_STRUCT.flags = pUtil->GetDW((DWORD*)(pCur+0x24));
        if (isChild)
        {
			 pObj->OBJECT_STRUCT.flags |= FL_OBJ_IS_CHILD;
			 pObj->OBJECT_STRUCT.contSize = pUtil->GetDW((DWORD*)(pCur - 0x04));
		}
		else
			pObj->OBJECT_STRUCT.contSize = 0;
		pObj->OBJECT_STRUCT.contMax = pUtil->GetDW((DWORD*)(pCur + 0x4C));

		pObj->OBJECT_STRUCT.frameId = pUtil->GetDW((DWORD*)(pCur + 0x20));
		pObj->OBJECT_STRUCT.frameNum = pUtil->GetDW((DWORD*)(pCur + 0x18));
		pObj->OBJECT_STRUCT.dir = pUtil->GetDW((DWORD*)(pCur + 0x1C));

		pObj->OBJECT_STRUCT.offsetX = pUtil->GetDW((DWORD*)(pCur + 0x08));
		pObj->OBJECT_STRUCT.offsetY = pUtil->GetDW((DWORD*)(pCur + 0x0C));
		pObj->OBJECT_STRUCT.offsetX2 = pUtil->GetDW((DWORD*)(pCur + 0x10));
		pObj->OBJECT_STRUCT.offsetY2 = pUtil->GetDW((DWORD*)(pCur + 0x14));

		pObj->OBJECT_STRUCT.protoId = pUtil->GetDW((DWORD*)(pCur + 0x2C));
		pObj->OBJECT_STRUCT.combatId = pUtil->GetDW((DWORD*)(pCur + 0x30));
		pObj->OBJECT_STRUCT.indScript = pUtil->GetDW((DWORD*)(pCur + 0x40));
		pObj->OBJECT_STRUCT.scriptId = pUtil->GetDW((DWORD*)(pCur + 0x44));

		pObj->OBJECT_STRUCT.radius = pUtil->GetDW((DWORD*)(pCur + 0x34));
		pObj->OBJECT_STRUCT.intensity = pUtil->GetDW((DWORD*)(pCur + 0x38));
		pObj->OBJECT_STRUCT.contour = pUtil->GetDW((DWORD*)(pCur + 0x3C));

		pObj->OBJECT_STRUCT.unknown10 = pUtil->GetDW((DWORD*)(pCur + 0x50));

		// 6. сохраняем частные параметры объекта
        BYTE nFullType = (nType << 4) | (nSubType & 0x0F);
        WORD x, y;
        DWORD val;
        BYTE level;
        switch (nFullType)
        {
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x25:
			case 0x30:
			case 0x51:
				 break;
            // items
			case 0x03:      // Weapon
				 pObj->OBJECT_STRUCT.updatedFlags = pUtil->GetDW((DWORD*)(pCur + 0x54));
				 pObj->OBJECT_STRUCT.WEAPON.ammoPid   = pUtil->GetDW((DWORD*)(pCur + 0x5C));
				 pObj->OBJECT_STRUCT.WEAPON.ammoCount = pUtil->GetDW((DWORD*)(pCur + 0x58));
				 break;
			case 0x04:      // Ammo
				 pObj->OBJECT_STRUCT.updatedFlags = pUtil->GetDW((DWORD*)(pCur + 0x54));
				 pObj->OBJECT_STRUCT.AMMO.count = pUtil->GetDW((DWORD*)(pCur + 0x58));
				 break;
			case 0x05:      // Misc
				 pObj->OBJECT_STRUCT.updatedFlags = pUtil->GetDW((DWORD*)(pCur + 0x54));
				 pObj->OBJECT_STRUCT.ITEM_MISC.count = pUtil->GetDW((DWORD*)(pCur + 0x58));
				 break;
			case 0x06:      // Key
				 pObj->OBJECT_STRUCT.updatedFlags = pUtil->GetDW((DWORD*)(pCur + 0x54));
				 pObj->OBJECT_STRUCT.KEY.doorId = pUtil->GetDW((DWORD*)(pCur + 0x58));
				 break;
			case 0x10:      // Critter
				 pObj->OBJECT_STRUCT.CRITTER.action    = pUtil->GetDW((DWORD*)(pCur + 0x54));
				 pObj->OBJECT_STRUCT.CRITTER.team      = pUtil->GetDW((DWORD*)(pCur + 0x6C));
				 pObj->OBJECT_STRUCT.CRITTER.AI        = pUtil->GetDW((DWORD*)(pCur + 0x68));
				 pObj->OBJECT_STRUCT.CRITTER.life      = pUtil->GetDW((DWORD*)(pCur + 0x74));
				 pObj->OBJECT_STRUCT.CRITTER.radiation = pUtil->GetDW((DWORD*)(pCur + 0x78));
				 pObj->OBJECT_STRUCT.CRITTER.poison    = pUtil->GetDW((DWORD*)(pCur + 0x7C));
				 pObj->OBJECT_STRUCT.CRITTER.cdDmgLastTurn = pUtil->GetDW((DWORD*)(pCur + 0x58));
				 pObj->OBJECT_STRUCT.CRITTER.cdCurrMp = pUtil->GetDW((DWORD*)(pCur + 0x60));
				 pObj->OBJECT_STRUCT.CRITTER.cdResults = pUtil->GetDW((DWORD*)(pCur + 0x64));
				 pObj->OBJECT_STRUCT.CRITTER.cdWhoHitMe = pUtil->GetDW((DWORD*)(pCur + 0x70));
				 pObj->OBJECT_STRUCT.CRITTER.unknown2 = pUtil->GetDW((DWORD*)(pCur + 0x5C));
				 break;
			case 0x20:  // Door
				 pObj->OBJECT_STRUCT.updatedFlags = pUtil->GetDW((DWORD*)(pCur + 0x54));
				 pObj->OBJECT_STRUCT.DOOR.condition = pUtil->GetDW((DWORD*)(pCur + 0x58));
				 break;
			case 0x21:  // Stair
				 pObj->OBJECT_STRUCT.updatedFlags = pUtil->GetDW((DWORD*)(pCur + 0x54));
				 pObj->OBJECT_STRUCT.STAIR.mapId = pUtil->GetDW((DWORD*)(pCur + 0x58));
				 val = pUtil->GetDW((DWORD*)(pCur + 0x5C));
				 pObj->OBJECT_STRUCT.STAIR.mapX  = val%200;
				 pObj->OBJECT_STRUCT.STAIR.mapY  = val/200;
				 break;
			case 0x22:  // Elevator
				 pObj->OBJECT_STRUCT.updatedFlags = pUtil->GetDW((DWORD*)(pCur + 0x54));
				 pObj->OBJECT_STRUCT.ELEVATOR.type = pUtil->GetDW((DWORD*)(pCur + 0x58));
				 break;
			case 0x50: // Exit Grid
				 pObj->OBJECT_STRUCT.updatedFlags = pUtil->GetDW((DWORD*)(pCur + 0x54));
				 pObj->OBJECT_STRUCT.EXIT_GRID.mapId = pUtil->GetDW((DWORD*)(pCur + 0x58));
				 val  = pUtil->GetDW((DWORD*)(pCur + 0x5C));
				 pObj->OBJECT_STRUCT.EXIT_GRID.mapX  = val%200;
				 pObj->OBJECT_STRUCT.EXIT_GRID.mapY  = val/200;
				 pObj->OBJECT_STRUCT.EXIT_GRID.dir  = pUtil->GetDW((DWORD*)(pCur + 0x64));
				 break;
			case 0x23: // Ladder Top
			case 0x24: // Ladder Bottom
				 pObj->OBJECT_STRUCT.updatedFlags = pUtil->GetDW((DWORD*)(pCur + 0x54));
				 pObj->OBJECT_STRUCT.LADDER.mapId = pUtil->GetDW((DWORD*)(pCur + 0x58));
				 val = pUtil->GetDW((DWORD*)(pCur + 0x5C));
				 y = (WORD)val / 200;
				 x = (WORD)val % 200;
				 pObj->OBJECT_STRUCT.LADDER.mapX = x;
				 pObj->OBJECT_STRUCT.LADDER.mapY = y;
                 break;
            default:
            {
                free(pStart);
                return 0;
            }
        }

        // 7. Теперь можно увеличивать указатель
        pCur += nObjLen + (nLastChildCount? 4: 0);
        dwPos += nObjLen+ (nLastChildCount? 4: 0);
 //       count++ ;

        // 8. Сохраняем объект
		if (isChild) // Если загружаем потомка, то добавим его к соотв. контейнеру
            pLastContainer->AddChildObject(pObj);
		else
			AddObject(pObj,true);

	}

    // Если объектов прочитали меньше чем нужно, то выход
	if (count < nTotalCount)
    {
        free(pStart);
        return 0;
    };

    // Позиционируем указатель на начало след. уровня с объектами
    dwPos = SetFilePointer(h_map, dwPos, 0, FILE_BEGIN);

    bError = false;
    free(pStart);
    return nTotalCount;
}
//---------------------------------------------------------------------------
DWORD CObjSet::LoadFromFile_FOnline(HANDLE h_map)
{
    bError = true;
    DWORD cnt;
    DWORD i;
    // Первые 4 байта содержат общее кол-во объектов
    ReadFile(h_map, &cnt, 4, &i, NULL);
    if ( i != 4) return 0;

    frmMDI->frmPBar->NewTask(NULL, "Load objectset ...", 0, cnt, &frmMDI->iPos);
    Application->ProcessMessages();

    DWORD j;
    OBJECT * pObj;
    OBJECT * pContainer;
    for (j = 0; j < cnt; j++)
    {
        pObj = new OBJECT;
        ReadFile(h_map, &pObj->OBJECT_STRUCT, sizeof(pObj->OBJECT_STRUCT), &i, NULL);
        if (i != sizeof(pObj->OBJECT_STRUCT)) return 0;
        // Если загружаемый объект - потомок, то нужно найти контейнер.
        if (pObj->IsChild())
        {
            // Исчем с конца списка.
            for (int k = pObjects.size() - 1; k >= 0; k--)
            {
                pContainer = pObjects[k];
                if (!pContainer->IsChild() &&
                     pContainer->GetHexX() == pObj->GetHexX() &&
                     pContainer->GetHexY() == pObj->GetHexY())
                     {
                        pContainer->AddChildObject(pObj);
                        break;
					 }
			}
		}
		else
			AddObject(pObj,true);

        frmMDI->iPos++;
        Application->ProcessMessages();
    }

    bError = false;
    return cnt;
}
//---------------------------------------------------------------------------
int CObjSet::FindStartObjects(HANDLE h_map)
{
    // Данная процедура позиционирует указатель файла на
    // начало области с объектами
    DWORD i;
    pScriptBuf = new CScriptBuf();

    DWORD dwPos = SetFilePointer(h_map, 0, 0, FILE_CURRENT); // Get file pointer
    // получаем общий размер файла
    DWORD filesize = GetFileSize(h_map, NULL);
    // Вычисляем размер буфера под объекты и скрипты
    DWORD dwBufSize = filesize - dwPos;
    BYTE * pTempBuf;
    // выделяем память
    if ((pTempBuf = (BYTE *)malloc(dwBufSize)) == NULL) return -1;
    // Считываем
    ReadFile(h_map, pTempBuf, dwBufSize, &i, NULL);

    DWORD *ptrDW, count, blocknum, blockcount, length;
    BYTE *ptr;
    int type_count = 5;
    int ScrDescType;
    ptrDW = (DWORD *)pTempBuf;
    while (type_count)
    {
       count = pUtil->GetDW(ptrDW);
       ptrDW++;
       if (!count)
       {
          type_count--;
          continue;
       }
       blocknum = (count >> 4) + (count % 16 != 0);
       ScrDescType = 5 - type_count;
       pScriptBuf->CreateDescBlock(ScrDescType, count);
       while (blocknum)
       {
          for (int i = 0; i < 16; i++)
          {
             ptr = (BYTE *)ptrDW;
             switch (*ptr)
             {
                case 0x01:
                   length = 18;
                   break;
                case 0x02:
                   length = 17; //broken2.map use it
                   break;
                case 0x03:
                   length = 16;
                    break;
                case 0x04:
                   length = 16;
                   break;
                default:
                   length = 16;
                   break;
             }
             if (*ptr == ScrDescType && pScriptBuf->nDescCount[ScrDescType] < count)
                pScriptBuf->CopyDesc(ptrDW, ScrDescType);
             ptrDW += length;
           }
          blocknum--;
          ptrDW += blocknum ? 2 : 1;
       }
       type_count--;
       ptrDW++;
    }
    // ptrDW содержит указатель на начало области объектов
    int offset = -(dwBufSize -  ((BYTE*)ptrDW - pTempBuf)); // Размер области скриптов
    // Освобожаем память
    free(pTempBuf);
    // Перемещаемся на начало области скриптов
    dwPos = SetFilePointer(h_map, offset, NULL, FILE_END);
    DWORD tmp;
    // Считываем общее кол-во объектов на карте
    ReadFile(h_map, &tmp, 4, &i, NULL);
    tmp = pUtil->GetDW(&tmp);
    if (i != 4) return -1;

    return tmp;
}
//---------------------------------------------------------------------------
void CObjSet::ReLoadPROs(void)
{
    OBJECT * pObj;
    // Загружаем прототипы для objectset
    for (int j =0; j < pObjects.size(); j++)
    {
        pObj = pObjects[j];
        pObj->ReLoadPROTOs();
        frmMDI->iPos++;
        Application->ProcessMessages();
    }
}
//---------------------------------------------------------------------------
void CObjSet::ReLoadFRMs(void)
{
    String filename;
    BYTE nType;
    for (int j =0; j < pObjects.size(); j++)
    {
        // Загружаем рисунки
        pObjects[j]->ReLoadFRMs();
        frmMDI->iPos++;
        Application->ProcessMessages();
    }
}
//---------------------------------------------------------------------------
OBJECT * CObjSet::GetObject(DWORD index)
{
    if (index >= pObjects.size()) return NULL;
    return pObjects[index];
}
//---------------------------------------------------------------------------
BOOL CObjSet::SelectObject(DWORD index, BOOL sel)
{
    if (index >= pObjects.size()) return false;
    if (sel == true) pSelObjs.push_back(pObjects[index]);
	else
	{
		vector<OBJECT*>::iterator p = pSelObjs.begin();
		p += index;
		pSelObjs.erase(p);
	}
	frmEnv->panelObjSelected->Text = "Selected: " + (String)pSelObjs.size();
	frmEnv->sbar->Update();
    return true;
}
//---------------------------------------------------------------------------
OBJECT * CObjSet::GetSelectedObject(DWORD index)
{
    if (index >= pSelObjs.size()) return NULL;
    return pSelObjs[index];
}

//---------------------------------------------------------------------------
void CObjSet::ClearSelection(void)
{
    pSelObjs.clear();
}
//---------------------------------------------------------------------------
void CObjSet::AddObject(WORD HexX, WORD HexY, BYTE nType, WORD nProtoIndex)
{
    if (nType > 5 ||
        HexX >= FO_MAP_WIDTH * 2 ||
		HexY >= FO_MAP_HEIGHT* 2 )
		return;
	OBJECT * pObj   = new OBJECT;
    BYTE nSubType   = GetObjectSubType(nType, nProtoIndex);
    pObj->OBJECT_STRUCT.type      = nType;
	pObj->OBJECT_STRUCT.subType   = nSubType;
	pObj->OBJECT_STRUCT.mapX      = HexX;
	pObj->OBJECT_STRUCT.mapY      = HexY;
	pObj->OBJECT_STRUCT.protoId   = (nType << 24) | nProtoIndex;
	pObj->OBJECT_STRUCT.frameId   = pProSet->pPRO[nType][nProtoIndex].GetFrmID();
	SelectObject(AddObject(pObj,true),true);
	delete pObj;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void CObjSet::DeleteObject(DWORD num)
{
    if (num >= pObjects.size()) return;
    vector<OBJECT*>::iterator p = pObjects.begin();
    p += num;
    delete *p;
    pObjects.erase(p);
}
//---------------------------------------------------------------------------
void CObjSet::DeleteSelected(void)
{
    vector<OBJECT*>::iterator p  = pSelObjs.begin();
    vector<OBJECT*>::iterator p2;
    OBJECT * pObj;
    while (p != pSelObjs.end())
    {
        p2 = pObjects.begin();
        for (DWORD i = 0; i < pObjects.size(); i++)
        {
            pObj = pObjects[i];
            if (pObj == *p)
            {
                p2 += i;
                delete pObj;
                pObjects.erase(p2);
                break;
            }
        }
        p++;
    }
    pSelObjs.clear();
}
//---------------------------------------------------------------------------
BOOL CObjSet::ObjIsSelected(OBJECT * pObj)
{
    DWORD i;
    vector<OBJECT*>::iterator p = pSelObjs.begin();
    while (p != pSelObjs.end())
    {
        if (*p == pObj) return true;
         p++;
    }   

    return false;
}
//---------------------------------------------------------------------------
bool CObjSet::SaveToFile(HANDLE h_map)
{
    DWORD i,j;
    OBJECT * pObj;
    // сохраняем число сохраняемых объектов вместе с потомками     
    DWORD cnt = GetObjCount(true);
    WriteFile(h_map, (void*)&cnt, sizeof(cnt), &i, NULL);
    if (i != sizeof(cnt)) return false;
    for (DWORD j = 0; j < GetObjCount(false); j++)
    {
        pObj = pObjects[j];
        if (pObj->SaveToFile(h_map) == false) return false;
    }
    return true;
};

