using WinApiUndefGeneration;
using WinApiUndefGeneration.Tests;


static void Check(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException(message);
    }
}

string temporary = Path.Combine(Path.GetTempPath(), $"winapi-undef-generator-{Guid.NewGuid():N}");
Directory.CreateDirectory(temporary);
try
{
    string assemblyPath = typeof(RegistryFixture).Assembly.Location;
    string sdkDirectory = Path.Combine(temporary, "sdk");
    Directory.CreateDirectory(sdkDirectory);
    File.WriteAllText(
        Path.Combine(sdkDirectory, "fixture.h"),
        "#define Foo FooW\n#define InterlockedCompareExchange8 _InterlockedCompareExchange8\n#define InterlockedCompareExchangeAcquire8 _InterlockedCompareExchange8_acq\n#define InterlockedCompareExchangeRelease8 _InterlockedCompareExchange8_rel\n#define InterlockedCompareExchangeNoFence8 _InterlockedCompareExchange8_nf\n#define GetProcessMemoryInfo GetProcessMemoryInfo\n#define INCLUDE_GUARD\n#define CONSTANT 1\n");

    string existingPath = Path.Combine(temporary, "existing.inc");
    File.WriteAllText(
        existingPath,
        "#if FO_WINDOWS\n// #undef GetExceptionCode — required\n// #undef GetProcessMemoryInfo — first-party memory statistics\n#undef GetProcessMemoryInfo\n#undef ZExistingMacro\n#endif\n");
    string firstOutput = Path.Combine(temporary, "first.inc");
    string secondOutput = Path.Combine(temporary, "second.inc");

    WinApiUndefGenerator.Generate(assemblyPath, sdkDirectory, existingPath, firstOutput);
    WinApiUndefGenerator.Generate(assemblyPath, sdkDirectory, existingPath, secondOutput);

    string generated = File.ReadAllText(firstOutput);
    Check(generated.Contains("#undef Foo\n", StringComparison.Ordinal), "Generic A/W alias is missing");
    Check(generated.Contains("#undef InterlockedCompareExchange8\n", StringComparison.Ordinal), "PInvoke macro is missing");
    Check(!generated.Contains("#undef InterlockedCompareExchangeAcquire8\n", StringComparison.Ordinal), "Acquire8 macro was added without PInvoke evidence");
    Check(!generated.Contains("#undef InterlockedCompareExchangeRelease8\n", StringComparison.Ordinal), "Release8 macro was added without PInvoke evidence");
    Check(!generated.Contains("#undef InterlockedCompareExchangeNoFence8\n", StringComparison.Ordinal), "NoFence8 macro was added without PInvoke evidence");
    Check(generated.Contains("#undef Foo\n#undef InterlockedCompareExchange8\n#undef ZExistingMacro\n", StringComparison.Ordinal), "Active #undef entries are not one globally sorted list");
    Check(!generated.Contains("#undef Other\n", StringComparison.Ordinal), "PInvoke without a macro was included");
    Check(!generated.Contains("#undef InlineOnly\n", StringComparison.Ordinal), "Inline-only method was included");
    Check(!generated.Contains("#undef INCLUDE_GUARD\n", StringComparison.Ordinal), "Include guard was included");
    Check(!generated.Contains("#undef CONSTANT\n", StringComparison.Ordinal), "Object-like constant was included");
    Check(!generated.Contains("#undef GetExceptionCode\n", StringComparison.Ordinal), "Allowlisted macro became active");
    Check(!generated.Contains("#undef GetProcessMemoryInfo\n", StringComparison.Ordinal), "Compile-proven macro became active");
    Check(generated.Contains("// #undef GetExceptionCode", StringComparison.Ordinal), "Allowlist comment was lost");
    Check(generated == File.ReadAllText(secondOutput), "Generation is not idempotent");
    Console.WriteLine("GeneratorTests passed");
    return 0;
}
finally
{
    Directory.Delete(temporary, recursive: true);
}
