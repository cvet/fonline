#ifndef __MUTEX__
#define __MUTEX__

#include "Common.h"
#include <intrin.h>

#define SCOPE_LOCK(mutex) volatile MutexLocker scope_lock__(mutex)
#define SCOPE_SPINLOCK(mutex) volatile MutexSpinlockLocker scope_spinlock__(mutex)

class Mutex
{
private:
	friend class MutexLocker;
	CRITICAL_SECTION mutexCS;
	Mutex(const Mutex&){}
	void operator=(const Mutex&){}

public:
	Mutex(){InitializeCriticalSection(&mutexCS);}
	~Mutex(){DeleteCriticalSection(&mutexCS);}
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
	void LockCode(){mcLocker.Lock(); ResetEvent(mcEvent); while(_InterlockedCompareExchange(&mcCounter,0,0)) Sleep(0); mcLocker.Unlock();}
	void UnlockCode(){SetEvent(mcEvent);}
	void EnterCode(){mcLocker.Lock(); WaitForSingleObject(mcEvent,INFINITE); _InterlockedIncrement(&mcCounter); mcLocker.Unlock();}
	void LeaveCode(){_InterlockedDecrement(&mcCounter);}
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
	void Synchronize(long count){_InterlockedIncrement(&msCounter); msLocker.Lock(); WaitForSingleObject(msEvent,INFINITE); _InterlockedDecrement(&msCounter); ResetEvent(msEvent); while(_InterlockedCompareExchange(&msCounter,0,0)!=count) Sleep(0); msLocker.Unlock();}
	void Resynchronize(){SetEvent(msEvent);}
	void SynchronizePoint(){_InterlockedIncrement(&msCounter); msLocker.Lock(); WaitForSingleObject(msEvent,INFINITE); _InterlockedDecrement(&msCounter); msLocker.Unlock();}
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
	void Lock(){while(_InterlockedCompareExchange(&spinCounter,1,0)) /* Wait */;}
	bool TryLock(){return _InterlockedCompareExchange(&spinCounter,1,0)==0;}
	void Unlock(){_InterlockedExchange(&spinCounter,0);}
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