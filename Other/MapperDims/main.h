//---------------------------------------------------------------------------
#ifndef mainH
#define mainH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "utilites.h"
#include "log.h"
#include "map.h"
#include "tileset.h"
#include "frmset.h"
#include "lists.h"
#include "msg.h"
#include "proset.h"
#include "objset.h"
#include "pal.h"
#include <ExtCtrls.hpp>
#include <Graphics.hpp>
#include <ComCtrls.hpp>
#include <Menus.hpp>
#include <Buttons.hpp>
#include <ImgList.hpp>
#include <ddraw.h>
#include "frameHeader.h"
#include "frmInven.h"
#include "frmObj.h"
#include "frmMiniMap.h"
#include "template.h"
#include <vector>

class CFrmSet;
class CProSet;
class CObjSet;
class CTileSet;

extern CLog            * pLog;
extern CUtilites       * pUtil;
extern CPal            * pPal;
extern CListFiles      * pLstFiles;
extern CFrmSet         * pFrmSet;
extern CProSet         * pProSet;
extern CMsg            * pMsg;

//---------------------------------------------------------------------------
class TfrmEnv : public TForm
{
friend class TfrmObject;
friend class TfrmTmpl;
__published:	// IDE-managed Components
        TShape *shp;
        TPanel *imgObj;
        TTabControl *tabc;
        TShape *Shape3;
        TShape *Shape4;
        TLabel *Label4;
        TLabel *lblInfo;
        TShape *Shape5;
        TScrollBar *sb;
        TPopupMenu *popupMap;
        TMenuItem *Mapperv09byDims1;
        TMenuItem *N1;
        TImageList *ImageList1;
        TMenuItem *piProperties;
        TMenuItem *piDelete;
        TMenuItem *N2;
        TStatusBar *sbar;
        TShape *Shape7;
        TLabel *Label7;
        TfrmHdr *frmHdr1;
        TShape *Shape9;
        TLabel *Label1;
        TShape *Shape1;
        TLabel *Label2;
        TLabel *Label3;
        TShape *Shape8;
        TSpeedButton *btnWorld;
        TSpeedButton *btnLocal;
        TfrmInv *frmInv1;
        TfrmObject *frmObj1;
        TfrmMMap *frmMMap1;
        TEdit *Edit1;
	TPanel *imgMap;
        void __fastcall FormDestroy(TObject *Sender);
        void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
        void __fastcall imgMapMouseDown(TObject *Sender,
          TMouseButton Button, TShiftState Shift, int X, int Y);
        void __fastcall imgMapMouseUp(TObject *Sender, TMouseButton Button,
          TShiftState Shift, int X, int Y);
        void __fastcall imgMapMouseMove(TObject *Sender, TShiftState Shift,
          int X, int Y);
        void __fastcall imgMiniMapMouseDown(TObject *Sender,
          TMouseButton Button, TShiftState Shift, int X, int Y);
        void __fastcall imgMiniMapMouseMove(TObject *Sender,
          TShiftState Shift, int X, int Y);
        void __fastcall imgMiniMapMouseUp(TObject *Sender,
          TMouseButton Button, TShiftState Shift, int X, int Y);
        void __fastcall imgMapClick(TObject *Sender);
        void __fastcall popupMapPopup(TObject *Sender);
        void __fastcall piPropertiesClick(TObject *Sender);
        void __fastcall tabcChange(TObject *Sender);
        void __fastcall imgObjMouseDown(TObject *Sender,
          TMouseButton Button, TShiftState Shift, int X, int Y);
        void __fastcall btnWorldClick(TObject *Sender);
        void __fastcall btnLocalClick(TObject *Sender);
        void __fastcall sbScroll(TObject *Sender, TScrollCode ScrollCode,
          int &ScrollPos);
        void __fastcall piDeleteClick(TObject *Sender);
        void __fastcall FormKeyDown(TObject *Sender, WORD &Key,
          TShiftState Shift);
        void __fastcall editStartXKeyPress(TObject *Sender, char &Key);
        void __fastcall Shape9MouseDown(TObject *Sender, TMouseButton Button,
                        TShiftState Shift, int X, int Y);
        void __fastcall Shape7MouseDown(TObject *Sender, TMouseButton Button,
                        TShiftState Shift, int X, int Y);
        void __fastcall frmInv1btnChangeClick(TObject *Sender);
        void __fastcall frmInv1imgInvMouseDown(TObject *Sender,
                        TMouseButton Button, TShiftState Shift, int X, int Y);
        void __fastcall frmInv1btnRemoveClick(TObject *Sender);
        void __fastcall frmInv1btnAddClick(TObject *Sender);
        void __fastcall frmInv1sbINVChange(TObject *Sender);
        void __fastcall Shape1MouseDown(TObject *Sender, TMouseButton Button,
                        TShiftState Shift, int X, int Y);
        void __fastcall Shape4MouseDown(TObject *Sender, TMouseButton Button,
                        TShiftState Shift, int X, int Y);
        void __fastcall Shape4MouseMove(TObject *Sender, TShiftState Shift,
                        int X, int Y);
    void __fastcall FormMouseMove(TObject *Sender, TShiftState Shift,
          int X, int Y);
    void __fastcall Shape2MouseDown(TObject *Sender, TMouseButton Button,
          TShiftState Shift, int X, int Y);
private:	// User declarations
        int             ObjsInPanel;

        bool            mouseBLeft, mouseBLeft2;
        bool            mouseBRight, mouseBRight2;
        int             downX, downY, upX, upY; //for Mouse at Map
        int             downHexX, downHexY;
        int             downTileX, downTileY;
        int             cursorX, cursorY;
        int             prevX, prevY, have_sel; // for selection-frame drawing
        int             TileX, TileY;
        int             HexX, HexY;
        int             OldX, OldY; // for Locator
        int             OldShowObjX, OldShowObjY; // for indicating obj
        HANDLE          h_map;
        Graphics::TBitmap *pBMPlocator;
        Graphics::TBitmap *pBMPHex;
		TWndMethod OldMapWndProc;
		TWndMethod OldNavWndProc;
		TWndMethod OldInvWndProc;
        TWndMethod OldObjWndProc;
        struct OBJNAVIGATOR
        {
           BYTE         nObjType;
           DWORD        nSelID;
           DWORD        nNavID[12];
           BYTE         nShowMode;
           int          nCount;
           int          nMaxID;
        } Navigator;
        struct OBJINVENTORY
        {
           OBJECT       *pObj;
           OBJECT       *pChildObj[NAV_MAX_COUNT];
           int           nItemNum;
           int           nInvStartItem;
        } Inventory;
        void __fastcall NewMapWndProc(TMessage &Msg);
        void __fastcall NewNavWndProc(TMessage &Msg);
        void __fastcall NewInvWndProc(TMessage &Msg);
        void __fastcall NewObjWndProc(TMessage &Msg);

        void DrawMiniMap(void);
        void RedrawFloor(float aspect);
        void RedrawRoof(float aspect);
        void RedrawObjects(CObjSet * pObjSet, float aspect);
        void RedrawBlockers(float aspect);
        void RedrawHex(float aspect);
        void RedrawLocator(void);
        void SelectObjXY(int X, int Y);
        bool SelectObjRegion(int X, int Y, int X2, int Y2);
        void SelectionChanged(void);
        bool SelectTileRegion(int TileMode, int X, int Y, int X2, int Y2);
        void LogSelected(void);
        void UpdatePanels(void);

        void CopySelectedFloor(void);
        void CopySelectedRoof(void);
        void CopySelectedObjects(void);
        void PasteSelectedFloor(void);
        void PasteSelectedRoof(void);

        void UpdateObjPanel(void);
        void RedrawObjPanel(OBJECT * pObj);

        CHeader         *pBufFloorHeader;
        CTileSet        *pBufFloor;
        CHeader         *pBufRoofHeader;
        CTileSet        *pBufRoof;
        CHeader         *pBufObjHeader;
        CObjSet      *pBufObjSet;

        DWORD           TmpWorldX;
        DWORD           TmpWorldY;

        CObjSet	        *pOS;
        CTileSet        *pTileSet;
        CMap            *pMap;          // Тек. карта
        vector<CMap*>   MapList;
public:
        __fastcall TfrmEnv(TComponent* Owner);

        void            ChangeMode(BYTE new_mode);
        void            AddMap(CMap * map, bool set);
        void            DeleteMap(int index);
        CMap           *GetCurMap() const {return pMap;};
        void            SetMap(int index);

        int             tile_sel;
        // User declarations
        int             SelectMode;

        int             WorldX, OldWorldX;
        int             WorldY, OldWorldY;
        bool            bShowObj[15];
        LPDIRECTDRAWSURFACE7 dds, dds2Map, dds2Nav, dds2Inv, dds2Obj;
        LPDIRECTDRAWCLIPPER ddc, ddcMap, ddcNav, ddcInv, ddcObj;
        TStatusPanel *panelHEX;
        TStatusPanel *panelTILE;
        TStatusPanel *panelObjCount;
        TStatusPanel *panelObjSelected;
        TStatusPanel *panelMSG;

        void ClearSelection(void);
        void ClearFloorSelection(bool NeedRedrawMap);
        void ClearRoofSelection(bool NeedRedrawMap);
        void ClearObjSelection(bool NeedRedrawMap);
        void MoveSelectedObjects(int offsetHexX, int offsetHexY);
        void RotateSelectedObjects(int direction);
        void SetButtonSave(bool State);
        void RedrawMap(bool StaticRedraw);
        void PrepareNavigator(BYTE nObjType);
        void RedrawNavigator(void);
        void ResetInventoryInfo(void);
        void RedrawInventory(void);

       	void TransBltFrm(CFrame* frm, TControl*, short nDir, short nFrame,
                                       int x, int y, LPDIRECTDRAWSURFACE7 dds2, float aspect);
	    void TransBltMask(CFrame* frm, TControl*, short nDir, short nFrame,
                 int x, int y, LPDIRECTDRAWSURFACE7 dds2, int width, int height);
        void TransBltTileRegion(LPDIRECTDRAWSURFACE7 dds2,
                                int TileX1, int TileY1, int TileX2, int TileY2,
                                TColor color, float aspect);
	    void RepaintDDrawWindow(TWinControl *win, LPDIRECTDRAWSURFACE7 dds,
                          LPDIRECTDRAWSURFACE7 dds2, LPDIRECTDRAWCLIPPER ddc);
	    void AttachDDraw(TControl *win, LPDIRECTDRAWSURFACE7 *dds, int primary);

        void CreateNewMap(void);
        void LoadMapFromFile(void);
//        void SetTemplate(CTemplate * pTmpl);
//        void UnsetTemplate();

        float fAspect;

        
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmEnv *frmEnv;
//---------------------------------------------------------------------------
#endif
