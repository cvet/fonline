#include "StdAfx.h"
#include "Jobs.h"
#include "Mutex.h"

static Mutex JobLocker; // Defense code from simultaneously execution

typedef vector<Job> JobVec;
typedef vector<Job>::iterator JobVecIt;
JobVec Jobs;

Job::Job()
{
	Type=JOB_NOP;
	Data=NULL;
}

Job::Job(int type, void* data)
{
	Type=type;
	Data=data;
}

void Job::Add(int type, void* data)
{
	SCOPE_LOCK(JobLocker);
	Jobs.push_back(Job(type,data));
}

void Job::Add(const Job& job)
{
	SCOPE_LOCK(JobLocker);
	Jobs.push_back(job);
}

Job Job::PopFront()
{
	SCOPE_LOCK(JobLocker);
	if(Jobs.empty()) return Job(JOB_NOP,NULL);
	Job job=Jobs.front();
	Jobs.erase(Jobs.begin());
	return job;
}

void Job::Erase(int type, void* data)
{
	SCOPE_LOCK(JobLocker);
	for(JobVecIt it=Jobs.begin(),end=Jobs.end();it!=end;++it)
	{
		Job& job=*it;
		if(job.Type==type && job.Data==data)
		{
			Jobs.erase(it);
			break;
		}
	}
}