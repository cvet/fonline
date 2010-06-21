//---------------------------------------------------------------------------


#ifndef frmObjH
#define frmObjH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <ComCtrls.hpp>
#include <Buttons.hpp>
#include "objset.h"
#include "msg.h"

extern CProSet         *pProSet;
extern CMsg            *pMsg;

//---------------------------------------------------------------------------
class TfrmObject : public TFrame
{
__published:	// IDE-managed Components
    TShape *Shape1;
    TLabel *Label1;
    TLabel *lbID;
    TLabel *Label2;
    TLabel *lbType;
    TLabel *Label3;
    TLabel *lbSubType;
    TLabel *Label4;
    TLabel *Label5;
    TLabel *lbName;
    TMemo *mmDesc;
    TListView *lwParams;
    TRadioButton *rbFlags;
    TRadioButton *rbProps;
    TPanel *imgObj;
    TLabel *Label7;
    TEdit *txtEdit;
    TSpeedButton *sbOk;
    TSpeedButton *sbCancel;
    void __fastcall lwParamsSelectItem(TObject *Sender, TListItem *Item,
          bool Selected);
    void __fastcall txtEditKeyDown(TObject *Sender, WORD &Key,
          TShiftState Shift);
    void __fastcall sbCancelClick(TObject *Sender);
    void __fastcall sbOkClick(TObject *Sender);
    void __fastcall SpeedButton1Click(TObject *Sender);
    void __fastcall rbFlagsClick(TObject *Sender);
    void __fastcall lwParamsClick(TObject *Sender);
private:	// User declarations
    OBJECT          *pSelObj;
    void        ChangeButtonsState(bool State);
    void        UpdateListView();
public:		// User declarations
    void UpdateForm(OBJECT * pObj);
    __fastcall TfrmObject(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmObject *frmObject;
//---------------------------------------------------------------------------
#endif
