#ifndef __3D_ANIMATION__
#define __3D_ANIMATION__

#include "GraphicStructures.h"

class AnimSet
{
private:
    friend class AnimController;
    struct Output
    {
        string        name;
        FloatVec      scaleTime;
        VectorVec     scaleValue;
        FloatVec      rotationTime;
        QuaternionVec rotationValue;
        FloatVec      translationTime;
        VectorVec     translationValue;
    };
    typedef vector< Output > OutputVec;

    string    animName;
    float     durationTicks;
    float     ticksPerSecond;
    OutputVec outputs;

public:
    void SetData( const char* name, float ticks, float tps )
    {
        animName = name;
        durationTicks = ticks;
        ticksPerSecond = tps;
    }

    void AddOutput( const char* name, const FloatVec& st, const VectorVec& sv,
                    const FloatVec& rt, const QuaternionVec& rv, const FloatVec& tt, const VectorVec& tv  )
    {
        outputs.push_back( Output() );
        Output& o = outputs.back();
        o.name = name;
        o.scaleTime = st;
        o.scaleValue = sv;
        o.rotationTime = rt;
        o.rotationValue = rv;
        o.translationTime = tt;
        o.translationValue = tv;
    }

    const char* GetName()
    {
        return animName.c_str();
    }

    uint GetOutputCount()
    {
        return outputs.size();
    }

    float GetDuration()
    {
        return durationTicks / ticksPerSecond;
    }
};
typedef vector< AnimSet* > AnimSetVec;

class AnimController
{
private:
    struct Output
    {
        string        name;
        Matrix*       matrix;
        // Data for tracks blending
        BoolVec       valid;
        FloatVec      factor;
        VectorVec     scale;
        QuaternionVec rotation;
        VectorVec     translation;
    };
    typedef vector< Output >  OutputVec;
    typedef vector< Output* > OutputPtrVec;
    struct Track
    {
        struct Event
        {
            enum EType { Enable, Speed, Weight };
            Event( EType t, float v, float start, float smooth ): type( t ), valueFrom( -1.0f ), valueTo( v ), startTime( start ), smoothTime( smooth ) {}
            EType type;
            float valueFrom;
            float valueTo;
            float startTime;
            float smoothTime;
        };
        typedef vector< Event > EventVec;

        bool         enabled;
        float        speed;
        float        weight;
        float        position;
        AnimSet*     anim;
        OutputPtrVec animOutput;
        EventVec     events;
    };
    typedef vector< Track > TrackVec;

    bool        cloned;
    AnimSetVec* sets;
    OutputVec*  outputs;
    TrackVec    tracks;
    float       curTime;

public:
    AnimController(): sets( NULL ), outputs( NULL ), curTime( 0.0f )
    {}

    ~AnimController()
    {
        if( !cloned )
        {
            delete sets;
            delete outputs;
        }
    }

    static AnimController* Create( uint track_count )
    {
        // Prototype
        AnimController* controller = new AnimController();
        controller->cloned = false;
        controller->sets = new AnimSetVec();
        controller->outputs = new OutputVec();
        controller->tracks.resize( track_count );
        memzero( &controller->tracks[ 0 ], sizeof( Track ) * track_count );
        controller->curTime = 0.0f;
        return controller;
    }

    AnimController* Clone()
    {
        // Instance
        AnimController* clone = new AnimController();
        clone->cloned = true;
        clone->sets = sets;
        clone->outputs = outputs;
        clone->tracks = tracks;
        clone->curTime = 0.0f;
        return clone;
    }

    void RegisterAnimationOutput( const char* frame_name, Matrix& output_matrix )
    {
        outputs->push_back( Output() );
        Output& o = outputs->back();
        o.name = frame_name;
        o.matrix = &output_matrix;
        o.valid.resize( tracks.size() );
        o.factor.resize( tracks.size() );
        o.scale.resize( tracks.size() );
        o.rotation.resize( tracks.size() );
        o.translation.resize( tracks.size() );
    }

    void RegisterAnimationSet( AnimSet* animation )
    {
        sets->push_back( animation );
    }

    AnimSet* GetAnimationSet( uint index )
    {
        if( index >= sets->size() )
            return NULL;
        return ( *sets )[ index ];
    }

    AnimSet* GetAnimationSetByName( const char* name )
    {
        for( auto it = sets->begin(), end = sets->end(); it != end; ++it )
        {
            if( ( *it )->animName == name )
                return *it;
        }
        return NULL;
    }

    float GetTrackPosition( uint track )
    {
        return tracks[ track ].position;
    }

    uint GetMaxNumAnimationSets()
    {
        return sets->size();
    }

    void SetTrackAnimationSet( uint track, AnimSet* anim )
    {
        // Set and link animation
        tracks[ track ].anim = anim;
        uint count = anim->GetOutputCount();
        tracks[ track ].animOutput.resize( count );
        for( uint i = 0; i < count; i++ )
        {
            const string& link_name = anim->outputs[ i ].name;
            Output*       output = NULL;
            for( uint j = 0; j < outputs->size(); j++ )
            {
                if( ( *outputs )[ j ].name == link_name )
                {
                    output = &( *outputs )[ j ];
                    break;
                }
            }
            tracks[ track ].animOutput[ i ] = output;
        }
    }

    void Reset()
    {
        curTime = 0.0f;
        for( uint i = 0; i < tracks.size(); i++ )
            tracks[ i ].events.clear();
    }

    float GetTime()
    {
        return curTime;
    }

    void AddEventEnable( uint track, bool enable, float start_time )
    {
        tracks[ track ].events.push_back( Track::Event( Track::Event::Enable, enable ? 1.0f : -1.0f, start_time, 0.0f ) );
    }

    void AddEventSpeed( uint track, float speed, float start_time, float smooth_time )
    {
        tracks[ track ].events.push_back( Track::Event( Track::Event::Speed, speed, start_time, smooth_time ) );
    }

    void AddEventWeight( uint track, float weight, float start_time, float smooth_time )
    {
        tracks[ track ].events.push_back( Track::Event( Track::Event::Weight, weight, start_time, smooth_time ) );
    }

    void SetTrackEnable( uint track, bool enable )
    {
        tracks[ track ].enabled = enable;
    }

    void SetTrackPosition( uint track, float position )
    {
        tracks[ track ].position = position;
    }

    void AdvanceTime( float time )
    {
        // Animation time
        curTime += time;

        // Track events
        for( uint i = 0, j = tracks.size(); i < j; i++ )
        {
            Track& track = tracks[ i ];

            // Events
            for( auto it = track.events.begin(); it != track.events.end();)
            {
                Track::Event& e = *it;
                if( curTime >= e.startTime )
                {
                    if( e.smoothTime > 0.0f && e.valueFrom == -1.0f )
                    {
                        if( e.type == Track::Event::Speed )
                            e.valueFrom = track.speed;
                        else if( e.type == Track::Event::Weight )
                            e.valueFrom = track.weight;
                    }

                    bool  erase = false;
                    float value = e.valueTo;
                    if( curTime < e.startTime + e.smoothTime )
                    {
                        if( e.valueTo > e.valueFrom )
                            value = e.valueFrom + ( e.valueTo - e.valueFrom ) / e.smoothTime * ( curTime - e.startTime );
                        else
                            value = e.valueFrom - ( e.valueFrom - e.valueTo ) / e.smoothTime * ( curTime - e.startTime );
                    }
                    else
                    {
                        erase = true;
                    }

                    if( e.type == Track::Event::Enable )
                        track.enabled = ( value > 0.0f ? true : false );
                    else if( e.type == Track::Event::Speed )
                        track.speed = value;
                    else if( e.type == Track::Event::Weight )
                        track.weight = value;

                    if( erase )
                    {
                        it = track.events.erase( it );
                        continue;
                    }
                }

                ++it;
            }

            // Add track time
            if( track.enabled )
                track.position += time * track.speed;
        }

        // Track animation
        for( uint i = 0, j = tracks.size(); i < j; i++ )
        {
            Track& track = tracks[ i ];

            for( uint k = 0, l = outputs->size(); k < l; k++ )
                ( *outputs )[ k ].valid[ i ] = false;

            if( !track.enabled || track.weight <= 0.0f || !track.anim )
                continue;

            for( uint k = 0, l = track.anim->outputs.size(); k < l; k++ )
            {
                if( !track.animOutput[ k ] )
                    continue;

                AnimSet::Output& o = track.anim->outputs[ k ];

                float            time = fmod( track.position * track.anim->ticksPerSecond, track.anim->durationTicks );
                FindSRTValue< Vector >( time, o.scaleTime, o.scaleValue, track.animOutput[ k ]->scale[ i ] );
                FindSRTValue< Quaternion >( time, o.rotationTime, o.rotationValue, track.animOutput[ k ]->rotation[ i ] );
                FindSRTValue< Vector >( time, o.translationTime, o.translationValue, track.animOutput[ k ]->translation[ i ] );
                track.animOutput[ k ]->valid[ i ] = true;
                track.animOutput[ k ]->factor[ i ] = track.weight;
            }
        }

        // Blend tracks
        for( uint i = 0, j = outputs->size(); i < j; i++ )
        {
            Output& o = ( *outputs )[ i ];

            // Todo: add interpolation for tracks more than two
            if( tracks.size() >= 2 && o.valid[ 0 ] && o.valid[ 1 ] )
            {
                float factor = o.factor[ 1 ];
                Interpolate( o.scale[ 0 ], o.scale[ 1 ], factor );
                Interpolate( o.rotation[ 0 ], o.rotation[ 1 ], factor );
                Interpolate( o.translation[ 0 ], o.translation[ 1 ], factor );
                Matrix ms, mr, mt;
                Matrix::Scaling( o.scale[ 0 ], ms );
                mr = Matrix( o.rotation[ 0 ].GetMatrix() );
                Matrix::Translation( o.translation[ 0 ], mt );
                Matrix m = mt * mr * ms;
                MATRIX_TRANSPOSE( m );
                *o.matrix = m;
            }
            else
            {
                for( uint k = 0, l = tracks.size(); k < l; k++ )
                {
                    if( o.valid[ k ] )
                    {
                        Matrix ms, mr, mt;
                        Matrix::Scaling( o.scale[ k ], ms );
                        mr = Matrix( o.rotation[ k ].GetMatrix() );
                        Matrix::Translation( o.translation[ k ], mt );
                        Matrix m = mt * mr * ms;
                        MATRIX_TRANSPOSE( m );
                        *o.matrix = m;
                        break;
                    }
                }
            }
        }
    }

private:
    template< class T >
    void FindSRTValue( float time, FloatVec& times, vector< T >& values, T& result )
    {
        for( uint n = 0, m = times.size(); n < m; n++ )
        {
            if( n + 1 < m )
            {
                if( time >= times[ n ] && time < times[ n + 1 ] )
                {
                    result = values[ n ];
                    T&    value = values[ n + 1 ];
                    float factor = ( time - times[ n ] ) / ( times[ n + 1 ] - times[ n ] );
                    Interpolate( result, value, factor );
                    return;
                }
            }
            else
            {
                result = values[ n ];
            }
        }
    }

    void Interpolate( Quaternion& q1, Quaternion& q2, float factor )
    {
        Quaternion::Interpolate( q1, q1, q2, factor );
    }

    void Interpolate( Vector& v1, Vector& v2, float factor )
    {
        v1.x = v1.x + ( v2.x - v1.x ) * factor;
        v1.y = v1.y + ( v2.y - v1.y ) * factor;
        v1.z = v1.z + ( v2.z - v1.z ) * factor;
    }
};

#endif // __3D_ANIMATION__
