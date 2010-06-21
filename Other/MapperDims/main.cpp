//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include <stdlib.h>
#include "main.h"                                                                                 
#include "mdi.h"
#include "utilites.h"                                 
#include "log.h"
#include "map.h"
#include "lists.h"                    
#include "frmset.h"
#include "frame.h"
#include "tileset.h"
#include "pal.h"
#include "macros.h"
#include "msg.h"
#include "change.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "frameHeader"
#pragma link "frmInven"
#pragma link "frmObj"
#pragma link "frmMiniMap"
#pragma resource "*.dfm"

TfrmEnv *frmEnv;
//---------------------------------------------------------------------------
__fastcall TfrmEnv::TfrmEnv(TComponent* Owner)
        : TForm(Owner)
{
    pBufFloor           = NULL;
    pBufRoof            = NULL;
    pBufObjSet          = NULL;
    pBufFloorHeader     = NULL;
    pBufRoofHeader      = NULL;
    pBufObjHeader       = NULL;

    dds          = NULL;
    dds2Map      = NULL;
    dds2Nav      = NULL;
    dds2Inv      = NULL;
    dds2Obj      = NULL;
    ddcMap       = NULL;
    ddcNav       = NULL;
    ddcInv       = NULL;
    ddcObj       = NULL;

    panelTILE        = (TStatusPanel *)sbar->Panels->Items[0];
    panelHEX         = (TStatusPanel *)sbar->Panels->Items[1];
    panelObjCount    = (TStatusPanel *)sbar->Panels->Items[2];
    panelObjSelected = (TStatusPanel *)sbar->Panels->Items[3];
    panelMSG         = (TStatusPanel *)sbar->Panels->Items[4];

    pLog->LogX("Attach DDraw primary surface ...");
    AttachDDraw (this, &dds, 1);
    pLog->LogX("Attach DDraw map surface ...");
    AttachDDraw (imgMap, &dds2Map, 0);
    pLog->LogX("Attach DDraw navigator surface ...");
    AttachDDraw (imgObj, &dds2Nav, 0);
    pLog->LogX("Attach DDraw inventory surface ...");
    AttachDDraw (frmInv1->imgInv, &dds2Inv, 0);
    pLog->LogX("Attach DDraw object editor surface ...");
    AttachDDraw (frmObj1->imgObj, &dds2Obj, 0);

    frmMMap1->Visible   = false;
    frmHdr1->Visible    = false;
    frmInv1->Visible    = false;
    frmObj1->Visible    = false;
    UpdatePanels();

    Edit1->Width = 0;

    bShowObj[item_ID]            = frmMDI->btnItems->Down;
    bShowObj[critter_ID]         = frmMDI->btnCritters->Down;
    bShowObj[scenery_ID]         = frmMDI->btnScenery->Down;
    bShowObj[wall_ID]            = frmMDI->btnWalls->Down;
    bShowObj[floor_ID]           = frmMDI->btnFloor->Down;
    bShowObj[misc_ID]            = frmMDI->btnMisc->Down;
	bShowObj[roof_ID]            = frmMDI->btnRoof->Down;
	bShowObj[TG_blockID]         = frmMDI->btnTGBlock->Down;
    bShowObj[EG_blockID]         = frmMDI->btnEGBlock->Down;
    bShowObj[SAI_blockID]        = frmMDI->btnSaiBlock->Down;
    bShowObj[wall_blockID]       = frmMDI->btnWallBlock->Down;
    bShowObj[obj_blockID]        = frmMDI->btnObjBlock->Down;
    bShowObj[light_blockID]      = frmMDI->btnLightBlock->Down;
    bShowObj[scroll_blockID]     = frmMDI->btnScrollBlock->Down;
    bShowObj[obj_self_blockID]   = frmMDI->btnObjSelfBlock->Down;

    fAspect = 1;

    if (pUtil->GetScreenWidth() == 1024)
    {
        ObjsInPanel = 12;
    }
    else
    {
        ObjsInPanel = 9;
    }


    imgMap->Width = pUtil->GetScreenWidth();
    imgMap->Height = pUtil->GetScreenHeight();
    shp->Width = pUtil->GetScreenWidth() + 8;
    shp->Height = pUtil->GetScreenHeight() + 8;
    Shape3->Width = pUtil->GetScreenWidth() + 8;
    Label7->Left = pUtil->GetScreenWidth() / 2 - 50;

    /*
    btnWorld->Left = 13;
    btnLocal->Left = btnWorld->Left + 1;
    Shape8->Left = btnWorld->Left;
    imgObj->Left = 13;
    Shape5->Left = 11;
     */
    Shape5->Width = imgMap->Width - 5 + 5;
    imgObj->Width = ObjsInPanel * 83;
    // ObjsInPanel * 83
    sb->Left = Shape5->Left;
    sb->Width = Shape5->Width;

    btnLocal->Top = shp->Top + shp->Height + 2;
    btnWorld->Top = btnLocal->Top;
    Shape8->Top = btnWorld->Top;
    imgObj->Top = btnWorld->Top + 24;
    Shape5->Top = btnWorld->Top + 23;
    sb->Top = Shape5->Top + Shape5->Height + 1;
    tabc->Top = btnWorld->Top + 3;
    tabc->Left = Shape5->Left + Shape5->Width - tabc->Width - 5;

    frmEnv->AutoSize = true;
    sbar->Align = alNone;
    sbar->Top = sb->Top + sb->Height + 5;
    sbar->Width = frmEnv->Width;
 //   sbar->Realign();
    Shape4->Left = shp->Left + shp->Width + 7;

    frmEnv->Width = pUtil->GetScreenWidth();
    frmEnv->Height = pUtil->GetScreenHeight();
    UpdatePanels();
}

void TfrmEnv::AddMap(CMap* map, bool set)
{
    if (!map) return;
    String name = map->GetFileName();
    String str;
    for (int i =0; i < frmHdr1->lbMaps->Items->Count; i++)
    {
        str = frmHdr1->lbMaps->Items->Strings[i];
        if (name == str)
        {
            MessageDlg("Данная карта уже открыта.", mtError, TMsgDlgButtons() << mbOK, 0);
            delete map;
            return;
        }
    }
    MapList.push_back(map);
    frmHdr1->lbMaps->Items->Add(name);
    frmHdr1->lbMaps->ItemIndex = frmHdr1->lbMaps->Items->Count-1;
    if (set)
    {
        BYTE index = MapList.size()-1;
        SetMap(index);
    }
}

void TfrmEnv::DeleteMap(int index)
{
    CMap * map = MapList[index];
    vector<CMap*>::iterator p = MapList.begin();
    while (p != MapList.end())
    {
        if (*p == map)
        {
            delete map;
            MapList.erase(p);
            return;
        }
        p++;
    }
}

void TfrmEnv::SetMap(int index)
{
    if (index > MapList.size() - 1 ) return;
    // Если добавляется первая карта, то нужно инициализировать DDraw
    if (!pMap)
    {
        imgMap->Cursor = (TCursor)crHandCursor;
        OldMapWndProc = imgMap->WindowProc;
        imgMap->WindowProc = NewMapWndProc;
        OldNavWndProc = imgObj->WindowProc;
        imgObj->WindowProc = NewNavWndProc;
        OldInvWndProc = frmInv1->imgInv->WindowProc;
        frmInv1->imgInv->WindowProc = NewInvWndProc;
        OldObjWndProc = frmObj1->imgObj->WindowProc;
        frmObj1->imgObj->WindowProc = NewObjWndProc;

        pBMPlocator = new Graphics::TBitmap();
        pBMPlocator->LoadFromResourceName((UINT)HInstance, "locator");
        pBMPHex     = new Graphics::TBitmap();
        pBMPHex->LoadFromResourceName((UINT)HInstance, "hex");

    } else
    {
        pMap->WorldX = WorldX;
        pMap->WorldY = WorldY;
    }
    pMap     = MapList[index];
    pOS      = pMap->GetObjSet();
    pTileSet = pMap->GetTileSet();

    OldX     = -(int)pMap->GetWidth();
    OldY     = -(int)pMap->GetHeight();

    WorldX   = pMap->WorldX;
    WorldY   = pMap->WorldY;

    ChangeMode(SELECT_NONE);
    frmMDI->SpeedButton1->Down = true;

    Label7->Caption = "Карта - " + pMap->GetFileName();
    frmMDI->OpenDialog1->InitialDir = pMap->GetDirName();
    frmMDI->SaveDialog1->InitialDir = pMap->GetDirName();

    CHeader * pHeader = pMap->GetHeader();

	frmHdr1->editNumSP->Text	= String("0"); //!Cvet
	frmHdr1->editStartX->Text   = String(pHeader->GetStartX(0));
	frmHdr1->editStartY->Text   = String(pHeader->GetStartY(0));
	frmHdr1->editStartDir->Text = String(pHeader->GetStartDir(0));
    char buf[16];
    sprintf(buf,"0x%.8X", pHeader->GetMapID());
    frmHdr1->editMapID->Text = String(buf);
    frmHdr1->lbMaps->ItemIndex = index;

    fAspect = 1.0f;
    // Перерисовываем
    RedrawMap(true);         // Карту
    ResetInventoryInfo();
    RedrawInventory();       // Инвентарь
    UpdateObjPanel();

    Edit1->SetFocus();

    btnLocalClick(this);
    RedrawNavigator();
    SetButtonSave(pMap->SaveState);
    frmMDI->spbUndo->Enabled = (pMap->GetPos() != -1);
    frmMDI->spbUndo->Hint    = pMap->GetChangeHint();; 
}

//---------------------------------------------------------------------------
void TfrmEnv::LoadMapFromFile()
{

}
//---------------------------------------------------------------------------
void TfrmEnv::CreateNewMap()
{
}

//---------------------------------------------------------------------------
void __fastcall TfrmEnv::FormDestroy(TObject *Sender)
{
    imgMap->WindowProc = OldMapWndProc;
    imgObj->WindowProc = OldNavWndProc;
    if (pBMPlocator) delete pBMPlocator;
    if (pBMPHex)     delete pBMPHex;

   if (dds) dds->Release();
   if (ddc) ddc->Release();

    for (DWORD i =0; i < MapList.size(); i++)
    {
        CMap * map = MapList[i];
        delete map;
    }
    MapList.clear();

//   _RELEASE(pHeader);
//   _RELEASE(pTileSet);
//   _RELEASE(pOS);

   _RELEASE(pBufFloorHeader);
   _RELEASE(pBufRoofHeader);
   _RELEASE(pBufObjHeader);
   _RELEASE(pBufObjSet);
   _RELEASE(pBufFloor);
   _RELEASE(pBufRoof);

   if (ddcInv) ddcInv->Release();
   if (ddcMap) ddcMap->Release();
   if (ddcNav) ddcNav->Release();
   if (ddcObj) ddcObj->Release();

   if (dds2Map) dds2Map->Release();
   if (dds2Nav) dds2Nav->Release();
   if (dds2Inv) dds2Inv->Release();
   if (dds2Obj) dds2Obj->Release();
   frmMDI->tbcurs->Visible      = false;
   frmMDI->tbVis->Visible       = false;
   frmMDI->tbZoom->Visible      = false;
   frmMDI->btnhand->Down        = true;
}
//---------------------------------------------------------------------------
void TfrmEnv::SetButtonSave(bool State)
{
    pMap->SaveState = State;
    if (State == false) frmMDI->btnSave->Enabled = State;
        else
    if (pMap->GetFileName() != "noname.map")
        frmMDI->btnSave->Enabled = State;
}
//---------------------------------------------------------------------------
void TfrmEnv::DrawMiniMap(void)
{
   Graphics::TBitmap *OffMiniMap = new Graphics::TBitmap;
   OffMiniMap->PixelFormat = pf32bit;
   OffMiniMap->IgnorePalette = true;
   OffMiniMap->Width = frmMMap1->imgMiniMap->Width;
   OffMiniMap->Height = frmMMap1->imgMiniMap->Height;
   PatBlt(OffMiniMap->Canvas->Handle, 0, 0, frmMMap1->imgMiniMap->Width,
                                            frmMMap1->imgMiniMap->Height,
                                            BLACKNESS);
   DWORD w = pMap->GetWidth();
   DWORD h = pMap->GetHeight();

   for (DWORD y = 0; y < h ; y++)
   {
      unsigned int *LinePtr = (unsigned int *)OffMiniMap->ScanLine[y];
      for (DWORD x = 0; x < w; x++)
      {
         int FloorId = pTileSet->GetFloorID(x, y);
         if (FloorId != 1)
         {
            LinePtr[frmMMap1->imgMiniMap->Width - x - 1] =
             pPal->GetEntry(pFrmSet->pFRM[tile_ID][FloorId].GetCenterPix(0, 0));
         }
      }
   }
  frmMMap1->imgMiniMap->Canvas->Draw(0, 0, OffMiniMap);
  delete OffMiniMap;
}
//---------------------------------------------------------------------------
void TfrmEnv::PrepareNavigator(BYTE nObjType)
{
   sb->Max = 1; // Обходим ошибку, когда sb->Min > sbMax
   // Подготавливаем scrollbar в навигаторе для работы с объектами
   // Минимум равен 1, т.к. ПРО и ФРМ начинаются с индекса 1
   sb->Min = nObjType == tile_ID ? 0 : 1;
   sb->Position = sb->Min;
   // Проверяем режим показа объектов : локальный или глобальный

   switch (nObjType)
   {
      case tile_ID:
         if (Navigator.nShowMode == SHOW_WORLD)
            Navigator.nCount = pLstFiles->pFRMlst[nObjType]->Count - 1;
         else
            Navigator.nCount = pFrmSet->nFrmCount[nObjType];
         Navigator.nMaxID = pLstFiles->pFRMlst[nObjType]->Count;
         break;
      case inven_ID:
         if (Navigator.nShowMode == SHOW_WORLD)
            Navigator.nCount = pLstFiles->pPROlst[item_ID]->Count;
         else
            Navigator.nCount = pProSet->nProCount[item_ID];
         Navigator.nMaxID = pLstFiles->pPROlst[item_ID]->Count;
         break;
      default:
         if (Navigator.nShowMode == SHOW_WORLD)
            Navigator.nCount = pLstFiles->pPROlst[nObjType]->Count;
         else
            Navigator.nCount = pProSet->nProCount[nObjType];
         Navigator.nMaxID = pLstFiles->pPROlst[nObjType]->Count;
   }
   sb->Max = Navigator.nCount > 12 ? Navigator.nCount - 11 : sb->Min;
   sb->Enabled = sb->Min != sb->Max;
   // Устанавливаем тип отображаемых элементов для навигатора
   Navigator.nObjType = nObjType;
   // Сбрасываем селектирование элементов в навигаторе
   Navigator.nSelID = -1;
   // Установка стиля фонта отображаемого в навигаторе
   imgObj->Font->Name = "Tahoma";
   imgObj->Font->Height = 11;
   imgObj->Font->Style = TFontStyles()<< fsBold;
   imgObj->Brush->Color = clBlack;
   imgObj->Font->Color = clWhite;
}
//---------------------------------------------------------------------------
void TfrmEnv::RedrawNavigator(void)
{
//    return;
    int navX, nID, newWidth, newHeight, nPos, nTemp;
    WORD nFrmID;
    CFrame *frm;
    String frmfile;
    navX = NAV_SIZE_X / 2;
    nPos = 0;
    int cnt = 0;

    nID = 0;
    if (Navigator.nShowMode == SHOW_LOCAL)
    {
       nTemp = Navigator.nObjType == tile_ID ? sb->Position + 1 : sb->Position;
       // nTemp содержит число объектов
       while (nTemp)
       {
          nID ++;
          switch (Navigator.nObjType)
          {
             case tile_ID:
                // Для тайлов нужно отнимать 1, т.к. их отсчет начинается с 0
                if (pFrmSet->pFRM[Navigator.nObjType][nID - 1].bLocal)
                      nTemp --;
                 break;
             case inven_ID:
                if (pProSet->pPRO[item_ID][nID].bLocal)
                      nTemp --;
                break;
             default:
                 if (pProSet->pPRO[Navigator.nObjType][nID].bLocal)
                   nTemp --;
         }
       }
    }
    else  nID = sb->Position;

   memset(Navigator.nNavID, -1, 12 * 4);
   HDC dc;
   RECT rcs, rcd;
   dds2Nav->GetDC (&dc);
   PatBlt(dc, 0, 0, imgObj->Width,imgObj->Height, BLACKNESS);
   dds2Nav->ReleaseDC (dc);

   if (Navigator.nObjType == tile_ID && Navigator.nShowMode == SHOW_LOCAL) nID -= 1;
   while (nID <= Navigator.nMaxID && nPos < ObjsInPanel)
   {
      if (Navigator.nShowMode == SHOW_LOCAL)
      {
         if ((Navigator.nObjType == tile_ID &&
             !pFrmSet->pFRM[Navigator.nObjType][nID].bLocal) ||
             (Navigator.nObjType == inven_ID &&
             !pProSet->pPRO[item_ID][nID].bLocal) ||
             (Navigator.nObjType != tile_ID &&  Navigator.nObjType != inven_ID &&
             !pProSet->pPRO[Navigator.nObjType][nID].bLocal) )
         {
            nID++;
            continue;
         }
      }
      switch (Navigator.nObjType)
      {
         case tile_ID:
            nFrmID = nID;
            break;
         case inven_ID :
            pProSet->LoadPRO(item_ID, nID, false);
            nFrmID = pProSet->pPRO[item_ID][nID].GetInvFrmID();
            break;
         default :
            pProSet->LoadPRO(Navigator.nObjType, nID, false);
            nFrmID = pProSet->pPRO[Navigator.nObjType][nID].GetFrmID();
      }
      if (nFrmID != 0xFFFF)
      {
         pFrmSet->LoadFRM(Navigator.nObjType, nFrmID, false);
         frm = &pFrmSet->pFRM[Navigator.nObjType][nFrmID];
      }
      else
      {
         pFrmSet->LoadFRM(item_ID, 0, false);
         frm = &pFrmSet->pFRM[item_ID][0];
      }
      if (Navigator.nObjType == inven_ID)
      {
         pProSet->LoadPRO(item_ID, nID, false);
         nFrmID = pProSet->pPRO[item_ID][nID].GetFrmID();
         pFrmSet->LoadFRM(item_ID, nFrmID, false);
//         frm = &pFrmSet->pFRM[item_ID][nFrmID];
      }
      newWidth = frm->GetWi(0, 0);
      newWidth = newWidth > NAV_SIZE_X - 4 ? NAV_SIZE_X - 4 : newWidth;
      newHeight = frm->GetHe(0, 0);
      newHeight = newHeight > NAV_SIZE_Y ? NAV_SIZE_Y : newHeight;

      double ar; // aspect ratio of the frame
      if (newWidth && newHeight)
      {
         double ax = (double)frm->GetWi(0,0) / newWidth,
                ay = (double)frm->GetHe(0,0) / newHeight;
         ar = max (ax, ay);
         if (ar > .001)
         {
            newWidth = (int)frm->GetWi (0,0) / ar;
            newHeight = (int)frm->GetHe (0,0) / ar;
         }
      }

      int x = navX - (newWidth >> 1);
      int y = 35 - (newHeight >> 1);
      rcs = Rect (0, 0, frm->GetWi(0, 0), frm->GetHe(0, 0));
      rcd = Bounds (x, y, newWidth, newHeight);
      dds2Nav->Blt(&rcd, frm->GetBMP(), &rcs, DDBLT_WAIT | DDBLT_KEYSRC, NULL);

      Navigator.nNavID[nPos] = nID;
      if (Navigator.nSelID == nID)
         TransBltMask (frm, imgObj, 0, 0, x, y, dds2Nav, newWidth, newHeight);

      String id_str = "ID: " + String(nID);
      dds2Nav->GetDC(&dc);
      SelectObject (dc, imgObj->Font->Handle);
      SelectObject (dc, Brush->Handle);
      SetBkMode (dc, TRANSPARENT);
      SetTextColor (dc,
                     ColorToRGB( (Navigator.nSelID==nID) ? clYellow : clWhite));
      SetBkColor (dc, ColorToRGB(clBlack));
      TextOutA (dc, navX - 40, 66, id_str.c_str(), id_str.Length());
      dds2Nav->ReleaseDC(dc);

      nID++;
      navX += NAV_SIZE_X;
      nPos++;
   }
   imgObj->Repaint();
}
//---------------------------------------------------------------------------
void TfrmEnv::ResetInventoryInfo(void)
{
    Inventory.pObj = NULL;
    Inventory.pChildObj[0] = NULL;
    Inventory.pChildObj[1] = NULL;
    Inventory.pChildObj[2] = NULL;
    Inventory.pChildObj[3] = NULL;
    Inventory.nItemNum = -1;
    Inventory.nInvStartItem = -1;
//   frmInv1->sbINV->Position = 0;
    frmInv1->sbINV->Enabled = true;
    frmInv1->sbINV->Visible = false;
    frmInv1->btnChange->Enabled = false;
//   btnAdd->Enabled = false;
    frmInv1->btnRemove->Enabled = false;
}
//---------------------------------------------------------------------------
void TfrmEnv::RedrawInventory(void)
{
    if (!frmInv1->Visible) return;
    CFrame *frm;
    HDC dc;
    dds2Inv->GetDC (&dc);
    PatBlt(dc, 0, 0, frmInv1->imgInv->Width, frmInv1->imgInv->Height, BLACKNESS); //BLACKNESS
    dds2Inv->ReleaseDC (dc);
    frmInv1->imgInv->Repaint();

    if (pOS->GetSelectCount() != 1) return;
    // Получаем указатель на Selected object
    OBJECT * pContainer = pOS->GetSelectedObject(0);
    WORD hexX = pContainer->GetHexX();
    WORD hexY = pContainer->GetHexY();
    Inventory.pObj = pContainer;

    RECT rcs, rcd;

    OBJECT * pObj = NULL;
    int nChildCount = 0;
    WORD nFrameIndex, nProtoIndex;
    DWORD newWidth, newHeight;
    int nPos = 0;

     for (DWORD i = 0; i < pContainer->GetChildCount(); i++)
    {
        pObj = pContainer->GetChild(i);
        nChildCount++;
        // Находится.
        if (nChildCount - 1 < Inventory.nInvStartItem || nPos == NAV_MAX_COUNT) continue;

        if (pObj->GetFrameIndex() != 0xFFFF)
        {
            nFrameIndex = pProSet->pPRO[item_ID][pObj->GetProtoIndex()].GetInvFrmID();
            pFrmSet->LoadFRM(inven_ID, nFrameIndex, true);
            frm = &pFrmSet->pFRM[inven_ID][nFrameIndex];
        }
        else
        {
            pFrmSet->LoadFRM(item_ID, 0, true);
            frm = &pFrmSet->pFRM[item_ID][0];
        }

        newWidth = frm->GetWi(0, 0);
        newWidth = newWidth > NAV_SIZE_X ? NAV_SIZE_X : newWidth;
        newHeight = frm->GetHe(0, 0);
        newHeight = newHeight > NAV_SIZE_Y ? NAV_SIZE_Y : newHeight;

        double ar; // aspect ratio of the frame
        if (newWidth && newHeight)
        {
            double ax = (double)frm->GetWi(0,0) / newWidth,
                   ay = (double)frm->GetHe(0,0) / newHeight;
            ar = max (ax, ay);
            if (ar > .001)
            {
                newWidth = (int)frm->GetWi (0,0) / ar;
                newHeight = (int)frm->GetHe (0,0) / ar;
            }
        }

        int x,y;
        int CenterX, CenterY;
        CenterX = NAV_SIZE_X / 2 + 3;
        CenterY = NAV_SIZE_Y / 2;
        switch (nPos)
        {
            case 1: CenterX += NAV_SIZE_X; break;
            case 2: CenterY += NAV_SIZE_Y; break;
            case 3: CenterX += NAV_SIZE_X;
                    CenterY += NAV_SIZE_Y;
            break;
        }

        x = CenterX - (newWidth >> 1);
        y = CenterY - (newHeight >> 1);

        rcs = Rect (0,0, frm->GetWi(0,0), frm->GetHe(0,0));
        rcd = Bounds (x, y, newWidth, newHeight);
        dds2Inv->Blt(&rcd, frm->GetBMP(), &rcs, DDBLT_WAIT | DDBLT_KEYSRC, NULL);
        if (Inventory.nItemNum == nPos)
        {
            dds2Inv->GetDC(&dc);
            Canvas->Brush->Color = clLime; //
            Canvas->Brush->Style = bsSolid; //
            RECT rc = Rect(CenterX - NAV_SIZE_X / 2, CenterY - NAV_SIZE_Y / 2,
                           CenterX + NAV_SIZE_X / 2, CenterY + NAV_SIZE_Y / 2);
            //Rect(0, navY, frmInv1->imgInv->Width, navY + NAV_SIZE_Y);
            FrameRect (dc, &rc, Canvas->Brush->Handle);
            dds2Inv->ReleaseDC (dc);
        }
        // Сохраняем указатель на одного из видимых объектов
        Inventory.pChildObj[nPos] = pObj;

        String count_str = "x" + String(pObj->GetCount());
        dds2Inv->GetDC(&dc);
        SelectObject (dc, frmInv1->imgInv->Font->Handle);
        SelectObject (dc, Brush->Handle);
        SetBkMode (dc, TRANSPARENT);
        SetTextColor (dc, ColorToRGB(clLime));
        SetBkColor (dc, ColorToRGB(clBlack));
        TextOutA (dc, CenterX - NAV_SIZE_X / 2 + 5, CenterY - NAV_SIZE_Y / 2 + 5, count_str.c_str(), count_str.Length());
        dds2Inv->ReleaseDC(dc);
        nPos ++;
    }

    int max = 0;
    frmInv1->sbINV->Visible = true;
    max = nChildCount > 4 ? (nChildCount - 2) >> 1: 0;
    if (!(nChildCount & 1) && (nChildCount > 4)) max--;
    frmInv1->sbINV->Max = max;
    frmInv1->sbINV->Min = 0;
    frmInv1->sbINV->Visible = (frmInv1->sbINV->Max > 0);

    frmInv1->imgInv->Repaint();
}
//---------------------------------------------------------------------------
void TfrmEnv::TransBltFrm(CFrame* frm, TControl* win, short nDir, short nFrame,
                                        int x, int y, LPDIRECTDRAWSURFACE7 dds2, float aspect)
{
   RECT rcs, rcd;
   rcs = Bounds(frm->GetSprX(nDir, nFrame), frm->GetSprY(nDir, nFrame),
                            frm->GetWi(nDir, nFrame), frm->GetHe(nDir, nFrame));
   rcd = Bounds(x, y, frm->GetWi(nDir, nFrame), frm->GetHe(nDir, nFrame));
   rcd.left   /= aspect;
   rcd.right  /= aspect;
   rcd.top    /= aspect;
   rcd.bottom /= aspect;
   HRESULT res = dds2->Blt(&rcd, frm->GetBMP(), &rcs,
                                               DDBLT_WAIT | DDBLT_KEYSRC, NULL);
}
//---------------------------------------------------------------------------
void TfrmEnv::TransBltMask(CFrame* frm, TControl* win, short nDir, short nFrame,
                 int x, int y, LPDIRECTDRAWSURFACE7 dds2, int width, int height)
{
   RECT rcs, rcd;
   rcs = Bounds(0, 0, width, height);
   rcd = Bounds(x, y, width, height);
   frm->InitBorder(nDir, nFrame, width, height);
   HRESULT res = dds2->Blt(&rcd, frm->GetBorder(), &rcs,
                                               DDBLT_WAIT | DDBLT_KEYSRC, NULL);
}
//---------------------------------------------------------------------------
void TfrmEnv::TransBltTileRegion(LPDIRECTDRAWSURFACE7 dds2,
                                 int TileX1, int TileY1,
                                 int TileX2, int TileY2, TColor color, float aspect)
{
      int v1x, v1y, v2x, v2y, v3x, v3y, v4x, v4y;

/*      TileX1 = TileX1 % 100;

      TileY1 = TileY1 % 100;

      TileX2 = TileX2 % 100;

      TileY2 = TileY2 % 100; */
      pUtil->GetTileWorldCoord(TileX1 , TileY1, &v1x, &v1y);
      v1x += TILE_UP_CORNER_X - WorldX;
      v1y += TILE_UP_CORNER_Y - WorldY;
      pUtil->GetTileWorldCoord(TileX1, TileY2, &v2x, &v2y);
      v2x += TILE_RIGHT_CORNER_X - WorldX;
      v2y += TILE_RIGHT_CORNER_Y - WorldY;
      pUtil->GetTileWorldCoord(TileX2, TileY2, &v3x, &v3y);
      v3x += TILE_DOWN_CORNER_X - WorldX;
      v3y += TILE_DOWN_CORNER_Y - WorldY;
      pUtil->GetTileWorldCoord(TileX2, TileY1, &v4x, &v4y);
      v4x += TILE_LEFT_CORNER_X - WorldX;
      v4y += TILE_LEFT_CORNER_Y - WorldY;

	  TPoint points[4];
      points[0] = Point(v1x / aspect, v1y / aspect);
      points[1] = Point(v2x / aspect, v2y / aspect);
      points[2] = Point(v3x / aspect, v3y / aspect);
      points[3] = Point(v4x / aspect, v4y / aspect);

      Canvas->Brush->Style = bsClear; //
      Canvas->Pen->Color = color; //
      HDC dc;
      dds2->GetDC(&dc);
      HPEN pen = Canvas->Pen->Handle;
      HPEN oldpen = SelectObject(dc, pen);
      HBRUSH brush = Canvas->Brush->Handle;
      HBRUSH oldbrush = SelectObject(dc, brush);
      Polygon(dc, points, 4);
      SelectObject(dc, oldpen);
      SelectObject(dc, oldbrush);
      dds2->ReleaseDC (dc);
}
//---------------------------------------------------------------------------
void TfrmEnv::AttachDDraw(TControl *win, LPDIRECTDRAWSURFACE7 *dds, int primary)
{
   HRESULT res;
   DDSURFACEDESC2 ddSurfaceDesc;
   if (primary)
   {
      ZeroMemory(&ddSurfaceDesc, sizeof(ddSurfaceDesc));
      ddSurfaceDesc.dwSize = sizeof(ddSurfaceDesc);
      ddSurfaceDesc.dwFlags = DDSD_CAPS;
      ddSurfaceDesc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
      res = frmMDI->pDD->CreateSurface (&ddSurfaceDesc, dds, NULL);
      res = frmMDI->pDD->CreateClipper (0, &ddcMap, NULL);
      res = ddcMap->SetHWnd (0, imgMap->Handle);
      res = frmMDI->pDD->CreateClipper (0, &ddcNav, NULL);
      res = ddcNav->SetHWnd (0, imgObj->Handle);
      res = frmMDI->pDD->CreateClipper (0, &ddcInv, NULL);
      res = ddcInv->SetHWnd (0, frmInv1->imgInv->Handle);
      res = frmMDI->pDD->CreateClipper (0, &ddcObj, NULL);
      res = ddcObj->SetHWnd (0, frmObj1->imgObj->Handle);
   }
   else
   {
      ZeroMemory(&ddSurfaceDesc, sizeof(ddSurfaceDesc));
      ddSurfaceDesc.dwSize = sizeof(ddSurfaceDesc);
      ddSurfaceDesc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
      ddSurfaceDesc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
      ddSurfaceDesc.dwWidth = win->Width;
      ddSurfaceDesc.dwHeight = win->Height;
      res = frmMDI->pDD->CreateSurface(&ddSurfaceDesc, dds, NULL);

      struct
      {
	 RGNDATAHEADER rdh;
	 RECT rect;
      } clip_rgn;
      memset(&clip_rgn, 0, sizeof(clip_rgn));
      clip_rgn.rect = win->ClientRect;
      clip_rgn.rdh.dwSize = sizeof(RGNDATAHEADER);
      clip_rgn.rdh.iType = RDH_RECTANGLES;
      clip_rgn.rdh.nCount = 1;
      clip_rgn.rdh.nRgnSize = sizeof(RECT);
      clip_rgn.rdh.rcBound = clip_rgn.rect;

      res = frmMDI->pDD->CreateClipper (0, &ddc, NULL);
      res = ddc->SetClipList ((LPRGNDATA)&clip_rgn, 0);
      res = (*dds)->SetClipper (ddc); 

      DDCOLORKEY ck;
      ck.dwColorSpaceLowValue = 0;
      ck.dwColorSpaceHighValue = 0;
      res = (*dds)->SetColorKey(DDCKEY_SRCBLT, &ck);
   }
}
//---------------------------------------------------------------------------
void TfrmEnv::RedrawMap(bool StaticRedraw)
{
    //  Положение не изменилось
    if ((OldWorldX == WorldX) && (OldWorldY == WorldY) && !StaticRedraw) return;
    int w1 = pUtil->GetScreenWidth();
    int h1 = pUtil->GetScreenHeight();
    HDC dc;
    dds2Map->GetDC (&dc);
    PatBlt(dc, 0, 0, w1, h1, BLACKNESS);
    dds2Map->ReleaseDC (dc);
    if (bShowObj[floor_ID]) RedrawFloor(fAspect);
    RedrawHex(fAspect);
    RedrawObjects(pOS, fAspect);
    if (SelectMode == PASTE_OBJ) RedrawObjects(pBufObjSet, fAspect);
    if (bShowObj[roof_ID]) RedrawRoof(fAspect);

	if (bShowObj[TG_blockID] || bShowObj[EG_blockID] || bShowObj[SAI_blockID] ||
        bShowObj[wall_blockID] || bShowObj[obj_blockID] ||
        bShowObj[light_blockID] || bShowObj[scroll_blockID] ||
        bShowObj[obj_self_blockID])
        RedrawBlockers(fAspect);

   if (have_sel )
   {
      dds2Map->GetDC(&dc);
      Canvas->Brush->Color = clAqua; //
      Canvas->Brush->Style = bsSolid; //
      RECT rc = Rect(min(downX, cursorX), min(downY, cursorY),
                     max(downX, cursorX), max(downY, cursorY));
      FrameRect (dc, &rc, Canvas->Brush->Handle);
      dds2Map->ReleaseDC (dc);
   }
   DrawMiniMap();
   imgMap->Repaint();
   RedrawLocator();
   UpdateObjPanel();
   OldWorldX = WorldX;
   OldWorldY = WorldY;
}
//---------------------------------------------------------------------------
void TfrmEnv::RedrawFloor(float aspect)
{
    int x, y, prev_xx, prev_yy;
    WORD TileId;
    int w1 = pUtil->GetScreenWidth();
    int h1 = pUtil->GetScreenHeight();

    HDC dc;

    String str;
    int xx = (pMap->GetWidth()-1) * 48; //
    int yy = 0; //
    for (y = 0; y < pMap->GetHeight(); y++)
    {
        prev_xx = xx;
        prev_yy = yy;
        for (x = 0; x < pMap->GetWidth(); x++)
        {
            TileId = pTileSet->GetFloorID(x, y);
            if (pBufFloor)
            {
                if (SelectMode == PASTE_FLOOR && x >= TileX && x < (TileX + pBufFloor->GetWidth())
                    && y >= TileY && y < (pBufFloor->GetHeight() + TileY))
                {
                    TileId = pBufFloor->GetFloorID(x - TileX, y - TileY);
                }
            }
            TileId &= 0x0FFF;
            if (TileId != 1) //несуществующие тайлы пола
            {
                if (
                      (xx + pFrmSet->pFRM[tile_ID][TileId].GetWi(0, 0))/aspect > WorldX/aspect &&
                       xx/aspect < (WorldX)/aspect + w1 &&
                      (yy + pFrmSet->pFRM[tile_ID][TileId].GetHe(0, 0))/aspect > WorldY/aspect &&
                       yy/aspect < (WorldY)/aspect + h1
                    )
                {
                    TransBltFrm(&pFrmSet->pFRM[tile_ID][TileId], imgMap, 0, 0,
                                                 (xx - WorldX), (yy - WorldY), dds2Map, aspect);

                }
            }

            xx -= 48;
            yy += 12;
        }
        xx = prev_xx + 32;
        yy = prev_yy + 24;
    }

    if (pTileSet->dwSelection == floor_ID)
    {
          TransBltTileRegion(dds2Map,
                             pTileSet->SelectedFloorX1,
                             pTileSet->SelectedFloorY1,
                             pTileSet->SelectedFloorX2,
                             pTileSet->SelectedFloorY2, clLime, aspect);
    }

    if (SelectMode == PASTE_FLOOR)
    {
        
           // Обводим всю карту контуром
        TransBltTileRegion(dds2Map, TileX, TileY,
                                    pBufFloor->GetWidth()-1 + TileX,
                                    pBufFloor->GetHeight()-1 + TileY ,clBlue, aspect);

    }

    // Обводим всю карту контуром
    TransBltTileRegion(dds2Map, 0, 0,
                              pMap->GetWidth() - 1,
                              pMap->GetHeight() -1 ,clWhite, aspect);
}
//---------------------------------------------------------------------------
void TfrmEnv::RedrawObjects(CObjSet * pObjSet, float aspect)
{
    int w1 = pUtil->GetScreenWidth();
    int h1 = pUtil->GetScreenHeight();

    int X, Y;
    WORD nX, nY, nXX, nYY;
    int nDir;
    int nMapX, nMapY;
    short xx, yy;

    DWORD nProtoIndex;
    WORD  nFrameIndex;

    WORD FID;
    OBJECT * pObj;

    BYTE nType;

    DWORD cnt = pObjSet->GetObjCount(false);

    frmHdr1->lbTotalCount->Caption = String(pObjSet->GetObjCount(true));

    for (DWORD j = 0; j < cnt; j++)
    {
        pObj = pObjSet->GetObject(j);
        if (pObj->IsChild()) continue;
        nType = pObj->GetType();
        nDir = pObj->GetDirection();
  //    if (nType != critter_ID)
  //        continue;
        // Полуtчаем индекс объекта в FrameList
        nFrameIndex  = pObj->GetFrameIndex();
        // Для криттера индекс определяется особенным образом.
        if (nType == critter_ID)
        {
            String filename = pUtil->GetFRMFileName(nType,
                              pLstFiles->pFRMlst[nType]->Strings[nFrameIndex]);
            pFrmSet->GetCritterFName(&filename, pObj->GetFrameID(), &nFrameIndex);
        }
        nProtoIndex = pObj->GetProtoIndex();
        if (bShowObj[nType] && !pUtil->GetBlockType(nType, nProtoIndex))
        {
            nX = pObj->GetHexX();
            nY = pObj->GetHexY();
            nDir = pObj->GetDirection();
            pUtil->GetHexWorldCoord(nX, nY, &nMapX, &nMapY);

            X = nMapX - WorldX -
                (pFrmSet->pFRM[nType][nFrameIndex].GetWi(nDir, 0) >> 1) +
                 pFrmSet->pFRM[nType][nFrameIndex].GetDoffX(nDir) + pObj->GetOffsetX();

            Y = nMapY - WorldY -
                 pFrmSet->pFRM[nType][nFrameIndex].GetHe(nDir, 0) +
                 pFrmSet->pFRM[nType][nFrameIndex].GetDoffY(nDir) + pObj->GetOffsetY();

            if ((X + pFrmSet->pFRM[nType][nFrameIndex].GetWi(nDir, 0))/aspect > 0 &&
                 X/aspect < w1 &&
                (Y + pFrmSet->pFRM[nType][nFrameIndex].GetHe(nDir, 0))/aspect > 0 &&
                 Y/aspect < h1)
            {
	            TransBltFrm(&(pFrmSet->pFRM[nType][nFrameIndex]), imgMap, nDir, 0, X, Y, dds2Map, aspect);
                if (pObjSet->ObjIsSelected(pObj))
                {
	                TransBltMask(&pFrmSet->pFRM[nType][nFrameIndex], imgMap, nDir, 0,
                                    X, Y, dds2Map, pFrmSet->pFRM[nType][nFrameIndex].GetWi(nDir, 0),
                                 pFrmSet->pFRM[nType][nFrameIndex].GetHe(nDir, 0));
                }
            }
        }
    }
}

//---------------------------------------------------------------------------
void TfrmEnv::RedrawRoof(float aspect)
{
   int x, y, prev_xx, prev_yy;
   WORD TileId;
   int xx = (pMap->GetWidth() - 1) * 48;
   int yy = -96; //0                    TileY = 0
   int w1 = pUtil->GetScreenWidth();
   int h1 = pUtil->GetScreenHeight();
   for (y = 0; y < pMap->GetHeight(); y++)
   {
      prev_xx = xx;
      prev_yy = yy;
      for (x = 0; x < pMap->GetWidth(); x++)
      {
            TileId = pTileSet->GetRoofID(x, y);
            if (pBufRoof)
            {
                 if (SelectMode == PASTE_ROOF && x >= TileX + 2 && x < (TileX + 2 + pBufRoof->GetWidth())
                    && y >= TileY + 3 && y < (pBufRoof->GetHeight() + TileY + 3))
                {
                        TileId = pBufRoof->GetRoofID(x - (TileX + 2), y - (TileY + 3));
                }
             }

         TileId &= 0x0FFF;
         if (TileId != 1) {//несуществующие тайлы потолка
            if (
                 (xx + pFrmSet->pFRM[tile_ID][TileId].GetWi(0, 0))/aspect > WorldX/aspect &&
                  xx/aspect < WorldX/aspect + w1 &&
                 (yy + pFrmSet->pFRM[tile_ID][TileId].GetHe(0, 0))/aspect > WorldY/aspect &&
                  yy/aspect < WorldY/aspect + h1
               )
            {
               TransBltFrm(&pFrmSet->pFRM[tile_ID][TileId], imgMap, 0, 0,
                                             xx - WorldX, yy - WorldY, dds2Map, aspect);
            }
         }
         xx -= 48;
         yy += 12;
      }
      xx = prev_xx + 32;
      yy = prev_yy + 24;
   }
   if (pTileSet->dwSelection == roof_ID)
   {
      TransBltTileRegion(dds2Map,
                         pTileSet->SelectedRoofX1 - 2,
                         pTileSet->SelectedRoofY1 - 3,
                         pTileSet->SelectedRoofX2 - 2,
                         pTileSet->SelectedRoofY2 - 3, clLime, aspect);
   }

    if (SelectMode == PASTE_ROOF)
    {

           // Обводим всю карту контуром
        TransBltTileRegion(dds2Map, TileX, TileY,
                                    pBufRoof->GetWidth()-1 + TileX,
                                    pBufRoof->GetHeight()-1 + TileY ,clBlue, aspect);

    }
}
//---------------------------------------------------------------------------
void TfrmEnv::RedrawBlockers(float aspect)
{
    int w1 = pUtil->GetScreenWidth();
    int h1 = pUtil->GetScreenHeight();

    int X, Y;
    BYTE nType;
    WORD nX, nY, nXX, nYY, nFrmID, nProID;
    int nMapX, nMapY;
    short xx, yy;

    OBJECT * pObj = NULL;

    for (DWORD j = 0; j < pOS->GetObjCount(false); j++)
    {
        pObj = pOS->GetObject(j);
        nType = pObj->GetType();
        nFrmID = pObj->GetFrameID();
        nProID = pObj->GetProtoID();
        int nBlockType = pUtil->GetBlockType(nType, nProID);

        if (bShowObj[obj_self_blockID] && !(pObj->GetFlags() & 0x00000010))
            nBlockType = obj_self_blockID;
        if (nBlockType && bShowObj[nBlockType])
        {
            pUtil->GetBlockFrm(nBlockType, &nType, &nFrmID);
            nX = pObj->GetHexX();
            nY = pObj->GetHexY();
            pUtil->GetHexWorldCoord(nX, nY, &nMapX, &nMapY);
            X = nMapX - WorldX -
                (pFrmSet->pFRM[nType][nFrmID].GetWi(0, 0) >> 1) +
                 pFrmSet->pFRM[nType][nFrmID].GetDoffX(0) +
                 pObj->GetOffsetX();
            Y = nMapY - WorldY -
                 pFrmSet->pFRM[nType][nFrmID].GetHe(0, 0) +
                 pFrmSet->pFRM[nType][nFrmID].GetDoffY(0) +
                 pObj->GetOffsetY();
            if (
                (X + pFrmSet->pFRM[nType][nFrmID].GetWi(0, 0))/aspect > 0 &&
                 X/aspect < w1 &&
                (Y + pFrmSet->pFRM[nType][nFrmID].GetHe(0, 0))/aspect > 0 &&
                 Y/aspect < h1)
            {
                TransBltFrm(&pFrmSet->pFRM[nType][nFrmID], imgMap, 0, 0, X, Y, dds2Map, aspect);
                if (pOS->ObjIsSelected(pObj))
                {
                    TransBltMask(&pFrmSet->pFRM[nType][nFrmID], imgMap, 0, 0,
                                   X, Y, dds2Map,
                                   pFrmSet->pFRM[nType][nFrmID].GetWi(0, 0),
                                   pFrmSet->pFRM[nType][nFrmID].GetHe(0, 0));
                }
            }
        }
    }
}
//---------------------------------------------------------------------------
void TfrmEnv::RedrawHex(float aspect)
{
    if (!frmMDI->sbHex->Down) return;
    int w1 = pUtil->GetScreenWidth();
    int h1 = pUtil->GetScreenHeight();

    int nMapX, nMapY;
    int x, y;
    HDC dc;
    dds2Map->GetDC(&dc);
    for (y = 0; y < pMap->GetHeight() * 2; y++)
    {
        for (x = 0; x < pMap->GetWidth() * 2; x++)
        {
            pUtil->GetHexWorldCoord(x, y, &nMapX, &nMapY);
            if (((nMapX + 16)/aspect > WorldX/aspect &&
                  nMapX/aspect < (WorldX)/aspect  + w1) &&
                 ((nMapY + 8)/aspect > WorldY/aspect &&
                   nMapY/aspect < (WorldY)/aspect  + h1))
            {
//                StretchBlt(dc, (nMapX - WorldX - 16)/aspect, (nMapY - WorldY - 8)/aspect,
//                                32/aspect , 16/aspect, pBMPHex->Canvas->Handle, 0,0, 32, 16, SRCPAINT);


                 BitBlt(dc, (nMapX - WorldX - 16)/aspect, (nMapY - WorldY - 8)/aspect,
                             32/aspect , 16/aspect, pBMPHex->Canvas->Handle, 0, 0, SRCPAINT);

            }
        }
    }
   dds2Map->ReleaseDC(dc);
}
//---------------------------------------------------------------------------
void TfrmEnv::RedrawLocator(void)
{
    int x, y;
    pUtil->GetCursorTile(0, 0, WorldX, WorldY, &x, &y);
    if (x == OldX && y == OldY) return;
    x = 189 - x;
    BitBlt(frmMMap1->imgMiniMap->Canvas->Handle, x, y, 21, 21,
                                  pBMPlocator->Canvas->Handle, 0, 0, SRCPAINT);
    OldX = x;
    OldY = y;
    frmMMap1->imgMiniMap->Repaint();
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::FormClose(TObject *Sender, TCloseAction &Action)
{
    Action = caNone;
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::imgMapMouseDown(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
    if (Button == mbLeft)
    {
        pUtil->GetCursorHex(X, Y, WorldX, WorldY, &downHexX, &downHexY);
        pUtil->GetCursorTile(X, Y, WorldX, WorldY, &downTileX, &downTileY);

        if (SelectMode == SELECT_FLOOR || SelectMode == SELECT_OBJ)
            if (downTileX < 0 || downTileY < 0 ||
                downTileX >= pMap->GetWidth() || downTileY >=  pMap->GetHeight())
                    return;
        if (SelectMode == SELECT_ROOF)
            if (downTileX < -2 || downTileY < -3 ||
                downTileX >= (int)pMap->GetWidth() - 2 || downTileY >=  (int)pMap->GetHeight() - 3)
                    return;

        downX = X;
        downY = Y;
        switch (SelectMode)
        {
            case SELECT_NONE:
               Screen->Cursor = (TCursor)crHandTakeCursor;
               break;
            case SELECT_OBJ:
               ClearObjSelection(true);
               break;
            case SELECT_FLOOR:
            case SELECT_ROOF:
                tile_sel = 0;
                break;
         }
         mouseBLeft = true;
    } else if (Button == mbRight)
    {
         mouseBRight = true;
         if (SelectMode == DRAW_ROOF)
         {
             frmMDI->btnselect1->Click();
             frmMDI->btnselect1->Down = true;
         }
         if (SelectMode == DRAW_OBJ)
         {
             frmMDI->btnselect2->Click();
             frmMDI->btnselect2->Down = true;
         }
         if (SelectMode == DRAW_FLOOR)
         {
             frmMDI->btnselect3->Click();
             frmMDI->btnselect3->Down = true;
         }
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::imgMapClick(TObject *Sender)
{
    frmEnv->SetFocus();
    Edit1->SetFocus();
//   int TileX, TileY;
//  pUtil->GetCursorTile(downX, downY, WorldX, WorldY, &TileX, &TileY);

   int HexX, HexY;
   switch (SelectMode)
   {
      case SELECT_OBJ:
         if (upX != downX || upY != downY) return;
         SelectObjXY(downX, downY);
         RedrawMap(true);
         break;
      case DRAW_FLOOR:
         pMap->SaveChange(LM_FLOOR);   
         if (Navigator.nObjType != tile_ID || Navigator.nSelID == NONE_SELECTED)
            return;
         pTileSet->SetFloor(TileX, TileY, Navigator.nSelID);
         RedrawMap(true);
         SetButtonSave(true);
         RedrawMap(true);
         break;
      case DRAW_ROOF:
         pMap->SaveChange(LM_ROOF);
         if (Navigator.nObjType != tile_ID || Navigator.nSelID == NONE_SELECTED)
            return;
         pTileSet->SetRoof(TileX + 2, TileY + 3, Navigator.nSelID);
         RedrawMap(true);
         SetButtonSave(true);
         RedrawMap(true);
         break;
      case DRAW_OBJ:
         pMap->SaveChange(LM_OBJ);
         if (Navigator.nObjType == tile_ID || Navigator.nSelID == NONE_SELECTED)
            return;
         pUtil->GetCursorHex(downX, downY, WorldX, WorldY, &HexX, &HexY);
         BYTE ObjType = Navigator.nObjType == inven_ID ? item_ID : Navigator.nObjType;
         // Добавляем новый объект
         pOS->AddObject(HexX, HexY, ObjType, Navigator.nSelID);
         SetButtonSave(true);
         RedrawMap(true);
         break;
   }
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::imgMapMouseUp(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
   upX = X;
   upY = Y;
   int w,h;

   switch (Button)
   {
      case mbLeft:
         mouseBLeft = false;
         break;
      case mbRight:
         mouseBRight = false;
         return;
   }
   switch (SelectMode)
   {
      case PASTE_FLOOR:
            w = pBufFloorHeader->GetWidth();
            h = pBufFloorHeader->GetHeight();
            pTileSet->SelectTiles(SELECT_FLOOR, TileX, TileY, TileX + w - 1, TileY + h - 1);
            pMap->SaveChange(LM_FLOOR);
            // Сохраняем изменения.
            pBufFloor->CopyTo(pTileSet, TileX, TileY, COPY_FLOOR);
            ChangeMode(SELECT_FLOOR);
            pTileSet->SelectTiles(SELECT_FLOOR, TileX, TileY, TileX + w - 1, TileY + h - 1);
            SetButtonSave(true);
            RedrawMap(true);
        break;
      case PASTE_ROOF:
            w = pBufRoofHeader->GetWidth();
            h = pBufRoofHeader->GetHeight();
            pTileSet->SelectTiles(SELECT_ROOF, TileX, TileY, TileX + w - 1, TileY + h - 1);
            pMap->SaveChange(LM_ROOF);
            // Сохраняем изменения.
            pBufRoof->CopyTo(pTileSet, TileX+2, TileY+3, COPY_ROOF);
            ChangeMode(SELECT_ROOF);
            pTileSet->SelectTiles(SELECT_ROOF, TileX, TileY, TileX + w - 1, TileY + h - 1);
            SetButtonSave(true);
            RedrawMap(true);
        break;
      case PASTE_OBJ:
            pMap->SaveChange(LM_OBJ);
            pBufObjSet->CopySelectedTo(pOS, true);
            ChangeMode(SELECT_OBJ);
            SetButtonSave(true);
            RedrawMap(true);
        break;
      case SELECT_NONE:
         Screen->Cursor = crDefault;
         break;
      case SELECT_OBJ:
         if (upX - downX && upY - downY)
         {
            have_sel = 0;
            RedrawMap(true);
         }
         else
            imgMapClick(Sender);
         break;
      case SELECT_FLOOR:
      case SELECT_ROOF:
         if (upX - downX && upY - downY)
         {
            tile_sel = 1;
            RedrawMap(true);
         }
         else
         {
            // Выбран всего 1 тайл.
            tile_sel = 2;
            SelectTileRegion(SelectMode, downX, downY, downX, downY);
            RedrawMap(true);
         }
   }
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::imgMapMouseMove(TObject *Sender,
      TShiftState Shift, int X, int Y)
{
   pUtil->GetCursorTile(X, Y, WorldX, WorldY, &TileX, &TileY);
   panelTILE->Text = "TILE: (" + (String)TileX + ", " + TileY + ")";
   pUtil->GetCursorHex(X, Y, WorldX, WorldY, &HexX, &HexY);
   panelHEX->Text = "HEX: (" + (String)HexX + ", " + (String)HexY + ")";
   cursorX = X;
   cursorY = Y;
   switch (SelectMode)
   {
      case PASTE_FLOOR:
      case PASTE_ROOF:
            RedrawMap(true);
         break;
      case PASTE_OBJ:
            pBufObjSet->MoveSelectedTo(HexX, HexY);
            RedrawMap(true);
         break;
      case SELECT_NONE:
         if (mouseBLeft)
         {
            int changeX = X - downX;
            int changeY = Y - downY;
    //        if ((WorldX - changeX) < 0 || (WorldX - changeX) > pHeader->GetWidth() * 48) return;
     //       if ((WorldY - changeY) < 0 || (WorldY - changeY) > pMap->GetHeight() * 32) return;

            WorldX -= changeX;
            WorldY -= changeY;
            RedrawMap(false);
            downX = X;
            downY = Y;
         }
         break;
      case SELECT_FLOOR:
      case SELECT_ROOF:
         if ((X - downX) && (Y - downY) && (mouseBLeft))
         {
            SelectTileRegion(SelectMode, downX, downY, X, Y);
            RedrawMap(true);
/*            if (!tile_sel || prevX != X || prevY != Y)
            {
               tile_sel = 1;
               prevX = X;
               prevY = Y;
            } */
         }
         break;
      case SELECT_OBJ:
         if ((X - downX) && (Y - downY) &&
             (X - prevX) && (Y - prevY) && (mouseBLeft))
         {
            SelectObjRegion(min(downX, X) , min(downY, Y),
                            max(downX, X) , max(downY, Y));
            RedrawMap(true);

            if (!have_sel || prevX != X || prevY != Y)
            {
               have_sel = 1;
               prevX = X;
               prevY = Y;
            }
         }
         break;
      case DRAW_OBJ:
         if (Navigator.nSelID != -1 && Navigator.nObjType != tile_ID)
         {
            int HexX, HexY, objX, objY;
            BYTE ObjType;
            ObjType = Navigator.nObjType == inven_ID ? item_ID : Navigator.nObjType;
            WORD nFrmID = pProSet->pPRO[ObjType][Navigator.nSelID].GetFrmID();
            CFrame *frm = &pFrmSet->pFRM[ObjType][nFrmID];
            pUtil->GetCursorHex(X, Y, WorldX, WorldY, &HexX, &HexY);
            pUtil->GetHexWorldCoord(HexX, HexY, &objX, &objY);

            objX = objX - WorldX - (frm->GetWi(0, 0) >> 1) + frm->GetDoffX(0);
            objY = objY - WorldY - frm->GetHe(0, 0) + frm->GetDoffY(0);
            if (!(objX == OldShowObjX && objY == OldShowObjY))
            {
               RedrawMap(true);
               TransBltFrm(frm, imgMap, 0, 0, objX, objY, dds2Map, 1.0);
               imgMap->Repaint();
               OldShowObjX = objX;
               OldShowObjY = objY;
            }
         }
         break;
      case DRAW_FLOOR:
      case DRAW_ROOF:
/*         if (BufferMode == PASTE_FLOOR || BufferMode == PASTE_ROOF)
         {
            RedrawMap(true);
            break;
         }
*/
         if (Navigator.nSelID != -1 && Navigator.nObjType == tile_ID)
         {
            int TileX, TileY, objX, objY;
            CFrame *frm = &pFrmSet->pFRM[tile_ID][Navigator.nSelID];
            pUtil->GetCursorTile(X, Y, WorldX, WorldY, &TileX, &TileY);

            if (SelectMode == DRAW_FLOOR && (TileX < 0 || TileX >= FO_MAP_WIDTH ||
                                               TileY < 0 || TileY >= FO_MAP_HEIGHT)) break;
            if (SelectMode == DRAW_ROOF &&  (TileX < -2 || TileX >= FO_MAP_WIDTH-2 ||
                                               TileY < -3 || TileY >= FO_MAP_HEIGHT-3)) break;
            pUtil->GetTileWorldCoord(TileX, TileY, &objX, &objY);

            if (!(objX == OldShowObjX && objY == OldShowObjY))
            {
                pTileSet->ClearFloorSelection();
                pTileSet->ClearRoofSelection();
                RedrawMap(true);
                TransBltFrm(frm, imgMap, 0, 0, objX - WorldX, objY - WorldY, dds2Map, 1.0);
                if (SelectMode == DRAW_FLOOR)
                    pTileSet->SelectTiles(SELECT_FLOOR, TileX, TileY, TileX, TileY);
                else
                    pTileSet->SelectTiles(SELECT_ROOF, TileX, TileY, TileX, TileY);
                TransBltTileRegion(dds2Map, TileX, TileY, TileX, TileY, clLime, fAspect);
                imgMap->Repaint();
        //       RedrawMap(true);
                OldShowObjX = objX;
                OldShowObjY = objY;
            }
         }
         break;
      case MOVE_OBJ:
         int offsetHexX = HexX - downHexX;
         int offsetHexY = HexY - downHexY;
         if ((offsetHexX || offsetHexY) && mouseBLeft && pOS->GetSelectCount())
         {
            MoveSelectedObjects(offsetHexX, offsetHexY);
            RedrawMap(true);
            downHexX = HexX;
            downHexY = HexY;
         }
         break;
   }
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::imgMiniMapMouseDown(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
    if ( X >= pMap->GetWidth() || Y >= pMap->GetHeight()) return;
   pUtil->GetWorldCoord(X, Y, &WorldX, &WorldY);
   switch (Button)
   {
      case mbLeft:
         mouseBLeft2 = true;
         break;
      case mbRight:
         mouseBRight2 = true;
         break;
   }
   RedrawMap(false);
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::imgMiniMapMouseUp(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
   switch (Button)
   {
      case mbLeft:
         mouseBLeft2 = false;
         break;
      case mbRight:
         mouseBRight2 = false;
         break;
   }
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::imgMiniMapMouseMove(TObject *Sender,
      TShiftState Shift, int X, int Y)
{
   X = (X <= pMap->GetWidth() - 1 ? X : pMap->GetWidth() - 1) * (X >= 0);
   Y = (Y <= pMap->GetHeight() - 1 ? Y : pMap->GetWidth() - 1) * (Y >= 0);
   panelTILE->Text =
                  "TILE: (" + (String)(pMap->GetWidth() - 1 - X) + ", " + (String)(Y) + ")";
   if (mouseBLeft2)
      imgMiniMapMouseDown(Sender, mbRight, Shift, X, Y);
}
//---------------------------------------------------------------------------
void TfrmEnv::SelectObjXY(int X, int Y)
{
    WORD hexX, hexY;
    BYTE nType;
    int nMapX, nMapY, objX, objY;
    DWORD nDir;

    OBJECT * pObj = NULL;
    int nSelObj = -1;

    WORD nFrameIndex;
    WORD nProtoIndex;

    ClearObjSelection(false);
    DWORD i;

    for (i = 0; i < pOS->GetObjCount(false); i++)
    {
        // Получаем указатель на объект
        pObj        = pOS->GetObject(i);
        nType       = pObj->GetType();
        nFrameIndex = pObj->GetFrameIndex();
        nProtoIndex = pObj->GetProtoIndex();
        nDir        = pObj->GetDirection();

        if (pObj->GetFlag(FL_OBJ_IS_CHILD)) continue;

        // Для криттера индекс определяется особенным образом.
        if (nType == critter_ID)
        {
            String filename = pUtil->GetFRMFileName(nType,
                              pLstFiles->pFRMlst[nType]->Strings[nFrameIndex]);
            pFrmSet->GetCritterFName(&filename, pObj->GetFrameID(), &nFrameIndex);
        }

        int nBlockType = pUtil->GetBlockType(nType, nProtoIndex);
        if ((!nBlockType && bShowObj[nType]) ||
            (nBlockType && bShowObj[nBlockType]))
        {
            pUtil->GetBlockFrm(nBlockType, &nType, &nFrameIndex);
            hexX = pObj->GetHexX();
            hexY = pObj->GetHexY();
            pUtil->GetHexWorldCoord(hexX, hexY, &nMapX, &nMapY);
            // Получаем координаты объекта на экране
            objX = nMapX - WorldX -
                    (pFrmSet->pFRM[nType][nFrameIndex].GetWi(nDir, 0) >> 1) +
                     pFrmSet->pFRM[nType][nFrameIndex].GetDoffX(nDir) +
                     pObj->GetOffsetX();
            objY = nMapY - WorldY -
                     pFrmSet->pFRM[nType][nFrameIndex].GetHe(nDir, 0) +
                     pFrmSet->pFRM[nType][nFrameIndex].GetDoffY(nDir) +
                     pObj->GetOffsetY();
            // Проверяем - попадает ли указатель на объект.
            if (X >= objX &&
                X <= objX + pFrmSet->pFRM[nType][nFrameIndex].GetWi(nDir, 0) &&
                Y >= objY &&
                Y <= objY + pFrmSet->pFRM[nType][nFrameIndex].GetHe(nDir, 0))
            {
                // Провремяем - попали ли мы на объект.
                if (pFrmSet->pFRM[nType][nFrameIndex].GetPixel(X - objX, Y - objY, nDir, 0))
                {
                    nSelObj = i;
                }
            }
        }
    }
     if ( nSelObj == -1) return;       // Не нашли
     pOS->SelectObject(nSelObj, true);
/*
    OBJECT * pSelObj =  pOS->GetObject(nSelObj);
    for (i = 0; i < pOS->GetObjCount(); i++)
    {
        pObj = pOS->GetObject(i);
        if (pObj->IsChild() && pObj != pSelObj && pObj->GetHexX() == pSelObj->GetHexX()
                                               && pObj->GetHexY() == pSelObj->GetHexY())
            pOS->SelectObject(i, true);
   } */
   SelectionChanged();
}
//---------------------------------------------------------------------------
bool TfrmEnv::SelectObjRegion(int X, int Y, int X2, int Y2)
{
    WORD  hexX, hexY;
    BYTE  nType;
    WORD  nFrameIndex, nProtoIndex;
    int   nMapX, nMapY, objX, objY;
    DWORD nDir;

    OBJECT * pObj;

    // Очищаем все выделенные объекты.
    ClearObjSelection(false);

    for (DWORD j = 0; j < pOS->GetObjCount(false); j++ )
    {
        // Получаем указатель на объект
        pObj        = pOS->GetObject(j);
        if (pObj->IsChild()) continue;
        nType       = pObj->GetType();
        nFrameIndex = pObj->GetFrameIndex();
        nProtoIndex = pObj->GetProtoIndex();
        nDir        = pObj->GetDirection();

        // Для криттера индекс определяется особенным образом.
        if (nType == critter_ID)
        {
            String filename = pUtil->GetFRMFileName(nType,
                              pLstFiles->pFRMlst[nType]->Strings[nFrameIndex]);
            pFrmSet->GetCritterFName(&filename, pObj->GetFrameID(), &nFrameIndex);
        }

        int nBlockType = pUtil->GetBlockType(nType, nProtoIndex);
        if ((!nBlockType && bShowObj[nType]) ||
            (nBlockType && bShowObj[nBlockType]))
        {
            pUtil->GetBlockFrm(nBlockType, &nType, &nFrameIndex);
            hexX = pObj->GetHexX();
            hexY = pObj->GetHexY();
            pUtil->GetHexWorldCoord(hexX, hexY, &nMapX, &nMapY);
            // Получаем координаты объекта на экране
            objX = nMapX - WorldX -
                    (pFrmSet->pFRM[nType][nFrameIndex].GetWi(nDir, 0) >> 1) +
                     pFrmSet->pFRM[nType][nFrameIndex].GetDoffX(nDir) +
                     pObj->GetOffsetX();
            objY = nMapY - WorldY -
                     pFrmSet->pFRM[nType][nFrameIndex].GetHe(nDir, 0) +
                     pFrmSet->pFRM[nType][nFrameIndex].GetDoffY(nDir) +
                     pObj->GetOffsetY();

            if ((objX + pFrmSet->pFRM[nType][nFrameIndex].GetWi(nDir, 0) > X &&
               objX < X2) &&
                (objY + pFrmSet->pFRM[nType][nFrameIndex].GetHe(nDir, 0) > Y &&
               objY < Y2))
            {
                pOS->SelectObject(j, true);
            }
        }
   }
   SelectionChanged();
   return true;
}
//---------------------------------------------------------------------------
void TfrmEnv::SelectionChanged(void)
{
   ResetInventoryInfo();
   RedrawInventory();
   UpdateObjPanel();
//   panelObjSelected->Text = "Selected: " + (String)pOS->Get;
   frmInv1->btnAdd->Enabled =
        ((Navigator.nObjType == inven_ID || Navigator.nObjType == item_ID) &&
         pOS->GetSelectCount() == 1 && Navigator.nSelID != 0xFFFFFFFF);
}
//---------------------------------------------------------------------------
bool TfrmEnv::SelectTileRegion(int TileMode, int X, int Y, int X2, int Y2)
{
   int TileX1, TileY1, TileX2, TileY2;
   pUtil->GetCursorTile(X, Y, WorldX, WorldY, &TileX1, &TileY1);
   pUtil->GetCursorTile(X2, Y2, WorldX, WorldY, &TileX2, &TileY2);

   if (TileMode == SELECT_FLOOR)
   {
        if (TileX1 < 0) TileX1 = 0;
        if (TileX1 >= pMap->GetWidth()) TileX1 = pMap->GetWidth()-1;
        if (TileX2 < 0) TileX2 = 0;
        if (TileX2 >= pMap->GetWidth()) TileX2 = pMap->GetWidth()-1;

        if (TileY1 < 0) TileY1 = 0;
        if (TileY1 >= pMap->GetHeight()) TileY1 = pMap->GetHeight()-1;
        if (TileY2 < 0) TileY2 = 0;
        if (TileY2 >= pMap->GetHeight()) TileY2 = pMap->GetHeight()-1;
   } else if (TileMode == SELECT_ROOF)
   {
        if (TileX1 < -2) TileX1 = -2;
        if (TileX1 >= (int)pMap->GetWidth() - 2) TileX1 = (int)pMap->GetWidth()-3;
        if (TileX2 < -2) TileX2 = -2;
        if (TileX2 >= (int)pMap->GetWidth() - 2) TileX2 = (int)pMap->GetWidth()-3;

        if (TileY1 < -3) TileY1 = -3;
        if (TileY1 >= (int)pMap->GetHeight() - 3) TileY1 = (int)pMap->GetHeight()-4;
        if (TileY2 < -3) TileY2 = -3;
        if (TileY2 >= (int)pMap->GetHeight() - 3) TileY2 = (int)pMap->GetHeight()-4;
   }

   pTileSet->SelectTiles(TileMode,min(TileX1, TileX2), min(TileY1, TileY2),
                                  max(TileX1, TileX2), max(TileY1, TileY2));
}
//---------------------------------------------------------------------------
void TfrmEnv::ClearSelection(void)
{
   ClearFloorSelection(false);
   ClearRoofSelection(false);
   ClearObjSelection(false);
   frmEnv->panelObjSelected->Text = "Selected: " + (String)pOS->GetSelectCount();
   frmEnv->sbar->Update();
   tile_sel = 0;
   RedrawMap(true);
}
//---------------------------------------------------------------------------
void TfrmEnv::ClearFloorSelection(bool NeedRedrawMap)
{
    pTileSet->ClearFloorSelection();
    if (NeedRedrawMap) RedrawMap(true);
}
//---------------------------------------------------------------------------
void TfrmEnv::ClearRoofSelection(bool NeedRedrawMap)
{
    pTileSet->ClearRoofSelection();
    if (NeedRedrawMap) RedrawMap(true);
}
//---------------------------------------------------------------------------
void TfrmEnv::ClearObjSelection(bool NeedRedrawMap)
{
    pOS->ClearSelection();
    ResetInventoryInfo();
    RedrawInventory();
    UpdateObjPanel();
    SelectionChanged();
    if (NeedRedrawMap) RedrawMap(true);
}
//---------------------------------------------------------------------------
void TfrmEnv::MoveSelectedObjects(int offsetHexX, int offsetHexY)
{
    OBJECT * pObj;

    for (DWORD j =0; j < pOS->GetSelectCount(); j++)
    {
        pObj = pOS->GetSelectedObject(j);
        for (DWORD i = 0; i < pOS->GetObjCount(false); i++)
        {
            OBJECT * pChildObject = pOS->GetObject(i);
            if (pChildObject->IsChild() && pChildObject != pObj && pChildObject->GetHexX() == pObj->GetHexX()
                                                                && pChildObject->GetHexY() == pObj->GetHexY())
            {
                pChildObject->SetHexX(offsetHexX + pChildObject->GetHexX());
                pChildObject->SetHexY(offsetHexY + pChildObject->GetHexY());
            }
        }
        pObj->SetHexX(offsetHexX + pObj->GetHexX());
        pObj->SetHexY(offsetHexY + pObj->GetHexY());
    }
    pMap->SaveChange(LM_OBJ);
    SetButtonSave(true);
}
//---------------------------------------------------------------------------
void TfrmEnv::RotateSelectedObjects(int direction)
{
    WORD hexX, hexY;
    BYTE nType;
    int nMapX, nMapY, objX, objY;
    int nDir;

    OBJECT * pObj = NULL;
    DWORD nSelObj;

    WORD nFrameIndex;
    WORD nProtoIndex;

    BYTE nMaxDir;
    if (pOS->GetSelectCount() != 0) pMap->SaveChange(LM_OBJ);

    for (DWORD i = 0; i < pOS->GetSelectCount(); i++)
    {
        // Получаем указатель на объект
        pObj        = pOS->GetSelectedObject(i);
        nType       = pObj->GetType();
        nFrameIndex = pObj->GetFrameIndex();
        nProtoIndex = pObj->GetProtoIndex();
        nDir        = pObj->GetDirection();

        // Для криттера индекс определяется особенным образом.
        if (nType == critter_ID)
        {
            String filename = pUtil->GetFRMFileName(nType,
                              pLstFiles->pFRMlst[nType]->Strings[nFrameIndex]);
            pFrmSet->GetCritterFName(&filename, pObj->GetFrameID(), &nFrameIndex);
        }

        nMaxDir = pFrmSet->pFRM[nType][nFrameIndex].nDirTotal;
        if (nMaxDir)
           if (direction == ROTATE_CW) pObj->SetDirection(++nDir == nMaxDir ? 0 : nDir);
                else  pObj->SetDirection((int)--nDir < 0 ? nMaxDir-1 : nDir);
    }

    SetButtonSave(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::popupMapPopup(TObject *Sender)
{
   piProperties->Enabled = pOS->GetSelectCount() ? true : false;
   piDelete->Enabled = pOS->GetSelectCount() | pTileSet->dwSelection != NONE_SELECTED ? true : false;
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::piDeleteClick(TObject *Sender)
{
   if (pTileSet->dwSelection == floor_ID)
   {
      pMap->SaveChange(LM_FLOOR);
      pTileSet->SetFloorRegion(pTileSet->SelectedFloorX1,
                               pTileSet->SelectedFloorY1,
                               pTileSet->SelectedFloorX2,
                               pTileSet->SelectedFloorY2, 1);
   }
   if (pTileSet->dwSelection == roof_ID)
   {
      pMap->SaveChange(LM_ROOF);
      pTileSet->SetRoofRegion(pTileSet->SelectedRoofX1,
                              pTileSet->SelectedRoofY1,
                              pTileSet->SelectedRoofX2,
                              pTileSet->SelectedRoofY2, 1);
   }
   if (pOS->GetSelectCount())
   {
        pMap->SaveChange(LM_OBJ);
        pOS->DeleteSelected();
   }
   RedrawMap(true);
   SelectionChanged();
   SetButtonSave(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::piPropertiesClick(TObject *Sender)
{
   //frmProp->ShowProperties();
   frmObj1->Visible = true;
   UpdatePanels();
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::tabcChange(TObject *Sender)
{
   PrepareNavigator(tabc->TabIndex);
   RedrawNavigator();
   frmInv1->btnAdd->Enabled = false;
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::imgObjMouseDown(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
   int nPos = X / 83;
   if (nPos >= ObjsInPanel) return;
   nPos = nPos > ObjsInPanel - 1 ? ObjsInPanel - 1 : nPos;
   if (Navigator.nNavID[nPos] == -1) return;
   Navigator.nSelID = Navigator.nNavID[nPos];
   if (Navigator.nObjType != tile_ID)
   {
      BYTE ObjType = Navigator.nObjType == inven_ID ? item_ID : Navigator.nObjType;
      DWORD nMsgID = pProSet->pPRO[ObjType][Navigator.nSelID].GetMsgID();
      panelMSG->Text = pMsg->GetMSG(ObjType, nMsgID);
   }
   else
   {
      panelMSG->Text = "";
      switch (pTileSet->dwSelection)
      {
         case floor_ID:
            pMap->SaveChange(LM_FLOOR);
//            pTileSet->SetFloorRegion(pTileSet->SelectedFloorX1,
//                                   pTileSet->SelectedFloorY1,
//  								 pTileSet->SelectedFloorX2,
//									 pTileSet->SelectedFloorY2,
//									 Navigator.nSelID);
			RedrawMap(true);
			SetButtonSave(true);
			break;
		 case roof_ID:
			pMap->SaveChange(LM_ROOF);
//			pTileSet->SetRoofRegion(pTileSet->SelectedRoofX1,
//									pTileSet->SelectedRoofY1,
//									pTileSet->SelectedRoofX2,
//                                  pTileSet->SelectedRoofY2,
//                                  Navigator.nSelID);
            RedrawMap(true);
            SetButtonSave(true);
            break;
      }
   }
   frmInv1->btnAdd->Enabled =
        ((Navigator.nObjType == inven_ID) && pOS->GetSelectCount() == 1 && Navigator.nSelID != 0xFFFFFFFF);
   RedrawNavigator();
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::btnWorldClick(TObject *Sender)
{
   Navigator.nShowMode = SHOW_WORLD;
   PrepareNavigator(tabc->TabIndex);
   RedrawNavigator();
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::btnLocalClick(TObject *Sender)
{
   Navigator.nShowMode = SHOW_LOCAL;
   PrepareNavigator(tabc->TabIndex);
   RedrawNavigator();
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::sbScroll(TObject *Sender, TScrollCode ScrollCode,
      int &ScrollPos)
{
   RedrawNavigator();
}
//---------------------------------------------------------------------------
void TfrmEnv::RepaintDDrawWindow(TWinControl *win, LPDIRECTDRAWSURFACE7 dds,
                           LPDIRECTDRAWSURFACE7 dds2, LPDIRECTDRAWCLIPPER ddc)
{
   RECT rcs, rcd;
//   GetUpdateRect (win->Handle, &rcs, false);
   rcs = win->ClientRect;
   rcd = rcs;
   OffsetRect(&rcd, win->ClientOrigin.x, win->ClientOrigin.y);
   dds->SetClipper(ddc);
   HRESULT res = dds->Blt(&rcd, dds2, &rcs, DDBLT_ASYNC, NULL);
   if (res == DDERR_SURFACELOST)
   {
      if (dds->IsLost())
         res = dds->Restore();
      if (dds2->IsLost())
         res = dds2->Restore();
      res = dds->Blt(&rcd, dds2, &rcs, DDBLT_ASYNC, NULL);
   }
   ValidateRect (win->Handle, &rcs);
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::NewMapWndProc(TMessage &Msg)
{
    WORD key;
    WORD data;
   switch (Msg.Msg)
   {
      case WM_PAINT:
         RepaintDDrawWindow(imgMap, dds, dds2Map, ddcMap);
         Msg.Result = 0;
         break;
      case WM_KEYDOWN:
          key  = (Msg.WParamHi << 8) | Msg.WParamHi;
          data = (Msg.LParamHi << 8) | Msg.LParamLo;
          break;
      case CM_MOUSELEAVE:
         RedrawMap(true);
      default:
         OldMapWndProc(Msg);
   }
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::NewNavWndProc(TMessage &Msg)
{
   if (Msg.Msg == WM_PAINT)
   {
      RepaintDDrawWindow(imgObj, dds, dds2Nav, ddcNav);
      Msg.Result = 0;
   }
   else
   {
      OldNavWndProc(Msg);
   }
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::NewInvWndProc(TMessage &Msg)
{
   if (Msg.Msg == WM_PAINT)
   {
      RepaintDDrawWindow(frmInv1->imgInv, dds, dds2Inv, ddcInv);
      Msg.Result = 0;
   }
   else
   {
      OldInvWndProc(Msg);
   }
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::NewObjWndProc(TMessage &Msg)
{
   if (Msg.Msg == WM_PAINT)
   {
      RepaintDDrawWindow(frmObj1->imgObj, dds, dds2Obj, ddcObj);
      Msg.Result = 0;
   }
   else
   {
      OldObjWndProc(Msg);
   }
}
//---------------------------------------------------------------------------
void TfrmEnv::LogSelected(void)
{
    OBJECT * pObj;
    BYTE    nType;
    WORD    nProtoIndex;
    WORD    nX, nY;

    for (DWORD j =0; j < pOS->GetSelectCount(); j++)
    {
        pObj        = pOS->GetSelectedObject(j);
        nX          = pObj->GetHexX();
        nY          = pObj->GetHexY();
        nType       = pObj->GetType();
        nProtoIndex = pObj->GetProtoIndex();

        pLog->LogX("Object type=" + String(nType) + ", ID=" +
                    String(nProtoIndex) + "[" + String(nX) + ", " + String(nY) + "]");
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmEnv::FormKeyDown(TObject *Sender, WORD &Key,
      TShiftState Shift)
{
   switch(Key)
   {
      case VK_UP:
         WorldY -= 32;
         RedrawMap(false);
         break;
      case VK_DOWN:
         WorldY += 32;
         RedrawMap(false);
         break;
      case VK_LEFT:
         if (Shift.Contains(ssCtrl))
         {
            RotateSelectedObjects(ROTATE_CCW);
            RedrawMap(true);
            break;
         } else
         {
            WorldX -= 32;
            RedrawMap(false);
         }
         break;
      case VK_RIGHT:
         if (Shift.Contains(ssCtrl))
         {
            RotateSelectedObjects(ROTATE_CW);
            RedrawMap(true);
            break;
         } else
         {
            WorldX += 32;
            RedrawMap(false);
         }
         break;
      case VK_RETURN:
         if (Shift.Contains(ssAlt))
         break;
      case VK_DELETE:
     //    if (pOS->GetSelectCount())
            piDeleteClick(Sender);
         break;
      case VK_F5:
         if (pOS->GetSelectCount())
            LogSelected();
         break;
      case 'c':
      case 'C':
         if (Shift.Contains(ssCtrl))
         {
            if (SelectMode == SELECT_FLOOR && tile_sel != 0)
                CopySelectedFloor();
            if (SelectMode == SELECT_ROOF && tile_sel != 0)
                CopySelectedRoof();
            if (SelectMode == SELECT_OBJ && pOS->GetSelectCount())
                CopySelectedObjects();
         }
         break;
      case 'z':
      case 'Z':
         if (Shift.Contains(ssCtrl))
         {
            frmMDI->spbUndoClick(this);
         }
         break;
      case 'x':
      case 'X':
         if (Shift.Contains(ssCtrl))
         {
    //        ClearSelection();
    //        pMap->RestoreChange(RT_FORWARD);
    //        RedrawMap(true);
         }
         break;
      case 'v':
      case 'V':
         if (Shift.Contains(ssCtrl))
         {
            if (SelectMode == SELECT_FLOOR)
                ChangeMode(PASTE_FLOOR);
            if (SelectMode == SELECT_ROOF)
                ChangeMode(PASTE_ROOF);
            if (SelectMode == SELECT_OBJ)
                ChangeMode(PASTE_OBJ);
         }
        break;
   }
}

void TfrmEnv::CopySelectedObjects(void)
{
    OBJECT *pObj;
    OBJECT *pNewObj = NULL;
    _RELEASE(pBufObjHeader);
    _RELEASE(pBufObjSet);
    DWORD i;

    pBufObjHeader = new CHeader();
    pBufObjHeader->CreateNewMap();
    pBufObjHeader->SetWidth(pTileSet->GetWidth());
    pBufObjHeader->SetHeight(pTileSet->GetHeight());
    pBufObjHeader->SetVersion(FO_MAP_EDITOR_VERSION);

    pBufObjSet = new CObjSet(pBufObjHeader);
    pOS->CopySelectedTo(pBufObjSet, true);
    pBufObjSet->FindUpRightObj();
}

void TfrmEnv::CopySelectedFloor()
{
    _RELEASE(pBufFloor);
    _RELEASE(pBufFloorHeader);
    pBufFloorHeader = new CHeader();
    pBufFloorHeader->CreateNewMap();
    pBufFloorHeader->SetWidth(pTileSet->SelectedFloorX2 - pTileSet->SelectedFloorX1 + 1);
    pBufFloorHeader->SetHeight(pTileSet->SelectedFloorY2 - pTileSet->SelectedFloorY1 + 1);
    pBufFloorHeader->SetVersion(FO_MAP_EDITOR_VERSION);
    pBufFloor = new CTileSet(pBufFloorHeader);
    pBufFloor->CreateNewTileSet();
    pTileSet->CopySelectedTo(pBufFloor,0,0, COPY_FLOOR );
}

void TfrmEnv::CopySelectedRoof()
{
    _RELEASE(pBufRoof);
    _RELEASE(pBufRoofHeader);
    pBufRoofHeader = new CHeader();
    pBufRoofHeader->CreateNewMap();
    pBufRoofHeader->SetWidth(pTileSet->SelectedRoofX2 - pTileSet->SelectedRoofX1 + 1);
    pBufRoofHeader->SetHeight(pTileSet->SelectedRoofY2 - pTileSet->SelectedRoofY1 + 1);
    pBufRoofHeader->SetVersion(FO_MAP_EDITOR_VERSION);
    pBufRoof = new CTileSet(pBufRoofHeader);
    pBufRoof->CreateNewTileSet();
    pTileSet->CopySelectedTo(pBufRoof,0,0, COPY_ROOF );
}

void TfrmEnv::PasteSelectedFloor()
{
    if (!pBufFloor) return;
        
}
//---------------------------------------------------------------------------

void __fastcall TfrmEnv::editStartXKeyPress(TObject *Sender, char &Key)
{

    if ((Key < '0' || Key > '9') && Key != '\b') Key = 0;
}
//---------------------------------------------------------------------------

void __fastcall TfrmEnv::Shape9MouseDown(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
    frmHdr1->Visible = !frmHdr1->Visible;
    UpdatePanels();

}
//---------------------------------------------------------------------------


void __fastcall TfrmEnv::Shape7MouseDown(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
    frmInv1->Visible = !frmInv1->Visible;
    RedrawInventory();
    UpdatePanels();
}
//---------------------------------------------------------------------------

void __fastcall TfrmEnv::frmInv1btnChangeClick(TObject *Sender)
{
    if (Inventory.nItemNum == -1) return;
    OBJECT * pObj =  Inventory.pChildObj[Inventory.nItemNum];
    int nOld = pObj->GetCount();
    DWORD nNew = fmChange->ChangeValue(nOld);
    if (nNew != nOld)
    {
        pMap->SaveChange(LM_OBJ);
        pObj->SetCount(nNew);
        RedrawInventory();
        SetButtonSave(true);
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmEnv::frmInv1imgInvMouseDown(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
   int nPos = 2 * (Y / NAV_SIZE_Y);
   nPos += (X/NAV_SIZE_X);
   if (Inventory.pChildObj[nPos] != NULL)
      Inventory.nItemNum = nPos;
        else Inventory.nItemNum = -1;
   frmInv1->btnChange->Enabled = (Inventory.pChildObj[nPos] != NULL);
   frmInv1->btnRemove->Enabled = (Inventory.pChildObj[nPos] != NULL);
   RedrawInventory();
   UpdateObjPanel();
}
//---------------------------------------------------------------------------

void __fastcall TfrmEnv::frmInv1btnRemoveClick(TObject *Sender)
{
    pMap->SaveChange(LM_OBJ);
    Inventory.pObj->DeleteChild(Inventory.pChildObj[Inventory.nItemNum]);
    ResetInventoryInfo();
    RedrawInventory();
    frmHdr1->lbTotalCount->Caption = String(pOS->GetObjCount(true));
    SetButtonSave(true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmEnv::frmInv1btnAddClick(TObject *Sender)
{
    pMap->SaveChange(LM_OBJ);
    BYTE ObjType = Navigator.nObjType == inven_ID ? item_ID : Navigator.nObjType;
    DWORD nNew = fmChange->ChangeValue(1);
    Inventory.pObj->AddChildObject(ObjType, Navigator.nSelID, nNew);
//    ResetInventoryInfo();
    RedrawInventory();
    frmHdr1->lbTotalCount->Caption = String(pOS->GetObjCount(true));
    SetButtonSave(true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmEnv::frmInv1sbINVChange(TObject *Sender)
{
    ResetInventoryInfo();
    UpdateObjPanel();
    Inventory.nInvStartItem = (frmInv1->sbINV->Position)*2;
    RedrawInventory();
}
//---------------------------------------------------------------------------
void TfrmEnv::UpdatePanels(void)
{
    int left, top;
    left = Shape4->Left; //1040 -     frmEnv->HorzScrollBar->ScrollPos;
    top = 3;

    Shape4->Left    = left;
    Shape4->Top     = top;
    Shape4->Height  = 20;
    Shape4->Width   = 236;
    Label4->Top     = Shape4->Top + 1;
    Label4->Left    = Shape4->Left + 90;

    frmMMap1->Left   = left;
    frmMMap1->Top    = Shape4->Top + 15;
    frmMMap1->Width  = 236;
    frmMMap1->Height = 208;

    if (frmMMap1->Visible)
    {
        top = frmMMap1->Top + frmMMap1->Height + 8;
        Shape4->Pen->Color = clMaroon;
        frmMMap1->Shape2->Pen->Color = clMaroon;
    } else
    {
        top = Shape4->Top + 24;
        Shape4->Height = 18;
        Shape4->Pen->Color = 0x00789DAF;
        frmMMap1->Shape2->Pen->Color = 0x00789DAF;
    }
 
    Shape9->Left    = left;
    Shape9->Top     = top;
    Shape9->Height  = 20;
    Shape9->Width   = 236;
    Label1->Top     = Shape9->Top + 1;
    Label1->Left    = Shape9->Left + 88;

    frmHdr1->Left   = left;
    frmHdr1->Top    = Shape9->Top + 15;
    frmHdr1->Width  = 236;
    frmHdr1->Height = 248;

    if (frmHdr1->Visible)
    {
        top = frmHdr1->Top + frmHdr1->Height + 8;
        Shape9->Pen->Color = clMaroon;
        frmHdr1->Shape10->Pen->Color = clMaroon;
    } else
    {
        top = Shape9->Top + 24;
        Shape9->Height = 18;
        Shape9->Pen->Color = 0x00789DAF;
		frmHdr1->Shape10->Pen->Color = 0x00789DAF;
    }

    Shape7->Top     = top;
    Shape7->Left    = left;
    Shape7->Width   = 236;
    Shape7->Height  = 20;
    Label2->Top     = Shape7->Top + 1;
    Label2->Left    = Shape7->Left + 94;

    frmInv1->Top    = Shape7->Top + 15;
    frmInv1->Left   = left;
    frmInv1->Width  = 236;
    frmInv1->Height = 170;

    if (frmInv1->Visible)
    {
        top = frmInv1->Top + frmInv1->Height + 8;
        Shape7->Pen->Color = clMaroon;
        frmInv1->Shape1->Pen->Color = clMaroon;
    } else
    {
        top = Shape7->Top + 24;
        Shape7->Height = 18;
        Shape7->Pen->Color = 0x00789DAF;
        frmInv1->Shape1->Pen->Color = 0x00789DAF;
    }

    Shape1->Top         = top;
    Shape1->Left        = left;
    Shape1->Width       = 236;
    Shape1->Height      = 20;
    Label3->Top         = Shape1->Top + 1;
    Label3->Left        = Shape1->Left + 63;

    frmObj1->Left    = left;
    frmObj1->Top     = Shape1->Top + 15;
    frmObj1->Width   = 236;
    frmObj1->Height  = 616;

    if (frmObj1->Visible)
    {
        top = frmObj1->Top + frmObj1->Height + 8;
        Shape1->Pen->Color = clMaroon;
        frmObj1->Shape1->Pen->Color = clMaroon;
    } else
    {
        top = Shape1->Top + 24;
        Shape1->Height = 18;
        Shape1->Pen->Color = 0x00789DAF;
        frmObj1->Shape1->Pen->Color = 0x00789DAF;
    }
}
void __fastcall TfrmEnv::Shape1MouseDown(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
    frmObj1->Visible = !frmObj1->Visible;
    UpdatePanels();
    UpdateObjPanel();
}
//---------------------------------------------------------------------------

void __fastcall TfrmEnv::Shape4MouseDown(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
    frmMMap1->Visible = !frmMMap1->Visible;
    UpdatePanels();
}
//---------------------------------------------------------------------------

void __fastcall TfrmEnv::Shape4MouseMove(TObject *Sender,
      TShiftState Shift, int X, int Y)
{
    ((TShape*)Sender)->Cursor = (TCursor)crHandPoint;
}
//---------------------------------------------------------------------------

void __fastcall TfrmEnv::FormMouseMove(TObject *Sender, TShiftState Shift,
      int X, int Y)
{
//    if (Cursor != (TCursor)crDefault)
//        Cursor = (TCursor)crDefault;
}
//---------------------------------------------------------------------------
void TfrmEnv::UpdateObjPanel(void)
{
    OBJECT * pObj;
    if (Inventory.nItemNum == -1)
    {
        if (pOS->GetSelectCount() == 1)
        {
            // Указатель на выбранный объект
            pObj = pOS->GetSelectedObject(0);
        } else pObj = NULL;
    } else
    {
        pObj = Inventory.pChildObj[Inventory.nItemNum];
    }
    frmObj1->UpdateForm(pObj);
    RedrawObjPanel(pObj);
}
//---------------------------------------------------------------------------
void TfrmEnv::RedrawObjPanel(OBJECT * pObj)
{
    if (!frmObj1->Visible) return;
    HDC dc;
    // Почистим экран
    dds2Obj->GetDC (&dc);
    PatBlt(dc, 0, 0, frmObj1->imgObj->Width,frmObj1->imgObj->Height, BLACKNESS);
    dds2Obj->ReleaseDC (dc);
    frmObj1->imgObj->Repaint();

    if (!pObj) return;

    DWORD newWidth, newHeight;

    WORD nProtoIndex = pObj->GetProtoIndex();
    WORD nFrameIndex = pObj->GetFrameIndex();
    BYTE nType       = pObj->GetType();
    if (nType == item_ID && pObj->IsChild())
    {
        nType = inven_ID;
        nFrameIndex = pProSet->pPRO[item_ID][nProtoIndex].GetInvFrmID();
    }
    if (nType == critter_ID)
    {
        String filename = pUtil->GetFRMFileName(nType,
                          pLstFiles->pFRMlst[nType]->Strings[nFrameIndex]);
        pFrmSet->GetCritterFName(&filename, pObj->GetFrameID(), &nFrameIndex);
    }

    CFrame * frm = &pFrmSet->pFRM[nType][nFrameIndex];
    newWidth = frm->GetWi(0, 0);
    newWidth = newWidth > frmObj1->imgObj->Width ? frmObj1->imgObj->Width : newWidth;
    newHeight = frm->GetHe(0, 0);
    newHeight = newHeight > frmObj1->imgObj->Height ? frmObj1->imgObj->Height : newHeight;

    double ar; // aspect ratio of the frame
    if (newWidth && newHeight)
    {
        double ax = (double)frm->GetWi(0,0) / newWidth,
               ay = (double)frm->GetHe(0,0) / newHeight;
        ar = max (ax, ay);
        if (ar > .001)
        {
            newWidth = (int)frm->GetWi (0,0) / ar;
            newHeight = (int)frm->GetHe (0,0) / ar;
        }
    }

    int x = (frmObj1->imgObj->Width >> 1) - (newWidth >> 1);
    int y = (frmObj1->imgObj->Height >> 1) - (newHeight >> 1);
    RECT rcs = Rect (0,0, frm->GetWi(0,0), frm->GetHe(0,0));
    RECT rcd = Bounds (x, y, newWidth, newHeight);
    dds2Obj->Blt(&rcd, frm->GetBMP(), &rcs, DDBLT_WAIT | DDBLT_KEYSRC, NULL);
    frmObj1->imgObj->Repaint();
}

//---------------------------------------------------------------------------
void __fastcall TfrmEnv::Shape2MouseDown(TObject *Sender,
      TMouseButton Button, TShiftState Shift, int X, int Y)
{
    UpdatePanels();
}
//---------------------------------------------------------------------------

void TfrmEnv::ChangeMode(BYTE new_mode)
{
    SelectMode = new_mode;
    switch (new_mode)
    {
        case SELECT_NONE:
            frmMDI->btnhand->Click();
            frmMDI->btnhand->Down = true;

            break;
        case PASTE_FLOOR:
            if (!pBufFloor) SelectMode = SELECT_FLOOR;
              else ClearSelection();
            break;
        case PASTE_ROOF:
            if (!pBufRoof) SelectMode = SELECT_ROOF;
                else ClearSelection();
            break;
        case SELECT_FLOOR:
            ClearSelection();
        break;
        case SELECT_OBJ:
            ClearSelection();
        break;
        case PASTE_OBJ:
            if (!pBufObjSet) SelectMode = SELECT_OBJ;
                else
                {
                    ClearSelection();
                    pBufObjSet->MoveSelectedTo(HexX, HexY);
                    RedrawMap(true);
                }
            // Копируем.
//            pBufObjSet->CopySelectedTo(pOS);
        break;
    }
}




