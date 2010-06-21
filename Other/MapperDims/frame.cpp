//////////////////////////////////////////////////////////////////////
// CFrame Class
//////////////////////////////////////////////////////////////////////

#include "frame.h"
#include "mdi.h"
#include "main.h"
#include "macros.h"
#include <vcl.h>
//---------------------------------------------------------------------------
CFrame::CFrame(void)
{
   data = NULL;
   width = NULL;
   height = NULL;
   a_width = NULL;
   foffX = NULL;
   foffY = NULL;
   sprX = NULL;
   sprY = NULL;
   doffX = NULL;
   doffY = NULL;
   data = NULL;
   pBMP = NULL;
   pMask = NULL;
   pLtMask = NULL;
}
//---------------------------------------------------------------------------
void CFrame::PrepareFrames()
{
    // nDirTotal - общее кол-во направлений
   width = new short* [nDirTotal];
   height = new short* [nDirTotal];
   a_width = new short* [nDirTotal];
   foffX = new signed short* [nDirTotal];
   foffY = new signed short* [nDirTotal];
   sprX = new signed short* [nDirTotal];
   sprY = new signed short* [nDirTotal];
   data = new BYTE ** [nDirTotal];
   for (int nDir = 0; nDir < nDirTotal; nDir++)
   {
      width[nDir] = new short [nFrames];
      height[nDir] = new short [nFrames];
      a_width[nDir] = new short [nFrames];
      foffX[nDir] = new signed short [nFrames];
      foffY[nDir] = new signed short [nFrames];
      sprX[nDir] = new signed short [nFrames];
      sprY[nDir] = new signed short [nFrames];
      data[nDir] = new BYTE * [nFrames];
   }
   doffX = new signed short [nDirTotal];
   doffY = new signed short [nDirTotal];
}
//---------------------------------------------------------------------------
void CFrame::BuildFrames(WORD WidthSpr, WORD HeightSpr)
{
    // Наверняка эта процедура строит 1 большой спрайт
    // содержащий все кадры
   dwSpriteWidth = WidthSpr;
   dwSpriteHeight = HeightSpr;
   DDSURFACEDESC2 ddSurfaceDesc;
   ZeroMemory(&ddSurfaceDesc, sizeof(ddSurfaceDesc));
   ddSurfaceDesc.dwSize = sizeof(ddSurfaceDesc);
   ddSurfaceDesc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
   ddSurfaceDesc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
//   ddsd.dwWidth = WeightSpr;
//   ddsd.dwHeight = HeightSpr;
//   ddSurfaceDesc.dwWidth = GetWi(0, 0) + 1;
//   ddSurfaceDesc.dwHeight = GetHe(0, 0) + 1;
   ddSurfaceDesc.dwWidth = WidthSpr;
   ddSurfaceDesc.dwHeight = HeightSpr;
   HRESULT res = frmMDI->pDD->CreateSurface (&ddSurfaceDesc, &pBMP, NULL);

   if (!pBMP)
      return;

   DDCOLORKEY ck;
   ck.dwColorSpaceLowValue = 0;
   ck.dwColorSpaceHighValue = 0;
   res = pBMP->SetColorKey(DDCKEY_SRCBLT, &ck);

   HDC dc;
   pBMP->GetDC(&dc);
   BitBlt (dc, 0,0,GetWi(0,0)+2,GetHe(0,0)+2, 0, 0,0, BLACKNESS);
//   for (int nDir = 0; nDir < 1; nDir++)
//   {
//      for (int nFrame = 0; nFrame < 1; nFrame++)
//      {
   for (int nDir = 0; nDir < nDirTotal; nDir++)
   {
      for (int nFrame = 0; nFrame < nFrames; nFrame++)
      {
         pPal->bmi.bmiHeader.biWidth = width[nDir][nFrame];
         pPal->bmi.bmiHeader.biHeight = height[nDir][nFrame];
         StretchDIBits(dc, sprX[nDir][nFrame], sprY[nDir][nFrame],
                       width[nDir][nFrame], height[nDir][nFrame],
                       0, 0, width[nDir][nFrame], height[nDir][nFrame],
                       data[nDir][nFrame], (LPBITMAPINFO)&pPal->bmi,
                       DIB_RGB_COLORS, SRCCOPY);
      }
   }
   pBMP->ReleaseDC(dc);
}
//---------------------------------------------------------------------------
void CFrame::LoadData(int nDir, int nFrame, BYTE *tdata)
{
   //Получаем кол-во байт, необходимых для кратности scanline
   a_width[nDir][nFrame] = (4 - width[nDir][nFrame] % 4) % 4;
   //Получаем размер буффера для преобразованного растра
   int size = (a_width[nDir][nFrame] + width[nDir][nFrame])
              * height[nDir][nFrame];
   if ((data[nDir][nFrame] = (BYTE *)malloc(size)) == NULL) return;
   tdata += width[nDir][nFrame] * (height[nDir][nFrame] - 1);
   // Данные храняться в перевернутом виде
   for (int i = 0; i < height[nDir][nFrame]; i++)
      memcpy(data[nDir][nFrame] + i * (width[nDir][nFrame] +
                        a_width[nDir][nFrame]), tdata - i * width[nDir][nFrame],
                                                           width[nDir][nFrame]);
//   GetBmp();
}
//---------------------------------------------------------------------------
BYTE CFrame::GetCenterPix(int nDir, int nFrame)
{
   if (data != NULL)
      return data[nDir][nFrame][(height[nDir][nFrame] >> 1) *
                              width[nDir][nFrame] + (width[nDir][nFrame] >> 1)];
   return 0;
}
//---------------------------------------------------------------------------
BYTE CFrame::GetPixel(int X, int Y, int nDir, int nFrame)
{
    if (X >= width[nDir][nFrame] || Y>= height[nDir][nFrame]) return 0;
   if (data[nDir][nFrame] != NULL)
   {
      Y = height[nDir][nFrame] - Y;
      return data[nDir][nFrame][(width[nDir][nFrame] +
                                                a_width[nDir][nFrame]) * Y + X];
   }
   return 0;
}
//---------------------------------------------------------------------------
HBITMAP CFrame::GetHBITMAP(void)
{
/*
   pPal->bmi.bmiHeader.biWidth = width[nDir][nFrame];
   pPal->bmi.bmiHeader.biHeight = height[nDir][nFrame];
   HDC hDC = GetDC(NULL);
   HBITMAP hbm = CreateDIBitmap(hDC, (LPBITMAPINFOHEADER)&pPal->bmi.bmiHeader,
        (LONG)CBM_INIT, (LPBYTE)data[nDir][nFrame],
        (LPBITMAPINFO)&pPal->bmi, DIB_RGB_COLORS);
   ReleaseDC(NULL, hDC);
   return hbm;
*/
}
//---------------------------------------------------------------------------
LPDIRECTDRAWSURFACE7 CFrame::GetBMP(void)
{
   return pBMP;
}
//---------------------------------------------------------------------------
void CFrame::InitBorder(WORD nDir, WORD nFrame, int b_width, int b_height)
{
   if (pMask && b_width == nBorderWidth && b_height == nBorderHeight)
      return;
   if (pMask)
   {
      pMask->Release();
      pMask = NULL;
   }
   DDSURFACEDESC2 ddSurfaceDesc;
   ZeroMemory(&ddSurfaceDesc, sizeof(ddSurfaceDesc));
   ddSurfaceDesc.dwSize = sizeof(ddSurfaceDesc);
   ddSurfaceDesc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
   ddSurfaceDesc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
   ddSurfaceDesc.dwWidth = b_width;
   ddSurfaceDesc.dwHeight = b_height;
   frmMDI->pDD->CreateSurface(&ddSurfaceDesc, &pMask, NULL);
   nBorderWidth = b_width;
   nBorderHeight = b_height;

   DDCOLORKEY ck;
   ck.dwColorSpaceLowValue = 0;
   ck.dwColorSpaceHighValue = 0;
   pMask->SetColorKey(DDCKEY_SRCBLT, &ck);

   Graphics::TBitmap *frm_bmp = new Graphics::TBitmap();
   frm_bmp->Width = b_width;
   frm_bmp->Height = b_height;
   PatBlt(frm_bmp->Canvas->Handle, 0, 0, b_width, b_height, BLACKNESS);
   frm_bmp->Canvas->Brush->Color = clAqua; //
   RECT rc = Rect(0, 0, b_width, b_height);
   FrameRect (frm_bmp->Canvas->Handle, &rc, frm_bmp->Canvas->Brush->Handle);
   frm_bmp->Canvas->Brush->Style = bsBDiagonal; //
   rc = Rect(1, 1, b_width - 1, b_height - 1);
   FrameRect (frm_bmp->Canvas->Handle, &rc, frm_bmp->Canvas->Brush->Handle);
   HDC dc;
   pMask->GetDC(&dc);
   BitBlt (dc, 0, 0, b_width, b_height, frm_bmp->Canvas->Handle, 0, 0, SRCCOPY);
   pMask->ReleaseDC(dc);
   delete frm_bmp;
}
//---------------------------------------------------------------------------
LPDIRECTDRAWSURFACE7 CFrame::GetBorder(void)
{
   return pMask;
}
//---------------------------------------------------------------------------
short CFrame::GetWi(WORD nDir, WORD nFrame)
{
   if (!pBMP) return 0;
   if (nDir <= nDirTotal - 1 && nFrame <= nFrames - 1)
      return width[nDir][nFrame];
   else
      return width[0][0];
}
//---------------------------------------------------------------------------
short CFrame::GetHe(WORD nDir, WORD nFrame)
{
   if (!pBMP) return 0;
   if (nDir <= nDirTotal - 1 && nFrame <= nFrames - 1)
      return height[nDir][nFrame];
   else
      return height[0][0];
}
//---------------------------------------------------------------------------
short CFrame::GetSprX(WORD nDir, WORD nFrame)
{
   if (!pBMP) return 0;
   if (nDir <= nDirTotal - 1 && nFrame <= nFrames - 1)
      return sprX[nDir][nFrame];
   else
      return sprX[0][0];
}
//---------------------------------------------------------------------------
short CFrame::GetSprY(WORD nDir, WORD nFrame)
{
   if (!pBMP) return 0;
   if (nDir <= nDirTotal - 1 && nFrame <= nFrames - 1)
      return sprY[nDir][nFrame];
   else
      return sprY[0][0];
}
//---------------------------------------------------------------------------
signed short CFrame::GetDoffX(WORD nDir)
{
   if (!pBMP) return 0;
   if (nDir <= nDirTotal - 1)
      return doffX[nDir];
   else
      return doffX[0];
}
//---------------------------------------------------------------------------
signed short CFrame::GetDoffY(WORD nDir)
{
   if (!pBMP) return 0;
   if (nDir <= nDirTotal -1)
      return doffY[nDir];
   else
      return doffY[0];
}
//---------------------------------------------------------------------------
CFrame::~CFrame()
{
   for (int nDir = 0; nDir < nDirTotal; nDir++)
   {
      if (width && width[nDir]) delete[] width[nDir];
      if (height && height[nDir]) delete[] height[nDir];
      if (a_width && a_width[nDir]) delete[] a_width[nDir];
      if (foffX && foffX[nDir]) delete[] foffX[nDir];
      if (foffY && foffY[nDir]) delete[] foffY[nDir];
      if (sprX && sprX[nDir]) delete[] sprX[nDir];
      if (sprY && sprY[nDir]) delete[] sprY[nDir];
      if (data && data[nDir]) {
         for (int i=0; i<nFrames; i++) {
            if (data[nDir][i]) free (data[nDir][i]);
         }
         delete[] data[nDir];
      }
   }
   if (width) delete[] width;
   if (height) delete[] height;
   if (a_width) delete[] a_width;
   if (foffX) delete[] foffX;
   if (foffY) delete[] foffY;
   if (sprX) delete[] sprX;
   if (sprY) delete[] sprY;
   if (data) delete[] data;
   if (doffX) delete[] doffX;
   if (doffY) delete[] doffY;
   FreeUp();
}
//---------------------------------------------------------------------------
void CFrame::FreeUp()
{
   if (pBMP) pBMP->Release(); pBMP = NULL;
   if (pMask) pMask->Release(); pMask = NULL;
   if (pLtMask) pLtMask->Release(); pLtMask = NULL;
}
//---------------------------------------------------------------------------
Graphics::TBitmap* CreateSpriteMask (Graphics::TBitmap &bmp, int w, int h)
{
   Graphics::TBitmap* tmp = new Graphics::TBitmap();
   tmp->Assign (&bmp);
   tmp->Mask (clBlack);
   // в tmp лежит маска для исходного спрайта
   // (всё, что было чёрным цветом в спрайте заменено на белый в маске,
   // остальные цвета заменены чёрным)

   Graphics::TBitmap* tmp1 = new Graphics::TBitmap();
   int w1 = w + 2;
   int h1 = h + 2;
   RECT rc = Rect (0, 0, w, h);
   RECT rc1 = Rect (0, 0, w1, h1);
   tmp1->Width = w1;
   tmp1->Height = h1;
   tmp1->Canvas->Brush->Color = pUtil->selColor;
   tmp1->Canvas->FillRect (rc1);

   tmp1->Canvas->CopyMode = cmSrcAnd;
   for (int x = 0; x < 3; x++)
   {
      for (int y = 0; y < 3; y++)
      {
         tmp1->Canvas->CopyRect (Bounds(x, y, w, h), tmp->Canvas, rc);
      }
   }
   // Теперь tmp1 содержит "раздвинутую" маску первого подкадра исходной
   // картинки - дополнительно вырезано по одному пикселю с каждой стороны
   // Маска изначально делается полностью закрашенной, а потом из неё
   // 9 раз вырезается профиль спрайта (со смещением - чтоб сделать дырку).

   Graphics::TBitmap* cntr = new Graphics::TBitmap();
   cntr->Width = w1;
   cntr->Height = h1;
   cntr->Canvas->Brush->Color = pUtil->selColor;
   cntr->Canvas->FillRect (rc1);
   cntr->Canvas->CopyMode = cmSrcAnd;
   cntr->Canvas->CopyRect(Bounds(1, 1, w, h), tmp->Canvas, rc);
   // cntr - "центральная" маска. От предыдущей отличается тем, что здесь
   // вырезан только один профиль картинки. То есть эта маска на один пиксель
   // у'же предыдущей.

   tmp1->Canvas->CopyMode = cmSrcInvert;
   tmp1->Canvas->CopyRect (rc1, cntr->Canvas, rc1);
   // Теперь XOR-им эти две маски - остаётся только каёмочка (один пиксель,
   // на который отличались маски).
   delete tmp;
   delete cntr;
   return tmp1;
}
//---------------------------------------------------------------------------
LPDIRECTDRAWSURFACE7 CFrame::GetMask(WORD nDir, WORD nFrame)
{
   if (!pMask)
   {
      DDSURFACEDESC2 ddSurfaceDesc;
      ZeroMemory(&ddSurfaceDesc, sizeof(ddSurfaceDesc));
      ddSurfaceDesc.dwSize = sizeof(ddSurfaceDesc);
      ddSurfaceDesc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
      ddSurfaceDesc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
      ddSurfaceDesc.dwWidth = GetWi(nDir, nFrame) + 2;
      ddSurfaceDesc.dwHeight = GetHe(nDir, nFrame) + 2;
      frmMDI->pDD->CreateSurface(&ddSurfaceDesc, &pMask, NULL);

      DDCOLORKEY ck;
      ck.dwColorSpaceLowValue = 0;
      ck.dwColorSpaceHighValue = 0;
      pMask->SetColorKey(DDCKEY_SRCBLT, &ck);

      HDC dc;
      pBMP->GetDC(&dc);
      Graphics::TBitmap *frm_bmp = new Graphics::TBitmap();
      frm_bmp->Width = GetWi(nDir, nFrame);
      frm_bmp->Height = GetHe(nDir, nFrame);
//      BitBlt (frm_bmp->Canvas->Handle, 0, 0,
//                    GetWi(nDir, nFrame), GetHe(nDir, nFrame), dc, 0,0, SRCCOPY);
      BitBlt (frm_bmp->Canvas->Handle, GetSprX(nDir, nFrame), GetSprY(nDir, nFrame),
                    GetWi(nDir, nFrame), GetHe(nDir, nFrame), dc, 0,0, SRCCOPY);
      Graphics::TBitmap *frm_mask = CreateSpriteMask (*frm_bmp,
                                      GetWi(nDir, nFrame), GetHe(nDir, nFrame));
      delete frm_bmp;
      pBMP->ReleaseDC(dc);
      pMask->GetDC(&dc);
      BitBlt (dc, 0,0, GetWi(nDir, nFrame) + 2, GetHe(nDir, nFrame) + 2,
              frm_mask->Canvas->Handle, 0,0, SRCCOPY);
      pMask->ReleaseDC(dc);
      delete frm_mask;
   }
   return pMask;
}
//---------------------------------------------------------------------------
LPDIRECTDRAWSURFACE7 CFrame::GetLtMask(WORD nDir, WORD nFrame)
{
   if (!pLtMask)
   {
      int newWidth = GetWi(nDir, nFrame);
      newWidth = newWidth > NAV_SIZE_X ? NAV_SIZE_X : newWidth;
      int newHeight = GetHe(nDir, nFrame);
      newHeight = newHeight > NAV_SIZE_Y ? NAV_SIZE_Y : newHeight;

      double ar; // aspect ratio of the frame
      if (newWidth && newHeight)
      {
	 double ax = (double)GetWi(nDir, nFrame) / newWidth;
         double ay = (double)GetHe(nDir, nFrame)/newHeight;
	 ar = max (ax, ay);
	 if (ar > .001)
         {
	    newWidth = (int)GetWi(nDir, nFrame) / ar;
	    newHeight = (int)GetHe (nDir, nFrame) / ar;
	 }
      }
      if (ar == 1.0)
      {
      	 pLtMask = GetMask(nDir, nFrame);
	 pLtMask->AddRef();
      }
      else
      {
	 DDSURFACEDESC2 ddSurfaceDesc;
         ZeroMemory(&ddSurfaceDesc, sizeof(ddSurfaceDesc));
 	 ddSurfaceDesc.dwSize = sizeof(ddSurfaceDesc);
	 ddSurfaceDesc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	 ddSurfaceDesc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	 ddSurfaceDesc.dwWidth = GetWi(nDir, nFrame) + 2; //newWidth+2;
	 ddSurfaceDesc.dwHeight = GetHe(nDir, nFrame) + 2; //newHeight+2;
	 HRESULT res = frmMDI->pDD->CreateSurface(&ddSurfaceDesc, &pLtMask, NULL);

	 DDCOLORKEY ck;
	 ck.dwColorSpaceLowValue = 0;
	 ck.dwColorSpaceHighValue = 0;
	 pLtMask->SetColorKey(DDCKEY_SRCBLT, &ck);
         RECT rcs = Rect(0, 0, GetWi(nDir, nFrame), GetHe(nDir, nFrame));
	 RECT rcd = Rect(0, 0, newWidth, newHeight);
	 HDC dc;
	 pLtMask->GetDC(&dc);
	 BitBlt (dc, 0, 0, GetWi(nDir, nFrame) + 2, GetHe(nDir, nFrame) + 2, 0,
                                                               0, 0, BLACKNESS);
	 pLtMask->ReleaseDC(&dc);

	 pLtMask->Blt (&rcd, pBMP, &rcs, DDBLT_WAIT, NULL);

	 pLtMask->GetDC(&dc);
	 Graphics::TBitmap *frm_bmp = new Graphics::TBitmap();
	 frm_bmp->Width = newWidth; frm_bmp->Height = newHeight;
	 BitBlt (frm_bmp->Canvas->Handle, 0, 0, newWidth, newHeight, dc,
                                                                 0, 0, SRCCOPY);

	 Graphics::TBitmap *frm_mask = CreateSpriteMask(*frm_bmp, newWidth, newHeight);
	 BitBlt (dc, 0, 0, newWidth + 2, newHeight + 2, frm_mask->Canvas->Handle,
                                                                 0, 0, SRCCOPY);
	 pLtMask->ReleaseDC(dc);
	 delete frm_bmp;
	 delete frm_mask;
      }
   }
   return pLtMask;
}
//---------------------------------------------------------------------------
