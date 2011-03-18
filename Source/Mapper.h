#ifndef __MAPPER__
#define __MAPPER__

#include "Common.h"
#include "Keyboard.h"
#include "SpriteManager.h"
#include "HexManager.h"
#include "ItemManager.h"
#include "Item.h"
#include "CritterCl.h"
#include "Text.h"
#include "ConstantsManager.h"
#include "CritterData.h"
#include "CritterManager.h"
#include "ResourceManager.h"
#include "CritterType.h"
#include "Script.h"
#include "ScriptPragmas.h"

// Fonts
#define FONT_FO                 (0)
#define FONT_NUM                (1)
#define FONT_BIG_NUM            (2)
#define FONT_SAND_NUM           (3)
#define FONT_SPECIAL            (4)
#define FONT_DEFAULT            (5)
#define FONT_THIN               (6)
#define FONT_FAT                (7)
#define FONT_BIG                (8)

typedef vector<CritData*> CritDataVec;

class FOMapper
{
public:
	static FOMapper* Self;
	int FPS;
	HWND Wnd;
	LPDIRECTINPUT8 DInput;
	LPDIRECTINPUTDEVICE8 Keyboard;
	LPDIRECTINPUTDEVICE8 Mouse;
	FileManager FileMngr;
	HexManager HexMngr;
	IniParser IfaceIni;
	bool IsMapperStarted;

	FOMapper();
	bool Init(HWND wnd);
	int InitIface();
	bool IfaceLoadRect(INTRECT& comp, const char* name);
	void Finish();
	bool InitDI();
	void ParseKeyboard();
	void ParseMouse();
	void MainLoop();
	void RefreshTiles(int tab);

#define DRAW_CR_INFO_MAX      (3)
	int DrawCrExtInfo;

	// Game color
	DWORD DayTime;
	void ChangeGameTime();

	// MSG File
	LanguagePack CurLang;
	FOMsg* MsgText,*MsgDlg,*MsgItem,*MsgGame,*MsgGM,*MsgCombat,*MsgQuest,*MsgHolo,*MsgCraft;

	// Map text
	struct MapText 
	{
		WORD HexX,HexY;
		DWORD StartTick,Tick;
		string Text;
		DWORD Color;
		bool Fade;
		INTRECT Rect;
		INTRECT EndRect;
		bool operator==(const MapText& r){return HexX==r.HexX && HexY==r.HexY;}
	};
typedef vector<MapText> MapTextVec;
typedef vector<MapText>::iterator MapTextVecIt;
	MapTextVec GameMapTexts;

	// Animations
	struct IfaceAnim
	{
		AnyFrames* Frames;
		WORD Flags;
		DWORD CurSpr;
		DWORD LastTick;
		int ResType;

		IfaceAnim(AnyFrames* frm, int res_type):Frames(frm),Flags(0),CurSpr(0),LastTick(Timer::FastTick()),ResType(res_type){}
	};
	typedef vector<IfaceAnim*> IfaceAnimVec;
	typedef vector<IfaceAnim*>::iterator IfaceAnimVecIt;

#define ANIMRUN_TO_END         (0x0001)
#define ANIMRUN_FROM_END       (0x0002)
#define ANIMRUN_CYCLE          (0x0004)
#define ANIMRUN_STOP           (0x0008)
#define ANIMRUN_SET_FRM(frm)   ((DWORD(BYTE((frm)+1)))<<16)

	IfaceAnimVec Animations;

	DWORD AnimLoad(DWORD name_hash, BYTE dir, int res_type);
	DWORD AnimLoad(const char* fname, int path_type, int res_type);
	DWORD AnimGetCurSpr(DWORD anim_id);
	DWORD AnimGetCurSprCnt(DWORD anim_id);
	DWORD AnimGetSprCount(DWORD anim_id);
	AnyFrames* AnimGetFrames(DWORD anim_id);
	void AnimRun(DWORD anim_id, DWORD flags);
	void AnimProcess();
	void AnimFree(int res_type);

	// Cursor
	int CurMode;
#define CUR_MODE_DEF		(0)
#define CUR_MODE_MOVE		(1)
#define CUR_MODE_DRAW		(2)
#define MAX_CUR_MODES		(3)

	AnyFrames* CurPDef,*CurPHand;

	void CurDraw();
	void CurRMouseUp();
	void CurMMouseDown();

	bool GetMouseHex(WORD& hx, WORD& hy);
	bool IsCurInRect(INTRECT& rect, int ax, int ay){return (GameOpt.MouseX>=rect[0]+ax && GameOpt.MouseY>=rect[1]+ay && GameOpt.MouseX<=rect[2]+ax && GameOpt.MouseY<=rect[3]+ay);}
	bool IsCurInRect(INTRECT& rect){return (GameOpt.MouseX>=rect[0] && GameOpt.MouseY>=rect[1] && GameOpt.MouseX<=rect[2] && GameOpt.MouseY<=rect[3]);}
	bool IsCurInRectNoTransp(DWORD spr_id,INTRECT& rect, int ax, int ay){return IsCurInRect(rect,ax,ay) && SprMngr.IsPixNoTransp(spr_id,GameOpt.MouseX-rect.L-ax,GameOpt.MouseY-rect.T-ay,false);}

	int IntMode;
#define INT_MODE_CUSTOM0    (0)
#define INT_MODE_CUSTOM1    (1)
#define INT_MODE_CUSTOM2    (2)
#define INT_MODE_CUSTOM3    (3)
#define INT_MODE_CUSTOM4    (4)
#define INT_MODE_CUSTOM5    (5)
#define INT_MODE_CUSTOM6    (6)
#define INT_MODE_CUSTOM7    (7)
#define INT_MODE_CUSTOM8    (8)
#define INT_MODE_CUSTOM9    (9)
#define INT_MODE_ITEM       (10)
#define INT_MODE_TILE       (11)
#define INT_MODE_CRIT       (12)
#define INT_MODE_FAST       (13)
#define INT_MODE_IGNORE     (14)
#define INT_MODE_INCONT     (15)
#define INT_MODE_MESS       (16)
#define INT_MODE_LIST       (17)
#define INT_MODE_COUNT      (18)
#define TAB_COUNT           (15)

	int IntHold;
#define INT_NONE            (0)
#define INT_BUTTON          (1)
#define INT_MAIN            (2)
#define INT_SELECT          (3)
#define INT_OBJECT          (4)
#define INT_SUB_TAB         (5)

	AnyFrames* IntMainPic,*IntPTab,*IntPSelect,*IntPShow;
	int IntX,IntY;
	int IntVectX,IntVectY;
	WORD SelectHX1,SelectHY1,SelectHX2,SelectHY2;
	int SelectX,SelectY;

#define SELECT_TYPE_OLD     (0)
#define SELECT_TYPE_NEW     (1)
	int SelectType;

	bool IntVisible,IntFix;

	INTRECT IntWMain;
	INTRECT IntWWork,IntWHint;

	INTRECT IntBCust[10],IntBItem,IntBTile,IntBCrit,IntBFast,IntBIgnore,IntBInCont,IntBMess,IntBList,
		IntBScrBack,IntBScrBackFst,IntBScrFront,IntBScrFrontFst;

	INTRECT IntBShowItem,IntBShowScen,IntBShowWall,IntBShowCrit,IntBShowTile,IntBShowRoof,IntBShowFast;

	void IntDraw();
	void IntLMouseDown();
	void IntLMouseUp();
	void IntMouseMove();
	void IntSetMode(int mode);

	// Maps
	ProtoMapPtrVec LoadedProtoMaps;
	ProtoMap* CurProtoMap;

	// Tabs
#define DEFAULT_SUB_TAB     "000 - all"
	struct SubTab
	{
		ProtoItemVec ItemProtos;
		CritDataVec NpcProtos;
		StrVec TileNames;
		DwordVec TileHashes;
		int Index,Scroll;
		SubTab():Index(0),Scroll(0){}
	};
	typedef map<string,SubTab> SubTabMap;
	typedef map<string,SubTab>::iterator SubTabMapIt;

	struct TileTab
	{
		StrVec TileDirs;
		BoolVec TileSubDirs;
	};

	SubTabMap Tabs[TAB_COUNT];
	SubTab* TabsActive[TAB_COUNT];
	TileTab TabsTiles[TAB_COUNT];
	string TabsName[INT_MODE_COUNT];
	int TabsScroll[INT_MODE_COUNT];

	bool SubTabsActive;
	int SubTabsActiveTab;
	AnyFrames* SubTabsPic;
	INTRECT SubTabsRect;
	int SubTabsX,SubTabsY;

	// Prototypes
	ProtoItemVec* CurItemProtos;
	DwordVec* CurTileHashes;
	StrVec* CurTileNames;
	CritDataVec* CurNpcProtos;
	int NpcDir;
	int* CurProtoScroll;
	int ProtoWidth;
	int ProtosOnScreen;
	int TabIndex[INT_MODE_COUNT];
	int InContScroll;
	int ListScroll;
	MapObject* InContObject;
	bool DrawRoof;
	int TileLayer;

	int GetTabIndex();
	void SetTabIndex(int index);
	void RefreshCurProtos();
	bool IsObjectMode(){return CurItemProtos && CurProtoScroll;}
	bool IsTileMode(){return CurTileHashes && CurTileNames && CurProtoScroll;}
	bool IsCritMode(){return CurNpcProtos && CurProtoScroll;}

	// Select
	INTRECT IntBSelectItem,IntBSelectScen,IntBSelectWall,IntBSelectCrit,IntBSelectTile,IntBSelectRoof;
	bool IsSelectItem,IsSelectScen,IsSelectWall,IsSelectCrit,IsSelectTile,IsSelectRoof;

	// Select Map Object
	struct SelMapObj 
	{
		MapObject* MapObj;
		ItemHex* MapItem;
		CritterCl* MapNpc;
		MapObjectPtrVec Childs;

		SelMapObj(MapObject* mobj, ItemHex* itm):MapObj(mobj),MapItem(itm),MapNpc(NULL){}
		SelMapObj(MapObject* mobj, CritterCl* npc):MapObj(mobj),MapItem(NULL),MapNpc(npc){}
		SelMapObj(const SelMapObj& r){MapObj=r.MapObj;MapItem=r.MapItem;MapNpc=r.MapNpc;Childs=r.Childs;}
		SelMapObj& operator=(const SelMapObj& r){MapObj=r.MapObj;MapItem=r.MapItem;MapNpc=r.MapNpc;Childs=r.Childs; return *this;}
		bool operator==(const MapObject* r){return MapObj==r;}
		bool IsItem(){return MapItem!=NULL;}
		bool IsNpc(){return MapNpc!=NULL;}
		bool IsContainer(){return IsNpc() || (IsItem() && MapItem->Proto->Type==ITEM_TYPE_CONTAINER);}
	};
typedef vector<SelMapObj> SelMapProtoItemVec;
	SelMapProtoItemVec SelectedObj;

	// Select Tile, Roof
	struct SelMapTile
	{
		WORD HexX,HexY;
		bool IsRoof;

		SelMapTile(WORD hx, WORD hy, bool is_roof):HexX(hx),HexY(hy),IsRoof(is_roof){}
		SelMapTile(const SelMapTile& r){memcpy(this,&r,sizeof(SelMapTile));}
		SelMapTile& operator=(const SelMapTile& r){memcpy(this,&r,sizeof(SelMapTile)); return *this;}
	};
typedef vector<SelMapTile> SelMapTileVec;
	SelMapTileVec SelectedTile;

	// Select methods
	MapObject* FindMapObject(ProtoMap& pmap, WORD hx, WORD hy, BYTE mobj_type, WORD pid, DWORD skip);
	void FindMapObjects(ProtoMap& pmap, WORD hx, WORD hy, DWORD radius, BYTE mobj_type, WORD pid, MapObjectPtrVec& objects);
	MapObject* FindMapObject(WORD hx, WORD hy, BYTE mobj_type, WORD pid, bool skip_selected);
	void UpdateMapObject(MapObject* mobj);
	void MoveMapObject(MapObject* mobj, WORD hx, WORD hy);
	void DeleteMapObject(MapObject* mobj);
	void SelectClear();
	void SelectAddItem(ItemHex* item);
	void SelectAddCrit(CritterCl* npc);
	void SelectAddTile(WORD hx, WORD hy, bool is_roof);
	void SelectAdd(MapObject* mobj);
	void SelectErase(MapObject* mobj);
	void SelectAll();
	void SelectMove(int offs_hx, int offs_hy, int offs_x, int offs_y);
	void SelectDelete();

	// Parse new
	DWORD AnyId;
	int DefaultCritterParam[MAPOBJ_CRITTER_PARAMS];

	void ParseProto(WORD pid, WORD hx, WORD hy, bool in_cont);
	void ParseTile(DWORD name_hash, WORD hx, WORD hy, short ox, short oy, BYTE layer, bool is_roof);
	void ParseNpc(WORD pid, WORD hx, WORD hy);
	MapObject* ParseMapObj(MapObject* mobj);

	// Buffer
	struct TileBuf
	{
		DWORD NameHash;
		WORD HexX,HexY;
		short OffsX,OffsY;
		BYTE Layer;
		bool IsRoof;
	};
	typedef vector<TileBuf> TileBufVec;

	MapObjectPtrVec MapObjBuffer;
	TileBufVec TilesBuffer;

	void BufferCopy();
	void BufferCut();
	void BufferPaste(int hx, int hy);

	// Object
#define DRAW_NEXT_HEIGHT			(12)

	AnyFrames* ObjWMainPic,*ObjPBToAllDn;
	INTRECT ObjWMain,ObjWWork,ObjBToAll;
	int ObjX,ObjY;
	int ItemVectX,ItemVectY;
	int ObjCurLine;
	bool ObjVisible,ObjFix;
	bool ObjToAll;

	void ObjDraw();
	void ObjKeyDown(BYTE dik);
	void ObjKeyDownA(MapObject* o, BYTE dik);

	// Console
	AnyFrames* ConsolePic;
	int ConsolePicX,ConsolePicY,ConsoleTextX,ConsoleTextY;
	bool ConsoleEdit;
	char ConsoleStr[MAX_NET_TEXT+1];
	int ConsoleCur;

	vector<string> ConsoleHistory;
	int ConsoleHistoryCur;

#define CONSOLE_KEY_TICK			500
#define CONSOLE_MAX_ACCELERATE		460
	int ConsoleLastKey;
	DWORD ConsoleKeyTick;
	int ConsoleAccelerate;

	void ConsoleDraw();
	void ConsoleKeyDown(BYTE dik);
	void ConsoleKeyUp(BYTE dik);
	void ConsoleProcess();
	void ParseCommand(const char* cmd);

	// Mess box
	struct MessBoxMessage
	{
		int Type;
		string Mess;
		string Time;

		MessBoxMessage(int type, const char* mess, const char* time):Type(type),Mess(mess),Time(time){}
		MessBoxMessage(const MessBoxMessage& r){Type=r.Type;Mess=r.Mess;Time=r.Time;}
		MessBoxMessage& operator=(const MessBoxMessage& r){Type=r.Type;Mess=r.Mess;Time=r.Time;return *this;}
	};
	typedef vector<MessBoxMessage> MessBoxMessageVec;

	MessBoxMessageVec MessBox;
	string MessBoxCurText;
	int MessBoxScroll;

	void MessBoxGenerate();
	void AddMess(const char* message_text);
	void AddMessFormat(const char* message_text, ...);
	void MessBoxDraw();
	bool SaveLogFile();

	// Scripts
	static bool SpritesCanDraw;
	void InitScriptSystem();
	void FinishScriptSystem();
	void RunStartScript();
	void DrawIfaceLayer(DWORD layer);

	struct SScriptFunc
	{
		static CScriptString* MapperObject_get_ScriptName(MapObject& mobj);
		static void MapperObject_set_ScriptName(MapObject& mobj, CScriptString* str);
		static CScriptString* MapperObject_get_FuncName(MapObject& mobj);
		static void MapperObject_set_FuncName(MapObject& mobj, CScriptString* str);
		static BYTE MapperObject_get_Critter_Cond(MapObject& mobj);
		static void MapperObject_set_Critter_Cond(MapObject& mobj, BYTE value);
		static CScriptString* MapperObject_get_PicMap(MapObject& mobj);
		static void MapperObject_set_PicMap(MapObject& mobj, CScriptString* str);
		static CScriptString* MapperObject_get_PicInv(MapObject& mobj);
		static void MapperObject_set_PicInv(MapObject& mobj, CScriptString* str);
		static void MapperObject_Update(MapObject& mobj);
		static MapObject* MapperObject_AddChild(MapObject& mobj, WORD pid);
		static DWORD MapperObject_GetChilds(MapObject& mobj, CScriptArray* objects);
		static void MapperObject_MoveToHex(MapObject& mobj, WORD hx, WORD hy);
		static void MapperObject_MoveToHexOffset(MapObject& mobj, int x, int y);
		static void MapperObject_MoveToDir(MapObject& mobj, BYTE dir);

		static MapObject* MapperMap_AddObject(ProtoMap& pmap, WORD hx, WORD hy, int mobj_type, WORD pid);
		static MapObject* MapperMap_GetObject(ProtoMap& pmap, WORD hx, WORD hy, int mobj_type, WORD pid, DWORD skip);
		static DWORD MapperMap_GetObjects(ProtoMap& pmap, WORD hx, WORD hy, DWORD radius, int mobj_type, WORD pid, CScriptArray* objects);
		static void MapperMap_UpdateObjects(ProtoMap& pmap);
		static void MapperMap_Resize(ProtoMap& pmap, WORD width, WORD height);
		static DWORD MapperMap_GetTilesCount(ProtoMap& pmap, WORD hx, WORD hy, bool roof);
		static void MapperMap_DeleteTile(ProtoMap& pmap, WORD hx, WORD hy, bool roof, DWORD index);
		static DWORD MapperMap_GetTileHash(ProtoMap& pmap, WORD hx, WORD hy, bool roof, DWORD index);
		static void MapperMap_AddTileHash(ProtoMap& pmap, WORD hx, WORD hy, int ox, int oy, int layer, bool roof, DWORD pic_hash);
		static CScriptString* MapperMap_GetTileName(ProtoMap& pmap, WORD hx, WORD hy, bool roof, DWORD index);
		static void MapperMap_AddTileName(ProtoMap& pmap, WORD hx, WORD hy, int ox, int oy, int layer, bool roof, CScriptString* pic_name);
		static DWORD MapperMap_GetDayTime(ProtoMap& pmap, DWORD day_part);
		static void MapperMap_SetDayTime(ProtoMap& pmap, DWORD day_part, DWORD time);
		static void MapperMap_GetDayColor(ProtoMap& pmap, DWORD day_part, BYTE& r, BYTE& g, BYTE& b);
		static void MapperMap_SetDayColor(ProtoMap& pmap, DWORD day_part, BYTE r, BYTE g, BYTE b);
		static CScriptString* MapperMap_get_ScriptModule(ProtoMap& pmap);
		static void MapperMap_set_ScriptModule(ProtoMap& pmap, CScriptString* str);
		static CScriptString* MapperMap_get_ScriptFunc(ProtoMap& pmap);
		static void MapperMap_set_ScriptFunc(ProtoMap& pmap, CScriptString* str);

		static void Global_SetDefaultCritterParam(DWORD index, int param);
		static ProtoMap* Global_LoadMap(CScriptString& file_name, int path_type);
		static void Global_UnloadMap(ProtoMap* pmap);
		static bool Global_SaveMap(ProtoMap* pmap, CScriptString& file_name, int path_type);
		static bool Global_ShowMap(ProtoMap* pmap);
		static int Global_GetLoadedMaps(CScriptArray* maps);
		static DWORD Global_GetMapFileNames(CScriptString* dir, CScriptArray* names);
		static void Global_DeleteObject(MapObject* mobj);
		static void Global_DeleteObjects(CScriptArray& objects);
		static void Global_SelectObject(MapObject* mobj, bool set);
		static void Global_SelectObjects(CScriptArray& objects, bool set);
		static MapObject* Global_GetSelectedObject();
		static DWORD Global_GetSelectedObjects(CScriptArray* objects);

		static DWORD Global_TabGetTileDirs(int tab, CScriptArray* dir_names, CScriptArray* include_subdirs);
		static DWORD Global_TabGetItemPids(int tab, CScriptString* sub_tab, CScriptArray* item_pids);
		static DWORD Global_TabGetCritterPids(int tab, CScriptString* sub_tab, CScriptArray* critter_pids);
		static void Global_TabSetTileDirs(int tab, CScriptArray* dir_names, CScriptArray* include_subdirs);
		static void Global_TabSetItemPids(int tab, CScriptString* sub_tab, CScriptArray* item_pids);
		static void Global_TabSetCritterPids(int tab, CScriptString* sub_tab, CScriptArray* critter_pids);
		static void Global_TabDelete(int tab);
		static void Global_TabSelect(int tab, CScriptString* sub_tab);
		static void Global_TabSetName(int tab, CScriptString* tab_name);

		static ProtoItem* Global_GetProtoItem(WORD proto_id);
		static void Global_MoveScreen(WORD hx, WORD hy, DWORD speed);
		static void Global_MoveHexByDir(WORD& hx, WORD& hy, BYTE dir, DWORD steps);
		static CScriptString* Global_GetIfaceIniStr(CScriptString& key);
		static DWORD Global_GetTick(){return Timer::FastTick();}
		static DWORD Global_GetAngelScriptProperty(int property);
		static bool Global_SetAngelScriptProperty(int property, DWORD value);
		static DWORD Global_GetStrHash(CScriptString* str);
		static bool Global_LoadDataFile(CScriptString& dat_name);
		static int Global_GetConstantValue(int const_collection, CScriptString* name);
		static CScriptString* Global_GetConstantName(int const_collection, int value);
		static void Global_AddConstant(int const_collection, CScriptString* name, int value);
		static bool Global_LoadConstants(int const_collection, CScriptString* file_name, int path_type);
		static int Global_GetKeybLang(){return Keyb::Lang;}

		static CScriptString* Global_GetLastError();
		static void Global_Log(CScriptString& text);
		static bool Global_StrToInt(CScriptString& text, int& result);
		static void Global_Message(CScriptString& msg);
		static void Global_MessageMsg(int text_msg, DWORD str_num);
		static void Global_MapMessage(CScriptString& text, WORD hx, WORD hy, DWORD ms, DWORD color, bool fade, int ox, int oy);
		static CScriptString* Global_GetMsgStr(int text_msg, DWORD str_num);
		static CScriptString* Global_GetMsgStrSkip(int text_msg, DWORD str_num, DWORD skip_count);
		static DWORD Global_GetMsgStrNumUpper(int text_msg, DWORD str_num);
		static DWORD Global_GetMsgStrNumLower(int text_msg, DWORD str_num);
		static DWORD Global_GetMsgStrCount(int text_msg, DWORD str_num);
		static bool Global_IsMsgStr(int text_msg, DWORD str_num);
		static CScriptString* Global_ReplaceTextStr(CScriptString& text, CScriptString& replace, CScriptString& str);
		static CScriptString* Global_ReplaceTextInt(CScriptString& text, CScriptString& replace, int i);		

		static DWORD Global_GetDistantion(WORD hex_x1, WORD hex_y1, WORD hex_x2, WORD hex_y2);
		static BYTE Global_GetDirection(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy);
		static BYTE Global_GetOffsetDir(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, float offset);
		static void Global_GetHexInPath(WORD from_hx, WORD from_hy, WORD& to_hx, WORD& to_hy, float angle, DWORD dist);
		static DWORD Global_GetPathLengthHex(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, DWORD cut);
		static bool Global_GetHexPos(WORD hx, WORD hy, int& x, int& y);
		static bool Global_GetMonitorHex(int x, int y, WORD& hx, WORD& hy);

		static DWORD Global_LoadSprite(CScriptString& spr_name, int path_index);
		static DWORD Global_LoadSpriteHash(DWORD name_hash, BYTE dir);
		static int Global_GetSpriteWidth(DWORD spr_id, int spr_index);
		static int Global_GetSpriteHeight(DWORD spr_id, int spr_index);
		static DWORD Global_GetSpriteCount(DWORD spr_id);
		static void Global_GetTextInfo(CScriptString& text, int w, int h, int font, int flags, int& tw, int& th, int& lines);
		static void Global_DrawSprite(DWORD spr_id, int spr_index, int x, int y, DWORD color);
		static void Global_DrawSpriteOffs(DWORD spr_id, int spr_index, int x, int y, DWORD color, bool offs);
		static void Global_DrawSpriteSize(DWORD spr_id, int spr_index, int x, int y, int w, int h, bool scratch, bool center, DWORD color);
		static void Global_DrawSpriteSizeOffs(DWORD spr_id, int spr_index, int x, int y, int w, int h, bool scratch, bool center, DWORD color, bool offs);
		static void Global_DrawText(CScriptString& text, int x, int y, int w, int h, DWORD color, int font, int flags);
		static void Global_DrawPrimitive(int primitive_type, CScriptArray& data);
		static void Global_DrawMapSprite(WORD hx, WORD hy, WORD proto_id, DWORD spr_id, int spr_index, int ox, int oy);
		static void Global_DrawCritter2d(DWORD crtype, DWORD anim1, DWORD anim2, BYTE dir, int l, int t, int r, int b, bool scratch, bool center, DWORD color);
		static void Global_DrawCritter3d(DWORD instance, DWORD crtype, DWORD anim1, DWORD anim2, CScriptArray* layers, CScriptArray* position, DWORD color);

		static bool Global_IsCritterCanWalk(DWORD cr_type);
		static bool Global_IsCritterCanRun(DWORD cr_type);
		static bool Global_IsCritterCanRotate(DWORD cr_type);
		static bool Global_IsCritterCanAim(DWORD cr_type);
		static bool Global_IsCritterAnim1(DWORD cr_type, DWORD index);
		static int Global_GetCritterAnimType(DWORD cr_type);
		static DWORD Global_GetCritterAlias(DWORD cr_type);
		static CScriptString* Global_GetCritterTypeName(DWORD cr_type);
		static CScriptString* Global_GetCritterSoundName(DWORD cr_type);
	};
};

#endif // __MAPPER__