//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "mdi.h"
#include "main.h"
#include "pbar.h"
#include "log.h"
#include "macros.h"
#include <ddraw.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmMDI *frmMDI;
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

CLog            * pLog;
CUtilites       * pUtil;
CPal            * pPal;
CListFiles      * pLstFiles;
CFrmSet         * pFrmSet;
CProSet         * pProSet;
CMsg            * pMsg;

//---------------------------------------------------------------------------
__fastcall TfrmMDI::TfrmMDI(TComponent* Owner)
        : TForm(Owner)
{
   pLog         = NULL;
   frmEnv       = NULL;
   frmPBar      = NULL;
   pDD          = NULL;
   bError       = false;
   pUtil = new CUtilites();
   pUtil->InitDirectories();
   pLog = new CLog(ExtractFilePath(Application->ExeName) + "\\mapper.log");
   this->OpenDialog1->InitialDir        = pUtil->DataDir;
   Screen->Cursors[crHandCursor]        = LoadCursor(HInstance, "hand");
   Screen->Cursors[crHandTakeCursor]    = LoadCursor(HInstance, "handtake");
   Screen->Cursors[crCrossCursor]       = LoadCursor(HInstance, "cross");
   Screen->Cursors[crMyPenCursor]       = LoadCursor(HInstance, "mypen");
   Screen->Cursors[crMoveCursor]        = LoadCursor(HInstance, "move");
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::FormCreate(TObject *Sender)
{
	this->WindowState = wsMaximized;
    btnNew->Enabled = true;
    bError = !InitClasses();

    if (bError)
        MessageDlg("Ошибка при загрузки ресурсов. Смотрите лог.", mtError, TMsgDlgButtons() << mbOK, 0);

    btnNew->Enabled     = !bError;
    btnOpen->Enabled    = !bError;
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::FormDestroy(TObject *Sender)
{
   if (pPal)        delete pPal;
   if (pUtil)       delete pUtil;
   if (pFrmSet)     delete pFrmSet;
   if (pProSet)     delete pProSet;
   if (pLstFiles)   delete pLstFiles;
   if (pMsg)        delete pMsg;
   if (pDD)         pDD->Release();
   if (pLog)        delete pLog;
}
//---------------------------------------------------------------------------
bool TfrmMDI::InitClasses(void)
{
   HRESULT res;
   if (!pPal) pPal = new CPal(); //passed
   if (pPal->lError)
   {
      delete pPal;
      pPal = NULL;
      pLog->LogX("Failed to init palette.");
      return false;
   }
   else
      pLog->LogX("Palette init ... OK.");
   if (!pLstFiles) pLstFiles = new CListFiles(); //passed
   if (pLstFiles->lError)
   {
      delete pLstFiles;
      pLstFiles = NULL;
      pLog->LogX("Failed to load LST files");
      return false;
   }
   else
      pLog->LogX("Load LST files ... OK.");

   if (!pProSet) pProSet = new CProSet(); //passed
   if (pProSet->lError)
   {
      delete pProSet;
      pProSet = NULL;
      pLog->LogX("Failed to create PRO structure.");
      return false;
   }
   else
      pLog->LogX("Create PRO structure ... OK.");
   if (!pFrmSet) pFrmSet = new CFrmSet(); //
   if (pFrmSet->lError == true)
   {
      delete pFrmSet;
      pFrmSet = NULL;
      pLog->LogX("Failed to create FRM structure.");
      return false;
   }
   else
      pLog->LogX("Create FRM structure ... OK.");

   if (!pMsg) pMsg = new CMsg(); //
   if (pMsg->lError == true)
   {
      delete pMsg;
      pMsg = NULL;
      pLog->LogX("Failed to load MSG files.");
      return false;
   }
   else
      pLog->LogX("Load MSG files ... OK.");

   if (!pDD)
   {
      res = DirectDrawCreateEx(NULL, (void**)&pDD, IID_IDirectDraw7, NULL);
      if (res == DD_OK)
         pLog->LogX("DDraw create ... OK.");
      else
      {
         pLog->LogX("DDraw create failed.");
         pLog->LogX(pUtil->GetDxError((DWORD)res));
         return false;
      }
      res = pDD->SetCooperativeLevel (0, DDSCL_NORMAL);
      if (res == DD_OK)
         pLog->LogX("Set DirectX cooperative level ... OK.");
      else
      {
         pLog->LogX("Failed to set DirectX cooperative level");
         pLog->LogX(pUtil->GetDxError((DWORD)res));
         return false;
      }
   }

    // Загружаем прототипы.
    pProSet->ClearLocals();
    pFrmSet->ClearLocals();
    pFrmSet->LoadLocalFRMs();           // Грузим интерфейс

    return true;
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnOpenClick(TObject *Sender)
{
    if (OpenDialog1->Execute())
    {
        CMap * map = new CMap;
        // Загружаем карту.
        pLog->LogX("Load map \"" + OpenDialog1->FileName + "\".");
   /*     if (frmEnv)
        {
            String FileName;
            String DirName;
            pUtil->GetfileName(OpenDialog1->FileName, FileName, DirName);
            for (int i = 0; i < frmEnv->frmHdr1->lbMaps->Items->Count; i++)
            {
                if (frmEnv->frmHdr1->lbMaps->Items->Strings[i] == )
            }
        } */

        if (!map->LoadFromFile(OpenDialog1->FileName))
        {
            delete map;
            return;
        }
        // Если загружается первая карта, то создаем форму.
        if (!frmEnv) Application->CreateForm(__classid(TfrmEnv), &frmEnv);
        // Добавляем новую карту.
        frmEnv->AddMap(map, true);

        tbcurs->Visible     = true;
        tbVis->Visible      = true;
        tbVis->Visible      = true;
        tbZoom->Visible     = true;

        btnSaveAs->Enabled  = true;
        btnSaveAs->Enabled  = true;
    }
}

//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnSaveClick(TObject *Sender)
{
    CMap * map = frmEnv->GetCurMap();
    if (map->GetFileName() != "noname.map")
        map->SaveToFile(map->GetFullFileName());
    frmEnv->SetButtonSave(false);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnSaveAsClick(TObject *Sender)
{
    if (SaveDialog1->Execute())
    {
        CMap * map = frmEnv->GetCurMap();
        // для карты устанавливаем новый файл.
        map->SetFullFileName(SaveDialog1->FileName);
        frmEnv->Label7->Caption = "Карта - " + map->GetFileName();
        int index = frmEnv->frmHdr1->lbMaps->ItemIndex;
        frmEnv->frmHdr1->lbMaps->Items->Strings[index] = map->GetFileName();
        btnSaveClick(Sender);
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnhandClick(TObject *Sender)
{
   frmEnv->imgMap->Cursor = (TCursor)crHandCursor;
   frmEnv->SelectMode = SELECT_NONE;
   frmEnv->ClearSelection();
}
//---------------------------------------------------------------------------
void TfrmMDI::OpenPBarForm(void)
{
   frmPBar = new TfrmPBar(this);
}
//---------------------------------------------------------------------------
void TfrmMDI::DeletePBarForm(void)
{
   delete frmPBar;
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnFloorClick(TObject *Sender)
{
   frmEnv->bShowObj[floor_ID] = btnFloor->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnRoofClick(TObject *Sender)
{
   frmEnv->bShowObj[roof_ID] = btnRoof->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnItemsClick(TObject *Sender)
{
   frmEnv->bShowObj[item_ID] = btnItems->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnCrittersClick(TObject *Sender)
{
   frmEnv->bShowObj[critter_ID] = btnCritters->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnSceneryClick(TObject *Sender)
{
   frmEnv->bShowObj[scenery_ID] = btnScenery->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnWallsClick(TObject *Sender)
{
   frmEnv->bShowObj[wall_ID] = btnWalls->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnMiscClick(TObject *Sender)
{
   frmEnv->bShowObj[misc_ID] = btnMisc->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnTGBlockClick(TObject *Sender)
{
   frmEnv->bShowObj[TG_blockID] = btnTGBlock->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnEGBlockClick(TObject *Sender)
{
   frmEnv->bShowObj[EG_blockID] = btnEGBlock->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnScrollBlockClick(TObject *Sender)
{
   frmEnv->bShowObj[scroll_blockID] = btnScrollBlock->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnmoveClick(TObject *Sender)
{
   frmEnv->imgMap->Cursor = (TCursor)crMoveCursor;
   frmEnv->SelectMode = MOVE_OBJ;
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnselect1Click(TObject *Sender)
{
   frmEnv->imgMap->Cursor = (TCursor)crCrossCursor;
   frmEnv->SelectMode = SELECT_ROOF;
   frmEnv->ClearSelection();
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnselect2Click(TObject *Sender)
{
   frmEnv->imgMap->Cursor = (TCursor)crCrossCursor;
   frmEnv->SelectMode = SELECT_OBJ;
   frmEnv->ClearSelection();
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnselect3Click(TObject *Sender)
{
   frmEnv->imgMap->Cursor = (TCursor)crCrossCursor;
   frmEnv->SelectMode = SELECT_FLOOR;
   frmEnv->ClearSelection();
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnpen1Click(TObject *Sender)
{
   frmEnv->imgMap->Cursor = (TCursor)crMyPenCursor;
   frmEnv->SelectMode = DRAW_FLOOR;
   frmEnv->ClearSelection();
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnpen2Click(TObject *Sender)
{
   frmEnv->imgMap->Cursor = (TCursor)crMyPenCursor;
   frmEnv->SelectMode = DRAW_OBJ;
   frmEnv->ClearSelection();
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnpen3Click(TObject *Sender)
{
   frmEnv->imgMap->Cursor = (TCursor)crMyPenCursor;
   frmEnv->SelectMode = DRAW_ROOF;
   frmEnv->ClearSelection();
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnConfigClick(TObject *Sender)
{
   pUtil->MapperConfiguration();
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnAboutClick(TObject *Sender)
{
   Application->MessageBox(
       "Mapper Version 0.98c (24.02.2003)\n"
       "Code by Dims <dims@hot.ee>\n"
       "DirectX support by ABel <abel@krasu.ru>\n"
       "Upped interface to DirectX 7 by Dims <dims@hot.ee>\n",
       "About mapper", MB_OK);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnSaiBlockClick(TObject *Sender)
{
   frmEnv->bShowObj[SAI_blockID] = btnSaiBlock->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnWallBlockClick(TObject *Sender)
{
   frmEnv->bShowObj[wall_blockID] = btnWallBlock->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnObjBlockClick(TObject *Sender)
{
   frmEnv->bShowObj[obj_blockID] = btnObjBlock->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnLightBlockClick(TObject *Sender)
{
   frmEnv->bShowObj[light_blockID] = btnLightBlock->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnObjSelfBlockClick(TObject *Sender)
{
   frmEnv->bShowObj[obj_self_blockID] = btnObjSelfBlock->Down;
   frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::btnNewClick(TObject *Sender)
{
    CMap * map = new CMap;
    // Если загружается первая карта, то создаем форму.
    if (!frmEnv) Application->CreateForm(__classid(TfrmEnv), &frmEnv);

    // Загружаем карту.
    pLog->LogX("Load map \"" + OpenDialog1->FileName + "\".");
    map->CreateNewMap(FO_MAP_WIDTH, FO_MAP_HEIGHT);
    // Добавляем новую карту.
    frmEnv->AddMap(map, true);

    tbcurs->Visible     = true;
    tbVis->Visible      = true;
    tbVis->Visible      = true;
    tbZoom->Visible     = true;

    btnSaveAs->Enabled  = true;
    btnSaveAs->Enabled  = true;
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::sbHexClick(TObject *Sender)
{
    frmEnv->RedrawMap(true);
}
void TfrmMDI::DisableInterface(void)
{
    frmEnv->ChangeMode(SELECT_NONE);
    tbcurs->Enabled = false;
    sbHex->Enabled = false;
}

void TfrmMDI::EnableInterface(void)
{
    tbcurs->Enabled = true;
    sbHex->Enabled = true;
}
//---------------------------------------------------------------------------
void __fastcall TfrmMDI::SpeedButton1Click(TObject *Sender)
{
    frmEnv->fAspect = 1.0f;
    EnableInterface();
    frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmMDI::SpeedButton2Click(TObject *Sender)
{
    frmEnv->fAspect = 2.0f;
    DisableInterface();
    frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmMDI::SpeedButton3Click(TObject *Sender)
{
    frmEnv->fAspect = 3.0f;
    DisableInterface();
    frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmMDI::SpeedButton4Click(TObject *Sender)
{
    frmEnv->fAspect = 4.0f;
    DisableInterface();
    frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmMDI::SpeedButton5Click(TObject *Sender)
{
    frmEnv->fAspect = 5.0f;
    DisableInterface();
    frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmMDI::SpeedButton6Click(TObject *Sender)
{
    frmEnv->fAspect = 6.0f;
    DisableInterface();
    frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmMDI::SpeedButton7Click(TObject *Sender)
{
    frmEnv->fAspect = 7.0f;
    DisableInterface();
    frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmMDI::SpeedButton8Click(TObject *Sender)
{
    frmEnv->fAspect = 8.0f;
    DisableInterface();
    frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmMDI::SpeedButton9Click(TObject *Sender)
{
    frmEnv->fAspect = 9.0f;
    DisableInterface();
    frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------


void __fastcall TfrmMDI::spbUndoClick(TObject *Sender)
{
    frmEnv->ClearSelection();
    CMap * map = frmEnv->GetCurMap();
    map->RestoreChange(RT_BACK);
    frmEnv->RedrawMap(true);
}
//---------------------------------------------------------------------------



