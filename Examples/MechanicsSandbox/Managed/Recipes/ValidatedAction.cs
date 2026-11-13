// Recipe: a validated action request, Managed C# shape of Scripts/Recipes/ValidatedAction.fos
// Parsed by Engine tests for declaration parity; not compiled by Engine CI
namespace MechanicsSandbox;

using System.Threading.Tasks;
using FOnline;

///@ Property Critter Common int32 ActionEnergy Mutable Persistent OwnerSync
///@ Property Item Server ident DeviceOwnerId Mutable Persistent
///@ Property Item Common int32 DeviceActivations Mutable Persistent PublicSync

///@ RemoteCall Server ActivateDevice(ident deviceId) MaxBytes 8
///@ RemoteCall Client DeviceActivationResult(ident deviceId, bool accepted) MaxBytes 9

public static class ValidatedAction
{
#if SERVER
    public static readonly hstring DevicePid = "ValidatedAction_Device".hstr();
    public const int ActivationDistance = 2;
    public const int ActivationCost = 1;

    [ServerRemoteCall]
    internal static async Task ActivateDevice([RequiresCover] Player player, ident deviceId)
    {
        // The engine supplies the calling player; nothing in the payload names the actor
        Critter? actor = player.GetControlledCritter();
        if (actor == null) {
            Refuse(player, deviceId, "no_actor");
            return;
        }

        Map? map = actor.GetMap();
        if (map == null) {
            Refuse(player, deviceId, "actor_not_on_map");
            return;
        }

        // The entry cover holds the player and its critter; map items need the map, and Lock replaces the set
        if (!await Sync.Lock(player, map)) {
            return;
        }

        // Other work may run while the cover changes hands, so the actor and its map are derived again
        actor = player.GetControlledCritter();
        if (actor == null || actor.MapId != map.Id) {
            Refuse(player, deviceId, "actor_moved");
            return;
        }

        // Only items on the actor's own map resolve; an id from anywhere else is not a target
        Item? device = map.GetItem(deviceId);
        if (device == null || device.ProtoId != DevicePid) {
            Refuse(player, deviceId, "unknown_target");
            return;
        }

        if (device.DeviceOwnerId != actor.Id) {
            Refuse(player, deviceId, "not_owner");
            return;
        }

        if (Game.GetDistance(actor, device) > ActivationDistance) {
            Refuse(player, deviceId, "too_far");
            return;
        }

        if (actor.ActionEnergy < ActivationCost) {
            Refuse(player, deviceId, "no_energy");
            return;
        }

        // Every check that can refuse has passed: charge the cost, then apply the effect
        actor.ActionEnergy = actor.ActionEnergy - ActivationCost;
        device.DeviceActivations = device.DeviceActivations + 1;

        Game.Log("validated_action_accepted device=" + deviceId);
        player.ClientCall.DeviceActivationResult(deviceId, true);
    }

    // Logs the caller and the claimed id for diagnosis; the reply tells the client only that it failed
    private static void Refuse([RequiresCover] Player player, ident deviceId, string reason)
    {
        Game.Log("validated_action_refused reason=" + reason + " player=" + player.Id + " device=" + deviceId);
        player.ClientCall.DeviceActivationResult(deviceId, false);
    }
#endif

#if CLIENT
    [ClientRemoteCall]
    internal static void DeviceActivationResult(ident deviceId, bool accepted)
    {
        Game.Log("validated_action_result device=" + deviceId + " accepted=" + (accepted ? "true" : "false"));
    }
#endif
}
