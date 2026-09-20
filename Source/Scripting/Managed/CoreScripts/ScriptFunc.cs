namespace FOnline;

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using System.Threading.Tasks;

// Calls a script function by name: a managed static method admitted by [CallableByName] (or [AdminRemoteCall] for
// TryInvokeAdmin), falling back to the native global-function map. A name that resolves nowhere throws, and an
// exception thrown by the target reaches the caller unchanged
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

    // An asynchronous target runs until its first incomplete await; a fault after that is observed and accounted in
    // ScriptExceptions, because this caller has already returned
    public static void Invoke(string funcName, params object?[]? args)
    {
        object?[] invokeArgs = args ?? Array.Empty < object ?>();
        MethodInfo? method = FindInvokeMethod(funcName, invokeArgs);

        if (method == null) {
            InvokeNative(funcName, invokeArgs);
            return;
        }

        CoerceInvokeArgs(method, invokeArgs);
        CompleteOrObserve(InvokeTarget(method, invokeArgs));
    }

    // Awaits the dispatched target when it is asynchronous. `Invoke` only observes the returned Task
    // for faults, which leaves an async target still running after the call returns; a caller that
    // needs the work finished before it reads the resulting state must await this instead
    public static async Task InvokeAsync(string funcName, params object?[]? args)
    {
        object?[] invokeArgs = args ?? Array.Empty < object ?>();
        MethodInfo? method = FindInvokeMethod(funcName, invokeArgs);

        if (method == null) {
            InvokeNative(funcName, invokeArgs);
            return;
        }

        CoerceInvokeArgs(method, invokeArgs);

        if (InvokeTarget(method, invokeArgs) is Task task) {
            await task;
        }
    }

    // Admin commands have their own allowlist and do not become internal name-dispatch targets merely because the
    // same method is exposed to an authenticated administrator. The name comes from the administrator, so a name
    // that matches no admin function is an expected answer; a failure inside the function still propagates
    public static bool TryInvokeAdmin(string funcName, params object?[]? args)
    {
        object?[] invokeArgs = args ?? Array.Empty < object ?>();
        MethodInfo? method = FindAdminCallMethod(funcName, invokeArgs);

        if (method == null) {
            return false;
        }

        CoerceInvokeArgs(method, invokeArgs);
        CompleteOrObserve(InvokeTarget(method, invokeArgs));
        return true;
    }

    // The result form: the target either returns the value or writes it through a trailing ref/out parameter, which
    // receives the caller's current value first
    public static void Invoke<TResult>(string funcName, ref TResult result)
    {
        object?[] args = { result };
        InvokeWithResult(funcName, args, 0);
        result = ConvertInvokeResult<TResult>(args[0]);
    }

    public static void Invoke<TResult>(string funcName, object? arg0, ref TResult result)
    {
        object?[] args = { arg0, result };
        InvokeWithResult(funcName, args, 1);
        result = ConvertInvokeResult<TResult>(args[1]);
    }

    public static void Invoke<TResult>(string funcName, object? arg0, object? arg1, ref TResult result)
    {
        object?[] args = { arg0, arg1, result };
        InvokeWithResult(funcName, args, 2);
        result = ConvertInvokeResult<TResult>(args[2]);
    }

    public static void Invoke<TResult>(string funcName, object? arg0, object? arg1, object? arg2, ref TResult result)
    {
        object?[] args = { arg0, arg1, arg2, result };
        InvokeWithResult(funcName, args, 3);
        result = ConvertInvokeResult<TResult>(args[3]);
    }

    public static void Invoke<TResult>(string funcName, object? arg0, object? arg1, object? arg2, object? arg3,
                                       ref TResult result)
    {
        object?[] args = { arg0, arg1, arg2, arg3, result };
        InvokeWithResult(funcName, args, 4);
        result = ConvertInvokeResult<TResult>(args[4]);
    }

    public static void Invoke<TResult>(string funcName, object? arg0, object? arg1, object? arg2, object? arg3,
                                       object? arg4, ref TResult result)
    {
        object?[] args = { arg0, arg1, arg2, arg3, arg4, result };
        InvokeWithResult(funcName, args, 5);
        result = ConvertInvokeResult<TResult>(args[5]);
    }

    public static void Invoke<TResult>(string funcName, object? arg0, object? arg1, object? arg2, object? arg3,
                                       object? arg4, object? arg5, ref TResult result)
    {
        object?[] args = { arg0, arg1, arg2, arg3, arg4, arg5, result };
        InvokeWithResult(funcName, args, 6);
        result = ConvertInvokeResult<TResult>(args[6]);
    }

    private static void InvokeWithResult(string funcName, object?[] args, int resultIndex)
    {
        MethodInfo? method = FindInvokeMethod(funcName, args);

        if (method != null) {
            CoerceInvokeArgs(method, args);
            CompleteOrObserve(InvokeTarget(method, args));
            return;
        }

        object?[] inputArgs = new object?[args.Length - 1];
        Array.Copy(args, 0, inputArgs, 0, resultIndex);
        Array.Copy(args, resultIndex + 1, inputArgs, resultIndex, args.Length - resultIndex - 1);
        method = FindInvokeResultMethod(funcName, inputArgs);

        if (method == null) {
            InvokeNative(funcName, args);
            return;
        }

        CoerceInvokeArgs(method, inputArgs);
        args[resultIndex] = InvokeTarget(method, inputArgs);
    }

    private static void InvokeNative(string funcName, object?[] args)
    {
        if (!Native.InvokeScriptFunc(funcName, args)) {
            Invariant.Failed("Script function is not found", funcName, args.Length);
        }
    }

    // Reflection would wrap the target's exception in TargetInvocationException; the caller must see the original
    private static object? InvokeTarget(MethodInfo method, object?[] args)
    {
        return method.Invoke(null, BindingFlags.DoNotWrapExceptions, null, args, CultureInfo.InvariantCulture);
    }

    private static void CompleteOrObserve(object? result)
    {
        if (result is not Task task) {
            return;
        }

        if (task.IsCompleted) {
            task.GetAwaiter().GetResult();
            return;
        }

        ScriptExceptions.ObserveTask(task);
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
                bool parsedOk = Enums.TryParseObject(nonNullableTarget, enumText, out object? enumValue);
                Invariant.Verify(parsedOk, "Enum value is not found");
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

    private static TResult ConvertInvokeResult<TResult>(object? value)
    {
        object? converted = CoerceInvokeArg(typeof(TResult), value);
        return converted == null ? default! : (TResult)converted;
    }
}
