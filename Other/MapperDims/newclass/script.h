//---------------------------------------------------------------------------

#ifndef scriptH
#define scriptH

#include "utilites.h"

class CScript
{
public:
   bool lError;

   CScript(void);
   virtual ~CScript();

protected:
   CScript *pScript;
};
//---------------------------------------------------------------------------
#endif
 