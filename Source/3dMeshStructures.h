#ifndef __MESH_STRUCTURES__
#define __MESH_STRUCTURES__

#include "Defines.h"

struct TextureEx
{
    const char* Name;
    TextureType Texture;
};

struct EffectEx
{
    const char*        Name;
    EffectType         Effect;
    uint               EffectFlags;
    EffectDefaultsType Defaults;
    EffectValueType    EffectParams;
    EffectValueType    TechniqueSkinned;
    EffectValueType    TechniqueSkinnedWithShadow;
    EffectValueType    TechniqueSimple;
    EffectValueType    TechniqueSimpleWithShadow;
    EffectValueType    BonesInfluences;
    EffectValueType    GroundPosition;
    EffectValueType    LightDir;
    EffectValueType    LightDiffuse;
    EffectValueType    MaterialAmbient;
    EffectValueType    MaterialDiffuse;
    EffectValueType    WorldMatrices;
    EffectValueType    ViewProjMatrix;

    // Automatic variables
    bool               IsNeedProcess;
    EffectValueType    PassIndex;
    bool               IsTime;
    EffectValueType    Time;
    float              TimeCurrent;
    double             TimeLastTick;
    EffectValueType    TimeGame;
    float              TimeGameCurrent;
    double             TimeGameLastTick;
    bool               IsRandomPass;
    EffectValueType    Random1Pass;
    EffectValueType    Random2Pass;
    EffectValueType    Random3Pass;
    EffectValueType    Random4Pass;
    bool               IsRandomEffect;
    EffectValueType    Random1Effect;
    EffectValueType    Random2Effect;
    EffectValueType    Random3Effect;
    EffectValueType    Random4Effect;
    bool               IsTextures;
    EffectValueType    Textures[ EFFECT_TEXTURES ];
    bool               IsScriptValues;
    EffectValueType    ScriptValues[ EFFECT_SCRIPT_VALUES ];
    bool               IsAnimPos;
    EffectValueType    AnimPosProc;
    EffectValueType    AnimPosTime;
};

#ifdef FO_D3D
struct D3DXMESHCONTAINER_EXTENDED: public D3DXMESHCONTAINER
#else
struct D3DXMESHCONTAINER_EXTENDED
#endif
{
    // Material
    char**              exTexturesNames;
    MaterialType*       exMaterials;                      // Array of materials

    // Effect
    EffectInstanceType* exEffects;

    // Skinned mesh variables
    MeshType            exSkinMesh;                     // The skin mesh
    MatrixType*         exBoneOffsets;                  // The bone matrix Offsets, one per bone
    MatrixType**        exFrameCombinedMatrixPointer;   // Array of frame matrix pointers

    // Used for indexed shader skinning
    MeshType            exSkinMeshBlended;              // The blended skin mesh
    ID3DXBuffer*        exBoneCombinationBuf;
    DWORD               exNumAttributeGroups;
    DWORD               exNumPaletteEntries;
    DWORD               exNumInfl;
};

struct FrameEx: public D3DXFRAME
{
    const char* exFileName;
    MatrixType  exCombinedTransformationMatrix;
};

typedef vector< D3DXMESHCONTAINER_EXTENDED* > MeshContainerVec;
typedef vector< FrameEx* >                    FrameVec;
typedef vector< D3DXVECTOR3 >                 Vector3Vec;
typedef vector< MatrixType >                  MatrixVec;

typedef vector< TextureEx* >                  TextureExVec;
typedef vector< EffectEx* >                   EffectExVec;

typedef D3DXFRAME                             Frame;
typedef FrameEx                               FrameEx;

typedef ID3DXAnimationSet                     AnimSet;
typedef vector< AnimSet* >                    AnimSetVec;

#endif // __MESH_STRUCTURES__
