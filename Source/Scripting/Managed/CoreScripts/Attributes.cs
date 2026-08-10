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

    // Declares that the CALLER must already hold synchronization cover for the named entities when it calls
    // this method -- the method reads or mutates them and does not acquire cover itself. Each name must be a
    // parameter of the annotated method, so write it as `nameof(cr)`: a renamed parameter then breaks the
    // build instead of silently detaching the contract from the code, which is exactly the failure mode of
    // the `// SyncScope:` comments this replaces.
    //
    // The obligation is transitive. A method that calls a [RequiresCover] method either acquires the cover
    // itself (Sync.Lock / Sync.Widen*) or re-declares the same obligation on its own signature and passes it
    // to ITS caller. FOSYNC001/FOSYNC002 check both halves at compile time; the model mirrors the FO_TSA_*
    // Clang Thread Safety annotations used on the native side (Engine/Docs/ThreadSafetyAnalysis.md).
    [AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
    public sealed class RequiresCoverAttribute : Attribute
    {
        public RequiresCoverAttribute(params string[] entities)
        {
            Entities = entities;
        }

        public string[] Entities { get; private set; }
    }
}
