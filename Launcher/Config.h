//---------------------------------------------------------------------------

#ifndef ConfigH
#define ConfigH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <ComCtrls.hpp>
#include <Dialogs.hpp>
#include "cspin.h"
#include "Common.h"
#include "Selector.h"
//---------------------------------------------------------------------------
class TConfigForm : public TForm
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
	TLabel *LabelScrollStep;
	TLabel *LabelScrollDelay;
	TGroupBox *GbSoundVolume;
	TTrackBar *TbMusicVolume;
	TTrackBar *TbSoundVolume;
	TLabel *LabelMusicVolume;
	TLabel *LabelSoundVolume;
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
	TGroupBox *GbChangeGame;
	TButton *BtnChangeGame;
	void __fastcall BtnExitClick(TObject *Sender);
	void __fastcall RbEnglishClick(TObject *Sender);
	void __fastcall RbRussianClick(TObject *Sender);
	void __fastcall BtnParseClick(TObject *Sender);
	void __fastcall CbScreenWidthChange(TObject *Sender);
	void __fastcall CbScreenHeightChange(TObject *Sender);
	void __fastcall BtnChangeGameClick(TObject *Sender);
private:	// User declarations
	void Serialize(bool save);
	void Translate();
public:		// User declarations
	__fastcall TConfigForm(TComponent* Owner);

	TSelectorForm* Selector;
};
//---------------------------------------------------------------------------
extern PACKAGE TConfigForm *ConfigForm;
//---------------------------------------------------------------------------
#endif
