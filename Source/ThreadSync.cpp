#include "StdAfx.h"
#include "ThreadSync.h"
#include <Windows.h>
#include "Mutex.h"

static Mutex SyncLocker; // Defense code from simultaneously execution

SyncObject::SyncObject()
{
	curMngr=NULL;
}

SyncObject::~SyncObject()
{
	Unlock();
}

void SyncObject::Lock()
{
	SyncLocker.Lock();

	SyncManager* curm=SyncManager::GetForCurThread();

	// Object is free
	if(!curMngr)
	{
		curMngr=curm;
		curMngr->lockedObjects.push_back(this);
	}
	// Object busy by another thread
	else if(curm!=curMngr)
	{
		// Another thread in wait state
		if(curMngr->isWaiting)
		{
			// Pick from waiting thread
			SyncObjectVecIt it=std::find(curMngr->lockedObjects.begin(),curMngr->lockedObjects.end(),this);
			curMngr->lockedObjects.erase(it);
			curMngr->busyObjects.push_back(this);

			curMngr=curm;
			curMngr->lockedObjects.push_back(this);
		}
		// Another thread work with object
		else
		{
			// Go to wait state
			curm->isWaiting=true;
			curm->busyObjects.push_back(this);

			SyncLocker.Unlock();

			// Wait and try lock all busy objects
			while(true)
			{
				Sleep(0);

				SyncLocker.Lock();

				for(SyncObjectVecIt it=curm->busyObjects.begin();it!=curm->busyObjects.end();)
				{
					SyncObject* obj=*it;
					if(!obj->curMngr)
					{
						obj->curMngr=curm;
						curm->lockedObjects.push_back(obj);
						it=curm->busyObjects.erase(it);
					}
					else ++it;
				}

				bool all_done=curm->busyObjects.empty();
				if(all_done) curm->isWaiting=false;

				SyncLocker.Unlock();

				if(all_done) return;
			}
		}
	}
	// else object already locked by current thread

	SyncLocker.Unlock();
}

void SyncObject::Unlock()
{
	SCOPE_LOCK(SyncLocker);

	if(curMngr)
	{
		SyncObjectVecIt it=std::find(curMngr->lockedObjects.begin(),curMngr->lockedObjects.end(),this);
		curMngr->lockedObjects.erase(it);
	}
}

SyncManager::SyncManager()
{
	isWaiting=false;
	lockedObjects.reserve(100);
	busyObjects.reserve(100);
}

SyncManager::~SyncManager()
{
	UnlockAll();
}

void SyncManager::UnlockAll()
{
	SCOPE_LOCK(SyncLocker);

	for(SyncObjectVecIt it=lockedObjects.begin(),end=lockedObjects.end();it!=end;++it)
	{
		SyncObject* obj=*it;
		if(obj->curMngr==this) obj->curMngr=NULL;
	}
	lockedObjects.clear();
	busyObjects.clear();
}

SyncManager* SyncManager::GetForCurThread()
{
	static THREAD SyncManager* sync_mngr=NULL;
	if(!sync_mngr) sync_mngr=new(nothrow) SyncManager();
	return sync_mngr;
}



