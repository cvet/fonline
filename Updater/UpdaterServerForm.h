//---------------------------------------------------------------------------

#ifndef UpdaterServerFormH
#define UpdaterServerFormH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "cspin.h"
#include <ExtCtrls.hpp>
#include "IdCustomTCPServer.hpp"
#include <IdBaseComponent.hpp>
#include <IdComponent.hpp>
#include <IdTCPServer.hpp>
#include <vector>
#include "Common.h"
//---------------------------------------------------------------------------
#define LOG(mess) do{MLog->Lines->Add(mess); MLog->Refresh();}while(0)

class ContextInfo : public TObject
{
public:
	int Version;
	bool IsClosed;
};

class TServerForm : public TForm
{
__published:	// IDE-managed Components
	TMemo *MLog;
	TListBox *LbList;
	TGroupBox *GbOptions;
	TCSpinEdit *CseListenPort;
	TLabel *LPort;
	TButton *BProcess;
	TLabel *LConnAll;
	TLabel *LConnSucc;
	TLabel *LConnFail;
	TLabel *LConnCur;
	TButton *BRecalcFiles;
	TLabel *Label1;
	TLabel *Label2;
	TLabel *Label3;
	TLabel *Label4;
	TLabel *Label5;
	TLabel *LBytesSend;
	TLabel *Label7;
	TLabel *LFilesSend;
	TShape *ShapeIndicator;
	TIdTCPServer *IdTCPServer;
	TButton *Button1;
	TButton *Button2;
	void __fastcall BProcessClick(TObject *Sender);
	void __fastcall BRecalcFilesClick(TObject *Sender);
	void __fastcall IdTCPServerExecute(TIdContext *AContext);
	void __fastcall IdTCPServerConnect(TIdContext *AContext);
	void __fastcall IdTCPServerDisconnect(TIdContext *AContext);
	void __fastcall IdTCPServerException(TIdContext *AContext,
          Exception *AException);
private:	// User declarations
public:		// User declarations
	__fastcall TServerForm(TComponent* Owner);
	void SwitchListen();
	void RecalcFiles();
	unsigned int BufferSize;

	struct PrepFile
	{
		AnsiString FName;
		AnsiString FHash;
		TMemoryStream* Stream;
		TMemoryStream* StreamPack;
	};
typedef std::vector<PrepFile> PrepFileVec;
	PrepFileVec PrepFiles;

	__int64 ConnAll,ConnCur,ConnSucc,ConnFail;
	__int64 BytesSend,FilesSend;
	void UpdateInfo();
	void ConnStart(TIdContext* context, const AnsiString& infoMessage);
	void ConnError(TIdContext* context, const AnsiString& errorMessage);
	void ConnEnd(TIdContext* context, const AnsiString& infoMessage);
	void ConnInfo(TIdContext* context, const AnsiString& infoMessage);
	bool SendData(TIdContext* context, TMemoryStream* stream, const AnsiString& errorMessage);
	bool SendData(TIdContext* context, void* data, unsigned int length, const AnsiString& errorMessage);
	bool SendStr(TIdContext* context, AnsiString str, const AnsiString& errorMessage);
	AnsiString RecvStr(TIdContext* context, const AnsiString& errorMessage);
};
//---------------------------------------------------------------------------
extern PACKAGE TServerForm *ServerForm;
//---------------------------------------------------------------------------
#endif
