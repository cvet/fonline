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

Three smaller markers describe how a value's cover moves rather than who owes it:

| Attribute | Where | Meaning |
|-----------|-------|---------|
| `[PreservesCover]` | method | Awaiting it gives the caller back the cover it had (see FOSYNC009). |
| `[AcquiresCover]` | method | The helper acquires cover through `Sync` for entities it names itself -- a global-map group's members, the carrier and map a radio resolves to -- and answers whether it succeeded, possibly with a record of what it covered. A body calling it counts as acquiring, exactly as one calling `Sync` directly. Where the helper covers an entity it returns or takes, `[ProvidesCover]` is the more precise statement and wins. |
| `[PassesCover]` | parameter | The method returns this argument unchanged (`Game.VerifyNotNull`), so the result is covered exactly when the argument was, with the same reach. |
| `[CoverEffect(kind)]` | method | What the call does to the held cover: `Replace`, `Extend`, `Restore`, `Snapshot` or `Release`. It is what the `Sync` surface declares about itself, and every rule below reads the effect from there. |

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
| `FOSYNC002` | An argument for a `[RequiresCover]` parameter that is neither covered by the caller, received from a `[ProvidesCover]` source, nor re-declared. A provided value is one returned by a provider, handed to a `[ProvidesCover]` parameter earlier in the body (with that parameter's reach), passed through a `[PassesCover]` parameter, taken out of a provided collection, deconstructed from a provided tuple, or chosen by `?:` between provided values and `null`. A body calling `Sync` or an `[AcquiresCover]` helper discharges it. Nothing is owed for a `null` or `default` argument or an omitted optional parameter, nor in a compilation whose `Sync` has no acquisition helpers (a client or mapper target, whose scripts run on one thread). |
| `FOSYNC003` | An execution-context entry point that does not declare `[RequiresCover]` on the entity the engine already synchronized for it. |
| `FOSYNC004` | Cover state is probed (`Sync.IsCovered`, `Game.IsEntityLocked`) instead of acquired. |
| `FOSYNC005` | A raw synchronization primitive is used outside its wrapper: `Game.Sync` / `Game.SyncWiden` / `Game.SyncRelease` outside `Sync`, `Game.Lock` / `Game.Unlock` outside `GameLock`. |
| `FOSYNC009` | Cover for a value is not re-proved after an await that released it. |
| `FOSYNC010` | The boolean answer of a cover acquisition is discarded -- the call is a whole statement, or assigned to `_` -- instead of read. |
| `FOSYNC011` | A `Sync` helper changes the held cover -- through the primitive or through another helper whose declared effect changes it -- without declaring a `[CoverEffect]` of its own. |

FOSYNC004 and the entity half of FOSYNC005 come from the retired external sync-flow audit, which owned them as
`forbidden-is-covered-probe` / `forbidden-is-entity-locked-probe` and `direct-game-sync`. Neither needs
dataflow: they are about which surface a call reaches for. A probe answers what was true a moment ago, so
code branching on it either works unprotected on one path or silently skips the work on the other; and the
`Sync` helpers are not thin wrappers, they acquire multi-root packages atomically and retry with a re-proof
that nothing migrated, which reaching for the primitive directly drops.

**Which methods those are is declared at the export, not listed in the analyzer.** A C++ script export marks
itself `FO_COVER_PRIMITIVE` (`Game.Sync`, `Game.SyncWiden`, `Game.SyncRelease`), `FO_COVER_PROBE`
(`Game.IsEntityLocked`) or `FO_SINGLETON_LOCK` (`Game.Lock`, `Game.Unlock`); codegen carries the marker through
`MethodDesc`, and the managed baker emits `[CoverPrimitive]`, `[CoverProbe]` or `[SingletonLock]` on the
generated method. The model's own probe, `Sync.IsCovered`, declares `[CoverProbe]` in C# beside it. Until
2026-09-16 the analyzer held these as three arrays of method names, which meant a rename in the engine would have
disarmed both rules in silence — and unlike the analyzer's own rules, nothing downstream would have reported that
silence either. Two self-test cases pin the property from both sides: renaming the export keeps the rule firing,
and a method that merely wears the old name is ordinary code. `Game.TrySyncEntity` deliberately carries no
marker: it resolves an *id* to a live entity and answers false when the entity is gone, and no `Sync` helper can
stand in for it, because every one of them takes an entity that may already be dead.

### The singleton bucket lock: `GameLock`

`Game.Lock` / `Game.Unlock` lock the `Game` singleton's property bucket, which is a different thing from entity
cover. Scripts take it through one scope and nothing else:

```csharp
using GameLock scope = GameLock.Acquire();
Dictionary<int, int> counters = Game.Counters;

if (!counters.TryGetValue(key, out int current)) {
    return 0;
}

counters[key] = current + 1;
Game.Counters = counters;
return current + 1;
```

`GameLock` (`Source/Scripting/Managed/CoreScripts/GameLock.cs`, server only) is a `ref struct` whose `Dispose`
releases the lock, and each half of that shape answers one of the bucket's two failure modes, both of which are
hard rather than gradual:

- **Left held.** The bucket is kept outside the entity-cover set — `SyncEntities` replaces `_heldLocks`
  wholesale, while a singleton acquisition survives every later `Sync::Lock` in the same job — so nothing
  drops it implicitly until the job ends and `SyncContext::Release` drains it. Until then every other thread
  waits for the bucket, and the next `Sync` acquisition in the job throws because the singleton is held, which
  reads as a different defect than the one that caused it. A `using` scope releases on every path out of its
  block, an exception included; the paired calls released only where the author remembered to, and a region
  that anything inside could throw from was left held unless it carried a hand-written `try/finally`.
- **Held across an `await`.** `UnlockSingleton` verifies `IsLockedByCurrentThread()` and throws otherwise. A
  continuation may resume on a different thread, so the release becomes a throw rather than a slow path — and
  whatever the `await` waits on runs with the bucket held. A `ref struct` local cannot survive an `await` or a
  `yield`, so the compiler reports `CS4007` for any scope that would, in nested blocks and iterators too.

That is why the scope replaced the two structural rules that used to guard the pair. `FOSYNC006` (lock left
held) walked the statements after `Lock` until the first one that contained *any* `Unlock`, so a region whose
first early exit released could lose its final release unreported, and it did not see exceptions at all;
`FOSYNC007` (lock held across an await) looked only at the same block. Both are retired, and FOSYNC005 reserves
the raw pair for `GameLock` itself, so a paired call cannot come back. A copied scope disposed twice, or a
default one, releases a lock it did not take; with nothing left held the engine throws, so that mistake is loud.

The one legitimate caller of the raw pair outside `GameLock` is a test of the primitive itself — recursion, an
unbalanced release, `SyncRelease` draining the bucket, a `Sync` acquisition rejected while the bucket is held,
whose `await` has to sit inside the held region because the throw it proves comes from inside `Sync.Lock`.
Such a test carries a local `#pragma warning disable FOSYNC005` that says so.

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

## An acquisition answers, and the answer is the point (FOSYNC010)

An acquisition can legitimately fail: the entity may have been destroyed or migrated while the caller
waited. The boolean is that answer, so discarding it walks on over a value whose cover was never
established -- the very state the re-proof was written to prevent, now invisible because nothing branches
on it. FOSYNC009 cannot see this: its discharge asks whether an acquisition *names* the value, not whether
anything reads the result, so an ignored re-proof satisfies it.

Sometimes there is genuinely nothing to decide -- cover handed back on the way out of a cover-neutral
helper, a subject the code has already proved gone. That is what the **best-effort forms** are for:
`Sync.WidenBestEffort`, `Sync.RestoreBestEffort`, `Sync.WidenCritterWithMapBestEffort` and
`Sync.RestoreCallerCover` return `Task`, not `Task<bool>`, so they answer nothing and say so in the call
itself. The decision to drop the answer then lives in one named place instead of at each call site, where
it is indistinguishable from an oversight.

**Two different things wear that name, and the difference is worth knowing.** `WidenBestEffort` differs from
strict `Widen` in the *work*: `Widen` refuses as a whole the moment any requested entity is gone and leaves
the held set untouched, while the best-effort widen skips the dead and keeps the live remainder -- one
cannot be written as the other plus a discarded answer. `RestoreBestEffort` and
`WidenCritterWithMapBestEffort` differ in the *answer* alone: `Restore` is already partial by construction
(it locks the survivors, and its bool reports only whether the input was complete), so these two are the
strict call with its completeness answer deliberately unread. They are kept as names of their own for
readability (owner decision 2026-09-16): at the call site a name states that the answer was weighed and
found to decide nothing, which `_ = await ...` states only to a reader who already knows the helper. The
rule treats both kinds alike, because what it checks is whether the call promises an answer at all.

The backlog the rule opened with was 166 sites, and all 166 were that shape: a snapshot put back before a
`return`, a `break`, a `continue`, inside a `finally`, or as the last statement of a method, plus two
cover-neutral helpers handing the critter and its map back at the end. Not one of them was an acquisition
guarding work that followed -- those already read their answer, which is why the rule found none.

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
*"lock calls inside replace or widen the caller's cover"*. The embedding project has since deleted all 3702
of them: 31 stated a cover requirement the signature did not declare and were moved onto the declaration,
and the rest either restated an annotation already there or described the body. 61 claimed preservation
with nothing declared, and those are the backlog `[PreservesCover]` exists for -- a claim can only be
turned into an annotation by proving the acquisition is top-level, which is per-site work.

`[PreservesCover]` on a method states the preserving half, and awaiting such a method releases nothing. It
says nothing about lifetime: the entity may have been destroyed while the callee ran, so a caller that keeps
using it still owes the ordinary liveness check.

### What the rule proves instead of asking

Both halves of that contract are usually visible in the callee's own body, and the analyzer has the body: the
scripts and the `Sync` helpers compile into one compilation. So it proves what the annotation would have
asserted, and the annotation stays for what no body here shows — a helper that locks and then restores the
caller's snapshot, and anything compiled elsewhere.

The proofs rest on what the acquisition families actually do, and each one says so on its own declaration rather
than in its name. `Sync.Lock` hands the native primitive the listed entities and it **replaces** the held set
with exactly those — `[CoverEffect(CoverEffectKind.Replace)]`. `Sync.Widen` asks the native widen to add the
extras to the held set, so it **keeps** what it found — `Extend`; that keeping happens natively, where its body
cannot show it, so it also states `[PreservesCover]`. `Sync.Restore(snapshot)` puts back exactly the snapshot —
`Restore`; `Sync.Snapshot` reports it without changing anything; `Sync.Release` drops it. Nothing in the analysis
recognises `Widen`, `Lock` or `Restore` as words: a helper renamed keeps its meaning, a helper added without the
attribute has none, and a project that spells its acquisitions differently is read the same way. That last
property cuts both ways, which is what `FOSYNC011` is for: a helper added to `Sync` without the attribute is not
merely undeclared, it is **invisible** -- no rule objects, the build stays green, and cover silently stops being
tracked through it. So a body that reaches the primitive, or another helper whose declared effect changes the
held set, must declare its own. A helper that only answers a question -- a membership comparison, a probe, a
collector -- changes nothing and declares nothing, which is why the rule asks what the body reaches rather than
where it is declared. From the effects:

- **Preserving is provable.** A body whose every await widens — directly, through `Sync.Restore` of a snapshot
  it took itself, or through another method the same proof covers — cannot take the caller's cover away. A body
  that never awaits proves nothing (awaiting at all is what the rule counts as the loss) and one that calls
  `Sync.Release` is out.
- **Providing is provable.** Handing a value to a parameter leaves it covered when the callee's own acquisition
  names that parameter, runs on every path that returns past it, and nothing the body awaits afterwards takes
  the cover away again. "Names it" includes the list idiom — `List<Entity> roots = new List<Entity> { cr, map };
  await Sync.Widen(roots);` names both. "Runs on every path" is a statement of the body itself, or the
  condition of a guard whose branch never falls through (`if (!await Sync.Lock(cr)) { return false; }`); an
  acquisition inside a loop, an `else` or a switch section proves nothing, which is the conditional-acquisition
  trap below, resolved by refusing to guess.
- **A re-proof may be a restore.** At the use site, the re-proof is an acquisition naming the value — through a
  list of roots as well — or a `Sync.Restore` of a snapshot taken **before** the await that lost the cover: the
  snapshot is the cover, so it names the value without mentioning it.

Two more shapes are proved the same way. A body that takes its own snapshot, runs re-entrant work and puts the
snapshot back **in the very next statement** loses nothing across the whole body — that is the hand-written form
of preservation, and it is what a helper does when it knows its callee replaces the cover. And a local the body
**re-reads after the await** — `map = cr.GetMap();` once the attacker has been locked again — is as fresh as one
declared there: what the use sees is what that assignment put there. The re-read has to sit in the use's own
block, since one inside a branch leaves the stale value on the other path.

Cycles answer "no" on the recursive edge, and a "no" reached that way is not cached, so the result does not
depend on which method the walk started from. Every step is conservative in one direction: an await the
analyzer cannot resolve, or a body outside this compilation, reports rather than hides.

In the embedding project these proofs took the backlog from 3 933 sites to 779 without a line of game code
changing.

### What the rule approximates

It is not a full control flow graph. It walks source order, with two corrections that the position-only
version got wrong and that the self-tests pin:

- **Sibling branches.** Both sides of an if/else precede a later line in the text, but only one runs. An await
  counts only if it lifts to a statement in the same block as the use, starting earlier.
- **Guard branches.** `if (bad) { await Report(...); return; }` puts an await before the work in the text, and
  nothing flows out of it. The check asks the compiler (`AnalyzeControlFlow().EndPointIsReachable`) rather
  than pattern-matching the shape.

A value the await itself produced, or one declared after it, is fresh and exempt — the await held no cover for
it to lose. That includes both deconstruction spellings, `(T x, bool y) = await ...` and
`var (x, y) = await ...`: their designations end before the await in the text, so the assignment they sit in
is what decides, not the designation's own position.

What remains unmodelled is a loop whose re-proof sits at the top of the next iteration, and any path shape a
source walk cannot see. The rule under-reports there rather than guessing.

### Closing the backlog

The embedding project took it from 3 933 sites to zero and gates the rule as an error. The proofs above did
four fifths of it; what was left was the honest remainder, where the callee really does hand the cover to
something else. The awaits it named most often are the subject-taking helpers a whole layer is built on --
firing a modifier event, killing a critter, granting an ability, reporting a quest objective -- each of which
re-validates its own subject internally and promises the caller nothing.

The remainder split by what the caller is:

- **A test re-proves and asserts in the same breath.** The fixture has to survive for the rest of the test to
  mean anything, so a one-line `Testing.KeepCovered(cr)` — whose body widens, then `Invariant.Verify`s the
  answer — states that and fails loudly when it does not. The analyzer proves the helper's own contract from that
  body, so no annotation is needed anywhere. Two shapes were worth doing at the helper instead of the call site:
  a thin test wrapper that forwards to production work re-proves once at its end and closes every call site it
  has (nine of them closed 305 sites).
- **Production acts on the answer.** `if (!await Sync.Widen(x)) { return; }` after the call that could have
  taken the cover away — the acquisition's failure means the entity is gone, which is exactly the case the old
  code walked into. A scene fixture is the exception that verifies instead: a dev stand with a dead fixture has
  nothing left to show.

Neither pass is a sweep over the text: the insertion point is the statement that lost the cover, found through
the syntax tree, and every site whose await sits inside a condition or an argument was read by hand.

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

Provider inference checks that a candidate runs on every returning path before inspecting its callees.
Calls in conditional branches cannot prove the contract, and traversing their cyclic dependencies first can
repeat an exponential amount of work. The analyzer self-tests include a dense conditional call cycle with
a time limit and still require FOSYNC009 for the uncovered use after an await.

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
why at the site rather than in a list somewhere else. The shape that holds up is a project's own **fixture-side
cover helper**, which reaches for the primitive by construction and probes afterwards to prove the acquisition
landed. A **synchronous predicate that branches on the probe** — reading a counterpart's fields only when it
happens to be covered, and trusting the covered side's own link otherwise — does not: the same data then gets a
different answer depending on what the caller holds. When a predicate cannot acquire, judge only from the
entities it is guaranteed to cover.

Every rule of the family now gates the build as an error, and none carries a backlog: a cover annotation on a
non-entity can never be satisfied, every entry point is annotated, every obligation is discharged (see
[Closing the FOSYNC002 backlog](#closing-the-fosync002-backlog)) and every value is re-proved after the await
that released it (see [Closing the backlog](#closing-the-backlog)). Severities come from the embedding project's
`.editorconfig`. Note that the generated managed project sets `TreatWarningsAsErrors`, so promoting a rule to
`warning` makes it a hard build failure — roll out by severity, not all at once.

## Where it lives

- Analyzer: `Source/Scripting/Managed/Analyzers/` (`netstandard2.0`, which is how the compiler loads
  analyzers). The `Microsoft.CodeAnalysis.CSharp` version must stay **below** the Roslyn in the SDK running
  the build: an analyzer may be older than its host compiler, never newer.
- Rule changelog: `AnalyzerReleases.{Shipped,Unshipped}.md` — required by Roslyn's own RS2008.
- Self-tests: `Source/Scripting/Managed/Analyzers/Tests/`, a plain console runner (compile a snippet, assert
  the reported ids). `dotnet run` exits 0 when every case passes.
- Wiring: the embedding project points `ManagedScript.Analyzers` at the analyzer project;
  `ManagedScriptBaker` emits it as `<ProjectReference OutputItemType="Analyzer"
  ReferenceOutputAssembly="false" GlobalPropertiesToRemove="OutputPath;Configuration;Platform" />`. The
  `GlobalPropertiesToRemove` is load-bearing: without it the analyzer inherits the script project's
  `OutputPath` and lands in the baked script assembly directory, where the baker would pick it up as a
  runtime script assembly.
- The setting is a **list**, so this analyzer is not the only one an embedding project can carry: a project
  with rules of its own adds a second entry rather than growing this one, which keeps game-specific
  diagnostics out of the reusable engine. The rest of the analysis profile — rule-set version and mode,
  packaged analyzers, analyzer configuration files — is described in
  [Managed C# Scripting](en/how-to/scripting/managed-csharp.md#configure-the-backend). Editing an analyzer
  project re-triggers the managed bake, so a new rule takes effect on the next build rather than waiting for
  an unrelated source file to change.

## Next: the engine's own exports

The contracts that matter most are the native ones — a script's cover obligation almost always exists
because some `FO_SCRIPT_API` call reads or mutates an entity. Those were recorded in the external audit's
`contracts/native_exports.json` in the embedding project -- retired with the audit, so the counts below are
the measurement that decided the design rather than a file to go and read: **88 entries, 74 with a
`requires_access`**, in 16 distinct shapes.

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
explicit synchronization primitive `Game.Sync` exists precisely to provide it. A single
positional marker could not say the second thing, because in parameter position it would already mean the
first. `FO_PROVIDES_COVER` is wired for the return position today, which is the case that exists; the
parameter position is the natural extension when those primitives get declared.

`FO_PROVIDES_COVER` marks a **downward accessor** — one whose returned entities live under its receiver, so
the receiver's cover already covers them, and the value discharges the obligation at the next call that takes
it. The direction has to be declared rather than inferred: `Map.GetCritters` provides cover for what it
returns, while `Critter.GetMap` returns a *parent* and provides nothing (acquisition takes the requested
entities plus their sync-widen partners and nothing else). 44 accessors on `Map`, `Location`, `Critter` and
`Item` are declared.

The upward accessors carry the other half of the direction. `FO_RETURNS_PARENT` marks an export returning the
receiver's immediate sync-hierarchy parent (`Critter.GetMap`, `Map.GetLocation`), and `FO_RETURNS_ANCESTOR` one
whose result may sit further up the chain (`Item.GetMap`, `Item.GetMapPosition` and `Item.GetCritter`, which walk
out through any number of containers). Codegen carries them as `MethodDesc::ReturnIsParent` /
`ReturnIsAncestor`, and the baker emits `[return: ReturnsParent]` / `[return: ReturnsAncestor]`. Neither provides
cover by itself -- the receiver's own cover does not reach up -- but a receiver that arrived with declared reach
does: a value from a `[ProvidesCover(CoverReach.Parent)]` source covers its `[ReturnsParent]` result, and one with
`CoverReach.Ancestors` covers every upward accessor on the chain (`cr.GetMap().GetLocation()`). One step of
`Parent` is spent getting the parent, so the parent's own parent is not covered by it. This is what makes the reach
a provider already declares worth declaring: `Testing.SpawnNpc` has said `CoverReach.Parent` since it was written,
and until the accessors stated their direction nothing could use it.

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

### Closing the FOSYNC002 backlog

Measured again before the work started, the backlog was 1 943 sites (320 production, 1 623 tests), and the
"mechanical pass" the paragraph above promises closes little of it: only about 10 % of the bare identifiers are
parameters of the enclosing method. The rest are locals obtained on the spot. The embedding project took it to
zero without a single suppression, in this order:

1. **Nothing owed.** A `null` / `default` / omitted argument, and a compilation whose `Sync` has no acquisition
   helpers (client and mapper), stopped reporting.
2. **Providers that code proves.** `[return: ProvidesCover]` on helpers that create the entity or return it only
   after a top-level acquisition, and on helpers whose result is a descendant of a `[RequiresCover]` parameter
   (a container found on a covered map). The gameplay-test runner locks every context slot and tracked entity
   with their maps and locations before each callback, so the context accessors state that.
3. **Reach.** `FO_RETURNS_PARENT` / `FO_RETURNS_ANCESTOR` let a provider's declared reach cover an upward accessor.
4. **Parameters to a fixed point.** `[RequiresCover]` where the obligation belongs to the caller, iterated until
   nothing new rose.
5. **The remainder per site.** Five more analyzer precision steps -- reach handed over through a
   `[ProvidesCover]` parameter, tuple deconstruction, `?:` choices, `[PassesCover]`, `[AcquiresCover]` -- and a
   real `Sync` acquisition wherever the cover was genuinely unproved (a quest owner resolved from a companion
   link, a modifier context's target, the participants of a caravan record).

Proving cover where there was none raised FOSYNC009 from 3 554 to 3 932: a value FOSYNC002 used to report as
uncovered is now covered, and where an await releases it before use FOSYNC009 says so instead. That is the
same defect class moving to the rule that names it, not new debt — and that backlog was itself closed the next
day (see [Closing the backlog](#closing-the-backlog)).

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
branch the analyzer cannot yet follow. Cover handed over through a `[ProvidesCover]` parameter is followed with the
same body scope: the call may sit anywhere in the body. Elements taken from a `[ProvidesCover]` collection are
tracked as described above; neither makes acquisition path-sensitive.
