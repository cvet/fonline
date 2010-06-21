//////////////////////////////////////////////////////////////////////
// CProSet Class
//////////////////////////////////////////////////////////////////////

#include "proset.h"
#include "log.h"
#include "pro.h"
#include "objset.h"
#include "main.h"
#include "mdi.h"
#include "utilites.h"
#include "macros.h"

//---------------------------------------------------------------------------
CProSet::CProSet(void)
{
    lError   = true;
    InitPROArrays();
    ClearLocals();
    lError   = false;
}
//---------------------------------------------------------------------------
void CProSet::InitPROArrays(void)
{
    // Выделяем память под прототипы
    for (int i = 0; i < 6; i++)
        pPRO[i] = new CPro[pLstFiles->pPROlst[i]->Count + 1];
}
//---------------------------------------------------------------------------
void CProSet::ClearLocals(void)
{
    // Абсолютно всем прототипам сбрасываем флаг bLocal
    int i, j;
    for (i = 0; i < 6; i++)
        for (j = 0; j < pLstFiles->pPROlst[i]->Count + 1; j++)
            pPRO[i][j].bLocal = false;
    // И все счетчики лок прототипов
    memset(nProCount, 0, sizeof(nProCount));
}
//---------------------------------------------------------------------------
void CProSet::LoadLocalPROs(CHeader * pHeader, CObjSet * pObjSet)
{
 /*   // Данная процедура просматривает весь список объектов
    // на карте и загружает ко всем объектам прототипы
    DWORD x, y, nObjNum, nLevel, nChildCount = 0;
    BYTE *pObj;
    BYTE nObjType;
    WORD nProID;
    nLevel = 0;
    do
    {
        pObj = pObjSet->GetFirstObj(&nObjNum, nLevel);
        while (pObj)
        {
//         if (nObjNum == 600) // bug in junkcsno.map [fallout1]
//            nObjNum = nObjNum;
//         pLog->LogX("Object Num:" + String(nObjNum) + " Child Count:" + String(nChildCount));
            pObjSet->GetObjType(&nObjType, &nProID);
            LoadPRO(nObjType, nProID, true);
            pObj = pObjSet->GetNextObj(&nObjNum, &nChildCount, nLevel);
            frmMDI->iPos++;
            Application->ProcessMessages();
        }
    } while (++nLevel < pHeader->GetLevels());      */
}
//---------------------------------------------------------------------------
void CProSet::LoadPRO(BYTE nObjType, WORD nObjID, bool bLocal)
{
    CPro *l_pPRO = &pPRO[nObjType][nObjID];
    // Увеличивем счетчик лок прототипов
    nProCount[nObjType] += !l_pPRO->bLocal && bLocal ? 1 : 0;
    // Если прототип не лок, то присваиваем ему bLocal
    l_pPRO->bLocal = l_pPRO->bLocal ? l_pPRO->bLocal : bLocal;
    // Если данные прототипа уже загружены, то выход
    if (l_pPRO->data != NULL) return;

    String IDname[6] = {"items", "critters", "scenery", "walls", "tiles", "misc"};
    ULONG i;
    DWORD filesize, filesize2;

    pLog->LogX("Need PRO file for ObjType = " + String(nObjType) +
                  ", ObjID = \"" + String(nObjID));
    // Проверяем - есть ли прототип для данного объекта
    if (nObjID > pLstFiles->pPROlst[nObjType]->Count) return;
    String profile = "proto\\" + IDname[nObjType]+ "\\" +
                         pLstFiles->pPROlst[nObjType]->Strings[nObjID - 1];
    // Открываем файл
    HANDLE h_pro = pUtil->OpenFileX(profile);
    if (h_pro == INVALID_HANDLE_VALUE)
    {
       pLog->LogX("Cannot open file \"" + profile + "\"");
       return;
    }
    filesize = pUtil->GetFileSizeX(h_pro, &filesize2);
    // Выделяем память
    pPRO[nObjType][nObjID].LoadData(filesize);     //nObjID + 1
    // Считываем все что есть в файле
    pUtil->ReadFileX(h_pro, l_pPRO->data, filesize, &i);
    pLog->LogX("Loaded PRO file \"" + profile + "\" load " + String(i) + " of " +
               String(filesize) + " bytes.");
    pUtil->CloseHandleX(h_pro);
}
//---------------------------------------------------------------------------
DWORD CProSet::GetSubType(BYTE nObjType, WORD nObjID)
{
    // Если прототипа для данного объекта нет, то загрузим его
    // и получим поддит
    LoadPRO(nObjType, nObjID, true);
    return pPRO[nObjType][nObjID].GetSubType();
}
//---------------------------------------------------------------------------
CProSet::~CProSet()
{
    for (int i = 0; i < 6; i++)
        _DELETE(pPRO[i]);
}
//---------------------------------------------------------------------------

