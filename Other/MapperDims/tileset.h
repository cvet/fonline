//---------------------------------------------------------------------------
#ifndef tilesetH
#define tilesetH

#include <classes.hpp>
#include "lists.h"
#include "frmset.h"
#include "utilites.h"

//#define MAP_SIZE_FONLINE_X 100
//#define MAP_SIZE_FONLINE_Y 100

class CFrmSet;
class CProSet;
class CObjSet;

extern CUtilites            *pUtil;
extern CListFiles           *pLstFiles;     // Список файлов
extern CFrmSet              *pFrmSet;       // Список кадров.

struct TILES
{
   WORD roof;                        // Номер тайла крыши
   WORD floor;                       // Номер тайлы пола
};

// Переделано под FOnline
class CTileSet
{
public:
    BOOL    bError;

    // Возвращают ID пола и потолка с координатами X, Y
    WORD GetRoofID(int x, int y);
    WORD GetFloorID(int x, int y);
    // Устанавливают tileID
    void SetFloor(int x, int y, WORD tileID);
    void SetRoof (int x, int y, WORD tileID);
    void SetFloorRegion(int X1, int Y1, int X2, int Y2, WORD tileID);
    void SetRoofRegion(int X1, int Y1, int X2,  int Y2, WORD tileID);
    void SelectTiles(int TileMode, int TileX1,  int TileY1,
                                   int TileX2, int TileY2);

    bool FloorIsSelected(int TileX, int TileY);
    bool RoofIsSelected(int TileX, int TileY);
    void ClearFloorSelection(void);
    void ClearRoofSelection(void);

    CTileSet(CHeader * pHeader);
    void CreateNewTileSet();
    void LoadFromFile(HANDLE h_map);
    void SaveToFile(HANDLE h_map);
    virtual ~CTileSet();

    void ReLoadFRMs(void);
    // Установка нового размера
    void Resize(WORD newwidth, WORD newheight);
    // Заполняет тайлсет pTS выделением.
    void CopySelectedTo(CTileSet * pTS, WORD StartX, WORD StartY, BYTE tile_set);

    void CopyTo(CTileSet * pTS, int StartX, int StartY, BYTE tile_set);

    DWORD dwSelection;                   // Что выбрано
    int SelectedFloorX1, SelectedFloorY1, SelectedFloorX2, SelectedFloorY2;
    int SelectedRoofX1, SelectedRoofY1, SelectedRoofX2, SelectedRoofY2;

    WORD    GetWidth() const {return width;};
    WORD    GetHeight() const {return height;};

protected:
    TILES                *pTile;           
    CHeader              *pHeader;       // Заголовок карты

    DWORD                 width, height;
};
//---------------------------------------------------------------------------
#endif
