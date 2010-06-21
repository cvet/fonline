//---------------------------------------------------------------------------

#ifndef UpdaterClientFormH
#define UpdaterClientFormH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "cspin.h"
#include <Sockets.hpp>
#include "Common.h"
#include <ExtCtrls.hpp>
#include <vector>
//---------------------------------------------------------------------------
typedef std::vector<AnsiString> AnsiStringVec;
typedef std::vector<AnsiString>::iterator AnsiStringVecIt;

#define CFG_FILE                ".\\FOnline.cfg"
#define CFG_FILE_APP_NAME       "Game Options"
#define LANG_RUS                (0)
#define LANG_ENG                (1)

class TMainForm : public TForm
{
__published:	// IDE-managed Components
	TComboBox *CbHost;
	TLabel *LHost;
	TCSpinEdit *CsePort;
	TTcpClient *TcpClient;
	TLabel *LPort;
	TButton *BCheck;
	TMemo *MLog;
	TTimer *WaitTimer;
	void __fastcall BCheckClick(TObject *Sender);
	void __fastcall WaitTimerTimer(TObject *Sender);
private:	// User declarations
public:		// User declarations
	__fastcall TMainForm(TComponent* Owner);
	void Process();
	int DotsCount;
	int Lang;
};
//---------------------------------------------------------------------------
extern PACKAGE TMainForm *MainForm;
//---------------------------------------------------------------------------
#endif
