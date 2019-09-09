#pragma once

#include "Common.h"

#if defined ( FO_WINDOWS ) || defined ( FO_LINUX ) || defined ( FO_MAC )

# include <mutex>
# include <thread>

# define THREAD    thread_local
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

    static size_t      GetId();
    static void        SetName( const char* name );
    static const char* GetName();
    static const char* FindName( size_t thread_id );
    static void        Sleep( uint ms );
};

#else

# define THREAD
# define SCOPE_LOCK( mutex )

class Mutex
{
public:
    Mutex() = default;
    Mutex( const Mutex& ) = delete;
    Mutex& operator=( const Mutex& ) = delete;

    void Lock()    {}
    bool TryLock() {}
    void Unlock()  {}
};

class MutexLocker
{
public:
    MutexLocker() = delete;
    MutexLocker( const MutexLocker& ) = delete;
    MutexLocker& operator=( const MutexLocker& ) = delete;
    MutexLocker( Mutex& mutex ) {}
    ~MutexLocker() {}
};

class Thread
{
public:
    using ThreadFunc = std::function< void(void*) >;

    Thread() = default;
    ~Thread() = default;
    Thread( const Thread& ) = delete;
    Thread& operator=( const Thread& ) = delete;

    void Start( ThreadFunc func, const string& name, void* arg = nullptr );
    void Wait();

    static size_t      GetName();
    static void        SetName( const char* name );
    static const char* GetName();
    static const char* FindName( size_t thread_id );
    static void        Sleep( uint ms );
};

#endif
