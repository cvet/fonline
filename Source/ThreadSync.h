#ifndef __THREAD_SYNC__
#define __THREAD_SYNC__

#include "Common.h"

#define SYNC_LOCK( obj )    ( obj )->Sync.Lock()

class SyncObject;
class SyncManager;
typedef vector< SyncObject* >  SyncObjectVec;
typedef vector< SyncManager* > SyncManagerVec;

class SyncObject
{
private:
    friend class SyncManager;
    SyncManager* curMngr;

public:
    SyncObject();
    void Lock();
    void Unlock();
};

class SyncManager
{
private:
    friend class SyncObject;
    volatile bool isWaiting;
    volatile int  threadPriority;
    SyncObjectVec lockedObjects;
    SyncObjectVec busyObjects;
    IntVec        priorityStack;

public:
    SyncManager();
    ~SyncManager();
    void PushPriority( int priority );
    void PopPriority();
    void UnlockAll();
    void Suspend();
    void Resume();

    static SyncManagerVec Managers;
    static SyncManager* GetForCurThread();
};

#endif // __THREAD_SYNC__
