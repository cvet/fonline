namespace FOnline;

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

// How many entity wrappers are alive, and - when asked for it - which ones. A wrapper gives its native entity
// reference back from its finalizer and from nowhere else, so a wrapper still alive when scripting shuts down is
// a reference nobody gave back.
//
// Two depths, because the two answers cost differently. The live counter is always on: one interlocked increment
// in the wrapper constructor and one decrement in its finalizer, which is what makes "how many are left" an
// answer every shutdown has. Deep tracking adds the weak table that can NAME them, and costs a dictionary write
// per wrapper - a wrapper is built on every marshalling, so that is millions of writes per run - therefore it
// waits for ManagedScript.DeepTrackEntityWrappers to ask. A non-zero count is what tells an operator to turn it
// on.
//
// The reference to the wrapper is weak on purpose: a strong one would root exactly what the table measures,
// nothing would ever be finalized and the table would never empty
internal static class EntityWrapperTracker
{
    private static readonly ConcurrentDictionary<long, TrackedWrapper> Tracked =
        new ConcurrentDictionary<long, TrackedWrapper>();
    private static long LiveWrappers;
    private static long LastTrackerId;
    private static bool DeepTrackingEnabled;

    private sealed class TrackedWrapper
    {
        internal TrackedWrapper(Entity wrapper, IntPtr entityPtr)
        {
            Wrapper = new WeakReference<Entity>(wrapper);
            EntityPtr = entityPtr;
        }

        internal WeakReference<Entity> Wrapper { get; }
        internal IntPtr EntityPtr { get; }
    }

    // Whether this run can name what it counts: the static cleanup says more when it can
    internal static bool IsDeepTracking => DeepTrackingEnabled;

    // Switched on by the engine while it loads the assemblies, never asked for from a static constructor:
    // those run while the initializator walks every type, long before there is an active backend to ask
    [CallableByEngine]
    internal static void EnableDeepWrapperTracking()
    {
        DeepTrackingEnabled = true;
    }

    // Called from the generated wrapper constructor. The returned id is what the finalizer gives back, so a
    // wrapper never touches an entry that a later wrapper of the same entity put there; zero means the deep
    // table was not asked for and this wrapper is carried by the counter alone
    internal static long Register(Entity wrapper, IntPtr entityPtr)
    {
        Interlocked.Increment(ref LiveWrappers);

        if (!DeepTrackingEnabled) {
            return 0;
        }

        long trackerId = Interlocked.Increment(ref LastTrackerId);
        Tracked[trackerId] = new TrackedWrapper(wrapper, entityPtr);
        return trackerId;
    }

    internal static void Unregister(long trackerId)
    {
        Interlocked.Decrement(ref LiveWrappers);

        if (trackerId != 0) {
            Tracked.TryRemove(trackerId, out TrackedWrapper? _);
        }
    }

    // Collecting from managed code rather than through the embedding API: GC.WaitForPendingFinalizers is the
    // supported way to wait for the finalizer thread, and it is the runtime's own business how that is done.
    // Passes alternate because a finalized wrapper can drop the last reference to another one, and the loop
    // stops at zero or the pass limit. Waiting for the finalizer queue has its own deadline
    [CallableByEngine]
    internal static int CollectAndWaitForFinalizers(int passLimit)
    {
        const int waitBudgetMilliseconds = 5000;
        long deadline = Environment.TickCount64 + waitBudgetMilliseconds;

        for (int pass = 0; pass < passLimit; pass++) {
            GC.Collect();

            // The browser runtime has neither a thread pool nor a finalizer thread: queuing a pool task aborts it,
            // and finalizers run as main-thread jobs after this call returns, so one inline pass is all there is
            if (OperatingSystem.IsBrowser()) {
                GC.WaitForPendingFinalizers();
                break;
            }

            long remaining = deadline - Environment.TickCount64;

            // The runtime wait has no timeout; keep it off the engine teardown thread
            if (remaining <= 0 || !Task.Run(GC.WaitForPendingFinalizers).Wait((int)remaining)) {
                throw new TimeoutException("Managed finalizer queue did not drain within the shutdown budget");
            }

            if (Interlocked.Read(ref LiveWrappers) == 0) {
                break;
            }
        }

        return LiveWrapperCount();
    }

    // The count is the always-on half and comes first; below it, deep tracking separates the two states that
    // matter, because they need opposite answers: a reachable wrapper is held by something (a static, a closure,
    // a captured async state machine) and will never finalize itself, while an unreachable one is simply waiting
    // for the collector and needs no action but patience
    [CallableByEngine]
    internal static string DumpOutstandingWrappers()
    {
        int live = LiveWrapperCount();

        // With the counter alone, silence is the normal answer and the engine keeps its shutdown log clean; a
        // non-zero count is the whole reason for counting, and it names the setting that turns the hunt on
        if (!DeepTrackingEnabled) {
            return live == 0 ? string.Empty
                             : "outstanding entity wrappers: " + live +
                                   " (set ManagedScript.DeepTrackEntityWrappers to name them)";
        }

        StringBuilder report = new StringBuilder();
        report.Append("outstanding entity wrappers: ").Append(live);
        report.Append(", registered over the run: ").Append(Interlocked.Read(ref LastTrackerId));

        if (Tracked.IsEmpty) {
            return report.ToString();
        }

        Dictionary<string, int> reachable = new Dictionary<string, int>();
        int pending = 0;

        foreach (KeyValuePair<long, TrackedWrapper> entry in Tracked) {
            if (!entry.Value.Wrapper.TryGetTarget(out Entity? wrapper)) {
                pending++;
                continue;
            }

            string description = Describe(entry.Value.EntityPtr);
            GC.KeepAlive(wrapper);
            reachable.TryGetValue(description, out int count);
            reachable[description] = count + 1;
        }

        if (pending != 0) {
            report.Append(", waiting for the collector: ").Append(pending);
        }

        foreach (KeyValuePair<string, int> held in reachable) {
            report.Append("\n- still reachable: ").Append(held.Key);

            if (held.Value > 1) {
                report.Append(" x").Append(held.Value);
            }
        }

        return report.ToString();
    }

    // A count too large for the engine's int32 is not a count any more, it is a defect measuring itself, so the
    // report saturates instead of wrapping into a negative number
    private static int LiveWrapperCount()
    {
        long live = Interlocked.Read(ref LiveWrappers);
        return live > int.MaxValue ? int.MaxValue : (int)live;
    }

    // The entry holds a native reference for as long as it exists, so the entity behind the pointer is alive and
    // can name itself
    private static string Describe(IntPtr entityPtr)
    {
        if (entityPtr == IntPtr.Zero) {
            return "(null)";
        }

        return Native.GetEntityName(entityPtr) + " id=" + Native.GetEntityId(entityPtr);
    }
}
