#include "ResourceConverter.h"
#include "GraphicStructures.h"
#include "3dAnimation.h"
#include "assimp/cimport.h"
#include "assimp/postprocess.h"
#include "fbxsdk/fbxsdk.h"
#include "png.h"
#include "minizip/zip.h"

static uchar* LoadPNG( const uchar* data, uint data_size, uint& result_width, uint& result_height );
static uchar* LoadTGA( const uchar* data, uint data_size, uint& result_width, uint& result_height );

static Bone* ConvertAssimpPass1( aiScene* ai_scene, aiNode* ai_node );
static void  ConvertAssimpPass2( Bone* root_bone, Bone* parent_bone, Bone* bone, aiScene* ai_scene, aiNode* ai_node );

// FBX stuff
class FbxStreamImpl: public FbxStream
{
private:
    FileManager* fm;
    EState       curState;

public:
    FbxStreamImpl(): FbxStream()
    {
        fm = nullptr;
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
        fm = nullptr;
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
    {
        //
    }
};

static Bone*  ConvertFbxPass1( FbxNode* fbx_node, vector< FbxNode* >& fbx_all_nodes );
static void   ConvertFbxPass2( Bone* root_bone, Bone* bone, FbxNode* fbx_node );
static Matrix ConvertFbxMatrix( const FbxAMatrix& m );

static void FixTexCoord( float& x, float& y )
{
    if( x < 0.0f )
        x = 1.0f - fmodf( -x, 1.0f );
    else if( x > 1.0f )
        x = fmodf( x, 1.0f );
    if( y < 0.0f )
        y = 1.0f - fmodf( -y, 1.0f );
    else if( y > 1.0f )
        y = fmodf( y, 1.0f );
}

FileManager* ResourceConverter::Convert( const string& name, FileManager& file )
{
    string ext = FileManager::GetExtension( name );
    if( ext == "png" || ext == "tga" )
        return ConvertImage( name, file );
    if( !ext.empty() && Is3dExtensionSupported( ext.c_str() ) && ext != "fo3d" )
        return Convert3d( name, file );
    file.SwitchToWrite();
    return &file;
}

FileManager* ResourceConverter::ConvertImage( const string& name, FileManager& file )
{
    uchar* data;
    uint   width, height;
    string ext = FileManager::GetExtension( name );
    if( ext == "png" )
        data = LoadPNG( file.GetBuf(), file.GetFsize(), width, height );
    else
        data = LoadTGA( file.GetBuf(), file.GetFsize(), width, height );
    if( !data )
        return nullptr;

    FileManager* converted_file = new FileManager();
    converted_file->SetLEUInt( width );
    converted_file->SetLEUInt( height );
    converted_file->SetData( data, width * height * 4 );

    delete[] data;

    return converted_file;
}

FileManager* ResourceConverter::Convert3d( const string& name, FileManager& file )
{
    // Result bone
    Bone*      root_bone = nullptr;
    AnimSetVec loaded_animations;

    // FBX loader
    string ext = FileManager::GetExtension( name );
    if( ext == "fbx" )
    {
        // Create manager
        static FbxManager* fbx_manager = nullptr;
        if( !fbx_manager )
        {
            fbx_manager = FbxManager::Create();
            if( !fbx_manager )
            {
                WriteLog( "Unable to create FBX Manager.\n" );
                return nullptr;
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
            WriteLog( "Unable to create FBX scene.\n" );
            return nullptr;
        }

        // Create an importer
        FbxImporter* fbx_importer = FbxImporter::Create( fbx_manager, "" );
        if( !fbx_importer )
        {
            WriteLog( "Unable to create FBX importer.\n" );
            return nullptr;
        }

        // Initialize the importer
        FbxStreamImpl fbx_stream;
        if( !fbx_importer->Initialize( &fbx_stream, &file, -1, fbx_manager->GetIOSettings() ) )
        {
            WriteLog( "Call to FbxImporter::Initialize() failed, error '{}'.\n", fbx_importer->GetStatus().GetErrorString() );
            if( fbx_importer->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion )
            {
                int file_major, file_minor, file_revision;
                int sdk_major,  sdk_minor,  sdk_revision;
                FbxManager::GetFileFormatVersion( sdk_major, sdk_minor, sdk_revision );
                fbx_importer->GetFileVersion( file_major, file_minor, file_revision );
                WriteLog( "FBX file format version for this FBX SDK is {}.{}.{}.\n", sdk_major, sdk_minor, sdk_revision );
                WriteLog( "FBX file format version for file '{}' is {}.{}.{}.\n", name, file_major, file_minor, file_revision );
            }
            return nullptr;
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
            WriteLog( "Can't import scene, file '{}'.\n", name );
            return nullptr;
        }

        // Load hierarchy
        vector< FbxNode* > fbx_all_nodes;
        root_bone = ConvertFbxPass1( fbx_scene->GetRootNode(), fbx_all_nodes );
        ConvertFbxPass2( root_bone, root_bone, fbx_scene->GetRootNode() );

        // Extract animations
        if( fbx_scene->GetCurrentAnimationStack() )
        {
            FbxAnimEvaluator* fbx_anim_evaluator = fbx_scene->GetAnimationEvaluator();
            FbxCriteria       fbx_anim_stack_criteria = FbxCriteria::ObjectType( fbx_scene->GetCurrentAnimationStack()->GetClassId() );
            for( int i = 0, j = fbx_scene->GetSrcObjectCount( fbx_anim_stack_criteria ); i < j; i++ )
            {
                FbxAnimStack* fbx_anim_stack = (FbxAnimStack*) fbx_scene->GetSrcObject( fbx_anim_stack_criteria, i );
                fbx_scene->SetCurrentAnimationStack( fbx_anim_stack );

                FbxTakeInfo*  take_info = fbx_importer->GetTakeInfo( i );
                int           frames_count = (int) take_info->mLocalTimeSpan.GetDuration().GetFrameCount() + 1;
                float         frame_rate = (float) ( frames_count - 1 ) / (float) take_info->mLocalTimeSpan.GetDuration().GetSecondDouble();
                int           frame_offset = (int) take_info->mLocalTimeSpan.GetStart().GetFrameCount();

                FloatVec      st;
                VectorVec     sv;
                FloatVec      rt;
                QuaternionVec rv;
                FloatVec      tt;
                VectorVec     tv;
                st.reserve( frames_count );
                sv.reserve( frames_count );
                rt.reserve( frames_count );
                rv.reserve( frames_count );
                tt.reserve( frames_count );
                tv.reserve( frames_count );

                AnimSet* anim_set = new AnimSet();
                for( uint n = 0; n < (uint) fbx_all_nodes.size(); n++ )
                {
                    st.clear();
                    sv.clear();
                    rt.clear();
                    rv.clear();
                    tt.clear();
                    tv.clear();

                    FbxTime cur_time;
                    for( int f = 0; f < frames_count; f++ )
                    {
                        float time = (float) f;
                        cur_time.SetFrame( frame_offset + f );

                        const FbxAMatrix&    fbx_m = fbx_anim_evaluator->GetNodeLocalTransform( fbx_all_nodes[ n ], cur_time );
                        const FbxVector4&    fbx_s = fbx_m.GetS();
                        const FbxQuaternion& fbx_q = fbx_m.GetQ();
                        const FbxVector4&    fbx_t = fbx_m.GetT();
                        const Vector&        s = Vector( (float) fbx_s[ 0 ], (float) fbx_s[ 1 ], (float) fbx_s[ 2 ] );
                        const Quaternion&    r = Quaternion( (float) fbx_q[ 3 ], (float) fbx_q[ 0 ], (float) fbx_q[ 1 ], (float) fbx_q[ 2 ] );
                        const Vector&        t = Vector( (float) fbx_t[ 0 ], (float) fbx_t[ 1 ], (float) fbx_t[ 2 ] );

                        // Manage duplicates
                        if( f < 2 || sv.back() != s || sv[ sv.size() - 2 ] != s )
                            st.push_back( time ), sv.push_back( s );
                        else
                            st.back() = time;
                        if( f < 2 || rv.back() != r || rv[ rv.size() - 2 ] != r )
                            rt.push_back( time ), rv.push_back( r );
                        else
                            rt.back() = time;
                        if( f < 2 || tv.back() != t || tv[ tv.size() - 2 ] != t )
                            tt.push_back( time ), tv.push_back( t );
                        else
                            tt.back() = time;
                    }

                    UIntVec  hierarchy;
                    FbxNode* fbx_node = fbx_all_nodes[ n ];
                    while( fbx_node != nullptr )
                    {
                        hierarchy.insert( hierarchy.begin(), Bone::GetHash( fbx_node->GetName() ) );
                        fbx_node = fbx_node->GetParent();
                    }

                    anim_set->AddBoneOutput( hierarchy, st, sv, rt, rv, tt, tv );
                }

                anim_set->SetData( name, take_info->mName.Buffer(), (float) frames_count, frame_rate );
                loaded_animations.push_back( anim_set );
            }
        }

        // Release importer and scene
        fbx_importer->Destroy( true );
        fbx_scene->Destroy( true );
    }
    // Assimp loader
    else
    {
        // Logging
        if( GameOpt.AssimpLogging )
        {
            aiEnableVerboseLogging( true );
            static aiLogStream c = aiGetPredefinedLogStream( aiDefaultLogStream_FILE, "Assimp.log" );
            aiAttachLogStream( &c );
        }

        // Load scene
        aiScene* scene = (aiScene*) aiImportFileFromMemory( (const char*) file.GetBuf(), file.GetFsize(),
                                                            aiProcess_CalcTangentSpace | aiProcess_GenNormals | aiProcess_GenUVCoords |
                                                            aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                                                            aiProcess_SortByPType | aiProcess_SplitLargeMeshes | aiProcess_LimitBoneWeights |
                                                            aiProcess_ImproveCacheLocality, "" );
        if( !scene )
        {
            WriteLog( "Can't load 3d file, name '{}', error '{}'.\n", name, aiGetErrorString() );
            return nullptr;
        }

        // Extract bones
        root_bone = ConvertAssimpPass1( scene, scene->mRootNode );
        ConvertAssimpPass2( root_bone, nullptr, root_bone, scene, scene->mRootNode );

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

                UIntVec hierarchy;
                aiNode* ai_node = scene->mRootNode->FindNode( na->mNodeName );
                while( ai_node != nullptr )
                {
                    hierarchy.insert( hierarchy.begin(), Bone::GetHash( ai_node->mName.data ) );
                    ai_node = ai_node->mParent;
                }

                anim_set->AddBoneOutput( hierarchy, st, sv, rt, rv, tt, tv );
            }

            anim_set->SetData( name, anim->mName.data, (float) anim->mDuration, (float) anim->mTicksPerSecond );
            loaded_animations.push_back( anim_set );
        }

        aiReleaseImport( scene );
    }

    // Make new file
    FileManager* converted_file = new FileManager();
    root_bone->Save( *converted_file );
    converted_file->SetBEUInt( (uint) loaded_animations.size() );
    for( size_t i = 0; i < loaded_animations.size(); i++ )
        loaded_animations[ i ]->Save( *converted_file );

    delete root_bone;
    for( size_t i = 0; i < loaded_animations.size(); i++ )
        delete loaded_animations[ i ];

    return converted_file;
}

static Matrix AssimpGlobalTransform( aiNode* ai_node )
{
    return ( ai_node->mParent ? AssimpGlobalTransform( ai_node->mParent ) : Matrix() ) * ai_node->mTransformation;
}

static Bone* ConvertAssimpPass1( aiScene* ai_scene, aiNode* ai_node )
{
    Bone* bone = new Bone();
    bone->NameHash = Bone::GetHash( ai_node->mName.data );
    bone->TransformationMatrix = ai_node->mTransformation;
    bone->GlobalTransformationMatrix = AssimpGlobalTransform( ai_node );
    bone->CombinedTransformationMatrix = Matrix();
    bone->Mesh = nullptr;
    bone->Children.resize( ai_node->mNumChildren );

    for( uint i = 0; i < ai_node->mNumChildren; i++ )
        bone->Children[ i ] = ConvertAssimpPass1( ai_scene, ai_node->mChildren[ i ] );
    return bone;
}

static void ConvertAssimpPass2( Bone* root_bone, Bone* parent_bone, Bone* bone, aiScene* ai_scene, aiNode* ai_node )
{
    for( uint m = 0; m < ai_node->mNumMeshes; m++ )
    {
        aiMesh* ai_mesh = ai_scene->mMeshes[ ai_node->mMeshes[ m ] ];

        // Mesh
        Bone* mesh_bone;
        if( m == 0 )
        {
            mesh_bone = bone;
        }
        else
        {
            mesh_bone = new Bone();
            mesh_bone->NameHash = Bone::GetHash( _str( "{}_{}", ai_node->mName.data, m + 1 ).c_str() );
            mesh_bone->CombinedTransformationMatrix = Matrix();
            if( parent_bone )
            {
                parent_bone->Children.push_back( mesh_bone );
                mesh_bone->TransformationMatrix = bone->TransformationMatrix;
                mesh_bone->GlobalTransformationMatrix = AssimpGlobalTransform( ai_node );
            }
            else
            {
                bone->Children.push_back( mesh_bone );
                mesh_bone->TransformationMatrix = Matrix();
                mesh_bone->GlobalTransformationMatrix = AssimpGlobalTransform( ai_node );
            }
        }

        MeshData* mesh = mesh_bone->Mesh = new MeshData();
        mesh->Owner = mesh_bone;

        // Vertices
        mesh->Vertices.resize( ai_mesh->mNumVertices );
        bool has_tangents_and_bitangents = ai_mesh->HasTangentsAndBitangents();
        bool has_tex_coords = ai_mesh->HasTextureCoords( 0 );
        for( uint i = 0; i < ai_mesh->mNumVertices; i++ )
        {
            Vertex3D& v = mesh->Vertices[ i ];
            memzero( &v, sizeof( v ) );
            v.Position = ai_mesh->mVertices[ i ];
            v.Normal = ai_mesh->mNormals[ i ];
            if( has_tangents_and_bitangents )
            {
                v.Tangent = ai_mesh->mTangents[ i ];
                v.Bitangent = ai_mesh->mBitangents[ i ];
            }
            if( has_tex_coords )
            {
                v.TexCoord[ 0 ] = ai_mesh->mTextureCoords[ 0 ][ i ].x;
                v.TexCoord[ 1 ] = 1.0f - ai_mesh->mTextureCoords[ 0 ][ i ].y;
                FixTexCoord( v.TexCoord[ 0 ], v.TexCoord[ 1 ] );
                v.TexCoordBase[ 0 ] = v.TexCoord[ 0 ];
                v.TexCoordBase[ 1 ] = v.TexCoord[ 1 ];
            }
            v.BlendIndices[ 0 ] = -1.0f;
            v.BlendIndices[ 1 ] = -1.0f;
            v.BlendIndices[ 2 ] = -1.0f;
            v.BlendIndices[ 3 ] = -1.0f;
        }

        // Faces
        mesh->Indices.resize( ai_mesh->mNumFaces * 3 );
        for( uint i = 0; i < ai_mesh->mNumFaces; i++ )
        {
            aiFace& face = ai_mesh->mFaces[ i ];
            mesh->Indices[ i * 3 + 0 ] = face.mIndices[ 0 ];
            mesh->Indices[ i * 3 + 1 ] = face.mIndices[ 1 ];
            mesh->Indices[ i * 3 + 2 ] = face.mIndices[ 2 ];
        }

        // Material
        aiMaterial* material = ai_scene->mMaterials[ ai_mesh->mMaterialIndex ];
        aiString    path;
        if( aiGetMaterialTextureCount( material, aiTextureType_DIFFUSE ) )
        {
            aiGetMaterialTexture( material, aiTextureType_DIFFUSE, 0, &path, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr );
            mesh->DiffuseTexture = path.data;
        }

        // Effect
        mesh->DrawEffect.EffectFilename = nullptr;

        // Skinning
        if( ai_mesh->mNumBones > 0 )
        {
            mesh->SkinBoneNameHashes.resize( ai_mesh->mNumBones );
            mesh->SkinBoneOffsets.resize( ai_mesh->mNumBones );
            mesh->SkinBones.resize( ai_mesh->mNumBones );
            RUNTIME_ASSERT( ai_mesh->mNumBones <= MAX_BONES_PER_MODEL );
            for( uint i = 0; i < ai_mesh->mNumBones; i++ )
            {
                aiBone* ai_bone = ai_mesh->mBones[ i ];

                // Matrices
                Bone* skin_bone = root_bone->Find( Bone::GetHash( ai_bone->mName.data ) );
                if( !skin_bone )
                {
                    WriteLog( "Skin bone '{}' for mesh '{}' not found.\n", ai_bone->mName.data, ai_node->mName.data );
                    skin_bone = bone;
                }
                mesh->SkinBoneNameHashes[ i ] = skin_bone->NameHash;
                mesh->SkinBoneOffsets[ i ] = ai_bone->mOffsetMatrix;
                mesh->SkinBones[ i ] = skin_bone;

                // Blend data
                float bone_index = (float) i;
                for( uint j = 0; j < ai_bone->mNumWeights; j++ )
                {
                    aiVertexWeight& vw = ai_bone->mWeights[ j ];
                    Vertex3D&       v = mesh->Vertices[ vw.mVertexId ];
                    uint            index;
                    if( v.BlendIndices[ 0 ] < 0.0f )
                        index = 0;
                    else if( v.BlendIndices[ 1 ] < 0.0f )
                        index = 1;
                    else if( v.BlendIndices[ 2 ] < 0.0f )
                        index = 2;
                    else
                        index = 3;
                    v.BlendIndices[ index ] = bone_index;
                    v.BlendWeights[ index ] = vw.mWeight;
                }
            }
        }
        else
        {
            mesh->SkinBoneNameHashes.resize( 1 );
            mesh->SkinBoneOffsets.resize( 1 );
            mesh->SkinBones.resize( 1 );
            mesh->SkinBoneNameHashes[ 0 ] = 0;
            mesh->SkinBoneOffsets[ 0 ] = Matrix();
            mesh->SkinBones[ 0 ] = mesh_bone;
            for( size_t i = 0, j = mesh->Vertices.size(); i < j; i++ )
            {
                Vertex3D& v = mesh->Vertices[ i ];
                v.BlendIndices[ 0 ] = 0.0f;
                v.BlendWeights[ 0 ] = 1.0f;
            }
        }

        // Drop not filled indices
        for( size_t i = 0, j = mesh->Vertices.size(); i < j; i++ )
        {
            Vertex3D& v = mesh->Vertices[ i ];
            float     w = 0.0f;
            int       last_bone = 0;
            for( int b = 0; b < BONES_PER_VERTEX; b++ )
            {
                if( v.BlendIndices[ b ] < 0.0f )
                    v.BlendIndices[ b ] = v.BlendWeights[ b ] = 0.0f;
                else
                    last_bone = b;
                w += v.BlendWeights[ b ];
            }
            v.BlendWeights[ last_bone ] += 1.0f - w;
        }
    }

    for( uint i = 0; i < ai_node->mNumChildren; i++ )
        ConvertAssimpPass2( root_bone, bone, bone->Children[ i ], ai_scene, ai_node->mChildren[ i ] );
}

static Bone* ConvertFbxPass1( FbxNode* fbx_node, vector< FbxNode* >& fbx_all_nodes )
{
    fbx_all_nodes.push_back( fbx_node );

    Bone* bone = new Bone();
    bone->NameHash = Bone::GetHash( fbx_node->GetName() );
    bone->TransformationMatrix = ConvertFbxMatrix( fbx_node->EvaluateLocalTransform() );
    bone->GlobalTransformationMatrix = ConvertFbxMatrix( fbx_node->EvaluateGlobalTransform() );
    bone->CombinedTransformationMatrix = Matrix();
    bone->Mesh = nullptr;
    bone->Children.resize( fbx_node->GetChildCount() );

    for( int i = 0; i < fbx_node->GetChildCount(); i++ )
        bone->Children[ i ] = ConvertFbxPass1( fbx_node->GetChild( i ), fbx_all_nodes );
    return bone;
}

template< class T, class T2 >
static T2 FbxGetElement( T* elements, int index, int* vertices )
{
    if( elements->GetMappingMode() == FbxGeometryElement::eByPolygonVertex )
    {
        if( elements->GetReferenceMode() == FbxGeometryElement::eDirect )
            return elements->GetDirectArray().GetAt( index );
        else if( elements->GetReferenceMode() == FbxGeometryElement::eIndexToDirect )
            return elements->GetDirectArray().GetAt( elements->GetIndexArray().GetAt( index ) );
    }
    else if( elements->GetMappingMode() == FbxGeometryElement::eByControlPoint )
    {
        if( elements->GetReferenceMode() == FbxGeometryElement::eDirect )
            return elements->GetDirectArray().GetAt( vertices[ index ] );
        else if( elements->GetReferenceMode() == FbxGeometryElement::eIndexToDirect )
            return elements->GetDirectArray().GetAt( elements->GetIndexArray().GetAt( vertices[ index ] ) );
    }

    WriteLog( "Unknown mapping mode {} or reference mode {}.\n", elements->GetMappingMode(), elements->GetReferenceMode() );
    return elements->GetDirectArray().GetAt( 0 );
}

static void ConvertFbxPass2( Bone* root_bone, Bone* bone, FbxNode* fbx_node )
{
    FbxMesh* fbx_mesh = fbx_node->GetMesh();
    if( fbx_mesh && fbx_node->Show && fbx_mesh->GetPolygonVertexCount() == fbx_mesh->GetPolygonCount() * 3 && fbx_mesh->GetPolygonCount() > 0 )
    {
        bone->Mesh = new MeshData();
        bone->Mesh->Owner = bone;
        MeshData* mesh = bone->Mesh;

        // Generate tangents
        fbx_mesh->GenerateTangentsDataForAllUVSets();

        // Vertices
        int*        vertices = fbx_mesh->GetPolygonVertices();
        int         vertices_count = fbx_mesh->GetPolygonVertexCount();
        FbxVector4* vertices_data = fbx_mesh->GetControlPoints();
        mesh->Vertices.resize( vertices_count );

        FbxGeometryElementNormal*   fbx_normals = fbx_mesh->GetElementNormal();
        FbxGeometryElementTangent*  fbx_tangents = fbx_mesh->GetElementTangent();
        FbxGeometryElementBinormal* fbx_binormals = fbx_mesh->GetElementBinormal();
        FbxGeometryElementUV*       fbx_uvs = fbx_mesh->GetElementUV();
        for( int i = 0; i < vertices_count; i++ )
        {
            Vertex3D&   v = mesh->Vertices[ i ];
            FbxVector4& fbx_v = vertices_data[ vertices[ i ] ];

            memzero( &v, sizeof( v ) );
            v.Position = Vector( (float) fbx_v.mData[ 0 ], (float) fbx_v.mData[ 1 ], (float) fbx_v.mData[ 2 ] );

            if( fbx_normals )
            {
                const FbxVector4& fbx_normal = FbxGetElement< FbxGeometryElementNormal, FbxVector4 >( fbx_normals, i, vertices );
                v.Normal = Vector( (float) fbx_normal[ 0 ], (float) fbx_normal[ 1 ], (float) fbx_normal[ 2 ] );
            }
            if( fbx_tangents )
            {
                const FbxVector4& fbx_tangent = FbxGetElement< FbxGeometryElementTangent, FbxVector4 >( fbx_tangents, i, vertices );
                v.Tangent = Vector( (float) fbx_tangent[ 0 ], (float) fbx_tangent[ 1 ], (float) fbx_tangent[ 2 ] );
            }
            if( fbx_binormals )
            {
                const FbxVector4& fbx_binormal = FbxGetElement< FbxGeometryElementBinormal, FbxVector4 >( fbx_binormals, i, vertices );
                v.Bitangent = Vector( (float) fbx_binormal[ 0 ], (float) fbx_binormal[ 1 ], (float) fbx_binormal[ 2 ] );
            }
            if( fbx_uvs )
            {
                const FbxVector2& fbx_uv = FbxGetElement< FbxGeometryElementUV, FbxVector2 >( fbx_uvs, i, vertices );
                v.TexCoord[ 0 ] = (float) fbx_uv[ 0 ];
                v.TexCoord[ 1 ] = 1.0f - (float) fbx_uv[ 1 ];
                FixTexCoord( v.TexCoord[ 0 ], v.TexCoord[ 1 ] );
                v.TexCoordBase[ 0 ] = v.TexCoord[ 0 ];
                v.TexCoordBase[ 1 ] = v.TexCoord[ 1 ];
            }
            #undef FBX_GET_ELEMENT

            v.BlendIndices[ 0 ] = -1.0f;
            v.BlendIndices[ 1 ] = -1.0f;
            v.BlendIndices[ 2 ] = -1.0f;
            v.BlendIndices[ 3 ] = -1.0f;
        }

        // Faces
        mesh->Indices.resize( vertices_count );
        for( int i = 0; i < vertices_count; i++ )
            mesh->Indices[ i ] = i;

        // Material
        FbxSurfaceMaterial* fbx_material = fbx_node->GetMaterial( 0 );
        FbxProperty         prop_diffuse = ( fbx_material ? fbx_material->FindProperty( "DiffuseColor" ) : FbxProperty() );
        if( prop_diffuse.IsValid() && prop_diffuse.GetSrcObjectCount() > 0 )
        {
            for( int i = 0, j = prop_diffuse.GetSrcObjectCount(); i < j; i++ )
            {
                if( Str::Compare( prop_diffuse.GetSrcObject( i )->GetClassId().GetName(), "FbxFileTexture" ) )
                {
                    FbxFileTexture* fbx_file_texture = (FbxFileTexture*) prop_diffuse.GetSrcObject( i );
                    mesh->DiffuseTexture = FileManager::ExtractFileName( fbx_file_texture->GetFileName() );
                    break;
                }
            }
        }

        // Effect
        mesh->DrawEffect.EffectFilename = nullptr;

        // Skinning
        FbxSkin* fbx_skin = (FbxSkin*) fbx_mesh->GetDeformer( 0, FbxDeformer::eSkin );
        if( fbx_skin )
        {
            // 3DS Max specific - Geometric transform
            Matrix     ms, mr, mt;
            FbxVector4 gt = fbx_node->GetGeometricTranslation( FbxNode::eSourcePivot );
            FbxVector4 gr = fbx_node->GetGeometricRotation( FbxNode::eSourcePivot );
            FbxVector4 gs = fbx_node->GetGeometricScaling( FbxNode::eSourcePivot );
            Matrix::Translation( Vector( (float) gt[ 0 ], (float) gt[ 1 ], (float) gt[ 2 ] ), mt );
            mr.FromEulerAnglesXYZ( Vector( (float) gr[ 0 ], (float) gr[ 1 ], (float) gr[ 2 ] ) );
            Matrix::Scaling( Vector( (float) gs[ 0 ], (float) gs[ 1 ], (float) gs[ 2 ] ), ms );

            // Process skin bones
            int num_bones = fbx_skin->GetClusterCount();
            mesh->SkinBoneNameHashes.resize( num_bones );
            mesh->SkinBoneOffsets.resize( num_bones );
            mesh->SkinBones.resize( num_bones );
            RUNTIME_ASSERT( num_bones <= MAX_BONES_PER_MODEL );
            for( int i = 0; i < num_bones; i++ )
            {
                FbxCluster* fbx_cluster = fbx_skin->GetCluster( i );

                // Matrices
                FbxAMatrix link_matrix;
                fbx_cluster->GetTransformLinkMatrix( link_matrix );
                FbxAMatrix cur_matrix;
                fbx_cluster->GetTransformMatrix( cur_matrix );
                Bone*      skin_bone = root_bone->Find( Bone::GetHash( fbx_cluster->GetLink()->GetName() ) );
                if( !skin_bone )
                {
                    WriteLog( "Skin bone '{}' for mesh '{}' not found.\n", fbx_cluster->GetLink()->GetName(), fbx_node->GetName() );
                    skin_bone = bone;
                }
                mesh->SkinBoneNameHashes[ i ] = skin_bone->NameHash;
                mesh->SkinBoneOffsets[ i ] = ConvertFbxMatrix( link_matrix ).Inverse() * ConvertFbxMatrix( cur_matrix ) * mt * mr * ms;
                mesh->SkinBones[ i ] = skin_bone;

                // Blend data
                float   bone_index = (float) i;
                int     num_weights = fbx_cluster->GetControlPointIndicesCount();
                int*    indices = fbx_cluster->GetControlPointIndices();
                double* weights = fbx_cluster->GetControlPointWeights();
                int     vertices_count = fbx_mesh->GetPolygonVertexCount();
                int*    vertices = fbx_mesh->GetPolygonVertices();
                for( int j = 0; j < num_weights; j++ )
                {
                    for( int k = 0; k < vertices_count; k++ )
                    {
                        if( vertices[ k ] != indices[ j ] )
                            continue;

                        Vertex3D& v = mesh->Vertices[ k ];
                        uint      index;
                        if( v.BlendIndices[ 0 ] < 0.0f )
                            index = 0;
                        else if( v.BlendIndices[ 1 ] < 0.0f )
                            index = 1;
                        else if( v.BlendIndices[ 2 ] < 0.0f )
                            index = 2;
                        else
                            index = 3;
                        v.BlendIndices[ index ] = bone_index;
                        v.BlendWeights[ index ] = (float) weights[ j ];
                    }
                }
            }
        }
        else
        {
            // 3DS Max specific - Geometric transform
            Matrix     ms, mr, mt;
            FbxVector4 gt = fbx_node->GetGeometricTranslation( FbxNode::eSourcePivot );
            FbxVector4 gr = fbx_node->GetGeometricRotation( FbxNode::eSourcePivot );
            FbxVector4 gs = fbx_node->GetGeometricScaling( FbxNode::eSourcePivot );
            Matrix::Translation( Vector( (float) gt[ 0 ], (float) gt[ 1 ], (float) gt[ 2 ] ), mt );
            mr.FromEulerAnglesXYZ( Vector( (float) gr[ 0 ], (float) gr[ 1 ], (float) gr[ 2 ] ) );
            Matrix::Scaling( Vector( (float) gs[ 0 ], (float) gs[ 1 ], (float) gs[ 2 ] ), ms );

            mesh->SkinBoneNameHashes.resize( 1 );
            mesh->SkinBoneOffsets.resize( 1 );
            mesh->SkinBones.resize( 1 );
            mesh->SkinBoneNameHashes[ 0 ] = 0;
            mesh->SkinBoneOffsets[ 0 ] = mt * mr * ms;
            mesh->SkinBones[ 0 ] = bone;
            for( size_t i = 0, j = mesh->Vertices.size(); i < j; i++ )
            {
                Vertex3D& v = mesh->Vertices[ i ];
                v.BlendIndices[ 0 ] = 0.0f;
                v.BlendWeights[ 0 ] = 1.0f;
            }
        }

        // Drop not filled indices
        for( size_t i = 0, j = mesh->Vertices.size(); i < j; i++ )
        {
            Vertex3D& v = mesh->Vertices[ i ];
            float     w = 0.0f;
            int       last_bone = 0;
            for( int b = 0; b < BONES_PER_VERTEX; b++ )
            {
                if( v.BlendIndices[ b ] < 0.0f )
                    v.BlendIndices[ b ] = v.BlendWeights[ b ] = 0.0f;
                else
                    last_bone = b;
                w += v.BlendWeights[ b ];
            }
            v.BlendWeights[ last_bone ] += 1.0f - w;
        }
    }

    for( int i = 0; i < fbx_node->GetChildCount(); i++ )
        ConvertFbxPass2( root_bone, bone->Children[ i ], fbx_node->GetChild( i ) );
}

static Matrix ConvertFbxMatrix( const FbxAMatrix& m )
{
    return Matrix( (float) m.Get( 0, 0 ), (float) m.Get( 1, 0 ), (float) m.Get( 2, 0 ), (float) m.Get( 3, 0 ),
                   (float) m.Get( 0, 1 ), (float) m.Get( 1, 1 ), (float) m.Get( 2, 1 ), (float) m.Get( 3, 1 ),
                   (float) m.Get( 0, 2 ), (float) m.Get( 1, 2 ), (float) m.Get( 2, 2 ), (float) m.Get( 3, 2 ),
                   (float) m.Get( 0, 3 ), (float) m.Get( 1, 3 ), (float) m.Get( 2, 3 ), (float) m.Get( 3, 3 ) );
}

static uchar* LoadPNG( const uchar* data, uint data_size, uint& result_width, uint& result_height )
{
    struct PNGMessage
    {
        static void Error( png_structp png_ptr, png_const_charp error_msg )
        {
            UNUSED_VARIABLE( png_ptr );
            WriteLog( "PNG error '{}'.\n", error_msg );
        }
        static void Warning( png_structp png_ptr, png_const_charp error_msg )
        {
            UNUSED_VARIABLE( png_ptr );
            // WriteLog( "PNG warning '{}'.\n", error_msg );
        }
    };

    // Setup PNG reader
    png_structp png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr );
    if( !png_ptr )
        return nullptr;

    png_set_error_fn( png_ptr, png_get_error_ptr( png_ptr ), &PNGMessage::Error, &PNGMessage::Warning );

    png_infop info_ptr = png_create_info_struct( png_ptr );
    if( !info_ptr )
    {
        png_destroy_read_struct( &png_ptr, nullptr, nullptr );
        return nullptr;
    }

    if( setjmp( png_jmpbuf( png_ptr ) ) )
    {
        png_destroy_read_struct( &png_ptr, &info_ptr, nullptr );
        return nullptr;
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
    png_set_read_fn( png_ptr, nullptr, &PNGReader::Read );
    png_read_info( png_ptr, info_ptr );

    if( setjmp( png_jmpbuf( png_ptr ) ) )
    {
        png_destroy_read_struct( &png_ptr, &info_ptr, nullptr );
        return nullptr;
    }

    // Get information
    png_uint_32 width, height;
    int         bit_depth;
    int         color_type;
    png_get_IHDR( png_ptr, info_ptr, (png_uint_32*) &width, (png_uint_32*) &height, &bit_depth, &color_type, nullptr, nullptr, nullptr );

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
        png_destroy_read_struct( &png_ptr, &info_ptr, nullptr );
        return nullptr;
    }

    // Set the individual row_pointers to point at the correct offsets
    uchar* result = new uchar[ width * height * 4 ];
    for( uint i = 0; i < height; i++ )
        row_pointers[ i ] = result + i * width * 4;

    // Read image
    png_read_image( png_ptr, row_pointers );

    // Clean up
    png_read_end( png_ptr, info_ptr );
    png_destroy_read_struct( &png_ptr, &info_ptr, (png_infopp) nullptr );
    free( row_pointers );

    // Return
    result_width = width;
    result_height = height;
    return result;
}

static uchar* LoadTGA( const uchar* data, uint data_size, uint& result_width, uint& result_height )
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
            memset( x, 0, len );                        \
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
        return nullptr;

    // Check if the image is color indexed
    if( type == 1 )
        return nullptr;

    // Check for TrueColor
    if( type != 2 && type != 10 )
        return nullptr;

    // Check for RGB(A)
    if( pixel_depth != 24 && pixel_depth != 32 )
        return nullptr;

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
                    return nullptr;
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
                    return nullptr;
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
        return nullptr;
    }

    // Copy data
    uchar* result = new uchar[ width * height * 4 ];
    for( short y = 0; y < height; y++ )
    {
        for( short x = 0; x < width; x++ )
        {
            int i = ( height - y - 1 ) * width + x;
            int j = y * width + x;
            result[ i * 4 + 0 ] = read_data[ j * bpp + 2 ];
            result[ i * 4 + 1 ] = read_data[ j * bpp + 1 ];
            result[ i * 4 + 2 ] = read_data[ j * bpp + 0 ];
            result[ i * 4 + 3 ] = ( bpp == 4 ? read_data[ j * bpp + 3 ] : 0xFF );
        }
    }
    delete[] read_data;

    // Return data
    result_width = width;
    result_height = height;
    return result;
}

bool ResourceConverter::Generate( StrVec* resource_names )
{
    // Generate resources
    bool   something_changed = false;
    StrSet update_file_names;

    for( size_t m = 0; m < GameModules.size(); m++ )
    {
        StrVec      dummy_vec;
        StrVec      all_dirs_path;
        FindDataVec all_dirs;
        FileManager::GetFolderFileNames( GameModules[ m ], true, "", dummy_vec, nullptr, &all_dirs_path, &all_dirs );
        for( size_t d = 0; d < all_dirs.size(); d++ )
        {
            if( !Str::CompareCase( all_dirs[ d ].FileName.c_str(), "Resources" ) )
                continue;

            string      resources_root = GameModules[ m ] + all_dirs_path[ d ];
            FindDataVec resources_dirs;
            FileManager::GetFolderFileNames( resources_root, false, "", dummy_vec, nullptr, nullptr, &resources_dirs );
            for( size_t r = 0; r < resources_dirs.size(); r++ )
            {
                const char*     res_name = resources_dirs[ r ].FileName.c_str();
                FilesCollection resources( "", resources_root + res_name + "/" );

                if( !Str::Substring( res_name, "_Raw" ) )
                {
                    string res_name_zip = (string) res_name + ".zip";
                    string zip_path = (string) "Update/" + res_name_zip;
                    bool   skip_making_zip = true;

                    FileManager::CreateDirectoryTree( zip_path.c_str() );

                    // Check if file available
                    FileManager zip_file;
                    if( !zip_file.LoadFile( zip_path.c_str(), true ) )
                        skip_making_zip = false;

                    // Test consistency
                    if( skip_making_zip )
                    {
                        zipFile zip = zipOpen( zip_path.c_str(), APPEND_STATUS_ADDINZIP );
                        if( !zip )
                            skip_making_zip = false;
                        else
                            zipClose( zip, nullptr );
                    }

                    // Check timestamps of inner resources
                    while( resources.IsNextFile() )
                    {
                        string       relative_path;
                        FileManager& file = resources.GetNextFile( nullptr, nullptr, &relative_path, true );
                        if( resource_names )
                            resource_names->push_back( relative_path );

                        if( skip_making_zip && file.GetWriteTime() > zip_file.GetWriteTime() )
                            skip_making_zip = false;
                    }

                    // Make zip
                    if( !skip_making_zip )
                    {
                        WriteLog( "Pack resource '{}', files {}...\n", res_name, resources.GetFilesCount() );

                        zipFile zip = zipOpen( ( zip_path + ".tmp" ).c_str(), APPEND_STATUS_CREATE );
                        if( zip )
                        {
                            resources.ResetCounter();
                            while( resources.IsNextFile() )
                            {
                                string       relative_path;
                                FileManager& file = resources.GetNextFile( nullptr, nullptr, &relative_path );
                                FileManager* converted_file = Convert( relative_path, file );
                                if( !converted_file )
                                {
                                    WriteLog( "File '{}' conversation error.\n", relative_path );
                                    continue;
                                }

                                zip_fileinfo zfi;
                                memzero( &zfi, sizeof( zfi ) );
                                if( zipOpenNewFileInZip( zip, relative_path.c_str(), &zfi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, Z_BEST_SPEED ) == ZIP_OK )
                                {
                                    if( zipWriteInFileInZip( zip, converted_file->GetOutBuf(), converted_file->GetOutBufLen() ) )
                                        WriteLog( "Can't write file '{}' in zip file '{}'.\n", relative_path, zip_path.c_str() );

                                    zipCloseFileInZip( zip );
                                }
                                else
                                {
                                    WriteLog( "Can't open file '{}' in zip file '{}'.\n", relative_path, zip_path.c_str() );
                                }

                                if( converted_file != &file )
                                    delete converted_file;
                            }
                            zipClose( zip, nullptr );

                            FileManager::DeleteFile( zip_path.c_str() );
                            if( !FileManager::RenameFile( ( zip_path + ".tmp" ).c_str(), zip_path.c_str() ) )
                                WriteLog( "Can't rename file '{}' to '{}'.\n", zip_path + ".tmp", zip_path );

                            something_changed = true;
                        }
                        else
                        {
                            WriteLog( "Can't open zip file '{}'.\n", zip_path.c_str() );
                        }
                    }

                    update_file_names.insert( res_name_zip );
                }
                else
                {
                    bool log_shown = false;
                    while( resources.IsNextFile() )
                    {
                        string       path, relative_path;
                        FileManager& file = resources.GetNextFile( nullptr, &path, &relative_path );
                        string       fname = "Update/" + relative_path;
                        FileManager  update_file;
                        if( !update_file.LoadFile( fname, true ) || file.GetWriteTime() > update_file.GetWriteTime() )
                        {
                            if( !log_shown )
                            {
                                log_shown = true;
                                WriteLog( "Copy resource '{}', files {}...\n", res_name, resources.GetFilesCount() );
                            }

                            FileManager* converted_file = Convert( fname, file );
                            if( !converted_file )
                            {
                                WriteLog( "File '{}' conversation error.\n", fname );
                                continue;
                            }
                            converted_file->SaveFile( fname );
                            if( converted_file != &file )
                                delete converted_file;

                            something_changed = true;
                        }

                        if( resource_names )
                        {
                            string ext = FileManager::GetExtension( fname );
                            if( ext == "zip" || ext == "bos" || ext == "dat" )
                            {
                                DataFile* inner = OpenDataFile( path );
                                if( inner )
                                {
                                    StrVec inner_files;
                                    inner->GetFileNames( "", true, "", inner_files );
                                    resource_names->insert( resource_names->end(), inner_files.begin(), inner_files.end() );
                                    delete inner;
                                }
                                else
                                {
                                    WriteLog( "Can't read data file '{}'.\n", path );
                                }
                            }
                        }

                        update_file_names.insert( relative_path );
                    }
                }
            }
        }
    }

    // Delete unnecessary update files
    FilesCollection update_files( "", "Update/" );
    while( update_files.IsNextFile() )
    {
        string path, relative_path;
        update_files.GetNextFile( nullptr, &path, &relative_path, true );
        if( !update_file_names.count( relative_path ) )
        {
            FileManager::DeleteFile( path );
            something_changed = true;
        }
    }

    return something_changed;
}
