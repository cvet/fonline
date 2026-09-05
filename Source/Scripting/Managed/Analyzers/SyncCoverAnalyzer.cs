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
        public const string PreservesCoverAttributeFullName = "FOnline.PreservesCoverAttribute";
        public const string EntityTypeFullName = "FOnline.Entity";

        // The cover primitives are engine-owned, so they are matched by their symbol's full metadata name.
        // Matching a bare type name would let any project class called `Sync` silently discharge an
        // obligation it knows nothing about.
        public const string SyncTypeFullName = "FOnline.Sync";
        public const string GameTypeFullName = "FOnline.Game";

        // Entities that carry their cover with them: baked map data and prototypes. They are immutable and
        // readable at any time, so an obligation for one is satisfied the moment it is stated, and acquiring
        // one succeeds trivially (Sync answers success without reaching the native primitive).
        //
        // They are still ENTITIES, and annotations on them are still legal. Excluding them from the notion of
        // "entity" instead would lose the contract on an upcast: ProtoCritter derives from Critter and
        // StaticItem from the shared item base, so a value flowing through the base type would silently stop
        // demanding cover for the mutable half. The exemption belongs to the value, not to the type system.
        //
        // Asked of the type rather than kept as a list of names: the baker overrides `IsAlwaysCovered` on
        // exactly the generated prototype and static classes, so a project's own entities (its Proto<Faction>,
        // its Proto<Modifier>) are covered without this engine-owned analyzer having to know their names.
        private const string AlwaysCoveredMemberName = "IsAlwaysCovered";

        // Asking whether cover is held, rather than taking it. `Sync.IsCovered` is the model's own probe and
        // `Game.IsEntityLocked` the engine's.
        private static readonly string[] CoverProbeNames = {"IsCovered", "IsEntityLocked"};

        // The raw entity-cover primitives, meaning the ones a Sync helper can replace.
        //
        // `Game.Lock` / `Game.Unlock` are absent because they lock the Game singleton's property bucket, which
        // is a different thing from entity cover; FOSYNC006/FOSYNC007 own that pair instead.
        //
        // `Game.TrySyncEntity` is absent for a different reason: it resolves an *id* to a live entity and
        // covers it, answering false when the entity is gone. Every Sync helper takes an entity, so none can
        // stand in for it -- a handle retained across a yield may already be dead, which is exactly when this
        // is the right call. Flagging it would report code for using the only tool that fits.
        private static readonly string[] RawSyncPrimitiveNames = {"Sync", "SyncRelease"};

        // The Game methods whose subject is an entity to cover. Game also carries the whole rest of the
        // script surface, so a rule about acquisitions must name these rather than take the type as a whole:
        // a static item passed to Game.Verify as failure context or to Game.CallStaticItemFunction as its
        // subject is an ordinary argument, not an acquisition.
        private static readonly string[] GameCoverPrimitiveNames =
            {"Sync", "SyncRelease", "Lock", "Unlock", "TrySyncEntity", "IsEntityLocked"};

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

        internal static readonly DiagnosticDescriptor CoverProbeRule = new DiagnosticDescriptor(
            id: "FOSYNC004",
            title: "Cover is probed instead of acquired",
            messageFormat: "'{0}' asks whether cover is held; acquire what this code needs with Sync.Lock / Sync.Widen instead",
            category: Category,
            defaultSeverity: DiagnosticSeverity.Warning,
            isEnabledByDefault: true,
            description:
                "A probe answers what was true a moment ago, and code that branches on it either does the work "
                + "unprotected on one path or silently skips it on the other. Production code should state what "
                + "it needs and take it. Tests are the honest exception -- asserting that a call did or did not "
                + "widen the caller cover is exactly how the sync contract is pinned -- so lower this rule for "
                + "test sources in the embedding project's .editorconfig rather than working around it.");

        internal static readonly DiagnosticDescriptor RawSyncPrimitiveRule = new DiagnosticDescriptor(
            id: "FOSYNC005",
            title: "Raw synchronization primitive used outside the Sync module",
            messageFormat: "'{0}' is a raw synchronization primitive; use the Sync helpers, which acquire atomically and re-prove after migration",
            category: Category,
            defaultSeverity: DiagnosticSeverity.Warning,
            isEnabledByDefault: true,
            description:
                "The Sync helpers are not thin wrappers: they acquire multi-root packages as one step and retry "
                + "with a re-proof that nothing migrated in between. Reaching for the primitive directly gets the "
                + "first half and silently drops the second. This does not cover the Game singleton bucket lock "
                + "(Game.Lock / Game.Unlock), which guards property storage rather than entity cover.");

        internal static readonly DiagnosticDescriptor SingletonLockLeakRule = new DiagnosticDescriptor(
            id: "FOSYNC006",
            title: "Singleton lock is not released on every path",
            messageFormat: "'{0}' leaves the singleton lock held on this path; release it with Game.Unlock() before the scope ends",
            category: Category,
            defaultSeverity: DiagnosticSeverity.Warning,
            isEnabledByDefault: true,
            description:
                "The singleton bucket is deliberately kept outside the entity-cover set, so it survives every "
                + "later Sync acquisition in the same job and nothing drops it implicitly. A job that ends still "
                + "holding one does not merely block others: SyncContext's destructor asserts the bucket is empty "
                + "(FO_STRONG_ASSERT), which is an always-on deterministic exit.");

        internal static readonly DiagnosticDescriptor SingletonLockAcrossAwaitRule = new DiagnosticDescriptor(
            id: "FOSYNC007",
            title: "Singleton lock is held across an await",
            messageFormat: "the singleton lock is held across this await; take it after the await, or release it before",
            category: Category,
            defaultSeverity: DiagnosticSeverity.Warning,
            isEnabledByDefault: true,
            description:
                "The lock is owned by the thread that took it: UnlockSingleton throws unless the releasing thread "
                + "is the holder. A continuation may resume on a different thread, so an await between Lock and "
                + "Unlock turns the release into a throw rather than a slow path -- and everything the await waits "
                + "on runs while the bucket is held.");

        internal static readonly DiagnosticDescriptor CoverLostToAwaitRule = new DiagnosticDescriptor(
            id: "FOSYNC009",
            title: "Cover for a value is not re-proved after an await",
            messageFormat: "'{0}' was covered before awaiting {1}, but nothing re-proved it after; the call here needs cover for it",
            category: Category,
            defaultSeverity: DiagnosticSeverity.Warning,
            isEnabledByDefault: true,
            description:
                "An await releases the caller cover: the continuation may resume on another thread, and the world "
                + "moves while it waits. A value covered before the await is not covered after it, so the entity may "
                + "have been destroyed or relocated in between. The re-proof is a Sync call naming that value, and its "
                + "result is an answer to act on -- the acquisition can legitimately fail because the entity is gone.");

        public override ImmutableArray<DiagnosticDescriptor> SupportedDiagnostics { get; } =
            ImmutableArray.Create(NonEntityTargetRule, UndischargedCoverRule, EntryPointCoverRule, CoverProbeRule, RawSyncPrimitiveRule,
                SingletonLockLeakRule, SingletonLockAcrossAwaitRule, CoverLostToAwaitRule);

        public override void Initialize(AnalysisContext context)
        {
            context.ConfigureGeneratedCodeAnalysis(GeneratedCodeAnalysisFlags.None);
            context.EnableConcurrentExecution();

            context.RegisterCompilationStartAction(compilationStart =>
            {
                Compilation compilation = compilationStart.Compilation;

                INamedTypeSymbol? requiresCover = compilation.GetTypeByMetadataName(RequiresCoverAttributeFullName);
                INamedTypeSymbol? providesCover = compilation.GetTypeByMetadataName(ProvidesCoverAttributeFullName);
                INamedTypeSymbol? preservesCover = compilation.GetTypeByMetadataName(PreservesCoverAttributeFullName);
                INamedTypeSymbol? entityType = compilation.GetTypeByMetadataName(EntityTypeFullName);

                if (requiresCover == null || entityType == null) {
                    return;
                }

                INamedTypeSymbol? syncType = compilation.GetTypeByMetadataName(SyncTypeFullName);
                INamedTypeSymbol? gameType = compilation.GetTypeByMetadataName(GameTypeFullName);

                var entryMarkers = new List<INamedTypeSymbol>();

                foreach (string name in EntryPointAttributeFullNames) {
                    INamedTypeSymbol? marker = compilation.GetTypeByMetadataName(name);

                    if (marker != null) {
                        entryMarkers.Add(marker);
                    }
                }

                var model = new CoverModel(requiresCover, providesCover, preservesCover, entityType, syncType, gameType, entryMarkers);

                compilationStart.RegisterSymbolAction(
                    symbolContext => AnalyzeDeclaration(symbolContext, model),
                    SymbolKind.Method);

                compilationStart.RegisterSyntaxNodeAction(
                    nodeContext => AnalyzeInvocation(nodeContext, model),
                    SyntaxKind.InvocationExpression);

                compilationStart.RegisterSyntaxNodeAction(
                    nodeContext => AnalyzeSyncSurfaceUse(nodeContext, model),
                    SyntaxKind.InvocationExpression);

                compilationStart.RegisterSyntaxNodeAction(
                    nodeContext => AnalyzeSingletonLock(nodeContext, model),
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
        // The two rules the retired sync-flow audit owned that need no dataflow at all: they are about which
        // surface a call reaches for, not about what it proves. Both are scoped by the *symbol's* containing
        // type, so a project class called Sync or Game cannot silently satisfy or trip them.
        private static void AnalyzeSyncSurfaceUse(SyntaxNodeAnalysisContext context, CoverModel model)
        {
            var invocation = (InvocationExpressionSyntax)context.Node;

            if (context.SemanticModel.GetSymbolInfo(invocation, context.CancellationToken).Symbol is not IMethodSymbol callee) {
                return;
            }

            // Inside Sync itself both are the implementation, not a smell.
            if (model.SyncType != null && context.ContainingSymbol is IMethodSymbol caller &&
                SymbolEqualityComparer.Default.Equals(caller.ContainingType, model.SyncType)) {
                return;
            }

            bool onSync = model.SyncType != null && SymbolEqualityComparer.Default.Equals(callee.ContainingType, model.SyncType);
            bool onGame = model.GameType != null && SymbolEqualityComparer.Default.Equals(callee.ContainingType, model.GameType);

            if (!onSync && !onGame) {
                return;
            }

            if (System.Array.IndexOf(CoverProbeNames, callee.Name) >= 0) {
                context.ReportDiagnostic(Diagnostic.Create(CoverProbeRule, invocation.GetLocation(), callee.Name));
                return;
            }

            if (onGame && System.Array.IndexOf(RawSyncPrimitiveNames, callee.Name) >= 0) {
                context.ReportDiagnostic(Diagnostic.Create(RawSyncPrimitiveRule, invocation.GetLocation(), "Game." + callee.Name));
            }
        }

        // The singleton bucket lock is a plain paired resource, so it is checked structurally rather than
        // through the cover model: from the Lock statement, walk the statements that follow it in the same
        // block until the matching Unlock.
        private static void AnalyzeSingletonLock(SyntaxNodeAnalysisContext context, CoverModel model)
        {
            var invocation = (InvocationExpressionSyntax)context.Node;

            if (model.GameType == null || invocation.ArgumentList.Arguments.Count != 0) {
                return;
            }

            if (context.SemanticModel.GetSymbolInfo(invocation, context.CancellationToken).Symbol is not IMethodSymbol callee ||
                callee.Name != "Lock" || !SymbolEqualityComparer.Default.Equals(callee.ContainingType, model.GameType)) {
                return;
            }

            var statement = invocation.FirstAncestorOrSelf<StatementSyntax>();

            if (statement?.Parent is not BlockSyntax block) {
                return;
            }

            var following = block.Statements.SkipWhile(s => s != statement).Skip(1).ToList();

            foreach (StatementSyntax next in following) {
                if (ContainsSingletonUnlock(next, context.SemanticModel, model, context.CancellationToken)) {
                    return;
                }

                // An await before the release: the continuation may resume on another thread, and only the
                // holder may release.
                foreach (AwaitExpressionSyntax await in next.DescendantNodesAndSelf().OfType<AwaitExpressionSyntax>()) {
                    context.ReportDiagnostic(Diagnostic.Create(SingletonLockAcrossAwaitRule, await.GetLocation()));
                    return;
                }

                // A path that leaves the scope without releasing.
                foreach (SyntaxNode exit in next.DescendantNodesAndSelf()) {
                    if (exit is ReturnStatementSyntax or ThrowStatementSyntax) {
                        context.ReportDiagnostic(Diagnostic.Create(SingletonLockLeakRule, exit.GetLocation(), "Game.Lock()"));
                        return;
                    }
                }
            }

            context.ReportDiagnostic(Diagnostic.Create(SingletonLockLeakRule, invocation.GetLocation(), "Game.Lock()"));
        }

        private static bool ContainsSingletonUnlock(SyntaxNode node, SemanticModel semantics, CoverModel model, CancellationToken cancellationToken)
        {
            foreach (InvocationExpressionSyntax candidate in node.DescendantNodesAndSelf().OfType<InvocationExpressionSyntax>()) {
                if (semantics.GetSymbolInfo(candidate, cancellationToken).Symbol is IMethodSymbol symbol &&
                    symbol.Name == "Unlock" && candidate.ArgumentList.Arguments.Count == 0 &&
                    model.GameType != null && SymbolEqualityComparer.Default.Equals(symbol.ContainingType, model.GameType)) {
                    return true;
                }
            }

            return false;
        }

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

            // FOSYNC009 asks a different question from the rest of this method: not "is there cover for this
            // value" but "did the cover survive to here". It therefore runs FIRST -- the propagation and
            // acquisition discharges below return early, and the propagated case (an entry point holding the
            // dispatcher's cover across an await) is exactly where the class this rule names lives.
            //
            // It reports only where the ordinary discharge is satisfied, so a value with no cover at all is
            // FOSYNC002's to report and is not said twice.
            ReportCoverLostToAwait(context, model, body, invocation, callee, demanding, demandsReceiver, caller, semantics, cancellationToken);

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

                // Baked map data and prototypes are covered by being what they are, so the obligation is met
                // the moment it is stated. Only the receiver is exempt here: the call's own arguments are
                // checked below either way.
                if (!model.IsAlwaysCovered(semantics.GetTypeInfo(receiver, cancellationToken).Type) &&
                    !model.ComesFromProvidedCover(receiver, semantics, cancellationToken) &&
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

                // A parameter typed as the mutable half accepts the always-covered half too, since both derive
                // from the same base. Such a value satisfies the obligation on its own.
                if (argument != null && model.IsAlwaysCovered(semantics.GetTypeInfo(argument, cancellationToken).Type)) {
                    continue;
                }

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

        // The gate in front of the per-value check: only values whose obligation is otherwise discharged.
        private static void ReportCoverLostToAwait(SyntaxNodeAnalysisContext context, CoverModel model, SyntaxNode body,
            InvocationExpressionSyntax invocation, IMethodSymbol callee, List<IParameterSymbol> demanding, bool demandsReceiver,
            IMethodSymbol? caller, SemanticModel semantics, CancellationToken cancellationToken)
        {
            // Inside Sync itself these calls are the mechanism, not a consumer of it -- the same exemption the
            // ordinary discharge makes below, and it has to be repeated here because this check runs first.
            if (caller != null && model.SyncType != null &&
                SymbolEqualityComparer.Default.Equals(caller.ContainingType, model.SyncType)) {
                return;
            }

            bool propagates = caller != null && caller.Parameters.Any(model.HasRequiresCover);
            bool acquires = AcquiresCover(body, semantics, model, cancellationToken);

            if (demandsReceiver && invocation.Expression is MemberAccessExpressionSyntax memberAccess) {
                ExpressionSyntax receiver = memberAccess.Expression;

                if (propagates || acquires ||
                    model.ComesFromProvidedCover(receiver, semantics, cancellationToken) ||
                    model.CoveredByEarlierCall(body, receiver, semantics, cancellationToken)) {
                    ReportIfCoverLostToAwait(context, model, body, invocation, receiver, caller, semantics, cancellationToken);
                }
            }

            foreach (IParameterSymbol parameter in demanding) {
                ExpressionSyntax? argument = ArgumentFor(invocation, callee, parameter);

                if (argument == null) {
                    continue;
                }

                if (propagates || acquires ||
                    model.ComesFromProvidedCover(argument, semantics, cancellationToken) ||
                    model.CoveredByEarlierCall(body, argument, semantics, cancellationToken)) {
                    ReportIfCoverLostToAwait(context, model, body, invocation, argument, caller, semantics, cancellationToken);
                }
            }
        }

        // FOSYNC009 -- the value-aware half of the discharge, for the one case where the value's cover provably
        // went away: an await between the point that established it and the point that needs it.
        //
        // This reuses the FOSYNC002 annotation rather than inventing a second notion of "needs cover": the
        // obligation is whatever [RequiresCover] already says, and the only question added here is whether the
        // cover survived to this point. Position in source order, not a control flow graph -- see the rule's
        // documentation for what that costs.
        private static void ReportIfCoverLostToAwait(SyntaxNodeAnalysisContext context, CoverModel model, SyntaxNode body,
            InvocationExpressionSyntax invocation, ExpressionSyntax value, IMethodSymbol? caller, SemanticModel semantics,
            CancellationToken cancellationToken)
        {
            if (model.IsAlwaysCovered(semantics.GetTypeInfo(value, cancellationToken).Type)) {
                return;
            }

            ISymbol? tracked = semantics.GetSymbolInfo(value, cancellationToken).Symbol;

            // Only a plain named value can be followed. Anything derived on the spot (a call, an element, a
            // member chain) is re-read here anyway, so an await before it says nothing about it.
            if (tracked is not IParameterSymbol && tracked is not ILocalSymbol) {
                return;
            }

            int callSite = invocation.SpanStart;
            AwaitExpressionSyntax? lastAwait = null;

            foreach (AwaitExpressionSyntax candidate in body.DescendantNodes().OfType<AwaitExpressionSyntax>()) {
                if (!PrecedesOnSomePath(candidate, invocation)) {
                    continue;
                }

                // A preserving callee hands the cover back, so awaiting it releases nothing.
                if (semantics.GetSymbolInfo(candidate.Expression, cancellationToken).Symbol is IMethodSymbol awaited &&
                    model.PreservesCover(awaited)) {
                    continue;
                }

                if (!ReachesFrom(candidate, invocation, semantics)) {
                    continue;
                }

                if (candidate.Span.End <= callSite && (lastAwait == null || candidate.SpanStart > lastAwait.SpanStart)) {
                    lastAwait = candidate;
                }
            }

            if (lastAwait == null) {
                return;
            }

            // A value the await itself produced, or one declared after it, is fresh: the await says nothing
            // about cover it never held. This is the bulk of the shape -- `Location? loc = await Find(...)`
            // followed by work on `loc`.
            if (DeclaredAtOrAfter(tracked, lastAwait.SpanStart, cancellationToken)) {
                return;
            }

            // The re-proof may be the await itself (`await Sync.Widen(... value ...)`), which starts at or after
            // the await's own start, so the window opens there rather than after it.
            if (ReProvesCover(body, lastAwait.SpanStart, callSite, tracked, model, semantics, cancellationToken)) {
                return;
            }

            string awaitedName = semantics.GetSymbolInfo(lastAwait.Expression, cancellationToken).Symbol is IMethodSymbol awaitedMethod
                ? awaitedMethod.Name
                : "the call";

            context.ReportDiagnostic(
                Diagnostic.Create(
                    CoverLostToAwaitRule,
                    value.GetLocation(),
                    value.ToString(),
                    awaitedName));
        }

        // Source order is not execution order. Two sides of an if/else both come "before" a later line in the
        // text, but only one of them runs, and an await in the branch that was not taken released nothing. So
        // the await counts only if it can be lifted to a statement that sits in the same block as the use and
        // starts earlier -- which keeps `if (!await Sync.X()) return;` followed by the use, and drops the
        // sibling-branch pairing that reads identical in the text.
        private static bool PrecedesOnSomePath(SyntaxNode awaited, SyntaxNode use)
        {
            StatementSyntax? useStatement = StatementInBlock(use);

            if (useStatement == null) {
                return false;
            }

            for (SyntaxNode? node = awaited; node != null; node = node.Parent) {
                if (node is StatementSyntax statement && ReferenceEquals(statement.Parent, useStatement.Parent)) {
                    return statement.SpanStart < useStatement.SpanStart;
                }
            }

            return false;
        }

        // Whether execution can actually get from the await to the use. The common shape that cannot is a guard
        // branch: `if (bad) { await Report(...); return; }` followed by the real work. Source order puts the
        // await first and PrecedesOnSomePath lifts it to the enclosing if, but nothing flows out of a block
        // whose end is unreachable -- so ask the compiler rather than pattern-match the shape.
        private static bool ReachesFrom(SyntaxNode awaited, SyntaxNode use, SemanticModel semantics)
        {
            StatementSyntax? useStatement = StatementInBlock(use);

            if (useStatement == null) {
                return false;
            }

            for (SyntaxNode? node = awaited.Parent; node != null && node != useStatement.Parent; node = node.Parent) {
                if (node is not BlockSyntax block || block.Statements.Count == 0) {
                    continue;
                }

                ControlFlowAnalysis? flow = semantics.AnalyzeControlFlow(block);

                if (flow != null && flow.Succeeded && !flow.EndPointIsReachable) {
                    return false;
                }
            }

            return true;
        }

        private static StatementSyntax? StatementInBlock(SyntaxNode node)
        {
            for (SyntaxNode? current = node; current != null; current = current.Parent) {
                if (current is StatementSyntax statement && statement.Parent is BlockSyntax) {
                    return statement;
                }
            }

            return null;
        }

        private static IParameterSymbol? BoundParameter(InvocationExpressionSyntax invocation, IMethodSymbol callee, ArgumentSyntax argument)
        {
            if (argument.NameColon?.Name.Identifier.ValueText is string named) {
                return callee.Parameters.FirstOrDefault(p => p.Name == named);
            }

            int index = invocation.ArgumentList.Arguments.IndexOf(argument);

            return index >= 0 && index < callee.Parameters.Length ? callee.Parameters[index] : null;
        }

        private static bool DeclaredAtOrAfter(ISymbol tracked, int position, CancellationToken cancellationToken)
        {
            foreach (SyntaxReference reference in tracked.DeclaringSyntaxReferences) {
                // The declarator of `T x = await ...` starts before the await and ends after it, so the end is
                // what says whether the await produced this value.
                if (reference.GetSyntax(cancellationToken) is VariableDeclaratorSyntax declarator &&
                    declarator.Span.End >= position) {
                    return true;
                }
            }

            return false;
        }

        private static bool ReProvesCover(SyntaxNode body, int windowStart, int windowEnd, ISymbol tracked,
            CoverModel model, SemanticModel semantics, CancellationToken cancellationToken)
        {
            if (model.SyncType == null) {
                return false;
            }

            foreach (InvocationExpressionSyntax candidate in body.DescendantNodes().OfType<InvocationExpressionSyntax>()) {
                if (candidate.SpanStart < windowStart || candidate.SpanStart >= windowEnd) {
                    continue;
                }

                if (semantics.GetSymbolInfo(candidate, cancellationToken).Symbol is not IMethodSymbol symbol) {
                    continue;
                }

                bool onSync = SymbolEqualityComparer.Default.Equals(symbol.ContainingType, model.SyncType);

                // Sync is not the only way back: a helper that takes the value on a [ProvidesCover] parameter
                // acquires it just as well, and the codebase routes most multi-root acquisitions through one.
                if (!onSync && !symbol.Parameters.Any(model.HasProvidesCover)) {
                    continue;
                }

                foreach (ArgumentSyntax argument in candidate.ArgumentList.Arguments) {
                    if (!onSync) {
                        IParameterSymbol? bound = BoundParameter(candidate, symbol, argument);

                        if (bound == null || !model.HasProvidesCover(bound)) {
                            continue;
                        }
                    }

                    // Names the value anywhere in the acquisition, including inside a collection of roots.
                    foreach (IdentifierNameSyntax mention in argument.DescendantNodesAndSelf().OfType<IdentifierNameSyntax>()) {
                        if (SymbolEqualityComparer.Default.Equals(
                                semantics.GetSymbolInfo(mention, cancellationToken).Symbol, tracked)) {
                            return true;
                        }
                    }
                }
            }

            return false;
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

                // An always-covered parameter is not what the dispatcher synchronized -- it needed no
                // synchronizing. The walk-trigger handler takes the static item first and the critter second,
                // and the critter is the subject. Declaring it on the static half would state nothing.
                if (model.IsAlwaysCovered(parameter.Type)) {
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

            private readonly INamedTypeSymbol? _preservesCover;
            private readonly INamedTypeSymbol _entityType;

            private readonly List<INamedTypeSymbol> _entryMarkers;

            public CoverModel(INamedTypeSymbol requiresCover, INamedTypeSymbol? providesCover, INamedTypeSymbol? preservesCover, INamedTypeSymbol entityType, INamedTypeSymbol? syncType, INamedTypeSymbol? gameType, List<INamedTypeSymbol> entryMarkers)
            {
                _requiresCover = requiresCover;
                _providesCover = providesCover;
                _preservesCover = preservesCover;
                _entityType = entityType;
                SyncType = syncType;
                GameType = gameType;
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

            public INamedTypeSymbol? GameType { get; }

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
            // Baked map data and prototypes carry their own cover, so an obligation for one is already met.
            // The generated class says so itself by overriding the base property.
            public bool IsAlwaysCovered(ITypeSymbol? type)
            {
                for (ITypeSymbol? current = type; current != null; current = current.BaseType) {
                    foreach (ISymbol member in current.GetMembers(AlwaysCoveredMemberName)) {
                        if (member is IPropertySymbol { IsOverride: true }) {
                            return true;
                        }
                    }
                }

                return false;
            }

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
            // Does awaiting this call give the caller back the cover it had?
            public bool PreservesCover(IMethodSymbol method)
            {
                return _preservesCover != null &&
                    method.GetAttributes().Any(a => SymbolEqualityComparer.Default.Equals(a.AttributeClass, _preservesCover));
            }

            public bool ComesFromProvidedCover(ExpressionSyntax expression, SemanticModel semantics, CancellationToken cancellationToken)
            {
                if (_providesCover == null) {
                    return false;
                }

                if (semantics.GetSymbolInfo(expression, cancellationToken).Symbol is IMethodSymbol direct) {
                    return HasProvidesCoverOnReturn(direct);
                }

                // An entity taken out of a covered collection is covered: the annotation is on the collection
                // because that is what the acquisition covered -- `map.GetCrittersInRadius(...)` returns
                // critters the map's own cover reaches. Both ways of taking one out count.
                if (expression is ElementAccessExpressionSyntax element &&
                    ComesFromProvidedCover(element.Expression, semantics, cancellationToken)) {
                    return true;
                }

                if (semantics.GetSymbolInfo(expression, cancellationToken).Symbol is IParameterSymbol parameter) {
                    return HasProvidesCover(parameter);
                }

                // The usual shape is a local initialized from such a call, then passed on.
                if (semantics.GetSymbolInfo(expression, cancellationToken).Symbol is not ILocalSymbol local) {
                    return false;
                }

                // `foreach (Critter other in covered)` binds the loop variable to an element of the collection.
                foreach (SyntaxReference loopRef in local.DeclaringSyntaxReferences) {
                    if (loopRef.GetSyntax(cancellationToken) is ForEachStatementSyntax loop &&
                        ComesFromProvidedCover(loop.Expression, semantics, cancellationToken)) {
                        return true;
                    }
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

                    // `Critter other = critters[i];` -- the element carries the collection's cover.
                    if (ComesFromProvidedCover(initializer, semantics, cancellationToken)) {
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
