namespace FOnline;

using System;
using System.Reflection;
using System.Runtime.CompilerServices;

// Names the script code a native entry runs, the way scripts name functions for dispatch by name ("Type::Method").
// Engine diagnostics such as run overruns ask for it only after the fact, so the reflection stays off the hot path
internal static class ScriptEntryNames
{
    internal static string Describe(object? entry)
    {
        if (entry is Delegate handler) {
            // The script pump runs a posted continuation through a delegate bound to it
            if (handler.Target is ScriptSynchronizationContext.PostedContinuation continuation) {
                return DescribeContinuation(continuation);
            }

            return DescribeMethod(handler.Method);
        }
        if (entry is ScriptSynchronizationContext.PostedContinuation posted) {
            return DescribeContinuation(posted);
        }

        return entry != null ? entry.GetType().Name : "null";
    }

    private static string DescribeContinuation(ScriptSynchronizationContext.PostedContinuation continuation)
    {
        // An await posts the async method's box, or the MoveNext delegate bound to it, and the box type carries the
        // state machine; anything else posted to the context is named after its callback
        Delegate? resume = continuation.State as Delegate;
        object? resumed = resume != null ? resume.Target : continuation.State;
        Type? stateMachine = resumed != null ? FindStateMachine(resumed.GetType()) : null;

        if (stateMachine != null) {
            return DescribeType(stateMachine.DeclaringType) + "::" + TrimGeneratedName(stateMachine.Name) +
                   " (continuation)";
        }

        return DescribeMethod(resume != null ? resume.Method : continuation.Callback.Method) + " (continuation)";
    }

    private static Type? FindStateMachine(Type type)
    {
        if (typeof(IAsyncStateMachine).IsAssignableFrom(type) && type.IsDefined(typeof(CompilerGeneratedAttribute))) {
            return type;
        }
        if (!type.IsGenericType) {
            return null;
        }

        foreach (Type argument in type.GetGenericArguments()) {
            Type? stateMachine = FindStateMachine(argument);

            if (stateMachine != null) {
                return stateMachine;
            }
        }

        return null;
    }

    private static string DescribeMethod(MethodInfo method)
    {
        return DescribeType(method.DeclaringType) + "::" + TrimGeneratedName(method.Name);
    }

    // A lambda, a local function and an async state machine are compiler-generated members of generated types
    // nested in the script type that wrote them, so the name walks out to that type
    private static string DescribeType(Type? type)
    {
        while (type != null && type.DeclaringType != null && IsGeneratedName(type.Name)) {
            type = type.DeclaringType;
        }

        return type != null ? type.Name : "<unknown>";
    }

    // "<OnLoop>b__12_0" and "<ProcessAsync>d__7" both come from the member named between the angle brackets
    private static string TrimGeneratedName(string name)
    {
        if (!IsGeneratedName(name)) {
            return name;
        }

        int end = name.IndexOf('>', StringComparison.Ordinal);
        return end > 1 ? name.Substring(1, end - 1) : name;
    }

    private static bool IsGeneratedName(string name)
    {
        return name.StartsWith('<');
    }
}
