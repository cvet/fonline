#ifndef __THREAD_SYNC__
#define __THREAD_SYNC__

#include <vector>
using namespace std;

// Sync objects
// Critter
// Location
// Map
// GameVar
// ProtoItem ?
// Item
// NpcPlane ?
// Scenery ?

class SyncObject;
class SyncManager;
typedef vector<SyncObject*> SyncObjectVec;
typedef vector<SyncObject*>::iterator SyncObjectVecIt;
typedef vector<SyncManager*> SyncManagerVec;
typedef vector<SyncManager*>::iterator SyncManagerVecIt;

class SyncObject
{
private:
	friend SyncManager;
	SyncManager* curMngr;

public:
	SyncObject();
	~SyncObject();
	void Lock();
	void Unlock();
};

class SyncManager
{
private:
	friend SyncObject;
	bool isWaiting;
	SyncObjectVec lockedObjects;
	SyncObjectVec busyObjects;

public:
	SyncManager();
	~SyncManager();
	void UnlockAll();
	static SyncManager* GetForCurThread();
};

#endif // __THREAD_SYNC__