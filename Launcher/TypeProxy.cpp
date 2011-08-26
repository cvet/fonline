//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "TypeProxy.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "cspin"
#pragma resource "*.dfm"
TTypeProxyForm *TypeProxyForm;
//---------------------------------------------------------------------------
__fastcall TTypeProxyForm::TTypeProxyForm(TComponent* Owner)
	: TForm(Owner)
{
	DoubleBuffered = true;
	Caption = LANG("Прокси", "Proxy");
	GbProxy->Caption = LANG("","");
	LabelProxyType->Caption = LANG("Тип", "Type");
	RbProxyNone->Caption = LANG("Нет", "None");
	RbProxySocks4->Caption = LANG("SOCKS4", "SOCKS4");
	RbProxySocks5->Caption = LANG("SOCKS5", "SOCKS5");
	RbProxyHttp->Caption = LANG("HTTP", "HTTP");
	LabelProxyHost->Caption = LANG("Хост", "Host");
	LabelProxyPort->Caption = LANG("Порт", "Port");
	LabelProxyName->Caption = LANG("Логин", "Login");
	LabelProxyPass->Caption = LANG("Пароль", "Password");
	BtnDone->Caption = LANG("Готово", "Done");
	BtnCancel->Caption = LANG("Отмена", "Cancel");
}
//---------------------------------------------------------------------------
void __fastcall TTypeProxyForm::BtnDoneClick(TObject *Sender)
{
	ProxyType = PROXY_NONE;
	ProxyType = RbProxySocks4->Checked ? PROXY_SOCKS4 : ProxyType;
	ProxyType = RbProxySocks5->Checked ? PROXY_SOCKS5 : ProxyType;
	ProxyType = RbProxyHttp->Checked ? PROXY_HTTP : ProxyType;
	ProxyHost = EditProxyHost->Text;
	ProxyPort = SeProxyPort->Value;
	ProxyUsername = EditProxyUser->Text;
	ProxyPassword = EditProxyPass->Text;

	if(ProxyHost == "") ProxyType = PROXY_NONE;

	ModalResult = mrOk;
}
//---------------------------------------------------------------------------
void __fastcall TTypeProxyForm::BtnCancelClick(TObject *Sender)
{
	Close();
}
//---------------------------------------------------------------------------

