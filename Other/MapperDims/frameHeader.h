//---------------------------------------------------------------------------


#ifndef frameHeaderH
#define frameHeaderH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include "macros.h"
#include <ComCtrls.hpp>
//---------------------------------------------------------------------------
class TfrmHdr : public TFrame
{
__published:	// IDE-managed Components
    TShape *Shape10;
    TLabel *Label2;
    TLabel *Label3;
    TLabel *Label4;
    TLabel *Label5;
    TLabel *Label6;
	TEdit *editStartX;
    TEdit *editStartY;
    TEdit *editStartDir;
    TLabel *Label1;
    TLabel *lbTotalCount;
    TLabel *Label7;
    TEdit *editMapID;
    TListBox *lbMaps;
    TButton *btnClose;
    TButton *btnChange;
	TLabel *Label8;
	TEdit *editNumSP;
	TUpDown *udNumSP;
    void __fastcall editStartXChange(TObject *Sender);
    void __fastcall editStartYChange(TObject *Sender);
    void __fastcall editStartDirChange(TObject *Sender);
    void __fastcall editStartXKeyPress(TObject *Sender, char &Key);
    void __fastcall editMapIDKeyPress(TObject *Sender, char &Key);
    void __fastcall lbMapsClick(TObject *Sender);
    void __fastcall btnCloseClick(TObject *Sender);
    void __fastcall btnChangeClick(TObject *Sender);
	void __fastcall editNumSPChange(TObject *Sender);
	void __fastcall udNumSPChanging(TObject *Sender, bool &AllowChange);
private:	// User declarations
public:		// User declarations
    __fastcall TfrmHdr(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmHdr *frmHdr;
//---------------------------------------------------------------------------
#endif
