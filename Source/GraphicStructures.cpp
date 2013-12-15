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
    anim->CntFrm = min( frames, MAX_FRAMES );
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
// MeshSubset
//

void MeshSubset::Save( FileManager& file )
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
    file.SetData( DiffuseColor, sizeof( DiffuseColor ) );
    file.SetData( AmbientColor, sizeof( AmbientColor ) );
    file.SetData( SpecularColor, sizeof( SpecularColor ) );
    file.SetData( EmissiveColor, sizeof( EmissiveColor ) );
    file.SetData( &BoneInfluences, sizeof( BoneInfluences ) );
    len = (uint) BoneOffsets.size();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &BoneOffsets[ 0 ], len * sizeof( BoneOffsets[ 0 ] ) );
    len = (uint) BoneNames.size();
    file.SetData( &len, sizeof( len ) );
    for( uint i = 0, j = len; i < j; i++ )
    {
        len = (uint) BoneNames[ i ].length();
        file.SetData( &len, sizeof( len ) );
        file.SetData( &BoneNames[ i ][ 0 ], len );
    }
}

void MeshSubset::Load( FileManager& file )
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
    file.CopyMem( DiffuseColor, sizeof( DiffuseColor ) );
    file.CopyMem( AmbientColor, sizeof( AmbientColor ) );
    file.CopyMem( SpecularColor, sizeof( SpecularColor ) );
    file.CopyMem( EmissiveColor, sizeof( EmissiveColor ) );
    file.CopyMem( &BoneInfluences, sizeof( BoneInfluences ) );
    file.CopyMem( &len, sizeof( len ) );
    BoneOffsets.resize( len );
    file.CopyMem( &BoneOffsets[ 0 ], len * sizeof( BoneOffsets[ 0 ] ) );
    file.CopyMem( &len, sizeof( len ) );
    BoneNames.resize( len );
    for( uint i = 0, j = len; i < j; i++ )
    {
        file.CopyMem( &len, sizeof( len ) );
        BoneNames[ i ].resize( len );
        file.CopyMem( &BoneNames[ i ][ 0 ], len );
    }
    VerticesTransformed = Vertices;
    VerticesTransformedValid = false;
    BoneCombinedMatrices.resize( BoneOffsets.size() );
    memzero( &DrawEffect, sizeof( DrawEffect ) );
    VAO = VBO = IBO = 0;
}

//
// Node
//

const char* Node::GetName()
{
    return Name.c_str();
}

Node* Node::Find( const char* name )
{
    if( Str::Compare( Name.c_str(), name ) )
        return this;
    for( uint i = 0; i < Children.size(); i++ )
    {
        Node* node = Children[ i ]->Find( name );
        if( node )
            return node;
    }
    return NULL;
}

void Node::Save( FileManager& file )
{
    uint len = (uint) Name.length();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &Name[ 0 ], len );
    file.SetData( &TransformationMatrix, sizeof( TransformationMatrix ) );
    len = (uint) Mesh.size();
    file.SetData( &len, sizeof( len ) );
    for( uint i = 0, j = len; i < j; i++ )
        Mesh[ i ].Save( file );
    len = (uint) Children.size();
    file.SetData( &len, sizeof( len ) );
    for( uint i = 0, j = len; i < j; i++ )
        Children[ i ]->Save( file );
}

void Node::Load( FileManager& file )
{
    uint len = 0;
    file.CopyMem( &len, sizeof( len ) );
    Name.resize( len );
    file.CopyMem( &Name[ 0 ], len );
    file.CopyMem( &TransformationMatrix, sizeof( TransformationMatrix ) );
    file.CopyMem( &len, sizeof( len ) );
    Mesh.resize( len );
    for( uint i = 0, j = len; i < j; i++ )
        Mesh[ i ].Load( file );
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
    for( auto it = Mesh.begin(), end = Mesh.end(); it != end; ++it )
    {
        MeshSubset& mesh = *it;
        for( uint i = 0, j = (uint) mesh.BoneNames.size(); i < j; i++ )
        {
            Node* bone_node = root_node->Find( mesh.BoneNames[ i ].c_str() );
            if( bone_node )
                mesh.BoneCombinedMatrices[ i ] = &bone_node->CombinedTransformationMatrix;
            else
                mesh.BoneCombinedMatrices[ i ] = NULL;
        }
    }

    for( uint i = 0, j = (uint) Children.size(); i < j; i++ )
        Children[ i ]->FixAfterLoad( root_node );
}
