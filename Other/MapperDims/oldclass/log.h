//---------------------------------------------------------------------------

#ifndef logH
#define logH

#include "utilites.h"

class CLog
{
public:
   String logfile;

   CLog(String filename);
   void LogX(String LogLine);
   virtual ~CLog();

protected:
   CLog *pLog;
   String MapperPath;
};
//---------------------------------------------------------------------------
#endif
 