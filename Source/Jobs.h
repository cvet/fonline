#ifndef __JOBS__
#define __JOBS__

// Server job types
#define JOB_NOP                   ( 0 )
#define JOB_CLIENT                ( 1 )
#define JOB_CRITTER               ( 2 )
#define JOB_MAP                   ( 3 )
#define JOB_TIME_EVENTS           ( 4 )
#define JOB_GARBAGE_ITEMS         ( 5 )
#define JOB_GARBAGE_CRITTERS      ( 6 )
#define JOB_GARBAGE_LOCATIONS     ( 7 )
#define JOB_DEFERRED_RELEASE      ( 8 )
#define JOB_GAME_TIME             ( 9 )
#define JOB_BANS                  ( 10 )
#define JOB_LOOP_SCRIPT           ( 11 )
#define JOB_THREAD_LOOP           ( 12 )
#define JOB_THREAD_SYNCHRONIZE    ( 13 )
#define JOB_THREAD_FINISH         ( 14 )
#define JOB_COUNT                 ( 15 )

class Critter;
class Map;
class Location;
class Item;
class GameVar;

class Job
{
public:
    int    Type;
    void*  Data;
    size_t ThreadId;

    Job();
    Job( int type, void* data, bool cur_thread );

    static void PushBack( int type );
    static void PushBack( int type, void* data );
    static uint PushBack( const Job& job );
    static void PushFront( const Job& job );
    static Job  PopFront();
    static void Erase( int type );
    static uint Count();

    // Deferred releasing
    static void DeferredRelease( Critter* cr );
    static void DeferredRelease( Map* cr );
    static void DeferredRelease( Location* cr );
    static void DeferredRelease( Item* cr );
    static void SetDeferredReleaseCycle( uint cycle );
    static void ProcessDeferredReleasing();
};

#endif // __JOBS__
