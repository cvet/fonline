#ifndef __THREADING__
#define __THREADING__

#include "Common.h"

#if defined ( FO_WINDOWS ) || defined ( FO_LINUX ) || defined ( FO_MAC ) || defined ( FO_ANDROID )
void Thread_Sleep( uint ms );
#endif

#ifndef NO_THREADING

# include <mutex>
# include <thread>

// TLS
# if defined ( FO_MSVC )
#  define THREAD    __declspec( thread )
# elif defined ( FO_GCC )
#  define THREAD    __thread
# else
#  error No TLS
# endif

# define SCOPE_LOCK( mutex )    volatile MutexLocker scope_lock_ ## mutex( mutex )

class Mutex
{
    std::mutex mutex;

public:
    Mutex() = default;
    Mutex( const Mutex& ) = delete;
    Mutex& operator=( const Mutex& ) = delete;

    void Lock()    { mutex.lock(); }
    bool TryLock() { return mutex.try_lock(); }
    void Unlock()  { mutex.unlock(); }
};

class MutexLocker
{
    Mutex& pMutex;

public:
    MutexLocker() = delete;
    MutexLocker( const MutexLocker& ) = delete;
    MutexLocker& operator=( const MutexLocker& ) = delete;
    MutexLocker( Mutex& mutex ): pMutex( mutex ) { pMutex.Lock(); }
    ~MutexLocker() { pMutex.Unlock(); }
};

class Thread
{
    std::thread thread;

    static THREAD char threadName[ 64 ];
    static SizeTStrMap threadNames;
    static Mutex       threadNamesLocker;

public:
    using ThreadFunc = std::function< void(void*) >;

    Thread() = default;
    ~Thread();
    Thread( const Thread& ) = delete;
    Thread& operator=( const Thread& ) = delete;

    void Start( ThreadFunc func, const string& name, void* arg = nullptr );
    void Wait();

    static size_t      GetCurrentId();
    static void        SetCurrentName( const char* name );
    static const char* GetCurrentName();
    static const char* FindName( size_t thread_id );
};

#endif

#endif // __THREADING__
