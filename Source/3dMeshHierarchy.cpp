#include "StdAfx.h"
#include "3dMeshHierarchy.h"
#include "3dLoader.h"
#include "FileManager.h"

#ifdef FO_D3D
HRESULT MeshHierarchy::CreateFrame( LPCSTR Name, LPD3DXFRAME* retNewFrame )
{
    // Always a good idea to initialise a return pointer before proceeding
    *retNewFrame = 0;

    // Create a new frame using the derived version of the structure
    Frame* newFrame = new Frame;
    memzero( newFrame, sizeof( Frame ) );

    // Now initialize other data members of the frame to defaults
    D3DXMatrixIdentity( &newFrame->TransformationMatrix );
    D3DXMatrixIdentity( &newFrame->CombinedTransformationMatrix );

    newFrame->Meshes = 0;
    newFrame->Sibling = 0;
    newFrame->FirstChild = 0;

    // Assign the return pointer to our newly created frame
    *retNewFrame = (LPD3DXFRAME) newFrame;

    // The frame name (note: may be 0 or zero length)
    if( Name && Str::Length( Name ) )
        newFrame->Name = Str::Duplicate( Name );

    return S_OK;
}

HRESULT MeshHierarchy::CreateMeshContainer(
    LPCSTR Name,
    CONST D3DXMESHDATA* meshData,
    CONST D3DXMATERIAL* materials,
    CONST D3DXEFFECTINSTANCE* effectInstances,
    DWORD numMaterials,
    CONST DWORD* adjacency,
    SkinInfo_ SkinInfo,
    LPD3DXMESHCONTAINER* retNewMeshContainer )
{
    // Create a mesh container structure to fill and initilaise to zero values
    // Note: I use my extended version of the structure (MeshContainer) defined in MeshStructures.h
    MeshContainer* newMeshContainer = new MeshContainer;
    memzero( newMeshContainer, sizeof( MeshContainer ) );

    // Always a good idea to initialise return pointer before proceeding
    *retNewMeshContainer = 0;

    // The mesh name (may be 0) needs copying over
    if( Name && Str::Length( Name ) )
        newMeshContainer->Name = Str::Duplicate( Name );

    // The mesh type (D3DXMESHTYPE_MESH, D3DXMESHTYPE_PMESH or D3DXMESHTYPE_PATCHMESH)
    if( meshData->Type != D3DXMESHTYPE_MESH )
    {
        // This demo does not handle mesh types other than the standard
        // Other types are D3DXMESHTYPE_PMESH (progressive mesh) and D3DXMESHTYPE_PATCHMESH (patch mesh)
        DestroyMeshContainer( (LPD3DXMESHCONTAINER) newMeshContainer );
        return E_FAIL;
    }

    newMeshContainer->MeshData.Type = D3DXMESHTYPE_MESH;

    // Adjacency data - holds information about triangle adjacency, required by the ID3DMESH object
    uint dwFaces = meshData->pMesh->GetNumFaces();
    newMeshContainer->Adjacency = new uint[ dwFaces * 3 ];
    if( adjacency )
        memcpy( newMeshContainer->Adjacency, adjacency, sizeof( uint ) * dwFaces * 3 );
    else
        meshData->pMesh->GenerateAdjacency( 0.0000125f, (DWORD*) newMeshContainer->Adjacency );

    // Get the Direct3D device, luckily this is held in the mesh itself (Note: must release it when done with it)
    Device_ pd3dDevice = NULL;
    meshData->pMesh->GetDevice( &pd3dDevice );

    // Changed 24/09/07 - can just assign pointer and add a ref rather than need to clone
    newMeshContainer->MeshData.Mesh = meshData->pMesh;
    newMeshContainer->MeshData.Mesh->AddRef();

    // Create material and texture arrays. Note that I always want to have at least one
    newMeshContainer->NumMaterials = max( numMaterials, 1 );
    newMeshContainer->Materials = new Material_[ newMeshContainer->NumMaterials ];
    newMeshContainer->TextureNames = new char*[ newMeshContainer->NumMaterials ];
    newMeshContainer->Effects = new EffectInstance[ newMeshContainer->NumMaterials ];

    if( numMaterials > 0 )
    {
        // Load all the textures and copy the materials over
        for( uint i = 0; i < numMaterials; i++ )
        {
            if( materials[ i ].pTextureFilename )
                newMeshContainer->TextureNames[ i ] = Str::Duplicate( materials[ i ].pTextureFilename );
            else
                newMeshContainer->TextureNames[ i ] = NULL;
            newMeshContainer->Materials[ i ] = materials[ i ].MatD3D;

            // The mesh may contain a reference to an effect file
            memzero( &newMeshContainer->Effects[ i ], sizeof( EffectInstance ) );
            if( effectInstances && effectInstances[ i ].pEffectFilename && effectInstances[ i ].pEffectFilename[ 0 ] )
            {
                newMeshContainer->Effects[ i ].EffectFilename = Str::Duplicate( effectInstances[ i ].pEffectFilename );
                newMeshContainer->Effects[ i ].DefaultsCount = effectInstances[ i ].NumDefaults;
                newMeshContainer->Effects[ i ].Defaults = NULL;

                uint defaults = newMeshContainer->Effects[ i ].DefaultsCount;
                if( defaults )
                {
                    newMeshContainer->Effects[ i ].Defaults = new EffectDefault[ defaults ];
                    for( uint j = 0; j < defaults; j++ )
                    {
                        newMeshContainer->Effects[ i ].Defaults[ j ].Name = Str::Duplicate( effectInstances[ i ].pDefaults[ j ].pParamName );
                        if( effectInstances[ i ].pDefaults[ j ].Type == D3DXEDT_STRING )
                            newMeshContainer->Effects[ i ].Defaults[ j ].Type = EffectDefault::String;
                        else if( effectInstances[ i ].pDefaults[ j ].Type == D3DXEDT_FLOATS )
                            newMeshContainer->Effects[ i ].Defaults[ j ].Type = EffectDefault::Floats;
                        else if( effectInstances[ i ].pDefaults[ j ].Type == D3DXEDT_DWORD )
                            newMeshContainer->Effects[ i ].Defaults[ j ].Type = EffectDefault::Dword;
                        newMeshContainer->Effects[ i ].Defaults[ j ].Size = effectInstances[ i ].pDefaults[ j ].NumBytes;
                        newMeshContainer->Effects[ i ].Defaults[ j ].Data = new char[ newMeshContainer->Effects[ i ].Defaults[ j ].Size ];
                        memcpy( newMeshContainer->Effects[ i ].Defaults[ j ].Data, effectInstances[ i ].pDefaults[ j ].pValue, newMeshContainer->Effects[ i ].Defaults[ j ].Size );
                    }
                }
            }
        }
    }
    else
    {
        // Make a default material in the case where the mesh did not provide one
        memzero( &newMeshContainer->Materials[ 0 ], sizeof( Material_ ) );
        newMeshContainer->Materials[ 0 ].Diffuse.a = 1.0f;
        newMeshContainer->Materials[ 0 ].Diffuse.r = 0.5f;
        newMeshContainer->Materials[ 0 ].Diffuse.g = 0.5f;
        newMeshContainer->Materials[ 0 ].Diffuse.b = 0.5f;
        newMeshContainer->Materials[ 0 ].Specular = newMeshContainer->Materials[ 0 ].Diffuse;
        newMeshContainer->TextureNames[ 0 ] = NULL;
        memzero( &newMeshContainer->Effects[ 0 ], sizeof( EffectInstance ) );
    }

    // If there is skin data associated with the mesh copy it over
    if( SkinInfo )
    {
        // Save off the SkinInfo
        newMeshContainer->SkinInfo = SkinInfo;
        SkinInfo->AddRef();

        // Need an array of offset matrices to move the vertices from the figure space to the bone's space
        UINT numBones = SkinInfo->GetNumBones();
        newMeshContainer->BoneOffsets = new Matrix_[ numBones ];

        // Create the arrays for the bones and the frame matrices
        newMeshContainer->FrameCombinedMatrixPointer = new Matrix_*[ numBones ];
        memzero( newMeshContainer->FrameCombinedMatrixPointer, sizeof( Matrix_* ) * numBones );

        // get each of the bone offset matrices so that we don't need to get them later
        for( UINT i = 0; i < numBones; i++ )
            newMeshContainer->BoneOffsets[ i ] = *( newMeshContainer->SkinInfo->GetBoneOffsetMatrix( i ) );
    }
    else
    {
        // No skin info so NULL all the pointers
        newMeshContainer->SkinInfo = NULL;
        newMeshContainer->BoneOffsets = NULL;
        newMeshContainer->SkinMesh = NULL;
        newMeshContainer->FrameCombinedMatrixPointer = NULL;
    }

    // When we got the device we caused an internal reference count to be incremented
    // So we now need to release it
    pd3dDevice->Release();

    // Set the output mesh container pointer to our newly created one
    *retNewMeshContainer = (LPD3DXMESHCONTAINER) newMeshContainer;

    return S_OK;
}

HRESULT MeshHierarchy::DestroyFrame( LPD3DXFRAME frameToFree )
{
    // Convert to our extended type. OK to do this as we know for sure it is:
    Frame* frame = (Frame*) frameToFree;

    SAFEDELA( frame->Name );
    delete frame;

    return S_OK;
}

HRESULT MeshHierarchy::DestroyMeshContainer( LPD3DXMESHCONTAINER meshContainerBase )
{
    // Convert to our extended type. OK as we know for sure it is:
    MeshContainer* mesh_container = (MeshContainer*) meshContainerBase;
    if( !mesh_container )
        return S_OK;

    // Name
    SAFEDELA( mesh_container->Name );
    // Materials array
    SAFEDELA( mesh_container->Materials );
    // Release the textures before deleting the array
    if( mesh_container->TextureNames )
    {
        for( uint i = 0; i < mesh_container->NumMaterials; i++ )
            SAFEDELA( mesh_container->TextureNames[ i ] );
    }
    SAFEDELA( mesh_container->TextureNames );
    // Delete effect
    if( mesh_container->Effects )
    {
        for( uint i = 0; i < mesh_container->NumMaterials; i++ )
        {
            for( uint j = 0; j < mesh_container->Effects[ i ].DefaultsCount; j++ )
                SAFEDELA( mesh_container->Effects[ i ].Defaults[ j ].Data );
            SAFEDELA( mesh_container->Effects[ i ].Defaults );
        }
    }
    SAFEDEL( mesh_container->Effects );
    // Adjacency data
    SAFEDELA( mesh_container->Adjacency );
    // Bone parts
    SAFEDELA( mesh_container->BoneOffsets );
    // Frame matrices
    SAFEDELA( mesh_container->FrameCombinedMatrixPointer );
    // Release skin mesh
    SAFEREL( mesh_container->SkinMesh );
    // Release the main mesh
    SAFEREL( mesh_container->MeshData.Mesh );
    // Release skin information
    SAFEREL( mesh_container->SkinInfo );
    // Release blend mesh
    SAFEREL( mesh_container->SkinMeshBlended );
    // Finally delete the mesh container itself
    SAFEDEL( mesh_container );
    return S_OK;
}
#endif
