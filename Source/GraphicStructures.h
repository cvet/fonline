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

#ifdef FO_D3D
# define MATRIX_TRANSPOSE( m )    m.Transpose()
#else
# define MATRIX_TRANSPOSE( m )
#endif

struct Texture
{
    const char*        Name;

    #ifdef FO_D3D
    LPDIRECT3DTEXTURE9 Instance;
    Texture(): Name( NULL ), Instance( NULL ) {}
    ~Texture() { SAFEREL( Instance ); }
    #else
    GLuint             Id;
    void*              Data;
    uint               Size;
    uint               Width;
    uint               Height;
    float              SizeData[ 4 ]; // Width, Height, TexelWidth, TexelHeight
    float              Samples;
    Texture(): Name( NULL ), Id( 0 ), Data( NULL ), Size( 0 ), Width( 0 ), Height( 0 ), Samples( 0.0f ) {}
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
    bool Update( const INTRECT& r )
    {
        GL( glBindTexture( GL_TEXTURE_2D, Id ) );
        GL( glTexSubImage2D( GL_TEXTURE_2D, 0, 0, r.T, Width, r.H(), GL_BGRA, GL_UNSIGNED_BYTE, (uint*) Data + r.T * Width ) );
        return true;
    }
    #endif
};

struct EffectDefault
{
    enum EType { String, Floats, Dword };
    char* Name;
    EType Type;
    void* Data;
    uint  Size;
};

struct EffectInstance
{
    char*          EffectFilename;
    EffectDefault* Defaults;
    uint           DefaultsCount;
};

struct Effect
{
    const char*    Name;
    const char*    Defines;

    #ifdef FO_D3D
    LPD3DXEFFECT   DXInstance;
    uint           EffectFlags;
    EffectValue_   EffectParams;
    EffectValue_   TechniqueSkinned;
    EffectValue_   TechniqueSkinnedWithShadow;
    EffectValue_   TechniqueSimple;
    EffectValue_   TechniqueSimpleWithShadow;
    EffectValue_   LightDir;
    EffectValue_   LightDiffuse;
    #else
    GLuint         Program;
    GLuint         VertexShader;
    GLuint         FragmentShader;
    uint           Passes;
    GLint          ZoomFactor;
    GLint          ColorMap;
    GLint          ColorMapSize;
    GLint          ColorMapSamples;
    GLint          EggMap;
    GLint          EggMapSize;
    # ifdef SHADOW_MAP
    GLint          ShadowMap;
    GLint          ShadowMapSize;
    GLint          ShadowMapSamples;
    GLint          ShadowMapMatrix;
    # endif
    GLint          SpriteBorder;
    #endif

    EffectDefault* Defaults;
    EffectValue_   ProjectionMatrix;
    EffectValue_   BoneInfluences;
    EffectValue_   GroundPosition;
    EffectValue_   MaterialAmbient;
    EffectValue_   MaterialDiffuse;
    EffectValue_   WorldMatrices;
    EffectValue_   WorldMatrix;

    // Automatic variables
    bool           IsNeedProcess;
    EffectValue_   PassIndex;
    bool           IsTime;
    EffectValue_   Time;
    float          TimeCurrent;
    double         TimeLastTick;
    EffectValue_   TimeGame;
    float          TimeGameCurrent;
    double         TimeGameLastTick;
    bool           IsRandomPass;
    EffectValue_   Random1Pass;
    EffectValue_   Random2Pass;
    EffectValue_   Random3Pass;
    EffectValue_   Random4Pass;
    bool           IsRandomEffect;
    EffectValue_   Random1Effect;
    EffectValue_   Random2Effect;
    EffectValue_   Random3Effect;
    EffectValue_   Random4Effect;
    bool           IsTextures;
    EffectValue_   Textures[ EFFECT_TEXTURES ];
    #ifndef FO_D3D
    EffectValue_   TexturesSize[ EFFECT_TEXTURES ];
    #endif
    bool           IsScriptValues;
    EffectValue_   ScriptValues[ EFFECT_SCRIPT_VALUES ];
    bool           IsAnimPos;
    EffectValue_   AnimPosProc;
    EffectValue_   AnimPosTime;
};
#ifdef FO_D3D
# define IS_EFFECT_VALUE( pos )                 ( ( pos ) != NULL )
# define SET_EFFECT_VALUE( eff, pos, value )    eff->DXInstance->SetFloat( pos, value )
#else
# define IS_EFFECT_VALUE( pos )                 ( ( pos ) != -1 )
# define SET_EFFECT_VALUE( eff, pos, value )    GL( glUniform1f( pos, value ) )
#endif

#ifdef FO_D3D
struct MeshContainer
{

    ID3DXMesh*      InitMesh;
    ID3DXMesh*      SkinMesh;
    ID3DXMesh*      SkinMeshBlended;
    Material_*      Materials;
    EffectInstance* Effects;
    uint            NumMaterials;
    uint*           Adjacency;
    SkinInfo_       Skin;
    char**          TextureNames;
    Matrix*         BoneOffsets;
    Matrix**        FrameCombinedMatrixPointer;
    ID3DXBuffer*    BoneCombinationBuf;
    uint            NumAttributeGroups;
    uint            NumPaletteEntries;
    uint            NumInfluences;
};
typedef vector< MeshContainer* > MeshContainerVec;

struct Frame
{
    char*          Name;
    Matrix         TransformationMatrix;
    Matrix         CombinedTransformationMatrix;
    MeshContainer* DrawMesh;
    Frame*         Sibling;
    Frame*         FirstChild;

    const char*    GetName()
    {
        return Name;
    }

    Frame* Find( const char* name )
    {
        if( Name && Str::Compare( Name, name ) )
            return this;
        Frame* frame = NULL;
        if( Sibling )
            frame = Sibling->Find( name );
        if( !frame && FirstChild )
            frame = FirstChild->Find( name );
        return frame;
    }
};
typedef vector< Frame* > FrameVec;
typedef ID3DXMesh        MeshSubset;
#else
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
#endif

typedef vector< Texture* > TextureVec;
typedef vector< Effect* >  EffectVec;

#endif // __GRAPHIC_STRUCTURES__
