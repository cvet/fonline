// Example suite for the copyable harness: registration, expectations and an asynchronous test with a time step
// A game suite keeps its own fixtures, content ids and cleanup in files like this one
namespace MyGame;

using System;
using System.Threading.Tasks;
using FOnline;

public static class Test_HarnessExample
{
#if SERVER
    [ModuleInit]
    internal static void InitTestHarnessExample()
    {
        Game.OnTestingInit.Subscribe(OnTestingInit);
    }

    [Event]
    internal static void OnTestingInit()
    {
        Testing.RegisterTest("harness_example", "split_drops_empty_entries", SplitDropsEmptyEntries);
        Testing.RegisterTest("harness_example",
                             "awaited_time_step_completes_the_test",
                             AwaitedTimeStepCompletesTheTest,
                             2000);
    }

    private static void SplitDropsEmptyEntries(Testing.TestContext ctx)
    {
        string[] parts = "alpha,,beta".Split(',', StringSplitOptions.RemoveEmptyEntries);
        if (!Testing.ExpectEq(ctx, parts.Length, 2, "Split entry count")) {
            return;
        }

        Testing.ExpectStrEq(ctx, parts[1], "beta", "Second entry");
    }

    private static async Task AwaitedTimeStepCompletesTheTest(Testing.TestContext ctx)
    {
        await Testing.WaitAsync(ctx, 50);
        Testing.Pass(ctx);
    }
#endif
}
