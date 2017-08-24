#include "Threading.h"

#if defined ( FO_WINDOWS ) || defined ( FO_LINUX ) || defined ( FO_MAC ) || defined ( FO_ANDROID )
void Thread_Sleep( uint ms )
{
    # if defined ( FO_WINDOWS )
    Sleep( ms );
    # else
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = ( ms % 1000 ) * 1000000;
    while( nanosleep( &req, &req ) == -1 && errno == EINTR )
        continue;
    # endif
}
#endif

#ifndef NO_THREADING

# ifndef FO_WINDOWS
// Mutex static stuff
bool                Mutex::attrInitialized = false;
pthread_mutexattr_t Mutex::mutexAttr;
# endif

# ifdef FO_WINDOWS
static DWORD WINAPI ThreadBeginExecution( void* args )
# else
static void* ThreadBeginExecution( void* args )
# endif
{
    void** args_ = (void**) args;
    void   ( * func )( void* ) = ( void ( * )( void* ) )args_[ 0 ];
    void*  func_arg = args_[ 1 ];
    char*  name = (char*) args_[ 2 ];
    Thread::SetCurrentName( name );
    delete[] name;
    free( args );
    func( func_arg );
    # ifdef FO_WINDOWS
    return 0;
    # else
    return nullptr;
    # endif
}

Thread::Thread()
{
    isStarted = false;
}

void Thread::Start( void ( * func )( void* ), const char* name, void* arg /* = NULL */ )
{
    void** args = (void**) malloc( sizeof( void* ) * 3 );
    char*  name_ = Str::Duplicate( name );
    args[ 0 ] = (void*) func, args[ 1 ] = arg, args[ 2 ] = name_;
    # ifdef FO_WINDOWS
    threadId = CreateThread( nullptr, 0, ThreadBeginExecution, args, 0, nullptr );
    isStarted = ( threadId != nullptr );
    # else
    isStarted = ( pthread_create( &threadId, nullptr, ThreadBeginExecution, args ) == 0 );
    # endif
    RUNTIME_ASSERT( isStarted );
}

void Thread::Wait()
{
    if( isStarted )
    {
        # ifdef FO_WINDOWS
        WaitForSingleObject( threadId, INFINITE );
        # else
        pthread_join( threadId, nullptr );
        # endif
        isStarted = false;
    }
}

void Thread::Release()
{
    isStarted = false;
    threadId = 0;
}

THREAD char Thread::threadName[ 64 ] = { 0 };
SizeTStrMap Thread::threadNames;
Mutex       Thread::threadNamesLocker;

size_t Thread::GetCurrentId()
{
    # ifdef FO_WINDOWS
    return (size_t) GetCurrentThreadId();
    # else
    return (size_t) pthread_self();
    # endif
}

void Thread::SetCurrentName( const char* name )
{
    if( threadName[ 0 ] )
        return;

    Str::Copy( threadName, name );
    SCOPE_LOCK( threadNamesLocker );
    threadNames.insert( std::make_pair( GetCurrentId(), threadName ) );
}

const char* Thread::GetCurrentName()
{
    return threadName;
}

const char* Thread::FindName( uint thread_id )
{
    SCOPE_LOCK( threadNamesLocker );
    auto it = threadNames.find( thread_id );
    return it != threadNames.end() ? it->second.c_str() : nullptr;
}

#endif
