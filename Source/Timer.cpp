#include "Common.h"
#include "Timer.h"
#include "Entity.h"

#ifdef FO_WINDOWS
# include <Mmsystem.h>
# if defined ( FO_MSVC )
#  pragma comment(lib,"Winmm.lib")
# endif
#else
# include <sys/time.h>
#endif

static double InitialAccurateTick = 0;
static uint   TimerTick = 0;
static uint   LastGameTick = 0;
static uint   SkipGameTick = 0;
static bool   GameTickPaused = false;

#ifdef FO_WINDOWS
static int64 QPCStartValue = 0;
static int64 QPCFrequency = 0;
#endif

void Timer::Init()
{
    #ifdef FO_WINDOWS
    QueryPerformanceCounter( (LARGE_INTEGER*) &QPCStartValue );
    QueryPerformanceFrequency( (LARGE_INTEGER*) &QPCFrequency );
    #endif

    InitialAccurateTick = AccurateTick();
    UpdateTick();
    LastGameTick = FastTick();
    SkipGameTick = LastGameTick;
    GameTickPaused = false;
}

void Timer::UpdateTick()
{
    TimerTick = (uint) AccurateTick();
}

uint Timer::FastTick()
{
    return TimerTick;
}

double Timer::AccurateTick()
{
    #if defined ( FO_WINDOWS )
    int64 qpc_value;
    QueryPerformanceCounter( (LARGE_INTEGER*) &qpc_value );
    return (double) ( (double) ( qpc_value - QPCStartValue ) / (double) QPCFrequency * 1000.0 ) - InitialAccurateTick;
    #elif defined ( FO_WEB )
    return emscripten_get_now();
    #else
    struct timeval tv;
    gettimeofday( &tv, nullptr );
    return (double) ( tv.tv_sec * 1000000 + tv.tv_usec ) / 1000.0 - InitialAccurateTick;
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

void Timer::GetCurrentDateTime( DateTimeStamp& dt )
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
    gettimeofday( &tv, nullptr );
    dt.Milliseconds = tv.tv_usec / 1000;
    #endif
}

#ifdef FO_MSVC
# pragma optimize( "", off )
#endif
void Timer::DateTimeToFullTime( const DateTimeStamp& dt, uint64& ft )
{
    // Minor year
    ft = (uint64) ( dt.Year - 1601 ) * 365ULL * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;

    // Leap days
    uint leap_days = ( dt.Year - 1601 ) / 4;
    leap_days += ( dt.Year - 1601 ) / 400;
    leap_days -= ( dt.Year - 1601 ) / 100;

    // Current month
    static const uint count1[ 12 ] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    static const uint count2[ 12 ] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }; // Leap
    if( dt.Year % 400 == 0 || ( dt.Year % 4 == 0 && dt.Year % 100 != 0 ) )
        ft += (uint64) ( count2[ dt.Month - 1 ] ) * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;
    else
        ft += (uint64) ( count1[ dt.Month - 1 ] ) * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;

    // Other calculations
    ft += (uint64) ( dt.Day - 1 + leap_days ) * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;
    ft += (uint64) dt.Hour * 60ULL * 60ULL * 1000ULL * 1000ULL;
    ft += (uint64) dt.Minute * 60ULL * 1000ULL * 1000ULL;
    ft += (uint64) dt.Second * 1000ULL * 1000ULL;
    ft += (uint64) dt.Milliseconds * 1000ULL;
    ft *= (uint64) 10ULL;
}

void Timer::FullTimeToDateTime( uint64 ft, DateTimeStamp& dt )
{
    // Base
    ft /= 10000ULL;
    dt.Milliseconds = (ushort) ( ft % 1000ULL );
    ft /= 1000ULL;
    dt.Second = (ushort) ( ft % 60ULL );
    ft /= 60ULL;
    dt.Minute = (ushort) ( ft % 60ULL );
    ft /= 60ULL;
    dt.Hour = (ushort) ( ft % 24ULL );
    ft /= 24ULL;

    // Year
    int year = (int) ( ft / 365ULL );
    int days = (int) ( ft % 365ULL );
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
#ifdef FO_MSVC
# pragma optimize( "", on )
#endif

int Timer::GetTimeDifference( const DateTimeStamp& dt1, const DateTimeStamp& dt2 )
{
    uint64 ft1 = 0, ft2 = 0;
    DateTimeToFullTime( dt1, ft1 );
    DateTimeToFullTime( dt2, ft2 );
    return (int) ( ( ft1 - ft2 ) / 10000000ULL );
}

void Timer::ContinueTime( DateTimeStamp& td, int seconds )
{
    uint64 ft;
    DateTimeToFullTime( td, ft );
    ft += (uint64) seconds * 10000000ULL;
    FullTimeToDateTime( ft, td );
}

void Timer::InitGameTime()
{
    DateTimeStamp dt = { Globals->GetYearStart(), 1, 0, 1, 0, 0, 0, 0 };
    uint64        start_ft;
    DateTimeToFullTime( dt, start_ft );
    GameOpt.YearStartFTHi = ( start_ft >> 32 ) & 0xFFFFFFFF;
    GameOpt.YearStartFTLo = start_ft & 0xFFFFFFFF;

    GameOpt.FullSecond = GetFullSecond( Globals->GetYear(), Globals->GetMonth(), Globals->GetDay(),
                                        Globals->GetHour(), Globals->GetMinute(), Globals->GetSecond() );
    GameOpt.FullSecondStart = GameOpt.FullSecond;
    GameOpt.GameTimeTick = Timer::GameTick();
}

uint Timer::GetFullSecond( ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second )
{
    DateTimeStamp dt = { year, month, 0, day, hour, minute, second, 0 };
    uint64        ft = 0;
    Timer::DateTimeToFullTime( dt, ft );
    ft -= PACKUINT64( GameOpt.YearStartFTHi, GameOpt.YearStartFTLo );
    return (uint) ( ft / 10000000ULL );
}

DateTimeStamp Timer::GetGameTime( uint full_second )
{
    uint64        ft = PACKUINT64( GameOpt.YearStartFTHi, GameOpt.YearStartFTLo ) + uint64( full_second ) * 10000000ULL;
    DateTimeStamp dt;
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

bool Timer::ProcessGameTime()
{
    uint tick = Timer::GameTick();
    uint dt = tick - GameOpt.GameTimeTick;
    uint delta_second = dt / 1000 * Globals->GetTimeMultiplier() + dt % 1000 * Globals->GetTimeMultiplier() / 1000;
    uint fs = GameOpt.FullSecondStart + delta_second;
    if( GameOpt.FullSecond != fs )
    {
        GameOpt.FullSecond = fs;
        DateTimeStamp st = GetGameTime( GameOpt.FullSecond );
        Globals->SetYear( st.Year );
        Globals->SetMonth( st.Month );
        Globals->SetDay( st.Day );
        Globals->SetHour( st.Hour );
        Globals->SetMinute( st.Minute );
        Globals->SetSecond( st.Second );
        return true;
    }
    return false;
}
