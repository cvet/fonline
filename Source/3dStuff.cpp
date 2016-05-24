#include "Common.h"
#include "3dStuff.h"
#include "3dAnimation.h"
#include "GraphicLoader.h"
#include "Text.h"
#include "Script.h"
#include "CritterType.h"
#include "GL/glu_stuff.h"

int       ModeWidth = 0, ModeHeight = 0;
float     ModeWidthF = 0, ModeHeightF = 0;
Matrix    MatrixProjRM, MatrixEmptyRM, MatrixProjCM, MatrixEmptyCM; // Row or Column major order
float     MoveTransitionTime = 0.25f;
float     GlobalSpeedAdjust = 1.0f;
bool      SoftwareSkinning = false;
uint      AnimDelay = 0;
Color     LightColor;
MatrixVec WorldMatrices;

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

Animation3d::Animation3d()
{
    animEntity = nullptr;
    animController = nullptr;
    combinedMeshesSize = 0;
    disableCulling = false;
    currentTrack = 0;
    lastDrawTick = 0;
    endTick = 0;
    speedAdjustBase = 1.0f;
    speedAdjustCur = 1.0f;
    speedAdjustLink = 1.0f;
    shadowDisabled = false;
    dirAngle = ( GameOpt.MapHexagonal ? 150.0f : 135.0f );
    parentAnimation = nullptr;
    parentBone = nullptr;
    childChecker = true;
    useGameTimer = true;
    animPosProc = 0.0f;
    animPosTime = 0.0f;
    animPosPeriod = 0.0f;
    allowMeshGeneration = false;
    SprId = 0;
    SprAtlasType = 0;
    memzero( currentLayers, sizeof( currentLayers ) );
    memzero( &animLink, sizeof( animLink ) );
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
    int   anim_pair = ( anim1 << 16 ) | anim2;
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
            index = animEntity->GetAnimationIndex( anim1, anim2, &speed, FLAG( flags, ANIMATION_COMBAT ) );
        }
    }

    if( FLAG( flags, ANIMATION_PERIOD( 0 ) ) )
        period_proc = (float) ( flags >> 16 );
    period_proc = CLAMP( period_proc, 0.0f, 99.9f );

    // Check animation changes
    int new_layers[ LAYERS3D_COUNT ];
    if( layers )
        memcpy( new_layers, layers, sizeof( int ) * LAYERS3D_COUNT );
    else
        memcpy( new_layers, currentLayers, sizeof( int ) * LAYERS3D_COUNT );

    // Animation layers
    auto it = animEntity->animLayerValues.find( anim_pair );
    if( it != animEntity->animLayerValues.end() )
        for( auto it_ = it->second.begin(); it_ != it->second.end(); ++it_ )
            new_layers[ it_->first ] = it_->second;

    // Check for change
    bool layer_changed = ( FLAG( flags, ANIMATION_INIT ) ? true : false );
    if( !layer_changed )
    {
        for( int i = 0; i < LAYERS3D_COUNT; i++ )
        {
            if( new_layers[ i ] != currentLayers[ i ] )
            {
                layer_changed = true;
                break;
            }
        }
    }

    // Is not one time play and same anim
    if( !FLAG( flags, ANIMATION_INIT | ANIMATION_ONE_TIME ) && currentLayers[ LAYERS3D_COUNT ] == anim_pair && !layer_changed )
        return false;

    memcpy( currentLayers, new_layers, sizeof( int ) * LAYERS3D_COUNT );
    currentLayers[ LAYERS3D_COUNT ] = anim_pair;

    bool    mesh_changed = false;
    HashVec fast_transition_bones;

    if( layer_changed )
    {
        // Store disabled meshes
        for( size_t i = 0, j = allMeshes.size(); i < j; i++ )
            allMeshesDisabled[ i ] = allMeshes[ i ].Disabled;

        // Set base data
        SetAnimData( animEntity->animDataDefault, true );

        // Append linked data
        if( parentBone )
            SetAnimData( animLink, false );

        // Mark animations as unused
        for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
            ( *it )->childChecker = false;

        // Get unused layers and meshes
        bool unused_layers[ LAYERS3D_COUNT ] = { 0 };
        for( int i = 0; i < LAYERS3D_COUNT; i++ )
        {
            if( new_layers[ i ] == 0 )
                continue;

            for( auto it_ = animEntity->animData.begin(), end_ = animEntity->animData.end(); it_ != end_; ++it_ )
            {
                AnimParams& link = *it_;
                if( link.Layer == i && link.LayerValue == new_layers[ i ] && !link.ChildFName )
                {
                    for( uint j = 0; j < link.DisabledLayersCount; j++ )
                    {
                        unused_layers[ link.DisabledLayers[ j ] ] = true;
                    }
                    for( uint j = 0; j < link.DisabledMeshCount; j++ )
                    {
                        uint disabled_mesh_name_hash = link.DisabledMesh[ j ];
                        for( auto it = allMeshes.begin(), end = allMeshes.end(); it != end; ++it )
                            if( !disabled_mesh_name_hash || disabled_mesh_name_hash == it->Mesh->Owner->NameHash )
                                it->Disabled = true;
                    }
                }
            }
        }

        if( parentBone )
        {
            for( uint j = 0; j < animLink.DisabledLayersCount; j++ )
            {
                unused_layers[ animLink.DisabledLayers[ j ] ] = true;
            }
            for( uint j = 0; j < animLink.DisabledMeshCount; j++ )
            {
                uint disabled_mesh_name_hash = animLink.DisabledMesh[ j ];
                for( auto it = allMeshes.begin(), end_ = allMeshes.end(); it != end_; ++it )
                    if( !disabled_mesh_name_hash || disabled_mesh_name_hash == it->Mesh->Owner->NameHash )
                        it->Disabled = true;
            }
        }

        // Append animations
        for( int i = 0; i < LAYERS3D_COUNT; i++ )
        {
            if( unused_layers[ i ] || new_layers[ i ] == 0 )
                continue;

            for( auto it = animEntity->animData.begin(), end = animEntity->animData.end(); it != end; ++it )
            {
                AnimParams& link = *it;
                if( link.Layer == i && link.LayerValue == new_layers[ i ] )
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
                        Animation3d* anim3d = nullptr;

                        // Link to main bone
                        if( link.LinkBoneHash )
                        {
                            Bone* to_bone = animEntity->xFile->rootBone->Find( link.LinkBoneHash );
                            if( to_bone )
                            {
                                anim3d = Animation3d::GetAnimation( link.ChildFName, true );
                                if( anim3d )
                                {
                                    mesh_changed = true;
                                    anim3d->parentAnimation = this;
                                    anim3d->parentBone = to_bone;
                                    anim3d->animLink = link;
                                    anim3d->SetAnimData( link, false );
                                    childAnimations.push_back( anim3d );
                                }

                                if( animEntity->fastTransitionBones.count( link.LinkBoneHash ) )
                                    fast_transition_bones.push_back( link.LinkBoneHash );
                            }
                        }
                        // Link all bones
                        else
                        {
                            anim3d = Animation3d::GetAnimation( link.ChildFName, true );
                            if( anim3d )
                            {
                                for( auto it = anim3d->animEntity->xFile->allBones.begin(), end = anim3d->animEntity->xFile->allBones.end(); it != end; ++it )
                                {
                                    Bone* child_bone = *it;
                                    Bone* root_bone = animEntity->xFile->rootBone->Find( child_bone->NameHash );
                                    if( root_bone )
                                    {
                                        anim3d->linkBones.push_back( root_bone );
                                        anim3d->linkBones.push_back( child_bone );
                                    }
                                }

                                mesh_changed = true;
                                anim3d->parentAnimation = this;
                                anim3d->parentBone = animEntity->xFile->rootBone;
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

        // Compare changed effect
        if( !mesh_changed )
        {
            for( size_t i = 0, j = allMeshes.size(); i < j && !mesh_changed; i++ )
                mesh_changed = ( allMeshes[ i ].LastEffect != allMeshes[ i ].CurEffect );
        }

        // Compare changed textures
        if( !mesh_changed )
        {
            for( size_t i = 0, j = allMeshes.size(); i < j && !mesh_changed; i++ )
                for( size_t k = 0; k < EFFECT_TEXTURES && !mesh_changed; k++ )
                    mesh_changed = ( allMeshes[ i ].LastTexures[ k ] != allMeshes[ i ].CurTexures[ k ] );
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

        // Turn off fast transition bones on other tracks
        if( !fast_transition_bones.empty() )
            animController->ResetBonesTransition( new_track, fast_transition_bones );

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
        else
            animController->AdvanceTime( 0.0001f );

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

        // Force redraw
        lastDrawTick = 0;
    }

    // Set animation for children
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
    {
        Animation3d* child = *it;
        if( child->SetAnimation( anim1, anim2, layers, flags ) )
            mesh_changed = true;
    }

    // Regenerate mesh for drawing
    if( !parentBone && mesh_changed )
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
    if( animEntity->GetAnimationIndex( anim1, anim2, nullptr, false ) == -1 )
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

void Animation3d::GetRenderFramesData( float& period, int& proc_from, int& proc_to, int& dir )
{
    period = 0.0f;
    proc_from = animEntity->renderAnimProcFrom;
    proc_to = animEntity->renderAnimProcTo;
    dir = animEntity->renderAnimDir;

    if( animController )
    {
        AnimSet* set = animController->GetAnimationSet( animEntity->renderAnim );
        if( set )
            period = set->GetDuration();
    }
}

void Animation3d::GetDrawSize( uint& draw_width, uint& draw_height )
{
    draw_width = animEntity->drawWidth;
    draw_height = animEntity->drawHeight;
    int s = (int) ceilf( MAX( MAX( matScale.a1, matScale.b2 ), matScale.c3 ) );
    draw_width *= s;
    draw_height *= s;
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
            memcpy( mesh_instance.LastTexures, mesh_instance.CurTexures, sizeof( mesh_instance.LastTexures ) );
            memcpy( mesh_instance.CurTexures, mesh_instance.DefaultTexures, sizeof( mesh_instance.CurTexures ) );
        }
    }

    if( data.TextureCount )
    {
        for( uint i = 0; i < data.TextureCount; i++ )
        {
            MeshTexture* texture = nullptr;

            // Get texture
            if( Str::CompareCount( data.TextureName[ i ], "Parent", 6 ) )     // Parent_MeshName
            {
                if( parentAnimation )
                {
                    const char* mesh_name = data.TextureName[ i ] + 6;
                    if( *mesh_name && *mesh_name == '_' )
                        mesh_name++;
                    uint mesh_name_hash = ( *mesh_name ? Bone::GetHash( mesh_name ) : 0 );
                    for( auto it = parentAnimation->allMeshes.begin(), end = parentAnimation->allMeshes.end(); it != end && !texture; ++it )
                        if( !mesh_name_hash || mesh_name_hash == it->Mesh->Owner->NameHash )
                            texture = it->CurTexures[ data.TextureNum[ i ] ];
                }
            }
            else
            {
                texture = animEntity->xFile->GetTexture( data.TextureName[ i ] );
            }

            // Assign it
            int  texture_num = data.TextureNum[ i ];
            uint mesh_name_hash = data.TextureMesh[ i ];
            for( auto it = allMeshes.begin(), end = allMeshes.end(); it != end; ++it )
                if( !mesh_name_hash || mesh_name_hash == it->Mesh->Owner->NameHash )
                    it->CurTexures[ texture_num ] = texture;
        }
    }

    // Effects
    if( clear )
    {
        for( auto it = allMeshes.begin(), end = allMeshes.end(); it != end; ++it )
        {
            MeshInstance& mesh_instance = *it;
            mesh_instance.LastEffect = mesh_instance.CurEffect;
            mesh_instance.CurEffect = mesh_instance.DefaultEffect;
        }
    }

    if( data.EffectCount )
    {
        for( uint i = 0; i < data.EffectCount; i++ )
        {
            Effect* effect = nullptr;

            // Get effect
            if( Str::CompareCount( data.EffectInst[ i ].EffectFilename, "Parent", 6 ) )     // Parent_MeshName
            {
                if( parentAnimation )
                {
                    const char* mesh_name = data.EffectInst[ i ].EffectFilename + 6;
                    if( *mesh_name && *mesh_name == '_' )
                        mesh_name++;
                    uint mesh_name_hash = ( *mesh_name ? Bone::GetHash( mesh_name ) : 0 );
                    for( auto it = parentAnimation->allMeshes.begin(), end = parentAnimation->allMeshes.end(); it != end && !effect; ++it )
                        if( !mesh_name_hash || mesh_name_hash == it->Mesh->Owner->NameHash )
                            effect = it->CurEffect;
                }
            }
            else
            {
                effect = animEntity->xFile->GetEffect( &data.EffectInst[ i ] );
            }

            // Assign it
            uint mesh_name_hash = data.EffectMesh[ i ];
            for( auto it = allMeshes.begin(), end = allMeshes.end(); it != end; ++it )
                if( !mesh_name_hash || mesh_name_hash == it->Mesh->Owner->NameHash )
                    it->CurEffect = effect;
        }
    }

    // Cut
    if( clear )
        allCuts.clear();
    for( uint i = 0; i < data.CutCount; i++ )
        allCuts.push_back( data.Cut[ i ] );
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
    Matrix::RotationX( rx, mx );
    Matrix::RotationY( ry, my );
    Matrix::RotationZ( rz, mz );
    matRot = mx * my * mz;
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

    // Cut
    disableCulling = false;
    CutCombinedMeshes( this, this );

    // Finalize meshes
    for( size_t i = 0, j = combinedMeshesSize; i < j; i++ )
        combinedMeshes[ i ]->Finalize();
}

void Animation3d::FillCombinedMeshes( Animation3d* base, Animation3d* cur )
{
    // Combine meshes
    for( size_t i = 0, j = cur->allMeshes.size(); i < j; i++ )
        base->CombineMesh( cur->allMeshes[ i ], cur->parentBone ? cur->animLink.Layer : 0 );

    // Fill child
    for( auto it = cur->childAnimations.begin(), end = cur->childAnimations.end(); it != end; ++it )
        FillCombinedMeshes( base, *it );
}

void Animation3d::CombineMesh( MeshInstance& mesh_instance, int anim_layer )
{
    // Skip disabled meshes
    if( mesh_instance.Disabled )
        return;

    // Try encapsulate mesh instance to current combined mesh
    for( size_t i = 0, j = combinedMeshesSize; i < j; i++ )
    {
        if( combinedMeshes[ i ]->CanEncapsulate( mesh_instance ) )
        {
            combinedMeshes[ i ]->Encapsulate( mesh_instance, anim_layer );
            return;
        }
    }

    // Create new combined mesh
    if( combinedMeshesSize >= combinedMeshes.size() )
        combinedMeshes.push_back( new CombinedMesh() );
    combinedMeshes[ combinedMeshesSize ]->Encapsulate( mesh_instance, anim_layer );
    combinedMeshesSize++;
}

void Animation3d::CutCombinedMeshes( Animation3d* base, Animation3d* cur )
{
    // Cut meshes
    if( !cur->allCuts.empty() )
    {
        for( size_t i = 0, j = cur->allCuts.size(); i < j; i++ )
            for( size_t k = 0; k < base->combinedMeshesSize; k++ )
                base->CutCombinedMesh( base->combinedMeshes[ k ], cur->allCuts[ i ] );
        disableCulling = true;
    }

    // Fill child
    for( auto it = cur->childAnimations.begin(), end = cur->childAnimations.end(); it != end; ++it )
        CutCombinedMeshes( base, *it );
}

// -2 - ignore
// -1 - inside
// 0 - outside
// 1 - one point
inline float Square( float f ) { return f * f; }
int          SphereLineIntersection( const Vertex3D& p1, const Vertex3D& p2, const Vector& sp, float r, Vertex3D& in )
{
    float a = Square( p2.Position.x - p1.Position.x ) + Square( p2.Position.y - p1.Position.y ) + Square( p2.Position.z - p1.Position.z );
    float b = 2 * ( ( p2.Position.x - p1.Position.x ) * ( p1.Position.x - sp.x ) + ( p2.Position.y - p1.Position.y ) * ( p1.Position.y - sp.y ) + ( p2.Position.z - p1.Position.z ) * ( p1.Position.z - sp.z ) );
    float c = Square( sp.x ) + Square( sp.y ) + Square( sp.z ) + Square( p1.Position.x ) + Square( p1.Position.y ) + Square( p1.Position.z ) - 2 * ( sp.x * p1.Position.x + sp.y * p1.Position.y + sp.z * p1.Position.z ) - Square( r );
    float i = Square( b ) - 4 * a * c;

    if( i > 0.0 )
    {
        float sqrt_i = sqrt( i );
        float mu1 = ( -b + sqrt_i ) / ( 2 * a );
        float mu2 = ( -b - sqrt_i ) / ( 2 * a );

        // Line segment doesn't intersect and on outside of sphere, in which case both values of u wll either be less than 0 or greater than 1
        if( ( mu1 < 0.0 && mu2 < 0.0 ) || ( mu1 > 1.0 && mu2 > 1.0 ) )
            return 0;

        // Line segment doesn't intersect and is inside sphere, in which case one value of u will be negative and the other greater than 1
        if( ( mu1 < 0.0 && mu2 > 1.0 ) || ( mu2 < 0.0 && mu1 > 1.0 ) )
            return -1;

        // Line segment intersects at one point, in which case one value of u will be between 0 and 1 and the other not
        if( ( mu1 >= 0.0 && mu1 <= 1.0 && ( mu2 < 0.0 || mu2 > 1.0 ) ) || ( mu2 >= 0.0 && mu2 <= 1.0 && ( mu1 < 0.0 || mu1 > 1.0 ) ) )
        {
            float& mu = ( ( mu1 >= 0.0 && mu1 <= 1.0 ) ? mu1 : mu2 );
            in = p1;
            in.Position.x = p1.Position.x + mu * ( p2.Position.x - p1.Position.x );
            in.Position.y = p1.Position.y + mu * ( p2.Position.y - p1.Position.y );
            in.Position.z = p1.Position.z + mu * ( p2.Position.z - p1.Position.z );
            in.TexCoord[ 0 ] = p1.TexCoord[ 0 ] + mu * ( p2.TexCoord[ 0 ] - p1.TexCoord[ 0 ] );
            in.TexCoord[ 1 ] = p1.TexCoord[ 1 ] + mu * ( p2.TexCoord[ 1 ] - p1.TexCoord[ 1 ] );
            in.TexCoordBase[ 0 ] = p1.TexCoordBase[ 0 ] + mu * ( p2.TexCoordBase[ 0 ] - p1.TexCoordBase[ 0 ] );
            in.TexCoordBase[ 1 ] = p1.TexCoordBase[ 1 ] + mu * ( p2.TexCoordBase[ 1 ] - p1.TexCoordBase[ 1 ] );
            return 1;
        }

        // Line segment intersects at two points, in which case both values of u will be between 0 and 1
        if( mu1 >= 0.0 && mu1 <= 1.0 && mu2 >= 0.0 && mu2 <= 1.0 )
        {
            // Ignore
            return -2;
        }
    }
    else if( i == 0.0 )
    {
        // Ignore
        return -2;
    }
    return 0;
}

void Animation3d::CutCombinedMesh( CombinedMesh* combined_mesh, CutData* cut )
{
    // Process
    IntVec& cut_layers = cut->Layers;
    for( size_t i = 0, j = cut->Shapes.size(); i < j; i++ )
    {
        bool        cut_all = ( std::find( cut_layers.begin(), cut_layers.end(), -1 ) != cut_layers.end() );
        Vertex3DVec result_vertices;
        UShortVec   result_indices;
        UIntVec     result_mesh_vertices;
        UIntVec     result_mesh_indices;
        result_vertices.reserve( combined_mesh->Vertices.size() );
        result_indices.reserve( combined_mesh->Indices.size() );
        result_mesh_vertices.reserve( combined_mesh->MeshVertices.size() );
        result_mesh_indices.reserve( combined_mesh->MeshIndices.size() );
        uint i_pos = 0, i_count = 0;
        for( size_t k = 0, l = combined_mesh->MeshIndices.size(); k < l; k++ )
        {
            // Move shape to face space
            Matrix     mesh_transform = combined_mesh->Meshes[ k ]->Owner->GlobalTransformationMatrix;
            Matrix     sm = mesh_transform.Inverse() * cut->Shapes[ i ].GlobalTransformationMatrix;
            Vector     ss, sp;
            Quaternion sr;
            sm.Decompose( ss, sr, sp );

            // Check anim layer
            int  mesh_anim_layer = combined_mesh->MeshAnimLayers[ k ];
            bool skip = ( !cut_all && std::find( cut_layers.begin(), cut_layers.end(), mesh_anim_layer ) == cut_layers.end() );

            // Process faces
            i_count += combined_mesh->MeshIndices[ k ];
            size_t vertices_old_size = result_vertices.size();
            size_t indices_old_size = result_indices.size();
            for( ; i_pos < i_count; i_pos += 3 )
            {
                // Face points
                const Vertex3D& v1 = combined_mesh->Vertices[ combined_mesh->Indices[ i_pos + 0 ] ];
                const Vertex3D& v2 = combined_mesh->Vertices[ combined_mesh->Indices[ i_pos + 1 ] ];
                const Vertex3D& v3 = combined_mesh->Vertices[ combined_mesh->Indices[ i_pos + 2 ] ];

                // Skip mesh
                if( skip )
                {
                    result_vertices.push_back( v1 );
                    result_vertices.push_back( v2 );
                    result_vertices.push_back( v3 );
                    result_indices.push_back( (ushort) result_indices.size() );
                    result_indices.push_back( (ushort) result_indices.size() );
                    result_indices.push_back( (ushort) result_indices.size() );
                    continue;
                }

                // Find intersections
                Vertex3D i1, i2, i3;
                int      r1 = SphereLineIntersection( v1, v2, sp, cut->Shapes[ i ].SphereRadius * ss.x, i1 );
                int      r2 = SphereLineIntersection( v2, v3, sp, cut->Shapes[ i ].SphereRadius * ss.x, i2 );
                int      r3 = SphereLineIntersection( v3, v1, sp, cut->Shapes[ i ].SphereRadius * ss.x, i3 );

                // Process intersections
                bool outside = ( r1 == 0 && r2 == 0 && r3 == 0 );
                bool ignore = ( r1 == -2 || r2 == -2 || r3 == -2 );
                int  sum = r1 + r2 + r3;
                if( !ignore && sum == 2 )                 // 1 1 0, corner in
                {
                    const Vertex3D& vv1 = ( r1 == 0 ? v1 : ( r2 == 0 ? v2 : v3 ) );
                    const Vertex3D& vv2 = ( r1 == 0 ? v2 : ( r2 == 0 ? v3 : v1 ) );
                    const Vertex3D& vv3 = ( r1 == 0 ? i3 : ( r2 == 0 ? i1 : i2 ) );
                    const Vertex3D& vv4 = ( r1 == 0 ? i2 : ( r2 == 0 ? i3 : i1 ) );

                    // First face
                    result_vertices.push_back( vv1 );
                    result_vertices.push_back( vv2 );
                    result_vertices.push_back( vv3 );
                    result_indices.push_back( (ushort) result_indices.size() );
                    result_indices.push_back( (ushort) result_indices.size() );
                    result_indices.push_back( (ushort) result_indices.size() );

                    // Second face
                    result_vertices.push_back( vv3 );
                    result_vertices.push_back( vv2 );
                    result_vertices.push_back( vv4 );
                    result_indices.push_back( (ushort) result_indices.size() );
                    result_indices.push_back( (ushort) result_indices.size() );
                    result_indices.push_back( (ushort) result_indices.size() );
                }
                else if( !ignore && sum == 1 )                 // 1 1 -1, corner out
                {
                    const Vertex3D& vv1 = ( r1 == -1 ? i3 : ( r2 == -1 ? v1 : i1 ) );
                    const Vertex3D& vv2 = ( r1 == -1 ? i2 : ( r2 == -1 ? i1 : v2 ) );
                    const Vertex3D& vv3 = ( r1 == -1 ? v3 : ( r2 == -1 ? i3 : i2 ) );

                    // One face
                    result_vertices.push_back( vv1 );
                    result_vertices.push_back( vv2 );
                    result_vertices.push_back( vv3 );
                    result_indices.push_back( (ushort) result_indices.size() );
                    result_indices.push_back( (ushort) result_indices.size() );
                    result_indices.push_back( (ushort) result_indices.size() );
                }
                else if( ignore || outside )
                {
                    if( ignore && sum == 0 )                     // 1 1 -2
                        continue;

                    result_vertices.push_back( v1 );
                    result_vertices.push_back( v2 );
                    result_vertices.push_back( v3 );
                    result_indices.push_back( (ushort) result_indices.size() );
                    result_indices.push_back( (ushort) result_indices.size() );
                    result_indices.push_back( (ushort) result_indices.size() );
                }
            }

            result_mesh_vertices.push_back( (uint) ( result_vertices.size() - vertices_old_size ) );
            result_mesh_indices.push_back( (uint) ( result_indices.size() - indices_old_size ) );
        }
        combined_mesh->Vertices = result_vertices;
        combined_mesh->Indices = result_indices;
        combined_mesh->MeshVertices = result_mesh_vertices;
        combined_mesh->MeshIndices = result_mesh_indices;
    }

    // Unskin
    if( cut->UnskinBone )
    {
        // Find unskin bone
        Bone* unskin_bone = nullptr;
        for( size_t i = 0; i < combined_mesh->CurBoneMatrix; i++ )
        {
            if( combined_mesh->SkinBones[ i ]->NameHash == cut->UnskinBone )
            {
                unskin_bone = combined_mesh->SkinBones[ i ];
                break;
            }
        }

        // Unskin
        if( unskin_bone )
        {
            // Process meshes
            size_t v_pos = 0, v_count = 0;
            for( size_t i = 0, j = combined_mesh->MeshVertices.size(); i < j; i++ )
            {
                // Move shape to face space
                Matrix     mesh_transform = combined_mesh->Meshes[ i ]->Owner->GlobalTransformationMatrix;
                Matrix     sm = mesh_transform.Inverse() * cut->UnskinShape.GlobalTransformationMatrix;
                Vector     ss, sp;
                Quaternion sr;
                sm.Decompose( ss, sr, sp );
                float      sphere_square_radius = powf( cut->UnskinShape.SphereRadius * ss.x, 2.0f );

                // Process mesh vertices
                v_count += combined_mesh->MeshVertices[ i ];
                for( ; v_pos < v_count; v_pos++ )
                {
                    Vertex3D& v = combined_mesh->Vertices[ v_pos ];

                    // Get vertex side
                    bool v_side = ( ( v.Position - sp ).SquareLength() <= sphere_square_radius );

                    // Check influences
                    for( int b = 0; b < BONES_PER_VERTEX; b++ )
                    {
                        // No influence
                        float w = v.BlendWeights[ b ];
                        if( w <= 0.0f )
                            continue;

                        // Last influence, don't reskin
                        if( w >= 1.0f )
                            break;

                        // Skip equal influence side
                        bool influence_side = ( unskin_bone->Find( combined_mesh->SkinBones[ (int) v.BlendIndices[ b ] ]->NameHash ) != nullptr );
                        if( v_side == influence_side )
                            continue;

                        // Move influence to other bones
                        v.BlendWeights[ b ] = 0.0f;
                        for( int b2 = 0; b2 < BONES_PER_VERTEX; b2++ )
                            v.BlendWeights[ b2 ] += v.BlendWeights[ b2 ] / ( 1.0f - w ) * w;
                    }
                }
            }
        }
    }
}

bool Animation3d::NeedDraw()
{
    return combinedMeshesSize && ( !lastDrawTick || GetTick() - lastDrawTick >= AnimDelay );
}

void Animation3d::Draw( int x, int y )
{
    // Skip drawing if no meshes generated
    if( !combinedMeshesSize )
        return;

    // Move timer
    uint  tick = GetTick();
    float elapsed = ( lastDrawTick ? 0.001f * (float) ( tick - lastDrawTick ) : 0.0f );
    lastDrawTick = tick;

    // Move animation
    ProcessAnimation( elapsed, x ? x : ModeWidth / 2, y ? y : ModeHeight - ModeHeight / 4, 1.0f );

    // Draw mesh
    DrawCombinedMeshes();
}

void Animation3d::ProcessAnimation( float elapsed, int x, int y, float scale )
{
    // Update world matrix, only for root
    if( !parentBone )
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
    UpdateBoneMatrices( animEntity->xFile->rootBone, &parentMatrix );

    // Update linked matrices
    if( parentBone && linkBones.size() )
    {
        for( uint i = 0, j = (uint) linkBones.size() / 2; i < j; i++ )
            // UpdateBoneMatrices(linkBones[i*2+1],&linkMatricles[i]);
            // linkBones[i*2+1]->exCombinedTransformationMatrix=linkMatricles[i];
            linkBones[ i * 2 + 1 ]->CombinedTransformationMatrix = linkBones[ i * 2 ]->CombinedTransformationMatrix;
    }

    // Update world matrices for children
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
    {
        Animation3d* child = *it;
        child->groundPos = groundPos;
        child->parentMatrix = child->parentBone->CombinedTransformationMatrix * child->matTransBase * child->matRotBase * child->matScaleBase;
    }

    // Move child animations
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
        ( *it )->ProcessAnimation( elapsed, x, y, 1.0f );
}

void Animation3d::UpdateBoneMatrices( Bone* bone, const Matrix* parent_matrix )
{
    // If parent matrix exists multiply our bone matrix by it
    bone->CombinedTransformationMatrix = ( *parent_matrix ) * bone->TransformationMatrix;

    // Update child
    for( auto it = bone->Children.begin(), end = bone->Children.end(); it != end; ++it )
        UpdateBoneMatrices( *it, &bone->CombinedTransformationMatrix );
}

void Animation3d::DrawCombinedMeshes()
{
    if( !disableCulling )
        GL( glEnable( GL_CULL_FACE ) );
    GL( glEnable( GL_DEPTH_TEST ) );

    for( size_t i = 0; i < combinedMeshesSize; i++ )
        DrawCombinedMesh( combinedMeshes[ i ], shadowDisabled || animEntity->shadowDisabled );

    if( !disableCulling )
        GL( glDisable( GL_CULL_FACE ) );
    GL( glDisable( GL_DEPTH_TEST ) );
}

void Animation3d::DrawCombinedMesh( CombinedMesh* combined_mesh, bool shadow_disabled )
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
        GL( glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoord ) ) );
        GL( glVertexAttribPointer( 3, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, TexCoordBase ) ) );
        GL( glVertexAttribPointer( 4, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Tangent ) ) );
        GL( glVertexAttribPointer( 5, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, Bitangent ) ) );
        GL( glVertexAttribPointer( 6, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, BlendWeights ) ) );
        GL( glVertexAttribPointer( 7, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex3D ), (void*) (size_t) OFFSETOF( Vertex3D, BlendIndices ) ) );
        for( uint i = 0; i <= 7; i++ )
            GL( glEnableVertexAttribArray( i ) );
    }

    Effect*       effect = ( combined_mesh->DrawEffect ? combined_mesh->DrawEffect : Effect::Skinned3d );
    MeshTexture** textures = combined_mesh->Textures;

    bool          matrices_combined = false;
    for( size_t pass = 0; pass < effect->Passes.size(); pass++ )
    {
        EffectPass& effect_pass = effect->Passes[ pass ];

        if( shadow_disabled && effect_pass.IsShadow )
            continue;

        GL( glUseProgram( effect_pass.Program ) );

        if( IS_EFFECT_VALUE( effect_pass.ZoomFactor ) )
            GL( glUniform1f( effect_pass.ZoomFactor, GameOpt.SpritesZoom ) );
        if( IS_EFFECT_VALUE( effect_pass.ProjectionMatrix ) )
            GL( glUniformMatrix4fv( effect_pass.ProjectionMatrix, 1, GL_FALSE, MatrixProjCM[ 0 ] ) );
        if( IS_EFFECT_VALUE( effect_pass.ColorMap ) && textures[ 0 ] )
        {
            GL( glBindTexture( GL_TEXTURE_2D, textures[ 0 ]->Id ) );
            GL( glUniform1i( effect_pass.ColorMap, 0 ) );
            if( IS_EFFECT_VALUE( effect_pass.ColorMapSize ) )
                GL( glUniform4fv( effect_pass.ColorMapSize, 1, textures[ 0 ]->SizeData ) );
        }
        if( IS_EFFECT_VALUE( effect_pass.LightColor ) )
            GL( glUniform4fv( effect_pass.LightColor, 1, (float*) &LightColor ) );
        if( IS_EFFECT_VALUE( effect_pass.WorldMatrices ) )
        {
            if( !matrices_combined )
            {
                matrices_combined = true;
                for( size_t i = 0; i < combined_mesh->CurBoneMatrix; i++ )
                {
                    WorldMatrices[ i ] = combined_mesh->SkinBones[ i ]->CombinedTransformationMatrix * combined_mesh->SkinBoneOffsets[ i ];
                    WorldMatrices[ i ].Transpose();                                                             // Convert to column major order
                }
            }
            GL( glUniformMatrix4fv( effect_pass.WorldMatrices, combined_mesh->CurBoneMatrix, GL_FALSE, (float*) &WorldMatrices[ 0 ] ) );
        }
        if( IS_EFFECT_VALUE( effect_pass.GroundPosition ) )
            GL( glUniform3fv( effect_pass.GroundPosition, 1, (float*) &groundPos ) );

        if( effect_pass.IsNeedProcess )
            GraphicLoader::EffectProcessVariables( effect_pass, true, animPosProc, animPosTime, textures );

        GL( glDrawElements( GL_TRIANGLES, (uint) combined_mesh->Indices.size(), GL_UNSIGNED_SHORT, (void*) 0 ) );

        if( effect_pass.IsNeedProcess )
            GraphicLoader::EffectProcessVariables( effect_pass, false, animPosProc, animPosTime, textures );
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

bool Animation3d::StartUp()
{
    // Calculate max effect bones
    GLint max_uniform_components = 0;
    GL( glGetIntegerv( GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_uniform_components ) );
    if( max_uniform_components < 1024 )
        WriteLog( "Warning! GL_MAX_VERTEX_UNIFORM_COMPONENTS is %u.\n", max_uniform_components );
    Effect::MaxBones = MIN( MAX( max_uniform_components, 1024 ) / 16, 256 ) - 4;
    RUNTIME_ASSERT( Effect::MaxBones >= MAX_BONES_PER_MODEL );
    WorldMatrices.resize( Effect::MaxBones );

    // Check effects
    if( !GraphicLoader::Load3dEffects() )
        return false;

    // Smoothing
    MoveTransitionTime = GameOpt.Animation3dSmoothTime / 1000.0f;
    if( MoveTransitionTime < 0.001f )
        MoveTransitionTime = 0.001f;

    // 2D rendering time
    if( GameOpt.Animation3dFPS )
        AnimDelay = 1000 / GameOpt.Animation3dFPS;
    else
        AnimDelay = 0;

    return true;
}

void Animation3d::SetScreenSize( int width, int height )
{
    if( width == ModeWidth && height == ModeHeight )
        return;

    // Build orthogonal projection
    ModeWidth = width;
    ModeHeight = height;
    ModeWidthF = (float) ModeWidth;
    ModeHeightF = (float) ModeHeight;

    // Projection
    float k = (float) ModeHeight / 768.0f;
    gluStuffOrtho( MatrixProjRM[ 0 ], 0.0f, 18.65f * k * ModeWidthF / ModeHeightF, 0.0f, 18.65f * k, -10.0f, 10.0f );
    MatrixProjCM = MatrixProjRM;
    MatrixProjCM.Transpose();
    MatrixEmptyRM = Matrix();
    MatrixEmptyCM = MatrixEmptyRM;
    MatrixEmptyCM.Transpose();

    // View port
    GL( glViewport( 0, 0, ModeWidth, ModeHeight ) );
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

Animation3d* Animation3d::GetAnimation( const char* name, int path_type, bool is_child )
{
    char fname[ MAX_FOPATH ];
    FileManager::GetDataPath( name, path_type, fname );
    return GetAnimation( fname, is_child );
}

Animation3d* Animation3d::GetAnimation( const char* name, bool is_child )
{
    Animation3dEntity* entity = Animation3dEntity::GetEntity( name );
    if( !entity )
        return nullptr;
    Animation3d* anim3d = entity->CloneAnimation();
    if( !anim3d )
        return nullptr;

    // Create mesh instances
    anim3d->allMeshes.resize( entity->xFile->allDrawBones.size() );
    anim3d->allMeshesDisabled.resize( anim3d->allMeshes.size() );
    for( size_t i = 0, j = entity->xFile->allDrawBones.size(); i < j; i++ )
    {
        MeshInstance& mesh_instance = anim3d->allMeshes[ i ];
        MeshData*     mesh = entity->xFile->allDrawBones[ i ]->Mesh;
        memzero( &mesh_instance, sizeof( mesh_instance ) );
        mesh_instance.Mesh = mesh;
        const char* tex_name = ( mesh->DiffuseTexture.length() ? mesh->DiffuseTexture.c_str() : nullptr );
        mesh_instance.CurTexures[ 0 ] = mesh_instance.DefaultTexures[ 0 ] = ( tex_name ? entity->xFile->GetTexture( tex_name ) : nullptr );
        mesh_instance.CurEffect = mesh_instance.DefaultEffect = ( mesh->DrawEffect.EffectFilename ? entity->xFile->GetEffect( &mesh->DrawEffect ) : nullptr );
    }

    // Set default data
    anim3d->SetAnimation( 0, 0, nullptr, ANIMATION_INIT );

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
    pos.z = 0.0f;
    return pos;
}

Point Animation3d::Convert3dTo2d( Vector pos )
{
    Vector coords;
    VecProject( pos, coords );
    return Point( (int) coords.x, (int) coords.y );
}

/************************************************************************/
/* Animation3dEntity                                                    */
/************************************************************************/

Animation3dEntityVec Animation3dEntity::allEntities;

Animation3dEntity::Animation3dEntity(): xFile( nullptr ), animController( nullptr ),
                                        renderAnim( 0 ), renderAnimProcFrom( 0 ), renderAnimProcTo( 100 ), renderAnimDir( 0 ),
                                        shadowDisabled( false ), drawWidth( DEFAULT_DRAW_SIZE ), drawHeight( DEFAULT_DRAW_SIZE )
{
    memzero( &animDataDefault, sizeof( animDataDefault ) );
}

Animation3dEntity::~Animation3dEntity()
{
    animData.push_back( animDataDefault );
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
        SAFEDELA( link.Cut );
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
        if( !fo3d.LoadFile( name, PT_CLIENT_DATA ) )
            return false;
        char* big_buf = Str::GetBigBuf();
        fo3d.CopyMem( big_buf, fo3d.GetFsize() );
        big_buf[ fo3d.GetFsize() ] = 0;

        // Extract file path
        char path[ MAX_FOPATH ];
        FileManager::ExtractDir( name, path );

        // Parse
        char             model[ MAX_FOPATH ] = { 0 };
        char             render_fname[ MAX_FOPATH ] = { 0 };
        char             render_anim[ MAX_FOPATH ] = { 0 };
        vector< size_t > anim_indexes;
        bool             disable_animation_interpolation = false;
        bool             convert_value_fail = false;

        uint             mesh = 0;
        int              layer = -1;
        int              layer_val = 0;
        EffectInstance*  cur_effect = nullptr;

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
                if( Str::Compare( token, "ContinueParsing" ) )
                    closed = false;
                continue;
            }

            if( Str::Compare( token, "StopParsing" ) )
            {
                closed = true;
            }
            else if( Str::Compare( token, "Model" ) )
            {
                ( *istr ) >> buf;
                FileManager::MakeFilePath( buf, path, model );
            }
            else if( Str::Compare( token, "Include" ) )
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
                if( !fo3d_ex.LoadFile( fname, PT_CLIENT_DATA ) )
                {
                    WriteLogF( _FUNC_, " - Include file '%s' not found.\n", fname );
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
            else if( Str::Compare( token, "Root" ) )
            {
                if( layer == -1 )
                {
                    link = &animDataDefault;
                }
                else if( !layer_val )
                {
                    WriteLogF( _FUNC_, " - Wrong layer '%d' zero value.\n", layer );
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
            else if( Str::Compare( token, "Mesh" ) )
            {
                ( *istr ) >> buf;
                if( !Str::Compare( buf, "All" ) )
                    mesh = Bone::GetHash( buf );
                else
                    mesh = 0;
            }
            else if( Str::Compare( token, "Subset" ) )
            {
                ( *istr ) >> buf;
                WriteLogF( _FUNC_, " - Tag 'Subset' obsolete, use 'Mesh' instead.\n" );
            }
            else if( Str::Compare( token, "Layer" ) || Str::Compare( token, "Value" ) )
            {
                ( *istr ) >> buf;
                if( Str::Compare( token, "Layer" ) )
                    layer = (int) ConvertParamValue( buf, convert_value_fail );
                else
                    layer_val = (int) ConvertParamValue( buf, convert_value_fail );

                link = &dummy_link;
                mesh = 0;
            }
            else if( Str::Compare( token, "Attach" ) )
            {
                ( *istr ) >> buf;
                if( layer < 0 || !layer_val )
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
            else if( Str::Compare( token, "Link" ) )
            {
                ( *istr ) >> buf;
                if( link->Id )
                    link->LinkBoneHash = Bone::GetHash( buf );
            }
            else if( Str::Compare( token, "Cut" ) )
            {
                ( *istr ) >> buf;
                char              fname[ MAX_FOPATH ];
                FileManager::MakeFilePath( buf, path, fname );
                Animation3dXFile* area = Animation3dXFile::GetXFile( fname );
                if( area )
                {
                    // Add cut
                    CutData*  cut = new CutData();
                    CutData** tmp = link->Cut;
                    link->Cut = new CutData*[ link->CutCount + 1 ];
                    for( uint i = 0; i < link->CutCount; i++ )
                        link->Cut[ i ] = tmp[ i ];
                    SAFEDELA( tmp );
                    link->Cut[ link->CutCount ] = cut;
                    link->CutCount++;

                    // Layers
                    ( *istr ) >> buf;
                    StrVec layers;
                    Str::ParseLine( buf, '-', layers, Str::ParseLineDummy );
                    for( uint m = 0, n = (uint) layers.size(); m < n; m++ )
                    {
                        int layer = (int) ConvertParamValue( layers[ m ].c_str(), convert_value_fail );
                        if( Str::Compare( layers[ m ].c_str(), "All" ) )
                            layer = -1;
                        cut->Layers.push_back( layer );
                    }

                    // Shapes
                    ( *istr ) >> buf;
                    StrVec shapes;
                    Str::ParseLine( buf, '-', shapes, Str::ParseLineDummy );
                    for( uint m = 0, n = (uint) shapes.size(); m < n; m++ )
                    {
                        uint shape_name = Bone::GetHash( shapes[ m ].c_str() );
                        if( Str::Compare( shapes[ m ].c_str(), "All" ) )
                            shape_name = 0;
                        for( size_t k = 0, l = area->allDrawBones.size(); k < l; k++ )
                        {
                            if( !shape_name || shape_name == area->allDrawBones[ k ]->NameHash )
                                cut->Shapes.push_back( CutShape::Make( area->allDrawBones[ k ]->Mesh ) );
                        }
                    }

                    // Unskin bone
                    ( *istr ) >> buf;
                    cut->UnskinBone = Bone::GetHash( buf );
                    if( Str::Compare( buf, "-" ) )
                        cut->UnskinBone = 0;

                    // Unskin shape
                    ( *istr ) >> buf;
                    if( cut->UnskinBone )
                    {
                        uint unskin_shape_name = Bone::GetHash( buf );
                        for( size_t k = 0, l = area->allDrawBones.size(); k < l; k++ )
                        {
                            if( unskin_shape_name == area->allDrawBones[ k ]->NameHash )
                                cut->UnskinShape = CutShape::Make( area->allDrawBones[ k ]->Mesh );
                        }
                    }
                }
                else
                {
                    WriteLogF( _FUNC_, " - Cut file '%s' not found.\n", fname );
                    ( *istr ) >> buf;
                    ( *istr ) >> buf;
                    ( *istr ) >> buf;
                    ( *istr ) >> buf;
                }
            }
            else if( Str::Compare( token, "RotX" ) )
                ( *istr ) >> link->RotX;
            else if( Str::Compare( token, "RotY" ) )
                ( *istr ) >> link->RotY;
            else if( Str::Compare( token, "RotZ" ) )
                ( *istr ) >> link->RotZ;
            else if( Str::Compare( token, "MoveX" ) )
                ( *istr ) >> link->MoveX;
            else if( Str::Compare( token, "MoveY" ) )
                ( *istr ) >> link->MoveY;
            else if( Str::Compare( token, "MoveZ" ) )
                ( *istr ) >> link->MoveZ;
            else if( Str::Compare( token, "ScaleX" ) )
                ( *istr ) >> link->ScaleX;
            else if( Str::Compare( token, "ScaleY" ) )
                ( *istr ) >> link->ScaleY;
            else if( Str::Compare( token, "ScaleZ" ) )
                ( *istr ) >> link->ScaleZ;
            else if( Str::Compare( token, "Scale" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleX = link->ScaleY = link->ScaleZ = valuef;
            }
            else if( Str::Compare( token, "Speed" ) )
                ( *istr ) >> link->SpeedAjust;
            else if( Str::Compare( token, "RotX+" ) )
            {
                ( *istr ) >> valuef;
                link->RotX = ( link->RotX == 0.0f ? valuef : link->RotX + valuef );
            }
            else if( Str::Compare( token, "RotY+" ) )
            {
                ( *istr ) >> valuef;
                link->RotY = ( link->RotY == 0.0f ? valuef : link->RotY + valuef );
            }
            else if( Str::Compare( token, "RotZ+" ) )
            {
                ( *istr ) >> valuef;
                link->RotZ = ( link->RotZ == 0.0f ? valuef : link->RotZ + valuef );
            }
            else if( Str::Compare( token, "MoveX+" ) )
            {
                ( *istr ) >> valuef;
                link->MoveX = ( link->MoveX == 0.0f ? valuef : link->MoveX + valuef );
            }
            else if( Str::Compare( token, "MoveY+" ) )
            {
                ( *istr ) >> valuef;
                link->MoveY = ( link->MoveY == 0.0f ? valuef : link->MoveY + valuef );
            }
            else if( Str::Compare( token, "MoveZ+" ) )
            {
                ( *istr ) >> valuef;
                link->MoveZ = ( link->MoveZ == 0.0f ? valuef : link->MoveZ + valuef );
            }
            else if( Str::Compare( token, "ScaleX+" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleX = ( link->ScaleX == 0.0f ? valuef : link->ScaleX + valuef );
            }
            else if( Str::Compare( token, "ScaleY+" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleY = ( link->ScaleY == 0.0f ? valuef : link->ScaleY + valuef );
            }
            else if( Str::Compare( token, "ScaleZ+" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleZ = ( link->ScaleZ == 0.0f ? valuef : link->ScaleZ + valuef );
            }
            else if( Str::Compare( token, "Scale+" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleX = ( link->ScaleX == 0.0f ? valuef : link->ScaleX + valuef );
                link->ScaleY = ( link->ScaleY == 0.0f ? valuef : link->ScaleY + valuef );
                link->ScaleZ = ( link->ScaleZ == 0.0f ? valuef : link->ScaleZ + valuef );
            }
            else if( Str::Compare( token, "Speed+" ) )
            {
                ( *istr ) >> valuef;
                link->SpeedAjust = ( link->SpeedAjust == 0.0f ? valuef : link->SpeedAjust * valuef );
            }
            else if( Str::Compare( token, "RotX*" ) )
            {
                ( *istr ) >> valuef;
                link->RotX = ( link->RotX == 0.0f ? valuef : link->RotX * valuef );
            }
            else if( Str::Compare( token, "RotY*" ) )
            {
                ( *istr ) >> valuef;
                link->RotY = ( link->RotY == 0.0f ? valuef : link->RotY * valuef );
            }
            else if( Str::Compare( token, "RotZ*" ) )
            {
                ( *istr ) >> valuef;
                link->RotZ = ( link->RotZ == 0.0f ? valuef : link->RotZ * valuef );
            }
            else if( Str::Compare( token, "MoveX*" ) )
            {
                ( *istr ) >> valuef;
                link->MoveX = ( link->MoveX == 0.0f ? valuef : link->MoveX * valuef );
            }
            else if( Str::Compare( token, "MoveY*" ) )
            {
                ( *istr ) >> valuef;
                link->MoveY = ( link->MoveY == 0.0f ? valuef : link->MoveY * valuef );
            }
            else if( Str::Compare( token, "MoveZ*" ) )
            {
                ( *istr ) >> valuef;
                link->MoveZ = ( link->MoveZ == 0.0f ? valuef : link->MoveZ * valuef );
            }
            else if( Str::Compare( token, "ScaleX*" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleX = ( link->ScaleX == 0.0f ? valuef : link->ScaleX * valuef );
            }
            else if( Str::Compare( token, "ScaleY*" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleY = ( link->ScaleY == 0.0f ? valuef : link->ScaleY * valuef );
            }
            else if( Str::Compare( token, "ScaleZ*" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleZ = ( link->ScaleZ == 0.0f ? valuef : link->ScaleZ * valuef );
            }
            else if( Str::Compare( token, "Scale*" ) )
            {
                ( *istr ) >> valuef;
                link->ScaleX = ( link->ScaleX == 0.0f ? valuef : link->ScaleX * valuef );
                link->ScaleY = ( link->ScaleY == 0.0f ? valuef : link->ScaleY * valuef );
                link->ScaleZ = ( link->ScaleZ == 0.0f ? valuef : link->ScaleZ * valuef );
            }
            else if( Str::Compare( token, "Speed*" ) )
            {
                ( *istr ) >> valuef;
                link->SpeedAjust = ( link->SpeedAjust == 0.0f ? valuef : link->SpeedAjust * valuef );
            }
            else if( Str::Compare( token, "DisableLayer" ) )
            {
                ( *istr ) >> buf;
                StrVec layers;
                Str::ParseLine( buf, '-', layers, Str::ParseLineDummy );
                for( uint m = 0, n = (uint) layers.size(); m < n; m++ )
                {
                    int layer = (int) ConvertParamValue( layers[ m ].c_str(), convert_value_fail );
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
            else if( Str::Compare( token, "DisableSubset" ) )
            {
                ( *istr ) >> buf;
                WriteLogF( _FUNC_, " - Tag 'DisableSubset' obsolete, use 'DisableMesh' instead.\n" );
            }
            else if( Str::Compare( token, "DisableMesh" ) )
            {
                ( *istr ) >> buf;
                StrVec meshes;
                Str::ParseLine( buf, '-', meshes, Str::ParseLineDummy );
                for( uint m = 0, n = (uint) meshes.size(); m < n; m++ )
                {
                    uint mesh_name_hash = 0;
                    if( !Str::Compare( meshes[ m ].c_str(), "All" ) )
                        mesh_name_hash = Bone::GetHash( meshes[ m ].c_str() );
                    uint* tmp = link->DisabledMesh;
                    link->DisabledMesh = new uint[ link->DisabledMeshCount + 1 ];
                    for( uint h = 0; h < link->DisabledMeshCount; h++ )
                        link->DisabledMesh[ h ] = tmp[ h ];
                    SAFEDELA( tmp );
                    link->DisabledMesh[ link->DisabledMeshCount ] = mesh_name_hash;
                    link->DisabledMeshCount++;
                }
            }
            else if( Str::Compare( token, "Texture" ) )
            {
                ( *istr ) >> buf;
                int index = (int) ConvertParamValue( buf, convert_value_fail );
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
            else if( Str::Compare( token, "Effect" ) )
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
            else if( Str::Compare( token, "EffDef" ) )
            {
                char def_name[ MAX_FOTEXT ];
                char def_value[ MAX_FOTEXT ];
                ( *istr ) >> buf;
                ( *istr ) >> def_name;
                ( *istr ) >> def_value;

                if( !cur_effect )
                    continue;

                EffectDefault::EType type;
                uchar*               data = nullptr;
                uint                 data_len = 0;
                if( Str::Compare( buf, "String" ) )
                {
                    type = EffectDefault::String;
                    data = (uchar*) Str::Duplicate( def_value );
                    data_len = Str::Length( (char*) data );
                }
                else if( Str::Compare( buf, "Float" ) || Str::Compare( buf, "Floats" ) )
                {
                    type = EffectDefault::Float;
                    StrVec floats;
                    Str::ParseLine( def_value, '-', floats, Str::ParseLineDummy );
                    if( floats.empty() )
                        continue;
                    data_len = (uint) floats.size() * sizeof( float );
                    data = new uchar[ data_len ];
                    for( uint i = 0, j = (uint) floats.size(); i < j; i++ )
                        ( (float*) data )[ i ] = Str::AtoF( floats[ i ].c_str() );
                }
                else if( Str::Compare( buf, "Int" ) || Str::Compare( buf, "Dword" ) )
                {
                    type = EffectDefault::Int;
                    StrVec ints;
                    Str::ParseLine( def_value, '-', ints, Str::ParseLineDummy );
                    if( ints.empty() )
                        continue;
                    data_len = (uint) ints.size() * sizeof( int );
                    data = new uchar[ data_len ];
                    for( uint i = 0, j = (uint) ints.size(); i < j; i++ )
                        ( (int*) data )[ i ] = (int) ConvertParamValue( ints[ i ].c_str(), convert_value_fail );
                }
                else
                {
                    continue;
                }

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
            else if( Str::Compare( token, "Anim" ) || Str::Compare( token, "AnimSpeed" ) ||
                     Str::Compare( token, "AnimExt" ) || Str::Compare( token, "AnimSpeedExt" ) )
            {
                // Index animation
                int ind1 = 0, ind2 = 0;
                ( *istr ) >> buf;
                ind1 = (int) ConvertParamValue( buf, convert_value_fail );
                ( *istr ) >> buf;
                ind2 = (int) ConvertParamValue( buf, convert_value_fail );

                if( Str::Compare( token, "Anim" ) || Str::Compare( token, "AnimExt" ) )
                {
                    // Deferred loading
                    // Todo: Reverse play

                    anim_indexes.push_back( ( ind1 << 8 ) | ind2 );
                    ( *istr ) >> buf;
                    anim_indexes.push_back( ( size_t ) Str::Duplicate( buf ) );
                    ( *istr ) >> buf;
                    anim_indexes.push_back( ( size_t ) Str::Duplicate( buf ) );

                    if( Str::Compare( token, "AnimExt" ) )
                    {
                        anim_indexes.push_back( ( ind1 << 8 ) | ( ind2 | 0x80 ) );
                        ( *istr ) >> buf;
                        anim_indexes.push_back( ( size_t ) Str::Duplicate( buf ) );
                        ( *istr ) >> buf;
                        anim_indexes.push_back( ( size_t ) Str::Duplicate( buf ) );
                    }
                }
                else
                {
                    ( *istr ) >> valuef;
                    animSpeed.insert( PAIR( ( ind1 << 8 ) | ind2, valuef ) );

                    if( Str::Compare( token, "AnimSpeedExt" ) )
                    {
                        ( *istr ) >> valuef;
                        animSpeed.insert( PAIR( ( ind1 << 8 ) | ( ind2 | 0x80 ), valuef ) );
                    }
                }
            }
            else if( Str::Compare( token, "AnimLayerValue" ) )
            {
                int ind1 = 0, ind2 = 0;
                ( *istr ) >> buf;
                ind1 = (int) ConvertParamValue( buf, convert_value_fail );
                ( *istr ) >> buf;
                ind2 = (int) ConvertParamValue( buf, convert_value_fail );

                int layer = 0, value = 0;
                ( *istr ) >> buf;
                layer = (int) ConvertParamValue( buf, convert_value_fail );
                ( *istr ) >> buf;
                value = (int) ConvertParamValue( buf, convert_value_fail );

                uint index = ( ind1 << 16 ) | ind2;
                if( !animLayerValues.count( index ) )
                    animLayerValues.insert( PAIR( index, IntPairVec() ) );
                animLayerValues[ index ].push_back( IntPair( layer, value ) );
            }
            else if( Str::Compare( token, "FastTransitionBone" ) )
            {
                ( *istr ) >> buf;
                fastTransitionBones.insert( Bone::GetHash( buf ) );
            }
            else if( Str::Compare( token, "AnimEqual" ) )
            {
                ( *istr ) >> valuei;

                int ind1 = 0, ind2 = 0;
                ( *istr ) >> buf;
                ind1 = (int) ConvertParamValue( buf, convert_value_fail );
                ( *istr ) >> buf;
                ind2 = (int) ConvertParamValue( buf, convert_value_fail );

                if( valuei == 1 )
                    anim1Equals.insert( PAIR( ind1, ind2 ) );
                else if( valuei == 2 )
                    anim2Equals.insert( PAIR( ind1, ind2 ) );
            }
            else if( Str::Compare( token, "RenderFrame" ) || Str::Compare( token, "RenderFrames" ) )
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
                if( Str::Compare( token, "RenderFrames" ) )
                    ( *istr ) >> renderAnimProcTo;

                // Check
                renderAnimProcFrom = CLAMP( renderAnimProcFrom, 0, 100 );
                renderAnimProcTo = CLAMP( renderAnimProcTo, 0, 100 );
            }
            else if( Str::Compare( token, "RenderDir" ) )
            {
                ( *istr ) >> buf;

                renderAnimDir = (int) ConvertParamValue( buf, convert_value_fail );
            }
            else if( Str::Compare( token, "DisableShadow" ) )
            {
                shadowDisabled = true;
            }
            else if( Str::Compare( token, "DrawSize" ) )
            {
                int w = 0, h = 0;
                ( *istr ) >> buf;
                w = (int) ConvertParamValue( buf, convert_value_fail );
                ( *istr ) >> buf;
                h = (int) ConvertParamValue( buf, convert_value_fail );

                drawWidth = w;
                drawHeight = h;
            }
            else if( Str::Compare( token, "DisableAnimationInterpolation" ) )
            {
                disable_animation_interpolation = true;
            }
            else
            {
                WriteLogF( _FUNC_, " - Unknown token '%s' in file '%s'.\n", token, name );
            }
        }

        // Process pathes
        if( !model[ 0 ] )
        {
            WriteLogF( _FUNC_, " - 'Model' section not found in file '%s'.\n", name );
            return false;
        }

        // Check for correct param values
        if( convert_value_fail )
        {
            WriteLogF( _FUNC_, " - Invalid param values for file '%s'.\n", name );
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
                WriteLogF( _FUNC_, " - Unable to create animation controller, file '%s'.\n", name );
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
                if( Str::Compare( anim_fname, "ModelFile" ) )
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
                else
                {
                    // WriteLogF( _FUNC_, " - Animation '%s'/'%s' not found.\n", anim_path, anim_name );
                }

                delete[] anim_fname;
                delete[] anim_name;
            }

            numAnimationSets = animController->GetNumAnimationSets();
            if( numAnimationSets > 0 )
            {
                // Add animation bones, not included to base hierarchy
                // All bones linked with animation in SetupAnimationOutput
                for( uint i = 0; i < numAnimationSets; i++ )
                {
                    AnimSet*    set = animController->GetAnimationSet( i );
                    UIntVecVec& bones_hierarchy = set->GetBonesHierarchy();
                    for( size_t j = 0; j < bones_hierarchy.size(); j++ )
                    {
                        UIntVec& bone_hierarchy = bones_hierarchy[ j ];
                        Bone*    bone = xFile->rootBone;
                        for( size_t b = 1; b < bone_hierarchy.size(); b++ )
                        {
                            Bone* child = bone->Find( bone_hierarchy[ b ] );
                            if( !child )
                            {
                                child = new Bone();
                                child->NameHash = bone_hierarchy[ b ];
                                child->Mesh = nullptr;
                                bone->Children.push_back( child );
                            }
                            bone = child;
                        }
                    }
                }

                animController->SetInterpolation( !disable_animation_interpolation );
                xFile->SetupAnimationOutput( animController );
            }
            else
            {
                SAFEDEL( animController );
            }
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

int Animation3dEntity::GetAnimationIndex( uint& anim1, uint& anim2, float* speed, bool combat_first )
{
    // Find index
    int index = -1;
    if( combat_first )
        index = GetAnimationIndexEx( anim1, anim2 | 0x80, speed );
    if( index == -1 )
        index = GetAnimationIndexEx( anim1, anim2, speed );
    if( !combat_first && index == -1 )
        index = GetAnimationIndexEx( anim1, anim2 | 0x80, speed );
    if( index != -1 )
        return index;

    // Find substitute animation
    hash base_model_name = 0;
    uint anim1_base = anim1, anim2_base = anim2;
    #ifdef FONLINE_CLIENT
    while( index == -1 && Script::PrepareContext( ClientFunctions.CritterAnimationSubstitute, _FUNC_, "Anim" ) )
    #else // FONLINE_MAPPER
    while( index == -1 && Script::PrepareContext( MapperFunctions.CritterAnimationSubstitute, _FUNC_, "Anim" ) )
    #endif
    {
        hash model_name = base_model_name;
        uint anim1_ = anim1, anim2_ = anim2;
        Script::SetArgUInt( base_model_name );
        Script::SetArgUInt( anim1_base );
        Script::SetArgUInt( anim2_base );
        Script::SetArgAddress( &model_name );
        Script::SetArgAddress( &anim1 );
        Script::SetArgAddress( &anim2 );
        if( Script::RunPrepared() && Script::GetReturnedBool() && ( anim1 != anim1_ || anim2 != anim2_ ) )
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
    auto it2 = anim2Equals.find( anim2 & 0x7F );
    if( it2 != anim2Equals.end() )
        anim2 = ( ( *it2 ).second | ( anim2 & 0x80 ) );

    // Make index
    int ii = ( anim1 << 8 ) | anim2;

    // Speed
    if( speed )
    {
        auto it = animSpeed.find( ii );
        if( it != animSpeed.end() )
            *speed = it->second;
        else
            *speed = 1.0f;
    }

    // Find number of animation
    auto it = animIndexes.find( ii );
    if( it != animIndexes.end() )
        return it->second;

    return -1;
}

Animation3d* Animation3dEntity::CloneAnimation()
{
    // Create instance
    Animation3d* a3d = new Animation3d();
    if( !a3d )
        return nullptr;

    a3d->animEntity = this;

    if( animController )
        a3d->animController = animController->Clone();

    return a3d;
}

Animation3dEntity* Animation3dEntity::GetEntity( const char* name )
{
    // Try find instance
    Animation3dEntity* entity = nullptr;
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
            return nullptr;
        }

        allEntities.push_back( entity );
    }

    return entity;
}

/************************************************************************/
/* Animation3dXFile                                                     */
/************************************************************************/

Animation3dXFileVec Animation3dXFile::xFiles;

Animation3dXFile::Animation3dXFile(): rootBone( nullptr )
{}

Animation3dXFile::~Animation3dXFile()
{
    GraphicLoader::DestroyModel( rootBone );
    rootBone = nullptr;
}

Animation3dXFile* Animation3dXFile::GetXFile( const char* xname )
{
    Animation3dXFile* xfile = nullptr;

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
        Bone* root_bone = GraphicLoader::LoadModel( xname );
        if( !root_bone )
        {
            WriteLogF( _FUNC_, " - Unable to load 3d file '%s'.\n", xname );
            return nullptr;
        }

        xfile = new Animation3dXFile();
        if( !xfile )
        {
            WriteLogF( _FUNC_, " - Allocation fail, x file '%s'.\n", xname );
            return nullptr;
        }

        xfile->fileName = xname;
        xfile->rootBone = root_bone;
        xfile->SetupBones();

        xFiles.push_back( xfile );
    }

    return xfile;
}

void SetupBonesExt( multimap< uint, Bone* >& bones, Bone* bone, uint depth )
{
    bones.insert( PAIR( depth, bone ) );
    for( size_t i = 0, j = bone->Children.size(); i < j; i++ )
        SetupBonesExt( bones, bone->Children[ i ], depth + 1 );
}

void Animation3dXFile::SetupBones()
{
    multimap< uint, Bone* > bones;
    SetupBonesExt( bones, rootBone, 0 );

    for( auto it = bones.begin(), end = bones.end(); it != end; ++it )
    {
        Bone* bone = it->second;
        allBones.push_back( bone );
        if( bone->Mesh )
            allDrawBones.push_back( bone );
    }
}

void SetupAnimationOutputExt( AnimController* anim_controller, Bone* bone )
{
    anim_controller->RegisterAnimationOutput( bone->NameHash, bone->TransformationMatrix );

    for( auto it = bone->Children.begin(), end = bone->Children.end(); it != end; ++it )
        SetupAnimationOutputExt( anim_controller, *it );
}

void Animation3dXFile::SetupAnimationOutput( AnimController* anim_controller )
{
    SetupAnimationOutputExt( anim_controller, rootBone );
}

MeshTexture* Animation3dXFile::GetTexture( const char* tex_name )
{
    MeshTexture* texture = GraphicLoader::LoadTexture( tex_name, fileName.c_str() );
    if( !texture )
        WriteLogF( _FUNC_, " - Can't load texture '%s'.\n", tex_name ? tex_name : "nullptr" );
    return texture;
}

Effect* Animation3dXFile::GetEffect( EffectInstance* effect_inst )
{
    Effect* effect = GraphicLoader::LoadEffect( effect_inst->EffectFilename, false, nullptr, fileName.c_str(), effect_inst->Defaults, effect_inst->DefaultsCount );
    if( !effect )
        WriteLogF( _FUNC_, " - Can't load effect '%s'.\n", effect_inst && effect_inst->EffectFilename ? effect_inst->EffectFilename : "nullptr" );
    return effect;
}
