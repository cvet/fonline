#ifndef __MUTEX__
#define __MUTEX__

#include <Windows.h>

#define SCOPE_LOCK(mutex) Mutex::Unlocker scope_lock_##__LINE__=mutex.AutoLock()

class Mutex
{
private:
	CRITICAL_SECTION mutexCS;
	Mutex(const Mutex&){}
	void operator=(const Mutex&){}

public:
	Mutex(){InitializeCriticalSection(&mutexCS);}
	~Mutex(){DeleteCriticalSection(&mutexCS);}
	void Lock(){EnterCriticalSection(&mutexCS);}
	void Unlock(){LeaveCriticalSection(&mutexCS);}

	class Unlocker
	{
	private:
		CRITICAL_SECTION* pCS;

	public:
		Unlocker():pCS(NULL){}
		Unlocker(Unlocker& unlock){unlock.pCS=pCS; pCS=NULL;}
		Unlocker(CRITICAL_SECTION& cs){pCS=&cs; EnterCriticalSection(pCS);}
		Unlocker& operator=(Unlocker& unlock){unlock.pCS=pCS; pCS=NULL; return *this;}
		~Unlocker(){if(pCS) LeaveCriticalSection(pCS);}
	};

	Unlocker AutoLock(){return Unlocker(mutexCS);}
};

#endif // __MUTEX__