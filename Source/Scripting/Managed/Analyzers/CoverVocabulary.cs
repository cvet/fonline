namespace FOnline.Analyzers;

using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using Microsoft.CodeAnalysis;

// Resolved contract vocabulary for one compilation: the attribute symbols the cover contract is written in,
// the engine-owned types the rules are scoped by, and the predicates that read a declaration.
//
// It is a layer of its own because it answers a different kind of question from CoverModel beside it: what a
// declaration SAYS, as opposed to what a callee's body is approximated to do. Keeping the two apart is what
// lets a reader see which answers are contract and which are inference.
//
// Everything here is read from a DECLARATION. Nothing recognises a method by its name: a name is documentation,
// and documentation that decides analysis changes meaning silently when someone renames a method
internal sealed class CoverVocabulary
{
    public const string EntityTypeFullName = "FOnline.Entity";
    public const string EntryPointMarkerAttributeFullName = "FOnline.EntryPointMarkerAttribute";
    public const string RequiresCoverAttributeFullName = "FOnline.RequiresCoverAttribute";
    public const string ProvidesCoverAttributeFullName = "FOnline.ProvidesCoverAttribute";
    public const string PreservesCoverAttributeFullName = "FOnline.PreservesCoverAttribute";
    public const string ReturnsParentAttributeFullName = "FOnline.ReturnsParentAttribute";
    public const string ReturnsAncestorAttributeFullName = "FOnline.ReturnsAncestorAttribute";
    public const string AcquiresCoverAttributeFullName = "FOnline.AcquiresCoverAttribute";
    public const string PassesCoverAttributeFullName = "FOnline.PassesCoverAttribute";
    public const string CoverEffectAttributeFullName = "FOnline.CoverEffectAttribute";

    // The cover primitives are engine-owned, so they are matched by their symbol's full metadata name.
    // Matching a bare type name would let any project class called `Sync` silently discharge an
    // obligation it knows nothing about
    public const string SyncTypeFullName = "FOnline.Sync";
    public const string GameTypeFullName = "FOnline.Game";
    public const string GameLockTypeFullName = "FOnline.GameLock";

    // The raw synchronization surface is declared where it is exported, not listed here. A list of method
    // names is a rule that a rename disarms with nothing to notice it -- and unlike the analyzer's own rules,
    // nothing downstream would report the silence either. `Game.Sync` / `Game.SyncRelease` carry
    // FO_COVER_PRIMITIVE, `Game.IsEntityLocked` carries FO_COVER_PROBE, `Game.Lock` / `Game.Unlock` carry
    // FO_SINGLETON_LOCK, and `Sync.IsCovered` -- the model's own probe, written in C# -- declares
    // [CoverProbe] directly.
    //
    // `Game.TrySyncEntity` carries no marker, for a reason worth keeping: it resolves an *id* to a live
    // entity and covers it, answering false when the entity is gone. Every Sync helper takes an entity, so
    // none can stand in for it -- a handle retained across a yield may already be dead, which is exactly when
    // this is the right call. Marking it would report code for using the only tool that fits
    public const string CoverPrimitiveAttributeFullName = "FOnline.CoverPrimitiveAttribute";
    public const string CoverProbeAttributeFullName = "FOnline.CoverProbeAttribute";
    public const string SingletonLockAttributeFullName = "FOnline.SingletonLockAttribute";

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
    // its Proto<Modifier>) are covered without this engine-owned analyzer having to know their names
    private const string AlwaysCoveredMemberName = "IsAlwaysCovered";

    private CoverVocabulary(Compilation compilation, INamedTypeSymbol entityType)
    {
        CompilationContext = compilation;
        EntityType = entityType;
        EntryPointMarkerAttribute = compilation.GetTypeByMetadataName(EntryPointMarkerAttributeFullName);
        RequiresCoverAttribute = compilation.GetTypeByMetadataName(RequiresCoverAttributeFullName);
        ProvidesCoverAttribute = compilation.GetTypeByMetadataName(ProvidesCoverAttributeFullName);
        PreservesCoverAttribute = compilation.GetTypeByMetadataName(PreservesCoverAttributeFullName);
        ReturnsParentAttribute = compilation.GetTypeByMetadataName(ReturnsParentAttributeFullName);
        ReturnsAncestorAttribute = compilation.GetTypeByMetadataName(ReturnsAncestorAttributeFullName);
        AcquiresCoverAttribute = compilation.GetTypeByMetadataName(AcquiresCoverAttributeFullName);
        PassesCoverAttribute = compilation.GetTypeByMetadataName(PassesCoverAttributeFullName);
        CoverEffectAttribute = compilation.GetTypeByMetadataName(CoverEffectAttributeFullName);
        CoverPrimitiveAttribute = compilation.GetTypeByMetadataName(CoverPrimitiveAttributeFullName);
        CoverProbeAttribute = compilation.GetTypeByMetadataName(CoverProbeAttributeFullName);
        SingletonLockAttribute = compilation.GetTypeByMetadataName(SingletonLockAttributeFullName);
        SyncType = compilation.GetTypeByMetadataName(SyncTypeFullName);
        GameType = compilation.GetTypeByMetadataName(GameTypeFullName);
        GameLockType = compilation.GetTypeByMetadataName(GameLockTypeFullName);
    }

    // Answers null for a compilation that carries no entity type at all, where nothing the contract talks
    // about exists and every rule above it has nothing to say
    public static CoverVocabulary? Resolve(Compilation compilation)
    {
        INamedTypeSymbol? entityType = compilation.GetTypeByMetadataName(EntityTypeFullName);

        if (entityType == null) {
            return null;
        }

        return new CoverVocabulary(compilation, entityType);
    }

    public Compilation CompilationContext { get; }

    public INamedTypeSymbol EntityType { get; }

    public INamedTypeSymbol? RequiresCoverAttribute { get; }

    public INamedTypeSymbol? EntryPointMarkerAttribute { get; }

    public INamedTypeSymbol? ProvidesCoverAttribute { get; }

    public INamedTypeSymbol? PreservesCoverAttribute { get; }

    public INamedTypeSymbol? ReturnsParentAttribute { get; }

    public INamedTypeSymbol? ReturnsAncestorAttribute { get; }

    public INamedTypeSymbol? AcquiresCoverAttribute { get; }

    public INamedTypeSymbol? PassesCoverAttribute { get; }

    public INamedTypeSymbol? CoverEffectAttribute { get; }

    public INamedTypeSymbol? CoverPrimitiveAttribute { get; }

    public INamedTypeSymbol? CoverProbeAttribute { get; }

    public INamedTypeSymbol? SingletonLockAttribute { get; }

    public INamedTypeSymbol? SyncType { get; }

    public INamedTypeSymbol? GameType { get; }

    public INamedTypeSymbol? GameLockType { get; }

    // Whether this compilation has any acquisition at all. `Sync` is declared on every target, but its
    // helpers exist only where entities are synchronized
    public bool CanAcquireCover => SyncType != null && SyncType.GetMembers().OfType<IMethodSymbol>().Any(Acquires);

    // A method that BEGINS an execution context: it carries a marker whose own class is declared
    // [EntryPointMarker]. The dispatcher establishes cover for the arguments it hands such a method, so its entity
    // parameters arrive covered. Sound only because these methods are called by their attribute rule and never
    // directly from regular code -- the convention that keeps an entry point's assumption from leaking into an
    // ordinary call chain. Which markers those are is declared by whoever owns them, engine or embedder alike
    public bool IsEntryPoint(IMethodSymbol method)
    {
        if (EntryPointMarkerAttribute == null) {
            return false;
        }

        foreach (AttributeData attribute in method.GetAttributes()) {
            if (attribute.AttributeClass != null &&
                HasAttribute(attribute.AttributeClass.GetAttributes(), EntryPointMarkerAttribute)) {
                return true;
            }
        }

        return false;
    }

    public bool IsSyncMember(ISymbol? symbol)
    {
        return SyncType != null && symbol != null &&
               SymbolEqualityComparer.Default.Equals(symbol.ContainingType, SyncType);
    }

    public bool HasRequiresCover(IParameterSymbol parameter)
    {
        return RequiresCoverAttribute != null && HasAttribute(parameter.GetAttributes(), RequiresCoverAttribute);
    }

    // On a method the attribute names the receiver, which no parameter can express
    public bool HasRequiresCoverOnMethod(IMethodSymbol method)
    {
        return RequiresCoverAttribute != null && HasAttribute(method.GetAttributes(), RequiresCoverAttribute);
    }

    public bool HasProvidesCover(IParameterSymbol parameter)
    {
        return ProvidesCoverAttribute != null && HasAttribute(parameter.GetAttributes(), ProvidesCoverAttribute);
    }

    public bool HasProvidesCoverOnReturn(IMethodSymbol method)
    {
        return ProvidesCoverAttribute != null && HasAttribute(method.GetReturnTypeAttributes(), ProvidesCoverAttribute);
    }

    public bool HasPreservesCover(IMethodSymbol method)
    {
        return PreservesCoverAttribute != null &&
               HasAttribute(method.OriginalDefinition.GetAttributes(), PreservesCoverAttribute);
    }

    public bool HasPassesCover(IParameterSymbol parameter)
    {
        return PassesCoverAttribute != null && HasAttribute(parameter.GetAttributes(), PassesCoverAttribute);
    }

    public bool ReturnsParent(IMethodSymbol method)
    {
        return ReturnsParentAttribute != null && HasAttribute(method.GetReturnTypeAttributes(), ReturnsParentAttribute);
    }

    public bool ReturnsAncestor(IMethodSymbol method)
    {
        return ReturnsAncestorAttribute != null &&
               HasAttribute(method.GetReturnTypeAttributes(), ReturnsAncestorAttribute);
    }

    // A script helper declared as an acquisition: it establishes cover through Sync for entities it names itself
    public bool AcquiresCover(IMethodSymbol method)
    {
        return AcquiresCoverAttribute != null && HasAttribute(method.GetAttributes(), AcquiresCoverAttribute);
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

    // Baked map data and prototypes carry their own cover, so an obligation for one is already met.
    // The generated class says so itself by overriding the base property
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

    public bool IsEntity(ITypeSymbol? type)
    {
        for (ITypeSymbol? current = type; current != null; current = current.BaseType) {
            if (SymbolEqualityComparer.Default.Equals(current, EntityType)) {
                return true;
            }
        }

        return false;
    }

    // The CoverReach flags an annotation declares, or null when the annotation is absent
    public int? DeclaredReach(ImmutableArray<AttributeData> attributes)
    {
        foreach (AttributeData attribute in attributes) {
            if (!SymbolEqualityComparer.Default.Equals(attribute.AttributeClass, ProvidesCoverAttribute)) {
                continue;
            }

            if (attribute.ConstructorArguments.Length != 0 && attribute.ConstructorArguments[0].Value is int reach) {
                return reach;
            }

            return 0;
        }

        return null;
    }

    public static bool HasAttribute(ImmutableArray<AttributeData> attributes, INamedTypeSymbol wanted)
    {
        foreach (AttributeData attribute in attributes) {
            if (SymbolEqualityComparer.Default.Equals(attribute.AttributeClass, wanted)) {
                return true;
            }
        }

        return false;
    }
}

// Mirrors FOnline.CoverEffectKind: the analyzer reads the value the attribute carries, and the order is the
// contract between them
internal enum CoverEffect
{
    Replace,
    Extend,
    Restore,
    Snapshot,
    Release,
}
