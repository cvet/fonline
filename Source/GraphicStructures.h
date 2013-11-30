#ifndef __GRAPHIC_STRUCTURES__
#define __GRAPHIC_STRUCTURES__

#include "Defines.h"
#include "Assimp/aiTypes.h"
#include "Assimp/aiScene.h"

typedef aiMatrix4x4          Matrix;
typedef aiVector3D           Vector;
typedef aiQuaternion         Quaternion;
typedef aiColor4D            Color;
typedef vector< Vector >     VectorVec;
typedef vector< Quaternion > QuaternionVec;
typedef vector< Matrix >     MatrixVec;
typedef vector< Matrix* >    MatrixPtrVec;

struct Texture
{
    const char* Name;
    GLuint      Id;
    uchar*      Data;
    uint        Size;
    uint        Width;
    uint        Height;
    float       SizeData[ 4 ];        // Width, Height, TexelWidth, TexelHeight
    float       Samples;
    #ifdef FO_WINDOWS
    HPBUFFERARB PBuffer;
    #endif
    Texture(): Name( NULL ), Id( 0 ), Data( NULL ), Size( 0 ), Width( 0 ), Height( 0 ), Samples( 0.0f )
    {
        #ifdef FO_WINDOWS
        PBuffer = NULL;
        #endif
    }
    ~Texture()
    {
        GL( glDeleteTextures( 1, &Id ) );
        SAFEDELA( Data );
    }
    inline uint& Pixel( uint x, uint y ) { return *( (uint*) Data + y * Width + x ); }
    bool         Update()
    {
        GL( glBindTexture( GL_TEXTURE_2D, Id ) );
        GL( glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, Width, Height, GL_BGRA, GL_UNSIGNED_BYTE, Data ) );
        return true;
    }
    bool Update( const Rect& r )
    {
        GL( glBindTexture( GL_TEXTURE_2D, Id ) );
        GL( glTexSubImage2D( GL_TEXTURE_2D, 0, 0, r.T, Width, r.H(), GL_BGRA, GL_UNSIGNED_BYTE, (uint*) Data + r.T * Width ) );
        return true;
    }
};

struct EffectDefault
{
    enum EType { String, Floats, Dword };
    char*  Name;
    EType  Type;
    uchar* Data;
    uint   Size;
};

struct EffectInstance
{
    char*          EffectFilename;
    EffectDefault* Defaults;
    uint           DefaultsCount;
};

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
#define IS_EFFECT_VALUE( pos )                 ( ( pos ) != -1 )
#define SET_EFFECT_VALUE( eff, pos, value )    GL( glUniform1f( pos, value ) )

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

struct MeshSubset
{
    Vertex3DVec    Vertices;
    Vertex3DVec    VerticesTransformed;
    bool           VerticesTransformedValid;
    uint           FacesCount;
    UShortVec      Indicies;
    string         DiffuseTexture;
    float          DiffuseColor[ 4 ];
    float          AmbientColor[ 4 ];
    float          SpecularColor[ 4 ];
    float          EmissiveColor[ 4 ];
    uint           BoneInfluences;
    MatrixVec      BoneOffsets;
    MatrixPtrVec   FrameCombinedMatrixPointer;
    EffectInstance DrawEffect;
    GLuint         VAO, VBO, IBO;
};
typedef vector< MeshSubset > MeshSubsetVec;

struct Frame;
typedef vector< Frame* >     FrameVec;
struct Frame
{
    string        Name;
    Matrix        TransformationMatrix;
    Matrix        CombinedTransformationMatrix;
    MeshSubsetVec Mesh;
    FrameVec      Children;

    const char*   GetName()
    {
        return Name.c_str();
    }

    Frame* Find( const char* name )
    {
        const char* frame_name = Name.c_str();
        if( Str::Compare( frame_name, name ) )
            return this;
        for( uint i = 0; i < Children.size(); i++ )
        {
            Frame* frame = Children[ i ]->Find( name );
            if( frame )
                return frame;
        }
        return NULL;
    }
};

typedef vector< Texture* > TextureVec;
typedef vector< Effect* >  EffectVec;

#endif // __GRAPHIC_STRUCTURES__
