namespace FOnline;

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

// Owns the native references of one entry assembly without retaining its managed wrappers
internal sealed class EntityReferenceRegistry
{
    private readonly object Gate = new();
    private readonly Dictionary<IntPtr, int> References = new();
    private bool Closed;
    private bool FinalizerReleasesStopped;
    private int ActiveDescriptions;

    internal void Acquire(IntPtr pointer)
    {
        if (pointer == IntPtr.Zero) {
            throw new ArgumentException("Entity reference pointer must not be null", nameof(pointer));
        }

        lock (Gate)
        {
            ObjectDisposedException.ThrowIf(Closed || !Native.IsBackendAlive, typeof(EntityReferenceRegistry));
            bool present = References.TryGetValue(pointer, out int count);
            int nextCount = checked(count + 1);

            if (!present) {
                References.Add(pointer, 0);
            }

            try {
                Native.AddRefEntity(pointer);
            }
            catch {
                if (!present) {
                    References.Remove(pointer);
                }

                throw;
            }

            References[pointer] = nextCount;
        }
    }

    internal void Release(IntPtr pointer)
    {
        lock (Gate)
        {
            if (Closed || FinalizerReleasesStopped || !Native.IsBackendAlive) {
                return;
            }

            if (!References.TryGetValue(pointer, out int count) || count == 0) {
                throw new InvalidOperationException("Entity reference is not owned by this backend");
            }

            Native.ReleaseEntity(pointer);

            if (count == 1) {
                References.Remove(pointer);
            }
            else {
                References[pointer] = count - 1;
            }
        }
    }

    internal string? Describe(IntPtr pointer)
    {
        lock (Gate)
        {
            if (Closed) {
                return null;
            }

            ActiveDescriptions = checked(ActiveDescriptions + 1);
        }

        try {
            return Native.GetEntityName(pointer) + " id=" + Native.GetEntityId(pointer);
        }
        finally {
            lock (Gate)
            {
                ActiveDescriptions--;
                Monitor.PulseAll(Gate);
            }
        }
    }

    internal void StopFinalizerReleases()
    {
        // Wait for in-flight releases before the engine takes exclusive teardown locks.
        // Finish callbacks can still acquire references that Close will return
        lock (Gate)
        {
            FinalizerReleasesStopped = true;
        }
    }

    internal long Close()
    {
        lock (Gate)
        {
            Closed = true;

            while (ActiveDescriptions != 0) {
                Monitor.Wait(Gate);
            }

            long released = 0;
            List<IntPtr> pointers = new(References.Keys);

            foreach (IntPtr pointer in pointers) {
                while (References.TryGetValue(pointer, out int count)) {
                    Native.ReleaseEntity(pointer);

                    if (count == 1) {
                        References.Remove(pointer);
                    }
                    else {
                        References[pointer] = count - 1;
                    }

                    released++;
                }
            }

            // The entry assembly remains loaded after shutdown
            References.TrimExcess();

            return released;
        }
    }
}

// Tracks wrapper diagnostics separately from native references, which the backend can return at shutdown.
// Deep tracking holds only weak wrapper references so it does not change their lifetime
internal static class EntityWrapperTracker
{
    private static readonly EntityReferenceRegistry References = new();
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

    // Switched on by the engine, which reads ManagedScript.DeepTrackEntityWrappers once while it loads the assemblies
    [CallableByEngine]
    internal static void EnableDeepWrapperTracking()
    {
        DeepTrackingEnabled = true;
    }

    // Acquire the native reference before publishing the wrapper; roll back optional diagnostics on failure
    internal static long Register(Entity wrapper, IntPtr entityPtr)
    {
        long trackerId = 0;

        References.Acquire(entityPtr);
        try {
            if (DeepTrackingEnabled) {
                trackerId = Interlocked.Increment(ref LastTrackerId);
                Tracked[trackerId] = new TrackedWrapper(wrapper, entityPtr);
            }
        }
        catch {
            References.Release(entityPtr);
            throw;
        }

        Interlocked.Increment(ref LiveWrappers);
        return trackerId;
    }

    internal static void Unregister(IntPtr entityPtr, long trackerId)
    {
        References.Release(entityPtr);
        Interlocked.Decrement(ref LiveWrappers);

        if (trackerId != 0) {
            Tracked.TryRemove(trackerId, out TrackedWrapper? _);
        }
    }

    [CallableByEngine]
    internal static void StopFinalizerReleases()
    {
        References.StopFinalizerReleases();
    }

    [CallableByEngine]
    internal static long CloseAndReleaseReferences()
    {
        long released = References.Close();
        Tracked.Clear();
        return released;
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

            // Browser finalizers are main-thread jobs; native teardown pumps them between managed collection calls
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

            string? entityDescription = References.Describe(entry.Value.EntityPtr);
            GC.KeepAlive(wrapper);

            if (entityDescription == null) {
                continue;
            }

            string description = wrapper.GetType().Name + " (" + entityDescription + ")";
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

}
