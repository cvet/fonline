namespace FOnline;

public static partial class Game
{
    public static void RunScriptGC()
    {
        System.GC.Collect();
        System.GC.WaitForPendingFinalizers();
        System.GC.Collect();
    }
}
