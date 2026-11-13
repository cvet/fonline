# Mechanics Recipes Documentation Plan

> Internal plan. It proposes a task-shaped "mechanics recipes" section for the Engine documentation, derived from two read-only audits of how Last Frontier (Managed C#) and TLA (AngelScript) implement gameplay on the Engine, and a third audit of what the Engine documentation and examples already teach. The first slice is implemented; later waves are not.

## Status

- State: first slice implemented; waves 1 (remaining recipes) and 2 open
- Owner: documentation program
- Last updated: 2026-11-13
- Engine baseline: branch `lifecycle-docs-and-tooling` on `origin/master` `7d134df5f8` (`2026.1.118-dev`)
- Source audits: 35 Last Frontier mechanics, 36 TLA mechanics plus 8 AngelScript idioms, and the Engine coverage audit (98 API symbol IDs checked against `Docs/generated/api.json`)

## Goal

A game team that has finished the first tutorials can learn how to build a real mechanic with the Engine primitives (validated actions, timed effects that survive a restart, equipment slots, containers, trade, quests, NPC routines, map transitions, death and respawn) without opening another game's repository. Each page teaches Engine usage and its traps, shows a runnable minimal example, and explains how a larger project extends it in AngelScript and in Managed C#.

## What the audits found

1. **The Engine teaches setup and internals, not mechanics.** The tutorials end at one remote call that increments one persistent `int32` (`Examples/MinimalMultiplayer/Scripts/Tutorial.fos`, 262 lines at most in any example). Reference pages explain the cover model, remote-call security and persistence, but no page answers "how do I build X". No Engine example proves behavior across a database restart; every example uses an in-memory database.
2. **Every API symbol is `experimental`** (2,615 of 2,616; one is `internal`). A "stable symbols only" filter is impossible, so recipes use a proxy: the symbol exists, is described, is fired or exercised by Engine code or tests, and is checked against `api.json` in CI.
3. **Some declared Engine events are never fired.** The API descriptions say so for `Critter.OnTalk`, `Critter.OnBarter`, `Map.OnCheckLook`, `Map.OnCheckTrapLook`, `Player.OnLogout` (only `Game.OnPlayerLogout` fires), `Player.OnAllowCommand` and `Player.OnGetAccess`. Dialog, barter and perception recipes must say so and show the project-side pattern instead.
4. **Time events do not survive an ordinary database reload.** The load path does not restore them; destroy and unload paths cancel them; only server snapshots save them. A timed effect therefore needs a persisted deadline that is re-armed on load behind a `CountTimeEvent == 0` check. No Engine page states this consequence for gameplay. Both projects contain code that gets it wrong (see "Project findings").
5. **Several families have no Engine primitive**: dialog runtime, quests, trade flow, stats and skills, combat resolution, factions, AI decisions, crafting and equipment slots beyond `Inventory/Main/Outside`. Recipes for them are labelled "pattern recipes" and teach a project-side structure built from Engine primitives.
6. **The same subsystems are rebuilt by both projects**: the native dialog system (`Dialogs.cpp` plus `DialogBaker.cpp`, about 1,200 and 1,500 lines), item stacking (the Engine removed it; each project rebuilt it), a visibility hook, a project event vocabulary (77 and about 120 events), and a GUI library. These are the strongest signals for recipes and for possible Engine features.

## Proposed structure

- **Layout:** `Docs/en/how-to/mechanics/` with a hub page and one page per recipe, an RU mirror, and a navigation group "Build game mechanics" after "Script games".
- **Page template** (fixed H2 order, validated by a test): Problem; Engine primitives used (with symbol IDs); Minimal example (file list and verbatim slices); Step by step; Pitfalls and anti-patterns; Authority and cover notes; Persistence and migration notes; Test route; AngelScript version; Managed C# version; How a larger project extends it. Front matter records `recipe_kind` (`primitive` or `pattern`) and a status badge per backend (`shape`, `compile-verified`, `runtime-verified`).
- **Example strategy:** one growing `Examples/MechanicsSandbox`, kept in the Engine repository like `Examples/GameplayTestHarness` and promoted to a public example repository later. One sandbox costs one Engine build; per-recipe examples would cost one each, and snippets inside `MinimalMultiplayer` would break its tutorial tags. Each recipe owns a script module and a content fragment; the copyable test harness selects it with `Testing.Filter`; restart scenarios run in two phases against a JSON-backed database. Estimated cost: three to five days to bootstrap, then half a day to a day and a half per recipe.
- **Backends:** every page shows both. AngelScript is runtime-verified by the in-process unit-test harness. Managed C# starts as `shape` or `compile-verified` and becomes `runtime-verified` only when a managed sandbox preset runs in a CI lane; a parity test keeps declarations identical.
- **Provenance:** recipes are derived from the projects but Engine-owned: no project paths in the pages, no copied code, no game policy (balance numbers, lore, content). Derivation is recorded in `BuildTools/ExternalProjectEvidence.json` as one record per recipe family so promotion gates stay visible.
- **Anti-rot gates:** fenced code is a verbatim slice of sandbox files; page structure and front matter match a recipe manifest; every cited symbol exists in `api.json`; a recipe may not subscribe to a never-fired event unless it fires it itself; a backend parity check; the in-process unit route as a required test (a test that reads `Examples/`, which no test does today) and an opt-in full smoke.
- **Localization and evaluation:** about twelve gameplay terms for the glossary; AI-evaluation tasks fit the existing categories.

## Recipe waves

### Wave 1: primitive recipes (verifiable against Engine behavior today)

| # | Recipe | Draws on | Main teaching |
|---|---|---|---|
| 1 | Custom persistent character state | both | `Mutable Persistent` plus sync scope, init and load events, restart proof |
| 2 | Validated action request | LF (17 remote calls), TLA | the server re-derives everything from the controlled critter, payload ids are never trusted, costs are charged after the last check that can refuse, forged input throws, `MaxBytes` limits |
| 3 | Data-driven item behavior | both | property components as item kinds, protos, `InitScript` |
| 4 | Equip into a slot with a shared rule | both | three Engine slots, finer slots as project data, the move veto must run on the server too |
| 5 | Timed effect that survives restart | both | persisted deadline, re-arm on load, `CountTimeEvent` guard, typed timer data |
| 6 | Derived stats | both | virtual property getters, transforming versus post setters, replication limits |
| 7 | Container put and take | both | cover both owners, stack handling, `SendItems` and `OnReceiveItems` |
| 8 | Map transition trigger | both | trigger items, `TransferToMap`, exit rules, locks |
| 9 | NPC patrol and awareness | both | `MoveToHex`, `MovingContext`, stop events, appeared and disappeared events |
| 10 | Interruptible long action | LF | cancellable timed work with a shared registry |

### Wave 2: pattern recipes (project-side structures on Engine primitives)

Project event vocabulary and dispatcher with a check that every declared event has a producer; two-party trade; conversation without an Engine dialog system; quest stage and journal; death, knockout and respawn; status effects with decay and staged modifiers; attack with deferred hit (timers carry only ids and re-resolve the target); item stacking; server-driven prompts with session tokens; safe admin commands with access checks; login to a controllable character; world clock and day and night; custom entity types with persistent holders; data catalogs from FixedType rows.

### Cross-cutting pages

1. How to lock several entities for one gameplay action: a worked cover transaction with widening and re-validation after a suspension. Last Frontier calls the cover helpers more than 2,800 times and no Engine page shows a transaction.
2. Restoring state idempotently on load.
3. Splitting one fact into server, public and owner-only properties.
4. Declaring project events with veto chains.
5. The same rule compiled for client and server.
6. AngelScript idioms for a new project: module layout and `Sort` ordering, side macros, generated content constants, attribute dispatch, mutable-globals policy, `EventResult` polarity.

## Engine gaps the audits surfaced (candidates for Engine work, not documentation)

- Never-fired declared events listed above: fire them or remove them.
- A start-if-not-running timer call, or timers that persist; typed timer payloads instead of text parsed by hand.
- Safe continuation after the owning entity is destroyed (today a project matches an exception message).
- Script-declared enum values visible to native code (a project casts an integer).
- A reference visibility hook and an item stacking helper (both projects write their own).
- Remote calls with replies, replication of computed properties, access checks for admin calls, death and respawn events.
- A server HTTP client and client secure storage (both written natively in the projects).
- A generator for the id-constants module that AngelScript projects use (TLA's is a 10,000-line file without an emitter).

## Project findings (reported to the projects, not Engine work)

- **Last Frontier:** the radiation and poison decay timers start only inside the property setter and are not re-armed on load, so a saved level probably stops decaying after a server restart; several owners start timers without the `CountTimeEvent` guard (`Health`, `Stamina`, `Lockers`, `StatusBar`). Found by reading code, not reproduced.
- **TLA:** 24 declared events have subscribers but no `.Fire` call (for example `Critter.OnAttacked` with 30 subscriptions), which makes `Main::CritterAttack` dead; the drug modifiers are saved but the timers that remove them are not, so effects never end after a restart (140 `SavedDeferredCall` markers show the same loss); the equipment slot rule is enforced only on the client, so a forged remote call can place any item in the armor slot; two server calls lack access checks, `ConsoleCommands.AllowEveryone=True` ships in the config and none of the 48 server remote calls declares `MaxBytes` or `MaxCollectionSize`; `Critter.Wait`, `IsBusy` and `IsFree` are native stubs.

## Decisions needed from the owner

1. Approve `Examples/MechanicsSandbox` as an Engine fixture and `how-to/mechanics/` as the section.
2. Approve a required unit test that loads recipe scripts from `Examples/`.
3. Decide whether to fund a managed C# CI lane; without it, C# stays `shape` or `compile-verified`.
4. Decide whether recipes may state the derivation in `ExternalProjectEvidence.json` (the pages themselves never name a project).
5. Decide which Engine gaps above to fix first; the never-fired events and the time-event consequence need a statement in `lifecycle-and-concurrency.md` regardless.

## Suggested first slice

The hub, the page template with its validating test, the sandbox with recipes 1, 2 and 5 (persistent state, validated action request, timed effect that survives restart), and the cover-transaction page. This slice exercises the whole pipeline, repairs the most consequential gap (restart behavior), and costs roughly the bootstrap estimate plus three recipes.

## Decisions taken

1. Approved: `Examples/MechanicsSandbox` is an Engine fixture and `how-to/mechanics/` is the section.
2. Approved: `BuildTools/tests/test_mechanics_recipes.py` and `test_mechanics_sandbox.py` run in the tooling regression job and read `Examples/`. The runtime route (`validate.py`, a native build) stays on demand.
3. No managed C# CI lane for now. Managed recipes stay `shape`; parity tests keep their declarations identical to the AngelScript modules. Revisit when a managed example preset exists.
4. Recipe derivation is recorded in `BuildTools/ExternalProjectEvidence.json` as one `promoted` record per recipe; the pages never name a project.
5. Engine gaps fixed first are the defects the sandbox exposed: `Game.UnloadCritter` deleted the record of an NPC persistent only through its map, `Game.LoginPlayerToNewRecord` rejected a player with only default properties, and the generated example configurations wrote enum settings as enumerators and left the secure-channel key empty. The never-fired events and the time-event consequence are stated in `lifecycle-and-concurrency.md`; firing or removing those events, persistent timers and the other gaps above remain open.

## First slice results

- Pages: hub, `persistent-state`, `validated-action`, `timed-effect` and `cover-transactions`, each with a Russian mirror, in the manifest, navigation, AI evaluation and changelog.
- Sandbox: eight smoke scenarios, including two-phase restarts on a JSON database and a validated-action drill through a real headless client, passed on a Windows host; the snapshot restore path of time events is not runtime-tested.
- Defects found and fixed: the widening sentence of the Managed C# guide and the `Game.SyncWiden` description (widening may release and reacquire the held cover), the two runtime defects above, an exception-safety defect in `ItemHexView::SetAnimPhase`, and the example configuration defects.
