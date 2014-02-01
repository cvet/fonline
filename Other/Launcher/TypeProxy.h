//---------------------------------------------------------------------------

#ifndef TypeProxyH
#define TypeProxyH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "cspin.h"
#include "Common.h"
//---------------------------------------------------------------------------
class TTypeProxyForm : public TForm
{
__published:	// IDE-managed Components
	TGroupBox *GbProxy;
	TLabel *LabelProxyType;
	TLabel *LabelProxyHost;
	TLabel *LabelProxyPort;
	TLabel *LabelProxyName;
	TLabel *LabelProxyPass;
	TRadioButton *RbProxyNone;
	TRadioButton *RbProxySocks4;
	TRadioButton *RbProxySocks5;
	TRadioButton *RbProxyHttp;
	TCSpinEdit *SeProxyPort;
	TEdit *EditProxyUser;
	TEdit *EditProxyPass;
	TButton *BtnDone;
	TButton *BtnCancel;
	TEdit *EditProxyHost;
	void __fastcall BtnDoneClick(TObject *Sender);
	void __fastcall BtnCancelClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
	__fastcall TTypeProxyForm(TComponent* Owner);

	int ProxyType;
	AnsiString ProxyHost;
	int ProxyPort;
	AnsiString ProxyUsername;
	AnsiString ProxyPassword;
};
//---------------------------------------------------------------------------
extern PACKAGE TTypeProxyForm *TypeProxyForm;
//---------------------------------------------------------------------------
#endif
