//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "change.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfmChange *fmChange;
//---------------------------------------------------------------------------
__fastcall TfmChange::TfmChange(TComponent* Owner)
        : TForm(Owner)
{
   nOldValue = 1; 
}
//---------------------------------------------------------------------------
DWORD TfmChange::ChangeValue(DWORD nValueToChange)
{
   nOldValue = nCurrentValue;
   nCurrentValue = nValueToChange;
   UpDown1->Position = nCurrentValue;
   edValue->Text = String(nValueToChange);
   int result = this->ShowModal();
   return result == mrOk ? nCurrentValue : nValueToChange;
}
//---------------------------------------------------------------------------
void __fastcall TfmChange::FormCloseQuery(TObject *Sender, bool &CanClose)
{
   CanClose = true;
   try
   {
      nCurrentValue = edValue->Text.ToInt();
   }
   catch(Exception &exception)
   {
      nCurrentValue = nOldValue;
      UpDown1->Position = nCurrentValue;
      CanClose = false;
   }
}
//---------------------------------------------------------------------------

