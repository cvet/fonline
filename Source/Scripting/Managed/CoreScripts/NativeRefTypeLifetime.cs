namespace FOnline;

using System;
using System.Collections.Generic;

// Entry-assembly state belongs to one backend. Records retain native ownership, never their managed wrappers.
internal sealed class NativeRefTypeLifetime
{
    private static readonly object Gate = new object();
    private static readonly HashSet<Reference> OwnedReferences = new HashSet<Reference>();
    private static Reference? Pending;
    private static volatile bool Closed;

    private readonly Reference NativeReference;
    private volatile bool Disposed;

    internal NativeRefTypeLifetime(IntPtr pointer, int addRefMethod, int releaseMethod, int disposeMethod = -1)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(addRefMethod);
        ArgumentOutOfRangeException.ThrowIfNegative(releaseMethod);
        Invariant.Verify(pointer != IntPtr.Zero, "Native RefType pointer must not be null");
        NativeReference = new Reference(pointer, addRefMethod, releaseMethod, disposeMethod);

        // Construction is completed by the backend's private acquire/adopt adapter before exposing the wrapper.
        GC.SuppressFinalize(this);
    }

    ~NativeRefTypeLifetime()
    {
        // The intrusive queue allocates nothing and never calls a resource destructor on the finalizer thread.
        lock (Gate)
        {
            Reference? reference = NativeReference;

            if (!Closed && reference != null && reference.Count != 0) {
                reference.Next = Pending;
                Pending = reference;
                Native.SignalContinuationsReady();
            }
        }
    }

    internal void VerifyAlive()
    {
        ObjectDisposedException.ThrowIf(Closed || Disposed || !Native.IsBackendAlive, typeof(NativeRefTypeLifetime));
    }

    // A factory or PassOwnership export supplies this reference; adopting it must not call native AddRef.
    internal void AdoptReference()
    {
        lock (Gate)
        {
            ObjectDisposedException.ThrowIf(Closed || Disposed, typeof(NativeRefTypeLifetime));
            int count = checked(NativeReference.Count + 1);

            if (NativeReference.Count == 0) {
                GC.ReRegisterForFinalize(this);
            }

            OwnedReferences.Add(NativeReference);
            NativeReference.Count = count;
        }
    }

    internal void AddReference()
    {
        lock (Gate)
        {
            ObjectDisposedException.ThrowIf(Closed || Disposed, typeof(NativeRefTypeLifetime));
            int count = checked(NativeReference.Count + 1);

            // Allocate tracking storage before acquiring the native reference.
            OwnedReferences.Add(NativeReference);

            try {
                if (NativeReference.Count == 0) {
                    GC.ReRegisterForFinalize(this);
                }

                CallReferenceMethod(NativeReference.AddRefMethod, NativeReference.Pointer);
                NativeReference.Count = count;
            }
            catch {
                if (NativeReference.Count == 0) {
                    OwnedReferences.Remove(NativeReference);
                    GC.SuppressFinalize(this);
                }

                throw;
            }
        }
    }

    internal void ReleaseReference()
    {
        lock (Gate)
        {
            ObjectDisposedException.ThrowIf(Closed || Disposed, typeof(NativeRefTypeLifetime));

            if (NativeReference.Count == 0) {
                throw new InvalidOperationException("The managed RefType wrapper owns no reference to release");
            }

            ReleaseOne(NativeReference);

            if (NativeReference.Count == 0) {
                GC.SuppressFinalize(this);
            }
        }
    }

    internal void Dispose()
    {
        lock (Gate)
        {
            if (Disposed) {
                return;
            }

            // Reject reentrant native callbacks while returning the lease's references
            Disposed = true;

            try {
                if (!Closed) {
                    ReleaseAll(NativeReference);
                }
            }
            catch {
                Disposed = false;
                throw;
            }

            GC.SuppressFinalize(this);
        }
    }

    [CallableByEngine]
    internal static void Drain()
    {
        lock (Gate)
        {
            while (Pending != null) {
                Reference reference = Pending;
                ReleaseAll(reference);
                Pending = reference.Next;
                reference.Next = null;
            }
        }
    }

    [CallableByEngine]
    internal static void Shutdown()
    {
        lock (Gate)
        {
            Reference[] remaining = new Reference[OwnedReferences.Count];
            OwnedReferences.CopyTo(remaining);
            // Close before unbinding so a late finalizer cannot signal a dead backend or enqueue twice.
            Closed = true;
            Pending = null;

            foreach (Reference reference in remaining) {
                ReleaseAll(reference);
                reference.Next = null;
            }
        }
    }

    private static void ReleaseAll(Reference reference)
    {
        if (reference.Count != 0 && reference.DisposeMethod >= 0 && !reference.NativeDisposed) {
            CallReferenceMethod(reference.DisposeMethod, reference.Pointer);
            reference.NativeDisposed = true;
        }

        while (reference.Count != 0) {
            ReleaseOne(reference);
        }
    }

    private static void ReleaseOne(Reference reference)
    {
        CallReferenceMethod(reference.ReleaseMethod, reference.Pointer);
        reference.Count--;

        if (reference.Count == 0) {
            OwnedReferences.Remove(reference);
        }
    }

    private static void CallReferenceMethod(int methodId, IntPtr pointer)
    {
        byte frame = 0;
        Native.CallMethodIndexed(methodId, pointer, ref frame, 0);
    }

    private sealed class Reference
    {
        internal Reference(IntPtr pointer, int addRefMethod, int releaseMethod, int disposeMethod)
        {
            Pointer = pointer;
            AddRefMethod = addRefMethod;
            ReleaseMethod = releaseMethod;
            DisposeMethod = disposeMethod;
        }

        internal readonly IntPtr Pointer;
        internal readonly int AddRefMethod;
        internal readonly int ReleaseMethod;
        internal readonly int DisposeMethod;
        internal bool NativeDisposed;
        internal int Count;
        internal Reference? Next;
    }
}
