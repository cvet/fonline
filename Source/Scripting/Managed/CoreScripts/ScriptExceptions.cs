namespace FOnline;

using System;
using System.Threading;
using System.Threading.Tasks;

// Accounting for the exceptions script code raises. Every fault that reaches a dispatch boundary is reported
// through the engine and counted, so a harness can prove that a run stayed clean
public static class ScriptExceptions
{
    private static int _globalCount;

    [ThreadStatic]
    private static int _contextCount;

    public static int GlobalCount => _globalCount;

    // Synchronous faults only: a deferred Task fault completes on a foreign thread and counts globally
    public static int ContextCount => _contextCount;

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
        _contextCount++;
        RecordGlobal(ex, log);
    }

    private static void RecordGlobal(Exception ex, bool log)
    {
        Interlocked.Increment(ref _globalCount);

        if (log) {
            Native.ReportException(ex);
        }
    }
}
