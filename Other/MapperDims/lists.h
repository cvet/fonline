//---------------------------------------------------------------------------
#ifndef listsH
#define listsH

#include <classes.hpp>
#include "utilites.h"
extern CUtilites *pUtil;

class CListFiles
{
public:
   bool lError;

   int nFRM_count;              // Общее кол-во фреймов
   int nPRO_count;              // Общее кол-во прототипов

   TStringList *pFRMlst[8];
   TStringList *pPROlst[6];
   TStringList *pScript;        // Скрипты

   CListFiles(void);
   virtual ~CListFiles();

protected:
};
//---------------------------------------------------------------------------
#endif
