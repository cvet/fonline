#ifndef __MESH_STRUCTURES__
#define __MESH_STRUCTURES__

struct D3DXMESHCONTAINER_EXTENDED: public D3DXMESHCONTAINER
{
	// The base D3DXMESHCONTAINER has a pMaterials member which is a D3DXMATERIAL structure 
	// that contains a texture filename and material data. It is easier to ignore this and 
	// instead store the data in arrays of textures and materials in this extended structure:
	char**               exTexturesNames;
	D3DMATERIAL9*		 exMaterials;		    // Array of materials
	// Skinned mesh variables
	ID3DXMesh*           exSkinMesh;			// The skin mesh
	D3DXMATRIX*			 exBoneOffsets;			// The bone matrix Offsets, one per bone
	D3DXMATRIX**		 exFrameCombinedMatrixPointer;	// Array of frame matrix pointers
	// Used for indexed shader skinning
	ID3DXMesh*           exSkinMeshBlended;     // The blended skin mesh
	ID3DXBuffer*         exBoneCombinationBuf;
	DWORD                exNumAttributeGroups;
	DWORD                exNumPaletteEntries;
	DWORD                exNumInfl;
	// Effect
	ID3DXEffect*         exEffect;
	D3DXHANDLE           exEffectParams;
};

struct D3DXFRAME_EXTENDED: public D3DXFRAME
{
    D3DXMATRIX exCombinedTransformationMatrix;
};

#endif // __MESH_STRUCTURES__