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
#include "Names.h"
#include "CritterData.h"
#include "CritterManager.h"
#include "ResourceManager.h"
#include "CritterType.h"
#include "Script.h"

// Fonts
#define FONT_FO         (0)
#define FONT_NUM        (1)
#define FONT_BIG_NUM    (2)
#define FONT_SPECIAL    (3)
#define FONT_DEF        (4)
#define FONT_THIN       (5)
#define FONT_FAT        (6)
#define FONT_BIG        (7)

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
	SpriteManager SprMngr;
	HexManager HexMngr;
	IniParser IfaceIni;
	bool IsMapperStarted;

	FOMapper();
	bool Init(HWND wnd);
	int InitIface();
	void Finish();
	bool InitDI();
	void ParseKeyboard();
	void ParseMouse();
	void MainLoop();
	void AddFastProto(WORD pid);
	void RefreshTiles();

#define DRAW_CR_INFO_MAX      (3)
	int DrawCrExtInfo;

	// Game color
	DWORD DayTime;
	void ProcessGameTime();

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
#define CUR_MODE_DEF		0
#define CUR_MODE_MOVE		1
#define CUR_MODE_DRAW		2
#define MAX_CUR_MODES		3

	DWORD CurPDef,CurPHand;
	int CurX,CurY;

	void CurDraw();
	void CurRMouseUp();
	void CurMMouseDown();

	bool GetMouseHex(WORD& hx, WORD& hy);
	bool IsCurInRect(INTRECT& rect, int ax, int ay){return (CurX>=rect[0]+ax && CurY>=rect[1]+ay && CurX<=rect[2]+ax && CurY<=rect[3]+ay);}
	bool IsCurInRect(INTRECT& rect){return (CurX>=rect[0] && CurY>=rect[1] && CurX<=rect[2] && CurY<=rect[3]);}
	bool IsCurInRectNoTransp(DWORD spr_id,INTRECT& rect, int ax, int ay){return IsCurInRect(rect,ax,ay) && SprMngr.IsPixNoTransp(spr_id,CurX-rect.L-ax,CurY-rect.T-ay,false);}

	int IntMode;
#define INT_MODE_NONE       (0)
#define INT_MODE_ARMOR      (1)
#define INT_MODE_DRUG       (2)
#define INT_MODE_WEAPON     (3)
#define INT_MODE_AMMO       (4)
#define INT_MODE_MISC       (5)
#define INT_MODE_MISC2      (6)
#define INT_MODE_KEY        (7)
#define INT_MODE_CONT       (8)
#define INT_MODE_DOOR       (9)
#define INT_MODE_GRID       (10)
#define INT_MODE_GENERIC    (11)
#define INT_MODE_WALL       (12)
#define INT_MODE_TILE       (13)
#define INT_MODE_CRIT       (14)
#define INT_MODE_FAST       (15)
#define INT_MODE_IGNORE     (16)
#define INT_MODE_INCONT     (17)
#define INT_MODE_MESS       (18)
#define INT_MODE_LIST       (19)

	int IntHold;
#define INT_NONE            (0)
#define INT_BUTTON          (1)
#define INT_MAIN            (2)
#define INT_SELECT          (3)
#define INT_OBJECT          (4)

	DWORD IntMainPic,IntPTab,IntPSelect,IntPShow;
	int IntX,IntY;
	int IntVectX,IntVectY;
	WORD SelectHX1,SelectHY1,SelectHX2,SelectHY2;

#define SELECT_TYPE_OLD     (0)
#define SELECT_TYPE_NEW     (1)
	int SelectType;

	bool IntVisible,IntFix;

	INTRECT IntWMain;
	INTRECT IntWWork,IntWHint;

	INTRECT IntBArm,IntBDrug,IntBWpn,IntBAmmo,IntBMisc,IntBMisc2,IntBKey,IntBCont,IntBDoor,
		IntBGrid,IntBGen,IntBWall,IntBTile,IntBCrit,
		IntBFast,IntBIgnore,IntBInCont,IntBMess,IntBList,
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

	// Proto
	int ScrArm,ScrDrug,ScrWpn,ScrAmmo,ScrMisc,ScrMisc2,ScrKey,ScrCont,ScrDoor,
		ScrGrid,ScrGen,ScrWall,ScrTile,ScrCrit,ScrList;

	ProtoItemVec* CurItemProtos;
	ProtoItemVec FastItemProto;
	ProtoItemVec IgnoreItemProto;
	DwordVec TilesPictures;
	StrVec TilesPicturesNames;
	CritDataVec NpcProtos;
	int NpcDir;
	int ProtoScroll[20];
	int* CurProtoScroll;
	int ProtoWidth;
	int ProtosOnScreen;
	int CurProto[20];
	int InContScroll;
	int ListScroll;
	MapObject* InContObject;
	bool DrawRoof;

	bool IsObjectMode(){return (IntMode==INT_MODE_ARMOR || IntMode==INT_MODE_DRUG ||
		IntMode==INT_MODE_WEAPON || IntMode==INT_MODE_AMMO || IntMode==INT_MODE_MISC ||
		IntMode==INT_MODE_MISC2 || IntMode==INT_MODE_KEY || IntMode==INT_MODE_CONT ||
		IntMode==INT_MODE_DOOR || IntMode==INT_MODE_GRID || IntMode==INT_MODE_GENERIC ||
		IntMode==INT_MODE_WALL || IntMode==INT_MODE_FAST ||
		IntMode==INT_MODE_IGNORE) &&
		(CurItemProtos && CurProtoScroll);}
	bool IsTileMode(){return IntMode==INT_MODE_TILE;}
	bool IsCritMode(){return IntMode==INT_MODE_CRIT && CurProtoScroll;}

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
		bool IsContainer(){return IsNpc() || (IsItem() && MapItem->Proto->GetType()==ITEM_TYPE_CONTAINER);}
	};
typedef vector<SelMapObj> SelMapProtoItemVec;
	SelMapProtoItemVec SelectedObj;

	// Select Tile, Roof
	struct SelMapTile 
	{
		WORD TileX;
		WORD TileY;
		DWORD PicId;
		bool IsRoof;
		WORD HexX;
		WORD HexY;

		SelMapTile(WORD tx, WORD ty, DWORD name, bool is_roof):TileX(tx),TileY(ty),PicId(name),IsRoof(is_roof){HexX=TileX*2;HexY=TileY*2;}
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
	void SelectAddTile(WORD tx, WORD ty, DWORD name_hash, bool is_roof, bool null_old);
	void SelectAdd(MapObject* mobj);
	void SelectErase(MapObject* mobj);
	void SelectAll();
	void SelectMove(int vect_hx, int vect_hy);
	void SelectDelete();

	// Parse new
	DWORD AnyId;
	int DefaultCritterParam[MAPOBJ_CRITTER_PARAMS];

	void ParseProto(WORD pid, WORD hx, WORD hy, bool in_cont);
	void ParseTile(DWORD name_hash, WORD hx, WORD hy);
	void ParseNpc(WORD pid, WORD hx, WORD hy);
	MapObject* ParseMapObj(MapObject* mobj);

	// Buffer
	MapObjectPtrVec MapObjBuffer;
	SelMapTileVec TilesBuffer;

	void BufferCopy();
	void BufferCut();
	void BufferPaste(int hx, int hy);

	// Object
#define DRAW_NEXT_HEIGHT			12

	DWORD ObjWMainPic,ObjPBToAllDn;
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
	DWORD ConsolePic;
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
	void DrawIfaceLayer(DWORD layer);

	struct SScriptFunc
	{
		static CScriptString* MapperObject_get_ScriptName(MapObject& mobj);
		static void MapperObject_set_ScriptName(MapObject& mobj, CScriptString* str);
		static CScriptString* MapperObject_get_FuncName(MapObject& mobj);
		static void MapperObject_set_FuncName(MapObject& mobj, CScriptString* str);
		static BYTE MapperObject_get_Critter_Cond(MapObject& mobj);
		static void MapperObject_set_Critter_Cond(MapObject& mobj, BYTE value);
		static void MapperObject_Update(MapObject& mobj);
		static MapObject* MapperObject_AddChild(MapObject& mobj, WORD pid);
		static DWORD MapperObject_GetChilds(MapObject& mobj, asIScriptArray* objects);
		static void MapperObject_MoveToHex(MapObject& mobj, WORD hx, WORD hy);
		static void MapperObject_MoveToHexOffset(MapObject& mobj, int x, int y);
		static void MapperObject_MoveToDir(MapObject& mobj, BYTE dir);

		static MapObject* MapperMap_AddObject(ProtoMap& pmap, WORD hx, WORD hy, int mobj_type, WORD pid);
		static MapObject* MapperMap_GetObject(ProtoMap& pmap, WORD hx, WORD hy, int mobj_type, WORD pid, DWORD skip);
		static DWORD MapperMap_GetObjects(ProtoMap& pmap, WORD hx, WORD hy, DWORD radius, int mobj_type, WORD pid, asIScriptArray* objects);
		static void MapperMap_UpdateObjects(ProtoMap& pmap);
		static void MapperMap_Resize(ProtoMap& pmap, WORD width, WORD height);
		static DWORD MapperMap_GetTileHash(ProtoMap& pmap, WORD tx, WORD ty, bool roof);
		static void MapperMap_SetTileHash(ProtoMap& pmap, WORD tx, WORD ty, bool roof, DWORD pic_hash);
		static CScriptString* MapperMap_GetTileName(ProtoMap& pmap, WORD tx, WORD ty, bool roof);
		static void MapperMap_SetTileName(ProtoMap& pmap, WORD tx, WORD ty, bool roof, CScriptString* pic_name);
		static DWORD MapperMap_GetDayTime(ProtoMap& pmap, DWORD day_part);
		static void MapperMap_SetDayTime(ProtoMap& pmap, DWORD day_part, DWORD time);
		static void MapperMap_GetDayColor(ProtoMap& pmap, DWORD day_part, BYTE& r, BYTE& g, BYTE& b);
		static void MapperMap_SetDayColor(ProtoMap& pmap, DWORD day_part, BYTE r, BYTE g, BYTE b);

		static void Global_SetDefaultCritterParam(DWORD index, int param);
		static DWORD Global_GetFastPrototypes(asIScriptArray* pids);
		static void Global_SetFastPrototypes(asIScriptArray* pids);
		static ProtoMap* Global_LoadMap(CScriptString& file_name, int path_type);
		static void Global_UnloadMap(ProtoMap* pmap);
		static bool Global_SaveMap(ProtoMap* pmap, CScriptString& file_name, int path_type, bool text, bool pack);
		static bool Global_ShowMap(ProtoMap* pmap);
		static int Global_GetLoadedMaps(asIScriptArray* maps);
		static DWORD Global_GetMapFileNames(CScriptString* dir, asIScriptArray* names);
		static void Global_DeleteObject(MapObject* mobj);
		static void Global_DeleteObjects(asIScriptArray& objects);
		static void Global_SelectObject(MapObject* mobj, bool set);
		static void Global_SelectObjects(asIScriptArray& objects, bool set);
		static ProtoItem* Global_GetProtoItem(WORD proto_id);
		static bool Global_LoadDat(CScriptString& dat_name);
		static void Global_MoveScreen(WORD hx, WORD hy, DWORD speed);
		static void Global_MoveHexByDir(WORD& hx, WORD& hy, BYTE dir, DWORD steps);
		static CScriptString* Global_GetIfaceIniStr(CScriptString& key);
		static DWORD Global_GetTick(){return Timer::FastTick();}
		static DWORD Global_GetAngelScriptProperty(int property);
		static bool Global_SetAngelScriptProperty(int property, DWORD value);
		static DWORD Global_GetStrHash(CScriptString* str);
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
		static BYTE Global_GetDirection(WORD from_x, WORD from_y, WORD to_x, WORD to_y);
		static BYTE Global_GetOffsetDir(WORD hx, WORD hy, WORD tx, WORD ty, float offset);
		static void Global_GetHexInPath(WORD from_hx, WORD from_hy, WORD& to_hx, WORD& to_hy, float angle, DWORD dist);
		static DWORD Global_GetPathLength(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, DWORD cut);
		static bool Global_GetHexPos(WORD hx, WORD hy, int& x, int& y);
		static bool Global_GetMonitorHex(int x, int y, WORD& hx, WORD& hy);
		static void Global_GetMousePosition(int& x, int& y);

		static DWORD Global_LoadSprite(CScriptString& spr_name, int path_index);
		static DWORD Global_LoadSpriteHash(DWORD name_hash, BYTE dir);
		static int Global_GetSpriteWidth(DWORD spr_id, int spr_index);
		static int Global_GetSpriteHeight(DWORD spr_id, int spr_index);
		static DWORD Global_GetSpriteCount(DWORD spr_id);
		static void Global_DrawSprite(DWORD spr_id, int spr_index, int x, int y, DWORD color);
		static void Global_DrawSpriteSize(DWORD spr_id, int spr_index, int x, int y, int w, int h, bool scratch, bool center, DWORD color);
		static void Global_DrawText(CScriptString& text, int x, int y, int w, int h, DWORD color, int font, int flags);
		static void Global_DrawPrimitive(int primitive_type, asIScriptArray& data);
		static void Global_DrawMapSprite(WORD hx, WORD hy, WORD proto_id, DWORD spr_id, int spr_index, int ox, int oy);
		static void Global_DrawCritter2d(DWORD crtype, DWORD anim1, DWORD anim2, BYTE dir, int l, int t, int r, int b, bool scratch, bool center, DWORD color);
		static void Global_DrawCritter3d(DWORD instance, DWORD crtype, DWORD anim1, DWORD anim2, asIScriptArray* layers, asIScriptArray* position, DWORD color);
	};
};

#endif // __MAPPER__