#include "StdAfx.h"
#include "Jobs.h"
#include "Mutex.h"
#include "Map.h"
#include "Critter.h"
#include "Item.h"
#include "Vars.h"

static Mutex JobLocker; // Defense code from simultaneously execution

typedef deque< Job > JobDeque;
static JobDeque Jobs;

Job::Job(): Type( JOB_NOP ),
            Data( NULL ),
            ThreadId( 0 )
{}

Job::Job( int type, void* data, bool cur_thread ): Type( type ),
                                                   Data( data ),
                                                   ThreadId( 0 )
{
    if( cur_thread )
        ThreadId = GetCurrentThreadId();
}

void Job::PushBack( int type )
{
    SCOPE_LOCK( JobLocker );

    Jobs.push_back( Job( type, NULL, 0 ) );
}

void Job::PushBack( int type, void* data )
{
    SCOPE_LOCK( JobLocker );

    Jobs.push_back( Job( type, data, 0 ) );
}

uint Job::PushBack( const Job& job )
{
    SCOPE_LOCK( JobLocker );

    Jobs.push_back( job );
    uint size = (uint) Jobs.size();
    return size;
}

void Job::PushFront( const Job& job )
{
    SCOPE_LOCK( JobLocker );

    Jobs.push_front( job );
}

Job Job::PopFront()
{
    SCOPE_LOCK( JobLocker );

    if( Jobs.empty() )
        return Job( JOB_NOP, NULL, false );

    Job job = Jobs.front();

    // Check owner
    int tid = (int) GetCurrentThreadId();
    if( job.ThreadId && job.ThreadId != tid )
    {
        for( auto it = Jobs.begin() + 1; it != Jobs.end(); ++it )
        {
            Job& job_ = *it;
            if( !job_.ThreadId || job_.ThreadId == tid )
            {
                Job job__ = job_;
                Jobs.erase( it );
                return job__;
            }
        }
        return Job( JOB_NOP, NULL, false );
    }

    Jobs.pop_front();
    return job;
}

void Job::Erase( int type )
{
    SCOPE_LOCK( JobLocker );

    for( auto it = Jobs.begin(); it != Jobs.end();)
    {
        Job& job = *it;
        if( job.Type == type )
            it = Jobs.erase( it );
        else
            ++it;
    }
}

uint Job::Count()
{
    SCOPE_LOCK( JobLocker );
    uint count = (uint) Jobs.size();
    return count;
}

// Deferred releasing
static CrVec      DeferredReleaseCritters;
static UIntVec    DeferredReleaseCrittersCycle;
static MapVec     DeferredReleaseMaps;
static UIntVec    DeferredReleaseMapsCycle;
static LocVec     DeferredReleaseLocs;
static UIntVec    DeferredReleaseLocsCycle;
static ItemPtrVec DeferredReleaseItems;
static UIntVec    DeferredReleaseItemsCycle;
static VarsVec    DeferredReleaseVars;
static UIntVec    DeferredReleaseVarsCycle;
static uint       DeferredReleaseCycle = 0;
static Mutex      DeferredReleaseLocker;

void Job::DeferredRelease( Critter* cr )
{
    SCOPE_LOCK( DeferredReleaseLocker );

    DeferredReleaseCritters.push_back( cr );
    DeferredReleaseCrittersCycle.push_back( DeferredReleaseCycle );
}

void Job::DeferredRelease( Map* map )
{
    SCOPE_LOCK( DeferredReleaseLocker );

    DeferredReleaseMaps.push_back( map );
    DeferredReleaseMapsCycle.push_back( DeferredReleaseCycle );
}

void Job::DeferredRelease( Location* loc )
{
    SCOPE_LOCK( DeferredReleaseLocker );

    DeferredReleaseLocs.push_back( loc );
    DeferredReleaseLocsCycle.push_back( DeferredReleaseCycle );
}

void Job::DeferredRelease( Item* item )
{
    SCOPE_LOCK( DeferredReleaseLocker );

    DeferredReleaseItems.push_back( item );
    DeferredReleaseItemsCycle.push_back( DeferredReleaseCycle );
}

void Job::DeferredRelease( GameVar* var )
{
    SCOPE_LOCK( DeferredReleaseLocker );

    DeferredReleaseVars.push_back( var );
    DeferredReleaseVarsCycle.push_back( DeferredReleaseCycle );
}

void Job::SetDeferredReleaseCycle( uint cycle )
{
    SCOPE_LOCK( DeferredReleaseLocker );

    DeferredReleaseCycle = cycle;
}

template< class T1, class T2 >
void ProcessDeferredReleasing_( T1& cont, T2& cont_cycle )
{
    uint del_count = 0;
    for( uint i = 0, j = (uint) cont_cycle.size(); i < j; i++ )
    {
        if( cont_cycle[ i ] >= DeferredReleaseCycle - 2 )
            break;
        else
            del_count++;
    }
    if( del_count )
    {
        for( uint i = 0; i < del_count; i++ )
            cont[ i ]->Release();
        cont.erase( cont.begin(), cont.begin() + del_count );
        cont_cycle.erase( cont_cycle.begin(), cont_cycle.begin() + del_count );
    }
}

void Job::ProcessDeferredReleasing()
{
    SCOPE_LOCK( DeferredReleaseLocker );

    // Wait at least 3 cycles
    if( DeferredReleaseCycle < 3 )
        return;

    ProcessDeferredReleasing_( DeferredReleaseCritters, DeferredReleaseCrittersCycle );
    ProcessDeferredReleasing_( DeferredReleaseMaps, DeferredReleaseMapsCycle );
    ProcessDeferredReleasing_( DeferredReleaseLocs, DeferredReleaseLocsCycle );
    ProcessDeferredReleasing_( DeferredReleaseItems, DeferredReleaseItemsCycle );
    ProcessDeferredReleasing_( DeferredReleaseVars, DeferredReleaseVarsCycle );
}
