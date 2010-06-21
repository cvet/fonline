//---------------------------------------------------------------------------
#ifndef mapH
#define mapH

#include <classes.hpp>
#include "utilites.h"
#include "macros.h"

extern CUtilites    *pUtil;

class CHeader
{
private:
   struct
   {
		DWORD ver;               // 0x00  Версия карты.

		WORD MapPID;             // 0x04 ID прототипа карты

		//!Cvet для тайлов, сценери, объектов своя версия
		DWORD ver_tile;         //0x06
		DWORD ver_scen;   		//0x0A
		DWORD ver_obj;          //0x0E

		DWORD PlLimit;          //0x12

		DWORD Reserved;			//0x16 CRC of header

		WORD  DefPlX[MAX_GROUPS_ON_MAP];			//0x1A..0x2C Старт игрока по X
		WORD  DefPlY[MAX_GROUPS_ON_MAP];            //0x2E..0x40 Старт игрока по Y
		BYTE  DefPlDirection[MAX_GROUPS_ON_MAP];    //0x42..0x4B Направление игрока по умолчанию.

		DWORD buffer[5];       //0x4C..0x60 !Cvet обязательно выровнить до кратных 4

		DWORD CRC;			   //0x16 CRC of header

		//0x64 end of header
   } header;                    // Заголовок FOnline

   WORD width, height;          // Размеры.

// Для совместимости с пред. версиями фола
   struct
   {
	  DWORD ver;                // 0x0000  Версия карты.
	  char mapname[16];         // 0x0004  Имя карты
	  DWORD DefPlPos;           // 0x0014  Стартовая позиция NPC
	  DWORD DefMapElev;         // 0x0018  Уровень по дефулту.
	  DWORD DefPlDirection;     // 0x001C  Направление игрока по умолчанию.
	  DWORD Num1;               // 0x0020  Количество локальных переменных
	  DWORD MapScriptID;        // 0x0024  Скрипт карты.
	  DWORD TilesCountID;       // 0x0028
	  DWORD Unknown4;           // 0x002C  1
	  DWORD Num2;               // 0x0030 :: #Map Global Vars (*.gam)
	  DWORD MapID;              // 0x0034
	  DWORD GameStartDate;      // 0x0038 :: Time since the epoch.
	  DWORD Unknown6[44];       // 0x003C -> 0x00EB
   } mapvars;                   // Заголовок стандартного Fallout 1/2

   DWORD        *pMapGVars;     // Глобальные переменные
   DWORD        *pMapLVars;     // Локальные переменные

   DWORD        Levels;         // Число уровней на карте

    // Пересчет контрольной суммы
    DWORD ReCalcCRC(void);
    void  ResetAll(void);
    bool  LoadFromFile_Fallout(HANDLE h_map);
    bool LoadFromFile_FOnline(HANDLE h_map);
 public:
    bool bError;                         // Факт наличия ошибки

    CHeader(void);                       // Конструктор
    virtual ~CHeader(void);              // Деструктор
    void CreateNewMap(void);             // Создает новую карту
    void SaveToFile(HANDLE h_map);       // Сохранение карты в файле
    void LoadFromFile(HANDLE h_map);     // Загрузка карты из файла

	DWORD GetVersion()			const {return header.ver;};

	WORD GetMapID()             const {return header.MapPID;};
 
//    DWORD GetMapScriptID()       const {return header.MapScriptID; };
//    void  GetMapName(char * out) const {strcpy(out, header.mapname);};

	WORD GetStartX(WORD num)    const {return num>=MAX_GROUPS_ON_MAP?0:header.DefPlX[num];}; //!Cvet add MAX_GROUPS_ON_MAP
	WORD GetStartY(WORD num)    const {return num>=MAX_GROUPS_ON_MAP?0:header.DefPlY[num];}; //!Cvet add MAX_GROUPS_ON_MAP
	DWORD GetStartDir(WORD num) const {return num>=MAX_GROUPS_ON_MAP?0:header.DefPlDirection[num];}; //!Cvet add MAX_GROUPS_ON_MAP

	//!Cvet своя версия для всего
	DWORD GetVersion_tile()     const {return header.ver_tile;};
	DWORD GetVersion_scen()     const {return header.ver_scen;};
	DWORD GetVersion_obj()      const {return header.ver_obj;};

	DWORD GetWidth()            const {return width;};
	DWORD GetHeight()           const {return height;};

	DWORD GetLevels()           const {return Levels;};

    // версия карты
    void SetVersion(DWORD version) {header.ver = version;};
    // Используеются для установки нового размера карты (в тайлах).
    // В случае преобразования из формата Fallout к FOnline
    void  SetWidth(WORD w);
    void  SetHeight(WORD h);
   // Новое название карты
//   void SetMapName(char * name);
   // Номер скрипта
   void SetMapScriptID(DWORD id);
   // Номер карты
   void SetMapID(WORD id);
	// Позиция игрока на карте
   void SetStartPos(WORD num, WORD x, WORD y, WORD dir); //!Cvet add WORD num
};
//---------------------------------------------------------------------------

#endif
