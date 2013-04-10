#ifndef __MUTEX__
#define __MUTEX__

#include "Common.h"

#define DEFAULT_SPIN_COUNT              ( 4000 )
#define SCOPE_LOCK( mutex )                               volatile MutexLocker< decltype( mutex ) > scope_lock__( mutex )

extern void Thread_Sleep( uint ms ); // Definition in Common.cpp

#if defined ( FO_WINDOWS )

# ifdef FO_MSVC
#  include <intrin.h>
#  define InterlockedCompareExchange    _InterlockedCompareExchange
#  define InterlockedExchange           _InterlockedExchange
#  define InterlockedIncrement          _InterlockedIncrement
#  define InterlockedDecrement          _InterlockedDecrement
# endif

class Mutex
{
private:
    CRITICAL_SECTION mutexCS;
    Mutex( const Mutex& ) {}
    void operator=( const Mutex& ) {}

public:
    Mutex() { InitializeCriticalSectionAndSpinCount( &mutexCS, DEFAULT_SPIN_COUNT ); }
    ~Mutex() { DeleteCriticalSection( &mutexCS ); }
    void SetSpinCount( int count ) { SetCriticalSectionSpinCount( &mutexCS, count ); }
    void Lock()                    { EnterCriticalSection( &mutexCS ); }
    bool TryLock()                 { return TryEnterCriticalSection( &mutexCS ) != FALSE; }
    void Unlock()                  { LeaveCriticalSection( &mutexCS ); }
};

class MutexEvent
{
private:
    HANDLE mutexEvent;
    MutexEvent( const MutexEvent& ) {}
    MutexEvent& operator=( const MutexEvent& ) { return *this; }

public:
    MutexEvent() { mutexEvent = CreateEvent( NULL, TRUE, TRUE, NULL ); }
    ~MutexEvent() { CloseHandle( mutexEvent ); }
    void  Allow()     { SetEvent( mutexEvent ); }
    void  Disallow()  { ResetEvent( mutexEvent ); }
    void  Wait()      { WaitForSingleObject( mutexEvent, INFINITE ); }
    void* GetHandle() { return mutexEvent; }
};

#else // FO_LINUX

# define InterlockedCompareExchange( val, exch, comp )    __sync_val_compare_and_swap( val, comp, exch )
# define InterlockedExchange( val, newval )               __sync_lock_test_and_set( val, newval )
# define InterlockedIncrement( val )                      __sync_add_and_fetch( val, 1 )
# define InterlockedDecrement( val )                      __sync_sub_and_fetch( val, 1 )

class Mutex
{
private:
    pthread_mutex_t mutexCS;
    Mutex( const Mutex& ) {}
    void operator=( const Mutex& ) {}

    static bool                attrInitialized;
    static pthread_mutexattr_t mutexAttr;
    static void InitAttributes()
    {
        if( !attrInitialized )
        {
            pthread_mutexattr_init( &mutexAttr );
            pthread_mutexattr_settype( &mutexAttr, PTHREAD_MUTEX_RECURSIVE );
            attrInitialized = true;
        }
    }

public:
    Mutex()
    {
        InitAttributes();
        pthread_mutex_init( &mutexCS, &mutexAttr );
    }
    ~Mutex() { pthread_mutex_destroy( &mutexCS ); }
    void SetSpinCount( int count ) { /*Todo: linux*/ }
    void Lock()                    { pthread_mutex_lock( &mutexCS ); }
    bool TryLock()                 { return pthread_mutex_trylock( &mutexCS ) == 0; }
    void Unlock()                  { pthread_mutex_unlock( &mutexCS ); }
};

class MutexEvent
{
private:
    pthread_mutex_t ptMutex;
    pthread_cond_t  ptCond;
    bool            flag;
    MutexEvent( const MutexEvent& ) {}
    MutexEvent& operator=( const MutexEvent& ) { return *this; }

public:
    MutexEvent()
    {
        pthread_mutex_init( &ptMutex, NULL );
        pthread_cond_init( &ptCond, NULL );
        flag = true;
    }
    ~MutexEvent()
    {
        pthread_mutex_destroy( &ptMutex );
        pthread_cond_destroy( &ptCond );
    }
    void Allow()
    {
        pthread_mutex_lock( &ptMutex );
        flag = true;
        pthread_cond_broadcast( &ptCond );
        pthread_mutex_unlock( &ptMutex );
    }
    void Disallow()
    {
        pthread_mutex_lock( &ptMutex );
        flag = false;
        pthread_mutex_unlock( &ptMutex );
    }
    void Wait()
    {
        pthread_mutex_lock( &ptMutex );
        if( !flag ) pthread_cond_wait( &ptCond, &ptMutex );
        pthread_mutex_unlock( &ptMutex );
    }
};

#endif

class MutexCode
{
private:
    MutexEvent    mcEvent;
    Mutex         mcLocker;
    volatile long mcCounter;
    MutexCode( const MutexCode& ) {}
    MutexCode& operator=( const MutexCode& ) { return *this; }

public:
    MutexCode(): mcCounter( 0 ) {}
    void LockCode()
    {
        mcLocker.Lock();
        mcEvent.Disallow();
        while( InterlockedCompareExchange( &mcCounter, 0, 0 ) ) Thread_Sleep( 0 );
        mcLocker.Unlock();
    }
    void UnlockCode() { mcEvent.Allow(); }
    void EnterCode()
    {
        mcLocker.Lock();
        mcEvent.Wait();
        InterlockedIncrement( &mcCounter );
        mcLocker.Unlock();
    }
    void LeaveCode() { InterlockedDecrement( &mcCounter ); }
};

class MutexSynchronizer
{
private:
    MutexEvent    msEvent;
    Mutex         msLocker;
    volatile long msCounter;
    MutexSynchronizer( const MutexSynchronizer& ) {}
    MutexSynchronizer& operator=( const MutexSynchronizer& ) { return *this; }

public:
    MutexSynchronizer(): msCounter( 0 ) {}
    void Synchronize( long count )
    {
        InterlockedIncrement( &msCounter );
        msLocker.Lock();
        msEvent.Wait();
        InterlockedDecrement( &msCounter );
        msEvent.Disallow();
        while( InterlockedCompareExchange( &msCounter, 0, 0 ) != count ) Thread_Sleep( 0 );
        msLocker.Unlock();
    }
    void Resynchronize() { msEvent.Allow(); }
    void SynchronizePoint()
    {
        InterlockedIncrement( &msCounter );
        msLocker.Lock();
        msEvent.Wait();
        InterlockedDecrement( &msCounter );
        msLocker.Unlock();
    }
};

class MutexSpinlock
{
private:
    volatile long spinCounter;
    MutexSpinlock( const MutexSpinlock& ) {}
    MutexSpinlock& operator=( const MutexSpinlock& ) { return *this; }

public:
    MutexSpinlock(): spinCounter( 0 ) {}
    void Lock()    { while( InterlockedCompareExchange( &spinCounter, 1, 0 ) ) /*Wait*/; }
    bool TryLock() { return InterlockedCompareExchange( &spinCounter, 1, 0 ) == 0; }
    void Unlock()  { InterlockedExchange( &spinCounter, 0 ); }
};

template< class T >
class MutexLocker
{
private:
    T& pMutex;
    MutexLocker() {}
    MutexLocker( const MutexLocker& ) {}
    MutexLocker& operator=( const MutexLocker& ) { return *this; }

public:
    MutexLocker( T& mutex ): pMutex( mutex ) { pMutex.Lock(); }
    ~MutexLocker() { pMutex.Unlock(); }
};

#endif // __MUTEX__
