// Recipe: a timed effect that survives a restart, Managed C# shape of Scripts/Recipes/TimedEffect.fos
// Parsed by Engine tests for declaration parity; not compiled by Engine CI
namespace MechanicsSandbox;

using FOnline;

///@ Property Critter Common synctime BoostEndTime Mutable Persistent PublicSync

public static class TimedEffect
{
#if SERVER
    [ModuleInit]
    internal static void InitTimedEffect()
    {
        Game.OnCritterInit.Subscribe(OnCritterInit);
    }

    // Starts the boost or moves its deadline; an armed timer reads the new deadline when it fires
    public static void ApplyBoost([RequiresCover] Critter cr, timespan duration)
    {
        cr.BoostEndTime = Game.SynchronizedTime + duration;
        ArmBoostTimer(cr);
    }

    public static bool IsBoosted([RequiresCover] Critter cr)
    {
        return cr.BoostEndTime != default(synctime);
    }

    // Time events are not restored from the database: a restored critter re-arms from its saved deadline
    [Event]
    internal static void OnCritterInit([RequiresCover] Critter cr, bool firstTime)
    {
        if (!firstTime && IsBoosted(cr)) {
            ArmBoostTimer(cr);
        }
    }

    // At most one timer per critter, whichever path asks first; the handler is passed by method group,
    // so counting and starting name the same callback
    private static void ArmBoostTimer([RequiresCover] Critter cr)
    {
        if (cr.CountTimeEvent(OnBoostTimer) != 0) {
            return;
        }

        cr.StartTimeEvent(RemainingTime(cr), OnBoostTimer);
    }

    private static timespan RemainingTime([RequiresCover] Critter cr)
    {
        timespan remaining = cr.BoostEndTime - Game.SynchronizedTime;
        return remaining > default(timespan) ? remaining : default(timespan);
    }

    [TimeEvent]
    internal static void OnBoostTimer([RequiresCover] Critter cr, TimeEventContext timer)
    {
        // A server snapshot restores its saved timers after the init re-arm, so a second timer can arrive here
        if (!IsBoosted(cr)) {
            timer.Stop();
            return;
        }

        // A deadline moved later keeps the same timer waiting for the rest of the time
        timespan remaining = RemainingTime(cr);
        if (remaining > default(timespan)) {
            timer.Repeat(remaining);
            return;
        }

        // Repeat made the event periodic, so the last firing stops it explicitly
        timer.Stop();
        cr.BoostEndTime = default(synctime);
        Game.Log("timed_effect_boost_expired critter=" + cr.Id);
    }
#endif
}
