namespace AcceptanceProbe;

using System;
using System.Threading.Tasks;
using FOnline;

// One deliberate defect per engine-contract rule, compiled by the analyzer self-tests against the real
// CoreScripts/Attributes.cs and CoreScripts/Invariant.cs. A rule that is enabled but never fires reads exactly like a
// rule that found nothing; this file makes the difference visible. Each `ACCEPT: <id>` marker expects that id inside
// the declaration that follows it, and `ACCEPT-NA: <id> - <reason>` records a rule that cannot fire on source.
//
// The file is not part of any script build: no ManagedScript.Dirs entry and no analyzer project compiles it
public static class EngineContractProbe
{
    public sealed class Subject
    {
        public bool IsReady { get; set; }
    }

    [Event]
    internal static void OnSubjectChanged(Subject subject)
    {
        _ = subject;
    }

    // ACCEPT: FOENTRY001
    // A dispatched entry point called from ordinary code skips the context its dispatcher prepares
    internal static void CallsEntryPointDirectly(Subject subject)
    {
        OnSubjectChanged(subject);
    }

    // ACCEPT: FOENTRY002
    // An engine-called method declared public promises callers outside the script assembly
    [CallableByName]
    public static void PublicNamedEntry()
    {
    }

    // ACCEPT-NA: FOENTRY003 - reports a misspelt .editorconfig attribute name at compilation level, with no source location

    // ACCEPT: FOVERIFY001
    // The message repeats the expression instead of naming the broken invariant
    internal static void VerifyEchoesCondition(int count)
    {
        Invariant.Verify(count > 0, "count > 0");
    }

    // ACCEPT: FOVERIFY002
    // A constant false condition where the branch has already proved the state wrong
    internal static void VerifyWithConstantFalse()
    {
        Invariant.Verify(false, "Probe must never reach this branch");
    }

    // ACCEPT: FOVERIFY003
    // The condition does the work the rest of the method depends on
    internal static async Task VerifyConditionDoesWork(Task<bool> pending)
    {
        Invariant.Verify(await pending, "Probe condition must not do work");
    }

    // ACCEPT: FOSTATIC001
    // The hidden backing field of a generic static auto-property is out of reach of the shutdown sweep
    public static class GenericState<T>
    {
        public static object? Value { get; set; }
    }

    // ACCEPT: FOSTATIC001
    // A thread static has one slot per thread, and the sweep clears only one
    public static class ThreadState
    {
        [ThreadStatic]
        private static Subject? Current;

        internal static void Remember(Subject subject)
        {
            Current = subject;
        }
    }
}
