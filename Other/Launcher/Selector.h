//---------------------------------------------------------------------------

#ifndef SelectorH
#define SelectorH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "Common.h"
#include <ExtCtrls.hpp>
#include <ComCtrls.hpp>
//---------------------------------------------------------------------------
#define SERVERS_LIST_URL  "http://fonline.ru/servers.html\0*RESERVED*RESERVED*RESERVED*"
//---------------------------------------------------------------------------
class TSelectorForm : public TForm
{
__published:	// IDE-managed Components
	TPanel *PanelMain;
	TListBox *LbServers;
	TPanel *PanelButtons;
	TRadioButton *RbLangRus;
	TRadioButton *RbLangEng;
	TLabel *LServerInfo;
	TLabel *LServers;
	TLabel *LOptions;
	TLabel *LWaitRus;
	TLabel *LWaitEng;
	TRichEdit *ReAbout;
	TTimer *TimerCheck;
	TButton *BtnSelect;
	TButton *BtnExit;
	TButton *BtnProxy;
	void __fastcall RbLangRusClick(TObject *Sender);
	void __fastcall RbLangEngClick(TObject *Sender);
	void __fastcall LbServersClick(TObject *Sender);
	void __fastcall TimerCheckTimer(TObject *Sender);
	void __fastcall BtnSelectClick(TObject *Sender);
	void __fastcall BtnExitClick(TObject *Sender);
	void __fastcall BtnProxyClick(TObject *Sender);
	void __fastcall LbServersDblClick(TObject *Sender);
public:	// User declarations
	__fastcall TSelectorForm(TComponent* Owner);

	class ServerInfo
	{
	public:
		HANDLE HThread;
		bool Pinged;
		AnsiString Name;
		AnsiString DescriptionRu;
		AnsiString DescriptionEn;
		AnsiString UpdaterHost;
		AnsiString UpdaterPort;

		ServerInfo():HThread(NULL),Pinged(false){}
		bool IsValid(){return Name != "" && UpdaterHost != "" && UpdaterPort != "";}
	};
	std::vector<ServerInfo*> Servers;
	ServerInfo Result;

	ServerInfo* GetSelectedServer();
	void Localization();
	bool ParseHost(const char* host, AnsiString& resultHost, AnsiString& resultPort);
	void ShowServers();

protected:
	void __fastcall WndProc(Messages::TMessage &Message);
};
//---------------------------------------------------------------------------
extern PACKAGE TSelectorForm *SelectorForm;
//---------------------------------------------------------------------------
#endif
