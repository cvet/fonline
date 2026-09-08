from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess

import pytest


ENGINE = Path(__file__).resolve().parents[2]
CORE = ENGINE / "Source/Scripting/Managed/CoreScripts"
PROJECT = """<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup><TargetFramework>net10.0</TargetFramework><OutputType>Exe</OutputType><Nullable>enable</Nullable><ImplicitUsings>disable</ImplicitUsings><TreatWarningsAsErrors>true</TreatWarningsAsErrors><NoWarn>$(NoWarn);CS8981</NoWarn></PropertyGroup>
</Project>
"""

PROBE_SOURCE = r"""
using System;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using FOnline;

internal static class Program
{
    private static readonly TaskCompletionSource<bool> Resume = new();
    private static int stage;
    private static int returned;
    private static readonly TaskCompletionSource<bool> Second = new();
    private static int ownerThread;
    private static int resumedThread;
    private static int unrelatedStage;
    private static bool runModule;


    private static int Main(string[] args)
    {
        AppDomain.CurrentDomain.UnhandledException += (_, e) => Console.WriteLine("UNHANDLED terminating=" + e.IsTerminating + " recorded=" + Game.GetGlobalExceptionCount() + " " + e.ExceptionObject);
        string boundary = args[0];
        string kind = args[1];
        ownerThread = Environment.CurrentManagedThreadId;
        if (kind.StartsWith("guard-")) return CheckInitializationGuard(kind == "guard-reject");
        if (kind == "late-result") return LateResult(boundary);
        if (kind == "nested") return Nested();
        if (kind == "shutdown") return Shutdown();
        if (kind == "late-yield") return LateYield();
        if (kind == "reject-yield") return RejectYield(boundary);

        if (kind == "identity")
        {
            var first = new CallbackOwner();
            var second = new CallbackOwner();
            Func<Task> firstDelegate = first.Run;
            Func<Task> firstAgain = first.Run;
            Func<Task> secondDelegate = second.Run;
            Func<Task<int>> withResult = first.Run;
            string key = Native.GetDelegateKey(firstDelegate);
            bool stable = key == Native.GetDelegateKey(firstAgain);
            bool distinct = key != Native.GetDelegateKey(secondDelegate);
            bool covariant = key == Native.GetDelegateKey(withResult);
            Console.WriteLine("IDENTITY stable=" + stable + " distinct=" + distinct + " covariant=" + covariant);
            return stable && distinct && covariant ? 0 : 26;
        }
        if (kind == "generic-result")
        {
            object? result = Native.InvokeCallback((Func<Task<int>>)(() => Task.FromResult(42)), Array.Empty<object>());
            Console.WriteLine("GENERIC_RESULT " + result);
            return result is int value && value == 42 ? 0 : 25;
        }
        bool returnsTask = kind != "void";
        Delegate callback = kind == "covariant-task" ? (Func<Task>)RunGenericTask : returnsTask ? (Func<Task>)RunTask : (Action)RunVoid;
        if (kind == "covariant-task")
        {
            _ = Task.Run(() =>
            {
                Thread.Sleep(2000);
                if (Volatile.Read(ref returned) == 0)
                {
                    Console.WriteLine("CALLBACK_BLOCKED_BEFORE_RESUME");
                    Environment.Exit(24);
                }
            });
        }
        Console.WriteLine("CONTEXT " + (SynchronizationContext.Current == null ? "null" : "present"));
        if (boundary == "event")
        {
            Console.WriteLine("EVENT_RESULT " + Native.InvokeEvent(callback, false, Array.Empty<object>()));
        }
        else
        {
            Console.WriteLine("CALLBACK_RESULT " + (Native.InvokeCallback(callback, Array.Empty<object>()) == null ? "null" : "value"));
        }
        Volatile.Write(ref returned, 1);
        Console.WriteLine("RETURNED stage=" + stage + " recorded=" + Game.GetGlobalExceptionCount());
        if (stage != 1 || Resume.Task.IsCompleted) return 20;
        Resume.SetResult(true);
        if (returnsTask || kind == "void")
        {
            if (!PumpUntil(() => Game.GetGlobalExceptionCount() == 1)) return 21;
            Console.WriteLine("TASK_OBSERVED stage=" + stage + " recorded=" + Game.GetGlobalExceptionCount());
            return stage == 2 && resumedThread == ownerThread ? 0 : 22;
        }
        Thread.Sleep(5000);
        Console.WriteLine("VOID_UNEXPECTEDLY_SURVIVED");
        return 23;
    }


    private static int CheckInitializationGuard(bool expectRejection)
    {
        bool rejected = false;
        try
        {
            typeof(Initializator).GetMethod("InitializeEarly", BindingFlags.NonPublic | BindingFlags.Static)!.Invoke(null, null);
        }
        catch (TargetInvocationException ex)
        {
            Console.WriteLine("GUARD_DIAGNOSTIC " + ex.InnerException?.Message);
            rejected = ex.InnerException is InvalidOperationException && ex.InnerException.Message.StartsWith("Async void script methods are not supported; return Task:");
        }
        int registered = ScriptFuncRegistration.Calls + RemoteCallScriptFuncs.Calls;
        Console.WriteLine("GUARD rejected=" + rejected + " registrations=" + registered);
        return rejected == expectRejection && registered == (expectRejection ? 0 : 2) ? 0 : 38;
    }

    private static bool PumpUntil(Func<bool> condition)
    {
        return SpinWait.SpinUntil(() => { Native.PumpContinuations(); return condition(); }, 5000);
    }

    private static async Task MarkUnrelated()
    {
        await Second.Task;
        unrelatedStage = 1;
    }

    private static async Task<int> DelayedResult()
    {
        stage = 1;
        await Resume.Task;
        resumedThread = Environment.CurrentManagedThreadId;
        stage = 2;
        return 42;
    }

    private static async Task<EventResult> DelayedEventResult()
    {
        await DelayedResult();
        return EventResult.StopChain;
    }

    private static int LateResult(string boundary)
    {
        Native.InvokeCallback((Func<Task>)MarkUnrelated, Array.Empty<object>());
        Second.SetResult(true);
        Thread worker = new(() =>
        {
            if (!SpinWait.SpinUntil(() => Volatile.Read(ref stage) == 1, 5000)) return;
            Resume.SetResult(true);
        });
        worker.Start();
        object? result = boundary == "event"
            ? Native.InvokeEvent((Func<Task<EventResult>>)DelayedEventResult, true, Array.Empty<object>())
            : Native.InvokeCallback((Func<Task<int>>)DelayedResult, Array.Empty<object>());
        worker.Join();
        bool correctResult = boundary == "event" ? result is EventResult.StopChain : result is int value && value == 42;
        bool isolated = unrelatedStage == 0;
        bool owner = resumedThread == ownerThread;
        Native.PumpContinuations();
        Console.WriteLine("LATE_RESULT correct=" + correctResult + " owner=" + owner + " isolated=" + isolated + " unrelated=" + unrelatedStage);
        return correctResult && owner && isolated && unrelatedStage == 1 ? 0 : 30;
    }

    private static async Task NestedTask()
    {
        stage = 1;
        await Resume.Task;
        if (Environment.CurrentManagedThreadId != ownerThread) throw new Exception("first continuation left owner");
        stage = 2;
        await Second.Task;
        resumedThread = Environment.CurrentManagedThreadId;
        stage = 3;
        throw new InvalidOperationException("nested-delayed-fault");
    }

    private static int Nested()
    {
        Native.InvokeCallback((Func<Task>)NestedTask, Array.Empty<object>());
        Thread first = new(() => Resume.SetResult(true));
        first.Start(); first.Join();
        bool deferred = stage == 1;
        if (!PumpUntil(() => stage == 2)) return 31;
        Thread second = new(() => Second.SetResult(true));
        second.Start(); second.Join();
        deferred &= stage == 2;
        if (!PumpUntil(() => Game.GetGlobalExceptionCount() == 1)) return 32;
        Console.WriteLine("NESTED deferred=" + deferred + " stage=" + stage + " owner=" + (resumedThread == ownerThread));
        return deferred && stage == 3 && resumedThread == ownerThread ? 0 : 33;
    }

    private static async Task WaitForShutdown(Task task)
    {
        await task;
        stage++;
    }

    private static int Shutdown()
    {
        Native.InvokeCallback((Func<Task>)(() => WaitForShutdown(Resume.Task)), Array.Empty<object>());
        Native.InvokeCallback((Func<Task>)(() => WaitForShutdown(Second.Task)), Array.Empty<object>());
        Resume.SetResult(true);
        Native.ShutdownContinuations();
        Second.SetResult(true);
        Native.PumpContinuations();
        Console.WriteLine("SHUTDOWN stage=" + stage + " recorded=" + Game.GetGlobalExceptionCount());
        return stage == 0 && Game.GetGlobalExceptionCount() == 0 ? 0 : 34;
    }

    private static Task RegisterLateYield()
    {
        var awaiter = Game.YieldAsync(1).GetAwaiter();
        if (awaiter.IsCompleted) throw new Exception("timer completed before explicit fire");
        Game.FireTimer();
        stage = 1;
        awaiter.UnsafeOnCompleted(() => { resumedThread = Environment.CurrentManagedThreadId; stage = 2; });
        return Task.CompletedTask;
    }

    private static int LateYield()
    {
        Native.InvokeCallback((Func<Task>)RegisterLateYield, Array.Empty<object>());
        bool deferred = stage == 1;
        if (!PumpUntil(() => stage == 2)) return 35;
        Console.WriteLine("LATE_YIELD deferred=" + deferred + " owner=" + (resumedThread == ownerThread) + " timers=" + Game.TimerCount);
        return deferred && resumedThread == ownerThread && Game.TimerCount == 1 ? 0 : 36;
    }

    private static async Task<int> YieldResult()
    {
        await Game.YieldAsync(1);
        return 42;
    }

    private static async Task<EventResult> YieldEventResult()
    {
        await Game.YieldAsync(1);
        return EventResult.ContinueChain;
    }

    [ModuleInit]
    public static void ModuleYield()
    {
        if (runModule) Game.YieldAsync(1);
    }

    private static int RejectYield(string boundary)
    {
        bool rejected = false;
        try
        {
            if (boundary == "event")
            {
                rejected = Native.InvokeEvent((Func<Task<EventResult>>)YieldEventResult, true, Array.Empty<object>()) == EventResult.StopChain;
            }
            else if (boundary == "module")
            {
                runModule = true;
                typeof(Initializator).GetMethod("Initialize", BindingFlags.NonPublic | BindingFlags.Static)!.Invoke(null, null);
            }
            else Native.InvokeCallback((Func<Task<int>>)YieldResult, Array.Empty<object>());
        }
        catch (Exception ex)
        {
            rejected = ex.ToString().Contains("A synchronous script callback cannot yield an engine timer");
        }
        Console.WriteLine("YIELD_REJECTED " + rejected + " timers=" + Game.TimerCount);
        return rejected && Game.TimerCount == 0 ? 0 : 37;
    }

    private sealed class CallbackOwner
    {
        public Task<int> Run() => Task.FromResult(42);
    }

    private static async void RunVoid()
    {
        stage = 1;
        await Resume.Task;
        stage = 2;
        resumedThread = Environment.CurrentManagedThreadId;
        throw new InvalidOperationException("delayed-async-void-probe");
    }

    private static async Task<int> RunGenericTask()
    {
        stage = 1;
        await Resume.Task;
        stage = 2;
        resumedThread = Environment.CurrentManagedThreadId;
        throw new InvalidOperationException("delayed-covariant-task-probe");
    }

    private static async Task RunTask()
    {
        stage = 1;
        await Resume.Task;
        stage = 2;
        resumedThread = Environment.CurrentManagedThreadId;
        throw new InvalidOperationException("delayed-task-probe");
    }
}

namespace FOnline
{
    public class Entity { }
    public readonly struct timespan { public timespan(long value) { } }
    public static partial class Game
    {
        private static Action? timer;
        public static int TimerCount;
        public static void StartTimeEvent(timespan duration, Action callback) { TimerCount++; timer = callback; }
        public static void FireTimer() => (timer ?? throw new Exception("timer missing"))();
    }
    public static class ScriptFuncRegistration { public static int Calls; public static void RegisterEngineAttributeFuncs() { Calls++; } }
    public static class RemoteCallScriptFuncs { public static int Calls; public static void RegisterRemoteCalls() { Calls++; } }

    public enum GameProperty { Value }
    public enum ModifierEvent { Value }
    public enum ModifierScope { Value }
    public enum CritterProperty { Value }
    public enum EventResult { ContinueChain, StopChain }
    public readonly struct hstring
    {
        private readonly string value;
        private hstring(string value) { this.value = value; }
        public static hstring FromString(string value) => new(value);
        public override string ToString() => value;
    }
}
"""


@pytest.fixture(scope="module")
def callback_probe(tmp_path_factory):
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        pytest.skip("dotnet SDK is required for actual managed callback dispatch")
    output = tmp_path_factory.mktemp("managed-callback-dispatch")
    return build_probe(dotnet, output, PROBE_SOURCE)


def build_probe(dotnet, output, probe_source):
    for name in ("Native.cs", "ScriptInvoke.cs", "Attributes.cs", "Verify.cs", "Async.cs", "ScriptSynchronizationContext.cs", "Initializator.cs"):
        source = (CORE / name).read_text(encoding="utf-8")
        if name == "Native.cs":
            # External logging and native continuation entry are the only substituted boundaries
            declaration = "[MethodImpl(MethodImplOptions.InternalCall)]\n        internal static extern void Log(string text);"
            assert source.count(declaration) == 1
            source = source.replace(declaration, 'internal static void Log(string text) => Console.WriteLine("ENGINE_RECORDED " + text);')
            continuation = "[MethodImpl(MethodImplOptions.InternalCall)]\n        private static extern string? RunScriptContinuationInternal(Action continuation);"
            assert source.count(continuation) == 1
            source = source.replace(continuation, "private static string? RunScriptContinuationInternal(Action continuation) { continuation(); return null; }")
        (output / name).write_text(source, encoding="utf-8")
    (output / "Probe.csproj").write_text(PROJECT, encoding="utf-8")
    (output / "Program.cs").write_text(probe_source, encoding="utf-8")
    result = subprocess.run([dotnet, "build", "Probe.csproj", "--nologo", "-v:minimal"], cwd=output, capture_output=True, text=True, timeout=120)
    (output / "build.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    return dotnet, output


def disable_core_dump():
    if os.name == "posix":
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run_probe(callback_probe, boundary, kind):
    dotnet, output = callback_probe
    command = [dotnet, str(output / "bin/Debug/net10.0/Probe.dll"), boundary, kind]
    result = subprocess.run(command, cwd=output, capture_output=True, text=True, timeout=15,
                            preexec_fn=disable_core_dump if os.name == "posix" else None)
    (output / (boundary + "-" + kind + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
    return result


@pytest.mark.parametrize("boundary", ["event", "callback"])
def test_context_accounts_for_delayed_void_fault(callback_probe, boundary):
    result = run_probe(callback_probe, boundary, "void")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "RETURNED stage=1 recorded=0" in result.stdout
    assert "TASK_OBSERVED stage=2 recorded=1" in result.stdout
    assert "delayed-async-void-probe" in result.stdout
    assert "UNHANDLED" not in result.stdout


@pytest.mark.parametrize("boundary", ["event", "callback"])
def test_task_delayed_fault_is_recorded_after_dispatch_returns(callback_probe, boundary):
    result = run_probe(callback_probe, boundary, "task")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "RETURNED stage=1 recorded=0" in result.stdout
    assert "TASK_OBSERVED stage=2 recorded=1" in result.stdout
    assert "UNHANDLED" not in result.stdout


def test_task_delegate_covariance_does_not_block_callback(callback_probe):
    result = run_probe(callback_probe, "callback", "covariant-task")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "RETURNED stage=1 recorded=0" in result.stdout
    assert "TASK_OBSERVED stage=2 recorded=1" in result.stdout


def test_generic_task_delegate_preserves_callback_result(callback_probe):
    result = run_probe(callback_probe, "callback", "generic-result")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "GENERIC_RESULT 42" in result.stdout


def test_callback_identity_preserves_target_and_covariant_bindings(callback_probe):
    result = run_probe(callback_probe, "callback", "identity")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "IDENTITY stable=True distinct=True covariant=True" in result.stdout


@pytest.mark.parametrize("boundary", ["event", "callback"])
def test_late_result_wait_keeps_owner_and_does_not_drain_other_callbacks(callback_probe, boundary):
    result = run_probe(callback_probe, boundary, "late-result")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "LATE_RESULT correct=True owner=True isolated=True unrelated=1" in result.stdout


def test_nested_awaits_return_to_owner_and_record_one_fault(callback_probe):
    result = run_probe(callback_probe, "callback", "nested")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "NESTED deferred=True stage=3 owner=True" in result.stdout


def test_shutdown_discards_queued_work_and_late_posts(callback_probe):
    result = run_probe(callback_probe, "callback", "shutdown")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "SHUTDOWN stage=0 recorded=0" in result.stdout


def test_yield_completed_before_awaiter_registration_still_returns_to_owner(callback_probe):
    result = run_probe(callback_probe, "callback", "late-yield")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "LATE_YIELD deferred=True owner=True timers=1" in result.stdout


@pytest.mark.parametrize("boundary", ["event", "callback", "module"])
def test_synchronous_callback_rejects_yield_before_registering_timer(callback_probe, boundary):
    result = run_probe(callback_probe, boundary, "reject-yield")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "YIELD_REJECTED True timers=0" in result.stdout


@pytest.mark.parametrize("kind", ["static", "instance", "lambda", "task-control"])
def test_initialize_early_rejects_async_void_before_registration(callback_probe, tmp_path, kind):
    dotnet, _ = callback_probe
    source = PROBE_SOURCE
    if kind != "static":
        begin = source.index("    private static async void RunVoid()")
        end = source.index("    private static async Task<int> RunGenericTask()", begin)
        source = source[:begin] + "    private static void RunVoid() { }\n\n" + source[end:]
    if kind == "instance":
        source += "\npublic class InstanceCallback { public async void Handler() { await Task.Yield(); } }\n"
    elif kind == "lambda":
        source += "\npublic class LambdaCallback { public Action Create() => async () => { await Task.Yield(); }; }\n"
    probe = build_probe(dotnet, tmp_path, source)
    reject = kind != "task-control"
    result = run_probe(probe, "callback", "guard-reject" if reject else "guard-allow")
    assert result.returncode == 0, result.stdout + result.stderr
    expected = "GUARD rejected=True registrations=0" if reject else "GUARD rejected=False registrations=2"
    assert expected in result.stdout
