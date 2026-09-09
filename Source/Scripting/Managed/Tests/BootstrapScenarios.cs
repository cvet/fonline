using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using FOnline;

internal static class BootstrapScenarios
{
    internal static bool FailStaticConstructor;

    internal static void RunIsolated(string scenario)
    {
        string executable = Environment.ProcessPath ?? throw new InvalidOperationException("Missing test executable path");
        var start = new ProcessStartInfo(executable)
        {
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        if (Path.GetFileNameWithoutExtension(executable) == "dotnet")
        {
            start.ArgumentList.Add(typeof(BootstrapScenarios).Assembly.Location);
        }
        start.ArgumentList.Add(scenario);
        using Process process = Process.Start(start) ?? throw new InvalidOperationException("Could not start bootstrap scenario");
        var output = process.StandardOutput.ReadToEndAsync();
        var errors = process.StandardError.ReadToEndAsync();
        if (!process.WaitForExit(30000))
        {
            process.Kill(entireProcessTree: true);
            throw new InvalidOperationException("Bootstrap scenario timed out: " + scenario);
        }
        if (process.ExitCode != 0)
        {
            throw new InvalidOperationException(output.GetAwaiter().GetResult() + errors.GetAwaiter().GetResult());
        }
    }

    internal static int Run(string scenario)
    {
        string previousDirectory = Directory.GetCurrentDirectory();
        string temporaryDirectory = Path.Combine(Path.GetTempPath(), "FOnlineBootstrap_" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(temporaryDirectory);
            Directory.SetCurrentDirectory(temporaryDirectory);
            if (scenario == "source-tree")
            {
                Directory.CreateDirectory("Scripts");
                File.WriteAllText(Path.Combine("Scripts", "OwnershipProbe.fos"), "namespace OwnershipProbe {}\n");
            }
            FailStaticConstructor = scenario == "static-failure";
            Invoke("InitializeEarly");
            bool failed = false;
            try
            {
                Invoke("Initialize");
            }
            catch (TargetInvocationException exception) when (FailStaticConstructor && exception.GetBaseException().Message == "Bootstrap failure sentinel")
            {
                failed = true;
            }
            if (FailStaticConstructor)
            {
                if (!failed || OwnershipProbe.Initialized != 0)
                {
                    throw new InvalidOperationException("Static constructor failure must stop module initialization");
                }
            }
            else if (OwnershipProbe.Initialized != 1 ||
                     !Native.RegisteredFunctions.SequenceEqual(new[] { "OwnershipProbe::Registered" }) ||
                     !Native.RegisteredRemoteCalls.SequenceEqual(new[] { "RemoteProbe" }))
            {
                throw new InvalidOperationException("Managed initialization and registrations depend on the working directory");
            }
            return 0;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine(exception);
            return 1;
        }
        finally
        {
            Directory.SetCurrentDirectory(previousDirectory);
            Directory.Delete(temporaryDirectory, recursive: true);
        }
    }

    private static void Invoke(string method)
    {
        MethodInfo entry = typeof(Initializator).GetMethod(method, BindingFlags.Static | BindingFlags.NonPublic)
            ?? throw new InvalidOperationException("Missing initializer entrypoint: " + method);
        entry.Invoke(null, null);
    }
}

internal sealed class BootstrapHandlerAttribute : Attribute { }

internal static class OwnershipProbe
{
    internal static int Initialized;

    [ModuleInit]
    public static void Initialize() { Initialized++; }

    [ScriptFuncRegistrar]
    public static void Register() => ScriptFuncRegistration.RegisterAttributedScriptFuncs(typeof(BootstrapHandlerAttribute), "BootstrapHandler");

    [BootstrapHandler]
    public static void Registered() { }

    [ServerRemoteCall]
    public static void RemoteProbe() { }
}

internal static class ConstructorFailureProbe
{
    static ConstructorFailureProbe()
    {
        if (BootstrapScenarios.FailStaticConstructor)
        {
            throw new InvalidOperationException("Bootstrap failure sentinel");
        }
    }
}
