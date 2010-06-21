//////////////////////////////////////////////////////////////////////
// CMsg Class
//////////////////////////////////////////////////////////////////////

#include "msg.h"
#include "utilites.h"
#include "mdi.h"
//---------------------------------------------------------------------------
CMsg::CMsg(void)
{
   lError = true;
   for (int i = 0; i < 6; i++)
      pMSGlst[i] = NULL;
   LoadDefaultMsg();
   lError = false;
}
//---------------------------------------------------------------------------
void CMsg::LoadDefaultMsg(void)
{
   String IDname[6] = {"pro_item", "pro_crit", "pro_scen", "pro_wall",
                       "pro_tile", "pro_misc"};
   int i;
   for (i = 0; i < 6; i++)
   {
      pMSGlst[i] = new TStringList();
      pUtil->GetListFromFile("text\\english\\game\\" + IDname[i] + ".msg",
                             pMSGlst[i]);
   }
}
//---------------------------------------------------------------------------
String CMsg::GetMSG(BYTE nType, DWORD nID)
{
   if (pMSGlst[nType] == NULL) return "";
   for (int i = 0; i < pMSGlst[nType]->Count; i++)
   {
      String sValue = pUtil->GetMsgValue(pMSGlst[nType]->Strings[i], 1);
      if (String(nID) == sValue)
      {
         String strValue = pUtil->GetMsgValue(pMSGlst[nType]->Strings[i], 3);
         pUtil->RetranslateString(strValue.c_str());
         return strValue;
      }
   }
   return "";
}
//---------------------------------------------------------------------------
CMsg::~CMsg()
{
   for (int i = 0; i < 6; i++)
      if (pMSGlst[i]) delete pMSGlst[i];
}
//---------------------------------------------------------------------------

