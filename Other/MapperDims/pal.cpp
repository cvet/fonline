//////////////////////////////////////////////////////////////////////
// CPal Class
//////////////////////////////////////////////////////////////////////

#include "pal.h"
#include "mdi.h"
#include "utilites.h"
#include "macros.h"

//---------------------------------------------------------------------------
CPal::CPal(void)
{
   lError = !(InitPal());
}
//---------------------------------------------------------------------------
bool CPal::InitPal(void)
{
   ULONG i;
   int cBlue, cGreen, cRed;
   memset(&bmi, 0, sizeof(bmi));
   HANDLE h_pal = pUtil->OpenFileX("color.pal");
   if (h_pal == INVALID_HANDLE_VALUE) return false;
   pUtil->ReadFileX(h_pal, &frm_pal, sizeof(frm_pal), &i);
   pUtil->CloseHandleX(h_pal);
   for (i = 1; i < 256; i++)  //Using 229 colors only !
   {
      cBlue = frm_pal[i].rgbtRed * BRIGHTNESS_FACTOR +
                                               BRIGHTNESS_FINETUNE + B_FINETUNE;
      bmi.bmiColors[i].rgbBlue = cBlue > 255 ? 255 : cBlue;
      cGreen = frm_pal[i].rgbtGreen * BRIGHTNESS_FACTOR +
                                               BRIGHTNESS_FINETUNE + G_FINETUNE;
      bmi.bmiColors[i].rgbGreen = cGreen > 255 ? 255 : cGreen;
      cRed = frm_pal[i].rgbtBlue * BRIGHTNESS_FACTOR +
                                               BRIGHTNESS_FINETUNE + R_FINETUNE;
      bmi.bmiColors[i].rgbRed = cRed > 255 ? 255 : cRed;
   }
   bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth = 0;
   bmi.bmiHeader.biHeight = 0;
   bmi.bmiHeader.biPlanes = 1;
   bmi.bmiHeader.biBitCount = 8;
   bmi.bmiHeader.biCompression = BI_RGB;

   return true;
}
//---------------------------------------------------------------------------
unsigned int CPal::GetEntry(BYTE Index)
{
    unsigned int temp =
            bmi.bmiColors[Index].rgbRed << 16 |
            bmi.bmiColors[Index].rgbGreen << 8 |
            bmi.bmiColors[Index].rgbBlue;
    return temp;
}
//---------------------------------------------------------------------------
CPal::~CPal()
{
}
//---------------------------------------------------------------------------

