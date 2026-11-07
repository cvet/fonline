// Copyable server test harness core; the marker and settings contract is Docs/en/how-to/project/script-test-harness.md
// Suites subscribe to Game.OnTestingInit and call Testing.RegisterTest; game fixtures stay in the suite files
namespace MyGame;

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using FOnline;

public static partial class Testing
{
#if SERVER
    ///@ Setting Server bool Testing.Enabled
    ///@ Setting Server string Testing.Filter
    ///@ Setting Server bool Testing.StopOnFirstFailure
    ///@ Setting Server bool Testing.ExitOnFinish
    ///@ Setting Server int32 Testing.TestTimeout

    ///@ Event Server Game OnTestingInit()

    private const string MarkerPrefix = "[Testing] ";

    public sealed class TestContext
    {
        internal TestContext(int id, string fullName)
        {
            Id = id;
            FullName = fullName;
        }

        public int Id { get; }
        public string FullName { get; }

        // A verdict is recorded; later Pass/Fail calls are ignored
        internal bool Finished;
        // The runner has logged the verdict and moved on
        internal bool Completed;
        internal bool Ok;
        internal string FailText = string.Empty;
        internal TaskCompletionSource? PendingWait;
    }

    private sealed record TestCase(string FullName, Func<TestContext, Task> Run, int TimeoutMs);

    private sealed class RunState
    {
        public readonly List<TestCase> Tests = new List<TestCase>();
        public readonly List<int> Selected = new List<int>();
        public bool Registering;
        public string ConfigError = string.Empty;
        public int Position;
        public TestContext? Current;
        public uint WatchdogId;
        public int Passed;
        public int Failed;
    }

    // Test-process state only: never persisted, and the backend's isolated load context keeps it per engine
    private static RunState Run = new RunState();

    [ModuleInit]
    internal static void InitTesting()
    {
        if (!Settings.Testing.Enabled) {
            return;
        }

        Game.OnStart.Subscribe(StartTesting);
    }

    [Event]
    internal static void StartTesting()
    {
        RunState run = new RunState();
        Run = run;

        // Suites register lazily from the event, so module initialization order does not matter
        run.Registering = true;
        Game.OnTestingInit.Fire();
        run.Registering = false;

        if (Settings.Testing.TestTimeout <= 0) {
            AddConfigError(run, "Testing.TestTimeout must be a positive number of milliseconds");
        }

        for (int i = 0; i < run.Tests.Count; i++) {
            if (MatchesFilter(run.Tests[i].FullName)) {
                run.Selected.Add(i);
            }
        }

        string counts = "registered=" + run.Tests.Count + " selected=" + run.Selected.Count;
        Game.Log(MarkerPrefix + "START " + counts + " filter='" + Settings.Testing.Filter + "'");

        if (run.ConfigError.Length != 0) {
            Game.Log(MarkerPrefix + "ERROR " + run.ConfigError);
            FinishRun();
            return;
        }

        if (run.Selected.Count == 0) {
            Game.Log(MarkerPrefix + "ERROR no registered test matches the filter");
            FinishRun();
            return;
        }

        Game.StartTimeEvent(Milliseconds(1), NextTest);
    }

    // Registers one test; valid only inside a Game.OnTestingInit handler. A timeout of 0 uses Testing.TestTimeout
    public static void RegisterTest(string suite, string name, Action<TestContext> test, int timeoutMs = 0)
    {
        RegisterTest(suite,
                     name,
                     ctx =>
                     {
                         test(ctx);
                         return Task.CompletedTask;
                     },
                     timeoutMs);
    }

    // An asynchronous test finishes when its task completes; the watchdog fails it after the timeout
    public static void RegisterTest(string suite, string name, Func<TestContext, Task> test, int timeoutMs = 0)
    {
        Invariant.Verify(Run.Registering, "Tests register only while Game.OnTestingInit is fired", suite, name);

        string fullName = suite + "." + name;

        // A throwing registration would silently drop the rest of the suite, so problems fail the whole run instead
        if (suite.Length == 0 || name.Length == 0 || suite.Contains('.') || timeoutMs < 0) {
            AddConfigError(Run, "invalid registration '" + fullName + "'");
            return;
        }

        for (int i = 0; i < Run.Tests.Count; i++) {
            if (Run.Tests[i].FullName == fullName) {
                AddConfigError(Run, "duplicate test '" + fullName + "'");
                return;
            }
        }

        Run.Tests.Add(new TestCase(fullName, test, timeoutMs));
    }

    [TimeEvent]
    internal static async Task NextTest()
    {
        RunState run = Run;

        if (run.Failed > 0 && Settings.Testing.StopOnFirstFailure) {
            FinishRun();
            return;
        }

        if (run.Position >= run.Selected.Count) {
            FinishRun();
            return;
        }

        TestCase test = run.Tests[run.Selected[run.Position]];
        TestContext ctx = new TestContext(run.Position, test.FullName);
        run.Current = ctx;

        int timeoutMs = test.TimeoutMs > 0 ? test.TimeoutMs : Settings.Testing.TestTimeout;
        run.WatchdogId = Game.StartTimeEvent(Milliseconds(timeoutMs), WatchTest, ctx.Id);

        try {
            await test.Run(ctx);
        }
        catch (Exception exception) {
            Fail(ctx, "exception: " + exception.GetType().Name + ": " + exception.Message);
        }

        // A task that completes without a verdict has passed; a late completion after the watchdog changes nothing
        if (!ctx.Finished) {
            ctx.Finished = true;
            ctx.Ok = true;
        }

        if (!ctx.Completed) {
            CompleteTest(ctx);
        }
    }

    [TimeEvent]
    internal static void WatchTest(any value)
    {
        TestContext? ctx = Run.Current;
        if (ctx is null || ctx.Id != (int)value || ctx.Completed) {
            return;
        }

        if (!ctx.Finished) {
            ctx.Finished = true;
            ctx.Ok = false;
            ctx.FailText = "timeout";
        }

        // The watchdog event is the one firing, so it must not be stopped again
        Run.WatchdogId = 0;
        CompleteTest(ctx);

        // Releases a test suspended in WaitAsync; its late completion no longer changes the verdict
        TaskCompletionSource? wait = ctx.PendingWait;
        ctx.PendingWait = null;
        wait?.TrySetCanceled();
    }

    private static void CompleteTest(TestContext ctx)
    {
        RunState run = Run;
        ctx.Completed = true;

        if (run.WatchdogId != 0) {
            Game.StopTimeEvent(run.WatchdogId);
            run.WatchdogId = 0;
        }

        if (ctx.Ok) {
            run.Passed++;
            Game.Log(MarkerPrefix + "PASS " + ctx.FullName);
        }
        else {
            run.Failed++;
            Game.Log(MarkerPrefix + "FAIL " + ctx.FullName + ": " + ctx.FailText);
        }

        run.Position++;
        Game.StartTimeEvent(Milliseconds(1), NextTest);
    }

    private static void FinishRun()
    {
        RunState run = Run;
        int selected = run.Selected.Count;
        int executed = run.Passed + run.Failed;
        bool passed = run.ConfigError.Length == 0 && selected > 0 && run.Failed == 0;

        string summary =
            "status=" + (passed ? "passed" : "failed") + " registered=" + run.Tests.Count + " selected=" + selected;
        summary += " passed=" + run.Passed + " failed=" + run.Failed;
        summary += " skipped=" + (run.Tests.Count - selected) + " not_run=" + (selected - executed);
        Game.Log(MarkerPrefix + "COMPLETE " + summary);

        if (Settings.Testing.ExitOnFinish) {
            Game.RequestQuit(passed);
        }
    }

    private static void AddConfigError(RunState run, string message)
    {
        run.ConfigError += (run.ConfigError.Length == 0 ? "" : "; ") + message;
    }

    // Comma-separated tokens; a test is selected when its suite.name contains any token, ignoring case
    private static bool MatchesFilter(string fullName)
    {
        string filter = Settings.Testing.Filter.Trim();
        if (filter.Length == 0) {
            return true;
        }

        string[] tokens = filter.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

        for (int i = 0; i < tokens.Length; i++) {
            if (fullName.Contains(tokens[i], StringComparison.OrdinalIgnoreCase)) {
                return true;
            }
        }

        return false;
    }

    private static timespan Milliseconds(int value)
    {
        return new timespan(value, TimePlace.Milliseconds);
    }

    // Returns the condition so a test can stop at the first failed expectation
    public static bool Expect(TestContext ctx, bool condition, string message)
    {
        if (!condition) {
            Fail(ctx, message);
            return false;
        }

        return true;
    }

    public static bool ExpectEq(TestContext ctx, long actual, long expected, string message)
    {
        return Expect(ctx, actual == expected, message + " (actual " + actual + ", expected " + expected + ")");
    }

    public static bool ExpectStrEq(TestContext ctx, string actual, string expected, string message)
    {
        return Expect(ctx, actual == expected, message + " (actual '" + actual + "', expected '" + expected + "')");
    }

    public static void Fail(TestContext ctx, string message)
    {
        if (ctx.Finished) {
            return;
        }

        ctx.Finished = true;
        ctx.Ok = false;
        ctx.FailText = message;
    }

    public static void Pass(TestContext ctx)
    {
        if (ctx.Finished) {
            return;
        }

        ctx.Finished = true;
        ctx.Ok = true;
    }

    // Suspends an asynchronous test through an Engine time event; the test's own timeout still applies
    public static Task WaitAsync(TestContext ctx, int milliseconds)
    {
        Invariant.Verify(ctx.PendingWait is null, "A test waits for one step at a time", ctx.FullName);

        TaskCompletionSource wait = new TaskCompletionSource();
        ctx.PendingWait = wait;
        Game.StartTimeEvent(Milliseconds(milliseconds), ResumeWait, ctx.Id);
        return wait.Task;
    }

    [TimeEvent]
    internal static void ResumeWait(any value)
    {
        TestContext? ctx = Run.Current;
        if (ctx is null || ctx.Id != (int)value || ctx.Completed) {
            return;
        }

        TaskCompletionSource? wait = ctx.PendingWait;
        ctx.PendingWait = null;
        wait?.SetResult();
    }
#endif
}
