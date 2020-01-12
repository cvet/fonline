#pragma once

#include "Common.h"

struct DateTimeStamp
{
    ushort Year; // 1601 .. 30827
    ushort Month; // 1 .. 12
    ushort DayOfWeek; // 0 .. 6
    ushort Day; // 1 .. 31
    ushort Hour; // 0 .. 23
    ushort Minute; // 0 .. 59
    ushort Second; // 0 .. 59
    ushort Milliseconds; // 0 .. 999
};

namespace Timer
{
    void Init();
    void UpdateTick();

    uint FastTick();
    double AccurateTick();

    uint GameTick();
    void SetGamePause(bool pause);
    bool IsGamePaused();

    void GetCurrentDateTime(DateTimeStamp& dt);
    void DateTimeToFullTime(const DateTimeStamp& dt, uint64& ft);
    void FullTimeToDateTime(uint64 ft, DateTimeStamp& dt);
    int GetTimeDifference(const DateTimeStamp& dt1, const DateTimeStamp& dt2);
    void ContinueTime(DateTimeStamp& dt, int seconds);

    // Game time
    void InitGameTime();
    uint GetFullSecond(ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second);
    DateTimeStamp GetGameTime(uint full_second);
    uint GameTimeMonthDay(ushort year, ushort month);
    bool ProcessGameTime();
}; // namespace Timer
