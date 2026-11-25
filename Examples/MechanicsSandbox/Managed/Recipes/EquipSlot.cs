// Recipe: equip an item into a slot under one shared rule, Managed C# shape of Scripts/Recipes/EquipSlot.fos
// Parsed by Engine tests for declaration parity; not compiled by Engine CI
namespace MechanicsSandbox;

using FOnline;

///@ Enum CritterItemSlot Armor = 2

///@ Property Item Common CritterItemSlot EquipSlot

///@ RemoteCall Server EquipItem(ident itemId, CritterItemSlot slot) MaxBytes 9
///@ RemoteCall Client EquipResult(ident itemId, bool accepted) MaxBytes 9

public static class EquipSlot
{
    // The one rule, compiled for both sides: the UI predicts exactly what the server will decide
    public static bool CanEquip(Critter cr, Item item, CritterItemSlot slot)
    {
        if (item.CritterId != cr.Id) {
            return false;
        }

        // Putting an item back in the plain inventory is always allowed
        if (slot == CritterItemSlot.Inventory) {
            return true;
        }

        return item.EquipSlot == slot;
    }

#if SERVER
    [ModuleInit]
    internal static void InitEquipSlot()
    {
        Game.OnCritterItemMoved.Subscribe(OnCritterItemMoved);
    }

    // The only request path: the engine supplies the player, the critter and its inventory are derived from it
    [ServerRemoteCall]
    internal static void EquipItem([RequiresCover] Player player, ident itemId, CritterItemSlot slot)
    {
        Critter? cr = player.GetControlledCritter();
        if (cr == null) {
            Refuse(player, itemId, "no_actor");
            return;
        }

        // Only the actor's own inventory resolves; any other id is not an item the actor may move
        Item? item = cr.GetItem(itemId);
        if (item == null) {
            Refuse(player, itemId, "not_owned");
            return;
        }

        if (!CanEquip(cr, item, slot)) {
            Refuse(player, itemId, "slot_rule");
            return;
        }

        cr.ChangeItemSlot(itemId, slot);
        Game.Log("equip_slot_accepted item=" + itemId + " slot=" + (int)slot);
        player.ClientCall.EquipResult(itemId, true);
    }

    private static void Refuse([RequiresCover] Player player, ident itemId, string reason)
    {
        Game.Log("equip_slot_refused reason=" + reason + " player=" + player.Id + " item=" + itemId);
        player.ClientCall.EquipResult(itemId, false);
    }

    // The engine fires this after the move is committed, so it cannot veto: it repairs a move that bypassed the request path
    [Event]
    internal static void OnCritterItemMoved([RequiresCover] Critter cr, [RequiresCover] Item item, CritterItemSlot fromSlot)
    {
        // An item that left the inventory has no slot to audit
        if (item.CritterId != cr.Id || CanEquip(cr, item, item.CritterSlot)) {
            return;
        }

        Game.Log("equip_slot_repaired item=" + item.Id + " slot=" + (int)item.CritterSlot);
        cr.ChangeItemSlot(item.Id, CritterItemSlot.Inventory);
    }
#endif

#if CLIENT
    // The client asks only for what the shared rule allows; the server repeats the check
    public static void RequestEquip(Item item, CritterItemSlot slot)
    {
        if (!CanEquip(Game.Chosen, item, slot)) {
            Game.Log("equip_slot_client_refused item=" + item.Id);
            return;
        }

        Game.CurPlayer.ServerCall.EquipItem(item.Id, slot);
    }

    [ClientRemoteCall]
    internal static void EquipResult(ident itemId, bool accepted)
    {
        Game.Log("equip_slot_result item=" + itemId + " accepted=" + (accepted ? "true" : "false"));
    }
#endif
}
