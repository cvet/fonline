#ifndef __TIMER__
#define __TIMER__

#include <windows.h>

namespace Timer
{
	void Init();

	DWORD FastTick();
	double AccurateTick();

	DWORD GameTick();
	void SetGamePause(bool pause);
	bool IsGamePaused();

	void StartAccelerator(int num);
	bool ProcessAccelerator(int num);
	int GetAcceleratorNum();

	int GetTimeDifference(SYSTEMTIME& st1, SYSTEMTIME& st2);
	void ContinueTime(SYSTEMTIME& st, int seconds);
};

#endif // __TIMER__