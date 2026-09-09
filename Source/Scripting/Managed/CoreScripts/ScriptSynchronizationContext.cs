#nullable enable

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace FOnline
{
    internal sealed class ScriptSynchronizationContext : SynchronizationContext, IDisposable
    {
        // Core scripts belong to one backend's collectible entry assembly, including this scheduler state
        private static readonly object SchedulerGate = new object();
        private static readonly Queue<ScriptSynchronizationContext> ReadyContexts = new Queue<ScriptSynchronizationContext>();
        private static readonly HashSet<ScriptSynchronizationContext> SynchronousContexts = new HashSet<ScriptSynchronizationContext>();
        private static bool _closed;

        private readonly Queue<Action> _continuations = new Queue<Action>();
        private readonly SynchronizationContext? _previous;
        private readonly ScriptSynchronizationContext? _synchronousOwner;
        private bool _synchronous;

        private ScriptSynchronizationContext(bool synchronous)
        {
            _previous = Current;
            _synchronous = synchronous;

            lock (SchedulerGate)
            {
                if (_closed)
                {
                    throw new ObjectDisposedException(nameof(ScriptSynchronizationContext));
                }

                if (synchronous)
                {
                    SynchronousContexts.Add(this);
                }
                else if (_previous is ScriptSynchronizationContext parent)
                {
                    _synchronousOwner = parent.GetSynchronousOwner();
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
            if (Current is ScriptSynchronizationContext context)
            {
                lock (SchedulerGate)
                {
                    if (context.GetSynchronousOwner() != null)
                    {
                        throw new InvalidOperationException("A synchronous script callback cannot yield an engine timer");
                    }
                }
            }
        }

        public override void Post(SendOrPostCallback callback, object? state)
        {
            lock (SchedulerGate)
            {
                if (_closed)
                {
                    return;
                }

                ScriptSynchronizationContext? owner = GetSynchronousOwner();

                if (owner != null)
                {
                    owner._continuations.Enqueue(() => Run(() => callback(state)));
                }
                else
                {
                    _continuations.Enqueue(() => callback(state));
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
            SetSynchronizationContext(_previous);

            if (_synchronous)
            {
                lock (SchedulerGate)
                {
                    SynchronousContexts.Remove(this);
                    _synchronous = false;

                    if (!_closed)
                    {
                        for (int i = 0; i < _continuations.Count; i++)
                        {
                            ReadyContexts.Enqueue(this);
                        }
                    }
                }
            }
        }

        internal void Wait(Task task)
        {
            if (!task.IsCompleted)
            {
                _ = task.ContinueWith(
                    static _ =>
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

            while (!task.IsCompleted)
            {
                Action? continuation;

                lock (SchedulerGate)
                {
                    while (!_closed && !task.IsCompleted && _continuations.Count == 0)
                    {
                        Monitor.Wait(SchedulerGate);
                    }

                    if (_closed)
                    {
                        throw new ObjectDisposedException(nameof(ScriptSynchronizationContext));
                    }

                    continuation = _continuations.Count != 0 ? _continuations.Dequeue() : null;
                }

                if (continuation != null)
                {
                    Native.RunScriptContinuation(() => Run(continuation));
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
            for (int i = 0; i < count; i++)
            {
                ScriptSynchronizationContext context;
                Action continuation;

                lock (SchedulerGate)
                {
                    if (_closed || ReadyContexts.Count == 0)
                    {
                        return;
                    }

                    context = ReadyContexts.Dequeue();
                    continuation = context._continuations.Dequeue();
                }

                Native.RunScriptContinuation(() => context.Run(continuation));
            }
        }

        internal static void Shutdown()
        {
            lock (SchedulerGate)
            {
                _closed = true;

                foreach (ScriptSynchronizationContext context in ReadyContexts)
                {
                    context._continuations.Clear();
                }

                ReadyContexts.Clear();

                foreach (ScriptSynchronizationContext context in SynchronousContexts)
                {
                    context._continuations.Clear();
                }

                Monitor.PulseAll(SchedulerGate);
            }
        }

        private ScriptSynchronizationContext? GetSynchronousOwner()
        {
            if (_synchronous)
            {
                return this;
            }

            return _synchronousOwner != null && _synchronousOwner._synchronous ? _synchronousOwner : null;
        }

        private void Run(Action continuation)
        {
            lock (SchedulerGate)
            {
                if (_closed)
                {
                    return;
                }
            }

            SynchronizationContext? previous = Current;
            SetSynchronizationContext(this);

            try
            {
                continuation();
            }
            catch (Exception ex)
            {
                Game.RecordManagedException(ex, true);
            }
            finally
            {
                SetSynchronizationContext(previous);
            }
        }
    }
}
