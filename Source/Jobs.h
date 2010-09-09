#ifndef __JOBS__
#define __JOBS__

// Server job types
#define JOB_NOP                (0)
#define JOB_CRITTER            (1)
#define JOB_MAP                (2)
#define JOB_TIME_EVENT         (3)
#define JOB_GARBAGE            (4)
#define JOB_GAME_LOOP          (5)

class Job
{
public:
	int Type;
	void* Data;

	Job();
	Job(int type, void* data);

	static void Add(int type, void* data);
	static void Add(const Job& job);
	static Job PopFront();
	static void Erase(int type, void* data);
};

#endif // __JOBS__