#ifndef __MESH_STRUCTURES__
#define __MESH_STRUCTURES__

#include "Defines.h"

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
    Texture(): Name( NULL ), Id( 0 ), Data( NULL ), Size( 0 ), Width( 0 ), Height( 0 ) {}
    ~Texture()
    {
        GL( glDeleteTextures( 1, &Id ) );
        SAFEDELA( Data );
    }
    inline uint& Pixel( uint x, uint y ) { return *( (uint*) Data + y * Width + x ); }
    bool         Update()
    {
        GL( glBindTexture( GL_TEXTURE_2D, Id ) );
        GL( glTexImage2D( GL_TEXTURE_2D, 0, 4, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, Data ) );
        return true;
    }
    bool Update( const INTRECT& r )
    {
        GL( glBindTexture( GL_TEXTURE_2D, Id ) );
        GL( glTexSubImage2D( GL_TEXTURE_2D, 0, 0, r.T, Width, r.H(), GL_RGBA, GL_UNSIGNED_BYTE, (uint*) Data + r.T * Width ) );
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

    #ifdef FO_D3D
    LPD3DXEFFECT   DXInstance;
    uint           EffectFlags;
    EffectValue_   EffectParams;
    EffectValue_   TechniqueSkinned;
    EffectValue_   TechniqueSkinnedWithShadow;
    EffectValue_   TechniqueSimple;
    EffectValue_   TechniqueSimpleWithShadow;
    #else
    GLuint         Program;
    GLuint         VertexShader;
    GLuint         FragmentShader;
    uint           Passes;
    GLint          ColorMap;
    GLint          EggMap;
    #endif

    EffectDefault* Defaults;
    EffectValue_   BonesInfluences;
    EffectValue_   GroundPosition;
    EffectValue_   LightDir;
    EffectValue_   LightDiffuse;
    EffectValue_   MaterialAmbient;
    EffectValue_   MaterialDiffuse;
    EffectValue_   WorldMatrices;
    EffectValue_   ViewProjMatrix;

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
    bool           IsScriptValues;
    EffectValue_   ScriptValues[ EFFECT_SCRIPT_VALUES ];
    bool           IsAnimPos;
    EffectValue_   AnimPosProc;
    EffectValue_   AnimPosTime;
};

struct SkinInfo
{
    uint Dummy;
};

struct AnimController
{
    uint Dummy;
    void Release() {}
};

struct MeshContainer
{
    char* Name;
    struct
    {
        int   Type; // Exclude
        Mesh_ Mesh;
    } MeshData;
    Material_*      Materials;
    EffectInstance* Effects;
    uint            NumMaterials;
    uint*           Adjacency;
    SkinInfo_       SkinInfo;
    MeshContainer*  NextMeshContainer;

    // Material
    char**          TextureNames;

    // Skinned mesh variables
    Mesh_           SkinMesh;                      // The skin mesh
    Matrix_*        BoneOffsets;                   // The bone matrix Offsets, one per bone
    Matrix_**       FrameCombinedMatrixPointer;    // Array of frame matrix pointers

    // Used for indexed shader skinning
    Mesh_           SkinMeshBlended;               // The blended skin mesh
    Buffer_*        BoneCombinationBuf;
    uint            NumAttributeGroups;
    uint            NumPaletteEntries;
    uint            NumInfluences;
};

struct Frame
{
    char*          Name;
    Matrix_        TransformationMatrix;
    MeshContainer* Meshes;
    Frame*         Sibling;
    Frame*         FirstChild;
    Matrix_        CombinedTransformationMatrix;
};

typedef vector< MeshContainer* > MeshContainerVec;
typedef vector< Frame* >         FrameVec;
typedef vector< Vector3_ >       Vector3Vec;
typedef vector< Matrix_ >        MatrixVec;
typedef vector< Texture* >       TextureVec;
typedef vector< Effect* >        EffectVec;
typedef vector< AnimSet_* >      AnimSetVec;

#endif // __MESH_STRUCTURES__
