// Recipe: item behavior selected by data, Managed C# shape of Scripts/Recipes/DataDrivenItem.fos
// Parsed by Engine tests for declaration parity; not compiled by Engine CI
namespace MechanicsSandbox;

using FOnline;

///@ Property Item Common bool Remedy Component
///@ Property Item Common int32 Remedy.Restore
///@ Property Item Common int32 Remedy.Charges
///@ Property Item Common int32 Remedy.ChargesLeft Mutable Persistent PublicSync
///@ Property Critter Common int32 Vigor Mutable Persistent OwnerSync
///@ Setting Server int32 DataDrivenItem.MaxVigor
///@ MigrationRule Property Item Rename RemedyRestore Remedy.Restore

public static class DataDrivenItem
{
#if SERVER
    // The prototype names this method in its InitScript: it runs for each created item and for each restored one
    public static void InitRemedy([RequiresCover] Item item, bool firstTime)
    {
        // Creation seeds the instance state once; a restored item already carries what the last run left in it
        if (firstTime && item.HasRemedy) {
            item.Remedy.ChargesLeft = item.Remedy.Charges;
        }
    }

    // Uses an inventory item and returns the vigor it restored; zero means nothing happened
    public static int UseItem([RequiresCover] Critter cr, [RequiresCover] Item item)
    {
        // The kind comes from the component marker, never from the prototype id or from a nonempty field
        if (item.CritterId != cr.Id || !item.HasRemedy) {
            return 0;
        }

        // A marker without authored values is a content mistake; the default zero must not read as a valid effect
        if (item.Remedy.Restore <= 0 || item.Remedy.ChargesLeft <= 0) {
            return 0;
        }

        int room = Settings.DataDrivenItem.MaxVigor - cr.Vigor;
        int restored = item.Remedy.Restore < room ? item.Remedy.Restore : room;
        if (restored <= 0) {
            return 0;
        }

        cr.Vigor = cr.Vigor + restored;
        item.Remedy.ChargesLeft = item.Remedy.ChargesLeft - 1;

        // A spent item leaves the world with its last charge
        if (item.Remedy.ChargesLeft == 0) {
            Game.DestroyItem(item);
        }

        return restored;
    }

    // Content check for one prototype: a kind marker needs values that make the kind usable
    public static bool IsValidRemedyProto(hstring protoId)
    {
        if (!Game.CheckProtoItem(protoId)) {
            return false;
        }

        ProtoItem proto = Game.GetProtoItem(protoId);
        return proto.HasRemedy && proto.Remedy.Restore > 0 && proto.Remedy.Charges > 0;
    }
#endif
}
