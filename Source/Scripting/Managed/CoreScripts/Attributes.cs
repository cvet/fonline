#nullable enable

using System;

namespace FOnline
{
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class ModuleInitAttribute : Attribute
    {
        public ModuleInitAttribute(int priority = 0)
        {
            Priority = priority;
        }

        public int Priority { get; private set; }
    }

    [AttributeUsage(AttributeTargets.Method)]
    public sealed class EventAttribute : Attribute
    {
    }

    // Marks a static parameterless method that registers attributed script functions into the engine's
    // cross-backend function registry (ScriptFuncRegistration.RegisterAttributedScriptFuncs with
    // project-supplied attribute types, e.g. dialog demand/result markers). Registrars run in the
    // registration phase (InitializeEarly, right after the engine attribute funcs), NOT at [ModuleInit]
    // time: registration must also happen inside bake-time validation engines, which load the compiled
    // assembly to restore the script subsystem for reflection (ScriptSystem::FindFunc) but never run
    // game module initialization.
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class ScriptFuncRegistrarAttribute : Attribute
    {
    }

    // Marks a coroutine-style method (one that suspends via Game.YieldAsync). Ported [[Async]]
    // AngelScript functions become `async Task` C# methods; this attribute documents that contract
    // and lets reflection/codegen identify async entry points.
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class AsyncAttribute : Attribute
    {
    }

    // Callback / codegen marker attributes mirroring the AngelScript `[[...]]` markers
    // (AngelScriptAttributes.cpp). They tag methods (or classes) for the managed runtime/baker to wire up —
    // time events, property getters/setters, remote calls, item triggers, etc. They are markers (no arguments);
    // AttributeTargets.All keeps them permissive across the targets they decorate. Markers for project-specific
    // extensions live in the embedding project, not here.
    [AttributeUsage(AttributeTargets.All)]
    public sealed class TimeEventAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class PropertyGetterAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class PropertySetterAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class ServerRemoteCallAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class ClientRemoteCallAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class AdminRemoteCallAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class ItemTriggerAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class ItemInitAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class ItemStaticAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class CritterInitAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class MapInitAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class LocationInitAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class ClassExtensionAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.All)]
    public sealed class AnimCallbackAttribute : Attribute
    {
    }

    // Entities reachable from an annotated one that share its cover requirement. Element-wise application
    // over a collection parameter needs no syntax at all -- it follows from the parameter's own type.
    //
    // The steps are named by RELATION, never by concrete type. The legacy contracts spelled them as
    // `.map` (critter), `.holder` (item) and `.location` (map), but those are the same relation at
    // different levels: the engine's own sync hierarchy parent (ServerEntity::_parent, which the lock
    // machinery walks). Naming it `Map` would be wrong the moment the annotated entity is an item, whose
    // parent may be a critter, a map or a containing item.
    [Flags]
    public enum CoverReach
    {
        None = 0,

        // The immediate parent in the sync hierarchy: a critter's map, an item's holder, a map's location.
        Parent = 1 << 0,

        // The whole parent chain, not just one step -- the legacy `cr.map.location` shape.
        Ancestors = 1 << 1,

        // Not a navigation step but a closure: everything the entity's destruction would cascade through.
        DestroyGraph = 1 << 2,

        // There is deliberately NO ControlledCritter / OwningPlayer member. Critter and Player are linked
        // through Critter::_player / Player::_controlledCr rather than the parent chain, and `EntitySync`
        // widens every acquisition across that link symmetrically via `GetSyncWidenEntity`, iterated to a
        // fixed point -- so holding either half always covers the other. Declaring it would state a
        // requirement the engine already guarantees.
        //
        // The parent direction is the opposite case and is why `Parent`/`Ancestors` exist: acquisition locks
        // "EXACTLY the requested entities plus each one's sync-widen partner, and NOTHING else"
        // (Server/EntitySync.cpp). Holding a map covers the critters beneath it, but holding a critter does
        // not reach up to its map -- sibling-to-parent escalation and parent-cover reduction were both
        // removed deliberately.
    }

    // Declares that the CALLER must already hold synchronization cover for this entity when it calls the
    // method -- the method reads or mutates it and acquires nothing itself.
    //
    // It is PURELY STATIC: it changes no runtime behavior and exists so the compiler can prove the obligation
    // is met. The cover itself comes from one of two places. At an execution-context entry point the engine
    // has already synchronized the subject it dispatches on -- a remote call's Player, an event's own entity
    // -- so those parameters are annotated to state that guarantee, not to request anything. Everywhere else
    // the code establishes what it needs with an explicit Sync call; the attribute never locks anything.
    //
    // It sits on the parameter rather than naming it from the method, so the contract cannot drift from the
    // signature: there is no name to get wrong and a rename carries the annotation with it. That is the
    // failure mode of the `// SyncScope:` comments this replaces, several of which were found already
    // detached from their code.
    //
    // The obligation is transitive. A method that calls one of these either acquires the cover itself
    // (Sync.Lock / Sync.Widen*) or re-declares it on its own parameter and passes it to ITS caller; an entry
    // point discharges it by carrying the annotation for what the engine already covers.
    // FOSYNC001/FOSYNC002 check both halves at compile time; the model mirrors the FO_TSA_* Clang Thread
    // Safety annotations used on the native side (Engine/Docs/ThreadSafetyAnalysis.md).
    // On a METHOD it names the receiver instead of a parameter: `cr.SendGlobalMapGroupInfo()` needs `cr`
    // covered, and the receiver is not in the parameter list. That is the dominant shape among the engine's
    // own exports -- 44 of the 74 native cover contracts are exactly "the receiver" -- so it is the form
    // codegen emits for a server entity export by default.
    [AttributeUsage(AttributeTargets.Parameter | AttributeTargets.Method, AllowMultiple = false)]
    public sealed class RequiresCoverAttribute : Attribute
    {
        public RequiresCoverAttribute(CoverReach reach = CoverReach.None)
        {
            Reach = reach;
        }

        public CoverReach Reach { get; private set; }
    }

    // For a method that ESTABLISHES cover for the annotated parameter or return value mid-flight, so its
    // caller may pass that value to a [RequiresCover] position without acquiring anything else. Fixture and
    // resolution helpers that lock what they hand back are the common case; without this half every such
    // helper's callers read as violations. Distinct from [SyncCover]: that one is a request to the
    // dispatcher before a context starts, this one is a statement about what a call returns.
    [AttributeUsage(AttributeTargets.Parameter | AttributeTargets.ReturnValue, AllowMultiple = false)]
    public sealed class ProvidesCoverAttribute : Attribute
    {
        public ProvidesCoverAttribute(CoverReach reach = CoverReach.None)
        {
            Reach = reach;
        }

        public CoverReach Reach { get; private set; }
    }

    // For an awaitable method that GIVES THE CALLER BACK the cover it had. An await ordinarily releases the
    // caller cover, so a value covered before one is not covered after it -- except when the callee restores
    // what it found. That distinction already existed as prose ("preserves the caller cover" versus "replace
    // or widen the caller's cover") in the `// SyncScope:` comments the cover attributes replaced, and it is
    // the half that was never made checkable. Without it every caller of a preserving helper reads as having
    // lost cover it still holds.
    //
    // It says nothing about lifetime: the entity may have been destroyed while the callee ran, so a caller
    // that keeps using it still owes the ordinary liveness check.
    [AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
    public sealed class PreservesCoverAttribute : Attribute
    {
    }
}
