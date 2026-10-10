namespace FOnline;

using System;
using System.Collections.Generic;
using System.Globalization;

// A migrator edits a detached property value. False retains the original serialized value exactly
public delegate bool PropertyMigrator<T>(ref T value, DatabaseDocument document);

[AttributeUsage(AttributeTargets.Method)]
public sealed class PropertyMigratorAttribute : Attribute
{
}

[AttributeUsage(AttributeTargets.Method)]
public sealed class ProtoMigratorAttribute : Attribute
{
}

// Read-only original document, valid only during the synchronous migrator call
// Property reads include prototype defaults when the stored document omits a value
public sealed class DatabaseDocument
{
    private readonly IntPtr Context;
    private readonly IReadOnlyDictionary<string, object>? Values;
    private bool Closed;

    public string EntityType { get; }

    internal DatabaseDocument(IntPtr context, string entityType)
    {
        Context = context;
        EntityType = entityType;
    }

    // Owned documents support isolated migration fixtures without creating an entity
    public DatabaseDocument(string entityType, IReadOnlyDictionary<string, object> values)
    {
        EntityType = entityType;
        Values = values;
    }

    public T? Read<T>(string property)
    {
        if (Closed) {
            throw new InvalidOperationException("Database document context is no longer valid");
        }

        object? value = Values != null ? (Values.TryGetValue(property, out object ? stored) ? stored : null): Native
                                              .ReadDatabaseDocument(Context, property);

        if (value == null) {
            return default;
        }

        if (value is T typed) {
            return typed;
        }

        if (typeof(T) == typeof(hstring) && value is string text) {
            return (T)(object) new hstring(text);
        }

        return (T)Convert.ChangeType(value, typeof(T), CultureInfo.InvariantCulture);
    }

    public string ReadEncoded(string field)
    {
        if (Closed || Values != null) {
            throw new InvalidOperationException("Encoded reads require an active native database document");
        }

        return Native.ReadDatabaseDocumentEncoded(Context, field);
    }

    public bool Migrate<T>(string property, ref T value)
    {
        if (Closed || Values == null) {
            throw new InvalidOperationException("Explicit property migration requires an owned document");
        }

        return Native.MigrateOwnedProperty(EntityType, property, ref value, this);
    }

    public bool MigrateProto(ref hstring value)
    {
        if (Closed || Values == null) {
            throw new InvalidOperationException("Explicit prototype migration requires an owned document");
        }

        return Native.MigrateOwnedPrototype(EntityType, ref value, this);
    }

    internal void Close()
    {
        Closed = true;
    }
}
