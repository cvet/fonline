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
    private static int PendingTasks;

    public static int GlobalCount => RecordedGlobally;

    internal static int PendingTaskCount => Volatile.Read(ref PendingTasks);

    // Opens a counting scope on the current logical flow. A synchronous fault recorded anywhere in that flow while
    // the scope is open counts into it, on whichever thread an await resumes the flow, and a fault of any other flow
    // never does - a per-thread counter charged a test's fault to whichever callback next ran on that thread. A
    // deferred Task fault completes on a foreign thread and counts only globally
    public static Scope OpenScope()
    {
        Scope scope = new Scope(CurrentScope.Value, false);
        CurrentScope.Value = scope;
        return scope;
    }

    // Captures deliberate reports in this logical flow without invoking native error notification; counters and
    // textual diagnostics remain intact. Unrelated flows, deferred faults and reports after disposal are unaffected
    public static Scope OpenCaptureScope()
    {
        Scope scope = new Scope(CurrentScope.Value, true);
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

    internal static void ObserveTask(object? result, bool deferred = false)
    {
        if (result is not Task task) {
            return;
        }

        if (task.IsCompleted) {
            if (task.IsFaulted && task.Exception != null) {
                if (deferred) {
                    RecordGlobal(task.Exception, true);
                }
                else {
                    Record(task.Exception, true);
                }
            }

            return;
        }

        Interlocked.Increment(ref PendingTasks);

        _ = task.ContinueWith(
            static completedTask =>
            {
                try {
                    if (completedTask.IsFaulted && completedTask.Exception != null) {
                        // A deferred fault runs on a foreign thread, so only the global counter is incremented here
                        RecordGlobal(completedTask.Exception, true);
                    }
                }
                finally {
                    Interlocked.Decrement(ref PendingTasks);
                }
            },
            CancellationToken.None,
            TaskContinuationOptions.ExecuteSynchronously,
            // Without an explicit scheduler the continuation would inherit TaskScheduler.Current, which inside a
            // script continuation is the engine's context-bound scheduler -- the exact opposite of the foreign
            // thread this recorder is written for, and a way back into the script context while reporting a fault
            TaskScheduler.Default);
    }

    internal static void Record(Exception ex, bool log)
    {
        // Nested scopes all see the fault: an outer harness keeps its own total while an inner one measures a part
        bool captured = false;
        for (Scope? scope = CurrentScope.Value; scope != null; scope = scope.Parent) {
            scope.Increment();
            captured |= scope.CapturesReports;
        }

        RecordGlobal(ex, log && !captured);

        if (log && captured) {
            Native.Log("Captured script exception: " + ex);
        }
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
        private readonly bool CaptureReports;
        private int Disposed;
        private int RecordedCount;

        internal Scope(Scope? parent, bool captureReports)
        {
            Parent = parent;
            CaptureReports = captureReports;
        }

        public int Count => Volatile.Read(ref RecordedCount);

        internal Scope? Parent { get; }

        internal bool CapturesReports => CaptureReports && Volatile.Read(ref Disposed) == 0;

        internal void Increment()
        {
            Interlocked.Increment(ref RecordedCount);
        }

        public void Dispose()
        {
            Interlocked.Exchange(ref Disposed, 1);

            if (CurrentScope.Value == this) {
                CurrentScope.Value = Parent;
            }
        }
    }
}
