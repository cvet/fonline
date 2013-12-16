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
    file.SetData( &DiffuseColor, sizeof( DiffuseColor ) );
    file.SetData( &AmbientColor, sizeof( AmbientColor ) );
    len = (uint) SkinBoneNames.size();
    file.SetData( &len, sizeof( len ) );
    for( uint i = 0, j = len; i < j; i++ )
    {
        len = (uint) SkinBoneNames[ i ].length();
        file.SetData( &len, sizeof( len ) );
        file.SetData( &SkinBoneNames[ i ][ 0 ], len );
    }
    len = (uint) SkinBoneOffsets.size();
    file.SetData( &len, sizeof( len ) );
    file.SetData( &SkinBoneOffsets[ 0 ], len * sizeof( SkinBoneOffsets[ 0 ] ) );
    file.SetData( SkinBoneIndicies, sizeof( SkinBoneIndicies ) );
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
    file.CopyMem( &DiffuseColor, sizeof( DiffuseColor ) );
    file.CopyMem( &AmbientColor, sizeof( AmbientColor ) );
    file.CopyMem( &len, sizeof( len ) );
    SkinBoneNames.resize( len );
    for( uint i = 0, j = len; i < j; i++ )
    {
        file.CopyMem( &len, sizeof( len ) );
        SkinBoneNames[ i ].resize( len );
        file.CopyMem( &SkinBoneNames[ i ][ 0 ], len );
    }
    file.CopyMem( &len, sizeof( len ) );
    SkinBoneOffsets.resize( len );
    file.CopyMem( &SkinBoneOffsets[ 0 ], len * sizeof( SkinBoneOffsets[ 0 ] ) );
    file.CopyMem( SkinBoneIndicies, sizeof( SkinBoneIndicies ) );
    VerticesTransformed = Vertices;
    VerticesTransformedValid = false;
    memzero( BoneCombinedMatrices, sizeof( BoneCombinedMatrices ) );
    memzero( &DrawEffect, sizeof( DrawEffect ) );
    VAO = VBO = IBO = 0;
}

//
// Node
//

StrVec Node::Bones;

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
        MeshSubset& ms = *it;

        // Fix skin
        if( ms.IsSkinned() )
        {
            int    bone_indicies[ MAX_BONE_MATRICES ];
            memcpy( bone_indicies, ms.SkinBoneIndicies, sizeof( bone_indicies ) );
            IntVec new_bone_indicies( ms.SkinBoneNames.size() );
            for( size_t i = 0, j = ms.SkinBoneNames.size(); i < j; i++ )
            {
                Node* bone_node = root_node->Find( ms.SkinBoneNames[ i ].c_str() );
                int   bone_index = bone_node->GetBoneIndex();
                ms.BoneCombinedMatrices[ bone_index ] = &bone_node->CombinedTransformationMatrix;
                ms.BoneOffsetMatrices[ bone_index ] = ms.SkinBoneOffsets[ i ];
                ms.SkinBoneIndicies[ bone_index ] = i;
                new_bone_indicies[ i ] = bone_index;
            }

            // Fix blend matrix indicies
            for( size_t i = 0, j = ms.Vertices.size(); i < j; i++ )
            {
                Vertex3D& v = ms.Vertices[ i ];
                for( int b = 0; b < BONES_PER_VERTEX; b++ )
                {
                    if( v.BlendWeights[ b ] > 0.0f )
                        v.BlendIndices[ b ] = (float) new_bone_indicies[ bone_indicies[ (int) v.BlendIndices[ b ] ] ];
                }
            }
        }
    }

    for( uint i = 0, j = (uint) Children.size(); i < j; i++ )
        Children[ i ]->FixAfterLoad( root_node );
}

int Node::GetBoneIndex()
{
    for( int i = 0, j = (int) Bones.size(); i < j; i++ )
        if( Bones[ i ] == Name )
            return i;
    Bones.push_back( Name );
    return (int) Bones.size() - 1;
}
