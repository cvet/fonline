#ifndef __3D_STUFF__
#define __3D_STUFF__

#include "Common.h"
#include "GraphicStructures.h"

#define LAYERS3D_COUNT         ( 30 )
#define DEFAULT_DRAW_SIZE      ( 128 )

#define ANIMATION_STAY         ( 0x01 )
#define ANIMATION_ONE_TIME     ( 0x02 )
#define ANIMATION_PERIOD( proc )    ( 0x04 | ( ( proc ) << 16 ) )
#define ANIMATION_NO_SMOOTH    ( 0x08 )
#define ANIMATION_INIT         ( 0x10 )
#define ANIMATION_COMBAT       ( 0x20 )

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
    uint            LinkBoneHash;
    char*           ChildFName;
    float           RotX, RotY, RotZ;
    float           MoveX, MoveY, MoveZ;
    float           ScaleX, ScaleY, ScaleZ;
    float           SpeedAjust;

    int*            DisabledLayers;
    uint            DisabledLayersCount;

    uint*           DisabledMesh;
    uint            DisabledMeshCount;

    char**          TextureName;
    uint*           TextureMesh;
    int*            TextureNum;
    uint            TextureCount;

    EffectInstance* EffectInst;
    uint*           EffectMesh;
    uint            EffectCount;
};
typedef vector< AnimParams > AnimParamsVec;

class Animation3d
{
private:
    friend class Animation3dEntity;
    friend class Animation3dXFile;

    // All loaded animations
    static Animation3dVec loadedAnimations;

    // Parameters
    CombinedMeshVec    combinedMeshes;
    size_t             combinedMeshesSize;
    MeshInstanceVec    allMeshes;
    BoolVec            allMeshesDisabled;
    Animation3dEntity* animEntity;
    AnimController*    animController;
    int                currentLayers[ LAYERS3D_COUNT + 1 ];        // +1 for actions
    uint               currentTrack;
    uint               lastDrawTick;
    uint               endTick;
    Matrix             matRot, matScale;
    Matrix             matScaleBase, matRotBase, matTransBase;
    float              speedAdjustBase, speedAdjustCur, speedAdjustLink;
    bool               shadowDisabled;
    float              dirAngle;
    Vector             groundPos;
    bool               useGameTimer;
    float              animPosProc, animPosTime, animPosPeriod;
    bool               allowMeshGeneration;

    // Derived animations
    Animation3dVec childAnimations;
    Animation3d*   parentAnimation;
    Node*          parentNode;
    Matrix         parentMatrix;
    NodeVec        linkNodes;
    MatrixVec      linkMatricles;
    AnimParams     animLink;
    bool           childChecker;

    void  GenerateCombinedMeshes();
    void  FillCombinedMeshes( Animation3d* base, Animation3d* cur );
    void  CombineMesh( MeshInstance& mesh_instance );
    void  ProcessAnimation( float elapsed, int x, int y, float scale );
    void  UpdateNodeMatrices( Node* node, const Matrix* parent_matrix );
    void  DrawCombinedMeshes();
    void  DrawCombinedMesh( CombinedMesh* combined_mesh, bool shadow );
    void  TransformMesh( CombinedMesh* combined_mesh );
    float GetSpeed();
    uint  GetTick();
    void  SetAnimData( AnimParams& data, bool clear );

public:
    uint SprId;
    int  SprAtlasType;

    Animation3d();
    ~Animation3d();

    void StartMeshGeneration();
    bool SetAnimation( uint anim1, uint anim2, int* layers, int flags );
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
    bool NeedDraw();
    void Draw( int x, int y );
    bool IsAnimationPlaying();
    void GetRenderFramesData( float& period, int& proc_from, int& proc_to, int& dir );
    void GetDrawSize( uint& draw_width, uint& draw_height );

    static bool         StartUp();
    static void         SetScreenSize( int width, int height );
    static void         Finish();
    static void         BeginScene();
    static Animation3d* GetAnimation( const char* name, int path_type, bool is_child );
    static Animation3d* GetAnimation( const char* name, bool is_child );
    static void         AnimateFaster();
    static void         AnimateSlower();
    static Vector       Convert2dTo3d( int x, int y );
    static Point        Convert3dTo2d( Vector pos );
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
    int                         renderAnimDir;
    bool                        shadowDisabled;
    bool                        calcualteTangetSpace;
    uint                        drawWidth, drawHeight;

    void ProcessTemplateDefines( char* str, StrVec& def );
    int  GetAnimationIndex( uint& anim1, uint& anim2, float* speed, bool combat_first );
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
    void         FixTextureCoords( Node* node, const char* model_path );

public:
    Animation3dXFile();
    ~Animation3dXFile();

    static void FixAllTextureCoords();
};

#endif // __3D_STUFF__
