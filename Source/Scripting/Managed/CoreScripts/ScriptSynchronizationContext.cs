namespace FOnline;

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

internal sealed class ScriptSynchronizationContext : SynchronizationContext, IDisposable
{
    // Core scripts belong to one backend's collectible entry assembly, including this scheduler state
    private static readonly object SchedulerGate = new object();
    private static readonly Queue<ScriptSynchronizationContext> ReadyContexts =
        new Queue<ScriptSynchronizationContext>();
    private static readonly HashSet<ScriptSynchronizationContext> SynchronousContexts =
        new HashSet<ScriptSynchronizationContext>();
    private static bool Closed;

    // Created on the first post: most entries finish without an await and never need it
    private Queue<PostedContinuation>? Continuations;

    private readonly SynchronizationContext? Previous;
    private readonly ScriptSynchronizationContext? SynchronousOwner;
    private bool Synchronous;

    private ScriptSynchronizationContext(bool synchronous)
    {
        Previous = Current;
        Synchronous = synchronous;

        lock (SchedulerGate)
        {
            ObjectDisposedException.ThrowIf(Closed, typeof(ScriptSynchronizationContext));

            if (synchronous) {
                SynchronousContexts.Add(this);
            }
            else if (Previous is ScriptSynchronizationContext parent) {
                SynchronousOwner = parent.GetSynchronousOwner();
            }
        }

        SetSynchronizationContext(this);
    }

    internal static ScriptSynchronizationContext Enter(bool synchronous = false)
    {
        return new ScriptSynchronizationContext(synchronous);
    }

    internal static void VerifyCanYield()
    {
        if (Current is ScriptSynchronizationContext context) {
            lock (SchedulerGate)
            {
                if (context.GetSynchronousOwner() != null) {
                    throw new InvalidOperationException("A synchronous script callback cannot yield an engine timer");
                }
            }
        }
    }

    public override void Post(SendOrPostCallback callback, object? state)
    {
        lock (SchedulerGate)
        {
            if (Closed) {
                return;
            }

            ScriptSynchronizationContext? owner = GetSynchronousOwner();

            if (owner != null) {
                owner.EnqueueContinuation(new PostedContinuation(this, callback, state));
            }
            else {
                EnqueueContinuation(new PostedContinuation(this, callback, state));
                ReadyContexts.Enqueue(this);
            }

            Monitor.PulseAll(SchedulerGate);
        }
    }

    public override SynchronizationContext CreateCopy()
    {
        return this;
    }

    public override void Send(SendOrPostCallback callback, object? state)
    {
        throw new NotSupportedException("Synchronous cross-thread script dispatch is not supported");
    }

    public void Dispose()
    {
        SetSynchronizationContext(Previous);

        if (Synchronous) {
            lock (SchedulerGate)
            {
                SynchronousContexts.Remove(this);
                Synchronous = false;

                if (!Closed) {
                    for (int i = 0; i < ContinuationCount; i++) {
                        ReadyContexts.Enqueue(this);
                    }
                }
            }
        }
    }

    internal void Wait(Task task)
    {
        if (!task.IsCompleted) {
            _ = task.ContinueWith(
                static
                _ =>
                {
                    lock (SchedulerGate)
                    {
                        Monitor.PulseAll(SchedulerGate);
                    }
                },
                CancellationToken.None,
                TaskContinuationOptions.ExecuteSynchronously,
                TaskScheduler.Default);
        }

        while (!task.IsCompleted) {
            PostedContinuation? continuation;

            lock (SchedulerGate)
            {
                while (!Closed && !task.IsCompleted && ContinuationCount == 0) {
                    Monitor.Wait(SchedulerGate);
                }

                ObjectDisposedException.ThrowIf(Closed, typeof(ScriptSynchronizationContext));

                continuation = ContinuationCount != 0 ? DequeueContinuation() : null;
            }

            if (continuation != null) {
                Native.RunScriptContinuation(continuation.Run);
            }
        }

        task.GetAwaiter().GetResult();
    }

    internal static void Pump()
    {
        int count;

        lock (SchedulerGate)
        {
            count = ReadyContexts.Count;
        }

        // Newly posted work waits for the next engine frame; a yielding loop cannot monopolize this frame
        for (int i = 0; i < count; i++) {
            PostedContinuation continuation;

            lock (SchedulerGate)
            {
                if (Closed || ReadyContexts.Count == 0) {
                    return;
                }

                continuation = ReadyContexts.Dequeue().DequeueContinuation();
            }

            Native.RunScriptContinuation(continuation.Run);
        }
    }

    internal static void Shutdown()
    {
        lock (SchedulerGate)
        {
            Closed = true;

            foreach (ScriptSynchronizationContext context in ReadyContexts) {
                context.Continuations?.Clear();
            }

            ReadyContexts.Clear();

            foreach (ScriptSynchronizationContext context in SynchronousContexts) {
                context.Continuations?.Clear();
            }

            Monitor.PulseAll(SchedulerGate);
        }
    }

    // Callers hold SchedulerGate
    private int ContinuationCount => Continuations?.Count ?? 0;

    private void EnqueueContinuation(PostedContinuation continuation)
    {
        Continuations ??= new Queue<PostedContinuation>();
        Continuations.Enqueue(continuation);
    }

    private PostedContinuation DequeueContinuation()
    {
        Invariant.Verify(Continuations != null, "A context with a ready continuation must hold its queue");
        return Continuations.Dequeue();
    }

    private ScriptSynchronizationContext? GetSynchronousOwner()
    {
        if (Synchronous) {
            return this;
        }

        return SynchronousOwner != null && SynchronousOwner.Synchronous ? SynchronousOwner : null;
    }

    // A posted callback kept with the context it was posted to. The pump runs it through a delegate bound to this
    // object, so engine diagnostics can name the script code it resumes (ScriptEntryNames)
    internal sealed class PostedContinuation
    {
        internal PostedContinuation(ScriptSynchronizationContext context, SendOrPostCallback callback, object? state)
        {
            Context = context;
            Callback = callback;
            State = state;
        }

        internal ScriptSynchronizationContext Context { get; }

        internal SendOrPostCallback Callback { get; }

        internal object? State { get; }

        internal void Run()
        {
            Context.Run(Callback, State);
        }
    }

    private void Run(SendOrPostCallback callback, object? state)
    {
        lock (SchedulerGate)
        {
            if (Closed) {
                return;
            }
        }

        SynchronizationContext? previous = Current;
        SetSynchronizationContext(this);

        try {
            callback(state);
        }
        catch (Exception ex) {
            ScriptExceptions.Record(ex, true);
        }
        finally {
            SetSynchronizationContext(previous);
        }
    }
}
