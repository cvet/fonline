#ifndef __MESH_STRUCTURES__
#define __MESH_STRUCTURES__

struct EffectEx
{
	ID3DXEffect* Effect;
	D3DXHANDLE EffectParams;
	D3DXHANDLE TechniqueSkin;
	D3DXHANDLE TechniqueSkinWithShadow;
	D3DXHANDLE TechniqueSimple;
	D3DXHANDLE TechniqueSimpleWithShadow;
	D3DXHANDLE NumBones;
	D3DXHANDLE GroundPos;
	D3DXHANDLE LightDir;
	D3DXHANDLE LightDiffuse;
	D3DXHANDLE MaterialAmbient;
	D3DXHANDLE MaterialDiffuse;
	D3DXHANDLE WorldMatrices;
	D3DXHANDLE ProjMatrix;

	EffectEx(ID3DXEffect* effect)
	{
		Effect=effect;
		EffectParams=NULL;
		TechniqueSkin=Effect->GetTechniqueByName("Skin");
		TechniqueSkinWithShadow=Effect->GetTechniqueByName("SkinWithShadow");
		TechniqueSimple=Effect->GetTechniqueByName("Simple");
		TechniqueSimpleWithShadow=Effect->GetTechniqueByName("SimpleWithShadow");
		NumBones=Effect->GetParameterByName(NULL,"NumBones");
		GroundPos=Effect->GetParameterByName(NULL,"GroundPos");
		LightDir=Effect->GetParameterByName(NULL,"LightDir");
		LightDiffuse=Effect->GetParameterByName(NULL,"LightDiffuse");
		MaterialAmbient=Effect->GetParameterByName(NULL,"MaterialAmbient");
		MaterialDiffuse=Effect->GetParameterByName(NULL,"MaterialDiffuse");
		WorldMatrices=Effect->GetParameterByName(NULL,"WorldMatrices");
		ProjMatrix=Effect->GetParameterByName(NULL,"ProjMatrix");
	}

	~EffectEx()
	{
		if(Effect)
		{
			if(EffectParams) Effect->DeleteParameterBlock(EffectParams);
			Effect->Release();
		}
	}
};

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
	EffectEx*            exEffect;
};

struct D3DXFRAME_EXTENDED: public D3DXFRAME
{
    D3DXMATRIX exCombinedTransformationMatrix;
};

#endif // __MESH_STRUCTURES__