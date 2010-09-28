#ifndef __MUTEX__
#define __MUTEX__

#include <Windows.h>

#define SCOPE_LOCK(mutex) volatile MutexLocker scope_lock__(mutex)

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
	void operator=(const MutexCode&){}

public:
	MutexCode():mcCounter(0){mcEvent=CreateEvent(NULL,TRUE,TRUE,NULL);}
	~MutexCode(){CloseHandle(mcEvent);}
	void LockCode(){mcLocker.Lock(); ResetEvent(mcEvent); while(mcCounter) Sleep(0); mcLocker.Unlock();}
	void UnlockCode(){SetEvent(mcEvent);}
	void EnterCode(){mcLocker.Lock(); WaitForSingleObject(mcEvent,INFINITE); mcCounter++; mcLocker.Unlock();}
	void LeaveCode(){mcCounter--;}
};

class MutexSynchronizer
{
private:
	HANDLE msEvent;
	Mutex msLocker;
	volatile long msCounter;
	MutexSynchronizer(const MutexSynchronizer&){}
	void operator=(const MutexSynchronizer&){}

public:
	MutexSynchronizer():msCounter(0){msEvent=CreateEvent(NULL,TRUE,TRUE,NULL);}
	~MutexSynchronizer(){CloseHandle(msEvent);}
	void Synchronize(long count){msCounter++; msLocker.Lock(); WaitForSingleObject(msEvent,INFINITE); msCounter--; ResetEvent(msEvent); while(msCounter!=count) Sleep(0); msLocker.Unlock();}
	void Resynchronize(){SetEvent(msEvent);}
	void SynchronizePoint(){msCounter++; msLocker.Lock(); WaitForSingleObject(msEvent,INFINITE); msCounter--; msLocker.Unlock();}
};

class MutexEvent
{
private:
	HANDLE meEvent;
	MutexEvent(const MutexEvent&){}
	void operator=(const MutexEvent&){}

public:
	MutexEvent(){meEvent=CreateEvent(NULL,TRUE,TRUE,NULL);}
	~MutexEvent(){CloseHandle(meEvent);}
	void Allow(){SetEvent(meEvent);}
	void Disallow(){ResetEvent(meEvent);}
	void Wait(){WaitForSingleObject(meEvent,INFINITE);}
};

#endif // __MUTEX__