#include "Common.h"
#include "Jobs.h"

typedef deque< Job > JobDeque;
static JobDeque Jobs;

Job::Job( int type, void* data ): Type( type ), Data( data )
{}

void Job::PushBack( int type )
{
    Jobs.push_back( Job( type, nullptr ) );
}

void Job::PushBack( int type, void* data )
{
    Jobs.push_back( Job( type, data ) );
}

uint Job::PushBack( const Job& job )
{
    Jobs.push_back( job );
    uint size = (uint) Jobs.size();
    return size;
}

void Job::PushFront( const Job& job )
{
    Jobs.push_front( job );
}

Job Job::PopFront()
{
    RUNTIME_ASSERT( !Jobs.empty() );
    Job job = Jobs.front();
    Jobs.pop_front();
    return job;
}

void Job::Erase( int type )
{
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
    return (uint) Jobs.size();
}
