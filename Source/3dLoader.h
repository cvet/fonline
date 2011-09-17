#ifndef __3D_LOADER__
#define __3D_LOADER__

#include "Common.h"
#include "Defines.h"
#include "FileManager.h"
#include "3dMeshHierarchy.h"
#include "3dMeshStructures.h"

struct aiNode;
struct aiScene;

class Loader3d
{
	// Models
public:
	static FrameEx* LoadModel(IDirect3DDevice9* device, const char* fname, bool calc_tangent);
	static AnimSet* LoadAnimation(IDirect3DDevice9* device, const char* anim_fname, const char* anim_name);
	static void Free(D3DXFRAME* frame);
	static bool IsExtensionSupported(const char* ext);

private:
	static MeshHierarchy memAllocator;
	static PCharVec processedFiles;
	static FrameVec loadedModels;
	static PCharVec loadedAnimationsFNames;
	static AnimSetVec loadedAnimations;

	static FrameEx* FillNode(IDirect3DDevice9* device, const aiNode* node, const aiScene* scene, bool with_tangent);
	static FrameEx* LoadX(IDirect3DDevice9* device, FileManager& fm, const char* fname);

	// Textures
public:
	static TextureEx* LoadTexture(IDirect3DDevice9* device, const char* texture_name, const char* model_path);
	static void FreeTexture(TextureEx* texture); // If texture is NULL than free all textures

private:
	static TextureExVec loadedTextures;

	// Effects
public:
	static EffectEx* LoadEffect(IDirect3DDevice9* device, const char* effect_name);
	static EffectEx* LoadEffect(IDirect3DDevice9* device, D3DXEFFECTINSTANCE* effect_inst, const char* model_path);
	static void EffectProcessVariables(EffectEx* effect_ex, int pass, float anim_proc = 0.0f, float anim_time = 0.0f, TextureEx** textures = NULL);
	static bool EffectsPreRestore();
	static bool EffectsPostRestore();

private:
	static EffectExVec loadedEffects;
};

#endif // __3D_LOADER__

