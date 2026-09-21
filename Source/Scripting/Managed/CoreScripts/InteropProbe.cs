namespace FOnline;

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading;

// Reusable interop benchmark. A Recorder times batches the caller writes inline, which is how the managed-to-native
// direction is covered with whatever surface the embedding project exposes; MeasureCallback lets native code drive
// one fixed adapter over each native-to-managed transport. Every figure is a batch mean: the series describes how
// those means spread across batches and is never the latency of a single call
public static class InteropProbe
{
    // Which part of the native-to-managed path the native loop exercises. The first three call the same adapter
    // body over a bare transport; ScriptEntry adds the script-entry bookkeeping of a real invoke, and FullDispatch
    // runs the production callback dispatcher end to end. The *Only modes time one piece of the dispatch
    // scaffolding alone and never reach managed code; DispatchInContext is FullDispatch without the script-context
    // entry around it
    public enum CallbackMode
    {
        RuntimeInvoke = 0,
        Thunk = 1,
        UnmanagedCallersOnly = 2,
        ScriptEntry = 3,
        FullDispatch = 4,
        SyncContextOnly = 5,
        EntryScopeOnly = 6,
        AttachmentOnly = 7,
        OverrunReportOnly = 8,
        DispatchInContext = 9,
    }

    // What one measured stretch cost besides time, as totals; Series divides them per call
    public readonly struct Counters
    {
        public long GcHandles { get; init; }
        public long MetadataLookups { get; init; }
        public long ManagedObjects { get; init; }
        public long Wrappers { get; init; }
        public long NativeAllocations { get; init; }
        public long NativeBytes { get; init; }
        public bool NativeAvailable { get; init; }

        public static Counters Read(bool enable)
        {
            bool nativeAvailable = Native.ReadInteropCounters(enable,
                                                              out long gcHandles,
                                                              out long metadataLookups,
                                                              out long managedObjects,
                                                              out long nativeAllocations,
                                                              out long nativeBytes);
            return new Counters {
                GcHandles = gcHandles,
                MetadataLookups = metadataLookups,
                ManagedObjects = managedObjects,
                Wrappers = Native.ReadWrapperCount(enable),
                NativeAllocations = nativeAllocations,
                NativeBytes = nativeBytes,
                NativeAvailable = nativeAvailable,
            };
        }

        public Counters Since(Counters start)
        {
            return new Counters {
                GcHandles = GcHandles - start.GcHandles,
                MetadataLookups = MetadataLookups - start.MetadataLookups,
                ManagedObjects = ManagedObjects - start.ManagedObjects,
                Wrappers = Wrappers - start.Wrappers,
                NativeAllocations = NativeAllocations - start.NativeAllocations,
                NativeBytes = NativeBytes - start.NativeBytes,
                NativeAvailable = NativeAvailable,
            };
        }
    }

    public readonly struct Series
    {
        public Series(string name, int iterations, double[] batchNs, double calibrationNs, double managedBytesPerCall,
                      Counters counters)
        {
            double[] sorted = (double[])batchNs.Clone();
            Array.Sort(sorted);
            double sum = 0;

            foreach (double value in sorted) {
                sum += value;
            }

            Name = name;
            Iterations = iterations;
            Batches = sorted.Length;
            MinNs = sorted[0];
            P50Ns = Rank(sorted, 0.50);
            P95Ns = Rank(sorted, 0.95);
            MaxNs = sorted[sorted.Length - 1];
            MeanNs = sum / sorted.Length;
            CalibrationNs = calibrationNs;
            ManagedBytesPerCall = managedBytesPerCall;
            double calls = (double)iterations * sorted.Length;
            GcHandlesPerCall = counters.GcHandles / calls;
            MetadataLookupsPerCall = counters.MetadataLookups / calls;
            ManagedObjectsPerCall = counters.ManagedObjects / calls;
            WrappersPerCall = counters.Wrappers / calls;
            NativeAllocationsPerCall = counters.NativeAvailable ? counters.NativeAllocations / calls : double.NaN;
            NativeBytesPerCall = counters.NativeAvailable ? counters.NativeBytes / calls : double.NaN;
        }

        public string Name { get; }
        public int Iterations { get; }
        public int Batches { get; }
        public double MinNs { get; }
        public double P50Ns { get; }
        public double P95Ns { get; }
        public double MaxNs { get; }
        public double MeanNs { get; }

        // Cost of the loop alone, already subtracted from every figure above
        public double CalibrationNs { get; }
        public double ManagedBytesPerCall { get; }

        // Bridge work per call: GC handles taken, class/method lookups by name, managed objects the native side created,
        // wrapper constructions, and native heap allocations (NaN outside profiling builds, which alone count them)
        public double GcHandlesPerCall { get; }
        public double MetadataLookupsPerCall { get; }
        public double ManagedObjectsPerCall { get; }
        public double WrappersPerCall { get; }
        public double NativeAllocationsPerCall { get; }
        public double NativeBytesPerCall { get; }

        // Spread of the batch means relative to the median: the noise floor any comparison has to clear
        public double NoisePercent => P50Ns > 0?(P95Ns - MinNs) / P50Ns * 100.0 : 0;

        public override string ToString()
        {
            string native = double.IsNaN(NativeAllocationsPerCall) ? "native n/a"
                                                                   : string.Format(CultureInfo.InvariantCulture,
                                                                                   "native {0:G3} allocs {1:G3} B",
                                                                                   NativeAllocationsPerCall,
                                                                                   NativeBytesPerCall);
            return string.Format(CultureInfo.InvariantCulture,
                                 "{0}: p50 {1:F1} ns, min {2:F1}, p95 {3:F1}, max {4:F1}, noise {5:F0}%, " +
                                     "{6:F2} managed bytes/call, {7}, {8:G3} gc handles, {9:G3} lookups, " +
                                     "{10:G3} objects, {11:G3} wrappers, {12} x {13}",
                                 Name,
                                 P50Ns,
                                 MinNs,
                                 P95Ns,
                                 MaxNs,
                                 NoisePercent,
                                 ManagedBytesPerCall,
                                 native,
                                 GcHandlesPerCall,
                                 MetadataLookupsPerCall,
                                 ManagedObjectsPerCall,
                                 WrappersPerCall,
                                 Batches,
                                 Iterations);
        }

        private static double Rank(double[] sorted, double quantile)
        {
            int index = (int)Math.Ceiling(quantile * sorted.Length) - 1;
            return sorted[Math.Clamp(index, 0, sorted.Length - 1)];
        }
    }

    // Times batches written inline by the caller, so the measured code keeps its own locals, cover and call shape:
    //     Recorder recorder = new Recorder("name", iterations, batches);
    //     while (recorder.NextBatch()) { recorder.Consume(Body(iterations)); }
    //     Series series = recorder.Finish(calibrationNs);
    // The first batches warm the JIT and the registrations and are dropped
    public sealed class Recorder
    {
        private readonly string SeriesName;
        private readonly int Iterations;
        private readonly double[] BatchNs;
        private int Batch = -WarmupBatches - 1;
        private long Started;
        private long AllocatedBefore;
        private long Allocated;
        private Counters CountersBefore;
        private Counters CountersTotal;

        public Recorder(string name, int iterations, int batches)
        {
            Invariant.Verify(iterations > 0 && batches > 0, "Probe needs a positive iteration and batch count");
            SeriesName = name;
            Iterations = iterations;
            BatchNs = new double[batches];
        }

        public bool NextBatch()
        {
            long now = Stopwatch.GetTimestamp();

            if (Batch >= 0) {
                BatchNs[Batch] = (now - Started) * 1_000_000_000.0 / Stopwatch.Frequency / Iterations;
            }

            Batch++;

            if (Batch == BatchNs.Length) {
                Allocated = GC.GetAllocatedBytesForCurrentThread() - AllocatedBefore;
                CountersTotal = Counters.Read(false).Since(CountersBefore);
                return false;
            }

            if (Batch == 0) {
                CountersBefore = Counters.Read(true);
                AllocatedBefore = GC.GetAllocatedBytesForCurrentThread();
            }

            Started = Stopwatch.GetTimestamp();
            return true;
        }

        // Keeps the measured results observable, so the loop cannot be optimized away
        public void Consume(long value)
        {
            Sink += value;
        }

        public Series Finish(double calibrationNs = 0)
        {
            Invariant.Verify(Batch == BatchNs.Length, "Probe recorder must run every batch before it finishes");

            for (int i = 0; i < BatchNs.Length; i++) {
                BatchNs[i] = Math.Max(0, BatchNs[i] - calibrationNs);
            }

            double managedBytesPerCall = Allocated / ((double)Iterations * BatchNs.Length);
            return new Series(SeriesName, Iterations, BatchNs, calibrationNs, managedBytesPerCall, CountersTotal);
        }
    }

    // One transport against one condition; the capability matrix is built from these
    public readonly struct TransportCheck
    {
        public TransportCheck(CallbackMode transport, string scenario, bool passed, string detail)
        {
            Transport = transport;
            Scenario = scenario;
            Passed = passed;
            Detail = detail;
        }

        public CallbackMode Transport { get; }
        public string Scenario { get; }
        public bool Passed { get; }
        public string Detail { get; }

        public override string ToString()
        {
            return Transport + " / " + Scenario + ": " + (Passed ? "ok" : "FAILED") + " (" + Detail + ")";
        }
    }

    private const int WarmupBatches = 3;
    private const int ProbeArgSum = 1 + 2 + 3 + 4;
    private const int ScenarioCalls = 64;
    private const int PlainRegistration = 1;
    private const int MixedRegistration = 2;
    private const long MixedWide = 0x1122334455667788;

    private static readonly Action<int, int, int, int> ProbeHandler = OnProbe;
    private static readonly Action<int, int, int, int> CollectHandler = OnProbeCollect;
    private static readonly Action<int, int, int, int> FaultHandler = OnProbeFault;
    private static readonly Action<int, int, int, int> ReenterHandler = OnProbeReenter;
    private static readonly Action<CallbackMode, bool, long, mpos, hstring> MixedHandler = OnProbeMixed;
    private static readonly InvalidOperationException ProbeFault =
        new InvalidOperationException("Deliberate probe fault");
    private static readonly Delegate?[] Registrations = new Delegate?[3];
    private static readonly hstring ProbeTag = "InteropProbeTag".hstr();
    private static long ProbeSum;
    private static long Sink;
    private static IntPtr UcoEntry;
    private static IntPtr UcoMixedEntry;
    private static string? UcoFault;
    private static int UcoFaultCount;
    private static int MixedMatches;
    private static CallbackMode ReentryTransport;
    private static long ReentryInnerSum;

    // Delegate form of the recorder for bodies that need no cover of their own; the calibration body is the same
    // loop without the crossing, and its median is subtracted
    public static Series Measure(string name, int iterations, int batches, Func<int, long> body,
                                 Func<int, long>? calibration = null)
    {
        double calibrationNs = 0;

        if (calibration != null) {
            Recorder calibrationRecorder = new Recorder(name, iterations, batches);

            while (calibrationRecorder.NextBatch()) {
                calibrationRecorder.Consume(calibration(iterations));
            }

            calibrationNs = calibrationRecorder.Finish().P50Ns;
        }

        Recorder recorder = new Recorder(name, iterations, batches);

        while (recorder.NextBatch()) {
            recorder.Consume(body(iterations));
        }

        return recorder.Finish(calibrationNs);
    }

    // Native code calls the probe adapter `iterations` times per batch and times the loop itself, so no managed
    // loop overhead enters the figure. Every batch is checked: the handler must have run exactly that many times
    public static Series MeasureCallback(CallbackMode mode, int iterations, int batches)
    {
        Invariant.Verify(iterations > 0 && batches > 0, "Probe needs a positive iteration and batch count");
        Registrations[1] = ProbeHandler;

        if (mode == CallbackMode.UnmanagedCallersOnly && UcoEntry == IntPtr.Zero) {
            UcoEntry = ResolveUcoEntry();
        }

        double[] batchNs = new double[batches];
        long allocatedBefore = 0;
        Counters countersBefore = default;

        for (int batch = -WarmupBatches; batch < batches; batch++) {
            if (batch == 0) {
                countersBefore = Counters.Read(true);
                allocatedBefore = GC.GetAllocatedBytesForCurrentThread();
            }

            ProbeSum = 0;
            long elapsedNs = Native.ProbeCallbackTransport(ProbeHandler, (int)mode, iterations, UcoEntry, 1);
            Invariant.Verify(UcoFault == null, "Probe adapter must not fault", UcoFault);
            bool scaffoldingOnly = mode >= CallbackMode.SyncContextOnly && mode <= CallbackMode.OverrunReportOnly;
            long expectedSum = scaffoldingOnly ? 0 : (long)iterations * ProbeArgSum;
            Invariant.Verify(ProbeSum == expectedSum,
                             "Probe handler must run once per native call with intact arguments",
                             ProbeSum,
                             iterations);

            if (batch >= 0) {
                batchNs[batch] = (double)elapsedNs / iterations;
            }
        }

        long allocated = GC.GetAllocatedBytesForCurrentThread() - allocatedBefore;
        Counters counters = Counters.Read(false).Since(countersBefore);
        return new Series("callback/" + mode,
                          iterations,
                          batchNs,
                          0,
                          allocated / ((double)iterations * batches),
                          counters);
    }

    // Runs every transport against every condition the engine relies on and returns one check per pair: plain and
    // mixed argument frames, a throwing handler and a clean call after it, a collection inside the handler, a nested
    // native-managed-native-managed entry, a native thread of its own, and instance and virtual delegate targets
    public static List<TransportCheck> VerifyTransports()
    {
        Registrations[PlainRegistration] = ProbeHandler;
        List<TransportCheck> checks = [];
        List<CallbackMode> transports = [CallbackMode.RuntimeInvoke];

        // A thunk and an unmanaged-callers-only entry are native code the runtime compiles; an interpreter-only
        // runtime (the browser) has none to hand out, and production calls in through the runtime invoke there too
        if (RuntimeFeature.IsDynamicCodeCompiled) {
            // Creating an unmanaged-callers-only entry compiles its wrapper, which no later call pays again
            if (UcoEntry == IntPtr.Zero) {
                long started = Stopwatch.GetTimestamp();
                UcoEntry = ResolveUcoEntry(nameof(AdaptProbeUco));
                UcoMixedEntry = ResolveUcoEntry(nameof(AdaptProbeMixedUco));
                double elapsedUs = (Stopwatch.GetTimestamp() - started) * 1_000_000.0 / Stopwatch.Frequency;
                checks.Add(
                    new TransportCheck(CallbackMode.UnmanagedCallersOnly,
                                       "entry creation",
                                       UcoEntry != IntPtr.Zero && UcoMixedEntry != IntPtr.Zero,
                                       elapsedUs.ToString("F1", CultureInfo.InvariantCulture) + " us for 2 entries"));
            }

            transports.Add(CallbackMode.Thunk);
            transports.Add(CallbackMode.UnmanagedCallersOnly);
        }

        // The browser runtime is single-threaded, so no native thread of its own can call into it
        bool nativeThreads = !OperatingSystem.IsBrowser();

        foreach (CallbackMode transport in transports) {
            checks.Add(CheckPlain(transport, "plain int32 x4", ProbeHandler, false));
            checks.Add(CheckMixed(transport));
            checks.Add(CheckFault(transport));
            checks.Add(CheckPlain(transport, "clean call after faults", ProbeHandler, false));
            checks.Add(CheckPlain(transport, "collection inside the handler", CollectHandler, false));
            checks.Add(CheckReentry(transport));

            if (nativeThreads) {
                checks.Add(CheckPlain(transport, "native thread of its own", ProbeHandler, true));
            }

            checks.Add(CheckPlain(transport, "instance target", new InstanceTarget().OnCall, false));
            VirtualTarget virtualTarget = new DerivedVirtualTarget();
            checks.Add(CheckPlain(transport, "virtual target", virtualTarget.OnCall, false));
        }

        return checks;
    }

    // Logs every check and a closing count, so a client that runs no test suite is qualified from its log alone
    public static void LogTransportChecks()
    {
        List<TransportCheck> checks = VerifyTransports();
        int failed = 0;

        foreach (TransportCheck check in checks) {
            Game.Log("INTEROP-TRANSPORT " + check);
            failed += check.Passed ? 0 : 1;
        }

        Game.Log("INTEROP-TRANSPORT summary: " + checks.Count + " checks, " + failed + " failed, pointer size " +
                 IntPtr.Size + ", compiled code " + RuntimeFeature.IsDynamicCodeCompiled);
    }

    // Same shape as a generated callback adapter, so every transport is compared over one adapter body
    [CallableByEngine]
    internal static void AdaptProbe(Delegate handler, ref byte frame, int frameSize)
    {
        Invariant.Verify(frameSize == 16, "Probe frame size must match the probe layout");
        int a0 = Unsafe.ReadUnaligned<int>(ref Unsafe.Add(ref frame, 0));
        int a1 = Unsafe.ReadUnaligned<int>(ref Unsafe.Add(ref frame, 4));
        int a2 = Unsafe.ReadUnaligned<int>(ref Unsafe.Add(ref frame, 8));
        int a3 = Unsafe.ReadUnaligned<int>(ref Unsafe.Add(ref frame, 12));

        if (handler is Action<int, int, int, int> action) {
            using ScriptSynchronizationContext context = ScriptSynchronizationContext.Enter(false);

            // A deliberate probe fault is counted by the caller and stays out of the script exception accounting
            try {
                action(a0, a1, a2, a3);
            }
            catch (Exception ex) when (ex != ProbeFault) {
                ScriptExceptions.Record(ex, false);
                throw;
            }

            return;
        }

        throw new InvalidOperationException("Probe adapter got a delegate of another shape");
    }

    // Mixed frame: enum at 0, bool at 4, int64 at 8, mpos at 16, hstring at 24, as a generated frame lays them out
    [CallableByEngine]
    internal static void AdaptProbeMixed(Delegate handler, ref byte frame, int frameSize)
    {
        Invariant.Verify(frameSize == 32, "Probe mixed frame size must match the probe layout");
        CallbackMode mode = Unsafe.ReadUnaligned<CallbackMode>(ref Unsafe.Add(ref frame, 0));
        bool flag = Unsafe.ReadUnaligned<bool>(ref Unsafe.Add(ref frame, 4));
        long wide = Unsafe.ReadUnaligned<long>(ref Unsafe.Add(ref frame, 8));
        mpos hex = Unsafe.ReadUnaligned<mpos>(ref Unsafe.Add(ref frame, 16));
        hstring tag = Unsafe.ReadUnaligned<hstring>(ref Unsafe.Add(ref frame, 24));

        if (handler is Action<CallbackMode, bool, long, mpos, hstring> action) {
            using ScriptSynchronizationContext context = ScriptSynchronizationContext.Enter(false);
            action(mode, flag, wide, hex, tag);
            return;
        }

        throw new InvalidOperationException("Probe adapter got a delegate of another shape");
    }

    // An unmanaged-callers-only entry takes no managed object: the delegate travels as a registration id, the frame
    // as its address, and a fault is parked for the caller instead of crossing the native boundary
    [UnmanagedCallersOnly]
    private static void AdaptProbeUco(int registrationId, IntPtr frame, int frameSize)
    {
        try {
            Delegate? handler = Registrations[registrationId];
            Invariant.Verify(handler != null, "Probe registration must be live");
            AdaptProbe(handler, ref Unsafe.AddByteOffset(ref Unsafe.NullRef<byte>(), frame), frameSize);
        }
        catch (Exception ex) {
            UcoFault = ex.ToString();
            UcoFaultCount++;
        }
    }

    [UnmanagedCallersOnly]
    private static void AdaptProbeMixedUco(int registrationId, IntPtr frame, int frameSize)
    {
        try {
            Delegate? handler = Registrations[registrationId];
            Invariant.Verify(handler != null, "Probe registration must be live");
            AdaptProbeMixed(handler, ref Unsafe.AddByteOffset(ref Unsafe.NullRef<byte>(), frame), frameSize);
        }
        catch (Exception ex) {
            UcoFault = ex.ToString();
            UcoFaultCount++;
        }
    }

    // RuntimeMethodHandle.GetFunctionPointer hands out the native-callable wrapper of an UnmanagedCallersOnly
    // method, which needs neither an unsafe context nor a private runtime export
    private static IntPtr ResolveUcoEntry(string methodName = nameof(AdaptProbeUco))
    {
        MethodInfo? method = typeof(InteropProbe).GetMethod(methodName, BindingFlags.Static | BindingFlags.NonPublic);
        Invariant.Verify(method != null, "Probe unmanaged entry must exist", methodName);
        return method.MethodHandle.GetFunctionPointer();
    }

    private static TransportCheck CheckPlain(CallbackMode transport, string scenario,
                                             Action<int, int, int, int> handler, bool externalThread)
    {
        Registrations[PlainRegistration] = handler;
        ProbeSum = 0;
        UcoFaultCount = 0;
        long started = Stopwatch.GetTimestamp();
        int faults = RunScenario(transport, 0, handler, ScenarioCalls, externalThread, PlainRegistration);
        double elapsedUs = (Stopwatch.GetTimestamp() - started) * 1_000_000.0 / Stopwatch.Frequency;
        Registrations[PlainRegistration] = ProbeHandler;
        long expected = (long)ScenarioCalls * ProbeArgSum;
        bool passed = faults == 0 && UcoFaultCount == 0 && ProbeSum == expected;

        // The first check of a transport also pays for creating its entry and compiling the adapter; the clean call
        // after the faults is the same scenario warm, so the two figures separate creation from steady state
        string timing = elapsedUs.ToString("F1", CultureInfo.InvariantCulture) + " us for " + ScenarioCalls + " calls";
        return new TransportCheck(transport,
                                  scenario,
                                  passed,
                                  ProbeSum + "/" + expected + " summed, " + faults + " faults, " + timing);
    }

    private static TransportCheck CheckMixed(CallbackMode transport)
    {
        Registrations[MixedRegistration] = MixedHandler;
        MixedMatches = 0;
        UcoFaultCount = 0;
        int faults = RunScenario(transport, 1, MixedHandler, ScenarioCalls, false, MixedRegistration);
        bool passed = faults == 0 && UcoFaultCount == 0 && MixedMatches == ScenarioCalls;
        return new TransportCheck(transport,
                                  "enum, bool, int64, mpos, hstring",
                                  passed,
                                  MixedMatches + "/" + ScenarioCalls + " intact, " + faults + " faults");
    }

    private static TransportCheck CheckFault(CallbackMode transport)
    {
        Registrations[PlainRegistration] = FaultHandler;
        UcoFaultCount = 0;
        int faults = RunScenario(transport, 0, FaultHandler, ScenarioCalls, false, PlainRegistration);
        Registrations[PlainRegistration] = ProbeHandler;

        // The runtime invoke and the thunk hand the exception back; the unmanaged entry parks it on the managed side
        int observed = transport == CallbackMode.UnmanagedCallersOnly ? UcoFaultCount : faults;
        UcoFault = null;
        UcoFaultCount = 0;
        return new TransportCheck(transport,
                                  "throwing handler",
                                  observed == ScenarioCalls,
                                  observed + "/" + ScenarioCalls + " faults reported");
    }

    private static TransportCheck CheckReentry(CallbackMode transport)
    {
        ReentryTransport = transport;
        ReentryInnerSum = 0;
        Registrations[PlainRegistration] = ReenterHandler;
        UcoFaultCount = 0;
        int faults = RunScenario(transport, 0, ReenterHandler, 8, false, PlainRegistration);
        Registrations[PlainRegistration] = ProbeHandler;

        // Every outer call entered the transport once more from inside the handler
        long expected = 8L * ProbeArgSum;
        bool passed = faults == 0 && UcoFaultCount == 0 && ReentryInnerSum == expected;
        return new TransportCheck(transport,
                                  "nested native-managed entry",
                                  passed,
                                  ReentryInnerSum + "/" + expected + " summed inside, " + faults + " faults");
    }

    private static int RunScenario(CallbackMode transport, int adapterKind, Delegate handler, int iterations,
                                   bool externalThread, int registrationId)
    {
        IntPtr ucoEntry = adapterKind == 0 ? UcoEntry : UcoMixedEntry;
        return Native.ProbeTransportScenario(handler,
                                             (int)transport,
                                             adapterKind,
                                             iterations,
                                             externalThread,
                                             ucoEntry,
                                             registrationId);
    }

    private static void OnProbeReenter(int a0, int a1, int a2, int a3)
    {
        // The nested entry runs on the plain handler, so the inner sum is what the inner call delivered
        Registrations[PlainRegistration] = ProbeHandler;
        long before = ProbeSum;
        Native.ProbeTransportScenario(ProbeHandler, (int)ReentryTransport, 0, 1, false, UcoEntry, PlainRegistration);
        ReentryInnerSum += ProbeSum - before;
        Registrations[PlainRegistration] = ReenterHandler;
        Sink += a0 + a1 + a2 + a3;
    }

    private static void OnProbe(int a0, int a1, int a2, int a3)
    {
        ProbeSum += a0 + a1 + a2 + a3;
    }

    // A nursery collection moves young objects, the handler delegate and its wrappers among them
    private static void OnProbeCollect(int a0, int a1, int a2, int a3)
    {
        GC.Collect(0);
        ProbeSum += a0 + a1 + a2 + a3;
    }

    private static void OnProbeFault(int a0, int a1, int a2, int a3)
    {
        Sink += a0 + a1 + a2 + a3;
        throw ProbeFault;
    }

    private static void OnProbeMixed(CallbackMode mode, bool flag, long wide, mpos hex, hstring tag)
    {
        bool intact = mode == CallbackMode.UnmanagedCallersOnly && flag && wide == MixedWide && hex.x == 7 &&
                      hex.y == -9 && tag == ProbeTag;
        MixedMatches += intact ? 1 : 0;
    }

    private sealed class InstanceTarget
    {
        private int Calls;

        public void OnCall(int a0, int a1, int a2, int a3)
        {
            Calls++;
            ProbeSum += a0 + a1 + a2 + a3 + (Calls > 0 ? 0 : 1);
        }
    }

    // The base body spoils the sum, so only a dispatch that reaches the override passes
    private class VirtualTarget
    {
        public virtual void OnCall(int a0, int a1, int a2, int a3)
        {
            ProbeSum -= 1;
        }
    }

    private sealed class DerivedVirtualTarget : VirtualTarget
    {
        public override void OnCall(int a0, int a1, int a2, int a3)
        {
            ProbeSum += a0 + a1 + a2 + a3;
        }
    }
}
