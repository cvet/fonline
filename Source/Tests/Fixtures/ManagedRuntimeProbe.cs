namespace EngineTests;

using System;
using System.Collections.Generic;
using System.Threading;
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
                                   new timespan(mode == 3 ? 0L : 1_000_000_000L),
                                   AwaitedTimer);
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
    private static TaskCompletionSource? PendingRemoteCall;

    [ServerRemoteCall]
    internal static Task UnitTestRuntimeRpc(Player player)
    {
        // An ownerless nested RPC must not inherit or release this invocation's completion
        RemoteCall.Loopback(null, "UnitTestRuntimeGenericRpc");
        PendingRemoteCall = new TaskCompletionSource();
        return PendingRemoteCall.Task;
    }

    [CallableFromNative]
    internal static void FinishRuntimeRpc(int outcome)
    {
        TaskCompletionSource pending =
            PendingRemoteCall ?? throw new InvalidOperationException("No pending runtime RPC probe");
        PendingRemoteCall = null;

        if (outcome == 0) {
            pending.SetResult();
        }
        else if (outcome == 1) {
            pending.SetCanceled();
        }
        else {
            pending.SetException(new InvalidOperationException("Deferred runtime RPC probe failure"));
        }
    }

    [CallableFromNative]
    internal static void FinishRuntimeRpcOffThread(int outcome)
    {
        Task.Run(() => FinishRuntimeRpc(outcome)).GetAwaiter().GetResult();
    }

    [CallableFromNative]
    internal static void RaceRuntimeRpcCompletionWithTeardown()
    {
        using ManualResetEventSlim start = new();
        Task worker = Task.Run(() =>
                               {
                                   start.Wait();
                                   FinishRuntimeRpc(0);
                               });
        start.Set();
        Native.BeginBackendTeardown();
        worker.GetAwaiter().GetResult();
    }

    [CallableFromNative]
    internal static int GetRuntimeScriptErrors() => ScriptExceptions.GlobalCount;

    private static int GenericRpcFinishes;

    [ServerRemoteCall]
    internal static async Task<int> UnitTestRuntimeGenericRpc(Player player)
    {
        await Task.Yield();
        GenericRpcFinishes++;
        return 73;
    }

    [CallableFromNative]
    internal static int GetGenericRpcFinishes() => GenericRpcFinishes;

    [ServerRemoteCall]
    internal static Task
    UnitTestRuntimeImmediateCanceledRpc(Player player) => Task.FromCanceled(new CancellationToken(true));

    [ServerRemoteCall]
    internal static Task UnitTestRuntimeImmediateSuccessRpc(Player player) => Task.CompletedTask;

    [CallableFromNative]
    internal static long MeasureRuntimeRpcMetadataLookups()
    {
        RemoteCall.Loopback(null, "UnitTestRuntimeRpc");
        FinishRuntimeRpc(0);
        InteropProbe.Counters start = InteropProbe.Counters.Read(true);

        try {
            RemoteCall.Loopback(null, "UnitTestRuntimeRpc");
            FinishRuntimeRpc(0);
            return InteropProbe.Counters.Read(true).Since(start).MetadataLookups;
        }
        finally {
            _ = InteropProbe.Counters.Read(false);
        }
    }

    private const int PropertyCallbackSlots = 22;
    private static readonly int[] PropertyCalls = new int[PropertyCallbackSlots];
    private static readonly int[] PropertyValues = new int[PropertyCallbackSlots];
    private static readonly int[] PropertyCompletions = new int[PropertyCallbackSlots];
    private static readonly int[] PropertyReverseIndices = new int[PropertyCallbackSlots];
    private static readonly int[] PropertyReverseIdentities = new int[PropertyCallbackSlots];
    private static readonly long[] PropertyOwners = new long[PropertyCallbackSlots];
    private static readonly string?[] PropertyFailures = new string?[PropertyCallbackSlots];
#endif

    [ModuleInit]
    internal static void Init()
    {
        InitCalls++;
#if SERVER
        Game.OnCritterInit.Subscribe(OnCritterInit);
        Game.AddPropertySetter(CritterProperty.RuntimeStoredText,
                               (Critter owner, ref string value) =>
                               {
                                   PropertySetterCalls++;
                                   value = "set:" + value;
                               });
        Game.SetPropertyGetter(CritterProperty.RuntimeVirtualText,
                               (Critter owner) =>
                               {
                                   PropertyGetterCalls++;
                                   return "get:" + owner.RuntimeStoredText;
                               });
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
        TaskCompletionSource completion =
            SnapshotGate ?? throw new InvalidOperationException("Snapshot gate was not started");
        completion.SetResult();
    }

    [CallableFromNative]
    internal static void CancelSnapshotGate()
    {
        TaskCompletionSource completion =
            SnapshotGate ?? throw new InvalidOperationException("Snapshot gate was not started");
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

    private static int PropertySetterCalls;
    private static int PropertyGetterCalls;

    [CallableFromNative]
    internal static string ProbePropertyBridge(Critter owner)
    {
        int setters = PropertySetterCalls;
        int getters = PropertyGetterCalls;
        owner.RuntimeStoredText = "probe";
        string result = owner.RuntimeStoredText + "|" + owner.RuntimeVirtualText;
        return result + "|" + (PropertySetterCalls - setters) + "|" + (PropertyGetterCalls - getters);
    }

    [Event]
    internal static void OnCritterInit(Critter critter, bool firstTime)
    {
        CritterInitCalls++;
        LastCritterId = critter.Id.value;
    }

    [CallableFromNative]
    internal static int RegistryAnyUncovered(long playerId, long npcId, long deadNpcId)
    {
        Critter? player = Game.GetCritter(new ident(playerId));
        Critter? npc = Game.GetCritter(new ident(npcId));
        Critter? deadNpc = Game.GetCritter(new ident(deadNpcId));
        if (player == null || npc == null || deadNpc == null) {
            return -1;
        }
        if (Game.IsEntityLocked(player) || Game.IsEntityLocked(npc) || Game.IsEntityLocked(deadNpc)) {
            return -2;
        }

        List<Critter> values = Game.GetCritters(CritterFindType.Any);
        if (values.Count != 3 || !values.Contains(player) || !values.Contains(npc) || !values.Contains(deadNpc)) {
            return -3;
        }
        if (Game.IsEntityLocked(player) || Game.IsEntityLocked(npc) || Game.IsEntityLocked(deadNpc)) {
            return -4;
        }
        return 0;
    }

    [CallableFromNative]
    internal static string RegistryUncoveredFailure(int query, long npcId)
    {
        Critter? npc = Game.GetCritter(new ident(npcId));
        if (npc == null || Game.IsEntityLocked(npc)) {
            return "INVALID_FIXTURE_COVER";
        }
        try {
            if (query == 10) {
                return "NO_EXCEPTION:" + npc.ControlledByPlayer;
            }
            return "NO_EXCEPTION:" + RegistrySelectedCritters(query).Count;
        }
        catch (InvalidOperationException exception) {
            return exception.GetType().Name + "|" + exception.Message;
        }
    }

    [CallableFromNative]
    internal static List<long> RegistryCoveredIds(int query)
    {
        List<long> result = new List<long>();
        foreach (Critter critter in RegistrySelectedCritters(query)) {
            result.Add(critter.Id.value);
        }
        return result;
    }

    private static List<Critter> RegistrySelectedCritters(int query) => query switch {
        0 => Game.GetCritters(CritterFindType.Any),
        1 => Game.GetCritters(CritterFindType.Players),
        2 => Game.GetCritters(CritterFindType.Npc),
        3 => Game.GetCritters(CritterFindType.Dead),
        4 => Game.GetCritters(CritterFindType.NonDeadNpc),
        5 => Game.GetCritters(CritterFindType.DeadPlayers),
        6 => Game.GetCritters(CritterFindType.NonDeadPlayers),
        7 => Game.GetAllNpc(),
        8 => Game.GetAllNpc("UnitTestRat".hstr()),
        9 => Game.GetAllNpc(Game.GetProtoCritter("UnitTestRat".hstr())),
        _ => throw new InvalidOperationException("Unknown registry fixture query"),
    };

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
    internal static Critter? EchoCritter(Critter? critter, bool fail)
    {
        if (fail) {
            throw new InvalidOperationException("Expected entity-return failure");
        }

        return critter;
    }

    [CallableFromNative]
    internal static Critter EchoNonNullCritter(Critter critter) => critter;

    [CallableFromNative]
    internal static bool IsItemNull(Item? item) => item == null;

    [CallableFromNative]
    internal static void UpdateString(Critter first, Critter second, ref string value)
    {
        value = first.Id == second.Id ? "same:" + value : "different:" + value;
    }

    [CallableFromNative]
    internal static List<int> GetCallbackPropertyIndices() => new List<int> {
        (int)CritterProperty.Callback_Probe_Getter_Value,
        (int)CritterProperty.Callback_Probe_Deferred_Action,
        (int)CritterProperty.Callback_Probe_Alias_Action,
        (int)CritterProperty.Callback_Probe_Deferred_Task,
        (int)CritterProperty.Callback_Probe_Alias_Task,
        (int)CritterProperty.Callback_Probe_Transform_Value,
        (int)CritterProperty.Callback_Probe_With_Property,
        (int)CritterProperty.RuntimeGetter,
        (int)CritterProperty.RuntimeDeferredAction,
        (int)CritterProperty.RuntimeAliasAction,
        (int)CritterProperty.RuntimeDeferredTask,
        (int)CritterProperty.RuntimeAliasTask,
        (int)CritterProperty.RuntimeTransform,
        (int)CritterProperty.RuntimeWithProperty,
        (int)CritterProperty.RuntimeRecoveryGetter,
        (int)CritterProperty.RuntimeRecoveryDeferredAction,
        (int)CritterProperty.RuntimeRecoveryAliasAction,
        (int)CritterProperty.RuntimeRecoveryDeferredTask,
        (int)CritterProperty.RuntimeRecoveryAliasTask,
        (int)CritterProperty.RuntimeRecoveryTransform,
        (int)CritterProperty.RuntimeRecoveryWithProperty,
        (int)CritterProperty.Runtime_Plain_Underscore,
    };

    [CallableFromNative]
    internal static bool HasCallbackComponent(Critter critter) =>
        critter.HasCallback_Probe && critter.Callback_Probe.Deferred_Action == 0;

    [CallableFromNative]
    internal static List<int> GetPropertyCallbackState(int slot) => new List<int> {
        PropertyCalls[slot], PropertyValues[slot], PropertyCompletions[slot],
        PropertyReverseIndices[slot], PropertyReverseIdentities[slot],
    };

    [CallableFromNative]
    internal static long GetPropertyCallbackOwner(int slot) => PropertyOwners[slot];

    [CallableFromNative]
    internal static string GetPropertyCallbackFailure(int slot) => PropertyFailures[slot] ?? string.Empty;

    [CallableFromNative]
    internal static string RegisterPropertyCallback(int slot, int form, int propertyIndex)
    {
        CritterProperty property = (CritterProperty)propertyIndex;

        try {
            switch (form) {
            case 0:
                Game.SetPropertyGetter<int>(property, critter => {
                    int value = 731 + slot;
                    RecordPropertyCallback(critter, slot, value);
                    return value;
                });
                break;
            case 1:
                Action<Critter> deferredAction = critter =>
                    RecordPropertyCallback(critter, slot, ReadCallbackProperty(critter, property));
                Game.AddPropertyDeferredSetter(property, deferredAction);
                break;
            case 2:
                Action<Critter> aliasAction = critter =>
                    RecordPropertyCallback(critter, slot, ReadCallbackProperty(critter, property));
                Game.AddPropertySetter(property, aliasAction);
                break;
            case 3:
                Func<Critter, Task> deferredTask = critter => RunAsyncPropertyCallback(critter, slot, property);
                Game.AddPropertyDeferredSetter(property, deferredTask);
                break;
            case 4:
                Func<Critter, Task> aliasTask = critter => RunAsyncPropertyCallback(critter, slot, property);
                Game.AddPropertySetter(property, aliasTask);
                break;
            case 5:
                PropertySetter<Critter, int> valueSetter = (Critter critter, ref int value) => {
                    value += 10;
                    RecordPropertyCallback(critter, slot, value);
                };
                Game.AddPropertySetter(property, valueSetter);
                break;
            case 6:
                PropertySetterWithProperty<Critter, CritterProperty, int> propertySetter = (Critter critter, CritterProperty actualProperty, ref int value) => {
                    value += 20;
                    PropertyReverseIndices[slot] = (int)actualProperty;
                    PropertyReverseIdentities[slot] = actualProperty == property ? 1 : 0;
                    RecordPropertyCallback(critter, slot, value);
                };
                Game.AddPropertySetter(property, propertySetter);
                break;
            default:
                throw new ArgumentOutOfRangeException(nameof(form));
            }

            return string.Empty;
        }
        catch (Exception exception) {
            // The test distinguishes the published native rejection from acceptance or a different failure
            return exception.GetType().Name + "|" + exception.Message;
        }
    }

    private static void RecordPropertyCallback(Critter critter, int slot, int value)
    {
        PropertyCalls[slot]++;
        PropertyValues[slot] = value;
        PropertyOwners[slot] = critter.Id.value;
        PropertyCompletions[slot]++;
    }

    private static async Task RunAsyncPropertyCallback(Critter critter, int slot, CritterProperty property)
    {
        try {
            PropertyCalls[slot]++;
            PropertyOwners[slot] = critter.Id.value;
            await ScriptTask.Delay(20);

            if (!await Sync.Lock(critter)) {
                PropertyFailures[slot] = "Callback entity became unavailable";
                return;
            }

            PropertyValues[slot] = ReadCallbackProperty(critter, property);
            PropertyCompletions[slot]++;
        }
        catch (Exception exception) {
            PropertyFailures[slot] = exception.ToString();
        }
    }

    private static int ReadCallbackProperty(Critter critter, CritterProperty property) => property switch {
        CritterProperty.Callback_Probe_Deferred_Action => critter.Callback_Probe.Deferred_Action,
        CritterProperty.Callback_Probe_Alias_Action => critter.Callback_Probe.Alias_Action,
        CritterProperty.Callback_Probe_Deferred_Task => critter.Callback_Probe.Deferred_Task,
        CritterProperty.Callback_Probe_Alias_Task => critter.Callback_Probe.Alias_Task,
        CritterProperty.RuntimeDeferredAction => critter.RuntimeDeferredAction,
        CritterProperty.RuntimeAliasAction => critter.RuntimeAliasAction,
        CritterProperty.RuntimeDeferredTask => critter.RuntimeDeferredTask,
        CritterProperty.RuntimeAliasTask => critter.RuntimeAliasTask,
        CritterProperty.RuntimeRecoveryDeferredAction => critter.RuntimeRecoveryDeferredAction,
        CritterProperty.RuntimeRecoveryAliasAction => critter.RuntimeRecoveryAliasAction,
        CritterProperty.RuntimeRecoveryDeferredTask => critter.RuntimeRecoveryDeferredTask,
        CritterProperty.RuntimeRecoveryAliasTask => critter.RuntimeRecoveryAliasTask,
        _ => throw new InvalidOperationException("Property has no entity-only callback fixture"),
    };
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
