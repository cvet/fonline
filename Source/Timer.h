#ifndef __TIMER__
#define __TIMER__

#include <windows.h>
/*struct SysTime {
	WORD Year;
	WORD Month;
	WORD DayOfWeek;
	WORD Day;
	WORD Hour;
	WORD Minute;
	WORD Second;
	WORD Milliseconds;
};*/

namespace Timer
{
	void Init();
	unsigned long FastTick();
	double AccurateTick();

	void StartAccelerator(int num);
	bool ProcessAccelerator(int num);
	int GetAcceleratorNum();

	int GetTimeDifference(SYSTEMTIME& st1, SYSTEMTIME& st2);
	void ContinueTime(SYSTEMTIME& st, int minutes);
};

#endif // __TIMER__