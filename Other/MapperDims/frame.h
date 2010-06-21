//---------------------------------------------------------------------------
#ifndef frameH
#define frameH

#include "pal.h"
#include <classes.hpp>
#include <Graphics.hpp>
#include <imglist.hpp>
#include <ddraw.h>

extern CPal *pPal;

class CFrame
{
public:
   bool         bLocal;
   String       FileName;
   WORD         nFrames, nDirTotal;
   DWORD        dwSpriteWidth, dwSpriteHeight;
   short **width, **height;            // Ширина и высота кадра
   short **sprX, **sprY;               // Позиция кадра в спрайте
   short **a_width;                    // Дополнения ширины кадров до кратности 4
   signed short *doffX, *doffY;        // Смещение кадров по Х и У для данного направления
   signed short **foffX, **foffY;      // Смещение кадра относительно предыдущего
   LPDIRECTDRAWSURFACE7 pBMP, pMask, pLtMask;
   int nBorderWidth, nBorderHeight;

   void                 PrepareFrames();
   void                 BuildFrames(WORD WidthSpr, WORD HeightSpr);
   void                 LoadData(int nDir, int nFrame, BYTE *tdata);
   BYTE                 GetCenterPix(int nDir, int nFrame);
   BYTE                 GetPixel(int X, int Y, int nDir, int nFrame);
   HBITMAP              GetHBITMAP(void);
   LPDIRECTDRAWSURFACE7 GetBMP(void);

   void                 InitBorder(WORD nDir, WORD nFrame, int b_width, int b_height);
   LPDIRECTDRAWSURFACE7 GetBorder(void);

   LPDIRECTDRAWSURFACE7 GetMask(WORD nDir, WORD nFrame);
   LPDIRECTDRAWSURFACE7 GetLtMask(WORD nDir, WORD nFrame);
   short                GetWi(WORD nDir, WORD nFrame);
   short                GetHe(WORD nDir, WORD nFrame);
   short                GetSprX(WORD nDir, WORD nFrame);
   short                GetSprY(WORD nDir, WORD nFrame);
   signed short         GetDoffX(WORD nDir);
   signed short         GetDoffY(WORD nDir);

   CFrame(void);
   virtual ~CFrame();

   void FreeUp();
protected:
   BYTE ***data;
};

Graphics::TBitmap* CreateSpriteMask (Graphics::TBitmap &bmp, int w, int h);
//---------------------------------------------------------------------------
#endif
