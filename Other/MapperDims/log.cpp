//////////////////////////////////////////////////////////////////////
// CLog Class
//////////////////////////////////////////////////////////////////////

#include "log.h"
#include "utilites.h"
//---------------------------------------------------------------------------
CLog::CLog(String filename)
{
   logfile = filename;
   FILE *stream;
   stream = fopen(logfile.c_str(), "w");
   fclose(stream);
}
//---------------------------------------------------------------------------
void CLog::LogX(String LogLine)
{
   FILE *stream;
   stream = fopen(logfile.c_str(), "a+");
   LogLine += "\n";
   fprintf(stream, LogLine.c_str());
   fclose(stream);
}
//---------------------------------------------------------------------------
CLog::~CLog()
{
}
//---------------------------------------------------------------------------

