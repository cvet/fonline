namespace FOnline;

using System.Threading.Tasks;

public static partial class Game
{
    // The timer completes on an engine worker; ScriptSynchronizationContext also returns late-registered
    // and nested Task continuations to the owning engine's script pump
    public static Task YieldAsync(int ms)
    {
        ScriptSynchronizationContext.VerifyCanYield();
        var completion = new TaskCompletionSource<bool>();
        StartTimeEvent(new timespan(ms * 1_000_000L), () => completion.TrySetResult(true));
        return completion.Task;
    }
}
