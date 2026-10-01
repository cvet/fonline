namespace FOnline;

using System;
using System.Threading;
using System.Threading.Tasks;

// Accounting for the exceptions script code raises. Every fault that reaches a dispatch boundary is reported
// through the engine and counted, so a harness can prove that a run stayed clean
public static class ScriptExceptions
{
    private static readonly AsyncLocal<Scope?> CurrentScope = new AsyncLocal<Scope?>();
    private static int RecordedGlobally;

    public static int GlobalCount => RecordedGlobally;

    // Opens a counting scope on the current logical flow. A synchronous fault recorded anywhere in that flow while
    // the scope is open counts into it, on whichever thread an await resumes the flow, and a fault of any other flow
    // never does - a per-thread counter charged a test's fault to whichever callback next ran on that thread. A
    // deferred Task fault completes on a foreign thread and counts only globally
    public static Scope OpenScope()
    {
        Scope scope = new Scope(CurrentScope.Value);
        CurrentScope.Value = scope;
        return scope;
    }

    // A script-owned dispatch boundary -- a loop that runs independent content callbacks and must survive one of
    // them failing -- stops the fault there and reports it exactly as an engine dispatch boundary would
    public static void Report(Exception exception)
    {
        ArgumentNullException.ThrowIfNull(exception);

        Record(exception, true);
    }

    // An exception caught entirely inside script code never crosses a dispatch boundary, so the runtime cannot
    // observe it. A harness that deliberately catches one as expected reports it here to keep the accounting whole
    public static void RecordCaught(Exception exception)
    {
        ArgumentNullException.ThrowIfNull(exception);

        Record(exception, false);
    }

    internal static void ObserveTask(object? result)
    {
        if (result is not Task task) {
            return;
        }

        if (task.IsCompleted) {
            if (task.IsFaulted && task.Exception != null) {
                Record(task.Exception, true);
            }

            return;
        }

        _ = task.ContinueWith(
            static failedTask =>
            {
                if (failedTask.Exception != null) {
                    // A deferred fault runs on a foreign thread, so only the global counter is incremented here
                    RecordGlobal(failedTask.Exception, true);
                }
            },
            CancellationToken.None,
            TaskContinuationOptions.OnlyOnFaulted | TaskContinuationOptions.ExecuteSynchronously,
            // Without an explicit scheduler the continuation would inherit TaskScheduler.Current, which inside a
            // script continuation is the engine's context-bound scheduler -- the exact opposite of the foreign
            // thread this recorder is written for, and a way back into the script context while reporting a fault
            TaskScheduler.Default);
    }

    internal static void Record(Exception ex, bool log)
    {
        // Nested scopes all see the fault: an outer harness keeps its own total while an inner one measures a part
        for (Scope? scope = CurrentScope.Value; scope != null; scope = scope.Parent) {
            scope.Increment();
        }

        RecordGlobal(ex, log);
    }

    private static void RecordGlobal(Exception ex, bool log)
    {
        Interlocked.Increment(ref RecordedGlobally);

        if (log) {
            Native.ReportException(ex);
        }
    }

    // The faults one logical flow recorded since the scope was opened. Disposing closes it for the rest of the flow
    public sealed class Scope : IDisposable
    {
        private int RecordedCount;

        internal Scope(Scope? parent)
        {
            Parent = parent;
        }

        public int Count => Volatile.Read(ref RecordedCount);

        internal Scope? Parent { get; }

        internal void Increment()
        {
            Interlocked.Increment(ref RecordedCount);
        }

        public void Dispose()
        {
            if (CurrentScope.Value == this) {
                CurrentScope.Value = Parent;
            }
        }
    }
}
