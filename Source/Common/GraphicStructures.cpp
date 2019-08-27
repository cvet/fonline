#include "GraphicStructures.h"
#include "StringUtils.h"
#include "Exception.h"

bool Is3dExtensionSupported( const string& ext )
{
    static const string supported_formats[] =
    {
        "fo3d", "fbx", "x", "3ds", "obj", "dae", "blend", "ase", "ply", "dxf", "lwo", "lxo", "stl", "ms3d",
        "scn", "smd", "vta", "mdl", "md2", "md3", "pk3", "mdc", "md5", "bvh", "csm", "b3d", "q3d", "cob",
        "q3s", "mesh", "xml", "irrmesh", "irr", "nff", "nff", "off", "raw", "ter", "mdl", "hmp", "ndo", "ac"
    };

    for( const string& entry : supported_formats )
        if( entry == ext )
            return true;
    return false;
}

//
// AnyFrames
//

static MemoryPool< sizeof( AnyFrames ), ANY_FRAMES_POOL_SIZE > AnyFramesPool;
static AnyFrames*                                              DummyAnim;

AnyFrames* AnyFrames::Create( uint frames, uint ticks )
{
    AnyFrames* anim = (AnyFrames*) AnyFramesPool.Get();
    memzero( anim, sizeof( AnyFrames ) );
    anim->CntFrm = MIN( frames, MAX_FRAMES );
    anim->Ticks = ( ticks ? ticks : frames * 100 );
    anim->HaveDirs = false;
    return anim;
}

void AnyFrames::Destroy( AnyFrames* anim )
{
    if( !anim || anim == DummyAnim )
        return;

    for( int dir = 1; dir < anim->DirCount(); dir++ )
        AnyFramesPool.Put( anim->GetDir( dir ) );
    AnyFramesPool.Put( anim );
}

void AnyFrames::SetDummy( AnyFrames* anim )
{
    DummyAnim = anim;
}

void AnyFrames::CreateDirAnims()
{
    HaveDirs = true;
    for( int dir = 0; dir < DIRS_COUNT - 1; dir++ )
        Dirs[ dir ] = Create( CntFrm, Ticks );
}

//
// Texture
//

Texture::Texture(): Name( nullptr ), Id( 0 ), Width( 0 ), Height( 0 ), Samples( 0.0f )
{
    // Dummy comment
}

Texture::~Texture()
{
    #ifndef FO_SERVER_DAEMON
    GL( glDeleteTextures( 1, &Id ) );
    #endif
}

void Texture::UpdateRegion( const Rect& r, const uchar* data )
{
    #ifndef FO_SERVER_DAEMON
    GL( glBindTexture( GL_TEXTURE_2D, Id ) );
    GL( glTexSubImage2D( GL_TEXTURE_2D, 0, r.L, r.T, r.W(), r.H(), GL_RGBA, GL_UNSIGNED_BYTE, data ) );
    GL( glBindTexture( GL_TEXTURE_2D, 0 ) );
    #endif
}

//
// TextureAtlas
//

TextureAtlas::SpaceNode::SpaceNode( int x, int y, int w, int h )
{
    busy = false;
    posX = x;
    posY = y;
    width = w;
    height = h;
    child1 = child2 = nullptr;
}

TextureAtlas::SpaceNode::~SpaceNode()
{
    SAFEDEL( child1 );
    SAFEDEL( child2 );
}

bool TextureAtlas::SpaceNode::FindPosition( int w, int h, int& x, int& y )
{
    bool result = false;
    if( child1 )
        result = child1->FindPosition( w, h, x, y );
    if( !result && child2 )
        result = child2->FindPosition( w, h, x, y );
    if( !result && !busy && width >= w && height >= h )
    {
        result = true;
        busy = true;
        x = posX;
        y = posY;
        if( width == w && height > h )
        {
            child1 = new SpaceNode( posX, posY + h, width, height - h );
        }
        else if( height == h && width > w )
        {
            child1 = new SpaceNode( posX + w, posY, width - w, height );
        }
        else if( width > w && height > h )
        {
            child1 = new SpaceNode( posX + w, posY, width - w, h );
            child2 = new SpaceNode( posX, posY + h, width, height - h );
        }
    }
    return result;
}

TextureAtlas::TextureAtlas()
{
    memzero( this, sizeof( TextureAtlas ) );
}

TextureAtlas::~TextureAtlas()
{
    SAFEDEL( RootNode );
}

//
// MeshData
//

void MeshData::Save( File& file )
{
    uint len = (uint) Vertices.size();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &Vertices[ 0 ], len * sizeof( Vertices[ 0 ] ) );
    len = (uint) Indices.size();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &Indices[ 0 ], len * sizeof( Indices[ 0 ] ) );
    len = (uint) DiffuseTexture.length();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &DiffuseTexture[ 0 ], len );
    len = (uint) SkinBoneNameHashes.size();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &SkinBoneNameHashes[ 0 ], len * sizeof( SkinBoneNameHashes[ 0 ] ) );
    len = (uint) SkinBoneOffsets.size();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &SkinBoneOffsets[ 0 ], len * sizeof( SkinBoneOffsets[ 0 ] ) );
}

void MeshData::Load( File& file )
{
    uint len = 0;
    file.CopyMem( &len, sizeof( len ) );
    Vertices.resize( len );
    file.CopyMem( &Vertices[ 0 ], len * sizeof( Vertices[ 0 ] ) );
    file.CopyMem( &len, sizeof( len ) );
    Indices.resize( len );
    file.CopyMem( &Indices[ 0 ], len * sizeof( Indices[ 0 ] ) );
    file.CopyMem( &len, sizeof( len ) );
    DiffuseTexture.resize( len );
    file.CopyMem( &DiffuseTexture[ 0 ], len );
    file.CopyMem( &len, sizeof( len ) );
    SkinBoneNameHashes.resize( len );
    file.CopyMem( &SkinBoneNameHashes[ 0 ], len * sizeof( SkinBoneNameHashes[ 0 ] ) );
    file.CopyMem( &len, sizeof( len ) );
    SkinBoneOffsets.resize( len );
    file.CopyMem( &SkinBoneOffsets[ 0 ], len * sizeof( SkinBoneOffsets[ 0 ] ) );
    SkinBones.resize( SkinBoneOffsets.size() );
    memzero( &DrawEffect, sizeof( DrawEffect ) );
}

//
// CombinedMesh
//

void CombinedMesh::Clear()
{
    EncapsulatedMeshCount = 0;
    #ifndef FO_SERVER_DAEMON
    if( VBO )
        GL( glDeleteBuffers( 1, &VBO ) );
    if( IBO )
        GL( glDeleteBuffers( 1, &IBO ) );
    if( VAO )
        GL( glDeleteVertexArrays( 1, &VAO ) );
    #endif
    VAO = VBO = IBO = 0;
    CurBoneMatrix = 0;
    Vertices.clear();
    Indices.clear();
    Meshes.clear();
    MeshIndices.clear();
    MeshVertices.clear();
    MeshAnimLayers.clear();
}

bool CombinedMesh::CanEncapsulate( MeshInstance& mesh_instance )
{
    if( EncapsulatedMeshCount == 0 )
        return true;
    if( DrawEffect != mesh_instance.CurEffect )
        return false;
    for( int i = 0; i < EFFECT_TEXTURES; i++ )
        if( Textures[ i ] && mesh_instance.CurTexures[ i ] && Textures[ i ]->Id != mesh_instance.CurTexures[ i ]->Id )
            return false;
    if( CurBoneMatrix + mesh_instance.Mesh->SkinBones.size() > SkinBones.size() )
        return false;
    return true;
}

void CombinedMesh::Encapsulate( MeshInstance& mesh_instance, int anim_layer )
{
    MeshData* mesh = mesh_instance.Mesh;
    size_t    vertices_old_size = Vertices.size();
    size_t    indices_old_size = Indices.size();

    // Set or add data
    if( !EncapsulatedMeshCount )
    {
        Vertices = mesh->Vertices;
        Indices = mesh->Indices;
        DrawEffect = mesh_instance.CurEffect;
        memzero( &SkinBones[ 0 ], SkinBones.size() * sizeof( SkinBones[ 0 ] ) );
        memzero( Textures, sizeof( Textures ) );
        CurBoneMatrix = 0;
    }
    else
    {
        Vertices.insert( Vertices.end(), mesh->Vertices.begin(), mesh->Vertices.end() );
        Indices.insert( Indices.end(), mesh->Indices.begin(), mesh->Indices.end() );

        // Add indices offset
        ushort index_offset = (ushort) ( Vertices.size() - mesh->Vertices.size() );
        size_t start_index = Indices.size() - mesh->Indices.size();
        for( size_t i = start_index, j = Indices.size(); i < j; i++ )
            Indices[ i ] += index_offset;

        // Add bones matrices offset
        float  bone_index_offset = (float) CurBoneMatrix;
        size_t start_vertex = Vertices.size() - mesh->Vertices.size();
        for( size_t i = start_vertex, j = Vertices.size(); i < j; i++ )
            for( size_t b = 0; b < BONES_PER_VERTEX; b++ )
                Vertices[ i ].BlendIndices[ b ] += bone_index_offset;
    }

    // Set mesh transform and anim layer
    Meshes.push_back( mesh );
    MeshVertices.push_back( (uint) ( Vertices.size() - vertices_old_size ) );
    MeshIndices.push_back( (uint) ( Indices.size() - indices_old_size ) );
    MeshAnimLayers.push_back( anim_layer );

    // Add bones matrices
    for( size_t i = 0, j = mesh->SkinBones.size(); i < j; i++ )
    {
        SkinBones[ CurBoneMatrix + i ] = mesh->SkinBones[ i ];
        SkinBoneOffsets[ CurBoneMatrix + i ] = mesh->SkinBoneOffsets[ i ];
    }
    CurBoneMatrix += mesh->SkinBones.size();

    // Add textures
    for( int i = 0; i < EFFECT_TEXTURES; i++ )
        if( !Textures[ i ] && mesh_instance.CurTexures[ i ] )
            Textures[ i ] = mesh_instance.CurTexures[ i ];

    // Fix texture coords
    if( mesh_instance.CurTexures[ 0 ] )
    {
        MeshTexture* mesh_tex = mesh_instance.CurTexures[ 0 ];
        for( size_t i = vertices_old_size, j = Vertices.size(); i < j; i++ )
        {
            Vertices[ i ].TexCoord[ 0 ] = ( Vertices[ i ].TexCoord[ 0 ] * mesh_tex->AtlasOffsetData[ 2 ] ) + mesh_tex->AtlasOffsetData[ 0 ];
            Vertices[ i ].TexCoord[ 1 ] = ( Vertices[ i ].TexCoord[ 1 ] * mesh_tex->AtlasOffsetData[ 3 ] ) + mesh_tex->AtlasOffsetData[ 1 ];
        }
    }

    // Increment mesh count
    EncapsulatedMeshCount++;
}

void CombinedMesh::Finalize()
{
    #ifndef FO_SERVER_DAEMON
    GL( glGenBuffers( 1, &VBO ) );
    GL( glBindBuffer( GL_ARRAY_BUFFER, VBO ) );
    GL( glBufferData( GL_ARRAY_BUFFER, Vertices.size() * sizeof( Vertex3D ), &Vertices[ 0 ], GL_STATIC_DRAW ) );
    GL( glGenBuffers( 1, &IBO ) );
    GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, IBO ) );
    GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, Indices.size() * sizeof( short ), &Indices[ 0 ], GL_STATIC_DRAW ) );

    if( !VAO && GL_HAS( vertex_array_object ) && ( GL_HAS( framebuffer_object ) || GL_HAS( framebuffer_object_ext ) ) )
    {
        GL( glGenVertexArrays( 1, &VAO ) );
        GL( glBindVertexArray( VAO ) );
        GL( glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Position ) ) );
        GL( glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Normal ) ) );
        GL( glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoord ) ) );
        GL( glVertexAttribPointer( 3, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoordBase ) ) );
        GL( glVertexAttribPointer( 4, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Tangent ) ) );
        GL( glVertexAttribPointer( 5, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Bitangent ) ) );
        GL( glVertexAttribPointer( 6, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, BlendWeights ) ) );
        GL( glVertexAttribPointer( 7, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, BlendIndices ) ) );
        for( uint i = 0; i <= 7; i++ )
            GL( glEnableVertexAttribArray( i ) );
        GL( glBindVertexArray( 0 ) );
    }
    #endif
}

//
// Bone
//

Bone::~Bone()
{
    SAFEDEL( Mesh );
    for( auto& child : Children )
        delete child;
}

Bone* Bone::Find( uint name_hash )
{
    if( NameHash == name_hash )
        return this;
    for( uint i = 0; i < Children.size(); i++ )
    {
        Bone* bone = Children[ i ]->Find( name_hash );
        if( bone )
            return bone;
    }
    return nullptr;
}

void Bone::Save( File& file )
{
    file.SetData( &NameHash, sizeof( NameHash ) );
    file.SetData( &TransformationMatrix, sizeof( TransformationMatrix ) );
    file.SetData( &GlobalTransformationMatrix, sizeof( GlobalTransformationMatrix ) );
    file.SetUChar( Mesh != nullptr ? 1 : 0 );
    if( Mesh )
        Mesh->Save( file );
    uint len = (uint) Children.size();
    file.SetData( &len, sizeof( len ) );
    for( uint i = 0, j = len; i < j; i++ )
        Children[ i ]->Save( file );
}

void Bone::Load( File& file )
{
    file.CopyMem( &NameHash, sizeof( NameHash ) );
    file.CopyMem( &TransformationMatrix, sizeof( TransformationMatrix ) );
    file.CopyMem( &GlobalTransformationMatrix, sizeof( GlobalTransformationMatrix ) );
    if( file.GetUChar() )
    {
        Mesh = new MeshData();
        Mesh->Load( file );
        Mesh->Owner = this;
    }
    else
    {
        Mesh = nullptr;
    }
    uint len = 0;
    file.CopyMem( &len, sizeof( len ) );
    Children.resize( len );
    for( uint i = 0, j = len; i < j; i++ )
    {
        Children[ i ] = new Bone();
        Children[ i ]->Load( file );
    }
    CombinedTransformationMatrix = Matrix();
}

void Bone::FixAfterLoad( Bone* root_bone )
{
    // Fix skin
    if( Mesh )
    {
        for( size_t i = 0, j = Mesh->SkinBoneNameHashes.size(); i < j; i++ )
        {
            if( Mesh->SkinBoneNameHashes[ i ] )
                Mesh->SkinBones[ i ] = root_bone->Find( Mesh->SkinBoneNameHashes[ i ] );
            else
                Mesh->SkinBones[ i ] = Mesh->Owner;
        }
    }

    for( uint i = 0, j = (uint) Children.size(); i < j; i++ )
        Children[ i ]->FixAfterLoad( root_bone );
}

uint Bone::GetHash( const string& name )
{
    return _str( name ).toHash();
}

//
// Cut
//

CutShape CutShape::Make( MeshData* mesh )
{
    CutShape result;
    result.GlobalTransformationMatrix = mesh->Owner->GlobalTransformationMatrix;

    // Calculate sphere radius
    float vmin, vmax;
    for( size_t i = 0, j = mesh->Vertices.size(); i < j; i++ )
    {
        Vertex3D v = mesh->Vertices[ i ];
        if( !i || v.Position.x < vmin )
            vmin = v.Position.x;
        if( !i || v.Position.x > vmax )
            vmax = v.Position.x;
    }
    result.SphereRadius = ( vmax - vmin ) / 2.0f;

    return result;
}
