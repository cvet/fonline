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
    Texture(): Name( NULL ), Id( 0 ), Data( NULL ), Size( 0 ), Width( 0 ), Height( 0 ), Samples( 0.0f )
    {}
    ~Texture()
    {
        GL( glDeleteTextures( 1, &Id ) );
        SAFEDELA( Data );
    }
    inline uint& Pixel( uint x, uint y ) { return *( (uint*) Data + y * Width + x ); }
    bool         Update()
    {
        GL( glBindTexture( GL_TEXTURE_2D, Id ) );
        GL( glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, Width, Height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, Data ) );
        return true;
    }
    bool Update( const Rect& r )
    {
        GL( glBindTexture( GL_TEXTURE_2D, Id ) );
        GL( glTexSubImage2D( GL_TEXTURE_2D, 0, 0, r.T, Width, r.H(), GL_BGRA_EXT, GL_UNSIGNED_BYTE, (uint*) Data + r.T * Width ) );
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
    MatrixPtrVec   FrameCombinedMatrixPointer;
    EffectInstance DrawEffect;
    GLuint         VAO, VBO, IBO;

    // Methods
    void           CreateBuffers()
    {
        GL( glGenBuffers( 1, &VBO ) );
        GL( glBindBuffer( GL_ARRAY_BUFFER, VBO ) );
        GL( glBufferData( GL_ARRAY_BUFFER, Vertices.size() * sizeof( Vertex3D ), &Vertices[ 0 ], GL_STATIC_DRAW ) );
        GL( glGenBuffers( 1, &IBO ) );
        GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, IBO ) );
        GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, Indicies.size() * sizeof( short ), &Indicies[ 0 ], GL_STATIC_DRAW ) );
        VAO = 0;
        if( GL_HAS( ARB_vertex_array_object ) && ( GL_HAS( ARB_framebuffer_object ) || GL_HAS( EXT_framebuffer_object ) ) )
        {
            GL( glGenVertexArrays( 1, &VAO ) );
            GL( glBindVertexArray( VAO ) );
            GL( glVertexAttribPointer( 0, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Position ) ) );
            GL( glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Normal ) ) );
            GL( glVertexAttribPointer( 2, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Color ) ) );
            GL( glVertexAttribPointer( 3, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoord ) ) );
            GL( glVertexAttribPointer( 4, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoord2 ) ) );
            GL( glVertexAttribPointer( 5, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoord3 ) ) );
            GL( glVertexAttribPointer( 6, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Tangent ) ) );
            GL( glVertexAttribPointer( 7, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Bitangent ) ) );
            GL( glVertexAttribPointer( 8, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, BlendWeights ) ) );
            GL( glVertexAttribPointer( 9, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, BlendIndices ) ) );
            for( uint i = 0; i <= 9; i++ )
                GL( glEnableVertexAttribArray( i ) );
            GL( glBindVertexArray( 0 ) );
        }
    }

    void Save( FileManager& file )
    {
        uint len = (uint) Vertices.size();
        file.SetData( &len, sizeof( len ) );
        file.SetData( &Vertices[ 0 ], len * sizeof( Vertices[ 0 ] ) );
        len = (uint) Indicies.size();
        file.SetData( &len, sizeof( len ) );
        file.SetData( &Indicies[ 0 ], len * sizeof( Indicies[ 0 ] ) );
        len = (uint) DiffuseTexture.length();
        file.SetData( &len, sizeof( len ) );
        file.SetData( &DiffuseTexture[ 0 ], len );
        file.SetData( DiffuseColor, sizeof( DiffuseColor ) );
        file.SetData( AmbientColor, sizeof( AmbientColor ) );
        file.SetData( SpecularColor, sizeof( SpecularColor ) );
        file.SetData( EmissiveColor, sizeof( EmissiveColor ) );
        file.SetData( &BoneInfluences, sizeof( BoneInfluences ) );
        len = (uint) BoneOffsets.size();
        file.SetData( &len, sizeof( len ) );
        file.SetData( &BoneOffsets[ 0 ], len * sizeof( BoneOffsets[ 0 ] ) );
        len = (uint) BoneNames.size();
        file.SetData( &len, sizeof( len ) );
        for( uint i = 0, j = len; i < j; i++ )
        {
            len = (uint) BoneNames[ i ].length();
            file.SetData( &len, sizeof( len ) );
            file.SetData( &BoneNames[ i ][ 0 ], len );
        }
    }

    void Load( FileManager& file )
    {
        uint len = 0;
        file.CopyMem( &len, sizeof( len ) );
        Vertices.resize( len );
        file.CopyMem( &Vertices[ 0 ], len * sizeof( Vertices[ 0 ] ) );
        file.CopyMem( &len, sizeof( len ) );
        Indicies.resize( len );
        file.CopyMem( &Indicies[ 0 ], len * sizeof( Indicies[ 0 ] ) );
        file.CopyMem( &len, sizeof( len ) );
        DiffuseTexture.resize( len );
        file.CopyMem( &DiffuseTexture[ 0 ], len );
        file.CopyMem( DiffuseColor, sizeof( DiffuseColor ) );
        file.CopyMem( AmbientColor, sizeof( AmbientColor ) );
        file.CopyMem( SpecularColor, sizeof( SpecularColor ) );
        file.CopyMem( EmissiveColor, sizeof( EmissiveColor ) );
        file.CopyMem( &BoneInfluences, sizeof( BoneInfluences ) );
        file.CopyMem( &len, sizeof( len ) );
        BoneOffsets.resize( len );
        file.CopyMem( &BoneOffsets[ 0 ], len * sizeof( BoneOffsets[ 0 ] ) );
        file.CopyMem( &len, sizeof( len ) );
        BoneNames.resize( len );
        for( uint i = 0, j = len; i < j; i++ )
        {
            file.CopyMem( &len, sizeof( len ) );
            BoneNames[ i ].resize( len );
            file.CopyMem( &BoneNames[ i ][ 0 ], len );
        }
        VerticesTransformed = Vertices;
        VerticesTransformedValid = false;
        FrameCombinedMatrixPointer.resize( BoneOffsets.size() );
        memzero( &DrawEffect, sizeof( DrawEffect ) );
        VAO = VBO = IBO = 0;
    }
};
typedef vector< MeshSubset > MeshSubsetVec;

struct Frame;
typedef vector< Frame* >     FrameVec;
struct Frame
{
    string        Name;
    Matrix        TransformationMatrix;
    MeshSubsetVec Mesh;

    // Runtime data
    FrameVec      Children;
    Matrix        CombinedTransformationMatrix;
    uint          ParticleEmitter;

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

    void Save( FileManager& file )
    {
        uint len = (uint) Name.length();
        file.SetData( &len, sizeof( len ) );
        file.SetData( &Name[ 0 ], len );
        file.SetData( &TransformationMatrix, sizeof( TransformationMatrix ) );
        len = (uint) Mesh.size();
        file.SetData( &len, sizeof( len ) );
        for( uint i = 0, j = len; i < j; i++ )
            Mesh[ i ].Save( file );
        len = (uint) Children.size();
        file.SetData( &len, sizeof( len ) );
        for( uint i = 0, j = len; i < j; i++ )
            Children[ i ]->Save( file );
    }

    void Load( FileManager& file )
    {
        uint len = 0;
        file.CopyMem( &len, sizeof( len ) );
        Name.resize( len );
        file.CopyMem( &Name[ 0 ], len );
        file.CopyMem( &TransformationMatrix, sizeof( TransformationMatrix ) );
        file.CopyMem( &len, sizeof( len ) );
        Mesh.resize( len );
        for( uint i = 0, j = len; i < j; i++ )
            Mesh[ i ].Load( file );
        file.CopyMem( &len, sizeof( len ) );
        Children.resize( len );
        for( uint i = 0, j = len; i < j; i++ )
        {
            Children[ i ] = new Frame();
            Children[ i ]->Load( file );
        }
        CombinedTransformationMatrix = Matrix();
    }

    void FixAfterLoad( Frame* root_frame )
    {
        for( auto it = Mesh.begin(), end = Mesh.end(); it != end; ++it )
        {
            MeshSubset& mesh = *it;
            for( uint i = 0, j = (uint) mesh.BoneNames.size(); i < j; i++ )
            {
                Frame* bone_frame = root_frame->Find( mesh.BoneNames[ i ].c_str() );
                if( bone_frame )
                    mesh.FrameCombinedMatrixPointer[ i ] = &bone_frame->CombinedTransformationMatrix;
                else
                    mesh.FrameCombinedMatrixPointer[ i ] = NULL;
            }
            mesh.CreateBuffers();
        }

        for( uint i = 0, j = (uint) Children.size(); i < j; i++ )
            Children[ i ]->FixAfterLoad( root_frame );
    }
};

typedef vector< Texture* > TextureVec;
typedef vector< Effect* >  EffectVec;

#endif // __GRAPHIC_STRUCTURES__
