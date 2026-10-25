namespace EngineTests;

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using FOnline;

public static class ManagedRuntimeProbe
{
    private static TaskCompletionSource<bool>? TimerGate;
    private static int TimerStarts;
    private static int TimerFinishes;
    private static int TimerMode;
    private static long TimerFinishFrame;
    private static long TimerFirstFinishFrame;
    private static long TimerSecondStartFrame;
    private static int TimerErrorsBefore;

    [CallableFromNative]
    internal static uint StartAwaitedTimer(int mode)
    {
        TimerGate = new TaskCompletionSource<bool>();
        TimerStarts = 0;
        TimerFinishes = 0;
        TimerMode = mode;
        TimerFinishFrame = 0;
        TimerFirstFinishFrame = 0;
        TimerSecondStartFrame = 0;
        TimerErrorsBefore = ScriptExceptions.GlobalCount;
        return Game.StartTimeEvent(new timespan(1_000_000L),
                                   new timespan(mode == 3 ? 0L : 1_000_000_000L), AwaitedTimer);
    }

    [CallableFromNative]
    internal static uint QueueAwaitedTimer()
    {
        return Game.StartTimeEvent(new timespan(30_000_000_000L), AwaitedTimer);
    }

    [CallableFromNative]
    internal static void ReleaseAwaitedTimer()
    {
        Invariant.Verify(TimerGate != null, "Timer gate must be initialized");
        TimerGate.SetResult(true);
    }

    [CallableFromNative]
    internal static Task<int> GetCompletedTaskValue() => Task.FromResult(42);

    [CallableFromNative]
    internal static Task<string> GetCompletedTaskText() => Task.FromResult("completed");

    [CallableFromNative]
    internal static int GetTimerStarts() => TimerStarts;

    [CallableFromNative]
    internal static int GetTimerFinishes() => TimerFinishes;

    [CallableFromNative]
    internal static int GetTimerErrors() => ScriptExceptions.GlobalCount - TimerErrorsBefore;

    [CallableFromNative]
    internal static long GetTimerFinishFrame() => TimerFinishFrame;

    [CallableFromNative]
    internal static long GetTimerFirstFinishFrame() => TimerFirstFinishFrame;

    [CallableFromNative]
    internal static long GetTimerSecondStartFrame() => TimerSecondStartFrame;

    [TimeEvent]
    internal static Task AwaitedTimer(TimeEventContext context)
    {
        int waitingById = Game.CountTimeEvent(context.GetId(), excludeExecuting: true);
        int activeById = Game.CountTimeEvent(context.GetId());
        int waitingByCallback = Game.CountTimeEvent(AwaitedTimer, excludeExecuting: true);
        int activeByCallback = Game.CountTimeEvent(AwaitedTimer, excludeExecuting: false);
        Invariant.Verify(waitingById == 0 && waitingByCallback == 0 && activeById == 1 && activeByCallback == 1,
                         "Running timer count must honor excludeExecuting");

        return RunAwaitedTimer(context);
    }

    private static async Task RunAwaitedTimer(TimeEventContext context)
    {
        TimerStarts++;

        if (TimerStarts == 2) {
            TimerSecondStartFrame = Game.FrameTime.value;
        }

        if (TimerMode == 9 || TimerMode == 10) {
            context.SetData("before await");
            context.Repeat(new timespan(5_000_000_000L));
        }

        Invariant.Verify(TimerGate != null, "Timer gate must be initialized");
        await TimerGate.Task;
        TimerFinishes++;
        TimerFinishFrame = Game.FrameTime.value;

        if (TimerFinishes == 1) {
            TimerFirstFinishFrame = TimerFinishFrame;
        }

        if (TimerMode == 1 || TimerMode == 6 || TimerMode == 8) {
            context.SetData("after await");
            context.Repeat(new timespan(5_000_000_000L));
        }
        else if (TimerMode == 2 || TimerMode == 10) {
            context.Stop();
        }

        if (TimerMode == 4 || TimerMode == 8 || TimerMode == 9 || TimerMode == 10) {
            throw new InvalidOperationException("Awaited timer probe failure");
        }
    }

#if SERVER
    [CallableFromNative]
    internal static uint StartAwaitedCritterTimer(Critter owner)
    {
        TimerGate = new TaskCompletionSource<bool>();
        TimerStarts = 0;
        TimerFinishes = 0;
        TimerFinishFrame = 0;
        TimerFirstFinishFrame = 0;
        TimerSecondStartFrame = 0;
        TimerMode = 0;
        return owner.StartTimeEvent(new timespan(1_000_000L), new timespan(1_000_000_000L), AwaitedCritterTimer);
    }

    [TimeEvent]
    internal static Task AwaitedCritterTimer(Critter owner, TimeEventContext context)
    {
        int waitingById = owner.CountTimeEvent(context.GetId(), excludeExecuting: true);
        int activeById = owner.CountTimeEvent(context.GetId());
        int waitingByCallback = owner.CountTimeEvent(AwaitedCritterTimer, excludeExecuting: true);
        int activeByCallback = owner.CountTimeEvent(AwaitedCritterTimer, excludeExecuting: false);
        Invariant.Verify(waitingById == 0 && waitingByCallback == 0 && activeById == 1 && activeByCallback == 1,
                         "Running entity timer count must honor excludeExecuting");

        return RunAwaitedTimer(context);
    }
#endif

    private static int InitCalls;
#if SERVER
    private static int CritterInitCalls;
    private static long LastCritterId;
    private static TaskCompletionSource? SnapshotGate;
    private static int SnapshotResumed;
    private static int SnapshotTimerCalls;
#endif

    [ModuleInit]
    internal static void Init()
    {
        InitCalls++;
#if SERVER
        Game.OnCritterInit.Subscribe(OnCritterInit);
#endif
    }

#if SERVER
    [CallableFromNative]
    internal static async Task WaitForSnapshotGate()
    {
        SnapshotGate = new TaskCompletionSource();
        await SnapshotGate.Task;
        SnapshotResumed++;
    }

    [CallableFromNative]
    internal static void ReleaseSnapshotGate()
    {
        TaskCompletionSource completion = SnapshotGate ?? throw new InvalidOperationException("Snapshot gate was not started");
        completion.SetResult();
    }

    [CallableFromNative]
    internal static void CancelSnapshotGate()
    {
        TaskCompletionSource completion = SnapshotGate ?? throw new InvalidOperationException("Snapshot gate was not started");
        completion.SetCanceled();
    }

    [CallableFromNative]
    internal static int GetSnapshotResumed() => SnapshotResumed;

    [TimeEvent]
    internal static void OnSnapshotTimer() => SnapshotTimerCalls++;

    [TimeEvent]
    internal static void OnSnapshotCritterTimer(Critter critter)
    {
        if (critter.IsDestroyed) {
            throw new InvalidOperationException("Restored timer owner is destroyed");
        }
        SnapshotTimerCalls++;
    }

    [CallableFromNative]
    internal static int GetSnapshotTimerCalls() => SnapshotTimerCalls;

    [Event]
    internal static void OnCritterInit(Critter critter, bool firstTime)
    {
        CritterInitCalls++;
        LastCritterId = critter.Id.value;
    }

    [CallableFromNative]
    internal static int TestConditionalDatabaseRecords()
    {
        hstring collection = "test_collection".hstr();
        ident id = new ident(1234511);
        Dictionary<string, string> first = new Dictionary<string, string> { { "Name", "First" } };
        Dictionary<string, string> second = new Dictionary<string, string> { { "Name", "Second" } };

        if (!Game.DbTryInsertRecord(collection, id, first)) {
            return -1;
        }
        if (Game.DbTryInsertRecord(collection, id, second)) {
            return -2;
        }
        if (Game.DbGetRecord(collection, id)["Name"] != "First") {
            return -3;
        }
        if (!Game.DbCompareExchangeRecordString(collection, id, "Name", "First", "Advanced")) {
            return -4;
        }
        if (Game.DbCompareExchangeRecordString(collection, id, "Name", "First", "Wrong")) {
            return -5;
        }
        if (Game.DbCompareExchangeRecordString(collection, id, "Missing", "", "Wrong")) {
            return -6;
        }

        collection = "test_strings".hstr();
        string key = "conditional-key";

        if (!Game.DbTryInsertRecord(collection, key, first)) {
            return -7;
        }
        if (Game.DbTryInsertRecord(collection, key, second)) {
            return -8;
        }
        if (!Game.DbCompareExchangeRecordString(collection, key, "Name", "First", "Advanced")) {
            return -9;
        }
        if (Game.DbCompareExchangeRecordString(collection, key, "Name", "First", "Wrong")) {
            return -10;
        }
        if (Game.DbGetRecord(collection, key)["Name"] != "Advanced") {
            return -11;
        }

        return 0;
    }

    [CallableFromNative]
    internal static int GetCritterInitCalls() => CritterInitCalls;

    [CallableFromNative]
    internal static long GetLastCritterId() => LastCritterId;

    [CallableFromNative]
    internal static long GetCritterId(Critter critter) => critter.Id.value;

    [CallableFromNative]
    internal static bool IsItemNull(Item? item) => item == null;

    [CallableFromNative]
    internal static void UpdateString(Critter first, Critter second, ref string value)
    {
        value = first.Id == second.Id ? "same:" + value : "different:" + value;
    }
#endif

    [CallableFromNative]
    internal static int GetInitCalls() => InitCalls;

    [CallableFromNative]
    internal static int SumArray(List<int> values)
    {
        int sum = 0;

        foreach (int value in values) {
            sum += value;
        }

        return sum;
    }

    [CallableFromNative]
    internal static List<int> TransformArray(List<int> values)
    {
        values[0] += 10;
        values.Add(77);
        return values;
    }

    [CallableFromNative]
    internal static int SumDictionary(Dictionary<int, int> values)
    {
        int sum = 0;

        foreach (KeyValuePair<int, int> value in values) {
            sum += value.Key + value.Value;
        }

        return sum;
    }

    [CallableFromNative]
    internal static Dictionary<int, int> TransformDictionary(Dictionary<int, int> values)
    {
        values[1] += 5;
        values[3] = 30;
        return values;
    }

    [CallableFromNative]
    internal static string EchoString(string value) => value;

    [CallableFromNative]
    internal static hstring EchoHash(hstring value) => value;

    [CallableFromNative]
    internal static void ThrowFromScript() => throw new InvalidOperationException("Managed runtime probe failure");
}
