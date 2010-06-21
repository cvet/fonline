//---------------------------------------------------------------------------
#ifndef pbarH
#define pbarH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <ComCtrls.hpp>
#include <Graphics.hpp>
//---------------------------------------------------------------------------
class TfrmPBar : public TForm
{
__published:	// IDE-managed Components
        TProgressBar *pbar;
        TLabel *lblStatus;
        TLabel *lblCaption;
        TTimer *tm;
        TPanel *Panel1;
        TImage *Image1;
        TImage *Image2;
        TImage *Image3;
        TShape *Shape1;
        void __fastcall UpdatePos(TObject *Sender);
private:	// User declarations
        int *nPos;
public:		// User declarations
        __fastcall TfrmPBar(TComponent* Owner);
        void NewTask(String sCaption, String sStatus, int nStart,
                                                            int nEnd, int *Pos);
        void SetTitle(String caption)
        {
            this->Caption = caption;
        }
        void EndTask(void);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmPBar *frmPBar;
//---------------------------------------------------------------------------
#endif
