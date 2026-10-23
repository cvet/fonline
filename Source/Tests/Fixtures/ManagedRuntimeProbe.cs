namespace EngineTests;

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using FOnline;

public static class ManagedRuntimeProbe
{
    private static int InitCalls;
#if SERVER
    private static int CritterInitCalls;
    private static long LastCritterId;
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
#endif
    }

#if SERVER
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
