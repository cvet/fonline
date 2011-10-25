#ifndef __3D_ANIMATION__
#define __3D_ANIMATION__

#include "GraphicStructures.h"

class AnimSet
{
public:
    ID3DXAnimationSet* DXSet;
    char*              Name;

    uint GetNumAnimations()
    {
        return DXSet->GetNumAnimations();
    }

    float GetPeriod()
    {
        return (float) DXSet->GetPeriod();
    }
};
typedef vector< AnimSet* > AnimSetVec;

class AnimController
{
private:
    ID3DXAnimationController* dxAnimController;
    AnimSetVec                animations;

public:
    AnimController()
    {
        dxAnimController = NULL;
    }

    ~AnimController()
    {
        SAFEREL( dxAnimController );
    }

    bool Create( uint output_count, uint animation_count )
    {
        return SUCCEEDED( D3DXCreateAnimationController( output_count, animation_count, 2, 10, &dxAnimController ) );
    }

    AnimController* Clone()
    {
        AnimController* clone = new AnimController();
        if( SUCCEEDED( dxAnimController->CloneAnimationController(
                           dxAnimController->GetMaxNumAnimationOutputs(),
                           dxAnimController->GetMaxNumAnimationSets(),
                           dxAnimController->GetMaxNumTracks(),
                           dxAnimController->GetMaxNumEvents(),
                           &clone->dxAnimController ) ) )
        {
            clone->animations = animations;
            return clone;
        }
        delete clone;
        return NULL;
    }

    void RegisterAnimationOutput( const char* frame_name, Matrix& output_matrix )
    {
        dxAnimController->RegisterAnimationOutput( frame_name, (D3DXMATRIX*) &output_matrix, NULL, NULL, NULL );
    }

    AnimSet* GetAnimationSet( uint index )
    {
        if( index >= animations.size() )
            return NULL;
        return animations[ index ];
    }

    AnimSet* GetAnimationSetByName( const char* name )
    {
        for( auto it = animations.begin(), end = animations.end(); it != end; ++it )
        {
            if( Str::Compare( ( *it )->Name, name ) )
                return *it;
        }
        return NULL;
    }

    void SetTrackAnimationSet( uint track, AnimSet* set )
    {
        dxAnimController->SetTrackAnimationSet( track, set->DXSet );
    }

    void UnkeyAllTrackEvents( uint track )
    {
        dxAnimController->UnkeyAllTrackEvents( track );
    }

    void ResetTime()
    {
        dxAnimController->ResetTime();
    }

    float GetTime()
    {
        return (float) dxAnimController->GetTime();
    }

    void KeyTrackEnable( uint track, bool enable, float smooth_time )
    {
        dxAnimController->KeyTrackEnable( track, enable, smooth_time );
    }

    void KeyTrackSpeed( uint track, float speed, float start_time, float smooth_time )
    {
        dxAnimController->KeyTrackSpeed( track, speed, start_time, smooth_time, D3DXTRANSITION_LINEAR );
    }

    void KeyTrackWeight( uint track, float weight, float start_time, float smooth_time )
    {
        dxAnimController->KeyTrackWeight( track, weight, start_time, smooth_time, D3DXTRANSITION_LINEAR );
    }

    void SetTrackEnable( uint track, bool enable )
    {
        dxAnimController->SetTrackEnable( track, enable );
    }

    void SetTrackPosition( uint track, float position )
    {
        dxAnimController->SetTrackPosition( track, position );
    }

    void AdvanceTime( float time )
    {
        dxAnimController->AdvanceTime( time, NULL );
    }

    float GetTrackPosition( uint track )
    {
        D3DXTRACK_DESC tdesc;
        if( SUCCEEDED( dxAnimController->GetTrackDesc( track, &tdesc ) ) )
            return (float) tdesc.Position;
        return 0.0f;
    }

    void RegisterAnimationSet( AnimSet* animation )
    {
        animations.push_back( animation );
        dxAnimController->RegisterAnimationSet( animation->DXSet );
    }

    uint GetMaxNumAnimationSets()
    {
        return dxAnimController->GetMaxNumAnimationSets();
    }
};

#endif // __3D_ANIMATION__
