//---------------------------------------------------------------------------


#ifndef frmMiniMapH
#define frmMiniMapH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------
class TfrmMMap : public TFrame
{
__published:	// IDE-managed Components
    TShape *Shape2;
    TImage *imgMiniMap;
private:	// User declarations
public:		// User declarations
    __fastcall TfrmMMap(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmMMap *frmMMap;
//---------------------------------------------------------------------------
#endif
