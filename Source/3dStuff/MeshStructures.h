#ifndef __MESH_STRUCTURES__
#define __MESH_STRUCTURES__

#include "Defines.h"

struct TextureEx
{
	const char* Name;
	IDirect3DTexture9* Texture;
};

struct EffectEx
{
	const char* Name;
	ID3DXEffect* Effect;
	DWORD EffectFlags;
	LPD3DXEFFECTDEFAULT Defaults;
	D3DXHANDLE EffectParams;
	D3DXHANDLE TechniqueSkinned;
	D3DXHANDLE TechniqueSkinnedWithShadow;
	D3DXHANDLE TechniqueSimple;
	D3DXHANDLE TechniqueSimpleWithShadow;
	D3DXHANDLE BonesInfluences;
	D3DXHANDLE GroundPosition;
	D3DXHANDLE LightDir;
	D3DXHANDLE LightDiffuse;
	D3DXHANDLE MaterialAmbient;
	D3DXHANDLE MaterialDiffuse;
	D3DXHANDLE WorldMatrices;
	D3DXHANDLE ViewProjMatrix;

	// Automatic variables
	bool IsNeedProcess;
	D3DXHANDLE PassIndex;
	bool IsTime;
	D3DXHANDLE Time;
	float TimeCurrent;
	double TimeLastTick;
	D3DXHANDLE TimeGame;
	float TimeGameCurrent;
	double TimeGameLastTick;
	bool IsRandomPass;
	D3DXHANDLE Random1Pass;
	D3DXHANDLE Random2Pass;
	D3DXHANDLE Random3Pass;
	D3DXHANDLE Random4Pass;
	bool IsRandomEffect;
	D3DXHANDLE Random1Effect;
	D3DXHANDLE Random2Effect;
	D3DXHANDLE Random3Effect;
	D3DXHANDLE Random4Effect;
	bool IsTextures;
	D3DXHANDLE Textures[EFFECT_TEXTURES];
	bool IsScriptValues;
	D3DXHANDLE ScriptValues[EFFECT_SCRIPT_VALUES];
};

struct D3DXMESHCONTAINER_EXTENDED: public D3DXMESHCONTAINER
{
	// Material
	char**              exTexturesNames;
	D3DMATERIAL9*       exMaterials;                  // Array of materials

	// Effect
	D3DXEFFECTINSTANCE* exEffects;

	// Skinned mesh variables
	ID3DXMesh*          exSkinMesh;                   // The skin mesh
	D3DXMATRIX*         exBoneOffsets;                // The bone matrix Offsets, one per bone
	D3DXMATRIX**        exFrameCombinedMatrixPointer; // Array of frame matrix pointers

	// Used for indexed shader skinning
	ID3DXMesh*          exSkinMeshBlended;            // The blended skin mesh
	ID3DXBuffer*        exBoneCombinationBuf;
	DWORD               exNumAttributeGroups;
	DWORD               exNumPaletteEntries;
	DWORD               exNumInfl;
};

struct FrameEx: public D3DXFRAME
{
	const char* exFileName;
    D3DXMATRIX exCombinedTransformationMatrix;
};

typedef vector<D3DXMESHCONTAINER_EXTENDED*> MeshContainerVec;
typedef vector<D3DXMESHCONTAINER_EXTENDED*>::iterator MeshContainerVecIt;
typedef vector<FrameEx*> FrameVec;
typedef vector<FrameEx*>::iterator FrameVecIt;
typedef vector<D3DXVECTOR3> Vector3Vec;
typedef vector<D3DXVECTOR3>::iterator Vector3VecIt;
typedef vector<D3DXMATRIX> MatrixVec;
typedef vector<D3DXMATRIX>::iterator MatrixVecIt;

typedef vector<TextureEx*> TextureExVec;
typedef vector<TextureEx*>::iterator TextureExVecIt;
typedef vector<EffectEx*> EffectExVec;
typedef vector<EffectEx*>::iterator EffectExVecIt;

typedef D3DXFRAME Frame;
typedef FrameEx FrameEx;

typedef ID3DXAnimationSet AnimSet;
typedef vector<AnimSet*> AnimSetVec;
typedef vector<AnimSet*>::iterator AnimSetVecIt;

#endif // __MESH_STRUCTURES__