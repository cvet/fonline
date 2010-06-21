//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "frameHeader.h"
#include "main.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
//---------------------------------------------------------------------------
__fastcall TfrmHdr::TfrmHdr(TComponent* Owner)
    : TFrame(Owner)
{
}
//---------------------------------------------------------------------------

void __fastcall TfrmHdr::editStartXChange(TObject *Sender)
{
    AnsiString str =  ((TEdit*)Sender)->Text;
    if (str == "") str = "0";
    if (StrToInt(str) > (FO_MAP_WIDTH<<1) - 1) ((TEdit*)Sender)->Text = String((FO_MAP_WIDTH<<1) - 1);
}
//---------------------------------------------------------------------------

void __fastcall TfrmHdr::editStartYChange(TObject *Sender)
{
    AnsiString str =  ((TEdit*)Sender)->Text;
    if (str == "") str = "0";
    if (StrToInt(str) > (FO_MAP_HEIGHT << 1) - 1) ((TEdit*)Sender)->Text = String((FO_MAP_HEIGHT<<1) - 1);
}
//---------------------------------------------------------------------------

void __fastcall TfrmHdr::editStartDirChange(TObject *Sender)
{
	AnsiString str =  ((TEdit*)Sender)->Text;
	if (str == "") str = "0";
	if (StrToInt(str) > 5) ((TEdit*)Sender)->Text = "5";
}
//---------------------------------------------------------------------------
void __fastcall TfrmHdr::editStartXKeyPress(TObject *Sender, char &Key)
{
	if ((Key < '0' || Key > '9') && Key != '\b' && Key != VK_LEFT && Key != VK_RIGHT) Key = 0;
}
//---------------------------------------------------------------------------

void __fastcall TfrmHdr::editMapIDKeyPress(TObject *Sender, char &Key)
{
    if ((Key < '0' || Key > '9') && Key != 'x' && Key != 'X' && Key != '\b'
    && Key != VK_LEFT && Key != VK_RIGHT && (Key < 'A' || Key > 'F') &&
    (Key < 'a' || Key > 'f')) Key = 0;
}
//---------------------------------------------------------------------------

void __fastcall TfrmHdr::lbMapsClick(TObject *Sender)
{
    int index = lbMaps->ItemIndex;
    frmEnv->SetMap(index);
}
//---------------------------------------------------------------------------

void __fastcall TfrmHdr::btnCloseClick(TObject *Sender)
{
    if (lbMaps->Items->Count <= 1) return;
    int index = lbMaps->ItemIndex;
    frmEnv->DeleteMap(index);
    lbMaps->Items->Delete(index);
    frmEnv->SetMap(0);
}
//---------------------------------------------------------------------------
void __fastcall TfrmHdr::btnChangeClick(TObject *Sender)
{
    CMap * pMap         = frmEnv->GetCurMap();
	CHeader * pHeader   = pMap->GetHeader();
	int num             = StrToInt(editNumSP->Text); //!Cvet
	int x               = StrToInt(editStartX->Text);
	int y               = StrToInt(editStartY->Text);
	int dir             = StrToInt(editStartDir->Text);
	pHeader->SetStartPos(num,x,y,dir);

    DWORD id            = StrToInt(editMapID->Text);
    pHeader->SetMapID(id);
    frmEnv->SetButtonSave(true);
}
//---------------------------------------------------------------------------

void __fastcall TfrmHdr::editNumSPChange(TObject *Sender) //!Cvet
{
	AnsiString str =  ((TEdit*)Sender)->Text;
	if (str == "") str = "0";
	if (StrToInt(str) >= MAX_GROUPS_ON_MAP) str = IntToStr(MAX_GROUPS_ON_MAP-1);

	((TEdit*)Sender)->Text=str;

	CMap* pMap=frmEnv->GetCurMap();
	CHeader* pHeader=pMap->GetHeader();
	editStartX->Text=IntToStr(pHeader->GetStartX(StrToInt(str)));
	editStartY->Text=IntToStr(pHeader->GetStartY(StrToInt(str)));
	editStartDir->Text=IntToStr(pHeader->GetStartDir(StrToInt(str)));
}
//---------------------------------------------------------------------------

void __fastcall TfrmHdr::udNumSPChanging(TObject *Sender, bool &AllowChange) //!Cvet
{
	udNumSP->Min=0;
	udNumSP->Max=MAX_GROUPS_ON_MAP-1;
	udNumSP->Increment=1;

	if(StrToInt(editNumSP->Text)!=udNumSP->Position)
		editNumSP->Text=IntToStr(udNumSP->Position);
}
//---------------------------------------------------------------------------

