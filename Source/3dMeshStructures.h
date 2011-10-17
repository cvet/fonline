#ifndef __MESH_STRUCTURES__
#define __MESH_STRUCTURES__

#include "Defines.h"

struct Texture
{
    const char* Name;
    Texture_    TextureInstance;
};

struct Effect
{
    const char*     Name;
    Effect_         EffectInstance;
    uint            EffectFlags;
    EffectDefaults_ Defaults;
    EffectValue_    EffectParams;
    EffectValue_    TechniqueSkinned;
    EffectValue_    TechniqueSkinnedWithShadow;
    EffectValue_    TechniqueSimple;
    EffectValue_    TechniqueSimpleWithShadow;
    EffectValue_    BonesInfluences;
    EffectValue_    GroundPosition;
    EffectValue_    LightDir;
    EffectValue_    LightDiffuse;
    EffectValue_    MaterialAmbient;
    EffectValue_    MaterialDiffuse;
    EffectValue_    WorldMatrices;
    EffectValue_    ViewProjMatrix;

    // Automatic variables
    bool            IsNeedProcess;
    EffectValue_    PassIndex;
    bool            IsTime;
    EffectValue_    Time;
    float           TimeCurrent;
    double          TimeLastTick;
    EffectValue_    TimeGame;
    float           TimeGameCurrent;
    double          TimeGameLastTick;
    bool            IsRandomPass;
    EffectValue_    Random1Pass;
    EffectValue_    Random2Pass;
    EffectValue_    Random3Pass;
    EffectValue_    Random4Pass;
    bool            IsRandomEffect;
    EffectValue_    Random1Effect;
    EffectValue_    Random2Effect;
    EffectValue_    Random3Effect;
    EffectValue_    Random4Effect;
    bool            IsTextures;
    EffectValue_    Textures[ EFFECT_TEXTURES ];
    bool            IsScriptValues;
    EffectValue_    ScriptValues[ EFFECT_SCRIPT_VALUES ];
    bool            IsAnimPos;
    EffectValue_    AnimPosProc;
    EffectValue_    AnimPosTime;
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
    Material_*       Materials;
    EffectInstance_* Effects;
    uint             NumMaterials;
    uint*            Adjacency;
    SkinInfo_        SkinInfo;
    MeshContainer*   NextMeshContainer;

    // Material
    char**           TextureNames;

    // Skinned mesh variables
    Mesh_            SkinMesh;                     // The skin mesh
    Matrix_*         BoneOffsets;                  // The bone matrix Offsets, one per bone
    Matrix_**        FrameCombinedMatrixPointer;   // Array of frame matrix pointers

    // Used for indexed shader skinning
    Mesh_            SkinMeshBlended;              // The blended skin mesh
    Buffer_*         BoneCombinationBuf;
    uint             NumAttributeGroups;
    uint             NumPaletteEntries;
    uint             NumInfluences;
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
typedef vector< Effect* >        EffectExVec;
typedef vector< AnimSet_* >      AnimSetVec;

#endif // __MESH_STRUCTURES__
