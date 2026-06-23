namespace FOnline
{
    // Ported from Engine/Source/Scripting/AngelScript/CoreScripts/Time.fos (the engine Time:: core script).
    // Kept name-for-name so ported game modules' `Time.Foo(...)` calls resolve (the AngelScript `Time::Foo`).
    // timespan(value, place) is the unit-tagged constructor (see ValueTypeExtensions.cs); IsTimeoutActive uses
    // the synctime comparison operator added there.
    public static class Time
    {
        public const int NanosecondsPlace = 0;
        public const int MicrosecondsPlace = 1;
        public const int MillisecondsPlace = 2;
        public const int SecondsPlace = 3;

        public static timespan Asap() => new timespan(0, NanosecondsPlace);
        public static timespan Milliseconds(int value) => new timespan(value, MillisecondsPlace);
        public static timespan Seconds(int value) => new timespan(value, SecondsPlace);
        public static timespan Minutes(int value) => new timespan(value * 60, SecondsPlace);
        public static timespan Hours(int value) => new timespan(value * 3600, SecondsPlace);
        public static timespan Days(int value) => new timespan(value * 86400, SecondsPlace);

        public static bool IsTimeoutActive(synctime timeout) => timeout > Game.SynchronizedTime;

        public static timespan GameMinutes(int value) => new timespan(value * 60 / Settings.WorldTime_Multiplier, SecondsPlace);
        public static timespan GameHours(int value) => new timespan(value * 3600 / Settings.WorldTime_Multiplier, SecondsPlace);
        public static timespan GameDays(int value) => new timespan(value * 86400 / Settings.WorldTime_Multiplier, SecondsPlace);
    }
}
