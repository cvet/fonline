#include "StdAfx.h"
#include "GraphicLoader.h"
#include "3dAnimation.h"
#include "Text.h"
#include "Timer.h"
#include "Version.h"

#include "Assimp/assimp.h"
#include "Assimp/aiPostProcess.h"

// Assimp functions
const aiScene* ( *Ptr_aiImportFileFromMemory )( const char* pBuffer, unsigned int pLength, unsigned int pFlags, const char* pHint );
void           ( * Ptr_aiReleaseImport )( const aiScene* pScene );
const char*    ( *Ptr_aiGetErrorString )( );
void           ( * Ptr_aiEnableVerboseLogging )( aiBool d );
aiLogStream    ( * Ptr_aiGetPredefinedLogStream )( aiDefaultLogStream pStreams, const char* file );
void           ( * Ptr_aiAttachLogStream )( const aiLogStream* stream );
unsigned int   ( * Ptr_aiGetMaterialTextureCount )( const aiMaterial* pMat, aiTextureType type );
aiReturn       ( * Ptr_aiGetMaterialTexture )( const aiMaterial* mat, aiTextureType type, unsigned int  index, aiString* path, aiTextureMapping* mapping, unsigned int* uvindex, float* blend, aiTextureOp* op, aiTextureMapMode* mapmode, unsigned int* flags );
aiReturn       ( * Ptr_aiGetMaterialFloatArray )( const aiMaterial* pMat, const char* pKey, unsigned int type, unsigned int index, float* pOut, unsigned int* pMax );

/************************************************************************/
/* Models                                                               */
/************************************************************************/

PCharVec GraphicLoader::processedFiles;
FrameVec GraphicLoader::loadedModels;
PCharVec GraphicLoader::loadedAnimationsFNames;
PtrVec   GraphicLoader::loadedAnimations;

Frame* GraphicLoader::LoadModel( Device_ device, const char* fname )
{
    // Load Assimp dynamic library
    static bool binded = false;
    static bool binded_try = false;
    if( !binded )
    {
        // Already try
        if( binded_try )
            return NULL;
        binded_try = true;

        // Library extension
        #ifdef FO_WINDOWS
        # define ASSIMP_PATH        ""
        # define ASSIMP_LIB_NAME    "Assimp32.dll"
        #else // FO_LINUX
        # define ASSIMP_PATH        "./"
        # define ASSIMP_LIB_NAME    "Assimp32.so"
        #endif

        // Check dll availability
        void* dll = DLL_Load( ASSIMP_PATH ASSIMP_LIB_NAME );
        if( !dll )
        {
            if( GameOpt.ClientPath != "" )
                dll = DLL_Load( ( GameOpt.ClientPath + ASSIMP_LIB_NAME ).c_str() );
            if( !dll )
            {
                WriteLogF( _FUNC_, " - '" ASSIMP_LIB_NAME "' not found.\n" );
                return NULL;
            }
        }

        // Bind functions
        uint errors = 0;
        #define BIND_ASSIMP_FUNC( f )                                            \
            Ptr_ ## f = ( decltype( Ptr_ ## f ) )DLL_GetAddress( dll, # f );     \
            if( !Ptr_ ## f )                                                     \
            {                                                                    \
                WriteLogF( _FUNC_, " - Assimp function<" # f "> not found.\n" ); \
                errors++;                                                        \
            }
        BIND_ASSIMP_FUNC( aiImportFileFromMemory );
        BIND_ASSIMP_FUNC( aiReleaseImport );
        BIND_ASSIMP_FUNC( aiGetErrorString );
        BIND_ASSIMP_FUNC( aiEnableVerboseLogging );
        BIND_ASSIMP_FUNC( aiGetPredefinedLogStream );
        BIND_ASSIMP_FUNC( aiAttachLogStream );
        BIND_ASSIMP_FUNC( aiGetMaterialTextureCount );
        BIND_ASSIMP_FUNC( aiGetMaterialTexture );
        BIND_ASSIMP_FUNC( aiGetMaterialFloatArray );
        #undef BIND_ASSIMP_FUNC
        if( errors )
            return NULL;
        binded = true;

        // Logging
        if( false )
        {
            Ptr_aiEnableVerboseLogging( true );
            static aiLogStream c = Ptr_aiGetPredefinedLogStream( aiDefaultLogStream_FILE, "Assimp.log" );
            Ptr_aiAttachLogStream( &c );
        }
    }

    // Find already loaded
    for( auto it = loadedModels.begin(), end = loadedModels.end(); it != end; ++it )
    {
        Frame* frame = *it;
        if( Str::CompareCase( frame->GetName(), fname ) )
            return frame;
    }

    // Add to already processed
    for( uint i = 0, j = (uint) processedFiles.size(); i < j; i++ )
        if( Str::CompareCase( processedFiles[ i ], fname ) )
            return NULL;
    processedFiles.push_back( Str::Duplicate( fname ) );

    // Load file
    FileManager fm;
    if( !fm.LoadFile( fname, PT_DATA ) )
    {
        WriteLogF( _FUNC_, " - 3d file not found, name<%s>.\n", fname );
        return NULL;
    }

    // Load scene
    aiScene* scene = (aiScene*) Ptr_aiImportFileFromMemory( (const char*) fm.GetBuf(), fm.GetFsize(),
                                                            aiProcess_CalcTangentSpace | aiProcess_GenNormals | aiProcess_GenUVCoords |
                                                            aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                                                            aiProcess_SortByPType | aiProcess_SplitLargeMeshes | aiProcess_LimitBoneWeights |
                                                            aiProcess_ImproveCacheLocality
                                                            #ifdef FO_D3D
                                                            | aiProcess_ConvertToLeftHanded
                                                            #endif
                                                            , "" );
    if( !scene )
    {
        WriteLogF( _FUNC_, " - Can't load 3d file, name<%s>, error<%s>.\n", fname, Ptr_aiGetErrorString() );
        return NULL;
    }

    // Extract frames
    #ifdef FO_D3D
    Frame* frame_root = FillNode( device, scene->mRootNode, scene );
    if( !frame_root )
    {
        WriteLogF( _FUNC_, " - Conversion fail, name<%s>.\n", fname );
        Ptr_aiReleaseImport( scene );
        return NULL;
    }
    #else
    Frame* frame_root = FillNode( scene, scene->mRootNode );
    FixFrame( frame_root, frame_root, scene, scene->mRootNode );
    #endif
    frame_root->Name = Str::Duplicate( fname );
    loadedModels.push_back( frame_root );

    // Extract animations
    FloatVec      st;
    VectorVec     sv;
    FloatVec      rt;
    QuaternionVec rv;
    FloatVec      tt;
    VectorVec     tv;
    for( unsigned int i = 0; i < scene->mNumAnimations; i++ )
    {
        aiAnimation* anim = scene->mAnimations[ i ];
        AnimSet*     anim_set = new AnimSet();

        for( unsigned int j = 0; j < anim->mNumChannels; j++ )
        {
            aiNodeAnim* na = anim->mChannels[ j ];

            st.resize( na->mNumScalingKeys );
            sv.resize( na->mNumScalingKeys );
            for( unsigned int k = 0; k < na->mNumScalingKeys; k++ )
            {
                st[ k ] = (float) na->mScalingKeys[ k ].mTime;
                sv[ k ] = na->mScalingKeys[ k ].mValue;
            }
            rt.resize( na->mNumRotationKeys );
            rv.resize( na->mNumRotationKeys );
            for( unsigned int k = 0; k < na->mNumRotationKeys; k++ )
            {
                rt[ k ] = (float) na->mRotationKeys[ k ].mTime;
                rv[ k ] = na->mRotationKeys[ k ].mValue;
            }
            tt.resize( na->mNumPositionKeys );
            tv.resize( na->mNumPositionKeys );
            for( unsigned int k = 0; k < na->mNumPositionKeys; k++ )
            {
                tt[ k ] = (float) na->mPositionKeys[ k ].mTime;
                tv[ k ] = na->mPositionKeys[ k ].mValue;
            }

            anim_set->AddOutput( na->mNodeName.data, st, sv, rt, rv, tt, tv );
        }

        anim_set->SetData( anim->mName.data, (float) anim->mDuration, (float) anim->mTicksPerSecond );
        loadedAnimations.push_back( anim_set );
        loadedAnimationsFNames.push_back( Str::Duplicate( fname ) );
    }

    Ptr_aiReleaseImport( scene );
    return frame_root;
}

#ifdef FO_D3D
Frame* GraphicLoader::FillNode( Device_ device, const aiNode* node, const aiScene* scene )
{
    // Create frame
    Frame* frame = new Frame();
    memzero( frame, sizeof( Frame ) );
    frame->Name = Str::Duplicate( node->mName.data );
    frame->DrawMesh = NULL;
    frame->Sibling = NULL;
    frame->FirstChild = NULL;
    frame->TransformationMatrix = node->mTransformation;
    MATRIX_TRANSPOSE( frame->TransformationMatrix );
    frame->CombinedTransformationMatrix = Matrix();

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
                    if( Str::Compare( all_bones[ b ]->mName.data, bone->mName.data ) )
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
            if( Ptr_aiGetMaterialTextureCount( mtrl, aiTextureType_DIFFUSE ) )
            {
                Ptr_aiGetMaterialTexture( mtrl, aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL, NULL );
                material.pTextureFilename = Str::Duplicate( path.data );
            }

            Ptr_aiGetMaterialFloatArray( mtrl, AI_MATKEY_COLOR_DIFFUSE, (float*) &material.MatD3D.Diffuse, NULL );
            Ptr_aiGetMaterialFloatArray( mtrl, AI_MATKEY_COLOR_AMBIENT, (float*) &material.MatD3D.Ambient, NULL );
            Ptr_aiGetMaterialFloatArray( mtrl, AI_MATKEY_COLOR_SPECULAR, (float*) &material.MatD3D.Specular, NULL );
            Ptr_aiGetMaterialFloatArray( mtrl, AI_MATKEY_COLOR_EMISSIVE, (float*) &material.MatD3D.Emissive, NULL );
            material.MatD3D.Diffuse.a = 1.0f;
            material.MatD3D.Ambient.a = 1.0f;
            material.MatD3D.Specular.a = 1.0f;
            material.MatD3D.Emissive.a = 1.0f;

            materials.push_back( material );
        }

        // Mesh declarations
        D3DVERTEXELEMENT9 declaration[] =
        {
            { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
            { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            { 0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
            { 0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },
            D3DDECL_END()
        };

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
        int    vsize = 56;
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
                if( false && mesh->mTangents )
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
                skin_info->SetBoneOffsetMatrix( b, (D3DXMATRIX*) &bone->mOffsetMatrix );

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
                        if( Str::Compare( all_bones[ b ]->mName.data, bone->mName.data ) )
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

        // Adjacency data - holds information about triangle adjacency, required by the ID3DMESH object
        uint faces = dxmesh.pMesh->GetNumFaces();
        mesh_container->Adjacency = new uint[ faces * 3 ];
        dxmesh.pMesh->GenerateAdjacency( 0.0000125f, (DWORD*) mesh_container->Adjacency );

        // Changed 24/09/07 - can just assign pointer and add a ref rather than need to clone
        mesh_container->InitMesh = dxmesh.pMesh;
        mesh_container->InitMesh->AddRef();

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
            mesh_container->Skin = skin_info;
            skin_info->AddRef();

            // Need an array of offset matrices to move the vertices from the figure space to the bone's space
            UINT numBones = skin_info->GetNumBones();
            mesh_container->BoneOffsets = new Matrix[ numBones ];

            // Create the arrays for the bones and the frame matrices
            mesh_container->FrameCombinedMatrixPointer = new Matrix*[ numBones ];
            memzero( mesh_container->FrameCombinedMatrixPointer, sizeof( Matrix* ) * numBones );

            // get each of the bone offset matrices so that we don't need to get them later
            for( UINT i = 0; i < numBones; i++ )
                mesh_container->BoneOffsets[ i ] = *( (Matrix*) mesh_container->Skin->GetBoneOffsetMatrix( i ) );
        }
        else
        {
            // No skin info so NULL all the pointers
            mesh_container->Skin = NULL;
            mesh_container->BoneOffsets = NULL;
            mesh_container->FrameCombinedMatrixPointer = NULL;
            mesh_container->SkinMesh = NULL;
        }

        dxmesh.pMesh->Release();
        if( skin_info )
            skin_info->Release();

        frame->DrawMesh = mesh_container;
    }

    // Childs
    Frame* frame_last = NULL;
    for( unsigned int i = 0; i < node->mNumChildren; i++ )
    {
        aiNode* node_child = node->mChildren[ i ];
        Frame*  frame_child = FillNode( device, node_child, scene );
        if( !i )
            frame->FirstChild = frame_child;
        else
            frame_last->Sibling = frame_child;
        frame_last = frame_child;
    }

    return frame;
}
#else
Frame* GraphicLoader::FillNode( aiScene* scene, aiNode* node )
{
    Frame* frame = new Frame();
    frame->Name = node->mName.data;
    frame->TransformationMatrix = node->mTransformation;
    frame->CombinedTransformationMatrix = Matrix();
    frame->Children.resize( node->mNumChildren );
    frame->Mesh.resize( node->mNumMeshes );

    for( uint m = 0; m < node->mNumMeshes; m++ )
    {
        aiMesh*     aimesh = scene->mMeshes[ node->mMeshes[ m ] ];
        MeshSubset& ms = frame->Mesh[ m ];

        // Vertices
        ms.Vertices.resize( aimesh->mNumVertices );
        ms.VerticesTransformed.resize( aimesh->mNumVertices );
        bool has_color0 = aimesh->HasVertexColors( 0 );
        // bool has_color1 = aimesh->HasVertexColors( 1 );
        bool has_tex_coords0 = aimesh->HasTextureCoords( 0 );
        bool has_tex_coords1 = aimesh->HasTextureCoords( 1 );
        bool has_tex_coords2 = aimesh->HasTextureCoords( 2 );
        for( uint i = 0; i < aimesh->mNumVertices; i++ )
        {
            Vertex3D& v = ms.Vertices[ i ];
            memzero( &v, sizeof( v ) );
            v.Position = aimesh->mVertices[ i ];
            v.PositionW = 1.0f;
            v.Normal = aimesh->mNormals[ i ];
            v.Tangent = aimesh->mTangents[ i ];
            v.Bitangent = aimesh->mBitangents[ i ];
            if( has_color0 )
            {
                v.Color[ 2 ] = aimesh->mColors[ 0 ][ i ].r;
                v.Color[ 1 ] = aimesh->mColors[ 0 ][ i ].g;
                v.Color[ 0 ] = aimesh->mColors[ 0 ][ i ].b;
                v.Color[ 3 ] = aimesh->mColors[ 0 ][ i ].a;
            }
            /*if( has_color1 )
               {
                v.Color2[ 2 ] = aimesh->mColors[ 1 ][ i ].r;
                v.Color2[ 1 ] = aimesh->mColors[ 1 ][ i ].g;
                v.Color2[ 0 ] = aimesh->mColors[ 1 ][ i ].b;
                v.Color2[ 3 ] = aimesh->mColors[ 1 ][ i ].a;
               }*/
            if( has_tex_coords0 )
            {
                v.TexCoord[ 0 ] = aimesh->mTextureCoords[ 0 ][ i ].x;
                v.TexCoord[ 1 ] = aimesh->mTextureCoords[ 0 ][ i ].y;
            }
            if( has_tex_coords1 )
            {
                v.TexCoord2[ 0 ] = aimesh->mTextureCoords[ 1 ][ i ].x;
                v.TexCoord2[ 1 ] = aimesh->mTextureCoords[ 1 ][ i ].y;
            }
            if( has_tex_coords2 )
            {
                v.TexCoord3[ 0 ] = aimesh->mTextureCoords[ 2 ][ i ].x;
                v.TexCoord3[ 1 ] = aimesh->mTextureCoords[ 2 ][ i ].y;
            }
            v.BlendIndices[ 0 ] = -1.0f;
            v.BlendIndices[ 1 ] = -1.0f;
            v.BlendIndices[ 2 ] = -1.0f;
            v.BlendIndices[ 3 ] = -1.0f;
        }

        // Faces
        ms.FacesCount = aimesh->mNumFaces;
        ms.Indicies.resize( ms.FacesCount * 3 );
        for( uint i = 0; i < aimesh->mNumFaces; i++ )
        {
            aiFace& face = aimesh->mFaces[ i ];
            ms.Indicies[ i * 3 + 0 ] = face.mIndices[ 0 ];
            ms.Indicies[ i * 3 + 1 ] = face.mIndices[ 1 ];
            ms.Indicies[ i * 3 + 2 ] = face.mIndices[ 2 ];
        }

        // Material
        aiMaterial* material = scene->mMaterials[ aimesh->mMaterialIndex ];
        aiString    path;
        if( Ptr_aiGetMaterialTextureCount( material, aiTextureType_DIFFUSE ) )
        {
            Ptr_aiGetMaterialTexture( material, aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL, NULL );
            ms.DiffuseTexture = path.data;
        }
        Ptr_aiGetMaterialFloatArray( material, AI_MATKEY_COLOR_DIFFUSE, ms.DiffuseColor, NULL );
        Ptr_aiGetMaterialFloatArray( material, AI_MATKEY_COLOR_AMBIENT, ms.AmbientColor, NULL );
        Ptr_aiGetMaterialFloatArray( material, AI_MATKEY_COLOR_SPECULAR, ms.SpecularColor, NULL );
        Ptr_aiGetMaterialFloatArray( material, AI_MATKEY_COLOR_EMISSIVE, ms.EmissiveColor, NULL );
        ms.DiffuseColor[ 3 ] = 1.0f;
        ms.AmbientColor[ 3 ] = 1.0f;
        ms.SpecularColor[ 3 ] = 1.0f;
        ms.EmissiveColor[ 3 ] = 1.0f;

        // Effect
        ms.DrawEffect.EffectFilename = NULL;

        // Transformed vertices position
        ms.VerticesTransformed = ms.Vertices;
        ms.VerticesTransformedValid = false;
    }

    for( uint i = 0; i < node->mNumChildren; i++ )
        frame->Children[ i ] = FillNode( scene, node->mChildren[ i ] );
    return frame;
}

void GraphicLoader::FixFrame( Frame* root_frame, Frame* frame, aiScene* scene, aiNode* node )
{
    for( uint m = 0; m < node->mNumMeshes; m++ )
    {
        aiMesh*     aimesh = scene->mMeshes[ node->mMeshes[ m ] ];
        MeshSubset& mesh = frame->Mesh[ m ];

        // Bones
        mesh.BoneInfluences = 0;
        mesh.BoneOffsets.resize( aimesh->mNumBones );
        mesh.FrameCombinedMatrixPointer.resize( aimesh->mNumBones );
        for( uint i = 0; i < aimesh->mNumBones; i++ )
        {
            aiBone* bone = aimesh->mBones[ i ];

            // Matrices
            mesh.BoneOffsets[ i ] = bone->mOffsetMatrix;
            Frame* bone_frame = root_frame->Find( bone->mName.data );
            if( bone_frame )
                mesh.FrameCombinedMatrixPointer[ i ] = &bone_frame->CombinedTransformationMatrix;
            else
                mesh.FrameCombinedMatrixPointer[ i ] = NULL;

            // Blend data
            for( uint j = 0; j < bone->mNumWeights; j++ )
            {
                aiVertexWeight& vw = bone->mWeights[ j ];
                Vertex3D&       v = mesh.Vertices[ vw.mVertexId ];
                uint            index;
                if( v.BlendIndices[ 0 ] < 0.0f )
                    index = 0;
                else if( v.BlendIndices[ 1 ] < 0.0f )
                    index = 1;
                else if( v.BlendIndices[ 2 ] < 0.0f )
                    index = 2;
                else
                    index = 3;
                v.BlendWeights[ index ] = vw.mWeight;
                v.BlendIndices[ index ] = (float) i;
                if( mesh.BoneInfluences <= index )
                    mesh.BoneInfluences = index + 1;
            }
        }

        // Drop not filled indices
        for( uint i = 0; i < aimesh->mNumVertices; i++ )
        {
            Vertex3D& v = mesh.Vertices[ i ];
            if( v.BlendIndices[ 0 ] < 0.0f )
                v.BlendIndices[ 0 ] = 0.0f;
            if( v.BlendIndices[ 1 ] < 0.0f )
                v.BlendIndices[ 1 ] = 0.0f;
            if( v.BlendIndices[ 2 ] < 0.0f )
                v.BlendIndices[ 2 ] = 0.0f;
            if( v.BlendIndices[ 3 ] < 0.0f )
                v.BlendIndices[ 3 ] = 0.0f;
        }

        // OGL buffers
        GL( glGenBuffers( 1, &mesh.VBO ) );
        GL( glBindBuffer( GL_ARRAY_BUFFER, mesh.VBO ) );
        GL( glBufferData( GL_ARRAY_BUFFER, mesh.Vertices.size() * sizeof( Vertex3D ), &mesh.Vertices[ 0 ], GL_DYNAMIC_DRAW ) );
        GL( glGenBuffers( 1, &mesh.IBO ) );
        GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mesh.IBO ) );
        GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, mesh.Indicies.size() * sizeof( short ), &mesh.Indicies[ 0 ], GL_DYNAMIC_DRAW ) );
        GL( glGenVertexArrays( 1, &mesh.VAO ) );
        GL( glBindVertexArray( mesh.VAO ) );
        GL( glVertexAttribPointer( 0, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) OFFSETOF( Vertex3D, Position ) ) );
        GL( glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) OFFSETOF( Vertex3D, Normal ) ) );
        GL( glVertexAttribPointer( 2, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) OFFSETOF( Vertex3D, Color ) ) );
        GL( glVertexAttribPointer( 3, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) OFFSETOF( Vertex3D, TexCoord ) ) );
        GL( glVertexAttribPointer( 4, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) OFFSETOF( Vertex3D, TexCoord2 ) ) );
        GL( glVertexAttribPointer( 5, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) OFFSETOF( Vertex3D, TexCoord3 ) ) );
        GL( glVertexAttribPointer( 6, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) OFFSETOF( Vertex3D, Tangent ) ) );
        GL( glVertexAttribPointer( 7, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) OFFSETOF( Vertex3D, Bitangent ) ) );
        GL( glVertexAttribPointer( 8, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) OFFSETOF( Vertex3D, BlendWeights ) ) );
        GL( glVertexAttribPointer( 9, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) OFFSETOF( Vertex3D, BlendIndices ) ) );
        for( uint i = 0; i <= 9; i++ )
            GL( glEnableVertexAttribArray( i ) );
    }

    for( uint i = 0; i < node->mNumChildren; i++ )
        FixFrame( root_frame, frame->Children[ i ], scene, node->mChildren[ i ] );
}
#endif

AnimSet* GraphicLoader::LoadAnimation( Device_ device, const char* anim_fname, const char* anim_name )
{
    // Find in already loaded
    for( uint i = 0, j = (uint) loadedAnimations.size(); i < j; i++ )
    {
        AnimSet* anim = (AnimSet*) loadedAnimations[ i ];
        if( Str::CompareCase( loadedAnimationsFNames[ i ], anim_fname ) &&
            Str::CompareCase( anim->GetName(), anim_name ) )
            return anim;
    }

    // Check maybe file already processed and nothing founded
    for( uint i = 0, j = (uint) processedFiles.size(); i < j; i++ )
        if( Str::CompareCase( processedFiles[ i ], anim_fname ) )
            return NULL;

    // File not processed, load and recheck animations
    if( LoadModel( device, anim_fname ) )
        return LoadAnimation( device, anim_fname, anim_name );

    return NULL;
}

void GraphicLoader::Free( Frame* frame )
{
    #ifdef FO_D3D
    // Free frame
    if( frame )
    {
        SAFEDELA( frame->Name );
        MeshContainer* mesh_container = frame->DrawMesh;
        if( mesh_container )
        {
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
            SAFEREL( mesh_container->InitMesh );
            // Release the main mesh
            SAFEREL( mesh_container->SkinMesh );
            // Release blend mesh
            SAFEREL( mesh_container->SkinMeshBlended );
            // Release skin information
            SAFEREL( mesh_container->Skin );
            // Finally delete the mesh container itself
            delete mesh_container;
        }
        if( frame->Sibling )
            Free( frame->Sibling );
        if( frame->FirstChild )
            Free( frame->FirstChild );
        delete frame;
    }
    #else
    for( auto it = frame->Mesh.begin(), end = frame->Mesh.end(); it != end; ++it )
    {
        MeshSubset& ms = *it;
        GL( glDeleteVertexArrays( 1, &ms.VAO ) );
        GL( glDeleteBuffers( 1, &ms.VBO ) );
        GL( glDeleteBuffers( 1, &ms.IBO ) );
    }
    for( auto it = frame->Children.begin(), end = frame->Children.end(); it != end; ++it )
        Free( *it );
    #endif
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

    #ifdef FO_D3D
    // Create texture
    LPDIRECT3DTEXTURE9 dxtex = NULL;
    if( !fm.IsLoaded() || FAILED( D3DXCreateTextureFromFileInMemory( device, fm.GetBuf(), fm.GetFsize(), &dxtex ) ) )
        return NULL;
    Texture* texture = new Texture();
    texture->Instance = dxtex;
    #else
    // Detect type
    ILenum file_type = ilTypeFromExt( texture_name );
    if( file_type == IL_TYPE_UNKNOWN )
        return NULL;

    // Load image from memory
    ILuint img = 0;
    ilGenImages( 1, &img );
    ilBindImage( img );
    if( !ilLoadL( file_type, fm.GetBuf(), fm.GetFsize() ) )
    {
        ilDeleteImage( img );
        return NULL;
    }

    // Get image data
    uint w = ilGetInteger( IL_IMAGE_WIDTH );
    uint h = ilGetInteger( IL_IMAGE_HEIGHT );
    int  format = ilGetInteger( IL_IMAGE_FORMAT );
    int  type = ilGetInteger( IL_IMAGE_TYPE );

    // Convert data
    if( format != IL_BGRA || type != IL_UNSIGNED_BYTE )
    {
        if( !ilConvertImage( IL_BGRA, IL_UNSIGNED_BYTE ) )
        {
            ilDeleteImage( img );
            return NULL;
        }
    }

    // Copy data
    uint   size = ilGetInteger( IL_IMAGE_SIZE_OF_DATA );
    uchar* data = new uchar[ size ];
    memcpy( data, ilGetData(), size );

    // Delete image
    ilDeleteImage( img );

    // Create texture
    Texture* texture = new Texture();
    texture->Data = data;
    texture->Size = size;
    texture->Width = w;
    texture->Height = h;
    texture->SizeData[ 0 ] = (float) w;
    texture->SizeData[ 1 ] = (float) h;
    texture->SizeData[ 2 ] = 1.0f / texture->SizeData[ 0 ];
    texture->SizeData[ 3 ] = 1.0f / texture->SizeData[ 1 ];
    GL( glGenTextures( 1, &texture->Id ) );
    GL( glBindTexture( GL_TEXTURE_2D, texture->Id ) );
    GL( glPixelStorei( GL_UNPACK_ALIGNMENT, 1 ) );
    GL( glTexImage2D( GL_TEXTURE_2D, 0, 4, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, texture->Data ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT ) );
    #endif
    texture->Name = Str::Duplicate( texture_name );
    loadedTextures.push_back( texture );
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

Effect* GraphicLoader::LoadEffect( Device_ device, const char* effect_name, bool use_in_2d, const char* defines /* = NULL */ )
{
    EffectInstance effect_inst;
    memzero( &effect_inst, sizeof( effect_inst ) );
    effect_inst.EffectFilename = (char*) effect_name;
    return LoadEffect( device, &effect_inst, NULL, use_in_2d, defines );
}

Effect* GraphicLoader::LoadEffect( Device_ device, EffectInstance* effect_inst, const char* model_path, bool use_in_2d, const char* defines /* = NULL */ )
{
    if( !effect_inst || !effect_inst->EffectFilename || !effect_inst->EffectFilename[ 0 ] )
        return NULL;
    const char* effect_name = effect_inst->EffectFilename;

    // Try find already loaded texture
    for( auto it = loadedEffects.begin(), end = loadedEffects.end(); it != end; ++it )
    {
        Effect* effect = *it;
        if( Str::CompareCase( effect->Name, effect_name ) && effect->Defaults == effect_inst->Defaults &&
            Str::Compare( effect->Defines, defines ? defines : "" ) )
        {
            return effect;
        }
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
    Str::Format( cache_name, "%s.fxc", effect_name );
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
    effect->Defines = Str::Duplicate( "" );
    effect->DXInstance = dxeffect;
    effect->EffectFlags = D3DXFX_DONOTSAVESTATE;
    effect->Defaults = NULL;
    effect->EffectParams = NULL;

    effect->TechniqueSkinned = dxeffect->GetTechniqueByName( "Skinned" );
    effect->TechniqueSkinnedWithShadow = dxeffect->GetTechniqueByName( "SkinnedWithShadow" );
    effect->TechniqueSimple = dxeffect->GetTechniqueByName( "Simple" );
    effect->TechniqueSimpleWithShadow = dxeffect->GetTechniqueByName( "SimpleWithShadow" );
    effect->BoneInfluences = dxeffect->GetParameterByName( NULL, "BonesInfluences" );
    effect->GroundPosition = dxeffect->GetParameterByName( NULL, "GroundPosition" );
    effect->LightDir = dxeffect->GetParameterByName( NULL, "LightDir" );
    effect->LightDiffuse = dxeffect->GetParameterByName( NULL, "LightDiffuse" );
    effect->MaterialAmbient = dxeffect->GetParameterByName( NULL, "MaterialAmbient" );
    effect->MaterialDiffuse = dxeffect->GetParameterByName( NULL, "MaterialDiffuse" );
    effect->WorldMatrices = dxeffect->GetParameterByName( NULL, "WorldMatrices" );
    effect->ProjectionMatrix = dxeffect->GetParameterByName( NULL, "ViewProjMatrix" );

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
        Str::Format( tex_name, "Texture%d", i );
        effect->Textures[ i ] = dxeffect->GetParameterByName( NULL, tex_name );
        if( effect->Textures[ i ] )
            effect->IsTextures = true;
    }
    effect->IsScriptValues = false;
    for( int i = 0; i < EFFECT_SCRIPT_VALUES; i++ )
    {
        char val_name[ 32 ];
        Str::Format( val_name, "EffectValue%d", i );
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
                dxeffect->SetFloatArray( param, (float*) def.Data, def.Size / sizeof( float ) );
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
    #else
    // Make name
    char fname[ MAX_FOPATH ];
    Str::Copy( fname, effect_name );
    FileManager::EraseExtension( fname );
    Str::Append( fname, ".glsl" );

    // Shader program
    GLuint program = 0;

    // Load text file
    FileManager file;
    if( !file.LoadFile( fname, PT_EFFECTS ) && model_path )
    {
        // Try load from model folder
        char path[ MAX_FOPATH ];
        FileManager::ExtractPath( model_path, path );
        Str::Append( path, fname );
        file.LoadFile( path, PT_DATA );
    }
    if( !file.IsLoaded() )
    {
        WriteLogF( _FUNC_, " - Effect file<%s> not found.\n", fname );
        return NULL;
    }

    // Load from binary
    bool        have_binary = ( GLEW_ARB_get_program_binary != 0 );
    FileManager file_binary;
    if( have_binary )
    {
        Str::Append( fname, "b" );
        if( file_binary.LoadFile( fname, PT_CACHE ) )
        {
            uint64 last_write, last_write_binary;
            file.GetTime( NULL, NULL, &last_write );
            file_binary.GetTime( NULL, NULL, &last_write_binary );
            if( last_write > last_write_binary )
                file_binary.UnloadFile();      // Disable loading from this binary, because its outdated
        }
        fname[ Str::Length( fname ) - 1 ] = 0; // Erase 'b'
    }
    if( file_binary.IsLoaded() )
    {
        bool loaded = false;
        uint version = file_binary.GetBEUInt();
        if( version == SHADER_PROGRAM_BINARY_VERSION )
        {
            GLenum  format = file_binary.GetBEUInt();
            GLsizei length = file_binary.GetBEUInt();
            if( file_binary.GetFsize() >= length + sizeof( uint ) * 3 )
            {
                GL( program = glCreateProgram() );
                GL( glProgramBinary( program, format, file_binary.GetCurBuf(), length ) );
                GLint linked;
                GL( glGetProgramiv( program, GL_LINK_STATUS, &linked ) );
                if( linked )
                {
                    loaded = true;
                }
                else
                {
                    WriteLogF( _FUNC_, " - Failed to link binary shader program, effect<%s>.\n", fname );
                    GL( glDeleteProgram( program ) );
                }
            }
            else
            {
                WriteLogF( _FUNC_, " - Binary shader program truncated, effect<%s>.\n", fname );
            }
        }
        if( !loaded )
            file_binary.UnloadFile();
    }

    // Load from text
    if( !file_binary.IsLoaded() )
    {
        char* str = (char*) file.GetBuf();

        // Get version
        char* ver = Str::Substring( str, "#version" );
        if( ver )
        {
            char* ver_end = Str::Substring( ver, "\n" );
            if( ver_end )
            {
                str = ver_end + 1;
                *ver_end = 0;
            }
        }

        // Create shaders
        GLuint vs, fs;
        GL( vs = glCreateShader( GL_VERTEX_SHADER ) );
        GL( fs = glCreateShader( GL_FRAGMENT_SHADER ) );
        const char* vs_str[] = { ver ? ver : "", "\n", "#define VERTEX_SHADER\n", defines ? defines : "", "\n", str };
        GL( glShaderSource( vs, 6, (const GLchar**) vs_str, NULL ) );
        const char* fs_str[] = { ver ? ver : "", "\n", "#define FRAGMENT_SHADER\n", defines ? defines : "", "\n", str };
        GL( glShaderSource( fs, 6, (const GLchar**) fs_str, NULL ) );

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
                    WriteLog( "%s output:\n%s", shader_name, str );
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
            WriteLogF( _FUNC_, " - Vertex shader not compiled, effect<%s>.\n", fname );
            ShaderInfo::Log( "Vertex shader", vs );
            GL( glDeleteShader( vs ) );
            GL( glDeleteShader( fs ) );
            return NULL;
        }

        // Compile fs
        GL( glCompileShader( fs ) );
        GL( glGetShaderiv( fs, GL_COMPILE_STATUS, &compiled ) );
        if( !compiled )
        {
            WriteLogF( _FUNC_, " - Fragment shader not compiled, effect<%s>.\n", fname );
            ShaderInfo::Log( "Fragment shader", fs );
            GL( glDeleteShader( vs ) );
            GL( glDeleteShader( fs ) );
            return NULL;
        }

        // Make program
        GL( program = glCreateProgram() );
        GL( glAttachShader( program, vs ) );
        GL( glAttachShader( program, fs ) );

        if( use_in_2d )
        {
            GL( glBindAttribLocation( program, 0, "InPosition" ) );
            GL( glBindAttribLocation( program, 1, "InColor" ) );
            GL( glBindAttribLocation( program, 2, "InTexCoord" ) );
            GL( glBindAttribLocation( program, 3, "InTexEggCoord" ) );
        }
        else
        {
            GL( glBindAttribLocation( program, 0, "InPosition" ) );
            GL( glBindAttribLocation( program, 1, "InNormal" ) );
            GL( glBindAttribLocation( program, 2, "InColor" ) );
            GL( glBindAttribLocation( program, 3, "InTexCoord" ) );
            GL( glBindAttribLocation( program, 4, "InTexCoord2" ) );
            GL( glBindAttribLocation( program, 5, "InTexCoord3" ) );
            GL( glBindAttribLocation( program, 6, "InTangent" ) );
            GL( glBindAttribLocation( program, 7, "InBitangent" ) );
            GL( glBindAttribLocation( program, 8, "InBlendWeights" ) );
            GL( glBindAttribLocation( program, 9, "InBlendIndices" ) );
        }

        GL( glLinkProgram( program ) );
        GLint linked;
        GL( glGetProgramiv( program, GL_LINK_STATUS, &linked ) );
        if( !linked )
        {
            WriteLogF( _FUNC_, " - Failed to link shader program, effect<%s>.\n", fname );
            ShaderInfo::LogProgram( program );
            GL( glDetachShader( program, vs ) );
            GL( glDetachShader( program, fs ) );
            GL( glDeleteShader( vs ) );
            GL( glDeleteShader( fs ) );
            GL( glDeleteProgram( program ) );
            return NULL;
        }

        // Save in binary
        if( have_binary )
        {
            GLsizei  buf_size;
            GL( glGetProgramiv( program, GL_PROGRAM_BINARY_LENGTH, &buf_size ) );
            GLsizei  length;
            GLenum   format;
            UCharVec buf;
            buf.resize( buf_size );
            GL( glGetProgramBinary( program, buf_size, &length, &format, &buf[ 0 ] ) );
            Str::Append( fname, "b" );
            file_binary.SetBEUInt( SHADER_PROGRAM_BINARY_VERSION );
            file_binary.SetBEUInt( format );
            file_binary.SetBEUInt( length );
            file_binary.SetData( &buf[ 0 ], length );
            if( !file_binary.SaveOutBufToFile( fname, PT_CACHE ) )
                WriteLogF( _FUNC_, " - Can't save effect<%s> in binary.\n", fname );
        }
    }

    // Create effect instance
    Effect* effect = new Effect();
    memzero( effect, sizeof( Effect ) );
    effect->Name = Str::Duplicate( effect_name );
    effect->Defines = Str::Duplicate( defines ? defines : "" );
    effect->Program = program;
    effect->Defaults = NULL;
    effect->Passes = 1;

    // Get data
    GLint passes;
    GL( passes = effect->SpriteBorder = glGetUniformLocation( program, "Passes" ) );
    if( passes != -1 )
        glGetUniformiv( program, passes, (GLint*) &effect->Passes );

    // Bind data
    GL( effect->ProjectionMatrix = glGetUniformLocation( program, "ProjectionMatrix" ) );
    GL( effect->ZoomFactor = glGetUniformLocation( program, "ZoomFactor" ) );
    GL( effect->ColorMap = glGetUniformLocation( program, "ColorMap" ) );
    GL( effect->ColorMapSize = glGetUniformLocation( program, "ColorMapSize" ) );
    GL( effect->ColorMapSamples = glGetUniformLocation( program, "ColorMapSamples" ) );
    GL( effect->EggMap = glGetUniformLocation( program, "EggMap" ) );
    GL( effect->EggMapSize = glGetUniformLocation( program, "EggMapSize" ) );
    # ifdef SHADOW_MAP
    GL( effect->ShadowMap = glGetUniformLocation( program, "ShadowMap" ) );
    GL( effect->ShadowMapSize = glGetUniformLocation( program, "ShadowMapSize" ) );
    GL( effect->ShadowMapSamples = glGetUniformLocation( program, "ShadowMapSamples" ) );
    GL( effect->ShadowMapMatrix = glGetUniformLocation( program, "ShadowMapMatrix" ) );
    # endif
    GL( effect->SpriteBorder = glGetUniformLocation( program, "SpriteBorder" ) );

    GL( effect->BoneInfluences = glGetUniformLocation( program, "BoneInfluences" ) );
    GL( effect->GroundPosition = glGetUniformLocation( program, "GroundPosition" ) );
    GL( effect->MaterialAmbient = glGetUniformLocation( program, "MaterialAmbient" ) );
    GL( effect->MaterialDiffuse = glGetUniformLocation( program, "MaterialDiffuse" ) );
    GL( effect->WorldMatrices = glGetUniformLocation( program, "WorldMatrices" ) );
    GL( effect->WorldMatrix = glGetUniformLocation( program, "WorldMatrix" ) );

    GL( effect->PassIndex = glGetUniformLocation( program, "PassIndex" ) );
    GL( effect->Time = glGetUniformLocation( program, "Time" ) );
    effect->TimeCurrent = 0.0f;
    effect->TimeLastTick = Timer::AccurateTick();
    GL( effect->TimeGame = glGetUniformLocation( program, "TimeGame" ) );
    effect->TimeGameCurrent = 0.0f;
    effect->TimeGameLastTick = Timer::AccurateTick();
    effect->IsTime = ( effect->Time != -1 || effect->TimeGame != -1 );
    GL( effect->Random1Pass = glGetUniformLocation( program, "Random1Pass" ) );
    GL( effect->Random2Pass = glGetUniformLocation( program, "Random2Pass" ) );
    GL( effect->Random3Pass = glGetUniformLocation( program, "Random3Pass" ) );
    GL( effect->Random4Pass = glGetUniformLocation( program, "Random4Pass" ) );
    effect->IsRandomPass = ( effect->Random1Pass != -1 || effect->Random2Pass != -1 || effect->Random3Pass != -1 || effect->Random4Pass != -1 );
    GL( effect->Random1Effect = glGetUniformLocation( program, "Random1Effect" ) );
    GL( effect->Random2Effect = glGetUniformLocation( program, "Random2Effect" ) );
    GL( effect->Random3Effect = glGetUniformLocation( program, "Random3Effect" ) );
    GL( effect->Random4Effect = glGetUniformLocation( program, "Random4Effect" ) );
    effect->IsRandomEffect = ( effect->Random1Effect != -1 || effect->Random2Effect != -1 || effect->Random3Effect != -1 || effect->Random4Effect != -1 );
    effect->IsTextures = false;
    for( int i = 0; i < EFFECT_TEXTURES; i++ )
    {
        char tex_name[ 32 ];
        Str::Format( tex_name, "Texture%d", i );
        GL( effect->Textures[ i ] = glGetUniformLocation( program, tex_name ) );
        if( effect->Textures[ i ] != -1 )
        {
            effect->IsTextures = true;
            Str::Format( tex_name, "Texture%dSize", i );
            GL( effect->TexturesSize[ i ] = glGetUniformLocation( program, tex_name ) );
        }
    }
    effect->IsScriptValues = false;
    for( int i = 0; i < EFFECT_SCRIPT_VALUES; i++ )
    {
        char val_name[ 32 ];
        Str::Format( val_name, "EffectValue%d", i );
        GL( effect->ScriptValues[ i ] = glGetUniformLocation( program, val_name ) );
        if( effect->ScriptValues[ i ] != -1 )
            effect->IsScriptValues = true;
    }
    GL( effect->AnimPosProc = glGetUniformLocation( program, "AnimPosProc" ) );
    GL( effect->AnimPosTime = glGetUniformLocation( program, "AnimPosTime" ) );
    effect->IsAnimPos = ( effect->AnimPosProc != -1 || effect->AnimPosTime != -1 );
    effect->IsNeedProcess = ( effect->PassIndex != -1 || effect->IsTime || effect->IsRandomPass || effect->IsRandomEffect ||
                              effect->IsTextures || effect->IsScriptValues || effect->IsAnimPos );
    #endif

    loadedEffects.push_back( effect );
    return loadedEffects.back();
}

void GraphicLoader::EffectProcessVariables( Effect* effect, int pass,  float anim_proc /* = 0.0f */, float anim_time /* = 0.0f */, Texture** textures /* = NULL */ )
{
    // Process effect
    if( pass == -1 )
    {
        if( effect->IsTime )
        {
            double tick = Timer::AccurateTick();
            if( IS_EFFECT_VALUE( effect->Time ) )
            {
                effect->TimeCurrent += (float) ( tick - effect->TimeLastTick );
                effect->TimeLastTick = tick;
                if( effect->TimeCurrent >= 120.0f )
                    effect->TimeCurrent = fmod( effect->TimeCurrent, 120.0f );

                SET_EFFECT_VALUE( effect, effect->Time, effect->TimeCurrent );
            }
            if( IS_EFFECT_VALUE( effect->TimeGame ) )
            {
                if( !Timer::IsGamePaused() )
                {
                    effect->TimeGameCurrent += (float) ( tick - effect->TimeGameLastTick ) / 1000.0f;
                    effect->TimeGameLastTick = tick;
                    if( effect->TimeGameCurrent >= 120.0f )
                        effect->TimeGameCurrent = fmod( effect->TimeGameCurrent, 120.0f );
                }
                else
                {
                    effect->TimeGameLastTick = tick;
                }

                SET_EFFECT_VALUE( effect, effect->TimeGame, effect->TimeGameCurrent );
            }
        }

        if( effect->IsRandomEffect )
        {
            if( IS_EFFECT_VALUE( effect->Random1Effect ) )
                SET_EFFECT_VALUE( effect, effect->Random1Effect, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( IS_EFFECT_VALUE( effect->Random2Effect ) )
                SET_EFFECT_VALUE( effect, effect->Random2Effect, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( IS_EFFECT_VALUE( effect->Random3Effect ) )
                SET_EFFECT_VALUE( effect, effect->Random3Effect, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( IS_EFFECT_VALUE( effect->Random4Effect ) )
                SET_EFFECT_VALUE( effect, effect->Random4Effect, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
        }

        if( effect->IsTextures )
        {
            for( int i = 0; i < EFFECT_TEXTURES; i++ )
            {
                if( IS_EFFECT_VALUE( effect->Textures[ i ] ) )
                {
                    #ifdef FO_D3D
                    effect->DXInstance->SetTexture( effect->Textures[ i ], textures && textures[ i ] ? textures[ i ]->Instance : NULL );
                    #else
                    GLuint id = ( textures && textures[ i ] ? textures[ i ]->Id : 0 );
                    GL( glActiveTexture( GL_TEXTURE2 + i ) );
                    GL( glBindTexture( GL_TEXTURE_2D, id ) );
                    GL( glUniform1i( effect->Textures[ i ], 2 + i ) );
                    if( effect->TexturesSize[ i ] != -1 && textures && textures[ i ] )
                        GL( glUniform4fv( effect->TexturesSize[ i ], 1, textures[ i ]->SizeData ) );
                    #endif
                }
            }
        }

        if( effect->IsScriptValues )
        {
            for( int i = 0; i < EFFECT_SCRIPT_VALUES; i++ )
                if( IS_EFFECT_VALUE( effect->ScriptValues[ i ] ) )
                    SET_EFFECT_VALUE( effect, effect->ScriptValues[ i ], GameOpt.EffectValues[ i ] );
        }

        if( effect->IsAnimPos )
        {
            if( IS_EFFECT_VALUE( effect->AnimPosProc ) )
                SET_EFFECT_VALUE( effect, effect->AnimPosProc, anim_proc );
            if( IS_EFFECT_VALUE( effect->AnimPosTime ) )
                SET_EFFECT_VALUE( effect, effect->AnimPosTime, anim_time );
        }
    }
    // Process pass
    else
    {
        if( IS_EFFECT_VALUE( effect->PassIndex ) )
            SET_EFFECT_VALUE( effect, effect->Random1Pass, (float) pass );

        if( effect->IsRandomPass )
        {
            if( IS_EFFECT_VALUE( effect->Random1Pass ) )
                SET_EFFECT_VALUE( effect, effect->Random1Pass, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( IS_EFFECT_VALUE( effect->Random2Pass ) )
                SET_EFFECT_VALUE( effect, effect->Random2Pass, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( IS_EFFECT_VALUE( effect->Random3Pass ) )
                SET_EFFECT_VALUE( effect, effect->Random3Pass, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
            if( IS_EFFECT_VALUE( effect->Random4Pass ) )
                SET_EFFECT_VALUE( effect, effect->Random4Pass, (float) ( (double) Random( 0, 2000000000 ) / 2000000000.0 ) );
        }
    }
}

bool GraphicLoader::EffectsPreRestore()
{
    #ifdef FO_D3D
    for( auto it = loadedEffects.begin(), end = loadedEffects.end(); it != end; ++it )
    {
        Effect* effect = *it;
        D3D_HR( effect->DXInstance->OnLostDevice() );
    }
    #endif
    return true;
}

bool GraphicLoader::EffectsPostRestore()
{
    #ifdef FO_D3D
    for( auto it = loadedEffects.begin(), end = loadedEffects.end(); it != end; ++it )
    {
        Effect* effect = *it;
        D3D_HR( effect->DXInstance->OnResetDevice() );
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
