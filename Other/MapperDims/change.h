//---------------------------------------------------------------------------

#ifndef changeH
#define changeH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <Mask.hpp>
//---------------------------------------------------------------------------
class TfmChange : public TForm
{
__published:	// IDE-managed Components
        TUpDown *UpDown1;
        TButton *btnOK;
        TButton *btnCancel;
        TEdit *edValue;
        void __fastcall FormCloseQuery(TObject *Sender, bool &CanClose);
private:	// User declarations
        int nCurrentValue, nOldValue;
public:		// User declarations
        __fastcall TfmChange(TComponent* Owner);
        DWORD ChangeValue(DWORD nValueToChange);
};
//---------------------------------------------------------------------------
extern PACKAGE TfmChange *fmChange;
//---------------------------------------------------------------------------
#endif
