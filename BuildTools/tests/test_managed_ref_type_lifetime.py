"""Exercise native RefType ownership with the production GC lease in isolated managed processes."""

from pathlib import Path
import os
import shutil
import subprocess

import pytest


CORE = Path(__file__).resolve().parents[2] / "Source/Scripting/Managed/CoreScripts"
PROJECT = """<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework><OutputType>Exe</OutputType>
    <Nullable>enable</Nullable><ImplicitUsings>disable</ImplicitUsings>
    <CheckForOverflowUnderflow>true</CheckForOverflowUnderflow>
    <TreatWarningsAsErrors>true</TreatWarningsAsErrors>
  </PropertyGroup>
</Project>
"""
PROBE = r"""
namespace FOnline;

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;
using System.Threading;

internal sealed class CallableByEngineAttribute : Attribute { }

internal static class Invariant
{
    internal static void Verify(bool value, string message)
    {
        if (!value) { throw new InvalidOperationException(message); }
    }
}

internal static class Native
{
    internal static bool IsBackendAlive = true;
    internal static bool FailAdd;
    internal static int FailReleaseAfter = -1;
    internal static Action? Releasing;
    internal static int Signals;
    internal static int Adds;
    internal static int Releases;
    internal static int Destroyed;
    internal static int Disposals;
    internal static bool FailDispose;
    internal static int OwnerThread = Environment.CurrentManagedThreadId;
    internal static readonly Dictionary<IntPtr, int> Counts = new Dictionary<IntPtr, int>();

    internal static void SignalContinuationsReady()
    {
        Invariant.Verify(IsBackendAlive, "A finalizer signalled a dead backend");
        Interlocked.Increment(ref Signals);
    }

    internal static void CallMethodIndexed(int method, IntPtr pointer, ref byte frame, int size)
    {
        Invariant.Verify(IsBackendAlive, "Release reached an unbound backend");
        Invariant.Verify(Environment.CurrentManagedThreadId == OwnerThread, "A destructor ran on the finalizer thread");
        Invariant.Verify(size == 0 && Counts[pointer] > 0, "Reference ABI or double release is invalid");

        if (method == 0) {
            if (FailAdd) { throw new InvalidOperationException("Rejected AddRef"); }
            Counts[pointer]++;
            Adds++;
        }
        else if (method == 1) {
            if (Releases == FailReleaseAfter) { throw new InvalidOperationException("Rejected Release"); }
            Releasing?.Invoke();
            Releases++;
            Counts[pointer]--;
            if (Counts[pointer] == 0) { Destroyed++; }
        }
        else if (method == 2) {
            if (FailDispose) { throw new InvalidOperationException("Rejected native Dispose"); }
            Disposals++;
        }
        else { throw new InvalidOperationException("Unexpected reference method"); }
    }
}

// Mirrors the generated constructor's shared lease, including its temporary factory wrapper.
internal sealed class Wrapper : IDisposable
{
    internal readonly NativeRefTypeLifetime Lifetime;
    internal static WeakReference? FactoryTemporary;

    internal Wrapper(IntPtr pointer, int disposeMethod = -1)
    {
        Lifetime = new NativeRefTypeLifetime(pointer, 0, 1, disposeMethod);
    }

    internal Wrapper()
    {
        Wrapper created = Factory(new IntPtr(1));
        FactoryTemporary = new WeakReference(created);
        Lifetime = created.Lifetime;
    }

    internal static Wrapper Factory(IntPtr pointer)
    {
        Wrapper result = new Wrapper(pointer);
        result.Lifetime.AdoptReference();
        return result;
    }

    public void Dispose() => Lifetime.Dispose();

    internal static Wrapper BorrowedReturn(IntPtr pointer, int disposeMethod = -1)
    {
        Wrapper result = new Wrapper(pointer, disposeMethod);
        result.Lifetime.AddReference();
        return result;
    }
}

public static class Probe
{
    private static Wrapper? Retained;

    public static void SeedRetained()
    {
        Native.Counts[new IntPtr(1)] = 1;
        Retained = Wrapper.Factory(new IntPtr(1));
    }

    public static int Count() => Native.Counts[new IntPtr(1)];

    public static void Close()
    {
        NativeRefTypeLifetime.Shutdown();
        Native.IsBackendAlive = false;
        Retained = null;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static WeakReference Seed(string mode)
    {
        IntPtr pointer = new IntPtr(1);
        Native.Counts[pointer] = 1;
        Wrapper wrapper = mode == "borrowed" || mode == "borrowed-retained" || mode == "failed-add" || mode == "reacquire"
            ? new Wrapper(pointer) : Wrapper.Factory(pointer);

        if (mode == "borrowed-retained") {
            wrapper.Lifetime.AddReference();
        }
        else if (mode == "reacquire") {
            wrapper.Lifetime.AddReference();
            wrapper.Lifetime.ReleaseReference();
            wrapper.Lifetime.AddReference();
        }
        else if (mode == "manual") {
            wrapper.Lifetime.ReleaseReference();
        }
        else if (mode == "multiple") {
            wrapper.Lifetime.AddReference();
            wrapper.Lifetime.AddReference();
            wrapper.Lifetime.ReleaseReference();
        }
        else if (mode == "failed-add") {
            Native.FailAdd = true;
            try { wrapper.Lifetime.AddReference(); }
            catch (InvalidOperationException) { }
            Native.FailAdd = false;
        }

        return new WeakReference(wrapper.Lifetime);
    }

    private static void Collect()
    {
        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static WeakReference AcquiredReturn(int disposeMethod)
    {
        Wrapper wrapper = Wrapper.BorrowedReturn(new IntPtr(1), disposeMethod);
        // The native owner ends its lifetime while the managed reference remains.
        byte frame = 0;
        Native.CallMethodIndexed(1, new IntPtr(1), ref frame, 0);
        wrapper.Lifetime.VerifyAlive();
        Invariant.Verify(Native.Counts[new IntPtr(1)] == 1, "The returned wrapper failed to retain its native object");
        return new WeakReference(wrapper.Lifetime);
    }

    private static void AutomaticOwnership(string mode)
    {
        IntPtr pointer = new IntPtr(1);
        Native.Counts[pointer] = 1;

        if (mode == "automatic-gc" || mode == "native-dispose-gc") {
            WeakReference lease = AcquiredReturn(mode == "native-dispose-gc" ? 2 : -1);
            Collect();
            Invariant.Verify(!lease.IsAlive && Native.Releases == 1 && Native.Disposals == 0, "Finalization ran native cleanup inline");
            NativeRefTypeLifetime.Drain();
        }
        else {
            using Wrapper wrapper = Wrapper.BorrowedReturn(pointer, mode == "automatic-dispose" ? -1 : 2);
            Invariant.Verify(Native.Counts[pointer] == 2 && Native.Adds == 1, "Borrowed return must acquire exactly one reference");
            byte frame = 0;
            Native.CallMethodIndexed(1, pointer, ref frame, 0);

            if (mode == "native-dispose-shutdown") {
                NativeRefTypeLifetime.Shutdown();
                Native.IsBackendAlive = false;
            }
            else if (mode == "native-dispose-failed") {
                Native.FailDispose = true;
                try { wrapper.Dispose(); }
                catch (InvalidOperationException) { }
                Invariant.Verify(Native.Counts[pointer] == 1 && Native.Disposals == 0, "Failed cleanup lost ownership");
                Native.FailDispose = false;
            }
            else if (mode == "native-dispose-release-failed") {
                Native.FailReleaseAfter = Native.Releases;
                try { wrapper.Dispose(); }
                catch (InvalidOperationException) { }
                Invariant.Verify(Native.Counts[pointer] == 1 && Native.Disposals == 1, "Cleanup must precede reference release");
                Native.FailReleaseAfter = -1;
            }

            wrapper.Dispose();
            wrapper.Dispose();
            ExpectDisposed(wrapper.Lifetime.VerifyAlive);
        }

        Invariant.Verify(Native.Adds == 1 && Native.Releases == 2 && Native.Destroyed == 1, "Automatic ownership was not balanced exactly once");
        Invariant.Verify(Native.Disposals == (mode.StartsWith("native-", StringComparison.Ordinal) ? 1 : 0), "Native Dispose must run once before releasing ownership");
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static WeakReference BorrowAndRetain()
    {
        Wrapper borrowed = new Wrapper(new IntPtr(1));
        borrowed.Lifetime.AddReference();
        return new WeakReference(borrowed.Lifetime);
    }

    private static void ExpectDisposed(Action call)
    {
        try { call(); }
        catch (ObjectDisposedException) { return; }
        throw new InvalidOperationException("A wrapper remained usable after disposal or shutdown");
    }

    private static void Disposal(string mode)
    {
        IntPtr pointer = new IntPtr(1);
        Native.Counts[pointer] = 1;
        Wrapper wrapper = mode == "dispose-borrowed" ? new Wrapper(pointer) : Wrapper.Factory(pointer);

        if (mode == "dispose-using") {
            using (wrapper) {
                wrapper.Lifetime.VerifyAlive();
            }
        }
        else if (mode == "dispose-exception") {
            try {
                using (wrapper) {
                    throw new ApplicationException("Leave the resource scope");
                }
            }
            catch (ApplicationException) { }
        }
        else if (mode == "dispose-multiple" || mode == "dispose-failed") {
            wrapper.Lifetime.AddReference();
            wrapper.Lifetime.AddReference();

            if (mode == "dispose-multiple") {
                wrapper.Lifetime.ReleaseReference();
            }
            else {
                Native.FailReleaseAfter = 1;
                try { wrapper.Dispose(); }
                catch (InvalidOperationException) { }
                Invariant.Verify(Native.Releases == 1 && Native.Counts[pointer] == 2, "Failed disposal lost its remaining ownership");
                wrapper.Lifetime.VerifyAlive();
                Native.FailReleaseAfter = -1;
            }

            wrapper.Dispose();
        }
        else if (mode == "dispose-manual") {
            wrapper.Lifetime.ReleaseReference();
            wrapper.Dispose();
        }
        else if (mode == "dispose-aliases") {
            Wrapper borrowed = new Wrapper(pointer);
            borrowed.Lifetime.AddReference();
            Wrapper alias = wrapper;
            alias.Dispose();
            wrapper.Dispose();
            Invariant.Verify(Native.Counts[pointer] == 1 && Native.Releases == 1, "Disposing an alias stole an independent lease");
            borrowed.Lifetime.VerifyAlive();
            borrowed.Dispose();
        }
        else if (mode == "dispose-shutdown") {
            NativeRefTypeLifetime.Shutdown();
            Native.IsBackendAlive = false;
            wrapper.Dispose();
        }
        else if (mode == "dispose-reentrant") {
            Native.Releasing = () => {
                wrapper.Dispose();
                ExpectDisposed(wrapper.Lifetime.VerifyAlive);
                ExpectDisposed(wrapper.Lifetime.AddReference);
            };
            wrapper.Dispose();
            Native.Releasing = null;
        }
        else {
            wrapper.Dispose();
        }

        wrapper.Dispose();
        ExpectDisposed(wrapper.Lifetime.VerifyAlive);
        ExpectDisposed(wrapper.Lifetime.AddReference);
        ExpectDisposed(wrapper.Lifetime.AdoptReference);
        ExpectDisposed(wrapper.Lifetime.ReleaseReference);
        int expected = mode == "dispose-borrowed" ? 0 : mode == "dispose-multiple" || mode == "dispose-failed" ? 3 : mode == "dispose-aliases" ? 2 : 1;
        Invariant.Verify(Native.Releases == expected, "Dispose did not return exactly the wrapper's references");
        Invariant.Verify(Native.Counts[pointer] == (mode == "dispose-borrowed" ? 1 : 0), "Dispose returned a borrowed native reference");
        GC.KeepAlive(wrapper);
        wrapper = null!;
        Collect();
        NativeRefTypeLifetime.Drain();
        Invariant.Verify(Native.Releases == expected && Native.Signals == 0, "GC repeated an explicit disposal");
    }

    private static void Isolation()
    {
        AssemblyLoadContext first = new AssemblyLoadContext("first", true);
        AssemblyLoadContext second = new AssemblyLoadContext("second", true);
        Type a = first.LoadFromAssemblyPath(typeof(Probe).Assembly.Location).GetType("FOnline.Probe")!;
        Type b = second.LoadFromAssemblyPath(typeof(Probe).Assembly.Location).GetType("FOnline.Probe")!;
        a.GetMethod("SeedRetained")!.Invoke(null, null);
        b.GetMethod("SeedRetained")!.Invoke(null, null);
        a.GetMethod("Close")!.Invoke(null, null);
        Invariant.Verify((int)a.GetMethod("Count")!.Invoke(null, null)! == 0, "First backend retained ownership");
        Invariant.Verify((int)b.GetMethod("Count")!.Invoke(null, null)! == 1, "First shutdown released a foreign reference");
        b.GetMethod("Close")!.Invoke(null, null);
        Invariant.Verify((int)b.GetMethod("Count")!.Invoke(null, null)! == 0, "Second backend retained ownership");
        first.Unload();
        second.Unload();
        Collect();
    }

    private static int Main(string[] args)
    {
        string mode = args[0];

        if (mode.StartsWith("automatic-", StringComparison.Ordinal) || mode.StartsWith("native-dispose-", StringComparison.Ordinal)) {
            AutomaticOwnership(mode);
            return 0;
        }

        if (mode.StartsWith("dispose-", StringComparison.Ordinal)) {
            Disposal(mode);
            return 0;
        }

        if (mode == "isolation") {
            Isolation();
            return 0;
        }

        if (mode == "constructor") {
            Native.Counts[new IntPtr(1)] = 1;
            Wrapper constructed = new Wrapper();
            Collect();
            NativeRefTypeLifetime.Drain();
            Invariant.Verify(!Wrapper.FactoryTemporary!.IsAlive, "Factory temporary is unexpectedly rooted");
            Invariant.Verify(Native.Counts[new IntPtr(1)] == 1 && Native.Releases == 0, "Temporary factory wrapper released the shared lease");
            constructed.Lifetime.ReleaseReference();
            GC.KeepAlive(constructed);
            Collect();
            NativeRefTypeLifetime.Drain();
            Invariant.Verify(Native.Releases == 1 && Native.Destroyed == 1, "Constructor ownership was not returned once");
            return 0;
        }

        if (mode == "unowned-release") {
            Native.Counts[new IntPtr(1)] = 1;
            Wrapper borrowed = new Wrapper(new IntPtr(1));
            try { borrowed.Lifetime.ReleaseReference(); }
            catch (InvalidOperationException) {
                Invariant.Verify(Native.Releases == 0, "Borrowed wrapper stole an owner's reference");
                return 0;
            }
            throw new InvalidOperationException("An unowned release was accepted");
        }

        if (mode == "separate-leases") {
            Native.Counts[new IntPtr(1)] = 1;
            Wrapper owner = Wrapper.Factory(new IntPtr(1));
            WeakReference retainedBorrow = BorrowAndRetain();
            Collect();
            Invariant.Verify(!retainedBorrow.IsAlive && Native.Releases == 0, "Borrow finalization returned a reference inline");
            owner.Lifetime.ReleaseReference();
            Invariant.Verify(Native.Counts[new IntPtr(1)] == 1, "Manual owner cleanup stole the retained borrow");
            NativeRefTypeLifetime.Drain();
            Invariant.Verify(Native.Releases == 2 && Native.Destroyed == 1, "Independent leases were not balanced once");
            GC.KeepAlive(owner);
            return 0;
        }

        WeakReference forgotten = Seed(mode);
        int releasedBeforeGC = Native.Releases;
        Collect();
        Invariant.Verify(!forgotten.IsAlive, "Registry rooted the finalizable lease");
        Invariant.Verify(Native.Releases == releasedBeforeGC, "GC released a native reference inline");

        if (mode == "shutdown") {
            Native.Counts[new IntPtr(2)] = 1;
            Wrapper retained = Wrapper.Factory(new IntPtr(2));
            retained.Lifetime.AddReference();
            NativeRefTypeLifetime.Shutdown();
            Native.IsBackendAlive = false;
            Invariant.Verify(Native.Releases == 3 && Native.Destroyed == 2, "Shutdown missed queued or retained references");
            ExpectDisposed(retained.Lifetime.VerifyAlive);
            ExpectDisposed(retained.Lifetime.AddReference);
            ExpectDisposed(retained.Lifetime.ReleaseReference);
            int signals = Native.Signals;
            retained = null!;
            Collect();
            NativeRefTypeLifetime.Drain();
            Invariant.Verify(Native.Signals == signals && Native.Releases == 3, "Late finalization reached a closed backend");
            return 0;
        }

        NativeRefTypeLifetime.Drain();
        int expected = mode == "borrowed" || mode == "failed-add" ? 0 : mode == "multiple" ? 3 : mode == "reacquire" ? 2 : 1;
        Invariant.Verify(Native.Releases == expected, "Wrong number of returned references");
        Invariant.Verify(Native.Destroyed == (expected == 0 || mode == "borrowed-retained" || mode == "reacquire" ? 0 : 1), "Wrong native destruction count");
        NativeRefTypeLifetime.Drain();
        Invariant.Verify(Native.Releases == expected, "A second pump released the reference twice");
        return 0;
    }
}
"""


@pytest.fixture(scope="module")
def ref_type_probe(tmp_path_factory):
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        pytest.skip("dotnet SDK is required for managed RefType GC tests")
    output = tmp_path_factory.mktemp("managed-ref-type-lifetime")
    shutil.copyfile(CORE / "NativeRefTypeLifetime.cs", output / "NativeRefTypeLifetime.cs")
    (output / "Probe.csproj").write_text(PROJECT, encoding="utf-8")
    (output / "Program.cs").write_text(PROBE, encoding="utf-8")
    result = subprocess.run([dotnet, "build", "Probe.csproj", "-c", "Release", "--nologo", "-v:q"],
                            cwd=output, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr
    return dotnet, output / "bin/Release/net10.0/Probe.dll"


@pytest.mark.parametrize("mode", ["forgotten", "borrowed", "borrowed-retained", "reacquire", "manual", "multiple", "failed-add",
                                 "constructor", "unowned-release", "separate-leases", "shutdown", "isolation",
                                 "dispose-using", "dispose-exception", "dispose-borrowed", "dispose-multiple", "dispose-manual",
                                 "dispose-aliases", "dispose-shutdown", "dispose-failed", "dispose-reentrant",
                                 "automatic-dispose", "automatic-gc", "native-dispose-manual", "native-dispose-gc",
                                 "native-dispose-shutdown", "native-dispose-failed", "native-dispose-release-failed"])
def test_native_ref_type_gc_ownership(ref_type_probe, mode):
    dotnet, assembly = ref_type_probe
    env = {**os.environ, "DOTNET_TieredCompilation": "0"}
    result = subprocess.run([dotnet, str(assembly), mode], env=env, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
