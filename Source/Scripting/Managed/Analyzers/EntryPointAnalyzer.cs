namespace FOnline.Analyzers;

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Diagnostics;

// Methods the engine reaches through an attribute: a dispatcher calls them, or a registry looks them up by name.
//
// FOENTRY001 is the Managed C# counterpart of the direct-call gate the AngelScript backend applies to attributed
// functions (ValidateAttributedFunctionUsage). An entry point's [RequiresCover] is sound only because its dispatcher
// synchronized the subject first; an ordinary call skips that and quietly turns FOSYNC003 into a promise about
// nothing. FOENTRY002 keeps every such method internal: the lookup is reflective, so a private method looks unused
// to IDE0051, while public promises callers outside the script assembly that do not exist
[DiagnosticAnalyzer(LanguageNames.CSharp)]
public sealed class EntryPointAnalyzer : DiagnosticAnalyzer
{
    private const string Category = "Dispatch";

    // Comma-separated metadata names of project attributes, read from the .editorconfig section that covers the file
    // declaring the method. Dispatched attributes behave like the engine dispatch markers; registered attributes
    // only require internal access. An attribute class marked [EntryPointMarker] is dispatched without any option
    public const string DispatchedAttributesOption = "fonline_entry.dispatched_attributes";
    public const string RegisteredAttributesOption = "fonline_entry.registered_attributes";

    // Engine attributes whose methods only their dispatcher calls (CoreScripts/Attributes.cs, DatabaseDocument.cs).
    // Markers that already carry [EntryPointMarker] are recognised through it and are not repeated here
    private static readonly string[] EngineDispatchedAttributeNames = {
        "FOnline.ModuleInitAttribute",
        "FOnline.PropertyGetterAttribute",
        "FOnline.PropertySetterAttribute",
        "FOnline.AnimCallbackAttribute",
        "FOnline.ScriptFuncRegistrarAttribute",
    };

    // Engine attributes that publish a method for lookup by name or by its migration rule. An ordinary C# call of
    // such a method stays legal; only its access is part of the contract
    private static readonly string[] EngineRegisteredAttributeNames = {
        "FOnline.CallableByNameAttribute",
        "FOnline.CallableFromNativeAttribute",
        "FOnline.CallableByEngineAttribute",
        "FOnline.PropertyMigratorAttribute",
        "FOnline.ProtoMigratorAttribute",
    };

    internal static readonly DiagnosticDescriptor DirectEntryPointCallRule = new DiagnosticDescriptor(
        id: "FOENTRY001", title: "Dispatched entry point is called directly",
        messageFormat: "'{0}' is called by its dispatcher; move the shared logic into a plain helper and call that instead",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "A method carrying a dispatch attribute ([Event], [TimeEvent], [ModuleInit], remote calls, entity " +
            "init and trigger markers, property getters and setters, [AnimCallback], [ScriptFuncRegistrar], or " +
            "any marker declared [EntryPointMarker]) is invoked by the engine, which prepares its execution context " +
            "and synchronizes its subject first. An ordinary call skips that preparation and voids the entry " +
            "point's [RequiresCover] declaration. Subscribing the method group remains the prescribed registration.");

    internal static readonly DiagnosticDescriptor EngineCalledAccessRule = new DiagnosticDescriptor(
        id: "FOENTRY002", title: "Engine-called method is not internal",
        messageFormat: "'{0}' is called by the engine and must be declared internal (it is {1})", category: Category,
        defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "A method the engine dispatches or looks up through an attribute is reached by reflection. A " +
            "private one looks unused to IDE0051, and a public one promises callers outside the script assembly. " +
            "Declare it internal. Overrides and interface implementations repeat the access of their base and " +
            "are not judged.");

    internal static readonly DiagnosticDescriptor UnknownEntryAttributeRule = new DiagnosticDescriptor(
        id: "FOENTRY003", title: "Configured entry-point attribute does not exist",
        messageFormat: "'{0}' lists '{1}', which is not a type in this compilation", category: Category,
        defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "Every name listed in fonline_entry.dispatched_attributes or fonline_entry.registered_attributes " +
            "must be the metadata name of an attribute type the compilation declares or references. A misspelt " +
            "name would otherwise silently drop that attribute from FOENTRY001 and FOENTRY002.",
        customTags: WellKnownDiagnosticTags.CompilationEnd);

    public override ImmutableArray<DiagnosticDescriptor> SupportedDiagnostics =>
        ImmutableArray.Create(DirectEntryPointCallRule, EngineCalledAccessRule, UnknownEntryAttributeRule);

    public override void Initialize(AnalysisContext context)
    {
        context.ConfigureGeneratedCodeAnalysis(GeneratedCodeAnalysisFlags.Analyze |
                                               GeneratedCodeAnalysisFlags.ReportDiagnostics);
        context.EnableConcurrentExecution();

        context.RegisterCompilationStartAction(
            compilationStart =>
            {
                var vocabulary = new EntryVocabulary(compilationStart.Compilation, compilationStart.Options);

                compilationStart.RegisterSyntaxTreeAction(treeContext => vocabulary.ValidateOptions(treeContext.Tree));
                compilationStart.RegisterSymbolAction(symbolContext => AnalyzeMethod(symbolContext, vocabulary),
                                                      SymbolKind.Method);
                compilationStart.RegisterSyntaxNodeAction(nodeContext => AnalyzeInvocation(nodeContext, vocabulary),
                                                          SyntaxKind.InvocationExpression);
                compilationStart.RegisterCompilationEndAction(endContext => vocabulary.ReportUnknownNames(endContext));
            });
    }

    private static void AnalyzeMethod(SymbolAnalysisContext context, EntryVocabulary vocabulary)
    {
        var method = (IMethodSymbol)context.Symbol;

        if (method.IsImplicitlyDeclared || method.MethodKind != MethodKind.Ordinary || method.IsOverride ||
            method.IsExtern || method.ExplicitInterfaceImplementations.Length > 0 ||
            method.DeclaredAccessibility == Accessibility.Internal) {
            return;
        }

        // The baker's own wrappers keep private lookup members on purpose: scripts must not call them
        if (context.IsGeneratedCode && ScriptStaticAnalyzer.IsEngineNamespace(method.ContainingNamespace)) {
            return;
        }

        if (vocabulary.RoleOf(method) == EntryRole.None) {
            return;
        }

        context.ReportDiagnostic(Diagnostic.Create(EngineCalledAccessRule,
                                                   method.Locations[0],
                                                   method.Name,
                                                   AccessWord(method.DeclaredAccessibility)));
    }

    // Only an invocation counts: passing the method group, as `Game.OnCritterDamaged.Subscribe(OnDamaged)` does, is
    // how a handler is registered, and nameof is not a call
    private static void AnalyzeInvocation(SyntaxNodeAnalysisContext context, EntryVocabulary vocabulary)
    {
        var invocation = (InvocationExpressionSyntax)context.Node;

        if (context.SemanticModel.GetSymbolInfo(invocation, context.CancellationToken)
                .Symbol is not IMethodSymbol target) {
            return;
        }

        IMethodSymbol definition = (target.ReducedFrom ?? target).OriginalDefinition;

        if (vocabulary.RoleOf(definition) != EntryRole.Dispatched) {
            return;
        }

        context.ReportDiagnostic(
            Diagnostic.Create(DirectEntryPointCallRule, invocation.GetLocation(), definition.Name));
    }

    private static string AccessWord(Accessibility accessibility)
    {
        switch (accessibility) {
        case Accessibility.Private:
            return "private";
        case Accessibility.Public:
            return "public";
        case Accessibility.Protected:
            return "protected";
        case Accessibility.ProtectedAndInternal:
            return "private protected";
        case Accessibility.ProtectedOrInternal:
            return "protected internal";
        default:
            return "unspecified";
        }
    }

    private enum EntryRole
    {
        None,
        Registered,
        Dispatched,
    }

    // Attribute symbols for one compilation. Project names come from the options of the file that declares the
    // method, so a section that covers only some sources extends the vocabulary only for them
    private sealed class EntryVocabulary
    {
        private static readonly char[] Separators = { ',', ' ', '\t', ';' };

        private readonly Compilation CompilationContext;
        private readonly AnalyzerOptions Options;
        private readonly INamedTypeSymbol? EntryPointMarker;
        private readonly ImmutableHashSet<INamedTypeSymbol> EngineDispatched;
        private readonly ImmutableHashSet<INamedTypeSymbol> EngineRegistered;
        private readonly ConcurrentDictionary<string, ImmutableHashSet<INamedTypeSymbol>> ResolvedLists =
            new ConcurrentDictionary<string, ImmutableHashSet<INamedTypeSymbol>>(StringComparer.Ordinal);
        private readonly ConcurrentDictionary<(string Option, string Name), bool> UnknownNames =
            new ConcurrentDictionary<(string Option, string Name), bool>();

        public EntryVocabulary(Compilation compilation, AnalyzerOptions options)
        {
            CompilationContext = compilation;
            Options = options;
            EntryPointMarker = compilation.GetTypeByMetadataName(CoverVocabulary.EntryPointMarkerAttributeFullName);
            EngineDispatched = ResolveAll(EngineDispatchedAttributeNames);
            EngineRegistered = ResolveAll(EngineRegisteredAttributeNames);
        }

        public EntryRole RoleOf(IMethodSymbol method)
        {
            ImmutableArray<AttributeData> attributes = method.GetAttributes();

            if (attributes.IsEmpty) {
                return EntryRole.None;
            }

            SyntaxTree? tree = method.DeclaringSyntaxReferences.FirstOrDefault()?.SyntaxTree;
            ImmutableHashSet<INamedTypeSymbol> projectDispatched = ProjectList(tree, DispatchedAttributesOption);
            ImmutableHashSet<INamedTypeSymbol> projectRegistered = ProjectList(tree, RegisteredAttributesOption);
            EntryRole role = EntryRole.None;

            foreach (AttributeData attribute in attributes) {
                INamedTypeSymbol? type = attribute.AttributeClass;

                if (type == null) {
                    continue;
                }

                if (EngineDispatched.Contains(type) || projectDispatched.Contains(type) || IsMarkedEntryPoint(type)) {
                    return EntryRole.Dispatched;
                }

                if (EngineRegistered.Contains(type) || projectRegistered.Contains(type)) {
                    role = EntryRole.Registered;
                }
            }

            return role;
        }

        public void ValidateOptions(SyntaxTree tree)
        {
            ProjectList(tree, DispatchedAttributesOption);
            ProjectList(tree, RegisteredAttributesOption);
        }

        public void ReportUnknownNames(CompilationAnalysisContext context)
        {
            foreach ((string option, string name)
                         in UnknownNames.Keys.OrderBy(key => key.Option, StringComparer.Ordinal)
                             .ThenBy(key => key.Name, StringComparer.Ordinal)) {
                context.ReportDiagnostic(Diagnostic.Create(UnknownEntryAttributeRule, Location.None, option, name));
            }
        }

        private bool IsMarkedEntryPoint(INamedTypeSymbol attributeType)
        {
            return EntryPointMarker != null &&
                   attributeType.GetAttributes().Any(
                       marker => SymbolEqualityComparer.Default.Equals(marker.AttributeClass, EntryPointMarker));
        }

        private ImmutableHashSet<INamedTypeSymbol> ProjectList(SyntaxTree? tree, string option)
        {
            if (tree == null || !Options.AnalyzerConfigOptionsProvider.GetOptions(tree).TryGetValue(option, out string? value) ||
                value.Length == 0) {
                return ImmutableHashSet<INamedTypeSymbol>.Empty;
            }

            return ResolvedLists.GetOrAdd(option + "=" + value,
                                          _ => ResolveOption(option, value));
        }

        private ImmutableHashSet<INamedTypeSymbol> ResolveOption(string option, string value)
        {
            ImmutableHashSet<INamedTypeSymbol>.Builder resolved =
                ImmutableHashSet.CreateBuilder<INamedTypeSymbol>(SymbolEqualityComparer.Default);

            foreach (string name in value.Split(Separators, StringSplitOptions.RemoveEmptyEntries)) {
                INamedTypeSymbol? type = CompilationContext.GetTypeByMetadataName(name);

                if (type == null) {
                    UnknownNames.TryAdd((option, name), true);
                    continue;
                }

                resolved.Add(type);
            }

            return resolved.ToImmutable();
        }

        private ImmutableHashSet<INamedTypeSymbol> ResolveAll(IEnumerable<string> names)
        {
            ImmutableHashSet<INamedTypeSymbol>.Builder resolved =
                ImmutableHashSet.CreateBuilder<INamedTypeSymbol>(SymbolEqualityComparer.Default);

            foreach (string name in names) {
                INamedTypeSymbol? type = CompilationContext.GetTypeByMetadataName(name);

                if (type != null) {
                    resolved.Add(type);
                }
            }

            return resolved.ToImmutable();
        }
    }
}
