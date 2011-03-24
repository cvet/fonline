#ifndef __MUTEX__
#define __MUTEX__

#include "Common.h"

#define DEFAULT_SPIN_COUNT    (4000)
#define SCOPE_LOCK(mutex)     volatile MutexLocker scope_lock__(mutex)
#define SCOPE_SPINLOCK(mutex) volatile MutexSpinlockLocker scope_lock__(mutex)


#ifdef FO_WINDOWS

	#ifdef FO_MSVC
		#include <intrin.h>
		#define InterlockedCompareExchange _InterlockedCompareExchange
		#define InterlockedExchange        _InterlockedExchange
		#define InterlockedIncrement       _InterlockedIncrement
		#define InterlockedDecrement       _InterlockedDecrement
	#endif

#elif FO_LINUX

	#include <pthread.h>
	#define CRITICAL_SECTION pthread_mutex_t
	#define EnterCriticalSection pthread_mutex_lock(pthread_mutex_t *);
	#define LeaveCriticalSection pthread_mutex_unlock(pthread_mutex_t *);
	#define DeleteCriticalSection pthread_mutex_destroy(pthread_mutex_t *);

pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
InitializeCriticalSectionAndSpinCount
SetCriticalSectionSpinCount

InterlockedCompareExchange __sync_val_compare_and_swap
InterlockedExchange __sync_lock_test_and_set
InterlockedIncrement __sync_add_and_fetch(&value, 1);
InterlockedDecrement __sync_sub_and_fetch(&value, 1);

CreateEvent 
CloseHandle 
ResetEvent 
SetEvent 
WaitForSingleObject 

http://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread.h.html

#endif


class Mutex
{
private:
	friend class MutexLocker;
	CRITICAL_SECTION mutexCS;
	Mutex(const Mutex&){}
	void operator=(const Mutex&){}

public:
	Mutex(){InitializeCriticalSectionAndSpinCount(&mutexCS,DEFAULT_SPIN_COUNT);}
	~Mutex(){DeleteCriticalSection(&mutexCS);}
	void SetSpinCount(int count){SetCriticalSectionSpinCount(&mutexCS,count);}
	void Lock(){EnterCriticalSection(&mutexCS);}
	bool TryLock(){return TryEnterCriticalSection(&mutexCS)!=FALSE;}
	void Unlock(){LeaveCriticalSection(&mutexCS);}
};

class MutexLocker
{
private:
	CRITICAL_SECTION* pCS;
	MutexLocker(){}
	MutexLocker(const MutexLocker&){}
	MutexLocker& operator=(const MutexLocker&){return *this;}

public:
	MutexLocker(Mutex& mutex):pCS(&mutex.mutexCS){EnterCriticalSection(pCS);}
	~MutexLocker(){LeaveCriticalSection(pCS);}
};

class MutexCode
{
private:
	HANDLE mcEvent;
	Mutex mcLocker;
	volatile long mcCounter;
	MutexCode(const MutexCode&){}
	MutexCode& operator=(const MutexCode&){return *this;}

public:
	MutexCode():mcCounter(0){mcEvent=CreateEvent(NULL,TRUE,TRUE,NULL);}
	~MutexCode(){CloseHandle(mcEvent);}
	void LockCode(){mcLocker.Lock(); ResetEvent(mcEvent); while(InterlockedCompareExchange(&mcCounter,0,0)) Sleep(0); mcLocker.Unlock();}
	void UnlockCode(){SetEvent(mcEvent);}
	void EnterCode(){mcLocker.Lock(); WaitForSingleObject(mcEvent,INFINITE); InterlockedIncrement(&mcCounter); mcLocker.Unlock();}
	void LeaveCode(){InterlockedDecrement(&mcCounter);}
};

class MutexSynchronizer
{
private:
	HANDLE msEvent;
	Mutex msLocker;
	volatile long msCounter;
	MutexSynchronizer(const MutexSynchronizer&){}
	MutexSynchronizer& operator=(const MutexSynchronizer&){return *this;}

public:
	MutexSynchronizer():msCounter(0){msEvent=CreateEvent(NULL,TRUE,TRUE,NULL);}
	~MutexSynchronizer(){CloseHandle(msEvent);}
	void Synchronize(long count){InterlockedIncrement(&msCounter); msLocker.Lock(); WaitForSingleObject(msEvent,INFINITE); InterlockedDecrement(&msCounter); ResetEvent(msEvent); while(InterlockedCompareExchange(&msCounter,0,0)!=count) Sleep(0); msLocker.Unlock();}
	void Resynchronize(){SetEvent(msEvent);}
	void SynchronizePoint(){InterlockedIncrement(&msCounter); msLocker.Lock(); WaitForSingleObject(msEvent,INFINITE); InterlockedDecrement(&msCounter); msLocker.Unlock();}
};

class MutexEvent
{
private:
	HANDLE meEvent;
	MutexEvent(const MutexEvent&){}
	MutexEvent& operator=(const MutexEvent&){return *this;}

public:
	MutexEvent(){meEvent=CreateEvent(NULL,TRUE,TRUE,NULL);}
	~MutexEvent(){CloseHandle(meEvent);}
	void Allow(){SetEvent(meEvent);}
	void Disallow(){ResetEvent(meEvent);}
	void Wait(){WaitForSingleObject(meEvent,INFINITE);}
};

class MutexSpinlock
{
private:
	friend class MutexSpinlockLocker;
	volatile long spinCounter;
	MutexSpinlock(const MutexSpinlock&){}
	MutexSpinlock& operator=(const MutexSpinlock&){return *this;}

public:
	MutexSpinlock():spinCounter(0){}
	void Lock(){while(InterlockedCompareExchange(&spinCounter,1,0)) /*Wait*/;}
	bool TryLock(){return InterlockedCompareExchange(&spinCounter,1,0)==0;}
	void Unlock(){InterlockedExchange(&spinCounter,0);}
};

class MutexSpinlockLocker
{
private:
	MutexSpinlock* spinLock;
	MutexSpinlockLocker(){}
	MutexSpinlockLocker(const MutexSpinlockLocker&){}
	MutexSpinlockLocker& operator=(const MutexSpinlockLocker&){return *this;}

public:
	MutexSpinlockLocker(MutexSpinlock& mutexSpinlock):spinLock(&mutexSpinlock){spinLock->Lock();}
	~MutexSpinlockLocker(){spinLock->Unlock();}
};

#endif // __MUTEX__