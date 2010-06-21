//////////////////////////////////////////////////////////////////////
// CUtilites Class
//////////////////////////////////////////////////////////////////////

#include "utilites.h"
#include "config.h"
#include "macros.h"
#include "datfile.h"
//#include "winrustb.h"
//#include <classes.hpp>
#include <WindowsX.h>
#include <registry.hpp>
#include <shlobj.h>
#include <ddraw.h>

//---------------------------------------------------------------------------
CUtilites::CUtilites(void)
{
    HWND desctop = GetDesktopWindow();
    RECT rc;
    GetWindowRect(desctop, &rc);
   if (rc.right == 1280)
   {
  //    MAP_SCREEN_WIDTH = 1024;
  //    MAP_SCREEN_HEIGHT = 768;
		MAP_SCREEN_WIDTH = 800;
		MAP_SCREEN_HEIGHT = 600;
   } else
   {
       MAP_SCREEN_WIDTH = 768;
       MAP_SCREEN_HEIGHT = 512;
   }

   lError = true;
   DataDir = "";
   GameDir = "";
   char TempBuf[256];
   GetCurrentDirectory(256, TempBuf);
   MapperDir = (String)TempBuf;

   IDname[0] = "items";
   IDname[1] = "critters";
   IDname[2] = "scenery";
   IDname[3] = "walls";
   IDname[4] = "tiles";
   IDname[5] = "misc";
   IDname[6] = "inven";
   pPatchDAT = NULL;
   pMasterDAT = NULL;
   pCritterDAT = NULL;
   pSuffix = NULL;

   selColor = clSilver;
   pSuffix = new TStringList();
   pSuffix->Text =
         "AA.FRM\r\nAB.FRM\r\nAE.FRM\r\nAG.FRM\r\nAK.FRM\r\nAL.FRM\r\nAN.FRM\r\nAO.FRM\r\n"
         "AP.FRM\r\nAQ.FRM\r\nAR.FRM\r\nAS.FRM\r\nAT.FRM\r\nAU.FRM\r\nBA.FR0\r\nBA.FR1\r\n"
         "BA.FR2\r\nBA.FR3\r\nBA.FR4\r\nBA.FR5\r\nBA.FRM\r\nBB.FR0\r\nBB.FR1\r\nBB.FR1\r\n"
         "BB.FR3\r\nBB.FR4\r\nBB.FRM\r\nBC.FR0\r\nBC.FR1\r\nBC.FR2\r\nBC.FR3\r\nBC.FR4\r\n"
         "BC.FR5\r\nBD.FR0\r\nBD.FR1\r\nBD.FR2\r\nBD.FR3\r\nBD.FR4\r\nBD.FR5\r\nBD.FRM\r\n"
         "BE.FR0\r\nBE.FR1\r\nBE.FR2\r\nBE.FR3\r\nBE.FR4\r\nBE.FR5\r\nBE.FRM\r\nBF.FR0\r\n"
         "BF.FR1\r\nBF.FR2\r\nBF.FR3\r\nBF.FR4\r\nBF.FR5\r\nBF.FRM\r\nBG.FR0\r\nBG.FR1\r\n"
         "BG.FR2\r\nBG.FR3\r\nBG.FR4\r\nBG.FR5\r\nBG.FRM\r\nBH.FR0\r\nBH.FR1\r\nBH.FR2\r\n"
         "BH.FR3\r\nBH.FR4\r\nBH.FR5\r\nBH.FR5\r\nBH.FRM\r\nBI.FR0\r\nBI.FR1\r\nBI.FR2\r\n"
         "BI.FR3\r\nBI.FR4\r\nBI.FR5\r\nBI.FRM\r\nBJ.FR0\r\nBJ.FR1\r\nBJ.FR2\r\nBJ.FR3\r\n"
         "BJ.FR4\r\nBJ.FR5\r\nBJ.FR5\r\nBK.FR0\r\nBK.FR1\r\nBK.FR2\r\nBK.FR3\r\nBK.FR4\r\n"
         "BK.FR5\r\nBK.FRM\r\nBL.FR0\r\nBL.FR1\r\nBL.FR2\r\nBL.FR3\r\nBL.FR4\r\nBL.FR5\r\n"
         "BL.FRM\r\nBM.FR0\r\nBM.FR2\r\nBM.FR3\r\nBM.FR4\r\nBM.FR5\r\nBM.FRM\r\nBN.FRM\r\n"
         "BO.FR0\r\nBO.FR1\r\nBO.FR2\r\nBO.FR3\r\nBO.FR4\r\nBO.FR5\r\nBO.FRM\r\nBP.FR0\r\n"
         "BP.FR1\r\nBP.FR2\r\nBP.FR3\r\nBP.FR4\r\nBP.FR5\r\nBP.FRM\r\nCH.FRM\r\nCJ.FRM\r\n"
         "DA.FRM\r\nDB.FRM\r\nDC.FRM\r\nDD.FRM\r\nDE.FRM\r\nDF.FRM\r\nDF.FRM\r\nDG.FRM\r\n"
         "DM.FRM\r\nEA.FRM\r\nEB.FRM\r\nEC.FRM\r\nED.FRM\r\nEE.FRM\r\nEF.FRM\r\nEG.FRM\r\n"
         "FA.FRM\r\nFB.FRM\r\nFC.FRM\r\nFD.FRM\r\nFE.FRM\r\nFG.FRM\r\nGA.FRM\r\nGB.FRM\r\n"
         "GC.FRM\r\nGD.FRM\r\nGE.FRM\r\nGF.FRM\r\nGG.FRM\r\nGM.FRM\r\nHA.FRM\r\nHB.FRM\r\n"
         "HC.FRM\r\nHD.FRM\r\nHE.FRM\r\nHH.FRM\r\nHI.FRM\r\nHJ.FRM\r\nIA.FRM\r\nIC.FRM\r\n"
         "ID.FRM\r\nIE.FRM\r\nIH.FRM\r\nII.FRM\r\nIJ.FRM\r\nJA.FRM\r\nJB.FRM\r\nJC.FRM\r\n"
         "JD.FRM\r\nJE.FRM\r\nJH.FRM\r\nJI.FRM\r\nJJ.FRM\r\nJK.FRM\r\nKA.FRM\r\nKB.FRM\r\n"
         "KC.FRM\r\nKD.FRM\r\nKE.FRM\r\nKH.FRM\r\nKI.FRM\r\nKJ.FRM\r\nKK.FRM\r\nKL.FRM\r\n"
         "LA.FRM\r\nLB.FRM\r\nLC.FRM\r\nLD.FRM\r\nLE.FRM\r\nLH.FRM\r\nLI.FRM\r\nLK.FRM\r\n"
         "MA.FRM\r\nMC.FRM\r\nMD.FRM\r\nME.FRM\r\nMH.FRM\r\nMI.FRM\r\nMJ.FRM\r\nNA.FRM\r\n"
         "RA.FRM\r\nRB.FRM\r\nRC.FRM\r\nRD.FRM\r\nRE.FRM\r\nRF.FRM\r\nRG.FRM\r\nRH.FRM\r\n"
         "RI.FRM\r\nRJ.FRM\r\nRK.FRM\r\nRL.FRM\r\nRM.FRM\r\nRO.FRM\r\nRP.FRM\r\n";
   pSuffix->Sorted = true;
   lError = false;
}
void CUtilites::GetFileName(String FullFileName, String * FileName, String * DirName, String * Name, String * Ext)
{
    String file_name, dir_name;
    String name, ext;
    int k, j;
    if (!FileName) return;
    for (k = FullFileName.Length(); k >= 1 ; k-- )
        if (FullFileName[k] == '\\') break;
    file_name.SetLength(FullFileName.Length() - k);
    for (j = k + 1; j <= FullFileName.Length(); j++)
        file_name[j - k] = FullFileName[j];

    name.SetLength(file_name.Length());
    ext.SetLength(file_name.Length());

    if (Name && Ext)
    {
        for (int t = 1; t <= file_name.Length(); t++ )
        {
            if (file_name[t] == '.')
            {
                for (int r = t+1; r <= file_name.Length(); r++  )
                    ext[r - t] = file_name[r];
                ext[file_name.Length() - t + 1] = 0;
                name[t] = 0;
                break;
            }
            name[t] = file_name[t];
        }
        *Name = name;
        *Ext  = ext;
    }

    *FileName = file_name;
    if (!DirName) return;
    dir_name.SetLength(k);
    for ( j = 1; j <= k; j++)
        dir_name[j] = FullFileName[j];
    *DirName = dir_name;
}
//---------------------------------------------------------------------------
void CUtilites::InitDirectories(void)
{
   if (GameDir.IsEmpty())
   {
      FirstRun = true;
      GameDir = GetRegInfoS("\\SOFTWARE\\Dims\\mapper", "GamePath");
   }
   else
      FirstRun = false;
   if (GameDir.IsEmpty())
      GameDir = GetRegInfoS("\\SOFTWARE\\Interplay\\Fallout2\\1.0", "DestPath");

   DataDir = GetRegInfoS("\\SOFTWARE\\Dims\\mapper", "DataPath");
   if (DataDir.IsEmpty())
      DataDir = GameDir + "\\data";
   TemplatesDir = GetRegInfoS("\\SOFTWARE\\Dims\\mapper", "TemplatesPath");
   if (TemplatesDir.IsEmpty())
      TemplatesDir = MapperDir + "\\Templates";
}
//---------------------------------------------------------------------------
void CUtilites::MapperConfiguration(void)
{
   frmConfig->ShowDialog();
}
//---------------------------------------------------------------------------
String CUtilites::GetFolder(String Caption)
{
   BROWSEINFO bi;
   char GDir[MAX_PATH];
   char FolderName[MAX_PATH];
   LPITEMIDLIST ItemID;
   memset(&bi, 0, sizeof(BROWSEINFO));
   memset(GDir, 0, MAX_PATH);
   bi.hwndOwner      = Application->Handle;
   bi.pszDisplayName = FolderName;
   bi.lpszTitle      = Caption.c_str();
   ItemID = SHBrowseForFolder(&bi);
   SHGetPathFromIDList(ItemID, GDir);
   GlobalFreePtr(ItemID);
   return String(GDir);
}
//---------------------------------------------------------------------------
String CUtilites::GetRegInfoS(String RegKey, String KeyName)
{
    String RetVal = "";
    TRegistry *MyRegistry = new TRegistry();
    MyRegistry->RootKey = HKEY_LOCAL_MACHINE;
    try
    {
       if(MyRegistry->OpenKey(RegKey, false))
       {
          RetVal = MyRegistry->ReadString(KeyName);
          MyRegistry->CloseKey();
       }
    }
    catch(ERegistryException &E)
    {
       Application->MessageBoxA(E.Message.c_str(), "Error", MB_OK);
    }
    delete MyRegistry;
    return RetVal;
}
//--------------------------------------------------------------------------
bool CUtilites::SetRegInfoS(String RegKey, String KeyName, String Value)
{
    bool RetVal = false;
    TRegistry *MyRegistry = new TRegistry();
    MyRegistry->RootKey = HKEY_LOCAL_MACHINE;
    try
    {
       if(MyRegistry->OpenKey(RegKey, true))
       {
          MyRegistry->WriteString(KeyName, Value);
          MyRegistry->CloseKey();
          RetVal = true;
       }
    }
    catch(ERegistryException &E)
    {
       Application->MessageBoxA(E.Message.c_str(), "Error", MB_OK);
    }
    delete MyRegistry;
    return RetVal;
}
//---------------------------------------------------------------------------
HANDLE CUtilites::OpenFileX(String sFile)
{
//           Init and indexing dat files
   if (pPatchDAT == NULL)
      pPatchDAT = new TDatFile(GameDir + "\\patch000.dat");
   if (pMasterDAT == NULL)
      pMasterDAT = new TDatFile(GameDir + "\\master.dat");
   if (pCritterDAT == NULL)
      pCritterDAT = new TDatFile(GameDir + "\\critter.dat");

   HANDLE hFile;

   String sResFile = MapperDir + "\\" + sFile;
   hFile = CreateFile(sResFile.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
   if (hFile != INVALID_HANDLE_VALUE) return hFile;

   String sFileOnDisk = DataDir + "\\" + sFile;
   hFile = CreateFile(sFileOnDisk.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
   if (hFile != INVALID_HANDLE_VALUE) return hFile;
   hFile = pPatchDAT->DATOpenFile(sFile);
   if (hFile != INVALID_HANDLE_VALUE) return hFile;
   hFile = pMasterDAT->DATOpenFile(sFile);
   if (hFile != INVALID_HANDLE_VALUE) return hFile;
   hFile = pCritterDAT->DATOpenFile(sFile);

   return hFile;
}
//---------------------------------------------------------------------------
void CUtilites::SetFilePointerX(HANDLE hFile, LONG lDistanceToMove,
                                                             DWORD dwMoveMethod)
{
   TDatFile *pDAT = NULL;
   int i;
   if (hFile == pPatchDAT->h_in)
      pDAT = pPatchDAT;
   if (hFile == pMasterDAT->h_in)
      pDAT = pMasterDAT;
   if (hFile == pCritterDAT->h_in)
      pDAT = pCritterDAT;
   if (pDAT)
      i = pDAT->DATSetFilePointer(lDistanceToMove, dwMoveMethod);
   else
      i = SetFilePointer(hFile, lDistanceToMove, NULL, dwMoveMethod);
}
//---------------------------------------------------------------------------
void CUtilites::ReadFileX(HANDLE hFile, LPVOID lpBuffer,
                        DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead)
{
   TDatFile *pDAT = NULL;
   if (hFile == pPatchDAT->h_in)
      pDAT = pPatchDAT;
   if (hFile == pMasterDAT->h_in)
      pDAT = pMasterDAT;
   if (hFile == pCritterDAT->h_in)
      pDAT = pCritterDAT;
   if (pDAT)
      pDAT->DATReadFile(lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead);
   else
      ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, NULL);
}
//---------------------------------------------------------------------------
DWORD CUtilites::GetFileSizeX(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
   TDatFile *pDAT = NULL;
   if (hFile == pPatchDAT->h_in)
      pDAT = pPatchDAT;
   if (hFile == pMasterDAT->h_in)
      pDAT = pMasterDAT;
   if (hFile == pCritterDAT->h_in)
      pDAT = pCritterDAT;
   if (pDAT)
      return pDAT->DATGetFileSize();
   else
      return GetFileSize(hFile, lpFileSizeHigh);
}
//---------------------------------------------------------------------------
bool CUtilites::CloseHandleX(HANDLE hFile)
{
   TDatFile *pDAT = NULL;
   if (hFile == pPatchDAT->h_in)
      pDAT = pPatchDAT;
   if (hFile == pMasterDAT->h_in)
      pDAT = pMasterDAT;
   if (hFile == pCritterDAT->h_in)
      pDAT = pCritterDAT;
   if (pDAT)
      return true;
   else
      return CloseHandle(hFile);
}
//---------------------------------------------------------------------------
String CUtilites::GetFRMFileName(BYTE nObjType, String FileName)
{
   return ("art\\" + IDname[nObjType] + "\\" + FileName);
}
//---------------------------------------------------------------------------
void CUtilites::ReverseDW(DWORD *addr)
{
   BYTE *b, tmp;
   b = (BYTE*)addr;
   tmp = *(b + 3);
   *(b + 3) = *b;
   *b = tmp;
   tmp = *(b + 2);
   *(b + 2) = *(b + 1);
   *(b + 1) = tmp;
}
//---------------------------------------------------------------------------
void CUtilites::ReverseW(WORD *addr)
{
   BYTE *b, tmp;
   b = (BYTE*)addr;
   tmp = *(b + 1);
   *(b + 1) = *b;
   *b = tmp;
}
//------------------------------------------------------------------------------
DWORD CUtilites::GetDW(DWORD *addr)
{
   DWORD retv = *addr;
   DWORD *pretv = &retv;
   BYTE *b, tmp;
   b = (BYTE*)pretv;
   tmp = *(b + 3);
   *(b + 3) = *b;
   *b = tmp;
   tmp = *(b + 2);
   *(b + 2) = *(b + 1);
   *(b + 1) = tmp;
   return (DWORD)retv;
}
//---------------------------------------------------------------------------
WORD CUtilites::GetW(WORD *addr)
{
   WORD retv = *addr;
   WORD *pretv = &retv;
   BYTE *b, tmp;
   b = (BYTE*)pretv;
   tmp = *(b + 1);
   *(b + 1) = *b;
   *b = tmp;
   return (WORD)retv;
}
//------------------------------------------------------------------------------
void CUtilites::GetTileWorldCoord(int TileX, int TileY, int *WorldX, int *WorldY)
{
   *WorldX = (FO_MAP_WIDTH - 1) * 48 + (32 * TileY) - (48 * TileX);
   *WorldY = (24 * TileY) + (12 * TileX);
}
//------------------------------------------------------------------------------
void CUtilites::GetCursorTile(int X, int Y, int wX, int wY,
                                                         int *TileX, int *TileY)
{
   int off_x = -(FO_MAP_WIDTH * 48) + (wX + X);
   int off_y = wY + Y;
   int xx = -off_x + off_y * 4 / 3;
   (*TileX) = xx / 64;
   if (xx < 0) (*TileX)--;
   int yy = off_y + off_x / 4;
   (*TileY) = yy / 32;
   if (yy < 0) (*TileY)--;
}
//------------------------------------------------------------------------------
void CUtilites::GetWorldCoord(int X, int Y, int *WorldX, int *WorldY)
{
   X = (FO_MAP_WIDTH - 1) - X;
    // Делает тоже самое, что и GetTileWorldCoord, но смещает в центр экрана.
   *WorldX = (FO_MAP_WIDTH - 1)*48 - (48 * X) + (Y << 5) - MAP_SCREEN_WIDTH / 2;
   *WorldY = (12 * X) + (24 * Y) - MAP_SCREEN_HEIGHT / 2;
}
//------------------------------------------------------------------------------
void CUtilites::GetHexWorldCoord(int X, int Y, int *WorldX, int *WorldY)
{
//  *WorldX = 4816 - (32 * ((X + 1) >> 1) + (16 * (X >> 1)) - (Y * 16)); //ver1
//  *WorldY = ((12 * (X >> 1)) + (Y * 12)) + 11; // + 8
  *WorldX = (FO_MAP_WIDTH * 48) + 16 - ((((X + 1) >> 1) << 5 )+ ((X >> 1) << 4) - (Y << 4)); //ver1
  *WorldY = ((12 * (X >> 1)) + (Y * 12)) + 11; // + 8
}
//------------------------------------------------------------------------------
void CUtilites::GetCursorHex(int X, int Y, int WorldX, int WorldY,
                                                         int *HexX, int *HexY)
{
  int x0 = (FO_MAP_WIDTH * 48);
  int y0 = 0; 
  X += WorldX;
  Y += WorldY;
  long nx;

  if (X - x0 < 0)
     nx = (X - x0 + 1) / 16 - 1;
  else
     nx = (X - x0) / 16;

   long ny;

   if (Y - y0 < 0)
      ny = (Y - y0 + 1) / 12 - 1;
   else
      ny = (Y - y0) / 12;

   if (abs(nx) % 2 != abs(ny) % 2)
      nx--;

   long xhBase = x0 + 16 * nx;
   long yhBase = y0 + 12 * ny;

   *HexX = (4 * (yhBase - y0) - 3 * (xhBase - x0)) / 96;
   *HexY = (yhBase - y0) / 12 - *HexX / 2;

   long dx = X - xhBase;
   long dy = Y - yhBase;

   switch(dy)
   {
      case 0:
         if (dx < 12)
         {
	    *HexY--;
	    break;
	 }
	 if (dx > 18)
         {
	    if (*HexX % 2 == 1)
	       *HexY--;
            *HexX--;
            break;
         }

      case 1:
         if (dx < 8)
         {
	    *HexX--;
	    break;
	 }
	 if (dx > 23)
         {
	    if (*HexX % 2 == 1)
	       *HexY--;
	    *HexX--;
	    break;
	 }

      case 2:
	 if (dx < 4)
         {
	    *HexY--;
	    break;
	 }
         if (dx > 28)
         {
	    if (*HexX % 2 == 1)
	       *HexY--;
            *HexX--;
	    break;
	 }
      default:
         break;
   }
}
//------------------------------------------------------------------------------
bool CUtilites::GetListFromFile(String filename, TStringList *pList)
{
   DWORD dwFileSize, dwFileSize2;
   HANDLE h_lst;
   BYTE *lstbuf;
   ULONG i;
   h_lst = OpenFileX(filename);
   if (h_lst == INVALID_HANDLE_VALUE)
      return false;
   dwFileSize = GetFileSizeX(h_lst, &dwFileSize2);
   if ((lstbuf = (BYTE *)malloc(dwFileSize + 1)) == NULL)
   {
      CloseHandleX (h_lst);
      return false;
   }
   ReadFileX(h_lst, lstbuf, dwFileSize, &i);
   CloseHandleX (h_lst);
   lstbuf[dwFileSize] = 0x00;
   pList->SetText(lstbuf);
   free(lstbuf);
}
//------------------------------------------------------------------------------
String CUtilites::GetMsgValue(String MsgLine, int ValuePos)
{
   String MsgValue;
   for (int i = 0; i < ValuePos; i++)
   {
      int Pos1 = MsgLine.Pos("{");
      int Pos2 = MsgLine.Pos("}");
      if (Pos2 < Pos1 || !Pos1 || !Pos2)
         return "";
      MsgValue = MsgLine.SubString(Pos1 + 1, Pos2 - Pos1 - 1);
      MsgLine = MsgLine.SubString(Pos2 + 1, MsgLine.Length());
   }
   return MsgValue;
}
//------------------------------------------------------------------------------
String CUtilites::GetName(String str)
{
   int DelimeterPosition;
   DelimeterPosition = str.Pos(";");
   if (DelimeterPosition) str.SetLength(--DelimeterPosition);
   return Trim(str);
}
//------------------------------------------------------------------------------
String CUtilites::GetDescription(String str)
{
   int DelimeterPosition, Delimeter2Position, StringLenght;
   StringLenght = str.Length();
   DelimeterPosition = str.Pos(";");
   Delimeter2Position = str.Pos("#");
   StringLenght = Delimeter2Position - DelimeterPosition;
   if (StringLenght > 0)
      str = str.SubString(DelimeterPosition + 1, StringLenght - 1);
   return Trim(str);
}
//------------------------------------------------------------------------------
void CUtilites::RetranslateString(char *ptr)
{
   OemToChar(ptr, ptr);
//   while (*ptr)
//   {
//      *ptr = TableWinRus[(BYTE)(*ptr)];
//      ptr++;
//   }
}
//------------------------------------------------------------------------------
int CUtilites::GetBlockType(BYTE nProObjType, WORD nProID)
{
   if (nProObjType == scenery_ID && nProID == 128)     return TG_blockID;
   if (nProObjType == scenery_ID && nProID == 0x31)     return EG_blockID;
   if (nProObjType == scenery_ID && nProID == 0x0158)   return SAI_blockID;
   if (nProObjType == scenery_ID && nProID == 0x43)     return obj_blockID;
   if (nProObjType == scenery_ID && nProID == 0x8d)     return light_blockID;
   if (nProObjType == wall_ID && nProID == 0x026e)      return wall_blockID;
   if (nProObjType == misc_ID && nProID == 0x05)        return scroll_blockID;
   if (nProObjType == misc_ID && nProID == 0x0c)        return scroll_blockID;
   return 0;
}
//------------------------------------------------------------------------------
void CUtilites::GetBlockFrm(int nBlockType, BYTE *nObjType, WORD *nFrmID)
{
   switch (nBlockType)
   {
   case TG_blockID:
		 *nObjType = intrface_ID;
		 *nFrmID = TG_blockID;
		 break;
	  case EG_blockID:
         *nObjType = intrface_ID;
		 *nFrmID = EG_blockID;
         break;
      case SAI_blockID:
         *nObjType = intrface_ID;
         *nFrmID = SAI_blockID;
         break;
      case wall_blockID:
         *nObjType = intrface_ID;
         *nFrmID = wall_blockID;
         break;
      case obj_blockID:
         *nObjType = intrface_ID;
         *nFrmID = obj_blockID;
         break;
      case light_blockID:
         *nObjType = intrface_ID;
         *nFrmID = light_blockID;
         break;
      case scroll_blockID:
         *nObjType = intrface_ID;
         *nFrmID = scroll_blockID;
         break;
      case obj_self_blockID:
         *nObjType = intrface_ID;
         *nFrmID = obj_self_blockID;
         break;
   }
}
//------------------------------------------------------------------------------
DWORD CUtilites::GetIndexBySuffix(String Suffix)
{
   bool Found;
   int Index;
   if (pSuffix->Find(Suffix.UpperCase(), Index))
   {
      return (DWORD)++Index;
   }
   else
   {
      return 0;
   }
}
//------------------------------------------------------------------------------
String CUtilites::GetDxError(DWORD dwErrCode)
{
   switch(dwErrCode)
   {
      case DDERR_ALREADYINITIALIZED:
         return
         "This object is already initialized";
      case DDERR_CANNOTATTACHSURFACE:
         return
         "This surface can not be attached to the requested surface.";
      case DDERR_CANNOTDETACHSURFACE:
         return
         "This surface can not be detached from the requested surface.";
      case DDERR_CURRENTLYNOTAVAIL:
         return
         "Support is currently not available.";
      case DDERR_EXCEPTION:
         return
         "An exception was encountered while performing the requested operation";
      case DDERR_GENERIC:
         return
         "Generic failure.";
      case DDERR_HEIGHTALIGN:
         return
         "Height of rectangle provided is not a multiple of reqd alignment";
      case DDERR_INCOMPATIBLEPRIMARY:
         return
         "Unable to match primary surface creation request with existing "
         "primary surface.";
      case DDERR_INVALIDCAPS:
         return
         "One or more of the caps bits passed to the callback are incorrect.";
      case DDERR_INVALIDCLIPLIST:
          return
          "DirectDraw does not support provided Cliplist.";
      case DDERR_INVALIDMODE:
         return
         "DirectDraw does not support the requested mode";
      case DDERR_INVALIDOBJECT:
         return
         "DirectDraw received a pointer that was an invalid DIRECTDRAW object.";
      case DDERR_INVALIDPARAMS:
         return
         "One or more of the parameters passed to the callback function are "
         "incorrect.";
      case DDERR_INVALIDPIXELFORMAT:
         return
         "pixel format was invalid as specified";
      case DDERR_INVALIDRECT:
         return
         "Rectangle provided was invalid.";
      case DDERR_LOCKEDSURFACES:
         return
         "Operation could not be carried out because one or more surfaces are locked";
      case DDERR_NO3D:
         return
         "There is no 3D present.";
      case DDERR_NOALPHAHW:
         return
         "Operation could not be carried out because there is no alpha accleration "
         "hardware present or available.";
      case DDERR_NOSTEREOHARDWARE:
         return
         "Operation could not be carried out because there is no stereo "
         "hardware present or available.";
      case DDERR_NOSURFACELEFT:
         return
         "Operation could not be carried out because there is no hardware"
         "present which supports stereo surfaces";
      case DDERR_NOCLIPLIST:
         return
         "no clip list available";
      case DDERR_NOCOLORCONVHW:
         return
         "Operation could not be carried out because there is no color conversion "
         "hardware present or available.";
      case DDERR_NOCOOPERATIVELEVELSET:
         return
         "Create function called without DirectDraw object method SetCooperativeLevel "
         "being called.";
      case DDERR_NOCOLORKEY:
         return
         "Surface doesn't currently have a color key";
      case DDERR_NOCOLORKEYHW:
         return
         "Operation could not be carried out because there is no hardware support "
         "of the dest color key.";
      case DDERR_NODIRECTDRAWSUPPORT:
         return
         "No DirectDraw support possible with current display driver";
      case DDERR_NOEXCLUSIVEMODE:
         return
         "Operation requires the application to have exclusive mode but the "
         "application does not have exclusive mode.";
      case DDERR_NOFLIPHW:
         return
         "Flipping visible surfaces is not supported.";
      case DDERR_NOGDI:
         return
         "There is no GDI present.";
      case DDERR_NOMIRRORHW:
         return
         "Operation could not be carried out because there is no hardware present "
         "or available.";
      case DDERR_NOTFOUND:
         return
         "Requested item was not found";
      case DDERR_NOOVERLAYHW:
         return
         "Operation could not be carried out because there is no overlay hardware "
         "present or available.";
      case DDERR_OVERLAPPINGRECTS:
         return
         "Operation could not be carried out because the source and destination "
         "rectangles are on the same surface and overlap each other.";
      case DDERR_NORASTEROPHW:
         return
         "Operation could not be carried out because there is no appropriate raster "
         "op hardware present or available.";
      case DDERR_NOROTATIONHW:
         return
         "Operation could not be carried out because there is no rotation hardware "
         "present or available.";
      case DDERR_NOSTRETCHHW:
         return
         "Operation could not be carried out because there is no hardware support "
         "for stretching";
      case DDERR_NOT4BITCOLOR:
         return
         "DirectDrawSurface is not in 4 bit color palette and the requested operation "
         "requires 4 bit color palette.";
      case DDERR_NOT4BITCOLORINDEX:
         return
         "DirectDrawSurface is not in 4 bit color index palette and the requested "
         "operation requires 4 bit color index palette.";
      case DDERR_NOT8BITCOLOR:
         return
         "DirectDraw Surface is not in 8 bit color mode and the requested operation "
         "requires 8 bit color.";
      case DDERR_NOTEXTUREHW:
         return
         "Operation could not be carried out because there is no texture mapping "
         "hardware present or available.";
      case DDERR_NOVSYNCHW:
         return
         "Operation could not be carried out because there is no hardware support "
         "for vertical blank synchronized operations.";
      case DDERR_NOZBUFFERHW:
         return
         "Operation could not be carried out because there is no hardware support "
         "for zbuffer blting.";
      case DDERR_NOZOVERLAYHW:
        return
        "Overlay surfaces could not be z layered based on their BltOrder because "
        "the hardware does not support z layering of overlays.";
      case DDERR_OUTOFCAPS:
         return
         "The hardware needed for the requested operation has already been "
         "allocated.";
      case DDERR_OUTOFMEMORY:
         return
         "DirectDraw does not have enough memory to perform the operation.";
      case DDERR_OUTOFVIDEOMEMORY:
         return
         "DirectDraw does not have enough memory to perform the operation.";
      case DDERR_OVERLAYCANTCLIP:
         return
         "hardware does not support clipped overlays";
      case DDERR_OVERLAYCOLORKEYONLYONEACTIVE:
         return
         "Can only have ony color key active at one time for overlays";
      case DDERR_PALETTEBUSY:
         return
         "Access to this palette is being refused because the palette is already "
         "locked by another thread.";
      case DDERR_COLORKEYNOTSET:
         return
         "No src color key specified for this operation.";
      case DDERR_SURFACEALREADYATTACHED:
         return
         "This surface is already attached to the surface it is being attached to.";
      case DDERR_SURFACEALREADYDEPENDENT:
         return
         "This surface is already a dependency of the surface it is being made a "
         "dependency of.";
      case DDERR_SURFACEBUSY:
         return
         "Access to this surface is being refused because the surface is already "
         "locked by another thread.";
      case DDERR_CANTLOCKSURFACE:
         return
         "Access to this surface is being refused because no driver exists "
         "which can supply a pointer to the surface. "
         "This is most likely to happen when attempting to lock the primary "
         "surface when no DCI provider is present. "
         "Will also happen on attempts to lock an optimized surface.";
      case DDERR_SURFACEISOBSCURED:
         return
         "Access to Surface refused because Surface is obscured.";
      case DDERR_SURFACELOST:
         return
         "Access to this surface is being refused because the surface is gone. "
         "The DIRECTDRAWSURFACE object representing this surface should "
         "have Restore called on it.";
      case DDERR_SURFACENOTATTACHED:
         return
         "The requested surface is not attached.";
      case DDERR_TOOBIGHEIGHT:
         return
         "Height requested by DirectDraw is too large.";
      case DDERR_TOOBIGSIZE:
         return
         "Size requested by DirectDraw is too large --  The individual height and "
         "width are OK.";
      case DDERR_TOOBIGWIDTH:
         return
         "Width requested by DirectDraw is too large.";
      case DDERR_UNSUPPORTED:
         return
         "Action not supported.";
      case DDERR_UNSUPPORTEDFORMAT:
         return
         "Pixel format requested is unsupported by DirectDraw";
      case DDERR_UNSUPPORTEDMASK:
         return
         "Bitmask in the pixel format requested is unsupported by DirectDraw";
      case DDERR_INVALIDSTREAM:
         return
         "The specified stream contains invalid data";
      case DDERR_VERTICALBLANKINPROGRESS:
         return
         "vertical blank is in progress";
      case DDERR_WASSTILLDRAWING:
         return
         "Informs DirectDraw that the previous Blt which is transfering information "
         "to or from this Surface is incomplete.";
      case DDERR_DDSCAPSCOMPLEXREQUIRED:
         return
         "The specified surface type requires specification of the COMPLEX flag";
      case DDERR_XALIGN:
         return
         "Rectangle provided was not horizontally aligned on reqd. boundary";
      case DDERR_INVALIDDIRECTDRAWGUID:
         return
         "The GUID passed to DirectDrawCreate is not a valid DirectDraw driver "
         "identifier.";
      case DDERR_DIRECTDRAWALREADYCREATED:
         return
         "A DirectDraw object representing this driver has already been created "
         "for this process.";
      case DDERR_NODIRECTDRAWHW:
         return
         "A hardware only DirectDraw object creation was attempted but the driver "
         "did not support any hardware.";
      case DDERR_PRIMARYSURFACEALREADYEXISTS:
         return
         "this process already has created a primary surface";
      case DDERR_NOEMULATION:
         return
         "software emulation not available.";
      case DDERR_REGIONTOOSMALL:
         return
         "region passed to Clipper::GetClipList is too small.";
      case DDERR_CLIPPERISUSINGHWND:
         return
         "an attempt was made to set a clip list for a clipper objec that "
         "is already monitoring an hwnd.";
      case DDERR_NOCLIPPERATTACHED:
         return
         "No clipper object attached to surface object";
      case DDERR_NOHWND:
         return
         "Clipper notification requires an HWND or "
         "no HWND has previously been set as the CooperativeLevel HWND.";
      case DDERR_HWNDSUBCLASSED:
         return
         "HWND used by DirectDraw CooperativeLevel has been subclassed, "
         "this prevents DirectDraw from restoring state.";
      case DDERR_HWNDALREADYSET:
         return
         "The CooperativeLevel HWND has already been set. "
         "It can not be reset while the process has surfaces or palettes created.";
      case DDERR_NOPALETTEATTACHED:
         return
         "No palette object attached to this surface.";
      case DDERR_NOPALETTEHW:
         return
         "No hardware support for 16 or 256 color palettes.";
      case DDERR_BLTFASTCANTCLIP:
         return
         "If a clipper object is attached to the source surface passed into a "
         "BltFast call.";
      case DDERR_NOBLTHW:
         return
         "No blter.";
      case DDERR_NODDROPSHW:
         return
         "No DirectDraw ROP hardware.";
      case DDERR_OVERLAYNOTVISIBLE:
         return
         "returned when GetOverlayPosition is called on a hidden overlay";
      case DDERR_NOOVERLAYDEST:
         return
         "returned when GetOverlayPosition is called on a overlay that UpdateOverlay "
         "has never been called on to establish a destionation.";
      case DDERR_INVALIDPOSITION:
         return
         "returned when the position of the overlay on the destionation is no longer "
         "legal for that destionation.";
      case DDERR_NOTAOVERLAYSURFACE:
         return
         "returned when an overlay member is called for a non-overlay surface";
      case DDERR_EXCLUSIVEMODEALREADYSET:
         return
         "An attempt was made to set the cooperative level when it was already "
         "set to exclusive.";
      case DDERR_NOTFLIPPABLE:
         return
         "An attempt has been made to flip a surface that is not flippable.";
      case DDERR_CANTDUPLICATE:
         return
         "Can't duplicate primary & 3D surfaces, or surfaces that are implicitly "
         "created.";
      case DDERR_NOTLOCKED:
         return
         "Surface was not locked.  An attempt to unlock a surface that was not "
         "locked at all, or by this process, has been attempted.";
      case DDERR_CANTCREATEDC:
         return
         "Windows can not create any more DCs, or a DC was requested for a paltte-indexed "
         "surface when the surface had no palette AND the display mode was not palette-indexed "
         "(in this case DirectDraw cannot select a proper palette into the DC)";
      case DDERR_NODC:
         return
         "No DC was ever created for this surface.";
      case DDERR_WRONGMODE:
         return
         "This surface can not be restored because it was created in a different "
         "mode.";
      case DDERR_IMPLICITLYCREATED:
         return
         "This surface can not be restored because it is an implicitly created "
         "surface.";
      case DDERR_NOTPALETTIZED:
         return
         "The surface being used is not a palette-based surface";
      case DDERR_UNSUPPORTEDMODE:
         return
         "The display is currently in an unsupported mode";
      case DDERR_NOMIPMAPHW:
         return
         "Operation could not be carried out because there is no mip-map "
         "texture mapping hardware present or available.";
      case DDERR_INVALIDSURFACETYPE:
         return
         "The requested action could not be performed because the surface was of "
         "the wrong type.";
      case DDERR_NOOPTIMIZEHW:
         return
         "Device does not support optimized surfaces, therefore no video memory optimized surfaces";
      case DDERR_NOTLOADED:
         return
         "Surface is an optimized surface, but has not yet been allocated any memory";
      case DDERR_NOFOCUSWINDOW:
         return
         "Attempt was made to create or set a device window without first setting "
         "the focus window";
      case DDERR_NOTONMIPMAPSUBLEVEL:
         return
         "Attempt was made to set a palette on a mipmap sublevel";
      case DDERR_DCALREADYCREATED:
         return
         "A DC has already been returned for this surface. Only one DC can be "
         "retrieved per surface.";
      case DDERR_NONONLOCALVIDMEM:
         return
         "An attempt was made to allocate non-local video memory from a device "
         "that does not support non-local video memory.";
      case DDERR_CANTPAGELOCK:
         return
         "The attempt to page lock a surface failed.";
      case DDERR_CANTPAGEUNLOCK:
         return
         "The attempt to page unlock a surface failed.";
      case DDERR_NOTPAGELOCKED:
         return
         "An attempt was made to page unlock a surface with no outstanding page locks.";
      case DDERR_MOREDATA:
         return
         "There is more data available than the specified buffer size could hold";
      case DDERR_EXPIRED:
         return
         "The data has expired and is therefore no longer valid.";
      case DDERR_TESTFINISHED:
         return
         "The mode test has finished executing.";
      case DDERR_NEWMODE:
         return
         "The mode test has switched to a new mode.";
      case DDERR_D3DNOTINITIALIZED:
         return
         "D3D has not yet been initialized.";
      case DDERR_VIDEONOTACTIVE:
         return
         "The video port is not active";
      case DDERR_NOMONITORINFORMATION:
         return
         "The monitor does not have EDID data.";
      case DDERR_NODRIVERSUPPORT:
         return
         "The driver does not enumerate display mode refresh rates.";
      case DDERR_DEVICEDOESNTOWNSURFACE:
         return
         "Surfaces created by one direct draw device cannot be used directly by "
         "another direct draw device.";
      case DDERR_NOTINITIALIZED:
         return
         "An attempt was made to invoke an interface member of a DirectDraw object "
         "created by CoCreateInstance() before it was initialized.";
      default:
         return
         "Unknown error.";
   }
}
//------------------------------------------------------------------------------
CUtilites::~CUtilites()
{
   if (pPatchDAT != NULL)
      delete pPatchDAT;
   if (pMasterDAT != NULL)
      delete pMasterDAT;
   if (pCritterDAT != NULL)
      delete pCritterDAT;
   if (pSuffix != NULL)
      delete pSuffix;
}
//---------------------------------------------------------------------------

