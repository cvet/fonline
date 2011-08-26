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
#include <Sockets.hpp>
#include <IdContext.hpp>
#include <vector>
#include "../Common.h"
//---------------------------------------------------------------------------
#define LOG(mess) do{MLog->Lines->Add(mess); MLog->Refresh();}while(0)
#define SEND_PORTION       (100000)

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
	TLabel *LBytesSent;
	TLabel *Label7;
	TLabel *LFilesSent;
	TShape *ShapeIndicator;
	TIdTCPServer *IdTCPServer;
	TSplitter *Splitter;
	TLabel *Label6;
	TLabel *LPings;
	void __fastcall BProcessClick(TObject *Sender);
	void __fastcall BRecalcFilesClick(TObject *Sender);
	void __fastcall IdTCPServerExecute(TIdContext *AContext);
	void __fastcall IdTCPServerConnect(TIdContext *AContext);
	void __fastcall IdTCPServerDisconnect(TIdContext *AContext);
	void __fastcall IdTCPServerException(TIdContext *AContext,
		  Exception *AException);
	void __fastcall FormShow(TObject *Sender);
private:	// User declarations
public:		// User declarations
	__fastcall TServerForm(TComponent* Owner);
	void SwitchListen();
	void LoadConfigArray(AnsiStringVec& cfg, AnsiString keyName);
	bool CheckName(const char* name, AnsiStringVec& where);
	void RecursiveDirLook(const char* initDir, const char* curDir, const char* query);
	void RecalcFiles();

	struct PrepFile
	{
		AnsiString FName;
		AnsiString FHash;
		AnsiString FSize;
		AnsiString FOptions;
		TStream* Stream;
		TCriticalSection* StreamLocker;
		DynamicArray<System::Byte> StreamBuffer;
		bool Packed;
	};
typedef std::vector<PrepFile> PrepFileVec;
typedef std::vector<PrepFile>::iterator PrepFileVecIt;
	PrepFileVec PrepFiles;

	AnsiString GameFileName;
	AnsiString UpdaterURL;
	AnsiString GameServer;
	AnsiString GameServerPort;
	AnsiStringVec CfgContent;
	AnsiStringVec CfgQuery;
	AnsiStringVec CfgIgnore;
	AnsiStringVec CfgPack;
	AnsiStringVec CfgNoRewrite;
	AnsiStringVec CfgDelete;
	AnsiStringVec CfgHddInsteadRam;

	__int64 ConnAll, ConnCur, ConnSucc, ConnFail;
	__int64 BytesSent, FilesSent;
	__int64 Pings;
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
