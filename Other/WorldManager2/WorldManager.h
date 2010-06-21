//---------------------------------------------------------------------------

#ifndef WorldManagerH
#define WorldManagerH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <jpeg.hpp>
#include <ComCtrls.hpp>
#include <Grids.hpp>
#include <ValEdit.hpp>
#include <vector>
//---------------------------------------------------------------------------
typedef std::vector<int> IntVec;
typedef std::vector<AnsiString> AnsiStringVec;
//---------------------------------------------------------------------------
#define GM_MAXX                 (1400)
#define GM_MAXY                 (1500)
#define GM_ZONE_LEN             (50) //должна быть кратна GM_MAXX и GM_MAXY
#define GM_MAXZONEX             (GM_MAXX/GM_ZONE_LEN)
#define GM_MAXZONEY             (GM_MAXY/GM_ZONE_LEN)
#define GM_ZONES_FOG_SIZE       ((GM_MAXZONEX/4+(GM_MAXZONEX%4?1:0))*GM_MAXZONEY)
#define GM_FOG_FULL             (0)
#define GM_FOG_SELF             (1)
#define GM_FOG_NONE             (3)
#define GM_MAX_GROUP_COUNT      (10)
#define GM_MAX_GROUPS_MULT      (5)
#define GM_MOVE_1KM_TIME        (10000.0f) //30000
#define GM_ZONE_TIME_PROC       (500)
#define GM_ZONE(x)              ((x)/GM_ZONE_LEN)
//---------------------------------------------------------------------------
#define TAB_MASKS               (0)
#define TAB_CITIES              (1)
#define TAB_MAPS                (2)
#define TAB_RELIEF              (3)
//---------------------------------------------------------------------------
class TMainForm : public TForm
{
__published:	// IDE-managed Components
	TGroupBox *GbWork;
	TGroupBox *GbGlobalMap;
	TImage *ImgMap;
	TGroupBox *GbInfo;
	TLabel *LabelCoordX;
	TLabel *LabelCoordY;
	TLabel *LabelZoneX;
	TLabel *LabelZoneY;
	TPageControl *Pages;
	TTabSheet *TabMasks;
	TTabSheet *TabCities;
	TTabSheet *TabMaps;
	TListBox *LbMasks;
	TPanel *Panel1;
	TButton *BtnLoadMasks;
	TButton *BtnSaveMasks;
	TCheckBox *CbShowProc;
	TTrackBar *TbMaskProcStep;
	TButton *BtnEditLine;
	TEdit *EditMask;
	TCheckBox *CbMaskMultiselect;
	TTabSheet *TabRelief;
	TPanel *Panel2;
	TValueListEditor *VleCityInfo;
	TCheckBox *CbShowCity;
	TCheckBox *CbShowEncaunters;
	TListBox *LbCities;
	TCheckBox *CbShowOtherCities;
	TCheckBox *CbShowLocations;
	TSplitter *Splitter1;
	TCheckBox *CbShowMeetGraph;
	TCheckBox *CbShowMeetProc;
	TLabel *LabelMeetDiv;
	TTrackBar *TbMeetDiv;
	void __fastcall ImgMapMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall ImgMapMouseUp(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void __fastcall ImgMapMouseMove(TObject *Sender, TShiftState Shift, int X, int Y);
	void __fastcall FormResize(TObject *Sender);
	void __fastcall BtnLoadMasksClick(TObject *Sender);
	void __fastcall LbMasksDblClick(TObject *Sender);
	void __fastcall FormMouseWheel(TObject *Sender, TShiftState Shift, int WheelDelta, TPoint &MousePos, bool &Handled);
	void __fastcall ImgMapMouseEnter(TObject *Sender);
	void __fastcall ImgMapMouseLeave(TObject *Sender);
	void __fastcall CbShowProcClick(TObject *Sender);
	void __fastcall PagesChange(TObject *Sender);
	void __fastcall BtnEditLineClick(TObject *Sender);
	void __fastcall BtnSaveMasksClick(TObject *Sender);
	void __fastcall LbCitiesDblClick(TObject *Sender);
	void __fastcall VleCityInfoKeyPress(TObject *Sender, char &Key);
	void __fastcall VleCityInfoSelectCell(TObject *Sender, int ACol, int ARow, bool &CanSelect);
	void __fastcall VleCityInfoValidate(TObject *Sender, int ACol, int ARow, const AnsiString KeyName,
          const AnsiString KeyValue);
	void __fastcall CbShowCityClick(TObject *Sender);
	void __fastcall TbMeetDivChange(TObject *Sender);
private:	// User declarations
	TPicture* MapF2;

	struct ZoneMask
    {
    	int Num;
        AnsiString Name;
        Graphics::TBitmap* Bmp;
        bool ToDraw;
        int GetProcent(int zx, int zy);
        void SetProcent(int zx, int zy, int proc);
        //ZoneMask(const ZoneMask& r):Bmp(r.Bmp),Name(r.Name),ToDraw(r.ToDraw){r.Bmp=NULL;}
        ZoneMask(AnsiString& name, int num, Graphics::TBitmap* bmp):Name(name),Num(num),Bmp(bmp),ToDraw(false){}
        //~ZoneMask(){if(Bmp) delete Bmp;} //Don't toutch! It is specal memory leak.
    };
typedef std::vector<ZoneMask> ZoneMaskVec;
	ZoneMaskVec Masks;
    void LoadMasks(AnsiString path);
    int GetMaskNum(AnsiString& line, AnsiString& name);
    ZoneMask* GetMask(int num);

    struct City
    {
    	int Number;
    	AnsiString Name;
    	int StoreOnMove;
        int Sneak;
        int Size;
        int EntranceCount;
        int Encaunter;
        int AutoGarbage;
        int MeetChance;
        int MobGenerate;
        int EnteredProto;
        int EnteredCopy;
        int EnteredGroups;
        int EnteredPlayers;
        AnsiStringVec Maps;
        IntVec Zones;

        bool IsCity(){return !Encaunter;}
        bool IsEncaunter(){return Encaunter && MeetChance && Zones.size();}
    };
typedef std::vector<City> CityVec;
typedef std::vector<City*> CityPtrVec;
	CityVec Cities;
    void LoadCities(AnsiString path);
    void RefreshCitiesList();
    City* GetCity(AnsiString ident);
    City* GetCity(int num);
    void FillCityInfo(City* city);
    void EditCityInfo(AnsiString key, AnsiString value, bool edit);

    struct ZoneMeetStat
    {
        double Procent;
    	AnsiStringVec FullData;
        void Clear(){Procent=0.0;FullData.clear();}
    };
    AnsiStringVec MeetInfo;
    ZoneMeetStat MeetStatistics[GM_MAXZONEX][GM_MAXZONEY];
    int MeetX,MeetY;
    void UpdateEncauntersMeet(City* base);

    struct WorldCity
    {
    	int CityNum;
    	int ZoneX,ZoneY;
    };
typedef std::vector<WorldCity> WorldCityVec;
	WorldCityVec World;
    void LoadWorld(AnsiString path);

    bool IsPressed;
	int PressX,PressY;
	int CoordX,CoordY;
	int MapX,MapY;
	void DrawMap();
public:		// User declarations
	__fastcall TMainForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TMainForm *MainForm;
//---------------------------------------------------------------------------
#endif
