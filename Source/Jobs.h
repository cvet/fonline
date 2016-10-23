#ifndef __JOBS__
#define __JOBS__

// Server job types
#define JOB_CLIENT                ( 1 )
#define JOB_CRITTER               ( 2 )
#define JOB_MAP                   ( 3 )
#define JOB_GARBAGE_LOCATIONS     ( 6 )
#define JOB_GAME_TIME             ( 8 )
#define JOB_BANS                  ( 9 )
#define JOB_INVOCATIONS           ( 10 )
#define JOB_LOOP_SCRIPT           ( 11 )
#define JOB_SUSPENDED_CONTEXTS    ( 12 )
#define JOB_THREAD_LOOP           ( 13 )
#define JOB_COUNT                 ( 16 )

class Job
{
public:
    int   Type;
    void* Data;

    Job( int type, void* data );

    static void PushBack( int type );
    static void PushBack( int type, void* data );
    static uint PushBack( const Job& job );
    static void PushFront( const Job& job );
    static Job  PopFront();
    static void Erase( int type );
    static uint Count();
};

#endif // __JOBS__
