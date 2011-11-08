#include "StdAfx.h"
#include "Timer.h"

#if defined ( FO_WINDOWS )
# include <Mmsystem.h>
# if defined ( FO_MSVC )
#  pragma comment(lib,"Winmm.lib")
# elif defined ( FO_GCC )
// Linker option: -lwinmm
# endif
#else // FO_LINUX
# include <sys/time.h>
#endif

int64 QPC_StartValue = 0;
int64 QPC_Freq = 0;
bool  QPC_error = false;

uint  LastGameTick = 0;
uint  SkipGameTick = 0;
bool  GameTickPaused = false;

#define MAX_ACCELERATE_TICK    ( 500 )
#define MIN_ACCELERATE_TICK    ( 40 )
int  AcceleratorNum = 0;
uint AcceleratorLastTick = 0;
uint AcceleratorAccelerate = 0;

void Timer::Init()
{
    #if defined ( FO_WINDOWS )
    if( !QueryPerformanceCounter( (LARGE_INTEGER*) &QPC_StartValue ) ||
        !QueryPerformanceFrequency( (LARGE_INTEGER*) &QPC_Freq ) )
        QPC_error = true;

    timeBeginPeriod( 1 );
    #endif

    LastGameTick = FastTick();
    SkipGameTick = LastGameTick;
    GameTickPaused = false;
}

uint Timer::FastTick()
{
    #if defined ( FO_WINDOWS )
    return timeGetTime();
    #else // FO_LINUX
    struct timeval tv;
    gettimeofday( &tv, NULL );
    return (uint) ( tv.tv_sec * 1000 + tv.tv_usec / 1000 );
    #endif
}

double Timer::AccurateTick()
{
    #if defined ( FO_WINDOWS )
    int64 qpc_value;
    QueryPerformanceCounter( (LARGE_INTEGER*) &qpc_value );
    return (double) ( (double) ( qpc_value - QPC_StartValue ) / (double) QPC_Freq * 1000.0 );
    #else // FO_LINUX
    struct timeval tv;
    gettimeofday( &tv, NULL );
    return (double) ( tv.tv_sec * 1000000 + tv.tv_usec ) / 1000.0;
    #endif
}

uint Timer::GameTick()
{
    if( GameTickPaused )
        return LastGameTick - SkipGameTick;
    return FastTick() - SkipGameTick;
}

void Timer::SetGamePause( bool pause )
{
    if( GameTickPaused == pause )
        return;
    if( pause )
        LastGameTick = FastTick();
    else
        SkipGameTick += FastTick() - LastGameTick;
    GameTickPaused = pause;
}

bool Timer::IsGamePaused()
{
    return GameTickPaused;
}

void Timer::StartAccelerator( int num )
{
    AcceleratorNum = num;
    AcceleratorLastTick = FastTick();
    AcceleratorAccelerate = MAX_ACCELERATE_TICK;
}

bool Timer::ProcessAccelerator( int num )
{
    if( AcceleratorNum != num )
        return false;
    if( AcceleratorLastTick + AcceleratorAccelerate > FastTick() )
        return false;
    // AcceleratorData[num].Accelerate/=2;
    // if(AcceleratorData[num].Accelerate<MIN_ACCELERATE_TICK) AcceleratorData[num].Accelerate=MIN_ACCELERATE_TICK;
    AcceleratorAccelerate = MIN_ACCELERATE_TICK;
    AcceleratorLastTick = FastTick();
    return true;
}

int Timer::GetAcceleratorNum()
{
    return AcceleratorNum;
}

#include <time.h>
void Timer::GetCurrentDateTime( DateTime& dt )
{
    #if defined ( FO_WINDOWS )
    SYSTEMTIME st;
    GetLocalTime( &st );
    dt.Year = st.wYear, dt.Month = st.wMonth, dt.DayOfWeek = st.wDayOfWeek,
    dt.Day = st.wDay, dt.Hour = st.wHour, dt.Minute = st.wMinute,
    dt.Second = st.wSecond, dt.Milliseconds = st.wMilliseconds;
    #else // FO_LINUX
    time_t     long_time;
    time( &long_time );
    struct tm* lt = localtime( &long_time );
    dt.Year = lt->tm_year + 1900, dt.Month = lt->tm_mon + 1, dt.DayOfWeek = lt->tm_wday,
    dt.Day = lt->tm_mday, dt.Hour = lt->tm_hour, dt.Minute = lt->tm_min,
    dt.Second = lt->tm_sec;
    struct timeval tv;
    gettimeofday( &tv, NULL );
    dt.Milliseconds = tv.tv_usec / 1000;
    #endif
}

bool Timer::DateTimeToFullTime( DateTime& dt, uint64& ft )
{
    // Minor year
    ft = (uint64) ( dt.Year - 1601 ) * 365 * 24 * 60 * 60 * 1000 * 1000;

    // Leap days
    uint leap_days = ( dt.Year - 1601 ) / 4;
    leap_days += ( dt.Year - 1601 ) / 400;
    leap_days -= ( dt.Year - 1601 ) / 100;

    // Current month
    static const uint count1[ 12 ] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    static const uint count2[ 12 ] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }; // Leap
    if( dt.Year % 400 == 0 || ( dt.Year % 4 == 0 && dt.Year % 100 != 0 ) )
        ft += (uint64) ( count2[ dt.Month - 1 ] ) * 24 * 60 * 60 * 1000 * 1000;
    else
        ft += (uint64) ( count1[ dt.Month - 1 ] ) * 24 * 60 * 60 * 1000 * 1000;

    // Other calculations
    ft += (uint64) ( dt.Day - 1 + leap_days ) * 24 * 60 * 60 * 1000 * 1000;
    ft += (uint64) dt.Hour * 60 * 60 * 1000 * 1000;
    ft += (uint64) dt.Minute * 60 * 1000 * 1000;
    ft += (uint64) dt.Second * 1000 * 1000;
    ft += (uint64) dt.Milliseconds * 1000;
    ft *= (uint64) 10;

    return true;
}

ushort Timer::CountLeapYears(ushort from_year, ushort to_year)
{
	from_year--;
	to_year--;
	ushort leap_before_from = from_year/4 - from_year/100 + from_year/400;
	ushort leap_before_to = to_year/4 - to_year/100 + to_year/400;
	return leap_before_to-leap_before_from;
}

bool Timer::FullTimeToDateTime( uint64& ft, DateTime& dt )
{
    // Base
    ft /= 10000;
    dt.Milliseconds = ft % 1000;
    ft /= 1000;
    dt.Second = ft % 60;
    ft /= 60;
    dt.Minute = ft % 60;
    ft /= 60;
    dt.Hour = ft % 24;
    ft /= 24;

    // Year
    dt.Year = 1601;
    while( ft >= 366 )
    {
        ushort years = (uint) ft / 366;
        ft %= 366;
        ft += years-CountLeapYears(dt.Year,dt.Year+years);
        dt.Year += years;
    }

	if(ft==365 && !( dt.Year % 400 == 0 || ( dt.Year % 4 == 0 && dt.Year % 100 != 0 ) )) // full year in non-leap year
	{
		dt.Year++;
		ft=0;
	}

    // Month
    static const uint count1[ 12 ] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    static const uint count2[ 12 ] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }; // Leap
    const uint*       count = ( ( dt.Year % 400 == 0 || ( dt.Year % 4 == 0 && dt.Year % 100 != 0 ) ) ? count2 : count1 );
	int i = 0;
    for( ; i < 11; i++ )
		if( (uint) ft >= count[ i ] && (uint) ft < count[ i + 1 ] ) break;

    ft -= count[ i ];
    dt.Month = i + 1;

    // Day
    dt.Day = (ushort) ft + 1;

    // Day of week
    int a = ( 14 - dt.Month ) / 12;
    int y = dt.Year - a;
    int m = dt.Month + 12 * a - 2;
    dt.DayOfWeek = ( 7000 + ( dt.Day + y + y / 4 - y / 100 + y / 400 + ( 31 * m ) / 12 ) ) % 7;

    return true;
}

int Timer::GetTimeDifference( DateTime& dt1, DateTime& dt2 )
{
    uint64 ft1 = 0, ft2 = 0;
    if( !DateTimeToFullTime( dt1, ft1 ) || !DateTimeToFullTime( dt2, ft2 ) )
        return 0;
    return (int) ( ( ft1 - ft2 ) / 10000000 );
}

bool Timer::ContinueTime( DateTime& td, int seconds )
{
    uint64 ft;
    if( !DateTimeToFullTime( td, ft ) )
        return false;
    ft += (uint64) seconds * 10000000;
    if( !FullTimeToDateTime( ft, td ) )
        return false;
    return true;
}

uint Timer::GetFullSecond( ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second )
{
    DateTime dt = { year, month, 0, day, hour, minute, second, 0 };
    uint64   ft = 0;
    if( !Timer::DateTimeToFullTime( dt, ft ) )
        WriteLogF( _FUNC_, " - Args<%u,%u,%u,%u,%u,%u>.\n", year, month, day, hour, minute, second );
    ft -= PACKUINT64( GameOpt.YearStartFTHi, GameOpt.YearStartFTLo );
    return (uint) ( ft / 10000000 );
}

DateTime Timer::GetGameTime( uint full_second )
{
    uint64   ft = PACKUINT64( GameOpt.YearStartFTHi, GameOpt.YearStartFTLo ) + uint64( full_second ) * 10000000;
    DateTime dt;
    if( !Timer::FullTimeToDateTime( ft, dt ) )
        WriteLogF( _FUNC_, " - Full second<%u>.\n", full_second );
    return dt;
}

uint Timer::GameTimeMonthDay( ushort year, ushort month )
{
    switch( month )
    {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:    // 31
        return 31;
    case 2:     // 28-29
        if( year % 400 == 0 || ( year % 4 == 0 && year % 100 != 0 ) )
            return 29;
        return 28;
    default:     // 30
        return 30;
    }
    return 0;
}

void Timer::ProcessGameTime()
{
    uint tick = Timer::GameTick();
    uint dt = tick - GameOpt.GameTimeTick;
    uint delta_second = dt / 1000 * GameOpt.TimeMultiplier + dt % 1000 * GameOpt.TimeMultiplier / 1000;
    uint fs = GameOpt.FullSecondStart + delta_second;
    if( GameOpt.FullSecond != fs )
    {
        GameOpt.FullSecond = fs;
        DateTime st = GetGameTime( GameOpt.FullSecond );
        GameOpt.Year = st.Year;
        GameOpt.Month = st.Month;
        GameOpt.Day = st.Day;
        GameOpt.Hour = st.Hour;
        GameOpt.Minute = st.Minute;
        GameOpt.Second = st.Second;
    }
}
