namespace FOnline;

using System;
using System.Collections.Concurrent;
using System.Reflection;
using System.Runtime.CompilerServices;

internal static partial class Native
{
    internal delegate bool DocumentMigratorAdapter(object? input, DatabaseDocument document, out object? result);

    internal static DocumentMigratorAdapter CreateMigratorAdapter<T>(PropertyMigrator<T> migrator)
    {
        return (object? input, DatabaseDocument document, out object? result) =>
        {
            T value = (T)input!;

            bool changed = migrator(ref value, document);
            result = changed ? value : null;

            return changed;
        };
    }

    private static readonly ConcurrentDictionary<(string Owner, string Property), Delegate> PropertyMigrators = [];
    private static readonly ConcurrentDictionary<(string Owner, hstring Prototype), PropertyMigrator<hstring>>
        ProtoMigrators = [];

    static partial void ClearPropertyMigrators()
    {
        PropertyMigrators.Clear();
        ProtoMigrators.Clear();
    }

    private static PropertyMigrator<T> FindMigrator<T>(string function, Type attribute, string subject)
    {
        string qualified = function.Replace("::", ".");
        int split = qualified.LastIndexOf('.');
        string methodName = split < 0 ? qualified : qualified.Substring(split + 1);
        string typeName = split < 0 ? "" : qualified.Substring(0, split);

        MethodInfo? selected = null;
        Assembly assembly = typeof(Initializator).Assembly;
        Type? qualifiedType = typeName.Length == 0 ? null : assembly.GetType(typeName, false);

        foreach (Type type in qualifiedType != null ? new Type[] { qualifiedType } : assembly.GetTypes()) {
            if (typeName.Length != 0 && type.FullName != typeName && type.Name != typeName) {
                continue;
            }

            foreach (MethodInfo method in type.GetMethods(BindingFlags.Static | BindingFlags.Public |
                                                          BindingFlags.NonPublic | BindingFlags.DeclaredOnly)) {
                if (method.Name != methodName) {
                    continue;
                }

                if (selected != null) {
                    throw new InvalidOperationException(subject + " migrator function is ambiguous: " + function);
                }

                selected = method;
            }
        }

        if (selected == null || !selected.IsDefined(attribute, false) || selected.ReturnType != typeof(bool) ||
            selected.ContainsGenericParameters || selected.IsDefined(typeof(AsyncStateMachineAttribute), false)) {
            throw new InvalidOperationException(subject +
                                                " migrator must be synchronous, static and attributed: " + function);
        }

        ParameterInfo[] parameters = selected.GetParameters();

        if (parameters.Length != 2 || parameters[0].ParameterType != typeof(T).MakeByRefType() || parameters[0].IsOut ||
            parameters[1].ParameterType != typeof(DatabaseDocument)) {
            throw new InvalidOperationException(subject + " migrator signature does not match the value: " + function);
        }

        return selected.CreateDelegate<PropertyMigrator<T>>();
    }

    internal static void RegisterPropertyMigrator<T>(string owner, string property, string function)
    {
        PropertyMigrator<T> migrator = FindMigrator<T>(function, typeof(PropertyMigratorAttribute), "Property");
        DocumentMigratorAdapter adapter = CreateMigratorAdapter(migrator);

        ThrowNativeError(RegisterPropertyMigratorInternal(BoundBackend, owner, property, adapter));

        if (!PropertyMigrators.TryAdd((owner, property), migrator)) {
            throw new InvalidOperationException("Property migrator delegate is already registered");
        }
    }

    internal static void RegisterProtoMigrator(string owner, string prototype, string function)
    {
        PropertyMigrator<hstring> migrator = FindMigrator<hstring>(function, typeof(ProtoMigratorAttribute), "Proto");
        DocumentMigratorAdapter adapter = CreateMigratorAdapter(migrator);

        ThrowNativeError(RegisterProtoMigratorInternal(BoundBackend, owner, prototype, adapter));

        if (!ProtoMigrators.TryAdd((owner, new hstring(prototype)), migrator)) {
            throw new InvalidOperationException("Proto migrator delegate is already registered");
        }
    }

    internal static bool MigrateOwnedPrototype(string owner, ref hstring value, DatabaseDocument document)
    {
        if (!ProtoMigrators.TryGetValue((owner, value), out PropertyMigrator<hstring>? migrator)) {
            throw new InvalidOperationException("Prototype has no registered document migrator");
        }

        return migrator(ref value, document);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? RegisterProtoMigratorInternal(IntPtr backend, string owner, string prototype,
                                                                Delegate migrator);

    internal static bool MigrateOwnedProperty<T>(string owner, string property, ref T value, DatabaseDocument document)
    {
        if (!PropertyMigrators.TryGetValue((owner, property), out Delegate? callback) ||
            callback is not PropertyMigrator<T> migrator) {
            throw new InvalidOperationException("Registered property migrator does not match the requested value");
        }

        return migrator(ref value, document);
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? RegisterPropertyMigratorInternal(IntPtr backend, string owner, string property,
                                                                   Delegate migrator);

    [CallableByEngine]
    internal static object? InvokePropertyMigrator(Delegate migrator, object? input, IntPtr context, string owner,
                                                   out bool changed)
    {
        DatabaseDocument document = new DatabaseDocument(context, owner);

        try {
            changed = ((DocumentMigratorAdapter)migrator)(input, document, out object? result);

            return result;
        }
        finally {
            document.Close();
        }
    }

    internal static object? ReadDatabaseDocument(IntPtr context, string property)
    {
        ThrowNativeError(ReadDatabaseDocumentInternal(BoundBackend, context, property, out object? value));

        return value;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? ReadDatabaseDocumentInternal(IntPtr backend, IntPtr context, string property,
                                                               out object? value);

    internal static string ReadDatabaseDocumentEncoded(IntPtr context, string field)
    {
        ThrowNativeError(ReadDatabaseDocumentEncodedInternal(BoundBackend, context, field, out string value));

        return value;
    }

    [MethodImpl(MethodImplOptions.InternalCall)]
    private static extern string? ReadDatabaseDocumentEncodedInternal(IntPtr backend, IntPtr context, string field,
                                                                      out string value);
}
