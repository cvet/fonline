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

Frame* GraphicLoader::LoadModel( const char* fname )
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
        #else
        # define ASSIMP_PATH        "./"
        # define ASSIMP_LIB_NAME    "Assimp32.so"
        #endif

        // Check dll availability
        void* dll = DLL_Load( ASSIMP_PATH ASSIMP_LIB_NAME );
        if( !dll )
        {
            if( GameOpt.ClientPath.c_std_str() != "" )
                dll = DLL_Load( ( GameOpt.ClientPath.c_std_str() + ASSIMP_LIB_NAME ).c_str() );
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
        if( GameOpt.AssimpLogging )
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
                                                            aiProcess_ImproveCacheLocality, "" );
    if( !scene )
    {
        WriteLogF( _FUNC_, " - Can't load 3d file, name<%s>, error<%s>.\n", fname, Ptr_aiGetErrorString() );
        return NULL;
    }

    // Extract frames
    Frame* frame_root = FillNode( scene, scene->mRootNode );
    FixFrame( frame_root, frame_root, scene, scene->mRootNode );
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
        bool has_tangents_and_bitangents = aimesh->HasTangentsAndBitangents();
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
            if( has_tangents_and_bitangents )
            {
                v.Tangent = aimesh->mTangents[ i ];
                v.Bitangent = aimesh->mBitangents[ i ];
            }
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
        GL( glBufferData( GL_ARRAY_BUFFER, mesh.Vertices.size() * sizeof( Vertex3D ), &mesh.Vertices[ 0 ], GL_STATIC_DRAW ) );
        GL( glGenBuffers( 1, &mesh.IBO ) );
        GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mesh.IBO ) );
        GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, mesh.Indicies.size() * sizeof( short ), &mesh.Indicies[ 0 ], GL_STATIC_DRAW ) );
        mesh.VAO = 0;
        if( GLEW_ARB_vertex_array_object && ( GLEW_ARB_framebuffer_object || GLEW_EXT_framebuffer_object ) )
        {
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
            GL( glBindVertexArray( 0 ) );
        }
    }

    for( uint i = 0; i < node->mNumChildren; i++ )
        FixFrame( root_frame, frame->Children[ i ], scene, node->mChildren[ i ] );
}

AnimSet* GraphicLoader::LoadAnimation( const char* anim_fname, const char* anim_name )
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
    if( LoadModel( anim_fname ) )
        return LoadAnimation( anim_fname, anim_name );

    return NULL;
}

void GraphicLoader::Free( Frame* frame )
{
    for( auto it = frame->Mesh.begin(), end = frame->Mesh.end(); it != end; ++it )
    {
        MeshSubset& ms = *it;
        if( ms.VAO )
            GL( glDeleteVertexArrays( 1, &ms.VAO ) );
        GL( glDeleteBuffers( 1, &ms.VBO ) );
        GL( glDeleteBuffers( 1, &ms.IBO ) );
    }
    for( auto it = frame->Children.begin(), end = frame->Children.end(); it != end; ++it )
        Free( *it );
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

Texture* GraphicLoader::LoadTexture( const char* texture_name, const char* model_path )
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
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT ) );
    GL( glTexImage2D( GL_TEXTURE_2D, 0, 4, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, texture->Data ) );
    texture->Name = Str::Duplicate( texture_name );
    loadedTextures.push_back( texture );
    return loadedTextures.back();
}

void GraphicLoader::FreeTexture( Texture* texture )
{
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
        SAFEDELA( texture->Data );
        SAFEDEL( texture );
    }
    else
    {
        TextureVec textures = loadedTextures;
        for( auto it = textures.begin(), end = textures.end(); it != end; ++it )
            FreeTexture( *it );
    }
}

/************************************************************************/
/* Effects                                                              */
/************************************************************************/

EffectVec GraphicLoader::loadedEffects;

Effect* GraphicLoader::LoadEffect( const char* effect_name, bool use_in_2d, const char* defines /* = NULL */, const char* model_path /* = NULL */, EffectDefault* defaults /* = NULL */, uint defaults_count /* = 0 */ )
{
    // Erase extension
    char fname[ MAX_FOPATH ];
    Str::Copy( fname, effect_name );
    FileManager::EraseExtension( fname );

    // Reset defaults to NULL if it's count is zero
    if( defaults_count == 0 )
        defaults = NULL;

    // Try find already loaded effect
    char loaded_fname[ MAX_FOPATH ];
    Str::Copy( loaded_fname, fname );
    for( auto it = loadedEffects.begin(), end = loadedEffects.end(); it != end; ++it )
    {
        Effect* effect = *it;
        if( Str::CompareCase( effect->Name, loaded_fname ) && Str::Compare( effect->Defines, defines ? defines : "" ) && effect->Defaults == defaults )
            return effect;
    }

    // Shader program
    GLuint program = 0;

    // Add extension
    Str::Append( fname, ".glsl" );

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

    // Make effect binary file name
    char binary_fname[ MAX_FOPATH ] = { 0 };
    if( GLEW_ARB_get_program_binary )
    {
        Str::Copy( binary_fname, fname );
        FileManager::EraseExtension( binary_fname );
        if( defines )
        {
            char binary_fname_defines[ MAX_FOPATH ];
            Str::Copy( binary_fname_defines, defines );
            Str::Replacement( binary_fname_defines, '\t', ' ' );       // Tabs to spaces
            Str::Replacement( binary_fname_defines, ' ', ' ', ' ' );   // Multiple spaces to single
            Str::Replacement( binary_fname_defines, '\r', '\n', '_' ); // EOL's to '_'
            Str::Replacement( binary_fname_defines, '\r', '_' );       // EOL's to '_'
            Str::Replacement( binary_fname_defines, '\n', '_' );       // EOL's to '_'
            Str::Append( binary_fname, "_" );
            Str::Append( binary_fname, binary_fname_defines );
        }
        Str::Append( binary_fname, ".glslb" );
    }

    // Load from binary
    FileManager file_binary;
    if( GLEW_ARB_get_program_binary )
    {
        if( file_binary.LoadFile( binary_fname, PT_CACHE ) )
        {
            uint64 last_write, last_write_binary;
            file.GetTime( NULL, NULL, &last_write );
            file_binary.GetTime( NULL, NULL, &last_write_binary );
            if( last_write > last_write_binary )
                file_binary.UnloadFile();      // Disable loading from this binary, because its outdated
        }
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
                glProgramBinary( program, format, file_binary.GetCurBuf(), length );
                glGetError(); // Skip error from glProgramBinary, if it has
                GLint linked;
                GL( glGetProgramiv( program, GL_LINK_STATUS, &linked ) );
                if( linked )
                {
                    loaded = true;
                }
                else
                {
                    WriteLogF( _FUNC_, " - Failed to link binary shader program<%s>, effect<%s>.\n", binary_fname, fname );
                    GL( glDeleteProgram( program ) );
                }
            }
            else
            {
                WriteLogF( _FUNC_, " - Binary shader program<%s> truncated, effect<%s>.\n", binary_fname, fname );
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

        if( GLEW_ARB_get_program_binary )
            GL( glProgramParameteri( program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE ) );

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
        if( GLEW_ARB_get_program_binary )
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
            if( !file_binary.SaveOutBufToFile( binary_fname, PT_CACHE ) )
                WriteLogF( _FUNC_, " - Can't save effect<%s> in binary<%s>.\n", fname, binary_fname );
        }
    }

    // Create effect instance
    Effect* effect = new Effect();
    memzero( effect, sizeof( Effect ) );
    effect->Name = Str::Duplicate( loaded_fname );
    effect->Defines = Str::Duplicate( defines ? defines : "" );
    effect->Program = program;
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
    #ifdef SHADOW_MAP
    GL( effect->ShadowMap = glGetUniformLocation( program, "ShadowMap" ) );
    GL( effect->ShadowMapSize = glGetUniformLocation( program, "ShadowMapSize" ) );
    GL( effect->ShadowMapSamples = glGetUniformLocation( program, "ShadowMapSamples" ) );
    GL( effect->ShadowMapMatrix = glGetUniformLocation( program, "ShadowMapMatrix" ) );
    #endif
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

    // Defaults
    effect->Defaults = NULL;
    if( defaults )
    {
        bool something_binded = false;
        for( uint d = 0; d < defaults_count; d++ )
        {
            EffectDefault& def = defaults[ d ];

            GLint          location = -1;
            GL( location = glGetUniformLocation( program, def.Name ) );
            if( location == -1 )
                continue;

            switch( def.Type )
            {
            case EffectDefault::String:
                break;
            case EffectDefault::Floats:
                GL( glUniform1fv( location, def.Size / sizeof( GLfloat ), (GLfloat*) def.Data ) );
                something_binded = true;
                break;
            case EffectDefault::Dword:
                GL( glUniform1i( location, *(GLint*) def.Data ) );
                something_binded = true;
                break;
            default:
                break;
            }
        }
        if( something_binded )
            effect->Defaults = defaults;
    }

    static int effect_id = 0;
    effect->Id = ++effect_id;

    loadedEffects.push_back( effect );
    return effect;
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
                    GLuint id = ( textures && textures[ i ] ? textures[ i ]->Id : 0 );
                    GL( glActiveTexture( GL_TEXTURE2 + i ) );
                    GL( glBindTexture( GL_TEXTURE_2D, id ) );
                    GL( glActiveTexture( GL_TEXTURE0 ) );
                    GL( glUniform1i( effect->Textures[ i ], 2 + i ) );
                    if( effect->TexturesSize[ i ] != -1 && textures && textures[ i ] )
                        GL( glUniform4fv( effect->TexturesSize[ i ], 1, textures[ i ]->SizeData ) );
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

Effect* Effect::Contour, * Effect::ContourDefault;
Effect* Effect::Generic, * Effect::GenericDefault;
Effect* Effect::Critter, * Effect::CritterDefault;
Effect* Effect::Tile, * Effect::TileDefault;
Effect* Effect::Roof, * Effect::RoofDefault;
Effect* Effect::Rain, * Effect::RainDefault;
Effect* Effect::Iface, * Effect::IfaceDefault;
Effect* Effect::Primitive, * Effect::PrimitiveDefault;
Effect* Effect::Light, * Effect::LightDefault;
Effect* Effect::FlushRenderTarget, * Effect::FlushRenderTargetDefault;
Effect* Effect::FlushRenderTargetMS, * Effect::FlushRenderTargetMSDefault;
Effect* Effect::FlushPrimitive, * Effect::FlushPrimitiveDefault;
Effect* Effect::FlushMap, * Effect::FlushMapDefault;
Effect* Effect::Font, * Effect::FontDefault;
Effect* Effect::Simple3d, * Effect::Simple3dDefault;
Effect* Effect::Skinned3d, * Effect::Skinned3dDefault;
Effect* Effect::Simple3dShadow, * Effect::Simple3dShadowDefault;
Effect* Effect::Skinned3dShadow, * Effect::Skinned3dShadowDefault;

bool GraphicLoader::LoadDefaultEffects()
{
    // Default effects
    #define LOAD_EFFECT( effect_handle, effect_name, use_in_2d, defines )         \
        effect_handle ## Default = LoadEffect( effect_name, use_in_2d, defines ); \
        if( effect_handle ## Default )                                            \
            effect_handle = new Effect( *effect_handle ## Default );              \
        else                                                                      \
            effect_errors++
    uint effect_errors = 0;
    LOAD_EFFECT( Effect::Generic, "2D_Default", true, NULL );
    LOAD_EFFECT( Effect::Critter, "2D_Default", true, NULL );
    LOAD_EFFECT( Effect::Roof, "2D_Default", true, NULL );
    LOAD_EFFECT( Effect::Rain, "2D_Default", true, NULL );
    LOAD_EFFECT( Effect::Iface, "Interface_Default", true, NULL );
    LOAD_EFFECT( Effect::Primitive, "Primitive_Default", true, NULL );
    LOAD_EFFECT( Effect::Light, "Primitive_Default", true, NULL );
    LOAD_EFFECT( Effect::Font, "Font_Default", true, NULL );
    LOAD_EFFECT( Effect::Contour, "Contour_Default", true, NULL );
    LOAD_EFFECT( Effect::Tile, "2D_WithoutEgg", true, NULL );
    LOAD_EFFECT( Effect::FlushRenderTarget, "Flush_RenderTarget", true, NULL );
    LOAD_EFFECT( Effect::FlushPrimitive, "Flush_Primitive", true, NULL );
    LOAD_EFFECT( Effect::FlushMap, "Flush_Map", true, NULL );
    LOAD_EFFECT( Effect::Simple3d, "3D_Simple", false, NULL );
    LOAD_EFFECT( Effect::Simple3dShadow, "3D_Simple", false, "#define SHADOW" );
    LOAD_EFFECT( Effect::Skinned3d, "3D_Skinned", false, NULL );
    LOAD_EFFECT( Effect::Skinned3dShadow, "3D_Skinned", false, "#define SHADOW" );
    if( effect_errors > 0 )
    {
        WriteLog( "Default effects not loaded.\n" );
        return false;
    }
    return true;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
