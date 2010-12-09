#ifndef __3D_LOADER__
#define __3D_LOADER__

#include "Common.h"
#include "Defines.h"
#include "FileManager.h"
#include "MeshHierarchy.h"
#include "MeshStructures.h"


class Loader3d
{
	// Models
public:
	static D3DXFRAME* Load(IDirect3DDevice9* device, const char* fname, int path_type, ID3DXAnimationController** anim);
	static void Free(D3DXFRAME* frame);

private:
	static MeshHierarchy memAllocator;

	static D3DXFRAME* Load_X(FileManager& fm, IDirect3DDevice9* device, ID3DXAnimationController** anim);
	static D3DXFRAME* Load_3ds(FileManager& fm, IDirect3DDevice9* device);

	// Textures
public:
	static TextureEx* LoadTexture(IDirect3DDevice9* device, const char* texture_name, const char* model_path, int model_path_type);
	static void FreeTexture(TextureEx* texture); // If texture is NULL than free all textures

private:
	static TextureExVec loadedTextures;

	// Effects
public:
	static EffectEx* LoadEffect(IDirect3DDevice9* device, const char* effect_name);
	static EffectEx* LoadEffect(IDirect3DDevice9* device, D3DXEFFECTINSTANCE* effect_inst, const char* model_path, int model_path_type);
	static void EffectProcessVariables(EffectEx* effect_ex, int pass, TextureEx** textures = NULL);
	static bool EffectsPreRestore();
	static bool EffectsPostRestore();

private:
	static EffectExVec loadedEffects;
};

#endif // __3D_LOADER__

