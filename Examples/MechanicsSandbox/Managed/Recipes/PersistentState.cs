// Recipe: custom persistent character state, Managed C# shape of Scripts/Recipes/PersistentState.fos
// Parsed by Engine tests for declaration parity; not compiled by Engine CI
namespace MechanicsSandbox;

using FOnline;

///@ Property Critter Common int32 VisitCount Mutable Persistent OwnerSync
///@ Property Critter Server synctime FirstVisitTime Mutable Persistent

public static class PersistentState
{
#if SERVER
    [ModuleInit]
    internal static void InitPersistentState()
    {
        Game.OnCritterInit.Subscribe(OnCritterInit);
    }

    // firstTime is true only for a new critter; a restored critter keeps what the database returned
    [Event]
    internal static void OnCritterInit([RequiresCover] Critter cr, bool firstTime)
    {
        if (firstTime) {
            cr.FirstVisitTime = Game.SynchronizedTime;
        }
    }

    // The only writer of VisitCount: read the stored value, change the copy, assign it back
    public static int RecordVisit([RequiresCover] Critter cr)
    {
        int count = cr.VisitCount + 1;
        cr.VisitCount = count;
        return count;
    }
#endif
}
