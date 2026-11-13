---
layout: default
title: Build Game Mechanics
locale: en
document_id: mechanics-recipes
permalink: /Docs/en/how-to/mechanics/
---

# Build Game Mechanics

This section continues where the tutorials stop. Each recipe builds one gameplay mechanic from Engine primitives and states which side owns the state, what survives a restart, which traps the Engine has and how the result is tested. Every recipe is backed by a runnable module in `Examples/MechanicsSandbox`, and `BuildTools/mechanics_recipes.py` checks that its page still quotes that module and cites API symbols that exist.

## Source paths inspected

- `BuildTools/mechanics_recipes.py`
- `BuildTools/tests/test_mechanics_recipes.py`
- `BuildTools/gameplay_test_runner.py`
- `Docs/generated/api.json`
- `Examples/MechanicsSandbox/recipes.json`
- `Examples/GameplayTestHarness/ScriptTests/AngelScript/Testing.fos`
- `Examples/MinimalMultiplayer/tutorial-smoke.json`
- `.github/workflows/validate.yml`

## What a recipe is

A recipe is a task-shaped page: "make a buff expire correctly after a restart", not "the time-event API". It teaches Engine usage and its traps. Game policy such as balance values, content and lore stays with the game project; a recipe names where that policy plugs in.

Each recipe has four parts that change together:

| Part | Location |
| --- | --- |
| Page and Russian mirror | `Docs/en/how-to/mechanics/<slug>.md`, `Docs/ru/how-to/mechanics/<slug>.md` |
| Script module | `Examples/MechanicsSandbox/Scripts/Recipes/<PascalSlug>.fos`, namespace `<PascalSlug>` |
| Tests and smoke scenarios | `Examples/MechanicsSandbox/Scripts/Tests/Test_<PascalSlug>.fos`, scenarios in `Examples/MechanicsSandbox/mechanics-smoke.json` |
| Manifest entry | `Examples/MechanicsSandbox/recipes.json`: kind, backend statuses, files, symbols, scenarios |

A recipe module never references another recipe; shared login and world code lives in the sandbox bootstrap.

How recipes relate to the rest of the documentation:

- The [tutorials](../../tutorials/first-client.md) end with one remote call that changes one persistent property, and [the first test](../../tutorials/first-test.md) proves it without restart persistence. Recipes start from that point.
- [Choose How to Represent Game Data](../../explanation/entity-and-property-model/choosing-a-representation.md) decides where data lives: a setting, a FixedType catalog, a component, a RefType, a ValueType or an entity. A recipe applies one of those choices to a mechanic and links back instead of repeating the decision table.
- Contract pages such as [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md) and [Remote Calls](../../reference/scripting/remote-calls.md) remain the authority. A recipe shows one correct use of a contract and links to its owner.

## Recipe kinds

The front matter field `recipe_kind` has one of two values:

| Kind | Meaning |
| --- | --- |
| `primitive` | The Engine provides the core primitive, for example persistent properties, time events or validated remote calls. The recipe shows its correct use and its traps. |
| `pattern` | The Engine has no primitive for the mechanic. The recipe teaches a project-side structure built from Engine primitives and says that the policy belongs to the project. |

Mechanics without an Engine primitive include dialog runtimes, quests and journals, trade flows, stats and skills, combat resolution, factions, AI decisions, crafting, and equipment slots beyond the three `CritterItemSlot` values `Inventory`, `Main` and `Outside`. Some of them have declared Engine events that the Engine never fires; see [Declared events the Engine never fires](../scripting/lifecycle-and-concurrency.md#declared-events-the-engine-never-fires).

## Status badges

A recipe shows one status for each backend. The value comes from `backends` in `recipes.json`, and the gate fails when a page or this index shows a different one.

| Status | What it proves |
| --- | --- |
| `runtime-verified` | The recipe's sandbox scenarios ran against an Engine build and passed the runner contract: the required markers such as `[Testing] COMPLETE status=passed`, no forbidden marker and the expected exit code. This proves Engine dispatch at the revision where the scenarios last ran. Engine CI does not run example runtime routes, and `mechanics_recipes.py` does not run them either, so rerun the sandbox after an Engine change that touches the recipe. |
| `compile-verified` | The backend's real compile step accepts the recipe sources, but no scenario has run them. |
| `shape` | The sources show the API shape and pass static checks only, such as matching `///@` declarations between backends. No Engine lane compiles them. |

Managed C# recipes start as `shape` because no Engine CI lane builds a managed sandbox. A status moves up only when the corresponding compile or smoke actually passes.

## Page template

Recipe pages use this front matter and `##` order. The gate rejects missing, extra or reordered `##` sections; `###` subsections are free.

```text
---
layout: default
title: <Title>
locale: en
document_id: recipe-<slug>
permalink: /Docs/en/how-to/mechanics/<slug>.html
recipe_kind: <primitive or pattern>
---

# <Title>

<Purpose paragraph, then a status table with one row per backend:>
| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

## Source paths inspected
## Problem
## Engine primitives used
## Minimal example
## Step by step
## Authority and sync
## Persistence and migration
## Pitfalls and anti-patterns
## Test route
## Managed C#
## How a larger project extends it
## See also
```

| Section | Holds |
| --- | --- |
| Problem | What the player experiences and which part is project policy. |
| Engine primitives used | A table of generated API ids with side and purpose, plus the primitives that are absent or never fired. |
| Minimal example | The sandbox files and the command that runs them. |
| Step by step | Verbatim excerpts in order: declarations, server authority, client presentation, content. |
| Authority and sync | Which side owns the state, sync flags, remote-call limits, the `Game.Sync` cover and revalidation after `Yield`. |
| Persistence and migration | What survives a restart, the re-arm hook, `///@ MigrationRule` for renames and the restart test. |
| Pitfalls and anti-patterns | At least three list items, each tied to Engine source or an owning page. |
| Test route | The exact command, scenarios and markers. |
| Managed C# | The same steps in C#, with the backend status. |
| How a larger project extends it | Data-driven catalogs, components, project events, and what stays out of the Engine. |

The Russian mirror translates the section headings and keeps their number and order.

Cross-cutting pages, such as the cover-transaction page, are not recipes: they use the front matter without `recipe_kind`, document id `mechanics-<slug>`, and start with `## Source paths inspected`. The excerpt, symbol, mirror and project-name rules apply to them as well.

## Run the sandbox

The static gate needs no build. It reads `recipes.json`, every recipe page and mirror, this index and the sandbox files:

```powershell
py -3 BuildTools/mechanics_recipes.py --check
```

```bash
python3 BuildTools/mechanics_recipes.py --check
```

It prints `error[<check>]` and `warning[<check>]` lines and exits with `0` when there are no errors, `1` when there are errors, and `2` when the manifest or `Docs/generated/api.json` is missing. `--list` prints the recipes with their kinds and statuses. A page outside the 150-300 line target is a warning only.

The runtime route builds the sandbox, bakes it and runs `mechanics-smoke.json` through `BuildTools/gameplay_test_runner.py`. Each recipe's in-server tests are selected with the harness setting `Testing.Filter`; restart scenarios run in two phases against a JSON-backed database that the runner keeps between the phases. `Examples/MechanicsSandbox/README.md` lists the prerequisites and options. From that directory:

```powershell
python validate.py
```

```bash
python3 validate.py
```

## Propose a recipe

The gate checks each item marked as checked; the others are review items.

1. Confirm the mechanic in the Engine first: every symbol exists in `Docs/generated/api.json`, the Engine fires the events it relies on, and an Engine test or example exercises it. Choose `primitive` or `pattern`.
2. Add the module, tests, content and Managed C# file with identical `///@` declarations, plus smoke scenarios, in `Examples/MechanicsSandbox`.
3. Add the manifest entry (checked): `slug`, `title`, `kind`, `backends` with both `angelscript` and `managed`, `page` as `Docs/en/how-to/mechanics/<slug>.md`, `files` grouped as `angelscript`, `managed`, `content`, `maps` or `tests` with every path inside the sandbox, `symbols`, optional `builtins`, `events_fired_by_recipe` and `smoke` with existing scenario ids.
4. Write the page (checked): the front matter keys in the template order with matching `recipe_kind`, a title matching the manifest title, the twelve `##` sections in order, at least three pitfall list items, and a table row per backend showing its status in backticks.
5. Quote code verbatim (checked): an `angelscript`, `csharp`, `ini` or `json` fence must be a contiguous run of whole lines, with their original indentation, from a file listed for the same recipe. A fence inside a list item is indented as a whole. `bash`, `powershell` and `text` fences are free.
6. Cite symbols (checked): the page mentions every manifest symbol id and no generated API id that the manifest does not list. Backend built-ins and metadata grammar go to `builtins`.
7. Keep to fired events (checked): a recipe file must not subscribe to an event that the Engine never fires unless the recipe lists it in `events_fired_by_recipe` and one of its files fires it.
8. Add the Russian mirror (checked): the same front matter keys with `locale: ru`, the same number of `##` headings and identical fenced blocks.
9. Stay project-neutral (checked): no embedding-project names in pages, this index or sandbox files.
10. Add a row to the table below (checked) with the same kind and statuses as the manifest, then run the static gate, the sandbox route and the documentation gates.

## Recipes

| Recipe | Kind | AngelScript | Managed C# | Teaches |
| --- | --- | --- | --- | --- |
| [Custom persistent character state](persistent-state.md) | `primitive` | `runtime-verified` | `shape` | Persistent properties, init and load events, and a restart proof |
| [Validated action request](validated-action.md) | `primitive` | `runtime-verified` | `shape` | A client intent that the server re-derives and validates before acting |
| [Timed effect that survives a restart](timed-effect.md) | `primitive` | `runtime-verified` | `shape` | A persisted deadline, a re-arm on load and a `CountTimeEvent` guard |

Cross-cutting page:

- [Lock several entities for one action](cover-transactions.md): one `Game.Sync` cover for every entity an action touches, and revalidation after a suspension.

## Coming next

These recipes are planned and not written yet. The titles name topics, not pages.

Primitive recipes:

- Data-driven item behavior
- Equip into a slot with a shared rule
- Derived stats
- Container put and take
- Map transition trigger
- NPC patrol and awareness
- Interruptible long action

Pattern recipes:

- Project event vocabulary and dispatcher
- Two-party trade
- Conversation without an Engine dialog system
- Quest stage and journal
- Death, knockout and respawn
- Status effects with decay and staged modifiers
- Attack with deferred hit
- Item stacking
- Server-driven prompts with session tokens
- Safe admin commands with access checks
- Login to a controllable character
- World clock and day and night
- Custom entity types with persistent holders
- Data catalogs from FixedType rows

Cross-cutting pages:

- Restoring state idempotently on load
- Splitting one fact into server, public and owner-only properties
- Declaring project events with veto chains
- The same rule compiled for client and server
- AngelScript idioms for a new project

## See also

- [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md), including [time events after a database reload](../scripting/lifecycle-and-concurrency.md#time-events-after-a-database-reload)
- [Remote Calls](../../reference/scripting/remote-calls.md)
- [Choose a Script Backend](../../explanation/scripting-runtime/choosing-a-backend.md) and [Managed C# Scripting](../scripting/managed-csharp.md)
- [Script Test Harness Pattern](../project/script-test-harness.md)
- [Public Example Repositories](../build/public-example-repositories.md)
- [Generated script API reference](../../reference/script-api/index.md)
