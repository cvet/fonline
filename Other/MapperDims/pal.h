//---------------------------------------------------------------------------

#ifndef palH
#define palH

#include "utilites.h"
#include <classes.hpp>
extern CUtilites *pUtil;
class CPal
{
public:
   bool lError;

   struct  // BITMAPINFO with 256 colors
   {
      BITMAPINFOHEADER bmiHeader;
      RGBQUAD      bmiColors[256];
   } bmi;
   RGBTRIPLE frm_pal[256];    // Fallout palette

   bool InitPal(void);
   unsigned int GetEntry(BYTE Index);

   CPal(void);
   virtual ~CPal();
};
//---------------------------------------------------------------------------
#endif
 