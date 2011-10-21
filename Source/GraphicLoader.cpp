#include "StdAfx.h"
#include "GraphicLoader.h"
#include "Text.h"
#include "Timer.h"

#include "Assimp/aiScene.h"
#include "Assimp/aiAnim.h"
#include "Assimp/aiPostProcess.h"
#include "Assimp/assimp.hpp"
#include "Assimp/Logger.h"
#include "Assimp/LogStream.h"
#include "Assimp/DefaultLogger.h"
#pragma comment (lib, "assimp.lib")

/************************************************************************/
/* Models                                                               */
/************************************************************************/

PCharVec   GraphicLoader::processedFiles;
FrameVec   GraphicLoader::loadedModels;
PCharVec   GraphicLoader::loadedAnimationsFNames;
AnimSetVec GraphicLoader::loadedAnimations;

Frame* GraphicLoader::LoadModel( Device_ device, const char* fname, bool calc_tangent )
{
    for( auto it = loadedModels.begin(), end = loadedModels.end(); it != end; ++it )
    {
        Frame* frame = *it;
        if( Str::CompareCase( frame->Name, fname ) )
            return frame;
    }

    for( uint i = 0, j = (uint) processedFiles.size(); i < j; i++ )
        if( Str::CompareCase( processedFiles[ i ], fname ) )
            return NULL;
    processedFiles.push_back( Str::Duplicate( fname ) );

    FileManager fm;
    if( !fm.LoadFile( fname, PT_DATA ) )
    {
        WriteLogF( _FUNC_, " - 3d file not found, name<%s>.\n", fname );
        return NULL;
    }

    // Assimp loader
    static Assimp::Importer* importer = NULL;
    if( !importer )
    {
        // Check dll availability
        HMODULE dll = LoadLibrary( "Assimp32.dll" );
        if( !dll )
        {
            if( GameOpt.ClientPath != "" )
                dll = LoadLibrary( ( GameOpt.ClientPath + "Assimp32.dll" ).c_str() );

            if( !dll )
            {
                WriteLogF( _FUNC_, " - Assimp32.dll not found.\n" );
                return NULL;
            }
        }

        // Create importer instance
        importer = new ( nothrow ) Assimp::Importer();
        if( !importer )
            return NULL;

        // Logging
        if( false )
            Assimp::DefaultLogger::create( ASSIMP_DEFAULT_LOG_NAME, Assimp::Logger::VERBOSE );
    }

    const aiScene* scene = importer->ReadFileFromMemory( fm.GetBuf(), fm.GetFsize(),
                                                         ( calc_tangent ? aiProcess_CalcTangentSpace : 0 ) | aiProcess_GenNormals | aiProcess_GenUVCoords |
                                                         aiProcess_ConvertToLeftHanded | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                                                         aiProcess_SortByPType | aiProcess_SplitLargeMeshes | aiProcess_LimitBoneWeights |
                                                         aiProcess_ImproveCacheLocality );
    // Todo: optional aiProcess_ValidateDataStructure|aiProcess_FindInvalidData

    if( !scene )
    {
        WriteLogF( _FUNC_, " - Can't load 3d file, name<%s>, error<%s>.\n", fname, importer->GetErrorString() );
        return NULL;
    }

    // Extract frames
    Frame* frame_root = FillNode( device, scene->mRootNode, scene, calc_tangent );
    if( !frame_root )
    {
        WriteLogF( _FUNC_, " - Conversion fail, name<%s>.\n", fname );
        importer->FreeScene();
        return NULL;
    }
    frame_root->Name = Str::Duplicate( fname );
    loadedModels.push_back( frame_root );

    // Extract animations
    #ifdef FO_D3D
    vector< D3DXKEY_VECTOR3 >    scale;
    vector< D3DXKEY_QUATERNION > rot;
    vector< D3DXKEY_VECTOR3 >    pos;
    for( unsigned int i = 0; i < scene->mNumAnimations; i++ )
    {
        aiAnimation*                anim = scene->mAnimations[ i ];

        ID3DXKeyframedAnimationSet* set;
        if( FAILED( D3DXCreateKeyframedAnimationSet( anim->mName.data, anim->mTicksPerSecond, D3DXPLAY_LOOP, anim->mNumChannels, 0, NULL, &set ) ) )
        {
            WriteLogF( _FUNC_, " - Can't extract animation<%s> from file<%s>.\n", anim->mName.data, fname );
            continue;
        }

        for( unsigned int j = 0; j < anim->mNumChannels; j++ )
        {
            aiNodeAnim* na = anim->mChannels[ j ];

            if( scale.size() < na->mNumScalingKeys )
                scale.resize( na->mNumScalingKeys );
            for( unsigned int k = 0; k < na->mNumScalingKeys; k++ )
            {
                scale[ k ].Time = (float) na->mScalingKeys[ k ].mTime;
                scale[ k ].Value.x = (float) na->mScalingKeys[ k ].mValue.x;
                scale[ k ].Value.y = (float) na->mScalingKeys[ k ].mValue.y;
                scale[ k ].Value.z = (float) na->mScalingKeys[ k ].mValue.z;
            }
            if( rot.size() < na->mNumRotationKeys )
                rot.resize( na->mNumRotationKeys );
            for( unsigned int k = 0; k < na->mNumRotationKeys; k++ )
            {
                rot[ k ].Time = (float) na->mRotationKeys[ k ].mTime;
                rot[ k ].Value.x = (float) na->mRotationKeys[ k ].mValue.x;
                rot[ k ].Value.y = (float) na->mRotationKeys[ k ].mValue.y;
                rot[ k ].Value.z = (float) na->mRotationKeys[ k ].mValue.z;
                rot[ k ].Value.w = -(float) na->mRotationKeys[ k ].mValue.w;
            }
            if( pos.size() < na->mNumPositionKeys )
                pos.resize( na->mNumPositionKeys );
            for( unsigned int k = 0; k < na->mNumPositionKeys; k++ )
            {
                pos[ k ].Time = (float) na->mPositionKeys[ k ].mTime;
                pos[ k ].Value.x = (float) na->mPositionKeys[ k ].mValue.x;
                pos[ k ].Value.y = (float) na->mPositionKeys[ k ].mValue.y;
                pos[ k ].Value.z = (float) na->mPositionKeys[ k ].mValue.z;
            }

            DWORD index;
            set->RegisterAnimationSRTKeys( na->mNodeName.data, na->mNumScalingKeys, na->mNumRotationKeys, na->mNumPositionKeys,
                                           na->mNumScalingKeys ? &scale[ 0 ] : NULL, na->mNumRotationKeys ? &rot[ 0 ] : NULL, na->mNumPositionKeys ? &pos[ 0 ] : NULL, &index );
        }

        Buffer_*                     buf;
        ID3DXCompressedAnimationSet* cset;
        if( FAILED( set->Compress( D3DXCOMPRESS_DEFAULT, 0.0f, NULL, &buf ) ) )
            continue;
        if( FAILED( D3DXCreateCompressedAnimationSet( anim->mName.data, anim->mTicksPerSecond, D3DXPLAY_LOOP, buf, 0, NULL, &cset ) ) )
            continue;
        buf->Release();
        set->Release();

        loadedAnimationsFNames.push_back( Str::Duplicate( fname ) );
        loadedAnimations.push_back( cset );
    }
    #endif

    importer->FreeScene();
    return frame_root;
}

Matrix_ ConvertMatrix( const aiMatrix4x4& mat )
{
    Matrix_ m;
    #ifdef FO_D3D
    m._11 = mat.a1;
    m._21 = mat.a2;
    m._31 = mat.a3;
    m._41 = mat.a4;
    m._12 = mat.b1;
    m._22 = mat.b2;
    m._32 = mat.b3;
    m._42 = mat.b4;
    m._13 = mat.c1;
    m._23 = mat.c2;
    m._33 = mat.c3;
    m._43 = mat.c4;
    m._14 = mat.d1;
    m._24 = mat.d2;
    m._34 = mat.d3;
    m._44 = mat.d4;
    #endif
    return m;
}

Frame* GraphicLoader::FillNode( Device_ device, const aiNode* node, const aiScene* scene, bool with_tangent )
{
    #ifdef FO_D3D
    // Create frame
    Frame* frame = new Frame();
    memzero( frame, sizeof( Frame ) );
    frame->Name = Str::Duplicate( node->mName.data );
    frame->Meshes = NULL;
    frame->Sibling = NULL;
    frame->FirstChild = NULL;
    D3DXMatrixIdentity( &frame->TransformationMatrix );
    D3DXMatrixIdentity( &frame->CombinedTransformationMatrix );

    frame->TransformationMatrix = ConvertMatrix( node->mTransformation );

    // Merge meshes, because Assimp split subsets
    if( node->mNumMeshes )
    {
        // Calculate whole data
        uint                   faces_count = 0;
        uint                   vertices_count = 0;
        vector< aiBone* >      all_bones;
        vector< D3DXMATERIAL > materials;
        for( unsigned int m = 0; m < node->mNumMeshes; m++ )
        {
            aiMesh* mesh = scene->mMeshes[ node->mMeshes[ m ] ];

            // Faces and vertices
            faces_count += mesh->mNumFaces;
            vertices_count += mesh->mNumVertices;

            // Shared bones
            for( unsigned int i = 0; i < mesh->mNumBones; i++ )
            {
                aiBone* bone = mesh->mBones[ i ];
                bool    bone_aviable = false;
                for( uint b = 0, bb = (uint) all_bones.size(); b < bb; b++ )
                {
                    if( !strcmp( all_bones[ b ]->mName.data, bone->mName.data ) )
                    {
                        bone_aviable = true;
                        break;
                    }
                }
                if( !bone_aviable )
                    all_bones.push_back( bone );
            }

            // Material
            D3DXMATERIAL material;
            memzero( &material, sizeof( material ) );
            aiMaterial*  mtrl = scene->mMaterials[ mesh->mMaterialIndex ];

            aiString     path;
            if( mtrl->GetTextureCount( aiTextureType_DIFFUSE ) )
            {
                mtrl->GetTexture( aiTextureType_DIFFUSE, 0, &path );
                material.pTextureFilename = Str::Duplicate( path.data );
            }

            mtrl->Get< float >( AI_MATKEY_COLOR_DIFFUSE, (float*) &material.MatD3D.Diffuse, NULL );
            mtrl->Get< float >( AI_MATKEY_COLOR_AMBIENT, (float*) &material.MatD3D.Ambient, NULL );
            mtrl->Get< float >( AI_MATKEY_COLOR_SPECULAR, (float*) &material.MatD3D.Specular, NULL );
            mtrl->Get< float >( AI_MATKEY_COLOR_EMISSIVE, (float*) &material.MatD3D.Emissive, NULL );
            material.MatD3D.Diffuse.a = 1.0f;
            material.MatD3D.Ambient.a = 1.0f;
            material.MatD3D.Specular.a = 1.0f;
            material.MatD3D.Emissive.a = 1.0f;

            materials.push_back( material );
        }

        // Mesh declarations
        D3DVERTEXELEMENT9  delc_simple[] =
        {
            { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
            { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };
        D3DVERTEXELEMENT9  delc_tangent[] =
        {
            { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
            { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            { 0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
            { 0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },
            D3DDECL_END()
        };
        D3DVERTEXELEMENT9* declaration = ( with_tangent ? delc_tangent : delc_simple );

        // Create mesh
        D3DXMESHDATA dxmesh;
        dxmesh.Type = D3DXMESHTYPE_MESH;
        D3D_HR( D3DXCreateMesh( faces_count, vertices_count, D3DXMESH_MANAGED, declaration, device, &dxmesh.pMesh ) );

        // Skinning
        ID3DXSkinInfo* skin_info = NULL;

        // Fill data
        struct Vertex
        {
            float x, y, z;
            float nx, ny, nz;
            float u, v;
            float tx, ty, tz;
            float bx, by, bz;
        }* vb;
        int    vsize = ( with_tangent ? 56 : 32 );
        WORD*  ib;
        DWORD* att;
        D3D_HR( dxmesh.pMesh->LockVertexBuffer( 0, (void**) &vb ) );
        D3D_HR( dxmesh.pMesh->LockIndexBuffer( 0, (void**) &ib ) );
        D3D_HR( dxmesh.pMesh->LockAttributeBuffer( 0, &att ) );

        uint cur_vertices = 0;
        uint cur_faces = 0;
        for( unsigned int m = 0; m < node->mNumMeshes; m++ )
        {
            aiMesh* mesh = scene->mMeshes[ node->mMeshes[ m ] ];

            for( unsigned int i = 0; i < mesh->mNumVertices; i++ )
            {
                Vertex& v = *vb;
                vb = (Vertex*) ( ( (char*) vb ) + vsize );
                memzero( &v, vsize );

                // Vertices data
                v.x = mesh->mVertices[ i ].x;
                v.y = mesh->mVertices[ i ].y;
                v.z = mesh->mVertices[ i ].z;

                if( mesh->mTextureCoords && mesh->mTextureCoords[ 0 ] )
                {
                    v.u = mesh->mTextureCoords[ 0 ][ i ].x;
                    v.v = mesh->mTextureCoords[ 0 ][ i ].y;
                }
                if( mesh->mNormals )
                {
                    v.nx = mesh->mNormals[ i ].x;
                    v.ny = mesh->mNormals[ i ].y;
                    v.nz = mesh->mNormals[ i ].z;
                }
                if( with_tangent && mesh->mTangents )
                {
                    v.tx = mesh->mTangents[ i ].x;
                    v.ty = mesh->mTangents[ i ].y;
                    v.tz = mesh->mTangents[ i ].z;
                    v.bx = mesh->mBitangents[ i ].x;
                    v.by = mesh->mBitangents[ i ].y;
                    v.bz = mesh->mBitangents[ i ].z;
                }
            }

            // Indices
            for( unsigned int i = 0; i < mesh->mNumFaces; i++ )
            {
                aiFace& face = mesh->mFaces[ i ];
                for( unsigned int j = 0; j < face.mNumIndices; j++ )
                {
                    *ib = face.mIndices[ j ] + cur_vertices;
                    ib++;
                }

                *( att + cur_faces + i ) = m;
            }

            cur_vertices += mesh->mNumVertices;
            cur_faces += mesh->mNumFaces;
        }

        // Skin info
        if( all_bones.size() )
        {
            D3D_HR( D3DXCreateSkinInfo( vertices_count, declaration, (uint) all_bones.size(), &skin_info ) );

            vector< vector< DWORD > > vertices( all_bones.size() );
            vector< FloatVec >        weights( all_bones.size() );

            for( uint b = 0, bb = (uint) all_bones.size(); b < bb; b++ )
            {
                aiBone* bone = all_bones[ b ];

                // Bone options
                skin_info->SetBoneName( b, bone->mName.data );
                skin_info->SetBoneOffsetMatrix( b, &ConvertMatrix( bone->mOffsetMatrix ) );

                // Reserve memory
                vertices.reserve( vertices_count );
                weights.reserve( vertices_count );
            }

            cur_vertices = 0;
            for( unsigned int m = 0; m < node->mNumMeshes; m++ )
            {
                aiMesh* mesh = scene->mMeshes[ node->mMeshes[ m ] ];

                for( unsigned int i = 0; i < mesh->mNumBones; i++ )
                {
                    aiBone* bone = mesh->mBones[ i ];

                    // Get bone id
                    uint bone_id = 0;
                    for( uint b = 0, bb = (uint) all_bones.size(); b < bb; b++ )
                    {
                        if( !strcmp( all_bones[ b ]->mName.data, bone->mName.data ) )
                        {
                            bone_id = b;
                            break;
                        }
                    }

                    // Fill weights
                    if( bone->mNumWeights )
                    {
                        for( unsigned int j = 0; j < bone->mNumWeights; j++ )
                        {
                            aiVertexWeight& vw = bone->mWeights[ j ];
                            vertices[ bone_id ].push_back( vw.mVertexId + cur_vertices );
                            weights[ bone_id ].push_back( vw.mWeight );
                        }
                    }
                }

                cur_vertices += mesh->mNumVertices;
            }

            // Set influences
            for( uint b = 0, bb = (uint) all_bones.size(); b < bb; b++ )
            {
                if( vertices[ b ].size() )
                    skin_info->SetBoneInfluence( b, (uint) vertices[ b ].size(), &vertices[ b ][ 0 ], &weights[ b ][ 0 ] );
            }
        }

        D3D_HR( dxmesh.pMesh->UnlockVertexBuffer() );
        D3D_HR( dxmesh.pMesh->UnlockIndexBuffer() );
        D3D_HR( dxmesh.pMesh->UnlockAttributeBuffer() );

        MeshContainer* mesh_container = new MeshContainer();
        memzero( mesh_container, sizeof( MeshContainer ) );
        mesh_container->Name = Str::Duplicate( node->mName.data );

        // Adjacency data - holds information about triangle adjacency, required by the ID3DMESH object
        uint faces = dxmesh.pMesh->GetNumFaces();
        mesh_container->Adjacency = new uint[ faces * 3 ];
        dxmesh.pMesh->GenerateAdjacency( 0.0000125f, (DWORD*) mesh_container->Adjacency );

        // Changed 24/09/07 - can just assign pointer and add a ref rather than need to clone
        mesh_container->Mesh = dxmesh.pMesh;
        mesh_container->Mesh->AddRef();

        // Create material and texture arrays. Note that I always want to have at least one
        mesh_container->NumMaterials = max( materials.size(), 1 );
        mesh_container->Materials = new Material_[ mesh_container->NumMaterials ];
        mesh_container->TextureNames = new char*[ mesh_container->NumMaterials ];
        mesh_container->Effects = new EffectInstance[ mesh_container->NumMaterials ];

        if( materials.size() > 0 )
        {
            // Load all the textures and copy the materials over
            for( uint i = 0; i < materials.size(); i++ )
            {
                if( materials[ i ].pTextureFilename )
                    mesh_container->TextureNames[ i ] = Str::Duplicate( materials[ i ].pTextureFilename );
                else
                    mesh_container->TextureNames[ i ] = NULL;
                mesh_container->Materials[ i ] = materials[ i ].MatD3D;
                memzero( &mesh_container->Effects[ i ], sizeof( EffectInstance ) );
            }
        }
        else
        {
            // Make a default material in the case where the mesh did not provide one
            memzero( &mesh_container->Materials[ 0 ], sizeof( Material_ ) );
            mesh_container->Materials[ 0 ].Diffuse.a = 1.0f;
            mesh_container->Materials[ 0 ].Diffuse.r = 0.5f;
            mesh_container->Materials[ 0 ].Diffuse.g = 0.5f;
            mesh_container->Materials[ 0 ].Diffuse.b = 0.5f;
            mesh_container->Materials[ 0 ].Specular = mesh_container->Materials[ 0 ].Diffuse;
            mesh_container->TextureNames[ 0 ] = NULL;
            memzero( &mesh_container->Effects[ 0 ], sizeof( EffectInstance ) );
        }

        // If there is skin data associated with the mesh copy it over
        if( skin_info )
        {
            // Save off the SkinInfo
            mesh_container->SkinInfo = skin_info;
            skin_info->AddRef();

            // Need an array of offset matrices to move the vertices from the figure space to the bone's space
            UINT numBones = skin_info->GetNumBones();
            mesh_container->BoneOffsets = new Matrix_[ numBones ];

            // Create the arrays for the bones and the frame matrices
            mesh_container->FrameCombinedMatrixPointer = new Matrix_*[ numBones ];
            memzero( mesh_container->FrameCombinedMatrixPointer, sizeof( Matrix_* ) * numBones );

            // get each of the bone offset matrices so that we don't need to get them later
            for( UINT i = 0; i < numBones; i++ )
                mesh_container->BoneOffsets[ i ] = *( mesh_container->SkinInfo->GetBoneOffsetMatrix( i ) );
        }
        else
        {
            // No skin info so NULL all the pointers
            mesh_container->SkinInfo = NULL;
            mesh_container->BoneOffsets = NULL;
            mesh_container->SkinMesh = NULL;
            mesh_container->FrameCombinedMatrixPointer = NULL;
        }

        dxmesh.pMesh->Release();
        if( skin_info )
            skin_info->Release();

        frame->Meshes = (MeshContainer*) mesh_container;
    }

    // Childs
    Frame* frame_last = NULL;
    for( unsigned int i = 0; i < node->mNumChildren; i++ )
    {
        aiNode* node_child = node->mChildren[ i ];
        Frame*  frame_child = FillNode( device, node_child, scene, with_tangent );
        if( !i )
            frame->FirstChild = frame_child;
        else
            frame_last->Sibling = frame_child;
        frame_last = frame_child;
    }

    return frame;
    #else
    return NULL;
    #endif
}

AnimSet_* GraphicLoader::LoadAnimation( Device_ device, const char* anim_fname, const char* anim_name )
{
    // Find in already loaded
    #ifdef FO_D3D
    for( uint i = 0, j = (uint) loadedAnimations.size(); i < j; i++ )
    {
        if( Str::CompareCase( loadedAnimationsFNames[ i ], anim_fname ) &&
            Str::CompareCase( loadedAnimations[ i ]->GetName(), anim_name ) )
            return loadedAnimations[ i ];
    }
    #endif

    // Check maybe file already processed and nothing founded
    for( uint i = 0, j = (uint) processedFiles.size(); i < j; i++ )
        if( Str::CompareCase( processedFiles[ i ], anim_fname ) )
            return NULL;

    // File not processed, load and recheck animations
    if( LoadModel( device, anim_fname, false ) )
        return LoadAnimation( device, anim_fname, anim_name );

    return NULL;
}

void GraphicLoader::Free( Frame* frame )
{
    // Free frame
    if( frame )
    {
        SAFEDELA( frame->Name );
        MeshContainer* mesh_container = frame->Meshes;
        while( mesh_container )
        {
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
            SAFEREL( mesh_container->Mesh );
            // Release skin information
            SAFEREL( mesh_container->SkinInfo );
            // Release blend mesh
            SAFEREL( mesh_container->SkinMeshBlended );
            // Finally delete the mesh container itself
            MeshContainer* next_container = mesh_container->NextMeshContainer;
            delete mesh_container;
            mesh_container = next_container;
        }
        if( frame->Sibling )
            Free( frame->Sibling );
        if( frame->FirstChild )
            Free( frame->FirstChild );
        delete frame;
    }
}

bool GraphicLoader::IsExtensionSupported( const char* ext )
{
    static const char* arr[] =
    {
        "fo3d", "x", "3ds", "obj", "dae", "blend", "ase", "ply", "dxf", "lwo", "lxo", "stl", "ac", "ms3d",
        "scn", "smd", "vta", "mdl", "md2", "md3", "pk3", "mdc", "md5", "bvh", "csm", "b3d", "q3d", "cob",
        "q3s", "mesh", "xml", "irrmesh", "irr", "nff", "nff", "off", "raw", "ter", "mdl", "hmp", "ndo"
    };

    for( int i = 0, j = sizeof( arr ) / sizeof( arr[ 0 ] ); i < j; i++ )
        if( Str::CompareCase( ext, arr[ i ] ) )
            return true;
    return false;
}

/************************************************************************/
/* Textures                                                             */
/************************************************************************/
TextureVec GraphicLoader::loadedTextures;

Texture* GraphicLoader::LoadTexture( Device_ device, const char* texture_name, const char* model_path )
{
    if( !texture_name || !texture_name[ 0 ] )
        return NULL;

    // Try find already loaded texture
    for( auto it = loadedTextures.begin(), end = loadedTextures.end(); it != end; ++it )
    {
        Texture* texture = *it;
        if( Str::CompareCase( texture->Name, texture_name ) )
            return texture;
    }

    // First try load from textures folder
    FileManager fm;
    if( !fm.LoadFile( texture_name, PT_TEXTURES ) && model_path )
    {
        // After try load from file folder
        char path[ MAX_FOPATH ];
        FileManager::ExtractPath( model_path, path );
        Str::Append( path, texture_name );
        fm.LoadFile( path, PT_DATA );
    }

    // Create texture
    Texture*           texture_ex = new Texture();
    #ifdef FO_D3D
    LPDIRECT3DTEXTURE9 dxtex = NULL;
    if( !fm.IsLoaded() || FAILED( D3DXCreateTextureFromFileInMemory( device, fm.GetBuf(), fm.GetFsize(), &dxtex ) ) )
        return NULL;
    texture_ex->Instance = dxtex;
    #endif
    texture_ex->Name = Str::Duplicate( texture_name );
    loadedTextures.push_back( texture_ex );
    return loadedTextures.back();
}

void GraphicLoader::FreeTexture( Texture* texture )
{
    #ifdef FO_D3D
    if( texture )
    {
        for( auto it = loadedTextures.begin(), end = loadedTextures.end(); it != end; ++it )
        {
            Texture* texture_ = *it;
            if( texture_ == texture )
            {
                loadedTextures.erase( it );
                break;
            }
        }

        SAFEDELA( texture->Name );
        SAFEREL( texture->Instance );
        SAFEDEL( texture );
    }
    else
    {
        TextureVec textures = loadedTextures;
        for( auto it = textures.begin(), end = textures.end(); it != end; ++it )
            FreeTexture( *it );
    }
    #endif
}

/************************************************************************/
/* Effects                                                              */
/************************************************************************/
#ifdef FO_D3D
class IncludeParser: public ID3DXInclude
{
public:
    char* RootPath;

    STDMETHOD( Open ) ( THIS_ D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID * ppData, UINT * pBytes )
    {
        FileManager fm;
        if( !fm.LoadFile( pFileName, PT_EFFECTS ) )
        {
            if( !RootPath ) return S_FALSE;
            char path[ MAX_FOPATH ];
            FileManager::ExtractPath( RootPath, path );
            Str::Append( path, pFileName );
            if( !fm.LoadFile( path, PT_DATA ) ) return S_FALSE;
        }
        *pBytes = fm.GetFsize();
        *ppData = fm.ReleaseBuffer();
        return S_OK;
    }

    STDMETHOD( Close ) ( THIS_ LPCVOID pData )
    {
        if( pData ) delete[] pData;
        return S_OK;
    }
} includeParser;
#endif

EffectVec GraphicLoader::loadedEffects;

Effect* GraphicLoader::LoadEffect( Device_ device, const char* effect_name )
{
    EffectInstance effect_inst;
    memzero( &effect_inst, sizeof( effect_inst ) );
    effect_inst.EffectFilename = (char*) effect_name;
    return LoadEffect( device, &effect_inst, NULL );
}

Effect* GraphicLoader::LoadEffect( Device_ device, EffectInstance* effect_inst, const char* model_path )
{
    if( !effect_inst || !effect_inst->EffectFilename || !effect_inst->EffectFilename[ 0 ] )
        return NULL;
    const char* effect_name = effect_inst->EffectFilename;

    // Try find already loaded texture
    for( auto it = loadedEffects.begin(), end = loadedEffects.end(); it != end; ++it )
    {
        Effect* effect_ex = *it;
        if( Str::CompareCase( effect_ex->Name, effect_name ) && effect_ex->Defaults == effect_inst->Defaults )
            return effect_ex;
    }

    #ifdef FO_D3D
    // First try load from effects folder
    FileManager fm;
    if( !fm.LoadFile( effect_name, PT_EFFECTS ) && model_path )
    {
        // After try load from file folder
        char path[ MAX_FOPATH ];
        FileManager::ExtractPath( model_path, path );
        Str::Append( path, effect_name );
        fm.LoadFile( path, PT_DATA );
    }
    if( !fm.IsLoaded() )
        return NULL;

    // Find already compiled effect in cache
    char        cache_name[ MAX_FOPATH ];
    sprintf( cache_name, "%s.fxc", effect_name );
    FileManager fm_cache;
    if( fm_cache.LoadFile( cache_name, PT_CACHE ) )
    {
        uint64 last_write, last_write_cache;
        fm.GetTime( NULL, NULL, &last_write );
        fm_cache.GetTime( NULL, NULL, &last_write_cache );
        if( last_write > last_write_cache )
            fm_cache.UnloadFile();
    }

    // Load and cache effect
    if( !fm_cache.IsLoaded() )
    {
        char*                buf = (char*) fm.GetBuf();
        uint                 size = fm.GetFsize();

        LPD3DXEFFECTCOMPILER ef_comp = NULL;
        LPD3DXBUFFER         ef_buf = NULL;
        LPD3DXBUFFER         errors = NULL;
        LPD3DXBUFFER         errors31 = NULL;
        HRESULT              hr = 0, hr31 = 0;

        includeParser.RootPath = (char*) model_path;
        hr = D3DXCreateEffectCompiler( buf, size, NULL, &includeParser, 0, &ef_comp, &errors );
        if( FAILED( hr ) )
            hr31 = D3DXCreateEffectCompiler( buf, size, NULL, &includeParser, D3DXSHADER_USE_LEGACY_D3DX9_31_DLL, &ef_comp, &errors31 );

        if( SUCCEEDED( hr ) || SUCCEEDED( hr31 ) )
        {
            SAFEREL( errors );
            if( SUCCEEDED( ef_comp->CompileEffect( 0, &ef_buf, &errors ) ) )
            {
                fm_cache.SetData( ef_buf->GetBufferPointer(), ef_buf->GetBufferSize() );
                fm_cache.SaveOutBufToFile( cache_name, PT_CACHE );
                fm_cache.SwitchToRead();
            }
            else
            {
                WriteLogF( _FUNC_, " - Unable to compile effect, effect<%s>, errors<\n%s>.\n", effect_name, errors ? errors->GetBufferPointer() : "nullptr\n" );
            }
        }
        else
        {
            WriteLogF( _FUNC_, " - Unable to create effect compiler, effect<%s>, errors<%s\n%s>, legacy compiler errors<%s\n%s>.\n", effect_name,
                       DXGetErrorString( hr ), errors ? errors->GetBufferPointer() : "", DXGetErrorString( hr31 ), errors31 ? errors31->GetBufferPointer() : "" );
        }

        SAFEREL( ef_comp );
        SAFEREL( ef_buf );
        SAFEREL( errors );
        SAFEREL( errors31 );

        if( !fm_cache.IsLoaded() )
            return NULL;
    }

    // Create effect
    LPD3DXEFFECT dxeffect = NULL;
    LPD3DXBUFFER errors = NULL;
    if( FAILED( D3DXCreateEffect( device, fm_cache.GetBuf(), fm_cache.GetFsize(), NULL, NULL, /*D3DXSHADER_SKIPVALIDATION|*/ D3DXFX_NOT_CLONEABLE, NULL, &dxeffect, &errors ) ) )
    {
        WriteLogF( _FUNC_, " - Unable to create effect, effect<%s>, errors<\n%s>.\n", effect_name, errors ? errors->GetBufferPointer() : "nullptr" );
        SAFEREL( dxeffect );
        SAFEREL( errors );
        return NULL;
    }
    SAFEREL( errors );

    Effect* effect = new Effect();
    effect->Name = Str::Duplicate( effect_name );
    effect->DXInstance = dxeffect;
    effect->EffectFlags = D3DXFX_DONOTSAVESTATE;
    effect->Defaults = NULL;
    effect->EffectParams = NULL;

    effect->TechniqueSkinned = dxeffect->GetTechniqueByName( "Skinned" );
    effect->TechniqueSkinnedWithShadow = dxeffect->GetTechniqueByName( "SkinnedWithShadow" );
    effect->TechniqueSimple = dxeffect->GetTechniqueByName( "Simple" );
    effect->TechniqueSimpleWithShadow = dxeffect->GetTechniqueByName( "SimpleWithShadow" );
    effect->BonesInfluences = dxeffect->GetParameterByName( NULL, "BonesInfluences" );
    effect->GroundPosition = dxeffect->GetParameterByName( NULL, "GroundPosition" );
    effect->LightDir = dxeffect->GetParameterByName( NULL, "LightDir" );
    effect->LightDiffuse = dxeffect->GetParameterByName( NULL, "LightDiffuse" );
    effect->MaterialAmbient = dxeffect->GetParameterByName( NULL, "MaterialAmbient" );
    effect->MaterialDiffuse = dxeffect->GetParameterByName( NULL, "MaterialDiffuse" );
    effect->WorldMatrices = dxeffect->GetParameterByName( NULL, "WorldMatrices" );
    effect->ViewProjMatrix = dxeffect->GetParameterByName( NULL, "ViewProjMatrix" );

    if( !effect->TechniqueSimple )
    {
        WriteLogF( _FUNC_, " - Technique 'Simple' not founded, effect<%s>.\n", effect_name );
        delete effect;
        SAFEREL( dxeffect );
        return NULL;
    }

    if( !effect->TechniqueSimpleWithShadow )
        effect->TechniqueSimpleWithShadow = effect->TechniqueSimple;
    if( !effect->TechniqueSkinned )
        effect->TechniqueSkinned = effect->TechniqueSimple;
    if( !effect->TechniqueSkinnedWithShadow )
        effect->TechniqueSkinnedWithShadow = effect->TechniqueSkinned;

    effect->PassIndex = dxeffect->GetParameterByName( NULL, "PassIndex" );
    effect->Time = dxeffect->GetParameterByName( NULL, "Time" );
    effect->TimeCurrent = 0.0f;
    effect->TimeLastTick = Timer::AccurateTick();
    effect->TimeGame = dxeffect->GetParameterByName( NULL, "TimeGame" );
    effect->TimeGameCurrent = 0.0f;
    effect->TimeGameLastTick = Timer::AccurateTick();
    effect->IsTime = ( effect->Time || effect->TimeGame );
    effect->Random1Pass = dxeffect->GetParameterByName( NULL, "Random1Pass" );
    effect->Random2Pass = dxeffect->GetParameterByName( NULL, "Random2Pass" );
    effect->Random3Pass = dxeffect->GetParameterByName( NULL, "Random3Pass" );
    effect->Random4Pass = dxeffect->GetParameterByName( NULL, "Random4Pass" );
    effect->IsRandomPass = ( effect->Random1Pass || effect->Random2Pass || effect->Random3Pass || effect->Random4Pass );
    effect->Random1Effect = dxeffect->GetParameterByName( NULL, "Random1Effect" );
    effect->Random2Effect = dxeffect->GetParameterByName( NULL, "Random2Effect" );
    effect->Random3Effect = dxeffect->GetParameterByName( NULL, "Random3Effect" );
    effect->Random4Effect = dxeffect->GetParameterByName( NULL, "Random4Effect" );
    effect->IsRandomEffect = ( effect->Random1Effect || effect->Random2Effect || effect->Random3Effect || effect->Random4Effect );
    effect->IsTextures = false;
    for( int i = 0; i < EFFECT_TEXTURES; i++ )
    {
        char tex_name[ 32 ];
        sprintf( tex_name, "Texture%d", i );
        effect->Textures[ i ] = dxeffect->GetParameterByName( NULL, tex_name );
        if( effect->Textures[ i ] )
            effect->IsTextures = true;
    }
    effect->IsScriptValues = false;
    for( int i = 0; i < EFFECT_SCRIPT_VALUES; i++ )
    {
        char val_name[ 32 ];
        sprintf( val_name, "EffectValue%d", i );
        effect->ScriptValues[ i ] = dxeffect->GetParameterByName( NULL, val_name );
        if( effect->ScriptValues[ i ] )
            effect->IsScriptValues = true;
    }
    effect->AnimPosProc = dxeffect->GetParameterByName( NULL, "AnimPosProc" );
    effect->AnimPosTime = dxeffect->GetParameterByName( NULL, "AnimPosTime" );
    effect->IsAnimPos = ( effect->AnimPosProc || effect->AnimPosTime );
    effect->IsNeedProcess = ( effect->PassIndex || effect->IsTime || effect->IsRandomPass || effect->IsRandomEffect ||
                              effect->IsTextures || effect->IsScriptValues || effect->IsAnimPos );

    if( effect_inst->DefaultsCount )
    {
        dxeffect->BeginParameterBlock();
        for( uint d = 0; d < effect_inst->DefaultsCount; d++ )
        {
            EffectDefault& def = effect_inst->Defaults[ d ];
            EffectValue_   param = dxeffect->GetParameterByName( NULL, def.Name );
            if( !param )
                continue;
            switch( def.Type )
            {
            case D3DXEDT_STRING:             // pValue points to a null terminated ASCII string
                dxeffect->SetString( param, (LPCSTR) def.Data );
                break;
            case D3DXEDT_FLOATS:             // pValue points to an array of floats - number of floats is NumBytes / sizeof(float)
                dxeffect->SetFloatArray( param, (FLOAT*) def.Data, def.Size / sizeof( FLOAT ) );
                break;
            case D3DXEDT_DWORD:              // pValue points to a uint
                dxeffect->SetInt( param, *(uint*) def.Data );
                break;
            default:
                break;
            }
        }
        effect->EffectParams = dxeffect->EndParameterBlock();
        effect->Defaults = effect_inst->Defaults;
    }

    loadedEffects.push_back( effect );
    return loadedEffects.back();
    #else
    // Make names
    char vs_fname[ MAX_FOPATH ];
    Str::Copy( vs_fname, effect_name );
    FileManager::EraseExtension( vs_fname );
    Str::Append( vs_fname, ".vert" );
    char fs_fname[ MAX_FOPATH ];
    Str::Copy( fs_fname, effect_name );
    FileManager::EraseExtension( fs_fname );
    Str::Append( fs_fname, ".frag" );

    // Load files
    FileManager vs_file;
    if( !vs_file.LoadFile( vs_fname, PT_EFFECTS ) )
    {
        WriteLogF( _FUNC_, " - Vertex shader file<%s> not found.\n", vs_fname );
        return NULL;
    }
    FileManager fs_file;
    if( !fs_file.LoadFile( fs_fname, PT_EFFECTS ) )
    {
        WriteLogF( _FUNC_, " - Fragment shader file<%s> not found.\n", fs_fname );
        return NULL;
    }
    const char* vs_str = (char*) vs_file.GetBuf();
    const char* fs_str = (char*) fs_file.GetBuf();

    // Create shaders
    GLuint vs, fs;
    GL( vs = glCreateShader( GL_VERTEX_SHADER ) );
    GL( fs = glCreateShader( GL_FRAGMENT_SHADER ) );
    GL( glShaderSource( vs, 1, (const GLchar**) &vs_str, NULL ) );
    GL( glShaderSource( fs, 1, (const GLchar**) &fs_str, NULL ) );

    // Info parser
    struct ShaderInfo
    {
        static void Log( const char* shader_name, GLint shader )
        {
            int len = 0;
            GL( glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &len ) );
            if( len > 0 )
            {
                GLchar* str = new GLchar[ len ];
                int     chars = 0;
                glGetShaderInfoLog( shader, len, &chars, str );
                WriteLog( "Shader<%s> info output:\n%s", shader_name, str );
                delete[] str;
            }
        }
        static void LogProgram( GLint program )
        {
            int len = 0;
            GL( glGetProgramiv( program, GL_INFO_LOG_LENGTH, &len ) );
            if( len > 0 )
            {
                GLchar* str = new GLchar[ len ];
                int     chars = 0;
                glGetProgramInfoLog( program, len, &chars, str );
                WriteLog( "Program info output:\n%s", str );
                delete[] str;
            }
        }
    };

    // Compile vs
    GLint compiled;
    GL( glCompileShader( vs ) );
    GL( glGetShaderiv( vs, GL_COMPILE_STATUS, &compiled ) );
    if( !compiled )
    {
        WriteLogF( _FUNC_, " - Vertex shader<%s> not compiled.\n", vs_fname );
        ShaderInfo::Log( vs_fname, vs );
        GL( glDeleteShader( vs ) );
        GL( glDeleteShader( fs ) );
        return NULL;
    }

    // Compile fs
    GL( glCompileShader( fs ) );
    GL( glGetShaderiv( fs, GL_COMPILE_STATUS, &compiled ) );
    if( !compiled )
    {
        WriteLogF( _FUNC_, " - Fragment shader<%s> not compiled.\n", fs_fname );
        ShaderInfo::Log( fs_fname, fs );
        GL( glDeleteShader( vs ) );
        GL( glDeleteShader( fs ) );
        return NULL;
    }

    // Make program
    GLuint program;
    GL( program = glCreateProgram() );
    GL( glAttachShader( program, vs ) );
    GL( glAttachShader( program, fs ) );

    GL( glBindAttribLocation( program, 0, "InPosition" ) );
    GL( glBindAttribLocation( program, 1, "InColor" ) );
    GL( glBindAttribLocation( program, 2, "InTexCoord" ) );
    GL( glBindAttribLocation( program, 3, "InTexEggCoord" ) );

    GL( glLinkProgram( program ) );
    GLint linked;
    GL( glGetProgramiv( program, GL_LINK_STATUS, &linked ) );
    if( !linked )
    {
        WriteLogF( _FUNC_, " - Failed to link shader program, vs<%s>, fs<%s>.\n", vs_fname, fs_fname );
        ShaderInfo::LogProgram( program );
        GL( glDetachShader( program, vs ) );
        GL( glDetachShader( program, fs ) );
        GL( glDeleteShader( vs ) );
        GL( glDeleteShader( fs ) );
        GL( glDeleteProgram( program ) );
        return NULL;
    }

    // Create effect instance
    Effect* effect = new Effect();
    memzero( effect, sizeof( Effect ) );
    effect->Name = Str::Duplicate( effect_name );
    effect->Program = program;
    effect->VertexShader = vs;
    effect->FragmentShader = fs;
    effect->Defaults = NULL;
    effect->Passes = 1;

    // Bind data
    GL( effect->ColorMap = glGetUniformLocation( program, "ColorMap" ) );
    GL( effect->EggMap = glGetUniformLocation( program, "EggMap" ) );

    return effect;
    #endif
}

void GraphicLoader::EffectProcessVariables( Effect* effect_ex, int pass,  float anim_proc /* = 0.0f */, float anim_time /* = 0.0f */, Texture** textures /* = NULL */ )
{
    #ifdef FO_D3D
    // Process effect
    if( pass == -1 )
    {
        if( effect_ex->IsTime )
        {
            double tick = Timer::AccurateTick();
            if( effect_ex->Time )
            {
                effect_ex->TimeCurrent += (float) ( tick - effect_ex->TimeLastTick );
                effect_ex->TimeLastTick = tick;
                if( effect_ex->TimeCurrent >= 120.0f )
                    effect_ex->TimeCurrent = fmod( effect_ex->TimeCurrent, 120.0f );

                effect_ex->DXInstance->SetFloat( effect_ex->Time, effect_ex->TimeCurrent );
            }
            if( effect_ex->TimeGame )
            {
                if( !Timer::IsGamePaused() )
                {
                    effect_ex->TimeGameCurrent += (float) ( tick - effect_ex->TimeGameLastTick );
                    effect_ex->TimeGameLastTick = tick;
                    if( effect_ex->TimeGameCurrent >= 120.0f )
                        effect_ex->TimeGameCurrent = fmod( effect_ex->TimeGameCurrent, 120.0f );
                }
                else
                {
                    effect_ex->TimeGameLastTick = tick;
                }

                effect_ex->DXInstance->SetFloat( effect_ex->TimeGame, effect_ex->TimeGameCurrent );
            }
        }

        if( effect_ex->IsRandomEffect )
        {
            if( effect_ex->Random1Effect )
                effect_ex->DXInstance->SetFloat( effect_ex->Random1Effect, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( effect_ex->Random2Effect )
                effect_ex->DXInstance->SetFloat( effect_ex->Random2Effect, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( effect_ex->Random3Effect )
                effect_ex->DXInstance->SetFloat( effect_ex->Random3Effect, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( effect_ex->Random4Effect )
                effect_ex->DXInstance->SetFloat( effect_ex->Random4Effect, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
        }

        if( effect_ex->IsTextures )
        {
            for( int i = 0; i < EFFECT_TEXTURES; i++ )
                if( effect_ex->Textures[ i ] )
                    effect_ex->DXInstance->SetTexture( effect_ex->Textures[ i ], textures && textures[ i ] ? textures[ i ]->Instance : NULL );
        }

        if( effect_ex->IsScriptValues )
        {
            for( int i = 0; i < EFFECT_SCRIPT_VALUES; i++ )
                if( effect_ex->ScriptValues[ i ] )
                    effect_ex->DXInstance->SetFloat( effect_ex->ScriptValues[ i ], GameOpt.EffectValues[ i ] );
        }

        if( effect_ex->IsAnimPos )
        {
            if( effect_ex->AnimPosProc )
                effect_ex->DXInstance->SetFloat( effect_ex->AnimPosProc, anim_proc );
            if( effect_ex->AnimPosTime )
                effect_ex->DXInstance->SetFloat( effect_ex->AnimPosTime, anim_time );
        }
    }
    // Process pass
    else
    {
        if( effect_ex->PassIndex )
            effect_ex->DXInstance->SetFloat( effect_ex->Random1Pass, (float) pass );

        if( effect_ex->IsRandomPass )
        {
            if( effect_ex->Random1Pass )
                effect_ex->DXInstance->SetFloat( effect_ex->Random1Pass, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( effect_ex->Random2Pass )
                effect_ex->DXInstance->SetFloat( effect_ex->Random2Pass, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( effect_ex->Random3Pass )
                effect_ex->DXInstance->SetFloat( effect_ex->Random3Pass, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( effect_ex->Random4Pass )
                effect_ex->DXInstance->SetFloat( effect_ex->Random4Pass, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
        }
    }
    #endif
}

bool GraphicLoader::EffectsPreRestore()
{
    #ifdef FO_D3D
    for( auto it = loadedEffects.begin(), end = loadedEffects.end(); it != end; ++it )
    {
        Effect* effect_ex = *it;
        D3D_HR( effect_ex->DXInstance->OnLostDevice() );
    }
    #endif
    return true;
}

bool GraphicLoader::EffectsPostRestore()
{
    #ifdef FO_D3D
    for( auto it = loadedEffects.begin(), end = loadedEffects.end(); it != end; ++it )
    {
        Effect* effect_ex = *it;
        D3D_HR( effect_ex->DXInstance->OnResetDevice() );
    }
    #endif
    return true;
}

/*
   Todo:
        if(Name) delete Name;
        if(Effect)
        {
                if(EffectParams) Effect->DeleteParameterBlock(EffectParams);
                Effect->Release();
                Effect=NULL;
        }
 */

/************************************************************************/
/*                                                                      */
/************************************************************************/
