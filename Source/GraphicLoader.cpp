#include "StdAfx.h"
#include "GraphicLoader.h"
#include "3dAnimation.h"
#include "Text.h"
#include "Timer.h"
#include "Version.h"
#include "SpriteManager.h"
#include "ResourceManager.h"

#include "Assimp/assimp.h"
#include "Assimp/aiPostProcess.h"

#include "fbxsdk/fbxsdk.h"
#ifdef FO_WINDOWS
# pragma comment( lib, "libfbxsdk.lib" )
#endif

#include "PNG/png.h"
#ifdef FO_MSVC
# pragma comment( lib, "libpng16.lib" )
#endif

static void CreateBuffers( Node* node );

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

static Node* ConvertAssimpPass1( aiScene* scene, aiNode* ai_node );
static void  ConvertAssimpPass2( Node* root_node, Node* node, aiScene* scene, aiNode* ai_node );

// FBX stuff
class FbxStreamImpl: public FbxStream
{
private:
    FileManager* fm;
    EState       curState;

public:
    FbxStreamImpl(): FbxStream()
    {
        fm = NULL;
        curState = FbxStream::eClosed;
    }

    virtual EState GetState()
    {
        return curState;
    }

    virtual bool Open( void* stream )
    {
        fm = (FileManager*) stream;
        fm->SetCurPos( 0 );
        curState = FbxStream::eOpen;
        return true;
    }

    virtual bool Close()
    {
        fm->SetCurPos( 0 );
        fm = NULL;
        curState = FbxStream::eClosed;
        return true;
    }

    virtual bool Flush()
    {
        return true;
    }

    virtual int Write( const void* data, int size )
    {
        return 0;
    }

    virtual int Read( void* data, int size ) const
    {
        return fm->CopyMem( data, size ) ? size : 0;
    }

    virtual char* ReadString( char* buffer, int max_size, bool stop_at_first_white_space = false )
    {
        const char* str = (char*) fm->GetCurBuf();
        int         len = 0;
        while( *str && len < max_size - 1 )
        {
            str++;
            len++;
            if( *str == '\n' || ( stop_at_first_white_space && *str == ' ' ) )
                break;
        }
        if( len )
            fm->CopyMem( buffer, len );
        buffer[ len ] = 0;
        return buffer;
    }

    virtual int GetReaderID() const
    {
        return 0;
    }

    virtual int GetWriterID() const
    {
        return -1;
    }

    virtual void Seek( const FbxInt64& offset, const FbxFile::ESeekPos& seek_pos )
    {
        if( seek_pos == FbxFile::eBegin )
            fm->SetCurPos( (uint) offset );
        else if( seek_pos == FbxFile::eCurrent )
            fm->GoForward( (uint) offset );
        else if( seek_pos == FbxFile::eEnd )
            fm->SetCurPos( fm->GetFsize() - (uint) offset );
    }

    virtual long GetPosition() const
    {
        return fm->GetCurPos();
    }

    virtual void SetPosition( long position )
    {
        fm->SetCurPos( (uint) position );
    }

    virtual int GetError() const
    {
        return 0;
    }

    virtual void ClearError()
    {}
};

Node*  ConvertFbxPass1( FbxScene* scene, FbxNode* fbx_node, vector< FbxNode* >& fbx_bones );
void   ConvertFbxPass2( Node* root_node, Node* node, FbxScene* fbx_scene, FbxNode* fbx_node );
Matrix ConvertFbxMatrix( const FbxAMatrix& m );

/************************************************************************/
/* Models                                                               */
/************************************************************************/

PCharVec GraphicLoader::processedFiles;
NodeVec  GraphicLoader::loadedModels;
PtrVec   GraphicLoader::loadedAnimations;

Node* GraphicLoader::LoadModel( const char* fname )
{
    // Find already loaded
    for( auto it = loadedModels.begin(), end = loadedModels.end(); it != end; ++it )
    {
        Node* node = *it;
        if( Str::CompareCase( node->GetName(), fname ) )
            return node;
    }

    // Add to already processed
    for( uint i = 0, j = (uint) processedFiles.size(); i < j; i++ )
        if( Str::CompareCase( processedFiles[ i ], fname ) )
            return NULL;
    processedFiles.push_back( Str::Duplicate( fname ) );

    // Load file write time
    FileManager file;
    if( !file.LoadFile( fname, PT_DATA, true ) )
    {
        WriteLogF( _FUNC_, " - 3d file not found, name<%s>.\n", fname );
        return NULL;
    }

    // Try load from cache
    char fname_cache[ MAX_FOPATH ] = { 0 };
    Str::Copy( fname_cache, fname );
    Str::Replacement( fname_cache, '/', '_' );
    Str::Replacement( fname_cache, '\\', '_' );
    Str::Append( fname_cache, "b" );
    FileManager file_cache;
    if( file_cache.LoadFile( fname_cache, PT_CACHE ) )
    {
        uint version = file_cache.GetBEUInt();
        if( version != MODELS_BINARY_VERSION || file.GetWriteTime() > file_cache.GetWriteTime() )
            file_cache.UnloadFile();                  // Disable loading from this binary, because its outdated
    }
    if( file_cache.IsLoaded() )
    {
        // Load nodes
        Node* root_node = new Node();
        root_node->Load( file_cache );
        root_node->FixAfterLoad( root_node );
        CreateBuffers( root_node );

        // Load animations
        uint anim_sets_count = file_cache.GetBEUInt();
        for( uint i = 0; i < anim_sets_count; i++ )
        {
            AnimSet* anim_set = new AnimSet();
            anim_set->Load( file_cache );
            loadedAnimations.push_back( anim_set );
        }
        return root_node;
    }

    // Load file data
    if( !file.LoadFile( fname, PT_DATA ) )
    {
        WriteLogF( _FUNC_, " - 3d file not found, name<%s>.\n", fname );
        return NULL;
    }

    // Result node
    Node* root_node = NULL;
    uint  loaded_anim_sets = 0;

    // Check extension
    const char* ext = FileManager::GetExtension( fname );
    if( !ext )
    {
        WriteLogF( _FUNC_, " - Extension not found, file<%s>.\n", fname );
        return NULL;
    }

    // FBX loader
    if( Str::CompareCase( ext, "fbx" ) )
    {
        // Create manager
        static FbxManager* fbx_manager = NULL;
        if( !fbx_manager )
        {
            fbx_manager = FbxManager::Create();
            if( !fbx_manager )
            {
                WriteLogF( _FUNC_, " - Unable to create FBX Manager.\n" );
                return NULL;
            }

            // Create an IOSettings object. This object holds all import/export settings.
            FbxIOSettings* ios = FbxIOSettings::Create( fbx_manager, IOSROOT );
            fbx_manager->SetIOSettings( ios );

            // Load plugins from the executable directory (optional)
            fbx_manager->LoadPluginsDirectory( FbxGetApplicationDirectory().Buffer() );
        }

        // Create an FBX scene
        FbxScene* fbx_scene = FbxScene::Create( fbx_manager, "My Scene" );
        if( !fbx_scene )
        {
            WriteLogF( _FUNC_, " - Unable to create FBX scene.\n" );
            return NULL;
        }

        // Create an importer
        FbxImporter* fbx_importer = FbxImporter::Create( fbx_manager, "" );
        if( !fbx_importer )
        {
            WriteLogF( _FUNC_, " - Unable to create FBX importer.\n" );
            return NULL;
        }

        // Initialize the importer
        FbxStreamImpl fbx_stream;
        if( !fbx_importer->Initialize( &fbx_stream, &file, -1, fbx_manager->GetIOSettings() ) )
        {
            WriteLogF( _FUNC_, " - Call to FbxImporter::Initialize() failed, error<%s>.\n", fbx_importer->GetStatus().GetErrorString() );
            if( fbx_importer->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion )
            {
                int file_major, file_minor, file_revision;
                int sdk_major,  sdk_minor,  sdk_revision;
                FbxManager::GetFileFormatVersion( sdk_major, sdk_minor, sdk_revision );
                fbx_importer->GetFileVersion( file_major, file_minor, file_revision );
                WriteLogF( _FUNC_, " - FBX file format version for this FBX SDK is %d.%d.%d.\n", sdk_major, sdk_minor, sdk_revision );
                WriteLogF( _FUNC_, " - FBX file format version for file<%s> is %d.%d.%d.\n", fname, file_major, file_minor, file_revision );
            }
            return NULL;
        }

        // Import the scene
        if( !fbx_importer->Import( fbx_scene ) )
        {
            // Todo: Password
            /*if (fbx_importer->GetStatus().GetCode() == FbxStatus::ePasswordError)
                        {
                                char password[MAX_FOTEXT];
                                IOS_REF.SetStringProp(IMP_FBX_PASSWORD, FbxString(password));
                                IOS_REF.SetBoolProp(IMP_FBX_PASSWORD_ENABLE, true);
                                if(!fbx_importer->Import(pScene) && lImporter->GetStatus().GetCode() == FbxStatus::ePasswordError)
                                        return NULL;
                        }*/
            WriteLogF( _FUNC_, " - Can't import scene, file<%s>.\n", fname );
            return NULL;
        }

        // Load hierarchy
        vector< FbxNode* > fbx_bones;
        root_node = ConvertFbxPass1( fbx_scene, fbx_scene->GetRootNode(), fbx_bones );
        ConvertFbxPass2( root_node, root_node, fbx_scene, fbx_scene->GetRootNode() );
        root_node->Name = Str::Duplicate( fname );
        loadedModels.push_back( root_node );

        // Extract animations
        FloatVec          times;
        VectorVec         sv;
        QuaternionVec     rv;
        VectorVec         tv;
        FbxAnimEvaluator* fbx_anim_evaluator = fbx_manager->GetAnimationEvaluator();
        FbxCriteria       fbx_anim_stack_criteria = FbxCriteria::ObjectType( fbx_anim_evaluator->GetContext()->GetClassId() );
        for( int i = 0, j = fbx_scene->GetSrcObjectCount( fbx_anim_stack_criteria ); i < j; i++ )
        {
            FbxAnimStack* fbx_anim_stack = (FbxAnimStack*) fbx_scene->GetSrcObject( fbx_anim_stack_criteria, i );
            fbx_anim_evaluator->SetContext( fbx_anim_stack );

            FbxTakeInfo* take_info = fbx_importer->GetTakeInfo( i );
            int          frames_count = (int) take_info->mLocalTimeSpan.GetDuration().GetFrameCount() + 1;
            float        frame_rate = (float) ( frames_count - 1 ) / (float) take_info->mLocalTimeSpan.GetDuration().GetSecondDouble();

            times.resize( frames_count );
            sv.resize( frames_count );
            rv.resize( frames_count );
            tv.resize( frames_count );

            AnimSet* anim_set = new AnimSet();
            for( uint n = 0; n < (uint) fbx_bones.size(); n++ )
            {
                FbxTime cur_time;
                for( int f = 0; f < frames_count; f++ )
                {
                    float time = (float) f;
                    cur_time.SetFrame( f );

                    times[ f ] = time;

                    Matrix m = ConvertFbxMatrix( fbx_anim_evaluator->GetNodeLocalTransform( fbx_bones[ n ], cur_time ) );
                    m.Decompose( sv[ f ], rv[ f ], tv[ f ] );
                }
                anim_set->AddBoneOutput( fbx_bones[ n ]->GetName(), times, sv, times, rv, times, tv );
            }

            anim_set->SetData( fname, take_info->mName.Buffer(), (float) frames_count, frame_rate );
            loadedAnimations.push_back( anim_set );
            loaded_anim_sets++;
        }

        // Release importer and scene
        fbx_importer->Destroy( true );
        fbx_scene->Destroy( true );
    }
    // Assimp loader
    else
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

        // Load scene
        aiScene* scene = (aiScene*) Ptr_aiImportFileFromMemory( (const char*) file.GetBuf(), file.GetFsize(),
                                                                aiProcess_CalcTangentSpace | aiProcess_GenNormals | aiProcess_GenUVCoords |
                                                                aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                                                                aiProcess_SortByPType | aiProcess_SplitLargeMeshes | aiProcess_LimitBoneWeights |
                                                                aiProcess_ImproveCacheLocality, "" );
        if( !scene )
        {
            WriteLogF( _FUNC_, " - Can't load 3d file, name<%s>, error<%s>.\n", fname, Ptr_aiGetErrorString() );
            return NULL;
        }

        // Extract nodes
        root_node = ConvertAssimpPass1( scene, scene->mRootNode );
        ConvertAssimpPass2( root_node, root_node, scene, scene->mRootNode );
        root_node->Name = Str::Duplicate( fname );
        loadedModels.push_back( root_node );

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

                anim_set->AddBoneOutput( na->mNodeName.data, st, sv, rt, rv, tt, tv );
            }

            anim_set->SetData( fname, anim->mName.data, (float) anim->mDuration, (float) anim->mTicksPerSecond );
            loadedAnimations.push_back( anim_set );
            loaded_anim_sets++;
        }

        Ptr_aiReleaseImport( scene );
    }

    // Fix nodes
    CreateBuffers( root_node );

    // Save to cache
    file_cache.SwitchToWrite();
    file_cache.SetBEUInt( MODELS_BINARY_VERSION );
    root_node->Save( file_cache );
    file_cache.SetBEUInt( loaded_anim_sets );
    for( uint i = 0; i < loaded_anim_sets; i++ )
        ( (AnimSet*) loadedAnimations[ loadedAnimations.size() - i - 1 ] )->Save( file_cache );
    file_cache.SaveOutBufToFile( fname_cache, PT_CACHE );

    return root_node;
}

Node* ConvertAssimpPass1( aiScene* scene, aiNode* ai_node )
{
    Node* node = new Node();
    node->Name = ai_node->mName.data;
    node->TransformationMatrix = ai_node->mTransformation;
    node->CombinedTransformationMatrix = Matrix();
    node->Children.resize( ai_node->mNumChildren );
    node->Mesh.resize( ai_node->mNumMeshes );

    for( uint m = 0; m < ai_node->mNumMeshes; m++ )
    {
        aiMesh*     ai_mesh = scene->mMeshes[ ai_node->mMeshes[ m ] ];
        MeshSubset& ms = node->Mesh[ m ];

        // Vertices
        ms.Vertices.resize( ai_mesh->mNumVertices );
        ms.VerticesTransformed.resize( ai_mesh->mNumVertices );
        bool has_tangents_and_bitangents = ai_mesh->HasTangentsAndBitangents();
        bool has_color0 = ai_mesh->HasVertexColors( 0 );
        // bool has_color1 = aimesh->HasVertexColors( 1 );
        bool has_tex_coords0 = ai_mesh->HasTextureCoords( 0 );
        bool has_tex_coords1 = ai_mesh->HasTextureCoords( 1 );
        bool has_tex_coords2 = ai_mesh->HasTextureCoords( 2 );
        for( uint i = 0; i < ai_mesh->mNumVertices; i++ )
        {
            Vertex3D& v = ms.Vertices[ i ];
            memzero( &v, sizeof( v ) );
            v.Position = ai_mesh->mVertices[ i ];
            v.PositionW = 1.0f;
            v.Normal = ai_mesh->mNormals[ i ];
            if( has_tangents_and_bitangents )
            {
                v.Tangent = ai_mesh->mTangents[ i ];
                v.Bitangent = ai_mesh->mBitangents[ i ];
            }
            if( has_color0 )
            {
                v.Color[ 2 ] = ai_mesh->mColors[ 0 ][ i ].r;
                v.Color[ 1 ] = ai_mesh->mColors[ 0 ][ i ].g;
                v.Color[ 0 ] = ai_mesh->mColors[ 0 ][ i ].b;
                v.Color[ 3 ] = ai_mesh->mColors[ 0 ][ i ].a;
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
                v.TexCoord[ 0 ] = ai_mesh->mTextureCoords[ 0 ][ i ].x;
                v.TexCoord[ 1 ] = ai_mesh->mTextureCoords[ 0 ][ i ].y;
            }
            if( has_tex_coords1 )
            {
                v.TexCoord2[ 0 ] = ai_mesh->mTextureCoords[ 1 ][ i ].x;
                v.TexCoord2[ 1 ] = ai_mesh->mTextureCoords[ 1 ][ i ].y;
            }
            if( has_tex_coords2 )
            {
                v.TexCoord3[ 0 ] = ai_mesh->mTextureCoords[ 2 ][ i ].x;
                v.TexCoord3[ 1 ] = ai_mesh->mTextureCoords[ 2 ][ i ].y;
            }
            v.BlendIndices[ 0 ] = -1.0f;
            v.BlendIndices[ 1 ] = -1.0f;
            v.BlendIndices[ 2 ] = -1.0f;
            v.BlendIndices[ 3 ] = -1.0f;
        }

        // Faces
        ms.Indicies.resize( ai_mesh->mNumFaces * 3 );
        for( uint i = 0; i < ai_mesh->mNumFaces; i++ )
        {
            aiFace& face = ai_mesh->mFaces[ i ];
            ms.Indicies[ i * 3 + 0 ] = face.mIndices[ 0 ];
            ms.Indicies[ i * 3 + 1 ] = face.mIndices[ 1 ];
            ms.Indicies[ i * 3 + 2 ] = face.mIndices[ 2 ];
        }

        // Material
        aiMaterial* material = scene->mMaterials[ ai_mesh->mMaterialIndex ];
        aiString    path;
        if( Ptr_aiGetMaterialTextureCount( material, aiTextureType_DIFFUSE ) )
        {
            Ptr_aiGetMaterialTexture( material, aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL, NULL );
            ms.DiffuseTexture = path.data;
        }
        Ptr_aiGetMaterialFloatArray( material, AI_MATKEY_COLOR_DIFFUSE, (float*) &ms.DiffuseColor, NULL );
        Ptr_aiGetMaterialFloatArray( material, AI_MATKEY_COLOR_AMBIENT, (float*) &ms.AmbientColor, NULL );
        ms.DiffuseColor[ 3 ] = 1.0f;
        ms.AmbientColor[ 3 ] = 1.0f;

        // Effect
        ms.DrawEffect.EffectFilename = NULL;

        // Transformed vertices position
        ms.VerticesTransformed = ms.Vertices;
        ms.VerticesTransformedValid = false;
    }

    for( uint i = 0; i < ai_node->mNumChildren; i++ )
        node->Children[ i ] = ConvertAssimpPass1( scene, ai_node->mChildren[ i ] );
    return node;
}

void ConvertAssimpPass2( Node* root_node, Node* node, aiScene* scene, aiNode* ai_node )
{
    for( uint m = 0; m < ai_node->mNumMeshes; m++ )
    {
        aiMesh*     ai_mesh = scene->mMeshes[ ai_node->mMeshes[ m ] ];
        MeshSubset& ms = node->Mesh[ m ];

        // Bones
        ms.SkinBoneOffsets.resize( ai_mesh->mNumBones );
        ms.SkinBoneNames.resize( ai_mesh->mNumBones );
        memzero( ms.BoneCombinedMatrices, sizeof( ms.BoneCombinedMatrices ) );
        memzero( ms.SkinBoneIndicies, sizeof( ms.SkinBoneIndicies ) );
        for( uint i = 0; i < ai_mesh->mNumBones; i++ )
        {
            aiBone* bone = ai_mesh->mBones[ i ];

            // Matrices
            ms.SkinBoneOffsets[ i ] = bone->mOffsetMatrix;
            ms.SkinBoneNames[ i ] = bone->mName.data;
            Node* bone_node = root_node->Find( bone->mName.data );
            int bone_index = bone_node->GetBoneIndex();
            ms.BoneCombinedMatrices[ bone_index ] = &bone_node->CombinedTransformationMatrix;
            ms.BoneOffsetMatrices[ bone_index ] = ms.SkinBoneOffsets[ i ];
            ms.SkinBoneIndicies[ bone_index ] = i;

            // Blend data
            for( uint j = 0; j < bone->mNumWeights; j++ )
            {
                aiVertexWeight& vw = bone->mWeights[ j ];
                Vertex3D&       v = ms.Vertices[ vw.mVertexId ];
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
                v.BlendIndices[ index ] = (float) bone_index;
            }
        }

        // Drop not filled indices
        for( uint i = 0; i < ai_mesh->mNumVertices; i++ )
        {
            Vertex3D& v = ms.Vertices[ i ];
            for( int b = 0; b < BONES_PER_VERTEX; b++ )
                if( v.BlendIndices[ b ] < 0.0f )
                    v.BlendIndices[ b ] = v.BlendWeights[ b ] = 0.0f;
        }
    }

    for( uint i = 0; i < ai_node->mNumChildren; i++ )
        ConvertAssimpPass2( root_node, node->Children[ i ], scene, ai_node->mChildren[ i ] );
}

Node* ConvertFbxPass1( FbxScene* scene, FbxNode* fbx_node, vector< FbxNode* >& fbx_bones )
{
    Node* node = new Node();
    node->Name = fbx_node->GetName();
    node->TransformationMatrix = ConvertFbxMatrix( fbx_node->EvaluateLocalTransform() );
    node->CombinedTransformationMatrix = Matrix();
    node->Children.resize( fbx_node->GetChildCount() );

    if( fbx_node->GetSkeleton() != NULL )
        fbx_bones.push_back( fbx_node );

    for( int i = 0; i < fbx_node->GetChildCount(); i++ )
        node->Children[ i ] = ConvertFbxPass1( scene, fbx_node->GetChild( i ), fbx_bones );
    return node;
}

void ConvertFbxPass2( Node* root_node, Node* node, FbxScene* fbx_scene, FbxNode* fbx_node )
{
    FbxMesh* fbx_mesh = fbx_node->GetMesh();
    if( fbx_mesh && fbx_node->Show && fbx_mesh->GetPolygonVertexCount() == fbx_mesh->GetPolygonCount() * 3 && fbx_mesh->GetPolygonCount() > 0 )
    {
        int material_count = fbx_node->GetMaterialCount();
        node->Mesh.resize( material_count );

        // Vertices
        int*        vertices = fbx_mesh->GetPolygonVertices();
        int         vertices_count = fbx_mesh->GetPolygonVertexCount();
        FbxVector4* vertices_data = fbx_mesh->GetControlPoints();
        int         polygon_count = fbx_mesh->GetPolygonCount();

        // Vertex data
        FbxGeometryElementNormal*   fbx_normals = fbx_mesh->GetElementNormal();
        FbxGeometryElementTangent*  fbx_tangents = fbx_mesh->GetElementTangent();
        FbxGeometryElementBinormal* fbx_binormals = fbx_mesh->GetElementBinormal();
        FbxLayerElementVertexColor* fbx_colors = fbx_mesh->GetElementVertexColor();
        FbxGeometryElementUV*       fbx_uvs0 = fbx_mesh->GetElementUV( 0 );
        FbxGeometryElementUV*       fbx_uvs1 = fbx_mesh->GetElementUV( 1 );
        FbxGeometryElementUV*       fbx_uvs2 = fbx_mesh->GetElementUV( 2 );

        // Reserve space for material vertices
        vector< vector< int > > material_vertices( material_count );
        for( int m = 0; m < material_count; m++ )
            material_vertices[ m ].reserve( vertices_count );

        // Determine material vertices
        FbxGeometryElementMaterial* fbx_material_element = fbx_mesh->GetElementMaterial();
        FbxGeometryElement::EMappingMode mat_mapping = fbx_material_element->GetMappingMode();
        for( int i = 0; i < polygon_count; i++ )
        {
            int m = fbx_material_element->GetIndexArray().GetAt( mat_mapping == FbxGeometryElement::eAllSame ? 0 : i );
            material_vertices[ m ].push_back( i * 3 + 0 );
            material_vertices[ m ].push_back( i * 3 + 1 );
            material_vertices[ m ].push_back( i * 3 + 2 );
        }

        // Process each subset
        for( int m = 0; m < material_count; m++ )
        {
            MeshSubset& ms = node->Mesh[ m ];

            int material_vertices_count = (int) material_vertices[ m ].size();

            ms.Vertices.resize( material_vertices_count );
            ms.VerticesTransformed.resize( material_vertices_count );

            for( int i = 0; i < material_vertices_count; i++ )
            {
                Vertex3D&   v = ms.Vertices[ i ];
                int         vertex_index = material_vertices[ m ][ i ];
                FbxVector4& fbx_v = vertices_data[ vertices[ vertex_index ] ];

                memzero( &v, sizeof( v ) );
                v.Position = Vector( (float) fbx_v.mData[ 0 ], (float) fbx_v.mData[ 1 ], (float) fbx_v.mData[ 2 ] );
                v.PositionW = 1.0f;

                #define FBX_GET_ELEMENT( elements, index )                                                                                                                     \
                    ( elements->GetMappingMode() == FbxGeometryElement::eByPolygonVertex ?                                                                                     \
                      ( elements->GetReferenceMode() == FbxGeometryElement::eDirect ?                                                                                          \
                        elements->GetDirectArray().GetAt( index ) : elements->GetDirectArray().GetAt( elements->GetIndexArray().GetAt( index ) ) ) :                           \
                      ( elements->GetMappingMode() == FbxGeometryElement::eByControlPoint ?                                                                                    \
                        ( elements->GetReferenceMode() == FbxGeometryElement::eDirect ?                                                                                        \
                          elements->GetDirectArray().GetAt( vertices[ index ] ) : elements->GetDirectArray().GetAt( elements->GetIndexArray().GetAt( vertices[ index ] ) ) ) : \
                        elements->GetDirectArray().GetAt( -1 ) ) )

                if( fbx_normals )
                {
                    const FbxVector4& fbx_normal = FBX_GET_ELEMENT( fbx_normals, vertex_index );
                    v.Normal = Vector( (float) fbx_normal[ 0 ], (float) fbx_normal[ 1 ], (float) fbx_normal[ 2 ] );
                }
                if( fbx_tangents )
                {
                    const FbxVector4& fbx_tangent = FBX_GET_ELEMENT( fbx_tangents, vertex_index );
                    v.Tangent = Vector( (float) fbx_tangent[ 0 ], (float) fbx_tangent[ 1 ], (float) fbx_tangent[ 2 ] );
                }
                if( fbx_binormals )
                {
                    const FbxVector4& fbx_binormal = FBX_GET_ELEMENT( fbx_binormals, vertex_index );
                    v.Bitangent = Vector( (float) fbx_binormal[ 0 ], (float) fbx_binormal[ 1 ], (float) fbx_binormal[ 2 ] );
                }
                if( fbx_colors )
                {
                    const FbxColor& fbx_color = FBX_GET_ELEMENT( fbx_colors, vertex_index );
                    v.Color[ 2 ] = (float) fbx_color.mRed;
                    v.Color[ 1 ] = (float) fbx_color.mGreen;
                    v.Color[ 0 ] = (float) fbx_color.mBlue;
                    v.Color[ 3 ] = (float) fbx_color.mAlpha;
                }
                if( fbx_uvs0 )
                {
                    const FbxVector2& fbx_uv0 = FBX_GET_ELEMENT( fbx_uvs0, vertex_index );
                    v.TexCoord[ 0 ] = (float) fbx_uv0[ 0 ];
                    v.TexCoord[ 1 ] = (float) fbx_uv0[ 1 ];
                }
                if( fbx_uvs1 )
                {
                    const FbxVector2& fbx_uv1 = FBX_GET_ELEMENT( fbx_uvs1, vertex_index );
                    v.TexCoord2[ 0 ] = (float) fbx_uv1[ 0 ];
                    v.TexCoord2[ 1 ] = (float) fbx_uv1[ 1 ];
                }
                if( fbx_uvs2 )
                {
                    const FbxVector2& fbx_uv2 = FBX_GET_ELEMENT( fbx_uvs2, vertex_index );
                    v.TexCoord3[ 0 ] = (float) fbx_uv2[ 0 ];
                    v.TexCoord3[ 1 ] = (float) fbx_uv2[ 1 ];
                }
                #undef FBX_GET_ELEMENT

                v.BlendIndices[ 0 ] = -1.0f;
                v.BlendIndices[ 1 ] = -1.0f;
                v.BlendIndices[ 2 ] = -1.0f;
                v.BlendIndices[ 3 ] = -1.0f;
            }

            // Faces
            ms.Indicies.resize( material_vertices_count );
            for( int i = 0; i < material_vertices_count; i++ )
                ms.Indicies[ i ] = i;

            // Material
            for( int i = 0; i < 4; i++ )
            {
                ms.DiffuseColor[ i ] = ( i < 3 ? 0.0f : 1.0f );
                ms.AmbientColor[ i ] = ( i < 3 ? 0.0f : 1.0f );
            }

            FbxSurfaceMaterial* fbx_material = fbx_node->GetMaterial( m );
            FbxProperty         prop_diffuse = fbx_material->FindProperty( "DiffuseColor" );
            FbxProperty         prop_diffuse_factor = fbx_material->FindProperty( "DiffuseFactor" );
            if( prop_diffuse.IsValid() )
            {
                if( prop_diffuse.GetSrcObjectCount() > 0 )
                {
                    char tex_fname[ MAX_FOPATH ];
                    FbxFileTexture* fbx_file_texture = (FbxFileTexture*) prop_diffuse.GetSrcObject();
                    FileManager::ExtractFileName( fbx_file_texture->GetFileName(), tex_fname );
                    ms.DiffuseTexture = tex_fname;
                }

                if( prop_diffuse_factor.IsValid() )
                {
                    FbxDouble3 color = prop_diffuse.Get< FbxDouble3 >();
                    FbxDouble  factor = prop_diffuse_factor.Get< FbxDouble >();
                    ms.DiffuseColor[ 0 ] = (float) ( color.mData[ 0 ] * factor );
                    ms.DiffuseColor[ 1 ] = (float) ( color.mData[ 1 ] * factor );
                    ms.DiffuseColor[ 2 ] = (float) ( color.mData[ 2 ] * factor );
                }
            }

            FbxProperty prop_ambient = fbx_material->FindProperty( "AmbientColor" );
            FbxProperty prop_ambient_factor = fbx_material->FindProperty( "AmbientFactor" );
            if( prop_ambient.IsValid() && prop_ambient_factor.IsValid() )
            {
                FbxDouble3 color = prop_ambient.Get< FbxDouble3 >();
                FbxDouble  factor = prop_ambient_factor.Get< FbxDouble >();
                ms.AmbientColor[ 0 ] = (float) ( color.mData[ 0 ] * factor );
                ms.AmbientColor[ 1 ] = (float) ( color.mData[ 1 ] * factor );
                ms.AmbientColor[ 2 ] = (float) ( color.mData[ 2 ] * factor );
            }

            // Effect
            ms.DrawEffect.EffectFilename = NULL;

            // Transformed vertices position
            ms.VerticesTransformed = ms.Vertices;
            ms.VerticesTransformedValid = false;

            // Skinning
            memzero( ms.SkinBoneIndicies, sizeof( ms.SkinBoneIndicies ) );
            memzero( ms.BoneCombinedMatrices, sizeof( ms.BoneCombinedMatrices ) );
            if( fbx_mesh->GetDeformerCount( FbxDeformer::eSkin ) )
            {
                FbxSkin* fbx_skin = (FbxSkin*) fbx_mesh->GetDeformer( 0, FbxDeformer::eSkin );

                // Bones
                int num_bones = fbx_skin->GetClusterCount();
                ms.SkinBoneOffsets.resize( num_bones );
                ms.SkinBoneNames.resize( num_bones );
                for( int i = 0; i < num_bones; i++ )
                {
                    FbxCluster* fbx_cluster = fbx_skin->GetCluster( i );

                    // Matrices
                    FbxAMatrix link_matrix;
                    fbx_cluster->GetTransformLinkMatrix( link_matrix );
                    FbxAMatrix cur_matrix;
                    fbx_cluster->GetTransformMatrix( cur_matrix );
                    ms.SkinBoneOffsets[ i ] = ConvertFbxMatrix( link_matrix ).Inverse() * ConvertFbxMatrix( cur_matrix );
                    ms.SkinBoneNames[ i ] = fbx_cluster->GetLink()->GetName();

                    Node* bone_node = root_node->Find( fbx_cluster->GetLink()->GetName() );
                    int bone_index = bone_node->GetBoneIndex();
                    ms.BoneCombinedMatrices[ bone_index ] = &bone_node->CombinedTransformationMatrix;
                    ms.BoneOffsetMatrices[ bone_index ] = ms.SkinBoneOffsets[ i ];
                    ms.SkinBoneIndicies[ bone_index ] = i;

                    // Blend data
                    int     num_weights = fbx_cluster->GetControlPointIndicesCount();
                    int*    indicies = fbx_cluster->GetControlPointIndices();
                    double* weights = fbx_cluster->GetControlPointWeights();
                    for( int j = 0; j < num_weights; j++ )
                    {
                        for( int k = 0; k < material_vertices_count; k++ )
                        {
                            if( vertices[ material_vertices[ m ][ k ] ] != indicies[ j ] )
                                continue;

                            Vertex3D& v = ms.Vertices[ k ];
                            uint      index;
                            if( v.BlendIndices[ 0 ] < 0.0f )
                                index = 0;
                            else if( v.BlendIndices[ 1 ] < 0.0f )
                                index = 1;
                            else if( v.BlendIndices[ 2 ] < 0.0f )
                                index = 2;
                            else
                                index = 3;

                            v.BlendWeights[ index ] = (float) weights[ j ];
                            v.BlendIndices[ index ] = (float) bone_index;
                        }
                    }
                }

                // Drop not filled indices
                for( size_t i = 0; i < ms.Vertices.size(); i++ )
                {
                    Vertex3D& v = ms.Vertices[ i ];
                    for( int b = 0; b < BONES_PER_VERTEX; b++ )
                        if( v.BlendIndices[ b ] < 0.0f )
                            v.BlendIndices[ b ] = v.BlendWeights[ b ] = 0.0f;
                }
            }
        }
    }

    for( int i = 0; i < fbx_node->GetChildCount(); i++ )
        ConvertFbxPass2( root_node, node->Children[ i ], fbx_scene, fbx_node->GetChild( i ) );
}

void CreateBuffers( Node* node )
{
    for( size_t m = 0; m < node->Mesh.size(); m++ )
    {
        MeshSubset& ms = node->Mesh[ m ];

        // Generate OGL buffers
        GL( glGenBuffers( 1, &ms.VBO ) );
        GL( glBindBuffer( GL_ARRAY_BUFFER, ms.VBO ) );
        GL( glBufferData( GL_ARRAY_BUFFER, ms.Vertices.size() * sizeof( Vertex3D ), &ms.Vertices[ 0 ], GL_STATIC_DRAW ) );
        GL( glGenBuffers( 1, &ms.IBO ) );
        GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ms.IBO ) );
        GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, ms.Indicies.size() * sizeof( short ), &ms.Indicies[ 0 ], GL_STATIC_DRAW ) );
        ms.VAO = 0;
        if( GL_HAS( vertex_array_object ) && ( GL_HAS( framebuffer_object ) || GL_HAS( framebuffer_object_ext ) ) )
        {
            GL( glGenVertexArrays( 1, &ms.VAO ) );
            GL( glBindVertexArray( ms.VAO ) );
            GL( glVertexAttribPointer( 0, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Position ) ) );
            GL( glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Normal ) ) );
            GL( glVertexAttribPointer( 2, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Color ) ) );
            GL( glVertexAttribPointer( 3, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoord ) ) );
            GL( glVertexAttribPointer( 4, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoord2 ) ) );
            GL( glVertexAttribPointer( 5, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoord3 ) ) );
            GL( glVertexAttribPointer( 6, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Tangent ) ) );
            GL( glVertexAttribPointer( 7, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Bitangent ) ) );
            GL( glVertexAttribPointer( 8, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, BlendWeights ) ) );
            GL( glVertexAttribPointer( 9, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, BlendIndices ) ) );
            for( uint i = 0; i <= 9; i++ )
                GL( glEnableVertexAttribArray( i ) );
            GL( glBindVertexArray( 0 ) );
        }
    }

    for( auto it = node->Children.begin(), end = node->Children.end(); it != end; ++it )
        CreateBuffers( *it );
}

Matrix ConvertFbxMatrix( const FbxAMatrix& m )
{
    return Matrix( (float) m.Get( 0, 0 ), (float) m.Get( 1, 0 ), (float) m.Get( 2, 0 ), (float) m.Get( 3, 0 ),
                   (float) m.Get( 0, 1 ), (float) m.Get( 1, 1 ), (float) m.Get( 2, 1 ), (float) m.Get( 3, 1 ),
                   (float) m.Get( 0, 2 ), (float) m.Get( 1, 2 ), (float) m.Get( 2, 2 ), (float) m.Get( 3, 2 ),
                   (float) m.Get( 0, 3 ), (float) m.Get( 1, 3 ), (float) m.Get( 2, 3 ), (float) m.Get( 3, 3 ) );
}

AnimSet* GraphicLoader::LoadAnimation( const char* anim_fname, const char* anim_name )
{
    // Find in already loaded
    bool take_first = Str::CompareCase( anim_name, "Base" );
    for( uint i = 0, j = (uint) loadedAnimations.size(); i < j; i++ )
    {
        AnimSet* anim = (AnimSet*) loadedAnimations[ i ];
        if( Str::CompareCase( anim->GetFileName(), anim_fname ) && ( Str::CompareCase( anim->GetName(), anim_name ) || take_first ) )
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

void GraphicLoader::Free( Node* node )
{
    for( auto it = node->Mesh.begin(), end = node->Mesh.end(); it != end; ++it )
    {
        MeshSubset& ms = *it;
        if( ms.VAO )
            GL( glDeleteVertexArrays( 1, &ms.VAO ) );
        GL( glDeleteBuffers( 1, &ms.VBO ) );
        GL( glDeleteBuffers( 1, &ms.IBO ) );
    }
    for( auto it = node->Children.begin(), end = node->Children.end(); it != end; ++it )
        Free( *it );
}

bool GraphicLoader::IsExtensionSupported( const char* ext )
{
    static const char* arr[] =
    {
        "fo3d", "fbx", "x", "3ds", "obj", "dae", "blend", "ase", "ply", "dxf", "lwo", "lxo", "stl", "ms3d",
        "scn", "smd", "vta", "mdl", "md2", "md3", "pk3", "mdc", "md5", "bvh", "csm", "b3d", "q3d", "cob",
        "q3s", "mesh", "xml", "irrmesh", "irr", "nff", "nff", "off", "raw", "ter", "mdl", "hmp", "ndo", "ac"
    };

    for( int i = 0, j = sizeof( arr ) / sizeof( arr[ 0 ] ); i < j; i++ )
        if( Str::CompareCase( ext, arr[ i ] ) )
            return true;
    return false;
}

/************************************************************************/
/* Textures                                                             */
/************************************************************************/
MeshTextureVec GraphicLoader::loadedMeshTextures;

MeshTexture* GraphicLoader::LoadTexture( const char* texture_name, const char* model_path )
{
    if( !texture_name || !texture_name[ 0 ] )
        return NULL;

    // Try find already loaded texture
    for( auto it = loadedMeshTextures.begin(), end = loadedMeshTextures.end(); it != end; ++it )
    {
        MeshTexture* texture = *it;
        if( Str::CompareCase( texture->Name.c_str(), texture_name ) )
            return texture && texture->Id ? texture : NULL;
    }

    // Allocate structure
    MeshTexture* mesh_tex = new MeshTexture();
    mesh_tex->Name = texture_name;
    mesh_tex->Id = 0;
    loadedMeshTextures.push_back( mesh_tex );

    // First try load from textures folder
    SprMngr.PushAtlasType( RES_ATLAS_DYNAMIC );
    AnyFrames* anim = SprMngr.LoadAnimation( texture_name, PT_TEXTURES );
    if( !anim && model_path )
    {
        // After try load from file folder
        char path[ MAX_FOPATH ];
        FileManager::ExtractPath( model_path, path );
        Str::Append( path, texture_name );
        anim = SprMngr.LoadAnimation( path, PT_DATA );
    }
    SprMngr.PopAtlasType();
    if( !anim )
        return NULL;

    SpriteInfo* si = SprMngr.GetSpriteInfo( anim->Ind[ 0 ] );
    mesh_tex->Id = si->Atlas->TextureOwner->Id;
    memcpy( mesh_tex->SizeData, si->Atlas->TextureOwner->SizeData, sizeof( mesh_tex->SizeData ) );
    mesh_tex->AtlasOffsetData[ 0 ] = si->SprRect[ 0 ];
    mesh_tex->AtlasOffsetData[ 1 ] = si->SprRect[ 1 ];
    mesh_tex->AtlasOffsetData[ 2 ] = si->SprRect[ 2 ] - si->SprRect[ 0 ];
    mesh_tex->AtlasOffsetData[ 3 ] = si->SprRect[ 3 ] - si->SprRect[ 1 ];
    AnyFrames::Destroy( anim );
    return mesh_tex;
}

void GraphicLoader::DestroyTextures()
{
    for( auto it = loadedMeshTextures.begin(), end = loadedMeshTextures.end(); it != end; ++it )
        delete *it;
    loadedMeshTextures.clear();
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
    if( GL_HAS( get_program_binary ) )
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
    if( GL_HAS( get_program_binary ) )
    {
        if( file_binary.LoadFile( binary_fname, PT_CACHE ) )
        {
            if( file.GetWriteTime() > file_binary.GetWriteTime() )
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
            UNUSED_VARIABLE( format ); // OGL ES
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
        #ifdef FO_OGL_ES
        char ios_data[] = { "precision mediump float;\n" };
        ver = ios_data;
        #endif

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
            #ifndef DISABLE_EGG
            GL( glBindAttribLocation( program, 3, "InTexEggCoord" ) );
            #endif
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

        if( GL_HAS( get_program_binary ) )
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
        if( GL_HAS( get_program_binary ) )
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
    GL( effect->ColorMapAtlasOffset = glGetUniformLocation( program, "ColorMapAtlasOffset" ) );
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
            Str::Format( tex_name, "Texture%dAtlasOffset", i );
            GL( effect->TexturesAtlasOffset[ i ] = glGetUniformLocation( program, tex_name ) );
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

void GraphicLoader::EffectProcessVariables( Effect* effect, int pass,  float anim_proc /* = 0.0f */, float anim_time /* = 0.0f */, MeshTexture** textures /* = NULL */ )
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
                    if( effect->TexturesAtlasOffset[ i ] != -1 && textures && textures[ i ] )
                        GL( glUniform4fv( effect->TexturesAtlasOffset[ i ], 1, textures[ i ]->AtlasOffsetData ) );
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
    #ifndef DISABLE_EGG
    LOAD_EFFECT( Effect::Generic, "2D_Default", true, NULL );
    LOAD_EFFECT( Effect::Critter, "2D_Default", true, NULL );
    LOAD_EFFECT( Effect::Roof, "2D_Default", true, NULL );
    LOAD_EFFECT( Effect::Rain, "2D_Default", true, NULL );
    #else
    LOAD_EFFECT( Effect::Generic, "2D_WithoutEgg", true, NULL );
    LOAD_EFFECT( Effect::Critter, "2D_WithoutEgg", true, NULL );
    LOAD_EFFECT( Effect::Roof, "2D_WithoutEgg", true, NULL );
    LOAD_EFFECT( Effect::Rain, "2D_WithoutEgg", true, NULL );
    #endif
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

uchar* GraphicLoader::LoadPNG( const uchar* data, uint data_size, uint& result_width, uint& result_height )
{
    struct PNGMessage
    {
        static void Error( png_structp png_ptr, png_const_charp error_msg )
        {
            UNUSED_VARIABLE( png_ptr );
            WriteLogF( _FUNC_, " - PNG error<%s>.\n", error_msg );
        }
        static void Warning( png_structp png_ptr, png_const_charp error_msg )
        {
            UNUSED_VARIABLE( png_ptr );
            // WriteLogF( _FUNC_, " - PNG warning<%s>.\n", error_msg );
        }
    };

    // Setup PNG reader
    png_structp png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
    if( !png_ptr )
        return NULL;

    png_set_error_fn( png_ptr, png_get_error_ptr( png_ptr ), &PNGMessage::Error, &PNGMessage::Warning );

    png_infop info_ptr = png_create_info_struct( png_ptr );
    if( !info_ptr )
    {
        png_destroy_read_struct( &png_ptr, NULL, NULL );
        return NULL;
    }

    if( setjmp( png_jmpbuf( png_ptr ) ) )
    {
        png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
        return NULL;
    }

    static const uchar* data_;
    struct PNGReader
    {
        static void Read( png_structp png_ptr, png_bytep png_data, png_size_t length )
        {
            UNUSED_VARIABLE( png_ptr );
            memcpy( png_data, data_, length );
            data_ += length;
        }
    };
    data_ = data;
    png_set_read_fn( png_ptr, NULL, &PNGReader::Read );
    png_read_info( png_ptr, info_ptr );

    if( setjmp( png_jmpbuf( png_ptr ) ) )
    {
        png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
        return NULL;
    }

    // Get information
    png_uint_32 width, height;
    int         bit_depth;
    int         color_type;
    png_get_IHDR( png_ptr, info_ptr, (png_uint_32*) &width, (png_uint_32*) &height, &bit_depth, &color_type, NULL, NULL, NULL );

    // Settings
    png_set_strip_16( png_ptr );
    png_set_packing( png_ptr );
    if( color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8 )
        png_set_expand( png_ptr );
    if( color_type == PNG_COLOR_TYPE_PALETTE )
        png_set_expand( png_ptr );
    if( png_get_valid( png_ptr, info_ptr, PNG_INFO_tRNS ) )
        png_set_expand( png_ptr );
    png_set_filler( png_ptr, 0x000000ff, PNG_FILLER_AFTER );
    png_read_update_info( png_ptr, info_ptr );

    // Allocate row pointers
    png_bytepp row_pointers = (png_bytepp) malloc( height * sizeof( png_bytep ) );
    if( !row_pointers )
    {
        png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
        return NULL;
    }

    // Set the individual row_pointers to point at the correct offsets
    uchar* result = new uchar[ width * height * 4 ];
    for( uint i = 0; i < height; i++ )
        row_pointers[ i ] = result + i * width * 4;

    // Read image
    png_read_image( png_ptr, row_pointers );

    // Clean up
    png_read_end( png_ptr, info_ptr );
    png_destroy_read_struct( &png_ptr, &info_ptr, (png_infopp) NULL );
    free( row_pointers );

    // Return
    result_width = width;
    result_height = height;
    return result;
}

void GraphicLoader::SavePNG( const char* fname, uchar* data, uint width, uint height )
{
    // Initialize stuff
    png_structp png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
    if( !png_ptr )
        return;
    png_infop info_ptr = png_create_info_struct( png_ptr );
    if( !info_ptr )
        return;
    if( setjmp( png_jmpbuf( png_ptr ) ) )
        return;

    static UCharVec result_png;
    struct PNGWriter
    {
        static void Write( png_structp png_ptr, png_bytep png_data, png_size_t length )
        {
            UNUSED_VARIABLE( png_ptr );
            for( png_size_t i = 0; i < length; i++ )
                result_png.push_back( png_data[ i ] );
        }
        static void Flush( png_structp png_ptr )
        {
            UNUSED_VARIABLE( png_ptr );
        }
    };
    result_png.clear();
    png_set_write_fn( png_ptr, NULL, &PNGWriter::Write, &PNGWriter::Flush );

    // Write header
    if( setjmp( png_jmpbuf( png_ptr ) ) )
        return;
    png_byte color_type = PNG_COLOR_TYPE_RGB_ALPHA;
    png_byte bit_depth = 8;
    png_set_IHDR( png_ptr, info_ptr, width, height, bit_depth, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );
    png_write_info( png_ptr, info_ptr );

    // Write pointers
    uchar** row_pointers = new uchar*[ height ];
    for( uint y = 0; y < height; y++ )
        row_pointers[ y ] = &data[ y * width * 4 ];

    // Write bytes
    if( setjmp( png_jmpbuf( png_ptr ) ) )
        return;
    png_write_image( png_ptr, row_pointers );

    // End write
    if( setjmp( png_jmpbuf( png_ptr ) ) )
        return;
    png_write_end( png_ptr, NULL );

    // Clean up
    delete[] row_pointers;

    // Write to disk
    FileManager fm;
    fm.SetData( &result_png[ 0 ], (uint) result_png.size() );
    fm.SaveOutBufToFile( fname, PT_ROOT );
}

uchar* GraphicLoader::LoadTGA( const uchar* data, uint data_size, uint& result_width, uint& result_height )
{
    // Reading macros
    bool read_error = false;
    uint cur_pos = 0;
    #define READ_TGA( x, len )                          \
        if( !read_error && cur_pos + len <= data_size ) \
        {                                               \
            memcpy( x, data + cur_pos, len );           \
            cur_pos += len;                             \
        }                                               \
        else                                            \
        {                                               \
            read_error = true;                          \
        }

    // Load header
    unsigned char type, pixel_depth;
    short int     width, height;
    unsigned char unused_char;
    short int     unused_short;
    READ_TGA( &unused_char, 1 );
    READ_TGA( &unused_char, 1 );
    READ_TGA( &type, 1 );
    READ_TGA( &unused_short, 2 );
    READ_TGA( &unused_short, 2 );
    READ_TGA( &unused_char, 1 );
    READ_TGA( &unused_short, 2 );
    READ_TGA( &unused_short, 2 );
    READ_TGA( &width, 2 );
    READ_TGA( &height, 2 );
    READ_TGA( &pixel_depth, 1 );
    READ_TGA( &unused_char, 1 );

    // Check for errors when loading the header
    if( read_error )
        return NULL;

    // Check if the image is color indexed
    if( type == 1 )
        return NULL;

    // Check for TrueColor
    if( type != 2 && type != 10 )
        return NULL;

    // Check for RGB(A)
    if( pixel_depth != 24 && pixel_depth != 32 )
        return NULL;

    // Read
    int    bpp = pixel_depth / 8;
    uint   read_size = height * width * bpp;
    uchar* read_data = new uchar[ read_size ];
    if( type == 2 )
    {
        READ_TGA( read_data, read_size );
    }
    else
    {
        uint  bytes_read = 0, run_len, i, to_read;
        uchar header, color[ 4 ];
        int   c;
        while( bytes_read < read_size )
        {
            READ_TGA( &header, 1 );
            if( header & 0x00000080 )
            {
                header &= ~0x00000080;
                READ_TGA( color, bpp );
                if( read_error )
                {
                    delete[] read_data;
                    return NULL;
                }
                run_len = ( header + 1 ) * bpp;
                for( i = 0; i < run_len; i += bpp )
                    for( c = 0; c < bpp && bytes_read + i + c < read_size; c++ )
                        read_data[ bytes_read + i + c ] = color[ c ];
                bytes_read += run_len;
            }
            else
            {
                run_len = ( header + 1 ) * bpp;
                if( bytes_read + run_len > read_size )
                    to_read = read_size - bytes_read;
                else
                    to_read = run_len;
                READ_TGA( read_data + bytes_read, to_read );
                if( read_error )
                {
                    delete[] read_data;
                    return NULL;
                }
                bytes_read += run_len;
                if( bytes_read + run_len > read_size )
                    cur_pos += run_len - to_read;
            }
        }
    }
    if( read_error )
    {
        delete[] read_data;
        return NULL;
    }

    // Copy data
    uchar* result = new uchar[ width * height * 4 ];
    for( int i = 0, j = width * height; i < j; i++ )
    {
        result[ i * 4 + 0 ] = read_data[ i * bpp + 2 ];
        result[ i * 4 + 1 ] = read_data[ i * bpp + 1 ];
        result[ i * 4 + 2 ] = read_data[ i * bpp + 0 ];
        result[ i * 4 + 3 ] = ( bpp == 4 ? read_data[ i * bpp + 3 ] : 0xFF );
    }
    delete[] read_data;

    // Return data
    result_width = width;
    result_height = height;
    return result;
}
