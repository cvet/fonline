#ifndef __3D_LOADER__
#define __3D_LOADER__

#include "common.h"
#include "Defines.h"
#include "FileManager.h"
#include "MeshHierarchy.h"
#include "MeshStructures.h"


class Loader3d
{
public:
	static D3DXFRAME* Load(IDirect3DDevice9* device, const char* fname, int path_type, ID3DXAnimationController** anim);
	static void Free(D3DXFRAME* frame);

	//static ID3DXAnimationController* LoadAnimation();

private:
	static MeshHierarchy memAllocator;

	// X
	static D3DXFRAME* Load_X(FileManager& fm, IDirect3DDevice9* device, ID3DXAnimationController** anim);

	// 3ds
	static D3DXFRAME* Load_3ds(FileManager& fm, IDirect3DDevice9* device);
};

#endif // __3D_LOADER__

