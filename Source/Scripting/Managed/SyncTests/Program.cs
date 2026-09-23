namespace FOnline.Tests;

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

internal static class Program
{
    private static readonly List<Sync.FailureInfo> Reports = new();

    private static async Task Main()
    {
        await CheckCase("success and empty requests stay silent", SuccessfulRequests);
        await CheckCase("all boolean acquisition overloads report the external caller once", EveryOverload);
        await CheckCase("post-acquisition destruction reports entity and phase", DestroyedDuringAcquire);
        await CheckCase("recovered map migration stays silent", RecoveredMigration);
        await CheckCase("best-effort operations and predicates stay silent", BestEffort);
        await CheckCase("partial restoration reports once and still covers survivors", PartialRestore);
        await CheckCase("holder and mapped-group failures have distinct reasons", DomainReasons);
        await CheckCase("native exceptions keep their existing path", NativeException);
        await CheckCase("caller data is preserved without presentation formatting", RawCallerData);
        await CheckCase("exhausted retries retain the caller across asynchronous yields", ExhaustedRetries);
        await CheckCase("repeated failures are counted without sampling", RepeatedFailures);
        await CheckCase("no subscribers preserve false results without collecting reports", NoSubscribers);
        await CheckCase("independent subscribers receive reports and can unsubscribe", MultipleSubscribers);
        await CheckCase("subscriber faults preserve false results and other deliveries", SubscriberException);
        await CheckCase("subscribers observe stable entity snapshots and read-only collections", SnapshotIsolation);
        await CheckCase("context IDs and prototypes retain their value types", TypedContext);
        Console.WriteLine("PASS: 16 sync diagnostic cases");
    }

    private static async Task CheckCase(string name, Func<Task> run)
    {
        Reports.Clear();
        Game.Held.Clear();
        Game.OnAcquire = null;
        ScriptTask.OnDelay = null;
        Native.Exceptions.Clear();
        Sync.OnFailure += Reports.Add;

        try {
            await run();
        }
        finally {
            Sync.OnFailure -= Reports.Add;
        }

        Console.WriteLine("PASS: " + name);
    }

    private static void Check(bool value, string message)
    {
        if (!value) {
            throw new InvalidOperationException(message);
        }
    }

    private static Sync.FailureInfo ReadSingle(string reason, [CallerMemberName] string caller = "")
    {
        Check(Reports.Count == 1, "Expected exactly one terminal failure, got " + Reports.Count);
        Sync.FailureInfo report = Reports[0];
        Check(report.Reason == reason, "Wrong failure reason: " + report.Reason);
        Check(report.CallerMember == caller, "Caller lost through delegation");
        Check(report.CallerLine > 0, "Missing caller line");
        Check(report.HelperLine > 0, "Missing failure branch line");
        Check(report.HelperFile.EndsWith("Sync.cs", StringComparison.Ordinal), "Missing helper source");
        Check(report.StackTrace.Length > 0, "Missing failure stack");
        return report;
    }

    private static async Task SuccessfulRequests()
    {
        Entity entity = new();
        Check(await Sync.Lock(entity), "Live lock failed");
        Check(await Sync.Widen(new List<Entity>()), "Empty widen failed");
        Check(await Sync.Lock(new List<Critter>()), "Empty typed lock failed");
        Check(await Sync.Lock(new Entity { IsAlwaysCovered = true }), "Static lock failed");
        Check(Reports.Count == 0, "Success produced a failure log");
    }

    private static Task EveryOverload() => CheckEveryOverload(true);

    private static async Task CheckEveryOverload(bool expectReports)
    {
        int checkedCount = 0;

        foreach (MethodInfo method in typeof(Sync).GetMethods(BindingFlags.Public | BindingFlags.Static)) {
            if (method.ReturnType != typeof(Task<bool>)) {
                continue;
            }

            ParameterInfo[] parameters = method.GetParameters();
            object?[] args = new object?[parameters.Length];

            for (int i = 0; i < parameters.Length; i++) {
                ParameterInfo parameter = parameters[i];
                Type type = parameter.ParameterType;

                if (parameter.Name == "callerFile") {
                    args[i] = "/project/Scripts/Quest.cs";
                }
                else if (parameter.Name == "callerMember") {
                    args[i] = nameof(EveryOverload);
                }
                else if (parameter.Name == "callerLine") {
                    args[i] = 42;
                }
                else if (typeof(Entity).IsAssignableFrom(type)) {
                    args[i] = DeadEntity(type);
                }
                else if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(List<>)) {
                    System.Collections.IList list =
                        (System.Collections.IList)(Activator.CreateInstance(type) ??
                                                   throw new InvalidOperationException("Cannot create fixture list"));
                    Type element = type.GetGenericArguments()[0];

                    if (typeof(Entity).IsAssignableFrom(element)) {
                        list.Add(DeadEntity(element));
                    }

                    args[i] = list;
                }
                else {
                    args[i] = Activator.CreateInstance(type);
                }
            }

            Reports.Clear();
            object? result = method.Invoke(null, args);
            Check(result is Task<bool>, "Missing helper task");
            Check(!await(Task<bool>)(result ?? throw new InvalidOperationException()), "Dead root accepted");
            checkedCount++;

            if (!expectReports) {
                Check(Reports.Count == 0, method + " emitted a diagnostic without subscribers");
                continue;
            }

            Check(Reports.Count == 1, method + " did not emit exactly one diagnostic");
            Sync.FailureInfo report = Reports[0];
            Check(report.CallerFile == "/project/Scripts/Quest.cs", "Wrong path");
            Check(report.CallerMember == nameof(EveryOverload), "Wrong caller");
            Check(report.CallerLine == 42, "Wrong call line");
            Check(report.Entities.Count > 0, "Missing entity context");
        }

        Check(checkedCount >= 60, "Acquisition overload coverage unexpectedly shrank");
    }

    private static Entity DeadEntity(Type type)
    {
        Entity entity = (Entity)(Activator.CreateInstance(type) ?? throw new InvalidOperationException());
        entity.IsDestroyed = true;
        return entity;
    }

    private static async Task DestroyedDuringAcquire()
    {
        Entity entity = new() { Id = new ident(17) };
        Game.OnAcquire = () => entity.IsDestroyed = true;
        Check(!await Sync.Lock(entity), "Destroyed entity accepted");
        Sync.FailureInfo report = ReadSingle("entity_unavailable_after_acquire");
        Sync.FailureEntity entityInfo = report.Entities[0];
        Check(report.Operation == nameof(Sync.Lock), "Wrong operation");
        Check(entityInfo.TypeName == nameof(Entity), "Entity type lost");
        Check(entityInfo.Id == new ident(17), "Entity ID lost");
        Check(entityInfo.IsDestroyed && !entityInfo.IsDestroying, "Lifecycle flags lost");
    }

    private static async Task RecoveredMigration()
    {
        Map oldMap = new() { Id = new ident(1) };
        Map newMap = new() { Id = new ident(2) };
        Critter cr = new() { Map = oldMap };
        int acquisitions = 0;
        Game.OnAcquire = () =>
        {
            acquisitions++;

            if (acquisitions == 2) {
                oldMap.IsDestroyed = true;
                cr.Map = newMap;
            }
        };
        Check(await Sync.WidenCritterWithMap(cr), "Migration did not recover");
        Check(acquisitions >= 4 && Game.Held.Contains(newMap), "Retry did not take the current map");
        Check(Reports.Count == 0, "Internal retry counted as terminal failure");
    }

    private static async Task BestEffort()
    {
        Critter dead = new() { IsDestroyed = true };
        Check(!Sync.IsCovered(dead), "Dead entity reported covered");
        await Sync.RestoreBestEffort(new List<Entity> { dead });
        await Sync.WidenBestEffort(dead);
        await Sync.WidenBestEffort(new List<Entity> { dead });
        await Sync.WidenCritterWithMapBestEffort(dead);
        await Sync.RestoreCallerCover(new List<Entity> { dead }, dead, dead);
        Check(Reports.Count == 0, "Intentional best-effort cleanup reported failure");
    }

    private static async Task PartialRestore()
    {
        Entity live = new();
        Entity dead = new() { IsDestroying = true };
        Check(!await Sync.Restore(new List<Entity> { live, dead }), "Partial restore reported complete");
        Check(Game.Held.Contains(live) && !Game.Held.Contains(dead), "Survivor restoration changed");
        ReadSingle("snapshot_incomplete");
    }

    private static async Task DomainReasons()
    {
        Check(!await Sync.LockItemWithHolder(new Item()), "Parentless item accepted");
        ReadSingle("holder_missing");
        Reports.Clear();
        Check(!await Sync.WidenCritterWithGlobalMapGroup(new Critter { Map = new Map { Id = new ident(1) } }),
              "Mapped critter accepted as global");
        ReadSingle("mapped_critter");
    }

    private static async Task NativeException()
    {
        Game.OnAcquire = () => throw new InvalidOperationException("native failure");

        try {
            await Sync.Lock(new Entity());
            throw new InvalidOperationException("Expected native failure");
        }
        catch (InvalidOperationException ex) when (ex.Message == "native failure") {
            Check(Reports.Count == 0, "Exception double-reported as false");
        }
    }

    private static async Task RawCallerData()
    {
        Check(!await Sync.Lock(new Entity { IsDestroyed = true }, "C:\\game\\Scripts\\Quest.cs", "Member\"\n\t\\", 3),
              "Dead root accepted");
        Sync.FailureInfo report = ReadSingle("entity_unavailable_before_acquire", "Member\"\n\t\\");
        Check(report.CallerFile == "C:\\game\\Scripts\\Quest.cs", "Caller path was reformatted");
    }

    private static async Task ExhaustedRetries()
    {
        int yields = 0;
        ScriptTask.OnDelay = async () =>
        {
            yields++;
            await Task.Yield();
        };
        Critter cr = new() { MapIdOverride = new ident(7) };
        Check(!await Sync.LockCrittersInitialInfoGraphs(new List<Entity>(), new List<Critter> { cr }),
              "Unstable graph unexpectedly accepted");
        Check(yields > 1, "Retry path never suspended");
        ReadSingle("retry_exhausted");
    }

    private static async Task RepeatedFailures()
    {
        Entity dead = new() { IsDestroyed = true };

        for (int i = 0; i < 10; i++) {
            Check(!await Sync.Lock(dead), "Dead entity accepted");
        }

        Check(Reports.Count == 10, "Repeated failures were suppressed");
    }

    private static async Task NoSubscribers()
    {
        Sync.OnFailure -= Reports.Add;
        await CheckEveryOverload(false);
        await SuccessfulRequests();

        Entity live = new();
        Entity dead = new() { IsDestroyed = true };
        Check(!await Sync.Restore(new List<Entity> { live, dead }), "Partial restore reported complete");
        Check(Game.Held.Contains(live), "Missing subscribers changed survivor restoration");
        Check(Reports.Count == 0, "Missing subscribers produced reports");
    }

    private static async Task MultipleSubscribers()
    {
        List<Sync.FailureInfo> first = new();
        List<Sync.FailureInfo> second = new();
        Sync.OnFailure -= Reports.Add;
        Sync.OnFailure += first.Add;
        Sync.OnFailure += second.Add;

        try {
            Entity dead = new() { IsDestroyed = true };
            Check(!await Sync.Lock(dead), "Dead entity accepted");
            Check(first.Count == 1 && second.Count == 1, "A subscriber missed the report");
            Check(ReferenceEquals(first[0], second[0]), "Subscribers received different report snapshots");
            Sync.OnFailure -= first.Add;
            Check(!await Sync.Lock(dead), "Dead entity accepted after unsubscribe");
            Check(first.Count == 1 && second.Count == 2, "Unsubscribe affected another subscriber");
            Check(Reports.Count == 0, "Removed subscriber received a report");
        }
        finally {
            Sync.OnFailure -= first.Add;
            Sync.OnFailure -= second.Add;
        }
    }

    private static async Task SubscriberException()
    {
        Exception failure = new InvalidOperationException("observer failure");
        Action<Sync.FailureInfo> faulty =
            _ => throw failure;
        List<Sync.FailureInfo> later = new();
        int before = ScriptExceptions.GlobalCount;
        Sync.OnFailure += faulty;
        Sync.OnFailure += later.Add;

        try {
            Check(!await Sync.Lock(new Entity { IsDestroyed = true }), "Subscriber fault changed the result");
            Check(Reports.Count == 1 && later.Count == 1, "Subscriber fault stopped another delivery");
            Check(Native.Exceptions.Count == 1 && ReferenceEquals(Native.Exceptions[0], failure),
                  "Subscriber exception was not reported");
            Check(ScriptExceptions.GlobalCount == before + 1, "Subscriber exception escaped accounting");
        }
        finally {
            Sync.OnFailure -= faulty;
            Sync.OnFailure -= later.Add;
        }
    }

    private static async Task SnapshotIsolation()
    {
        Entity dead = new() { Id = new ident(91), IsDestroying = true };
        List<Entity> source = new() { dead };
        Action<Sync.FailureInfo> mutateSource =
            _ =>
        {
            dead.IsDestroying = false;
            dead.IsDestroyed = true;
            source.Clear();
        };
        List<Sync.FailureInfo> later = new();
        Sync.OnFailure += mutateSource;
        Sync.OnFailure += later.Add;

        try {
            Check(!await Sync.Lock(source), "Destroyed entity accepted");
            Sync.FailureInfo report = ReadSingle("entity_unavailable_before_acquire");
            Check(later.Count == 1 && ReferenceEquals(later[0], report), "Subscribers did not share a snapshot");
            Check(source.Count == 0 && dead.IsDestroyed, "Source mutation did not run");
            Check(report.Entities.Count == 2, "Source collection mutation changed snapshot");
            Check(report.Entities[0].Id == new ident(91) && report.Entities[0].IsDestroying &&
                      !report.Entities[0].IsDestroyed,
                  "Entity mutation changed snapshot");
            Check(((ICollection<Sync.FailureEntity>)report.Entities).IsReadOnly, "Mutable entity collection exposed");
            Check(((ICollection<ident>)report.EntityIds).IsReadOnly, "Mutable ID collection exposed");
            Check(((ICollection<hstring>)report.ProtoIds).IsReadOnly, "Mutable prototype collection exposed");
        }
        finally {
            Sync.OnFailure -= mutateSource;
            Sync.OnFailure -= later.Add;
        }
    }

    private static async Task TypedContext()
    {
        Critter attached = new() { IsAttached = true, AttachMaster = new ident(93) };
        Check(!await Sync.WidenCritterAttachmentGraph(attached), "Missing attachment master accepted");
        Sync.FailureInfo attachment = ReadSingle("attachment_master_missing");
        Check(attachment.EntityIds.Count == 1 && attachment.EntityIds[0] == new ident(93), "Scalar ID lost");
        Reports.Clear();

        Critter grouped = new() { GlobalMapTripId = 1, Members = new List<ident> { new(94), new(95) } };
        Check(!await Sync.WidenCritterAttachmentGraph(grouped), "Incomplete group accepted");
        Sync.FailureInfo group = ReadSingle("group_member_missing");
        grouped.Members.Clear();
        Check(group.EntityIds.Count == 2 && group.EntityIds[0] == new ident(94) && group.EntityIds[1] == new ident(95),
              "Member IDs lost or retained by reference");
        Reports.Clear();

        hstring proto = new("RequestedItem");
        List<hstring> protos = new() { proto };
        Critter cr = new() { OnGetItems =
                                 _ => new List<Item> { new() } };
        Check(!await Sync.WidenCritterItemsForDestroy(cr, protos), "Changing item graph accepted");
        Sync.FailureInfo items = ReadSingle("retry_exhausted");
        protos.Clear();
        Check(items.ProtoIds.Count == 1 && items.ProtoIds[0] == proto, "Prototype IDs lost or formatted as text");
    }
}
