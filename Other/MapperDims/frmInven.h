//---------------------------------------------------------------------------


#ifndef frmInvenH
#define frmInvenH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------
class TfrmInv : public TFrame
{
__published:	// IDE-managed Components
    TShape *Shape1;
    TPanel *imgInv;
    TScrollBar *sbINV;
    TSpeedButton *btnChange;
    TSpeedButton *btnRemove;
    TSpeedButton *btnAdd;
private:	// User declarations
public:		// User declarations
    __fastcall TfrmInv(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmInv *frmInv;
//---------------------------------------------------------------------------
#endif
