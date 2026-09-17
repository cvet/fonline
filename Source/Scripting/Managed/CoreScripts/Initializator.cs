namespace FOnline;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

public static partial class Initializator
{
    private static bool InitializedEarly;

    static partial void BindGeneratedAbi();

    [CallableByEngine]
    internal static void InitializeEarly()
    {
        if (InitializedEarly) {
            throw new InvalidOperationException(
                "Managed entry assembly was initialized more than once in one load context");
        }

        InitializedEarly = true;
        BindGeneratedAbi();
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

    [CallableByEngine]
    internal static void Initialize()
    {
        using ScriptSynchronizationContext context = ScriptSynchronizationContext.Enter(true);

        List<Tuple<int, MethodInfo>> moduleInits = new List<Tuple<int, MethodInfo>>();

        Assembly assembly = typeof(Initializator).Assembly;

        foreach (Type type in assembly.GetTypes()) {
            RunClassConstructor(type);

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
            object? result = Invoke(method, "ModuleInit");

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

    // Reflection answers a failure inside the invoked method with an exception of its own, and the name of the
    // method that actually failed appears nowhere in it
    private static object? Invoke(MethodInfo method, string role)
    {
        try {
            return method.Invoke(null, null);
        }
        catch (TargetInvocationException ex) when (ex.InnerException != null) {
            throw new InvalidOperationException(role + " failed: " + method.DeclaringType?.FullName + "." + method.Name,
                                                ex.InnerException);
        }
    }

    // A static constructor failure names the type only inside the exception the runtime wraps it in, and the
    // engine reports the innermost message, so the type is stated here instead
    private static void RunClassConstructor(Type type)
    {
        try {
            RuntimeHelpers.RunClassConstructor(type.TypeHandle);
        }
        catch (TypeInitializationException ex) {
            throw new InvalidOperationException("Static constructor failed: " + type.FullName, ex);
        }
    }
}
