#include "StdAfx.h"
#include "Jobs.h"
#include "Mutex.h"
#include "Map.h"
#include "Critter.h"
#include "Item.h"
#include "Vars.h"

static Mutex JobLocker; // Defense code from simultaneously execution

typedef deque<Job> JobDeque;
typedef deque<Job>::iterator JobDequeIt;
static JobDeque Jobs;

Job::Job():
Type(JOB_NOP),
Data(NULL),
ThreadId(0)
{
}

Job::Job(int type, void* data, bool cur_thread):
Type(type),
Data(data),
ThreadId(0)
{
	if(cur_thread) ThreadId=GetCurrentThreadId();
}

void Job::PushBack(int type)
{
	SCOPE_LOCK(JobLocker);
	Jobs.push_back(Job(type,NULL,0));
}

void Job::PushBack(int type, void* data)
{
	SCOPE_LOCK(JobLocker);
	Jobs.push_back(Job(type,data,0));
}

void Job::PushBack(const Job& job)
{
	SCOPE_LOCK(JobLocker);
	Jobs.push_back(job);
}

void Job::PushFront(const Job& job)
{
	SCOPE_LOCK(JobLocker);
	Jobs.push_front(job);
}

Job Job::PopFront()
{
	SCOPE_LOCK(JobLocker);

	if(Jobs.empty()) return Job(JOB_NOP,NULL,false);

	Job job=Jobs.front();
	Jobs.pop_front();

	// Check owner
	if(job.ThreadId && job.ThreadId!=GetCurrentThreadId())
	{
		Job job_next=PopFront();
		Jobs.push_front(job);
		return job_next;
	}

	return job;
}

void Job::Erase(int type)
{
	SCOPE_LOCK(JobLocker);

	for(JobDequeIt it=Jobs.begin();it!=Jobs.end();)
	{
		Job& job=*it;
		if(job.Type==type)
			it=Jobs.erase(it);
		else
			++it;
	}
}

size_t Job::Count()
{
	SCOPE_LOCK(JobLocker);
	size_t count=Jobs.size();
	return count;
}

// Deferred releasing
static CrVec DeferredReleaseCritters;
static DwordVec DeferredReleaseCrittersCycle;
static MapVec DeferredReleaseMaps;
static DwordVec DeferredReleaseMapsCycle;
static LocVec DeferredReleaseLocs;
static DwordVec DeferredReleaseLocsCycle;
static ItemPtrVec DeferredReleaseItems;
static DwordVec DeferredReleaseItemsCycle;
static VarsVec DeferredReleaseVars;
static DwordVec DeferredReleaseVarsCycle;
static DWORD DeferredReleaseCycle=0;
static Mutex DeferredReleaseLocker;

void Job::DeferredRelease(Critter* cr)
{
	SCOPE_LOCK(DeferredReleaseLocker);
	DeferredReleaseCritters.push_back(cr);
	DeferredReleaseCrittersCycle.push_back(DeferredReleaseCycle);
}

void Job::DeferredRelease(Map* map)
{
	SCOPE_LOCK(DeferredReleaseLocker);
	DeferredReleaseMaps.push_back(map);
	DeferredReleaseMapsCycle.push_back(DeferredReleaseCycle);
}

void Job::DeferredRelease(Location* loc)
{
	SCOPE_LOCK(DeferredReleaseLocker);
	DeferredReleaseLocs.push_back(loc);
	DeferredReleaseLocsCycle.push_back(DeferredReleaseCycle);
}

void Job::DeferredRelease(Item* item)
{
	SCOPE_LOCK(DeferredReleaseLocker);
	DeferredReleaseItems.push_back(item);
	DeferredReleaseItemsCycle.push_back(DeferredReleaseCycle);
}

void Job::DeferredRelease(GameVar* var)
{
	SCOPE_LOCK(DeferredReleaseLocker);
	DeferredReleaseVars.push_back(var);
	DeferredReleaseVarsCycle.push_back(DeferredReleaseCycle);
}

void Job::SetDeferredReleaseCycle(DWORD cycle)
{
	SCOPE_LOCK(DeferredReleaseLocker);
	DeferredReleaseCycle=cycle;
}

void Job::ProcessDeferredReleasing()
{
	SCOPE_LOCK(DeferredReleaseLocker);

	// Wait at least 3 cycles
	if(DeferredReleaseCycle<3) return;

//================================================================================================
#define PROCESS_DEFERRED_RELEASING(name) \
	{\
		size_t del_count=0;\
		for(size_t i=0,j=DeferredRelease##name##Cycle.size();i<j;i++)\
		{\
			if(DeferredRelease##name##Cycle[i]>=DeferredReleaseCycle-2) break;\
			else del_count++;\
		}\
		if(del_count)\
		{\
			for(size_t i=0;i<del_count;i++) DeferredRelease##name##[i]->Release();\
			DeferredRelease##name##.erase(DeferredRelease##name##.begin(),DeferredRelease##name##.begin()+del_count);\
			DeferredRelease##name##Cycle.erase(DeferredRelease##name##Cycle.begin(),DeferredRelease##name##Cycle.begin()+del_count);\
		}\
	}\
//================================================================================================

	PROCESS_DEFERRED_RELEASING(Critters);
	PROCESS_DEFERRED_RELEASING(Maps);
	PROCESS_DEFERRED_RELEASING(Locs);
	PROCESS_DEFERRED_RELEASING(Items);
	PROCESS_DEFERRED_RELEASING(Vars);
}