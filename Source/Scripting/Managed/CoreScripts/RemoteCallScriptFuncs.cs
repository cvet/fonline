#nullable enable

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

            // Remote calls do not return wire values. Task handlers must release the inbound network pump as
            // soon as they suspend; waiting here deadlocks handlers whose continuation needs a later client tick.
            Type delegateType = Expression.GetActionType(delegateParamTypes);
            Delegate handler;

            if (method.ReturnType == typeof(void))
            {
                handler = Delegate.CreateDelegate(delegateType, method);
            }
            else if (typeof(Task).IsAssignableFrom(method.ReturnType))
            {
                ParameterExpression[] lambdaParameters = delegateParamTypes
                    .Select((type, index) => Expression.Parameter(type, "arg" + index))
                    .ToArray();
                MethodCallExpression invokeHandler = Expression.Call(method, lambdaParameters);
                MethodCallExpression observeTask = Expression.Call(
                    typeof(RemoteCallScriptFuncs),
                    nameof(ObserveRemoteCallTask),
                    Type.EmptyTypes,
                    Expression.Convert(invokeHandler, typeof(Task)));

                handler = Expression.Lambda(delegateType, observeTask, lambdaParameters).Compile();
            }
            else
            {
                throw new InvalidOperationException(
                    "Remote call method must return void or Task: " +
                    method.DeclaringType?.FullName +
                    "." +
                    method.Name);
            }

            Native.RegisterRemoteCallHandler(method.Name, parameters.Length, handler);
        }

        private static void ObserveRemoteCallTask(Task task)
        {
            Game.ObserveInvokeTask(task);
        }
    }
}
