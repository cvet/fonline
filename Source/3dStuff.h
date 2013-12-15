#ifndef __3D_STUFF__
#define __3D_STUFF__

#include "Common.h"
#include "GraphicStructures.h"

#define BORDERS_OFFSET         ( 50.0f )

#define LAYERS3D_COUNT         ( 30 )

#define ANIMATION_STAY         ( 0x01 )
#define ANIMATION_ONE_TIME     ( 0x02 )
#define ANIMATION_PERIOD( proc )    ( 0x04 | ( ( proc ) << 16 ) )
#define ANIMATION_NO_SMOOTH    ( 0x08 )
#define ANIMATION_INIT         ( 0x10 )

class AnimController;
class AnimSet;
class Animation3d;
typedef vector< Animation3d* >       Animation3dVec;
class Animation3dEntity;
typedef vector< Animation3dEntity* > Animation3dEntityVec;
class Animation3dXFile;
typedef vector< Animation3dXFile* >  Animation3dXFileVec;

struct AnimParams
{
    uint            Id;
    int             Layer;
    int             LayerValue;
    char*           LinkBone;
    char*           ChildFName;
    float           RotX, RotY, RotZ;
    float           MoveX, MoveY, MoveZ;
    float           ScaleX, ScaleY, ScaleZ;
    float           SpeedAjust;
    int*            DisabledLayers;
    uint            DisabledLayersCount;
    int*            DisabledSubsets;
    int*            DisabledSubsetsMesh;
    uint            DisabledSubsetsCount;
    char**          TextureName;
    int*            TextureMesh;
    int*            TextureSubset;
    int*            TextureNum;
    uint            TextureCount;
    EffectInstance* EffectInst;
    int*            EffectInstMesh;
    int*            EffectInstSubset;
    uint            EffectInstCount;
};
typedef vector< AnimParams > AnimParamsVec;

struct MeshOptions
{
    Node*         NodePtr;
    uint          SubsetsCount;
    bool*         DisabledSubsets;
    MeshTexture** TexSubsets;
    MeshTexture** DefaultTexSubsets;
    Effect**      EffectSubsets;
    Effect**      DefaultEffectSubsets;
};
typedef vector< MeshOptions > MeshOptionsVec;

class Animation3d
{
private:
    friend class Animation3dEntity;
    friend class Animation3dXFile;
    static Animation3dVec generalAnimations;

    Animation3dEntity*    animEntity;
    AnimController*       animController;
    int                   currentLayers[ LAYERS3D_COUNT + 1 ];     // +1 for actions
    uint                  currentTrack;
    uint                  lastTick;
    uint                  endTick;
    Matrix                matRot, matScale;
    Matrix                matScaleBase, matRotBase, matTransBase;
    float                 speedAdjustBase, speedAdjustCur, speedAdjustLink;
    bool                  shadowDisabled;
    float                 dirAngle;
    uint                  sprId;
    Point                 drawXY;
    float                 drawScale;
    Vector                groundPos;
    RectF                 bonesBorder;
    bool                  noDraw;
    MeshOptionsVec        meshOpt;
    bool                  useGameTimer;
    float                 animPosProc, animPosTime, animPosPeriod;

    // Derived animations
    Animation3dVec childAnimations;
    Animation3d*   parentAnimation;
    Node*          parentNode;
    Matrix         parentMatrix;
    NodeVec        linkNodes;
    MatrixVec      linkMatricles;
    AnimParams     animLink;
    bool           childChecker;

    bool         MoveNode( float elapsed, int x, int y, float scale, bool transform );
    void         UpdateNodeMatrices( Node* node, const Matrix* parent_matrix );
    bool         DrawNode( Node* node, bool shadow );
    bool         IsIntersectNode( Node* node, const Vector& ray_origin, const Vector& ray_dir, float x, float y );
    float        GetSpeed();
    uint         GetTick();
    MeshOptions* GetMeshOptions( Node* node );
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
    #ifdef SHADOW_MAP
    void SetPitch( float angle );
    #endif
    void  SetScale( float sx, float sy, float sz );
    void  SetSpeed( float speed );
    void  SetTimer( bool use_game_timer );
    void  EnableShadow( bool enabled ) { shadowDisabled = !enabled; }
    bool  Draw( int x, int y, float scale, RectF* stencil, uint color );
    void  SetDrawPos( int x, int y );
    bool  IsAnimationPlaying();
    bool  IsIntersect( int x, int y );
    void  SetSprId( uint value ) { sprId = value; }
    uint  GetSprId()             { return sprId; }
    Point GetGroundPos();
    Rect  GetBonesBorder( bool add_offsets = false );
    Point GetBonesBorderPivot();
    void  GetRenderFramesData( float& period, int& proc_from, int& proc_to );

    static bool         StartUp();
    static bool         SetScreenSize( int width, int height );
    static void         Finish();
    static void         BeginScene();
    static void         PreRestore();
    static Animation3d* GetAnimation( const char* name, int path_type, bool is_child );
    static Animation3d* GetAnimation( const char* name, bool is_child );
    static void         AnimateFaster();
    static void         AnimateSlower();
    static PointF       Convert2dTo3d( int x, int y );
    static Point        Convert3dTo2d( float x, float y );
    static bool         Is2dEmulation();
};

class Animation3dEntity
{
private:
    friend class Animation3d;
    friend class Animation3dXFile;
    static Animation3dEntityVec allEntities;

    string                      fileName;
    string                      pathName;
    Animation3dXFile*           xFile;
    AnimController*             animController;
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
    friend class Animation3d;
    friend class Animation3dEntity;
    static Animation3dXFileVec xFiles;

    string                     fileName;
    Node*                      rootNode;
    NodeVec                    allNodes;
    NodeVec                    allDrawNodes;

    static Animation3dXFile* GetXFile( const char* xname );
    static void              SetupNodes( Animation3dXFile* xfile, Node* node, Node* root_node );
    static void              SetupAnimationOutput( Node* node, AnimController* anim_controller );

    MeshTexture* GetTexture( const char* tex_name );
    Effect*      GetEffect( EffectInstance* effect_inst );

public:
    Animation3dXFile();
    ~Animation3dXFile();
};

#endif // __3D_STUFF__
