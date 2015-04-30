#ifndef __HEX_MANAGER__
#define __HEX_MANAGER__

#include "Common.h"
#include "SpriteManager.h"
#include "Item.h"
#include "ItemManager.h"
#include "CritterCl.h"
#include "ItemHex.h"
#include "ProtoMap.h"

#define MAX_FIND_PATH    ( 600 )
#define TILE_ALPHA       ( 0xFF )
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

    LightSource( ushort hx, ushort hy, uint color, uchar distance, int inten, uchar flags ): HexX( hx ), HexY( hy ), ColorRGB( color ), Distance( distance ), Flags( flags ), Intensity( inten ) {}
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
    uchar LightValues[ 3 ];

    Field();
    ~Field();
    void  AddItem( ItemHex* item );
    void  EraseItem( ItemHex* item );
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
    bool   ResizeField( ushort w, ushort h );
    Field& GetField( ushort hx, ushort hy )     { return hexField[ hy * maxHexX + hx ]; }
    bool&  GetHexToDraw( ushort hx, ushort hy ) { return hexToDraw[ hy * maxHexX + hx ]; }
    char&  GetHexTrack( ushort hx, ushort hy )  { return hexTrack[ hy * maxHexX + hx ]; }
    ushort GetMaxHexX()                         { return maxHexX; }
    ushort GetMaxHexY()                         { return maxHexY; }
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
    uint  curHashTiles;
    uint  curHashWalls;
    uint  curHashScen;

public:
    bool   IsMapLoaded() { return curPidMap != 0; }
    bool   LoadMap( hash map_pid );
    void   UnloadMap();
    void   GetMapHash( hash map_pid, uint& hash_tiles, uint& hash_walls, uint& hash_scen );
    bool   GetMapData( hash map_pid, ItemVec& items, ushort& maxhx, ushort& maxhy );
    bool   ParseScenery( SceneryCl& scen );
    int    GetDayTime();
    int    GetMapTime();
    int*   GetMapDayTime();
    uchar* GetMapDayColor();
    void   OnResolutionChanged();

    // Init, finish, restore
private:
    RenderTarget* rtMap;
    Sprites       mainTree;
    ViewField*    viewField;

    int           screenHexX, screenHexY;
    int           hTop, hBottom, wLeft, wRight;
    int           wVisible, hVisible;

    void InitView( int cx, int cy );
    void ResizeView();
    bool IsVisible( uint spr_id, int ox, int oy );
    bool ProcessHexBorders( uint spr_id, int ox, int oy, bool resize_map );

public:
    void ChangeZoom( int zoom );   // <0 in, >0 out, 0 normalize
    void GetScreenHexes( int& sx, int& sy );
    void GetHexCurrentPosition( ushort hx, ushort hy, int& x, int& y );
    bool ProcessHexBorders( ItemHex* item );

public:
    bool SpritesCanDrawMap;

    HexManager();
    bool Init();
    void Finish();
    void ReloadSprites();

    void     RebuildMap( int rx, int ry );
    void     DrawMap();
    bool     Scroll();
    Sprites& GetDrawTree() { return mainTree; }
    void     RefreshMap()  { RebuildMap( screenHexX, screenHexY ); }

    struct AutoScroll_
    {
        bool   Active;
        bool   CanStop;
        double OffsX, OffsY;
        double OffsXStep, OffsYStep;
        double Speed;
        uint   LockedCritter;
    } AutoScroll;

    void ScrollToHex( int hx, int hy, double speed, bool can_stop );

private:
    bool ScrollCheckPos( int(&positions)[ 4 ], int dir1, int dir2 );
    bool ScrollCheck( int xmod, int ymod );

public:
    void SwitchShowHex();
    void SwitchShowRain();
    void SetWeather( int time, uchar rain );

    void SetCrit( CritterCl* cr );
    bool TransitCritter( CritterCl* cr, int hx, int hy, bool animate, bool force );

    // Critters
private:
    CritMap allCritters;
    uint    chosenId;
    uint    critterContourCrId;
    int     critterContour, crittersContour;

public:
    CritterCl* GetCritter( uint crid )
    {
        if( !crid ) return NULL;
        auto it = allCritters.find( crid );
        return it != allCritters.end() ? ( *it ).second : NULL;
    }
    CritterCl* GetChosen()
    {
        if( !chosenId ) return NULL;
        auto it = allCritters.find( chosenId );
        return it != allCritters.end() ? ( *it ).second : NULL;
    }
    void     AddCrit( CritterCl* cr );
    void     RemoveCrit( CritterCl* cr );
    void     EraseCrit( uint crid );
    void     ClearCritters();
    void     GetCritters( ushort hx, ushort hy, CritVec& crits, int find_type );
    CritMap& GetCritters() { return allCritters; }
    void     SetCritterContour( uint crid, int contour );
    void     SetCrittersContour( int contour );
    void     SetMultihex( ushort hx, ushort hy, uint multihex, bool set );

    // Items
private:
    ItemHexVec hexItems;

    void PlaceItemBlocks( ushort hx, ushort hy, ProtoItem* proto_item );
    void ReplaceItemBlocks( ushort hx, ushort hy, ProtoItem* proto_item );

public:
    bool                 AddItem( uint id, hash pid, ushort hx, ushort hy, bool is_added, UCharVecVec* data );
    void                 FinishItem( uint id, bool is_deleted );
    ItemHexVec::iterator DeleteItem( ItemHex* item, bool with_delete = true );
    void                 PushItem( ItemHex* item );
    ItemHex*             GetItem( ushort hx, ushort hy, hash pid );
    ItemHex*             GetItemById( ushort hx, ushort hy, uint id );
    ItemHex*             GetItemById( uint id );
    void                 GetItems( ushort hx, ushort hy, ItemHexVec& items );
    ItemHexVec& GetItems() { return hexItems; }
    Rect        GetRectForText( ushort hx, ushort hy );
    void        ProcessItems();

    // Light
private:
    bool           requestRebuildLight;
    uchar*         hexLight;
    uint           lightPointsCount;
    PointVecVec    lightPoints;
    PointVec       lightSoftPoints;
    LightSourceVec lightSources;
    LightSourceVec lightSourcesScen;

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
    void            RebuildLight()                      { requestRebuildLight = true; }
    LightSourceVec& GetLights()                         { return lightSources; }

    // Tiles, roof
private:
    Sprites tilesTree;
    int     roofSkip;
    Sprites roofTree;

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
    void SetCursorVisible( bool visible ) { isShowCursor = visible; }
    void DrawCursor( uint spr_id );
    void DrawCursor( const char* text );

private:
    bool       isShowCursor;
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

    // Selected tile, roof
public:
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
    void AffectItem( MapObject* mobj, ItemHex* item );
    void AffectCritter( MapObject* mobj, CritterCl* cr );
    #endif // FONLINE_MAPPER

};

#endif // __HEX_MANAGER__
