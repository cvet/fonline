namespace FOnline.Analyzers;

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using System.Threading;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Diagnostics;
using Microsoft.CodeAnalysis.Operations;

// FOSYNC015 -- a widening that names only entities the caller already covers. Sync.Snapshot is the one observer of the
// own lock such a call adds, so the rule stays silent wherever a snapshot may follow in the same script entry
public sealed partial class SyncCoverAnalyzer
{
    internal static readonly DiagnosticDescriptor CoveredWideningRule = new DiagnosticDescriptor(
        id: "FOSYNC015", title: "A widening acquisition names only entities the caller already covers",
        messageFormat: "'{0}' acquires nothing the caller does not already hold ({1}); the engine takes an entity's " +
            "own lock itself where a mutation needs it, so the call only adds a suspension point, and the one covered " +
            "entity it refuses is a destroyed one, which is a test of IsDestroyed",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "Holding an entity covers everything beneath it in the sync hierarchy, and a dispatcher's cover is " +
            "the caller's cover. A Sync helper declared [CoversOnlyArguments] covers exactly what its [ProvidesCover] " +
            "parameters state, so when every argument is already covered with the declared reach and no await or cover " +
            "change has released that cover since, the call acquires nothing. It still makes the method asynchronous, " +
            "which is exactly what a handler that must finish before its event returns cannot afford. The rule is " +
            "silent where Sync.Snapshot may run later in the same script entry, because the snapshot reports the " +
            "context's own locks and would miss an entity the call made explicit.",
        customTags: WellKnownDiagnosticTags.CompilationEnd);

    private static void RegisterCoveredWidening(CompilationStartAnalysisContext compilationStart, CoverModel model)
    {
        if (!model.CanAcquireCover || model.Declarations.CoversOnlyArgumentsAttribute == null) {
            return;
        }

        var collector = new CoveredWideningCollector(model);

        compilationStart.RegisterOperationAction(collector.CollectCall,
                                                 OperationKind.Invocation,
                                                 OperationKind.ObjectCreation,
                                                 OperationKind.PropertyReference,
                                                 OperationKind.DelegateCreation);
        compilationStart.RegisterSymbolAction(collector.CollectDispatch, SymbolKind.Method);
        compilationStart.RegisterSyntaxNodeAction(collector.CollectCandidate, SyntaxKind.InvocationExpression);
        compilationStart.RegisterCompilationEndAction(collector.Report);
    }

    private enum CallKind
    {
        Call,

        // A reflection invoke, through which any [CallableByName] method may run
        Reflective,
    }

    // One call edge of the whole compilation: who calls what, from where, and inside which lambdas (innermost first)
    private sealed class CallRecord
    {
        public CallRecord(IMethodSymbol owner, IMethodSymbol target, SyntaxNode node,
                          ImmutableArray<SyntaxNode> lambdas, bool awaited, CallKind kind, ITypeSymbol? delegateType,
                          bool inTests)
        {
            InTests = inTests;
            Owner = owner;
            Target = target;
            Node = node;
            Lambdas = lambdas;
            Awaited = awaited;
            Kind = kind;
            DelegateType = delegateType;
        }

        public IMethodSymbol Owner { get; }

        public IMethodSymbol Target { get; }

        public SyntaxNode Node { get; }

        public ImmutableArray<SyntaxNode> Lambdas { get; }

        public bool Awaited { get; }

        public CallKind Kind { get; }

        // The delegate type an invocation goes through, as constructed at the call
        public ITypeSymbol? DelegateType { get; }

        public bool InTests { get; }
    }

    // A delegate built from a method group or a lambda. One handed straight to a method outside the compilation runs
    // inside that call; any other runs wherever a delegate of its type is invoked
    private sealed class DelegateRecord
    {
        public DelegateRecord(ITypeSymbol key, IMethodSymbol? method, SyntaxNode? lambda, IMethodSymbol? owner,
                              SyntaxNode node, ImmutableArray<SyntaxNode> lambdas, bool inline, bool inTests)
        {
            InTests = inTests;
            Key = key;
            Method = method;
            Lambda = lambda;
            Owner = owner;
            Node = node;
            Lambdas = lambdas;
            Inline = inline;
        }

        public ITypeSymbol Key { get; }

        public IMethodSymbol? Method { get; }

        public SyntaxNode? Lambda { get; }

        public IMethodSymbol? Owner { get; }

        public SyntaxNode Node { get; }

        public ImmutableArray<SyntaxNode> Lambdas { get; }

        public bool Inline { get; }

        public bool InTests { get; }
    }

    // A widening whose arguments are covered by what the call site shows; whether that cover survived to the call and
    // whether a snapshot can follow is decided once the whole call graph is known
    private sealed class CoveredWideningCandidate
    {
        public CoveredWideningCandidate(InvocationExpressionSyntax node, IMethodSymbol owner, IMethodSymbol callee,
                                        SyntaxNode body, List<ISymbol> roots, List<string> reasons, bool inTests)
        {
            InTests = inTests;
            Node = node;
            Owner = owner;
            Callee = callee;
            Body = body;
            Roots = roots;
            Reasons = reasons;
        }

        public InvocationExpressionSyntax Node { get; }

        public IMethodSymbol Owner { get; }

        public IMethodSymbol Callee { get; }

        public SyntaxNode Body { get; }

        public List<ISymbol> Roots { get; }

        public List<string> Reasons { get; }

        public bool InTests { get; }
    }

    // What the arguments of one call are covered by, gathered while they are read
    private sealed class CoverageProof
    {
        public List<ISymbol> Roots { get; } = new List<ISymbol>();

        public List<string> Reasons { get; } = new List<string>();

        public int NamedEntities { get; set; }
    }

    private sealed class CoveredWideningCollector
    {
        private const int ReachParent = 1 << 0;
        private const int ReachAncestors = 1 << 1;
        private const int MaxDepth = 8;
        private const string CallableByNameAttributeFullName = "FOnline.CallableByNameAttribute";

        // Set in .editorconfig on the sources that never ship: their calls into shipped code do not decide whether a
        // widening there is needed, because shipped code runs without them
        public const string TestCodeOption = "fonline_sync.test_code";

        private readonly CoverModel Model;
        private readonly ConcurrentBag<CallRecord> Calls = new ConcurrentBag<CallRecord>();
        private readonly ConcurrentBag<DelegateRecord> Delegates = new ConcurrentBag<DelegateRecord>();
        private readonly ConcurrentBag<(IMethodSymbol Derived, IMethodSymbol Base)> Dispatch =
            new ConcurrentBag<(IMethodSymbol Derived, IMethodSymbol Base)>();
        private readonly ConcurrentBag<CoveredWideningCandidate> Candidates =
            new ConcurrentBag<CoveredWideningCandidate>();
        private readonly Dictionary<IMethodSymbol, bool> Suspension =
            new Dictionary<IMethodSymbol, bool>(SymbolEqualityComparer.Default);

        public CoveredWideningCollector(CoverModel model)
        {
            Model = model;
        }

        public void CollectCall(OperationAnalysisContext context)
        {
            IOperation operation = context.Operation;
            SemanticModel? semantics = operation.SemanticModel;

            if (semantics == null) {
                return;
            }

            bool inTests = IsTestCode(context.Options, operation.Syntax.SyntaxTree);

            switch (operation) {
            case IDelegateCreationOperation creation:
                RecordDelegate(creation, semantics, inTests, context.CancellationToken);
                return;

            case IPropertyReferenceOperation property:
                Record(operation,
                       property.Property.GetMethod,
                       CallKind.Call,
                       semantics,
                       inTests,
                       context.CancellationToken);
                Record(operation,
                       property.Property.SetMethod,
                       CallKind.Call,
                       semantics,
                       inTests,
                       context.CancellationToken);
                return;

            case IInvocationOperation invocation:
                Record(operation,
                       invocation.TargetMethod,
                       IsReflectiveInvoke(invocation.TargetMethod) ? CallKind.Reflective : CallKind.Call,
                       semantics,
                       inTests,
                       context.CancellationToken);
                return;

            case IObjectCreationOperation objectCreation:
                Record(operation,
                       objectCreation.Constructor,
                       CallKind.Call,
                       semantics,
                       inTests,
                       context.CancellationToken);
                return;
            }
        }

        private void Record(IOperation operation, IMethodSymbol? target, CallKind kind, SemanticModel semantics,
                            bool inTests, CancellationToken cancellationToken)
        {
            if (target == null || (kind == CallKind.Call && !Relevant(target))) {
                return;
            }

            (IMethodSymbol? owner, ImmutableArray<SyntaxNode> lambdas) = Owner(operation.Syntax, semantics,
                                                                               cancellationToken);

            if (owner != null) {
                ITypeSymbol? delegateType =
                    target.MethodKind == MethodKind.DelegateInvoke ? target.ContainingType : null;

                Calls.Add(new CallRecord(owner,
                                         target.OriginalDefinition,
                                         operation.Syntax,
                                         lambdas,
                                         IsAwaited(operation.Syntax),
                                         kind,
                                         delegateType,
                                         inTests));
            }
        }

        private void RecordDelegate(IDelegateCreationOperation creation, SemanticModel semantics, bool inTests,
                                    CancellationToken cancellationToken)
        {
            IMethodSymbol? method = (creation.Target as IMethodReferenceOperation)?.Method;
            SyntaxNode? lambda = (creation.Target as IAnonymousFunctionOperation)?.Syntax;

            if (creation.Type == null || (method == null && lambda == null)) {
                return;
            }

            // A lambda's own syntax would count as enclosing it, so its context is read from the parent
            SyntaxNode site = lambda?.Parent ?? creation.Syntax;
            (IMethodSymbol? owner, ImmutableArray<SyntaxNode> lambdas) = Owner(site, semantics, cancellationToken);
            bool inline = creation.Parent is IArgumentOperation { Parent : IInvocationOperation receiver } &&
                          receiver.TargetMethod.DeclaringSyntaxReferences.Length == 0 &&
                          receiver.TargetMethod.MethodKind != MethodKind.DelegateInvoke;

            Delegates.Add(new DelegateRecord(creation.Type,
                                             method?.OriginalDefinition,
                                             lambda,
                                             owner,
                                             creation.Syntax,
                                             lambdas,
                                             inline,
                                             inTests));
        }

        // A call to a virtual or interface member runs whichever override the object has, so each override a source
        // method provides is an edge from the member it overrides or implements
        public void CollectDispatch(SymbolAnalysisContext context)
        {
            var method = (IMethodSymbol)context.Symbol;
            IMethodSymbol derived = method.OriginalDefinition;

            if (method.OverriddenMethod != null) {
                Dispatch.Add((derived, method.OverriddenMethod.OriginalDefinition));
            }

            foreach (IMethodSymbol implemented in method.ExplicitInterfaceImplementations) {
                Dispatch.Add((derived, implemented.OriginalDefinition));
            }

            foreach (INamedTypeSymbol contract in method.ContainingType.AllInterfaces) {
                foreach (IMethodSymbol member in contract.GetMembers().OfType<IMethodSymbol>()) {
                    if (SymbolEqualityComparer.Default.Equals(
                            method.ContainingType.FindImplementationForInterfaceMember(member),
                            method)) {
                        Dispatch.Add((derived, member.OriginalDefinition));
                    }
                }
            }
        }

        private static bool IsTestCode(AnalyzerOptions options, SyntaxTree tree)
        {
            return options.AnalyzerConfigOptionsProvider.GetOptions(tree).TryGetValue(TestCodeOption, out string? value) &&
                   value == "true";
        }

        // Metadata carries no Sync call, so only source methods, delegates and the declared effects matter
        private bool Relevant(IMethodSymbol target)
        {
            return target.MethodKind == MethodKind.DelegateInvoke || Model.EffectOf(target) != null ||
                   target.DeclaringSyntaxReferences.Length != 0;
        }

        private static bool IsReflectiveInvoke(IMethodSymbol target)
        {
            return target.Name == "Invoke" && target.ContainingType?.ToDisplayString() is
                                              "System.Reflection.MethodBase" or "System.Reflection.MethodInfo";
        }

        // A delegate type still naming a type parameter stands for every construction of its definition
        public static bool IsOpenType(ITypeSymbol type)
        {
            return type switch {
                ITypeParameterSymbol => true,
                IArrayTypeSymbol array => IsOpenType(array.ElementType),
                INamedTypeSymbol named => named.TypeArguments.Any(IsOpenType),
                _ => false,
            };
        }

        public void CollectCandidate(SyntaxNodeAnalysisContext context)
        {
            var invocation = (InvocationExpressionSyntax)context.Node;
            SemanticModel semantics = context.SemanticModel;
            CancellationToken cancellationToken = context.CancellationToken;

            if (semantics.GetSymbolInfo(invocation, cancellationToken).Symbol is not IMethodSymbol callee ||
                !Model.Declarations.IsSyncMember(callee) || Model.EffectOf(callee) != CoverEffect.Extend ||
                !Model.Declarations.CoversOnlyArguments(callee)) {
                return;
            }

            (IMethodSymbol? owner, ImmutableArray<SyntaxNode> lambdas) = Owner(invocation, semantics, cancellationToken);

            if (owner == null || lambdas.Length != 0 || owner.MethodKind != MethodKind.Ordinary ||
                Model.Declarations.IsSyncMember(owner)) {
                return;
            }

            SyntaxNode? body = BodyOf(owner, cancellationToken);

            if (body == null) {
                return;
            }

            var proof = new CoverageProof();

            foreach (ArgumentSyntax argument in invocation.ArgumentList.Arguments) {
                IParameterSymbol? parameter = BoundParameter(invocation, callee, argument);

                if (parameter == null || !Model.IsEntityish(parameter.Type)) {
                    continue;
                }

                // A parameter the helper does not declare it covers is an input, a prototype filter for instance
                int? reach = Model.Declarations.DeclaredReach(parameter.GetAttributes());

                if (reach == null) {
                    continue;
                }

                bool needParent = (reach.Value & (ReachParent | ReachAncestors)) != 0;
                bool covered = Model.Declarations.IsEntity(parameter.Type) ? CoveredValue(argument.Expression,
                                                                                          needParent,
                                                                                          owner,
                                                                                          body,
                                                                                          invocation,
                                                                                          proof,
                                                                                          semantics,
                                                                                          cancellationToken,
                                                                                          0)
                                                                           : CoveredCollection(argument.Expression,
                                                                                               needParent,
                                                                                               owner,
                                                                                               body,
                                                                                               invocation,
                                                                                               proof,
                                                                                               semantics,
                                                                                               cancellationToken,
                                                                                               0);

                if (!covered) {
                    return;
                }
            }

            // Widening nothing refreshes a moved owner's ancestor marks, which is work of its own
            if (proof.NamedEntities == 0) {
                return;
            }

            Candidates.Add(new CoveredWideningCandidate(invocation,
                                                        owner,
                                                        callee,
                                                        body,
                                                        proof.Roots,
                                                        proof.Reasons,
                                                        IsTestCode(context.Options, invocation.SyntaxTree)));
        }

        // An entity argument that the call site proves covered, recording what that proof rests on
        private bool CoveredValue(ExpressionSyntax expression, bool needParent, IMethodSymbol owner, SyntaxNode body,
                                  InvocationExpressionSyntax site, CoverageProof proof, SemanticModel semantics,
                                  CancellationToken cancellationToken, int depth)
        {
            if (depth > MaxDepth) {
                return false;
            }

            ExpressionSyntax value = Unwrap(expression);

            if (IsNullValue(value, semantics, cancellationToken)) {
                return true;
            }

            proof.NamedEntities++;

            if (Model.IsAlwaysCovered(semantics.GetTypeInfo(value, cancellationToken).Type)) {
                return true;
            }

            switch (value) {
            case AwaitExpressionSyntax awaited:
                return CoveredValue(awaited.Expression,
                                    needParent,
                                    owner,
                                    body,
                                    site,
                                    proof,
                                    semantics,
                                    cancellationToken,
                                    depth + 1);

            case ElementAccessExpressionSyntax element:
                return CoveredCollection(element.Expression,
                                         needParent,
                                         owner,
                                         body,
                                         site,
                                         proof,
                                         semantics,
                                         cancellationToken,
                                         depth + 1);

            case ConditionalExpressionSyntax choice:
                return CoveredValue(choice.WhenTrue,
                                    needParent,
                                    owner,
                                    body,
                                    site,
                                    proof,
                                    semantics,
                                    cancellationToken,
                                    depth + 1) &&
                       CoveredValue(choice.WhenFalse,
                                    needParent,
                                    owner,
                                    body,
                                    site,
                                    proof,
                                    semantics,
                                    cancellationToken,
                                    depth + 1);

            case BinaryExpressionSyntax coalesce when coalesce.IsKind(SyntaxKind.CoalesceExpression):
                return CoveredValue(coalesce.Left,
                                    needParent,
                                    owner,
                                    body,
                                    site,
                                    proof,
                                    semantics,
                                    cancellationToken,
                                    depth + 1) &&
                       CoveredValue(coalesce.Right,
                                    needParent,
                                    owner,
                                    body,
                                    site,
                                    proof,
                                    semantics,
                                    cancellationToken,
                                    depth + 1);

            case InvocationExpressionSyntax call:
                return ProvidedByCall(call, needParent, owner, body, site, proof, semantics, cancellationToken, depth);
            }

            ISymbol? symbol = semantics.GetSymbolInfo(value, cancellationToken).Symbol;

            if (symbol is IParameterSymbol parameter) {
                return CoveredParameter(parameter, needParent, owner, proof);
            }

            if (symbol is not ILocalSymbol local ||
                !AssignedOnlyAtDeclaration(local, body, semantics, cancellationToken)) {
                return false;
            }

            foreach (SyntaxReference reference in local.DeclaringSyntaxReferences) {
                SyntaxNode declaration = reference.GetSyntax(cancellationToken);

                // `foreach (Item item in cr.GetItems())` binds each element of a covered collection
                if (declaration is ForEachStatementSyntax loop) {
                    proof.Roots.Add(local);
                    return CoveredCollection(loop.Expression,
                                             needParent,
                                             owner,
                                             body,
                                             site,
                                             proof,
                                             semantics,
                                             cancellationToken,
                                             depth + 1);
                }

                if (declaration is VariableDeclaratorSyntax { Initializer.Value : {} initializer }) {
                    proof.Roots.Add(local);
                    proof.NamedEntities--;
                    return CoveredValue(initializer,
                                        needParent,
                                        owner,
                                        body,
                                        site,
                                        proof,
                                        semantics,
                                        cancellationToken,
                                        depth + 1);
                }
            }

            return false;
        }

        // A collection argument, element-wise: a covered collection, a set written in place, or a local list whose
        // every contribution in the body is visible and covered
        private bool CoveredCollection(ExpressionSyntax expression, bool needParent, IMethodSymbol owner,
                                       SyntaxNode body, InvocationExpressionSyntax site, CoverageProof proof,
                                       SemanticModel semantics, CancellationToken cancellationToken, int depth)
        {
            if (depth > MaxDepth) {
                return false;
            }

            ExpressionSyntax value = Unwrap(expression);

            if (IsNullValue(value, semantics, cancellationToken)) {
                return true;
            }

            ITypeSymbol? elementType = ElementType(semantics.GetTypeInfo(value, cancellationToken).Type);

            if (elementType != null && Model.IsAlwaysCovered(elementType)) {
                proof.NamedEntities++;
                return true;
            }

            switch (value) {
            case AwaitExpressionSyntax awaited:
                return CoveredCollection(awaited.Expression,
                                         needParent,
                                         owner,
                                         body,
                                         site,
                                         proof,
                                         semantics,
                                         cancellationToken,
                                         depth + 1);

            case ConditionalExpressionSyntax choice:
                return CoveredCollection(choice.WhenTrue,
                                         needParent,
                                         owner,
                                         body,
                                         site,
                                         proof,
                                         semantics,
                                         cancellationToken,
                                         depth + 1) &&
                       CoveredCollection(choice.WhenFalse,
                                         needParent,
                                         owner,
                                         body,
                                         site,
                                         proof,
                                         semantics,
                                         cancellationToken,
                                         depth + 1);

            case InvocationExpressionSyntax call:
                proof.NamedEntities++;
                return ProvidedByCall(call, needParent, owner, body, site, proof, semantics, cancellationToken, depth);

            case CollectionExpressionSyntax collection:
                foreach (CollectionElementSyntax element in collection.Elements) {
                    bool covered = element switch {
                        ExpressionElementSyntax single => CoveredValue(single.Expression,
                                                                       needParent,
                                                                       owner,
                                                                       body,
                                                                       site,
                                                                       proof,
                                                                       semantics,
                                                                       cancellationToken,
                                                                       depth + 1),
                        SpreadElementSyntax spread => CoveredCollection(spread.Expression,
                                                                        needParent,
                                                                        owner,
                                                                        body,
                                                                        site,
                                                                        proof,
                                                                        semantics,
                                                                        cancellationToken,
                                                                        depth + 1),
                        _ => false,
                    };

                    if (!covered) {
                        return false;
                    }
                }

                return true;

            case BaseObjectCreationExpressionSyntax creation:
                return CoveredCreation(creation,
                                       needParent,
                                       owner,
                                       body,
                                       site,
                                       proof,
                                       semantics,
                                       cancellationToken,
                                       depth);

            case InitializerExpressionSyntax initializer:
                return CoveredElements(initializer,
                                       needParent,
                                       owner,
                                       body,
                                       site,
                                       proof,
                                       semantics,
                                       cancellationToken,
                                       depth);

            case ArrayCreationExpressionSyntax array:
                return array.Initializer != null && CoveredElements(array.Initializer,
                                                                    needParent,
                                                                    owner,
                                                                    body,
                                                                    site,
                                                                    proof,
                                                                    semantics,
                                                                    cancellationToken,
                                                                    depth);

            case ImplicitArrayCreationExpressionSyntax array:
                return CoveredElements(array.Initializer,
                                       needParent,
                                       owner,
                                       body,
                                       site,
                                       proof,
                                       semantics,
                                       cancellationToken,
                                       depth);
            }

            ISymbol? symbol = semantics.GetSymbolInfo(value, cancellationToken).Symbol;

            if (symbol is IParameterSymbol parameter) {
                proof.NamedEntities++;
                return CoveredParameter(parameter, needParent, owner, proof);
            }

            if (symbol is not ILocalSymbol local ||
                !AssignedOnlyAtDeclaration(local, body, semantics, cancellationToken)) {
                return false;
            }

            ExpressionSyntax? declared = null;

            foreach (SyntaxReference reference in local.DeclaringSyntaxReferences) {
                if (reference.GetSyntax(cancellationToken) is
                    VariableDeclaratorSyntax { Initializer.Value : {} init }) {
                    declared = init;
                }
            }

            if (declared == null || !CoveredCollection(declared,
                                                       needParent,
                                                       owner,
                                                       body,
                                                       site,
                                                       proof,
                                                       semantics,
                                                       cancellationToken,
                                                       depth + 1)) {
                return false;
            }

            proof.Roots.Add(local);

            return ContributionsCovered(local,
                                        needParent,
                                        owner,
                                        body,
                                        site,
                                        proof,
                                        semantics,
                                        cancellationToken,
                                        depth);
        }

        private bool CoveredCreation(BaseObjectCreationExpressionSyntax creation, bool needParent, IMethodSymbol owner,
                                     SyntaxNode body, InvocationExpressionSyntax site, CoverageProof proof,
                                     SemanticModel semantics, CancellationToken cancellationToken, int depth)
        {
            if (semantics.GetSymbolInfo(creation, cancellationToken).Symbol is not IMethodSymbol constructor ||
                !SymbolEqualityComparer.Default.Equals(
                    constructor.ContainingType.OriginalDefinition,
                    semantics.Compilation.GetTypeByMetadataName("System.Collections.Generic.List`1"))) {
                return false;
            }

            SeparatedSyntaxList<ArgumentSyntax> arguments =
                creation.ArgumentList?.Arguments ?? default(SeparatedSyntaxList<ArgumentSyntax>);

            // The copy constructor brings its source's elements, a capacity brings none
            if (arguments.Count == 1) {
                ITypeSymbol parameterType = constructor.Parameters[0].Type;

                if (parameterType.SpecialType == SpecialType.System_Int32) {
                    return creation.Initializer == null || CoveredElements(creation.Initializer,
                                                                           needParent,
                                                                           owner,
                                                                           body,
                                                                           site,
                                                                           proof,
                                                                           semantics,
                                                                           cancellationToken,
                                                                           depth);
                }

                if (!CoveredCollection(arguments[0].Expression,
                                       needParent,
                                       owner,
                                       body,
                                       site,
                                       proof,
                                       semantics,
                                       cancellationToken,
                                       depth + 1)) {
                    return false;
                }
            }
            else if (arguments.Count > 1) {
                return false;
            }

            return creation.Initializer == null || CoveredElements(creation.Initializer,
                                                                   needParent,
                                                                   owner,
                                                                   body,
                                                                   site,
                                                                   proof,
                                                                   semantics,
                                                                   cancellationToken,
                                                                   depth);
        }

        private bool CoveredElements(InitializerExpressionSyntax initializer, bool needParent, IMethodSymbol owner,
                                     SyntaxNode body, InvocationExpressionSyntax site, CoverageProof proof,
                                     SemanticModel semantics, CancellationToken cancellationToken, int depth)
        {
            foreach (ExpressionSyntax element in initializer.Expressions) {
                if (!CoveredValue(element,
                                  needParent,
                                  owner,
                                  body,
                                  site,
                                  proof,
                                  semantics,
                                  cancellationToken,
                                  depth + 1)) {
                    return false;
                }
            }

            return true;
        }

        // Every use of a local list in the body, besides reading it: an Add of a covered entity, an AddRange of a
        // covered collection, a removal, or this very call. Anything else can put an unknown entity into it
        private bool ContributionsCovered(ILocalSymbol local, bool needParent, IMethodSymbol owner, SyntaxNode body,
                                          InvocationExpressionSyntax site, CoverageProof proof, SemanticModel semantics,
                                          CancellationToken cancellationToken, int depth)
        {
            foreach (IdentifierNameSyntax use in body.DescendantNodes().OfType<IdentifierNameSyntax>()) {
                if (use.Identifier.ValueText != local.Name ||
                    !SymbolEqualityComparer.Default.Equals(semantics.GetSymbolInfo(use, cancellationToken).Symbol,
                                                           local)) {
                    continue;
                }

                if (use.Parent is ArgumentSyntax { Parent : ArgumentListSyntax { Parent : {} argumentOwner } }) {
                    if (argumentOwner == site || argumentOwner is BaseObjectCreationExpressionSyntax) {
                        continue;
                    }

                    return false;
                }

                if (use.Parent is ForEachStatementSyntax loop && loop.Expression == use) {
                    continue;
                }

                if (use.Parent is ElementAccessExpressionSyntax element && element.Expression == use) {
                    if (IsWritten(element)) {
                        return false;
                    }

                    continue;
                }

                if (use.Parent is not MemberAccessExpressionSyntax access || access.Expression != use) {
                    return false;
                }

                if (access.Parent is not InvocationExpressionSyntax call || call.Expression != access) {
                    if (IsWritten(access)) {
                        return false;
                    }

                    continue;
                }

                string name = access.Name.Identifier.ValueText;
                SeparatedSyntaxList<ArgumentSyntax> arguments = call.ArgumentList.Arguments;

                switch (name) {
                case "Add" when arguments.Count == 1:
                    if (!CoveredValue(arguments[0].Expression,
                                      needParent,
                                      owner,
                                      body,
                                      site,
                                      proof,
                                      semantics,
                                      cancellationToken,
                                      depth + 1)) {
                        return false;
                    }

                    break;

                case "Insert" when arguments.Count == 2:
                    if (!CoveredValue(arguments[1].Expression,
                                      needParent,
                                      owner,
                                      body,
                                      site,
                                      proof,
                                      semantics,
                                      cancellationToken,
                                      depth + 1)) {
                        return false;
                    }

                    break;

                case "AddRange" when arguments.Count == 1:
                    if (!CoveredCollection(arguments[0].Expression,
                                           needParent,
                                           owner,
                                           body,
                                           site,
                                           proof,
                                           semantics,
                                           cancellationToken,
                                           depth + 1)) {
                        return false;
                    }

                    break;

                case "Contains":
                case "IndexOf":
                case "Remove":
                case "RemoveAt":
                case "Clear":
                case "ToArray":
                    break;

                default:
                    return false;
                }
            }

            return true;
        }

        // An instance accessor that provides cover returns what lives under its receiver, so the result's holder is covered
        // too -- except across the Critter-Player link, where each half provides the other and neither is beneath it
        private bool ProvidedByCall(InvocationExpressionSyntax call, bool needParent, IMethodSymbol owner,
                                    SyntaxNode body, InvocationExpressionSyntax site, CoverageProof proof,
                                    SemanticModel semantics, CancellationToken cancellationToken, int depth)
        {
            if (semantics.GetSymbolInfo(call, cancellationToken).Symbol is not IMethodSymbol method) {
                return false;
            }

            foreach (IParameterSymbol passed in method.Parameters) {
                if (!Model.Declarations.HasPassesCover(passed)) {
                    continue;
                }

                ExpressionSyntax? argument = ArgumentFor(call, method, passed);

                return argument != null && CoveredValue(argument,
                                                        needParent,
                                                        owner,
                                                        body,
                                                        site,
                                                        proof,
                                                        semantics,
                                                        cancellationToken,
                                                        depth + 1);
            }

            int? reach = Model.Declarations.DeclaredReach(method.GetReturnTypeAttributes());

            if (reach == null) {
                return false;
            }

            bool downward =
                !method.IsStatic && Model.Declarations.IsEntity(method.ContainingType) && !IsPartnerAccessor(method);

            if (needParent && !downward && (reach.Value & (ReachParent | ReachAncestors)) == 0) {
                return false;
            }

            proof.Reasons.Add("'" + method.Name + "' provides its result");

            return true;
        }

        private bool CoveredParameter(IParameterSymbol parameter, bool needParent, IMethodSymbol owner,
                                      CoverageProof proof)
        {
            if (!SymbolEqualityComparer.Default.Equals(parameter.ContainingSymbol, owner)) {
                return false;
            }

            int? reach = Model.Declarations.RequiredReach(parameter.GetAttributes());

            if (reach == null || (needParent && (reach.Value & (ReachParent | ReachAncestors)) == 0)) {
                return false;
            }

            proof.Roots.Add(parameter);
            proof.Reasons.Add("'" + parameter.Name + "' is declared [RequiresCover]");

            return true;
        }

        // Each half of the Critter-Player link provides cover for the other, which is how the pair is told apart from
        // a downward accessor without naming either type
        private bool IsPartnerAccessor(IMethodSymbol accessor)
        {
            ITypeSymbol? result = ElementType(accessor.ReturnType) ?? accessor.ReturnType;
            INamedTypeSymbol receiver = accessor.ContainingType;

            for (ITypeSymbol? current = result; current != null; current = current.BaseType) {
                foreach (IMethodSymbol back in current.GetMembers().OfType<IMethodSymbol>()) {
                    if (back.IsStatic || !Model.HasProvidesCoverOnReturn(back)) {
                        continue;
                    }

                    ITypeSymbol backResult = ElementType(back.ReturnType) ?? back.ReturnType;

                    if (Related(backResult, receiver)) {
                        return true;
                    }
                }
            }

            return false;
        }

        private static bool Related(ITypeSymbol first, ITypeSymbol second)
        {
            return DerivesFrom(first, second) || DerivesFrom(second, first);
        }

        private static bool DerivesFrom(ITypeSymbol type, ITypeSymbol ancestor)
        {
            for (ITypeSymbol? current = type.WithNullableAnnotation(NullableAnnotation.NotAnnotated); current != null;
                 current = current.BaseType) {
                if (SymbolEqualityComparer.Default.Equals(current.OriginalDefinition, ancestor.OriginalDefinition)) {
                    return true;
                }
            }

            return false;
        }

        public void Report(CompilationAnalysisContext context)
        {
            if (Candidates.IsEmpty) {
                return;
            }

            var graph = new CallGraph(Calls.ToList(), Delegates.ToList(), Dispatch.ToList());
            Reach maySnapshot = graph.Reaching(method => Model.EffectOf(method) == CoverEffect.Snapshot);
            Reach mayChangeCover = graph.Reaching(
                method => Model.EffectOf(method) is CoverEffect.Replace or CoverEffect.Restore or CoverEffect.Release);
            INamedTypeSymbol? callableByName =
                context.Compilation.GetTypeByMetadataName(CallableByNameAttributeFullName);
            var shipped =
                new SnapshotContinuation(graph, maySnapshot, Model, callableByName, context.Compilation, true);
            var everything =
                new SnapshotContinuation(graph, maySnapshot, Model, callableByName, context.Compilation, false);

            foreach (CoveredWideningCandidate candidate in Candidates) {
                SnapshotContinuation continuation = candidate.InTests ? everything : shipped;
                if (continuation.RestMaySnapshot(candidate.Owner.OriginalDefinition, candidate.Node) ||
                    continuation.ReturnMaySnapshot(candidate.Owner.OriginalDefinition)) {
                    continue;
                }

                SemanticModel semantics = context.Compilation.GetSemanticModel(candidate.Node.SyntaxTree);
                List<SyntaxNode> losses = CoverLosses(candidate,
                                                      graph,
                                                      mayChangeCover,
                                                      semantics,
                                                      context.Compilation,
                                                      context.CancellationToken);

                if (candidate.Roots.Any(
                        root => CoverLostBefore(candidate, root, losses, semantics, context.CancellationToken))) {
                    continue;
                }

                string reasons = candidate.Reasons.Count != 0 ? string.Join("; ", candidate.Reasons.Distinct())
                                                              : "every argument is always covered";

                context.ReportDiagnostic(Diagnostic.Create(CoveredWideningRule,
                                                           candidate.Node.GetLocation(),
                                                           "Sync." + candidate.Callee.Name,
                                                           reasons));
            }
        }

        // Points in the body where held cover can go away: an await of a call that does not hand the cover back, and
        // a call not awaited whose callee can replace, restore or release cover
        private List<SyntaxNode> CoverLosses(CoveredWideningCandidate candidate, CallGraph graph, Reach mayChangeCover,
                                             SemanticModel semantics, Compilation compilation,
                                             CancellationToken cancellationToken)
        {
            var losses = new List<SyntaxNode>();

            foreach (AwaitExpressionSyntax awaited in candidate.Body.DescendantNodes()
                         .OfType<AwaitExpressionSyntax>()) {
                if (RunsElsewhere(awaited, candidate.Body, graph)) {
                    continue;
                }

                // A restore after a real suspension brings back only what the snapshot named, never the dispatcher's cover,
                // so only a callee that never suspends and changes nothing it does not put back keeps the cover whole
                if (semantics.GetSymbolInfo(awaited.Expression, cancellationToken).Symbol is IMethodSymbol callee &&
                    IsTaskLike(callee.ReturnType) &&
                    NeverSuspends(callee, compilation, new HashSet<IMethodSymbol>(SymbolEqualityComparer.Default)) &&
                    (!mayChangeCover.Methods.Contains(callee.OriginalDefinition) || Model.PreservesCover(callee))) {
                    continue;
                }

                losses.Add(awaited);
            }

            foreach (CallRecord call in graph.CallsIn(candidate.Owner.OriginalDefinition)) {
                if (!call.Awaited && call.Node.SyntaxTree == candidate.Node.SyntaxTree &&
                    mayChangeCover.Reaches(call)) {
                    losses.Add(call.Node);
                }
            }

            return losses;
        }

        // A method whose task is complete when it returns: every await in it awaits another such method, and a body that
        // is not async hands back only such a task or a finished one. Anything else may suspend for real
        private bool NeverSuspends(IMethodSymbol method, Compilation compilation, HashSet<IMethodSymbol> visiting)
        {
            IMethodSymbol definition = method.OriginalDefinition;

            if (Suspension.TryGetValue(definition, out bool known)) {
                return known;
            }

            if (!visiting.Add(definition)) {
                return false;
            }

            bool result = ComputeNeverSuspends(definition, compilation, visiting);
            visiting.Remove(definition);

            if (result || visiting.Count == 0) {
                Suspension[definition] = result;
            }

            return result;
        }

        private bool ComputeNeverSuspends(IMethodSymbol method, Compilation compilation,
                                          HashSet<IMethodSymbol> visiting)
        {
            if (!IsTaskLike(method.ReturnType)) {
                return true;
            }

            foreach (SyntaxReference reference in method.DeclaringSyntaxReferences) {
                SyntaxNode declaration = reference.GetSyntax();

                if (declaration is not BaseMethodDeclarationSyntax {} declared ||
                    !compilation.ContainsSyntaxTree(declaration.SyntaxTree)) {
                    return false;
                }

                SyntaxNode? body = (SyntaxNode?)declared.Body ?? declared.ExpressionBody;

                if (body == null) {
                    return false;
                }

                SemanticModel semantics = compilation.GetSemanticModel(declaration.SyntaxTree);

                foreach (SyntaxNode node in body.DescendantNodes(descend =>
                                                                     descend is not AnonymousFunctionExpressionSyntax &&
                                                                     descend is not LocalFunctionStatementSyntax)) {
                    if (node is ForEachStatementSyntax { AwaitKeyword.RawKind : not 0 } ||
                        node is UsingStatementSyntax { AwaitKeyword.RawKind : not 0 } ||
                        node is LocalDeclarationStatementSyntax { AwaitKeyword.RawKind : not 0 }) {
                        return false;
                    }
                    if (node is AwaitExpressionSyntax awaited &&
                        !(semantics.GetSymbolInfo(awaited.Expression).Symbol is IMethodSymbol callee &&
                          IsTaskLike(callee.ReturnType) && NeverSuspends(callee, compilation, visiting))) {
                        return false;
                    }
                }

                if (declared.Modifiers.Any(SyntaxKind.AsyncKeyword)) {
                    continue;
                }

                IEnumerable < ExpressionSyntax
                    ? > returned =
                          declared.ExpressionBody != null
                              ? new[] { declared.ExpressionBody.Expression }
                              : body.DescendantNodes(descend => descend is not AnonymousFunctionExpressionSyntax &&
                                                                descend is not LocalFunctionStatementSyntax)
                                    .OfType<ReturnStatementSyntax>()
                                    .Select(statement => statement.Expression);

                foreach (ExpressionSyntax? value in returned) {
                    if (value == null || !FinishedTask(value, semantics, compilation, visiting)) {
                        return false;
                    }
                }
            }

            return method.DeclaringSyntaxReferences.Length != 0;
        }

        // `Task.CompletedTask`, `Task.FromResult(...)`, or the task of a method that never suspends
        private bool FinishedTask(ExpressionSyntax value, SemanticModel semantics, Compilation compilation,
                                  HashSet<IMethodSymbol> visiting)
        {
            ISymbol? symbol = semantics.GetSymbolInfo(value).Symbol;

            if (symbol is IPropertySymbol { Name : "CompletedTask" } completed &&
                completed.ContainingType?.ToDisplayString() == "System.Threading.Tasks.Task") {
                return true;
            }

            if (symbol is not IMethodSymbol called) {
                return false;
            }

            if (called.Name == "FromResult" &&
                called.ContainingType?.ToDisplayString() == "System.Threading.Tasks.Task") {
                return true;
            }

            return NeverSuspends(called, compilation, visiting);
        }

        private static bool IsTaskLike(ITypeSymbol type)
        {
            return type is INamedTypeSymbol named &&
                   named.OriginalDefinition.ToDisplayString() is
                   "System.Threading.Tasks.Task" or "System.Threading.Tasks.Task<TResult>" or
                   "System.Threading.Tasks.ValueTask" or "System.Threading.Tasks.ValueTask<TResult>";
        }

        // A root is fresh again when declared, rebound or re-acquired after the last loss that may run before the call;
        // a loss anywhere in a loop around the call reaches every root the loop does not declare anew
        private bool CoverLostBefore(CoveredWideningCandidate candidate, ISymbol root, List<SyntaxNode> losses,
                                     SemanticModel semantics, CancellationToken cancellationToken)
        {
            SyntaxNode site = candidate.Node;

            foreach (SyntaxNode loop in EnclosingLoops(site)) {
                if (!losses.Any(loss => loop.Span.Contains(loss.Span))) {
                    continue;
                }

                if (!DeclaredInside(root, LoopBody(loop), cancellationToken)) {
                    return true;
                }
            }

            SyntaxNode? lastLoss = null;

            foreach (SyntaxNode loss in losses) {
                if (loss.Span.End > site.SpanStart || loss.Span.Contains(site.Span) ||
                    !MayReach(loss, site, candidate.Body, semantics)) {
                    continue;
                }

                if (lastLoss == null || loss.SpanStart > lastLoss.SpanStart) {
                    lastLoss = loss;
                }
            }

            if (lastLoss == null) {
                return false;
            }

            if (DeclaredAtOrAfter(root, lastLoss.SpanStart, cancellationToken) ||
                ReboundAfter(candidate.Body, lastLoss.SpanStart, site, root, semantics, cancellationToken)) {
                return false;
            }

            // The loss may itself be the re-acquisition: `if (!await Sync.Lock(cr)) return;` replaces the held set with cr
            return !ReacquiredBefore(candidate, root, lastLoss.SpanStart, semantics, cancellationToken);
        }

        // An acquisition naming the root that runs on every path between the loss and the call: a statement of a block
        // around the call, or the condition of a guard whose failure branch leaves
        private bool ReacquiredBefore(CoveredWideningCandidate candidate, ISymbol root, int windowStart,
                                      SemanticModel semantics, CancellationToken cancellationToken)
        {
            SyntaxNode site = candidate.Node;

            foreach (InvocationExpressionSyntax call in candidate.Body.DescendantNodes()
                         .OfType<InvocationExpressionSyntax>()) {
                if (call.SpanStart < windowStart || call.Span.End > site.SpanStart) {
                    continue;
                }

                if (semantics.GetSymbolInfo(call, cancellationToken).Symbol is not IMethodSymbol callee ||
                    !Model.Acquires(callee) ||
                    !call.ArgumentList.Arguments.Any(
                        argument => Model.NamesValue(argument.Expression, root, candidate.Body, semantics))) {
                    continue;
                }

                if (Dominates(call, site, semantics)) {
                    return true;
                }
            }

            return false;
        }

        private static bool Dominates(SyntaxNode call, SyntaxNode site, SemanticModel semantics)
        {
            StatementSyntax? statement = StatementInBlock(call);

            if (statement == null || !site.Ancestors().Contains(statement.Parent) ||
                statement.Span.Contains(site.Span)) {
                return false;
            }

            if (statement is ExpressionStatementSyntax or LocalDeclarationStatementSyntax) {
                return true;
            }

            if (statement is not IfStatementSyntax guard || guard.Else != null ||
                !guard.Condition.Span.Contains(call.Span)) {
                return false;
            }

            ControlFlowAnalysis? flow = semantics.AnalyzeControlFlow(guard.Statement);

            return flow != null && flow.Succeeded && !flow.EndPointIsReachable;
        }

        // Source order over-approximates execution order here on purpose; the only paths ruled out are those leaving
        // through a block that returns or throws, and a sibling branch still counts, which silences rather than reports
        public static bool MayReach(SyntaxNode from, SyntaxNode to, SyntaxNode? body, SemanticModel semantics)
        {
            for (SyntaxNode? node = from.Parent; node != null && node != body; node = node.Parent) {
                if (node.Span.Contains(to.Span)) {
                    return true;
                }

                SyntaxList<StatementSyntax> statements = node switch {
                    BlockSyntax block => block.Statements,
                    SwitchSectionSyntax section => section.Statements,
                    _ => default(SyntaxList<StatementSyntax>),
                };

                if (statements.Count == 0) {
                    continue;
                }

                ControlFlowAnalysis? flow = semantics.AnalyzeControlFlow(statements.First(), statements.Last());

                if (flow != null && flow.Succeeded && !flow.EndPointIsReachable &&
                    flow.ExitPoints.All(exit => exit is ReturnStatementSyntax or YieldStatementSyntax)) {
                    return false;
                }
            }

            return true;
        }

        public static IEnumerable<SyntaxNode> EnclosingLoops(SyntaxNode node)
        {
            foreach (SyntaxNode ancestor in node.Ancestors()) {
                if (ancestor is BaseMethodDeclarationSyntax or LocalFunctionStatementSyntax or
                        AnonymousFunctionExpressionSyntax) {
                    yield break;
                }

                if (ancestor is ForStatementSyntax or CommonForEachStatementSyntax or WhileStatementSyntax or
                        DoStatementSyntax) {
                    yield return ancestor;
                }
            }
        }

        private static SyntaxNode LoopBody(SyntaxNode loop)
        {
            return loop switch {
                ForStatementSyntax f => f.Statement,
                CommonForEachStatementSyntax f => f.Statement,
                WhileStatementSyntax w => w.Statement,
                DoStatementSyntax d => d.Statement,
                _ => loop,
            };
        }

        private static bool DeclaredInside(ISymbol symbol, SyntaxNode region, CancellationToken cancellationToken)
        {
            if (symbol is not ILocalSymbol) {
                return false;
            }

            foreach (SyntaxReference reference in symbol.DeclaringSyntaxReferences) {
                SyntaxNode declaration = reference.GetSyntax(cancellationToken);

                // The loop variable of the loop itself is bound to an element read before the loop began
                if (declaration is not CommonForEachStatementSyntax && region.Span.Contains(declaration.Span)) {
                    return true;
                }
            }

            return false;
        }

        private static bool AssignedOnlyAtDeclaration(ILocalSymbol local, SyntaxNode body, SemanticModel semantics,
                                                      CancellationToken cancellationToken)
        {
            foreach (IdentifierNameSyntax use in body.DescendantNodes().OfType<IdentifierNameSyntax>()) {
                if (use.Identifier.ValueText != local.Name || !IsWritten(use) ||
                    !SymbolEqualityComparer.Default.Equals(semantics.GetSymbolInfo(use, cancellationToken).Symbol,
                                                           local)) {
                    continue;
                }

                return false;
            }

            return true;
        }

        private static bool IsWritten(ExpressionSyntax expression)
        {
            SyntaxNode? parent = expression.Parent;

            if (parent is AssignmentExpressionSyntax assignment && assignment.Left == expression) {
                return true;
            }

            if (parent is ArgumentSyntax argument && !argument.RefKindKeyword.IsKind(SyntaxKind.None)) {
                return true;
            }

            return parent is PrefixUnaryExpressionSyntax or PostfixUnaryExpressionSyntax;
        }

        // An await that does not suspend this body: one inside a local function, or inside a lambda stored to run later
        private static bool RunsElsewhere(SyntaxNode node, SyntaxNode body, CallGraph graph)
        {
            for (SyntaxNode? current = node.Parent; current != null && current != body; current = current.Parent) {
                if (current is LocalFunctionStatementSyntax ||
                    (current is AnonymousFunctionExpressionSyntax && !graph.IsInline(current))) {
                    return true;
                }
            }

            return false;
        }

        // The method a node executes in, and the lambdas between them, innermost first
        private static (IMethodSymbol? Owner, ImmutableArray<SyntaxNode> Lambdas)
            Owner(SyntaxNode node, SemanticModel semantics, CancellationToken cancellationToken)
        {
            ImmutableArray<SyntaxNode>.Builder? lambdas = null;

            for (SyntaxNode? current = node; current != null; current = current.Parent) {
                switch (current) {
                case AnonymousFunctionExpressionSyntax:
                    lambdas ??= ImmutableArray.CreateBuilder<SyntaxNode>();
                    lambdas.Add(current);
                    break;

                case LocalFunctionStatementSyntax or BaseMethodDeclarationSyntax or AccessorDeclarationSyntax:
                    return (semantics.GetDeclaredSymbol(current, cancellationToken) as IMethodSymbol,
                            lambdas?.ToImmutable() ?? ImmutableArray<SyntaxNode>.Empty);

                case ArrowExpressionClauseSyntax arrow when arrow.Parent is PropertyDeclarationSyntax property:
                    return ((semantics.GetDeclaredSymbol(property, cancellationToken) as IPropertySymbol)?.GetMethod,
                            lambdas?.ToImmutable() ?? ImmutableArray<SyntaxNode>.Empty);

                case TypeDeclarationSyntax:
                    return (null, ImmutableArray<SyntaxNode>.Empty);
                }
            }

            return (null, ImmutableArray<SyntaxNode>.Empty);
        }

        private static SyntaxNode? BodyOf(IMethodSymbol method, CancellationToken cancellationToken)
        {
            foreach (SyntaxReference reference in method.DeclaringSyntaxReferences) {
                SyntaxNode declaration = reference.GetSyntax(cancellationToken);
                SyntaxNode? body = declaration switch {
                    BaseMethodDeclarationSyntax m => (SyntaxNode?)m.Body ?? m.ExpressionBody,
                    _ => null,
                };

                if (body != null) {
                    return body;
                }
            }

            return null;
        }

        private static bool IsAwaited(SyntaxNode node)
        {
            SyntaxNode? current = node.Parent;

            while (current is ParenthesizedExpressionSyntax) {
                current = current.Parent;
            }

            return current is AwaitExpressionSyntax;
        }

        private static ExpressionSyntax Unwrap(ExpressionSyntax expression)
        {
            ExpressionSyntax current = expression;

            while (true) {
                switch (current) {
                case ParenthesizedExpressionSyntax parenthesized:
                    current = parenthesized.Expression;
                    break;

                case CastExpressionSyntax cast:
                    current = cast.Expression;
                    break;

                case PostfixUnaryExpressionSyntax suppress when suppress.IsKind(
                    SyntaxKind.SuppressNullableWarningExpression):
                    current = suppress.Operand;
                    break;

                case BinaryExpressionSyntax conversion when conversion.IsKind(SyntaxKind.AsExpression):
                    current = conversion.Left;
                    break;

                default:
                    return current;
                }
            }
        }

        // The element type of a collection, looking through tasks and nullability
        private static ITypeSymbol? ElementType(ITypeSymbol? type)
        {
            if (type is IArrayTypeSymbol array) {
                return array.ElementType;
            }

            if (type is not INamedTypeSymbol { IsGenericType : true } named || named.TypeArguments.Length != 1) {
                return null;
            }

            ITypeSymbol argument = named.TypeArguments[0];

            if (named.Name == "Task" || named.Name == "ValueTask") {
                return ElementType(argument) ?? argument;
            }

            return argument;
        }
    }

    // The call edges of the compilation, indexed by where each call runs: a method, or a lambda stored to run later.
    // A lambda handed straight to a method outside the compilation runs inside that call, so it is seen through
    private sealed class CallGraph
    {
        private readonly HashSet<SyntaxNode> InlineLambdas = new HashSet<SyntaxNode>();
        private readonly Dictionary<IMethodSymbol, List<CallRecord>> ByOwner =
            new Dictionary<IMethodSymbol, List<CallRecord>>(SymbolEqualityComparer.Default);
        private readonly Dictionary<SyntaxNode, List<CallRecord>> ByHostLambda =
            new Dictionary<SyntaxNode, List<CallRecord>>();
        private readonly Dictionary<IMethodSymbol, List<CallRecord>> ByTarget =
            new Dictionary<IMethodSymbol, List<CallRecord>>(SymbolEqualityComparer.Default);
        private readonly Dictionary<ITypeSymbol, List<CallRecord>> InvokesExact =
            new Dictionary<ITypeSymbol, List<CallRecord>>(SymbolEqualityComparer.Default);
        private readonly Dictionary<ITypeSymbol, List<CallRecord>> InvokesByDefinition =
            new Dictionary<ITypeSymbol, List<CallRecord>>(SymbolEqualityComparer.Default);
        private readonly Dictionary<ITypeSymbol, List<CallRecord>> OpenInvokes =
            new Dictionary<ITypeSymbol, List<CallRecord>>(SymbolEqualityComparer.Default);
        private readonly Dictionary<IMethodSymbol, List<DelegateRecord>> ByMethod =
            new Dictionary<IMethodSymbol, List<DelegateRecord>>(SymbolEqualityComparer.Default);
        private readonly Dictionary<SyntaxNode, List<DelegateRecord>> ByLambda =
            new Dictionary<SyntaxNode, List<DelegateRecord>>();
        private readonly Dictionary<IMethodSymbol, List<IMethodSymbol>> BasesByDerived =
            new Dictionary<IMethodSymbol, List<IMethodSymbol>>(SymbolEqualityComparer.Default);
        private readonly List<CallRecord> Calls;
        private readonly List<DelegateRecord> Delegates;

        public CallGraph(List<CallRecord> calls, List<DelegateRecord> delegates,
                         List<(IMethodSymbol Derived, IMethodSymbol Base)> dispatch)
        {
            Calls = calls;
            Delegates = delegates;

            foreach ((IMethodSymbol derived, IMethodSymbol overridden) in dispatch) {
                Add(BasesByDerived, derived, overridden);
            }

            foreach (DelegateRecord created in delegates) {
                if (created.Lambda != null && created.Inline) {
                    InlineLambdas.Add(created.Lambda);
                }
                if (created.Method != null) {
                    Add(ByMethod, created.Method, created);
                }
                if (created.Lambda != null) {
                    Add(ByLambda, created.Lambda, created);
                }
            }

            foreach (CallRecord call in calls) {
                SyntaxNode? host = HostLambda(call.Lambdas);

                if (host != null) {
                    Add(ByHostLambda, host, call);
                }
                else {
                    Add(ByOwner, call.Owner.OriginalDefinition, call);
                }

                Add(ByTarget, call.Target, call);

                if (call.DelegateType != null) {
                    Add(InvokesByDefinition, call.DelegateType.OriginalDefinition, call);
                    Add(CoveredWideningCollector.IsOpenType(call.DelegateType) ? OpenInvokes : InvokesExact,
                        CoveredWideningCollector.IsOpenType(call.DelegateType) ? call.DelegateType.OriginalDefinition
                                                                               : call.DelegateType,
                        call);
                }
                if (call.Kind == CallKind.Reflective) {
                    Reflective.Add(call);
                }
            }
        }

        public List<CallRecord> Reflective { get; } = new List<CallRecord>();

        public bool IsInline(SyntaxNode lambda)
        {
            return InlineLambdas.Contains(lambda);
        }

        // The stored lambda the code runs in once inline lambdas are seen through, or null when it runs in its method
        public SyntaxNode? HostLambda(ImmutableArray<SyntaxNode> lambdas)
        {
            foreach (SyntaxNode lambda in lambdas) {
                if (!InlineLambdas.Contains(lambda)) {
                    return lambda;
                }
            }

            return null;
        }

        public List<CallRecord> CallsIn(object host)
        {
            List<CallRecord>? calls =
                host is IMethodSymbol method ? Find(ByOwner, method) : Find(ByHostLambda, (SyntaxNode)host);

            return calls ?? new List<CallRecord>();
        }

        public List<CallRecord> CallersOf(IMethodSymbol method)
        {
            return Find(ByTarget, method) ?? new List<CallRecord>();
        }

        // The invocations a delegate of this type may be invoked through
        public List<CallRecord> InvokesOf(ITypeSymbol created)
        {
            if (CoveredWideningCollector.IsOpenType(created)) {
                return Find(InvokesByDefinition, created.OriginalDefinition) ?? new List<CallRecord>();
            }

            var invokes = new List<CallRecord>(Find(InvokesExact, created) ?? new List<CallRecord>());
            invokes.AddRange(Find(OpenInvokes, created.OriginalDefinition) ?? new List<CallRecord>());

            return invokes;
        }

        // The members a call may reach this method through: those it overrides or implements, transitively
        public List<IMethodSymbol> BasesOf(IMethodSymbol method)
        {
            var bases = new List<IMethodSymbol>();
            var pending = new Queue<IMethodSymbol>();
            pending.Enqueue(method);

            while (pending.Count != 0) {
                foreach (IMethodSymbol overridden in Find(BasesByDerived, pending.Dequeue()) ??
                         new List<IMethodSymbol>()) {
                    if (!bases.Contains(overridden, SymbolEqualityComparer.Default)) {
                        bases.Add(overridden);
                        pending.Enqueue(overridden);
                    }
                }
            }

            return bases;
        }

        public List<DelegateRecord> DelegatesOf(IMethodSymbol method)
        {
            return Find(ByMethod, method) ?? new List<DelegateRecord>();
        }

        public List<DelegateRecord> DelegatesOf(SyntaxNode lambda)
        {
            return Find(ByLambda, lambda) ?? new List<DelegateRecord>();
        }

        // Every method whose execution may reach a source: through calls, through a delegate of a type some reaching
        // method or lambda was turned into, and through a member dispatched at runtime
        public Reach Reaching(Func<IMethodSymbol, bool> isSource)
        {
            var reach = new Reach();

            while (true) {
                var methods = new HashSet<IMethodSymbol>(SymbolEqualityComparer.Default);
                var lambdas = new HashSet<SyntaxNode>();
                var pending = new Queue<IMethodSymbol>();

                bool Source(IMethodSymbol method)
                {
                    return isSource(method);
                }

                void MarkHost(ImmutableArray<SyntaxNode> within, IMethodSymbol? owner)
                {
                    SyntaxNode? host = HostLambda(within);

                    if (host != null) {
                        lambdas.Add(host);
                    }
                    else if (owner != null && methods.Add(owner.OriginalDefinition)) {
                        pending.Enqueue(owner.OriginalDefinition);
                    }
                }

                foreach (CallRecord call in Calls) {
                    if (call.DelegateType != null) {
                        if (reach.Dangerous(call.DelegateType)) {
                            MarkHost(call.Lambdas, call.Owner);
                        }

                        continue;
                    }

                    if (Source(call.Target) && methods.Add(call.Target)) {
                        pending.Enqueue(call.Target);
                    }
                }

                while (pending.Count != 0) {
                    IMethodSymbol method = pending.Dequeue();

                    foreach (CallRecord caller in CallersOf(method)) {
                        MarkHost(caller.Lambdas, caller.Owner);
                    }

                    // A call to the member it overrides may run it
                    foreach (IMethodSymbol overridden in BasesOf(method)) {
                        if (methods.Add(overridden)) {
                            pending.Enqueue(overridden);
                        }
                    }

                    // A method group handed to a method outside the compilation runs inside that call
                    foreach (DelegateRecord created in DelegatesOf(method)) {
                        if (created.Inline) {
                            MarkHost(created.Lambdas, created.Owner);
                        }
                    }
                }

                bool grown = false;

                foreach (DelegateRecord created in Delegates) {
                    bool reaches = (created.Method != null && methods.Contains(created.Method)) ||
                                   (created.Lambda != null && lambdas.Contains(created.Lambda));

                    if (reaches && reach.AddDangerous(created.Key, ReachesClosedInvokes(created))) {
                        grown = true;
                    }
                }

                if (!grown) {
                    reach.Methods = methods;
                    return reach;
                }
            }
        }

        // A delegate of a still-open type becomes a closed one only where its generic method is instantiated. One no
        // code in the compilation calls is instantiated through reflection, and hands what it builds to the engine
        private bool ReachesClosedInvokes(DelegateRecord created)
        {
            if (!CoveredWideningCollector.IsOpenType(created.Key) || created.Owner == null) {
                return true;
            }

            IMethodSymbol owner = created.Owner.OriginalDefinition;

            return CallersOf(owner).Count != 0 || DelegatesOf(owner).Count != 0;
        }

        private static List<TValue>? Find<TKey, TValue>(Dictionary<TKey, List<TValue>> index, TKey key)
            where TKey : notnull
        {
            return index.TryGetValue(key, out List<TValue>? found) ? found : null;
        }

        private static void Add<TKey, TValue>(Dictionary<TKey, List<TValue>> index, TKey key, TValue value)
            where TKey : notnull
        {
            if (!index.TryGetValue(key, out List<TValue>? list)) {
                list = new List<TValue>();
                index[key] = list;
            }

            list.Add(value);
        }
    }

    // What reaches a source: methods by symbol, and delegate invocations by the type a reaching delegate was made as
    private sealed class Reach
    {
        private readonly HashSet<ITypeSymbol> Exact = new HashSet<ITypeSymbol>(SymbolEqualityComparer.Default);
        private readonly HashSet<ITypeSymbol> Definitions = new HashSet<ITypeSymbol>(SymbolEqualityComparer.Default);
        private readonly HashSet<ITypeSymbol> OpenlyCreated = new HashSet<ITypeSymbol>(SymbolEqualityComparer.Default);

        public HashSet<IMethodSymbol> Methods { get; set; } =
            new HashSet<IMethodSymbol>(SymbolEqualityComparer.Default);

        public bool Reaches(CallRecord call)
        {
            return call.DelegateType != null ? Dangerous(call.DelegateType) : Methods.Contains(call.Target);
        }

        public bool Dangerous(ITypeSymbol delegateType)
        {
            ITypeSymbol definition = delegateType.OriginalDefinition;

            return CoveredWideningCollector.IsOpenType(delegateType)
                     ? Definitions.Contains(definition)
                     : Exact.Contains(delegateType) || OpenlyCreated.Contains(definition);
        }

        public bool AddDangerous(ITypeSymbol created, bool reachesClosedInvokes)
        {
            bool open = CoveredWideningCollector.IsOpenType(created);
            bool added = Definitions.Add(created.OriginalDefinition);

            if (open) {
                if (reachesClosedInvokes) {
                    added |= OpenlyCreated.Add(created.OriginalDefinition);
                }
            }
            else {
                added |= Exact.Add(created);
            }

            return added;
        }
    }

    // Whether Sync.Snapshot may run after a point in the same script entry. An entry point starts a context of its own;
    // a [CallableByName] method returns into every reflection invoke, a delegate into every invocation of its type
    private sealed class SnapshotContinuation
    {
        private readonly CallGraph Graph;
        private readonly Reach MaySnapshot;
        private readonly CoverModel Model;
        private readonly INamedTypeSymbol? CallableByName;
        private readonly Compilation CompilationContext;
        private readonly bool ShippedOnly;
        private readonly Dictionary<SyntaxTree, SemanticModel> Models = new Dictionary<SyntaxTree, SemanticModel>();
        private readonly Dictionary<object, bool> Memo = new Dictionary<object, bool>(HostComparer.Instance);
        private readonly HashSet<object> Visiting = new HashSet<object>(HostComparer.Instance);

        public SnapshotContinuation(CallGraph graph, Reach maySnapshot, CoverModel model,
                                    INamedTypeSymbol? callableByName, Compilation compilation, bool shippedOnly)
        {
            Graph = graph;
            MaySnapshot = maySnapshot;
            Model = model;
            CallableByName = callableByName;
            CompilationContext = compilation;
            ShippedOnly = shippedOnly;
        }

        private bool Counts(bool inTests)
        {
            return !ShippedOnly || !inTests;
        }

        private SemanticModel ModelOf(SyntaxTree tree)
        {
            if (!Models.TryGetValue(tree, out SemanticModel? model)) {
                model = CompilationContext.GetSemanticModel(tree);
                Models[tree] = model;
            }

            return model;
        }

        // A snapshot the rest of this host may take, after the point or on a later turn of a loop around it
        public bool RestMaySnapshot(object host, SyntaxNode site)
        {
            List<SyntaxNode> loops = CoveredWideningCollector.EnclosingLoops(site).ToList();

            foreach (CallRecord call in Graph.CallsIn(host)) {
                if (call.Node.SyntaxTree != site.SyntaxTree || !MaySnapshot.Reaches(call)) {
                    continue;
                }

                if (loops.Any(loop => loop.Span.Contains(call.Node.Span))) {
                    return true;
                }

                // A later call in a branch the point leaves by returning cannot follow it
                if (call.Node.SpanStart >= site.Span.End &&
                    CoveredWideningCollector.MayReach(site, call.Node, null, ModelOf(site.SyntaxTree))) {
                    return true;
                }
            }

            return false;
        }

        public bool ReturnMaySnapshot(object host)
        {
            if (Memo.TryGetValue(host, out bool known)) {
                return known;
            }

            // A cycle proves nothing, so the recursive edge answers the cautious way
            if (!Visiting.Add(host)) {
                return true;
            }

            bool result = host is IMethodSymbol method ? MethodReturn(method) : LambdaReturn((SyntaxNode)host);
            Visiting.Remove(host);
            Memo[host] = result;

            return result;
        }

        private bool MethodReturn(IMethodSymbol method)
        {
            var reached = new List<IMethodSymbol> { method };
            reached.AddRange(Graph.BasesOf(method));

            foreach (IMethodSymbol target in reached) {
                foreach (CallRecord call in Graph.CallersOf(target)) {
                    if (call.Kind == CallKind.Call && Counts(call.InTests) &&
                        Continues(call.Lambdas, call.Owner, call.Node)) {
                        return true;
                    }
                }
            }

            // An entry point subscribed to an engine event is dispatched into a context of its own
            if (!Model.IsEntryPoint(method)) {
                foreach (DelegateRecord created in Graph.DelegatesOf(method)) {
                    if (!Counts(created.InTests)) {
                        continue;
                    }
                    if (created.Inline
                            ? created.Owner == null || Continues(created.Lambdas, created.Owner, created.Node)
                            : InvokedWithSnapshotAfter(created.Key)) {
                        return true;
                    }
                }
            }

            if (CallableByName != null &&
                method.GetAttributes().Any(
                    attribute => SymbolEqualityComparer.Default.Equals(attribute.AttributeClass, CallableByName))) {
                foreach (CallRecord call in Graph.Reflective) {
                    if (Counts(call.InTests) && Continues(call.Lambdas, call.Owner, call.Node)) {
                        return true;
                    }
                }
            }

            return false;
        }

        private bool LambdaReturn(SyntaxNode lambda)
        {
            List<DelegateRecord> created = Graph.DelegatesOf(lambda);

            return created.Count == 0 || created.Any(delegateRecord => InvokedWithSnapshotAfter(delegateRecord.Key));
        }

        // A delegate nobody in the compilation invokes is invoked where the analysis cannot see
        private bool InvokedWithSnapshotAfter(ITypeSymbol key)
        {
            List<CallRecord> invokes = Graph.InvokesOf(key);

            return invokes.Count == 0 ||
                   invokes.Any(call => Counts(call.InTests) && Continues(call.Lambdas, call.Owner, call.Node));
        }

        private bool Continues(ImmutableArray<SyntaxNode> lambdas, IMethodSymbol owner, SyntaxNode node)
        {
            SyntaxNode? lambda = Graph.HostLambda(lambdas);
            object host = lambda != null ? lambda : owner.OriginalDefinition;

            return RestMaySnapshot(host, node) || ReturnMaySnapshot(host);
        }
    }

    // Hosts are methods, compared as symbols, and lambdas, compared as nodes
    private sealed class HostComparer : IEqualityComparer<object>
    {
        public static readonly HostComparer Instance = new HostComparer();

        bool IEqualityComparer<object>.Equals(object? first, object? second)
        {
            return first is ISymbol left && second is ISymbol right ? SymbolEqualityComparer.Default.Equals(left, right)
                                                                    : ReferenceEquals(first, second);
        }

        int IEqualityComparer<object>.GetHashCode(object host)
        {
            return host is ISymbol symbol ? SymbolEqualityComparer.Default.GetHashCode(symbol) : host.GetHashCode();
        }
    }
}
