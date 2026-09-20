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
    public const string ReturnsParentAttributeFullName = "FOnline.ReturnsParentAttribute";
    public const string ReturnsAncestorAttributeFullName = "FOnline.ReturnsAncestorAttribute";
    public const string AcquiresCoverAttributeFullName = "FOnline.AcquiresCoverAttribute";
    public const string PassesCoverAttributeFullName = "FOnline.PassesCoverAttribute";
    public const string CoverEffectAttributeFullName = "FOnline.CoverEffectAttribute";
    public const string EntityTypeFullName = "FOnline.Entity";

    // The cover primitives are engine-owned, so they are matched by their symbol's full metadata name.
    // Matching a bare type name would let any project class called `Sync` silently discharge an
    // obligation it knows nothing about.
    public const string SyncTypeFullName = "FOnline.Sync";
    public const string GameTypeFullName = "FOnline.Game";
    public const string GameLockTypeFullName = "FOnline.GameLock";

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

    // The raw synchronization surface is declared where it is exported, not listed here. A list of method
    // names is a rule that a rename disarms with nothing to notice it -- and unlike the analyzer's own rules,
    // nothing downstream would report the silence either. `Game.Sync` / `Game.SyncRelease` carry
    // FO_COVER_PRIMITIVE, `Game.IsEntityLocked` carries FO_COVER_PROBE, `Game.Lock` / `Game.Unlock` carry
    // FO_SINGLETON_LOCK, and `Sync.IsCovered` -- the model's own probe, written in C# -- declares
    // [CoverProbe] directly
    //
    // `Game.TrySyncEntity` carries no marker, for a reason worth keeping: it resolves an *id* to a live
    // entity and covers it, answering false when the entity is gone. Every Sync helper takes an entity, so
    // none can stand in for it -- a handle retained across a yield may already be dead, which is exactly when
    // this is the right call. Marking it would report code for using the only tool that fits
    public const string CoverPrimitiveAttributeFullName = "FOnline.CoverPrimitiveAttribute";
    public const string CoverProbeAttributeFullName = "FOnline.CoverProbeAttribute";
    public const string SingletonLockAttributeFullName = "FOnline.SingletonLockAttribute";

    // The Game methods whose subject is an entity to cover. Game also carries the whole rest of the
    // script surface, so a rule about acquisitions must name these rather than take the type as a whole:
    // a static item passed to Game.CallStaticItemFunction as its subject is an ordinary argument, not an
    // acquisition.
    private static readonly string[] GameCoverPrimitiveNames =
        { "Sync", "SyncRelease", "Lock", "Unlock", "TrySyncEntity", "IsEntityLocked" };

    // Methods that BEGIN an execution context: the engine dispatcher establishes cover for the arguments
    // it hands them, so their entity parameters arrive already covered and need no annotation. This is
    // sound only because such methods are called by their attribute rule and never directly from regular
    // code -- the project convention that keeps an entry point's assumption from leaking into an ordinary
    // call chain.
    private static readonly string[] EntryPointAttributeFullNames = {
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
        id: "FOSYNC001", title: "Cover annotation on a non-entity target",
        messageFormat: "'{0}' is not an entity, so a cover annotation on it means nothing", category: Category,
        defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "[RequiresCover] and [ProvidesCover] describe synchronization cover for an engine entity. " +
            "Applying one to a value that is neither an Entity nor a collection of entities declares a " +
            "contract that can never be satisfied or checked.");

    internal static readonly DiagnosticDescriptor UndischargedCoverRule = new DiagnosticDescriptor(
        id: "FOSYNC002", title: "Cover obligation is neither acquired nor propagated",
        messageFormat: "'{0}' requires the caller to hold cover for '{1}'; '{2}' neither acquires it, receives it from a " +
            "[ProvidesCover] source, nor declares [RequiresCover]",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "Calling a method with a [RequiresCover] parameter obliges the caller to have established " +
            "synchronization cover for the argument. Acquire it (Sync.Lock / Sync.Widen*), pass a value " +
            "that came from a [ProvidesCover] source, or re-declare [RequiresCover] so the obligation " +
            "travels to the next caller.");

    internal static readonly DiagnosticDescriptor EntryPointCoverRule = new DiagnosticDescriptor(
        id: "FOSYNC003", title: "Entry point does not declare the cover the engine gives it",
        messageFormat: "'{0}' is an execution-context entry point; declare [RequiresCover] on '{1}', which the engine has already synchronized",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "The engine synchronizes the subject it dispatches an execution context on -- a remote call's " +
            "Player, an event's own entity -- before the handler runs. Stating that with [RequiresCover] " +
            "makes the guarantee checkable and lets the obligation flow to everything the handler calls; " +
            "leaving it off means callees cannot rely on a contract that in fact holds.");

    internal static readonly DiagnosticDescriptor CoverProbeRule = new DiagnosticDescriptor(
        id: "FOSYNC004", title: "Cover is probed instead of acquired",
        messageFormat: "'{0}' asks whether cover is held; acquire what this code needs with Sync.Lock / Sync.Widen instead",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "A probe answers what was true a moment ago, and code that branches on it either does the work " +
            "unprotected on one path or silently skips it on the other. Production code should state what " +
            "it needs and take it. Tests are the honest exception -- asserting that a call did or did not " +
            "widen the caller cover is exactly how the sync contract is pinned -- so lower this rule for " +
            "test sources in the embedding project's .editorconfig rather than working around it.");

    internal static readonly DiagnosticDescriptor RawSyncPrimitiveRule = new DiagnosticDescriptor(
        id: "FOSYNC005", title: "Raw synchronization primitive used outside its wrapper",
        messageFormat: "'{0}' is a raw synchronization primitive; {1}", category: Category,
        defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "The Sync helpers are not thin wrappers: they acquire multi-root packages as one step and retry " +
            "with a re-proof that nothing migrated in between. Reaching for the primitive directly gets the " +
            "first half and silently drops the second. The Game singleton bucket lock (Game.Lock / Game.Unlock) " +
            "is taken through the GameLock scope instead: a paired call leaves the lock held when anything between " +
            "the two throws, and only the scope, a ref struct, makes holding it across an await a compile error.");

    // FOSYNC006 (singleton lock left held) and FOSYNC007 (singleton lock held across an await) are retired: the
    // GameLock scope releases on every path, and the compiler rejects a ref struct local that survives an await.

    internal static readonly DiagnosticDescriptor CoverLostToAwaitRule = new DiagnosticDescriptor(
        id: "FOSYNC009", title: "Cover for a value is not re-proved after an await",
        messageFormat: "'{0}' was covered before awaiting {1}, but nothing re-proved it after; the call here needs cover for it",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "An await releases the caller cover: the continuation may resume on another thread, and the world " +
            "moves while it waits. A value covered before the await is not covered after it, so the entity may " +
            "have been destroyed or relocated in between. The re-proof is a Sync call naming that value, and its " +
            "result is an answer to act on -- the acquisition can legitimately fail because the entity is gone.");

    internal static readonly DiagnosticDescriptor DiscardedAcquisitionAnswerRule = new DiagnosticDescriptor(
        id: "FOSYNC010", title: "The answer of a cover acquisition is discarded",
        messageFormat: "'{0}' answers whether the cover was established and the answer is discarded; read it, or take the cover back with a best-effort helper that promises no answer",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "An acquisition can legitimately fail: the entity may have been destroyed or migrated while the " +
            "caller waited. The boolean is that answer, so discarding it continues over a value whose cover was " +
            "never established -- which is the very state the re-proof was written to prevent, now invisible " +
            "because nothing branches on it. FOSYNC009 cannot see this: its discharge asks whether an acquisition " +
            "names the value, not whether anything reads the result. Where there is genuinely nothing to decide -- " +
            "cover taken back on the way out, a subject the code has already proved gone -- the best-effort " +
            "helpers say so in the call itself and answer nothing at all.");

    internal static readonly DiagnosticDescriptor UndeclaredCoverEffectRule = new DiagnosticDescriptor(
        id: "FOSYNC011", title: "A Sync helper changes the held cover without declaring what it does",
        messageFormat: "'{0}' changes the held cover through {1} but declares no [CoverEffect]; every rule reads the effect from the declaration, so cover stops being tracked through this call",
        category: Category, defaultSeverity: DiagnosticSeverity.Warning, isEnabledByDefault: true,
        description: "What a call does to the held cover is read from its own [CoverEffect] declaration, never from its " +
            "name. That is what keeps a rename a rename -- and it is also why a helper added to Sync without the " +
            "attribute is invisible to the whole analysis: no rule objects, the build stays green, and cover " +
            "silently stops being tracked through it. A helper that only asks a question -- a membership " +
            "comparison, a probe, a collector -- changes nothing and declares nothing, which is why the rule asks " +
            "about reaching the primitive or another changing helper rather than about being declared in Sync.");

    public override ImmutableArray<DiagnosticDescriptor> SupportedDiagnostics {
        get;
    } = ImmutableArray.Create(NonEntityTargetRule, UndischargedCoverRule, EntryPointCoverRule, CoverProbeRule,
                              RawSyncPrimitiveRule, CoverLostToAwaitRule, DiscardedAcquisitionAnswerRule,
                              UndeclaredCoverEffectRule);

    public override void Initialize(AnalysisContext context)
    {
        context.ConfigureGeneratedCodeAnalysis(GeneratedCodeAnalysisFlags.None);
        context.EnableConcurrentExecution();

        context.RegisterCompilationStartAction(
            compilationStart =>
            {
                Compilation compilation = compilationStart.Compilation;

                INamedTypeSymbol? requiresCover = compilation.GetTypeByMetadataName(RequiresCoverAttributeFullName);
                INamedTypeSymbol? providesCover = compilation.GetTypeByMetadataName(ProvidesCoverAttributeFullName);
                INamedTypeSymbol? preservesCover = compilation.GetTypeByMetadataName(PreservesCoverAttributeFullName);
                INamedTypeSymbol? returnsParent = compilation.GetTypeByMetadataName(ReturnsParentAttributeFullName);
                INamedTypeSymbol? returnsAncestor = compilation.GetTypeByMetadataName(ReturnsAncestorAttributeFullName);
                INamedTypeSymbol? acquiresCover = compilation.GetTypeByMetadataName(AcquiresCoverAttributeFullName);
                INamedTypeSymbol? passesCover = compilation.GetTypeByMetadataName(PassesCoverAttributeFullName);
                INamedTypeSymbol? coverEffect = compilation.GetTypeByMetadataName(CoverEffectAttributeFullName);
                INamedTypeSymbol? coverPrimitive = compilation.GetTypeByMetadataName(CoverPrimitiveAttributeFullName);
                INamedTypeSymbol? coverProbe = compilation.GetTypeByMetadataName(CoverProbeAttributeFullName);
                INamedTypeSymbol? singletonLock = compilation.GetTypeByMetadataName(SingletonLockAttributeFullName);
                INamedTypeSymbol? entityType = compilation.GetTypeByMetadataName(EntityTypeFullName);

                if (requiresCover == null || entityType == null) {
                    return;
                }

                INamedTypeSymbol? syncType = compilation.GetTypeByMetadataName(SyncTypeFullName);
                INamedTypeSymbol? gameType = compilation.GetTypeByMetadataName(GameTypeFullName);
                INamedTypeSymbol? gameLockType = compilation.GetTypeByMetadataName(GameLockTypeFullName);

                var entryMarkers = new List<INamedTypeSymbol>();

                foreach (string name in EntryPointAttributeFullNames) {
                    INamedTypeSymbol? marker = compilation.GetTypeByMetadataName(name);

                    if (marker != null) {
                        entryMarkers.Add(marker);
                    }
                }

                var model = new CoverModel(compilation,
                                           requiresCover,
                                           providesCover,
                                           preservesCover,
                                           returnsParent,
                                           returnsAncestor,
                                           acquiresCover,
                                           passesCover,
                                           coverEffect,
                                           coverPrimitive,
                                           coverProbe,
                                           singletonLock,
                                           entityType,
                                           syncType,
                                           gameType,
                                           gameLockType,
                                           entryMarkers);

                compilationStart.RegisterSymbolAction(symbolContext => AnalyzeDeclaration(symbolContext, model),
                                                      SymbolKind.Method);

                compilationStart.RegisterSyntaxNodeAction(nodeContext => AnalyzeInvocation(nodeContext, model),
                                                          SyntaxKind.InvocationExpression);

                compilationStart.RegisterSyntaxNodeAction(nodeContext => AnalyzeSyncSurfaceUse(nodeContext, model),
                                                          SyntaxKind.InvocationExpression);

                compilationStart.RegisterSyntaxNodeAction(nodeContext =>
                                                              AnalyzeEntryPointDeclaration(nodeContext, model),
                                                          SyntaxKind.MethodDeclaration);

                compilationStart.RegisterSyntaxNodeAction(nodeContext =>
                                                              AnalyzeSyncHelperDeclaration(nodeContext, model),
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

            context.ReportDiagnostic(Diagnostic.Create(NonEntityTargetRule, location, "the return value"));
        }
    }

    private static void Report(SymbolAnalysisContext context, CoverModel model, IParameterSymbol parameter,
                               string display)
    {
        Location location = parameter.Locations.FirstOrDefault() ?? Location.None;

        context.ReportDiagnostic(Diagnostic.Create(NonEntityTargetRule, location, display));
    }

    // FOSYNC002 -- the obligation must be discharged at every call site.
    // A helper answers when it hands back a bool -- directly or through a task. The best-effort forms answer
    // nothing, which is how a call says in its own name that there is nothing to decide here
    private static bool AnswersWhetherCoverWasTaken(IMethodSymbol method)
    {
        ITypeSymbol type = method.ReturnType;

        if (type is INamedTypeSymbol { IsGenericType : true } task &&
            (task.Name == "Task" || task.Name == "ValueTask") && task.TypeArguments.Length == 1) {
            type = task.TypeArguments[0];
        }

        return type.SpecialType == SpecialType.System_Boolean;
    }

    // The answer is thrown away when the call is the whole statement, with or without an await, and when it is
    // assigned to a discard. Everything else -- a condition, a return, an argument, a local -- reads it
    private static bool IsDiscardedResult(InvocationExpressionSyntax invocation)
    {
        SyntaxNode? node = invocation;

        while (node?.Parent is ParenthesizedExpressionSyntax or AwaitExpressionSyntax) {
            node = node.Parent;
        }

        if (node?.Parent is AssignmentExpressionSyntax assignment &&
            assignment.Left is IdentifierNameSyntax { Identifier.ValueText : "_" }) {
            return true;
        }

        return node?.Parent is ExpressionStatementSyntax;
    }

    // The two rules the retired sync-flow audit owned that need no dataflow at all: they are about which
    // surface a call reaches for, not about what it proves. Both are scoped by the *symbol's* containing
    // type, so a project class called Sync or Game cannot silently satisfy or trip them.
    private static void AnalyzeSyncSurfaceUse(SyntaxNodeAnalysisContext context, CoverModel model)
    {
        var invocation = (InvocationExpressionSyntax)context.Node;

        if (context.SemanticModel.GetSymbolInfo(invocation, context.CancellationToken)
                .Symbol is not IMethodSymbol callee) {
            return;
        }

        INamedTypeSymbol? callerType = (context.ContainingSymbol as IMethodSymbol)?.ContainingType;

        // Inside Sync itself both are the implementation, not a smell.
        if (model.SyncType != null && SymbolEqualityComparer.Default.Equals(callerType, model.SyncType)) {
            return;
        }

        bool onSync =
            model.SyncType != null && SymbolEqualityComparer.Default.Equals(callee.ContainingType, model.SyncType);
        bool onGame =
            model.GameType != null && SymbolEqualityComparer.Default.Equals(callee.ContainingType, model.GameType);

        if (!onSync && !onGame) {
            return;
        }

        // FOSYNC010 -- the acquisition answered and nobody listened
        if (onSync && AnswersWhetherCoverWasTaken(callee) && IsDiscardedResult(invocation)) {
            context.ReportDiagnostic(
                Diagnostic.Create(DiscardedAcquisitionAnswerRule, invocation.GetLocation(), "Sync." + callee.Name));
            return;
        }

        if (model.IsCoverProbe(callee)) {
            context.ReportDiagnostic(Diagnostic.Create(CoverProbeRule, invocation.GetLocation(), callee.Name));
            return;
        }

        if (model.IsCoverPrimitive(callee)) {
            context.ReportDiagnostic(
                Diagnostic.Create(RawSyncPrimitiveRule,
                                  invocation.GetLocation(),
                                  "Game." + callee.Name,
                                  "use the Sync helpers, which acquire atomically and re-prove after migration"));
            return;
        }

        // The marker names the pair exactly, so no arity check is needed to keep an unrelated overload out
        bool isSingletonPair = model.IsSingletonLock(callee);

        if (isSingletonPair && !SymbolEqualityComparer.Default.Equals(callerType, model.GameLockType)) {
            context.ReportDiagnostic(
                Diagnostic.Create(RawSyncPrimitiveRule,
                                  invocation.GetLocation(),
                                  "Game." + callee.Name,
                                  "take the singleton lock with 'using GameLock scope = GameLock.Acquire();', " +
                                      "which releases it on every path and cannot be held across an await"));
        }
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

        // Cover exists only where `Sync` can acquire it. A target whose scripts run on one thread compiles the
        // helpers out (the client and the mapper see an empty `Sync`), so nothing there could satisfy an
        // obligation and nothing is owed: a shared helper's [RequiresCover] names the server contract
        if (!model.CanAcquireCover) {
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
        ReportCoverLostToAwait(context,
                               model,
                               body,
                               invocation,
                               callee,
                               demanding,
                               demandsReceiver,
                               caller,
                               semantics,
                               cancellationToken);

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
                context.ReportDiagnostic(Diagnostic.Create(UndischargedCoverRule,
                                                           receiver.GetLocation(),
                                                           callee.Name,
                                                           "the receiver",
                                                           caller?.Name ?? "the enclosing member"));
            }
        }

        foreach (IParameterSymbol parameter in demanding) {
            ExpressionSyntax? argument = ArgumentFor(invocation, callee, parameter);

            // No entity, nothing to cover: an omitted optional parameter takes its default, which for an entity
            // is null, and a `null` or `default` argument says the same thing explicitly
            if (argument == null || IsNullValue(argument, semantics, cancellationToken)) {
                continue;
            }

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
            if (argument != null && model.CoveredByEarlierCall(body, argument, semantics, cancellationToken)) {
                continue;
            }

            context.ReportDiagnostic(Diagnostic.Create(UndischargedCoverRule,
                                                       (argument ?? (ExpressionSyntax)invocation).GetLocation(),
                                                       callee.Name,
                                                       parameter.Name,
                                                       caller?.Name ?? "the enclosing member"));
        }
    }

    // The gate in front of the per-value check: only values whose obligation is otherwise discharged.
    private static void ReportCoverLostToAwait(SyntaxNodeAnalysisContext context, CoverModel model, SyntaxNode body,
                                               InvocationExpressionSyntax invocation, IMethodSymbol callee,
                                               List<IParameterSymbol> demanding, bool demandsReceiver,
                                               IMethodSymbol? caller, SemanticModel semantics,
                                               CancellationToken cancellationToken)
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

            if (propagates || acquires || model.ComesFromProvidedCover(receiver, semantics, cancellationToken) ||
                model.CoveredByEarlierCall(body, receiver, semantics, cancellationToken)) {
                ReportIfCoverLostToAwait(context,
                                         model,
                                         body,
                                         invocation,
                                         receiver,
                                         caller,
                                         semantics,
                                         cancellationToken);
            }
        }

        foreach (IParameterSymbol parameter in demanding) {
            ExpressionSyntax? argument = ArgumentFor(invocation, callee, parameter);

            if (argument == null) {
                continue;
            }

            if (propagates || acquires || model.ComesFromProvidedCover(argument, semantics, cancellationToken) ||
                model.CoveredByEarlierCall(body, argument, semantics, cancellationToken)) {
                ReportIfCoverLostToAwait(context,
                                         model,
                                         body,
                                         invocation,
                                         argument,
                                         caller,
                                         semantics,
                                         cancellationToken);
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
                                                 InvocationExpressionSyntax invocation, ExpressionSyntax value,
                                                 IMethodSymbol? caller, SemanticModel semantics,
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
        AwaitExpressionSyntax? firstAwait = null;

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

            if (candidate.Span.End > callSite) {
                continue;
            }

            if (lastAwait == null || candidate.SpanStart > lastAwait.SpanStart) {
                lastAwait = candidate;
            }

            if (firstAwait == null || candidate.SpanStart < firstAwait.SpanStart) {
                firstAwait = candidate;
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

        // The same holds for a local the body re-reads after the await -- `map = cr.GetMap();` once the attacker
        // has been locked again. What the use sees is what that assignment put there, not what the await released
        if (ReboundAfter(body, lastAwait.SpanStart, invocation, tracked, semantics, cancellationToken)) {
            return;
        }

        // The re-proof may be the await itself (`await Sync.Widen(... value ...)`), which starts at or after
        // the await's own start, so the window opens there rather than after it.
        if (ReProvesCover(body,
                          lastAwait.SpanStart,
                          callSite,
                          firstAwait?.SpanStart ?? lastAwait.SpanStart,
                          tracked,
                          model,
                          semantics,
                          cancellationToken)) {
            return;
        }

        string awaitedName =
            semantics.GetSymbolInfo(lastAwait.Expression, cancellationToken).Symbol is IMethodSymbol awaitedMethod
                ? awaitedMethod.Name
                : "the call";

        context.ReportDiagnostic(
            Diagnostic.Create(CoverLostToAwaitRule, value.GetLocation(), value.ToString(), awaitedName));
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

    private static IParameterSymbol? BoundParameter(InvocationExpressionSyntax invocation, IMethodSymbol callee,
                                                    ArgumentSyntax argument)
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
            SyntaxNode declaration = reference.GetSyntax(cancellationToken);

            // The declarator of `T x = await ...` starts before the await and ends after it, so the end is
            // what says whether the await produced this value.
            if (declaration is VariableDeclaratorSyntax declarator && declarator.Span.End >= position) {
                return true;
            }

            // A deconstruction `(T x, bool y) = await ...` declares through a designation that ends before the
            // await, so the assignment it sits on the left of is what answers the same question
            if (declaration is SingleVariableDesignationSyntax designation &&
                designation.Ancestors().OfType<AssignmentExpressionSyntax>().FirstOrDefault() is {} deconstruction &&
                deconstruction.Left.Span.Contains(designation.Span) && deconstruction.Span.End >= position) {
                return true;
            }
        }

        return false;
    }

    // An assignment to the tracked value between the await and the use, placed where the use cannot be reached
    // without it: the same block, earlier in it. The value at the use is that assignment's, so whatever the await
    // did to the old one says nothing about it
    private static bool ReboundAfter(SyntaxNode body, int windowStart, SyntaxNode use, ISymbol tracked,
                                     SemanticModel semantics, CancellationToken cancellationToken)
    {
        foreach (AssignmentExpressionSyntax assignment in body.DescendantNodes().OfType<AssignmentExpressionSyntax>()) {
            if (assignment.SpanStart < windowStart || assignment.SpanStart >= use.SpanStart ||
                !assignment.IsKind(SyntaxKind.SimpleAssignmentExpression)) {
                continue;
            }

            if (!SymbolEqualityComparer.Default.Equals(
                    semantics.GetSymbolInfo(assignment.Left, cancellationToken).Symbol?.OriginalDefinition,
                    tracked.OriginalDefinition)) {
                continue;
            }

            // Unlike an await, an assignment counts only when it cannot be skipped: one inside a branch leaves
            // the stale value on the other path, so the statement has to sit in the use's own block
            StatementSyntax? assigned = StatementInBlock(assignment);
            StatementSyntax? used = StatementInBlock(use);

            if (assigned != null && used != null && ReferenceEquals(assigned.Parent, used.Parent) &&
                assigned.SpanStart < used.SpanStart) {
                return true;
            }
        }

        return false;
    }

    private static bool ReProvesCover(SyntaxNode body, int windowStart, int windowEnd, int coverHeldUntil,
                                      ISymbol tracked, CoverModel model, SemanticModel semantics,
                                      CancellationToken cancellationToken)
    {
        if (model.SyncType == null) {
            return false;
        }

        // Restoring a snapshot taken while the value was still covered puts that cover back -- the snapshot is
        // the cover, so it names the value without mentioning it
        if (RestoresSnapshotTakenWhileCovered(body,
                                              windowStart,
                                              windowEnd,
                                              coverHeldUntil,
                                              tracked,
                                              model,
                                              semantics,
                                              cancellationToken)) {
            return true;
        }

        foreach (InvocationExpressionSyntax candidate in body.DescendantNodes().OfType<InvocationExpressionSyntax>()) {
            if (candidate.SpanStart < windowStart || candidate.SpanStart >= windowEnd) {
                continue;
            }

            if (semantics.GetSymbolInfo(candidate, cancellationToken).Symbol is not IMethodSymbol symbol) {
                continue;
            }

            bool acquisition = model.Acquires(symbol);

            // An acquisition is not the only way back: a helper that takes the value on a providing parameter
            // acquires it just as well, and the codebase routes most multi-root acquisitions through one.
            if (!acquisition && !symbol.Parameters.Any(model.ProvidesCover)) {
                continue;
            }

            foreach (ArgumentSyntax argument in candidate.ArgumentList.Arguments) {
                if (!acquisition) {
                    IParameterSymbol? bound = BoundParameter(candidate, symbol, argument);

                    if (bound == null || !model.ProvidesCover(bound)) {
                        continue;
                    }
                }

                // Names the value anywhere in the acquisition, including through a local list of roots the
                // body fills before handing it over
                if (model.NamesValue(argument.Expression, tracked, body, semantics)) {
                    return true;
                }
            }
        }

        return false;
    }

    // `List<Entity> cover = Sync.Snapshot();` early, `await Sync.Restore(cover)` after the re-entrant work: the
    // restore re-establishes exactly what was held at the snapshot, so a value covered then is covered again.
    // The snapshot has to predate the await that took the cover away, or it captured a set the value had already
    // dropped out of.
    private static bool RestoresSnapshotTakenWhileCovered(SyntaxNode body, int windowStart, int windowEnd,
                                                          int coverHeldUntil, ISymbol tracked, CoverModel model,
                                                          SemanticModel semantics, CancellationToken cancellationToken)
    {
        foreach (InvocationExpressionSyntax candidate in body.DescendantNodes().OfType<InvocationExpressionSyntax>()) {
            if (candidate.SpanStart < windowStart || candidate.SpanStart >= windowEnd ||
                candidate.ArgumentList.Arguments.Count != 1) {
                continue;
            }

            if (semantics.GetSymbolInfo(candidate, cancellationToken).Symbol is not IMethodSymbol restore ||
                model.EffectOf(restore) != CoverEffect.Restore) {
                continue;
            }

            if (semantics.GetSymbolInfo(candidate.ArgumentList.Arguments[0].Expression, cancellationToken)
                    .Symbol is not ILocalSymbol snapshot) {
                continue;
            }

            foreach (SyntaxReference reference in snapshot.DeclaringSyntaxReferences) {
                if (reference.GetSyntax(cancellationToken) is not
                    VariableDeclaratorSyntax { Initializer.Value : {} initializer } declarator ||
                    declarator.SpanStart >= coverHeldUntil) {
                    continue;
                }

                if (semantics.GetSymbolInfo(initializer, cancellationToken).Symbol is IMethodSymbol source &&
                    model.EffectOf(source) == CoverEffect.Snapshot &&
                    !DeclaredAtOrAfter(tracked, declarator.SpanStart, cancellationToken)) {
                    return true;
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

        if (context.SemanticModel.GetDeclaredSymbol(declaration, context.CancellationToken)
                is not IMethodSymbol method) {
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
                    Diagnostic.Create(EntryPointCoverRule,
                                      DeclarationLocation(parameter, method, context.CancellationToken),
                                      method.Name,
                                      parameter.Name));
            }

            return;
        }
    }

    // FOSYNC011 -- a helper inside Sync that changes the held set says so on its own declaration. The rule
    // asks what the body reaches, not what the method is called: the primitive itself, or another helper whose
    // declared effect changes the set. A predicate that only reports stays silent, and so does a body that
    // merely takes a snapshot -- a snapshot changes nothing
    private static void AnalyzeSyncHelperDeclaration(SyntaxNodeAnalysisContext context, CoverModel model)
    {
        var declaration = (MethodDeclarationSyntax)context.Node;

        if (context.SemanticModel.GetDeclaredSymbol(declaration, context.CancellationToken)
                is not IMethodSymbol method) {
            return;
        }

        if (model.SyncType == null || !SymbolEqualityComparer.Default.Equals(method.ContainingType, model.SyncType) ||
            model.EffectOf(method) != null) {
            return;
        }

        SyntaxNode? body = (SyntaxNode?)declaration.Body ?? declaration.ExpressionBody;

        if (body == null) {
            return;
        }

        foreach (InvocationExpressionSyntax call in body.DescendantNodes().OfType<InvocationExpressionSyntax>()) {
            if (context.SemanticModel.GetSymbolInfo(call, context.CancellationToken)
                    .Symbol is not IMethodSymbol callee) {
                continue;
            }

            bool primitive = model.IsCoverPrimitive(callee);

            if (!primitive && !model.Acquires(callee) && model.EffectOf(callee) != CoverEffect.Release) {
                continue;
            }

            context.ReportDiagnostic(Diagnostic.Create(UndeclaredCoverEffectRule,
                                                       declaration.Identifier.GetLocation(),
                                                       method.Name,
                                                       primitive ? "Game." + callee.Name : callee.Name));
            return;
        }
    }

    // v1 is body-scoped, not path-sensitive: any cover acquisition anywhere in the enclosing body
    // discharges the obligation. That direction is deliberate -- it under-reports rather than blocking a
    // build on a branch the analyzer cannot yet follow. Path sensitivity wants a ControlFlowGraph walk,
    // not a wider syntax scan.
    private static bool AcquiresCover(SyntaxNode body, SemanticModel semantics, CoverModel model,
                                      CancellationToken cancellationToken)
    {
        if (model.SyncType == null) {
            return false;
        }

        foreach (InvocationExpressionSyntax candidate in body.DescendantNodes().OfType<InvocationExpressionSyntax>()) {
            if (semantics.GetSymbolInfo(candidate, cancellationToken).Symbol is not IMethodSymbol symbol) {
                continue;
            }

            if (model.AcquiresCover(symbol) || model.Acquires(symbol)) {
                return true;
            }
        }

        return false;
    }

    // The whole parameter declaration, not just its identifier: it is what a reader wants highlighted, and
    // it is where an inserted attribute has to go -- `[RequiresCover] Critter cr`, never `Critter [..] cr`.
    private static Location DeclarationLocation(IParameterSymbol parameter, IMethodSymbol method,
                                                CancellationToken cancellationToken)
    {
        foreach (SyntaxReference reference in parameter.DeclaringSyntaxReferences) {
            if (reference.GetSyntax(cancellationToken) is ParameterSyntax syntax) {
                return syntax.GetLocation();
            }
        }

        return parameter.Locations.FirstOrDefault() ?? method.Locations.FirstOrDefault() ?? Location.None;
    }

    private static bool IsNullValue(ExpressionSyntax expression, SemanticModel semantics,
                                    CancellationToken cancellationToken)
    {
        Optional < object ? > constant = semantics.GetConstantValue(expression, cancellationToken);
        return constant.HasValue && constant.Value == null;
    }

    private static ExpressionSyntax? ArgumentFor(InvocationExpressionSyntax invocation, IMethodSymbol callee,
                                                 IParameterSymbol parameter)
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

            if (i < callee.Parameters.Length &&
                SymbolEqualityComparer.Default.Equals(callee.Parameters[i], parameter)) {
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
    // Mirrors FOnline.CoverEffectKind: the analyzer reads the value the attribute carries, and the order is the
    // contract between them
    private enum CoverEffect
    {
        Replace,
        Extend,
        Restore,
        Snapshot,
        Release,
    }

    private sealed class CoverModel
    {
        private readonly Compilation CompilationContext;
        private readonly ConcurrentDictionary<IMethodSymbol, bool> PreservingCache =
            new ConcurrentDictionary<IMethodSymbol, bool>(SymbolEqualityComparer.Default);
        private readonly ConcurrentDictionary<IParameterSymbol, bool> ProvidingCache =
            new ConcurrentDictionary<IParameterSymbol, bool>(SymbolEqualityComparer.Default);
        private readonly INamedTypeSymbol RequiresCoverAttribute;
        private readonly INamedTypeSymbol? ProvidesCoverAttribute;

        private readonly INamedTypeSymbol? PreservesCoverAttribute;
        private readonly INamedTypeSymbol? ReturnsParentAttribute;
        private readonly INamedTypeSymbol? ReturnsAncestorAttribute;
        private readonly INamedTypeSymbol? AcquiresCoverAttribute;
        private readonly INamedTypeSymbol? PassesCoverAttribute;
        private readonly INamedTypeSymbol? CoverEffectAttribute;
        private readonly INamedTypeSymbol? CoverPrimitiveAttribute;
        private readonly INamedTypeSymbol? CoverProbeAttribute;
        private readonly INamedTypeSymbol? SingletonLockAttribute;

        // CoverReach.Parent and CoverReach.Ancestors, as the attribute's constructor argument carries them
        private const int ReachParent = 1 << 0;
        private const int ReachAncestors = 1 << 1;
        private readonly INamedTypeSymbol EntityType;

        private readonly List<INamedTypeSymbol> EntryMarkers;

        public CoverModel(Compilation compilation, INamedTypeSymbol requiresCover, INamedTypeSymbol? providesCover,
                          INamedTypeSymbol? preservesCover, INamedTypeSymbol? returnsParent,
                          INamedTypeSymbol? returnsAncestor, INamedTypeSymbol? acquiresCover,
                          INamedTypeSymbol? passesCover, INamedTypeSymbol? coverEffect,
                          INamedTypeSymbol? coverPrimitive, INamedTypeSymbol? coverProbe,
                          INamedTypeSymbol? singletonLock, INamedTypeSymbol entityType, INamedTypeSymbol? syncType,
                          INamedTypeSymbol? gameType, INamedTypeSymbol? gameLockType,
                          List<INamedTypeSymbol> entryMarkers)
        {
            CompilationContext = compilation;
            RequiresCoverAttribute = requiresCover;
            ProvidesCoverAttribute = providesCover;
            PreservesCoverAttribute = preservesCover;
            ReturnsParentAttribute = returnsParent;
            ReturnsAncestorAttribute = returnsAncestor;
            AcquiresCoverAttribute = acquiresCover;
            PassesCoverAttribute = passesCover;
            CoverEffectAttribute = coverEffect;
            CoverPrimitiveAttribute = coverPrimitive;
            CoverProbeAttribute = coverProbe;
            SingletonLockAttribute = singletonLock;
            EntityType = entityType;
            SyncType = syncType;
            GameType = gameType;
            GameLockType = gameLockType;
            EntryMarkers = entryMarkers;
        }

        public bool IsEntryPoint(IMethodSymbol method)
        {
            foreach (INamedTypeSymbol marker in EntryMarkers) {
                if (HasAttribute(method.GetAttributes(), marker)) {
                    return true;
                }
            }

            return false;
        }

        public INamedTypeSymbol? SyncType { get; }

        // Whether this compilation has any acquisition at all. `Sync` is declared on every target, but its
        // helpers exist only where entities are synchronized
        public bool CanAcquireCover => SyncType != null && SyncType.GetMembers().OfType<IMethodSymbol>().Any(Acquires);

        public INamedTypeSymbol? GameType { get; }

        public INamedTypeSymbol? GameLockType { get; }

        public bool HasRequiresCover(IParameterSymbol parameter)
        {
            return HasAttribute(parameter.GetAttributes(), RequiresCoverAttribute);
        }

        // On a method the attribute names the receiver, which no parameter can express.
        public bool HasRequiresCoverOnMethod(IMethodSymbol method)
        {
            return HasAttribute(method.GetAttributes(), RequiresCoverAttribute);
        }

        public bool HasProvidesCover(IParameterSymbol parameter)
        {
            return ProvidesCoverAttribute != null && HasAttribute(parameter.GetAttributes(), ProvidesCoverAttribute);
        }

        // Does handing the value to this parameter leave it covered when the call returns? Declared with
        // [ProvidesCover], or proved from the callee's own body: an acquisition naming that parameter, taken
        // where the method cannot return past it without having run it, and not undone by anything the body
        // awaits afterwards. A conditional acquisition proves nothing -- a helper that widens only for a
        // transport and returns true on every other path gives no guarantee at all -- so the shape is limited
        // to a top-level acquisition and the guard form (`if (!await Sync...) { return; }`) that means the same.
        public bool ProvidesCover(IParameterSymbol parameter)
        {
            return HasProvidesCover(parameter) || ProvidesCoverByBody(parameter, new Recursion());
        }

        private bool ProvidesCoverByBody(IParameterSymbol parameter, Recursion recursion)
        {
            IParameterSymbol definition = parameter.OriginalDefinition;

            if (ProvidingCache.TryGetValue(definition, out bool cached)) {
                return cached;
            }

            if (!recursion.Parameters.Add(definition)) {
                recursion.HitCycle = true;

                return false;
            }

            bool outerCycle = recursion.HitCycle;
            recursion.HitCycle = false;
            bool result = ComputesToProvidingBody(definition, recursion);
            recursion.Parameters.Remove(definition);

            if (result || !recursion.HitCycle) {
                ProvidingCache[definition] = result;
            }

            recursion.HitCycle |= outerCycle;

            return result;
        }

        private bool ComputesToProvidingBody(IParameterSymbol parameter, Recursion recursion)
        {
            if (parameter.ContainingSymbol is not IMethodSymbol method || !IsEntityish(parameter.Type)) {
                return false;
            }

            foreach (SyntaxReference reference in method.DeclaringSyntaxReferences) {
                SyntaxNode declaration = reference.GetSyntax();
                SyntaxNode? body = declaration switch {
                    BaseMethodDeclarationSyntax m => (SyntaxNode?)m.Body ?? m.ExpressionBody,
                    LocalFunctionStatementSyntax f => (SyntaxNode?)f.Body ?? f.ExpressionBody,
                    _ => null,
                };

                if (body == null || !CompilationContext.ContainsSyntaxTree(declaration.SyntaxTree)) {
                    continue;
                }

                SemanticModel semantics = CompilationContext.GetSemanticModel(declaration.SyntaxTree);

                foreach (InvocationExpressionSyntax candidate in body.DescendantNodes()
                             .OfType<InvocationExpressionSyntax>()) {
                    if (!AcquiresFor(candidate, parameter, body, semantics, recursion) ||
                        !RunsOnEveryReturningPath(candidate, body, semantics)) {
                        continue;
                    }

                    if (HoldsUntilReturn(candidate, parameter, body, semantics, recursion)) {
                        return true;
                    }
                }
            }

            return false;
        }

        // An acquisition that names this parameter: `Sync` itself, or another method that provides for the
        // parameter the value is handed to
        private bool AcquiresFor(InvocationExpressionSyntax call, IParameterSymbol parameter, SyntaxNode body,
                                 SemanticModel semantics, Recursion recursion)
        {
            if (semantics.GetSymbolInfo(call).Symbol is not IMethodSymbol callee) {
                return false;
            }

            SeparatedSyntaxList<ArgumentSyntax> arguments = call.ArgumentList.Arguments;

            for (int i = 0; i < arguments.Count; i++) {
                if (!NamesValue(arguments[i].Expression, parameter, body, semantics)) {
                    continue;
                }

                if (Acquires(callee)) {
                    return true;
                }

                IParameterSymbol? bound = BoundParameter(call, callee, arguments[i]);

                if (bound != null && (HasProvidesCover(bound) || ProvidesCoverByBody(bound, recursion))) {
                    return true;
                }
            }

            return false;
        }

        // The value named directly, or through a local collection the acquisition takes as its roots --
        // `List<Entity> roots = new List<Entity> { cr, map }; await Sync.Widen(roots);` names both
        public bool NamesValue(ExpressionSyntax expression, ISymbol value, SyntaxNode body, SemanticModel semantics)
        {
            foreach (IdentifierNameSyntax mention in expression.DescendantNodesAndSelf()
                         .OfType<IdentifierNameSyntax>()) {
                ISymbol? symbol = semantics.GetSymbolInfo(mention).Symbol;

                if (SymbolEqualityComparer.Default.Equals(symbol?.OriginalDefinition, value.OriginalDefinition)) {
                    return true;
                }

                if (symbol is not ILocalSymbol local || !IsEntityish(local.Type)) {
                    continue;
                }

                foreach (SyntaxNode contribution in CollectionContributions(local, body, semantics)) {
                    foreach (IdentifierNameSyntax inner in contribution.DescendantNodesAndSelf()
                                 .OfType<IdentifierNameSyntax>()) {
                        if (SymbolEqualityComparer.Default.Equals(
                                semantics.GetSymbolInfo(inner).Symbol?.OriginalDefinition,
                                value.OriginalDefinition)) {
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        // What a local collection was built from: its initializer plus every Add/AddRange on it in this body
        private IEnumerable<SyntaxNode> CollectionContributions(ILocalSymbol local, SyntaxNode body,
                                                                SemanticModel semantics)
        {
            foreach (SyntaxReference reference in local.DeclaringSyntaxReferences) {
                if (reference.GetSyntax() is VariableDeclaratorSyntax { Initializer.Value : {} initializer }) {
                    yield return initializer;
                }
            }

            foreach (InvocationExpressionSyntax call in body.DescendantNodes().OfType<InvocationExpressionSyntax>()) {
                if (call.Expression is not MemberAccessExpressionSyntax access ||
                    (access.Name.Identifier.ValueText != "Add" && access.Name.Identifier.ValueText != "AddRange") ||
                    !SymbolEqualityComparer.Default.Equals(semantics.GetSymbolInfo(access.Expression).Symbol, local)) {
                    continue;
                }

                foreach (ArgumentSyntax argument in call.ArgumentList.Arguments) {
                    yield return argument.Expression;
                }
            }
        }

        // Only a statement of the body itself, or the condition of a guard whose branch never falls through, is
        // run by every path that returns after it. Anything deeper -- a loop, an else, a switch section -- is a
        // choice the caller cannot see, and a contract that holds only on one branch is not a contract
        private bool RunsOnEveryReturningPath(SyntaxNode call, SyntaxNode body, SemanticModel semantics)
        {
            for (SyntaxNode? node = call; node != null && node != body; node = node.Parent) {
                if (node is not StatementSyntax statement) {
                    continue;
                }

                if (!ReferenceEquals(statement.Parent, body)) {
                    return false;
                }

                if (statement is not IfStatementSyntax guard) {
                    return true;
                }

                // The call sits in the guard's own condition, so it runs; the branch must take the failure away
                if (!guard.Condition.Span.Contains(call.Span) || guard.Else != null) {
                    return false;
                }

                ControlFlowAnalysis? flow = semantics.AnalyzeControlFlow(guard.Statement);

                return flow != null && flow.Succeeded && !flow.EndPointIsReachable;
            }

            return false;
        }

        // Nothing awaited after the acquisition may take the cover away again: a later await either preserves
        // what it found or names the value itself
        private bool HoldsUntilReturn(SyntaxNode acquisition, IParameterSymbol parameter, SyntaxNode body,
                                      SemanticModel semantics, Recursion recursion)
        {
            foreach (AwaitExpressionSyntax later in body.DescendantNodes().OfType<AwaitExpressionSyntax>()) {
                if (later.SpanStart <= acquisition.SpanStart) {
                    continue;
                }

                if (later.Expression is InvocationExpressionSyntax invocation &&
                    AcquiresFor(invocation, parameter, body, semantics, recursion)) {
                    continue;
                }

                if (semantics.GetSymbolInfo(later.Expression).Symbol is IMethodSymbol awaited &&
                    PreservesCover(awaited)) {
                    continue;
                }

                return false;
            }

            return true;
        }

        public bool HasProvidesCoverOnReturn(IMethodSymbol method)
        {
            return ProvidesCoverAttribute != null &&
                   HasAttribute(method.GetReturnTypeAttributes(), ProvidesCoverAttribute);
        }

        // Baked map data and prototypes carry their own cover, so an obligation for one is already met.
        // The generated class says so itself by overriding the base property.
        public bool IsAlwaysCovered(ITypeSymbol? type)
        {
            for (ITypeSymbol? current = type; current != null; current = current.BaseType) {
                foreach (ISymbol member in current.GetMembers(AlwaysCoveredMemberName)) {
                    if (member is IPropertySymbol { IsOverride : true }) {
                        return true;
                    }
                }
            }

            return false;
        }

        // An entity, or a collection of them: `List<Critter>` carries the contract element-wise, which is
        // why the old string grammar needed a `[*]` marker and this one does not. The same holds through nesting,
        // so a `Task<(Critter?, bool)>` names the critter it resolves
        public bool IsEntityish(ITypeSymbol type)
        {
            if (IsEntity(type)) {
                return true;
            }

            if (type is IArrayTypeSymbol array) {
                return IsEntityish(array.ElementType);
            }

            if (type is INamedTypeSymbol named && named.IsGenericType) {
                return named.TypeArguments.Any(IsEntityish);
            }

            return false;
        }

        // A script helper declared as an acquisition: it establishes cover through Sync for entities it names itself
        public bool AcquiresCover(IMethodSymbol method)
        {
            return AcquiresCoverAttribute != null && HasAttribute(method.GetAttributes(), AcquiresCoverAttribute);
        }

        // Does awaiting this call give the caller back the cover it had? Declared with [PreservesCover], or
        // proved here from the callee's own body.
        public bool PreservesCover(IMethodSymbol method)
        {
            if (PreservesCoverAttribute != null &&
                method.GetAttributes().Any(
                    a => SymbolEqualityComparer.Default.Equals(a.AttributeClass, PreservesCoverAttribute))) {
                return true;
            }

            return PreservesCoverByBody(method, new Recursion());
        }

        // `Sync.Widen` re-establishes the cover it found and adds to it, so a body whose every await widens --
        // directly, or through another method this proves the same of -- hands the caller back everything it
        // held. That is exactly what [PreservesCover] asserts, and proving it beats asking for it: the contract
        // exists in hundreds of helpers that never wrote it down, and a proved fact cannot drift from the body
        // the way a written one can. The annotation stays for what no body here shows -- a helper that locks and
        // then restores the caller's snapshot, and anything compiled elsewhere.
        //
        // Conservative in one direction only: an await this cannot resolve, or a body outside this compilation,
        // answers "does not preserve", which reports rather than hides.
        private bool PreservesCoverByBody(IMethodSymbol method, Recursion recursion)
        {
            IMethodSymbol definition = method.OriginalDefinition;

            if (PreservingCache.TryGetValue(definition, out bool cached)) {
                return cached;
            }

            // A cycle proves nothing on its own: answer "no" for the recursive edge rather than assuming
            if (!recursion.Methods.Add(definition)) {
                recursion.HitCycle = true;

                return false;
            }

            bool outerCycle = recursion.HitCycle;
            recursion.HitCycle = false;
            bool result = ComputesToPreservingBody(definition, recursion);
            recursion.Methods.Remove(definition);

            // A "no" that came out of a cycle edge depends on where the walk started, so it is not a fact to keep
            if (result || !recursion.HitCycle) {
                PreservingCache[definition] = result;
            }

            recursion.HitCycle |= outerCycle;

            return result;
        }

        private bool ComputesToPreservingBody(IMethodSymbol method, Recursion recursion)
        {
            if (method.DeclaringSyntaxReferences.Length == 0) {
                return false;
            }

            bool restoresAnything = false;

            foreach (SyntaxReference reference in method.DeclaringSyntaxReferences) {
                SyntaxNode declaration = reference.GetSyntax();
                SyntaxNode? body = declaration switch {
                    BaseMethodDeclarationSyntax m => (SyntaxNode?)m.Body ?? m.ExpressionBody,
                    LocalFunctionStatementSyntax f => (SyntaxNode?)f.Body ?? f.ExpressionBody,
                    _ => null,
                };

                if (body == null || !CompilationContext.ContainsSyntaxTree(declaration.SyntaxTree)) {
                    return false;
                }

                SemanticModel semantics = CompilationContext.GetSemanticModel(declaration.SyntaxTree);

                // Releasing the cover outright is the one way a body drops it without awaiting anything
                foreach (InvocationExpressionSyntax call in body.DescendantNodes()
                             .OfType<InvocationExpressionSyntax>()) {
                    if (semantics.GetSymbolInfo(call).Symbol is IMethodSymbol released &&
                        EffectOf(released) == CoverEffect.Release) {
                        return false;
                    }
                }

                foreach (AwaitExpressionSyntax await in body.DescendantNodes().OfType<AwaitExpressionSyntax>()) {
                    if (semantics.GetSymbolInfo(await.Expression).Symbol is not IMethodSymbol awaited) {
                        return false;
                    }

                    if (EffectOf(awaited) is {} effect) {
                        if (effect != CoverEffect.Extend && !RestoresOwnSnapshot(await.Expression, body, semantics) &&
                            !RepairedRightAfter(await, body, semantics)) {
                            return false;
                        }
                    }
                    else if (!PreservesCoverByBody(awaited, recursion) && !RepairedRightAfter(await, body, semantics)) {
                        return false;
                    }

                    restoresAnything = true;
                }
            }

            // A body that never awaits re-establishes nothing, and the rule's premise is that awaiting at all
            // is what costs the caller its cover -- so only a body that puts it back counts as preserving
            return restoresAnything;
        }

        // `List<Entity> cover = Sync.Snapshot(); ... await Sync.Restore(cover);` puts back what the body found,
        // so the caller loses nothing across it
        private bool RestoresOwnSnapshot(ExpressionSyntax call, SyntaxNode body, SemanticModel semantics)
        {
            if (call is not InvocationExpressionSyntax invocation ||
                semantics.GetSymbolInfo(invocation).Symbol is not IMethodSymbol restore ||
                EffectOf(restore) != CoverEffect.Restore || invocation.ArgumentList.Arguments.Count != 1) {
                return false;
            }

            if (semantics.GetSymbolInfo(invocation.ArgumentList.Arguments[0].Expression)
                    .Symbol is not ILocalSymbol snapshot) {
                return false;
            }

            foreach (SyntaxReference reference in snapshot.DeclaringSyntaxReferences) {
                if (reference.GetSyntax() is VariableDeclaratorSyntax { Initializer.Value : {} initializer } &&
                    semantics.GetSymbolInfo(initializer).Symbol is IMethodSymbol source &&
                    EffectOf(source) == CoverEffect.Snapshot) {
                    return true;
                }
            }

            return false;
        }

        // The other preserving shape, written out by hand where a callee is known to replace the cover: take the
        // snapshot, run the re-entrant work, and put the snapshot back in the very next statement, leaving on
        // failure. Nothing between the two can observe the gap, so the caller loses nothing across the whole body
        private bool RepairedRightAfter(AwaitExpressionSyntax awaited, SyntaxNode body, SemanticModel semantics)
        {
            StatementSyntax? statement = null;

            for (SyntaxNode? node = awaited; node != null && node != body; node = node.Parent) {
                if (node is StatementSyntax candidate && candidate.Parent is BlockSyntax) {
                    statement = candidate;
                    break;
                }
            }

            if (statement?.Parent is not BlockSyntax block) {
                return false;
            }

            int index = block.Statements.IndexOf(statement);

            if (index < 0 || index + 1 >= block.Statements.Count) {
                return false;
            }

            foreach (AwaitExpressionSyntax repair in block.Statements[index + 1]
                         .DescendantNodesAndSelf()
                         .OfType<AwaitExpressionSyntax>()) {
                if (RestoresOwnSnapshot(repair.Expression, body, semantics)) {
                    return true;
                }
            }

            return false;
        }

        private bool IsSyncMethod(IMethodSymbol method)
        {
            return SyncType != null && SymbolEqualityComparer.Default.Equals(method.ContainingType, SyncType);
        }

        // What the call does to the held cover, as its own declaration states it. Reading the effect instead of
        // the method's name is what keeps a rename a rename: nothing here recognises `Widen`, `Restore` or
        // `Lock` as words
        public CoverEffect? EffectOf(IMethodSymbol method)
        {
            if (CoverEffectAttribute == null) {
                return null;
            }

            foreach (AttributeData attribute in method.OriginalDefinition.GetAttributes()) {
                if (!SymbolEqualityComparer.Default.Equals(attribute.AttributeClass, CoverEffectAttribute) ||
                    attribute.ConstructorArguments.Length == 0 ||
                    attribute.ConstructorArguments[0].Value is not int value) {
                    continue;
                }

                return (CoverEffect)value;
            }

            return null;
        }

        // The raw surface, as the export declares itself
        public bool IsCoverPrimitive(IMethodSymbol method)
        {
            return CoverPrimitiveAttribute != null &&
                   HasAttribute(method.OriginalDefinition.GetAttributes(), CoverPrimitiveAttribute);
        }

        public bool IsCoverProbe(IMethodSymbol method)
        {
            return CoverProbeAttribute != null &&
                   HasAttribute(method.OriginalDefinition.GetAttributes(), CoverProbeAttribute);
        }

        public bool IsSingletonLock(IMethodSymbol method)
        {
            return SingletonLockAttribute != null &&
                   HasAttribute(method.OriginalDefinition.GetAttributes(), SingletonLockAttribute);
        }

        // An acquisition is any effect that leaves the job holding something it can name
        public bool Acquires(IMethodSymbol method)
        {
            CoverEffect? effect = EffectOf(method);

            return effect is CoverEffect.Replace or CoverEffect.Extend or CoverEffect.Restore;
        }

        // One walk of the call graph: what it is standing in, and whether it had to cut a cycle to answer
        private sealed class Recursion
        {
            public HashSet<IMethodSymbol> Methods { get; } = new HashSet<IMethodSymbol>(SymbolEqualityComparer.Default);

            public HashSet<IParameterSymbol> Parameters {
                get;
            } = new HashSet<IParameterSymbol>(SymbolEqualityComparer.Default);

            public bool HitCycle { get; set; }
        }

        public bool ComesFromProvidedCover(ExpressionSyntax expression, SemanticModel semantics,
                                           CancellationToken cancellationToken)
        {
            return ProvidedReach(expression, semantics, cancellationToken) != null;
        }

        // The reach with which the value arrives covered -- the CoverReach flags its provider declared -- or null
        // when nothing provides it. The reach is what lets an upward accessor's result count as covered: a critter
        // provided with CoverReach.Parent comes with its map, so `cr.GetMap()` is covered too
        private int? ProvidedReach(ExpressionSyntax expression, SemanticModel semantics,
                                   CancellationToken cancellationToken)
        {
            if (ProvidesCoverAttribute == null) {
                return null;
            }

            if (expression is ParenthesizedExpressionSyntax parenthesized) {
                return ProvidedReach(parenthesized.Expression, semantics, cancellationToken);
            }

            if (expression is AwaitExpressionSyntax awaited) {
                return ProvidedReach(awaited.Expression, semantics, cancellationToken);
            }

            // `ok ? Provide() : null` -- a choice arrives covered when every branch does, with the reach they share;
            // a branch that yields nothing owes nothing, so it narrows no reach
            if (expression is ConditionalExpressionSyntax choice) {
                int? whenTrue = ChoiceBranchReach(choice.WhenTrue, semantics, cancellationToken);
                int? whenFalse = ChoiceBranchReach(choice.WhenFalse, semantics, cancellationToken);

                return whenTrue != null && whenFalse != null ? whenTrue.Value & whenFalse.Value : null;
            }

            ISymbol? symbol = semantics.GetSymbolInfo(expression, cancellationToken).Symbol;

            if (symbol is IMethodSymbol direct) {
                int? returned = DeclaredReach(direct.GetReturnTypeAttributes());

                if (returned != null) {
                    return returned;
                }

                // A pass-through hands back the very argument it was given, cover and reach included
                if (expression is InvocationExpressionSyntax passThrough && PassesCoverAttribute != null) {
                    foreach (IParameterSymbol passed in direct.Parameters) {
                        ExpressionSyntax? argument = HasAttribute(passed.GetAttributes(), PassesCoverAttribute)
                                                       ? ArgumentFor(passThrough, direct, passed)
                                                       : null;

                        if (argument != null) {
                            return ProvidedReach(argument, semantics, cancellationToken);
                        }
                    }
                }

                // An upward accessor hands back the receiver's parent or an ancestor, which is covered exactly
                // when the receiver arrived with the reach that includes it
                if (expression is InvocationExpressionSyntax { Expression : MemberAccessExpressionSyntax access }) {
                    return UpwardReach(direct, access.Expression, semantics, cancellationToken);
                }

                return null;
            }

            // An entity taken out of a covered collection is covered: the annotation is on the collection
            // because that is what the acquisition covered -- `map.GetCrittersInRadius(...)` returns
            // critters the map's own cover reaches. Both ways of taking one out count.
            if (expression is ElementAccessExpressionSyntax element) {
                return ProvidedReach(element.Expression, semantics, cancellationToken);
            }

            if (symbol is IParameterSymbol parameter) {
                return DeclaredReach(parameter.GetAttributes()) ??
                       HandedOverReach(expression, symbol, semantics, cancellationToken);
            }

            // The usual shape is a local initialized from such a call, then passed on.
            if (symbol is not ILocalSymbol local) {
                return null;
            }

            foreach (SyntaxReference reference in local.DeclaringSyntaxReferences) {
                SyntaxNode declaration = reference.GetSyntax(cancellationToken);

                // `foreach (Critter other in covered)` binds the loop variable to an element of the collection
                if (declaration is ForEachStatementSyntax loop) {
                    int? looped = ProvidedReach(loop.Expression, semantics, cancellationToken);

                    if (looped != null) {
                        return looped;
                    }

                    continue;
                }

                // `(Critter? cr, bool loaded) = await Resolve(id);` -- a deconstructed local carries the cover the
                // call provides for the entity half of what it returns
                if (declaration is SingleVariableDesignationSyntax designation) {
                    AssignmentExpressionSyntax? deconstruction =
                        designation.Ancestors().OfType<AssignmentExpressionSyntax>().FirstOrDefault();

                    if (deconstruction == null || !deconstruction.Left.Span.Contains(designation.Span)) {
                        continue;
                    }

                    int? deconstructed = ProvidedReach(deconstruction.Right, semantics, cancellationToken);

                    if (deconstructed != null) {
                        return deconstructed;
                    }

                    continue;
                }

                ExpressionSyntax? initializer = (declaration as VariableDeclaratorSyntax)?.Initializer?.Value;

                // `Critter other = critters[i];` -- the element carries the collection's cover
                int? initialized =
                    initializer != null ? ProvidedReach(initializer, semantics, cancellationToken) : null;

                if (initialized != null) {
                    return initialized;
                }
            }

            return HandedOverReach(expression, symbol, semantics, cancellationToken);
        }

        private int? ChoiceBranchReach(ExpressionSyntax branch, SemanticModel semantics,
                                       CancellationToken cancellationToken)
        {
            return IsNullValue(branch, semantics, cancellationToken)
                     ? ~0
                     : ProvidedReach(branch, semantics, cancellationToken);
        }

        private int? UpwardReach(IMethodSymbol accessor, ExpressionSyntax receiver, SemanticModel semantics,
                                 CancellationToken cancellationToken)
        {
            bool returnsParent = ReturnsParentAttribute != null &&
                                 HasAttribute(accessor.GetReturnTypeAttributes(), ReturnsParentAttribute);
            bool returnsAncestor = ReturnsAncestorAttribute != null &&
                                   HasAttribute(accessor.GetReturnTypeAttributes(), ReturnsAncestorAttribute);

            if (!returnsParent && !returnsAncestor) {
                return null;
            }

            int? receiverReach = ProvidedReach(receiver, semantics, cancellationToken);

            if (receiverReach == null) {
                return null;
            }

            bool ancestors = (receiverReach.Value & ReachAncestors) != 0;

            // The whole chain stays covered above the parent, so the result carries Ancestors on; one step of
            // Parent is spent getting here and leaves the result with no reach of its own
            if (ancestors) {
                return ReachAncestors;
            }

            if (returnsParent && (receiverReach.Value & ReachParent) != 0) {
                return 0;
            }

            return null;
        }

        private int? DeclaredReach(ImmutableArray<AttributeData> attributes)
        {
            foreach (AttributeData attribute in attributes) {
                if (!SymbolEqualityComparer.Default.Equals(attribute.AttributeClass, ProvidesCoverAttribute)) {
                    continue;
                }

                if (attribute.ConstructorArguments.Length != 0 &&
                    attribute.ConstructorArguments[0].Value is int reach) {
                    return reach;
                }

                return 0;
            }

            return null;
        }

        // Some earlier call in this body handed the same value to a [ProvidesCover] parameter, which is
        // how a helper establishes cover for something it does not return. Body-scoped like the rest of
        // the discharge rule, so it under-reports rather than blocking a build on control flow the
        // analyzer cannot yet follow.
        public bool CoveredByEarlierCall(SyntaxNode body, ExpressionSyntax expression, SemanticModel semantics,
                                         CancellationToken cancellationToken)
        {
            ISymbol? wanted = semantics.GetSymbolInfo(expression, cancellationToken).Symbol;

            return wanted != null && HandedOverReach(body, wanted, semantics, cancellationToken) != null;
        }

        // The same rule asked of a local or a parameter met as a receiver: the reach its [ProvidesCover] parameter
        // declares is what lets `cr.GetMap()` count after `WidenWithMap(cr)`
        private int? HandedOverReach(ExpressionSyntax expression, ISymbol wanted, SemanticModel semantics,
                                     CancellationToken cancellationToken)
        {
            SyntaxNode? body = EnclosingBody(expression);

            return body != null ? HandedOverReach(body, wanted, semantics, cancellationToken) : null;
        }

        // The union of the reaches every such call declares, or null when no call hands the value over
        private int? HandedOverReach(SyntaxNode body, ISymbol wanted, SemanticModel semantics,
                                     CancellationToken cancellationToken)
        {
            if (ProvidesCoverAttribute == null) {
                return null;
            }

            int? handedOver = null;

            foreach (InvocationExpressionSyntax candidate in body.DescendantNodes()
                         .OfType<InvocationExpressionSyntax>()) {
                if (semantics.GetSymbolInfo(candidate, cancellationToken).Symbol is not IMethodSymbol callee) {
                    continue;
                }

                SeparatedSyntaxList<ArgumentSyntax> arguments = candidate.ArgumentList.Arguments;

                for (int i = 0; i < arguments.Count && i < callee.Parameters.Length; i++) {
                    int? reach = DeclaredReach(callee.Parameters[i].GetAttributes());

                    if (reach == null) {
                        continue;
                    }

                    ISymbol? passed = semantics.GetSymbolInfo(arguments[i].Expression, cancellationToken).Symbol;

                    if (passed != null && SymbolEqualityComparer.Default.Equals(passed, wanted)) {
                        handedOver = (handedOver ?? 0) | reach.Value;
                    }
                }
            }

            return handedOver;
        }

        private bool IsEntity(ITypeSymbol type)
        {
            for (ITypeSymbol? current = type; current != null; current = current.BaseType) {
                if (SymbolEqualityComparer.Default.Equals(current, EntityType)) {
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
