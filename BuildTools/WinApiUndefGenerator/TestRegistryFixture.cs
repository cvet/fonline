using System.Runtime.InteropServices;

namespace WinApiUndefGeneration.Tests;

public static class RegistryFixture
{
    [DllImport("kernel32.dll")]
    public static extern void FooA();

    [DllImport("kernel32.dll")]
    public static extern void FooW();

    [DllImport("kernel32.dll")]
    public static extern void InterlockedCompareExchange8();

    [DllImport("kernel32.dll")]
    public static extern void Other();

    [DllImport("kernel32.dll")]
    public static extern void GetProcessMemoryInfo();

    public static void InlineOnly()
    {
    }
}
