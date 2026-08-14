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
| `FOSYNC004` | Cover state is probed (`Sync.IsCovered`, `Game.IsEntityLocked`) instead of acquired. |
| `FOSYNC005` | A raw entity-cover primitive (`Game.Sync`, `Game.SyncRelease`, `Game.TrySyncEntity`) is used outside `Sync`. |
| `FOSYNC006` | A singleton bucket lock (`Game.Lock`) is not released on every path out of its scope. |
| `FOSYNC007` | A singleton bucket lock is held across an `await`. |
| `FOSYNC009` | Cover for a value is not re-proved after an await that released it. |

FOSYNC004 and FOSYNC005 come from the retired external sync-flow audit, which owned them as
`forbidden-is-covered-probe` / `forbidden-is-entity-locked-probe` and `direct-game-sync`. Neither needs
dataflow: they are about which surface a call reaches for. A probe answers what was true a moment ago, so
code branching on it either works unprotected on one path or silently skips the work on the other; and the
`Sync` helpers are not thin wrappers, they acquire multi-root packages atomically and retry with a re-proof
that nothing migrated, which reaching for the primitive directly drops.

**`Game.Lock` / `Game.Unlock` are deliberately not raw cover primitives.** They lock the `Game` singleton's
property bucket, which is a different thing from entity cover and is used in over a hundred places. They get
their own pair of rules instead, because the bucket is a plain paired resource whose two failure modes are
both hard rather than gradual:

- **Left held (FOSYNC006).** The bucket is deliberately kept outside the entity-cover set — `SyncEntities`
  replaces `_heldLocks` wholesale, while a singleton acquired through `Game.Lock()` survives every later
  `Sync::Lock` in the same job — so nothing drops it implicitly. `SyncContext`'s destructor then asserts the
  bucket is empty with `FO_STRONG_ASSERT`, which is an always-on deterministic exit.
- **Held across an `await` (FOSYNC007).** `UnlockSingleton` verifies `IsLockedByCurrentThread()` and throws
  otherwise. A continuation may resume on a different thread, so the release becomes a throw rather than a
  slow path — and whatever the `await` waits on runs with the bucket held.

Both are checked structurally rather than through the cover model, since this is ownership of one resource
rather than a claim about entities. Both gate the build: all 97 call sites are balanced and none awaits
between, so there is no backlog to phase in. The single deliberate violation is a negative test proving the
engine rejects a `Sync` acquisition while the bucket is held; the throw comes from inside `Sync.Lock`, so the
`await` cannot leave the held region without losing what the test proves, and it carries a local
`#pragma warning disable` that says so.

## Entities that carry their own cover

Baked map data and prototypes — `StaticItem`, `ProtoItem`, `ProtoCritter`, `ProtoMap`, `ProtoLocation`,
`ProtoFaction`, `ProtoModifier` — are immutable and readable at any time. An obligation for one of them is
satisfied the moment it is stated, and acquiring one succeeds trivially. Natively this is already true rather
than a convention: `ProtoEntity` derives from `Entity`, not `ServerEntity`, so it has no entity lock and the
base `ValidateAccess()` is empty.

**They remain entities, and annotations on them remain legal.** That is the load-bearing part. The obvious
alternative — excluding them from the analyzer's notion of "entity" — loses the contract on an upcast:
`ProtoCritter` derives from `Critter`, and `StaticItem` from the same abstract base as `Item`, so a value
flowing through the base type would silently stop demanding cover for the mutable half as well. The exemption
belongs to the **value**, not to the type system, and is applied wherever a value appears: as a receiver, as
an argument, and as the thing FOSYNC009 tracks across an await.

One consequence is worth stating because it reads as an inconsistency otherwise. FOSYNC003 skips an
always-covered parameter when looking for the subject the dispatcher synchronized, since it needed no
synchronizing. `MapTransfer.OnMapExit` — the sole subscriber to `OnStaticItemWalk` — takes the static item
first and the critter second, and the critter is the subject; declaring cover on the static half would state
nothing. Its annotation had in fact been on the static item, which is the defect this modelling turned up.

An earlier iteration of this rule went the other way: it excluded the types from "entity" and added a
diagnostic (`FOSYNC008`) for acquiring one. Both are gone. The exclusion was the upcast hole described above,
and the diagnostic contradicted the runtime it was supposed to describe — an acquisition aimed at
always-covered data is not a mistake to report, it is a call that succeeds.

## Cover that an await released (FOSYNC009)

The discharge rules above answer *is there cover for this value*. `FOSYNC009` answers the other half — *did
that cover survive to here* — for the one case where the answer is provably no: an await between the point
that established the cover and the point that needs it. An await releases the caller cover, the continuation
may resume on another thread, and the world moves while it waits.

It deliberately reuses `[RequiresCover]` rather than inventing a second notion of "needs cover": the
obligation is whatever the annotation already states, and the only question added is whether the cover is
still there. It also reports **only** where the ordinary discharge is satisfied — a value with no cover at all
is FOSYNC002's to report, and is not said twice. That ordering matters: the propagated case (an entry point
holding the dispatcher's cover across an await) is where the class actually lives, and the propagation
discharge returns early, so this check runs before it.

### `[PreservesCover]`

Introducing the rule surfaced a piece of the contract that was never made checkable. Some awaitable helpers
hand the caller back the cover they found; others replace it. The distinction already existed, but only as
prose in the `// SyncScope:` comments the cover attributes replaced — *"preserves the caller cover"* versus
*"lock calls inside replace or widen the caller's cover"*. 3012 of those comments are still in the embedding
project, 21 of them claiming preservation and 538 claiming replacement.

`[PreservesCover]` on a method states the preserving half, and awaiting such a method releases nothing. It
says nothing about lifetime: the entity may have been destroyed while the callee ran, so a caller that keeps
using it still owes the ordinary liveness check.

### What the rule approximates

It is not a full control flow graph. It walks source order, with two corrections that the position-only
version got wrong and that the self-tests pin:

- **Sibling branches.** Both sides of an if/else precede a later line in the text, but only one runs. An await
  counts only if it lifts to a statement in the same block as the use, starting earlier.
- **Guard branches.** `if (bad) { await Report(...); return; }` puts an await before the work in the text, and
  nothing flows out of it. The check asks the compiler (`AnalyzeControlFlow().EndPointIsReachable`) rather
  than pattern-matching the shape.

A value the await itself produced, or one declared after it, is fresh and exempt — the await held no cover for
it to lose.

What remains unmodelled is a loop whose re-proof sits at the top of the next iteration, and any path shape a
source walk cannot see. The rule under-reports there rather than guessing.

### Backlog

410 sites in the embedding project's production code and 2096 in its tests, so it ships at `suggestion`. The
count is not noise: the awaits it names most often are `Sync.Widen` (37), `Sync.Lock` (17), and `Sync.Restore`
(15) — that is, values *left out of the acquisition list*, which is exactly what the embedding project's own
guidance warns about when it says to pass required caller entities through the `strictRoots` overload. The
class has already crashed in production (`Managed entity target is destroyed`).

Each site needs its author's intent — add the value to the acquisition, re-prove it after, or mark the callee
`[PreservesCover]` when it really does hand cover back — so the backlog is per-site work, not a sweep.

**Most of it is undeclared contract, not defective code.** A large share of the backlog is one shape: a
`Task<bool>` helper that hands its own entity parameter to a `Sync` acquisition and returns the outcome. That
IS `[ProvidesCover]` — the contract exists, it just was never written down, so every caller reads as having
lost cover it still holds. Annotating those is a bounded pass over signatures rather than a per-site
investigation, and it makes the callee state what it does instead of leaving each caller to know.

**The trap when finding them mechanically is the conditional acquisition.** `AiThreatControl.ModifyThreat`
widens `{cr, owner}` only when the target is a transport, so on every other path it returns `true` having
acquired nothing; declaring `[ProvidesCover]` there would state a guarantee the method does not give and
silence real reports. Only a **top-level** acquisition — one the method cannot return `true` without running,
either as the returned value or as an `if (!await …) return false;` at the top of the body — proves the
contract. In the embedding project that separated 93 provable parameters from 38 conditional ones that need a
human. It is the same distinction FOSYNC009 itself has to make: source position is not control flow.

### An element of a covered collection is covered

`[ProvidesCover]` on a collection means the acquisition reached its elements, because that is what the
acquisition actually covered: `Map.GetCrittersInRadius(...)` is `[RequiresCover]` on the receiver and
`[return: ProvidesCover]` on the result, so the map had to be covered to call it and the map's cover reaches
the critters standing on it. Taking one out — by index or through `foreach` — therefore yields a covered
value, and a local bound to it stays covered.

This is what lets a caller hand a covered set to a helper without every element reading as a violation. The
helper still has to say it takes one: an element of an **undeclared** collection parameter is reported as
before, since nothing there states that the caller owed cover for it.

### Pass the covered entity; do not read it back out of a context

A system that threads a context object through its call graph hides the subject from the contract. The
embedding project's modifier system did exactly that: every helper took a `ModifierContext` and reached the
carrier as `ctx.Self`, so the obligation had nowhere to live even though the caller genuinely held cover.
The analyzer cannot follow a field, and neither can a reader without tracing where the context was built.

Moving the subject into the signature — `([RequiresCover] Critter self, ModifierContext ctx, …)` — fixes
both: the contract becomes checkable, and the indirection disappears. The context keeps carrying the event
payload, which is what it is for. The obligation then travels up the call chain one layer at a time until it
reaches the entry point that actually holds the cover, so expect the count to RISE for an iteration before it
falls; that is the obligation moving, not new defects appearing.

There is a natural stopping point. When the caller is a property of the context object itself, no entity
parameter exists to declare, and the chain ends there rather than being forced.

**`Game.TrySyncEntity` is deliberately not a raw primitive either.** It resolves an *id* to a live entity and
covers it, answering false when the entity is gone. Every `Sync` helper takes an entity, so none can stand in
for it: a handle retained across a yield may already be dead, which is exactly when this is the right call.
Flagging it would report code for using the only tool that fits — the first version of FOSYNC005 did, and the
one production site it hit was correct.

**Tests are the honest exception to the probe rule** — asserting that a call did or did not widen the caller
cover is exactly how the sync contract is pinned — so the embedding project lowers FOSYNC004 for its test
sources through a per-path `.editorconfig` section rather than the analyzer knowing about test folders.

Beyond that the remaining sites are few and deliberate, so both rules gate the build and each exception says
why at the site rather than in a list somewhere else. Two shapes recur: a **synchronous predicate** where
cover *is* the classification — an uncovered counterpart is judged from the covered side's persistent link and
only a covered one has its fields read — and a project's own **fixture-side cover helper**, which reaches for
the primitive by construction and probes afterwards to prove the acquisition landed.

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

## The one capability the rest of the roadmap waits on

Almost everything still missing is the same thing: a discharge that knows **which value** is covered **at this
point**, rather than "some acquisition happens somewhere in this body". That single capability is what
FOSYNC002's limitation below describes, what would have caught the destroyed-entity race family, and what the
seven still-unimplemented rules inherited from the retired external audit all need — the redundancy family
(`redundant-cover-lock`, `redundant-widen`, `redundant-snapshot-restore`, `redundant-critter-player-lock`,
`redundant-entry-lock`), `entry-cover-state-manipulation`, and `broad-world-lock`.

`redundant-entry-lock` shows the shape cheaply. An entry point re-locking a parameter its dispatcher already
covered looks redundant by inspection, and in the embedding project 75 sites do exactly that — but the ones
sampled are correct: the re-lock follows a failed `await`, where the cover really is gone. Symbol lookup
cannot separate those; path knowledge can.

One further rule from that audit is not owed at all: `helper-replaces-caller-cover` warned that a non-entry
helper *may* replace the caller's cover, which describes 856 helpers — the normal idiom, warned about because
nothing stated the effect per method. `[RequiresCover]` and `[ProvidesCover]` state it now.

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
