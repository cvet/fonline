namespace FOnline;

using System;
using System.Collections.Concurrent;
using System.Globalization;

// Enum names as scripts and authored data spell them. Beyond the BCL this validates that a value is declared, reads
// the `Type::Member` form content uses, and finds an enum type by name
public static class Enums
{
    // Core scripts are compiled into each backend's entry assembly, so these caches are engine-local
    private static readonly Lazy<Type[]> AssemblyTypes = new Lazy<Type[]>(() => typeof(Enums).Assembly.GetTypes());
    private static readonly ConcurrentDictionary<string, Type[]> TypeCandidates =
        new ConcurrentDictionary<string, Type[]>(StringComparer.Ordinal);

    // The member name; an undeclared value (a [Flags] combination included) breaks the caller's invariant
    public static string GetName<T>(T value)
        where T : struct, Enum
    {
        string? name = Enum.GetName(value);
        Invariant.Verify(name != null, "Invalid enum index", typeof(T).Name, value);
        return name;
    }

    // The `EnumType::Member` form the engine metadata and authored content use
    public static string GetFullName<T>(T value)
        where T : struct, Enum
    {
        return typeof(T).Name + "::" + GetName(value);
    }

    public static bool TryParse<T>(string valueName, out T result)
        where T : struct, Enum
    {
        string normalized = NormalizeValueName(valueName);

        if (Enum.TryParse(normalized, false, out result)) {
            return true;
        }

        return Enum.TryParse(normalized, true, out result);
    }

    // Accepts the value itself, its name as a string or hstring, or its underlying number
    public static T Parse<T>(object value)
        where T : struct, Enum
    {
        if (TryParseValue(value, out T result)) {
            return result;
        }

        Invariant.Failed("Enum value is not found", typeof(T).Name, value);
        return default;
    }

    public static int Parse(string enumTypeName, object valueName)
    {
        Type? enumType = FindType(enumTypeName);
        Invariant.Verify(enumType != null, "Enum type is not found", enumTypeName);
        string text = valueName is hstring hvalue
                        ? hvalue.ToString()
                        : Convert.ToString(valueName, CultureInfo.InvariantCulture) ?? string.Empty;
        bool parsedOk = TryParseObject(enumType, text, out object? result);
        Invariant.Verify(parsedOk, "Enum value is not found", enumTypeName, text);
        return Convert.ToInt32(result, CultureInfo.InvariantCulture);
    }

    internal static bool TryParseObject(Type enumType, string valueName, out object? result)
    {
        string normalized = NormalizeValueName(valueName);

        return Enum.TryParse(enumType, normalized, false, out result) ||
               Enum.TryParse(enumType, normalized, true, out result);
    }

    private static bool TryParseValue<T>(object value, out T result)
        where T : struct, Enum
    {
        if (value is T typed) {
            result = typed;
            return true;
        }

        if (value is hstring hvalue) {
            return TryParse(hvalue.ToString(), out result);
        }

        if (value is string svalue) {
            return TryParse(svalue, out result);
        }

        try {
            if (value is IConvertible) {
                result = (T)Enum.ToObject(typeof(T), value);
                return Enum.IsDefined(result);
            }
        }
        catch (ArgumentException) {
            // Converts, but not to an integral type this enum is built on - which is the Try contract's answer
        }

        result = default;
        return false;
    }

    private static string NormalizeValueName(string valueName)
    {
        string normalized = valueName.Replace("::", ".").Replace(" ", string.Empty);
        int dot = normalized.LastIndexOf('.');

        if (dot >= 0) {
            normalized = normalized.Substring(dot + 1);
        }

        return normalized;
    }

    private static Type? FindType(string enumTypeName)
    {
        string normalized = enumTypeName.Replace("::", ".").Replace(" ", string.Empty);
        string shortName = normalized;
        int dot = shortName.LastIndexOf('.');

        if (dot >= 0) {
            shortName = shortName.Substring(dot + 1);
        }

        Type[] candidates =
            TypeCandidates.GetOrAdd(normalized,
                                    _ =>
                                    {
                                        Type? fallback = null;

                                        foreach (Type type in AssemblyTypes.Value) {
                                            if (!type.IsEnum) {
                                                continue;
                                            }
                                            if (type.FullName == normalized) {
                                                return new[] { type };
                                            }
                                            if (type.Name == shortName) {
                                                fallback ??= type;
                                            }
                                        }

                                        return fallback == null ? Array.Empty<Type>() : new[] { fallback };
                                    });

        return candidates.Length == 0 ? null : candidates[0];
    }
}
