namespace FOnline.ScriptCompiler;

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Runtime.Loader;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Emit;

public enum DynamicCompileKind
{
    // A method body, or a single expression, run once as a script entry
    Fragment,

    // A compilation unit whose [ReplacesMethod] methods replace script methods through their patch points
    Patch,
}

// A fragment or a patch and what it is compiled against; see Docs/Scripting.md, "Compiling code after the bake"
public sealed class DynamicCompileRequest
{
    public required string Source { get; init; }

    public DynamicCompileKind Kind { get; init; } = DynamicCompileKind.Fragment;

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

    public byte[] Symbols { get; }

    public IReadOnlyList<string> Errors { get; }
}

public static class DynamicScriptCompiler
{
    public const string AssemblyNamePrefix = "FOnline.Dynamic.";
    public const string EntryTypeName = "DynamicFragment";
    public const string EntryMethodName = "Run";

    // Must match ScriptPatches in CoreScripts, which this library cannot reference
    public const string PatchManifestTypeName = "DynamicPatchManifest";
    public const string PatchManifestMethodName = "GetFunctions";
    public const string PatchPointTableResourceName = "FOnline.PatchPoints.Table";
    public const string ReplacesMethodAttributeName = "FOnline.ReplacesMethodAttribute";

    private const string FragmentPath = "fragment";
    private const string PatchPath = "patch";

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

        if (request.Kind == DynamicCompileKind.Patch) {
            return CompilePatch(request, references, scriptsName, assemblyName);
        }

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
            CSharpCompilation.Create(assemblyName, new[] { tree }, references, MakeCompilationOptions(false));
        return EmitCompilation(compilation, assemblyName);
    }

    private static DynamicCompileResult EmitCompilation(CSharpCompilation compilation, string assemblyName)
    {
        using MemoryStream image = new MemoryStream();
        using MemoryStream symbols = new MemoryStream();
        EmitResult emitted =
            compilation.Emit(image,
                             symbols,
                             options: new EmitOptions(debugInformationFormat: DebugInformationFormat.PortablePdb));
        List<string> errors = FormatErrors(emitted.Diagnostics);

        if (!emitted.Success && errors.Count == 0) {
            errors.Add("Fragment compilation failed without an error diagnostic");
        }

        return new DynamicCompileResult(assemblyName,
                                        emitted.Success ? image.ToArray() : Array.Empty<byte>(),
                                        emitted.Success ? symbols.ToArray() : Array.Empty<byte>(),
                                        errors);
    }

    // In the invariant culture, so the answer reads the same whatever locale the server runs under
    private static List<string> FormatErrors(IEnumerable<Diagnostic> diagnostics)
    {
        List<string> errors = new List<string>();

        foreach (Diagnostic diagnostic in diagnostics) {
            if (diagnostic.Severity == DiagnosticSeverity.Error) {
                errors.Add(CSharpDiagnosticFormatter.Instance.Format(diagnostic, CultureInfo.InvariantCulture));
            }
        }

        return errors;
    }

    // A patch is compiled twice: the first pass binds its [ReplacesMethod] methods to their targets and checks them,
    // the second adds the manifest that hands ScriptPatches a function pointer for each. The pointer is taken with
    // ldftn in the patch itself, because the interpreter expects its own method handle where the JIT expects code
    private static DynamicCompileResult CompilePatch(DynamicCompileRequest request, List<MetadataReference> references,
                                                     string scriptsName, string assemblyName)
    {
        CSharpParseOptions parseOptions = new CSharpParseOptions(LanguageVersion.Latest,
                                                                 DocumentationMode.None,
                                                                 SourceCodeKind.Regular,
                                                                 request.PreprocessorSymbols);
        SyntaxTree prelude = CSharpSyntaxTree.ParseText(MakePatchPrelude(request, scriptsName, assemblyName),
                                                        parseOptions,
                                                        "prelude.cs",
                                                        Encoding.UTF8);
        SyntaxTree patch = CSharpSyntaxTree.ParseText(request.Source, parseOptions, PatchPath, Encoding.UTF8);
        CSharpCompilation compilation =
            CSharpCompilation.Create(assemblyName, new[] { prelude, patch }, references, MakeCompilationOptions(true));
        List<string> errors = FormatErrors(compilation.GetDiagnostics());

        if (errors.Count != 0) {
            return new DynamicCompileResult(assemblyName, Array.Empty<byte>(), Array.Empty<byte>(), errors);
        }

        HashSet<int>? patchPoints = request.ScriptsImage != null ? ReadPatchPointTable(request.ScriptsImage)
                                                                 : ReadPatchPointTable(request.ScriptsAssembly);
        SemanticModel model = compilation.GetSemanticModel(patch);
        List<IMethodSymbol> replacements = new List<IMethodSymbol>();
        HashSet<IMethodSymbol> targets = new HashSet<IMethodSymbol>(SymbolEqualityComparer.Default);

        foreach (MethodDeclarationSyntax declaration in patch.GetRoot()
                     .DescendantNodes()
                     .OfType<MethodDeclarationSyntax>()) {
            if (model.GetDeclaredSymbol(declaration) is not IMethodSymbol replacement) {
                continue;
            }

            AttributeData? attribute = replacement.GetAttributes().FirstOrDefault(
                data => data.AttributeClass?.ToDisplayString() == ReplacesMethodAttributeName);

            if (attribute == null) {
                continue;
            }

            string? refusal = CheckReplacement(replacement, attribute, scriptsName, patchPoints, out IMethodSymbol? target);

            if (refusal == null && target != null && !targets.Add(target)) {
                refusal = "patch replaces " + target.ToDisplayString() + " twice";
            }

            if (refusal != null) {
                errors.Add(FormatPatchError(replacement, refusal));
                continue;
            }

            replacements.Add(replacement);
        }

        if (errors.Count == 0 && replacements.Count == 0) {
            errors.Add(PatchPath + "(1,1): error FOPATCH001: patch has no [ReplacesMethod] method");
        }

        if (errors.Count != 0) {
            return new DynamicCompileResult(assemblyName, Array.Empty<byte>(), Array.Empty<byte>(), errors);
        }

        SyntaxTree manifest =
            CSharpSyntaxTree.ParseText(MakePatchManifest(replacements), parseOptions, "manifest.cs", Encoding.UTF8);
        return EmitCompilation(compilation.AddSyntaxTrees(manifest), assemblyName);
    }

    // The manifest takes the address of replacements that may be private, and a private replacement is what lets a
    // signature name a private script type, so the patch ignores access checks to itself as well as to the scripts
    private static string MakePatchPrelude(DynamicCompileRequest request, string scriptsName, string assemblyName)
    {
        StringBuilder source = new StringBuilder();

        foreach (string ns in request.Usings) {
            source.Append("global using ").Append(ns).Append(";\n");
        }

        source.Append("[assembly: System.Runtime.CompilerServices.IgnoresAccessChecksTo(\"")
            .Append(assemblyName)
            .Append("\")]\n");
        AppendIgnoresAccessChecks(source, scriptsName);
        return source.ToString();
    }

    // The replacement is static and takes the target's parameters, preceded by the target object for an instance
    // method (by ref for a struct)
    private static string? CheckReplacement(IMethodSymbol replacement, AttributeData attribute, string scriptsName,
                                            HashSet<int>? patchPoints, out IMethodSymbol? target)
    {
        target = null;

        if (!replacement.IsStatic || replacement.IsGenericMethod) {
            return "a replacement must be a static non-generic method";
        }

        for (INamedTypeSymbol? owner = replacement.ContainingType; owner != null; owner = owner.ContainingType) {
            if (owner.IsGenericType) {
                return "a replacement cannot live in a generic type";
            }
        }

        if (attribute.ConstructorArguments.Length != 2 ||
            attribute.ConstructorArguments[0].Value is not INamedTypeSymbol type ||
            attribute.ConstructorArguments[1].Value is not string name) {
            return "[ReplacesMethod] needs the target type and the method name";
        }

        List<IMethodSymbol> matches = type.GetMembers(name)
                                          .OfType<IMethodSymbol>()
                                          .Where(candidate => MatchesTarget(candidate, replacement))
                                          .ToList();

        if (matches.Count == 0) {
            return "no method " + type.ToDisplayString() + "." + name + " has the replacement's signature";
        }
        if (matches.Count != 1) {
            return "the replacement's signature matches more than one " + type.ToDisplayString() + "." + name;
        }

        target = matches[0];

        if (target.ContainingAssembly?.Name != scriptsName) {
            return target.ToDisplayString() + " is not a script method";
        }
        if (patchPoints == null) {
            return "the scripts carry no patch points (ManagedScript.PatchPointWeaver)";
        }
        if (!patchPoints.Contains(target.MetadataToken)) {
            return target.ToDisplayString() +
                   " has no patch point: constructors, generic, compiler-generated and [NoPatchPoint] methods are not patchable";
        }

        return null;
    }

    private static bool MatchesTarget(IMethodSymbol target, IMethodSymbol replacement)
    {
        if (target.IsGenericMethod || target.MethodKind == MethodKind.Constructor ||
            target.MethodKind == MethodKind.StaticConstructor ||
            !SymbolEqualityComparer.Default.Equals(target.ReturnType, replacement.ReturnType) ||
            target.RefKind != replacement.RefKind) {
            return false;
        }

        int offset = target.IsStatic ? 0 : 1;

        if (replacement.Parameters.Length != target.Parameters.Length + offset) {
            return false;
        }
        if (!target.IsStatic) {
            IParameterSymbol self = replacement.Parameters[0];
            RefKind expected = target.ContainingType.IsValueType ? RefKind.Ref : RefKind.None;

            if (!SymbolEqualityComparer.Default.Equals(self.Type, target.ContainingType) || self.RefKind != expected) {
                return false;
            }
        }

        for (int i = 0; i < target.Parameters.Length; i++) {
            IParameterSymbol expected = target.Parameters[i];
            IParameterSymbol actual = replacement.Parameters[i + offset];

            if (!SymbolEqualityComparer.Default.Equals(expected.Type, actual.Type) ||
                !SameRefKind(expected.RefKind, actual.RefKind)) {
                return false;
            }
        }

        return true;
    }

    // At run time in, ref readonly and ref are one byref type
    private static bool SameRefKind(RefKind left, RefKind right)
    {
        return (left == RefKind.None) == (right == RefKind.None) && (left == RefKind.Out) == (right == RefKind.Out);
    }

    private static string FormatPatchError(IMethodSymbol replacement, string message)
    {
        FileLinePositionSpan span =
            replacement.Locations.Length != 0 ? replacement.Locations[0].GetLineSpan() : default;
        return PatchPath + "(" + (span.StartLinePosition.Line + 1).ToString(CultureInfo.InvariantCulture) + "," +
               (span.StartLinePosition.Character + 1).ToString(CultureInfo.InvariantCulture) +
               "): error FOPATCH002: " + message;
    }

    private static string MakePatchManifest(List<IMethodSymbol> replacements)
    {
        StringBuilder source = new StringBuilder();
        source.Append("internal static unsafe class ").Append(PatchManifestTypeName).Append("\n{\n");
        source.Append("    public static global::FOnline.ScriptPatchFunction[] ")
            .Append(PatchManifestMethodName)
            .Append("()\n    {\n");
        source.Append("        return new global::FOnline.ScriptPatchFunction[] {\n");

        foreach (IMethodSymbol replacement in replacements) {
            string owner = replacement.ContainingType.ToDisplayString(SymbolDisplayFormat.FullyQualifiedFormat);
            List<string> parameterTypes = new List<string>();
            List<string> pointerParameters = new List<string>();

            foreach (IParameterSymbol parameter in replacement.Parameters) {
                string type = parameter.Type.ToDisplayString(SymbolDisplayFormat.FullyQualifiedFormat);
                parameterTypes.Add(parameter.RefKind == RefKind.None ? "typeof(" + type + ")"
                                                                     : "typeof(" + type + ").MakeByRefType()");
                pointerParameters.Add(RefKindPrefix(parameter.RefKind) + type);
            }

            string returnType = (replacement.RefKind == RefKind.RefReadOnly ? "ref readonly "
                                 : replacement.RefKind == RefKind.Ref       ? "ref "
                                                                            : "") +
                                replacement.ReturnType.ToDisplayString(SymbolDisplayFormat.FullyQualifiedFormat);
            pointerParameters.Add(returnType);

            source.Append("            new global::FOnline.ScriptPatchFunction(typeof(")
                .Append(owner)
                .Append(").GetMethod(\"")
                .Append(replacement.Name)
                .Append("\", (global::System.Reflection.BindingFlags)58, null, new global::System.Type[] { ")
                .Append(string.Join(", ", parameterTypes))
                .Append(" }, null), (nint)(delegate*<")
                .Append(string.Join(", ", pointerParameters))
                .Append(">)&")
                .Append(owner)
                .Append(".@")
                .Append(replacement.Name)
                .Append("),\n");
        }

        source.Append("        };\n    }\n}\n");
        return source.ToString();
    }

    private static string RefKindPrefix(RefKind kind)
    {
        return kind switch {
            RefKind.Ref => "ref ",
            RefKind.Out => "out ",
            RefKind.In => "in ",
            RefKind.RefReadOnlyParameter => "ref readonly ",
            _ => "",
        };
    }

    private static HashSet<int>? ReadPatchPointTable(Assembly scripts)
    {
        using Stream? stream = scripts.GetManifestResourceStream(PatchPointTableResourceName);

        if (stream == null) {
            return null;
        }

        using MemoryStream copy = new MemoryStream();
        stream.CopyTo(copy);
        return ToTokenSet(copy.ToArray());
    }

    private static HashSet<int>? ReadPatchPointTable(byte[] image)
    {
        using PEReader reader = new PEReader(ImmutableArray.Create(image));
        MetadataReader metadata = reader.GetMetadataReader();

        foreach (ManifestResourceHandle handle in metadata.ManifestResources) {
            ManifestResource resource = metadata.GetManifestResource(handle);

            if (!resource.Implementation.IsNil || metadata.GetString(resource.Name) != PatchPointTableResourceName) {
                continue;
            }

            CorHeader header =
                reader.PEHeaders.CorHeader ?? throw new BadImageFormatException("Script image has no CLI header");
            BlobReader blob =
                reader.GetSectionData(header.ResourcesDirectory.RelativeVirtualAddress + (int)resource.Offset)
                    .GetReader();
            int length = blob.ReadInt32();
            return ToTokenSet(blob.ReadBytes(length));
        }

        return null;
    }

    private static HashSet<int> ToTokenSet(byte[] table)
    {
        HashSet<int> tokens = new HashSet<int>();

        for (int offset = 0; offset + sizeof(int) <= table.Length; offset += sizeof(int)) {
            tokens.Add(BitConverter.ToInt32(table, offset));
        }

        return tokens;
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

        AppendIgnoresAccessChecks(source, scriptsName);
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

    private static void AppendIgnoresAccessChecks(StringBuilder source, string scriptsName)
    {
        source.Append("[assembly: System.Runtime.CompilerServices.IgnoresAccessChecksTo(\"")
            .Append(scriptsName)
            .Append("\")]\n");
        source.Append("namespace System.Runtime.CompilerServices\n{\n");
        source.Append("    [System.AttributeUsage(System.AttributeTargets.Assembly, AllowMultiple = true)]\n");
        source.Append("    internal sealed class IgnoresAccessChecksToAttribute : System.Attribute\n    {\n");
        source.Append(
            "        public IgnoresAccessChecksToAttribute(string assemblyName) { AssemblyName = assemblyName; }\n");
        source.Append("        public string AssemblyName { get; }\n    }\n}\n");
    }

    // Code compiled here reads private and internal members of the scripts; the runtime lets it through the matching
    // IgnoresAccessChecksTo attribute, and the compiler needs the binder flag Roslyn keeps for its own scripting. A
    // patch replaces a method that keeps running, so it is optimized, and its manifest takes function pointers
    private static CSharpCompilationOptions MakeCompilationOptions(bool patch)
    {
        CSharpCompilationOptions options =
            new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary,
                                         optimizationLevel: patch ? OptimizationLevel.Release : OptimizationLevel.Debug,
                                         allowUnsafe: patch,
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
