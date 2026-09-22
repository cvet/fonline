namespace FOnline;

using System;
using System.Globalization;

// The engine `any_t`: a value held as the engine's text form, converted in implicitly and out explicitly by the
// engine's rules; see Docs/Scripting.md, "The `any` type"
public readonly struct any : IEquatable<any>
{
    // The native bridge boxes and unboxes this struct as the one text reference, so it stays the only field
    private readonly string? Text;

    private any(string? text)
    {
        Text = text;
    }

    public override string ToString() => Text ?? string.Empty;

    public bool IsEmpty => string.IsNullOrEmpty(Text);

    public bool Equals(any other) => string.Equals(ToString(), other.ToString(), StringComparison.Ordinal);
    public override bool Equals(object? obj) => obj is any other && Equals(other);
    public override int GetHashCode() => StringComparer.Ordinal.GetHashCode(ToString());
    public static bool operator ==(any left, any right) => left.Equals(right);
    public static bool operator !=(any left, any right) => !left.Equals(right);

    public static implicit operator any(string value) => new any(value);
    public static implicit operator any(hstring value) => new any(value.ToString());
    public static implicit operator any(bool value) => new any(FieldText(value));
    public static implicit operator any(sbyte value) => new any(FieldText(value));
    public static implicit operator any(byte value) => new any(FieldText(value));
    public static implicit operator any(short value) => new any(FieldText(value));
    public static implicit operator any(ushort value) => new any(FieldText(value));
    public static implicit operator any(int value) => new any(FieldText(value));
    public static implicit operator any(uint value) => new any(FieldText(value));
    public static implicit operator any(long value) => new any(FieldText(value));
    public static implicit operator any(ulong value) => new any(FieldText(value));
    public static implicit operator any(float value) => new any(FieldText(value));
    public static implicit operator any(double value) => new any(FieldText(value));

    // An enum is written with its type, `EnumType::Member`, so reading it back as another enum is refused
    public static implicit operator any(Enum value)
    {
        Type enumType = value.GetType();
        string? name = Enum.GetName(enumType, value);
        Invariant.Verify(name != null, "Enum value has no member to write into any", enumType.Name, value);
        return new any(enumType.Name + "::" + name);
    }

    public static explicit operator string(any value) => value.ToString();
    public static explicit operator hstring(any value) => new hstring(value.ToString());
    public static explicit operator bool(any value) => !value.IsEmpty && ParseBool(value.ToString());
    public static explicit operator sbyte(any value) => (sbyte)value.ToInteger(sbyte.MinValue, sbyte.MaxValue, "int8");
    public static explicit operator byte(any value) => (byte)value.ToInteger(byte.MinValue, byte.MaxValue, "uint8");
    public static explicit operator short(any value) => (short)value.ToInteger(short.MinValue, short.MaxValue, "int16");
    public static explicit operator ushort(any value) => (ushort)value.ToInteger(ushort.MinValue, ushort.MaxValue,
                                                                                 "uint16");
    public static explicit operator int(any value) => (int)value.ToInteger(int.MinValue, int.MaxValue, "int32");
    public static explicit operator uint(any value) => (uint)value.ToInteger(uint.MinValue, uint.MaxValue, "uint32");
    public static explicit operator long(any value) => value.ToInteger(long.MinValue, long.MaxValue, "int64");

    // The engine reads every integer through int64, so the unsigned 64-bit range ends where the signed one does
    public static explicit operator ulong(any value) => (ulong)value.ToInteger(0, long.MaxValue, "uint64");

    public static explicit operator float(any value)
    {
        double number = value.IsEmpty ? 0.0 : ParseFloat(value.ToString());
        float narrowed = (float)number;
        Invariant.Verify(float.IsFinite(narrowed), "Any value does not fit the floating point type", value, "float32");
        return narrowed;
    }

    public static explicit operator double(any value) => value.IsEmpty ? 0.0 : ParseFloat(value.ToString());

    // `EnumType::Member`, the bare member name, or the member's number, which is how property data reads back
    public T ToEnum<T>()
        where T : struct, Enum
    {
        return (T)ToEnum(typeof(T));
    }

    private object ToEnum(Type enumType)
    {
        string text = ToString().Trim();

        if (TryParseInteger(text, out _) && !text.Contains('.')) {
            // Enum.ToObject truncates to the underlying storage, so validate that range before boxing the enum
            object numbered = Enum.ToObject(enumType, ToObject(Enum.GetUnderlyingType(enumType)));
            Invariant.Verify(Enum.IsDefined(enumType, numbered),
                             "Any value is not a member of the enum",
                             text,
                             enumType.Name);
            return numbered;
        }

        string memberName = text;
        int separator = text.IndexOf("::", StringComparison.Ordinal);

        if (separator >= 0) {
            Invariant.Verify(text.AsSpan(0, separator).SequenceEqual(enumType.Name),
                             "Any value holds another enum type",
                             text,
                             enumType.Name);
            memberName = text.Substring(separator + 2);
        }

        // Enum.TryParse also reads a number and a comma-separated flag set, neither of which names one member
        object? result = null;
        bool parsed = memberName.Length != 0 && (char.IsLetter(memberName[0]) || memberName[0] == '_') &&
                      !memberName.Contains(',') && Enum.TryParse(enumType, memberName, false, out result);
        Invariant.Verify(parsed && result != null, "Any value is not a member of the enum", text, enumType.Name);
        return result;
    }

    // Primitive fields in declaration order, nested value types flattened, separated by a space; the operators the
    // baker generates on each value type build and read the text through these helpers
    internal static any FromFields(string[] fields) => new any(string.Join(" ", fields));

    // Written text keeps a field with empty text in its place, which only an exact split reads back; text typed by
    // hand is read the way the engine reads it, with repeated spaces ignored
    internal string[] SplitFields(int count, string typeName)
    {
        string text = ToString();
        string[] fields = text.Split(' ');

        if (fields.Length != count) {
            fields = text.Split(' ', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        }

        Invariant.Verify(fields.Length == count,
                         "Any value has another number of fields than the value type",
                         this,
                         typeName,
                         count);
        return fields;
    }

    internal static string FieldText(bool value) => value ? "true" : "false";
    internal static string FieldText(sbyte value) => value.ToString(CultureInfo.InvariantCulture);
    internal static string FieldText(byte value) => value.ToString(CultureInfo.InvariantCulture);
    internal static string FieldText(short value) => value.ToString(CultureInfo.InvariantCulture);
    internal static string FieldText(ushort value) => value.ToString(CultureInfo.InvariantCulture);
    internal static string FieldText(int value) => value.ToString(CultureInfo.InvariantCulture);
    internal static string FieldText(uint value) => value.ToString(CultureInfo.InvariantCulture);
    internal static string FieldText(long value) => value.ToString(CultureInfo.InvariantCulture);
    internal static string FieldText(ulong value) => value.ToString(CultureInfo.InvariantCulture);
    internal static string FieldText(float value) => value.ToString(CultureInfo.InvariantCulture);
    internal static string FieldText(double value) => value.ToString(CultureInfo.InvariantCulture);
    internal static string FieldText(hstring value) => value.ToString();

    internal static bool FieldBool(string field) => (bool)new any(field);
    internal static sbyte FieldInt8(string field) => (sbyte) new any(field);
    internal static byte FieldUInt8(string field) => (byte) new any(field);
    internal static short FieldInt16(string field) => (short)new any(field);
    internal static ushort FieldUInt16(string field) => (ushort) new any(field);
    internal static int FieldInt32(string field) => (int)new any(field);
    internal static uint FieldUInt32(string field) => (uint) new any(field);
    internal static long FieldInt64(string field) => (long)new any(field);
    internal static ulong FieldUInt64(string field) => (ulong) new any(field);
    internal static float FieldFloat32(string field) => (float)new any(field);
    internal static double FieldFloat64(string field) => (double)new any(field);
    internal static hstring FieldHash(string field) => new hstring(field);

    // Name dispatch passes arguments as objects, so a value reaches an `any` parameter through its runtime type
    internal static bool TryFromObject(object value, out any result)
    {
        switch (value) {
        case any text:
            result = text;
            return true;
        case string text:
            result = text;
            return true;
        case hstring hash:
            result = hash;
            return true;
        case bool flag:
            result = flag;
            return true;
        case Enum enumValue:
            result = enumValue;
            return true;
        case sbyte number:
            result = number;
            return true;
        case byte number:
            result = number;
            return true;
        case short number:
            result = number;
            return true;
        case ushort number:
            result = number;
            return true;
        case int number:
            result = number;
            return true;
        case uint number:
            result = number;
            return true;
        case long number:
            result = number;
            return true;
        case ulong number:
            result = number;
            return true;
        case float number:
            result = number;
            return true;
        case double number:
            result = number;
            return true;
        }

        // A value type converts through the operator the baker generated on it
        System.Reflection.MethodInfo? conversion =
            FindConversion(value.GetType(), "op_Implicit", value.GetType(), typeof(any));

        if (conversion != null && InvokeConversion(conversion, value) is any converted) {
            result = converted;
            return true;
        }

        result = default;
        return false;
    }

    // The reverse of TryFromObject, for an `any` handed to a typed parameter by name dispatch
    internal object ToObject(Type type)
    {
        if (type == typeof(any)) {
            return this;
        }
        if (type == typeof(string)) {
            return (string)this;
        }
        if (type == typeof(hstring)) {
            return (hstring)this;
        }
        if (type == typeof(bool)) {
            return (bool)this;
        }
        if (type.IsEnum) {
            return ToEnum(type);
        }
        if (type == typeof(sbyte)) {
            return (sbyte)this;
        }
        if (type == typeof(byte)) {
            return (byte)this;
        }
        if (type == typeof(short)) {
            return (short)this;
        }
        if (type == typeof(ushort)) {
            return (ushort)this;
        }
        if (type == typeof(int)) {
            return (int)this;
        }
        if (type == typeof(uint)) {
            return (uint)this;
        }
        if (type == typeof(long)) {
            return (long)this;
        }
        if (type == typeof(ulong)) {
            return (ulong)this;
        }
        if (type == typeof(float)) {
            return (float)this;
        }
        if (type == typeof(double)) {
            return (double)this;
        }

        System.Reflection.MethodInfo? conversion = FindConversion(type, "op_Explicit", typeof(any), type);
        object? converted = conversion != null ? InvokeConversion(conversion, this) : null;
        Invariant.Verify(converted != null, "Any value does not convert to the parameter type", this, type.Name);
        return converted;
    }

    internal static bool ConvertsTo(Type type)
    {
        return type == typeof(any) || type == typeof(string) || type == typeof(hstring) || type == typeof(bool) ||
               type.IsEnum ||
               (type.IsPrimitive && type != typeof(char) && type != typeof(IntPtr) && type != typeof(UIntPtr)) ||
               FindConversion(type, "op_Explicit", typeof(any), type) != null;
    }

    // The operator's own failure reaches the caller as it is, not wrapped in a reflection exception
    private static object? InvokeConversion(System.Reflection.MethodInfo conversion, object value)
    {
        return conversion.Invoke(null, System.Reflection.BindingFlags.DoNotWrapExceptions, null, new[] { value }, null);
    }

    private static System.Reflection.MethodInfo? FindConversion(Type owner, string name, Type source, Type target)
    {
        foreach (System.Reflection.MethodInfo method in owner.GetMethods(System.Reflection.BindingFlags.Public |
                                                                         System.Reflection.BindingFlags.Static)) {
            if (method.Name == name && method.ReturnType == target) {
                System.Reflection.ParameterInfo[] parameters = method.GetParameters();

                if (parameters.Length == 1 && parameters[0].ParameterType == source) {
                    return method;
                }
            }
        }

        return null;
    }

    // Empty text is zero, `true`/`false` are one and zero, a number is read as the engine reads one, and
    // `EnumType::Member` is the member's value
    private long ToInteger(long min, long max, string typeName)
    {
        string text = ToString();

        if (text.Length == 0) {
            return 0;
        }

        string trimmed = text.Trim();
        long number;

        if (TryParseExplicitBool(trimmed, out bool flag)) {
            number = flag ? 1 : 0;
        }
        else if (!TryParseInteger(trimmed, out number)) {
            int separator = trimmed.IndexOf("::", StringComparison.Ordinal);
            Invariant.Verify(separator > 0, "Any value is not an integer", text, typeName);
            number = Enums.Parse(trimmed.Substring(0, separator), trimmed.Substring(separator + 2));
        }

        Invariant.Verify(number >= min && number <= max, "Any value does not fit the integer type", text, typeName);
        return number;
    }

    private static bool ParseBool(string text)
    {
        string trimmed = text.Trim();

        if (TryParseExplicitBool(trimmed, out bool flag)) {
            return flag;
        }

        bool parsed = TryParseInteger(trimmed, out long number);
        Invariant.Verify(parsed, "Any value is not a boolean", text);
        return number != 0;
    }

    private static double ParseFloat(string text)
    {
        string trimmed = text.Trim();
        double number;

        if (IsHexNumber(trimmed)) {
            bool parsedHex = TryParseInteger(trimmed, out long hexNumber);
            Invariant.Verify(parsedHex, "Any value is not a number", text);
            number = hexNumber;
        }
        else {
            // The engine accepts the `f` suffix of a C++ float literal
            string digits =
                trimmed.Length > 1 && trimmed.EndsWith('f') ? trimmed.Substring(0, trimmed.Length - 1) : trimmed;
            bool parsed = double.TryParse(digits, NumberStyles.Float, CultureInfo.InvariantCulture, out number);
            Invariant.Verify(parsed, "Any value is not a number", text);
        }

        Invariant.Verify(double.IsFinite(number), "Any value is not a finite number", text);
        return number;
    }

    private static bool TryParseExplicitBool(string trimmed, out bool flag)
    {
        if (trimmed.Equals("true", StringComparison.OrdinalIgnoreCase)) {
            flag = true;
            return true;
        }
        if (trimmed.Equals("false", StringComparison.OrdinalIgnoreCase)) {
            flag = false;
            return true;
        }

        flag = false;
        return false;
    }

    // Decimal, hexadecimal after `0x`, and a fraction, which is truncated toward zero and clamped to the range
    private static bool TryParseInteger(string trimmed, out long number)
    {
        if (IsHexNumber(trimmed)) {
            bool negative = trimmed[0] == '-';
            bool parsedHex = ulong.TryParse(trimmed.AsSpan(negative ? 3 : 2),
                                            NumberStyles.AllowHexSpecifier,
                                            CultureInfo.InvariantCulture,
                                            out ulong magnitude);
            number = negative ? unchecked(-(long)magnitude) : unchecked((long)magnitude);
            return parsedHex;
        }

        if (long.TryParse(trimmed, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out number)) {
            return true;
        }

        string digits =
            trimmed.Length > 1 && trimmed.EndsWith('f') ? trimmed.Substring(0, trimmed.Length - 1) : trimmed;

        if (double.TryParse(digits, NumberStyles.Float, CultureInfo.InvariantCulture, out double fraction) &&
            double.IsFinite(fraction)) {
            number = fraction >= long.MaxValue ? long.MaxValue
                   : fraction <= long.MinValue ? long.MinValue
                                               : (long)fraction;
            return true;
        }

        number = 0;
        return false;
    }

    private static bool IsHexNumber(string trimmed)
    {
        int start = trimmed.StartsWith('-') ? 1 : 0;
        return trimmed.Length > start + 2 && trimmed[start] == '0' &&
               (trimmed[start + 1] == 'x' || trimmed[start + 1] == 'X');
    }
}

