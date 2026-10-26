namespace FOnline;

using System;
using System.Reflection;
using System.Threading.Tasks;

// Publishes every managed [ServerRemoteCall] / [ClientRemoteCall] method; the engine keeps only inbound names, and
// the native completion backend owns dispatch and Task lifetime for the direct Action or Task delegate it receives
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

        // Remote calls have no wire result; a Task<T> handler is normalized to a Task delegate
        Delegate handler;

        if (method.ReturnType == typeof(void)) {
            handler = Delegate.CreateDelegate(ScriptFuncRegistration.MakeActionType(delegateParamTypes), method);
        }
        else if (typeof(Task).IsAssignableFrom(method.ReturnType)) {
            handler =
                Delegate.CreateDelegate(ScriptFuncRegistration.MakeFuncType(delegateParamTypes, typeof(Task)), method);
        }
        else {
            throw new InvalidOperationException(
                "Remote call method must return void or Task: " + method.DeclaringType?.FullName + "." + method.Name);
        }

        Native.RegisterRemoteCallHandler(method.Name, parameters.Length, handler);
    }
}
