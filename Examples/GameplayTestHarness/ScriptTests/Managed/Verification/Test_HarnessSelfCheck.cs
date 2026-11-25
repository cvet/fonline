// Negative controls for the harness, registered only when the stand-in host selects them with HarnessHost.Mode
namespace MyGame;

using System;
using System.Threading.Tasks;
using FOnline;

public static class Test_HarnessSelfCheck
{
    [ModuleInit]
    internal static void InitTestHarnessSelfCheck()
    {
        Game.OnTestingInit.Subscribe(OnTestingInit);
    }

    [Event]
    internal static void OnTestingInit()
    {
        switch (Settings.HarnessHost.Mode) {
        case "faults":
            Testing.RegisterTest("selfcheck", "expectation_fails", ExpectationFails);
            Testing.RegisterTest("selfcheck", "exception_fails", ExceptionFails);
            Testing.RegisterTest("selfcheck", "suspended_step_times_out", SuspendedStepTimesOut, 100);
            Testing.RegisterTest("selfcheck", "unfinished_task_times_out", UnfinishedTaskTimesOut, 100);
            Testing.RegisterTest("selfcheck", "first_verdict_wins", FirstVerdictWins);
            break;
        case "duplicate":
            Testing.RegisterTest("harness_example", "split_drops_empty_entries", FirstVerdictWins);
            break;
        case "invalid":
            Testing.RegisterTest("self.check", "dotted_suite_is_rejected", FirstVerdictWins);
            break;
        }
    }

    private static void ExpectationFails(Testing.TestContext ctx)
    {
        Testing.ExpectEq(ctx, 1, 2, "Deliberate mismatch");
    }

    private static void ExceptionFails(Testing.TestContext ctx)
    {
        throw new InvalidOperationException("Deliberate failure");
    }

    private static async Task SuspendedStepTimesOut(Testing.TestContext ctx)
    {
        await Testing.WaitAsync(ctx, 10000);
        Testing.Fail(ctx, "The step resumed after the watchdog");
    }

    private static Task UnfinishedTaskTimesOut(Testing.TestContext ctx)
    {
        return new TaskCompletionSource().Task;
    }

    private static void FirstVerdictWins(Testing.TestContext ctx)
    {
        Testing.Pass(ctx);
        Testing.Fail(ctx, "A later failure must not replace the first verdict");
    }
}
