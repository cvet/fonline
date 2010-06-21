//---------------------------------------------------------------------------
#ifndef proH
#define proH

#include <classes.hpp>
#include "utilites.h"

extern CUtilites *pUtil;

class CPro
{
public:
   bool     bLocal;
   BYTE     *data;
   String   FileName;

   void     LoadData(DWORD size);
   DWORD    GetMsgID(void);
   DWORD    GetFrmIDDW(void);
   WORD     GetFrmID(void);
   WORD     GetInvFrmID(void);
   DWORD    GetSubType(void);
   DWORD    GetFlags(void);

   CPro(void);
   virtual ~CPro();
protected:
};
//---------------------------------------------------------------------------
#endif
 