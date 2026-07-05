#nullable enable

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;

namespace FOnline
{
    public static partial class Game
    {
        private const BindingFlags InvokeMethodFlags =
            BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;

        private static int _managedGlobalExceptionCount;

        [ThreadStatic]
        private static int _managedContextExceptionCount;

        public static bool Invoke(string funcName, params object?[]? args)
        {
            return InvokeCore(funcName, args ?? Array.Empty<object?>());
        }

        public static bool Invoke<TResult>(string funcName, object? arg0, ref TResult result)
        {
            object?[] args = { arg0, result };
            bool invoked = InvokeCore(funcName, args);
            if (invoked)
            {
                CopyInvokeResult(args, 1, ref result);
            }
            return invoked;
        }

        public static bool Invoke<TResult>(string funcName, object? arg0, object? arg1, ref TResult result)
        {
            object?[] args = { arg0, arg1, result };
            bool invoked = InvokeCore(funcName, args);
            if (invoked)
            {
                CopyInvokeResult(args, 2, ref result);
            }
            return invoked;
        }

        public static bool Invoke<TResult>(string funcName, object? arg0, object? arg1, object? arg2, ref TResult result)
        {
            object?[] args = { arg0, arg1, arg2, result };
            bool invoked = InvokeCore(funcName, args);
            if (invoked)
            {
                CopyInvokeResult(args, 3, ref result);
            }
            return invoked;
        }

        public static bool Invoke<TResult>(string funcName, object? arg0, object? arg1, object? arg2, object? arg3, ref TResult result)
        {
            object?[] args = { arg0, arg1, arg2, arg3, result };
            bool invoked = InvokeCore(funcName, args);
            if (invoked)
            {
                CopyInvokeResult(args, 4, ref result);
            }
            return invoked;
        }

        public static bool Invoke<TResult>(
            string funcName,
            object? arg0,
            object? arg1,
            object? arg2,
            object? arg3,
            object? arg4,
            ref TResult result)
        {
            object?[] args = { arg0, arg1, arg2, arg3, arg4, result };
            bool invoked = InvokeCore(funcName, args);
            if (invoked)
            {
                CopyInvokeResult(args, 5, ref result);
            }
            return invoked;
        }

        public static int GetGlobalExceptionCount()
        {
            return _managedGlobalExceptionCount;
        }

        // Reflects synchronous faults only; deferred (Task continuation) faults run on a foreign
        // thread and increment the global counter exclusively.
        public static int GetContextExceptionCount()
        {
            return _managedContextExceptionCount;
        }

        public static int GetAsInt(GameProperty prop)
        {
            return Convert.ToInt32(Native.GetProperty("Game", prop.ToString(), IntPtr.Zero), CultureInfo.InvariantCulture);
        }

        public static void SetAsInt(GameProperty prop, int value)
        {
            Native.SetProperty("Game", prop.ToString(), IntPtr.Zero, value);
        }

        public static bool TryParseEnum<TEnum>(string valueName, out TEnum result)
            where TEnum : struct, Enum
        {
            return TryParseEnumValue(valueName, out result);
        }

        public static TEnum ParseEnumValue<TEnum>(object value)
            where TEnum : struct, Enum
        {
            if (TryParseEnumValue(value, out TEnum result))
            {
                return result;
            }

            Verify(false, "Enum value is not found");
            return default;
        }

        public static int ParseGenericEnum(string enumName, object valueName)
        {
            Type? enumType = FindEnumType(enumName);
            Verify(enumType != null, "Enum type is not found");
            string text = valueName is hstring hvalue ? hvalue.ToString() : Convert.ToString(valueName, CultureInfo.InvariantCulture) ?? string.Empty;
            Verify(TryParseEnumObject(enumType!, text, out object? result), "Enum value is not found");
            return Convert.ToInt32(result, CultureInfo.InvariantCulture);
        }

        public static ModifierEvent ParseEnum_ModifierEvent(object value)
        {
            return ParseEnumValue<ModifierEvent>(value);
        }

        public static ModifierScope ParseEnum_ModifierScope(object value)
        {
            return ParseEnumValue<ModifierScope>(value);
        }

        public static CritterProperty ParseEnum_CritterProperty(object value)
        {
            return ParseEnumValue<CritterProperty>(value);
        }

        private static bool InvokeCore(string funcName, object?[] args)
        {
            try
            {
                MethodInfo? method = FindInvokeMethod(funcName, args);
                if (method == null)
                {
                    return Native.InvokeScriptFunc(funcName, args);
                }

                CoerceInvokeArgs(method, args);
                object? result = method.Invoke(null, args);
                ObserveInvokeTask(result);
                return true;
            }
            catch (TargetInvocationException ex)
            {
                RecordManagedException(ex.InnerException ?? ex, true);
                return false;
            }
            catch (Exception ex)
            {
                RecordManagedException(ex, true);
                return false;
            }
        }

        private static bool TryParseEnumValue<TEnum>(object value, out TEnum result)
            where TEnum : struct, Enum
        {
            if (value is TEnum typed)
            {
                result = typed;
                return true;
            }

            if (value is hstring hvalue)
            {
                return TryParseEnumValue(hvalue.ToString(), out result);
            }

            if (value is string svalue)
            {
                return TryParseEnumValue(svalue, out result);
            }

            try
            {
                if (value is IConvertible)
                {
                    result = (TEnum)Enum.ToObject(typeof(TEnum), value);
                    return Enum.IsDefined(typeof(TEnum), result);
                }
            }
            catch
            {
            }

            result = default;
            return false;
        }

        private static bool TryParseEnumValue<TEnum>(string valueName, out TEnum result)
            where TEnum : struct, Enum
        {
            string normalized = NormalizeEnumValueName(valueName);

            if (Enum.TryParse(normalized, false, out result))
            {
                return true;
            }

            return Enum.TryParse(normalized, true, out result);
        }

        private static bool TryParseEnumObject(Type enumType, string valueName, out object? result)
        {
            string normalized = NormalizeEnumValueName(valueName);

            try
            {
                result = Enum.Parse(enumType, normalized, false);
                return true;
            }
            catch
            {
            }

            try
            {
                result = Enum.Parse(enumType, normalized, true);
                return true;
            }
            catch
            {
                result = null;
                return false;
            }
        }

        private static string NormalizeEnumValueName(string valueName)
        {
            string normalized = valueName.Replace("::", ".").Replace(" ", string.Empty);
            int dot = normalized.LastIndexOf('.');
            if (dot >= 0)
            {
                normalized = normalized.Substring(dot + 1);
            }

            return normalized;
        }

        private static Type? FindEnumType(string enumName)
        {
            string normalized = enumName.Replace("::", ".").Replace(" ", string.Empty);
            string shortName = normalized;
            int dot = shortName.LastIndexOf('.');
            if (dot >= 0)
            {
                shortName = shortName.Substring(dot + 1);
            }

            foreach (Assembly assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                foreach (Type type in assembly.GetTypes())
                {
                    if (type.IsEnum && (type.FullName == normalized || type.Name == normalized || type.Name == shortName))
                    {
                        return type;
                    }
                }
            }

            return null;
        }

        private static MethodInfo? FindInvokeMethod(string funcName, object?[] args)
        {
            if (string.IsNullOrWhiteSpace(funcName))
            {
                return null;
            }

            ParseInvokeName(funcName, out string? moduleName, out string methodName);

            Assembly assembly = typeof(Game).Assembly;
            foreach (Type type in GetInvokeCandidateTypes(assembly, moduleName))
            {
                MethodInfo? method = FindInvokeMethodOnType(type, methodName, args);
                if (method != null)
                {
                    return method;
                }
            }

            return null;
        }

        private static void ParseInvokeName(string funcName, out string? moduleName, out string methodName)
        {
            int separator = funcName.LastIndexOf("::", StringComparison.Ordinal);
            if (separator != -1)
            {
                moduleName = funcName.Substring(0, separator);
                methodName = funcName.Substring(separator + 2);
                return;
            }

            separator = funcName.LastIndexOf('.');
            if (separator != -1)
            {
                moduleName = funcName.Substring(0, separator);
                methodName = funcName.Substring(separator + 1);
                return;
            }

            moduleName = null;
            methodName = funcName;
        }

        private static IEnumerable<Type> GetInvokeCandidateTypes(Assembly assembly, string? moduleName)
        {
            if (moduleName != null)
            {
                Type? type = assembly.GetType("LastFrontier." + moduleName);
                if (type != null)
                {
                    yield return type;
                }

                type = assembly.GetType("FOnline." + moduleName);
                if (type != null)
                {
                    yield return type;
                }
            }

            Type[] types = assembly.GetTypes();
            for (int i = 0; i < types.Length; i++)
            {
                if (moduleName == null || types[i].Name == moduleName)
                {
                    yield return types[i];
                }
            }
        }

        private static MethodInfo? FindInvokeMethodOnType(Type type, string methodName, object?[] args)
        {
            MethodInfo? fallback = null;
            MethodInfo[] methods = type.GetMethods(InvokeMethodFlags);
            for (int i = 0; i < methods.Length; i++)
            {
                MethodInfo method = methods[i];
                if (method.Name != methodName && method.Name != methodName + "_")
                {
                    continue;
                }

                if (!IsInvokeMethodCompatible(method, args))
                {
                    continue;
                }

                if (method.Name == methodName)
                {
                    return method;
                }

                fallback ??= method;
            }

            return fallback;
        }

        private static bool IsInvokeMethodCompatible(MethodInfo method, object?[] args)
        {
            ParameterInfo[] parameters = method.GetParameters();
            if (parameters.Length != args.Length)
            {
                return false;
            }

            for (int i = 0; i < parameters.Length; i++)
            {
                Type paramType = UnwrapByRef(parameters[i].ParameterType);
                if (!CanCoerceInvokeArg(paramType, args[i]))
                {
                    return false;
                }
            }

            return true;
        }

        private static bool CanCoerceInvokeArg(Type targetType, object? value)
        {
            if (value == null)
            {
                return !targetType.IsValueType || Nullable.GetUnderlyingType(targetType) != null;
            }

            Type valueType = value.GetType();
            if (targetType.IsAssignableFrom(valueType))
            {
                return true;
            }

            Type nonNullableTarget = Nullable.GetUnderlyingType(targetType) ?? targetType;
            if (nonNullableTarget == typeof(hstring) && value is string)
            {
                return true;
            }

            if (nonNullableTarget.IsEnum)
            {
                return value is string || value is IConvertible;
            }

            return value is IConvertible && typeof(IConvertible).IsAssignableFrom(nonNullableTarget);
        }

        private static void CoerceInvokeArgs(MethodInfo method, object?[] args)
        {
            ParameterInfo[] parameters = method.GetParameters();
            for (int i = 0; i < parameters.Length; i++)
            {
                Type paramType = UnwrapByRef(parameters[i].ParameterType);
                args[i] = CoerceInvokeArg(paramType, args[i]);
            }
        }

        private static object? CoerceInvokeArg(Type targetType, object? value)
        {
            if (value == null || targetType.IsInstanceOfType(value))
            {
                return value;
            }

            Type nonNullableTarget = Nullable.GetUnderlyingType(targetType) ?? targetType;
            if (nonNullableTarget == typeof(hstring) && value is string text)
            {
                return hstring.FromString(text);
            }

            if (nonNullableTarget.IsEnum)
            {
                return value is string enumText
                    ? Enum.Parse(nonNullableTarget, enumText)
                    : Enum.ToObject(nonNullableTarget, value);
            }

            if (value is IConvertible && typeof(IConvertible).IsAssignableFrom(nonNullableTarget))
            {
                return Convert.ChangeType(value, nonNullableTarget, CultureInfo.InvariantCulture);
            }

            return value;
        }

        private static Type UnwrapByRef(Type type)
        {
            return type.IsByRef ? type.GetElementType()! : type;
        }

        private static void CopyInvokeResult<TResult>(object?[] args, int index, ref TResult result)
        {
            result = args[index] == null ? default! : (TResult)args[index]!;
        }

        private static void ObserveInvokeTask(object? result)
        {
            if (result is not Task task)
            {
                return;
            }

            if (task.IsCompleted)
            {
                if (task.IsFaulted && task.Exception != null)
                {
                    RecordManagedException(task.Exception, true);
                }
                return;
            }

            _ = task.ContinueWith(
                static failedTask =>
                {
                    if (failedTask.Exception != null)
                    {
                        // Deferred fault runs on a foreign thread, so only the global counter is
                        // incremented here. GetContextExceptionCount reflects synchronous faults only.
                        RecordManagedExceptionGlobal(failedTask.Exception, true);
                    }
                },
                TaskContinuationOptions.OnlyOnFaulted | TaskContinuationOptions.ExecuteSynchronously);
        }

        internal static void RecordManagedException(Exception ex, bool log)
        {
            _managedContextExceptionCount++;
            RecordManagedExceptionGlobal(ex, log);
        }

        private static void RecordManagedExceptionGlobal(Exception ex, bool log)
        {
            Interlocked.Increment(ref _managedGlobalExceptionCount);
            if (log)
            {
                Native.Log(ex.ToString());
            }
        }
    }
}
