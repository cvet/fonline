//---------------------------------------------------------------------------

#ifndef TypeServerH
#define TypeServerH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "Common.h"
//---------------------------------------------------------------------------
class TTypeServerForm : public TForm
{
__published:	// IDE-managed Components
	TButton *BtnCancel;
	TButton *BtnDone;
	TEdit *EditUpdaterHost;
	TLabel *LUpdaterHost;
	TEdit *EditUpdaterPort;
	void __fastcall BtnCancelClick(TObject *Sender);
	void __fastcall BtnDoneClick(TObject *Sender);
	void __fastcall EditUpdaterHostKeyDown(TObject *Sender, WORD &Key,
          TShiftState Shift);
private:	// User declarations
public:		// User declarations
	__fastcall TTypeServerForm(TComponent* Owner);
	void GetResult(AnsiString& updaterHost, AnsiString& updaterPort);
};
//---------------------------------------------------------------------------
extern PACKAGE TTypeServerForm *TypeServerForm;
//---------------------------------------------------------------------------
#endif
