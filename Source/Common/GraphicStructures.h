#ifndef __GRAPHIC_STRUCTURES__
#define __GRAPHIC_STRUCTURES__

#include "Common.h"
#include "GraphicApi.h"
#include "FileUtils.h"
#include "Timer.h"
#include "Settings.h"
#include "assimp/types.h"
#include "assimp/scene.h"

#define EFFECT_TEXTURES         ( 10 )
#define BONES_PER_VERTEX        ( 4 )
#define MAX_BONES_PER_MODEL     ( 60 )

typedef aiMatrix4x4          Matrix;
typedef aiVector3D           Vector;
typedef aiQuaternion         Quaternion;
typedef aiColor4D            Color;
typedef vector< Vector >     VectorVec;
typedef vector< Quaternion > QuaternionVec;
typedef vector< Matrix >     MatrixVec;
typedef vector< Matrix* >    MatrixPtrVec;

struct Bone;
typedef vector< Bone* >      BoneVec;

extern bool Is3dExtensionSupported( const string& ext );

//
// Vertex2D
//

struct Vertex2D
{
    float X, Y;
    uint  Diffuse;
    float TU, TV;
    #ifndef DISABLE_EGG
    float TUEgg, TVEgg;
    #endif
};
typedef vector< Vertex2D > Vertex2DVec;

//
// Vertex3D
//

struct Vertex3D
{
    Vector Position;
    Vector Normal;
    float  TexCoord[ 2 ];
    float  TexCoordBase[ 2 ];
    Vector Tangent;
    Vector Bitangent;
    float  BlendWeights[ BONES_PER_VERTEX ];
    float  BlendIndices[ BONES_PER_VERTEX ];
};
static_assert( sizeof( Vertex3D ) == 96, "Wrong Vertex3D size." );
typedef vector< Vertex3D >    Vertex3DVec;
typedef vector< Vertex3DVec > Vertex3DVecVec;

//
// AnyFrames
//

#define ANY_FRAMES_POOL_SIZE    ( 2000 )
#define MAX_FRAMES              ( 50 )

struct AnyFrames
{
    // Data
    uint  Ind[ MAX_FRAMES ];       // Sprite Ids
    short NextX[ MAX_FRAMES ];     // Offsets
    short NextY[ MAX_FRAMES ];     // Offsets
    uint  CntFrm;                  // Frames count
    uint  Ticks;                   // Time of playing animation
    uint  Anim1;
    uint  Anim2;
    hash  NameHash;

    uint  GetSprId( uint num_frm ) { return Ind[ num_frm % CntFrm ]; }
    short GetNextX( uint num_frm ) { return NextX[ num_frm % CntFrm ]; }
    short GetNextY( uint num_frm ) { return NextY[ num_frm % CntFrm ]; }
    uint  GetCnt()                 { return CntFrm; }
    uint  GetCurSprId()            { return CntFrm > 1 ? Ind[ ( ( Timer::GameTick() % Ticks ) * 100 / Ticks ) * CntFrm / 100 ] : Ind[ 0 ]; }
    uint  GetCurSprIndex()         { return CntFrm > 1 ? ( ( Timer::GameTick() % Ticks ) * 100 / Ticks ) * CntFrm / 100 : 0; }

    // Dir animations
    bool              HaveDirs;
    AnyFrames*        Dirs[ 7 ];     // 7 additional for square hexes, 5 for hexagonal

    int        DirCount()        { return HaveDirs ? DIRS_COUNT : 1; }
    AnyFrames* GetDir( int dir ) { return dir == 0 || !HaveDirs ? this : Dirs[ dir - 1 ]; }
    void       CreateDirAnims();

    // Creation in pool
    static AnyFrames* Create( uint frames, uint ticks );
    static void       Destroy( AnyFrames* anim );
    static void       SetDummy( AnyFrames* anim );

    // Disable constructors to avoid unnecessary calls
private:
    AnyFrames() {}
    ~AnyFrames() {}
};
typedef map< uint, AnyFrames* > AnimMap;
typedef vector< AnyFrames* >    AnimVec;

//
// Point
//

struct PrepPoint
{
    short  PointX;
    short  PointY;
    short* PointOffsX;
    short* PointOffsY;
    uint   PointColor;

    PrepPoint(): PointX( 0 ), PointY( 0 ), PointOffsX( nullptr ), PointOffsY( nullptr ), PointColor( 0 ) {}
    PrepPoint( short x, short y, uint color, short* ox = nullptr, short* oy = nullptr ): PointX( x ), PointY( y ), PointOffsX( ox ), PointOffsY( oy ), PointColor( color ) {}
};
typedef vector< PrepPoint > PointVec;
typedef vector< PointVec >  PointVecVec;

//
// EffectDefault
//

struct EffectDefault
{
    enum EType { String, Float, Int };
    char*  Name;
    EType  Type;
    uchar* Data;
    uint   Size;
};

//
// EffectInstance
//

struct EffectInstance
{
    char*          EffectFilename;
    EffectDefault* Defaults;
    uint           DefaultsCount;
};

//
// Effect
//

#define IS_EFFECT_VALUE( pos )                 ( ( pos ) != -1 )
#define SET_EFFECT_VALUE( eff, pos, value )    GL( glUniform1f( pos, value ) )

struct EffectPass
{
    GLuint Program;
    bool   IsShadow;

    GLint  ZoomFactor;
    GLint  ColorMap;
    GLint  ColorMapSize;
    GLint  ColorMapSamples;
    GLint  EggMap;
    GLint  EggMapSize;
    GLint  SpriteBorder;
    GLint  ProjectionMatrix;
    GLint  GroundPosition;
    GLint  LightColor;
    GLint  WorldMatrices;

    bool   IsNeedProcess;
    bool   IsTime;
    GLint  Time;
    float  TimeCurrent;
    double TimeLastTick;
    GLint  TimeGame;
    float  TimeGameCurrent;
    double TimeGameLastTick;
    bool   IsRandom;
    GLint  Random1;
    GLint  Random2;
    GLint  Random3;
    GLint  Random4;
    bool   IsTextures;
    GLint  Textures[ EFFECT_TEXTURES ];
    GLint  TexturesSize[ EFFECT_TEXTURES ];
    GLint  TexturesAtlasOffset[ EFFECT_TEXTURES ];
    bool   IsScriptValues;
    GLint  ScriptValues[ EFFECT_SCRIPT_VALUES ];
    bool   IsAnimPos;
    GLint  AnimPosProc;
    GLint  AnimPosTime;
    bool   IsChangeStates;
    int    BlendFuncParam1;
    int    BlendFuncParam2;
    int    BlendEquation;
};
typedef vector< EffectPass > EffectPassVec;

struct Effect
{
    uint           Id;
    string         Name;
    string         Defines;
    EffectDefault* Defaults;
    EffectPassVec  Passes;

    // Default effects
    static Effect* Contour, * ContourDefault;
    static Effect* Generic, * GenericDefault;
    static Effect* Critter, * CritterDefault;
    static Effect* Tile, * TileDefault;
    static Effect* Roof, * RoofDefault;
    static Effect* Rain, * RainDefault;
    static Effect* Iface, * IfaceDefault;
    static Effect* Primitive, * PrimitiveDefault;
    static Effect* Light, * LightDefault;
    static Effect* Fog, * FogDefault;
    static Effect* FlushRenderTarget, * FlushRenderTargetDefault;
    static Effect* FlushRenderTargetMS, * FlushRenderTargetMSDefault;
    static Effect* FlushPrimitive, * FlushPrimitiveDefault;
    static Effect* FlushMap, * FlushMapDefault;
    static Effect* FlushLight, * FlushLightDefault;
    static Effect* FlushFog, * FlushFogDefault;
    static Effect* Font, * FontDefault;
    static Effect* Skinned3d, * Skinned3dDefault;

    // Constants
    static uint    MaxBones;
};
typedef vector< Effect* > EffectVec;

//
// Texture
//

struct Texture
{
    const char* Name;
    GLuint      Id;
    uint        Width;
    uint        Height;
    float       SizeData[ 4 ];        // Width, Height, TexelWidth, TexelHeight
    float       Samples;

    Texture();
    ~Texture();
    void   UpdateRegion( const Rect& r, const uchar* data );
    uchar* ReadData();
};
typedef vector< Texture* > TextureVec;

//
// MeshTexture
//

struct MeshTexture
{
    string Name;
    GLuint Id;
    float  SizeData[ 4 ];
    float  AtlasOffsetData[ 4 ];
};
typedef vector< MeshTexture* > MeshTextureVec;

//
// Render target
//

#define MAX_STORED_PIXEL_PICKS    ( 100 )

struct RenderTarget
{
    GLuint       FBO;
    Texture*     TargetTexture;
    GLuint       DepthBuffer;
    Effect*      DrawEffect;
    bool         Multisampling;
    bool         ScreenSize;
    uint         Width;
    uint         Height;
    bool         TexLinear;
    UIntPairVec* LastPixelPicks;
};
typedef vector< RenderTarget* > RenderTargetVec;

//
// TextureAtlas
//

struct TextureAtlas
{
    struct SpaceNode
    {
private:
        bool       busy;
        int        posX, posY;
        int        width, height;
        SpaceNode* child1, * child2;

public:
        SpaceNode( int x, int y, int w, int h );
        ~SpaceNode();
        bool FindPosition( int w, int h, int& x, int& y );
    };

    int           Type;
    RenderTarget* RT;
    Texture*      TextureOwner;
    uint          Width, Height;
    SpaceNode*    RootNode;                               // Packer 1
    uint          CurX, CurY, LineMaxH, LineCurH, LineW;  // Packer 2

    TextureAtlas();
    ~TextureAtlas();
};
typedef vector< TextureAtlas* > TextureAtlasVec;

//
// SpriteInfo
//

class Animation3d;
struct SpriteInfo
{
    TextureAtlas* Atlas;
    RectF         SprRect;
    short         Width;
    short         Height;
    short         OffsX;
    short         OffsY;
    Effect*       DrawEffect;
    bool          UsedForAnim3d;
    Animation3d*  Anim3d;
    uchar*        Data;
    int           DataAtlasType;
    bool          DataAtlasOneImage;
    SpriteInfo() { memzero( this, sizeof( SpriteInfo ) ); }
};
typedef vector< SpriteInfo* > SprInfoVec;

//
// DipData
//

struct DipData
{
    Texture* SourceTexture;
    Effect*  SourceEffect;
    uint     SpritesCount;
    RectF    SpriteBorder;
    DipData( Texture* tex, Effect* effect ): SourceTexture( tex ), SourceEffect( effect ), SpritesCount( 1 ) {}
};
typedef vector< DipData > DipDataVec;

//
// MeshData
//

struct MeshData
{
    Bone*          Owner;
    Vertex3DVec    Vertices;
    UShortVec      Indices;
    string         DiffuseTexture;
    UIntVec        SkinBoneNameHashes;
    MatrixVec      SkinBoneOffsets;

    // Runtime data
    BoneVec        SkinBones;
    EffectInstance DrawEffect;

    void Save( File& file );
    void Load( File& file );
};
typedef vector< MeshData* > MeshDataVec;

//
// MeshInstance
//

struct MeshInstance
{
    MeshData*    Mesh;
    bool         Disabled;
    MeshTexture* CurTexures[ EFFECT_TEXTURES ];
    MeshTexture* DefaultTexures[ EFFECT_TEXTURES ];
    MeshTexture* LastTexures[ EFFECT_TEXTURES ];
    Effect*      CurEffect;
    Effect*      DefaultEffect;
    Effect*      LastEffect;
};
typedef vector< MeshInstance > MeshInstanceVec;

//
// CombinedMesh
//

struct CombinedMesh
{
    int          EncapsulatedMeshCount;
    Vertex3DVec  Vertices;
    UShortVec    Indices;
    MeshDataVec  Meshes;
    UIntVec      MeshVertices;
    UIntVec      MeshIndices;
    IntVec       MeshAnimLayers;
    size_t       CurBoneMatrix;
    BoneVec      SkinBones;
    MatrixVec    SkinBoneOffsets;
    MeshTexture* Textures[ EFFECT_TEXTURES ];
    Effect*      DrawEffect;
    GLuint       VAO, VBO, IBO;

    void Clear();
    bool CanEncapsulate( MeshInstance& mesh_instance );
    void Encapsulate( MeshInstance& mesh_instance, int anim_layer );
    void Finalize();

    CombinedMesh(): EncapsulatedMeshCount( 0 ), CurBoneMatrix( 0 ), VAO( 0 ), VBO( 0 ), IBO( 0 ), DrawEffect( nullptr )
    {
        memzero( Textures, sizeof( Textures ) );
        SkinBones.resize( Effect::MaxBones );
        SkinBoneOffsets.resize( Effect::MaxBones );
    }
    ~CombinedMesh() { Clear(); }
};
typedef vector< CombinedMesh* > CombinedMeshVec;

//
// Bone
//

struct Bone
{
    ~Bone();

    uint        NameHash;
    Matrix      TransformationMatrix;
    Matrix      GlobalTransformationMatrix;
    MeshData*   Mesh;
    BoneVec     Children;

    // Runtime data
    Matrix      CombinedTransformationMatrix;

    Bone*       Find( uint name_hash );
    void        Save( File& file );
    void        Load( File& file );
    void        FixAfterLoad( Bone* root_bone );
    static uint GetHash( const string& name );
};

//
// Cut
//

struct CutShape
{
    Matrix          GlobalTransformationMatrix;
    float           SphereRadius;

    static CutShape Make( MeshData* mesh );
};
typedef vector< CutShape > CutShapeVec;

struct CutData
{
    CutShapeVec Shapes;
    IntVec      Layers;
    uint        UnskinBone;
    CutShape    UnskinShape;
    bool        RevertUnskinShape;
};
typedef vector< CutData* > CutDataVec;

//
// MapSprite
//

struct MapSprite
{
    static MapSprite* Factory()
    {
        MapSprite* ms = new MapSprite();
        memzero( ms, sizeof( MapSprite ) );
        ms->RefCount = 1;
        return ms;
    }

    void AddRef()
    {
        ++RefCount;
    }

    void Release()
    {
        if( --RefCount == 0 )
            delete this;
    }

    int    RefCount;

    bool   Valid;
    uint   SprId;
    ushort HexX;
    ushort HexY;
    hash   ProtoId;
    int    FrameIndex;
    int    OffsX;
    int    OffsY;
    bool   IsFlat;
    bool   NoLight;
    int    DrawOrder;
    int    DrawOrderHyOffset;
    int    Corner;
    bool   DisableEgg;
    uint   Color;
    uint   ContourColor;
    bool   IsTweakOffs;
    short  TweakOffsX;
    short  TweakOffsY;
    bool   IsTweakAlpha;
    uchar  TweakAlpha;
};

#endif // __GRAPHIC_STRUCTURES__
