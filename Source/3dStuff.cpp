#include "StdAfx.h"
#include "3dStuff.h"
#include "3dAnimation.h"
#include "GraphicLoader.h"
#include "Common.h"
#include "Text.h"
#include "ConstantsManager.h"
#include "Script.h"
#include "CritterType.h"

int    ModeWidth = 0, ModeHeight = 0;
float  ModeWidthF = 0, ModeHeightF = 0;
Matrix MatrixProjRM, MatrixEmptyRM, MatrixProjCM, MatrixEmptyCM;    // Row or Column major order
float  MoveTransitionTime = 0.25f;
float  GlobalSpeedAdjust = 1.0f;
bool   SoftwareSkinning = false;
uint   AnimDelay = 0;
Color  LightColor;
Matrix WorldMatrices[ MAX_BONE_MATRICES ];
float  CurZ = 0.0f;

void VecProject( const Vector& v, Vector& out )
{
    int   viewport[ 4 ] = { 0, 0, ModeWidth, ModeHeight };
    float x = 0.0f, y = 0.0f, z = 0.0f;
    gluStuffProject( v.x, v.y, v.z, MatrixEmptyCM[ 0 ], MatrixProjCM[ 0 ], viewport, &x, &y, &z );
    out.x = x;
    out.y = y;
    out.z = z;
}

void VecUnproject( const Vector& v, Vector& out )
{
    int   viewport[ 4 ] = { 0, 0, ModeWidth, ModeHeight };
    float x = 0.0f, y = 0.0f, z = 0.0f;
    gluStuffUnProject( v.x, (float) ModeHeight - v.y, v.z, MatrixEmptyCM[ 0 ], MatrixProjCM[ 0 ], viewport, &x, &y, &z );
    out.x = x;
    out.y = y;
    out.z = z;
}

void ProjectPosition( Vector& v )
{
    v *= MatrixProjRM;
    v.x = ( ( v.x - 1.0f ) * 0.5f + 1.0f ) * ModeWidthF;
    v.y = ( ( 1.0f - v.y ) * 0.5f ) * ModeHeightF;
}

/************************************************************************/
/* Animation3d                                                          */
/************************************************************************/

Animation3dVec Animation3d::loadedAnimations;

Animation3d::Animation3d(): animEntity( NULL ), animController( NULL ), combinedMeshesSize( 0 ),
                            currentTrack( 0 ), lastTick( 0 ), endTick( 0 ), speedAdjustBase( 1.0f ), speedAdjustCur( 1.0f ), speedAdjustLink( 1.0f ),
                            shadowDisabled( false ), dirAngle( GameOpt.MapHexagonal ? 150.0f : 135.0f ), sprId( 0 ),
                            drawScale( 0.0f ), noDraw( true ), parentAnimation( NULL ), parentNode( NULL ),
                            childChecker( true ), useGameTimer( true ), animPosProc( 0.0f ), animPosTime( 0.0f ), animPosPeriod( 0.0f ),
                            allowMeshGeneration( false )
{
    memzero( currentLayers, sizeof( currentLayers ) );
    Matrix::RotationX( GameOpt.MapCameraAngle * PI_VALUE / 180.0f, matRot );
    matScale = Matrix();
    matScaleBase = Matrix();
    matRotBase = Matrix();
    matTransBase = Matrix();
}

Animation3d::~Animation3d()
{
    delete animController;
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
        delete *it;
    childAnimations.clear();

    auto it = std::find( loadedAnimations.begin(), loadedAnimations.end(), this );
    if( it != loadedAnimations.end() )
        loadedAnimations.erase( it );

    allMeshes.clear();

    for( auto it = combinedMeshes.begin(), end = combinedMeshes.end(); it != end; ++it )
        delete *it;
    combinedMeshes.clear();
}

void Animation3d::StartMeshGeneration()
{
    if( !allowMeshGeneration )
    {
        allowMeshGeneration = true;
        GenerateCombinedMeshes();
    }
}

bool Animation3d::SetAnimation( uint anim1, uint anim2, int* layers, int flags )
{
    // Get animation index
    float speed = 1.0f;
    int   index = 0;
    float period_proc = 0.0f;
    if( !FLAG( flags, ANIMATION_INIT ) )
    {
        if( !anim1 )
        {
            index = animEntity->renderAnim;
            period_proc = (float) anim2 / 10.0f;
        }
        else
        {
            index = animEntity->GetAnimationIndex( anim1, anim2, &speed );
        }
    }

    if( FLAG( flags, ANIMATION_PERIOD( 0 ) ) )
        period_proc = (float) ( flags >> 16 );
    period_proc = CLAMP( period_proc, 0.0f, 99.9f );

    // Check animation changes
    bool layer_changed = false;
    if( !layers )
        layers = currentLayers;
    if( !FLAG( flags, ANIMATION_INIT ) )
    {
        for( int i = 0; i < LAYERS3D_COUNT; i++ )
        {
            if( currentLayers[ i ] != layers[ i ] )
            {
                layer_changed = true;
                break;
            }
        }
    }
    else
    {
        layer_changed = true;
    }

    // Is not one time play and same action
    int action = ( anim1 << 16 ) | anim2;
    if( !FLAG( flags, ANIMATION_INIT | ANIMATION_ONE_TIME ) && currentLayers[ LAYERS3D_COUNT ] == action && !layer_changed )
        return false;

    memcpy( currentLayers, layers, sizeof( int ) * LAYERS3D_COUNT );
    currentLayers[ LAYERS3D_COUNT ] = action;

    bool mesh_changed = false;

    if( layer_changed )
    {
        // Store disabled meshes
        for( size_t i = 0, j = allMeshes.size(); i < j; i++ )
            allMeshesDisabled[ i ] = allMeshes[ i ].Disabled;

        // Set base data
        SetAnimData( animEntity->animDataDefault, true );

        // Append linked data
        if( parentNode )
            SetAnimData( animLink, false );

        // Mark animations as unused
        for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
            ( *it )->childChecker = false;

        // Get unused layers and meshes
        bool unused_layers[ LAYERS3D_COUNT ] = { 0 };
        for( int i = 0; i < LAYERS3D_COUNT; i++ )
        {
            if( layers[ i ] == 0 )
                continue;

            for( auto it_ = animEntity->animData.begin(), end_ = animEntity->animData.end(); it_ != end_; ++it_ )
            {
                AnimParams& link = *it_;
                if( link.Layer == i && link.LayerValue == layers[ i ] && !link.ChildFName )
                {
                    for( uint j = 0; j < link.DisabledLayersCount; j++ )
                    {
                        unused_layers[ link.DisabledLayers[ j ] ] = true;
                    }
                    for( uint j = 0; j < link.DisabledMeshCount; j++ )
                    {
                        uint disabled_mesh_name_hash = link.DisabledMesh[ j ];
                        for( auto it = allMeshes.begin(), end = allMeshes.end(); it != end; ++it )
                            if( !disabled_mesh_name_hash || disabled_mesh_name_hash == ( *it ).Mesh->Parent->NameHash )
                                ( *it ).Disabled = true;
                    }
                }
            }
        }

        if( parentNode )
        {
            for( uint j = 0; j < animLink.DisabledLayersCount; j++ )
            {
                unused_layers[ animLink.DisabledLayers[ j ] ] = true;
            }
            for( uint j = 0; j < animLink.DisabledMeshCount; j++ )
            {
                uint disabled_mesh_name_hash = animLink.DisabledMesh[ j ];
                for( auto it = allMeshes.begin(), end_ = allMeshes.end(); it != end_; ++it )
                    if( !disabled_mesh_name_hash || disabled_mesh_name_hash == ( *it ).Mesh->Parent->NameHash )
                        ( *it ).Disabled = true;
            }
        }

        // Append animations
        for( int i = 0; i < LAYERS3D_COUNT; i++ )
        {
            if( unused_layers[ i ] || layers[ i ] == 0 )
                continue;

            for( auto it = animEntity->animData.begin(), end = animEntity->animData.end(); it != end; ++it )
            {
                AnimParams& link = *it;
                if( link.Layer == i && link.LayerValue == layers[ i ] )
                {
                    if( !link.ChildFName )
                    {
                        SetAnimData( link, false );
                        continue;
                    }

                    bool aviable = false;
                    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
                    {
                        Animation3d* anim3d = *it;
                        if( anim3d->animLink.Id == link.Id )
                        {
                            anim3d->childChecker = true;
                            aviable = true;
                            break;
                        }
                    }

                    if( !aviable )
                    {
                        Animation3d* anim3d = NULL;

                        // Link to main node
                        if( link.LinkBoneHash )
                        {
                            Node* to_node = animEntity->xFile->rootNode->Find( link.LinkBoneHash );
                            if( to_node )
                            {
                                anim3d = Animation3d::GetAnimation( link.ChildFName, true );
                                if( anim3d )
                                {
                                    mesh_changed = true;
                                    anim3d->parentAnimation = this;
                                    anim3d->parentNode = to_node;
                                    anim3d->animLink = link;
                                    anim3d->SetAnimData( link, false );
                                    childAnimations.push_back( anim3d );
                                }
                            }
                        }
                        // Link all bones
                        else
                        {
                            anim3d = Animation3d::GetAnimation( link.ChildFName, true );
                            if( anim3d )
                            {
                                for( auto it = anim3d->animEntity->xFile->allNodes.begin(), end = anim3d->animEntity->xFile->allNodes.end(); it != end; ++it )
                                {
                                    Node* child_node = *it;
                                    Node* root_node = animEntity->xFile->rootNode->Find( child_node->NameHash );
                                    if( root_node )
                                    {
                                        anim3d->linkNodes.push_back( root_node );
                                        anim3d->linkNodes.push_back( child_node );
                                    }
                                }

                                mesh_changed = true;
                                anim3d->parentAnimation = this;
                                anim3d->parentNode = animEntity->xFile->rootNode;
                                anim3d->animLink = link;
                                anim3d->SetAnimData( link, false );
                                childAnimations.push_back( anim3d );
                            }
                        }
                    }
                }
            }
        }

        // Erase unused animations
        for( auto it = childAnimations.begin(); it != childAnimations.end();)
        {
            Animation3d* anim3d = *it;
            if( !anim3d->childChecker )
            {
                mesh_changed = true;
                delete anim3d;
                it = childAnimations.erase( it );
            }
            else
                ++it;
        }

        // Compare disabled meshes
        if( !mesh_changed )
        {
            for( size_t i = 0, j = allMeshes.size(); i < j && !mesh_changed; i++ )
                mesh_changed = ( allMeshesDisabled[ i ] != allMeshes[ i ].Disabled );
        }
    }

    if( animController && index >= 0 )
    {
        // Get the animation set from the controller
        AnimSet* set = animController->GetAnimationSet( index );

        // Alternate tracks
        uint  new_track = ( currentTrack == 0 ? 1 : 0 );
        float period = set->GetDuration();
        animPosPeriod = period;
        if( FLAG( flags, ANIMATION_INIT ) )
            period = 0.0002f;

        // Assign to our track
        animController->SetTrackAnimationSet( new_track, set );

        // Prepare to new tracking
        animController->Reset();

        // Smooth time
        float smooth_time = ( FLAG( flags, ANIMATION_NO_SMOOTH | ANIMATION_STAY | ANIMATION_INIT ) ? 0.0001f : MoveTransitionTime );
        float start_time = period * period_proc / 100.0f;
        if( FLAG( flags, ANIMATION_STAY ) )
            period = start_time + 0.0002f;

        // Add an event key to disable the currently playing track smooth_time seconds in the future
        animController->AddEventEnable( currentTrack, false, smooth_time );
        // Add an event key to change the speed right away so the animation completes in smooth_time seconds
        animController->AddEventSpeed( currentTrack, 0.0f, 0.0f, smooth_time );
        // Add an event to change the weighting of the current track (the effect it has blended with the second track)
        animController->AddEventWeight( currentTrack, 0.0f, 0.0f, smooth_time );

        // Enable the new track
        animController->SetTrackEnable( new_track, true );
        animController->SetTrackPosition( new_track, 0.0f );
        // Add an event key to set the speed of the track
        animController->AddEventSpeed( new_track, 1.0f, 0.0f, smooth_time );
        if( FLAG( flags, ANIMATION_ONE_TIME | ANIMATION_STAY | ANIMATION_INIT ) )
            animController->AddEventSpeed( new_track, 0.0f, period - 0.0001f, 0.0 );
        // Add an event to change the weighting of the current track (the effect it has blended with the first track)
        // As you can see this will go from 0 effect to total effect(1.0f) in smooth_time seconds and the first track goes from
        // total to 0.0f in the same time.
        animController->AddEventWeight( new_track, 1.0f, 0.0f, smooth_time );

        if( start_time != 0.0 )
            animController->AdvanceTime( start_time );

        // Remember current track
        currentTrack = new_track;

        // Speed
        speedAdjustCur = speed;

        // End time
        uint tick = GetTick();
        if( FLAG( flags, ANIMATION_ONE_TIME ) )
            endTick = tick + uint( period / GetSpeed() * 1000.0f );
        else
            endTick = 0;

        // FPS imitation
        if( AnimDelay )
        {
            lastTick = tick;
            animController->AdvanceTime( 0.0f );
        }
    }

    // Set animation for children
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
    {
        Animation3d* child = *it;
        if( child->SetAnimation( anim1, anim2, layers, flags ) )
            mesh_changed = true;
    }

    // Regenerate mesh for drawing
    if( !parentNode && mesh_changed )
        GenerateCombinedMeshes();

    return mesh_changed;
}

bool Animation3d::IsAnimation( uint anim1, uint anim2 )
{
    int  ii = ( anim1 << 8 ) | anim2;
    auto it = animEntity->animIndexes.find( ii );
    return it != animEntity->animIndexes.end();
}

bool Animation3d::CheckAnimation( uint& anim1, uint& anim2 )
{
    if( animEntity->GetAnimationIndex( anim1, anim2, NULL ) == -1 )
    {
        anim1 = ANIM1_UNARMED;
        anim2 = ANIM2_IDLE;
        return false;
    }
    return true;
}

int Animation3d::GetAnim1()
{
    return currentLayers[ LAYERS3D_COUNT ] >> 16;
}

int Animation3d::GetAnim2()
{
    return currentLayers[ LAYERS3D_COUNT ] & 0xFFFF;
}

bool Animation3d::IsAnimationPlaying()
{
    return GetTick() < endTick;
}

bool Animation3d::IsIntersect( int x, int y )
{
    // Mesh not draw at last frame
    if( noDraw )
        return false;

    // Check dirty region
    Rect borders = GetBonesBorder( true );
    if( x < borders.L || x > borders.R || y < borders.T || y > borders.B )
        return false;

    // Move animation
    ProcessAnimation( 0.0f, drawXY.X, drawXY.Y, drawScale );

    // Check precisely
    for( size_t m = 0; m < combinedMeshesSize; m++ )
    {
        CombinedMesh* combined_mesh = combinedMeshes[ m ];
        TransformMesh( combined_mesh );
        for( size_t i = 0, j = combined_mesh->Indicies.size(); i < j; i += 3 )
        {
            Vector& c1 = combined_mesh->VerticesTransformed[ combined_mesh->Indicies[ i + 0 ] ];
            Vector& c2 = combined_mesh->VerticesTransformed[ combined_mesh->Indicies[ i + 1 ] ];
            Vector& c3 = combined_mesh->VerticesTransformed[ combined_mesh->Indicies[ i + 2 ] ];
            #define SQUARE( x1, y1, x2, y2, x3, y3 )    fabs( x2 * y3 - x3 * y2 - x1 * y3 + x3 * y1 + x1 * y2 - x2 * y1 )
            float   s = 0.0f;
            s += SQUARE( x, y, c2.x, c2.y, c3.x, c3.y );
            s += SQUARE( c1.x, c1.y, x, y, c3.x, c3.y );
            s += SQUARE( c1.x, c1.y, c2.x, c2.y, x, y );
            if( s <= SQUARE( c1.x, c1.y, c2.x, c2.y, c3.x, c3.y ) )
                return true;
            #undef SQUARE
        }
    }

    return false;
}

Point Animation3d::GetGroundPos()
{
    if( noDraw )
        return Point();
    Vector pos = groundPos;
    ProjectPosition( pos );
    return PointF( pos.x, pos.y );
}

Rect Animation3d::GetBonesBorder( bool add_offsets /* = false */ )
{
    if( noDraw )
        return Rect();
    if( add_offsets )
        return RectF( bonesBorder.L - BORDERS_OFFSET, bonesBorder.T - BORDERS_OFFSET, bonesBorder.R + BORDERS_OFFSET, bonesBorder.B + BORDERS_OFFSET );
    return bonesBorder;
}

Point Animation3d::GetBonesBorderPivot()
{
    if( noDraw )
        return Point();
    return Point( drawXY.X - (int) bonesBorder.L, drawXY.Y - (int) bonesBorder.T );
}

void Animation3d::GetRenderFramesData( float& period, int& proc_from, int& proc_to )
{
    period = 0.0f;
    proc_from = animEntity->renderAnimProcFrom;
    proc_to = animEntity->renderAnimProcTo;

    if( animController )
    {
        AnimSet* set = animController->GetAnimationSet( animEntity->renderAnim );
        if( set )
            period = set->GetDuration();
    }
}

float Animation3d::GetSpeed()
{
    return speedAdjustCur * speedAdjustBase * speedAdjustLink * GlobalSpeedAdjust;
}

uint Animation3d::GetTick()
{
    if( useGameTimer )
        return Timer::GameTick();
    return Timer::FastTick();
}

void Animation3d::SetAnimData( AnimParams& data, bool clear )
{
    // Transformations
    if( clear )
    {
        matScaleBase = Matrix();
        matRotBase = Matrix();
        matTransBase = Matrix();
    }
    Matrix mat_tmp;
    if( data.ScaleX != 0.0f )
        matScaleBase = matScaleBase * Matrix::Scaling( Vector( data.ScaleX, 1.0f, 1.0f ), mat_tmp );
    if( data.ScaleY != 0.0f )
        matScaleBase = matScaleBase * Matrix::Scaling( Vector( 1.0f, data.ScaleY, 1.0f ), mat_tmp );
    if( data.ScaleZ != 0.0f )
        matScaleBase = matScaleBase * Matrix::Scaling( Vector( 1.0f, 1.0f, data.ScaleZ ), mat_tmp );
    if( data.RotX != 0.0f )
        matRotBase = matRotBase * Matrix::RotationX( -data.RotX * PI_VALUE / 180.0f, mat_tmp );
    if( data.RotY != 0.0f )
        matRotBase = matRotBase * Matrix::RotationY( data.RotY * PI_VALUE / 180.0f, mat_tmp );
    if( data.RotZ != 0.0f )
        matRotBase = matRotBase * Matrix::RotationZ( data.RotZ * PI_VALUE / 180.0f, mat_tmp );
    if( data.MoveX != 0.0f )
        matTransBase = matTransBase * Matrix::Translation( Vector( data.MoveX, 0.0f, 0.0f ), mat_tmp );
    if( data.MoveY != 0.0f )
        matTransBase = matTransBase * Matrix::Translation( Vector( 0.0f, data.MoveY, 0.0f ), mat_tmp );
    if( data.MoveZ != 0.0f )
        matTransBase = matTransBase * Matrix::Translation( Vector( 0.0f, 0.0f, -data.MoveZ ), mat_tmp );

    // Speed
    if( clear )
        speedAdjustLink = 1.0f;
    if( data.SpeedAjust != 0.0f )
        speedAdjustLink *= data.SpeedAjust;

    // Textures
    if( clear )
    {
        // Enable all meshes, set default texture
        for( auto it = allMeshes.begin(), end = allMeshes.end(); it != end; ++it )
        {
            MeshInstance& mesh_instance = *it;
            mesh_instance.Disabled = false;
            memcpy( mesh_instance.CurTexures, mesh_instance.DefaultTexures, sizeof( mesh_instance.CurTexures ) );
        }
    }

    if( data.TextureCount )
    {
        for( uint i = 0; i < data.TextureCount; i++ )
        {
            MeshTexture* texture = NULL;

            // Get texture
            if( Str::CompareCaseCount( data.TextureName[ i ], "Parent", 6 ) )     // Parent_MeshName
            {
                if( parentAnimation )
                {
                    const char* mesh_name = data.TextureName[ i ] + 6;
                    if( *mesh_name && *mesh_name == '_' )
                        mesh_name++;
                    uint mesh_name_hash = ( *mesh_name ? Node::GetHash( mesh_name ) : 0 );
                    for( auto it = parentAnimation->allMeshes.begin(), end = parentAnimation->allMeshes.end(); it != end && !texture; ++it )
                        if( !mesh_name_hash || mesh_name_hash == ( *it ).Mesh->Parent->NameHash )
                            texture = ( *it ).CurTexures[ data.TextureNum[ i ] ];
                }
            }
            else
            {
                texture = animEntity->xFile->GetTexture( data.TextureName[ i ] );
            }

            // Assign it
            int texture_num = data.TextureNum[ i ];
            uint mesh_name_hash = data.TextureMesh[ i ];
            for( auto it = allMeshes.begin(), end = allMeshes.end(); it != end; ++it )
                if( !mesh_name_hash || mesh_name_hash == ( *it ).Mesh->Parent->NameHash )
                    ( *it ).CurTexures[ texture_num ] = texture;
        }
    }

    // Effects
    if( clear )
    {
        for( auto it = allMeshes.begin(), end = allMeshes.end(); it != end; ++it )
        {
            MeshInstance& mesh_instance = *it;
            mesh_instance.CurEffect = mesh_instance.DefaultEffect;
        }
    }

    if( data.EffectCount )
    {
        for( uint i = 0; i < data.EffectCount; i++ )
        {
            Effect* effect = NULL;

            // Get effect
            if( Str::CompareCaseCount( data.EffectInst[ i ].EffectFilename, "Parent", 6 ) )     // Parent_MeshName
            {
                if( parentAnimation )
                {
                    const char* mesh_name = data.EffectInst[ i ].EffectFilename + 6;
                    if( *mesh_name && *mesh_name == '_' )
                        mesh_name++;
                    uint mesh_name_hash = ( *mesh_name ? Node::GetHash( mesh_name ) : 0 );
                    for( auto it = parentAnimation->allMeshes.begin(), end = parentAnimation->allMeshes.end(); it != end && !effect; ++it )
                        if( !mesh_name_hash || mesh_name_hash == ( *it ).Mesh->Parent->NameHash )
                            effect = ( *it ).CurEffect;
                }
            }
            else
            {
                effect = animEntity->xFile->GetEffect( &data.EffectInst[ i ] );
            }

            // Assign it
            uint mesh_name_hash = data.EffectMesh[ i ];
            for( auto it = allMeshes.begin(), end = allMeshes.end(); it != end; ++it )
                if( !mesh_name_hash || mesh_name_hash == ( *it ).Mesh->Parent->NameHash )
                    ( *it ).CurEffect = effect;
        }
    }
}

void Animation3d::SetDir( int dir )
{
    float angle = (float) ( GameOpt.MapHexagonal ? 150 - dir * 60 : 135 - dir * 45 );
    if( angle != dirAngle )
        dirAngle = angle;
}

void Animation3d::SetDirAngle( int dir_angle )
{
    float angle = (float) dir_angle;
    if( angle != dirAngle )
        dirAngle = angle;
}

void Animation3d::SetRotation( float rx, float ry, float rz )
{
    Matrix my, mx, mz;
    Matrix::RotationY( ry, my );
    Matrix::RotationX( rx, mx );
    Matrix::RotationZ( rz, mz );
    matRot = my * mx * mz;
}

void Animation3d::SetScale( float sx, float sy, float sz )
{
    Matrix::Scaling( Vector( sx, sy, sz ), matScale );
}

void Animation3d::SetSpeed( float speed )
{
    speedAdjustBase = speed;
}

void Animation3d::SetTimer( bool use_game_timer )
{
    useGameTimer = use_game_timer;
}

void Animation3d::GenerateCombinedMeshes()
{
    // Generation disabled
    if( !allowMeshGeneration )
        return;

    // Clean up buffers
    for( size_t i = 0, j = combinedMeshesSize; i < j; i++ )
        combinedMeshes[ i ]->Clear();
    combinedMeshesSize = 0;

    // Combine meshes recursively
    FillCombinedMeshes( this, this );

    // Finalize meshes
    for( size_t i = 0, j = combinedMeshesSize; i < j; i++ )
        combinedMeshes[ i ]->Finalize();
}

void Animation3d::FillCombinedMeshes( Animation3d* base, Animation3d* cur )
{
    // Combine meshes
    for( size_t i = 0, j = cur->allMeshes.size(); i < j; i++ )
        base->CombineMesh( cur->allMeshes[ i ] );

    // Fill child
    for( auto it = cur->childAnimations.begin(), end = cur->childAnimations.end(); it != end; ++it )
        FillCombinedMeshes( base, *it );
}

void Animation3d::CombineMesh( MeshInstance& mesh_instance )
{
    // Skip disabled meshes
    if( mesh_instance.Disabled )
        return;

    // Try encapsulate mesh instance to current combined mesh
    for( size_t i = 0, j = combinedMeshesSize; i < j; i++ )
    {
        if( combinedMeshes[ i ]->CanEncapsulate( mesh_instance ) )
        {
            combinedMeshes[ i ]->Encapsulate( mesh_instance );
            return;
        }
    }

    // Create new combined mesh
    if( combinedMeshesSize >= combinedMeshes.size() )
        combinedMeshes.push_back( new CombinedMesh() );
    combinedMeshes[ combinedMeshesSize ]->Encapsulate( mesh_instance );
    combinedMeshesSize++;
}

void Animation3d::Draw( int x, int y, float scale, uint color )
{
    // Skip drawing if no meshes generated
    if( !combinedMeshesSize )
        return;

    // Increment Z
    CurZ += DRAW_Z_STEP;

    // Lighting
    LightColor.r = (float) ( (uchar*) &color )[ 0 ] / 255.0f;
    LightColor.g = (float) ( (uchar*) &color )[ 1 ] / 255.0f;
    LightColor.b = (float) ( (uchar*) &color )[ 2 ] / 255.0f;
    LightColor.a = (float) ( (uchar*) &color )[ 3 ] / 255.0f;

    // Move timer
    float elapsed = 0.0f;
    uint  tick = GetTick();
    if( AnimDelay && animController )
    {
        // 2D emulation
        while( lastTick + AnimDelay <= tick )
        {
            elapsed += 0.001f * (float) AnimDelay;
            lastTick += AnimDelay;
        }
    }
    else
    {
        // Smooth
        elapsed = 0.001f * (float) ( tick - lastTick );
        lastTick = tick;
    }

    // Move animation
    ProcessAnimation( elapsed, x, y, scale );

    // Draw mesh
    if( LightColor.a == 1.0f )
    {
        // Non transparent
        DrawCombinedMeshes();
    }
    else
    {
        // Transparent
        GL( glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ) );
        GL( glDepthFunc( GL_LESS ) );
        DrawCombinedMeshes();
        GL( glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE ) );
        GL( glDepthFunc( GL_EQUAL ) );
        DrawCombinedMeshes();
        GL( glDepthFunc( GL_LESS ) );
    }

    // Store draw parameters
    noDraw = false;
    drawScale = scale;
    drawXY.X = x;
    drawXY.Y = y;
}

void Animation3d::SetDrawPos( int x, int y )
{
    drawXY.X = x;
    drawXY.Y = y;
}

void Animation3d::ProcessAnimation( float elapsed, int x, int y, float scale )
{
    // Update world matrix, only for root
    if( !parentNode )
    {
        Vector pos = Convert2dTo3d( x, y );
        Matrix mat_rot_y, mat_scale, mat_trans;
        Matrix::Scaling( Vector( scale, scale, scale ), mat_scale );
        Matrix::RotationY( dirAngle * PI_VALUE / 180.0f, mat_rot_y );
        Matrix::Translation( pos, mat_trans );
        parentMatrix = mat_trans * matTransBase * matRot * mat_rot_y * matRotBase * mat_scale * matScale * matScaleBase;
        groundPos.x = parentMatrix.a4;
        groundPos.y = parentMatrix.b4;
        groundPos.z = parentMatrix.c4;

        // Clean border
        bonesBorder( 1000000.0f, 1000000.0f, -1000000.0f, -1000000.0f );
    }

    // Advance animation time
    if( animController && elapsed >= 0.0f )
    {
        elapsed *= GetSpeed();
        animController->AdvanceTime( elapsed );

        float track_position = animController->GetTrackPosition( currentTrack );
        if( animPosPeriod > 0.0f )
        {
            animPosProc = track_position / animPosPeriod;
            if( animPosProc >= 1.0f )
                animPosProc = fmod( animPosProc, 1.0f );
            animPosTime = track_position;
            if( animPosTime >= animPosPeriod )
                animPosTime = fmod( animPosTime, animPosPeriod );
        }
    }

    // Update matrices
    UpdateNodeMatrices( animEntity->xFile->rootNode, &parentMatrix );

    // Update linked matrices
    if( parentNode && linkNodes.size() )
    {
        for( uint i = 0, j = (uint) linkNodes.size() / 2; i < j; i++ )
            // UpdateNodeMatrices(linkNodes[i*2+1],&linkMatricles[i]);
            // linkNodes[i*2+1]->exCombinedTransformationMatrix=linkMatricles[i];
            linkNodes[ i * 2 + 1 ]->CombinedTransformationMatrix = linkNodes[ i * 2 ]->CombinedTransformationMatrix;
    }

    // Update world matrices for children
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
    {
        Animation3d* child = *it;
        child->groundPos = groundPos;
        child->parentMatrix = child->parentNode->CombinedTransformationMatrix * child->matTransBase * child->matRotBase * child->matScaleBase;
    }

    // Move child animations
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
        ( *it )->ProcessAnimation( elapsed, x, y, 1.0f );
}

void Animation3d::UpdateNodeMatrices( Node* node, const Matrix* parent_matrix )
{
    // If parent matrix exists multiply our node matrix by it
    node->CombinedTransformationMatrix = ( *parent_matrix ) * node->TransformationMatrix;

    // Calculate bone screen position
    Matrix& m = node->CombinedTransformationMatrix;
    node->ScreenPos.Set( m[ 0 ][ 3 ], m[ 1 ][ 3 ], m[ 2 ][ 3 ] );
    ProjectPosition( node->ScreenPos );

    // Update borders
    Vector& pos = node->ScreenPos;
    if( pos.x < bonesBorder.L )
        bonesBorder.L = pos.x;
    if( pos.x > bonesBorder.R )
        bonesBorder.R = pos.x;
    if( pos.y < bonesBorder.T )
        bonesBorder.T = pos.y;
    if( pos.y > bonesBorder.B )
        bonesBorder.B = pos.y;

    // Update child
    for( auto it = node->Children.begin(), end = node->Children.end(); it != end; ++it )
        UpdateNodeMatrices( *it, &node->CombinedTransformationMatrix );
}

void Animation3d::DrawCombinedMeshes()
{
    GL( glEnable( GL_CULL_FACE ) );
    GL( glEnable( GL_DEPTH_TEST ) );

    if( !shadowDisabled && !animEntity->shadowDisabled )
        for( size_t i = 0; i < combinedMeshesSize; i++ )
            DrawCombinedMesh( combinedMeshes[ i ], true );
    for( size_t i = 0; i < combinedMeshesSize; i++ )
        DrawCombinedMesh( combinedMeshes[ i ], false );

    GL( glDisable( GL_CULL_FACE ) );
    GL( glDisable( GL_DEPTH_TEST ) );
}

void Animation3d::DrawCombinedMesh( CombinedMesh* combined_mesh, bool shadow )
{
    if( combined_mesh->VAO )
    {
        GL( glBindVertexArray( combined_mesh->VAO ) );
        GL( glBindBuffer( GL_ARRAY_BUFFER, combined_mesh->VBO ) );
        GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, combined_mesh->IBO ) );
    }
    else
    {
        GL( glBindBuffer( GL_ARRAY_BUFFER, combined_mesh->VBO ) );
        GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, combined_mesh->IBO ) );
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
    }

    Effect*       effect = ( !shadow ? ( combined_mesh->DrawEffect ? combined_mesh->DrawEffect : Effect::Skinned3d ) : Effect::Skinned3dShadow );
    MeshTexture** textures = combined_mesh->Textures;

    GL( glUseProgram( effect->Program ) );

    if( effect->ZoomFactor != -1 )
        GL( glUniform1f( effect->ZoomFactor, GameOpt.SpritesZoom ) );
    if( effect->ProjectionMatrix != -1 )
        GL( glUniformMatrix4fv( effect->ProjectionMatrix, 1, GL_FALSE, MatrixProjCM[ 0 ] ) );
    if( effect->ColorMap != -1 && textures[ 0 ] )
    {
        GL( glBindTexture( GL_TEXTURE_2D, textures[ 0 ]->Id ) );
        GL( glUniform1i( effect->ColorMap, 0 ) );
        if( effect->ColorMapSize != -1 )
            GL( glUniform4fv( effect->ColorMapSize, 1, textures[ 0 ]->SizeData ) );
    }
    if( effect->LightColor != -1 )
        GL( glUniform4fv( effect->LightColor, 1, (float*) &LightColor ) );
    if( effect->WorldMatrices != -1 )
    {
        for( uint i = 0; i < MAX_BONE_MATRICES; i++ )
        {
            Matrix* m = combined_mesh->BoneCombinedMatrices[ i ];
            if( m )
            {
                WorldMatrices[ i ] = ( *m ) * combined_mesh->BoneOffsetMatrices[ i ];
                WorldMatrices[ i ].Transpose();                                         // Convert to column major order
            }
        }
        GL( glUniformMatrix4fv( effect->WorldMatrices, MAX_BONE_MATRICES, GL_FALSE, (float*) &WorldMatrices[ 0 ] ) );
    }
    if( effect->GroundPosition != -1 )
        GL( glUniform3fv( effect->GroundPosition, 1, (float*) &groundPos ) );

    if( effect->IsNeedProcess )
        GraphicLoader::EffectProcessVariables( effect, -1, animPosProc, animPosTime, textures );
    for( uint pass = 0; pass < effect->Passes; pass++ )
    {
        if( effect->IsNeedProcess )
            GraphicLoader::EffectProcessVariables( effect, pass );
        GL( glDrawElements( GL_TRIANGLES, (uint) combined_mesh->Indicies.size(), GL_UNSIGNED_SHORT, (void*) 0 ) );
    }

    GL( glUseProgram( 0 ) );

    if( combined_mesh->VAO )
    {
        GL( glBindVertexArray( 0 ) );
    }
    else
    {
        for( uint i = 0; i <= 7; i++ )
            GL( glDisableVertexAttribArray( i ) );
    }
}

void Animation3d::TransformMesh( CombinedMesh* combined_mesh )
{
    if( combined_mesh->VerticesTransformed.size() != combined_mesh->Vertices.size() )
        combined_mesh->VerticesTransformed.resize( combined_mesh->Vertices.size() );

    for( int i = 0; i < MAX_BONE_MATRICES; i++ )
    {
        Matrix* m = combined_mesh->BoneCombinedMatrices[ i ];
        if( m )
            WorldMatrices[ i ] = ( *m ) * combined_mesh->BoneOffsetMatrices[ i ];
    }

    for( size_t i = 0, j = combined_mesh->Vertices.size(); i < j; i++ )
    {
        Vertex3D& v = combined_mesh->Vertices[ i ];
        Vector position = Vector();
        for( int b = 0; b < BONES_PER_VERTEX; b++ )
        {
            Matrix& m = WorldMatrices[ (int) v.BlendIndices[ b ] ];
            position += m * v.Position * v.BlendWeights[ b ];
        }
        ProjectPosition( position );
        combined_mesh->VerticesTransformed[ i ] = position;
    }
}

bool Animation3d::StartUp()
{
    // FPS & Smooth
    if( GameOpt.Animation3dSmoothTime )
    {
        // Smoothing
        AnimDelay = 0;
        MoveTransitionTime = GameOpt.Animation3dSmoothTime / 1000.0f;
        if( MoveTransitionTime < 0.001f )
            MoveTransitionTime = 0.001f;
    }
    else
    {
        // 2D emulation
        AnimDelay = 1000 / ( GameOpt.Animation3dFPS ? GameOpt.Animation3dFPS : 10 );
        MoveTransitionTime = 0.001f;
    }

    return true;
}

bool Animation3d::SetScreenSize( int width, int height )
{
    // Build orthogonal projection
    ModeWidth = width;
    ModeHeight = height;
    ModeWidthF = (float) ModeWidth;
    ModeHeightF = (float) ModeHeight;

    // Projection
    float k = (float) ModeHeight / 768.0f;
    gluStuffOrtho( MatrixProjRM[ 0 ], 0.0f, 18.65f * k * ModeWidthF / ModeHeightF, 0.0f, 18.65f * k, -500.0f, 500.0f );
    MatrixProjCM = MatrixProjRM;
    MatrixProjCM.Transpose();
    MatrixEmptyRM = Matrix();
    MatrixEmptyCM = MatrixEmptyRM;
    MatrixEmptyCM.Transpose();

    // View port
    GL( glViewport( 0, 0, ModeWidth, ModeHeight ) );

    return true;
}

void Animation3d::Finish()
{
    for( auto it = Animation3dEntity::allEntities.begin(), end = Animation3dEntity::allEntities.end(); it != end; ++it )
        delete *it;
    Animation3dEntity::allEntities.clear();
    for( auto it = Animation3dXFile::xFiles.begin(), end = Animation3dXFile::xFiles.end(); it != end; ++it )
        delete *it;
    Animation3dXFile::xFiles.clear();
}

void Animation3d::BeginScene()
{
    CurZ = -450.0f;
    for( auto it = loadedAnimations.begin(), end = loadedAnimations.end(); it != end; ++it )
        ( *it )->noDraw = true;
}

Animation3d* Animation3d::GetAnimation( const char* name, int path_type, bool is_child )
{
    char fname[ MAX_FOPATH ];
    Str::Copy( fname, FileManager::GetPath( path_type ) );
    Str::Append( fname, name );
    return GetAnimation( fname, is_child );
}

Animation3d* Animation3d::GetAnimation( const char* name, bool is_child )
{
    Animation3dEntity* entity = Animation3dEntity::GetEntity( name );
    if( !entity )
        return NULL;
    Animation3d* anim3d = entity->CloneAnimation();
    if( !anim3d )
        return NULL;

    // Create mesh instances
    anim3d->allMeshes.resize( entity->xFile->allDrawNodes.size() );
    anim3d->allMeshesDisabled.resize( anim3d->allMeshes.size() );
    for( size_t i = 0, j = entity->xFile->allDrawNodes.size(); i < j; i++ )
    {
        MeshInstance& mesh_instance = anim3d->allMeshes[ i ];
        MeshData*     mesh = entity->xFile->allDrawNodes[ i ]->Mesh;
        memzero( &mesh_instance, sizeof( mesh_instance ) );
        mesh_instance.Mesh = mesh;
        const char* tex_name = ( mesh->DiffuseTexture.length() ? mesh->DiffuseTexture.c_str() : NULL );
        mesh_instance.CurTexures[ 0 ] = mesh_instance.DefaultTexures[ 0 ] = ( tex_name ? entity->xFile->GetTexture( tex_name ) : NULL );
        mesh_instance.CurEffect = mesh_instance.DefaultEffect = ( mesh->DrawEffect.EffectFilename ? entity->xFile->GetEffect( &mesh->DrawEffect ) : NULL );
    }

    // Set default data
    anim3d->SetAnimation( 0, 0, NULL, ANIMATION_INIT );

    if( !is_child )
        loadedAnimations.push_back( anim3d );
    return anim3d;
}

void Animation3d::AnimateSlower()
{
    if( GlobalSpeedAdjust > 0.1f )
        GlobalSpeedAdjust -= 0.1f;
}

void Animation3d::AnimateFaster()
{
    GlobalSpeedAdjust += 0.1f;
}

Vector Animation3d::Convert2dTo3d( int x, int y )
{
    Vector pos;
    VecUnproject( Vector( (float) x, (float) y, 0.0f ), pos );
    pos.z = CurZ;
    return pos;
}

Point Animation3d::Convert3dTo2d( Vector pos )
{
    Vector coords;
    VecProject( pos, coords );
    return Point( (int) coords.x, (int) coords.y );
}

bool Animation3d::Is2dEmulation()
{
    return AnimDelay != 0;
}

/************************************************************************/
/* Animation3dEntity                                                    */
/************************************************************************/

Animation3dEntityVec Animation3dEntity::allEntities;

Animation3dEntity::Animation3dEntity(): xFile( NULL ), animController( NULL ),
                                        renderAnim( 0 ), renderAnimProcFrom( 0 ), renderAnimProcTo( 100 ),
                                        shadowDisabled( false )
{
    memzero( &animDataDefault, sizeof( animDataDefault ) );
}

Animation3dEntity::~Animation3dEntity()
{
    animData.push_back( animDataDefault );
    memzero( &animDataDefault, sizeof( animDataDefault ) );
    for( auto it = animData.begin(), end = animData.end(); it != end; ++it )
    {
        AnimParams& link = *it;
        SAFEDELA( link.ChildFName );
        SAFEDELA( link.DisabledLayers );
        SAFEDELA( link.DisabledMesh );
        for( uint i = 0; i < link.TextureCount; i++ )
            SAFEDELA( link.TextureName[ i ] );
        SAFEDELA( link.TextureName );
        SAFEDELA( link.TextureMesh );
        SAFEDELA( link.TextureNum );
        for( uint i = 0; i < link.EffectCount; i++ )
        {
            for( uint j = 0; j < link.EffectInst[ i ].DefaultsCount; j++ )
            {
                SAFEDELA( link.EffectInst[ i ].Defaults[ j ].Name );
                SAFEDELA( link.EffectInst[ i ].Defaults[ j ].Data );
                SAFEDELA( link.EffectInst[ i ].Defaults );
            }
        }
        SAFEDELA( link.EffectInst );
        SAFEDELA( link.EffectMesh );
    }
    animData.clear();
    delete animController;
}

bool Animation3dEntity::Load( const char* name )
{
    const char* ext = FileManager::GetExtension( name );
    if( !ext )
        return false;

    // Load fonline 3d file
    if( Str::CompareCase( ext, "fo3d" ) )
    {
        // Load main fo3d file
        FileManager fo3d;
        if( !fo3d.LoadFile( name, PT_DATA ) )
            return false;
        char* big_buf = Str::GetBigBuf();
        fo3d.CopyMem( big_buf, fo3d.GetFsize() );
        big_buf[ fo3d.GetFsize() ] = 0;

        // Extract file path
        char path[ MAX_FOPATH ];
        FileManager::ExtractPath( name, path );

        // Parse
        char             model[ MAX_FOPATH ] = { 0 };
        char             render_fname[ MAX_FOPATH ] = { 0 };
        char             render_anim[ MAX_FOPATH ] = { 0 };
        vector< size_t > anim_indexes;

        uint             mesh = 0;
        int              layer = -1;
        int              layer_val = 0;
        EffectInstance*  cur_effect = NULL;

        AnimParams       dummy_link;
        memzero( &dummy_link, sizeof( dummy_link ) );
        AnimParams*      link = &animDataDefault;
        static uint      link_id = 0;

        int              istr_pos = 0;
        istrstream*      istr = new istrstream( big_buf );
        bool             closed = false;
        char             token[ MAX_FOTEXT ];
        char             line[ MAX_FOTEXT ];
        char             buf[ MAX_FOTEXT ];
        float            valuei = 0;
        float            valuef = 0.0f;
        while( !( *istr ).eof() )
        {
            ( *istr ) >> token;
            if( ( *istr ).fail() )
                break;

            char* comment = Str::Substring( token, ";" );
            if( !comment )
                comment = Str::Substring( token, "#" );
            if( comment )
            {
                *comment = 0;
                ( *istr ).getline( line, MAX_FOTEXT );
                if( !Str::Length( token ) )
                    continue;
            }

            if( closed )
            {
                if( Str::CompareCase( token, "ContinueParsing" ) )
                    closed = false;
                continue;
            }

            if( Str::CompareCase( token, "StopParsing" ) )
            {
                closed = true;
            }
            else if( Str::CompareCase( token, "Model" ) )
            {
                ( *istr ) >> buf;
                FileManager::MakeFilePath( buf, path, model );
            }
            else if( Str::CompareCase( token, "Include" ) )
            {
                // Get swapped words
                StrVec templates;
                ( *istr ).getline( line, MAX_FOTEXT );
                Str::EraseChars( line, '\r' );
                Str::EraseChars( line, '\n' );
                Str::ParseLine( line, ' ', templates, Str::ParseLineDummy );
                if( templates.empty() )
                    continue;
                for( uint i = 1, j = (uint) templates.size(); i < j - 1; i += 2 )
                    templates[ i ] = string( "%" ).append( templates[ i ] ).append( "%" );

                // Include file path
                char fname[ MAX_FOPATH ];
                FileManager::MakeFilePath( templates[ 0 ].c_str(), path, fname );

                // Load file
                FileManager fo3d_ex;
                if( !fo3d_ex.LoadFile( fname, PT_DATA ) )
                {
                    WriteLogF( _FUNC_, " - Include file<%s> not found.\n", fname );
                    continue;
                }

                // Words swapping
                uint  len = fo3d_ex.GetFsize();
                char* pbuf = (char*) fo3d_ex.ReleaseBuffer();
                if( templates.size() > 2 )
                {
                    // Grow buffer for longer swapped words
                    char* tmp_pbuf = new char[ len + MAX_FOTEXT ];
                    memcpy( tmp_pbuf, pbuf, len );
                    tmp_pbuf[ len ] = 0;
                    delete[] pbuf;
                    pbuf = tmp_pbuf;

                    // Swap words
                    for( int i = 1, j = (int) templates.size(); i < j - 1; i += 2 )
                    {
                        const char* from = templates[ i ].c_str();
                        const char* to = templates[ i + 1 ].c_str();

                        char*       replace = Str::Substring( pbuf, from );
                        while( replace )
                        {
                            Str::EraseInterval( replace, Str::Length( from ) );
                            Str::Insert( replace, to );
                            replace = Str::Substring( pbuf, from );
                        }
                    }
                }

                // Insert new buffer to main file
                istr_pos += (int) ( *istr ).tellg();
                Str::Insert( &big_buf[ istr_pos ], " " );
                Str::Insert( &big_buf[ istr_pos + 1 ], pbuf );
                Str::Insert( &big_buf[ istr_pos + 1 + Str::Length( pbuf ) ], " " );
                delete[] pbuf;

                // Reinitialize stream
                delete istr;
                istr = new istrstream( &big_buf[ istr_pos ] );
            }
            else if( Str::CompareCase( token, "Root" ) )
            {
                if( layer < 0 )
                    link = &animDataDefault;
                else if( layer_val <= 0 )
                {
                    WriteLogF( _FUNC_, "Wrong layer<%d> value<%d>.\n", layer, layer_val );
                    link = &dummy_link;
                }
                else
                {
                    animData.push_back( AnimParams() );
                    link = &animData.back();
                    memzero( link, sizeof( AnimParams ) );
                    link->Id = ++link_id;
                    link->Layer = layer;
                    link->LayerValue = layer_val;
                }

                mesh = 0;
            }
            else if( Str::CompareCase( token, "Mesh" ) )
            {
                ( *istr ) >> buf;
                if( !Str::CompareCase( buf, "All" ) )
                    mesh = Node::GetHash( buf );
                else
                    mesh = 0;
            }
            else if( Str::CompareCase( token, "Subset" ) )
            {
                ( *istr ) >> buf;
                WriteLogF( _FUNC_, " - Tag 'Subset' obsolete, use 'Mesh' instead.\n" );
            }
            else if( Str::CompareCase( token, "Layer" ) || Str::CompareCase( token, "Value" ) )
            {
                ( *istr ) >> buf;
                if( Str::CompareCase( token, "Layer" ) )
                    layer = ConstantsManager::GetDefineValue( buf );
                else
                    layer_val = ConstantsManager::GetDefineValue( buf );

                link = &dummy_link;
                mesh = 0;
            }
            else if( Str::CompareCase( token, "Attach" ) )
            {
                ( *istr ) >> buf;
                if( layer < 0 || layer_val <= 0 )
                    continue;

                animData.push_back( AnimParams() );
                link = &animData.back();
                memzero( link, sizeof( AnimParams ) );
                link->Id = ++link_id;
                link->Layer = layer;
                link->LayerValue = layer_val;

                char fname[ MAX_FOPATH ];
                FileManager::MakeFilePath( buf, path, fname );
                link->ChildFName = Str::Duplicate( fname );

                mesh = 0;
            }
            else if( Str::CompareCase( token, "Link" ) )
            {
                ( *istr ) >> buf;
                if( link->Id )
                    link->LinkBoneHash = Node::GetHash( buf );
            }
            else if( Str::CompareCase( token, "RotX" ) )
                ( *istr ) >> link->RotX;
            else if( Str::CompareCase( token, "RotY" ) )
                ( *istr ) >> link->RotY;
            else if( Str::CompareCase( token, "RotZ" ) )
                ( *istr ) >> link->RotZ;
            else if( Str::CompareCase( token, "MoveX" ) )
                ( *istr ) >> link->MoveX;
            else if( Str::CompareCase( token, "MoveY" ) )
                ( *istr ) >> link->MoveY;
            else if( Str::CompareCase( token, "MoveZ" ) )
                ( *istr ) >> link->MoveZ;
            else if( Str::CompareCase( token, "ScaleX" ) )
                ( *istr ) >> link->ScaleX;
            else if( Str::CompareCase( token, "ScaleY" ) )
                ( *istr ) >> link->ScaleY;
            else if( Str::CompareCase( token, "ScaleZ" ) )
                ( *istr ) >> link->ScaleZ;
            else if( Str::CompareCase( token, "Scale" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleX = link->ScaleY = link->ScaleZ = valuef;
            }
            else if( Str::CompareCase( token, "Speed" ) )
                ( *istr ) >> link->SpeedAjust;
            else if( Str::CompareCase( token, "RotX+" ) )
            {
                ( *istr ) >> valuef;
                link->RotX = ( link->RotX == 0.0f ? valuef : link->RotX + valuef );
            }
            else if( Str::CompareCase( token, "RotY+" ) )
            {
                ( *istr ) >> valuef;
                link->RotY = ( link->RotY == 0.0f ? valuef : link->RotY + valuef );
            }
            else if( Str::CompareCase( token, "RotZ+" ) )
            {
                ( *istr ) >> valuef;
                link->RotZ = ( link->RotZ == 0.0f ? valuef : link->RotZ + valuef );
            }
            else if( Str::CompareCase( token, "MoveX+" ) )
            {
                ( *istr ) >> valuef;
                link->MoveX = ( link->MoveX == 0.0f ? valuef : link->MoveX + valuef );
            }
            else if( Str::CompareCase( token, "MoveY+" ) )
            {
                ( *istr ) >> valuef;
                link->MoveY = ( link->MoveY == 0.0f ? valuef : link->MoveY + valuef );
            }
            else if( Str::CompareCase( token, "MoveZ+" ) )
            {
                ( *istr ) >> valuef;
                link->MoveZ = ( link->MoveZ == 0.0f ? valuef : link->MoveZ + valuef );
            }
            else if( Str::CompareCase( token, "ScaleX+" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleX = ( link->ScaleX == 0.0f ? valuef : link->ScaleX + valuef );
            }
            else if( Str::CompareCase( token, "ScaleY+" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleY = ( link->ScaleY == 0.0f ? valuef : link->ScaleY + valuef );
            }
            else if( Str::CompareCase( token, "ScaleZ+" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleZ = ( link->ScaleZ == 0.0f ? valuef : link->ScaleZ + valuef );
            }
            else if( Str::CompareCase( token, "Scale+" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleX = ( link->ScaleX == 0.0f ? valuef : link->ScaleX + valuef );
                link->ScaleY = ( link->ScaleY == 0.0f ? valuef : link->ScaleY + valuef );
                link->ScaleZ = ( link->ScaleZ == 0.0f ? valuef : link->ScaleZ + valuef );
            }
            else if( Str::CompareCase( token, "Speed+" ) )
            {
                ( *istr ) >> valuef;
                link->SpeedAjust = ( link->SpeedAjust == 0.0f ? valuef : link->SpeedAjust * valuef );
            }
            else if( Str::CompareCase( token, "RotX*" ) )
            {
                ( *istr ) >> valuef;
                link->RotX = ( link->RotX == 0.0f ? valuef : link->RotX * valuef );
            }
            else if( Str::CompareCase( token, "RotY*" ) )
            {
                ( *istr ) >> valuef;
                link->RotY = ( link->RotY == 0.0f ? valuef : link->RotY * valuef );
            }
            else if( Str::CompareCase( token, "RotZ*" ) )
            {
                ( *istr ) >> valuef;
                link->RotZ = ( link->RotZ == 0.0f ? valuef : link->RotZ * valuef );
            }
            else if( Str::CompareCase( token, "MoveX*" ) )
            {
                ( *istr ) >> valuef;
                link->MoveX = ( link->MoveX == 0.0f ? valuef : link->MoveX * valuef );
            }
            else if( Str::CompareCase( token, "MoveY*" ) )
            {
                ( *istr ) >> valuef;
                link->MoveY = ( link->MoveY == 0.0f ? valuef : link->MoveY * valuef );
            }
            else if( Str::CompareCase( token, "MoveZ*" ) )
            {
                ( *istr ) >> valuef;
                link->MoveZ = ( link->MoveZ == 0.0f ? valuef : link->MoveZ * valuef );
            }
            else if( Str::CompareCase( token, "ScaleX*" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleX = ( link->ScaleX == 0.0f ? valuef : link->ScaleX * valuef );
            }
            else if( Str::CompareCase( token, "ScaleY*" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleY = ( link->ScaleY == 0.0f ? valuef : link->ScaleY * valuef );
            }
            else if( Str::CompareCase( token, "ScaleZ*" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleZ = ( link->ScaleZ == 0.0f ? valuef : link->ScaleZ * valuef );
            }
            else if( Str::CompareCase( token, "Scale*" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleX = ( link->ScaleX == 0.0f ? valuef : link->ScaleX * valuef );
                link->ScaleY = ( link->ScaleY == 0.0f ? valuef : link->ScaleY * valuef );
                link->ScaleZ = ( link->ScaleZ == 0.0f ? valuef : link->ScaleZ * valuef );
            }
            else if( Str::CompareCase( token, "Speed*" ) )
            {
                ( *istr ) >> valuef;
                link->SpeedAjust = ( link->SpeedAjust == 0.0f ? valuef : link->SpeedAjust * valuef );
            }
            else if( Str::CompareCase( token, "DisableLayer" ) )
            {
                ( *istr ) >> buf;
                StrVec layers;
                Str::ParseLine( buf, '-', layers, Str::ParseLineDummy );
                for( uint m = 0, n = (uint) layers.size(); m < n; m++ )
                {
                    int layer = ConstantsManager::GetDefineValue( layers[ m ].c_str() );
                    if( layer >= 0 && layer < LAYERS3D_COUNT )
                    {
                        int* tmp = link->DisabledLayers;
                        link->DisabledLayers = new int[ link->DisabledLayersCount + 1 ];
                        for( uint h = 0; h < link->DisabledLayersCount; h++ )
                            link->DisabledLayers[ h ] = tmp[ h ];
                        SAFEDELA( tmp );
                        link->DisabledLayers[ link->DisabledLayersCount ] = layer;
                        link->DisabledLayersCount++;
                    }
                }
            }
            else if( Str::CompareCase( token, "DisableSubset" ) )
            {
                ( *istr ) >> buf;
                WriteLogF( _FUNC_, " - Tag 'DisableSubset' obsolete, use 'DisableMesh' instead.\n" );
            }
            else if( Str::CompareCase( token, "DisableMesh" ) )
            {
                ( *istr ) >> buf;
                StrVec meshes;
                Str::ParseLine( buf, '-', meshes, Str::ParseLineDummy );
                for( uint m = 0, n = (uint) meshes.size(); m < n; m++ )
                {
                    uint mesh_name_hash = 0;
                    if( !Str::CompareCase( meshes[ m ].c_str(), "All" ) )
                        mesh_name_hash = Node::GetHash( meshes[ m ].c_str() );
                    uint* tmp = link->DisabledMesh;
                    link->DisabledMesh = new uint[ link->DisabledMeshCount + 1 ];
                    for( uint h = 0; h < link->DisabledMeshCount; h++ )
                        link->DisabledMesh[ h ] = tmp[ h ];
                    SAFEDELA( tmp );
                    link->DisabledMesh[ link->DisabledMeshCount ] = mesh_name_hash;
                    link->DisabledMeshCount++;
                }
            }
            else if( Str::CompareCase( token, "Texture" ) )
            {
                ( *istr ) >> buf;
                int index = ConstantsManager::GetDefineValue( buf );
                ( *istr ) >> buf;
                if( index >= 0 && index < EFFECT_TEXTURES )
                {
                    char** tmp1 = link->TextureName;
                    uint*  tmp2 = link->TextureMesh;
                    int*   tmp3 = link->TextureNum;
                    link->TextureName = new char*[ link->TextureCount + 1 ];
                    link->TextureMesh = new uint[ link->TextureCount + 1 ];
                    link->TextureNum = new int[ link->TextureCount + 1 ];
                    for( uint h = 0; h < link->TextureCount; h++ )
                    {
                        link->TextureName[ h ] = tmp1[ h ];
                        link->TextureMesh[ h ] = tmp2[ h ];
                        link->TextureNum[ h ] = tmp3[ h ];
                    }
                    SAFEDELA( tmp1 );
                    SAFEDELA( tmp2 );
                    SAFEDELA( tmp3 );
                    link->TextureName[ link->TextureCount ] = Str::Duplicate( buf );
                    link->TextureMesh[ link->TextureCount ] = mesh;
                    link->TextureNum[ link->TextureCount ] = index;
                    link->TextureCount++;
                }
            }
            else if( Str::CompareCase( token, "Effect" ) )
            {
                ( *istr ) >> buf;
                EffectInstance* effect_inst = new EffectInstance;
                memzero( effect_inst, sizeof( EffectInstance ) );
                effect_inst->EffectFilename = Str::Duplicate( buf );

                EffectInstance* tmp1 = link->EffectInst;
                uint*           tmp2 = link->EffectMesh;
                link->EffectInst = new EffectInstance[ link->EffectCount + 1 ];
                link->EffectMesh = new uint[ link->EffectCount + 1 ];
                for( uint h = 0; h < link->EffectCount; h++ )
                {
                    link->EffectInst[ h ] = tmp1[ h ];
                    link->EffectMesh[ h ] = tmp2[ h ];
                }
                SAFEDELA( tmp1 );
                SAFEDELA( tmp2 );
                link->EffectInst[ link->EffectCount ] = *effect_inst;
                link->EffectMesh[ link->EffectCount ] = mesh;
                link->EffectCount++;

                cur_effect = &link->EffectInst[ link->EffectCount - 1 ];
            }
            else if( Str::CompareCase( token, "EffDef" ) )
            {
                char def_name[ MAX_FOTEXT ];
                char def_value[ MAX_FOTEXT ];
                ( *istr ) >> buf;
                ( *istr ) >> def_name;
                ( *istr ) >> def_value;

                if( !cur_effect )
                    continue;

                EffectDefault::EType type;
                uchar*               data = NULL;
                uint                 data_len = 0;
                if( Str::CompareCase( buf, "String" ) )
                {
                    type = EffectDefault::String;
                    data = (uchar*) Str::Duplicate( def_value );
                    data_len = Str::Length( (char*) data );
                }
                else if( Str::CompareCase( buf, "Floats" ) )
                {
                    type = EffectDefault::Floats;
                    StrVec floats;
                    Str::ParseLine( def_value, '-', floats, Str::ParseLineDummy );
                    if( floats.empty() )
                        continue;
                    data_len = (uint) floats.size() * sizeof( float );
                    data = new uchar[ data_len ];
                    for( uint i = 0, j = (uint) floats.size(); i < j; i++ )
                        ( (float*) data )[ i ] = (float) atof( floats[ i ].c_str() );
                }
                else if( Str::CompareCase( buf, "Dword" ) )
                {
                    type = EffectDefault::Dword;
                    data_len = sizeof( uint );
                    data = new uchar[ data_len ];
                    *( (uint*) data ) = ConstantsManager::GetDefineValue( def_value );
                }
                else
                    continue;

                EffectDefault* tmp = cur_effect->Defaults;
                cur_effect->Defaults = new EffectDefault[ cur_effect->DefaultsCount + 1 ];
                for( uint h = 0; h < cur_effect->DefaultsCount; h++ )
                    cur_effect->Defaults[ h ] = tmp[ h ];
                if( tmp )
                    delete[] tmp;
                cur_effect->Defaults[ cur_effect->DefaultsCount ].Type = type;
                cur_effect->Defaults[ cur_effect->DefaultsCount ].Name = Str::Duplicate( def_name );
                cur_effect->Defaults[ cur_effect->DefaultsCount ].Size = data_len;
                cur_effect->Defaults[ cur_effect->DefaultsCount ].Data = data;
                cur_effect->DefaultsCount++;
            }
            else if( Str::CompareCase( token, "Anim" ) || Str::CompareCase( token, "AnimSpeed" ) )
            {
                // Index animation
                int ind1 = 0, ind2 = 0;
                ( *istr ) >> buf;
                ind1 = ConstantsManager::GetDefineValue( buf );
                ( *istr ) >> buf;
                ind2 = ConstantsManager::GetDefineValue( buf );

                if( Str::CompareCase( token, "Anim" ) )
                {
                    // Deferred loading
                    // Todo: Reverse play

                    anim_indexes.push_back( ( ind1 << 8 ) | ind2 );
                    ( *istr ) >> buf;
                    anim_indexes.push_back( ( size_t ) Str::Duplicate( buf ) );
                    ( *istr ) >> buf;
                    anim_indexes.push_back( ( size_t ) Str::Duplicate( buf ) );
                }
                else
                {
                    ( *istr ) >> valuef;
                    animSpeed.insert( PAIR( ( ind1 << 8 ) | ind2, valuef ) );
                }
            }
            else if( Str::CompareCase( token, "AnimEqual" ) )
            {
                ( *istr ) >> valuei;

                int ind1 = 0, ind2 = 0;
                ( *istr ) >> buf;
                ind1 = ConstantsManager::GetDefineValue( buf );
                ( *istr ) >> buf;
                ind2 = ConstantsManager::GetDefineValue( buf );

                if( valuei == 1 )
                    anim1Equals.insert( PAIR( ind1, ind2 ) );
                else if( valuei == 2 )
                    anim2Equals.insert( PAIR( ind1, ind2 ) );
            }
            else if( Str::CompareCase( token, "RenderFrame" ) || Str::CompareCase( token, "RenderFrames" ) )
            {
                anim_indexes.push_back( 0 );
                ( *istr ) >> buf;
                Str::Copy( render_fname, buf );
                anim_indexes.push_back( ( size_t ) Str::Duplicate( buf ) );
                ( *istr ) >> buf;
                Str::Copy( render_anim, buf );
                anim_indexes.push_back( ( size_t ) Str::Duplicate( buf ) );

                ( *istr ) >> renderAnimProcFrom;

                // One frame
                renderAnimProcTo = renderAnimProcFrom;

                // Many frames
                if( Str::CompareCase( token, "RenderFrames" ) )
                    ( *istr ) >> renderAnimProcTo;

                // Check
                renderAnimProcFrom = CLAMP( renderAnimProcFrom, 0, 100 );
                renderAnimProcTo = CLAMP( renderAnimProcTo, 0, 100 );
            }
            else if( Str::CompareCase( token, "DisableShadow" ) )
            {
                shadowDisabled = true;
            }
            else
            {
                WriteLogF( _FUNC_, " - Unknown token<%s> in file<%s>.\n", token, name );
            }
        }

        // Process pathes
        if( !model[ 0 ] )
        {
            WriteLogF( _FUNC_, " - 'Model' section not found in file<%s>.\n", name );
            return false;
        }

        // Load x file
        Animation3dXFile* xfile = Animation3dXFile::GetXFile( model );
        if( !xfile )
            return false;

        // Create animation
        fileName = name;
        xFile = xfile;

        // Single frame render
        if( render_fname[ 0 ] && render_anim[ 0 ] )
        {
            anim_indexes.push_back( -1 );
            anim_indexes.push_back( ( size_t ) Str::Duplicate( render_fname ) );
            anim_indexes.push_back( ( size_t ) Str::Duplicate( render_anim ) );
        }

        // Create animation controller
        if( !anim_indexes.empty() )
        {
            animController = AnimController::Create( 2 );
            if( !animController )
                WriteLogF( _FUNC_, " - Unable to create animation controller, file<%s>.\n", name );
        }

        // Parse animations
        if( animController )
        {
            for( uint i = 0, j = (uint) anim_indexes.size(); i < j; i += 3 )
            {
                int   anim_index = (int) anim_indexes[ i + 0 ];
                char* anim_fname = (char*) anim_indexes[ i + 1 ];
                char* anim_name = (char*) anim_indexes[ i + 2 ];

                char  anim_path[ MAX_FOPATH ];
                if( Str::CompareCase( anim_fname, "ModelFile" ) )
                    Str::Copy( anim_path, model );
                else
                    FileManager::MakeFilePath( anim_fname, path, anim_path );

                AnimSet* set = GraphicLoader::LoadAnimation( anim_path, anim_name );
                if( set )
                {
                    animController->RegisterAnimationSet( set );
                    uint set_index = animController->GetNumAnimationSets() - 1;

                    if( anim_index == -1 )
                        renderAnim = set_index;
                    else if( anim_index )
                        animIndexes.insert( PAIR( anim_index, set_index ) );
                }

                delete[] anim_fname;
                delete[] anim_name;
            }

            numAnimationSets = animController->GetNumAnimationSets();
            if( numAnimationSets > 0 )
                Animation3dXFile::SetupAnimationOutput( xFile->rootNode, animController );
            else
                SAFEDEL( animController );
        }
    }
    // Load just x file
    else
    {
        Animation3dXFile* xfile = Animation3dXFile::GetXFile( name );
        if( !xfile )
            return false;

        // Create animation
        fileName = name;
        xFile = xfile;

        // Todo: process default animations
    }

    return true;
}

void Animation3dEntity::ProcessTemplateDefines( char* str, StrVec& def )
{
    for( int i = 1, j = (int) def.size(); i < j - 1; i += 2 )
    {
        const char* from = def[ i ].c_str();
        const char* to = def[ i + 1 ].c_str();

        char*       token = Str::Substring( str, from );
        while( token )
        {
            Str::EraseInterval( token, Str::Length( from ) );
            Str::Insert( token, to );
            token = Str::Substring( str, from );
        }
    }
}

int Animation3dEntity::GetAnimationIndex( uint& anim1, uint& anim2, float* speed )
{
    // Find index
    int index = GetAnimationIndexEx( anim1, anim2, speed );
    if( index != -1 )
        return index;

    // Find substitute animation
    uint crtype = 0;
    uint crtype_base = crtype, anim1_base = anim1, anim2_base = anim2;
    #ifdef FONLINE_CLIENT
    while( index == -1 && Script::PrepareContext( ClientFunctions.CritterAnimationSubstitute, _FUNC_, "Anim" ) )
    #else // FONLINE_MAPPER
    while( index == -1 && Script::PrepareContext( MapperFunctions.CritterAnimationSubstitute, _FUNC_, "Anim" ) )
    #endif
    {
        uint crtype_ = crtype, anim1_ = anim1, anim2_ = anim2;
        Script::SetArgUInt( ANIM_TYPE_3D );
        Script::SetArgUInt( crtype_base );
        Script::SetArgUInt( anim1_base );
        Script::SetArgUInt( anim2_base );
        Script::SetArgAddress( &crtype );
        Script::SetArgAddress( &anim1 );
        Script::SetArgAddress( &anim2 );
        if( Script::RunPrepared() && Script::GetReturnedBool() &&
            ( crtype_ != crtype || anim1 != anim1_ || anim2 != anim2_ ) )
            index = GetAnimationIndexEx( anim1, anim2, speed );
        else
            break;
    }

    return index;
}

int Animation3dEntity::GetAnimationIndexEx( uint anim1, uint anim2, float* speed )
{
    // Check equals
    auto it1 = anim1Equals.find( anim1 );
    if( it1 != anim1Equals.end() )
        anim1 = ( *it1 ).second;
    auto it2 = anim2Equals.find( anim2 );
    if( it2 != anim2Equals.end() )
        anim2 = ( *it2 ).second;

    // Make index
    int ii = ( anim1 << 8 ) | anim2;

    // Speed
    if( speed )
    {
        auto it = animSpeed.find( ii );
        if( it != animSpeed.end() )
            *speed = ( *it ).second;
        else
            *speed = 1.0f;
    }

    // Find number of animation
    auto it = animIndexes.find( ii );
    if( it != animIndexes.end() )
        return ( *it ).second;

    return -1;
}

Animation3d* Animation3dEntity::CloneAnimation()
{
    // Create instance
    Animation3d* a3d = new Animation3d();
    if( !a3d )
        return NULL;

    a3d->animEntity = this;
    a3d->lastTick = Timer::GameTick();

    if( animController )
        a3d->animController = animController->Clone();

    return a3d;
}

Animation3dEntity* Animation3dEntity::GetEntity( const char* name )
{
    // Try find instance
    Animation3dEntity* entity = NULL;
    for( auto it = allEntities.begin(), end = allEntities.end(); it != end; ++it )
    {
        Animation3dEntity* e = *it;
        if( e->fileName == name )
        {
            entity = e;
            break;
        }
    }

    // Create new instance
    if( !entity )
    {
        entity = new Animation3dEntity();
        if( !entity || !entity->Load( name ) )
        {
            SAFEDEL( entity );
            return NULL;
        }

        allEntities.push_back( entity );
    }

    return entity;
}

/************************************************************************/
/* Animation3dXFile                                                     */
/************************************************************************/

Animation3dXFileVec Animation3dXFile::xFiles;

Animation3dXFile::Animation3dXFile(): rootNode( NULL )
{}

Animation3dXFile::~Animation3dXFile()
{
    GraphicLoader::DestroyModel( rootNode );
    rootNode = NULL;
}

Animation3dXFile* Animation3dXFile::GetXFile( const char* xname )
{
    Animation3dXFile* xfile = NULL;

    for( auto it = xFiles.begin(), end = xFiles.end(); it != end; ++it )
    {
        Animation3dXFile* x = *it;
        if( x->fileName == xname )
        {
            xfile = x;
            break;
        }
    }

    if( !xfile )
    {
        // Load
        Node* root_node = GraphicLoader::LoadModel( xname );
        if( !root_node )
        {
            WriteLogF( _FUNC_, " - Unable to load 3d file<%s>.\n", xname );
            return NULL;
        }

        xfile = new Animation3dXFile();
        if( !xfile )
        {
            WriteLogF( _FUNC_, " - Allocation fail, x file<%s>.\n", xname );
            return NULL;
        }

        xfile->fileName = xname;
        xfile->rootNode = root_node;
        xfile->SetupNodes( xfile, xfile->rootNode, xfile->rootNode );
        xfile->FixTextureCoords( xfile->rootNode, xname );

        xFiles.push_back( xfile );
    }

    return xfile;
}

void Animation3dXFile::SetupNodes( Animation3dXFile* xfile, Node* node, Node* root_node )
{
    xfile->allNodes.push_back( node );

    if( node->Mesh )
        xfile->allDrawNodes.push_back( node );

    for( auto it = node->Children.begin(), end = node->Children.end(); it != end; ++it )
        SetupNodes( xfile, *it, root_node );
}

void Animation3dXFile::SetupAnimationOutput( Node* node, AnimController* anim_controller )
{
    anim_controller->RegisterAnimationOutput( node->NameHash, node->TransformationMatrix );

    for( auto it = node->Children.begin(), end = node->Children.end(); it != end; ++it )
        SetupAnimationOutput( *it, anim_controller );
}

MeshTexture* Animation3dXFile::GetTexture( const char* tex_name )
{
    MeshTexture* texture = GraphicLoader::LoadTexture( tex_name, fileName.c_str() );
    if( !texture )
        WriteLogF( _FUNC_, " - Can't load texture<%s>.\n", tex_name ? tex_name : "nullptr" );
    return texture;
}

Effect* Animation3dXFile::GetEffect( EffectInstance* effect_inst )
{
    Effect* effect = GraphicLoader::LoadEffect( effect_inst->EffectFilename, false, NULL, fileName.c_str(), effect_inst->Defaults, effect_inst->DefaultsCount );
    if( !effect )
        WriteLogF( _FUNC_, " - Can't load effect<%s>.\n", effect_inst && effect_inst->EffectFilename ? effect_inst->EffectFilename : "nullptr" );
    return effect;
}

void Animation3dXFile::FixTextureCoords( Node* node, const char* model_path )
{
    if( node->Mesh )
    {
        MeshTexture* texture = GraphicLoader::LoadTexture( node->Mesh->DiffuseTexture.c_str(), model_path );
        if( texture )
        {
            for( auto it = node->Mesh->Vertices.begin(), end = node->Mesh->Vertices.end(); it != end; ++it )
            {
                Vertex3D& v = *it;
                v.TexCoordAtlas[ 0 ] = ( v.TexCoordBase[ 0 ] * texture->AtlasOffsetData[ 2 ] ) + texture->AtlasOffsetData[ 0 ];
                v.TexCoordAtlas[ 1 ] = ( v.TexCoordBase[ 1 ] * texture->AtlasOffsetData[ 3 ] ) + texture->AtlasOffsetData[ 1 ];
            }
        }
    }

    for( size_t i = 0, j = node->Children.size(); i < j; i++ )
        FixTextureCoords( node->Children[ i ], model_path );
}

void Animation3dXFile::FixAllTextureCoords()
{
    for( auto it = xFiles.begin(), end = xFiles.end(); it != end; ++it )
    {
        Animation3dXFile* xfile = *it;
        xfile->FixTextureCoords( xfile->rootNode, xfile->fileName.c_str() );
    }
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
