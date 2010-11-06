#include "StdAfx.h"
#include "MeshHierarchy.h"
#include <FileManager.h>
#include "Loader.h"

HRESULT MeshHierarchy::CreateFrame(LPCSTR Name, LPD3DXFRAME *retNewFrame)
{
	// Always a good idea to initialise a return pointer before proceeding
	*retNewFrame=0;

	// Create a new frame using the derived version of the structure
    D3DXFRAME_EXTENDED *newFrame=new D3DXFRAME_EXTENDED;
	ZeroMemory(newFrame,sizeof(D3DXFRAME_EXTENDED));

	// Now fill in the data members in the frame structure
	
    // Now initialize other data members of the frame to defaults
    D3DXMatrixIdentity(&newFrame->TransformationMatrix);
    D3DXMatrixIdentity(&newFrame->exCombinedTransformationMatrix);

	newFrame->pMeshContainer=0;
    newFrame->pFrameSibling=0;
    newFrame->pFrameFirstChild=0;

	// Assign the return pointer to our newly created frame
    *retNewFrame=newFrame;
	
	// The frame name (note: may be 0 or zero length)
	if(Name && strlen(Name))
	{
		newFrame->Name=StringDuplicate(Name);	
//		Utility3d::DebugString("Added frame: "+ToString(Name));
	}
	else
	{
//		Utility3d::DebugString("Added frame: no name given");
	}
    return S_OK;
}

HRESULT MeshHierarchy::CreateMeshContainer(
    LPCSTR Name,
    CONST D3DXMESHDATA* meshData,
    CONST D3DXMATERIAL* materials,
    CONST D3DXEFFECTINSTANCE* effectInstances,
    DWORD numMaterials,
    CONST DWORD* adjacency,
    LPD3DXSKININFO pSkinInfo,
    LPD3DXMESHCONTAINER* retNewMeshContainer)
{
	// Create a mesh container structure to fill and initilaise to zero values
	// Note: I use my extended version of the structure (D3DXMESHCONTAINER_EXTENDED) defined in MeshStructures.h
	D3DXMESHCONTAINER_EXTENDED *newMeshContainer=new D3DXMESHCONTAINER_EXTENDED;
	ZeroMemory(newMeshContainer, sizeof(D3DXMESHCONTAINER_EXTENDED));

	// Always a good idea to initialise return pointer before proceeding
	*retNewMeshContainer=0;

	// The mesh name (may be 0) needs copying over
	if(Name && strlen(Name)) newMeshContainer->Name=StringDuplicate(Name);

	// The mesh type (D3DXMESHTYPE_MESH, D3DXMESHTYPE_PMESH or D3DXMESHTYPE_PATCHMESH)
	if(meshData->Type!=D3DXMESHTYPE_MESH)
	{
		// This demo does not handle mesh types other than the standard
		// Other types are D3DXMESHTYPE_PMESH (progressive mesh) and D3DXMESHTYPE_PATCHMESH (patch mesh)
		DestroyMeshContainer(newMeshContainer);
		return E_FAIL;
	}

	newMeshContainer->MeshData.Type=D3DXMESHTYPE_MESH;
	
	// Adjacency data - holds information about triangle adjacency, required by the ID3DMESH object
	DWORD dwFaces=meshData->pMesh->GetNumFaces();
	newMeshContainer->pAdjacency=new DWORD[dwFaces*3];
	if(adjacency) memcpy(newMeshContainer->pAdjacency, adjacency, sizeof(DWORD)*dwFaces*3);
	else meshData->pMesh->GenerateAdjacency(0.0000125f,newMeshContainer->pAdjacency);

	// Get the Direct3D device, luckily this is held in the mesh itself (Note: must release it when done with it)
	LPDIRECT3DDEVICE9 pd3dDevice=0;
	meshData->pMesh->GetDevice(&pd3dDevice);

	// Changed 24/09/07 - can just assign pointer and add a ref rather than need to clone
	newMeshContainer->MeshData.pMesh=meshData->pMesh;
	newMeshContainer->MeshData.pMesh->AddRef();

	// Create material and texture arrays. Note that I always want to have at least one
	newMeshContainer->NumMaterials=max(numMaterials,1);
	newMeshContainer->exMaterials=new D3DMATERIAL9[newMeshContainer->NumMaterials];
	newMeshContainer->exTexturesNames=new char*[newMeshContainer->NumMaterials];
	newMeshContainer->exEffects=new D3DXEFFECTINSTANCE[newMeshContainer->NumMaterials];

	if(numMaterials>0)
	{
		// Load all the textures and copy the materials over		
		for(DWORD i=0;i<numMaterials;i++)
		{
			newMeshContainer->exTexturesNames[i]=StringDuplicate(materials[i].pTextureFilename);
			newMeshContainer->exMaterials[i]=materials[i].MatD3D;

			// The mesh may contain a reference to an effect file
			ZeroMemory(&newMeshContainer->exEffects[i],sizeof(D3DXEFFECTINSTANCE));
			if(effectInstances && effectInstances[i].pEffectFilename && effectInstances[i].pEffectFilename[0])
			{
				newMeshContainer->exEffects[i].pEffectFilename=StringDuplicate(effectInstances[i].pEffectFilename);
				newMeshContainer->exEffects[i].NumDefaults=effectInstances[i].NumDefaults;
				newMeshContainer->exEffects[i].pDefaults=NULL;

				DWORD defaults=newMeshContainer->exEffects[i].NumDefaults;
				if(defaults)
				{
					newMeshContainer->exEffects[i].pDefaults=new D3DXEFFECTDEFAULT[defaults];
					for(DWORD j=0;j<defaults;j++)
					{
						newMeshContainer->exEffects[i].pDefaults[j].pParamName=StringDuplicate(effectInstances[i].pDefaults[j].pParamName);
						newMeshContainer->exEffects[i].pDefaults[j].Type=effectInstances[i].pDefaults[j].Type;
						newMeshContainer->exEffects[i].pDefaults[j].NumBytes=effectInstances[i].pDefaults[j].NumBytes;
						newMeshContainer->exEffects[i].pDefaults[j].pValue=new char[newMeshContainer->exEffects[i].pDefaults[j].NumBytes];
						memcpy(newMeshContainer->exEffects[i].pDefaults[j].pValue,effectInstances[i].pDefaults[j].pValue,newMeshContainer->exEffects[i].pDefaults[j].NumBytes);
					}
				}
			}
		}
	}
	else
    {
		// Make a default material in the case where the mesh did not provide one
		ZeroMemory(&newMeshContainer->exMaterials[0],sizeof(D3DMATERIAL9));
		newMeshContainer->exMaterials[0].Diffuse.a=1.0f;
		newMeshContainer->exMaterials[0].Diffuse.r=0.5f;
		newMeshContainer->exMaterials[0].Diffuse.g=0.5f;
		newMeshContainer->exMaterials[0].Diffuse.b=0.5f;
		newMeshContainer->exMaterials[0].Specular=newMeshContainer->exMaterials[0].Diffuse;
		newMeshContainer->exTexturesNames[0]=NULL;
		ZeroMemory(&newMeshContainer->exEffects[0],sizeof(D3DXEFFECTINSTANCE));
    }

	// If there is skin data associated with the mesh copy it over
	if(pSkinInfo)
	{
		// save off the SkinInfo
	    newMeshContainer->pSkinInfo=pSkinInfo;
	    pSkinInfo->AddRef();

	    // Need an array of offset matrices to move the vertices from the figure space to the bone's space
	    UINT numBones=pSkinInfo->GetNumBones();
	    newMeshContainer->exBoneOffsets=new D3DXMATRIX[numBones];

		// Create the arrays for the bones and the frame matrices
		newMeshContainer->exFrameCombinedMatrixPointer=new D3DXMATRIX*[numBones];
		ZeroMemory(newMeshContainer->exFrameCombinedMatrixPointer,sizeof(D3DXMATRIX*)*numBones);

	    // get each of the bone offset matrices so that we don't need to get them later
	    for (UINT i=0; i < numBones; i++)
	        newMeshContainer->exBoneOffsets[i]=*(newMeshContainer->pSkinInfo->GetBoneOffsetMatrix(i));
	}
	else
	{
		// No skin info so 0 all the pointers
		newMeshContainer->pSkinInfo=0;
		newMeshContainer->exBoneOffsets=0;
		newMeshContainer->exSkinMesh=0;
		newMeshContainer->exFrameCombinedMatrixPointer=0;
	}

	// When we got the device we caused an internal reference count to be incremented
	// So we now need to release it
	pd3dDevice->Release();

	// Set the output mesh container pointer to our newly created one
	*retNewMeshContainer=newMeshContainer;    

	return S_OK;
}

HRESULT MeshHierarchy::DestroyFrame(LPD3DXFRAME frameToFree) 
{
	// Convert to our extended type. OK to do this as we know for sure it is:
	D3DXFRAME_EXTENDED* frame=(D3DXFRAME_EXTENDED*)frameToFree;

	SAFEDELA(frame->Name);
	delete frame;

    return S_OK; 
}

HRESULT MeshHierarchy::DestroyMeshContainer(LPD3DXMESHCONTAINER meshContainerBase)
{
	// Convert to our extended type. OK as we know for sure it is:
    D3DXMESHCONTAINER_EXTENDED* mesh_container=(D3DXMESHCONTAINER_EXTENDED*)meshContainerBase;
	if(!mesh_container) return S_OK;

	// Name
	SAFEDELA(mesh_container->Name);
	// Materials array
	SAFEDELA(mesh_container->exMaterials);
	// Release the textures before deleting the array
	if(mesh_container->exTexturesNames)
	{
		for(DWORD i=0;i<mesh_container->NumMaterials;i++)
			SAFEDELA(mesh_container->exTexturesNames[i]);
	}
	SAFEDELA(mesh_container->exTexturesNames);
	// Delete effect
	if(mesh_container->exEffects)
	{
		for(DWORD i=0;i<mesh_container->NumMaterials;i++)
		{
			for(DWORD j=0;j<mesh_container->exEffects[i].NumDefaults;j++)
				SAFEDELA(mesh_container->exEffects[i].pDefaults[j].pValue);
			SAFEDELA(mesh_container->exEffects[i].pDefaults);
		}
	}
	SAFEDEL(mesh_container->exEffects);
	// Adjacency data
	SAFEDELA(mesh_container->pAdjacency);
	// Bone parts
	SAFEDELA(mesh_container->exBoneOffsets);
	// Frame matrices
	SAFEDELA(mesh_container->exFrameCombinedMatrixPointer);
	// Release skin mesh
	SAFEREL(mesh_container->exSkinMesh);
	// Release the main mesh
	SAFEREL(mesh_container->MeshData.pMesh);
	// Release skin information
	SAFEREL(mesh_container->pSkinInfo);
	// Release blend mesh
	SAFEREL(mesh_container->exSkinMeshBlended);
	// Finally delete the mesh container itself
	SAFEDEL(mesh_container);
    return S_OK;
}
