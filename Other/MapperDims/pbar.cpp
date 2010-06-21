//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "pbar.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmPBar *frmPBar;
//---------------------------------------------------------------------------
__fastcall TfrmPBar::TfrmPBar(TComponent* Owner)
        : TForm(Owner)
{
   SendMessage(pbar->Handle, PBM_SETBARCOLOR, 0, RGB(0, 0, 0));
}
//---------------------------------------------------------------------------
void TfrmPBar::NewTask(String sCaption, String sStatus, int nStart, int nEnd,
                                                                       int *Pos)
{
//   this->Repaint();
//   pbar->Position = pbar->Min;
//   pbar->Repaint();
   if (sCaption != NULL) lblCaption->Caption = sCaption;
   if (sStatus != NULL) lblStatus->Caption = sStatus;
   pbar->Min = nStart;
   pbar->Max = nEnd;
   pbar->Position = *Pos;
   nPos = Pos;
   pbar->Repaint();
   *nPos = nStart;
   tm->Enabled = true;
}
//---------------------------------------------------------------------------
void TfrmPBar::EndTask(void)
{
   pbar->Position = pbar->Max;
   pbar->Repaint();
   tm->Enabled = false;
}
//---------------------------------------------------------------------------
void __fastcall TfrmPBar::UpdatePos(TObject *Sender)
{
   pbar->Position = *nPos;
   if(*nPos >= pbar->Max) EndTask();
}
//---------------------------------------------------------------------------

