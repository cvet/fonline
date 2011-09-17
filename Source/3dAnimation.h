#ifndef __3D_STUFF__
#define __3D_STUFF__
// Some code used from http://www.toymaker.info/Games/html/load_x_hierarchy.html

#include <Timer.h>
#include <Defines.h>
#include <3dMeshStructures.h>
#include <FlexRect.h>
#include <common.h>

#define LAYERS3D_COUNT         ( 30 )

#define ANIMATION_STAY         ( 0x01 )
#define ANIMATION_ONE_TIME     ( 0x02 )
#define ANIMATION_PERIOD( proc )    ( 0x04 | ( ( proc ) << 16 ) )
#define ANIMATION_NO_SMOOTH    ( 0x08 )
#define ANIMATION_INIT         ( 0x10 )


class Animation3d;
typedef vector< Animation3d* >                 Animation3dVec;
typedef vector< Animation3d* >::iterator       Animation3dVecIt;
class Animation3dEntity;
typedef vector< Animation3dEntity* >           Animation3dEntityVec;
typedef vector< Animation3dEntity* >::iterator Animation3dEntityVecIt;
class Animation3dXFile;
typedef vector< Animation3dXFile* >            Animation3dXFileVec;
typedef vector< Animation3dXFile* >::iterator  Animation3dXFileVecIt;

struct AnimParams
{
    uint                Id;
    int                 Layer;
    int                 LayerValue;
    char*               LinkBone;
    char*               ChildFName;
    float               RotX, RotY, RotZ;
    float               MoveX, MoveY, MoveZ;
    float               ScaleX, ScaleY, ScaleZ;
    float               SpeedAjust;
    int*                DisabledLayers;
    uint                DisabledLayersCount;
    int*                DisabledSubsets;
    uint                DisabledSubsetsCount;
    char**              TextureNames;
    int*                TextureSubsets;
    int*                TextureNum;
    uint                TextureNamesCount;
    D3DXEFFECTINSTANCE* EffectInst;
    int*                EffectInstSubsets;
    uint                EffectInstSubsetsCount;
};
typedef vector< AnimParams >           AnimParamsVec;
typedef vector< AnimParams >::iterator AnimParamsVecIt;

struct MeshOptions
{
    D3DXMESHCONTAINER_EXTENDED* MeshPtr;
    uint                        SubsetsCount;
    bool*                       DisabledSubsets;
    TextureEx**                 TexSubsets;
    TextureEx**                 DefaultTexSubsets;
    EffectEx**                  EffectSubsets;
    EffectEx**                  DefaultEffectSubsets;
};
typedef vector< MeshOptions >           MeshOptionsVec;
typedef vector< MeshOptions >::iterator MeshOptionsVecIt;

class Animation3d
{
private:
    friend Animation3dEntity;
    friend Animation3dXFile;
    static Animation3dVec     generalAnimations;

    Animation3dEntity*        animEntity;
    ID3DXAnimationController* animController;
    int                       currentLayers[ LAYERS3D_COUNT + 1 ]; // +1 for actions
    uint                      numAnimationSets;
    uint                      currentTrack;
    uint                      lastTick;
    uint                      endTick;
    D3DXMATRIX                matRot, matScale;
    D3DXMATRIX                matScaleBase, matRotBase, matTransBase;
    float                     speedAdjustBase, speedAdjustCur, speedAdjustLink;
    bool                      shadowDisabled;
    float                     dirAngle;
    uint                      sprId;
    INTPOINT                  drawXY, bordersXY;
    float                     drawScale;
    D3DXVECTOR4               groundPos;
    bool                      bordersDisabled;
    INTRECT                   baseBorders, fullBorders;
    uint                      calcBordersTick;
    Vector3Vec                bordersResult;
    bool                      noDraw;
    MeshOptionsVec            meshOpt;
    bool                      useGameTimer;
    float                     animPosProc, animPosTime, animPosPeriod;

    // Derived animations
    Animation3dVec childAnimations;
    Animation3d*   parentAnimation;
    FrameEx*       parentFrame;
    D3DXMATRIX     parentMatrix;
    FrameVec       linkFrames;
    MatrixVec      linkMatricles;
    AnimParams     animLink;
    bool           childChecker;

    bool         FrameMove( double elapsed, int x, int y, float scale, bool software_skinning );
    void         UpdateFrameMatrices( const D3DXFRAME* frame_base, const D3DXMATRIX* parent_matrix );
    bool         DrawFrame( LPD3DXFRAME frame, bool with_shadow );
    bool         DrawMeshEffect( ID3DXMesh* mesh, uint subset, EffectEx* effect_ex, TextureEx** textures, D3DXHANDLE technique );
    bool         IsIntersectFrame( LPD3DXFRAME frame, const D3DXVECTOR3& ray_origin, const D3DXVECTOR3& ray_dir );
    bool         SetupBordersFrame( LPD3DXFRAME frame, FLTRECT& borders );
    void         ProcessBorders();
    double       GetSpeed();
    uint         GetTick();
    MeshOptions* GetMeshOptions( D3DXMESHCONTAINER_EXTENDED* mesh );
    static void  SetAnimData( Animation3d* anim3d, AnimParams& data, bool clear );

public:
    Animation3d();
    ~Animation3d();

    void SetAnimation( uint anim1, uint anim2, int* layers, int flags );
    bool IsAnimation( uint anim1, uint anim2 );
    bool CheckAnimation( uint& anim1, uint& anim2 );
    int  GetAnim1();
    int  GetAnim2();
    void SetDir( int dir );
    void SetDirAngle( int dir_angle );
    void SetRotation( float rx, float ry, float rz );
    void SetScale( float sx, float sy, float sz );
    void SetSpeed( float speed );
    void SetTimer( bool use_game_timer );
    void EnableShadow( bool enabled ) { shadowDisabled = !enabled; }
    bool Draw( int x, int y, float scale, FLTRECT* stencil, uint color );
    void SetDrawPos( int x, int y ) { drawXY.X = x;
                                      drawXY.Y = y; }
    bool     IsAnimationPlaying();
    bool     IsIntersect( int x, int y );
    void     SetSprId( uint value )             { sprId = value; }
    uint     GetSprId()                         { return sprId; }
    void     EnableSetupBorders( bool enabled ) { bordersDisabled = !enabled; }
    void     SetupBorders();
    INTPOINT GetBaseBordersPivot();
    INTPOINT GetFullBordersPivot();
    INTRECT  GetBaseBorders();
    INTRECT  GetFullBorders();
    INTRECT  GetExtraBorders();
    void     GetRenderFramesData( float& period, int& proc_from, int& proc_to );

    static bool         StartUp( LPDIRECT3DDEVICE9 device, bool software_skinning );
    static bool         SetScreenSize( int width, int height );
    static void         Finish();
    static void         BeginScene();
    static void         PreRestore();
    static Animation3d* GetAnimation( const char* name, int path_type, bool is_child );
    static Animation3d* GetAnimation( const char* name, bool is_child );
    static void         AnimateFaster();
    static void         AnimateSlower();
    static FLTPOINT     Convert2dTo3d( int x, int y );
    static INTPOINT     Convert3dTo2d( float x, float y );
    static void         SetDefaultEffect( EffectEx* effect );
    static bool         Is2dEmulation();
};

class Animation3dEntity
{
private:
    friend Animation3d;
    friend Animation3dXFile;
    static Animation3dEntityVec allEntities;

    string                      fileName;
    string                      pathName;
    Animation3dXFile*           xFile;
    ID3DXAnimationController*   animController;
    uint                        numAnimationSets;
    IntMap                      anim1Equals, anim2Equals;
    IntMap                      animIndexes;
    IntFloatMap                 animSpeed;
    AnimParams                  animDataDefault;
    AnimParamsVec               animData;
    int                         renderAnim;
    int                         renderAnimProcFrom, renderAnimProcTo;
    bool                        shadowDisabled;
    bool                        calcualteTangetSpace;

    void ProcessTemplateDefines( char* str, StrVec& def );
    int  GetAnimationIndex( const char* anim_name );
    int  GetAnimationIndex( uint& anim1, uint& anim2, float* speed );
    int  GetAnimationIndexEx( uint anim1, uint anim2, float* speed );

    bool         Load( const char* name );
    Animation3d* CloneAnimation();

public:
    Animation3dEntity();
    ~Animation3dEntity();
    static Animation3dEntity* GetEntity( const char* name );
};

class Animation3dXFile
{
private:
    friend Animation3d;
    friend Animation3dEntity;
    static Animation3dXFileVec xFiles;

    string                     fileName;
    D3DXFRAME*                 frameRoot;
    FrameVec                   framesSkinned;
    MeshContainerVec           allMeshes;
    uint                       facesCount;
    bool                       tangentsCalculated;

    static Animation3dXFile* GetXFile( const char* xname, bool calc_tangent );
    static bool              CalculateNormalTangent( FrameEx* frame );
    static bool              SetupSkinning( Animation3dXFile* xfile, FrameEx* frame, FrameEx* frame_root );
    static void              SetupFacesCount( FrameEx* frame, uint& count );
    static void              SetupAnimationOutput( D3DXFRAME* frame, ID3DXAnimationController* anim_controller );

    TextureEx* GetTexture( const char* tex_name );
    EffectEx*  GetEffect( D3DXEFFECTINSTANCE* effect_inst );

public:
    Animation3dXFile();
    ~Animation3dXFile();
};

#endif // __3D_STUFF__
