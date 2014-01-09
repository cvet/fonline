#include "StdAfx.h"
#include "GraphicStructures.h"

//
// AnyFrames
//

MemoryPool< sizeof( AnyFrames ), ANY_FRAMES_POOL_SIZE > AnyFramesPool;

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
    if( !anim )
        return;
    for( int dir = 1; dir < anim->DirCount(); dir++ )
        AnyFramesPool.Put( anim->GetDir( dir ) );
    AnyFramesPool.Put( anim );
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

Texture::Texture(): Name( NULL ), Id( 0 ), Data( NULL ), Size( 0 ), Width( 0 ), Height( 0 ), Samples( 0.0f )
{
    // Dummy comment
}

Texture::~Texture()
{
    GL( glDeleteTextures( 1, &Id ) );
    SAFEDELA( Data );
}

bool Texture::Update()
{
    GL( glBindTexture( GL_TEXTURE_2D, Id ) );
    GL( glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, Data ) );
    return true;
}

bool Texture::UpdateRegion( const Rect& r )
{
    GL( glBindTexture( GL_TEXTURE_2D, Id ) );
    GL( glTexSubImage2D( GL_TEXTURE_2D, 0, 0, r.T, Width, r.H(), GL_RGBA, GL_UNSIGNED_BYTE, (uint*) Data + r.T * Width ) );
    return true;
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
    child1 = child2 = NULL;
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
    SAFEDEL( TextureOwner );
    SAFEDEL( RootNode );
}

//
// MeshData
//

void MeshData::Save( FileManager& file )
{
    uint len = (uint) Vertices.size();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &Vertices[ 0 ], len * sizeof( Vertices[ 0 ] ) );
    len = (uint) Indicies.size();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &Indicies[ 0 ], len * sizeof( Indicies[ 0 ] ) );
    len = (uint) DiffuseTexture.length();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &DiffuseTexture[ 0 ], len );
    len = (uint) BoneNameHashes.size();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &BoneNameHashes[ 0 ], len * sizeof( BoneNameHashes[ 0 ] ) );
    len = (uint) BoneOffsets.size();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &BoneOffsets[ 0 ], len * sizeof( BoneOffsets[ 0 ] ) );
}

void MeshData::Load( FileManager& file )
{
    uint len = 0;
    file.CopyMem( &len, sizeof( len ) );
    Vertices.resize( len );
    file.CopyMem( &Vertices[ 0 ], len * sizeof( Vertices[ 0 ] ) );
    file.CopyMem( &len, sizeof( len ) );
    Indicies.resize( len );
    file.CopyMem( &Indicies[ 0 ], len * sizeof( Indicies[ 0 ] ) );
    file.CopyMem( &len, sizeof( len ) );
    DiffuseTexture.resize( len );
    file.CopyMem( &DiffuseTexture[ 0 ], len );
    file.CopyMem( &len, sizeof( len ) );
    BoneNameHashes.resize( len );
    file.CopyMem( &BoneNameHashes[ 0 ], len * sizeof( BoneNameHashes[ 0 ] ) );
    file.CopyMem( &len, sizeof( len ) );
    BoneOffsets.resize( len );
    file.CopyMem( &BoneOffsets[ 0 ], len * sizeof( BoneOffsets[ 0 ] ) );
    BoneCombinedMatrices.resize( BoneOffsets.size() );
    memzero( &DrawEffect, sizeof( DrawEffect ) );
}

//
// CombinedMesh
//

void CombinedMesh::Clear()
{
    EncapsulatedMeshCount = 0;
    if( VBO )
        GL( glDeleteBuffers( 1, &VBO ) );
    if( IBO )
        GL( glDeleteBuffers( 1, &IBO ) );
    if( VAO )
        GL( glDeleteVertexArrays( 1, &VAO ) );
    VAO = VBO = IBO = 0;
    CurBoneMatrix = 0;
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
    if( CurBoneMatrix + mesh_instance.Mesh->BoneCombinedMatrices.size() > MAX_BONE_MATRICES )
        return false;
    return true;
}

void CombinedMesh::Encapsulate( MeshInstance& mesh_instance )
{
    MeshData* mesh = mesh_instance.Mesh;

    // Set or add data
    if( !EncapsulatedMeshCount )
    {
        Vertices = mesh->Vertices;
        Indicies = mesh->Indicies;
        DrawEffect = mesh_instance.CurEffect;
        memzero( BoneCombinedMatrices, sizeof( BoneCombinedMatrices ) );
        memzero( Textures, sizeof( Textures ) );
        CurBoneMatrix = 0;
    }
    else
    {
        Vertices.insert( Vertices.end(), mesh->Vertices.begin(), mesh->Vertices.end() );
        Indicies.insert( Indicies.end(), mesh->Indicies.begin(), mesh->Indicies.end() );

        // Add indicies offset
        ushort index_offset = (ushort) ( Vertices.size() - mesh->Vertices.size() );
        size_t start_index = Indicies.size() - mesh->Indicies.size();
        for( size_t i = start_index, j = Indicies.size(); i < j; i++ )
            Indicies[ i ] += index_offset;

        // Add bones matrices offset
        float  bone_index_offset = (float) CurBoneMatrix;
        size_t start_vertex = Vertices.size() - mesh->Vertices.size();
        for( size_t i = start_vertex, j = Vertices.size(); i < j; i++ )
            for( size_t b = 0; b < BONES_PER_VERTEX; b++ )
                Vertices[ i ].BlendIndices[ b ] += bone_index_offset;
    }

    // Add bones matrices
    for( size_t i = 0, j = mesh->BoneCombinedMatrices.size(); i < j; i++ )
    {
        BoneCombinedMatrices[ CurBoneMatrix + i ] = mesh->BoneCombinedMatrices[ i ];
        BoneOffsetMatrices[ CurBoneMatrix + i ] = mesh->BoneOffsets[ i ];
    }
    CurBoneMatrix += mesh->BoneCombinedMatrices.size();

    // Add textures
    for( int i = 0; i < EFFECT_TEXTURES; i++ )
        if( !Textures[ i ] && mesh_instance.CurTexures[ i ] )
            Textures[ i ] = mesh_instance.CurTexures[ i ];

    // Increment mesh count
    EncapsulatedMeshCount++;
}

void CombinedMesh::Finalize()
{
    GL( glGenBuffers( 1, &VBO ) );
    GL( glBindBuffer( GL_ARRAY_BUFFER, VBO ) );
    GL( glBufferData( GL_ARRAY_BUFFER, Vertices.size() * sizeof( Vertex3D ), &Vertices[ 0 ], GL_STATIC_DRAW ) );
    GL( glGenBuffers( 1, &IBO ) );
    GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, IBO ) );
    GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, Indicies.size() * sizeof( short ), &Indicies[ 0 ], GL_STATIC_DRAW ) );

    if( !VAO && GL_HAS( vertex_array_object ) && ( GL_HAS( framebuffer_object ) || GL_HAS( framebuffer_object_ext ) ) )
    {
        GL( glGenVertexArrays( 1, &VAO ) );
        GL( glBindVertexArray( VAO ) );
        GL( glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Position ) ) );
        GL( glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Normal ) ) );
        GL( glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoordAtlas ) ) );
        GL( glVertexAttribPointer( 3, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoordBase ) ) );
        GL( glVertexAttribPointer( 4, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Tangent ) ) );
        GL( glVertexAttribPointer( 5, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Bitangent ) ) );
        GL( glVertexAttribPointer( 6, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, BlendWeights ) ) );
        GL( glVertexAttribPointer( 7, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, BlendIndices ) ) );
        for( uint i = 0; i <= 7; i++ )
            GL( glEnableVertexAttribArray( i ) );
        GL( glBindVertexArray( 0 ) );
    }
}

//
// Node
//

Node* Node::Find( uint name_hash )
{
    if( NameHash == name_hash )
        return this;
    for( uint i = 0; i < Children.size(); i++ )
    {
        Node* node = Children[ i ]->Find( name_hash );
        if( node )
            return node;
    }
    return NULL;
}

void Node::Save( FileManager& file )
{
    file.SetData( &NameHash, sizeof( NameHash ) );
    file.SetData( &TransformationMatrix, sizeof( TransformationMatrix ) );
    file.SetUChar( Mesh != NULL ? 1 : 0 );
    if( Mesh )
        Mesh->Save( file );
    uint len = (uint) Children.size();
    file.SetData( &len, sizeof( len ) );
    for( uint i = 0, j = len; i < j; i++ )
        Children[ i ]->Save( file );
}

void Node::Load( FileManager& file )
{
    file.CopyMem( &NameHash, sizeof( NameHash ) );
    file.CopyMem( &TransformationMatrix, sizeof( TransformationMatrix ) );
    if( file.GetUChar() )
    {
        Mesh = new MeshData();
        Mesh->Load( file );
        Mesh->Parent = this;
    }
    else
    {
        Mesh = NULL;
    }
    uint len = 0;
    file.CopyMem( &len, sizeof( len ) );
    Children.resize( len );
    for( uint i = 0, j = len; i < j; i++ )
    {
        Children[ i ] = new Node();
        Children[ i ]->Load( file );
    }
    CombinedTransformationMatrix = Matrix();
}

void Node::FixAfterLoad( Node* root_node )
{
    // Fix skin
    if( Mesh )
    {
        for( size_t i = 0, j = Mesh->BoneNameHashes.size(); i < j; i++ )
        {
            if( Mesh->BoneNameHashes[ i ] )
            {
                Node* bone_node = root_node->Find( Mesh->BoneNameHashes[ i ] );
                Mesh->BoneCombinedMatrices[ i ] = &bone_node->CombinedTransformationMatrix;
            }
            else
            {
                Mesh->BoneCombinedMatrices[ i ] = &Mesh->Parent->CombinedTransformationMatrix;
            }
        }
    }

    for( uint i = 0, j = (uint) Children.size(); i < j; i++ )
        Children[ i ]->FixAfterLoad( root_node );
}

uint Node::GetHash( const char* name )
{
    uint hash = 2166136261U;
    for( uint i = 0, j = Str::Length( name ); i < j; i++ )
        hash = 16777619U * hash ^ (uint) name[ i ];
    return hash;
}
