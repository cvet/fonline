// Cross-cutting recipe: one action under one cover, Managed C# shape of Scripts/Recipes/CoverTransaction.fos
// Parsed by Engine tests for declaration parity; not compiled by Engine CI
namespace MechanicsSandbox;

using System.Threading.Tasks;
using FOnline;

public static class CoverTransaction
{
#if SERVER
    public const int HandOverDistance = 1;
    public const int HandOverDelayMs = 300;

    // Ids, not handles, cross the suspension: every step resolves and covers its entities again
    public static async Task<bool> HandOverItem(ident giverId, ident receiverId, ident itemId)
    {
        if (!await PrepareHandOver(giverId, receiverId, itemId)) {
            Game.Log("cover_transaction_refused step=prepare");
            return false;
        }

        // The hand-over takes time; no cover and no decision survive this await
        await ScriptTask.Delay(HandOverDelayMs);

        return await CommitHandOver(giverId, receiverId, itemId);
    }

    private static async Task<bool> PrepareHandOver(ident giverId, ident receiverId, ident itemId)
    {
        Critter? giver = Game.GetCritter(giverId);
        if (giver == null || !await Sync.Lock(giver)) {
            return false;
        }

        // Start from the acting critter, then widen the cover to the partner it names
        Critter? receiver = Game.GetCritter(receiverId);
        if (receiver == null || !await Sync.Widen(receiver)) {
            return false;
        }

        // Widening can hand the held locks over before it returns, so the checks follow it
        return CanHandOver(giver, receiver) && giver.GetItem(itemId) != null;
    }

    private static async Task<bool> CommitHandOver(ident giverId, ident receiverId, ident itemId)
    {
        // A later observation: resolve both parties again and cover them with one request
        Critter? giver = Game.GetCritter(giverId);
        Critter? receiver = Game.GetCritter(receiverId);
        if (giver == null || receiver == null || !await Sync.Lock(giver, receiver)) {
            Game.Log("cover_transaction_refused step=commit reason=party_gone");
            return false;
        }

        if (!CanHandOver(giver, receiver)) {
            Game.Log("cover_transaction_refused step=commit reason=parties_changed");
            return false;
        }

        Item? item = giver.GetItem(itemId);
        if (item == null) {
            Game.Log("cover_transaction_refused step=commit reason=item_gone");
            return false;
        }

        // All checks ran under the same cover as the move, so nothing can change in between
        Item? moved = Game.MoveItem(item, receiver);
        return moved != null;
    }

    // The rules that both parties must satisfy, read from the currently covered state
    private static bool CanHandOver([RequiresCover] Critter giver, [RequiresCover] Critter receiver)
    {
        if (giver.MapId == default(ident) || giver.MapId != receiver.MapId) {
            return false;
        }

        return Game.GetDistance(giver, receiver) <= HandOverDistance;
    }
#endif
}
