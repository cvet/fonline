#ifndef __HEX_MANAGER__
#define __HEX_MANAGER__

#include "Common.h"
#include "SpriteManager.h"
#include "Item.h"
#include "CritterCl.h"
#include "ItemHex.h"
#include "ProtoMap.h"

#define MAX_FIND_PATH    ( 600 )
#define VIEW_WIDTH       ( (int) ( ( GameOpt.ScreenWidth / GameOpt.MapHexWidth + ( ( GameOpt.ScreenWidth % GameOpt.MapHexWidth ) ? 1 : 0 ) ) * GameOpt.SpritesZoom ) )
#define VIEW_HEIGHT      ( (int) ( ( GameOpt.ScreenHeight / GameOpt.MapHexLineHeight + ( ( GameOpt.ScreenHeight % GameOpt.MapHexLineHeight ) ? 1 : 0 ) ) * GameOpt.SpritesZoom ) )
#define SCROLL_OX        ( GameOpt.MapHexWidth )
#define SCROLL_OY        ( GameOpt.MapHexLineHeight * 2 )
#define HEX_W            ( GameOpt.MapHexWidth )
#define HEX_LINE_H       ( GameOpt.MapHexLineHeight )
#define HEX_REAL_H       ( GameOpt.MapHexHeight )
#define HEX_OX           ( GameOpt.MapHexWidth / 2 )
#define HEX_OY           ( GameOpt.MapHexHeight / 2 )
#define TILE_OX          ( GameOpt.MapTileOffsX )
#define TILE_OY          ( GameOpt.MapTileOffsY )
#define ROOF_OX          ( GameOpt.MapRoofOffsX )
#define ROOF_OY          ( GameOpt.MapRoofOffsY )
#define MAX_MOVE_OX      ( 30000 )
#define MAX_MOVE_OY      ( 30000 )

/************************************************************************/
/* ViewField                                                            */
/************************************************************************/

struct ViewField
{
    int   HexX, HexY;
    int   ScrX, ScrY;
    float ScrXf, ScrYf;

    ViewField(): HexX( 0 ), HexY( 0 ), ScrX( 0 ), ScrY( 0 ), ScrXf( 0.0f ), ScrYf( 0.0f ) {};
};

/************************************************************************/
/* LightSource                                                          */
/************************************************************************/

struct LightSource
{
    ushort HexX;
    ushort HexY;
    uint   ColorRGB;
    uchar  Distance;
    uchar  Flags;
    int    Intensity;
    short* OffsX;
    short* OffsY;
    short  LastOffsX;
    short  LastOffsY;

    LightSource( ushort hx, ushort hy, uint color, uchar distance, int inten, uchar flags, short* ox = nullptr, short* oy = nullptr ): HexX( hx ), HexY( hy ), ColorRGB( color ), Distance( distance ), Flags( flags ), Intensity( inten ),
                                                                                                                                       OffsX( ox ), OffsY( oy ), LastOffsX( ox ? *ox : 0 ), LastOffsY( oy ? *oy : 0 ) {}
};
typedef vector< LightSource > LightSourceVec;

/************************************************************************/
/* Field                                                                */
/************************************************************************/

struct Field
{
    struct Tile
    {
        AnyFrames* Anim;
        short      OffsX;
        short      OffsY;
        uchar      Layer;
    };
    typedef vector< Tile > TileVec;

    CritterCl*  Crit;
    CritVec*    DeadCrits;
    int         ScrX;
    int         ScrY;
    AnyFrames*  SimplyTile[ 2 ]; // Tile / Roof
    TileVec*    Tiles[ 2 ];      // Tile / Roof
    ItemHexVec* Items;
    ItemHexVec* BlockLinesItems;
    short       RoofNum;

    struct
    {
        bool ScrollBlock : 1;
        bool IsWall : 1;
        bool IsWallSAI : 1;
        bool IsWallTransp : 1;
        bool IsScen : 1;
        bool IsExitGrid : 1;
        bool IsNotPassed : 1;
        bool IsNotRaked : 1;
        bool IsNoLight : 1;
        bool IsMultihex : 1;
    } Flags;

    uchar Corner;

    Field() = default;
    ~Field();

    void  AddItem( ItemHex* item, ItemHex* block_lines_item );
    void  EraseItem( ItemHex* item, ItemHex* block_lines_item );
    Tile& AddTile( AnyFrames* anim, short ox, short oy, uchar layer, bool is_roof );
    void  EraseTile( uint index, bool is_roof );
    uint  GetTilesCount( bool is_roof );
    Tile& GetTile( uint index, bool is_roof );
    void  AddDeadCrit( CritterCl* cr );
    void  EraseDeadCrit( CritterCl* cr );
    void  ProcessCache();
};

/************************************************************************/
/* Rain                                                                 */
/************************************************************************/

struct Drop
{
    uint  CurSprId;
    short OffsX;
    short OffsY;
    short GroundOffsY;
    short DropCnt;

    Drop(): CurSprId( 0 ), OffsX( 0 ), OffsY( 0 ), GroundOffsY( 0 ), DropCnt( 0 ) {};
    Drop( ushort id, short x, short y, short ground_y ): CurSprId( id ), OffsX( x ), OffsY( y ), GroundOffsY( ground_y ), DropCnt( -1 ) {};
};
typedef vector< Drop* > DropVec;

/************************************************************************/
/* HexField                                                             */
/************************************************************************/

class HexManager
{
    // Hexes
private:
    ushort     maxHexX, maxHexY;
    Field*     hexField;
    bool*      hexToDraw;
    char*      hexTrack;
    AnyFrames* picTrack1, * picTrack2;
    AnyFrames* picHexMask;
    bool       isShowTrack;
    bool       isShowHex;
    AnyFrames* picHex[ 3 ];
    string     curDataPrefix;

public:
    void   ResizeField( ushort w, ushort h );
    Field& GetField( ushort hx, ushort hy )     { return hexField[ hy * maxHexX + hx ]; }
    bool&  GetHexToDraw( ushort hx, ushort hy ) { return hexToDraw[ hy * maxHexX + hx ]; }
    char&  GetHexTrack( ushort hx, ushort hy )  { return hexTrack[ hy * maxHexX + hx ]; }
    ushort GetWidth()                           { return maxHexX; }
    ushort GetHeight()                          { return maxHexY; }
    void   ClearHexToDraw()                     { memzero( hexToDraw, maxHexX * maxHexY * sizeof( bool ) ); }
    void   ClearHexTrack()                      { memzero( hexTrack, maxHexX * maxHexY * sizeof( char ) ); }
    void   SwitchShowTrack();
    bool   IsShowTrack() { return isShowTrack; };

    bool FindPath( CritterCl* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, UCharVec& steps, int cut );
    bool CutPath( CritterCl* cr, ushort start_x, ushort start_y, ushort& end_x, ushort& end_y, int cut );
    bool TraceBullet( ushort hx, ushort hy, ushort tx, ushort ty, uint dist, float angle, CritterCl* find_cr, bool find_cr_safe, CritVec* critters, int find_type, UShortPair* pre_block, UShortPair* block, UShortPairVec* steps, bool check_passed );

    // Center
public:
    void FindSetCenter( int cx, int cy );

private:
    void FindSetCenterDir( ushort & hx, ushort & hy, int dirs[ 2 ], int steps );

    // Map load
private:
    hash  curPidMap;
    int   curMapTime;
    int   dayTime[ 4 ];
    uchar dayColor[ 12 ];
    hash  curHashTiles;
    hash  curHashScen;

public:
    bool   IsMapLoaded() { return curPidMap != 0; }
    bool   LoadMap( hash map_pid );
    void   UnloadMap();
    void   GetMapHash( hash map_pid, hash& hash_tiles, hash& hash_scen );
    void   GenerateItem( uint id, hash proto_id, Properties& props );
    int    GetDayTime();
    int    GetMapTime();
    int*   GetMapDayTime();
    uchar* GetMapDayColor();
    void   OnResolutionChanged();

    // Init, finish, restore
private:
    RenderTarget* rtMap;
    RenderTarget* rtTiles;
    RenderTarget* rtRoof;
    RenderTarget* rtLight;
    RenderTarget* rtFog;
    uint          rtScreenOX, rtScreenOY;
    Sprites       mainTree;
    ViewField*    viewField;

    int           screenHexX, screenHexY;
    int           hTop, hBottom, wLeft, wRight;
    int           wVisible, hVisible;

    void InitView( int cx, int cy );
    void ResizeView();
    bool IsVisible( uint spr_id, int ox, int oy );
    bool ProcessHexBorders( uint spr_id, int ox, int oy, bool resize_map );

    short*   fogOffsX, * fogOffsY;
    short    fogLastOffsX, fogLastOffsY;
    bool     fogForceRerender;
    PointVec fogLookPoints;
    PointVec fogShootPoints;
    void PrepareFogToDraw();

public:
    bool SpritesCanDrawMap;

    HexManager();
    bool Init();
    void Finish();
    void ReloadSprites();

    void ChangeZoom( int zoom );     // <0 in, >0 out, 0 normalize
    void GetScreenHexes( int& sx, int& sy );
    void GetHexCurrentPosition( ushort hx, ushort hy, int& x, int& y );
    bool ProcessHexBorders( ItemHex* item );

    void     RebuildMap( int rx, int ry );
    void     DrawMap();
    void     SetFog( PointVec& look_points, PointVec& shoot_points, short* offs_x, short* offs_y );
    Sprites& GetDrawTree() { return mainTree; }
    void     RefreshMap()  { RebuildMap( screenHexX, screenHexY ); }

    // Scroll
public:
    struct AutoScroll_
    {
        bool  Active;
        bool  CanStop;
        float OffsX, OffsY;
        float OffsXStep, OffsYStep;
        float Speed;
        uint  LockedCritter;
    } AutoScroll;

    bool Scroll();
    void ScrollToHex( int hx, int hy, float speed, bool can_stop );
    void ScrollOffset( int ox, int oy, float speed, bool can_stop );

private:
    bool ScrollCheckPos( int(&positions)[ 4 ], int dir1, int dir2 );
    bool ScrollCheck( int xmod, int ymod );

    // Weather
public:
    void SwitchShowHex();
    void SwitchShowRain();
    void SetWeather( int time, uchar rain );

    // Critters
private:
    CritMap allCritters;
    uint    chosenId;
    uint    critterContourCrId;
    int     critterContour, crittersContour;

public:
    void       SetCritter( CritterCl* cr );
    bool       TransitCritter( CritterCl* cr, int hx, int hy, bool animate, bool force );
    CritterCl* GetCritter( uint crid );
    CritterCl* GetChosen();
    void       AddCritter( CritterCl* cr );
    void       RemoveCritter( CritterCl* cr );
    void       DeleteCritter( uint crid );
    void       DeleteCritters();
    void       GetCritters( ushort hx, ushort hy, CritVec& crits, int find_type );
    CritMap& GetCritters() { return allCritters; }
    void     SetCritterContour( uint crid, int contour );
    void     SetCrittersContour( int contour );
    void     SetMultihex( ushort hx, ushort hy, uint multihex, bool set );

    // Items
private:
    ItemHexVec hexItems;

    void AddFieldItem( ushort hx, ushort hy, ItemHex* item );
    void EraseFieldItem( ushort hx, ushort hy, ItemHex* item );

public:
    uint        AddItem( uint id, hash pid, ushort hx, ushort hy, bool is_added, UCharVecVec* data );
    void        FinishItem( uint id, bool is_deleted );
    void        DeleteItem( ItemHex* item, bool destroy_item = true, ItemHexVec::iterator* it_hex_items = nullptr );
    void        PushItem( ItemHex* item );
    ItemHex*    GetItem( ushort hx, ushort hy, hash pid );
    ItemHex*    GetItemById( ushort hx, ushort hy, uint id );
    ItemHex*    GetItemById( uint id );
    void        GetItems( ushort hx, ushort hy, ItemHexVec& items );
    ItemHexVec& GetItems() { return hexItems; }
    Rect        GetRectForText( ushort hx, ushort hy );
    void        ProcessItems();

    // Light
private:
    bool           requestRebuildLight;
    bool           requestRenderLight;
    uchar*         hexLight;
    uint           lightPointsCount;
    PointVecVec    lightPoints;
    PointVec       lightSoftPoints;
    LightSourceVec lightSources;
    LightSourceVec lightSourcesScen;

    // Rebuild data
    int lightCapacity;
    int lightMinHx;
    int lightMaxHx;
    int lightMinHy;
    int lightMaxHy;
    int lightProcentR;
    int lightProcentG;
    int lightProcentB;

    void PrepareLightToDraw();
    void MarkLight( ushort hx, ushort hy, uint inten );
    void MarkLightEndNeighbor( ushort hx, ushort hy, bool north_south, uint inten );
    void MarkLightEnd( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten );
    void MarkLightStep( ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint inten );
    void TraceLight( ushort from_hx, ushort from_hy, ushort& hx, ushort& hy, int dist, uint inten );
    void ParseLightTriangleFan( LightSource& ls );
    void ParseLight( ushort hx, ushort hy, int dist, uint inten, uint flags );
    void RealRebuildLight();
    void CollectLightSources();

public:
    void            ClearHexLight()                     { memzero( hexLight, maxHexX * maxHexY * sizeof( uchar ) * 3 ); }
    uchar*          GetLightHex( ushort hx, ushort hy ) { return &hexLight[ hy * maxHexX * 3 + hx * 3 ]; }
    void            RebuildLight()                      { requestRebuildLight = requestRenderLight = true; }
    LightSourceVec& GetLights()                         { return lightSources; }

    // Tiles, roof
private:
    Sprites tilesTree, tilesAnimatedTree;
    int     roofSkip;
    Sprites roofTree, roofAnimatedTree;

    bool CheckTilesBorder( Field::Tile& tile, bool is_roof );

public:
    void RebuildTiles();
    void RebuildRoof();
    void SetSkipRoof( int hx, int hy );
    void MarkRoofNum( int hx, int hy, int num );

    // Pixel get
public:
    bool       GetHexPixel( int x, int y, ushort& hx, ushort& hy );
    ItemHex*   GetItemPixel( int x, int y, bool& item_egg ); // With transparent egg
    CritterCl* GetCritterPixel( int x, int y, bool ignore_dead_and_chosen );
    void       GetSmthPixel( int x, int y, ItemHex*& item, CritterCl*& cr );

    // Effects
public:
    bool RunEffect( hash eff_pid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy );

    // Rain
private:
    DropVec    rainData;
    int        rainCapacity;
    string     picRainFallName;
    string     picRainDropName;
    AnyFrames* picRainFall;
    AnyFrames* picRainDrop;
    Sprites    roofRainTree;

public:
    void ProcessRain();
    void SetRainAnimation( const char* fall_anim_name, const char* drop_anim_name );

    // Cursor
public:
    void SetCursorPos( int x, int y, bool show_steps, bool refresh );
    void DrawCursor( uint spr_id );
    void DrawCursor( const char* text );

private:
    int        drawCursorX;
    AnyFrames* cursorPrePic, * cursorPostPic, * cursorXPic;
    int        cursorX, cursorY;

/************************************************************************/
/* Mapper                                                               */
/************************************************************************/

    #ifdef FONLINE_MAPPER
public:
    // Proto map
    ProtoMap* CurProtoMap;
    bool SetProtoMap( ProtoMap& pmap );
    void GetProtoMap( ProtoMap& pmap );

    // Selected tile, roof
public:
    typedef vector< ProtoMap::TileVec > TileVecVec;
    TileVecVec TilesField;
    TileVecVec RoofsField;
    ProtoMap::TileVec& GetTiles( ushort hx, ushort hy, bool is_roof ) { return is_roof ? RoofsField[ hy * GetWidth() + hx ] : TilesField[ hy * GetWidth() + hx ]; }

    void ClearSelTiles();
    void ParseSelTiles();
    void SetTile( hash name, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof, bool select );
    void EraseTile( ushort hx, ushort hy, uchar layer, bool is_roof, uint skip_index );

    // Ignore pids to draw
private:
    HashSet fastPids;
    HashSet ignorePids;

public:
    void AddFastPid( hash pid );
    bool IsFastPid( hash pid );
    void ClearFastPids();
    void AddIgnorePid( hash pid );
    void SwitchIgnorePid( hash pid );
    bool IsIgnorePid( hash pid );
    void ClearIgnorePids();

    void GetHexesRect( const Rect& rect, UShortPairVec& hexes );
    void MarkPassedHexes();
    #endif // FONLINE_MAPPER

};

#endif // __HEX_MANAGER__
