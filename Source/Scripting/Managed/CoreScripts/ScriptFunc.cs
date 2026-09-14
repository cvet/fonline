namespace FOnline;

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using System.Threading.Tasks;

// Calls a script function by name: a managed static method admitted by [CallableByName] (or [AdminRemoteCall] for
// InvokeAdmin), falling back to the native global-function map. A fault in the target is accounted in
// ScriptExceptions and answered with false
public static class ScriptFunc
{
    private const BindingFlags InvokeMethodFlags =
        BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;

    // Core scripts are compiled into each backend's entry assembly, so these caches are engine-local
    private static readonly Lazy<Type[]> InvokeTypes = new Lazy<Type[]>(() => typeof(ScriptFunc).Assembly.GetTypes());
    private static readonly ConcurrentDictionary<string, MethodInfo[]> InvokeCandidates =
        new ConcurrentDictionary<string, MethodInfo[]>(StringComparer.Ordinal);
    private static readonly ConcurrentDictionary<string, MethodInfo[]> AdminCallCandidates =
        new ConcurrentDictionary<string, MethodInfo[]>(StringComparer.Ordinal);

    public static bool Invoke(string funcName, params object?[]? args)
    {
        return InvokeCore(funcName, args ?? Array.Empty<object?>());
    }

    // Awaits the dispatched target when it is asynchronous. `Invoke` only observes the returned Task
    // for faults, which leaves an async target still running after the call returns; a caller that
    // needs the work finished before it reads the resulting state must await this instead
    public static Task<bool> InvokeAsync(string funcName, params object?[]? args)
    {
        return InvokeCoreAsync(funcName, args ?? Array.Empty<object?>());
    }

    // Admin commands have their own allowlist and do not become internal name-dispatch targets merely
    // because the same method is exposed to an authenticated administrator
    public static bool InvokeAdmin(string funcName, params object?[]? args)
    {
        return InvokeAdminCore(funcName, args ?? Array.Empty<object?>());
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
        catch (Exception ex) {
            ScriptExceptions.Record(ex, true);
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
            ScriptExceptions.ObserveTask(result);
            return true;
        }
        catch (Exception ex) {
            ScriptExceptions.Record(ex, true);
            return false;
        }
    }

    private static bool InvokeAdminCore(string funcName, object?[] args)
    {
        try {
            MethodInfo? method = FindAdminCallMethod(funcName, args);
            if (method == null) {
                return false;
            }

            CoerceInvokeArgs(method, args);
            object? result = method.Invoke(null, args);
            ScriptExceptions.ObserveTask(result);
            return true;
        }
        catch (Exception ex) {
            ScriptExceptions.Record(ex, true);
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
                ScriptExceptions.ObserveTask(result);
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
        catch (Exception ex) {
            ScriptExceptions.Record(ex, true);
            return false;
        }
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
            Assembly scriptAssembly = typeof(ScriptFunc).Assembly;
            qualifiedType = scriptAssembly.GetType(normalized) ?? scriptAssembly.GetType("FOnline." + normalized);

            if (qualifiedType == null) {
                // Game scripts live in the host assembly, which is not always this one
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
                Invariant.Verify(Enums.TryParseObject(nonNullableTarget, enumText, out object? enumValue),
                                 "Enum value is not found");
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
            ScriptExceptions.Record(ex, true);
            return false;
        }
    }
}
