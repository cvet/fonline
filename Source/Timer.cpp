#include "StdAfx.h"
#include "Timer.h"
#pragma comment(lib,"Winmm.lib")

__int64 QPC_StartValue=0;
__int64 QPC_Freq=0;
bool QPC_error=false;

#define MAX_ACCELERATE_TICK      (500)
#define MIN_ACCELERATE_TICK      (40)
int AcceleratorNum=0;
DWORD AcceleratorLastTick=0;
DWORD AcceleratorAccelerate=0;

void Timer::Init()
{
//	DWORD oldmask = SetThreadAffinityMask(GetCurrentThread(), 1); 
//	QueryPerformanceCounter(...); 
//	QueryPerformanceFrequency(...); 
//	SetThreadAffinityMask(GetCurrentThread(), oldmask);

	if(!QueryPerformanceCounter((LARGE_INTEGER*)&QPC_StartValue) ||
	   !QueryPerformanceFrequency((LARGE_INTEGER*)&QPC_Freq)) QPC_error=true;

	timeBeginPeriod(1);
//	TIMECAPS ptc;
//	timeGetDevCaps(&ptc,sizeof(ptc));
//	WriteLog("<%u,%u>\n",ptc.wPeriodMin,ptc.wPeriodMax);
}

unsigned long Timer::FastTick()
{
	return timeGetTime();
}

double Timer::AccurateTick()
{
//	return timeGetTime();
//	if(QPC_error) return GetTickCount();

	__int64 qpc_value;
	QueryPerformanceCounter((LARGE_INTEGER*)&qpc_value);
//	return (qpc_value-QPC_StartValue)/QPC_Freq;
	return (double)((double)(qpc_value-QPC_StartValue)/(double)QPC_Freq*1000.0);
}

void Timer::StartAccelerator(int num)
{
	AcceleratorNum=num;
	AcceleratorLastTick=FastTick();
	AcceleratorAccelerate=MAX_ACCELERATE_TICK;
}

bool Timer::ProcessAccelerator(int num)
{
	if(AcceleratorNum!=num) return false;
	if(AcceleratorLastTick+AcceleratorAccelerate>FastTick()) return false;
	//AcceleratorData[num].Accelerate/=2;
	//if(AcceleratorData[num].Accelerate<MIN_ACCELERATE_TICK) AcceleratorData[num].Accelerate=MIN_ACCELERATE_TICK;
	AcceleratorAccelerate=MIN_ACCELERATE_TICK;
	AcceleratorLastTick=FastTick();
	return true;
}

int Timer::GetAcceleratorNum()
{
	return AcceleratorNum;
}

int Timer::GetTimeDifference(SYSTEMTIME& st1, SYSTEMTIME& st2)
{
	FILETIMELI ft1,ft2;
	SystemTimeToFileTime(&st1,&ft1.ft);
	SystemTimeToFileTime(&st2,&ft2.ft);

	__int64 result=(ft1.ul.QuadPart-ft2.ul.QuadPart)/600000000;
	return (int)result;
}

void Timer::ContinueTime(SYSTEMTIME& st, int minutes)
{
	FILETIMELI ft;
	SystemTimeToFileTime(&st,&ft.ft);
	ft.ul.QuadPart+=__int64(minutes)*600000000;
	FileTimeToSystemTime(&ft.ft,&st);
}

