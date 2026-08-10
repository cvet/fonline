# Sync-Cover Analysis (Managed Scripts)

Compile-time checking of the entity synchronization cover contract for managed (C#) scripts. It is the
managed counterpart of [ThreadSafetyAnalysis.md](ThreadSafetyAnalysis.md): annotations on the code, checked
by the compiler, gated in the build — rather than a convention a reviewer has to remember.

## Why annotations

The cover contract used to live in `// SyncScope:` comments plus an external dataflow audit. Both failed in
the same way: a comment cannot be checked, so it drifts from the code it describes, and an analyzer that
infers cover across the whole program degrades silently when a callee leaves its view — it falls back to
"may-destroy" and produces pessimism instead of proof.

Declared contracts do not have that failure mode. Each method states what it needs; the analyzer checks each
call against the declaration. An unmet contract is a diagnostic, never a gap in a table.

## The three attributes

All three live in `Source/Scripting/Managed/CoreScripts/Attributes.cs` and target **parameters**, so a rename
carries the annotation with it and there is no name to get wrong.

| Attribute | Where | Meaning |
|-----------|-------|---------|
| `[SyncCover]` | entry-point parameter | The engine synchronizes this argument **before** starting the execution context. |
| `[RequiresCover]` | downstream parameter | The caller must already hold cover for this argument. |
| `[ProvidesCover]` | parameter or return value | This method establishes cover for the annotated value. |

`[SyncCover]` is the only one with **runtime effect**: it is the declarative replacement for opening a
handler with a manual `Sync.Lock` of what it was just handed, and it is the preferred way to establish that
cover. It is optional — omitting it means the handler does not need the entity covered.

`[RequiresCover]` is **purely static**. It changes no behavior and exists so the compiler can prove the
obligation is met; the cover itself comes from `[SyncCover]` at the entry or an explicit `Sync` call
mid-flight.

The chain is: entry points declare what arrives covered → everything called along the way declares what it
needs → helpers that lock what they hand back declare that too. This is sound because an entry point is
invoked by its attribute rule and **never called from ordinary code**, so its assumption cannot leak into a
normal call chain.

## `CoverReach`

Extra entities that share the annotated one's requirement, named by **relation**, never by concrete type —
an item's parent may be a critter, a map or a containing item, so `Map` would be the wrong name for the same
relation.

| Member | Meaning |
|--------|---------|
| `Parent` | The immediate sync-hierarchy parent: a critter's map, an item's holder, a map's location. |
| `Ancestors` | The whole parent chain. |
| `DestroyGraph` | Everything the entity's destruction would cascade through. |

Element-wise application over a collection parameter needs no syntax: it follows from the parameter's type.

**There is deliberately no `ControlledCritter` / `OwningPlayer`.** `Critter` and `Player` are linked through
`Critter::_player` / `Player::_controlledCr` rather than the parent chain, and `SyncContext::SyncEntities`
widens every acquisition across that link **symmetrically** via `GetSyncWidenEntity()`, iterated to a fixed
point — holding either half always covers the other. Declaring it would state a requirement the engine
already guarantees, and locking it by hand is redundant (see [ServerRuntime.md](ServerRuntime.md)).

The parent direction is the opposite case, which is why `Parent`/`Ancestors` exist: acquisition takes
"EXACTLY the requested entities plus each one's sync-widen partner, and NOTHING else"
(`Source/Server/EntitySync.cpp`). Holding a map covers the critters beneath it; holding a critter does **not**
reach up to its map. Sibling-to-parent escalation and parent-cover reduction were both removed deliberately.

## Diagnostics

| Id | Reports |
|----|---------|
| `FOSYNC001` | A cover annotation on a value that is neither an entity nor a collection of entities. |
| `FOSYNC002` | An argument for a `[RequiresCover]` parameter that is neither covered by the caller, received from a `[ProvidesCover]` source, declared `[SyncCover]` in an entry point, nor re-declared. |
| `FOSYNC003` | An entry point that locks its own parameter by hand instead of declaring `[SyncCover]`. |

Severities come from the embedding project's `.editorconfig`. Note that the generated managed project sets
`TreatWarningsAsErrors`, so promoting a rule to `warning` makes it a hard build failure — roll out by
severity, not all at once.

## Where it lives

- Analyzer: `Source/Scripting/Managed/Analyzers/` (`netstandard2.0`, which is how the compiler loads
  analyzers). The `Microsoft.CodeAnalysis.CSharp` version must stay **below** the Roslyn in the SDK running
  the build: an analyzer may be older than its host compiler, never newer.
- Rule changelog: `AnalyzerReleases.{Shipped,Unshipped}.md` — required by Roslyn's own RS2008.
- Self-tests: `Source/Scripting/Managed/Analyzers/Tests/`, a plain console runner (compile a snippet, assert
  the reported ids). `dotnet run` exits 0 when every case passes.
- Wiring: the embedding project points `Script.ManagedScriptAnalyzers` at the analyzer project;
  `ManagedScriptBaker` emits it as `<ProjectReference OutputItemType="Analyzer"
  ReferenceOutputAssembly="false" GlobalPropertiesToRemove="OutputPath;Configuration;Platform" />`. The
  `GlobalPropertiesToRemove` is load-bearing: without it the analyzer inherits the script project's
  `OutputPath` and lands in the baked script assembly directory, where the baker would pick it up as a
  runtime script assembly.

## Current limitation

The discharge rule is body-scoped, not path-sensitive: any cover acquisition anywhere in the enclosing body
discharges the obligation. That direction is deliberate — it under-reports rather than blocking a build on a
branch the analyzer cannot yet follow. Cover established through a *parameter* of a called method, and
entities taken from a covered collection, are not followed yet either.
