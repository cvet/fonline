namespace FOnline;

using System.Threading.Tasks;

public static class ScriptTask
{
    // An engine timer, not Task.Delay: it completes on an engine worker, ScriptSynchronizationContext returns the
    // continuation to the owning engine's script pump, and a synchronous callback is refused instead of deadlocking
    public static Task Delay(int ms)
    {
        return Delay(new timespan(ms, 2));
    }

    public static Task Delay(timespan duration)
    {
        ScriptSynchronizationContext.VerifyCanYield();
        var completion = new TaskCompletionSource<bool>();
        Game.StartTimeEvent(duration, () => completion.TrySetResult(true));
        return completion.Task;
    }
}
