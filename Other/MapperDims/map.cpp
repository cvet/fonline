//////////////////////////////////////////////////////////////////////
// CMap Class
//////////////////////////////////////////////////////////////////////

#include "map.h"
#include "mdi.h"
#include "utilites.h"
#include "macros.h"

//---------------------------------------------------------------------------
CHeader::CHeader(void)
{
    pMapGVars   = NULL;
    pMapLVars   = NULL;
    ResetAll();
    SetWidth(FO_MAP_WIDTH);
    SetHeight(FO_MAP_HEIGHT);
}

void CHeader::ResetAll(void)
{
    // Чистка памяти
    memset(&header, 0, sizeof(header));
    bError = false;
    Levels = 1;
    _DELETE(pMapGVars);
    _DELETE(pMapLVars);
}

//---------------------------------------------------------------------------
CHeader::~CHeader(void)
{
    _DELETE(pMapGVars);
    _DELETE(pMapLVars);
}
//---------------------------------------------------------------------------
void CHeader::CreateNewMap(void)
{
    ResetAll();
	header.ver = FO_MAP_VERSION;
    // Установление тайлов в 1.
}
//---------------------------------------------------------------------------
DWORD CHeader::ReCalcCRC(void)
{
    DWORD ret = 0;
    for (int i = 0; i < sizeof(header) - 4; i++)
        ret +=  ((BYTE*)&header)[i];
    return ret;
}
//---------------------------------------------------------------------------
void CHeader::SaveToFile(HANDLE h_map)
{
	header.ver_tile++;
	header.ver_obj++;
	header.ver_scen++;

	header.PlLimit=20;

	header.CRC = ReCalcCRC();

	DWORD i;
    WriteFile(h_map, &header, sizeof(header), &i, NULL);
//    if (header.dwGlobalVars)
//    WriteFile(h_map, pMapGVars, header.dwGlobalVars * 4, &i, NULL);
//    WriteFile(h_map, pMapLVars, header.dwLocalVars  * 4, &i, NULL);
}
//---------------------------------------------------------------------------
bool CHeader::LoadFromFile_FOnline(HANDLE h_map)
{
    DWORD i;
    ReadFile(h_map, &header, sizeof(header), &i, NULL);
    if (i != sizeof(header)) return false;
//    DWORD crc = ReCalcCRC();
    // Проверяем контрольную сумму.
//    if ( crc != header.CRC  ) return false;

/*    if (header.dwGlobalVars != 0)
    {
        // Грузим глоб. переменные
        pMapGVars = new DWORD[header.dwGlobalVars];
        ReadFile(h_map, pMapGVars, header.dwGlobalVars * 4, &i, NULL);
        if ( i != header.dwGlobalVars * 4 ) return false;
    }

    if (header.dwLocalVars != 0)
    {
        // Грузим лок. переменные
        pMapLVars = new DWORD[header.dwLocalVars];
        ReadFile(h_map, pMapLVars, header.dwLocalVars * 4, &i, NULL);
        if ( i != header.dwLocalVars * 4 ) return false;
    }  */

/*    if (!header.MapWidth  || header.MapWidth  == 0xffffffff ||
        !header.MapHeight || header.MapHeight == 0xffffffff)
    {
        header.MapWidth  = FO_MAP_WIDTH;
        header.MapHeight = FO_MAP_HEIGHT;
    }
 */
    return true;
}
//---------------------------------------------------------------------------
bool CHeader::LoadFromFile_Fallout(HANDLE h_map)
{
    ULONG i;
	pMapGVars = NULL;
	pMapLVars = NULL;

	ReadFile(h_map, &mapvars, sizeof(mapvars), &i, NULL);

	DWORD  dwLocalVars = pUtil->GetDW(&mapvars.Num1);   // 0x0020 #Map Local Vars;
    DWORD dwGlobalVars = pUtil->GetDW(&mapvars.Num2);  // 0x0030 #Map Global Vars(*.gam);

	if (dwGlobalVars != 0)
    {
         // Грузим глоб. переменные
		 pMapGVars = new DWORD[dwGlobalVars];
         ReadFile(h_map, pMapGVars, dwGlobalVars * 4, &i, NULL);
    }

	if (dwLocalVars != 0)
    {
         // Грузим лок. переменные
         pMapLVars = new DWORD[dwLocalVars];
         ReadFile(h_map, pMapLVars, dwLocalVars * 4, &i, NULL);
    }

//   dwTilesCount = pUtil->GetDW(&mapvars.TilesCountID);
    DWORD dwTilesCount = pUtil->GetDW(&mapvars.TilesCountID) &0xE;

    switch (dwTilesCount)
    {
       case 0x0c :  Levels = 1; break;
       case 0x08 :  Levels = 2; break;
       case 0x00 :  Levels = 3; break;
       default : return false;
    }

    // Теперь нужно сохранить инфу в header
	header.ver = FO_MAP_VERSION;

	memset(&header.DefPlX[0], 0, sizeof(header.DefPlX[0])*MAX_GROUPS_ON_MAP); //!Cvet
	memset(&header.DefPlY[0], 0, sizeof(header.DefPlY[0])*MAX_GROUPS_ON_MAP); //!Cvet
	memset(&header.DefPlDirection[0], 0, sizeof(header.DefPlDirection[0])*MAX_GROUPS_ON_MAP); //!Cvet
	header.DefPlX[0]  = ((BYTE*)&mapvars.DefPlPos)[2];
	header.DefPlY[0]  = ((BYTE*)&mapvars.DefPlPos)[3];
	header.DefPlDirection[0] = pUtil->GetDW(&mapvars.DefPlDirection);

//    header.dwGlobalVars = header.dwGlobalVars;
//    header.dwLocalVars  = header.dwLocalVars;
//    header.MapScriptID   = pUtil->GetDW(&mapvars.MapScriptID);
    header.MapPID         = pUtil->GetDW(&mapvars.MapID);
//    header.MapWidth      = FO_MAP_WIDTH;
//    header.MapHeight     = FO_MAP_HEIGHT;
//    memcpy(header.mapname, mapvars.mapname, sizeof(mapvars.mapname));
    return true;
}
//---------------------------------------------------------------------------
void CHeader::SetWidth(WORD w)
{
    if (w <= 200) width = w;
}
//---------------------------------------------------------------------------
void CHeader::SetHeight(WORD h)
{
    if (h <= 200) height = h;
}
//---------------------------------------------------------------------------
void CHeader::LoadFromFile(HANDLE h_map)
{
    ResetAll();
    bError  = true;
    DWORD i;
    DWORD dwVersion;

    // Загружаем версию карты
    ReadFile(h_map, &dwVersion, sizeof(dwVersion), &i, NULL);
	if (i != sizeof(dwVersion)) return;

    // Перемещаем указатель на начало карты
	SetFilePointer(h_map, 0, NULL, FILE_BEGIN);
#ifdef MAPPER
    if ( dwVersion == FO_MAP_VERSION )
    {
        // Грузим карту FOnline
		LoadFromFile_FOnline(h_map);
        // В оригинальном фоле данные в файл записаны в обратном порядке
	}
	else if ( dwVersion == F1_MAP_VERSION || dwVersion == F2_MAP_VERSION )
    {
		// Грузим карту оригинального Fallout
		if (!LoadFromFile_Fallout(h_map))  return;
    }
#else
    // Мы не в режиме редактора
    if (ver != FO_MAP_VERSION) return;
    if (!LoadFromFile_FOnline(h_map)) return;
#endif
	header.ver = dwVersion;

    // Проверяем считанные данные
	if (header.DefPlX[0] >= GetWidth()  * 2 ||
		header.DefPlY[0] >= GetHeight() * 2 ||
		header.DefPlDirection[0] > 5) return;
		
    bError = false;
}
//---------------------------------------------------------------------------
/*void CHeader::SetMapName(char * name)
{
//    int len = strlen(name);
//    if ( len > 15 ) len = 15;
//    memcpy(&header.mapname, name, len);
} */
//---------------------------------------------------------------------------
void CHeader::SetMapID(WORD id)
{
	header.MapPID = id;
}
//---------------------------------------------------------------------------
void CHeader::SetMapScriptID(DWORD id)
{
//    header.MapScriptID = id;
}
//---------------------------------------------------------------------------
void CHeader::SetStartPos(WORD num, WORD x, WORD y, WORD dir) //!Cvet add WORD num
{
	bError = true;
	if(num>=MAX_GROUPS_ON_MAP) return; //!Cvet
    if ( x > GetWidth() * 2 || y > GetHeight() * 2 || dir > 5) return;
	header.DefPlX[num] = x;
	header.DefPlY[num] = y;
	header.DefPlDirection[num] = dir;
    bError = false;
}
//---------------------------------------------------------------------------



