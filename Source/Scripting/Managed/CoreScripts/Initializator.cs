namespace FOnline;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

public static class Initializator
{
    private static bool _initializedEarly;

    static void InitializeEarly()
    {
        if (_initializedEarly) {
            throw new InvalidOperationException(
                "Managed entry assembly was initialized more than once in one load context");
        }

        _initializedEarly = true;
        ValidateAsyncMethods();
        ScriptFuncRegistration.RegisterEngineAttributeFuncs();
        RemoteCallScriptFuncs.RegisterRemoteCalls();
        RunScriptFuncRegistrars();
    }

    private static void ValidateAsyncMethods()
    {
        foreach (Type type in typeof(Initializator).Assembly.GetTypes()) {
            foreach (MethodInfo method in type.GetMethods(BindingFlags.Static | BindingFlags.Instance |
                                                          BindingFlags.Public | BindingFlags.NonPublic |
                                                          BindingFlags.DeclaredOnly)) {
                if (method.ReturnType == typeof(void) && method.IsDefined(typeof(AsyncStateMachineAttribute), false)) {
                    throw new InvalidOperationException("Async void script methods are not supported; return Task: " +
                                                        type.FullName + "." + method.Name);
                }
            }
        }
    }

    // Registrars run before module initialization so bake-time validation can resolve project attributes
    private static void RunScriptFuncRegistrars()
    {
        Assembly assembly = typeof(Initializator).Assembly;

        foreach (Type type in assembly.GetTypes()) {
            foreach (MethodInfo method in type.GetMethods(BindingFlags.Static | BindingFlags.Public |
                                                          BindingFlags.NonPublic | BindingFlags.DeclaredOnly)) {
                if (Attribute.GetCustomAttribute(method, typeof(ScriptFuncRegistrarAttribute)) == null) {
                    continue;
                }

                if (method.GetParameters().Length != 0 || method.ReturnType != typeof(void)) {
                    throw new InvalidOperationException(
                        "ScriptFuncRegistrar method must be static void with no parameters: " + type.FullName + "." +
                        method.Name);
                }

                method.Invoke(null, null);
            }
        }
    }

    static void Initialize()
    {
        using ScriptSynchronizationContext context = ScriptSynchronizationContext.Enter(true);

        List<Tuple<int, MethodInfo>> moduleInits = new List<Tuple<int, MethodInfo>>();

        Assembly assembly = typeof(Initializator).Assembly;

        foreach (Type type in assembly.GetTypes()) {
            RuntimeHelpers.RunClassConstructor(type.TypeHandle);

            foreach (MethodInfo method in type.GetMethods(BindingFlags.Static | BindingFlags.Public |
                                                          BindingFlags.NonPublic)) {
                ModuleInitAttribute? attr = (ModuleInitAttribute?)Attribute.GetCustomAttribute(
                    method,
                    typeof(ModuleInitAttribute));

                if (attr == null) {
                    continue;
                }

                if (method.GetParameters().Length != 0 ||
                    (method.ReturnType != typeof(void) && !typeof(Task).IsAssignableFrom(method.ReturnType))) {
                    throw new InvalidOperationException(
                        "ModuleInit method must be static void or Task with no parameters: " + type.FullName + "." +
                        method.Name);
                }

                moduleInits.Add(Tuple.Create(attr.Priority, method));
            }
        }

        foreach (Tuple<int, MethodInfo> moduleInit in moduleInits.OrderBy(entry => entry.Item1)) {
            MethodInfo method = moduleInit.Item2;
            object? result = method.Invoke(null, null);

            if (method.ReturnType == typeof(void)) {
                continue;
            }

            Task? task = (Task?)result;
            if (task == null) {
                throw new InvalidOperationException(
                    "ModuleInit Task method returned null: " + method.DeclaringType?.FullName + "." + method.Name);
            }

            context.Wait(task);
        }
    }
}
