#nullable enable

namespace FOnline
{
    // Resolution of the entity an item currently lives in. This is pure entity-model mechanics -- a switch over
    // the engine's own ItemOwnership plus the matching engine id property -- so it belongs beside the entity
    // types rather than in a game module. Sync's holder-covering helpers need it, and the engine cannot depend
    // on a project class.
    public static class ItemHolderExtensions
    {
#if SERVER

        // Null-tolerant: ItemOwnership.Nowhere, or an id whose entity is already gone, is a normal "no holder"
        // answer for callers that legitimately race holder teardown (deferred destroy/explode time events).
        public static Entity? FindHolder(this Item item)
        {
            switch (item.Ownership) {
            case ItemOwnership.CritterInventory:
                return Game.GetCritter(item.CritterId);
            case ItemOwnership.MapHex:
                return Game.GetMap(item.MapId);
            case ItemOwnership.ItemContainer:
                return Game.GetItem(item.ContainerId);
            default:
                break;
            }

            return null;
        }

        // For paths where a missing holder is an invariant violation rather than a race.
        public static Entity GetHolder(this Item item)
        {
            Entity? holder = item.FindHolder();

            Game.Verify(holder != null, "Item has no holder");
            return holder;
        }

#endif
    }
}
