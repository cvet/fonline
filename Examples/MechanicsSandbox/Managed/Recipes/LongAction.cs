// Recipe: an interruptible long action, Managed C# shape of Scripts/Recipes/LongAction.fos
// Parsed by Engine tests for declaration parity; not compiled by Engine CI
namespace MechanicsSandbox;

using System.Threading.Tasks;
using FOnline;

///@ Property Critter Server bool ActionActive Mutable
///@ Property Critter Server int32 ActionToken Mutable
///@ Property Critter Server ident ActionTarget Mutable
///@ Property Item Common int32 NodeCharges Mutable Persistent PublicSync
///@ Property Item Common int32 NodeWorkMs

///@ RemoteCall Server BeginGather(ident nodeId) MaxBytes 8
///@ RemoteCall Client GatherResult(ident nodeId, bool accepted) MaxBytes 9

public static class LongAction
{
    public static readonly hstring OrePid = "LongAction_Ore".hstr();
    public const int WorkDistance = 1;

#if SERVER
    [ModuleInit]
    internal static void InitLongAction()
    {
        // The interrupts: each one is an engine fact that makes the work pointless
        Game.OnCritterStartMoving.Subscribe(OnCritterStartMoving);
        Game.OnCritterTransfer.Subscribe(OnCritterTransfer);
        Game.OnPlayerLogout.Subscribe(OnPlayerLogout);

        // The engine has no death event; the condition property is the signal
        Game.AddPropertyDeferredSetter(CritterProperty.Condition, OnConditionChanged);
    }

    [ServerRemoteCall]
    internal static async Task BeginGather([RequiresCover] Player player, ident nodeId)
    {
        Critter? actor = player.GetControlledCritter();
        Map? map = actor?.GetMap();
        if (actor == null || map == null) {
            Refuse(player, nodeId, "no_actor");
            return;
        }

        // Map items need the map's cover, and the actor and its map are derived again once it is held
        if (!await Sync.Lock(player, map)) {
            return;
        }

        actor = player.GetControlledCritter();
        if (actor == null || actor.MapId != map.Id) {
            Refuse(player, nodeId, "actor_moved");
            return;
        }

        string reason = Start(actor, map, nodeId);
        if (reason.Length != 0) {
            Refuse(player, nodeId, reason);
            return;
        }

        player.ClientCall.GatherResult(nodeId, true);
    }

    private static void Refuse([RequiresCover] Player player, ident nodeId, string reason)
    {
        Game.Log("long_action_refused reason=" + reason + " player=" + player.Id + " node=" + nodeId);
        player.ClientCall.GatherResult(nodeId, false);
    }

    // Starts the one action a critter may have; returns the refusal reason or an empty string. The caller covers the critter and its map
    public static string Start([RequiresCover] Critter cr, [RequiresCover] Map map, ident nodeId)
    {
        if (cr.Condition != CritterCondition.Alive) {
            return "not_alive";
        }

        // One job per critter; a cancelled completion may still be running with a stale token, so only a waiting one counts
        if (cr.ActionActive || cr.CountTimeEvent(FinishAction, true) != 0) {
            return "busy";
        }

        // The node is resolved on the critter's own map; its work time is data on the item
        Item? node = map.GetItem(nodeId);
        if (node == null || node.NodeWorkMs <= 0 || node.NodeCharges <= 0) {
            return "no_node";
        }

        if (Game.GetDistance(cr, node) > WorkDistance) {
            return "too_far";
        }

        cr.ActionToken = cr.ActionToken + 1;
        cr.ActionActive = true;
        cr.ActionTarget = nodeId;

        // Only an id and a token travel in the timer: no handle to the critter's map or node survives the wait
        cr.StartTimeEvent(new timespan(node.NodeWorkMs, TimePlace.Milliseconds), FinishAction, new List<any> { new any(cr.ActionToken) });
        return "";
    }

    // The one way out for every interrupt; it is safe to call when nothing is running
    public static void Cancel([RequiresCover] Critter cr, string reason)
    {
        if (!cr.ActionActive) {
            return;
        }

        cr.ActionActive = false;

        // A completion that is already running cannot be stopped, so it also finds a token that no longer matches
        cr.ActionToken = cr.ActionToken + 1;
        cr.StopTimeEvent(FinishAction);
        Game.Log("long_action_cancelled critter=" + cr.Id + " reason=" + reason);
    }

    private static bool IsCurrent([RequiresCover] Critter cr, int token)
    {
        return cr.ActionActive && cr.ActionToken == token;
    }

    [TimeEvent]
    internal static async Task FinishAction([RequiresCover] Critter cr, List<any> data)
    {
        await Complete(cr, (int)data[0]);
    }

    // The completion step: it runs long after the decision to start, so it resolves and checks everything again
    public static async Task Complete([RequiresCover] Critter cr, int token)
    {
        if (!IsCurrent(cr, token)) {
            return;
        }

        Map? map = cr.GetMap();
        if (map == null) {
            Cancel(cr, "no_map");
            return;
        }

        // The work changes a map item, so the map joins the cover; getting it may let other work run first
        ident nodeId = cr.ActionTarget;
        if (!await Sync.Lock(cr, map)) {
            return;
        }

        if (cr.IsDestroyed || cr.MapId != map.Id || !IsCurrent(cr, token)) {
            return;
        }

        Item? node = map.GetItem(nodeId);
        if (node == null || node.NodeCharges <= 0) {
            Cancel(cr, "node_gone");
            return;
        }

        cr.ActionActive = false;
        node.NodeCharges = node.NodeCharges - 1;
        cr.AddItem(OrePid);
        Game.Log("long_action_done critter=" + cr.Id + " node=" + nodeId);
    }

    [Event]
    internal static void OnCritterStartMoving([RequiresCover] Critter cr, bool wasMoving)
    {
        Cancel(cr, "moved");
    }

    [Event]
    internal static void OnCritterTransfer([RequiresCover] Critter cr, Map? prevMap)
    {
        Cancel(cr, "transferred");
    }

    [Event]
    internal static void OnPlayerLogout([RequiresCover] Player player)
    {
        Critter? cr = player.GetControlledCritter();
        if (cr != null) {
            Cancel(cr, "disconnected");
        }
    }

    [PropertySetter]
    internal static void OnConditionChanged([RequiresCover] Critter cr)
    {
        if (cr.Condition != CritterCondition.Alive) {
            Cancel(cr, "incapacitated");
        }
    }
#endif

#if CLIENT
    [ClientRemoteCall]
    internal static void GatherResult(ident nodeId, bool accepted)
    {
        Game.Log("long_action_result node=" + nodeId + " accepted=" + (accepted ? "true" : "false"));
    }
#endif
}
