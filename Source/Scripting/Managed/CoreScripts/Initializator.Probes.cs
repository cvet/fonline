namespace FOnline;

public static partial class Initializator
{
    // A client no test suite runs on (a browser, a device) qualifies its native-to-managed transports on request
    static partial void RunStartupProbes()
    {
        if (Settings.ManagedScript.InteropProbeOnStart) {
            InteropProbe.LogTransportChecks();
        }
    }
}
