#ifndef __SPRITE_MANAGER__
#define __SPRITE_MANAGER__

#include "Common.h"
#include "Sprites.h"
#include "FileManager.h"
#include "Text.h"
#include "3dStuff.h"
#include "GraphicLoader.h"

// Font flags
#define FT_NOBREAK                         ( 0x0001 )
#define FT_NOBREAK_LINE                    ( 0x0002 )
#define FT_CENTERX                         ( 0x0004 )
#define FT_CENTERY                         ( 0x0008 )
#define FT_CENTERR                         ( 0x0010 )
#define FT_BOTTOM                          ( 0x0020 )
#define FT_UPPER                           ( 0x0040 )
#define FT_NO_COLORIZE                     ( 0x0080 )
#define FT_ALIGN                           ( 0x0100 )
#define FT_BORDERED                        ( 0x0200 )
#define FT_SKIPLINES( l )             ( 0x0400 | ( ( l ) << 16 ) )
#define FT_SKIPLINES_END( l )         ( 0x0800 | ( ( l ) << 16 ) )

// Animation loading
#define ANIM_DIR( d )                 ( ( d ) & 0xFF )
#define ANIM_USE_DUMMY                     ( 0x100 )
#define ANIM_FRM_ANIM_PIX                  ( 0x200 )

// Colors
#define COLOR_CHANGE_ALPHA( v, a )    ( ( ( ( v ) | 0xFF000000 ) ^ 0xFF000000 ) | ( (uint) ( a ) & 0xFF ) << 24 )
#define COLOR_IFACE_FIX                    COLOR_XRGB( 103, 95, 86 )
#define COLOR_IFACE                        SpriteManager::PackColor( ( ( COLOR_IFACE_FIX >> 16 ) & 0xFF ) + GameOpt.Light, ( ( COLOR_IFACE_FIX >> 8 ) & 0xFF ) + GameOpt.Light, ( COLOR_IFACE_FIX & 0xFF ) + GameOpt.Light )
#define COLOR_IFACE_A( a )            ( ( COLOR_IFACE ^ 0xFF000000 ) | ( ( a ) << 24 ) )
#define COLOR_GAME_RGB( r, g, b )     SpriteManager::PackColor( ( r ) + GameOpt.Light, ( g ) + GameOpt.Light, ( b ) + GameOpt.Light )
#define COLOR_IFACE_RED                    ( COLOR_IFACE | ( 0xFF << 16 ) )
#define COLOR_IFACE_GREEN                  ( COLOR_IFACE | ( 0xFF << 8 ) )
#define COLOR_CRITTER_NAME                 COLOR_XRGB( 0xAD, 0xAD, 0xB9 )
#define COLOR_TEXT                         COLOR_XRGB( 60, 248, 0 )
#define COLOR_TEXT_WHITE                   COLOR_XRGB( 0xFF, 0xFF, 0xFF )
#define COLOR_TEXT_DWHITE                  COLOR_XRGB( 0xBF, 0xBF, 0xBF )
#define COLOR_TEXT_RED                     COLOR_XRGB( 0xC8, 0, 0 )
#define COLOR_TEXT_DRED                    COLOR_XRGB( 0xAA, 0, 0 )
#define COLOR_TEXT_DDRED                   COLOR_XRGB( 0x66, 0, 0 )
#define COLOR_TEXT_LRED                    COLOR_XRGB( 0xFF, 0, 0 )
#define COLOR_TEXT_BLUE                    COLOR_XRGB( 0, 0, 0xC8 )
#define COLOR_TEXT_DBLUE                   COLOR_XRGB( 0, 0, 0xAA )
#define COLOR_TEXT_LBLUE                   COLOR_XRGB( 0, 0, 0xFF )
#define COLOR_TEXT_GREEN                   COLOR_XRGB( 0, 0xC8, 0 )
#define COLOR_TEXT_DGREEN                  COLOR_XRGB( 0, 0xAA, 0 )
#define COLOR_TEXT_DDGREEN                 COLOR_XRGB( 0, 0x66, 0 )
#define COLOR_TEXT_LGREEN                  COLOR_XRGB( 0, 0xFF, 0 )
#define COLOR_TEXT_BLACK                   COLOR_XRGB( 0, 0, 0 )
#define COLOR_TEXT_SBLACK                  COLOR_XRGB( 0x10, 0x10, 0x10 )
#define COLOR_TEXT_DARK                    COLOR_XRGB( 0x30, 0x30, 0x30 )
#define COLOR_TEXT_GREEN_RED               COLOR_XRGB( 0, 0xC8, 0xC8 )
#define COLOR_TEXT_SAND                    COLOR_XRGB( 0x8F, 0x6F, 0 )

// Default effects
#define DEFAULT_EFFECT_CONTOUR             ( 0 )
#define DEFAULT_EFFECT_GENERIC             ( 1 )
#define DEFAULT_EFFECT_TILE                ( 2 )
#define DEFAULT_EFFECT_ROOF                ( 3 )
#define DEFAULT_EFFECT_IFACE               ( 4 )
#define DEFAULT_EFFECT_POINT               ( 5 )
#define DEFAULT_EFFECT_FLUSH_TEXTURE       ( 6 )
#define DEFAULT_EFFECT_FLUSH_TEXTURE_MS    ( 7 )
#define DEFAULT_EFFECT_FLUSH_COLOR         ( 8 )
#define DEFAULT_EFFECT_FLUSH_FINAL         ( 9 )
#define DEFAULT_EFFECT_COUNT               ( 10 )

// Sprite layers
#define DRAW_ORDER_FLAT                    ( 0 )
#define DRAW_ORDER                         ( 20 )
#define DRAW_ORDER_TILE                    ( DRAW_ORDER_FLAT + 0 )
#define DRAW_ORDER_TILE_END                ( DRAW_ORDER_FLAT + 4 )
#define DRAW_ORDER_HEX_GRID                ( DRAW_ORDER_FLAT + 5 )
#define DRAW_ORDER_FLAT_SCENERY            ( DRAW_ORDER_FLAT + 8 )
#define DRAW_ORDER_LIGHT                   ( DRAW_ORDER_FLAT + 9 )
#define DRAW_ORDER_DEAD_CRITTER            ( DRAW_ORDER_FLAT + 10 )
#define DRAW_ORDER_FLAT_ITEM               ( DRAW_ORDER_FLAT + 13 )
#define DRAW_ORDER_TRACK                   ( DRAW_ORDER_FLAT + 16 )
#define DRAW_ORDER_SCENERY                 ( DRAW_ORDER + 3 )
#define DRAW_ORDER_ITEM                    ( DRAW_ORDER + 6 )
#define DRAW_ORDER_CRITTER                 ( DRAW_ORDER + 9 )
#define DRAW_ORDER_RAIN                    ( DRAW_ORDER + 12 )
#define DRAW_ORDER_LAST                    ( 39 )
#define DRAW_ORDER_ITEM_AUTO( i )     ( i->IsFlat() ? ( i->IsItem() ? DRAW_ORDER_FLAT_ITEM : DRAW_ORDER_FLAT_SCENERY ) : ( i->IsItem() ? DRAW_ORDER_ITEM : DRAW_ORDER_SCENERY ) )
#define DRAW_ORDER_CRIT_AUTO( c )     ( c->IsDead() && !c->IsRawParam( MODE_NO_FLATTEN ) ? DRAW_ORDER_DEAD_CRITTER : DRAW_ORDER_CRITTER )

// Sprites cutting
#define SPRITE_CUT_HORIZONTAL              ( 1 )
#define SPRITE_CUT_VERTICAL                ( 2 )
#define SPRITE_CUT_CUSTOM                  ( 3 ) // Todo

// Egg types
#define EGG_ALWAYS                         ( 1 )
#define EGG_X                              ( 2 )
#define EGG_Y                              ( 3 )
#define EGG_X_AND_Y                        ( 4 )
#define EGG_X_OR_Y                         ( 5 )

// Egg types
#define EGG_ALWAYS                         ( 1 )
#define EGG_X                              ( 2 )
#define EGG_Y                              ( 3 )
#define EGG_X_AND_Y                        ( 4 )
#define EGG_X_OR_Y                         ( 5 )

// Contour types
#define CONTOUR_RED                        ( 1 )
#define CONTOUR_YELLOW                     ( 2 )
#define CONTOUR_CUSTOM                     ( 3 )

// Primitives
#define PRIMITIVE_POINTLIST                ( 1 )
#define PRIMITIVE_LINELIST                 ( 2 )
#define PRIMITIVE_LINESTRIP                ( 3 )
#define PRIMITIVE_TRIANGLELIST             ( 4 )
#define PRIMITIVE_TRIANGLESTRIP            ( 5 )
#define PRIMITIVE_TRIANGLEFAN              ( 6 )

struct Surface
{
    int      Type;
    Texture* TextureOwner;
    uint     Width, Height;           // Texture size
    uint     BusyH;                   // Height point position
    uint     FreeX, FreeY;            // Busy positions on current surface

    Surface(): Type( 0 ), TextureOwner( NULL ), Width( 0 ), Height( 0 ), BusyH( 0 ), FreeX( 0 ), FreeY( 0 ) {}
    ~Surface() { SAFEDEL( TextureOwner ); }
};
typedef vector< Surface* > SurfaceVec;

struct Vertex
{
    #ifdef FO_D3D
    FLOAT x, y, z, rhw;
    uint  diffuse;
    FLOAT tu, tv;
    FLOAT tu2, tv2;
    Vertex(): x( 0 ), y( 0 ), z( 0 ), rhw( 1 ), tu( 0 ), tv( 0 ), tu2( 0 ), tv2( 0 ), diffuse( 0 ) {}
    # define D3DFVF_MYVERTEX               ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2 )
    #else
    float x, y;
    uint  diffuse;
    float tu, tv;
    float tu2, tv2;
    float padding;
    Vertex(): x( 0 ), y( 0 ), diffuse( 0 ), tu( 0 ), tv( 0 ), tu2( 0 ), tv2( 0 ) {}
    #endif
};
typedef vector< Vertex > VertexVec;

struct MYVERTEX_PRIMITIVE
{
    FLOAT x, y, z, rhw;
    uint  Diffuse;

    MYVERTEX_PRIMITIVE(): x( 0 ), y( 0 ), z( 0 ), rhw( 1 ), Diffuse( 0 ) {}
};
#define D3DFVF_MYVERTEX_PRIMITIVE          ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE )

struct SpriteInfo
{
    Surface*     Surf;
    FLTRECT      SprRect;
    short        Width;
    short        Height;
    short        OffsX;
    short        OffsY;
    Effect*      DrawEffect;
    Animation3d* Anim3d;
    SpriteInfo(): Surf( NULL ), Width( 0 ), Height( 0 ), OffsX( 0 ), OffsY( 0 ), DrawEffect( NULL ), Anim3d( NULL ) {}
};
typedef vector< SpriteInfo* > SprInfoVec;

struct DipData
{
    Texture* SourceTexture;
    Effect*  SourceEffect;
    uint     SpritesCount;
    #ifndef FO_D3D
    FLTRECT  SpriteBorder;
    #endif
    DipData( Texture* tex, Effect* effect ): SourceTexture( tex ), SourceEffect( effect ), SpritesCount( 1 ) {}
};
typedef vector< DipData > DipDataVec;

struct AnyFrames
{
    uint*  Ind;    // Sprite Ids
    short* NextX;  // Offsets
    short* NextY;  // Offsets
    uint   CntFrm; // Frames count
    uint   Ticks;  // Time of playing animation
    uint   Anim1;
    uint   Anim2;

    uint  GetSprId( uint num_frm ) { return Ind[ num_frm % CntFrm ]; }
    short GetNextX( uint num_frm ) { return NextX[ num_frm % CntFrm ]; }
    short GetNextY( uint num_frm ) { return NextY[ num_frm % CntFrm ]; }
    uint  GetCnt()                 { return CntFrm; }
    uint  GetCurSprId()            { return CntFrm > 1 ? Ind[ ( ( Timer::GameTick() % Ticks ) * 100 / Ticks ) * CntFrm / 100 ] : Ind[ 0 ]; }
    uint  GetCurSprIndex()         { return CntFrm > 1 ? ( ( Timer::GameTick() % Ticks ) * 100 / Ticks ) * CntFrm / 100 : 0; }

    AnyFrames(): Ind( NULL ), NextX( NULL ), NextY( NULL ), CntFrm( 0 ), Ticks( 0 ), Anim1( 0 ), Anim2( 0 ) {};
    ~AnyFrames()
    {
        SAFEDELA( Ind );
        SAFEDELA( NextX );
        SAFEDELA( NextY );
    }
};
typedef map< uint, AnyFrames*, less< uint > > AnimMap;
typedef vector< AnyFrames* >                  AnimVec;

struct PrepPoint
{
    short  PointX;
    short  PointY;
    short* PointOffsX;
    short* PointOffsY;
    uint   PointColor;

    PrepPoint(): PointX( 0 ), PointY( 0 ), PointColor( 0 ), PointOffsX( NULL ), PointOffsY( NULL ) {}
    PrepPoint( short x, short y, uint color, short* ox = NULL, short* oy = NULL ): PointX( x ), PointY( y ), PointColor( color ), PointOffsX( ox ), PointOffsY( oy ) {}
};
typedef vector< PrepPoint > PointVec;
typedef vector< PointVec >  PointVecVec;

struct SpriteMngrParams
{
    void ( * PreRestoreFunc )();
    void ( * PostRestoreFunc )();
};

#ifndef FO_D3D
struct RenderTarget
{
    GLuint   FBO;
    Texture* TargetTexture;
    GLuint   DepthStencilBuffer;
    Effect*  DrawEffect;
};
typedef vector< RenderTarget* > RenderTargetVec;
#endif

class SpriteManager
{
private:
    bool             isInit;
    SpriteMngrParams mngrParams;
    #ifdef FO_D3D
    LPDIRECT3D9      direct3D;
    #else
    float            projectionMatrix[ 16 ];
    #endif
    Device_          d3dDevice;
    PresentParams_   presentParams;
    Caps_            deviceCaps;
    int              modeWidth, modeHeight;
    bool             sceneBeginned;
    #ifndef FO_D3D
    # ifdef FO_WINDOWS
    HDC             dcScreen;
    # endif
    RenderTarget    rtMain;
    RenderTarget    rtContours;
    RenderTarget    rtContoursMid;
    RenderTarget    rtPrimitive;
    RenderTarget    rt3D;
    RenderTargetVec rtStack;
    #endif

public:
    static AnyFrames* DummyAnimation;

    SpriteManager();
    bool    Init( SpriteMngrParams& params );
    bool    InitBuffers();
    bool    InitRenderStates();
    bool    IsInit() { return isInit; }
    void    Finish();
    Device_ GetDevice() { return d3dDevice; }
    bool    BeginScene( uint clear_color );
    void    EndScene();
    bool    Restore();
    void ( * PreRestore )();
    void ( * PostRestore )();
    #ifndef FO_D3D
    bool CreateRenderTarget( RenderTarget& rt, bool depth_stencil, bool multisampling = false );
    void ClearRenderTarget( RenderTarget& rt, uint color );
    void ClearRenderTargetDS( RenderTarget& rt, bool depth, bool stencil );
    void PushRenderTarget( RenderTarget& rt );
    void PopRenderTarget();
    void DrawRenderTarget( RenderTarget& rt );
    #endif

    // Surfaces
public:
    int  SurfType;
    bool SurfFilterNearest;

    void FreeSurfaces( int surf_type );
    void SaveSufaces();

private:
    int        baseTextureSize;
    SurfaceVec surfList;

    Surface* CreateNewSurface( int w, int h );
    Surface* FindSurfacePlace( SpriteInfo* si, int& x, int& y );
    uint     FillSurfaceFromMemory( SpriteInfo* si, void* data, uint size );

    // Load sprites
public:
    AnyFrames*   LoadAnimation( const char* fname, int path_type, int flags = 0 );
    AnyFrames*   ReloadAnimation( AnyFrames* anim, const char* fname, int path_type );
    Animation3d* LoadPure3dAnimation( const char* fname, int path_type );
    void         FreePure3dAnimation( Animation3d* anim3d );

private:
    SprInfoVec sprData;
    Surface_   spr3dRT, spr3dRTEx, spr3dDS, spr3dRTData;
    int        spr3dSurfWidth, spr3dSurfHeight;

    AnyFrames* CreateAnimation( uint frames, uint ticks );
    AnyFrames* LoadAnimationFrm( const char* fname, int path_type, int dir, bool anim_pix );
    AnyFrames* LoadAnimationRix( const char* fname, int path_type );
    AnyFrames* LoadAnimationFofrm( const char* fname, int path_type, int dir );
    AnyFrames* LoadAnimation3d( const char* fname, int path_type, int dir );
    AnyFrames* LoadAnimationArt( const char* fname, int path_type, int dir );
    AnyFrames* LoadAnimationSpr( const char* fname, int path_type, int dir );
    AnyFrames* LoadAnimationZar( const char* fname, int path_type );
    AnyFrames* LoadAnimationTil( const char* fname, int path_type );
    AnyFrames* LoadAnimationMos( const char* fname, int path_type );
    AnyFrames* LoadAnimationBam( const char* fname, int path_type );
    AnyFrames* LoadAnimationOther( const char* fname, int path_type );
    uint       Render3dSprite( Animation3d* anim3d, int dir, int time_proc );

    // Draw
public:
    static uint PackColor( int r, int g, int b );
    void        SetSpritesColor( uint c ) { baseColor = c; }
    uint        GetSpritesColor()         { return baseColor; }
    SprInfoVec& GetSpritesInfo()          { return sprData; }
    SpriteInfo* GetSpriteInfo( uint id )  { return sprData[ id ]; }
    void        GetDrawCntrRect( Sprite* prep, INTRECT* prect );
    uint        GetPixColor( uint spr_id, int offs_x, int offs_y, bool with_zoom = true );
    bool        IsPixNoTransp( uint spr_id, int offs_x, int offs_y, bool with_zoom = true );
    bool        IsEggTransp( int pix_x, int pix_y );

    void PrepareSquare( PointVec& points, FLTRECT& r, uint color );
    void PrepareSquare( PointVec& points, FLTPOINT& lt, FLTPOINT& rt, FLTPOINT& lb, FLTPOINT& rb, uint color );
    bool PrepareBuffer( Sprites& dtree, Surface_ surf, int ox, int oy, uchar alpha );
    bool Flush();

    bool DrawSprite( uint id, int x, int y, uint color = 0 );
    bool DrawSpriteSize( uint id, int x, int y, float w, float h, bool stretch_up, bool center, uint color = 0 );
    bool DrawSprites( Sprites& dtree, bool collect_contours, bool use_egg, int draw_oder_from, int draw_oder_to );
    bool DrawPrepared( Surface_& surf, int ox, int oy );
    bool DrawSurface( Surface_& surf, RECT& dst );
    bool DrawPoints( PointVec& points, int prim, float* zoom = NULL, FLTRECT* stencil = NULL, FLTPOINT* offset = NULL );
    bool Draw3d( int x, int y, float scale, Animation3d* anim3d, FLTRECT* stencil, uint color );
    bool Draw3dSize( FLTRECT rect, bool stretch_up, bool center, Animation3d* anim3d, FLTRECT* stencil, uint color );

    inline bool DrawSprite( AnyFrames* frames, int x, int y, uint color = 0 )
    {
        if( frames && frames != DummyAnimation ) return DrawSprite( frames->GetCurSprId(), x, y, color );
        return false;
    }
    inline bool DrawSpriteSize( AnyFrames* frames, int x, int y, float w, float h, bool stretch_up, bool center, uint color = 0 )
    {
        if( frames && frames != DummyAnimation ) return DrawSpriteSize( frames->GetCurSprId(), x, y, w, h, stretch_up, center, color );
        return false;
    }

    void SetDefaultEffect2D( uint index, Effect* effect ) { sprDefaultEffect[ index ] = effect; }
    void SetCurEffect2D( uint index )                     { curDefaultEffect = sprDefaultEffect[ index ]; }

private:
    #ifndef FO_D3D
    GLuint        vaMain;
    #endif
    VertexBuffer_ vbMain;
    IndexBuffer_  ibMain;
    IndexBuffer_  ibDirect;
    VertexVec     vBuffer;
    DipDataVec    dipQueue;
    uint          baseColor;
    int           flushSprCnt;           // Max sprites to flush
    int           curSprCnt;             // Current sprites to flush
    #ifdef FO_D3D
    VertexBuffer_ vbPoints;
    int           vbPointsSize;
    #endif
    Effect*       sprDefaultEffect[ DEFAULT_EFFECT_COUNT ];
    Effect*       curDefaultEffect;

    // Contours
public:
    bool DrawContours();
    void ClearSpriteContours() { createdSpriteContours.clear(); }

private:
    Texture*        contoursTexture;
    Surface_        contoursTextureSurf;
    Texture*        contoursMidTexture;
    Surface_        contoursMidTextureSurf;
    Surface_        contours3dRT;
    PixelShader_*   contoursPS;
    ConstantTable_* contoursCT;
    EffectValue_    contoursConstWidthStep, contoursConstHeightStep,
                    contoursConstSpriteBorders, contoursConstSpriteBordersHeight,
                    contoursConstContourColor, contoursConstContourColorOffs;
    bool    contoursAdded;
    UIntMap createdSpriteContours;
    Sprites spriteContours;

    bool CollectContour( int x, int y, SpriteInfo* si, Sprite* spr );
    #ifdef FO_D3D
    uint GetSpriteContour( SpriteInfo* si, Sprite* spr );
    #endif

    #ifdef FO_D3D
    void WriteContour4( uint* buf, uint buf_w, LockRect_& r, uint w, uint h, uint color );
    void WriteContour8( uint* buf, uint buf_w, LockRect_& r, uint w, uint h, uint color );
    #endif

    // Transparent egg
private:
    bool        eggValid;
    ushort      eggHx, eggHy;
    int         eggX, eggY;
    short*      eggOX, * eggOY;
    SpriteInfo* sprEgg;
    int         eggSprWidth, eggSprHeight;
    float       eggSurfWidth, eggSurfHeight;

public:
    bool CompareHexEgg( ushort hx, ushort hy, int egg_type );
    void SetEgg( ushort hx, ushort hy, Sprite* spr );
    void EggNotValid() { eggValid = false; }

    // Fonts
public:
    void SetDefaultFont( int index, uint color );
    void SetFontEffect( int index, Effect* effect );
    bool LoadFontFO( int index, const char* font_name );
    bool LoadFontBMF( int index, const char* font_name );
    bool DrawStr( INTRECT& r, const char* str, uint flags, uint color = 0, int num_font = -1 );
    int  GetLinesCount( int width, int height, const char* str, int num_font = -1 );
    int  GetLinesHeight( int width, int height, const char* str, int num_font = -1 );
    int  GetLineHeight( int num_font = -1 );
    void GetTextInfo( int width, int height, const char* str, int num_font, int flags, int& tw, int& th, int& lines );
    int  SplitLines( INTRECT& r, const char* cstr, int num_font, StrVec& str_vec );
};

extern SpriteManager SprMngr;

#endif // __SPRITE_MANAGER__
