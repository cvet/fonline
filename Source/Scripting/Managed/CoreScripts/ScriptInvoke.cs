namespace FOnline;

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;

public static partial class Game
{
    private const BindingFlags InvokeMethodFlags =
        BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;

    // Core scripts are compiled into each backend's entry assembly, so these caches are engine-local
    private static readonly Lazy<Type[]> InvokeTypes = new Lazy<Type[]>(() => typeof(Game).Assembly.GetTypes());
    private static readonly ConcurrentDictionary<string, MethodInfo[]> InvokeCandidates =
        new ConcurrentDictionary<string, MethodInfo[]>(StringComparer.Ordinal);
    private static readonly ConcurrentDictionary<string, MethodInfo[]> AdminCallCandidates =
        new ConcurrentDictionary<string, MethodInfo[]>(StringComparer.Ordinal);
    private static readonly ConcurrentDictionary<string, Type[]> EnumCandidates =
        new ConcurrentDictionary<string, Type[]>(StringComparer.Ordinal);

    private static int _managedGlobalExceptionCount;

    [ThreadStatic]
    private static int _managedContextExceptionCount;

    public static bool Invoke(string funcName, params object?[]? args)
    {
        return InvokeCore(funcName, args ?? Array.Empty<object?>());
    }

    // Awaits the dispatched target when it is asynchronous. `Invoke` only observes the returned Task
    // for faults, which leaves an async target still running after the call returns; a caller that
    // needs the work finished before it reads the resulting state must await this instead.
    public static Task<bool> InvokeAsync(string funcName, params object?[]? args)
    {
        return InvokeCoreAsync(funcName, args ?? Array.Empty<object?>());
    }

    // Admin commands have their own allowlist and do not become internal name-dispatch targets merely
    // because the same method is exposed to an authenticated administrator
    public static bool CallAdminFunc(string funcName, params object?[]? args)
    {
        return CallAdminFuncCore(funcName, args ?? Array.Empty<object?>());
    }

    public static bool Invoke<TResult>(string funcName, ref TResult result)
    {
        object?[] args = { result };
        return InvokeCoreWithResult(funcName, args, 0) && CopyInvokeResult(args, 0, ref result);
    }

    public static bool Invoke<TResult>(string funcName, object? arg0, ref TResult result)
    {
        object?[] args = { arg0, result };
        return InvokeCoreWithResult(funcName, args, 1) && CopyInvokeResult(args, 1, ref result);
    }

    public static bool Invoke<TResult>(string funcName, object? arg0, object? arg1, ref TResult result)
    {
        object?[] args = { arg0, arg1, result };
        return InvokeCoreWithResult(funcName, args, 2) && CopyInvokeResult(args, 2, ref result);
    }

    public static bool Invoke<TResult>(string funcName, object? arg0, object? arg1, object? arg2, ref TResult result)
    {
        object?[] args = { arg0, arg1, arg2, result };
        return InvokeCoreWithResult(funcName, args, 3) && CopyInvokeResult(args, 3, ref result);
    }

    public static bool Invoke<TResult>(string funcName, object? arg0, object? arg1, object? arg2, object? arg3,
                                       ref TResult result)
    {
        object?[] args = { arg0, arg1, arg2, arg3, result };
        return InvokeCoreWithResult(funcName, args, 4) && CopyInvokeResult(args, 4, ref result);
    }

    public static bool Invoke<TResult>(string funcName, object? arg0, object? arg1, object? arg2, object? arg3,
                                       object? arg4, ref TResult result)
    {
        object?[] args = { arg0, arg1, arg2, arg3, arg4, result };
        return InvokeCoreWithResult(funcName, args, 5) && CopyInvokeResult(args, 5, ref result);
    }

    public static bool Invoke<TResult>(string funcName, object? arg0, object? arg1, object? arg2, object? arg3,
                                       object? arg4, object? arg5, ref TResult result)
    {
        object?[] args = { arg0, arg1, arg2, arg3, arg4, arg5, result };
        return InvokeCoreWithResult(funcName, args, 6) && CopyInvokeResult(args, 6, ref result);
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

    // A managed exception caught entirely inside project C# never crosses InvokeCore or a native callback
    // boundary, so the runtime cannot observe it automatically. Test harnesses that deliberately exercise
    // and catch such a path report the caught instance here before acknowledging it; this preserves the
    // same bounded exception accounting that AngelScript provided for caught script exceptions.
    public static void RecordCaughtException(Exception exception)
    {
        if (exception == null) {
            throw new ArgumentNullException(nameof(exception));
        }

        RecordManagedException(exception, false);
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
        if (TryParseEnumValue(value, out TEnum result)) {
            return result;
        }

        Verify(false, "Enum value is not found");
        return default;
    }

    public static int ParseGenericEnum(string enumName, object valueName)
    {
        Type? enumType = FindEnumType(enumName);
        Verify(enumType != null, "Enum type is not found");
        string text = valueName is hstring hvalue
                        ? hvalue.ToString()
                        : Convert.ToString(valueName, CultureInfo.InvariantCulture) ?? string.Empty;
        Verify(TryParseEnumObject(enumType, text, out object? result), "Enum value is not found");
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

    private static async Task<bool> InvokeCoreAsync(string funcName, object?[] args)
    {
        try {
            MethodInfo? method = FindInvokeMethod(funcName, args);
            if (method == null) {
                return Native.InvokeScriptFunc(funcName, args);
            }

            CoerceInvokeArgs(method, args);
            object? result = method.Invoke(null, args);

            if (result is Task task) {
                await task;
            }

            return true;
        }
        catch (TargetInvocationException ex) {
            RecordManagedException(ex.InnerException ?? ex, true);
            return false;
        }
        catch (Exception ex) {
            RecordManagedException(ex, true);
            return false;
        }
    }

    private static bool InvokeCore(string funcName, object?[] args)
    {
        try {
            MethodInfo? method = FindInvokeMethod(funcName, args);
            if (method == null) {
                return Native.InvokeScriptFunc(funcName, args);
            }

            CoerceInvokeArgs(method, args);
            object? result = method.Invoke(null, args);
            ObserveInvokeTask(result);
            return true;
        }
        catch (TargetInvocationException ex) {
            RecordManagedException(ex.InnerException ?? ex, true);
            return false;
        }
        catch (Exception ex) {
            RecordManagedException(ex, true);
            return false;
        }
    }

    private static bool CallAdminFuncCore(string funcName, object?[] args)
    {
        try {
            MethodInfo? method = FindAdminCallMethod(funcName, args);
            if (method == null) {
                return false;
            }

            CoerceInvokeArgs(method, args);
            object? result = method.Invoke(null, args);
            ObserveInvokeTask(result);
            return true;
        }
        catch (TargetInvocationException ex) {
            RecordManagedException(ex.InnerException ?? ex, true);
            return false;
        }
        catch (Exception ex) {
            RecordManagedException(ex, true);
            return false;
        }
    }

    private static bool InvokeCoreWithResult(string funcName, object?[] args, int resultIndex)
    {
        try {
            MethodInfo? method = FindInvokeMethod(funcName, args);
            if (method != null) {
                CoerceInvokeArgs(method, args);
                object? result = method.Invoke(null, args);
                ObserveInvokeTask(result);
                return true;
            }

            object?[] inputArgs = new object?[args.Length - 1];
            Array.Copy(args, 0, inputArgs, 0, resultIndex);
            Array.Copy(args, resultIndex + 1, inputArgs, resultIndex, args.Length - resultIndex - 1);
            method = FindInvokeResultMethod(funcName, inputArgs);
            if (method == null) {
                return Native.InvokeScriptFunc(funcName, args);
            }

            CoerceInvokeArgs(method, inputArgs);
            args[resultIndex] = method.Invoke(null, inputArgs);
            return true;
        }
        catch (TargetInvocationException ex) {
            RecordManagedException(ex.InnerException ?? ex, true);
            return false;
        }
        catch (Exception ex) {
            RecordManagedException(ex, true);
            return false;
        }
    }

    private static bool TryParseEnumValue<TEnum>(object value, out TEnum result)
        where TEnum : struct, Enum
    {
        if (value is TEnum typed) {
            result = typed;
            return true;
        }

        if (value is hstring hvalue) {
            return TryParseEnumValue(hvalue.ToString(), out result);
        }

        if (value is string svalue) {
            return TryParseEnumValue(svalue, out result);
        }

        try {
            if (value is IConvertible) {
                result = (TEnum)Enum.ToObject(typeof(TEnum), value);
                return Enum.IsDefined(typeof(TEnum), result);
            }
        }
        catch {
        }

        result = default;
        return false;
    }

    private static bool TryParseEnumValue<TEnum>(string valueName, out TEnum result)
        where TEnum : struct, Enum
    {
        string normalized = NormalizeEnumValueName(valueName);

        if (Enum.TryParse(normalized, false, out result)) {
            return true;
        }

        return Enum.TryParse(normalized, true, out result);
    }

    private static bool TryParseEnumObject(Type enumType, string valueName, out object? result)
    {
        string normalized = NormalizeEnumValueName(valueName);

        try {
            result = Enum.Parse(enumType, normalized, false);
            return true;
        }
        catch {
        }

        try {
            result = Enum.Parse(enumType, normalized, true);
            return true;
        }
        catch {
            result = null;
            return false;
        }
    }

    private static string NormalizeEnumValueName(string valueName)
    {
        string normalized = valueName.Replace("::", ".").Replace(" ", string.Empty);
        int dot = normalized.LastIndexOf('.');
        if (dot >= 0) {
            normalized = normalized.Substring(dot + 1);
        }

        return normalized;
    }

    private static Type? FindEnumType(string enumName)
    {
        string normalized = enumName.Replace("::", ".").Replace(" ", string.Empty);
        string shortName = normalized;
        int dot = shortName.LastIndexOf('.');
        if (dot >= 0) {
            shortName = shortName.Substring(dot + 1);
        }

        Type[] candidates =
            EnumCandidates.GetOrAdd(normalized,
                                    _ =>
                                    {
                                        Type? fallback = null;

                                        foreach (Type type in InvokeTypes.Value) {
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

    private static MethodInfo? FindInvokeMethod(string funcName, object?[] args)
    {
        if (string.IsNullOrWhiteSpace(funcName)) {
            return null;
        }

        foreach (MethodInfo method in GetInvokeCandidates(funcName)) {
            if (IsInvokeMethodCompatible(method, args)) {
                return method;
            }
        }

        return null;
    }

    private static MethodInfo? FindInvokeResultMethod(string funcName, object?[] args)
    {
        if (string.IsNullOrWhiteSpace(funcName)) {
            return null;
        }

        foreach (MethodInfo method in GetInvokeCandidates(funcName)) {
            if (method.ReturnType != typeof(void) && !typeof(Task).IsAssignableFrom(method.ReturnType) &&
                IsInvokeMethodCompatible(method, args)) {
                return method;
            }
        }

        return null;
    }

    private static MethodInfo? FindAdminCallMethod(string funcName, object?[] args)
    {
        if (string.IsNullOrWhiteSpace(funcName)) {
            return null;
        }

        foreach (MethodInfo method in GetAdminCallCandidates(funcName)) {
            if (IsInvokeMethodCompatible(method, args)) {
                return method;
            }
        }

        return null;
    }

    private static MethodInfo[] GetInvokeCandidates(string funcName)
    {
        return GetAttributedCallCandidates(InvokeCandidates, funcName, typeof(CallableByNameAttribute));
    }

    private static MethodInfo[] GetAdminCallCandidates(string funcName)
    {
        return GetAttributedCallCandidates(AdminCallCandidates, funcName, typeof(AdminRemoteCallAttribute));
    }

    private static MethodInfo[] GetAttributedCallCandidates(ConcurrentDictionary<string, MethodInfo[]> cache,
                                                             string funcName, Type attributeType)
    {
        return cache.GetOrAdd(funcName,
                              name =>
                              {
            ParseInvokeName(name, out string? moduleName, out string methodName);
            var methods = new List<MethodInfo>();

            foreach (Type type in GetInvokeCandidateTypes(moduleName)) {
                MethodInfo[] declared = type.GetMethods(InvokeMethodFlags);

                foreach (MethodInfo method in declared) {
                    if (!method.ContainsGenericParameters && method.Name == methodName &&
                        Attribute.IsDefined(method, attributeType)) {
                        methods.Add(method);
                    }
                }
                foreach (MethodInfo method in declared) {
                    if (!method.ContainsGenericParameters && method.Name == methodName + "_" &&
                        Attribute.IsDefined(method, attributeType)) {
                        methods.Add(method);
                    }
                }
            }

            return methods.ToArray();
                              });
    }

    private static void ParseInvokeName(string funcName, out string? moduleName, out string methodName)
    {
        int separator = funcName.LastIndexOf("::", StringComparison.Ordinal);
        if (separator != -1) {
            moduleName = funcName.Substring(0, separator);
            methodName = funcName.Substring(separator + 2);
            return;
        }

        separator = funcName.LastIndexOf('.');
        if (separator != -1) {
            moduleName = funcName.Substring(0, separator);
            methodName = funcName.Substring(separator + 1);
            return;
        }

        moduleName = null;
        methodName = funcName;
    }

    private static IEnumerable<Type> GetInvokeCandidateTypes(string? moduleName)
    {
        Type? qualifiedType = null;

        if (moduleName != null) {
            string normalized = moduleName.Replace("::", ".");
            Assembly gameAssembly = typeof(Game).Assembly;
            qualifiedType = gameAssembly.GetType(normalized) ?? gameAssembly.GetType("FOnline." + normalized);

            if (qualifiedType == null) {
                // Game scripts live in the host assembly, which is not always `typeof(Game).Assembly`
                // when CoreScripts are compiled into a separate engine module. Nested types keep the
                // reflection `Outer+Inner` spelling that callers already pass
                foreach (Assembly assembly in AppDomain.CurrentDomain.GetAssemblies()) {
                    qualifiedType = assembly.GetType(normalized);
                    if (qualifiedType != null) {
                        break;
                    }
                }
            }

            if (qualifiedType != null) {
                yield return qualifiedType;
            }
        }

        foreach (Type type in InvokeTypes.Value) {
            if (type != qualifiedType && (moduleName == null || type.Name == moduleName)) {
                yield return type;
            }
        }
    }

    private static bool IsInvokeMethodCompatible(MethodInfo method, object?[] args)
    {
        ParameterInfo[] parameters = method.GetParameters();
        if (parameters.Length != args.Length) {
            return false;
        }

        for (int i = 0; i < parameters.Length; i++) {
            Type paramType = UnwrapByRef(parameters[i].ParameterType);
            if (!CanCoerceInvokeArg(paramType, args[i])) {
                return false;
            }
        }

        return true;
    }

    private static bool CanCoerceInvokeArg(Type targetType, object? value)
    {
        if (value == null) {
            return !targetType.IsValueType || Nullable.GetUnderlyingType(targetType) != null;
        }

        Type valueType = value.GetType();
        if (targetType.IsAssignableFrom(valueType)) {
            return true;
        }

        Type nonNullableTarget = Nullable.GetUnderlyingType(targetType) ?? targetType;
        if (nonNullableTarget == typeof(hstring) && value is string) {
            return true;
        }

        if (nonNullableTarget.IsEnum) {
            return value is string || value is IConvertible;
        }

        return value is IConvertible && typeof(IConvertible).IsAssignableFrom(nonNullableTarget);
    }

    private static void CoerceInvokeArgs(MethodInfo method, object?[] args)
    {
        ParameterInfo[] parameters = method.GetParameters();
        for (int i = 0; i < parameters.Length; i++) {
            Type paramType = UnwrapByRef(parameters[i].ParameterType);
            args[i] = CoerceInvokeArg(paramType, args[i]);
        }
    }

    private static object? CoerceInvokeArg(Type targetType, object? value)
    {
        if (value == null || targetType.IsInstanceOfType(value)) {
            return value;
        }

        Type nonNullableTarget = Nullable.GetUnderlyingType(targetType) ?? targetType;
        if (nonNullableTarget == typeof(hstring) && value is string text) {
            return hstring.FromString(text);
        }

        if (nonNullableTarget.IsEnum) {
            if (value is string enumText) {
                Verify(TryParseEnumObject(nonNullableTarget, enumText, out object? enumValue), "Enum value is not found");
                return enumValue;
            }

            return Enum.ToObject(nonNullableTarget, value);
        }

        if (value is IConvertible && typeof(IConvertible).IsAssignableFrom(nonNullableTarget)) {
            return Convert.ChangeType(value, nonNullableTarget, CultureInfo.InvariantCulture);
        }

        return value;
    }

    private static Type UnwrapByRef(Type type)
    {
        return type.IsByRef ? type.GetElementType()! : type;
    }

    private static bool CopyInvokeResult<TResult>(object?[] args, int index, ref TResult result)
    {
        try {
            object? value = CoerceInvokeArg(typeof(TResult), args[index]);
            result = value == null ? default! : (TResult)value;
            return true;
        }
        catch (Exception ex) {
            RecordManagedException(ex, true);
            return false;
        }
    }

    internal static void ObserveInvokeTask(object? result)
    {
        if (result is not Task task) {
            return;
        }

        if (task.IsCompleted) {
            if (task.IsFaulted && task.Exception != null) {
                RecordManagedException(task.Exception, true);
            }
            return;
        }

        _ = task.ContinueWith(
            static failedTask =>
            {
                if (failedTask.Exception != null) {
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
        if (log) {
            Native.Log(ex.ToString());
        }
    }
}
