namespace FOnline.Analyzers;

using System;
using System.Collections.Immutable;
using System.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Diagnostics;

// Static storage the engine cannot clear when scripting shuts down.
//
// The script load context is not collectible, so a script static roots everything behind it for the life of the
// process. CoreScripts/ScriptStaticCleanup.cs therefore clears every static reference field of the script and dynamic
// assemblies before the backend unbinds. Three shapes are out of its reach, and this rule rejects them: a static of a
// generic type (its instantiations cannot be enumerated), a [ThreadStatic] (one slot per thread), and a static value
// type with a reference inside (the sweep selects fields by their own type). The engine's own FOnline namespace is
// outside the sweep and outside this rule
[DiagnosticAnalyzer(LanguageNames.CSharp)]
public sealed class ScriptStaticAnalyzer : DiagnosticAnalyzer
{
    private const string Category = "Lifetime";
    private const string EngineNamespace = "FOnline";
    private const string ThreadStaticAttributeFullName = "System.ThreadStaticAttribute";

    private const string GenericReason = "a static of a generic type";
    private const string ThreadStaticReason = "a thread static";
    private const string ValueTypeReason = "a value type with a reference inside";

    internal static readonly DiagnosticDescriptor UnreachableStaticRule = new DiagnosticDescriptor(
        id: "FOSTATIC001", title: "Static the engine cannot clear at shutdown",
        messageFormat: "Static '{0}' holds a reference the engine cannot clear when scripting shuts down ({1})",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "Script statics are roots for the life of the process, because the script assembly load context " +
            "is not collectible, so the engine clears every static reference itself when scripting shuts down. " +
            "A static of a generic type, a [ThreadStatic], and a static value type with a reference inside are out " +
            "of its reach. Move the state into a non-generic type, or hold it behind an ordinary static reference field.");

    public override ImmutableArray<DiagnosticDescriptor> SupportedDiagnostics =>
        ImmutableArray.Create(UnreachableStaticRule);

    public override void Initialize(AnalysisContext context)
    {
        // Project generators emit script state as well, so generated sources are judged like the rest
        context.ConfigureGeneratedCodeAnalysis(GeneratedCodeAnalysisFlags.Analyze |
                                               GeneratedCodeAnalysisFlags.ReportDiagnostics);
        context.EnableConcurrentExecution();

        context.RegisterSymbolAction(AnalyzeType, SymbolKind.NamedType);
        context.RegisterSyntaxNodeAction(AnalyzeEvent, SyntaxKind.EventFieldDeclaration);
    }

    private static void AnalyzeType(SymbolAnalysisContext context)
    {
        var type = (INamedTypeSymbol)context.Symbol;

        // Roslyn raises no field action for the hidden backing field of an auto-property; the type still lists it
        foreach (ISymbol member in type.GetMembers()) {
            if (member is IFieldSymbol field && field.IsStatic && !field.IsConst &&
                field.AssociatedSymbol is not IEventSymbol) {
                bool threadStatic = field.GetAttributes().Any(
                    attribute => attribute.AttributeClass?.ToDisplayString() == ThreadStaticAttributeFullName);

                AnalyzeStorage(field.AssociatedSymbol ?? field, field.Type, threadStatic, context.ReportDiagnostic);
            }
        }
    }

    // The backing field of a field-like event is not part of the public symbol model, so its field-targeted
    // attributes are read from the declaration
    private static void AnalyzeEvent(SyntaxNodeAnalysisContext context)
    {
        var declaration = (EventFieldDeclarationSyntax)context.Node;

        bool threadStatic =
            declaration.AttributeLists.Where(list => list.Target?.Identifier.ValueText == "field")
                .SelectMany(list => list.Attributes)
                .Any(attribute => context.SemanticModel.GetSymbolInfo(attribute, context.CancellationToken)
                                      .Symbol is IMethodSymbol constructor &&
                                  constructor.ContainingType.ToDisplayString() == ThreadStaticAttributeFullName);

        foreach (VariableDeclaratorSyntax variable in declaration.Declaration.Variables) {
            if (context.SemanticModel.GetDeclaredSymbol(variable, context.CancellationToken)
                    is IEventSymbol eventSymbol &&
                eventSymbol.IsStatic) {
                AnalyzeStorage(eventSymbol, eventSymbol.Type, threadStatic, context.ReportDiagnostic);
            }
        }
    }

    private static void AnalyzeStorage(ISymbol storage, ITypeSymbol storageType, bool threadStatic,
                                       Action<Diagnostic> reportDiagnostic)
    {
        Location? location = storage.Locations.FirstOrDefault(candidate => candidate.IsInSource);

        if (location == null || IsEngineNamespace(storage.ContainingNamespace) || !CanHoldReference(storageType)) {
            return;
        }

        string? reason = DescribeUnreachableShape(storage, storageType, threadStatic);

        if (reason == null) {
            return;
        }

        reportDiagnostic(Diagnostic.Create(UnreachableStaticRule, location, storage.Name, reason));
    }

    private static string? DescribeUnreachableShape(ISymbol storage, ITypeSymbol storageType, bool threadStatic)
    {
        for (INamedTypeSymbol? owner = storage.ContainingType; owner != null; owner = owner.ContainingType) {
            if (!owner.TypeParameters.IsEmpty) {
                return GenericReason;
            }
        }

        if (threadStatic) {
            return ThreadStaticReason;
        }

        if (storageType.IsValueType) {
            return ValueTypeReason;
        }

        return null;
    }

    // Roslyn knows the full layout, including private fields of value types from other assemblies
    private static bool CanHoldReference(ITypeSymbol type)
    {
        if (type.IsUnmanagedType) {
            return false;
        }

        return type.IsReferenceType || type.IsValueType || type.TypeKind == TypeKind.TypeParameter ||
               type.TypeKind == TypeKind.Dynamic;
    }

    // Matches the namespace filter of ScriptStaticCleanup: FOnline and everything below it
    internal static bool IsEngineNamespace(INamespaceSymbol? scope)
    {
        for (INamespaceSymbol? current = scope; current != null && !current.IsGlobalNamespace;
             current = current.ContainingNamespace) {
            if (current.ContainingNamespace != null && current.ContainingNamespace.IsGlobalNamespace &&
                current.Name == EngineNamespace) {
                return true;
            }
        }

        return false;
    }
}
