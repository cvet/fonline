#include "StdAfx.h"
#include "Timer.h"

#if defined(FO_WINDOWS)
	#include <Mmsystem.h>
	#if defined(FO_MSVC)
		#pragma comment(lib,"Winmm.lib")
	#elif defined(FO_GCC)
		// Linker option: -lwinmm
	#endif
#else // FO_LINUX
	//
#endif

int64 QPC_StartValue=0;
int64 QPC_Freq=0;
bool QPC_error=false;

uint LastGameTick=0;
uint SkipGameTick=0;
bool GameTickPaused=false;

#define MAX_ACCELERATE_TICK      (500)
#define MIN_ACCELERATE_TICK      (40)
int AcceleratorNum=0;
uint AcceleratorLastTick=0;
uint AcceleratorAccelerate=0;

void Timer::Init()
{
#if defined(FO_WINDOWS)
	if(!QueryPerformanceCounter((LARGE_INTEGER*)&QPC_StartValue) ||
	   !QueryPerformanceFrequency((LARGE_INTEGER*)&QPC_Freq)) QPC_error=true;

	timeBeginPeriod(1);
#else // FO_LINUX
	// Todo: linux
#endif

	LastGameTick=FastTick();
	SkipGameTick=LastGameTick;
	GameTickPaused=false;
}

uint Timer::FastTick()
{
#if defined(FO_WINDOWS)
	return timeGetTime();
#else // FO_LINUX
	// Todo: linux
#endif
}

double Timer::AccurateTick()
{
#if defined(FO_WINDOWS)
	int64 qpc_value;
	QueryPerformanceCounter((LARGE_INTEGER*)&qpc_value);
	return (double)((double)(qpc_value-QPC_StartValue)/(double)QPC_Freq*1000.0);
#else // FO_LINUX
	// Todo: linux
#endif
}

uint Timer::GameTick()
{
	if(GameTickPaused) return LastGameTick-SkipGameTick;
#if defined(FO_WINDOWS)
	return timeGetTime()-SkipGameTick;
#else // FO_LINUX
	// Todo: linux
#endif
}

void Timer::SetGamePause(bool pause)
{
	if(GameTickPaused==pause) return;
	if(pause) LastGameTick=FastTick();
	else SkipGameTick+=FastTick()-LastGameTick;
	GameTickPaused=pause;
}

bool Timer::IsGamePaused()
{
	return GameTickPaused;
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

void Timer::GetCurrentDateTime(DateTime& dt)
{
#if defined(FO_WINDOWS)
	SYSTEMTIME st;
	GetLocalTime(&st);
	dt.Year=st.wYear,dt.Month=st.wMonth,dt.DayOfWeek=st.wDayOfWeek,
		dt.Day=st.wDay,dt.Hour=st.wHour,dt.Minute=st.wMinute,
		dt.Second=st.wSecond,dt.Milliseconds=st.wMilliseconds;
#else // FO_LINUX
	struct tm* dt_;
	__time64_t long_time;
	time(&long_time);
	dt_=localtime(&long_time);
	dt.Year=dt_->tm_year,dt.Month=dt_->tm_mday,dt.DayOfWeek=dt_->tm_wday,
		dt.Day=dt_->tm_mday,dt.Hour=dt_->tm_hour,dt.Minute=dt_->tm_min,
		dt.Second=dt_->tm_sec;
	dt.Milliseconds=0; // Todo: set something, or exclude ms from DateTime
#endif
}

bool Timer::DateTimeToFullTime(DateTime& dt, uint64& ft)
{
#if defined(FO_WINDOWS)
	union {FILETIME ft; ULARGE_INTEGER ul;} ft_;
	SYSTEMTIME st={dt.Year,dt.Month,dt.DayOfWeek,dt.Day,dt.Hour,dt.Minute,dt.Second,dt.Milliseconds};
	if(!SystemTimeToFileTime(&st,&ft_.ft)) return false;
	ft=PACKUINT64(ft_.ul.HighPart,ft_.ul.LowPart);
#else // FO_LINUX
	// Todo: linux
#endif
	return true;
}

bool Timer::FullTimeToDateTime(uint64& ft, DateTime& dt)
{
#if defined(FO_WINDOWS)
	SYSTEMTIME st;
	union {FILETIME ft; ULARGE_INTEGER ul;} ft_;
	ft_.ul.HighPart=(ft>>32);
	ft_.ul.LowPart=(ft&0xFFFFFFFF);
	if(!FileTimeToSystemTime(&ft_.ft,&st)) return false;
	dt.Year=st.wYear,dt.Month=st.wMonth,dt.DayOfWeek=st.wDayOfWeek,
		dt.Day=st.wDay,dt.Hour=st.wHour,dt.Minute=st.wMinute,
		dt.Second=st.wSecond,dt.Milliseconds=st.wMilliseconds;
#else // FO_LINUX
	// Todo: linux
#endif
	return true;
}

int Timer::GetTimeDifference(DateTime& dt1, DateTime& dt2)
{
	uint64 ft1,ft2;
	if(!DateTimeToFullTime(dt1,ft1) || !DateTimeToFullTime(dt2,ft2)) return 0;
	return (int)((ft1-ft2)/10000000);
}

bool Timer::ContinueTime(DateTime& td, int seconds)
{
	uint64 ft;
	if(!DateTimeToFullTime(td,ft)) return false;
	ft+=int64(seconds)*10000000;
	if(!FullTimeToDateTime(ft,td)) return false;
	return true;
}

uint Timer::GetFullSecond(ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second)
{
	DateTime dt={year,month,0,day,hour,minute,second,0};
	uint64 ft;
	if(!Timer::DateTimeToFullTime(dt,ft)) WriteLogF(_FUNC_," - Args<%u,%u,%u,%u,%u,%u>.\n",year,month,day,hour,minute,second);
	ft-=GameOpt.YearStartFT;
	return (uint)(ft/10000000);
}

DateTime Timer::GetGameTime(uint full_second)
{
	uint64 ft=GameOpt.YearStartFT+uint64(full_second)*10000000;
	DateTime dt;
	if(!Timer::FullTimeToDateTime(ft,dt)) WriteLogF(_FUNC_," - Full second<%u>.\n",full_second);
	return dt;
}

uint Timer::GameTimeMonthDay(ushort year, ushort month)
{
	switch(month)
	{
	case 1:case 3:case 5:case 7:case 8:case 10:case 12: // 31
		return 31;
	case 2: // 28-29
		if(year%4) return 28;
		return 29;
	default: // 30
		return 30;
	}
	return 0;
}

void Timer::ProcessGameTime()
{
	uint tick=Timer::GameTick();
	uint dt=tick-GameOpt.GameTimeTick;
	uint delta_second=dt/1000*GameOpt.TimeMultiplier+dt%1000*GameOpt.TimeMultiplier/1000;
	uint fs=GameOpt.FullSecondStart+delta_second;
	if(GameOpt.FullSecond!=fs)
	{
		GameOpt.FullSecond=fs;
		DateTime st=GetGameTime(GameOpt.FullSecond);
		GameOpt.Year=st.Year;
		GameOpt.Month=st.Month;
		GameOpt.Day=st.Day;
		GameOpt.Hour=st.Hour;
		GameOpt.Minute=st.Minute;
		GameOpt.Second=st.Second;
	}
}