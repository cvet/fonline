//---------------------------------------------------------------------------

#ifndef logH
#define logH

#include "utilites.h"

class CLog
{
public:
   bool lError;

   CLog(void);
   virtual ~CLog();

protected:
   CLog *pLog;
};
//---------------------------------------------------------------------------
#endif
 