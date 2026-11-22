using WinApiUndefGeneration;
using WinApiUndefGeneration.Tests;


static void Check(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException(message);
    }
}

static string Header(string body)
{
    // Raw literals inherit checkout line endings; construct LF fixtures before explicit CRLF conversion
    return body.ReplaceLineEndings("\n") + "\n";
}

static void CheckHeaderCase(string temporary, string assemblyPath, string existingPath, string name, string header, bool hasFoo, bool hasOther)
{
    string sdkDirectory = Path.Combine(temporary, "headers", name, "sdk");
    Directory.CreateDirectory(sdkDirectory);
    File.WriteAllText(Path.Combine(sdkDirectory, "fixture.h"), header);

    string outputPath = Path.Combine(temporary, "headers", name, "output.inc");
    WinApiUndefGenerator.Generate(assemblyPath, sdkDirectory, existingPath, outputPath);
    string generated = File.ReadAllText(outputPath);
    Check(generated.Contains("#undef Foo\n", StringComparison.Ordinal) == hasFoo, $"{name}: Foo discovery mismatch");
    Check(generated.Contains("#undef Other\n", StringComparison.Ordinal) == hasOther, $"{name}: Other discovery mismatch");
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
    string headerExistingPath = Path.Combine(temporary, "headers.inc");
    File.WriteAllText(headerExistingPath, "#if FO_WINDOWS\n#endif\n");
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

    (string Name, string Header, bool HasFoo, bool HasOther)[] headerCases =
    {
        ("clean", Header("#define Foo FooW"), true, false),
        ("block_comment", Header("""
            #define Foo FooW
            /*
            #define Other Other
            */
            """), true, false),
        ("continued_comment_lf", Header("""
            #define Foo FooW
            // documentation \
            #define Other Other
            """), true, false),
        ("continued_comment_crlf", Header("""
            #define Foo FooW
            // documentation \
            #define Other Other
            """).Replace("\n", "\r\n"), true, false),
        ("continued_macro_body", Header("""
            #define Foo FooW
            #define KEEP_CONTINUED \
            #define Other Other
            """), true, false),
        ("raw_literal", Header("""
            #define Foo FooW
            const char* documentation = R"tag(
            #define Other Other
            )tag";
            """), true, false),
        ("leading_comment", Header("/* SDK alias */ #define Foo FooW"), true, false),
        ("genuine_other", Header("""
            #define Foo FooW
            #define Other Other
            """), true, true),
        ("split_directive", Header("""
            #def\
            ine Fo\
            o FooW
            """), true, false),
        ("comment_separator", Header("#/**/define/**/Foo FooW"), true, false),
        ("function_macro", Header("#define Foo(value) FooW(value)"), true, false),
        ("prefixed_raw_literal", Header("""
            #define Foo FooW
            const auto documentation = u8R"SDK(
            #define Other Other
            )SDK";
            """), true, false),
        ("raw_closing_token", Header("""
            #define Foo FooW
            const auto documentation = R"tag(
            )tag" #define Other Other
            """), true, false),
        ("quoted_continuation", Header("""
            #define Foo FooW
            const auto documentation = "text\
            #define Other Other";
            """), true, false),
        ("quoted_prefix", Header("""
            #define Foo FooW
            "text\
            #define Other Other";
            """), true, false),
        ("nonleading_directive", Header("""
            #define Foo FooW
            int token; /* comment */ #define Other Other
            """), true, false),
        ("split_comment_delimiter", Header("""
            #define Foo FooW
            /\
            * documentation
            #define Other Other
            */
            """), true, false),
        ("literal_prefix", Header("""
            #define Foo FooW
            "" #define Other Other
            'x' #define Other Other
            """), true, false),
        ("literal_splice", Header("""
            #define Foo FooW
            ""\
            #define Other Other
            """), true, false),
        ("raw_splice_closing", Header("""
            #define Foo FooW
            const auto text = R"tag(
            )ta\
            g"
            #define Other Other
            )tag";
            """), true, false),
        ("digit_separator_comment", Header("""
            #define Foo FooW
            const auto count = 1'000 /*
            #define Other Other
            */;
            """), true, false),
    };

    foreach ((string name, string header, bool hasFoo, bool hasOther) in headerCases)
    {
        CheckHeaderCase(temporary, assemblyPath, headerExistingPath, name, header, hasFoo, hasOther);
    }

    Console.WriteLine("GeneratorTests passed");
    return 0;
}
finally
{
    Directory.Delete(temporary, recursive: true);
}
