// Recipe: an NPC patrol with awareness, Managed C# shape of Scripts/Recipes/NpcPatrol.fos
// Parsed by Engine tests for declaration parity; not compiled by Engine CI
namespace MechanicsSandbox;

using System.Threading.Tasks;
using FOnline;

///@ Property Critter Server mpos[] PatrolRoute Mutable Persistent
///@ Property Critter Server int32 PatrolIndex Mutable Persistent
///@ Property Critter Server int32 PatrolFailures Mutable
///@ Property Critter Server bool PatrolPaused Mutable

public static class NpcPatrol
{
    public static readonly hstring IntruderPid = "NpcPatrol_Intruder".hstr();
    public const int WalkSpeed = 500;
    public const int LegPauseMs = 200;
    public const int RetryMs = 500;
    public const int SightPauseMs = 400;
    public const int MaxFailures = 3;

#if SERVER
    [ModuleInit]
    internal static void InitNpcPatrol()
    {
        Game.OnCritterStopMoving.Subscribe(OnCritterStopMoving);
    }

    // The prototype names this method in its InitScript: it runs for a new patroller and again for every restored one
    public static void InitPatroller([RequiresCover] Critter cr, bool firstTime)
    {
        // Subscriptions and walking are runtime state; the saved route is not, so a restored patroller starts over from it
        cr.OnCritterAppeared.Subscribe(OnSeen);
        cr.OnCritterDisappeared.Subscribe(OnLost);

        if (cr.PatrolRoute.Count != 0) {
            ScheduleLeg(cr, LegPauseMs);
        }
    }

    // Gives a patroller its route and starts it; the first leg runs from a time event, not from the caller
    public static void StartPatrol([RequiresCover] Critter cr, List<mpos> route)
    {
        cr.PatrolRoute = route;
        cr.PatrolIndex = 0;
        cr.PatrolFailures = 0;
        cr.PatrolPaused = false;
        ScheduleLeg(cr, LegPauseMs);
    }

    // At most one waiting leg per critter; a running leg schedules its successor, so it does not count itself
    private static void ScheduleLeg([RequiresCover] Critter cr, int delayMs)
    {
        if (cr.CountTimeEvent(PatrolStep, true) == 0) {
            cr.StartTimeEvent(new timespan(delayMs, TimePlace.Milliseconds), PatrolStep);
        }
    }

    // The path-finder asks this about an item that gags a hex; the patrol walks through none
    private static bool NoGagPassage(Critter cr, Item gag)
    {
        return false;
    }

    // A critter time event covers only its critter; walking also changes the map, so the map joins the cover
    [TimeEvent]
    internal static async Task PatrolStep([RequiresCover] Critter cr)
    {
        Map? map = cr.GetMap();
        if (map == null || cr.PatrolPaused || cr.PatrolRoute.Count == 0) {
            return;
        }

        if (!await Sync.Lock(cr, map)) {
            return;
        }

        // Other work may run while the cover changes hands
        if (cr.IsDestroyed || cr.MapId != map.Id || cr.PatrolPaused) {
            return;
        }

        List<mpos> route = cr.PatrolRoute;
        MovingContext move = cr.MoveToHex(route[cr.PatrolIndex % route.Count], 0, WalkSpeed, NoGagPassage);

        // A leg that is walking ends in OnCritterStopMoving; a refused one never raised that event, so its result is read here
        if (!move.IsCompleted()) {
            cr.PatrolFailures = 0;
            return;
        }

        MovingState state = move.GetCompleteReason();
        if (state == MovingState.Success) {
            Advance(cr);
            ScheduleLeg(cr, LegPauseMs);
            return;
        }

        RetryOrSkip(cr, state);
    }

    // The engine reports every stop, including the ones this script causes; only a finished walk moves the patrol on
    [Event]
    internal static void OnCritterStopMoving([RequiresCover] Critter cr)
    {
        if (cr.PatrolRoute.Count == 0 || cr.PatrolPaused) {
            return;
        }

        MovingState state = cr.GetMovingState();
        if (state == MovingState.Success) {
            Advance(cr);
            ScheduleLeg(cr, LegPauseMs);
            return;
        }

        // A script stop is a decision of its caller; any other reason is a failed leg
        if (state != MovingState.Stopped) {
            RetryOrSkip(cr, state);
        }
    }

    private static void Advance([RequiresCover] Critter cr)
    {
        cr.PatrolFailures = 0;
        cr.PatrolIndex = cr.PatrolIndex + 1;
        Game.Log("npc_patrol_arrived critter=" + cr.Id + " index=" + cr.PatrolIndex + " hex=" + cr.Hex.x + "," + cr.Hex.y);
    }

    // A blocked waypoint is retried a few times and then skipped, so one stuck hex cannot hold the patrol forever
    private static void RetryOrSkip([RequiresCover] Critter cr, MovingState state)
    {
        cr.PatrolFailures = cr.PatrolFailures + 1;
        Game.Log("npc_patrol_leg_failed critter=" + cr.Id + " state=" + (int)state + " failures=" + cr.PatrolFailures);

        if (cr.PatrolFailures >= MaxFailures) {
            cr.PatrolFailures = 0;
            cr.PatrolIndex = cr.PatrolIndex + 1;
        }

        ScheduleLeg(cr, RetryMs);
    }

    // Runs on the watcher: a critter entered its view; the event owner is the first argument
    [Event]
    internal static void OnSeen([RequiresCover] Critter cr, [RequiresCover] Critter seen)
    {
        if (seen.ProtoId != IntruderPid) {
            return;
        }

        // The pause comes first, so the stop event that follows sees it and leaves the patrol alone
        cr.PatrolPaused = true;
        cr.StopMoving();
        cr.StopTimeEvent(PatrolStep);
        Game.Log("npc_patrol_saw critter=" + cr.Id + " seen=" + seen.Id);

        if (cr.CountTimeEvent(ResumePatrol) == 0) {
            cr.StartTimeEvent(new timespan(SightPauseMs, TimePlace.Milliseconds), ResumePatrol);
        }
    }

    [Event]
    internal static void OnLost([RequiresCover] Critter cr, [RequiresCover] Critter lost)
    {
        if (lost.ProtoId == IntruderPid) {
            Game.Log("npc_patrol_lost critter=" + cr.Id + " lost=" + lost.Id);
        }
    }

    [TimeEvent]
    internal static void ResumePatrol([RequiresCover] Critter cr)
    {
        cr.PatrolPaused = false;
        ScheduleLeg(cr, LegPauseMs);
    }
#endif
}
