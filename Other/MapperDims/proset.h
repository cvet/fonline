//---------------------------------------------------------------------------
#ifndef prosetH
#define prosetH

#include <classes.hpp>
#include "utilites.h"
#include "pro.h"
#include "objset.h"
#include "map.h"
#include "log.h"

class CObjSet;
class CHeader;

extern CUtilites    *pUtil;
extern CLog         *pLog;


class CProSet
{
public:
   bool     lError;

   CPro     *pPRO[6];            // Списки прототипов
   int      nProCount[6];        // Кол-во лок прототипов для различных типов

   void     ClearLocals(void);
   void     LoadLocalPROs(CHeader * pHeader, CObjSet * pObjSet);
   void     LoadPRO(BYTE nObjType, WORD nObjID, bool bLocal);
   DWORD    GetSubType(BYTE nObjType, WORD nObjID);
   CProSet();
   virtual ~CProSet();

protected:
   void  InitPROArrays(void);
};
//---------------------------------------------------------------------------
#endif
