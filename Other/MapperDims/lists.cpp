//////////////////////////////////////////////////////////////////////
// CListFiles Class
//////////////////////////////////////////////////////////////////////

#include "mdi.h"
#include "map.h"
#include "lists.h"
#include "tileset.h"
#include "utilites.h"
#include "macros.h"

//---------------------------------------------------------------------------
CListFiles::CListFiles(void)
{
   String IDname[8] = {"items", "critters", "scenery", "walls",
                       "tiles", "misc", "inven", "intrface"};
   lError       = true;
   nFRM_count   = 0;
   nPRO_count   = 0;
   frmMDI->iPos = 0;

   for (int i = 0; i < 8; i++) pFRMlst[i] = NULL;
   for (int i = 0; i < 6; i++) pPROlst[i] = NULL;

   DWORD dwFileSize, dwFileSize2;
   for (int i = 0; i < 8; i++)
   {
      pFRMlst[i] = new TStringList();
      // Загружаем список файлов в соотв. директории
      pUtil->GetListFromFile("art\\" + IDname[i]+ "\\" + IDname[i] + ".lst", pFRMlst[i]);
      nFRM_count += pFRMlst[i]->Count;
      frmMDI->iPos++;
      if (i < 6)
      {
         pPROlst[i] = new TStringList();
         // Загружаем список прототипов в соотв. директории
         pUtil->GetListFromFile("proto\\" + IDname[i]+ "\\" + IDname[i] + ".lst", pPROlst[i]);
         nPRO_count += pPROlst[i]->Count;
         frmMDI->iPos++;
      }
   }
   pScript = new TStringList();
   // Загружаем список скриптов
   pUtil->GetListFromFile("scripts\\scripts.lst", pScript);


   if(pFRMlst[scenery_ID])
   {
      if(pFRMlst[scenery_ID]->Count >= 187)
      {
         if(pFRMlst[scenery_ID]->Strings[187].Length() > 42)
         {
             pFRMlst[scenery_ID]->Strings[187] =
                    pFRMlst[scenery_ID]->Strings[187].SetLength(13);
         }
      }
   }
   lError = false;
}
//---------------------------------------------------------------------------
CListFiles::~CListFiles()
{
   for (int i = 0; i < 8; i++)
      if (pFRMlst[i]) delete pFRMlst[i];
   for (int i = 0; i < 6; i++)
      if (pPROlst[i]) delete pPROlst[i];
   if (pScript) delete pScript;
}
//---------------------------------------------------------------------------

