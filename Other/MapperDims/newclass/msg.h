//---------------------------------------------------------------------------

#ifndef msgH
#define msgH

#include "utilites.h"

class CMsg
{
public:
   bool lError;

   CMsg(void);
   virtual ~CMsg();

protected:
   CMsg *pMsg;
};
//---------------------------------------------------------------------------
#endif
 