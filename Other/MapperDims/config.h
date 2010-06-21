//---------------------------------------------------------------------------

#ifndef configH
#define configH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>
#include "utilites.h"
extern CUtilites *pUtil;
//---------------------------------------------------------------------------
class TfrmConfig : public TForm
{
__published:	// IDE-managed Components
        TGroupBox *gb1;
        TButton *btnBrowse;
        TEdit *edGameDir;
        TButton *btnOK;
        TButton *btnCancel;
        TGroupBox *gb2;
        TRadioButton *rb1;
        TRadioButton *rb2;
        TRadioButton *rb3;
        TListView *lvDAT;
        TLabel *lbl1;
        TEdit *edDataDir;
        TButton *btnBrowse2;
        TLabel *lblMapperGray;
        TLabel *lblMapperSilver;
    TGroupBox *GroupBox1;
    TEdit *txtTmplLocation;
    TButton *Button1;
        void __fastcall btnBrowseClick(TObject *Sender);
        void __fastcall btnBrowse2Click(TObject *Sender);
        void __fastcall btnOKClick(TObject *Sender);
    void __fastcall Button1Click(TObject *Sender);
public:		// User declarations
        bool HasChanged;
        void ShowDialog(void);
        void InitDAT(void);
        __fastcall TfrmConfig(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmConfig *frmConfig;
//---------------------------------------------------------------------------
#endif
