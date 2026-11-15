// Recipe: put and take between a critter and a container item, Managed C# shape of Scripts/Recipes/ContainerTransfer.fos
// Parsed by Engine tests for declaration parity; not compiled by Engine CI
namespace MechanicsSandbox;

using System.Threading.Tasks;
using FOnline;

///@ Property Item Common int32 ContainerCapacity
///@ Property Item Common int32 StorageSection

///@ RemoteCall Server TransferItem(ident containerId, ident itemId, bool put) MaxBytes 17
///@ RemoteCall Server OpenContainer(ident containerId) MaxBytes 8
///@ RemoteCall Client ContainerResult(ident containerId, bool accepted) MaxBytes 9

public static class ContainerTransfer
{
#if SERVER
    public const int ReachDistance = 1;

    [ServerRemoteCall]
    internal static async Task TransferItem([RequiresCover] Player player, ident containerId, ident itemId, bool put)
    {
        // The engine supplies the player; the actor and its map are derived, and one cover holds both holders
        Critter? actor = player.GetControlledCritter();
        Map? map = actor?.GetMap();
        if (actor == null || map == null) {
            Refuse(player, containerId, "no_actor");
            return;
        }

        if (!await Sync.Lock(player, map)) {
            return;
        }

        actor = player.GetControlledCritter();
        if (actor == null || actor.MapId != map.Id) {
            Refuse(player, containerId, "actor_moved");
            return;
        }

        string reason = TryTransfer(actor, map, containerId, itemId, put);
        if (reason.Length != 0) {
            Refuse(player, containerId, reason);
            return;
        }

        player.ClientCall.ContainerResult(containerId, true);
    }

    // A fresh snapshot of the contents; the client never builds its view from its own guesses
    [ServerRemoteCall]
    internal static async Task OpenContainer([RequiresCover] Player player, ident containerId)
    {
        Critter? actor = player.GetControlledCritter();
        Map? map = actor?.GetMap();
        if (actor == null || map == null) {
            Refuse(player, containerId, "no_actor");
            return;
        }

        if (!await Sync.Lock(player, map)) {
            return;
        }

        actor = player.GetControlledCritter();
        if (actor == null || actor.MapId != map.Id) {
            Refuse(player, containerId, "actor_moved");
            return;
        }

        Item? container = FindReachableContainer(actor, map, containerId);
        if (container == null) {
            Refuse(player, containerId, "unknown_container");
            return;
        }

        // The context value travels with the batch so the client knows which container it describes
        actor.SendItems(container.GetItems(), false, false, new any(containerId.value));
    }

    // The rules for one transfer under the cover of the actor and its map; returns the refusal reason or an empty string
    public static string TryTransfer([RequiresCover] Critter actor, [RequiresCover] Map map, ident containerId, ident itemId, bool put)
    {
        Item? container = FindReachableContainer(actor, map, containerId);
        if (container == null) {
            return "unknown_container";
        }

        return put ? TryPut(actor, container, itemId) : TryTake(actor, container, itemId);
    }

    // A chest stands on the map and must be near; a bag in the actor's own inventory is always in reach
    private static Item? FindReachableContainer([RequiresCover] Critter actor, [RequiresCover] Map map, ident containerId)
    {
        Item? container = actor.GetItem(containerId) ?? map.GetItem(containerId);
        if (container == null || container.ContainerCapacity <= 0) {
            return null;
        }

        if (container.Ownership == ItemOwnership.MapHex && Game.GetDistance(actor, container) > ReachDistance) {
            return null;
        }

        return container;
    }

    private static string TryPut([RequiresCover] Critter actor, [RequiresCover] Item container, ident itemId)
    {
        // Only the actor's own inventory resolves, so a foreign or invented id is not an item to move
        Item? item = actor.GetItem(itemId);
        if (item == null) {
            return "not_carried";
        }

        // Policy: containers do not nest, which also keeps a container out of its own subtree
        if (item.Id == container.Id || item.ContainerCapacity > 0) {
            return "no_nesting";
        }

        if (container.GetItems().Count >= container.ContainerCapacity) {
            return "full";
        }

        // The section is the grouping key; the engine merges nothing, so equal sections stay separate items
        Item? moved = Game.MoveItem(item, container, new any(item.StorageSection));
        return moved != null ? "" : "move_failed";
    }

    private static string TryTake([RequiresCover] Critter actor, [RequiresCover] Item container, ident itemId)
    {
        // The item is found through the covered container; a global id lookup would hand back an uncovered entity
        foreach (Item content in container.GetItems()) {
            if (content.Id == itemId) {
                Item? moved = Game.MoveItem(content, actor);
                return moved != null ? "" : "move_failed";
            }
        }

        return "not_inside";
    }

    private static void Refuse([RequiresCover] Player player, ident containerId, string reason)
    {
        Game.Log("container_transfer_refused reason=" + reason + " player=" + player.Id + " container=" + containerId);
        player.ClientCall.ContainerResult(containerId, false);
    }
#endif

#if CLIENT
    [ModuleInit]
    internal static void InitContainerTransferClient()
    {
        Game.OnReceiveItems.Subscribe(OnReceiveItems);
    }

    // Received items are detached views: read them now, they are not part of the client's inventory
    [Event]
    internal static void OnReceiveItems(List<Item> items, any contextParam)
    {
        long containerId = contextParam;
        Game.Log("container_snapshot container=" + containerId + " count=" + items.Count);
    }

    [ClientRemoteCall]
    internal static void ContainerResult(ident containerId, bool accepted)
    {
        Game.Log("container_transfer_result container=" + containerId + " accepted=" + (accepted ? "true" : "false"));
    }
#endif
}
