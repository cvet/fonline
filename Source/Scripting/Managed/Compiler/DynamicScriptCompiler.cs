namespace FOnline.ScriptCompiler;

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Runtime.Loader;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Emit;

// A fragment and what it is compiled against; see Docs/Scripting.md, "Compiling code after the bake"
public sealed class DynamicCompileRequest
{
    public required string Source { get; init; }

    // The running script assembly: its load context supplies the references
    public required Assembly ScriptsAssembly { get; init; }

    // Another side's script assembly, compiled against in place of the running one
    public byte[]? ScriptsImage { get; init; }

    public IReadOnlyList<string> PreprocessorSymbols { get; init; } = Array.Empty<string>();

    public IReadOnlyList<string> Usings { get; init; } = Array.Empty<string>();
}

public sealed class DynamicCompileResult
{
    internal DynamicCompileResult(string assemblyName, byte[] image, byte[] symbols, IReadOnlyList<string> errors)
    {
        AssemblyName = assemblyName;
        Image = image;
        Symbols = symbols;
        Errors = errors;
    }

    public bool Succeeded => Errors.Count == 0;

    public string AssemblyName { get; }

    public byte[] Image { get; }

    // Empty, since a fragment is emitted without a PDB
    public byte[] Symbols { get; }

    public IReadOnlyList<string> Errors { get; }
}

public static class DynamicScriptCompiler
{
    public const string AssemblyNamePrefix = "FOnline.Dynamic.";
    public const string EntryTypeName = "DynamicFragment";
    public const string EntryMethodName = "Run";

    private const string FragmentPath = "fragment";

    private static readonly Regex UsingDirective = new Regex(
        @"^\s*using\s+(static\s+)?[A-Za-z_][\w.]*(\s*=\s*[A-Za-z_][\w.<>, ]*)?\s*;\s*$", RegexOptions.CultureInvariant);

    private static readonly Dictionary<string, List<MetadataReference>> RuntimeReferences =
        new Dictionary<string, List<MetadataReference>>(StringComparer.OrdinalIgnoreCase);

    // Roslyn spends seconds on its first run, and the compilation reads nothing of the engine, so it leaves the script
    // worker free and runs on a pool thread
    public static Task<DynamicCompileResult> CompileAsync(DynamicCompileRequest request)
    {
        ArgumentNullException.ThrowIfNull(request);

        return Task.Run(() => Compile(request));
    }

    // A fragment is a method body, or a single expression whose value the entry answers with
    public static DynamicCompileResult Compile(DynamicCompileRequest request)
    {
        ArgumentNullException.ThrowIfNull(request);

        string scriptsName = request.ScriptsImage != null ? ReadAssemblyName(request.ScriptsImage)
                                                          : request.ScriptsAssembly.GetName().Name ?? "";

        if (scriptsName.Length == 0) {
            throw new InvalidOperationException("Script assembly has no simple name");
        }

        string assemblyName = AssemblyNamePrefix + Guid.NewGuid().ToString("N");
        List<MetadataReference> references = CollectReferences(request, scriptsName);
        SplitUsings(request.Source, out List<(string Line, int Number)> usings, out string body, out int bodyLine);

        if (IsExpression(body)) {
            DynamicCompileResult asValue = Emit(request,
                                                references,
                                                scriptsName,
                                                assemblyName,
                                                usings,
                                                "return (object)(" + body.Trim() + "\n);",
                                                bodyLine);

            if (asValue.Succeeded) {
                return asValue;
            }

            // An expression of type void or Task has no value to answer with, so it runs as a statement instead
            DynamicCompileResult asStatement =
                Emit(request, references, scriptsName, assemblyName, usings, body.Trim() + "\n;", bodyLine);
            return asStatement.Succeeded ? asStatement : asValue;
        }

        return Emit(request, references, scriptsName, assemblyName, usings, body, bodyLine);
    }

    public static string ReadAssemblyName(byte[] image)
    {
        ArgumentNullException.ThrowIfNull(image);

        using PEReader reader = new PEReader(ImmutableArray.Create(image));
        MetadataReader metadata = reader.GetMetadataReader();
        return metadata.GetString(metadata.GetAssemblyDefinition().Name);
    }

    public static Guid ReadModuleVersionId(byte[] image)
    {
        ArgumentNullException.ThrowIfNull(image);

        using PEReader reader = new PEReader(ImmutableArray.Create(image));
        MetadataReader metadata = reader.GetMetadataReader();
        return metadata.GetGuid(metadata.GetModuleDefinition().Mvid);
    }

    private static DynamicCompileResult Emit(DynamicCompileRequest request, List<MetadataReference> references,
                                             string scriptsName, string assemblyName,
                                             List<(string Line, int Number)> usings, string body, int bodyLine)
    {
        string source = MakeSource(request, scriptsName, usings, body, bodyLine);
        CSharpParseOptions parseOptions = new CSharpParseOptions(LanguageVersion.Latest,
                                                                 DocumentationMode.None,
                                                                 SourceCodeKind.Regular,
                                                                 request.PreprocessorSymbols);
        SyntaxTree tree = CSharpSyntaxTree.ParseText(source, parseOptions, FragmentPath + ".cs", Encoding.UTF8);
        CSharpCompilation compilation =
            CSharpCompilation.Create(assemblyName, new[] { tree }, references, MakeCompilationOptions());

        // Emit hashes nothing: the embedded runtime may lack cryptography (Linux links no crypto shim), and a PDB
        // needs a document checksum, so a fragment ships without one and its frames carry no line numbers
        using MemoryStream image = new MemoryStream();
        EmitResult emitted =
            compilation.Emit(image, options: new EmitOptions(pdbChecksumAlgorithm: default(HashAlgorithmName)));
        List<string> errors = new List<string>();

        // In the invariant culture, so the answer reads the same whatever locale the server runs under
        foreach (Diagnostic diagnostic in emitted.Diagnostics) {
            if (diagnostic.Severity == DiagnosticSeverity.Error) {
                errors.Add(CSharpDiagnosticFormatter.Instance.Format(diagnostic, CultureInfo.InvariantCulture));
            }
        }

        if (!emitted.Success && errors.Count == 0) {
            errors.Add("Fragment compilation failed without an error diagnostic");
        }

        return new DynamicCompileResult(assemblyName,
                                        emitted.Success ? image.ToArray() : Array.Empty<byte>(),
                                        Array.Empty<byte>(),
                                        errors);
    }

    // The fragment lands inside an async method, and #line keeps every diagnostic on the line the author wrote
    private static string MakeSource(DynamicCompileRequest request, string scriptsName,
                                     List<(string Line, int Number)> usings, string body, int bodyLine)
    {
        StringBuilder source = new StringBuilder();

        foreach (string ns in request.Usings) {
            source.Append("using ").Append(ns).Append(";\n");
        }

        foreach ((string line, int number) in usings) {
            source.Append("#line ").Append(number).Append(" \"").Append(FragmentPath).Append("\"\n");
            source.Append(line).Append('\n');
            source.Append("#line default\n");
        }

        source.Append("[assembly: System.Runtime.CompilerServices.IgnoresAccessChecksTo(\"")
            .Append(scriptsName)
            .Append("\")]\n");
        source.Append("namespace System.Runtime.CompilerServices\n{\n");
        source.Append("    [System.AttributeUsage(System.AttributeTargets.Assembly, AllowMultiple = true)]\n");
        source.Append("    internal sealed class IgnoresAccessChecksToAttribute : System.Attribute\n    {\n");
        source.Append(
            "        public IgnoresAccessChecksToAttribute(string assemblyName) { AssemblyName = assemblyName; }\n");
        source.Append("        public string AssemblyName { get; }\n    }\n}\n");
        source.Append("public static class ").Append(EntryTypeName).Append("\n{\n");
        source.Append("    public static async System.Threading.Tasks.Task<object> ")
            .Append(EntryMethodName)
            .Append("()\n    {\n");
        source.Append("#line ").Append(bodyLine).Append(" \"").Append(FragmentPath).Append("\"\n");
        source.Append(body).Append('\n');
        source.Append("#line hidden\n");
        source.Append("        return null;\n    }\n}\n");
        return source.ToString();
    }

    // The fragment reads private and internal members of the scripts; the runtime lets it through the matching
    // IgnoresAccessChecksTo attribute, and the compiler needs the binder flag Roslyn keeps for its own scripting
    private static CSharpCompilationOptions MakeCompilationOptions()
    {
        CSharpCompilationOptions options =
            new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary,
                                         optimizationLevel: OptimizationLevel.Debug,
                                         allowUnsafe: false,
                                         concurrentBuild: false,
                                         deterministic: false,
                                         nullableContextOptions: NullableContextOptions.Disable)
                .WithMetadataImportOptions(MetadataImportOptions.All);

        Type? binderFlags =
            typeof(CSharpCompilationOptions).Assembly.GetType("Microsoft.CodeAnalysis.CSharp.BinderFlags");
        MethodInfo? withFlags =
            typeof(CSharpCompilationOptions)
                .GetMethod("WithTopLevelBinderFlags", BindingFlags.Instance | BindingFlags.NonPublic);

        if (binderFlags == null || withFlags == null) {
            throw new InvalidOperationException("Roslyn no longer exposes the accessibility binder flag");
        }

        return (
            CSharpCompilationOptions)(withFlags.Invoke(options,
                                                       new[] { Enum.Parse(binderFlags, "IgnoreAccessibility") }) ??
                                      throw new InvalidOperationException("Roslyn returned no compilation options"));
    }

    // The class libraries the backend runs on, plus the assemblies its load context and the default context hold,
    // so the fragment binds to exactly what it will run against
    private static List<MetadataReference> CollectReferences(DynamicCompileRequest request, string scriptsName)
    {
        Dictionary<string, MetadataReference> references =
            new Dictionary<string, MetadataReference>(StringComparer.OrdinalIgnoreCase);
        string runtimeDir = Path.GetDirectoryName(typeof(object).Assembly.Location) ??
                            throw new InvalidOperationException("Runtime class library directory is unknown");

        foreach ((string name, MetadataReference reference) in GetRuntimeReferences(runtimeDir)) {
            references[name] = reference;
        }

        AssemblyLoadContext scriptsContext =
            AssemblyLoadContext.GetLoadContext(request.ScriptsAssembly) ?? AssemblyLoadContext.Default;
        string? runningScriptsName = request.ScriptsAssembly.GetName().Name;

        foreach (AssemblyLoadContext context in new[] { AssemblyLoadContext.Default, scriptsContext }) {
            foreach (Assembly assembly in context.Assemblies) {
                string? name = assembly.GetName().Name;

                if (name == null || assembly.IsDynamic || assembly.Location.Length == 0 ||
                    !IsReferencedByFragments(name)) {
                    continue;
                }
                if (request.ScriptsImage != null &&
                    string.Equals(name, runningScriptsName, StringComparison.OrdinalIgnoreCase)) {
                    continue;
                }

                references[name] = MetadataReference.CreateFromFile(assembly.Location);
            }
        }

        if (request.ScriptsImage != null) {
            references[scriptsName] = MetadataReference.CreateFromImage(request.ScriptsImage);
        }

        return new List<MetadataReference>(references.Values);
    }

    // Earlier fragments and the compiler itself are nothing a fragment binds to
    private static bool IsReferencedByFragments(string name)
    {
        return !name.StartsWith(AssemblyNamePrefix, StringComparison.Ordinal) &&
               !name.StartsWith("Microsoft.CodeAnalysis", StringComparison.Ordinal) &&
               !string.Equals(name, typeof(DynamicScriptCompiler).Assembly.GetName().Name, StringComparison.Ordinal);
    }

    private static List<(string Name, MetadataReference Reference)> GetRuntimeReferences(string runtimeDir)
    {
        lock (RuntimeReferences)
        {
            if (!RuntimeReferences.TryGetValue(runtimeDir, out List<MetadataReference>? cached)) {
                cached = new List<MetadataReference>();

                foreach (string path in Directory.EnumerateFiles(runtimeDir, "*.dll")) {
                    if (IsManagedAssembly(path)) {
                        cached.Add(MetadataReference.CreateFromFile(path));
                    }
                }

                RuntimeReferences.Add(runtimeDir, cached);
            }

            List<(string Name, MetadataReference Reference)> named =
                new List<(string Name, MetadataReference Reference)>();

            foreach (MetadataReference reference in cached) {
                named.Add((Path.GetFileNameWithoutExtension(reference.Display ?? ""), reference));
            }

            return named;
        }
    }

    // A runtime directory also holds native libraries, which a metadata reference cannot read
    private static bool IsManagedAssembly(string path)
    {
        try {
            using FileStream stream = File.OpenRead(path);
            using PEReader reader = new PEReader(stream);
            return reader.HasMetadata && reader.GetMetadataReader().IsAssembly;
        }
        catch (BadImageFormatException) {
            return false;
        }
    }

    // Leading using directives belong above the wrapper; the rest is the body, with the line it starts on
    private static void SplitUsings(string source, out List<(string Line, int Number)> usings, out string body,
                                    out int bodyLine)
    {
        usings = new List<(string Line, int Number)>();
        string[] lines = source.Replace("\r\n", "\n").Split('\n');
        int first = 0;

        while (first < lines.Length) {
            string trimmed = lines[first].Trim();

            if (trimmed.Length == 0 || trimmed.StartsWith("//", StringComparison.Ordinal)) {
                first++;
                continue;
            }
            if (!UsingDirective.IsMatch(lines[first])) {
                break;
            }

            usings.Add((lines[first], first + 1));
            first++;
        }

        body = string.Join("\n", lines, first, lines.Length - first);
        bodyLine = first + 1;
    }

    private static bool IsExpression(string body)
    {
        string text = body.Trim();

        if (text.Length == 0 || text.EndsWith(';')) {
            return false;
        }

        ExpressionSyntax expression = SyntaxFactory.ParseExpression(text);
        return !expression.ContainsDiagnostics;
    }
}
