#include "StdAfx.h"
#include "Timer.h"

#ifdef FO_WINDOWS
# include <Mmsystem.h>
# if defined ( FO_MSVC )
#  pragma comment(lib,"Winmm.lib")
# elif defined ( FO_GCC )
// Linker option: -lwinmm
# endif
#else
# include <sys/time.h>
#endif

uint LastGameTick = 0;
uint SkipGameTick = 0;
bool GameTickPaused = false;

#ifdef FO_WINDOWS
int64 QPCStartValue = 0;
int64 QPCFrequency = 0;
#endif

#ifndef FO_WINDOWS
void SetTick();
void UpdateTick( void* );
Thread        TimerUpdateThread;
volatile uint TimerTick = 0;
volatile bool QuitTick = false;
# ifdef FO_OSX
double        InitialAccurateTick = 0;
# endif
#endif

#define MAX_ACCELERATE_TICK    ( 500 )
#define MIN_ACCELERATE_TICK    ( 40 )
int  AcceleratorNum = 0;
uint AcceleratorLastTick = 0;
uint AcceleratorAccelerate = 0;

void Timer::Init()
{
    #ifdef FO_WINDOWS
    QueryPerformanceCounter( (LARGE_INTEGER*) &QPCStartValue );
    QueryPerformanceFrequency( (LARGE_INTEGER*) &QPCFrequency );
    timeBeginPeriod( 1 );
    #else
    # ifdef FO_OSX
    InitialAccurateTick = AccurateTick();
    # endif
    SetTick();
    TimerUpdateThread.Start( UpdateTick, "UpdateTick" );
    #endif

    LastGameTick = FastTick();
    SkipGameTick = LastGameTick;
    GameTickPaused = false;
}

void Timer::Finish()
{
    #ifdef FO_WINDOWS
    timeEndPeriod( 1 );
    #else
    InterlockedExchange( &QuitTick, true );
    TimerUpdateThread.Wait();
    #endif
}

uint Timer::FastTick()
{
    #ifdef FO_WINDOWS
    return timeGetTime();
    #else
    return TimerTick;
    #endif
}

double Timer::AccurateTick()
{
    #ifdef FO_WINDOWS
    int64 qpc_value;
    QueryPerformanceCounter( (LARGE_INTEGER*) &qpc_value );
    return (double) ( (double) ( qpc_value - QPCStartValue ) / (double) QPCFrequency * 1000.0 );
    #else
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

void Timer::GetCurrentDateTime( DateTime& dt )
{
    #ifdef FO_WINDOWS
    SYSTEMTIME st;
    GetLocalTime( &st );
    dt.Year = st.wYear, dt.Month = st.wMonth, dt.DayOfWeek = st.wDayOfWeek,
    dt.Day = st.wDay, dt.Hour = st.wHour, dt.Minute = st.wMinute,
    dt.Second = st.wSecond, dt.Milliseconds = st.wMilliseconds;
    #else
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

void Timer::DateTimeToFullTime( const DateTime& dt, uint64& ft )
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
}

void Timer::FullTimeToDateTime( uint64 ft, DateTime& dt )
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
    int year = (int) ft / 365;
    int days = (int) ft % 365;
    days -= year / 4 + year / 400 - year / 100;
    while( days < 0 )
    {
        if( year % 400 == 0 || ( year % 4 == 0 && year % 100 != 0 ) )
            days += 366;
        else
            days += 365;
        year--;
    }
    dt.Year = 1601 + year;
    ft = days;

    // Month
    static const uint count1[ 13 ] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
    static const uint count2[ 13 ] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }; // Leap
    const uint*       count = ( ( dt.Year % 400 == 0 || ( dt.Year % 4 == 0 && dt.Year % 100 != 0 ) ) ? count2 : count1 );
    for( int i = 0; i < 12; i++ )
    {
        if( (uint) ft >= count[ i ] && (uint) ft < count[ i + 1 ] )
        {
            ft -= count[ i ];
            dt.Month = i + 1;
            break;
        }
    }

    // Day
    dt.Day = (ushort) ft + 1;

    // Day of week
    int a = ( 14 - dt.Month ) / 12;
    int y = dt.Year - a;
    int m = dt.Month + 12 * a - 2;
    dt.DayOfWeek = ( 7000 + ( dt.Day + y + y / 4 - y / 100 + y / 400 + ( 31 * m ) / 12 ) ) % 7;
}

int Timer::GetTimeDifference( const DateTime& dt1, const DateTime& dt2 )
{
    uint64 ft1 = 0, ft2 = 0;
    DateTimeToFullTime( dt1, ft1 );
    DateTimeToFullTime( dt2, ft2 );
    return (int) ( ( ft1 - ft2 ) / 10000000 );
}

void Timer::ContinueTime( DateTime& td, int seconds )
{
    uint64 ft;
    DateTimeToFullTime( td, ft );
    ft += (uint64) seconds * 10000000;
    FullTimeToDateTime( ft, td );
}

uint Timer::GetFullSecond( ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second )
{
    DateTime dt = { year, month, 0, day, hour, minute, second, 0 };
    uint64   ft = 0;
    Timer::DateTimeToFullTime( dt, ft );
    ft -= PACKUINT64( GameOpt.YearStartFTHi, GameOpt.YearStartFTLo );
    return (uint) ( ft / 10000000 );
}

DateTime Timer::GetGameTime( uint full_second )
{
    uint64   ft = PACKUINT64( GameOpt.YearStartFTHi, GameOpt.YearStartFTLo ) + uint64( full_second ) * 10000000;
    DateTime dt;
    Timer::FullTimeToDateTime( ft, dt );
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

#ifndef FO_WINDOWS
void SetTick()
{
    # ifdef FO_LINUX
    struct timespec tv;
    clock_gettime( CLOCK_MONOTONIC, &tv );
    uint            timer_tick = (uint) ( tv.tv_sec * 1000 + tv.tv_nsec / 1000000 );
    InterlockedExchange( &TimerTick, timer_tick );
    # else
    uint timer_tick = (uint) ( Timer::AccurateTick() - InitialAccurateTick );
    InterlockedExchange( &TimerTick, timer_tick );
    # endif
}

void UpdateTick( void* ) // Thread
{
    while( !QuitTick )
    {
        SetTick();
        Thread::Sleep( 1 );
    }
}
#endif
