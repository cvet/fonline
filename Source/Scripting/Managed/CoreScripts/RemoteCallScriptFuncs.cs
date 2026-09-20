namespace FOnline;

using System;
using System.Reflection;
using System.Threading.Tasks;

// Registers managed [ServerRemoteCall] / [ClientRemoteCall] methods as inbound
// remote-call handlers, mirroring how AngelScript wires inbound remote calls (RegisterAngelScriptRemoteCalls).
// Runs once during Initializator.InitializeEarly; the engine keeps only the handlers whose name is inbound on this side
// (subsystem "cs" in the remote-call metadata), so reflecting a method that is outbound on this side (the
// opposite peer's caller) is harmless. Remote calls always return void, so handlers are Action<...> delegates.
internal static class RemoteCallScriptFuncs
{
    internal static void RegisterRemoteCalls()
    {
        Assembly assembly = typeof(RemoteCallScriptFuncs).Assembly;

        foreach (Type type in assembly.GetTypes()) {
            foreach (MethodInfo method in type.GetMethods(BindingFlags.Static | BindingFlags.Public |
                                                          BindingFlags.NonPublic | BindingFlags.DeclaredOnly)) {
                if (IsInboundRemoteCall(method)) {
                    RegisterRemoteCall(method);
                }
            }
        }
    }

    private static bool IsInboundRemoteCall(MethodInfo method)
    {
        return Attribute.GetCustomAttribute(method, typeof(ServerRemoteCallAttribute)) != null ||
               Attribute.GetCustomAttribute(method, typeof(ClientRemoteCallAttribute)) != null;
    }

    private static void RegisterRemoteCall(MethodInfo method)
    {
        ParameterInfo[] parameters = method.GetParameters();
        Type[] delegateParamTypes = new Type[parameters.Length];

        for (int i = 0; i < parameters.Length; i++) {
            delegateParamTypes[i] = parameters[i].ParameterType;
        }

        // Remote calls do not return wire values. Task handlers must release the inbound network pump as
        // soon as they suspend; waiting here deadlocks handlers whose continuation needs a later client tick.
        Delegate handler;

        if (method.ReturnType == typeof(void)) {
            handler = Delegate.CreateDelegate(ScriptFuncRegistration.MakeActionType(delegateParamTypes), method);
        }
        else if (typeof(Task).IsAssignableFrom(method.ReturnType)) {
            Delegate taskHandler =
                Delegate.CreateDelegate(ScriptFuncRegistration.MakeFuncType(delegateParamTypes, typeof(Task)), method);
            handler = (Delegate)MakeObserveTaskHandler(delegateParamTypes).Invoke(null, new object[] { taskHandler })!;
        }
        else {
            throw new InvalidOperationException(
                "Remote call method must return void or Task: " + method.DeclaringType?.FullName + "." + method.Name);
        }

        Native.RegisterRemoteCallHandler(method.Name, parameters.Length, handler);
    }

    private static MethodInfo MakeObserveTaskHandler(Type[] parameterTypes)
    {
        foreach (MethodInfo adapter in typeof(RemoteCallScriptFuncs)
                     .GetMethods(BindingFlags.Static | BindingFlags.NonPublic)) {
            if (adapter.Name == nameof(ObserveTaskHandler) &&
                adapter.GetGenericArguments().Length == parameterTypes.Length) {
                return parameterTypes.Length == 0 ? adapter : adapter.MakeGenericMethod(parameterTypes);
            }
        }

        throw new NotSupportedException("Remote call method has more parameters than a delegate can carry: " +
                                        parameterTypes.Length);
    }

    // One adapter per parameter count wraps a Task handler in the Action the engine invokes and hands the task to the
    // exception accounting instead of waiting for it
    private static Action ObserveTaskHandler(Func<Task> handler) => () => ScriptExceptions.ObserveTask(handler());
    private static Action<T1>
    ObserveTaskHandler<T1>(Func<T1, Task> handler) => a1 => ScriptExceptions.ObserveTask(handler(a1));
    private static Action<T1, T2>
    ObserveTaskHandler<T1, T2>(Func<T1, T2, Task> handler) => (a1, a2) => ScriptExceptions.ObserveTask(handler(a1, a2));
    private static Action<T1, T2, T3> ObserveTaskHandler<T1, T2, T3>(Func<T1, T2, T3, Task> handler) => (a1, a2, a3) =>
        ScriptExceptions.ObserveTask(handler(a1, a2, a3));
    private static Action<T1, T2, T3, T4> ObserveTaskHandler<T1, T2, T3, T4>(Func<T1, T2, T3, T4, Task> handler) =>
        (a1, a2, a3, a4) => ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4));
    private static Action<T1, T2, T3, T4, T5> ObserveTaskHandler<T1, T2, T3, T4, T5>(
        Func<T1, T2, T3, T4, T5, Task> handler) => (a1, a2, a3, a4,
                                                    a5) => ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5));
    private static Action<T1, T2, T3, T4, T5, T6>
    ObserveTaskHandler<T1, T2, T3, T4, T5, T6>(Func<T1, T2, T3, T4, T5, T6, Task> handler) =>
        (a1, a2, a3, a4, a5, a6) => ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5, a6));
    private static Action<T1, T2, T3, T4, T5, T6, T7>
    ObserveTaskHandler<T1, T2, T3, T4, T5, T6, T7>(Func<T1, T2, T3, T4, T5, T6, T7, Task> handler) =>
        (a1, a2, a3, a4, a5, a6, a7) => ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5, a6, a7));
    private static Action<T1, T2, T3, T4, T5, T6, T7, T8>
    ObserveTaskHandler<T1, T2, T3, T4, T5, T6, T7, T8>(Func<T1, T2, T3, T4, T5, T6, T7, T8, Task> handler) =>
        (a1, a2, a3, a4, a5, a6, a7, a8) => ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5, a6, a7, a8));
    private static Action<T1, T2, T3, T4, T5, T6, T7, T8, T9>
    ObserveTaskHandler<T1, T2, T3, T4, T5, T6, T7, T8, T9>(Func<T1, T2, T3, T4, T5, T6, T7, T8, T9, Task> handler) =>
        (a1, a2, a3, a4, a5, a6, a7, a8,
         a9) => ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5, a6, a7, a8, a9));
    private static Action<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>
    ObserveTaskHandler<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>(
        Func<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, Task> handler) => (a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) =>
        ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10));
    private static Action<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11>
    ObserveTaskHandler<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11>(
        Func<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, Task> handler) => (a1, a2, a3, a4, a5, a6, a7, a8, a9, a10,
                                                                              a11) =>
        ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11));
    private static Action<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12>
    ObserveTaskHandler<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12>(
        Func<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, Task> handler) => (a1, a2, a3, a4, a5, a6, a7, a8, a9,
                                                                                   a10, a11, a12) =>
        ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12));
    private static Action<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13>
    ObserveTaskHandler<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13>(
        Func<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, Task> handler) => (a1, a2, a3, a4, a5, a6, a7, a8,
                                                                                        a9, a10, a11, a12, a13) =>
        ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13));
    private static Action<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14>
    ObserveTaskHandler<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14>(
        Func<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, Task> handler) =>
        (a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13,
         a14) => ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14));
    private static Action<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15>
    ObserveTaskHandler<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15>(
        Func<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, Task> handler) =>
        (a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) =>
            ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15));
    private static Action<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16>
    ObserveTaskHandler<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16>(
        Func<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, Task> handler) =>
        (a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
         a16) => ScriptExceptions.ObserveTask(handler(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
                                                      a16));
}
