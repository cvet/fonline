//---------------------------------------------------------------------------
#ifndef frmsetH
#define frmsetH

#include <classes.hpp>
#include <graphics.hpp>
#include "frame.h"
#include "utilites.h"
#include "log.h"
#include "objset.h"
#include "lists.h"

extern CUtilites    *pUtil;
extern CLog         *pLog;
extern CListFiles   *pLstFiles;


class CHeader;
//---------------------------------------------------------------------------
class CFrmSet
{
public:
   bool lError;

   CFrame       *pFRM[8];               // Фреймы всех 8 типов
//   CFrame *pFRM_HEX;
   int          nFrmCount[8];           // Счетчики фреймов различных типов ( в том числе интерфейс)
   DWORD        dwDirectionOffset[6];   // Смещения начала области кадоров iтого направления
                                        // относительно начала области кадров
   signed short doffX[6];               // Требуемый сдвиг вдоль оси X кадров направления i
   signed short doffY[6];               // Требуемый сдвиг вдоль оси Y кадров направления i

   // Сброс всех локальных фреймов
   void         ClearLocals(void);
   // Загрузка всех локальных фреймов
   void         LoadLocalFRMs(void);
   // Загрузка фреймов
   void         LoadFRM(BYTE nFrmType, DWORD nFrmID, bool bLocal);
   // Освобождение памяти фрейма
   void         FreeUpFRM(CFrame *l_pFRM);
   void         GetCritterFName(String* filename, DWORD frmPID, WORD *frmID);
   bool         getSuffixes(DWORD ID1, DWORD ID2, char& Suffix1, char& Suffix2);

   CFrmSet(void);
   virtual ~CFrmSet();

protected:
};
//---------------------------------------------------------------------------
#endif
