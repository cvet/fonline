//---------------------------------------------------------------------------
#ifndef objsetH
#define objsetH

#include <classes.hpp>
#include "utilites.h"
#include "proset.h"
#include "frmset.h"
#include "script.h"
#include "map.h"
#pragma option push
#include <vector>
#include <list>
#pragma option pop

using namespace std;

class CProSet;
class CFrmSet;
class CListFiles;

extern String _Types[6];
extern String SubTypes[6][7];
extern String AdditionalParamsNames[6][7][6];
extern String GenericParamsNames[23];

BYTE GetObjectSubType(BYTE nType, DWORD nProtoID);
bool GetObjectType(BYTE * pObj, BYTE  * nType, BYTE  * nSubType, DWORD * nID);

extern CLog            * pLog;
extern CUtilites       * pUtil;
extern CPal            * pPal;
extern CListFiles      * pLstFiles;
extern CFrmSet         * pFrmSet;
extern CProSet         * pProSet;

struct OBJECT
{
friend class CObjSet;
private:
    struct
    {
        // Общие поля для всех типов объектов
		BYTE    type;           // 0x0000 Тип объекта
		BYTE    subType;        // 0x0001 Подтип объекта

		DWORD   contSize;      // 0x0002 Если этот тип объектов входит в состав какого-либо контейнера,
								//        то здесь задается количество объектов данного типа.
								//        Если же это самостоятельный объект, то параметр не имеет значения
		DWORD	contMax;

		WORD    mapX;           // 0x0004 Координата X на карте
		WORD    mapY;           // 0x0006 Координата Y на карте
		DWORD   flags;          // 0x0008 Флаги
		DWORD   frameId;        // 0x000C FrameID
		DWORD	frameNum;		// 0x0010 Frame number
		BYTE    dir;            // 0x00 Ориентация объекта

		DWORD   offsetX;        // 0x00 Смещение картинки объекта относит.
		DWORD   offsetY;        //        стандартного местоположения
		DWORD	offsetX2;
		DWORD	offsetY2;
							//        в хексагональной сетке.
		DWORD   protoId;        // 0x00 Прототип объекта
		DWORD	combatId;
		DWORD	indScript;
		DWORD   scriptId;       // 0x00 Идентификатор скрипта

		BYTE    radius;         // 0x00 Радиус окружности, освещаемой объектом
		BYTE    intensity;      // 0x00 Интенсивность света.
		BYTE    contour;

		DWORD	unknown10;

		DWORD   updatedFlags;

        // Специфичные поля для типов объектов
        union
		{
			struct
			{
				DWORD	dialogId;
				DWORD	action;
				DWORD	team;
				DWORD	AI;

				DWORD	life;
				DWORD	radiation;
				DWORD	poison;

				DWORD	cdDmgLastTurn;
				DWORD	cdCurrMp;
				DWORD	cdResults;
				DWORD	cdWhoHitMe;

				DWORD	unknown2;
			} CRITTER;

			struct
			{
				DWORD 	timeWear;
				DWORD 	brokenInfo;
			} ARMOR;

			struct
			{
				DWORD 	timeWear;
				DWORD 	brokenInfo;

				DWORD   ammoPid;            // Прототип заряженных патронов.
				WORD    ammoCount;          // Количество заряженных патронов
				WORD    ammoPidExt;        // I?ioioei ca?y?aiiuo iao?iiia.
				DWORD   ammoCountExt;      // Eiee?anoai ca?y?aiiuo iao?iiia
			} WEAPON;

            struct
            {
				WORD  count;        // кол-во патронов в обойме
			} AMMO;

			struct
			{
				WORD  count;        // Текущее количество зарядов (т.е., насколько заряжен).
				DWORD val1;
				DWORD val2;
				DWORD val3;
			} ITEM_MISC;

			struct
			{
				DWORD doorId;
				DWORD condition;
			} DOOR;

			struct
			{
				DWORD   doorId;     // Код двери, которую открывает ключ
			} KEY;

			struct
			{
				DWORD   mapId;      // Номер карты, на которую ведет лестница
				WORD    mapX;       // Позиция игрока на новой карте
				WORD    mapY;
				BYTE    dir;        // Ориентация игрока
            } STAIR;

            struct
            {
				DWORD   mapId;      // Номер карты, на которую попадает игрок.
				WORD    mapX;       // Позиция игрока на новой карте
				WORD    mapY;
				BYTE    dir;        // Ориентация игрока
			} EXIT_GRID;

			struct
			{
				DWORD   mapId;      // Номер карты, на которую ведет лифт
				WORD    mapX;       // Позиция игрока
				WORD    mapY;
				BYTE    dir;        // Ориентация
				DWORD   type;       // Тип лифта
            } ELEVATOR;

            // Лестница
            struct
            {
				DWORD   mapId;       // Номер карты
				WORD    mapX;
				WORD    mapY;
				BYTE    dir;
            } LADDER;

			char buffer[0x18];
        };
    } OBJECT_STRUCT;

    vector<OBJECT*> childs;        // потомки

    // Сохраняет объект в БД
    bool  SaveToDB(void);
    // Сохраняет объект в файл
    bool  SaveToFile(HANDLE h_map);
public:
    OBJECT(void);
    OBJECT(OBJECT * obj);
    ~OBJECT();

    BYTE    GetType(void)       const {return OBJECT_STRUCT.type;};
	BYTE    GetSubType(void)    const {return OBJECT_STRUCT.subType;};

	WORD    GetHexX(void)       const {return OBJECT_STRUCT.mapX;};
	WORD    GetHexY(void)       const {return OBJECT_STRUCT.mapY;};
	DWORD   GetOffsetX(void)    const {return OBJECT_STRUCT.offsetX;};
	DWORD   GetOffsetY(void)    const {return OBJECT_STRUCT.offsetY;};
	BYTE    GetDirection(void)  const {return OBJECT_STRUCT.dir;};
	DWORD   GetFrameID(void)    const {return OBJECT_STRUCT.frameId;};
	WORD    GetFrameIndex(void) const {return (OBJECT_STRUCT.type != 1) ? (WORD)(OBJECT_STRUCT.frameId & 0x0000FFFF) :
																		  (WORD)(OBJECT_STRUCT.frameId & 0x000000FF);};
	DWORD   GetScriptID(void)   const {return OBJECT_STRUCT.scriptId;};
	DWORD   GetProtoID(void)    const {return OBJECT_STRUCT.protoId;};
	DWORD   GetProtoIndex(void) const {return OBJECT_STRUCT.protoId & 0x00FFFFFF;}

	DWORD   GetFlags(void)      const {return OBJECT_STRUCT.flags;};

	BOOL    IsChild(void)       const {return OBJECT_STRUCT.flags & (FL_OBJ_IS_CHILD);};
	DWORD   GetCount(void)      const {return OBJECT_STRUCT.contSize;};

    WORD SetHexX(WORD new_x);
    WORD SetHexY(WORD new_y);

	void  SetCount(WORD cnt) {OBJECT_STRUCT.contSize = cnt;};
	DWORD GetFlag(DWORD flag){ return OBJECT_STRUCT.flags & flag; }
	void  SetFlag(DWORD flag) { OBJECT_STRUCT.flags |= flag; };
	void  ClearFlag(DWORD flag) { OBJECT_STRUCT.flags &= ~flag; };
	void  SetDirection(BYTE new_dir) {if (new_dir <= 6) OBJECT_STRUCT.dir = new_dir;};
	// Функция возвращает имя параметра с порядковым номером index
	void  GetParamName(String & out, BYTE index);
	void  GetFlagName(String & out, BYTE index);
	// Значение параметра под номером index
    DWORD GetParamValue(BYTE index);
    void  SetParamValue(BYTE index, void * data);
    void  ReLoadPROTOs(void);
    void  ReLoadFRMs(void);
	bool  SaveObject(BOOL db, HANDLE h_map = NULL)
	{
		return db?SaveToDB():SaveToFile(h_map);
    }

    void     ClearChilds(void);
    DWORD    GetChildCount(void)   const {return childs.size();};
    OBJECT * GetChild(DWORD i) const {return childs[i];};
    OBJECT * AddChildObject(BYTE nType, WORD nProtoIndex, DWORD count);
    void     AddChildObject(OBJECT * pChild);
	void     DeleteChild(OBJECT * pChild);

	DWORD 	 GetPos(){return GetHexY()*200+GetHexX();}
};

class CObjSet
{
private:
    CHeader             *pHeader;
    vector<OBJECT*>      pObjects;
    vector<OBJECT*>      pSelObjs;
    CScriptBuf          *pScriptBuf;

    DWORD LoadFromFile_Fallout(HANDLE h_map);        // Загрузка из файла Fallout
    DWORD LoadFromFile_FOnline(HANDLE h_map);

    BYTE GetObjectLen(void);

    int     MinY, MaxY, MinX, MaxX;
    int     width, height;

public:

    bool bError;
    CObjSet(CHeader * hdr);
    ~CObjSet();

    // Устанавливает указатель файла на начало области с объектами
    int FindStartObjects(HANDLE h_map);

    void CreateNewObjSet();
    DWORD LoadFromFile(HANDLE h_map);
    bool SaveToFile(HANDLE h_map);
    int  GetObjCount(bool with_childs) const
    {
        if (!with_childs)
            return pObjects.size();
        OBJECT * pObj;
        int count = 0;
        for (int i = 0; i < pObjects.size(); i++)
        {
            pObj = pObjects[i];
            count += 1 + pObj->GetChildCount();
        }
        return count;
    };
    void ReLoadPROs(void);
    void ReLoadFRMs(void);

    BOOL    ObjIsSelected(OBJECT * pObj);
    BOOL    SelectObject(DWORD index, BOOL sel);
    DWORD   GetSelectCount(void)  const {return pSelObjs.size();};
    OBJECT * GetSelectedObject(DWORD index);
    //OBJECT* GetLastSelected(void) const {return pSelObjs.back();};
    void    ClearSelection();

    // Возращает указатель на объект типа nType и с индексом в массиве index
    OBJECT * GetObject(DWORD index);
	void	 AddObject(WORD HexX, WORD HexY, BYTE nType, WORD nProtoIndex);
	// Создает копию объекта pObj и сохраняет в списке.
	// Возвращает свой индекс в списке объектов
	int      AddObject(OBJECT * pObj, bool sort);
    void     CopySelectedTo(CObjSet * pOS, bool with_childs);
    void     CopyTo(CObjSet * pOS, bool with_childs);
    void     MoveSelectedTo(int x, int y);
    void     FindUpRightObj(void);

    void     DeleteObject(DWORD num);
    void     DeleteSelected(void);

    void    ClearObjSet(void);
};

//---------------------------------------------------------------------------
#endif

