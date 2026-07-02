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
}
