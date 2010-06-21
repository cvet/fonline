//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "frmObj.h"
#include "main.h"
#include "mdi.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
//---------------------------------------------------------------------------
__fastcall TfrmObject::TfrmObject(TComponent* Owner)
    : TFrame(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TfrmObject::lwParamsSelectItem(TObject *Sender,
      TListItem *Item, bool Selected)
{
    if (!Item->SubItems->Count || !rbProps->Checked) return;
    txtEdit->Text = Item->SubItems->Strings[0];
    ChangeButtonsState(false);
}
//---------------------------------------------------------------------------
void TfrmObject::UpdateForm(OBJECT * pObj)
{
    if (!pObj)
    {
        pSelObj = NULL;
        lbID->Caption = "0";
        lbType->Caption = "none";
        lbSubType->Caption = "none";
        lbName->Caption = "none";
        mmDesc->Lines->Clear();
        lwParams->Items->Clear();
        rbProps->Enabled = false;
        rbFlags->Enabled = false;
        return;
    }

    rbProps->Enabled = true;
    rbFlags->Enabled = true;

    BYTE nType          = pObj->GetType();
    BYTE nSubType       = pObj->GetSubType();
    WORD nProtoIndex    = pObj->GetProtoIndex();
    lbID->Caption       = String(nProtoIndex);
	lbType->Caption     = _Types[nType];
    lbSubType->Caption  = SubTypes[nType][nSubType];

    DWORD nMsgID                 = pProSet->pPRO[nType][nProtoIndex].GetMsgID();
    lbName->Caption     = pMsg->GetMSG(nType, nMsgID);
    DWORD nScriptID              = pObj->GetScriptID();
//        if (nScriptID != 0xFFFFFFFF)
//            frmObj1->lbSID->Caption      = String(nScriptID);
//
//                else frmObj1->lbSID->Caption = "none";
    mmDesc->Lines->Text = pMsg->GetMSG(nType, nMsgID + 1);
    if (pObj != pSelObj)
    {
        pSelObj = pObj;
        UpdateListView();
    }

}
//---------------------------------------------------------------------------
void __fastcall TfrmObject::txtEditKeyDown(TObject *Sender, WORD &Key,
      TShiftState Shift)
{
   if ( (Key < '0' || Key > '9') && Key != 'x' && Key != 'X')
   {
        Key = 0;
        return;
   }
   if (!lwParams->SelCount) return;
   switch(Key)
   {
      case VK_RETURN:
         sbOk->Click();
         break;
      case VK_ESCAPE:
         sbCancel->Click();
         break;
   }
   ChangeButtonsState(true);
}

void TfrmObject::UpdateListView()
{
    if (rbProps->Checked)
    {
        lwParams->Columns->Clear();
        TListColumn * lc = lwParams->Columns->Add();
        lc->Caption = "Название";
        lc->Width   = 135;
        lc = lwParams->Columns->Add();
        lc->Caption = "Значение";
        lc->Width   = 72;

        lwParams->Checkboxes = false;
        lwParams->Items->BeginUpdate();
        // Очищаем.
        lwParams->Items->Clear();
        txtEdit->Enabled = true;
        TListItem  *ListItem;
        char buffer[16];
        String s;
		for (int i = 0; i <= 28; i++)
        {
            pSelObj->GetParamName(s, i);
            if (s == "") break;
            ListItem = lwParams->Items->Add();
            ListItem->Caption = s;
            ListItem->SubItems->Clear();
            sprintf(buffer, "0x%.8X", pSelObj->GetParamValue(i) );
            ListItem->SubItems->Add(String(buffer));
        }
        lwParams->Items->EndUpdate();
     } else
	 {
        lwParams->Columns->Clear();
        TListColumn * lc = lwParams->Columns->Add();
        lc->Caption = "Флаг";
        lc->Width   = 207;
        lc->Alignment = taCenter;
        lwParams->Checkboxes = true;
        txtEdit->Enabled = false;
        lwParams->Items->BeginUpdate();
        // Очищаем.
        lwParams->Items->Clear();
        lwParams->Items->EndUpdate();

        String s;
        TListItem * ListItem;
        for (int i =0; i < 32; i++)
        {
            pSelObj->GetFlagName(s, i);
            if (s == "Not used") continue;
            ListItem = lwParams->Items->Add();
            ListItem->Caption = s;
            ListItem->SubItems->Clear();
            s = String(i);
            ListItem->SubItems->Add(s);
            if (pSelObj->GetFlag(1 << i))
                ListItem->Checked = true;
        }

     }
}

//---------------------------------------------------------------------------
void TfrmObject::ChangeButtonsState(bool State)
{
   if (sbOk->Enabled == State && sbCancel->Enabled == State) return;
   sbOk->Enabled = State;
   sbCancel->Enabled = State;
}
//---------------------------------------------------------------------------

void __fastcall TfrmObject::sbCancelClick(TObject *Sender)
{
   if (lwParams->SelCount) lwParamsSelectItem(Sender, lwParams->Selected, true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmObject::sbOkClick(TObject *Sender)
{
   int Value;
   try
   {
      Value = txtEdit->Text.ToInt();
   }
   catch (Exception &exception)
   {
      Application->ShowException(&exception);
      return;
   }
   CMap *  map = frmEnv->GetCurMap();
   map->SaveChange(LM_OBJ);
   pSelObj->SetParamValue(lwParams->Selected->Index, &Value);
   UpdateForm(pSelObj);
   UpdateListView();
   frmEnv->RedrawMap(true);
   frmEnv->SetButtonSave(true);
}
//---------------------------------------------------------------------------
void __fastcall TfrmObject::SpeedButton1Click(TObject *Sender)
{
   int Value;
   try
   {
      Value = txtEdit->Text.ToInt();
   }
   catch (Exception &exception)
   {
      Application->ShowException(&exception);
      return;
   }
   pSelObj->SetParamValue(lwParams->Selected->Index, &Value);
   UpdateForm(pSelObj);
   frmEnv->RedrawMap(true);
   frmEnv->SetButtonSave(true);
}
//---------------------------------------------------------------------------


void __fastcall TfrmObject::rbFlagsClick(TObject *Sender)
{
    UpdateListView();
}
//---------------------------------------------------------------------------void __fastcall TfrmObject::lwParamsEditing(TObject *Sender,


void __fastcall TfrmObject::lwParamsClick(TObject *Sender)
{
    if (!rbFlags->Checked) return;
    if (lwParams->Checkboxes)
    {
        TListItem * Item;
        for (int j = 0; j < lwParams->Items->Count; j++)
        {
            Item = lwParams->Items->Item[j];
            int i = StrToInt(Item->SubItems->Strings[0]);
            if (Item->Checked)
            {
                pSelObj->SetFlag(1 << i);
            } else
            {
                pSelObj->ClearFlag(1 << i);
            }
        }
    }
}
//---------------------------------------------------------------------------


