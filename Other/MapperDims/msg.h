//---------------------------------------------------------------------------

#ifndef msgH
#define msgH

#include "utilites.h"

extern CUtilites *pUtil;

class CMsg
{
public:
   bool lError;

   TStringList *pMSGlst[6];

   String GetMSG(BYTE nType, DWORD nID);

   CMsg(void);
   virtual ~CMsg();

protected:

   void LoadDefaultMsg(void);
};
//---------------------------------------------------------------------------
#endif
 