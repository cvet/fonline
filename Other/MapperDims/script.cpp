//////////////////////////////////////////////////////////////////////
// CScriptBuf Class
//////////////////////////////////////////////////////////////////////

#include "script.h"
#include "mdi.h"
#include "utilites.h"
//---------------------------------------------------------------------------
CScriptBuf::CScriptBuf()
{
   lError = true;
//   dwScriptSize = dwSize;
//   if ((pScriptBuf = (BYTE *)malloc(dwScriptSize)) == NULL) return;
//   memcpy(pScriptBuf, pTemp, dwScriptSize);

   pDescBuf = NULL;
   pDescBuf = new DWORD* [5];
   pDescBuf[0] = NULL;
   pDescBuf[1] = NULL;
   pDescBuf[2] = NULL;
   pDescBuf[3] = NULL;
   pDescBuf[4] = NULL;
   nMaxDescID[0] = -1;
   nMaxDescID[1] = -1;
   nMaxDescID[2] = -1;
   nMaxDescID[3] = -1;
   nMaxDescID[4] = -1;
   nDescCount[0] = 0;
   nDescCount[1] = 0;
   nDescCount[2] = 0;
   nDescCount[3] = 0;
   nDescCount[4] = 0;
   lError = false;
}
//---------------------------------------------------------------------------
void CScriptBuf::CreateDescBlock(int ScrDescType, int count)
{
   int BlockLenDW;
   BlockLenDW = count * (ScrDescType == 1 ? 18 : ScrDescType == 2 ? 17 : 16);
   pDescBuf[ScrDescType] = new DWORD [BlockLenDW];
   ZeroMemory(pDescBuf[ScrDescType], BlockLenDW * 4);
}
//---------------------------------------------------------------------------
void CScriptBuf::CopyDesc(DWORD *pDesc, int ScrDescType)
{
   DWORD *pDW;
   int DescLen = (ScrDescType == 1 ? 18 : ScrDescType == 2 ? 17 : 16);
   int Offset = nDescCount[ScrDescType] * DescLen;
   pDW = pDescBuf[ScrDescType];
   pDW += Offset;
   CopyMemory((void*)pDW, (void*)pDesc, DescLen * 4);
   int dwDescID = GetDescID(pDW) & 0x0000FFFF;
   if (dwDescID > nMaxDescID[ScrDescType])
      nMaxDescID[ScrDescType] = dwDescID;
   nDescCount[ScrDescType]++;
}
//---------------------------------------------------------------------------
/* ‘-ци€ создаЄт описатель заданного типа и возвращает полный номер описател€ */
DWORD CScriptBuf::AppendDesc(int ScrDescType)
{
   DWORD *pTemp;
   int DescLen = (ScrDescType == 1 ? 18 : ScrDescType == 2 ? 17 : 16);
   int OldLenght = nDescCount[ScrDescType] * DescLen;
   int NewLenght = (nDescCount[ScrDescType] + 1) * DescLen;
   pTemp = new DWORD [NewLenght];
   ZeroMemory(pTemp, NewLenght * 4);
   if (pDescBuf[ScrDescType])
   {
      CopyMemory((void*)pTemp, (void*)pDescBuf[ScrDescType], OldLenght * 4);
      delete pDescBuf[ScrDescType];
   }
   int dwScriptDescNum = ++nMaxDescID[ScrDescType];
   DWORD dwDescID = dwScriptDescNum | (ScrDescType << 24);
   SetDescID(pTemp + OldLenght, dwDescID);
   DWORD dwScriptID = 0xFFFFFFFF;
   SetScriptID(pTemp + OldLenght, dwScriptID);
   pDescBuf[ScrDescType] = pTemp;
   nDescCount[ScrDescType]++;
   return dwDescID;
}
//--------------------------------------------------------------------------
void CScriptBuf::ChangeScriptID(DWORD dwScriptDesc, DWORD dwScriptID)
{
   DWORD *pDW;
   int ScrDescType = (dwScriptDesc & 0xFF000000) >> 24;
   int DescLen = ScrDescType == 1 ? 18 : ScrDescType == 2 ? 17 : 16;
   pDW = pDescBuf[ScrDescType];
   for (int i = 0; i < nDescCount[ScrDescType]; i++)
   {
      if (pUtil->GetDW(pDW) == dwScriptDesc)
      {
         SetScriptID(pDW, dwScriptID);
      }
      pDW += DescLen;
   }
}
//---------------------------------------------------------------------------
DWORD CScriptBuf::GetDescID(DWORD *pDW)
{
   return pUtil->GetDW(pDW);
}
//---------------------------------------------------------------------------
void CScriptBuf::SetDescID(DWORD *pDW, DWORD dwDescID)
{
   *(DWORD *)pDW = pUtil->GetDW(&dwDescID);
}
//---------------------------------------------------------------------------
DWORD CScriptBuf::GetScriptID(DWORD *pDW)
{
   return pUtil->GetDW(pDW + 0x03);
}
//---------------------------------------------------------------------------
void CScriptBuf::SetScriptID(DWORD *pDW, DWORD dwScriptID)
{
   *(DWORD *)(pDW + 0x03) = pUtil->GetDW(&dwScriptID);
}
//---------------------------------------------------------------------------
void CScriptBuf::SaveToFile(HANDLE h_map)
{
   BYTE *pBuf;
   DWORD *pDW, *pWrite, *pTemp;
   int DescLen;
   int ScriptCount[5];
   int TypeLenght;
   int BufLenght = 0;
   for (int nType = 0; nType < 5; nType++)
   {
      DescLen = nType == 1 ? 18 : nType == 2 ? 17 : 16;
      if (pDescBuf[nType])
      {
         ScriptCount[nType] = 0;
         TypeLenght = 0;
         pDW = pDescBuf[nType];
         for (int i = 0; i < nDescCount[nType]; i++)
         {
            if (GetScriptID(pDW) != 0xFFFFFFFF)
            {
               ScriptCount[nType]++;
            }
            pDW += DescLen;
         }
      }
      if (pDescBuf[nType] && ScriptCount[nType])
      {
         TypeLenght = (ScriptCount[nType] * DescLen) +           // desc in use lenght
                      ((16 - (ScriptCount[nType] % 16)) *
                        ((ScriptCount[nType] % 16) != 0)) * 16 + // unused desc lenght
                      ((ScriptCount[nType] - 1) / 16) * 2 +      // block counter
                      3;                                         // counter & check counter & 0
         BufLenght += TypeLenght;
      }
      else
      {
         BufLenght ++; //countrer
         ScriptCount[nType] = 0;
      }
   }

   pBuf = new BYTE[BufLenght * 4];
   ZeroMemory(pBuf, BufLenght * 4);
   pWrite = (DWORD *)pBuf;
   pTemp = (DWORD *)pBuf;

   for (int nType = 0; nType < 5; nType++)
   {
      (DWORD)*pWrite = pUtil->GetDW((DWORD *)&ScriptCount[nType]);
      pWrite++;
      int DescWrited = 0;
      if (ScriptCount[nType])
      {
         DescLen = nType == 1 ? 18 : nType == 2 ? 17 : 16;
         pDW = pDescBuf[nType];
         for (int i = 0; i < nDescCount[nType]; i++)
         {
            if (GetScriptID(pDW) != 0xFFFFFFFF)
            {
               CopyMemory((void*)pWrite, (void*)pDW, DescLen * 4);
               pWrite += DescLen;
               DescWrited++;
               if  (DescWrited == 16)
               {
                  (DWORD)*pWrite = pUtil->GetDW((DWORD *)&DescWrited);
                  pWrite++;
                  DescWrited = 0;
                  (DWORD)*pWrite = 0x00000000;
                  pWrite++;
               }
            }
            pDW += DescLen;
         }
         if (DescWrited)
         {
            FillMemory((void*)pWrite, (16 - DescWrited) * 64, 0xCC);
            pWrite += (16 - DescWrited) * 16;
         }
         (DWORD)*pWrite = pUtil->GetDW((DWORD *)&DescWrited);
         pWrite++;
         (DWORD)*pWrite = 0x00000000;
         pWrite++;
      }
   }

   DWORD i;
//   WriteFile(h_map, pScriptBuf, dwScriptSize, &i, NULL);
   WriteFile(h_map, pBuf, BufLenght * 4, &i, NULL);
   delete pBuf;
   return;
}
//---------------------------------------------------------------------------
CScriptBuf::~CScriptBuf()
{
//   if (pScriptBuf != NULL) free(pScriptBuf);
   for (int nType = 0; nType < 5; nType++)
   {
      if (pDescBuf && pDescBuf[nType]) delete[] pDescBuf[nType];
   }
   if (pDescBuf) delete[] pDescBuf;
}
//---------------------------------------------------------------------------



