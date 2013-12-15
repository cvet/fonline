#ifndef __GRAPHIC_STRUCTURES__
#define __GRAPHIC_STRUCTURES__

#include "Defines.h"
#include "Assimp/aiTypes.h"
#include "Assimp/aiScene.h"
#include "FileManager.h"

typedef aiMatrix4x4          Matrix;
typedef aiVector3D           Vector;
typedef aiQuaternion         Quaternion;
typedef aiColor4D            Color;
typedef vector< Vector >     VectorVec;
typedef vector< Quaternion > QuaternionVec;
typedef vector< Matrix >     MatrixVec;
typedef vector< Matrix* >    MatrixPtrVec;

//
// Vertex
//

struct Vertex
{
    float x, y;
    uint  diffuse;
    float tu, tv;
    #ifndef DISABLE_EGG
    float tu2, tv2;
    #endif
};
typedef vector< Vertex > VertexVec;

//
// Vertex3D
//

struct Vertex3D
{
    Vector Position;
    float  PositionW;
    Vector Normal;
    float  Color[ 4 ];
    float  TexCoord[ 2 ];
    float  TexCoord2[ 2 ];
    float  TexCoord3[ 2 ];
    Vector Tangent;
    Vector Bitangent;
    float  BlendWeights[ 4 ];
    float  BlendIndices[ 4 ];
    float  Padding[ 1 ];
};
static_assert( sizeof( Vertex3D ) == 128, "Wrong Vertex3D size." );
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

    // Disable constructors to avoid unnecessary calls
private:
    AnyFrames() {}
    ~AnyFrames() {}
};
typedef map< uint, AnyFrames*, less< uint > > AnimMap;
typedef vector< AnyFrames* >                  AnimVec;

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

    PrepPoint(): PointX( 0 ), PointY( 0 ), PointColor( 0 ), PointOffsX( NULL ), PointOffsY( NULL ) {}
    PrepPoint( short x, short y, uint color, short* ox = NULL, short* oy = NULL ): PointX( x ), PointY( y ), PointColor( color ), PointOffsX( ox ), PointOffsY( oy ) {}
};
typedef vector< PrepPoint > PointVec;
typedef vector< PointVec >  PointVecVec;

//
// EffectDefault
//

struct EffectDefault
{
    enum EType { String, Floats, Dword };
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
struct Effect
{
    int            Id;
    const char*    Name;
    const char*    Defines;

    GLuint         Program;
    uint           Passes;
    GLint          ZoomFactor;
    GLint          ColorMap;
    GLint          ColorMapSize;
    GLint          ColorMapAtlasOffset;
    GLint          ColorMapSamples;
    GLint          EggMap;
    GLint          EggMapSize;
    #ifdef SHADOW_MAP
    GLint          ShadowMap;
    GLint          ShadowMapSize;
    GLint          ShadowMapSamples;
    GLint          ShadowMapMatrix;
    #endif
    GLint          SpriteBorder;

    EffectDefault* Defaults;
    GLint          ProjectionMatrix;
    GLint          BoneInfluences;
    GLint          GroundPosition;
    GLint          MaterialAmbient;
    GLint          MaterialDiffuse;
    GLint          WorldMatrices;
    GLint          WorldMatrix;

    // Automatic variables
    bool           IsNeedProcess;
    GLint          PassIndex;
    bool           IsTime;
    GLint          Time;
    float          TimeCurrent;
    double         TimeLastTick;
    GLint          TimeGame;
    float          TimeGameCurrent;
    double         TimeGameLastTick;
    bool           IsRandomPass;
    GLint          Random1Pass;
    GLint          Random2Pass;
    GLint          Random3Pass;
    GLint          Random4Pass;
    bool           IsRandomEffect;
    GLint          Random1Effect;
    GLint          Random2Effect;
    GLint          Random3Effect;
    GLint          Random4Effect;
    bool           IsTextures;
    GLint          Textures[ EFFECT_TEXTURES ];
    GLint          TexturesSize[ EFFECT_TEXTURES ];
    GLint          TexturesAtlasOffset[ EFFECT_TEXTURES ];
    bool           IsScriptValues;
    GLint          ScriptValues[ EFFECT_SCRIPT_VALUES ];
    bool           IsAnimPos;
    GLint          AnimPosProc;
    GLint          AnimPosTime;

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
    static Effect* FlushRenderTarget, * FlushRenderTargetDefault;
    static Effect* FlushRenderTargetMS, * FlushRenderTargetMSDefault;
    static Effect* FlushPrimitive, * FlushPrimitiveDefault;
    static Effect* FlushMap, * FlushMapDefault;
    static Effect* Font, * FontDefault;
    static Effect* Simple3d, * Simple3dDefault;
    static Effect* Skinned3d, * Skinned3dDefault;
    static Effect* Simple3dShadow, * Simple3dShadowDefault;
    static Effect* Skinned3dShadow, * Skinned3dShadowDefault;
};
typedef vector< Effect* > EffectVec;

//
// Texture
//

struct Texture
{
    const char*  Name;
    GLuint       Id;
    uchar*       Data;
    uint         Size;
    uint         Width;
    uint         Height;
    float        SizeData[ 4 ];       // Width, Height, TexelWidth, TexelHeight
    float        Samples;

    Texture();
    ~Texture();
    bool         Update();
    bool         UpdateRegion( const Rect& r );
    inline uint& Pixel( uint x, uint y ) { return *( (uint*) Data + y * Width + x ); }
};
typedef vector< Texture* > TextureVec;

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

    int        Type;
    bool       Finalized;
    Texture*   TextureOwner;
    uint       Width, Height;
    SpaceNode* RootNode;                                  // Packer 1
    uint       CurX, CurY, LineMaxH, LineCurH, LineW;     // Packer 2

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
    Animation3d*  Anim3d;
    uchar*        Data;
    uint          DataSize;
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
// Render target
//

struct RenderTarget
{
    GLuint   FBO;
    Texture* TargetTexture;
    GLuint   DepthStencilBuffer;
    Effect*  DrawEffect;
};
typedef vector< RenderTarget* > RenderTargetVec;

//
// MeshSubset
//

struct MeshSubset
{
    Vertex3DVec    Vertices;
    UShortVec      Indicies;
    string         DiffuseTexture;
    float          DiffuseColor[ 4 ];
    float          AmbientColor[ 4 ];
    float          SpecularColor[ 4 ];
    float          EmissiveColor[ 4 ];
    uint           BoneInfluences;
    MatrixVec      BoneOffsets;
    StrVec         BoneNames;

    // Runtime data
    Vertex3DVec    VerticesTransformed;
    bool           VerticesTransformedValid;
    MatrixPtrVec   BoneCombinedMatrices;
    EffectInstance DrawEffect;
    GLuint         VAO, VBO, IBO;

    void Save( FileManager& file );
    void Load( FileManager& file );
};
typedef vector< MeshSubset > MeshSubsetVec;

//
// Node
//

struct Node;
typedef vector< Node* > NodeVec;
struct Node
{
    string        Name;
    Matrix        TransformationMatrix;
    MeshSubsetVec Mesh;

    // Runtime data
    NodeVec       Children;
    Matrix        CombinedTransformationMatrix;
    Vector        ScreenPos;

    const char* GetName();
    Node*       Find( const char* name );
    void        Save( FileManager& file );
    void        Load( FileManager& file );
    void        FixAfterLoad( Node* root_node );
};

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

#endif // __GRAPHIC_STRUCTURES__
