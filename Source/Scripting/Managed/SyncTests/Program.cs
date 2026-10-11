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
        await CheckCase("every acquisition overload refuses a destroyed root without publishing it", EveryOverload);
        await CheckCase("destruction during acquisition is refused without a report", DestroyedDuringAcquire);
        await CheckCase("an entity its own thread is destroying stays available", CoveredDestroyingEntity);
        await CheckCase("an entity another thread is destroying is refused before any wait", ForeignDestroyingEntity);
        await CheckCase("recovered map migration stays silent", RecoveredMigration);
        await CheckCase("best-effort operations and predicates stay silent", BestEffort);
        await CheckCase("partial restoration covers survivors and publishes nothing", PartialRestore);
        await CheckCase("holder and mapped-group failures have distinct reasons", DomainReasons);
        await CheckCase("native exceptions keep their existing path", NativeException);
        await CheckCase("caller data is preserved without presentation formatting", RawCallerData);
        await CheckCase("an unresolved entity defers the retry to a later frame", ExhaustedRetries);
        await CheckCase("a changed relation waits in the engine and never suspends", ChangedRelationWaitsInEngine);
        await CheckCase("every retry is published with its site, including nested ones", RetriesArePublished);
        await CheckCase("repeated failures are counted without sampling", RepeatedFailures);
        await CheckCase("no subscribers preserve false results without collecting reports", NoSubscribers);
        await CheckCase("independent subscribers receive reports and can unsubscribe", MultipleSubscribers);
        await CheckCase("subscriber faults preserve false results and other deliveries", SubscriberException);
        await CheckCase("subscribers observe stable entity snapshots and read-only collections", SnapshotIsolation);
        await CheckCase("context IDs and prototypes retain their value types", TypedContext);
        Console.WriteLine("PASS: 18 sync diagnostic cases");
    }

    private static async Task CheckCase(string name, Func<Task> run)
    {
        Reports.Clear();
        Game.Held.Clear();
        Game.OnAcquire = null;
        Game.Yields = 0;
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

    // Every overload is driven with a destroyed root: the refusal is the answer the caller needs, and destruction
    // explains it, so no overload may publish it
    private static Task EveryOverload() => CheckEveryOverload();

    private static async Task CheckEveryOverload()
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
            Check(Reports.Count == 0, method + " published a refusal destruction explains");
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
        Check(Reports.Count == 0, "Destruction during acquisition was published");
    }

    // A finish handler runs on the thread that destroys its subject and already covers it
    private static async Task CoveredDestroyingEntity()
    {
        Entity own = new() { Id = new ident(21), IsDestroying = true };
        Game.Held.Add(own);
        Check(Sync.IsCovered(own), "Own teardown subject reported uncovered");
        Check(await Sync.Widen(own), "Own teardown subject refused by a widen");
        Check(await Sync.Lock(own), "Own teardown subject refused by a lock");
        Check(await Sync.Restore(new List<Entity> { own }), "Own teardown subject dropped from a restore");
        Check(Game.Held.Contains(own), "Own teardown subject lost its cover");
        Check(Reports.Count == 0, "An available teardown subject produced a failure");
    }

    private static async Task ForeignDestroyingEntity()
    {
        Entity foreign = new() { Id = new ident(22), IsDestroying = true };
        int acquisitions = 0;
        Game.OnAcquire = () => acquisitions++;
        Check(!Sync.IsCovered(foreign), "Foreign teardown subject reported covered");
        Check(!await Sync.Lock(foreign), "Foreign teardown subject accepted by a lock");
        Check(!await Sync.Widen(foreign), "Foreign teardown subject accepted by a widen");
        Check(acquisitions == 0, "The refusal waited on the destroyer");
        Check(Reports.Count == 0, "A foreign teardown refusal was published");
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
        Check(Reports.Count == 0, "A snapshot shortened by destruction was published");
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
        Check(!await Sync.LockItemWithHolder(new Item(), "C:\\game\\Scripts\\Quest.cs", "Member\"\n\t\\", 3),
              "Parentless item accepted");
        Sync.FailureInfo report = ReadSingle("holder_missing", "Member\"\n\t\\");
        Check(report.CallerFile == "C:\\game\\Scripts\\Quest.cs", "Caller path was reformatted");
    }

    private static async Task ExhaustedRetries()
    {
        int frameYields = 0;
        ScriptTask.OnDelay = async () =>
        {
            frameYields++;
            await Task.Yield();
        };
        // A map id with no map is a critter mid-load or mid-unload, which may be the caller's own operation
        Critter cr = new() { MapIdOverride = new ident(7) };
        Check(!await Sync.LockCrittersInitialInfoGraphs(new List<Entity>(), new List<Critter> { cr }),
              "Unstable graph unexpectedly accepted");
        Check(frameYields > 1, "Unresolved placement never deferred to a later frame");
        Check(Game.Yields == 0, "Unresolved placement waited in the engine, where its own caller cannot finish");
        ReadSingle("retry_exhausted");
    }

    private static async Task ChangedRelationWaitsInEngine()
    {
        int frameYields = 0;
        ScriptTask.OnDelay = async () =>
        {
            frameYields++;
            await Task.Yield();
        };
        Critter cr = new();
        Item first = new() { Holder = cr, Ownership = ItemOwnership.CritterInventory };
        Item second = new() { Holder = cr, Ownership = ItemOwnership.CritterInventory };
        int reads = 0;
        cr.OnGetItems =
            _ => ++reads == 1 ? new List<Item> { first } : new List<Item> { first, second };

        Task<bool> attempt = Sync.WidenCritterItemsForDestroy(cr, new hstring("Knife"));
        Check(attempt.IsCompleted, "A changed relation suspended instead of waiting in the engine");
        Check(await attempt, "A settled relation was not accepted");
        Check(Game.Yields == 1 && frameYields == 0, "A changed relation did not wait in the engine exactly once");
        Check(Game.Held.Contains(second), "The retry did not cover the current item set");
        Check(Reports.Count == 0, "A recovered retry counted as a failure");
    }

    private static async Task RetriesArePublished()
    {
        List<Sync.RetryInfo> retries = new();
        Sync.OnRetry += retries.Add;

        try {
            Critter cr = new() { MapIdOverride = new ident(7) };
            Check(!await Sync.LockCrittersInitialInfoGraphs(new List<Entity>(), new List<Critter> { cr }),
                  "Unstable graph unexpectedly accepted");
            Check(retries.Count > 1 && Game.Yields == 0, "Deferred retries were not published");
            Check(retries.TrueForAll(retry => retry.Reason == "placement_unresolved"), "Wrong retry reason");
            Check(retries.TrueForAll(retry => retry.CallerMember == nameof(RetriesArePublished)),
                  "Retry lost its caller");
            Check(retries.TrueForAll(retry => retry.HelperLine > 0 &&
                                              retry.HelperFile.EndsWith("Sync.cs", StringComparison.Ordinal)),
                  "Retry lost its site");

            retries.Clear();
            Sync.ReportRetry("loop_changed");
            Check(retries.Count == 1 && retries[0].Reason == "loop_changed" &&
                      retries[0].CallerMember == nameof(RetriesArePublished) && retries[0].HelperLine > 0,
                  "An outside retry loop was not published at its own site");
        }
        finally {
            Sync.OnRetry -= retries.Add;
        }

        Reports.Clear();
    }

    private static async Task RepeatedFailures()
    {
        Item parentless = new();

        for (int i = 0; i < 10; i++) {
            Check(!await Sync.LockItemWithHolder(parentless), "Parentless item accepted");
        }

        Check(Reports.Count == 10, "Repeated failures were suppressed");
    }

    private static async Task NoSubscribers()
    {
        Sync.OnFailure -= Reports.Add;
        await CheckEveryOverload();
        await SuccessfulRequests();
        Check(!await Sync.LockItemWithHolder(new Item()), "Parentless item accepted");

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
            Item parentless = new();
            Check(!await Sync.LockItemWithHolder(parentless), "Parentless item accepted");
            Check(first.Count == 1 && second.Count == 1, "A subscriber missed the report");
            Check(ReferenceEquals(first[0], second[0]), "Subscribers received different report snapshots");
            Sync.OnFailure -= first.Add;
            Check(!await Sync.LockItemWithHolder(parentless), "Parentless item accepted after unsubscribe");
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
            Check(!await Sync.LockItemWithHolder(new Item()), "Subscriber fault changed the result");
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
        Entity root = new() { Id = new ident(91) };
        List<Entity> source = new() { root };
        List<Critter> critters = new() { new Critter { MapIdOverride = new ident(7) } };
        Action<Sync.FailureInfo> mutateSource =
            _ =>
        {
            root.IsDestroyed = true;
            source.Clear();
            critters.Clear();
        };
        List<Sync.FailureInfo> later = new();
        Sync.OnFailure += mutateSource;
        Sync.OnFailure += later.Add;

        try {
            Check(!await Sync.LockCrittersInitialInfoGraphs(source, critters), "Unstable graph accepted");
            Sync.FailureInfo report = ReadSingle("retry_exhausted");
            Check(later.Count == 1 && ReferenceEquals(later[0], report), "Subscribers did not share a snapshot");
            Check(source.Count == 0 && critters.Count == 0 && root.IsDestroyed, "Source mutation did not run");
            Check(report.Entities.Count == 2, "Source collection mutation changed snapshot");
            Check(report.Entities[0].Id == new ident(91) && !report.Entities[0].IsDestroyed,
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
