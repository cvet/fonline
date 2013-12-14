#include "StdAfx.h"
#include "3dStuff.h"
#include "3dAnimation.h"
#include "GraphicLoader.h"
#include "Common.h"
#include "Text.h"
#include "ConstantsManager.h"
#include "Script.h"
#include "CritterType.h"

int       ModeWidth = 0, ModeHeight = 0;
float     FixedZ = 0.0f;
Matrix    MatrixProjRM, MatrixEmptyRM, MatrixProjCM, MatrixEmptyCM; // Row or Column major order
float     MoveTransitionTime = 0.25f;
float     GlobalSpeedAdjust = 1.0f;
MatrixVec BoneMatrices;
bool      SoftwareSkinning = false;
uint      AnimDelay = 0;

#ifdef SHADOW_MAP
GLuint FBODepth = 0;
GLuint DepthTexId = 0;
#endif

void VecProject( const Vector& v, Vector& out )
{
    int   viewport[ 4 ] = { 0, 0, ModeWidth, ModeHeight };
    float x = 0.0f, y = 0.0f, z = 0.0f;
    gluStuffProject( v.x, v.y, v.z, MatrixEmptyCM[ 0 ], MatrixProjCM[ 0 ], viewport, &x, &y, &z );
    out.x = x;
    out.y = y;
    out.z = z;
    out.z = 0.0f;
}

void VecUnproject( const Vector& v, Vector& out )
{
    int   viewport[ 4 ] = { 0, 0, ModeWidth, ModeHeight };
    float x = 0.0f, y = 0.0f, z = 0.0f;
    gluStuffUnProject( v.x, (float) ModeHeight - v.y, v.z, MatrixEmptyCM[ 0 ], MatrixProjCM[ 0 ], viewport, &x, &y, &z );
    out.x = x;
    out.y = y;
    out.z = z;
    out.z = 0.0f;
}

/************************************************************************/
/* Animation3d                                                          */
/************************************************************************/

Animation3dVec Animation3d::generalAnimations;

Animation3d::Animation3d(): animEntity( NULL ), animController( NULL ),
                            currentTrack( 0 ), lastTick( 0 ), endTick( 0 ), speedAdjustBase( 1.0f ), speedAdjustCur( 1.0f ), speedAdjustLink( 1.0f ),
                            shadowDisabled( false ), dirAngle( GameOpt.MapHexagonal ? 150.0f : 135.0f ), sprId( 0 ),
                            drawScale( 0.0f ), bordersDisabled( false ), calcBordersTick( 0 ), noDraw( true ), parentAnimation( NULL ), parentFrame( NULL ),
                            childChecker( true ), useGameTimer( true ), animPosProc( 0.0f ), animPosTime( 0.0f ), animPosPeriod( 0.0f )
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

    auto it = std::find( generalAnimations.begin(), generalAnimations.end(), this );
    if( it != generalAnimations.end() )
        generalAnimations.erase( it );

    for( auto it = meshOpt.begin(), end = meshOpt.end(); it != end; ++it )
    {
        MeshOptions& mopt = *it;
        SAFEDELA( mopt.DisabledSubsets );
        SAFEDELA( mopt.TexSubsets );
        SAFEDELA( mopt.DefaultTexSubsets );
    }
    meshOpt.clear();
}

// Handles transitions between animations to make it smooth and not a sudden jerk to a new position
void Animation3d::SetAnimation( uint anim1, uint anim2, int* layers, int flags )
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

    // Check changes indices or not
    if( !layers )
        layers = currentLayers;
    bool layer_changed = false;
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
        return;

    memcpy( currentLayers, layers, sizeof( int ) * LAYERS3D_COUNT );
    currentLayers[ LAYERS3D_COUNT ] = action;

    if( layer_changed )
    {
        // Set base data
        SetAnimData( this, animEntity->animDataDefault, true );

        // Append linked data
        if( parentFrame )
            SetAnimData( this, animLink, false );

        // Mark animations as unused
        for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
            ( *it )->childChecker = false;

        // Get unused layers and subsets
        bool unused_layers[ LAYERS3D_COUNT ] = { 0 };
        for( int i = 0; i < LAYERS3D_COUNT; i++ )
        {
            if( layers[ i ] == 0 )
                continue;

            for( auto it = animEntity->animData.begin(), end = animEntity->animData.end(); it != end; ++it )
            {
                AnimParams& link = *it;
                if( link.Layer == i && link.LayerValue == layers[ i ] && !link.ChildFName )
                {
                    for( uint j = 0; j < link.DisabledLayersCount; j++ )
                    {
                        unused_layers[ link.DisabledLayers[ j ] ] = true;
                    }
                    for( uint j = 0; j < link.DisabledSubsetsCount; j++ )
                    {
                        int disabled_subsets_mesh = link.DisabledSubsetsMesh[ j ];
                        int disabled_subsets = link.DisabledSubsets[ j ];
                        if( disabled_subsets_mesh < 0 )
                        {
                            for( auto it_ = meshOpt.begin(), end_ = meshOpt.end(); it_ != end_; ++it_ )
                                if( (uint) disabled_subsets < ( *it_ ).SubsetsCount )
                                    ( *it_ ).DisabledSubsets[ disabled_subsets ] = true;
                        }
                        else if( (uint) disabled_subsets_mesh < meshOpt.size() )
                        {
                            if( (uint) disabled_subsets < meshOpt[ disabled_subsets_mesh ].SubsetsCount )
                                meshOpt[ disabled_subsets_mesh ].DisabledSubsets[ disabled_subsets ] = true;
                        }
                    }
                }
            }
        }

        if( parentFrame )
        {
            for( uint j = 0; j < animLink.DisabledLayersCount; j++ )
            {
                unused_layers[ animLink.DisabledLayers[ j ] ] = true;
            }
            for( uint j = 0; j < animLink.DisabledSubsetsCount; j++ )
            {
                int disabled_subsets_mesh = animLink.DisabledSubsetsMesh[ j ];
                int disabled_subsets = animLink.DisabledSubsets[ j ];
                if( disabled_subsets_mesh < 0 )
                {
                    for( auto it_ = meshOpt.begin(), end_ = meshOpt.end(); it_ != end_; ++it_ )
                        if( (uint) disabled_subsets < ( *it_ ).SubsetsCount )
                            ( *it_ ).DisabledSubsets[ disabled_subsets ] = true;
                }
                else if( (uint) disabled_subsets_mesh < meshOpt.size() )
                {
                    if( (uint) disabled_subsets < meshOpt[ disabled_subsets_mesh ].SubsetsCount )
                        meshOpt[ disabled_subsets_mesh ].DisabledSubsets[ disabled_subsets ] = true;
                }
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
                        SetAnimData( this, link, false );
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

                        // Link to main frame
                        if( link.LinkBone )
                        {
                            Frame* to_frame = animEntity->xFile->frameRoot->Find( link.LinkBone );
                            if( to_frame )
                            {
                                anim3d = Animation3d::GetAnimation( link.ChildFName, true );
                                if( anim3d )
                                {
                                    anim3d->parentAnimation = this;
                                    anim3d->parentFrame = to_frame;
                                    anim3d->animLink = link;
                                    SetAnimData( anim3d, link, false );
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
                                for( auto it = anim3d->animEntity->xFile->allFrames.begin(), end = anim3d->animEntity->xFile->allFrames.end(); it != end; ++it )
                                {
                                    Frame*      child_frame = *it;
                                    const char* child_name = child_frame->GetName();
                                    if( child_name && child_name[ 0 ] )
                                    {
                                        Frame* root_frame = animEntity->xFile->frameRoot->Find( child_name );
                                        if( root_frame )
                                        {
                                            anim3d->linkFrames.push_back( root_frame );
                                            anim3d->linkFrames.push_back( child_frame );
                                        }
                                    }
                                }

                                anim3d->parentAnimation = this;
                                anim3d->parentFrame = animEntity->xFile->frameRoot;
                                anim3d->animLink = link;
                                SetAnimData( anim3d, link, false );
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
                delete anim3d;
                it = childAnimations.erase( it );
            }
            else
                ++it;
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

        // Borders
        if( !parentAnimation )
        {
            if( FLAG( flags, ANIMATION_ONE_TIME ) )
                calcBordersTick = endTick;
            else
                calcBordersTick = tick + uint( smooth_time * 1000.0f );
        }
    }

    // Set animation for children
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
    {
        Animation3d* child = *it;
        child->SetAnimation( anim1, anim2, layers, flags );
    }

    // Calculate borders
    SetupBorders();
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
    if( noDraw )
        return false;
    Rect borders = GetExtraBorders();
    if( x < borders.L || x > borders.R || y < borders.T || y > borders.B )
        return false;

    Vector ray_origin, ray_dir;
    VecUnproject( Vector( (float) x, (float) y, 0.0f ), ray_origin );
    VecUnproject( Vector( (float) x, (float) y, FixedZ ), ray_dir );

    // Main
    FrameMove( 0.0, drawXY.X, drawXY.Y, drawScale, true );
    if( IsIntersectFrame( animEntity->xFile->frameRoot, ray_origin, ray_dir, (float) x, (float) y ) )
        return true;
    // Children
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
    {
        Animation3d* child = *it;
        child->FrameMove( 0.0, drawXY.X, drawXY.Y, 1.0f, true );
        if( IsIntersectFrame( child->animEntity->xFile->frameRoot, ray_origin, ray_dir, (float) x, (float) y ) )
            return true;
    }
    return false;
}

bool Animation3d::IsIntersectFrame( Frame* frame, const Vector& ray_origin, const Vector& ray_dir, float x, float y )
{
    for( auto it = frame->Mesh.begin(), end = frame->Mesh.end(); it != end; ++it )
    {
        MeshSubset& ms = *it;
        if( !ms.VerticesTransformedValid )
            continue;

        for( size_t i = 0, j = ms.Indicies.size(); i < j; i += 3 )
        {
            Vector& c1 = ms.VerticesTransformed[ ms.Indicies[ i + 0 ] ].Position;
            Vector& c2 = ms.VerticesTransformed[ ms.Indicies[ i + 1 ] ].Position;
            Vector& c3 = ms.VerticesTransformed[ ms.Indicies[ i + 2 ] ].Position;
            #define SQUARE( x1, y1, x2, y2, x3, y3 )    fabs( x2 * y3 - x3 * y2 - x1 * y3 + x3 * y1 + x1 * y2 - x2 * y1 )
            float s = 0.0f;
            s += SQUARE( x, y, c2.x, c2.y, c3.x, c3.y );
            s += SQUARE( c1.x, c1.y, x, y, c3.x, c3.y );
            s += SQUARE( c1.x, c1.y, c2.x, c2.y, x, y );
            if( s <= SQUARE( c1.x, c1.y, c2.x, c2.y, c3.x, c3.y ) )
                return true;
            #undef SQUARE
        }
    }

    for( auto it = frame->Children.begin(), end = frame->Children.end(); it != end; ++it )
        if( IsIntersectFrame( *it, ray_origin, ray_dir, x, y ) )
            return true;

    return false;
}

void Animation3d::SetupBorders()
{
    if( bordersDisabled )
        return;

    RectF borders( 1000000.0f, 1000000.0f, -1000000.0f, -1000000.0f );

    // Root
    FrameMove( 0.0, drawXY.X, drawXY.Y, drawScale, true );
    SetupBordersFrame( animEntity->xFile->frameRoot, borders );
    baseBorders.L = (int) borders.L;
    baseBorders.R = (int) borders.R;
    baseBorders.T = (int) borders.T;
    baseBorders.B = (int) borders.B;

    // Children
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
    {
        Animation3d* child = *it;
        child->FrameMove( 0.0, drawXY.X, drawXY.Y, 1.0f, true );
        SetupBordersFrame( child->animEntity->xFile->frameRoot, borders );
    }

    // Store result
    fullBorders.L = (int) borders.L;
    fullBorders.R = (int) borders.R;
    fullBorders.T = (int) borders.T;
    fullBorders.B = (int) borders.B;
    bordersXY = drawXY;

    // Grow borders
    baseBorders.L -= 1;
    baseBorders.R += 2;
    baseBorders.T -= 1;
    baseBorders.B += 2;
    fullBorders.L -= 1;
    fullBorders.R += 2;
    fullBorders.T -= 1;
    fullBorders.B += 2;
}

bool Animation3d::SetupBordersFrame( Frame* frame, RectF& borders )
{
    for( auto it = frame->Mesh.begin(), end = frame->Mesh.end(); it != end; ++it )
    {
        MeshSubset& ms = *it;
        if( !ms.VerticesTransformedValid )
            continue;

        for( size_t i = 0, j = ms.Vertices.size(); i < j; i++ )
        {
            Vector& pos = ms.VerticesTransformed[ i ].Position;
            if( pos.x < borders.L )
                borders.L = pos.x;
            if( pos.x > borders.R )
                borders.R = pos.x;
            if( pos.y < borders.T )
                borders.T = pos.y;
            if( pos.y > borders.B )
                borders.B = pos.y;
        }
    }

    for( auto it = frame->Children.begin(), end = frame->Children.end(); it != end; ++it )
        SetupBordersFrame( *it, borders );

    return true;
}

void Animation3d::ProcessBorders()
{
    if( calcBordersTick && GetTick() >= calcBordersTick )
    {
        SetupBorders();
        calcBordersTick = 0;
    }
}

Rect Animation3d::GetBaseBorders( Point* pivot /* = NULL */ )
{
    ProcessBorders();
    if( pivot )
        *pivot = Point( bordersXY.X - baseBorders.L, bordersXY.Y - baseBorders.T );
    return Rect( baseBorders, drawXY.X - bordersXY.X, drawXY.Y - bordersXY.Y );
}

Rect Animation3d::GetFullBorders( Point* pivot /* = NULL */ )
{
    ProcessBorders();
    if( pivot )
        *pivot = Point( bordersXY.X - fullBorders.L, bordersXY.Y - fullBorders.T );
    return Rect( fullBorders, drawXY.X - bordersXY.X, drawXY.Y - bordersXY.Y );
}

Rect Animation3d::GetExtraBorders( Point* pivot /* = NULL */ )
{
    ProcessBorders();
    float w = (float) fullBorders.W();
    float h = (float) fullBorders.H();
    if( pivot )
    {
        *pivot = Point( bordersXY.X - fullBorders.L, bordersXY.Y - fullBorders.T );
        pivot->X += (int) ( w * 1.5f );
        pivot->Y += (int) ( h * 1.0f );
    }
    Rect result( fullBorders, drawXY.X - bordersXY.X, drawXY.Y - bordersXY.Y );
    result.L -= (int) ( w * 1.5f );
    result.T -= (int) ( h * 1.0f );
    result.R += (int) ( w * 1.5f );
    result.B += (int) ( h * 1.0f );
    return result;
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

MeshOptions* Animation3d::GetMeshOptions( Frame* frame )
{
    for( auto it = meshOpt.begin(), end = meshOpt.end(); it != end; ++it )
    {
        MeshOptions& mopt = *it;
        if( mopt.FramePtr == frame )
            return &mopt;
    }
    return NULL;
}

void Animation3d::SetAnimData( Animation3d* anim3d, AnimParams& data, bool clear )
{
    // Transformations
    if( clear )
    {
        anim3d->matScaleBase = Matrix();
        anim3d->matRotBase = Matrix();
        anim3d->matTransBase = Matrix();
    }
    Matrix mat_tmp;
    if( data.ScaleX != 0.0f )
        anim3d->matScaleBase = anim3d->matScaleBase * Matrix::Scaling( Vector( data.ScaleX, 1.0f, 1.0f ), mat_tmp );
    if( data.ScaleY != 0.0f )
        anim3d->matScaleBase = anim3d->matScaleBase * Matrix::Scaling( Vector( 1.0f, data.ScaleY, 1.0f ), mat_tmp );
    if( data.ScaleZ != 0.0f )
        anim3d->matScaleBase = anim3d->matScaleBase * Matrix::Scaling( Vector( 1.0f, 1.0f, data.ScaleZ ), mat_tmp );
    if( data.RotX != 0.0f )
        anim3d->matRotBase = anim3d->matRotBase * Matrix::RotationX( -data.RotX * PI_VALUE / 180.0f, mat_tmp );
    if( data.RotY != 0.0f )
        anim3d->matRotBase = anim3d->matRotBase * Matrix::RotationY( data.RotY * PI_VALUE / 180.0f, mat_tmp );
    if( data.RotZ != 0.0f )
        anim3d->matRotBase = anim3d->matRotBase * Matrix::RotationZ( data.RotZ * PI_VALUE / 180.0f, mat_tmp );
    if( data.MoveX != 0.0f )
        anim3d->matTransBase = anim3d->matTransBase * Matrix::Translation( Vector( data.MoveX, 0.0f, 0.0f ), mat_tmp );
    if( data.MoveY != 0.0f )
        anim3d->matTransBase = anim3d->matTransBase * Matrix::Translation( Vector( 0.0f, data.MoveY, 0.0f ), mat_tmp );
    if( data.MoveZ != 0.0f )
        anim3d->matTransBase = anim3d->matTransBase * Matrix::Translation( Vector( 0.0f, 0.0f, -data.MoveZ ), mat_tmp );

    // Speed
    if( clear )
        anim3d->speedAdjustLink = 1.0f;
    if( data.SpeedAjust != 0.0f )
        anim3d->speedAdjustLink *= data.SpeedAjust;

    // Textures
    if( clear )
    {
        // Enable all subsets, set default texture
        for( auto it = anim3d->meshOpt.begin(), end = anim3d->meshOpt.end(); it != end; ++it )
        {
            MeshOptions& mopt = *it;
            if( mopt.SubsetsCount )
            {
                memcpy( mopt.TexSubsets, mopt.DefaultTexSubsets, mopt.SubsetsCount * sizeof( MeshTexture* ) * EFFECT_TEXTURES );
                memset( mopt.DisabledSubsets, 0, mopt.SubsetsCount * sizeof( bool ) );
            }
        }
    }

    if( data.TextureCount )
    {
        for( uint i = 0; i < data.TextureCount; i++ )
        {
            MeshTexture* texture = NULL;

            // Get texture
            if( Str::CompareCaseCount( data.TextureName[ i ], "Parent", 6 ) )     // ParentX-Y, X mesh number, Y mesh subset, optional
            {
                uint        parent_mesh_num = 0;
                uint        parent_mesh_ss = 0;

                const char* parent = data.TextureName[ i ] + 6;
                if( *parent && *parent != '-' )
                    parent_mesh_num = atoi( parent );
                while( *parent && *parent != '-' )
                    parent++;
                if( *parent == '-' && *( parent + 1 ) )
                    parent_mesh_ss = atoi( parent + 1 );

                if( anim3d->parentAnimation && parent_mesh_num < anim3d->parentAnimation->meshOpt.size() )
                {
                    MeshOptions& mopt = anim3d->parentAnimation->meshOpt[ parent_mesh_num ];
                    if( parent_mesh_ss < mopt.SubsetsCount )
                        texture = mopt.TexSubsets[ parent_mesh_ss * EFFECT_TEXTURES + data.TextureNum[ i ] ];
                }
            }
            else
            {
                texture = anim3d->animEntity->xFile->GetTexture( data.TextureName[ i ] );
            }

            // Assign it
            if( data.TextureMesh[ i ] < 0 )
            {
                for( auto it = anim3d->meshOpt.begin(), end = anim3d->meshOpt.end(); it != end; ++it )
                {
                    MeshOptions& mopt = *it;
                    if( data.TextureSubset[ i ] < 0 )
                    {
                        for( uint j = 0; j < mopt.SubsetsCount; j++ )
                            mopt.TexSubsets[ j * EFFECT_TEXTURES + data.TextureNum[ i ] ] = texture;
                    }
                    else if( (uint) data.TextureSubset[ i ] < mopt.SubsetsCount )
                    {
                        mopt.TexSubsets[ data.TextureSubset[ i ] * EFFECT_TEXTURES + data.TextureNum[ i ] ] = texture;
                    }
                }
            }
            else if( (uint) data.TextureMesh[ i ] < anim3d->meshOpt.size() )
            {
                MeshOptions& mopt = anim3d->meshOpt[ data.TextureMesh[ i ] ];
                if( data.TextureSubset[ i ] < 0 )
                {
                    for( uint j = 0; j < mopt.SubsetsCount; j++ )
                        mopt.TexSubsets[ j * EFFECT_TEXTURES + data.TextureNum[ i ] ] = texture;
                }
                else if( (uint) data.TextureSubset[ i ] < mopt.SubsetsCount )
                {
                    mopt.TexSubsets[ data.TextureSubset[ i ] * EFFECT_TEXTURES + data.TextureNum[ i ] ] = texture;
                }
            }
        }
    }

    // Effects
    if( clear )
    {
        for( auto it = anim3d->meshOpt.begin(), end = anim3d->meshOpt.end(); it != end; ++it )
        {
            MeshOptions& mopt = *it;
            memcpy( mopt.EffectSubsets, mopt.DefaultEffectSubsets, mopt.SubsetsCount * sizeof( Effect* ) );
        }
    }

    if( data.EffectInstCount )
    {
        for( uint i = 0; i < data.EffectInstCount; i++ )
        {
            Effect* effect = NULL;

            // Get effect
            if( Str::CompareCaseCount( data.EffectInst[ i ].EffectFilename, "Parent", 6 ) )
            {
                uint        parent_mesh_num = 0;
                uint        parent_mesh_ss = 0;

                const char* parent = data.EffectInst[ i ].EffectFilename + 6;
                if( *parent && *parent != '-' )
                    parent_mesh_num = atoi( parent );
                while( *parent && *parent != '-' )
                    parent++;
                if( *parent == '-' && *( parent + 1 ) )
                    parent_mesh_ss = atoi( parent + 1 );

                if( anim3d->parentAnimation && parent_mesh_num < anim3d->parentAnimation->meshOpt.size() )
                {
                    MeshOptions& mopt = anim3d->parentAnimation->meshOpt[ parent_mesh_num ];
                    if( parent_mesh_ss < mopt.SubsetsCount )
                        effect = mopt.EffectSubsets[ parent_mesh_ss ];
                }
            }
            else
            {
                effect = anim3d->animEntity->xFile->GetEffect( &data.EffectInst[ i ] );
            }

            // Assign it
            if( data.EffectInstMesh[ i ] < 0 )
            {
                for( auto it = anim3d->meshOpt.begin(), end = anim3d->meshOpt.end(); it != end; ++it )
                {
                    MeshOptions& mopt = *it;
                    if( data.EffectInstSubset[ i ] < 0 )
                    {
                        for( uint j = 0; j < mopt.SubsetsCount; j++ )
                            mopt.EffectSubsets[ j ] = effect;
                    }
                    else if( (uint) data.EffectInstSubset[ i ] < mopt.SubsetsCount )
                    {
                        mopt.EffectSubsets[ data.EffectInstSubset[ i ] ] = effect;
                    }
                }
            }
            else if( (uint) data.EffectInstMesh[ i ] < anim3d->meshOpt.size() )
            {
                MeshOptions& mopt = anim3d->meshOpt[ data.EffectInstMesh[ i ] ];
                if( data.EffectInstSubset[ i ] < 0 )
                {
                    for( uint j = 0; j < mopt.SubsetsCount; j++ )
                        mopt.EffectSubsets[ j ] = effect;
                }
                else if( (uint) data.EffectInstSubset[ i ] < mopt.SubsetsCount )
                {
                    mopt.EffectSubsets[ data.EffectInstSubset[ i ] ] = effect;
                }
            }
        }
    }
}

void Animation3d::SetDir( int dir )
{
    float angle = (float) ( GameOpt.MapHexagonal ? 150 - dir * 60 : 135 - dir * 45 );
    if( angle != dirAngle )
    {
        dirAngle = angle;
        SetupBorders();
    }
}

void Animation3d::SetDirAngle( int dir_angle )
{
    float angle = (float) dir_angle;
    if( angle != dirAngle )
    {
        dirAngle = angle;
        SetupBorders();
    }
}

void Animation3d::SetRotation( float rx, float ry, float rz )
{
    Matrix my, mx, mz;
    Matrix::RotationY( ry, my );
    Matrix::RotationX( rx, mx );
    Matrix::RotationZ( rz, mz );
    matRot = my * mx * mz;
}

#ifdef SHADOW_MAP
void Animation3d::SetPitch( float angle )
{
    Matrix::RotationX( angle * PI_VALUE / 180.0f, matRot );
}
#endif

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

bool Animation3d::Draw( int x, int y, float scale, RectF* stencil, uint color )
{
    // Move & Draw
    GL( glEnable( GL_DEPTH_TEST ) );
    GL( glEnable( GL_CULL_FACE ) );

    float elapsed = 0.0f;
    uint  tick = GetTick();
    if( AnimDelay && animController )
    {
        while( lastTick + AnimDelay <= tick )
        {
            elapsed += 0.001f * AnimDelay;
            lastTick += AnimDelay;
        }
    }
    else
    {
        elapsed = 0.001f * ( tick - lastTick );
        lastTick = tick;
    }

    bool shadow_disabled = ( shadowDisabled || animEntity->shadowDisabled );

    #ifdef SHADOW_MAP
    drawXY.X = x;
    drawXY.Y = y;

    SetPitch( 75.7f );
    FrameMove( elapsed, x, y, scale, false );
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
        ( *it )->FrameMove( elapsed, x, y, 1.0f, false );

    // glViewport(0,0,ModeWidth*8,ModeWidth*8);
    GL( glBindFramebuffer( GL_FRAMEBUFFER, FBODepth ) );
    GL( glClearDepth( 1.0 ) );
    GL( glClear( GL_DEPTH_BUFFER_BIT ) );
    GL( glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ) );
    GL( glCullFace( GL_FRONT ) );
    glEnable( GL_POLYGON_OFFSET_FILL );
    glPolygonOffset( 2.0, 500.0 );
    DrawFrame( animEntity->xFile->frameRoot, true );
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
        ( *it )->DrawFrame( ( *it )->animEntity->xFile->frameRoot, true );
    GL( glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE ) );
    GL( glCullFace( GL_BACK ) );
    glDisable( GL_POLYGON_OFFSET_FILL );
    // glViewport(0,0, ModeWidth,ModeHeight);
    GLuint cur_fbo = (GLuint) color;
    GL( glBindFramebuffer( GL_FRAMEBUFFER, cur_fbo ) );

    SetPitch( GameOpt.MapCameraAngle );
    FrameMove( -1.0f, x, y, scale, false );
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
        ( *it )->FrameMove( -1.0f, x, y, 1.0f, false );
    DrawFrame( animEntity->xFile->frameRoot, false );
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
        ( *it )->DrawFrame( ( *it )->animEntity->xFile->frameRoot, false );
    #else
    FrameMove( elapsed, x, y, scale, true );
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
        ( *it )->FrameMove( elapsed, x, y, 1.0f, true );
    if( !shadow_disabled )
    {
        GL( glDisable( GL_DEPTH_TEST ) );
        DrawFrame( animEntity->xFile->frameRoot, true );
        for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
            ( *it )->DrawFrame( ( *it )->animEntity->xFile->frameRoot, true );
        GL( glEnable( GL_DEPTH_TEST ) );
    }
    DrawFrame( animEntity->xFile->frameRoot, false );
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
        ( *it )->DrawFrame( ( *it )->animEntity->xFile->frameRoot, false );
    #endif

    GL( glDisable( GL_DEPTH_TEST ) );
    GL( glDisable( GL_CULL_FACE ) );

    float old_scale = drawScale;
    noDraw = false;
    drawScale = scale;
    drawXY.X = x;
    drawXY.Y = y;

    if( scale != old_scale )
        SetupBorders();
    return true;
}

void Animation3d::SetDrawPos( int x, int y )
{
    drawXY.X = x;
    drawXY.Y = y;
}

bool Animation3d::FrameMove( float elapsed, int x, int y, float scale, bool transform )
{
    // Update world matrix, only for root
    if( !parentFrame )
    {
        PointF p3d = Convert2dTo3d( x, y );
        Matrix   mat_rot_y, mat_scale, mat_trans;
        Matrix::Scaling( Vector( scale, scale, scale ), mat_scale );
        Matrix::RotationY( dirAngle * PI_VALUE / 180.0f, mat_rot_y );
        Matrix::Translation( Vector( p3d.X, p3d.Y, 0.0f ), mat_trans );
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
    UpdateFrameMatrices( animEntity->xFile->frameRoot, &parentMatrix );

    // Update linked matrices
    if( parentFrame && linkFrames.size() )
    {
        for( uint i = 0, j = (uint) linkFrames.size() / 2; i < j; i++ )
            // UpdateFrameMatrices(linkFrames[i*2+1],&linkMatricles[i]);
            // linkFrames[i*2+1]->exCombinedTransformationMatrix=linkMatricles[i];
            linkFrames[ i * 2 + 1 ]->CombinedTransformationMatrix = linkFrames[ i * 2 ]->CombinedTransformationMatrix;
    }

    if( transform )
    {
        float wf = (float) ModeWidth;
        float hf = (float) ModeHeight;
        for( auto it = animEntity->xFile->allDrawFrames.begin(), end = animEntity->xFile->allDrawFrames.end(); it != end; ++it )
        {
            Frame* frame = *it;

            MeshOptions* mopt = GetMeshOptions( frame );
            for( uint k = 0, l = (uint) frame->Mesh.size(); k < l; k++ )
            {
                MeshSubset& ms = frame->Mesh[ k ];
                if( mopt->DisabledSubsets[ k ] )
                {
                    ms.VerticesTransformedValid = false;
                    continue;
                }
                ms.VerticesTransformedValid = true;

                // Simple
                size_t mcount = ms.FrameCombinedMatrixPointer.size();
                if( !ms.BoneInfluences || !mcount )
                {
                    for( size_t i = 0, j = ms.Vertices.size(); i < j; i++ )
                    {
                        Vertex3D& v = ms.Vertices[ i ];
                        Vertex3D& vt = ms.VerticesTransformed[ i ];
                        vt.Position = MatrixProjRM * frame->CombinedTransformationMatrix * v.Position;
                        vt.Position.x = ( ( vt.Position.x - 1.0f ) * 0.5f + 0.5f ) * wf;
                        vt.Position.y = ( ( 1.0f - vt.Position.y ) * 0.5f + 0.5f ) * hf;
                    }
                }
                // Skinned
                else
                {
                    for( size_t i = 0; i < mcount; i++ )
                    {
                        Matrix* m = ms.FrameCombinedMatrixPointer[ i ];
                        if( m )
                            BoneMatrices[ i ] = ( *m ) * ms.BoneOffsets[ i ];
                    }

                    for( size_t i = 0, j = ms.Vertices.size(); i < j; i++ )
                    {
                        Vertex3D& v = ms.Vertices[ i ];
                        Vertex3D& vt = ms.VerticesTransformed[ i ];
                        vt.Position = MatrixProjRM * v.Position;

                        Vector position = Vector();
                        for( int b = 0; b < int(ms.BoneInfluences); b++ )
                        {
                            Matrix& m = BoneMatrices[ int(v.BlendIndices[ b ]) ];
                            position += m * v.Position * v.BlendWeights[ b ];
                        }

                        vt.Position = MatrixProjRM * position;
                        vt.Position.x = ( ( vt.Position.x - 1.0f ) * 0.5f + 0.5f ) * wf;
                        vt.Position.y = ( ( 1.0f - vt.Position.y ) * 0.5f + 0.5f ) * hf;
                    }
                }
            }
        }
    }

    // Update world matrices for children
    for( auto it = childAnimations.begin(), end = childAnimations.end(); it != end; ++it )
    {
        Animation3d* child = *it;
        child->groundPos = groundPos;
        child->parentMatrix = child->parentFrame->CombinedTransformationMatrix * child->matTransBase * child->matRotBase * child->matScaleBase;
    }
    return true;
}

void Animation3d::UpdateFrameMatrices( Frame* frame, const Matrix* parent_matrix )
{
    // If parent matrix exists multiply our frame matrix by it
    frame->CombinedTransformationMatrix = ( *parent_matrix ) * frame->TransformationMatrix;

    for( auto it = frame->Children.begin(), end = frame->Children.end(); it != end; ++it )
        UpdateFrameMatrices( *it, &frame->CombinedTransformationMatrix );
}

bool Animation3d::DrawFrame( Frame* frame, bool shadow )
{
    MeshOptions* mopt = GetMeshOptions( frame );
    for( uint k = 0, l = (uint) frame->Mesh.size(); k < l; k++ )
    {
        if( mopt->DisabledSubsets[ k ] )
            continue;

        MeshSubset& ms = frame->Mesh[ k ];
        MeshTexture**   textures = &mopt->TexSubsets[ k * EFFECT_TEXTURES ];

        if( ms.VAO )
        {
            GL( glBindVertexArray( ms.VAO ) );
            GL( glBindBuffer( GL_ARRAY_BUFFER, ms.VBO ) );
            GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ms.IBO ) );
        }
        else
        {
            GL( glBindBuffer( GL_ARRAY_BUFFER, ms.VBO ) );
            GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ms.IBO ) );
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
        }

        Effect* effect;
        if( !shadow )
        {
            effect = mopt->EffectSubsets[ k ];
            if( !effect )
                effect = ( ms.BoneInfluences > 0 ? Effect::Skinned3d : Effect::Simple3d );
        }
        else
        {
            effect = ( ms.BoneInfluences > 0 ? Effect::Skinned3dShadow : Effect::Simple3dShadow );
        }

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
            if( effect->ColorMapAtlasOffset != -1 )
                GL( glUniform4fv( effect->ColorMapAtlasOffset, 1, textures[ 0 ]->AtlasOffsetData ) );
        }
        #ifdef SHADOW_MAP
        if( effect->ShadowMap != -1 )
        {
            GL( glActiveTexture( GL_TEXTURE1 ) );
            GL( glBindTexture( GL_TEXTURE_2D, DepthTexId ) );
            GL( glActiveTexture( GL_TEXTURE0 ) );
            GL( glUniform1i( effect->ShadowMap, 1 ) );
            // if( effect->ShadowMapSize != -1 )
            //    GL( glUniform4fv( effect->ColorMapSize, 1, rt3DSM.TargetTexture->SizeData ) );
            if( effect->ShadowMapMatrix != -1 )
            {
                Matrix mback;
                {
                    PointF p3d = Convert2dTo3d( drawXY.X, drawXY.Y );
                    Matrix mat_trans, mat_trans2, mat_rot;
                    Matrix::Translation( Vector( -p3d.X, -p3d.Y, 0.0f ), mat_trans );
                    Matrix::Translation( Vector( p3d.X, p3d.Y, 0.0f ), mat_trans2 );
                    Matrix::RotationX( -25.7f * PI_VALUE / 180.0f, mat_rot );
                    Matrix mr;
                    Matrix::RotationX( 75.7f * PI_VALUE / 180.0f, mr );
                    mback = mat_trans2 * mr * mat_rot * mat_trans;
                }

                Matrix ms, mt;
                Matrix::Scaling( Vector( 0.5f, 0.5f, 0.5f ), ms );
                Matrix::Translation( Vector( 0.5f, 0.5f, 0.5f ), mt );
                MatrixProj.Transpose();
                Matrix m = mt * ms * MatrixProj * mback;
                MatrixProj.Transpose();
                GL( glUniformMatrix4fv( effect->ShadowMapMatrix, 1, GL_TRUE, m[ 0 ] ) );
            }
            GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE ) );
            GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL ) );
        }
        #endif
        if( effect->MaterialDiffuse != -1 )
            GL( glUniform4fv( effect->MaterialDiffuse, 1, ms.DiffuseColor ) );
        if( effect->MaterialAmbient != -1 )
            GL( glUniform4fv( effect->MaterialAmbient, 1, ms.AmbientColor ) );
        if( effect->BoneInfluences != -1 )
            GL( glUniform1f( effect->BoneInfluences, (float) ms.BoneInfluences ) );
        if( effect->WorldMatrices != -1 )
        {
            uint mcount = (uint) ms.FrameCombinedMatrixPointer.size();
            if( mcount )
            {
                for( uint i = 0; i < mcount; i++ )
                {
                    Matrix* m = ms.FrameCombinedMatrixPointer[ i ];
                    if( m )
                    {
                        BoneMatrices[ i ] = ( *m ) * ms.BoneOffsets[ i ];
                        BoneMatrices[ i ].Transpose();                         // Convert to column major order
                    }
                }
                GL( glUniformMatrix4fv( effect->WorldMatrices, mcount, GL_FALSE, (float*) &BoneMatrices[ 0 ] ) );
            }
            else
            {
                BoneMatrices[ 0 ] = frame->CombinedTransformationMatrix;
                BoneMatrices[ 0 ].Transpose();                 // Convert to column major order
                GL( glUniformMatrix4fv( effect->WorldMatrices, mcount, GL_FALSE, BoneMatrices[ 0 ][ 0 ] ) );
            }
        }
        if( effect->WorldMatrix != -1 )
        {
            BoneMatrices[ 0 ] = frame->CombinedTransformationMatrix;
            BoneMatrices[ 0 ].Transpose();             // Convert to column major order
            GL( glUniformMatrix4fv( effect->WorldMatrix, 1, GL_FALSE, BoneMatrices[ 0 ][ 0 ] ) );
        }
        if( effect->GroundPosition != -1 )
            GL( glUniform3fv( effect->GroundPosition, 1, (float*) &groundPos ) );

        if( effect->IsNeedProcess )
            GraphicLoader::EffectProcessVariables( effect, -1, animPosProc, animPosTime, textures );
        for( uint pass = 0; pass < effect->Passes; pass++ )
        {
            if( effect->IsNeedProcess )
                GraphicLoader::EffectProcessVariables( effect, pass );
            GL( glDrawElements( GL_TRIANGLES, (uint) ms.Indicies.size(), GL_UNSIGNED_SHORT, (void*) 0 ) );
        }

        #ifdef SHADOW_MAP
        if( effect->ShadowMap != -1 )
        {
            GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE ) );
        }
        #endif

        GL( glUseProgram( 0 ) );

        if( ms.VAO )
        {
            GL( glBindVertexArray( 0 ) );
        }
        else
        {
            for( uint i = 0; i <= 9; i++ )
                GL( glDisableVertexAttribArray( i ) );
        }
    }

    for( auto it = frame->Children.begin(), end = frame->Children.end(); it != end; ++it )
        DrawFrame( *it, shadow );

    return true;
}

bool Animation3d::StartUp()
{
    #ifdef SHADOW_MAP
    GL( glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST ) );

    // Depth FBO
    GL( glGenFramebuffers( 1, &FBODepth ) );
    GL( glBindFramebuffer( GL_FRAMEBUFFER, FBODepth ) );
    GL( glDrawBuffer( GL_NONE ) );
    GL( glReadBuffer( GL_NONE ) );
    GL( glGenTextures( 1, &DepthTexId ) );
    GL( glBindTexture( GL_TEXTURE_2D, DepthTexId ) );
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST ) );
    GL( glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP ) );
    GL( glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP ) );
    GL( glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, ModeWidth, ModeHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL ) );
    GL( glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, DepthTexId, 0 ) );
    GLenum status;
    GL( status = glCheckFramebufferStatus( GL_FRAMEBUFFER ) );
    if( status != GL_FRAMEBUFFER_COMPLETE )
    {
        WriteLogF( _FUNC_, " - Fail to create depth render target.\n" );
        return false;
    }
    GL( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
    #endif

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
    FixedZ = 500.0f;

    // Projection
    float k = (float) ModeHeight / 768.0f;
    gluStuffOrtho( MatrixProjRM[ 0 ], 0.0f, 18.65f * k * (float) ModeWidth / ModeHeight, 0.0f, 18.65f * k, -200.0f, 200.0f );
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
    for( auto it = generalAnimations.begin(), end = generalAnimations.end(); it != end; ++it )
        ( *it )->noDraw = true;
}

#pragma MESSAGE("Release all need stuff.")
void Animation3d::PreRestore()
{}

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

    // Set mesh options
    anim3d->meshOpt.resize( entity->xFile->allDrawFrames.size() );
    for( uint i = 0, j = (uint) entity->xFile->allDrawFrames.size(); i < j; i++ )
    {
        MeshOptions& mopt = anim3d->meshOpt[ i ];
        Frame*       frame = entity->xFile->allDrawFrames[ i ];
        mopt.FramePtr = frame;
        mopt.SubsetsCount = (uint) frame->Mesh.size();
        mopt.DisabledSubsets = new bool[ mopt.SubsetsCount ];
        mopt.TexSubsets = new MeshTexture*[ mopt.SubsetsCount * EFFECT_TEXTURES ];
        mopt.DefaultTexSubsets = new MeshTexture*[ mopt.SubsetsCount * EFFECT_TEXTURES ];
        mopt.EffectSubsets = new Effect*[ mopt.SubsetsCount ];
        mopt.DefaultEffectSubsets = new Effect*[ mopt.SubsetsCount ];
        memzero( mopt.DisabledSubsets, mopt.SubsetsCount * sizeof( bool ) );
        memzero( mopt.TexSubsets, mopt.SubsetsCount * sizeof( MeshTexture* ) * EFFECT_TEXTURES );
        memzero( mopt.DefaultTexSubsets, mopt.SubsetsCount * sizeof( MeshTexture* ) * EFFECT_TEXTURES );
        memzero( mopt.EffectSubsets, mopt.SubsetsCount * sizeof( Effect* ) );
        memzero( mopt.DefaultEffectSubsets, mopt.SubsetsCount * sizeof( Effect* ) );
        // Set default textures and effects
        for( uint k = 0; k < mopt.SubsetsCount; k++ )
        {
            uint        tex_num = k * EFFECT_TEXTURES;
            const char* tex_name = ( frame->Mesh[ k ].DiffuseTexture.length() ? frame->Mesh[ k ].DiffuseTexture.c_str() : NULL );
            mopt.DefaultTexSubsets[ tex_num ] = ( tex_name ? entity->xFile->GetTexture( tex_name ) : NULL );
            mopt.TexSubsets[ tex_num ] = mopt.DefaultTexSubsets[ tex_num ];
            mopt.DefaultEffectSubsets[ k ] = ( frame->Mesh[ k ].DrawEffect.EffectFilename ? entity->xFile->GetEffect( &frame->Mesh[ k ].DrawEffect ) : NULL );
            mopt.EffectSubsets[ k ] = mopt.DefaultEffectSubsets[ k ];
        }
    }

    // Set default data
    anim3d->SetAnimation( 0, 0, NULL, ANIMATION_INIT );

    if( !is_child )
        generalAnimations.push_back( anim3d );
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

PointF Animation3d::Convert2dTo3d( int x, int y )
{
    Vector coords;
    VecUnproject( Vector( (float) x, (float) y, 0.0f ), coords );
    return PointF( coords.x, coords.y );
}

Point Animation3d::Convert3dTo2d( float x, float y )
{
    Vector coords;
    VecProject( Vector( x, y, FixedZ ), coords );
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
        SAFEDELA( link.LinkBone );
        SAFEDELA( link.ChildFName );
        SAFEDELA( link.DisabledLayers );
        SAFEDELA( link.DisabledSubsets );
        SAFEDELA( link.DisabledSubsetsMesh );
        for( uint i = 0; i < link.TextureCount; i++ )
            SAFEDELA( link.TextureName[ i ] );
        SAFEDELA( link.TextureName );
        SAFEDELA( link.TextureMesh );
        SAFEDELA( link.TextureSubset );
        SAFEDELA( link.TextureNum );
        for( uint i = 0; i < link.EffectInstCount; i++ )
        {
            for( uint j = 0; j < link.EffectInst[ i ].DefaultsCount; j++ )
            {
                SAFEDELA( link.EffectInst[ i ].Defaults[ j ].Name );
                SAFEDELA( link.EffectInst[ i ].Defaults[ j ].Data );
                SAFEDELA( link.EffectInst[ i ].Defaults );
            }
        }
        SAFEDELA( link.EffectInst );
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

        int              mesh = 0;
        int              subset = -1;
        int              layer = -1;
        int              layer_val = 0;
        EffectInstance*  cur_effect = NULL;

        AnimParams       dummy_link;
        memzero( &dummy_link, sizeof( dummy_link ) );
        AnimParams*      link = &animDataDefault;
        static uint      link_id = 0;

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
                closed = true;
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
                    memcpy( tmp_pbuf, pbuf, len + 1 );
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
                int pos = (int) ( *istr ).tellg();
                Str::Insert( &big_buf[ pos ], " " );
                Str::Insert( &big_buf[ pos + 1 ], pbuf );
                Str::Insert( &big_buf[ pos + 1 + Str::Length( pbuf ) ], " " );
                delete[] pbuf;

                // Reinitialize stream
                delete istr;
                istr = new istrstream( &big_buf[ pos ] );
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

                mesh = -1;
                subset = -1;
            }
            else if( Str::CompareCase( token, "Mesh" ) )
            {
                ( *istr ) >> buf;
                mesh = ConstantsManager::GetDefineValue( buf );
            }
            else if( Str::CompareCase( token, "Subset" ) )
            {
                ( *istr ) >> buf;
                subset = ConstantsManager::GetDefineValue( buf );
            }
            else if( Str::CompareCase( token, "Layer" ) || Str::CompareCase( token, "Value" ) )
            {
                ( *istr ) >> buf;
                if( Str::CompareCase( token, "Layer" ) )
                    layer = ConstantsManager::GetDefineValue( buf );
                else
                    layer_val = ConstantsManager::GetDefineValue( buf );

                link = &dummy_link;
                mesh = -1;
                subset = -1;
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

                mesh = -1;
                subset = -1;
            }
            else if( Str::CompareCase( token, "Link" ) )
            {
                ( *istr ) >> buf;
                if( link->Id )
                    link->LinkBone = Str::Duplicate( buf );
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
                StrVec subsets;
                Str::ParseLine( buf, '-', subsets, Str::ParseLineDummy );
                for( uint m = 0, n = (uint) subsets.size(); m < n; m++ )
                {
                    int ss = ConstantsManager::GetDefineValue( subsets[ m ].c_str() );
                    if( ss >= 0 )
                    {
                        int* tmp1 = link->DisabledSubsets;
                        int* tmp2 = link->DisabledSubsetsMesh;
                        link->DisabledSubsets = new int[ link->DisabledSubsetsCount + 1 ];
                        link->DisabledSubsetsMesh = new int[ link->DisabledSubsetsCount + 1 ];
                        for( uint h = 0; h < link->DisabledSubsetsCount; h++ )
                        {
                            link->DisabledSubsets[ h ] = tmp1[ h ];
                            link->DisabledSubsetsMesh[ h ] = tmp2[ h ];
                        }
                        SAFEDELA( tmp1 );
                        SAFEDELA( tmp2 );
                        link->DisabledSubsets[ link->DisabledSubsetsCount ] = ss;
                        link->DisabledSubsetsMesh[ link->DisabledSubsetsCount ] = mesh;
                        link->DisabledSubsetsCount++;
                    }
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
                    int*   tmp2 = link->TextureMesh;
                    int*   tmp3 = link->TextureSubset;
                    int*   tmp4 = link->TextureNum;
                    link->TextureName = new char*[ link->TextureCount + 1 ];
                    link->TextureMesh = new int[ link->TextureCount + 1 ];
                    link->TextureSubset = new int[ link->TextureCount + 1 ];
                    link->TextureNum = new int[ link->TextureCount + 1 ];
                    for( uint h = 0; h < link->TextureCount; h++ )
                    {
                        link->TextureName[ h ] = tmp1[ h ];
                        link->TextureMesh[ h ] = tmp2[ h ];
                        link->TextureSubset[ h ] = tmp3[ h ];
                        link->TextureNum[ h ] = tmp4[ h ];
                    }
                    SAFEDELA( tmp1 );
                    SAFEDELA( tmp2 );
                    SAFEDELA( tmp3 );
                    SAFEDELA( tmp4 );
                    link->TextureName[ link->TextureCount ] = Str::Duplicate( buf );
                    link->TextureMesh[ link->TextureCount ] = mesh;
                    link->TextureSubset[ link->TextureCount ] = subset;
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
                int*            tmp2 = link->EffectInstMesh;
                int*            tmp3 = link->EffectInstSubset;
                link->EffectInst = new EffectInstance[ link->EffectInstCount + 1 ];
                link->EffectInstMesh = new int[ link->EffectInstCount + 1 ];
                link->EffectInstSubset = new int[ link->EffectInstCount + 1 ];
                for( uint h = 0; h < link->EffectInstCount; h++ )
                {
                    link->EffectInst[ h ] = tmp1[ h ];
                    link->EffectInstMesh[ h ] = tmp2[ h ];
                    link->EffectInstSubset[ h ] = tmp3[ h ];
                }
                SAFEDELA( tmp1 );
                SAFEDELA( tmp2 );
                SAFEDELA( tmp3 );
                link->EffectInst[ link->EffectInstCount ] = *effect_inst;
                link->EffectInstMesh[ link->EffectInstCount ] = mesh;
                link->EffectInstSubset[ link->EffectInstCount ] = subset;
                link->EffectInstCount++;

                cur_effect = &link->EffectInst[ link->EffectInstCount - 1 ];
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
                int anim_index = (int) anim_indexes[ i + 0 ];
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
                else
                {
                    WriteLogF( _FUNC_, " - Unable to find animation<%s><%s>, file<%s>.\n", anim_path, anim_name, name );
                }

                delete[] anim_fname;
                delete[] anim_name;
            }

            numAnimationSets = animController->GetNumAnimationSets();
            if( numAnimationSets > 0 )
                Animation3dXFile::SetupAnimationOutput( xFile->frameRoot, animController );
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

Animation3dXFile::Animation3dXFile(): frameRoot( NULL )
{}

Animation3dXFile::~Animation3dXFile()
{
    GraphicLoader::Free( frameRoot );
    frameRoot = NULL;
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
        Frame* frame_root = GraphicLoader::LoadModel( xname );
        if( !frame_root )
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
        xfile->frameRoot = frame_root;
        xfile->SetupFrames( xfile, xfile->frameRoot, xfile->frameRoot );

        xFiles.push_back( xfile );
    }

    return xfile;
}

bool Animation3dXFile::SetupFrames( Animation3dXFile* xfile, Frame* frame, Frame* frame_root )
{
    xfile->allFrames.push_back( frame );
    if( !frame->Mesh.empty() )
        xfile->allDrawFrames.push_back( frame );

    for( auto it = frame->Mesh.begin(), end = frame->Mesh.end(); it != end; ++it )
    {
        MeshSubset& ms = *it;
        if( BoneMatrices.size() < ms.FrameCombinedMatrixPointer.size() )
            BoneMatrices.resize( ms.FrameCombinedMatrixPointer.size() );
    }

    for( auto it = frame->Children.begin(), end = frame->Children.end(); it != end; ++it )
        SetupFrames( xfile, *it, frame_root );

    return true;
}

void Animation3dXFile::SetupAnimationOutput( Frame* frame, AnimController* anim_controller )
{
    if( frame->Name.length() > 0 )
        anim_controller->RegisterAnimationOutput( frame->Name.c_str(), frame->TransformationMatrix );

    for( auto it = frame->Children.begin(), end = frame->Children.end(); it != end; ++it )
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

/************************************************************************/
/*                                                                      */
/************************************************************************/
