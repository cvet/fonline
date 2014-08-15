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
#include "MsgFiles.h"
#include "IniParser.h"

// Fonts
#define FONT_FO                        ( 0 )
#define FONT_NUM                       ( 1 )
#define FONT_BIG_NUM                   ( 2 )
#define FONT_SAND_NUM                  ( 3 )
#define FONT_SPECIAL                   ( 4 )
#define FONT_DEFAULT                   ( 5 )
#define FONT_THIN                      ( 6 )
#define FONT_FAT                       ( 7 )
#define FONT_BIG                       ( 8 )

typedef vector< CritData* > CritDataVec;

class FOMapper
{
public:
    static FOMapper* Self;
    HexManager       HexMngr;
    IniParser        IfaceIni;
    bool             IsMapperStarted;
    static char      ServerWritePath[ MAX_FOPATH ];
    static char      ClientWritePath[ MAX_FOPATH ];

    FOMapper();
    bool Init();
    int  InitIface();
    bool IfaceLoadRect( Rect& comp, const char* name );
    void Finish();
    void MainLoop();
    void RefreshTiles( int tab );

    void ParseKeyboard();
    void ParseMouse();

    #define DRAW_CR_INFO_MAX           ( 3 )
    int DrawCrExtInfo;

    // Game color
    uint DayTime;
    void ChangeGameTime();

    // MSG File
    LanguagePack CurLang;
    FOMsg*       MsgText, * MsgDlg, * MsgItem, * MsgGame, * MsgGM, * MsgCombat, * MsgQuest, * MsgHolo, * MsgCraft;

    // Map text
    struct MapText
    {
        ushort HexX, HexY;
        uint   StartTick, Tick;
        string Text;
        uint   Color;
        bool   Fade;
        Rect   Pos;
        Rect   EndPos;
        bool operator==( const MapText& r ) { return HexX == r.HexX && HexY == r.HexY; }
    };
    typedef vector< MapText > MapTextVec;
    MapTextVec GameMapTexts;

    // Animations
    struct IfaceAnim
    {
        AnyFrames* Frames;
        ushort     Flags;
        uint       CurSpr;
        uint       LastTick;
        int        ResType;

        IfaceAnim( AnyFrames* frm, int res_type ): Frames( frm ), Flags( 0 ), CurSpr( 0 ), LastTick( Timer::FastTick() ), ResType( res_type ) {}
    };
    typedef vector< IfaceAnim* > IfaceAnimVec;

    #define ANIMRUN_TO_END             ( 0x0001 )
    #define ANIMRUN_FROM_END           ( 0x0002 )
    #define ANIMRUN_CYCLE              ( 0x0004 )
    #define ANIMRUN_STOP               ( 0x0008 )
    #define ANIMRUN_SET_FRM( frm )    ( ( uint( uchar( ( frm ) + 1 ) ) ) << 16 )

    IfaceAnimVec Animations;

    uint       AnimLoad( uint name_hash, int res_type );
    uint       AnimLoad( const char* fname, int path_type, int res_type );
    uint       AnimGetCurSpr( uint anim_id );
    uint       AnimGetCurSprCnt( uint anim_id );
    uint       AnimGetSprCount( uint anim_id );
    AnyFrames* AnimGetFrames( uint anim_id );
    void       AnimRun( uint anim_id, uint flags );
    void       AnimProcess();
    void       AnimFree( int res_type );

    // Cursor
    int CurMode;
    #define CUR_MODE_DEFAULT           ( 0 )
    #define CUR_MODE_MOVE_SELECTION    ( 1 )
    #define CUR_MODE_PLACE_OBJECT      ( 2 )

    AnyFrames* CurPDef, * CurPHand;

    void CurDraw();
    void CurRMouseUp();
    void CurMMouseDown();

    bool IsCurInRect( Rect& rect, int ax, int ay )                      { return ( GameOpt.MouseX >= rect[ 0 ] + ax && GameOpt.MouseY >= rect[ 1 ] + ay && GameOpt.MouseX <= rect[ 2 ] + ax && GameOpt.MouseY <= rect[ 3 ] + ay ); }
    bool IsCurInRect( Rect& rect )                                      { return ( GameOpt.MouseX >= rect[ 0 ] && GameOpt.MouseY >= rect[ 1 ] && GameOpt.MouseX <= rect[ 2 ] && GameOpt.MouseY <= rect[ 3 ] ); }
    bool IsCurInRectNoTransp( uint spr_id, Rect& rect, int ax, int ay ) { return IsCurInRect( rect, ax, ay ) && SprMngr.IsPixNoTransp( spr_id, GameOpt.MouseX - rect.L - ax, GameOpt.MouseY - rect.T - ay, false ); }
    bool IsCurInInterface();
    bool GetCurHex( ushort& hx, ushort& hy, bool ignore_interface );

    int IntMode;
    #define INT_MODE_CUSTOM0           ( 0 )
    #define INT_MODE_CUSTOM1           ( 1 )
    #define INT_MODE_CUSTOM2           ( 2 )
    #define INT_MODE_CUSTOM3           ( 3 )
    #define INT_MODE_CUSTOM4           ( 4 )
    #define INT_MODE_CUSTOM5           ( 5 )
    #define INT_MODE_CUSTOM6           ( 6 )
    #define INT_MODE_CUSTOM7           ( 7 )
    #define INT_MODE_CUSTOM8           ( 8 )
    #define INT_MODE_CUSTOM9           ( 9 )
    #define INT_MODE_ITEM              ( 10 )
    #define INT_MODE_TILE              ( 11 )
    #define INT_MODE_CRIT              ( 12 )
    #define INT_MODE_FAST              ( 13 )
    #define INT_MODE_IGNORE            ( 14 )
    #define INT_MODE_INCONT            ( 15 )
    #define INT_MODE_MESS              ( 16 )
    #define INT_MODE_LIST              ( 17 )
    #define INT_MODE_COUNT             ( 18 )
    #define TAB_COUNT                  ( 15 )

    int IntHold;
    #define INT_NONE                   ( 0 )
    #define INT_BUTTON                 ( 1 )
    #define INT_MAIN                   ( 2 )
    #define INT_SELECT                 ( 3 )
    #define INT_OBJECT                 ( 4 )
    #define INT_SUB_TAB                ( 5 )

    AnyFrames* IntMainPic, * IntPTab, * IntPSelect, * IntPShow;
    int        IntX, IntY;
    int        IntVectX, IntVectY;
    ushort     SelectHX1, SelectHY1, SelectHX2, SelectHY2;
    int        SelectX, SelectY;

    #define SELECT_TYPE_OLD            ( 0 )
    #define SELECT_TYPE_NEW            ( 1 )
    int  SelectType;

    bool IntVisible, IntFix;

    Rect IntWMain;
    Rect IntWWork, IntWHint;

    Rect IntBCust[ 10 ], IntBItem, IntBTile, IntBCrit, IntBFast, IntBIgnore, IntBInCont, IntBMess, IntBList,
         IntBScrBack, IntBScrBackFst, IntBScrFront, IntBScrFrontFst;

    Rect IntBShowItem, IntBShowScen, IntBShowWall, IntBShowCrit, IntBShowTile, IntBShowRoof, IntBShowFast;

    void IntDraw();
    void IntLMouseDown();
    void IntLMouseUp();
    void IntMouseMove();
    void IntSetMode( int mode );

    // Maps
    ProtoMapPtrVec LoadedProtoMaps;
    ProtoMap*      CurProtoMap;

    // Tabs
    #define DEFAULT_SUB_TAB            "000 - all"
    struct SubTab
    {
        ProtoItemVec ItemProtos;
        CritDataVec  NpcProtos;
        StrVec       TileNames;
        UIntVec      TileHashes;
        int          Index, Scroll;
        SubTab(): Index( 0 ), Scroll( 0 ) {}
    };
    typedef map< string, SubTab > SubTabMap;

    struct TileTab
    {
        StrVec  TileDirs;
        BoolVec TileSubDirs;
    };

    SubTabMap  Tabs[ TAB_COUNT ];
    SubTab*    TabsActive[ TAB_COUNT ];
    TileTab    TabsTiles[ TAB_COUNT ];
    string     TabsName[ INT_MODE_COUNT ];
    int        TabsScroll[ INT_MODE_COUNT ];

    bool       SubTabsActive;
    int        SubTabsActiveTab;
    AnyFrames* SubTabsPic;
    Rect       SubTabsRect;
    int        SubTabsX, SubTabsY;

    // Prototypes
    ProtoItemVec* CurItemProtos;
    UIntVec*      CurTileHashes;
    StrVec*       CurTileNames;
    CritDataVec*  CurNpcProtos;
    int           NpcDir;
    int*          CurProtoScroll;
    uint          ProtoWidth;
    uint          ProtosOnScreen;
    uint          TabIndex[ INT_MODE_COUNT ];
    int           InContScroll;
    int           ListScroll;
    MapObject*    InContObject;
    bool          DrawRoof;
    int           TileLayer;

    uint GetTabIndex();
    void SetTabIndex( uint index );
    void RefreshCurProtos();
    bool IsObjectMode() { return CurItemProtos && CurProtoScroll; }
    bool IsTileMode()   { return CurTileHashes && CurTileNames && CurProtoScroll; }
    bool IsCritMode()   { return CurNpcProtos && CurProtoScroll; }

    // Select
    Rect IntBSelectItem, IntBSelectScen, IntBSelectWall, IntBSelectCrit, IntBSelectTile, IntBSelectRoof;
    bool IsSelectItem, IsSelectScen, IsSelectWall, IsSelectCrit, IsSelectTile, IsSelectRoof;

    // Select Map Object
    struct SelMapObj
    {
        MapObject*      MapObj;
        ItemHex*        MapItem;
        CritterCl*      MapNpc;
        MapObjectPtrVec Childs;

        SelMapObj( MapObject* mobj, ItemHex* itm ): MapObj( mobj ), MapItem( itm ), MapNpc( NULL ) {}
        SelMapObj( MapObject* mobj, CritterCl* npc ): MapObj( mobj ), MapItem( NULL ), MapNpc( npc ) {}
        SelMapObj( const SelMapObj& r )
        {
            MapObj = r.MapObj;
            MapItem = r.MapItem;
            MapNpc = r.MapNpc;
            Childs = r.Childs;
        }
        SelMapObj& operator=( const SelMapObj& r )
        {
            MapObj = r.MapObj;
            MapItem = r.MapItem;
            MapNpc = r.MapNpc;
            Childs = r.Childs;
            return *this;
        }
        bool operator==( const MapObject* r ) { return MapObj == r; }
        bool IsItem()                         { return MapItem != NULL; }
        bool IsNpc()                          { return MapNpc != NULL; }
        bool IsContainer()                    { return IsNpc() || ( IsItem() && MapItem->Proto->Type == ITEM_TYPE_CONTAINER ); }
    };
    typedef vector< SelMapObj > SelMapProtoItemVec;
    SelMapProtoItemVec SelectedObj;

    // Select Tile, Roof
    struct SelMapTile
    {
        ushort HexX, HexY;
        bool   IsRoof;

        SelMapTile( ushort hx, ushort hy, bool is_roof ): HexX( hx ), HexY( hy ), IsRoof( is_roof ) {}
        SelMapTile( const SelMapTile& r ) { memcpy( this, &r, sizeof( SelMapTile ) ); }
        SelMapTile& operator=( const SelMapTile& r )
        {
            memcpy( this, &r, sizeof( SelMapTile ) );
            return *this;
        }
    };
    typedef vector< SelMapTile > SelMapTileVec;
    SelMapTileVec SelectedTile;

    // Select methods
    MapObject* FindMapObject( ProtoMap& pmap, ushort hx, ushort hy, uchar mobj_type, ushort pid, uint skip );
    void       FindMapObjects( ProtoMap& pmap, ushort hx, ushort hy, uint radius, uchar mobj_type, ushort pid, MapObjectPtrVec& objects );
    MapObject* FindMapObject( ushort hx, ushort hy, uchar mobj_type, ushort pid, bool skip_selected );
    void       UpdateMapObject( MapObject* mobj );
    void       MoveMapObject( MapObject* mobj, ushort hx, ushort hy );
    void       DeleteMapObject( MapObject* mobj );
    void       SelectClear();
    void       SelectAddItem( ItemHex* item );
    void       SelectAddCrit( CritterCl* npc );
    void       SelectAddTile( ushort hx, ushort hy, bool is_roof );
    void       SelectAdd( MapObject* mobj, bool select_childs = true );
    void       SelectErase( MapObject* mobj );
    void       SelectAll();
    bool       SelectMove( bool hex_move, int& offs_hx, int& offs_hy, int& offs_x, int& offs_y );
    void       SelectDelete();

    // Parse new
    uint   AnyId;
    IntVec ShowCritterParams;
    StrVec ShowCritterParamNames;

    MapObject* ParseProto( ushort pid, ushort hx, ushort hy, MapObject* owner, bool is_child = false );
    void       ParseTile( uint name_hash, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof );
    void       ParseNpc( ushort pid, ushort hx, ushort hy );
    MapObject* ParseMapObj( MapObject* mobj );

    // Buffer
    struct TileBuf
    {
        uint   NameHash;
        ushort HexX, HexY;
        short  OffsX, OffsY;
        uchar  Layer;
        bool   IsRoof;
    };
    typedef vector< TileBuf > TileBufVec;

    MapObjectPtrVec MapObjBuffer;
    TileBufVec      TilesBuffer;

    void BufferCopy();
    void BufferCut();
    void BufferPaste( int hx, int hy );

    // Object
    #define DRAW_NEXT_HEIGHT          ( 12 )

    AnyFrames* ObjWMainPic, * ObjPBToAllDn;
    Rect       ObjWMain, ObjWWork, ObjBToAll;
    int        ObjX, ObjY;
    int        ItemVectX, ItemVectY;
    int        ObjCurLine;
    bool       ObjVisible, ObjFix;
    bool       ObjToAll;

    void ObjDraw();
    void ObjKeyDown( uchar dik, const char* dik_text );
    void ObjKeyDownA( MapObject* o, uchar dik, const char* dik_text );

    // Console
    AnyFrames*       ConsolePic;
    int              ConsolePicX, ConsolePicY, ConsoleTextX, ConsoleTextY;
    bool             ConsoleEdit;
    string           ConsoleStr;
    uint             ConsoleCur;

    vector< string > ConsoleHistory;
    int              ConsoleHistoryCur;

    #define CONSOLE_KEY_TICK          ( 500 )
    #define CONSOLE_MAX_ACCELERATE    ( 460 )
    int    ConsoleLastKey;
    string ConsoleLastKeyText;
    uint   ConsoleKeyTick;
    int    ConsoleAccelerate;

    void ConsoleDraw();
    void ConsoleKeyDown( uchar dik, const char* dik_text );
    void ConsoleKeyUp( uchar dik );
    void ConsoleProcess();
    void ParseCommand( const char* cmd );

    // Mess box
    struct MessBoxMessage
    {
        int    Type;
        string Mess;
        string Time;

        MessBoxMessage( int type, const char* mess, const char* time ): Type( type ), Mess( mess ), Time( time ) {}
        MessBoxMessage( const MessBoxMessage& r )
        {
            Type = r.Type;
            Mess = r.Mess;
            Time = r.Time;
        }
        MessBoxMessage& operator=( const MessBoxMessage& r )
        {
            Type = r.Type;
            Mess = r.Mess;
            Time = r.Time;
            return *this;
        }
    };
    typedef vector< MessBoxMessage > MessBoxMessageVec;

    MessBoxMessageVec MessBox;
    string            MessBoxCurText;
    int               MessBoxScroll;

    void MessBoxGenerate();
    void AddMess( const char* message_text );
    void AddMessFormat( const char* message_text, ... );
    void MessBoxDraw();
    bool SaveLogFile();

    // Extended slots
    struct SlotExt
    {
        uchar Index;
        char* SlotName;
    };
    typedef map< int, SlotExt > SlotExtMap;
    SlotExtMap SlotsExt;

    // Scripts
    static bool SpritesCanDraw;
    void InitScriptSystem();
    void FinishScriptSystem();
    void RunStartScript();
    void RunMapLoadScript( ProtoMap* pmap );
    void RunMapSaveScript( ProtoMap* pmap );
    void DrawIfaceLayer( uint layer );

    struct SScriptFunc
    {
        static ScriptString* MapperObject_get_ScriptName( MapObject& mobj );
        static void          MapperObject_set_ScriptName( MapObject& mobj, ScriptString* str );
        static ScriptString* MapperObject_get_FuncName( MapObject& mobj );
        static void          MapperObject_set_FuncName( MapObject& mobj, ScriptString* str );
        static uchar         MapperObject_get_Critter_Cond( MapObject& mobj );
        static void          MapperObject_set_Critter_Cond( MapObject& mobj, uchar value );
        static ScriptString* MapperObject_get_PicMap( MapObject& mobj );
        static void          MapperObject_set_PicMap( MapObject& mobj, ScriptString* str );
        static ScriptString* MapperObject_get_PicInv( MapObject& mobj );
        static void          MapperObject_set_PicInv( MapObject& mobj, ScriptString* str );
        static void          MapperObject_Update( MapObject& mobj );
        static MapObject*    MapperObject_AddChild( MapObject& mobj, ushort pid );
        static uint          MapperObject_GetChilds( MapObject& mobj, ScriptArray* objects );
        static void          MapperObject_MoveToHex( MapObject& mobj, ushort hx, ushort hy );
        static void          MapperObject_MoveToHexOffset( MapObject& mobj, int x, int y );
        static void          MapperObject_MoveToDir( MapObject& mobj, uchar dir );
        static int*          MapperObject_CritterParam_Index( MapObject& mobj, uint index );

        static ScriptString* MapperMap_get_Name( ProtoMap& pmap );
        static MapObject*    MapperMap_AddObject( ProtoMap& pmap, ushort hx, ushort hy, int mobj_type, ushort pid );
        static MapObject*    MapperMap_GetObject( ProtoMap& pmap, ushort hx, ushort hy, int mobj_type, ushort pid, uint skip );
        static uint          MapperMap_GetObjects( ProtoMap& pmap, ushort hx, ushort hy, uint radius, int mobj_type, ushort pid, ScriptArray* objects );
        static void          MapperMap_UpdateObjects( ProtoMap& pmap );
        static void          MapperMap_Resize( ProtoMap& pmap, ushort width, ushort height );
        static uint          MapperMap_GetTilesCount( ProtoMap& pmap, ushort hx, ushort hy, bool roof );
        static void          MapperMap_DeleteTile( ProtoMap& pmap, ushort hx, ushort hy, bool roof, int layer );
        static uint          MapperMap_GetTileHash( ProtoMap& pmap, ushort hx, ushort hy, bool roof, int layer );
        static void          MapperMap_AddTileHash( ProtoMap& pmap, ushort hx, ushort hy, int ox, int oy, int layer, bool roof, uint pic_hash );
        static ScriptString* MapperMap_GetTileName( ProtoMap& pmap, ushort hx, ushort hy, bool roof, int layer );
        static void          MapperMap_AddTileName( ProtoMap& pmap, ushort hx, ushort hy, int ox, int oy, int layer, bool roof, ScriptString* pic_name );
        static uint          MapperMap_GetDayTime( ProtoMap& pmap, uint day_part );
        static void          MapperMap_SetDayTime( ProtoMap& pmap, uint day_part, uint time );
        static void          MapperMap_GetDayColor( ProtoMap& pmap, uint day_part, uchar& r, uchar& g, uchar& b );
        static void          MapperMap_SetDayColor( ProtoMap& pmap, uint day_part, uchar r, uchar g, uchar b );
        static ScriptString* MapperMap_get_ScriptModule( ProtoMap& pmap );
        static void          MapperMap_set_ScriptModule( ProtoMap& pmap, ScriptString* str );
        static ScriptString* MapperMap_get_ScriptFunc( ProtoMap& pmap );
        static void          MapperMap_set_ScriptFunc( ProtoMap& pmap, ScriptString* str );

        static void       Global_ShowCritterParam( int param_index, bool show, ScriptString* param_name );
        static void       Global_AllowSlot( uchar index, ScriptString& slot_name );
        static ProtoMap*  Global_LoadMap( ScriptString& file_name, int path_type );
        static void       Global_UnloadMap( ProtoMap* pmap );
        static bool       Global_SaveMap( ProtoMap* pmap, ScriptString& file_name, int path_type, bool keep_name = false );
        static bool       Global_ShowMap( ProtoMap* pmap );
        static int        Global_GetLoadedMaps( ScriptArray* maps );
        static uint       Global_GetMapFileNames( ScriptString* dir, ScriptArray* names );
        static void       Global_DeleteObject( MapObject* mobj );
        static void       Global_DeleteObjects( ScriptArray& objects );
        static void       Global_SelectObject( MapObject* mobj, bool set );
        static void       Global_SelectObjects( ScriptArray& objects, bool set );
        static MapObject* Global_GetSelectedObject();
        static uint       Global_GetSelectedObjects( ScriptArray* objects );

        static uint Global_TabGetTileDirs( int tab, ScriptArray* dir_names, ScriptArray* include_subdirs );
        static uint Global_TabGetItemPids( int tab, ScriptString* sub_tab, ScriptArray* item_pids );
        static uint Global_TabGetCritterPids( int tab, ScriptString* sub_tab, ScriptArray* critter_pids );
        static void Global_TabSetTileDirs( int tab, ScriptArray* dir_names, ScriptArray* include_subdirs );
        static void Global_TabSetItemPids( int tab, ScriptString* sub_tab, ScriptArray* item_pids );
        static void Global_TabSetCritterPids( int tab, ScriptString* sub_tab, ScriptArray* critter_pids );
        static void Global_TabDelete( int tab );
        static void Global_TabSelect( int tab, ScriptString* sub_tab, bool show );
        static void Global_TabSetName( int tab, ScriptString* tab_name );

        static ProtoItem*    Global_GetProtoItem( ushort proto_id );
        static void          Global_MoveScreen( ushort hx, ushort hy, uint speed );
        static void          Global_MoveHexByDir( ushort& hx, ushort& hy, uchar dir, uint steps );
        static ScriptString* Global_GetIfaceIniStr( ScriptString& key );
        static bool          Global_LoadDataFile( ScriptString& dat_name );
        static int           Global_GetConstantValue( int const_collection, ScriptString* name );
        static ScriptString* Global_GetConstantName( int const_collection, int value );
        static void          Global_AddConstant( int const_collection, ScriptString* name, int value );
        static bool          Global_LoadConstants( int const_collection, ScriptString* file_name, int path_type );
        static bool          Global_LoadFont( int font, ScriptString& font_fname );
        static void          Global_SetDefaultFont( int font, uint color );
        static void          Global_MouseClick( int x, int y, int button, int cursor );
        static void          Global_KeyboardPress( uchar key1, uchar key2, ScriptString* key1_text, ScriptString* key2_text );
        static void          Global_SetRainAnimation( ScriptString* fall_anim_name, ScriptString* drop_anim_name );

        static ScriptString* Global_GetLastError();
        static void          Global_Message( ScriptString& msg );
        static void          Global_MessageMsg( int text_msg, uint str_num );
        static void          Global_MapMessage( ScriptString& text, ushort hx, ushort hy, uint ms, uint color, bool fade, int ox, int oy );
        static ScriptString* Global_GetMsgStr( int text_msg, uint str_num );
        static ScriptString* Global_GetMsgStrSkip( int text_msg, uint str_num, uint skip_count );
        static uint          Global_GetMsgStrNumUpper( int text_msg, uint str_num );
        static uint          Global_GetMsgStrNumLower( int text_msg, uint str_num );
        static uint          Global_GetMsgStrCount( int text_msg, uint str_num );
        static bool          Global_IsMsgStr( int text_msg, uint str_num );
        static ScriptString* Global_ReplaceTextStr( ScriptString& text, ScriptString& replace, ScriptString& str );
        static ScriptString* Global_ReplaceTextInt( ScriptString& text, ScriptString& replace, int i );

        static void       Global_GetHexInPath( ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist );
        static uint       Global_GetPathLengthHex( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut );
        static bool       Global_GetHexPos( ushort hx, ushort hy, int& x, int& y );
        static bool       Global_GetMonitorHex( int x, int y, ushort& hx, ushort& hy, bool ignore_interface );
        static MapObject* Global_GetMonitorObject( int x, int y, bool ignore_interface );

        static uint Global_LoadSprite( ScriptString& spr_name, int path_index );
        static uint Global_LoadSpriteHash( uint name_hash );
        static int  Global_GetSpriteWidth( uint spr_id, int spr_index );
        static int  Global_GetSpriteHeight( uint spr_id, int spr_index );
        static uint Global_GetSpriteCount( uint spr_id );
        static void Global_GetTextInfo( ScriptString* text, int w, int h, int font, int flags, int& tw, int& th, int& lines );
        static void Global_DrawSprite( uint spr_id, int frame_index, int x, int y, uint color, bool offs );
        static void Global_DrawSpriteSize( uint spr_id, int frame_index, int x, int y, int w, int h, bool zoom, uint color, bool offs );
        static void Global_DrawSpritePattern( uint spr_id, int frame_index, int x, int y, int w, int h, int spr_width, int spr_height, uint color );
        static void Global_DrawText( ScriptString& text, int x, int y, int w, int h, uint color, int font, int flags );
        static void Global_DrawPrimitive( int primitive_type, ScriptArray& data );
        static void Global_DrawMapSprite( ushort hx, ushort hy, ushort proto_id, uint spr_id, int spr_index, int ox, int oy );
        static void Global_DrawCritter2d( uint crtype, uint anim1, uint anim2, uchar dir, int l, int t, int r, int b, bool scratch, bool center, uint color );
        static void Global_DrawCritter3d( uint instance, uint crtype, uint anim1, uint anim2, ScriptArray* layers, ScriptArray* position, uint color );

        static bool          Global_IsCritterCanWalk( uint cr_type );
        static bool          Global_IsCritterCanRun( uint cr_type );
        static bool          Global_IsCritterCanRotate( uint cr_type );
        static bool          Global_IsCritterCanAim( uint cr_type );
        static bool          Global_IsCritterCanArmor( uint cr_type );
        static bool          Global_IsCritterAnim1( uint cr_type, uint index );
        static int           Global_GetCritterAnimType( uint cr_type );
        static uint          Global_GetCritterAlias( uint cr_type );
        static ScriptString* Global_GetCritterTypeName( uint cr_type );
        static ScriptString* Global_GetCritterSoundName( uint cr_type );
    };
};

#endif // __MAPPER__
