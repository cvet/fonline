//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "FormAskLevel.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmSelLevel *frmSelLevel;
//---------------------------------------------------------------------------
__fastcall TfrmSelLevel::TfrmSelLevel(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TfrmSelLevel::Button1Click(TObject *Sender)
{
    Close();
}
//---------------------------------------------------------------------------
