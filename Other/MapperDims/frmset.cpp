//////////////////////////////////////////////////////////////////////
// CFrmSet Class
//////////////////////////////////////////////////////////////////////

#include "frmset.h"
#include "mdi.h"
#include "frame.h"
#include "main.h"
#include "utilites.h"
#include "log.h"
#include "lists.h"
#include "macros.h"
//---------------------------------------------------------------------------
CFrmSet::CFrmSet(void)
{
   lError   = true;

   for (int i = 0; i < 8; i++)
   {
      if (i == critter_ID)
         // На каждого critter а 216 анимаций???
         pFRM[i] = new CFrame[(pLstFiles->pFRMlst[i]->Count + 1) * 216];
      else
         pFRM[i] = new CFrame[pLstFiles->pFRMlst[i]->Count + 1];
   }
   ClearLocals();

   lError = false;
}
//---------------------------------------------------------------------------
void CFrmSet::ClearLocals(void)
{
   int i, j;
   for (i = 0; i < 7; i++)
   {
      for (j = 0; j < pLstFiles->pFRMlst[i]->Count + 1; j++)
      {
         pFRM[i][j].bLocal = false;
      }
      nFrmCount[i] = 0;
   }
}
//---------------------------------------------------------------------------
void CFrmSet::LoadLocalFRMs(void)
{
   //-------------------LOAD INTERFACE--------------------
   LoadFRM(intrface_ID, TG_blockID, true);
   LoadFRM(intrface_ID, EG_blockID, true);
   LoadFRM(intrface_ID, SAI_blockID, true);
   LoadFRM(intrface_ID, wall_blockID, true);
   LoadFRM(intrface_ID, obj_blockID, true);
   LoadFRM(intrface_ID, light_blockID, true);
   LoadFRM(intrface_ID, scroll_blockID, true);
   LoadFRM(intrface_ID, obj_self_blockID, true); 
}
//---------------------------------------------------------------------------
void CFrmSet::LoadFRM(BYTE nFrmType, DWORD nFrmID, bool bLocal)
{
    String sFile;

    DWORD FID = nFrmID;

    switch (nFrmType)
    {
        // Тайлы
        case tile_ID:
             nFrmID &= 0x0FFF;
             sFile = pUtil->GetFRMFileName(tile_ID, pLstFiles->pFRMlst[tile_ID]->Strings[nFrmID]);
             break;
        case inven_ID:
        case item_ID:
        case scenery_ID:
        case wall_ID:
        case misc_ID:
             nFrmID = (DWORD)(nFrmID & 0xFFFF);
             sFile = pUtil->GetFRMFileName(nFrmType, pLstFiles->pFRMlst[nFrmType]->Strings[nFrmID]);
             break;

        case critter_ID:
             nFrmID &= 0xFF;
             sFile = pUtil->GetFRMFileName(nFrmType, pLstFiles->pFRMlst[nFrmType]->Strings[nFrmID]);
             GetCritterFName(&sFile, FID, (WORD*)&nFrmID);
			 break;

		case intrface_ID:
             switch (nFrmID)
			 {
				case TG_blockID:        sFile = "data\\trigger.frm"; break;
				case EG_blockID:        sFile = "data\\exit.frm"; break;
				case SAI_blockID:       sFile = "data\\sai.frm";  break;
				case wall_blockID:      sFile = "data\\wall.frm"; break;
				case obj_blockID:       sFile = "data\\obj.frm";  break;
				case light_blockID:     sFile = "data\\light.frm";break;
				case scroll_blockID:    sFile = "data\\scr.frm";  break;
				case obj_self_blockID:  sFile = "data\\self.frm"; break;
             }
//        case invent_ID:
//        case heads_ID:
//        case backgrnd_ID:
//        case skilldex_ID:
             break;
    }

   //Получаем указатель на FRM, куда будем писать данные
   CFrame *l_pFRM = &pFRM[nFrmType][nFrmID];
   //Увеличиваем счётчик FRM если данный FRM не был, но будет локальный
   nFrmCount[nFrmType] += !l_pFRM->bLocal && bLocal ? 1 : 0;
   //Проверяем был ли FRM локальным, при необходимости делаем его локальным
   l_pFRM->bLocal = l_pFRM->bLocal ? l_pFRM->bLocal : bLocal;
   //Проверяем FRM на готовность, если уже существуем то выход из ф-ции
   if (l_pFRM->GetBMP())
        return;

   ULONG i; // используется для файловых операций
   DWORD a_width, bmpsize, frmsize, dwPtr, dwFramesBufSize, dwPixDataSize;
   WORD nFrames, width, height;

   // Пробуем открыть FRM файл для чтения
   HANDLE h_frm = pUtil->OpenFileX(sFile);
   // Если получаем ошибку, то выход из ф-ции
   if (h_frm == INVALID_HANDLE_VALUE)
   {
      pLog->LogX("Cannot open file \"" + sFile + "\"");
      return;
   }
   //Ставим файловый указатель на смещение 0x08 от начала файла
   pUtil->SetFilePointerX(h_frm, 0x08, FILE_BEGIN);
   //Считываем 2 байта (кол-во кадров для одного направления(может быть = 1))
   pUtil->ReadFileX(h_frm, &nFrames, 2, &i);
   l_pFRM->nFrames = pUtil->GetW(&nFrames);         // Кол-во кадров на направление
   //Считываем 12 байт (WORD * 6 : смещение по Х относительно преведущего кадра)
   pUtil->ReadFileX(h_frm, &doffX, 12, &i);
   //Считываем 12 байт (WORD * 6 : смещение по У относительно преведущего кадра)
   pUtil->ReadFileX(h_frm, &doffY, 12, &i);
   //Считываем 24 байта (DWORD * 6 : смещение в файле от начала фреймов)
   pUtil->ReadFileX(h_frm, &dwDirectionOffset, 24, &i);
   //Считываем 4 байта (DWORD размер области фреймов)
   pUtil->ReadFileX(h_frm, &dwFramesBufSize, 4, &i);
   pUtil->ReverseDW(&dwFramesBufSize);
   //Готовим буфер и считываем туда область фреймов, при ошибке выход из ф-ции
   BYTE *framesdata;
   if ((framesdata = (BYTE *)malloc(dwFramesBufSize)) == NULL) return;
   pUtil->ReadFileX(h_frm, framesdata, dwFramesBufSize, &i);
   //Закрываем файл за ненадобностью
   pUtil->CloseHandleX(h_frm);
   //Смотрим, сколько реальных направлений имеет FRM файл
   l_pFRM->nDirTotal = 1 + (pUtil->GetDW(&dwDirectionOffset[1]) != 0) +
                           (pUtil->GetDW(&dwDirectionOffset[2]) != 0) +
                           (pUtil->GetDW(&dwDirectionOffset[3]) != 0) +
                           (pUtil->GetDW(&dwDirectionOffset[4]) != 0) +
                           (pUtil->GetDW(&dwDirectionOffset[5]) != 0);
   //Подготовка массивов для кадров
   l_pFRM->PrepareFrames();
   WORD HeightMAX = 0;
   WORD HeightSpr = 0;
   WORD WidthSpr = 0;
   WORD WidthSprMAX = 0;
   //Проходим цикл направлений
   for (int nDir = 0; nDir < l_pFRM->nDirTotal; nDir++)
   {
      //Получаем смещение для данного направления
      dwPtr = pUtil->GetDW(&dwDirectionOffset[nDir]);
      //Получаем смещение кадров по Х и У для данного направления
      l_pFRM->doffX[nDir] = pUtil->GetW((WORD *)&doffX[nDir]);
      l_pFRM->doffY[nDir] = pUtil->GetW((WORD *)&doffY[nDir]);

      //Проходим цикл кадров
      for (int nFrame = 0; nFrame < l_pFRM->nFrames; nFrame++)
      {
         //Х начальная координата кадра в спрайте
         l_pFRM->sprX[nDir][nFrame] = WidthSpr;
         //У начальная координата кадра в спрайте
         l_pFRM->sprY[nDir][nFrame] = HeightSpr;
         //Получаем длинну кадра
         width = pUtil->GetW((WORD *)(framesdata + dwPtr));
         l_pFRM->width[nDir][nFrame] = width;
         //Складываем длинны кадров
         WidthSpr += width;
         //Получаем высоту кадра
         height = pUtil->GetW((WORD *)(framesdata + dwPtr + 2));
         l_pFRM->height[nDir][nFrame] = height;
         //Высчитываем максимальную высоту кадров по одному направлению
         HeightMAX = max(HeightMAX, height);
         //Получаем размер области с пикселями для данного кадра
         dwPixDataSize = pUtil->GetDW((DWORD *)(framesdata + dwPtr + 4));
         //Получаем смещение по Х относительно преведущего кадра
         l_pFRM->foffX[nDir][nFrame] =
                                  pUtil->GetW((WORD *)(framesdata + dwPtr + 8));
         //Получаем смещение по У относительно преведущего кадра
         l_pFRM->foffY[nDir][nFrame] =
                                 pUtil->GetW((WORD *)(framesdata + dwPtr + 10));
         //Считываем кадр и пробразовываем в HBITMAP
         l_pFRM->LoadData(nDir, nFrame, framesdata + dwPtr + 12);
         //Увеличиваем смещение на длинну кадра для обработки следующего
         dwPtr += (12 + dwPixDataSize);
      }
      //Складываем высоты для каждого направления
      HeightSpr += HeightMAX;
      //Находим максимальную длинну суммы кадров направлений
      WidthSprMAX = max(WidthSprMAX, WidthSpr);
      //Обнуляем переменные
      WidthSpr = 0;
      HeightMAX = 0;
   }
   l_pFRM->BuildFrames(WidthSprMAX, HeightSpr);
   l_pFRM->FileName = ExtractFileName(sFile);
   if (l_pFRM->pBMP)
   {
      pLog->LogX("Load FRM file \"" + sFile +
                 "\" directions: " + String(l_pFRM->nDirTotal) +
                 " frames: " + String(l_pFRM->nFrames));
   }
   else
   {
      pLog->LogX("Failed to init FRM file \"" + sFile + "\"");
   }
   free (framesdata);
}
//---------------------------------------------------------------------------
void CFrmSet::FreeUpFRM(CFrame *l_pFRM)
{
    // Уничтожает не локальные спрайты
   if (l_pFRM->bLocal) return;
   l_pFRM->FreeUp();
}
//---------------------------------------------------------------------------
void CFrmSet::GetCritterFName(String* filename, DWORD frmPID, WORD *frmID)
{
   // Извлечение нового индекса из строки critters.lst
   int CommaPos = filename->Pos(",");
   String NewIndexAsStr = filename->SubString(filename->Pos(",") + 1,
                                             filename->Length() - CommaPos);
   CommaPos = NewIndexAsStr.Pos(",");

   if (CommaPos)
      NewIndexAsStr = NewIndexAsStr.SubString(1, CommaPos - 1);

   int NewIndex;
   try
   {
      NewIndex = NewIndexAsStr.ToInt();
   }

   catch(EConvertError&) {
      Application->MessageBox("Bad string in critters.lst\n"
                              "Object will be ignored",
                              "Mapper",
                              MB_ICONEXCLAMATION | MB_OK);
     *filename = "";
     return;
   }

   DWORD Index = frmPID & 0x00000FFF;           // Индекс в LST файле
   DWORD ID1   = (frmPID & 0x0000F000) >> 12;   // ID1
   DWORD ID2   = (frmPID & 0x00FF0000) >> 16;   // ID2
   DWORD ID3   = (frmPID & 0x70000000) >> 28;   // ID3

   // Изменение индекса
   if (ID2 == 0x1B || ID2 == 0x1D ||
       ID2 == 0x1E || ID2 == 0x37 ||
       ID2 == 0x39 || ID2 == 0x3A ||
       ID2 == 0x21 || ID2 == 0x40)
   {
       Index = NewIndex;
         *filename = pUtil->GetFRMFileName(critter_ID,
                                pLstFiles->pFRMlst[critter_ID]->Strings[Index]);
//       *filename = pLstFiles->pFRMlst[critter_ID]->Strings[Index];
   }

   filename->SetLength(filename->Pos(",") - 1);

   // Получение суффиксов
   char Suffix1;
   char Suffix2;
   if (!getSuffixes(ID1, ID2, Suffix1, Suffix2))
   {
      Application->MessageBox("Bad FRM PID\n"
                              "Object will be ignored",
                              "Mapper",
                              MB_ICONEXCLAMATION | MB_OK);
     *filename = "";
     return;
   }

   // Получение полного имени файла
   String FileExt = "";
   FileExt += Suffix1;
   FileExt += Suffix2;
   FileExt += ".fr";
   FileExt += (ID3) ? char('0' + ID3 - 1) : ('m');
   DWORD SuffIndex = pUtil->GetIndexBySuffix(FileExt);
   if (SuffIndex)
   {
      *frmID *= SuffIndex;
   }
   else
   {
      Application->MessageBox("Bad suffix\n"
                              "Object will be ignored",
                              "Mapper",
                              MB_ICONEXCLAMATION | MB_OK);
   }
   *filename += FileExt;
}
//------------------------------------------------------------------------------
bool CFrmSet::getSuffixes(DWORD ID1, DWORD ID2, char& Suffix1, char& Suffix2)
{
  if (ID1 >= 0x0B)
     return false;

  if (ID2 >= 0x26 && ID2 <= 0x2F) {
     Suffix2 = char(ID2) + 0x3D;

     if (ID1 == 0)
        return false;

     Suffix1 = char(ID1) + 'c';

     return true;
  }

  if (ID2 == 0x24) {
     Suffix2 = 'h';
     Suffix1 = 'c';

     return true;
  }

  if (ID2 == 0x25) {
     Suffix2 = 'j';
     Suffix1 = 'c';

     return true;
  }

  if (ID2 == 0x40) {
     Suffix2 = 'a';
     Suffix1 = 'n';

     return true;
  }

  if (ID2 >= 0x30) {
     Suffix2 = char(ID2) + 0x31;
     Suffix1 = 'r';

     return true;
  }

  if (ID2 >= 0x14) {
     Suffix2 = char(ID2) + 0x4D;
     Suffix1 = 'b';

     return true;
  }

  if (ID2 == 0x12) {
     if (ID1 == 0x01) {
        Suffix1 = 'd';
        Suffix2 = 'm';

        return true;
     }

     if (ID1 == 0x04) {
        Suffix1 = 'g';
        Suffix2 = 'm';

        return true;
     }

     Suffix1 = 'a';
     Suffix2 = 's';

     return true;
  }

  if (ID2 == 0x0D) {
     if (ID1 > 0x00) {
        Suffix1 = char(ID1) + 'c';
        Suffix2 = 'e';

        return true;
     }

     Suffix1 = 'a';
     Suffix2 = 'n';

     return true;
  }

  Suffix2 = char(ID2) + 'a';

  if (ID2 <= 1 && ID1 > 0){
     Suffix1 = char(ID1) + 'c';

     return true;
  }

  Suffix1 = 'a';

  return true;
}
//---------------------------------------------------------------------------
CFrmSet::~CFrmSet()
{
    for (int i = 0; i < 8; i++)
        _DELETE(pFRM[i]);
}
//---------------------------------------------------------------------------
