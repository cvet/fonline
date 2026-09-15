namespace FOnline.Analyzers.Tests;

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Diagnostics;

// Self-tests for the managed script analyzers. Each case compiles a snippet against a minimal stand-in for
// the pieces of the script surface the analyzer reasons about, runs the analyzer over it, and compares the
// reported diagnostic ids against the expectation.
internal static class Program
{
    // The analyzer resolves the attributes, FOnline.Entity and the engine-owned FOnline.Sync by metadata
    // name, so a small stand-in keeps the tests independent of the real CoreScripts build.
    private const string Preamble = @"
namespace FOnline
{
    [System.Flags]
    public enum CoverReach { None = 0, Parent = 1, Ancestors = 2, DestroyGraph = 4 }

    [System.AttributeUsage(System.AttributeTargets.Parameter | System.AttributeTargets.Method)]
    public sealed class RequiresCoverAttribute : System.Attribute
    {
        public RequiresCoverAttribute(CoverReach reach = CoverReach.None) { Reach = reach; }
        public CoverReach Reach { get; private set; }
    }


    [System.AttributeUsage(System.AttributeTargets.Parameter | System.AttributeTargets.ReturnValue)]
    public sealed class ProvidesCoverAttribute : System.Attribute
    {
        public ProvidesCoverAttribute(CoverReach reach = CoverReach.None) { Reach = reach; }
        public CoverReach Reach { get; private set; }
    }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class EventAttribute : System.Attribute { }

    // The baker puts this on the base and overrides it on the generated prototype/static classes; the analyzer
    // reads the override rather than a list of type names.
    public class Entity
    {
        public virtual bool IsAlwaysCovered { get { return false; } }
    }
    [System.AttributeUsage(System.AttributeTargets.ReturnValue)]
    public sealed class ReturnsParentAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class AcquiresCoverAttribute : System.Attribute { }

    [System.AttributeUsage(System.AttributeTargets.Parameter)]
    public sealed class PassesCoverAttribute : System.Attribute { }

    public enum CoverEffectKind
    {
        Replace,
        Extend,
        Restore,
        Snapshot,
        Release,
    }

    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class CoverEffectAttribute : System.Attribute
    {
        public CoverEffectAttribute(CoverEffectKind effect) { Effect = effect; }

        public CoverEffectKind Effect { get; }
    }

    [System.AttributeUsage(System.AttributeTargets.ReturnValue)]
    public sealed class ReturnsAncestorAttribute : System.Attribute { }

    public class Critter : Entity
    {
        [RequiresCover]
        public void SendGroupInfo() { }

        public void Untracked() { }

        [return: ReturnsParent]
        public Map? GetMap() { return null; }
    }
    public class Map : Entity
    {
        [return: ReturnsParent]
        public Location GetLocation() { return null; }
    }
    public class Location : Entity { }

    // Item methods are declared once on the shared base and inherited by both, which is what makes the
    // static side worth modelling explicitly.
    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class PreservesCoverAttribute : System.Attribute
    {
    }

    public class AbstractItem : Entity
    {
        [RequiresCover]
        public int GetCount() { return 0; }

        public void Give([RequiresCover] Critter cr) { }

        public static void Compare([RequiresCover] AbstractItem other) { }
    }

    public class Item : AbstractItem
    {
        [return: ReturnsAncestor]
        public Map? GetMap() { return null; }
    }
    public class StaticItem : AbstractItem
    {
        public override bool IsAlwaysCovered { get { return true; } }
    }
    public class ProtoItem : AbstractItem
    {
        public override bool IsAlwaysCovered { get { return true; } }
    }

    public class ProtoCritter : Critter
    {
        public override bool IsAlwaysCovered { get { return true; } }
    }

    // Stands in for the engine exports that hand back a covered set, such as Map.GetCrittersInRadius: the
    // acquisition covered the map, and the map's cover reaches the critters on it.
    public static class Roster
    {
        [return: ProvidesCover]
        public static System.Collections.Generic.List<Critter> Nearby(Map map) { return null; }
    }

    public static partial class Sync
    {
        [CoverEffect(CoverEffectKind.Extend)]
        public static System.Threading.Tasks.Task<bool> Widen(Entity entity) { return System.Threading.Tasks.Task.FromResult(true); }
        [CoverEffect(CoverEffectKind.Extend)]
        public static System.Threading.Tasks.Task<bool> Widen(System.Collections.Generic.List<Entity> entities) { return System.Threading.Tasks.Task.FromResult(true); }
        [CoverEffect(CoverEffectKind.Replace)]
        public static bool Lock(Entity entity) { return true; }
        [CoverEffect(CoverEffectKind.Extend)]
        public static bool WidenCritterWithMap(Critter cr) { return true; }
        public static bool IsCovered(Entity entity) { return true; }
        [CoverEffect(CoverEffectKind.Replace)]
        public static System.Threading.Tasks.Task<bool> LockAsync(Entity entity) { return System.Threading.Tasks.Task.FromResult(true); }
        [CoverEffect(CoverEffectKind.Snapshot)]
        public static System.Collections.Generic.List<Entity> Snapshot() { return new System.Collections.Generic.List<Entity>(); }
        [CoverEffect(CoverEffectKind.Restore)]
        public static System.Threading.Tasks.Task<bool> Restore(System.Collections.Generic.List<Entity> entities) { return System.Threading.Tasks.Task.FromResult(true); }
        [CoverEffect(CoverEffectKind.Release)]
        public static void Release() { }
        // An acquisition whose name says nothing, and a name that says everything with no declaration behind it
        [CoverEffect(CoverEffectKind.Extend)]
        public static System.Threading.Tasks.Task<bool> Grab(Entity entity) { return System.Threading.Tasks.Task.FromResult(true); }
        public static System.Threading.Tasks.Task<bool> WidenLookalike(Entity entity) { return System.Threading.Tasks.Task.FromResult(true); }
    }

    public static class Game
    {
        public static bool IsEntityLocked(Entity entity) { return true; }
        public static bool TrySyncEntity(int id) { return true; }
        public static void Sync(Entity entity) { }

        // The singleton bucket lock, reserved for the GameLock scope below
        public static void Lock() { }
        public static void Unlock() { }

        // The rest of the surface, which shares the type but takes entities as ordinary arguments.
        public static bool CallStaticItemFunction(Critter? cr, StaticItem staticItem, Item? usedItem, string param) { return true; }
    }

    // The scope that owns the raw pair; its own calls are the implementation
    public readonly ref struct GameLock
    {
        public static GameLock Acquire() { Game.Lock(); return new GameLock(); }
        public void Dispose() { Game.Unlock(); }
    }
}

namespace LastFrontier
{
    using FOnline;

    // A same-named project type must NOT be able to discharge an engine cover obligation.
    public static class Sync2
    {
        public static bool Lock(Entity entity) { return true; }
    }
}
";

    private static int Main()
    {
        var failures = new List<string>();

        Check(failures, "annotation on an entity parameter is silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }
    }
}");

        Check(failures, "reach flags on an entity parameter are silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover(CoverReach.Parent | CoverReach.Ancestors)] Critter cr) { }
    }
}");

        Check(failures, "a collection of entities carries the contract element-wise", @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    public static class Probe
    {
        public static void Reads([RequiresCover] List<Critter> crs) { }
    }
}");

        Check(failures,
              "annotation on a non-entity is reported",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] int hp) { }
    }
}",
              "FOSYNC001");

        Check(failures,
              "ProvidesCover on a non-entity return is reported",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [return: ProvidesCover]
        public static int Make() { return 0; }
    }
}",
              "FOSYNC001");

        Check(failures,
              "undischarged call is reported",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Caller(Critter cr) { Reads(cr); }
    }
}",
              "FOSYNC002");

        // Nothing to cover: an explicit null, a default, and an omitted optional entity parameter
        Check(failures, "a null or omitted entity argument owes no cover", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(int value, [RequiresCover] Critter? cr = null) { }

        public static void Caller()
        {
            Reads(1, null);
            Reads(2, default);
            Reads(3);
        }
    }
}");

        // A target whose `Sync` has no acquisition helpers (the client and mapper builds) owes nothing
        CheckWithPreamble(failures,
                          "no acquisition helpers means no obligation",
                          Preamble.Replace("[CoverEffect(CoverEffectKind.Replace)]", "")
                              .Replace("[CoverEffect(CoverEffectKind.Extend)]", "")
                              .Replace("[CoverEffect(CoverEffectKind.Restore)]", ""),
                          @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Caller(Critter cr) { Reads(cr); }
    }
}");

        // Upward accessors: the receiver's own cover does not reach its parent, declared reach does
        Check(failures,
              "a parent reached through declared reach is covered, one step past it is not",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [return: ProvidesCover(CoverReach.Parent)]
        public static Critter WithMap() { return null; }

        [return: ProvidesCover]
        public static Critter Alone() { return null; }

        public static void NeedsMap([RequiresCover] Map? map) { }
        public static void NeedsLocation([RequiresCover] Location loc) { }

        public static void Caller()
        {
            Critter cr = WithMap();
            NeedsMap(cr.GetMap());
            Map? map = cr.GetMap();
            NeedsMap(map);
            NeedsLocation(map.GetLocation());
            NeedsMap(Alone().GetMap());
        }
    }
}",
              "FOSYNC002",
              "FOSYNC002");

        Check(failures,
              "ancestors reach covers the whole chain, including an ancestor accessor",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [return: ProvidesCover(CoverReach.Ancestors)]
        public static Critter WithChain() { return null; }

        [return: ProvidesCover(CoverReach.Parent)]
        public static Item HeldItem() { return null; }

        [return: ProvidesCover(CoverReach.Ancestors)]
        public static Item HeldItemWithChain() { return null; }

        public static void NeedsMap([RequiresCover] Map? map) { }
        public static void NeedsLocation([RequiresCover] Location loc) { }

        public static void Caller()
        {
            Critter cr = WithChain();
            NeedsLocation(cr.GetMap().GetLocation());
            NeedsMap(HeldItemWithChain().GetMap());
            NeedsMap(HeldItem().GetMap());
        }
    }
}",
              "FOSYNC002");

        Check(failures,
              "a value handed to a providing parameter carries that parameter's reach",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static bool WidenWithMap([ProvidesCover(CoverReach.Parent)] Critter cr) { return true; }
        public static bool Widen([ProvidesCover] Critter cr) { return true; }

        public static void NeedsMap([RequiresCover] Map? map) { }

        public static void Reached(Critter cr)
        {
            if (WidenWithMap(cr)) { NeedsMap(cr.GetMap()); }
        }

        public static void NotReached(Critter cr)
        {
            Critter other = cr;
            if (Widen(other)) { NeedsMap(other.GetMap()); }
        }
    }
}",
              "FOSYNC002");

        Check(failures,
              "a deconstructed entity carries the cover its provider declares",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [return: ProvidesCover]
        public static (Critter? Found, bool Loaded) Resolve() { return (null, false); }

        public static (Critter? Found, bool Loaded) Guess() { return (null, false); }

        public static void Reads([RequiresCover] Critter? cr) { }

        public static void Caller()
        {
            (Critter? resolved, bool loaded) = Resolve();
            Reads(resolved);
            var (guessed, _) = Guess();
            Reads(guessed);
        }
    }
}",
              "FOSYNC002");

        Check(failures,
              "a choice between provided values keeps the reach every branch shares",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [return: ProvidesCover(CoverReach.Parent)]
        public static Critter WithMap() { return null; }

        [return: ProvidesCover]
        public static Critter Alone() { return null; }

        public static Critter Unknown() { return null; }

        public static void NeedsCritter([RequiresCover] Critter? cr) { }
        public static void NeedsMap([RequiresCover] Map? map) { }

        public static void Caller(bool ok)
        {
            Critter? maybe = ok ? (WithMap()) : null;
            NeedsMap(maybe.GetMap());
            Critter either = ok ? WithMap() : Alone();
            NeedsCritter(either);
            NeedsMap(either.GetMap());
            NeedsCritter(ok ? Unknown() : null);
        }
    }
}",
              "FOSYNC002",
              "FOSYNC002");

        Check(failures,
              "a pass-through returns its argument's cover and reach",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [return: ProvidesCover(CoverReach.Parent)]
        public static Critter? WithMap() { return null; }

        public static Critter? Unknown() { return null; }

        public static T Check<T>([PassesCover] T? value, string message) where T : class { return value; }
        public static T Plain<T>(T? value, string message) where T : class { return value; }

        public static void NeedsCritter([RequiresCover] Critter? cr) { }
        public static void NeedsMap([RequiresCover] Map? map) { }

        public static void Caller()
        {
            Critter cr = Check(WithMap(), ""covered"");
            NeedsMap(cr.GetMap());
            NeedsCritter(Check(Unknown(), ""uncovered""));
            NeedsCritter(Plain(WithMap(), ""no pass-through""));
        }
    }
}",
              "FOSYNC002",
              "FOSYNC002");

        Check(failures,
              "a helper declared as an acquisition discharges the obligation like Sync itself",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [AcquiresCover]
        public static bool CoverMembers() { return true; }

        public static bool NotAnAcquisition() { return true; }

        public static void Reads([RequiresCover] Critter cr) { }

        public static void Declared(Critter cr)
        {
            if (CoverMembers()) { Reads(cr); }
        }

        public static void Undeclared(Critter cr)
        {
            if (NotAnAcquisition()) { Reads(cr); }
        }
    }
}",
              "FOSYNC002");

        Check(failures, "acquiring cover discharges the obligation", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Caller(Critter cr)
        {
            Sync.Lock(cr);
            Reads(cr);
        }
    }
}");

        Check(failures, "propagating the obligation discharges it", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Caller([RequiresCover] Critter cr) { Reads(cr); }
    }
}");

        Check(failures, "a ProvidesCover return discharges through a local", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        [return: ProvidesCover(CoverReach.Parent)]
        public static Critter Spawn() { return null!; }

        public static void Caller()
        {
            Critter spawned = Spawn();
            Reads(spawned);
        }
    }
}");

        Check(failures, "a ProvidesCover call discharges inline", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        [return: ProvidesCover]
        public static Critter Spawn() { return null!; }

        public static void Caller() { Reads(Spawn()); }
    }
}");

        Check(failures,
              "an unprovided local is still reported",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static Critter Make() { return null!; }

        public static void Caller()
        {
            Critter made = Make();
            Reads(made);
        }
    }
}",
              "FOSYNC002");

        Check(failures,
              "only the demanding argument is reported",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr, Critter other) { }

        public static void Caller(Critter a, Critter b) { Reads(a, b); }
    }
}",
              "FOSYNC002");

        Check(failures,
              "each demanding argument is reported separately",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr, [RequiresCover] Critter killer) { }

        public static void Caller(Critter a, Critter b) { Reads(a, b); }
    }
}",
              "FOSYNC002",
              "FOSYNC002");

        Check(failures,
              "a look-alike project type does not discharge",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Caller(Critter cr)
        {
            Sync2.Lock(cr);
            Reads(cr);
        }
    }
}",
              "FOSYNC002");

        Check(failures, "an unannotated callee is not policed", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Critter cr) { }

        public static void Caller(Critter cr) { Reads(cr); }
    }
}");

        Check(failures,
              "a non-entry caller with the same shape is still reported",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void PlainHelper(Critter cr) { Reads(cr); }
    }
}",
              "FOSYNC002");

        Check(failures, "a ProvidesCover parameter discharges that argument", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Establish([ProvidesCover] Critter cr) { }

        public static void Caller(Critter cr)
        {
            Establish(cr);
            Reads(cr);
        }
    }
}");

        Check(failures, "an awaited ProvidesCover return discharges through a local", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        [return: ProvidesCover]
        public static Task<Critter> SpawnAsync() { return null!; }

        public static async Task Caller()
        {
            Critter spawned = await SpawnAsync();
            Reads(spawned);
        }
    }
}");

        Check(failures,
              "a ProvidesCover call on a different value does not discharge",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        public static void Establish([ProvidesCover] Critter cr) { }

        public static void Caller(Critter a, Critter b)
        {
            Establish(b);
            Reads(a);
        }
    }
}",
              "FOSYNC002");

        Check(failures,
              "an entry point must declare the cover the engine gives it",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [Event]
        public static void OnSomething(Critter cr) { }
    }
}",
              "FOSYNC003");

        Check(failures,
              "probing whether cover is held is reported",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Critter cr)
        {
            if (Sync.IsCovered(cr)) { }
        }
    }
}",
              "FOSYNC004");

        Check(failures,
              "the engine probe is reported the same way",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Critter cr)
        {
            if (Game.IsEntityLocked(cr)) { }
        }
    }
}",
              "FOSYNC004");

        Check(failures,
              "a raw cover primitive outside Sync is reported",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Critter cr) { Game.Sync(cr); }
    }
}",
              "FOSYNC005");

        Check(failures,
              "the raw singleton lock pair outside GameLock is reported",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads()
        {
            Game.Lock();
            Game.Unlock();
        }
    }
}",
              "FOSYNC005",
              "FOSYNC005");

        Check(failures, "Sync itself may probe and use the primitives", @"
namespace FOnline
{
    public static partial class Sync
    {
        public static void Helper(Critter cr)
        {
            if (IsCovered(cr)) { }
            Game.Sync(cr);
        }
    }
}");

        Check(failures, "resolving an id to a live entity is not a raw primitive", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Work(int playerId)
        {
            if (!Game.TrySyncEntity(playerId)) { }
        }
    }
}");

        // FOSYNC009 -- the value-aware half: cover that an await released and nothing took back.
        Check(failures,
              "a value used after an await with no re-proof is reported",
              @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        public static async Task Run([RequiresCover] Critter cr)
        {
            await Pause();
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}",
              "FOSYNC009");

        Check(failures, "re-proving the value after the await clears it", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        public static async Task Run([RequiresCover] Critter cr)
        {
            await Pause();
            if (!await Sync.Widen(cr)) { return; }
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

        Check(failures, "awaiting a preserving callee releases nothing", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        [PreservesCover]
        static Task Keeps() { return Task.CompletedTask; }

        public static async Task Run([RequiresCover] Critter cr)
        {
            await Keeps();
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

        Check(failures,
              "a callee that only widens is proved preserving without the annotation",
              @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    using System.Threading.Tasks;
    public static class Probe
    {
        static async Task<bool> WidensOnly(Critter cr) { return await Sync.Widen(cr); }

        static async Task<bool> WidensThroughHelper(Critter cr) { return await WidensOnly(cr); }

        static async Task<bool> Locks(Critter cr) { return await Sync.LockAsync(cr); }

        static async Task<bool> KeepsItsOwnSnapshot(Critter cr)
        {
            List<Entity> cover = Sync.Snapshot();
            return await Sync.Restore(cover);
        }

        public static async Task Widened([RequiresCover] Critter cr, Critter other)
        {
            await WidensOnly(other);
            Needs(cr);
        }

        public static async Task Chained([RequiresCover] Critter cr, Critter other)
        {
            await WidensThroughHelper(other);
            Needs(cr);
        }

        public static async Task Restored([RequiresCover] Critter cr, Critter other)
        {
            await KeepsItsOwnSnapshot(other);
            Needs(cr);
        }

        public static async Task Locked([RequiresCover] Critter cr, Critter other)
        {
            await Locks(other);
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}",
              "FOSYNC009");

        Check(failures,
              "a callee that acquires for its parameter re-proves the value it was handed",
              @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        static async Task<bool> Covers(Critter cr)
        {
            if (!await Sync.LockAsync(cr)) { return false; }

            return true;
        }

        static async Task<bool> CoversThroughList(Critter cr, Map map)
        {
            List<Entity> roots = new List<Entity> { cr, map };
            return await Sync.Widen(roots);
        }

        static async Task<bool> CoversOnlySometimes(Critter cr, bool transport)
        {
            if (transport) { return await Sync.LockAsync(cr); }

            return true;
        }

        static async Task<bool> CoversThenLetsGo(Critter cr, Critter other)
        {
            bool covered = await Sync.LockAsync(cr);
            return covered && await Sync.LockAsync(other);
        }

        public static async Task Reacquired([RequiresCover] Critter cr)
        {
            await Pause();
            await Covers(cr);
            Needs(cr);
        }

        public static async Task ReacquiredInList([RequiresCover] Critter cr, Map map)
        {
            await Pause();
            await CoversThroughList(cr, map);
            Needs(cr);
        }

        public static async Task Conditional([RequiresCover] Critter cr)
        {
            await Pause();
            await CoversOnlySometimes(cr, true);
            Needs(cr);
        }

        public static async Task Dropped([RequiresCover] Critter cr, Critter other)
        {
            await Pause();
            await CoversThenLetsGo(cr, other);
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}",
              "FOSYNC009",
              "FOSYNC009");

        Check(failures,
              "a re-proof may name the value through a list, or restore the snapshot that held it",
              @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        public static async Task ThroughList([RequiresCover] Critter cr, Map map)
        {
            await Pause();
            List<Entity> roots = new List<Entity> { cr };
            roots.Add(map);
            if (!await Sync.Widen(roots)) { return; }
            Needs(cr);
            NeedsMap(map);
        }

        public static async Task ThroughRestoredSnapshot([RequiresCover] Critter cr)
        {
            List<Entity> cover = Sync.Snapshot();
            await Pause();
            if (!await Sync.Restore(cover)) { return; }
            Needs(cr);
        }

        public static async Task SnapshotTakenTooLate([RequiresCover] Critter cr)
        {
            await Pause();
            List<Entity> cover = Sync.Snapshot();
            if (!await Sync.Restore(cover)) { return; }
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }

        static void NeedsMap([RequiresCover] Map map) { }
    }
}",
              "FOSYNC009");

        Check(failures,
              "restoring the snapshot in the next statement keeps the body preserving",
              @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        static async Task<bool> RepairsRightAfter(Critter cr)
        {
            List<Entity> cover = Sync.Snapshot();
            await Pause();
            if (!await Sync.Restore(cover)) { return false; }

            return true;
        }

        static async Task<bool> RepairsTooLate(Critter cr)
        {
            List<Entity> cover = Sync.Snapshot();
            await Pause();
            await Pause();
            return await Sync.Restore(cover);
        }

        public static async Task Repaired([RequiresCover] Critter cr, Critter other)
        {
            await RepairsRightAfter(other);
            Needs(cr);
        }

        public static async Task NotRepaired([RequiresCover] Critter cr, Critter other)
        {
            await RepairsTooLate(other);
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}",
              "FOSYNC009");

        Check(failures,
              "a local the body re-reads after the await is fresh",
              @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        public static async Task Rebound([RequiresCover] Critter cr)
        {
            Map? map = cr.GetMap();
            await Pause();
            map = cr.GetMap();
            NeedsMap(map);
        }

        public static async Task ReboundInBranch([RequiresCover] Critter cr, bool flag)
        {
            Map? map = cr.GetMap();
            await Pause();
            if (flag) { map = cr.GetMap(); }
            NeedsMap(map);
        }

        static void NeedsMap([RequiresCover] Map? map) { }
    }
}",
              "FOSYNC009");

        Check(failures,
              "what a call does to the cover is read from its declaration, not from its name",
              @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static async Task<bool> ExtendsUnderAnyName(Critter cr) { return await Sync.Grab(cr); }

        static async Task<bool> NamedLikeAWidenButUndeclared(Critter cr) { return await Sync.WidenLookalike(cr); }

        public static async Task Declared([RequiresCover] Critter cr, Critter other)
        {
            await ExtendsUnderAnyName(other);
            Needs(cr);
        }

        public static async Task NamedOnly([RequiresCover] Critter cr, Critter other)
        {
            await NamedLikeAWidenButUndeclared(other);
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}",
              "FOSYNC009");

        Check(failures,
              "a body that releases the cover outright is not preserving",
              @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static async Task<bool> WidensThenReleases(Critter cr)
        {
            bool widened = await Sync.Widen(cr);
            Sync.Release();
            return widened;
        }

        public static async Task Run([RequiresCover] Critter cr, Critter other)
        {
            await WidensThenReleases(other);
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}",
              "FOSYNC009");

        // Source order is not execution order; these two are what the position-only version got wrong.
        Check(failures, "an await in a sibling branch does not reach the other branch", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        public static async Task Run([RequiresCover] Critter cr, bool flag)
        {
            if (flag) { await Pause(); }
            else { Needs(cr); }
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

        Check(failures, "an await in a guard branch that returns does not reach past it", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task Pause() { return Task.CompletedTask; }

        public static async Task Run([RequiresCover] Critter cr, bool bad)
        {
            if (bad) { await Pause(); return; }
            Needs(cr);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

        Check(failures, "a value the await itself produced is fresh", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        [return: ProvidesCover]
        static Task<Critter> Find() { return Task.FromResult<Critter>(null); }

        public static async Task Run()
        {
            Critter found = await Find();
            Needs(found);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

        // Both deconstruction spellings declare through a designation that ends before the await
        Check(failures, "a value a deconstructing await produced is fresh", @"
namespace LastFrontier
{
    using FOnline;
    using System.Threading.Tasks;
    public static class Probe
    {
        static Task<(Critter Found, bool Loaded)> Find() { return Task.FromResult<(Critter, bool)>((null, false)); }

        public static async Task Run([RequiresCover] Critter cr)
        {
            (Critter found, bool loaded) = await Find();
            Needs(found);
            var (other, reloaded) = await Find();
            Needs(other);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

        // A covered collection covers what is taken out of it -- the acquisition reached the elements too.
        Check(failures, "an element read by index carries the collection cover", @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    public static class Probe
    {
        public static void Scan(Map map)
        {
            List<Critter> found = Roster.Nearby(map);
            Critter other = found[0];
            Needs(other);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

        Check(failures, "a foreach variable carries it too", @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    public static class Probe
    {
        public static void Scan(Map map)
        {
            foreach (Critter other in Roster.Nearby(map)) { Needs(other); }
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

        Check(failures, "a collection parameter declaring it passes it on", @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    public static class Probe
    {
        public static void Scan([ProvidesCover] List<Critter> critters)
        {
            Needs(critters[0]);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}");

        Check(failures,
              "an element of an undeclared collection is still reported",
              @"
namespace LastFrontier
{
    using FOnline;
    using System.Collections.Generic;
    public static class Probe
    {
        public static void Scan(List<Critter> critters)
        {
            Needs(critters[0]);
        }

        static void Needs([RequiresCover] Critter cr) { }
    }
}",
              "FOSYNC002");

        Check(failures, "static map data needs no cover", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(StaticItem item) { item.GetCount(); }
    }
}");

        Check(failures,
              "the same call on a mutable item still needs cover",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Item item) { item.GetCount(); }
    }
}",
              "FOSYNC002");

        Check(failures, "annotating always-covered data is legal -- it is still an entity", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] StaticItem item) { }
    }
}");

        // The reason the exemption belongs to the value and not to the type system: a prototype derives
        // from the concrete entity, so excluding it from "entity" would drop the contract on the upcast.
        Check(failures, "a prototype satisfies the obligation on its own", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(ProtoCritter proto) { Needs(proto); }
        static void Needs([RequiresCover] Critter cr) { }
    }
}");

        Check(failures,
              "the same parameter still demands cover for a live critter",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads(Critter cr) { Needs(cr); }
        static void Needs([RequiresCover] Critter cr) { }
    }
}",
              "FOSYNC002");

        Check(failures, "acquiring cover on always-covered data is legal and silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Takes(StaticItem item) { Sync.Lock(item); }
    }
}");

        Check(failures, "static map data as an argument needs no cover either", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Compares(StaticItem item) { AbstractItem.Compare(item); }
    }
}");

        Check(failures,
              "a mutable item in the same position still does",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Compares(Item item) { AbstractItem.Compare(item); }
    }
}",
              "FOSYNC002");

        // Exempting the receiver must not exempt the call.
        Check(failures,
              "an argument is still checked on a static-data receiver",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Gives(StaticItem item, Critter cr) { item.Give(cr); }
    }
}",
              "FOSYNC002");

        // Game carries the whole script surface, so the rule must be scoped to the acquisition methods.
        Check(failures, "static map data as an ordinary Game argument is not an acquisition", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Uses(StaticItem item) { Game.CallStaticItemFunction(null, item, null, """"); }
    }
}");

        Check(failures, "the GameLock scope is silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static int Work(bool skip)
        {
            using GameLock scope = GameLock.Acquire();

            if (skip) {
                return 0;
            }

            return 1;
        }
    }
}");

        Check(failures,
              "a project type named GameLock does not own the raw pair",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class GameLock
    {
        public static void Take() { Game.Lock(); }
    }
}",
              "FOSYNC005");

        Check(failures, "a declaring entry point is silent and discharges its callees", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Reads([RequiresCover] Critter cr) { }

        [Event]
        public static void OnSomething([RequiresCover] Critter cr) { Reads(cr); }
    }
}");

        Check(failures, "only the first entity parameter of an entry point is required to declare", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [Event]
        public static void OnSomething([RequiresCover] Critter cr, Critter other) { }
    }
}");

        Check(failures, "a non-entry method needs no entry-point declaration", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Helper(Critter cr) { }
    }
}");

        Check(failures,
              "an uncovered receiver is reported",
              @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Caller(Critter cr) { cr.SendGroupInfo(); }
    }
}",
              "FOSYNC002");

        Check(failures, "a covered receiver is silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Caller([RequiresCover] Critter cr) { cr.SendGroupInfo(); }
    }
}");

        Check(failures, "a receiver from a ProvidesCover source is silent", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        [return: ProvidesCover]
        public static Critter Spawn() { return null!; }

        public static void Caller()
        {
            Critter spawned = Spawn();
            spawned.SendGroupInfo();
        }
    }
}");

        Check(failures, "a method without the attribute does not police its receiver", @"
namespace LastFrontier
{
    using FOnline;
    public static class Probe
    {
        public static void Caller(Critter cr) { cr.Untracked(); }
    }
}");

        foreach (string failure in failures) {
            Console.Error.WriteLine("FAIL: " + failure);
        }

        Console.Out.WriteLine(failures.Count == 0 ? "OK: analyzer self-tests passed"
                                                  : $"FAILED: {failures.Count} analyzer self-test case(s)");

        return failures.Count == 0 ? 0 : 1;
    }

    private static void Check(List<string> failures, string name, string snippet, params string[] expected)
    {
        CheckWithPreamble(failures, name, Preamble, snippet, expected);
    }

    private static void CheckWithPreamble(List<string> failures, string name, string preamble, string snippet,
                                          params string[] expected)
    {
        ImmutableArray<Diagnostic> reported = Run(preamble + snippet);
        string[] actual = reported.Select(d => d.Id).OrderBy(id => id, StringComparer.Ordinal).ToArray();
        string[] wanted = expected.OrderBy(id => id, StringComparer.Ordinal).ToArray();

        if (!actual.SequenceEqual(wanted)) {
            failures.Add($"{name}: expected [{string.Join(", ", wanted)}] but got [{string.Join(", ", actual)}]");
        }
    }

    private static ImmutableArray<Diagnostic> Run(string source)
    {
        var references = ((string?)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") ?? string.Empty)
            .Split(System.IO.Path.PathSeparator)
            .Where(path => path.Length != 0)
            .Select(path => (MetadataReference)MetadataReference.CreateFromFile(path))
            .ToList();

        CSharpCompilation compilation =
            CSharpCompilation.Create("AnalyzerSelfTest",
                                     new[] { CSharpSyntaxTree.ParseText(source) },
                                     references,
                                     new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));

        // A snippet that does not compile would make a diagnostic expectation meaningless.
        ImmutableArray<Diagnostic> compileErrors =
            compilation.GetDiagnostics().Where(d => d.Severity == DiagnosticSeverity.Error).ToImmutableArray();

        if (!compileErrors.IsEmpty) {
            throw new InvalidOperationException("analyzer self-test snippet does not compile: " + compileErrors[0]);
        }

        CompilationWithAnalyzers withAnalyzers =
            compilation.WithAnalyzers(ImmutableArray.Create<DiagnosticAnalyzer>(new SyncCoverAnalyzer()));

        return withAnalyzers.GetAnalyzerDiagnosticsAsync().GetAwaiter().GetResult();
    }
}
