namespace FOnline;

using System;
using System.Reflection;
using System.Threading.Tasks;

// Registration coverage for inbound remote calls: the engine receives the method's own direct delegate, while the
// native completion backend -- not this suite -- owns executing and completing a returned Task
internal static class RemoteCallCompletionScenarios
{
    internal static void Run()
    {
        Native.RegisteredRemoteCalls.Clear();
        Native.RegisteredRemoteCallHandlers.Clear();
        Native.RegisteredRemoteCallParameterCounts.Clear();

        try {
            RemoteCallScriptFuncs.RegisterRemoteCalls();
            Require(Native.RegisteredRemoteCalls.Count == Native.RegisteredRemoteCallHandlers.Count,
                    "Remote call handler registrations do not match the registered names");
            Require(Native.RegisteredRemoteCalls.Count == Native.RegisteredRemoteCallParameterCounts.Count,
                    "Remote call parameter counts do not match the registered names");

            CheckPendingParameterlessTask();
            CheckPendingTaskWithArguments();
            CheckPendingWideTask();
            CheckGenericTask();
            CheckSynchronousActions();
        }
        finally {
            ExampleGame.DispatchProbe.ResetRemoteCallProbes();
            Native.RegisteredRemoteCalls.Clear();
            Native.RegisteredRemoteCallHandlers.Clear();
            Native.RegisteredRemoteCallParameterCounts.Clear();
        }
    }

    private static void CheckPendingParameterlessTask()
    {
        TaskCompletionSource source = new();
        ExampleGame.DispatchProbe.ZeroParameterRemoteSource = source;

        try {
            Delegate handler = RequireHandler("ZeroParameterRemote", 0);
            Require(DelegateReturnType(handler) == typeof(Task),
                    "A Task remote call was not registered as a Task delegate rather than an Action wrapper");
            object? task = handler.DynamicInvoke();

            Require(ReferenceEquals(task, source.Task),
                    "A pending Task remote call did not expose the method's own Task");
            Require(!source.Task.IsCompleted, "Registration completed a pending remote call Task");
        }
        finally {
            source.TrySetResult();
            ExampleGame.DispatchProbe.ZeroParameterRemoteSource = null;
        }
    }

    private static void CheckPendingTaskWithArguments()
    {
        TaskCompletionSource source = new();
        ExampleGame.DispatchProbe.TwoParameterRemoteSource = source;

        try {
            Delegate handler = RequireHandler("TwoParameterRemote", 2);
            Require(DelegateReturnType(handler) == typeof(Task),
                    "A Task remote call with arguments was not registered as a Task delegate");
            object? task = handler.DynamicInvoke(11, "marker");

            Require(ReferenceEquals(task, source.Task),
                    "A Task remote call with arguments did not expose the method's own Task");
            Require(!source.Task.IsCompleted, "Registration completed a pending Task remote call with arguments");
            Require(ExampleGame.DispatchProbe.RemoteNumber == 11 && ExampleGame.DispatchProbe.RemoteText == "marker",
                    "A Task remote call with arguments did not receive its actual arguments");
        }
        finally {
            source.TrySetResult();
            ExampleGame.DispatchProbe.TwoParameterRemoteSource = null;
        }
    }

    private static void CheckPendingWideTask()
    {
        TaskCompletionSource source = new();
        ExampleGame.DispatchProbe.SixteenParameterRemoteSource = source;
        object[] arguments = new object[16];

        for (int i = 0; i < arguments.Length; i++) {
            arguments[i] = i + 1;
        }

        try {
            Delegate handler = RequireHandler("SixteenParameterRemote", 16);
            Require(DelegateReturnType(handler) == typeof(Task),
                    "A 16-parameter Task remote call was not registered as a Task delegate");
            object? task = handler.DynamicInvoke(arguments);

            Require(ReferenceEquals(task, source.Task),
                    "A 16-parameter Task remote call did not expose the method's own Task");
            Require(!source.Task.IsCompleted, "Registration completed a pending 16-parameter Task remote call");
            Require(ExampleGame.DispatchProbe.RemoteWideValue == 1496,
                    "A 16-parameter Task remote call did not receive all of its actual arguments");
        }
        finally {
            source.TrySetResult();
            ExampleGame.DispatchProbe.SixteenParameterRemoteSource = null;
        }
    }

    private static void CheckGenericTask()
    {
        TaskCompletionSource<int> source = new();
        ExampleGame.DispatchProbe.GenericRemoteSource = source;

        try {
            Delegate handler = RequireHandler("GenericRemote", 0);
            Require(DelegateReturnType(handler) == typeof(Task),
                    "A Task<T> remote call was not normalized to a declared Task return type");
            object? result = handler.DynamicInvoke();

            Require(ReferenceEquals(result, source.Task), "A Task<T> remote call did not expose the method's own Task");
            Require(!source.Task.IsCompleted, "Registration completed a pending Task<T> remote call");

            source.SetResult(73);
            Task<int> task = (Task<int>)result!;

            Require(task.IsCompletedSuccessfully && task.Result == 73,
                    "A Task<T> remote call did not preserve its result identity");
        }
        finally {
            source.TrySetResult(73);
            ExampleGame.DispatchProbe.GenericRemoteSource = null;
        }
    }

    private static void CheckSynchronousActions()
    {
        Delegate handler = RequireHandler("VoidRemote", 0);
        Require(handler.GetType() == typeof(Action), "A parameterless void remote call was not a direct Action");
        Require(handler.DynamicInvoke() == null, "A parameterless void remote call produced an unexpected result");
        Require(ScriptEntryNames.Describe(handler) == "DispatchProbe::VoidRemote",
                "A void remote call handler is not named after its method");

        ExampleGame.DispatchProbe.RemoteNumber = 0;
        ExampleGame.DispatchProbe.RemoteText = "";
        Delegate typed = RequireHandler("VoidTwoRemote", 2);

        try {
            Require(typed.GetType() == typeof(Action<int, string>),
                    "A typed void remote call was not a direct Action<int, string>");
            Require(typed.DynamicInvoke(7, "sync") == null, "A typed void remote call unexpectedly produced a result");
            Require(ExampleGame.DispatchProbe.RemoteNumber == 7 && ExampleGame.DispatchProbe.RemoteText == "sync",
                    "A typed void remote call did not receive its actual arguments");
        }
        finally {
            ExampleGame.DispatchProbe.RemoteNumber = 0;
            ExampleGame.DispatchProbe.RemoteText = "";
        }
    }

    private static Delegate RequireHandler(string name, int parameters)
    {
        int index = Native.RegisteredRemoteCalls.IndexOf(name);

        Require(index >= 0, "Remote call " + name + " was not registered");
        Require(Native.RegisteredRemoteCallParameterCounts[index] == parameters,
                "Remote call " + name + " registered parameter count " +
                    Native.RegisteredRemoteCallParameterCounts[index] + " instead of " + parameters);
        return Native.RegisteredRemoteCallHandlers[index];
    }

    private static Type DelegateReturnType(Delegate handler)
    {
        MethodInfo? invoke = handler.GetType().GetMethod("Invoke");

        Require(invoke != null, "A remote call handler type has no Invoke method");
        return invoke!.ReturnType;
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) {
            throw new CheckFailedException(message);
        }
    }
}
