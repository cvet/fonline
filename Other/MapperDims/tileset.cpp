//////////////////////////////////////////////////////////////////////
// CTileSet Class
//////////////////////////////////////////////////////////////////////

#include "tileset.h"
#include "mdi.h"
#include "lists.h"
#include "frmset.h"
#include "utilites.h"
#include "macros.h"
#include "template.h"
//---------------------------------------------------------------------------
CTileSet::CTileSet(CHeader * hdr)
{
    ULONG i;
    pHeader              = hdr;
	width                = pHeader->GetWidth();
    height               = pHeader->GetHeight();

    pTile                = NULL;
}
//---------------------------------------------------------------------------
void CTileSet::LoadFromFile(HANDLE h_map)
{
    DWORD x, y;
    DWORD wi, he;

    bError = true;
    // Считываем тайлы из файла
    DWORD i;

    CreateNewTileSet();

  //   DWORD buffer[20000];

    if (pHeader->GetVersion() != FO_MAP_VERSION &&
        pHeader->GetVersion() != FO_MAP_EDITOR_VERSION)
    {
        he = 100;
        wi = 100;
    } else
    {
        he = height;
        wi = width;
    }

    for (y = 0; y < he; y++) {
        // Считываем по-строчно
        ReadFile(h_map, &pTile[y * width], wi * 4, &i, NULL);
 //       ReadFile(h_map, buffer, pHeader->GetWidth() * 4, &i, NULL);
        if (i != (wi * 4))
            return;
    }

    if (pHeader->GetVersion() != FO_MAP_VERSION &&
        pHeader->GetVersion() != FO_MAP_EDITOR_VERSION)
    {
        // Карты оригинального фола меньше.
        for (y = 0; y < he; y++) {
            for (x = 0; x < wi; x++) {
                pTile[y * width + x].floor = pUtil->GetW(&(pTile[y * width + x].floor));
                pTile[y * width + x].roof  = pUtil->GetW(&(pTile[y * width + x].roof));
            }
        }
    }

    bError = false;
}
//---------------------------------------------------------------------------
void CTileSet::Resize(WORD nw, WORD nh)
{
    if (!nw || !nh ||
         nw > FO_MAP_WIDTH || nh > FO_MAP_HEIGHT || !pTile) return;

    TILES * ntiles = new TILES[nw * nh];
    for (DWORD y = 0; y < nh; y++)
        for (DWORD x = 0; x < nw; x++)
        {
            ntiles[y * nw + x].roof =  2;//pTile[y * width + x].roof;
            ntiles[y * nw + x].floor = 2;//pTile[y * width + x].floor;
        }
    delete [] pTile;
    // Сохраняем
    pTile = ntiles;

    width = nw;
    height = nh;
}
//---------------------------------------------------------------------------
void CTileSet::CreateNewTileSet()
{
    bError = true;

    ClearFloorSelection();
    ClearRoofSelection();

    pTile   = new TILES[width * height];

    DWORD x,y;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pTile[y * width + x].floor = 1;
            pTile[y * width + x].roof  = 1;
        }
    }
    bError = false;
}
void CTileSet::CopyTo(CTileSet * pTS, int StartX, int StartY, BYTE tile_set)
{
    DWORD w, h;
    int x,y;
    w = width;
    h = height;

    if (tile_set == COPY_FLOOR)
    {

        if (w > pTS->width - StartX)  w = pTS->width - StartX;
        if (h > pTS->height - StartY) h = pTS->height - StartY;

        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
            {
                if (x + StartX < 0 || x + StartX >= pTS->GetWidth() ||
                    y + StartY < 0 || y + StartY >= pTS->GetHeight())
                        continue;
                pTS->SetFloor(x + StartX, y + StartY,this->GetFloorID(x, y));
            }
    }

    if (tile_set == COPY_ROOF)
    {
        if (w > pTS->width - StartX)  w = pTS->width - StartX;
        if (h > pTS->height - StartY) h = pTS->height - StartY;

        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
            {
                if (x + StartX < 0 || x + StartX >= pTS->GetWidth() ||
                    y + StartY < 0 || y + StartY >= pTS->GetHeight())
                        continue;
                pTS->SetRoof(x + StartX, y + StartY,this->GetRoofID(x, y));
            }
    }
}
//---------------------------------------------------------------------------
void CTileSet::CopySelectedTo(CTileSet * pTS, WORD StartX, WORD StartY, BYTE tile_set)
{
    if (!pTS || dwSelection == NONE_SELECTED) return;
    DWORD x,y;

    DWORD w, h;

    if (tile_set == COPY_FLOOR)
    {
        h = SelectedFloorY2 - SelectedFloorY1 + 1;
        w = SelectedFloorX2 - SelectedFloorX1 + 1;

        if (w > pTS->width - StartX)  w = pTS->width - StartX;
        if (h > pTS->height - StartY) h = pTS->height - StartY;

        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                pTS->SetFloor(x + StartX, y + StartY,this->GetFloorID(x + SelectedFloorX1, y + SelectedFloorY1));
    }

    if (tile_set == COPY_ROOF)
    {
//      StartX -= 2;
//      StartY -= 3;
        h = SelectedRoofY2 - SelectedRoofY1 + 1;
        w = SelectedRoofX2 - SelectedRoofX1 + 1;

        if (w > pTS->width - StartX)  w = pTS->width - StartX;
        if (h > pTS->height - StartY) h = pTS->height - StartY;

        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
//                pTS->pTile[]
                pTS->SetRoof(x + StartX, y + StartY,this->GetRoofID(x + SelectedRoofX1, y+ SelectedRoofY1));
    }
}

//---------------------------------------------------------------------------
WORD CTileSet::GetRoofID(int x, int y)
{
   return pTile[y * width + x].roof;
}
//---------------------------------------------------------------------------
WORD CTileSet::GetFloorID(int x, int y)
{
   return pTile[y * width + x].floor;
}
//------------------------------------------------------------------------------
void CTileSet::SetFloor(int x, int y, WORD tileID)
{
    pFrmSet->LoadFRM(tile_ID, tileID, true);
    pTile[y * width + x].floor = tileID;
}
//------------------------------------------------------------------------------
void CTileSet::SetRoof(int x, int y, WORD tileID)
{
    pFrmSet->LoadFRM(tile_ID, tileID, true);
    pTile[y * width + x].roof = tileID;
}
//------------------------------------------------------------------------------
void CTileSet::SetFloorRegion(int X1, int Y1, int X2, int Y2, WORD tileID)
{
    pFrmSet->LoadFRM(tile_ID, tileID, true);
    for (int y = Y1; y <= Y2; y++)
        for (int x = X1; x <= X2; x++)
            pTile[y * width + x].floor = tileID;
}
//------------------------------------------------------------------------------
void CTileSet::SetRoofRegion(int X1, int Y1, int X2, int Y2, WORD tileID)
{
    pFrmSet->LoadFRM(tile_ID, tileID, true);
    for (int y = Y1; y <= Y2; y++)
        for (int x = X1; x <= X2; x++)
            pTile[y * width + x].roof = tileID;
}
//------------------------------------------------------------------------------
void CTileSet::SelectTiles(int TileMode, int TileX1, int TileY1,
                                         int TileX2, int TileY2)
{
    DWORD t;
    if (TileX1 > TileX2)
    {
        t       = TileX1;
        TileX1  = TileX2;
        TileX2  = t;
    }
    if (TileY1 > TileY2)
    {
        t       = TileY1;
        TileY1  = TileY2;
        TileY2  = t;
    }

    if (TileMode == SELECT_FLOOR)
    {
        SelectedFloorX1 = TileX1;
        SelectedFloorY1 = TileY1;
        SelectedFloorX2 = TileX2;
        SelectedFloorY2 = TileY2;
        dwSelection = floor_ID;
    }
    else
    {
        SelectedRoofX1 = TileX1 + 2;
        SelectedRoofY1 = TileY1 + 3;
        SelectedRoofX2 = TileX2 + 2;
        SelectedRoofY2 = TileY2 + 3;
        dwSelection = roof_ID;
    }
}
//------------------------------------------------------------------------------
bool CTileSet::FloorIsSelected(int TileX, int TileY)
{
   return ((TileX >= SelectedFloorX1 && TileX <= SelectedFloorX2) &&
           (TileY >= SelectedFloorY1 && TileY <= SelectedFloorY2));
}
//------------------------------------------------------------------------------
bool CTileSet::RoofIsSelected(int TileX, int TileY)
{
   return ((TileX >= SelectedRoofX1 && TileX <= SelectedRoofX2) &&
           (TileY >= SelectedRoofY1 && TileY <= SelectedRoofY2));
}
//------------------------------------------------------------------------------
void CTileSet::ClearFloorSelection(void)
{
   SelectedFloorX1 = -1;
   SelectedFloorY1 = -1;
   SelectedFloorX2 = -1;
   SelectedFloorY2 = -1;
   dwSelection = NONE_SELECTED;
}
//------------------------------------------------------------------------------
void CTileSet::ClearRoofSelection(void)
{
   SelectedRoofX1 = -1;
   SelectedRoofY1 = -1;
   SelectedRoofX2 = -1;
   SelectedRoofY2 = -1;
   dwSelection = NONE_SELECTED;
}
//------------------------------------------------------------------------------
void CTileSet::SaveToFile(HANDLE h_map)
{
    bError = true;
    // Реализаация только под Fonline
    if (pHeader->GetVersion() != FO_MAP_VERSION) return;
    DWORD i;
	for (DWORD y = 0; y < height; y++)
 //	{
		WriteFile(h_map, &pTile[y * width], width * 4, &i, NULL);
 //		if(i!=width*4) return;
 //	}

	bError = false;
}
//---------------------------------------------------------------------------
void CTileSet::ReLoadFRMs(void)
{
    DWORD x,y;
    for (y = 0; y < height; y++)
    {
		for (x = 0; x < width; x++)
        {
            // Загружаем фрейм для пола
            DWORD nFrmID = GetFloorID(x, y);
            pFrmSet->LoadFRM(tile_ID, nFrmID, true);
            // Загружаем фрейм для потолка
            nFrmID = GetRoofID(x, y);
            pFrmSet->LoadFRM(tile_ID, nFrmID, true);
            frmMDI->iPos++;
        }
        Application->ProcessMessages();
    }
}

//---------------------------------------------------------------------------
CTileSet::~CTileSet()
{
    if (pTile) delete [] pTile;
}
//---------------------------------------------------------------------------

