//---------------------------------------------------------------------------

#ifndef FormAskLevelH
#define FormAskLevelH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------
class TfrmSelLevel : public TForm
{
__published:	// IDE-managed Components
    TButton *Button1;
    TLabel *Label1;
    TUpDown *UpDown1;
    TEdit *Edit1;
    TShape *Shape1;
    void __fastcall Button1Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TfrmSelLevel(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmSelLevel *frmSelLevel;
//---------------------------------------------------------------------------
#endif
