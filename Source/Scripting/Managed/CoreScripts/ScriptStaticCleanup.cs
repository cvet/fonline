namespace FOnline;

using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text;

// Clears the embedding project's script statics when scripting shuts down. Script statics are roots for the life
// of the process - the script assembly load context is not collectible - so an entity wrapper reachable from one
// is never collected, never finalized, and the native entity reference behind it is never given back. Clearing
// the root is enough: the collection, closure, delegate or cache behind it becomes garbage on its own.
//
// It runs as managed code on purpose. The same walk through the embedding API - mono_class_vtable plus
// mono_field_static_set_value - writes into the static area behind the runtime's back, and the collector then
// crashes scanning that area as a registered root. Reflection goes through the runtime's own stores.
//
// The engine's own namespace is out of scope: it is not script state but the runtime plumbing the engine shuts
// down through its own steps, and nulling the continuation scheduler's queues from under it is fatal
internal static class ScriptStaticCleanup
{
    private static readonly List<Assembly> LoadedDynamicAssemblies = new List<Assembly>();

    // Code loaded after the bake keeps statics of its own, and they root entity wrappers exactly as script ones do
    internal static void TrackDynamicAssembly(Assembly assembly)
    {
        lock (LoadedDynamicAssemblies)
        {
            LoadedDynamicAssemblies.Add(assembly);
        }
    }

    [CallableByEngine]
    internal static string ClearScriptStatics()
    {
        List<string> cleared = new List<string>();
        List<string> unreachable = new List<string>();
        List<Assembly> assemblies = new List<Assembly> { typeof(ScriptStaticCleanup).Assembly };

        lock (LoadedDynamicAssemblies)
        {
            assemblies.AddRange(LoadedDynamicAssemblies);
        }

        foreach (Assembly assembly in assemblies) {
            foreach (Type type in GetTypes(assembly, unreachable)) {
                // A compiler-generated type holds closure caches and literal blobs, not script state, and its
                // init-only singletons would otherwise fill the report with names nobody can act on
                if (IsEngineNamespace(type.Namespace) || type.ContainsGenericParameters ||
                    type.IsDefined(typeof(CompilerGeneratedAttribute), false)) {
                    continue;
                }

                foreach (FieldInfo field in type.GetFields(BindingFlags.Static | BindingFlags.Public |
                                                           BindingFlags.NonPublic | BindingFlags.DeclaredOnly)) {
                    // One field that refuses to be read or written - a type initializer that fails this late, a
                    // field the runtime will not let go of - must not take the rest of the cleanup with it
                    try {
                        ClearField(type, field, cleared, unreachable);
                    }
                    catch (Exception ex) {
                        unreachable.Add(type.FullName + "::" + field.Name + "(" + ex.GetType().Name + ")");
                    }
                }
            }
        }

        StringBuilder report = new StringBuilder();
        report.Append(cleared.Count).Append(" cleared, ").Append(unreachable.Count).Append(" out of reach");

        // Names are a debug detail: an ordinary shutdown only needs the counts, while a run that is hunting a
        // reference wants to know which field it is
        if (EntityWrapperTracker.IsDeepTracking) {
            foreach (string name in cleared) {
                report.Append(' ').Append(name);
            }

            foreach (string name in unreachable) {
                report.Append(" !").Append(name);
            }
        }

        return report.ToString();
    }

    private static void ClearField(Type type, FieldInfo field, List<string> cleared, List<string> unreachable)
    {
        // Literals have no storage; reference-bearing value types and thread statics require script analysis
        if (field.IsLiteral || field.FieldType.IsValueType || field.IsDefined(typeof(ThreadStaticAttribute), false)) {
            return;
        }

        object? value = field.GetValue(null);

        if (value == null) {
            return;
        }

        string name = type.FullName + "::" + field.Name;

        // An init-only field cannot be reassigned, but emptying what it holds releases the same references, and
        // that is what the shutdown is after
        if (field.IsInitOnly) {
            if (ClearContents(value)) {
                cleared.Add(name);
            }
            else {
                unreachable.Add(name);
            }

            return;
        }

        field.SetValue(null, null);
        cleared.Add(name);
    }

    private static bool ClearContents(object value)
    {
        if (value is IList list && !list.IsReadOnly) {
            list.Clear();
            return true;
        }

        if (value is IDictionary dictionary && !dictionary.IsReadOnly) {
            dictionary.Clear();
            return true;
        }

        // Queues and stacks do not implement ICollection<T>; only their known collection APIs may run here
        if (value is Queue queue) {
            queue.Clear();
            return true;
        }

        if (value is Stack stack) {
            stack.Clear();
            return true;
        }

        for (Type? type = value.GetType(); type != null; type = type.BaseType) {
            if (type.IsGenericType && (type.GetGenericTypeDefinition() == typeof(Queue<>) ||
                                       type.GetGenericTypeDefinition() == typeof(Stack<>))) {
                MethodInfo clear = type.GetMethod("Clear", Type.EmptyTypes) ??
                                   throw new MissingMethodException(type.FullName, "Clear");
                clear.Invoke(value, null);
                return true;
            }
        }

        // A set may offer only ICollection<T>; do not invoke a same-named method of an unrelated type
        foreach (Type contract in value.GetType().GetInterfaces()) {
            if (!contract.IsGenericType || contract.GetGenericTypeDefinition() != typeof(ICollection<>)) {
                continue;
            }

            PropertyInfo? readOnly = contract.GetProperty("IsReadOnly");
            MethodInfo? clear = contract.GetMethod("Clear", Type.EmptyTypes);

            // A read-only view - the shape a collection expression assigned to IReadOnlyList lands in - answers
            // Clear with NotSupportedException, and asking is cheaper than provoking one per field
            if (clear == null || readOnly == null || readOnly.GetValue(value) is not false) {
                continue;
            }

            clear.Invoke(value, null);
            return true;
        }

        return false;
    }

    // A dynamic assembly can hold a type that no longer loads, while its other types still need clearing
    private static List<Type> GetTypes(Assembly assembly, List<string> unreachable)
    {
        try {
            return new List<Type>(assembly.GetTypes());
        }
        catch (ReflectionTypeLoadException ex) {
            unreachable.Add(assembly.GetName().Name + "(" + ex.GetType().Name + ")");

            List<Type> loaded = new List<Type>();

            foreach (Type? type in ex.Types) {
                if (type != null) {
                    loaded.Add(type);
                }
            }

            return loaded;
        }
    }

    private static bool IsEngineNamespace(string? value)
    {
        return value != null && (value == "FOnline" || value.StartsWith("FOnline.", StringComparison.Ordinal));
    }
}
