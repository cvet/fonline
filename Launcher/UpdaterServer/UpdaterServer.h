//---------------------------------------------------------------------------

#ifndef UpdaterServerH
#define UpdaterServerH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
//---------------------------------------------------------------------------
class TServerForm : public TForm
{
__published:	// IDE-managed Components
private:	// User declarations
public:		// User declarations
	__fastcall TServerForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TServerForm *ServerForm;
//---------------------------------------------------------------------------
#endif
