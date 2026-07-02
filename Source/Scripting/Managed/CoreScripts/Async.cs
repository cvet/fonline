using System.Threading.Tasks;

namespace FOnline
{
    public static partial class Game
    {
        // Coroutine suspension for [[Async]]-ported methods. Completes after `ms` via a one-shot
        // engine time event, which fires on the script-processing thread; with no SynchronizationContext
        // in the embedded managed runtime, the awaiting continuation resumes inline on that same thread,
        // mirroring AngelScript's global Yield(ms) (which suspends/resumes the script context).
        //
        // Available on every target (Game.StartTimeEvent exists on server/client/mapper). This matches
        // AngelScript's global Yield(ms): neither auto-cancels when an entity is destroyed during the wait —
        // the continuation resumes regardless. Touching a destroyed entity after the await throws a safe
        // "entity is destroyed" exception rather than dangling, because managed entity wrappers hold a native
        // refcount (AddRef in the wrapper ctor, Release in its finalizer) that keeps a retained entity
        // alive-but-destroyed.
        public static Task YieldAsync(int ms)
        {
            var completion = new TaskCompletionSource<bool>();
            StartTimeEvent(new timespan((long)ms * 1_000_000L), () => completion.TrySetResult(true));
            return completion.Task;
        }
    }
}
