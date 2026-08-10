#nullable enable

using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Diagnostics;

namespace FOnline.Analyzers
{
    // Compile-time checking of the entity synchronization cover contract declared by [RequiresCover].
    //
    // This replaces the `// SyncScope:` comment convention and the external dataflow audit that read
    // AngelScript only. The contract now lives on the symbol, so it survives refactoring, is visible in an
    // IDE while typing, and is checked by the same compiler pass that already gates code style.
    [DiagnosticAnalyzer(LanguageNames.CSharp)]
    public sealed class SyncCoverAnalyzer : DiagnosticAnalyzer
    {
        public const string RequiresCoverAttributeFullName = "FOnline.RequiresCoverAttribute";

        // The cover primitives are engine-owned (FOnline.Sync), so they are matched by their symbol's full
        // metadata name. Matching a bare type name would let any project class called `Sync` silently
        // discharge an obligation it knows nothing about.
        public const string SyncTypeFullName = "FOnline.Sync";

        private const string Category = "Synchronization";

        internal static readonly DiagnosticDescriptor UnknownEntityRule = new DiagnosticDescriptor(
            id: "FOSYNC001",
            title: "[RequiresCover] names an entity that is not a parameter",
            messageFormat: "[RequiresCover] names '{0}', which is not a parameter of '{1}'",
            category: Category,
            defaultSeverity: DiagnosticSeverity.Warning,
            isEnabledByDefault: true,
            description:
                "Each [RequiresCover] entry must name a parameter of the annotated method so the contract cannot "
                + "drift away from the signature. Write it as nameof(parameter).");

        internal static readonly DiagnosticDescriptor UndischargedCoverRule = new DiagnosticDescriptor(
            id: "FOSYNC002",
            title: "Cover obligation is neither acquired nor propagated",
            messageFormat:
                "'{0}' requires the caller to hold cover for {1}; '{2}' neither acquires cover nor declares [RequiresCover]",
            category: Category,
            defaultSeverity: DiagnosticSeverity.Warning,
            isEnabledByDefault: true,
            description:
                "Calling a [RequiresCover] method obliges the caller to have established synchronization cover. "
                + "Either acquire it (Sync.Lock / Sync.Widen*) or re-declare [RequiresCover] so the obligation "
                + "travels to the next caller.");

        public override ImmutableArray<DiagnosticDescriptor> SupportedDiagnostics { get; } =
            ImmutableArray.Create(UnknownEntityRule, UndischargedCoverRule);

        public override void Initialize(AnalysisContext context)
        {
            context.ConfigureGeneratedCodeAnalysis(GeneratedCodeAnalysisFlags.None);
            context.EnableConcurrentExecution();

            context.RegisterCompilationStartAction(compilationStart =>
            {
                INamedTypeSymbol? requiresCover =
                    compilationStart.Compilation.GetTypeByMetadataName(RequiresCoverAttributeFullName);

                if (requiresCover == null) {
                    return;
                }

                compilationStart.RegisterSymbolAction(
                    symbolContext => AnalyzeDeclaration(symbolContext, requiresCover),
                    SymbolKind.Method);

                INamedTypeSymbol? syncType =
                    compilationStart.Compilation.GetTypeByMetadataName(SyncTypeFullName);

                compilationStart.RegisterSyntaxNodeAction(
                    nodeContext => AnalyzeInvocation(nodeContext, requiresCover, syncType),
                    SyntaxKind.InvocationExpression);
            });
        }

        // FOSYNC001 -- the declared contract must match the signature it sits on.
        private static void AnalyzeDeclaration(SymbolAnalysisContext context, INamedTypeSymbol requiresCover)
        {
            var method = (IMethodSymbol)context.Symbol;
            AttributeData? attribute = FindRequiresCover(method, requiresCover);

            if (attribute == null) {
                return;
            }

            var parameterNames = new HashSet<string>(method.Parameters.Select(p => p.Name));

            foreach (string entity in ReadEntities(attribute)) {
                // Only the root segment is a parameter: `cr.map` names a reachable entity through `cr`.
                string root = RootOf(entity);

                if (root.Length != 0 && !parameterNames.Contains(root)) {
                    Location location = attribute.ApplicationSyntaxReference?.GetSyntax(context.CancellationToken)
                        ?.GetLocation() ?? method.Locations.FirstOrDefault() ?? Location.None;

                    context.ReportDiagnostic(
                        Diagnostic.Create(UnknownEntityRule, location, entity, method.Name));
                }
            }
        }

        // FOSYNC002 -- the obligation must be discharged at every call site.
        private static void AnalyzeInvocation(SyntaxNodeAnalysisContext context, INamedTypeSymbol requiresCover, INamedTypeSymbol? syncType)
        {
            var invocation = (InvocationExpressionSyntax)context.Node;
            var callee = context.SemanticModel.GetSymbolInfo(invocation, context.CancellationToken).Symbol
                as IMethodSymbol;

            if (callee == null) {
                return;
            }

            AttributeData? calleeContract = FindRequiresCover(callee, requiresCover);

            if (calleeContract == null) {
                return;
            }

            SyntaxNode? body = EnclosingBody(invocation);

            if (body == null) {
                return;
            }

            var caller = context.SemanticModel.GetDeclaredSymbol(body, context.CancellationToken) as IMethodSymbol;

            // A caller that re-declares the obligation passes it on; that is a discharge here.
            if (caller != null && FindRequiresCover(caller, requiresCover) != null) {
                return;
            }

            if (AcquiresCover(body, context.SemanticModel, syncType, context.CancellationToken)) {
                return;
            }

            string entities = string.Join(", ", ReadEntities(calleeContract));

            context.ReportDiagnostic(
                Diagnostic.Create(
                    UndischargedCoverRule,
                    invocation.GetLocation(),
                    callee.Name,
                    entities.Length != 0 ? entities : "its entities",
                    caller?.Name ?? "the enclosing member"));
        }

        // v1 is body-scoped, not path-sensitive: any cover acquisition anywhere in the enclosing body
        // discharges the obligation. That direction is deliberate -- it under-reports rather than blocking a
        // build on a branch the analyzer cannot yet follow. Path sensitivity is the next increment and wants
        // a ControlFlowGraph walk, not a wider syntax scan.
        private static bool AcquiresCover(SyntaxNode body, SemanticModel model, INamedTypeSymbol? syncType, System.Threading.CancellationToken cancellationToken)
        {
            if (syncType == null) {
                return false;
            }

            foreach (InvocationExpressionSyntax candidate in body.DescendantNodes().OfType<InvocationExpressionSyntax>()) {
                var symbol = model.GetSymbolInfo(candidate, cancellationToken).Symbol as IMethodSymbol;

                if (symbol == null || symbol.ContainingType == null) {
                    continue;
                }

                if (!SymbolEqualityComparer.Default.Equals(symbol.ContainingType, syncType)) {
                    continue;
                }

                if (symbol.Name == "Lock" || symbol.Name.StartsWith("Lock", System.StringComparison.Ordinal) ||
                    symbol.Name.StartsWith("Widen", System.StringComparison.Ordinal) ||
                    symbol.Name.StartsWith("Restore", System.StringComparison.Ordinal)) {
                    return true;
                }
            }

            return false;
        }

        private static AttributeData? FindRequiresCover(IMethodSymbol method, INamedTypeSymbol requiresCover)
        {
            foreach (AttributeData attribute in method.GetAttributes()) {
                if (SymbolEqualityComparer.Default.Equals(attribute.AttributeClass, requiresCover)) {
                    return attribute;
                }
            }

            return null;
        }

        private static IEnumerable<string> ReadEntities(AttributeData attribute)
        {
            if (attribute.ConstructorArguments.Length == 0) {
                yield break;
            }

            TypedConstant argument = attribute.ConstructorArguments[0];

            if (argument.Kind != TypedConstantKind.Array) {
                yield break;
            }

            foreach (TypedConstant value in argument.Values) {
                if (value.Value is string text && text.Length != 0) {
                    yield return text;
                }
            }
        }

        private static string RootOf(string entity)
        {
            int dot = entity.IndexOf('.');

            return dot < 0 ? entity : entity.Substring(0, dot);
        }

        private static SyntaxNode? EnclosingBody(SyntaxNode node)
        {
            for (SyntaxNode? current = node; current != null; current = current.Parent) {
                if (current is MethodDeclarationSyntax || current is LocalFunctionStatementSyntax) {
                    return current;
                }
            }

            return null;
        }
    }
}
