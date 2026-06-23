using System;
using System.Linq;
using System.Linq.Expressions;
using System.Reflection;
using System.Threading.Tasks;

namespace FOnline
{
    // Registers managed [ServerRemoteCall] / [ClientRemoteCall] / [AdminRemoteCall] methods as inbound
    // remote-call handlers, mirroring how AngelScript wires inbound remote calls (RegisterAngelScriptRemoteCalls).
    // Runs once during Initializator.InitializeEarly; the engine keeps only the handlers whose name is inbound on this side
    // (subsystem "cs" in the remote-call metadata), so reflecting a method that is outbound on this side (the
    // opposite peer's caller) is harmless. Remote calls always return void, so handlers are Action<...> delegates.
    internal static class RemoteCallScriptFuncs
    {
        internal static void RegisterRemoteCalls()
        {
            Assembly assembly = typeof(RemoteCallScriptFuncs).Assembly;

            foreach (Type type in assembly.GetTypes())
            {
                if (Initializator.HasCoexistingAngelScriptModule(type, allowManagedModuleInitOwner: true))
                {
                    continue;
                }

                foreach (MethodInfo method in type.GetMethods(
                    BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly))
                {
                    if (IsRemoteCall(method))
                    {
                        RegisterRemoteCall(method);
                    }
                }
            }
        }

        private static bool IsRemoteCall(MethodInfo method)
        {
            return Attribute.GetCustomAttribute(method, typeof(ServerRemoteCallAttribute)) != null
                || Attribute.GetCustomAttribute(method, typeof(ClientRemoteCallAttribute)) != null
                || Attribute.GetCustomAttribute(method, typeof(AdminRemoteCallAttribute)) != null;
        }

        private static void RegisterRemoteCall(MethodInfo method)
        {
            ParameterInfo[] parameters = method.GetParameters();
            Type[] delegateParamTypes = new Type[parameters.Length];

            for (int i = 0; i < parameters.Length; i++)
            {
                delegateParamTypes[i] = parameters[i].ParameterType;
            }

            // Remote calls do not return wire values, but async handlers are represented as Task-returning
            // methods. Wrap the static method as Action<...> or Func<..., Task> so Native.InvokeCallback can
            // DynamicInvoke it and wait for the Task when needed.
            Type delegateType;
            if (method.ReturnType == typeof(void))
            {
                delegateType = Expression.GetActionType(delegateParamTypes);
            }
            else if (typeof(Task).IsAssignableFrom(method.ReturnType))
            {
                delegateType = Expression.GetFuncType(delegateParamTypes.Append(method.ReturnType).ToArray());
            }
            else
            {
                throw new InvalidOperationException(
                    "Remote call method must return void or Task: " +
                    method.DeclaringType.FullName +
                    "." +
                    method.Name);
            }

            Delegate handler = Delegate.CreateDelegate(delegateType, method);

            Native.RegisterRemoteCallHandler(method.Name, parameters.Length, handler);
        }
    }
}
