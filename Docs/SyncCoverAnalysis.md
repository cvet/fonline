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

## The two attributes

Both live in `Source/Scripting/Managed/CoreScripts/Attributes.cs`. They sit on the declaration they describe
— a parameter, a method (for its receiver), or a return value — so a rename carries the annotation with it
and there is no name to get wrong.

| Attribute | Where | Meaning |
|-----------|-------|---------|
| `[RequiresCover]` | parameter | Cover for this argument is already held when the method runs. |
| `[RequiresCover]` | method | Same, but for the **receiver** — `cr.SendGlobalMapGroupInfo()` needs `cr` covered, and the receiver is not in the parameter list. |
| `[ProvidesCover]` | parameter or return value | This method establishes cover for the annotated value as it runs. |

**Neither attribute locks anything.** Both are purely static; they exist so the compiler can prove an
obligation is met. Cover is established by ordinary `Sync.*` calls, written where they are needed.

`[RequiresCover]` means two things depending on where it sits, and that split is the design rather than an
overload:

- **On an execution-context entry point** it records a guarantee the engine already gives. The dispatcher
  synchronizes the subject it dispatches on — a remote call's `Player`, an event's own entity — before the
  handler runs, so the annotation states a fact rather than making a request. FOSYNC003 checks that every
  entry point states it.
- **Everywhere else** it is a demand on the caller: this method reads or mutates the entity and acquires
  nothing itself.

The chain follows from that. A method carrying `[RequiresCover]` discharges the obligation for whatever it
calls with that argument, so the entry point's guarantee flows onward by the ordinary propagation rule. This
is sound because an entry point is invoked by its attribute rule and **never called from ordinary code**, so
its assumption cannot leak into a normal call chain.

### Why the attributes do not synchronize

Having the annotation establish cover automatically was built twice — once natively in the dispatcher, once
in script by wrapping the handler at subscribe time — and rejected on evidence, not taste.

The manual calls are not "lock these". `Sync.LockCrittersWithMap(a, b)` acquires both roots *and* their maps
as one atomic step, with a retry that re-proves neither migrated. An attribute composing per-root steps loses
that, and doing so crashed the combat suite with a destroyed-entity access. Reproducing the real contracts
would need an attribute that *selects* the right combined helper for each parameter-set shape — re-encoding
the whole `Sync` API in declarative form, which is a worse interface than calling the helper.

The manual form also returns a bool that handlers answer differently on purpose: return, log, or fall back to
a last-known hex. A central policy cannot reproduce that, and picking one silently would be worse than the
boilerplate it removes.

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
| `FOSYNC002` | An argument for a `[RequiresCover]` parameter that is neither covered by the caller, received from a `[ProvidesCover]` source, nor re-declared. |
| `FOSYNC003` | An execution-context entry point that does not declare `[RequiresCover]` on the entity the engine already synchronized for it. |

FOSYNC001 and FOSYNC003 gate the build as errors: neither has a backlog -- a cover annotation on a non-entity
can never be satisfied, and every entry point is annotated. FOSYNC002 is held lower while annotation coverage
grows. Severities come from the embedding project's `.editorconfig`. Note that the generated managed project sets
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

## Next: the engine's own exports

The contracts that matter most are the native ones — a script's cover obligation almost always exists
because some `FO_SCRIPT_API` call reads or mutates an entity. Those are recorded today in
`Tools/SyncScopeAudit/contracts/native_exports.json` in the embedding project: **88 entries, 74 with a
`requires_access`**, in 16 distinct shapes. The distribution decides the design:

| Shape | Count |
|-------|-------|
| `['self']` — the receiver alone | 44 |
| argument-based (`$0`, `$0[*]`, `$0.destroy_graph`, …) | 27 roots |
| receiver with reach (`self.map`, `self.attachment_graph`, …) | rest |

So the dominant case is the receiver, which is why `[RequiresCover]` now also targets methods. Moving these
onto `///@ ExportMethod` would let codegen emit the attribute on the generated managed API, and every script
call into the engine would be checked by the same rule as script-to-script calls — which is the other half of
what the external audit does, and the last thing standing between it and retirement.

**Both halves are wired end to end**, and they are declared in different places on purpose — each contract
sits on the thing it describes.

The **receiver** half is not declared per method at all: it is the default. Every server entity export reads
or mutates its receiver, so the caller owns that entity's cover — the standing rule for the whole native
surface (see the cover paragraph in [../AGENTS.md](../AGENTS.md)), not a property some exports have. The
audit data agrees: all 44 receiver-only contracts recorded by the external audit are ordinary instance
methods (`Map.GetCritters`, `Critter.GetItems`, `Item.GetMap`), and every contract *without* a receiver
requirement is a `Game.*` static with no receiver at all. So `ManagedScriptBaker` emits `[RequiresCover]`
above every non-static method of a server entity class, gated on
`!is_static && !is_fixed_type && target_name == "Server"` — 582 methods today. Client and mapper get none:
those scripts run single-threaded, so the contract would name a guarantee with no mechanism behind it.

**There is no per-export opt-in for the receiver, and deliberately so.** The gate already excludes
everything without a covered receiver: statics have none, `RefType` and `FixedType` owners are not entities
(annotating them would trip FOSYNC001), and client/mapper scripts have no synchronization to name. A
`///@ ExportMethod RequiresCover` flag was built first and then removed once the default landed — all three
of its users were server entity instance methods, so it had no non-redundant case left.

The **argument** and **return** contracts are markers on the declaration itself — two empty macros that
mirror the two managed attributes one for one:

```cpp
FO_SCRIPT_API void Server_Map_VerifyTrigger(ptr<Map> self, FO_REQUIRES_COVER ptr<Critter> cr, mpos hex, mdir dir)
FO_SCRIPT_API FO_PROVIDES_COVER ptr<Critter> Server_Map_GetCritter(ptr<Map> self, ident_t crId)
```

Both expand to nothing — the compiler never sees them, codegen does — and each has to be stripped before its
type is parsed: `parse_method_args` splits a parameter on its last space, and the return type is joined from
the tokens between `FO_SCRIPT_API` and the function name. They then travel as `MethodArg.requires_cover` →
`ArgDesc::RequiresCover` → a C# parameter attribute, and `ExportMethodTag.ret_provides_cover` →
`MethodDesc::ReturnProvidesCover` → `[return: ProvidesCover]`. Putting them on the declaration rather than in
an `///@` list keeps each contract where a reader meets the thing it describes, and it survives reordering.

**Two markers rather than one whose meaning comes from position.** A return value can only ever *provide*
cover, but a parameter can do either: an ordinary export requires its argument to be covered, while the
explicit synchronization primitives (`Game.Sync`, `Game.Lock`) exist precisely to provide it. A single
positional marker could not say the second thing, because in parameter position it would already mean the
first. `FO_PROVIDES_COVER` is wired for the return position today, which is the case that exists; the
parameter position is the natural extension when those primitives get declared.

`FO_PROVIDES_COVER` marks a **downward accessor** — one whose returned entities live under its receiver, so
the receiver's cover already covers them, and the value discharges the obligation at the next call that takes
it. The direction has to be declared rather than inferred: `Map.GetCritters` provides cover for what it
returns, while `Critter.GetMap` returns a *parent* and provides nothing (acquisition takes the requested
entities plus their sync-widen partners and nothing else). 44 accessors on `Map`, `Location`, `Critter` and
`Item` are declared; the upward ones are deliberately left bare.

Declared so far: `Map.VerifyTrigger`, `Player.SwitchCritter`, `Game.LoginPlayerToNewRecord`. Each reports at
real call sites, independently of the receiver default.

### What the native contracts cost in diagnostics

Turning the receiver default on took FOSYNC002 from 141 sites to 2 616. Declaring the 44 downward accessors
and exempting `Sync` itself (the cover *mechanism* cannot be asked to already hold cover for what it is about
to acquire) brought it to **2 484** — only 5 % of the jump, which is the useful measurement: the bulk is not
accessor blindness but ordinary propagation. Those bodies call a server entity method while holding no
acquisition of their own, because their *caller* holds it — which is exactly what `[RequiresCover]` on the
parameter says. It is also close to fully mechanical: 2 348 of the sites are a receiver and 136 an argument,
and **2 468 of 2 484 are a bare identifier**, so each one names the parameter to annotate. The pass iterates
to a fixed point, and where the obligation reaches a top-level body that genuinely holds nothing, that is a
real defect the rule has surfaced.

**Count diagnostics deduplicated by `(file, line, column)`.** `dotnet build` prints each one twice, once per
MSBuild node, and the two copies differ only in their `N>` prefix — so a naive line count reports exactly
double.

FOSYNC001 and FOSYNC003 stayed at 0 across the change, and the managed build stayed clean — so the default is
carried entirely by FOSYNC002's backlog, not by the gates.

What remains is the reach vocabulary. `destroy_graph`, `attachment_graph`, `transfer_global_batch`,
`transfer_global_group` are engine-specific closures rather than parent walks, so each needs a decided
meaning before it can be declared — and a valued marker form to carry it. Declaring one without that is the
same silent under-cover that sank the auto-sync attempt.

## Current limitation

The discharge rule is body-scoped, not path-sensitive: any cover acquisition anywhere in the enclosing body
discharges the obligation.

**That limitation has a live failure to point at.** Managed gameplay tests hit an intermittent
`ScriptSystemException: Managed entity target is destroyed` in a family of bodies that all share one shape:
an entity reference is taken, an `await` follows, and the reference is used afterwards. Observed at
`Combat.DeferredAttackHitAsync` (the weapon item, read after a mid-body `Sync.LockCritterWithMap`),
`Combat.ApplyMeleeCleave` (the attacker, after `ApplyDamage` fires death events),
`AiThreatControl.ResolveAggressionMode` and `Ai.ProlongAttackEngagement`. Each of those bodies *does* call
`Sync.*` somewhere, so the body-scoped rule reports nothing — the acquisition it sees is real but does not
cover the value at the point of use, because the `await` released and reacquired in between. A value-aware
discharge (does *this* value have cover *here*) is what turns this class from a runtime crash into a
diagnostic, and it is the strongest argument for the next increment. That direction is deliberate — it under-reports rather than blocking a build on a
branch the analyzer cannot yet follow. Cover established through a *parameter* of a called method, and
entities taken from a covered collection, are not followed yet either.
