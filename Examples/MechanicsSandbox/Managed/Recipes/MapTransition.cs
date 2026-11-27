// Recipe: a trigger that sends a critter to another map, Managed C# shape of Scripts/Recipes/MapTransition.fos
// Parsed by Engine tests for declaration parity; not compiled by Engine CI
namespace MechanicsSandbox;

using System.Threading.Tasks;
using FOnline;

///@ Property Item Server hstring ExitMap
///@ Property Item Server mpos ExitHex
///@ Property Item Server bool ExitNeedsKey

public static class MapTransition
{
    public static readonly hstring KeyPid = "MapTransition_Key".hstr();

#if SERVER
    // The pad names this method in its TriggerScript; the engine calls it once for each entry and each exit of a hex
    [ItemTrigger]
    internal static void OnExitPad([RequiresCover] Critter cr, StaticItem pad, bool entered, mdir dir)
    {
        if (!entered) {
            return;
        }

        // The rule reads only the critter and the immutable pad data, so it needs no further cover
        if (pad.ExitNeedsKey && cr.GetItem(KeyPid) == null) {
            Game.Log("map_transition_refused reason=no_key critter=" + cr.Id);
            return;
        }

        // A trigger runs inside the walking step with only the walking map covered; the transfer needs more, so a time event does it
        mpos exitHex = pad.ExitHex;
        cr.StartTimeEvent(default(timespan), CompleteExit, new List<any> { new any(pad.ExitMap), new any(exitHex.x), new any(exitHex.y) });
    }

    // The deferred half: the critter is covered at entry, the maps and the destination location are not
    [TimeEvent]
    internal static async Task CompleteExit([RequiresCover] Critter cr, List<any> data)
    {
        hstring mapPid = (hstring)data[0];
        mpos exitHex = new mpos((int)data[1], (int)data[2]);

        Map? destination = Game.GetMap(mapPid);
        Map? source = cr.GetMap();
        if (destination == null || source == null) {
            Game.Log("map_transition_refused reason=no_map critter=" + cr.Id);
            return;
        }

        // A transfer changes both maps, so both maps and the critter are covered by one request
        if (!await Sync.Lock(cr, source, destination)) {
            return;
        }

        if (cr.IsDestroyed || destination.IsDestroyed) {
            Game.Log("map_transition_refused reason=gone critter=" + cr.Id);
            return;
        }

        // Entering a map also enters its location, which only a covered map can name; widening may hand the cover over
        if (!await Sync.Widen(destination.GetLocation())) {
            return;
        }

        // The world may have changed while the cover changed hands
        if (cr.IsDestroyed || destination.IsDestroyed || cr.MapId != source.Id) {
            Game.Log("map_transition_refused reason=moved critter=" + cr.Id);
            return;
        }

        // Another transfer of the same critter is running; it owns the critter until it finishes
        if (cr.MapTransfersLocked) {
            Game.Log("map_transition_refused reason=locked critter=" + cr.Id);
            return;
        }

        cr.TransferToMap(destination, exitHex);
        Game.Log("map_transition_done critter=" + cr.Id);
    }
#endif
}
