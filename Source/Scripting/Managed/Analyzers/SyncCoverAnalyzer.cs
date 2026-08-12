#nullable enable

using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using System.Threading;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Diagnostics;

namespace FOnline.Analyzers
{
    // Compile-time checking of the entity synchronization cover contract declared by [RequiresCover] /
    // [ProvidesCover].
    //
    // This replaces the `// SyncScope:` comment convention and the external dataflow audit that read
    // AngelScript only. The contract lives on the parameter, so it survives refactoring, is visible in an IDE
    // while typing, and is checked by the same compiler pass that already gates code style.
    [DiagnosticAnalyzer(LanguageNames.CSharp)]
    public sealed class SyncCoverAnalyzer : DiagnosticAnalyzer
    {
        public const string RequiresCoverAttributeFullName = "FOnline.RequiresCoverAttribute";
        public const string ProvidesCoverAttributeFullName = "FOnline.ProvidesCoverAttribute";
        public const string EntityTypeFullName = "FOnline.Entity";

        // The cover primitives are engine-owned, so they are matched by their symbol's full metadata name.
        // Matching a bare type name would let any project class called `Sync` silently discharge an
        // obligation it knows nothing about.
        public const string SyncTypeFullName = "FOnline.Sync";

        // Methods that BEGIN an execution context: the engine dispatcher establishes cover for the arguments
        // it hands them, so their entity parameters arrive already covered and need no annotation. This is
        // sound only because such methods are called by their attribute rule and never directly from regular
        // code -- the project convention that keeps an entry point's assumption from leaking into an ordinary
        // call chain.
        private static readonly string[] EntryPointAttributeFullNames =
        {
            "FOnline.EventAttribute",
            "FOnline.TimeEventAttribute",
            "FOnline.ServerRemoteCallAttribute",
            "FOnline.ClientRemoteCallAttribute",
            "FOnline.AdminRemoteCallAttribute",
            "FOnline.ItemTriggerAttribute",
            "FOnline.ItemInitAttribute",
            "FOnline.ItemStaticAttribute",
            "FOnline.CritterInitAttribute",
            "FOnline.MapInitAttribute",
            "FOnline.LocationInitAttribute",
            "LastFrontier.DialogDemandAttribute",
            "LastFrontier.DialogResultAttribute",
        };

        private const string Category = "Synchronization";

        internal static readonly DiagnosticDescriptor NonEntityTargetRule = new DiagnosticDescriptor(
            id: "FOSYNC001",
            title: "Cover annotation on a non-entity target",
            messageFormat: "'{0}' is not an entity, so a cover annotation on it means nothing",
            category: Category,
            defaultSeverity: DiagnosticSeverity.Warning,
            isEnabledByDefault: true,
            description:
                "[RequiresCover] and [ProvidesCover] describe synchronization cover for an engine entity. "
                + "Applying one to a value that is neither an Entity nor a collection of entities declares a "
                + "contract that can never be satisfied or checked.");

        internal static readonly DiagnosticDescriptor UndischargedCoverRule = new DiagnosticDescriptor(
            id: "FOSYNC002",
            title: "Cover obligation is neither acquired nor propagated",
            messageFormat:
                "'{0}' requires the caller to hold cover for '{1}'; '{2}' neither acquires it, receives it from a "
                + "[ProvidesCover] source, nor declares [RequiresCover]",
            category: Category,
            defaultSeverity: DiagnosticSeverity.Warning,
            isEnabledByDefault: true,
            description:
                "Calling a method with a [RequiresCover] parameter obliges the caller to have established "
                + "synchronization cover for the argument. Acquire it (Sync.Lock / Sync.Widen*), pass a value "
                + "that came from a [ProvidesCover] source, or re-declare [RequiresCover] so the obligation "
                + "travels to the next caller.");

        internal static readonly DiagnosticDescriptor EntryPointCoverRule = new DiagnosticDescriptor(
            id: "FOSYNC003",
            title: "Entry point does not declare the cover the engine gives it",
            messageFormat: "'{0}' is an execution-context entry point; declare [RequiresCover] on '{1}', which the engine has already synchronized",
            category: Category,
            defaultSeverity: DiagnosticSeverity.Warning,
            isEnabledByDefault: true,
            description:
                "The engine synchronizes the subject it dispatches an execution context on -- a remote call's "
                + "Player, an event's own entity -- before the handler runs. Stating that with [RequiresCover] "
                + "makes the guarantee checkable and lets the obligation flow to everything the handler calls; "
                + "leaving it off means callees cannot rely on a contract that in fact holds.");

        public override ImmutableArray<DiagnosticDescriptor> SupportedDiagnostics { get; } =
            ImmutableArray.Create(NonEntityTargetRule, UndischargedCoverRule, EntryPointCoverRule);

        public override void Initialize(AnalysisContext context)
        {
            context.ConfigureGeneratedCodeAnalysis(GeneratedCodeAnalysisFlags.None);
            context.EnableConcurrentExecution();

            context.RegisterCompilationStartAction(compilationStart =>
            {
                Compilation compilation = compilationStart.Compilation;

                INamedTypeSymbol? requiresCover = compilation.GetTypeByMetadataName(RequiresCoverAttributeFullName);
                INamedTypeSymbol? providesCover = compilation.GetTypeByMetadataName(ProvidesCoverAttributeFullName);
                INamedTypeSymbol? entityType = compilation.GetTypeByMetadataName(EntityTypeFullName);

                if (requiresCover == null || entityType == null) {
                    return;
                }

                INamedTypeSymbol? syncType = compilation.GetTypeByMetadataName(SyncTypeFullName);

                var entryMarkers = new List<INamedTypeSymbol>();

                foreach (string name in EntryPointAttributeFullNames) {
                    INamedTypeSymbol? marker = compilation.GetTypeByMetadataName(name);

                    if (marker != null) {
                        entryMarkers.Add(marker);
                    }
                }

                var model = new CoverModel(requiresCover, providesCover, entityType, syncType, entryMarkers);

                compilationStart.RegisterSymbolAction(
                    symbolContext => AnalyzeDeclaration(symbolContext, model),
                    SymbolKind.Method);

                compilationStart.RegisterSyntaxNodeAction(
                    nodeContext => AnalyzeInvocation(nodeContext, model),
                    SyntaxKind.InvocationExpression);

                compilationStart.RegisterSyntaxNodeAction(
                    nodeContext => AnalyzeEntryPointDeclaration(nodeContext, model),
                    SyntaxKind.MethodDeclaration);
            });
        }

        // FOSYNC001 -- a cover annotation only means something on an entity (or a collection of entities).
        private static void AnalyzeDeclaration(SymbolAnalysisContext context, CoverModel model)
        {
            var method = (IMethodSymbol)context.Symbol;

            foreach (IParameterSymbol parameter in method.Parameters) {
                bool annotated = model.HasRequiresCover(parameter) || model.HasProvidesCover(parameter);

                if (annotated && !model.IsEntityish(parameter.Type)) {
                    Report(context, model, parameter, parameter.Name);
                }
            }

            if (model.HasProvidesCoverOnReturn(method) && !model.IsEntityish(method.ReturnType)) {
                Location location = method.Locations.FirstOrDefault() ?? Location.None;

                context.ReportDiagnostic(
                    Diagnostic.Create(NonEntityTargetRule, location, "the return value"));
            }
        }

        private static void Report(SymbolAnalysisContext context, CoverModel model, IParameterSymbol parameter, string display)
        {
            Location location = parameter.Locations.FirstOrDefault() ?? Location.None;

            context.ReportDiagnostic(Diagnostic.Create(NonEntityTargetRule, location, display));
        }

        // FOSYNC002 -- the obligation must be discharged at every call site.
        private static void AnalyzeInvocation(SyntaxNodeAnalysisContext context, CoverModel model)
        {
            var invocation = (InvocationExpressionSyntax)context.Node;
            SemanticModel semantics = context.SemanticModel;
            CancellationToken cancellationToken = context.CancellationToken;

            if (semantics.GetSymbolInfo(invocation, cancellationToken).Symbol is not IMethodSymbol callee) {
                return;
            }

            List<IParameterSymbol> demanding = callee.Parameters.Where(model.HasRequiresCover).ToList();
            bool demandsReceiver = model.HasRequiresCoverOnMethod(callee) && !callee.IsStatic;

            if (demanding.Count == 0 && !demandsReceiver) {
                return;
            }

            SyntaxNode? body = EnclosingBody(invocation);

            if (body == null) {
                return;
            }

            var caller = semantics.GetDeclaredSymbol(body, cancellationToken) as IMethodSymbol;

            // A caller that re-declares the obligation passes it on; that is a discharge here.
            if (caller != null && caller.Parameters.Any(model.HasRequiresCover)) {
                return;
            }

            // The cover primitives are the mechanism, not a consumer of it: `Sync` walks the hierarchy to
            // decide what to acquire, so demanding that it already hold cover for what it is about to acquire
            // is circular. Nothing else is exempt.
            if (caller != null && model.SyncType != null &&
                SymbolEqualityComparer.Default.Equals(caller.ContainingType, model.SyncType)) {
                return;
            }

            if (AcquiresCover(body, semantics, model, cancellationToken)) {
                return;
            }

            if (demandsReceiver && invocation.Expression is MemberAccessExpressionSyntax memberAccess) {
                ExpressionSyntax receiver = memberAccess.Expression;

                if (!model.ComesFromProvidedCover(receiver, semantics, cancellationToken) &&
                    !model.CoveredByEarlierCall(body, receiver, semantics, cancellationToken)) {
                    context.ReportDiagnostic(
                        Diagnostic.Create(
                            UndischargedCoverRule,
                            receiver.GetLocation(),
                            callee.Name,
                            "the receiver",
                            caller?.Name ?? "the enclosing member"));
                }
            }

            foreach (IParameterSymbol parameter in demanding) {
                ExpressionSyntax? argument = ArgumentFor(invocation, callee, parameter);

                // An argument handed over by a [ProvidesCover] source arrives already covered.
                if (argument != null && model.ComesFromProvidedCover(argument, semantics, cancellationToken)) {
                    continue;
                }

                // Cover can also be established mid-flight by handing the value to a [ProvidesCover]
                // parameter of some earlier call -- Sync.Widen* is not the only way to acquire it.
                if (argument != null &&
                    model.CoveredByEarlierCall(body, argument, semantics, cancellationToken)) {
                    continue;
                }

                context.ReportDiagnostic(
                    Diagnostic.Create(
                        UndischargedCoverRule,
                        (argument ?? (ExpressionSyntax)invocation).GetLocation(),
                        callee.Name,
                        parameter.Name,
                        caller?.Name ?? "the enclosing member"));
            }
        }

        // FOSYNC003 -- an entry point must state the cover the engine already established for it.
        //
        // The engine synchronizes the subject it dispatches on before the context starts, so the first entity
        // parameter arrives covered. Declaring that is what lets the obligation flow onward: an entry point
        // carrying [RequiresCover] discharges FOSYNC002 for everything it calls with that argument, by the
        // ordinary propagation rule. Only the first entity parameter is checked -- the rest are whatever the
        // handler itself decides to acquire.
        private static void AnalyzeEntryPointDeclaration(SyntaxNodeAnalysisContext context, CoverModel model)
        {
            var declaration = (MethodDeclarationSyntax)context.Node;

            if (context.SemanticModel.GetDeclaredSymbol(declaration, context.CancellationToken) is not IMethodSymbol method) {
                return;
            }

            if (!model.IsEntryPoint(method)) {
                return;
            }

            foreach (IParameterSymbol parameter in method.Parameters) {
                if (!model.IsEntityish(parameter.Type)) {
                    continue;
                }

                if (!model.HasRequiresCover(parameter)) {
                    context.ReportDiagnostic(
                        Diagnostic.Create(
                            EntryPointCoverRule,
                            DeclarationLocation(parameter, method, context.CancellationToken),
                            method.Name,
                            parameter.Name));
                }

                return;
            }
        }

        // v1 is body-scoped, not path-sensitive: any cover acquisition anywhere in the enclosing body
        // discharges the obligation. That direction is deliberate -- it under-reports rather than blocking a
        // build on a branch the analyzer cannot yet follow. Path sensitivity wants a ControlFlowGraph walk,
        // not a wider syntax scan.
        private static bool AcquiresCover(SyntaxNode body, SemanticModel semantics, CoverModel model, CancellationToken cancellationToken)
        {
            if (model.SyncType == null) {
                return false;
            }

            foreach (InvocationExpressionSyntax candidate in body.DescendantNodes().OfType<InvocationExpressionSyntax>()) {
                if (semantics.GetSymbolInfo(candidate, cancellationToken).Symbol is not IMethodSymbol symbol) {
                    continue;
                }

                if (!SymbolEqualityComparer.Default.Equals(symbol.ContainingType, model.SyncType)) {
                    continue;
                }

                if (symbol.Name.StartsWith("Lock", System.StringComparison.Ordinal) ||
                    symbol.Name.StartsWith("Widen", System.StringComparison.Ordinal) ||
                    symbol.Name.StartsWith("Restore", System.StringComparison.Ordinal)) {
                    return true;
                }
            }

            return false;
        }

        // The whole parameter declaration, not just its identifier: it is what a reader wants highlighted, and
        // it is where an inserted attribute has to go -- `[RequiresCover] Critter cr`, never `Critter [..] cr`.
        private static Location DeclarationLocation(IParameterSymbol parameter, IMethodSymbol method, CancellationToken cancellationToken)
        {
            foreach (SyntaxReference reference in parameter.DeclaringSyntaxReferences) {
                if (reference.GetSyntax(cancellationToken) is ParameterSyntax syntax) {
                    return syntax.GetLocation();
                }
            }

            return parameter.Locations.FirstOrDefault() ?? method.Locations.FirstOrDefault() ?? Location.None;
        }

        private static ExpressionSyntax? ArgumentFor(InvocationExpressionSyntax invocation, IMethodSymbol callee, IParameterSymbol parameter)
        {
            SeparatedSyntaxList<ArgumentSyntax> arguments = invocation.ArgumentList.Arguments;

            for (int i = 0; i < arguments.Count; i++) {
                ArgumentSyntax argument = arguments[i];

                if (argument.NameColon != null) {
                    if (argument.NameColon.Name.Identifier.ValueText == parameter.Name) {
                        return argument.Expression;
                    }

                    continue;
                }

                if (i < callee.Parameters.Length && SymbolEqualityComparer.Default.Equals(callee.Parameters[i], parameter)) {
                    return argument.Expression;
                }
            }

            return null;
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

        // Resolved contract vocabulary for one compilation.
        private sealed class CoverModel
        {
            private readonly INamedTypeSymbol _requiresCover;
            private readonly INamedTypeSymbol? _providesCover;
            private readonly INamedTypeSymbol _entityType;

            private readonly List<INamedTypeSymbol> _entryMarkers;

            public CoverModel(INamedTypeSymbol requiresCover, INamedTypeSymbol? providesCover, INamedTypeSymbol entityType, INamedTypeSymbol? syncType, List<INamedTypeSymbol> entryMarkers)
            {
                _requiresCover = requiresCover;
                _providesCover = providesCover;
                _entityType = entityType;
                SyncType = syncType;
                _entryMarkers = entryMarkers;
            }

            public bool IsEntryPoint(IMethodSymbol method)
            {
                foreach (INamedTypeSymbol marker in _entryMarkers) {
                    if (HasAttribute(method.GetAttributes(), marker)) {
                        return true;
                    }
                }

                return false;
            }

            public INamedTypeSymbol? SyncType { get; }

            public bool HasRequiresCover(IParameterSymbol parameter)
            {
                return HasAttribute(parameter.GetAttributes(), _requiresCover);
            }


            // On a method the attribute names the receiver, which no parameter can express.
            public bool HasRequiresCoverOnMethod(IMethodSymbol method)
            {
                return HasAttribute(method.GetAttributes(), _requiresCover);
            }

            public bool HasProvidesCover(IParameterSymbol parameter)
            {
                return _providesCover != null && HasAttribute(parameter.GetAttributes(), _providesCover);
            }

            public bool HasProvidesCoverOnReturn(IMethodSymbol method)
            {
                return _providesCover != null && HasAttribute(method.GetReturnTypeAttributes(), _providesCover);
            }

            // An entity, or a collection of them: `List<Critter>` carries the contract element-wise, which is
            // why the old string grammar needed a `[*]` marker and this one does not.
            public bool IsEntityish(ITypeSymbol type)
            {
                if (IsEntity(type)) {
                    return true;
                }

                if (type is IArrayTypeSymbol array) {
                    return IsEntity(array.ElementType);
                }

                if (type is INamedTypeSymbol named && named.IsGenericType) {
                    return named.TypeArguments.Any(IsEntity);
                }

                return false;
            }

            // The value flows straight out of a call whose return value declares [ProvidesCover].
            public bool ComesFromProvidedCover(ExpressionSyntax expression, SemanticModel semantics, CancellationToken cancellationToken)
            {
                if (_providesCover == null) {
                    return false;
                }

                if (semantics.GetSymbolInfo(expression, cancellationToken).Symbol is IMethodSymbol direct) {
                    return HasProvidesCoverOnReturn(direct);
                }

                // The usual shape is a local initialized from such a call, then passed on.
                if (semantics.GetSymbolInfo(expression, cancellationToken).Symbol is not ILocalSymbol local) {
                    return false;
                }

                foreach (SyntaxReference reference in local.DeclaringSyntaxReferences) {
                    if (reference.GetSyntax(cancellationToken) is not VariableDeclaratorSyntax declarator) {
                        continue;
                    }

                    ExpressionSyntax? initializer = declarator.Initializer?.Value;

                    if (initializer == null) {
                        continue;
                    }

                    if (initializer is AwaitExpressionSyntax awaited) {
                        initializer = awaited.Expression;
                    }

                    if (semantics.GetSymbolInfo(initializer, cancellationToken).Symbol is IMethodSymbol source &&
                        HasProvidesCoverOnReturn(source)) {
                        return true;
                    }
                }

                return false;
            }

            // Some earlier call in this body handed the same value to a [ProvidesCover] parameter, which is
            // how a helper establishes cover for something it does not return. Body-scoped like the rest of
            // the discharge rule, so it under-reports rather than blocking a build on control flow the
            // analyzer cannot yet follow.
            public bool CoveredByEarlierCall(SyntaxNode body, ExpressionSyntax expression, SemanticModel semantics, CancellationToken cancellationToken)
            {
                if (_providesCover == null) {
                    return false;
                }

                ISymbol? wanted = semantics.GetSymbolInfo(expression, cancellationToken).Symbol;

                if (wanted == null) {
                    return false;
                }

                foreach (InvocationExpressionSyntax candidate in body.DescendantNodes().OfType<InvocationExpressionSyntax>()) {
                    if (semantics.GetSymbolInfo(candidate, cancellationToken).Symbol is not IMethodSymbol callee) {
                        continue;
                    }

                    SeparatedSyntaxList<ArgumentSyntax> arguments = candidate.ArgumentList.Arguments;

                    for (int i = 0; i < arguments.Count && i < callee.Parameters.Length; i++) {
                        if (!HasProvidesCover(callee.Parameters[i])) {
                            continue;
                        }

                        ISymbol? passed = semantics.GetSymbolInfo(arguments[i].Expression, cancellationToken).Symbol;

                        if (passed != null && SymbolEqualityComparer.Default.Equals(passed, wanted)) {
                            return true;
                        }
                    }
                }

                return false;
            }


            private bool IsEntity(ITypeSymbol type)
            {
                for (ITypeSymbol? current = type; current != null; current = current.BaseType) {
                    if (SymbolEqualityComparer.Default.Equals(current, _entityType)) {
                        return true;
                    }
                }

                return false;
            }

            private static bool HasAttribute(ImmutableArray<AttributeData> attributes, INamedTypeSymbol wanted)
            {
                foreach (AttributeData attribute in attributes) {
                    if (SymbolEqualityComparer.Default.Equals(attribute.AttributeClass, wanted)) {
                        return true;
                    }
                }

                return false;
            }
        }
    }
}
