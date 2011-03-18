//---------------------------------------------------------------------------

#ifndef FOConfigH
#define FOConfigH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <ComCtrls.hpp>
#include <Dialogs.hpp>
#include "cspin.h"
//---------------------------------------------------------------------------
#define CFG_FILE			    ".\\FOnline.cfg"
#define CFG_FILE_APP_NAME	    "Game Options"
//---------------------------------------------------------------------------
class TMainForm : public TForm
{
__published:	// IDE-managed Components
	TButton *BtnExit;
	TButton *BtnParse;
	TPageControl *PageControl;
	TTabSheet *TabOther;
	TTabSheet *TabNet;
	TTabSheet *TabVideo;
	TGroupBox *GbLanguage;
	TRadioButton *RbRussian;
	TRadioButton *RbEnglish;
	TGroupBox *GbOther;
	TCheckBox *CbWinNotify;
	TCheckBox *CbSoundNotify;
	TCheckBox *CbInvertMessBox;
	TCheckBox *CbLogging;
	TCheckBox *CbLoggingTime;
	TTabSheet *TabGame;
	TTabSheet *TabSound;
	TGroupBox *GbGame;
	TLabel *LabelTextDelay;
	TLabel *LabelMouseSpeed;
	TLabel *LabelScrollStep;
	TLabel *LabelScrollDelay;
	TGroupBox *GbSoundVolume;
	TTrackBar *TbMusicVolume;
	TTrackBar *TbSoundVolume;
	TLabel *LabelMusicVolume;
	TLabel *LabelSoundVolume;
	TGroupBox *GbSoundOther;
	TCheckBox *CbGlobalSound;
	TGroupBox *GbServer;
	TGroupBox *GbProxy;
	TComboBox *CbServerHost;
	TLabel *LabelServerHost;
	TLabel *LabelServerPort;
	TLabel *LabelProxyType;
	TRadioButton *RbProxySocks4;
	TRadioButton *RbProxySocks5;
	TRadioButton *RbProxyHttp;
	TCSpinEdit *SeScrollDelay;
	TCSpinEdit *SeScrollStep;
	TCSpinEdit *SeMouseSpeed;
	TCSpinEdit *SeTextDelay;
	TCSpinEdit *SeServerPort;
	TLabel *LabelProxyHost;
	TLabel *LabelProxyPort;
	TCSpinEdit *SeProxyPort;
	TComboBox *CbProxyHost;
	TLabel *LabelProxyName;
	TEdit *EditProxyUser;
	TRadioButton *RbProxyNone;
	TEdit *EditProxyPass;
	TLabel *LabelProxyPass;
	TGroupBox *GbScreenSize;
	TGroupBox *GbVideoOther;
	TComboBox *CbScreenWidth;
	TComboBox *CbScreenHeight;
	TLabel *LabelLight;
	TLabel *LabelSprites;
	TLabel *LabelTexture;
	TCheckBox *CbFullScreen;
	TCheckBox *CbClearScreen;
	TCheckBox *CbVSync;
	TCheckBox *CbAlwaysOnTop;
	TCSpinEdit *SeLight;
	TCSpinEdit *SeSprites;
	TCSpinEdit *SeTexture;
	TButton *BtnExecute;
	TTabSheet *TabCombat;
	TGroupBox *GbDefCmbtMode;
	TGroupBox *GbIndicator;
	TGroupBox *GbCmbtMess;
	TRadioButton *RbCmbtMessVerbose;
	TRadioButton *RbCmbtMessBrief;
	TGroupBox *GbDamageHitDelay;
	TCSpinEdit *SeDamageHitDelayValue;
	TRadioButton *RbDefCmbtModeBoth;
	TRadioButton *RbDefCmbtModeRt;
	TRadioButton *RbDefCmbtModeTb;
	TRadioButton *RbIndicatorLines;
	TRadioButton *RbIndicatorNumbers;
	TRadioButton *RbIndicatorBoth;
	TLabel *LabelDamageHitDelay;
	TCheckBox *CbSoftwareSkinning;
	TLabel *LabelSleep;
	TCSpinEdit *SeSleep;
	TComboBox *CbMultisampling;
	TLabel *LabelMultisampling;
	TLabel *LabelAnimation3dFPS;
	TLabel *LabelAnimation3dSmoothTime;
	TCSpinEdit *SeAnimation3dFPS;
	TCSpinEdit *SeAnimation3dSmoothTime;
	TCheckBox *CbAlwaysRun;
	TGroupBox *GbLangSwitch;
	TRadioButton *RbCtrlShift;
	TRadioButton *RbAltShift;
	void __fastcall FormCreate(TObject *Sender);
	void __fastcall BtnExitClick(TObject *Sender);
	void __fastcall RbEnglishClick(TObject *Sender);
	void __fastcall RbRussianClick(TObject *Sender);
	void __fastcall BtnParseClick(TObject *Sender);
	void __fastcall CbScreenWidthChange(TObject *Sender);
	void __fastcall CbScreenHeightChange(TObject *Sender);
	void __fastcall BtnExecuteClick(TObject *Sender);
private:	// User declarations
	void Serialize(bool save);
	AnsiString Lang;
	void Translate();
public:		// User declarations
	__fastcall TMainForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TMainForm *MainForm;
//---------------------------------------------------------------------------
#endif
