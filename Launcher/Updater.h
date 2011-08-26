//---------------------------------------------------------------------------

#ifndef UpdaterH
#define UpdaterH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "cgauges.h"
#include <OleCtrls.hpp>
#include <SHDocVw.hpp>
#include "Common.h"
#include <Sockets.hpp>
#include <ExtCtrls.hpp>
#include "SHDocVw_OCX.h"
#include <HTTPApp.hpp>
#include <stdio.h>
#include <cdoexm.h>
//---------------------------------------------------------------------------
#define RECV_PORTION       (100000)
//---------------------------------------------------------------------------
class TUpdaterForm : public TForm
{
__published:	// IDE-managed Components
	TPanel *PanelWB;
	TCppWebBrowser *WebBrowser;
	TPanel *PanelMain;
	TLabel *LGauge;
	TButton *BtnPlay;
	TButton *BtnOptions;
	TTcpClient *TcpClient;
	TShape *ShapeGaugeBg;
	TShape *ShapeGauge;
	TLabel *LGaugeProgress;
	TPanel *PanelWBCached;
	TCppWebBrowser *WebBrowserCached;
	void __fastcall BtnOptionsClick(TObject *Sender);
	void __fastcall BtnPlayClick(TObject *Sender);
	void __fastcall FormActivate(TObject *Sender);
	void __fastcall WebBrowserDocumentComplete(TObject *Sender, LPDISPATCH pDisp, Variant *URL);
	void __fastcall WebBrowserNavigateError(TObject *Sender, LPDISPATCH pDisp, Variant *URL,
          Variant *Frame, Variant *StatusCode, VARIANT_BOOL *Cancel);
	void __fastcall WebBrowserCachedNavigateError(TObject *Sender, LPDISPATCH pDisp,
          Variant *URL, Variant *Frame, Variant *StatusCode, VARIANT_BOOL *Cancel);


private:	// User declarations
	AnsiString LauncherPath;
    AnsiString OptionsFile;
	AnsiString FileHashes;
	AnsiString WebPageMht;

	AnsiString ExeName;
	AnsiString UpdaterHost;
	AnsiString UpdaterPort;
	AnsiString UpdaterUrl;
	AnsiString LastLogMessageRus;
	AnsiString LastLogMessageEng;
	bool IsPlayed;

	__int64 ProgressMax;
	__int64 ProgressCur;

	AnsiStringVec CachedHashes; // Name - Hash - Name - Hash...
	void LoadCachedHashes();
	int GetCachedHash(const AnsiString& name);
	void AddCachedHash(const AnsiString& name, int hash);

	void GaugeText(AnsiString rus, AnsiString eng);
	void GaugeText(AnsiString text);
	bool Receive(char* buf, int bytes, bool showProgress);

	void UpdateSelf();

public:		// User declarations
	__fastcall TUpdaterForm(TComponent* Owner);
	void Process();
};
//---------------------------------------------------------------------------
extern PACKAGE TUpdaterForm *UpdaterForm;
//---------------------------------------------------------------------------
#endif
